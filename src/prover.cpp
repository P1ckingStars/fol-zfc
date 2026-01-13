#include "prover.h"

namespace logic {

// ==================== ZFCProver Methods ====================

ProverResult ZFCProver::prove_with_assumptions(formula_id goal,
                                                const std::vector<formula_id>& extra_assumptions) {
    SearchState state(db_);

    // Add axioms
    for (const auto& [axiom, name] : axioms_) {
        auto step = state.proof->assume(axiom);
        add_derived(state, axiom, step);
    }

    // Add theorems
    for (const auto& [theorem, name] : theorems_) {
        auto step = state.proof->assume(theorem);
        add_derived(state, theorem, step);
    }

    // Add extra assumptions
    for (auto f : extra_assumptions) {
        auto step = state.proof->assume(f);
        add_derived(state, f, step);
    }

    // Check if goal is already derived
    if (is_derived(state, goal)) {
        return {true, std::move(state.proof), "Goal is an axiom/assumption", 0};
    }

    // Search for proof
    if (search(state, goal)) {
        return {true, std::move(state.proof), "Proof found", state.steps};
    }

    return {false, nullptr, "Could not find proof", state.steps};
}

bool ZFCProver::search(SearchState& state, formula_id goal) {
    // Check limits
    if (state.depth >= config_.max_depth || state.steps >= config_.max_steps) {
        return false;
    }
    state.steps++;
    state.depth++;

    if (config_.verbose) {
        std::cout << "[Prover] Searching for goal (depth=" << state.depth
                  << ", id=" << goal << ")\n";
    }

    // Already derived?
    if (is_derived(state, goal)) {
        state.depth--;
        return true;
    }

    // Try elimination rules first (may directly derive goal)
    if (try_elim_rules(state, goal)) {
        state.depth--;
        return true;
    }

    // Try introduction rules (backward reasoning)
    if (try_intro_rules(state, goal)) {
        state.depth--;
        return true;
    }

    state.depth--;
    return false;
}

bool ZFCProver::try_intro_rules(SearchState& state, formula_id goal) {
    if (try_and_intro(state, goal)) return true;
    if (try_implies_intro(state, goal)) return true;
    if (try_iff_intro(state, goal)) return true;
    if (try_or_intro(state, goal)) return true;
    if (try_not_intro(state, goal)) return true;
    if (try_forall_intro(state, goal)) return true;
    if (try_exists_intro(state, goal)) return true;
    return false;
}

bool ZFCProver::try_elim_rules(SearchState& state, formula_id goal) {
    if (try_modus_ponens(state, goal)) return true;
    if (try_and_elim(state, goal)) return true;
    if (try_forall_elim(state, goal)) return true;
    if (try_iff_elim(state, goal)) return true;
    return false;
}

// ==================== Introduction Rules ====================

bool ZFCProver::try_and_intro(SearchState& state, formula_id goal) {
    const Formula& f = db_.get_formula(goal);
    if (!f.is_compound() || f.as_compound().op != Op::And) return false;

    auto left = f.as_compound().left;
    auto right = f.as_compound().right;

    // Try to prove both conjuncts
    if (!search(state, left)) return false;
    if (!search(state, right)) return false;

    auto left_step = find_step(state, left);
    auto right_step = find_step(state, right);
    if (!left_step || !right_step) return false;

    auto step = state.proof->and_intro(*left_step, *right_step);
    add_derived(state, goal, step);
    return true;
}

bool ZFCProver::try_or_intro(SearchState& state, formula_id goal) {
    const Formula& f = db_.get_formula(goal);
    if (!f.is_compound() || f.as_compound().op != Op::Or) return false;

    auto left = f.as_compound().left;
    auto right = f.as_compound().right;

    // Try left disjunct first
    if (search(state, left)) {
        auto left_step = find_step(state, left);
        if (left_step) {
            auto step = state.proof->or_intro_l(*left_step, right);
            add_derived(state, goal, step);
            return true;
        }
    }

    // Try right disjunct
    if (search(state, right)) {
        auto right_step = find_step(state, right);
        if (right_step) {
            auto step = state.proof->or_intro_r(left, *right_step);
            add_derived(state, goal, step);
            return true;
        }
    }

    return false;
}

bool ZFCProver::try_implies_intro(SearchState& state, formula_id goal) {
    const Formula& f = db_.get_formula(goal);
    if (!f.is_compound() || f.as_compound().op != Op::Implies) return false;

    auto antecedent = f.as_compound().left;
    auto consequent = f.as_compound().right;

    // Assume antecedent
    auto assume_step = state.proof->assume(antecedent);
    assumption_id aid = *state.proof->get_step(assume_step).assumption_label;
    add_derived(state, antecedent, assume_step);

    // Try to prove consequent
    if (search(state, consequent)) {
        auto conseq_step = find_step(state, consequent);
        if (conseq_step) {
            auto step = state.proof->implies_intro(aid, *conseq_step);
            add_derived(state, goal, step);
            return true;
        }
    }

    return false;
}

bool ZFCProver::try_not_intro(SearchState& state, formula_id goal) {
    const Formula& f = db_.get_formula(goal);
    if (!f.is_compound() || f.as_compound().op != Op::Not) return false;

    auto inner = f.as_compound().left;
    auto bottom = db_.create_bottom();

    // Assume the inner formula
    auto assume_step = state.proof->assume(inner);
    assumption_id aid = *state.proof->get_step(assume_step).assumption_label;
    add_derived(state, inner, assume_step);

    // Try to derive bottom (contradiction)
    if (search(state, bottom)) {
        auto bottom_step = find_step(state, bottom);
        if (bottom_step) {
            auto step = state.proof->not_intro(aid, *bottom_step);
            add_derived(state, goal, step);
            return true;
        }
    }

    return false;
}

bool ZFCProver::try_iff_intro(SearchState& state, formula_id goal) {
    const Formula& f = db_.get_formula(goal);
    if (!f.is_compound() || f.as_compound().op != Op::Iff) return false;

    auto left = f.as_compound().left;
    auto right = f.as_compound().right;
    auto left_impl = db_.create_implies(left, right);
    auto right_impl = db_.create_implies(right, left);

    // Prove both directions
    if (!search(state, left_impl)) return false;
    if (!search(state, right_impl)) return false;

    auto left_step = find_step(state, left_impl);
    auto right_step = find_step(state, right_impl);
    if (!left_step || !right_step) return false;

    auto step = state.proof->iff_intro(*left_step, *right_step);
    add_derived(state, goal, step);
    return true;
}

bool ZFCProver::try_forall_intro(SearchState& state, formula_id goal) {
    const Formula& f = db_.get_formula(goal);
    if (!f.is_quantified() || f.as_quantified().op != Op::Forall) return false;

    auto var = f.as_quantified().var;
    auto body = f.as_quantified().body;

    // Try to prove body (with var as arbitrary)
    if (search(state, body)) {
        auto body_step = find_step(state, body);
        if (body_step) {
            auto step = state.proof->forall_intro(*body_step, var);
            add_derived(state, goal, step);
            return true;
        }
    }

    return false;
}

bool ZFCProver::try_exists_intro(SearchState& state, formula_id goal) {
    const Formula& f = db_.get_formula(goal);
    if (!f.is_quantified() || f.as_quantified().op != Op::Exists) return false;

    // This is tricky - we need to find a witness term
    // For now, just try with available constants
    return false;  // TODO: implement witness search
}

// ==================== Elimination Rules ====================

bool ZFCProver::try_modus_ponens(SearchState& state, formula_id goal) {
    // Look for A -> goal in derived formulas
    for (auto [fid, sid] : state.formula_to_step) {
        const Formula& f = db_.get_formula(fid);
        if (f.is_compound() && f.as_compound().op == Op::Implies) {
            if (f.as_compound().right == goal) {
                auto antecedent = f.as_compound().left;
                if (search(state, antecedent)) {
                    auto ante_step = find_step(state, antecedent);
                    if (ante_step) {
                        auto step = state.proof->implies_elim(sid, *ante_step);
                        add_derived(state, goal, step);
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

bool ZFCProver::try_and_elim(SearchState& state, formula_id goal) {
    // Look for goal & _ or _ & goal in derived formulas
    for (auto [fid, sid] : state.formula_to_step) {
        const Formula& f = db_.get_formula(fid);
        if (f.is_compound() && f.as_compound().op == Op::And) {
            if (f.as_compound().left == goal) {
                auto step = state.proof->and_elim_l(sid);
                add_derived(state, goal, step);
                return true;
            }
            if (f.as_compound().right == goal) {
                auto step = state.proof->and_elim_r(sid);
                add_derived(state, goal, step);
                return true;
            }
        }
    }
    return false;
}

bool ZFCProver::try_forall_elim(SearchState& state, formula_id goal) {
    // Look for forall x. P(x) where instantiating gives goal
    for (auto [fid, sid] : state.formula_to_step) {
        const Formula& f = db_.get_formula(fid);
        if (f.is_quantified() && f.as_quantified().op == Op::Forall) {
            // Try to find a term that when substituted gives goal
            // This is complex - for now, try available terms
            // TODO: proper unification
        }
    }
    return false;
}

bool ZFCProver::try_iff_elim(SearchState& state, formula_id goal) {
    // If goal is A -> B, look for A <-> B
    const Formula& g = db_.get_formula(goal);
    if (!g.is_compound() || g.as_compound().op != Op::Implies) return false;

    auto left = g.as_compound().left;
    auto right = g.as_compound().right;

    // Look for left <-> right
    auto iff1 = db_.create_iff(left, right);
    if (is_derived(state, iff1)) {
        auto iff_step = find_step(state, iff1);
        if (iff_step) {
            auto step = state.proof->iff_elim_l(*iff_step);
            add_derived(state, goal, step);
            return true;
        }
    }

    // Look for right <-> left
    auto iff2 = db_.create_iff(right, left);
    if (is_derived(state, iff2)) {
        auto iff_step = find_step(state, iff2);
        if (iff_step) {
            auto step = state.proof->iff_elim_r(*iff_step);
            add_derived(state, goal, step);
            return true;
        }
    }

    return false;
}

}  // namespace logic
