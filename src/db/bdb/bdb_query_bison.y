/*  $Id$
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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:
 *   Bison grammar description for simple query language. 
 *
 */

/* *************************** */
/* C includes and definitions  */
/* *************************** */

%{

#define YYSTYPE        CBDB_Query::TQueryClause*
#define YYPARSE_PARAM  parm
#define YYLEX_PARAM    parm
#define YYINITDEPTH    50
#define YYMAXDEPTH     1000

#define YYDEBUG 1


/* 
    Utility function to save current yyparse result in the
    parsing environment object (context).
*/
inline static 
void BisonSaveStageResult(YYSTYPE res, void* parm)
{
    CBDB_QueryParserEnvironment* env = 
          (CBDB_QueryParserEnvironment*) parm;
    env->AttachQueryClause(res);
    env->AddNodeToPool(res);
}


%}

/* *************************** */
/* BISON Declarations          */
/* *************************** */


%pure_parser     /* Reentrant parser */

%token NAME
%token STRING
%token NUM

%left NOT
%left AND
%left OR
%left EQ
%left NOTEQ
%left GT
%left GE
%left LT
%left LE



/* Grammar follows */
%%
input:  exp
;
/*
input:  input0
        | input0 exp

input0:    
        | input0 '\n' exp
        | input0 ';' exp
;
*/

exp:    
		NUM                
        {
            $$ = $1;         
        }
        | STRING
        {
            $$ = $1;         
        }
        | NAME
        {
            $$ = $1;         
        }
        | exp AND exp        
        { 
            $$ = CBDB_Query::NewLogicalNode(CBDB_QueryNode::eAnd, $1, $3);
            BisonSaveStageResult($$, parm);
        }
        | exp OR exp
        {
            $$ = CBDB_Query::NewLogicalNode(CBDB_QueryNode::eOr, $1, $3);
            BisonSaveStageResult($$, parm);
        }
        | exp EQ exp
        {
            $$ = CBDB_Query::NewOperatorNode(CBDB_QueryNode::eEQ, $1, $3);
            BisonSaveStageResult($$, parm);
        }
        | exp NOTEQ exp
        {
            $$ = CBDB_Query::NewOperatorNode(CBDB_QueryNode::eEQ, $1, $3);
            $$->GetValue().SetNot();
            BisonSaveStageResult($$, parm);
        }
        | exp GT exp
        {
            $$ = CBDB_Query::NewOperatorNode(CBDB_QueryNode::eGT, $1, $3);
            BisonSaveStageResult($$, parm);
        }
        | exp GE exp
        {
            $$ = CBDB_Query::NewOperatorNode(CBDB_QueryNode::eGE, $1, $3);
            BisonSaveStageResult($$, parm);
        }
        | exp LT exp
        {
            $$ = CBDB_Query::NewOperatorNode(CBDB_QueryNode::eLT, $1, $3);
            BisonSaveStageResult($$, parm);
        }
        | exp LE exp
        {
            $$ = CBDB_Query::NewOperatorNode(CBDB_QueryNode::eLE, $1, $3);
            BisonSaveStageResult($$, parm);
        }
        | '(' exp ')'
        { 
            $$ = $2;
        } 
        | NOT exp
        {
            $$ = CBDB_Query::NewLogicalNode(CBDB_QueryNode::eNot, $2, 0);
            BisonSaveStageResult($$, parm);
        }

;
%%
