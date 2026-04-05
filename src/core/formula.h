#pragma once

#include "../util/registry.h"

#include <functional>
#include <memory>
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace logic {

// Forward declarations for Handle types
class Formula;
class Predicate;
class Sentence;
class FormulaBuilder;
class QuantifierBuilder;

// Handle types - external code uses these, not raw IDs
using FormulaHandle = util::Handle<Formula>;
using PredicateHandle = util::Handle<Predicate>;
using SentenceHandle = util::Handle<Sentence>;

using var_index = size_t;  // Local variable index within a sentence

enum class Op {

/******** Quantifier *********/
    Exists,
    Forall,
/******* Propositional *******/
    And,
    Or,
    Implies,
    Not,
    Iff,
    Bottom  // ⊥ (falsum)
};

// Tagged wrappers to distinguish variable types in variant
struct FixedVarTag { var_index idx; bool operator==(const FixedVarTag&) const = default; };
struct GeneralizedVarTag { var_index idx; bool operator==(const GeneralizedVarTag&) const = default; };

// Definite description: ιx.φ(x) — "the x such that φ(x)"
// A term-forming operator that produces a term from a formula.
// De Bruijn: Gen(0) in body is the described variable. No explicit bound_var needed.
struct DescriptionTag {
    FormulaHandle body;
    bool operator==(const DescriptionTag&) const = default;
};

// A term is a generalized var, fixed var, or definite description
struct Term {
    std::variant<GeneralizedVarTag, FixedVarTag, DescriptionTag> data;

    static Term generalized(var_index idx) { return Term{GeneralizedVarTag{idx}}; }
    static Term fixed(var_index idx) { return Term{FixedVarTag{idx}}; }
    static Term description(FormulaHandle body) {
        return Term{DescriptionTag{body}};
    }

    bool is_generalized() const { return std::holds_alternative<GeneralizedVarTag>(data); }
    bool is_fixed() const { return std::holds_alternative<FixedVarTag>(data); }
    bool is_description() const { return std::holds_alternative<DescriptionTag>(data); }

    // Only valid for generalized and fixed vars (not descriptions)
    var_index as_variable() const {
        if (is_generalized()) return std::get<GeneralizedVarTag>(data).idx;
        return std::get<FixedVarTag>(data).idx;
    }

    const DescriptionTag& as_description() const { return std::get<DescriptionTag>(data); }

    bool operator==(const Term& other) const { return data == other.data; }
};

class Predicate {
    std::string name_;
    size_t num_args_;

public:
    Predicate(std::string name, size_t num_args)
        : name_(std::move(name)), num_args_(num_args) {}

    bool operator==(const Predicate& other) const {
        return name_ == other.name_ && num_args_ == other.num_args_;
    }

    const std::string& get_name() const { return name_; }
    size_t get_num_args() const { return num_args_; }

    // Key for KeyedRegistry - predicates are keyed by name + arity.
    // In standard FOL, P/1 and P/2 are different predicate symbols.
    std::string get_key() const { return name_ + "/" + std::to_string(num_args_); }
};

struct PredicateHash {
    size_t operator()(const Predicate& p) const {
        return std::hash<std::string>{}(p.get_name()) ^
               (std::hash<size_t>{}(p.get_num_args()) << 1);
    }
};

// Predicate applied to terms: P(t1, t2, ..., tn)
class PredicateInstance {
    PredicateHandle predicate_;
    std::vector<Term> args_;

public:
    PredicateInstance(PredicateHandle pred, std::vector<Term> args)
        : predicate_(pred), args_(std::move(args)) {}

    PredicateHandle predicate() const { return predicate_; }
    const std::vector<Term>& args() const { return args_; }

    bool operator==(const PredicateInstance& other) const {
        return predicate_ == other.predicate_ && args_ == other.args_;
    }
};

// Propositional connective: op(left, right) or op(left) for unary
struct Compound {
    Op op;
    FormulaHandle left;
    FormulaHandle right;

    bool operator==(const Compound& other) const {
        return op == other.op && left == other.left && right == other.right;
    }
};

// Quantified formula: ∀x.φ or ∃x.φ
// De Bruijn: Gen(0) in body is the bound variable. No explicit var field needed.
struct Quantified {
    Op op;  // Forall or Exists
    FormulaHandle body;

    bool operator==(const Quantified& other) const {
        return op == other.op && body == other.body;
    }
};

// Schema variable: a metavariable for formula or predicate substitution.
// Arity 0 (args empty): formula placeholder — substituted with a formula.
// Arity N (args has N terms): predicate placeholder — substituted with a lambda.
// Identified by positional index; name resolution handled at parser layer.
struct SchemaVar {
    size_t id;
    std::vector<Term> args;  // empty for formula vars, [t1,...,tN] for predicate vars
    bool operator==(const SchemaVar&) const = default;
};

class Formula {
    friend class FormulaBuilder;
    friend class QuantifierBuilder;
    friend class DescriptionBuilder;
    friend class Sentence;
    friend class FolLibrary;

    // max_free_debruijn_: max De Bruijn index + 1 among free Gen vars in subtree.
    // 0 means no free Gen vars (closed wrt generalized variables).
    var_index max_free_debruijn_;
    bool has_schema_vars_;
    mutable size_t content_hash_ = 0;  // Lazy-computed structural hash, cached
    std::variant<PredicateInstance, Compound, Quantified, SentenceHandle, SchemaVar> data_;

    // Decrement De Bruijn depth by 1, clamping at 0 (binder absorbs one free level)
    static var_index dec_free(var_index x) { return x > 0 ? x - 1 : 0; }

    static var_index compute_compound_max_free(const Compound& c) {
        var_index l = c.left.valid() ? c.left.get().max_free_debruijn_ : 0;
        var_index r = c.right.valid() ? c.right.get().max_free_debruijn_ : 0;
        return std::max(l, r);
    }

    // Compute max free De Bruijn index for predicates.
    // Counts ALL Gen vars in args (unlike old compute_pred_next_gen which skipped them).
    // Description args bind one variable, so their contribution is dec_free(body.max_free).
    static var_index compute_pred_max_free(const PredicateInstance& p) {
        var_index m = 0;
        for (const auto& t : p.args()) {
            if (t.is_generalized()) m = std::max(m, t.as_variable() + 1);
            if (t.is_description()) m = std::max(m, dec_free(t.as_description().body.get().max_free_debruijn_));
        }
        return m;
    }

    static bool compute_pred_has_schema_vars(const PredicateInstance& p) {
        for (const auto& t : p.args()) {
            if (t.is_description() && t.as_description().body.get().has_schema_vars_)
                return true;
        }
        return false;
    }

    static var_index compute_schema_var_max_free(const SchemaVar& sv) {
        var_index m = 0;
        for (const auto& t : sv.args) {
            if (t.is_generalized()) m = std::max(m, t.as_variable() + 1);
            if (t.is_description()) m = std::max(m, dec_free(t.as_description().body.get().max_free_debruijn_));
        }
        return m;
    }

    explicit Formula(PredicateInstance p) :
        max_free_debruijn_(compute_pred_max_free(p)),
        has_schema_vars_(compute_pred_has_schema_vars(p)),
        data_(std::move(p)) {}
    explicit Formula(Compound c) :
        max_free_debruijn_(compute_compound_max_free(c)),
        has_schema_vars_((c.left.valid() && c.left.get().has_schema_vars_) ||
                         (c.right.valid() && c.right.get().has_schema_vars_)),
        data_(std::move(c)) {}
    explicit Formula(Quantified q) :
        max_free_debruijn_(dec_free(q.body.get().max_free_debruijn_)),
        has_schema_vars_(q.body.get().has_schema_vars_),
        data_(std::move(q)) {}
    explicit Formula(SentenceHandle s);

    explicit Formula(SchemaVar sv) :
        max_free_debruijn_(compute_schema_var_max_free(sv)),
        has_schema_vars_(true),
        data_(std::move(sv)) {}

public:
    bool operator==(const Formula& other) const {
        return data_ == other.data_;
    }

    bool is_predicate() const { return std::holds_alternative<PredicateInstance>(data_); }
    bool is_compound() const { return std::holds_alternative<Compound>(data_); }
    bool is_quantified() const { return std::holds_alternative<Quantified>(data_); }
    bool is_sentence() const { return std::holds_alternative<SentenceHandle>(data_); }
    bool is_schema_var() const { return std::holds_alternative<SchemaVar>(data_); }

    const PredicateInstance& as_predicate() const { return std::get<PredicateInstance>(data_); }
    const Compound& as_compound() const { return std::get<Compound>(data_); }
    const Quantified& as_quantified() const { return std::get<Quantified>(data_); }
    SentenceHandle as_sentence() const { return std::get<SentenceHandle>(data_); }
    const SchemaVar& as_schema_var() const { return std::get<SchemaVar>(data_); }
    bool has_schema_vars() const { return has_schema_vars_; }

    var_index max_free_debruijn() const { return max_free_debruijn_; }

    std::string to_string() const;

    // Deterministic structural hash, computed lazily and cached.
    // Used by FormulaHash for O(1) registry dedup instead of to_string().
    size_t content_hash() const;

    friend std::ostream& operator<<(std::ostream& os, const Formula& f) {
        return os << f.to_string();
    }
};

struct FormulaHash {
    size_t operator()(const Formula& f) const {
        return f.content_hash();
    }
};

// Registry type aliases using the new handle-based registries
// Note: Id parameter in KeyedRegistry is vestigial but kept for compatibility
using FormulaRegistry = util::Registry<Formula, FormulaHash>;
using PredicateRegistry = util::KeyedRegistry<Predicate, size_t, std::string, std::hash<std::string>>;

// A sentence is a closed formula (no free variables)
// All variables are bound by quantifiers
// Sentences are lightweight - just a handle to the root formula in the shared registry
class Sentence {
    FormulaHandle root_;

public:
    Sentence() = default;
    explicit Sentence(FormulaHandle root) : root_(root) {}

    FormulaHandle root() const { return root_; }
    const Formula& get_formula(FormulaHandle h) const { return h.get(); }

    bool operator==(const Sentence& other) const {
        // Compare by root handle (same formula in shared registry = equal)
        return root_ == other.root_;
    }

    std::string to_string() const;

    friend std::ostream& operator<<(std::ostream& os, const Sentence& s) {
        return os << s.to_string();
    }
};

struct SentenceHash {
    size_t operator()(const Sentence& s) const {
        return std::hash<std::string>{}(s.to_string());
    }
};

using SentenceRegistry = util::Registry<Sentence, SentenceHash>;

// Hash for SentenceHandle (uses Handle's hash_value())
struct SentenceHandleHash {
    size_t operator()(const SentenceHandle& h) const {
        return h.hash_value();
    }
};

// A schema binding: either a formula (arity 0) or a lambda predicate (arity N).
// For arity 0, body is the substituted formula and params is empty.
// For arity N, body contains fixed vars from params; at instantiation,
// each SchemaVar arg is substituted for the corresponding param.
struct SchemaBind {
    FormulaHandle body;
    std::vector<var_index> params;  // lambda parameter indices (empty for arity 0)

    explicit SchemaBind() = default;
    explicit SchemaBind(FormulaHandle f) : body(f) {}
    SchemaBind(std::vector<var_index> p, FormulaHandle f) : body(f), params(std::move(p)) {}
};

// A schema is a formula template parameterized by metavariables (SchemaVar).
// Variables can be arity 0 (formula substitution) or arity N (predicate substitution).
struct SchemaDefinition {
    FormulaHandle body;                  // contains SchemaVar nodes
    std::vector<std::string> var_names;  // var_names[id] = name
    std::vector<size_t> var_arities;     // var_arities[id] = arity (0 = formula, N = N-arg predicate)
};

class GlobalContext {
    friend class FolLibrary;

    FormulaRegistry formulas_;  // Single shared formula registry
    SentenceRegistry sentences_;
    PredicateRegistry predicates_;

    // Known axioms and theorems (proven truths)
    std::unordered_set<SentenceHandle, SentenceHandleHash> known_;
    std::unordered_map<std::string, SentenceHandle> named_axioms_;
    std::unordered_map<std::string, SentenceHandle> named_theorems_;

    // Claims (to be proven)
    std::unordered_map<std::string, SentenceHandle> named_claims_;

    // Predicates that have been defined via @def (predicate name -> axiom name)
    std::unordered_map<std::string, std::string> defined_predicates_;

    // Theorems marked UNPROVED (assumed without proof)
    std::unordered_set<std::string> unproved_theorems_;

    // Schema storage
    std::unordered_map<std::string, SchemaDefinition> named_schemas_;
    std::unordered_set<std::string> proven_schemas_;

public:
    // Access the shared formula registry
    FormulaRegistry& formulas() { return formulas_; }
    const FormulaRegistry& formulas() const { return formulas_; }

    SentenceHandle add_sentence(Sentence s) { return sentences_.register_item(std::move(s)); }
    PredicateHandle add_predicate(std::string name, size_t arity) {
        return predicates_.register_item(Predicate(std::move(name), arity));
    }
    const Sentence& get_sentence(SentenceHandle h) const { return h.get(); }
    const Predicate& get_predicate(PredicateHandle h) const { return h.get(); }

    // Axiom/Theorem API
    void add_axiom(const std::string& name, SentenceHandle sentence) {
        named_axioms_[name] = sentence;
        known_.insert(sentence);
    }

    // Register a @def axiom: marks the predicate as defined and adds the axiom
    // Idempotent: re-registering the same predicate with the same axiom is a no-op
    // (needed for #pragma once semantics when files are loaded multiple times)
    void add_definition(const std::string& predicate_name, const std::string& axiom_name, SentenceHandle sentence) {
        defined_predicates_[predicate_name] = axiom_name;
        add_axiom(axiom_name, sentence);
    }

    bool is_defined(const std::string& predicate_name) const {
        return defined_predicates_.count(predicate_name) > 0;
    }

    // Check if this is a re-registration of the same definition (idempotent)
    bool is_same_definition(const std::string& predicate_name, const std::string& axiom_name) const {
        auto it = defined_predicates_.find(predicate_name);
        return it != defined_predicates_.end() && it->second == axiom_name;
    }

    void add_theorem(const std::string& name, SentenceHandle sentence) {
        named_theorems_[name] = sentence;
        known_.insert(sentence);
    }

    void add_unproved_theorem(const std::string& name, SentenceHandle sentence) {
        named_theorems_[name] = sentence;
        known_.insert(sentence);
        unproved_theorems_.insert(name);
    }

    bool is_unproved(const std::string& name) const {
        return unproved_theorems_.count(name) > 0;
    }

    const std::unordered_set<std::string>& unproved_theorems() const {
        return unproved_theorems_;
    }

    bool is_known(SentenceHandle sentence) const {
        return known_.count(sentence) > 0;
    }

    std::optional<SentenceHandle> find_axiom(const std::string& name) const {
        auto it = named_axioms_.find(name);
        if (it != named_axioms_.end()) return it->second;
        return std::nullopt;
    }

    std::optional<SentenceHandle> find_theorem(const std::string& name) const {
        auto it = named_theorems_.find(name);
        if (it != named_theorems_.end()) return it->second;
        return std::nullopt;
    }

    std::optional<SentenceHandle> find_known(const std::string& name) const {
        if (auto ax = find_axiom(name)) return ax;
        return find_theorem(name);
    }

    // Claim API (for theorems to be proven)
    void add_claim(const std::string& name, SentenceHandle sentence) {
        named_claims_[name] = sentence;
    }

    std::optional<SentenceHandle> find_claim(const std::string& name) const {
        auto it = named_claims_.find(name);
        if (it != named_claims_.end()) return it->second;
        return std::nullopt;
    }

    const std::unordered_map<std::string, SentenceHandle>& claims() const {
        return named_claims_;
    }

    const std::unordered_map<std::string, SentenceHandle>& axioms() const {
        return named_axioms_;
    }

    const std::unordered_map<std::string, SentenceHandle>& theorems() const {
        return named_theorems_;
    }

    // Schema API
    void add_schema(const std::string& name, SchemaDefinition def) {
        named_schemas_[name] = std::move(def);
    }
    void mark_schema_proven(const std::string& name) {
        proven_schemas_.insert(name);
    }
    bool is_schema_proven(const std::string& name) const {
        return proven_schemas_.count(name) > 0;
    }
    std::optional<SchemaDefinition> find_schema(const std::string& name) const {
        auto it = named_schemas_.find(name);
        if (it != named_schemas_.end()) return it->second;
        return std::nullopt;
    }
};

// FormulaBuilder builds formulas and tracks variable scope.
// local_vars_ = current quantifier depth (next var to bind has this index)
// used_vars_ = minimum variable index used (for detecting free variables)
// A formula is a sentence if used_vars_ >= start depth (no free vars from parent scope)
class FormulaBuilder {
    GlobalContext& ctx_;
    var_index local_vars_{0};  // Current depth / next var index
    var_index used_vars_{static_cast<var_index>(-1)};  // Min var used (init to max)

public:
    explicit FormulaBuilder(GlobalContext& ctx) : ctx_(ctx) {}

    // Access the global context
    GlobalContext& context() { return ctx_; }

    // Scope management for QuantifierBuilder
    var_index enter_scope() { return local_vars_++; }
    void exit_scope() { local_vars_--; }
    var_index depth() const { return local_vars_; }

    // Track variable usage - call when using a variable in formula
    void use_var(var_index v) {
        if (v < used_vars_) used_vars_ = v;
    }

    // Check if closed since given depth (no free vars from parent)
    bool is_closed_since(var_index start) const {
        return used_vars_ >= start;
    }

    // Formula creation - uses the global registry
    FormulaHandle add_formula(Formula f) {
        return ctx_.formulas().register_item(std::move(f));
    }

    FormulaHandle predicate(PredicateHandle pred, std::vector<Term> args);

    FormulaHandle make_and(FormulaHandle l, FormulaHandle r) { return add_formula(Formula(Compound{Op::And, l, r})); }
    FormulaHandle make_or(FormulaHandle l, FormulaHandle r) { return add_formula(Formula(Compound{Op::Or, l, r})); }
    FormulaHandle make_implies(FormulaHandle l, FormulaHandle r) { return add_formula(Formula(Compound{Op::Implies, l, r})); }
    FormulaHandle make_iff(FormulaHandle l, FormulaHandle r) { return add_formula(Formula(Compound{Op::Iff, l, r})); }
    FormulaHandle make_not(FormulaHandle f) { return add_formula(Formula(Compound{Op::Not, f, FormulaHandle{}})); }
    FormulaHandle make_bottom() { return add_formula(Formula(Compound{Op::Bottom, FormulaHandle{}, FormulaHandle{}})); }
    // Build quantified formula with De Bruijn body (Gen(0) = bound var).
    FormulaHandle make_quantified(Op op, FormulaHandle body) {
        return add_formula(Formula(Quantified{op, body}));
    }

    FormulaHandle make_schema_var(size_t id, std::vector<Term> args = {});

    // Add a sentence's root formula to the builder (for using theorems in proofs)
    FormulaHandle add_sentence(SentenceHandle s) {
        return s.get().root();
    }

    // ========== De Bruijn Operations ==========

    // Shift free De Bruijn indices. Gen(k) → Gen(k+delta) if k >= cutoff.
    Term shift_term(const Term& t, int cutoff, int delta);
    FormulaHandle shift_formula(FormulaHandle h, int cutoff, int delta);

    // Abstract: replace fixed(fixed_var) with Gen(depth) and shift existing free Gen up.
    Term abstract_var_in_term(const Term& t, var_index fixed_var, int depth);
    FormulaHandle abstract_var_impl(FormulaHandle h, var_index fixed_var, int depth);
    FormulaHandle abstract_var(FormulaHandle body, var_index fixed_var);

    // Instantiate: replace Gen(0) with term, shift remaining Gen down.
    Term instantiate_gen_in_term(const Term& t, const Term& replacement, int depth);
    FormulaHandle instantiate_gen_impl(FormulaHandle h, const Term& replacement, int depth);
    FormulaHandle instantiate_gen(FormulaHandle body, const Term& replacement);

    // Depth-aware fixed→term substitution. Used for schema arity-N lambda application.
    Term subst_fixed_in_term(const Term& t, var_index fixed_var, const Term& replacement, int depth);
    FormulaHandle subst_fixed_impl(FormulaHandle h, var_index fixed_var, const Term& replacement, int depth);

    // ========== Simple term substitution (no Gen interaction) ==========
    // Used by eq_subst: replaces one term with another throughout a formula.
    Term translate_in_term(const Term& t, const Term& old_term, const Term& new_term);
    FormulaHandle translate_term(FormulaHandle const &h, Term const &old_term, Term const &new_term);

    // ========== Schema Instantiation ==========
    Term instantiate_schema_in_term(const Term& t, const std::vector<SchemaBind>& bindings);
    FormulaHandle instantiate_schema(FormulaHandle h, const std::vector<SchemaBind>& bindings);

    // Build sentence if formula is closed (no free fixed vars AND no free Gen vars)
    SentenceHandle build_sentence(FormulaHandle root, var_index start = 0) {
        if (!is_closed_since(start)) return SentenceHandle{};
        if (root.get().has_schema_vars()) return SentenceHandle{};
        if (root.get().max_free_debruijn_ != 0) return SentenceHandle{};
        return ctx_.add_sentence(Sentence(root));
    }
};

// RAII scope for quantifiers. Creates bound variable on construction,
// builds quantified formula on destruction using De Bruijn abstraction.
// Usage:
//   QuantifierBuilder forall(builder, Op::Forall);
//   FormulaHandle body = builder.predicate(P, {forall.var()});
//   forall.set_body(body);
//   // destructor abstracts fixed var → Gen(0) and wraps in Quantified
class QuantifierBuilder {
    FormulaBuilder& builder_;
    Op op_;
    var_index var_;
    var_index start_depth_;
    FormulaHandle body_;
    FormulaHandle& handle_;
public:
    QuantifierBuilder(FormulaBuilder& builder, Op op, FormulaHandle & handle)
        : builder_(builder), op_(op), start_depth_(builder.depth()), handle_(handle)
    {
        var_ = builder_.enter_scope();
    }

    ~QuantifierBuilder() {
        builder_.exit_scope();
        if (body_.valid()) {
            // De Bruijn: abstract the fixed var to Gen(0), shifting existing Gen up
            FormulaHandle abstracted = builder_.abstract_var(body_, var_);
            handle_ = builder_.add_formula(Formula(Quantified{op_, abstracted}));
            builder_.build_sentence(handle_, start_depth_);
        }
    }

    Op get_op() const { return op_; }

    // Get bound variable as Term (fixed during proof construction)
    Term var() const { return Term::fixed(var_); }
    var_index var_idx() const { return var_; }

    // Set the body formula
    void set_body(FormulaHandle body) { body_ = body; }

    QuantifierBuilder(const QuantifierBuilder&) = delete;
    QuantifierBuilder& operator=(const QuantifierBuilder&) = delete;
};

// RAII scope for definite descriptions. Creates bound variable on construction,
// builds DescriptionTag term on destruction using De Bruijn abstraction.
// Usage:
//   Term result;
//   {
//       DescriptionBuilder db(builder, result);
//       FormulaHandle body = builder.predicate(P, {db.var()});
//       db.set_body(body);
//   }
//   // result is now Term::description(abstracted_body) with Gen(0) = described var
class DescriptionBuilder {
    FormulaBuilder& builder_;
    var_index var_;
    FormulaHandle body_;
    Term& result_;
public:
    DescriptionBuilder(FormulaBuilder& builder, Term& result)
        : builder_(builder), result_(result)
    {
        var_ = builder_.enter_scope();
    }

    ~DescriptionBuilder() {
        builder_.exit_scope();
        if (body_.valid()) {
            FormulaHandle abstracted = builder_.abstract_var(body_, var_);
            result_ = Term::description(abstracted);
        }
    }

    Term var() const { return Term::fixed(var_); }
    var_index var_idx() const { return var_; }

    void set_body(FormulaHandle body) { body_ = body; }

    DescriptionBuilder(const DescriptionBuilder&) = delete;
    DescriptionBuilder& operator=(const DescriptionBuilder&) = delete;
};

} // end logic
