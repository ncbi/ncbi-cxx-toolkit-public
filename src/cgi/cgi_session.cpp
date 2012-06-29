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

const char* CCgiSession::kDefaultSessionIdName = "ncbi_session_data";
const char* CCgiSession::kDefaultSessionCookieDomain = ".nih.gov";
const char* CCgiSession::kDefaultSessionCookiePath = "/";

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
    if (Exists()) {
        try {
            m_Impl->Reset();
        }
        catch (std::exception& e) {
            ERR_POST("Session implementation clean-up error: " << e.what());
        }
        catch (...) {
            ERR_POST("Session implementation clean-up error has occurred");
        }
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
    if (Exists()) {
        m_Impl->Reset();
        m_Status = eNotLoaded;
    }
    m_SessionId = id;
    //GetDiagContext().SetProperty(
    //    CDiagContext::kProperty_SessionID, m_SessionId);
}

void CCgiSession::ModifyId(const string& new_session_id)
{
    if (m_SessionId == new_session_id)
        return;
    if (!m_Impl)
        NCBI_THROW(CCgiSessionException, eImplNotSet,
                   "The session implementation is not set");
    if (!Exists())
        NCBI_THROW(CCgiSessionException, eSessionId,
                   "The session must be loaded");
    m_Impl->ModifySessionId(new_session_id);
    m_SessionId = new_session_id;
    //GetDiagContext().SetProperty(
    //    CDiagContext::kProperty_SessionID, m_SessionId);
}

void CCgiSession::Load()
{
    if (Exists())
        return;
    if (!m_Impl)
        NCBI_THROW(CCgiSessionException, eImplNotSet,
                   "The session implementation is not set");
    if (m_Status == eDeleted)
        NCBI_THROW(CCgiSessionException, eDeleted,
                   "Cannot load deleted session");
    if (m_Impl->LoadSession(GetId()))
        m_Status = eLoaded;
    else m_Status = eNotLoaded;
}

void CCgiSession::CreateNewSession()
{
    if (Exists())
        m_Impl->Reset();
    if (!m_Impl)
        NCBI_THROW(CCgiSessionException, eImplNotSet,
                   "The session implementation is not set");
    m_SessionId = m_Impl->CreateNewSession();
    //GetDiagContext().SetProperty(
    //    CDiagContext::kProperty_SessionID, m_SessionId);
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
    //GetDiagContext().SetProperty(
    //    CDiagContext::kProperty_SessionID, kEmptyStr);;
    m_Status = eDeleted;
}


const CCgiCookie * CCgiSession::GetSessionCookie() const
{
    if (m_CookieSupport == eNoCookie ||
        (!Exists() && m_Status != eDeleted))
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
