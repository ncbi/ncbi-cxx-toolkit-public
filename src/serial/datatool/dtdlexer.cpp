
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


DTDLexer::DTDLexer(CNcbiIstream& in)
    : AbstractLexer(in)
{
    m_CharsToSkip = 0;
}

DTDLexer::~DTDLexer(void)
{
}

TToken DTDLexer::LookupToken(void)
{
    TToken tok;
    char c = Char();
    switch (c) {
    case '<':
        if (Char(1)=='!') {
            SkipChars(2);
            if (isalpha(Char())) {
                return LookupIdentifier();
            } else {
                LexerError("name must start with a letter (alpha)");
//                _ASSERT(0);
            }
        } else {
             // not allowed in DTD
             LexerError("Incorrect format");
//             _ASSERT(0);
        }
        break;
    case '#':
        tok = LookupIdentifier();
        if (tok == T_IDENTIFIER) {
            LexerError("Unknown keyword");
        }
//        _ASSERT(tok != T_IDENTIFIER);
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
    default:
        if (isalpha(c)) {
            tok = LookupIdentifier();
            return tok;
        }
        break;
    }
    return T_SYMBOL;
}

//  find all comments and insert them into Lexer
void DTDLexer::LookupComments(void)
{
    EndPrevToken();
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
    for (;;) {
        char c = Char();
        switch ( c ) {
        case '\r':
            SkipChar();
            break;
        case '\n':
            SkipChar();
            NextLine();
            return true; // comment not ended - there is more
        case 0:
            if ( Eof() )
                return false;
            break;
        case '-':
            if ((Char(1) == '-') && (Char(2) == '>')) {
                // end of the comment
                SkipChars(3);
                return false;
            }
            // no break here
        default:
            comment.AddChar(c);
            SkipChar();
            break;
        }
    }
    return false;
}

TToken DTDLexer::LookupIdentifier(void)
{
    StartToken();
// something (not comment) started
// find where it ends
    for (char c = Char(); c != 0; c = Char()) {

// complete specification is here:
// http://www.w3.org/TR/2000/REC-xml-20001006#sec-common-syn
        if (isalnum(c) || strchr("#._-:", c)) {
            AddChar();
        } else {
            break;
        }
    }
    return LookupKeyword();
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
        break;
    case 7:
        CHECK("ELEMENT", K_ELEMENT, 7);
        CHECK("ATTLIST", K_ATTLIST, 7);
        CHECK("#PCDATA", K_PCDATA,  7);
        CHECK("NMTOKEN", K_NMTOKEN, 7);
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
//    _ASSERT(c == '%');
    if (isspace(Char(1))) {
        return T_SYMBOL;
    } else if (isalpha(Char(1))) {
        SkipChar();
        StartToken();
        for (c = Char(); c != ';'; c = Char()) {
            AddChar();
        }
        m_CharsToSkip = 1;
    } else {
        LexerError("Unexpected symbol");
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
//    _ASSERT(c0 == '\"' || c0 == '\'');
    SkipChar();
    StartToken();
    m_CharsToSkip = 1;
    for (char c = Char(); c != c0; c = Char()) {
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


/*
 * ==========================================================================
 * $Log$
 * Revision 1.7  2004/05/17 21:03:14  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.6  2004/01/16 19:56:52  gouriano
 * Minor correction when parsing end-of-line
 *
 * Revision 1.5  2002/12/17 16:24:43  gouriano
 * replaced _ASSERTs by throwing an exception
 *
 * Revision 1.4  2002/11/14 21:05:27  gouriano
 * added support of XML attribute lists
 *
 * Revision 1.3  2002/10/21 16:09:46  gouriano
 * added more DTD tokens
 *
 * Revision 1.2  2002/10/18 14:38:56  gouriano
 * added parsing of internal parsed entities
 *
 * Revision 1.1  2002/10/15 13:54:01  gouriano
 * DTD lexer and parser, first version
 *
 *
 * ==========================================================================
 */
