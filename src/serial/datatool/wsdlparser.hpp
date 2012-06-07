#ifndef WSDLPARSER_HPP
#define WSDLPARSER_HPP

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

#include <corelib/ncbiutil.hpp>
#include "xsdparser.hpp"
#include "wsdllexer.hpp"

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
// WSDL Parser

class WSDLParser : public XSDParser
{
public:
    WSDLParser( WSDLLexer& lexer);
    virtual ~WSDLParser(void);

protected:
    virtual void BuildDocumentTree(CDataTypeModule& module);
    virtual void BuildDataTree(AutoPtr<CFileModules>& modules,
                               AutoPtr<CDataTypeModule>& module);

    void ParseHeader(void);

    void ParseTypes(CDataTypeModule& module);

    void ParseContent(DTDElement& node);
    void ParsePortType(DTDElement& node);
    void ParseBinding(DTDElement& node);
    void ParseOperation(DTDElement& node);
    void ParseBody(DTDElement& node);
    void ParseHeader(DTDElement& node);
    void ParseInput(DTDElement& node);
    void ParseOutput(DTDElement& node);
    void ParsePart(DTDElement& node);
    void ParsePort(DTDElement& node);
    void ParseAddress(DTDElement& node);

    string CreateWsdlName(const string& name, DTDElement::EType type);
    string CreateEmbeddedName(DTDElement& node, DTDElement::EType type);
    DTDElement& EmbeddedElement(DTDElement& node, const string& name, DTDElement::EType type);

    void ParseMessage(void);
    void ParseService(void);
    
    virtual AbstractLexer* CreateEntityLexer(
        CNcbiIstream& in, const string& name, bool autoDelete=true);

private:
    void ProcessEndpointTypes(void);
    void CollectDataObjects(void);
    void CollectDataObjects(DTDElement& agent, DTDElement& node);

    bool m_ParsingTypes;
    bool m_ParsingOutput;
};

END_NCBI_SCOPE

#endif // WSDLPARSER_HPP
