#include "library.h"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace logic {

namespace {

// Extract the numeric ID from a handle (uses hash_value which returns id_)
template<typename T>
size_t handle_id(const util::Handle<T>& h) { return h.hash_value(); }

// Binary format constants
constexpr char kMagic[8] = {'F','O','L','L','I','B',0,0};
constexpr uint32_t kFormatVersion = 1;
constexpr size_t kHeaderSize = 16;       // 8 magic + 4 version + 4 section_count
constexpr size_t kSectionEntrySize = 20; // 4 type + 8 offset + 8 size
constexpr size_t kMaxSectionCount = 64;

}  // namespace

// ==================== Binary I/O ====================

void FolLibrary::write_u8(std::vector<uint8_t>& buf, uint8_t v) {
    buf.push_back(v);
}

void FolLibrary::write_u32(std::vector<uint8_t>& buf, uint32_t v) {
    buf.push_back(static_cast<uint8_t>(v));
    buf.push_back(static_cast<uint8_t>(v >> 8));
    buf.push_back(static_cast<uint8_t>(v >> 16));
    buf.push_back(static_cast<uint8_t>(v >> 24));
}

void FolLibrary::write_u64(std::vector<uint8_t>& buf, uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        buf.push_back(static_cast<uint8_t>(v >> (i * 8)));
    }
}

void FolLibrary::write_str(std::vector<uint8_t>& buf, const std::string& s) {
    write_u32(buf, static_cast<uint32_t>(s.size()));
    buf.insert(buf.end(), s.begin(), s.end());
}

uint8_t FolLibrary::Reader::read_u8() {
    if (pos >= size) throw std::runtime_error("unexpected end of library file");
    return data[pos++];
}

uint32_t FolLibrary::Reader::read_u32() {
    if (pos + 4 > size) throw std::runtime_error("unexpected end of library file");
    uint32_t v = static_cast<uint32_t>(data[pos])
               | (static_cast<uint32_t>(data[pos+1]) << 8)
               | (static_cast<uint32_t>(data[pos+2]) << 16)
               | (static_cast<uint32_t>(data[pos+3]) << 24);
    pos += 4;
    return v;
}

uint64_t FolLibrary::Reader::read_u64() {
    if (pos + 8 > size) throw std::runtime_error("unexpected end of library file");
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
        v |= static_cast<uint64_t>(data[pos + i]) << (i * 8);
    }
    pos += 8;
    return v;
}

std::string FolLibrary::Reader::read_str() {
    uint32_t len = read_u32();
    if (pos + len > size) throw std::runtime_error("unexpected end of library file");
    std::string s(reinterpret_cast<const char*>(data + pos), len);
    pos += len;
    return s;
}

// ==================== Term Serialization ====================

void FolLibrary::write_term(
    std::vector<uint8_t>& buf, const Term& t,
    const std::unordered_map<size_t, uint32_t>& formula_id_map) {
    if (t.is_generalized()) {
        write_u8(buf, static_cast<uint8_t>(TermTag::GeneralizedVar));
        write_u32(buf, static_cast<uint32_t>(t.as_variable()));
    } else if (t.is_description()) {
        write_u8(buf, static_cast<uint8_t>(TermTag::Description));
        const auto& d = t.as_description();
        write_u32(buf, static_cast<uint32_t>(d.bound_var));
        size_t body_global_id = handle_id(d.body);
        auto it = formula_id_map.find(body_global_id);
        if (it == formula_id_map.end()) {
            throw std::runtime_error(
                "description body formula not in formula_id_map (global_id="
                + std::to_string(body_global_id) + ")");
        }
        write_u32(buf, it->second);
    } else {
        // FixedVar — should never appear in stored formulas
        throw std::runtime_error("FixedVar in stored formula");
    }
}

Term FolLibrary::read_term(Reader& r, const std::vector<FormulaHandle>& formula_map) {
    auto tag = static_cast<TermTag>(r.read_u8());
    switch (tag) {
        case TermTag::GeneralizedVar:
            return Term::generalized(r.read_u32());
        case TermTag::Description: {
            uint32_t bound_var = r.read_u32();
            uint32_t body_id = r.read_u32();
            if (body_id == 0 || body_id > formula_map.size())
                throw std::runtime_error("invalid formula ref in description term");
            return Term::description(bound_var, formula_map[body_id - 1]);
        }
        default:
            throw std::runtime_error("unknown term tag");
    }
}

