#include "../parser/mm_database.h"
#include "mm_translator.h"
#include "proof_tree.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static void usage(const char* argv0) {
    std::cerr
        << "Usage: " << argv0
        << " <file.mm> [options]\n"
        << "\n"
        << "Options:\n"
        << "  --theorem <label>       Translate a single theorem\n"
        << "  --first <label>         First theorem in range (inclusive)\n"
        << "  --last <label>          Last theorem in range (inclusive)\n"
        << "  --count <N>             Translate first N theorems\n"
        << "  --out-def <file>        Output .fol.def file (default: stdout)\n"
        << "  --out-proof <file>      Output .fol.proof file (default: stdout)\n"
        << "  --batch-size <N>        Split output into batches of N theorems\n"
        << "  --batch-dir <dir>       Output directory for batch files\n"
        << "  --skip-errors           Skip unsupported theorems silently\n"
        << "  --known-theorems <file> Load known theorem labels (one per line)\n"
        << "  --emit-labels <file>    Write translated theorem labels to file\n"
        << "  --fol-repo <name>       Bazel repo name for fol_zfc (e.g., fol_zfc)\n"
        << "\n"
        << "If no --theorem/--first/--count given, translates all theorems.\n"
        << "\n"
        << "Batch mode (--batch-size + --batch-dir):\n"
        << "  Writes mm_comprehension.fol.def, mm_batch_NNN.fol.def/proof,\n"
        << "  and BUILD.bazel into the output directory.\n"
        << "  Translates and writes one batch at a time (streaming).\n";
}

// Format batch index as 3-digit zero-padded string
static std::string batch_name(int idx) {
    std::ostringstream oss;
    oss << "mm_batch_" << std::setfill('0') << std::setw(3) << idx;
    return oss.str();
}

static void write_one_batch(
    const std::string& dir, int idx,
    const std::vector<metamath::TranslatedTheorem>& batch) {

    std::string name = batch_name(idx);

    // Write .fol.def (claims only)
    {
        std::ofstream f(dir + "/" + name + ".fol.def");
        f << metamath::MmTranslator::emit_def(batch);
        std::cerr << "Wrote " << dir << "/" << name << ".fol.def ("
                  << batch.size() << " claims)\n";
    }

    // Write .fol.proof
    {
        std::ofstream f(dir + "/" + name + ".fol.proof");
        f << metamath::MmTranslator::emit_proof(batch);
        std::cerr << "Wrote " << dir << "/" << name << ".fol.proof\n";
    }
}

