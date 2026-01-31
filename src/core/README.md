# Core Module

This module contains the core logic system for first-order logic formulas and natural deduction proofs.

## Key Types

### Terms

A `Term` represents an argument to a predicate. It can be one of three types:

```cpp
Term::generalized(var_index idx)  // Bound by a quantifier (∀ or ∃)
Term::fixed(var_index idx)        // Eigenvariable during proof construction
Term::constant(ConstantHandle h)  // A constant symbol (e.g., 0, ∅)
```

### Formulas

A `Formula` is a variant of:
- `PredicateInstance` - A predicate applied to terms: `P(x, y)`
- `Compound` - Propositional connectives: `∧`, `∨`, `→`, `↔`, `¬`, `⊥`
- `Quantified` - Quantified formula: `∀x.φ` or `∃x.φ`
- `SentenceHandle` - Reference to a closed sub-sentence

### Sentences

A `Sentence` is a closed formula (no free variables). It owns its own `FormulaRegistry` containing all sub-formulas.

## Variable Index Conventions

The system uses two different variable indexing schemes:

### Fixed Variables (During Construction)

When building a formula with `QuantifierBuilder`, variables start as **fixed** (eigenvariables):
- Outer quantifier scope gets index **0**
- Next inner scope gets index **1**
- And so on...

```cpp
// Building: forall x. forall y. R(x, y)
QuantifierBuilder outer(builder, Op::Forall, result);  // var_ = 0
QuantifierBuilder inner(builder, Op::Forall, result);  // var_ = 1
// Body uses: Term::fixed(0) for x, Term::fixed(1) for y
```

### Generalized Variables (After Generalization)

When a `QuantifierBuilder` destructor runs, it generalizes the fixed variable:
- **Inner quantifiers get SMALLER indices**
- **Outer quantifiers get LARGER indices**

This happens because destructors run in reverse order (inner first, outer last).

```
Input:  forall x. forall y. R(x, y)
Output: forall x_1. forall x_0. R(x_1, x_0)
        ^^^^^^^^    ^^^^^^^^
        outer=1     inner=0
```

### Formula.next_gen_var_idx_

Each `Formula` tracks `next_gen_var_idx_` - the next available generalized index:
- `PredicateInstance`: 0
- `Compound`: max of children's indices
- `Quantified`: body's index + 1

When generalizing, the new generalized index is taken from `body.next_gen_var_idx_`.

## QuantifierBuilder

RAII class for building quantified formulas:

```cpp
FormulaHandle result;
{
    QuantifierBuilder qb(builder, Op::Forall, result);
    Term x = qb.var();  // Returns Term::fixed(var_idx)

    // Build body using x
    FormulaHandle body = builder.predicate(P, {x});
    qb.set_body(body);
}  // Destructor: generalizes x and creates ∀x.body
```

**Destructor behavior:**
1. Calls `exit_scope()` on builder
2. Gets `gen_idx = body.next_gen_var_idx_`
3. Translates `Term::fixed(var_)` → `Term::generalized(gen_idx)` in body
4. Creates `Quantified{op, gen_idx, generalized_body}`
5. If closed, wraps in a `Sentence`

## FormulaBuilder

Manages formula construction and variable scoping:

```cpp
FormulaBuilder builder(ctx);

// Create predicates
FormulaHandle p = builder.predicate(pred_handle, {term1, term2});

// Create compounds
FormulaHandle conj = builder.make_and(left, right);
FormulaHandle impl = builder.make_implies(antecedent, consequent);
FormulaHandle neg = builder.make_not(formula);
FormulaHandle bot = builder.make_bottom();

// Term translation (for generalization)
FormulaHandle new_f = builder.translate_term(f, old_term, new_term);
```

## Proof System

### ProofStack

Manages proof construction with scoped assumptions and fixed variables:

```cpp
ProofStack proof(ctx);

// Introduce a fixed variable for ∀-intro
Term x = proof.fix_var();

// Make an assumption for →-intro
FormulaHandle assumed = proof.assume(formula);

// Apply inference rules
proof.and_intro(a, b);       // From A, B derive A ∧ B
proof.implies_elim(impl, a); // From A → B, A derive B
proof.forall_intro(body);    // Close fix_var scope, derive ∀x.body
```

### Inference Rules

**Propositional:**
- `and_intro`, `and_elim_l`, `and_elim_r`
- `or_intro_l`, `or_intro_r`, `or_elim`
- `implies_intro`, `implies_elim`
- `not_intro`, `not_elim`
- `bottom_elim`
- `iff_intro`, `iff_elim_l`, `iff_elim_r`

**Quantifiers:**

| Rule | Description | Scope Effect |
|------|-------------|--------------|
| `forall_intro(body)` | From φ(c) in Forall scope, derive ∀x.φ(x) | Closes scope |
| `forall_elim(∀x.φ, t)` | From ∀x.φ(x), derive φ(t) | Derives in t's scope |
| `exists_intro(body, witness?)` | Two modes (see below) | Depends on mode |
| `exists_elim(∃x.φ)` | From ∃x.φ(x), get φ(c) for fresh c | Opens Exists scope |

