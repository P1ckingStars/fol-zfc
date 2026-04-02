#pragma once

#include "../parser/mm_database.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace metamath {

// ---------------------------------------------------------------------------
// Syntax-axiom-driven parser for Metamath expressions
// ---------------------------------------------------------------------------
//
// PATTERN: Metamath's syntax is not a fixed grammar. Instead, syntax axioms
// ($a statements where the typecode is NOT "|-") define the grammar rules
// dynamically from the database (set.mm). Each syntax axiom becomes a
// production rule:
//
//   LHS (typecode) → pattern of literals and typed sub-expressions
//
// Examples from set.mm:
//   wi  $a wff ( ph -> ps ) $.     →  wff  → "(" wff "->" wff ")"
//   wn  $a wff -. ph $.            →  wff  → "-." wff
//   wceq $a wff A = B $.           →  wff  → class "=" class
//   cin  $a class ( A i^i B ) $.   →  class → "(" class "i^i" class ")"
//   cv   $a class x $.             →  class → setvar
//   cab  $a class { x | ph } $.    →  class → "{" setvar "|" wff "}"
//
// Floating hypotheses ($f) define base cases:
//   wph $f wff ph $.               →  token "ph" has typecode "wff"
//   vx  $f setvar x $.             →  token "x" has typecode "setvar"
//
// Parsing is top-down recursive descent with backtracking:
//   1. To parse expr[pos..] as typecode T:
//   2.   If expr[pos] is a known variable of type T → leaf node (1 token)
//   3.   For each syntax axiom producing type T:
//   4.     Walk the pattern: literals must match exactly, variables are
//          parsed recursively for their typecode
//   5.     If full pattern matches → return SyntaxNode with children
//
// This is efficient because:
//   - Metamath expressions are fully parenthesized (unambiguous)
//   - Setvars are always single tokens
//   - Literal delimiters ("(", ")", "->", etc.) anchor the parse
//
// The parser replaces hand-written pattern matching, automatically handling
// every syntax constructor defined in set.mm.
// ---------------------------------------------------------------------------

// A node in the parse tree
struct SyntaxNode {
    std::string label;     // syntax axiom label (e.g. "wi", "cin") or "$f" for leaves
    std::string typecode;  // "wff", "class", "setvar"
    std::string token;     // for leaf nodes: the variable token name

    // Children correspond to variable slots in the syntax axiom pattern,
    // in the order they appear in the axiom expression.
    std::vector<SyntaxNode> children;

    bool is_leaf() const { return children.empty(); }
};

class SyntaxParser {
public:
    explicit SyntaxParser(const MmDatabase& db);

    // Parse an expression (first token = typecode, rest = body).
    // Returns the syntax tree or nullopt on failure.
    std::optional<SyntaxNode> parse(const Expression& expr) const;

    // Parse expr[start..] as the given typecode.
    // Returns (node, one-past-end position) or nullopt.
    std::optional<std::pair<SyntaxNode, size_t>> parse_at(
        const Expression& expr, size_t start,
        const std::string& typecode) const;

private:
    // A grammar rule derived from a syntax axiom
    struct PatternElement {
        bool is_variable;
        std::string value;     // literal token, or typecode for variables
        std::string var_name;  // original variable name (only for variables)
    };

    struct GrammarRule {
        std::string label;     // syntax axiom label
        std::string typecode;  // LHS typecode
        std::vector<PatternElement> pattern;
    };

    // Rules grouped by LHS typecode
    std::unordered_map<std::string, std::vector<GrammarRule>> rules_;

    // Variable name → typecode (from all $f in the database)
    std::unordered_map<std::string, std::string> var_types_;

    // Memoization cache: (position, typecode) → parse result
    using CacheKey = std::pair<size_t, std::string>;
    struct CacheKeyHash {
        size_t operator()(const CacheKey& k) const {
            return std::hash<size_t>()(k.first) ^
                   (std::hash<std::string>()(k.second) << 16);
        }
    };
    using CacheValue = std::optional<std::pair<SyntaxNode, size_t>>;
    using Cache = std::unordered_map<CacheKey, CacheValue, CacheKeyHash>;

    // Recursive parse with memoization
    std::optional<std::pair<SyntaxNode, size_t>> parse_impl(
        const Expression& expr, size_t start,
        const std::string& typecode, Cache& cache) const;

    bool try_rule(const Expression& expr, size_t pos,
                  const GrammarRule& rule,
                  std::vector<SyntaxNode>& children,
                  size_t& end_pos, Cache& cache) const;
};

}  // namespace metamath
