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

    // Execute all proofs
    auto exec_result = rt.execute_all_proofs(proof_result);
    if (!exec_result.ok()) {
        std::cerr << "ERROR: " << exec_result.error().to_string() << "\n";
        return 1;
    }

    // Check that every NEW claim (from the main header, not deps) is proved
    std::vector<std::string> unproven;
    size_t new_claims = 0;
    for (const auto& [name, _] : all_claims) {
        if (dep_claims.count(name) > 0) continue;  // Skip dependency claims
        new_claims++;
        if (!rt.context().find_theorem(name).has_value()) {
            unproven.push_back(name);
        }
    }

    if (!unproven.empty()) {
        std::sort(unproven.begin(), unproven.end());
        std::cerr << "ERROR: " << unproven.size() << " claim(s) not proved:\n";
        for (const auto& name : unproven) {
            std::cerr << "  - " << name << "\n";
        }
        return 1;
    }

    std::cout << "OK: All " << new_claims << " claim(s) proved in "
              << proof_path << "\n";

    // Write output marker file if FOL_OUTPUT is set (for Bazel)
    const char* output_path = std::getenv("FOL_OUTPUT");
    if (output_path) {
        std::ofstream out(output_path);
        out << "proven\n";
    }

    return 0;
}
