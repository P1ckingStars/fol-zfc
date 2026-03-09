#include "mm_translator.h"
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

// Negate a formula string, adding a space to avoid ~~ tokens.
std::string neg(const std::string& s) {
    if (!s.empty() && s[0] == '~') return "~ " + s;
    return "~" + s;
}

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
// Helper functions for extended axiom/definition support
// ===================================================================

std::string MmTranslator::emit_bridge_use(const std::string& bridge_name,
                                           const std::vector<std::string>& args,
                                           ProofState& state) {
    std::string h = state.fresh();
    state.emit(h + " = use " + bridge_name);
    for (const auto& arg : args) {
        std::string h_next = state.fresh();
        state.emit(h_next + " = forall_elim " + h + ", " + arg);
        h = h_next;
    }
    return h;
}

std::string MmTranslator::emit_identity_bic(const std::string& formula,
                                             ProofState& state) {
    std::string h1 = state.fresh();
    state.emit(h1 + " = assume " + formula);
    std::string hf = state.fresh();
    state.emit(hf + " = implies_intro " + h1);
    std::string h2 = state.fresh();
    state.emit(h2 + " = assume " + formula);
    std::string hb = state.fresh();
    state.emit(hb + " = implies_intro " + h2);
    std::string r = state.fresh();
    state.emit(r + " = iff_intro " + hf + ", " + hb);
    return r;
}

std::string MmTranslator::emit_transport(const std::string& outer_handle,
                                          const std::string& formula,
                                          ProofState& state) {
    std::string h_a = state.fresh();
    state.emit(h_a + " = assume " + formula);
    std::string h_id = state.fresh();
    state.emit(h_id + " = implies_intro " + h_a);
    std::string h_local = state.fresh();
    state.emit(h_local + " = implies_elim " + h_id + ", " + outer_handle);
    return h_local;
}

// ===================================================================
// Inline ND proofs for Hilbert axioms
// ===================================================================

// ax-1: A -> (B -> A)
std::string MmTranslator::inline_ax1(const std::string& a,
                                      const std::string& b,
                                      ProofState& state) {
    std::string ha = state.fresh();
    std::string hb = state.fresh();
    std::string hab = state.fresh();
    std::string ha2 = state.fresh();
    std::string h_inner = state.fresh();
    std::string h_outer = state.fresh();

    state.emit(ha + " = assume " + a);
    state.emit(hb + " = assume " + b);
    state.emit(hab + " = and_intro " + ha + ", " + hb);
    state.emit(ha2 + " = and_elim_l " + hab);
    state.emit(h_inner + " = implies_intro " + ha2);
    state.emit(h_outer + " = implies_intro " + h_inner);
    return h_outer;
}

// ax-2: (A -> (B -> C)) -> ((A -> B) -> (A -> C))
std::string MmTranslator::inline_ax2(const std::string& a,
                                      const std::string& b,
                                      const std::string& c,
                                      ProofState& state) {
    std::string h1 = state.fresh();
    std::string h2 = state.fresh();
    std::string h3 = state.fresh();
    std::string h4 = state.fresh();
    std::string h5 = state.fresh();
    std::string h6 = state.fresh();
    std::string h7 = state.fresh();
    std::string h8 = state.fresh();
    std::string h9 = state.fresh();

    state.emit(h1 + " = assume " + a + " -> (" + b + " -> " + c + ")");
    state.emit(h2 + " = assume " + a + " -> " + b);
    state.emit(h3 + " = assume " + a);
    state.emit(h4 + " = implies_elim " + h1 + ", " + h3);
    state.emit(h5 + " = implies_elim " + h2 + ", " + h3);
    state.emit(h6 + " = implies_elim " + h4 + ", " + h5);
    state.emit(h7 + " = implies_intro " + h6);
    state.emit(h8 + " = implies_intro " + h7);
    state.emit(h9 + " = implies_intro " + h8);
    return h9;
}

