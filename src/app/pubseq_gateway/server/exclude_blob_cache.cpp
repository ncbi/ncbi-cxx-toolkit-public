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


bool CUserExcludeBlobs::IsInCache(const SExcludeBlobId& blob_id,
                                  bool &  completed,
                                  psg_time_point_t &  completed_time)
{
    m_LastTouch = psg_clock_t::now();

    auto it = m_ExcludeBlobs.find(blob_id);
    if (it == m_ExcludeBlobs.cend())
        return false;

    completed = it->m_Completed;
    completed_time = it->m_CompletedTime;
    return true;
}


EPSGS_CacheAddResult CUserExcludeBlobs::AddBlobId(const SExcludeBlobId& blob_id,
                                                  bool &  completed,
                                                  psg_time_point_t &  completed_time)
{
    m_LastTouch = psg_clock_t::now();

    const auto  ret = m_ExcludeBlobs.emplace(blob_id);
    if (ret.second) {
        // There was an insertion of a new item
        m_LRU.emplace_front(blob_id);
        return ePSGS_Added;
    }

    // It already exists
    completed = ret.first->m_Completed;
    completed_time = ret.first->m_CompletedTime;

    // Note: not really effective; search + destroy + create instead of
    //       search + move to top
    auto it = find(m_LRU.begin(), m_LRU.end(), blob_id);
    if (it != m_LRU.begin()) {
        m_LRU.erase(it);
        m_LRU.push_front(blob_id);
    }

    return ePSGS_AlreadyInCache;
}


bool  CUserExcludeBlobs::SetCompleted(const SExcludeBlobId& blob_id, bool  new_val)
{
    m_LastTouch = psg_clock_t::now();

    auto it = m_ExcludeBlobs.find(blob_id);
    if (it == m_ExcludeBlobs.end())
        return false;

    // The compiler does not let to change the m_Completed value via an
    // iterator saying that the object is constant.
    // The m_Completed though is not participating in the element comparisons
    // so it is safe to change it.
    SExcludeBlobId *    p = const_cast<SExcludeBlobId*>(&(*it));
    p->m_Completed = new_val;
    p->m_CompletedTime = psg_clock_t::now();
    return true;
}


bool CUserExcludeBlobs::Remove(const SExcludeBlobId& blob_id)
{
    m_LastTouch = psg_clock_t::now();

    auto it = m_ExcludeBlobs.find(blob_id);
    if (it == m_ExcludeBlobs.end())
        return false;   // not found

    m_ExcludeBlobs.erase(it);

    auto lru_it = find(m_LRU.begin(), m_LRU.end(), blob_id);
    if (lru_it != m_LRU.end())      // paranoid check: if the blob id is found
        m_LRU.erase(lru_it);        // in the set then it must be in LRU
    return true;
}


void CUserExcludeBlobs::Purge(size_t  purged_size)
{
    while (m_ExcludeBlobs.size() > purged_size) {
        m_ExcludeBlobs.erase(m_ExcludeBlobs.find(m_LRU.back()));
        m_LRU.pop_back();
    }
}


void CUserExcludeBlobs::Clear(void)
{
    m_ExcludeBlobs.clear();
    m_LRU.clear();
    m_LastTouch = psg_clock_t::now();
}


EPSGS_CacheAddResult CExcludeBlobCache::AddBlobId(const string &  user,
                                                  const SExcludeBlobId& blob_id,
                                                  bool &  completed,
                                                  psg_time_point_t &  completed_time)
{
    if (m_MaxCacheSize == 0)
        return ePSGS_Added;
    if (user.empty())
        return ePSGS_Added;

    m_Lock.lock();                                  // acquire top level lock

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

    user_blobs->m_Lock.lock();                      // acquire the user data lock
    m_Lock.unlock();                                // release top level lock

    auto ret = user_blobs->AddBlobId(blob_id, completed, completed_time);
    user_blobs->m_Lock.unlock();                    // release user data lock
    return ret;
}


