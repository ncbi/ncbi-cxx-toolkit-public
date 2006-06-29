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
 * Author: Maxim Didenko
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbitime.hpp>

#include <cgi/cgi_session.hpp>
#include <cgi/ncbicgi.hpp>
#include <cgi/cgi_exception.hpp>

BEGIN_NCBI_SCOPE

const string CCgiSession::kDefaultSessionIdName = "ncbi_sessionid";
const string CCgiSession::kDefaultSessionCookieDomain = ".ncbi.nlm.nih.gov";
const string CCgiSession::kDefaultSessionCookiePath = "/";

CCgiSession::CCgiSession(const CCgiRequest& request, 
                         ICgiSessionStorage* impl, 
                         EOwnership impl_owner,
                         ECookieSupport cookie_sup)
    : m_Request(request), m_Impl(impl), m_CookieSupport(cookie_sup),
      m_SessionIdName(kDefaultSessionIdName),
      m_SessionCookieDomain(kDefaultSessionCookieDomain),
      m_SessionCookiePath(kDefaultSessionCookiePath)
{
    if (impl_owner == eTakeOwnership)
        m_ImplGuard.reset(m_Impl);
    m_Status = eNotLoaded;
}

CCgiSession::~CCgiSession()
{
    if (m_Status == eLoaded || m_Status == eNew) {
        try {
            m_Impl->Reset();
        } catch(...) {}
    }
}

const string& CCgiSession::GetId() const
{
    if (m_SessionId.empty()) {
        const_cast<CCgiSession*>(this)->m_SessionId = RetrieveSessionId();
        if (m_SessionId.empty())
            NCBI_THROW(CCgiSessionException, eSessionId,
                       "SessionId can not be retrieved from the cgi request");
    }
    return m_SessionId;
}

void CCgiSession::SetId(const string& id)
{
    if (m_SessionId == id) 
        return;
    if (m_Status == eLoaded || m_Status == eNew) {
        m_Impl->Reset();
        m_Status = eNotLoaded;
    }
    m_SessionId = id;
    //GetDiagContext().SetProperty("session_id", m_SessionId);
}

void CCgiSession::ModifyId(const string& new_session_id)
{
    if (m_SessionId == new_session_id)
        return;
    if (!m_Impl)
        NCBI_THROW(CCgiSessionException, eImplNotSet,
                   "The session implemetatin is not set");
    if (m_Status != eLoaded && m_Status != eNew)
        NCBI_THROW(CCgiSessionException, eSessionId,
                   "The Session must be load.");
    m_Impl->ModifySessionId(new_session_id);
    m_SessionId = new_session_id;
    //GetDiagContext().SetProperty("session_id", m_SessionId);
}

void CCgiSession::Load()
{
    if (m_Status == eLoaded || m_Status == eNew)
        return;
    if (!m_Impl)
        NCBI_THROW(CCgiSessionException, eImplNotSet,
                   "The session implemetatin is not set");
    if (m_Status == eDeleted)
        NCBI_THROW(CCgiSessionException, eDeleted,
                   "Can not load deleted session");
    if (m_Impl->LoadSession(GetId()))
        m_Status = eLoaded;
    else m_Status = eNotLoaded;
}

void CCgiSession::CreateNewSession()
{
    if (m_Status == eLoaded || m_Status == eNew)
        m_Impl->Reset();
    if (!m_Impl)
        NCBI_THROW(CCgiSessionException, eImplNotSet,
                   "The session implemetatin is not set");
    m_SessionId = m_Impl->CreateNewSession();
    //GetDiagContext().SetProperty("session_id", m_SessionId);
    m_Status = eNew;
}

CCgiSession::TNames CCgiSession::GetAttributeNames() const
{
    x_Load();
    return m_Impl->GetAttributeNames();
}

CNcbiIstream& CCgiSession::GetAttrIStream(const string& name, 
                                          size_t* size)
{
    Load();
    return m_Impl->GetAttrIStream(name, size);
}

