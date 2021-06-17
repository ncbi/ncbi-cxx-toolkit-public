
/* A Bison parser, made by GNU Bison 2.4.1.  */

/* Skeleton implementation for Bison's Yacc-like parsers in C
   
      Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

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

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.4.1"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Using locations.  */
#define YYLSP_NEEDED 0

/* Substitute the variable and function names.  */
#define yyparse         ncbi_q_parse
#define yylex           ncbi_q_lex
#define yyerror         ncbi_q_error
#define yylval          ncbi_q_lval
#define yychar          ncbi_q_char
#define yydebug         ncbi_q_debug
#define yynerrs         ncbi_q_nerrs


/* Copy the first part of user declarations.  */

/* Line 189 of yacc.c  */
#line 33 "query_parser_bison.y"


#define YYSTYPE         CQueryParseTree::TNode*
#define YYPARSE_PARAM   parm
#define YYLEX_PARAM     parm
#define YYMAXDEPTH      100000

#define YYDEBUG         1
#define YYERROR_VERBOSE 1


/// Add child node(s) to the parent
/// @internal
///
inline static
void QTreeAddNode(void*                   parm,
                  CQueryParseTree::TNode* rnode,
                  CQueryParseTree::TNode* node1,
                  CQueryParseTree::TNode* node2)
{
    if (node1)
        rnode->AddNode(node1);
    if (node2)
        rnode->AddNode(node2);
        
    CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
    env->AttachQueryTree(rnode);    
    env->ForgetPoolNodes(node1, node2);
}


/// Add node to a list if parser environment is in the list parsing context
/// @internal
///
inline static
void AddFunc_Arg(void*                   parm,
               CQueryParseTree::TNode* node)
{
    CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
    CQueryParseTree::TNode* func_node = env->GetContext();
    
    if (func_node) {
        func_node->AddNode(node);
    } 
    env->ForgetPoolNodes(node, 0);
}

inline static
void AddIn_Arg(void*                   parm,
               CQueryParseTree::TNode* node)
{
    CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
    CQueryParseTree::TNode* in_node = env->GetIN_Context();
    
    if (in_node) {
        in_node->AddNode(node);
    } 
    env->ForgetPoolNodes(node, 0);
}



/* Line 189 of yacc.c  */
#line 144 "query_parser_bison.tab.c"

/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     IDENT = 258,
     STRING = 259,
     NUM_INT = 260,
     SELECT = 261,
     FROM = 262,
     WHERE = 263,
     FUNCTION = 264,
     AND = 265,
     NOT = 266,
     OR = 267,
     SUB = 268,
     XOR = 269,
     RANGE = 270,
     EQ = 271,
     NOTEQ = 272,
     GT = 273,
     GE = 274,
     LT = 275,
     LE = 276,
     BETWEEN = 277,
     NOT_BETWEEN = 278,
     LIKE = 279,
     NOT_LIKE = 280,
     IN = 281,
     NOT_IN = 282
   };
#endif



#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef int YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif


/* Copy the second part of user declarations.  */


/* Line 264 of yacc.c  */
#line 213 "query_parser_bison.tab.c"

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(e) ((void) (e))
#else
# define YYUSE(e) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(n) (n)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int yyi)
#else
static int
YYID (yyi)
    int yyi;
#endif
{
  return yyi;
}
#endif

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef _STDLIB_H
#      define _STDLIB_H 1
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
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
#  if (defined __cplusplus && ! defined _STDLIB_H \
       && ! ((defined YYMALLOC || defined malloc) \
         && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef _STDLIB_H
#    define _STDLIB_H 1
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
     || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)        \
      do                    \
    {                    \
      YYSIZE_T yyi;                \
      for (yyi = 0; yyi < (Count); yyi++)    \
        (To)[yyi] = (From)[yyi];        \
    }                    \
      while (YYID (0))
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                \
    do                                    \
      {                                    \
    YYSIZE_T yynewbytes;                        \
    YYCOPY (&yyptr->Stack_alloc, Stack, yysize);            \
    Stack = &yyptr->Stack_alloc;                    \
    yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
    yyptr += yynewbytes / sizeof (*yyptr);                \
      }                                    \
    while (YYID (0))

#endif

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  25
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   589

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  31
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  11
/* YYNRULES -- Number of rules.  */
#define YYNRULES  53
/* YYNRULES -- Number of states.  */
#define YYNSTATES  96

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   282

