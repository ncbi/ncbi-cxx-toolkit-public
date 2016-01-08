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

/// @internal
///
static inline 
bool s_IsSpaceTerm(const char *c)
{
    return isspace(*c) || (*c == '\n') || (*c == '\r' && *(c+1) == '\n');
}



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
    } else {
        for ( ;  p != p_end  &&  t  &&  *t;  ++p, ++t) {
            if (*p != *t) {
                return false;
            }
        }
    }
    return !*t  &&  ( !*p  ||  isspace(*p));
}


/// Check if next substring is a function-like expression
/// "IN (..." or "Function(..."
///
/// @internal
///
static 
bool s_LookAheadCmp(const char* p, const char* p_end,
                    const char* t,
                    CQueryParseTree::ECase  case_sense,
                    bool is_func)
{
    if (case_sense == CQueryParseTree::eCaseInsensitive) {
        for ( ;  p != p_end  &&  t  &&  *t;  ++p, ++t) {
            if (toupper(*p) != *t) {
                return false;
            }
        }
    } else {
        for ( ;  p != p_end  &&  t  &&  *t;  ++p, ++t) {
            if (*p != *t) {
                return false;
            }
        }
    }
    if (!*p) return false;
    
    if (is_func) {  // look for opening parenthesis?
        // skip spaces
        for ( ;  p != p_end;  ++p) {
            if ( !s_IsSpaceTerm(p) ) {
                break;
            }    
        }
        if (!*p)       return false;
        if (*p != '(') return false;
    }
    return true;
}

/// Check if next substring is a function-like expression
/// "NOT IN (..." or "Function(..."
///
/// @internal
///
static 
bool s_LookAheadCmp2(const char* p, const char* p_end,
                     const char* t, const char* t2,
                     CQueryParseTree::ECase  case_sense,
                     bool is_func)
{
    if (case_sense == CQueryParseTree::eCaseInsensitive) {
        for ( ;  p != p_end  &&  t  &&  *t;  ++p, ++t) {
            if (toupper(*p) != *t) {
                return false;
            }
        }
    } else {
        for ( ;  p != p_end  &&  t  &&  *t;  ++p, ++t) {
            if (*p != *t) {
                return false;
            }
        }
    }
    if (!*p) return false;
    
    // skip spaces
    for ( ;  p != p_end;  ++p) {
        if ( !s_IsSpaceTerm(p) ) {
            break;
        }    
    }
    if (!*p)  return false;
    
    return s_LookAheadCmp(p, p_end, t2, case_sense, is_func);    
}



/// @internal
///
static inline
bool s_IsBreakingChar(char c)
{
    return (c == '"') ||
           (c == '~') ||
           (c == '[') ||
           (c == '(') ||  
           (c == ')') ||
           (c == '|') ||
           (c == '&') ||
           (c == '=') ||
           (c == '<') ||
           (c == '>') ||
           (c == '!');
}

/// @internal
///
static inline
bool s_IsOperatorChar(char c)
{
    return (c == '"') ||
           (c == '~') ||
           (c == '[') ||
           (c == '(') ||  
           (c == ')') ||
           (c == '=') ||
           (c == '<') ||
           (c == '>') ||
           (c == '!');
}



