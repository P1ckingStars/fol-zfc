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

%token <str> IDENTIFIER
%token LPAREN RPAREN COMMA DOT
%token AND OR NOT IMPLIES IFF BOTTOM
%token FORALL EXISTS

%type <node> formula iff_formula implies_formula or_formula and_formula
%type <node> unary_formula atom predicate term
%type <node_list> term_list

/* Precedence: lowest to highest */
%left IFF
%right IMPLIES
%left OR
%left AND
%precedence NOT FORALL EXISTS

%destructor { delete $$; } <str>
%destructor { delete $$; } <node>
%destructor { for (auto* n : *$$) delete n; delete $$; } <node_list>

%start input

%%

input
    : formula { ctx->result = $1; }
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
    ;

%%

void yyerror(YYLTYPE* loc, yyscan_t scanner, ParseContext* ctx, const char* msg) {
    (void)scanner;
    ctx->error = msg;
    ctx->error_col = loc->first_column;
}
