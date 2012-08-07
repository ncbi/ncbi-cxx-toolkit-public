#ifndef NCBI_CGI_CTX__HPP
#define NCBI_CGI_CTX__HPP

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
* Author: 
*	Vsevolod Sandomirskiy
*
* File Description:
*   Basic CGI Application class
*/

#include <cgi/cgiapp.hpp>
#include <cgi/cgi_session.hpp>
#include <connect/ncbi_types.h>
#include <cgi/cgi_exception.hpp>


/** @addtogroup CGIBase
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CUrl;

/////////////////////////////////////////////////////////////////////////////
//
//  CCgiServerContext::
//

class NCBI_XCGI_EXPORT CCgiServerContext
{
public:
    virtual ~CCgiServerContext(void);
};



/////////////////////////////////////////////////////////////////////////////
//
//  CCtxMsg::
//

class NCBI_XCGI_EXPORT CCtxMsg
{
public:
    virtual ~CCtxMsg(void);
    virtual CNcbiOstream& Write(CNcbiOstream& os) const = 0;
};


/* @} */


inline
CNcbiOstream& operator<< (CNcbiOstream& os, const CCtxMsg& ctx_msg)
{
    return ctx_msg.Write(os);
}


/** @addtogroup CGIBase
 *
 * @{
 */


/////////////////////////////////////////////////////////////////////////////
//
//  CCtxMsgString::
//

class NCBI_XCGI_EXPORT CCtxMsgString : public CCtxMsg
{
public:
    CCtxMsgString(const string& msg) : m_Message(msg) {}
    virtual ~CCtxMsgString(void);
    virtual CNcbiOstream& Write(CNcbiOstream& os) const;

    static const char* sm_nl;

private:
    string m_Message;
};


/////////////////////////////////////////////////////////////////////////////
//
//  CCgiContext::
//
// CCgiContext is a wrapper for request, response, server context.
// In addition, it contains list of messages (as HTML nodes).
// Having non-const reference, CCgiContext's user has access to all its 
// internal data.
// Context will try to create request from given data or default request
// on any request creation error
//

class CNcbiEnvironment;
class CNcbiRegistry;
class CNcbiResource;
class CCgiApplication;


class NCBI_XCGI_EXPORT CCgiContext
{
public:
    CCgiContext(CCgiApplication&        app,
                const CNcbiArguments*   args = 0 /* D: app.GetArguments()   */,
                const CNcbiEnvironment* env  = 0 /* D: app.GetEnvironment() */,
                CNcbiIstream*           inp  = 0 /* see ::CCgiRequest(istr) */,
                CNcbiOstream*           out  = 0 /* see ::CCgiResponse(out) */,
                int                     ifd  = -1,
                int                     ofd  = -1,
                size_t                  errbuf_size = 256, /* see CCgiRequest */
                CCgiRequest::TFlags     flags = 0
                );

    CCgiContext(CCgiApplication&        app,
                CNcbiIstream*           inp /* see ::CCgiRequest(istr) */,
                CNcbiOstream*           out /* see ::CCgiResponse(out) */,
                CCgiRequest::TFlags     flags = 0
                );

    virtual ~CCgiContext(void);

    const CCgiApplication& GetApp(void) const;

    const CNcbiRegistry& GetConfig(void) const;
    CNcbiRegistry& GetConfig(void);
    
    // these methods will throw exception if no server context is set
    const CNcbiResource& GetResource(void) const;
    CNcbiResource&       GetResource(void);

    const CCgiRequest& GetRequest(void) const;
    CCgiRequest&       GetRequest(void);
    
    const CCgiResponse& GetResponse(void) const;
    CCgiResponse&       GetResponse(void);
    
    // these methods will throw exception if no server context set
    const CCgiServerContext& GetServCtx(void) const;
    CCgiServerContext&       GetServCtx(void);

    // message buffer functions
    CNcbiOstream& PrintMsg(CNcbiOstream& os);

    void PutMsg(const string& msg);
    void PutMsg(CCtxMsg*      msg);

    bool EmptyMsg(void);
    void ClearMsg(void);

    // request access wrappers

    // return entry from request
    // return empty string if no such entry
    // throw runtime_error if there are several entries with the same name
    const CCgiEntry& GetRequestValue(const string& name, bool* is_found = 0)
        const;

    void AddRequestValue    (const string& name, const CCgiEntry& value);
    void RemoveRequestValues(const string& name);
    void ReplaceRequestValue(const string& name, const CCgiEntry& value);

    /// Whether to use the port number when composing the CGI's own URL
    /// @sa GetSelfURL()
    /// @deprecated The flag is ignored, use GetSelfURL(void).
    enum ESelfUrlPort {
        eSelfUrlPort_Use,     ///< Use port number in self-URL
        eSelfUrlPort_Strip,   ///< Do not use port number in self-URL
        eSelfUrlPort_Default  ///< Use port number, except for NCBI front-ends
    };

    /// Using HTTP environment variables, compose the CGI's own URL as:
    ///   http://SERVER_NAME[:SERVER_PORT]/SCRIPT_NAME
    /// @deprecated The flag is ignored, use GetSelfURL(void).
    NCBI_DEPRECATED
    const string& GetSelfURL(ESelfUrlPort use_port) const
        { return GetSelfURL(); }

    /// Using HTTP environment variables, compose the CGI's own URL as:
    ///   http://SERVER_NAME[:SERVER_PORT]/SCRIPT_NAME
    /// Port is always included if it does not correspond to the scheme's
    /// default port.
    const string& GetSelfURL(void) const;

