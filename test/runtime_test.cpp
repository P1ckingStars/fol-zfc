// Runtime tests and ordered pair proofs

#include "../src/runtime/runtime.h"
#include "../src/parser/parser.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <functional>
#include <vector>

using namespace logic;

// ==================== Test Framework ====================

struct TestResult {
    std::string name;
    bool passed;
    std::string message;
};

std::vector<TestResult> test_results;

void run_test(const std::string& name, std::function<bool()> test_fn) {
    std::cout << "Running: " << name << "... ";
    try {
        bool result = test_fn();
        if (result) {
            std::cout << "PASSED\n";
            test_results.push_back({name, true, ""});
        } else {
            std::cout << "FAILED\n";
            test_results.push_back({name, false, "Test returned false"});
        }
    } catch (const std::exception& e) {
        std::cout << "FAILED (exception: " << e.what() << ")\n";
        test_results.push_back({name, false, e.what()});
    }
}

void print_summary() {
    int passed = 0, failed = 0;
    for (const auto& r : test_results) {
        if (r.passed) passed++;
        else failed++;
    }

    std::cout << "\n════════════════════════════════════════════════════════\n";
    std::cout << "Test Summary: " << passed << " passed, " << failed << " failed\n";
    std::cout << "════════════════════════════════════════════════════════\n";

    if (failed > 0) {
        std::cout << "\nFailed tests:\n";
        for (const auto& r : test_results) {
            if (!r.passed) {
                std::cout << "  - " << r.name << ": " << r.message << "\n";
            }
        }
    }
}

// ==================== Runtime Tests ====================

bool test_runtime_load() {
    Runtime rt;
    auto result = rt.load(R"(
        axiom all_P: forall x. P(x)
        axiom all_P_impl_Q: forall x. (P(x) -> Q(x))
        claim all_Q: forall x. Q(x)
    )");

    if (!result.ok()) {
        std::cout << "[" << result.error() << "] ";
        return false;
    }

    auto stmts = result.value();
    if (stmts.size() != 3) {
        std::cout << "[expected 3 statements, got " << stmts.size() << "] ";
        return false;
    }

    // Check axioms are registered
    if (!rt.context().find_axiom("all_P").has_value()) {
        std::cout << "[all_P not found] ";
        return false;
    }
    if (!rt.context().find_axiom("all_P_impl_Q").has_value()) {
        std::cout << "[all_P_impl_Q not found] ";
        return false;
    }

    return true;
}

bool test_runtime_load_file() {
    Runtime rt;
    auto result = rt.load_file_recursive("zfc/basics/ordered_pair.fol.def");

    if (!result.ok()) {
        std::cout << "[" << result.error() << "] ";
        return false;
    }

    auto& parsed = result.value();
    std::cout << "[" << parsed.statements.size() << " statements] ";

    // Check some axioms are loaded
    if (!rt.context().find_axiom("singleton_def").has_value()) {
        std::cout << "[singleton_def not found] ";
        return false;
    }
    if (!rt.context().find_axiom("pair_def").has_value()) {
        std::cout << "[pair_def not found] ";
        return false;
    }

    return true;
}

bool test_simple_proof() {
    // Prove: forall x. (P(x) -> P(x)) (identity)
    Runtime rt;
    rt.load(R"(
        axiom dummy: forall x. P(x)
        claim identity: forall x. (P(x) -> P(x))
    )");

    auto ctx = rt.prove("identity");

    // Fix variable x
    Term x = ctx.fix_var();

    // Get P(x) by eliminating forall from dummy axiom
    auto dummy = ctx.use("dummy");
    if (!dummy.ok()) {
        std::cout << "[use dummy failed] ";
        return false;
    }
    auto p_x = ctx.forall_elim(dummy.value(), x);
    if (!p_x.ok()) {
        std::cout << "[forall_elim failed] ";
        return false;
    }

    // Assume P(x)
    ctx.assume(p_x.value());

    // Derive P(x) -> P(x)
    auto impl = ctx.implies_intro(p_x.value());
    if (!impl.ok()) {
        std::cout << "[implies_intro failed: " << impl.error() << "] ";
        return false;
    }

    // Generalize to forall x. (P(x) -> P(x))
    auto result = ctx.forall_intro(impl.value());
    if (!result.ok()) {
        std::cout << "[forall_intro failed: " << result.error() << "] ";
        return false;
    }

    auto qed_result = ctx.qed(result.value());
    if (!qed_result.ok()) {
        std::cout << "[qed failed: " << qed_result.error() << "] ";
        return false;
    }

    std::cout << "[" << result.value().get().to_string() << "] ";
    return true;
}

bool test_modus_ponens_proof() {
    // Given forall x. P(x) and forall x. (P(x) -> Q(x)), prove forall x. Q(x)
    Runtime rt;
    rt.load(R"(
        axiom all_P: forall x. P(x)
        axiom all_P_impl_Q: forall x. (P(x) -> Q(x))
        claim all_Q: forall x. Q(x)
    )");

    auto ctx = rt.prove("all_Q");

    // Fix variable
    Term x = ctx.fix_var();

    auto all_p = ctx.use("all_P");
    if (!all_p.ok()) {
        std::cout << "[use all_P failed: " << all_p.error() << "] ";
        return false;
    }

    auto all_p_impl_q = ctx.use("all_P_impl_Q");
    if (!all_p_impl_q.ok()) {
        std::cout << "[use all_P_impl_Q failed: " << all_p_impl_q.error() << "] ";
        return false;
    }

    // Instantiate with fixed var
    auto p_x = ctx.forall_elim(all_p.value(), x);
    if (!p_x.ok()) {
        std::cout << "[forall_elim failed] ";
        return false;
    }
    auto pq_x = ctx.forall_elim(all_p_impl_q.value(), x);
    if (!pq_x.ok()) {
        std::cout << "[forall_elim failed] ";
        return false;
    }

    auto q_x = ctx.implies_elim(pq_x.value(), p_x.value());
    if (!q_x.ok()) {
        std::cout << "[implies_elim failed: " << q_x.error() << "] ";
        return false;
    }

    // Generalize
    auto all_q = ctx.forall_intro(q_x.value());
    if (!all_q.ok()) {
        std::cout << "[forall_intro failed: " << all_q.error() << "] ";
        return false;
    }

    auto qed_result = ctx.qed(all_q.value());
    if (!qed_result.ok()) {
        std::cout << "[qed failed: " << qed_result.error() << "] ";
        return false;
    }

    // Check theorem is registered
    if (!rt.context().find_theorem("all_Q").has_value()) {
        std::cout << "[all_Q not registered as theorem] ";
        return false;
    }

    std::cout << "[all_Q proven] ";
    return true;
}

