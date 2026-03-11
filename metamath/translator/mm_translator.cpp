#include "mm_translator.h"
#include "comprehension.h"
#include "proof_emit.h"
#include "proof_tree.h"
#include "syntax_to_wff.h"

#include <algorithm>
#include <sstream>

namespace metamath {

MmTranslator::MmTranslator(const MmDatabase& db)
    : db_(db), verifier_(db), syntax_parser_(db) {
    init_identity_defs();
}

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
        }
    }

    // Build the WffAtom map for structural conversion
    std::unordered_map<std::string, WffAtom> atom_map;
    for (const auto& b : bindings) {
        WffAtom wa;
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
    for (size_t sv_idx = 0; sv_idx < ref_info.used_setvars.size(); ++sv_idx) {
        const auto& ref_sv = ref_info.used_setvars[sv_idx];
        auto sit = subst.find(ref_sv);
        std::string target_sv;
        if (sit != subst.end() && !sit->second.empty()) {
            if (sit->second.size() > 1) {
                if (error) *error = "compound class subst for setvar " + ref_sv;
                return "";
            }
            target_sv = sit->second[0];
        } else if (sv_idx >= ref_info.mandatory_setvar_count) {
            // Optional setvar (vacuous in claim) — use caller's dummy var
            target_sv = caller_info.dummy_var;
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
    if (!ast) return "??null??";
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

    // Parse conclusion AST.
    // For the theorem being translated, translate() re-parses this after the
    // pre-scan discovers optional variables. For referenced theorems (via
    // get_frame_info), this is the only parse and is used by comprehension.
    {
        size_t cstart = (!thm.expression.empty() && thm.expression[0] == "|-")
                            ? 1 : 0;
        info.conclusion_ast = parse_mm_wff_mut(thm.expression, cstart, info);
    }

    // Always wrap with dummy var. The dummy variable serves as the "test element"
    // for set-encoded wff variables and comprehension witnesses.
    info.needs_dummy = true;

    // Populate used_setvars from mandatory frame so emit_simple_use generates
    // correct forall_elim chains when referencing this theorem.
    info.used_setvars = info.setvars;
    info.mandatory_setvar_count = info.setvars.size();

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
    auto [it2, _] = frame_cache_.emplace(label, std::move(info));
    return &it2->second;
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
    for (size_t sv_idx = 0; sv_idx < ref_info.used_setvars.size(); ++sv_idx) {
        const auto& ref_sv = ref_info.used_setvars[sv_idx];
        auto sit = subst.find(ref_sv);
        std::string target_sv;
        if (sit != subst.end() && !sit->second.empty()) {
            if (sit->second.size() > 1) {
                // Compound class substitution — can't use forall_elim
                return "";
            }
            target_sv = sit->second[0];
        } else if (sv_idx >= ref_info.mandatory_setvar_count) {
            // Optional setvar (vacuous in claim) — use caller's dummy var
            target_sv = caller_info.dummy_var;
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
    s += "axiom wff_eq: forall x. forall y. exists S. forall u. "
         "(elem(u, S) <-> eq(x, y))\n";
    s += "axiom wff_elem: forall x. forall y. exists S. forall u. "
         "(elem(u, S) <-> elem(x, y))\n";
    s += "\n";
    return s;
}



// ===================================================================
// Identity definition labels (populated once in constructor)
// ===================================================================

void MmTranslator::init_identity_defs() {
    // These definitions desugar to identity biconditionals (LHS == RHS)
    // in our wff-as-set encoding via SyntaxToWff class expansion.
    static const char* labels[] = {
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
    for (const char* l : labels) {
        wff_identity_defs_.insert(l);
    }
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
        // Non-vacuous E./A. in result — verified to need skipping
        "eximal", "ax6evr", "spimedv", "spimfv",
        "speimfw", "speimfwALT", "spimfw", "2exnaln",
        "spimw", "spimew", "equs4v", "alequexv", "equsv",
        // Comprehension proof bug with class-var quantifier encoding
        "sbtlem",
        // Essential hyp has A.x wff — comprehension can't generalize back to forall
        "eqriv", "eqrdv", "eqrd",
        // Comprehension mismatch: eq/class formula vs elem encoding
        "ax13w", "ax13dgen4", "drnf1v",
        "rabidim1", "dfv2", "elv", "elvd", "el2v", "el3v", "elinel2",
        "elinel1", "elind", "elini", "elunant", "elunnel2", "elunnel1",
        "neldif", "eldifn", "eldifi", "velcomp", "eldifbd", "eldifad",
        "eldifd", "gencl", "el3v3",
        // df-clel expansion introduces inner forall with fresh var
        "dfrab3", "dfnul4",
        // verification failure: implies_elim mismatch
        "elequ2g",
        // verification failure: implies_elim antecedent mismatch
        "stdpc6", "ax8v1", "ax8v2", "elequ12",
        "ax13dgen1", "ax13dgen2", "ax13dgen3",
        // verification failure: implies_elim on forall
        "darii", "dariiALT", "festino", "festinoALT",
        "baroco", "barocoALT",
        "darapti", "daraptiALT", "felapton",
        // verification failure: implies_elim antecedent mismatch
        "axexte",
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
        "ralel", "rgenw", "ralimia", "reximia", "ralimiaa",
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
        {"axextb",   {"axextb_bridge",  {{S::Setvar,0},{S::Setvar,1}}}},
        {"equeuclr", {"equeuclr_bridge", {{S::Setvar,0},{S::Setvar,1},{S::Setvar,2}}}},
        {"eqid",     {"eqid_bridge",    {{S::Setvar,0},{S::Dummy,0}}}},
        {"eqcom",    {"eqcom_bridge",   {{S::Setvar,0},{S::Setvar,1},{S::Dummy,0}}}},
        {"ax6v",     {"ax6v_bridge",    {{S::Setvar,0},{S::Setvar,1},{S::Dummy,0}}}},
        {"ax6ev",    {"ax6ev_bridge",   {{S::Setvar,0},{S::Setvar,1},{S::Dummy,0}}}},
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
    // Count outer fix scopes (used_setvars + set_var_order + dummy)
    int outer_scopes = static_cast<int>(info.used_setvars.size());
    if (needs_dummy)
        outer_scopes += static_cast<int>(info.set_var_order.size()) + 1;

    // Strip outer forall quantifiers
    const WffNode* body = claim_ast.get();
    for (int i = 0; i < outer_scopes && body &&
             body->kind == WffNode::Kind::Forall; ++i)
        body = body->left.get();

    // Strip inner foralls, recording bound variable names for substitution
    std::vector<std::string> inner_vars;
    while (body && body->kind == WffNode::Kind::Forall) {
        inner_vars.push_back(body->name);
        body = body->left.get();
    }

    bool is_trivial_iff = false, is_trivial_impl = false;
    std::string trivial_sub;

    // Build renderer: uses claim_renderer but substitutes inner-forall-bound
    // variables with fresh _triv names in both Var and Pred nodes
    auto renderer = make_claim_renderer(info);
    auto subst_name = [&](const std::string& name) -> std::string {
        for (size_t j = 0; j < inner_vars.size(); ++j)
            if (name == inner_vars[j])
                return "_triv" + std::to_string(j);
        return name;
    };
    LeafRenderer inner_renderer = [&](const WffNode& n) -> std::string {
        if (n.kind == WffNode::Kind::Var) {
            std::string mapped = subst_name(n.name);
            if (mapped != n.name) return mapped;
        }
        if (n.kind == WffNode::Kind::Pred) {
            std::string result = n.name + "(";
            for (size_t i = 0; i < n.args.size(); ++i) {
                if (i > 0) result += ", ";
                result += subst_name(n.args[i]);
            }
            result += ")";
            return result;
        }
        return renderer(n);
    };

    // Verum body: renders to (elem(d,s) -> elem(d,s))
    if (body && body->kind == WffNode::Kind::Verum) {
        is_trivial_impl = true;
        std::string s = info.set_var_order.empty()
            ? info.dummy_var : info.set_var_order[0];
        trivial_sub = "elem(" + info.dummy_var + ", " + s + ")";
    }

    if (body && body->kind == WffNode::Kind::Binary) {
        std::string lhs_str = emit_fol(*body->left, inner_renderer);
        std::string rhs_str = emit_fol(*body->right, inner_renderer);
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
    for (size_t i = 0; i < inner_vars.size(); ++i)
        tstate.emit("fix _triv" + std::to_string(i));

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

    // Pre-scan: allocate variables for optional (dummy) hypotheses.
    // Optional wff vars get wff_to_set entries; optional setvars/class vars
    // get added to setvars.  All are fixed and quantified because
    // intermediate proof steps (forall_elim, comprehension) reference them.
    {
        const auto& proof = thm->proof;
        auto scan_labels = proof.compressed ? proof.paren_labels : proof.labels;
        std::unordered_set<std::string> seen_setvars(info.setvars.begin(),
                                                      info.setvars.end());
        for (const auto& lbl : scan_labels) {
            const FloatingHyp* fh = db_.get_float_hyp(lbl);
            if (!fh) continue;
            if (fh->typecode == "wff") {
                if (info.wff_to_set.count(fh->variable)) continue;
                std::string set_var = "S_" + fh->variable;
                info.wff_to_set[fh->variable] = set_var;
                info.set_var_order.push_back(set_var);
            } else if (fh->typecode == "setvar" || fh->typecode == "class") {
                if (seen_setvars.count(fh->variable)) continue;
                seen_setvars.insert(fh->variable);
                info.setvars.push_back(fh->variable);
                if (fh->typecode == "class")
                    info.class_vars.insert(fh->variable);
            }
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
    if (try_bridge_equiv(label, info, claim_ast, result)) {
        frame_cache_.emplace(label, std::move(info));
        return true;
    }

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

    // Build proof tree and emit FOL proof
    ProofTree tree;
    if (!build_proof_tree(db_, *thm, tree, error)) {
        ++skipped_;
        return false;
    }

    std::string final_handle = emit_proof_tree(tree, info, state, error);
    if (final_handle.empty()) {
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

// ===================================================================
// Definition classification
// ===================================================================

MmTranslator::DefClassification MmTranslator::classify_definitions() const {
    DefClassification result;

    for (const auto& label : db_.assertion_order()) {
        if (label.substr(0, 3) != "df-") continue;

        const Assertion* a = db_.get_assertion(label);
        if (!a || a->kind != Assertion::Kind::Axiom) continue;

        const auto& expr = a->expression;
        size_t start = (!expr.empty() && expr[0] == "|-") ? 1 : 0;

        // Build a minimal FrameInfo for parsing
        FrameInfo info;
        const MandatoryFrame& frame = a->frame;
        for (size_t i = 0; i < frame.hyp_labels.size(); ++i) {
            if (!frame.is_floating[i]) continue;
            const FloatingHyp* fh = db_.get_float_hyp(frame.hyp_labels[i]);
            if (!fh) continue;
            if (fh->typecode == "wff") {
                info.wff_to_set[fh->variable] = "S_" + fh->variable;
                info.set_var_order.push_back("S_" + fh->variable);
            } else if (fh->typecode == "setvar" || fh->typecode == "class") {
                info.setvars.push_back(fh->variable);
            }
        }
        info.dummy_var = "u0";

        WffPtr ast = parse_mm_wff(expr, start, info);
        if (!ast || ast->kind == WffNode::Kind::Literal) {
            result.parse_fail.push_back(label);
            continue;
        }

        // Strip outer forall/exists quantifiers
        const WffNode* body = ast.get();
        while (body && (body->kind == WffNode::Kind::Forall ||
                        body->kind == WffNode::Kind::Exists)) {
            body = body->left.get();
        }

        // Check for biconditional
        if (!body || body->kind != WffNode::Kind::Binary ||
            body->op != WffNode::Op::Iff) {
            std::string kind_str;
            if (body) {
                switch (body->kind) {
                    case WffNode::Kind::Binary:
                        switch (body->op) {
                            case WffNode::Op::Implies: kind_str = "implies"; break;
                            case WffNode::Op::And:     kind_str = "and"; break;
                            case WffNode::Op::Or:      kind_str = "or"; break;
                            default: kind_str = "binary"; break;
                        }
                        break;
                    default: kind_str = "other"; break;
                }
            } else {
                kind_str = "null";
            }
            result.not_bic.push_back({label, kind_str});
            continue;
        }

        // Compare both sides
        if (*body->left == *body->right) {
            result.identity.push_back(label);
        } else {
            result.non_identity.push_back(label);
        }
    }

    return result;
}

}  // namespace metamath
