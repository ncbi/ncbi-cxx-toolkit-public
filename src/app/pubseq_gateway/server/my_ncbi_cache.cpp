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
#include "pubseq_gateway.hpp"
#include "pubseq_gateway_utils.hpp"
#include "my_ncbi_cache.hpp"


// The mutex is used when acces to more than one case in a sequence is
// required. It happens when:
// - the caches are checked before sending a request to my ncbi
//   (CPSGS_CassProcessorBase::PopulateMyNCBIUser)
// - the caches are updated in my ncbi callback
//   (CMyNCBIErrorCallback::operator())
mutex g_MyNCBICacheLock;


// libuv glue: user data structure to do a callback and
// the libuv callback implementation for the success case
struct SMyNCBISuccessCBData
{
    string                                  m_Cookie;
    CPSG_MyNCBIRequest_WhoAmI::SUserInfo    m_UserInfo;
    TMyNCBIDataCB                           m_DataCB;
};

void my_ncbi_success_cb(void *  user_data)
{
    SMyNCBISuccessCBData *  cb_data = (SMyNCBISuccessCBData*)(user_data);
    cb_data->m_DataCB(cb_data->m_Cookie, cb_data->m_UserInfo);
    delete cb_data;
}

// libuv glue: user data structure to do a callback and
// the libuv callback implementation for the not found case
struct SMyNCBINotFoundCBData
{
    string                                  m_Cookie;
    TMyNCBIErrorCB                          m_NotFoundCB;
};

void my_ncbi_not_found_cb(void *  user_data)
{
    SMyNCBINotFoundCBData *  cb_data = (SMyNCBINotFoundCBData*)(user_data);
    cb_data->m_NotFoundCB(cb_data->m_Cookie, CRequestStatus::e404_NotFound,
                          -1, eDiag_Error, "");
    delete cb_data;
}

// libuv glue: user data structure to do a callback and
// the libuv callback implementation for the error case
struct SMyNCBIErrorCBData
{
    string                                  m_Cookie;
    TMyNCBIErrorCB                          m_ErrorCB;
    CRequestStatus::ECode                   m_Status;
    int                                     m_Code;
    EDiagSev                                m_Severity;
    string                                  m_Message;
};

void my_ncbi_error_cb(void *  user_data)
{
    SMyNCBIErrorCBData *  cb_data = (SMyNCBIErrorCBData*)(user_data);
    cb_data->m_ErrorCB(cb_data->m_Cookie, cb_data->m_Status,
                       cb_data->m_Code, cb_data->m_Severity,
                       cb_data->m_Message);
    delete cb_data;
}


void
SMyNCBIOKCacheItem::x_OnSuccess(const string &  cookie,
                                const CPSG_MyNCBIRequest_WhoAmI::SUserInfo &  user_info)
{
    // This will update last touch, status and data +
    // will schedule callbacks to the waiters and clear the wait list.
    m_LastTouch = psg_clock_t::now();
    m_UserInfo = user_info;
    m_Status = ePSGS_Ready;

    // Schedule data callbacks in the waiters uv loops
    for (const auto &  waiter : m_WaitList) {
        // Delete is done in the callback
        SMyNCBISuccessCBData *      user_data = new SMyNCBISuccessCBData();
        user_data->m_Cookie = cookie;
        user_data->m_UserInfo = user_info;
        user_data->m_DataCB = waiter.m_DataCB;

        waiter.m_Processor->PostponeInvoke(my_ncbi_success_cb,
                                           (void*)(user_data));
    }
}


void
SMyNCBIOKCacheItem::x_OnError(const string &  cookie,
                              CRequestStatus::ECode  status,
                              int  code,
                              EDiagSev  severity,
                              const string &  message)
{
    // Schedule error callbacks in the waiters uv loops
    for (const auto &  waiter : m_WaitList) {
        // Delete is done in the callback
        SMyNCBIErrorCBData *    user_data = new SMyNCBIErrorCBData();
        user_data->m_Cookie = cookie;
        user_data->m_ErrorCB = waiter.m_ErrorCB;
        user_data->m_Status = status;
        user_data->m_Code = code;
        user_data->m_Severity = severity;
        user_data->m_Message = message;

        waiter.m_Processor->PostponeInvoke(my_ncbi_error_cb,
                                           (void*)(user_data));
    }
}


