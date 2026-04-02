// Library serialization round-trip tests

#include "../src/library/library.h"
#include "../src/runtime/runtime.h"

#include <cstdio>
#include <functional>
#include <iostream>
#include <string>
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

// ==================== Tests ====================

bool test_basic_round_trip() {
    // Load axioms and claims, save to library, load back, verify state matches
    Runtime rt1;
    rt1.load(R"(
        axiom all_P: forall x. P(x)
        axiom all_P_impl_Q: forall x. (P(x) -> Q(x))
        claim all_Q: forall x. Q(x)
    )");

    auto baseline = FolLibrary::snapshot(rt1.context());

    // Hmm, baseline was taken AFTER loading, so delta is empty.
    // Need to snapshot BEFORE loading.

    Runtime rt2;
    auto baseline2 = FolLibrary::snapshot(rt2.context());

    rt2.load(R"(
        axiom all_P: forall x. P(x)
        axiom all_P_impl_Q: forall x. (P(x) -> Q(x))
        claim all_Q: forall x. Q(x)
    )");

    std::string lib_path = "/tmp/test_basic.fol.lib";
    auto save_status = FolLibrary::save(
        rt2.context(), baseline2, {}, {}, lib_path);
    if (!save_status.ok()) {
        std::cerr << "save failed: " << save_status.error().to_string() << "\n";
        return false;
    }

    // Load into a fresh runtime
    Runtime rt3;
    auto load_status = rt3.load_library(lib_path);
    if (!load_status.ok()) {
        std::cerr << "load failed: " << load_status.error().to_string() << "\n";
        return false;
    }

    // Verify axioms and claims are present
    if (!rt3.context().find_axiom("all_P").has_value()) return false;
    if (!rt3.context().find_axiom("all_P_impl_Q").has_value()) return false;
    if (!rt3.context().find_claim("all_Q").has_value()) return false;

    // Verify formula content matches
    auto ax1_orig = rt2.context().find_axiom("all_P").value();
    auto ax1_loaded = rt3.context().find_axiom("all_P").value();
    if (ax1_orig.get().to_string() != ax1_loaded.get().to_string()) return false;

    auto ax2_orig = rt2.context().find_axiom("all_P_impl_Q").value();
    auto ax2_loaded = rt3.context().find_axiom("all_P_impl_Q").value();
    if (ax2_orig.get().to_string() != ax2_loaded.get().to_string()) return false;

    auto cl_orig = rt2.context().find_claim("all_Q").value();
    auto cl_loaded = rt3.context().find_claim("all_Q").value();
    if (cl_orig.get().to_string() != cl_loaded.get().to_string()) return false;

    std::remove(lib_path.c_str());
    return true;
}

bool test_round_trip_with_proofs() {
    Runtime rt;
    auto baseline = FolLibrary::snapshot(rt.context());

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
    if (!result.ok()) return false;
    auto exec = rt.execute_all_proofs(result.value());
    if (!exec.ok()) return false;

    // Save with proof_deps
    std::string lib_path = "/tmp/test_proofs.fol.lib";
    auto save_status = FolLibrary::save(
        rt.context(), baseline, rt.proof_deps(), {}, lib_path);
    if (!save_status.ok()) return false;

    // Load into fresh runtime
    Runtime rt2;
    auto load_status = rt2.load_library(lib_path);
    if (!load_status.ok()) return false;

    // Verify theorem (not just claim) is present
    if (!rt2.context().find_theorem("all_Q").has_value()) return false;

    // Verify formula content
    auto thm_orig = rt.context().find_theorem("all_Q").value();
    auto thm_loaded = rt2.context().find_theorem("all_Q").value();
    if (thm_orig.get().to_string() != thm_loaded.get().to_string()) return false;

    // Verify proof_deps metadata
    auto it = rt2.proof_deps().find("all_Q");
    if (it == rt2.proof_deps().end()) return false;
    if (it->second.count("all_P") == 0) return false;
    if (it->second.count("all_P_impl_Q") == 0) return false;

    std::remove(lib_path.c_str());
    return true;
}

