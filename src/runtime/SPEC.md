
# Runtime of FoL logic verifier

A runtime interface can be used on different provers (can be human or machine)

The goal is to set up environment to prove theorems. Client should be able to state the theorem that trying to prove and show the steps to prove it in a block.

Proven theorem can be used to prove other theorm. To use a theorem to prove others, client need to explicitly state the theorem will be used. The runtime will need to check the dependencies of theorem and error on finding dependency cycles. User can name the theorem

User will be able to declare axioms, and should also be able to declare a set of axiom. User can name the axiom.

This runtime is a cpp interface. Need to think of uses cases for AI auto proving or human trying to prove it using FoL. At any moment, the runtime should be able to tell which theorem is proven.
