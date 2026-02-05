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

// ==================== AST Cloning ====================

// Deep clone an AST node (for deferred parsing)
static ASTNode* clone_ast_impl(const ASTNode* node) {
    if (!node) return nullptr;

    ASTNode* cloned = new ASTNode(node->type);
    cloned->name = node->name;
    cloned->rule_name = node->rule_name;
    cloned->left = clone_ast_impl(node->left);
    cloned->right = clone_ast_impl(node->right);
    cloned->body = clone_ast_impl(node->body);

    if (node->args) {
        cloned->args = new std::vector<ASTNode*>();
        for (const auto* arg : *node->args) {
            cloned->args->push_back(clone_ast_impl(arg));
        }
    }
    if (node->steps) {
        cloned->steps = new std::vector<ASTNode*>();
        for (const auto* step : *node->steps) {
            cloned->steps->push_back(clone_ast_impl(step));
        }
    }

    return cloned;
}

// Wrap in shared_ptr for automatic cleanup
static std::shared_ptr<ASTNode> clone_ast(const ASTNode* node) {
    return std::shared_ptr<ASTNode>(clone_ast_impl(node));
}

// ==================== AST to Formula Conversion ====================

class ASTConverter {
public:
    ASTConverter(GlobalContext& ctx, FormulaBuilder& builder)
        : ctx_(ctx), builder_(builder) {}

    // Set external variables (e.g., fixed variables in proofs)
    void set_external_vars(const std::unordered_map<std::string, Term>& vars) {
        external_vars_ = vars;
    }

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

        // Check if it's a bound variable (from quantifier scope)
        auto term = lookup_var(name);
        if (term) {
            return *term;
        }

        // Check if it's an external variable (e.g., fixed variable in proof)
        auto ext_it = external_vars_.find(name);
        if (ext_it != external_vars_.end()) {
            return ext_it->second;
        }

        // Not a bound variable - this is a free variable, which is not allowed
        // All variables in axioms/claims must be bound by quantifiers
        throw std::runtime_error("Free variable '" + name + "' not allowed in sentence. "
                                 "All variables must be bound by quantifiers (forall/exists).");
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

