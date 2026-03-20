%code requires {
#include <string>
#include <vector>
#include "formula_ast.h"

typedef void* yyscan_t;
}

%code provides {
void yyerror(YYLTYPE* loc, yyscan_t scanner, ParseContext* ctx, const char* msg);
}

%{
#include <string>
#include <vector>
#include <memory>
#include "formula_ast.h"
%}

%define api.pure full
%define api.push-pull push
%define parse.error verbose
%locations

%lex-param { yyscan_t scanner }
%parse-param { yyscan_t scanner }
%parse-param { ParseContext* ctx }

%union {
    std::string* str;
    ASTNode* node;
    std::vector<ASTNode*>* node_list;
}

%token <str> IDENTIFIER STRING_LITERAL
%token LPAREN RPAREN COMMA DOT COLON EQUALS
%token AND OR NOT IMPLIES IFF BOTTOM
%token FORALL EXISTS
%token AXIOM CLAIM PROOF INCLUDE DEF
%token FIX ASSUME QED USE LET
%token AND_INTRO AND_ELIM_L AND_ELIM_R
%token OR_INTRO_L OR_INTRO_R OR_ELIM
%token IMPLIES_INTRO IMPLIES_ELIM
%token NOT_INTRO NOT_ELIM BOTTOM_ELIM
%token IFF_INTRO IFF_ELIM_L IFF_ELIM_R
%token FORALL_INTRO FORALL_ELIM EXISTS_INTRO EXISTS_ELIM
%token DOUBLE_NEG_ELIM EXCLUDED_MIDDLE EQ_SUBST
%token IOTA_ELIM IOTA
%token UNPROVED
%token SCHEMA SCHEMA_INST LBRACKET RBRACKET LBRACE RBRACE
%token BACKSLASH
%token <str> NUMBER

%type <node> formula iff_formula implies_formula or_formula and_formula
%type <node> unary_formula atom predicate term
%type <node> statement proof_block proof_step rule_call
%type <node> schema_binding schema_var_decl
%type <node_list> term_list statement_list proof_step_list id_list unproved_deps
%type <node_list> schema_binding_list schema_var_list lambda_params

/* Precedence: lowest to highest */
%left IFF
%right IMPLIES
%left OR
%left AND
%precedence NOT FORALL EXISTS

%destructor { delete $$; } <str>
%destructor { delete $$; } schema_var_decl
%destructor { delete $$; } <node>
%destructor { for (auto* n : *$$) delete n; delete $$; } <node_list>

%start input

%%

input
    : formula { ctx->result = $1; }
    | statement_list { ctx->statements = $1; }
    ;

statement_list
    : statement {
        $$ = new std::vector<ASTNode*>();
        $$->push_back($1);
    }
    | statement_list statement {
        $$ = $1;
        $$->push_back($2);
    }
    ;

statement
    : AXIOM IDENTIFIER COLON formula {
        $$ = ASTNode::make_statement(ASTNode::AxiomStmt, *$2, $4);
        delete $2;
    }
    | DEF LPAREN IDENTIFIER RPAREN AXIOM IDENTIFIER COLON formula {
        $$ = ASTNode::make_def_statement(*$3, *$6, $8);
        delete $3; delete $6;
    }
    | CLAIM IDENTIFIER COLON formula {
        $$ = ASTNode::make_statement(ASTNode::ClaimStmt, *$2, $4);
        delete $2;
    }
    | SCHEMA IDENTIFIER LBRACKET schema_var_list RBRACKET COLON formula {
        $$ = ASTNode::make_schema_stmt(*$2, $4, $7);
        delete $2;
    }
    | proof_block { $$ = $1; }
    | INCLUDE STRING_LITERAL {
        $$ = ASTNode::make_include(*$2);
        delete $2;
    }
    ;

formula
    : iff_formula { $$ = $1; }
    ;

iff_formula
    : iff_formula IFF implies_formula {
        $$ = new ASTNode(ASTNode::Iff, $1, $3);
    }
    | implies_formula { $$ = $1; }
    ;

implies_formula
    : or_formula IMPLIES implies_formula {
        $$ = new ASTNode(ASTNode::Implies, $1, $3);
    }
    | or_formula { $$ = $1; }
    ;

or_formula
    : or_formula OR and_formula {
        $$ = new ASTNode(ASTNode::Or, $1, $3);
    }
    | and_formula { $$ = $1; }
    ;

and_formula
    : and_formula AND unary_formula {
        $$ = new ASTNode(ASTNode::And, $1, $3);
    }
    | unary_formula { $$ = $1; }
    ;

unary_formula
    : NOT unary_formula {
        $$ = new ASTNode(ASTNode::Not, $2);
    }
    | FORALL IDENTIFIER DOT formula {
        $$ = new ASTNode(ASTNode::Forall, *$2, $4);
        delete $2;
    }
    | EXISTS IDENTIFIER DOT formula {
        $$ = new ASTNode(ASTNode::Exists, *$2, $4);
        delete $2;
    }
    | atom { $$ = $1; }
    ;

