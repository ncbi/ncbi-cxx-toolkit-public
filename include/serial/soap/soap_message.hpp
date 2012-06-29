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
#include <serial/soap/soap_11__.hpp>

BEGIN_NCBI_SCOPE

class CSoapMessage : public CObject
{
public:
    CSoapMessage(void);
    CSoapMessage(const string& namespace_name);
    ~CSoapMessage(void);

    typedef vector< CConstRef<CSerialObject> > TSoapContent;

    enum EMessagePart {
        eMsgHeader,
        eMsgBody,
        eFaultDetail
    };

// attributes
    static const string GetSoapNamespace(void);
    void SetSoapNamespacePrefix(const string& prefix);
    const string& GetSoapNamespacePrefix(void) const;

    void SetDefaultObjectNamespaceName(const string& ns_name);
    const string& GetDefaultObjectNamespaceName(void) const;

// writing
    void AddObject(const CSerialObject& obj, EMessagePart destination);
    void Write(CObjectOStream& out) const;

// reading
    void RegisterObjectType(TTypeInfoGetter type_getter);
    void RegisterObjectType(const CTypeInfo& type);
    void RegisterObjectType(const CSerialObject& obj);

    void Read(CObjectIStream& in);

// data access
    void Reset(void);

    const TSoapContent& GetContent(EMessagePart source) const;
    CConstRef<CSerialObject> GetSerialObject(const string& type_name,
                                             EMessagePart source) const;
    CConstRef<CAnyContentObject> GetAnyContentObject(const string& name,
                                                     EMessagePart source) const;


    CSoapFault::ESoap_FaultcodeEnum GetFaultCode(void) const
    {
        return m_Fault;
    }

private:
    void x_Check(const CSoapEnvelope& env);
    void x_VerifyFaultObj(bool add_prefix) const;

    CSoapFault::ESoap_FaultcodeEnum m_Fault;
    string m_Prefix;
    string m_DefNamespaceName;
    TSoapContent m_Header;
    TSoapContent m_Body;
    TSoapContent m_FaultDetail;
    vector< const CTypeInfo* >  m_Types;

    static const char* ms_SoapNamespace;
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

template <typename TObj>
CConstRef<TObj> SOAP_GetKnownObject(const CSoapMessage& msg,
    CSoapMessage::EMessagePart source = CSoapMessage::eMsgBody)
{
    CConstRef<CSerialObject> oo = msg.GetSerialObject(
        TObj::GetTypeInfo()->GetName(),source);
    return CConstRef<TObj>( dynamic_cast<const TObj*>(oo.GetPointer()));
}

END_NCBI_SCOPE

#endif  /* SOAP_MESSAGE__HPP */
