#include "../runtime/runtime.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace logic;

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: proof_checker <header.fol.def> <proof.fol.proof> [dep1.fol.def ...] [-- dep1.fol.proof ...]\n";
        return 1;
    }

    std::string header_path = argv[1];
    std::string proof_path = argv[2];

    // Split args at "--" separator: dep headers before, dep proofs after
    std::vector<std::string> dep_header_paths;
    std::vector<std::string> dep_proof_paths;
    bool after_separator = false;
    for (int i = 3; i < argc; i++) {
        if (std::string(argv[i]) == "--") {
            after_separator = true;
            continue;
        }
        if (after_separator) {
            dep_proof_paths.push_back(argv[i]);
        } else {
            dep_header_paths.push_back(argv[i]);
        }
    }

    Runtime rt;

    // Load dependency headers first (in order)
    for (const auto& dep_path : dep_header_paths) {
        auto result = rt.load_file_recursive(dep_path);
        if (!result.ok()) {
            std::cerr << "ERROR: Failed to load dependency " << dep_path << ": "
                      << result.error().to_string() << "\n";
            return 1;
        }
        // Execute any proofs from dependencies (they become available as theorems)
        auto exec = rt.execute_all_proofs(result.value());
        if (!exec.ok()) {
            std::cerr << "ERROR: Failed to execute proofs in dependency " << dep_path << ": "
                      << exec.error().to_string() << "\n";
            return 1;
        }
    }

    // Load and execute dependency proof files (making proved claims available as theorems)
    for (const auto& dep_path : dep_proof_paths) {
        auto result = rt.load_file_recursive(dep_path);
        if (!result.ok()) {
            std::cerr << "ERROR: Failed to load dependency proof " << dep_path << ": "
                      << result.error().to_string() << "\n";
            return 1;
        }
        auto exec = rt.execute_all_proofs(result.value());
        if (!exec.ok()) {
            std::cerr << "ERROR: Failed to execute dependency proof " << dep_path << ": "
                      << exec.error().to_string() << "\n";
            return 1;
        }
    }

    // Snapshot claims from dependencies (before loading the main header)
    auto dep_claims = rt.context().claims();

    // Load the main header (axioms + claims)
    auto header_result = rt.load_file_recursive(header_path);
    if (!header_result.ok()) {
        std::cerr << "ERROR: Failed to load header " << header_path << ": "
                  << header_result.error().to_string() << "\n";
        return 1;
    }

    // Execute any proofs from the header's includes
    auto header_exec = rt.execute_all_proofs(header_result.value());
    if (!header_exec.ok()) {
        std::cerr << "ERROR: Failed to execute proofs from header includes: "
                  << header_exec.error().to_string() << "\n";
        return 1;
    }

    // Snapshot claims after loading the main header
    auto all_claims = rt.context().claims();

    // Load the proof file
    std::ifstream proof_file(proof_path);
    if (!proof_file.is_open()) {
        std::cerr << "ERROR: Could not open proof file: " << proof_path << "\n";
        return 1;
    }
    std::stringstream buffer;
    buffer << proof_file.rdbuf();

    std::string error;
    auto proof_result = try_parse_with_proofs(buffer.str(), rt.context(), &error);
    if (!error.empty()) {
        std::cerr << "ERROR: Parse error in " << proof_path << ": " << error << "\n";
        return 1;
    }

    // Execute all proofs (continue on error to get full results)
    int proof_errors = 0;
    for (const auto& proof : proof_result.proofs) {
        try {
            auto status = rt.execute_proof(proof);
            if (!status.ok()) {
                proof_errors++;
                if (proof_errors <= 10)
                    std::cerr << "WARNING: " << proof.claim_name << ": "
                              << status.error().to_string() << "\n";
                else if (proof_errors == 11)
                    std::cerr << "WARNING: ... more errors suppressed\n";
            }
        } catch (const std::exception& e) {
            proof_errors++;
            if (proof_errors <= 10)
                std::cerr << "WARNING: " << proof.claim_name << ": exception: "
                          << e.what() << "\n";
            else if (proof_errors == 11)
                std::cerr << "WARNING: ... more errors suppressed\n";
        }
    }

    // Classify each NEW claim (from the main header, not deps)
    // Categories: PROVED, CONDITIONAL, UNPROVED, MISSING
    std::vector<std::string> proved, conditional, unproved_list, missing;
    size_t new_claims = 0;

    // Helper: check if a theorem transitively depends on any UNPROVED claim
    auto has_unproved_dep = [&](const std::string& name) -> std::unordered_set<std::string> {
        std::unordered_set<std::string> unproved_deps;
        std::unordered_set<std::string> visited;
        std::vector<std::string> stack;
        stack.push_back(name);

        while (!stack.empty()) {
            std::string current = stack.back();
            stack.pop_back();
            if (visited.count(current)) continue;
            visited.insert(current);

            auto deps_it = rt.proof_deps().find(current);
            if (deps_it == rt.proof_deps().end()) continue;

            for (const auto& dep : deps_it->second) {
                if (rt.context().is_unproved(dep)) {
                    unproved_deps.insert(dep);
                }
                if (!visited.count(dep)) {
                    stack.push_back(dep);
                }
            }
        }
        return unproved_deps;
    };

    // Maps for conditional claims: name -> set of unproved deps it depends on
    std::unordered_map<std::string, std::unordered_set<std::string>> conditional_deps;

    for (const auto& [name, _] : all_claims) {
        if (dep_claims.count(name) > 0) continue;  // Skip dependency claims
        new_claims++;

        if (!rt.context().find_theorem(name).has_value()) {
            // No theorem at all — MISSING
            missing.push_back(name);
        } else if (rt.context().is_unproved(name)) {
            // Explicitly UNPROVED
            unproved_list.push_back(name);
        } else {
            // Has a real proof — check transitive deps
            auto deps = has_unproved_dep(name);
            if (deps.empty()) {
                proved.push_back(name);
            } else {
                conditional.push_back(name);
                conditional_deps[name] = std::move(deps);
            }
        }
    }

    // Sort all lists for deterministic output
    std::sort(proved.begin(), proved.end());
    std::sort(conditional.begin(), conditional.end());
    std::sort(unproved_list.begin(), unproved_list.end());
    std::sort(missing.begin(), missing.end());

    // Print status report
    std::cout << "Proved (" << proved.size() << "):\n";
    for (const auto& name : proved) {
        std::cout << "  + " << name << "\n";
    }
    std::cout << "\n";

    if (!conditional.empty()) {
        std::cout << "Conditional (" << conditional.size() << "):\n";
        for (const auto& name : conditional) {
            std::cout << "  ~ " << name << " (depends on:";
            std::vector<std::string> sorted_deps(conditional_deps[name].begin(),
                                                  conditional_deps[name].end());
            std::sort(sorted_deps.begin(), sorted_deps.end());
            for (const auto& dep : sorted_deps) {
                std::cout << " " << dep;
            }
            std::cout << ")\n";
        }
        std::cout << "\n";
    }

    if (!unproved_list.empty()) {
        std::cout << "Unproved (" << unproved_list.size() << "):\n";
        for (const auto& name : unproved_list) {
            std::cout << "  - " << name << "\n";
        }
        std::cout << "\n";
    }

    if (!missing.empty()) {
        std::cerr << "Missing (" << missing.size() << "):\n";
        for (const auto& name : missing) {
            std::cerr << "  ! " << name << "\n";
        }
        std::cerr << "\n";
    }

    // Fail only if there are MISSING claims (no proof and not UNPROVED)
    if (!missing.empty()) {
        std::cerr << "ERROR: " << missing.size() << " claim(s) have no proof and are not marked UNPROVED\n";
        return 1;
    }

    std::cout << "OK: " << new_claims << " claim(s) checked in " << proof_path;
    if (!unproved_list.empty() || !conditional.empty()) {
        std::cout << " (" << proved.size() << " proved";
        if (!conditional.empty()) std::cout << ", " << conditional.size() << " conditional";
        if (!unproved_list.empty()) std::cout << ", " << unproved_list.size() << " unproved";
        std::cout << ")";
    }
    std::cout << "\n";

    // Write output marker file if FOL_OUTPUT is set (for Bazel)
    const char* output_path = std::getenv("FOL_OUTPUT");
    if (output_path) {
        std::ofstream out(output_path);
        out << "proved: " << proved.size() << "\n";
        out << "conditional: " << conditional.size() << "\n";
        out << "unproved: " << unproved_list.size() << "\n";
    }

    return 0;
}
