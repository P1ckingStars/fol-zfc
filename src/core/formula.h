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

/******** Quentifier *********/
    Exists, // Introduce my substitute variable
    Forall, // Eliminate by substitute variableNow
/******* Propositional *******/
    And,
    Or,
    Implies,
    Not,
    Iff,
    Bottom  // ⊥ (falsum)
};

// Local variable within a sentence (identified by index)
class Variable {
    var_index index_;
public:
    explicit Variable(var_index idx) : index_(idx) {}

    var_index index() const { return index_; }

    bool operator==(const Variable& other) const {
        return index_ == other.index_;
    }
};

// Tagged wrappers to distinguish variable types in variant
struct FixedVarTag { var_index idx; bool operator==(const FixedVarTag&) const = default; };
struct GeneralizedVarTag { var_index idx; bool operator==(const GeneralizedVarTag&) const = default; };

// Definite description: ιx.φ(x) — "the x such that φ(x)"
// A term-forming operator that produces a term from a formula.
struct DescriptionTag {
    var_index bound_var;
    FormulaHandle body;
    bool operator==(const DescriptionTag&) const = default;
};

// A term is a generalized var, fixed var, or definite description
struct Term {
    std::variant<GeneralizedVarTag, FixedVarTag, DescriptionTag> data;

    static Term generalized(var_index idx) { return Term{GeneralizedVarTag{idx}}; }
    static Term fixed(var_index idx) { return Term{FixedVarTag{idx}}; }
    static Term description(var_index bound_var, FormulaHandle body) {
        return Term{DescriptionTag{bound_var, body}};
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
// Binds variable at index `var` in the body formula
struct Quantified {
    Op op;  // Forall or Exists
    var_index var;  // The variable index being bound
    FormulaHandle body;

    Term get_var_term() const { return Term::generalized(var); }
    bool operator==(const Quantified& other) const {
        return op == other.op && var == other.var && body == other.body;
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

    // next_gen_var_idx_ declared first for correct initialization order
    var_index next_gen_var_idx_;
    bool has_schema_vars_;
    mutable size_t content_hash_ = 0;  // Lazy-computed structural hash, cached
    std::variant<PredicateInstance, Compound, Quantified, SentenceHandle, SchemaVar> data_;

    static var_index compute_compound_next_gen(const Compound& c) {
        var_index left_idx = c.left.valid() ? c.left.get().next_gen_var_idx_ : 0;
        var_index right_idx = c.right.valid() ? c.right.get().next_gen_var_idx_ : 0;
        return std::max(left_idx, right_idx);
    }

    // Compute next_gen_var_idx_ for predicates containing DescriptionTag args
    // Only descriptions need handling: they bind variables without a wrapping Quantified node.
    // Generalized vars in predicate args are always inside a Quantified that already
    // increments next_gen_var_idx_, so they don't need to be counted here.
    static var_index compute_pred_next_gen(const PredicateInstance& p) {
        var_index max_idx = 0;
        for (const auto& t : p.args()) {
            if (t.is_description()) {
                const auto& d = t.as_description();
                var_index body_idx = d.body.get().next_gen_var_idx_;
                max_idx = std::max(max_idx, std::max(body_idx, d.bound_var + 1));
            }
        }
        return max_idx;
    }

    static bool compute_pred_has_schema_vars(const PredicateInstance& p) {
        for (const auto& t : p.args()) {
            if (t.is_description() && t.as_description().body.get().has_schema_vars_)
                return true;
        }
        return false;
    }

    explicit Formula(PredicateInstance p) :
        next_gen_var_idx_(compute_pred_next_gen(p)),
        has_schema_vars_(compute_pred_has_schema_vars(p)),
        data_(std::move(p)) {}
    explicit Formula(Compound c) :
        next_gen_var_idx_(compute_compound_next_gen(c)),
        has_schema_vars_((c.left.valid() && c.left.get().has_schema_vars_) ||
                         (c.right.valid() && c.right.get().has_schema_vars_)),
        data_(std::move(c)) {}
    explicit Formula(Quantified q) :
        next_gen_var_idx_(q.body.get().next_gen_var_idx_ + 1),
        has_schema_vars_(q.body.get().has_schema_vars_),
        data_(std::move(q)) {}
    explicit Formula(SentenceHandle s);
    static var_index compute_schema_var_next_gen(const SchemaVar& sv) {
        var_index max_idx = 0;
        for (const auto& t : sv.args) {
            if (t.is_description()) {
                const auto& d = t.as_description();
                max_idx = std::max(max_idx, std::max(d.body.get().next_gen_var_idx_, d.bound_var + 1));
            }
        }
        return max_idx;
    }

    explicit Formula(SchemaVar sv) :
        next_gen_var_idx_(compute_schema_var_next_gen(sv)),
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
    
    var_index get_next_gen_var_idx() {
        return next_gen_var_idx_;
    }

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

// Alpha-equivalence: equal up to renaming of quantifier-bound variables.
bool alpha_equiv(const Formula& a, const Formula& b);

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

    FormulaHandle predicate(PredicateHandle pred, std::vector<Term> args) {
        for (const auto& t : args) {
            if (!t.is_description()) {
                use_var(t.as_variable());
            }
            // Description terms: body vars already tracked during body construction
        }
        return add_formula(Formula(PredicateInstance(pred, std::move(args))));
    }

    FormulaHandle make_and(FormulaHandle l, FormulaHandle r) { return add_formula(Formula(Compound{Op::And, l, r})); }
    FormulaHandle make_or(FormulaHandle l, FormulaHandle r) { return add_formula(Formula(Compound{Op::Or, l, r})); }
    FormulaHandle make_implies(FormulaHandle l, FormulaHandle r) { return add_formula(Formula(Compound{Op::Implies, l, r})); }
    FormulaHandle make_iff(FormulaHandle l, FormulaHandle r) { return add_formula(Formula(Compound{Op::Iff, l, r})); }
    FormulaHandle make_not(FormulaHandle f) { return add_formula(Formula(Compound{Op::Not, f, FormulaHandle{}})); }
    FormulaHandle make_bottom() { return add_formula(Formula(Compound{Op::Bottom, FormulaHandle{}, FormulaHandle{}})); }
    // Build quantified formula with explicit var_index (for testing alpha-equivalence).
    FormulaHandle make_quantified(Op op, var_index var, FormulaHandle body) {
        return add_formula(Formula(Quantified{op, var, body}));
    }

    FormulaHandle make_schema_var(size_t id, std::vector<Term> args = {}) {
        for (const auto& t : args) {
            if (!t.is_description()) use_var(t.as_variable());
        }
        return add_formula(Formula(SchemaVar{id, std::move(args)}));
    }

    // Add a sentence's root formula to the builder (for using theorems in proofs)
    FormulaHandle add_sentence(SentenceHandle s) {
        // Just return the sentence's root - it's already in the global registry
        return s.get().root();
    }

    // Compute the true maximum generalized var index + 1 in a formula subtree.
    // Unlike next_gen_var_idx_ (which skips gen vars in predicate args by design),
    // this counts ALL generalized vars. Used for capture-safe alpha-renaming.
    static var_index true_next_gen_var(FormulaHandle h) {
        const Formula& f = h.get();
        var_index m = 0;

        auto check_term = [&](const Term& t) {
            if (t.is_generalized()) m = std::max(m, t.as_variable() + 1);
            if (t.is_description()) {
                m = std::max(m, t.as_description().bound_var + 1);
                m = std::max(m, true_next_gen_var(t.as_description().body));
            }
        };

        if (f.is_predicate()) {
            for (const auto& t : f.as_predicate().args()) check_term(t);
        } else if (f.is_compound()) {
            const auto& c = f.as_compound();
            if (c.left.valid()) m = std::max(m, true_next_gen_var(c.left));
            if (c.right.valid()) m = std::max(m, true_next_gen_var(c.right));
        } else if (f.is_quantified()) {
            m = std::max(m, f.as_quantified().var + 1);
            m = std::max(m, true_next_gen_var(f.as_quantified().body));
        } else if (f.is_schema_var()) {
            for (const auto& t : f.as_schema_var().args) check_term(t);
        }
        return m;
    }

    // Translate within a term: handles DescriptionTag recursion with capture avoidance
    Term translate_in_term(const Term& t, const Term& old_term, const Term& new_term) {
        if (t == old_term) return new_term;
        if (!t.is_description()) return t;
        const auto& d = t.as_description();
        // Capture avoidance: if substituting generalized(k) and this description binds k,
        // skip — k is locally bound here, not free. Fixed-var substitutions never need
        // this guard because FixedVarTag and GeneralizedVarTag are distinct types.
        if (old_term.is_generalized() && old_term.as_variable() == d.bound_var) return t;
        // Capture avoidance for new_term: if substituting IN generalized(k) and this
        // description binds k, alpha-rename the description to a fresh index first.
        if (new_term.is_generalized() && new_term.as_variable() == d.bound_var) {
            // true_next_gen_var(d.body) >= bound_var + 1 for non-vacuous descriptions
            // (Gen(bound_var) appears in body). The max with bound_var + 1 guards against
            // vacuous descriptions where the bound var doesn't appear in the body,
            // which would make fresh == bound_var and cause infinite recursion.
            var_index fresh = std::max(true_next_gen_var(d.body), d.bound_var + 1);
            FormulaHandle renamed_body = translate_term(d.body, Term::generalized(d.bound_var), Term::generalized(fresh));
            return translate_in_term(Term::description(fresh, renamed_body), old_term, new_term);
        }
        FormulaHandle new_body = translate_term(d.body, old_term, new_term);
        if (new_body != d.body) return Term::description(d.bound_var, new_body);
        return t;
    }

    // Generalize a fixed variable: replace Term::fixed(var_idx) with Term::generalized(var_idx)
    // Returns a new formula handle with the transformation applied
    FormulaHandle translate_term(FormulaHandle const &h, Term const &old_term, Term const &new_term) {
        const Formula& f = h.get();

        if (f.is_schema_var()) {
            const auto& sv = f.as_schema_var();
            if (sv.args.empty()) return h;
            std::vector<Term> new_args;
            bool changed = false;
            for (const auto& t : sv.args) {
                Term nt = translate_in_term(t, old_term, new_term);
                new_args.push_back(nt);
                if (!(nt == t)) changed = true;
            }
            if (changed) return add_formula(Formula(SchemaVar{sv.id, std::move(new_args)}));
            return h;
        }

        if (f.is_predicate()) {
            const PredicateInstance& p = f.as_predicate();
            std::vector<Term> new_args;
            bool changed = false;
            for (const Term& t : p.args()) {
                Term new_t = translate_in_term(t, old_term, new_term);
                new_args.push_back(new_t);
                if (!(new_t == t)) changed = true;
            }
            if (changed) {
                return add_formula(Formula(PredicateInstance(p.predicate(), std::move(new_args))));
            }
            return h;
        }
        else if (f.is_compound()) {
            const Compound& c = f.as_compound();
            FormulaHandle new_left = c.left.valid() ? translate_term(c.left, old_term, new_term) : c.left;
            FormulaHandle new_right = c.right.valid() ? translate_term(c.right, old_term, new_term) : c.right;
            if (new_left != c.left || new_right != c.right) {
                return add_formula(Formula(Compound{c.op, new_left, new_right}));
            }
            return h;
        }
        else if (f.is_quantified()) {
            const Quantified& q = f.as_quantified();
            // Capture avoidance: if substituting generalized(k) and this quantifier
            // binds k, skip — k is locally bound here. This matters when description
            // bodies contain quantifiers that reuse the same generalized index.
            if (old_term.is_generalized() && old_term.as_variable() == q.var) return h;
            // Capture avoidance for new_term: if substituting IN generalized(k) and this
            // quantifier binds k, alpha-rename the quantifier to a fresh index first.
            if (new_term.is_generalized() && new_term.as_variable() == q.var) {
                var_index fresh = true_next_gen_var(h);
                FormulaHandle renamed_body = translate_term(q.body, Term::generalized(q.var), Term::generalized(fresh));
                FormulaHandle renamed = add_formula(Formula(Quantified{q.op, fresh, renamed_body}));
                return translate_term(renamed, old_term, new_term);
            }
            FormulaHandle new_body = translate_term(q.body, old_term, new_term);
            if (new_body != q.body) {
                return add_formula(Formula(Quantified{q.op, q.var, new_body}));
            }
            return h;
        }
        // Already a sentence reference - no transformation needed
        return h;
    }

    // Helper: instantiate schema vars within a term (for description bodies)
    Term instantiate_schema_in_term(const Term& t, const std::vector<SchemaBind>& bindings) {
        if (!t.is_description()) return t;
        const auto& d = t.as_description();
        auto nb = instantiate_schema(d.body, bindings);
        if (nb != d.body) return Term::description(d.bound_var, nb);
        return t;
    }

    // Schema instantiation: replace SchemaVar nodes with bindings.
    // Arity-0 bindings: direct formula substitution.
    // Arity-N bindings: lambda substitution — replace lambda params with actual args.
    FormulaHandle instantiate_schema(FormulaHandle h, const std::vector<SchemaBind>& bindings) {
        const Formula& f = h.get();
        if (f.is_schema_var()) {
            const auto& sv = f.as_schema_var();
            const auto& bind = bindings[sv.id];
            if (bind.params.empty()) {
                // Arity-0: direct formula substitution
                return bind.body;
            }
            // Arity-N: apply lambda — substitute each param with corresponding arg
            FormulaHandle result = bind.body;
            for (size_t i = 0; i < bind.params.size() && i < sv.args.size(); ++i) {
                Term arg = instantiate_schema_in_term(sv.args[i], bindings);
                result = translate_term(result, Term::fixed(bind.params[i]), arg);
            }
            return result;
        }
        if (!f.has_schema_vars()) return h;  // fast path

        if (f.is_compound()) {
            const auto& c = f.as_compound();
            auto nl = c.left.valid() ? instantiate_schema(c.left, bindings) : c.left;
            auto nr = c.right.valid() ? instantiate_schema(c.right, bindings) : c.right;
            if (nl != c.left || nr != c.right)
                return add_formula(Formula(Compound{c.op, nl, nr}));
            return h;
        }
        if (f.is_quantified()) {
            const auto& q = f.as_quantified();
            auto nb = instantiate_schema(q.body, bindings);
            if (nb != q.body)
                return add_formula(Formula(Quantified{q.op, q.var, nb}));
            return h;
        }
        if (f.is_predicate()) {
            const auto& p = f.as_predicate();
            bool changed = false;
            std::vector<Term> new_args;
            for (const auto& t : p.args()) {
                Term nt = instantiate_schema_in_term(t, bindings);
                new_args.push_back(nt);
                if (!(nt == t)) changed = true;
            }
            if (changed)
                return add_formula(Formula(PredicateInstance(p.predicate(), std::move(new_args))));
            return h;
        }
        return h;
    }

    // Build sentence if formula is closed (no free vars, no schema vars)
    SentenceHandle build_sentence(FormulaHandle root, var_index start = 0) {
        if (!is_closed_since(start)) return SentenceHandle{};
        if (root.get().has_schema_vars()) return SentenceHandle{};
        return ctx_.add_sentence(Sentence(root));
    }
};

// RAII scope for quantifiers. Creates bound variable on construction,
// builds quantified formula on destruction.
// Usage:
//   QuantifierBuilder forall(builder, Op::Forall);
//   FormulaHandle body = builder.predicate(P, {forall.var()});
//   forall.set_body(body);
//   // destructor creates the quantified formula
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
            // Generalize the fixed var in body before creating quantified formula
            // gen_idx is the generalized index for this quantifier's bound variable
            var_index gen_idx = body_.get().next_gen_var_idx_;
            FormulaHandle generalized_body = builder_.translate_term(body_, var(), Term::generalized(gen_idx));
            handle_ = builder_.add_formula(Formula(Quantified{op_, gen_idx, generalized_body}));
            // If this is a closed formula (sentence), register it but keep handle_ as the formula
            // Don't wrap in SentenceHandle - the formula itself is what we work with
            builder_.build_sentence(handle_, start_depth_);
        }
    }