// ==================== Snapshot ====================

FolLibrary::Snapshot FolLibrary::snapshot(const GlobalContext& ctx) {
    Snapshot s;
    s.pred_id = ctx.predicates_.next_id();
    s.formula_id = ctx.formulas_.next_id();
    s.sentence_id = ctx.sentences_.next_id();
    s.axioms = ctx.named_axioms_;
    s.theorems = ctx.named_theorems_;
    s.claims = ctx.named_claims_;
    s.definitions = ctx.defined_predicates_;
    s.schemas = ctx.named_schemas_;
    s.proven_schemas = ctx.proven_schemas_;
    return s;
}

// ==================== Delta Collection ====================

FolLibrary::DeltaData FolLibrary::collect_delta_data(
    const GlobalContext& ctx, const Snapshot& baseline) {

    DeltaData d;

    // New predicates: those with ID >= baseline
    ctx.predicates_.for_each([&](size_t id, const Predicate& p) {
        if (id >= baseline.pred_id) {
            uint32_t idx = static_cast<uint32_t>(d.exported_preds.size());
            d.pred_export_index[id] = idx;
            d.exported_preds.push_back({id, p.get_name(), p.get_num_args()});
        }
    });

    // Walk formula trees from sentence roots and schema bodies to find reachable formulas
    std::unordered_set<size_t> reachable_formula_ids;
    std::function<void(FormulaHandle)> collect_reachable = [&](FormulaHandle h) {
        if (!h.valid()) return;
        size_t id = handle_id(h);
        if (reachable_formula_ids.count(id)) return;
        reachable_formula_ids.insert(id);

        const Formula& f = h.get();
        if (f.is_compound()) {
            collect_reachable(f.as_compound().left);
            collect_reachable(f.as_compound().right);
        } else if (f.is_quantified()) {
            collect_reachable(f.as_quantified().body);
        } else if (f.is_predicate()) {
            for (const auto& t : f.as_predicate().args()) {
                if (t.is_description()) collect_reachable(t.as_description().body);
            }
        } else if (f.is_schema_var()) {
            for (const auto& t : f.as_schema_var().args) {
                if (t.is_description()) collect_reachable(t.as_description().body);
            }
        }
    };

    ctx.sentences_.for_each([&](size_t id, const Sentence& s) {
        if (id >= baseline.sentence_id) {
            collect_reachable(s.root());
        }
    });
    for (const auto& [name, def] : ctx.named_schemas_) {
        if (baseline.schemas.find(name) == baseline.schemas.end()) {
            collect_reachable(def.body);
        }
    }

    // Collect reachable formulas in ID order (topological)
    ctx.formulas_.for_each([&](size_t id, const Formula& f) {
        if (reachable_formula_ids.count(id)) {
            d.new_formulas.push_back({id, &f});
        }
    });
    std::sort(d.new_formulas.begin(), d.new_formulas.end(),
              [](const auto& a, const auto& b) { return a.global_id < b.global_id; });

    for (uint32_t i = 0; i < d.new_formulas.size(); ++i) {
        d.formula_id_map[d.new_formulas[i].global_id] = i + 1;  // 1-based local IDs
    }

    // Determine imported predicates (used by new formulas but from deps)
    for (const auto& fe : d.new_formulas) {
        const Formula& f = *fe.formula;
        if (f.is_predicate()) {
            size_t pid = handle_id(f.as_predicate().predicate());
            if (pid < baseline.pred_id && d.pred_import_index.find(pid) == d.pred_import_index.end()) {
                uint32_t idx = static_cast<uint32_t>(d.imported_preds.size());
                d.pred_import_index[pid] = idx;
                const auto& p = f.as_predicate().predicate().get();
                d.imported_preds.push_back({p.get_name(), p.get_num_args()});
            }
        }
    }

    // New sentences
    std::unordered_map<size_t, uint32_t> sentence_id_map;
    ctx.sentences_.for_each([&](size_t id, const Sentence& s) {
        if (id >= baseline.sentence_id) {
            d.new_sentences.push_back({id, &s});
        }
    });
    std::sort(d.new_sentences.begin(), d.new_sentences.end(),
              [](const auto& a, const auto& b) { return a.global_id < b.global_id; });

    for (uint32_t i = 0; i < d.new_sentences.size(); ++i) {
        sentence_id_map[d.new_sentences[i].global_id] = i + 1;
    }

    // New axioms
    for (const auto& [name, sh] : ctx.named_axioms_) {
        if (baseline.axioms.find(name) != baseline.axioms.end()) continue;
        size_t sid = handle_id(sh);
        auto it = sentence_id_map.find(sid);
        if (it == sentence_id_map.end()) continue;
        bool is_def = false;
        std::string def_pred;
        for (const auto& [pred_name, axiom_name] : ctx.defined_predicates_) {
            if (axiom_name == name && baseline.definitions.find(pred_name) == baseline.definitions.end()) {
                is_def = true;
                def_pred = pred_name;
                break;
            }
        }
        d.new_axioms.push_back({name, it->second, is_def, def_pred});
    }

    // New theorems
    for (const auto& [name, sh] : ctx.named_theorems_) {
        if (baseline.theorems.find(name) != baseline.theorems.end()) continue;
        size_t sid = handle_id(sh);
        auto it = sentence_id_map.find(sid);
        if (it == sentence_id_map.end()) continue;
        d.new_theorems.push_back({name, it->second, ctx.unproved_theorems_.count(name) > 0});
    }

    // New claims
    for (const auto& [name, sh] : ctx.named_claims_) {
        if (baseline.claims.find(name) != baseline.claims.end()) continue;
        size_t sid = handle_id(sh);
        auto it = sentence_id_map.find(sid);
        if (it == sentence_id_map.end()) continue;
        d.new_claims.push_back({name, it->second});
    }

    // New schemas
    for (const auto& [name, def] : ctx.named_schemas_) {
        if (baseline.schemas.find(name) != baseline.schemas.end()) continue;
        size_t fid = handle_id(def.body);
        auto it = d.formula_id_map.find(fid);
        uint32_t local_id = (it != d.formula_id_map.end()) ? it->second : 0;
        d.new_schemas.push_back({name, local_id, def.var_names, def.var_arities,
                                  ctx.proven_schemas_.count(name) > 0});
    }

    return d;
}

