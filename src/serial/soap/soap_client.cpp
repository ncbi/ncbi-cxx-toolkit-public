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
 *   SOAP http client
 *
 */

#include <ncbi_pch.hpp>
#include <serial/soap/soap_client.hpp>
#include <serial/objistrxml.hpp>
#include <serial/objostrxml.hpp>
#include <connect/ncbi_conn_stream.hpp>

#include <algorithm>


BEGIN_NCBI_SCOPE

CSoapHttpClient::CSoapHttpClient(const string& server_url,
                                 const string& namespace_name)
    : m_ServerUrl(server_url), m_DefNamespace(namespace_name),
      m_OmitScopePrefixes(false)
{
}

CSoapHttpClient::~CSoapHttpClient(void)
{
}

void CSoapHttpClient::SetServerUrl(const string& server_url)
{
    m_ServerUrl = server_url;
}
const string& CSoapHttpClient::GetServerUrl(void) const
{
    return m_ServerUrl;
}
void CSoapHttpClient::SetDefaultNamespaceName(const string& namespace_name)
{
    m_DefNamespace = namespace_name;
}
const string& CSoapHttpClient::GetDefaultNamespaceName(void) const
{
    return m_DefNamespace;
}

void CSoapHttpClient::SetUserHeader(const string& user_header)
{
    m_UserHeader = user_header;
}
const string&  CSoapHttpClient::GetUserHeader(void) const
{
    return m_UserHeader;
}

void CSoapHttpClient::RegisterObjectType(TTypeInfoGetter type_getter)
{
    if (find(m_Types.begin(), m_Types.end(), type_getter) == m_Types.end()) {
        m_Types.push_back(type_getter);
    }
}

extern "C" {
static EHTTP_HeaderParse x_ParseHttpHeader(const char* /*http_header*/,
                                           void*       /*user_data*/,
                                           int         /*server_error*/)
{
    return eHTTP_HeaderContinue;
}
}


void CSoapHttpClient::Invoke(CSoapMessage& response,
                             const CSoapMessage& request,
                             CConstRef<CSoapFault>* fault /*=0*/,
                             const string& soap_action /*= kEmptyStr*/) const
{
    response.Reset();
    vector< TTypeInfoGetter >::const_iterator types_in;
    for (types_in = m_Types.begin(); types_in != m_Types.end(); ++types_in) {
        response.RegisterObjectType(*types_in);
    }

    char content_type[MAX_CONTENT_TYPE_LEN + 1];

// SOAPAction:
// http://www.w3.org/TR/2000/NOTE-SOAP-20000508/#_Toc478383528
// "An HTTP client MUST use this header field when issuing a SOAP HTTP Request"

    string header;
    header = "SOAPAction: \"" + soap_action + "\"\r\n";
    if (!m_UserHeader.empty()) {
        header += m_UserHeader;
        header += "\r\n";
    }
    CConn_HttpStream http(m_ServerUrl, 0,
        header + string(MIME_ComposeContentTypeEx(eMIME_T_Text, eMIME_Xml,
        eENCOD_None, content_type, sizeof(content_type) - 1)),
        x_ParseHttpHeader);

    auto_ptr<CObjectOStream> os(CObjectOStream::Open(eSerial_Xml, http));
    auto_ptr<CObjectIStream> is(CObjectIStream::Open(eSerial_Xml, http));

// allow unknown data
    is->SetSkipUnknownMembers(eSerialSkipUnknown_Yes);
// allow missing mandatory data
    is->SetVerifyData(eSerialVerifyData_No);

    if (m_OmitScopePrefixes) {
        dynamic_cast<CObjectOStreamXml*>(os.get())->SetEnforcedStdXml(true);
        dynamic_cast<CObjectIStreamXml*>(is.get())->SetEnforcedStdXml(true);
    }

    *os << request;
    dynamic_cast<CObjectIStreamXml*>(is.get())->FindFileHeader(false);
    *is >> response;
    if (fault) {
        *fault = SOAP_GetKnownObject<CSoapFault>(response);
    }
}

void CSoapHttpClient::SetOmitScopePrefixes(bool bOmit)
{
    m_OmitScopePrefixes = bOmit;
}

END_NCBI_SCOPE
