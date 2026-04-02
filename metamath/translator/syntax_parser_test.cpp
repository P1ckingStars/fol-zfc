#include "syntax_parser.h"

#include <iostream>
#include <string>

using namespace metamath;

void print_tree(const SyntaxNode& node, int depth = 0) {
    std::string indent(depth * 2, ' ');
    if (node.is_leaf()) {
        std::cout << indent << node.typecode << " '" << node.token
                  << "' [" << node.label << "]\n";
    } else {
        std::cout << indent << node.typecode << " [" << node.label << "]\n";
        for (const auto& child : node.children) {
            print_tree(child, depth + 1);
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: syntax_parser_test <file.mm>\n";
        return 1;
    }

    std::string err;
    auto db = MmDatabase::parse_file(argv[1], &err);
    if (!db.ok()) {
        std::cerr << "Parse failed: " << err << "\n";
        return 1;
    }

    std::cout << "Building syntax parser from " << db.num_assertions()
              << " assertions...\n";
    SyntaxParser parser(db);

    // Test: parse some known assertion expressions
    const char* test_labels[] = {
        "ax-1",    // |- ( ph -> ( ps -> ph ) )
        "ax7v",    // |- ( x = y -> ( x = z -> y = z ) )
        "cin",     // class ( A i^i B )
        "df-ne",   // |- ( A =/= B <-> -. A = B )
        "df-ss",   // |- ( A C_ B <-> A. x ( x e. A -> x e. B ) )
        "df-in",   // |- ( A i^i B ) = { x | ( x e. A /\ x e. B ) }
    };

    for (const char* label : test_labels) {
        const Assertion* a = db.get_assertion(label);
        if (!a) {
            std::cout << "\n" << label << ": not found\n";
            continue;
        }

        std::cout << "\n=== " << label << " ===\n";
        std::cout << "Expression:";
        for (const auto& tok : a->expression) std::cout << " " << tok;
        std::cout << "\n";

        // For "|-" assertions, parse the wff part (skip "|-")
        if (a->expression[0] == "|-") {
            Expression wff_expr = {"wff"};
            wff_expr.insert(wff_expr.end(),
                            a->expression.begin() + 1, a->expression.end());
            auto result = parser.parse(wff_expr);
            if (result) {
                print_tree(*result);
            } else {
                std::cout << "PARSE FAILED\n";
            }
        } else {
            auto result = parser.parse(a->expression);
            if (result) {
                print_tree(*result);
            } else {
                std::cout << "PARSE FAILED\n";
            }
        }
    }

    // Bulk test: parse all assertion expressions
    std::cout << "\n=== Bulk parse test ===\n";
    int ok_count = 0, fail_count = 0;
    for (const auto& lbl : db.assertion_order()) {
        const Assertion* a = db.get_assertion(lbl);
        if (!a) continue;

        Expression to_parse;
        if (a->expression[0] == "|-") {
            to_parse = {"wff"};
            to_parse.insert(to_parse.end(),
                            a->expression.begin() + 1, a->expression.end());
        } else {
            to_parse = a->expression;
        }

        auto result = parser.parse(to_parse);
        if (result) {
            ++ok_count;
        } else {
            ++fail_count;
            if (fail_count <= 10) {
                std::cout << "FAIL: " << lbl << " :";
                for (const auto& t : to_parse) std::cout << " " << t;
                std::cout << "\n";
            }
        }
    }
    std::cout << "Parsed: " << ok_count << ", Failed: " << fail_count
              << " / " << (ok_count + fail_count) << "\n";

    return 0;
}