// ==================== Section Writers ====================

void FolLibrary::write_predicate_section(
    std::vector<uint8_t>& buf,
    const std::vector<ImportPredEntry>& imported_preds,
    const std::vector<PredEntry>& exported_preds) {

    write_u32(buf, static_cast<uint32_t>(imported_preds.size()));
    for (const auto& p : imported_preds) {
        write_str(buf, p.name);
        write_u32(buf, static_cast<uint32_t>(p.arity));
    }
    write_u32(buf, static_cast<uint32_t>(exported_preds.size()));
    for (const auto& p : exported_preds) {
        write_u32(buf, static_cast<uint32_t>(p.global_id));
        write_str(buf, p.name);
        write_u32(buf, static_cast<uint32_t>(p.arity));
    }
}

util::ResultStatus FolLibrary::write_formula_section(
    std::vector<uint8_t>& buf,
    const DeltaData& delta) {

    write_u32(buf, static_cast<uint32_t>(delta.new_formulas.size()));

    for (uint32_t i = 0; i < delta.new_formulas.size(); ++i) {
        const Formula& f = *delta.new_formulas[i].formula;
        uint32_t local_id = i + 1;

        write_u32(buf, local_id);
        write_u64(buf, static_cast<uint64_t>(f.content_hash()));
        write_u32(buf, static_cast<uint32_t>(f.next_gen_var_idx_));
        write_u8(buf, f.has_schema_vars_ ? 1 : 0);

        if (f.is_predicate()) {
            write_u8(buf, static_cast<uint8_t>(FormulaTag::PredicateInstance));
            const auto& p = f.as_predicate();
            size_t pid = handle_id(p.predicate());
            auto imp_it = delta.pred_import_index.find(pid);
            auto exp_it = delta.pred_export_index.find(pid);
            if (imp_it != delta.pred_import_index.end()) {
                write_u8(buf, 0);  // import
                write_u32(buf, imp_it->second);
            } else if (exp_it != delta.pred_export_index.end()) {
                write_u8(buf, 1);  // export
                write_u32(buf, exp_it->second);
            } else {
                return MAKE_ERROR << "predicate not in import or export table";
            }
            write_u32(buf, static_cast<uint32_t>(p.args().size()));
            for (const auto& t : p.args()) {
                write_term(buf, t, delta.formula_id_map);
            }
        } else if (f.is_compound()) {
            write_u8(buf, static_cast<uint8_t>(FormulaTag::Compound));
            const auto& c = f.as_compound();
            write_u8(buf, static_cast<uint8_t>(c.op));
            auto map_fh = [&](FormulaHandle h) -> uint32_t {
                if (!h.valid()) return 0;
                size_t gid = handle_id(h);
                auto it = delta.formula_id_map.find(gid);
                return (it != delta.formula_id_map.end()) ? it->second : 0;
            };
            write_u32(buf, map_fh(c.left));
            write_u32(buf, map_fh(c.right));
        } else if (f.is_quantified()) {
            write_u8(buf, static_cast<uint8_t>(FormulaTag::Quantified));
            const auto& q = f.as_quantified();
            write_u8(buf, static_cast<uint8_t>(q.op));
            write_u32(buf, static_cast<uint32_t>(q.var));
            size_t body_gid = handle_id(q.body);
            auto it = delta.formula_id_map.find(body_gid);
            write_u32(buf, (it != delta.formula_id_map.end()) ? it->second : 0);
        } else if (f.is_schema_var()) {
            write_u8(buf, static_cast<uint8_t>(FormulaTag::SchemaVar));
            const auto& sv = f.as_schema_var();
            write_u32(buf, static_cast<uint32_t>(sv.id));
            write_u32(buf, static_cast<uint32_t>(sv.args.size()));
            for (const auto& t : sv.args) {
                write_term(buf, t, delta.formula_id_map);
            }
        } else {
            return MAKE_ERROR << "unsupported formula variant in serialization";
        }
    }

    return util::Ok();
}

