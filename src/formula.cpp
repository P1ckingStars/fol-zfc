#include "formula.h"

namespace logic {

std::string op_symbol(Op op) {
    switch (op) {
        case Op::And: return "∧";
        case Op::Or: return "∨";
        case Op::Implies: return "→";
        case Op::Not: return "¬";
        case Op::Iff: return "↔";
        case Op::Bottom: return "⊥";
    }
    return "?";
}

std::string to_string(const Formula& f) {
    if (is_atom(f)) {
        return as_atom(f).name;
    }

    const auto& comp = as_compound(f);

    if (comp.op == Op::Bottom) {
        return "⊥";
    }

    if (comp.op == Op::Not) {
        const auto& inner = *comp.args[0];
        if (is_atom(inner)) {
            return "¬" + to_string(inner);
        }
        return "¬(" + to_string(inner) + ")";
    }

    // Binary operators
    auto left = to_string(*comp.args[0]);
    auto right = to_string(*comp.args[1]);

    // Add parens if nested compound
    if (is_compound(*comp.args[0])) {
        left = "(" + left + ")";
    }
    if (is_compound(*comp.args[1])) {
        right = "(" + right + ")";
    }

    return left + " " + op_symbol(comp.op) + " " + right;
}

bool Formula::operator==(const Formula& other) const {
    if (data.index() != other.data.index()) return false;

    if (is_atom(*this)) {
        return as_atom(*this) == as_atom(other);
    }

    return as_compound(*this) == as_compound(other);
}

bool Compound::operator==(const Compound& other) const {
    if (op != other.op) return false;
    if (args.size() != other.args.size()) return false;
    for (size_t i = 0; i < args.size(); i++) {
        if (!(*args[i] == *other.args[i])) return false;
    }
    return true;
}

}  // namespace logic
