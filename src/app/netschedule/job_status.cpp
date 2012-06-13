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
 * Authors:  Anatoliy Kuznetsov, Victor Joukov
 *
 * File Description: Network scheduler job status store
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbi_system.hpp>

#include <connect/services/netschedule_api.hpp>
#include <util/bitset/bmalgo.h>

#include "job_status.hpp"
#include "ns_gc_registry.hpp"


BEGIN_NCBI_SCOPE


CJobStatusTracker::CJobStatusTracker()
 : m_DoneCnt(0)
{
    // Note: one bit vector is not used - the corresponding job state became
    // obsolete and was deleted. The matrix though uses job statuses as indexes
    // for fast access, so that missed status vector is also created. The rest
    // of the code iterates only through the valid states.
    for (int i = 0; i < CNetScheduleAPI::eLastStatus; ++i) {
        m_StatusStor.push_back(new TNSBitVector(bm::BM_GAP));
    }
}


CJobStatusTracker::~CJobStatusTracker()
{
    for (int i = 0; i < CNetScheduleAPI::eLastStatus; ++i) {
        delete m_StatusStor[i];
    }
}


TJobStatus CJobStatusTracker::GetStatus(unsigned job_id) const
{
    CReadLockGuard      guard(m_Lock);

    return x_GetStatusNoLock(job_id);
}


TJobStatus CJobStatusTracker::x_GetStatusNoLock(unsigned job_id) const
{
    for (size_t  k = 0; k < g_ValidJobStatusesSize; ++k) {
        const TNSBitVector &    bv = *m_StatusStor[g_ValidJobStatuses[k]];

        if (bv[job_id])
            return g_ValidJobStatuses[k];
    }
    return CNetScheduleAPI::eJobNotFound;
}


unsigned CJobStatusTracker::CountStatus(TJobStatus status) const
{
    CReadLockGuard      guard(m_Lock);

    return m_StatusStor[(int)status]->count();
}


unsigned int
CJobStatusTracker::CountStatus(const vector<CNetScheduleAPI::EJobStatus> &
                                                              statuses) const
{
    unsigned int    cnt = 0;
    CReadLockGuard  guard(m_Lock);

    for (vector<CNetScheduleAPI::EJobStatus>::const_iterator  k = statuses.begin();
         k != statuses.end(); ++k)
        cnt += m_StatusStor[(int)(*k)]->count();

    return cnt;
}


unsigned int  CJobStatusTracker::Count(void) const
{
    unsigned int    cnt = 0;
    CReadLockGuard  guard(m_Lock);

    for (size_t  k = 0; k < g_ValidJobStatusesSize; ++k)
        cnt += m_StatusStor[g_ValidJobStatuses[k]]->count();

    return cnt;
}


bool  CJobStatusTracker::AnyJobs(void) const
{
    CReadLockGuard  guard(m_Lock);

    for (size_t  k = 0; k < g_ValidJobStatusesSize; ++k)
        if (m_StatusStor[g_ValidJobStatuses[k]]->any())
            return true;
    return false;
}


bool  CJobStatusTracker::AnyJobs(TJobStatus  status) const
{
    CReadLockGuard      guard(m_Lock);

    return m_StatusStor[(int)status]->any();
}


bool  CJobStatusTracker::AnyJobs(
                const vector<CNetScheduleAPI::EJobStatus> &  statuses) const
{
    CReadLockGuard  guard(m_Lock);

    for (vector<CNetScheduleAPI::EJobStatus>::const_iterator  k = statuses.begin();
         k != statuses.end(); ++k)
        if (m_StatusStor[(int)(*k)]->any())
            return true;

    return false;
}



void CJobStatusTracker::StatusStatistics(TJobStatus                  status,
                                         TNSBitVector::statistics *  st) const
{
    _ASSERT(st);
    CReadLockGuard          guard(m_Lock);
    const TNSBitVector &    bv = *m_StatusStor[(int)status];

    bv.calc_stat(st);
}


void CJobStatusTracker::StatusSnapshot(TJobStatus      status,
                                       TNSBitVector *  bv) const
{
    _ASSERT(bv);
    CReadLockGuard          guard(m_Lock);
    const TNSBitVector &    bv_s = *m_StatusStor[(int)status];

    *bv |= bv_s;
}


