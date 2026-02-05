# First-Order Logic Theorem Prover with ZFC

A natural deduction theorem prover for first-order logic with ZFC set theory axioms, written in C++.

## Features

- **First-Order Logic**: Full support for universal and existential quantifiers
- **ZFC Set Theory**: ZFC axioms for set-theoretic proofs
- **Formula Parser**: Flex/Bison parser supporting multiple syntax styles
- **Natural Deduction**: Complete rule set for propositional and first-order logic
- **Proof Scripts**: Write and verify proofs in `.fol` files with declarative syntax

## Building

Requires Bazel 7+, C++20 compiler, flex, and bison.

```bash
# Run all tests
bazel test //test:all_tests

# Or run individual test suites
bazel test //test:core_test       # Core logic tests
bazel test //test:runtime_test    # Runtime and proof execution tests
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

Proof scripts use `.fol` files with axioms, claims, and proofs:

```fol
# Include other files
include "base_axioms.fol"

# Axioms and claims must be sentences (no free variables)
axiom all_P: forall x. P(x)
axiom all_P_impl_Q: forall x. (P(x) -> Q(x))
claim all_Q: forall x. Q(x)

# Proof block with natural deduction rules
proof all_Q:
    fix x                           # Introduce eigenvariable
    h1 = use all_P                  # Get axiom as premise
    h2 = forall_elim h1, x          # Instantiate with fixed var
    h3 = use all_P_impl_Q
    h4 = forall_elim h3, x
    h5 = implies_elim h4, h2        # Modus ponens
    h6 = forall_intro h5            # Generalize over x
    qed h6
```

### Proof Step Syntax

| Step | Description |
|------|-------------|
| `fix x` | Introduce eigenvariable for universal intro/existential elim |
| `h = use name` | Get axiom or proven theorem as premise |
| `h = assume φ` | Assume formula (for implies_intro, not_intro) |
| `h = let φ` | Create formula handle without assuming (for or_intro) |
| `h = rule args` | Apply inference rule |
| `qed h` | Complete proof with final step |

### Example: Proof with Fixed Variables

Fixed variables introduced by `fix` can be referenced in `assume` and `let` formulas:

```fol
axiom singleton_def: forall s. forall x. (singleton(s, x) <-> forall z. (elem(z, s) <-> eq(z, x)))

claim singleton_in_pair: forall p. forall a. forall b. forall s.
    ((pair(p, a, b) & singleton(s, a)) -> elem(s, p))

proof singleton_in_pair:
    fix p
    fix a
    fix b
    fix s
    h = assume pair(p, a, b) & singleton(s, a)    # References fixed vars
    h_pair = and_elim_l h
    h_sing = and_elim_r h
    # ... proof steps ...
    d2 = let forall w. (elem(w, s) <-> (eq(w, a) | eq(w, b)))  # For or_intro
    h_disj = or_intro_l h_d1, d2
    # ...
    qed result
```

## Project Structure

```
src/
├── core/
│   ├── formula.h/.cpp    # Formula AST and database
│   └── proof.h           # Proof construction and steps
├── parser/
│   ├── formula.l         # Flex lexer specification
│   ├── formula.y         # Bison parser specification
│   ├── formula_ast.h     # Parser AST nodes
│   ├── parser.h          # Parser API
│   └── parser_bison.cpp  # AST to formula conversion
├── runtime/
│   └── runtime.h/.cpp    # Runtime and ProofContext classes
├── util/
│   ├── registry.h        # Arena-style registry for formulas
│   ├── error.h           # Error handling utilities
│   └── logging.h         # Logging utilities

test/
├── core_test.cpp         # Core logic tests
└── runtime_test.cpp      # Runtime and proof execution tests

zfc/
├── axioms.fol            # ZFC axioms
└── ordered_pair.fol      # Kuratowski ordered pair proofs
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
#include "runtime/runtime.h"

using namespace logic;

// Create runtime and load axioms
Runtime rt;
rt.load(R"(
    axiom all_P: forall x. P(x)
    axiom all_P_impl_Q: forall x. (P(x) -> Q(x))
    claim all_Q: forall x. Q(x)
)");

// Or load from file
rt.load_file("zfc/axioms.fol");

// Execute proofs from a file
auto result = rt.load_file_with_proofs("zfc/ordered_pair.fol");
if (result.ok()) {
    rt.execute_all_proofs(result.value());
}
```

## Regenerating Parser

After modifying `formula.l` or `formula.y`:

```bash
cd src/parser
flex --header-file=formula_lexer.h -o formula_lexer.cc formula.l
bison -d --defines=formula_parser.tab.h -o formula_parser.tab.cc formula.y
```
