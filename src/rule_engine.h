#pragma once

#include "formula.h"

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace logic {

// Forward declarations
struct Proof;
using ProofPtr = std::shared_ptr<const Proof>;

// Inference rules
enum class Rule {
    // Structural
    Assumption,
    Premise,

    // Introduction rules
    AndIntro,      // From A and B, derive A ∧ B
    OrIntroLeft,   // From A, derive A ∨ B
    OrIntroRight,  // From B, derive A ∨ B
    ImpliesIntro,  // Assume A, derive B, discharge to get A → B
    NotIntro,      // Assume A, derive ⊥, discharge to get ¬A
    IffIntro,      // From A → B and B → A, derive A ↔ B
    BottomIntro,   // From A and ¬A, derive ⊥

    // Elimination rules
    AndElimLeft,   // From A ∧ B, derive A
    AndElimRight,  // From A ∧ B, derive B
    OrElim,        // From A ∨ B, A → C, B → C, derive C
    ImpliesElim,   // From A and A → B, derive B (modus ponens)
    NotElim,       // From ¬¬A, derive A (double negation elimination)
    IffElimLeft,   // From A ↔ B, derive A → B
    IffElimRight,  // From A ↔ B, derive B → A
    BottomElim,    // From ⊥, derive anything (ex falso)
};

std::string rule_name(Rule r);

// A proof tree node
struct Proof {
    FormulaPtr conclusion;
    Rule rule;
    std::vector<ProofPtr> premises;
    std::optional<FormulaPtr> discharged;  // For →I, ¬I: the discharged assumption

    bool operator==(const Proof&) const = default;
};

// Result of applying a rule
struct RuleResult {
    bool success;
    std::string error;
    ProofPtr proof;
};

// Proof context - tracks available formulas and assumptions
class Context {
public:
    struct Entry {
        FormulaPtr formula;
        ProofPtr proof;
        int scope_level;
        bool is_assumption;
    };

    Context() : scope_level_(0) {}

    // Scope management for hypothetical reasoning
    void push_scope();
    void pop_scope();
    int scope_level() const { return scope_level_; }

    // Add formulas to context
    void add_premise(FormulaPtr f);
    ProofPtr add_assumption(FormulaPtr f);
    void add_derived(FormulaPtr f, ProofPtr proof);

    // Query context
    std::optional<ProofPtr> find(const Formula& f) const;
    bool contains(const Formula& f) const;
    std::vector<Entry> all_entries() const { return entries_; }
    std::vector<Entry> current_scope_entries() const;

private:
    std::vector<Entry> entries_;
    int scope_level_;
};

// Rule engine - applies inference rules
class RuleEngine {
public:
    // Introduction rules
    RuleResult and_intro(ProofPtr left, ProofPtr right);
    RuleResult or_intro_left(ProofPtr a, FormulaPtr b);
    RuleResult or_intro_right(FormulaPtr a, ProofPtr b);
    RuleResult implies_intro(FormulaPtr assumption, ProofPtr consequent);
    RuleResult not_intro(FormulaPtr assumption, ProofPtr bottom_proof);
    RuleResult iff_intro(ProofPtr left_impl, ProofPtr right_impl);
    RuleResult bottom_intro(ProofPtr a, ProofPtr not_a);

    // Elimination rules
    RuleResult and_elim_left(ProofPtr conj);
    RuleResult and_elim_right(ProofPtr conj);
    RuleResult or_elim(ProofPtr disj, ProofPtr case_left, ProofPtr case_right);
    RuleResult implies_elim(ProofPtr antecedent, ProofPtr implication);
    RuleResult not_elim(ProofPtr double_neg);
    RuleResult iff_elim_left(ProofPtr biconditional);
    RuleResult iff_elim_right(ProofPtr biconditional);
    RuleResult bottom_elim(ProofPtr bottom_proof, FormulaPtr conclusion);

private:
    RuleResult make_error(const std::string& msg);
    RuleResult make_success(FormulaPtr conclusion, Rule rule,
                           std::vector<ProofPtr> premises,
                           std::optional<FormulaPtr> discharged = std::nullopt);
};

// Utility to create premise/assumption proofs
ProofPtr make_premise(FormulaPtr f);
ProofPtr make_assumption(FormulaPtr f);

// Pretty print a proof tree
std::string print_proof(const Proof& p, int indent = 0);

}  // namespace logic
