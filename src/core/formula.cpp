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
    } else if (t.is_fixed()) {
        // Fixed (instantiated) variables - use distinct f_N format
        return "f_" + std::to_string(t.as_variable());
    } else {
        return t.as_constant().get().get_name();
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
    data_(std::move(s)) {}

std::string Formula::to_string() const {
    return formula_to_string_impl(*this, 0);
}

// ==================== Sentence ====================

Term Sentence::remap_term(const Term& t, const std::unordered_map<var_index, var_index>& var_map) {
    if (t.is_variable()) {
        auto it = var_map.find(t.as_variable());
        if (it != var_map.end()) {
            // Preserve the variable type (generalized or fixed)
            if (!t.is_generalized()) {
                LOG_FATAL << "All term in sentence must be bounded by quantifier";
            }
            return Term::generalized(it->second);
        }
        return t;  // Variable not in map (shouldn't happen in well-formed sentence)
    }
    return t;  // Constants don't need remapping
}

FormulaHandle Sentence::copy_formula_recursive(
    const FormulaRegistry& src,
    FormulaHandle src_handle,
    std::unordered_map<FormulaHandle, FormulaHandle>& handle_map
) {
    auto it = handle_map.find(src_handle);
    if (it != handle_map.end()) {
        return it->second;
    }

    const Formula& f = src_handle.get();
    FormulaHandle new_handle;

    if (f.is_predicate()) {
        const PredicateInstance& p = f.as_predicate();
        std::vector<Term> new_args = p.args();
        new_handle = formulas_.register_item(Formula(PredicateInstance(p.predicate(), std::move(new_args))));
    }
    else if (f.is_compound()) {
        const Compound& c = f.as_compound();
        FormulaHandle new_left;
        FormulaHandle new_right;
        if (c.left.valid()) {
            new_left = copy_formula_recursive(src, c.left, handle_map);
        }
        if (c.right.valid()) {
            new_right = copy_formula_recursive(src, c.right, handle_map);
        }
        new_handle = formulas_.register_item(Formula(Compound{c.op, new_left, new_right}));
    }
    else if (f.is_quantified()) {
        const Quantified& q = f.as_quantified();
        // Recursively copy body with updated var_map
        FormulaHandle new_body = copy_formula_recursive(src, q.body, handle_map);
        new_handle = formulas_.register_item(Formula(Quantified{q.op, q.var, new_body}));
    }
    else if (f.is_sentence()) {
        new_handle = formulas_.register_item(Formula(f.as_sentence()));
    }

    handle_map[src_handle] = new_handle;
    return new_handle;
}

Sentence::Sentence(FormulaRegistry& src, FormulaHandle src_root) {
    std::unordered_map<FormulaHandle, FormulaHandle> handle_map;
    root_ = copy_formula_recursive(src, src_root, handle_map);
}

void Sentence::rebind_formula_handles(Formula& f, FormulaRegistry& new_reg) {
    if (f.is_compound()) {
        Compound& c = std::get<Compound>(f.data_);
        if (c.left.valid()) {
            c.left = new_reg.transfer_owner(c.left);
        }
        if (c.right.valid()) {
            c.right = new_reg.transfer_owner(c.right);
        }
    } else if (f.is_quantified()) {
        Quantified& q = std::get<Quantified>(f.data_);
        if (q.body.valid()) {
            q.body = new_reg.transfer_owner(q.body);
        }
    }
    // PredicateInstance and SentenceHandle don't contain FormulaHandles
}

Sentence::Sentence(const Sentence& other) {
    formulas_ = other.formulas_;
    root_ = formulas_.transfer_owner(other.root_);
    // Rebind all internal FormulaHandles to point to our registry
    formulas_.for_each_mut([this](Formula& f) {
        rebind_formula_handles(f, formulas_);
    });
    // Rebuild item2id_ since the keys had stale handles
    formulas_.rebuild_index();
}

Sentence::Sentence(Sentence&& other) noexcept
    : formulas_(std::move(other.formulas_)),
      root_(std::move(other.root_))
{
    // After move, root_'s registry_ still points to other.formulas_
    // We need to update it to point to our formulas_
    root_ = formulas_.transfer_owner(root_);
    // Also rebind all internal handles in formulas_
    formulas_.for_each_mut([this](Formula& f) {
        rebind_formula_handles(f, formulas_);
    });
}

std::string Sentence::to_string() const {
    if (!root_.valid()) return "";
    return root_.get().to_string();
}

}  // namespace logic
