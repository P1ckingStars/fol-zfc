# FOL-ZFC: First-Order Logic Proof System with ZFC Set Theory

## Overview

A C++ implementation of a natural deduction proof system for first-order logic, with ZFC (Zermelo-Fraenkel with Choice) set theory axioms. Built with Bazel.

## Build Commands

```bash
bazel build //src:main    # Demo binary with example proofs
bazel build //src:run     # Script runner binary
bazel test //test:zfc_test  # Run tests
```

## Architecture

### Directory Structure

```
src/
├── logic/                 # Core logic system
│   ├── formula.h/cpp      # Formula representation (predicates, compounds, quantifiers)
│   ├── proof.h/cpp        # Natural deduction proof system
│   ├── scope.h            # RAII scope classes (AssumptionScope, EigenvariableScope)
│   ├── theory.h/cpp       # Global theory (axioms + theorems database)
│   ├── prover.h/cpp       # Automated backward-chaining prover
│   └── zfc.h/cpp          # ZFC axioms initialization
├── runtime/               # Interactive proving runtime
│   ├── runtime.h/cpp      # Runtime and ProofContext classes
│   ├── SPEC.md            # Requirements specification
│   └── DESIGN.md          # Design document
├── parser/                # Formula parser (Flex/Bison)
│   ├── formula.l          # Flex lexer specification
│   ├── formula.y          # Bison grammar specification
│   ├── formula_ast.h      # AST nodes for parser
│   ├── parser.h           # Parser interface (parse_formula)
│   ├── parser_bison.cpp   # AST to formula_id conversion
│   ├── formula_lexer.h/cc       # Pre-generated lexer
│   └── formula_parser.tab.h/cc  # Pre-generated parser
├── util/
│   └── registry.h         # Generic registry utilities (IndexedStore, KeyedRegistry)
├── script.h/cpp           # Script runner for batch proofs
├── main.cpp               # Interactive demo
└── run.cpp                # Script runner entry point

test/
└── zfc_test.cpp           # Unit tests
```

### Key Types (logic/)

**formula.h:**
- `formula_id`, `predicate_id`, `constant_id` - Registry IDs (0 = invalid)
- `var_index` - De Bruijn-style variable index (0 = x, 1 = y, etc.)
- `Term` - Either a variable or constant
- `Formula` - Variant: `PredicateInstance`, `Compound`, `Quantified`, `Negation`, `Bottom`
- `Op` - Operators: And, Or, Implies, Iff, Not, Bottom, Forall, Exists
- `ProofDatabase` - Central storage for all formulas, predicates, constants

**theory.h:**
- `Theory` - Global store for axioms and theorems, owns the shared `ProofDatabase`
  - `db()` - Access the formula database
  - `add_axiom(f, name)` / `add_theorem(f, name)` - Register known truths
  - `is_known(f)` - Check if formula is axiom or theorem
  - `create_proof()` - Create a new proof context

**proof.h / scope.h:**
- `step_id`, `assumption_id` - Proof step identifiers
- `Rule` - Natural deduction rules (AndIntro, ImpliesElim, ForallIntro, etc.)
- `Proof` - Proof construction with step management
- `AssumptionScope` - RAII for assumption management (implies_intro, not_intro, etc.)
- `EigenvariableScope` - RAII for eigenvariable soundness (forall_intro, exists_elim)

**prover.h:**
- `ProverConfig` - Settings (max_depth, max_steps, verbose)
- `ProverResult` - Proof attempt result (success, proof, stats)
- `ZFCProver` - Backward-chaining automated prover, takes `Theory&`

**runtime/runtime.h:**
- `Runtime` - Stores axioms/theorems as strings, creates proof contexts
- `ProofContext` - Isolated proof environment with local ProofDatabase
  - `use(name)` - Get premise step for axiom/theorem
  - `parse(str)` - Parse formula into local db
  - `used()` - Get set of all axioms/theorems used
  - `qed()` - Complete proof and register theorem

## Natural Deduction Rules

### Propositional
- **Conjunction**: `and_intro`, `and_elim_l`, `and_elim_r`
- **Disjunction**: `or_intro_l`, `or_intro_r`, `or_elim`
- **Implication**: `implies_intro` (discharges assumption), `implies_elim` (modus ponens)
- **Negation**: `not_intro` (from bottom), `not_elim` (produces bottom)
- **Bottom**: `bottom_elim` (ex falso quodlibet)
- **Biconditional**: `iff_intro`, `iff_elim_l`, `iff_elim_r`

### First-Order
- **Universal**: `forall_intro` (requires eigenvariable), `forall_elim` (instantiate)
- **Existential**: `exists_intro`, `exists_elim` (requires eigenvariable)

## RAII Scoping System

Ensures soundness of quantifier rules by tracking eigenvariables.

