#include "proof_emit.h"

namespace metamath {

std::string neg(const std::string& s) {
    if (!s.empty() && s[0] == '~') return "~ " + s;
    return "~" + s;
}

std::string emit_bridge_use(const std::string& bridge_name,
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

std::string emit_identity_bic(const std::string& formula,
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

std::string emit_transport(const std::string& outer_handle,
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

// ax-1: A -> (B -> A)
std::string inline_ax1(const std::string& a, const std::string& b,
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
std::string inline_ax2(const std::string& a, const std::string& b,
                       const std::string& c, ProofState& state) {
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
std::string inline_ax3(const std::string& a, const std::string& b,
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

// df-bi: (A <-> B) <-> ~((A -> B) -> ~(B -> A))
// Returns handle to ~(fwd -> ~bwd) in Metamath's encoding.
std::string inline_df_bi(const std::string& a, const std::string& b,
                          ProofState& state) {
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

    // Metamath expects ~(fwd -> ~bwd), not an iff.
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
    return h_dfbi_neg;
}

// df-an: (A & B) <-> ~(A -> ~B)
std::string inline_df_an(const std::string& a, const std::string& b,
                          ProofState& state) {
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
    return h_result;
}

// df-or: (A | B) <-> (~A -> B)
std::string inline_df_or(const std::string& a, const std::string& b,
                          ProofState& state) {
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
    return h_result;
}

// IMPORTANT: captures `info` by reference; caller must ensure it outlives the lambda.
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
            case WffNode::Kind::Verum:
                return "(elem(" + info.dummy_var + ", " + info.dummy_var +
                       ") -> elem(" + info.dummy_var + ", " + info.dummy_var + "))";
            case WffNode::Kind::Falsum:
                return "~(elem(" + info.dummy_var + ", " + info.dummy_var +
                       ") -> elem(" + info.dummy_var + ", " + info.dummy_var + "))";

            default:
                return "??";
        }
    };
}

}  // namespace metamath
