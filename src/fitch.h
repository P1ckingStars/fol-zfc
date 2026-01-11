#pragma once

#include "rule_engine.h"

#include <map>
#include <string>
#include <vector>

namespace logic {

// Fitch-style proof line
struct FitchLine {
    int line_number;
    int scope_depth;
    FormulaPtr formula;
    Rule rule;
    std::vector<int> justification;  // Line numbers used
    bool is_assumption;
};

// Convert a proof tree to Fitch-style linear proof
class FitchPrinter {
public:
    // Convert proof tree to linear Fitch format
    std::vector<FitchLine> linearize(const Proof& proof);

    // Format as string
    std::string format(const std::vector<FitchLine>& lines);

    // Convenience: proof tree to formatted string
    std::string print(const Proof& proof);

private:
    struct Context {
        std::vector<FitchLine> lines;
        int next_line = 1;
        int scope_depth = 0;

        // Map from formula+rule to line number (for deduplication)
        std::map<std::string, int> formula_to_line;
    };

    int process(Context& ctx, const Proof& proof);
    std::string make_key(const Formula& f, Rule r);
};

}  // namespace logic
