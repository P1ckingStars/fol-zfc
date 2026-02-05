// Runtime tests and ordered pair proofs

#include "../src/runtime/runtime.h"

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
    auto result = rt.load_file("zfc/ordered_pair.fol");

    if (!result.ok()) {
        std::cout << "[" << result.error() << "] ";
        return false;
    }

    auto stmts = result.value();
    std::cout << "[" << stmts.size() << " statements] ";

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

// ==================== Ordered Pair Proofs ====================

// Helper to load ordered pair axioms
Runtime& load_ordered_pair_axioms() {
    static Runtime rt;
    static bool loaded = false;
    if (!loaded) {
        auto result = rt.load_file("zfc/ordered_pair.fol");
        if (!result.ok()) {
            throw std::runtime_error("Failed to load ordered_pair.fol: " + result.error().to_string());
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

    // Verify all axioms are present
    std::vector<std::string> required_axioms = {
        "singleton_def", "doubleton_def", "pair_def",
        "eq_refl", "eq_sym", "eq_trans",
        "eq_elem_l", "eq_elem_r", "extensionality"
    };

    for (const auto& name : required_axioms) {
        if (!rt.context().find_axiom(name).has_value()) {
            std::cout << "[" << name << " missing] ";
            return false;
        }
    }

    // Verify claims are registered
    std::vector<std::string> claims = {
        "pair_injective", "singleton_injective", "singleton_eq_doubleton",
        "doubleton_eq", "pair_elems", "singleton_in_pair", "doubleton_in_pair"
    };

    for (const auto& name : claims) {
        if (!rt.context().find_claim(name).has_value()) {
            std::cout << "[claim " << name << " missing] ";
            return false;
        }
    }

    std::cout << "[9 axioms, 7 claims loaded] ";
    return true;
}

bool test_execute_ordered_pair_proofs() {
    Runtime rt;

    // Load the ordered_pair.fol file with proofs
    auto result = rt.load_file_with_proofs("zfc/ordered_pair.fol");
    if (!result.ok()) {
        std::cout << "[load error: " << result.error() << "] ";
        return false;
    }

    auto& parsed = result.value();
    std::cout << "[" << parsed.statements.size() << " statements, "
              << parsed.proofs.size() << " proofs] ";

    // Execute all proofs
    auto exec_result = rt.execute_all_proofs(parsed);
    if (!exec_result.ok()) {
        std::cout << "[execution error: " << exec_result.error() << "] ";
        return false;
    }

    // Check that proofs registered theorems
    std::vector<std::string> expected_theorems = {
        "pair_elems", "singleton_in_pair", "doubleton_in_pair"
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

    // Ordered pair proofs
    std::cout << "\n── Ordered Pair Proofs ──\n";
    run_test("Load ordered pair axioms and claims", test_ordered_pair_axioms_loaded);
    run_test("Execute ordered pair proofs", test_execute_ordered_pair_proofs);

    // Print summary
    print_summary();

    // Return non-zero if any test failed
    for (const auto& r : test_results) {
        if (!r.passed) return 1;
    }
    return 0;
}
