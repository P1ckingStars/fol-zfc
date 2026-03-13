#include "comprehension.h"
#include "proof_emit.h"

#include <algorithm>

namespace metamath {

// Leaf renderer using WffAtom compound_str for Var nodes.
// IMPORTANT: captures `atoms` by reference; caller must ensure it outlives the lambda.
LeafRenderer make_compound_renderer(
    const std::unordered_map<std::string, WffAtom>& atoms) {
    return [&atoms](const WffNode& node) -> std::string {
        if (node.kind == WffNode::Kind::Var) {
            auto it = atoms.find(node.name);
            if (it != atoms.end()) return it->second.compound_str;
        }
        if (node.kind == WffNode::Kind::Verum) {
            auto it = atoms.find("T.");
            if (it != atoms.end()) return it->second.compound_str;
        }
        if (node.kind == WffNode::Kind::Falsum) {
            auto it = atoms.find("F.");
            if (it != atoms.end()) return it->second.compound_str;
        }
        if (node.kind == WffNode::Kind::Literal) return node.name;
        if (node.kind == WffNode::Kind::Pred) return render_pred(node);
        return "??";
    };
}

// Leaf renderer using WffAtom elem_str for Var nodes.
// Verum/Falsum use compound_str (same as make_compound_renderer) because
// make_claim_renderer renders them as compound, so the claim form has them
// in compound form — the elem/compound distinction only applies to Var nodes.
// IMPORTANT: captures `atoms` by reference; caller must ensure it outlives the lambda.
LeafRenderer make_elem_renderer(
    const std::unordered_map<std::string, WffAtom>& atoms) {
    return [&atoms](const WffNode& node) -> std::string {
        if (node.kind == WffNode::Kind::Var) {
            auto it = atoms.find(node.name);
            if (it != atoms.end()) return it->second.elem_str;
        }
        if (node.kind == WffNode::Kind::Verum) {
            auto it = atoms.find("T.");
            if (it != atoms.end()) return it->second.compound_str;
        }
        if (node.kind == WffNode::Kind::Falsum) {
            auto it = atoms.find("F.");
            if (it != atoms.end()) return it->second.compound_str;
        }
        if (node.kind == WffNode::Kind::Literal) return node.name;
        if (node.kind == WffNode::Kind::Pred) return render_pred(node);
        return "??";
    };
}

// Check if any leaf in the subtree needs iff conversion.
// Only Var nodes need conversion (Verum/Falsum are always compound in both
// claim form and target form, so no conversion is needed for them).
bool needs_conv(const WffNode& node,
                const std::unordered_map<std::string, WffAtom>& atoms) {
    return any_leaf(node, [&](const WffNode& leaf) {
        if (leaf.kind != WffNode::Kind::Var) return false;
        auto it = atoms.find(leaf.name);
        return it != atoms.end() && !it->second.iff_handle.empty();
    });
}

// Convert a proof handle between elem-form and compound-form by walking
// the WffNode tree and emitting natural deduction proof steps.
//
// forward=true:  elem-form → compound-form (uses iff_elim_l at atoms)
// forward=false: compound-form → elem-form (uses iff_elim_r at atoms)
std::string convert_proof(
    const WffNode& node,
    const std::string& h,
    const std::unordered_map<std::string, WffAtom>& atoms,
    bool forward,
    ProofState& state)
{
    // Early exit: no atoms in this subtree need conversion
    if (!needs_conv(node, atoms)) return h;

    auto src = forward ? make_elem_renderer(atoms) : make_compound_renderer(atoms);
    auto tgt = forward ? make_compound_renderer(atoms) : make_elem_renderer(atoms);

    switch (node.kind) {

    case WffNode::Kind::Var: {
        auto it = atoms.find(node.name);
        if (it == atoms.end() || it->second.iff_handle.empty()) return h;
        std::string r = state.fresh();
        if (forward)
            state.emit(r + " = iff_elim_l " + it->second.iff_handle + ", " + h);
        else
            state.emit(r + " = iff_elim_r " + it->second.iff_handle + ", " + h);
        return r;
    }

    case WffNode::Kind::Literal:
    case WffNode::Kind::Pred:
    case WffNode::Kind::Verum:
    case WffNode::Kind::Falsum:
        return h;

    case WffNode::Kind::Neg: {
        // h : ~A_src, want ~A_tgt
        std::string a_tgt = emit_fol(*node.left, tgt);
        std::string h_assume = state.fresh();
        state.emit(h_assume + " = assume " + a_tgt);
        std::string h_src = convert_proof(*node.left, h_assume, atoms, !forward, state);
        std::string h_bot = state.fresh();
        state.emit(h_bot + " = not_elim " + h + ", " + h_src);
        std::string r = state.fresh();
        state.emit(r + " = not_intro " + h_bot);
        return r;
    }

    case WffNode::Kind::Binary: {
        switch (node.op) {

        case WffNode::Op::Implies: {
            // h : A_src -> B_src, want A_tgt -> B_tgt
            std::string a_tgt = emit_fol(*node.left, tgt);
            std::string h_assume = state.fresh();
            state.emit(h_assume + " = assume " + a_tgt);
            std::string h_a_src = convert_proof(*node.left, h_assume, atoms, !forward, state);
            std::string h_b_src = state.fresh();
            state.emit(h_b_src + " = implies_elim " + h + ", " + h_a_src);
            std::string h_b_tgt = convert_proof(*node.right, h_b_src, atoms, forward, state);
            std::string r = state.fresh();
            state.emit(r + " = implies_intro " + h_b_tgt);
            return r;
        }

        case WffNode::Op::And: {
            // h : A_src & B_src, want A_tgt & B_tgt
            std::string h_a = state.fresh();
            state.emit(h_a + " = and_elim_l " + h);
            std::string h_a_tgt = convert_proof(*node.left, h_a, atoms, forward, state);
            std::string h_b = state.fresh();
            state.emit(h_b + " = and_elim_r " + h);
            std::string h_b_tgt = convert_proof(*node.right, h_b, atoms, forward, state);
            std::string r = state.fresh();
            state.emit(r + " = and_intro " + h_a_tgt + ", " + h_b_tgt);
            return r;
        }

        case WffNode::Op::Or: {
            // h : A_src | B_src, want A_tgt | B_tgt
            std::string a_src = emit_fol(*node.left, src);
            std::string b_tgt = emit_fol(*node.right, tgt);
            std::string a_tgt = emit_fol(*node.left, tgt);
            // Case A: assume A_src, convert to A_tgt, or_intro_l
            std::string h_a = state.fresh();
            state.emit(h_a + " = assume " + a_src);
            std::string h_a_tgt = convert_proof(*node.left, h_a, atoms, forward, state);
            std::string h_b_let = state.fresh();
            state.emit(h_b_let + " = let " + b_tgt);
            std::string h_a_or = state.fresh();
            state.emit(h_a_or + " = or_intro_l " + h_a_tgt + ", " + h_b_let);
            std::string h_a_imp = state.fresh();
            state.emit(h_a_imp + " = implies_intro " + h_a_or);
            // Case B: assume B_src, convert to B_tgt, or_intro_r
            std::string b_src = emit_fol(*node.right, src);
            std::string h_b = state.fresh();
            state.emit(h_b + " = assume " + b_src);
            std::string h_b_tgt = convert_proof(*node.right, h_b, atoms, forward, state);
            std::string h_a_let = state.fresh();
            state.emit(h_a_let + " = let " + a_tgt);
            std::string h_b_or = state.fresh();
            state.emit(h_b_or + " = or_intro_r " + h_a_let + ", " + h_b_tgt);
            std::string h_b_imp = state.fresh();
            state.emit(h_b_imp + " = implies_intro " + h_b_or);
            // or_elim
            std::string r = state.fresh();
            state.emit(r + " = or_elim " + h + ", " + h_a_imp + ", " + h_b_imp);
            return r;
        }

        case WffNode::Op::Iff: {
            // h : A_src <-> B_src, want A_tgt <-> B_tgt
            std::string a_tgt = emit_fol(*node.left, tgt);
            std::string b_tgt = emit_fol(*node.right, tgt);
            // Forward dir: A_tgt -> B_tgt
            std::string h_fa = state.fresh();
            state.emit(h_fa + " = assume " + a_tgt);
            std::string h_fa_src = convert_proof(*node.left, h_fa, atoms, !forward, state);
            std::string h_fb_src = state.fresh();
            state.emit(h_fb_src + " = iff_elim_l " + h + ", " + h_fa_src);
            std::string h_fb_tgt = convert_proof(*node.right, h_fb_src, atoms, forward, state);
            std::string h_fwd = state.fresh();
            state.emit(h_fwd + " = implies_intro " + h_fb_tgt);
            // Backward dir: B_tgt -> A_tgt
            std::string h_ba = state.fresh();
            state.emit(h_ba + " = assume " + b_tgt);
            std::string h_ba_src = convert_proof(*node.right, h_ba, atoms, !forward, state);
            std::string h_bb_src = state.fresh();
            state.emit(h_bb_src + " = iff_elim_r " + h + ", " + h_ba_src);
            std::string h_bb_tgt = convert_proof(*node.left, h_bb_src, atoms, forward, state);
            std::string h_bwd = state.fresh();
            state.emit(h_bwd + " = implies_intro " + h_bb_tgt);
            // Combine
            std::string r = state.fresh();
            state.emit(r + " = iff_intro " + h_fwd + ", " + h_bwd);
            return r;
        }

        } // switch op
        break;
    }

    case WffNode::Kind::Forall: {
        std::string fresh = state.fresh() + "_v";
        state.emit("fix " + fresh);
        std::string h_inner = state.fresh();
        state.emit(h_inner + " = forall_elim " + h + ", " + fresh);
        std::string h_conv = convert_proof(*node.left, h_inner, atoms, forward, state);
        std::string r = state.fresh();
        state.emit(r + " = forall_intro " + h_conv);
        return r;
    }

    case WffNode::Kind::Exists: {
        std::string witness = state.fresh() + "_w";
        std::string h_body = state.fresh();
        state.emit(h_body + " = iota_elim " + h + ", " + witness);
        std::string h_conv = convert_proof(*node.left, h_body, atoms, forward, state);
        std::string r = state.fresh();
        state.emit(r + " = exists_intro " + h_conv + ", " + witness);
        return r;
    }

    } // switch kind

    return h; // unreachable
}


// --- Comprehension set builder with iff chaining ---

CompResult build_comp_impl(
    const Expression& mm_tokens, size_t start,
    const FrameInfo& caller_info, ProofState& state) {

    size_t pos = start;
    if (pos >= mm_tokens.size()) return {};
    const auto& tok = mm_tokens[pos];

    // Leaf: wff variable in caller's frame
    auto wff_it = caller_info.wff_to_set.find(tok);
    if (wff_it != caller_info.wff_to_set.end()) {
        std::string elem_str = "elem(" + caller_info.dummy_var + ", " +
                               wff_it->second + ")";
        return {wff_it->second, "", elem_str};
    }

    // Verum (T.) / Falsum (F.) — create witness set via comprehension
    if (tok == "T." || tok == "F.") {
        // Always use dummy_var for canonical Verum/Falsum rendering
        const std::string& d = caller_info.dummy_var;
        std::string axiom = (tok == "T.") ? "wff_true" : "wff_false";
        std::string h_ax = state.fresh();
        state.emit(h_ax + " = use " + axiom);
        std::string h1 = state.fresh();
        state.emit(h1 + " = forall_elim " + h_ax + ", " + d);
        std::string witness = state.fresh() + "_w";
        std::string h2 = state.fresh();
        state.emit(h2 + " = iota_elim " + h1 + ", " + witness);
        std::string axiom_iff = state.fresh();
        state.emit(axiom_iff + " = forall_elim " + h2 + ", " + d);
        std::string taut = "(elem(" + d + ", " + d +
                           ") -> elem(" + d + ", " + d + "))";
        std::string compound = (tok == "T.") ? taut : neg(taut);
        return {witness, axiom_iff, compound};
    }

    // Class membership: VAR e. _V → T. (everything is in the universal class)
    if (pos + 2 < mm_tokens.size() && mm_tokens[pos + 1] == "e." &&
        mm_tokens[pos + 2] == "_V") {
        Expression t_expr = {"T."};
        return build_comp_impl(t_expr, 0, caller_info, state);
    }

    // Quantifier normalization: A., E., F/
    if (tok == "A." || tok == "E.") {
        pos++;
        if (pos >= mm_tokens.size()) return {};
        std::string bound = mm_tokens[pos];
        pos++;
        // Detect A. x x = x → T. (always true: reflexivity + universal)
        if (tok == "A." && pos + 2 < mm_tokens.size() &&
            mm_tokens[pos] == bound && mm_tokens[pos + 1] == "=" &&
            mm_tokens[pos + 2] == bound) {
            Expression t_expr = {"T."};
            return build_comp_impl(t_expr, 0, caller_info, state);
        }
        // Detect A. x -. x = VAR → F. (always false: contradicts eq_refl).
        // Covers A. x -. x = y and A. x -. y = x.
        if (tok == "A." && pos + 3 <= mm_tokens.size() &&
            mm_tokens[pos + 0] == "-.") {
            size_t end = mm_tokens.size();
            bool is_neg_eq =
                (end - pos == 4) &&
                mm_tokens[pos + 2] == "=" &&
                (mm_tokens[pos + 1] == bound || mm_tokens[pos + 3] == bound);
            if (is_neg_eq) {
                Expression f_expr = {"F."};
                return build_comp_impl(f_expr, 0, caller_info, state);
            }
        }
        // Detect E. x x = VAR → T. (always true: take x = VAR, eq_refl)
        // Covers both "E. x x = y" and "E. x y = x" (order doesn't matter).
        if (tok == "E." && pos + 2 <= mm_tokens.size()) {
            size_t end = mm_tokens.size();
            bool is_eq_pair =
                (end - pos == 3) &&
                mm_tokens[pos + 1] == "=" &&
                (mm_tokens[pos] == bound || mm_tokens[pos + 2] == bound);
            if (is_eq_pair) {
                Expression t_expr = {"T."};
                return build_comp_impl(t_expr, 0, caller_info, state);
            }
        }
        // Detect A. x ( x = VAR -> body ) → body
        // when bound var doesn't appear in body (equsv equivalence).
        // Pattern: A. x ( x = VAR -> ... ) or A. x ( VAR = x -> ... )
        if (tok == "A." && pos + 4 < mm_tokens.size() &&
            mm_tokens[pos] == "(") {
            // Check for "( x = VAR ->" or "( VAR = x ->"
            bool fwd = (mm_tokens[pos + 1] == bound &&
                        mm_tokens[pos + 2] == "=" &&
                        mm_tokens[pos + 4] == "->");
            bool rev = (mm_tokens[pos + 2] == "=" &&
                        mm_tokens[pos + 3] == bound &&
                        mm_tokens[pos + 4] == "->");
            if ((fwd || rev) && pos + 5 < mm_tokens.size()) {
                // Extract body: tokens after "->" up to matching ")"
                size_t body_start = pos + 5;
                // Find matching close paren (skip the outer "(")
                int depth = 1;
                size_t body_end = body_start;
                for (size_t i = body_start; i < mm_tokens.size(); ++i) {
                    if (mm_tokens[i] == "(") ++depth;
                    else if (mm_tokens[i] == ")") {
                        --depth;
                        if (depth == 0) { body_end = i; break; }
                    }
                }
                // Check bound var doesn't appear in body
                bool bound_in_body = false;
                for (size_t i = body_start; i < body_end; ++i) {
                    if (mm_tokens[i] == bound) { bound_in_body = true; break; }
                }
                if (!bound_in_body) {
                    Expression body(mm_tokens.begin() + body_start,
                                    mm_tokens.begin() + body_end);
                    return build_comp_impl(body, 0, caller_info, state);
                }
            }
        }
        // Detect E. x ( x = VAR /\ body ) → body[x:=VAR]
        // exists x. (eq(x, y) & P(x)) <-> P(y) by substitution (eq(y,y) + eq_subst).
        // Handles both "x = VAR" and "VAR = x" orders.
        if (tok == "E." && pos + 3 < mm_tokens.size() &&
            mm_tokens[pos] == "(") {
            // Pattern: ( x = VAR /\ body ) or ( VAR = x /\ body )
            bool fwd = (pos + 4 < mm_tokens.size() &&
                        mm_tokens[pos + 1] == bound &&
                        mm_tokens[pos + 2] == "=" &&
                        mm_tokens[pos + 4] == "/\\");
            bool rev = (pos + 4 < mm_tokens.size() &&
                        mm_tokens[pos + 2] == "=" &&
                        mm_tokens[pos + 3] == bound &&
                        mm_tokens[pos + 4] == "/\\");
            if ((fwd || rev) && pos + 5 < mm_tokens.size()) {
                std::string target = fwd ? mm_tokens[pos + 3]
                                         : mm_tokens[pos + 1];
                size_t body_start = pos + 5;
                int depth = 1;
                size_t body_end = body_start;
                for (size_t i = body_start; i < mm_tokens.size(); ++i) {
                    if (mm_tokens[i] == "(") ++depth;
                    else if (mm_tokens[i] == ")") {
                        --depth;
                        if (depth == 0) { body_end = i; break; }
                    }
                }
                // Substitute bound→target in body tokens
                Expression body;
                for (size_t i = body_start; i < body_end; ++i) {
                    body.push_back(mm_tokens[i] == bound ? target : mm_tokens[i]);
                }
                return build_comp_impl(body, 0, caller_info, state);
            }
        }
        // Non-vacuous quantifier: if the bound variable appears in the body
        // tokens, the quantifier can't be stripped (comprehension axioms only
        // handle propositional connectives, not quantifiers over setvars).
        for (size_t i = pos; i < mm_tokens.size(); ++i) {
            if (mm_tokens[i] == bound) return {};
        }
        // Vacuous: skip quantifier (x doesn't appear in wff-as-set encoding)
        return build_comp_impl(mm_tokens, pos, caller_info, state);
    }
    if (tok == "F/") {
        pos++;
        if (pos >= mm_tokens.size()) return {};
        std::string setvar = mm_tokens[pos];
        pos++;
        // Desugar: F/ x body → ( E. x body -> A. x body )
        Expression body(mm_tokens.begin() + pos, mm_tokens.end());
        Expression desugared = {"("};
        desugared.push_back("E."); desugared.push_back(setvar);
        desugared.insert(desugared.end(), body.begin(), body.end());
        desugared.push_back("->");
        desugared.push_back("A."); desugared.push_back(setvar);
        desugared.insert(desugared.end(), body.begin(), body.end());
        desugared.push_back(")");
        return build_comp_impl(desugared, 0, caller_info, state);
    }

    // Negation: -. A
    if (tok == "-.") {
        auto inner = build_comp_impl(mm_tokens, pos + 1, caller_info, state);
        if (inner.set_var.empty()) return {};

        std::string h_ax = state.fresh();
        state.emit(h_ax + " = use wff_neg");
        std::string h1 = state.fresh();
        state.emit(h1 + " = forall_elim " + h_ax + ", " + inner.set_var);
        std::string witness = state.fresh() + "_w";
        std::string h2 = state.fresh();
        state.emit(h2 + " = iota_elim " + h1 + ", " + witness);
        std::string axiom_iff = state.fresh();
        state.emit(axiom_iff + " = forall_elim " + h2 + ", " +
                   caller_info.dummy_var);

        std::string compound = neg(inner.compound_str);

        if (inner.iff_handle.empty()) {
            return {witness, axiom_iff, compound};
        }

        // Chain: axiom says elem(u0,W) <-> ~elem(u0,inner.set)
        //        inner says elem(u0,inner.set) <-> inner.compound
        //        Want: elem(u0,W) <-> ~inner.compound
        std::string elem_w = "elem(" + caller_info.dummy_var + ", " +
                             witness + ")";
        std::string elem_inner = "elem(" + caller_info.dummy_var + ", " +
                                 inner.set_var + ")";

        // Forward: elem(u0,W) -> ~inner.compound
        std::string hf1 = state.fresh();
        state.emit(hf1 + " = assume " + elem_w);
        std::string hf2 = state.fresh();
        state.emit(hf2 + " = iff_elim_l " + axiom_iff + ", " + hf1);
        std::string hf3 = state.fresh();
        state.emit(hf3 + " = assume " + inner.compound_str);
        std::string hf4 = state.fresh();
        state.emit(hf4 + " = iff_elim_r " + inner.iff_handle + ", " + hf3);
        std::string hf5 = state.fresh();
        state.emit(hf5 + " = not_elim " + hf2 + ", " + hf4);
        std::string hf6 = state.fresh();
        state.emit(hf6 + " = not_intro " + hf5);
        std::string hf7 = state.fresh();
        state.emit(hf7 + " = implies_intro " + hf6);

        // Backward: ~inner.compound -> elem(u0,W)
        std::string hb1 = state.fresh();
        state.emit(hb1 + " = assume " + compound);
        std::string hb2 = state.fresh();
        state.emit(hb2 + " = assume " + elem_inner);
        std::string hb3 = state.fresh();
        state.emit(hb3 + " = iff_elim_l " + inner.iff_handle + ", " + hb2);
        std::string hb4 = state.fresh();
        state.emit(hb4 + " = not_elim " + hb1 + ", " + hb3);
        std::string hb5 = state.fresh();
        state.emit(hb5 + " = not_intro " + hb4);
        std::string hb6 = state.fresh();
        state.emit(hb6 + " = iff_elim_r " + axiom_iff + ", " + hb5);
        std::string hb7 = state.fresh();
        state.emit(hb7 + " = implies_intro " + hb6);

        std::string chained = state.fresh();
        state.emit(chained + " = iff_intro " + hf7 + ", " + hb7);
        return {witness, chained, compound};
    }

    // Function-style 3-ary: if-(A, B, C), cadd(A, B, C), hadd(A, B, C)
    if (tok == "if-" || tok == "cadd" || tok == "hadd") {
        // Desugar to binary ops and recurse.
        // Build a synthetic expression with the desugared form.
        // if-(A,B,C) = (A /\ B) \/ (-. A /\ C)
        // cadd(A,B,C) = (A /\ B) \/ (C /\ -. (A <-> B))
        // hadd(A,B,C) = -. (-. (A <-> B) <-> C)
        pos++; // skip keyword
        if (pos < mm_tokens.size() && mm_tokens[pos] == "(") pos++; // skip (
        // Find the three arguments by scanning for commas at depth 0
        size_t arg1_start = pos;
        int depth2 = 0;
        while (pos < mm_tokens.size()) {
            if (mm_tokens[pos] == "(") depth2++;
            else if (mm_tokens[pos] == ")") { if (depth2 == 0) break; depth2--; }
            else if (depth2 == 0 && mm_tokens[pos] == ",") break;
            pos++;
        }
        size_t arg1_end = pos;
        pos++; // skip ,
        size_t arg2_start = pos;
        depth2 = 0;
        while (pos < mm_tokens.size()) {
            if (mm_tokens[pos] == "(") depth2++;
            else if (mm_tokens[pos] == ")") { if (depth2 == 0) break; depth2--; }
            else if (depth2 == 0 && mm_tokens[pos] == ",") break;
            pos++;
        }
        size_t arg2_end = pos;
        pos++; // skip ,
        size_t arg3_start = pos;
        depth2 = 0;
        while (pos < mm_tokens.size()) {
            if (mm_tokens[pos] == "(") depth2++;
            else if (mm_tokens[pos] == ")") { if (depth2 == 0) break; depth2--; }
            pos++;
        }
        // pos is at closing )
        size_t arg3_end = pos;
        if (pos < mm_tokens.size()) pos++; // skip )

        // Build desugared expression and recurse
        Expression desugared;
        Expression arg1(mm_tokens.begin() + arg1_start, mm_tokens.begin() + arg1_end);
        Expression arg2(mm_tokens.begin() + arg2_start, mm_tokens.begin() + arg2_end);
        Expression arg3(mm_tokens.begin() + arg3_start, mm_tokens.begin() + arg3_end);

        if (tok == "if-") {
            // (A /\ B) \/ (-. A /\ C)
            desugared = {"(", "("}; desugared.insert(desugared.end(), arg1.begin(), arg1.end());
            desugared.push_back("/\\"); desugared.insert(desugared.end(), arg2.begin(), arg2.end());
            desugared.push_back(")"); desugared.push_back("\\/");
            desugared.push_back("("); desugared.push_back("-."); desugared.insert(desugared.end(), arg1.begin(), arg1.end());
            desugared.push_back("/\\"); desugared.insert(desugared.end(), arg3.begin(), arg3.end());
            desugared.push_back(")"); desugared.push_back(")");
        } else if (tok == "cadd") {
            // (A /\ B) \/ (C /\ -. (A <-> B))
            desugared = {"(", "("}; desugared.insert(desugared.end(), arg1.begin(), arg1.end());
            desugared.push_back("/\\"); desugared.insert(desugared.end(), arg2.begin(), arg2.end());
            desugared.push_back(")"); desugared.push_back("\\/");
            desugared.push_back("("); desugared.insert(desugared.end(), arg3.begin(), arg3.end());
            desugared.push_back("/\\"); desugared.push_back("-.");
            desugared.push_back("("); desugared.insert(desugared.end(), arg1.begin(), arg1.end());
            desugared.push_back("<->"); desugared.insert(desugared.end(), arg2.begin(), arg2.end());
            desugared.push_back(")");
            desugared.push_back(")"); desugared.push_back(")");
        } else { // hadd
            // -. (-. (A <-> B) <-> C)
            desugared = {"-.", "(", "-.", "("}; desugared.insert(desugared.end(), arg1.begin(), arg1.end());
            desugared.push_back("<->"); desugared.insert(desugared.end(), arg2.begin(), arg2.end());
            desugared.push_back(")"); desugared.push_back("<->");
            desugared.insert(desugared.end(), arg3.begin(), arg3.end());
            desugared.push_back(")");
        }
        return build_comp_impl(desugared, 0, caller_info, state);
    }

    // Binary/ternary: ( A OP B ) or ( A OP B OP C )
    if (tok == "(") {
        pos++;
        size_t lhs_start = pos;
        int depth = 0;
        while (pos < mm_tokens.size()) {
            if (mm_tokens[pos] == "(") depth++;
            else if (mm_tokens[pos] == ")") {
                if (depth == 0) break;
                depth--;
            } else if (depth == 0 &&
                       (mm_tokens[pos] == "->" || mm_tokens[pos] == "/\\" ||
                        mm_tokens[pos] == "\\/" || mm_tokens[pos] == "<->" ||
                        mm_tokens[pos] == "\\/_" || mm_tokens[pos] == "-/\\" ||
                        mm_tokens[pos] == "-\\/"))
                break;
            pos++;
        }
        if (pos >= mm_tokens.size()) return {};  // no operator found
        std::string op = mm_tokens[pos];
        size_t rhs_start = pos + 1;

        // Check for n-way: ( A /\ B /\ C /\ ... ) or ( A \/ B \/ ... )
        if (op == "/\\" || op == "\\/") {
            // Scan past rhs to find potential repeated operator at depth 0
            size_t rhs_scan = rhs_start;
            int d = 0;
            while (rhs_scan < mm_tokens.size()) {
                if (mm_tokens[rhs_scan] == "(") d++;
                else if (mm_tokens[rhs_scan] == ")") {
                    if (d == 0) break;
                    d--;
                } else if (d == 0 && mm_tokens[rhs_scan] == op) break;
                rhs_scan++;
            }
            if (rhs_scan < mm_tokens.size() && mm_tokens[rhs_scan] == op) {
                // N-way! Left-associate first two: ( ( A op B ) op rest... )
                Expression desugared = {"(", "("};
                desugared.insert(desugared.end(), mm_tokens.begin() + lhs_start,
                                 mm_tokens.begin() + pos); // A
                desugared.push_back(op);
                desugared.insert(desugared.end(), mm_tokens.begin() + rhs_start,
                                 mm_tokens.begin() + rhs_scan); // B
                desugared.push_back(")");
                desugared.push_back(op);
                // Rest goes from rhs_scan+1 to the closing )
                size_t rest_start = rhs_scan + 1;
                size_t rest_end = rest_start;
                d = 0;
                while (rest_end < mm_tokens.size()) {
                    if (mm_tokens[rest_end] == "(") d++;
                    else if (mm_tokens[rest_end] == ")") {
                        if (d == 0) break;
                        d--;
                    }
                    rest_end++;
                }
                desugared.insert(desugared.end(), mm_tokens.begin() + rest_start,
                                 mm_tokens.begin() + rest_end);
                desugared.push_back(")");
                // Recurse — will handle remaining repeated ops if any
                return build_comp_impl(desugared, 0, caller_info, state);
            }
        }

        // Desugar XOR/NAND/NOR to compound expressions
        if (op == "\\/_" || op == "-/\\" || op == "-\\/") {
            // Find RHS end (closing paren)
            size_t rhs_end = rhs_start;
            int d = 0;
            while (rhs_end < mm_tokens.size()) {
                if (mm_tokens[rhs_end] == "(") d++;
                else if (mm_tokens[rhs_end] == ")") {
                    if (d == 0) break;
                    d--;
                }
                rhs_end++;
            }
            Expression desugared;
            Expression lhs_toks(mm_tokens.begin() + lhs_start, mm_tokens.begin() + pos);
            Expression rhs_toks(mm_tokens.begin() + rhs_start, mm_tokens.begin() + rhs_end);
            if (op == "\\/_") {
                // ~(A <-> B)
                desugared = {"-.", "("}; desugared.insert(desugared.end(), lhs_toks.begin(), lhs_toks.end());
                desugared.push_back("<->"); desugared.insert(desugared.end(), rhs_toks.begin(), rhs_toks.end());
                desugared.push_back(")");
            } else if (op == "-/\\") {
                // ~(A /\ B)
                desugared = {"-.", "("}; desugared.insert(desugared.end(), lhs_toks.begin(), lhs_toks.end());
                desugared.push_back("/\\"); desugared.insert(desugared.end(), rhs_toks.begin(), rhs_toks.end());
                desugared.push_back(")");
            } else { // -\/
                // ~(A \/ B)
                desugared = {"-.", "("}; desugared.insert(desugared.end(), lhs_toks.begin(), lhs_toks.end());
                desugared.push_back("\\/"); desugared.insert(desugared.end(), rhs_toks.begin(), rhs_toks.end());
                desugared.push_back(")");
            }
            return build_comp_impl(desugared, 0, caller_info, state);
        }

        // Extract sub-expressions so recursive calls see correct boundaries
        // (e.g., "x e. A" pattern uses mm_tokens.size() to verify it's the
        // full expression — passing the full vector would break that check).
        size_t rhs_end = rhs_start;
        {
            int d = 0;
            while (rhs_end < mm_tokens.size()) {
                if (mm_tokens[rhs_end] == "(") d++;
                else if (mm_tokens[rhs_end] == ")") {
                    if (d == 0) break;
                    d--;
                }
                rhs_end++;
            }
        }
        Expression lhs_toks(mm_tokens.begin() + lhs_start,
                            mm_tokens.begin() + pos);
        Expression rhs_toks(mm_tokens.begin() + rhs_start,
                            mm_tokens.begin() + rhs_end);
        auto lhs = build_comp_impl(lhs_toks, 0, caller_info, state);
        auto rhs = build_comp_impl(rhs_toks, 0, caller_info, state);
        if (lhs.set_var.empty() || rhs.set_var.empty()) return {};

        std::string axiom, fol_op;
        if (op == "->") { axiom = "wff_impl"; fol_op = " -> "; }
        else if (op == "/\\") { axiom = "wff_and"; fol_op = " & "; }
        else if (op == "\\/") { axiom = "wff_or"; fol_op = " | "; }
        else if (op == "<->") { axiom = "wff_bic"; fol_op = " <-> "; }
        else return {};

        std::string h_ax = state.fresh();
        state.emit(h_ax + " = use " + axiom);
        std::string h1 = state.fresh();
        state.emit(h1 + " = forall_elim " + h_ax + ", " + lhs.set_var);
        std::string h2 = state.fresh();
        state.emit(h2 + " = forall_elim " + h1 + ", " + rhs.set_var);
        std::string witness = state.fresh() + "_w";
        std::string h3 = state.fresh();
        state.emit(h3 + " = iota_elim " + h2 + ", " + witness);
        std::string axiom_iff = state.fresh();
        state.emit(axiom_iff + " = forall_elim " + h3 + ", " +
                   caller_info.dummy_var);

        std::string compound = "(" + lhs.compound_str + fol_op +
                               rhs.compound_str + ")";

        if (lhs.iff_handle.empty() && rhs.iff_handle.empty()) {
            return {witness, axiom_iff, compound};
        }

        // Need to chain: axiom iff + child iffs → fully expanded iff
        std::string elem_w = "elem(" + caller_info.dummy_var + ", " +
                             witness + ")";
        std::string elem_lhs = "elem(" + caller_info.dummy_var + ", " +
                               lhs.set_var + ")";
        std::string elem_rhs = "elem(" + caller_info.dummy_var + ", " +
                               rhs.set_var + ")";

        // Helpers: convert between elem-form and compound-form using child iffs
        auto lhs_fwd = [&](const std::string& h) -> std::string {
            if (lhs.iff_handle.empty()) return h;
            std::string r = state.fresh();
            state.emit(r + " = iff_elim_l " + lhs.iff_handle + ", " + h);
            return r;
        };
        auto lhs_bwd = [&](const std::string& h) -> std::string {
            if (lhs.iff_handle.empty()) return h;
            std::string r = state.fresh();
            state.emit(r + " = iff_elim_r " + lhs.iff_handle + ", " + h);
            return r;
        };
        auto rhs_fwd = [&](const std::string& h) -> std::string {
            if (rhs.iff_handle.empty()) return h;
            std::string r = state.fresh();
            state.emit(r + " = iff_elim_l " + rhs.iff_handle + ", " + h);
            return r;
        };
        auto rhs_bwd = [&](const std::string& h) -> std::string {
            if (rhs.iff_handle.empty()) return h;
            std::string r = state.fresh();
            state.emit(r + " = iff_elim_r " + rhs.iff_handle + ", " + h);
            return r;
        };

        if (op == "->") {
            // Forward: elem(u0,W) -> (lhs.compound -> rhs.compound)
            std::string hf1 = state.fresh();
            state.emit(hf1 + " = assume " + elem_w);
            std::string hf2 = state.fresh();
            state.emit(hf2 + " = iff_elim_l " + axiom_iff + ", " + hf1);
            std::string hf3 = state.fresh();
            state.emit(hf3 + " = assume " + lhs.compound_str);
            std::string hf4 = lhs_bwd(hf3);
            std::string hf5 = state.fresh();
            state.emit(hf5 + " = implies_elim " + hf2 + ", " + hf4);
            std::string hf6 = rhs_fwd(hf5);
            std::string hf7 = state.fresh();
            state.emit(hf7 + " = implies_intro " + hf6);
            std::string hf8 = state.fresh();
            state.emit(hf8 + " = implies_intro " + hf7);

            // Backward: (lhs.compound -> rhs.compound) -> elem(u0,W)
            std::string hb1 = state.fresh();
            state.emit(hb1 + " = assume " + compound);
            std::string hb2 = state.fresh();
            state.emit(hb2 + " = assume " + elem_lhs);
            std::string hb3 = lhs_fwd(hb2);
            std::string hb4 = state.fresh();
            state.emit(hb4 + " = implies_elim " + hb1 + ", " + hb3);
            std::string hb5 = rhs_bwd(hb4);
            std::string hb6 = state.fresh();
            state.emit(hb6 + " = implies_intro " + hb5);
            std::string hb7 = state.fresh();
            state.emit(hb7 + " = iff_elim_r " + axiom_iff + ", " + hb6);
            std::string hb8 = state.fresh();
            state.emit(hb8 + " = implies_intro " + hb7);

            std::string chained = state.fresh();
            state.emit(chained + " = iff_intro " + hf8 + ", " + hb8);
            return {witness, chained, compound};
        }

        if (op == "/\\") {
            // Forward: elem(u0,W) -> (lhs.compound & rhs.compound)
            std::string hf1 = state.fresh();
            state.emit(hf1 + " = assume " + elem_w);
            std::string hf2 = state.fresh();
            state.emit(hf2 + " = iff_elim_l " + axiom_iff + ", " + hf1);
            std::string hf3 = state.fresh();
            state.emit(hf3 + " = and_elim_l " + hf2);
            std::string hf4 = lhs_fwd(hf3);
            std::string hf5 = state.fresh();
            state.emit(hf5 + " = and_elim_r " + hf2);
            std::string hf6 = rhs_fwd(hf5);
            std::string hf7 = state.fresh();
            state.emit(hf7 + " = and_intro " + hf4 + ", " + hf6);
            std::string hf8 = state.fresh();
            state.emit(hf8 + " = implies_intro " + hf7);

            // Backward: (lhs.compound & rhs.compound) -> elem(u0,W)
            std::string hb1 = state.fresh();
            state.emit(hb1 + " = assume " + compound);
            std::string hb2 = state.fresh();
            state.emit(hb2 + " = and_elim_l " + hb1);
            std::string hb3 = lhs_bwd(hb2);
            std::string hb4 = state.fresh();
            state.emit(hb4 + " = and_elim_r " + hb1);
            std::string hb5 = rhs_bwd(hb4);
            std::string hb6 = state.fresh();
            state.emit(hb6 + " = and_intro " + hb3 + ", " + hb5);
            std::string hb7 = state.fresh();
            state.emit(hb7 + " = iff_elim_r " + axiom_iff + ", " + hb6);
            std::string hb8 = state.fresh();
            state.emit(hb8 + " = implies_intro " + hb7);

            std::string chained = state.fresh();
            state.emit(chained + " = iff_intro " + hf8 + ", " + hb8);
            return {witness, chained, compound};
        }

        if (op == "\\/") {
            // Forward: elem(u0,W) -> (lhs.compound | rhs.compound)
            std::string hf1 = state.fresh();
            state.emit(hf1 + " = assume " + elem_w);
            std::string hf2 = state.fresh();
            state.emit(hf2 + " = iff_elim_l " + axiom_iff + ", " + hf1);
            // Case left
            std::string hf3 = state.fresh();
            state.emit(hf3 + " = assume " + elem_lhs);
            std::string hf4 = lhs_fwd(hf3);
            std::string hf5l = state.fresh();
            state.emit(hf5l + " = let " + rhs.compound_str);
            std::string hf5 = state.fresh();
            state.emit(hf5 + " = or_intro_l " + hf4 + ", " + hf5l);
            std::string hf6 = state.fresh();
            state.emit(hf6 + " = implies_intro " + hf5);
            // Case right
            std::string hf7 = state.fresh();
            state.emit(hf7 + " = assume " + elem_rhs);
            std::string hf8 = rhs_fwd(hf7);
            std::string hf9l = state.fresh();
            state.emit(hf9l + " = let " + lhs.compound_str);
            std::string hf9 = state.fresh();
            state.emit(hf9 + " = or_intro_r " + hf9l + ", " + hf8);
            std::string hf10 = state.fresh();
            state.emit(hf10 + " = implies_intro " + hf9);
            // or_elim
            std::string hf11 = state.fresh();
            state.emit(hf11 + " = or_elim " + hf2 + ", " + hf6 + ", " +
                       hf10);
            std::string hf12 = state.fresh();
            state.emit(hf12 + " = implies_intro " + hf11);

            // Backward: (lhs.compound | rhs.compound) -> elem(u0,W)
            std::string hb1 = state.fresh();
            state.emit(hb1 + " = assume " + compound);
            // Case left
            std::string hb2 = state.fresh();
            state.emit(hb2 + " = assume " + lhs.compound_str);
            std::string hb3 = lhs_bwd(hb2);
            std::string hb4l = state.fresh();
            state.emit(hb4l + " = let " + elem_rhs);
            std::string hb4 = state.fresh();
            state.emit(hb4 + " = or_intro_l " + hb3 + ", " + hb4l);
            std::string hb5 = state.fresh();
            state.emit(hb5 + " = implies_intro " + hb4);
            // Case right
            std::string hb6 = state.fresh();
            state.emit(hb6 + " = assume " + rhs.compound_str);
            std::string hb7 = rhs_bwd(hb6);
            std::string hb8l = state.fresh();
            state.emit(hb8l + " = let " + elem_lhs);
            std::string hb8 = state.fresh();
            state.emit(hb8 + " = or_intro_r " + hb8l + ", " + hb7);
            std::string hb9 = state.fresh();
            state.emit(hb9 + " = implies_intro " + hb8);
            // or_elim
            std::string hb10 = state.fresh();
            state.emit(hb10 + " = or_elim " + hb1 + ", " + hb5 + ", " +
                       hb9);
            std::string hb11 = state.fresh();
            state.emit(hb11 + " = iff_elim_r " + axiom_iff + ", " + hb10);
            std::string hb12 = state.fresh();
            state.emit(hb12 + " = implies_intro " + hb11);

            std::string chained = state.fresh();
            state.emit(chained + " = iff_intro " + hf12 + ", " + hb12);
            return {witness, chained, compound};
        }

        if (op == "<->") {
            // Forward: elem(u0,W) -> (lhs.compound <-> rhs.compound)
            std::string hf1 = state.fresh();
            state.emit(hf1 + " = assume " + elem_w);
            std::string hf2 = state.fresh();
            state.emit(hf2 + " = iff_elim_l " + axiom_iff + ", " + hf1);
            // fwd dir: lhs.compound -> rhs.compound
            std::string hf3 = state.fresh();
            state.emit(hf3 + " = assume " + lhs.compound_str);
            std::string hf4 = lhs_bwd(hf3);
            std::string hf5 = state.fresh();
            state.emit(hf5 + " = iff_elim_l " + hf2 + ", " + hf4);
            std::string hf6 = rhs_fwd(hf5);
            std::string hf7 = state.fresh();
            state.emit(hf7 + " = implies_intro " + hf6);
            // bwd dir: rhs.compound -> lhs.compound
            std::string hf8 = state.fresh();
            state.emit(hf8 + " = assume " + rhs.compound_str);
            std::string hf9 = rhs_bwd(hf8);
            std::string hf10 = state.fresh();
            state.emit(hf10 + " = iff_elim_r " + hf2 + ", " + hf9);
            std::string hf11 = lhs_fwd(hf10);
            std::string hf12 = state.fresh();
            state.emit(hf12 + " = implies_intro " + hf11);
            // combine
            std::string hf13 = state.fresh();
            state.emit(hf13 + " = iff_intro " + hf7 + ", " + hf12);
            std::string hf14 = state.fresh();
            state.emit(hf14 + " = implies_intro " + hf13);

            // Backward: (lhs.compound <-> rhs.compound) -> elem(u0,W)
            std::string hb1 = state.fresh();
            state.emit(hb1 + " = assume " + compound);
            // fwd dir: elem_lhs -> elem_rhs
            std::string hb2 = state.fresh();
            state.emit(hb2 + " = assume " + elem_lhs);
            std::string hb3 = lhs_fwd(hb2);
            std::string hb4 = state.fresh();
            state.emit(hb4 + " = iff_elim_l " + hb1 + ", " + hb3);
            std::string hb5 = rhs_bwd(hb4);
            std::string hb6 = state.fresh();
            state.emit(hb6 + " = implies_intro " + hb5);
            // bwd dir: elem_rhs -> elem_lhs
            std::string hb7 = state.fresh();
            state.emit(hb7 + " = assume " + elem_rhs);
            std::string hb8 = rhs_fwd(hb7);
            std::string hb9 = state.fresh();
            state.emit(hb9 + " = iff_elim_r " + hb1 + ", " + hb8);
            std::string hb10 = lhs_bwd(hb9);
            std::string hb11 = state.fresh();
            state.emit(hb11 + " = implies_intro " + hb10);
            // combine
            std::string hb12 = state.fresh();
            state.emit(hb12 + " = iff_intro " + hb6 + ", " + hb11);
            std::string hb13 = state.fresh();
            state.emit(hb13 + " = iff_elim_r " + axiom_iff + ", " + hb12);
            std::string hb14 = state.fresh();
            state.emit(hb14 + " = implies_intro " + hb13);

            std::string chained = state.fresh();
            state.emit(chained + " = iff_intro " + hf14 + ", " + hb14);
            return {witness, chained, compound};
        }

        return {};
    }

    // Membership pattern: x e. A (setvar membership in set/class)
    // Produces a witness set C where elem(u, C) <-> elem(x, A).
    // The variable x may come from a referenced theorem's frame (not
    // the caller's mandatory frame), so we detect the pattern by the
    // "e." token rather than requiring x to be in caller_info.setvars.
    if (pos + 2 < mm_tokens.size() && mm_tokens[pos + 1] == "e." &&
        pos + 2 == mm_tokens.size() - 1) {
        const std::string& sv_x = tok;
        const std::string& sv_A = mm_tokens[pos + 2];
        // Both variables must be fixed setvars in the current proof scope.
        bool x_ok = (sv_x == caller_info.dummy_var ||
                     std::find(caller_info.setvars.begin(),
                               caller_info.setvars.end(), sv_x) !=
                         caller_info.setvars.end());
        bool a_ok = (std::find(caller_info.setvars.begin(),
                               caller_info.setvars.end(), sv_A) !=
                         caller_info.setvars.end());
        if (!x_ok || !a_ok) return {};
        std::string compound = "elem(" + sv_x + ", " + sv_A + ")";

        // x is the dummy var: elem(u0, A) — witness is A directly
        if (sv_x == caller_info.dummy_var) {
            return {sv_A, "", compound};
        }

        // General case: use wff_elem axiom
        std::string h_ax = state.fresh();
        state.emit(h_ax + " = use wff_elem");
        std::string h1 = state.fresh();
        state.emit(h1 + " = forall_elim " + h_ax + ", " + sv_x);
        std::string h2 = state.fresh();
        state.emit(h2 + " = forall_elim " + h1 + ", " + sv_A);
        std::string witness = state.fresh() + "_w";
        std::string h3 = state.fresh();
        state.emit(h3 + " = iota_elim " + h2 + ", " + witness);
        std::string h_iff = state.fresh();
        state.emit(h_iff + " = forall_elim " + h3 + ", " +
                   caller_info.dummy_var);
        return {witness, h_iff, compound};
    }

    // Equality pattern: x = y (setvar equality)
    // Produces a witness set C where elem(u, C) <-> eq(x, y).
    if (pos + 2 < mm_tokens.size() && mm_tokens[pos + 1] == "=" &&
        pos + 2 == mm_tokens.size() - 1) {
        const std::string& sv_x = tok;
        const std::string& sv_y = mm_tokens[pos + 2];
        bool x_ok = (sv_x == caller_info.dummy_var ||
                     std::find(caller_info.setvars.begin(),
                               caller_info.setvars.end(), sv_x) !=
                         caller_info.setvars.end());
        bool y_ok = (sv_y == caller_info.dummy_var ||
                     std::find(caller_info.setvars.begin(),
                               caller_info.setvars.end(), sv_y) !=
                         caller_info.setvars.end());
        if (!x_ok || !y_ok) return {};
        std::string compound = "eq(" + sv_x + ", " + sv_y + ")";

        std::string h_ax = state.fresh();
        state.emit(h_ax + " = use wff_eq");
        std::string h1 = state.fresh();
        state.emit(h1 + " = forall_elim " + h_ax + ", " + sv_x);
        std::string h2 = state.fresh();
        state.emit(h2 + " = forall_elim " + h1 + ", " + sv_y);
        std::string witness = state.fresh() + "_w";
        std::string h3 = state.fresh();
        state.emit(h3 + " = iota_elim " + h2 + ", " + witness);
        std::string h_iff = state.fresh();
        state.emit(h_iff + " = forall_elim " + h3 + ", " +
                   caller_info.dummy_var);
        return {witness, h_iff, compound};
    }

    // Inequality pattern: x =/= y (setvar not-equal)
    // Produces a witness set C where elem(u, C) <-> ne(x, y).
    if (pos + 2 < mm_tokens.size() && mm_tokens[pos + 1] == "=/=" &&
        pos + 2 == mm_tokens.size() - 1) {
        const std::string& sv_x = tok;
        const std::string& sv_y = mm_tokens[pos + 2];
        bool x_ok = (sv_x == caller_info.dummy_var ||
                     std::find(caller_info.setvars.begin(),
                               caller_info.setvars.end(), sv_x) !=
                         caller_info.setvars.end());
        bool y_ok = (sv_y == caller_info.dummy_var ||
                     std::find(caller_info.setvars.begin(),
                               caller_info.setvars.end(), sv_y) !=
                         caller_info.setvars.end());
        if (!x_ok || !y_ok) return {};
        std::string compound = "ne(" + sv_x + ", " + sv_y + ")";

        std::string h_ax = state.fresh();
        state.emit(h_ax + " = use wff_ne");
        std::string h1 = state.fresh();
        state.emit(h1 + " = forall_elim " + h_ax + ", " + sv_x);
        std::string h2 = state.fresh();
        state.emit(h2 + " = forall_elim " + h1 + ", " + sv_y);
        std::string witness = state.fresh() + "_w";
        std::string h3 = state.fresh();
        state.emit(h3 + " = iota_elim " + h2 + ", " + witness);
        std::string h_iff = state.fresh();
        state.emit(h_iff + " = forall_elim " + h3 + ", " +
                   caller_info.dummy_var);
        return {witness, h_iff, compound};
    }

    // Not-element pattern: x e/ y (setvar not-element)
    // Produces a witness set C where elem(u, C) <-> nel(x, y).
    if (pos + 2 < mm_tokens.size() && mm_tokens[pos + 1] == "e/" &&
        pos + 2 == mm_tokens.size() - 1) {
        const std::string& sv_x = tok;
        const std::string& sv_y = mm_tokens[pos + 2];
        bool x_ok = (sv_x == caller_info.dummy_var ||
                     std::find(caller_info.setvars.begin(),
                               caller_info.setvars.end(), sv_x) !=
                         caller_info.setvars.end());
        bool y_ok = (sv_y == caller_info.dummy_var ||
                     std::find(caller_info.setvars.begin(),
                               caller_info.setvars.end(), sv_y) !=
                         caller_info.setvars.end());
        if (!x_ok || !y_ok) return {};
        std::string compound = "nel(" + sv_x + ", " + sv_y + ")";

        std::string h_ax = state.fresh();
        state.emit(h_ax + " = use wff_nel");
        std::string h1 = state.fresh();
        state.emit(h1 + " = forall_elim " + h_ax + ", " + sv_x);
        std::string h2 = state.fresh();
        state.emit(h2 + " = forall_elim " + h1 + ", " + sv_y);
        std::string witness = state.fresh() + "_w";
        std::string h3 = state.fresh();
        state.emit(h3 + " = iota_elim " + h2 + ", " + witness);
        std::string h_iff = state.fresh();
        state.emit(h_iff + " = forall_elim " + h3 + ", " +
                   caller_info.dummy_var);
        return {witness, h_iff, compound};
    }

    // Setvar: no comprehension needed, use directly.
    // Only match when the entire remaining expression is a single token.
    if (pos + 1 >= mm_tokens.size() &&
        std::find(caller_info.setvars.begin(), caller_info.setvars.end(),
                  tok) != caller_info.setvars.end()) {
        return {tok, "", tok};
    }

    // Unknown token (wff variable not in caller's frame, quantifier, etc.)
    return {};
}

}  // namespace metamath
