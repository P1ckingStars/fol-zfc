%{
#include "formula_parser.tab.h"
#include <string>

// Track position for error reporting
static int col = 1;
#define YY_USER_ACTION yylloc->first_column = col; col += yyleng; yylloc->last_column = col;

%}

%option noyywrap
%option reentrant
%option bison-bridge
%option bison-locations
%option nounput
%option noinput

%%

[ \t\r]+        { /* skip whitespace */ }
\n              { col = 1; }
#[^\n]*         { /* skip comments */ }

"("             { return LPAREN; }
")"             { return RPAREN; }
","             { return COMMA; }
"."             { return DOT; }

"&"             { return AND; }
"|"             { return OR; }
"~"             { return NOT; }
"->"            { return IMPLIES; }
"<->"           { return IFF; }
"_|_"           { return BOTTOM; }

"forall"        { return FORALL; }
"exists"        { return EXISTS; }
"and"           { return AND; }
"or"            { return OR; }
"not"           { return NOT; }
"implies"       { return IMPLIES; }
"iff"           { return IFF; }
"false"         { return BOTTOM; }
"bottom"        { return BOTTOM; }
"axiom"         { return AXIOM; }
"claim"         { return CLAIM; }
"theorem"       { return CLAIM; }
":"             { return COLON; }
"="             { return EQUALS; }

"proof"         { return PROOF; }
"fix"           { return FIX; }
"assume"        { return ASSUME; }
"let"           { return LET; }
"qed"           { return QED; }
"use"           { return USE; }
"and_intro"     { return AND_INTRO; }
"and_elim_l"    { return AND_ELIM_L; }
"and_elim_r"    { return AND_ELIM_R; }
"or_intro_l"    { return OR_INTRO_L; }
"or_intro_r"    { return OR_INTRO_R; }
"or_elim"       { return OR_ELIM; }
"implies_intro" { return IMPLIES_INTRO; }
"implies_elim"  { return IMPLIES_ELIM; }
"not_intro"     { return NOT_INTRO; }
"not_elim"      { return NOT_ELIM; }
"bottom_elim"   { return BOTTOM_ELIM; }
"iff_intro"     { return IFF_INTRO; }
"iff_elim_l"    { return IFF_ELIM_L; }
"iff_elim_r"    { return IFF_ELIM_R; }
"forall_intro"  { return FORALL_INTRO; }
"forall_elim"   { return FORALL_ELIM; }
"exists_intro"  { return EXISTS_INTRO; }
"exists_elim"   { return EXISTS_ELIM; }
"double_neg_elim" { return DOUBLE_NEG_ELIM; }
"excluded_middle" { return EXCLUDED_MIDDLE; }
"eq_subst"      { return EQ_SUBST; }
"iota_elim"     { return IOTA_ELIM; }
"iota"          { return IOTA; }
"include"       { return INCLUDE; }
"@def"          { return DEF; }
"UNPROVED"      { return UNPROVED; }
"schema"        { return SCHEMA; }
"schema_inst"   { return SCHEMA_INST; }
"["             { return LBRACKET; }
"]"             { return RBRACKET; }
"{"             { return LBRACE; }
"}"             { return RBRACE; }

\"[^"]*\"       {
    // String literal - strip quotes
    yylval->str = new std::string(yytext + 1, yyleng - 2);
    return STRING_LITERAL;
}

[a-zA-Z_][a-zA-Z0-9_]*  {
    yylval->str = new std::string(yytext);
    return IDENTIFIER;
}

.               { return yytext[0]; }

%%