bool test_forall_elim_proof() {
    // Given forall x. P(x) and forall x. (P(x) -> Q(x)), prove forall x. Q(x)
    // All axioms and claims must be sentences (no free variables)
    Runtime rt;
    rt.load(R"(
        axiom all_P: forall x. P(x)
        axiom all_P_impl_Q: forall x. (P(x) -> Q(x))
        claim all_Q: forall x. Q(x)
    )");

    auto ctx = rt.prove("all_Q");

    // Fix a variable x
    Term x = ctx.fix_var();

    auto all_p = ctx.use("all_P");
    if (!all_p.ok()) {
        std::cout << "[use all_P failed] ";
        return false;
    }

    auto all_pq = ctx.use("all_P_impl_Q");
    if (!all_pq.ok()) {
        std::cout << "[use all_P_impl_Q failed] ";
        return false;
    }

    // Instantiate forall x. P(x) with fixed var x: P(x)
    auto p_x = ctx.forall_elim(all_p.value(), x);
    if (!p_x.ok()) {
        std::cout << "[forall_elim all_P failed: " << p_x.error() << "] ";
        return false;
    }

    // Instantiate forall x. (P(x) -> Q(x)) with fixed var x: P(x) -> Q(x)
    auto pq_x = ctx.forall_elim(all_pq.value(), x);
    if (!pq_x.ok()) {
        std::cout << "[forall_elim all_P_impl_Q failed: " << pq_x.error() << "] ";
        return false;
    }

    // Apply modus ponens: from P(x) -> Q(x) and P(x), derive Q(x)
    auto q_x = ctx.implies_elim(pq_x.value(), p_x.value());
    if (!q_x.ok()) {
        std::cout << "[implies_elim failed: " << q_x.error() << "] ";
        return false;
    }

    // Generalize: from Q(x) derive forall x. Q(x)
    auto all_q = ctx.forall_intro(q_x.value());
    if (!all_q.ok()) {
        std::cout << "[forall_intro failed: " << all_q.error() << "] ";
        return false;
    }

    // Complete the proof
    auto qed_result = ctx.qed(all_q.value());
    if (!qed_result.ok()) {
        std::cout << "[qed failed: " << qed_result.error() << "] ";
        return false;
    }

    // Check theorem is registered
    if (!rt.context().find_theorem("all_Q").has_value()) {
        std::cout << "[all_Q not registered as theorem] ";
        return false;
    }

    std::cout << "[forall x. Q(x) proven via forall_elim/forall_intro] ";
    return true;
}

// ==================== Proof Syntax Tests ====================

bool test_proof_syntax_parsing() {
    Runtime rt;

    // Load with proofs - all formulas must be sentences (no free variables)
    auto result = rt.load_with_proofs(R"(
        axiom all_P: forall x. P(x)
        axiom all_P_impl_Q: forall x. (P(x) -> Q(x))
        claim all_Q: forall x. Q(x)

        proof all_Q:
            fix x
            h1 = use all_P
            h2 = forall_elim h1, x
            h3 = use all_P_impl_Q
            h4 = forall_elim h3, x
            h5 = implies_elim h4, h2
            h6 = forall_intro h5
            qed h6
    )");

    if (!result.ok()) {
        std::cout << "[" << result.error() << "] ";
        return false;
    }

    auto& parsed = result.value();
    if (parsed.statements.size() != 3) {
        std::cout << "[expected 3 statements, got " << parsed.statements.size() << "] ";
        return false;
    }
    if (parsed.proofs.size() != 1) {
        std::cout << "[expected 1 proof, got " << parsed.proofs.size() << "] ";
        return false;
    }
    if (parsed.proofs[0].claim_name != "all_Q") {
        std::cout << "[expected proof for all_Q] ";
        return false;
    }
    if (parsed.proofs[0].steps.size() != 8) {
        std::cout << "[expected 8 steps, got " << parsed.proofs[0].steps.size() << "] ";
        return false;
    }

    std::cout << "[parsed 3 statements, 1 proof with 8 steps] ";
    return true;
}

bool test_proof_execution() {
    Runtime rt;

    // Load with proofs - all formulas must be sentences
    auto result = rt.load_with_proofs(R"(
        axiom all_P: forall x. P(x)
        axiom all_P_impl_Q: forall x. (P(x) -> Q(x))
        claim all_Q: forall x. Q(x)

        proof all_Q:
            fix x
            h1 = use all_P
            h2 = forall_elim h1, x
            h3 = use all_P_impl_Q
            h4 = forall_elim h3, x
            h5 = implies_elim h4, h2
            h6 = forall_intro h5
            qed h6
    )");

    if (!result.ok()) {
        std::cout << "[parse error: " << result.error() << "] ";
        return false;
    }

    // Execute the proof
    auto exec_result = rt.execute_all_proofs(result.value());
    if (!exec_result.ok()) {
        std::cout << "[execution error: " << exec_result.error() << "] ";
        return false;
    }

    // Check all_Q is now a theorem
    if (!rt.context().find_theorem("all_Q").has_value()) {
        std::cout << "[all_Q not registered as theorem] ";
        return false;
    }

    std::cout << "[all_Q proven via proof syntax] ";
    return true;
}

bool test_proof_with_forall() {
    Runtime rt;

    // Same as test_proof_execution but tests forall_elim specifically
    auto result = rt.load_with_proofs(R"(
        axiom all_P: forall x. P(x)
        axiom all_P_impl_Q: forall x. (P(x) -> Q(x))
        claim all_Q: forall x. Q(x)

        proof all_Q:
            fix x
            h1 = use all_P
            h2 = forall_elim h1, x
            h3 = use all_P_impl_Q
            h4 = forall_elim h3, x
            h5 = implies_elim h4, h2
            h6 = forall_intro h5
            qed h6
    )");

    if (!result.ok()) {
        std::cout << "[parse error: " << result.error() << "] ";
        return false;
    }

    auto exec_result = rt.execute_all_proofs(result.value());
    if (!exec_result.ok()) {
        std::cout << "[execution error: " << exec_result.error() << "] ";
        return false;
    }

    if (!rt.context().find_theorem("all_Q").has_value()) {
        std::cout << "[all_Q not registered as theorem] ";
        return false;
    }

    std::cout << "[all_Q proven with forall_elim] ";
    return true;
}

