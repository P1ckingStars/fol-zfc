#include "parser.h"

namespace logic {

// ==================== Lexer ====================

Token Lexer::next() {
    skip_whitespace();

    if (pos_ >= input_.size()) {
        return {TokenType::Eof, "", pos_};
    }

    size_t start = pos_;
    char c = input_[pos_];

    // Single character tokens
    if (c == '(') { pos_++; return {TokenType::LParen, "(", start}; }
    if (c == ')') { pos_++; return {TokenType::RParen, ")", start}; }
    if (c == ',') { pos_++; return {TokenType::Comma, ",", start}; }
    if (c == '.') { pos_++; return {TokenType::Dot, ".", start}; }
    if (c == '&') { pos_++; return {TokenType::And, "&", start}; }
    if (c == '|') { pos_++; return {TokenType::Or, "|", start}; }
    if (c == '~') { pos_++; return {TokenType::Not, "~", start}; }

    // Multi-character operators
    if (c == '-' && peek(1) == '>') {
        pos_ += 2;
        return {TokenType::Implies, "->", start};
    }
    if (c == '<' && peek(1) == '-' && peek(2) == '>') {
        pos_ += 3;
        return {TokenType::Iff, "<->", start};
    }
    if (c == '_' && peek(1) == '|' && peek(2) == '_') {
        pos_ += 3;
        return {TokenType::Bottom, "_|_", start};
    }

    // Identifiers and keywords
    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
        std::string ident;
        while (pos_ < input_.size() &&
               (std::isalnum(static_cast<unsigned char>(input_[pos_])) || input_[pos_] == '_')) {
            ident += input_[pos_++];
        }

        // Keywords
        if (ident == "forall" || ident == "Forall" || ident == "FORALL" || ident == "all") {
            return {TokenType::Forall, ident, start};
        }
        if (ident == "exists" || ident == "Exists" || ident == "EXISTS" || ident == "ex") {
            return {TokenType::Exists, ident, start};
        }
        if (ident == "and" || ident == "AND") {
            return {TokenType::And, ident, start};
        }
        if (ident == "or" || ident == "OR") {
            return {TokenType::Or, ident, start};
        }
        if (ident == "not" || ident == "NOT") {
            return {TokenType::Not, ident, start};
        }
        if (ident == "implies" || ident == "IMPLIES") {
            return {TokenType::Implies, ident, start};
        }
        if (ident == "iff" || ident == "IFF") {
            return {TokenType::Iff, ident, start};
        }
        if (ident == "false" || ident == "FALSE" || ident == "bottom" || ident == "BOTTOM") {
            return {TokenType::Bottom, ident, start};
        }

        return {TokenType::Identifier, ident, start};
    }

    pos_++;
    return {TokenType::Error, std::string(1, c), start};
}

Token Lexer::peek_token() {
    size_t saved = pos_;
    Token t = next();
    pos_ = saved;
    return t;
}

void Lexer::skip_whitespace() {
    while (pos_ < input_.size() && std::isspace(static_cast<unsigned char>(input_[pos_]))) {
        pos_++;
    }
}

char Lexer::peek(size_t offset) const {
    if (pos_ + offset >= input_.size()) return '\0';
    return input_[pos_ + offset];
}

// ==================== ParseError ====================

std::string ParseError::format_error(const std::string& msg, size_t pos, const std::string& input) {
    std::ostringstream oss;
    oss << "Parse error at position " << pos << ": " << msg;
    oss << "\n  Input: " << input;
    oss << "\n         " << std::string(pos, ' ') << "^";
    return oss.str();
}

// ==================== Parser ====================

Parser::Parser(std::string_view input, ProofDatabase& db)
    : lexer_(input), db_(db), input_(input) {
    advance();
}

formula_id Parser::parse() {
    formula_id result = parse_iff();
    if (current_.type != TokenType::Eof) {
        throw ParseError("Unexpected token after formula", current_.pos, std::string(input_));
    }
    return result;
}

formula_id Parser::parse_iff() {
    formula_id left = parse_implies();

    while (current_.type == TokenType::Iff) {
        advance();
        formula_id right = parse_implies();
        left = db_.create_iff(left, right);
    }

    return left;
}

formula_id Parser::parse_implies() {
    formula_id left = parse_or();

    if (current_.type == TokenType::Implies) {
        advance();
        formula_id right = parse_implies();  // Right associative
        return db_.create_implies(left, right);
    }

    return left;
}

