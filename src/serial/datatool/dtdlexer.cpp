
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

#include <serial/datatool/dtdlexer.hpp>
#include <serial/datatool/tokens.hpp>

BEGIN_NCBI_SCOPE


DTDLexer::DTDLexer(CNcbiIstream& in)
    : AbstractLexer(in)
{
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
                // name must start with a letter (alpha)
                _ASSERT(0);
            }
        } else {
             // not allowed in DTD
            _ASSERT(0);
        }
        break;
    case '#':
        tok = LookupIdentifier();
        _ASSERT(tok != T_IDENTIFIER);
        return tok;
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
    for (char c = Char(); ; c = Char()) {

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
    case 3:
        CHECK("ANY", K_ANY,  3);
        break;
    case 5:
        CHECK("EMPTY", K_EMPTY,  5);
        break;
    case 6:
        CHECK("ENTITY", K_ENTITY, 6);
        break;
    case 7:
        CHECK("ELEMENT", K_ELEMENT, 7);
        CHECK("ATTLIST", K_ATTLIST, 7);
        CHECK("#PCDATA", K_PCDATA,  7);
        break;
    }
    return T_IDENTIFIER;
}

END_NCBI_SCOPE


/*
 * ==========================================================================
 * $Log$
 * Revision 1.1  2002/10/15 13:54:01  gouriano
 * DTD lexer and parser, first version
 *
 *
 * ==========================================================================
 */
