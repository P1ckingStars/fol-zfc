// Unit tests for core formula system

#include "../src/core/formula.h"
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

bool test_parse_multiple_quantifiers() {
    // Test parsing formulas with multiple quantifiers binding different variables
    GlobalContext ctx;
    auto s = parse_sentence("forall x. forall y. R(x, y)", ctx);
    std::string str = s->to_string();
    std::cout << "[" << str << "] ";
    return str == "forall x_1. forall x_0. R(x_1, x_0)";
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

// ==================== Statement Parser Tests ====================

bool test_parse_single_axiom() {
    GlobalContext ctx;
    auto stmts = parse_statements("axiom foo: forall x. P(x)", ctx);
    if (stmts.size() != 1) return false;
    if (stmts[0].kind != ParsedStatement::Kind::Axiom) return false;
    if (stmts[0].name != "foo") return false;
    std::cout << "[" << stmts[0].formula->to_string() << "] ";
    return stmts[0].formula->to_string() == "forall x_0. P(x_0)";
}

bool test_parse_single_claim() {
    GlobalContext ctx;
    auto stmts = parse_statements("claim bar: forall x. (P(x) -> Q(x))", ctx);
    if (stmts.size() != 1) return false;
    if (stmts[0].kind != ParsedStatement::Kind::Claim) return false;
    if (stmts[0].name != "bar") return false;
    std::cout << "[" << stmts[0].formula->to_string() << "] ";
    return stmts[0].formula->to_string() == "forall x_0. P(x_0) -> Q(x_0)";
}

bool test_parse_theorem_alias() {
    GlobalContext ctx;
    auto stmts = parse_statements("theorem baz: A & B", ctx);
    if (stmts.size() != 1) return false;
    // theorem is an alias for claim
    if (stmts[0].kind != ParsedStatement::Kind::Claim) return false;
    if (stmts[0].name != "baz") return false;
    return true;
}

bool test_parse_multiple_statements() {
    GlobalContext ctx;
    std::string input = R"(
axiom ext: forall x. forall y. (R(x, y) -> R(y, x))
axiom refl: forall x. R(x, x)
claim trans: forall x. forall y. forall z. (R(x, y) & R(y, z) -> R(x, z))
)";
    auto stmts = parse_statements(input, ctx);
    if (stmts.size() != 3) return false;
    if (stmts[0].kind != ParsedStatement::Kind::Axiom) return false;
    if (stmts[0].name != "ext") return false;
    if (stmts[1].kind != ParsedStatement::Kind::Axiom) return false;
    if (stmts[1].name != "refl") return false;
    if (stmts[2].kind != ParsedStatement::Kind::Claim) return false;
    if (stmts[2].name != "trans") return false;
    return true;
}

bool test_axiom_registered_in_context() {
    GlobalContext ctx;
    auto stmts = parse_statements("axiom myaxiom: forall x. P(x)", ctx);
    if (stmts.size() != 1) return false;

    // Axiom should be registered in ctx
    auto found = ctx.find_axiom("myaxiom");
    if (!found.has_value()) return false;

    // Should be in known set
    if (!ctx.is_known(stmts[0].formula)) return false;

    return true;
}

bool test_claim_not_registered() {
    GlobalContext ctx;
    auto stmts = parse_statements("claim myclaim: forall x. Q(x)", ctx);
    if (stmts.size() != 1) return false;

    // Claim should NOT be registered as axiom
    auto found = ctx.find_axiom("myclaim");
    if (found.has_value()) return false;

    // Claim should NOT be in known set (until proven)
    if (ctx.is_known(stmts[0].formula)) return false;

    return true;
}

bool test_parse_zfc_axioms() {
    // Read ZFC axioms from file
    std::ifstream file("zfc/axioms.fol");
    if (!file.is_open()) {
        std::cout << "[ERROR: Could not open axioms.fol] ";
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string zfc_axioms = buffer.str();

    GlobalContext ctx;
    try {
        auto stmts = parse_statements(zfc_axioms, ctx);
        if (stmts.size() != 10) {
            std::cout << "[got " << stmts.size() << " statements] ";
            return false;
        }

        // All should be axioms
        for (const auto& stmt : stmts) {
            if (stmt.kind != ParsedStatement::Kind::Axiom) return false;
        }

        // Check specific axioms are registered
        if (!ctx.find_axiom("extensionality").has_value()) return false;
        if (!ctx.find_axiom("empty_set").has_value()) return false;
        if (!ctx.find_axiom("choice").has_value()) return false;

        // Print extensionality for verification
        auto ext = ctx.find_axiom("extensionality");
        std::cout << "[ext: " << ext.value()->to_string().substr(0, 30) << "...] ";

        return true;
    } catch (const std::exception& e) {
        std::cout << "[ERROR: " << e.what() << "] ";
        return false;
    }
}

bool test_parse_ordered_pair_proof() {
    // Read ordered pair proof from file
    std::ifstream file("zfc/ordered_pair.fol");
    if (!file.is_open()) {
        std::cout << "[ERROR: Could not open ordered_pair.fol] ";
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string proof = buffer.str();

    GlobalContext ctx;
    try {
        auto stmts = parse_statements(proof, ctx);

        // Count axioms and claims
        int axioms = 0, claims = 0;
        for (const auto& stmt : stmts) {
            if (stmt.kind == ParsedStatement::Kind::Axiom) axioms++;
            else claims++;
        }

        std::cout << "[" << axioms << " axioms, " << claims << " claims] ";

        // Check main theorem exists
        if (!ctx.find_axiom("pair_def").has_value()) {
            std::cout << "[missing pair_def] ";
            return false;
        }

        return stmts.size() > 0 && claims > 0;
    } catch (const std::exception& e) {
        std::cout << "[ERROR: " << e.what() << "] ";
        return false;
    }
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
    run_test("Parse multiple quantifiers", test_parse_multiple_quantifiers);
    run_test("Reject free variable", test_reject_free_variable);
    run_test("Parse bottom", test_parse_bottom);
    run_test("Parse negation", test_parse_negation);
    run_test("Parse biconditional", test_parse_biconditional);
    run_test("Parse complex formula", test_parse_complex);

    // Equality tests
    std::cout << "\n── Equality Tests ──\n";
    run_test("Sentence equality (alpha-equiv)", test_sentence_equality);

    // Statement parser tests
    std::cout << "\n── Statement Parser Tests ──\n";
    run_test("Parse single axiom", test_parse_single_axiom);
    run_test("Parse single claim", test_parse_single_claim);
    run_test("Parse theorem (alias for claim)", test_parse_theorem_alias);
    run_test("Parse multiple statements", test_parse_multiple_statements);
    run_test("Axiom registered in context", test_axiom_registered_in_context);
    run_test("Claim not registered", test_claim_not_registered);

    // ZFC axioms test
    std::cout << "\n── ZFC Axioms Test ──\n";
    run_test("Parse ZFC axioms", test_parse_zfc_axioms);
    run_test("Parse ordered pair proof", test_parse_ordered_pair_proof);

    // Print summary
    print_summary();

    // Return non-zero if any test failed
    for (const auto& r : test_results) {
        if (!r.passed) return 1;
    }
    return 0;
}