atom
    : BOTTOM {
        $$ = new ASTNode(ASTNode::Bottom);
    }
    | LPAREN formula RPAREN {
        $$ = $2;
    }
    | predicate { $$ = $1; }
    ;

predicate
    : IDENTIFIER LPAREN term_list RPAREN {
        $$ = new ASTNode(ASTNode::Predicate, *$1, $3);
        delete $1;
    }
    | IDENTIFIER LPAREN RPAREN {
        $$ = new ASTNode(ASTNode::Predicate, *$1, new std::vector<ASTNode*>());
        delete $1;
    }
    | IDENTIFIER {
        $$ = new ASTNode(ASTNode::Predicate, *$1, new std::vector<ASTNode*>());
        delete $1;
    }
    ;

term_list
    : term {
        $$ = new std::vector<ASTNode*>();
        $$->push_back($1);
    }
    | term_list COMMA term {
        $$ = $1;
        $$->push_back($3);
    }
    ;

term
    : IDENTIFIER {
        $$ = new ASTNode(ASTNode::Term, *$1);
        delete $1;
    }
    | LPAREN IOTA IDENTIFIER DOT formula RPAREN {
        /* Parenthesized iota term: (iota x. φ) */
        $$ = new ASTNode(ASTNode::DescriptionTerm, *$3, $5);
        delete $3;
    }
    ;

/* Proof blocks */
proof_block
    : PROOF IDENTIFIER COLON proof_step_list {
        $$ = ASTNode::make_proof_block(*$2, $4);
        delete $2;
    }
    | PROOF IDENTIFIER COLON UNPROVED unproved_deps {
        $$ = ASTNode::make_proof_block(*$2, $5);
        $$->rule_name = "UNPROVED";
        delete $2;
    }
    ;

unproved_deps
    : /* empty */ { $$ = new std::vector<ASTNode*>(); }
    | unproved_deps USE IDENTIFIER {
        $$ = $1;
        $$->push_back(ASTNode::make_proof_step(ASTNode::ProofStepUse, "", *$3, nullptr, nullptr));
        delete $3;
    }
    ;

proof_step_list
    : proof_step {
        $$ = new std::vector<ASTNode*>();
        $$->push_back($1);
    }
    | proof_step_list proof_step {
        $$ = $1;
        $$->push_back($2);
    }
    ;

proof_step
    : FIX IDENTIFIER {
        $$ = ASTNode::make_proof_step(ASTNode::ProofStepFix, "", *$2, nullptr, nullptr);
        delete $2;
    }
    | IDENTIFIER EQUALS ASSUME formula {
        $$ = ASTNode::make_proof_step(ASTNode::ProofStepAssume, *$1, "", $4, nullptr);
        delete $1;
    }
    | IDENTIFIER EQUALS LET formula {
        $$ = ASTNode::make_proof_step(ASTNode::ProofStepLet, *$1, "", $4, nullptr);
        delete $1;
    }
    | IDENTIFIER EQUALS USE IDENTIFIER {
        $$ = ASTNode::make_proof_step(ASTNode::ProofStepUse, *$1, *$4, nullptr, nullptr);
        delete $1; delete $4;
    }
    | IDENTIFIER EQUALS rule_call {
        $3->name = *$1;  /* Set result name on rule node */
        $$ = $3;
        delete $1;
    }
    | QED IDENTIFIER {
        $$ = ASTNode::make_proof_step(ASTNode::ProofStepQed, "", *$2, nullptr, nullptr);
        delete $2;
    }
    ;