formula_id Parser::parse_or() {
    formula_id left = parse_and();

    while (current_.type == TokenType::Or) {
        advance();
        formula_id right = parse_and();
        left = db_.create_or(left, right);
    }

    return left;
}

formula_id Parser::parse_and() {
    formula_id left = parse_unary();

    while (current_.type == TokenType::And) {
        advance();
        formula_id right = parse_unary();
        left = db_.create_and(left, right);
    }

    return left;
}

formula_id Parser::parse_unary() {
    if (current_.type == TokenType::Not) {
        advance();
        formula_id operand = parse_unary();
        return db_.create_not(operand);
    }

    if (current_.type == TokenType::Forall) {
        advance();
        return parse_quantifier(true);
    }

    if (current_.type == TokenType::Exists) {
        advance();
        return parse_quantifier(false);
    }

    return parse_atom();
}

formula_id Parser::parse_quantifier(bool is_forall) {
    if (current_.type != TokenType::Identifier) {
        throw ParseError("Expected variable name after quantifier", current_.pos, std::string(input_));
    }

    std::string var_name = current_.value;
    var_index var = get_or_create_var(var_name);
    advance();

    if (current_.type != TokenType::Dot) {
        throw ParseError("Expected '.' after quantifier variable", current_.pos, std::string(input_));
    }
    advance();

    formula_id body = parse_iff();  // Parse full formula as body

    if (is_forall) {
        return db_.create_forall(var, body);
    } else {
        return db_.create_exists(var, body);
    }
}

formula_id Parser::parse_atom() {
    if (current_.type == TokenType::Bottom) {
        advance();
        return db_.create_bottom();
    }

    if (current_.type == TokenType::LParen) {
        advance();
        formula_id inner = parse_iff();
        expect(TokenType::RParen, "Expected ')'");
        return inner;
    }

    if (current_.type == TokenType::Identifier) {
        return parse_predicate();
    }

    throw ParseError("Expected formula", current_.pos, std::string(input_));
}

formula_id Parser::parse_predicate() {
    std::string name = current_.value;
    advance();

    std::vector<Term> args;

    if (current_.type == TokenType::LParen) {
        advance();

        if (current_.type != TokenType::RParen) {
            args.push_back(parse_term());

            while (current_.type == TokenType::Comma) {
                advance();
                args.push_back(parse_term());
            }
        }

        expect(TokenType::RParen, "Expected ')' after predicate arguments");
    }

    // Get or create predicate
    predicate_id pred = get_or_create_predicate(name, args.size());

    return db_.create_predicate_instance(pred, std::move(args));
}

Term Parser::parse_term() {
    if (current_.type != TokenType::Identifier) {
        throw ParseError("Expected term (variable or constant)", current_.pos, std::string(input_));
    }

    std::string name = current_.value;
    advance();

    // Heuristic: single lowercase letter = variable, otherwise constant
    if (name.size() == 1 && std::islower(static_cast<unsigned char>(name[0]))) {
        return Term::var(get_or_create_var(name));
    } else {
        return Term::constant(get_or_create_constant(name));
    }
}

var_index Parser::get_or_create_var(const std::string& name) {
    auto it = var_map_.find(name);
    if (it != var_map_.end()) {
        return it->second;
    }
    var_index idx = next_var_++;
    var_map_[name] = idx;
    return idx;
}

predicate_id Parser::get_or_create_predicate(const std::string& name, size_t arity) {
    auto existing = db_.find_predicate(name);
    if (existing) {
        return *existing;
    }
    return db_.create_predicate(name, arity);
}

constant_id Parser::get_or_create_constant(const std::string& name) {
    auto existing = db_.find_constant(name);
    if (existing) {
        return *existing;
    }
    return db_.create_constant(name);
}

void Parser::advance() {
    current_ = lexer_.next();
}

void Parser::expect(TokenType type, const std::string& msg) {
    if (current_.type != type) {
        throw ParseError(msg, current_.pos, std::string(input_));
    }
    advance();
}

// ==================== Helper Functions ====================

formula_id parse_formula(std::string_view input, ProofDatabase& db) {
    Parser parser(input, db);
    return parser.parse();
}

std::optional<formula_id> try_parse_formula(std::string_view input, ProofDatabase& db,
                                             std::string* error) {
    try {
        return parse_formula(input, db);
    } catch (const ParseError& e) {
        if (error) *error = e.what();
        return std::nullopt;
    }
}

}  // namespace logic
