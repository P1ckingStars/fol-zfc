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

constexpr formula_id INVALID_FORMULA = 0;
constexpr predicate_id INVALID_PREDICATE = 0;

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

class Variable {
    size_t id;
public:
    bool operator==(const Variable& other) const {
        return id == other.id;
    }
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

class PredicateInstance {
    predicate_id predicate_;
    std::vector<Variable> variables_;
public:
    bool operator==(const PredicateInstance& other) const {
        return predicate_ == other.predicate_ && variables_ == other.variables_;
    }
};

struct Compound {
    Op op;
    formula_id left{INVALID_FORMULA};
    formula_id right{INVALID_FORMULA};

    bool operator==(const Compound& other) const {
        return op == other.op && left == other.left && right == other.right;
    }
};

class Formula {
friend ProofDatabase;
    std::variant<PredicateInstance, Compound> data_;
    explicit Formula(PredicateInstance p) : data_(std::move(p)) {}
    explicit Formula(Compound c) : data_(std::move(c)) {}
public:

    bool operator==(const Formula& other) const {
        return data_ == other.data_;
    }

    std::string to_string() const;
};

struct FormulaHash {
    size_t operator()(const Formula& f) const {
        return std::hash<std::string>{}(f.to_string());
    }
};

using FormulaRegistry = util::Registry<Formula, formula_id, FormulaHash>;
using PredicateRegistry = util::KeyedRegistry<Predicate, predicate_id, std::string, std::hash<std::string>>;

class ProofDatabase {
public:
    // Formula creation
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

    // Predicate creation - throws PredicateRegistry::ConflictError on conflict
    predicate_id create_predicate(const std::string& name, size_t num_args) {
        return predicates_.register_item(Predicate(name, num_args));
    }

    // Non-throwing version
    std::variant<predicate_id, const Predicate*> try_create_predicate(const std::string& name, size_t num_args) {
        return predicates_.try_register(Predicate(name, num_args));
    }

    // Getters
    const Formula& get_formula(formula_id id) const {
        return formulas_.get(id);
    }

    const Predicate& get_predicate(predicate_id id) const {
        return predicates_.get(id);
    }

    std::optional<formula_id> find_formula(const Formula& f) const {
        return formulas_.find(f);
    }

    std::optional<predicate_id> find_predicate(const std::string& name) const {
        return predicates_.find_by_key(name);
    }

private:
    FormulaRegistry formulas_;
    PredicateRegistry predicates_;
};


// String conversion
std::string to_string(const Formula& f);
std::string op_symbol(Op op);

}  // namespace logic
