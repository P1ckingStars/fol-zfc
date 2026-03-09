#include "wff_ast.h"

#include <cassert>

namespace metamath {

// --- Structural equality ---

bool WffNode::operator==(const WffNode& o) const {
    if (kind != o.kind) return false;
    switch (kind) {
        case Kind::Var:
        case Kind::Literal:
            return name == o.name;
        case Kind::Pred:
            return name == o.name && args == o.args;
        case Kind::Verum:
        case Kind::Falsum:
            return true;
        case Kind::Neg:
            return *left == *o.left;
        case Kind::Binary:
            if (op != o.op) return false;
            return *left == *o.left && *right == *o.right;
        case Kind::Forall:
        case Kind::Exists:
            return name == o.name && *left == *o.left;
    }
    return false;
}

// --- Factory functions ---

WffPtr wff_var(std::string name) {
    auto n = std::make_shared<WffNode>();
    n->kind = WffNode::Kind::Var;
    n->name = std::move(name);
    return n;
}

WffPtr wff_literal(std::string fol_str) {
    auto n = std::make_shared<WffNode>();
    n->kind = WffNode::Kind::Literal;
    n->name = std::move(fol_str);
    return n;
}

WffPtr wff_pred(std::string pred_name, std::vector<std::string> args) {
    auto n = std::make_shared<WffNode>();
    n->kind = WffNode::Kind::Pred;
    n->name = std::move(pred_name);
    n->args = std::move(args);
    return n;
}

WffPtr wff_verum() {
    auto n = std::make_shared<WffNode>();
    n->kind = WffNode::Kind::Verum;
    return n;
}

WffPtr wff_falsum() {
    auto n = std::make_shared<WffNode>();
    n->kind = WffNode::Kind::Falsum;
    return n;
}

WffPtr wff_neg(WffPtr child) {
    assert(child && "wff_neg: child must be non-null");
    auto n = std::make_shared<WffNode>();
    n->kind = WffNode::Kind::Neg;
    n->left = std::move(child);
    return n;
}

WffPtr wff_binary(WffNode::Op op, WffPtr lhs, WffPtr rhs) {
    assert(lhs && "wff_binary: lhs must be non-null");
    assert(rhs && "wff_binary: rhs must be non-null");
    auto n = std::make_shared<WffNode>();
    n->kind = WffNode::Kind::Binary;
    n->op = op;
    n->left = std::move(lhs);
    n->right = std::move(rhs);
    return n;
}

WffPtr wff_forall(std::string var, WffPtr body) {
    assert(body && "wff_forall: body must be non-null");
    auto n = std::make_shared<WffNode>();
    n->kind = WffNode::Kind::Forall;
    n->name = std::move(var);
    n->left = std::move(body);
    return n;
}

WffPtr wff_exists(std::string var, WffPtr body) {
    assert(body && "wff_exists: body must be non-null");
    auto n = std::make_shared<WffNode>();
    n->kind = WffNode::Kind::Exists;
    n->name = std::move(var);
    n->left = std::move(body);
    return n;
}

// --- Substitution ---

WffPtr wff_subst(const WffPtr& node,
                 const std::string& old_var,
                 const std::string& new_var) {
    if (!node) return nullptr;
    if (old_var == new_var) return node;

    switch (node->kind) {
        case WffNode::Kind::Var:
        case WffNode::Kind::Literal:
        case WffNode::Kind::Verum:
        case WffNode::Kind::Falsum:
            return node;  // no term variables here

        case WffNode::Kind::Pred: {
            bool changed = false;
            std::vector<std::string> new_args;
            new_args.reserve(node->args.size());
            for (const auto& a : node->args) {
                if (a == old_var) {
                    new_args.push_back(new_var);
                    changed = true;
                } else {
                    new_args.push_back(a);
                }
            }
            if (!changed) return node;
            return wff_pred(node->name, std::move(new_args));
        }

        case WffNode::Kind::Neg: {
            auto child = wff_subst(node->left, old_var, new_var);
            if (child == node->left) return node;
            return wff_neg(std::move(child));
        }

        case WffNode::Kind::Binary: {
            auto l = wff_subst(node->left, old_var, new_var);
            auto r = wff_subst(node->right, old_var, new_var);
            if (l == node->left && r == node->right) return node;
            return wff_binary(node->op, std::move(l), std::move(r));
        }

        case WffNode::Kind::Forall:
        case WffNode::Kind::Exists: {
            // If this quantifier binds old_var, stop (shadowed)
            if (node->name == old_var) return node;
            // If the quantifier binds new_var, we'd need alpha-renaming
            // to avoid capture. For our use case (Metamath vars are distinct),
            // this shouldn't happen. Assert to catch violations early.
            assert(node->name != new_var &&
                   "wff_subst: variable capture — alpha-renaming needed");
            auto body = wff_subst(node->left, old_var, new_var);
            std::string bvar = node->name;
            if (body == node->left) return node;
            if (node->kind == WffNode::Kind::Forall)
                return wff_forall(std::move(bvar), std::move(body));
            else
                return wff_exists(std::move(bvar), std::move(body));
        }
    }
    return node;
}

// --- Emission ---

std::string render_pred(const WffNode& node) {
    std::string result = node.name + "(";
    for (size_t i = 0; i < node.args.size(); ++i) {
        if (i > 0) result += ", ";
        result += node.args[i];
    }
    result += ")";
    return result;
}

std::string emit_fol(const WffNode& node, const LeafRenderer& render_leaf) {
    switch (node.kind) {
        case WffNode::Kind::Var:
        case WffNode::Kind::Literal:
        case WffNode::Kind::Pred:
        case WffNode::Kind::Verum:
        case WffNode::Kind::Falsum:
            return render_leaf(node);

        case WffNode::Kind::Neg: {
            std::string inner = emit_fol(*node.left, render_leaf);
            if (!inner.empty() && inner[0] == '~')
                return "~ " + inner;
            return "~" + inner;
        }

        case WffNode::Kind::Binary: {
            const char* op_str = nullptr;
            switch (node.op) {
                case WffNode::Op::Implies: op_str = " -> ";  break;
                case WffNode::Op::And:     op_str = " & ";   break;
                case WffNode::Op::Or:      op_str = " | ";   break;
                case WffNode::Op::Iff:     op_str = " <-> "; break;
            }
            return "(" + emit_fol(*node.left, render_leaf) +
                   op_str + emit_fol(*node.right, render_leaf) + ")";
        }

        case WffNode::Kind::Forall:
            return "(forall " + node.name + ". " +
                   emit_fol(*node.left, render_leaf) + ")";

        case WffNode::Kind::Exists:
            return "(exists " + node.name + ". " +
                   emit_fol(*node.left, render_leaf) + ")";
    }
    return "??";
}

// --- Tree queries ---

bool any_leaf(const WffNode& node, const LeafPredicate& pred) {
    switch (node.kind) {
        case WffNode::Kind::Var:
        case WffNode::Kind::Literal:
        case WffNode::Kind::Pred:
        case WffNode::Kind::Verum:
        case WffNode::Kind::Falsum:
            return pred(node);

        case WffNode::Kind::Neg:
            return any_leaf(*node.left, pred);

        case WffNode::Kind::Binary:
            return any_leaf(*node.left, pred) || any_leaf(*node.right, pred);

        case WffNode::Kind::Forall:
        case WffNode::Kind::Exists:
            return any_leaf(*node.left, pred);
    }
    return false;
}

void for_each_leaf(const WffNode& node, const LeafVisitor& visit) {
    switch (node.kind) {
        case WffNode::Kind::Var:
        case WffNode::Kind::Literal:
        case WffNode::Kind::Pred:
        case WffNode::Kind::Verum:
        case WffNode::Kind::Falsum:
            visit(node);
            return;

        case WffNode::Kind::Neg:
            for_each_leaf(*node.left, visit);
            return;

        case WffNode::Kind::Binary:
            for_each_leaf(*node.left, visit);
            for_each_leaf(*node.right, visit);
            return;

        case WffNode::Kind::Forall:
        case WffNode::Kind::Exists:
            for_each_leaf(*node.left, visit);
            return;
    }
}

}  // namespace metamath