bool test_proof_with_and() {
    Runtime rt;

    auto result = rt.load_with_proofs(R"(
        axiom all_P: forall x. P(x)
        axiom all_Q: forall x. Q(x)
        claim all_PQ: forall x. (P(x) & Q(x))

        proof all_PQ:
            fix x
            h1 = use all_P
            h2 = forall_elim h1, x
            h3 = use all_Q
            h4 = forall_elim h3, x
            h5 = and_intro h2, h4
            h6 = forall_intro h5
            qed h6
    )");

    if (!result.ok()) {
        std::cout << "[parse error: " << result.error() << "] ";
        return false;
    }

    auto exec_result = rt.execute_all_proofs(result.value());
    if (!exec_result.ok()) {
        std::cout << "[execution error: " << exec_result.error() << "] ";
        return false;
    }

    if (!rt.context().find_theorem("all_PQ").has_value()) {
        std::cout << "[all_PQ not registered as theorem] ";
        return false;
    }

    std::cout << "[all_PQ proven with and_intro] ";
    return true;
}

bool test_proof_with_fix_and_forall_intro() {
    // Prove: from forall x. P(x) and forall x. (P(x) -> Q(x)), derive forall x. Q(x)
    // This tests fix_var, forall_elim, and forall_intro
    Runtime rt;

    auto result = rt.load_with_proofs(R"(
        axiom all_P: forall x. P(x)
        axiom all_P_impl_Q: forall x. (P(x) -> Q(x))
        claim all_Q: forall x. Q(x)

        proof all_Q:
            fix x
            h1 = use all_P
            h2 = forall_elim h1, x
            h3 = use all_P_impl_Q
            h4 = forall_elim h3, x
            h5 = implies_elim h4, h2
            h6 = forall_intro h5
            qed h6
    )");

    if (!result.ok()) {
        std::cout << "[parse error: " << result.error() << "] ";
        return false;
    }

    auto exec_result = rt.execute_all_proofs(result.value());
    if (!exec_result.ok()) {
        std::cout << "[execution error: " << exec_result.error() << "] ";
        return false;
    }

    if (!rt.context().find_theorem("all_Q").has_value()) {
        std::cout << "[all_Q not registered as theorem] ";
        return false;
    }

    std::cout << "[forall x. Q(x) proven with fix/forall_intro] ";
    return true;
}

bool test_let_does_not_derive() {
    // let should only create a formula handle, not derive it.
    // A proof using only let + qed must fail.
    Runtime rt;

    auto result = rt.load_with_proofs(R"(
        claim fake: forall x. Q(x)

        proof fake:
            h = let forall x. Q(x)
            qed h
    )");

    if (!result.ok()) {
        std::cout << "[parse error: " << result.error() << "] ";
        return false;
    }

    auto exec_result = rt.execute_all_proofs(result.value());
    if (exec_result.ok()) {
        std::cout << "[SOUNDNESS BUG: let formula accepted by qed without derivation] ";
        return false;
    }

    // The proof should fail because the let formula is not derived
    std::cout << "[correctly rejected: " << exec_result.error() << "] ";
    return true;
}

// ==================== @def Annotation Tests ====================

bool test_def_annotation_basic() {
    // @def(P) axiom should parse and register correctly
    Runtime rt;
    auto result = rt.load_with_proofs(R"(
        @def(P) axiom p_def: forall x. (P(x) <-> Q(x))
    )");

    if (!result.ok()) {
        std::cout << "[parse error: " << result.error() << "] ";
        return false;
    }

    auto& parsed = result.value();
    if (parsed.statements.size() != 1) {
        std::cout << "[expected 1 statement, got " << parsed.statements.size() << "] ";
        return false;
    }

    // Should be registered as axiom
    if (!rt.context().find_axiom("p_def").has_value()) {
        std::cout << "[p_def not registered as axiom] ";
        return false;
    }

    // Should be marked as definition
    if (!rt.context().is_defined("P")) {
        std::cout << "[P not marked as defined] ";
        return false;
    }

    // def_predicate field should be set
    if (parsed.statements[0].def_predicate != "P") {
        std::cout << "[def_predicate not set] ";
        return false;
    }

    std::cout << "[@def(P) axiom parsed and registered] ";
    return true;
}

bool test_def_annotation_redefinition() {
    // Redefining the same predicate should error
    Runtime rt;
    auto result = rt.load_with_proofs(R"(
        @def(P) axiom p_def1: forall x. (P(x) <-> Q(x))
        @def(P) axiom p_def2: forall x. (P(x) <-> R(x))
    )");

    if (result.ok()) {
        std::cout << "[should have failed but succeeded] ";
        return false;
    }

    std::string err = result.error().message();
    if (err.find("already defined") == std::string::npos) {
        std::cout << "[unexpected error: " << err << "] ";
        return false;
    }

    std::cout << "[correctly rejected: " << err << "] ";
    return true;
}

bool test_def_annotation_missing_predicate() {
    // @def(P) on axiom not mentioning P should error
    Runtime rt;
    auto result = rt.load_with_proofs(R"(
        @def(P) axiom bad_def: forall x. (Q(x) <-> R(x))
    )");

    if (result.ok()) {
        std::cout << "[should have failed but succeeded] ";
        return false;
    }

    std::string err = result.error().message();
    if (err.find("does not mention") == std::string::npos) {
        std::cout << "[unexpected error: " << err << "] ";
        return false;
    }

    std::cout << "[correctly rejected: " << err << "] ";
    return true;
}

bool test_def_annotation_plain_axiom_unchanged() {
    // Plain axioms should still work and not be marked as definitions
    Runtime rt;
    auto result = rt.load_with_proofs(R"(
        axiom ext: forall x. forall y. (eq(x, y) <-> forall z. (elem(z, x) <-> elem(z, y)))
    )");

    if (!result.ok()) {
        std::cout << "[parse error: " << result.error() << "] ";
        return false;
    }

    if (!rt.context().find_axiom("ext").has_value()) {
        std::cout << "[ext not registered] ";
        return false;
    }

    if (rt.context().is_defined("eq")) {
        std::cout << "[eq should not be marked as defined] ";
        return false;
    }

    if (!result.value().statements[0].def_predicate.empty()) {
        std::cout << "[def_predicate should be empty for plain axiom] ";
        return false;
    }

    std::cout << "[plain axiom unchanged] ";
    return true;
}

