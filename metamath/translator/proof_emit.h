#pragma once

#include "mm_translator.h"

#include <string>
#include <vector>

namespace metamath {

using ProofState = MmTranslator::ProofState;

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

// Inline ND proofs for Hilbert axioms
std::string inline_ax1(const std::string& a, const std::string& b,
                       ProofState& state);
std::string inline_ax2(const std::string& a, const std::string& b,
                       const std::string& c, ProofState& state);
std::string inline_ax3(const std::string& a, const std::string& b,
                       ProofState& state);

// Inline ND proofs for Metamath definition axioms
std::string inline_df_bi(const std::string& a, const std::string& b,
                          ProofState& state);
std::string inline_df_an(const std::string& a, const std::string& b,
                          ProofState& state);
std::string inline_df_or(const std::string& a, const std::string& b,
                          ProofState& state);

// Negate a formula string, adding a space to avoid ~~ tokens.
std::string neg(const std::string& s);

}  // namespace metamath
