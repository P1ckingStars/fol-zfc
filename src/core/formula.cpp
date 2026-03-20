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

// ==================== Sentence ====================

std::string Sentence::to_string() const {
    if (!root_.valid()) return "";
    return root_.get().to_string();
}

}  // namespace logic