bool test_def_annotation_round_trip() {
    Runtime rt;
    auto baseline = FolLibrary::snapshot(rt.context());

    rt.load(R"(
        @def(singleton) axiom singleton_def: forall s. forall x. (singleton(s, x) <-> forall w. (elem(w, s) <-> eq(w, x)))
    )");

    std::string lib_path = "/tmp/test_def.fol.lib";
    auto save_status = FolLibrary::save(
        rt.context(), baseline, {}, {}, lib_path);
    if (!save_status.ok()) return false;

    Runtime rt2;
    auto load_status = rt2.load_library(lib_path);
    if (!load_status.ok()) return false;

    // Verify @def axiom
    if (!rt2.context().find_axiom("singleton_def").has_value()) return false;
    if (!rt2.context().is_defined("singleton")) return false;
    if (!rt2.context().is_same_definition("singleton", "singleton_def")) return false;

    std::remove(lib_path.c_str());
    return true;
}

bool test_two_library_chain() {
    // Library 1: axioms
    Runtime rt1;
    auto baseline1 = FolLibrary::snapshot(rt1.context());
    rt1.load(R"(
        axiom all_P: forall x. P(x)
        axiom all_P_impl_Q: forall x. (P(x) -> Q(x))
    )");

    std::string lib1_path = "/tmp/test_chain1.fol.lib";
    auto s1 = FolLibrary::save(rt1.context(), baseline1, {}, {}, lib1_path);
    if (!s1.ok()) return false;

    // Library 2: claims + proofs, depends on lib1
    Runtime rt2;
    auto l2 = rt2.load_library(lib1_path);
    if (!l2.ok()) return false;
    auto baseline2 = FolLibrary::snapshot(rt2.context());

    auto result = rt2.load_with_proofs(R"(
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
    if (!result.ok()) return false;
    auto exec = rt2.execute_all_proofs(result.value());
    if (!exec.ok()) return false;

    std::string lib2_path = "/tmp/test_chain2.fol.lib";
    auto s2 = FolLibrary::save(rt2.context(), baseline2, rt2.proof_deps(), {}, lib2_path);
    if (!s2.ok()) return false;

    // Load both libraries into a fresh runtime
    Runtime rt3;
    auto ll1 = rt3.load_library(lib1_path);
    if (!ll1.ok()) return false;
    auto ll2 = rt3.load_library(lib2_path);
    if (!ll2.ok()) return false;

    // Verify everything
    if (!rt3.context().find_axiom("all_P").has_value()) return false;
    if (!rt3.context().find_axiom("all_P_impl_Q").has_value()) return false;
    if (!rt3.context().find_theorem("all_Q").has_value()) return false;

    std::remove(lib1_path.c_str());
    std::remove(lib2_path.c_str());
    return true;
}

bool test_unproved_round_trip() {
    Runtime rt;
    auto baseline = FolLibrary::snapshot(rt.context());

    auto result = rt.load_with_proofs(R"(
        claim foo: forall x. P(x)

        proof foo: UNPROVED
    )");
    if (!result.ok()) return false;
    auto exec = rt.execute_all_proofs(result.value());
    if (!exec.ok()) return false;

    std::string lib_path = "/tmp/test_unproved.fol.lib";
    auto s = FolLibrary::save(rt.context(), baseline, rt.proof_deps(), {}, lib_path);
    if (!s.ok()) return false;

    Runtime rt2;
    auto l = rt2.load_library(lib_path);
    if (!l.ok()) return false;

    if (!rt2.context().find_theorem("foo").has_value()) return false;
    if (!rt2.context().is_unproved("foo")) return false;

    std::remove(lib_path.c_str());
    return true;
}

// ==================== Main ====================

void print_summary() {
    int passed = 0, failed = 0;
    for (const auto& r : test_results) {
        if (r.passed) passed++; else failed++;
    }
    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
}

int main() {
    std::cout << "\n── Library Round-Trip Tests ──\n";
    run_test("Basic round-trip", test_basic_round_trip);
    run_test("Round-trip with proofs", test_round_trip_with_proofs);
    run_test("@def annotation round-trip", test_def_annotation_round_trip);
    run_test("Two-library chain", test_two_library_chain);
    run_test("UNPROVED round-trip", test_unproved_round_trip);

    print_summary();
    for (const auto& r : test_results) {
        if (!r.passed) return 1;
    }
    return 0;
}
