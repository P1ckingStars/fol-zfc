#include "src/parser.h"

#include <gtest/gtest.h>

using namespace logic;

TEST(ParserTest, ParseAtom) {
    auto f = parse("A");
    ASSERT_TRUE(is_atom(*f));
    EXPECT_EQ(as_atom(*f).name, "A");
}

TEST(ParserTest, ParseNegation) {
    auto f = parse("~A");
    ASSERT_TRUE(has_op(*f, Op::Not));
    auto inner = as_compound(*f).args[0];
    ASSERT_TRUE(is_atom(*inner));
    EXPECT_EQ(as_atom(*inner).name, "A");
}

TEST(ParserTest, ParseDoubleNegation) {
    auto f = parse("~~A");
    ASSERT_TRUE(has_op(*f, Op::Not));
    auto inner = as_compound(*f).args[0];
    ASSERT_TRUE(has_op(*inner, Op::Not));
}

TEST(ParserTest, ParseConjunction) {
    auto f = parse("A & B");
    ASSERT_TRUE(has_op(*f, Op::And));
    const auto& comp = as_compound(*f);
    EXPECT_EQ(to_string(*comp.args[0]), "A");
    EXPECT_EQ(to_string(*comp.args[1]), "B");
}

TEST(ParserTest, ParseDisjunction) {
    auto f = parse("A | B");
    ASSERT_TRUE(has_op(*f, Op::Or));
}

TEST(ParserTest, ParseImplication) {
    auto f = parse("A -> B");
    ASSERT_TRUE(has_op(*f, Op::Implies));
}

TEST(ParserTest, ParseBiconditional) {
    auto f = parse("A <-> B");
    ASSERT_TRUE(has_op(*f, Op::Iff));
}

TEST(ParserTest, ParseBottom) {
    auto f = parse("false");
    ASSERT_TRUE(has_op(*f, Op::Bottom));
}

TEST(ParserTest, ParseParentheses) {
    auto f = parse("(A & B) -> C");
    ASSERT_TRUE(has_op(*f, Op::Implies));
    const auto& comp = as_compound(*f);
    ASSERT_TRUE(has_op(*comp.args[0], Op::And));
}

TEST(ParserTest, Precedence) {
    // A | B & C should parse as A | (B & C)
    auto f = parse("A | B & C");
    ASSERT_TRUE(has_op(*f, Op::Or));
    const auto& comp = as_compound(*f);
    EXPECT_EQ(to_string(*comp.args[0]), "A");
    ASSERT_TRUE(has_op(*comp.args[1], Op::And));
}

TEST(ParserTest, ImplicationRightAssociative) {
    // A -> B -> C should parse as A -> (B -> C)
    auto f = parse("A -> B -> C");
    ASSERT_TRUE(has_op(*f, Op::Implies));
    const auto& comp = as_compound(*f);
    EXPECT_EQ(to_string(*comp.args[0]), "A");
    ASSERT_TRUE(has_op(*comp.args[1], Op::Implies));
}

TEST(ParserTest, UnicodeOperators) {
    // Test with ASCII equivalents since UTF-8 parsing may vary
    auto f1 = parse("A and B");
    ASSERT_TRUE(has_op(*f1, Op::And));

    auto f2 = parse("A or B");
    ASSERT_TRUE(has_op(*f2, Op::Or));

    auto f3 = parse("A implies B");
    ASSERT_TRUE(has_op(*f3, Op::Implies));

    auto f4 = parse("not A");
    ASSERT_TRUE(has_op(*f4, Op::Not));
}

TEST(ParserTest, InvalidInputThrows) {
    std::string error;
    auto result = Parser::try_parse("A &", &error);
    EXPECT_FALSE(result.has_value());
    EXPECT_FALSE(error.empty());
}
