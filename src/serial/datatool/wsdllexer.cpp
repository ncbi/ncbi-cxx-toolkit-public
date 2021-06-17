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
*   WSDL lexer
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include "wsdllexer.hpp"
#include "tokens.hpp"

BEGIN_NCBI_SCOPE


WSDLLexer::WSDLLexer(CNcbiIstream& in, const string& name)
    : XSDLexer(in,name), m_UseXsd(false)
{
}

WSDLLexer::~WSDLLexer(void)
{
}

#define CHECK(keyword, t, length) \
    if ( memcmp(token, keyword, length) == 0 ) return t

TToken WSDLLexer::LookupKeyword(void)
{
    if (m_UseXsd)
    {
        return XSDLexer::LookupKeyword();
    }
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
        break;
    case 4:
        CHECK("part", K_PART, 4);
        CHECK("port", K_PORT, 4);
        CHECK("body", K_BODY, 4);
        break;
    case 5:
        CHECK("input", K_INPUT, 5);
        CHECK("types", K_TYPES, 5);
        break;
    case 6:
        CHECK("schema", K_SCHEMA, 6);
        CHECK("output", K_OUTPUT, 6);
        CHECK("header", K_HEADER, 6);
        break;
    case 7:
        CHECK("message", K_MESSAGE, 7);
        CHECK("binding", K_BINDING, 7);
        CHECK("service", K_SERVICE, 7);
        CHECK("address", K_ADDRESS, 7);
        break;
    case 8:
        CHECK("portType", K_PORTTYPE, 8);
        break;
    case 9:
        CHECK("operation", K_OPERATION, 9);
        break;
    case 11:
        CHECK("definitions", K_DEFINITIONS, 11);
        break;
    case 13:
        CHECK("documentation", K_DOCUMENTATION, 13);
    }
    return T_IDENTIFIER;
}

/////////////////////////////////////////////////////////////////////////////
// WSDLEntityLexer

WSDLEntityLexer::WSDLEntityLexer(CNcbiIstream& in, const string& name, bool autoDelete)
    : WSDLLexer(in,name)
{
    m_Str = &in;
    m_AutoDelete = autoDelete;
}
WSDLEntityLexer::~WSDLEntityLexer(void)
{
    if (m_AutoDelete) {
        delete m_Str;
    }
}

END_NCBI_SCOPE
