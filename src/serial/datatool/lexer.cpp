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
* Author: Eugene Vasilchenko
*
* File Description:
*   ASN.1 lexer
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.18  2004/05/17 21:03:14  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.17  2004/02/25 19:45:20  gouriano
* Made it possible to define DEFAULT for data members of type REAL
*
* Revision 1.16  2003/05/22 20:10:25  gouriano
* added UTF8 strings
*
* Revision 1.15  2003/01/31 03:38:41  lavr
* Heed int->bool performance warnings
*
* Revision 1.14  2002/09/26 16:57:31  vasilche
* Added flag for compatibility with asntool
*
* Revision 1.13  2001/06/11 14:35:02  grichenk
* Added support for numeric tags in ASN.1 specifications and data streams.
*
* Revision 1.12  2001/05/17 15:07:12  lavr
* Typos corrected
*
* Revision 1.11  2000/12/15 15:38:51  vasilche
* Added support of Int8 and long double.
* Added support of BigInt ASN.1 extension - mapped to Int8.
* Enum values now have type Int4 instead of long.
*
* Revision 1.10  2000/11/15 20:34:54  vasilche
* Added user comments to ENUMERATED types.
* Added storing of user comments to ASN.1 module definition.
*
* Revision 1.9  2000/11/14 21:41:25  vasilche
* Added preserving of ASN.1 definition comments.
*
* Revision 1.8  2000/08/25 15:59:22  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.7  2000/04/07 19:26:27  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.6  2000/02/01 21:48:00  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.5  1999/11/15 19:36:16  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <serial/datatool/lexer.hpp>
#include <serial/datatool/tokens.hpp>

BEGIN_NCBI_SCOPE

inline bool IsAlNum(char c)
{
    return isalnum(c) ? true : false;
}

inline bool IsDigit(char c)
{
    return isdigit(c) ? true : false;
}

ASNLexer::ASNLexer(CNcbiIstream& in)
    : AbstractLexer(in)
{
}

ASNLexer::~ASNLexer(void)
{
}

TToken ASNLexer::LookupToken(void)
{
    char c = Char();
    switch ( c ) {
    case ':':
        if ( Char(1) == ':' && Char(2) == '=' ) {
            StartToken();
            AddChars(3);
            return T_DEFINE;
        }
        return T_SYMBOL;
    case '-':
    case '+':
        if ( IsDigit(Char(1)) ) {
            StartToken();
            AddChar();
            return LookupNumber();
        }
        return T_SYMBOL;
    case '\"':
        StartToken();
        AddChar();
        StartString();
        LookupString();
        return T_STRING;
    case '\'':
        StartToken();
        AddChar();
        return LookupBinHexString();
    case '[':
        StartToken();
        AddChar();
        LookupTag();
        return T_TAG;
    default:
        if ( IsDigit(c) ) {
            StartToken();
            AddChar();
            return LookupNumber();
        }
        else if ( c >= 'a' && c <= 'z' ) {
            StartToken();
            AddChar();
            LookupIdentifier();
            return T_IDENTIFIER;
        }
        else if ( c >= 'A' && c <= 'Z' ) {
            StartToken();
            AddChar();
            LookupIdentifier();
            return LookupKeyword();
        }
        return T_SYMBOL;
    }
}

void ASNLexer::LookupComments(void)
{
    while ( true ) {
        char c = Char();
        switch ( c ) {
        case ' ':
        case '\t':
        case '\r':
            SkipChar();
            break;
        case '\n':
            SkipChar();
            NextLine();
            break;
        case '-':
            if ( Char(1) == '-' ) {
                // comments
                SkipChars(2);
                SkipComment();
                break;
            }
            return;
        default:
            return;
        }
    }
}

void ASNLexer::SkipComment(void)
{
    CComment& comment = AddComment();
    while ( true ) {
        // wait for end of comments
        char c = Char();
        switch ( c ) {
        case '\n':
            SkipChar();
            NextLine();
            return;
        case 0:
            if ( Eof() )
                return;
            break;
        case '-':
            if ( Char(1) == '-' ) {
                SkipChars(2);
                return;
            }
            break;
        }
        comment.AddChar(c);
        SkipChar();
    }
}

void ASNLexer::LookupString(void)
{
    while ( true ) {
        char c = Char();
        switch ( c ) {
        case '\r':
        case '\n':
            LexerWarning("unclosed string");
            return;
        case 0:
            if ( Eof() ) {
                LexerWarning("unclosed string");
                return;
            }
            LexerWarning("illegal character in string: \\0");
            AddStringChar(c);
            AddChar();
            break;
        case '\"':
            if ( Char(1) != '\"' ) {
                AddChar();
                return;
            }
            AddChars(2);
            break;
        default:
            if ( c < ' ' && c > '\0' ) {
                LexerWarning("illegal character in string: \\...");
            }
            else {
                AddStringChar(c);
            }
            AddChar();
            break;
        }
    }
}

