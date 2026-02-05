#pragma once

#include <string>
#include <vector>

// Simple AST node for the parser
// This is converted to formula_id by the parser wrapper
struct ASTNode {
    enum Type {
        // Binary operators
        And,
        Or,
        Implies,
        Iff,
        // Unary operator
        Not,
        // Quantifiers
        Forall,
        Exists,
        // Atoms
        Bottom,
        Predicate,
        Term,
        // Statements
        AxiomStmt,
        ClaimStmt,
        // Proof blocks
        ProofBlock,      // proof name: steps
        ProofStepFix,    // fix x
        ProofStepAssume, // h = assume formula
        ProofStepLet,    // h = let formula (create formula handle without assuming)
        ProofStepUse,    // h = use name
        ProofStepRule,   // h = rule args
        ProofStepQed,    // qed h
        // Include
        IncludeStmt      // include "path"
    };

    Type type;
    std::string name;           // For Predicate, Term, quantifier variable, result name
    std::string rule_name;      // For ProofStepRule: the rule being applied
    ASTNode* left = nullptr;    // For binary ops: left operand; For unary: operand
    ASTNode* right = nullptr;   // For binary ops: right operand
    ASTNode* body = nullptr;    // For quantifiers: body formula; For ProofStepAssume: assumed formula
    std::vector<ASTNode*>* args = nullptr;  // For predicates: argument terms; For rules: arguments
    std::vector<ASTNode*>* steps = nullptr; // For ProofBlock: list of proof steps

    // Bottom
    explicit ASTNode(Type t) : type(t) {}

    // Not (unary)
    ASTNode(Type t, ASTNode* operand) : type(t), left(operand) {}

    // Binary operators
    ASTNode(Type t, ASTNode* l, ASTNode* r) : type(t), left(l), right(r) {}

    // Quantifier
    ASTNode(Type t, const std::string& var, ASTNode* b)
        : type(t), name(var), body(b) {}

    // Predicate
    ASTNode(Type t, const std::string& n, std::vector<ASTNode*>* a)
        : type(t), name(n), args(a) {}

    // Term
    ASTNode(Type t, const std::string& n)
        : type(t), name(n) {}

    // Statement (axiom/claim with name and body formula)
    // Note: Uses same constructor signature as Quantifier but for statement types
    static ASTNode* make_statement(Type t, const std::string& stmt_name, ASTNode* formula_body) {
        auto* node = new ASTNode(t);
        node->name = stmt_name;
        node->body = formula_body;
        return node;
    }

    // Proof block: proof claim_name: steps
    static ASTNode* make_proof_block(const std::string& claim_name, std::vector<ASTNode*>* proof_steps) {
        auto* node = new ASTNode(ProofBlock);
        node->name = claim_name;
        node->steps = proof_steps;
        return node;
    }

    // Proof step: fix, assume, use, qed
    // - result_name: the name assigned to this step's result (e.g., "h1")
    // - arg_name: argument identifier (e.g., variable name for fix, axiom name for use)
    // - formula_body: for assume, the formula being assumed
    // - rule_args: for rule steps, the arguments
    static ASTNode* make_proof_step(Type t, const std::string& result_name,
                                     const std::string& arg_name,
                                     ASTNode* formula_body,
                                     std::vector<ASTNode*>* rule_args) {
        auto* node = new ASTNode(t);
        node->name = result_name;
        node->rule_name = arg_name;
        node->body = formula_body;
        node->args = rule_args;
        return node;
    }

    // Rule step: h = rule_name arg1, arg2, ...
    static ASTNode* make_rule_step(const std::string& rule, std::vector<ASTNode*>* rule_args) {
        auto* node = new ASTNode(ProofStepRule);
        node->rule_name = rule;
        node->args = rule_args;
        return node;
    }

    // Include statement: include "path"
    static ASTNode* make_include(const std::string& path) {
        auto* node = new ASTNode(IncludeStmt);
        node->name = path;  // Store path in name field
        return node;
    }

    ~ASTNode() {
        delete left;
        delete right;
        delete body;
        if (args) {
            for (auto* a : *args) delete a;
            delete args;
        }
        if (steps) {
            for (auto* s : *steps) delete s;
            delete steps;
        }
    }

    // Non-copyable
    ASTNode(const ASTNode&) = delete;
    ASTNode& operator=(const ASTNode&) = delete;
};

// Parse context passed to bison
struct ParseContext {
    ASTNode* result = nullptr;                  // For single formula parsing
    std::vector<ASTNode*>* statements = nullptr; // For statement list parsing
    std::string error;
    int error_col = 0;

    ~ParseContext() {
        delete result;
        if (statements) {
            for (auto* s : *statements) delete s;
            delete statements;
        }
    }
};
