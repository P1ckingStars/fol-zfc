# Propositional Logic Natural Deduction Prover

A natural deduction theorem prover for propositional logic written in C++ with Bazel.

## Features

- **Formula Parser**: Supports multiple syntax styles (ASCII and Unicode)
- **Automated Proof Search**: Backward-chaining algorithm with depth limiting
- **Fitch-Style Proofs**: Human-readable proof output with line numbers and scope indicators
- **Complete Rule Set**: All standard natural deduction introduction and elimination rules

## Building

Requires Bazel 7+ and a C++20 compatible compiler.

```bash
# Build the prover
bazel build //src:main

# Run the demo
bazel run //src:main

# Run tests
bazel test //test:all_tests
```

## Formula Syntax

| Connective | Symbols |
|------------|---------|
| Negation | `~`, `!`, `not`, `¬` |
| Conjunction | `&`, `and`, `/\`, `∧` |
| Disjunction | `\|`, `or`, `\/`, `∨` |
| Implication | `->`, `implies`, `→` |
| Biconditional | `<->`, `iff`, `↔` |
| Bottom | `false`, `_\|_`, `⊥` |

Precedence (lowest to highest): `<->` < `->` < `\|` < `&` < `~`

Implication is right-associative: `A -> B -> C` parses as `A -> (B -> C)`

## Example Output

```
═══════════════════════════════════════════════════════
Theorem: Hypothetical Syllogism
───────────────────────────────────────────────────────
Premise: A → B
Premise: B → C
Goal:    A → C
───────────────────────────────────────────────────────
✓ Proof found!

1. │ ┌ A        Assumption
2. │   A → B    Premise
3. │   B        →E 1, 2
4. │   B → C    Premise
5. │   C        →E 3, 4
6. A → C        →I 1, 5
```

## Inference Rules

### Introduction Rules

| Rule | Name | Description |
|------|------|-------------|
| `∧I` | And-Intro | From `A` and `B`, derive `A ∧ B` |
| `∨IL` | Or-Intro-Left | From `A`, derive `A ∨ B` |
| `∨IR` | Or-Intro-Right | From `B`, derive `A ∨ B` |
| `→I` | Implies-Intro | Assume `A`, derive `B`, discharge to get `A → B` |
| `¬I` | Not-Intro | Assume `A`, derive `⊥`, discharge to get `¬A` |
| `↔I` | Iff-Intro | From `A → B` and `B → A`, derive `A ↔ B` |
| `⊥I` | Bottom-Intro | From `A` and `¬A`, derive `⊥` |

### Elimination Rules

| Rule | Name | Description |
|------|------|-------------|
| `∧EL` | And-Elim-Left | From `A ∧ B`, derive `A` |
| `∧ER` | And-Elim-Right | From `A ∧ B`, derive `B` |
| `∨E` | Or-Elim | From `A ∨ B` with cases `A → C` and `B → C`, derive `C` |
| `→E` | Implies-Elim | From `A` and `A → B`, derive `B` (Modus Ponens) |
| `¬E` | Not-Elim | From `¬¬A`, derive `A` (Double Negation Elimination) |
| `↔EL` | Iff-Elim-Left | From `A ↔ B`, derive `A → B` |
| `↔ER` | Iff-Elim-Right | From `A ↔ B`, derive `B → A` |
| `⊥E` | Bottom-Elim | From `⊥`, derive anything (Ex Falso Quodlibet) |

## Project Structure

```
src/
├── formula.h/.cpp       # Formula AST and factory functions
├── parser.h/.cpp        # Recursive descent parser
├── rule_engine.h/.cpp   # Inference rules and proof context
├── prover.h/.cpp        # Backward-chaining proof search
├── fitch.h/.cpp         # Fitch-style proof linearization
└── main.cpp             # Demo application

test/
├── parser_test.cpp      # Parser unit tests
├── prover_test.cpp      # Prover integration tests
└── rule_engine_test.cpp # Rule engine unit tests
```

## API Usage

```cpp
#include "parser.h"
#include "prover.h"
#include "fitch.h"

using namespace logic;

// Parse formulas
auto premise1 = parse("A -> B");
auto premise2 = parse("A");
auto goal = parse("B");

// Configure and run prover
ProverConfig config;
config.max_depth = 25;
Prover prover(config);

auto result = prover.prove({premise1, premise2}, goal);

if (result.success) {
    FitchPrinter printer;
    std::cout << printer.print(**result.proof);
}
```

## Theorems Proven

The demo proves the following theorems:

- Modus Ponens: `A, A → B ⊢ B`
- Hypothetical Syllogism: `A → B, B → C ⊢ A → C`
- Conjunction Commutativity: `⊢ (A ∧ B) → (B ∧ A)`
- Disjunction Introduction: `A ⊢ A ∨ B`
- Contraposition: `A → B ⊢ ¬B → ¬A`
- Ex Falso Quodlibet: `A, ¬A ⊢ B`
- Double Negation Elimination: `¬¬A ⊢ A`
- Biconditional Introduction: `A → B, B → A ⊢ A ↔ B`
- Identity: `⊢ A → A`

## Limitations

- Proof search can be slow for complex theorems requiring deep case analysis
- Some proofs are more verbose than optimal due to the search strategy
- Classical logic only (includes double negation elimination)

## Future Improvements

- Proof optimization to find shorter proofs
- Better heuristics for proof search ordering
- Support for first-order logic (quantifiers)
- Interactive proof mode
- Proof verification/checking
