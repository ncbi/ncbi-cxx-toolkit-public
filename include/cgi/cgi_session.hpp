#ifndef CGI___SESSION__HPP
#define CGI___SESSION__HPP

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
* Author:  Maxim Didenko
*
*/

/// @file cgi_session.hpp
/// API to store CGI session data between Web requests.

#include <corelib/ncbistd.hpp>

#include <memory>

/** @addtogroup CGI
 *
 * @{
 */


BEGIN_NCBI_SCOPE

// fwd-decl
class ICgiSessionStorage;
class CCgiRequest;
class CCgiCookie;

/////////////////////////////////////////////////////////////////////////////
///
/// CCgiSession --
///
///   Facilitate the transfer of session ID between Web requests.
///   Store and retrieve the CGI session data from an external
///   data storage using the session ID.
///
class NCBI_XCGI_EXPORT CCgiSession 
{
public:
    typedef list<string> TNames;

    static const char* kDefaultSessionIdName;
    static const char* kDefaultSessionCookieDomain;
    static const char* kDefaultSessionCookiePath;

    /// Session status
    enum EStatus {
        eNew,        ///< The session has just been created
        eLoaded,     ///< The session is loaded
        eNotLoaded,  ///< The session has not been loaded yet
        eDeleted,    ///< The session is deleted
        eImplNotSet  ///< The CGI application didn't set the session
                     ///< implementation
    };

    /// Specifies if a client session cookie can be used to transfer
    /// session id between requests
    enum ECookieSupport {
        eUseCookie,  ///< A session cookie will     be added to the response
        eNoCookie    ///< A session cookie will not be added to the response
    };

    CCgiSession(const CCgiRequest&  request, 
                ICgiSessionStorage* impl, 
                EOwnership          impl_ownership = eTakeOwnership,
                ECookieSupport      cookie_support = eUseCookie);

    ~CCgiSession();

    /// Get session ID.
    /// @throw CCgiSessionException if session ID is not set and it
    ///        can not be retrieved from CGI request too.
    const string& GetId(void) const;

    /// Set session ID. 
    /// The previously loaded session (if any) will be closed.
    void SetId(const string& session_id);

    /// Modify session ID. 
    /// The session must be loaded before calling this method.
    void ModifyId(const string& new_session_id);

    /// Load the session.
    /// @throw CCgiSessionException if session ID is not set and it
    ///        can not be retrieved from CGI request too.
    void Load(void);

    /// Create new session. 
    /// The previously loaded session (if any) will be closed.
    void CreateNewSession(void);

    /// Retrieve names of all attributes attached to this session.
    /// @throw CCgiSessionException if the session is not loaded.
    TNames GetAttributeNames(void) const;

    /// Get input stream to read an attribute's data from.
    /// @param[in] name
    ///  Name of the attribute
    /// @param[out] size
    ///  Size of the attribute's data
    /// @return 
    ///  Stream to read attribute's data from.If the attribute does not exist, 
    ///  then return an empty stream.
    /// @throw CCgiSessionException if the session is not loaded.
    CNcbiIstream& GetAttrIStream(const string& name, size_t* size = NULL);

    /// Get output stream to write an attribute's data to.
    /// If the attribute does not exist it will be created and added 
    /// to the session. If the attribute exists its content will be
    /// overwritten.
    /// @param[in] name
    ///  Name of the attribute
    /// @throw CCgiSessionException if the session is not loaded.
    CNcbiOstream& GetAttrOStream(const string& name);

    /// Set attribute data as a string.
    /// @param[in] name
    ///  Name of the attribute to set
    /// @param[in] value
    ///  Value to set the attribute data to
    /// @throw CCgiSessionException if the session is not loaded.
    void SetAttribute(const string& name, const string& value);

    /// Get attribute data as string.
    /// @param[in] name
    ///  Name of the attribute to retrieve
    /// @return
    ///  Data of the attribute, if set.
    /// @throw CCgiSessionException with error code eNotLoaded
    ///  if the session has not been loaded yet;
    ///  CCgiSessionException with error code eAttrNotFound if
    ///  attribute with the specified name was not found;
    ///  CCgiSessionException with error code eImplException if
    ///  an error occured during attribute retrieval -- in the
    ///  latter case, more information can be obtained from the
    ///  embedded exception.
    string GetAttribute(const string& name) const;

    /// Remove attribute from the session.
    /// @param[in] name
    ///  Name of the attribute to remove
    /// @throw CCgiSessionException if the session is not loaded.
    void RemoveAttribute(const string& name);

    /// Delete current session
    /// @throw CCgiSessionException if the session is not loaded.
    void DeleteSession(void);

    /// Get current status of the session.
    EStatus GetStatus(void) const;

    /// Check if this session object is valid.
    /// @return True, if this session has been successfully loaded
    ///         or has just been created. False - if this session
    ///         does not exist and cannot be used.
    bool Exists(void) const;

    /// Get name for session ID.
    /// @sa SetSessionIdName
    const string& GetSessionIdName(void) const;

    /// Set name for session ID.
    /// This name is used as a cookie name for a session cookie.
    void SetSessionIdName(const string& name);

