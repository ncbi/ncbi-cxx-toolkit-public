/*  $Id: query_parser_bison.y,v 1.1 2007/01/10 16:14:01 kuznets Exp $
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author: Anatoliy Kuznetsov, Mike DiCuccio
 *
 * File Description:
 *   Bison grammar description for simple query language. 
 *
 */

%{

#define YYSTYPE         CQueryParseTree::TNode*
#define YYPARSE_PARAM   parm
#define YYLEX_PARAM     parm
#define YYMAXDEPTH      100000

#define YYDEBUG         1
#define YYERROR_VERBOSE 1

/* 
    Utility function to save current yyparse result in the
    parsing environment object (context).
*/
inline static 
void BisonSaveStageResult(YYSTYPE res, void* parm)
{
    CQueryParserEnv* env = (CQueryParserEnv*) parm;
    env->AttachQueryClause(res);
    env->AddNodeToPool(res);
}


%}

%pure_parser   /* Reentrant parser */

%token IDENT
%token STRING
%token NUM_INT


%left AND
%left NOT
%left OR
%left SUB
%left XOR
%left EQ
%left NOTEQ
%left GT
%left GE
%left LT
%left LE


%%

input :
    exp
/*	
    {
        CQueryParserEnv* env = (CQueryParserEnv*)parm;
        env->SetParseTree($1);
    }
*/	
;

exp :
    /* integer constant */
	NUM_INT
    {
        $$ = $1;         
    }
	
	/* text */
    | STRING
    {
        $$ = $1;         
    }
	
	/* pure string identifier */
    | IDENT
    {
        $$ = $1;         
    }

    /* concatenated expressions are implicit ANDs */
    | exp exp
    {
        yyerrok;
        $$ = CQueryParseTree::CreateBinaryNode(CQueryParseNode::eAnd, $1, $2);
    }

    /* parenthetical balanced expressions */
    | '(' exp ')'
    {
        $$ = $2;
    }

    /*
     * LOGICAL OPS
     */

    /* AND */
    | exp AND exp
    {
        $$ = CQueryParseTree::CreateBinaryNode(CQueryParseNode::eAnd, $1, $3);
		BisonSaveStageResult($$, parm);
    }
	
    /* MINUS */
    | exp SUB exp
    {
        $$ = CQueryParseTree::CreateBinaryNode(CQueryParseNode::eSub, $1, $3);
		BisonSaveStageResult($$, parm);
    }

    /* OR */
    | exp OR exp
    {
        $$ = CQueryParseTree::CreateBinaryNode(CQueryParseNode::eOr, $1, $3);
		BisonSaveStageResult($$, parm);
    }

    /* NOT */	
    | exp NOT exp
    {
        $$ = CQueryParseTree::CreateBinaryNode(CQueryParseNode::eNot2, $1, $3);
    }
    /* unary NOT */
    | NOT exp
    {
        $$ = CQueryParseTree::CreateUnaryNode(CQueryParseNode::eNot, $2);
		BisonSaveStageResult($$, parm);
    }

    /*
     * error cases
     */
/*	 
    | error TEXT   { $$ = $2; }
    | exp error    { $$ = $1; }
*/
    /* unbalanced parenthesis at the end-of-input */
/*	
    | '(' exp error { yyerrok; $$ = $2; }
    | exp OR  error { yyerrok; $$ = $1 }
    | exp NOT error { yyerrok; $$ = $1 }
    | exp AND error { yyerrok; $$ = $1 }
*/
/**
    | AND exp error
    {
        $$ = CQueryParseTree::CreateOpNode(CQueryParseNode::eAnd, 0, $2);
    }
    | OR exp error
    {
        $$ = CQueryParseTree::CreateOpNode(CQueryParseNode::eOr, 0, $2);
    }
**/
;

%%