bool test_def_annotation_with_proof() {
    // @def axiom should work in proofs just like regular axioms
    Runtime rt;
    auto result = rt.load_with_proofs(R"(
        @def(P) axiom p_def: forall x. (P(x) <-> Q(x))
        axiom all_Q: forall x. Q(x)
        claim all_P: forall x. P(x)

        proof all_P:
            fix x
            h1 = use all_Q
            h2 = forall_elim h1, x
            pd = use p_def
            pd1 = forall_elim pd, x
            h3 = iff_elim_r pd1, h2
            h4 = forall_intro h3
            qed h4
    )");

    if (!result.ok()) {
        std::cout << "[parse error: " << result.error() << "] ";
        return false;
    }

    auto exec = rt.execute_all_proofs(result.value());
    if (!exec.ok()) {
        std::cout << "[execution error: " << exec.error() << "] ";
        return false;
    }

    if (!rt.context().find_theorem("all_P").has_value()) {
        std::cout << "[all_P not proven] ";
        return false;
    }

    std::cout << "[proof with @def axiom works] ";
    return true;
}

// ==================== Ordered Pair Proofs ====================

// Helper to load ordered pair axioms
Runtime& load_ordered_pair_axioms() {
    static Runtime rt;
    static bool loaded = false;
    if (!loaded) {
        auto result = rt.load_file_recursive("zfc/basics/ordered_pair.fol.def");
        if (!result.ok()) {
            throw std::runtime_error("Failed to load ordered_pair.fol.def: " + result.error().to_string());
        }
        loaded = true;
    }
    return rt;
}

// The ordered pair proofs require complex quantifier manipulations.
// Each proof involves:
// 1. Loading axioms from zfc/ordered_pair.fol
// 2. Using forall_elim to instantiate quantified axioms
// 3. Applying iff_elim to extract biconditional directions
// 4. Using and_intro/and_elim for conjunctions
// 5. Using implies_intro/implies_elim for implications
//
// Example proof sketch for singleton_in_pair:
//   forall p. forall a. forall b. forall s. ((pair(p, a, b) & singleton(s, a)) -> elem(s, p))
//
// 1. fix_var for p, a, b, s
// 2. Assume pair(p, a, b) & singleton(s, a)
// 3. From pair_def: pair(p, a, b) <-> forall z. (elem(z, p) <-> φ(z))
// 4. From singleton_def: singleton(s, a) <-> forall z. (elem(z, s) <-> eq(z, a))
// 5. Show s satisfies φ(s) using the singleton definition
// 6. Apply iff_elim to get elem(s, p)
// 7. Use implies_intro to complete the implication
// 8. Use forall_intro (4 times) to close all quantifiers

bool test_ordered_pair_axioms_loaded() {
    Runtime& rt = load_ordered_pair_axioms();

    // Verify all axioms are present (definitions + extensionality from axioms.fol)
    std::vector<std::string> required_axioms = {
        "singleton_def", "doubleton_def", "pair_def", "extensionality"
    };

    for (const auto& name : required_axioms) {
        if (!rt.context().find_axiom(name).has_value()) {
            std::cout << "[" << name << " missing] ";
            return false;
        }
    }

    // Verify claims are registered
    std::vector<std::string> claims = {
        "eq_refl", "eq_sym", "eq_trans", "eq_elem_l", "eq_elem_r",
        "pair_injective", "singleton_injective", "singleton_eq_doubleton",
        "doubleton_eq", "pair_elems", "singleton_in_pair", "doubleton_in_pair"
    };

    for (const auto& name : claims) {
        if (!rt.context().find_claim(name).has_value()) {
            std::cout << "[claim " << name << " missing] ";
            return false;
        }
    }

    std::cout << "[axioms and claims loaded] ";
    return true;
}

bool test_execute_ordered_pair_proofs() {
    Runtime rt;

    // Load the header (includes axioms.fol.def recursively)
    auto header_result = rt.load_file_recursive("zfc/basics/ordered_pair.fol.def");
    if (!header_result.ok()) {
        std::cout << "[header load error: " << header_result.error() << "] ";
        return false;
    }
    rt.execute_all_proofs(header_result.value());

    // Load the proof file
    std::ifstream proof_file("zfc/basics/ordered_pair.fol.proof");
    if (!proof_file.is_open()) {
        std::cout << "[could not open proof file] ";
        return false;
    }
    std::stringstream buffer;
    buffer << proof_file.rdbuf();

    std::string error;
    auto proof_result = try_parse_with_proofs(buffer.str(), rt.context(), &error);
    if (!error.empty()) {
        std::cout << "[parse error: " << error << "] ";
        return false;
    }

    // Execute all proofs
    auto exec_result = rt.execute_all_proofs(proof_result);
    if (!exec_result.ok()) {
        std::cout << "[execution error: " << exec_result.error() << "] ";
        return false;
    }

    // Check that proofs registered theorems
    std::vector<std::string> expected_theorems = {
        "eq_refl", "eq_elem_l", "pair_elems", "singleton_in_pair", "singleton_injective", "singleton_eq_doubleton", "doubleton_eq", "doubleton_in_pair",
        "pair_first_eq", "pair_second_neq", "pair_second_degenerate", "pair_injective"
    };

    for (const auto& name : expected_theorems) {
        if (!rt.context().find_theorem(name).has_value()) {
            std::cout << "[" << name << " not proven] ";
            return false;
        }
    }

    std::cout << "[all proofs verified] ";
    return true;
}

