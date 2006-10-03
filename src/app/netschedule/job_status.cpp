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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description: Network scheduler job status store
 *
 */
#include <ncbi_pch.hpp>

#include <connect/services/netschedule_client.hpp>
#include <util/bitset/bmalgo.h>

#include "job_status.hpp"

BEGIN_NCBI_SCOPE

CNetScheduler_JobStatusTracker::CNetScheduler_JobStatusTracker()
 : m_BorrowedIds(bm::BM_GAP),
   m_LastPending(0),
   m_DoneCnt(0)
{
    for (int i = 0; i < CNetScheduleClient::eLastStatus; ++i) {
        bm::bvector<>* bv;
        bv = new bm::bvector<>(bm::BM_GAP);
/*
        if (i == (int)CNetScheduleClient::eDone     ||
            i == (int)CNetScheduleClient::eCanceled ||
            i == (int)CNetScheduleClient::eFailed   ||
            i == (int)CNetScheduleClient::ePending
            ) {
            bv = new bm::bvector<>(bm::BM_GAP);
        } else {
            bv = new bm::bvector<>();
        }
*/
        m_StatusStor.push_back(bv);
    }
}

CNetScheduler_JobStatusTracker::~CNetScheduler_JobStatusTracker()
{
    for (TStatusStorage::size_type i = 0; i < m_StatusStor.size(); ++i) {
        TBVector* bv = m_StatusStor[i];
        delete bv;
    }
}

CNetScheduleClient::EJobStatus 
CNetScheduler_JobStatusTracker::GetStatus(unsigned int job_id) const
{
    CReadLockGuard guard(m_Lock);
    return GetStatusNoLock(job_id);
}

CNetScheduleClient::EJobStatus 
CNetScheduler_JobStatusTracker::GetStatusNoLock(unsigned int job_id) const
{
    for (int i = m_StatusStor.size()-1; i >= 0; --i) {
        const TBVector& bv = *m_StatusStor[i];
        bool b = bv[job_id];
        if (b) {
            return (CNetScheduleClient::EJobStatus)i;
        }
    }
    if (m_BorrowedIds[job_id]) {
        return CNetScheduleClient::ePending;
    }
    return CNetScheduleClient::eJobNotFound;
}

unsigned 
CNetScheduler_JobStatusTracker::CountStatus(
    CNetScheduleClient::EJobStatus status) const
{
    CReadLockGuard guard(m_Lock);

    const TBVector& bv = *m_StatusStor[(int)status];
    unsigned cnt = bv.count();

    if (status == CNetScheduleClient::ePending) {
        cnt += m_BorrowedIds.count();
    }
    return cnt;
}

void CNetScheduler_JobStatusTracker::CountStatus(
                            TStatusSummaryMap*   status_map, 
                            const bm::bvector<>* candidate_set)
{
    _ASSERT(status_map);
    status_map->erase(status_map->begin(), status_map->end());
    
    CReadLockGuard guard(m_Lock);

    for (size_t i = 0; i < m_StatusStor.size(); ++i) {
        const TBVector& bv = *m_StatusStor[i];
        unsigned cnt;
        if (candidate_set) {
            cnt = bm::count_and(bv, *candidate_set);
        } else {
            cnt = bv.count();
        }
        (*status_map)[(CNetScheduleClient::EJobStatus)i] = cnt;
    } // for
}

void 
CNetScheduler_JobStatusTracker::StatusStatistics(
                      CNetScheduleClient::EJobStatus status,
                      TBVector::statistics*          st) const
{
    _ASSERT(st);
    CReadLockGuard guard(m_Lock);

    const TBVector& bv = *m_StatusStor[(int)status];
    bv.calc_stat(st);

}

void CNetScheduler_JobStatusTracker::StatusSnapshot(
                        CNetScheduleClient::EJobStatus status,
                        TBVector*                      bv) const
{
    _ASSERT(bv);
    CReadLockGuard guard(m_Lock);

    const TBVector& bv_s = *m_StatusStor[(int)status];
    *bv |= bv_s;
}


void CNetScheduler_JobStatusTracker::Returned2Pending()
{
    CWriteLockGuard guard(m_Lock);
    Returned2PendingNoLock();
}

