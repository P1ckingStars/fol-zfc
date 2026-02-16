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

// A term is a generalized var or fixed var
struct Term {
    std::variant<GeneralizedVarTag, FixedVarTag> data;

    static Term generalized(var_index idx) { return Term{GeneralizedVarTag{idx}}; }
    static Term fixed(var_index idx) { return Term{FixedVarTag{idx}}; }

    bool is_generalized() const { return std::holds_alternative<GeneralizedVarTag>(data); }
    bool is_fixed() const { return std::holds_alternative<FixedVarTag>(data); }

    var_index as_variable() const {
        if (is_generalized()) return std::get<GeneralizedVarTag>(data).idx;
        return std::get<FixedVarTag>(data).idx;
    }

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

    // Key for KeyedRegistry - predicates are keyed by name
    std::string get_key() const { return name_; }
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

class Formula {
    friend class FormulaBuilder;
    friend class QuantifierBuilder;
    friend class Sentence;

    // next_gen_var_idx_ declared first for correct initialization order
    var_index next_gen_var_idx_;
    std::variant<PredicateInstance, Compound, Quantified, SentenceHandle> data_;

    static var_index compute_compound_next_gen(const Compound& c) {
        var_index left_idx = c.left.valid() ? c.left.get().next_gen_var_idx_ : 0;
        var_index right_idx = c.right.valid() ? c.right.get().next_gen_var_idx_ : 0;
        return std::max(left_idx, right_idx);
    }

    explicit Formula(PredicateInstance p) : next_gen_var_idx_(0), data_(std::move(p)) {}
    explicit Formula(Compound c) :
        next_gen_var_idx_(compute_compound_next_gen(c)),
        data_(std::move(c)) {}
    explicit Formula(Quantified q) :
        next_gen_var_idx_(q.body.get().next_gen_var_idx_ + 1),
        data_(std::move(q)) {}
    explicit Formula(SentenceHandle s);

public:
    bool operator==(const Formula& other) const {
        return data_ == other.data_;
    }

    bool is_predicate() const { return std::holds_alternative<PredicateInstance>(data_); }
    bool is_compound() const { return std::holds_alternative<Compound>(data_); }
    bool is_quantified() const { return std::holds_alternative<Quantified>(data_); }
    bool is_sentence() const { return std::holds_alternative<SentenceHandle>(data_); }

    const PredicateInstance& as_predicate() const { return std::get<PredicateInstance>(data_); }
    const Compound& as_compound() const { return std::get<Compound>(data_); }
    const Quantified& as_quantified() const { return std::get<Quantified>(data_); }
    SentenceHandle as_sentence() const { return std::get<SentenceHandle>(data_); }
    
    var_index get_next_gen_var_idx() {
        return next_gen_var_idx_;
    }

    std::string to_string() const;

    friend std::ostream& operator<<(std::ostream& os, const Formula& f) {
        return os << f.to_string();
    }
};

struct FormulaHash {
    size_t operator()(const Formula& f) const {
        return std::hash<std::string>{}(f.to_string());
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

class GlobalContext {
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
            // All terms are variables now (generalized or fixed)
            use_var(t.as_variable());
        }
        return add_formula(Formula(PredicateInstance(pred, std::move(args))));
    }

    FormulaHandle make_and(FormulaHandle l, FormulaHandle r) { return add_formula(Formula(Compound{Op::And, l, r})); }
    FormulaHandle make_or(FormulaHandle l, FormulaHandle r) { return add_formula(Formula(Compound{Op::Or, l, r})); }
    FormulaHandle make_implies(FormulaHandle l, FormulaHandle r) { return add_formula(Formula(Compound{Op::Implies, l, r})); }
    FormulaHandle make_iff(FormulaHandle l, FormulaHandle r) { return add_formula(Formula(Compound{Op::Iff, l, r})); }
    FormulaHandle make_not(FormulaHandle f) { return add_formula(Formula(Compound{Op::Not, f, FormulaHandle{}})); }
    FormulaHandle make_bottom() { return add_formula(Formula(Compound{Op::Bottom, FormulaHandle{}, FormulaHandle{}})); }

    // Add a sentence's root formula to the builder (for using theorems in proofs)
    FormulaHandle add_sentence(SentenceHandle s) {
        // Just return the sentence's root - it's already in the global registry
        return s.get().root();
    }

    // Generalize a fixed variable: replace Term::fixed(var_idx) with Term::generalized(var_idx)
    // Returns a new formula handle with the transformation applied
    FormulaHandle translate_term(FormulaHandle const &h, Term const &old_term, Term const &new_term) {
        const Formula& f = h.get();

        if (f.is_predicate()) {
            const PredicateInstance& p = f.as_predicate();
            std::vector<Term> new_args;
            bool changed = false;
            for (const Term& t : p.args()) {
                if (t == old_term) {
                    new_args.push_back(new_term);
                    changed = true;
                } else {
                    new_args.push_back(t);
                }
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
            FormulaHandle new_body = translate_term(q.body, old_term, new_term);
            if (new_body != q.body) {
                return add_formula(Formula(Quantified{q.op, q.var, new_body}));
            }
            return h;
        }
        // Already a sentence reference - no transformation needed
        return h;
    }

    // Build sentence if formula is closed
    SentenceHandle build_sentence(FormulaHandle root, var_index start = 0) {
        if (!is_closed_since(start)) return SentenceHandle{};
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

} // end logic
