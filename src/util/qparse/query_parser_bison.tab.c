/* A Bison parser, made from query_parser_bison.y
   by GNU bison 1.35.  */

#define YYBISON 1  /* Identify Bison output.  */

# define	IDENT	257
# define	STRING	258
# define	NUM_INT	259
# define	SELECT	260
# define	FROM	261
# define	WHERE	262
# define	AND	263
# define	NOT	264
# define	OR	265
# define	SUB	266
# define	XOR	267
# define	RANGE	268
# define	EQ	269
# define	NOTEQ	270
# define	GT	271
# define	GE	272
# define	LT	273
# define	LE	274
# define	BETWEEN	275
# define	NOT_BETWEEN	276
# define	LIKE	277
# define	NOT_LIKE	278
# define	IN	279
# define	NOT_IN	280

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
    if (in_node) {
        in_node->AddNode(node);
        env->ForgetPoolNodes(node, 0);
    }
}

#ifndef YYSTYPE
# define YYSTYPE int
# define YYSTYPE_IS_TRIVIAL 1
#endif
#ifndef YYDEBUG
# define YYDEBUG 0
#endif



#define	YYFINAL		87
#define	YYFLAG		-32768
#define	YYNTBASE	30

/* YYTRANSLATE(YYLEX) -- Bison token number corresponding to YYLEX. */
#define YYTRANSLATE(x) ((unsigned)(x) <= 280 ? yytranslate[x] : 38)

/* YYTRANSLATE[YYLEX] -- Bison token number corresponding to YYLEX. */
static const char yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      28,    29,     2,     2,    27,     2,     2,     2,     2,     2,
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
       2,     2,     2,     2,     2,     2,     1,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26
};

#if YYDEBUG
static const short yyprhs[] =
{
       0,     0,     2,     4,    10,    11,    14,    16,    20,    22,
      24,    26,    28,    32,    34,    36,    38,    41,    44,    48,
      52,    56,    60,    64,    70,    76,    80,    84,    88,    94,
     100,   104,   108,   112,   116,   120,   124,   128,   132,   135,
     138,   141,   144,   147,   151,   155,   159,   163,   167
};
static const short yyrhs[] =
{
      37,     0,    31,     0,     6,    33,     7,    33,    32,     0,
       0,     8,    37,     0,    34,     0,    33,    27,    34,     0,
       5,     0,     4,     0,     3,     0,    34,     0,    35,    27,
      34,     0,    35,     0,    31,     0,    34,     0,     4,     3,
       0,    37,    37,     0,    28,    37,    29,     0,    37,     9,
      37,     0,    37,    12,    37,     0,    37,    11,    37,     0,
      37,    13,    37,     0,    34,    21,    34,     9,    34,     0,
      34,    22,    34,     9,    34,     0,    37,    14,    37,     0,
      34,    23,    34,     0,    34,    24,    34,     0,    34,    25,
      28,    36,    29,     0,    34,    26,    28,    35,    29,     0,
      37,    15,    37,     0,    37,    16,    37,     0,    37,    17,
      37,     0,    37,    18,    37,     0,    37,    19,    37,     0,
      37,    20,    37,     0,    28,    37,    29,     0,    37,    10,
      37,     0,    10,    37,     0,     1,     4,     0,     1,     3,
       0,     1,     5,     0,    37,     1,     0,    28,    37,     1,
       0,    37,    11,     1,     0,    37,    13,     1,     0,    37,
      10,     1,     0,    37,     9,     1,     0,    37,    12,     1,
       0
};

#endif

#if YYDEBUG
/* YYRLINE[YYN] -- source line where rule number YYN was defined. */
static const short yyrline[] =
{
       0,   112,   114,   119,   151,   156,   166,   171,   181,   188,
     193,   199,   204,   212,   214,   218,   223,   232,   243,   253,
     258,   263,   268,   273,   286,   300,   305,   310,   315,   326,
     337,   342,   346,   350,   354,   358,   362,   367,   372,   381,
     390,   399,   408,   420,   430,   440,   450,   460,   470
};
#endif


#if (YYDEBUG) || defined YYERROR_VERBOSE

