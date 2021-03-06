#ifndef SERIAL_DATATOOL___RPCGEN__HPP
#define SERIAL_DATATOOL___RPCGEN__HPP

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
* Author:  Aaron Ucko
*
* File Description:
*   ASN.1/XML RPC client generator
*
*/

#include "type.hpp"

BEGIN_NCBI_SCOPE

class CCodeGenerator;

// fake "data type" for RPC clients, necessary to use CFileCode...
class CClientPseudoDataType : public CDataType
{
public:
    // real methods
    CClientPseudoDataType(const CCodeGenerator& generator,
                          const string& section_name,
                          const string& class_name);
    virtual AutoPtr<CTypeStrings> GenerateCode(void) const override;

    // trivial definitions for CDataType's pure virtuals
    // (I *said* this was fake... ;-))
    virtual void PrintASN(CNcbiOstream&, int)  const override { }
    virtual void PrintJSONSchema(CNcbiOstream&, int, list<string>&, bool) const override { }
    virtual void PrintXMLSchema(CNcbiOstream&, int, bool) const override { }
    virtual void PrintDTDElement(CNcbiOstream&, bool) const override { }
    virtual bool CheckValue(const CDataValue&) const override { return false; }
    virtual TObjectPtr CreateDefault(const CDataValue&) const override { return 0; }

private:
    const CCodeGenerator& m_Generator; // source of all wisdom
    string                m_SectionName;
    string                m_ClassName; // already extracted anyway...
    string                m_RequestType,         m_ReplyType;
    string                m_RequestElement,      m_ReplyElement;
    const CDataType       *m_RequestDataType,   *m_ReplyDataType;
    const CChoiceDataType *m_RequestChoiceType, *m_ReplyChoiceType;

    friend class CClientPseudoTypeStrings;
};

END_NCBI_SCOPE

#endif  /* SERIAL_DATATOOL___RPCGEN__HPP */
