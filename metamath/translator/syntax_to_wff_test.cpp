#include "syntax_parser.h"
#include "syntax_to_wff.h"
#include "wff_ast.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>

using namespace metamath;

// Default leaf renderer for testing
std::string test_render(const WffNode& node) {
    switch (node.kind) {
        case WffNode::Kind::Var: return node.name;
        case WffNode::Kind::Literal: return node.name;
        case WffNode::Kind::Pred: return render_pred(node);
        case WffNode::Kind::Verum: return "T";
        case WffNode::Kind::Falsum: return "F";
        default: return "??";
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: syntax_to_wff_test <file.mm>\n";
        return 1;
    }

    std::string err;
    auto db = MmDatabase::parse_file(argv[1], &err);
    if (!db.ok()) {
        std::cerr << "Parse failed: " << err << "\n";
        return 1;
    }

    SyntaxParser parser(db);

    // Build sets of setvars and wff vars from the database
    // (For testing, we'll use the variables from each theorem's frame)
    struct TestCase {
        const char* label;
        const char* description;
    };

    TestCase tests[] = {
        {"ax-1",    "ph -> (ps -> ph)"},
        {"ax7v",    "x=y -> (x=z -> y=z)"},
        {"df-ne",   "A=/=B <-> ~A=B"},
        {"df-ss",   "A C_ B <-> forall x. (x e. A -> x e. B)"},
        {"df-in",   "(A i^i B) = {x | x e. A /\\ x e. B}"},
        {"df-un",   "(A u. B) = {x | x e. A \\/ x e. B}"},
        {"df-dif",  "(A \\ B) = {x | x e. A /\\ ~x e. B}"},
        {"df-uni",  "U. A = {x | E. y (x e. y /\\ y e. A)}"},
        {"df-int",  "|^| A = {x | A. y (y e. A -> x e. y)}"},
        {"df-rab",  "{x e. A | ph} = {x | (x e. A /\\ ph)}"},
        {"elun",    "A e. (B u. C) <-> (A e. B \\/ A e. C)"},
        {"elin",    "A e. (B i^i C) <-> (A e. B /\\ A e. C)"},
        {"eldif",   "A e. (B \\ C) <-> (A e. B /\\ ~A e. C)"},
        {"neirr",   "~A =/= A"},
        {"vex",     "x e. _V"},
    };

    int pass = 0, fail = 0;

    for (const auto& tc : tests) {
        const Assertion* a = db.get_assertion(tc.label);
        if (!a) {
            std::cout << "SKIP " << tc.label << ": not found\n";
            continue;
        }

        // Parse expression into syntax tree
        Expression wff_expr = {"wff"};
        wff_expr.insert(wff_expr.end(),
                        a->expression.begin() + 1, a->expression.end());
        auto syntax = parser.parse(wff_expr);
        if (!syntax) {
            std::cout << "FAIL " << tc.label << ": syntax parse failed\n";
            ++fail;
            continue;
        }

        // Collect setvars and wff vars from frame
        std::unordered_set<std::string> setvars, wff_vars;
        for (size_t i = 0; i < a->frame.hyp_labels.size(); ++i) {
            if (!a->frame.is_floating[i]) continue;
            const FloatingHyp* fh = db.get_float_hyp(a->frame.hyp_labels[i]);
            if (!fh) continue;
            if (fh->typecode == "setvar") setvars.insert(fh->variable);
            else if (fh->typecode == "wff") wff_vars.insert(fh->variable);
            // class vars → treat as setvars for the encoding
            else if (fh->typecode == "class") setvars.insert(fh->variable);
        }

        // Convert to WffPtr
        SyntaxToWff converter;
        WffPtr wff = converter.convert(*syntax, setvars, wff_vars);

        std::string fol = emit_fol(*wff, test_render);

        std::cout << "OK " << tc.label << ": " << fol;
        if (!converter.extra_vars().empty()) {
            std::cout << "  [extra vars:";
            for (const auto& v : converter.extra_vars())
                std::cout << " " << v;
            std::cout << "]";
        }
        std::cout << "\n";
        ++pass;
    }

    std::cout << "\nPassed: " << pass << ", Failed: " << fail << "\n";

    // Bulk test: convert all assertions and count successes
    std::cout << "\n=== Bulk conversion test (first 5000 theorems) ===\n";
    int bulk_ok = 0, bulk_fail = 0, bulk_unsupported = 0;
    std::unordered_map<std::string, int> unsup_tokens;
    int count = 0;
    for (const auto& lbl : db.assertion_order()) {
        const Assertion* a = db.get_assertion(lbl);
        if (!a || a->expression[0] != "|-") continue;
        if (++count > 5000) break;

        Expression wff_expr = {"wff"};
        wff_expr.insert(wff_expr.end(),
                        a->expression.begin() + 1, a->expression.end());
        auto syntax = parser.parse(wff_expr);
        if (!syntax) { ++bulk_fail; continue; }

        std::unordered_set<std::string> setvars, wff_vars;
        for (size_t i = 0; i < a->frame.hyp_labels.size(); ++i) {
            if (!a->frame.is_floating[i]) continue;
            const FloatingHyp* fh = db.get_float_hyp(a->frame.hyp_labels[i]);
            if (!fh) continue;
            if (fh->typecode == "setvar") setvars.insert(fh->variable);
            else if (fh->typecode == "wff") wff_vars.insert(fh->variable);
            else if (fh->typecode == "class") setvars.insert(fh->variable);
        }

        SyntaxToWff converter;
        WffPtr wff = converter.convert(*syntax, setvars, wff_vars);
        std::string fol = emit_fol(*wff, test_render);

        if (fol.find("??") != std::string::npos) {
            ++bulk_unsupported;
            // Extract the ?? markers
            size_t p = 0;
            while ((p = fol.find("??", p)) != std::string::npos) {
                auto p2 = fol.find("??", p + 2);
                if (p2 != std::string::npos) {
                    unsup_tokens[fol.substr(p + 2, p2 - p - 2)]++;
                    p = p2 + 2;
                } else break;
            }
        } else {
            ++bulk_ok;
        }
    }
    std::cout << "Converted: " << bulk_ok << ", Unsupported: " << bulk_unsupported
              << ", Parse fail: " << bulk_fail << " / " << count << "\n";

    // Sort and print unsupported token counts
    std::vector<std::pair<int, std::string>> sorted_tokens;
    for (const auto& [tok, cnt] : unsup_tokens)
        sorted_tokens.push_back({cnt, tok});
    std::sort(sorted_tokens.rbegin(), sorted_tokens.rend());
    std::cout << "\nUnsupported token breakdown:\n";
    for (const auto& [cnt, tok] : sorted_tokens)
        std::cout << "  " << cnt << " " << tok << "\n";

    return 0;
}
