#ifndef JSDPARSER_HPP
#define JSDPARSER_HPP

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
*   JSON Schema parser
*
* ===========================================================================
*/

#include <corelib/ncbiutil.hpp>
#include "dtdparser.hpp"
#include "jsdlexer.hpp"

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
// JSDParser

class JSDParser : public DTDParser
{
public:
    JSDParser( JSDLexer& lexer);
    virtual ~JSDParser(void);

protected:
    virtual void BeginDocumentTree(void) override;
    virtual void BuildDocumentTree(CDataTypeModule& module) override;

    void ParseRoot(void);
    void ParseObjectContent(DTDElement* owner);
    void ParseMemberDefinition(DTDElement* owner);
    void ParseArrayContent(DTDElement& node);
    void ParseNode(DTDElement& node);
    void AdjustMinOccurence(DTDElement& node, int occ);
    void ParseRequired(DTDElement& node);
    void ParseDependencies(DTDElement& node);
    void ParseEnumeration(DTDElement& node);
    void ParseOneOf(DTDElement& node);
    void ParseAnyOf(DTDElement& node);
    void ParseAllOf(DTDElement& node);

    void SkipUnknown(TToken tokend);

    const string& Value(void);

    TToken GetNextToken(void);

    string m_StringValue;
    list<string> m_URI;
};

END_NCBI_SCOPE

#endif // JSDPARSER_HPP