    // Which streams are ready?
    enum EStreamStatus {
        fInputReady  = 0x1,
        fOutputReady = 0x2
    };
    typedef int TStreamStatus;  // binary OR of 'EStreamStatus'
    TStreamStatus GetStreamStatus(STimeout* timeout) const;
    TStreamStatus GetStreamStatus(void) const; // supplies {0,0}

    string RetrieveTrackingId() const;

    /// Check if the context has any pending errors, perform any required actions
    /// (e.g. throw an exception).
    void CheckStatus(void) const;

private:
    CCgiServerContext& x_GetServerContext(void) const;
    void x_InitSession(CCgiRequest::TFlags flags);

    void x_SetStatus(CCgiException::EStatusCode code, const string& msg) const;

    CCgiApplication&      m_App;
    auto_ptr<CCgiRequest> m_Request;  // CGI request  information
    CCgiResponse          m_Response; // CGI response information
    auto_ptr<CCgiSession> m_Session;  // CGI session

    // message buffer
    typedef list< AutoPtr<CCtxMsg> > TMessages;
    TMessages m_Messages;

    // server context will be obtained from CCgiApp::LoadServerContext()
    auto_ptr<CCgiServerContext> m_ServerContext; // application defined context

    mutable string m_SelfURL;
    mutable string m_TrackingId; // cached tracking id

    // Request status code and message. The status is non-zero if there
    // is an error to report.
    mutable CCgiException::EStatusCode m_StatusCode;
    mutable string m_StatusMessage;

    // forbidden
    CCgiContext(const CCgiContext&);
    CCgiContext& operator=(const CCgiContext&);
}; 


/////////////////////////////////////////////////////////////////////////////
//
//  CCgiSessionParameters:
//  This class is used to pass additional optional parameters from a CGI
//  application to CGI session

class CCgiSessionParameters
{
public:

    /// Spescify which class is responsible for Session Storage destruction
    /// if set to eTakeOwnership, then a CGI session will delete it, otherwise
    /// the CGI application should do it. 
    /// Default the CGI session takes responsibility.
    void SetImplOwnership(EOwnership owner) { m_ImplOwner = owner; }
    
    /// Do not use a cookie to transfer session id between requests
    /// By default cookie is enabled
    void DisableCookie() { m_CookieEnabled = false; }

    /// Set name of the cookie with session id. 
    /// Default: ncbi_sessionid
    void SetSessionIdName(const string& name) { m_SessionIdName = name; }

    /// Set session cookie's domain
    /// Default: .ncbi.nlm.nih.gov
    void SetSessionCookieDomain(const string& domain)       
    { m_SessionCookieDomain = domain; }

    /// Set session cookie's path
    /// Default: /
    void SetSessionCookiePath(const string& path)
    { m_SessionCookiePath = path; }

    /// Set session cookie's expiration time
    /// Default: none
    void SetSessionCookieExpTime(const CTime& exp_time)
    { m_SessionCookieExpTime = exp_time; }

private:

    // Only CgiContext can create an instance of this class
    friend class CCgiContext;
    CCgiSessionParameters() : 
        m_ImplOwner(eTakeOwnership), m_CookieEnabled(true),
        m_SessionIdName(CCgiSession::kDefaultSessionIdName), 
        m_SessionCookieDomain(CCgiSession::kDefaultSessionCookieDomain),
        m_SessionCookiePath(CCgiSession::kDefaultSessionCookiePath) {}

    EOwnership m_ImplOwner;
    bool m_CookieEnabled;
    string m_SessionIdName;
    string m_SessionCookieDomain;
    string m_SessionCookiePath;
    CTime m_SessionCookieExpTime;
};

/* @} */


/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////
//  CCgiContext::
//

inline
const CCgiApplication& CCgiContext::GetApp(void) const
{ 
    return m_App;
}
    

inline
const CCgiRequest& CCgiContext::GetRequest(void) const
{
    return *m_Request;
}


inline
CCgiRequest& CCgiContext::GetRequest(void)
{
    return *m_Request;
}

    
inline
const CCgiResponse& CCgiContext::GetResponse(void) const
{
    return m_Response;
}


inline
CCgiResponse& CCgiContext::GetResponse(void)
{
    return m_Response;
}


inline
const CCgiServerContext& CCgiContext::GetServCtx(void) const
{
    return x_GetServerContext();
}


inline
CCgiServerContext& CCgiContext::GetServCtx(void)
{
    return x_GetServerContext();
}


inline
CNcbiOstream& CCgiContext::PrintMsg(CNcbiOstream& os)
{
    ITERATE (TMessages, it, m_Messages) {
        os << **it;
    }
    return os;
}


inline
void CCgiContext::PutMsg(const string& msg)
{
    m_Messages.push_back(new CCtxMsgString(msg));
}


inline
void CCgiContext::PutMsg(CCtxMsg* msg)
{
    m_Messages.push_back(msg);
}


inline
bool CCgiContext::EmptyMsg(void)
{
    return m_Messages.empty();
}


inline
void CCgiContext::ClearMsg(void)
{
    m_Messages.clear();
}


inline
CCgiContext::TStreamStatus CCgiContext::GetStreamStatus(void) const
{
    STimeout timeout = {0, 0};
    return GetStreamStatus(&timeout);
}


END_NCBI_SCOPE

#endif // NCBI_CGI_CTX__HPP