**Classical Logic (via `ClassicalProofStack`):**

These rules are valid in classical logic but not in intuitionistic/constructive logic:

| Rule | Description |
|------|-------------|
| `double_neg_elim(¬¬A)` | From ¬¬A, derive A |
| `excluded_middle(A)` | Derive A ∨ ¬A (law of excluded middle) |
| `classical_absurd(⊥)` | From assumption ¬A leading to ⊥, derive A |
| `peirce(A, B)` | Derive ((A → B) → A) → A (Peirce's law) |

### Scope Derivation Rules

When deriving formulas, results are placed in specific scopes:

- **forall_elim**: Result is derived in the scope that introduced the instantiation term
  - If term is a constant → derives at base level
  - If term is a fixed variable → derives in the FixVarScope that owns it
- **Other rules**: Derive in current (innermost) scope

### Quantifier Rule Usage Patterns

**∀-introduction** (proving ∀x.φ(x)):
```cpp
Term c = proof.fix_var();           // Open Forall scope, get fresh c
// ... derive φ(c) using c ...
proof.forall_intro(phi_c);          // Close scope, get ∀x.φ(x)
```

**∀-elimination** (using ∀x.φ(x)):
```cpp
// Have ∀x.φ(x) derived
Term t = /* some accessible term */;
auto phi_t = proof.forall_elim(forall_phi, t);  // Get φ(t)
// Result derived in t's owning scope
```

**∃-introduction** has two modes:

Mode 1: With explicit witness (from φ(t), derive ∃x.φ(x)):
```cpp
// Have φ(a) derived where 'a' is some term (constant or fixed var)
proof.exists_intro(phi_a, a);       // Generalize 'a' to get ∃x.φ(x)
```

Mode 2: In Exists scope (close scope and derive conclusion):
```cpp
// In an Exists scope (opened by exists_elim) with witness c
// ... derive φ(c) or some conclusion C not containing c ...
proof.exists_intro(phi_c);          // If body contains c: generalize to ∃x.φ(x)
                                    // If body doesn't contain c: just derive body in parent
```

**∃-elimination** (using ∃x.φ(x)):
```cpp
// Have ∃x.φ(x) derived
auto phi_c = proof.exists_elim(exists_phi);  // Open Exists scope, get φ(c)
// ... use φ(c) to derive conclusion C (must not contain c) ...
proof.exists_intro(C);              // Close scope, derive C in parent
```

### Classical Logic Usage Patterns

Use `ClassicalProofStack` instead of `ProofStack` to access classical rules:

**Double Negation Elimination** (from ¬¬A, derive A):
```cpp
ClassicalProofStack proof(ctx);
// ... derive ¬¬A ...
auto a = proof.double_neg_elim(not_not_a);  // Get A
```

**Law of Excluded Middle** (derive A ∨ ¬A):
```cpp
ClassicalProofStack proof(ctx);
FormulaHandle A = /* some formula */;
auto lem = proof.excluded_middle(A);  // Get A ∨ ¬A
```

**Classical Proof by Contradiction** (assume ¬A, derive ⊥, conclude A):
```cpp
ClassicalProofStack proof(ctx);
FormulaHandle not_a = proof.builder().make_not(A);
proof.assume(not_a);                  // Assume ¬A
// ... derive ⊥ from ¬A ...
auto a = proof.classical_absurd(bottom);  // Close scope, get A
```

Note: This differs from `not_intro`, which assumes A, derives ⊥, and concludes ¬A.

### Helper Methods

```cpp
// Check if a term is accessible (in scope)
bool is_term_accessible(Term const& t) const;

// Find which scope introduced a fixed variable (-1 for constants/base)
int find_scope_for_term(Term const& t) const;

// Derive formula in a specific scope
void derive_in_scope(FormulaHandle const& formula, int scope_idx);
```

## Example: Building a Formula

```cpp
GlobalContext ctx;
FormulaBuilder builder(ctx);

// Build: forall x. P(x) -> Q(x)
PredicateHandle P = ctx.add_predicate("P", 1);
PredicateHandle Q = ctx.add_predicate("Q", 1);

FormulaHandle result;
{
    QuantifierBuilder forall(builder, Op::Forall, result);
    Term x = forall.var();

    FormulaHandle px = builder.predicate(P, {x});
    FormulaHandle qx = builder.predicate(Q, {x});
    FormulaHandle impl = builder.make_implies(px, qx);

    forall.set_body(impl);
}
// result now contains: forall x_0. P(x_0) -> Q(x_0)
```

## File Structure

- `formula.h` - Core types: Term, Formula, Sentence, FormulaBuilder, QuantifierBuilder
- `formula.cpp` - Implementation of to_string and Sentence methods
- `proof.h` - ProofStack and scope classes
- `proof.cpp` - Implementation of inference rules