void SMyNCBIOKCacheItem::x_OnNotFound(const string &  cookie)
{
    // Schedule not found callbacks in the waiters uv loops
    for (const auto &  waiter : m_WaitList) {
        // Delete is done in the callback
        SMyNCBINotFoundCBData *     user_data = new SMyNCBINotFoundCBData();
        user_data->m_Cookie = cookie;

        // The error cb is used for not found as well. It sets the status to
        // 404
        user_data->m_NotFoundCB = waiter.m_ErrorCB;

        waiter.m_Processor->PostponeInvoke(my_ncbi_not_found_cb,
                                           (void*)(user_data));
    }
}


void SMyNCBIOKCacheItem::x_RemoveWaiter(IPSGS_Processor *  processor)
{
    for (auto it = m_WaitList.begin(); it != m_WaitList.end(); ++it) {
        if (it->m_Processor == processor) {
            m_WaitList.erase(it);
            return;
        }
    }
}


optional<CMyNCBIOKCache::SUserInfoItem>
CMyNCBIOKCache::GetUserInfo(IPSGS_Processor *  processor,
                            const string &  cookie,
                            TMyNCBIDataCB  data_cb,
                            TMyNCBIErrorCB  error_cb)
{
    optional<SUserInfoItem>     ret;
    auto &                      counters = m_App->GetCounters();

    lock_guard<mutex>   guard(m_Lock);

    auto    it = m_Cache.find(cookie);
    if (it == m_Cache.cend()) {
        counters.Increment(nullptr, CPSGSCounters::ePSGS_MyNCBIOKCacheMiss);

        // Created with the status InProgress;
        // Return is not set i.e. the caller must initiate the my ncbi request
        m_Cache[cookie] = SMyNCBIOKCacheItem();
        return ret;
    }

    SUserInfoItem       ret_item;

    // Here: the item is found. It can be in InProgress or in Ready state
    if (it->second.m_Status == SMyNCBIOKCacheItem::ePSGS_Ready) {
        // The user info is at hand. Prepare the return value accordingly
        ret_item.m_Status = SMyNCBIOKCacheItem::ePSGS_Ready;
        ret_item.m_UserInfo = it->second.m_UserInfo;

        counters.Increment(nullptr, CPSGSCounters::ePSGS_MyNCBIOKCacheHit);
    } else {
        // Here: m_Status == SMyNCBIOKCacheItem::ePSGS_InProgress
        // There is no user info. Add the requester to the wait list and set
        // only the status for the return value.
        ret_item.m_Status = SMyNCBIOKCacheItem::ePSGS_InProgress;
        it->second.x_AddToWaitList(processor, data_cb, error_cb);

        counters.Increment(nullptr, CPSGSCounters::ePSGS_MyNCBIOKCacheWaitHit);
    }

    auto lru_it = find(m_LRU.begin(), m_LRU.end(), cookie);
    if (lru_it != m_LRU.begin()) {
        m_LRU.erase(lru_it);
        m_LRU.push_front(cookie);
    }

    it->second.m_LastTouch = psg_clock_t::now();

    ret = ret_item;
    return ret;
}


void
CMyNCBIOKCache::AddUserInfo(const string &  cookie,
                            const CPSG_MyNCBIRequest_WhoAmI::SUserInfo &  user_info)
{
    lock_guard<mutex>   guard(m_Lock);

    auto  find_it = m_Cache.find(cookie);
    if (find_it == m_Cache.end()) {
        // No cookie. It should not really happened because someone had to
        // request it and thus the cookie record should be here.
        // To be safe, create a new record here.
        SMyNCBIOKCacheItem      item;
        item.m_UserInfo = user_info;
        item.m_Status = SMyNCBIOKCacheItem::ePSGS_Ready;
        m_Cache[cookie] = item;
        m_LRU.emplace_front(cookie);
        return;
    }

    // It already exists. Update LRU
    auto lru_it = find(m_LRU.begin(), m_LRU.end(), cookie);
    if (lru_it != m_LRU.begin()) {
        m_LRU.erase(lru_it);
        m_LRU.push_front(cookie);
    }

    // This will update last touch, status and data +
    // will schedule callbacks to the waiters and clear the wait list.
    find_it->second.x_OnSuccess(cookie, user_info);
}


