#pragma once

#include "formula.h"
#include "src/util/error.h"
#include "src/util/registry.h"
#include <memory>
#include <optional>
#include <unordered_set>
#include <vector>

namespace logic {

using FormulaResult = util::Result<FormulaHandle>;

class AssumptionScope {
    std::unordered_set<FormulaHandle> derived_;
    FormulaHandle assumption_;
public:
    AssumptionScope(FormulaHandle const & formula);
    FormulaHandle const& get_formula();
    bool contains(FormulaHandle const & f);
    void derive(FormulaHandle const & handle);
};

class FixVarScope {
    std::unordered_set<FormulaHandle> derived_;
    std::unique_ptr<QuantifierBuilder> qbuilder_;
    FormulaHandle result_;  // Where QuantifierBuilder stores the result

public:
    FixVarScope(FormulaBuilder& builder, Op op = Op::Forall);  // Creates QuantifierBuilder with Op::Forall
    Term var_term() const;  // Returns the bound variable as Term
    bool contains(FormulaHandle const & f) const;
    void derive(FormulaHandle const & handle);
    Op get_op() const { return qbuilder_->get_op(); }

    // Set body and destroy QuantifierBuilder to create the formula
    FormulaHandle finalize(FormulaHandle body);
};

using Scope = std::variant<AssumptionScope, FixVarScope>;

class ProofStack {
protected:
    std::vector<Scope> scopes;
    std::unordered_set<FormulaHandle> derived_;
    FormulaBuilder formula_builder_;

    // Helper to derive formula in current scope
    void derive_in_current_scope(FormulaHandle const& formula);

    // Helper to derive formula in a specific scope (by index, -1 means base level)
    void derive_in_scope(FormulaHandle const& formula, int scope_idx);

    // Helper to check if a term is accessible (constants always are, fixed vars must be in scope)
    bool is_term_accessible(Term const& t) const;

    // Helper to find the scope index that introduced a fixed variable (-1 if not found or constant)
    int find_scope_for_term(Term const& t) const;

    // Helper to check if a formula contains a specific fixed variable
    bool formula_contains_fixed_var(FormulaHandle const& f, var_index var_idx) const;

    // Helper to create existential by generalizing a witness term in a formula
    FormulaHandle make_exists_from_witness(FormulaHandle const& body, Term const& witness);

public:
    ProofStack(GlobalContext & context);

    Term fix_var();
    FormulaHandle assume(FormulaHandle const & formula);
    bool is_derived(FormulaHandle const &a);
    void pop();

    // Use a proven theorem
    FormulaResult use_theorem(SentenceHandle & sentence);

    // ========== Conjunction (And) ==========
    // From A and B, derive A ∧ B
    FormulaResult and_intro(FormulaHandle const &a, FormulaHandle const &b);
    // From A ∧ B, derive A
    FormulaResult and_elim_l(FormulaHandle const &and_formula);
    // From A ∧ B, derive B
    FormulaResult and_elim_r(FormulaHandle const &and_formula);

    // ========== Disjunction (Or) ==========
    // From A, derive A ∨ B
    FormulaResult or_intro_l(FormulaHandle const &a, FormulaHandle const &b);
    // From B, derive A ∨ B
    FormulaResult or_intro_r(FormulaHandle const &a, FormulaHandle const &b);
    // From A ∨ B, A → C, B → C, derive C
    FormulaResult or_elim(FormulaHandle const &or_formula,
                          FormulaHandle const &a_implies_c,
                          FormulaHandle const &b_implies_c);

    // ========== Implication ==========
    // Conclude assumption scope: from assumption A deriving B, get A → B
    FormulaResult implies_intro(FormulaHandle const & conclusion);
    // Modus ponens: from A → B and A, derive B
    FormulaResult implies_elim(FormulaHandle const &implication, FormulaHandle const &antecedent);

    // ========== Negation ==========
    // From assumption A leading to ⊥, derive ¬A
    FormulaResult not_intro(FormulaHandle const &bottom);
    // From ¬A and A, derive ⊥
    FormulaResult not_elim(FormulaHandle const &negation, FormulaHandle const &formula);

    // ========== Bottom (Falsum) ==========
    // Ex falso quodlibet: from ⊥, derive any formula
    FormulaResult bottom_elim(FormulaHandle const &bottom, FormulaHandle const &formula);

    // ========== Biconditional (Iff) ==========
    // From A → B and B → A, derive A ↔ B
    FormulaResult iff_intro(FormulaHandle const &a_implies_b, FormulaHandle const &b_implies_a);
    // From A ↔ B and A, derive B
    FormulaResult iff_elim_l(FormulaHandle const &iff_formula, FormulaHandle const &a);
    // From A ↔ B and B, derive A
    FormulaResult iff_elim_r(FormulaHandle const &iff_formula, FormulaHandle const &b);

    // ========== Forall =========

    FormulaResult forall_intro(FormulaHandle const &formula);
    FormulaResult forall_elim(FormulaHandle const &formula, Term const & term);

    // ========== Exists =========

    // Two modes:
    // 1. In Exists scope: closes scope, derives conclusion in parent (must not contain witness)
    // 2. With witness term: from φ(t), derive ∃x.φ(x) by generalizing t
    FormulaResult exists_intro(FormulaHandle const &body, std::optional<Term> witness = std::nullopt);
    // Opens Exists scope with fresh witness, returns φ(c) from ∃x.φ(x)
    FormulaResult exists_elim(FormulaHandle const &formula);

    // Access formula builder for creating formulas
    FormulaBuilder& builder() { return formula_builder_; }
};

// ========== Classical Logic Extensions ==========
// These rules are valid in classical logic but not intuitionistic logic.
// They can be added to ProofStack or used via ClassicalProofStack.

class ClassicalProofStack : public ProofStack {
public:
    using ProofStack::ProofStack;  // Inherit constructor

    // ========== Double Negation Elimination ==========
    // From ¬¬A, derive A (not valid in intuitionistic logic)
    FormulaResult double_neg_elim(FormulaHandle const &double_neg);

    // ========== Law of Excluded Middle ==========
    // Derive A ∨ ¬A for any formula A (not valid in intuitionistic logic)
    FormulaResult excluded_middle(FormulaHandle const &formula);

    // ========== Proof by Contradiction (Classical RAA) ==========
    // From assumption ¬A leading to ⊥, derive A
    // (Different from not_intro which derives ¬A from assumption A leading to ⊥)
    FormulaResult classical_absurd(FormulaHandle const &bottom);

    // ========== Peirce's Law ==========
    // Derive ((A → B) → A) → A (equivalent to LEM)
    FormulaResult peirce(FormulaHandle const &a, FormulaHandle const &b);
};

}
