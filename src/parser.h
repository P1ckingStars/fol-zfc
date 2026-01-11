#pragma once

#include "formula.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace logic {

class ParseError : public std::runtime_error {
public:
    ParseError(const std::string& msg, size_t pos)
        : std::runtime_error(msg), position(pos) {}
    size_t position;
};

class Parser {
public:
    explicit Parser(std::string_view input);

    // Parse a complete formula
    FormulaPtr parse();

    // Parse with error handling
    static std::optional<FormulaPtr> try_parse(std::string_view input, std::string* error = nullptr);

private:
    std::string_view input_;
    size_t pos_;

    // Tokenization
    char peek() const;
    char advance();
    bool at_end() const;
    void skip_whitespace();
    bool match(char c);
    bool match(std::string_view s);
    void expect(char c);

    // Grammar rules (precedence: iff < implies < or < and < not < atom)
    FormulaPtr parse_iff();
    FormulaPtr parse_implies();
    FormulaPtr parse_or();
    FormulaPtr parse_and();
    FormulaPtr parse_unary();
    FormulaPtr parse_primary();
    FormulaPtr parse_atom();
};

// Convenience function
FormulaPtr parse(std::string_view input);

}  // namespace logic
