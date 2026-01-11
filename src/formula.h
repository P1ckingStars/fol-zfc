#pragma once

#include "util/registry.h"

#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace logic {

class ProofDatabase;
class Formula;

using formula_id = size_t;
using predicate_id = size_t;
using constant_id = size_t;
using sentence_id = size_t;
using var_index = size_t;  // Local variable index within a sentence

constexpr formula_id INVALID_FORMULA = 0;
constexpr predicate_id INVALID_PREDICATE = 0;
constexpr constant_id INVALID_CONSTANT = 0;
constexpr sentence_id INVALID_SENTENCE = 0;

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

// A term is either a local variable (by index) or a global constant (by id)
struct Term {
    std::variant<var_index, constant_id> data;

    static Term var(var_index idx) { return Term{idx}; }
    static Term constant(constant_id id) { return Term{id}; }

    bool is_variable() const { return std::holds_alternative<var_index>(data); }
    bool is_constant() const { return std::holds_alternative<constant_id>(data); }

    var_index as_variable() const { return std::get<var_index>(data); }
    constant_id as_constant() const { return std::get<constant_id>(data); }

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
    predicate_id predicate_;
    std::vector<Term> args_;

public:
    PredicateInstance(predicate_id pred, std::vector<Term> args)
        : predicate_(pred), args_(std::move(args)) {}

    predicate_id predicate() const { return predicate_; }
    const std::vector<Term>& args() const { return args_; }

    bool operator==(const PredicateInstance& other) const {
        return predicate_ == other.predicate_ && args_ == other.args_;
    }
};

// Propositional connective: op(left, right) or op(left) for unary
struct Compound {
    Op op;
    formula_id left{INVALID_FORMULA};
    formula_id right{INVALID_FORMULA};

    bool operator==(const Compound& other) const {
        return op == other.op && left == other.left && right == other.right;
    }
};

// Quantified formula: ∀x.φ or ∃x.φ
// Binds variable at index `var` in the body formula
struct Quantified {
    Op op;  // Forall or Exists
    var_index var;  // The variable index being bound
    formula_id body;

    bool operator==(const Quantified& other) const {
        return op == other.op && var == other.var && body == other.body;
    }
};

class Formula {
friend ProofDatabase;
    std::variant<PredicateInstance, Compound, Quantified> data_;

    explicit Formula(PredicateInstance p) : data_(std::move(p)) {}
    explicit Formula(Compound c) : data_(std::move(c)) {}
    explicit Formula(Quantified q) : data_(std::move(q)) {}

public:
    bool operator==(const Formula& other) const {
        return data_ == other.data_;
    }

    bool is_predicate() const { return std::holds_alternative<PredicateInstance>(data_); }
    bool is_compound() const { return std::holds_alternative<Compound>(data_); }
    bool is_quantified() const { return std::holds_alternative<Quantified>(data_); }

    const PredicateInstance& as_predicate() const { return std::get<PredicateInstance>(data_); }
    const Compound& as_compound() const { return std::get<Compound>(data_); }
    const Quantified& as_quantified() const { return std::get<Quantified>(data_); }

    std::string to_string() const;
};

struct FormulaHash {
    size_t operator()(const Formula& f) const {
        return std::hash<std::string>{}(f.to_string());
    }
};

// A sentence is a closed formula (no free variables)
// All variables are bound by quantifiers
class Sentence {
    formula_id root_;
    size_t num_bound_vars_;  // Total number of bound variables

public:
    Sentence(formula_id root, size_t num_vars)
        : root_(root), num_bound_vars_(num_vars) {}

    formula_id root() const { return root_; }
    size_t num_bound_vars() const { return num_bound_vars_; }

    bool operator==(const Sentence& other) const {
        return root_ == other.root_ && num_bound_vars_ == other.num_bound_vars_;
    }

    std::string to_string() const;
};

struct SentenceHash {
    size_t operator()(const Sentence& s) const {
        return std::hash<formula_id>{}(s.root()) ^
               (std::hash<size_t>{}(s.num_bound_vars()) << 1);
    }
};

