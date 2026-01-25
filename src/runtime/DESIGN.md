# Runtime Design

## Overview

A C++ runtime interface for interactive theorem proving in first-order logic.

## Constraints

- **Sentences only**: All axioms and theorems are sentences (all variables bound by quantifiers)
- **First-order only**: No propositional atoms. Use predicates: `P(x)`, `elem(x, y)`, etc.

## Architecture

- **Runtime** stores axioms/theorems as strings
- **ProofContext** has its own local `ProofDatabase` for proof construction
- `use()` parses axiom/theorem into local db and creates premise step
- Each proof is isolated - local db discarded after proof

## Interface

### Runtime

```cpp
class Runtime {
public:
    void axiom(const std::string& name, const std::string& sentence);
    ProofContext prove(const std::string& name, const std::string& sentence);
    bool has(const std::string& name) const;
};
```

### ProofContext

```cpp
class ProofContext {
public:
    step_id use(const std::string& name);  // Get premise for axiom/theorem
    formula_id goal() const;
    formula_id parse(const std::string& formula);
    ProofDatabase& db();

    // Proof construction
    step_id assume(formula_id f);
    step_id and_intro(step_id left, step_id right);
    step_id implies_intro(assumption_id assumption, step_id conclusion);
    step_id implies_elim(step_id impl, step_id antecedent);
    step_id forall_intro(step_id body, var_index var);
    step_id forall_elim(step_id forall_step, Term term);
    // ... other rules

    assumption_id get_assumption_id(step_id s) const;
    const std::set<std::string>& used() const;
    bool qed();
};
```

## Usage

```cpp
Runtime rt;

rt.axiom("P_a", "P(a)");
rt.axiom("P_impl_Q", "forall x. (P(x) -> Q(x))");

{
    auto ctx = rt.prove("Q_a", "Q(a)");

    step_id p_a = ctx.use("P_a");
    step_id all_pq = ctx.use("P_impl_Q");

    // Get constant 'a' from local db
    auto a_const = ctx.db().find_constant("a").value();
    step_id pq_a = ctx.forall_elim(all_pq, Term::constant(a_const));
    step_id q_a = ctx.implies_elim(pq_a, p_a);

    for (const auto& name : ctx.used()) {
        std::cout << "Used: " << name << "\n";
    }

    ctx.qed();
}
```

## Key Points

1. **`use()` parses into local db** - Each axiom/theorem is parsed fresh into the proof's local database
2. **Premise vs Assumption**:
   - `use()` creates a **Premise** - given truth that doesn't need discharge
   - `assume()` creates an **Assumption** - hypothetical that must be discharged via `implies_intro`, `not_intro`, etc.
3. **`implies_intro` takes `assumption_id`** - Use `get_assumption_id()` to get it from an assumption step
4. **`used()` tracks dependencies** - Automatically recorded when `use()` is called
5. **`qed()` validates** - Checks conclusion matches goal and all assumptions are discharged (premises don't count)

## File Structure

```
src/runtime/
├── SPEC.md
├── DESIGN.md
├── runtime.h
├── runtime.cpp
└── BUILD.bazel
```
