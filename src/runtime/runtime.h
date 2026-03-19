#pragma once

#include "../core/formula.h"
#include "../core/proof.h"
#include "../parser/parser.h"
#include "../util/error.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace logic {

class ProofContext;

// Runtime manages axioms/theorems and creates proof contexts
class Runtime {
    GlobalContext ctx_;
    std::unordered_map<std::string, std::unordered_set<std::string>> proof_deps_;

public:
    Runtime() = default;

    // Load axioms and claims from a string (uses parse_statements)
    util::Result<std::vector<ParsedStatement>> load(std::string_view input);

    // Load axioms and claims from a file
    util::Result<std::vector<ParsedStatement>> load_file(const std::string& path);

    // Load axioms, claims, and proofs from a string
    util::Result<ParseResult> load_with_proofs(std::string_view input);

    // Load axioms, claims, and proofs from a file
    util::Result<ParseResult> load_file_with_proofs(const std::string& path);

    // Execute a parsed proof and verify it
    util::ResultStatus execute_proof(const ParsedProof& proof);

    // Execute all proofs from a ParseResult
    util::ResultStatus execute_all_proofs(const ParseResult& result);

    // Load file with includes, processing all includes recursively
    // Detects cycles and reports errors
    util::Result<ParseResult> load_file_recursive(const std::string& path);

    // Get the global context (for direct access to axioms/theorems)
    GlobalContext& context() { return ctx_; }

private:
    // Helper for recursive loading with cycle detection
    util::Result<ParseResult> load_file_impl(
        const std::string& path,
        const std::string& base_dir,
        std::unordered_set<std::string>& loaded);

public:
    const GlobalContext& context() const { return ctx_; }

    // Proof dependency graph: proof name -> set of axiom/theorem names used
    const std::unordered_map<std::string, std::unordered_set<std::string>>& proof_deps() const {
        return proof_deps_;
    }

    // Create a proof context for proving a claim
    ProofContext prove(const std::string& claim_name);
    ProofContext prove(SentenceHandle goal);
};

// ProofContext wraps ClassicalProofStack for proving a specific claim
class ProofContext {
    Runtime& runtime_;
    ClassicalProofStack stack_;
    FormulaHandle goal_formula_;     // for qed comparison (always set for schemas)
    SentenceHandle goal_sentence_;   // for add_theorem (regular proofs only)
    std::string name_;
    bool completed_ = false;
    bool is_schema_ = false;
    std::unordered_set<std::string> used_names_;

public:
    ProofContext(Runtime& rt, const std::string& name, SentenceHandle goal);

    // Factory for schema proofs
    static ProofContext for_schema(Runtime& rt, const std::string& name,
                                   const SchemaDefinition& schema);

    // Access formula builder for creating formulas
    FormulaBuilder& builder() { return stack_.builder(); }

    // Parse a formula string in this context
    FormulaHandle parse(std::string_view input);

    // Use an axiom or theorem by name
    FormulaResult use(const std::string& name);

    // ========== Scope Management ==========
    Term fix_var();
    FormulaHandle assume(FormulaHandle const& formula);
    void pop();

    // ========== Conjunction (And) ==========
    FormulaResult and_intro(FormulaHandle const& a, FormulaHandle const& b);
    FormulaResult and_elim_l(FormulaHandle const& and_formula);
    FormulaResult and_elim_r(FormulaHandle const& and_formula);

    // ========== Disjunction (Or) ==========
    FormulaResult or_intro_l(FormulaHandle const& a, FormulaHandle const& b);
    FormulaResult or_intro_r(FormulaHandle const& a, FormulaHandle const& b);
    FormulaResult or_elim(FormulaHandle const& or_f, FormulaHandle const& a_impl_c, FormulaHandle const& b_impl_c);

    // ========== Implication ==========
    FormulaResult implies_intro(FormulaHandle const& conclusion);
    FormulaResult implies_elim(FormulaHandle const& implication, FormulaHandle const& antecedent);

    // ========== Negation ==========
    FormulaResult not_intro(FormulaHandle const& bottom);
    FormulaResult not_elim(FormulaHandle const& negation, FormulaHandle const& formula);

    // ========== Bottom (Falsum) ==========
    FormulaResult bottom_elim(FormulaHandle const& bottom, FormulaHandle const& formula);

    // ========== Biconditional (Iff) ==========
    FormulaResult iff_intro(FormulaHandle const& a_impl_b, FormulaHandle const& b_impl_a);
    FormulaResult iff_elim_l(FormulaHandle const& iff_f, FormulaHandle const& a);
    FormulaResult iff_elim_r(FormulaHandle const& iff_f, FormulaHandle const& b);

    // ========== Quantifiers ==========
    FormulaResult forall_intro(FormulaHandle const& body);
    FormulaResult forall_elim(FormulaHandle const& formula, Term const& var);
    FormulaResult exists_intro(FormulaHandle const& body, std::optional<Term> witness = std::nullopt);
    FormulaResult exists_elim(FormulaHandle const& formula);
    std::optional<Term> last_witness() const { return stack_.last_witness_var(); }

    // ========== Equality ==========
    FormulaResult eq_subst(FormulaHandle const& eq_formula, FormulaHandle const& target);

    // ========== Schema Instantiation ==========
    FormulaResult schema_inst(const SchemaDefinition& schema,
                              const std::vector<FormulaHandle>& bindings);

    // ========== Classical Extensions ==========
    FormulaResult double_neg_elim(FormulaHandle const& double_neg);
    FormulaResult excluded_middle(FormulaHandle const& formula);
    FormulaResult classical_absurd(FormulaHandle const& bottom);
    FormulaResult peirce(FormulaHandle const& a, FormulaHandle const& b);

    // ========== Definite Descriptions ==========
    FormulaResult iota_elim(FormulaHandle const& exists_formula);
    std::optional<Term> last_iota_term() const { return stack_.last_iota_term(); }

    // ========== Proof Completion ==========
    // Complete the proof - verifies goal is derived and registers theorem
    util::ResultStatus qed(FormulaHandle const& derived);

    // Get proof goal
    SentenceHandle goal() const { return goal_sentence_; }
    const std::string& name() const { return name_; }
    bool is_completed() const { return completed_; }
    const std::unordered_set<std::string>& used() const { return used_names_; }
};

}  // namespace logic