#define YYTRANSLATE(YYX)                        \
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      28,    29,     2,     2,    30,     2,     2,     2,     2,     2,
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
      25,    26,    27
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint8 yyprhs[] =
{
       0,     0,     3,     5,     7,    13,    14,    17,    22,    26,
      28,    32,    34,    36,    38,    40,    42,    46,    48,    52,
      54,    56,    58,    60,    63,    66,    70,    74,    78,    82,
      86,    92,    98,   102,   106,   110,   116,   122,   126,   130,
     134,   138,   142,   146,   149,   152,   155,   158,   161,   165,
     169,   173,   177,   181
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int8 yyrhs[] =
{
      32,     0,    -1,    41,    -1,    33,    -1,     6,    36,     7,
      36,    34,    -1,    -1,     8,    41,    -1,     9,    28,    39,
      29,    -1,     9,    28,    29,    -1,    37,    -1,    36,    30,
      37,    -1,     5,    -1,     4,    -1,     3,    -1,    35,    -1,
      37,    -1,    38,    30,    37,    -1,    41,    -1,    39,    30,
      41,    -1,    38,    -1,    33,    -1,    37,    -1,    35,    -1,
       4,     3,    -1,    41,    41,    -1,    28,    41,    29,    -1,
      41,    10,    41,    -1,    41,    13,    41,    -1,    41,    12,
      41,    -1,    41,    14,    41,    -1,    37,    22,    37,    10,
      37,    -1,    37,    23,    37,    10,    37,    -1,    41,    15,
      41,    -1,    37,    24,    37,    -1,    37,    25,    37,    -1,
      37,    26,    28,    40,    29,    -1,    37,    27,    28,    40,
      29,    -1,    41,    16,    41,    -1,    41,    17,    41,    -1,
      41,    18,    41,    -1,    41,    19,    41,    -1,    41,    20,
      41,    -1,    41,    21,    41,    -1,    11,    41,    -1,     1,
       4,    -1,     1,     3,    -1,     1,     5,    -1,    41,     1,
      -1,    28,    41,     1,    -1,    41,    12,     1,    -1,    41,
      14,     1,    -1,    41,    11,     1,    -1,    41,    10,     1,
      -1,    41,    13,     1,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   128,   128,   129,   136,   176,   180,   190,   199,   210,
     214,   226,   231,   236,   240,   247,   251,   259,   263,   272,
     274,   278,   283,   288,   296,   307,   317,   322,   327,   332,
     337,   350,   364,   369,   374,   379,   390,   401,   406,   410,
     414,   418,   422,   433,   442,   451,   460,   469,   481,   491,
     501,   511,   521,   531
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "IDENT", "STRING", "NUM_INT", "SELECT",
  "FROM", "WHERE", "FUNCTION", "AND", "NOT", "OR", "SUB", "XOR", "RANGE",
  "EQ", "NOTEQ", "GT", "GE", "LT", "LE", "BETWEEN", "NOT_BETWEEN", "LIKE",
  "NOT_LIKE", "IN", "NOT_IN", "'('", "')'", "','", "$accept", "input",
  "select_clause", "opt_where", "functional", "obj_list", "scalar_value",
  "scalar_list", "exp_list", "in_sub_expr", "exp", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,    40,    41,
      44
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    31,    32,    32,    33,    34,    34,    35,    35,    36,
      36,    37,    37,    37,    37,    38,    38,    39,    39,    40,
      40,    41,    41,    41,    41,    41,    41,    41,    41,    41,
      41,    41,    41,    41,    41,    41,    41,    41,    41,    41,
      41,    41,    41,    41,    41,    41,    41,    41,    41,    41,
      41,    41,    41,    41
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     1,     1,     5,     0,     2,     4,     3,     1,
       3,     1,     1,     1,     1,     1,     3,     1,     3,     1,
       1,     1,     1,     2,     2,     3,     3,     3,     3,     3,
       5,     5,     3,     3,     3,     5,     5,     3,     3,     3,
       3,     3,     3,     2,     2,     2,     2,     2,     3,     3,
       3,     3,     3,     3
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,     0,    13,    12,    11,     0,     0,     0,     0,     0,
       3,    14,    21,     0,    45,    44,    46,    23,    12,    14,
       0,     9,     0,     0,     0,     1,     0,     0,     0,     0,
       0,     0,    47,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     8,     0,
       0,    47,    25,     0,     0,    33,    34,     0,     0,    52,
       0,    51,    49,     0,    53,     0,    50,     0,     0,     0,
       0,     0,     0,     0,     0,     5,    10,     7,     0,     0,
       0,    20,    15,    19,     0,     0,     0,     4,     0,    30,
      31,     0,    35,    36,     0,    16
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int8 yydefgoto[] =
{
      -1,     9,    81,    87,    11,    20,    12,    83,    49,    84,
      45
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -26
static const yytype_int16 yypact[] =
{
     503,    46,   -26,    -1,   -26,    62,   -19,   512,   512,    14,
     -26,   -26,    71,   461,   -26,   -26,   -26,   -26,   -26,   -26,
      -6,   -26,    59,   109,   482,   -26,    62,    62,    62,    62,
     -11,    -7,    46,   521,   532,   541,   550,   561,   512,   512,
     512,   512,   512,   512,   512,   131,    62,    62,   -26,   -25,
     395,    15,   -26,    13,    16,   -26,   -26,     7,     7,    46,
     153,    46,    46,   175,    46,   197,    46,   219,   241,   263,
     285,   307,   329,   351,   373,    -5,   -26,   -26,   512,    62,
      62,   -26,   -26,     2,    17,    27,   512,   -26,   417,   -26,
     -26,    62,   -26,   -26,   439,   -26
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
     -26,   -26,    57,   -26,     1,    23,    26,   -26,   -26,     3,
       0
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -49
static const yytype_int8 yytable[] =
{
      13,    46,    17,    86,    77,    78,    19,    23,    24,    22,
       2,    18,     4,     5,    25,   -48,     6,    57,    14,    15,
      16,    58,    50,    79,    47,    47,    80,    19,    19,    19,
      19,    21,    91,    60,    23,    63,    65,    67,    68,    69,
      70,    71,    72,    73,    74,   -48,    92,    19,    19,    14,
      15,    16,    53,    54,    55,    56,    93,    10,    19,    19,
       1,    85,     2,     3,     4,     2,    18,     4,     6,    75,
       7,     6,    21,    76,     0,     0,     0,     0,    88,     0,
      19,    19,     0,    82,    82,     0,    94,     8,    48,     0,
       0,     0,    19,    26,    27,    28,    29,    30,    31,     0,
       0,     0,     0,     0,     0,    89,    90,     0,     0,   -43,
      32,     0,     2,     3,     4,     0,     0,    95,     6,   -43,
     -43,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,   -24,    32,     0,     2,     3,     4,     8,   -43,   -43,
       6,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,   -26,    32,     0,     2,     3,     4,     8,
     -24,   -24,     6,   -26,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,   -28,    32,     0,     2,     3,
       4,     8,   -26,   -26,     6,   -28,   -28,   -28,    36,    37,
      38,    39,    40,    41,    42,    43,    44,   -27,    32,     0,
       2,     3,     4,     8,   -28,   -28,     6,   -27,   -27,   -27,
     -27,    37,    38,    39,    40,    41,    42,    43,    44,   -29,
      32,     0,     2,     3,     4,     8,   -27,   -27,     6,   -29,
     -29,   -29,   -29,   -29,    38,    39,    40,    41,    42,    43,
      44,   -32,    32,     0,     2,     3,     4,     8,   -29,   -29,
       6,   -32,   -32,   -32,   -32,   -32,   -32,    39,    40,    41,
      42,    43,    44,   -37,    32,     0,     2,     3,     4,     8,
     -32,   -32,     6,   -37,   -37,   -37,   -37,   -37,   -37,   -37,
      40,    41,    42,    43,    44,   -38,    32,     0,     2,     3,
       4,     8,   -37,   -37,     6,   -38,   -38,   -38,   -38,   -38,
     -38,   -38,   -38,    41,    42,    43,    44,   -39,    32,     0,
       2,     3,     4,     8,   -38,   -38,     6,   -39,   -39,   -39,
     -39,   -39,   -39,   -39,   -39,   -39,    42,    43,    44,   -40,
      32,     0,     2,     3,     4,     8,   -39,   -39,     6,   -40,
     -40,   -40,   -40,   -40,   -40,   -40,   -40,   -40,   -40,    43,
      44,   -41,    32,     0,     2,     3,     4,     8,   -40,   -40,
       6,   -41,   -41,   -41,   -41,   -41,   -41,   -41,   -41,   -41,
     -41,   -41,    44,   -42,    32,     0,     2,     3,     4,     8,
     -41,   -41,     6,   -42,   -42,   -42,   -42,   -42,   -42,   -42,
     -42,   -42,   -42,   -42,   -42,     0,    32,     0,     2,     3,
       4,     8,   -42,   -42,     6,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,     0,    32,     0,
       2,     3,     4,     8,   -17,   -17,     6,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    -6,
      32,     0,     2,     3,     4,     8,   -18,   -18,     6,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    -2,    32,     0,     2,     3,     4,     8,    -6,     0,
       6,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    51,     0,     2,     3,     4,     0,     8,
       0,     6,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,     1,     0,     2,     3,     4,     5,
       8,    52,     6,     1,     7,     2,     3,     4,     0,     0,
       0,     6,    59,     7,     2,     3,     4,     0,     0,     0,
       6,     8,     7,    61,     0,     2,     3,     4,     0,     0,
       8,     6,    62,     7,     2,     3,     4,     0,     0,     8,
       6,    64,     7,     2,     3,     4,     0,     0,     0,     6,
       8,     7,    66,     0,     2,     3,     4,     0,     0,     8,
       6,     0,     7,     0,     0,     0,     0,     0,     8,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     8
};

static const yytype_int8 yycheck[] =
{
       0,     7,     3,     8,    29,    30,     5,     7,     8,    28,
       3,     4,     5,     6,     0,     0,     9,    28,     3,     4,
       5,    28,    22,    10,    30,    30,    10,    26,    27,    28,
      29,     5,    30,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    30,    29,    46,    47,     3,
       4,     5,    26,    27,    28,    29,    29,     0,    57,    58,
       1,    58,     3,     4,     5,     3,     4,     5,     9,    46,
      11,     9,    46,    47,    -1,    -1,    -1,    -1,    78,    -1,
      79,    80,    -1,    57,    58,    -1,    86,    28,    29,    -1,
      -1,    -1,    91,    22,    23,    24,    25,    26,    27,    -1,
      -1,    -1,    -1,    -1,    -1,    79,    80,    -1,    -1,     0,
       1,    -1,     3,     4,     5,    -1,    -1,    91,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,     0,     1,    -1,     3,     4,     5,    28,    29,    30,
       9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,    21,     0,     1,    -1,     3,     4,     5,    28,
      29,    30,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    21,     0,     1,    -1,     3,     4,
       5,    28,    29,    30,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,     0,     1,    -1,
       3,     4,     5,    28,    29,    30,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,     0,
       1,    -1,     3,     4,     5,    28,    29,    30,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,     0,     1,    -1,     3,     4,     5,    28,    29,    30,
       9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,    21,     0,     1,    -1,     3,     4,     5,    28,
      29,    30,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    21,     0,     1,    -1,     3,     4,
       5,    28,    29,    30,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,     0,     1,    -1,
       3,     4,     5,    28,    29,    30,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,     0,
       1,    -1,     3,     4,     5,    28,    29,    30,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,     0,     1,    -1,     3,     4,     5,    28,    29,    30,
       9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,    21,     0,     1,    -1,     3,     4,     5,    28,
      29,    30,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    -1,     1,    -1,     3,     4,
       5,    28,    29,    30,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    -1,     1,    -1,
       3,     4,     5,    28,    29,    30,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,     0,
       1,    -1,     3,     4,     5,    28,    29,    30,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,     0,     1,    -1,     3,     4,     5,    28,    29,    -1,
       9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,    21,     1,    -1,     3,     4,     5,    -1,    28,
      -1,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,     1,    -1,     3,     4,     5,     6,
      28,    29,     9,     1,    11,     3,     4,     5,    -1,    -1,
      -1,     9,     1,    11,     3,     4,     5,    -1,    -1,    -1,
       9,    28,    11,     1,    -1,     3,     4,     5,    -1,    -1,
      28,     9,     1,    11,     3,     4,     5,    -1,    -1,    28,
       9,     1,    11,     3,     4,     5,    -1,    -1,    -1,     9,
      28,    11,     1,    -1,     3,     4,     5,    -1,    -1,    28,
       9,    -1,    11,    -1,    -1,    -1,    -1,    -1,    28,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    28
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     1,     3,     4,     5,     6,     9,    11,    28,    32,
      33,    35,    37,    41,     3,     4,     5,     3,     4,    35,
      36,    37,    28,    41,    41,     0,    22,    23,    24,    25,
      26,    27,     1,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    41,     7,    30,    29,    39,
      41,     1,    29,    37,    37,    37,    37,    28,    28,     1,
      41,     1,     1,    41,     1,    41,     1,    41,    41,    41,
      41,    41,    41,    41,    41,    36,    37,    29,    30,    10,
      10,    33,    37,    38,    40,    40,     8,    34,    41,    37,
      37,    30,    29,    29,    41,    37
};

#define yyerrok        (yyerrstatus = 0)
#define yyclearin    (yychar = YYEMPTY)
#define YYEMPTY        (-2)
#define YYEOF        0

#define YYACCEPT    goto yyacceptlab
#define YYABORT        goto yyabortlab
#define YYERROR        goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */

#define YYFAIL        goto yyerrlab

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                    \
do                                \
  if (yychar == YYEMPTY && yylen == 1)                \
    {                                \
      yychar = (Token);                        \
      yylval = (Value);                        \
      yytoken = YYTRANSLATE (yychar);                \
      YYPOPSTACK (1);                        \
      goto yybackup;                        \
    }                                \
  else                                \
    {                                \
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;                            \
    }                                \
while (YYID (0))


#define YYTERROR    1
#define YYERRCODE    256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)                \
    do                                    \
      if (YYID (N))                                                    \
    {                                \
      (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;    \
      (Current).first_column = YYRHSLOC (Rhs, 1).first_column;    \
      (Current).last_line    = YYRHSLOC (Rhs, N).last_line;        \
      (Current).last_column  = YYRHSLOC (Rhs, N).last_column;    \
    }                                \
      else                                \
    {                                \
      (Current).first_line   = (Current).last_line   =        \
        YYRHSLOC (Rhs, 0).last_line;                \
      (Current).first_column = (Current).last_column =        \
        YYRHSLOC (Rhs, 0).last_column;                \
    }                                \
    while (YYID (0))
#endif


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if YYLTYPE_IS_TRIVIAL
#  define YY_LOCATION_PRINT(File, Loc)            \
     fprintf (File, "%d.%d-%d.%d",            \
          (Loc).first_line, (Loc).first_column,    \
          (Loc).last_line,  (Loc).last_column)
# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (&yylval, YYLEX_PARAM)
#else
# define YYLEX yylex (&yylval)
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)            \
do {                        \
  if (yydebug)                    \
    YYFPRINTF Args;                \
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)              \
do {                                      \
  if (yydebug)                                  \
    {                                      \
      YYFPRINTF (stderr, "%s ", Title);                      \
      yy_symbol_print (stderr,                          \
          Type, Value); \
      YYFPRINTF (stderr, "\n");                          \
    }                                      \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
    break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
#else
static void
yy_stack_print (yybottom, yytop)
    yytype_int16 *yybottom;
    yytype_int16 *yytop;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                \
do {                                \
  if (yydebug)                            \
    yy_stack_print ((Bottom), (Top));                \
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, int yyrule)
#else
static void
yy_reduce_print (yyvsp, yyrule)
    YYSTYPE *yyvsp;
    int yyrule;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
         yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
               &(yyvsp[(yyi + 1) - (yynrhs)])
                              );
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)        \
do {                    \
  if (yydebug)                \
    yy_reduce_print (yyvsp, Rule); \
} while (YYID (0))

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef    YYINITDEPTH
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



#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
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
        /* Fall through.  */
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

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into YYRESULT an error message about the unexpected token
   YYCHAR while in state YYSTATE.  Return the number of bytes copied,
   including the terminating null byte.  If YYRESULT is null, do not
   copy anything; just return the number of bytes that would be
   copied.  As a special case, return 0 if an ordinary "syntax error"
   message will do.  Return YYSIZE_MAXIMUM if overflow occurs during
   size calculation.  */
static YYSIZE_T
yysyntax_error (char *yyresult, int yystate, int yychar)
{
  int yyn = yypact[yystate];

  if (! (YYPACT_NINF < yyn && yyn <= YYLAST))
    return 0;
  else
    {
      int yytype = YYTRANSLATE (yychar);
      YYSIZE_T yysize0 = yytnamerr (0, yytname[yytype]);
      YYSIZE_T yysize = yysize0;
      YYSIZE_T yysize1;
      int yysize_overflow = 0;
      enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
      char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
      int yyx;

# if 0
      /* This is so xgettext sees the translatable formats that are
     constructed on the fly.  */
      YY_("syntax error, unexpected %s");
      YY_("syntax error, unexpected %s, expecting %s");
      YY_("syntax error, unexpected %s, expecting %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s");
# endif
      char *yyfmt;
      char const *yyf;
      static char const yyunexpected[] = "syntax error, unexpected %s";
      static char const yyexpecting[] = ", expecting %s";
      static char const yyor[] = " or %s";
      char yyformat[sizeof yyunexpected
            + sizeof yyexpecting - 1
            + ((YYERROR_VERBOSE_ARGS_MAXIMUM - 2)
               * (sizeof yyor - 1))];
      char const *yyprefix = yyexpecting;

      /* Start YYX at -YYN if negative to avoid negative indexes in
     YYCHECK.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;

      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yycount = 1;

      yyarg[0] = yytname[yytype];
      yyfmt = yystpcpy (yyformat, yyunexpected);

      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
    if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
      {
        if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
          {
        yycount = 1;
        yysize = yysize0;
        yyformat[sizeof yyunexpected - 1] = '\0';
        break;
          }
        yyarg[yycount++] = yytname[yyx];
        yysize1 = yysize + yytnamerr (0, yytname[yyx]);
        yysize_overflow |= (yysize1 < yysize);
        yysize = yysize1;
        yyfmt = yystpcpy (yyfmt, yyprefix);
        yyprefix = yyor;
      }

      yyf = YY_(yyformat);
      yysize1 = yysize + yystrlen (yyf);
      yysize_overflow |= (yysize1 < yysize);
      yysize = yysize1;

      if (yysize_overflow)
    return YYSIZE_MAXIMUM;

      if (yyresult)
    {
      /* Avoid sprintf, as that infringes on the user's name space.
         Don't have undefined behavior even if the translation
         produced a string with the wrong number of "%s"s.  */
      char *yyp = yyresult;
      int yyi = 0;
      while ((*yyp = *yyf) != '\0')
        {
          if (*yyp == '%' && yyf[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyf += 2;
        }
          else
        {
          yyp++;
          yyf++;
        }
        }
    }
      return yysize;
    }
}
#endif /* YYERROR_VERBOSE */


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
#else
static void
yydestruct (yymsg, yytype, yyvaluep)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  YYUSE (yyvaluep);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
    break;
    }
}

/* Prevent warnings from -Wmissing-prototypes.  */
#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (void);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */





/*-------------------------.
| yyparse or yypush_parse.  |
`-------------------------*/

#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void)
#else
int
yyparse ()

#endif
#endif
{
/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;

    /* Number of syntax errors so far.  */
    int yynerrs;

    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       `yyss': related to states.
       `yyvs': related to semantic values.

       Refer to the stacks thru separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yytoken = 0;
  yyss = yyssa;
  yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */
  yyssp = yyss;
  yyvsp = yyvs;

  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
    /* Give user a chance to reallocate the stack.  Use copies of
       these so that the &'s don't force the real ones into
       memory.  */
    YYSTYPE *yyvs1 = yyvs;
    yytype_int16 *yyss1 = yyss;

    /* Each stack pointer address is followed by the size of the
       data in use in that stack, in bytes.  This used to be a
       conditional around just the two extra args, but that might
       be undefined if yyoverflow is a macro.  */
    yyoverflow (YY_("memory exhausted"),
            &yyss1, yysize * sizeof (*yyssp),
            &yyvs1, yysize * sizeof (*yyvsp),
            &yystacksize);

    yyss = yyss1;
    yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
    goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
    yystacksize = YYMAXDEPTH;

      {
    yytype_int16 *yyss1 = yyss;
    union yyalloc *yyptr =
      (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
    if (! yyptr)
      goto yyexhaustedlab;
    YYSTACK_RELOCATE (yyss_alloc, yyss);
    YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
    if (yyss1 != yyssa)
      YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
          (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
    YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

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
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
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
      if (yyn == 0 || yyn == YYTABLE_NINF)
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

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  *++yyvsp = yylval;

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
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 4:

/* Line 1455 of yacc.c  */
#line 137 "query_parser_bison.y"
    {
    CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);

    /* create a pseudo node to isolate select list in a dedicated subtree */
    //CQueryParseTree::TNode* qnode = 0;
    //qnode = env->QTree().CreateNode(CQueryParseNode::eList, 0, 0, "LIST");    
    //qnode->MoveSubnodes($2);
    CQueryParseTree::TNode* qnode = env->GetContext();
    if (qnode) {
        qnode->InsertNode(qnode->SubNodeBegin(), (yyvsp[(2) - (5)]));
    }

    //$1->InsertNode($1->SubNodeBegin(),qnode);                
    
    (yyvsp[(1) - (5)])->AddNode((yyvsp[(3) - (5)]));
    (yyvsp[(3) - (5)])->MoveSubnodes((yyvsp[(4) - (5)]));
    (yyvsp[(3) - (5)])->InsertNode((yyvsp[(3) - (5)])->SubNodeBegin(), (yyvsp[(4) - (5)]));    
    if ((yyvsp[(5) - (5)])) {
        (yyvsp[(1) - (5)])->AddNode((yyvsp[(5) - (5)]));
    }
    
    env->ForgetPoolNode((yyvsp[(1) - (5)]));
    env->ForgetPoolNode((yyvsp[(2) - (5)]));
    env->ForgetPoolNode((yyvsp[(3) - (5)]));
    env->ForgetPoolNode((yyvsp[(4) - (5)]));
    env->ForgetPoolNode((yyvsp[(5) - (5)]));
    
    env->SetSELECT_Context(0);
    env->SetContext(0);
    env->SetFROM_Context(0);
    
    (yyval) = (yyvsp[(1) - (5)]);
    env->AttachQueryTree((yyval));
    
    ;}
    break;

  case 5:

/* Line 1455 of yacc.c  */
#line 176 "query_parser_bison.y"
    {
    (yyval)=0;
    ;}
    break;

  case 6:

/* Line 1455 of yacc.c  */
#line 181 "query_parser_bison.y"
    {
    CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
    (yyvsp[(1) - (2)])->AddNode((yyvsp[(2) - (2)]));
    env->ForgetPoolNode((yyvsp[(2) - (2)]));
    (yyval) = (yyvsp[(1) - (2)]);
    ;}
    break;

  case 7:

/* Line 1455 of yacc.c  */
#line 191 "query_parser_bison.y"
    {
        (yyval) = (yyvsp[(1) - (4)]);
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        env->AttachQueryTree((yyval));    
        (yyval)->InsertNode((yyval)->SubNodeBegin(), (yyvsp[(3) - (4)]));
        env->SetContext(0);
        env->ForgetPoolNodes((yyvsp[(1) - (4)]), (yyvsp[(3) - (4)]));    
    ;}
    break;

  case 8:

/* Line 1455 of yacc.c  */
#line 200 "query_parser_bison.y"
    {
        (yyval) = (yyvsp[(1) - (3)]);
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        env->AttachQueryTree((yyval));
        env->SetContext(0);
        env->ForgetPoolNode((yyvsp[(1) - (3)]));
    ;}
    break;

  case 9:

/* Line 1455 of yacc.c  */
#line 211 "query_parser_bison.y"
    {
        (yyval) = (yyvsp[(1) - (1)]);
    ;}
    break;

  case 10:

/* Line 1455 of yacc.c  */
#line 215 "query_parser_bison.y"
    {
        (yyval) = (yyvsp[(1) - (3)]);
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        AddFunc_Arg(parm, (yyvsp[(3) - (3)]));        
        //$$->AddNode($3);
        env->ForgetPoolNode((yyvsp[(3) - (3)]));
    ;}
    break;

  case 11:

/* Line 1455 of yacc.c  */
#line 227 "query_parser_bison.y"
    {
        (yyval) = (yyvsp[(1) - (1)]);         
    ;}
    break;

  case 12:

/* Line 1455 of yacc.c  */
#line 232 "query_parser_bison.y"
    {
        (yyval) = (yyvsp[(1) - (1)]);         
    ;}
    break;

  case 13:

/* Line 1455 of yacc.c  */
#line 237 "query_parser_bison.y"
    {
        (yyval) = (yyvsp[(1) - (1)]);         
    ;}
    break;

  case 14:

/* Line 1455 of yacc.c  */
#line 241 "query_parser_bison.y"
    {
        (yyval) = (yyvsp[(1) - (1)]);  
    ;}
    break;

  case 15:

/* Line 1455 of yacc.c  */
#line 248 "query_parser_bison.y"
    {
        (yyval) = (yyvsp[(1) - (1)]);
    ;}
    break;

  case 16:

/* Line 1455 of yacc.c  */
#line 252 "query_parser_bison.y"
    {
        (yyval) = (yyvsp[(1) - (3)]);    
        AddIn_Arg(parm, (yyvsp[(3) - (3)]));
    ;}
    break;

  case 17:

/* Line 1455 of yacc.c  */
#line 260 "query_parser_bison.y"
    {
        (yyval) = (yyvsp[(1) - (1)]);
    ;}
    break;

  case 18:

/* Line 1455 of yacc.c  */
#line 264 "query_parser_bison.y"
    {
        (yyval) = (yyvsp[(1) - (3)]);    
        AddFunc_Arg(parm, (yyvsp[(3) - (3)]));
    ;}
    break;

  case 21:

/* Line 1455 of yacc.c  */
#line 279 "query_parser_bison.y"
    {
        (yyval) = (yyvsp[(1) - (1)]);
    ;}
    break;

  case 22:

/* Line 1455 of yacc.c  */
#line 284 "query_parser_bison.y"
    {
        (yyval) = (yyvsp[(1) - (1)]);
    ;}
    break;

  case 23:

/* Line 1455 of yacc.c  */
#line 289 "query_parser_bison.y"
    {    
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        (yyval) = env->QTree().CreateNode(CQueryParseNode::eFieldSearch, (yyvsp[(1) - (2)]), (yyvsp[(2) - (2)]));
        env->AttachQueryTree((yyval));
        env->ForgetPoolNodes((yyvsp[(1) - (2)]), (yyvsp[(2) - (2)]));
    ;}
    break;

  case 24:

/* Line 1455 of yacc.c  */
#line 297 "query_parser_bison.y"
    {
        yyerrok;
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        (yyval) = env->QTree().CreateNode(CQueryParseNode::eAnd, (yyvsp[(1) - (2)]), (yyvsp[(2) - (2)]));
        (yyval)->GetValue().SetExplicit(false);
        env->AttachQueryTree((yyval));
        env->ForgetPoolNodes((yyvsp[(1) - (2)]), (yyvsp[(2) - (2)]));
    ;}
    break;

  case 25:

/* Line 1455 of yacc.c  */
#line 308 "query_parser_bison.y"
    {
        (yyval) = (yyvsp[(2) - (3)]);
    ;}
    break;

  case 26:

/* Line 1455 of yacc.c  */
#line 318 "query_parser_bison.y"
    {
        QTreeAddNode(parm, (yyval) = (yyvsp[(2) - (3)]), (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)]));
    ;}
    break;

  case 27:

/* Line 1455 of yacc.c  */
#line 323 "query_parser_bison.y"
    {
        QTreeAddNode(parm, (yyval) = (yyvsp[(2) - (3)]), (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)]));
    ;}
    break;

  case 28:

/* Line 1455 of yacc.c  */
#line 328 "query_parser_bison.y"
    {
        QTreeAddNode(parm, (yyval) = (yyvsp[(2) - (3)]), (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)]));
    ;}
    break;

  case 29:

/* Line 1455 of yacc.c  */
#line 333 "query_parser_bison.y"
    {
        QTreeAddNode(parm, (yyval) = (yyvsp[(2) - (3)]), (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)]));
    ;}
    break;

  case 30:

/* Line 1455 of yacc.c  */
#line 338 "query_parser_bison.y"
    {
        (yyval) = (yyvsp[(2) - (5)]);
        (yyval)->AddNode((yyvsp[(1) - (5)]));
        (yyval)->AddNode((yyvsp[(3) - (5)]));
        (yyval)->AddNode((yyvsp[(5) - (5)]));

        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        env->AttachQueryTree((yyval));    
        env->ForgetPoolNodes((yyvsp[(1) - (5)]), (yyvsp[(3) - (5)]));
        env->ForgetPoolNodes((yyvsp[(1) - (5)]), (yyvsp[(5) - (5)]));
    ;}
    break;

  case 31:

/* Line 1455 of yacc.c  */
#line 351 "query_parser_bison.y"
    {
        (yyval) = (yyvsp[(2) - (5)]);
        (yyval)->AddNode((yyvsp[(1) - (5)]));
        (yyval)->AddNode((yyvsp[(3) - (5)]));
        (yyval)->AddNode((yyvsp[(5) - (5)]));

        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        env->AttachQueryTree((yyval));    
        env->ForgetPoolNodes((yyvsp[(1) - (5)]), (yyvsp[(3) - (5)]));
        env->ForgetPoolNodes((yyvsp[(1) - (5)]), (yyvsp[(5) - (5)]));        
    ;}
    break;

  case 32:

/* Line 1455 of yacc.c  */
#line 365 "query_parser_bison.y"
    {
        QTreeAddNode(parm, (yyval) = (yyvsp[(2) - (3)]), (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)]));
    ;}
    break;

  case 33:

/* Line 1455 of yacc.c  */
#line 370 "query_parser_bison.y"
    {
        QTreeAddNode(parm, (yyval) = (yyvsp[(2) - (3)]), (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)]));
    ;}
    break;

  case 34:

/* Line 1455 of yacc.c  */
#line 375 "query_parser_bison.y"
    {
        QTreeAddNode(parm, (yyval) = (yyvsp[(2) - (3)]), (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)]));
    ;}
    break;

  case 35:

/* Line 1455 of yacc.c  */
#line 380 "query_parser_bison.y"
    {
        (yyval) = (yyvsp[(2) - (5)]);
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        env->AttachQueryTree((yyval));    
        env->ForgetPoolNodes((yyvsp[(1) - (5)]), (yyvsp[(4) - (5)]));
        (yyval)->InsertNode((yyval)->SubNodeBegin(), (yyvsp[(4) - (5)]));
        (yyval)->InsertNode((yyval)->SubNodeBegin(), (yyvsp[(1) - (5)]));
        env->SetIN_Context(0);
    ;}
    break;

  case 36:

/* Line 1455 of yacc.c  */
#line 391 "query_parser_bison.y"
    {
        (yyval) = (yyvsp[(2) - (5)]);
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        env->AttachQueryTree((yyval));    
        env->ForgetPoolNodes((yyvsp[(1) - (5)]), (yyvsp[(4) - (5)]));
        (yyval)->InsertNode((yyval)->SubNodeBegin(), (yyvsp[(4) - (5)]));
        (yyval)->InsertNode((yyval)->SubNodeBegin(), (yyvsp[(1) - (5)]));
        env->SetIN_Context(0);
    ;}
    break;

  case 37:

/* Line 1455 of yacc.c  */
#line 402 "query_parser_bison.y"
    {
        QTreeAddNode(parm, (yyval) = (yyvsp[(2) - (3)]), (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)]));
    ;}
    break;

  case 38:

/* Line 1455 of yacc.c  */
#line 407 "query_parser_bison.y"
    {
        QTreeAddNode(parm, (yyval) = (yyvsp[(2) - (3)]), (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)]));
    ;}
    break;

  case 39:

/* Line 1455 of yacc.c  */
#line 411 "query_parser_bison.y"
    {
        QTreeAddNode(parm, (yyval) = (yyvsp[(2) - (3)]), (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)]));
    ;}
    break;

  case 40:

/* Line 1455 of yacc.c  */
#line 415 "query_parser_bison.y"
    {
        QTreeAddNode(parm, (yyval) = (yyvsp[(2) - (3)]), (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)]));
    ;}
    break;

  case 41:

/* Line 1455 of yacc.c  */
#line 419 "query_parser_bison.y"
    {
        QTreeAddNode(parm, (yyval) = (yyvsp[(2) - (3)]), (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)]));
    ;}
    break;

  case 42:

/* Line 1455 of yacc.c  */
#line 423 "query_parser_bison.y"
    {
        QTreeAddNode(parm, (yyval) = (yyvsp[(2) - (3)]), (yyvsp[(1) - (3)]), (yyvsp[(3) - (3)]));
    ;}
    break;

  case 43:

/* Line 1455 of yacc.c  */
#line 434 "query_parser_bison.y"
    {
        QTreeAddNode(parm, (yyval) = (yyvsp[(1) - (2)]), (yyvsp[(2) - (2)]), 0);
    ;}
    break;

  case 44:

/* Line 1455 of yacc.c  */
#line 443 "query_parser_bison.y"
    {
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        QTreeAddNode(parm, (yyval) = (yyvsp[(2) - (2)]), 0, 0);
    ;}
    break;

  case 45:

/* Line 1455 of yacc.c  */
#line 452 "query_parser_bison.y"
    { 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        QTreeAddNode(parm, (yyval) = (yyvsp[(2) - (2)]), 0, 0);
    ;}
    break;

  case 46:

