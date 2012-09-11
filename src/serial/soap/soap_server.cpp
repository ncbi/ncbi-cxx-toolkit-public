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
 * Author:  Andrei Gourianov
 *
 * File Description:
 *   SOAP server application class
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>
#include <serial/soap/soap_server.hpp>

#include <serial/objistrxml.hpp>
#include <serial/objostrxml.hpp>

BEGIN_NCBI_SCOPE

#if defined(SOAPSERVER_INTERNALSTORAGE)

static const size_t s_Step = 16;
CSoapServerApplication::Storage::Storage(void)
{
    m_Capacity=s_Step;
    m_Current = 0;
    m_Buffer = new TWebMethod[m_Capacity];
}
CSoapServerApplication::Storage::Storage(const Storage& src)
{
    m_Capacity = src.m_Capacity;
    m_Current = src.m_Current;
    m_Buffer = new TWebMethod[m_Capacity];
    memcpy( m_Buffer, src.m_Buffer, m_Capacity * sizeof(TWebMethod));
}
CSoapServerApplication::Storage::~Storage(void)
{
    delete [] m_Buffer;
}
CSoapServerApplication::Storage::const_iterator
CSoapServerApplication::Storage::begin(void) const
{
    return m_Buffer;
}
CSoapServerApplication::Storage::const_iterator
CSoapServerApplication::Storage::end(void) const
{
    return m_Buffer+m_Current;
}
void CSoapServerApplication::Storage::push_back(TWebMethod value)
{
    if (m_Current >= m_Capacity) {
        TWebMethod* newbuf = new TWebMethod[m_Capacity+s_Step];
        memcpy( newbuf, m_Buffer, m_Capacity * sizeof(TWebMethod));
        m_Capacity += s_Step;
        delete [] m_Buffer;
        m_Buffer = newbuf;
    }
    *(m_Buffer + m_Current) = value;
    ++m_Current;
}
#endif


CSoapServerApplication::CSoapServerApplication(
    const string& wsdl_filename, const string& namespace_name)
    : CCgiApplication(), m_DefNamespace(namespace_name), m_Wsdl(wsdl_filename),
      m_OmitScopePrefixes(false),
      m_FaultPostFlags(eDPF_Prefix | eDPF_Severity)
{
}

void CSoapServerApplication::SetDefaultNamespaceName(const string& namespace_name)
{
    m_DefNamespace = namespace_name;
}
const string& CSoapServerApplication::GetDefaultNamespaceName(void) const
{
    return m_DefNamespace;
}
void CSoapServerApplication::SetWsdlFilename(const string& wsdl_filename)
{
    m_Wsdl = wsdl_filename;
}


int CSoapServerApplication::ProcessRequest(CCgiContext& ctx)
{
    const CCgiRequest& request  = ctx.GetRequest();
    CCgiResponse&      response = ctx.GetResponse();
    response.SetContentType("text/xml");
    if (!x_ProcessWsdlRequest(response, request)) {
        x_ProcessSoapRequest(response, request);
    }
    response.Flush();
    return 0;
}

bool
CSoapServerApplication::x_ProcessWsdlRequest(CCgiResponse& response,
                                             const CCgiRequest& request) const
{
    const TCgiEntries& entries = request.GetEntries();
    if (entries.empty()) {
        return false;
    }
    for(TCgiEntries::const_iterator i = entries.begin();
        i != entries.end(); ++i) {
        if (NStr::CompareNocase(i->first, "wsdl") == 0) {
            response.WriteHeader();
            if (!m_Wsdl.empty()) {
                int len = (int)CFile(m_Wsdl).GetLength();
                if (len > 0) {
                    char* buf = new char[len];
                    ifstream iw(m_Wsdl.c_str());
                    iw.read(buf,len);
                    response.out().write(buf,len);
                    delete [] buf;
                }
            }
            return true;
        }
    }
    return false;
}

bool
CSoapServerApplication::x_ProcessSoapRequest(CCgiResponse& response,
                                             const CCgiRequest& request)
{
    bool input_ok=true;
    string fault_text;
    CSoapMessage soap_in, soap_out;
    soap_out.SetDefaultObjectNamespaceName(GetDefaultNamespaceName());

    vector< TTypeInfoGetter >::const_iterator types_in;
    for (types_in = m_Types.begin(); types_in != m_Types.end(); ++types_in) {
        soap_in.RegisterObjectType(*types_in);
    }
// read request
    if (request.GetInputStream()) {
        try {
            auto_ptr<CObjectIStream> is(CObjectIStream::Open(eSerial_Xml,*request.GetInputStream()));
            if (m_OmitScopePrefixes) {
                dynamic_cast<CObjectIStreamXml*>(is.get())->SetEnforcedStdXml(true);
            }
            *is >> soap_in;
        }
        catch (CException& e) {
            input_ok = false;
            ERR_POST(e);
            fault_text = e.ReportAll(m_FaultPostFlags);
        }
        catch (exception& e) {
            input_ok = false;
            fault_text = e.what();
        }
    }
#if 1
    else {
        input_ok = false;
        fault_text = "No input stream in CCgiRequest";
    }
#else
// for debugging only!
    else {
        try {
#if 0
            auto_ptr<CObjectIStream> is(CObjectIStream::Open(eSerial_Xml,"-",eSerial_StdWhenDash));
#else
            auto_ptr<CObjectIStream> is(CObjectIStream::Open(eSerial_Xml,"input.xml"));
#endif
            if (m_OmitScopePrefixes) {
                dynamic_cast<CObjectIStreamXml*>(is.get())->SetEnforcedStdXml(true);
            }
            *is >> soap_in;
        }
        catch (CException& e) {
            input_ok = false;
            ERR_POST(e);
            fault_text = e.ReportAll(m_FaultPostFlags);
        }
        catch (exception& e) {
            input_ok = false;
            fault_text = e.what();
        }
    }
#endif

    if (input_ok) {
        input_ok = false;
        if (soap_in.GetFaultCode() == CSoapFault::eVersionMismatch) {
            x_FaultVersionMismatch(soap_out);
        } else if (soap_in.GetFaultCode() == CSoapFault::eMustUnderstand) {
            x_FaultMustUnderstand(soap_out);
        } else {

            const TListeners* listeners = x_FindListeners(soap_in);
// process request
            if (listeners) {
                input_ok = true;
                TListeners::const_iterator it;
                for (it = listeners->begin(); it != listeners->end(); ++it) {
                    const TWebMethod listener = *it;
                    if (!(this->*listener)(soap_out, soap_in)) {
                        break;
                    }
                }
            } else {
                x_FaultNoListeners(soap_out);
            }
        }
    } else {
        x_FaultServer(soap_out, fault_text);
    }

// send it back
    if (!input_ok) {
        // http://www.w3.org/TR/2000/NOTE-SOAP-20000508/#_Toc478383529
        response.SetStatus(500);
    }
    response.WriteHeader();
    {
        auto_ptr<CObjectOStream> out(CObjectOStream::Open(eSerial_Xml,response.out()));
        if (m_OmitScopePrefixes) {
            dynamic_cast<CObjectOStreamXml*>(out.get())->SetEnforcedStdXml(true);
        }
        *out << soap_out;
    }	
    return true;
}

