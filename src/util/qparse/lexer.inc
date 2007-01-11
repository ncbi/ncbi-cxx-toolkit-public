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
 * Author: Mike DiCuccio, Anatoliy Kuznetsov
 *
 * File Description:
 *      Lexical analyser for query parser.
 *
 */

USING_NCBI_SCOPE;

/*
/// @internal
static
CQueryParseTree::TNode* LexerMakeTokenNode(CQueryParserEnv* env,
                                           const string& token)
{
    CQueryParseTree::TNode* qnode = 
             new CQueryParseTree::TNode(CQueryParseNode(token));
    env->AddNodeToPool(qnode);
    return qnode;
}
*/

///
/// @internal
///
static 
bool s_Compare(const char* p, const char* p_end,
               const char* t,
               CQueryParseTree::ECase  case_sense)
{
    if (case_sense == CQueryParseTree::eCaseInsensitive) {
        for ( ;  p != p_end  &&  t  &&  *t;  ++p, ++t) {
            if (toupper(*p) != *t) {
                return false;
            }
        }
        return !*t  &&  ( !*p  ||  isspace(*p));
    } else {
        for ( ;  p != p_end  &&  t  &&  *t;  ++p, ++t) {
            if (*p != *t) {
                return false;
            }
        }
        return !*t  &&  ( !*p  ||  isspace(*p));
    }
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
///
/// @internal
///
static
int yylex(CQueryParseTree::TNode** lvalp, void* param)
{
    CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(param);
    bool verbose = env->IsVerbose();
    char c;

    /// skip leading white space
    while ( isspace(c = env->GetChar()) ) {
    }

    const char* ptr = env->GetBufPtr();
    const char* end = env->GetBufEnd();
    const char* token_end = 0;
    
    CQueryParseTree::ECase  case_sense = env->GetCase();
    switch (case_sense) {
    case CQueryParseTree::eCaseSensitiveUpper:
        // nothing to do
        break;
    case CQueryParseTree::eCaseInsensitive:
        c = toupper(c);
        break;
    default:
        _ASSERT(0);
    }

    switch (c) {
    // AND
    case 'A':
        if ( !s_Compare(ptr, end, "ND", case_sense) ) {
            break;
        }
        ptr += 2;
        // must be "AND somethig" not "ANDsomethig"
        if (ptr != end  &&  !isspace(*ptr) ) {
            break;
        }
        env->Skip(2);
        if (verbose) { _TRACE("token: AND"); }
        return AND;
    // &&  as AND
    case '&':
        if ( *ptr != '&' ) {
            break;
        }
        /*
        ptr += 1;
        if (ptr != end  &&  !isspace(*ptr) ) {
            break;
        }
        */
        env->Skip(1);
        if (verbose) { _TRACE("token: &&"); }
        return AND;    
    // SUB
    case 'S':
        if ( !s_Compare(ptr, end, "UB", case_sense) ) {
            break;
        }
        ptr += 2;
        if (ptr != end  &&  !isspace(*ptr) ) {
            break;
        }
        env->Skip(2);
        if (verbose) { _TRACE("token: SUB"); }
        return SUB;
    // XOR
    case 'X':
        if ( !s_Compare(ptr, end, "OR", case_sense) ) {
            break;
        }
        ptr += 2;
        if (ptr != end  &&  !isspace(*ptr) ) {
            break;
        }
        env->Skip(2);
        if (verbose) { _TRACE("token: XOR"); }
        return XOR;
    // MINUS        
    case 'M':
        if ( !s_Compare(ptr, end, "INUS", case_sense) ) {
            break;
        }
        ptr += 4;
        if (ptr != end  &&  !isspace(*ptr) ) {
            break;
        }
        env->Skip(4);
        if (verbose) { _TRACE("token: MINUS"); }
        return SUB;
    // OR
    case 'O':
        if ( !s_Compare(ptr, end, "R", case_sense) ) {
            break;
        }
        ptr += 1;
        if (ptr != end  &&  !isspace(*ptr) ) {
            break;
        }
        env->Skip(1);
        if (verbose) { _TRACE("token: OR"); }
        return OR;
    case '|':
        if ( *ptr != '|' ) {
            break;
        }
        /*
        ptr += 1;
        if (ptr != end  &&  !isspace(*ptr) ) {
            break;
        }
        */
        env->Skip(1);
        if (verbose) { _TRACE("token: ||"); }
        return OR;        
    // NOT
    case 'N':
        if ( !s_Compare(ptr, end, "OT", case_sense) ) {
            break;
        }
        ptr += 2;
        if (ptr != end  &&  !isspace(*ptr) ) {
            break;
        }
        env->Skip(2);
        if (verbose) { _TRACE("token: NOT"); }
        return NOT;
    // <, <=, <>
    case '<':
        if ( *ptr == '=' ) {
            if (verbose) { _TRACE("token: <="); }
            env->Skip(1);
            return LE;
        } 
        if ( *ptr == '>' ) {
            if (verbose) { _TRACE("token: <>"); }
            env->Skip(1);
            return NOTEQ;
        } 
        /*
        ptr += 1;
        if (ptr != end  &&  !isspace(*ptr) ) {
            break;
        }
        */
        env->Skip(1);
        if (verbose) { _TRACE("token: <"); }
        return LT;
    // > >= ><
    case '>':
        if ( *ptr == '=' ) {
            if (verbose) { _TRACE("token: >="); }
            env->Skip(1);
            return GE;
        }
        if ( *ptr == '<' ) {
            if (verbose) { _TRACE("token: ><"); }
            env->Skip(1);
            return NOTEQ;
        }
        /*
        ptr += 1;
        if (ptr != end  &&  !isspace(*ptr) ) {
            break;
        }
        */
        env->Skip(1);
        if (verbose) { _TRACE("token: >"); }
        return GT;
    // !=
    case '!':
        if ( *ptr != '=' ) {
            break;
        }
        /*
        ptr += 1;
        if (ptr != end  &&  !isspace(*ptr) ) {
            break;
        }
        */
        env->Skip(1);
        if (verbose) { _TRACE("token: !="); }
        return NOTEQ;
    // = ==
    case '=':
        if ( *ptr != '=' ) {
            env->Skip(1);
            if (verbose) { _TRACE("token: ="); }
            return EQ;
        }
        /*
        ptr += 1;
        if (ptr != end  &&  !isspace(*ptr) ) {
            break;
        }
        */
        env->Skip(1);
        if (verbose) { _TRACE("token: =="); }
        return EQ;
    
    case '(':
        {{
             const char* p = ptr;
             for ( ;  ptr != end  &&  isspace(*ptr);  ++ptr) {
             }
             if (ptr == end) {
                 env->Skip(ptr - p);
                 return EOF;
             }
         }}
        if (verbose) { _TRACE("token: open-paren"); }
        return c;

    case ')':
        if (verbose) { _TRACE("token: close-paren"); }
        return c;

    case 0:
    case EOF:
        if (verbose) { _TRACE("token: EOF"); }
        return EOF;

    default:
        break;
    }

    // the query contains text
    //
    // query is searched up to the next operator (AND, OR, NOT, etc.)
    // so [ Vitamin C AND carrot ] is equivalent of ["Vitamin C" AND carrot]
    // (PubMed query compatibility)
    //

    ptr           = env->GetBufPtr();
    const char* p = ptr;
    if (verbose) _TRACE("parsing text fragment: " << string(ptr, end));
    while (ptr != end) {
        /// skip to the next word boundary
        for ( ;  ptr != end;  ++ptr) {
            if ( isspace(*ptr) ) {
                break;
            }
            if ((*ptr == '(') ||  
                (*ptr == ')') ||
                (*ptr == '|') ||
                (*ptr == '&') ||
                (*ptr == '=') ||
                (*ptr == '<') ||
                (*ptr == '>') ||
                (*ptr == '!')
                ) {
                if (verbose) { 
                    _TRACE("found breaking paren: " << string(ptr, end));
                }
                break;

                /**
                if (ptr == env->begin()  ||
                    isspace(*(ptr - 1))  ||
                    ptr + 1 == end       ||  isspace(*(ptr + 1))) {
                    _TRACE("found breaking paren: " << string(ptr, end));
                    break;
                }
                **/
            }
        }

        token_end = ptr; // not to add trailing spaces to the token
        /// skip intervening white space
        for ( ;  ptr != end;  ++ptr) {
            if ( !isspace(*ptr) ) {
                break;
            }
            if (*ptr == '('  ||  *ptr == ')') {
                if (verbose) 
                    { _TRACE("found breaking paren: " << string(ptr, end)); }
                break;

                /**
                if (ptr == env->begin()  ||  isspace(*(ptr - 1))  ||
                    ptr + 1 == end       ||  isspace(*(ptr + 1))) {
                    _TRACE("found breaking paren: " << string(ptr, end));
                    break;
                }
                **/
            }
        }

        //
        // skip to the next operator A, N, O, etc
        //
        if (s_Compare(ptr, end, "AND",  case_sense) ||
            s_Compare(ptr, end, "OR",   case_sense) ||
            s_Compare(ptr, end, "NOT",  case_sense) ||
            s_Compare(ptr, end, "SUB",  case_sense) ||
            s_Compare(ptr, end, "XOR",  case_sense) ||
            s_Compare(ptr, end, "MINUS",case_sense) ||
            (*ptr == '|') ||
            (*ptr == '&') ||
            (*ptr == '=') ||
            (*ptr == '<') ||
            (*ptr == '>') ||
            (*ptr == '!') 
            ) {
            break;
        }

        if (*ptr == '('  ||  *ptr == ')') {
            break;
        }

    }

    /// create a node for the text
    string text;
    text += c;
    text.append(p, token_end ? token_end : ptr);
    
    CQueryParseTree::TNode* qnode;    
    *lvalp = qnode = CQueryParseTree::CreateNode(text, text, false);
    env->AddNodeToPool(qnode); // to avoid leak when Bison rollbacks the stack

    /// skip and return
    env->Skip(ptr - p);
    if (verbose) { _TRACE("string: >>" << text << "<<"); }
    return STRING;
}




#if 0

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
    static int char_tokens[]      = { '&', '|', '='  };
    static int char_token_codes[] = { AND, OR,  EQ  };

    char c;
    unsigned int  i;
    string token;

    CQueryParserEnv* env = (CQueryParserEnv*) param;

    // Skip white space 
    while (isspace((unsigned char)(c = env->GetChar())));

    // Process single char tokens
    for (i = 0; i < sizeof(char_tokens) / sizeof(char_tokens[0]); ++i) {
        if (c == char_tokens[i]) {
            return char_token_codes[i];
        }
    }

    const char* tok_start = env->GetBufPtr() - 1;

    // Process string tokens in apostrophes
    if (c == '\'') {
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

    // Process operators tokens
    if (c == '<') {
        if (tok_start[1] == '>') { // <>
            env->Skip(2);
            return NOTEQ;
        }
        if (tok_start[1] == '=') { // <=
            env->Skip(2);
            return LE;
        }
        return LT; // <
    }
    if (c == '!') {
        if (tok_start[1] == '=') {
            env->Skip(2);
            return NOTEQ;
        }
    }
    if (c == '>') {
        if (tok_start[1] == '<') { // ><
            env->Skip(2);
            return NOTEQ;
        }
        if (tok_start[1] == '=') { // >=
            env->Skip(2);
            return GE;
        }
        return GT;
    }

    // Process AND token
    if (toupper((unsigned char) c) == 'A') {
        // look ahead if it is AND
        if (toupper((unsigned char) tok_start[1]) == 'N' &&
            toupper((unsigned char) tok_start[2]) == 'D' &&
            isspace((unsigned char) tok_start[3])
           ) {
            env->Skip(3);
            return AND;
        }
    }

    // Process OR token
    if (toupper((unsigned char) c) == 'O') {
        // look ahead if it is OR
        if (toupper((unsigned char) tok_start[1]) == 'R' &&
            isspace((unsigned char) tok_start[2])
           ) {
            env->Skip(2);
            return OR;
        }
    }

    // Process NOT token
    if (toupper((unsigned char) c) == 'N') {
        // look ahead if it is NOT
        if (toupper((unsigned char) tok_start[1]) == 'O' &&
            toupper((unsigned char) tok_start[2]) == 'T' &&
            isspace((unsigned char) tok_start[3])
           ) {
            env->Skip(3);
            return NOT;
        }
    }

    // Process name-like tokens
    if (isalnum((unsigned char) c)) {
        do {
            c = env->GetChar();            
        } while (isalnum((unsigned char) c) || c == '_');

        env->UnGetChar();
        const char* tok_end = env->GetBufPtr();
        token.append(tok_start, tok_end - tok_start);

        *lvalp = LexerMakeTokenNode(env, token);

        // check if token is all digits
        string::size_type pos = 
            token.find_first_not_of("0123456789", 0, token.length());
        return (pos == string::npos) ? NUM : NAME;

    }


    /* return end-of-file  */
    if (c == EOF || c == 0)
        return 0;
    /* return single chars */
    return c;
}

#endif

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2007/01/11 14:49:51  kuznets
 * Many cosmetic fixes and functional development
 *
 * Revision 1.1  2007/01/10 16:14:01  kuznets
 * initial revision
 *
 *
 * ==========================================================================
 */

