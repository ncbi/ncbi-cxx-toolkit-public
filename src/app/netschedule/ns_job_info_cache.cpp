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
 * Authors:  Sergey Satskiy
 *
 * File Description:
 *   Net schedule job info cache targeting the read only info used in often
 *   received WST2 command.
 *
 */

#include <ncbi_pch.hpp>

#include "ns_job_info_cache.hpp"
#include "job_status.hpp"
#include "ns_statistics_counters.hpp"
#include "ns_types.hpp"



BEGIN_NCBI_SCOPE


CJobInfoCache::CJobInfoCache(const CJobStatusTracker &  status_tracker,
                             size_t                     limit,
                             CStatisticsCounters &      statistics) :
    m_StatusTracker(status_tracker),
    m_StatisticsCounters(statistics),
    m_Limit(limit)
{}


void CJobInfoCache::SetLimit(size_t  limit)
{
    // It is extremely rare if the limit is changed. It may happened
    // via reconfiguring on the fly only. So it is safer to grab the lock
    // to avoid any kind of potential problems due to non-atomic updating.
    CFastMutexGuard     guard(m_Lock);
    limit = m_Limit;
}


bool CJobInfoCache::IsInCleaning(void) const
{
    // There is intentionally no lock here.
    // Here are the considerations:
    // - the only entity which updates the variable is a service thread
    // - most probably it is atomic to update a boolean variable
    // - the worst scenario -- described below -- is rather rear and even so
    //   it is harmless in terms of breaking data structures

    // There are basically two scenarios which could be of concern.
    // 1. A WST2 gets this value as true while the very next moment it is
    //    updated by the cleaning thread to false. In this case WST2 will go to
    //    the database as it did in all the previous versions of NetSchedule.
    //    Thus, there is no harm except of a potential performance penalty.
    // 2. A WST2 gets this value as false while the very next moment it is
    //    updated by the cleaning thread to true. In this case WST2 will call
    //    the GetJobCachedInfo() method and will wait on a mutex till the
    //    cleaning thread is finished. The cleaning procedure tends to be not
    //    CPU intensive so the wait time promises to be short. There will be no
    //    harm of the data structure but a performance penalty.
    // Overall, the problematic cases are rare as the cleaning thread is waking
    // up not often. And in most of the cases the performance will be increased
    // because it is a way faster to get the information from the cache in
    // comparison to going to the BDB table.
    return m_Cleaning;
}

bool CJobInfoCache::GetJobCachedInfo(unsigned int       job_id,
                                     SJobCachedInfo &   info)
{
    CFastMutexGuard                 guard(m_Lock);
    TCachedInfo::const_iterator     found = m_Cache.find(job_id);

    if (found == m_Cache.end()) {
        m_StatisticsCounters.CountJobInfoCacheMiss(1);
        return false;
    }

    m_StatisticsCounters.CountJobInfoCacheHit(1);
    info = found->second;
    return true;
}

void CJobInfoCache::SetJobCachedInfo(unsigned int             job_id,
                                     const SJobCachedInfo &   info)
{
    CFastMutexGuard     guard(m_Lock);
    m_Cache[job_id] = info;
}

void CJobInfoCache::SetJobCachedInfo(unsigned int  job_id,
                                     const string &  client_ip,
                                     const string &  client_sid,
                                     const string &  client_phid)
{
    CFastMutexGuard     guard(m_Lock);
    pair<TCachedInfo::iterator,
         bool>          item = m_Cache.insert(
                                        pair<unsigned int,
                                             SJobCachedInfo>(job_id,
                                                             SJobCachedInfo()));
    item.first->second.m_ClientIP = client_ip;
    item.first->second.m_ClientSID = client_sid;
    item.first->second.m_NCBIPHID = client_phid;
}

void CJobInfoCache::RemoveJobCachedInfo(unsigned int  job_id)
{
    CFastMutexGuard     guard(m_Lock);
    m_Cache.erase(job_id);
}

void CJobInfoCache::RemoveJobCachedInfo(const TNSBitVector &  jobs)
{
    CFastMutexGuard             guard(m_Lock);
    TNSBitVector::enumerator    en(jobs.first());
    for (; en.valid(); ++en)
        m_Cache.erase(*en);
}

size_t CJobInfoCache::GetSize(void) const
{
    CFastMutexGuard     guard(m_Lock);
    return m_Cache.size();
}

unsigned int CJobInfoCache::Purge(void)
{
    CFastMutexGuard     guard(m_Lock);
    if (m_Limit == 0)
        return 0;
    if (m_Cache.size() <= m_Limit)
        return 0;

    unsigned int    count = 0;
    unsigned int    need_to_remove = m_Cache.size() - m_Limit;
    TNSBitVector    all_jobs;

    for (TCachedInfo::const_iterator  k = m_Cache.begin();
         k != m_Cache.end(); ++k)
        all_jobs.set_bit(k->first);

    vector<TJobStatus>  statuses;

    // Delete jobs in Confirmed, ReadFailed states
    statuses.push_back(CNetScheduleAPI::eConfirmed);
    statuses.push_back(CNetScheduleAPI::eReadFailed);
    count += x_RemoveJobsInStates(all_jobs, statuses);

    // Delete jobs in Canceled, Failed states
    if (count < need_to_remove) {
        statuses.clear();
        statuses.push_back(CNetScheduleAPI::eCanceled);
        statuses.push_back(CNetScheduleAPI::eFailed);
        count += x_RemoveJobsInStates(all_jobs, statuses);
    }

    // Delete jobs in Done, Reading states
    if (count < need_to_remove) {
        statuses.clear();
        statuses.push_back(CNetScheduleAPI::eDone);
        statuses.push_back(CNetScheduleAPI::eReading);
        count += x_RemoveJobsInStates(all_jobs, statuses);
    }

    // Delete jobs in Pending states
    if (count < need_to_remove) {
        statuses.clear();
        statuses.push_back(CNetScheduleAPI::ePending);
        count += x_RemoveJobsInStates(all_jobs, statuses);
    }

    // Delete some jobs
    while (count < need_to_remove) {
        TCachedInfo::iterator   f = m_Cache.begin();
        if (f == m_Cache.end())
            break;
        m_Cache.erase(f);
        count += 1;
    }

    m_StatisticsCounters.CountJobInfoCacheGCRemoved(count);
    return count;
}

unsigned int
CJobInfoCache::x_RemoveJobsInStates(const TNSBitVector &        all_jobs,
                                    const vector<TJobStatus> &  statuses)
{
    TNSBitVector        jobs_in_state = m_StatusTracker.GetJobs(statuses);
    TNSBitVector        candidates = all_jobs & jobs_in_state;
    unsigned int        count = candidates.count();

    if (count > 0) {
        TNSBitVector::enumerator    en(candidates.first());
        for (; en.valid(); ++en)
            m_Cache.erase(*en);
    }
    return count;
}


END_NCBI_SCOPE