bool test_fol_test_file() {
    Runtime rt;

    // Load the full header chain (replacement_choice includes functions includes ordered_pair includes axioms)
    auto header_result = rt.load_file_recursive("zfc/basics/replacement_choice.fol.def");
    if (!header_result.ok()) {
        std::cout << "[header load error: " << header_result.error() << "] ";
        return false;
    }
    rt.execute_all_proofs(header_result.value());

    // Load and execute ordered_pair proofs
    std::ifstream op_file("zfc/basics/ordered_pair.fol.proof");
    if (!op_file.is_open()) {
        std::cout << "[could not open ordered_pair.fol.proof] ";
        return false;
    }
    std::stringstream op_buf;
    op_buf << op_file.rdbuf();
    std::string op_error;
    auto op_result = try_parse_with_proofs(op_buf.str(), rt.context(), &op_error);
    if (!op_error.empty()) {
        std::cout << "[op parse error: " << op_error << "] ";
        return false;
    }
    auto op_exec = rt.execute_all_proofs(op_result);
    if (!op_exec.ok()) {
        std::cout << "[op execution error: " << op_exec.error() << "] ";
        return false;
    }

    // Load and execute functions proofs
    std::ifstream fn_file("zfc/basics/functions.fol.proof");
    if (!fn_file.is_open()) {
        std::cout << "[could not open functions.fol.proof] ";
        return false;
    }
    std::stringstream fn_buf;
    fn_buf << fn_file.rdbuf();
    std::string fn_error;
    auto fn_result = try_parse_with_proofs(fn_buf.str(), rt.context(), &fn_error);
    if (!fn_error.empty()) {
        std::cout << "[fn parse error: " << fn_error << "] ";
        return false;
    }
    auto fn_exec = rt.execute_all_proofs(fn_result);
    if (!fn_exec.ok()) {
        std::cout << "[fn execution error: " << fn_exec.error() << "] ";
        return false;
    }

    std::cout << "[all proofs verified] ";
    return true;
}

// ==================== Definite Description (Iota) Tests ====================

bool test_iota_elim_basic() {
    // From ∃x.P(x), derive P(ιx.P(x))
    Runtime rt;

    auto result = rt.load_with_proofs(R"(
        axiom exists_p: exists x. P(x)
        claim p_of_iota: P((iota x_0. P(x_0)))
    )");
    if (!result.ok()) {
        std::cout << "[parse error: " << result.error() << "] ";
        return false;
    }

    auto pctx = rt.prove("p_of_iota");
    auto h_exists = pctx.use("exists_p");
    if (!h_exists.ok()) { std::cout << "[use error] "; return false; }

    auto h_result = pctx.iota_elim(h_exists.value());
    if (!h_result.ok()) { std::cout << "[iota_elim error: " << h_result.error() << "] "; return false; }

    auto qed = pctx.qed(h_result.value());
    if (!qed.ok()) { std::cout << "[qed error: " << qed.error() << "] "; return false; }

    return true;
}

bool test_iota_elim_syntax() {
    // Test iota_elim via proof syntax
    Runtime rt;

    auto result = rt.load_with_proofs(R"(
        axiom exists_p: exists x. P(x)
        claim p_of_iota: P((iota x_0. P(x_0)))

        proof p_of_iota:
            h1 = use exists_p
            h2 = iota_elim h1
            qed h2
    )");
    if (!result.ok()) {
        std::cout << "[parse error: " << result.error() << "] ";
        return false;
    }

    auto exec = rt.execute_all_proofs(result.value());
    if (!exec.ok()) {
        std::cout << "[execution error: " << exec.error() << "] ";
        return false;
    }

    return true;
}

bool test_iota_elim_with_forall() {
    // Use iota term in forall_elim
    // From ∃x.P(x) and ∀y.Q(y), derive Q(ιx.P(x))
    Runtime rt;

    auto result = rt.load_with_proofs(R"(
        axiom exists_p: exists x. P(x)
        axiom all_q: forall y. Q(y)
        claim q_of_iota: Q((iota x_0. P(x_0)))

        proof q_of_iota:
            h1 = use exists_p
            h2 = iota_elim h1, t
            h3 = use all_q
            h4 = forall_elim h3, t
            qed h4
    )");
    if (!result.ok()) {
        std::cout << "[parse error: " << result.error() << "] ";
        return false;
    }

    auto exec = rt.execute_all_proofs(result.value());
    if (!exec.ok()) {
        std::cout << "[execution error: " << exec.error() << "] ";
        return false;
    }

    return true;
}

bool test_iota_elim_binary_predicate() {
    // From ∃x.R(x,x), derive R(ιx.R(x,x), ιx.R(x,x))
    Runtime rt;

    auto result = rt.load_with_proofs(R"(
        axiom exists_r: exists x. R(x, x)
        claim r_of_iota: R((iota x_0. R(x_0, x_0)), (iota x_0. R(x_0, x_0)))

        proof r_of_iota:
            h1 = use exists_r
            h2 = iota_elim h1
            qed h2
    )");
    if (!result.ok()) {
        std::cout << "[parse error: " << result.error() << "] ";
        return false;
    }

    auto exec = rt.execute_all_proofs(result.value());
    if (!exec.ok()) {
        std::cout << "[execution error: " << exec.error() << "] ";
        return false;
    }

    return true;
}

bool test_iota_elim_nested_quantifier() {
    // From ∃x.∀y.R(x,y), derive ∀y.R(ιx.∀y.R(x,y), y)
    // Uses C++ API because de Bruijn indices differ between
    // parsed and derived formulas for nested quantifiers
    Runtime rt;

    auto result = rt.load(R"(
        axiom exists_forall_r: exists x. forall y. R(x, y)
    )");
    if (!result.ok()) {
        std::cout << "[parse error: " << result.error() << "] ";
        return false;
    }

    auto pctx = rt.prove(SentenceHandle{});
    auto h = pctx.use("exists_forall_r");
    if (!h.ok()) { std::cout << "[use error] "; return false; }

    auto h2 = pctx.iota_elim(h.value());
    if (!h2.ok()) {
        std::cout << "[iota_elim error: " << h2.error() << "] ";
        return false;
    }

    // Verify the result has the expected structure
    std::string formula_str = h2.value().get().to_string();
    std::string expected = "forall x_0. R((iota x_1. forall x_0. R(x_1, x_0)), x_0)";
    if (formula_str != expected) {
        std::cout << "[unexpected: " << formula_str << " != " << expected << "] ";
        return false;
    }

    return true;
}

bool test_iota_elim_not_exists_error() {
    // iota_elim should fail on non-existential
    Runtime rt;

    auto result = rt.load(R"(
        axiom all_p: forall x. P(x)
    )");
    if (!result.ok()) {
        std::cout << "[parse error: " << result.error() << "] ";
        return false;
    }

    auto pctx = rt.prove(SentenceHandle{});  // no specific goal
    auto h = pctx.use("all_p");
    if (!h.ok()) { std::cout << "[use error] "; return false; }

    auto fail = pctx.iota_elim(h.value());
    if (fail.ok()) {
        std::cout << "[should have failed] ";
        return false;
    }

    return true;
}

