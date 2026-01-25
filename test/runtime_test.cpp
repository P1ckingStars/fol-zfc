// Unit tests for runtime interface

#include "../src/runtime/runtime.h"
#include "../src/logic/formula.h"
#include "../src/logic/proof.h"
#include "../src/parser/parser.h"

#include <cassert>
#include <iostream>
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

// Test: Basic axiom registration
bool test_axiom_registration() {
    Runtime rt;

    rt.axiom("P_a", "P(a)");
    rt.axiom("P_impl_Q", "forall x. (P(x) -> Q(x))");

    return rt.has("P_a") && rt.has("P_impl_Q") && !rt.has("nonexistent");
}

// Test: Duplicate name rejection
bool test_duplicate_name_rejection() {
    Runtime rt;

    rt.axiom("foo", "P(a)");

    try {
        rt.axiom("foo", "Q(a)");  // Should throw
        return false;
    } catch (const std::runtime_error&) {
        return true;
    }
}

// Test: Non-sentence rejection
bool test_non_sentence_rejection() {
    Runtime rt;

    try {
        rt.axiom("bad", "P(x)");  // x is free, not a sentence
        return false;
    } catch (const std::runtime_error&) {
        return true;
    }
}

// Test: Simple modus ponens proof
bool test_modus_ponens() {
    Runtime rt;

    rt.axiom("P_a", "P(a)");
    rt.axiom("P_impl_Q", "forall x. (P(x) -> Q(x))");

    auto ctx = rt.prove("Q_a", "Q(a)");

    step_id p_a = ctx.use("P_a");
    step_id all_pq = ctx.use("P_impl_Q");

    // Get constant 'a' from local db
    auto a_const = ctx.db().find_constant("a");
    if (!a_const) return false;

    step_id pq_a = ctx.forall_elim(all_pq, Term::constant(*a_const));
    step_id q_a = ctx.implies_elim(pq_a, p_a);
    (void)q_a;

    return ctx.qed();
}

// Test: Dependency tracking
bool test_dependency_tracking() {
    Runtime rt;

    rt.axiom("A1", "P(a)");
    rt.axiom("A2", "Q(a)");
    rt.axiom("A3", "R(a)");

    auto ctx = rt.prove("conj", "P(a) & Q(a)");

    step_id p = ctx.use("A1");
    step_id q = ctx.use("A2");
    // A3 is not used

    ctx.and_intro(p, q);
    ctx.qed();

    const auto& used = ctx.used();
    return used.count("A1") == 1 &&
           used.count("A2") == 1 &&
           used.count("A3") == 0 &&
           used.size() == 2;
}

// Test: Proven theorem can be used
bool test_proven_theorem_usage() {
    Runtime rt;

    rt.axiom("P_a", "P(a)");
    rt.axiom("Q_a", "Q(a)");

    // First prove P(a) & Q(a)
    {
        auto ctx = rt.prove("conj", "P(a) & Q(a)");
        step_id p = ctx.use("P_a");
        step_id q = ctx.use("Q_a");
        ctx.and_intro(p, q);
        if (!ctx.qed()) return false;
    }

    // Now use the theorem to prove Q(a) & P(a)
    {
        auto ctx = rt.prove("conj_swapped", "Q(a) & P(a)");
        step_id conj = ctx.use("conj");  // Use proven theorem
        step_id p = ctx.and_elim_l(conj);
        step_id q = ctx.and_elim_r(conj);
        ctx.and_intro(q, p);
        if (!ctx.qed()) return false;
    }

    return rt.has("conj") && rt.has("conj_swapped");
}

// Test: Undischarged assumption fails qed
bool test_undischarged_assumption_fails() {
    Runtime rt;

    auto ctx = rt.prove("bad", "P(a) -> P(a)");

    auto p_a = ctx.parse("P(a)");
    ctx.assume(p_a);  // This assumption is not discharged

    // qed should fail because we have an undischarged assumption
    return !ctx.qed();
}

