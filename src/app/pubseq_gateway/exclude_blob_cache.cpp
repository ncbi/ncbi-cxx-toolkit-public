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
 * Authors: Sergey Satskiy
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>

#include <algorithm>
#include "exclude_blob_cache.hpp"


bool CUserExcludeBlobs::IsInCache(int  sat, int  sat_key)
{
    m_LastTouch = std::chrono::steady_clock::now();

    return m_ExcludeBlobs.find(SExcludeBlobId(sat, sat_key)) !=
           m_ExcludeBlobs.cend();
}


ECacheAddResult CUserExcludeBlobs::AddBlobId(int  sat, int  sat_key)
{
    m_LastTouch = std::chrono::steady_clock::now();

    const auto  ret = m_ExcludeBlobs.emplace(sat, sat_key);
    if (ret.second) {
        // There was an insertion of a new item
        m_LRU.emplace_front(sat, sat_key);
        return eAdded;
    }

    // Note: not really effective; search + destroy + create instead of
    //       search + move to top
    SExcludeBlobId  blob_id(sat, sat_key);
    auto it = find(m_LRU.begin(), m_LRU.end(), blob_id);
    if (it != m_LRU.begin()) {
        m_LRU.erase(it);
        m_LRU.push_front(blob_id);
    }

    return eAlreadyInCache;
}


void CUserExcludeBlobs::Purge(size_t  purged_size)
{
    while (m_ExcludeBlobs.size() > purged_size) {
        m_ExcludeBlobs.erase(m_ExcludeBlobs.find(m_LRU.front()));
        m_LRU.pop_front();
    }
}


void CUserExcludeBlobs::Clear(void)
{
    m_ExcludeBlobs.clear();
    m_LRU.clear();
    m_LastTouch = std::chrono::steady_clock::now();
}


ECacheAddResult CExcludeBlobCache::AddBlobId(const string &  user,
                                             int  sat, int  sat_key)
{
    if (m_MaxCacheSize == 0)
        return eAdded;
    if (user.empty())
        return eAdded;

    while (m_Lock.exchange(true)) {}    // acquire top level lock

    const auto user_item = m_UserBlobs.find(user);
    CUserExcludeBlobs *  user_blobs;
    if (user_item == m_UserBlobs.cend()) {
        // Not found; create a new user info
        user_blobs = m_Pool.Get();
        m_UserBlobs[user] = user_blobs;
    } else {
        // Found
        user_blobs = user_item->second;
    }

    while (user_blobs->m_Lock.exchange(true)) {}    // acquire the user data lock
    m_Lock = false;                                 // release top level lock

    auto ret = user_blobs->AddBlobId(sat, sat_key);
    user_blobs->m_Lock = false;                     // release user data lock
    return ret;
}


bool CExcludeBlobCache::IsInCache(const string &  user,
                                  int  sat, int  sat_key)
{
    if (m_MaxCacheSize == 0)
        return false;
    if (user.empty())
        return false;

    while (m_Lock.exchange(true)) {}    // acquire top level lock

    const auto user_item = m_UserBlobs.find(user);
    CUserExcludeBlobs *  user_blobs;
    if (user_item == m_UserBlobs.cend()) {
        // Not found
        m_Lock = false;                 // release top level lock
        return false;
    }

    // Found
    user_blobs = user_item->second;

    while (user_blobs->m_Lock.exchange(true)) {}    // acquire the user data lock
    m_Lock = false;                                 // release top level lock

    auto ret = user_blobs->IsInCache(sat, sat_key);
    user_blobs->m_Lock = false;                     // release user data lock
    return ret;
}


// Garbage collecting
void CExcludeBlobCache::Purge(void)
{
    if (m_MaxCacheSize == 0)
        return;

    std::chrono::time_point<std::chrono::steady_clock>  limit =
        std::chrono::steady_clock::now() - m_InactivityTimeout;

    while (m_Lock.exchange(true)) {}    // acquire top level lock

    // Purge the users and their caches approprietly
    for (auto it = m_UserBlobs.begin(); it != m_UserBlobs.end(); ) {
        while (it->second->m_Lock.exchange(true)) {}    // acquire the user data lock
        if (it->second->m_LastTouch < limit) {
            m_ToDiscard.push_back(it->second);
            it->second->m_Lock = false;                 // release user data lock
            it = m_UserBlobs.erase(it);
        } else {
            m_ToPurge.push_back(it->second);
            it->second->m_Lock = false;                 // release user data lock
            ++it;
        }
    }

    m_Lock = false;                 // release top level lock

    // Discard obsolete users
    for (auto user_data : m_ToDiscard) {
        user_data->Clear();
        m_Pool.Return(user_data);
    }
    m_ToDiscard.clear();

    // Purge the rest
    for (auto user_data : m_ToPurge) {
        while (user_data->m_Lock.exchange(true)) {} // acquire the user data lock
        user_data->Purge(m_PurgedSize);
        user_data->m_Lock = false;                  // release the user data lock
    }
    m_ToPurge.clear();
}

