
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

#include <ncbi_pch.hpp>
#include <serial/serial.hpp>
#include <serial/objostr.hpp>
#include <serial/objistr.hpp>
#include <serial/objostrxml.hpp>
#include <serial/soap/soap_message.hpp>
#include <serial/soap/soap_fault.hpp>
#include "soap_envelope.hpp"
#include "soap_body.hpp"
#include "soap_header.hpp"
#include "soap_writehook.hpp"
#include "soap_readhook.hpp"
#include <algorithm>

BEGIN_NCBI_SCOPE

const string& CSoapMessage::ms_SoapNamespace =
//    "http://www.w3.org/2003/05/soap-envelope";
    "http://schemas.xmlsoap.org/soap/envelope/";   // v1.1

CSoapMessage::CSoapMessage(void)
    : m_Prefix("env")
{
    RegisterObjectType(CSoapFault::GetTypeInfo);
}

CSoapMessage::~CSoapMessage(void)
{
}

const string& CSoapMessage::GetSoapNamespace(void)
{
    return ms_SoapNamespace;
}

void CSoapMessage::SetSoapNamespacePrefix(const string& prefix)
{
    m_Prefix = prefix;
}

const string& CSoapMessage::GetSoapNamespacePrefix(void) const
{
    return m_Prefix;
}

void CSoapMessage::SetDefaultObjectNamespaceName(const string& ns_name)
{
    m_DefNamespaceName = ns_name;
}
const string& CSoapMessage::GetDefaultObjectNamespaceName(void) const
{
    return m_DefNamespaceName;
}


void CSoapMessage::AddObject(const CSerialObject& obj,
                             EMessagePart destination)
{
// verify namespace
    if (!m_DefNamespaceName.empty()) {
        CSerialObject* ser = const_cast<CSerialObject*>(&obj);
        CAnyContentObject* any = dynamic_cast<CAnyContentObject*>(ser);
        if (any) {
            if ((any->GetNamespaceName()).empty()) {
                any->SetNamespaceName(m_DefNamespaceName);
            }
        } else {
            if (!ser->HasNamespaceName()) {
                ser->SetNamespaceName(m_DefNamespaceName);
            }
        }
    }

    if (destination == eMsgHeader) {
        m_Header.push_back( CConstRef<CSerialObject>(&obj));
    } else if (destination == eMsgBody) {
        m_Body.push_back( CConstRef<CSerialObject>(&obj));
    } else {
        m_FaultDetail.push_back( CConstRef<CSerialObject>(&obj));
    }
}


void CSoapMessage::Write(CObjectOStream& out) const
{
    CObjectOStreamXml* os = 0;
    bool schema = false, loc = false;
    string ns_default;
    ESerialDataFormat fmt = out.GetDataFormat();
    if (fmt == eSerial_Xml) {
        os = dynamic_cast<CObjectOStreamXml*>(&out);
        if (os) {
            schema = os->GetReferenceSchema();
            os->SetReferenceSchema();
            loc = os->GetUseSchemaLocation();
            os->SetUseSchemaLocation(false);
            ns_default = os->GetDefaultSchemaNamespace();
            os->SetDefaultSchemaNamespace(GetSoapNamespace());
        }
    }

    CSoapEnvelope env;
    env.SetNamespaceName(GetSoapNamespace());
    env.SetNamespacePrefix(GetSoapNamespacePrefix());

    if (!m_Header.empty()) {
// This is to make the stream think the Header was not empty.
// Since Header is optional, we do not have to make it *always*
        CRef<CSoapHeader::C_E> h(new CSoapHeader::C_E);
        h->SetAnyContent(*(new CAnyContentObject));
        env.SetHeader().Set().push_back(h);
    }

// This is to make the stream think the Body was not empty.
// Body is mandatory
    CRef<CSoapBody::C_E> h(new CSoapBody::C_E);
    h->SetAnyContent(*(new CAnyContentObject));
    env.SetBody().Set().push_back(h);

    CSoapFault* flt = 0;
    if (!m_FaultDetail.empty()) {
// This is to make the stream think the Detail was not empty.
// Since Detail is optional, we do not have to make it *always*
        flt = dynamic_cast<CSoapFault*>(const_cast<CSerialObject*>(
            GetSerialObject("Fault", eMsgBody).GetPointer()));
        if (!flt) {
// throw exception here (?)
        }
        CRef<CSoapDetail::C_E> h2(new CSoapDetail::C_E);
        h2->SetAnyContent(*(new CAnyContentObject));
        flt->SetSoapDetail().Set().push_back(h2);
    }

    CObjectTypeInfo typeH = CType<CSoapHeader::C_E>();
    typeH.SetLocalWriteHook(out, new CSoapWriteHook(m_Header));

    CObjectTypeInfo typeB = CType<CSoapBody::C_E>();
    typeB.SetLocalWriteHook(out, new CSoapWriteHook(m_Body));

    CObjectTypeInfo typeF = CType<CSoapDetail::C_E>();
    typeF.SetLocalWriteHook(out, new CSoapWriteHook(m_FaultDetail));

    out << env;

    if (flt) {
        flt->SetSoapDetail().Set().clear();
    }
    if (os) {
        os->SetReferenceSchema(schema);
        os->SetUseSchemaLocation(loc);
        os->SetDefaultSchemaNamespace(ns_default);
    }
}