    /// Set session cookie domain
    /// @sa SetSessionIdName
    void SetSessionCookieDomain(const string& domain);

    /// Set session cookie path
    /// @sa SetSessionIdName
    void SetSessionCookiePath(const string& path);

    /// Set session cookie expiration time
    void SetSessionCookieExpTime(const CTime& exp_time);

    /// Get a cookie pertaining to the session. May create new cookie,
    /// if needed and allowed to.
    /// @return
    ///  Session CGI cookie; 
    ///  NULL if no session is loaded or if cookie support is disabled.
    const CCgiCookie* GetSessionCookie(void) const;

    /// Retrieve a session id from a query string or a session cookie
    string RetrieveSessionId() const;

private:
    const CCgiRequest& m_Request;
    ICgiSessionStorage* m_Impl;
    auto_ptr<ICgiSessionStorage> m_ImplGuard;
    ECookieSupport m_CookieSupport;

    string m_SessionId;

    string m_SessionIdName;
    string m_SessionCookieDomain;
    string m_SessionCookiePath;
    CTime m_SessionCookieExpTime;
    auto_ptr<CCgiCookie> m_SessionCookie;
    EStatus m_Status;

    void x_Load() const;
private:
    CCgiSession(const CCgiSession&);
    CCgiSession& operator=(const CCgiSession&);
};

inline bool CCgiSession::Exists(void) const
{
    return m_Status == eLoaded || m_Status == eNew;
}


/////////////////////////////////////////////////////////////////////////////
///
/// ICgiSessionStorage --
///
///   Implement data storage and retrieval for CCgiSession.
///   @sa CCgiSession
///


class NCBI_XCGI_EXPORT ICgiSessionStorage
{
public:
    typedef CCgiSession::TNames TNames;

    virtual ~ICgiSessionStorage();

    /// Create a new empty session. 
    /// @return ID of the new session
    virtual string CreateNewSession() = 0;

    /// Modify session id. 
    /// Change Id of the current session.
    virtual void ModifySessionId(const string& new_id) = 0;

    /// Load the session
    /// @param[in]
    ///  ID of the session
    /// @return true if the session was loaded, false otherwise
    virtual bool LoadSession(const string& sessionid) = 0;

    /// Retrieve names of all attributes attached to this session.
    virtual TNames GetAttributeNames(void) const = 0;

    /// Get input stream to read an attribute's data from.
    /// @param[in] name
    ///  Name of the attribute
    /// @param[out] size
    ///  Size of the attribute's data
    /// @return 
    ///  Stream to read attribute's data from.If the attribute does not exist, 
    ///  then return an empty stream.
    virtual CNcbiIstream& GetAttrIStream(const string& name, 
                                         size_t* size = 0) = 0;

    /// Get output stream to write an attribute's data to.
    /// If the attribute does not exist it will be created and added 
    /// to the session. If the attribute exists its content will be
    /// overwritten.
    /// @param[in] name
    ///  Name of the attribute
    virtual CNcbiOstream& GetAttrOStream(const string& name) = 0;

    /// Set attribute data as a string.
    /// @param[in] name
    ///  Name of the attribute to set
    /// @param[in] value
    ///  Value to set the attribute data to
    virtual void SetAttribute(const string& name, const string& value) = 0;

    /// Get attribute data as string.
    /// @param[in] name
    ///  Name of the attribute to retrieve
    /// @return
    ///  Data of the attribute, if set.
    /// @throw CCgiSessionException with error code eNotLoaded
    ///  if the session has not been loaded yet;
    ///  CCgiSessionException with error code eAttrNotFound if
    ///  attribute with the specified name was not found;
    ///  CCgiSessionException with error code eImplException if
    ///  an error occured during attribute retrieval -- in the
    ///  latter case, more information can be obtained from the
    ///  embedded exception.
    virtual string GetAttribute(const string& name) const = 0;

    /// Remove attribute from the session.
    /// @param[in] name
    ///  Name of the attribute to remove
    virtual void RemoveAttribute(const string& name) = 0;

    /// Delete current session
    virtual void DeleteSession() = 0;

    /// Reset the session. The an implementation should close 
    /// all input/ouptut streams here.
    virtual void Reset() = 0;
};

/////////////////////////////////////////////////////////////////////

inline 
CCgiSession::EStatus CCgiSession::GetStatus() const
{
    return m_Status;
}
inline
const string& CCgiSession::GetSessionIdName() const
{
    return m_SessionIdName;
}
inline
void CCgiSession::SetSessionIdName(const string& name)
{
    m_SessionIdName = name;
}
inline
void CCgiSession::SetSessionCookieDomain(const string& domain)
{
    m_SessionCookieDomain = domain;
}
inline
void CCgiSession::SetSessionCookiePath(const string& path)
{
    m_SessionCookiePath = path;
}
inline
void CCgiSession::SetSessionCookieExpTime(const CTime& exp_time)
{
    m_SessionCookieExpTime = exp_time;
}


END_NCBI_SCOPE


/* @} */

#endif  /* CGI___SESSION__HPP */