void CNetScheduler_JobStatusTracker::Returned2PendingNoLock()
{
    TBVector& bv_ret = *m_StatusStor[(int)CNetScheduleClient::eReturned];
    if (!bv_ret.any())
        return;

    TBVector& bv_pen = *m_StatusStor[(int)CNetScheduleClient::ePending];

    bv_pen |= bv_ret;
    bv_ret.clear();
    bv_pen.count();
    m_LastPending = 0;
}


void CNetScheduler_JobStatusTracker::ForceReschedule(unsigned job_id)
{
    CWriteLockGuard guard(m_Lock);

    CNetScheduleClient::EJobStatus old_st = GetStatusNoLock(job_id);
    x_SetClearStatusNoLock(job_id, CNetScheduleClient::ePending, old_st);
}


void 
CNetScheduler_JobStatusTracker::SetStatus(unsigned int  job_id, 
                        CNetScheduleClient::EJobStatus  status)
{
    CWriteLockGuard guard(m_Lock);

    for (TStatusStorage::size_type i = 0; i < m_StatusStor.size(); ++i) {
        TBVector& bv = *m_StatusStor[i];
        bv.set(job_id, (int)status == (int)i);
    }
    m_BorrowedIds.set(job_id, false);

    if (status == CNetScheduleClient::eDone) {
        IncDoneJobs();
    }
}

void CNetScheduler_JobStatusTracker::ClearAll(TBVector* bv)
{
    CWriteLockGuard guard(m_Lock);

    for (TStatusStorage::size_type i = 0; i < m_StatusStor.size(); ++i) {
        TBVector& bv1 = *m_StatusStor[i];
        if (bv) {
            *bv |= bv1;
        }
        bv1.clear(true);
    }
    if (bv) {
        *bv |= m_BorrowedIds;
    }
    m_BorrowedIds.clear(true);
}

void CNetScheduler_JobStatusTracker::FreeUnusedMem()
{
    for (TStatusStorage::size_type i = 0; i < m_StatusStor.size(); ++i) {
        TBVector& bv = *m_StatusStor[i];
        {{
        CWriteLockGuard guard(m_Lock);
        bv.optimize(0, TBVector::opt_free_0);
        }}
    }
    {{
    CWriteLockGuard guard(m_Lock);
    m_BorrowedIds.optimize(0, TBVector::opt_free_0);
    }}
}

/*
void CNetScheduler_JobStatusTracker::FreeUnusedMemNoLock()
{
    for (TStatusStorage::size_type i = 0; i < m_StatusStor.size(); ++i) {
        TBVector& bv = *m_StatusStor[i];
        bv.optimize(0, TBVector::opt_free_0);
    }
    m_BorrowedIds.optimize(0, TBVector::opt_free_0);
}
*/

void
CNetScheduler_JobStatusTracker::SetExactStatusNoLock(unsigned int job_id, 
                                   CNetScheduleClient::EJobStatus status,
                                                     bool         set_clear)
{
    TBVector& bv = *m_StatusStor[(int)status];
    bv.set(job_id, set_clear);

    if ((status == CNetScheduleClient::ePending) && 
        (job_id < m_LastPending)) {
        m_LastPending = job_id - 1;
    }
}