bool test_iota_term_to_string() {
    // Verify the string representation of iota terms
    Runtime rt;

    auto result = rt.load(R"(
        axiom exists_p: exists x. P(x)
    )");
    if (!result.ok()) return false;

    auto pctx = rt.prove(SentenceHandle{});
    auto h = pctx.use("exists_p");
    auto iota_result = pctx.iota_elim(h.value());
    if (!iota_result.ok()) return false;

    std::string formula_str = iota_result.value().get().to_string();
    // Should be P(ιx_0. P(x_0)) printed as P((iota x_0. P(x_0)))
    if (formula_str != "P((iota x_0. P(x_0)))") {
        std::cout << "[unexpected string: " << formula_str << "] ";
        return false;
    }

    return true;
}

bool test_iota_in_eq_subst() {
    // Use iota term with eq_subst
    // From ∃x.P(x) and eq(a,b) and P(a), derive P(b) via eq_subst
    // Then from ∃y.Q(y), use iota term in forall_elim on ∀z.(z=z)
    Runtime rt;

    auto result = rt.load_with_proofs(R"(
        axiom exists_eq: exists x. eq(x, x)
        axiom all_refl: forall x. eq(x, x)
        claim iota_eq: eq((iota x_0. eq(x_0, x_0)), (iota x_0. eq(x_0, x_0)))

        proof iota_eq:
            h1 = use exists_eq
            h2 = iota_elim h1, t
            h3 = use all_refl
            h4 = forall_elim h3, t
            qed h4
    )");
    if (!result.ok()) {
        std::cout << "[parse error: " << result.error() << "] ";
        return false;
    }

    auto exec = rt.execute_all_proofs(result.value());
    if (!exec.ok()) {
        std::cout << "[execution error: " << exec.error() << "] ";
        return false;
    }

    return true;
}

bool test_iota_elim_capture_avoidance() {
    // Tests that forall_elim correctly handles capture avoidance when the
    // formula contains an iota term whose body reuses the same generalized index.
    //
    // This is the exact pattern the translator needs for comprehension:
    //   axiom: ∀A.∀B.∃C.∀u.(elem(u,C) <-> (elem(u,A) -> elem(u,B)))
    //   After forall_elim(A,B) and iota_elim, the outer ∀u and inner ∀u
    //   (inside the iota body) use the same generalized index.
    //   forall_elim on the outer ∀u must NOT substitute inside the iota body.
    Runtime rt;

    auto result = rt.load(R"(
        axiom comp: forall A. forall B. exists C. forall u. (R(u, C) <-> (R(u, A) -> R(u, B)))
    )");
    if (!result.ok()) { std::cout << "[load error: " << result.error() << "] "; return false; }

    auto pctx = rt.prove(SentenceHandle{});

    // fix A, B
    auto A = pctx.fix_var();
    auto B = pctx.fix_var();

    // use comp; forall_elim with A, B
    auto h_comp = pctx.use("comp");
    if (!h_comp.ok()) { std::cout << "[use error] "; return false; }
    auto h1 = pctx.forall_elim(h_comp.value(), A);
    if (!h1.ok()) { std::cout << "[fe1 error: " << h1.error() << "] "; return false; }
    auto h2 = pctx.forall_elim(h1.value(), B);
    if (!h2.ok()) { std::cout << "[fe2 error: " << h2.error() << "] "; return false; }

    // iota_elim: from ∃C.∀u.(...) get ∀u.(R(u, ιC.∀u.(...)) <-> ...)
    auto h3 = pctx.iota_elim(h2.value());
    if (!h3.ok()) { std::cout << "[iota error: " << h3.error() << "] "; return false; }

    // fix u; forall_elim with u — this is the critical step.
    // Without capture avoidance, this would incorrectly substitute inside
    // the iota body's ∀u, corrupting the term.
    auto u = pctx.fix_var();
    auto h4 = pctx.forall_elim(h3.value(), u);
    if (!h4.ok()) { std::cout << "[fe3 error: " << h4.error() << "] "; return false; }

    // Verify the result: R(u, ιC.∀u.(...)) <-> (R(u, A) -> R(u, B))
    // Extract both sides of the iff to verify structure
    const auto& f = h4.value().get();
    if (!f.is_compound() || f.as_compound().op != logic::Op::Iff) {
        std::cout << "[expected iff, got: " << f << "] ";
        return false;
    }

    // The left side should contain the iota term
    std::string s = f.to_string();
    if (s.find("(iota") == std::string::npos) {
        std::cout << "[expected iota term in result: " << s << "] ";
        return false;
    }

    return true;
}

// ==================== Schema Tests ====================

bool test_schema_declare_prove_instantiate() {
    // Declare schema S [ph, ps]: (ph -> (ps -> ph))
    // Prove it, then schema_inst with concrete formulas
    Runtime rt;

    auto result = rt.load_with_proofs(R"(
        schema S [ph, ps]: (ph -> (ps -> ph))

        proof S:
            h1 = assume ph
            h2 = assume ps
            h3 = implies_intro h1
            h4 = implies_intro h3
            qed h4

        claim test_inst: forall x. forall y. forall a. (eq(x, y) -> (P(a) -> eq(x, y)))

        proof test_inst:
            fix x
            fix y
            fix a
            h = schema_inst S { ph: eq(x, y), ps: P(a) }
            h1 = forall_intro h
            h2 = forall_intro h1
            h3 = forall_intro h2
            qed h3
    )");

    if (!result.ok()) {
        std::cout << "[parse error: " << result.error() << "] ";
        return false;
    }

    auto exec = rt.execute_all_proofs(result.value());
    if (!exec.ok()) {
        std::cout << "[execution error: " << exec.error() << "] ";
        return false;
    }

    // Schema should be proven
    if (!rt.context().is_schema_proven("S")) {
        std::cout << "[S not marked as proven] ";
        return false;
    }

    // Claim should be proven as theorem
    if (!rt.context().find_theorem("test_inst").has_value()) {
        std::cout << "[test_inst not proven] ";
        return false;
    }

    std::cout << "[schema declared, proved, instantiated] ";
    return true;
}