rule_call
    : AND_INTRO id_list {
        $$ = ASTNode::make_rule_step("and_intro", $2);
    }
    | AND_ELIM_L IDENTIFIER {
        auto* args = new std::vector<ASTNode*>();
        args->push_back(new ASTNode(ASTNode::Term, *$2));
        $$ = ASTNode::make_rule_step("and_elim_l", args);
        delete $2;
    }
    | AND_ELIM_R IDENTIFIER {
        auto* args = new std::vector<ASTNode*>();
        args->push_back(new ASTNode(ASTNode::Term, *$2));
        $$ = ASTNode::make_rule_step("and_elim_r", args);
        delete $2;
    }
    | OR_INTRO_L id_list {
        $$ = ASTNode::make_rule_step("or_intro_l", $2);
    }
    | OR_INTRO_R id_list {
        $$ = ASTNode::make_rule_step("or_intro_r", $2);
    }
    | OR_ELIM id_list {
        $$ = ASTNode::make_rule_step("or_elim", $2);
    }
    | IMPLIES_INTRO IDENTIFIER {
        auto* args = new std::vector<ASTNode*>();
        args->push_back(new ASTNode(ASTNode::Term, *$2));
        $$ = ASTNode::make_rule_step("implies_intro", args);
        delete $2;
    }
    | IMPLIES_ELIM id_list {
        $$ = ASTNode::make_rule_step("implies_elim", $2);
    }
    | NOT_INTRO IDENTIFIER {
        auto* args = new std::vector<ASTNode*>();
        args->push_back(new ASTNode(ASTNode::Term, *$2));
        $$ = ASTNode::make_rule_step("not_intro", args);
        delete $2;
    }
    | NOT_ELIM id_list {
        $$ = ASTNode::make_rule_step("not_elim", $2);
    }
    | BOTTOM_ELIM id_list {
        $$ = ASTNode::make_rule_step("bottom_elim", $2);
    }
    | IFF_INTRO id_list {
        $$ = ASTNode::make_rule_step("iff_intro", $2);
    }
    | IFF_ELIM_L id_list {
        $$ = ASTNode::make_rule_step("iff_elim_l", $2);
    }
    | IFF_ELIM_R id_list {
        $$ = ASTNode::make_rule_step("iff_elim_r", $2);
    }
    | FORALL_INTRO IDENTIFIER {
        auto* args = new std::vector<ASTNode*>();
        args->push_back(new ASTNode(ASTNode::Term, *$2));
        $$ = ASTNode::make_rule_step("forall_intro", args);
        delete $2;
    }
    | FORALL_ELIM id_list {
        $$ = ASTNode::make_rule_step("forall_elim", $2);
    }
    | EXISTS_INTRO id_list {
        $$ = ASTNode::make_rule_step("exists_intro", $2);
    }
    | EXISTS_ELIM id_list {
        $$ = ASTNode::make_rule_step("exists_elim", $2);
    }
    | DOUBLE_NEG_ELIM IDENTIFIER {
        auto* args = new std::vector<ASTNode*>();
        args->push_back(new ASTNode(ASTNode::Term, *$2));
        $$ = ASTNode::make_rule_step("double_neg_elim", args);
        delete $2;
    }
    | EXCLUDED_MIDDLE IDENTIFIER {
        auto* args = new std::vector<ASTNode*>();
        args->push_back(new ASTNode(ASTNode::Term, *$2));
        $$ = ASTNode::make_rule_step("excluded_middle", args);
        delete $2;
    }
    | EQ_SUBST id_list {
        $$ = ASTNode::make_rule_step("eq_subst", $2);
    }
    | IOTA_ELIM id_list {
        $$ = ASTNode::make_rule_step("iota_elim", $2);
    }
    | SCHEMA_INST IDENTIFIER LBRACE schema_binding_list RBRACE {
        $$ = ASTNode::make_schema_inst_step(*$2, $4);
        delete $2;
    }
    ;

id_list
    : IDENTIFIER {
        $$ = new std::vector<ASTNode*>();
        $$->push_back(new ASTNode(ASTNode::Term, *$1));
        delete $1;
    }
    | id_list COMMA IDENTIFIER {
        $$ = $1;
        $$->push_back(new ASTNode(ASTNode::Term, *$3));
        delete $3;
    }
    ;

schema_binding_list
    : schema_binding {
        $$ = new std::vector<ASTNode*>();
        $$->push_back($1);
    }
    | schema_binding_list COMMA schema_binding {
        $$ = $1;
        $$->push_back($3);
    }
    ;

schema_var_list
    : schema_var_decl {
        $$ = new std::vector<ASTNode*>();
        $$->push_back($1);
    }
    | schema_var_list COMMA schema_var_decl {
        $$ = $1;
        $$->push_back($3);
    }
    ;

schema_var_decl
    : IDENTIFIER {
        /* Arity 0 (formula-level, backward compatible) */
        $$ = new ASTNode(ASTNode::Term, *$1);
        $$->rule_name = "0";
        delete $1;
    }
    | IDENTIFIER LPAREN NUMBER RPAREN {
        /* Arity N (predicate-level): P(1), R(2), etc. */
        $$ = new ASTNode(ASTNode::Term, *$1);
        $$->rule_name = *$3;
        delete $1; delete $3;
    }
    ;

schema_binding
    : IDENTIFIER COLON formula {
        /* Arity-0 binding: var_name: formula */
        $$ = new ASTNode(ASTNode::Term, *$1);
        $$->body = $3;
        delete $1;
    }
    | IDENTIFIER COLON BACKSLASH lambda_params DOT formula {
        /* Arity-N binding: var_name: \x y. formula */
        $$ = new ASTNode(ASTNode::Term, *$1);
        $$->body = $6;
        $$->args = $4;
        delete $1;
    }
    ;

lambda_params
    : IDENTIFIER {
        $$ = new std::vector<ASTNode*>();
        $$->push_back(new ASTNode(ASTNode::Term, *$1));
        delete $1;
    }
    | lambda_params IDENTIFIER {
        $$ = $1;
        $$->push_back(new ASTNode(ASTNode::Term, *$2));
        delete $2;
    }
    ;

%%

void yyerror(YYLTYPE* loc, yyscan_t scanner, ParseContext* ctx, const char* msg) {
    (void)scanner;
    ctx->error = msg;
    ctx->error_col = loc->first_column;
}
