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
 *	  Lexical analyser for query parser.
 *
 */

USING_NCBI_SCOPE;

/// @internal
static
CBDB_Query::TQueryClause* LexerMakeTokenNode(CBDB_QueryParserEnvironment* env,
                                             const string& token)
{
    CBDB_Query::TQueryClause* qnode = 
             new CBDB_Query::TQueryClause(CBDB_QueryNode(token));
    env->AddNodeToPool(qnode);
    return qnode;    
}

/// Simple lexer to extract tokens from the query stream(string) 
///
/// This variant of lexical analyser works with the reentrant parser
/// so it receives YYSTYPE lvalp variable which is a pointer on the 
/// current member somewhere in the BISON stack.
///
/// Additional void* param is an additional environment variable
/// (carries both the query string to parse and the resulting syntax tree)
/// Lexer uses environment a state storage for the lexer's finit automata.
static
int yylex(YYSTYPE *lvalp, void* param)
{
    static int char_tokens[]      = { '&', '|', '=' };
    static int char_token_codes[] = { AND, OR,  EQ };

    int c, i;
    string token;

    CBDB_QueryParserEnvironment* env = (CBDB_QueryParserEnvironment*) param;

    // Skip white space 
    while (isspace(c = env->GetChar()));

    // Process single char tokens
    for (i = 0; i < sizeof(char_tokens) / sizeof(char_tokens[0]); ++i) {
        if (c == char_tokens[i]) {
            return char_token_codes[i];
        }
    }

    // Process string tokens in apostrophes
    if (c == '\'') {
        const char* tok_start = env->GetBufPtr() - 1;
        do {
            c = env->GetChar();
            if (c == 0) {
                string err_msg = "Invalid string ";
                err_msg.append(tok_start);
                BDB_THROW(eQuerySyntaxError, err_msg);
            }
        } while (c != '\'');

        const char* tok_end = env->GetBufPtr();
        token.append(tok_start, tok_end - tok_start);

        *lvalp = LexerMakeTokenNode(env, token);

        return STRING;
    }

    // Process name-like tokens
    if (isalpha(c)) {
        const char* tok_start = env->GetBufPtr() - 1;
        do {
            c = env->GetChar();            
        } while (isalnum(c) || c == '_');

        env->UnGetChar();
        const char* tok_end = env->GetBufPtr();
        token.append(tok_start, tok_end - tok_start);

        *lvalp = LexerMakeTokenNode(env, token);
        return NAME;
    }


    // process numbers   
    while (isdigit(c)) {
        token.append(1, c);
        c = env->GetChar();
    }
    if (!token.empty()) {
        env->UnGetChar();
        *lvalp = LexerMakeTokenNode(env, token);

        return NUM;
    }
    /* return end-of-file  */
    if (c == EOF || c == 0)
        return 0;
    /* return single chars */
    return c;
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2004/02/24 16:37:38  kuznets
 * Initial revision
 *
 *
 * ==========================================================================
 */