// Test: Proper implies_intro discharges assumption
bool test_implies_intro_discharges() {
    Runtime rt;

    auto ctx = rt.prove("identity", "forall x. (P(x) -> P(x))");

    auto p_x = ctx.parse("P(x)");
    step_id assume_step = ctx.assume(p_x);
    assumption_id aid = ctx.get_assumption_id(assume_step);
    step_id impl = ctx.implies_intro(aid, assume_step);

    // Now do forall intro
    {
        EigenvariableScope x_scope(ctx.db(), ctx.proof(), 0);
        ctx.forall_intro(impl, 0);
    }

    return ctx.qed();
}

// Test: Premise caching (same axiom returns same step)
bool test_premise_caching() {
    Runtime rt;

    rt.axiom("A", "P(a)");

    auto ctx = rt.prove("test", "P(a)");

    step_id s1 = ctx.use("A");
    step_id s2 = ctx.use("A");

    return s1 == s2;
}

// ==================== First-Order Logic Tests ====================

// Test: forall elimination
bool test_forall_elim() {
    Runtime rt;

    rt.axiom("all_P", "forall x. P(x)");

    auto ctx = rt.prove("P_c", "P(c)");

    step_id all_p = ctx.use("all_P");
    auto c = ctx.db().find_constant("c");
    if (!c) c = ctx.db().create_constant("c");

    ctx.forall_elim(all_p, Term::constant(*c));

    return ctx.qed();
}

// Test: exists introduction
bool test_exists_intro() {
    Runtime rt;

    rt.axiom("P_a", "P(a)");

    auto ctx = rt.prove("exists_P", "exists x. P(x)");

    step_id p_a = ctx.use("P_a");
    auto a = ctx.db().find_constant("a");
    if (!a) return false;

    ctx.exists_intro(p_a, 0, Term::constant(*a));

    return ctx.qed();
}

// ==================== Parser Integration Tests ====================

bool test_parser_simple() {
    Runtime rt;

    // These should all be valid sentences
    rt.axiom("a1", "forall x. P(x)");
    rt.axiom("a2", "exists x. P(x)");
    rt.axiom("a3", "forall x. (P(x) -> Q(x))");
    rt.axiom("a4", "forall x. forall y. R(x, y)");

    return rt.has("a1") && rt.has("a2") && rt.has("a3") && rt.has("a4");
}

bool test_parser_complex() {
    Runtime rt;

    // More complex sentences
    rt.axiom("a1", "forall x. exists y. R(x, y)");
    rt.axiom("a2", "forall x. (P(x) -> exists y. R(x, y))");
    rt.axiom("a3", "forall x. forall y. (R(x, y) <-> R(y, x))");

    return rt.has("a1") && rt.has("a2") && rt.has("a3");
}

// ==================== Main ====================

int main() {
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════╗\n";
    std::cout << "║              RUNTIME INTERFACE TEST SUITE              ║\n";
    std::cout << "╚════════════════════════════════════════════════════════╝\n\n";

    // Runtime basic tests
    std::cout << "── Runtime Basic Tests ──\n";
    run_test("Axiom registration", test_axiom_registration);
    run_test("Duplicate name rejection", test_duplicate_name_rejection);
    run_test("Non-sentence rejection", test_non_sentence_rejection);
    run_test("Premise caching", test_premise_caching);

    // Proof construction tests
    std::cout << "\n── Proof Construction Tests ──\n";
    run_test("Modus ponens", test_modus_ponens);
    run_test("Dependency tracking", test_dependency_tracking);
    run_test("Proven theorem usage", test_proven_theorem_usage);
    run_test("Undischarged assumption fails", test_undischarged_assumption_fails);
    run_test("Implies intro discharges", test_implies_intro_discharges);

    // First-order logic tests
    std::cout << "\n── First-Order Logic Tests ──\n";
    run_test("Forall elimination", test_forall_elim);
    run_test("Exists introduction", test_exists_intro);

    // Parser integration tests
    std::cout << "\n── Parser Integration Tests ──\n";
    run_test("Parser: simple sentences", test_parser_simple);
    run_test("Parser: complex sentences", test_parser_complex);

    // Print summary
    print_summary();

    // Return non-zero if any test failed
    for (const auto& r : test_results) {
        if (!r.passed) return 1;
    }
    return 0;
}
