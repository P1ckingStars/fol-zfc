#pragma once

#include "mm_translator.h"
#include "wff_ast.h"

#include <string>
#include <unordered_map>

namespace metamath {

using FrameInfo = MmTranslator::FrameInfo;
using ProofState = MmTranslator::ProofState;

// Emit forall_elim of a universally-quantified iff handle at a given variable.
// Returns the instantiated iff handle, or "" if forall_iff is empty.
inline std::string instantiate_forall_iff(
    const std::string& forall_iff,
    const std::string& var,
    ProofState& state) {
    if (forall_iff.empty()) return "";
    std::string h = state.fresh();
    state.emit(h + " = forall_elim " + forall_iff + ", " + var);
    return h;
}

struct WffAtom {
    std::string forall_iff;    // Handle to forall u. (elem(u,S) <-> ...), empty if identity
    std::string compound_str;
    std::string elem_str;
};

struct CompResult {
    std::string set_var;       // witness set name (iota term) or caller's S_var
    std::string forall_iff;    // forall u. (elem(u,set_var) <-> ...), empty if identity
    std::string compound_str;  // FOL string of fully expanded compound form
};

// Leaf renderer using WffAtom compound_str for Var nodes.
LeafRenderer make_compound_renderer(
    const std::unordered_map<std::string, WffAtom>& atoms);

// Leaf renderer using WffAtom elem_str for Var nodes.
LeafRenderer make_elem_renderer(
    const std::unordered_map<std::string, WffAtom>& atoms);

// Check if any leaf in the subtree needs iff conversion.
bool needs_conv(const WffNode& node,
                const std::unordered_map<std::string, WffAtom>& atoms);

// Convert a proof handle between elem-form and compound-form.
// forward=true:  elem-form -> compound-form
// forward=false: compound-form -> elem-form
// test_var: the variable at which to instantiate forall_iff handles
std::string convert_proof(
    const WffNode& node,
    const std::string& h,
    const std::unordered_map<std::string, WffAtom>& atoms,
    bool forward,
    const std::string& test_var,
    ProofState& state);

// Build a comprehension witness set for a compound Metamath expression.
// Returns forall_iff as forall u. (elem(u, set_var) <-> compound(u)).
CompResult build_comp_impl(
    const Expression& mm_tokens, size_t start,
    const FrameInfo& caller_info, ProofState& state);

}  // namespace metamath