/* YYTNAME[TOKEN_NUM] -- String name of the token TOKEN_NUM. */
static const char *const yytname[] =
{
  "$", "error", "$undefined.", "IDENT", "STRING", "NUM_INT", "SELECT", 
  "FROM", "WHERE", "AND", "NOT", "OR", "SUB", "XOR", "RANGE", "EQ", 
  "NOTEQ", "GT", "GE", "LT", "LE", "BETWEEN", "NOT_BETWEEN", "LIKE", 
  "NOT_LIKE", "IN", "NOT_IN", "','", "'('", "')'", "input", 
  "select_clause", "opt_where", "obj_list", "scalar_value", "scalar_list", 
  "in_sub_expr", "exp", 0
};
#endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives. */
static const short yyr1[] =
{
       0,    30,    30,    31,    32,    32,    33,    33,    34,    34,
      34,    35,    35,    36,    36,    37,    37,    37,    37,    37,
      37,    37,    37,    37,    37,    37,    37,    37,    37,    37,
      37,    37,    37,    37,    37,    37,    37,    37,    37,    37,
      37,    37,    37,    37,    37,    37,    37,    37,    37
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN. */
static const short yyr2[] =
{
       0,     1,     1,     5,     0,     2,     1,     3,     1,     1,
       1,     1,     3,     1,     1,     1,     2,     2,     3,     3,
       3,     3,     3,     5,     5,     3,     3,     3,     5,     5,
       3,     3,     3,     3,     3,     3,     3,     3,     2,     2,
       2,     2,     2,     3,     3,     3,     3,     3,     3
};

/* YYDEFACT[S] -- default rule to reduce with in state S when YYTABLE
   doesn't specify something else to do.  Zero means the default is an
   error. */
static const short yydefact[] =
{
       0,     0,    10,     9,     8,     0,     0,     0,     2,    15,
       0,    40,    39,    41,    16,     9,     0,     6,     0,     0,
       0,     0,     0,     0,     0,     0,    42,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    42,    18,     0,     0,    26,    27,     0,     0,
      47,     0,    46,     0,    44,     0,    48,     0,    45,     0,
       0,     0,     0,     0,     0,     0,     0,     4,     7,     0,
       0,    14,    11,    13,     0,     0,     0,     3,    23,    24,
       0,    28,    29,     0,    12,     0,     0,     0
};

static const short yydefgoto[] =
{
      85,     8,    77,    16,     9,    73,    74,    39
};

static const short yypact[] =
{
      55,    59,-32768,    12,-32768,    67,    13,    13,-32768,   -13,
     392,-32768,-32768,-32768,-32768,-32768,    -2,-32768,    77,   412,
      67,    67,    67,    67,    14,    17,    59,   432,   442,   452,
     462,   472,    13,    13,    13,    13,    13,    13,    13,    98,
      67,    67,    46,-32768,    -7,    35,-32768,-32768,    49,    67,
      59,   119,    59,   140,    59,   161,    59,   182,    59,   203,
     224,   245,   266,   287,   308,   329,   350,    16,-32768,    67,
      67,-32768,-32768,    30,    -3,   -26,    13,-32768,-32768,-32768,
      67,-32768,-32768,   371,-32768,    66,    73,-32768
};

static const short yypgoto[] =
{
  -32768,    19,-32768,    34,    -1,    26,-32768,     0
};


#define	YYLAST		500


static const short yytable[] =
{
      10,    80,    69,    82,    17,    40,    18,    19,    20,    21,
      22,    23,    24,    25,     1,    14,     2,     3,     4,    44,
      45,    46,    47,     6,    76,    41,    81,    51,    53,    55,
      57,    59,    60,    61,    62,    63,    64,    65,    66,    17,
      68,     7,    48,    41,    70,    49,   -43,    72,    72,    11,
      12,    13,     2,    15,     4,     5,     1,    80,     2,     3,
       4,     5,    11,    12,    13,     6,    86,    71,    78,    79,
       2,    15,     4,    87,    67,    75,    83,   -38,    26,    84,
       2,     3,     4,     7,     0,     0,   -38,   -38,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    38,   -17,    26,
       0,     2,     3,     4,     0,     7,   -38,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,   -19,
      26,     0,     2,     3,     4,     0,     7,   -17,   -19,    28,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    38,
     -37,    26,     0,     2,     3,     4,     0,     7,   -19,   -37,
     -37,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,   -21,    26,     0,     2,     3,     4,     0,     7,   -37,
     -21,   -21,   -21,    30,    31,    32,    33,    34,    35,    36,
      37,    38,   -20,    26,     0,     2,     3,     4,     0,     7,
     -21,   -20,   -20,   -20,   -20,    31,    32,    33,    34,    35,
      36,    37,    38,   -22,    26,     0,     2,     3,     4,     0,
       7,   -20,   -22,   -22,   -22,   -22,   -22,    32,    33,    34,
      35,    36,    37,    38,   -25,    26,     0,     2,     3,     4,
       0,     7,   -22,   -25,   -25,   -25,   -25,   -25,   -25,    33,
      34,    35,    36,    37,    38,   -30,    26,     0,     2,     3,
       4,     0,     7,   -25,   -30,   -30,   -30,   -30,   -30,   -30,
     -30,    34,    35,    36,    37,    38,   -31,    26,     0,     2,
       3,     4,     0,     7,   -30,   -31,   -31,   -31,   -31,   -31,
     -31,   -31,   -31,    35,    36,    37,    38,   -32,    26,     0,
       2,     3,     4,     0,     7,   -31,   -32,   -32,   -32,   -32,
     -32,   -32,   -32,   -32,   -32,    36,    37,    38,   -33,    26,
       0,     2,     3,     4,     0,     7,   -32,   -33,   -33,   -33,
     -33,   -33,   -33,   -33,   -33,   -33,   -33,    37,    38,   -34,
      26,     0,     2,     3,     4,     0,     7,   -33,   -34,   -34,
     -34,   -34,   -34,   -34,   -34,   -34,   -34,   -34,   -34,    38,
     -35,    26,     0,     2,     3,     4,     0,     7,   -34,   -35,
     -35,   -35,   -35,   -35,   -35,   -35,   -35,   -35,   -35,   -35,
     -35,    -5,    26,     0,     2,     3,     4,     0,     7,   -35,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    -1,    26,     0,     2,     3,     4,     0,     7,
      -5,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    42,     0,     2,     3,     4,     0,     0,
       7,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    50,     0,     2,     3,     4,     0,     0,
       7,    43,     6,    52,     0,     2,     3,     4,     0,     0,
       0,     0,     6,    54,     0,     2,     3,     4,     0,     0,
       7,     0,     6,    56,     0,     2,     3,     4,     0,     0,
       7,     0,     6,    58,     0,     2,     3,     4,     0,     0,
       7,     0,     6,     0,     0,     0,     0,     0,     0,     0,
       7,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       7
};

static const short yycheck[] =
{
       0,    27,     9,    29,     5,     7,     6,     7,    21,    22,
      23,    24,    25,    26,     1,     3,     3,     4,     5,    20,
      21,    22,    23,    10,     8,    27,    29,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    40,
      41,    28,    28,    27,     9,    28,     0,    48,    49,     3,
       4,     5,     3,     4,     5,     6,     1,    27,     3,     4,
       5,     6,     3,     4,     5,    10,     0,    48,    69,    70,
       3,     4,     5,     0,    40,    49,    76,     0,     1,    80,
       3,     4,     5,    28,    -1,    -1,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,     0,     1,
      -1,     3,     4,     5,    -1,    28,    29,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,     0,
       1,    -1,     3,     4,     5,    -1,    28,    29,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
       0,     1,    -1,     3,     4,     5,    -1,    28,    29,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,     0,     1,    -1,     3,     4,     5,    -1,    28,    29,
       9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,     0,     1,    -1,     3,     4,     5,    -1,    28,
      29,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,     0,     1,    -1,     3,     4,     5,    -1,
      28,    29,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,     0,     1,    -1,     3,     4,     5,
      -1,    28,    29,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,     0,     1,    -1,     3,     4,
       5,    -1,    28,    29,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,     0,     1,    -1,     3,
       4,     5,    -1,    28,    29,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,     0,     1,    -1,
       3,     4,     5,    -1,    28,    29,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,     0,     1,
      -1,     3,     4,     5,    -1,    28,    29,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,     0,
       1,    -1,     3,     4,     5,    -1,    28,    29,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
       0,     1,    -1,     3,     4,     5,    -1,    28,    29,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,     0,     1,    -1,     3,     4,     5,    -1,    28,    29,
       9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,     0,     1,    -1,     3,     4,     5,    -1,    28,
      29,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,     1,    -1,     3,     4,     5,    -1,    -1,
      28,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,     1,    -1,     3,     4,     5,    -1,    -1,
      28,    29,    10,     1,    -1,     3,     4,     5,    -1,    -1,
      -1,    -1,    10,     1,    -1,     3,     4,     5,    -1,    -1,
      28,    -1,    10,     1,    -1,     3,     4,     5,    -1,    -1,
      28,    -1,    10,     1,    -1,     3,     4,     5,    -1,    -1,
      28,    -1,    10,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      28,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      28
};
#define YYPURE 1

/* -*-C-*-  Note some compilers choke on comments on `#line' lines.  */
#line 3 "/usr/share/bison/bison.simple"

/* Skeleton output parser for bison,

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002 Free Software
   Foundation, Inc.

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

/* This is the parser code that is written into each bison parser when
   the %semantic_parser declaration is not specified in the grammar.
   It was written by Richard Stallman by simplifying the hairy parser
   used when %semantic_parser is specified.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

#if ! defined (yyoverflow) || defined (YYERROR_VERBOSE)

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
#endif /* ! defined (yyoverflow) || defined (YYERROR_VERBOSE) */


#if (! defined (yyoverflow) \
     && (! defined (__cplusplus) \
	 || (YYLTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  short yyss;
  YYSTYPE yyvs;
# if YYLSP_NEEDED
  YYLTYPE yyls;
# endif
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAX (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# if YYLSP_NEEDED
#  define YYSTACK_BYTES(N) \
     ((N) * (sizeof (short) + sizeof (YYSTYPE) + sizeof (YYLTYPE))	\
      + 2 * YYSTACK_GAP_MAX)
# else
#  define YYSTACK_BYTES(N) \
     ((N) * (sizeof (short) + sizeof (YYSTYPE))				\
      + YYSTACK_GAP_MAX)
# endif

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
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAX;	\
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (0)

#endif


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
#define YYEMPTY		-2
#define YYEOF		0
#define YYACCEPT	goto yyacceptlab
#define YYABORT 	goto yyabortlab
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
      yychar1 = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    { 								\
      yyerror ("syntax error: cannot back up");			\
      YYERROR;							\
    }								\
while (0)

#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Compute the default location (before the actions
   are run).

   When YYLLOC_DEFAULT is run, CURRENT is set the location of the
   first token.  By default, to implement support for ranges, extend
   its range to the last symbol.  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)       	\
   Current.last_line   = Rhs[N].last_line;	\
   Current.last_column = Rhs[N].last_column;
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#if YYPURE
# if YYLSP_NEEDED
#  ifdef YYLEX_PARAM
#   define YYLEX		yylex (&yylval, &yylloc, YYLEX_PARAM)
#  else
#   define YYLEX		yylex (&yylval, &yylloc)
#  endif
# else /* !YYLSP_NEEDED */
#  ifdef YYLEX_PARAM
#   define YYLEX		yylex (&yylval, YYLEX_PARAM)
#  else
#   define YYLEX		yylex (&yylval)
#  endif
# endif /* !YYLSP_NEEDED */
#else /* !YYPURE */
# define YYLEX			yylex ()
#endif /* !YYPURE */


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
/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
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

#ifdef YYERROR_VERBOSE

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
#endif

#line 315 "/usr/share/bison/bison.simple"


/* The user can define YYPARSE_PARAM as the name of an argument to be passed
   into yyparse.  The argument should have type void *.
   It should actually point to an object.
   Grammar actions can access the variable by casting it
   to the proper pointer type.  */

#ifdef YYPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
#  define YYPARSE_PARAM_ARG void *YYPARSE_PARAM
#  define YYPARSE_PARAM_DECL
# else
#  define YYPARSE_PARAM_ARG YYPARSE_PARAM
#  define YYPARSE_PARAM_DECL void *YYPARSE_PARAM;
# endif
#else /* !YYPARSE_PARAM */
# define YYPARSE_PARAM_ARG
# define YYPARSE_PARAM_DECL
#endif /* !YYPARSE_PARAM */

/* Prevent warning if -Wstrict-prototypes.  */
#ifdef __GNUC__
# ifdef YYPARSE_PARAM
int yyparse (void *);
# else
int yyparse (void);
# endif
#endif

/* YY_DECL_VARIABLES -- depending whether we use a pure parser,
   variables are global, or local to YYPARSE.  */

#define YY_DECL_NON_LSP_VARIABLES			\
/* The lookahead symbol.  */				\
int yychar;						\
							\
/* The semantic value of the lookahead symbol. */	\
YYSTYPE yylval;						\
							\
/* Number of parse errors so far.  */			\
int yynerrs;

#if YYLSP_NEEDED
# define YY_DECL_VARIABLES			\
YY_DECL_NON_LSP_VARIABLES			\
						\
/* Location data for the lookahead symbol.  */	\
YYLTYPE yylloc;
#else
# define YY_DECL_VARIABLES			\
YY_DECL_NON_LSP_VARIABLES
#endif


/* If nonreentrant, generate the variables here. */

#if !YYPURE
YY_DECL_VARIABLES
#endif  /* !YYPURE */

int
yyparse (YYPARSE_PARAM_ARG)
     YYPARSE_PARAM_DECL
{
  /* If reentrant, generate the variables here. */
#if YYPURE
  YY_DECL_VARIABLES
#endif  /* !YYPURE */

  register int yystate;
  register int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Lookahead token as an internal (translated) token number.  */
  int yychar1 = 0;

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

  /* The state stack. */
  short	yyssa[YYINITDEPTH];
  short *yyss = yyssa;
  register short *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  register YYSTYPE *yyvsp;

#if YYLSP_NEEDED
  /* The location stack.  */
  YYLTYPE yylsa[YYINITDEPTH];
  YYLTYPE *yyls = yylsa;
  YYLTYPE *yylsp;
#endif

#if YYLSP_NEEDED
# define YYPOPSTACK   (yyvsp--, yyssp--, yylsp--)
#else
# define YYPOPSTACK   (yyvsp--, yyssp--)
#endif

  YYSIZE_T yystacksize = YYINITDEPTH;


  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
#if YYLSP_NEEDED
  YYLTYPE yyloc;
#endif

  /* When reducing, the number of symbols on the RHS of the reduced
     rule. */
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
#if YYLSP_NEEDED
  yylsp = yyls;
#endif
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

  if (yyssp >= yyss + yystacksize - 1)
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
	   data in use in that stack, in bytes.  */
# if YYLSP_NEEDED
	YYLTYPE *yyls1 = yyls;
	/* This used to be a conditional around just the two extra args,
	   but that might be undefined if yyoverflow is a macro.  */
	yyoverflow ("parser stack overflow",
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yyls1, yysize * sizeof (*yylsp),
		    &yystacksize);
	yyls = yyls1;
# else
	yyoverflow ("parser stack overflow",
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yystacksize);
# endif
	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyoverflowlab;
# else
      /* Extend the stack our own way.  */
      if (yystacksize >= YYMAXDEPTH)
	goto yyoverflowlab;
      yystacksize *= 2;
      if (yystacksize > YYMAXDEPTH)
	yystacksize = YYMAXDEPTH;

      {
	short *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyoverflowlab;
	YYSTACK_RELOCATE (yyss);
	YYSTACK_RELOCATE (yyvs);
# if YYLSP_NEEDED
	YYSTACK_RELOCATE (yyls);
# endif
# undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;
#if YYLSP_NEEDED
      yylsp = yyls + yysize - 1;
#endif

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyssp >= yyss + yystacksize - 1)
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
  if (yyn == YYFLAG)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* yychar is either YYEMPTY or YYEOF
     or a valid token in external form.  */

  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  /* Convert token to internal form (in yychar1) for indexing tables with */

  if (yychar <= 0)		/* This means end of input. */
    {
      yychar1 = 0;
      yychar = YYEOF;		/* Don't call YYLEX any more */

      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yychar1 = YYTRANSLATE (yychar);

#if YYDEBUG
     /* We have to keep this `#if YYDEBUG', since we use variables
	which are defined only if `YYDEBUG' is set.  */
      if (yydebug)
	{
	  YYFPRINTF (stderr, "Next token is %d (%s",
		     yychar, yytname[yychar1]);
	  /* Give the individual parser a way to print the precise
	     meaning of a token, for further debugging info.  */
# ifdef YYPRINT
	  YYPRINT (stderr, yychar, yylval);
# endif
	  YYFPRINTF (stderr, ")\n");
	}
#endif
    }

  yyn += yychar1;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != yychar1)
    goto yydefault;

  yyn = yytable[yyn];

  /* yyn is what to do for this token type in this state.
     Negative => reduce, -yyn is rule number.
     Positive => shift, yyn is new state.
       New state is final state => don't bother to shift,
       just return success.
     0, or most negative number => error.  */

  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrlab;

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Shift the lookahead token.  */
  YYDPRINTF ((stderr, "Shifting token %d (%s), ",
	      yychar, yytname[yychar1]));

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;
#if YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

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

     Otherwise, the following line sets YYVAL to the semantic value of
     the lookahead token.  This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];

#if YYLSP_NEEDED
  /* Similarly for the default location.  Let the user run additional
     commands if for instance locations are ranges.  */
  yyloc = yylsp[1-yylen];
  YYLLOC_DEFAULT (yyloc, (yylsp - yylen), yylen);
#endif

#if YYDEBUG
  /* We have to keep this `#if YYDEBUG', since we use variables which
     are defined only if `YYDEBUG' is set.  */
  if (yydebug)
    {
      int yyi;

      YYFPRINTF (stderr, "Reducing via rule %d (line %d), ",
		 yyn, yyrline[yyn]);

      /* Print the symbols being reduced, and their result.  */
      for (yyi = yyprhs[yyn]; yyrhs[yyi] > 0; yyi++)
	YYFPRINTF (stderr, "%s ", yytname[yyrhs[yyi]]);
      YYFPRINTF (stderr, " -> %s\n", yytname[yyr1[yyn]]);
    }
#endif

  switch (yyn) {

case 3:
#line 122 "query_parser_bison.y"
{
    CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        
    yyvsp[-4]->MoveSubnodes(yyvsp[-3]);    
    yyvsp[-4]->InsertNode(yyvsp[-4]->SubNodeBegin(), yyvsp[-3]);    
        
    
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
    env->SetFROM_Context(0);
    
    yyval = yyvsp[-4];
    env->AttachQueryTree(yyval);
    
    ;
    break;}
case 4:
#line 153 "query_parser_bison.y"
{
    yyval=0;
    ;
    break;}
case 5:
#line 158 "query_parser_bison.y"
{
    CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
    yyvsp[-1]->AddNode(yyvsp[0]);
    env->ForgetPoolNode(yyvsp[0]);
    yyval = yyvsp[-1];
    ;
    break;}
case 6:
#line 168 "query_parser_bison.y"
{
        yyval = yyvsp[0];
    ;
    break;}
case 7:
#line 172 "query_parser_bison.y"
{
        yyval = yyvsp[-2];
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);        
        yyval->AddNode(yyvsp[0]);
        env->ForgetPoolNode(yyvsp[0]);
    ;
    break;}
case 8:
#line 184 "query_parser_bison.y"
{
        yyval = yyvsp[0];         
    ;
    break;}
case 9:
#line 189 "query_parser_bison.y"
{
        yyval = yyvsp[0];         
    ;
    break;}
case 10:
#line 194 "query_parser_bison.y"
{
        yyval = yyvsp[0];         
    ;
    break;}
case 11:
#line 201 "query_parser_bison.y"
{
        yyval = yyvsp[0];
    ;
    break;}
case 12:
#line 205 "query_parser_bison.y"
{
        yyval = yyvsp[-2];    
        AddFunc_Arg(parm, yyvsp[0]);
    ;
    break;}
case 15:
#line 220 "query_parser_bison.y"
{
    ;
    break;}
case 16:
#line 224 "query_parser_bison.y"
{
    
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        yyval = env->QTree().CreateNode(CQueryParseNode::eFieldSearch, yyvsp[-1], yyvsp[0]);
        env->AttachQueryTree(yyval);
        env->ForgetPoolNodes(yyvsp[-1], yyvsp[0]);
    ;
    break;}
case 17:
#line 233 "query_parser_bison.y"
{
        yyerrok;
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        yyval = env->QTree().CreateNode(CQueryParseNode::eAnd, yyvsp[-1], yyvsp[0]);
        yyval->GetValue().SetExplicit(false);
        env->AttachQueryTree(yyval);
        env->ForgetPoolNodes(yyvsp[-1], yyvsp[0]);
    ;
    break;}
case 18:
#line 244 "query_parser_bison.y"
{
        yyval = yyvsp[-1];
    ;
    break;}
case 19:
#line 254 "query_parser_bison.y"
{
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 20:
#line 259 "query_parser_bison.y"
{
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 21:
#line 264 "query_parser_bison.y"
{
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 22:
#line 269 "query_parser_bison.y"
{
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 23:
#line 274 "query_parser_bison.y"
{
        yyval = yyvsp[-3];
        yyval->AddNode(yyvsp[-4]);
        yyval->AddNode(yyvsp[-2]);
        yyval->AddNode(yyvsp[0]);

        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        env->AttachQueryTree(yyval);    
        env->ForgetPoolNodes(yyvsp[-4], yyvsp[-2]);
        env->ForgetPoolNodes(yyvsp[-4], yyvsp[0]);
    ;
    break;}
case 24:
#line 287 "query_parser_bison.y"
{
        yyval = yyvsp[-3];
        yyval->AddNode(yyvsp[-4]);
        yyval->AddNode(yyvsp[-2]);
        yyval->AddNode(yyvsp[0]);

        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        env->AttachQueryTree(yyval);    
        env->ForgetPoolNodes(yyvsp[-4], yyvsp[-2]);
        env->ForgetPoolNodes(yyvsp[-4], yyvsp[0]);        
    ;
    break;}
case 25:
#line 301 "query_parser_bison.y"
{
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 26:
#line 306 "query_parser_bison.y"
{
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 27:
#line 311 "query_parser_bison.y"
{
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 28:
#line 316 "query_parser_bison.y"
{
        yyval = yyvsp[-3];
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        env->AttachQueryTree(yyval);    
        env->ForgetPoolNodes(yyvsp[-4], yyvsp[-1]);
        yyval->InsertNode(yyval->SubNodeBegin(), yyvsp[-1]);
        yyval->InsertNode(yyval->SubNodeBegin(), yyvsp[-4]);
        env->SetIN_Context(0);
    ;
    break;}
case 29:
#line 327 "query_parser_bison.y"
{
        yyval = yyvsp[-3];
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        env->AttachQueryTree(yyval);    
        env->ForgetPoolNodes(yyvsp[-4], yyvsp[-1]);
        yyval->InsertNode(yyval->SubNodeBegin(), yyvsp[-1]);
        yyval->InsertNode(yyval->SubNodeBegin(), yyvsp[-4]);
        env->SetIN_Context(0);
    ;
    break;}
case 30:
#line 338 "query_parser_bison.y"
{
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 31:
#line 343 "query_parser_bison.y"
{
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 32:
#line 347 "query_parser_bison.y"
{
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 33:
#line 351 "query_parser_bison.y"
{
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 34:
#line 355 "query_parser_bison.y"
{
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 35:
#line 359 "query_parser_bison.y"
{
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 36:
#line 363 "query_parser_bison.y"
{ 
        yyval = yyvsp[-1];
    ;
    break;}
case 37:
#line 368 "query_parser_bison.y"
{
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[-2], yyvsp[0]);
    ;
    break;}
case 38:
#line 373 "query_parser_bison.y"
{
        QTreeAddNode(parm, yyval = yyvsp[-1], yyvsp[0], 0);
    ;
    break;}
case 39:
#line 382 "query_parser_bison.y"
{
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        QTreeAddNode(parm, yyval = yyvsp[0], 0, 0);
    ;
    break;}
case 40:
#line 391 "query_parser_bison.y"
{ 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        QTreeAddNode(parm, yyval = yyvsp[0], 0, 0);
    ;
    break;}
case 41:
#line 400 "query_parser_bison.y"
{ 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        QTreeAddNode(parm, yyval = yyvsp[0], 0, 0);
    ;
    break;}
case 42:
#line 409 "query_parser_bison.y"
{ 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        QTreeAddNode(parm, yyval = yyvsp[-1], 0, 0);
    ;
    break;}
case 43:
#line 421 "query_parser_bison.y"
{ 
        yyerrok;
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error. Unbalanced parenthesis");        
        }
        QTreeAddNode(parm, yyval = yyvsp[-1], 0, 0);
    ;
    break;}
case 44:
#line 431 "query_parser_bison.y"
{ 
        yyerrok; 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        QTreeAddNode(parm, yyval = yyvsp[-2], 0, 0);
    ;
    break;}
case 45:
#line 441 "query_parser_bison.y"
{ 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        yyerrok; 
        QTreeAddNode(parm, yyval = yyvsp[-2], 0, 0);
    ;
    break;}
case 46:
#line 451 "query_parser_bison.y"
{ 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        yyerrok; 
        QTreeAddNode(parm, yyval = yyvsp[-2], 0, 0);
    ;
    break;}
case 47:
#line 461 "query_parser_bison.y"
{ 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        yyerrok; 
        QTreeAddNode(parm, yyval = yyvsp[-2], 0, 0);
    ;
    break;}
case 48:
#line 471 "query_parser_bison.y"
{ 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        yyerrok; 
        QTreeAddNode(parm, yyval = yyvsp[-2], 0, 0);
    ;
    break;}
}

#line 705 "/usr/share/bison/bison.simple"


  yyvsp -= yylen;
  yyssp -= yylen;
#if YYLSP_NEEDED
  yylsp -= yylen;
#endif

#if YYDEBUG
  if (yydebug)
    {
      short *yyssp1 = yyss - 1;
      YYFPRINTF (stderr, "state stack now");
      while (yyssp1 != yyssp)
	YYFPRINTF (stderr, " %d", *++yyssp1);
      YYFPRINTF (stderr, "\n");
    }
#endif

  *++yyvsp = yyval;
#if YYLSP_NEEDED
  *++yylsp = yyloc;
#endif

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTBASE] + *yyssp;
  if (yystate >= 0 && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTBASE];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;

#ifdef YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (yyn > YYFLAG && yyn < YYLAST)
	{
	  YYSIZE_T yysize = 0;
	  char *yymsg;
	  int yyx, yycount;

	  yycount = 0;
	  /* Start YYX at -YYN if negative to avoid negative indexes in
	     YYCHECK.  */
	  for (yyx = yyn < 0 ? -yyn : 0;
	       yyx < (int) (sizeof (yytname) / sizeof (char *)); yyx++)
	    if (yycheck[yyx + yyn] == yyx)
	      yysize += yystrlen (yytname[yyx]) + 15, yycount++;
	  yysize += yystrlen ("parse error, unexpected ") + 1;
	  yysize += yystrlen (yytname[YYTRANSLATE (yychar)]);
	  yymsg = (char *) YYSTACK_ALLOC (yysize);
	  if (yymsg != 0)
	    {
	      char *yyp = yystpcpy (yymsg, "parse error, unexpected ");
	      yyp = yystpcpy (yyp, yytname[YYTRANSLATE (yychar)]);

	      if (yycount < 5)
		{
		  yycount = 0;
		  for (yyx = yyn < 0 ? -yyn : 0;
		       yyx < (int) (sizeof (yytname) / sizeof (char *));
		       yyx++)
		    if (yycheck[yyx + yyn] == yyx)
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
	    yyerror ("parse error; also virtual memory exhausted");
	}
      else
#endif /* defined (YYERROR_VERBOSE) */
	yyerror ("parse error");
    }
  goto yyerrlab1;


/*--------------------------------------------------.
| yyerrlab1 -- error raised explicitly by an action |
`--------------------------------------------------*/
yyerrlab1:
  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      /* return failure if at end of input */
      if (yychar == YYEOF)
	YYABORT;
      YYDPRINTF ((stderr, "Discarding token %d (%s).\n",
		  yychar, yytname[yychar1]));
      yychar = YYEMPTY;
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */

  yyerrstatus = 3;		/* Each real token shifted decrements this */

  goto yyerrhandle;


/*-------------------------------------------------------------------.
| yyerrdefault -- current state does not do anything special for the |
| error token.                                                       |
`-------------------------------------------------------------------*/
yyerrdefault:
#if 0
  /* This is wrong; only states that explicitly want error tokens
     should shift them.  */

  /* If its default is to accept any token, ok.  Otherwise pop it.  */
  yyn = yydefact[yystate];
  if (yyn)
    goto yydefault;
#endif


/*---------------------------------------------------------------.
| yyerrpop -- pop the current state because it cannot handle the |
| error token                                                    |
`---------------------------------------------------------------*/
yyerrpop:
  if (yyssp == yyss)
    YYABORT;
  yyvsp--;
  yystate = *--yyssp;
#if YYLSP_NEEDED
  yylsp--;
#endif

#if YYDEBUG
  if (yydebug)
    {
      short *yyssp1 = yyss - 1;
      YYFPRINTF (stderr, "Error: state stack now");
      while (yyssp1 != yyssp)
	YYFPRINTF (stderr, " %d", *++yyssp1);
      YYFPRINTF (stderr, "\n");
    }
#endif

/*--------------.
| yyerrhandle.  |
`--------------*/
yyerrhandle:
  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yyerrdefault;

  yyn += YYTERROR;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != YYTERROR)
    goto yyerrdefault;

  yyn = yytable[yyn];
  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrpop;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrpop;

  if (yyn == YYFINAL)
    YYACCEPT;

  YYDPRINTF ((stderr, "Shifting error token, "));

  *++yyvsp = yylval;
#if YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

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

/*---------------------------------------------.
| yyoverflowab -- parser overflow comes here.  |
`---------------------------------------------*/
yyoverflowlab:
  yyerror ("parser stack overflow");
  yyresult = 2;
  /* Fall through.  */

yyreturn:
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
  return yyresult;
}
#line 493 "query_parser_bison.y"


