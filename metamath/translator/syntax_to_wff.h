#pragma once

#include "syntax_parser.h"
#include "wff_ast.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace metamath {

// ---------------------------------------------------------------------------
// SyntaxNode → WffPtr converter
// ---------------------------------------------------------------------------
//
// Converts a SyntaxNode tree (from SyntaxParser) into a WffPtr AST
// (for emission as FOL formulas). Handles:
//
// 1. Propositional connectives: wi, wa, wo, wb, wn → binary/neg WffNodes
// 2. Quantifiers: wal, wex → forall/exists WffNodes
// 3. Atomic predicates via class expansion:
//    - wceq (A = B)  → eq(x,y) or extensionality expansion
//    - wcel (A ∈ B)  → elem(x,y) or membership expansion
//    - wne  (A ≠ B)  → ¬eq(...)
//    - wss  (A ⊆ B)  → ∀x(x∈A → x∈B)
// 4. Class operations expanded inline via definitions:
//    - cab  {x|φ}    → φ[t/x] (when used in membership context)
//    - cin  (A∩B)    → t∈A ∧ t∈B
//    - cun  (A∪B)    → t∈A ∨ t∈B
//    - cdif (A\B)    → t∈A ∧ ¬t∈B
//    - crab {x∈A|φ}  → t∈A ∧ φ[t/x]
//    - cuni ⋃A       → ∃y(t∈y ∧ y∈A)
//    - cint ⋂A       → ∀y(y∈A → t∈y)
//    - c0   ∅        → ⊥
//    - cvv  V        → ⊤
//    - cpw  𝒫(A)     → ∀z(z∈t → z∈A)
//    - csn  {A}      → t=A
//    - cpr  {A,B}    → t=A ∨ t=B
//
// Fresh variables (zz0, zz1, ...) are generated for expansions that
// require them and tracked in `extra_vars`.
// ---------------------------------------------------------------------------

class SyntaxToWff {
public:
    // Convert a wff-typed SyntaxNode to WffPtr.
    // `setvars` is the set of known setvars (for quantifier stripping).
    // `wff_vars` is the set of wff variable names.
    WffPtr convert(const SyntaxNode& node,
                   const std::unordered_set<std::string>& setvars,
                   const std::unordered_set<std::string>& wff_vars);

    // Fresh variables generated during conversion.
    // Caller should add these to the theorem's fixed variables.
    const std::vector<std::string>& extra_vars() const { return extra_vars_; }

private:
    std::unordered_set<std::string> setvars_;
    std::unordered_set<std::string> wff_vars_;
    std::vector<std::string> extra_vars_;
    int fresh_counter_ = 0;

    std::string fresh_var();

    // Convert a wff-typed node
    WffPtr convert_wff(const SyntaxNode& node);

    // Get the setvar name from a class node (must be cv(x) leaf)
    // Returns empty string if not a simple setvar.
    std::string class_to_setvar(const SyntaxNode& node) const;

    // Expand membership: t ∈ C  (t is a setvar, C is a class syntax node)
    WffPtr expand_membership(const std::string& t, const SyntaxNode& cls);

    // Expand equality between a setvar and a class: t = C
    WffPtr expand_eq_var(const std::string& t, const SyntaxNode& cls);

    // Expand class equality: C1 = C2
    WffPtr expand_class_eq(const SyntaxNode& c1, const SyntaxNode& c2);
};

}  // namespace metamath