/// Extract string 
///
/// @internal
///
static
void s_yylex_extract_string(CQueryParserEnv*  env,
                            string&           token,
                            string&           orig_token)
{
    char c;
    for (c=(char)env->GetChar(); c != 0; c=(char)env->GetChar()) {
        orig_token += c;
        if (c == '"') {  // end quote?
            const char* ptr = env->GetBufPtr(); // next to c
            if (*ptr == '"') { // escape symbol?
                c = (char)env->GetChar();
            } else {
                break;
            }
        }
        token += c;
    } // for


    if (c == 0) {  // syntax error EOF
        if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
            NCBI_THROW(CQueryParseException, eParserError, 
                       "Syntax error. String quote mismatch.");
        }
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
int ncbi_q_lex(CQueryParseTree::TNode** lvalp, void* param)
{
    CQueryParserEnv* env = reinterpret_cast<CQueryParserEnv*>(param);
    bool verbose = env->IsVerbose();
    char c, c_orig;
    const char* ptr = env->GetBufPtr();
    const char* end = env->GetBufEnd();
    const char* token_end = 0;
    CQueryParseTree::TNode* qnode = 0;    
    CQueryParseTree::TNode* in_node = env->GetIN_Context();
    CQueryParseTree::TNode* select_node = env->GetSELECT_Context();
    CQueryParseTree::TNode* from_node = env->GetFROM_Context();
    CQueryParseTree::TNode* ctx_node = env->GetContext();
    
    const CQueryParseTree::TFunctionNames* function_names 
                                            = env->GetFunctions();
    
    // comma works as a token breaker?
    bool comma_break = in_node || select_node || ctx_node || from_node;

    // skip leading white space
    //
    for (c = (char)env->GetChar(); ptr < end; ++ptr) {
        if (!s_IsSpaceTerm(ptr)) {
            break;
        }        
        if (*ptr == 0) {
            return EOF;
        }        
        c = (char)env->GetChar();
    }
    ptr = env->GetBufPtr();    
    if (ptr > end) {
        return EOF;
    }

    // save position of token start in the text
    unsigned line     = env->GetLine();
    unsigned line_pos = env->GetLinePos() - 1;

    
    c_orig = c;
    CQueryParseTree::ECase  case_sense = env->GetCase();
    switch (case_sense) {
    case CQueryParseTree::eCaseSensitiveUpper:
        // nothing to do
        break;
    case CQueryParseTree::eCaseInsensitive:
        c = (char)toupper((unsigned char)c);
        break;
    default:
        _ASSERT(0);
    }

    switch (c) {
    // BETWEEN
    case 'B':
        if ( !s_Compare(ptr, end, "ETWEEN", case_sense) ) {
            break;
        }
        ptr += 6;
        if (ptr != end  &&  !s_IsSpaceTerm(ptr) ) {
            break;
        }
        env->Skip(6);
        if (verbose) { _TRACE("token: BETWEEN"); }
        *lvalp = qnode = 
            env->QTree().CreateNode(CQueryParseNode::eBetween, 0, 0,
                                    string(ptr-7, 7));
        env->AttachToPool(qnode); 
        qnode->GetValue().SetLoc(line, line_pos);
        return BETWEEN;
    // IN
    case 'I':
        if ( !s_LookAheadCmp(ptr, end, "N", case_sense, true) ) {
            break;
        }
        ptr += 1;
        env->Skip(1);
        if (verbose) { _TRACE("token: IN"); }
        *lvalp = qnode = 
            env->QTree().CreateNode(CQueryParseNode::eIn, 0, 0,
                                    string(ptr-2, 2));
        env->AttachToPool(qnode); 
        env->SetIN_Context(qnode);
        qnode->GetValue().SetLoc(line, line_pos);
        return IN;
    // SELECT, SUB
    case 'S':
        if ( !s_Compare(ptr, end, "ELECT", case_sense) ) {
            // SUB
            if ( !s_Compare(ptr, end, "UB", case_sense) ) {
                break;
            }
            ptr += 2;
            if (ptr != end  &&  !s_IsSpaceTerm(ptr) ) {
                break;
            }
            env->Skip(2);
            if (verbose) { _TRACE("token: SUB"); }
            *lvalp = qnode = 
                env->QTree().CreateNode(CQueryParseNode::eSub, 0, 0,
                                        string(ptr-3, 3));
            env->AttachToPool(qnode);                
            qnode->GetValue().SetLoc(line, line_pos);
            return SUB;
        }
        ptr += 5;
        if (ptr != end  &&  !s_IsSpaceTerm(ptr) ) {
            break;
        }
        env->Skip(5);
        if (verbose) { _TRACE("token: SELECT"); }
        *lvalp = qnode = 
            env->QTree().CreateNode(CQueryParseNode::eSelect, 0, 0,
                                    string(ptr-6, 6));
                                    
        /* create a pseudo node to isolate select list in a dedicated subtree */
        {{
        CQueryParseTree::TNode* lnode = 0;
        lnode = env->QTree().CreateNode(CQueryParseNode::eList, 0, 0, "LIST");
        qnode->AddNode(lnode);
        env->AttachToPool(qnode); 
        env->SetSELECT_Context(qnode);
        env->SetContext(lnode);
        }}
        qnode->GetValue().SetLoc(line, line_pos);        
        return SELECT;
    // FROM
    case 'F':
        if ( !select_node ) {  // FROM depends on previous SELECT
            break;
        }
        if (!s_Compare(ptr, end, "ROM", case_sense)) {
            break;
        }
        ptr += 3;
        if (ptr != end  &&  !s_IsSpaceTerm(ptr) ) {
            break;
        }
        env->Skip(3);
        if (verbose) { _TRACE("token: FROM"); }
        *lvalp = qnode = 
            env->QTree().CreateNode(CQueryParseNode::eFrom, 0, 0,
                                    string(ptr-4, 4));
        env->AttachToPool(qnode); 
        env->SetFROM_Context(qnode);
        qnode->GetValue().SetLoc(line, line_pos);
        return FROM;
    // WHERE
    case 'W':
        if ( !select_node ) {  // WHERE depends on previous SELECT
            break;
        }
        if (!s_Compare(ptr, end, "HERE", case_sense)) {
            break;
        }
        ptr += 4;
        if (ptr != end  &&  !s_IsSpaceTerm(ptr) ) {
            break;
        }
        env->Skip(4);
        if (verbose) { _TRACE("token: WHERE"); }
        *lvalp = qnode = 
            env->QTree().CreateNode(CQueryParseNode::eWhere, 0, 0,
                                    string(ptr-5, 5));
        env->AttachToPool(qnode); 
        qnode->GetValue().SetLoc(line, line_pos);
        return WHERE;
        
    // LIKE
    case 'L':
        if ( !s_Compare(ptr, end, "IKE", case_sense) ) {
            break;
        }
        ptr += 3;
        if (ptr != end  &&  !s_IsSpaceTerm(ptr) ) {
            break;
        }
        env->Skip(3);
        if (verbose) { _TRACE("token: LIKE"); }
        *lvalp = qnode = 
            env->QTree().CreateNode(CQueryParseNode::eLike, 0, 0,
                                    string(ptr-4, 4));
        env->AttachToPool(qnode); 
        qnode->GetValue().SetLoc(line, line_pos);
        return LIKE;
        
    // AND
    case 'A':
        if ( !s_Compare(ptr, end, "ND", case_sense) ) {
            break;
        }
        ptr += 2;
        // must be "AND somethig" not "ANDsomethig"
        if (ptr != end  &&  !s_IsSpaceTerm(ptr) ) {
            break;
        }
        env->Skip(2);
        if (verbose) { _TRACE("token: AND"); }
        *lvalp = qnode = 
            env->QTree().CreateNode(CQueryParseNode::eAnd, 0, 0,
                                    string(ptr-3, 3));
        env->AttachToPool(qnode); 
        qnode->GetValue().SetLoc(line, line_pos);
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
        *lvalp = qnode = 
            env->QTree().CreateNode(CQueryParseNode::eAnd, 0, 0,
                                    string(ptr-1, 2));
        env->AttachToPool(qnode);                
        qnode->GetValue().SetLoc(line, line_pos);
        return AND;
/*            
    // SUB
    case 'S':
        if ( !s_Compare(ptr, end, "UB", case_sense) ) {
            break;
        }
        ptr += 2;
        if (ptr != end  &&  !s_IsSpaceTerm(ptr) ) {
            break;
        }
        env->Skip(2);
        if (verbose) { _TRACE("token: SUB"); }
        *lvalp = qnode = 
            env->QTree().CreateNode(CQueryParseNode::eSub, 0, 0,
                                    string(ptr-3, 3));
        env->AttachToPool(qnode);                
        qnode->GetValue().SetLoc(line, line_pos);
        return SUB;
*/        
    // XOR
    case 'X':
        if ( !s_Compare(ptr, end, "OR", case_sense) ) {
            break;
        }
        ptr += 2;
        if (ptr != end  &&  !s_IsSpaceTerm(ptr) ) {
            break;
        }
        env->Skip(2);
        if (verbose) { _TRACE("token: XOR"); }
        *lvalp = qnode = 
            env->QTree().CreateNode(CQueryParseNode::eXor, 0, 0,
                                    string(ptr-3, 3));
        env->AttachToPool(qnode);                
        qnode->GetValue().SetLoc(line, line_pos);
        return XOR;
    // MINUS        
    case 'M':
        if ( !s_Compare(ptr, end, "INUS", case_sense) ) {
            break;
        }
        ptr += 4;
        if (ptr != end  &&  !s_IsSpaceTerm(ptr) ) {
            break;
        }
        env->Skip(4);
        if (verbose) { _TRACE("token: MINUS"); }
        *lvalp = qnode = 
            env->QTree().CreateNode(CQueryParseNode::eSub, 0, 0,
                                    string(ptr-5, 5));
        env->AttachToPool(qnode);                        
        qnode->GetValue().SetLoc(line, line_pos);
        return SUB;
    // OR
    case 'O':
        if ( !s_Compare(ptr, end, "R", case_sense) ) {
            break;
        }
        ptr += 1;
        if (ptr != end  &&  !s_IsSpaceTerm(ptr) ) {
            break;
        }
        env->Skip(1);
        if (verbose) { _TRACE("token: OR"); }
        *lvalp = qnode = 
            env->QTree().CreateNode(CQueryParseNode::eOr, 0, 0,
                                    string(ptr-2, 2));
        env->AttachToPool(qnode);                        
        qnode->GetValue().SetLoc(line, line_pos);
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
        *lvalp = qnode = 
            env->QTree().CreateNode(CQueryParseNode::eOr, 0, 0,
                                    string(ptr-1, 2));
        env->AttachToPool(qnode);
        qnode->GetValue().SetLoc(line, line_pos);
        return OR;        
    case '~':
        env->Skip(1);
        if (verbose) { _TRACE("token: ~"); }
        *lvalp = qnode = 
            env->QTree().CreateNode(CQueryParseNode::eLike, 0, 0,
                                    string(ptr-1, 1));
        env->AttachToPool(qnode);
        qnode->GetValue().SetLoc(line, line_pos);
        return LIKE;
    // NOT
    case 'N':
        {
        const char* const not_start = ptr-1;
        if ( !s_Compare(ptr, end, "OT", case_sense) ) {
            break;
        }
        ptr += 2;
        if (ptr != end  &&  !s_IsSpaceTerm(ptr) ) {
            break;
        }
        env->Skip(2);
        
        // check "NOT IN", "NOT BETWEEN", "NOT LIKE" hypothesis
        {{
            const char* const string_start = ptr;
            ptr = env->GetBufPtr();
            // skip spaces
           for ( ;  ptr != end;  ++ptr) {
               if ( !s_IsSpaceTerm(ptr) ) {
                   break;
               } 
            } // for
            
            if (*ptr) {
                if ( s_LookAheadCmp(ptr, end, "IN", case_sense, true) ) {
                    if (verbose) { _TRACE("token: NOT IN"); }            
                    *lvalp = qnode = 
                        env->QTree().CreateNode(CQueryParseNode::eIn, 0, 0,
                                           string(not_start, ptr-not_start+2));
                    env->AttachToPool(qnode);
			        env->SetIN_Context(qnode);                    
                    qnode->GetValue().SetLoc(line, line_pos);
                    qnode->GetValue().SetNot();
                    
                    env->Skip(int(ptr - string_start + 2));
                    return NOT_IN;
                }
                if ( s_Compare(ptr, end, "BETWEEN", case_sense) ) {
                    if (verbose) { _TRACE("token: NOT BETWEEN"); }            
                    *lvalp = qnode = 
                        env->QTree().CreateNode(CQueryParseNode::eBetween, 0, 0,
                                           string(not_start, ptr-not_start+7));
                    env->AttachToPool(qnode);
                    qnode->GetValue().SetLoc(line, line_pos);
                    qnode->GetValue().SetNot();
                    
                    env->Skip(int(ptr - string_start + 7));
                    return NOT_BETWEEN;
                }
                if ( s_Compare(ptr, end, "LIKE", case_sense) ) {
                    if (verbose) { _TRACE("token: NOT LIKE"); }            
                    *lvalp = qnode = 
                        env->QTree().CreateNode(CQueryParseNode::eLike, 0, 0,
                                           string(not_start, ptr-not_start+4));
                    env->AttachToPool(qnode);
                    qnode->GetValue().SetLoc(line, line_pos);
                    qnode->GetValue().SetNot();
                    
                    env->Skip(int(ptr - string_start + 4));
                    return NOT_LIKE;
                }
                
            }
        }}
        
        
        if (verbose) { _TRACE("token: NOT"); }
        
        *lvalp = qnode = 
            env->QTree().CreateNode(CQueryParseNode::eNot, 0, 0,
                                    string(ptr-3, 3));
        env->AttachToPool(qnode);
        qnode->GetValue().SetLoc(line, line_pos);
        return NOT;
        }
    // <, <=, <>
    case '<':
        if ( *ptr == '=' ) {
            if (verbose) { _TRACE("token: <="); }
            env->Skip(1);
            *lvalp = qnode = 
                env->QTree().CreateNode(CQueryParseNode::eLE, 0, 0,
                                        string(ptr-1, 2));
            env->AttachToPool(qnode);   
            qnode->GetValue().SetLoc(line, line_pos);         
            return LE;
        } 
        if ( *ptr == '>' ) {
            if (verbose) { _TRACE("token: <>"); }
            env->Skip(1);
            *lvalp = qnode = 
                env->QTree().CreateNode(CQueryParseNode::eEQ, 0, 0,
                                        string(ptr-1, 1));
            qnode->GetValue().SetNot();
            env->AttachToPool(qnode);            
            qnode->GetValue().SetLoc(line, line_pos);
            return NOTEQ;
        } 
        /*
        ptr += 1;
        if (ptr != end  &&  !isspace(*ptr) ) {
            break;
        }
        env->Skip(1);
        */
        if (verbose) { _TRACE("token: <"); }
        *lvalp = qnode = 
            env->QTree().CreateNode(CQueryParseNode::eLT, 0, 0,
                                    string(ptr-1, 1));
        env->AttachToPool(qnode);                    
        qnode->GetValue().SetLoc(line, line_pos);
        return LT;
    // > >= ><
    case '>':
        if ( *ptr == '=' ) {
            if (verbose) { _TRACE("token: >="); }
            env->Skip(1);
            *lvalp = qnode = 
                env->QTree().CreateNode(CQueryParseNode::eGE, 0, 0,
                                        string(ptr-1, 2));
            qnode->GetValue().SetLoc(line, line_pos);                                              
            env->AttachToPool(qnode);            
            return GE;
        }
        if ( *ptr == '<' ) {
            if (verbose) { _TRACE("token: ><"); }
            env->Skip(1);
            *lvalp = qnode = 
                env->QTree().CreateNode(CQueryParseNode::eEQ, 0, 0,
                                        string(ptr-1, 2));
            qnode->GetValue().SetNot();
            env->AttachToPool(qnode);            
            qnode->GetValue().SetLoc(line, line_pos);
            return NOTEQ;
        }
        /*
        ptr += 1;
        if (ptr != end  &&  !isspace(*ptr) ) {
            break;
        }
        env->Skip(1);
        */
        if (verbose) { _TRACE("token: >"); }
        *lvalp = qnode = 
            env->QTree().CreateNode(CQueryParseNode::eGT, 0, 0,
                                    string(ptr-1, 1));
        env->AttachToPool(qnode);
        qnode->GetValue().SetLoc(line, line_pos);        
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
        *lvalp = qnode = 
            env->QTree().CreateNode(CQueryParseNode::eEQ, 0, 0, 
                                    string(ptr-1, 2));
        qnode->GetValue().SetNot();
        env->AttachToPool(qnode);
        qnode->GetValue().SetLoc(line, line_pos);        
        return NOTEQ;
    // = ==
    case '=':
        if ( *ptr != '=' ) {
            //env->Skip(1);
            if (verbose) { _TRACE("token: ="); }
            *lvalp = qnode = 
                env->QTree().CreateNode(CQueryParseNode::eEQ, 0, 0,
                                        string(ptr-1, 1));
            env->AttachToPool(qnode);
            qnode->GetValue().SetLoc(line, line_pos);
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
        *lvalp = qnode = 
            env->QTree().CreateNode(CQueryParseNode::eEQ, 0, 0,
                                    string(ptr-1, 2));
        env->AttachToPool(qnode);
        qnode->GetValue().SetLoc(line, line_pos);
        return EQ;
    case ':':
        if (verbose) { _TRACE("token: :"); }
        *lvalp = qnode = 
            env->QTree().CreateNode(CQueryParseNode::eRange, 0, 0,
                                    string(ptr-1, 1));
        env->AttachToPool(qnode);
        qnode->GetValue().SetLoc(line, line_pos);
        return RANGE;
    case '(':
        {{
             const char* p = ptr;
             for ( ;  ptr != end  &&  s_IsSpaceTerm(ptr);  ++ptr) {
             }
             if (ptr == end) {
                 env->Skip(int(ptr - p));
                 return EOF;
             }
         }}
        if (verbose) { _TRACE("token: open-paren"); }
        return c;

    case ')':
        if (verbose) { _TRACE("token: close-paren"); }
        return c;
    case ',':
        if (comma_break) {
            return c;
        }
        break;
    case 0:
    case EOF:
        if (verbose) { _TRACE("token: EOF"); }
        return EOF;

    default:
        break;
    }
    
    // Quoted string
    //
    // this implementation takes strings in "  "
    // escaping follows the SQL syntax of double quote: (" 123""4 ")
    if (c == '"') {
        string token, orig_token;
        orig_token += c;
                
        s_yylex_extract_string(env, token, orig_token);
        
        *lvalp = qnode = 
            env->QTree().CreateNode(token, orig_token, false /*not identifier*/);
        env->AddNodeToPool(qnode); 
        qnode->GetValue().SetLoc(line, line_pos);
        return STRING;
    }
    
    // Field search metadata like "[MESH]"
    //
    if (c == '[') {
        string token, orig_token;
        orig_token += c;
        
        for (c=(char)env->GetChar(); c != 0; c=(char)env->GetChar()) {
            orig_token += c;
            if (c == ']') {  // end?
                break;
            }
            token += c;
        } // for
        
        
        if (c == 0) {  // syntax error: EOF
            if (env->GetParserTolerance() == CQueryParseTree::eSyntaxCheck) {
                NCBI_THROW(CQueryParseException, eParserError, 
                           "Syntax error. Identifier brackets mismatch.");
            }
        }
        *lvalp = qnode = 
            env->QTree().CreateNode(token, orig_token, true /*identifier*/);
        env->AddNodeToPool(qnode); 
        qnode->GetValue().SetLoc(line, line_pos);
        return IDENT;
    }
    
    // Evaluate functions
    //
    if (function_names) {
    
        ptr = env->GetBufPtr();
        
        // scan all functions. maybe it is faster to extract a token
        // and compare it to sorted function list 
        // (if there are a LOT of functions)
        // current implementation seems to be big-O suboptimal
        //        
        for (size_t f = 0; f < function_names->size(); ++f) {
            const string& fname = (*function_names)[f];
            if (fname.empty()) {
                continue;
            }            
            const char* fn = fname.c_str();
            if (fn[0] != c) {
                continue;
            }
            ++fn;
            if (s_LookAheadCmp (ptr, end, fn, case_sense, true)) {
                *lvalp = qnode = 
                    env->QTree().CreateFuncNode(fname);
                env->AddNodeToPool(qnode); 
                qnode->GetValue().SetLoc(line, line_pos);
                
                env->Skip(int(fname.length()-1));
                
                env->SetContext(qnode);
                
                return FUNCTION;
            }
        } // for f
    }
    


    // the query contains text
    //
    // query is searched up to the next operator (AND, OR, NOT, etc.)
    // so [ Vitamin C AND carrot ] is equivalent of ["Vitamin C" AND carrot]
    // (PubMed query compatibility)
    //

    // string_start marks the beginning of the text fragment
    // ptr is a moving pointer
    const char* const string_start = ptr = env->GetBufPtr();
    
    // last word pointer (bookmark)
    const char* last_word_ptr = 0;
    const char* last_token_end_ptr = 0;
    
    if (verbose) _TRACE("parsing text fragment: " << string(ptr, end));
    while (ptr != end) {
        // skip to the next word boundary
        for ( ;ptr != end;  ++ptr) {
            if ( s_IsSpaceTerm(ptr) ) {
                break;
            } 
            if (s_IsOperatorChar(*ptr)) {
                if (verbose) { 
                    _TRACE("found breaking paren: " << string(ptr, end));
                }
                break;
            } 
            if ((ptr[0] == '|' && ptr[1] == '|') || 
                (ptr[0] == '&' && ptr[1] == '&')) {
                if (verbose) { 
                    _TRACE("found breaking paren: " << ptr[0] << ptr[1]);
                }
                break;
            }
            if (comma_break && *ptr == ',') {
                if (verbose) { 
                    _TRACE("found breaking paren: " << string(ptr, end));
                }
                break;
            }
        } // for

        token_end = ptr; // not to add trailing spaces to the token
        // skip intervening white space
        for ( ;  ptr != end;  ++ptr) {
            if ( !s_IsSpaceTerm(ptr) ) {
                break;
            } 
            // TODO: we dont need this...? (not space showld break first?
            /*
            if (s_IsOperatorChar(*ptr)) {
                if (verbose) { 
                    _TRACE("found breaking paren: " << ptr[0] << ptr[1]);
                }
                break;
            }          
            if ((ptr[0] == '|' && ptr[1] == '|') || 
                (ptr[0] == '&' && ptr[1] == '&')) {
                if (verbose) { 
                    _TRACE("found breaking paren: " << ptr[0] << ptr[1]);
                }
                break;
            }
            if (comma_break && *ptr == ',') {
                if (verbose) { 
                    _TRACE("found breaking paren: " << string(ptr, end));
                }
                break;
            }
            */            
        } // for

        // skip to the next operator A, N, O, etc
        //
        if (s_Compare(ptr, end, "AND",    case_sense) ||
            s_Compare(ptr, end, "OR",     case_sense) ||
            s_Compare(ptr, end, "SUB",    case_sense) ||
            s_Compare(ptr, end, "XOR",    case_sense) ||
            s_Compare(ptr, end, "MINUS",  case_sense) 
            ) {
            break;
        }
        if ((ptr[0] == '|' && ptr[1] == '|') || 
            (ptr[0] == '&' && ptr[1] == '&')) {
            break;
        }
        
        
        // BETWEEN, LIKE, etc. require parser to step back one word
        // example:
        // "vitamin C journal LIKE medicine"        reads as
        // "vitamin C" AND journal LIKE medicine    but not as
        // "vitamin C journal" LIKE medicine 
        
        if (s_Compare(ptr, end, "BETWEEN",case_sense) ||
            s_Compare(ptr, end, "LIKE",   case_sense) ||
            s_LookAheadCmp (ptr, end, "IN", case_sense, true) ||
            s_LookAheadCmp2(ptr, end, "NOT", "IN", case_sense, true) ||
            s_LookAheadCmp2(ptr, end, "NOT", "BETWEEN", case_sense, false) ||
            s_LookAheadCmp2(ptr, end, "NOT", "LIKE", case_sense, false) ||
            s_LookAheadCmp2(ptr, end, "NOT", "~", case_sense, false) ||
            (*ptr == '~')
           ) {
            if (last_word_ptr) {
                ptr = last_word_ptr; token_end = last_token_end_ptr;
            }
            break;
        }
        if (s_Compare(ptr, end, "NOT",    case_sense)) {
            break;
        }
        
        if (select_node) {
            if (s_Compare(ptr, end, "FROM",case_sense)) {
                break;
            }
        }
        
        if (select_node) {
            if (s_Compare(ptr, end, "WHERE",case_sense)) {
                break;
            }
        }
        
        if (s_IsOperatorChar(*ptr)) {
            break;
        }
        
        if (comma_break && *ptr == ',') {
            break;
        }
        
        // bookmark 
        last_word_ptr = ptr; last_token_end_ptr = token_end;
        
    } // while
        

    // create a node for the text
    //
    
    string token;
    token += c_orig;
    token.append(string_start, token_end ? token_end : ptr);
    
    // detailed type analysis (INT, FLOAT, BOOL ?)
    //
    // check if token is all digits
    //
    string::size_type pos = 
        token.find_first_not_of("0123456789");
    if (pos == string::npos) {  // all digits
        
        Int4 i4 = NStr::StringToInt(token);
        *lvalp = qnode = env->QTree().CreateNode(i4, token);
        env->AddNodeToPool(qnode);

    } else {
    
        *lvalp = qnode = 
            env->QTree().CreateNode(token, token, false /*not identifier*/);
        env->AddNodeToPool(qnode); // to avoid leak when Bison rollbacks the stack
    }
    
    // skip and return
    env->Skip(int(ptr - string_start));
    if (verbose) { _TRACE("string: >>" << token << "<<"); }
    qnode->GetValue().SetLoc(line, line_pos);
    return STRING;
}
