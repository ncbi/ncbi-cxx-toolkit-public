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

#include <corelib/ncbistre.hpp>
#include <corelib/ncbimisc.hpp>

#include <list>
#include <string>
#include <memory>

#include <stddef.h>

/** @addtogroup CGI
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class ICgiSession_Impl;
class CCgiRequest;
class CCgiCookie;

class NCBI_XCGI_EXPORT CCgiSession 
{
public:
    typedef list<string> TNames;

    static const string kDefaultSessionIdName;
    static const string kDefaultSessionCookieDomain;
    static const string kDefaultSessionCookiePath;

    /// Session status
    enum EStatus {
        eNew,        ///< The session has just been created
        eLoaded,     ///< The session is loaded
        eNotLoaded,  ///< The session has not been loaded yet
        eDeleted,    ///< The session is deleted
        eImplNotSet  ///< The CGI application didn't set the session
                     ///  implementation
    };

    /// Specifies if a client session cookie can be used to transfer
    /// a session id between request
    enum ECookieSupport {
        eUseCookie,     ///< A session cookie will be add to a response
        eDonotUseCookie ///< A sesion cookie will not be add to a response
    };
    
    CCgiSession(const CCgiRequest& request, 
                ICgiSession_Impl* impl, 
                EOwnership impl_owner = eTakeOwnership,
                ECookieSupport cookie_sup = eUseCookie);

    ~CCgiSession();

    /// Get a session id. Throws an exception if an id is not set and
    /// can not be retrived from the cgi request.
    const string& GetId() const;

    /// Set session id. 
    /// If the session was previously loaded it will be closed
    void SetId(const string& id);

    /// Load the sesion. Throws an exception if an id is not set and
    /// can not be retrived from the cgi request.
    void Load();

    /// Create a new session. 
    /// If the session was previously loaded it will be closed
    void CreateNewSession();

    /// Retrieve a list of attributes names attached to this session
    /// Throws an exception if the session is not loaded
    ///
    void GetAttributeNames(TNames& names) const;

    /// Get an input stream for an attribute with the given name
    /// If the attribute does not exist the empty stream is returned
    /// Throws an exception if the session is not loaded
    ///
    CNcbiIstream& GetAttrIStream(const string& name, 
                                 size_t* size = 0);

    /// Get an output stream for an attribute with the given name
    /// If the attribute does not exist it will be create and added 
    /// to the session. If the attribute exists its content will be
    /// overwritten.
    /// Throws an exception if the session is not loaded
    ///
    CNcbiOstream& GetAttrOStream(const string& name);

    /// Set a string attribute
    /// Throws an exception if the session is not loaded
    ///
    void SetAttribute(const string& name, const string& value);

    /// Get a string attribute. If the attribute does not exist an empty
    /// string is returned
    /// Throws an exception if the session is not loaded
    ///
    void GetAttribute(const string& name, string& value) const;

    /// Remove an attribute from the session
    /// Throws an exception if the session is not loaded
    ///
    void RemoveAttribute(const string& name);

    /// Delete the current session
    /// Throws an exception if the session is not loaded
    ///
    void DeleteSession();


    /// Get a session status
    EStatus GetStatus() const;

    /// Get a session id name. This name is used as a cookie name for
    /// a session cookie.
    ///
    const string& GetSessionIdName() const;
    
    /// Set a session id name
    void SetSessionIdName(const string& name);

    

    /// Set a session cookie domain
    void SetSessionCookieDomain(const string& domain);

    /// Set a session cookie path
    void SetSessionCookiePath(const string& path);
    
    /// Get a session cookie. Returns NULL if a session
    /// is not load or a cookie support is disable.
    const CCgiCookie * const GetSessionCookie() const;
    
private:
    const CCgiRequest& m_Request;
    ICgiSession_Impl* m_Impl;
    auto_ptr<ICgiSession_Impl> m_ImplGuard;
    ECookieSupport m_CookieSupport;

    string m_SessionId;

    string m_SessionIdName;
    string m_SessionCookieDomain;
    string m_SessionCookiePath;
    auto_ptr<CCgiCookie> m_SessionCookie;
    EStatus m_Status;

    string x_RetrieveSessionId() const;
    void x_Load() const;
private:
    CCgiSession(const CCgiSession&);
    CCgiSession& operator=(const CCgiSession&);
};


class NCBI_XCGI_EXPORT ICgiSession_Impl
{
public:
    typedef CCgiSession::TNames TNames;

    virtual ~ICgiSession_Impl();

    /// Create a new empty session. Returns a new
    /// session id
    ///
    virtual string CreateNewSession() = 0;

    /// Load a session this the given session id from
    /// the underlying storage. If the session with given id
    /// is not found it returns eNotLoaded status.
    ///
    virtual bool LoadSession(const string& sessionid) = 0;

    /// Retrieve a list of attributes names attached to this session
    ///
    virtual void GetAttributeNames(TNames& names) const = 0;

    /// Get an input stream for an attribute with the given name
    /// If the attribute does not exist the empty stream is returned
    ///
    virtual CNcbiIstream& GetAttrIStream(const string& name, 
                                         size_t* size = 0) = 0;

    /// Get an output stream for an attribute with the given name
    /// If the attribute does not exist it will be create and added 
    /// to the session. If the attribute exists its content will be
    /// overwritten.
    ///
    virtual CNcbiOstream& GetAttrOStream(const string& name) = 0;

    /// Set a string attribute
    ///
    virtual void SetAttribute(const string& name, const string& value) = 0;

    /// Get a string attribute. If the attribute does not exist an empty
    /// string is returned
    ///
    virtual void GetAttribute(const string& name, string& value) const = 0;

    /// Remove an attribute from the session
    ///
    virtual void RemoveAttribute(const string& name) = 0;

    /// Delete the current session
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


END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2005/12/20 01:28:46  ucko
 * +<memory> for auto_ptr<>, and rearrange other headers slightly.
 *
 * Revision 1.2  2005/12/19 16:55:03  didenko
 * Improved CGI Session implementation
 *
 * Revision 1.1  2005/12/15 18:21:15  didenko
 * Added CGI session support
 *
 * ===========================================================================
 */

#endif  /* CGI___SESSION__HPP */
