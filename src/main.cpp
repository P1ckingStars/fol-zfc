#include "formula.h"
#include "parser.h"
#include "proof.h"
#include "zfc.h"

#include <iomanip>
#include <iostream>
#include <string>

using namespace logic;

void print_separator(const std::string& title) {
    std::cout << "\n════════════════════════════════════════════════════════\n";
    std::cout << "  " << title << "\n";
    std::cout << "════════════════════════════════════════════════════════\n\n";
}

void print_proof(const Proof& proof, ProofDatabase& db) {
    std::cout << "Step | Rule       | Formula\n";
    std::cout << "-----+------------+----------------------------------\n";

    for (const auto& step : proof.steps()) {
        std::cout << std::setw(4) << step.id << " | ";
        std::cout << std::setw(10) << std::left << rule_name(step.rule) << " | ";
        std::cout << db.get_formula(step.conclusion).to_string();

        if (!step.premises.empty()) {
            std::cout << "  [from: ";
            for (size_t i = 0; i < step.premises.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << step.premises[i];
            }
            std::cout << "]";
        }

        if (step.assumption_label) {
            std::cout << "  {assumption " << *step.assumption_label << "}";
        }

        if (!step.discharged.empty()) {
            std::cout << "  [discharges: ";
            for (size_t i = 0; i < step.discharged.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << step.discharged[i];
            }
            std::cout << "]";
        }

        std::cout << "\n";
    }

    std::cout << "\nActive assumptions: ";
    if (proof.active_assumptions().empty()) {
        std::cout << "(none - this is a theorem!)";
    } else {
        for (auto aid : proof.active_assumptions()) {
            std::cout << aid << " ";
        }
    }
    std::cout << "\n";
}

// ==================== Propositional Proofs Using Parser ====================

// Proof: A -> A (identity)
void prove_identity(ProofDatabase& db) {
    print_separator("Proof 1: Identity (A -> A)");

    auto A = parse_formula("A", db);

    Proof proof(db);
    auto step1 = proof.assume(A);
    (void)proof.implies_intro(1, step1);

    std::cout << "Goal: A -> A\n\n";
    print_proof(proof, db);
    std::cout << "\nResult: " << (proof.is_theorem() ? "PROVED" : "INCOMPLETE") << "\n";
}

// Proof: A, A -> B |- B (modus ponens)
void prove_modus_ponens(ProofDatabase& db) {
    print_separator("Proof 2: Modus Ponens (A, A -> B |- B)");

    auto A = parse_formula("A", db);
    auto A_impl_B = parse_formula("A -> B", db);

    Proof proof(db);
    auto step1 = proof.assume(A);
    auto step2 = proof.assume(A_impl_B);
    (void)proof.implies_elim(step2, step1);

    std::cout << "Premises: A, A -> B\n";
    std::cout << "Goal: B\n\n";
    print_proof(proof, db);
}

// Proof: (A & B) -> (B & A) (conjunction commutativity)
void prove_and_commutative(ProofDatabase& db) {
    print_separator("Proof 3: Conjunction Commutativity ((A & B) -> (B & A))");

    auto A_and_B = parse_formula("A & B", db);

    Proof proof(db);
    auto step1 = proof.assume(A_and_B);
    auto step2 = proof.and_elim_r(step1);
    auto step3 = proof.and_elim_l(step1);
    auto step4 = proof.and_intro(step2, step3);
    (void)proof.implies_intro(1, step4);

    std::cout << "Goal: (A & B) -> (B & A)\n\n";
    print_proof(proof, db);
    std::cout << "\nResult: " << (proof.is_theorem() ? "PROVED" : "INCOMPLETE") << "\n";
}

// Proof: A, ~A |- B (ex falso quodlibet)
void prove_ex_falso(ProofDatabase& db) {
    print_separator("Proof 4: Ex Falso Quodlibet (A, ~A |- B)");

    auto A = parse_formula("A", db);
    auto not_A = parse_formula("~A", db);
    auto B = parse_formula("B", db);

    Proof proof(db);
    auto step1 = proof.assume(A);
    auto step2 = proof.assume(not_A);
    auto step3 = proof.not_elim(step2, step1);
    (void)proof.bottom_elim(step3, B);

    std::cout << "Premises: A, ~A\n";
    std::cout << "Goal: B\n\n";
    print_proof(proof, db);
}

