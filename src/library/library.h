#pragma once

#include "../core/formula.h"
#include "../util/error.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace logic {

// Metadata from a library (not part of GlobalContext, returned to caller)
struct LibraryMetadata {
    // Proof dependency graph: proof_name -> set of axiom/theorem names used
    std::unordered_map<std::string, std::unordered_set<std::string>> proof_deps;
    // Source files compiled into this library (for #pragma once dedup)
    std::vector<std::string> source_files;
};

// Result of loading a library
struct LinkResult {
    LibraryMetadata metadata;
};

// Binary library serializer and linker.
// Serializes GlobalContext state to a compact binary format (.fol.lib)
// and links (deserializes) libraries back into a GlobalContext.
class FolLibrary {
public:
    // Section types in the binary format
    enum class SectionType : uint32_t {
        PredicateSymtab = 1,
        Formulas        = 2,
        Sentences       = 3,
        Exports         = 4,
        ProofDeps       = 5,
        SourceFiles     = 6,
    };

    // Formula variant tags in binary format
    enum class FormulaTag : uint8_t {
        PredicateInstance = 0,
        Compound          = 1,
        Quantified        = 2,
        SchemaVar         = 3,
    };

    // Term variant tags in binary format
    enum class TermTag : uint8_t {
        GeneralizedVar = 0,
        Description    = 1,
    };

    // Snapshot of GlobalContext state for delta computation.
    // Captured via friend access to GlobalContext internals.
    struct Snapshot {
        size_t pred_id;
        size_t formula_id;
        size_t sentence_id;
        std::unordered_map<std::string, SentenceHandle> axioms;
        std::unordered_map<std::string, SentenceHandle> theorems;
        std::unordered_map<std::string, SentenceHandle> claims;
        std::unordered_map<std::string, std::string> definitions;
        std::unordered_map<std::string, SchemaDefinition> schemas;
        std::unordered_set<std::string> proven_schemas;
    };

    // Take a snapshot of the current GlobalContext state
    static Snapshot snapshot(const GlobalContext& ctx);

    // Serialize a GlobalContext delta to a .fol.lib file.
    // Only items added after the baseline snapshot are serialized.
    // proof_deps and source_files are serialized as metadata sections.
    static util::ResultStatus save(
        const GlobalContext& ctx,
        const Snapshot& baseline,
        const std::unordered_map<std::string, std::unordered_set<std::string>>& proof_deps,
        const std::vector<std::string>& source_files,
        const std::string& path);

    // Load (link) a .fol.lib file into a GlobalContext.
    // Returns metadata (proof_deps, source_files) for the caller to consume.
    static util::Result<LinkResult> load(
        GlobalContext& ctx,
        const std::string& path);

private:
    // Binary I/O helpers
    static void write_u8(std::vector<uint8_t>& buf, uint8_t v);
    static void write_u32(std::vector<uint8_t>& buf, uint32_t v);
    static void write_u64(std::vector<uint8_t>& buf, uint64_t v);
    static void write_str(std::vector<uint8_t>& buf, const std::string& s);

    struct Reader {
        const uint8_t* data;
        size_t size;
        size_t pos = 0;

        uint8_t  read_u8();
        uint32_t read_u32();
        uint64_t read_u64();
        std::string read_str();
        bool at_end() const { return pos >= size; }
        void seek(size_t offset) { pos = offset; }
    };

    // Section directory entry
    struct SectionEntry {
        SectionType type;
        uint64_t offset;
        uint64_t size;
    };

    // ---- Delta data collected from GlobalContext ----

    struct PredEntry { size_t global_id; std::string name; size_t arity; };
    struct ImportPredEntry { std::string name; size_t arity; };
    struct FormulaEntry { size_t global_id; const Formula* formula; };
    struct SentenceEntry { size_t global_id; const Sentence* sentence; };
    struct AxiomEntry { std::string name; uint32_t sentence_local_id; bool is_def; std::string def_pred; };
    struct TheoremEntry { std::string name; uint32_t sentence_local_id; bool is_unproved; };
    struct ClaimEntry { std::string name; uint32_t sentence_local_id; };
    struct SchemaEntry {
        std::string name; uint32_t body_local_id;
        std::vector<std::string> var_names; std::vector<size_t> var_arities;
        bool is_proven;
    };

