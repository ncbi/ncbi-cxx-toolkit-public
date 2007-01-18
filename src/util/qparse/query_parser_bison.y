/*  $Id: query_parser_bison.y,v 1.2 2007/01/11 14:49:52 kuznets Exp $
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
%left RANGE
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
    /* Field search */
    | STRING IDENT
    {
    
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        $$ = env->QTree().CreateBinaryNode(CQueryParseNode::eFieldSearch, $1, $2);
        env->AttachQueryTree($$);
        env->ForgetPoolNodes($1, $2);
    }
    /* concatenated expressions are implicit ANDs */    
    | exp exp
    {
        yyerrok;
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        $$ = env->QTree().CreateBinaryNode(CQueryParseNode::eAnd, $1, $2);
        $$->GetValue().SetExplicit(false);
        env->AttachQueryTree($$);
        env->ForgetPoolNodes($1, $2);
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
        QTreeAddNode(parm, $$ = $2, $1, $3);
    }
    /* MINUS */
    | exp SUB exp
    {
        QTreeAddNode(parm, $$ = $2, $1, $3);
    }
    /* OR */
    | exp OR exp
    {
        QTreeAddNode(parm, $$ = $2, $1, $3);
    }
    /* XOR */
    | exp XOR exp
    {
        QTreeAddNode(parm, $$ = $2, $1, $3);
    }
    /* RANGE */
    | exp RANGE exp
    {
        QTreeAddNode(parm, $$ = $2, $1, $3);
    }
    /* == */
    | exp EQ exp
    {
        QTreeAddNode(parm, $$ = $2, $1, $3);
    }
    /* != */
    | exp NOTEQ exp
    {
        QTreeAddNode(parm, $$ = $2, $1, $3);
    }
    | exp GT exp
    {
        QTreeAddNode(parm, $$ = $2, $1, $3);
    }
    | exp GE exp
    {
        QTreeAddNode(parm, $$ = $2, $1, $3);
    }
    | exp LT exp
    {
        QTreeAddNode(parm, $$ = $2, $1, $3);
    }
    | exp LE exp
    {
        QTreeAddNode(parm, $$ = $2, $1, $3);
    }
    | '(' exp ')'
    { 
        $$ = $2;
    } 
    /* NOT */    
    | exp NOT exp
    {
        QTreeAddNode(parm, $$ = $2, $1, $3);
    }
    /* unary NOT */
    | NOT exp
    {
        QTreeAddNode(parm, $$ = $1, $2, 0);
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

