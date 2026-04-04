#include "parser.h"
#include "formula_ast.h"
#include "src/util/profiler.h"

// Generated headers
#include "formula_parser.tab.h"
#include "formula_lexer.h"

#include <cctype>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

extern void reset_lexer_col();

namespace logic {

// ==================== AST Cloning ====================

// Deep clone an AST node (for deferred parsing)
static ASTNode* clone_ast_impl(const ASTNode* node) {
    if (!node) return nullptr;

    ASTNode* cloned = new ASTNode(node->type);
    cloned->name = node->name;
    cloned->rule_name = node->rule_name;
    cloned->def_predicate = node->def_predicate;
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

    // Set schema variable context: name -> positional id
    void set_schema_vars(const std::unordered_map<std::string, size_t>& vars) {
        schema_vars_ = vars;
    }

    // Set schema variable arities: name -> arity (0 = formula, N = N-arg predicate)
    void set_schema_arities(const std::unordered_map<std::string, size_t>& arities) {
        schema_arities_ = arities;
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

            case ASTNode::Forall:
            case ASTNode::Exists: {
                Op op = (node->type == ASTNode::Forall) ? Op::Forall : Op::Exists;
                FormulaHandle result;
                {
                    QuantifierBuilder qb(builder_, op, result);
                    var_scopes_.push_back({node->name, qb.var()});
                    qb.set_body(convert(node->body));
                    var_scopes_.pop_back();
                }
                return result;
            }

            case ASTNode::Predicate: {
                // Check if identifier matches a schema variable
                auto sv_it = schema_vars_.find(node->name);
                if (sv_it != schema_vars_.end()) {
                    size_t arity = 0;
                    auto ar_it = schema_arities_.find(node->name);
                    if (ar_it != schema_arities_.end()) arity = ar_it->second;
                    size_t nargs = (node->args) ? node->args->size() : 0;
                    if (arity == 0 && nargs == 0) {
                        // Arity-0: bare schema variable (formula placeholder)
                        return builder_.make_schema_var(sv_it->second);
                    }
                    if (arity > 0 && nargs == arity) {
                        // Arity-N: schema var applied to N terms
                        std::vector<Term> terms;
                        for (const ASTNode* arg : *node->args)
                            terms.push_back(convert_term(arg));
                        return builder_.make_schema_var(sv_it->second, std::move(terms));
                    }
                    if (arity > 0 && nargs != arity) {
                        throw std::runtime_error(
                            "Schema variable '" + node->name + "' expects " +
                            std::to_string(arity) + " arguments, got " + std::to_string(nargs));
                    }
                    // arity==0 but nargs>0: not a schema var use, fall through to predicate
                }
                std::vector<Term> terms;
                if (node->args) {
                    for (const ASTNode* arg : *node->args)
                        terms.push_back(convert_term(arg));
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
        if (node->type == ASTNode::DescriptionTerm) {
            // Build description term using DescriptionBuilder
            Term result;
            {
                DescriptionBuilder db(builder_, result);
                var_scopes_.push_back({node->name, db.var()});
                FormulaHandle body = convert(node->body);
                db.set_body(body);
                var_scopes_.pop_back();
            }
            return result;
        }

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
        std::string key = name + "/" + std::to_string(arity);
        auto it = predicates_.find(key);
        if (it != predicates_.end()) {
            return it->second;
        }
        PredicateHandle h = ctx_.add_predicate(name, arity);
        predicates_[key] = h;
        return h;
    }

    GlobalContext& ctx_;
    FormulaBuilder& builder_;
    std::vector<std::pair<std::string, Term>> var_scopes_;
    std::unordered_map<std::string, PredicateHandle> predicates_;
    std::unordered_map<std::string, Term> external_vars_;
    std::unordered_map<std::string, size_t> schema_vars_;
    std::unordered_map<std::string, size_t> schema_arities_;
};

// ==================== Parser Implementation ====================

// Forward declaration
static ParseContext run_parser(std::string_view input);

SentenceHandle parse_sentence(std::string_view input, GlobalContext& ctx) {
    ParseContext parse_ctx = run_parser(input);
    if (!parse_ctx.result)
        throw std::runtime_error("Expected a formula, got statements");

    FormulaBuilder builder(ctx);
    ASTConverter converter(ctx, builder);
    FormulaHandle root = converter.convert(parse_ctx.result);

    SentenceHandle sh = builder.build_sentence(root);
    if (!sh.valid())
        throw std::runtime_error("Formula has free variables, not a valid sentence");
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

// ==================== AST Predicate Search ====================

// Check if a predicate name appears anywhere in an AST formula
static bool ast_contains_predicate(const ASTNode* node, const std::string& pred_name) {
    if (!node) return false;
    if (node->type == ASTNode::Predicate && node->name == pred_name) return true;
    if (ast_contains_predicate(node->left, pred_name)) return true;
    if (ast_contains_predicate(node->right, pred_name)) return true;
    if (ast_contains_predicate(node->body, pred_name)) return true;
    if (node->args) {
        for (const auto* arg : *node->args) {
            if (ast_contains_predicate(arg, pred_name)) return true;
        }
    }
    return false;
}

// ==================== Shared Parser Helpers ====================

// Run the Bison push-parser on input and return the resulting ParseContext.
// Throws on parse failure.
static ParseContext run_parser(std::string_view input) {
    reset_lexer_col();

    yyscan_t scanner;
    if (yylex_init(&scanner) != 0) {
        throw std::runtime_error("Failed to initialize lexer");
    }

    std::string input_str(input);
    YY_BUFFER_STATE buffer = yy_scan_string(input_str.c_str(), scanner);

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

    if (status != 0) {
        std::ostringstream oss;
        oss << "Parse error at column " << parse_ctx.error_col << ": " << parse_ctx.error;
        oss << "\n  Input: " << input;
        if (parse_ctx.error_col > 0 && parse_ctx.error_col <= static_cast<int>(input.size())) {
            oss << "\n         " << std::string(parse_ctx.error_col - 1, ' ') << "^";
        }
        throw std::runtime_error(oss.str());
    }

    return parse_ctx;
}

// Register a schema statement in the GlobalContext.
static void process_schema_stmt(const ASTNode* stmt_node, GlobalContext& ctx) {
    std::vector<std::string> var_names;
    std::vector<size_t> var_arities;
    std::unordered_map<std::string, size_t> var_map;
    std::unordered_map<std::string, size_t> arity_map;
    if (stmt_node->args) {
        for (size_t i = 0; i < stmt_node->args->size(); ++i) {
            const auto* vnode = (*stmt_node->args)[i];
            var_names.push_back(vnode->name);
            size_t arity = vnode->rule_name.empty() ? 0 : std::stoul(vnode->rule_name);
            var_arities.push_back(arity);
            var_map[vnode->name] = i;
            arity_map[vnode->name] = arity;
        }
    }
    FormulaBuilder builder(ctx);
    ASTConverter converter(ctx, builder);
    converter.set_schema_vars(var_map);
    converter.set_schema_arities(arity_map);
    FormulaHandle body = converter.convert(stmt_node->body);
    SchemaDefinition def;
    def.body = body;
    def.var_names = std::move(var_names);
    def.var_arities = std::move(var_arities);
    ctx.add_schema(stmt_node->name, std::move(def));
}

// Convert an axiom, claim, or @def AST node to a ParsedStatement and register it.
static ParsedStatement process_axiom_claim_stmt(const ASTNode* stmt_node, GlobalContext& ctx) {
    ParsedStatement stmt;

    switch (stmt_node->type) {
        case ASTNode::AxiomStmt:
            stmt.kind = ParsedStatement::Kind::Axiom;
            break;
        case ASTNode::DefStmt:
            stmt.kind = ParsedStatement::Kind::Axiom;
            stmt.def_predicate = stmt_node->def_predicate;
            break;
        case ASTNode::ClaimStmt:
            stmt.kind = ParsedStatement::Kind::Claim;
            break;
        default:
            throw std::runtime_error("Invalid statement AST node type");
    }

    stmt.name = stmt_node->name;

    // Validate @def annotations before converting formula
    if (!stmt.def_predicate.empty()) {
        if (ctx.is_defined(stmt.def_predicate) &&
            !ctx.is_same_definition(stmt.def_predicate, stmt.name)) {
            throw std::runtime_error("Predicate '" + stmt.def_predicate +
                "' is already defined (in @def axiom '" + stmt.name + "')");
        }
        if (!ast_contains_predicate(stmt_node->body, stmt.def_predicate)) {
            throw std::runtime_error("@def(" + stmt.def_predicate +
                ") axiom '" + stmt.name + "' does not mention predicate '" +
                stmt.def_predicate + "'");
        }
    }

    // Convert the formula body
    FormulaBuilder builder(ctx);
    ASTConverter converter(ctx, builder);
    FormulaHandle root;
    { PROFILE_SCOPE("ast_convert"); root = converter.convert(stmt_node->body); }

    SentenceHandle sh;
    { PROFILE_SCOPE("build_sentence"); sh = builder.build_sentence(root); }
    if (!sh.valid()) {
        throw std::runtime_error("Statement '" + stmt.name + "' formula has free variables");
    }

    stmt.formula = sh;

    // Register statements in GlobalContext
    { PROFILE_SCOPE("register_stmt");
    if (!stmt.def_predicate.empty()) {
        ctx.add_definition(stmt.def_predicate, stmt.name, stmt.formula);
    } else if (stmt.kind == ParsedStatement::Kind::Axiom) {
        ctx.add_axiom(stmt.name, stmt.formula);
    } else if (stmt.kind == ParsedStatement::Kind::Claim) {
        ctx.add_claim(stmt.name, stmt.formula);
    }
    }

    return stmt;
}

// ==================== Statement Parser Implementation ====================

std::vector<ParsedStatement> parse_statements(std::string_view input, GlobalContext& ctx) {
    ParseContext parse_ctx = run_parser(input);

    if (!parse_ctx.statements)
        throw std::runtime_error("Expected statement list, got bare formula");

    std::vector<ParsedStatement> result;
    for (const ASTNode* stmt_node : *parse_ctx.statements) {
        if (stmt_node->type == ASTNode::ProofBlock) {
            continue;
        }
        if (stmt_node->type == ASTNode::SchemaStmt) {
            process_schema_stmt(stmt_node, ctx);
            continue;
        }
        result.push_back(process_axiom_claim_stmt(stmt_node, ctx));
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
static ParsedProofStep convert_proof_step(const ASTNode* step_node) {
    ParsedProofStep step;

    switch (step_node->type) {
        case ASTNode::ProofStepFix:
            step.kind = ParsedProofStep::Kind::Fix;
            step.result_name = step_node->rule_name;
            step.args.push_back(step_node->rule_name);
            break;

        case ASTNode::ProofStepAssume:
            step.kind = ParsedProofStep::Kind::Assume;
            step.result_name = step_node->name;
            if (step_node->body) {
                step.formula_ast = clone_ast(step_node->body);
            }
            break;

        case ASTNode::ProofStepLet:
            step.kind = ParsedProofStep::Kind::Let;
            step.result_name = step_node->name;
            if (step_node->body) {
                step.formula_ast = clone_ast(step_node->body);
            }
            break;

        case ASTNode::ProofStepUse:
            step.kind = ParsedProofStep::Kind::Use;
            step.result_name = step_node->name;
            step.rule_name = step_node->rule_name;
            step.args.push_back(step_node->rule_name);
            break;

        case ASTNode::ProofStepRule:
            step.kind = ParsedProofStep::Kind::Rule;
            step.result_name = step_node->name;
            step.rule_name = step_node->rule_name;
            if (step_node->args) {
                for (const ASTNode* arg : *step_node->args) {
                    step.args.push_back(arg->name);
                }
            }
            break;

        case ASTNode::ProofStepQed:
            step.kind = ParsedProofStep::Kind::Qed;
            step.args.push_back(step_node->rule_name);
            break;

        case ASTNode::ProofStepSchemaInst:
            step.kind = ParsedProofStep::Kind::SchemaInst;
            step.result_name = step_node->name;
            step.rule_name = step_node->rule_name;
            if (step_node->steps) {
                for (const ASTNode* binding : *step_node->steps) {
                    SchemaBinding sb;
                    sb.var_name = binding->name;
                    if (binding->body)
                        sb.formula_ast = clone_ast(binding->body);
                    if (binding->args) {
                        for (const ASTNode* param : *binding->args)
                            sb.lambda_params.push_back(param->name);
                    }
                    step.schema_bindings.push_back(std::move(sb));
                }
            }
            break;

        default:
            throw std::runtime_error("Invalid proof step AST node type");
    }

    return step;
}

// Convert proof block AST to ParsedProof
static ParsedProof convert_proof_block(const ASTNode* proof_node) {
    ParsedProof proof;
    proof.claim_name = proof_node->name;
    proof.unproved = (proof_node->rule_name == "UNPROVED");

    if (proof_node->steps) {
        for (const ASTNode* step_node : *proof_node->steps) {
            proof.steps.push_back(convert_proof_step(step_node));
        }
    }

    return proof;
}

ParseResult parse_with_proofs(std::string_view input, GlobalContext& ctx) {
    ParseContext parse_ctx = ([&]() { PROFILE_SCOPE("parse_bison"); return run_parser(input); })();

    if (!parse_ctx.statements)
        throw std::runtime_error("Expected statement list, got bare formula");

    ParseResult result;
    for (const ASTNode* stmt_node : *parse_ctx.statements) {
        if (stmt_node->type == ASTNode::ProofBlock) {
            PROFILE_SCOPE("convert_proof_block");
            result.proofs.push_back(convert_proof_block(stmt_node));
        } else if (stmt_node->type == ASTNode::SchemaStmt) {
            PROFILE_SCOPE("process_schema");
            process_schema_stmt(stmt_node, ctx);
        } else if (stmt_node->type == ASTNode::IncludeStmt) {
            ParsedInclude inc;
            inc.path = stmt_node->name;
            result.includes.push_back(std::move(inc));
        } else {
            PROFILE_SCOPE("process_axiom_claim");
            result.statements.push_back(process_axiom_claim_stmt(stmt_node, ctx));
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
    const std::unordered_map<std::string, Term>& external_vars,
    const std::unordered_map<std::string, size_t>& schema_vars,
    const std::unordered_map<std::string, size_t>& schema_arities) {

    ASTConverter converter(ctx, builder);
    converter.set_external_vars(external_vars);
    if (!schema_vars.empty()) {
        converter.set_schema_vars(schema_vars);
        converter.set_schema_arities(schema_arities);
    }
    return converter.convert(ast);
}

}  // namespace logic