    Op get_op() const { return op_; }

    // Get bound variable as Term (fixed/instantiated during proof construction)
    Term var() const { return Term::fixed(var_); }
    var_index var_idx() const { return var_; }

    // Set the body formula
    void set_body(FormulaHandle body) { body_ = body; }

    QuantifierBuilder(const QuantifierBuilder&) = delete;
    QuantifierBuilder& operator=(const QuantifierBuilder&) = delete;
};

// RAII scope for definite descriptions. Creates bound variable on construction,
// builds DescriptionTag term on destruction.
// Usage:
//   Term result;
//   {
//       DescriptionBuilder db(builder, result);
//       FormulaHandle body = builder.predicate(P, {db.var()});
//       db.set_body(body);
//   }
//   // result is now Term::description(gen_idx, generalized_body)
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
            var_index gen_idx = body_.get().next_gen_var_idx_;
            FormulaHandle gen_body = builder_.translate_term(body_, var(), Term::generalized(gen_idx));
            result_ = Term::description(gen_idx, gen_body);
        }
    }

    Term var() const { return Term::fixed(var_); }
    var_index var_idx() const { return var_; }

    void set_body(FormulaHandle body) { body_ = body; }

    DescriptionBuilder(const DescriptionBuilder&) = delete;
    DescriptionBuilder& operator=(const DescriptionBuilder&) = delete;
};

} // end logic
