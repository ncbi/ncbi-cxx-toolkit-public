/* A Bison parser, made by GNU Bison 1.875.  */

/* Skeleton parser for Yacc-like parsing with Bison,
   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

/* Written by Richard Stallman by simplifying the original so called
   ``semantic'' parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Using locations.  */
#define YYLSP_NEEDED 0

/* If NAME_PREFIX is specified substitute the variables and functions
   names.  */
#define yyparse ncbi_q_parse
#define yylex   ncbi_q_lex
#define yyerror ncbi_q_error
#define yylval  ncbi_q_lval
#define yychar  ncbi_q_char
#define yydebug ncbi_q_debug
#define yynerrs ncbi_q_nerrs


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
#define IDENT 258
#define STRING 259
#define NUM_INT 260
#define SELECT 261
#define FROM 262
#define WHERE 263
#define FUNCTION 264
#define AND 265
#define NOT 266
#define OR 267
#define SUB 268
#define XOR 269
#define RANGE 270
#define EQ 271
#define NOTEQ 272
#define GT 273
#define GE 274
#define LT 275
#define LE 276
#define BETWEEN 277
#define NOT_BETWEEN 278
#define LIKE 279
#define NOT_LIKE 280
#define IN 281
#define NOT_IN 282




/* Copy the first part of user declarations.  */
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
    CQueryParseTree::TNode* in_node = env->GetIN_Context();
    CQueryParseTree::TNode* func_node = env->GetContext();
    
    // function is higher priority 
    // TODO: (strickly speaking IN and FUNC should be separated)
    //
    if (func_node) {
        func_node->AddNode(node);
    } else
    if (in_node) {
        in_node->AddNode(node);
    }
    env->ForgetPoolNodes(node, 0);
}



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

#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
typedef int YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */


/* Line 214 of yacc.c.  */
#line 205 "query_parser_bison.tab.c"

#if ! defined (yyoverflow) || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# if YYSTACK_USE_ALLOCA
#  define YYSTACK_ALLOC alloca
# else
#  ifndef YYSTACK_USE_ALLOCA
#   if defined (alloca) || defined (_ALLOCA_H)
#    define YYSTACK_ALLOC alloca
#   else
#    ifdef __GNUC__
#     define YYSTACK_ALLOC __builtin_alloca
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning. */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
# else
#  if defined (__STDC__) || defined (__cplusplus)
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   define YYSIZE_T size_t
#  endif
#  define YYSTACK_ALLOC malloc
#  define YYSTACK_FREE free
# endif
#endif /* ! defined (yyoverflow) || YYERROR_VERBOSE */


#if (! defined (yyoverflow) \
     && (! defined (__cplusplus) \
	 || (YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  short yyss;
  YYSTYPE yyvs;
  };

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (short) + sizeof (YYSTYPE))				\
      + YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  register YYSIZE_T yyi;		\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (0)
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack)					\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack, Stack, yysize);				\
	Stack = &yyptr->Stack;						\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (0)

#endif

#if defined (__STDC__) || defined (__cplusplus)
   typedef signed char yysigned_char;
#else
   typedef short yysigned_char;
#endif

/* YYFINAL -- State number of the termination state. */
#define YYFINAL  25
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   577

/* YYNTOKENS -- Number of terminals. */
#define YYNTOKENS  31
/* YYNNTS -- Number of nonterminals. */
#define YYNNTS  11
/* YYNRULES -- Number of rules. */
#define YYNRULES  52
/* YYNRULES -- Number of states. */
#define YYNSTATES  95

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   282

#define YYTRANSLATE(YYX) 						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const unsigned char yytranslate[] =
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
static const unsigned char yyprhs[] =
{
       0,     0,     3,     5,     7,    13,    14,    17,    22,    24,
      28,    30,    32,    34,    36,    38,    42,    44,    48,    50,
      52,    54,    56,    59,    62,    66,    70,    74,    78,    82,
      88,    94,    98,   102,   106,   112,   118,   122,   126,   130,
     134,   138,   142,   145,   148,   151,   154,   157,   161,   165,
     169,   173,   177
};

