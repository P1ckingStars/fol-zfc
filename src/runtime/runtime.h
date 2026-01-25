#ifndef RUNTIME_H
#define RUNTIME_H

#include "../logic/formula.h"
#include "../logic/proof.h"

#include <map>
#include <set>
#include <string>

namespace logic {

class Runtime;

class ProofContext {
public:
    /// Get a premise step for an axiom/theorem (parses into local db)
    step_id use(const std::string& name);

    /// Get the goal formula_id (in local db)
    formula_id goal() const { return goal_; }

    /// Parse a formula into local db
    formula_id parse(const std::string& formula);

    /// Access local db
    ProofDatabase& db() { return local_db_; }
    const ProofDatabase& db() const { return local_db_; }

    // === Proof Construction (delegates to proof_) ===

    step_id assume(formula_id f);
    step_id premise(formula_id f);  // For external axioms (no discharge needed)

    step_id and_intro(step_id left, step_id right);
    step_id and_elim_l(step_id conj);
    step_id and_elim_r(step_id conj);
    step_id or_intro_l(step_id left, formula_id right);
    step_id or_intro_r(formula_id left, step_id right);
    step_id implies_intro(assumption_id assumption, step_id conclusion);
    step_id implies_elim(step_id impl, step_id antecedent);
    step_id not_intro(assumption_id assumption, step_id bottom);
    step_id not_elim(step_id negation, step_id positive);
    step_id bottom_elim(step_id bottom, formula_id conclusion);
    step_id iff_intro(step_id lr, step_id rl);
    step_id iff_elim_l(step_id iff);
    step_id iff_elim_r(step_id iff);

    step_id forall_intro(step_id body, var_index var);
    step_id forall_elim(step_id forall_step, Term term);
    step_id exists_intro(step_id witness, var_index var, Term term);
    step_id exists_elim(step_id exists, step_id subproof, assumption_id assumption, var_index var);

    /// Get assumption_id from an assumption step
    assumption_id get_assumption_id(step_id s) const;

    /// Get all axioms/theorems used
    const std::set<std::string>& used() const { return used_; }

    /// Complete proof
    bool qed();

private:
    friend class Runtime;
    ProofContext(Runtime& rt, std::string name, const std::string& goal_str);

    Runtime& runtime_;
    std::string name_;
    std::string goal_str_;

    ProofDatabase local_db_;
    formula_id goal_;
    Proof proof_;

    std::set<std::string> used_;
    std::map<std::string, step_id> premise_cache_;
};

class Runtime {
public:
    Runtime() = default;

    /// Register an axiom (must be a sentence)
    void axiom(const std::string& name, const std::string& sentence);

    /// Begin proving a theorem
    ProofContext prove(const std::string& name, const std::string& sentence);

    /// Check if name exists
    bool has(const std::string& name) const;

private:
    friend class ProofContext;

    // Store axioms/theorems as strings (parsed into local db when used)
    std::map<std::string, std::string> known_;
};

}  // namespace logic

#endif  // RUNTIME_H
