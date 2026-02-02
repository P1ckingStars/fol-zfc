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

[a-zA-Z_][a-zA-Z0-9_]*  {
    yylval->str = new std::string(yytext);
    return IDENTIFIER;
}

.               { return yytext[0]; }

%%