/* YYRHS -- A `-1'-separated list of the rules' RHS. */
static const yysigned_char yyrhs[] =
{
      32,     0,    -1,    41,    -1,    33,    -1,     6,    36,     7,
      36,    34,    -1,    -1,     8,    41,    -1,     9,    28,    39,
      29,    -1,    37,    -1,    36,    30,    37,    -1,     5,    -1,
       4,    -1,     3,    -1,    35,    -1,    37,    -1,    38,    30,
      37,    -1,    41,    -1,    39,    30,    41,    -1,    38,    -1,
      33,    -1,    37,    -1,    35,    -1,     4,     3,    -1,    41,
      41,    -1,    28,    41,    29,    -1,    41,    10,    41,    -1,
      41,    13,    41,    -1,    41,    12,    41,    -1,    41,    14,
      41,    -1,    37,    22,    37,    10,    37,    -1,    37,    23,
      37,    10,    37,    -1,    41,    15,    41,    -1,    37,    24,
      37,    -1,    37,    25,    37,    -1,    37,    26,    28,    40,
      29,    -1,    37,    27,    28,    40,    29,    -1,    41,    16,
      41,    -1,    41,    17,    41,    -1,    41,    18,    41,    -1,
      41,    19,    41,    -1,    41,    20,    41,    -1,    41,    21,
      41,    -1,    11,    41,    -1,     1,     4,    -1,     1,     3,
      -1,     1,     5,    -1,    41,     1,    -1,    28,    41,     1,
      -1,    41,    12,     1,    -1,    41,    14,     1,    -1,    41,
      11,     1,    -1,    41,    10,     1,    -1,    41,    13,     1,
      -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const unsigned short yyrline[] =
{
       0,   122,   122,   123,   130,   170,   174,   184,   196,   200,
     212,   217,   222,   226,   233,   237,   245,   249,   258,   260,
     264,   269,   274,   282,   293,   303,   308,   313,   318,   323,
     336,   350,   355,   360,   365,   376,   387,   392,   396,   400,
     404,   408,   419,   428,   437,   446,   455,   467,   477,   487,
     497,   507,   517
};
#endif

#if YYDEBUG || YYERROR_VERBOSE
/* YYTNME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals. */
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
static const unsigned short yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,    40,    41,
      44
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const unsigned char yyr1[] =
{
       0,    31,    32,    32,    33,    34,    34,    35,    36,    36,
      37,    37,    37,    37,    38,    38,    39,    39,    40,    40,
      41,    41,    41,    41,    41,    41,    41,    41,    41,    41,
      41,    41,    41,    41,    41,    41,    41,    41,    41,    41,
      41,    41,    41,    41,    41,    41,    41,    41,    41,    41,
      41,    41,    41
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const unsigned char yyr2[] =
{
       0,     2,     1,     1,     5,     0,     2,     4,     1,     3,
       1,     1,     1,     1,     1,     3,     1,     3,     1,     1,
       1,     1,     2,     2,     3,     3,     3,     3,     3,     5,
       5,     3,     3,     3,     5,     5,     3,     3,     3,     3,
       3,     3,     2,     2,     2,     2,     2,     3,     3,     3,
       3,     3,     3
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const unsigned char yydefact[] =
{
       0,     0,    12,    11,    10,     0,     0,     0,     0,     0,
       3,    13,    20,     0,    44,    43,    45,    22,    11,    13,
       0,     8,     0,     0,     0,     1,     0,     0,     0,     0,
       0,     0,    46,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      46,    24,     0,     0,    32,    33,     0,     0,    51,     0,
      50,    48,     0,    52,     0,    49,     0,     0,     0,     0,
       0,     0,     0,     0,     5,     9,     7,     0,     0,     0,
      19,    14,    18,     0,     0,     0,     4,     0,    29,    30,
       0,    34,    35,     0,    15
};

/* YYDEFGOTO[NTERM-NUM]. */
static const yysigned_char yydefgoto[] =
{
      -1,     9,    80,    86,    11,    20,    12,    82,    48,    83,
      45
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -26
static const short yypact[] =
{
      58,    46,   -26,    -1,   -26,    84,   -19,   502,   502,    14,
     -26,   -26,    72,   460,   -26,   -26,   -26,   -26,   -26,   -26,
      -6,   -26,   502,   108,   481,   -26,    84,    84,    84,    84,
     -11,    -7,    46,   511,   520,   531,   540,   549,   502,   502,
     502,   502,   502,   502,   502,   130,    84,    84,   -25,   394,
      15,   -26,    13,    16,   -26,   -26,     7,     7,    46,   152,
      46,    46,   174,    46,   196,    46,   218,   240,   262,   284,
     306,   328,   350,   372,    -5,   -26,   -26,   502,    84,    84,
     -26,   -26,     2,    17,    27,   502,   -26,   416,   -26,   -26,
      84,   -26,   -26,   438,   -26
};

/* YYPGOTO[NTERM-NUM].  */
static const yysigned_char yypgoto[] =
{
     -26,   -26,    60,   -26,     1,    19,    26,   -26,   -26,     9,
       0
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -48
static const yysigned_char yytable[] =
{
      13,    46,    17,    85,    76,    77,    19,    23,    24,    22,
       2,    18,     4,     5,    25,   -47,     6,    56,    14,    15,
      16,    57,    49,    78,    47,    47,    79,    19,    19,    19,
      19,    21,    90,    59,    23,    62,    64,    66,    67,    68,
      69,    70,    71,    72,    73,   -47,    91,    19,    19,    14,
      15,    16,    52,    53,    54,    55,    92,    19,    19,     1,
      10,     2,     3,     4,     5,    74,    84,     6,     0,     7,
       0,     0,    21,    75,     0,     0,     0,    87,     0,    19,
      19,     0,    81,    81,     0,    93,     8,     2,    18,     4,
       0,    19,     0,     6,    26,    27,    28,    29,    30,    31,
       0,     0,     0,     0,    88,    89,     0,     0,   -42,    32,
       0,     2,     3,     4,     0,     0,    94,     6,   -42,   -42,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
     -23,    32,     0,     2,     3,     4,     8,   -42,   -42,     6,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,   -25,    32,     0,     2,     3,     4,     8,   -23,
     -23,     6,   -25,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,   -27,    32,     0,     2,     3,     4,
       8,   -25,   -25,     6,   -27,   -27,   -27,    36,    37,    38,
      39,    40,    41,    42,    43,    44,   -26,    32,     0,     2,
       3,     4,     8,   -27,   -27,     6,   -26,   -26,   -26,   -26,
      37,    38,    39,    40,    41,    42,    43,    44,   -28,    32,
       0,     2,     3,     4,     8,   -26,   -26,     6,   -28,   -28,
     -28,   -28,   -28,    38,    39,    40,    41,    42,    43,    44,
     -31,    32,     0,     2,     3,     4,     8,   -28,   -28,     6,
     -31,   -31,   -31,   -31,   -31,   -31,    39,    40,    41,    42,
      43,    44,   -36,    32,     0,     2,     3,     4,     8,   -31,
     -31,     6,   -36,   -36,   -36,   -36,   -36,   -36,   -36,    40,
      41,    42,    43,    44,   -37,    32,     0,     2,     3,     4,
       8,   -36,   -36,     6,   -37,   -37,   -37,   -37,   -37,   -37,
     -37,   -37,    41,    42,    43,    44,   -38,    32,     0,     2,
       3,     4,     8,   -37,   -37,     6,   -38,   -38,   -38,   -38,
     -38,   -38,   -38,   -38,   -38,    42,    43,    44,   -39,    32,
       0,     2,     3,     4,     8,   -38,   -38,     6,   -39,   -39,
     -39,   -39,   -39,   -39,   -39,   -39,   -39,   -39,    43,    44,
     -40,    32,     0,     2,     3,     4,     8,   -39,   -39,     6,
     -40,   -40,   -40,   -40,   -40,   -40,   -40,   -40,   -40,   -40,
     -40,    44,   -41,    32,     0,     2,     3,     4,     8,   -40,
     -40,     6,   -41,   -41,   -41,   -41,   -41,   -41,   -41,   -41,
     -41,   -41,   -41,   -41,     0,    32,     0,     2,     3,     4,
       8,   -41,   -41,     6,    33,    34,    35,    36,    37,    38,
      39,    40,    41,    42,    43,    44,     0,    32,     0,     2,
       3,     4,     8,   -16,   -16,     6,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    -6,    32,
       0,     2,     3,     4,     8,   -17,   -17,     6,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      -2,    32,     0,     2,     3,     4,     8,    -6,     0,     6,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    50,     0,     2,     3,     4,     0,     8,     0,
       6,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,     1,     0,     2,     3,     4,     0,     8,
      51,     6,    58,     7,     2,     3,     4,     0,     0,     0,
       6,    60,     7,     2,     3,     4,     0,     0,     0,     6,
       8,     7,    61,     0,     2,     3,     4,     0,     0,     8,
       6,    63,     7,     2,     3,     4,     0,     0,     8,     6,
      65,     7,     2,     3,     4,     0,     0,     0,     6,     8,
       7,     0,     0,     0,     0,     0,     0,     0,     8,     0,
       0,     0,     0,     0,     0,     0,     0,     8
};

static const yysigned_char yycheck[] =
{
       0,     7,     3,     8,    29,    30,     5,     7,     8,    28,
       3,     4,     5,     6,     0,     0,     9,    28,     3,     4,
       5,    28,    22,    10,    30,    30,    10,    26,    27,    28,
      29,     5,    30,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    30,    29,    46,    47,     3,
       4,     5,    26,    27,    28,    29,    29,    56,    57,     1,
       0,     3,     4,     5,     6,    46,    57,     9,    -1,    11,
      -1,    -1,    46,    47,    -1,    -1,    -1,    77,    -1,    78,
      79,    -1,    56,    57,    -1,    85,    28,     3,     4,     5,
      -1,    90,    -1,     9,    22,    23,    24,    25,    26,    27,
      -1,    -1,    -1,    -1,    78,    79,    -1,    -1,     0,     1,
      -1,     3,     4,     5,    -1,    -1,    90,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
       0,     1,    -1,     3,     4,     5,    28,    29,    30,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,     0,     1,    -1,     3,     4,     5,    28,    29,
      30,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,     0,     1,    -1,     3,     4,     5,
      28,    29,    30,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,     0,     1,    -1,     3,
       4,     5,    28,    29,    30,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,     0,     1,
      -1,     3,     4,     5,    28,    29,    30,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
       0,     1,    -1,     3,     4,     5,    28,    29,    30,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,     0,     1,    -1,     3,     4,     5,    28,    29,
      30,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,     0,     1,    -1,     3,     4,     5,
      28,    29,    30,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,     0,     1,    -1,     3,
       4,     5,    28,    29,    30,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,     0,     1,
      -1,     3,     4,     5,    28,    29,    30,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
       0,     1,    -1,     3,     4,     5,    28,    29,    30,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,     0,     1,    -1,     3,     4,     5,    28,    29,
      30,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    -1,     1,    -1,     3,     4,     5,
      28,    29,    30,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    -1,     1,    -1,     3,
       4,     5,    28,    29,    30,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,     0,     1,
      -1,     3,     4,     5,    28,    29,    30,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
       0,     1,    -1,     3,     4,     5,    28,    29,    -1,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,     1,    -1,     3,     4,     5,    -1,    28,    -1,
       9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,    21,     1,    -1,     3,     4,     5,    -1,    28,
      29,     9,     1,    11,     3,     4,     5,    -1,    -1,    -1,
       9,     1,    11,     3,     4,     5,    -1,    -1,    -1,     9,
      28,    11,     1,    -1,     3,     4,     5,    -1,    -1,    28,
       9,     1,    11,     3,     4,     5,    -1,    -1,    28,     9,
       1,    11,     3,     4,     5,    -1,    -1,    -1,     9,    28,
      11,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    28,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    28
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const unsigned char yystos[] =
{
       0,     1,     3,     4,     5,     6,     9,    11,    28,    32,
      33,    35,    37,    41,     3,     4,     5,     3,     4,    35,
      36,    37,    28,    41,    41,     0,    22,    23,    24,    25,
      26,    27,     1,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    41,     7,    30,    39,    41,
       1,    29,    37,    37,    37,    37,    28,    28,     1,    41,
       1,     1,    41,     1,    41,     1,    41,    41,    41,    41,
      41,    41,    41,    41,    36,    37,    29,    30,    10,    10,
      33,    37,    38,    40,    40,     8,    34,    41,    37,    37,
      30,    29,    29,    41,    37
};

#if ! defined (YYSIZE_T) && defined (__SIZE_TYPE__)
# define YYSIZE_T __SIZE_TYPE__
#endif
#if ! defined (YYSIZE_T) && defined (size_t)
# define YYSIZE_T size_t
#endif
#if ! defined (YYSIZE_T)
# if defined (__STDC__) || defined (__cplusplus)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# endif
#endif
#if ! defined (YYSIZE_T)
# define YYSIZE_T unsigned int
#endif

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrlab1

/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */

#define YYFAIL		goto yyerrlab

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yytoken = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    { 								\
      yyerror ("syntax error: cannot back up");\
      YYERROR;							\
    }								\
while (0)

#define YYTERROR	1
#define YYERRCODE	256

/* YYLLOC_DEFAULT -- Compute the default location (before the actions
   are run).  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)         \
  Current.first_line   = Rhs[1].first_line;      \
  Current.first_column = Rhs[1].first_column;    \
  Current.last_line    = Rhs[N].last_line;       \
  Current.last_column  = Rhs[N].last_column;
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

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (0)

# define YYDSYMPRINT(Args)			\
do {						\
  if (yydebug)					\
    yysymprint Args;				\
} while (0)

# define YYDSYMPRINTF(Title, Token, Value, Location)		\
do {								\
  if (yydebug)							\
    {								\
      YYFPRINTF (stderr, "%s ", Title);				\
      yysymprint (stderr, 					\
                  Token, Value);	\
      YYFPRINTF (stderr, "\n");					\
    }								\
} while (0)

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (cinluded).                                                   |
`------------------------------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yy_stack_print (short *bottom, short *top)
#else
static void
yy_stack_print (bottom, top)
    short *bottom;
    short *top;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (/* Nothing. */; bottom <= top; ++bottom)
    YYFPRINTF (stderr, " %d", *bottom);
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yy_reduce_print (int yyrule)
#else
static void
yy_reduce_print (yyrule)
    int yyrule;
#endif
{
  int yyi;
  unsigned int yylineno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %u), ",
             yyrule - 1, yylineno);
  /* Print the symbols being reduced, and their result.  */
  for (yyi = yyprhs[yyrule]; 0 <= yyrhs[yyi]; yyi++)
    YYFPRINTF (stderr, "%s ", yytname [yyrhs[yyi]]);
  YYFPRINTF (stderr, "-> %s\n", yytname [yyr1[yyrule]]);
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (Rule);		\
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YYDSYMPRINT(Args)
# define YYDSYMPRINTF(Title, Token, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   SIZE_MAX < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#if YYMAXDEPTH == 0
# undef YYMAXDEPTH
#endif

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif



#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined (__GLIBC__) && defined (_STRING_H)
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
#   if defined (__STDC__) || defined (__cplusplus)
yystrlen (const char *yystr)
#   else
yystrlen (yystr)
     const char *yystr;
#   endif
{
  register const char *yys = yystr;

  while (*yys++ != '\0')
    continue;

  return yys - yystr - 1;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined (__GLIBC__) && defined (_STRING_H) && defined (_GNU_SOURCE)
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
#   if defined (__STDC__) || defined (__cplusplus)
yystpcpy (char *yydest, const char *yysrc)
#   else
yystpcpy (yydest, yysrc)
     char *yydest;
     const char *yysrc;
#   endif
{
  register char *yyd = yydest;
  register const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

#endif /* !YYERROR_VERBOSE */



#if YYDEBUG
/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yysymprint (FILE *yyoutput, int yytype, YYSTYPE *yyvaluep)
#else
static void
yysymprint (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  /* Pacify ``unused variable'' warnings.  */
  (void) yyvaluep;

  if (yytype < YYNTOKENS)
    {
      YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
# ifdef YYPRINT
      YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
    }
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  switch (yytype)
    {
      default:
        break;
    }
  YYFPRINTF (yyoutput, ")");
}

#endif /* ! YYDEBUG */
/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yydestruct (int yytype, YYSTYPE *yyvaluep)
#else
static void
yydestruct (yytype, yyvaluep)
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  /* Pacify ``unused variable'' warnings.  */
  (void) yyvaluep;

  switch (yytype)
    {

      default:
        break;
    }
}


/* Prevent warnings from -Wmissing-prototypes.  */

#ifdef YYPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
int yyparse (void *YYPARSE_PARAM);
# else
int yyparse ();
# endif
#else /* ! YYPARSE_PARAM */
#if defined (__STDC__) || defined (__cplusplus)
int yyparse (void);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */






/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
int yyparse (void *YYPARSE_PARAM)
# else
int yyparse (YYPARSE_PARAM)
  void *YYPARSE_PARAM;
# endif
#else /* ! YYPARSE_PARAM */
#if defined (__STDC__) || defined (__cplusplus)
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

  register int yystate;
  register int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

  /* The state stack.  */
  short	yyssa[YYINITDEPTH];
  short *yyss = yyssa;
  register short *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  register YYSTYPE *yyvsp;



#define YYPOPSTACK   (yyvsp--, yyssp--)

  YYSIZE_T yystacksize = YYINITDEPTH;

  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;


  /* When reducing, the number of symbols on the RHS of the reduced
     rule.  */
  int yylen;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

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
     have just been pushed. so pushing a state here evens the stacks.
     */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack. Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	short *yyss1 = yyss;


	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow ("parser stack overflow",
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),

		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyoverflowlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyoverflowlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	short *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyoverflowlab;
	YYSTACK_RELOCATE (yyss);
	YYSTACK_RELOCATE (yyvs);

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

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

/* Do appropriate processing given the current state.  */
/* Read a lookahead token if we need one and don't already have one.  */
/* yyresume: */

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
      YYDSYMPRINTF ("Next token is", yytoken, &yylval, &yylloc);
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

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Shift the lookahead token.  */
  YYDPRINTF ((stderr, "Shifting token %s, ", yytname[yytoken]));

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;


  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  yystate = yyn;
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
#line 131 "query_parser_bison.y"
    {
    CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);

    /* create a pseudo node to isolate select list in a dedicated subtree */
    //CQueryParseTree::TNode* qnode = 0;
    //qnode = env->QTree().CreateNode(CQueryParseNode::eList, 0, 0, "LIST");    
    //qnode->MoveSubnodes($2);
    CQueryParseTree::TNode* qnode = env->GetContext();
    if (qnode) {
        qnode->InsertNode(qnode->SubNodeBegin(), yyvsp[-3]);
    }

    //$1->InsertNode($1->SubNodeBegin(),qnode);                
    
    yyvsp[-4]->AddNode(yyvsp[-2]);
    yyvsp[-2]->MoveSubnodes(yyvsp[-1]);
    yyvsp[-2]->InsertNode(yyvsp[-2]->SubNodeBegin(), yyvsp[-1]);    
    if (yyvsp[0]) {
        yyvsp[-4]->AddNode(yyvsp[0]);
    }
    
    env->ForgetPoolNode(yyvsp[-4]);
    env->ForgetPoolNode(yyvsp[-3]);
    env->ForgetPoolNode(yyvsp[-2]);
    env->ForgetPoolNode(yyvsp[-1]);
    env->ForgetPoolNode(yyvsp[0]);
    
    env->SetSELECT_Context(0);
    env->SetContext(0);
    env->SetFROM_Context(0);
    
    yyval = yyvsp[-4];
    env->AttachQueryTree(yyval);
    
    ;}
    break;

  case 5:
#line 170 "query_parser_bison.y"
    {
    yyval=0;
    ;}
    break;

  case 6:
#line 175 "query_parser_bison.y"
    {
    CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
    yyvsp[-1]->AddNode(yyvsp[0]);
    env->ForgetPoolNode(yyvsp[0]);
    yyval = yyvsp[-1];
    ;}
    break;

  case 7:
#line 185 "query_parser_bison.y"
    {
        yyval = yyvsp[-3];
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        env->AttachQueryTree(yyval);    
        yyval->InsertNode(yyval->SubNodeBegin(), yyvsp[-1]);
        env->SetContext(0);
        env->ForgetPoolNodes(yyvsp[-3], yyvsp[-1]);    
    ;}
    break;

  case 8:
#line 197 "query_parser_bison.y"
    {
        yyval = yyvsp[0];
    ;}
    break;

  case 9:
#line 201 "query_parser_bison.y"
    {
        yyval = yyvsp[-2];
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        AddFunc_Arg(parm, yyvsp[0]);        
        //$$->AddNode($3);
        env->ForgetPoolNode(yyvsp[0]);
    ;}
    break;

  case 10:
#line 213 "query_parser_bison.y"
    {
        yyval = yyvsp[0];         
    ;}
    break;

  case 11:
#line 218 "query_parser_bison.y"
    {
        yyval = yyvsp[0];         
    ;}
    break;

  case 12:
#line 223 "query_parser_bison.y"
    {
        yyval = yyvsp[0];         
    ;}
    break;

  case 13:
#line 227 "query_parser_bison.y"
    {
        yyval = yyvsp[0];  
    ;}
    break;

  case 14:
#line 234 "query_parser_bison.y"
    {
        yyval = yyvsp[0];
    ;}
    break;

  case 15:
#line 238 "query_parser_bison.y"
    {
        yyval = yyvsp[-2];    
        AddFunc_Arg(parm, yyvsp[0]);
    ;}
    break;

  case 16:
#line 246 "query_parser_bison.y"
    {
        yyval = yyvsp[0];
    ;}
    break;

  case 17:
#line 250 "query_parser_bison.y"
    {
        yyval = yyvsp[-2];    
        AddFunc_Arg(parm, yyvsp[0]);
    ;}
    break;

  case 20:
#line 265 "query_parser_bison.y"
    {
        yyval = yyvsp[0];
    ;}
    break;

  case 21:
#line 270 "query_parser_bison.y"
    {
        yyval = yyvsp[0];
    ;}
    break;

  case 22:
#line 275 "query_parser_bison.y"
    {    
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        yyval = env->QTree().CreateNode(CQueryParseNode::eFieldSearch, yyvsp[-1], yyvsp[0]);
        env->AttachQueryTree(yyval);
        env->ForgetPoolNodes(yyvsp[-1], yyvsp[0]);
    ;}
    break;

  case 23:
#line 283 "query_parser_bison.y"
    {
        yyerrok;
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        yyval = env->QTree().CreateNode(CQueryParseNode::eAnd, yyvsp[-1], yyvsp[0]);
        yyval->GetValue().SetExplicit(false);
        env->AttachQueryTree(yyval);
        env->ForgetPoolNodes(yyvsp[-1], yyvsp[0]);
    ;}
    break;

  case 24:
#line 294 "query_parser_bison.y"
    {
        yyval = yyvsp[-1];
    ;}
    break;

  case 25:
#line 304 "query_parser_bison.y"
    {
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[-2], yyvsp[0]);
    ;}
    break;

  case 26:
#line 309 "query_parser_bison.y"
    {
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[-2], yyvsp[0]);
    ;}
    break;

  case 27:
#line 314 "query_parser_bison.y"
    {
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[-2], yyvsp[0]);
    ;}
    break;

  case 28:
#line 319 "query_parser_bison.y"
    {
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[-2], yyvsp[0]);
    ;}
    break;

  case 29:
