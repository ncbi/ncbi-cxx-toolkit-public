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
*   WSDL parser
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include "exceptions.hpp"
#include "wsdlparser.hpp"
#include "tokens.hpp"
#include "module.hpp"
#include <serial/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Serial_Parsers

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
// WSDL Parser

WSDLParser::WSDLParser(WSDLLexer& lexer)
    : XSDParser(lexer)
{
}

WSDLParser::~WSDLParser(void)
{
}

void WSDLParser::BuildDocumentTree(CDataTypeModule& module)
{
    ParseHeader();
    CopyComments(module.Comments());

    TToken tok;
    int emb=0;
    for (;;) {
        tok = GetNextToken();
        switch ( tok ) {
        case K_TYPES:
            ParseTypes(module);
            break;
        case K_MESSAGE:
            ParseMessage();
            break;
        case K_PORTTYPE:
            ParsePortType();
            break;
        case K_BINDING:
            ParseBinding();
            break;
        case K_SERVICE:
            ParseService();
            break;
        case K_ENDOFTAG:
        case T_EOF:
            return;
        default:
            ParseError("Invalid keyword", "keyword");
            return;
        }
    }
}

void WSDLParser::ParseHeader(void)
{
// xml header
    TToken tok = GetNextToken();
    if (tok == K_XML) {
        for ( ; tok != K_ENDOFTAG; tok=GetNextToken())
            ;
        tok = GetNextToken();
    } else {
        ERR_POST_X(4, "LINE " << Location() << " XML declaration is missing");
    }
// schema    
    if (tok != K_DEFINITIONS) {
        ParseError("Unexpected token", "definitions");
    }
    for ( tok = GetNextToken(); tok == K_ATTPAIR || tok == K_XMLNS; tok = GetNextToken()) {
        if (tok == K_ATTPAIR) {
            if (IsAttribute("targetNamespace")) {
                m_TargetNamespace = m_Value;
            }
        }
    }
    if (tok != K_CLOSING) {
        ParseError("tag closing");
    }
}

void WSDLParser::ParseTypes(CDataTypeModule& module)
{
    TToken tok = GetRawAttributeSet();
    if (tok == K_CLOSING) {
        BeginScope();
        XSDParser::BuildDocumentTree(module);
        EndScope();
        tok = GetNextToken();
        if (tok != K_ENDOFTAG) {
            ParseError("Unexpected token", "end of tag");
        }
    }
}

void WSDLParser::ParseMessage(void)
{
    TToken tok = GetRawAttributeSet();
    if (tok == K_CLOSING) {
        SkipContent();
    }
}

void WSDLParser::ParsePortType(void)
{
    TToken tok = GetRawAttributeSet();
    if (tok == K_CLOSING) {
        SkipContent();
    }
}

void WSDLParser::ParseBinding(void)
{
    TToken tok = GetRawAttributeSet();
    if (tok == K_CLOSING) {
        SkipContent();
    }
}

void WSDLParser::ParseService(void)
{
    TToken tok = GetRawAttributeSet();
    if (tok == K_CLOSING) {
        SkipContent();
    }
}


END_NCBI_SCOPE