void FolLibrary::write_sentence_section(
    std::vector<uint8_t>& buf,
    const std::vector<SentenceEntry>& new_sentences,
    const std::unordered_map<size_t, uint32_t>& formula_id_map) {

    write_u32(buf, static_cast<uint32_t>(new_sentences.size()));
    for (uint32_t i = 0; i < new_sentences.size(); ++i) {
        write_u32(buf, i + 1);  // local_id
        size_t root_gid = handle_id(new_sentences[i].sentence->root());
        auto it = formula_id_map.find(root_gid);
        write_u32(buf, (it != formula_id_map.end()) ? it->second : 0);
    }
}

void FolLibrary::write_export_section(
    std::vector<uint8_t>& buf,
    const std::vector<AxiomEntry>& new_axioms,
    const std::vector<TheoremEntry>& new_theorems,
    const std::vector<ClaimEntry>& new_claims,
    const std::vector<SchemaEntry>& new_schemas) {

    write_u32(buf, static_cast<uint32_t>(new_axioms.size()));
    for (const auto& a : new_axioms) {
        write_str(buf, a.name);
        write_u32(buf, a.sentence_local_id);
        write_u8(buf, a.is_def ? 1 : 0);
        write_str(buf, a.def_pred);
    }

    write_u32(buf, static_cast<uint32_t>(new_theorems.size()));
    for (const auto& t : new_theorems) {
        write_str(buf, t.name);
        write_u32(buf, t.sentence_local_id);
        write_u8(buf, t.is_unproved ? 1 : 0);
    }

    write_u32(buf, static_cast<uint32_t>(new_claims.size()));
    for (const auto& c : new_claims) {
        write_str(buf, c.name);
        write_u32(buf, c.sentence_local_id);
    }

    write_u32(buf, static_cast<uint32_t>(new_schemas.size()));
    for (const auto& s : new_schemas) {
        write_str(buf, s.name);
        write_u32(buf, s.body_local_id);
        write_u32(buf, static_cast<uint32_t>(s.var_names.size()));
        for (const auto& vn : s.var_names) write_str(buf, vn);
        for (const auto& va : s.var_arities) write_u32(buf, static_cast<uint32_t>(va));
        write_u8(buf, s.is_proven ? 1 : 0);
    }
}