CNcbiOstream& CCgiSession::GetAttrOStream(const string& name)
{
    Load();
    return m_Impl->GetAttrOStream(name);
}

void CCgiSession::SetAttribute(const string& name, const string& value)
{
    Load();
    m_Impl->SetAttribute(name,value);
}

string CCgiSession::GetAttribute(const string& name) const
{
    x_Load();
    return m_Impl->GetAttribute(name);
}

void CCgiSession::RemoveAttribute(const string& name)
{
    Load();
    m_Impl->RemoveAttribute(name);
}

void CCgiSession::DeleteSession()
{
    if (m_SessionId.empty()) {
        m_SessionId = RetrieveSessionId();
        if (m_SessionId.empty())
            return;
    }
    Load();
    m_Impl->DeleteSession();
    //GetDiagContext().SetProperty("session_id", kEmptyStr);;
    m_Status = eDeleted;
}


const CCgiCookie * const CCgiSession::GetSessionCookie() const
{
    if (m_CookieSupport == eNoCookie ||
        (m_Status != eLoaded && m_Status != eNew && m_Status != eDeleted))
        return NULL;

    if (!m_SessionCookie.get()) {
        const_cast<CCgiSession*>(this)->
            m_SessionCookie.reset(new CCgiCookie(m_SessionIdName,
                                                 m_SessionId,
                                                 m_SessionCookieDomain,
                                                 m_SessionCookiePath));
        if (m_Status == eDeleted) {
            CTime exp(CTime::eCurrent, CTime::eGmt);
            exp.AddYear(-10);
            const_cast<CCgiSession*>(this)->
                m_SessionCookie->SetExpTime(exp);
        } else {
            if (!m_SessionCookieExpTime.IsEmpty())
                const_cast<CCgiSession*>(this)->
                    m_SessionCookie->SetExpTime(m_SessionCookieExpTime);
        }
        
    }
    return m_SessionCookie.get();
}

string CCgiSession::RetrieveSessionId() const
{
    if (m_CookieSupport == eUseCookie) {
        const CCgiCookies& cookies = m_Request.GetCookies();
        const CCgiCookie* cookie = cookies.Find(m_SessionIdName, kEmptyStr, kEmptyStr); 

        if (cookie) {
            return cookie->GetValue();
        }
    }
    bool is_found = false;
    const CCgiEntry& entry = m_Request.GetEntry(m_SessionIdName, &is_found);
    if (is_found) {
        return entry.GetValue();
    }
    return kEmptyStr;
}

void CCgiSession::x_Load() const
{
    const_cast<CCgiSession*>(this)->Load();
}

///////////////////////////////////////////////////////////
ICgiSessionStorage::~ICgiSessionStorage()
{
}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.11  2006/06/29 14:32:43  didenko
 * Added tracking cookie
 *
 * Revision 1.10  2006/06/27 20:08:27  golikov
 * condition fix
 *
 * Revision 1.9  2006/06/27 18:52:33  didenko
 * Added methods which allow modifing the session id
 *
 * Revision 1.8  2006/06/12 18:44:34  didenko
 * Fixed cgi sessionid logging
 *
 * Revision 1.7  2006/06/09 14:31:05  golikov
 * typo; removed diag interaction
 *
 * Revision 1.6  2006/06/08 16:54:13  didenko
 * Modified the expiration date for a deleted cgi session
 *
 * Revision 1.5  2006/06/08 15:58:10  didenko
 * Added possibility to set an expiration date for a session cookie
 *
 * Revision 1.4  2006/06/06 16:44:50  grichenk
 * Set session id in CDiagContext.
 *
 * Revision 1.3  2005/12/20 20:36:02  didenko
 * Comments cosmetics
 * Small interace changes
 *
 * Revision 1.2  2005/12/19 16:55:04  didenko
 * Improved CGI Session implementation
 *
 * Revision 1.1  2005/12/15 18:21:15  didenko
 * Added CGI session support
 *
 * ===========================================================================
 */
