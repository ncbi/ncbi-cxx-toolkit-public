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


void CJobStatusTracker::CountStatus(TStatusSummaryMap *     status_map,
                                    const TNSBitVector *    candidate_set)
{
    _ASSERT(status_map);
    status_map->clear();

    CReadLockGuard      guard(m_Lock);

    for (int i = 0; i < CNetScheduleAPI::eLastStatus; ++i) {
        const TNSBitVector &    bv = *m_StatusStor[i];
        unsigned                cnt;

        if (candidate_set)
            cnt = bm::count_and(bv, *candidate_set);
        else
            cnt = bv.count();

        (*status_map)[(TJobStatus) i] = cnt;
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
        old_status = IsStatusNoLock(job_id,
                                    CNetScheduleAPI::ePending,
                                    CNetScheduleAPI::eCanceled);
        if (old_status != CNetScheduleAPI::eJobNotFound) {
            if (IsCancelCode(old_status)) {
                break;
            }
            x_SetClearStatusNoLock(job_id, status, old_status);
            status_updated = true;
            break;
        }

        ReportInvalidStatus(job_id, status, old_status);
        break;

    case CNetScheduleAPI::eReturned:
        _ASSERT(0);
        break;

    case CNetScheduleAPI::eCanceled:
        old_status = IsStatusNoLock(job_id,
                                    CNetScheduleAPI::ePending,
                                    CNetScheduleAPI::eRunning);
        if (old_status != CNetScheduleAPI::eJobNotFound) {
            x_SetClearStatusNoLock(job_id, status, old_status);
            status_updated = true;
            break;
        }
        // in this case (failed, done) we just do nothing.
        old_status = CNetScheduleAPI::eCanceled;
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
                                    CNetScheduleAPI::eCanceled,
                                    CNetScheduleAPI::eFailed);
        if (IsCancelCode(old_status)) {
            break;
        }

        old_status = x_GetStatusNoLock(job_id);
        if (old_status == CNetScheduleAPI::eDone) {
            break;
        }

        ReportInvalidStatus(job_id, status, old_status);
        break;

    default:
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


bool CJobStatusTracker::GetPendingJobFromSet(TNSBitVector *  candidate_set,
                                             unsigned *      job_id)
{
    *job_id = 0;

    TNSBitVector &      bv = *m_StatusStor[(int) CNetScheduleAPI::ePending];
    unsigned            candidate_id = 0;

    while ( 0 == candidate_id ) {
        // STAGE 1: (read lock)
        // look for the first pending candidate bit
        {{
            CReadLockGuard              guard(m_Lock);
            TNSBitVector::enumerator    en(candidate_set->first());

            if (!en.valid()) 
                return bv.any();    // no more candidates

            for (; en.valid(); ++en) {
                unsigned        id = *en;
                if (bv[id]) { // check if candidate is pending
                    candidate_id = id;
                    break;
                }
            }
        }}

        // STAGE 2: (write lock)
        // candidate job goes to running status
        // set of candidates is corrected to reflect new disposition
        // (clear all non-pending candidates)
        //
        if (candidate_id) {
            CWriteLockGuard guard(m_Lock);
            // clean the candidate set, to reflect stage 1 scan
            candidate_set->set_range(0, candidate_id, false);
            if (bv[candidate_id]) { // still pending?
                x_SetClearStatusNoLock(candidate_id,
                                       CNetScheduleAPI::eRunning,
                                       CNetScheduleAPI::ePending);
                *job_id = candidate_id;
                return true;
            } else {
                // somebody picked up this id already
                candidate_id = 0;
            }
        } else {
            // previous step did not pick up a sutable(pending) candidate
            // candidate set is dismissed, pending search stopped
            candidate_set->clear(true); // clear with memfree
            break;
        }
    } // while

    CReadLockGuard guard(m_Lock);
    return bv.any();
}


bool CJobStatusTracker::GetPendingJob(const TNSBitVector& unwanted_jobs,
                                      unsigned*           job_id)
{
    *job_id = 0;

    CWriteLockGuard     guard(m_Lock);
    TNSBitVector&       bv = *m_StatusStor[(int) CNetScheduleAPI::ePending];

    if (!bv.any())
        return false;

    TNSBitVector        bv_pending(bv);

    bv_pending -= unwanted_jobs;
    TNSBitVector::enumerator en(bv_pending.first());
    if (!en.valid())
        return bv.any();    // no more candidates

    unsigned            candidate_id = *en;
    x_SetClearStatusNoLock(candidate_id,
                            CNetScheduleAPI::eRunning,
                            CNetScheduleAPI::ePending);
    *job_id = candidate_id;
    return true;
}


void CJobStatusTracker::PendingIntersect(TNSBitVector* candidate_set)
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


unsigned CJobStatusTracker::GetFirstDone() const
{
    return GetFirst(CNetScheduleAPI::eDone);
}


unsigned CJobStatusTracker::GetFirst(TJobStatus status) const
{
    const TNSBitVector &    bv = *m_StatusStor[(int)status];
    CReadLockGuard          guard(m_Lock);

    return bv.get_first();
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
               "Cannot change job status from " +
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

///////////////////////////////////////////////////////////////////////
// CNetSchedule_JSGroupGuard

CNetSchedule_JSGroupGuard::CNetSchedule_JSGroupGuard(
    CJobStatusTracker&  strack,
    TJobStatus          old_status,
    const TNSBitVector& jobs,
    TJobStatus          new_status)
    : m_Tracker(strack),
    m_Commited(false),
    m_OldStatus(old_status),
    m_Jobs(jobs)
{
    if (new_status != CNetScheduleAPI::eJobNotFound) {
        CReadLockGuard                      guard(m_Tracker.m_Lock);
        CJobStatusTracker::TStatusStorage   sstor = m_Tracker.m_StatusStor;

        for (int i = 0; i < CNetScheduleAPI::eLastStatus; ++i) {
            TNSBitVector &      bv = *sstor[i];

            if ((int) new_status == i)
                bv |= m_Jobs;
            else
                bv -= m_Jobs;
        }
    }
}

CNetSchedule_JSGroupGuard::~CNetSchedule_JSGroupGuard()
{
    if (m_Commited)
        return;

    CReadLockGuard                      guard(m_Tracker.m_Lock);
    CJobStatusTracker::TStatusStorage   sstor = m_Tracker.m_StatusStor;

    for (int i = 0; i < CNetScheduleAPI::eLastStatus; ++i) {
        TNSBitVector &      bv = *sstor[i];

        if ((int) m_OldStatus == i)
            bv |= m_Jobs;
        else
            bv -= m_Jobs;
    }
}


///////////////////////////////////////////////////////////////////////
// CJSGuard
/*
CJSGuard::CJSGuard(CJobStatusTracker& strack,
                   unsigned           job_id,
                   TJobStatus         status,
                   int                timeout_ms)
    : m_Tracker(strack),
    m_JobId(job_id),
    m_NewStatus(status)
{
    _ASSERT(job_id);
    unsigned cnt = 0; unsigned sleep_ms = 10;
    while (true) {
        {{
            CWriteLockGuard guard(m_Tracker.m_Lock);
            if (!m_Tracker.m_UsedIds[job_id]) {
                m_OldStatus = (TJobStatus)
                    m_Tracker.x_GetStatusNoLock(job_id);
                m_Tracker.m_UsedIds.set(job_id);
                break;
            }
        }}
        cnt += sleep_ms;
        if (timeout_ms >= 0 && cnt > (unsigned) timeout_ms) {
            NCBI_THROW(CNetServiceException,
                    eTimeout, "Failed to lock object");
        }
        SleepMilliSec(sleep_ms);
    } // while
}
*/

END_NCBI_SCOPE

