#include "formula.h"
#include "src/util/logging.h"

#include <sstream>

namespace logic {

// ==================== Formula::to_string ====================

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

std::string formula_to_string_impl(const Formula& f, int parent_prec);

std::string term_to_string(const Term& t) {
    if (t.is_generalized()) {
        // Generalized variables are bound by quantifiers - use same x_N format
        return "x_" + std::to_string(t.as_variable());
    } else if (t.is_description()) {
        const auto& d = t.as_description();
        return "(iota x_" + std::to_string(d.bound_var) + ". " +
               formula_to_string_impl(d.body.get(), 0) + ")";
    } else {
        // Fixed (instantiated) variables - use distinct f_N format
        return "f_" + std::to_string(t.as_variable());
    }
}

std::string predicate_instance_to_string(const PredicateInstance& p) {
    std::ostringstream oss;
    oss << p.predicate().get().get_name();
    if (!p.args().empty()) {
        oss << "(";
        for (size_t i = 0; i < p.args().size(); ++i) {
            if (i > 0) oss << ", ";
            oss << term_to_string(p.args()[i]);
        }
        oss << ")";
    }
    return oss.str();
}

std::string compound_to_string(const Compound& c, int parent_prec) {
    std::ostringstream oss;
    int my_prec = precedence(c.op);

    switch (c.op) {
        case Op::Bottom:
            return "_|_";
        case Op::Not:
            oss << "~" << formula_to_string_impl(c.left.get(), my_prec);
            break;
        case Op::And:
            oss << formula_to_string_impl(c.left.get(), my_prec)
                << " & "
                << formula_to_string_impl(c.right.get(), my_prec + 1);
            break;
        case Op::Or:
            oss << formula_to_string_impl(c.left.get(), my_prec)
                << " | "
                << formula_to_string_impl(c.right.get(), my_prec + 1);
            break;
        case Op::Implies:
            oss << formula_to_string_impl(c.left.get(), my_prec + 1)
                << " -> "
                << formula_to_string_impl(c.right.get(), my_prec);
            break;
        case Op::Iff:
            oss << formula_to_string_impl(c.left.get(), my_prec + 1)
                << " <-> "
                << formula_to_string_impl(c.right.get(), my_prec + 1);
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

std::string quantified_to_string(const Quantified& q, int parent_prec) {
    std::ostringstream oss;
    int my_prec = precedence(q.op);

    if (q.op == Op::Forall) {
        oss << "forall x_" << q.var << ". ";
    } else {
        oss << "exists x_" << q.var << ". ";
    }
    oss << formula_to_string_impl(q.body.get(), 0);

    std::string result = oss.str();
    if (my_prec < parent_prec) {
        result = "(" + result + ")";
    }
    return result;
}

std::string formula_to_string_impl(const Formula& f, int parent_prec) {
    if (f.is_schema_var()) {
        const auto& sv = f.as_schema_var();
        std::string s = "?" + std::to_string(sv.id);
        if (!sv.args.empty()) {
            s += "(";
            for (size_t i = 0; i < sv.args.size(); ++i) {
                if (i > 0) s += ", ";
                s += term_to_string(sv.args[i]);
            }
            s += ")";
        }
        return s;
    }
    if (f.is_predicate()) {
        return predicate_instance_to_string(f.as_predicate());
    }
    else if (f.is_compound()) {
        return compound_to_string(f.as_compound(), parent_prec);
    }
    else if (f.is_quantified()) {
        return quantified_to_string(f.as_quantified(), parent_prec);
    }
    else if (f.is_sentence()) {
        return f.as_sentence().get().to_string();
    }
    return "???";
}

}  // anonymous namespace

Formula::Formula(SentenceHandle s):
    next_gen_var_idx_(s.get().root().get().next_gen_var_idx_),
    has_schema_vars_(false),
    data_(std::move(s)) {}

std::string Formula::to_string() const {
    return formula_to_string_impl(*this, 0);
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
        const auto& d = t.as_description();
        size_t h = hash_combine(2, d.bound_var);
        return hash_combine(h, d.body.get().content_hash());
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
        const auto& q = f.as_quantified();
        size_t h = hash_combine(0x20, static_cast<size_t>(q.op));
        h = hash_combine(h, q.var);
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

// ==================== Alpha-Equivalence ====================

namespace {

struct AlphaCtx {
    std::unordered_map<var_index, size_t> lhs, rhs;
    size_t next_id = 0;
};

// RAII: binds two quantifier variables to the same canonical id,
// restores previous bindings on destruction (handles shadowing).
struct ScopedBind {
    AlphaCtx& ctx;
    var_index l, r;
    std::optional<size_t> prev_l, prev_r;

    ScopedBind(AlphaCtx& c, var_index l, var_index r) : ctx(c), l(l), r(r) {
        auto li = ctx.lhs.find(l), ri = ctx.rhs.find(r);
        prev_l = li != ctx.lhs.end() ? std::optional(li->second) : std::nullopt;
        prev_r = ri != ctx.rhs.end() ? std::optional(ri->second) : std::nullopt;
        size_t id = ctx.next_id++;
        ctx.lhs[l] = id;
        ctx.rhs[r] = id;
    }
    ~ScopedBind() {
        if (prev_l) ctx.lhs[l] = *prev_l; else ctx.lhs.erase(l);
        if (prev_r) ctx.rhs[r] = *prev_r; else ctx.rhs.erase(r);
    }
};

bool formula_eq(const Formula& a, const Formula& b, AlphaCtx& ctx);

bool term_eq(const Term& a, const Term& b, AlphaCtx& ctx) {
    if (a.is_generalized() && b.is_generalized()) {
        auto ai = ctx.lhs.find(a.as_variable()), bi = ctx.rhs.find(b.as_variable());
        bool ab = ai != ctx.lhs.end(), bb = bi != ctx.rhs.end();
        if (ab && bb) return ai->second == bi->second;
        return !ab && !bb && a.as_variable() == b.as_variable();
    }
    if (a.is_fixed() && b.is_fixed())
        return a.as_variable() == b.as_variable();
    if (a.is_description() && b.is_description()) {
        ScopedBind bind(ctx, a.as_description().bound_var, b.as_description().bound_var);
        return formula_eq(a.as_description().body.get(), b.as_description().body.get(), ctx);
    }
    return false;
}

bool terms_eq(const std::vector<Term>& a, const std::vector<Term>& b, AlphaCtx& ctx) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (!term_eq(a[i], b[i], ctx)) return false;
    return true;
}

bool formula_eq(const Formula& a, const Formula& b, AlphaCtx& ctx) {
    if (a.is_predicate() && b.is_predicate()) {
        const auto &pa = a.as_predicate(), &pb = b.as_predicate();
        return pa.predicate() == pb.predicate() && terms_eq(pa.args(), pb.args(), ctx);
    }
    if (a.is_compound() && b.is_compound()) {
        const auto &ca = a.as_compound(), &cb = b.as_compound();
        return ca.op == cb.op
            && ca.left.valid() == cb.left.valid() && ca.right.valid() == cb.right.valid()
            && (!ca.left.valid() || formula_eq(ca.left.get(), cb.left.get(), ctx))
            && (!ca.right.valid() || formula_eq(ca.right.get(), cb.right.get(), ctx));
    }
    if (a.is_quantified() && b.is_quantified()) {
        const auto &qa = a.as_quantified(), &qb = b.as_quantified();
        if (qa.op != qb.op) return false;
        ScopedBind bind(ctx, qa.var, qb.var);
        return formula_eq(qa.body.get(), qb.body.get(), ctx);
    }
    if (a.is_sentence() && b.is_sentence())
        return formula_eq(a.as_sentence().get().root().get(),
                          b.as_sentence().get().root().get(), ctx);
    if (a.is_schema_var() && b.is_schema_var()) {
        const auto &sa = a.as_schema_var(), &sb = b.as_schema_var();
        return sa.id == sb.id && terms_eq(sa.args, sb.args, ctx);
    }
    return false;
}

}  // namespace

bool alpha_equiv(const Formula& a, const Formula& b) {
    if (&a == &b || a == b) return true;
    AlphaCtx ctx;
    return formula_eq(a, b, ctx);
}

// ==================== Sentence ====================

std::string Sentence::to_string() const {
    if (!root_.valid()) return "";
    return root_.get().to_string();
}

}  // namespace logic
