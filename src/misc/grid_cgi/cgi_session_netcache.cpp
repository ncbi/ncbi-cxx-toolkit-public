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
 *
 */
#include <ncbi_pch.hpp>

#include <corelib/ncbireg.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbistr.hpp>

#include <cgi/ncbicgi.hpp>
#include <cgi/cgiapp.hpp>

#include <misc/grid_cgi/cgi_session_netcache.hpp>

#include <connect/services/netcache_client.hpp>
#include <connect/services/grid_default_factories.hpp>

BEGIN_NCBI_SCOPE

CCgiSession_Netcache::CCgiSession_Netcache(const IRegistry& conf) 
    : m_Dirty(false), m_Status(eNotLoaded)
{
    CNetScheduleStorageFactory_NetCache factory(conf);
    m_Storage.reset(factory.CreateInstance());
}

CCgiSession_Netcache::~CCgiSession_Netcache()
{
    try {
        x_Reset();
    } catch (...) {}
}

void CCgiSession_Netcache::CreateNewSession()
{
    m_Blobs.clear();
    m_SessionId.clear();
    x_Reset();
    m_SessionId = m_Storage->CreateEmptyBlob();
    m_Status = eLoaded;
}


ICgiSession::EStatus CCgiSession_Netcache::LoadSession(const string& sessionid)
{
    m_Blobs.clear();
    m_SessionId.clear();
    x_Reset();
    string master_value;
    try {
        master_value = m_Storage->GetBlobAsString(sessionid);
    }
    catch(...) {
        m_Status = eNotLoaded;
        return m_Status;
    }
    m_SessionId = sessionid;
    m_Status = eLoaded;
    list<string> pairs;
    NStr::Split(master_value, ";", pairs);
    ITERATE(list<string>, it, pairs) {
        string blobid, blobname;
        NStr::SplitInTwo(*it, "|", blobname, blobid);
        m_Blobs[blobname] = blobid;          
    }
    return m_Status;
}

string CCgiSession_Netcache::GetSessionId() const
{
    return m_SessionId;
}

ICgiSession::EStatus CCgiSession_Netcache::GetStatus() const
{
    return m_Status;
}

void CCgiSession_Netcache::GetAttributeNames(TNames& names) const
{
    if (m_Status != eLoaded) return;
    ITERATE(TBlobs, it, m_Blobs) {
        names.push_back(it->first);
    }
}

CNcbiIstream& CCgiSession_Netcache::GetAttrIStream(const string& name, 
                                                   size_t* size)
{
    static CNcbiIstrstream sEmptyStream("",0);
    if (size) *size = 0;
    if (m_Status != eLoaded) return sEmptyStream;

    x_Reset();
    TBlobs::const_iterator i = m_Blobs.find(name);
    if (i == m_Blobs.end()) {
        if (size) *size = 0;
        return sEmptyStream;
    }
    return m_Storage->GetIStream(i->second, size);
}
CNcbiOstream& CCgiSession_Netcache::GetAttrOStream(const string& name)
{
    x_Reset();
    m_Dirty = true;
    string& blobid = m_Blobs[name];
    return m_Storage->CreateOStream(blobid);
}

void CCgiSession_Netcache::SetAttribute(const string& name, const string& value)
{
    if (m_Status != eLoaded) return;
    x_Reset();
    m_Dirty = true;
    string& blobid = m_Blobs[name];
    CNcbiOstream& os = m_Storage->CreateOStream(blobid);
    os << value;
    x_Reset();
}
void CCgiSession_Netcache::GetAttribute(const string& name, string& value) const
{
    if (m_Status != eLoaded) return;
    const_cast<CCgiSession_Netcache*>(this)->x_Reset();
    TBlobs::const_iterator i = m_Blobs.find(name);
    if (i == m_Blobs.end()) {
        value = "";
        return;
    }
    value = m_Storage->GetBlobAsString(i->second);
}

void CCgiSession_Netcache::RemoveAttribute(const string& name)
{
    if (m_Status != eLoaded) return;
    TBlobs::iterator i = m_Blobs.find(name);
    if (i == m_Blobs.end())
        return;           
    x_Reset(); 
    string blobid = i->second;
    m_Blobs.erase(i);
    m_Storage->DeleteBlob(blobid);
    m_Dirty = true;
    x_Reset();
}
void CCgiSession_Netcache::DeleteSession()
{
    if (m_Status != eLoaded) return;
    x_Reset();
    ITERATE(TBlobs, it, m_Blobs) {
        m_Storage->DeleteBlob(it->second);
    }
    m_Storage->DeleteBlob(m_SessionId);
    m_Status = eDeleted;
}


void CCgiSession_Netcache::x_Reset()
{
    m_Storage->Reset();
    if (m_Dirty) {
        string master_value;
        ITERATE(TBlobs, it, m_Blobs) {
            if (it != m_Blobs.begin())
                master_value += ";";
            master_value += it->first + "|" + it->second;
        }
        CNcbiOstream& os = m_Storage->CreateOStream(m_SessionId);
        os << master_value;
        m_Storage->Reset();
        m_Dirty = false;
    }
}


END_NCBI_SCOPE

/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/12/15 18:21:16  didenko
 * Added CGI session support
 *
 * ===========================================================================
 */
