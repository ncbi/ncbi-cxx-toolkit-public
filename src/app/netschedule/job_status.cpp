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

#include <connect/netschedule_client.hpp>

#include "job_status.hpp"

BEGIN_NCBI_SCOPE

CNetScheduler_JobStatusTracker::CNetScheduler_JobStatusTracker()
{
    for (int i = 0; i < CNetScheduleClient::eLastStatus; ++i) {
        bm::bvector<>* bv = new bm::bvector<>();
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
    return CNetScheduleClient::eJobNotFound;
}

unsigned 
CNetScheduler_JobStatusTracker::CountStatus(
    CNetScheduleClient::EJobStatus status) const
{
    CReadLockGuard guard(m_Lock);

    const TBVector& bv = *m_StatusStor[(int)status];
    return bv.count();
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
}

void CNetScheduler_JobStatusTracker::ClearAll()
{
    CWriteLockGuard guard(m_Lock);

    for (TStatusStorage::size_type i = 0; i < m_StatusStor.size(); ++i) {
        TBVector& bv = *m_StatusStor[i];
        bv.clear(true);
    }
}

void
CNetScheduler_JobStatusTracker::SetExactStatusNoLock(unsigned int job_id, 
                                   CNetScheduleClient::EJobStatus status,
                                                     bool         set_clear)
{
    TBVector& bv = *m_StatusStor[(int)status];
    bv.set(job_id, set_clear);
}


CNetScheduleClient::EJobStatus 
CNetScheduler_JobStatusTracker::ChangeStatus(unsigned int  job_id, 
                           CNetScheduleClient::EJobStatus  status)
{
    CWriteLockGuard guard(m_Lock);
    CNetScheduleClient::EJobStatus old_status = 
                                CNetScheduleClient::eJobNotFound;

    switch (status) {

    case CNetScheduleClient::ePending:        
        old_status = GetStatusNoLock(job_id);
        if ((old_status == CNetScheduleClient::eJobNotFound) // new job
            ||
            (old_status == CNetScheduleClient::eReturned)
            ||
            (old_status == CNetScheduleClient::eRunning)
            ) {
            SetExactStatusNoLock(job_id, status, true);
            break;
        }
        ReportInvalidStatus(job_id, status, old_status);
        break;

    case CNetScheduleClient::eRunning:
        old_status = IsStatusNoLock(job_id, 
                                    CNetScheduleClient::ePending,
                                    CNetScheduleClient::eReturned,
                                    CNetScheduleClient::eCanceled);
        if ((int)old_status >= 0) {
            if (IsCancelCode(old_status)) {
                break;
            }
            x_SetClearStatusNoLock(job_id, status, old_status);
            break;
        }

        ReportInvalidStatus(job_id, status, old_status);
        break;

    case CNetScheduleClient::eReturned:
        old_status = IsStatusNoLock(job_id, 
                                    CNetScheduleClient::eRunning,
                                    CNetScheduleClient::eCanceled,
                                    CNetScheduleClient::eFailed);
        if ((int)old_status >= 0) {
            if (IsCancelCode(old_status)) {
                break;
            }
            x_SetClearStatusNoLock(job_id, status, old_status);
            break;
        }
        ReportInvalidStatus(job_id, status, old_status);
        break;

    case CNetScheduleClient::eCanceled:
        old_status = IsStatusNoLock(job_id, 
                                    CNetScheduleClient::ePending,
                                    CNetScheduleClient::eRunning,
                                    CNetScheduleClient::eReturned);
        if ((int)old_status >= 0) {
            x_SetClearStatusNoLock(job_id, status, old_status);
            break;
        }
        // in this case (failed, done) we just do nothing.
        old_status = CNetScheduleClient::eCanceled;
        break;

    case CNetScheduleClient::eFailed:
        old_status = IsStatusNoLock(job_id, 
                                    CNetScheduleClient::eRunning,
                                    CNetScheduleClient::eReturned);
        if ((int)old_status >= 0) {
            x_SetClearStatusNoLock(job_id, status, old_status);
            break;
        }

        old_status = CNetScheduleClient::eCanceled;
        break;

    case CNetScheduleClient::eDone:
        old_status = IsStatusNoLock(job_id, 
                                    CNetScheduleClient::eRunning,
                                    CNetScheduleClient::eReturned);
        if ((int)old_status >= 0) {
            x_SetClearStatusNoLock(job_id, status, old_status);
            break;
        }
        old_status = IsStatusNoLock(job_id, 
                                    CNetScheduleClient::eCanceled,
                                    CNetScheduleClient::eFailed);
        if (IsCancelCode(old_status)) {
            break;
        }

        ReportInvalidStatus(job_id, status, old_status);
        break;    
        
    default:
        _ASSERT(0);
    }

    return old_status;
}

unsigned int CNetScheduler_JobStatusTracker::GetPendingJob()
{
    TBVector& bv = *m_StatusStor[(int)CNetScheduleClient::ePending];

    CWriteLockGuard guard(m_Lock);
    unsigned int job_id = bv.get_first();
    if (job_id) {
        x_SetClearStatusNoLock(job_id, 
                               CNetScheduleClient::eRunning, 
                               CNetScheduleClient::ePending);
    }
    return job_id;
}

unsigned int CNetScheduler_JobStatusTracker::GetFirstDone() const
{
    TBVector& bv = *m_StatusStor[(int)CNetScheduleClient::eDone];

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
CNetScheduler_JobStatusTracker::ReportInvalidStatus(unsigned int /*job_id*/, 
                         CNetScheduleClient::EJobStatus          /*status*/,
                         CNetScheduleClient::EJobStatus      /*old_status*/)
{
    NCBI_THROW(CNetScheduleException, 
                eInvalidJobStatus, "Job status cannot be changed.");
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


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
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