bool test_schema_inst_before_proof_fails() {
    // schema_inst before the schema is proved should fail
    Runtime rt;

    auto result = rt.load_with_proofs(R"(
        schema S [ph]: (ph -> ph)
        claim bad: (P -> P)

        proof bad:
            h = schema_inst S { ph: P }
            qed h
    )");

    if (!result.ok()) {
        std::cout << "[parse error: " << result.error() << "] ";
        return false;
    }

    auto exec = rt.execute_all_proofs(result.value());
    if (exec.ok()) {
        std::cout << "[SOUNDNESS BUG: schema_inst succeeded before schema was proven] ";
        return false;
    }

    std::string err = exec.error().message();
    if (err.find("not yet proven") == std::string::npos) {
        std::cout << "[unexpected error: " << err << "] ";
        return false;
    }

    std::cout << "[correctly rejected] ";
    return true;
}

bool test_schema_missing_binding_fails() {
    // Missing a binding in schema_inst should fail
    Runtime rt;

    auto result = rt.load_with_proofs(R"(
        schema S [ph, ps]: (ph -> (ps -> ph))

        proof S:
            h1 = assume ph
            h2 = assume ps
            h3 = implies_intro h1
            h4 = implies_intro h3
            qed h4

        claim bad: P

        proof bad:
            h = schema_inst S { ph: P }
            qed h
    )");

    if (!result.ok()) {
        std::cout << "[parse error: " << result.error() << "] ";
        return false;
    }

    auto exec = rt.execute_all_proofs(result.value());
    if (exec.ok()) {
        std::cout << "[should have failed with missing binding] ";
        return false;
    }

    std::string err = exec.error().message();
    if (err.find("missing binding") == std::string::npos) {
        std::cout << "[unexpected error: " << err << "] ";
        return false;
    }

    std::cout << "[correctly rejected: missing binding] ";
    return true;
}

bool test_schema_to_string() {
    // Verify SchemaVar serialization
    Runtime rt;

    auto result = rt.load_with_proofs(R"(
        schema S [ph, ps]: (ph -> (ps -> ph))
    )");

    if (!result.ok()) {
        std::cout << "[parse error: " << result.error() << "] ";
        return false;
    }

    auto schema = rt.context().find_schema("S");
    if (!schema.has_value()) {
        std::cout << "[schema S not found] ";
        return false;
    }

    std::string body_str = schema->body.get().to_string();
    std::cout << "[" << body_str << "] ";
    return body_str == "?0 -> ?1 -> ?0";
}

bool test_schema_with_quantifiers() {
    // Schema with quantifiers in body
    Runtime rt;

    auto result = rt.load_with_proofs(R"(
        schema ax1 [ph, ps]: (ph -> (ps -> ph))

        proof ax1:
            h1 = assume ph
            h2 = assume ps
            h3 = implies_intro h1
            h4 = implies_intro h3
            qed h4

        claim inst: forall x. forall y. (eq(x, y) -> (P(x) -> eq(x, y)))

        proof inst:
            fix x
            fix y
            h = schema_inst ax1 { ph: eq(x, y), ps: P(x) }
            h1 = forall_intro h
            h2 = forall_intro h1
            qed h2
    )");

    if (!result.ok()) {
        std::cout << "[parse error: " << result.error() << "] ";
        return false;
    }

    auto exec = rt.execute_all_proofs(result.value());
    if (!exec.ok()) {
        std::cout << "[execution error: " << exec.error() << "] ";
        return false;
    }

    std::cout << "[schema with quantifier instantiation works] ";
    return true;
}

bool test_schema_connectives_in_proof() {
    // Prove a schema using connective rules, then instantiate
    Runtime rt;

    auto result = rt.load_with_proofs(R"(
        schema id [ph]: (ph -> ph)

        proof id:
            h = assume ph
            h1 = implies_intro h
            qed h1

        claim use_id: forall x. (P(x) -> P(x))

        proof use_id:
            fix x
            h = schema_inst id { ph: P(x) }
            h1 = forall_intro h
            qed h1
    )");

    if (!result.ok()) {
        std::cout << "[parse error: " << result.error() << "] ";
        return false;
    }

    auto exec = rt.execute_all_proofs(result.value());
    if (!exec.ok()) {
        std::cout << "[execution error: " << exec.error() << "] ";
        return false;
    }

    std::cout << "[schema identity proved and instantiated] ";
    return true;
}

// ==================== Predicate Schema Tests ====================

bool test_predicate_schema_arity1() {
    // P(1) means P is a unary predicate, substituted with \x. formula
    Runtime rt;
    auto result = rt.load_with_proofs(R"(
        schema allE [P(1)]: forall a. (forall x. P(x)) -> P(a)

        proof allE:
            fix a
            h = assume forall x. P(x)
            h1 = forall_elim h, a
            h2 = implies_intro h1
            h3 = forall_intro h2
            qed h3

        claim test: forall a. (forall x. elem(x, x)) -> elem(a, a)

        proof test:
            h = schema_inst allE { P: \x. elem(x, x) }
            qed h
    )");

    if (!result.ok()) {
        std::cout << "[parse error: " << result.error() << "] ";
        return false;
    }
    auto exec = rt.execute_all_proofs(result.value());
    if (!exec.ok()) {
        std::cout << "[execution error: " << exec.error() << "] ";
        return false;
    }
    if (!rt.context().find_theorem("test").has_value()) {
        std::cout << "[test not proven] ";
        return false;
    }
    std::cout << "[arity-1 predicate schema works] ";
    return true;
}

bool test_predicate_schema_arity2() {
    // R(2) is a binary predicate — test with a provable schema
    Runtime rt;
    auto result = rt.load_with_proofs(R"(
        schema refl2 [R(2)]: forall x. forall y. (R(x, y) -> R(x, y))

        proof refl2:
            fix x
            fix y
            h = assume R(x, y)
            h1 = implies_intro h
            h2 = forall_intro h1
            h3 = forall_intro h2
            qed h3

        claim test: forall x. forall y. (eq(x, y) -> eq(x, y))

        proof test:
            h = schema_inst refl2 { R: \x y. eq(x, y) }
            qed h
    )");

    if (!result.ok()) {
        std::cout << "[parse error: " << result.error() << "] ";
        return false;
    }
    auto exec = rt.execute_all_proofs(result.value());
    if (!exec.ok()) {
        std::cout << "[execution error: " << exec.error() << "] ";
        return false;
    }
    std::cout << "[arity-2 predicate schema works] ";
    return true;
}

