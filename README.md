# First-Order Logic Theorem Prover with ZFC

A natural deduction theorem prover for first-order logic with ZFC set theory axioms, written in C++.

## Features

- **First-Order Logic**: Full support for universal and existential quantifiers
- **ZFC Set Theory**: All standard ZFC axioms (extensionality, empty set, pairing, union, power set, infinity, foundation, choice)
- **Formula Parser**: Flex/Bison parser supporting multiple syntax styles
- **Automated Proof Search**: Backward-chaining algorithm with configurable depth/step limits
- **Natural Deduction**: Complete rule set for propositional and first-order logic
- **Script Runner**: Run proof scripts with declarations and claims

## Building

Requires Bazel 7+, C++20 compiler, flex, and bison.

```bash
# Build the interactive prover
bazel build //src:main

# Build the script runner
bazel build //src:run

# Run tests
bazel test //test:zfc_test
```

## Formula Syntax

### Propositional Connectives

| Connective | Symbols |
|------------|---------|
| Negation | `~`, `!`, `not` |
| Conjunction | `&`, `and` |
| Disjunction | `\|`, `or` |
| Implication | `->`, `implies` |
| Biconditional | `<->`, `iff` |
| Bottom | `false`, `_\|_` |

### Quantifiers

| Quantifier | Syntax |
|------------|--------|
| Universal | `forall x. P(x)` |
| Existential | `exists x. P(x)` |

### Predicates

```
P(x)           # Unary predicate
R(x, y)        # Binary predicate
elem(x, y)     # Set membership (x in y)
eq(x, y)       # Equality
```

Precedence (lowest to highest): `<->` < `->` < `|` < `&` < `~` < quantifiers

## Script Syntax

The script runner (`//src:run`) executes proof scripts:

```
# Declare a constant with an axiom
declare empty : forall x. ~elem(x, empty)

# Prove a claim
claim forall A. forall x. (elem(x, A) -> elem(x, A))
```

Run with:
```bash
bazel run //src:run -- path/to/script.zfc
```

Options:
- `-v, --verbose`: Enable verbose output
- `-d, --depth N`: Set max proof search depth (default: 20)
- `-s, --steps N`: Set max proof search steps (default: 10000)

## Project Structure

```
src/
├── logic/
│   ├── formula.h/.cpp    # Formula AST and database
│   ├── proof.h/.cpp      # Proof construction and steps
│   ├── scope.h           # RAII scope management for proofs
│   ├── theory.h/.cpp     # Global theory (axioms + theorems)
│   ├── prover.h/.cpp     # Backward-chaining proof search
│   └── zfc.h/.cpp        # ZFC axiom initialization
├── parser/
│   ├── formula.l         # Flex lexer specification
│   ├── formula.y         # Bison parser specification
│   ├── formula_ast.h     # Parser AST nodes
│   ├── parser.h          # Parser API
│   └── parser_bison.cpp  # AST to formula conversion
├── util/
│   └── registry.h        # Arena-style registry for formulas
├── script.h/.cpp         # Script runner
├── main.cpp              # Interactive demo
└── run.cpp               # Script runner entry point

test/
└── zfc_test.cpp          # Unit tests
```

## Inference Rules

### Propositional Rules

| Rule | Description |
|------|-------------|
| `&I` | And-Intro: From `A` and `B`, derive `A & B` |
| `&EL/&ER` | And-Elim: From `A & B`, derive `A` or `B` |
| `\|IL/\|IR` | Or-Intro: From `A`, derive `A \| B` |
| `\|E` | Or-Elim: Case analysis on disjunction |
| `->I` | Implies-Intro: Assume `A`, derive `B`, get `A -> B` |
| `->E` | Implies-Elim: Modus ponens |
| `~I` | Not-Intro: Assume `A`, derive bottom, get `~A` |
| `~E` | Not-Elim: From `A` and `~A`, derive bottom |
| `<->I` | Iff-Intro: From `A -> B` and `B -> A`, derive `A <-> B` |
| `<->EL/ER` | Iff-Elim: Extract implications from biconditional |
| `_|_E` | Bottom-Elim: Ex falso quodlibet |

### First-Order Rules

| Rule | Description |
|------|-------------|
| `forall-I` | Universal intro with eigenvariable |
| `forall-E` | Universal elim: instantiate with any term |
| `exists-I` | Existential intro: generalize from witness |
| `exists-E` | Existential elim with eigenvariable |

## ZFC Axioms

The prover includes all standard ZFC axioms:

1. **Extensionality**: Sets with same elements are equal
2. **Empty Set**: Existence of empty set
3. **Pairing**: For any a, b, the set {a, b} exists
4. **Union**: For any set, its union exists
5. **Power Set**: For any set, its power set exists
6. **Infinity**: An infinite set exists
7. **Foundation**: No infinite descending membership chains
8. **Choice**: Every family of non-empty sets has a choice function

## API Usage

```cpp
#include "logic/theory.h"
#include "logic/prover.h"
#include "parser/parser.h"

using namespace logic;

// Create a theory (includes formula database)
Theory theory;

// Parse and add axioms
auto axiom = parse_formula("A -> B", theory.db());
theory.add_axiom(axiom, "my axiom");

// Configure prover
ProverConfig config;
config.max_depth = 25;
config.max_steps = 10000;

ZFCProver prover(theory, config);

// Prove a goal
auto goal = parse_formula("A -> B", theory.db());
auto result = prover.prove(goal);

if (result.success) {
    std::cout << "Proof found!\n";
}
```

## Regenerating Parser

After modifying `formula.l` or `formula.y`:

```bash
cd src/parser
flex --header-file=formula_lexer.h -o formula_lexer.cc formula.l
bison -d --defines=formula_parser.tab.h -o formula_parser.tab.cc formula.y
```