void FolLibrary::write_metadata_sections(
    std::vector<uint8_t>& section_data,
    std::vector<SectionEntry>& sections,
    const std::unordered_map<std::string, std::unordered_set<std::string>>& proof_deps,
    const std::vector<std::string>& source_files) {

    // ProofDeps section
    {
        size_t start = section_data.size();
        write_u32(section_data, static_cast<uint32_t>(proof_deps.size()));
        for (const auto& [name, deps] : proof_deps) {
            write_str(section_data, name);
            write_u32(section_data, static_cast<uint32_t>(deps.size()));
            for (const auto& dep : deps) {
                write_str(section_data, dep);
            }
        }
        sections.push_back({SectionType::ProofDeps,
                             static_cast<uint64_t>(start),
                             static_cast<uint64_t>(section_data.size() - start)});
    }

    // SourceFiles section
    {
        size_t start = section_data.size();
        write_u32(section_data, static_cast<uint32_t>(source_files.size()));
        for (const auto& sf : source_files) {
            write_str(section_data, sf);
        }
        sections.push_back({SectionType::SourceFiles,
                             static_cast<uint64_t>(start),
                             static_cast<uint64_t>(section_data.size() - start)});
    }
}

std::vector<uint8_t> FolLibrary::assemble_file(
    const std::vector<SectionEntry>& sections,
    const std::vector<uint8_t>& section_data) {

    std::vector<uint8_t> buf;
    size_t dir_size = sections.size() * kSectionEntrySize;
    buf.reserve(kHeaderSize + dir_size + section_data.size());

    // Header
    buf.insert(buf.end(), kMagic, kMagic + sizeof(kMagic));
    write_u32(buf, kFormatVersion);
    write_u32(buf, static_cast<uint32_t>(sections.size()));

    // Section directory — offsets are relative to start of file
    size_t data_base = kHeaderSize + dir_size;
    for (const auto& sec : sections) {
        write_u32(buf, static_cast<uint32_t>(sec.type));
        write_u64(buf, sec.offset + data_base);
        write_u64(buf, sec.size);
    }

    // Section data
    buf.insert(buf.end(), section_data.begin(), section_data.end());

    return buf;
}

// ==================== Serialization (save) ====================

util::ResultStatus FolLibrary::save(
    const GlobalContext& ctx,
    const Snapshot& baseline,
    const std::unordered_map<std::string, std::unordered_set<std::string>>& proof_deps,
    const std::vector<std::string>& source_files,
    const std::string& path) {

    DeltaData delta = collect_delta_data(ctx, baseline);

    // Write each section into section_data, recording section entries
    std::vector<SectionEntry> sections;
    std::vector<uint8_t> section_data;
    section_data.reserve(64 * 1024);

    auto sec_begin = [&]() -> size_t {
        return section_data.size();
    };
    auto sec_end = [&](SectionType type, size_t start) {
        sections.push_back({type, static_cast<uint64_t>(start),
                            static_cast<uint64_t>(section_data.size() - start)});
    };

    // Section: PREDICATE_SYMTAB
    {
        size_t start = sec_begin();
        write_predicate_section(section_data, delta.imported_preds, delta.exported_preds);
        sec_end(SectionType::PredicateSymtab, start);
    }

    // Section: FORMULAS
    {
        size_t start = sec_begin();
        auto status = write_formula_section(section_data, delta);
        if (!status.ok()) return status;
        sec_end(SectionType::Formulas, start);
    }

    // Section: SENTENCES
    {
        size_t start = sec_begin();
        write_sentence_section(section_data, delta.new_sentences, delta.formula_id_map);
        sec_end(SectionType::Sentences, start);
    }

    // Section: EXPORTS
    {
        size_t start = sec_begin();
        write_export_section(section_data, delta.new_axioms, delta.new_theorems,
                              delta.new_claims, delta.new_schemas);
        sec_end(SectionType::Exports, start);
    }

    // Sections: PROOF_DEPS + SOURCE_FILES
    write_metadata_sections(section_data, sections, proof_deps, source_files);

    // Assemble header + directory + section data
    auto buf = assemble_file(sections, section_data);

    // Write to file
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) {
        return MAKE_ERROR << "cannot open output file: " << path;
    }
    out.write(reinterpret_cast<const char*>(buf.data()), buf.size());
    if (!out.good()) {
        return MAKE_ERROR << "write error: " << path;
    }

    return util::Ok();
}

// ==================== Deserialization (load / link) ====================

