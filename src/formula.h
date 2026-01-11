#pragma once

#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace logic {

struct Formula;
using FormulaPtr = std::shared_ptr<const Formula>;

enum class Op {
    And,
    Or,
    Implies,
    Not,
    Iff,
    Bottom  // ⊥ (falsum)
};

struct Atom {
    std::string name;
    bool operator==(const Atom&) const = default;
};

struct Compound {
    Op op;
    std::vector<FormulaPtr> args;

    bool operator==(const Compound& other) const;
};

struct Formula {
    std::variant<Atom, Compound> data;
    bool operator==(const Formula& other) const;
};

// Factory functions
inline FormulaPtr atom(std::string name) {
    return std::make_shared<Formula>(Formula{Atom{std::move(name)}});
}

inline FormulaPtr bottom() {
    return std::make_shared<Formula>(Formula{Compound{Op::Bottom, {}}});
}

inline FormulaPtr neg(FormulaPtr a) {
    return std::make_shared<Formula>(Formula{Compound{Op::Not, {std::move(a)}}});
}

inline FormulaPtr conj(FormulaPtr a, FormulaPtr b) {
    return std::make_shared<Formula>(Formula{Compound{Op::And, {std::move(a), std::move(b)}}});
}

inline FormulaPtr disj(FormulaPtr a, FormulaPtr b) {
    return std::make_shared<Formula>(Formula{Compound{Op::Or, {std::move(a), std::move(b)}}});
}

inline FormulaPtr impl(FormulaPtr a, FormulaPtr b) {
    return std::make_shared<Formula>(Formula{Compound{Op::Implies, {std::move(a), std::move(b)}}});
}

inline FormulaPtr iff(FormulaPtr a, FormulaPtr b) {
    return std::make_shared<Formula>(Formula{Compound{Op::Iff, {std::move(a), std::move(b)}}});
}

// Accessors
inline bool is_atom(const Formula& f) {
    return std::holds_alternative<Atom>(f.data);
}

inline bool is_compound(const Formula& f) {
    return std::holds_alternative<Compound>(f.data);
}

inline const Atom& as_atom(const Formula& f) {
    return std::get<Atom>(f.data);
}

inline const Compound& as_compound(const Formula& f) {
    return std::get<Compound>(f.data);
}

inline bool has_op(const Formula& f, Op op) {
    return is_compound(f) && as_compound(f).op == op;
}

// String conversion
std::string to_string(const Formula& f);
std::string op_symbol(Op op);

}  // namespace logic
