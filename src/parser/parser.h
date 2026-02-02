#pragma once

#include "../core/formula.h"

#include <optional>
#include <string>
#include <string_view>
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

}  // namespace logic