static void write_build_file(const std::string& dir, int num_batches,
                             const std::string& fol_repo = "") {
    std::ofstream f(dir + "/BUILD.bazel");
    std::string repo_prefix = fol_repo.empty() ? "" : "@" + fol_repo;
    f << "load(\"" << repo_prefix << "//build:fol.bzl\", \"fol_library\", \"fol_proof\")\n\n";

    f << "fol_library(\n"
      << "    name = \"mm_comprehension\",\n"
      << "    header = \"mm_comprehension.fol.def\",\n"
      << "    deps = [\"//metamath/bridge:predicate\"],\n"
      << ")\n\n";

    for (int i = 0; i < num_batches; ++i) {
        std::string name = batch_name(i);
        std::string dep = (i == 0)
            ? ":mm_comprehension"
            : ":" + batch_name(i - 1);

        f << "fol_proof(\n"
          << "    name = \"" << name << "\",\n"
          << "    header = \"" << name << ".fol.def\",\n"
          << "    proof = \"" << name << ".fol.proof\",\n"
          << "    deps = [\"" << dep << "\"],\n"
          << ")\n\n";
    }

    std::cerr << "Wrote " << dir << "/BUILD.bazel ("
              << num_batches << " batch targets)\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    std::string filepath;
    std::string single_theorem;
    std::string first_label, last_label;
    int count = -1;
    std::string out_def_path, out_proof_path;
    int batch_size = -1;
    std::string batch_dir;
    std::string known_theorems_file;
    std::string emit_labels_file;
    std::string fol_repo;
    bool skip_errors = false;

    bool classify_defs = false;
    bool build_trees = false;
    std::string dump_tree_label;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--classify-defs") {
            classify_defs = true;
        } else if (arg == "--build-trees") {
            build_trees = true;
        } else if (arg == "--dump-tree" && i + 1 < argc) {
            dump_tree_label = argv[++i];
        } else if (arg == "--theorem" && i + 1 < argc) {
            single_theorem = argv[++i];
        } else if (arg == "--first" && i + 1 < argc) {
            first_label = argv[++i];
        } else if (arg == "--last" && i + 1 < argc) {
            last_label = argv[++i];
        } else if (arg == "--count" && i + 1 < argc) {
            count = std::atoi(argv[++i]);
        } else if (arg == "--out-def" && i + 1 < argc) {
            out_def_path = argv[++i];
        } else if (arg == "--out-proof" && i + 1 < argc) {
            out_proof_path = argv[++i];
        } else if (arg == "--batch-size" && i + 1 < argc) {
            batch_size = std::atoi(argv[++i]);
        } else if (arg == "--batch-dir" && i + 1 < argc) {
            batch_dir = argv[++i];
        } else if (arg == "--known-theorems" && i + 1 < argc) {
            known_theorems_file = argv[++i];
        } else if (arg == "--emit-labels" && i + 1 < argc) {
            emit_labels_file = argv[++i];
        } else if (arg == "--fol-repo" && i + 1 < argc) {
            fol_repo = argv[++i];
        } else if (arg == "--skip-errors") {
            skip_errors = true;
        } else if (arg == "--help" || arg == "-h") {
            usage(argv[0]);
            return 0;
        } else if (arg[0] == '-') {
            std::cerr << "Unknown option: " << arg << "\n";
            usage(argv[0]);
            return 1;
        } else {
            filepath = arg;
        }
    }

    if (filepath.empty()) {
        std::cerr << "No input file specified\n";
        return 1;
    }

    // Validate batch options
    if (!batch_dir.empty() && batch_size <= 0) {
        std::cerr << "--batch-dir requires --batch-size N (N > 0)\n";
        return 1;
    }
    if (batch_size > 0 && batch_dir.empty()) {
        std::cerr << "--batch-size requires --batch-dir <dir>\n";
        return 1;
    }

    // Parse
    std::string parse_error;
    metamath::MmDatabase db =
        metamath::MmDatabase::parse_file(filepath, &parse_error);
    if (!db.ok()) {
        std::cerr << "Parse FAILED: " << parse_error << "\n";
        return 1;
    }
    std::cerr << "Parsed " << db.num_assertions() << " assertions\n";

    // Definition classification mode
    if (classify_defs) {
        metamath::MmTranslator translator(db);
        auto cls = translator.classify_definitions();

        std::cout << "=== Identity definitions (" << cls.identity.size() << ") ===\n";
        for (const auto& l : cls.identity) std::cout << "  " << l << "\n";

        std::cout << "\n=== Non-identity definitions (" << cls.non_identity.size() << ") ===\n";
        for (const auto& l : cls.non_identity) std::cout << "  " << l << "\n";

        std::cout << "\n=== Non-biconditional definitions (" << cls.not_bic.size() << ") ===\n";
        for (const auto& [l, k] : cls.not_bic) std::cout << "  " << l << " [" << k << "]\n";

        std::cout << "\n=== Parse failures (" << cls.parse_fail.size() << ") ===\n";
        for (const auto& l : cls.parse_fail) std::cout << "  " << l << "\n";

        return 0;
    }

    // Build proof trees mode
    if (build_trees) {
        std::vector<std::string> labels_to_try;
        int thm_count = 0;
        for (const auto& lbl : db.assertion_order()) {
            const metamath::Assertion* a = db.get_assertion(lbl);
            if (!a || a->kind != metamath::Assertion::Kind::Theorem) continue;
            if (a->expression.empty() || a->expression[0] != "|-") continue;
            labels_to_try.push_back(lbl);
            ++thm_count;
            if (count > 0 && thm_count >= count) break;
        }

        int ok = 0, fail = 0;
        size_t total_nodes = 0, total_inferences = 0;
        std::map<std::string, int> label_counts;  // how often each assertion label appears as inference

        for (const auto& lbl : labels_to_try) {
            const metamath::Assertion* a = db.get_assertion(lbl);
            metamath::ProofTree tree;
            std::string err;
            if (metamath::build_proof_tree(db, *a, tree, &err)) {
                // Verify root result matches theorem expression
                if (tree.nodes[tree.root].result_expr == a->expression) {
                    ++ok;
                    total_nodes += tree.nodes.size();
                    for (const auto& n : tree.nodes) {
                        if (!n.is_syntax_only && !n.is_ess_hyp) {
                            ++total_inferences;
                            ++label_counts[n.label];
                        }
                    }
                } else {
                    ++fail;
                    std::cerr << "MISMATCH " << lbl << ": result_expr != expression\n";
                }
            } else {
                ++fail;
                std::cerr << "FAIL " << lbl << ": " << err << "\n";
            }
        }

        std::cout << "Trees built: " << ok << " / " << labels_to_try.size()
                  << " (" << fail << " failures)\n";
        std::cout << "Total nodes: " << total_nodes
                  << ", inferences: " << total_inferences << "\n";

        // Top 30 most-referenced assertion labels
        std::vector<std::pair<int, std::string>> sorted_labels;
        for (const auto& [l, c] : label_counts)
            sorted_labels.push_back({c, l});
        std::sort(sorted_labels.rbegin(), sorted_labels.rend());
        std::cout << "\nTop 30 inference labels:\n";
        for (int i = 0; i < 30 && i < (int)sorted_labels.size(); ++i)
            std::cout << "  " << sorted_labels[i].second
                      << ": " << sorted_labels[i].first << "\n";

        // Count by category
        int n_ax_mp = label_counts["ax-mp"];
        int n_axioms = 0, n_defs = 0, n_theorems = 0;
        for (const auto& [l, c] : label_counts) {
            if (l.substr(0, 3) == "ax-") n_axioms += c;
            else if (l.substr(0, 3) == "df-") n_defs += c;
            else n_theorems += c;
        }
        std::cout << "\nBy category:\n"
                  << "  ax-mp: " << n_ax_mp << "\n"
                  << "  other axioms: " << (n_axioms - n_ax_mp) << "\n"
                  << "  definitions: " << n_defs << "\n"
                  << "  theorems: " << n_theorems << "\n";
        return 0;
    }

    // Dump proof tree for a single theorem
    if (!dump_tree_label.empty()) {
        const metamath::Assertion* a = db.get_assertion(dump_tree_label);
        if (!a) {
            std::cerr << "Assertion not found: " << dump_tree_label << "\n";
            return 1;
        }
        metamath::ProofTree tree;
        std::string err;
        if (!metamath::build_proof_tree(db, *a, tree, &err)) {
            std::cerr << "Failed to build tree: " << err << "\n";
            return 1;
        }
        std::cout << "Tree for " << dump_tree_label << ": "
                  << tree.nodes.size() << " nodes, root=" << tree.root << "\n\n";
        for (size_t i = 0; i < tree.nodes.size(); ++i) {
            const auto& n = tree.nodes[i];
            if (n.is_syntax_only) continue;
            std::cout << "[" << i << "] " << n.label;
            if (n.is_ess_hyp) std::cout << " (ess_hyp)";
            std::cout << "\n";
            if (!n.children.empty()) {
                std::cout << "  children:";
                for (auto c : n.children) std::cout << " " << c;
                std::cout << "\n";
            }
            if (!n.subst.empty()) {
                for (const auto& [k, v] : n.subst) {
                    std::cout << "  subst[" << k << "] =";
                    for (const auto& t : v) std::cout << " " << t;
                    std::cout << "\n";
                }
            }
            std::cout << "  result:";
            for (const auto& t : n.result_expr) std::cout << " " << t;
            std::cout << "\n\n";
        }
        return 0;
    }

    // Build list of theorems to translate
    std::vector<std::string> labels;

    if (!single_theorem.empty()) {
        labels.push_back(single_theorem);
    } else {
        bool in_range = first_label.empty();
        int translated = 0;
        for (const auto& lbl : db.assertion_order()) {
            if (!in_range && lbl == first_label) in_range = true;
            if (!in_range) continue;

            const metamath::Assertion* a = db.get_assertion(lbl);
            if (!a || a->kind != metamath::Assertion::Kind::Theorem) continue;
            // Skip non-|- syntax proofs (e.g., weq: $p wff x = y)
            if (a->expression.empty() || a->expression[0] != "|-") continue;

            labels.push_back(lbl);
            ++translated;

            if (count > 0 && translated >= count) break;
            if (!last_label.empty() && lbl == last_label) break;
        }
    }

    std::cerr << "Translating " << labels.size() << " theorems...\n";

    // Streaming batch mode: translate and write one batch at a time
    if (!batch_dir.empty()) {
        namespace fs = std::filesystem;
        fs::create_directories(batch_dir);

        // Write comprehension axioms
        {
            std::ofstream f(batch_dir + "/mm_comprehension.fol.def");
            f << metamath::MmTranslator::emit_comprehension_axioms();
            std::cerr << "Wrote " << batch_dir << "/mm_comprehension.fol.def\n";
        }

        metamath::MmTranslator translator(db);

        // Load known theorems from file (for per-batch process isolation)
        if (!known_theorems_file.empty()) {
            std::ifstream ktf(known_theorems_file);
            std::vector<std::string> known;
            std::string line;
            while (std::getline(ktf, line)) {
                if (!line.empty()) known.push_back(line);
            }
            translator.add_known_theorems(known);
            std::cerr << "Loaded " << known.size() << " known theorems\n";
        }

        int num_batches = 0;
        int total_translated = 0;
        int total_errors = 0;
        size_t label_idx = 0;
        std::vector<std::string> translated_labels;

        while (label_idx < labels.size()) {
            // Translate one batch worth of theorems
            std::vector<metamath::TranslatedTheorem> batch;
            batch.reserve(batch_size);

            while (batch.size() < (size_t)batch_size &&
                   label_idx < labels.size()) {
                metamath::TranslatedTheorem thm;
                std::string err;
                if (translator.translate(labels[label_idx], thm, &err)) {
                    batch.push_back(std::move(thm));
                } else {
                    ++total_errors;
                    if (!skip_errors) {
                        std::cerr << "SKIP " << labels[label_idx]
                                  << ": " << err << "\n";
                    }
                }
                ++label_idx;
            }

            if (!batch.empty()) {
                for (const auto& t : batch)
                    translated_labels.push_back(t.mm_label);
                write_one_batch(batch_dir, num_batches, batch);
                total_translated += batch.size();
                ++num_batches;
            }
            // Free memory: batch results and translator caches
            translator.clear_caches();
        }

        write_build_file(batch_dir, num_batches, fol_repo);

        // Emit translated labels if requested
        if (!emit_labels_file.empty()) {
            std::ofstream lf(emit_labels_file);
            for (const auto& lbl : translated_labels)
                lf << lbl << "\n";
            std::cerr << "Wrote " << translated_labels.size()
                      << " labels to " << emit_labels_file << "\n";
        }

        std::cerr << "Translated: " << total_translated
                  << ", Skipped: " << total_errors
                  << ", Batches: " << num_batches << "\n";
        return 0;
    }

    // Non-batch mode: translate all at once
    metamath::MmTranslator translator(db);

    // Load known theorems (for non-batch mode too)
    if (!known_theorems_file.empty()) {
        std::ifstream ktf(known_theorems_file);
        std::vector<std::string> known;
        std::string line;
        while (std::getline(ktf, line)) {
            if (!line.empty()) known.push_back(line);
        }
        translator.add_known_theorems(known);
        std::cerr << "Loaded " << known.size() << " known theorems\n";
    }
    std::vector<metamath::TranslatedTheorem> results;
    int errors = 0;

    for (const auto& lbl : labels) {
        metamath::TranslatedTheorem thm;
        std::string err;
        if (translator.translate(lbl, thm, &err)) {
            results.push_back(std::move(thm));
        } else {
            ++errors;
            if (!skip_errors) {
                std::cerr << "SKIP " << lbl << ": " << err << "\n";
            }
        }
    }

    std::cerr << "Translated: " << results.size()
              << ", Skipped: " << errors
              << "\n";

    // Single-file output mode
    std::string comprehension = metamath::MmTranslator::emit_comprehension_axioms();
    std::string def_content = comprehension + metamath::MmTranslator::emit_def(results);
    std::string proof_content = metamath::MmTranslator::emit_proof(results);

    if (!out_def_path.empty()) {
        std::ofstream f(out_def_path);
        f << def_content;
        std::cerr << "Wrote " << out_def_path << "\n";
    } else {
        std::cout << "=== .fol.def ===\n" << def_content;
    }

    if (!out_proof_path.empty()) {
        std::ofstream f(out_proof_path);
        f << proof_content;
        std::cerr << "Wrote " << out_proof_path << "\n";
    } else {
        std::cout << "=== .fol.proof ===\n" << proof_content;
    }

    return 0;
}
