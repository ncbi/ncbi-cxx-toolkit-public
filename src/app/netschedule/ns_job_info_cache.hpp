#ifndef NETSCHEDULE_JOB_INFO_CACHE__HPP
#define NETSCHEDULE_JOB_INFO_CACHE__HPP

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


#include <corelib/ncbistl.hpp>
#include <corelib/ncbimtx.hpp>

#include <map>
#include <string>

#include "ns_types.hpp"


BEGIN_NCBI_SCOPE

class CJobStatusTracker;
class CStatisticsCounters;


// Some often used job information. Notably WST2 uses it.
struct SJobCachedInfo
{
    string      m_ClientIP;
    string      m_ClientSID;
    string      m_NCBIPHID;
};


// Specialized job info cache. It holds the job information which does not
// change since the moment the job is created.
class CJobInfoCache
{
    public:
        CJobInfoCache(const CJobStatusTracker &  status_tracker,
                      size_t                     limit,
                      CStatisticsCounters &      statistics);

    public:
        void SetLimit(size_t  limit);
        bool IsInCleaning(void) const;
        bool GetJobCachedInfo(unsigned int  job_id, SJobCachedInfo &  info);
        void SetJobCachedInfo(unsigned int  job_id,
                              const SJobCachedInfo &  info);
        void SetJobCachedInfo(unsigned int    job_id,
                              const string &  client_ip,
                              const string &  client_sid,
                              const string &  client_phid);
        void RemoveJobCachedInfo(unsigned int  job_id);
        void RemoveJobCachedInfo(const TNSBitVector &  jobs);
        size_t GetSize(void) const;
        unsigned int Purge(void);

    private:
        const CJobStatusTracker &           m_StatusTracker;
        CStatisticsCounters &               m_StatisticsCounters;

    private:
        typedef map<unsigned int,
                    SJobCachedInfo>         TCachedInfo;

        size_t              m_Limit;    // Max number of cache elements before
                                        // removing some. 0 - unlimited size.
        TCachedInfo         m_Cache;
        mutable CFastMutex  m_Lock;
        bool                m_Cleaning; // true => a service thread is cleaning
                                        // the cache

    private:
        unsigned int x_RemoveJobsInStates(const TNSBitVector &        all_jobs,
                                          const vector<TJobStatus> &  statuses);
};

END_NCBI_SCOPE

#endif /* NETSCHEDULE_JOB_INFO_CACHE__HPP */