#line 324 "query_parser_bison.y"
    {
        yyval = yyvsp[-3];
        yyval->AddNode(yyvsp[-4]);
        yyval->AddNode(yyvsp[-2]);
        yyval->AddNode(yyvsp[0]);

        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        env->AttachQueryTree(yyval);    
        env->ForgetPoolNodes(yyvsp[-4], yyvsp[-2]);
        env->ForgetPoolNodes(yyvsp[-4], yyvsp[0]);
    ;}
    break;

  case 30:
#line 337 "query_parser_bison.y"
    {
        yyval = yyvsp[-3];
        yyval->AddNode(yyvsp[-4]);
        yyval->AddNode(yyvsp[-2]);
        yyval->AddNode(yyvsp[0]);

        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        env->AttachQueryTree(yyval);    
        env->ForgetPoolNodes(yyvsp[-4], yyvsp[-2]);
        env->ForgetPoolNodes(yyvsp[-4], yyvsp[0]);        
    ;}
    break;

  case 31:
#line 351 "query_parser_bison.y"
    {
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[-2], yyvsp[0]);
    ;}
    break;

  case 32:
#line 356 "query_parser_bison.y"
    {
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[-2], yyvsp[0]);
    ;}
    break;

  case 33:
#line 361 "query_parser_bison.y"
    {
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[-2], yyvsp[0]);
    ;}
    break;

  case 34:
