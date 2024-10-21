#ifndef PSG_MYNCBI__MYNCBI_REQUEST__HPP
#define PSG_MYNCBI__MYNCBI_REQUEST__HPP

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

#include <corelib/ncbistr.hpp>
#include <corelib/request_status.hpp>

#include <functional>

BEGIN_NCBI_SCOPE

enum class EPSG_MyNCBIResponseStatus
{
    eUndefined = 0,
    eSuccess,
    eInProgress,
    eError
};

using TDataErrorCallback = function<void(CRequestStatus::ECode status, int code, EDiagSev severity, const string & message)>;

template <typename TResponse>
struct SPSG_MyNCBISyncResponse
{
private:
    struct SError {
        CRequestStatus::ECode status{};
        int code{0};
        EDiagSev severity{eDiagSevMin};
        string message;
    };
public:
    using TRequestResponse = TResponse;
    using TError = SError;

    EPSG_MyNCBIResponseStatus response_status{EPSG_MyNCBIResponseStatus::eUndefined};
    TRequestResponse response{};
    TError error{};
};

class IPSG_MyNCBIRequest
{
public:
    void SetErrorCallback(TDataErrorCallback fn)
    {
        m_ErrorCallback = std::move(fn);
    }

    void ReceiveHttpResponseData(string data);
    void SetHttpResponseStatus(long status);
    string GetHttpResponseData() const;
    long GetHttpResponseStatus() const;

    void SetResponseStatus(EPSG_MyNCBIResponseStatus status);
    EPSG_MyNCBIResponseStatus GetResponseStatus() const;

    virtual string GetRequestXML() const = 0;
    virtual void HandleResponse();

    virtual ~IPSG_MyNCBIRequest() = default;
protected:
    TDataErrorCallback m_ErrorCallback;
private:
    string m_HttpResponseData;
    long m_HttpResponseStatus{-1};
    EPSG_MyNCBIResponseStatus m_ResponseStatus{EPSG_MyNCBIResponseStatus::eInProgress};
};

class CPSG_MyNCBIRequest_WhoAmI
    : public IPSG_MyNCBIRequest
{
public:
    struct SUserInfo {
        Int8 user_id{0};
        string username;
        string email_address;
    };
    using TResponse = SUserInfo;
    using TConsumeCallback = function<void(TResponse)>;

    explicit CPSG_MyNCBIRequest_WhoAmI(string cookie)
        : m_Cookie(std::move(cookie))
    {}

    void SetConsumeCallback(TConsumeCallback fn)
    {
        m_Consume = std::move(fn);
    }

    string GetRequestXML() const override
    {
        return
        "<MyNcbiRequest version=\"2.0\">"
            "<Command name=\"whoami\"/>"
            "<Parameter>" + NStr::XmlEncode(m_Cookie) + "</Parameter>"
        "</MyNcbiRequest>";
    }

    void HandleResponse() override;

    Int8 GetUserId() const
    {
        return m_UserInfo.user_id;
    }

    string GetUserName() const
    {
        return m_UserInfo.username;
    }

    string GetEmailAddress() const
    {
        return m_UserInfo.email_address;
    }

private:
    string m_Cookie;
    SUserInfo m_UserInfo;
    TConsumeCallback m_Consume;
};

/**
 * @for_testing_only
 */
class CPSG_MyNCBIRequest_SignIn
    : public IPSG_MyNCBIRequest
{
public:
    CPSG_MyNCBIRequest_SignIn(string username, string password)
        : m_Username(std::move(username))
        , m_Password(std::move(password))
    {}

    string GetRequestXML() const override
    {
        return
        "<myncbi-account-request version=\"1.0\">"
            "<command name=\"sign-in\">"
                "<param name=\"username\" value=\"" + NStr::XmlEncode(m_Username) + "\"/>"
                "<param name=\"password\" value=\"" + NStr::XmlEncode(m_Password) + "\"/>"
                "<param name=\"persistent\" value=\"true\"/>"
            "</command>"
        "</myncbi-account-request>";
    }

    void HandleResponse() override;

    string GetCookie() const
    {
        return m_Cookie;
    }

private:
    string m_Username;
    string m_Password;
    string m_Cookie;
};

END_NCBI_SCOPE

#endif  // PSG_MYNCBI__MYNCBI_REQUEST__HPP
