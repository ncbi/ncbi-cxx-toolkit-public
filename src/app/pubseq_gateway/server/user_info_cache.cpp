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
#include "user_info_cache.hpp"
#include "pubseq_gateway.hpp"



optional<CPSG_MyNCBIRequest_WhoAmI::SUserInfo>
CUserInfoCache::GetUserInfo(const string &  cookie)
{
    optional<CPSG_MyNCBIRequest_WhoAmI::SUserInfo>  ret;
    auto &                                          counters = m_App->GetCounters();

    lock_guard<mutex>   guard(m_Lock);

    auto    it = m_Cache.find(cookie);
    if (it == m_Cache.cend()) {
        counters.Increment(nullptr, CPSGSCounters::ePSGS_UserInfoCacheMiss);
        return ret;
    }

    auto lru_it = find(m_LRU.begin(), m_LRU.end(), cookie);
    if (lru_it != m_LRU.begin()) {
        m_LRU.erase(lru_it);
        m_LRU.push_front(cookie);
    }

    counters.Increment(nullptr, CPSGSCounters::ePSGS_UserInfoCacheHit);
    it->second.m_LastTouch = psg_clock_t::now();
    ret = it->second.m_UserInfo;
    return ret;
}


void
CUserInfoCache::AddUserInfo(const string &  cookie,
                            const CPSG_MyNCBIRequest_WhoAmI::SUserInfo &  user_info)
{
    lock_guard<mutex>   guard(m_Lock);

    auto  find_it = m_Cache.find(cookie);
    if (find_it == m_Cache.end()) {
        // No cookie, add a new one
        m_Cache[cookie] = SUserInfoCacheItem(user_info);
        m_LRU.emplace_front(cookie);
        return;
    }

    // It already exists
    find_it->second.m_LastTouch = psg_clock_t::now();

    auto lru_it = find(m_LRU.begin(), m_LRU.end(), cookie);
    if (lru_it != m_LRU.begin()) {
        m_LRU.erase(lru_it);
        m_LRU.push_front(cookie);
    }
}


void CUserInfoCache::Maintain(void)
{
    lock_guard<mutex>   guard(m_Lock);

    if (m_Cache.size() <= m_HighMark)
        return;

    while (m_Cache.size() > m_LowMark) {
        m_Cache.erase(m_Cache.find(m_LRU.back()));
        m_LRU.pop_back();
    }
}

