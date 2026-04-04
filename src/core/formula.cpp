#include "formula.h"
#include "src/util/logging.h"

#include <sstream>
#include <vector>

namespace logic {

// ==================== Formula::to_string ====================
// De Bruijn: uses a name stack to produce readable variable names.
// Outer binders get lower subscripts (x_0, x_1, ...).
// Gen(k) maps to name_stack[size-1-k].

namespace {

// Operator precedence (higher = binds tighter)
int precedence(Op op) {
    switch (op) {
        case Op::Iff:     return 1;
        case Op::Implies: return 2;
        case Op::Or:      return 3;
        case Op::And:     return 4;
        case Op::Not:     return 5;
        case Op::Forall:  return 5;
        case Op::Exists:  return 5;
        case Op::Bottom:  return 6;
        default:          return 0;
    }
}

// Forward declaration
std::string formula_to_str(const Formula& f, int parent_prec, std::vector<std::string>& names);

std::string term_to_str(const Term& t, std::vector<std::string>& names) {
    if (t.is_generalized()) {
        var_index k = t.as_variable();
        size_t idx = names.size() - 1 - k;
        if (idx < names.size()) return names[idx];
        return "x_FREE_" + std::to_string(k);  // free gen var (shouldn't happen in sentences)
    } else if (t.is_description()) {
        const auto& d = t.as_description();
        std::string var_name = "x_" + std::to_string(names.size());
        names.push_back(var_name);
        std::string body_str = formula_to_str(d.body.get(), 0, names);
        names.pop_back();
        return "(iota " + var_name + ". " + body_str + ")";
    } else {
        // Fixed (instantiated) variables
        return "f_" + std::to_string(t.as_variable());
    }
}

std::string predicate_to_str(const PredicateInstance& p, std::vector<std::string>& names) {
    std::ostringstream oss;
    oss << p.predicate().get().get_name();
    if (!p.args().empty()) {
        oss << "(";
        for (size_t i = 0; i < p.args().size(); ++i) {
            if (i > 0) oss << ", ";
            oss << term_to_str(p.args()[i], names);
        }
        oss << ")";
    }
    return oss.str();
}

std::string compound_to_str(const Compound& c, int parent_prec, std::vector<std::string>& names) {
    std::ostringstream oss;
    int my_prec = precedence(c.op);

    switch (c.op) {
        case Op::Bottom:
            return "_|_";
        case Op::Not:
            oss << "~" << formula_to_str(c.left.get(), my_prec, names);
            break;
        case Op::And:
            oss << formula_to_str(c.left.get(), my_prec, names)
                << " & "
                << formula_to_str(c.right.get(), my_prec + 1, names);
            break;
        case Op::Or:
            oss << formula_to_str(c.left.get(), my_prec, names)
                << " | "
                << formula_to_str(c.right.get(), my_prec + 1, names);
            break;
        case Op::Implies:
            oss << formula_to_str(c.left.get(), my_prec + 1, names)
                << " -> "
                << formula_to_str(c.right.get(), my_prec, names);
            break;
        case Op::Iff:
            oss << formula_to_str(c.left.get(), my_prec + 1, names)
                << " <-> "
                << formula_to_str(c.right.get(), my_prec + 1, names);
            break;
        default:
            return "???";
    }

    std::string result = oss.str();
    if (my_prec < parent_prec) {
        result = "(" + result + ")";
    }
    return result;
}

std::string quantified_to_str(const Quantified& q, int parent_prec, std::vector<std::string>& names) {
    std::ostringstream oss;
    int my_prec = precedence(q.op);

    std::string var_name = "x_" + std::to_string(names.size());
    names.push_back(var_name);

    if (q.op == Op::Forall) {
        oss << "forall " << var_name << ". ";
    } else {
        oss << "exists " << var_name << ". ";
    }
    oss << formula_to_str(q.body.get(), 0, names);

    names.pop_back();

    std::string result = oss.str();
    if (my_prec < parent_prec) {
        result = "(" + result + ")";
    }
    return result;
}

std::string formula_to_str(const Formula& f, int parent_prec, std::vector<std::string>& names) {
    if (f.is_schema_var()) {
        const auto& sv = f.as_schema_var();
        std::string s = "?" + std::to_string(sv.id);
        if (!sv.args.empty()) {
            s += "(";
            for (size_t i = 0; i < sv.args.size(); ++i) {
                if (i > 0) s += ", ";
                s += term_to_str(sv.args[i], names);
            }
            s += ")";
        }
        return s;
    }
    if (f.is_predicate()) {
        return predicate_to_str(f.as_predicate(), names);
    }
    else if (f.is_compound()) {
        return compound_to_str(f.as_compound(), parent_prec, names);
    }
    else if (f.is_quantified()) {
        return quantified_to_str(f.as_quantified(), parent_prec, names);
    }
    else if (f.is_sentence()) {
        return f.as_sentence().get().to_string();
    }
    return "???";
}

}  // anonymous namespace

Formula::Formula(SentenceHandle s):
    max_free_debruijn_(s.get().root().get().max_free_debruijn_),
    has_schema_vars_(false),
    data_(std::move(s)) {}

std::string Formula::to_string() const {
    std::vector<std::string> names;
    return formula_to_str(*this, 0, names);
}

// ==================== Content Hash ====================

namespace {

// Combine hash values (boost-style)
inline size_t hash_combine(size_t seed, size_t value) {
    return seed ^ (value + 0x9e3779b9 + (seed << 6) + (seed >> 2));
}

size_t term_content_hash(const Term& t) {
    if (t.is_generalized()) {
        return hash_combine(0, t.as_variable());
    } else if (t.is_fixed()) {
        return hash_combine(1, t.as_variable());
    } else {
        // De Bruijn: description has no bound_var, just hash the body
        const auto& d = t.as_description();
        return hash_combine(2, d.body.get().content_hash());
    }
}

size_t compute_content_hash(const Formula& f) {
    if (f.is_predicate()) {
        const auto& p = f.as_predicate();
        size_t h = std::hash<std::string>{}(p.predicate().get().get_name());
        h = hash_combine(h, p.predicate().get().get_num_args());
        for (const auto& arg : p.args()) {
            h = hash_combine(h, term_content_hash(arg));
        }
        return h;
    }
    if (f.is_compound()) {
        const auto& c = f.as_compound();
        size_t h = hash_combine(0x10, static_cast<size_t>(c.op));
        if (c.left.valid()) h = hash_combine(h, c.left.get().content_hash());
        if (c.right.valid()) h = hash_combine(h, c.right.get().content_hash());
        return h;
    }
    if (f.is_quantified()) {
        // De Bruijn: no q.var to hash — just op + body
        const auto& q = f.as_quantified();
        size_t h = hash_combine(0x20, static_cast<size_t>(q.op));
        h = hash_combine(h, q.body.get().content_hash());
        return h;
    }
    if (f.is_schema_var()) {
        const auto& sv = f.as_schema_var();
        size_t h = hash_combine(0x30, sv.id);
        for (const auto& arg : sv.args) {
            h = hash_combine(h, term_content_hash(arg));
        }
        return h;
    }
    if (f.is_sentence()) {
        return f.as_sentence().get().root().get().content_hash();
    }
    return 0;
}

}  // anonymous namespace

size_t Formula::content_hash() const {
    if (content_hash_ == 0) {
        content_hash_ = compute_content_hash(*this);
        // Avoid 0 as a cached value (it means "not computed")
        if (content_hash_ == 0) content_hash_ = 1;
    }
    return content_hash_;
}

// ==================== Sentence ====================

std::string Sentence::to_string() const {
    if (!root_.valid()) return "";
    return root_.get().to_string();
}

// ==================== FormulaBuilder ====================

FormulaHandle FormulaBuilder::predicate(PredicateHandle pred, std::vector<Term> args) {
    for (const auto& t : args) {
        if (!t.is_description()) {
            use_var(t.as_variable());
        }
    }
    return add_formula(Formula(PredicateInstance(pred, std::move(args))));
}

FormulaHandle FormulaBuilder::make_schema_var(size_t id, std::vector<Term> args) {
    for (const auto& t : args) {
        if (!t.is_description()) use_var(t.as_variable());
    }
    return add_formula(Formula(SchemaVar{id, std::move(args)}));
}

// ========== De Bruijn: shift ==========

Term FormulaBuilder::shift_term(const Term& t, int cutoff, int delta) {
    if (t.is_generalized()) {
        var_index k = t.as_variable();
        if (static_cast<int>(k) >= cutoff)
            return Term::generalized(static_cast<var_index>(static_cast<int>(k) + delta));
        return t;
    }
    if (t.is_description()) {
        const auto& d = t.as_description();
        FormulaHandle nb = shift_formula(d.body, cutoff + 1, delta);
        if (nb != d.body) return Term::description(nb);
        return t;
    }
    return t;
}

FormulaHandle FormulaBuilder::shift_formula(FormulaHandle h, int cutoff, int delta) {
    if (delta == 0) return h;
    const Formula& f = h.get();

    if (f.is_predicate()) {
        const auto& p = f.as_predicate();
        std::vector<Term> new_args;
        bool changed = false;
        for (const auto& t : p.args()) {
            Term nt = shift_term(t, cutoff, delta);
            new_args.push_back(nt);
            if (!(nt == t)) changed = true;
        }
        if (changed) return add_formula(Formula(PredicateInstance(p.predicate(), std::move(new_args))));
        return h;
    }
    if (f.is_compound()) {
        const auto& c = f.as_compound();
        auto nl = c.left.valid() ? shift_formula(c.left, cutoff, delta) : c.left;
        auto nr = c.right.valid() ? shift_formula(c.right, cutoff, delta) : c.right;
        if (nl != c.left || nr != c.right) return add_formula(Formula(Compound{c.op, nl, nr}));
        return h;
    }
    if (f.is_quantified()) {
        const auto& q = f.as_quantified();
        auto nb = shift_formula(q.body, cutoff + 1, delta);
        if (nb != q.body) return add_formula(Formula(Quantified{q.op, nb}));
        return h;
    }
    if (f.is_schema_var()) {
        const auto& sv = f.as_schema_var();
        if (sv.args.empty()) return h;
        std::vector<Term> new_args;
        bool changed = false;
        for (const auto& t : sv.args) {
            Term nt = shift_term(t, cutoff, delta);
            new_args.push_back(nt);
            if (!(nt == t)) changed = true;
        }
        if (changed) return add_formula(Formula(SchemaVar{sv.id, std::move(new_args)}));
        return h;
    }
    return h;
}

// ========== De Bruijn: abstract ==========

Term FormulaBuilder::abstract_var_in_term(const Term& t, var_index fixed_var, int depth) {
    if (t.is_fixed() && t.as_variable() == fixed_var)
        return Term::generalized(static_cast<var_index>(depth));
    if (t.is_generalized()) {
        var_index k = t.as_variable();
        return (static_cast<int>(k) >= depth) ? Term::generalized(k + 1) : t;
    }
    if (t.is_description()) {
        const auto& d = t.as_description();
        FormulaHandle nb = abstract_var_impl(d.body, fixed_var, depth + 1);
        if (nb != d.body) return Term::description(nb);
        return t;
    }
    return t;
}

FormulaHandle FormulaBuilder::abstract_var_impl(FormulaHandle h, var_index fixed_var, int depth) {
    const Formula& f = h.get();

    if (f.is_predicate()) {
        const auto& p = f.as_predicate();
        std::vector<Term> new_args;
        bool changed = false;
        for (const auto& t : p.args()) {
            Term nt = abstract_var_in_term(t, fixed_var, depth);
            new_args.push_back(nt);
            if (!(nt == t)) changed = true;
        }
        if (changed) return add_formula(Formula(PredicateInstance(p.predicate(), std::move(new_args))));
        return h;
    }
    if (f.is_compound()) {
        const auto& c = f.as_compound();
        auto nl = c.left.valid() ? abstract_var_impl(c.left, fixed_var, depth) : c.left;
        auto nr = c.right.valid() ? abstract_var_impl(c.right, fixed_var, depth) : c.right;
        if (nl != c.left || nr != c.right) return add_formula(Formula(Compound{c.op, nl, nr}));
        return h;
    }
    if (f.is_quantified()) {
        const auto& q = f.as_quantified();
        auto nb = abstract_var_impl(q.body, fixed_var, depth + 1);
        if (nb != q.body) return add_formula(Formula(Quantified{q.op, nb}));
        return h;
    }
    if (f.is_schema_var()) {
        const auto& sv = f.as_schema_var();
        if (sv.args.empty()) return h;
        std::vector<Term> new_args;
        bool changed = false;
        for (const auto& t : sv.args) {
            Term nt = abstract_var_in_term(t, fixed_var, depth);
            new_args.push_back(nt);
            if (!(nt == t)) changed = true;
        }
        if (changed) return add_formula(Formula(SchemaVar{sv.id, std::move(new_args)}));
        return h;
    }
    return h;
}

FormulaHandle FormulaBuilder::abstract_var(FormulaHandle body, var_index fixed_var) {
    return abstract_var_impl(body, fixed_var, 0);
}

// ========== De Bruijn: instantiate ==========

Term FormulaBuilder::instantiate_gen_in_term(const Term& t, const Term& replacement, int depth) {
    if (t.is_generalized()) {
        var_index k = t.as_variable();
        if (static_cast<int>(k) == depth)
            return shift_term(replacement, 0, depth);
        if (static_cast<int>(k) > depth)
            return Term::generalized(k - 1);
        return t;
    }
    if (t.is_description()) {
        const auto& d = t.as_description();
        FormulaHandle nb = instantiate_gen_impl(d.body, replacement, depth + 1);
        if (nb != d.body) return Term::description(nb);
        return t;
    }
    return t;
}

FormulaHandle FormulaBuilder::instantiate_gen_impl(FormulaHandle h, const Term& replacement, int depth) {
    const Formula& f = h.get();

    if (f.is_predicate()) {
        const auto& p = f.as_predicate();
        std::vector<Term> new_args;
        bool changed = false;
        for (const auto& t : p.args()) {
            Term nt = instantiate_gen_in_term(t, replacement, depth);
            new_args.push_back(nt);
            if (!(nt == t)) changed = true;
        }
        if (changed) return add_formula(Formula(PredicateInstance(p.predicate(), std::move(new_args))));
        return h;
    }
    if (f.is_compound()) {
        const auto& c = f.as_compound();
        auto nl = c.left.valid() ? instantiate_gen_impl(c.left, replacement, depth) : c.left;
        auto nr = c.right.valid() ? instantiate_gen_impl(c.right, replacement, depth) : c.right;
        if (nl != c.left || nr != c.right) return add_formula(Formula(Compound{c.op, nl, nr}));
        return h;
    }
    if (f.is_quantified()) {
        const auto& q = f.as_quantified();
        auto nb = instantiate_gen_impl(q.body, replacement, depth + 1);
        if (nb != q.body) return add_formula(Formula(Quantified{q.op, nb}));
        return h;
    }
    if (f.is_schema_var()) {
        const auto& sv = f.as_schema_var();
        if (sv.args.empty()) return h;
        std::vector<Term> new_args;
        bool changed = false;
        for (const auto& t : sv.args) {
            Term nt = instantiate_gen_in_term(t, replacement, depth);
            new_args.push_back(nt);
            if (!(nt == t)) changed = true;
        }
        if (changed) return add_formula(Formula(SchemaVar{sv.id, std::move(new_args)}));
        return h;
    }
    return h;
}

FormulaHandle FormulaBuilder::instantiate_gen(FormulaHandle body, const Term& replacement) {
    return instantiate_gen_impl(body, replacement, 0);
}

// ========== De Bruijn: subst_fixed ==========

Term FormulaBuilder::subst_fixed_in_term(const Term& t, var_index fixed_var, const Term& replacement, int depth) {
    if (t.is_fixed() && t.as_variable() == fixed_var)
        return shift_term(replacement, 0, depth);
    if (t.is_description()) {
        const auto& d = t.as_description();
        FormulaHandle nb = subst_fixed_impl(d.body, fixed_var, replacement, depth + 1);
        if (nb != d.body) return Term::description(nb);
        return t;
    }
    return t;
}

FormulaHandle FormulaBuilder::subst_fixed_impl(FormulaHandle h, var_index fixed_var, const Term& replacement, int depth) {
    const Formula& f = h.get();

    if (f.is_predicate()) {
        const auto& p = f.as_predicate();
        std::vector<Term> new_args;
        bool changed = false;
        for (const auto& t : p.args()) {
            Term nt = subst_fixed_in_term(t, fixed_var, replacement, depth);
            new_args.push_back(nt);
            if (!(nt == t)) changed = true;
        }
        if (changed) return add_formula(Formula(PredicateInstance(p.predicate(), std::move(new_args))));
        return h;
    }
    if (f.is_compound()) {
        const auto& c = f.as_compound();
        auto nl = c.left.valid() ? subst_fixed_impl(c.left, fixed_var, replacement, depth) : c.left;
        auto nr = c.right.valid() ? subst_fixed_impl(c.right, fixed_var, replacement, depth) : c.right;
        if (nl != c.left || nr != c.right) return add_formula(Formula(Compound{c.op, nl, nr}));
        return h;
    }
    if (f.is_quantified()) {
        const auto& q = f.as_quantified();
        auto nb = subst_fixed_impl(q.body, fixed_var, replacement, depth + 1);
        if (nb != q.body) return add_formula(Formula(Quantified{q.op, nb}));
        return h;
    }
    if (f.is_schema_var()) {
        const auto& sv = f.as_schema_var();
        if (sv.args.empty()) return h;
        std::vector<Term> new_args;
        bool changed = false;
        for (const auto& t : sv.args) {
            Term nt = subst_fixed_in_term(t, fixed_var, replacement, depth);
            new_args.push_back(nt);
            if (!(nt == t)) changed = true;
        }
        if (changed) return add_formula(Formula(SchemaVar{sv.id, std::move(new_args)}));
        return h;
    }
    return h;
}

// ========== Simple term substitution ==========

Term FormulaBuilder::translate_in_term(const Term& t, const Term& old_term, const Term& new_term) {
    if (t == old_term) return new_term;
    if (!t.is_description()) return t;
    const auto& d = t.as_description();
    FormulaHandle new_body = translate_term(d.body, old_term, new_term);
    if (new_body != d.body) return Term::description(new_body);
    return t;
}

FormulaHandle FormulaBuilder::translate_term(FormulaHandle const &h, Term const &old_term, Term const &new_term) {
    const Formula& f = h.get();

    if (f.is_schema_var()) {
        const auto& sv = f.as_schema_var();
        if (sv.args.empty()) return h;
        std::vector<Term> new_args;
        bool changed = false;
        for (const auto& t : sv.args) {
            Term nt = translate_in_term(t, old_term, new_term);
            new_args.push_back(nt);
            if (!(nt == t)) changed = true;
        }
        if (changed) return add_formula(Formula(SchemaVar{sv.id, std::move(new_args)}));
        return h;
    }

    if (f.is_predicate()) {
        const auto& p = f.as_predicate();
        std::vector<Term> new_args;
        bool changed = false;
        for (const auto& t : p.args()) {
            Term nt = translate_in_term(t, old_term, new_term);
            new_args.push_back(nt);
            if (!(nt == t)) changed = true;
        }
        if (changed) return add_formula(Formula(PredicateInstance(p.predicate(), std::move(new_args))));
        return h;
    }
    if (f.is_compound()) {
        const auto& c = f.as_compound();
        auto nl = c.left.valid() ? translate_term(c.left, old_term, new_term) : c.left;
        auto nr = c.right.valid() ? translate_term(c.right, old_term, new_term) : c.right;
        if (nl != c.left || nr != c.right) return add_formula(Formula(Compound{c.op, nl, nr}));
        return h;
    }
    if (f.is_quantified()) {
        const auto& q = f.as_quantified();
        auto nb = translate_term(q.body, old_term, new_term);
        if (nb != q.body) return add_formula(Formula(Quantified{q.op, nb}));
        return h;
    }
    return h;
}

// ========== Schema Instantiation ==========

Term FormulaBuilder::instantiate_schema_in_term(const Term& t, const std::vector<SchemaBind>& bindings) {
    if (!t.is_description()) return t;
    const auto& d = t.as_description();
    auto nb = instantiate_schema(d.body, bindings);
    if (nb != d.body) return Term::description(nb);
    return t;
}

FormulaHandle FormulaBuilder::instantiate_schema(FormulaHandle h, const std::vector<SchemaBind>& bindings) {
    const Formula& f = h.get();
    if (f.is_schema_var()) {
        const auto& sv = f.as_schema_var();
        const auto& bind = bindings[sv.id];
        if (bind.params.empty()) {
            return bind.body;
        }
        FormulaHandle result = bind.body;
        for (size_t i = 0; i < bind.params.size() && i < sv.args.size(); ++i) {
            Term arg = instantiate_schema_in_term(sv.args[i], bindings);
            result = subst_fixed_impl(result, bind.params[i], arg, 0);
        }
        return result;
    }
    if (!f.has_schema_vars()) return h;

    if (f.is_compound()) {
        const auto& c = f.as_compound();
        auto nl = c.left.valid() ? instantiate_schema(c.left, bindings) : c.left;
        auto nr = c.right.valid() ? instantiate_schema(c.right, bindings) : c.right;
        if (nl != c.left || nr != c.right)
            return add_formula(Formula(Compound{c.op, nl, nr}));
        return h;
    }
    if (f.is_quantified()) {
        const auto& q = f.as_quantified();
        auto nb = instantiate_schema(q.body, bindings);
        if (nb != q.body)
            return add_formula(Formula(Quantified{q.op, nb}));
        return h;
    }
    if (f.is_predicate()) {
        const auto& p = f.as_predicate();
        bool changed = false;
        std::vector<Term> new_args;
        for (const auto& t : p.args()) {
            Term nt = instantiate_schema_in_term(t, bindings);
            new_args.push_back(nt);
            if (!(nt == t)) changed = true;
        }
        if (changed)
            return add_formula(Formula(PredicateInstance(p.predicate(), std::move(new_args))));
        return h;
    }
    return h;
}

}  // namespace logic
