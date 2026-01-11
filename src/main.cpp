#include "fitch.h"
#include "parser.h"
#include "prover.h"

#include <iostream>
#include <string>
#include <vector>

using namespace logic;

void prove_and_print(const std::string& name,
                     const std::vector<std::string>& premise_strs,
                     const std::string& goal_str) {
    std::cout << "═══════════════════════════════════════════════════════\n";
    std::cout << "Theorem: " << name << "\n";
    std::cout << "───────────────────────────────────────────────────────\n";

    // Parse premises
    std::vector<FormulaPtr> premises;
    for (const auto& p : premise_strs) {
        premises.push_back(parse(p));
        std::cout << "Premise: " << to_string(*premises.back()) << "\n";
    }

    // Parse goal
    auto goal = parse(goal_str);
    std::cout << "Goal:    " << to_string(*goal) << "\n";
    std::cout << "───────────────────────────────────────────────────────\n";

    // Prove
    ProverConfig config;
    config.max_depth = 25;
    Prover prover(config);
    auto result = prover.prove(premises, goal);

    if (result.success) {
        std::cout << "✓ Proof found!\n\n";
        FitchPrinter printer;
        std::cout << printer.print(**result.proof);
    } else {
        std::cout << "✗ No proof found\n";
    }
    std::cout << "\n";
}

int main() {
    std::cout << "\n";
    std::cout << "╔═══════════════════════════════════════════════════════╗\n";
    std::cout << "║     PROPOSITIONAL LOGIC NATURAL DEDUCTION PROVER      ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════╝\n\n";

    // 1. Modus Ponens
    prove_and_print(
        "Modus Ponens",
        {"A", "A -> B"},
        "B"
    );

    // 2. Hypothetical Syllogism
    prove_and_print(
        "Hypothetical Syllogism",
        {"A -> B", "B -> C"},
        "A -> C"
    );

    // 3. Conjunction commutativity
    prove_and_print(
        "Conjunction Commutativity",
        {},
        "(A & B) -> (B & A)"
    );

    // 4. Disjunction introduction
    prove_and_print(
        "Disjunction Introduction",
        {"A"},
        "A | B"
    );

    // 5. Contraposition
    prove_and_print(
        "Contraposition",
        {"A -> B"},
        "~B -> ~A"
    );

    // 6. Ex falso quodlibet
    prove_and_print(
        "Ex Falso Quodlibet",
        {"A", "~A"},
        "B"
    );

    // 7. Double negation elimination
    prove_and_print(
        "Double Negation Elimination",
        {"~~A"},
        "A"
    );

    // 8. Biconditional
    prove_and_print(
        "Biconditional Introduction",
        {"A -> B", "B -> A"},
        "A <-> B"
    );

    // 9. Law of non-contradiction
    prove_and_print(
        "Law of Non-Contradiction",
        {},
        "~(A & ~A)"
    );

    // 10. Identity
    prove_and_print(
        "Identity",
        {},
        "A -> A"
    );

    prove_and_print("Contrapositive", {"A -> B"}, "~B -> ~A");

    return 0;
}
