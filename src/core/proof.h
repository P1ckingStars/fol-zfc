#pragma once

#include "formula.h"
#include "src/util/error.h"
#include "src/util/registry.h"
#include <optional>
#include <vector>

namespace logic {

using FormulaResult = util::Result<FormulaHandle>;

class AssumptionScope {
    // Formulas deduced from current scope need to be put under the same scope
    std::set<FormulaHandle> derived_;

    FormulaHandle assumption_;
public:
    AssumptionScope(FormulaHandle const & formula) {
        /* TODO: 1. move handle into local formulas_
        *  2. Add handle into derived
        *  3. populate assumption
        */
    }
    FormulaHandle const& get_formula() {
        return assumption_;
    }
    bool contains(FormulaHandle const & f) {
        return derived_.contains(f);
    }
    void derive(FormulaHandle const & handle) {
        derived_.insert(handle);
    }
};

class FixVarScope  {
    // Formulas deduced from current scope need to be put under the same scope
    std::set<FormulaHandle> derived_;
    // Fix the variable, so we can substite this variable in other places
    var_index var_;

public:
    FixVarScope(var_index& index): var_(index) {
    }
    var_index get_var() const {
        return var_;
    }
    bool contains(FormulaHandle const & f) const {
        return derived_.contains(f);
    }
    void derive(FormulaHandle const & handle) {
        derived_.insert(handle);
    }
};

using Scope = std::variant<AssumptionScope, FixVarScope>;

class ProofStack {
    std::vector<Scope> scopes;
    var_index current_ = 0;
    std::set<FormulaHandle> derived_;
    FormulaBuilder formula_builder_;
public:
    ProofStack(GlobalContext & context): formula_builder_(context) {}
    var_index fix_var() {
        scopes.push_back(FixVarScope(current_));
        return current_++;
    }
    FormulaHandle assume(FormulaHandle const & formula) {
        scopes.push_back(AssumptionScope(formula));
        return std::get<AssumptionScope>(*scopes.rbegin()).get_formula();
    }

    util::Result<FormulaHandle> use_theorem(SentenceHandle & sentence) {
        auto res = formula_builder_.add_sentence(sentence);
        derived_.insert(res);
        return res;
    }
    
    FormulaResult and_intro(FormulaHandle const &a, FormulaHandle const &b) {
        if (!is_derived(a)) {
            return MAKE_ERROR << "Formula is not derived: " << a.get();
        }
        if (!is_derived(b)) {
            return MAKE_ERROR << "Formula is not derived: " << b.get();
        }
        auto formula = formula_builder_.make_and(a, b);
        std::visit([&](auto&& scope)
        {
            scope.derive(formula);
        }, *scopes.rbegin());
        return formula;
    }

    util::Result<FormulaHandle> conclude_assumption(const FormulaHandle & conclusion) {
        if (!std::holds_alternative<AssumptionScope>(*scopes.rbegin())) {
            return MAKE_ERROR << "Current scope is not assumption, conclude for quantifier first";
        }
        bool conclusion_valid = false;
        FormulaHandle assumption;
        std::visit([&](auto&& arg)
        {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, AssumptionScope>) {
                if (arg.contains(conclusion)) {
                    conclusion_valid = true;
                    assumption = arg.get_formula();
                }
            }
        }, *scopes.rbegin());
        if (!conclusion_valid) {
            return MAKE_ERROR << "Conclusion not exists" << conclusion.get();
        }
        auto new_formula = formula_builder_.make_implies(assumption, conclusion);
        if (scopes.size() > 1) {
            std::visit([&](auto&& arg)
            {
                arg.derive(new_formula);
            }, scopes[scopes.size() - 2]);
        }
        return new_formula;
    }
    void pop() {
        scopes.pop_back();
    }

    bool is_derived(FormulaHandle const &a) {
        if (derived_.contains(a)) {
            return true;
        }
        bool found_scope = false;
        for (auto it = scopes.rbegin(); it != scopes.rend(); it++) {
            std::visit([&](auto&& arg)
            {
                if (arg.contains(a)) {
                    found_scope = true;
                }
            }, *it);
            if (found_scope) {
                return true;
            }
        }
        return false;
    }
};

}