// ax-3: (~A -> ~B) -> (B -> A)
std::string MmTranslator::inline_ax3(const std::string& a,
                                      const std::string& b,
                                      ProofState& state) {
    std::string h1 = state.fresh();
    std::string h2 = state.fresh();
    std::string h3 = state.fresh();
    std::string h4 = state.fresh();
    std::string h5 = state.fresh();
    std::string h6 = state.fresh();
    std::string h7 = state.fresh();
    std::string h8 = state.fresh();
    std::string h9 = state.fresh();

    state.emit(h1 + " = assume " + neg(a) + " -> " + neg(b));
    state.emit(h2 + " = assume " + b);
    state.emit(h3 + " = assume " + neg(a));
    state.emit(h4 + " = implies_elim " + h1 + ", " + h3);
    state.emit(h5 + " = not_elim " + h4 + ", " + h2);
    state.emit(h6 + " = not_intro " + h5);
    state.emit(h7 + " = double_neg_elim " + h6);
    state.emit(h8 + " = implies_intro " + h7);
    state.emit(h9 + " = implies_intro " + h8);
    return h9;
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

// --- Helper: skip over one wff subexpression in MM tokens ---
// --- Structural conversion ---
namespace {

struct WffAtom {
    std::string witness_set;
    std::string iff_handle;
    std::string compound_str;
    std::string elem_str;
};

using FrameInfo = MmTranslator::FrameInfo;
using ProofState = MmTranslator::ProofState;

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
            if (it != atoms.end()) return it->second.elem_str;
        }
        if (node.kind == WffNode::Kind::Falsum) {
            auto it = atoms.find("F.");
            if (it != atoms.end()) return it->second.elem_str;
        }
        if (node.kind == WffNode::Kind::Literal) return node.name;
        if (node.kind == WffNode::Kind::Pred) return render_pred(node);
        return "??";
    };
}

// Check if any leaf in the subtree needs iff conversion.
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
        std::string h_witness = state.fresh();
        state.emit(h_witness + " = exists_elim " + h);
        std::string h_conv = convert_proof(*node.left, h_witness, atoms, forward, state);
        std::string r = state.fresh();
        state.emit(r + " = exists_intro " + h_conv);
        return r;
    }

    } // switch kind

    return h; // unreachable
}


// --- Comprehension set builder with iff chaining ---

struct CompResult {
    std::string set_var;       // witness set name or caller's S_var
    std::string iff_handle;    // fully expanded iff, empty if identity
    std::string compound_str;  // FOL string of fully expanded compound form
    int exists_opened = 0;     // number of exists scopes opened
};

static CompResult build_comp_impl(
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
        return {wff_it->second, "", elem_str, 0};
    }

    // Verum (T.) / Falsum (F.) — create witness set via comprehension
    if (tok == "T." || tok == "F.") {
        std::string any_set = caller_info.set_var_order.empty()
            ? caller_info.dummy_var
            : caller_info.set_var_order[0];
        std::string axiom = (tok == "T.") ? "wff_true" : "wff_false";
        std::string h_ax = state.fresh();
        state.emit(h_ax + " = use " + axiom);
        std::string h1 = state.fresh();
        state.emit(h1 + " = forall_elim " + h_ax + ", " + any_set);
        std::string witness = state.fresh() + "_w";
        std::string h2 = state.fresh();
        state.emit(h2 + " = exists_elim " + h1 + ", " + witness);
        std::string axiom_iff = state.fresh();
        state.emit(axiom_iff + " = forall_elim " + h2 + ", " +
                   caller_info.dummy_var);
        std::string taut = "(elem(" + caller_info.dummy_var + ", " +
                           any_set + ") -> elem(" + caller_info.dummy_var +
                           ", " + any_set + "))";
        std::string compound = (tok == "T.") ? taut : neg(taut);
        return {witness, axiom_iff, compound, 1};
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
        state.emit(h2 + " = exists_elim " + h1 + ", " + witness);
        std::string axiom_iff = state.fresh();
        state.emit(axiom_iff + " = forall_elim " + h2 + ", " +
                   caller_info.dummy_var);

        std::string compound = neg(inner.compound_str);
        int total_exists = inner.exists_opened + 1;

        if (inner.iff_handle.empty()) {
            return {witness, axiom_iff, compound, total_exists};
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
        return {witness, chained, compound, total_exists};
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
        if (pos < mm_tokens.size()) pos++; // skip )

        // Build desugared expression and recurse
        Expression desugared;
        Expression arg1(mm_tokens.begin() + arg1_start, mm_tokens.begin() + arg1_end);
        Expression arg2(mm_tokens.begin() + arg2_start, mm_tokens.begin() + arg2_end);
        Expression arg3(mm_tokens.begin() + arg3_start, mm_tokens.begin() + pos - 1);

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

        auto lhs = build_comp_impl(mm_tokens, lhs_start, caller_info, state);
        auto rhs = build_comp_impl(mm_tokens, rhs_start, caller_info, state);
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
        state.emit(h3 + " = exists_elim " + h2 + ", " + witness);
        std::string axiom_iff = state.fresh();
        state.emit(axiom_iff + " = forall_elim " + h3 + ", " +
                   caller_info.dummy_var);

        std::string compound = "(" + lhs.compound_str + fol_op +
                               rhs.compound_str + ")";
        int total_exists = lhs.exists_opened + rhs.exists_opened + 1;

        if (lhs.iff_handle.empty() && rhs.iff_handle.empty()) {
            return {witness, axiom_iff, compound, total_exists};
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
            return {witness, chained, compound, total_exists};
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
            return {witness, chained, compound, total_exists};
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
            return {witness, chained, compound, total_exists};
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
            return {witness, chained, compound, total_exists};
        }

        return {};
    }

    // Setvar: no comprehension needed, use directly.
    // Only accept tokens that are actual setvars from the caller's frame.
    if (std::find(caller_info.setvars.begin(), caller_info.setvars.end(),
                  tok) != caller_info.setvars.end()) {
        return {tok, "", tok, 0};
    }

    // Unknown token (wff variable not in caller's frame, quantifier, etc.)
    return {};
}

}  // anonymous namespace

