// Unit tests for core formula system

#include "../src/core/formula.h"
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

// ==================== Parser Tests ====================

bool test_parse_simple_forall() {
    GlobalContext ctx;
    auto s = parse_sentence("forall x. P(x)", ctx);
    std::string str = s->to_string();
    std::cout << "[" << str << "] ";
    return str == "forall x_0. P(x_0)";
}

bool test_parse_nested_quantifiers() {
    GlobalContext ctx;
    auto s = parse_sentence("forall x. exists y. R(x, y)", ctx);
    std::string str = s->to_string();
    std::cout << "[" << str << "] ";
    // Convention: outer quantifiers get larger indices, inner get smaller
    return str == "forall x_1. exists x_0. R(x_1, x_0)";
}

bool test_parse_same_var_name_different_scope() {
    GlobalContext ctx;
    // Two foralls with same variable name 'x' but different scopes
    auto s = parse_sentence("forall x. P(x) & forall x. Q(x)", ctx);
    std::string str = s->to_string();
    std::cout << "[" << str << "] ";
    // Left forall processed first, gets larger index; right forall gets smaller
    return str == "forall x_1. P(x_1) & forall x_0. Q(x_0)";
}

bool test_parse_implication() {
    GlobalContext ctx;
    auto s = parse_sentence("forall x. (P(x) -> Q(x))", ctx);
    std::string str = s->to_string();
    std::cout << "[" << str << "] ";
    return str == "forall x_0. P(x_0) -> Q(x_0)";
}

bool test_parse_with_constants() {
    GlobalContext ctx;
    auto s = parse_sentence("forall x. R(x, a)", ctx);
    std::string str = s->to_string();
    std::cout << "[" << str << "] ";
    return str == "forall x_0. R(x_0, a)";
}

bool test_reject_free_variable() {
    GlobalContext ctx;
    try {
        [[maybe_unused]] auto s = parse_sentence("P(x)", ctx);  // x is free
        return false;  // Should have thrown
    } catch (const std::runtime_error& e) {
        std::string msg = e.what();
        return msg.find("Free variable") != std::string::npos ||
               msg.find("free variables") != std::string::npos;
    }
}

bool test_parse_bottom() {
    GlobalContext ctx;
    auto s = parse_sentence("forall x. (P(x) -> _|_)", ctx);
    std::string str = s->to_string();
    std::cout << "[" << str << "] ";
    return str == "forall x_0. P(x_0) -> _|_";
}

bool test_parse_negation() {
    GlobalContext ctx;
    auto s = parse_sentence("forall x. ~P(x)", ctx);
    std::string str = s->to_string();
    std::cout << "[" << str << "] ";
    return str == "forall x_0. ~P(x_0)";
}

bool test_parse_biconditional() {
    GlobalContext ctx;
    auto s = parse_sentence("forall x. (P(x) <-> Q(x))", ctx);
    std::string str = s->to_string();
    std::cout << "[" << str << "] ";
    return str == "forall x_0. P(x_0) <-> Q(x_0)";
}

bool test_parse_complex() {
    GlobalContext ctx;
    // This is a single closed sentence with nested quantifiers
    auto s = parse_sentence("forall x. forall y. (R(x, y) -> exists z. S(x, z) & S(z, y))", ctx);
    std::string str = s->to_string();
    std::cout << "[" << str << "] ";
    // Convention: outer quantifiers get larger indices, inner get smaller
    // x (outermost) = x_2, y (middle) = x_1, z (innermost) = x_0
    return str == "forall x_2. forall x_1. R(x_2, x_1) -> exists x_0. S(x_2, x_0) & S(x_0, x_1)";
}

// ==================== Sentence Equality Tests ====================

bool test_sentence_equality() {
    GlobalContext ctx;
    auto s1 = parse_sentence("forall x. P(x)", ctx);
    auto s2 = parse_sentence("forall y. P(y)", ctx);  // Same structure, different var names
    // Both should normalize to same string
    return s1->to_string() == s2->to_string();
}

// ==================== Main ====================

int main() {
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════╗\n";
    std::cout << "║              CORE FORMULA TEST SUITE                   ║\n";
    std::cout << "╚════════════════════════════════════════════════════════╝\n\n";

    // Parser tests
    std::cout << "── Parser Tests ──\n";
    run_test("Parse simple forall", test_parse_simple_forall);
    run_test("Parse nested quantifiers", test_parse_nested_quantifiers);
    run_test("Parse same var name different scope", test_parse_same_var_name_different_scope);
    run_test("Parse implication", test_parse_implication);
    run_test("Parse with constants", test_parse_with_constants);
    run_test("Reject free variable", test_reject_free_variable);
    run_test("Parse bottom", test_parse_bottom);
    run_test("Parse negation", test_parse_negation);
    run_test("Parse biconditional", test_parse_biconditional);
    run_test("Parse complex formula", test_parse_complex);

    // Equality tests
    std::cout << "\n── Equality Tests ──\n";
    run_test("Sentence equality (alpha-equiv)", test_sentence_equality);

    // Print summary
    print_summary();

    // Return non-zero if any test failed
    for (const auto& r : test_results) {
        if (!r.passed) return 1;
    }
    return 0;
}