#line 366 "query_parser_bison.y"
    {
        yyval = yyvsp[-3];
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        env->AttachQueryTree(yyval);    
        env->ForgetPoolNodes(yyvsp[-4], yyvsp[-1]);
        yyval->InsertNode(yyval->SubNodeBegin(), yyvsp[-1]);
        yyval->InsertNode(yyval->SubNodeBegin(), yyvsp[-4]);
        env->SetIN_Context(0);
    ;}
    break;

  case 35:
#line 377 "query_parser_bison.y"
    {
        yyval = yyvsp[-3];
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        env->AttachQueryTree(yyval);    
        env->ForgetPoolNodes(yyvsp[-4], yyvsp[-1]);
        yyval->InsertNode(yyval->SubNodeBegin(), yyvsp[-1]);
        yyval->InsertNode(yyval->SubNodeBegin(), yyvsp[-4]);
        env->SetIN_Context(0);
    ;}
    break;

  case 36:
#line 388 "query_parser_bison.y"
    {
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[-2], yyvsp[0]);
    ;}
    break;

  case 37:
#line 393 "query_parser_bison.y"
    {
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[-2], yyvsp[0]);
    ;}
    break;

  case 38:
#line 397 "query_parser_bison.y"
    {
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[-2], yyvsp[0]);
    ;}
    break;

  case 39:
