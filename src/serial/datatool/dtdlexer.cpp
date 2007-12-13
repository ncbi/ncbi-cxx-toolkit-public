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
*   DTD lexer
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <serial/datatool/dtdlexer.hpp>
#include <serial/datatool/tokens.hpp>

BEGIN_NCBI_SCOPE


DTDLexer::DTDLexer(CNcbiIstream& in, const string& name)
    : AbstractLexer(in,name)
{
    m_CharsToSkip = 0;
    m_IdentifierEnd = false;
}

DTDLexer::~DTDLexer(void)
{
}

TToken DTDLexer::LookupToken(void)
{
    if (m_IdentifierEnd) {
        m_IdentifierEnd = false;
        return T_IDENTIFIER_END;
    }
    TToken tok;
    char c = Char();
    switch (c) {
    case '<':
        if (Char(1)=='!') {
            SkipChars(2);
            if (isalpha((unsigned char) Char())) {
                return LookupIdentifier();
            } else if (Char() == '[') {
// Conditional section
// http://www.w3.org/TR/2004/REC-xml-20040204/#sec-condition-sect
                SkipChar();
                return T_CONDITIONAL_BEGIN;
            } else {
                LexerError("name must start with a letter (alpha)");
            }
        } else {
             // not allowed in DTD
             LexerError("Incorrect format");
        }
        break;
    case '#':
        tok = LookupIdentifier();
        if (tok == T_IDENTIFIER) {
            LexerError("Unknown keyword");
        }
        return tok;
    case '%':
        tok = LookupEntity();
        return tok;
    case '\"':
    case '\'':
        if (!EndPrevToken()) {
            tok = LookupString();
            return tok;
        }
        break;
    case ']':
        if (Char(1) == ']' && Char(2) == '>') {
            SkipChars(3);
            return T_CONDITIONAL_END;
        }
        break;
    default:
        if (isalpha((unsigned char) c)) {
            tok = LookupIdentifier();
            return tok;
        }
        if (isdigit((unsigned char) c)) {
            LookupIdentifier();
            return T_NMTOKEN;
        }
        break;
    }
    return T_SYMBOL;
}

//  find all comments and insert them into Lexer
void DTDLexer::LookupComments(void)
{
    EndPrevToken();
    if (m_IdentifierEnd) {
        return;
    }
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
            EndCommentBlock();
            SkipChar();
            NextLine();
            break;
        case '<':
            if ((Char(1) == '!') && (Char(2) == '-') && (Char(3) == '-')) {
                // comment started
                SkipChars(4);
                while (ProcessComment())
                    ;
                break;
            }
            return; // if it is not comment, it is token
        default:
            return;
        }
    }
}

bool DTDLexer::ProcessComment(void)
{
    CComment& comment = AddComment();
    bool allblank = true;
    for (;;) {
        char c = Char();
        switch ( c ) {
        case '\r':
            SkipChar();
            break;
        case '\n':
            SkipChar();
            NextLine();
            if (allblank) {
                RemoveLastComment();
            }
            return true; // comment not ended - there is more
        case 0:
            if ( Eof() )
                return false;
            break;
        case '-':
            if ((Char(1) == '-') && (Char(2) == '>')) {
                // end of the comment
                SkipChars(3);
                if (allblank) {
                    RemoveLastComment();
                }
                return false;
            }
            // no break here
        default:
            allblank = allblank && isspace((unsigned char)c);
            comment.AddChar(c);
            SkipChar();
            break;
        }
    }
    return false;
}

bool DTDLexer::IsIdentifierSymbol(char c)
{
// complete specification is here:
// http://www.w3.org/TR/2000/REC-xml-20001006#sec-common-syn
    return isalnum((unsigned char) c) || strchr("#._-:", c);
}