CNetScheduleClient::EJobStatus 
CNetScheduler_JobStatusTracker::ChangeStatus(unsigned int  job_id, 
                           CNetScheduleClient::EJobStatus  status,
                           bool*                           updated)
{
    CWriteLockGuard guard(m_Lock);
    bool status_updated = false;
    CNetScheduleClient::EJobStatus old_status = 
                                CNetScheduleClient::eJobNotFound;

    switch (status) {

    case CNetScheduleClient::ePending:
        old_status = GetStatusNoLock(job_id);
        if (old_status == CNetScheduleClient::eJobNotFound) { // new job
            SetExactStatusNoLock(job_id, status, true);
            status_updated = true;
            break;
        }
        if ((old_status == CNetScheduleClient::eReturned) ||
            (old_status == CNetScheduleClient::eRunning)) {
            x_SetClearStatusNoLock(job_id, status, old_status);
            status_updated = true;
            break;
        }
        ReportInvalidStatus(job_id, status, old_status);
        break;

    case CNetScheduleClient::eRunning:
        old_status = IsStatusNoLock(job_id, 
                                    CNetScheduleClient::ePending,
                                    CNetScheduleClient::eReturned,
                                    CNetScheduleClient::eCanceled);
        if (old_status != CNetScheduleClient::eJobNotFound) {
            if (IsCancelCode(old_status)) {
                break;
            }
            x_SetClearStatusNoLock(job_id, status, old_status);
            status_updated = true;
            break;
        }

        ReportInvalidStatus(job_id, status, old_status);
        break;

    case CNetScheduleClient::eReturned:
        old_status = IsStatusNoLock(job_id, 
                                    CNetScheduleClient::eRunning,
                                    CNetScheduleClient::eCanceled,
                                    CNetScheduleClient::eFailed);
        if (old_status != CNetScheduleClient::eJobNotFound) {
            if (IsCancelCode(old_status)) {
                break;
            }
            x_SetClearStatusNoLock(job_id, status, old_status);
            status_updated = true;
            break;
        }
        old_status = IsStatusNoLock(job_id,                                 
                                    CNetScheduleClient::eReturned,
                                    CNetScheduleClient::eDone,
                                    CNetScheduleClient::ePending);
        if (old_status != CNetScheduleClient::eJobNotFound) {
            break;
        }
        old_status = GetStatusNoLock(job_id);
        ReportInvalidStatus(job_id, status, old_status);
        break;

    case CNetScheduleClient::eCanceled:
        old_status = IsStatusNoLock(job_id, 
                                    CNetScheduleClient::ePending,
                                    CNetScheduleClient::eRunning,
                                    CNetScheduleClient::eReturned);
        if (old_status != CNetScheduleClient::eJobNotFound) {
            x_SetClearStatusNoLock(job_id, status, old_status);
            status_updated = true;
            break;
        }
        // in this case (failed, done) we just do nothing.
        old_status = CNetScheduleClient::eCanceled;
        break;

    case CNetScheduleClient::eFailed:
        old_status = IsStatusNoLock(job_id, 
                                    CNetScheduleClient::eRunning,
                                    CNetScheduleClient::eReturned);
        if (old_status != CNetScheduleClient::eJobNotFound) {
            x_SetClearStatusNoLock(job_id, status, old_status);
            status_updated = true;
            break;
        }

        old_status = CNetScheduleClient::eFailed;
        break;

    case CNetScheduleClient::eDone:

        old_status = IsStatusNoLock(job_id,
                                    CNetScheduleClient::eRunning,
                                    CNetScheduleClient::ePending,
                                    CNetScheduleClient::eReturned);
        if (old_status != CNetScheduleClient::eJobNotFound) {
            x_SetClearStatusNoLock(job_id, status, old_status);
            status_updated = true;
            IncDoneJobs();
            break;
        }
        old_status = IsStatusNoLock(job_id, 
                                    CNetScheduleClient::eCanceled,
                                    CNetScheduleClient::eFailed);
        if (IsCancelCode(old_status)) {
            break;
        }
        old_status = GetStatusNoLock(job_id);
        if (old_status == CNetScheduleClient::eDone) {
            break;
        }
        ReportInvalidStatus(job_id, status, old_status);
        break;    
        
    default:
        _ASSERT(0);
    }

    if (updated) *updated = status_updated;
    return old_status;
}


void 
CNetScheduler_JobStatusTracker::AddPendingBatch(
                                    unsigned job_id_from, unsigned job_id_to)
{
    CWriteLockGuard guard(m_Lock);

    TBVector& bv = *m_StatusStor[(int)CNetScheduleClient::ePending];

    bv.set_range(job_id_from, job_id_to);
}


//unsigned CNetScheduler_JobStatusTracker::GetPendingJob()
//{
//    CWriteLockGuard guard(m_Lock);
//    return GetPendingJobNoLock();
//}