bool CExcludeBlobCache::IsInCache(const string &  user,
                                  const SExcludeBlobId& blob_id,
                                  bool &  completed,
                                  psg_time_point_t &  completed_time)
{
    if (m_MaxCacheSize == 0)
        return false;
    if (user.empty())
        return false;

    m_Lock.lock();                      // acquire top level lock

    const auto user_item = m_UserBlobs.find(user);
    CUserExcludeBlobs *  user_blobs;
    if (user_item == m_UserBlobs.cend()) {
        // Not found
        m_Lock.unlock();                // release top level lock
        return false;
    }

    // Found
    user_blobs = user_item->second;

    user_blobs->m_Lock.lock();          // acquire the user data lock
    m_Lock.unlock();                    // release top level lock

    auto ret = user_blobs->IsInCache(blob_id, completed, completed_time);
    user_blobs->m_Lock.unlock();        // release user data lock
    return ret;
}


bool CExcludeBlobCache::SetCompleted(const string &  user,
                                     const SExcludeBlobId& blob_id, bool  new_val)
{
    if (m_MaxCacheSize == 0)
        return true;
    if (user.empty())
        return true;

    m_Lock.lock();                      // acquire top level lock

    const auto user_item = m_UserBlobs.find(user);
    CUserExcludeBlobs *  user_blobs;
    if (user_item == m_UserBlobs.cend()) {
        // Not found
        m_Lock.unlock();                // release top level lock
        return false;
    }

    // Found
    user_blobs = user_item->second;

    user_blobs->m_Lock.lock();          // acquire the user data lock
    m_Lock.unlock();                    // release top level lock

    auto ret = user_blobs->SetCompleted(blob_id, new_val);
    user_blobs->m_Lock.unlock();        // release user data lock
    return ret;
}


bool CExcludeBlobCache::Remove(const string &  user,
                               const SExcludeBlobId& blob_id)
{
    if (m_MaxCacheSize == 0)
        return true;
    if (user.empty())
        return true;

    m_Lock.lock();                      // acquire top level lock

    const auto user_item = m_UserBlobs.find(user);
    CUserExcludeBlobs *  user_blobs;
    if (user_item == m_UserBlobs.cend()) {
        // Not found
        m_Lock.unlock();                // release top level lock
        return false;
    }

    // Found
    user_blobs = user_item->second;

    user_blobs->m_Lock.lock();          // acquire the user data lock
    m_Lock.unlock();                    // release top level lock

    auto ret = user_blobs->Remove(blob_id);
    user_blobs->m_Lock.unlock();        // release user data lock
    return ret;
}


// Garbage collecting
void CExcludeBlobCache::Purge(void)
{
    if (m_MaxCacheSize == 0)
        return;

    psg_time_point_t    limit = psg_clock_t::now() - m_InactivityTimeout;

    m_Lock.lock();                          // acquire top level lock

    // Purge the users and their caches approprietly
    for (auto it = m_UserBlobs.begin(); it != m_UserBlobs.end(); ) {
        it->second->m_Lock.lock();          // acquire the user data lock
        if (it->second->m_LastTouch < limit) {
            m_ToDiscard.push_back(it->second);
            it->second->m_Lock.unlock();    // release user data lock
            it = m_UserBlobs.erase(it);
        } else {
            m_ToPurge.push_back(it->second);
            it->second->m_Lock.unlock();    // release user data lock
            ++it;
        }
    }

    m_Lock.unlock();                        // release top level lock

    // Discard obsolete users
    for (auto user_data : m_ToDiscard) {
        user_data->Clear();
        m_Pool.Return(user_data);
    }
    m_ToDiscard.clear();

    // Purge the rest
    for (auto user_data : m_ToPurge) {
        user_data->m_Lock.lock();           // acquire the user data lock
        user_data->Purge(m_PurgedSize);
        user_data->m_Lock.unlock();         // release the user data lock
    }
    m_ToPurge.clear();
}

