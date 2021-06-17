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
* Author: Andrei Gourianov
*
* File Description:
*   JSON Schema lexer
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include "jsdlexer.hpp"
#include "tokens.hpp"

BEGIN_NCBI_SCOPE


JSDLexer::JSDLexer(CNcbiIstream& in, const string& name)
    : DTDLexer(in,name)
{
}

JSDLexer::~JSDLexer(void)
{
}


void JSDLexer::SkipWhitespace(void)
{
   char c;
    for (;;) {
        c = Char();
        switch (c) {
        case ' ':
        case '\t':
        case '\r':
            SkipChar();
            break;
        case '\n':
            SkipChar();
            NextLine();
            break;
        default:
            return;
        }
    }
}

void JSDLexer::LookupComments(void)
{
    SkipWhitespace();
}

TToken JSDLexer::LookupToken(void)
{
    TToken tok  = T_SYMBOL;
    if (m_CharsToSkip != 0) {
        SkipChars(m_CharsToSkip);
        m_CharsToSkip = 0;
        SkipWhitespace();
    }
    SkipWhitespace();
    char c = Char();
    switch (c) {
    case '{':
        tok = K_BEGIN_OBJECT;
        break;
    case '}':
        tok = K_END_OBJECT;
        break;
    case '[':
        tok = K_BEGIN_ARRAY;
        break;
    case ']':
        tok = K_END_ARRAY;
        break;
    case ':':
        tok = K_KEY;
        break;
    case '\"':
        LookupString();
        return T_STRING;
    case '-':
    case '+':
        if ( isdigit((unsigned char)Char(1)) ) {
            StartToken();
            AddChar();
            return LookupNumber();
        }
        return T_SYMBOL;
    default:
        if ( isdigit((unsigned char)c) ) {
            StartToken();
            AddChar();
            return LookupNumber();
        } else if ( isalpha ((unsigned char)c)) {
            StartToken();
            AddChar();
            LookupIdentifier();
            return LookupKeyword();
        }
        return T_SYMBOL;
    }
    StartToken();
    AddChar();
    return tok;
}

TToken JSDLexer::LookupNumber(void)
{
    while ( isdigit((unsigned char)Char()) ) {
        AddChar();
    }
    char c = Char();
    if (c == '.' || c == 'e' || c == 'E' || c == '-' || c == '+') {
        AddChar();
        LookupNumber();
        return T_DOUBLE;
    }
    return T_NUMBER;
}

void JSDLexer::LookupIdentifier(void)
{
    for (;;) {
        char c = Char();
        if ( isalpha((unsigned char)c) ) {
            AddChar();
        } else {
            return;
        }
    }
}

#define CHECK(keyword, t, length) \
    if ( memcmp(token, keyword, length) == 0 ) return t

TToken JSDLexer::LookupKeyword(void)
{
    const char* token = CurrentTokenStart();
    switch ( CurrentTokenLength() ) {
    case 4:
        CHECK("true", K_TRUE, 4);
        CHECK("null", K_NULL, 4);
        break;
    case 5:
        CHECK("false", K_FALSE, 5);
        break;
    }
    return T_TYPE_REFERENCE;
}

END_NCBI_SCOPE
