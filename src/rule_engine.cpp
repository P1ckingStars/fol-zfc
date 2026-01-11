#include "rule_engine.h"

#include <algorithm>
#include <sstream>

namespace logic {

std::string rule_name(Rule r) {
    switch (r) {
        case Rule::Assumption: return "Assumption";
        case Rule::Premise: return "Premise";
        case Rule::AndIntro: return "∧I";
        case Rule::OrIntroLeft: return "∨IL";
        case Rule::OrIntroRight: return "∨IR";
        case Rule::ImpliesIntro: return "→I";
        case Rule::NotIntro: return "¬I";
        case Rule::IffIntro: return "↔I";
        case Rule::BottomIntro: return "⊥I";
        case Rule::AndElimLeft: return "∧EL";
        case Rule::AndElimRight: return "∧ER";
        case Rule::OrElim: return "∨E";
        case Rule::ImpliesElim: return "→E";
        case Rule::NotElim: return "¬E";
        case Rule::IffElimLeft: return "↔EL";
        case Rule::IffElimRight: return "↔ER";
        case Rule::BottomElim: return "⊥E";
    }
    return "?";
}

// Context implementation

void Context::push_scope() {
    scope_level_++;
}

void Context::pop_scope() {
    // Remove all entries at current scope level
    entries_.erase(
        std::remove_if(entries_.begin(), entries_.end(),
            [this](const Entry& e) { return e.scope_level >= scope_level_; }),
        entries_.end());
    scope_level_--;
}

void Context::add_premise(FormulaPtr f) {
    auto proof = make_premise(f);
    entries_.push_back({f, proof, 0, false});
}

ProofPtr Context::add_assumption(FormulaPtr f) {
    auto proof = make_assumption(f);
    entries_.push_back({f, proof, scope_level_, true});
    return proof;
}

void Context::add_derived(FormulaPtr f, ProofPtr proof) {
    entries_.push_back({f, proof, scope_level_, false});
}

std::optional<ProofPtr> Context::find(const Formula& f) const {
    // Search from most recent
    for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
        if (*it->formula == f) {
            return it->proof;
        }
    }
    return std::nullopt;
}

bool Context::contains(const Formula& f) const {
    return find(f).has_value();
}

std::vector<Context::Entry> Context::current_scope_entries() const {
    std::vector<Entry> result;
    for (const auto& e : entries_) {
        if (e.scope_level == scope_level_) {
            result.push_back(e);
        }
    }
    return result;
}

// Proof factories

ProofPtr make_premise(FormulaPtr f) {
    return std::make_shared<Proof>(Proof{f, Rule::Premise, {}, std::nullopt});
}

ProofPtr make_assumption(FormulaPtr f) {
    return std::make_shared<Proof>(Proof{f, Rule::Assumption, {}, std::nullopt});
}

// RuleEngine helpers

RuleResult RuleEngine::make_error(const std::string& msg) {
    return RuleResult{false, msg, nullptr};
}

RuleResult RuleEngine::make_success(FormulaPtr conclusion, Rule rule,
                                    std::vector<ProofPtr> premises,
                                    std::optional<FormulaPtr> discharged) {
    auto proof = std::make_shared<Proof>(Proof{conclusion, rule, std::move(premises), discharged});
    return RuleResult{true, "", proof};
}

// Introduction rules

RuleResult RuleEngine::and_intro(ProofPtr left, ProofPtr right) {
    auto conclusion = conj(left->conclusion, right->conclusion);
    return make_success(conclusion, Rule::AndIntro, {left, right});
}

RuleResult RuleEngine::or_intro_left(ProofPtr a, FormulaPtr b) {
    auto conclusion = disj(a->conclusion, b);
    return make_success(conclusion, Rule::OrIntroLeft, {a});
}

RuleResult RuleEngine::or_intro_right(FormulaPtr a, ProofPtr b) {
    auto conclusion = disj(a, b->conclusion);
    return make_success(conclusion, Rule::OrIntroRight, {b});
}

RuleResult RuleEngine::implies_intro(FormulaPtr assumption, ProofPtr consequent) {
    auto conclusion = impl(assumption, consequent->conclusion);
    return make_success(conclusion, Rule::ImpliesIntro, {consequent}, assumption);
}

RuleResult RuleEngine::not_intro(FormulaPtr assumption, ProofPtr bottom_proof) {
    if (!has_op(*bottom_proof->conclusion, Op::Bottom)) {
        return make_error("not_intro requires ⊥ as conclusion of subproof");
    }
    auto conclusion = neg(assumption);
    return make_success(conclusion, Rule::NotIntro, {bottom_proof}, assumption);
}

