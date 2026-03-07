#pragma once

#include <functional>
#include <memory>
#include <string>

namespace metamath {

struct WffNode;
using WffPtr = std::shared_ptr<const WffNode>;

struct WffNode {
    enum class Kind { Var, Literal, Verum, Falsum, Neg, Binary, Forall, Exists };
    enum class Op   { Implies, And, Or, Iff };

    Kind kind;
    Op   op;           // meaningful only when kind == Binary
    std::string name;  // Var: wff variable ("ph", "ps", ...)
                       // Literal: verbatim FOL string ("elem(x, y)", "eq(x, y)")
                       // Forall/Exists: bound variable name
    WffPtr left;       // Neg: child. Binary: lhs. Forall/Exists: body.
    WffPtr right;      // Binary: rhs. Others: nullptr.

    // Syntactic (name-sensitive) equality — NOT alpha-equivalence.
    // forall("x", body) != forall("y", body[x/y]).
    bool operator==(const WffNode& other) const;
    bool operator!=(const WffNode& other) const { return !(*this == other); }
};

// --- Factory functions ---

WffPtr wff_var(std::string name);
WffPtr wff_literal(std::string fol_str);
WffPtr wff_verum();
WffPtr wff_falsum();
WffPtr wff_neg(WffPtr child);           // child must be non-null
WffPtr wff_binary(WffNode::Op op, WffPtr lhs, WffPtr rhs);  // both non-null
WffPtr wff_forall(std::string var, WffPtr body);  // body must be non-null
WffPtr wff_exists(std::string var, WffPtr body);  // body must be non-null

// --- Emission ---

// Render a WffNode tree to a FOL formula string.
// render_leaf is called for Var, Literal, Verum, and Falsum nodes.
using LeafRenderer = std::function<std::string(const WffNode&)>;
std::string emit_fol(const WffNode& node, const LeafRenderer& render_leaf);

// --- Tree queries ---

// Returns true if any leaf (Var, Literal, Verum, Falsum) satisfies pred.
using LeafPredicate = std::function<bool(const WffNode&)>;
bool any_leaf(const WffNode& node, const LeafPredicate& pred);

// Visit every leaf in the tree.
using LeafVisitor = std::function<void(const WffNode&)>;
void for_each_leaf(const WffNode& node, const LeafVisitor& visit);

}  // namespace metamath