// Proof: |- ~(A & ~A) (law of non-contradiction)
void prove_non_contradiction(ProofDatabase& db) {
    print_separator("Proof 5: Law of Non-Contradiction (|- ~(A & ~A))");

    auto A_and_not_A = parse_formula("A & ~A", db);

    Proof proof(db);
    auto step1 = proof.assume(A_and_not_A);
    auto step2 = proof.and_elim_l(step1);
    auto step3 = proof.and_elim_r(step1);
    auto step4 = proof.not_elim(step3, step2);
    (void)proof.not_intro(1, step4);

    std::cout << "Goal: ~(A & ~A)\n\n";
    print_proof(proof, db);
    std::cout << "\nResult: " << (proof.is_theorem() ? "PROVED" : "INCOMPLETE") << "\n";
}

// Proof: A -> B, B -> C |- A -> C (hypothetical syllogism)
void prove_hypothetical_syllogism(ProofDatabase& db) {
    print_separator("Proof 6: Hypothetical Syllogism (A -> B, B -> C |- A -> C)");

    auto A = parse_formula("A", db);
    auto A_impl_B = parse_formula("A -> B", db);
    auto B_impl_C = parse_formula("B -> C", db);

    Proof proof(db);
    auto step1 = proof.assume(A_impl_B);
    auto step2 = proof.assume(B_impl_C);
    auto step3 = proof.assume(A);
    auto step4 = proof.implies_elim(step1, step3);  // B
    auto step5 = proof.implies_elim(step2, step4);  // C
    (void)proof.implies_intro(3, step5);            // A -> C

    std::cout << "Premises: A -> B, B -> C\n";
    std::cout << "Goal: A -> C\n\n";
    print_proof(proof, db);
}

// Proof: A -> B |- ~B -> ~A (contraposition)
void prove_contraposition(ProofDatabase& db) {
    print_separator("Proof 7: Contraposition (A -> B |- ~B -> ~A)");

    auto A = parse_formula("A", db);
    auto A_impl_B = parse_formula("A -> B", db);
    auto not_B = parse_formula("~B", db);

    Proof proof(db);
    auto step1 = proof.assume(A_impl_B);
    auto step2 = proof.assume(not_B);
    auto step3 = proof.assume(A);
    auto step4 = proof.implies_elim(step1, step3);  // B
    auto step5 = proof.not_elim(step2, step4);      // _|_
    auto step6 = proof.not_intro(3, step5);         // ~A (discharge A)
    (void)proof.implies_intro(2, step6);            // ~B -> ~A (discharge ~B)

    std::cout << "Premise: A -> B\n";
    std::cout << "Goal: ~B -> ~A\n\n";
    print_proof(proof, db);
}

// ==================== First-Order Proofs Using Parser ====================

// Proof: forall x. P(x) |- P(c)
void prove_forall_elim(ProofDatabase& db) {
    print_separator("Proof 8: Universal Elimination (forall x. P(x) |- P(c))");

    auto forall_P = parse_formula("forall x. P(x)", db);
    auto const_c = db.find_constant("c");
    if (!const_c) {
        const_c = db.create_constant("c");
    }

    Proof proof(db);
    auto step1 = proof.assume(forall_P);
    (void)proof.forall_elim(step1, Term::constant(*const_c));

    std::cout << "Premise: forall x. P(x)\n";
    std::cout << "Goal: P(c)\n\n";
    print_proof(proof, db);
}

// Proof: P(c) |- exists x. P(x)
void prove_exists_intro(ProofDatabase& db) {
    print_separator("Proof 9: Existential Introduction (P(c) |- exists x. P(x))");

    auto P_c = parse_formula("P(c)", db);
    auto const_c = db.find_constant("c");

    Proof proof(db);
    auto step1 = proof.assume(P_c);
    (void)proof.exists_intro(step1, 0, Term::constant(*const_c));

    std::cout << "Premise: P(c)\n";
    std::cout << "Goal: exists x. P(x)\n\n";
    print_proof(proof, db);
}

