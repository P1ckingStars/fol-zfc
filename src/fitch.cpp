#include "fitch.h"

#include <algorithm>
#include <iomanip>
#include <map>
#include <sstream>

namespace logic {

std::string FitchPrinter::make_key(const Formula& f, Rule r) {
    return to_string(f) + ":" + std::to_string(static_cast<int>(r));
}

int FitchPrinter::process(Context& ctx, const Proof& proof) {
    // Check if we've already processed this exact formula
    auto key = make_key(*proof.conclusion, proof.rule);
    if (auto it = ctx.formula_to_line.find(key); it != ctx.formula_to_line.end()) {
        return it->second;
    }

    std::vector<int> justification;

    if (proof.rule == Rule::ImpliesIntro || proof.rule == Rule::NotIntro) {
        // Open a new scope and add the assumption
        ctx.scope_depth++;

        if (proof.discharged) {
            FitchLine assumption_line;
            assumption_line.line_number = ctx.next_line++;
            assumption_line.scope_depth = ctx.scope_depth;
            assumption_line.formula = *proof.discharged;
            assumption_line.rule = Rule::Assumption;
            assumption_line.is_assumption = true;
            ctx.lines.push_back(assumption_line);
            justification.push_back(assumption_line.line_number);
        }

        // Process the subproof
        for (const auto& premise : proof.premises) {
            int line = process(ctx, *premise);
            justification.push_back(line);
        }

        ctx.scope_depth--;

    } else if (proof.rule == Rule::OrElim) {
        // Or elimination: first premise is the disjunction
        // Next two premises are the case proofs (as implications)
        int disj_line = process(ctx, *proof.premises[0]);
        justification.push_back(disj_line);

        // Left case: A → C
        const auto& left_case = proof.premises[1];
        ctx.scope_depth++;
        if (left_case->discharged) {
            FitchLine left_assumption;
            left_assumption.line_number = ctx.next_line++;
            left_assumption.scope_depth = ctx.scope_depth;
            left_assumption.formula = *left_case->discharged;
            left_assumption.rule = Rule::Assumption;
            left_assumption.is_assumption = true;
            ctx.lines.push_back(left_assumption);
        }
        for (const auto& p : left_case->premises) {
            process(ctx, *p);
        }
        // Add the conclusion of the left case
        if (!left_case->premises.empty()) {
            justification.push_back(ctx.lines.back().line_number);
        }
        ctx.scope_depth--;

        // Right case: B → C
        const auto& right_case = proof.premises[2];
        ctx.scope_depth++;
        if (right_case->discharged) {
            FitchLine right_assumption;
            right_assumption.line_number = ctx.next_line++;
            right_assumption.scope_depth = ctx.scope_depth;
            right_assumption.formula = *right_case->discharged;
            right_assumption.rule = Rule::Assumption;
            right_assumption.is_assumption = true;
            ctx.lines.push_back(right_assumption);
        }
        for (const auto& p : right_case->premises) {
            process(ctx, *p);
        }
        if (!right_case->premises.empty()) {
            justification.push_back(ctx.lines.back().line_number);
        }
        ctx.scope_depth--;

    } else {
        // Normal rule: process all premises first
        for (const auto& premise : proof.premises) {
            int line = process(ctx, *premise);
            justification.push_back(line);
        }
    }

    // Add current line
    FitchLine line;
    line.line_number = ctx.next_line++;
    line.scope_depth = ctx.scope_depth;
    line.formula = proof.conclusion;
    line.rule = proof.rule;
    line.justification = justification;
    line.is_assumption = (proof.rule == Rule::Assumption);
    ctx.lines.push_back(line);
    ctx.formula_to_line[key] = line.line_number;

    return line.line_number;
}

std::vector<FitchLine> FitchPrinter::linearize(const Proof& proof) {
    Context ctx;
    process(ctx, proof);
    return ctx.lines;
}

std::string FitchPrinter::format(const std::vector<FitchLine>& lines) {
    if (lines.empty()) return "";

    // Find maximum widths for alignment
    int max_line_num = lines.back().line_number;
    int line_num_width = std::to_string(max_line_num).length();

    size_t max_formula_width = 0;
    int max_scope = 0;
    for (const auto& line : lines) {
        max_formula_width = std::max(max_formula_width, to_string(*line.formula).length());
        max_scope = std::max(max_scope, line.scope_depth);
    }

    std::ostringstream ss;

    for (const auto& line : lines) {
        // Line number
        ss << std::setw(line_num_width) << line.line_number << ". ";

        // Scope bars
        for (int i = 0; i < line.scope_depth; i++) {
            ss << "│ ";
        }

        // Assumption marker
        if (line.is_assumption && line.rule == Rule::Assumption) {
            ss << "┌ ";
        } else if (line.scope_depth > 0) {
            ss << "  ";
        }

        // Formula
        std::string formula_str = to_string(*line.formula);
        ss << formula_str;

        // Padding to align justification
        size_t current_len = formula_str.length();
        size_t scope_extra = line.scope_depth * 2 + (line.scope_depth > 0 ? 2 : 0);
        size_t padding = max_formula_width + max_scope * 2 + 4 - current_len - scope_extra;
        ss << std::string(padding, ' ');

        // Justification
        ss << rule_name(line.rule);
        if (!line.justification.empty()) {
            ss << " ";
            for (size_t i = 0; i < line.justification.size(); i++) {
                if (i > 0) ss << ", ";
                ss << line.justification[i];
            }
        }

        ss << "\n";
    }

    return ss.str();
}

std::string FitchPrinter::print(const Proof& proof) {
    auto lines = linearize(proof);
    return format(lines);
}

}  // namespace logic