void CJobStatusTracker::SetStatus(unsigned    job_id,
                                  TJobStatus  status)
{
    TJobStatus              old_status = CNetScheduleAPI::eJobNotFound;
    CWriteLockGuard         guard(m_Lock);

    for (size_t  k = 0; k < g_ValidJobStatusesSize; ++k) {
        TNSBitVector &      bv = *m_StatusStor[g_ValidJobStatuses[k]];

        if (bv[job_id]) {
            if (old_status != CNetScheduleAPI::eJobNotFound)
                NCBI_THROW(CNetScheduleException, eInternalError,
                           "State matrix was damaged, more than one status "
                           "active for job " + NStr::UIntToString(job_id));
            old_status = g_ValidJobStatuses[k];

            if (status != g_ValidJobStatuses[k])
                bv.set_bit(job_id, false);
        } else {
            if (status == g_ValidJobStatuses[k])
                bv.set_bit(job_id, true);
        }
    }
}


void CJobStatusTracker::AddPendingJob(unsigned int  job_id)
{
    CWriteLockGuard         guard(m_Lock);
    m_StatusStor[(int) CNetScheduleAPI::ePending]->set_bit(job_id, true);
}


void CJobStatusTracker::Erase(unsigned  job_id)
{
    SetStatus(job_id, CNetScheduleAPI::eJobNotFound);
}


void CJobStatusTracker::ClearAll(TNSBitVector *  bv)
{
    CWriteLockGuard         guard(m_Lock);

    for (size_t  k = 0; k < g_ValidJobStatusesSize; ++k) {
        TNSBitVector &      bv1 = *m_StatusStor[g_ValidJobStatuses[k]];

        if (bv)
            *bv |= bv1;

        bv1.clear(true);
    }
}


void CJobStatusTracker::OptimizeMem()
{
    for (size_t  k = 0; k < g_ValidJobStatusesSize; ++k) {
        TNSBitVector &      bv = *m_StatusStor[g_ValidJobStatuses[k]];
        {{
            CWriteLockGuard     guard(m_Lock);
            bv.optimize(0, TNSBitVector::opt_free_0);
        }}
    }
}


void CJobStatusTracker::SetExactStatusNoLock(unsigned   job_id,
                                             TJobStatus status,
                                             bool       set_clear)
{
    TNSBitVector &      bv = *m_StatusStor[(int)status];
    bv.set(job_id, set_clear);
}


void CJobStatusTracker::AddPendingBatch(unsigned  job_id_from,
                                        unsigned  job_id_to)
{
    CWriteLockGuard     guard(m_Lock);
    m_StatusStor[(int) CNetScheduleAPI::ePending]->set_range(job_id_from,
                                                             job_id_to);
}


unsigned int
CJobStatusTracker::GetPendingJobFromSet(const TNSBitVector &  candidate_set)
{
    TNSBitVector &              bv = *m_StatusStor[(int) CNetScheduleAPI::ePending];
    TNSBitVector::enumerator    en(candidate_set.first());
    CReadLockGuard              guard(m_Lock);

    for (; en.valid(); ++en) {
        unsigned int    id = *en;

        if (bv[id])
            // The candidate is pending
            return id;
    }

    // No one matches
    return 0;
}


unsigned int
CJobStatusTracker::GetJobByStatus(TJobStatus            status,
                                  const TNSBitVector &  unwanted_jobs,
                                  const TNSBitVector &  group_jobs) const
{
    TNSBitVector &      bv = *m_StatusStor[(int)status];
    CReadLockGuard      guard(m_Lock);

    if (!bv.any())
        return 0;

    TNSBitVector        candidates(bv);

    candidates -= unwanted_jobs;
    if (group_jobs.any())
        // Group restriction is provided
        candidates &= group_jobs;

    if (!candidates.any())
        return 0;

    return *candidates.first();
}


unsigned int
CJobStatusTracker::GetJobByStatus(TJobStatus            status,
                                  const TNSBitVector &  unwanted_jobs) const
{
    TNSBitVector &      bv = *m_StatusStor[(int)status];
    CReadLockGuard      guard(m_Lock);

    if (!bv.any())
        return 0;

    if (unwanted_jobs.any()) {
        TNSBitVector        candidates(bv);
        candidates -= unwanted_jobs;
        if (!candidates.any())
            return 0;
        return *candidates.first();
    }

    return *bv.first();
}


TNSBitVector
CJobStatusTracker::GetJobs(const vector<CNetScheduleAPI::EJobStatus> &  statuses) const
{
    TNSBitVector        jobs;
    CReadLockGuard      guard(m_Lock);

    for (vector<CNetScheduleAPI::EJobStatus>::const_iterator  k = statuses.begin();
         k != statuses.end(); ++k)
        jobs |= *m_StatusStor[(int)(*k)];

    return jobs;
}


TNSBitVector
CJobStatusTracker::GetJobs(CNetScheduleAPI::EJobStatus  status) const
{
    CReadLockGuard      guard(m_Lock);
    return *m_StatusStor[(int)status];
}


