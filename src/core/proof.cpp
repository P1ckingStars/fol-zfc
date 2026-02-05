#include "proof.h"
#include "src/core/formula.h"
#include "src/util/error.h"

namespace logic {

// ========== AssumptionScope ==========

AssumptionScope::AssumptionScope(FormulaHandle const & formula)
    : assumption_(formula) {
    derived_.insert(formula);
}

FormulaHandle const& AssumptionScope::get_formula() {
    return assumption_;
}

bool AssumptionScope::contains(FormulaHandle const & f) {
    return derived_.contains(f);
}

void AssumptionScope::derive(FormulaHandle const & handle) {
    derived_.insert(handle);
}

// ========== FixVarScope ==========

FixVarScope::FixVarScope(FormulaBuilder& builder, Op op)
    : result_(std::make_unique<FormulaHandle>()),
      qbuilder_(std::make_unique<QuantifierBuilder>(builder, op, *result_)) {}

Term FixVarScope::var_term() const {
    return qbuilder_->var();
}

bool FixVarScope::contains(FormulaHandle const & f) const {
    return derived_.contains(f);
}

void FixVarScope::derive(FormulaHandle const & handle) {
    derived_.insert(handle);
}

FormulaHandle FixVarScope::finalize(FormulaHandle body) {
    qbuilder_->set_body(body);
    qbuilder_.reset();  // Destroy QuantifierBuilder, triggers formula creation
    return *result_;
}

// ========== ProofStack ==========

ProofStack::ProofStack(GlobalContext & context)
    : formula_builder_(context) {}

Term ProofStack::fix_var() {
    scopes.push_back(FixVarScope(formula_builder_));
    return std::get<FixVarScope>(scopes.back()).var_term();
}

FormulaHandle ProofStack::assume(FormulaHandle const & formula) {
    scopes.push_back(AssumptionScope(formula));
    return std::get<AssumptionScope>(*scopes.rbegin()).get_formula();
}

bool ProofStack::is_derived(FormulaHandle const &a) {
    if (derived_.contains(a)) {
        return true;
    }
    for (auto it = scopes.rbegin(); it != scopes.rend(); it++) {
        bool found = std::visit([&](auto&& arg) {
            return arg.contains(a);
        }, *it);
        if (found) {
            return true;
        }
    }
    return false;
}

void ProofStack::pop() {
    scopes.pop_back();
}

void ProofStack::derive_in_current_scope(FormulaHandle const& formula) {
    if (scopes.empty()) {
        derived_.insert(formula);
    } else {
        std::visit([&](auto&& scope) {
            scope.derive(formula);
        }, *scopes.rbegin());
    }
}

void ProofStack::derive_in_scope(FormulaHandle const& formula, int scope_idx) {
    if (scope_idx < 0 || static_cast<size_t>(scope_idx) >= scopes.size()) {
        // Derive at base level
        derived_.insert(formula);
    } else {
        std::visit([&](auto&& scope) {
            scope.derive(formula);
        }, scopes[scope_idx]);
    }
}

int ProofStack::find_scope_for_term(Term const& t) const {
    // Generalized vars shouldn't appear in proof terms
    if (t.is_generalized()) {
        return -1;
    }
    // Fixed vars: find the FixVarScope that introduced it
    var_index var_idx = t.as_variable();
    for (size_t i = 0; i < scopes.size(); ++i) {
        if (std::holds_alternative<FixVarScope>(scopes[i])) {
            const auto& fix_scope = std::get<FixVarScope>(scopes[i]);
            if (fix_scope.var_term().as_variable() == var_idx) {
                return static_cast<int>(i);
            }
        }
    }
    return -1;
}

bool ProofStack::is_term_accessible(Term const& t) const {
    // Generalized vars shouldn't appear in proof terms
    if (t.is_generalized()) {
        return false;
    }
    // Fixed vars must be introduced by an enclosing FixVarScope
    return find_scope_for_term(t) >= 0;
}

bool ProofStack::formula_contains_fixed_var(FormulaHandle const& h, var_index var_idx) const {
    const Formula& f = h.get();

    if (f.is_predicate()) {
        const PredicateInstance& p = f.as_predicate();
        for (const Term& t : p.args()) {
            if (t.is_fixed() && t.as_variable() == var_idx) {
                return true;
            }
        }
        return false;
    }
    else if (f.is_compound()) {
        const Compound& c = f.as_compound();
        if (c.left.valid() && formula_contains_fixed_var(c.left, var_idx)) {
            return true;
        }
        if (c.right.valid() && formula_contains_fixed_var(c.right, var_idx)) {
            return true;
        }
        return false;
    }
    else if (f.is_quantified()) {
        const Quantified& q = f.as_quantified();
        return formula_contains_fixed_var(q.body, var_idx);
    }
    else if (f.is_sentence()) {
        // Sentences are closed, they don't contain free fixed vars
        return false;
    }
    return false;
}

FormulaHandle ProofStack::make_exists_from_witness(FormulaHandle const& body, Term const& witness) {
    FormulaHandle result;
    {
        // Create QuantifierBuilder for Exists - it will handle generalization
        QuantifierBuilder qb(formula_builder_, Op::Exists, result);
        // Translate witness to the quantifier's fixed var in the body
        FormulaHandle translated_body = formula_builder_.translate_term(body, witness, qb.var());
        qb.set_body(translated_body);
    }  // Destructor generalizes and creates ∃x.body
    return result;
}

FormulaResult ProofStack::use_theorem(SentenceHandle & sentence) {
    auto res = formula_builder_.add_sentence(sentence);
    derived_.insert(res);
    return res;
}

// ========== Conjunction (And) ==========

FormulaResult ProofStack::and_intro(FormulaHandle const &a, FormulaHandle const &b) {
    if (!is_derived(a)) {
        return MAKE_ERROR << "and_intro: left formula not derived: " << a.get();
    }
    if (!is_derived(b)) {
        return MAKE_ERROR << "and_intro: right formula not derived: " << b.get();
    }
    auto formula = formula_builder_.make_and(a, b);
    derive_in_current_scope(formula);
    return formula;
}

FormulaResult ProofStack::and_elim_l(FormulaHandle const &and_formula) {
    if (!is_derived(and_formula)) {
        return MAKE_ERROR << "and_elim_l: formula not derived: " << and_formula.get();
    }
    const Formula& f = and_formula.get();
    if (!f.is_compound()) {
        return MAKE_ERROR << "and_elim_l: expected compound formula, got: " << f;
    }
    const Compound& c = f.as_compound();
    if (c.op != Op::And) {
        return MAKE_ERROR << "and_elim_l: expected And, got: " << f;
    }
    derive_in_current_scope(c.left);
    return c.left;
}

FormulaResult ProofStack::and_elim_r(FormulaHandle const &and_formula) {
    if (!is_derived(and_formula)) {
        return MAKE_ERROR << "and_elim_r: formula not derived: " << and_formula.get();
    }
    const Formula& f = and_formula.get();
    if (!f.is_compound()) {
        return MAKE_ERROR << "and_elim_r: expected compound formula, got: " << f;
    }
    const Compound& c = f.as_compound();
    if (c.op != Op::And) {
        return MAKE_ERROR << "and_elim_r: expected And, got: " << f;
    }
    derive_in_current_scope(c.right);
    return c.right;
}

// ========== Disjunction (Or) ==========

FormulaResult ProofStack::or_intro_l(FormulaHandle const &a, FormulaHandle const &b) {
    if (!is_derived(a)) {
        return MAKE_ERROR << "or_intro_l: formula not derived: " << a.get();
    }
    auto formula = formula_builder_.make_or(a, b);
    derive_in_current_scope(formula);
    return formula;
}

FormulaResult ProofStack::or_intro_r(FormulaHandle const &a, FormulaHandle const &b) {
    if (!is_derived(b)) {
        return MAKE_ERROR << "or_intro_r: formula not derived: " << b.get();
    }
    auto formula = formula_builder_.make_or(a, b);
    derive_in_current_scope(formula);
    return formula;
}

FormulaResult ProofStack::or_elim(FormulaHandle const &or_formula,
                                   FormulaHandle const &a_implies_c,
                                   FormulaHandle const &b_implies_c) {
    if (!is_derived(or_formula)) {
        return MAKE_ERROR << "or_elim: disjunction not derived: " << or_formula.get();
    }
    if (!is_derived(a_implies_c)) {
        return MAKE_ERROR << "or_elim: first implication not derived: " << a_implies_c.get();
    }
    if (!is_derived(b_implies_c)) {
        return MAKE_ERROR << "or_elim: second implication not derived: " << b_implies_c.get();
    }

    // Check or_formula is A ∨ B
    const Formula& orf = or_formula.get();
    if (!orf.is_compound() || orf.as_compound().op != Op::Or) {
        return MAKE_ERROR << "or_elim: expected Or formula, got: " << orf;
    }
    const Compound& or_c = orf.as_compound();

    // Check a_implies_c is A → C
    const Formula& f1 = a_implies_c.get();
    if (!f1.is_compound() || f1.as_compound().op != Op::Implies) {
        return MAKE_ERROR << "or_elim: expected implication for first case, got: " << f1;
    }
    const Compound& impl1 = f1.as_compound();

    // Check b_implies_c is B → C
    const Formula& f2 = b_implies_c.get();
    if (!f2.is_compound() || f2.as_compound().op != Op::Implies) {
        return MAKE_ERROR << "or_elim: expected implication for second case, got: " << f2;
    }
    const Compound& impl2 = f2.as_compound();

    // Verify A matches
    if (or_c.left != impl1.left) {
        return MAKE_ERROR << "or_elim: left disjunct doesn't match first implication antecedent";
    }
    // Verify B matches
    if (or_c.right != impl2.left) {
        return MAKE_ERROR << "or_elim: right disjunct doesn't match second implication antecedent";
    }
    // Verify both implications have same consequent C
    if (impl1.right != impl2.right) {
        return MAKE_ERROR << "or_elim: implications have different consequents";
    }

    derive_in_current_scope(impl1.right);
    return impl1.right;
}

// ========== Implication ==========

FormulaResult ProofStack::implies_intro(FormulaHandle const & conclusion) {
    if (scopes.empty() || !std::holds_alternative<AssumptionScope>(*scopes.rbegin())) {
        return MAKE_ERROR << "implies_intro: current scope is not an assumption scope";
    }

    auto& scope = std::get<AssumptionScope>(*scopes.rbegin());
    if (!scope.contains(conclusion)) {
        return MAKE_ERROR << "implies_intro: conclusion not derived in current scope: " << conclusion.get();
    }

    FormulaHandle assumption = scope.get_formula();
    auto new_formula = formula_builder_.make_implies(assumption, conclusion);

    // Pop the assumption scope
    scopes.pop_back();

    // Derive in parent scope
    derive_in_current_scope(new_formula);
    return new_formula;
}

FormulaResult ProofStack::implies_elim(FormulaHandle const &implication, FormulaHandle const &antecedent) {
    if (!is_derived(implication)) {
        return MAKE_ERROR << "implies_elim: implication not derived: " << implication.get();
    }
    if (!is_derived(antecedent)) {
        return MAKE_ERROR << "implies_elim: antecedent not derived: " << antecedent.get();
    }

    const Formula& f = implication.get();
    if (!f.is_compound()) {
        return MAKE_ERROR << "implies_elim: expected compound formula, got: " << f;
    }
    const Compound& c = f.as_compound();
    if (c.op != Op::Implies) {
        return MAKE_ERROR << "implies_elim: expected Implies, got: " << f;
    }
    if (c.left != antecedent) {
        return MAKE_ERROR << "implies_elim: antecedent doesn't match implication's left side";
    }

    derive_in_current_scope(c.right);
    return c.right;
}

// ========== Negation ==========

FormulaResult ProofStack::not_intro(FormulaHandle const &bottom) {
    if (scopes.empty() || !std::holds_alternative<AssumptionScope>(*scopes.rbegin())) {
        return MAKE_ERROR << "not_intro: current scope is not an assumption scope";
    }

    auto& scope = std::get<AssumptionScope>(*scopes.rbegin());
    if (!scope.contains(bottom)) {
        return MAKE_ERROR << "not_intro: bottom not derived in current scope";
    }

    // Check that bottom is actually ⊥
    const Formula& f = bottom.get();
    if (!f.is_compound() || f.as_compound().op != Op::Bottom) {
        return MAKE_ERROR << "not_intro: expected bottom (⊥), got: " << f;
    }

    FormulaHandle assumption = scope.get_formula();
    auto negation = formula_builder_.make_not(assumption);

    // Pop the assumption scope
    scopes.pop_back();

    // Derive in parent scope
    derive_in_current_scope(negation);
    return negation;
}

FormulaResult ProofStack::not_elim(FormulaHandle const &negation, FormulaHandle const &formula) {
    if (!is_derived(negation)) {
        return MAKE_ERROR << "not_elim: negation not derived: " << negation.get();
    }
    if (!is_derived(formula)) {
        return MAKE_ERROR << "not_elim: formula not derived: " << formula.get();
    }

    const Formula& neg_f = negation.get();
    if (!neg_f.is_compound()) {
        return MAKE_ERROR << "not_elim: expected compound formula, got: " << neg_f;
    }
    const Compound& c = neg_f.as_compound();
    if (c.op != Op::Not) {
        return MAKE_ERROR << "not_elim: expected Not, got: " << neg_f;
    }
    if (c.left != formula) {
        return MAKE_ERROR << "not_elim: formula doesn't match negation's operand";
    }

    auto bottom = formula_builder_.make_bottom();
    derive_in_current_scope(bottom);
    return bottom;
}

// ========== Bottom (Falsum) ==========

FormulaResult ProofStack::bottom_elim(FormulaHandle const &bottom, FormulaHandle const &formula) {
    if (!is_derived(bottom)) {
        return MAKE_ERROR << "bottom_elim: bottom not derived: " << bottom.get();
    }

    const Formula& f = bottom.get();
    if (!f.is_compound() || f.as_compound().op != Op::Bottom) {
        return MAKE_ERROR << "bottom_elim: expected bottom (⊥), got: " << f;
    }

    // Ex falso quodlibet: from ⊥ we can derive anything
    derive_in_current_scope(formula);
    return formula;
}

// ========== Biconditional (Iff) ==========

FormulaResult ProofStack::iff_intro(FormulaHandle const &a_implies_b, FormulaHandle const &b_implies_a) {
    if (!is_derived(a_implies_b)) {
        return MAKE_ERROR << "iff_intro: first implication not derived: " << a_implies_b.get();
    }
    if (!is_derived(b_implies_a)) {
        return MAKE_ERROR << "iff_intro: second implication not derived: " << b_implies_a.get();
    }

    const Formula& f1 = a_implies_b.get();
    if (!f1.is_compound() || f1.as_compound().op != Op::Implies) {
        return MAKE_ERROR << "iff_intro: expected implication, got: " << f1;
    }
    const Compound& impl1 = f1.as_compound();

    const Formula& f2 = b_implies_a.get();
    if (!f2.is_compound() || f2.as_compound().op != Op::Implies) {
        return MAKE_ERROR << "iff_intro: expected implication, got: " << f2;
    }
    const Compound& impl2 = f2.as_compound();

    // Check A → B and B → A have matching formulas
    if (impl1.left != impl2.right || impl1.right != impl2.left) {
        return MAKE_ERROR << "iff_intro: implications don't form a biconditional";
    }

    auto iff = formula_builder_.make_iff(impl1.left, impl1.right);
    derive_in_current_scope(iff);
    return iff;
}

FormulaResult ProofStack::iff_elim_l(FormulaHandle const &iff_formula, FormulaHandle const &a) {
    if (!is_derived(iff_formula)) {
        return MAKE_ERROR << "iff_elim_l: biconditional not derived: " << iff_formula.get();
    }
    if (!is_derived(a)) {
        return MAKE_ERROR << "iff_elim_l: formula not derived: " << a.get();
    }

    const Formula& f = iff_formula.get();
    if (!f.is_compound() || f.as_compound().op != Op::Iff) {
        return MAKE_ERROR << "iff_elim_l: expected biconditional, got: " << f;
    }
    const Compound& c = f.as_compound();

    if (c.left != a) {
        return MAKE_ERROR << "iff_elim_l: formula doesn't match biconditional's left side";
    }

    derive_in_current_scope(c.right);
    return c.right;
}

FormulaResult ProofStack::iff_elim_r(FormulaHandle const &iff_formula, FormulaHandle const &b) {
    if (!is_derived(iff_formula)) {
        return MAKE_ERROR << "iff_elim_r: biconditional not derived: " << iff_formula.get();
    }
    if (!is_derived(b)) {
        return MAKE_ERROR << "iff_elim_r: formula not derived: " << b.get();
    }

    const Formula& f = iff_formula.get();
    if (!f.is_compound() || f.as_compound().op != Op::Iff) {
        return MAKE_ERROR << "iff_elim_r: expected biconditional, got: " << f;
    }
    const Compound& c = f.as_compound();

    if (c.right != b) {
        return MAKE_ERROR << "iff_elim_r: formula doesn't match biconditional's right side";
    }

    derive_in_current_scope(c.left);
    return c.left;
}

FormulaResult ProofStack::forall_intro(FormulaHandle const &body) {
    if (scopes.empty() || !std::holds_alternative<FixVarScope>(*scopes.rbegin())) {
        return MAKE_ERROR << "forall_intro: current scope is not a fix_var scope";
    }
    
    auto& scope = std::get<FixVarScope>(*scopes.rbegin());
    if (!scope.contains(body)) {
        return MAKE_ERROR << "forall_intro: body not derived in current scope: " << body.get();
    }

    if (!(scope.get_op() == Op::Forall)) {
        return MAKE_ERROR << "forall_intro: current scope is not forall: " << body.get();
    }

    // Finalize: set body and destroy QuantifierBuilder to create ∀x. body
    FormulaHandle result = scope.finalize(body);

    // Pop the fix_var scope
    scopes.pop_back();

    // Derive in parent scope
    derive_in_current_scope(result);
    return result;
}

FormulaResult ProofStack::forall_elim(FormulaHandle const &formula, Term const& var) {
    if (!is_derived(formula)) {
        return MAKE_ERROR << "forall_elim: formula not derived: " << formula.get();
    }
    if (!(formula->is_quantified() && formula->as_quantified().op == Op::Forall)) {
        return MAKE_ERROR << "forall_elim: formula op is not a forall quantifier";
    }
    int scope_idx = find_scope_for_term(var);
    if (!is_term_accessible(var)) {
        return MAKE_ERROR << "forall_elim: term is not accessible in current scope";
    }
    auto generalized_term = formula->as_quantified().get_var_term();
    auto f = formula_builder_.translate_term(formula->as_quantified().body, generalized_term, var);
    // Derive in the scope that owns the term (fixed var's scope, or base level for constants)
    derive_in_scope(f, scope_idx);
    return f;
}

FormulaResult ProofStack::exists_intro(FormulaHandle const &body, std::optional<Term> witness) {
    if (!is_derived(body)) {
        return MAKE_ERROR << "exists_intro: body not derived: " << body.get();
    }

    // Mode 1: With explicit witness - create ∃x.φ(x) from φ(t)
    if (witness.has_value()) {
        Term w = witness.value();
        if (!is_term_accessible(w)) {
            return MAKE_ERROR << "exists_intro: witness term not accessible";
        }
        FormulaHandle result = make_exists_from_witness(body, w);
        derive_in_current_scope(result);
        return result;
    }

    // Mode 2: In Exists scope - close scope, derive conclusion in parent
    if (scopes.empty() || !std::holds_alternative<FixVarScope>(*scopes.rbegin())) {
        return MAKE_ERROR << "exists_intro: not in Exists scope and no witness provided";
    }

    auto& scope = std::get<FixVarScope>(*scopes.rbegin());

    if (scope.get_op() != Op::Exists) {
        return MAKE_ERROR << "exists_intro: current scope is not Exists scope";
    }

    if (!scope.contains(body)) {
        return MAKE_ERROR << "exists_intro: body not derived in current scope: " << body.get();
    }

    // Check if body contains the witness variable
    var_index witness_var = scope.var_term().as_variable();
    if (formula_contains_fixed_var(body, witness_var)) {
        // Body contains witness - generalize to create ∃x.body
        FormulaHandle result = scope.finalize(body);
        scopes.pop_back();
        derive_in_current_scope(result);
        return result;
    } else {
        // Body doesn't contain witness - just close scope and derive body in parent
        scopes.pop_back();
        derive_in_current_scope(body);
        return body;
    }
}

FormulaResult ProofStack::exists_elim(FormulaHandle const &formula) {
    if (!is_derived(formula)) {
        return MAKE_ERROR << "exists_elim: formula not derived: " << formula.get();
    }
    if (!(formula->is_quantified() && formula->as_quantified().op == Op::Exists)) {
        return MAKE_ERROR << "exists_elim: formula op is not a exists quantifier";
    }
    auto generalized_term = formula->as_quantified().get_var_term();
    scopes.push_back(FixVarScope(formula_builder_, Op::Exists));
    Term var = std::get<FixVarScope>(scopes.back()).var_term();
    auto f = formula_builder_.translate_term(formula->as_quantified().body, generalized_term, var);

    // derive in child scope
    derive_in_current_scope(f);
    return f;
}

// ========== Classical Logic Extensions ==========

FormulaResult ClassicalProofStack::double_neg_elim(FormulaHandle const &double_neg) {
    if (!is_derived(double_neg)) {
        return MAKE_ERROR << "double_neg_elim: formula not derived: " << double_neg.get();
    }

    const Formula& f = double_neg.get();
    // Check it's ¬¬A: Not(Not(A))
    if (!f.is_compound() || f.as_compound().op != Op::Not) {
        return MAKE_ERROR << "double_neg_elim: expected negation, got: " << f;
    }
    const Compound& outer_not = f.as_compound();

    const Formula& inner = outer_not.left.get();
    if (!inner.is_compound() || inner.as_compound().op != Op::Not) {
        return MAKE_ERROR << "double_neg_elim: expected double negation ¬¬A, got: " << f;
    }
    const Compound& inner_not = inner.as_compound();

    // Extract A from ¬¬A
    FormulaHandle a = inner_not.left;
    derive_in_current_scope(a);
    return a;
}

FormulaResult ClassicalProofStack::excluded_middle(FormulaHandle const &formula) {
    // Derive A ∨ ¬A for any formula A
    // This is an axiom in classical logic
    FormulaHandle not_a = builder().make_not(formula);
    FormulaHandle lem = builder().make_or(formula, not_a);
    derive_in_current_scope(lem);
    return lem;
}

FormulaResult ClassicalProofStack::classical_absurd(FormulaHandle const &bottom) {
    // From assumption ¬A leading to ⊥, derive A
    // This is the classical form of reductio ad absurdum
    if (scopes.empty() || !std::holds_alternative<AssumptionScope>(*scopes.rbegin())) {
        return MAKE_ERROR << "classical_absurd: current scope is not an assumption scope";
    }

    auto& scope = std::get<AssumptionScope>(*scopes.rbegin());
    if (!scope.contains(bottom)) {
        return MAKE_ERROR << "classical_absurd: bottom not derived in current scope";
    }

    // Check that bottom is actually ⊥
    const Formula& f = bottom.get();
    if (!f.is_compound() || f.as_compound().op != Op::Bottom) {
        return MAKE_ERROR << "classical_absurd: expected bottom (⊥), got: " << f;
    }

    // Check that assumption is ¬A
    FormulaHandle assumption = scope.get_formula();
    const Formula& neg_f = assumption.get();
    if (!neg_f.is_compound() || neg_f.as_compound().op != Op::Not) {
        return MAKE_ERROR << "classical_absurd: assumption must be a negation ¬A, got: " << neg_f;
    }

    // Extract A from ¬A
    FormulaHandle a = neg_f.as_compound().left;

    // Pop the assumption scope
    scopes.pop_back();

    // Derive A in parent scope (this is the classical part - from ¬A → ⊥, derive A)
    derive_in_current_scope(a);
    return a;
}

FormulaResult ClassicalProofStack::peirce(FormulaHandle const &a, FormulaHandle const &b) {
    // Derive ((A → B) → A) → A
    // This is Peirce's law, which is equivalent to LEM in classical logic
    FormulaHandle a_impl_b = builder().make_implies(a, b);
    FormulaHandle inner = builder().make_implies(a_impl_b, a);
    FormulaHandle peirce_formula = builder().make_implies(inner, a);
    derive_in_current_scope(peirce_formula);
    return peirce_formula;
}

}  // namespace logic
