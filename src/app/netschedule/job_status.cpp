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

BEGIN_NCBI_SCOPE


CJobStatusTracker::CJobStatusTracker()
 : m_LastPending(0),
   m_DoneCnt(0)
{
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
    for (int i = 0; i < CNetScheduleAPI::eLastStatus; ++i) {
        const TNSBitVector &    bv = *m_StatusStor[i];

        if (bv[job_id])
            return TJobStatus(i);
    }
    return CNetScheduleAPI::eJobNotFound;
}


unsigned CJobStatusTracker::CountStatus(TJobStatus status) const
{
    CReadLockGuard      guard(m_Lock);

    return m_StatusStor[(int)status]->count();
}



static  CNetScheduleAPI::EJobStatus
            s_StatToCount[] = { CNetScheduleAPI::ePending,
                                CNetScheduleAPI::eRunning,
                                CNetScheduleAPI::eCanceled,
                                CNetScheduleAPI::eFailed,
                                CNetScheduleAPI::eDone,
                                CNetScheduleAPI::eReading,
                                CNetScheduleAPI::eConfirmed,
                                CNetScheduleAPI::eReadFailed };
static size_t
            s_StatToCountSize = sizeof(s_StatToCount) /
                                sizeof(CNetScheduleAPI::EJobStatus);

void CJobStatusTracker::CountStatus(TStatusSummaryMap *     status_map,
                                    const TNSBitVector *    candidate_set)
{
    _ASSERT(status_map);
    status_map->clear();

    CReadLockGuard      guard(m_Lock);

    for (size_t  k = 0; k < s_StatToCountSize; ++k) {
        const TNSBitVector &    bv = *m_StatusStor[s_StatToCount[k]];
        unsigned                cnt;

        if (candidate_set)
            cnt = bm::count_and(bv, *candidate_set);
        else
            cnt = bv.count();

        (*status_map)[s_StatToCount[k]] = cnt;
    }
}


unsigned CJobStatusTracker::Count(void)
{
    CReadLockGuard guard(m_Lock);

    unsigned        cnt = 0;
    for (int i = 0; i < CNetScheduleAPI::eLastStatus; ++i) {
        const TNSBitVector &    bv = *m_StatusStor[i];

        cnt += bv.count();
    }
    return cnt;
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
    CWriteLockGuard         guard(m_Lock);
    TJobStatus              old_status = TJobStatus(-1);

    for (int i = 0; i < CNetScheduleAPI::eLastStatus; ++i) {
        TNSBitVector &      bv = *m_StatusStor[i];

        if (bv[job_id]) {
            if (old_status != TJobStatus(-1))
                ERR_POST("State matrix was damaged, "
                         "more than one status active for job " << job_id);
            old_status = TJobStatus(i);

            if ((int)status != (int)i)
                bv.set(job_id, false);
        } else {
            if ((int)status == (int)i)
                bv.set(job_id, true);
        }
    }

    if (old_status == CNetScheduleAPI::eDone &&
        status     == CNetScheduleAPI::ePending)
        ERR_POST("Illegal status change from Done to Pending for job " <<
                 job_id);

    if (status == CNetScheduleAPI::eDone)
        IncDoneJobs();
}


void CJobStatusTracker::Erase(unsigned  job_id)
{
    SetStatus(job_id, CNetScheduleAPI::eJobNotFound);
}


void CJobStatusTracker::ClearAll(TNSBitVector *  bv)
{
    CWriteLockGuard         guard(m_Lock);

    for (int i = 0; i < CNetScheduleAPI::eLastStatus; ++i) {
        TNSBitVector &      bv1 = *m_StatusStor[i];

        if (bv)
            *bv |= bv1;

        bv1.clear(true);
    }
}


