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
        ClaimStmt
    };

    Type type;
    std::string name;           // For Predicate, Term, quantifier variable
    ASTNode* left = nullptr;    // For binary ops: left operand; For unary: operand
    ASTNode* right = nullptr;   // For binary ops: right operand
    ASTNode* body = nullptr;    // For quantifiers: body formula
    std::vector<ASTNode*>* args = nullptr;  // For predicates: argument terms

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

    ~ASTNode() {
        delete left;
        delete right;
        delete body;
        if (args) {
            for (auto* a : *args) delete a;
            delete args;
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
