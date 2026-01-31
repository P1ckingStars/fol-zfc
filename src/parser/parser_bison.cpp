#include "parser.h"
#include "formula_ast.h"

// Generated headers
#include "formula_parser.tab.h"
#include "formula_lexer.h"

#include <cctype>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace logic {

// ==================== AST to Formula Conversion ====================

class ASTConverter {
public:
    ASTConverter(GlobalContext& ctx, FormulaBuilder& builder)
        : ctx_(ctx), builder_(builder) {}

    FormulaHandle convert(const ASTNode* node) {
        switch (node->type) {
            case ASTNode::Bottom:
                return builder_.make_bottom();

            case ASTNode::Not:
                return builder_.make_not(convert(node->left));

            case ASTNode::And:
                return builder_.make_and(convert(node->left), convert(node->right));

            case ASTNode::Or:
                return builder_.make_or(convert(node->left), convert(node->right));

            case ASTNode::Implies:
                return builder_.make_implies(convert(node->left), convert(node->right));

            case ASTNode::Iff:
                return builder_.make_iff(convert(node->left), convert(node->right));

            case ASTNode::Forall: {
                FormulaHandle result;
                {
                    QuantifierBuilder qb(builder_, Op::Forall, result);
                    var_scopes_.push_back({node->name, qb.var()});
                    FormulaHandle body = convert(node->body);
                    qb.set_body(body);
                    var_scopes_.pop_back();
                }
                return result;
            }

            case ASTNode::Exists: {
                FormulaHandle result;
                {
                    QuantifierBuilder qb(builder_, Op::Exists, result);
                    var_scopes_.push_back({node->name, qb.var()});
                    FormulaHandle body = convert(node->body);
                    qb.set_body(body);
                    var_scopes_.pop_back();
                }
                return result;
            }

            case ASTNode::Predicate: {
                std::vector<Term> terms;
                if (node->args) {
                    for (const ASTNode* arg : *node->args) {
                        terms.push_back(convert_term(arg));
                    }
                }
                PredicateHandle pred = get_or_create_predicate(node->name, terms.size());
                return builder_.predicate(pred, std::move(terms));
            }

            default:
                throw std::runtime_error("Invalid AST node type for formula");
        }
    }

private:
    Term convert_term(const ASTNode* node) {
        if (node->type != ASTNode::Term) {
            throw std::runtime_error("Expected term node");
        }

        const std::string& name = node->name;

        // First check if it's a bound variable (fixed during construction)
        auto term = lookup_var(name);
        if (term) {
            return *term;
        }

        // Not a bound variable - check if it looks like a variable name
        // Convention: x, y, z, u, v, w (and their variants) are variables
        // Single letters a-t are treated as constants
        if (name.size() == 1) {
            char c = name[0];
            if (c >= 'u' && c <= 'z') {
                throw std::runtime_error("Free variable '" + name + "' not allowed in sentence");
            }
        }

        // Otherwise treat as constant
        return Term::constant(get_or_create_constant(name));
    }

    std::optional<Term> lookup_var(const std::string& name) const {
        // Search from innermost scope outward
        for (auto it = var_scopes_.rbegin(); it != var_scopes_.rend(); ++it) {
            if (it->first == name) {
                return it->second;
            }
        }
        return std::nullopt;
    }

    PredicateHandle get_or_create_predicate(const std::string& name, size_t arity) {
        auto it = predicates_.find(name);
        if (it != predicates_.end()) {
            return it->second;
        }
        PredicateHandle h = ctx_.add_predicate(name, arity);
        predicates_[name] = h;
        return h;
    }

    ConstantHandle get_or_create_constant(const std::string& name) {
        auto it = constants_.find(name);
        if (it != constants_.end()) {
            return it->second;
        }
        ConstantHandle h = ctx_.add_constant(name);
        constants_[name] = h;
        return h;
    }

    GlobalContext& ctx_;
    FormulaBuilder& builder_;
    std::vector<std::pair<std::string, Term>> var_scopes_;
    std::unordered_map<std::string, PredicateHandle> predicates_;
    std::unordered_map<std::string, ConstantHandle> constants_;
};

// ==================== Parser Implementation ====================

SentenceHandle parse_sentence(std::string_view input, GlobalContext& ctx) {
    // Initialize scanner
    yyscan_t scanner;
    if (yylex_init(&scanner) != 0) {
        throw std::runtime_error("Failed to initialize lexer");
    }

    // Set up input buffer
    std::string input_str(input);
    YY_BUFFER_STATE buffer = yy_scan_string(input_str.c_str(), scanner);

    // Parse
    ParseContext parse_ctx;
    yypstate* ps = yypstate_new();

    int status;
    YYSTYPE yylval;
    YYLTYPE yylloc = {1, 1, 1, 1};

    do {
        int token = yylex(&yylval, &yylloc, scanner);
        status = yypush_parse(ps, token, &yylval, &yylloc, scanner, &parse_ctx);
    } while (status == YYPUSH_MORE);

    yypstate_delete(ps);
    yy_delete_buffer(buffer, scanner);
    yylex_destroy(scanner);

    if (status != 0 || !parse_ctx.result) {
        std::ostringstream oss;
        oss << "Parse error at column " << parse_ctx.error_col << ": " << parse_ctx.error;
        oss << "\n  Input: " << input;
        if (parse_ctx.error_col > 0 && parse_ctx.error_col <= static_cast<int>(input.size())) {
            oss << "\n         " << std::string(parse_ctx.error_col - 1, ' ') << "^";
        }
        throw std::runtime_error(oss.str());
    }

    // Convert AST to Formula using FormulaBuilder
    FormulaBuilder builder(ctx);
    ASTConverter converter(ctx, builder);
    FormulaHandle root = converter.convert(parse_ctx.result);

    // Build sentence - this checks for free variables
    SentenceHandle sh = builder.build_sentence(root);
    if (!sh.valid()) {
        throw std::runtime_error("Formula has free variables, not a valid sentence");
    }

    return sh;
}

SentenceHandle try_parse_sentence(std::string_view input, GlobalContext& ctx,
                                   std::string* error) {
    try {
        return parse_sentence(input, ctx);
    } catch (const std::exception& e) {
        if (error) *error = e.what();
        return SentenceHandle{};
    }
}

}  // namespace logic