util::Result<LinkResult> FolLibrary::load(
    GlobalContext& ctx,
    const std::string& path) {

    // Read entire file into memory
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in.is_open()) {
        return MAKE_ERROR << "cannot open library file: " << path;
    }
    auto file_size = in.tellg();
    if (file_size < 0) {
        return MAKE_ERROR << "cannot determine file size: " << path;
    }
    in.seekg(0);
    std::vector<uint8_t> file_data(static_cast<size_t>(file_size));
    in.read(reinterpret_cast<char*>(file_data.data()), file_size);
    if (!in.good()) {
        return MAKE_ERROR << "read error: " << path;
    }

    Reader r{file_data.data(), file_data.size()};

    // Validate header
    if (r.size < kHeaderSize) return MAKE_ERROR << "file too small";
    if (std::memcmp(r.data, kMagic, sizeof(kMagic)) != 0) {
        return MAKE_ERROR << "invalid magic number";
    }
    r.pos = sizeof(kMagic);
    uint32_t version = r.read_u32();
    if (version != kFormatVersion) {
        return MAKE_ERROR << "unsupported version: " << version;
    }
    uint32_t section_count = r.read_u32();
    if (section_count > kMaxSectionCount) {
        return MAKE_ERROR << "too many sections: " << section_count;
    }

    // Parse section directory
    std::vector<SectionEntry> sections;
    sections.reserve(section_count);
    for (uint32_t i = 0; i < section_count; ++i) {
        SectionEntry se;
        se.type = static_cast<SectionType>(r.read_u32());
        se.offset = r.read_u64();
        se.size = r.read_u64();
        sections.push_back(se);
    }

    return link(ctx, r, sections);
}

// ==================== Link Phase Helpers ====================

util::Result<FolLibrary::PredHandles> FolLibrary::link_predicates(
    GlobalContext& ctx, Reader& r, const SectionEntry& sec) {

    PredHandles result;
    r.seek(sec.offset);

    uint32_t import_count = r.read_u32();
    result.imported.reserve(import_count);
    for (uint32_t i = 0; i < import_count; ++i) {
        std::string name = r.read_str();
        uint32_t arity = r.read_u32();
        auto key = name + "/" + std::to_string(arity);
        auto h = ctx.predicates_.find_by_key(key);
        if (!h.has_value()) {
            return MAKE_ERROR << "unresolved imported predicate: " << name << "/" << arity;
        }
        result.imported.push_back(h.value());
    }

    uint32_t export_count = r.read_u32();
    result.exported.reserve(export_count);
    for (uint32_t i = 0; i < export_count; ++i) {
        r.read_u32();  // local_id (not used during loading)
        std::string name = r.read_str();
        uint32_t arity = r.read_u32();
        auto h = ctx.add_predicate(std::move(name), arity);
        result.exported.push_back(h);
    }

    return result;
}