const CSoapServerApplication::TListeners*
CSoapServerApplication::x_FindListeners(const CSoapMessage& request)
{
    const CSoapMessage::TSoapContent& content =
        request.GetContent(CSoapMessage::eMsgBody);
    CSoapMessage::TSoapContent::const_iterator i;
    for (i = content.begin(); i != content.end(); ++i) {
        string name = (*i)->GetThisTypeInfo()->GetName();
        string ns_name = (*i)->GetNamespaceName();
        const TListeners* listeners = x_FindListenersByName(name,ns_name);
        if (listeners) {
            return listeners;
        }
    }
    for (i = content.begin(); i != content.end(); ++i) {
        const CAnyContentObject* obj =
            dynamic_cast<const CAnyContentObject*>(i->GetPointer());
        if (obj) {
            string name = obj->GetName();
            string ns_name = obj->GetNamespaceName();
            const TListeners* listeners = x_FindListenersByName(name,ns_name);
            if (listeners) {
                return listeners;
            }
        }
    }
    return 0;
}

CSoapServerApplication::TListeners*
CSoapServerApplication::x_FindListenersByName(const string& message_name,
                                              const string& namespace_name)
{
    multimap<string, pair<string,TListeners > >::iterator l;
    for (l = m_Listeners.find(message_name); l != m_Listeners.end(); ++l) {
        if ((l->second).first == namespace_name) {
            return &((l->second).second);
        }
    }
    return 0;
}

void
CSoapServerApplication::RegisterObjectType(TTypeInfoGetter type_getter)
{
    if (find(m_Types.begin(), m_Types.end(), type_getter) == m_Types.end()) {
        m_Types.push_back(type_getter);
    }
}

void
CSoapServerApplication::AddMessageListener(TWebMethod listener,
                                           const string& message_name,
                                           const string& namespace_name)
{
    string ns(namespace_name);
    if (ns.empty()) {
        ns = m_DefNamespace;
    }
    TListeners* listeners = x_FindListenersByName(message_name, ns);
    if (listeners) {
        listeners->push_back(listener);
    } else {
        TListeners new_listeners;
        new_listeners.push_back(listener);
        m_Listeners.insert(
            pair<string const, pair<string,TListeners> >(message_name,
                make_pair(ns,new_listeners)));
    }
}

void
CSoapServerApplication::SetOmitScopePrefixes(bool bOmit)
{
    m_OmitScopePrefixes = bOmit;
}

void
CSoapServerApplication::x_FaultVersionMismatch(CSoapMessage& response) const
{
    CRef<CSoapFault> fault(new CSoapFault);
    fault->SetFaultcodeEnum(CSoapFault::eVersionMismatch);
    fault->SetFaultstring("Server supports SOAP v1.1 only");
    response.AddObject( *fault, CSoapMessage::eMsgBody);
}

void
CSoapServerApplication::x_FaultMustUnderstand(CSoapMessage& response) const
{
    CRef<CSoapFault> fault(new CSoapFault);
    fault->SetFaultcodeEnum(CSoapFault::eMustUnderstand);
    fault->SetFaultstring("An immediate child element of the SOAP Header not understood");
    response.AddObject( *fault, CSoapMessage::eMsgBody);
}

void
CSoapServerApplication::x_FaultServer(CSoapMessage& response, const string& text) const
{
    CRef<CSoapFault> fault(new CSoapFault);
    fault->SetFaultcodeEnum(CSoapFault::eServer);
    fault->SetFaultstring(text);
    response.AddObject( *fault, CSoapMessage::eMsgBody);
}

void
CSoapServerApplication::x_FaultNoListeners(CSoapMessage& response) const
{
    CRef<CSoapFault> fault(new CSoapFault);
    fault->SetFaultcodeEnum(CSoapFault::eClient);
    fault->SetFaultstring("Unsupported request type");
    response.AddObject( *fault, CSoapMessage::eMsgBody);
}


END_NCBI_SCOPE