/* Line 1455 of yacc.c  */
#line 461 "query_parser_bison.y"
    { 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        QTreeAddNode(parm, (yyval) = (yyvsp[(2) - (2)]), 0, 0);
    ;}
    break;

  case 47:

/* Line 1455 of yacc.c  */
#line 470 "query_parser_bison.y"
    { 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        QTreeAddNode(parm, (yyval) = (yyvsp[(1) - (2)]), 0, 0);
    ;}
    break;

  case 48:

/* Line 1455 of yacc.c  */
#line 482 "query_parser_bison.y"
    { 
        yyerrok;
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error. Unbalanced parenthesis");        
        }
        QTreeAddNode(parm, (yyval) = (yyvsp[(2) - (3)]), 0, 0);
    ;}
    break;

  case 49:

/* Line 1455 of yacc.c  */
#line 492 "query_parser_bison.y"
    { 
        yyerrok; 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        QTreeAddNode(parm, (yyval) = (yyvsp[(1) - (3)]), 0, 0);
    ;}
    break;

  case 50:

/* Line 1455 of yacc.c  */
#line 502 "query_parser_bison.y"
    { 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        yyerrok; 
        QTreeAddNode(parm, (yyval) = (yyvsp[(1) - (3)]), 0, 0);
    ;}
    break;

  case 51:

