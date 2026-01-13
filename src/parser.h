#pragma once

#include "formula.h"

#include <cctype>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace logic {

// ==================== Token Types ====================

enum class TokenType {
    // Identifiers
    Identifier,     // A, B, P, x, y, etc.

    // Connectives
    And,            // & or ∧
    Or,             // | or ∨
    Implies,        // -> or →
    Iff,            // <-> or ↔
    Not,            // ~ or ¬
    Bottom,         // _|_ or ⊥

    // Quantifiers
    Forall,         // forall or ∀
    Exists,         // exists or ∃

    // Punctuation
    LParen,         // (
    RParen,         // )
    Comma,          // ,
    Dot,            // .

    // Special
    Eof,
    Error
};

struct Token {
    TokenType type;
    std::string value;
    size_t pos;
};

// ==================== Lexer ====================

class Lexer {
public:
    explicit Lexer(std::string_view input) : input_(input), pos_(0) {}

    Token next();
    Token peek_token();

private:
    void skip_whitespace();
    char peek(size_t offset = 0) const;

    std::string_view input_;
    size_t pos_;
};

// ==================== Parse Error ====================

class ParseError : public std::runtime_error {
public:
    ParseError(const std::string& msg, size_t pos, const std::string& input)
        : std::runtime_error(format_error(msg, pos, input)), position(pos) {}

    size_t position;

private:
    static std::string format_error(const std::string& msg, size_t pos, const std::string& input);
};

// ==================== Parser ====================

class Parser {
public:
    Parser(std::string_view input, ProofDatabase& db);

    // Parse a complete formula
    formula_id parse();

    // Get variable map (for debugging/display)
    const std::unordered_map<std::string, var_index>& var_map() const { return var_map_; }

private:
    // Operator precedence (lowest to highest):
    // 1. <-> (iff)
    // 2. -> (implies, right associative)
    // 3. | (or)
    // 4. & (and)
    // 5. ~ (not), quantifiers
    // 6. atoms, parentheses

    formula_id parse_iff();
    formula_id parse_implies();
    formula_id parse_or();
    formula_id parse_and();
    formula_id parse_unary();
    formula_id parse_quantifier(bool is_forall);
    formula_id parse_atom();
    formula_id parse_predicate();
    Term parse_term();

    var_index get_or_create_var(const std::string& name);
    predicate_id get_or_create_predicate(const std::string& name, size_t arity);
    constant_id get_or_create_constant(const std::string& name);

    void advance();
    void expect(TokenType type, const std::string& msg);

    Lexer lexer_;
    ProofDatabase& db_;
    std::string_view input_;
    Token current_;

    std::unordered_map<std::string, var_index> var_map_;
    var_index next_var_ = 0;
};

// ==================== Helper Functions ====================

formula_id parse_formula(std::string_view input, ProofDatabase& db);
std::optional<formula_id> try_parse_formula(std::string_view input, ProofDatabase& db,
                                             std::string* error = nullptr);

}  // namespace logic