    struct DeltaData {
        std::vector<ImportPredEntry> imported_preds;
        std::vector<PredEntry> exported_preds;
        std::unordered_map<size_t, uint32_t> pred_import_index;
        std::unordered_map<size_t, uint32_t> pred_export_index;

        std::vector<FormulaEntry> new_formulas;
        std::unordered_map<size_t, uint32_t> formula_id_map;

        std::vector<SentenceEntry> new_sentences;

        std::vector<AxiomEntry> new_axioms;
        std::vector<TheoremEntry> new_theorems;
        std::vector<ClaimEntry> new_claims;
        std::vector<SchemaEntry> new_schemas;
    };

    // Collect all delta data between ctx and baseline
    static DeltaData collect_delta_data(const GlobalContext& ctx, const Snapshot& baseline);

    // Section writers
    static void write_predicate_section(
        std::vector<uint8_t>& buf,
        const std::vector<ImportPredEntry>& imported_preds,
        const std::vector<PredEntry>& exported_preds);

    static util::ResultStatus write_formula_section(
        std::vector<uint8_t>& buf,
        const DeltaData& delta);

    static void write_sentence_section(
        std::vector<uint8_t>& buf,
        const std::vector<SentenceEntry>& new_sentences,
        const std::unordered_map<size_t, uint32_t>& formula_id_map);

    static void write_export_section(
        std::vector<uint8_t>& buf,
        const std::vector<AxiomEntry>& new_axioms,
        const std::vector<TheoremEntry>& new_theorems,
        const std::vector<ClaimEntry>& new_claims,
        const std::vector<SchemaEntry>& new_schemas);

    static void write_metadata_sections(
        std::vector<uint8_t>& section_data,
        std::vector<SectionEntry>& sections,
        const std::unordered_map<std::string, std::unordered_set<std::string>>& proof_deps,
        const std::vector<std::string>& source_files);

    static std::vector<uint8_t> assemble_file(
        const std::vector<SectionEntry>& sections,
        const std::vector<uint8_t>& section_data);

    // Serialization helpers
    static void write_term(std::vector<uint8_t>& buf, const Term& t,
                           const std::unordered_map<size_t, uint32_t>& formula_id_map);

    // Deserialization helpers
    static Term read_term(Reader& r,
                          const std::vector<FormulaHandle>& formula_map);

    // Link phase helpers
    static util::Result<LinkResult> link(
        GlobalContext& ctx, Reader& r,
        const std::vector<SectionEntry>& sections);

    struct PredHandles {
        std::vector<PredicateHandle> imported;
        std::vector<PredicateHandle> exported;
    };

    static util::Result<PredHandles> link_predicates(
        GlobalContext& ctx, Reader& r, const SectionEntry& sec);

    static util::Result<std::vector<FormulaHandle>> link_formulas(
        GlobalContext& ctx, Reader& r, const SectionEntry& sec,
        const std::vector<PredicateHandle>& imported_preds,
        const std::vector<PredicateHandle>& exported_preds);

    static util::Result<std::vector<SentenceHandle>> link_sentences(
        GlobalContext& ctx, Reader& r, const SectionEntry& sec,
        const std::vector<FormulaHandle>& formula_map);

    static util::ResultStatus link_exports(
        GlobalContext& ctx, Reader& r, const SectionEntry& sec,
        const std::vector<SentenceHandle>& sentence_map,
        const std::vector<FormulaHandle>& formula_map);

    static LibraryMetadata read_metadata(
        Reader& r, const std::vector<SectionEntry>& sections);
};

}  // namespace logic
