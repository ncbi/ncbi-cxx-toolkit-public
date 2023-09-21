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
 * Authors: Dmitrii Saprykin
 *
 * File Description: MyNCBIAccount request factory
 *
 */

#include <ncbi_pch.hpp>

#include <misc/xmlwrapp/xmlwrapp.hpp>

#include <objtools/pubseq_gateway/impl/myncbi/myncbi_request.hpp>

BEGIN_NCBI_SCOPE

void IPSG_MyNCBIRequest::ReceiveHttpResponseData(string data)
{
    m_HttpResponseData.append(data);
}

string IPSG_MyNCBIRequest::GetHttpResponseData() const
{
    return m_HttpResponseData;
}

void IPSG_MyNCBIRequest::SetHttpResponseStatus(long status)
{
    m_HttpResponseStatus = status;
}

long IPSG_MyNCBIRequest::GetHttpResponseStatus() const
{
    return m_HttpResponseStatus;
}

void IPSG_MyNCBIRequest::SetResponseStatus(EPSG_MyNCBIResponseStatus status)
{
    m_ResponseStatus = status;
}

EPSG_MyNCBIResponseStatus IPSG_MyNCBIRequest::GetResponseStatus() const
{
    return m_ResponseStatus;
}

void IPSG_MyNCBIRequest::HandleResponse()
{
    if (m_HttpResponseStatus != 200)
    {
        m_ResponseStatus = EPSG_MyNCBIResponseStatus::eError;
        m_ErrorCallback(CRequestStatus::e500_InternalServerError, m_HttpResponseStatus, eDiag_Error,
             "Request failed: MyNCBI response status - " + to_string(m_HttpResponseStatus)
             + "; MyNCBI response text - '" + GetHttpResponseData() + "'");
    }
}

void CPSG_MyNCBIRequest_WhoAmI::HandleResponse()
{
    IPSG_MyNCBIRequest::HandleResponse();
    if (GetResponseStatus() != EPSG_MyNCBIResponseStatus::eError) {
        string response = GetHttpResponseData();
        auto xml = xml::document(response.c_str(), response.size(), nullptr);
        for (auto node = xml.begin(); node != xml.end(); ++node) {
            if (strcmp(node->get_name(), "MyNcbiResponse") == 0) {
                for (auto data_node = node->begin(); data_node != node->end(); ++data_node) {
                    if (strcmp(data_node->get_name(), "UserId") == 0) {
                        m_UserInfo.user_id = NStr::StringToNumeric<Int8>(data_node->get_content(), NStr::fConvErr_NoThrow);
                    }
                    else if (strcmp(data_node->get_name(), "UserName") == 0) {
                        m_UserInfo.username = data_node->get_content();
                    }
                    else if (strcmp(data_node->get_name(), "EmailAddress") == 0) {
                        m_UserInfo.email_address = data_node->get_content();
                    }
                }
            }
        }
        if (m_UserInfo.user_id > 0 && !m_UserInfo.username.empty() && !m_UserInfo.email_address.empty()) {
            SetResponseStatus(EPSG_MyNCBIResponseStatus::eSuccess);
            m_Consume(m_UserInfo);
        }
        else {
            SetResponseStatus(EPSG_MyNCBIResponseStatus::eError);
            m_ErrorCallback(CRequestStatus::e404_NotFound, GetHttpResponseStatus(), eDiag_Error,
                "MyNCBIUser data not found: MyNCBI response status - " + to_string(GetHttpResponseStatus())
                + "; MyNCBI response text - '" + GetHttpResponseData() + "'");
        }
    }
}

void CPSG_MyNCBIRequest_SignIn::HandleResponse()
{
    IPSG_MyNCBIRequest::HandleResponse();
    if (GetResponseStatus() != EPSG_MyNCBIResponseStatus::eError) {
        string response = GetHttpResponseData();
        auto xml = xml::document(response.c_str(), response.size(), nullptr);
        xml::xpath_expression expr("/myncbi-account-response/result/wcu");
        const xml::node_set nset( xml.get_root_node().run_xpath_query(expr) );
        xml::node_set::const_iterator  k(nset.begin());
        for (auto item = nset.begin() ; item != nset.end(); ++item ) {
            if (m_Cookie.empty()) {
                m_Cookie = item->get_content();
            }
        }

        if (!m_Cookie.empty()) {
            SetResponseStatus(EPSG_MyNCBIResponseStatus::eSuccess);
        }
        else {
            SetResponseStatus(EPSG_MyNCBIResponseStatus::eError);
            m_ErrorCallback(CRequestStatus::e404_NotFound, GetHttpResponseStatus(), eDiag_Error,
                "MyNCBIUser data not found: MyNCBI response status - " + to_string(GetHttpResponseStatus())
                + "; MyNCBI response text - '" + GetHttpResponseData() + "'");
        }
    }
}

END_NCBI_SCOPE