void CMyNCBIOKCache::OnError(const string &  cookie,
                             CRequestStatus::ECode  status,
                             int  code,
                             EDiagSev  severity,
                             const string &  message)
{
    lock_guard<mutex>   guard(m_Lock);

    auto  find_it = m_Cache.find(cookie);
    if (find_it != m_Cache.end()) {
        // Will schedule a notification for those who waits
        find_it->second.x_OnError(cookie, status, code, severity, message);

        // Remove it from cache because no activity is expected
        m_Cache.erase(find_it);

        auto lru_it = find(m_LRU.begin(), m_LRU.end(), cookie);
        if (lru_it != m_LRU.end()) {
            m_LRU.erase(lru_it);
        }
    }
}


void CMyNCBIOKCache::OnNotFound(const string &  cookie){
    lock_guard<mutex>   guard(m_Lock);

    auto  find_it = m_Cache.find(cookie);
    if (find_it != m_Cache.end()) {
        // Will schedule a notification for those who waits
        find_it->second.x_OnNotFound(cookie);

        // Remove it from cache because no activity is expected
        m_Cache.erase(find_it);

        auto lru_it = find(m_LRU.begin(), m_LRU.end(), cookie);
        if (lru_it != m_LRU.end()) {
            m_LRU.erase(lru_it);
        }
    }
}


void CMyNCBIOKCache::ClearWaitingProcessor(const string &  cookie,
                                           IPSGS_Processor *  processor)
{
    lock_guard<mutex>   guard(m_Lock);

    auto  find_it = m_Cache.find(cookie);
    if (find_it != m_Cache.end()) {
        if (find_it->second.m_Status == SMyNCBIOKCacheItem::ePSGS_InProgress) {
            // Only if the date are not delivered yet it makes sense to check
            // for the waiters
            find_it->second.x_RemoveWaiter(processor);
        }
    }
}


void CMyNCBIOKCache::ClearInitiatedRequest(const string &  cookie)
{
    lock_guard<mutex>   guard(m_Lock);

    auto  find_it = m_Cache.find(cookie);
    if (find_it != m_Cache.end()) {
        // Notify those who are waiting that there is an error receiving a
        // reply from my NCBI if the myNCBI reply is in progress
        if (find_it->second.m_Status == SMyNCBIOKCacheItem::ePSGS_InProgress) {
            find_it->second.x_OnError(cookie, CRequestStatus::e503_ServiceUnavailable,
                                      ePSGS_MyNCBIRequestInitiatorDestroyed, eDiag_Error,
                                      "The initiator of the myNCBI request for cookie " +
                                      SanitizeInputValue(cookie) + " is destroyed. "
                                      "So the myNCBI reply will not be delivered. "
                                      "Please try again.");

            // Remove it from cache because no activity is expected
            m_Cache.erase(find_it);

            m_LRU.erase(find(m_LRU.begin(), m_LRU.end(), cookie));
        }
    }
}


void CMyNCBIOKCache::Maintain(void)
{
    lock_guard<mutex>   guard(m_Lock);

    if (m_Cache.size() <= m_HighMark)
        return;

    // Cannot remove records which have a non empty wait list or it is in
    // progress.
    // If to do so then those request will freeze forever-ish.
    size_t          need_to_delete_cnt = m_Cache.size() - m_LowMark;
    list<string>    to_be_deleted;

    for (auto  it = m_LRU.rbegin(); it != m_LRU.rend(); ++it) {
        auto    cache_it = m_Cache.find(*it);
        if (cache_it->second.m_WaitList.empty() &&
            cache_it->second.m_Status == SMyNCBIOKCacheItem::ePSGS_Ready) {
            // Can remove - the wait list is empty and the request has been
            // completed
            to_be_deleted.push_back(*it);
            --need_to_delete_cnt;
            if (need_to_delete_cnt == 0)
                break;
        }
    }

    // Now the actual removal
    for (const auto &  cookie_to_delete: to_be_deleted) {
        m_Cache.erase(m_Cache.find(cookie_to_delete));
        m_LRU.erase(find(m_LRU.begin(), m_LRU.end(), cookie_to_delete));
    }
}


