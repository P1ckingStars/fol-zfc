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
// Dynamic bitset tracking which scopes a derived formula depends on.
// Bit i set means the formula depends on scope at stack index i.
// Supports arbitrary scope depth (no 64-bit limit).
class ScopeDeps {
    std::vector<uint64_t> words_;

    static size_t word_idx(int i) { return i / 64; }
    static uint64_t word_bit(int i) { return 1ULL << (i % 64); }

public:
    ScopeDeps() = default;

    static ScopeDeps from_bit(int i) {
        if (i < 0) return {};
        ScopeDeps d;
        d.words_.resize(word_idx(i) + 1, 0);
        d.words_[word_idx(i)] |= word_bit(i);
        return d;
    }

    bool empty() const {
        for (auto w : words_) if (w) return false;
        return true;
    }

    bool test(int i) const {
        size_t wi = word_idx(i);
        return wi < words_.size() && (words_[wi] & word_bit(i)) != 0;
    }

    void set(int i) {
        size_t wi = word_idx(i);
        if (wi >= words_.size()) words_.resize(wi + 1, 0);
        words_[wi] |= word_bit(i);
    }

    void clear(int i) {
        size_t wi = word_idx(i);
        if (wi < words_.size()) words_[wi] &= ~word_bit(i);
    }

    int deepest() const {
        for (int w = static_cast<int>(words_.size()) - 1; w >= 0; --w) {
            if (words_[w]) return w * 64 + 63 - __builtin_clzll(words_[w]);
        }
        return -1;
    }

    ScopeDeps operator|(ScopeDeps const& o) const {
        ScopeDeps r;
        r.words_.resize(std::max(words_.size(), o.words_.size()), 0);
        for (size_t i = 0; i < words_.size(); ++i) r.words_[i] |= words_[i];
        for (size_t i = 0; i < o.words_.size(); ++i) r.words_[i] |= o.words_[i];
        return r;
    }

    ScopeDeps& operator|=(ScopeDeps const& o) {
        if (o.words_.size() > words_.size()) words_.resize(o.words_.size(), 0);
        for (size_t i = 0; i < o.words_.size(); ++i) words_[i] |= o.words_[i];
        return *this;
    }
};

inline ScopeDeps scope_dep(int i) { return ScopeDeps::from_bit(i); }

inline ScopeDeps discharge(ScopeDeps d, int i) {
    d.clear(i);
    return d;
}

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

    // Helper to check if all fixed vars in a description body are in live scopes
    bool description_body_accessible(FormulaHandle const& body) const;

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

    // Schema instantiation: substitute schema vars with bindings
    FormulaResult schema_inst(const SchemaDefinition& schema,
                              const std::vector<SchemaBind>& bindings);

    // Access formula builder for creating formulas
    FormulaBuilder& builder() { return formula_builder_; }
};

// ========== Classical Logic Extensions ==========
// These rules are valid in classical logic but not intuitionistic logic.
// They can be added to ProofStack or used via ClassicalProofStack.

class ClassicalProofStack : public ProofStack {
    std::optional<Term> last_iota_term_;

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

    // ========== Definite Descriptions (Hilbert ε) ==========
    // From ∃x.φ(x), derive φ(ιx.φ(x)) where ιx.φ(x) is the description term
    FormulaResult iota_elim(FormulaHandle const &exists_formula);
    // Returns the iota term from the last iota_elim call
    std::optional<Term> last_iota_term() const { return last_iota_term_; }
};

}
