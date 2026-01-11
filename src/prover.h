#pragma once

#include "rule_engine.h"

#include <optional>
#include <set>
#include <vector>

namespace logic {

struct ProverConfig {
    int max_depth = 50;
    bool verbose = false;
};

// Result of proof search
struct ProverResult {
    bool success;
    std::optional<ProofPtr> proof;
    std::string message;
};

// Backward-chaining proof search
class Prover {
public:
    explicit Prover(ProverConfig config = {});

    // Prove goal from premises
    ProverResult prove(const std::vector<FormulaPtr>& premises, FormulaPtr goal);

private:
    struct SearchState {
        std::vector<std::pair<FormulaPtr, ProofPtr>> context;  // Available formulas
        std::set<std::string> seen_goals;  // Loop detection
        int depth;
    };

    ProverConfig config_;
    RuleEngine engine_;

    // Core search
    std::optional<ProofPtr> search(SearchState& state, FormulaPtr goal);

    // Try to find goal directly in context
    std::optional<ProofPtr> find_in_context(const SearchState& state, const Formula& goal);

    // Introduction rule strategies (goal-directed)
    std::optional<ProofPtr> try_intro_rules(SearchState& state, FormulaPtr goal);

    // Elimination rule strategies (forward from context)
    std::optional<ProofPtr> try_elim_rules(SearchState& state, FormulaPtr goal);

    // Specific intro rules
    std::optional<ProofPtr> try_and_intro(SearchState& state, FormulaPtr goal);
    std::optional<ProofPtr> try_or_intro(SearchState& state, FormulaPtr goal);
    std::optional<ProofPtr> try_impl_intro(SearchState& state, FormulaPtr goal);
    std::optional<ProofPtr> try_neg_intro(SearchState& state, FormulaPtr goal);
    std::optional<ProofPtr> try_iff_intro(SearchState& state, FormulaPtr goal);

    // Specific elim rules
    std::optional<ProofPtr> try_modus_ponens(SearchState& state, FormulaPtr goal);
    std::optional<ProofPtr> try_and_elim(SearchState& state, FormulaPtr goal);
    std::optional<ProofPtr> try_or_elim(SearchState& state, FormulaPtr goal);
    std::optional<ProofPtr> try_double_neg_elim(SearchState& state, FormulaPtr goal);
    std::optional<ProofPtr> try_bottom_elim(SearchState& state, FormulaPtr goal);
    std::optional<ProofPtr> try_iff_elim(SearchState& state, FormulaPtr goal);

    // Helper to add to context temporarily
    void push_to_context(SearchState& state, FormulaPtr f, ProofPtr p);
    void pop_from_context(SearchState& state);

    // Goal tracking for loop detection
    std::string goal_key(const SearchState& state, const Formula& goal);
};

}  // namespace logic
