# Metamath Translator Status

## Current Coverage

**2,306 verified theorems** from the first 5,000 Metamath set.mm theorems (46% coverage).
All verified via Bazel: `bazel build //metamath/batches:mm_batch_004`

| Batch | Claims | Status |
|-------|--------|--------|
| mm_batch_000 | 500 | PASS |
| mm_batch_001 | 500 | PASS |
| mm_batch_002 | 500 | PASS |
| mm_batch_003 | 500 | PASS |
| mm_batch_004 | 306 | PASS |
| **Total** | **2,306** | |

## Skip Analysis (2,694 skipped from first 5K)

**~500 primary failures**, ~2,194 cascade failures ("theorem not yet translated").

### Primary failure categories

| Category | Count | Top cascades unblocked |
|---|---|---|
| `df-ss` (subset definition) | 21 | ssel (20), sseq1 (9), ssun1 (9) |
| `F/_ x A` comprehension (nf-class) | 19 | nfcri (30), nfan (14) |
| `df-eu` / `df-rmo` / `df-reu` (uniqueness) | 34 | sbequ12 (15), ax6e (12) |
| `df-csb` / compound class subst | 24 | eleq1 (29), eleq2 (16), elex (28) |
| Set-op comprehension (`A i^i B`, `A u. B`, etc.) | 20+ | uncom (10), incom (10) |
| Restricted quantifier comp (`A.x e. A`, `E.x e. A`) | 11 | ralbidv (13), ralbii (12), ralimi (11) |
| `df-if` (if-then-else) | 4 | iftrue (22) |
| `df-pss` / `df-symdif` / `df-iun` / `df-iin` | 19 | various |
| T./F. encoding mismatch | 17 | trujust, dftru2, falim chains |
| `ax-12` / non-vacuous quantifier | 8 | eximal, ax6evr, spimedv |
| Known encoding issues (skip list) | ~50 | various |

### Top cascade blockers (theorems whose absence blocks the most)

1. **nfcri** (30 dependents) — blocked by `F/_ x A` comprehension
2. **eleq1** (29) — blocked by compound class / df-csb
3. **elex** (28) — blocked by compound class / df-csb
4. **iftrue** (22) — blocked by df-if
5. **ssel** (20) — blocked by df-ss
6. **eleq2d** (19) — blocked by eleq2 (compound class)
7. **eqcomd** (17) — blocked by eqcom → eqeq1 chain
8. **elisset** (16) — blocked by dfclel chain
9. **eleq2** (16) — blocked by compound class
10. **sbequ12** (15) — blocked by df-eu

## Path to 3,000 Theorems (+694 needed)

### High-leverage targets

1. **`df-ss` bridge** (~100+ unlocked)
   - Identity definition: `A C_ B <-> A.x(x e. A -> x e. B)`
   - Straightforward bridge theorem, similar to existing df-ne/df-nel
   - Unlocks ssel, sseq1, sseq2, ssun1, ssun2, ssin, etc.

2. **Restricted quantifier comprehension** (~80+ unlocked)
   - New comprehension patterns for `A. x e. A ph` and `E. x e. A ph`
   - Pattern: `A. x e. A ph` = `A. x (x e. A -> ph)` in comprehension tokens
   - Unlocks ralbidv, ralbii, ralimi, rexbidv, etc.

3. **`F/_ x A` comprehension** (~50+ unlocked)
   - `F/_ x A` = `A. y (A. x (x e. y <-> x e. A))` — non-freeness of x in A
   - Needs new comprehension builder or bridge
   - Unlocks nfcri, nfan chain

4. **`df-eu` / `df-reu` / `df-rmo` bridges** (~50+ unlocked)
   - `E!` (unique existence), restricted variants
   - Identity definitions, bridge-able
   - Unlocks sbequ12, ax6e chains

5. **`df-csb` / compound class forall_elim** (~70+ unlocked, hardest)
   - Class substitution: `[_ A / x _] B`
   - Requires compound class expression handling in forall_elim
   - Unlocks eleq1, eleq2, elex — biggest single cascade

### Estimated yield

- Items 1-2: likely sufficient for 3,000 (~180+ direct unlocks + cascades)
- Items 1-4: ~3,200-3,500 range
- All items: ~3,500+ (approaching 70% coverage of first 5K)

## Architecture

```
Metamath set.mm (47K theorems)
    |
    v
SyntaxParser -> SyntaxToWff -> WffNode AST
    |
    v
build_proof_tree -> ProofTree (DAG from compressed proofs)
    |
    v
emit_proof_tree -> emit_node (dispatch on MM label)
    |                |
    |                +-- Bridge theorems (ax-mp, ax-1/2/3, df-bi/an/or, df-ne/nel, etc.)
    |                +-- Identity biconditionals (df-an, df-or, df-3an, etc.)
    |                +-- Simple theorem refs (emit_simple_use)
    |                +-- Compound theorem refs (emit_comprehension_use)
    |
    v
FOL .fol.def + .fol.proof files
    |
    v
proof_checker (Bazel fol_proof rule) -> .proven marker
```

### Key components

- **mm_translator.cpp**: Orchestration, emit_comprehension_use, emit_simple_use
- **proof_emit_tree.cpp**: Tree-based proof emission, label dispatch
- **proof_emit.cpp**: Shared helpers (claim renderer, bridge use, inline axioms)
- **comprehension.cpp**: Witness set construction via comprehension axioms + iota_elim
- **syntax_to_wff.cpp**: Metamath syntax -> WffNode AST conversion
- **wff_ast.cpp**: WffNode types, wff_subst_map (simultaneous substitution)
- **proof_tree.cpp**: Build proof DAG from Metamath compressed proofs

### Bridge theorems (metamath/bridge/)

28 proven equivalences mapping Metamath axioms/definitions to FOL-ZFC:
- Propositional: ax_1..ax_3, df_bi, df_an, df_or, df_3an, df_3or
- Predicate: ax_4..ax_9, df_ex, forall_imp_dist
- Equality: eq_refl, eq_sym, eq_trans, equequ1, elequ1, elequ2, etc.
- Set-theoretic: axextb_bridge, df_clel
- New: ne_def, nel_def