util::Result<std::vector<FormulaHandle>> FolLibrary::link_formulas(
    GlobalContext& ctx, Reader& r, const SectionEntry& sec,
    const std::vector<PredicateHandle>& imported_preds,
    const std::vector<PredicateHandle>& exported_preds) {

    std::vector<FormulaHandle> formula_map;
    r.seek(sec.offset);
    uint32_t count = r.read_u32();
    formula_map.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        r.read_u32();  // local_id
        uint64_t content_hash = r.read_u64();
        r.read_u32();  // next_gen (recomputed by Formula constructors)
        r.read_u8();   // has_schema (recomputed by Formula constructors)
        auto tag = static_cast<FormulaTag>(r.read_u8());

        FormulaHandle fh;

        switch (tag) {
            case FormulaTag::PredicateInstance: {
                uint8_t pred_ref = r.read_u8();
                uint32_t pred_idx = r.read_u32();
                PredicateHandle ph;
                if (pred_ref == 0) {
                    if (pred_idx >= imported_preds.size())
                        return MAKE_ERROR << "invalid imported predicate index";
                    ph = imported_preds[pred_idx];
                } else {
                    if (pred_idx >= exported_preds.size())
                        return MAKE_ERROR << "invalid exported predicate index";
                    ph = exported_preds[pred_idx];
                }
                uint32_t arg_count = r.read_u32();
                std::vector<Term> args;
                args.reserve(arg_count);
                for (uint32_t j = 0; j < arg_count; ++j) {
                    args.push_back(read_term(r, formula_map));
                }
                Formula f(PredicateInstance(ph, std::move(args)));
                f.content_hash_ = static_cast<size_t>(content_hash);
                fh = ctx.formulas_.register_item(std::move(f));
                break;
            }
            case FormulaTag::Compound: {
                auto op = static_cast<Op>(r.read_u8());
                uint32_t left_id = r.read_u32();
                uint32_t right_id = r.read_u32();
                FormulaHandle left_h, right_h;
                if (left_id > 0 && left_id <= formula_map.size())
                    left_h = formula_map[left_id - 1];
                if (right_id > 0 && right_id <= formula_map.size())
                    right_h = formula_map[right_id - 1];
                Formula f(Compound{op, left_h, right_h});
                f.content_hash_ = static_cast<size_t>(content_hash);
                fh = ctx.formulas_.register_item(std::move(f));
                break;
            }
            case FormulaTag::Quantified: {
                auto op = static_cast<Op>(r.read_u8());
                uint32_t var = r.read_u32();
                uint32_t body_id = r.read_u32();
                FormulaHandle body_h;
                if (body_id > 0 && body_id <= formula_map.size())
                    body_h = formula_map[body_id - 1];
                Formula f(Quantified{op, static_cast<var_index>(var), body_h});
                f.content_hash_ = static_cast<size_t>(content_hash);
                fh = ctx.formulas_.register_item(std::move(f));
                break;
            }
            case FormulaTag::SchemaVar: {
                uint32_t schema_id = r.read_u32();
                uint32_t arg_count = r.read_u32();
                std::vector<Term> args;
                args.reserve(arg_count);
                for (uint32_t j = 0; j < arg_count; ++j) {
                    args.push_back(read_term(r, formula_map));
                }
                Formula f(SchemaVar{schema_id, std::move(args)});
                f.content_hash_ = static_cast<size_t>(content_hash);
                fh = ctx.formulas_.register_item(std::move(f));
                break;
            }
            default:
                return MAKE_ERROR << "unknown formula tag: " << static_cast<int>(tag);
        }

        formula_map.push_back(fh);
    }

    return formula_map;
}

util::Result<std::vector<SentenceHandle>> FolLibrary::link_sentences(
    GlobalContext& ctx, Reader& r, const SectionEntry& sec,
    const std::vector<FormulaHandle>& formula_map) {

    std::vector<SentenceHandle> sentence_map;
    r.seek(sec.offset);
    uint32_t count = r.read_u32();
    sentence_map.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        r.read_u32();  // local_id
        uint32_t root_id = r.read_u32();
        FormulaHandle root_h;
        if (root_id > 0 && root_id <= formula_map.size())
            root_h = formula_map[root_id - 1];
        auto sh = ctx.add_sentence(Sentence(root_h));
        sentence_map.push_back(sh);
    }

    return sentence_map;
}

util::ResultStatus FolLibrary::link_exports(
    GlobalContext& ctx, Reader& r, const SectionEntry& sec,
    const std::vector<SentenceHandle>& sentence_map,
    const std::vector<FormulaHandle>& formula_map) {

    r.seek(sec.offset);

    // Axioms
    uint32_t axiom_count = r.read_u32();
    for (uint32_t i = 0; i < axiom_count; ++i) {
        std::string name = r.read_str();
        uint32_t sid = r.read_u32();
        uint8_t is_def = r.read_u8();
        std::string def_pred = r.read_str();
        if (sid == 0 || sid > sentence_map.size())
            return MAKE_ERROR << "invalid sentence ref in axiom: " << name;
        auto sh = sentence_map[sid - 1];
        if (is_def) {
            ctx.add_definition(def_pred, name, sh);
        } else {
            ctx.add_axiom(name, sh);
        }
    }

    // Theorems
    uint32_t theorem_count = r.read_u32();
    for (uint32_t i = 0; i < theorem_count; ++i) {
        std::string name = r.read_str();
        uint32_t sid = r.read_u32();
        uint8_t is_unproved = r.read_u8();
        if (sid == 0 || sid > sentence_map.size())
            return MAKE_ERROR << "invalid sentence ref in theorem: " << name;
        auto sh = sentence_map[sid - 1];
        if (is_unproved) {
            ctx.add_unproved_theorem(name, sh);
        } else {
            ctx.add_theorem(name, sh);
        }
    }

    // Claims
    uint32_t claim_count = r.read_u32();
    for (uint32_t i = 0; i < claim_count; ++i) {
        std::string name = r.read_str();
        uint32_t sid = r.read_u32();
        if (sid == 0 || sid > sentence_map.size())
            return MAKE_ERROR << "invalid sentence ref in claim: " << name;
        ctx.add_claim(name, sentence_map[sid - 1]);
    }

    // Schemas
    uint32_t schema_count = r.read_u32();
    for (uint32_t i = 0; i < schema_count; ++i) {
        std::string name = r.read_str();
        uint32_t body_id = r.read_u32();
        uint32_t var_count = r.read_u32();
        std::vector<std::string> var_names(var_count);
        for (uint32_t j = 0; j < var_count; ++j) var_names[j] = r.read_str();
        std::vector<size_t> var_arities(var_count);
        for (uint32_t j = 0; j < var_count; ++j) var_arities[j] = r.read_u32();
        uint8_t is_proven = r.read_u8();

        FormulaHandle body_h;
        if (body_id > 0 && body_id <= formula_map.size())
            body_h = formula_map[body_id - 1];

        ctx.add_schema(name, SchemaDefinition{body_h, std::move(var_names), std::move(var_arities)});
        if (is_proven) ctx.mark_schema_proven(name);
    }

    return util::Ok();
}

