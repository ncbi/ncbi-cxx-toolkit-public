#ifndef SOAP_MESSAGE__HPP
#define SOAP_MESSAGE__HPP

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
*   Hold the content of, send and receive SOAP messages
*/

#include <serial/serialbase.hpp>


BEGIN_NCBI_SCOPE

class CSoapMessage : public CObject
{
public:
    CSoapMessage(void);
    ~CSoapMessage(void);

    typedef vector< CConstRef<CSerialObject> > TSoapContent;

    enum EMessagePart {
        eMsgHeader,
        eMsgBody
    };

// attributes
    static const string& GetSoapNamespace(void);
    void SetSoapNamespacePrefix(const string& prefix);
    const string& GetSoapNamespacePrefix(void) const;

// writing
    void AddObject(const CSerialObject& obj, EMessagePart eDestination);
    void Write(CObjectOStream& out) const;

// reading
    void Read(CObjectIStream& in);

// data access
    void RegisterObjectType(TTypeInfoGetter typeGetter);
    void RegisterObjectType(const CTypeInfo& type);
    void RegisterObjectType(const CSerialObject& obj);

    void Reset(void);

    const TSoapContent& GetContent(EMessagePart eSource) const;
    CConstRef<CSerialObject> GetSerialObject(const string& typeName,
                                             EMessagePart eSource) const;
    CConstRef<CAnyContentObject> GetAnyContentObject(const string& name,
                                                     EMessagePart eSource) const;

private:
    string m_Prefix;
    TSoapContent m_Header;
    TSoapContent m_Body;
    vector< const CTypeInfo* >  m_Types;

    static const string& ms_SoapNamespace;
};

inline
CObjectOStream& operator<< (CObjectOStream& out, const CSoapMessage& object)
{
    object.Write(out);
    return out;
}

inline                     
CObjectIStream& operator>> (CObjectIStream& in, CSoapMessage& object)
{
    object.Read(in);
    return in;
}

END_NCBI_SCOPE

#endif  /* SOAP_MESSAGE__HPP */



/* --------------------------------------------------------------------------
* $Log$
* Revision 1.1  2003/09/22 20:58:20  gouriano
* Initial revision
*
*
* ===========================================================================
*/
