#pragma once

#include "../parser/mm_database.h"
#include "../verifier/mm_verifier.h"
#include "syntax_parser.h"
#include "wff_ast.h"

#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace metamath {

struct TranslatedTheorem {
    std::string mm_label;
    std::string fol_label;
    std::string claim_formula;
    std::vector<std::string> proof_lines;
};

class MmTranslator {
public:
    explicit MmTranslator(const MmDatabase& db);

    // Translate a single theorem. Returns false on error/unsupported.
    bool translate(const std::string& label,
                   TranslatedTheorem& result,
                   std::string* error = nullptr);

    // Emit .fol.def content for translated theorems
    static std::string emit_def(const std::vector<TranslatedTheorem>& thms);

    // Emit .fol.proof content for translated theorems
    static std::string emit_proof(const std::vector<TranslatedTheorem>& thms);

    // Get generated instantiation lemmas (must be emitted before main theorems)
    const std::vector<TranslatedTheorem>& lemmas() const { return lemmas_; }

    // Emit comprehension axioms needed by the translation
    static std::string emit_comprehension_axioms();

    size_t translated_count() const { return translated_set_.size(); }
    size_t skipped_count() const { return skipped_; }

    // Clear internal caches to free memory (call between batches)
    void clear_caches() { frame_cache_.clear(); }

    // Pre-populate translated_set_ with known theorem labels (for per-batch processes)
    void add_known_theorems(const std::vector<std::string>& labels) {
        for (const auto& l : labels) translated_set_.insert(l);
    }

    struct FrameInfo {
        // wff variable → set variable mapping (e.g. "ph" → "S_ph")
        std::unordered_map<std::string, std::string> wff_to_set;

        // Ordered set variable names for outer forall quantifiers
        std::vector<std::string> set_var_order;

        // Dummy setvar for 0-ary wff encoding (e.g. "u0")
        std::string dummy_var;

        // Setvar variables from the frame (includes class vars treated as setvars)
        std::vector<std::string> setvars;

        // Class variables (subset of setvars — those from $f class declarations)
        std::unordered_set<std::string> class_vars;

        // Setvars that actually appear in the FOL formula (subset of setvars)
        std::vector<std::string> used_setvars;

        // Whether the claim formula includes the dummy var quantifier
        bool needs_dummy = true;

        // Essential hypotheses in frame order
        struct EssHyp {
            std::string mm_label;
            std::string fol_formula;  // translated
            WffPtr ast;               // parsed AST
        };
        std::vector<EssHyp> ess_hyps;

        // Conclusion AST (parsed from assertion expression)
        WffPtr conclusion_ast;

        // Copy of frame data for proof simulation
        std::vector<std::string> hyp_labels;
        std::vector<bool> is_floating;
    };

    struct StackEntry {
        Expression expr;     // Metamath expression (with typecode)
        std::string handle;  // FOL-ZFC handle ("" = type-checking only)
    };

    struct ProofState {
        std::vector<StackEntry> stack;
        std::vector<StackEntry> saved;   // compressed proof saves
        std::vector<std::string> lines;
        int counter = 0;

        std::string fresh() { return "h" + std::to_string(++counter); }
        void emit(const std::string& line) { lines.push_back("    " + line); }
    };

private:
    const MmDatabase& db_;
    MmVerifier verifier_;
    SyntaxParser syntax_parser_;
    std::unordered_set<std::string> translated_set_;
    size_t skipped_ = 0;

    bool build_frame_info(const Assertion& thm, FrameInfo& info,
                          std::string* error);

    // Expression translation
    std::string translate_expr(const Expression& tokens, size_t start,
                               const FrameInfo& info) const;

    // Parse Metamath expression tokens into a WffNode AST
    WffPtr parse_mm_wff(const Expression& tokens, size_t start,
                        const FrameInfo& info) const;

    // Mutable variant: tracks extra variables from class expansion
    WffPtr parse_mm_wff_mut(const Expression& tokens, size_t start,
                            FrameInfo& info);

    bool simulate_proof(const Assertion& thm, const FrameInfo& info_in,
                        ProofState& state, std::string* error);

    // Process one assertion application during proof simulation
    bool apply_step(const std::string& step_label,
                    const FrameInfo& thm_info,
                    ProofState& state,
                    std::string* error);

    // Inline ND proofs for Hilbert axioms
    // Returns handle name of the result formula
    std::string inline_ax1(const std::string& a, const std::string& b,
                           ProofState& state);
    std::string inline_ax2(const std::string& a, const std::string& b,
                           const std::string& c, ProofState& state);
    std::string inline_ax3(const std::string& a, const std::string& b,
                           ProofState& state);

    // ---------------------------------------------------------------
    // Hybrid theorem reference system
    // ---------------------------------------------------------------

    // Frame cache: avoids rebuilding FrameInfo for referenced theorems
    std::unordered_map<std::string, FrameInfo> frame_cache_;

    // Get or build FrameInfo for a referenced theorem (cached)
    const FrameInfo* get_frame_info(const std::string& label,
                                     const Assertion& thm,
                                     std::string* error);

    // Check if a substitution is "simple": every wff var maps to a
    // single wff variable (not a compound expression)
    bool is_simple_substitution(
        const Assertion& ref_thm,
        const std::map<std::string, Expression>& subst,
        const FrameInfo& caller_info) const;

    // Simple path: use + forall_elim + implies_elim
    std::string emit_simple_use(
        const std::string& ref_label,
        const Assertion& ref_thm,
        const FrameInfo& ref_info,
        const FrameInfo& caller_info,
        const std::map<std::string, Expression>& subst,
        const std::vector<std::string>& ess_handles,
        ProofState& state);

    // ---------------------------------------------------------------
    // Comprehension-based compound substitution
    // ---------------------------------------------------------------

    // For each compound wff var, create a witness set via comprehension
    // axiom, then forall_elim + structural conversion.
    // Returns handle to the result formula.
    std::string emit_comprehension_use(
        const std::string& ref_label,
        const Assertion& ref_thm,
        const FrameInfo& ref_info,
        const FrameInfo& caller_info,
        const std::map<std::string, Expression>& subst,
        const std::vector<std::string>& ess_handles,
        ProofState& state,
        std::string* error);

    // Build a comprehension witness set for a compound Metamath expression.
    // Returns (witness_set_var, iff_handle) where iff connects
    // elem(dummy, witness) <-> compound_formula
    static std::pair<std::string, std::string> build_comprehension_set(
        const Expression& mm_tokens, size_t start,
        const FrameInfo& caller_info, ProofState& state);

    // Unused lemma storage (kept for interface compatibility)
    std::vector<TranslatedTheorem> lemmas_;

    // Use a bridge theorem and instantiate with forall_elim chain
    std::string emit_bridge_use(const std::string& bridge_name,
                                const std::vector<std::string>& args,
                                ProofState& state);

    // Emit trivial A <-> A biconditional via two identity implications
    std::string emit_identity_bic(const std::string& formula,
                                  ProofState& state);

    // Re-derive formula in current fix scope via identity implication
    std::string emit_transport(const std::string& outer_handle,
                               const std::string& formula,
                               ProofState& state);

    // Resolve wff substitution to its corresponding set variable.
    std::string get_wff_set(const Expression& wff_expr,
                            const FrameInfo& thm_info,
                            ProofState& state);

    // Utility
    bool is_syntax_builder(const Assertion* a) const;
    static std::string sanitize_label(const std::string& mm_label);
};

}  // namespace metamath
