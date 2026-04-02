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

namespace {

// Check if a name appears anywhere in the tree: as a free term variable
// (Pred arg) or as a quantifier binding name.
bool name_occurs(const WffNode& node, const std::string& var) {
    switch (node.kind) {
        case WffNode::Kind::Pred:
            for (const auto& a : node.args)
                if (a == var) return true;
            return false;
        case WffNode::Kind::Var:
        case WffNode::Kind::Literal:
        case WffNode::Kind::Verum:
        case WffNode::Kind::Falsum:
            return false;
        case WffNode::Kind::Neg:
            return name_occurs(*node.left, var);
        case WffNode::Kind::Binary:
            return name_occurs(*node.left, var) ||
                   name_occurs(*node.right, var);
        case WffNode::Kind::Forall:
        case WffNode::Kind::Exists:
            return node.name == var || name_occurs(*node.left, var);
    }
    return false;
}

}  // anonymous namespace

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

            // If the quantifier binds new_var, alpha-rename to avoid capture.
            if (node->name == new_var) {
                std::string fresh = node->name + "'";
                while (name_occurs(*node->left, fresh) ||
                       fresh == old_var || fresh == new_var) {
                    fresh += "'";
                }
                // Rename bound var: node->name → fresh in body
                auto renamed_body = wff_subst(node->left, node->name, fresh);
                // Now substitute old_var → new_var in the renamed body
                auto final_body = wff_subst(renamed_body, old_var, new_var);
                if (node->kind == WffNode::Kind::Forall)
                    return wff_forall(std::move(fresh), std::move(final_body));
                else
                    return wff_exists(std::move(fresh), std::move(final_body));
            }

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

