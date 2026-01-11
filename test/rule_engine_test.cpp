#include "src/rule_engine.h"

#include <gtest/gtest.h>

using namespace logic;

class RuleEngineTest : public ::testing::Test {
protected:
    RuleEngine engine;
    Context ctx;
};

TEST_F(RuleEngineTest, AndIntroduction) {
    auto a = atom("A");
    auto b = atom("B");

    ctx.add_premise(a);
    ctx.add_premise(b);

    auto proof_a = ctx.find(*a).value();
    auto proof_b = ctx.find(*b).value();

    auto result = engine.and_intro(proof_a, proof_b);

    ASSERT_TRUE(result.success);
    EXPECT_EQ(to_string(*result.proof->conclusion), "A ∧ B");
    EXPECT_EQ(result.proof->rule, Rule::AndIntro);
}

TEST_F(RuleEngineTest, AndElimination) {
    auto a = atom("A");
    auto b = atom("B");
    auto a_and_b = conj(a, b);

    ctx.add_premise(a_and_b);
    auto proof = ctx.find(*a_and_b).value();

    auto left = engine.and_elim_left(proof);
    auto right = engine.and_elim_right(proof);

    ASSERT_TRUE(left.success);
    ASSERT_TRUE(right.success);
    EXPECT_EQ(to_string(*left.proof->conclusion), "A");
    EXPECT_EQ(to_string(*right.proof->conclusion), "B");
}

TEST_F(RuleEngineTest, ImpliesElimination_ModusPonens) {
    auto a = atom("A");
    auto b = atom("B");
    auto a_impl_b = impl(a, b);

    ctx.add_premise(a);
    ctx.add_premise(a_impl_b);

    auto proof_a = ctx.find(*a).value();
    auto proof_impl = ctx.find(*a_impl_b).value();

    auto result = engine.implies_elim(proof_a, proof_impl);

    ASSERT_TRUE(result.success);
    EXPECT_EQ(to_string(*result.proof->conclusion), "B");
    EXPECT_EQ(result.proof->rule, Rule::ImpliesElim);
}

TEST_F(RuleEngineTest, ImpliesIntroduction) {
    // To prove A → B, assume A and derive B
    auto a = atom("A");
    auto b = atom("B");

    // Setup: we have A → B as a premise, and we assume A
    auto a_impl_b = impl(a, b);
    ctx.add_premise(a_impl_b);

    ctx.push_scope();
    auto assumption_proof = ctx.add_assumption(a);

    // Derive B using modus ponens
    auto proof_impl = ctx.find(*a_impl_b).value();
    auto mp_result = engine.implies_elim(assumption_proof, proof_impl);
    ASSERT_TRUE(mp_result.success);

    ctx.pop_scope();

    // Now we can introduce A → B (trivially, since we already have it)
    auto intro_result = engine.implies_intro(a, mp_result.proof);

    ASSERT_TRUE(intro_result.success);
    EXPECT_EQ(to_string(*intro_result.proof->conclusion), "A → B");
    EXPECT_EQ(intro_result.proof->rule, Rule::ImpliesIntro);
}

TEST_F(RuleEngineTest, OrIntroduction) {
    auto a = atom("A");
    auto b = atom("B");

    ctx.add_premise(a);
    auto proof_a = ctx.find(*a).value();

    auto left_result = engine.or_intro_left(proof_a, b);
    auto right_result = engine.or_intro_right(a, make_premise(b));

    ASSERT_TRUE(left_result.success);
    ASSERT_TRUE(right_result.success);
    EXPECT_EQ(to_string(*left_result.proof->conclusion), "A ∨ B");
    EXPECT_EQ(to_string(*right_result.proof->conclusion), "A ∨ B");
}

TEST_F(RuleEngineTest, BottomIntroAndElim) {
    auto a = atom("A");
    auto not_a = neg(a);
    auto b = atom("B");

    ctx.add_premise(a);
    ctx.add_premise(not_a);

    auto proof_a = ctx.find(*a).value();
    auto proof_not_a = ctx.find(*not_a).value();

    // A and ¬A gives ⊥
    auto bottom_result = engine.bottom_intro(proof_a, proof_not_a);
    ASSERT_TRUE(bottom_result.success);
    EXPECT_EQ(to_string(*bottom_result.proof->conclusion), "⊥");

    // From ⊥ we can derive anything
    auto anything_result = engine.bottom_elim(bottom_result.proof, b);
    ASSERT_TRUE(anything_result.success);
    EXPECT_EQ(to_string(*anything_result.proof->conclusion), "B");
}

TEST_F(RuleEngineTest, DoubleNegationElimination) {
    auto a = atom("A");
    auto not_not_a = neg(neg(a));

    ctx.add_premise(not_not_a);
    auto proof = ctx.find(*not_not_a).value();

    auto result = engine.not_elim(proof);

    ASSERT_TRUE(result.success);
    EXPECT_EQ(to_string(*result.proof->conclusion), "A");
}

TEST_F(RuleEngineTest, ProofPrinting) {
    auto a = atom("A");
    auto b = atom("B");

    ctx.add_premise(a);
    ctx.add_premise(b);

    auto proof_a = ctx.find(*a).value();
    auto proof_b = ctx.find(*b).value();

    auto conj_result = engine.and_intro(proof_a, proof_b);

    std::string output = print_proof(*conj_result.proof);
    EXPECT_NE(output.find("∧I"), std::string::npos);
    EXPECT_NE(output.find("A ∧ B"), std::string::npos);
}

TEST_F(RuleEngineTest, ErrorOnInvalidRule) {
    auto a = atom("A");  // Not a conjunction
    ctx.add_premise(a);
    auto proof = ctx.find(*a).value();

    auto result = engine.and_elim_left(proof);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error.empty());
}
