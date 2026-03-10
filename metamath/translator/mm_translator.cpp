#include "mm_translator.h"
#include "comprehension.h"
#include "proof_emit.h"
#include "syntax_to_wff.h"

#include <algorithm>
#include <sstream>

namespace metamath {

MmTranslator::MmTranslator(const MmDatabase& db)
    : db_(db), verifier_(db), syntax_parser_(db) {}

// ===================================================================
// Utility
// ===================================================================

std::string MmTranslator::sanitize_label(const std::string& mm_label) {
    std::string result = "mm_";
    for (char c : mm_label) {
        if (c == '-' || c == '.') result += '_';
        else result += c;
    }
    return result;
}

bool MmTranslator::is_syntax_builder(const Assertion* a) const {
    return a && !a->expression.empty() && a->expression[0] != "|-";
}

// ===================================================================
// Expression translation
// ===================================================================

namespace {

// Leaf renderer: converts Var/Literal/Verum/Falsum to FOL strings.
// IMPORTANT: The returned lambda captures `info` by reference.
// The caller must ensure `info` outlives the lambda.
LeafRenderer make_claim_renderer(const MmTranslator::FrameInfo& info) {
    return [&info](const WffNode& node) -> std::string {
        switch (node.kind) {
            case WffNode::Kind::Var: {
                auto it = info.wff_to_set.find(node.name);
                if (it != info.wff_to_set.end())
                    return "elem(" + info.dummy_var + ", " + it->second + ")";
                return node.name;
            }
            case WffNode::Kind::Literal:
                return node.name;
            case WffNode::Kind::Pred:
                return render_pred(node);
            case WffNode::Kind::Verum: {
                std::string s = info.set_var_order.empty()
                    ? info.dummy_var : info.set_var_order[0];
                return "(elem(" + info.dummy_var + ", " + s +
                       ") -> elem(" + info.dummy_var + ", " + s + "))";
            }
            case WffNode::Kind::Falsum: {
                std::string s = info.set_var_order.empty()
                    ? info.dummy_var : info.set_var_order[0];
                return "~(elem(" + info.dummy_var + ", " + s +
                       ") -> elem(" + info.dummy_var + ", " + s + "))";
            }
            default:
                return "??";
        }
    };
}

}  // anonymous namespace

// ===================================================================
// Comprehension helpers (member wrappers)
// ===================================================================

std::string MmTranslator::get_wff_set(const Expression& wff_expr,
                                       const FrameInfo& thm_info,
                                       ProofState& state) {
    if (wff_expr.size() == 1) {
        auto it = thm_info.wff_to_set.find(wff_expr[0]);
        if (it != thm_info.wff_to_set.end()) return it->second;
    }
    // Compound case: use build_comp_impl to create witness set
    auto cr = build_comp_impl(wff_expr, 0, thm_info, state);
    if (cr.set_var.empty()) return "";
    return cr.set_var;
}

std::string MmTranslator::emit_comprehension_use(
    const std::string& ref_label,
    const Assertion& ref_thm,
    const FrameInfo& ref_info,
    const FrameInfo& caller_info,
    const std::map<std::string, Expression>& subst,
    const std::vector<std::string>& ess_handles,
    ProofState& state,
    std::string* error) {

    std::string fol_label = sanitize_label(ref_label);

    // For each wff var in ref's frame, create a comprehension set for the
    // substitution value (or just use the caller's set var if simple).
    struct Binding {
        std::string wff_var;
        std::string target_set;   // witness or caller's set var
        std::string iff_handle;   // non-empty if compound
    };
    std::vector<Binding> bindings;
    int exists_count = 0;  // number of exists scopes opened

    for (const auto& ref_set : ref_info.set_var_order) {
        std::string wff_var;
        for (const auto& [wv, sv] : ref_info.wff_to_set) {
            if (sv == ref_set) { wff_var = wv; break; }
        }
        if (wff_var.empty()) {
            if (error) *error = "can't find wff var for " + ref_set;
            return "";
        }
        auto sit = subst.find(wff_var);
        if (sit == subst.end()) {
            // Dummy wff var in ref's frame: use caller's dummy var
            bindings.push_back({wff_var, caller_info.dummy_var, ""});
            continue;
        }
        const auto& val = sit->second;

        if (val.size() == 1 && caller_info.wff_to_set.count(val[0])) {
            bindings.push_back({wff_var, caller_info.wff_to_set.at(val[0]), ""});
        } else {
            auto cr = build_comp_impl(val, 0, caller_info, state);
            if (cr.set_var.empty()) {
                std::string val_str;
                for (const auto& t : val) val_str += t + " ";
                if (error) *error = "comprehension failed for " + wff_var + " [" + val_str + "]";
                return "";
            }
            bindings.push_back({wff_var, cr.set_var, cr.iff_handle});
            exists_count += cr.exists_opened;
        }
    }

    // Build the WffAtom map for structural conversion
    std::unordered_map<std::string, WffAtom> atom_map;
    for (const auto& b : bindings) {
        WffAtom wa;
        wa.witness_set = b.target_set;
        wa.iff_handle = b.iff_handle;
        wa.elem_str = "elem(" + caller_info.dummy_var + ", " + b.target_set + ")";
        // Compute compound form
        auto sit = subst.find(b.wff_var);
        if (sit != subst.end()) {
            wa.compound_str = translate_expr(sit->second, 0, caller_info);
            // Wrap in parens if it starts with a quantifier to avoid precedence issues
            if (wa.compound_str.substr(0, 6) == "forall" ||
                wa.compound_str.substr(0, 6) == "exists") {
                wa.compound_str = "(" + wa.compound_str + ")";
            }
        } else {
            wa.compound_str = wa.elem_str;
        }
        atom_map[b.wff_var] = wa;
    }

    // Add T./F. as atoms if they appear in the referenced expression.
    for (const auto& tok : ref_thm.expression) {
        if ((tok == "T." || tok == "F.") && atom_map.find(tok) == atom_map.end()) {
            std::string tf_set;
            if (!ref_info.set_var_order.empty()) {
                for (const auto& b : bindings) {
                    std::string ref_wff;
                    for (const auto& [wv, sv] : ref_info.wff_to_set) {
                        if (sv == ref_info.set_var_order[0]) { ref_wff = wv; break; }
                    }
                    if (b.wff_var == ref_wff) { tf_set = b.target_set; break; }
                }
            }
            if (tf_set.empty()) tf_set = caller_info.dummy_var;

            WffAtom wa;
            wa.witness_set = tf_set;
            wa.iff_handle = "";
            wa.elem_str = "elem(" + caller_info.dummy_var + ", " + tf_set + ")";
            std::string taut = "(elem(" + caller_info.dummy_var + ", " + tf_set +
                               ") -> elem(" + caller_info.dummy_var + ", " + tf_set + "))";
            wa.compound_str = (tok == "T.") ? taut : neg(taut);
            atom_map[tok] = wa;
        }
    }

    // Use the theorem
    std::string h = state.fresh();
    state.emit(h + " = use " + fol_label);

    // forall_elim for used setvars
    for (const auto& ref_sv : ref_info.used_setvars) {
        auto sit = subst.find(ref_sv);
        std::string target_sv;
        if (sit != subst.end() && !sit->second.empty()) {
            if (sit->second.size() > 1) {
                if (error) *error = "compound class subst for setvar " + ref_sv;
                return "";
            }
            target_sv = sit->second[0];
        } else {
            target_sv = ref_sv;
        }
        std::string h_next = state.fresh();
        state.emit(h_next + " = forall_elim " + h + ", " + target_sv);
        h = h_next;
    }

    // forall_elim with witness/caller sets
    for (const auto& b : bindings) {
        std::string h_next = state.fresh();
        state.emit(h_next + " = forall_elim " + h + ", " + b.target_set);
        h = h_next;
    }

    // forall_elim for dummy (last, innermost quantifier)
    if (ref_info.needs_dummy) {
        std::string h_next = state.fresh();
        state.emit(h_next + " = forall_elim " + h + ", " + caller_info.dummy_var);
        h = h_next;
    }

    // For each essential hyp: convert caller's handle (compound-form)
    // to elem-form via backward conversion, then implies_elim.
    for (size_t ess_idx = 0; ess_idx < ref_info.ess_hyps.size() &&
                              ess_idx < ess_handles.size(); ++ess_idx) {
        std::string h_elem = convert_proof(
            *ref_info.ess_hyps[ess_idx].ast, ess_handles[ess_idx],
            atom_map, /*forward=*/false, state);

        std::string h_next = state.fresh();
        state.emit(h_next + " = implies_elim " + h + ", " + h_elem);
        h = h_next;
    }

    // h is the conclusion in elem-form. Convert to compound-form.
    std::string h_compound = convert_proof(
        *ref_info.conclusion_ast, h,
        atom_map, /*forward=*/true, state);

    // Close exists scopes
    for (int i = 0; i < exists_count; ++i) {
        std::string h_next = state.fresh();
        state.emit(h_next + " = exists_intro " + h_compound);
        h_compound = h_next;
    }

    return h_compound;
}

// ===================================================================
// Expression translation (continued)
// ===================================================================

WffPtr MmTranslator::parse_mm_wff(const Expression& tokens, size_t start,
                                    const FrameInfo& info) const {
    Expression expr = {"wff"};
    expr.insert(expr.end(), tokens.begin() + start, tokens.end());

    auto syntax = syntax_parser_.parse(expr);
    if (!syntax) {
        return wff_literal("??parse_fail??");
    }

    std::unordered_set<std::string> setvars(info.setvars.begin(),
                                             info.setvars.end());
    std::unordered_set<std::string> wff_vars;
    for (const auto& [wv, sv] : info.wff_to_set)
        wff_vars.insert(wv);

    SyntaxToWff converter;
    return converter.convert(*syntax, setvars, wff_vars);
}

WffPtr MmTranslator::parse_mm_wff_mut(const Expression& tokens, size_t start,
                                       FrameInfo& info) {
    Expression expr = {"wff"};
    expr.insert(expr.end(), tokens.begin() + start, tokens.end());

    auto syntax = syntax_parser_.parse(expr);
    if (!syntax) {
        return wff_literal("??parse_fail??");
    }

    std::unordered_set<std::string> setvars(info.setvars.begin(),
                                             info.setvars.end());
    std::unordered_set<std::string> wff_vars;
    for (const auto& [wv, sv] : info.wff_to_set)
        wff_vars.insert(wv);

    SyntaxToWff converter;
    WffPtr result = converter.convert(*syntax, setvars, wff_vars);

    for (const auto& v : converter.extra_vars()) {
        info.setvars.push_back(v);
    }

    return result;
}

std::string MmTranslator::translate_expr(const Expression& tokens,
                                          size_t start,
                                          const FrameInfo& info) const {
    WffPtr ast = parse_mm_wff(tokens, start, info);
    return emit_fol(*ast, make_claim_renderer(info));
}

// ===================================================================
// Frame analysis
// ===================================================================

bool MmTranslator::build_frame_info(const Assertion& thm, FrameInfo& info,
                                     std::string* error) {
    const MandatoryFrame& frame = thm.frame;
    info.hyp_labels = frame.hyp_labels;
    info.is_floating = frame.is_floating;

    // Classify variables from $f hypotheses
    for (size_t i = 0; i < frame.hyp_labels.size(); ++i) {
        if (!frame.is_floating[i]) continue;

        const FloatingHyp* fh = db_.get_float_hyp(frame.hyp_labels[i]);
        if (!fh) {
            if (error) *error = "floating hyp not found: " + frame.hyp_labels[i];
            return false;
        }

        if (fh->typecode == "wff") {
            std::string set_var = "S_" + fh->variable;
            info.wff_to_set[fh->variable] = set_var;
            info.set_var_order.push_back(set_var);
        } else if (fh->typecode == "setvar") {
            info.setvars.push_back(fh->variable);
        } else if (fh->typecode == "class") {
            // Treat class variables as setvars — in ZFC every class in a
            // valid theorem is a set.  Record in class_vars so we can
            // distinguish them at call sites (compound class instantiation
            // needs comprehension, not plain forall_elim).
            info.setvars.push_back(fh->variable);
            info.class_vars.insert(fh->variable);
        }
    }

    // Choose a dummy setvar that doesn't clash with existing setvars
    info.dummy_var = "u0";
    while (std::find(info.setvars.begin(), info.setvars.end(),
                     info.dummy_var) != info.setvars.end()) {
        info.dummy_var += "0";
    }

    // Build essential hypothesis info
    for (size_t i = 0; i < frame.hyp_labels.size(); ++i) {
        if (frame.is_floating[i]) continue;

        const EssentialHyp* eh = db_.get_ess_hyp(frame.hyp_labels[i]);
        if (!eh) {
            if (error) *error = "essential hyp not found: " + frame.hyp_labels[i];
            return false;
        }

        // Translate the $e expression (skip leading "|-")
        size_t start = (!eh->expression.empty() && eh->expression[0] == "|-")
                           ? 1 : 0;
        WffPtr ast = parse_mm_wff_mut(eh->expression, start, info);
        std::string fol = emit_fol(*ast, make_claim_renderer(info));

        info.ess_hyps.push_back({frame.hyp_labels[i], fol, ast});
    }

    // Parse conclusion AST
    {
        size_t cstart = (!thm.expression.empty() && thm.expression[0] == "|-")
                            ? 1 : 0;
        info.conclusion_ast = parse_mm_wff_mut(thm.expression, cstart, info);
    }

    // Always wrap with dummy var. The dummy variable serves as the "test element"
    // for set-encoded wff variables and comprehension witnesses.
    info.needs_dummy = true;

    return true;
}

// ===================================================================
// Hybrid theorem reference system
// ===================================================================

const MmTranslator::FrameInfo* MmTranslator::get_frame_info(
    const std::string& label, const Assertion& thm, std::string* error) {
    auto it = frame_cache_.find(label);
    if (it != frame_cache_.end()) return &it->second;

    FrameInfo info;
    if (!build_frame_info(thm, info, error)) return nullptr;
    auto [inserted, ok] = frame_cache_.emplace(label, std::move(info));
    return &inserted->second;
}

bool MmTranslator::is_simple_substitution(
    const Assertion& ref_thm,
    const std::map<std::string, Expression>& subst,
    const FrameInfo& caller_info) const {
    const MandatoryFrame& frame = ref_thm.frame;
    for (size_t i = 0; i < frame.hyp_labels.size(); ++i) {
        if (!frame.is_floating[i]) continue;
        const FloatingHyp* fh = db_.get_float_hyp(frame.hyp_labels[i]);
        if (!fh) continue;

        if (fh->typecode == "wff") {
            auto it = subst.find(fh->variable);
            if (it == subst.end()) return false;
            const Expression& val = it->second;
            if (val.size() != 1) return false;
            if (caller_info.wff_to_set.find(val[0]) == caller_info.wff_to_set.end())
                return false;
        } else if (fh->typecode == "setvar" || fh->typecode == "class") {
            // Compound class substitutions (multi-token) can't use forall_elim
            auto it = subst.find(fh->variable);
            if (it != subst.end() && it->second.size() > 1)
                return false;
        }
    }
    return true;
}

std::string MmTranslator::emit_simple_use(
    const std::string& ref_label,
    const Assertion& ref_thm,
    const FrameInfo& ref_info,
    const FrameInfo& caller_info,
    const std::map<std::string, Expression>& subst,
    const std::vector<std::string>& ess_handles,
    ProofState& state) {

    std::string fol_label = sanitize_label(ref_label);
    std::string h = state.fresh();
    state.emit(h + " = use " + fol_label);

    // forall_elim for each used setvar in R's frame.
    for (const auto& ref_sv : ref_info.used_setvars) {
        auto sit = subst.find(ref_sv);
        std::string target_sv;
        if (sit != subst.end() && !sit->second.empty()) {
            if (sit->second.size() > 1) {
                // Compound class substitution — can't use forall_elim
                return "";
            }
            target_sv = sit->second[0];
        } else {
            target_sv = ref_sv;
        }
        std::string h_next = state.fresh();
        state.emit(h_next + " = forall_elim " + h + ", " + target_sv);
        h = h_next;
    }

    // forall_elim for each set variable in R's set_var_order.
    for (const auto& ref_set_var : ref_info.set_var_order) {
        std::string target_set_var;
        for (const auto& [wff_var, set_var] : ref_info.wff_to_set) {
            if (set_var == ref_set_var) {
                auto sit = subst.find(wff_var);
                if (sit != subst.end() && sit->second.size() == 1) {
                    const std::string& target_wff = sit->second[0];
                    auto tit = caller_info.wff_to_set.find(target_wff);
                    if (tit != caller_info.wff_to_set.end()) {
                        target_set_var = tit->second;
                    }
                }
                break;
            }
        }
        if (target_set_var.empty()) {
            target_set_var = caller_info.dummy_var;
        }
        std::string h_next = state.fresh();
        state.emit(h_next + " = forall_elim " + h + ", " + target_set_var);
        h = h_next;
    }

    // forall_elim for the dummy variable (last, innermost quantifier)
    if (ref_info.needs_dummy) {
        std::string h_next = state.fresh();
        state.emit(h_next + " = forall_elim " + h + ", " + caller_info.dummy_var);
        h = h_next;
    }

    // implies_elim for each essential hypothesis
    for (const auto& eh : ess_handles) {
        std::string h_next = state.fresh();
        state.emit(h_next + " = implies_elim " + h + ", " + eh);
        h = h_next;
    }

    return h;
}

// ===================================================================
// Comprehension-based compound substitution
// ===================================================================

std::string MmTranslator::emit_comprehension_axioms() {
    std::string s;
    s += "# Comprehension axioms for wff-as-set encoding\n";
    s += "# These assert that the set universe is closed under propositional ops\n\n";
    s += "axiom wff_impl: forall A. forall B. exists C. forall u. "
         "(elem(u, C) <-> (elem(u, A) -> elem(u, B)))\n";
    s += "axiom wff_neg: forall A. exists B. forall u. "
         "(elem(u, B) <-> ~elem(u, A))\n";
    s += "axiom wff_and: forall A. forall B. exists C. forall u. "
         "(elem(u, C) <-> (elem(u, A) & elem(u, B)))\n";
    s += "axiom wff_or: forall A. forall B. exists C. forall u. "
         "(elem(u, C) <-> (elem(u, A) | elem(u, B)))\n";
    s += "axiom wff_bic: forall A. forall B. exists C. forall u. "
         "(elem(u, C) <-> (elem(u, A) <-> elem(u, B)))\n";
    s += "axiom wff_true: forall A. exists B. forall u. "
         "(elem(u, B) <-> (elem(u, A) -> elem(u, A)))\n";
    s += "axiom wff_false: forall A. exists B. forall u. "
         "(elem(u, B) <-> ~(elem(u, A) -> elem(u, A)))\n";
    s += "\n";
    return s;
}



// ===================================================================
// Proof simulation
// ===================================================================

bool MmTranslator::apply_step(const std::string& step_label,
                               const FrameInfo& thm_info,
                               ProofState& state,
                               std::string* error) {
    // Check if it's a hypothesis first
    const FloatingHyp* fh = db_.get_float_hyp(step_label);
    if (fh) {
        state.stack.push_back({{fh->typecode, fh->variable}, ""});
        return true;
    }

    const EssentialHyp* eh = db_.get_ess_hyp(step_label);
    if (eh) {
        std::string handle;
        for (const auto& hyp : thm_info.ess_hyps) {
            if (hyp.mm_label == step_label) {
                handle = "hyp_" + sanitize_label(step_label);
                break;
            }
        }
        state.stack.push_back({eh->expression, handle});
        return true;
    }

    // Must be an assertion
    const Assertion* assertion = db_.get_assertion(step_label);
    if (!assertion) {
        if (error) *error = "unknown label: " + step_label;
        return false;
    }

    const MandatoryFrame& frame = assertion->frame;
    size_t n_hyps = frame.hyp_labels.size();

    if (state.stack.size() < n_hyps) {
        if (error) *error = "stack underflow applying " + step_label;
        return false;
    }

    // Pop n_hyps items
    size_t base = state.stack.size() - n_hyps;
    std::vector<StackEntry> popped(state.stack.begin() + base,
                                    state.stack.end());
    state.stack.resize(base);

    // --- Syntax builder: skip (push result with empty handle) ---
    if (is_syntax_builder(assertion)) {
        std::map<std::string, Expression> subst;
        for (size_t i = 0; i < n_hyps; ++i) {
            if (!frame.is_floating[i]) continue;
            const FloatingHyp* f = db_.get_float_hyp(frame.hyp_labels[i]);
            if (!f) continue;
            subst[f->variable] = Expression(popped[i].expr.begin() + 1,
                                             popped[i].expr.end());
        }
        Expression result = MmVerifier::substitute(assertion->expression, subst);
        state.stack.push_back({result, ""});
        return true;
    }

    // --- Build substitution from floating hyps ---
    std::map<std::string, Expression> subst;
    for (size_t i = 0; i < n_hyps; ++i) {
        if (!frame.is_floating[i]) continue;
        const FloatingHyp* f = db_.get_float_hyp(frame.hyp_labels[i]);
        if (!f) continue;
        subst[f->variable] = Expression(popped[i].expr.begin() + 1,
                                         popped[i].expr.end());
    }

    // Collect essential hypothesis handles
    std::vector<std::string> ess_handles;
    for (size_t i = 0; i < n_hyps; ++i) {
        if (frame.is_floating[i]) continue;
        ess_handles.push_back(popped[i].handle);
    }

    // Compute result expression
    Expression result = MmVerifier::substitute(assertion->expression, subst);

    // --- ax-mp: implies_elim ---
    if (step_label == "ax-mp") {
        if (ess_handles.size() < 2) {
            if (error) *error = "ax-mp: need 2 essential hypotheses";
            return false;
        }
        std::string h = state.fresh();
        state.emit(h + " = implies_elim " + ess_handles[1] + ", " +
                   ess_handles[0]);
        state.stack.push_back({result, h});
        return true;
    }

    // --- ax-1: inline ND proof ---
    if (step_label == "ax-1") {
        auto it_ph = subst.find("ph");
        auto it_ps = subst.find("ps");
        if (it_ph == subst.end() || it_ps == subst.end()) {
            if (error) *error = "ax-1: missing substitution variables";
            return false;
        }
        std::string fol_a = translate_expr(it_ph->second, 0, thm_info);
        std::string fol_b = translate_expr(it_ps->second, 0, thm_info);
        std::string h = inline_ax1(fol_a, fol_b, state);
        state.stack.push_back({result, h});
        return true;
    }

    // --- ax-2: inline ND proof ---
    if (step_label == "ax-2") {
        auto it_ph = subst.find("ph");
        auto it_ps = subst.find("ps");
        auto it_ch = subst.find("ch");
        if (it_ph == subst.end() || it_ps == subst.end() ||
            it_ch == subst.end()) {
            if (error) *error = "ax-2: missing substitution variables";
            return false;
        }
        std::string fol_a = translate_expr(it_ph->second, 0, thm_info);
        std::string fol_b = translate_expr(it_ps->second, 0, thm_info);
        std::string fol_c = translate_expr(it_ch->second, 0, thm_info);
        std::string h = inline_ax2(fol_a, fol_b, fol_c, state);
        state.stack.push_back({result, h});
        return true;
    }

    // --- ax-3: inline ND proof ---
    if (step_label == "ax-3") {
        auto it_ph = subst.find("ph");
        auto it_ps = subst.find("ps");
        if (it_ph == subst.end() || it_ps == subst.end()) {
            if (error) *error = "ax-3: missing substitution variables";
            return false;
        }
        std::string fol_a = translate_expr(it_ph->second, 0, thm_info);
        std::string fol_b = translate_expr(it_ps->second, 0, thm_info);
        std::string h = inline_ax3(fol_a, fol_b, state);
        state.stack.push_back({result, h});
        return true;
    }

    // --- Definition axioms: df-bi, df-an, df-or ---
    if (step_label == "df-bi" || step_label == "df-an" ||
        step_label == "df-or") {
        auto it_ph = subst.find("ph");
        auto it_ps = subst.find("ps");
        if (it_ph == subst.end() || it_ps == subst.end()) {
            if (error) *error = step_label + ": missing substitution";
            return false;
        }
        std::string a = translate_expr(it_ph->second, 0, thm_info);
        std::string b = translate_expr(it_ps->second, 0, thm_info);
        std::string h;
        if (step_label == "df-bi") h = inline_df_bi(a, b, state);
        else if (step_label == "df-an") h = inline_df_an(a, b, state);
        else h = inline_df_or(a, b, state);
        state.stack.push_back({result, h});
        return true;
    }

    // --- Identity-biconditional definitions ---
    // Both sides desugar to the same FOL formula via parse_wff
    {
        static const std::unordered_set<std::string> identity_defs = {
            // Propositional connective definitions
            "df-3an", "df-3or", "df-xor", "df-nan", "df-nor",
            "df-fal", "df-ifp", "df-cad", "df-had", "df-nf",
            "df-tru",
            // Restricted quantifier / predicate abbreviation definitions
            "df-ral", "df-rex", "df-rab",
            "df-sbc", "df-clab",
            // Set operation membership definitions
            "df-pr", "df-sn", "df-tp", "df-un", "df-in",
            // Negated predicate definitions (no new quantifiers)
            "df-ne", "df-nel",
        };
        if (identity_defs.count(step_label)) {
            // Find <-> in substituted result at depth 0 (skip leading |- and ()
            size_t bic_pos = 2; // skip |- (
            int depth = 0;
            while (bic_pos < result.size()) {
                if (result[bic_pos] == "(") depth++;
                else if (result[bic_pos] == ")") {
                    if (depth == 0) break;
                    depth--;
                } else if (depth == 0 && result[bic_pos] == "<->") break;
                bic_pos++;
            }
            // Translate LHS: result[2..bic_pos)
            Expression lhs_expr(result.begin() + 2, result.begin() + bic_pos);
            std::string formula = translate_expr(lhs_expr, 0, thm_info);
            std::string h = emit_identity_bic(formula, state);
            state.stack.push_back({result, h});
            return true;
        }
    }

    // --- Predicate logic axiom handlers ---

    // ax-5: Vacuous quantification: ph -> A. x ph
    // With setvar quantifier stripping, A. x ph = ph, so this is ph -> ph (identity)
    if (step_label == "ax-5") {
        auto it_ph = subst.find("ph");
        if (it_ph == subst.end()) {
            if (error) *error = "ax-5: missing ph substitution";
            return false;
        }
        std::string phi_fol = translate_expr(it_ph->second, 0, thm_info);
        // Generate identity: assume phi, implies_intro → (phi -> phi)
        std::string h_assume = state.fresh();
        state.emit(h_assume + " = assume " + phi_fol);
        std::string h_id = state.fresh();
        state.emit(h_id + " = implies_intro " + h_assume);
        state.stack.push_back({result, h_id});
        return true;
    }

    // ax-gen: Generalization rule
    // From |- ph, derive |- A. x ph
    // In set encoding, A. x ph = ph (vacuous setvar quantifier) — identity.
    // For non-setvar quantifiers, use fix/forall_intro.
    if (step_label == "ax-gen") {
        if (ess_handles.empty() || ess_handles[0].empty()) {
            if (error) *error = "ax-gen: missing essential hyp handle";
            return false;
        }
        auto it_x = subst.find("x");
        bool is_setvar = it_x != subst.end() && !it_x->second.empty() &&
            std::find(thm_info.setvars.begin(), thm_info.setvars.end(),
                      it_x->second[0]) != thm_info.setvars.end();
        if (is_setvar) {
            // Vacuous quantifier: result = essential hyp
            state.stack.push_back({result, ess_handles[0]});
            return true;
        }
        // Non-setvar quantifier: fix fresh var, transport, forall_intro
        auto it_ph = subst.find("ph");
        if (it_ph == subst.end()) {
            if (error) *error = "ax-gen: missing ph substitution";
            return false;
        }
        std::string phi_fol = translate_expr(it_ph->second, 0, thm_info);
        std::string gen_var = "_genv" + std::to_string(state.counter);
        state.emit("fix " + gen_var);
        std::string h_assume = state.fresh();
        state.emit(h_assume + " = assume " + phi_fol);
        std::string h_id = state.fresh();
        state.emit(h_id + " = implies_intro " + h_assume);
        std::string h_local = state.fresh();
        state.emit(h_local + " = implies_elim " + h_id + ", " + ess_handles[0]);
        std::string h_result = state.fresh();
        state.emit(h_result + " = forall_intro " + h_local);
        state.stack.push_back({result, h_result});
        return true;
    }

    // ax-4: Quantifier distribution: A. x (ph -> ps) -> (A. x ph -> A. x ps)
    // With setvar quantifier stripping, this is (ph -> ps) -> (ph -> ps) — identity
    if (step_label == "ax-4") {
        auto it_ph = subst.find("ph"), it_ps = subst.find("ps"),
             it_x = subst.find("x");
        if (it_ph == subst.end() || it_ps == subst.end() ||
            it_x == subst.end()) {
            if (error) *error = "ax-4: missing substitution variables";
            return false;
        }
        std::string ph = translate_expr(it_ph->second, 0, thm_info);
        std::string ps = translate_expr(it_ps->second, 0, thm_info);

        // Identity: (ph -> ps) -> (ph -> ps)
        std::string formula = "(" + ph + " -> " + ps + ")";
        std::string h1 = state.fresh();
        state.emit(h1 + " = assume " + formula);
        std::string h2 = state.fresh();
        state.emit(h2 + " = implies_intro " + h1);
        state.stack.push_back({result, h2});
        return true;
    }

    // ax-6: Existence of equal elements (via bridge)
    if (step_label == "ax-6") {
        std::string y = subst.at("y")[0];
        std::string h = emit_bridge_use("ax_6", {y}, state);
        state.stack.push_back({result, h});
        return true;
    }

    // ax-7: Equality is right-Euclidean (via bridge)
    if (step_label == "ax-7") {
        std::string h = emit_bridge_use("ax_7",
            {subst.at("x")[0], subst.at("y")[0], subst.at("z")[0]}, state);
        state.stack.push_back({result, h});
        return true;
    }

    // ax-8: Left substitution into membership (via bridge)
    if (step_label == "ax-8") {
        std::string h = emit_bridge_use("ax_8",
            {subst.at("x")[0], subst.at("y")[0], subst.at("z")[0]}, state);
        state.stack.push_back({result, h});
        return true;
    }

    // ax-9: Right substitution into membership (via bridge)
    if (step_label == "ax-9") {
        std::string h = emit_bridge_use("ax_9",
            {subst.at("x")[0], subst.at("y")[0], subst.at("z")[0]}, state);
        state.stack.push_back({result, h});
        return true;
    }

    // ax-10: Quantifier negation (via bridge)
    if (step_label == "ax-10") {
        auto it_ph = subst.find("ph");
        if (it_ph == subst.end()) {
            if (error) *error = "ax-10: missing ph substitution";
            return false;
        }
        std::string S_ph = get_wff_set(it_ph->second, thm_info, state);
        if (S_ph.empty()) {
            if (error) *error = "ax-10: cannot resolve wff to set";
            return false;
        }
        std::string h = emit_bridge_use("ax_10", {S_ph}, state);
        state.stack.push_back({result, h});
        return true;
    }

    // ax-11: Quantifier commutation (via bridge)
    if (step_label == "ax-11") {
        // ax_11 bridge is for elem(x,y), but mm ax-11 is about wff ph
        // with two setvars. The bridge is specialized; skip for now.
        if (error) *error = "unsupported proof step: " + step_label;
        return false;
    }

    // ax-ext: Extensionality (via bridge)
    if (step_label == "ax-ext") {
        auto ix = subst.find("x"), iy = subst.find("y");
        if (ix == subst.end() || iy == subst.end()) {
            if (error) *error = "ax-ext: missing x/y substitution";
            return false;
        }
        std::string h = emit_bridge_use("ax_ext",
            {ix->second[0], iy->second[0]}, state);
        state.stack.push_back({result, h});
        return true;
    }

    // df-cleq: Class equality definition (via axextb_bridge)
    // For setvars: eq(A,B) <-> forall x. (elem(x,A) <-> elem(x,B))
    if (step_label == "df-cleq") {
        auto ia = subst.find("A"), ib = subst.find("B");
        if (ia == subst.end() || ib == subst.end()) {
            if (error) *error = "df-cleq: missing A/B substitution";
            return false;
        }
        if (ia->second.size() != 1 || ib->second.size() != 1) {
            if (error) *error = "df-cleq: compound class expression";
            return false;
        }
        std::string h = emit_bridge_use("axextb_bridge",
            {ia->second[0], ib->second[0]}, state);
        state.stack.push_back({result, h});
        return true;
    }

    // df-ex: Existential definition (via bridge)
    if (step_label == "df-ex") {
        auto it_ph = subst.find("ph");
        if (it_ph == subst.end()) {
            if (error) *error = "df-ex: missing ph substitution";
            return false;
        }
        std::string S_ph = get_wff_set(it_ph->second, thm_info, state);
        if (S_ph.empty()) {
            if (error) *error = "df-ex: cannot resolve wff to set";
            return false;
        }
        std::string h = emit_bridge_use("df_ex", {S_ph, thm_info.dummy_var}, state);
        state.stack.push_back({result, h});
        return true;
    }

    // --- Theorem reference: hybrid approach ---
    if (assertion->kind == Assertion::Kind::Theorem) {
        // Check if the referenced theorem has been translated
        if (translated_set_.find(step_label) == translated_set_.end()) {
            if (error) *error = "theorem not yet translated: " + step_label;
            return false;
        }

        // Get/build frame info for the referenced theorem (cached)
        const FrameInfo* ref_info = get_frame_info(step_label, *assertion, error);
        if (!ref_info) return false;

        // SIMPLE PATH: all wff vars map to single wff vars
        if (is_simple_substitution(*assertion, subst, thm_info)) {
            std::string h = emit_simple_use(step_label, *assertion, *ref_info,
                                             thm_info, subst, ess_handles, state);
            if (h.empty()) {
                // Fall through to compound path
            } else {
                state.stack.push_back({result, h});
                return true;
            }
        }

        // COMPOUND PATH: comprehension-based use
        std::string h = emit_comprehension_use(
            step_label, *assertion, *ref_info, thm_info,
            subst, ess_handles, state, error);
        if (h.empty()) return false;
        state.stack.push_back({result, h});
        return true;
    }

    // --- Unsupported step ---
    if (error)
        *error = "unsupported proof step: " + step_label;
    return false;
}

bool MmTranslator::simulate_proof(const Assertion& thm,
                                    const FrameInfo& info_in,
                                    ProofState& state,
                                    std::string* error) {
    // info_in already has dummy wff vars from pre-scan in translate()
    const FrameInfo& info = info_in;
    const DecodedProof& proof = thm.proof;

    // Helper: push a hypothesis by label
    auto push_hyp = [&](const std::string& hyp_label) -> bool {
        const FloatingHyp* fh = db_.get_float_hyp(hyp_label);
        if (fh) {
            state.stack.push_back({{fh->typecode, fh->variable}, ""});
            return true;
        }
        const EssentialHyp* eh = db_.get_ess_hyp(hyp_label);
        if (eh) {
            std::string handle;
            for (const auto& hyp : info.ess_hyps) {
                if (hyp.mm_label == hyp_label) {
                    handle = "hyp_" + sanitize_label(hyp_label);
                    break;
                }
            }
            state.stack.push_back({eh->expression, handle});
            return true;
        }
        return false;
    };

    auto process_label = [&](const std::string& step_label) -> bool {
        if (push_hyp(step_label)) return true;
        return apply_step(step_label, info, state, error);
    };

    if (proof.compressed) {
        size_t mandhypt = thm.frame.hyp_labels.size();
        size_t labelt = mandhypt + proof.paren_labels.size();

        for (size_t num : proof.numbers) {
            if (num == 0) {
                if (state.stack.empty()) {
                    if (error) *error = "save on empty stack";
                    return false;
                }
                state.saved.push_back(state.stack.back());
            } else if (num <= mandhypt) {
                if (!push_hyp(thm.frame.hyp_labels[num - 1])) {
                    if (error)
                        *error = "mandatory hyp not found: " +
                                 thm.frame.hyp_labels[num - 1];
                    return false;
                }
            } else if (num <= labelt) {
                if (!process_label(proof.paren_labels[num - mandhypt - 1]))
                    return false;
            } else {
                size_t idx = num - labelt - 1;
                if (idx >= state.saved.size()) {
                    if (error)
                        *error = "saved index " + std::to_string(idx) +
                                 " out of range";
                    return false;
                }
                state.stack.push_back(state.saved[idx]);
            }
        }
    } else {
        for (const auto& step_label : proof.labels) {
            if (!process_label(step_label)) return false;
        }
    }

    return true;
}

// ===================================================================
// translate() helpers
// ===================================================================

bool MmTranslator::is_skipped(const std::string& label) {
    static const std::unordered_set<std::string> skip_labels = {
        // T./F. set-variable mismatch
        "alfal", "altru", "bifal", "bitru", "cadtru",
        "dftru2", "dfnot", "falim", "mptru", "nbfal",
        "tbtru", "truan", "trud", "trujust", "trut",
        // hadd/cadd structural conversion
        "cadnot", "hadnot",
        // Quantifier bridge mismatch after vacuous stripping
        "eximal", "ax6ev", "speimfw", "speimfwALT", "spimfw", "2exnaln",
        "spimw", "spimew", "equs4v", "alequexv", "equsv",
        // Non-vacuous quantifier encoding mismatch
        "nf2", "nfi", "nfri", "nfd", "nfrd",
        "ala1", "alex", "exa1", "nfbii", "nfbiit",
        "imnang", "exanali", "2exanali", "exancom", "exan",
        "nexdh", "albidh", "exsimpl", "exsimpr",
        "19.33b", "19.40b", "albiim", "exintrbi", "exintr",
        "alsyl", "nfbidv", "3exdistr", "ax12i", "ax6v",
        // Comprehension proof bug with class-var quantifier encoding
        "sbtlem",
        // Comprehension mismatch: eq/class formula vs elem encoding
        "ax13w", "ax13dgen4", "drnf1v",
        "rabidim1", "dfv2", "elv", "elvd", "el2v", "el3v", "elinel2",
        "elinel1", "elind", "elini", "elunant", "elunnel2", "elunnel1",
        "neldif", "eldifn", "eldifi", "velcomp", "eldifbd", "eldifad",
        "eldifd", "gencl", "el3v3",
        // df-clel expansion introduces inner forall with fresh var
        "dfrab3", "dfnul4",
        // verification failure: implies_elim mismatch
        "equeuclr", "elequ2g",
        // verification failure: implies_elim antecedent mismatch
        "stdpc6", "ax8v1", "ax8v2", "elequ12",
        "ax13dgen1", "ax13dgen2", "ax13dgen3",
        // verification failure: implies_elim on forall
        "darii", "dariiALT", "festino", "festinoALT",
        "baroco", "barocoALT",
        "darapti", "daraptiALT", "felapton",
        // verification failure: implies_elim antecedent mismatch
        "axexte",
        // verification failure: formula mismatch (extensionality encoding)
        "axextb",
        // verification failure: implies_elim antecedent mismatch (distributing rnf)
        "drnf1",
        // forall_intro scope mismatch (class var expansion)
        "cvjust", "abid1", "abid2",
        // ne (not-equal) expansion: ~eq encoding mismatch
        "nne", "exmidne", "eqneqall", "necon3ad", "necon2bd",
        "necon1bd", "necon2d", "necon3ai", "necon3bi",
        "necon2ai", "necon2bi", "necon1bi", "necon1i",
        "necon2i", "necon4i", "necon3abid", "necon3bbid",
        "necon4bbid", "necon2bbid", "necon3abii", "necon3bbii",
        "necon2abii", "necon2bbii", "nebi", "pm2.21ddne",
        "necon1bbid", "mteqand", "neor", "neanior", "neorian",
        "nemtbir", "nelcon3d", "nnel", "pm2.24nel", "pm2.61danel",
        // restricted quantifier (ral*/rex*) encoding mismatch
        "rgenw", "ralimia", "reximia", "ralimiaa",
        "ralimi", "reximi", "ralbiia", "rexbiia",
        "ralbii", "rexbii", "ralanid", "rexanid", "ralcom3",
        "ralrimiva", "rexlimiv", "ralrimivw", "rexlimivw",
        "ralrimdva", "reximdvai", "ralimdva", "reximdva",
        "ralimdv", "reximdv", "ralbidva", "rexbidva",
        "ralbidv", "rexbidv",
        // forall_intro scope mismatch (class/csb/set-op expansion)
        "vjust", "csbid", "csbcow", "csbco",
        "difjust", "unjust", "injust", "dfin5", "dfdif2",
        // conditional equality encoding mismatch
        "cdeqth", "cdeqnot", "cdeqim",
        // forall_intro scope mismatch (subset/class-builder/csb expansion)
        "ssid", "unab", "inab", "difab",
        "csb0", "csbcom", "csbidm", "csbab", "csbun", "csbin", "csbdif", "csbif",
        "snjust", "dfpr2", "dftp2",
        // ne/class implies_elim mismatch
        "elprn1", "elprn2", "eldifsnd", "eldifsni",
        // implies_elim antecedent mismatch (replacement axiom)
        "axreplem",
    };
    return skip_labels.count(label) > 0;
}

bool MmTranslator::try_bridge_equiv(const std::string& label,
                                     const FrameInfo& info,
                                     const WffPtr& claim_ast,
                                     TranslatedTheorem& result) {
    struct BridgeEquiv {
        std::string bridge_name;
        enum class Src { Setvar, WffSet, Dummy };
        struct Param { Src src; size_t idx; };
        std::vector<Param> params;
    };
    using S = BridgeEquiv::Src;
    static const std::unordered_map<std::string, BridgeEquiv> bridge_equiv = {
        {"ax7",      {"ax_7",    {{S::Setvar,0},{S::Setvar,1},{S::Setvar,2}}}},
        {"ax8",      {"ax_8",    {{S::Setvar,0},{S::Setvar,1},{S::Setvar,2}}}},
        {"ax9",      {"ax_9",    {{S::Setvar,0},{S::Setvar,1},{S::Setvar,2}}}},
        {"equid",    {"eq_refl", {{S::Setvar,0}}}},
        {"equcomi",  {"eq_sym",  {{S::Setvar,0},{S::Setvar,1}}}},
        {"equcomiv", {"eq_sym",  {{S::Setvar,0},{S::Setvar,1}}}},
        {"equcom",   {"eq_sym_iff",  {{S::Setvar,0},{S::Setvar,1}}}},
        {"equtr",    {"eq_trans",    {{S::Setvar,0},{S::Setvar,1},{S::Setvar,2}}}},
        {"equtrr",   {"eq_subst_eq", {{S::Setvar,0},{S::Setvar,1},{S::Setvar,2}}}},
        {"equequ1",  {"equequ1_bridge", {{S::Setvar,0},{S::Setvar,1},{S::Setvar,2}}}},
        {"elequ2",   {"elequ2_bridge",  {{S::Setvar,0},{S::Setvar,1},{S::Setvar,2}}}},
        {"elequ1",   {"elequ1_bridge",  {{S::Setvar,0},{S::Setvar,1},{S::Setvar,2}}}},
        {"equcoms",  {"equcoms_bridge", {{S::Setvar,0},{S::Setvar,1},{S::WffSet,0},{S::Dummy,0}}}},
        {"equcomd",  {"equcomd_bridge", {{S::Setvar,0},{S::Setvar,1},{S::WffSet,0},{S::Dummy,0}}}},
    };
    auto it = bridge_equiv.find(label);
    if (it == bridge_equiv.end()) return false;

    // Strip forall vars from claim AST
    const WffNode* body = claim_ast.get();
    std::vector<std::string> forall_vars;
    while (body && body->kind == WffNode::Kind::Forall) {
        forall_vars.push_back(body->name);
        body = body->left.get();
    }

    ProofState tstate;
    for (const auto& v : forall_vars)
        tstate.emit("fix " + v);

    std::string h = tstate.fresh();
    tstate.emit(h + " = use " + it->second.bridge_name);
    for (const auto& p : it->second.params) {
        std::string arg;
        switch (p.src) {
            case S::Setvar: arg = info.setvars.at(p.idx); break;
            case S::WffSet: arg = info.set_var_order.at(p.idx); break;
            case S::Dummy:  arg = info.dummy_var; break;
        }
        std::string h_next = tstate.fresh();
        tstate.emit(h_next + " = forall_elim " + h + ", " + arg);
        h = h_next;
    }

    for (int i = static_cast<int>(forall_vars.size()) - 1; i >= 0; --i) {
        std::string h_next = tstate.fresh();
        tstate.emit(h_next + " = forall_intro " + h);
        h = h_next;
    }

    tstate.emit("qed " + h);
    result.proof_lines = tstate.lines;
    translated_set_.insert(label);
    return true;
}

bool MmTranslator::try_trivial_claim(const FrameInfo& info,
                                      const WffPtr& claim_ast,
                                      int num_foralls, bool needs_dummy,
                                      TranslatedTheorem& result) {
    // Strip outer forall quantifiers
    const WffNode* body = claim_ast.get();
    while (body && body->kind == WffNode::Kind::Forall)
        body = body->left.get();

    bool is_trivial_iff = false, is_trivial_impl = false;
    std::string trivial_sub;

    // Verum body: renders to (elem(d,s) -> elem(d,s))
    if (body && body->kind == WffNode::Kind::Verum) {
        is_trivial_impl = true;
        std::string s = info.set_var_order.empty()
            ? info.dummy_var : info.set_var_order[0];
        trivial_sub = "elem(" + info.dummy_var + ", " + s + ")";
    }

    if (body && body->kind == WffNode::Kind::Binary) {
        auto renderer = make_claim_renderer(info);
        std::string lhs_str = emit_fol(*body->left, renderer);
        std::string rhs_str = emit_fol(*body->right, renderer);
        if (body->op == WffNode::Op::Iff && lhs_str == rhs_str) {
            is_trivial_iff = true;
            trivial_sub = std::move(lhs_str);
        } else if (body->op == WffNode::Op::Implies && lhs_str == rhs_str) {
            is_trivial_impl = true;
            trivial_sub = std::move(lhs_str);
        }
    }

    if (!is_trivial_iff && !is_trivial_impl) return false;

    ProofState tstate;
    for (const auto& sv : info.used_setvars)
        tstate.emit("fix " + sv);
    if (needs_dummy) {
        for (const auto& sv : info.set_var_order)
            tstate.emit("fix " + sv);
        tstate.emit("fix " + info.dummy_var);
    }

    std::string h1 = tstate.fresh();
    tstate.emit(h1 + " = assume " + trivial_sub);
    std::string h2 = tstate.fresh();
    tstate.emit(h2 + " = implies_intro " + h1);
    std::string last;
    if (is_trivial_iff) {
        std::string h3 = tstate.fresh();
        tstate.emit(h3 + " = assume " + trivial_sub);
        std::string h4 = tstate.fresh();
        tstate.emit(h4 + " = implies_intro " + h3);
        last = tstate.fresh();
        tstate.emit(last + " = iff_intro " + h2 + ", " + h4);
    } else {
        last = h2;
    }
    for (int i = num_foralls - 1; i >= 0; --i) {
        std::string h = tstate.fresh();
        tstate.emit(h + " = forall_intro " + last);
        last = h;
    }
    tstate.emit("qed " + last);
    result.proof_lines = std::move(tstate.lines);
    return true;
}

// ===================================================================
// Main translation entry point
// ===================================================================

bool MmTranslator::translate(const std::string& label,
                              TranslatedTheorem& result,
                              std::string* error) {
    if (is_skipped(label)) {
        if (error) *error = "known encoding issue: " + label;
        ++skipped_;
        return false;
    }

    const Assertion* thm = db_.get_assertion(label);
    if (!thm || thm->kind != Assertion::Kind::Theorem) {
        if (error) *error = "'" + label + "' is not a theorem";
        return false;
    }

    // Build frame info
    FrameInfo info;
    if (!build_frame_info(*thm, info, error)) {
        ++skipped_;
        return false;
    }

    // Pre-scan: allocate set variables for optional (dummy) wff hypotheses.
    // Added to wff_to_set AND set_var_order — they must be fixed and quantified
    // because intermediate proof steps reference them.
    {
        const auto& proof = thm->proof;
        auto scan_labels = proof.compressed ? proof.paren_labels : proof.labels;
        for (const auto& lbl : scan_labels) {
            const FloatingHyp* fh = db_.get_float_hyp(lbl);
            if (!fh || fh->typecode != "wff") continue;
            if (info.wff_to_set.count(fh->variable)) continue;
            std::string set_var = "S_" + fh->variable;
            info.wff_to_set[fh->variable] = set_var;
            info.set_var_order.push_back(set_var);
        }
    }

    // Translate the conclusion expression (skip "|-")
    size_t start = (!thm->expression.empty() && thm->expression[0] == "|-")
                       ? 1 : 0;
    info.conclusion_ast = parse_mm_wff_mut(thm->expression, start, info);
    std::string conclusion = emit_fol(*info.conclusion_ast, make_claim_renderer(info));

    // Build the full claim AST: forall S_i. forall u0. (H1 -> (H2 -> ... -> C))
    WffPtr claim_ast = info.conclusion_ast;
    for (int i = static_cast<int>(info.ess_hyps.size()) - 1; i >= 0; --i) {
        claim_ast = wff_binary(WffNode::Op::Implies,
                               info.ess_hyps[i].ast, claim_ast);
    }
    bool needs_dummy = info.needs_dummy;

    // Wrap innermost: dummy var, then set_var_order, then used_setvars (outermost)
    if (needs_dummy) {
        claim_ast = wff_forall(info.dummy_var, claim_ast);
        for (int i = static_cast<int>(info.set_var_order.size()) - 1; i >= 0;
             --i) {
            claim_ast = wff_forall(info.set_var_order[i], claim_ast);
        }
    }

    // Build the claim formula string
    std::string formula = conclusion;
    for (int i = static_cast<int>(info.ess_hyps.size()) - 1; i >= 0; --i) {
        formula = "(" + info.ess_hyps[i].fol_formula + " -> " + formula + ")";
    }
    if (needs_dummy) {
        formula = "forall " + info.dummy_var + ". " + formula;
        for (int i = static_cast<int>(info.set_var_order.size()) - 1; i >= 0;
             --i) {
            formula = "forall " + info.set_var_order[i] + ". " + formula;
        }
    }

    // Include ALL setvars (not just those in the formula string).
    // Class variables may appear only in ess_hyps or proof references,
    // not in the claim formula (e.g., x e. _V expands to Verum).
    info.used_setvars = info.setvars;
    for (int i = static_cast<int>(info.used_setvars.size()) - 1; i >= 0; --i) {
        claim_ast = wff_forall(info.used_setvars[i], claim_ast);
        formula = "forall " + info.used_setvars[i] + ". " + formula;
    }

    // Skip if formula contains untranslatable tokens
    if (formula.find("??") != std::string::npos) {
        // Extract the first untranslatable token for diagnostics
        auto p1 = formula.find("??");
        auto p2 = formula.find("??", p1 + 2);
        std::string bad_tok = (p2 != std::string::npos)
            ? formula.substr(p1 + 2, p2 - p1 - 2) : "?";
        if (error) *error = "untranslatable token in formula [" + bad_tok + "]: " + label;
        ++skipped_;
        return false;
    }

    result.mm_label = label;
    result.fol_label = sanitize_label(label);
    result.claim_formula = formula;

    // Bridge-equivalent theorems (direct bridge proofs)
    if (try_bridge_equiv(label, info, claim_ast, result))
        return true;

    // Trivial claims: A <-> A, A -> A, Verum
    {
        const WffNode* body = claim_ast.get();
        int num_foralls = 0;
        while (body && body->kind == WffNode::Kind::Forall) {
            ++num_foralls;
            body = body->left.get();
        }
        if (try_trivial_claim(info, claim_ast, num_foralls, needs_dummy, result)) {
            translated_set_.insert(label);
            frame_cache_.emplace(label, std::move(info));
            return true;
        }
    }

    // --- Translate the proof ---
    ProofState state;

    // Fix order: used_setvars (outermost), set_var_order, dummy (innermost)
    for (const auto& sv : info.used_setvars) {
        state.emit("fix " + sv);
    }
    if (needs_dummy) {
        for (const auto& sv : info.set_var_order) {
            state.emit("fix " + sv);
        }
        state.emit("fix " + info.dummy_var);
    }

    // Emit assume statements for essential hypotheses
    for (const auto& hyp : info.ess_hyps) {
        std::string handle = "hyp_" + sanitize_label(hyp.mm_label);
        state.emit(handle + " = assume " + hyp.fol_formula);
    }

    // Simulate the proof
    if (!simulate_proof(*thm, info, state, error)) {
        ++skipped_;
        return false;
    }

    // The stack should have exactly one item
    if (state.stack.size() != 1) {
        if (error)
            *error = "proof stack has " + std::to_string(state.stack.size()) +
                     " items, expected 1";
        ++skipped_;
        return false;
    }

    std::string final_handle = state.stack[0].handle;
    if (final_handle.empty()) {
        if (error) *error = "final stack entry has no handle";
        ++skipped_;
        return false;
    }

    // Close essential hypothesis scopes
    std::string h_cur = final_handle;
    for (int i = static_cast<int>(info.ess_hyps.size()) - 1; i >= 0; --i) {
        std::string hyp_handle =
            "hyp_" + sanitize_label(info.ess_hyps[i].mm_label);
        std::string h_pair = state.fresh();
        state.emit(h_pair + " = and_intro " + hyp_handle + ", " + h_cur);
        std::string h_local = state.fresh();
        state.emit(h_local + " = and_elim_r " + h_pair);
        std::string h_next = state.fresh();
        state.emit(h_next + " = implies_intro " + h_local);
        h_cur = h_next;
    }

    // Transport result to innermost scope if needed.
    // When needs_dummy is true, the result may be derived in an outer setvar
    // scope (via forall_elim). Transport re-derives it in the current (u0) scope
    // using assume + implies_intro + implies_elim.
    if (needs_dummy) {
        // Compute the formula at this point
        std::string body_formula = emit_fol(*info.conclusion_ast, make_claim_renderer(info));
        for (int i = static_cast<int>(info.ess_hyps.size()) - 1; i >= 0; --i) {
            body_formula = "(" + info.ess_hyps[i].fol_formula + " -> " +
                           body_formula + ")";
        }
        h_cur = emit_transport(h_cur, body_formula, state);
    }

    // Close forall scopes: dummy first (innermost), then set_var_order, then setvars
    if (needs_dummy) {
        std::string h_next = state.fresh();
        state.emit(h_next + " = forall_intro " + h_cur);
        h_cur = h_next;
        for (int i = static_cast<int>(info.set_var_order.size()) - 1; i >= 0;
             --i) {
            std::string h_next2 = state.fresh();
            state.emit(h_next2 + " = forall_intro " + h_cur);
            h_cur = h_next2;
        }
    }
    for (int i = static_cast<int>(info.used_setvars.size()) - 1; i >= 0; --i) {
        std::string h_next = state.fresh();
        state.emit(h_next + " = forall_intro " + h_cur);
        h_cur = h_next;
    }

    state.emit("qed " + h_cur);

    result.proof_lines = std::move(state.lines);

    // Check proof lines for untranslatable tokens
    for (const auto& line : result.proof_lines) {
        if (line.find("??") != std::string::npos) {
            if (error) *error = "untranslatable token in proof: " + label +
                " [line: " + line.substr(0, 120) + "]";
            ++skipped_;
            return false;
        }
    }

    translated_set_.insert(label);

    // Also cache the frame info for this theorem (for future references)
    frame_cache_.emplace(label, std::move(info));

    return true;
}

// ===================================================================
// Emission
// ===================================================================

std::string MmTranslator::emit_def(
    const std::vector<TranslatedTheorem>& thms) {
    std::ostringstream oss;
    oss << "# Translated from Metamath set.mm\n\n";
    for (const auto& thm : thms) {
        oss << "# " << thm.mm_label << "\n";
        oss << "claim " << thm.fol_label << ": " << thm.claim_formula << "\n\n";
    }
    return oss.str();
}

std::string MmTranslator::emit_proof(
    const std::vector<TranslatedTheorem>& thms) {
    std::ostringstream oss;
    oss << "# Translated from Metamath set.mm\n\n";
    for (const auto& thm : thms) {
        oss << "# " << thm.mm_label << "\n";
        oss << "proof " << thm.fol_label << ":\n";
        for (const auto& line : thm.proof_lines) {
            oss << line << "\n";
        }
        oss << "\n";
    }
    return oss.str();
}

}  // namespace metamath
