/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

#ifndef YY_YY_FORMULA_PARSER_TAB_H_INCLUDED
# define YY_YY_FORMULA_PARSER_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif
/* "%code requires" blocks.  */
#line 1 "formula.y"

#include <string>
#include <vector>
#include "formula_ast.h"

typedef void* yyscan_t;

#line 57 "formula_parser.tab.h"

/* Token kinds.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    YYEMPTY = -2,
    YYEOF = 0,                     /* "end of file"  */
    YYerror = 256,                 /* error  */
    YYUNDEF = 257,                 /* "invalid token"  */
    IDENTIFIER = 258,              /* IDENTIFIER  */
    STRING_LITERAL = 259,          /* STRING_LITERAL  */
    LPAREN = 260,                  /* LPAREN  */
    RPAREN = 261,                  /* RPAREN  */
    COMMA = 262,                   /* COMMA  */
    DOT = 263,                     /* DOT  */
    COLON = 264,                   /* COLON  */
    EQUALS = 265,                  /* EQUALS  */
    AND = 266,                     /* AND  */
    OR = 267,                      /* OR  */
    NOT = 268,                     /* NOT  */
    IMPLIES = 269,                 /* IMPLIES  */
    IFF = 270,                     /* IFF  */
    BOTTOM = 271,                  /* BOTTOM  */
    FORALL = 272,                  /* FORALL  */
    EXISTS = 273,                  /* EXISTS  */
    AXIOM = 274,                   /* AXIOM  */
    CLAIM = 275,                   /* CLAIM  */
    PROOF = 276,                   /* PROOF  */
    INCLUDE = 277,                 /* INCLUDE  */
    DEF = 278,                     /* DEF  */
    FIX = 279,                     /* FIX  */
    ASSUME = 280,                  /* ASSUME  */
    QED = 281,                     /* QED  */
    USE = 282,                     /* USE  */
    LET = 283,                     /* LET  */
    AND_INTRO = 284,               /* AND_INTRO  */
    AND_ELIM_L = 285,              /* AND_ELIM_L  */
    AND_ELIM_R = 286,              /* AND_ELIM_R  */
    OR_INTRO_L = 287,              /* OR_INTRO_L  */
    OR_INTRO_R = 288,              /* OR_INTRO_R  */
    OR_ELIM = 289,                 /* OR_ELIM  */
    IMPLIES_INTRO = 290,           /* IMPLIES_INTRO  */
    IMPLIES_ELIM = 291,            /* IMPLIES_ELIM  */
    NOT_INTRO = 292,               /* NOT_INTRO  */
    NOT_ELIM = 293,                /* NOT_ELIM  */
    BOTTOM_ELIM = 294,             /* BOTTOM_ELIM  */
    IFF_INTRO = 295,               /* IFF_INTRO  */
    IFF_ELIM_L = 296,              /* IFF_ELIM_L  */
    IFF_ELIM_R = 297,              /* IFF_ELIM_R  */
    FORALL_INTRO = 298,            /* FORALL_INTRO  */
    FORALL_ELIM = 299,             /* FORALL_ELIM  */
    EXISTS_INTRO = 300,            /* EXISTS_INTRO  */
    EXISTS_ELIM = 301,             /* EXISTS_ELIM  */
    DOUBLE_NEG_ELIM = 302,         /* DOUBLE_NEG_ELIM  */
    EXCLUDED_MIDDLE = 303,         /* EXCLUDED_MIDDLE  */
    EQ_SUBST = 304,                /* EQ_SUBST  */
    EQ_SYM = 305,                  /* EQ_SYM  */
    IOTA_ELIM = 306,               /* IOTA_ELIM  */
    IOTA = 307,                    /* IOTA  */
    UNPROVED = 308,                /* UNPROVED  */
    SCHEMA = 309,                  /* SCHEMA  */
    SCHEMA_INST = 310,             /* SCHEMA_INST  */
    LBRACKET = 311,                /* LBRACKET  */
    RBRACKET = 312,                /* RBRACKET  */
    LBRACE = 313,                  /* LBRACE  */
    RBRACE = 314,                  /* RBRACE  */
    BACKSLASH = 315,               /* BACKSLASH  */
    NUMBER = 316                   /* NUMBER  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 29 "formula.y"

    std::string* str;
    ASTNode* node;
    std::vector<ASTNode*>* node_list;

#line 141 "formula_parser.tab.h"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif

/* Location type.  */
#if ! defined YYLTYPE && ! defined YYLTYPE_IS_DECLARED
typedef struct YYLTYPE YYLTYPE;
struct YYLTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
};
# define YYLTYPE_IS_DECLARED 1
# define YYLTYPE_IS_TRIVIAL 1
#endif




#ifndef YYPUSH_MORE_DEFINED
# define YYPUSH_MORE_DEFINED
enum { YYPUSH_MORE = 4 };
#endif

typedef struct yypstate yypstate;


int yypush_parse (yypstate *ps,
                  int pushed_char, YYSTYPE const *pushed_val, YYLTYPE *pushed_loc, yyscan_t scanner, ParseContext* ctx);

yypstate *yypstate_new (void);
void yypstate_delete (yypstate *ps);

/* "%code provides" blocks.  */
#line 9 "formula.y"

void yyerror(YYLTYPE* loc, yyscan_t scanner, ParseContext* ctx, const char* msg);

#line 185 "formula_parser.tab.h"

#endif /* !YY_YY_FORMULA_PARSER_TAB_H_INCLUDED  */
