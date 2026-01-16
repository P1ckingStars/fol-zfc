#include "formula.h"

#include <sstream>
#include <stdexcept>

namespace core {

// ==================== GlobalContext ====================

predicate_id GlobalContext::create_predicate(const std::string& name, size_t arity) {
    auto it = predicate_by_name_.find(name);
    if (it != predicate_by_name_.end()) {
        // Already exists - verify arity matches
        if (predicates_[it->second].arity != arity) {
            throw std::runtime_error("Predicate '" + name + "' already exists with different arity");
        }
        return it->second;
    }
    predicate_id id = predicates_.size();
    predicates_.push_back(Predicate{name, arity});
    predicate_by_name_[name] = id;
    return id;
}

std::optional<predicate_id> GlobalContext::find_predicate(const std::string& name) const {
    auto it = predicate_by_name_.find(name);
    if (it != predicate_by_name_.end()) {
        return it->second;
    }
    return std::nullopt;
}

const Predicate& GlobalContext::get_predicate(predicate_id id) const {
    return predicates_.at(id);
}

constant_id GlobalContext::create_constant(const std::string& name) {
    auto it = constant_by_name_.find(name);
    if (it != constant_by_name_.end()) {
        return it->second;
    }
    constant_id id = constants_.size();
    constants_.push_back(Constant{name});
    constant_by_name_[name] = id;
    return id;
}

std::optional<constant_id> GlobalContext::find_constant(const std::string& name) const {
    auto it = constant_by_name_.find(name);
    if (it != constant_by_name_.end()) {
        return it->second;
    }
    return std::nullopt;
}

const Constant& GlobalContext::get_constant(constant_id id) const {
    return constants_.at(id);
}

// ==================== Sentence ====================

formula_id Sentence::add_formula(Formula f) {
    formulas_.push_back(std::move(f));
    return formulas_.size();  // 1-indexed
}

formula_id Sentence::create_predicate(predicate_id pred, std::vector<Term> args) {
    return add_formula(Formula(PredicateFormula{pred, std::move(args)}));
}

formula_id Sentence::create_and(formula_id left, formula_id right) {
    return add_formula(Formula(CompoundFormula{Op::And, left, right}));
}

formula_id Sentence::create_or(formula_id left, formula_id right) {
    return add_formula(Formula(CompoundFormula{Op::Or, left, right}));
}

formula_id Sentence::create_implies(formula_id left, formula_id right) {
    return add_formula(Formula(CompoundFormula{Op::Implies, left, right}));
}

formula_id Sentence::create_iff(formula_id left, formula_id right) {
    return add_formula(Formula(CompoundFormula{Op::Iff, left, right}));
}

formula_id Sentence::create_not(formula_id operand) {
    return add_formula(Formula(CompoundFormula{Op::Not, operand, INVALID_FORMULA}));
}

formula_id Sentence::create_bottom() {
    return add_formula(Formula(CompoundFormula{Op::Bottom, INVALID_FORMULA, INVALID_FORMULA}));
}

formula_id Sentence::create_forall(var_index var, formula_id body) {
    return add_formula(Formula(QuantifiedFormula{Op::Forall, var, body}));
}

formula_id Sentence::create_exists(var_index var, formula_id body) {
    return add_formula(Formula(QuantifiedFormula{Op::Exists, var, body}));
}

std::set<var_index> Sentence::get_free_vars(formula_id id) const {
    std::set<var_index> result;
    const Formula& f = get_formula(id);

    if (f.is_predicate()) {
        const auto& pred = f.as_predicate();
        for (const auto& term : pred.args) {
            if (term.is_variable()) {
                result.insert(term.as_variable());
            }
        }
    } else if (f.is_compound()) {
        const auto& comp = f.as_compound();
        if (comp.left != INVALID_FORMULA) {
            auto left_vars = get_free_vars(comp.left);
            result.insert(left_vars.begin(), left_vars.end());
        }
        if (comp.right != INVALID_FORMULA) {
            auto right_vars = get_free_vars(comp.right);
            result.insert(right_vars.begin(), right_vars.end());
        }
    } else if (f.is_quantified()) {
        const auto& quant = f.as_quantified();
        result = get_free_vars(quant.body);
        result.erase(quant.var);  // Bound variable is not free
    }

    return result;
}

std::string Sentence::to_string() const {
    if (root_ == INVALID_FORMULA) {
        return "<empty>";
    }
    return get_formula(root_).to_string(*this);
}

bool Sentence::operator==(const Sentence& other) const {
    return to_string() == other.to_string();
}

// ==================== Formula to_string ====================

static std::string term_to_string(const Term& t, const Sentence& s) {
    if (t.is_variable()) {
        return "x_" + std::to_string(t.as_variable());
    } else {
        return s.context().get_constant(t.as_constant()).name;
    }
}

static std::string formula_to_string_impl(formula_id id, const Sentence& s, int parent_prec);

static int precedence(Op op) {
    switch (op) {
        case Op::Iff: return 1;
        case Op::Implies: return 2;
        case Op::Or: return 3;
        case Op::And: return 4;
        case Op::Not: return 5;
        case Op::Forall:
        case Op::Exists: return 0;  // Quantifiers have lowest precedence
        case Op::Bottom: return 6;
    }
    return 0;
}

static std::string formula_to_string_impl(formula_id id, const Sentence& s, int parent_prec) {
    const Formula& f = s.get_formula(id);
    std::ostringstream oss;

    if (f.is_predicate()) {
        const auto& pred = f.as_predicate();
        oss << s.context().get_predicate(pred.predicate).name;
        if (!pred.args.empty()) {
            oss << "(";
            for (size_t i = 0; i < pred.args.size(); ++i) {
                if (i > 0) oss << ", ";
                oss << term_to_string(pred.args[i], s);
            }
            oss << ")";
        }
    } else if (f.is_compound()) {
        const auto& comp = f.as_compound();
        int my_prec = precedence(comp.op);

        switch (comp.op) {
            case Op::Bottom:
                oss << "_|_";
                break;
            case Op::Not:
                oss << "~";
                oss << formula_to_string_impl(comp.left, s, my_prec);
                break;
            case Op::And: {
                bool need_parens = my_prec < parent_prec;
                if (need_parens) oss << "(";
                oss << formula_to_string_impl(comp.left, s, my_prec);
                oss << " & ";
                oss << formula_to_string_impl(comp.right, s, my_prec);
                if (need_parens) oss << ")";
                break;
            }
            case Op::Or: {
                bool need_parens = my_prec < parent_prec;
                if (need_parens) oss << "(";
                oss << formula_to_string_impl(comp.left, s, my_prec);
                oss << " | ";
                oss << formula_to_string_impl(comp.right, s, my_prec);
                if (need_parens) oss << ")";
                break;
            }
            case Op::Implies: {
                bool need_parens = my_prec < parent_prec;
                if (need_parens) oss << "(";
                oss << formula_to_string_impl(comp.left, s, my_prec + 1);  // Left-assoc needs higher prec
                oss << " -> ";
                oss << formula_to_string_impl(comp.right, s, my_prec);
                if (need_parens) oss << ")";
                break;
            }
            case Op::Iff: {
                bool need_parens = my_prec < parent_prec;
                if (need_parens) oss << "(";
                oss << formula_to_string_impl(comp.left, s, my_prec + 1);
                oss << " <-> ";
                oss << formula_to_string_impl(comp.right, s, my_prec);
                if (need_parens) oss << ")";
                break;
            }
            default:
                oss << "<?>";
        }
    } else if (f.is_quantified()) {
        const auto& quant = f.as_quantified();
        bool need_parens = parent_prec > 0;
        if (need_parens) oss << "(";
        if (quant.op == Op::Forall) {
            oss << "forall x_" << quant.var << ". ";
        } else {
            oss << "exists x_" << quant.var << ". ";
        }
        oss << formula_to_string_impl(quant.body, s, 0);
        if (need_parens) oss << ")";
    }

    return oss.str();
}

std::string Formula::to_string(const Sentence& sentence) const {
    // Find this formula's id in the sentence
    // This is a bit awkward but necessary since Formula doesn't know its own id
    formula_id my_id = INVALID_FORMULA;
    for (size_t i = 0; i < sentence.formulas_.size(); ++i) {
        if (&sentence.formulas_[i] == this) {
            my_id = i + 1;
            break;
        }
    }
    if (my_id == INVALID_FORMULA) {
        return "<invalid>";
    }
    return formula_to_string_impl(my_id, sentence, 0);
}

// ==================== ConversionContext ====================

std::optional<var_index> ConversionContext::lookup_var(const std::string& name) const {
    // Search from innermost to outermost scope
    for (auto it = var_stack_.rbegin(); it != var_stack_.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            return found->second;
        }
    }
    return std::nullopt;
}

var_index ConversionContext::bind_var(const std::string& name) {
    if (var_stack_.empty()) {
        var_stack_.emplace_back();
    }
    var_index idx = next_var_++;
    var_stack_.back()[name] = idx;
    return idx;
}

}  // namespace core
