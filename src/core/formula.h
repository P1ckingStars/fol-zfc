#pragma once

#include "../util/registry.h"

#include <functional>
#include <memory>
#include <set>
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

// Tagged wrappers to distinguish var_index and constant_id in variant
struct VarTag { var_index idx; bool operator==(const VarTag&) const = default; };
struct ConstTag { constant_id id; bool operator==(const ConstTag&) const = default; };

// A term is either a local variable (by index) or a global constant (by id)
struct Term {
    std::variant<VarTag, ConstTag> data;

    static Term var(var_index idx) { return Term{VarTag{idx}}; }
    static Term constant(constant_id id) { return Term{ConstTag{id}}; }

    bool is_variable() const { return std::holds_alternative<VarTag>(data); }
    bool is_constant() const { return std::holds_alternative<ConstTag>(data); }

    var_index as_variable() const { return std::get<VarTag>(data).idx; }
    constant_id as_constant() const { return std::get<ConstTag>(data).id; }

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

using FormulaRegistry = util::Registry<Formula, formula_id, FormulaHash>;
using PredicateRegistry = util::KeyedRegistry<Predicate, predicate_id, std::string, std::hash<std::string>>;
using ConstantRegistry = util::KeyedRegistry<Constant, constant_id, std::string, std::hash<std::string>>;
// A sentence is a closed formula (no free variables)
// All variables are bound by quantifiers
class Sentence {
    FormulaRegistry formulas_;
public:
    bool operator==(const Sentence& other) const {
        return formulas_ == other.formulas_;
    }

    std::string to_string() const;
};

struct SentenceHash {
    size_t operator()(const Sentence& s) const {
        return std::hash<std::string>{}(s.to_string());
    }
};

using SentenceRegistry = util::Registry<Sentence, sentence_id, SentenceHash>;

class GlobalContext {
    SentenceRegistry sentences_;
    SentenceRegistry predicates_;
    SentenceRegistry constants_;
};

} // end core