### AssumptionScope
```cpp
Proof proof = theory.create_proof();
formula_id impl;
{
    AssumptionScope scope(proof, A);  // Assume A
    auto step1 = scope.assumption_step();
    // ... derive B from A ...
    impl = proof.implies_intro(step1, step_deriving_B);
}  // Assumption discharged
```

### EigenvariableScope
```cpp
{
    EigenvariableScope x_scope(proof, 0);  // x is fresh eigenvariable
    auto step = proof.forall_elim(forall_step, Term::var(0));
    // ... derive P(x) ...
    proof.forall_intro(step_n, 0);  // OK: x is eigenvariable
}  // x retired
```

### Soundness Checks
- `forall_intro`: Variable must be eigenvariable AND not free in active assumptions
- `exists_elim`: Witness variable must be eigenvariable AND not free in conclusion

## Parser (Flex/Bison)

Pre-generated files in `src/parser/`. To regenerate after editing `.l` or `.y`:

```bash
cd src/parser
flex --header-file=formula_lexer.h -o formula_lexer.cc formula.l
bison -d --defines=formula_parser.tab.h -o formula_parser.tab.cc formula.y
```

### Syntax

```
Atoms:      A, B, P(x), R(x,y), _|_ (bottom), false
Negation:   ~A, !A, not A
Binary:     A & B, A | B, A -> B, A <-> B
            and, or, implies, iff (keyword alternatives)
Quantified: forall x. P(x), exists x. P(x)
```

### Precedence (lowest to highest)
1. `<->` (iff) - left associative
2. `->` (implies) - right associative
3. `|` (or) - left associative
4. `&` (and) - left associative
5. `~`, `forall`, `exists` - unary/quantifiers
6. atoms, parentheses

## ZFC Axioms

Predicates: `elem(x, y)` (membership), `eq(x, y)` (equality)

Axioms initialized via `zfc::init_zfc(db)`:
1. **Extensionality** - Sets equal iff same members
2. **Empty Set** - Exists set with no members
3. **Pairing** - For any a,b exists {a,b}
4. **Union** - For any set, exists union of its members
5. **Power Set** - For any set, exists set of all subsets
6. **Infinity** - Exists infinite set
7. **Foundation** - No infinite descending membership chains
8. **Choice** - Every collection of non-empty sets has choice function

## Example Usage

### API
```cpp
#include "logic/theory.h"
#include "logic/prover.h"
#include "parser/parser.h"

using namespace logic;

Theory theory;
auto A = parse_formula("A", theory.db());
auto A_impl_B = parse_formula("A -> B", theory.db());
auto B = parse_formula("B", theory.db());

theory.add_axiom(A, "A");
theory.add_axiom(A_impl_B, "A -> B");

ZFCProver prover(theory);
auto result = prover.prove(B);
// result.success == true
```

### Script File
```
# Declare constants with axioms
declare empty : forall x. ~elem(x, empty)

# Prove claims
claim forall A. forall x. (elem(x, A) -> elem(x, A))
```

Run: `bazel run //src:run -- script.zfc`

### Runtime API
```cpp
#include "runtime/runtime.h"

using namespace logic;

Runtime rt;
rt.axiom("P_a", "P(a)");
rt.axiom("P_impl_Q", "forall x. (P(x) -> Q(x))");

auto ctx = rt.prove("Q_a", "Q(a)");
step_id p_a = ctx.use("P_a");
step_id all_pq = ctx.use("P_impl_Q");

auto a = ctx.db().find_constant("a").value();
step_id pq_a = ctx.forall_elim(all_pq, Term::constant(a));
step_id q_a = ctx.implies_elim(pq_a, p_a);

ctx.qed();  // Registers "Q_a" as proven theorem
```

## Build Dependencies

```
src/logic:formula    <- //src/util:registry
src/logic:scope      <- src/logic:formula (header-only)
src/logic:proof      <- src/logic:formula, src/logic:scope
src/logic:theory     <- src/logic:formula, src/logic:proof
src/logic:zfc        <- src/logic:formula
src/logic:prover     <- src/logic:formula, src/logic:proof, src/logic:theory, src/parser:parser
src/parser:parser    <- src/logic:formula
src/runtime:runtime  <- src/logic:formula, src/logic:proof, src/parser:parser
//src:script         <- src/logic:*, src/parser:parser
//src:main           <- src/logic:*, src/parser:parser
//src:run            <- //src:script
```

## Design Decisions

1. **De Bruijn indices**: Variables are indices, not names. Simplifies substitution.

2. **Formula interning**: All formulas stored in ProofDatabase. Equality by ID comparison.

3. **Theory vs ProofDatabase**: Theory holds global axioms/theorems. ProofDatabase is the formula arena shared across all proofs in a theory.

4. **Pre-generated parser**: Flex/Bison outputs committed to repo for clangd compatibility.

5. **RAII scopes**: `AssumptionScope` and `EigenvariableScope` ensure proof soundness automatically.