bool 
CNetScheduler_JobStatusTracker::GetPendingJobFromSet(
    bm::bvector<>* candidate_set, unsigned* job_id)
{
    *job_id = 0;
    TBVector& bv = *m_StatusStor[(int)CNetScheduleClient::ePending];

    {{
        unsigned p_id;
        {{
            CWriteLockGuard guard(m_Lock);

            if (!bv.any()) {
                Returned2PendingNoLock();
                if (!bv.any()) {
                    return false;
                }
            }
            p_id = bv.get_first();
        }}

        // here i'm trying to check if gap between head of candidates
        // and head of pending set is big enough to justify a preliminary
        // filtering using logical operation
        // often logical AND costs more than just iterating the bitset
        //  

        unsigned c_id = candidate_set->get_first();
        unsigned min_id = min(c_id, p_id);
        unsigned max_id = max(c_id, p_id);
        // comparison value is a pure guess, nobody knows the optimal value
        if ((max_id - min_id) > 250) {
            // Filter candidates to use only pending jobs
            CReadLockGuard guard(m_Lock);
            *candidate_set &= bv;
        }
    }}

    unsigned candidate_id = 0;
    while ( 0 == candidate_id ) {
        // STAGE 1: (read lock)
        // look for the first pending candidate bit 
        {{
            CReadLockGuard guard(m_Lock);
            bm::bvector<>::enumerator en(candidate_set->first());
            if (!en.valid()) { // no more candidates
                return bv.any();
            }
            for (; en.valid(); ++en) {
                unsigned id = *en;
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
                                       CNetScheduleClient::eRunning,
                                       CNetScheduleClient::ePending);
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

    {{
        CReadLockGuard guard(m_Lock);
        return bv.any();
    }}
}

bool 
CNetScheduler_JobStatusTracker::GetPendingJob(
                                const bm::bvector<>& unwanted_jobs,
                                unsigned*            job_id)
{
    CWriteLockGuard guard(m_Lock);
    *job_id = 0;
    TBVector& bv = *m_StatusStor[(int)CNetScheduleClient::ePending];
    if (!bv.any()) {
        Returned2PendingNoLock();
        if (!bv.any()) {
            return false;
        }
    }
    TBVector  bv_pending(bv);
    bv_pending -= unwanted_jobs;
    bm::bvector<>::enumerator en(bv_pending.first());
    if (!en.valid()) { // no more candidates
        return bv.any();
    }

    unsigned candidate_id = *en;
    x_SetClearStatusNoLock(candidate_id, 
                            CNetScheduleClient::eRunning,
                            CNetScheduleClient::ePending);
    *job_id = candidate_id;
    return true;
}


void 
CNetScheduler_JobStatusTracker::PendingIntersect(bm::bvector<>* candidate_set)
{
    CReadLockGuard guard(m_Lock);

    TBVector& bv = *m_StatusStor[(int)CNetScheduleClient::ePending];
    *candidate_set &= bv;
}


unsigned 
CNetScheduler_JobStatusTracker::GetPendingJobNoLock()
{
    TBVector& bv = *m_StatusStor[(int)CNetScheduleClient::ePending];

    bm::id_t job_id;
    int i = 0;
    do {
        job_id = bv.extract_next(m_LastPending);
        if (job_id) {
            TBVector& bv2 = *m_StatusStor[(int)CNetScheduleClient::eRunning];
            bv2.set_bit(job_id, true);
            m_LastPending = job_id;
            break;
        } else {
            Returned2PendingNoLock();            
        }
    } while (++i < 2);
    return job_id;
}


unsigned int CNetScheduler_JobStatusTracker::BorrowPendingJob()
{
    TBVector& bv = 
        *m_StatusStor[(int)CNetScheduleClient::ePending];

    bm::id_t job_id;
    CWriteLockGuard guard(m_Lock);

    for (int i = 0; i < 2; ++i) {
        job_id = bv.get_next(m_LastPending);
        if (job_id) {
            m_BorrowedIds.set(job_id, true);
            m_LastPending = job_id;
            bv.set(job_id, false);
            break;
        } else {
            Returned2PendingNoLock();
        }
/*
        if (bv.any()) {
            job_id = bv.get_first();
            if (job_id) {
                m_BorrowedIds.set(job_id, true);
                return job_id;
            }
        }        
        Returned2PendingNoLock();
*/
    }
    return job_id;
}

void CNetScheduler_JobStatusTracker::ReturnBorrowedJob(unsigned int  job_id, 
                                                       CNetScheduleClient::EJobStatus status)
{
    CWriteLockGuard guard(m_Lock);
    if (!m_BorrowedIds[job_id]) {
        ReportInvalidStatus(job_id, status, CNetScheduleClient::ePending);
        return;
    }
    m_BorrowedIds.set(job_id, false);
    SetExactStatusNoLock(job_id, status, true /*set status*/);
}


bool CNetScheduler_JobStatusTracker::AnyPending() const
{
    const TBVector& bv = 
        *m_StatusStor[(int)CNetScheduleClient::ePending];

    CReadLockGuard guard(m_Lock);
    return bv.any();
}

unsigned int CNetScheduler_JobStatusTracker::GetFirstDone() const
{
    return GetFirst(CNetScheduleClient::eDone);
}

unsigned int 
CNetScheduler_JobStatusTracker::GetFirst(
                        CNetScheduleClient::EJobStatus status) const
{
    const TBVector& bv = *m_StatusStor[(int)status];

    CReadLockGuard guard(m_Lock);
    unsigned int job_id = bv.get_first();
    return job_id;
}


void 
CNetScheduler_JobStatusTracker::x_SetClearStatusNoLock(
                                            unsigned int job_id,
                          CNetScheduleClient::EJobStatus status,
                          CNetScheduleClient::EJobStatus old_status)
{
    SetExactStatusNoLock(job_id, status, true);
    SetExactStatusNoLock(job_id, old_status, false);
}

void 
CNetScheduler_JobStatusTracker::ReportInvalidStatus(unsigned int job_id, 
                         CNetScheduleClient::EJobStatus          status,
                         CNetScheduleClient::EJobStatus      old_status)
{
    string msg = "Job status cannot be changed. ";
    msg += "Old status ";
    msg += CNetScheduleClient::StatusToString(old_status);
    msg += ". New status ";
    msg += CNetScheduleClient::StatusToString(status);
    NCBI_THROW(CNetScheduleException, eInvalidJobStatus, msg);
}


CNetScheduleClient::EJobStatus 
CNetScheduler_JobStatusTracker::IsStatusNoLock(unsigned int job_id, 
        CNetScheduleClient::EJobStatus st1,
        CNetScheduleClient::EJobStatus st2,
        CNetScheduleClient::EJobStatus st3
        ) const
{
    if (st1 == CNetScheduleClient::eJobNotFound) {
        return CNetScheduleClient::eJobNotFound;;
    } else {
        const TBVector& bv = *m_StatusStor[(int)st1];
        if (bv[job_id]) {
            return st1;
        }
    }

    if (st2 == CNetScheduleClient::eJobNotFound) {
        return CNetScheduleClient::eJobNotFound;;
    } else {
        const TBVector& bv = *m_StatusStor[(int)st2];
        if (bv[job_id]) {
            return st2;
        }
    }

    if (st3 == CNetScheduleClient::eJobNotFound) {
        return CNetScheduleClient::eJobNotFound;;
    } else {
        const TBVector& bv = *m_StatusStor[(int)st3];
        if (bv[job_id]) {
            return st3;
        }
    }

    return CNetScheduleClient::eJobNotFound;
}


bool
CNetScheduler_JobStatusTracker::SwitchStatusNoLock(unsigned int job_id,
    CNetScheduleClient::EJobStatus new_st,
    CNetScheduleClient::EJobStatus st1,
    CNetScheduleClient::EJobStatus st2,
    CNetScheduleClient::EJobStatus st3
    )
{
    CNetScheduleClient::EJobStatus old_status = IsStatusNoLock(job_id, 
                                    st1, st2, st3);
    if (old_status == CNetScheduleClient::eJobNotFound)
        return false;
    x_SetClearStatusNoLock(job_id, new_st, old_status);
    return true;
}


void 
CNetScheduler_JobStatusTracker::PrintStatusMatrix(CNcbiOstream& out) const
{
    CReadLockGuard guard(m_Lock);
    for (size_t i = CNetScheduleClient::ePending; 
                i < m_StatusStor.size(); ++i) {
        out << "status:" 
            << CNetScheduleClient::StatusToString(
                        (CNetScheduleClient::EJobStatus)i) << "\n\n";
        const TBVector& bv = *m_StatusStor[i];
        TBVector::enumerator en(bv.first());
        for (int cnt = 0;en.valid(); ++en, ++cnt) {
            out << *en << ", ";
            if (cnt % 10 == 0) {
                out << "\n";
            }
        } // for
        out << "\n\n";
        if (!out.good()) break;
    } // for

    out << "status: Borrowed pending" << "\n\n";
    TBVector::enumerator en(m_BorrowedIds.first());
    for (int cnt = 0;en.valid(); ++en, ++cnt) {
        out << *en << ", ";
        if (cnt % 10 == 0) {
            out << "\n";
        }
    } // for
    out << "\n\n";
    
}

void CNetScheduler_JobStatusTracker::IncDoneJobs()
{
    ++m_DoneCnt;
    if (m_DoneCnt == 65535 * 2) {
        m_DoneCnt = 0;
        {{
        TBVector& bv = *m_StatusStor[(int)CNetScheduleClient::eDone];
        bv.optimize(0, TBVector::opt_free_01);
        }}
        {{
        TBVector& bv = *m_StatusStor[(int)CNetScheduleClient::ePending];
        bv.optimize(0, TBVector::opt_free_0);
        }}
    }
}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.32  2006/10/03 14:56:56  joukovv
 * Delayed job deletion implemented, code restructured preparing to move to
 * thread-per-request model.
 *
 * Revision 1.31  2006/09/21 21:28:59  joukovv
 * Consistency of memory state and database strengthened, ability to retry failed
 * jobs on different nodes (and corresponding queue parameter, failed_retries)
 * added, overall code regularization performed.
 *
 * Revision 1.30  2006/06/26 13:46:01  kuznets
 * Fixed job expiration and restart mechanism
 *
 * Revision 1.29  2006/06/07 13:00:01  kuznets
 * Implemented command to get status summary based on affinity token
 *
 * Revision 1.28  2006/03/17 14:25:29  kuznets
 * Force reschedule (to re-try failed jobs)
 *
 * Revision 1.27  2006/03/13 16:01:36  kuznets
 * Fixed queue truncation (transaction log overflow). Added commands to print queue selectively
 *
 * Revision 1.26  2006/02/23 15:45:04  kuznets
 * Added more frequent and non-intrusive memory optimization of status matrix
 *
 * Revision 1.25  2006/02/21 14:44:57  kuznets
 * Bug fixes, improvements in statistics
 *
 * Revision 1.24  2006/02/09 17:07:42  kuznets
 * Various improvements in job scheduling with respect to affinity
 *
 * Revision 1.23  2006/02/06 14:10:29  kuznets
 * Added job affinity
 *
 * Revision 1.22  2005/08/26 12:36:10  kuznets
 * Performance optimization
 *
 * Revision 1.21  2005/08/22 14:01:58  kuznets
 * Added JobExchange command
 *
 * Revision 1.20  2005/08/18 19:25:02  kuznets
 * Bug fix
 *
 * Revision 1.19  2005/08/18 19:16:31  kuznets
 * Performance optimization
 *
 * Revision 1.18  2005/08/18 18:04:47  kuznets
 * GetPendingJob() performance optimization
 *
 * Revision 1.17  2005/08/18 16:40:01  kuznets
 * Fixed bug in GetPendingJob()
 *
 * Revision 1.16  2005/08/18 16:24:32  kuznets
 * Optimized job retrival out o the bit matrix
 *
 * Revision 1.15  2005/08/15 13:29:46  kuznets
 * Implemented batch job submission
 *
 * Revision 1.14  2005/07/14 13:12:56  kuznets
 * Added load balancer
 *
 * Revision 1.13  2005/06/20 15:36:25  kuznets
 * Let job go from pending to done (rescheduling)
 *
 * Revision 1.12  2005/05/04 19:09:43  kuznets
 * Added queue dumping
 *
 * Revision 1.11  2005/05/02 14:44:40  kuznets
 * Implemented remote monitoring
 *
 * Revision 1.10  2005/04/27 14:59:46  kuznets
 * Improved error messaging
 *
 * Revision 1.9  2005/03/22 19:02:54  kuznets
 * Reflecting chnages in connect layout
 *
 * Revision 1.8  2005/03/09 17:37:16  kuznets
 * Added node notification thread and execution control timeline
 *
 * Revision 1.7  2005/03/04 12:06:41  kuznets
 * Implenyed UDP callback to clients
 *
 * Revision 1.6  2005/02/28 12:24:17  kuznets
 * New job status Returned, better error processing and queue management
 *
 * Revision 1.5  2005/02/23 19:16:38  kuznets
 * Implemented garbage collection thread
 *
 * Revision 1.4  2005/02/22 16:13:00  kuznets
 * Performance optimization
 *
 * Revision 1.3  2005/02/14 17:57:41  kuznets
 * Fixed a bug in queue procesing
 *
 * Revision 1.2  2005/02/11 14:45:29  kuznets
 * Fixed compilation issue (GCC)
 *
 * Revision 1.1  2005/02/08 16:42:55  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */


