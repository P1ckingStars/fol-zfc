/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison implementation for Yacc-like parsers in C

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

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 2

/* Push parsers.  */
#define YYPUSH 1

/* Pull parsers.  */
#define YYPULL 0




/* First part of user prologue.  */
#line 13 "src/parser/formula.y"

#include <string>
#include <vector>
#include <memory>
#include "formula_ast.h"

#line 78 "src/parser/formula_parser.tab.cc"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

#include "formula_parser.tab.h"
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_IDENTIFIER = 3,                 /* IDENTIFIER  */
  YYSYMBOL_STRING_LITERAL = 4,             /* STRING_LITERAL  */
  YYSYMBOL_LPAREN = 5,                     /* LPAREN  */
  YYSYMBOL_RPAREN = 6,                     /* RPAREN  */
  YYSYMBOL_COMMA = 7,                      /* COMMA  */
  YYSYMBOL_DOT = 8,                        /* DOT  */
  YYSYMBOL_COLON = 9,                      /* COLON  */
  YYSYMBOL_EQUALS = 10,                    /* EQUALS  */
  YYSYMBOL_AND = 11,                       /* AND  */
  YYSYMBOL_OR = 12,                        /* OR  */
  YYSYMBOL_NOT = 13,                       /* NOT  */
  YYSYMBOL_IMPLIES = 14,                   /* IMPLIES  */
  YYSYMBOL_IFF = 15,                       /* IFF  */
  YYSYMBOL_BOTTOM = 16,                    /* BOTTOM  */
  YYSYMBOL_FORALL = 17,                    /* FORALL  */
  YYSYMBOL_EXISTS = 18,                    /* EXISTS  */
  YYSYMBOL_AXIOM = 19,                     /* AXIOM  */
  YYSYMBOL_CLAIM = 20,                     /* CLAIM  */
  YYSYMBOL_PROOF = 21,                     /* PROOF  */
  YYSYMBOL_INCLUDE = 22,                   /* INCLUDE  */
  YYSYMBOL_DEF = 23,                       /* DEF  */
  YYSYMBOL_FIX = 24,                       /* FIX  */
  YYSYMBOL_ASSUME = 25,                    /* ASSUME  */
  YYSYMBOL_QED = 26,                       /* QED  */
  YYSYMBOL_USE = 27,                       /* USE  */
  YYSYMBOL_LET = 28,                       /* LET  */
  YYSYMBOL_AND_INTRO = 29,                 /* AND_INTRO  */
  YYSYMBOL_AND_ELIM_L = 30,                /* AND_ELIM_L  */
  YYSYMBOL_AND_ELIM_R = 31,                /* AND_ELIM_R  */
  YYSYMBOL_OR_INTRO_L = 32,                /* OR_INTRO_L  */
  YYSYMBOL_OR_INTRO_R = 33,                /* OR_INTRO_R  */
  YYSYMBOL_OR_ELIM = 34,                   /* OR_ELIM  */
  YYSYMBOL_IMPLIES_INTRO = 35,             /* IMPLIES_INTRO  */
  YYSYMBOL_IMPLIES_ELIM = 36,              /* IMPLIES_ELIM  */
  YYSYMBOL_NOT_INTRO = 37,                 /* NOT_INTRO  */
  YYSYMBOL_NOT_ELIM = 38,                  /* NOT_ELIM  */
  YYSYMBOL_BOTTOM_ELIM = 39,               /* BOTTOM_ELIM  */
  YYSYMBOL_IFF_INTRO = 40,                 /* IFF_INTRO  */
  YYSYMBOL_IFF_ELIM_L = 41,                /* IFF_ELIM_L  */
  YYSYMBOL_IFF_ELIM_R = 42,                /* IFF_ELIM_R  */
  YYSYMBOL_FORALL_INTRO = 43,              /* FORALL_INTRO  */
  YYSYMBOL_FORALL_ELIM = 44,               /* FORALL_ELIM  */
  YYSYMBOL_EXISTS_INTRO = 45,              /* EXISTS_INTRO  */
  YYSYMBOL_EXISTS_ELIM = 46,               /* EXISTS_ELIM  */
  YYSYMBOL_DOUBLE_NEG_ELIM = 47,           /* DOUBLE_NEG_ELIM  */
  YYSYMBOL_EXCLUDED_MIDDLE = 48,           /* EXCLUDED_MIDDLE  */
  YYSYMBOL_EQ_SUBST = 49,                  /* EQ_SUBST  */
  YYSYMBOL_EQ_SYM = 50,                    /* EQ_SYM  */
  YYSYMBOL_IOTA_ELIM = 51,                 /* IOTA_ELIM  */
  YYSYMBOL_IOTA = 52,                      /* IOTA  */
  YYSYMBOL_UNPROVED = 53,                  /* UNPROVED  */
  YYSYMBOL_SCHEMA = 54,                    /* SCHEMA  */
  YYSYMBOL_SCHEMA_INST = 55,               /* SCHEMA_INST  */
  YYSYMBOL_LBRACKET = 56,                  /* LBRACKET  */
  YYSYMBOL_RBRACKET = 57,                  /* RBRACKET  */
  YYSYMBOL_LBRACE = 58,                    /* LBRACE  */
  YYSYMBOL_RBRACE = 59,                    /* RBRACE  */
  YYSYMBOL_BACKSLASH = 60,                 /* BACKSLASH  */
  YYSYMBOL_NUMBER = 61,                    /* NUMBER  */
  YYSYMBOL_YYACCEPT = 62,                  /* $accept  */
  YYSYMBOL_input = 63,                     /* input  */
  YYSYMBOL_statement_list = 64,            /* statement_list  */
  YYSYMBOL_statement = 65,                 /* statement  */
  YYSYMBOL_formula = 66,                   /* formula  */
  YYSYMBOL_iff_formula = 67,               /* iff_formula  */
  YYSYMBOL_implies_formula = 68,           /* implies_formula  */
  YYSYMBOL_or_formula = 69,                /* or_formula  */
  YYSYMBOL_and_formula = 70,               /* and_formula  */
  YYSYMBOL_unary_formula = 71,             /* unary_formula  */
  YYSYMBOL_atom = 72,                      /* atom  */
  YYSYMBOL_predicate = 73,                 /* predicate  */
  YYSYMBOL_term_list = 74,                 /* term_list  */
  YYSYMBOL_term = 75,                      /* term  */
  YYSYMBOL_proof_block = 76,               /* proof_block  */
  YYSYMBOL_unproved_deps = 77,             /* unproved_deps  */
  YYSYMBOL_proof_step_list = 78,           /* proof_step_list  */
  YYSYMBOL_proof_step = 79,                /* proof_step  */
  YYSYMBOL_rule_call = 80,                 /* rule_call  */
  YYSYMBOL_id_list = 81,                   /* id_list  */
  YYSYMBOL_schema_binding_list = 82,       /* schema_binding_list  */
  YYSYMBOL_schema_var_list = 83,           /* schema_var_list  */
  YYSYMBOL_schema_var_decl = 84,           /* schema_var_decl  */
  YYSYMBOL_schema_binding = 85,            /* schema_binding  */
  YYSYMBOL_lambda_params = 86              /* lambda_params  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;




#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_uint8 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if 1

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* 1 */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL \
             && defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
  YYLTYPE yyls_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE) \
             + YYSIZEOF (YYLTYPE)) \
      + 2 * YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  36
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   171

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  62
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  25
/* YYNRULES -- Number of rules.  */
#define YYNRULES  82
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  173

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   316


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61
};

#if YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,    78,    78,    79,    83,    87,    94,    98,   102,   106,
     110,   111,   118,   122,   125,   129,   132,   136,   139,   143,
     146,   150,   153,   157,   161,   165,   168,   171,   175,   179,
     183,   190,   194,   201,   205,   214,   218,   226,   227,   235,
     239,   246,   250,   254,   258,   262,   267,   274,   277,   283,
     289,   292,   295,   298,   304,   307,   313,   316,   319,   322,
     325,   328,   334,   337,   340,   343,   349,   355,   358,   361,
     364,   371,   376,   384,   388,   395,   399,   406,   412,   421,
     427,   437,   442
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if 1
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "IDENTIFIER",
  "STRING_LITERAL", "LPAREN", "RPAREN", "COMMA", "DOT", "COLON", "EQUALS",
  "AND", "OR", "NOT", "IMPLIES", "IFF", "BOTTOM", "FORALL", "EXISTS",
  "AXIOM", "CLAIM", "PROOF", "INCLUDE", "DEF", "FIX", "ASSUME", "QED",
  "USE", "LET", "AND_INTRO", "AND_ELIM_L", "AND_ELIM_R", "OR_INTRO_L",
  "OR_INTRO_R", "OR_ELIM", "IMPLIES_INTRO", "IMPLIES_ELIM", "NOT_INTRO",
  "NOT_ELIM", "BOTTOM_ELIM", "IFF_INTRO", "IFF_ELIM_L", "IFF_ELIM_R",
  "FORALL_INTRO", "FORALL_ELIM", "EXISTS_INTRO", "EXISTS_ELIM",
  "DOUBLE_NEG_ELIM", "EXCLUDED_MIDDLE", "EQ_SUBST", "EQ_SYM", "IOTA_ELIM",
  "IOTA", "UNPROVED", "SCHEMA", "SCHEMA_INST", "LBRACKET", "RBRACKET",
  "LBRACE", "RBRACE", "BACKSLASH", "NUMBER", "$accept", "input",
  "statement_list", "statement", "formula", "iff_formula",
  "implies_formula", "or_formula", "and_formula", "unary_formula", "atom",
  "predicate", "term_list", "term", "proof_block", "unproved_deps",
  "proof_step_list", "proof_step", "rule_call", "id_list",
  "schema_binding_list", "schema_var_list", "schema_var_decl",
  "schema_binding", "lambda_params", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-13)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-1)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
       6,     9,    49,    49,   -13,    29,    42,    52,    53,    55,
      57,    59,    65,    70,    18,   -13,   -13,   -12,   -13,    30,
      63,   -13,   -13,   -13,   -13,    15,    67,   -13,    68,    69,
      66,    71,    72,   -13,    75,    23,   -13,   -13,    49,    49,
      49,    49,   -13,    32,   -13,    44,   -13,   -13,    49,    49,
      49,    49,    10,    76,    80,   -13,    63,   -13,   -13,   111,
     -13,     3,   -13,   -13,   -13,   -13,   105,   113,   119,   -13,
       7,   -13,   115,   125,     0,   -13,   116,   -13,    62,   -13,
     -13,   108,   -13,   136,    79,    80,   132,    49,    49,   139,
      49,   140,   141,   142,   140,   140,   140,   143,   140,   144,
     140,   140,   140,   140,   140,   145,   140,   140,   140,   147,
     148,   140,   140,   140,   149,   -13,   150,   146,   151,   -13,
      49,   152,   -13,   -13,   -13,   -13,   154,   -13,   -13,   154,
     154,   154,   -13,   154,   -13,   154,   154,   154,   154,   154,
     -13,   154,   154,   154,   -13,   -13,   154,   154,   154,    96,
     -13,    49,   -13,   -13,   -13,   153,   156,   -13,   -13,   155,
      -6,   -13,    -1,   156,   -13,   159,   -13,   -13,   -13,    27,
     -13,    49,   -13
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_int8 yydefact[] =
{
       0,    30,     0,     0,    25,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     3,     4,     2,    12,    14,    16,
      18,    20,    24,    27,    10,     0,     0,    21,     0,     0,
       0,     0,     0,    11,     0,     0,     1,     5,     0,     0,
       0,     0,    33,     0,    29,     0,    31,    26,     0,     0,
       0,     0,     0,     0,     0,    13,    17,    15,    19,     0,
      28,     0,    22,    23,     6,     8,     0,     0,     0,    37,
      35,    39,     0,    77,     0,    75,     0,    32,     0,    41,
      46,    36,    40,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    45,     0,     0,     0,    76,
       0,     0,    42,    44,    43,    71,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    67,    68,    69,     0,
      38,     0,    78,     9,    34,     0,     0,     7,    72,     0,
       0,    73,     0,     0,    70,     0,    79,    74,    81,     0,
      82,     0,    80
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
     -13,   -13,   -13,   157,    -2,   -13,    31,   -13,   124,     2,
     -13,   -13,   -13,   104,   -13,   -13,   -13,    97,   -13,    25,
     -13,   -13,    81,     5,   -13
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_uint8 yydefgoto[] =
{
       0,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    45,    46,    24,    81,    70,    71,   115,   126,
     160,    74,    75,   161,   169
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_uint8 yytable[] =
{
      26,   163,     1,    38,     2,    27,    42,    85,    43,     1,
      66,     2,     3,    66,    25,     4,     5,     6,    42,     3,
      43,    44,     4,     5,     6,     7,     8,     9,    10,    11,
     170,    67,    28,    68,    67,   171,    68,     7,     8,     9,
      10,    11,    39,    58,    40,    29,    62,    63,    64,    65,
      60,    61,     1,   164,     2,    30,    31,    86,    32,   165,
      12,    33,     3,    69,    34,     4,     5,     6,    35,    55,
      36,    57,    12,    47,    41,    50,    48,    49,    53,    54,
      51,    52,    72,    73,    59,   121,   122,    88,   124,    89,
      90,    91,    92,    93,    94,    95,    96,    97,    98,    99,
     100,   101,   102,   103,   104,   105,   106,   107,   108,   109,
     110,   111,   112,   113,    76,    78,    79,   114,   153,   129,
     130,   131,    80,   133,    87,   135,   136,   137,   138,   139,
      84,   141,   142,   143,    83,   116,   146,   147,   148,   117,
     118,   120,   123,   125,   127,   128,   132,   134,   140,   157,
     144,   145,   149,   150,   156,   151,   158,   152,   154,   159,
     166,   155,   168,    56,   162,    77,   119,    82,   167,   172,
       0,    37
};

static const yytype_int16 yycheck[] =
{
       2,     7,     3,    15,     5,     3,     3,     7,     5,     3,
       3,     5,    13,     3,     5,    16,    17,    18,     3,    13,
       5,     6,    16,    17,    18,    19,    20,    21,    22,    23,
       3,    24,     3,    26,    24,     8,    26,    19,    20,    21,
      22,    23,    12,    41,    14,     3,    48,    49,    50,    51,
       6,     7,     3,    59,     5,     3,     3,    57,     3,    60,
      54,     4,    13,    53,     5,    16,    17,    18,     3,    38,
       0,    40,    54,     6,    11,     9,     8,     8,     3,    56,
       9,     9,     6,     3,    52,    87,    88,    25,    90,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,     3,    10,     3,    55,   120,    94,
      95,    96,     3,    98,     8,   100,   101,   102,   103,   104,
       5,   106,   107,   108,    19,    27,   111,   112,   113,     3,
      61,     9,     3,     3,     3,     3,     3,     3,     3,   151,
       3,     3,     3,     3,    58,     9,     3,     6,     6,     3,
     162,     7,     3,    39,     9,    61,    85,    70,   163,   171,
      -1,    14
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_int8 yystos[] =
{
       0,     3,     5,    13,    16,    17,    18,    19,    20,    21,
      22,    23,    54,    63,    64,    65,    66,    67,    68,    69,
      70,    71,    72,    73,    76,     5,    66,    71,     3,     3,
       3,     3,     3,     4,     5,     3,     0,    65,    15,    12,
      14,    11,     3,     5,     6,    74,    75,     6,     8,     8,
       9,     9,     9,     3,    56,    68,    70,    68,    71,    52,
       6,     7,    66,    66,    66,    66,     3,    24,    26,    53,
      78,    79,     6,     3,    83,    84,     3,    75,    10,     3,
       3,    77,    79,    19,     5,     7,    57,     8,    25,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    55,    80,    27,     3,    61,    84,
       9,    66,    66,     3,    66,     3,    81,     3,     3,    81,
      81,    81,     3,    81,     3,    81,    81,    81,    81,    81,
       3,    81,    81,    81,     3,     3,    81,    81,    81,     3,
       3,     9,     6,    66,     6,     7,    58,    66,     3,     3,
      82,    85,     9,     7,    59,    60,    66,    85,     3,    86,
       3,     8,    66
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr1[] =
{
       0,    62,    63,    63,    64,    64,    65,    65,    65,    65,
      65,    65,    66,    67,    67,    68,    68,    69,    69,    70,
      70,    71,    71,    71,    71,    72,    72,    72,    73,    73,
      73,    74,    74,    75,    75,    76,    76,    77,    77,    78,
      78,    79,    79,    79,    79,    79,    79,    80,    80,    80,
      80,    80,    80,    80,    80,    80,    80,    80,    80,    80,
      80,    80,    80,    80,    80,    80,    80,    80,    80,    80,
      80,    81,    81,    82,    82,    83,    83,    84,    84,    85,
      85,    86,    86
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     1,     1,     1,     2,     4,     8,     4,     7,
       1,     2,     1,     3,     1,     3,     1,     3,     1,     3,
       1,     2,     4,     4,     1,     1,     3,     1,     4,     3,
       1,     1,     3,     1,     6,     4,     5,     0,     3,     1,
       2,     2,     4,     4,     4,     3,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       5,     1,     3,     1,     3,     1,     3,     1,     4,     3,
       6,     1,     2
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == YYEMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (&yylloc, scanner, ctx, YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use YYerror or YYUNDEF. */
#define YYERRCODE YYUNDEF

/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)                                \
    do                                                                  \
      if (N)                                                            \
        {                                                               \
          (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;        \
          (Current).first_column = YYRHSLOC (Rhs, 1).first_column;      \
          (Current).last_line    = YYRHSLOC (Rhs, N).last_line;         \
          (Current).last_column  = YYRHSLOC (Rhs, N).last_column;       \
        }                                                               \
      else                                                              \
        {                                                               \
          (Current).first_line   = (Current).last_line   =              \
            YYRHSLOC (Rhs, 0).last_line;                                \
          (Current).first_column = (Current).last_column =              \
            YYRHSLOC (Rhs, 0).last_column;                              \
        }                                                               \
    while (0)
#endif

#define YYRHSLOC(Rhs, K) ((Rhs)[K])


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)


/* YYLOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

# ifndef YYLOCATION_PRINT

#  if defined YY_LOCATION_PRINT

   /* Temporary convenience wrapper in case some people defined the
      undocumented and private YY_LOCATION_PRINT macros.  */
#   define YYLOCATION_PRINT(File, Loc)  YY_LOCATION_PRINT(File, *(Loc))

#  elif defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL

/* Print *YYLOCP on YYO.  Private, do not rely on its existence. */

YY_ATTRIBUTE_UNUSED
static int
yy_location_print_ (FILE *yyo, YYLTYPE const * const yylocp)
{
  int res = 0;
  int end_col = 0 != yylocp->last_column ? yylocp->last_column - 1 : 0;
  if (0 <= yylocp->first_line)
    {
      res += YYFPRINTF (yyo, "%d", yylocp->first_line);
      if (0 <= yylocp->first_column)
        res += YYFPRINTF (yyo, ".%d", yylocp->first_column);
    }
  if (0 <= yylocp->last_line)
    {
      if (yylocp->first_line < yylocp->last_line)
        {
          res += YYFPRINTF (yyo, "-%d", yylocp->last_line);
          if (0 <= end_col)
            res += YYFPRINTF (yyo, ".%d", end_col);
        }
      else if (0 <= end_col && yylocp->first_column < end_col)
        res += YYFPRINTF (yyo, "-%d", end_col);
    }
  return res;
}

#   define YYLOCATION_PRINT  yy_location_print_

    /* Temporary convenience wrapper in case some people defined the
       undocumented and private YY_LOCATION_PRINT macros.  */
#   define YY_LOCATION_PRINT(File, Loc)  YYLOCATION_PRINT(File, &(Loc))

#  else

#   define YYLOCATION_PRINT(File, Loc) ((void) 0)
    /* Temporary convenience wrapper in case some people defined the
       undocumented and private YY_LOCATION_PRINT macros.  */
#   define YY_LOCATION_PRINT  YYLOCATION_PRINT

#  endif
# endif /* !defined YYLOCATION_PRINT */


# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value, Location, scanner, ctx); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, yyscan_t scanner, ParseContext* ctx)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  YY_USE (yylocationp);
  YY_USE (scanner);
  YY_USE (ctx);
  if (!yyvaluep)
    return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, yyscan_t scanner, ParseContext* ctx)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  YYLOCATION_PRINT (yyo, yylocationp);
  YYFPRINTF (yyo, ": ");
  yy_symbol_value_print (yyo, yykind, yyvaluep, yylocationp, scanner, ctx);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp, YYLTYPE *yylsp,
                 int yyrule, yyscan_t scanner, ParseContext* ctx)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)],
                       &(yylsp[(yyi + 1) - (yynrhs)]), scanner, ctx);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, yylsp, Rule, scanner, ctx); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif
/* Parser data structure.  */
struct yypstate
  {
    /* Number of syntax errors so far.  */
    int yynerrs;

    yy_state_fast_t yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize;

    /* The state stack: array, bottom, top.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss;
    yy_state_t *yyssp;

    /* The semantic value stack: array, bottom, top.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    /* The location stack: array, bottom, top.  */
    YYLTYPE yylsa[YYINITDEPTH];
    YYLTYPE *yyls;
    YYLTYPE *yylsp;
    /* Whether this instance has not started parsing yet.
     * If 2, it corresponds to a finished parsing.  */
    int yynew;
  };


/* Context of a parse error.  */
typedef struct
{
  yypstate* yyps;
  yysymbol_kind_t yytoken;
  YYLTYPE *yylloc;
} yypcontext_t;

/* Put in YYARG at most YYARGN of the expected tokens given the
   current YYCTX, and return the number of tokens stored in YYARG.  If
   YYARG is null, return the number of expected tokens (guaranteed to
   be less than YYNTOKENS).  Return YYENOMEM on memory exhaustion.
   Return 0 if there are more than YYARGN expected tokens, yet fill
   YYARG up to YYARGN. */
static int
yypstate_expected_tokens (yypstate *yyps,
                          yysymbol_kind_t yyarg[], int yyargn)
{
  /* Actual size of YYARG. */
  int yycount = 0;
  int yyn = yypact[+*yyps->yyssp];
  if (!yypact_value_is_default (yyn))
    {
      /* Start YYX at -YYN if negative to avoid negative indexes in
         YYCHECK.  In other words, skip the first -YYN actions for
         this state because they are default actions.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;
      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yyx;
      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
        if (yycheck[yyx + yyn] == yyx && yyx != YYSYMBOL_YYerror
            && !yytable_value_is_error (yytable[yyx + yyn]))
          {
            if (!yyarg)
              ++yycount;
            else if (yycount == yyargn)
              return 0;
            else
              yyarg[yycount++] = YY_CAST (yysymbol_kind_t, yyx);
          }
    }
  if (yyarg && yycount == 0 && 0 < yyargn)
    yyarg[0] = YYSYMBOL_YYEMPTY;
  return yycount;
}


/* Similar to the previous function.  */
static int
yypcontext_expected_tokens (const yypcontext_t *yyctx,
                            yysymbol_kind_t yyarg[], int yyargn)
{
  return yypstate_expected_tokens (yyctx->yyps, yyarg, yyargn);
}


#ifndef yystrlen
# if defined __GLIBC__ && defined _STRING_H
#  define yystrlen(S) (YY_CAST (YYPTRDIFF_T, strlen (S)))
# else
/* Return the length of YYSTR.  */
static YYPTRDIFF_T
yystrlen (const char *yystr)
{
  YYPTRDIFF_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
# endif
#endif

#ifndef yystpcpy
# if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#  define yystpcpy stpcpy
# else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
yystpcpy (char *yydest, const char *yysrc)
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
# endif
#endif

#ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYPTRDIFF_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYPTRDIFF_T yyn = 0;
      char const *yyp = yystr;
      for (;;)
        switch (*++yyp)
          {
          case '\'':
          case ',':
            goto do_not_strip_quotes;

          case '\\':
            if (*++yyp != '\\')
              goto do_not_strip_quotes;
            else
              goto append;

          append:
          default:
            if (yyres)
              yyres[yyn] = *yyp;
            yyn++;
            break;

          case '"':
            if (yyres)
              yyres[yyn] = '\0';
            return yyn;
          }
    do_not_strip_quotes: ;
    }

  if (yyres)
    return yystpcpy (yyres, yystr) - yyres;
  else
    return yystrlen (yystr);
}
#endif


static int
yy_syntax_error_arguments (const yypcontext_t *yyctx,
                           yysymbol_kind_t yyarg[], int yyargn)
{
  /* Actual size of YYARG. */
  int yycount = 0;
  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yyctx->yytoken != YYSYMBOL_YYEMPTY)
    {
      int yyn;
      if (yyarg)
        yyarg[yycount] = yyctx->yytoken;
      ++yycount;
      yyn = yypcontext_expected_tokens (yyctx,
                                        yyarg ? yyarg + 1 : yyarg, yyargn - 1);
      if (yyn == YYENOMEM)
        return YYENOMEM;
      else
        yycount += yyn;
    }
  return yycount;
}

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return -1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return YYENOMEM if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYPTRDIFF_T *yymsg_alloc, char **yymsg,
                const yypcontext_t *yyctx)
{
  enum { YYARGS_MAX = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat: reported tokens (one for the "unexpected",
     one per "expected"). */
  yysymbol_kind_t yyarg[YYARGS_MAX];
  /* Cumulated lengths of YYARG.  */
  YYPTRDIFF_T yysize = 0;

  /* Actual size of YYARG. */
  int yycount = yy_syntax_error_arguments (yyctx, yyarg, YYARGS_MAX);
  if (yycount == YYENOMEM)
    return YYENOMEM;

  switch (yycount)
    {
#define YYCASE_(N, S)                       \
      case N:                               \
        yyformat = S;                       \
        break
    default: /* Avoid compiler warnings. */
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
#undef YYCASE_
    }

  /* Compute error message size.  Don't count the "%s"s, but reserve
     room for the terminator.  */
  yysize = yystrlen (yyformat) - 2 * yycount + 1;
  {
    int yyi;
    for (yyi = 0; yyi < yycount; ++yyi)
      {
        YYPTRDIFF_T yysize1
          = yysize + yytnamerr (YY_NULLPTR, yytname[yyarg[yyi]]);
        if (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM)
          yysize = yysize1;
        else
          return YYENOMEM;
      }
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return -1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yytname[yyarg[yyi++]]);
          yyformat += 2;
        }
      else
        {
          ++yyp;
          ++yyformat;
        }
  }
  return 0;
}


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep, YYLTYPE *yylocationp, yyscan_t scanner, ParseContext* ctx)
{
  YY_USE (yyvaluep);
  YY_USE (yylocationp);
  YY_USE (scanner);
  YY_USE (ctx);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  switch (yykind)
    {
    case YYSYMBOL_IDENTIFIER: /* IDENTIFIER  */
#line 68 "src/parser/formula.y"
            { delete ((*yyvaluep).str); }
#line 1391 "src/parser/formula_parser.tab.cc"
        break;

    case YYSYMBOL_STRING_LITERAL: /* STRING_LITERAL  */
#line 68 "src/parser/formula.y"
            { delete ((*yyvaluep).str); }
#line 1397 "src/parser/formula_parser.tab.cc"
        break;

    case YYSYMBOL_NUMBER: /* NUMBER  */
#line 68 "src/parser/formula.y"
            { delete ((*yyvaluep).str); }
#line 1403 "src/parser/formula_parser.tab.cc"
        break;

    case YYSYMBOL_statement_list: /* statement_list  */
#line 71 "src/parser/formula.y"
            { for (auto* n : *((*yyvaluep).node_list)) delete n; delete ((*yyvaluep).node_list); }
#line 1409 "src/parser/formula_parser.tab.cc"
        break;

    case YYSYMBOL_statement: /* statement  */
#line 70 "src/parser/formula.y"
            { delete ((*yyvaluep).node); }
#line 1415 "src/parser/formula_parser.tab.cc"
        break;

    case YYSYMBOL_formula: /* formula  */
#line 70 "src/parser/formula.y"
            { delete ((*yyvaluep).node); }
#line 1421 "src/parser/formula_parser.tab.cc"
        break;

    case YYSYMBOL_iff_formula: /* iff_formula  */
#line 70 "src/parser/formula.y"
            { delete ((*yyvaluep).node); }
#line 1427 "src/parser/formula_parser.tab.cc"
        break;

    case YYSYMBOL_implies_formula: /* implies_formula  */
#line 70 "src/parser/formula.y"
            { delete ((*yyvaluep).node); }
#line 1433 "src/parser/formula_parser.tab.cc"
        break;

    case YYSYMBOL_or_formula: /* or_formula  */
#line 70 "src/parser/formula.y"
            { delete ((*yyvaluep).node); }
#line 1439 "src/parser/formula_parser.tab.cc"
        break;

    case YYSYMBOL_and_formula: /* and_formula  */
#line 70 "src/parser/formula.y"
            { delete ((*yyvaluep).node); }
#line 1445 "src/parser/formula_parser.tab.cc"
        break;

    case YYSYMBOL_unary_formula: /* unary_formula  */
#line 70 "src/parser/formula.y"
            { delete ((*yyvaluep).node); }
#line 1451 "src/parser/formula_parser.tab.cc"
        break;

    case YYSYMBOL_atom: /* atom  */
#line 70 "src/parser/formula.y"
            { delete ((*yyvaluep).node); }
#line 1457 "src/parser/formula_parser.tab.cc"
        break;

    case YYSYMBOL_predicate: /* predicate  */
#line 70 "src/parser/formula.y"
            { delete ((*yyvaluep).node); }
#line 1463 "src/parser/formula_parser.tab.cc"
        break;

    case YYSYMBOL_term_list: /* term_list  */
#line 71 "src/parser/formula.y"
            { for (auto* n : *((*yyvaluep).node_list)) delete n; delete ((*yyvaluep).node_list); }
#line 1469 "src/parser/formula_parser.tab.cc"
        break;

    case YYSYMBOL_term: /* term  */
#line 70 "src/parser/formula.y"
            { delete ((*yyvaluep).node); }
#line 1475 "src/parser/formula_parser.tab.cc"
        break;

    case YYSYMBOL_proof_block: /* proof_block  */
#line 70 "src/parser/formula.y"
            { delete ((*yyvaluep).node); }
#line 1481 "src/parser/formula_parser.tab.cc"
        break;

    case YYSYMBOL_unproved_deps: /* unproved_deps  */
#line 71 "src/parser/formula.y"
            { for (auto* n : *((*yyvaluep).node_list)) delete n; delete ((*yyvaluep).node_list); }
#line 1487 "src/parser/formula_parser.tab.cc"
        break;

    case YYSYMBOL_proof_step_list: /* proof_step_list  */
#line 71 "src/parser/formula.y"
            { for (auto* n : *((*yyvaluep).node_list)) delete n; delete ((*yyvaluep).node_list); }
#line 1493 "src/parser/formula_parser.tab.cc"
        break;

    case YYSYMBOL_proof_step: /* proof_step  */
#line 70 "src/parser/formula.y"
            { delete ((*yyvaluep).node); }
#line 1499 "src/parser/formula_parser.tab.cc"
        break;

    case YYSYMBOL_rule_call: /* rule_call  */
#line 70 "src/parser/formula.y"
            { delete ((*yyvaluep).node); }
#line 1505 "src/parser/formula_parser.tab.cc"
        break;

    case YYSYMBOL_id_list: /* id_list  */
#line 71 "src/parser/formula.y"
            { for (auto* n : *((*yyvaluep).node_list)) delete n; delete ((*yyvaluep).node_list); }
#line 1511 "src/parser/formula_parser.tab.cc"
        break;

    case YYSYMBOL_schema_binding_list: /* schema_binding_list  */
#line 71 "src/parser/formula.y"
            { for (auto* n : *((*yyvaluep).node_list)) delete n; delete ((*yyvaluep).node_list); }
#line 1517 "src/parser/formula_parser.tab.cc"
        break;

    case YYSYMBOL_schema_var_list: /* schema_var_list  */
#line 71 "src/parser/formula.y"
            { for (auto* n : *((*yyvaluep).node_list)) delete n; delete ((*yyvaluep).node_list); }
#line 1523 "src/parser/formula_parser.tab.cc"
        break;

    case YYSYMBOL_schema_var_decl: /* schema_var_decl  */
#line 69 "src/parser/formula.y"
            { delete ((*yyvaluep).node); }
#line 1529 "src/parser/formula_parser.tab.cc"
        break;

    case YYSYMBOL_schema_binding: /* schema_binding  */
#line 70 "src/parser/formula.y"
            { delete ((*yyvaluep).node); }
#line 1535 "src/parser/formula_parser.tab.cc"
        break;

    case YYSYMBOL_lambda_params: /* lambda_params  */
#line 71 "src/parser/formula.y"
            { for (auto* n : *((*yyvaluep).node_list)) delete n; delete ((*yyvaluep).node_list); }
#line 1541 "src/parser/formula_parser.tab.cc"
        break;

      default:
        break;
    }
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}





#define yynerrs yyps->yynerrs
#define yystate yyps->yystate
#define yyerrstatus yyps->yyerrstatus
#define yyssa yyps->yyssa
#define yyss yyps->yyss
#define yyssp yyps->yyssp
#define yyvsa yyps->yyvsa
#define yyvs yyps->yyvs
#define yyvsp yyps->yyvsp
#define yylsa yyps->yylsa
#define yyls yyps->yyls
#define yylsp yyps->yylsp
#define yystacksize yyps->yystacksize

/* Initialize the parser data structure.  */
static void
yypstate_clear (yypstate *yyps)
{
  yynerrs = 0;
  yystate = 0;
  yyerrstatus = 0;

  yyssp = yyss;
  yyvsp = yyvs;
  yylsp = yyls;

  /* Initialize the state stack, in case yypcontext_expected_tokens is
     called before the first call to yyparse. */
  *yyssp = 0;
  yyps->yynew = 1;
}

/* Initialize the parser data structure.  */
yypstate *
yypstate_new (void)
{
  yypstate *yyps;
  yyps = YY_CAST (yypstate *, YYMALLOC (sizeof *yyps));
  if (!yyps)
    return YY_NULLPTR;
  yystacksize = YYINITDEPTH;
  yyss = yyssa;
  yyvs = yyvsa;
  yyls = yylsa;
  yypstate_clear (yyps);
  return yyps;
}

void
yypstate_delete (yypstate *yyps)
{
  if (yyps)
    {
#ifndef yyoverflow
      /* If the stack was reallocated but the parse did not complete, then the
         stack still needs to be freed.  */
      if (yyss != yyssa)
        YYSTACK_FREE (yyss);
#endif
      YYFREE (yyps);
    }
}



/*---------------.
| yypush_parse.  |
`---------------*/

int
yypush_parse (yypstate *yyps,
              int yypushed_char, YYSTYPE const *yypushed_val, YYLTYPE *yypushed_loc, yyscan_t scanner, ParseContext* ctx)
{
/* Lookahead token kind.  */
int yychar;


/* The semantic value of the lookahead symbol.  */
/* Default value used for initialization, for pacifying older GCCs
   or non-GCC compilers.  */
YY_INITIAL_VALUE (static YYSTYPE yyval_default;)
YYSTYPE yylval YY_INITIAL_VALUE (= yyval_default);

/* Location data for the lookahead symbol.  */
static YYLTYPE yyloc_default
# if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL
  = { 1, 1, 1, 1 }
# endif
;
YYLTYPE yylloc = yyloc_default;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
  YYLTYPE yyloc;

  /* The locations where the error started and ended.  */
  YYLTYPE yyerror_range[3];

  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYPTRDIFF_T yymsg_alloc = sizeof yymsgbuf;

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N), yylsp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  switch (yyps->yynew)
    {
    case 0:
      yyn = yypact[yystate];
      goto yyread_pushed_token;

    case 2:
      yypstate_clear (yyps);
      break;

    default:
      break;
    }

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = YYEMPTY; /* Cause a token to be read.  */

  yylsp[0] = *yypushed_loc;
  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    YYNOMEM;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;
        YYLTYPE *yyls1 = yyls;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yyls1, yysize * YYSIZEOF (*yylsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
        yyls = yyls1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        YYNOMEM;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          YYNOMEM;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
        YYSTACK_RELOCATE (yyls_alloc, yyls);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;
      yylsp = yyls + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */


  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == YYEMPTY)
    {
      if (!yyps->yynew)
        {
          YYDPRINTF ((stderr, "Return for a new token:\n"));
          yyresult = YYPUSH_MORE;
          goto yypushreturn;
        }
      yyps->yynew = 0;
yyread_pushed_token:
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yypushed_char;
      if (yypushed_val)
        yylval = *yypushed_val;
      if (yypushed_loc)
        yylloc = *yypushed_loc;
    }

  if (yychar <= YYEOF)
    {
      yychar = YYEOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == YYerror)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = YYUNDEF;
      yytoken = YYSYMBOL_YYerror;
      yyerror_range[1] = yylloc;
      goto yyerrlab1;
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END
  *++yylsp = yylloc;

  /* Discard the shifted token.  */
  yychar = YYEMPTY;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];

  /* Default location. */
  YYLLOC_DEFAULT (yyloc, (yylsp - yylen), yylen);
  yyerror_range[1] = yyloc;
  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 2: /* input: formula  */
#line 78 "src/parser/formula.y"
              { ctx->result = (yyvsp[0].node); }
#line 1910 "src/parser/formula_parser.tab.cc"
    break;

  case 3: /* input: statement_list  */
#line 79 "src/parser/formula.y"
                     { ctx->statements = (yyvsp[0].node_list); }
#line 1916 "src/parser/formula_parser.tab.cc"
    break;

  case 4: /* statement_list: statement  */
#line 83 "src/parser/formula.y"
                {
        (yyval.node_list) = new std::vector<ASTNode*>();
        (yyval.node_list)->push_back((yyvsp[0].node));
    }
#line 1925 "src/parser/formula_parser.tab.cc"
    break;

  case 5: /* statement_list: statement_list statement  */
#line 87 "src/parser/formula.y"
                               {
        (yyval.node_list) = (yyvsp[-1].node_list);
        (yyval.node_list)->push_back((yyvsp[0].node));
    }
#line 1934 "src/parser/formula_parser.tab.cc"
    break;

  case 6: /* statement: AXIOM IDENTIFIER COLON formula  */
#line 94 "src/parser/formula.y"
                                     {
        (yyval.node) = ASTNode::make_statement(ASTNode::AxiomStmt, *(yyvsp[-2].str), (yyvsp[0].node));
        delete (yyvsp[-2].str);
    }
#line 1943 "src/parser/formula_parser.tab.cc"
    break;

  case 7: /* statement: DEF LPAREN IDENTIFIER RPAREN AXIOM IDENTIFIER COLON formula  */
#line 98 "src/parser/formula.y"
                                                                  {
        (yyval.node) = ASTNode::make_def_statement(*(yyvsp[-5].str), *(yyvsp[-2].str), (yyvsp[0].node));
        delete (yyvsp[-5].str); delete (yyvsp[-2].str);
    }
#line 1952 "src/parser/formula_parser.tab.cc"
    break;

  case 8: /* statement: CLAIM IDENTIFIER COLON formula  */
#line 102 "src/parser/formula.y"
                                     {
        (yyval.node) = ASTNode::make_statement(ASTNode::ClaimStmt, *(yyvsp[-2].str), (yyvsp[0].node));
        delete (yyvsp[-2].str);
    }
#line 1961 "src/parser/formula_parser.tab.cc"
    break;

  case 9: /* statement: SCHEMA IDENTIFIER LBRACKET schema_var_list RBRACKET COLON formula  */
#line 106 "src/parser/formula.y"
                                                                        {
        (yyval.node) = ASTNode::make_schema_stmt(*(yyvsp[-5].str), (yyvsp[-3].node_list), (yyvsp[0].node));
        delete (yyvsp[-5].str);
    }
#line 1970 "src/parser/formula_parser.tab.cc"
    break;

  case 10: /* statement: proof_block  */
#line 110 "src/parser/formula.y"
                  { (yyval.node) = (yyvsp[0].node); }
#line 1976 "src/parser/formula_parser.tab.cc"
    break;

  case 11: /* statement: INCLUDE STRING_LITERAL  */
#line 111 "src/parser/formula.y"
                             {
        (yyval.node) = ASTNode::make_include(*(yyvsp[0].str));
        delete (yyvsp[0].str);
    }
#line 1985 "src/parser/formula_parser.tab.cc"
    break;

  case 12: /* formula: iff_formula  */
#line 118 "src/parser/formula.y"
                  { (yyval.node) = (yyvsp[0].node); }
#line 1991 "src/parser/formula_parser.tab.cc"
    break;

  case 13: /* iff_formula: iff_formula IFF implies_formula  */
#line 122 "src/parser/formula.y"
                                      {
        (yyval.node) = new ASTNode(ASTNode::Iff, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 1999 "src/parser/formula_parser.tab.cc"
    break;

  case 14: /* iff_formula: implies_formula  */
#line 125 "src/parser/formula.y"
                      { (yyval.node) = (yyvsp[0].node); }
#line 2005 "src/parser/formula_parser.tab.cc"
    break;

  case 15: /* implies_formula: or_formula IMPLIES implies_formula  */
#line 129 "src/parser/formula.y"
                                         {
        (yyval.node) = new ASTNode(ASTNode::Implies, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2013 "src/parser/formula_parser.tab.cc"
    break;

  case 16: /* implies_formula: or_formula  */
#line 132 "src/parser/formula.y"
                 { (yyval.node) = (yyvsp[0].node); }
#line 2019 "src/parser/formula_parser.tab.cc"
    break;

  case 17: /* or_formula: or_formula OR and_formula  */
#line 136 "src/parser/formula.y"
                                {
        (yyval.node) = new ASTNode(ASTNode::Or, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2027 "src/parser/formula_parser.tab.cc"
    break;

  case 18: /* or_formula: and_formula  */
#line 139 "src/parser/formula.y"
                  { (yyval.node) = (yyvsp[0].node); }
#line 2033 "src/parser/formula_parser.tab.cc"
    break;

  case 19: /* and_formula: and_formula AND unary_formula  */
#line 143 "src/parser/formula.y"
                                    {
        (yyval.node) = new ASTNode(ASTNode::And, (yyvsp[-2].node), (yyvsp[0].node));
    }
#line 2041 "src/parser/formula_parser.tab.cc"
    break;

  case 20: /* and_formula: unary_formula  */
#line 146 "src/parser/formula.y"
                    { (yyval.node) = (yyvsp[0].node); }
#line 2047 "src/parser/formula_parser.tab.cc"
    break;

  case 21: /* unary_formula: NOT unary_formula  */
#line 150 "src/parser/formula.y"
                        {
        (yyval.node) = new ASTNode(ASTNode::Not, (yyvsp[0].node));
    }
#line 2055 "src/parser/formula_parser.tab.cc"
    break;

  case 22: /* unary_formula: FORALL IDENTIFIER DOT formula  */
#line 153 "src/parser/formula.y"
                                    {
        (yyval.node) = new ASTNode(ASTNode::Forall, *(yyvsp[-2].str), (yyvsp[0].node));
        delete (yyvsp[-2].str);
    }
#line 2064 "src/parser/formula_parser.tab.cc"
    break;

  case 23: /* unary_formula: EXISTS IDENTIFIER DOT formula  */
#line 157 "src/parser/formula.y"
                                    {
        (yyval.node) = new ASTNode(ASTNode::Exists, *(yyvsp[-2].str), (yyvsp[0].node));
        delete (yyvsp[-2].str);
    }
#line 2073 "src/parser/formula_parser.tab.cc"
    break;

  case 24: /* unary_formula: atom  */
#line 161 "src/parser/formula.y"
           { (yyval.node) = (yyvsp[0].node); }
#line 2079 "src/parser/formula_parser.tab.cc"
    break;

  case 25: /* atom: BOTTOM  */
#line 165 "src/parser/formula.y"
             {
        (yyval.node) = new ASTNode(ASTNode::Bottom);
    }
#line 2087 "src/parser/formula_parser.tab.cc"
    break;

  case 26: /* atom: LPAREN formula RPAREN  */
#line 168 "src/parser/formula.y"
                            {
        (yyval.node) = (yyvsp[-1].node);
    }
#line 2095 "src/parser/formula_parser.tab.cc"
    break;

  case 27: /* atom: predicate  */
#line 171 "src/parser/formula.y"
                { (yyval.node) = (yyvsp[0].node); }
#line 2101 "src/parser/formula_parser.tab.cc"
    break;

  case 28: /* predicate: IDENTIFIER LPAREN term_list RPAREN  */
#line 175 "src/parser/formula.y"
                                         {
        (yyval.node) = new ASTNode(ASTNode::Predicate, *(yyvsp[-3].str), (yyvsp[-1].node_list));
        delete (yyvsp[-3].str);
    }
#line 2110 "src/parser/formula_parser.tab.cc"
    break;

  case 29: /* predicate: IDENTIFIER LPAREN RPAREN  */
#line 179 "src/parser/formula.y"
                               {
        (yyval.node) = new ASTNode(ASTNode::Predicate, *(yyvsp[-2].str), new std::vector<ASTNode*>());
        delete (yyvsp[-2].str);
    }
#line 2119 "src/parser/formula_parser.tab.cc"
    break;

  case 30: /* predicate: IDENTIFIER  */
#line 183 "src/parser/formula.y"
                 {
        (yyval.node) = new ASTNode(ASTNode::Predicate, *(yyvsp[0].str), new std::vector<ASTNode*>());
        delete (yyvsp[0].str);
    }
#line 2128 "src/parser/formula_parser.tab.cc"
    break;

  case 31: /* term_list: term  */
#line 190 "src/parser/formula.y"
           {
        (yyval.node_list) = new std::vector<ASTNode*>();
        (yyval.node_list)->push_back((yyvsp[0].node));
    }
#line 2137 "src/parser/formula_parser.tab.cc"
    break;

  case 32: /* term_list: term_list COMMA term  */
#line 194 "src/parser/formula.y"
                           {
        (yyval.node_list) = (yyvsp[-2].node_list);
        (yyval.node_list)->push_back((yyvsp[0].node));
    }
#line 2146 "src/parser/formula_parser.tab.cc"
    break;

  case 33: /* term: IDENTIFIER  */
#line 201 "src/parser/formula.y"
                 {
        (yyval.node) = new ASTNode(ASTNode::Term, *(yyvsp[0].str));
        delete (yyvsp[0].str);
    }
#line 2155 "src/parser/formula_parser.tab.cc"
    break;

  case 34: /* term: LPAREN IOTA IDENTIFIER DOT formula RPAREN  */
#line 205 "src/parser/formula.y"
                                                {
        /* Parenthesized iota term: (iota x. φ) */
        (yyval.node) = new ASTNode(ASTNode::DescriptionTerm, *(yyvsp[-3].str), (yyvsp[-1].node));
        delete (yyvsp[-3].str);
    }
#line 2165 "src/parser/formula_parser.tab.cc"
    break;

  case 35: /* proof_block: PROOF IDENTIFIER COLON proof_step_list  */
#line 214 "src/parser/formula.y"
                                             {
        (yyval.node) = ASTNode::make_proof_block(*(yyvsp[-2].str), (yyvsp[0].node_list));
        delete (yyvsp[-2].str);
    }
#line 2174 "src/parser/formula_parser.tab.cc"
    break;

  case 36: /* proof_block: PROOF IDENTIFIER COLON UNPROVED unproved_deps  */
#line 218 "src/parser/formula.y"
                                                    {
        (yyval.node) = ASTNode::make_proof_block(*(yyvsp[-3].str), (yyvsp[0].node_list));
        (yyval.node)->rule_name = "UNPROVED";
        delete (yyvsp[-3].str);
    }
#line 2184 "src/parser/formula_parser.tab.cc"
    break;

  case 37: /* unproved_deps: %empty  */
#line 226 "src/parser/formula.y"
                  { (yyval.node_list) = new std::vector<ASTNode*>(); }
#line 2190 "src/parser/formula_parser.tab.cc"
    break;

  case 38: /* unproved_deps: unproved_deps USE IDENTIFIER  */
#line 227 "src/parser/formula.y"
                                   {
        (yyval.node_list) = (yyvsp[-2].node_list);
        (yyval.node_list)->push_back(ASTNode::make_proof_step(ASTNode::ProofStepUse, "", *(yyvsp[0].str), nullptr, nullptr));
        delete (yyvsp[0].str);
    }
#line 2200 "src/parser/formula_parser.tab.cc"
    break;

  case 39: /* proof_step_list: proof_step  */
#line 235 "src/parser/formula.y"
                 {
        (yyval.node_list) = new std::vector<ASTNode*>();
        (yyval.node_list)->push_back((yyvsp[0].node));
    }
#line 2209 "src/parser/formula_parser.tab.cc"
    break;

  case 40: /* proof_step_list: proof_step_list proof_step  */
#line 239 "src/parser/formula.y"
                                 {
        (yyval.node_list) = (yyvsp[-1].node_list);
        (yyval.node_list)->push_back((yyvsp[0].node));
    }
#line 2218 "src/parser/formula_parser.tab.cc"
    break;

  case 41: /* proof_step: FIX IDENTIFIER  */
#line 246 "src/parser/formula.y"
                     {
        (yyval.node) = ASTNode::make_proof_step(ASTNode::ProofStepFix, "", *(yyvsp[0].str), nullptr, nullptr);
        delete (yyvsp[0].str);
    }
#line 2227 "src/parser/formula_parser.tab.cc"
    break;

  case 42: /* proof_step: IDENTIFIER EQUALS ASSUME formula  */
#line 250 "src/parser/formula.y"
                                       {
        (yyval.node) = ASTNode::make_proof_step(ASTNode::ProofStepAssume, *(yyvsp[-3].str), "", (yyvsp[0].node), nullptr);
        delete (yyvsp[-3].str);
    }
#line 2236 "src/parser/formula_parser.tab.cc"
    break;

  case 43: /* proof_step: IDENTIFIER EQUALS LET formula  */
#line 254 "src/parser/formula.y"
                                    {
        (yyval.node) = ASTNode::make_proof_step(ASTNode::ProofStepLet, *(yyvsp[-3].str), "", (yyvsp[0].node), nullptr);
        delete (yyvsp[-3].str);
    }
#line 2245 "src/parser/formula_parser.tab.cc"
    break;

  case 44: /* proof_step: IDENTIFIER EQUALS USE IDENTIFIER  */
#line 258 "src/parser/formula.y"
                                       {
        (yyval.node) = ASTNode::make_proof_step(ASTNode::ProofStepUse, *(yyvsp[-3].str), *(yyvsp[0].str), nullptr, nullptr);
        delete (yyvsp[-3].str); delete (yyvsp[0].str);
    }
#line 2254 "src/parser/formula_parser.tab.cc"
    break;

  case 45: /* proof_step: IDENTIFIER EQUALS rule_call  */
#line 262 "src/parser/formula.y"
                                  {
        (yyvsp[0].node)->name = *(yyvsp[-2].str);  /* Set result name on rule node */
        (yyval.node) = (yyvsp[0].node);
        delete (yyvsp[-2].str);
    }
#line 2264 "src/parser/formula_parser.tab.cc"
    break;

  case 46: /* proof_step: QED IDENTIFIER  */
#line 267 "src/parser/formula.y"
                     {
        (yyval.node) = ASTNode::make_proof_step(ASTNode::ProofStepQed, "", *(yyvsp[0].str), nullptr, nullptr);
        delete (yyvsp[0].str);
    }
#line 2273 "src/parser/formula_parser.tab.cc"
    break;

  case 47: /* rule_call: AND_INTRO id_list  */
#line 274 "src/parser/formula.y"
                        {
        (yyval.node) = ASTNode::make_rule_step("and_intro", (yyvsp[0].node_list));
    }
#line 2281 "src/parser/formula_parser.tab.cc"
    break;

  case 48: /* rule_call: AND_ELIM_L IDENTIFIER  */
#line 277 "src/parser/formula.y"
                            {
        auto* args = new std::vector<ASTNode*>();
        args->push_back(new ASTNode(ASTNode::Term, *(yyvsp[0].str)));
        (yyval.node) = ASTNode::make_rule_step("and_elim_l", args);
        delete (yyvsp[0].str);
    }
#line 2292 "src/parser/formula_parser.tab.cc"
    break;

  case 49: /* rule_call: AND_ELIM_R IDENTIFIER  */
#line 283 "src/parser/formula.y"
                            {
        auto* args = new std::vector<ASTNode*>();
        args->push_back(new ASTNode(ASTNode::Term, *(yyvsp[0].str)));
        (yyval.node) = ASTNode::make_rule_step("and_elim_r", args);
        delete (yyvsp[0].str);
    }
#line 2303 "src/parser/formula_parser.tab.cc"
    break;

  case 50: /* rule_call: OR_INTRO_L id_list  */
#line 289 "src/parser/formula.y"
                         {
        (yyval.node) = ASTNode::make_rule_step("or_intro_l", (yyvsp[0].node_list));
    }
#line 2311 "src/parser/formula_parser.tab.cc"
    break;

  case 51: /* rule_call: OR_INTRO_R id_list  */
#line 292 "src/parser/formula.y"
                         {
        (yyval.node) = ASTNode::make_rule_step("or_intro_r", (yyvsp[0].node_list));
    }
#line 2319 "src/parser/formula_parser.tab.cc"
    break;

  case 52: /* rule_call: OR_ELIM id_list  */
#line 295 "src/parser/formula.y"
                      {
        (yyval.node) = ASTNode::make_rule_step("or_elim", (yyvsp[0].node_list));
    }
#line 2327 "src/parser/formula_parser.tab.cc"
    break;

  case 53: /* rule_call: IMPLIES_INTRO IDENTIFIER  */
#line 298 "src/parser/formula.y"
                               {
        auto* args = new std::vector<ASTNode*>();
        args->push_back(new ASTNode(ASTNode::Term, *(yyvsp[0].str)));
        (yyval.node) = ASTNode::make_rule_step("implies_intro", args);
        delete (yyvsp[0].str);
    }
#line 2338 "src/parser/formula_parser.tab.cc"
    break;

  case 54: /* rule_call: IMPLIES_ELIM id_list  */
#line 304 "src/parser/formula.y"
                           {
        (yyval.node) = ASTNode::make_rule_step("implies_elim", (yyvsp[0].node_list));
    }
#line 2346 "src/parser/formula_parser.tab.cc"
    break;

  case 55: /* rule_call: NOT_INTRO IDENTIFIER  */
#line 307 "src/parser/formula.y"
                           {
        auto* args = new std::vector<ASTNode*>();
        args->push_back(new ASTNode(ASTNode::Term, *(yyvsp[0].str)));
        (yyval.node) = ASTNode::make_rule_step("not_intro", args);
        delete (yyvsp[0].str);
    }
#line 2357 "src/parser/formula_parser.tab.cc"
    break;

  case 56: /* rule_call: NOT_ELIM id_list  */
#line 313 "src/parser/formula.y"
                       {
        (yyval.node) = ASTNode::make_rule_step("not_elim", (yyvsp[0].node_list));
    }
#line 2365 "src/parser/formula_parser.tab.cc"
    break;

  case 57: /* rule_call: BOTTOM_ELIM id_list  */
#line 316 "src/parser/formula.y"
                          {
        (yyval.node) = ASTNode::make_rule_step("bottom_elim", (yyvsp[0].node_list));
    }
#line 2373 "src/parser/formula_parser.tab.cc"
    break;

  case 58: /* rule_call: IFF_INTRO id_list  */
#line 319 "src/parser/formula.y"
                        {
        (yyval.node) = ASTNode::make_rule_step("iff_intro", (yyvsp[0].node_list));
    }
#line 2381 "src/parser/formula_parser.tab.cc"
    break;

  case 59: /* rule_call: IFF_ELIM_L id_list  */
#line 322 "src/parser/formula.y"
                         {
        (yyval.node) = ASTNode::make_rule_step("iff_elim_l", (yyvsp[0].node_list));
    }
#line 2389 "src/parser/formula_parser.tab.cc"
    break;

  case 60: /* rule_call: IFF_ELIM_R id_list  */
#line 325 "src/parser/formula.y"
                         {
        (yyval.node) = ASTNode::make_rule_step("iff_elim_r", (yyvsp[0].node_list));
    }
#line 2397 "src/parser/formula_parser.tab.cc"
    break;

  case 61: /* rule_call: FORALL_INTRO IDENTIFIER  */
#line 328 "src/parser/formula.y"
                              {
        auto* args = new std::vector<ASTNode*>();
        args->push_back(new ASTNode(ASTNode::Term, *(yyvsp[0].str)));
        (yyval.node) = ASTNode::make_rule_step("forall_intro", args);
        delete (yyvsp[0].str);
    }
#line 2408 "src/parser/formula_parser.tab.cc"
    break;

  case 62: /* rule_call: FORALL_ELIM id_list  */
#line 334 "src/parser/formula.y"
                          {
        (yyval.node) = ASTNode::make_rule_step("forall_elim", (yyvsp[0].node_list));
    }
#line 2416 "src/parser/formula_parser.tab.cc"
    break;

  case 63: /* rule_call: EXISTS_INTRO id_list  */
#line 337 "src/parser/formula.y"
                           {
        (yyval.node) = ASTNode::make_rule_step("exists_intro", (yyvsp[0].node_list));
    }
#line 2424 "src/parser/formula_parser.tab.cc"
    break;

  case 64: /* rule_call: EXISTS_ELIM id_list  */
#line 340 "src/parser/formula.y"
                          {
        (yyval.node) = ASTNode::make_rule_step("exists_elim", (yyvsp[0].node_list));
    }
#line 2432 "src/parser/formula_parser.tab.cc"
    break;

  case 65: /* rule_call: DOUBLE_NEG_ELIM IDENTIFIER  */
#line 343 "src/parser/formula.y"
                                 {
        auto* args = new std::vector<ASTNode*>();
        args->push_back(new ASTNode(ASTNode::Term, *(yyvsp[0].str)));
        (yyval.node) = ASTNode::make_rule_step("double_neg_elim", args);
        delete (yyvsp[0].str);
    }
#line 2443 "src/parser/formula_parser.tab.cc"
    break;

  case 66: /* rule_call: EXCLUDED_MIDDLE IDENTIFIER  */
#line 349 "src/parser/formula.y"
                                 {
        auto* args = new std::vector<ASTNode*>();
        args->push_back(new ASTNode(ASTNode::Term, *(yyvsp[0].str)));
        (yyval.node) = ASTNode::make_rule_step("excluded_middle", args);
        delete (yyvsp[0].str);
    }
#line 2454 "src/parser/formula_parser.tab.cc"
    break;

  case 67: /* rule_call: EQ_SUBST id_list  */
#line 355 "src/parser/formula.y"
                       {
        (yyval.node) = ASTNode::make_rule_step("eq_subst", (yyvsp[0].node_list));
    }
#line 2462 "src/parser/formula_parser.tab.cc"
    break;

  case 68: /* rule_call: EQ_SYM id_list  */
#line 358 "src/parser/formula.y"
                     {
        (yyval.node) = ASTNode::make_rule_step("eq_sym", (yyvsp[0].node_list));
    }
#line 2470 "src/parser/formula_parser.tab.cc"
    break;

  case 69: /* rule_call: IOTA_ELIM id_list  */
#line 361 "src/parser/formula.y"
                        {
        (yyval.node) = ASTNode::make_rule_step("iota_elim", (yyvsp[0].node_list));
    }
#line 2478 "src/parser/formula_parser.tab.cc"
    break;

  case 70: /* rule_call: SCHEMA_INST IDENTIFIER LBRACE schema_binding_list RBRACE  */
#line 364 "src/parser/formula.y"
                                                               {
        (yyval.node) = ASTNode::make_schema_inst_step(*(yyvsp[-3].str), (yyvsp[-1].node_list));
        delete (yyvsp[-3].str);
    }
#line 2487 "src/parser/formula_parser.tab.cc"
    break;

  case 71: /* id_list: IDENTIFIER  */
#line 371 "src/parser/formula.y"
                 {
        (yyval.node_list) = new std::vector<ASTNode*>();
        (yyval.node_list)->push_back(new ASTNode(ASTNode::Term, *(yyvsp[0].str)));
        delete (yyvsp[0].str);
    }
#line 2497 "src/parser/formula_parser.tab.cc"
    break;

  case 72: /* id_list: id_list COMMA IDENTIFIER  */
#line 376 "src/parser/formula.y"
                               {
        (yyval.node_list) = (yyvsp[-2].node_list);
        (yyval.node_list)->push_back(new ASTNode(ASTNode::Term, *(yyvsp[0].str)));
        delete (yyvsp[0].str);
    }
#line 2507 "src/parser/formula_parser.tab.cc"
    break;

  case 73: /* schema_binding_list: schema_binding  */
#line 384 "src/parser/formula.y"
                     {
        (yyval.node_list) = new std::vector<ASTNode*>();
        (yyval.node_list)->push_back((yyvsp[0].node));
    }
#line 2516 "src/parser/formula_parser.tab.cc"
    break;

  case 74: /* schema_binding_list: schema_binding_list COMMA schema_binding  */
#line 388 "src/parser/formula.y"
                                               {
        (yyval.node_list) = (yyvsp[-2].node_list);
        (yyval.node_list)->push_back((yyvsp[0].node));
    }
#line 2525 "src/parser/formula_parser.tab.cc"
    break;

  case 75: /* schema_var_list: schema_var_decl  */
#line 395 "src/parser/formula.y"
                      {
        (yyval.node_list) = new std::vector<ASTNode*>();
        (yyval.node_list)->push_back((yyvsp[0].node));
    }
#line 2534 "src/parser/formula_parser.tab.cc"
    break;

  case 76: /* schema_var_list: schema_var_list COMMA schema_var_decl  */
#line 399 "src/parser/formula.y"
                                            {
        (yyval.node_list) = (yyvsp[-2].node_list);
        (yyval.node_list)->push_back((yyvsp[0].node));
    }
#line 2543 "src/parser/formula_parser.tab.cc"
    break;

  case 77: /* schema_var_decl: IDENTIFIER  */
#line 406 "src/parser/formula.y"
                 {
        /* Arity 0 (formula-level, backward compatible) */
        (yyval.node) = new ASTNode(ASTNode::Term, *(yyvsp[0].str));
        (yyval.node)->rule_name = "0";
        delete (yyvsp[0].str);
    }
#line 2554 "src/parser/formula_parser.tab.cc"
    break;

  case 78: /* schema_var_decl: IDENTIFIER LPAREN NUMBER RPAREN  */
#line 412 "src/parser/formula.y"
                                      {
        /* Arity N (predicate-level): P(1), R(2), etc. */
        (yyval.node) = new ASTNode(ASTNode::Term, *(yyvsp[-3].str));
        (yyval.node)->rule_name = *(yyvsp[-1].str);
        delete (yyvsp[-3].str); delete (yyvsp[-1].str);
    }
#line 2565 "src/parser/formula_parser.tab.cc"
    break;

  case 79: /* schema_binding: IDENTIFIER COLON formula  */
#line 421 "src/parser/formula.y"
                               {
        /* Arity-0 binding: var_name: formula */
        (yyval.node) = new ASTNode(ASTNode::Term, *(yyvsp[-2].str));
        (yyval.node)->body = (yyvsp[0].node);
        delete (yyvsp[-2].str);
    }
#line 2576 "src/parser/formula_parser.tab.cc"
    break;

  case 80: /* schema_binding: IDENTIFIER COLON BACKSLASH lambda_params DOT formula  */
#line 427 "src/parser/formula.y"
                                                           {
        /* Arity-N binding: var_name: \x y. formula */
        (yyval.node) = new ASTNode(ASTNode::Term, *(yyvsp[-5].str));
        (yyval.node)->body = (yyvsp[0].node);
        (yyval.node)->args = (yyvsp[-2].node_list);
        delete (yyvsp[-5].str);
    }
#line 2588 "src/parser/formula_parser.tab.cc"
    break;

  case 81: /* lambda_params: IDENTIFIER  */
#line 437 "src/parser/formula.y"
                 {
        (yyval.node_list) = new std::vector<ASTNode*>();
        (yyval.node_list)->push_back(new ASTNode(ASTNode::Term, *(yyvsp[0].str)));
        delete (yyvsp[0].str);
    }
#line 2598 "src/parser/formula_parser.tab.cc"
    break;

  case 82: /* lambda_params: lambda_params IDENTIFIER  */
#line 442 "src/parser/formula.y"
                               {
        (yyval.node_list) = (yyvsp[-1].node_list);
        (yyval.node_list)->push_back(new ASTNode(ASTNode::Term, *(yyvsp[0].str)));
        delete (yyvsp[0].str);
    }
#line 2608 "src/parser/formula_parser.tab.cc"
    break;


#line 2612 "src/parser/formula_parser.tab.cc"

      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;
  *++yylsp = yyloc;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      {
        yypcontext_t yyctx
          = {yyps, yytoken, &yylloc};
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = yysyntax_error (&yymsg_alloc, &yymsg, &yyctx);
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == -1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = YY_CAST (char *,
                             YYSTACK_ALLOC (YY_CAST (YYSIZE_T, yymsg_alloc)));
            if (yymsg)
              {
                yysyntax_error_status
                  = yysyntax_error (&yymsg_alloc, &yymsg, &yyctx);
                yymsgp = yymsg;
              }
            else
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = YYENOMEM;
              }
          }
        yyerror (&yylloc, scanner, ctx, yymsgp);
        if (yysyntax_error_status == YYENOMEM)
          YYNOMEM;
      }
    }

  yyerror_range[1] = yylloc;
  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval, &yylloc, scanner, ctx);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;
  ++yynerrs;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;

      yyerror_range[1] = *yylsp;
      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp, yylsp, scanner, ctx);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  yyerror_range[2] = yylloc;
  ++yylsp;
  YYLLOC_DEFAULT (*yylsp, yyerror_range, 2);

  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturnlab;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturnlab;


/*-----------------------------------------------------------.
| yyexhaustedlab -- YYNOMEM (memory exhaustion) comes here.  |
`-----------------------------------------------------------*/
yyexhaustedlab:
  yyerror (&yylloc, scanner, ctx, YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;


/*----------------------------------------------------------.
| yyreturnlab -- parsing is finished, clean up and return.  |
`----------------------------------------------------------*/
yyreturnlab:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, &yylloc, scanner, ctx);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp, yylsp, scanner, ctx);
      YYPOPSTACK (1);
    }
  yyps->yynew = 2;
  goto yypushreturn;


/*-------------------------.
| yypushreturn -- return.  |
`-------------------------*/
yypushreturn:
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
  return yyresult;
}
#undef yynerrs
#undef yystate
#undef yyerrstatus
#undef yyssa
#undef yyss
#undef yyssp
#undef yyvsa
#undef yyvs
#undef yyvsp
#undef yylsa
#undef yyls
#undef yylsp
#undef yystacksize
#line 449 "src/parser/formula.y"


void yyerror(YYLTYPE* loc, yyscan_t scanner, ParseContext* ctx, const char* msg) {
    (void)scanner;
    ctx->error = msg;
    ctx->error_col = loc->first_column;
}
