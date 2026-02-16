include "functions.fol.h"

# ============================================================
# Remaining ZFC Axioms: Replacement and Choice
# ============================================================
#
# These axioms require the notion of functions as sets of
# ordered pairs, now available from functions.fol.h.

# 9. Replacement
# If f is a function and A is a set, then the image of A under f
# exists as a set: { b : exists a in A. rel_elem(f, a, b) }
axiom replacement: forall f. forall A.
    (is_function(f) -> exists B. forall b.
        (elem(b, B) <-> exists a. (elem(a, A) & rel_elem(f, a, b))))

# 10. Choice
# For any set A of non-empty pairwise disjoint sets, there exists
# a choice function that picks one element from each member of A.
#
# Formally: if every member of A is non-empty, then there exists
# a function f whose domain includes every member of A, and for
# each member x of A, f maps x to some element of x.
axiom choice: forall A.
    ((forall x. (elem(x, A) -> exists y. elem(y, x))) ->
     exists f. (is_function(f) &
        forall x. (elem(x, A) ->
            exists b. (rel_elem(f, x, b) & elem(b, x)))))