LibraryMetadata FolLibrary::read_metadata(
    Reader& r, const std::vector<SectionEntry>& sections) {

    LibraryMetadata metadata;

    auto find_section = [&](SectionType type) -> const SectionEntry* {
        for (const auto& s : sections) {
            if (s.type == type) return &s;
        }
        return nullptr;
    };

    // Proof deps
    if (auto* sec = find_section(SectionType::ProofDeps)) {
        r.seek(sec->offset);
        uint32_t count = r.read_u32();
        for (uint32_t i = 0; i < count; ++i) {
            std::string proof_name = r.read_str();
            uint32_t dep_count = r.read_u32();
            std::unordered_set<std::string> deps;
            for (uint32_t j = 0; j < dep_count; ++j) {
                deps.insert(r.read_str());
            }
            metadata.proof_deps[std::move(proof_name)] = std::move(deps);
        }
    }

    // Source files
    if (auto* sec = find_section(SectionType::SourceFiles)) {
        r.seek(sec->offset);
        uint32_t count = r.read_u32();
        metadata.source_files.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            metadata.source_files.push_back(r.read_str());
        }
    }

    return metadata;
}

// ==================== Link (main orchestrator) ====================

util::Result<LinkResult> FolLibrary::link(
    GlobalContext& ctx, Reader& r,
    const std::vector<SectionEntry>& sections) {

    auto find_section = [&](SectionType type) -> const SectionEntry* {
        for (const auto& s : sections) {
            if (s.type == type) return &s;
        }
        return nullptr;
    };

    // Phase 1: Predicates
    std::vector<PredicateHandle> imported_preds;
    std::vector<PredicateHandle> exported_preds;

    if (auto* sec = find_section(SectionType::PredicateSymtab)) {
        auto pred_result = link_predicates(ctx, r, *sec);
        if (!pred_result.ok()) return pred_result.error();
        imported_preds = std::move(pred_result.value().imported);
        exported_preds = std::move(pred_result.value().exported);
    }

    // Phase 2: Formulas
    std::vector<FormulaHandle> formula_map;

    if (auto* sec = find_section(SectionType::Formulas)) {
        auto formula_result = link_formulas(ctx, r, *sec, imported_preds, exported_preds);
        if (!formula_result.ok()) return formula_result.error();
        formula_map = std::move(formula_result.value());
    }

    // Phase 3: Sentences
    std::vector<SentenceHandle> sentence_map;

    if (auto* sec = find_section(SectionType::Sentences)) {
        auto sentence_result = link_sentences(ctx, r, *sec, formula_map);
        if (!sentence_result.ok()) return sentence_result.error();
        sentence_map = std::move(sentence_result.value());
    }

    // Phase 4: Exports
    if (auto* sec = find_section(SectionType::Exports)) {
        auto export_status = link_exports(ctx, r, *sec, sentence_map, formula_map);
        if (!export_status.ok()) return export_status.error();
    }

    // Phase 5-6: Metadata
    LinkResult result;
    result.metadata = read_metadata(r, sections);

    return result;
}

}  // namespace logic