using FormulaRegistry = util::Registry<Formula, formula_id, FormulaHash>;
using PredicateRegistry = util::KeyedRegistry<Predicate, predicate_id, std::string, std::hash<std::string>>;
using ConstantRegistry = util::KeyedRegistry<Constant, constant_id, std::string, std::hash<std::string>>;
using SentenceRegistry = util::Registry<Sentence, sentence_id, SentenceHash>;

class ProofDatabase {
public:
    // ==================== Propositional Formula Creation ====================
    formula_id create_and(formula_id left, formula_id right) {
        return formulas_.register_item(Formula(Compound{Op::And, left, right}));
    }

    formula_id create_or(formula_id left, formula_id right) {
        return formulas_.register_item(Formula(Compound{Op::Or, left, right}));
    }

    formula_id create_implies(formula_id left, formula_id right) {
        return formulas_.register_item(Formula(Compound{Op::Implies, left, right}));
    }

    formula_id create_iff(formula_id left, formula_id right) {
        return formulas_.register_item(Formula(Compound{Op::Iff, left, right}));
    }

    formula_id create_not(formula_id operand) {
        return formulas_.register_item(Formula(Compound{Op::Not, operand, INVALID_FORMULA}));
    }

    formula_id create_bottom() {
        return formulas_.register_item(Formula(Compound{Op::Bottom, INVALID_FORMULA, INVALID_FORMULA}));
    }

    // ==================== First-Order Formula Creation ====================
    formula_id create_predicate_instance(predicate_id pred, std::vector<Term> args) {
        return formulas_.register_item(Formula(PredicateInstance(pred, std::move(args))));
    }

    formula_id create_forall(var_index var, formula_id body) {
        return formulas_.register_item(Formula(Quantified{Op::Forall, var, body}));
    }

    formula_id create_exists(var_index var, formula_id body) {
        return formulas_.register_item(Formula(Quantified{Op::Exists, var, body}));
    }

    // ==================== Predicate Creation ====================
    predicate_id create_predicate(const std::string& name, size_t num_args) {
        return predicates_.register_item(Predicate(name, num_args));
    }

    std::variant<predicate_id, const Predicate*> try_create_predicate(const std::string& name, size_t num_args) {
        return predicates_.try_register(Predicate(name, num_args));
    }

    // ==================== Constant Creation ====================
    constant_id create_constant(const std::string& name) {
        return constants_.register_item(Constant(name));
    }

    std::variant<constant_id, const Constant*> try_create_constant(const std::string& name) {
        return constants_.try_register(Constant(name));
    }

    // ==================== Sentence Creation ====================
    // Creates a sentence from a closed formula
    // num_vars is the number of bound variables in the formula
    sentence_id create_sentence(formula_id root, size_t num_vars) {
        return sentences_.register_item(Sentence(root, num_vars));
    }

    // ==================== Getters ====================
    const Formula& get_formula(formula_id id) const {
        return formulas_.get(id);
    }

    const Predicate& get_predicate(predicate_id id) const {
        return predicates_.get(id);
    }

    const Constant& get_constant(constant_id id) const {
        return constants_.get(id);
    }

    const Sentence& get_sentence(sentence_id id) const {
        return sentences_.get(id);
    }

    // ==================== Lookup ====================
    std::optional<formula_id> find_formula(const Formula& f) const {
        return formulas_.find(f);
    }

    std::optional<predicate_id> find_predicate(const std::string& name) const {
        return predicates_.find_by_key(name);
    }

    std::optional<constant_id> find_constant(const std::string& name) const {
        return constants_.find_by_key(name);
    }

private:
    FormulaRegistry formulas_;
    PredicateRegistry predicates_;
    ConstantRegistry constants_;
    SentenceRegistry sentences_;
};


// String conversion
std::string to_string(const Formula& f);
std::string to_string(const Sentence& s);
std::string op_symbol(Op op);

}  // namespace logic