TToken ASNLexer::LookupBinHexString(void)
{
    TToken token = T_BINARY_STRING;
    while ( true ) {
        char c = Char();
        switch ( c ) {
        case '\r':
        case '\n':
            LexerWarning("unclosed bit string");
            return token;
        case 0:
            if ( Eof() ) {
                LexerWarning("unclosed bit string");
                return token;
            }
            AddChar();
            LexerWarning("illegal character in bit string");
            break;
        case '0':
        case '1':
            AddChar();
            break;
        case '2': case '3': case '4': case '5': case '6': case '7': case '8':
        case '9': case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
            AddChar();
            token = T_HEXADECIMAL_STRING;
            break;
        case '\'':
            switch ( Char(1) ) {
            case 'B':
                AddChars(2);
                if ( token != T_BINARY_STRING )
                    LexerWarning("binary string contains hexadecimal digits");
                return T_BINARY_STRING;
            case 'H':
                AddChars(2);
                return T_HEXADECIMAL_STRING;
            default:
                AddChar();
                LexerWarning("unknown type of bit string");
                return token;
            }
        default:
            AddChar();
            LexerWarning("illegal character in bit string");
            break;
        }
    }
}

void ASNLexer::LookupIdentifier(void)
{
    while ( true ) {
        char c = Char();
        if ( IsAlNum(c) )
            AddChar();
        else if ( c == '-' ) {
            if ( IsAlNum(Char(1)) )
                AddChars(2);
            else {
                if ( AllowIDsEndingWithMinus() )
                    AddChar();
                return;
            }
        }
        else
            return;
    }
}

TToken ASNLexer::LookupNumber(void)
{
    while ( IsDigit(Char()) ) {
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

void ASNLexer::LookupTag(void)
{
    while ( true ) {
        char c = Char();
        switch ( c ) {
        case '\r':
        case '\n':
            LexerWarning("unclosed tag");
            return;
        case 0:
            if ( Eof() ) {
                LexerWarning("unclosed tag");
                return;
            }
            AddChar();
            LexerWarning("illegal character in tag");
            break;
        case ']':
            AddChar();
            return;
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
        // case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
            AddChar();
            break;
        default:
            AddChar();
            LexerWarning("illegal character in tag");
            break;
        }
    }
}

#define CHECK(keyword, t, length) \
    if ( memcmp(token, keyword, length) == 0 ) return t

TToken ASNLexer::LookupKeyword(void)
{
    const char* token = CurrentTokenStart();
    switch ( CurrentTokenLength() ) {
    case 2:
        CHECK("OF", K_OF, 2);
        break;
    case 3:
        CHECK("SET", K_SET, 3);
        CHECK("BIT", K_BIT, 3);
        CHECK("END", K_END, 3);
        break;
    case 4:
        CHECK("TRUE", K_TRUE, 4);
        CHECK("NULL", K_NULL, 4);
        CHECK("REAL", K_REAL, 4);
        CHECK("FROM", K_FROM, 4);
        break;
    case 5:
        CHECK("OCTET", K_OCTET, 5);
        CHECK("BEGIN", K_BEGIN, 5);
        CHECK("FALSE", K_FALSE, 5);
        break;
    case 6:
        CHECK("CHOICE", K_CHOICE, 6);
        CHECK("STRING", K_STRING, 6);
        CHECK("BigInt", K_BIGINT, 6);
        break;
    case 7:
        CHECK("INTEGER", K_INTEGER, 7);
        CHECK("BOOLEAN", K_BOOLEAN, 7);
        CHECK("DEFAULT", K_DEFAULT, 7);
        CHECK("IMPORTS", K_IMPORTS, 7);
        CHECK("EXPORTS", K_EXPORTS, 7);
        break;
    case 8:
        CHECK("SEQUENCE", K_SEQUENCE, 8);
        CHECK("OPTIONAL", K_OPTIONAL, 8);
        break;
    case 10:
        CHECK("ENUMERATED", K_ENUMERATED, 10);
        CHECK("UTF8String", K_UTF8String, 10);
        break;
    case 11:
        CHECK("StringStore", K_StringStore, 11);
        CHECK("DEFINITIONS", K_DEFINITIONS, 11);
        break;
    case 13:
        CHECK("VisibleString", K_VisibleString, 13);
        break;
    }
    return T_TYPE_REFERENCE;
}

void ASNLexer::StartString(void)
{
    _ASSERT(TokenStarted());
    m_StringValue.erase();
}

void ASNLexer::AddStringChar(char c)
{
    _ASSERT(TokenStarted());
    m_StringValue += c;
}

END_NCBI_SCOPE