/* Line 1455 of yacc.c  */
#line 512 "query_parser_bison.y"
    { 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        yyerrok; 
        QTreeAddNode(parm, (yyval) = (yyvsp[(1) - (3)]), 0, 0);
    ;}
    break;

  case 52:

/* Line 1455 of yacc.c  */
#line 522 "query_parser_bison.y"
    { 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        yyerrok; 
        QTreeAddNode(parm, (yyval) = (yyvsp[(1) - (3)]), 0, 0);
    ;}
    break;

  case 53:

/* Line 1455 of yacc.c  */
#line 532 "query_parser_bison.y"
    { 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        yyerrok; 
        QTreeAddNode(parm, (yyval) = (yyvsp[(1) - (3)]), 0, 0);
    ;}
    break;



/* Line 1455 of yacc.c  */
#line 2156 "query_parser_bison.tab.c"
      default: break;
    }
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
      {
    YYSIZE_T yysize = yysyntax_error (0, yystate, yychar);
    if (yymsg_alloc < yysize && yymsg_alloc < YYSTACK_ALLOC_MAXIMUM)
      {
        YYSIZE_T yyalloc = 2 * yysize;
        if (! (yysize <= yyalloc && yyalloc <= YYSTACK_ALLOC_MAXIMUM))
          yyalloc = YYSTACK_ALLOC_MAXIMUM;
        if (yymsg != yymsgbuf)
          YYSTACK_FREE (yymsg);
        yymsg = (char *) YYSTACK_ALLOC (yyalloc);
        if (yymsg)
          yymsg_alloc = yyalloc;
        else
          {
        yymsg = yymsgbuf;
        yymsg_alloc = sizeof yymsgbuf;
          }
      }

    if (0 < yysize && yysize <= yymsg_alloc)
      {
        (void) yysyntax_error (yymsg, yystate, yychar);
        yyerror (yymsg);
      }
    else
      {
        yyerror (YY_("syntax error"));
        if (yysize != 0)
          goto yyexhaustedlab;
      }
      }
#endif
    }



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
              yytoken, &yylval);
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

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule which action triggered
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
  yyerrstatus = 3;    /* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (yyn != YYPACT_NINF)
    {
      yyn += YYTERROR;
      if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
        {
          yyn = yytable[yyn];
          if (0 < yyn)
        break;
        }
    }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
    YYABORT;


      yydestruct ("Error: popping",
          yystos[yystate], yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  *++yyvsp = yylval;


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#if !defined(yyoverflow) || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
     yydestruct ("Cleanup: discarding lookahead",
         yytoken, &yylval);
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
          yystos[*yyssp], yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}



/* Line 1675 of yacc.c  */
#line 554 "query_parser_bison.y"



