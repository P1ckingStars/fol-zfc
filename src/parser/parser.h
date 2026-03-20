#pragma once

#include "../core/formula.h"
#include "formula_ast.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace logic {

// Parse a formula string into a Sentence stored in ctx
// Returns handle to the sentence
// Throws std::runtime_error on parse failure
SentenceHandle parse_sentence(std::string_view input, GlobalContext& ctx);

// Try to parse a formula, returning invalid handle on failure
// If error is non-null, the error message is stored there
SentenceHandle try_parse_sentence(std::string_view input, GlobalContext& ctx,
                                   std::string* error = nullptr);

// Represents a parsed statement (axiom or claim/theorem)
struct ParsedStatement {
    enum class Kind { Axiom, Claim };
    Kind kind;
    std::string name;
    SentenceHandle formula;
    std::string def_predicate;  // Non-empty if this is a @def axiom
};

// A named binding for schema instantiation: var_name -> formula AST
// For predicate bindings, lambda_params holds parameter names (\x y. body).
struct SchemaBinding {
    std::string var_name;
    std::shared_ptr<ASTNode> formula_ast;
    std::vector<std::string> lambda_params;  // non-empty for predicate bindings
};

// Represents a single proof step
struct ParsedProofStep {
    enum class Kind {
        Fix,         // fix x - introduces eigenvariable
        Assume,      // h = assume formula
        Let,         // h = let formula - create formula handle without assuming
        Use,         // h = use axiom_name
        Rule,        // h = rule_name args
        Qed,         // qed h - completes proof
        SchemaInst   // h = schema_inst name { var: formula, ... }
    };
    Kind kind;
    std::string result_name;          // Name of step result (e.g., "h1")
    std::string rule_name;            // Rule name for Rule kind, or axiom/schema name for Use/SchemaInst
    std::vector<std::string> args;    // Arguments (step names or term identifiers)
    SentenceHandle formula;           // For Assume/Let kind (when no free vars)
    std::shared_ptr<ASTNode> formula_ast;  // For Assume/Let kind (deferred parsing)
    std::vector<SchemaBinding> schema_bindings;  // For SchemaInst kind
};

// Parse a formula with external variables (for proof steps)
FormulaHandle parse_formula_with_vars(
    const ASTNode* ast,
    GlobalContext& ctx,
    FormulaBuilder& builder,
    const std::unordered_map<std::string, Term>& external_vars,
    const std::unordered_map<std::string, size_t>& schema_vars = {});

// Represents a complete proof block
struct ParsedProof {
    std::string claim_name;
    std::vector<ParsedProofStep> steps;
    bool unproved = false;  // true if declared as "proof name: UNPROVED"
};

// Represents an include directive
struct ParsedInclude {
    std::string path;
};

// Result of parsing a file with statements and proofs
struct ParseResult {
    std::vector<ParsedStatement> statements;
    std::vector<ParsedProof> proofs;
    std::vector<ParsedInclude> includes;
};

// Parse multiple statements from input
// Axioms are automatically added to ctx's known set
// Returns list of statements in order
// Throws std::runtime_error on parse failure
std::vector<ParsedStatement> parse_statements(std::string_view input, GlobalContext& ctx);

// Try to parse statements, returning empty vector on failure
// If error is non-null, the error message is stored there
std::vector<ParsedStatement> try_parse_statements(std::string_view input, GlobalContext& ctx,
                                                   std::string* error = nullptr);

// Parse statements and proofs from input
// Returns both statements and proof blocks
// Throws std::runtime_error on parse failure
ParseResult parse_with_proofs(std::string_view input, GlobalContext& ctx);

// Try to parse statements and proofs, returning empty result on failure
ParseResult try_parse_with_proofs(std::string_view input, GlobalContext& ctx,
                                   std::string* error = nullptr);

}  // namespace logic