bool CMyNCBINotFoundCache::IsNotFound(const string &  cookie)
{
    if (m_ExpirationMs > 0) {
        auto &              counters = m_App->GetCounters();

        lock_guard<mutex>   guard(m_Lock);

        auto    it = m_Cache.find(cookie);
        if (it == m_Cache.cend()) {
            counters.Increment(nullptr, CPSGSCounters::ePSGS_MyNCBINotFoundCacheMiss);
            return false;
        }

        if (GetTimespanToNowMs(it->second) > m_ExpirationMs) {
            // Found but the item is here for too long
            counters.Increment(nullptr, CPSGSCounters::ePSGS_MyNCBINotFoundCacheMiss);
            m_Cache.erase(it);

            auto    lru_it = find(m_LRU.begin(), m_LRU.end(), cookie);
            if (lru_it != m_LRU.begin())
                m_LRU.erase(lru_it);

            return false;
        }

        // Found. Update LRU but do not update the time. The time is the
        // moment when an error was noticed.
        counters.Increment(nullptr, CPSGSCounters::ePSGS_MyNCBINotFoundCacheHit);

        auto lru_it = find(m_LRU.begin(), m_LRU.end(), cookie);
        if (lru_it != m_LRU.begin()) {
            m_LRU.erase(lru_it);
            m_LRU.push_front(cookie);
        }
        return true;
    }
    return false;
}


void CMyNCBINotFoundCache::AddNotFound(const string &  cookie)
{
    if (m_ExpirationMs > 0) {
        lock_guard<mutex>   guard(m_Lock);

        auto  find_it = m_Cache.find(cookie);
        if (find_it == m_Cache.end()) {
            // No cookie, add a new one
            m_Cache[cookie] = psg_clock_t::now();
            m_LRU.push_front(cookie);
            return;
        }

        // It already exists
        find_it->second = psg_clock_t::now();

        auto lru_it = find(m_LRU.begin(), m_LRU.end(), cookie);
        if (lru_it != m_LRU.begin()) {
            m_LRU.erase(lru_it);
            m_LRU.push_front(cookie);
        }
    }
}


void CMyNCBINotFoundCache::Maintain(void)
{
    if (m_ExpirationMs > 0) {
        lock_guard<mutex>   guard(m_Lock);

        if (m_Cache.size() <= m_HighMark)
            return;

        while (m_Cache.size() > m_LowMark) {
            m_Cache.erase(m_Cache.find(m_LRU.back()));
            m_LRU.pop_back();
        }
    }
}


optional<SMyNCBIErrorCacheItem>
CMyNCBIErrorCache::GetError(const string &  cookie)
{
    optional<SMyNCBIErrorCacheItem>     ret;
    if (m_BackOffMs > 0) {
        auto &              counters = m_App->GetCounters();

        lock_guard<mutex>   guard(m_Lock);

        auto    it = m_Cache.find(cookie);
        if (it == m_Cache.cend()) {
            counters.Increment(nullptr, CPSGSCounters::ePSGS_MyNCBIErrorCacheMiss);
            return ret;
        }

        if (GetTimespanToNowMs(it->second.m_LastTouch) > m_BackOffMs) {
            // Found but the item is here for too long
            counters.Increment(nullptr, CPSGSCounters::ePSGS_MyNCBIErrorCacheMiss);
            m_Cache.erase(it);

            auto    lru_it = find(m_LRU.begin(), m_LRU.end(), cookie);
            if (lru_it != m_LRU.begin())
                m_LRU.erase(lru_it);

            return ret;
        }

        // Found. Update LRU but do not update the time. The time is the
        // moment when an error was noticed.
        counters.Increment(nullptr, CPSGSCounters::ePSGS_MyNCBIErrorCacheHit);

        auto lru_it = find(m_LRU.begin(), m_LRU.end(), cookie);
        if (lru_it != m_LRU.begin()) {
            m_LRU.erase(lru_it);
            m_LRU.push_front(cookie);
        }
        ret = it->second;
        return ret;
    }
    return ret;
}


void CMyNCBIErrorCache::AddError(const string &  cookie,
                                 CRequestStatus::ECode  status,
                                 int  code,
                                 EDiagSev  severity,
                                 const string &  message)
{
    if (m_BackOffMs > 0) {
        lock_guard<mutex>   guard(m_Lock);

        auto  find_it = m_Cache.find(cookie);
        if (find_it == m_Cache.end()) {
            // No cookie, add a new one
            m_Cache[cookie] = SMyNCBIErrorCacheItem(status, code, severity, message);
            m_LRU.push_front(cookie);
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
}


void CMyNCBIErrorCache::Maintain(void)
{
    if (m_BackOffMs > 0) {
        lock_guard<mutex>   guard(m_Lock);

        if (m_Cache.size() <= m_HighMark)
            return;

        while (m_Cache.size() > m_LowMark) {
            m_Cache.erase(m_Cache.find(m_LRU.back()));
            m_LRU.pop_back();
        }
    }
}