WffPtr wff_subst_map(const WffPtr& node,
                     const std::unordered_map<std::string, std::string>& rename) {
    if (!node || rename.empty()) return node;

    switch (node->kind) {
        case WffNode::Kind::Var:
        case WffNode::Kind::Literal:
        case WffNode::Kind::Verum:
        case WffNode::Kind::Falsum:
            return node;

        case WffNode::Kind::Pred: {
            bool changed = false;
            std::vector<std::string> new_args;
            new_args.reserve(node->args.size());
            for (const auto& a : node->args) {
                auto it = rename.find(a);
                if (it != rename.end() && it->second != a) {
                    new_args.push_back(it->second);
                    changed = true;
                } else {
                    new_args.push_back(a);
                }
            }
            if (!changed) return node;
            return wff_pred(node->name, std::move(new_args));
        }

        case WffNode::Kind::Neg: {
            auto child = wff_subst_map(node->left, rename);
            if (child == node->left) return node;
            return wff_neg(std::move(child));
        }

        case WffNode::Kind::Binary: {
            auto l = wff_subst_map(node->left, rename);
            auto r = wff_subst_map(node->right, rename);
            if (l == node->left && r == node->right) return node;
            return wff_binary(node->op, std::move(l), std::move(r));
        }

        case WffNode::Kind::Forall:
        case WffNode::Kind::Exists: {
            // If this quantifier binds a renamed-from var, remove it from the
            // map (shadowed).  If it binds a renamed-to var, alpha-rename.
            auto it = rename.find(node->name);
            if (it != rename.end()) {
                // Bound var is being renamed — shadow it
                auto reduced = rename;
                reduced.erase(node->name);
                auto body = wff_subst_map(node->left, reduced);
                if (body == node->left) return node;
                if (node->kind == WffNode::Kind::Forall)
                    return wff_forall(std::string(node->name), std::move(body));
                else
                    return wff_exists(std::string(node->name), std::move(body));
            }

            // Check if bound var collides with any rename target
            bool collides = false;
            for (const auto& [from, to] : rename) {
                if (to == node->name) { collides = true; break; }
            }
            if (collides) {
                // Alpha-rename bound var to a fresh name
                std::string fresh = node->name + "'";
                auto needs_fresh = [&](const std::string& candidate) {
                    if (name_occurs(*node->left, candidate)) return true;
                    for (const auto& [from, to] : rename) {
                        if (candidate == from || candidate == to) return true;
                    }
                    return false;
                };
                while (needs_fresh(fresh)) fresh += "'";
                auto renamed_body = wff_subst(node->left, node->name, fresh);
                auto final_body = wff_subst_map(renamed_body, rename);
                if (node->kind == WffNode::Kind::Forall)
                    return wff_forall(std::move(fresh), std::move(final_body));
                else
                    return wff_exists(std::move(fresh), std::move(final_body));
            }

            auto body = wff_subst_map(node->left, rename);
            if (body == node->left) return node;
            if (node->kind == WffNode::Kind::Forall)
                return wff_forall(std::string(node->name), std::move(body));
            else
                return wff_exists(std::string(node->name), std::move(body));
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

// --- Alpha-equivalence ---

namespace {

// Scoped binding stack: pairs of (a_var, b_var) for quantifier introductions.
// Searched top-down so inner bindings shadow outer ones.
using Bindings = std::vector<std::pair<std::string, std::string>>;

// Look up a variable in the binding stack. Returns the paired name if bound,
// or empty string if free.
std::string lookup_a(const Bindings& binds, const std::string& var) {
    for (int i = static_cast<int>(binds.size()) - 1; i >= 0; --i)
        if (binds[i].first == var) return binds[i].second;
    return "";
}

std::string lookup_b(const Bindings& binds, const std::string& var) {
    for (int i = static_cast<int>(binds.size()) - 1; i >= 0; --i)
        if (binds[i].second == var) return binds[i].first;
    return "";
}

bool alpha_eq_impl(const WffPtr& a, const WffPtr& b, Bindings& binds) {
    if (!a && !b) return true;
    if (!a || !b) return false;
    if (a->kind != b->kind) return false;

    switch (a->kind) {
        case WffNode::Kind::Var:
        case WffNode::Kind::Literal:
            return a->name == b->name;

        case WffNode::Kind::Pred: {
            if (a->name != b->name) return false;
            if (a->args.size() != b->args.size()) return false;
            for (size_t i = 0; i < a->args.size(); ++i) {
                const auto& av = a->args[i];
                const auto& bv = b->args[i];
                std::string mapped = lookup_a(binds, av);
                if (!mapped.empty()) {
                    // av is bound in a — bv must be its paired binding
                    if (mapped != bv) return false;
                } else {
                    // av is free in a — bv must also be free and identical
                    if (!lookup_b(binds, bv).empty()) return false;
                    if (av != bv) return false;
                }
            }
            return true;
        }

        case WffNode::Kind::Verum:
        case WffNode::Kind::Falsum:
            return true;

        case WffNode::Kind::Neg:
            return alpha_eq_impl(a->left, b->left, binds);

        case WffNode::Kind::Binary:
            if (a->op != b->op) return false;
            return alpha_eq_impl(a->left, b->left, binds) &&
                   alpha_eq_impl(a->right, b->right, binds);

        case WffNode::Kind::Forall:
        case WffNode::Kind::Exists: {
            binds.push_back({a->name, b->name});
            bool eq = alpha_eq_impl(a->left, b->left, binds);
            binds.pop_back();
            return eq;
        }
    }
    return false;
}

}  // anonymous namespace

bool alpha_equal(const WffPtr& a, const WffPtr& b) {
    Bindings binds;
    return alpha_eq_impl(a, b, binds);
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

bool has_free_term_var(const WffNode& node, const std::string& var) {
    switch (node.kind) {
        case WffNode::Kind::Pred:
            for (const auto& a : node.args)
                if (a == var) return true;
            return false;

        case WffNode::Kind::Var:
        case WffNode::Kind::Literal:
        case WffNode::Kind::Verum:
        case WffNode::Kind::Falsum:
            return false;

        case WffNode::Kind::Neg:
            return has_free_term_var(*node.left, var);

        case WffNode::Kind::Binary:
            return has_free_term_var(*node.left, var) ||
                   has_free_term_var(*node.right, var);

        case WffNode::Kind::Forall:
        case WffNode::Kind::Exists:
            if (node.name == var) return false;  // shadowed by binding
            return has_free_term_var(*node.left, var);
    }
    return false;
}

}  // namespace metamath