void CJobStatusTracker::OptimizeMem()
{
    for (int i = 0; i < CNetScheduleAPI::eLastStatus; ++i) {
        TNSBitVector &      bv = *m_StatusStor[i];
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
    if ((status == CNetScheduleAPI::ePending) &&
        (job_id < m_LastPending)) {
        m_LastPending = job_id - 1;
    }
}


TJobStatus CJobStatusTracker::ChangeStatus(unsigned   job_id,
                                           TJobStatus status,
                                           bool*      updated)
{
    CWriteLockGuard     guard(m_Lock);
    bool                status_updated = false;
    TJobStatus          old_status = CNetScheduleAPI::eJobNotFound;

    switch (status) {

    case CNetScheduleAPI::ePending:
        old_status = x_GetStatusNoLock(job_id);
        if (old_status == CNetScheduleAPI::eJobNotFound) { // new job
            SetExactStatusNoLock(job_id, status, true);
            status_updated = true;
            break;
        }
        if (old_status == CNetScheduleAPI::eRunning) {
            x_SetClearStatusNoLock(job_id, status, old_status);
            status_updated = true;
            break;
        }

        ReportInvalidStatus(job_id, status, old_status);
        break;

    case CNetScheduleAPI::eRunning:
        old_status = x_GetStatusNoLock(job_id);
        if (old_status == CNetScheduleAPI::ePending) {
            x_SetClearStatusNoLock(job_id, status, old_status);
            status_updated = true;
            break;
        }

        ReportInvalidStatus(job_id, status, old_status);
        break;

    case CNetScheduleAPI::eCanceled:
        old_status = x_GetStatusNoLock(job_id);
        if (old_status == CNetScheduleAPI::eCanceled) {
            // There is nothing to do, the job has already been canceled
            break;
        }
        if (old_status != CNetScheduleAPI::eJobNotFound) {
            x_SetClearStatusNoLock(job_id, status, old_status);
            status_updated = true;
            break;
        }

        // Job not found - do nothing
        break;

    case CNetScheduleAPI::eFailed:
        old_status = IsStatusNoLock(job_id,
                                    CNetScheduleAPI::eRunning);
        if (old_status != CNetScheduleAPI::eJobNotFound) {
            x_SetClearStatusNoLock(job_id, status, old_status);
            status_updated = true;
            break;
        }

        old_status = CNetScheduleAPI::eFailed;
        break;

    case CNetScheduleAPI::eDone:
        old_status = IsStatusNoLock(job_id,
                                    CNetScheduleAPI::eRunning,
                                    CNetScheduleAPI::ePending);
        if (old_status != CNetScheduleAPI::eJobNotFound) {
            x_SetClearStatusNoLock(job_id, status, old_status);
            status_updated = true;
            IncDoneJobs();
            break;
        }

        old_status = IsStatusNoLock(job_id,
                                    CNetScheduleAPI::eReading);
        if (old_status != CNetScheduleAPI::eJobNotFound) {
            x_SetClearStatusNoLock(job_id, status, old_status);
            status_updated = true;
            break;
        }

        // For all other states the job results are just ignored
        old_status = x_GetStatusNoLock(job_id);
        break;

    case CNetScheduleAPI::eReading:
        old_status = x_GetStatusNoLock(job_id);
        if (old_status == CNetScheduleAPI::eDone) {
            x_SetClearStatusNoLock(job_id, status, old_status);
            status_updated = true;
            break;
        }

        ReportInvalidStatus(job_id, status, old_status);
        break;

    case CNetScheduleAPI::eReadFailed:
        old_status = IsStatusNoLock(job_id,
                                    CNetScheduleAPI::eReading);
        if (old_status != CNetScheduleAPI::eJobNotFound) {
            x_SetClearStatusNoLock(job_id, status, old_status);
            status_updated = true;
            break;
        }

        old_status = CNetScheduleAPI::eReadFailed;
        break;

    case CNetScheduleAPI::eConfirmed:
        old_status = x_GetStatusNoLock(job_id);
        if (old_status == CNetScheduleAPI::eConfirmed) {
            // There is nothing to do, the job has already been confirmed
            break;
        }
        if (old_status == CNetScheduleAPI::eReading) {
            x_SetClearStatusNoLock(job_id, status, old_status);
            status_updated = true;
            break;
        }

        ReportInvalidStatus(job_id, status, old_status);
        break;

    default:
        ERR_POST("Unexpected target state: " << (int)status);
        _ASSERT(0);
    }

    if (updated)
        *updated = status_updated;

    return old_status;
}


void CJobStatusTracker::AddPendingBatch(unsigned  job_id_from,
                                        unsigned  job_id_to)
{
    CWriteLockGuard     guard(m_Lock);
    TNSBitVector &      bv = *m_StatusStor[(int) CNetScheduleAPI::ePending];

    bv.set_range(job_id_from, job_id_to);
}


unsigned int
CJobStatusTracker::GetPendingJobFromSet(const TNSBitVector &  candidate_set)
{
    CReadLockGuard              guard(m_Lock);
    TNSBitVector &              bv = *m_StatusStor[(int) CNetScheduleAPI::ePending];
    TNSBitVector::enumerator    en(candidate_set.first());

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
                                  const TNSBitVector &  unwanted_jobs) const
{
    CReadLockGuard      guard(m_Lock);
    TNSBitVector &      bv = *m_StatusStor[(int)status];

    if (!bv.any())
        return 0;

    TNSBitVector        bv_pending(bv);

    bv_pending -= unwanted_jobs;

    TNSBitVector::enumerator    en(bv_pending.first());
    if (!en.valid())
        return 0;
    return *en;
}


void CJobStatusTracker::PendingIntersect(TNSBitVector* candidate_set) const
{
    CReadLockGuard      guard(m_Lock);
    TNSBitVector &      bv = *m_StatusStor[(int) CNetScheduleAPI::ePending];

    *candidate_set &= bv;
}


void CJobStatusTracker::SwitchJobs(unsigned count,
                                   TJobStatus old_status, TJobStatus new_status,
                                   TNSBitVector& jobs,
                                   const TNSBitVector* unwanted_jobs)
{
    CReadLockGuard              guard(m_Lock);
    TNSBitVector &              bv_old = *m_StatusStor[(int) old_status];
    TNSBitVector &              bv_new = *m_StatusStor[(int) new_status];
    TNSBitVector::enumerator    en(bv_old.first());

    for (unsigned n = 0; en.valid() && n < count; ++en) {
        unsigned    id = *en;

        if (unwanted_jobs && (*unwanted_jobs)[id])
            continue;

        jobs.set(id);
        ++n;
    }
    bv_old -= jobs;
    bv_new |= jobs;
}


void CJobStatusTracker::GetAliveJobs(TNSBitVector& ids)
{
    CReadLockGuard      guard(m_Lock);

    for (int i = 0; i < CNetScheduleAPI::eLastStatus; ++i) {
        TNSBitVector& bv = *m_StatusStor[i];
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


void CJobStatusTracker::ReportInvalidStatus(unsigned   job_id,
                                            TJobStatus status,
                                            TJobStatus old_status)
{
    NCBI_THROW(CNetScheduleException, eInvalidJobStatus,
               "Invalid change job status from " +
                CNetScheduleAPI::StatusToString(old_status) +
                " to " +
                CNetScheduleAPI::StatusToString(status));
}


TJobStatus CJobStatusTracker::IsStatusNoLock(unsigned job_id,
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


void CJobStatusTracker::IncDoneJobs()
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