    GlobalContext& ctx_;
    FormulaBuilder& builder_;
    std::vector<std::pair<std::string, Term>> var_scopes_;
    std::unordered_map<std::string, PredicateHandle> predicates_;
    std::unordered_map<std::string, Term> external_vars_;
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

// ==================== Statement Parser Implementation ====================

std::vector<ParsedStatement> parse_statements(std::string_view input, GlobalContext& ctx) {
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

    if (status != 0 || !parse_ctx.statements) {
        std::ostringstream oss;
        oss << "Parse error at column " << parse_ctx.error_col << ": " << parse_ctx.error;
        oss << "\n  Input: " << input;
        if (parse_ctx.error_col > 0 && parse_ctx.error_col <= static_cast<int>(input.size())) {
            oss << "\n         " << std::string(parse_ctx.error_col - 1, ' ') << "^";
        }
        throw std::runtime_error(oss.str());
    }

    // Convert statement AST nodes to ParsedStatements
    std::vector<ParsedStatement> result;
    for (const ASTNode* stmt_node : *parse_ctx.statements) {
        // Skip proof blocks in this function (use parse_with_proofs for proofs)
        if (stmt_node->type == ASTNode::ProofBlock) {
            continue;
        }

        ParsedStatement stmt;

        // Determine statement kind
        switch (stmt_node->type) {
            case ASTNode::AxiomStmt:
                stmt.kind = ParsedStatement::Kind::Axiom;
                break;
            case ASTNode::ClaimStmt:
                stmt.kind = ParsedStatement::Kind::Claim;
                break;
            default:
                throw std::runtime_error("Invalid statement AST node type");
        }

        stmt.name = stmt_node->name;

        // Convert the formula body
        FormulaBuilder builder(ctx);
        ASTConverter converter(ctx, builder);
        FormulaHandle root = converter.convert(stmt_node->body);

        // Build sentence - this checks for free variables
        SentenceHandle sh = builder.build_sentence(root);
        if (!sh.valid()) {
            throw std::runtime_error("Statement '" + stmt.name + "' formula has free variables");
        }

        stmt.formula = sh;

        // Register statements in GlobalContext
        if (stmt.kind == ParsedStatement::Kind::Axiom) {
            ctx.add_axiom(stmt.name, stmt.formula);
        } else if (stmt.kind == ParsedStatement::Kind::Claim) {
            ctx.add_claim(stmt.name, stmt.formula);
        }

        result.push_back(std::move(stmt));
    }

    return result;
}

std::vector<ParsedStatement> try_parse_statements(std::string_view input, GlobalContext& ctx,
                                                   std::string* error) {
    try {
        return parse_statements(input, ctx);
    } catch (const std::exception& e) {
        if (error) *error = e.what();
        return {};
    }
}

// ==================== Proof Parser Implementation ====================

// Helper to convert proof step AST to ParsedProofStep
ParsedProofStep convert_proof_step(const ASTNode* step_node, GlobalContext& ctx) {
    ParsedProofStep step;

    switch (step_node->type) {
        case ASTNode::ProofStepFix:
            step.kind = ParsedProofStep::Kind::Fix;
            step.result_name = step_node->rule_name;  // Variable name stored in rule_name
            step.args.push_back(step_node->rule_name);
            break;

        case ASTNode::ProofStepAssume:
            step.kind = ParsedProofStep::Kind::Assume;
            step.result_name = step_node->name;
            // Store AST for deferred parsing (will be parsed during execution with fixed vars)
            if (step_node->body) {
                step.formula_ast = clone_ast(step_node->body);
            }
            break;

        case ASTNode::ProofStepLet:
            step.kind = ParsedProofStep::Kind::Let;
            step.result_name = step_node->name;
            // Store AST for deferred parsing (will be parsed during execution with fixed vars)
            if (step_node->body) {
                step.formula_ast = clone_ast(step_node->body);
            }
            break;

        case ASTNode::ProofStepUse:
            step.kind = ParsedProofStep::Kind::Use;
            step.result_name = step_node->name;
            step.rule_name = step_node->rule_name;  // Axiom/theorem name
            step.args.push_back(step_node->rule_name);
            break;

        case ASTNode::ProofStepRule:
            step.kind = ParsedProofStep::Kind::Rule;
            step.result_name = step_node->name;
            step.rule_name = step_node->rule_name;
            // Convert args (Term nodes with names)
            if (step_node->args) {
                for (const ASTNode* arg : *step_node->args) {
                    step.args.push_back(arg->name);
                }
            }
            break;

        case ASTNode::ProofStepQed:
            step.kind = ParsedProofStep::Kind::Qed;
            step.args.push_back(step_node->rule_name);  // Handle name
            break;

        default:
            throw std::runtime_error("Invalid proof step AST node type");
    }

    return step;
}

// Convert proof block AST to ParsedProof
ParsedProof convert_proof_block(const ASTNode* proof_node, GlobalContext& ctx) {
    ParsedProof proof;
    proof.claim_name = proof_node->name;

    if (proof_node->steps) {
        for (const ASTNode* step_node : *proof_node->steps) {
            proof.steps.push_back(convert_proof_step(step_node, ctx));
        }
    }

    return proof;
}

ParseResult parse_with_proofs(std::string_view input, GlobalContext& ctx) {
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

    if (status != 0 || !parse_ctx.statements) {
        std::ostringstream oss;
        oss << "Parse error at column " << parse_ctx.error_col << ": " << parse_ctx.error;
        oss << "\n  Input: " << input;
        if (parse_ctx.error_col > 0 && parse_ctx.error_col <= static_cast<int>(input.size())) {
            oss << "\n         " << std::string(parse_ctx.error_col - 1, ' ') << "^";
        }
        throw std::runtime_error(oss.str());
    }

    // Convert statement AST nodes
    ParseResult result;
    for (const ASTNode* stmt_node : *parse_ctx.statements) {
        if (stmt_node->type == ASTNode::ProofBlock) {
            // Convert proof block
            result.proofs.push_back(convert_proof_block(stmt_node, ctx));
        } else if (stmt_node->type == ASTNode::IncludeStmt) {
            // Convert include statement
            ParsedInclude inc;
            inc.path = stmt_node->name;
            result.includes.push_back(std::move(inc));
        } else {
            // Convert axiom/claim statement
            ParsedStatement stmt;

            switch (stmt_node->type) {
                case ASTNode::AxiomStmt:
                    stmt.kind = ParsedStatement::Kind::Axiom;
                    break;
                case ASTNode::ClaimStmt:
                    stmt.kind = ParsedStatement::Kind::Claim;
                    break;
                default:
                    throw std::runtime_error("Invalid statement AST node type");
            }

            stmt.name = stmt_node->name;

            // Convert the formula body
            FormulaBuilder builder(ctx);
            ASTConverter converter(ctx, builder);
            FormulaHandle root = converter.convert(stmt_node->body);

            SentenceHandle sh = builder.build_sentence(root);
            if (!sh.valid()) {
                throw std::runtime_error("Statement '" + stmt.name + "' formula has free variables");
            }

            stmt.formula = sh;

            // Register statements in GlobalContext
            if (stmt.kind == ParsedStatement::Kind::Axiom) {
                ctx.add_axiom(stmt.name, stmt.formula);
            } else if (stmt.kind == ParsedStatement::Kind::Claim) {
                ctx.add_claim(stmt.name, stmt.formula);
            }

            result.statements.push_back(std::move(stmt));
        }
    }

    return result;
}

ParseResult try_parse_with_proofs(std::string_view input, GlobalContext& ctx,
                                   std::string* error) {
    try {
        return parse_with_proofs(input, ctx);
    } catch (const std::exception& e) {
        if (error) *error = e.what();
        return {};
    }
}

// ==================== Parse Formula with External Variables ====================

FormulaHandle parse_formula_with_vars(
    const ASTNode* ast,
    GlobalContext& ctx,
    FormulaBuilder& builder,
    const std::unordered_map<std::string, Term>& external_vars) {

    ASTConverter converter(ctx, builder);
    converter.set_external_vars(external_vars);
    return converter.convert(ast);
}

}  // namespace logic
