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
- `FormulaHandle`, `PredicateHandle`, `SentenceHandle` - Handle types for registry items
- `var_index` - De Bruijn-style variable index (0 = x, 1 = y, etc.)
- `Term` - A variable (generalized or fixed)
- `Formula` - Variant: `PredicateInstance`, `Compound`, `Quantified`
- `Op` - Operators: And, Or, Implies, Iff, Not, Bottom, Forall, Exists
- `GlobalContext` - Central storage for all formulas, predicates, sentences

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
# ZFC axioms and theorems
axiom ext: forall x. forall y. ((forall z. (elem(z, x) <-> elem(z, y))) -> eq(x, y))

# Prove claims
claim identity: forall x. (elem(x, A) -> elem(x, A))
```

### .fol File Syntax
```fol
# Axioms and claims (must be sentences - no free variables)
axiom all_P: forall x. P(x)
axiom all_P_impl_Q: forall x. (P(x) -> Q(x))
claim all_Q: forall x. Q(x)

# Proof block
proof all_Q:
    fix x
    h1 = use all_P
    h2 = forall_elim h1, x
    h3 = use all_P_impl_Q
    h4 = forall_elim h3, x
    h5 = implies_elim h4, h2
    h6 = forall_intro h5
    qed h6

# Include other files
include "base.fol"
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

## Proof Syntax

Proofs can be written in a declarative syntax and executed by the runtime.

### Syntax

```fol
# Axioms and claims (must be sentences - no free variables)
axiom all_P: forall x. P(x)
axiom all_P_impl_Q: forall x. (P(x) -> Q(x))
claim all_Q: forall x. Q(x)

# Proof block
proof all_Q:
    fix x                           # Introduce eigenvariable
    h1 = use all_P                  # Use axiom
    h2 = forall_elim h1, x          # Instantiate with fixed var
    h3 = use all_P_impl_Q
    h4 = forall_elim h3, x
    h5 = implies_elim h4, h2        # Modus ponens
    h6 = forall_intro h5            # Generalize
    qed h6
```

### Important: Sentences Only

**Axioms and claims must be sentences (formulas with no free variables).**

- ✓ `forall x. P(x)` - valid sentence (x is bound)
- ✓ `forall x. (P(x) -> Q(x))` - valid sentence (x is bound)
- ✗ `P(x)` - invalid (x is free)
- ✗ `P(a)` - invalid (a is free, not a constant)

If you want to assert something about a variable, quantify it:
- Instead of `axiom P_x: P(x)` (invalid)
- Use `axiom all_P: forall x. P(x)` (valid)

### Available Rules

```
# Scope management
fix x                   # Introduce eigenvariable x
h = assume <formula>    # Assume formula (can reference fixed vars)
h = let <formula>       # Create formula handle without assuming (for or_intro)
h = use <name>          # Use axiom or theorem by name

# Conjunction
and_intro h1, h2        # A, B ⊢ A ∧ B
and_elim_l h            # A ∧ B ⊢ A
and_elim_r h            # A ∧ B ⊢ B

# Disjunction
or_intro_l h1, h2       # A, B_formula ⊢ A ∨ B (h1 derived, h2 just formula)
or_intro_r h1, h2       # A_formula, B ⊢ A ∨ B (h1 just formula, h2 derived)
or_elim h_or, h_ac, h_bc # A ∨ B, A→C, B→C ⊢ C

# Implication
implies_intro h         # [A] ⊢ B closes scope → A → B
implies_elim h_impl, h  # A → B, A ⊢ B

# Negation
not_intro h_bot         # [A] ⊢ ⊥ closes scope → ¬A
not_elim h_neg, h       # ¬A, A ⊢ ⊥

# Biconditional
iff_intro h_ab, h_ba    # A→B, B→A ⊢ A ↔ B
iff_elim_l h_iff, h_a   # A ↔ B, A ⊢ B
iff_elim_r h_iff, h_b   # A ↔ B, B ⊢ A

# Quantifiers
forall_intro h          # [x] ⊢ P(x) closes fix scope → ∀x.P(x)
forall_elim h, term     # ∀x.P(x), t ⊢ P(t)
exists_intro h          # P(t) ⊢ ∃x.P(x)
exists_elim h           # ∃x.P(x) ⊢ opens witness scope

# Classical
double_neg_elim h       # ¬¬A ⊢ A
excluded_middle h       # ⊢ A ∨ ¬A
```

## Writing Tests

### Critical Rule: Sentences Only

**All axioms and claims in tests MUST be sentences (no free variables).** This is the most common mistake.

```cpp
// WRONG - free variable 'a'
rt.load("axiom P_a: P(a)");

// CORRECT - all variables bound
rt.load("axiom all_P: forall x. P(x)");
```

### Standard Test Pattern

When testing proofs, use the fix/forall_elim/forall_intro pattern:

```cpp
bool test_example() {
    Runtime rt;

    // All axioms and claims must be sentences
    rt.load(R"(
        axiom all_P: forall x. P(x)
        axiom all_P_impl_Q: forall x. (P(x) -> Q(x))
        claim all_Q: forall x. Q(x)
    )");

    auto ctx = rt.prove("all_Q");

    // 1. Fix a variable (creates eigenvariable)
    Term x = ctx.fix_var();

    // 2. Use axioms and instantiate with fixed var
    auto all_p = ctx.use("all_P");
    auto p_x = ctx.forall_elim(all_p.value(), x);

    auto all_pq = ctx.use("all_P_impl_Q");
    auto pq_x = ctx.forall_elim(all_pq.value(), x);

    // 3. Apply rules
    auto q_x = ctx.implies_elim(pq_x.value(), p_x.value());

    // 4. Generalize back to forall
    auto all_q = ctx.forall_intro(q_x.value());

    // 5. Complete proof
    auto result = ctx.qed(all_q.value());
    return result.ok();
}
```

