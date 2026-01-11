#include "prover.h"

#include <iostream>
#include <sstream>

namespace logic {

Prover::Prover(ProverConfig config) : config_(config) {}

ProverResult Prover::prove(const std::vector<FormulaPtr>& premises, FormulaPtr goal) {
    SearchState state;
    state.depth = 0;

    // Add premises to context
    for (const auto& p : premises) {
        state.context.emplace_back(p, make_premise(p));
    }

    auto result = search(state, goal);

    if (result) {
        return {true, result, "Proof found"};
    }
    return {false, std::nullopt, "No proof found"};
}

std::optional<ProofPtr> Prover::search(SearchState& state, FormulaPtr goal) {
    // Depth limit
    if (state.depth > config_.max_depth) {
        return std::nullopt;
    }

    // Loop detection
    auto key = goal_key(state, *goal);
    if (state.seen_goals.count(key)) {
        return std::nullopt;
    }
    state.seen_goals.insert(key);

    if (config_.verbose) {
        std::cerr << std::string(state.depth * 2, ' ')
                  << "Goal: " << to_string(*goal) << "\n";
    }

    state.depth++;

    // 1. Check if goal is directly in context
    if (auto proof = find_in_context(state, *goal)) {
        state.depth--;
        state.seen_goals.erase(key);
        return proof;
    }

    // 2. Try introduction rules (goal-directed)
    if (auto proof = try_intro_rules(state, goal)) {
        state.depth--;
        state.seen_goals.erase(key);
        return proof;
    }

    // 3. Try elimination rules (use context)
    if (auto proof = try_elim_rules(state, goal)) {
        state.depth--;
        state.seen_goals.erase(key);
        return proof;
    }

    state.depth--;
    state.seen_goals.erase(key);
    return std::nullopt;
}

std::optional<ProofPtr> Prover::find_in_context(const SearchState& state, const Formula& goal) {
    for (const auto& [f, p] : state.context) {
        if (*f == goal) {
            return p;
        }
    }
    return std::nullopt;
}

std::optional<ProofPtr> Prover::try_intro_rules(SearchState& state, FormulaPtr goal) {
    if (has_op(*goal, Op::And)) {
        if (auto p = try_and_intro(state, goal)) return p;
    }

    if (has_op(*goal, Op::Or)) {
        if (auto p = try_or_intro(state, goal)) return p;
    }

    if (has_op(*goal, Op::Implies)) {
        if (auto p = try_impl_intro(state, goal)) return p;
    }

    if (has_op(*goal, Op::Not)) {
        if (auto p = try_neg_intro(state, goal)) return p;
    }

    if (has_op(*goal, Op::Iff)) {
        if (auto p = try_iff_intro(state, goal)) return p;
    }

    return std::nullopt;
}

std::optional<ProofPtr> Prover::try_elim_rules(SearchState& state, FormulaPtr goal) {
    // Try modus ponens (most useful)
    if (auto p = try_modus_ponens(state, goal)) return p;

    // Try and elimination
    if (auto p = try_and_elim(state, goal)) return p;

    // Try or elimination (case split)
    if (auto p = try_or_elim(state, goal)) return p;

    // Try double negation elimination
    if (auto p = try_double_neg_elim(state, goal)) return p;

    // Try bottom elimination (ex falso)
    if (auto p = try_bottom_elim(state, goal)) return p;

    // Try iff elimination
    if (auto p = try_iff_elim(state, goal)) return p;

    return std::nullopt;
}

// Introduction rules

std::optional<ProofPtr> Prover::try_and_intro(SearchState& state, FormulaPtr goal) {
    const auto& comp = as_compound(*goal);
    auto left_goal = comp.args[0];
    auto right_goal = comp.args[1];

    auto left_proof = search(state, left_goal);
    if (!left_proof) return std::nullopt;

    auto right_proof = search(state, right_goal);
    if (!right_proof) return std::nullopt;

    auto result = engine_.and_intro(*left_proof, *right_proof);
    return result.success ? std::optional(result.proof) : std::nullopt;
}

std::optional<ProofPtr> Prover::try_or_intro(SearchState& state, FormulaPtr goal) {
    const auto& comp = as_compound(*goal);
    auto left = comp.args[0];
    auto right = comp.args[1];

    // Try proving left disjunct
    if (auto left_proof = search(state, left)) {
        auto result = engine_.or_intro_left(*left_proof, right);
        if (result.success) return result.proof;
    }

    // Try proving right disjunct
    if (auto right_proof = search(state, right)) {
        auto result = engine_.or_intro_right(left, *right_proof);
        if (result.success) return result.proof;
    }

    return std::nullopt;
}

std::optional<ProofPtr> Prover::try_impl_intro(SearchState& state, FormulaPtr goal) {
    const auto& comp = as_compound(*goal);
    auto antecedent = comp.args[0];
    auto consequent = comp.args[1];

    // Assume antecedent
    auto assumption = make_assumption(antecedent);
    push_to_context(state, antecedent, assumption);

    // Try to prove consequent
    auto consequent_proof = search(state, consequent);

    pop_from_context(state);

    if (consequent_proof) {
        auto result = engine_.implies_intro(antecedent, *consequent_proof);
        if (result.success) return result.proof;
    }

    return std::nullopt;
}

std::optional<ProofPtr> Prover::try_neg_intro(SearchState& state, FormulaPtr goal) {
    const auto& comp = as_compound(*goal);
    auto inner = comp.args[0];

    // Assume inner (to derive ⊥)
    auto assumption = make_assumption(inner);
    push_to_context(state, inner, assumption);

    // Try to derive ⊥
    auto bottom_proof = search(state, bottom());

    pop_from_context(state);

    if (bottom_proof) {
        auto result = engine_.not_intro(inner, *bottom_proof);
        if (result.success) return result.proof;
    }

    return std::nullopt;
}

std::optional<ProofPtr> Prover::try_iff_intro(SearchState& state, FormulaPtr goal) {
    const auto& comp = as_compound(*goal);
    auto left = comp.args[0];
    auto right = comp.args[1];

    // Prove left → right
    auto left_impl = impl(left, right);
    auto left_proof = search(state, left_impl);
    if (!left_proof) return std::nullopt;

    // Prove right → left
    auto right_impl = impl(right, left);
    auto right_proof = search(state, right_impl);
    if (!right_proof) return std::nullopt;

    auto result = engine_.iff_intro(*left_proof, *right_proof);
    return result.success ? std::optional(result.proof) : std::nullopt;
}

// Elimination rules

std::optional<ProofPtr> Prover::try_modus_ponens(SearchState& state, FormulaPtr goal) {
    // Look for implications in context that conclude with our goal
    for (const auto& [f, p] : state.context) {
        if (has_op(*f, Op::Implies)) {
            const auto& comp = as_compound(*f);
            if (*comp.args[1] == *goal) {
                // Found A → goal, try to prove A
                auto antecedent = comp.args[0];
                if (auto ant_proof = search(state, antecedent)) {
                    auto result = engine_.implies_elim(*ant_proof, p);
                    if (result.success) return result.proof;
                }
            }
        }
    }
    return std::nullopt;
}

std::optional<ProofPtr> Prover::try_and_elim(SearchState& state, FormulaPtr goal) {
    // Look for conjunctions in context
    for (const auto& [f, p] : state.context) {
        if (has_op(*f, Op::And)) {
            const auto& comp = as_compound(*f);

            // Check if left conjunct matches goal
            if (*comp.args[0] == *goal) {
                auto result = engine_.and_elim_left(p);
                if (result.success) return result.proof;
            }

            // Check if right conjunct matches goal
            if (*comp.args[1] == *goal) {
                auto result = engine_.and_elim_right(p);
                if (result.success) return result.proof;
            }
        }
    }
    return std::nullopt;
}

std::optional<ProofPtr> Prover::try_or_elim(SearchState& state, FormulaPtr goal) {
    // Look for disjunctions in context
    for (const auto& [f, p] : state.context) {
        if (has_op(*f, Op::Or)) {
            const auto& comp = as_compound(*f);
            auto left = comp.args[0];
            auto right = comp.args[1];

            // Try case split: assume left, prove goal
            auto left_assumption = make_assumption(left);
            push_to_context(state, left, left_assumption);
            auto left_case = search(state, goal);
            pop_from_context(state);

            if (!left_case) continue;

            // Assume right, prove goal
            auto right_assumption = make_assumption(right);
            push_to_context(state, right, right_assumption);
            auto right_case = search(state, goal);
            pop_from_context(state);

            if (!right_case) continue;

            // Build the case proofs as implications
            auto left_impl_result = engine_.implies_intro(left, *left_case);
            auto right_impl_result = engine_.implies_intro(right, *right_case);

            if (left_impl_result.success && right_impl_result.success) {
                auto result = engine_.or_elim(p, left_impl_result.proof, right_impl_result.proof);
                if (result.success) return result.proof;
            }
        }
    }
    return std::nullopt;
}

std::optional<ProofPtr> Prover::try_double_neg_elim(SearchState& state, FormulaPtr goal) {
    // Look for ¬¬goal in context
    auto double_neg = neg(neg(goal));

    if (auto proof = find_in_context(state, *double_neg)) {
        auto result = engine_.not_elim(*proof);
        if (result.success) return result.proof;
    }

    // Or try to prove ¬¬goal
    if (auto proof = search(state, double_neg)) {
        auto result = engine_.not_elim(*proof);
        if (result.success) return result.proof;
    }

    return std::nullopt;
}

std::optional<ProofPtr> Prover::try_bottom_elim(SearchState& state, FormulaPtr goal) {
    // Check if ⊥ is already in context
    if (auto bottom_proof = find_in_context(state, *bottom())) {
        auto result = engine_.bottom_elim(*bottom_proof, goal);
        if (result.success) return result.proof;
    }

    // Check for direct contradiction in context (A and ¬A)
    for (const auto& [f, p] : state.context) {
        // Case 1: f is ¬A, look for A
        if (has_op(*f, Op::Not)) {
            auto inner = as_compound(*f).args[0];
            if (auto inner_proof = find_in_context(state, *inner)) {
                auto bottom_result = engine_.bottom_intro(*inner_proof, p);
                if (bottom_result.success) {
                    auto result = engine_.bottom_elim(bottom_result.proof, goal);
                    if (result.success) return result.proof;
                }
            }
        }

        // Case 2: f is A, look for ¬A
        auto negated = neg(f);
        if (auto neg_proof = find_in_context(state, *negated)) {
            auto bottom_result = engine_.bottom_intro(p, *neg_proof);
            if (bottom_result.success) {
                auto result = engine_.bottom_elim(bottom_result.proof, goal);
                if (result.success) return result.proof;
            }
        }
    }

    // Try to derive a formula that contradicts something in context
    // Look for ¬X in context, try to prove X via modus ponens
    for (const auto& [f, p] : state.context) {
        if (has_op(*f, Op::Not)) {
            auto inner = as_compound(*f).args[0];
            // Try to prove inner using modus ponens
            for (const auto& [f2, p2] : state.context) {
                if (has_op(*f2, Op::Implies)) {
                    const auto& impl_comp = as_compound(*f2);
                    if (*impl_comp.args[1] == *inner) {
                        // Found X → inner, try to prove X
                        auto antecedent = impl_comp.args[0];
                        if (auto ant_proof = find_in_context(state, *antecedent)) {
                            auto mp_result = engine_.implies_elim(*ant_proof, p2);
                            if (mp_result.success) {
                                // Now we have inner and ¬inner
                                auto bottom_result = engine_.bottom_intro(mp_result.proof, p);
                                if (bottom_result.success) {
                                    auto result = engine_.bottom_elim(bottom_result.proof, goal);
                                    if (result.success) return result.proof;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return std::nullopt;
}

std::optional<ProofPtr> Prover::try_iff_elim(SearchState& state, FormulaPtr goal) {
    // Check if goal is an implication that can come from a biconditional
    if (has_op(*goal, Op::Implies)) {
        const auto& comp = as_compound(*goal);
        auto left = comp.args[0];
        auto right = comp.args[1];

        // Look for left ↔ right in context
        auto bicon1 = iff(left, right);
        if (auto proof = find_in_context(state, *bicon1)) {
            auto result = engine_.iff_elim_left(*proof);
            if (result.success) return result.proof;
        }

        // Look for right ↔ left in context
        auto bicon2 = iff(right, left);
        if (auto proof = find_in_context(state, *bicon2)) {
            auto result = engine_.iff_elim_right(*proof);
            if (result.success) return result.proof;
        }
    }
    return std::nullopt;
}

// Helpers

void Prover::push_to_context(SearchState& state, FormulaPtr f, ProofPtr p) {
    state.context.emplace_back(f, p);
}

void Prover::pop_from_context(SearchState& state) {
    state.context.pop_back();
}

std::string Prover::goal_key(const SearchState& state, const Formula& goal) {
    std::ostringstream ss;
    ss << to_string(goal) << "@";
    for (const auto& [f, _] : state.context) {
        ss << to_string(*f) << ";";
    }
    return ss.str();
}

}  // namespace logic
