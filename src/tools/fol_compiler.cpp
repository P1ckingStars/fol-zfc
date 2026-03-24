#include "../library/library.h"
#include "../runtime/runtime.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace logic;

static void usage() {
    std::cerr << "Usage:\n"
              << "  fol_compiler <output.fol.lib> <header.fol.def> [dep.fol.lib ...]\n"
              << "  fol_compiler <output.fol.lib> <header.fol.def> <proof.fol.proof> [dep.fol.lib ...]\n";
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        usage();
        return 1;
    }

    std::string output_path = argv[1];
    std::string header_path = argv[2];

    // Parse remaining args: optional proof path, then dep library paths
    std::string proof_path;
    std::vector<std::string> dep_paths;

    int i = 3;
    // If next arg doesn't start with -- and ends with .fol.proof, it's the proof file
    if (i < argc) {
        std::string arg = argv[i];
        if (arg.size() >= 10 && arg.substr(arg.size() - 10) == ".fol.proof") {
            proof_path = arg;
            i++;
        }
    }
    // Remaining args are dep library paths
    for (; i < argc; ++i) {
        dep_paths.push_back(argv[i]);
    }

    Runtime rt;

    // Load dependency libraries
    for (const auto& dep : dep_paths) {
        auto status = rt.load_library(dep);
        if (!status.ok()) {
            std::cerr << "ERROR: Failed to load dep library " << dep << ": "
                      << status.error().to_string() << "\n";
            return 1;
        }
    }

    // Snapshot baseline state (after deps, before new content)
    auto baseline = FolLibrary::snapshot(rt.context());

    // Load the header file
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

    // If a proof file is provided, load and execute it
    if (!proof_path.empty()) {
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

        // Execute proofs (continue on error)
        int proof_errors = 0;
        for (const auto& proof : proof_result.proofs) {
            try {
                auto status = rt.execute_proof(proof);
                if (!status.ok()) {
                    proof_errors++;
                    if (proof_errors <= 10)
                        std::cerr << "WARNING: " << proof.claim_name << ": "
                                  << status.error().to_string() << "\n";
                }
            } catch (const std::exception& e) {
                proof_errors++;
                if (proof_errors <= 10)
                    std::cerr << "WARNING: " << proof.claim_name << ": exception: "
                              << e.what() << "\n";
            }
        }
    }

    // Collect source files for this compilation unit
    std::vector<std::string> source_files;
    source_files.push_back(std::filesystem::canonical(header_path).string());
    if (!proof_path.empty()) {
        source_files.push_back(std::filesystem::canonical(proof_path).string());
    }

    // Compute proof_deps delta (only new ones, not from deps)
    std::unordered_map<std::string, std::unordered_set<std::string>> proof_deps_delta;
    for (const auto& [name, deps] : rt.proof_deps()) {
        // Check if this proof_dep is new (not from baseline)
        // Since baseline axioms/theorems don't have proof_deps entries loaded from deps,
        // any entry in rt.proof_deps() that wasn't there before dep loading is new.
        // After load_library, proof_deps_ is populated from dep metadata.
        // New entries come from execute_proof calls above.
        // Simple heuristic: if the name is a new theorem or claim, include it.
        if (baseline.theorems.find(name) == baseline.theorems.end() &&
            baseline.claims.find(name) == baseline.claims.end()) {
            proof_deps_delta[name] = deps;
        }
    }

    // Save the library
    auto save_status = FolLibrary::save(
        rt.context(), baseline, proof_deps_delta, source_files, output_path);
    if (!save_status.ok()) {
        std::cerr << "ERROR: Failed to save library: "
                  << save_status.error().to_string() << "\n";
        return 1;
    }

    // Count what was serialized
    size_t new_theorems = 0, new_claims = 0;
    for (const auto& [name, _] : rt.context().claims()) {
        if (baseline.claims.find(name) == baseline.claims.end()) new_claims++;
    }
    for (const auto& [name, _] : rt.context().theorems()) {
        if (baseline.theorems.find(name) == baseline.theorems.end()) new_theorems++;
    }
    // Count axioms by checking named_axioms_ through find_axiom
    // Since we don't have direct access, use the snapshot diff
    // For the summary, just report what we know
    std::cout << "Compiled " << output_path << ": "
              << new_claims << " claims, " << new_theorems << " theorems\n";

    // Write .proven marker if FOL_OUTPUT is set (for Bazel)
    const char* fol_output = std::getenv("FOL_OUTPUT");
    if (fol_output) {
        std::ofstream out(fol_output);
        out << "compiled: " << output_path << "\n";
    }

    return 0;
}
