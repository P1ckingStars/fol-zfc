#pragma once

#include "formula.h"
#include "proof.h"
#include "parser.h"

#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <queue>
#include <set>
#include <unordered_set>
#include <vector>

namespace logic {

// ==================== Prover Configuration ====================

struct ProverConfig {
    size_t max_depth = 20;              // Maximum proof search depth
    size_t max_steps = 10000;           // Maximum proof steps to try
    bool verbose = false;               // Print debug info
};

// ==================== Prover Result ====================

struct ProverResult {
    bool success;
    std::unique_ptr<Proof> proof;
    std::string message;
    size_t steps_tried;
};

// ==================== ZFC Prover ====================

class ZFCProver {
public:
    ZFCProver(ProofDatabase& db, const ProverConfig& config = {})
        : db_(db), config_(config) {}

    // Add an axiom as available premise
    void add_axiom(formula_id axiom, const std::string& name = "") {
        axioms_.push_back({axiom, name});
        if (config_.verbose) {
            std::cout << "[Prover] Added axiom: " << name << " (id=" << axiom << ")\n";
        }
    }

    // Add a previously proven theorem as available premise
    void add_theorem(formula_id theorem, const std::string& name = "") {
        theorems_.push_back({theorem, name});
    }

    // Try to prove a formula from axioms and theorems
    ProverResult prove(formula_id goal) {
        return prove_with_assumptions(goal, {});
    }

    // Try to prove with specific additional assumptions
    ProverResult prove_with_assumptions(formula_id goal, const std::vector<formula_id>& extra_assumptions);

    // Get the proof database
    ProofDatabase& db() { return db_; }

private:
    ProofDatabase& db_;
    ProverConfig config_;
    std::vector<std::pair<formula_id, std::string>> axioms_;
    std::vector<std::pair<formula_id, std::string>> theorems_;

    // Proof search state
    struct SearchState {
        std::unique_ptr<Proof> proof;
        std::set<formula_id> derived;
        std::unordered_map<formula_id, step_id> formula_to_step;
        size_t depth;
        size_t steps;

        SearchState(ProofDatabase& db)
            : proof(std::make_unique<Proof>(db)), depth(0), steps(0) {}
    };

    // Backward search: try to prove goal
    bool search(SearchState& state, formula_id goal);

    // Find step that proves a formula
    std::optional<step_id> find_step(const SearchState& state, formula_id f) const {
        auto it = state.formula_to_step.find(f);
        if (it != state.formula_to_step.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    // Add a derived formula
    void add_derived(SearchState& state, formula_id f, step_id s) {
        state.derived.insert(f);
        state.formula_to_step[f] = s;
    }

    // Is formula already derived?
    bool is_derived(const SearchState& state, formula_id f) const {
        return state.derived.count(f) > 0;
    }

    // Try introduction rules (backward reasoning)
    bool try_intro_rules(SearchState& state, formula_id goal);

    // Try elimination rules (forward reasoning)
    bool try_elim_rules(SearchState& state, formula_id goal);

    // Specific strategies
    bool try_and_intro(SearchState& state, formula_id goal);
    bool try_or_intro(SearchState& state, formula_id goal);
    bool try_implies_intro(SearchState& state, formula_id goal);
    bool try_not_intro(SearchState& state, formula_id goal);
    bool try_iff_intro(SearchState& state, formula_id goal);
    bool try_forall_intro(SearchState& state, formula_id goal);
    bool try_exists_intro(SearchState& state, formula_id goal);

    bool try_modus_ponens(SearchState& state, formula_id goal);
    bool try_and_elim(SearchState& state, formula_id goal);
    bool try_forall_elim(SearchState& state, formula_id goal);
    bool try_iff_elim(SearchState& state, formula_id goal);
};

}  // namespace logic
