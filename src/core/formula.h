#pragma once

#include "../util/registry.h"

#include <functional>
#include <memory>
#include <set>
#include <string>
#include <variant>
#include <vector>

namespace logic {

// Forward declarations for Handle types
class Formula;
class Predicate;
class Constant;
class Sentence;
class FormulaBuilder;
class QuantifierBuilder;

// Handle types - external code uses these, not raw IDs
using FormulaHandle = util::Handle<Formula>;
using PredicateHandle = util::Handle<Predicate>;
using ConstantHandle = util::Handle<Constant>;
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

// Global constant (e.g., 0, ∅, specific sets)
class Constant {
    std::string name_;

public:
    explicit Constant(std::string name) : name_(std::move(name)) {}

    bool operator==(const Constant& other) const {
        return name_ == other.name_;
    }

    const std::string& get_name() const { return name_; }

    // Key for KeyedRegistry - constants are keyed by name
    std::string get_key() const { return name_; }
};

struct ConstantHash {
    size_t operator()(const Constant& c) const {
        return std::hash<std::string>{}(c.get_name());
    }
};

// Tagged wrappers to distinguish var_index and ConstantHandle in variant
struct VarTag { var_index idx; bool operator==(const VarTag&) const = default; };
struct ConstTag { ConstantHandle handle; bool operator==(const ConstTag&) const = default; };

// A term is either a local variable (by index) or a global constant (by handle)
struct Term {
    std::variant<VarTag, ConstTag> data;

    static Term var(var_index idx) { return Term{VarTag{idx}}; }
    static Term constant(ConstantHandle h) { return Term{ConstTag{h}}; }

    bool is_variable() const { return std::holds_alternative<VarTag>(data); }
    bool is_constant() const { return std::holds_alternative<ConstTag>(data); }

    var_index as_variable() const { return std::get<VarTag>(data).idx; }
    ConstantHandle as_constant() const { return std::get<ConstTag>(data).handle; }

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

    bool operator==(const Quantified& other) const {
        return op == other.op && var == other.var && body == other.body;
    }
};

class Formula {
    friend class FormulaBuilder;
    friend class QuantifierBuilder;
    friend class Sentence;

    std::variant<PredicateInstance, Compound, Quantified, SentenceHandle> data_;

    explicit Formula(PredicateInstance p) : data_(std::move(p)) {}
    explicit Formula(Compound c) : data_(std::move(c)) {}
    explicit Formula(Quantified q) : data_(std::move(q)) {}
    explicit Formula(SentenceHandle s) : data_(std::move(s)) {}

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

    std::string to_string() const;
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
using ConstantRegistry = util::KeyedRegistry<Constant, size_t, std::string, std::hash<std::string>>;

// A sentence is a closed formula (no free variables)
// All variables are bound by quantifiers
class Sentence {
    FormulaRegistry formulas_;
    FormulaHandle root_;

    // Helper to remap terms with new variable indices
    static Term remap_term(const Term& t, const std::unordered_map<var_index, var_index>& var_map);

    // Helper to recursively copy a formula and its subformulas
    FormulaHandle copy_formula_recursive(
        const FormulaRegistry& src,
        FormulaHandle src_handle,
        std::unordered_map<FormulaHandle, FormulaHandle>& handle_map,
        std::unordered_map<var_index, var_index>& var_map,
        var_index& next_var
    );

    // Rebind a single formula's internal handles to a new registry
    static void rebind_formula_handles(Formula& f, FormulaRegistry& new_reg);

public:
    Sentence() = default;

    // Copy constructor - copies registry and rebinds all internal handles
    Sentence(const Sentence& other);
    Sentence& operator=(const Sentence&) = delete;
    // Move constructor - must update root_'s registry pointer
    Sentence(Sentence&& other) noexcept;
    Sentence& operator=(Sentence&&) = default;

    // Construct sentence by copying formula tree from a FormulaRegistry
    // Remaps both formula handles and variable indices to start from 0
    Sentence(FormulaRegistry& src, FormulaHandle src_root);