### Proof Syntax Tests

For testing the declarative proof syntax:

```cpp
bool test_proof_syntax() {
    Runtime rt;

    auto result = rt.load_with_proofs(R"(
        axiom all_P: forall x. P(x)
        axiom all_P_impl_Q: forall x. (P(x) -> Q(x))
        claim all_Q: forall x. Q(x)

        proof all_Q:
            fix x
            h1 = use all_P
            h2 = forall_elim h1, x
            h3 = use all_P_impl_Q
            h4 = forall_elim h3, x
            h5 = implies_elim h4, h2
            h6 = forall_intro h5
            qed h6
    )");

    if (!result.ok()) return false;

    auto exec = rt.execute_all_proofs(result.value());
    return exec.ok();
}
```

### Testing with assume and let

The `assume` and `let` keywords can reference fixed variables by name:

```cpp
bool test_assume_with_fixed_vars() {
    Runtime rt;

    auto result = rt.load_with_proofs(R"(
        axiom pair_def: forall p. forall a. forall b. (pair(p, a, b) <-> ...)
        claim pair_elems: forall p. forall a. forall b. forall z.
            ((pair(p, a, b) & elem(z, p)) -> ...)

        proof pair_elems:
            fix p
            fix a
            fix b
            fix z
            h = assume pair(p, a, b) & elem(z, p)   # References fixed vars
            h_pair = and_elim_l h
            # ... rest of proof
            qed result
    )");

    return result.ok() && rt.execute_all_proofs(result.value()).ok();
}
```

### Common Mistakes to Avoid

1. **Using constants**: There are no constants. `P(a)` treats `a` as a free variable.
   ```cpp
   // WRONG
   rt.load("axiom foo: P(a) -> Q(a)");

   // CORRECT
   rt.load("axiom foo: forall x. (P(x) -> Q(x))");
   ```

2. **Forgetting forall_intro**: After working with a fixed variable, generalize back.
   ```cpp
   Term x = ctx.fix_var();
   // ... derive Q(x) ...
   auto all_q = ctx.forall_intro(q_x);  // Don't forget this!
   ```

3. **Using Term::constant()**: This no longer exists. Use only fixed variables from `fix_var()`.

4. **Using assume/let before fix**: Variables must be fixed before they can be used in assume or let.
   ```fol
   # WRONG - x not fixed yet
   h = assume P(x)
   fix x

   # CORRECT - fix first, then assume
   fix x
   h = assume P(x)
   ```

## Example: Ordered Pair Proofs

The `zfc/ordered_pair.fol` file contains proofs about Kuratowski ordered pairs:

```fol
# Definition: (a, b) = {{a}, {a, b}}
axiom pair_def: forall p. forall a. forall b. (pair(p, a, b) <->
    forall z. (elem(z, p) <->
        (forall w. (elem(w, z) <-> eq(w, a))) |
        (forall w. (elem(w, z) <-> (eq(w, a) | eq(w, b))))))

# Prove: singleton {a} is in pair (a, b)
proof singleton_in_pair:
    fix p
    fix a
    fix b
    fix s
    h = assume pair(p, a, b) & singleton(s, a)
    h_pair = and_elim_l h
    h_sing = and_elim_r h
    # Expand pair_def
    pdef = use pair_def
    pdef1 = forall_elim pdef, p
    pdef2 = forall_elim pdef1, a
    pdef3 = forall_elim pdef2, b
    h_all_z = iff_elim_l pdef3, h_pair
    h_s_iff = forall_elim h_all_z, s
    # Expand singleton_def to get D1
    sdef = use singleton_def
    sdef1 = forall_elim sdef, s
    sdef2 = forall_elim sdef1, a
    h_d1 = iff_elim_l sdef2, h_sing
    # Use let to create D2 formula for or_intro
    d2 = let forall w. (elem(w, s) <-> (eq(w, a) | eq(w, b)))
    h_disj = or_intro_l h_d1, d2
    elem_s_p = iff_elim_r h_s_iff, h_disj
    h_impl = implies_intro elem_s_p
    h4 = forall_intro h_impl
    h3 = forall_intro h4
    h2 = forall_intro h3
    h1 = forall_intro h2
    qed h1
```

Key techniques demonstrated:
- **fix**: Introduce eigenvariables for universal quantifier proofs
- **assume**: Assume antecedent for implication introduction
- **let**: Create formula handles for `or_intro` without assuming them
- **forall_elim**: Instantiate universal axioms with fixed variables
- **iff_elim_l/r**: Extract directions from biconditionals
- **forall_intro**: Generalize over fixed variables

## Design Decisions

1. **De Bruijn indices**: Variables are indices, not names. Simplifies substitution.

2. **Formula interning**: All formulas stored in ProofDatabase. Equality by ID comparison.

3. **Theory vs ProofDatabase**: Theory holds global axioms/theorems. ProofDatabase is the formula arena shared across all proofs in a theory.

4. **Pre-generated parser**: Flex/Bison outputs committed to repo for clangd compatibility.

5. **RAII scopes**: `AssumptionScope` and `EigenvariableScope` ensure proof soundness automatically.

6. **Sentences only**: Axioms and claims must be sentences (closed formulas with no free variables). This ensures soundness.

7. **No constants**: The system only has variables (generalized and fixed). All terms in axioms/claims must be bound by quantifiers.

8. **Deferred formula parsing**: Formulas in `assume` and `let` proof steps are parsed during proof execution, not at file parse time. This allows them to reference fixed variables by name.
