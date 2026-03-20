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
    std::ifstream file("zfc/basics/axioms.fol.def");
    if (!file.is_open()) {
        std::cout << "[ERROR: Could not open axioms.fol.def] ";
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string zfc_axioms = buffer.str();

    GlobalContext ctx;
    try {
        auto stmts = parse_statements(zfc_axioms, ctx);
        if (stmts.size() != 8) {
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
        if (!ctx.find_axiom("separation_P").has_value()) return false;

        // Print extensionality for verification
        auto ext = ctx.find_axiom("extensionality");
        std::cout << "[ext: " << ext.value()->to_string().substr(0, 30) << "...] ";

        return true;
    } catch (const std::exception& e) {
        std::cout << "[ERROR: " << e.what() << "] ";
        return false;
    }
}

// ==================== Predicate Schema Core Tests ====================
// These bypass the parser and test FormulaBuilder directly.

// Helper: create a predicate P with given arity
static PredicateHandle make_pred(GlobalContext& ctx, const std::string& name, size_t arity) {
    return ctx.add_predicate(name, arity);
}

bool test_schema_var_args_to_string() {
    // SchemaVar{0, [gen(0)]} should print as ?0(v0)
    GlobalContext ctx;
    FormulaBuilder b(ctx);
    auto h = b.make_schema_var(0, {Term::generalized(0)});
    std::string s = h.get().to_string();
    std::cout << "[" << s << "] ";
    return s.find("?0") != std::string::npos;
}

bool test_instantiate_arity1_basic() {
    // Schema body: P(gen_0)  — "P applied to variable 0"
    // Binding: \x. elem(x, nat)
    // Result: elem(gen_0, nat)
    GlobalContext ctx;
    FormulaBuilder b(ctx);
    auto elem = make_pred(ctx, "elem", 2);

    // Build schema body: P(gen_0) where P is SchemaVar{0}
    auto body = b.make_schema_var(0, {Term::generalized(0)});

    // Build lambda binding: \x. elem(x, x)
    var_index x = b.enter_scope();
    auto lambda_body = b.predicate(elem, {Term::fixed(x), Term::fixed(x)});
    b.exit_scope();

    SchemaBind bind({x}, lambda_body);
    auto result = b.instantiate_schema(body, {bind});

    std::string s = result.get().to_string();
    std::cout << "[" << s << "] ";
    // Should be elem(v0, v0) — gen(0) substituted for fixed(x) in both positions
    return s.find("elem") != std::string::npos;
}

bool test_instantiate_arity1_two_occ() {
    // Schema: P(a) -> P(b)  where a=gen(0), b=gen(1)
    // Binding P: \x. R(x)
    // Result: R(a) -> R(b)
    GlobalContext ctx;
    FormulaBuilder b(ctx);
    auto R = make_pred(ctx, "R", 1);

    auto pa = b.make_schema_var(0, {Term::generalized(0)});
    auto pb = b.make_schema_var(0, {Term::generalized(1)});
    auto body = b.make_implies(pa, pb);

    var_index x = b.enter_scope();
    auto lambda_body = b.predicate(R, {Term::fixed(x)});
    b.exit_scope();

    SchemaBind bind({x}, lambda_body);
    auto result = b.instantiate_schema(body, {bind});

    std::string s = result.get().to_string();
    std::cout << "[" << s << "] ";
    // Should be R(v0) -> R(v1)
    return s == "R(x_0) -> R(x_1)";
}

bool test_instantiate_arity2() {
    // Schema: R(gen_0, gen_1)
    // Binding R: \x y. eq(x, y)
    // Result: eq(gen_0, gen_1)
    GlobalContext ctx;
    FormulaBuilder b(ctx);
    auto eq = make_pred(ctx, "eq", 2);

    auto body = b.make_schema_var(0, {Term::generalized(0), Term::generalized(1)});

    var_index x = b.enter_scope();
    var_index y = b.enter_scope();
    auto lambda_body = b.predicate(eq, {Term::fixed(x), Term::fixed(y)});
    b.exit_scope();
    b.exit_scope();

    SchemaBind bind({x, y}, lambda_body);
    auto result = b.instantiate_schema(body, {bind});

    std::string s = result.get().to_string();
    std::cout << "[" << s << "] ";
    return s == "eq(x_0, x_1)";
}

bool test_instantiate_mixed() {
    // Schema with arity-0 var ph and arity-1 var P:
    // ph -> P(gen_0)
    // Bindings: ph = Q(a), P = \x. R(x)
    // Result: Q(a) -> R(gen_0)
    GlobalContext ctx;
    FormulaBuilder b(ctx);
    auto Q = make_pred(ctx, "Q", 1);
    auto R = make_pred(ctx, "R", 1);

    // ph is SchemaVar{0}, P is SchemaVar{1}
    auto ph = b.make_schema_var(0);
    auto p_applied = b.make_schema_var(1, {Term::generalized(0)});
    auto body = b.make_implies(ph, p_applied);

    // Binding 0 (ph): Q(gen_0) — arity 0
    auto qa = b.predicate(Q, {Term::generalized(5)});  // use gen_5 as "a"
    SchemaBind bind_ph(qa);

    // Binding 1 (P): \x. R(x) — arity 1
    var_index x = b.enter_scope();
    auto lambda_body = b.predicate(R, {Term::fixed(x)});
    b.exit_scope();
    SchemaBind bind_P({x}, lambda_body);

    auto result = b.instantiate_schema(body, {bind_ph, bind_P});

    std::string s = result.get().to_string();
    std::cout << "[" << s << "] ";
    // Should be Q(v5) -> R(v0)
    return s.find("Q(") != std::string::npos && s.find("R(") != std::string::npos;
}

bool test_instantiate_arity0_compat() {
    // Pure arity-0 schema (backward compat): ph -> ps
    // Bindings: ph = A, ps = B
    // Result: A -> B
    GlobalContext ctx;
    FormulaBuilder b(ctx);
    auto A = make_pred(ctx, "A", 0);
    auto B = make_pred(ctx, "B", 0);

    auto ph = b.make_schema_var(0);
    auto ps = b.make_schema_var(1);
    auto body = b.make_implies(ph, ps);

    auto fa = b.predicate(A, {});
    auto fb = b.predicate(B, {});

    auto result = b.instantiate_schema(body, {SchemaBind(fa), SchemaBind(fb)});

    std::string s = result.get().to_string();
    std::cout << "[" << s << "] ";
    return s == "A -> B";
}

bool test_translate_term_schema_var() {
    // translate_term should propagate through SchemaVar args
    // SchemaVar{0, [fixed(x)]} with translate fixed(x) -> gen(0)
    // should become SchemaVar{0, [gen(0)]}
    GlobalContext ctx;
    FormulaBuilder b(ctx);

    var_index x = b.enter_scope();
    auto sv = b.make_schema_var(0, {Term::fixed(x)});

    // Translate fixed(x) -> generalized(0)
    auto result = b.translate_term(sv, Term::fixed(x), Term::generalized(0));
    b.exit_scope();

    std::string s = result.get().to_string();
    std::cout << "[" << s << "] ";
    // Should be ?0(v0) — the fixed var replaced with generalized
    return result != sv;  // should have changed
}

bool test_capture_avoidance() {
    // Schema: forall x. P(x)
    // Binding P: \y. elem(y, nat)
    // Result: forall x. elem(x, nat)
    //
    // The tricky part: P(x) has x=gen(0) which is bound by the forall.
    // The lambda has param y (a fixed var). When instantiating P(gen_0),
    // we substitute fixed(y) -> gen(0) in the lambda body.
    // This should produce elem(gen_0, nat) inside the forall scope.
    GlobalContext ctx;
    FormulaBuilder b(ctx);
    auto elem = make_pred(ctx, "elem", 2);

    // Build: forall x. P(x)
    FormulaHandle forall_result;
    {
        QuantifierBuilder qb(b, Op::Forall, forall_result);
        auto p_x = b.make_schema_var(0, {qb.var()});
        qb.set_body(p_x);
    }

    // Build lambda: \y. elem(y, y)
    var_index y = b.enter_scope();
    auto lambda_body = b.predicate(elem, {Term::fixed(y), Term::fixed(y)});
    b.exit_scope();

    SchemaBind bind({y}, lambda_body);
    auto result = b.instantiate_schema(forall_result, {bind});

    std::string s = result.get().to_string();
    std::cout << "[" << s << "] ";
    // Should be a forall with elem inside
    return s.find("forall") != std::string::npos && s.find("elem") != std::string::npos;
}

bool test_instantiate_complex_body() {
    // Schema: P(a) & P(b) -> P(a)
    // Binding P: \x. (elem(x, nat) & R(x))
    // Result: (elem(a,nat) & R(a)) & (elem(b,nat) & R(b)) -> (elem(a,nat) & R(a))
    GlobalContext ctx;
    FormulaBuilder b(ctx);
    auto R = make_pred(ctx, "R", 1);

    // Schema body: P(gen_0) & P(gen_1) -> P(gen_0)
    auto pa = b.make_schema_var(0, {Term::generalized(0)});
    auto pb = b.make_schema_var(0, {Term::generalized(1)});
    auto pa2 = b.make_schema_var(0, {Term::generalized(0)});
    auto lhs = b.make_and(pa, pb);
    auto body = b.make_implies(lhs, pa2);

    // Lambda: \x. R(x)  (simpler than original description)
    var_index x = b.enter_scope();
    auto lambda_body = b.predicate(R, {Term::fixed(x)});
    b.exit_scope();

    SchemaBind bind({x}, lambda_body);
    auto result = b.instantiate_schema(body, {bind});

    std::string s = result.get().to_string();
    std::cout << "[" << s << "] ";
    // Should be (R(v0) & R(v1)) -> R(v0)
    return s == "R(x_0) & R(x_1) -> R(x_0)";
}

bool test_lambda_param_unused() {
    // Schema: P(gen_0)
    // Binding P: \x. Q  (lambda param x is unused in body)
    // Result: Q (the formula Q, regardless of gen_0)
    GlobalContext ctx;
    FormulaBuilder b(ctx);
    auto Q = make_pred(ctx, "Q", 0);

    auto body = b.make_schema_var(0, {Term::generalized(0)});

    var_index x = b.enter_scope();
    auto lambda_body = b.predicate(Q, {});  // Q doesn't use x
    b.exit_scope();

    SchemaBind bind({x}, lambda_body);
    auto result = b.instantiate_schema(body, {bind});

    std::string s = result.get().to_string();
    std::cout << "[" << s << "] ";
    // Should be just "Q" — the unused param means P is a constant predicate
    return s == "Q";
}

bool test_parse_ordered_pair_proof() {
    // Parse ordered pair header (axioms + claims only, no proofs)
    std::ifstream file("zfc/basics/ordered_pair.fol.def");
    if (!file.is_open()) {
        std::cout << "[ERROR: Could not open ordered_pair.fol.def] ";
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string header = buffer.str();

    // Strip include directives (parse_statements doesn't handle them)
    std::string stripped;
    std::istringstream stream(header);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.substr(0, 7) != "include") {
            stripped += line + "\n";
        }
    }

    GlobalContext ctx;
    try {
        auto stmts = parse_statements(stripped, ctx);

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

    // Predicate schema core tests (bypass parser, test C++ API directly)
    std::cout << "\n── Predicate Schema Core Tests ──\n";
    run_test("SchemaVar with args to_string", test_schema_var_args_to_string);
    run_test("Instantiate arity-1 basic", test_instantiate_arity1_basic);
    run_test("Instantiate arity-1 two occurrences", test_instantiate_arity1_two_occ);
    run_test("Instantiate arity-2", test_instantiate_arity2);
    run_test("Instantiate mixed arity-0 and arity-1", test_instantiate_mixed);
    run_test("Instantiate arity-0 backward compat", test_instantiate_arity0_compat);
    run_test("translate_term propagates through SchemaVar args", test_translate_term_schema_var);
    run_test("Capture avoidance: lambda param vs quantifier var", test_capture_avoidance);
    run_test("Instantiate arity-1 with complex body", test_instantiate_complex_body);
    run_test("Lambda param unused in body", test_lambda_param_unused);

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