#line 401 "query_parser_bison.y"
    {
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[-2], yyvsp[0]);
    ;}
    break;

  case 40:
#line 405 "query_parser_bison.y"
    {
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[-2], yyvsp[0]);
    ;}
    break;

  case 41:
#line 409 "query_parser_bison.y"
    {
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[-2], yyvsp[0]);
    ;}
    break;

  case 42:
#line 420 "query_parser_bison.y"
    {
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[0], 0);
    ;}
    break;

  case 43:
#line 429 "query_parser_bison.y"
    {
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        QTreeAddNode(parm, yyval = yyvsp[0], 0, 0);
    ;}
    break;

  case 44:
#line 438 "query_parser_bison.y"
    { 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        QTreeAddNode(parm, yyval = yyvsp[0], 0, 0);
    ;}
    break;

  case 45:
#line 447 "query_parser_bison.y"
    { 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        QTreeAddNode(parm, yyval = yyvsp[0], 0, 0);
    ;}
    break;

  case 46:
#line 456 "query_parser_bison.y"
    { 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        QTreeAddNode(parm, yyval = yyvsp[-1], 0, 0);
    ;}
    break;

  case 47:
#line 468 "query_parser_bison.y"
    { 
        yyerrok;
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error. Unbalanced parenthesis");        
        }
        QTreeAddNode(parm, yyval = yyvsp[-1], 0, 0);
    ;}
    break;

  case 48:
#line 478 "query_parser_bison.y"
    { 
        yyerrok; 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        QTreeAddNode(parm, yyval = yyvsp[-2], 0, 0);
    ;}
    break;

  case 49:
#line 488 "query_parser_bison.y"
    { 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        yyerrok; 
        QTreeAddNode(parm, yyval = yyvsp[-2], 0, 0);
    ;}
    break;

  case 50:
#line 498 "query_parser_bison.y"
    { 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        yyerrok; 
        QTreeAddNode(parm, yyval = yyvsp[-2], 0, 0);
    ;}
    break;

  case 51:
#line 508 "query_parser_bison.y"
    { 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        yyerrok; 
        QTreeAddNode(parm, yyval = yyvsp[-2], 0, 0);
    ;}
    break;

  case 52:
#line 518 "query_parser_bison.y"
    { 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        yyerrok; 
        QTreeAddNode(parm, yyval = yyvsp[-2], 0, 0);
    ;}
    break;


    }

/* Line 991 of yacc.c.  */
#line 1723 "query_parser_bison.tab.c"

  yyvsp -= yylen;
  yyssp -= yylen;


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
#if YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (YYPACT_NINF < yyn && yyn < YYLAST)
	{
	  YYSIZE_T yysize = 0;
	  int yytype = YYTRANSLATE (yychar);
	  char *yymsg;
	  int yyx, yycount;

	  yycount = 0;
	  /* Start YYX at -YYN if negative to avoid negative indexes in
	     YYCHECK.  */
	  for (yyx = yyn < 0 ? -yyn : 0;
	       yyx < (int) (sizeof (yytname) / sizeof (char *)); yyx++)
	    if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	      yysize += yystrlen (yytname[yyx]) + 15, yycount++;
	  yysize += yystrlen ("syntax error, unexpected ") + 1;
	  yysize += yystrlen (yytname[yytype]);
	  yymsg = (char *) YYSTACK_ALLOC (yysize);
	  if (yymsg != 0)
	    {
	      char *yyp = yystpcpy (yymsg, "syntax error, unexpected ");
	      yyp = yystpcpy (yyp, yytname[yytype]);

	      if (yycount < 5)
		{
		  yycount = 0;
		  for (yyx = yyn < 0 ? -yyn : 0;
		       yyx < (int) (sizeof (yytname) / sizeof (char *));
		       yyx++)
		    if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
		      {
			const char *yyq = ! yycount ? ", expecting " : " or ";
			yyp = yystpcpy (yyp, yyq);
			yyp = yystpcpy (yyp, yytname[yyx]);
			yycount++;
		      }
		}
	      yyerror (yymsg);
	      YYSTACK_FREE (yymsg);
	    }
	  else
	    yyerror ("syntax error; also virtual memory exhausted");
	}
      else
