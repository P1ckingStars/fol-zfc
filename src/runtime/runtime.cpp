#include "runtime.h"
#include "../parser/parser.h"

#include <stdexcept>

namespace logic {

// === Runtime ===

void Runtime::axiom(const std::string& name, const std::string& sentence) {
    if (known_.count(name)) {
        throw std::runtime_error("Name already exists: " + name);
    }

    // Validate by parsing into a temporary db
    ProofDatabase temp_db;
    formula_id f = parse_formula(sentence, temp_db);
    if (f == 0) {
        throw std::runtime_error("Failed to parse formula: " + sentence);
    }

    if (!is_sentence(f, temp_db)) {
        throw std::runtime_error("Formula is not a sentence (has free variables): " + sentence);
    }

    known_[name] = sentence;
}

ProofContext Runtime::prove(const std::string& name, const std::string& sentence) {
    if (known_.count(name)) {
        throw std::runtime_error("Name already exists: " + name);
    }

    return ProofContext(*this, name, sentence);
}

bool Runtime::has(const std::string& name) const {
    return known_.count(name) > 0;
}

// === ProofContext ===

ProofContext::ProofContext(Runtime& rt, std::string name, const std::string& goal_str)
    : runtime_(rt)
    , name_(std::move(name))
    , goal_str_(goal_str)
    , proof_(local_db_)
{
    goal_ = parse_formula(goal_str_, local_db_);
    if (goal_ == 0) {
        throw std::runtime_error("Failed to parse goal: " + goal_str_);
    }

    if (!is_sentence(goal_, local_db_)) {
        throw std::runtime_error("Goal is not a sentence (has free variables): " + goal_str_);
    }
}

step_id ProofContext::use(const std::string& name) {
    // Check cache first
    auto it = premise_cache_.find(name);
    if (it != premise_cache_.end()) {
        return it->second;
    }

    // Look up in runtime
    auto known_it = runtime_.known_.find(name);
    if (known_it == runtime_.known_.end()) {
        throw std::runtime_error("Unknown axiom/theorem: " + name);
    }

    // Parse into local db
    formula_id f = parse_formula(known_it->second, local_db_);
    if (f == 0) {
        throw std::runtime_error("Failed to parse axiom/theorem: " + name);
    }

    // Create as premise (given truth, no discharge needed)
    step_id s = proof_.premise(f);

    // Track usage
    used_.insert(name);
    premise_cache_[name] = s;

    return s;
}

formula_id ProofContext::parse(const std::string& formula) {
    formula_id f = parse_formula(formula, local_db_);
    if (f == 0) {
        throw std::runtime_error("Failed to parse formula: " + formula);
    }
    return f;
}

assumption_id ProofContext::get_assumption_id(step_id s) const {
    const ProofStep& step = proof_.get_step(s);
    if (!step.assumption_label.has_value()) {
        throw std::runtime_error("Step is not an assumption");
    }
    return step.assumption_label.value();
}

// Proof construction - delegate to proof_

step_id ProofContext::assume(formula_id f) {
    return proof_.assume(f);
}

step_id ProofContext::premise(formula_id f) {
    return proof_.premise(f);
}

step_id ProofContext::and_intro(step_id left, step_id right) {
    return proof_.and_intro(left, right);
}

step_id ProofContext::and_elim_l(step_id conj) {
    return proof_.and_elim_l(conj);
}

step_id ProofContext::and_elim_r(step_id conj) {
    return proof_.and_elim_r(conj);
}

step_id ProofContext::or_intro_l(step_id left, formula_id right) {
    return proof_.or_intro_l(left, right);
}

step_id ProofContext::or_intro_r(formula_id left, step_id right) {
    return proof_.or_intro_r(left, right);
}

step_id ProofContext::implies_intro(assumption_id assumption, step_id conclusion) {
    return proof_.implies_intro(assumption, conclusion);
}

step_id ProofContext::implies_elim(step_id impl, step_id antecedent) {
    return proof_.implies_elim(impl, antecedent);
}

step_id ProofContext::not_intro(assumption_id assumption, step_id bottom) {
    return proof_.not_intro(assumption, bottom);
}

step_id ProofContext::not_elim(step_id negation, step_id positive) {
    return proof_.not_elim(negation, positive);
}

step_id ProofContext::bottom_elim(step_id bottom, formula_id conclusion) {
    return proof_.bottom_elim(bottom, conclusion);
}

step_id ProofContext::iff_intro(step_id lr, step_id rl) {
    return proof_.iff_intro(lr, rl);
}

step_id ProofContext::iff_elim_l(step_id iff) {
    return proof_.iff_elim_l(iff);
}

step_id ProofContext::iff_elim_r(step_id iff) {
    return proof_.iff_elim_r(iff);
}

step_id ProofContext::forall_intro(step_id body, var_index var) {
    return proof_.forall_intro(body, var);
}

step_id ProofContext::forall_elim(step_id forall_step, Term term) {
    return proof_.forall_elim(forall_step, term);
}

step_id ProofContext::exists_intro(step_id witness, var_index var, Term term) {
    return proof_.exists_intro(witness, var, term);
}

step_id ProofContext::exists_elim(step_id exists, step_id subproof, assumption_id assumption, var_index var) {
    return proof_.exists_elim(exists, subproof, assumption, var);
}

bool ProofContext::qed() {
    // Check proof concludes the goal
    formula_id conclusion = proof_.conclusion();
    if (conclusion != goal_) {
        return false;
    }

    // Check no undischarged assumptions
    // Note: premises (from use()) don't count as assumptions - they're given truths
    if (!proof_.active_assumptions().empty()) {
        return false;  // Has undischarged assumptions
    }

    // Register the theorem
    runtime_.known_[name_] = goal_str_;
    return true;
}

}  // namespace logic
