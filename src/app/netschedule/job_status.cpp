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


CJobStatusTracker::CJobStatusTracker()
 : m_BorrowedIds(bm::BM_GAP),
   m_LastPending(0),
   m_DoneCnt(0)
{
    for (int i = 0; i < CNetScheduleClient::eLastStatus; ++i) {
        TNSBitVector* bv;
        bv = new TNSBitVector(bm::BM_GAP);
/*
        if (i == (int)CNetScheduleClient::eDone     ||
            i == (int)CNetScheduleClient::eCanceled ||
            i == (int)CNetScheduleClient::eFailed   ||
            i == (int)CNetScheduleClient::ePending
            ) {
            bv = new TNSBitVector(bm::BM_GAP);
        } else {
            bv = new TNSBitVector();
        }
*/
        m_StatusStor.push_back(bv);
    }
}


CJobStatusTracker::~CJobStatusTracker()
{
    for (TStatusStorage::size_type i = 0; i < m_StatusStor.size(); ++i) {
        TNSBitVector* bv = m_StatusStor[i];
        delete bv;
    }
}


CNetScheduleClient::EJobStatus 
CJobStatusTracker::GetStatus(unsigned int job_id) const
{
    CReadLockGuard guard(m_Lock);
    return GetStatusNoLock(job_id);
}


