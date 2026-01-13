#include "formula.h"

namespace logic {

// ==================== String Conversion ====================

std::string op_symbol(Op op) {
    switch (op) {
        case Op::And: return "&";
        case Op::Or: return "|";
        case Op::Implies: return "->";
        case Op::Not: return "~";
        case Op::Iff: return "<->";
        case Op::Bottom: return "_|_";
        case Op::Forall: return "forall";
        case Op::Exists: return "exists";
    }
    return "?";
}

std::string var_name(var_index idx) {
    if (idx < 26) {
        return std::string(1, 'x' + static_cast<char>(idx));
    }
    return "v" + std::to_string(idx);
}

std::string term_to_string(const Term& t, const ProofDatabase* db) {
    if (t.is_variable()) {
        return var_name(t.as_variable());
    } else if (db) {
        return db->get_constant(t.as_constant()).get_name();
    } else {
        return "c" + std::to_string(t.as_constant());
    }
}

std::string formula_to_string_impl(const Formula& f, const ProofDatabase* db) {
    if (f.is_predicate()) {
        const auto& pred = f.as_predicate();
        std::string result;
        if (db) {
            result = db->get_predicate(pred.predicate()).get_name();
        } else {
            result = "P" + std::to_string(pred.predicate());
        }
        if (!pred.args().empty()) {
            result += "(";
            for (size_t i = 0; i < pred.args().size(); ++i) {
                if (i > 0) result += ", ";
                result += term_to_string(pred.args()[i], db);
            }
            result += ")";
        }
        return result;
    } else if (f.is_compound()) {
        const auto& c = f.as_compound();
        switch (c.op) {
            case Op::Bottom:
                return "_|_";
            case Op::Not:
                if (db) {
                    return "~" + formula_to_string_impl(db->get_formula(c.left), db);
                }
                return "~(...)";
            default: {
                std::string left_str = db ? formula_to_string_impl(db->get_formula(c.left), db) : "(...)";
                std::string right_str = db ? formula_to_string_impl(db->get_formula(c.right), db) : "(...)";
                return "(" + left_str + " " + op_symbol(c.op) + " " + right_str + ")";
            }
        }
    } else if (f.is_quantified()) {
        const auto& q = f.as_quantified();
        std::string body_str = db ? formula_to_string_impl(db->get_formula(q.body), db) : "(...)";
        return op_symbol(q.op) + " " + var_name(q.var) + ". " + body_str;
    }
    return "?";
}

std::string Formula::to_string() const {
    return formula_to_string_impl(*this, nullptr);
}

std::string Sentence::to_string() const {
    return "sentence(" + std::to_string(root_) + ")";
}

// ==================== Free Variable Analysis ====================

void collect_free_vars(const Formula& f, const ProofDatabase& db,
                       std::set<var_index>& free_vars,
                       std::set<var_index>& bound_vars) {
    if (f.is_predicate()) {
        const auto& pred = f.as_predicate();
        for (const auto& term : pred.args()) {
            if (term.is_variable()) {
                var_index v = term.as_variable();
                if (bound_vars.find(v) == bound_vars.end()) {
                    free_vars.insert(v);
                }
            }
        }
    } else if (f.is_compound()) {
        const auto& c = f.as_compound();
        if (c.op == Op::Bottom) {
            return;
        } else if (c.op == Op::Not) {
            collect_free_vars(db.get_formula(c.left), db, free_vars, bound_vars);
        } else {
            collect_free_vars(db.get_formula(c.left), db, free_vars, bound_vars);
            collect_free_vars(db.get_formula(c.right), db, free_vars, bound_vars);
        }
    } else if (f.is_quantified()) {
        const auto& q = f.as_quantified();
        bound_vars.insert(q.var);
        collect_free_vars(db.get_formula(q.body), db, free_vars, bound_vars);
        bound_vars.erase(q.var);
    }
}

std::set<var_index> get_free_vars(formula_id fid, const ProofDatabase& db) {
    std::set<var_index> free_vars;
    std::set<var_index> bound_vars;
    collect_free_vars(db.get_formula(fid), db, free_vars, bound_vars);
    return free_vars;
}

bool is_sentence(formula_id fid, const ProofDatabase& db) {
    return get_free_vars(fid, db).empty();
}

size_t count_bound_vars(formula_id fid, const ProofDatabase& db) {
    std::set<var_index> seen;
    std::function<void(const Formula&)> collect = [&](const Formula& f) {
        if (f.is_predicate()) {
            return;
        } else if (f.is_compound()) {
            const auto& c = f.as_compound();
            if (c.op == Op::Bottom) return;
            if (c.op == Op::Not) {
                collect(db.get_formula(c.left));
            } else {
                collect(db.get_formula(c.left));
                collect(db.get_formula(c.right));
            }
        } else if (f.is_quantified()) {
            const auto& q = f.as_quantified();
            seen.insert(q.var);
            collect(db.get_formula(q.body));
        }
    };
    collect(db.get_formula(fid));
    return seen.size();
}

}  // namespace logic
