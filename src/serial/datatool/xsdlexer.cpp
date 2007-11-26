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
*   XML Schema lexer
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <serial/datatool/xsdlexer.hpp>
#include <serial/datatool/tokens.hpp>

BEGIN_NCBI_SCOPE


XSDLexer::XSDLexer(CNcbiIstream& in, const string& name)
    : DTDLexer(in,name)
{
}

XSDLexer::~XSDLexer(void)
{
}

bool XSDLexer::ProcessDocumentation(void)
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
        case '<':
            if (allblank) {
                RemoveLastComment();
            }
            return false;
        default:
            allblank = allblank && isspace((unsigned char)c);
            comment.AddChar(c);
            SkipChar();
            break;
        }
    }
    return false;
}

TToken XSDLexer::Skip(void)
{
    char c = Char();
    for (;;) {
        c = Char();
        switch (c) {
        case '\0':
            return T_EOF;
        case '\r':
            SkipChar();
            break;
        case '\n':
            SkipChar();
            NextLine();
            break;
        case '>':
            StartToken();
            AddChar();
            return K_CLOSING;
        case '/':
            if (Char(1) == '>') {
                StartToken();
                AddChars(2);
                return K_ENDOFTAG;
            }
            SkipChar();
            break;
        case '<':
            if (Char(1) == '/') {
                StartToken();
                AddChars(2);
                AddElement();
                return K_ENDOFTAG;
            } else {
                SkipChar();
                StartToken();
                for (c = Char(); ; c = Char()) {
                    if (c == '>' || c == '\0') {
                        break;
                    }
                    if (c == '/' && Char(1) == '>') {
                        break;
                    }
                    AddChar();
                }
                return K_ELEMENT;
            }
            break;
        default:
            SkipChar();
            break;
        }
    }
}

TToken XSDLexer::LookupToken(void)
{
    TToken tok = LookupEndOfTag();
    if (tok == K_ENDOFTAG || tok == K_CLOSING) {
        return tok;
    }
    char c = Char();
    if (c == '<') {
        SkipChar();
        if (Char() == '?') {
            SkipChar();
        }
    }
    return LookupLexeme();
}

TToken XSDLexer::LookupLexeme(void)
{
    bool att = false;
    char c = Char();
    if (c == 0) {
        return T_EOF;
    }
    if (!isalpha((unsigned char) c)) {
        LexerError("Name must begin with an alphabetic character (alpha)");
    }
    StartToken();
    for (char c = Char(); c != 0; c = Char()) {
        if (att && (c == '\"' || c == '\'')) {
            AddChar();
            if (strncmp(CurrentTokenStart(),"xmlns",5)==0) {
                return K_XMLNS;
            }
            return K_ATTPAIR;
        } else if (isspace((unsigned char)c) || c == '>' ||
                   (c == '/' && Char(1) == '>')) {
            if (!(att && isspace((unsigned char)c))) {
                break;
            }
        }
        if (c == '=') {
            att = true;
            AddChar();
            c =  Char();
            if (c != '\"' && c != '\'') {
                LexerError("No opening quote in attribute");
            }
        }
        AddChar();
    }
    if (att) {
        LexerError("No closing quote in attribute");
    }
    return LookupKeyword();
}


#define CHECK(keyword, t, length) \
    if ( memcmp(token, keyword, length) == 0 ) return t

TToken XSDLexer::LookupKeyword(void)
{
    const char* token = CurrentTokenStart();
    const char* token_ns = strchr(token, ':');
    if (token_ns && (size_t)(token_ns - token) < CurrentTokenLength()) {
        token = ++token_ns;
    }
    switch ( CurrentTokenEnd() - token ) {
    default:
        break;
    case 3:
        CHECK("xml", K_XML, 3);
        CHECK("any", K_ANY, 3);
        CHECK("all", K_SET, 3);
        break;
    case 4:
        CHECK("list", K_LIST, 4);
        break;
    case 5:
        CHECK("group", K_GROUP, 5);
        CHECK("union", K_UNION, 5);
        break;
    case 6:
        CHECK("choice", K_CHOICE, 6);
        CHECK("schema", K_SCHEMA, 6);
        CHECK("import", K_IMPORT, 6);
        break;
    case 7:
        CHECK("include", K_INCLUDE, 7);
        CHECK("element", K_ELEMENT, 7);
        CHECK("appinfo", K_APPINFO, 7);
        break;
    case 8:
        CHECK("sequence", K_SEQUENCE, 8);
        break;
    case 9:
        CHECK("extension", K_EXTENSION, 9);
        CHECK("attribute", K_ATTRIBUTE, 9);
        break;
    case 10:
        CHECK("simpleType", K_SIMPLETYPE, 10);
        CHECK("annotation", K_ANNOTATION, 10);
        break;
    case 11:
        CHECK("complexType", K_COMPLEXTYPE, 11);
        CHECK("restriction", K_RESTRICTION, 11);
        CHECK("enumeration", K_ENUMERATION, 11);
        break;
    case 13:
        CHECK("simpleContent", K_SIMPLECONTENT, 13);
        CHECK("documentation", K_DOCUMENTATION, 13);
        break;
    case 14:
        CHECK("complexContent", K_COMPLEXCONTENT, 14);
        CHECK("attributeGroup", K_ATTRIBUTEGROUP, 14);
        break;

    }
    return T_IDENTIFIER;
}

TToken XSDLexer::LookupEndOfTag(void)
{
    for (;;) {
        char c = Char();
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
        case '/':
        case '?':
            if (Char(1) != '>') {
                LexerError("expected: />");
            }
            StartToken();
            AddChars(2);
            return K_ENDOFTAG;
        case '<':
            if (Char(1) == '/') {
                StartToken();
                AddChars(2);
                AddElement();
                return K_ENDOFTAG;
            } else {
                return T_SYMBOL;
            }
            break;
        case '>':
            StartToken();
            AddChar();
            return K_CLOSING;
        default:
            return T_SYMBOL;
        }
    }
}

void  XSDLexer::AddElement(void)
{
    char c = Char();
    for (; c != '>' && c != 0; c = Char()) {
        AddChar();
    }
    if (c != 0) {
        AddChar();
    }
    return;
}

END_NCBI_SCOPE
