#include "syntax_parser.h"

#include <algorithm>

namespace metamath {

SyntaxParser::SyntaxParser(const MmDatabase& db) {
    // Collect ALL variable types from every assertion's floating hyps
    for (const auto& label : db.assertion_order()) {
        const Assertion* a = db.get_assertion(label);
        if (!a) continue;
        for (size_t i = 0; i < a->frame.hyp_labels.size(); ++i) {
            if (!a->frame.is_floating[i]) continue;
            const FloatingHyp* fh = db.get_float_hyp(a->frame.hyp_labels[i]);
            if (fh) var_types_[fh->variable] = fh->typecode;
        }
    }

    // Build grammar rules from syntax axioms
    for (const auto& label : db.assertion_order()) {
        const Assertion* a = db.get_assertion(label);
        if (!a) continue;

        // Syntax axiom: typecode is NOT "|-", and must be an axiom ($a)
        // not a syntax theorem ($p) — syntax theorems are derived shortcuts
        // that would create ambiguous/redundant grammar rules.
        if (a->expression.empty() || a->expression[0] == "|-") continue;
        if (a->kind != Assertion::Kind::Axiom) continue;

        // Variable types for this axiom's pattern
        std::unordered_map<std::string, std::string> local_var_types;
        for (size_t i = 0; i < a->frame.hyp_labels.size(); ++i) {
            if (!a->frame.is_floating[i]) continue;
            const FloatingHyp* fh = db.get_float_hyp(a->frame.hyp_labels[i]);
            if (fh) local_var_types[fh->variable] = fh->typecode;
        }

        GrammarRule rule;
        rule.label = label;
        rule.typecode = a->expression[0];

        // Build pattern from expression tokens (skip typecode at [0])
        for (size_t i = 1; i < a->expression.size(); ++i) {
            const std::string& tok = a->expression[i];
            auto it = local_var_types.find(tok);
            if (it != local_var_types.end()) {
                rule.pattern.push_back({true, it->second, tok});
            } else {
                rule.pattern.push_back({false, tok, ""});
            }
        }

        rules_[rule.typecode].push_back(std::move(rule));
    }

    // Sort rules: longer patterns first (more specific = tried first)
    // This reduces backtracking by trying the most constrained rules first
    for (auto& [tc, rule_vec] : rules_) {
        std::stable_sort(rule_vec.begin(), rule_vec.end(),
            [](const GrammarRule& a, const GrammarRule& b) {
                // Count literal tokens (more literals = more constrained)
                auto count_literals = [](const GrammarRule& r) {
                    int n = 0;
                    for (const auto& e : r.pattern)
                        if (!e.is_variable) ++n;
                    return n;
                };
                return count_literals(a) > count_literals(b);
            });
    }
}

std::optional<SyntaxNode> SyntaxParser::parse(const Expression& expr) const {
    if (expr.empty()) return std::nullopt;
    Cache cache;
    auto result = parse_impl(expr, 1, expr[0], cache);
    if (!result) return std::nullopt;
    if (result->second != expr.size()) return std::nullopt;
    return std::move(result->first);
}

std::optional<std::pair<SyntaxNode, size_t>> SyntaxParser::parse_at(
    const Expression& expr, size_t start,
    const std::string& typecode) const {
    Cache cache;
    return parse_impl(expr, start, typecode, cache);
}

std::optional<std::pair<SyntaxNode, size_t>> SyntaxParser::parse_impl(
    const Expression& expr, size_t start,
    const std::string& typecode, Cache& cache) const {
    if (start >= expr.size()) return std::nullopt;

    // Check memoization cache
    CacheKey key{start, typecode};
    auto cit = cache.find(key);
    if (cit != cache.end()) return cit->second;

    // Will store result before returning
    auto& cached = cache[key];  // insert empty entry (marks as in-progress)

    // Base case: single token that is a variable of the right type
    {
        auto it = var_types_.find(expr[start]);
        if (it != var_types_.end() && it->second == typecode) {
            SyntaxNode node;
            node.label = "$f";
            node.typecode = typecode;
            node.token = expr[start];
            cached = std::make_pair(std::move(node), start + 1);
            return cached;
        }
    }

    // Try each grammar rule for this typecode
    auto rit = rules_.find(typecode);
    if (rit == rules_.end()) {
        cached = std::nullopt;
        return std::nullopt;
    }

    for (const auto& rule : rit->second) {
        // Quick check: if the first pattern element is a literal,
        // verify it matches before doing recursive work
        if (!rule.pattern.empty() && !rule.pattern[0].is_variable) {
            if (expr[start] != rule.pattern[0].value) continue;
        }

        std::vector<SyntaxNode> children;
        size_t end_pos = 0;
        if (try_rule(expr, start, rule, children, end_pos, cache)) {
            SyntaxNode node;
            node.label = rule.label;
            node.typecode = typecode;
            node.children = std::move(children);
            cached = std::make_pair(std::move(node), end_pos);
            return cached;
        }
    }

    cached = std::nullopt;
    return std::nullopt;
}

bool SyntaxParser::try_rule(
    const Expression& expr, size_t pos,
    const GrammarRule& rule,
    std::vector<SyntaxNode>& children,
    size_t& end_pos, Cache& cache) const {

    children.clear();
    size_t cur = pos;

    for (const auto& elem : rule.pattern) {
        if (cur >= expr.size()) return false;

        if (!elem.is_variable) {
            if (expr[cur] != elem.value) return false;
            ++cur;
        } else {
            auto sub = parse_impl(expr, cur, elem.value, cache);
            if (!sub) return false;
            children.push_back(std::move(sub->first));
            cur = sub->second;
        }
    }

    end_pos = cur;
    return true;
}

}  // namespace metamath
