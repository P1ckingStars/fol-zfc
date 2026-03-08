#pragma once

#include "formula.h"
#include "src/util/error.h"
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace logic {

using FormulaResult = util::Result<FormulaHandle>;

// ========== Scope Dependency Tracking ==========
// Bitmask tracking which scopes a derived formula depends on.
// Bit i set means the formula depends on scope at stack index i.
using ScopeDeps = uint64_t;
constexpr int MAX_SCOPE_DEPTH = 64;

inline ScopeDeps scope_dep(int i) { return i < 0 ? 0 : (1ULL << i); }
inline ScopeDeps discharge(ScopeDeps d, int i) { return d & ~(1ULL << i); }
inline int deepest(ScopeDeps d) { return d == 0 ? -1 : 63 - __builtin_clzll(d); }

class AssumptionScope {
    FormulaHandle assumption_;
    int bit_;
public:
    AssumptionScope(FormulaHandle const& formula, int bit);
    FormulaHandle const& get_formula() const;
    int bit() const { return bit_; }
};

class FixVarScope {
    std::unique_ptr<FormulaHandle> result_;  // Heap-allocated so reference survives moves
    std::unique_ptr<QuantifierBuilder> qbuilder_;  // Must be after result_ since it depends on it
    int bit_;

public:
    FixVarScope(FormulaBuilder& builder, Op op, int bit);
    Term var_term() const;  // Returns the bound variable as Term
    Op get_op() const { return qbuilder_->get_op(); }
    int bit() const { return bit_; }

    // Set body and destroy QuantifierBuilder to create the formula
    FormulaHandle finalize(FormulaHandle body);
};

using Scope = std::variant<AssumptionScope, FixVarScope>;

class ProofStack {
protected:
    std::vector<Scope> scopes;
    std::unordered_map<FormulaHandle, ScopeDeps> formula_deps_;
    FormulaBuilder formula_builder_;
    std::optional<Term> last_witness_var_;

    // Safe lookup: returns 0 (no deps) if formula not in map
    ScopeDeps deps_of(FormulaHandle const& f) const;

    // Compute which fix-var scopes' variables appear in the formula
    ScopeDeps compute_var_deps(FormulaHandle const& f) const;

    // Single derivation method: places result at correct scope based on deps
    void derive_with_deps(FormulaHandle const& f, ScopeDeps proof_deps);

    // Erase all formula_deps_ entries that depend on the given scope bit
    void cleanup_scope(int bit);

    // Clean up and pop the current (innermost) scope
    void close_current_scope();

    // Helper to check if a term is accessible (fixed vars must be in scope)
    bool is_term_accessible(Term const& t) const;

    // Helper to find the scope index that introduced a fixed variable (-1 if not found)
    int find_scope_for_term(Term const& t) const;

    // Helper to check if a formula contains a specific fixed variable
    bool formula_contains_fixed_var(FormulaHandle const& f, var_index var_idx) const;

    // Helper to create existential by generalizing a witness term in a formula
    FormulaHandle make_exists_from_witness(FormulaHandle const& body, Term const& witness);

public:
    ProofStack(GlobalContext & context);

    Term fix_var();
    FormulaHandle assume(FormulaHandle const & formula);
    bool is_derived(FormulaHandle const &a) const;
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
    // Returns the witness variable from the last exists_elim call
    std::optional<Term> last_witness_var() const { return last_witness_var_; }

    // ========== Equality Substitution ==========
    // From eq(a, b) and φ(a), derive φ(b) by replacing a with b
    FormulaResult eq_subst(FormulaHandle const &eq_formula, FormulaHandle const &target);

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