void CSoapMessage::Read(CObjectIStream& in)
{
    Reset();
    CSoapEnvelope env;

    CObjectTypeInfo typeH = CType<CSoapHeader::C_E>();
    typeH.SetLocalReadHook(in, new CSoapReadHook(m_Header,m_Types));

    CObjectTypeInfo typeB = CType<CSoapBody::C_E>();
    typeB.SetLocalReadHook(in, new CSoapReadHook(m_Body,m_Types));

    CObjectTypeInfo typeF = CType<CSoapDetail::C_E>();
    typeF.SetLocalReadHook(in, new CSoapReadHook(m_FaultDetail,m_Types));

    in >> env;
}


void CSoapMessage::RegisterObjectType(TTypeInfoGetter type_getter)
{
    RegisterObjectType(*type_getter());
}

void CSoapMessage::RegisterObjectType(const CTypeInfo& type)
{
    if (find(m_Types.begin(), m_Types.end(), &type) == m_Types.end()) {
        m_Types.push_back(&type);
    }
}

void CSoapMessage::RegisterObjectType(const CSerialObject& obj)
{
    RegisterObjectType(*obj.GetThisTypeInfo());
}


void CSoapMessage::Reset(void)
{
    m_Header.clear();
    m_Body.clear();
    m_FaultDetail.clear();
}

const CSoapMessage::TSoapContent&
CSoapMessage::GetContent(EMessagePart source) const
{
    if (source == eMsgHeader) {
        return m_Header;
    } else if (source == eMsgBody) {
        return m_Body;
    } else {
        return m_FaultDetail;
    }
}

CConstRef<CSerialObject>
CSoapMessage::GetSerialObject(const string& type_name,
                               EMessagePart source) const
{
    const TSoapContent& src = GetContent(source);
    TSoapContent::const_iterator it;
    for (it= src.begin(); it != src.end(); ++it) {
        if ((*it)->GetThisTypeInfo()->GetName() == type_name) {
            return (*it);
        }
    }
    return CConstRef<CSerialObject>(0);
}

CConstRef<CAnyContentObject>
CSoapMessage::GetAnyContentObject(const string& name,
                                   EMessagePart source) const
{
    const TSoapContent& src = GetContent(source);
    TSoapContent::const_iterator it;
    for (it= src.begin(); it != src.end(); ++it) {
        const CAnyContentObject* obj =
            dynamic_cast<const CAnyContentObject*>(it->GetPointer());
        if (obj && obj->GetName() == name) {
            return CConstRef<CAnyContentObject>(obj);
        }
    }
    return CConstRef<CAnyContentObject>(0);
}


END_NCBI_SCOPE



/* --------------------------------------------------------------------------
* $Log$
* Revision 1.8  2005/02/01 21:47:14  grichenk
* Fixed warnings
*
* Revision 1.7  2004/06/24 20:52:33  gouriano
* Check if default namespace was set before using it
*
* Revision 1.6  2004/06/24 20:39:13  gouriano
* Added default object namespace name
*
* Revision 1.5  2004/06/18 18:17:59  gouriano
* Use SOAP v1.1 namespace for envelope
*
* Revision 1.4  2004/05/17 21:03:24  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.3  2003/12/04 20:56:03  gouriano
* correct parameter names
*
* Revision 1.2  2003/09/25 19:45:33  gouriano
* Added soap Fault object
*
* Revision 1.1  2003/09/22 21:00:04  gouriano
* Initial revision
*
*
* ===========================================================================
*/
