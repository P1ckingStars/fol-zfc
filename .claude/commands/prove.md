# Prove a Claim

Write a formal proof for the claim **$ARGUMENTS** in the FOL-ZFC proof system. Prove only this ONE claim, then stop.

## Workflow

### Step 1: Find the claim

Search `zfc/**/*.fol` files for `claim $ARGUMENTS:`. Read the target .fol file and any files it includes (via `include` directives). If no such claim exists, report an error and stop.

### Step 2: Understand the context

Identify everything available for the proof:
- **Axioms**: All `axiom` statements (from the file and its includes)
- **Previously proven theorems**: All `proof` blocks that appear before where the new proof will go. Once a claim has a `proof` block, it becomes a theorem that can be `use`d.
- **The claim's formula**: Parse the claim to understand its logical structure

### Step 3: Plan the proof strategy

Analyze the claim's formula structure:
- **`forall x. ...`**: Will need `fix x` at the start and `forall_intro` at the end
- **`A -> B`**: Will need `assume A` and `implies_intro` to close
- **`A & B`**: Will need to derive both A and B, then `and_intro`
- **`A | B`**: Will need to derive one side and use `or_intro_l` or `or_intro_r`
- **`A <-> B`**: Will need both directions as implications, then `iff_intro`

Common proof patterns:
- To use a universally quantified axiom/theorem: `use` it, then `forall_elim` for each quantifier
- To extract from a biconditional definition: `iff_elim_l` (forward) or `iff_elim_r` (backward)
- To prove equality `eq(a,b)`: use `extensionality` to reduce to membership equivalence
- To substitute equals: `eq_subst h_eq, h_formula` replaces term `a` with `b` given `eq(a,b)`
- To create a formula handle without deriving it (e.g. for `or_intro`): use `let`
- To construct a concrete set: use the `pairing` axiom + `exists_elim` to get a witness
- To get a singleton from `pairing(a,a)`: need OR-idempotency conversion (~15 lines)
- To get a doubleton from `pairing(a,b)`: `doubleton_def` RHS matches directly
- To transfer membership between equal sets: use `eq_elem_r`

### Step 3b: Split complex proofs into lemmas

**If the proof looks complex (multiple existential witnesses, nested case splits, or excluded_middle), split it into smaller helper lemmas.** Signs a proof needs splitting:
- Requires more than one `exists_elim` (constructing multiple sets)
- Needs `excluded_middle` to handle degenerate cases
- Has nested `or_elim` with sub-case analysis inside each branch
- Would exceed ~80 lines

When splitting:
- Each helper lemma must be a sentence (all variables quantified)
- Use `->` instead of `&` for extra hypotheses to simplify usage: `(H1 & H2) -> (extra_hyp -> goal)` is easier to use than `(H1 & H2 & extra_hyp) -> goal`
- Add helper claims to the claims section and proofs in dependency order
- Test each proof incrementally before writing the next

### Step 4: Write the proof

Insert the proof block in the .fol file. Place it after all proofs it depends on and before any proofs that depend on it.

The proof block format:
```fol
proof <claim_name>:
    <steps>
    qed <final_step>
```

### Step 5: Verify

Run the test to check the proof is accepted:
```bash
bazel test //test:runtime_test
```

If it fails, read the test log to find the error, fix the proof, and re-run. Common errors:
- "formula not derived" - a step references something not yet proven in the current scope
- "expected eq(a, b)" - eq_subst's first arg isn't an equality
- "doesn't match" - formula structure mismatch (e.g. wrong side of iff_elim)
- "not in fix_var scope" / "not in assumption scope" - forall_intro/implies_intro called without proper scope

### Step 6: Update the test expectations

If the proof passes, check if `test/runtime_test.cpp` has an `expected_theorems` vector in `test_execute_ordered_pair_proofs`. If so, add `"$ARGUMENTS"` to it.

Re-run `bazel test //test:runtime_test` to confirm all tests pass.

## Available Rules Reference

```
# Scope management
fix x                   # Introduce eigenvariable x
h = assume <formula>    # Assume formula (can reference fixed vars)
h = let <formula>       # Create formula handle without assuming (for or_intro)
h = use <name>          # Use axiom or theorem by name

# Conjunction
and_intro h1, h2        # A, B => A & B
and_elim_l h            # A & B => A
and_elim_r h            # A & B => B

# Disjunction
or_intro_l h1, h2       # A, B_formula => A | B (h1 derived, h2 just formula)
or_intro_r h1, h2       # A_formula, B => A | B (h1 just formula, h2 derived)
or_elim h_or, h_ac, h_bc  # A | B, A->C, B->C => C

# Implication
implies_intro h         # [A] ... B closes assumption scope => A -> B
implies_elim h_impl, h  # A -> B, A => B

# Negation
not_intro h_bot         # [A] ... _|_ closes scope => ~A
not_elim h_neg, h       # ~A, A => _|_

# Biconditional
iff_intro h_ab, h_ba    # A->B, B->A => A <-> B
iff_elim_l h_iff, h_a   # A <-> B, A => B
iff_elim_r h_iff, h_b   # A <-> B, B => A

# Bottom
bottom_elim h_bot, h_f  # _|_, formula => formula (ex falso)

# Quantifiers
forall_intro h          # [x] ... P(x) closes fix scope => forall x. P(x)
forall_elim h, term     # forall x. P(x), t => P(t)
exists_intro h          # P(t) => exists x. P(x) (or closes exists scope if in one)
exists_elim h           # exists x. P(x) => opens witness scope with P(c)
exists_elim h, name     # same, but names the witness variable for use in forall_elim

# Equality
eq_subst h_eq, h_phi    # eq(a,b), phi(a) => phi(b) (replace a with b)

# Classical
double_neg_elim h       # ~~A => A
excluded_middle h       # => A | ~A
```

## Critical Rules

1. **Axioms and claims must be sentences** - no free variables. Everything must be under quantifiers.
2. **`fix` before `assume`/`let`** - Variables must be fixed before they can appear in formulas.
3. **Scopes nest and must close in order** - `forall_intro` closes the innermost `fix` scope, `implies_intro` closes the innermost `assume` scope.
4. **`let` does NOT derive** - it only creates a formula handle. Use it for the non-derived argument of `or_intro_l`/`or_intro_r`, and for `bottom_elim`.
5. **One claim at a time** - prove a single claim, verify it, then move to the next. Previously proven claims become available as theorems.
6. **`exists_intro` closes exists scope** - When inside an exists scope (from `exists_elim`), `exists_intro h` closes the scope. The formula `h` must NOT contain the witness variable.
7. **Compute shared values before case splits** - Derive utilities like `eq_refl`, `eq_sym` instances, and `eq_trans` in the enclosing scope so they're accessible in all branches.
8. **Handle names are global** - All handle names share a flat namespace across the proof. Use distinct names in different branches (e.g. `h_bd_s1` vs `h_bd_s2`).