RuleResult RuleEngine::iff_intro(ProofPtr left_impl, ProofPtr right_impl) {
    // Check left is A → B
    if (!has_op(*left_impl->conclusion, Op::Implies)) {
        return make_error("iff_intro: left premise must be an implication");
    }
    // Check right is B → A
    if (!has_op(*right_impl->conclusion, Op::Implies)) {
        return make_error("iff_intro: right premise must be an implication");
    }

    const auto& left_comp = as_compound(*left_impl->conclusion);
    const auto& right_comp = as_compound(*right_impl->conclusion);

    // Verify A → B and B → A
    if (*left_comp.args[0] != *right_comp.args[1] ||
        *left_comp.args[1] != *right_comp.args[0]) {
        return make_error("iff_intro: implications must be converses");
    }

    auto conclusion = iff(left_comp.args[0], left_comp.args[1]);
    return make_success(conclusion, Rule::IffIntro, {left_impl, right_impl});
}

RuleResult RuleEngine::bottom_intro(ProofPtr a, ProofPtr not_a) {
    // Check not_a is ¬A where A matches a->conclusion
    if (!has_op(*not_a->conclusion, Op::Not)) {
        return make_error("bottom_intro: second argument must be a negation");
    }

    const auto& neg_inner = as_compound(*not_a->conclusion).args[0];
    if (*neg_inner != *a->conclusion) {
        return make_error("bottom_intro: ¬A must negate A");
    }

    return make_success(bottom(), Rule::BottomIntro, {a, not_a});
}

// Elimination rules

RuleResult RuleEngine::and_elim_left(ProofPtr conj_proof) {
    if (!has_op(*conj_proof->conclusion, Op::And)) {
        return make_error("and_elim_left: argument must be a conjunction");
    }

    auto left = as_compound(*conj_proof->conclusion).args[0];
    return make_success(left, Rule::AndElimLeft, {conj_proof});
}

RuleResult RuleEngine::and_elim_right(ProofPtr conj_proof) {
    if (!has_op(*conj_proof->conclusion, Op::And)) {
        return make_error("and_elim_right: argument must be a conjunction");
    }

    auto right = as_compound(*conj_proof->conclusion).args[1];
    return make_success(right, Rule::AndElimRight, {conj_proof});
}

RuleResult RuleEngine::or_elim(ProofPtr disj_proof, ProofPtr case_left, ProofPtr case_right) {
    if (!has_op(*disj_proof->conclusion, Op::Or)) {
        return make_error("or_elim: first argument must be a disjunction");
    }

    // Both cases must conclude the same thing
    if (*case_left->conclusion != *case_right->conclusion) {
        return make_error("or_elim: both cases must have the same conclusion");
    }

    return make_success(case_left->conclusion, Rule::OrElim,
                       {disj_proof, case_left, case_right});
}

RuleResult RuleEngine::implies_elim(ProofPtr antecedent, ProofPtr implication) {
    if (!has_op(*implication->conclusion, Op::Implies)) {
        return make_error("implies_elim: second argument must be an implication");
    }

    const auto& impl_comp = as_compound(*implication->conclusion);
    if (*impl_comp.args[0] != *antecedent->conclusion) {
        return make_error("implies_elim: antecedent does not match implication");
    }

    return make_success(impl_comp.args[1], Rule::ImpliesElim, {antecedent, implication});
}

RuleResult RuleEngine::not_elim(ProofPtr double_neg) {
    if (!has_op(*double_neg->conclusion, Op::Not)) {
        return make_error("not_elim: argument must be a negation");
    }

    const auto& inner = as_compound(*double_neg->conclusion).args[0];
    if (!has_op(*inner, Op::Not)) {
        return make_error("not_elim: argument must be a double negation");
    }

    auto result = as_compound(*inner).args[0];
    return make_success(result, Rule::NotElim, {double_neg});
}

RuleResult RuleEngine::iff_elim_left(ProofPtr biconditional) {
    if (!has_op(*biconditional->conclusion, Op::Iff)) {
        return make_error("iff_elim_left: argument must be a biconditional");
    }

    const auto& comp = as_compound(*biconditional->conclusion);
    auto result = impl(comp.args[0], comp.args[1]);
    return make_success(result, Rule::IffElimLeft, {biconditional});
}

RuleResult RuleEngine::iff_elim_right(ProofPtr biconditional) {
    if (!has_op(*biconditional->conclusion, Op::Iff)) {
        return make_error("iff_elim_right: argument must be a biconditional");
    }

    const auto& comp = as_compound(*biconditional->conclusion);
    auto result = impl(comp.args[1], comp.args[0]);
    return make_success(result, Rule::IffElimRight, {biconditional});
}

RuleResult RuleEngine::bottom_elim(ProofPtr bottom_proof, FormulaPtr conclusion) {
    if (!has_op(*bottom_proof->conclusion, Op::Bottom)) {
        return make_error("bottom_elim: argument must be ⊥");
    }

    return make_success(conclusion, Rule::BottomElim, {bottom_proof});
}

// Proof printing

std::string print_proof(const Proof& p, int indent) {
    std::ostringstream ss;
    std::string pad(indent * 2, ' ');

    // Print sub-proofs first
    for (const auto& premise : p.premises) {
        ss << print_proof(*premise, indent + 1);
    }

    // Print this node
    ss << pad << to_string(*p.conclusion) << "  [" << rule_name(p.rule);
    if (p.discharged) {
        ss << ", discharging " << to_string(**p.discharged);
    }
    ss << "]\n";

    return ss.str();
}

}  // namespace logic