#endif /* YYERROR_VERBOSE */
	yyerror ("syntax error");
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      /* Return failure if at end of input.  */
      if (yychar == YYEOF)
        {
	  /* Pop the error token.  */
          YYPOPSTACK;
	  /* Pop the rest of the stack.  */
	  while (yyss < yyssp)
	    {
	      YYDSYMPRINTF ("Error: popping", yystos[*yyssp], yyvsp, yylsp);
	      yydestruct (yystos[*yyssp], yyvsp);
	      YYPOPSTACK;
	    }
	  YYABORT;
        }

      YYDSYMPRINTF ("Error: discarding", yytoken, &yylval, &yylloc);
      yydestruct (yytoken, &yylval);
      yychar = YYEMPTY;

    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab2;


/*----------------------------------------------------.
| yyerrlab1 -- error raised explicitly by an action.  |
`----------------------------------------------------*/
yyerrlab1:

  /* Suppress GCC warning that yyerrlab1 is unused when no action
     invokes YYERROR.  */
#if defined (__GNUC_MINOR__) && 2093 <= (__GNUC__ * 1000 + __GNUC_MINOR__) \
    && !defined __cplusplus
  __attribute__ ((__unused__))
#endif


  goto yyerrlab2;


/*---------------------------------------------------------------.
| yyerrlab2 -- pop states until the error token can be shifted.  |
`---------------------------------------------------------------*/
yyerrlab2:
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

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

      YYDSYMPRINTF ("Error: popping", yystos[*yyssp], yyvsp, yylsp);
      yydestruct (yystos[yystate], yyvsp);
      yyvsp--;
      yystate = *--yyssp;

      YY_STACK_PRINT (yyss, yyssp);
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  YYDPRINTF ((stderr, "Shifting error token, "));

  *++yyvsp = yylval;


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

#ifndef yyoverflow
/*----------------------------------------------.
| yyoverflowlab -- parser overflow comes here.  |
`----------------------------------------------*/
yyoverflowlab:
  yyerror ("parser stack overflow");
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
  return yyresult;
}


#line 540 "query_parser_bison.y"