    FormulaHandle root() const { return root_; }
    const Formula& get_formula(FormulaHandle h) const { return h.get(); }

    bool operator==(const Sentence& other) const {
        return to_string() == other.to_string();
    }

    std::string to_string() const;
};

struct SentenceHash {
    size_t operator()(const Sentence& s) const {
        return std::hash<std::string>{}(s.to_string());
    }
};

using SentenceRegistry = util::Registry<Sentence, SentenceHash>;

class GlobalContext {
    SentenceRegistry sentences_;
    PredicateRegistry predicates_;
    ConstantRegistry constants_;

public:
    SentenceHandle add_sentence(Sentence s) { return sentences_.register_item(std::move(s)); }
    PredicateHandle add_predicate(std::string name, size_t arity) {
        return predicates_.register_item(Predicate(std::move(name), arity));
    }
    ConstantHandle add_constant(std::string name) {
        return constants_.register_item(Constant(std::move(name)));
    }
    const Sentence& get_sentence(SentenceHandle h) const { return h.get(); }
    const Predicate& get_predicate(PredicateHandle h) const { return h.get(); }
    const Constant& get_constant(ConstantHandle h) const { return h.get(); }
};

// FormulaBuilder builds formulas and tracks variable scope.
// local_vars_ = current quantifier depth (next var to bind has this index)
// used_vars_ = minimum variable index used (for detecting free variables)
// A formula is a sentence if used_vars_ >= start depth (no free vars from parent scope)
class FormulaBuilder {
    GlobalContext& ctx_;
    FormulaRegistry formulas_;
    var_index local_vars_{0};  // Current depth / next var index
    var_index used_vars_{static_cast<var_index>(-1)};  // Min var used (init to max)

public:
    explicit FormulaBuilder(GlobalContext& ctx) : ctx_(ctx) {}

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

    // Formula creation
    FormulaHandle add_formula(Formula f) {
        return formulas_.register_item(std::move(f));
    }

    FormulaHandle predicate(PredicateHandle pred, std::vector<Term> args) {
        for (const auto& t : args) {
            if (t.is_variable()) use_var(t.as_variable());
        }
        return add_formula(Formula(PredicateInstance(pred, std::move(args))));
    }

    FormulaHandle make_and(FormulaHandle l, FormulaHandle r) { return add_formula(Formula(Compound{Op::And, l, r})); }
    FormulaHandle make_or(FormulaHandle l, FormulaHandle r) { return add_formula(Formula(Compound{Op::Or, l, r})); }
    FormulaHandle make_implies(FormulaHandle l, FormulaHandle r) { return add_formula(Formula(Compound{Op::Implies, l, r})); }
    FormulaHandle make_iff(FormulaHandle l, FormulaHandle r) { return add_formula(Formula(Compound{Op::Iff, l, r})); }
    FormulaHandle make_not(FormulaHandle f) { return add_formula(Formula(Compound{Op::Not, f, FormulaHandle{}})); }
    FormulaHandle make_bottom() { return add_formula(Formula(Compound{Op::Bottom, FormulaHandle{}, FormulaHandle{}})); }

    // Build sentence if formula is closed
    SentenceHandle build_sentence(FormulaHandle root, var_index start = 0) {
        if (!is_closed_since(start)) return SentenceHandle{};
        return ctx_.add_sentence(Sentence(formulas_, root));
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
            handle_ = builder_.add_formula(Formula(Quantified{op_, var_, body_}));
            if (SentenceHandle sentence = builder_.build_sentence(handle_, start_depth_); sentence.valid()) {
                handle_ = builder_.add_formula(Formula(sentence));
            }
        }
    }

    // Get bound variable as Term
    Term var() const { return Term::var(var_); }
    var_index var_idx() const { return var_; }

    // Set the body formula
    void set_body(FormulaHandle body) { body_ = body; }

    QuantifierBuilder(const QuantifierBuilder&) = delete;
    QuantifierBuilder& operator=(const QuantifierBuilder&) = delete;
};

} // end logic