// --- build_comprehension_set (thin wrapper for header compatibility) ---
std::pair<std::string, std::string> MmTranslator::build_comprehension_set(
    const Expression& mm_tokens, size_t start,
    const FrameInfo& caller_info, ProofState& state) {
    auto cr = build_comp_impl(mm_tokens, start, caller_info, state);
    return {cr.set_var, cr.iff_handle};
}

// --- get_wff_set ---
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

// --- emit_comprehension_use ---
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
    // T./F. reuse the first set variable (matching translate_expr behavior).
    for (const auto& tok : ref_thm.expression) {
        if ((tok == "T." || tok == "F.") && atom_map.find(tok) == atom_map.end()) {
            // T./F. in the ref theorem used ref_info.set_var_order[0] or dummy.
            // Find the corresponding binding's target_set.
            std::string tf_set;
            if (!ref_info.set_var_order.empty()) {
                // Find binding for ref's first set var
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
            wa.iff_handle = "";  // no conversion needed — T./F. is already in compound form
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
                // Compound class substitution — can't use forall_elim
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
    if (step_label == "df-bi") {
        auto it_ph = subst.find("ph");
        auto it_ps = subst.find("ps");
        if (it_ph == subst.end() || it_ps == subst.end()) {
            if (error) *error = "df-bi: missing substitution";
            return false;
        }
        std::string a = translate_expr(it_ph->second, 0, thm_info);
        std::string b = translate_expr(it_ps->second, 0, thm_info);

        // Forward: (A <-> B) -> ~((A -> B) -> ~(B -> A))
        std::string h_fwd_bic = state.fresh();
        state.emit(h_fwd_bic + " = assume " + a + " <-> " + b);
        std::string h_fwd_a = state.fresh();
        state.emit(h_fwd_a + " = assume " + a);
        std::string h_fwd_b1 = state.fresh();
        state.emit(h_fwd_b1 + " = iff_elim_l " + h_fwd_bic + ", " + h_fwd_a);
        std::string h_fwd_ab = state.fresh();
        state.emit(h_fwd_ab + " = implies_intro " + h_fwd_b1);
        std::string h_fwd_b2 = state.fresh();
        state.emit(h_fwd_b2 + " = assume " + b);
        std::string h_fwd_a2 = state.fresh();
        state.emit(h_fwd_a2 + " = iff_elim_r " + h_fwd_bic + ", " + h_fwd_b2);
        std::string h_fwd_ba = state.fresh();
        state.emit(h_fwd_ba + " = implies_intro " + h_fwd_a2);
        std::string h_fwd_neg_a = state.fresh();
        state.emit(h_fwd_neg_a + " = assume (" + a + " -> " + b +
                   ") -> ~(" + b + " -> " + a + ")");
        std::string h_fwd_nba = state.fresh();
        state.emit(h_fwd_nba + " = implies_elim " + h_fwd_neg_a + ", " +
                   h_fwd_ab);
        std::string h_fwd_bot = state.fresh();
        state.emit(h_fwd_bot + " = not_elim " + h_fwd_nba + ", " + h_fwd_ba);
        std::string h_fwd_neg = state.fresh();
        state.emit(h_fwd_neg + " = not_intro " + h_fwd_bot);
        std::string h_fwd = state.fresh();
        state.emit(h_fwd + " = implies_intro " + h_fwd_neg);

        // Backward: ~((A -> B) -> ~(B -> A)) -> (A <-> B)
        std::string h_bwd_h = state.fresh();
        state.emit(h_bwd_h + " = assume ~((" + a + " -> " + b +
                   ") -> ~(" + b + " -> " + a + "))");
        std::string h_nab = state.fresh();
        state.emit(h_nab + " = assume ~(" + a + " -> " + b + ")");
        std::string h_ab_a = state.fresh();
        state.emit(h_ab_a + " = assume " + a + " -> " + b);
        std::string h_ab_bot = state.fresh();
        state.emit(h_ab_bot + " = not_elim " + h_nab + ", " + h_ab_a);
        std::string h_nba_let = state.fresh();
        state.emit(h_nba_let + " = let ~(" + b + " -> " + a + ")");
        std::string h_nba1 = state.fresh();
        state.emit(h_nba1 + " = bottom_elim " + h_ab_bot + ", " + h_nba_let);
        std::string h_ab_impl = state.fresh();
        state.emit(h_ab_impl + " = implies_intro " + h_nba1);
        std::string h_bwd_bot1 = state.fresh();
        state.emit(h_bwd_bot1 + " = not_elim " + h_bwd_h + ", " + h_ab_impl);
        std::string h_nnab = state.fresh();
        state.emit(h_nnab + " = not_intro " + h_bwd_bot1);
        std::string h_ab_proved = state.fresh();
        state.emit(h_ab_proved + " = double_neg_elim " + h_nnab);
        std::string h_nba2 = state.fresh();
        state.emit(h_nba2 + " = assume ~(" + b + " -> " + a + ")");
        std::string h_ba_a = state.fresh();
        state.emit(h_ba_a + " = assume " + a + " -> " + b);
        std::string h_ba_pair = state.fresh();
        state.emit(h_ba_pair + " = and_intro " + h_ba_a + ", " + h_nba2);
        std::string h_nba3 = state.fresh();
        state.emit(h_nba3 + " = and_elim_r " + h_ba_pair);
        std::string h_ba_impl = state.fresh();
        state.emit(h_ba_impl + " = implies_intro " + h_nba3);
        std::string h_bwd_bot2 = state.fresh();
        state.emit(h_bwd_bot2 + " = not_elim " + h_bwd_h + ", " + h_ba_impl);
        std::string h_nnba = state.fresh();
        state.emit(h_nnba + " = not_intro " + h_bwd_bot2);
        std::string h_ba_proved = state.fresh();
        state.emit(h_ba_proved + " = double_neg_elim " + h_nnba);
        std::string h_bic = state.fresh();
        state.emit(h_bic + " = iff_intro " + h_ab_proved + ", " + h_ba_proved);
        std::string h_bwd = state.fresh();
        state.emit(h_bwd + " = implies_intro " + h_bic);

        // h_fwd: (A <-> B) -> ~((A -> B) -> ~(B -> A))
        // h_bwd: ~((A -> B) -> ~(B -> A)) -> (A <-> B)
        // Metamath expects ~(h_fwd -> ~h_bwd), not an iff.
        std::string bi_inner = "((" + a + " -> " + b + ") -> ~(" + b +
                               " -> " + a + "))";
        std::string bi_neg = neg(bi_inner);
        std::string fwd_s = "((" + a + " <-> " + b + ") -> " + bi_neg + ")";
        std::string bwd_s = "(" + bi_neg + " -> (" + a + " <-> " + b + "))";
        std::string h_da = state.fresh();
        state.emit(h_da + " = assume " + fwd_s + " -> " + neg(bwd_s));
        std::string h_neg_bwd = state.fresh();
        state.emit(h_neg_bwd + " = implies_elim " + h_da + ", " + h_fwd);
        std::string h_dfbi_bot = state.fresh();
        state.emit(h_dfbi_bot + " = not_elim " + h_neg_bwd + ", " + h_bwd);
        std::string h_dfbi_neg = state.fresh();
        state.emit(h_dfbi_neg + " = not_intro " + h_dfbi_bot);
        state.stack.push_back({result, h_dfbi_neg});
        return true;
    }

    if (step_label == "df-an") {
        auto it_ph = subst.find("ph");
        auto it_ps = subst.find("ps");
        if (it_ph == subst.end() || it_ps == subst.end()) {
            if (error) *error = "df-an: missing substitution";
            return false;
        }
        std::string a = translate_expr(it_ph->second, 0, thm_info);
        std::string b = translate_expr(it_ps->second, 0, thm_info);

        // Forward: (A & B) -> ~(A -> ~B)
        std::string hf_ab = state.fresh();
        state.emit(hf_ab + " = assume " + a + " & " + b);
        std::string hf_a = state.fresh();
        state.emit(hf_a + " = and_elim_l " + hf_ab);
        std::string hf_b = state.fresh();
        state.emit(hf_b + " = and_elim_r " + hf_ab);
        std::string hf_impl = state.fresh();
        state.emit(hf_impl + " = assume " + a + " -> " + neg(b));
        std::string hf_nb = state.fresh();
        state.emit(hf_nb + " = implies_elim " + hf_impl + ", " + hf_a);
        std::string hf_bot = state.fresh();
        state.emit(hf_bot + " = not_elim " + hf_nb + ", " + hf_b);
        std::string hf_neg = state.fresh();
        state.emit(hf_neg + " = not_intro " + hf_bot);
        std::string hf_fwd = state.fresh();
        state.emit(hf_fwd + " = implies_intro " + hf_neg);

        // Backward: ~(A -> ~B) -> (A & B)
        std::string hb_h = state.fresh();
        state.emit(hb_h + " = assume " + neg("(" + a + " -> " + neg(b) + ")"));
        std::string hb_na = state.fresh();
        state.emit(hb_na + " = assume " + neg(a));
        std::string hb_aa = state.fresh();
        state.emit(hb_aa + " = assume " + a);
        std::string hb_bot1 = state.fresh();
        state.emit(hb_bot1 + " = not_elim " + hb_na + ", " + hb_aa);
        std::string hb_nb_let = state.fresh();
        state.emit(hb_nb_let + " = let ~" + b);
        std::string hb_nb1 = state.fresh();
        state.emit(hb_nb1 + " = bottom_elim " + hb_bot1 + ", " + hb_nb_let);
        std::string hb_impl1 = state.fresh();
        state.emit(hb_impl1 + " = implies_intro " + hb_nb1);
        std::string hb_bot2 = state.fresh();
        state.emit(hb_bot2 + " = not_elim " + hb_h + ", " + hb_impl1);
        std::string hb_nna = state.fresh();
        state.emit(hb_nna + " = not_intro " + hb_bot2);
        std::string hb_a = state.fresh();
        state.emit(hb_a + " = double_neg_elim " + hb_nna);
        std::string hb_nb2 = state.fresh();
        state.emit(hb_nb2 + " = assume " + neg(b));
        std::string hb_aa2 = state.fresh();
        state.emit(hb_aa2 + " = assume " + a);
        std::string hb_pair = state.fresh();
        state.emit(hb_pair + " = and_intro " + hb_aa2 + ", " + hb_nb2);
        std::string hb_nb3 = state.fresh();
        state.emit(hb_nb3 + " = and_elim_r " + hb_pair);
        std::string hb_impl2 = state.fresh();
        state.emit(hb_impl2 + " = implies_intro " + hb_nb3);
        std::string hb_bot3 = state.fresh();
        state.emit(hb_bot3 + " = not_elim " + hb_h + ", " + hb_impl2);
        std::string hb_nnb = state.fresh();
        state.emit(hb_nnb + " = not_intro " + hb_bot3);
        std::string hb_b = state.fresh();
        state.emit(hb_b + " = double_neg_elim " + hb_nnb);
        std::string hb_and = state.fresh();
        state.emit(hb_and + " = and_intro " + hb_a + ", " + hb_b);
        std::string hb_bwd = state.fresh();
        state.emit(hb_bwd + " = implies_intro " + hb_and);

        std::string h_result = state.fresh();
        state.emit(h_result + " = iff_intro " + hf_fwd + ", " + hb_bwd);
        state.stack.push_back({result, h_result});
        return true;
    }

    if (step_label == "df-or") {
        auto it_ph = subst.find("ph");
        auto it_ps = subst.find("ps");
        if (it_ph == subst.end() || it_ps == subst.end()) {
            if (error) *error = "df-or: missing substitution";
            return false;
        }
        std::string a = translate_expr(it_ph->second, 0, thm_info);
        std::string b = translate_expr(it_ps->second, 0, thm_info);

        // Forward: (A | B) -> (~A -> B)
        std::string hf_disj = state.fresh();
        state.emit(hf_disj + " = assume " + a + " | " + b);
        std::string hf_na = state.fresh();
        state.emit(hf_na + " = assume " + neg(a));
        std::string hf_ca = state.fresh();
        state.emit(hf_ca + " = assume " + a);
        std::string hf_ca_bot = state.fresh();
        state.emit(hf_ca_bot + " = not_elim " + hf_na + ", " + hf_ca);
        std::string hf_b_let = state.fresh();
        state.emit(hf_b_let + " = let " + b);
        std::string hf_ca_b = state.fresh();
        state.emit(hf_ca_b + " = bottom_elim " + hf_ca_bot + ", " + hf_b_let);
        std::string hf_ca_impl = state.fresh();
        state.emit(hf_ca_impl + " = implies_intro " + hf_ca_b);
        std::string hf_cb = state.fresh();
        state.emit(hf_cb + " = assume " + b);
        std::string hf_cb_impl = state.fresh();
        state.emit(hf_cb_impl + " = implies_intro " + hf_cb);
        std::string hf_b = state.fresh();
        state.emit(hf_b + " = or_elim " + hf_disj + ", " + hf_ca_impl +
                   ", " + hf_cb_impl);
        std::string hf_inner = state.fresh();
        state.emit(hf_inner + " = implies_intro " + hf_b);
        std::string hf_fwd = state.fresh();
        state.emit(hf_fwd + " = implies_intro " + hf_inner);

        // Backward: (~A -> B) -> (A | B)
        std::string hb_h = state.fresh();
        state.emit(hb_h + " = assume " + neg(a) + " -> " + b);
        std::string hb_em_let = state.fresh();
        state.emit(hb_em_let + " = let " + a);
        std::string hb_em = state.fresh();
        state.emit(hb_em + " = excluded_middle " + hb_em_let);
        std::string hb_ca = state.fresh();
        state.emit(hb_ca + " = assume " + a);
        std::string hb_b_let = state.fresh();
        state.emit(hb_b_let + " = let " + b);
        std::string hb_ca_or = state.fresh();
        state.emit(hb_ca_or + " = or_intro_l " + hb_ca + ", " + hb_b_let);
        std::string hb_ca_impl = state.fresh();
        state.emit(hb_ca_impl + " = implies_intro " + hb_ca_or);
        std::string hb_cna = state.fresh();
        state.emit(hb_cna + " = assume " + neg(a));
        std::string hb_cna_b = state.fresh();
        state.emit(hb_cna_b + " = implies_elim " + hb_h + ", " + hb_cna);
        std::string hb_a_let = state.fresh();
        state.emit(hb_a_let + " = let " + a);
        std::string hb_cna_or = state.fresh();
        state.emit(hb_cna_or + " = or_intro_r " + hb_a_let + ", " + hb_cna_b);
        std::string hb_cna_impl = state.fresh();
        state.emit(hb_cna_impl + " = implies_intro " + hb_cna_or);
        std::string hb_or = state.fresh();
        state.emit(hb_or + " = or_elim " + hb_em + ", " + hb_ca_impl +
                   ", " + hb_cna_impl);
        std::string hb_bwd = state.fresh();
        state.emit(hb_bwd + " = implies_intro " + hb_or);

        std::string h_result = state.fresh();
        state.emit(h_result + " = iff_intro " + hf_fwd + ", " + hb_bwd);
        state.stack.push_back({result, h_result});
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
// Main translation entry point
// ===================================================================

bool MmTranslator::translate(const std::string& label,
                              TranslatedTheorem& result,
                              std::string* error) {
    // Skip theorems with known encoding issues
    static const std::unordered_set<std::string> skip_labels = {
        // T./F. set-variable mismatch (15)
        "alfal", "altru", "bifal", "bitru", "cadtru",
        "dftru2", "dfnot", "falim", "mptru", "nbfal",
        "tbtru", "truan", "trud", "trujust", "trut",
        // hadd/cadd structural conversion (2)
        "cadnot", "hadnot",
        // Quantifier bridge mismatch after vacuous stripping (11)
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
    if (skip_labels.count(label)) {
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

    // --- Bridge-equivalent theorems ---
    // Some Metamath theorems have claims identical to bridge axioms.
    // Generate direct proofs: use bridge + forall_elim + forall_intro.
    {
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
            // axextb: can't bridge (forall z outside claim vs inside biconditional)
        };
        auto it = bridge_equiv.find(label);
        if (it != bridge_equiv.end()) {
            // Strip forall vars from claim
            const WffNode* body = claim_ast.get();
            std::vector<std::string> forall_vars;
            while (body && body->kind == WffNode::Kind::Forall) {
                forall_vars.push_back(body->name);
                body = body->left.get();
            }

            ProofState tstate;
            for (const auto& v : forall_vars)
                tstate.emit("fix " + v);

            // Resolve bridge params to actual variable names
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

            // Close forall scopes
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
    }

    // --- Detect trivial claims (A <-> A) or (A -> A) via AST ---
    {
        // Strip outer forall quantifiers from the full claim AST
        const WffNode* body = claim_ast.get();
        int num_foralls = 0;
        while (body && body->kind == WffNode::Kind::Forall) {
            ++num_foralls;
            body = body->left.get();
        }

        bool is_trivial_iff = false, is_trivial_impl = false;
        std::string trivial_sub;

        // Verum body: renders to (elem(d,s) -> elem(d,s)), trivially provable
        if (body && body->kind == WffNode::Kind::Verum) {
            is_trivial_impl = true;
            std::string s = info.set_var_order.empty()
                ? info.dummy_var : info.set_var_order[0];
            trivial_sub = "elem(" + info.dummy_var + ", " + s + ")";
        }

        if (body && body->kind == WffNode::Kind::Binary) {
            // Compare rendered strings to handle Verum/Falsum equivalences
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

        if (is_trivial_iff || is_trivial_impl) {
            const std::string& sub = trivial_sub;

            ProofState tstate;
            for (const auto& sv : info.used_setvars)
                tstate.emit("fix " + sv);
            if (needs_dummy) {
                for (const auto& sv : info.set_var_order)
                    tstate.emit("fix " + sv);
                tstate.emit("fix " + info.dummy_var);
            }

            if (is_trivial_iff) {
                std::string h1 = tstate.fresh();
                tstate.emit(h1 + " = assume " + sub);
                std::string h2 = tstate.fresh();
                tstate.emit(h2 + " = implies_intro " + h1);
                std::string h3 = tstate.fresh();
                tstate.emit(h3 + " = assume " + sub);
                std::string h4 = tstate.fresh();
                tstate.emit(h4 + " = implies_intro " + h3);
                std::string h5 = tstate.fresh();
                tstate.emit(h5 + " = iff_intro " + h2 + ", " + h4);
                std::string last = h5;
                for (int i = num_foralls - 1; i >= 0; --i) {
                    std::string h = tstate.fresh();
                    tstate.emit(h + " = forall_intro " + last);
                    last = h;
                }
                tstate.emit("qed " + last);
            } else {
                std::string h1 = tstate.fresh();
                tstate.emit(h1 + " = assume " + sub);
                std::string h2 = tstate.fresh();
                tstate.emit(h2 + " = implies_intro " + h1);
                std::string last = h2;
                for (int i = num_foralls - 1; i >= 0; --i) {
                    std::string h = tstate.fresh();
                    tstate.emit(h + " = forall_intro " + last);
                    last = h;
                }
                tstate.emit("qed " + last);
            }

            result.proof_lines = std::move(tstate.lines);
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
