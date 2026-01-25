#pragma once

#include "../core/formula.h"

#include <optional>
#include <string>
#include <string_view>

namespace logic {

// Parse a formula string into a Sentence stored in ctx
// Returns handle to the sentence
// Throws std::runtime_error on parse failure
SentenceHandle parse_sentence(std::string_view input, GlobalContext& ctx);

// Try to parse a formula, returning invalid handle on failure
// If error is non-null, the error message is stored there
SentenceHandle try_parse_sentence(std::string_view input, GlobalContext& ctx,
                                   std::string* error = nullptr);

}  // namespace logic
