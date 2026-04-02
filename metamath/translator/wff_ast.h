#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace metamath {

struct WffNode;
using WffPtr = std::shared_ptr<const WffNode>;

struct WffNode {
    enum class Kind { Var, Literal, Pred, Verum, Falsum, Neg, Binary, Forall, Exists };
    enum class Op   { Implies, And, Or, Iff };

    Kind kind;
    Op   op;           // meaningful only when kind == Binary
    std::string name;  // Var: wff variable ("ph", "ps", ...)
                       // Literal: verbatim FOL string ("elem(x, y)")
                       // Pred: predicate name ("eq", "elem")
                       // Forall/Exists: bound variable name
    std::vector<std::string> args;  // Pred: term arguments (e.g. {"x", "y"})
    WffPtr left;       // Neg: child. Binary: lhs. Forall/Exists: body.
    WffPtr right;      // Binary: rhs. Others: nullptr.

    // Syntactic (name-sensitive) equality — NOT alpha-equivalence.
    bool operator==(const WffNode& other) const;
    bool operator!=(const WffNode& other) const { return !(*this == other); }
};

// --- Factory functions ---

WffPtr wff_var(std::string name);
WffPtr wff_literal(std::string fol_str);      // legacy: verbatim string
WffPtr wff_pred(std::string pred_name,         // structured atomic predicate
                std::vector<std::string> args);
WffPtr wff_verum();
WffPtr wff_falsum();
WffPtr wff_neg(WffPtr child);
WffPtr wff_binary(WffNode::Op op, WffPtr lhs, WffPtr rhs);
WffPtr wff_forall(std::string var, WffPtr body);
WffPtr wff_exists(std::string var, WffPtr body);

// --- Substitution ---

// Replace all free occurrences of term variable `old_var` with `new_var`.
// This operates on Pred args and Forall/Exists bound variable names.
// Respects shadowing: if a quantifier binds `old_var`, the body is not touched.
WffPtr wff_subst(const WffPtr& node,
                 const std::string& old_var,
                 const std::string& new_var);

// Simultaneous substitution: apply all renames at once to avoid collision
// when a target name is also a source name (e.g., {A→B, B→D}).
WffPtr wff_subst_map(const WffPtr& node,
                     const std::unordered_map<std::string, std::string>& rename);

// --- Emission ---

// Render a WffNode tree to a FOL formula string.
// render_leaf is called for Var, Literal, Pred, Verum, and Falsum nodes.
using LeafRenderer = std::function<std::string(const WffNode&)>;
std::string emit_fol(const WffNode& node, const LeafRenderer& render_leaf);

// Default rendering for Pred nodes: "name(arg1, arg2, ...)"
std::string render_pred(const WffNode& node);

// --- Comparison ---

// Alpha-equivalence: structural equality ignoring bound variable names.
// Free variables and predicate names must match exactly.
bool alpha_equal(const WffPtr& a, const WffPtr& b);

// --- Tree queries ---

using LeafPredicate = std::function<bool(const WffNode&)>;
bool any_leaf(const WffNode& node, const LeafPredicate& pred);

// Check if a term variable appears free in the WffPtr tree.
// A term variable is "free" if it appears in a Pred's args and is not
// shadowed by a Forall/Exists binding the same name.
bool has_free_term_var(const WffNode& node, const std::string& var);

}  // namespace metamath