TNSBitVector
CJobStatusTracker::GetOutdatedPendingJobs(CNSPreciseTime          timeout,
                                          const CJobGCRegistry &  gc_registry) const
{
    static CNSPreciseTime   s_LastTimeout = CNSPreciseTime();
    static unsigned int     s_LastCheckedJobID = 0;
    static const size_t     kMaxCandidates = 100;

    TNSBitVector            result;

    if (timeout.tv_sec == 0 && timeout.tv_nsec == 0)
        return result;  // Not configured

    size_t                  count = 0;
    const TNSBitVector &    pending_jobs = *m_StatusStor[(int) CNetScheduleAPI::ePending];
    CNSPreciseTime          limit = CNSPreciseTime::Current() - timeout;
    CReadLockGuard          guard(m_Lock);

    if (s_LastTimeout != timeout) {
        s_LastTimeout = timeout;
        s_LastCheckedJobID = 0;
    }

    TNSBitVector::enumerator    en(pending_jobs.first());
    for (; en.valid() && count < kMaxCandidates; ++en, ++count) {
        unsigned int    job_id = *en;
        if (job_id <= s_LastCheckedJobID) {
            result.set_bit(job_id, true);
            continue;
        }
        if (gc_registry.GetPreciseSubmitTime(job_id) < limit) {
            s_LastCheckedJobID = job_id;
            result.set_bit(job_id, true);
            continue;
        }

        // No more old jobs
        break;
    }

    return result;
}


void CJobStatusTracker::PendingIntersect(TNSBitVector* candidate_set) const
{
    CReadLockGuard      guard(m_Lock);
    TNSBitVector &      bv = *m_StatusStor[(int) CNetScheduleAPI::ePending];

    *candidate_set &= bv;
}


void CJobStatusTracker::GetAliveJobs(TNSBitVector& ids)
{
    CReadLockGuard      guard(m_Lock);

    for (size_t  k = 0; k < g_ValidJobStatusesSize; ++k) {
        TNSBitVector &  bv = *m_StatusStor[g_ValidJobStatuses[k]];
        ids |= bv;
    }
}


bool CJobStatusTracker::AnyPending() const
{
    const TNSBitVector &    bv = *m_StatusStor[(int) CNetScheduleAPI::ePending];
    CReadLockGuard          guard(m_Lock);

    return bv.any();
}


unsigned CJobStatusTracker::GetNext(TJobStatus status, unsigned job_id) const
{
    const TNSBitVector &    bv = *m_StatusStor[(int)status];
    CReadLockGuard          guard(m_Lock);

    return bv.get_next(job_id);
}


void CJobStatusTracker::x_SetClearStatusNoLock(unsigned   job_id,
                                               TJobStatus status,
                                               TJobStatus old_status)
{
    SetExactStatusNoLock(job_id, status, true);
    SetExactStatusNoLock(job_id, old_status, false);
}


void CJobStatusTracker::x_ReportInvalidStatus(unsigned   job_id,
                                              TJobStatus status,
                                              TJobStatus old_status)
{
    NCBI_THROW(CNetScheduleException, eInvalidJobStatus,
               "Invalid change job status from " +
                CNetScheduleAPI::StatusToString(old_status) +
                " to " +
                CNetScheduleAPI::StatusToString(status));
}


TJobStatus CJobStatusTracker::x_IsStatusNoLock(unsigned job_id,
                                              TJobStatus st1,
                                              TJobStatus st2,
                                              TJobStatus st3) const
{
    if (st1 == CNetScheduleAPI::eJobNotFound)
        return CNetScheduleAPI::eJobNotFound;

    const TNSBitVector&     bv_st1 = *m_StatusStor[(int)st1];
    if (bv_st1[job_id])
        return st1;


    if (st2 == CNetScheduleAPI::eJobNotFound)
        return CNetScheduleAPI::eJobNotFound;

    const TNSBitVector&     bv_st2 = *m_StatusStor[(int)st2];
    if (bv_st2[job_id])
        return st2;


    if (st3 == CNetScheduleAPI::eJobNotFound)
        return CNetScheduleAPI::eJobNotFound;

    const TNSBitVector&     bv_st3 = *m_StatusStor[(int)st3];
    if (bv_st3[job_id])
        return st3;

    return CNetScheduleAPI::eJobNotFound;
}


void CJobStatusTracker::x_IncDoneJobs(void)
{
    ++m_DoneCnt;
    if (m_DoneCnt == 65535 * 2) {
        m_DoneCnt = 0;
        {{
            TNSBitVector& bv = *m_StatusStor[(int) CNetScheduleAPI::eDone];
            bv.optimize(0, TNSBitVector::opt_free_01);
        }}
        {{
            TNSBitVector& bv = *m_StatusStor[(int) CNetScheduleAPI::ePending];
            bv.optimize(0, TNSBitVector::opt_free_0);
        }}
    }
}

END_NCBI_SCOPE