CNetScheduleClient::EJobStatus 
CJobStatusTracker::GetStatusNoLock(unsigned int job_id) const
{
    for (int i = m_StatusStor.size()-1; i >= 0; --i) {
        const TNSBitVector& bv = *m_StatusStor[i];
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
CJobStatusTracker::CountStatus(CNetScheduleClient::EJobStatus status) const
{
    CReadLockGuard guard(m_Lock);

    const TNSBitVector& bv = *m_StatusStor[(int)status];
    unsigned cnt = bv.count();

    if (status == CNetScheduleClient::ePending) {
        cnt += m_BorrowedIds.count();
    }
    return cnt;
}


void CJobStatusTracker::CountStatus(TStatusSummaryMap*  status_map,
                                    const TNSBitVector* candidate_set)
{
    _ASSERT(status_map);
    status_map->clear();
    
    CReadLockGuard guard(m_Lock);

    for (size_t i = 0; i < m_StatusStor.size(); ++i) {
        const TNSBitVector& bv = *m_StatusStor[i];
        unsigned cnt;
        if (candidate_set) {
            cnt = bm::count_and(bv, *candidate_set);
        } else {
            cnt = bv.count();
        }
        (*status_map)[(CNetScheduleClient::EJobStatus)i] = cnt;
    } // for
}


unsigned CJobStatusTracker::Count(void)
{
    CReadLockGuard guard(m_Lock);

    unsigned cnt = 0;
    for (size_t i = 0; i < m_StatusStor.size(); ++i) {
        const TNSBitVector& bv = *m_StatusStor[i];
        cnt += bv.count();
    }
    return cnt;
}


void 
CJobStatusTracker::StatusStatistics(CNetScheduleClient::EJobStatus status,
                                    TNSBitVector::statistics*      st) const
{
    _ASSERT(st);
    CReadLockGuard guard(m_Lock);

    const TNSBitVector& bv = *m_StatusStor[(int)status];
    bv.calc_stat(st);

}


void CJobStatusTracker::StatusSnapshot(CNetScheduleClient::EJobStatus status,
                                       TNSBitVector*                  bv) const
{
    _ASSERT(bv);
    CReadLockGuard guard(m_Lock);

    const TNSBitVector& bv_s = *m_StatusStor[(int)status];
    *bv |= bv_s;
}


void CJobStatusTracker::Returned2Pending()
{
    CWriteLockGuard guard(m_Lock);
    Returned2PendingNoLock();
}


void CJobStatusTracker::Returned2PendingNoLock()
{
    TNSBitVector& bv_ret = *m_StatusStor[(int)CNetScheduleClient::eReturned];
    if (!bv_ret.any())
        return;

    TNSBitVector& bv_pen = *m_StatusStor[(int)CNetScheduleClient::ePending];

    bv_pen |= bv_ret;
    bv_ret.clear();
    bv_pen.count();
    m_LastPending = 0;
}


void CJobStatusTracker::ForceReschedule(unsigned job_id)
{
    CWriteLockGuard guard(m_Lock);

    CNetScheduleClient::EJobStatus old_st = GetStatusNoLock(job_id);
    x_SetClearStatusNoLock(job_id, CNetScheduleClient::ePending, old_st);
}


void 
CJobStatusTracker::SetStatus(unsigned int  job_id, 
                             CNetScheduleClient::EJobStatus  status)
{
    CWriteLockGuard guard(m_Lock);

    for (TStatusStorage::size_type i = 0; i < m_StatusStor.size(); ++i) {
        TNSBitVector& bv = *m_StatusStor[i];
        bv.set(job_id, (int)status == (int)i);
    }
    m_BorrowedIds.set(job_id, false);

    if (status == CNetScheduleClient::eDone) {
        IncDoneJobs();
    }
}

void CJobStatusTracker::Erase(unsigned int job_id)
{
    SetStatus(job_id, CNetScheduleClient::eJobNotFound);
}


void CJobStatusTracker::ClearAll(TNSBitVector* bv)
{
    CWriteLockGuard guard(m_Lock);

    for (TStatusStorage::size_type i = 0; i < m_StatusStor.size(); ++i) {
        TNSBitVector& bv1 = *m_StatusStor[i];
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


void CJobStatusTracker::FreeUnusedMem()
{
    for (TStatusStorage::size_type i = 0; i < m_StatusStor.size(); ++i) {
        TNSBitVector& bv = *m_StatusStor[i];
        {{
        CWriteLockGuard guard(m_Lock);
        bv.optimize(0, TNSBitVector::opt_free_0);
        }}
    }
    {{
    CWriteLockGuard guard(m_Lock);
    m_BorrowedIds.optimize(0, TNSBitVector::opt_free_0);
    }}
}


/*
void CJobStatusTracker::FreeUnusedMemNoLock()
{
    for (TStatusStorage::size_type i = 0; i < m_StatusStor.size(); ++i) {
        TNSBitVector& bv = *m_StatusStor[i];
        bv.optimize(0, TNSBitVector::opt_free_0);
    }
    m_BorrowedIds.optimize(0, TNSBitVector::opt_free_0);
}
*/


void
CJobStatusTracker::SetExactStatusNoLock(unsigned int job_id, 
                      CNetScheduleClient::EJobStatus status,
                                        bool         set_clear)
{
    TNSBitVector& bv = *m_StatusStor[(int)status];
    bv.set(job_id, set_clear);

    if ((status == CNetScheduleClient::ePending) && 
        (job_id < m_LastPending)) {
        m_LastPending = job_id - 1;
    }
}


CNetScheduleClient::EJobStatus 
CJobStatusTracker::ChangeStatus(unsigned int  job_id, 
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
CJobStatusTracker::AddPendingBatch(unsigned job_id_from, unsigned job_id_to)
{
    CWriteLockGuard guard(m_Lock);

    TNSBitVector& bv = *m_StatusStor[(int)CNetScheduleClient::ePending];

    bv.set_range(job_id_from, job_id_to);
}


//unsigned CJobStatusTracker::GetPendingJob()
//{
//    CWriteLockGuard guard(m_Lock);
//    return GetPendingJobNoLock();
//}


bool 
CJobStatusTracker::GetPendingJobFromSet(TNSBitVector* candidate_set,
                                        unsigned* job_id)
{
    *job_id = 0;
    TNSBitVector& bv = *m_StatusStor[(int)CNetScheduleClient::ePending];

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
            TNSBitVector::enumerator en(candidate_set->first());
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
CJobStatusTracker::GetPendingJob(const TNSBitVector& unwanted_jobs,
                                 unsigned*           job_id)
{
    CWriteLockGuard guard(m_Lock);
    *job_id = 0;
    TNSBitVector& bv = *m_StatusStor[(int)CNetScheduleClient::ePending];
    if (!bv.any()) {
        Returned2PendingNoLock();
        if (!bv.any()) {
            return false;
        }
    }
    TNSBitVector  bv_pending(bv);
    bv_pending -= unwanted_jobs;
    TNSBitVector::enumerator en(bv_pending.first());
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
CJobStatusTracker::PendingIntersect(TNSBitVector* candidate_set)
{
    CReadLockGuard guard(m_Lock);

    TNSBitVector& bv = *m_StatusStor[(int)CNetScheduleClient::ePending];
    *candidate_set &= bv;
}


unsigned 
CJobStatusTracker::GetPendingJobNoLock()
{
    TNSBitVector& bv = *m_StatusStor[(int)CNetScheduleClient::ePending];

    bm::id_t job_id;
    int i = 0;
    do {
        job_id = bv.extract_next(m_LastPending);
        if (job_id) {
            TNSBitVector& bv2 =
                *m_StatusStor[(int)CNetScheduleClient::eRunning];
            bv2.set_bit(job_id, true);
            m_LastPending = job_id;
            break;
        } else {
            Returned2PendingNoLock();            
        }
    } while (++i < 2);
    return job_id;
}


unsigned int CJobStatusTracker::BorrowPendingJob()
{
    TNSBitVector& bv = 
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


void CJobStatusTracker::ReturnBorrowedJob(unsigned int  job_id, 
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


bool CJobStatusTracker::AnyPending() const
{
    const TNSBitVector& bv = 
        *m_StatusStor[(int)CNetScheduleClient::ePending];

    CReadLockGuard guard(m_Lock);
    return bv.any();
}


unsigned int CJobStatusTracker::GetFirstDone() const
{
    return GetFirst(CNetScheduleClient::eDone);
}


unsigned int 
CJobStatusTracker::GetFirst(CNetScheduleClient::EJobStatus status) const
{
    const TNSBitVector& bv = *m_StatusStor[(int)status];

    CReadLockGuard guard(m_Lock);
    unsigned int job_id = bv.get_first();
    return job_id;
}


void 
CJobStatusTracker::x_SetClearStatusNoLock(unsigned int job_id,
                        CNetScheduleClient::EJobStatus status,
                        CNetScheduleClient::EJobStatus old_status)
{
    SetExactStatusNoLock(job_id, status, true);
    SetExactStatusNoLock(job_id, old_status, false);
}


void 
CJobStatusTracker::ReportInvalidStatus(unsigned int job_id, 
                     CNetScheduleClient::EJobStatus status,
                     CNetScheduleClient::EJobStatus old_status)
{
    string msg = "Job status cannot be changed. ";
    msg += "Old status ";
    msg += CNetScheduleClient::StatusToString(old_status);
    msg += ". New status ";
    msg += CNetScheduleClient::StatusToString(status);
    NCBI_THROW(CNetScheduleException, eInvalidJobStatus, msg);
}


CNetScheduleClient::EJobStatus 
CJobStatusTracker::IsStatusNoLock(unsigned int job_id, 
        CNetScheduleClient::EJobStatus st1,
        CNetScheduleClient::EJobStatus st2,
        CNetScheduleClient::EJobStatus st3
        ) const
{
    if (st1 == CNetScheduleClient::eJobNotFound) {
        return CNetScheduleClient::eJobNotFound;;
    } else {
        const TNSBitVector& bv = *m_StatusStor[(int)st1];
        if (bv[job_id]) {
            return st1;
        }
    }

    if (st2 == CNetScheduleClient::eJobNotFound) {
        return CNetScheduleClient::eJobNotFound;;
    } else {
        const TNSBitVector& bv = *m_StatusStor[(int)st2];
        if (bv[job_id]) {
            return st2;
        }
    }

    if (st3 == CNetScheduleClient::eJobNotFound) {
        return CNetScheduleClient::eJobNotFound;;
    } else {
        const TNSBitVector& bv = *m_StatusStor[(int)st3];
        if (bv[job_id]) {
            return st3;
        }
    }

    return CNetScheduleClient::eJobNotFound;
}


bool
CJobStatusTracker::SwitchStatusNoLock(unsigned int job_id,
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
CJobStatusTracker::PrintStatusMatrix(CNcbiOstream& out) const
{
    CReadLockGuard guard(m_Lock);
    for (size_t i = CNetScheduleClient::ePending; 
                i < m_StatusStor.size(); ++i) {
        out << "status:" 
            << CNetScheduleClient::StatusToString(
                        (CNetScheduleClient::EJobStatus)i) << "\n\n";
        const TNSBitVector& bv = *m_StatusStor[i];
        TNSBitVector::enumerator en(bv.first());
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
    TNSBitVector::enumerator en(m_BorrowedIds.first());
    for (int cnt = 0;en.valid(); ++en, ++cnt) {
        out << *en << ", ";
        if (cnt % 10 == 0) {
            out << "\n";
        }
    } // for
    out << "\n\n";
    
}


void CJobStatusTracker::IncDoneJobs()
{
    ++m_DoneCnt;
    if (m_DoneCnt == 65535 * 2) {
        m_DoneCnt = 0;
        {{
        TNSBitVector& bv = *m_StatusStor[(int)CNetScheduleClient::eDone];
        bv.optimize(0, TNSBitVector::opt_free_01);
        }}
        {{
        TNSBitVector& bv = *m_StatusStor[(int)CNetScheduleClient::ePending];
        bv.optimize(0, TNSBitVector::opt_free_0);
        }}
    }
}


END_NCBI_SCOPE
