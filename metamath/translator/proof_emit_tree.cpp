#include "mm_translator.h"
#include "proof_emit.h"
#include "proof_tree.h"
#include "wff_ast.h"

#include <algorithm>

namespace metamath {

// ===================================================================
// Tree-based proof emission
// ===================================================================

std::string MmTranslator::emit_proof_tree(const ProofTree& tree,
                                           const FrameInfo& thm_info,
                                           ProofState& state,
                                           std::string* error) {
    std::unordered_map<size_t, std::string> handles;
    return emit_node(tree.root, tree, thm_info, handles, state, error);
}

std::string MmTranslator::emit_node(
    size_t idx, const ProofTree& tree,
    const FrameInfo& thm_info,
    std::unordered_map<size_t, std::string>& handles,
    ProofState& state, std::string* error) {

    // DAG reuse: if already emitted, return existing handle
    auto it = handles.find(idx);
    if (it != handles.end()) return it->second;

    const ProofNode& node = tree.nodes[idx];

    // Essential hypothesis: return pre-assigned handle
    if (node.is_ess_hyp) {
        std::string h = "hyp_" + sanitize_label(node.label);
        handles[idx] = h;
        return h;
    }

    // Syntax-only nodes should never be asked to emit
    if (node.is_syntax_only) {
        if (error) *error = "emit_node on syntax node: " + node.label;
        return "";
    }

    // Recursively emit children (essential hyps of the referenced assertion)
    std::vector<std::string> child_handles;
    for (size_t ci : node.children) {
        std::string ch = emit_node(ci, tree, thm_info, handles, state, error);
        if (ch.empty()) return "";
        child_handles.push_back(ch);
    }

    const auto& label = node.label;
    const auto& subst = node.subst;
    std::string h;

    // =================================================================
    // ax-mp: modus ponens
    // =================================================================
    if (label == "ax-mp") {
        if (child_handles.size() < 2) {
            if (error) *error = "ax-mp: need 2 essential hyps";
            return "";
        }
        h = state.fresh();
        state.emit(h + " = implies_elim " + child_handles[1] + ", " +
                   child_handles[0]);
        handles[idx] = h;
        return h;
    }

    // =================================================================
    // ax-1, ax-2, ax-3: propositional axioms (inline ND proofs)
    // =================================================================
    if (label == "ax-1") {
        auto ph = subst.find("ph"), ps = subst.find("ps");
        if (ph == subst.end() || ps == subst.end()) {
            if (error) *error = "ax-1: missing substitution";
            return "";
        }
        h = inline_ax1(translate_expr(ph->second, 0, thm_info),
                        translate_expr(ps->second, 0, thm_info), state);
        handles[idx] = h;
        return h;
    }
    if (label == "ax-2") {
        auto ph = subst.find("ph"), ps = subst.find("ps"),
             ch_it = subst.find("ch");
        if (ph == subst.end() || ps == subst.end() || ch_it == subst.end()) {
            if (error) *error = "ax-2: missing substitution";
            return "";
        }
        h = inline_ax2(translate_expr(ph->second, 0, thm_info),
                        translate_expr(ps->second, 0, thm_info),
                        translate_expr(ch_it->second, 0, thm_info), state);
        handles[idx] = h;
        return h;
    }
    if (label == "ax-3") {
        auto ph = subst.find("ph"), ps = subst.find("ps");
        if (ph == subst.end() || ps == subst.end()) {
            if (error) *error = "ax-3: missing substitution";
            return "";
        }
        h = inline_ax3(translate_expr(ph->second, 0, thm_info),
                        translate_expr(ps->second, 0, thm_info), state);
        handles[idx] = h;
        return h;
    }

    // =================================================================
    // df-bi, df-an, df-or: primitive definition axioms
    // =================================================================
    if (label == "df-bi" || label == "df-an" || label == "df-or") {
        auto ph = subst.find("ph"), ps = subst.find("ps");
        if (ph == subst.end() || ps == subst.end()) {
            if (error) *error = label + ": missing substitution";
            return "";
        }
        std::string a = translate_expr(ph->second, 0, thm_info);
        std::string b = translate_expr(ps->second, 0, thm_info);
        if (label == "df-bi") h = inline_df_bi(a, b, state);
        else if (label == "df-an") h = inline_df_an(a, b, state);
        else h = inline_df_or(a, b, state);
        handles[idx] = h;
        return h;
    }

    // =================================================================
    // Identity biconditionals: both sides same WffPtr
    // =================================================================
    if (wff_identity_defs_.count(label)) {
        WffPtr wff = parse_mm_wff(node.result_expr, 1, thm_info);
        if (!wff) {
            if (error) *error = "identity def parse failed: " + label;
            return "";
        }

        auto renderer = make_claim_renderer(thm_info);

        // Case 1: direct top-level biconditional (df-clab, df-3an, etc.)
        // Use LHS: it comes from SyntaxToWff class expansion (fresh bound
        // vars like zz0), matching how the rest of the proof renders formulas.
        // RHS retains original Metamath bound var names which may conflict.
        if (wff->kind == WffNode::Kind::Binary &&
            wff->op == WffNode::Op::Iff) {
            std::string formula = emit_fol(*wff->left, renderer);
            h = emit_identity_bic(formula, state);
            handles[idx] = h;
            return h;
        }

        // Case 2: forall-wrapped biconditional from class equality
        // expansion (df-rab, df-in, df-un, etc.)
        // expand_class_eq produces: forall zz0. (mem(zz0,LHS) <-> ...)
        {
            const WffNode* inner = wff.get();
            std::vector<std::string> bound_vars;
            while (inner && inner->kind == WffNode::Kind::Forall) {
                bound_vars.push_back(inner->name);
                inner = inner->left.get();
            }
            if (!bound_vars.empty() && inner &&
                inner->kind == WffNode::Kind::Binary &&
                inner->op == WffNode::Op::Iff) {
                // Fix all bound variables (fresh zz* names from SyntaxToWff)
                for (const auto& bv : bound_vars) {
                    state.emit("fix " + bv);
                }
                // Use LHS: consistent with direct case (Case 1).
                // LHS uses fresh SyntaxToWff bound vars (zz0, etc.),
                // matching how the rest of the proof renders formulas.
                std::string body_formula = emit_fol(*inner->left, renderer);
                h = emit_identity_bic(body_formula, state);
                // Close forall scopes (innermost first)
                for (int i = static_cast<int>(bound_vars.size()) - 1;
                     i >= 0; --i) {
                    std::string h_next = state.fresh();
                    state.emit(h_next + " = forall_intro " + h);
                    h = h_next;
                }
                handles[idx] = h;
                return h;
            }
        }

        if (error) *error = "identity def parse failed: " + label;
        return "";
    }

    // =================================================================
    // ax-5: Vacuous quantification (ph -> A.x ph = ph -> ph)
    // =================================================================
    if (label == "ax-5") {
        auto ph = subst.find("ph");
        if (ph == subst.end()) {
            if (error) *error = "ax-5: missing ph";
            return "";
        }
        std::string phi_fol = translate_expr(ph->second, 0, thm_info);
        std::string h_a = state.fresh();
        state.emit(h_a + " = assume " + phi_fol);
        h = state.fresh();
        state.emit(h + " = implies_intro " + h_a);
        handles[idx] = h;
        return h;
    }

    // =================================================================
    // ax-gen: Generalization
    // =================================================================
    if (label == "ax-gen") {
        if (child_handles.empty() || child_handles[0].empty()) {
            if (error) *error = "ax-gen: missing essential hyp";
            return "";
        }
        auto x = subst.find("x");
        auto ph = subst.find("ph");
        if (ph == subst.end()) {
            if (error) *error = "ax-gen: missing ph";
            return "";
        }
        bool is_setvar = x != subst.end() && !x->second.empty() &&
            std::find(thm_info.setvars.begin(), thm_info.setvars.end(),
                      x->second[0]) != thm_info.setvars.end();
        if (is_setvar) {
            // Check if the body actually uses x as a free term.
            // If not, A.x ph is stripped to ph (vacuous quantification).
            WffPtr phi_wff = parse_mm_wff(ph->second, 0, thm_info);
            bool body_uses_x = phi_wff &&
                has_free_term_var(*phi_wff, x->second[0]);
            if (!body_uses_x) {
                // Vacuous: result = essential hyp
                h = child_handles[0];
                handles[idx] = h;
                return h;
            }
            // Fall through: non-vacuous setvar quantification
        }
        // fix, transport, forall_intro
        std::string phi_fol = translate_expr(ph->second, 0, thm_info);
        std::string gen_var = "_genv" + std::to_string(++state.counter);
        state.emit("fix " + gen_var);
        std::string h_a = state.fresh();
        state.emit(h_a + " = assume " + phi_fol);
        std::string h_id = state.fresh();
        state.emit(h_id + " = implies_intro " + h_a);
        std::string h_local = state.fresh();
        state.emit(h_local + " = implies_elim " + h_id + ", " + child_handles[0]);
        h = state.fresh();
        state.emit(h + " = forall_intro " + h_local);
        handles[idx] = h;
        return h;
    }

    // =================================================================
    // ax-4: Quantifier distribution (identity with setvar stripping)
    // =================================================================
    if (label == "ax-4") {
        auto ph = subst.find("ph"), ps = subst.find("ps");
        if (ph == subst.end() || ps == subst.end()) {
            if (error) *error = "ax-4: missing substitution";
            return "";
        }
        std::string phi = translate_expr(ph->second, 0, thm_info);
        std::string psi = translate_expr(ps->second, 0, thm_info);
        std::string formula = "(" + phi + " -> " + psi + ")";
        std::string h_a = state.fresh();
        state.emit(h_a + " = assume " + formula);
        h = state.fresh();
        state.emit(h + " = implies_intro " + h_a);
        handles[idx] = h;
        return h;
    }

    // =================================================================
    // Predicate axiom bridges: ax-6 through ax-9
    // =================================================================
    if (label == "ax-6") {
        auto y = subst.find("y");
        if (y == subst.end() || y->second.empty()) {
            if (error) *error = "ax-6: missing y";
            return "";
        }
        h = emit_bridge_use("ax_6", {y->second[0]}, state);
        handles[idx] = h;
        return h;
    }
    if (label == "ax-7") {
        auto x = subst.find("x"), y = subst.find("y"), z = subst.find("z");
        if (x == subst.end() || y == subst.end() || z == subst.end()) {
            if (error) *error = "ax-7: missing x/y/z";
            return "";
        }
        h = emit_bridge_use("ax_7",
            {x->second[0], y->second[0], z->second[0]}, state);
        handles[idx] = h;
        return h;
    }
    if (label == "ax-8") {
        auto x = subst.find("x"), y = subst.find("y"), z = subst.find("z");
        if (x == subst.end() || y == subst.end() || z == subst.end()) {
            if (error) *error = "ax-8: missing x/y/z";
            return "";
        }
        h = emit_bridge_use("ax_8",
            {x->second[0], y->second[0], z->second[0]}, state);
        handles[idx] = h;
        return h;
    }
    if (label == "ax-9") {
        auto x = subst.find("x"), y = subst.find("y"), z = subst.find("z");
        if (x == subst.end() || y == subst.end() || z == subst.end()) {
            if (error) *error = "ax-9: missing x/y/z";
            return "";
        }
        h = emit_bridge_use("ax_9",
            {x->second[0], y->second[0], z->second[0]}, state);
        handles[idx] = h;
        return h;
    }

    // =================================================================
    // ax-10: Quantifier negation bridge (needs wff set)
    // =================================================================
    if (label == "ax-10") {
        auto ph = subst.find("ph");
        if (ph == subst.end()) {
            if (error) *error = "ax-10: missing ph";
            return "";
        }
        std::string S_ph = get_wff_set(ph->second, thm_info, state);
        if (S_ph.empty()) {
            if (error) *error = "ax-10: cannot resolve wff to set";
            return "";
        }
        h = emit_bridge_use("ax_10", {S_ph}, state);
        handles[idx] = h;
        return h;
    }

    // =================================================================
    // ax-11: Quantifier commutation (unsupported)
    // =================================================================
    if (label == "ax-11") {
        if (error) *error = "unsupported: ax-11";
        return "";
    }

    // =================================================================
    // ax-ext: Extensionality bridge
    // =================================================================
    if (label == "ax-ext") {
        auto x = subst.find("x"), y = subst.find("y");
        if (x == subst.end() || y == subst.end()) {
            if (error) *error = "ax-ext: missing x/y";
            return "";
        }
        h = emit_bridge_use("ax_ext", {x->second[0], y->second[0]}, state);
        handles[idx] = h;
        return h;
    }

    // =================================================================
    // df-cleq: Class equality bridge
    // =================================================================
    if (label == "df-cleq") {
        auto a = subst.find("A"), b = subst.find("B");
        if (a == subst.end() || b == subst.end()) {
            if (error) *error = "df-cleq: missing A/B";
            return "";
        }
        if (a->second.size() != 1 || b->second.size() != 1) {
            if (error) *error = "df-cleq: compound class expression";
            return "";
        }
        h = emit_bridge_use("axextb_bridge",
                             {a->second[0], b->second[0]}, state);
        handles[idx] = h;
        return h;
    }

    // =================================================================
    // df-clel: Class membership bridge
    // =================================================================
    if (label == "df-clel") {
        auto a = subst.find("A"), b = subst.find("B");
        if (a == subst.end() || b == subst.end()) {
            if (error) *error = "df-clel: missing A/B";
            return "";
        }
        if (a->second.size() != 1 || b->second.size() != 1) {
            if (error) *error = "df-clel: compound class expression";
            return "";
        }
        h = emit_bridge_use("df_clel",
                             {a->second[0], b->second[0]}, state);
        handles[idx] = h;
        return h;
    }

    // =================================================================
    // df-ex: Existential definition bridge
    // =================================================================
    if (label == "df-ex") {
        auto ph = subst.find("ph");
        if (ph == subst.end()) {
            if (error) *error = "df-ex: missing ph";
            return "";
        }
        std::string S_ph = get_wff_set(ph->second, thm_info, state);
        if (S_ph.empty()) {
            if (error) *error = "df-ex: cannot resolve wff to set";
            return "";
        }
        h = emit_bridge_use("df_ex", {S_ph, thm_info.dummy_var}, state);
        handles[idx] = h;
        return h;
    }

    // =================================================================
    // df-nfc: Class non-freeness (tautological biconditional)
    // LHS: (elem(z,A) → elem(z,A))  —  vacuously stripped F/_ x A
    // RHS: ∀y.(elem(y,A) → elem(y,A))  —  ∀y. F/ x (y ∈ A)
    // Both sides are tautologies; the proof doesn't use the assumptions.
    // =================================================================
    if (label == "df-nfc") {
        WffPtr wff = parse_mm_wff(node.result_expr, 1, thm_info);
        if (!wff || wff->kind != WffNode::Kind::Binary ||
            wff->op != WffNode::Op::Iff) {
            if (error) *error = "df-nfc: expected biconditional";
            return "";
        }

        auto renderer = make_claim_renderer(thm_info);
        std::string lhs_str = emit_fol(*wff->left, renderer);
        std::string rhs_str = emit_fol(*wff->right, renderer);

        // Validate structure: LHS = (F → F), RHS = ∀y.(G → G)
        if (wff->left->kind != WffNode::Kind::Binary ||
            wff->left->op != WffNode::Op::Implies ||
            wff->right->kind != WffNode::Kind::Forall) {
            if (error) *error = "df-nfc: unexpected structure";
            return "";
        }
        const WffNode* rhs_body = wff->right->left.get();
        if (!rhs_body || rhs_body->kind != WffNode::Kind::Binary ||
            rhs_body->op != WffNode::Op::Implies) {
            if (error) *error = "df-nfc: RHS body not implication";
            return "";
        }

        std::string bound_var = wff->right->name;
        std::string lhs_ante = emit_fol(*wff->left->left, renderer);
        std::string rhs_ante = emit_fol(*rhs_body->left, renderer);

        // Forward: LHS → RHS (prove RHS tautology independently)
        std::string h_a1 = state.fresh();
        state.emit(h_a1 + " = assume " + lhs_str);
        state.emit("fix " + bound_var);
        std::string h_a2 = state.fresh();
        state.emit(h_a2 + " = assume " + rhs_ante);
        std::string h_b2 = state.fresh();
        state.emit(h_b2 + " = implies_intro " + h_a2);
        std::string h_fa = state.fresh();
        state.emit(h_fa + " = forall_intro " + h_b2);
        std::string h_fwd = state.fresh();
        state.emit(h_fwd + " = implies_intro " + h_fa);

        // Backward: RHS → LHS (prove LHS tautology independently)
        std::string h_a3 = state.fresh();
        state.emit(h_a3 + " = assume " + rhs_str);
        std::string h_a4 = state.fresh();
        state.emit(h_a4 + " = assume " + lhs_ante);
        std::string h_b4 = state.fresh();
        state.emit(h_b4 + " = implies_intro " + h_a4);
        std::string h_bwd = state.fresh();
        state.emit(h_bwd + " = implies_intro " + h_b4);

        h = state.fresh();
        state.emit(h + " = iff_intro " + h_fwd + ", " + h_bwd);
        handles[idx] = h;
        return h;
    }

    // =================================================================
    // Theorem references
    // =================================================================
    const Assertion* ref = db_.get_assertion(label);
    if (ref && ref->kind == Assertion::Kind::Theorem) {
        if (translated_set_.find(label) == translated_set_.end()) {
            if (error) *error = "theorem not yet translated: " + label;
            return "";
        }

        const FrameInfo* ref_info = get_frame_info(label, *ref, error);
        if (!ref_info) return "";

        // Simple path
        if (is_simple_substitution(*ref, subst, thm_info)) {
            h = emit_simple_use(label, *ref, *ref_info, thm_info,
                                subst, child_handles, state);
            if (!h.empty()) {
                handles[idx] = h;
                return h;
            }
        }

        // Compound path (comprehension)
        h = emit_comprehension_use(label, *ref, *ref_info, thm_info,
                                    subst, child_handles, state, error);
        if (h.empty()) return "";
        handles[idx] = h;
        return h;
    }

    // =================================================================
    // Unsupported
    // =================================================================
    if (error) *error = "unsupported proof step: " + label;
    return "";
}

}  // namespace metamath
