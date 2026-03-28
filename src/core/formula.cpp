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

}  // namespace logic
