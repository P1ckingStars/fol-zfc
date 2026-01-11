#include "parser.h"

#include <cctype>

namespace logic {

Parser::Parser(std::string_view input) : input_(input), pos_(0) {}

char Parser::peek() const {
    if (at_end()) return '\0';
    return input_[pos_];
}

char Parser::advance() {
    if (at_end()) return '\0';
    return input_[pos_++];
}

bool Parser::at_end() const {
    return pos_ >= input_.size();
}

void Parser::skip_whitespace() {
    while (!at_end() && std::isspace(peek())) {
        advance();
    }
}

bool Parser::match(char c) {
    skip_whitespace();
    if (peek() == c) {
        advance();
        return true;
    }
    return false;
}

bool Parser::match(std::string_view s) {
    skip_whitespace();
    if (pos_ + s.size() <= input_.size() &&
        input_.substr(pos_, s.size()) == s) {
        // Make sure it's not a prefix of a longer identifier
        size_t end = pos_ + s.size();
        if (end < input_.size() && std::isalnum(input_[end])) {
            return false;
        }
        pos_ += s.size();
        return true;
    }
    return false;
}

void Parser::expect(char c) {
    skip_whitespace();
    if (peek() != c) {
        throw ParseError(std::string("Expected '") + c + "'", pos_);
    }
    advance();
}

FormulaPtr Parser::parse() {
    auto result = parse_iff();
    skip_whitespace();
    if (!at_end()) {
        throw ParseError("Unexpected characters after formula", pos_);
    }
    return result;
}

std::optional<FormulaPtr> Parser::try_parse(std::string_view input, std::string* error) {
    try {
        Parser p(input);
        return p.parse();
    } catch (const ParseError& e) {
        if (error) {
            *error = e.what();
        }
        return std::nullopt;
    }
}

// iff := implies (('<->' | '↔') implies)*
FormulaPtr Parser::parse_iff() {
    auto left = parse_implies();

    while (true) {
        skip_whitespace();
        // Check for ↔ (UTF-8: E2 86 94)
        if (pos_ + 2 < input_.size() &&
            (unsigned char)input_[pos_] == 0xE2 &&
            (unsigned char)input_[pos_+1] == 0x86 &&
            (unsigned char)input_[pos_+2] == 0x94) {
            pos_ += 3;
            auto right = parse_implies();
            left = iff(left, right);
        } else if (match("<->") || match("iff")) {
            auto right = parse_implies();
            left = iff(left, right);
        } else {
            break;
        }
    }

    return left;
}

// implies := or (('->' | '→') or)*  (right associative)
FormulaPtr Parser::parse_implies() {
    auto left = parse_or();

    skip_whitespace();
    // Check for → (UTF-8: E2 86 92)
    bool found_arrow = false;
    if (pos_ + 2 < input_.size() &&
        (unsigned char)input_[pos_] == 0xE2 &&
        (unsigned char)input_[pos_+1] == 0x86 &&
        (unsigned char)input_[pos_+2] == 0x92) {
        pos_ += 3;
        found_arrow = true;
    } else if (match("->") || match("implies")) {
        found_arrow = true;
    }

    if (found_arrow) {
        auto right = parse_implies();  // Right associative
        return impl(left, right);
    }

    return left;
}

// or := and (('|' | '∨' | 'or') and)*
FormulaPtr Parser::parse_or() {
    auto left = parse_and();

    while (true) {
        skip_whitespace();
        // Check for ∨ (UTF-8: E2 88 A8)
        if (pos_ + 2 < input_.size() &&
            (unsigned char)input_[pos_] == 0xE2 &&
            (unsigned char)input_[pos_+1] == 0x88 &&
            (unsigned char)input_[pos_+2] == 0xA8) {
            pos_ += 3;
            auto right = parse_and();
            left = disj(left, right);
        } else if (match("|") || match("or") || match("\\/")) {
            auto right = parse_and();
            left = disj(left, right);
        } else {
            break;
        }
    }

    return left;
}

// and := unary (('&' | '∧' | 'and') unary)*
FormulaPtr Parser::parse_and() {
    auto left = parse_unary();

    while (true) {
        skip_whitespace();
        // Check for ∧ (UTF-8: E2 88 A7)
        if (pos_ + 2 < input_.size() &&
            (unsigned char)input_[pos_] == 0xE2 &&
            (unsigned char)input_[pos_+1] == 0x88 &&
            (unsigned char)input_[pos_+2] == 0xA7) {
            pos_ += 3;
            auto right = parse_unary();
            left = conj(left, right);
        } else if (match("&") || match("and") || match("/\\")) {
            auto right = parse_unary();
            left = conj(left, right);
        } else {
            break;
        }
    }

    return left;
}

// unary := ('~' | '¬' | 'not') unary | primary
FormulaPtr Parser::parse_unary() {
    skip_whitespace();

    // Check for ¬ (UTF-8: C2 AC)
    if (pos_ + 1 < input_.size() &&
        (unsigned char)input_[pos_] == 0xC2 &&
        (unsigned char)input_[pos_+1] == 0xAC) {
        pos_ += 2;
        return neg(parse_unary());
    }

    if (match('~') || match('!')) {
        return neg(parse_unary());
    }

    if (match("not")) {
        return neg(parse_unary());
    }

    return parse_primary();
}

// primary := atom | '(' formula ')' | '⊥' | 'false'
FormulaPtr Parser::parse_primary() {
    skip_whitespace();

    // Check for ⊥ (UTF-8: E2 8A A5)
    if (pos_ + 2 < input_.size() &&
        (unsigned char)input_[pos_] == 0xE2 &&
        (unsigned char)input_[pos_+1] == 0x8A &&
        (unsigned char)input_[pos_+2] == 0xA5) {
        pos_ += 3;
        return bottom();
    }

    if (match("false") || match("_|_")) {
        return bottom();
    }

    if (match('(')) {
        auto inner = parse_iff();
        expect(')');
        return inner;
    }

    return parse_atom();
}

// atom := [A-Za-z][A-Za-z0-9_]*
FormulaPtr Parser::parse_atom() {
    skip_whitespace();

    if (at_end() || !std::isalpha(peek())) {
        throw ParseError("Expected atom (identifier)", pos_);
    }

    size_t start = pos_;
    while (!at_end() && (std::isalnum(peek()) || peek() == '_')) {
        advance();
    }

    std::string name(input_.substr(start, pos_ - start));
    return atom(std::move(name));
}

FormulaPtr parse(std::string_view input) {
    Parser p(input);
    return p.parse();
}

}  // namespace logic