bool test_predicate_schema_mixed_arity() {
    // Mix arity-0 (formula) and arity-1 (predicate) in one schema
    // Schema: (forall x. P(x)) -> ph -> (forall x. P(x))
    Runtime rt;
    auto result = rt.load_with_proofs(R"(
        schema mixed [ph, P(1)]: (forall x. P(x)) -> ph -> (forall x. P(x))

        proof mixed:
            h1 = assume forall x. P(x)
            h2 = assume ph
            h3 = implies_intro h1
            h4 = implies_intro h3
            qed h4

        claim test: (forall x. R(x)) -> Q -> (forall x. R(x))

        proof test:
            h = schema_inst mixed { ph: Q, P: \x. R(x) }
            qed h
    )");

    if (!result.ok()) {
        std::cout << "[parse error: " << result.error() << "] ";
        return false;
    }
    auto exec = rt.execute_all_proofs(result.value());
    if (!exec.ok()) {
        std::cout << "[execution error: " << exec.error() << "] ";
        return false;
    }
    std::cout << "[mixed arity schema works] ";
    return true;
}

bool test_predicate_schema_to_string() {
    // Verify SchemaVar with args prints correctly
    Runtime rt;
    auto result = rt.load_with_proofs(R"(
        schema S [P(1)]: forall x. P(x)
    )");
    if (!result.ok()) {
        std::cout << "[parse error: " << result.error() << "] ";
        return false;
    }
    auto schema = rt.context().find_schema("S");
    if (!schema.has_value()) {
        std::cout << "[schema not found] ";
        return false;
    }
    std::string s = schema->body.get().to_string();
    std::cout << "[" << s << "] ";
    // Should contain ?0 applied to a generalized var
    return s.find("?0") != std::string::npos;
}

bool test_predicate_schema_backward_compat() {
    // Existing arity-0 syntax must still work unchanged
    Runtime rt;
    auto result = rt.load_with_proofs(R"(
        schema S [ph, ps]: (ph -> (ps -> ph))

        proof S:
            h1 = assume ph
            h2 = assume ps
            h3 = implies_intro h1
            h4 = implies_intro h3
            qed h4

        claim test: (P -> (Q -> P))

        proof test:
            h = schema_inst S { ph: P, ps: Q }
            qed h
    )");

    if (!result.ok()) {
        std::cout << "[parse error: " << result.error() << "] ";
        return false;
    }
    auto exec = rt.execute_all_proofs(result.value());
    if (!exec.ok()) {
        std::cout << "[execution error: " << exec.error() << "] ";
        return false;
    }
    std::cout << "[backward compatible] ";
    return true;
}

// ==================== Main ====================

int main() {
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════╗\n";
    std::cout << "║              RUNTIME TEST SUITE                        ║\n";
    std::cout << "╚════════════════════════════════════════════════════════╝\n\n";

    // Basic runtime tests
    std::cout << "── Basic Runtime Tests ──\n";
    run_test("Load axioms from string", test_runtime_load);
    run_test("Load axioms from file", test_runtime_load_file);

    // Proof tests
    std::cout << "\n── Proof Tests ──\n";
    run_test("Simple proof (identity)", test_simple_proof);
    run_test("Modus ponens proof", test_modus_ponens_proof);
    run_test("Forall elimination", test_forall_elim_proof);

    // Proof syntax tests
    std::cout << "\n── Proof Syntax Tests ──\n";
    run_test("Parse proof syntax", test_proof_syntax_parsing);
    run_test("Execute proof", test_proof_execution);
    run_test("Proof with forall_elim", test_proof_with_forall);
    run_test("Proof with and_intro", test_proof_with_and);
    run_test("Proof with fix and forall_intro", test_proof_with_fix_and_forall_intro);
    run_test("Let does not derive (soundness)", test_let_does_not_derive);

    // @def annotation tests
    std::cout << "\n── @def Annotation Tests ──\n";
    run_test("@def annotation basic", test_def_annotation_basic);
    run_test("@def redefinition error", test_def_annotation_redefinition);
    run_test("@def missing predicate error", test_def_annotation_missing_predicate);
    run_test("Plain axiom unchanged", test_def_annotation_plain_axiom_unchanged);
    run_test("@def annotation with proof", test_def_annotation_with_proof);

    // Ordered pair proofs
    std::cout << "\n── Ordered Pair Proofs ──\n";
    run_test("Load ordered pair axioms and claims", test_ordered_pair_axioms_loaded);
    run_test("Execute ordered pair proofs", test_execute_ordered_pair_proofs);

    // Definite description (iota) tests
    std::cout << "\n── Definite Description (Iota) Tests ──\n";
    run_test("iota_elim basic (C++ API)", test_iota_elim_basic);
    run_test("iota_elim proof syntax", test_iota_elim_syntax);
    run_test("iota_elim with forall_elim", test_iota_elim_with_forall);
    run_test("iota_elim binary predicate", test_iota_elim_binary_predicate);
    run_test("iota_elim nested quantifier", test_iota_elim_nested_quantifier);
    run_test("iota_elim error on non-existential", test_iota_elim_not_exists_error);
    run_test("iota term to_string", test_iota_term_to_string);
    run_test("iota term in eq context", test_iota_in_eq_subst);
    run_test("iota_elim capture avoidance", test_iota_elim_capture_avoidance);

    // Schema tests
    std::cout << "\n── Schema Tests ──\n";
    run_test("Schema declare, prove, instantiate", test_schema_declare_prove_instantiate);
    run_test("Schema inst before proof fails", test_schema_inst_before_proof_fails);
    run_test("Schema missing binding fails", test_schema_missing_binding_fails);
    run_test("Schema to_string", test_schema_to_string);
    run_test("Schema with quantifiers", test_schema_with_quantifiers);
    run_test("Schema connectives in proof", test_schema_connectives_in_proof);

    // Predicate schema tests
    std::cout << "\n── Predicate Schema Tests ──\n";
    run_test("Predicate schema arity-1", test_predicate_schema_arity1);
    run_test("Predicate schema arity-2", test_predicate_schema_arity2);
    run_test("Predicate schema mixed arity", test_predicate_schema_mixed_arity);
    run_test("Predicate schema to_string", test_predicate_schema_to_string);
    run_test("Predicate schema backward compat", test_predicate_schema_backward_compat);

    // Integration test
    std::cout << "\n── Integration Test ──\n";
    run_test("Load and verify full proof chain", test_fol_test_file);

    // Print summary
    print_summary();

    // Return non-zero if any test failed
    for (const auto& r : test_results) {
        if (!r.passed) return 1;
    }
    return 0;
}
