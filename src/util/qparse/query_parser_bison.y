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

%}

%pure_parser   /* Reentrant parser */

%token IDENT
%token STRING
%token NUM_INT
%token SELECT
%token FROM
%token WHERE
%token FUNCTION


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
%left BETWEEN
%left NOT_BETWEEN
%left LIKE
%left NOT_LIKE
%left IN
%left NOT_IN

%%

input :
    exp
    | select_clause
;



select_clause :
    /* select clause */
    SELECT obj_list FROM obj_list opt_where
    {
    CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);

    /* create a pseudo node to isolate select list in a dedicated subtree */
    //CQueryParseTree::TNode* qnode = 0;
    //qnode = env->QTree().CreateNode(CQueryParseNode::eList, 0, 0, "LIST");    
    //qnode->MoveSubnodes($2);
    CQueryParseTree::TNode* qnode = env->GetContext();
    if (qnode) {
        qnode->InsertNode(qnode->SubNodeBegin(), $2);
    }

    //$1->InsertNode($1->SubNodeBegin(),qnode);                
    
    $1->AddNode($3);
    $3->MoveSubnodes($4);
    $3->InsertNode($3->SubNodeBegin(), $4);    
    if ($5) {
        $1->AddNode($5);
    }
    
    env->ForgetPoolNode($1);
    env->ForgetPoolNode($2);
    env->ForgetPoolNode($3);
    env->ForgetPoolNode($4);
    env->ForgetPoolNode($5);
    
    env->SetSELECT_Context(0);
    env->SetContext(0);
    env->SetFROM_Context(0);
    
    $$ = $1;
    env->AttachQueryTree($$);
    
    }
;

opt_where :
    /* empty */
    {
    $$=0;
    }
    |
    WHERE exp
    {
    CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
    $1->AddNode($2);
    env->ForgetPoolNode($2);
    $$ = $1;
    }
;

functional:
    FUNCTION '(' scalar_list ')'
    {
        $$ = $1;
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        env->AttachQueryTree($$);    
        $$->InsertNode($$->SubNodeBegin(), $3);
        env->SetContext(0);
        env->ForgetPoolNodes($1, $3);    
    }  
;

obj_list:
    scalar_value
    {
        $$ = $1;
    }
    | obj_list ',' scalar_value
    {
        $$ = $1;
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        AddFunc_Arg(parm, $3);        
        //$$->AddNode($3);
        env->ForgetPoolNode($3);
    }    
;

scalar_value :
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
    | functional
    {
        $$ = $1;         
    }
;

scalar_list:
    scalar_value
    {
        $$ = $1;
    }    
    | scalar_list ',' scalar_value
    {
        $$ = $1;    
        AddFunc_Arg(parm, $3);
    }    
;

/* IN sub-expression */
in_sub_expr:
    scalar_list
    |
    select_clause
;

exp :
    scalar_value
    {
        $$ = $1;
    }
    /* Function call */
    |  functional
    {
        $$ = $1;
    }    
    /* Field search */
    | STRING IDENT
    {    
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        $$ = env->QTree().CreateNode(CQueryParseNode::eFieldSearch, $1, $2);
        env->AttachQueryTree($$);
        env->ForgetPoolNodes($1, $2);
    }
    /* concatenated expressions are implicit ANDs */    
    | exp exp
    {
        yyerrok;
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        $$ = env->QTree().CreateNode(CQueryParseNode::eAnd, $1, $2);
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
    /* BETWEEN */
    | scalar_value BETWEEN scalar_value AND scalar_value
    {
        $$ = $2;
        $$->AddNode($1);
        $$->AddNode($3);
        $$->AddNode($5);

        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        env->AttachQueryTree($$);    
        env->ForgetPoolNodes($1, $3);
        env->ForgetPoolNodes($1, $5);
    }
    /* NOT BETWEEN */
    | scalar_value NOT_BETWEEN scalar_value AND scalar_value
    {
        $$ = $2;
        $$->AddNode($1);
        $$->AddNode($3);
        $$->AddNode($5);

        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        env->AttachQueryTree($$);    
        env->ForgetPoolNodes($1, $3);
        env->ForgetPoolNodes($1, $5);        
    }
    
    /* RANGE */
    | exp RANGE exp
    {
        QTreeAddNode(parm, $$ = $2, $1, $3);
    }
    /* LIKE */
    | scalar_value LIKE scalar_value
    {
        QTreeAddNode(parm, $$ = $2, $1, $3);
    }
    /* NOT LIKE */
    | scalar_value NOT_LIKE scalar_value
    {
        QTreeAddNode(parm, $$ = $2, $1, $3);
    }
    /* IN */
    | scalar_value IN '(' in_sub_expr ')'
    {
        $$ = $2;
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        env->AttachQueryTree($$);    
        env->ForgetPoolNodes($1, $4);
        $$->InsertNode($$->SubNodeBegin(), $4);
        $$->InsertNode($$->SubNodeBegin(), $1);
        env->SetIN_Context(0);
    }
    /* NOT IN */
    | scalar_value NOT_IN '(' in_sub_expr ')'
    {
        $$ = $2;
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        env->AttachQueryTree($$);    
        env->ForgetPoolNodes($1, $4);
        $$->InsertNode($$->SubNodeBegin(), $4);
        $$->InsertNode($$->SubNodeBegin(), $1);
        env->SetIN_Context(0);
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
    /* NOT
    | exp NOT exp
    {
        QTreeAddNode(parm, $$ = $2, $1, $3);
    }
*/
    /* unary NOT */
    | NOT exp
    {
        QTreeAddNode(parm, $$ = $1, $2, 0);
    }

    /*
     * error cases
     */
     
    | error STRING   
    {
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        QTreeAddNode(parm, $$ = $2, 0, 0);
    }
    | error IDENT   
    { 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        QTreeAddNode(parm, $$ = $2, 0, 0);
    }
    | error NUM_INT   
    { 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        QTreeAddNode(parm, $$ = $2, 0, 0);
    }    
    | exp error    
    { 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        QTreeAddNode(parm, $$ = $1, 0, 0);
    }

    /* unbalanced parenthesis at the end-of-input */
    
    | '(' exp error 
    { 
        yyerrok;
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error. Unbalanced parenthesis");        
        }
        QTreeAddNode(parm, $$ = $2, 0, 0);
    }
    | exp OR error 
    { 
        yyerrok; 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        QTreeAddNode(parm, $$ = $1, 0, 0);
    }
    | exp XOR error 
    { 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        yyerrok; 
        QTreeAddNode(parm, $$ = $1, 0, 0);
    }
    | exp NOT error 
    { 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        yyerrok; 
        QTreeAddNode(parm, $$ = $1, 0, 0);
    }
    | exp AND error 
    { 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        yyerrok; 
        QTreeAddNode(parm, $$ = $1, 0, 0);
    }
    | exp SUB error 
    { 
        CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(parm);
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error.");        
        }
        yyerrok; 
        QTreeAddNode(parm, $$ = $1, 0, 0);
    }

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