// Proof: forall x. (P(x) -> Q(x)), forall x. P(x) |- forall x. Q(x)
void prove_forall_distribution(ProofDatabase& db) {
    print_separator("Proof 10: Universal Distribution");

    auto premise1 = parse_formula("forall x. (P(x) -> Q(x))", db);
    auto premise2 = parse_formula("forall x. P(x)", db);

    Proof proof(db);
    (void)proof.assume(premise1);
    (void)proof.assume(premise2);

    // We need to eliminate forall, apply modus ponens, then reintroduce forall
    // For simplicity, just show the assumptions
    std::cout << "Premises:\n";
    std::cout << "  1. forall x. (P(x) -> Q(x))\n";
    std::cout << "  2. forall x. P(x)\n";
    std::cout << "Goal: forall x. Q(x)\n\n";
    print_proof(proof, db);
    std::cout << "\n(Full proof requires forall-elim, modus ponens, forall-intro)\n";
}

// ==================== Parser Demo ====================

void demo_parser(ProofDatabase& db) {
    print_separator("Parser Demo");

    std::vector<std::string> formulas = {
        "A",
        "A & B",
        "A | B",
        "A -> B",
        "A <-> B",
        "~A",
        "~~A",
        "A & B -> C",
        "(A -> B) & (B -> C)",
        "A | B | C",
        "_|_",
        "P(x)",
        "P(x, y)",
        "R(x, c)",
        "forall x. P(x)",
        "exists x. P(x)",
        "forall x. (P(x) -> Q(x))",
        "forall x. exists y. R(x, y)",
        "exists x. forall y. R(x, y)",
        "forall x. P(x) -> exists x. P(x)",
    };

    std::cout << "Parsing various formulas:\n\n";

    for (const auto& input : formulas) {
        std::cout << "  Input:  \"" << input << "\"\n";

        std::string error;
        auto result = try_parse_formula(input, db, &error);

        if (result) {
            std::cout << "  Parsed: " << db.get_formula(*result).to_string() << "\n";
            std::cout << "  ID:     " << *result << "\n";
        } else {
            std::cout << "  Error:  " << error << "\n";
        }
        std::cout << "\n";
    }
}

// ==================== ZFC Axioms ====================

void show_zfc_axioms(ProofDatabase& db) {
    print_separator("ZFC Axioms");

    auto [predicates, axioms] = zfc::init_zfc(db);

    std::cout << "Predicates:\n";
    std::cout << "  " << db.get_predicate(predicates.elem).get_name() << " (membership)\n";
    std::cout << "  " << db.get_predicate(predicates.eq).get_name() << " (equality)\n";

    std::cout << "\nAxioms registered:\n";
    std::cout << "  1. Extensionality  (sentence_id: " << axioms.extensionality << ")\n";
    std::cout << "  2. Empty Set       (sentence_id: " << axioms.empty_set << ")\n";
    std::cout << "  3. Pairing         (sentence_id: " << axioms.pairing << ")\n";
    std::cout << "  4. Union           (sentence_id: " << axioms.union_ax << ")\n";
    std::cout << "  5. Power Set       (sentence_id: " << axioms.power_set << ")\n";
    std::cout << "  6. Infinity        (sentence_id: " << axioms.infinity << ")\n";
    std::cout << "  7. Foundation      (sentence_id: " << axioms.foundation << ")\n";
    std::cout << "  8. Choice          (sentence_id: " << axioms.choice << ")\n";
}

// ==================== Main ====================

int main() {
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════╗\n";
    std::cout << "║   FIRST-ORDER LOGIC NATURAL DEDUCTION PROOF SYSTEM     ║\n";
    std::cout << "╚════════════════════════════════════════════════════════╝\n";

    ProofDatabase db;

    // Parser demonstration
    demo_parser(db);

    // Propositional proofs
    prove_identity(db);
    prove_modus_ponens(db);
    prove_and_commutative(db);
    prove_ex_falso(db);
    prove_non_contradiction(db);
    prove_hypothetical_syllogism(db);
    prove_contraposition(db);

    // First-order proofs
    prove_forall_elim(db);
    prove_exists_intro(db);
    prove_forall_distribution(db);

    // ZFC
    show_zfc_axioms(db);

    std::cout << "\n════════════════════════════════════════════════════════\n";
    std::cout << "  All demonstrations completed!\n";
    std::cout << "════════════════════════════════════════════════════════\n\n";

    return 0;
}