TToken DTDLexer::LookupIdentifier(void)
{
    StartToken();
// something (not comment) started
// find where it ends
    char c;
    for (c = Char(); c != 0; c = Char()) {
        if (IsIdentifierSymbol(c)) {
            AddChar();
        } else {
            break;
        }
    }
    TToken tok = LookupKeyword();
    if (c != 0 && tok == T_IDENTIFIER) {
        m_IdentifierEnd = (c != '%');
    }
    return tok;
}

#define CHECK(keyword, t, length) \
    if ( memcmp(token, keyword, length) == 0 ) return t

TToken DTDLexer::LookupKeyword(void)
{
    const char* token = CurrentTokenStart();
// check identifier against known keywords
    switch ( CurrentTokenLength() ) {
    default:
        break;
    case 2:
        CHECK("ID",K_ID,2);
        break;
    case 3:
        CHECK("ANY", K_ANY,  3);
        break;
    case 5:
        CHECK("EMPTY", K_EMPTY,  5);
        CHECK("CDATA", K_CDATA,  5);
        CHECK("IDREF", K_IDREF,  5);
        break;
    case 6:
        CHECK("ENTITY", K_ENTITY, 6);
        CHECK("SYSTEM", K_SYSTEM, 6);
        CHECK("PUBLIC", K_PUBLIC, 6);
        CHECK("IDREFS", K_IDREFS, 6);
        CHECK("#FIXED", K_FIXED,  6);
        CHECK("IGNORE", K_IGNORE, 6);
        break;
    case 7:
        CHECK("ELEMENT", K_ELEMENT, 7);
        CHECK("ATTLIST", K_ATTLIST, 7);
        CHECK("#PCDATA", K_PCDATA,  7);
        CHECK("NMTOKEN", K_NMTOKEN, 7);
        CHECK("INCLUDE", K_INCLUDE, 7);
        break;
    case 8:
        CHECK("NMTOKENS", K_NMTOKENS, 8);
        CHECK("ENTITIES", K_ENTITIES, 8);
        CHECK("NOTATION", K_NOTATION, 8);
        CHECK("#DEFAULT", K_DEFAULT,  8);
        CHECK("#IMPLIED", K_IMPLIED,  8);
        break;
    case 9:
        CHECK("#REQUIRED", K_REQUIRED, 9);
        break;
    }
    return T_IDENTIFIER;
}

TToken DTDLexer::LookupEntity(void)
{
// Entity declaration:
// http://www.w3.org/TR/2000/REC-xml-20001006#sec-entity-decl

    char c = Char();
    if (c != '%') {
        LexerError("Unexpected symbol: %");
    }
    if (isspace((unsigned char) Char(1))) {
        return T_SYMBOL;
    } else if (isalpha((unsigned char) Char(1))) {
        SkipChar();
        StartToken();
        for (c = Char(); c != ';' && c != 0; c = Char()) {
            AddChar();
        }
        m_CharsToSkip = 1;
    } else {
        LexerError("Unexpected symbol");
    }
    if (c != 0) {
        c = Char(m_CharsToSkip);
        m_IdentifierEnd = !(IsIdentifierSymbol(c) || c == '%' || c == '[');
    }
    return T_ENTITY;
}

TToken DTDLexer::LookupString(void)
{
// Entity value:
// http://www.w3.org/TR/2000/REC-xml-20001006#NT-EntityValue

    _ASSERT(m_CharsToSkip==0);
    char c0 = Char();
    if(c0 != '\"' && c0 != '\'') {
        LexerError("Unexpected symbol");
    }
    SkipChar();
    StartToken();
    m_CharsToSkip = 1;
    for (char c = Char(); c != c0 && c != 0; c = Char()) {
        if (c == '\n') {
            NextLine();
        }
        AddChar();
    }
    return T_STRING;
}

bool  DTDLexer::EndPrevToken(void)
{
    if (m_CharsToSkip != 0) {
        SkipChars(m_CharsToSkip);
        m_CharsToSkip = 0;
        return true;
    }
    return false;
}


END_NCBI_SCOPE
