#include "syntax_to_wff.h"

namespace metamath {

std::string SyntaxToWff::fresh_var() {
    std::string v;
    do {
        v = "zz" + std::to_string(fresh_counter_++);
    } while (setvars_.count(v));
    return v;
}

std::string SyntaxToWff::class_to_setvar(const SyntaxNode& node) const {
    // cv(x): class → setvar coercion
    if (node.label == "cv" && node.children.size() == 1 &&
        node.children[0].typecode == "setvar") {
        return node.children[0].token;
    }
    // Class variable leaf (e.g. A, B): treated as setvar in our encoding
    if (node.typecode == "class" && node.is_leaf()) {
        return node.token;
    }
    // Bare setvar leaf
    if (node.typecode == "setvar" && node.is_leaf()) {
        return node.token;
    }
    return "";
}

WffPtr SyntaxToWff::convert(const SyntaxNode& node,
                             const std::unordered_set<std::string>& setvars,
                             const std::unordered_set<std::string>& wff_vars) {
    setvars_ = setvars;
    wff_vars_ = wff_vars;
    extra_vars_.clear();
    fresh_counter_ = 0;
    return convert_wff(node);
}

WffPtr SyntaxToWff::convert_wff(const SyntaxNode& node) {
    // Leaf: wff variable (only actual $f variable leaves, not zero-child syntax axioms)
    if (node.label == "$f" && node.typecode == "wff") {
        return wff_var(node.token);
    }

    const auto& L = node.label;
    const auto& C = node.children;

    // --- Propositional connectives ---

    if (L == "wi" && C.size() == 2) {
        auto lhs = convert_wff(C[0]);
        auto rhs = convert_wff(C[1]);
        // (A -> A) where A is a ground term (no Var/Verum/Falsum) = Verum.
        // Only collapse fully ground self-implications like (forall x. eq(x,x) -> forall x. eq(x,x))
        // from df-tru/df-fal. Don't collapse (F.->F.) or (T.->T.) because these would create
        // mismatches between frame-level ASTs and substitution-level ASTs at ax-mp boundaries.
        if (*lhs == *rhs && !any_leaf(*lhs, [](const WffNode& n) {
                return n.kind == WffNode::Kind::Var ||
                       n.kind == WffNode::Kind::Verum ||
                       n.kind == WffNode::Kind::Falsum; }))
            return wff_verum();
        return wff_binary(WffNode::Op::Implies, std::move(lhs), std::move(rhs));
    }
    if (L == "wa" && C.size() == 2)
        return wff_binary(WffNode::Op::And, convert_wff(C[0]), convert_wff(C[1]));
    if (L == "wo" && C.size() == 2)
        return wff_binary(WffNode::Op::Or, convert_wff(C[0]), convert_wff(C[1]));
    if (L == "wb" && C.size() == 2)
        return wff_binary(WffNode::Op::Iff, convert_wff(C[0]), convert_wff(C[1]));
    if (L == "wn" && C.size() == 1) {
        auto child = convert_wff(C[0]);
        // ~T. = F. (needed for df-fal to be identity biconditional)
        if (child->kind == WffNode::Kind::Verum) return wff_falsum();
        return wff_neg(std::move(child));
    }

    // n-ary conjunction/disjunction: w3a, w3o
    if (L == "w3a" && C.size() == 3)
        return wff_binary(WffNode::Op::And,
            wff_binary(WffNode::Op::And, convert_wff(C[0]), convert_wff(C[1])),
            convert_wff(C[2]));
    if (L == "w3o" && C.size() == 3)
        return wff_binary(WffNode::Op::Or,
            wff_binary(WffNode::Op::Or, convert_wff(C[0]), convert_wff(C[1])),
            convert_wff(C[2]));

    // --- Quantifiers ---

    if (L == "wal" && C.size() == 2) {
        std::string var = C[0].token;  // setvar
        WffPtr body = convert_wff(C[1]);
        // Setvar quantifier: vacuous in wff-as-set encoding → strip,
        // BUT only if the body doesn't actually use the variable as a term.
        // E.g. A.x ph → strip (ph doesn't contain x).
        //      A.x (x e. A -> x e. B) → keep (body uses x concretely).
        if (setvars_.count(var) && !has_free_term_var(*body, var))
            return body;
        return wff_forall(var, std::move(body));
    }
    if (L == "wex" && C.size() == 2) {
        std::string var = C[0].token;
        WffPtr body = convert_wff(C[1]);
        if (setvars_.count(var) && !has_free_term_var(*body, var))
            return body;
        return wff_exists(var, std::move(body));
    }

    // --- Verum / Falsum ---

    if (L == "wtru") return wff_verum();
    if (L == "wfal") return wff_falsum();

    // --- Non-freeness: F/ x ph → (∃x.ph → ∀x.ph) ---

    if (L == "wnf" && C.size() == 2) {
        std::string var = C[0].token;
        WffPtr body = convert_wff(C[1]);
        // Setvar quantifiers are vacuous in wff-as-set encoding,
        // but only if body doesn't actually use the variable as a term.
        if (setvars_.count(var) && !has_free_term_var(*body, var))
            return wff_binary(WffNode::Op::Implies, body, body);
        return wff_binary(WffNode::Op::Implies,
                          wff_exists(var, body),
                          wff_forall(var, body));
    }

    // --- Unique existence: E! x ph → ∃x(φ ∧ ∀y(φ[y/x] → y=x)) ---

    if (L == "weu" && C.size() == 2) {
        std::string x = C[0].token;
        WffPtr body = convert_wff(C[1]);
        std::string y = fresh_var();
        extra_vars_.push_back(y);
        auto inner = wff_binary(WffNode::Op::And, body,
            wff_forall(y,
                wff_binary(WffNode::Op::Implies,
                    wff_subst(body, x, y),
                    wff_pred("eq", {y, x}))));
        // Strip outer ∃x only if the expansion doesn't use x as a term.
        // inner always contains x (in eq(y, x)), so never strip.
        return wff_exists(x, std::move(inner));
    }

    // --- "At most one": E* x ph → ∃y∀x(φ → x=y) ---

    if (L == "wmo" && C.size() == 2) {
        std::string x = C[0].token;
        WffPtr body = convert_wff(C[1]);
        std::string y = fresh_var();
        extra_vars_.push_back(y);
        // inner always contains x (in eq(x, y)), so never strip ∀x.
        auto inner = wff_binary(WffNode::Op::Implies,
            std::move(body), wff_pred("eq", {x, y}));
        return wff_exists(y, wff_forall(x, std::move(inner)));
    }

    // --- Not an element: A e/ B → nel(A, B) for setvars, ¬(A ∈ B) for compound ---

    if (L == "wnel" && C.size() == 2) {
        std::string sv1 = class_to_setvar(C[0]);
        std::string sv2 = class_to_setvar(C[1]);
        if (!sv1.empty() && !sv2.empty())
            return wff_pred("nel", {sv1, sv2});
        // Build wcel from children, then negate
        SyntaxNode wcel_node;
        wcel_node.label = "wcel";
        wcel_node.typecode = "wff";
        wcel_node.children = {C[0], C[1]};
        return wff_neg(convert_wff(wcel_node));
    }

    // --- Conditional equality: CondEq(x=y → φ) → (x=y → φ) ---

    if (L == "wcdeq" && C.size() == 3) {
        return wff_binary(WffNode::Op::Implies,
            wff_pred("eq", {C[0].token, C[1].token}),
            convert_wff(C[2]));
    }

    // --- Non-freeness for classes: F/_ x A → (∃x.body → ∀x.body) ---
    // wnfc: F/_ x A where A is class
    // Nfc(x, A) means x is not free in A, which is:
    // ∀y(y ∈ A → ∀x(y ∈ A))  — but simplified: F/(x, y ∈ A) for fresh y

    if (L == "wnfc" && C.size() == 2) {
        std::string x = C[0].token;
        std::string y = fresh_var();
        extra_vars_.push_back(y);
        WffPtr mem = expand_membership(y, C[1]);
        // Only simplify if the membership expansion doesn't use x as a term.
        if (setvars_.count(x) && !has_free_term_var(*mem, x))
            return wff_binary(WffNode::Op::Implies, mem, mem);
        return wff_binary(WffNode::Op::Implies,
            wff_exists(x, mem),
            wff_forall(x, mem));
    }

    // --- XOR, NAND, NOR ---

    if (L == "wnor" && C.size() == 2)
        return wff_neg(wff_binary(WffNode::Op::Or, convert_wff(C[0]), convert_wff(C[1])));
    if (L == "wxo" && C.size() == 2)
        return wff_neg(wff_binary(WffNode::Op::Iff, convert_wff(C[0]), convert_wff(C[1])));
    if (L == "wnan" && C.size() == 2)
        return wff_neg(wff_binary(WffNode::Op::And, convert_wff(C[0]), convert_wff(C[1])));
    if (L == "whad" && C.size() == 3)
        return wff_neg(wff_binary(WffNode::Op::Iff,
            wff_neg(wff_binary(WffNode::Op::Iff, convert_wff(C[0]), convert_wff(C[1]))),
            convert_wff(C[2])));
    if (L == "wcad" && C.size() == 3)
        return wff_binary(WffNode::Op::Or,
            wff_binary(WffNode::Op::And, convert_wff(C[0]), convert_wff(C[1])),
            wff_binary(WffNode::Op::And, convert_wff(C[2]),
                wff_neg(wff_binary(WffNode::Op::Iff, convert_wff(C[0]), convert_wff(C[1])))));

    // --- Conditional wff: if-(ph, ps, ch) → (ph ∧ ps) ∨ (¬ph ∧ ch) ---

    if (L == "wif" && C.size() == 3) {
        auto ph = convert_wff(C[0]);
        return wff_binary(WffNode::Op::Or,
            wff_binary(WffNode::Op::And, ph, convert_wff(C[1])),
            wff_binary(WffNode::Op::And, wff_neg(ph), convert_wff(C[2])));
    }

    // --- Class equality: A = B ---

    if (L == "wceq" && C.size() == 2)
        return expand_class_eq(C[0], C[1]);

    // --- Class membership: A ∈ B ---

    if (L == "wcel" && C.size() == 2) {
        std::string lhs_var = class_to_setvar(C[0]);
        if (!lhs_var.empty()) {
            return expand_membership(lhs_var, C[1]);
        }
        // Compound LHS: df-clel: A ∈ B ↔ ∃x(x = A ∧ x ∈ B)
        std::string x = fresh_var();
        extra_vars_.push_back(x);
        return wff_exists(x,
            wff_binary(WffNode::Op::And,
                expand_eq_var(x, C[0]),
                expand_membership(x, C[1])));
    }

    // --- Inequality: A ≠ B → ne(A, B) for setvars, ¬(A = B) for compound ---

    if (L == "wne" && C.size() == 2) {
        std::string sv1 = class_to_setvar(C[0]);
        std::string sv2 = class_to_setvar(C[1]);
        if (!sv1.empty() && !sv2.empty())
            return wff_pred("ne", {sv1, sv2});
        return wff_neg(expand_class_eq(C[0], C[1]));
    }

    // --- Subset: A ⊆ B → ∀x(x ∈ A → x ∈ B) ---

    if (L == "wss" && C.size() == 2) {
        std::string x = fresh_var();
        extra_vars_.push_back(x);
        return wff_forall(x,
            wff_binary(WffNode::Op::Implies,
                expand_membership(x, C[0]),
                expand_membership(x, C[1])));
    }

    // --- Proper subset: A ⊊ B → (A ⊆ B ∧ A ≠ B) ---

    if (L == "wpss" && C.size() == 2) {
        std::string x = fresh_var();
        extra_vars_.push_back(x);
        return wff_binary(WffNode::Op::And,
            wff_forall(x,
                wff_binary(WffNode::Op::Implies,
                    expand_membership(x, C[0]),
                    expand_membership(x, C[1]))),
            wff_neg(expand_class_eq(C[0], C[1])));
    }

    // --- Restricted quantifiers ---

    // wral: A. x e. A ph → children: [setvar x, class A, wff ph]
    // ∀x∈A.φ → ∀x(x∈A → φ)
    if (L == "wral" && C.size() == 3) {
        std::string var = C[0].token;
        WffPtr body = convert_wff(C[2]);
        WffPtr full = wff_binary(WffNode::Op::Implies,
            expand_membership(var, C[1]), std::move(body));
        // Strip forall if var is setvar and body doesn't use var as a term.
        // Note: membership expansion always introduces var, but the body (C[2])
        // may not. We check the full expression for consistency with wal.
        if (setvars_.count(var) && !has_free_term_var(*full, var))
            return full;
        return wff_forall(var, std::move(full));
    }

    // wrex: E. x e. A ph → children: [setvar x, class A, wff ph]
    // ∃x∈A.φ → ∃x(x∈A ∧ φ)
    if (L == "wrex" && C.size() == 3) {
        std::string var = C[0].token;
        WffPtr body = convert_wff(C[2]);
        WffPtr full = wff_binary(WffNode::Op::And,
            expand_membership(var, C[1]), std::move(body));
        if (setvars_.count(var) && !has_free_term_var(*full, var))
            return full;
        return wff_exists(var, std::move(full));
    }

    // wreu: E! x e. A ph → children: [setvar x, class A, wff ph]
    // ∃!x∈A.φ → ∃x(x∈A ∧ φ ∧ ∀y(y∈A ∧ φ[y/x] → y=x))
    if (L == "wreu" && C.size() == 3) {
        std::string x = C[0].token;
        WffPtr body = convert_wff(C[2]);
        WffPtr mem = expand_membership(x, C[1]);
        std::string y = fresh_var();
        extra_vars_.push_back(y);
        return wff_exists(x,
            wff_binary(WffNode::Op::And,
                wff_binary(WffNode::Op::And, mem, body),
                wff_forall(y,
                    wff_binary(WffNode::Op::Implies,
                        wff_binary(WffNode::Op::And,
                            wff_subst(mem, x, y),
                            wff_subst(body, x, y)),
                        wff_pred("eq", {y, x})))));
    }

    // wrmo: E* x e. A ph → children: [setvar x, class A, wff ph]
    // E*x∈A.φ → ∃y∀x(x∈A ∧ φ → x=y)
    if (L == "wrmo" && C.size() == 3) {
        std::string x = C[0].token;
        WffPtr body = convert_wff(C[2]);
        WffPtr mem = expand_membership(x, C[1]);
        std::string y = fresh_var();
        extra_vars_.push_back(y);
        return wff_exists(y,
            wff_forall(x,
                wff_binary(WffNode::Op::Implies,
                    wff_binary(WffNode::Op::And, std::move(mem), std::move(body)),
                    wff_pred("eq", {x, y}))));
    }

    // --- Substitution ---

    // wsbc: [. A / x ]. ph → children: [class A, setvar x, wff ph]
    if (L == "wsbc" && C.size() == 3) {
        std::string x = C[1].token;
        std::string a = class_to_setvar(C[0]);
        if (!a.empty()) {
            WffPtr body = convert_wff(C[2]);
            return wff_subst(body, x, a);
        }
        // Complex: fall through to unsupported
    }

    // wsb: [ y / x ] ph → children: [setvar y, setvar x, wff ph]
    // Substitution: φ[y/x]
    if (L == "wsb" && C.size() == 3) {
        std::string y = C[0].token;
        std::string x = C[1].token;
        WffPtr body = convert_wff(C[2]);
        return wff_subst(body, x, y);
    }

    // --- Leaf nodes of non-wff type reaching convert_wff ---
    if (node.label == "$f") {
        return wff_literal("??leaf:" + node.typecode + ":" + node.token + "??");
    }
    // Zero-child syntax axiom not handled above
    if (node.children.empty()) {
        return wff_literal("??syntax:" + node.label + "??");
    }

    // --- Unsupported: return a literal marker ---

    return wff_literal("??syntax:" + L + "??");
}

// ---------------------------------------------------------------------------
// Membership expansion: t ∈ C
// ---------------------------------------------------------------------------

WffPtr SyntaxToWff::expand_membership(const std::string& t,
                                       const SyntaxNode& cls) {
    const auto& L = cls.label;
    const auto& C = cls.children;

    // cv(x): t ∈ x → elem(t, x)
    std::string sv = class_to_setvar(cls);
    if (!sv.empty())
        return wff_pred("elem", {t, sv});

    // cab(x, φ): t ∈ {x|φ} → φ[t/x]
    if (L == "cab" && C.size() == 2) {
        std::string x = C[0].token;  // bound setvar
        WffPtr body = convert_wff(C[1]);
        return wff_subst(body, x, t);
    }

    // crab: { x e. A | ph } → children: [setvar x, class A, wff ph]
    // t ∈ {x∈A|φ} → t∈A ∧ φ[t/x]
    if (L == "crab" && C.size() == 3) {
        std::string x = C[0].token;
        WffPtr body = convert_wff(C[2]);
        return wff_binary(WffNode::Op::And,
            expand_membership(t, C[1]),
            wff_subst(body, x, t));
    }

    // cin(A, B): t ∈ A∩B → t∈A ∧ t∈B
    if (L == "cin" && C.size() == 2)
        return wff_binary(WffNode::Op::And,
            expand_membership(t, C[0]),
            expand_membership(t, C[1]));

    // cun(A, B): t ∈ A∪B → t∈A ∨ t∈B
    if (L == "cun" && C.size() == 2)
        return wff_binary(WffNode::Op::Or,
            expand_membership(t, C[0]),
            expand_membership(t, C[1]));

    // cdif(A, B): t ∈ A\B → t∈A ∧ ¬t∈B
    if (L == "cdif" && C.size() == 2)
        return wff_binary(WffNode::Op::And,
            expand_membership(t, C[0]),
            wff_neg(expand_membership(t, C[1])));

    // c0: t ∈ ∅ → ⊥
    if (L == "c0") return wff_falsum();

    // cvv: t ∈ V → ⊤
    if (L == "cvv") return wff_verum();

    // csn(A): t ∈ {A} → t = A
    if (L == "csn" && C.size() == 1)
        return expand_eq_var(t, C[0]);

    // cpr(A, B): t ∈ {A,B} → t=A ∨ t=B
    if (L == "cpr" && C.size() == 2)
        return wff_binary(WffNode::Op::Or,
            expand_eq_var(t, C[0]),
            expand_eq_var(t, C[1]));

    // ctp(A, B, C): t ∈ {A,B,C} → t=A ∨ t=B ∨ t=C
    if (L == "ctp" && C.size() == 3)
        return wff_binary(WffNode::Op::Or,
            wff_binary(WffNode::Op::Or,
                expand_eq_var(t, C[0]),
                expand_eq_var(t, C[1])),
            expand_eq_var(t, C[2]));

    // cpw(A): t ∈ 𝒫(A) → ∀z(z∈t → z∈A)
    if (L == "cpw" && C.size() == 1) {
        std::string z = fresh_var();
        extra_vars_.push_back(z);
        return wff_forall(z,
            wff_binary(WffNode::Op::Implies,
                wff_pred("elem", {z, t}),
                expand_membership(z, C[0])));
    }

    // cuni(A): t ∈ ⋃A → ∃y(t∈y ∧ y∈A)
    if (L == "cuni" && C.size() == 1) {
        std::string y = fresh_var();
        extra_vars_.push_back(y);
        return wff_exists(y,
            wff_binary(WffNode::Op::And,
                wff_pred("elem", {t, y}),
                expand_membership(y, C[0])));
    }

    // cint(A): t ∈ ⋂A → ∀y(y∈A → t∈y)
    if (L == "cint" && C.size() == 1) {
        std::string y = fresh_var();
        extra_vars_.push_back(y);
        return wff_forall(y,
            wff_binary(WffNode::Op::Implies,
                expand_membership(y, C[0]),
                wff_pred("elem", {t, y})));
    }

    // ciun(x, A, B): t ∈ ⋃x∈A B → ∃x(x∈A ∧ t∈B)
    if (L == "ciun" && C.size() == 3) {
        std::string x = C[0].token;
        return wff_exists(x,
            wff_binary(WffNode::Op::And,
                expand_membership(x, C[1]),
                expand_membership(t, C[2])));
    }

    // ciin(x, A, B): t ∈ ⋂x∈A B → ∀x(x∈A → t∈B)
    if (L == "ciin" && C.size() == 3) {
        std::string x = C[0].token;
        return wff_forall(x,
            wff_binary(WffNode::Op::Implies,
                expand_membership(x, C[1]),
                expand_membership(t, C[2])));
    }

    // cif(φ, A, B): t ∈ if(φ,A,B) → (t∈A ∧ φ) ∨ (t∈B ∧ ¬φ)
    // Operand order matches df-if: { x | ((x e. A /\ ph) \/ (x e. B /\ -. ph)) }
    if (L == "cif" && C.size() == 3) {
        auto ph = convert_wff(C[0]);
        return wff_binary(WffNode::Op::Or,
            wff_binary(WffNode::Op::And, expand_membership(t, C[1]), ph),
            wff_binary(WffNode::Op::And, expand_membership(t, C[2]), wff_neg(ph)));
    }

    // cop(A, B): t ∈ ⟨A,B⟩ → Kuratowski: t ∈ {{A}, {A,B}}
    // Expand: t = {A} ∨ t = {A,B}
    // Where t = {A} means ∀z(z∈t ↔ z=A), t = {A,B} means ∀z(z∈t ↔ z=A∨z=B)
    if (L == "cop" && C.size() == 2) {
        std::string z = fresh_var();
        extra_vars_.push_back(z);
        // t ∈ {{A}, {A,B}}
        // = ∀z(z∈t ↔ z={A}) ∨ ∀z(z∈t ↔ z={A,B})
        // We can't expand further without more fresh vars; use csn/cpr
        // Actually: t ∈ {X,Y} where X = csn(A), Y = cpr(A,B)
        // = t = X ∨ t = Y
        // Build via expand_eq_var
        SyntaxNode sn_node;
        sn_node.label = "csn"; sn_node.typecode = "class";
        sn_node.children = {C[0]};  // {A}
        SyntaxNode pr_node;
        pr_node.label = "cpr"; pr_node.typecode = "class";
        pr_node.children = {C[0], C[1]};  // {A,B}
        return wff_binary(WffNode::Op::Or,
            expand_eq_var(t, sn_node),
            expand_eq_var(t, pr_node));
    }

    // cotp(A, B, C): t ∈ ⟨A,B,C⟩ = ⟨⟨A,B⟩,C⟩
    if (L == "cotp" && C.size() == 3) {
        SyntaxNode inner;
        inner.label = "cop"; inner.typecode = "class";
        inner.children = {C[0], C[1]};
        SyntaxNode outer;
        outer.label = "cop"; outer.typecode = "class";
        outer.children = {inner, C[2]};
        return expand_membership(t, outer);
    }

    // csymdif(A, B): t ∈ A △ B → (t∈A ∧ ¬t∈B) ∨ (t∈B ∧ ¬t∈A)
    // Operand order matches df-symdif: (A \ B) ∪ (B \ A) where cdif gives (mem ∧ ¬mem)
    if (L == "csymdif" && C.size() == 2)
        return wff_binary(WffNode::Op::Or,
            wff_binary(WffNode::Op::And,
                expand_membership(t, C[0]),
                wff_neg(expand_membership(t, C[1]))),
            wff_binary(WffNode::Op::And,
                expand_membership(t, C[1]),
                wff_neg(expand_membership(t, C[0]))));

    // csb(A, x, B): t ∈ [_A/x]_B → [.A/x].t∈B (class substitution)
    // children: [class A, setvar x, class B]
    if (L == "csb" && C.size() == 3) {
        std::string x = C[1].token;
        std::string a = class_to_setvar(C[0]);
        if (!a.empty()) {
            // Simple: substitute a for x in membership
            WffPtr mem = expand_membership(t, C[2]);
            return wff_subst(mem, x, a);
        }
        // Complex: fall through
    }

    // Unsupported class expression
    return wff_literal("??class:" + L + "??");
}

// ---------------------------------------------------------------------------
// Equality expansion: t = C (setvar = class)
// ---------------------------------------------------------------------------

WffPtr SyntaxToWff::expand_eq_var(const std::string& t,
                                   const SyntaxNode& cls) {
    std::string sv = class_to_setvar(cls);
    if (!sv.empty())
        return wff_pred("eq", {t, sv});

    // General: t = C ↔ ∀z(z∈t ↔ z∈C)
    std::string z = fresh_var();
    extra_vars_.push_back(z);
    return wff_forall(z,
        wff_binary(WffNode::Op::Iff,
            wff_pred("elem", {z, t}),
            expand_membership(z, cls)));
}

// ---------------------------------------------------------------------------
// Class equality: C1 = C2
// ---------------------------------------------------------------------------

WffPtr SyntaxToWff::expand_class_eq(const SyntaxNode& c1,
                                     const SyntaxNode& c2) {
    std::string sv1 = class_to_setvar(c1);
    std::string sv2 = class_to_setvar(c2);
    if (!sv1.empty() && !sv2.empty())
        return wff_pred("eq", {sv1, sv2});

    // One side is setvar
    if (!sv1.empty())
        return expand_eq_var(sv1, c2);
    if (!sv2.empty())
        return expand_eq_var(sv2, c1);

    // General: C1 = C2 ↔ ∀x(x∈C1 ↔ x∈C2)
    std::string x = fresh_var();
    extra_vars_.push_back(x);
    return wff_forall(x,
        wff_binary(WffNode::Op::Iff,
            expand_membership(x, c1),
            expand_membership(x, c2)));
}

}  // namespace metamath
