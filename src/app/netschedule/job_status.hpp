#ifndef NETSCHEDULE_JOB_STATUS__HPP
#define NETSCHEDULE_JOB_STATUS__HPP

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
 * File Description:
 *   Net schedule job status states.
 *
 */

/// @file job_status.hpp
/// NetSchedule job status tracker. 
///
/// @internal

#include <util/bitset/ncbi_bitset.hpp>
#include <corelib/ncbimtx.hpp>

BEGIN_NCBI_SCOPE


/// In-Memory storage to track status of all jobs
/// Syncronized thread safe class
///
/// @internal
class CNetScheduler_JobStatusTracker
{
public:
    CNetScheduler_JobStatusTracker();
    ~CNetScheduler_JobStatusTracker();

    CNetScheduleClient::EJobStatus 
        GetStatus(unsigned int job_id) const;

    /// Set job status. (Controls status change logic)
    ///
    ///  Status switching rules ([] - means request ignored:
    ///  ePending   <- (eReturned, eRunning, "no status");
    ///  eRunning   <- (ePending, eReturned, [eCanceled])
    ///  eReturned  <- (eRunning, [eCanceled, eFailed])
    ///  eCanceled  <- (ePending, eRunning, eReturned) (ignored if job is ready)
    ///  eFailed    <- (eRunning, eReturned, [eCanceled])
    ///  eDone      <- (eRunning, eReturned, [eCanceled, eFailed])
    ///
    /// @return old status. Job may be already canceled (or failed)
    /// in this case status change is ignored (
    /// 
    CNetScheduleClient::EJobStatus
    ChangeStatus(unsigned int                   job_id, 
                 CNetScheduleClient::EJobStatus status);

    /// Return job id (job is moved from pending to running)
    /// 0 - no pending jobs
    unsigned int GetPendingJob();

    /// TRUE if we have pending jobs
    bool AnyPending() const;

    /// Returned jobs come back to pending area
    void Return2Pending();


    /// Get first job id from DONE status
    unsigned int GetFirstDone() const;

    /// Get first job in the specified status
    unsigned int GetFirst(CNetScheduleClient::EJobStatus  status) const;

    /// Set job status without logic control.
    /// @param status
    ///     Status to set (all other statuses are cleared)
    ///     Non existing status code clears all statuses
    void SetStatus(unsigned int                    job_id, 
                   CNetScheduleClient::EJobStatus  status);


    /// Set job status without any protection 
    void SetExactStatusNoLock(
        unsigned int                         job_id, 
        CNetScheduleClient::EJobStatus       status,
                              bool           set_clear);

    /// Return number of jobs in specified status
    unsigned CountStatus(
        CNetScheduleClient::EJobStatus status) const;

    static
    bool IsCancelCode(CNetScheduleClient::EJobStatus status)
    {
        return (status == CNetScheduleClient::eCanceled) ||
               (status == CNetScheduleClient::eFailed);
    }

    /// Clear status storage
    void ClearAll();

    /// Free unused memory buffers
    void FreeUnusedMem();

private:

    typedef bm::bvector<>     TBVector;
    typedef vector<TBVector*> TStatusStorage;

protected:

    CNetScheduleClient::EJobStatus 
        GetStatusNoLock(unsigned int job_id) const;

    /// Check if job is in specified status
    /// @return -1 if no, status value otherwise
    CNetScheduleClient::EJobStatus 
    IsStatusNoLock(unsigned int job_id, 
        CNetScheduleClient::EJobStatus st1,
        CNetScheduleClient::EJobStatus st2 = CNetScheduleClient::eJobNotFound,
        CNetScheduleClient::EJobStatus st3 = CNetScheduleClient::eJobNotFound
        ) const;
    
    void ReportInvalidStatus(unsigned int job_id, 
             CNetScheduleClient::EJobStatus  status,
             CNetScheduleClient::EJobStatus  old_status);
    void x_SetClearStatusNoLock(unsigned int job_id,
             CNetScheduleClient::EJobStatus          status,
             CNetScheduleClient::EJobStatus          old_status);

    void Return2PendingNoLock();

private:
    CNetScheduler_JobStatusTracker(const CNetScheduler_JobStatusTracker&);
    CNetScheduler_JobStatusTracker& 
        operator=(const CNetScheduler_JobStatusTracker&);
private:

    TStatusStorage          m_StatusStor;
    mutable CRWLock         m_Lock;
};

/// @internal
class CNetSchedule_JS_Guard
{
public:
    CNetSchedule_JS_Guard(
        CNetScheduler_JobStatusTracker& strack,
        unsigned int                    job_id,
        CNetScheduleClient::EJobStatus  status)
     : m_Tracker(strack),
       m_OldStatus(strack.ChangeStatus(job_id, status)),
       m_JobId(job_id)
    {
        _ASSERT(job_id);
    }

    ~CNetSchedule_JS_Guard()
    {
        // roll back to the old status
        if (m_OldStatus >= -1) {
            m_Tracker.SetStatus(m_JobId, 
              (CNetScheduleClient::EJobStatus)m_OldStatus);
        } 
    }

    void Release() { m_OldStatus = -2; }

    CNetScheduleClient::EJobStatus GetOldStatus() const
    {
        return (CNetScheduleClient::EJobStatus) m_OldStatus;
    }


private:
    CNetSchedule_JS_Guard(const CNetSchedule_JS_Guard&);
    CNetSchedule_JS_Guard& operator=(CNetSchedule_JS_Guard&);
private:
    CNetScheduler_JobStatusTracker&  m_Tracker;
    int                              m_OldStatus;
    unsigned int                     m_JobId;
};


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2005/03/09 17:37:17  kuznets
 * Added node notification thread and execution control timeline
 *
 * Revision 1.4  2005/03/04 12:06:41  kuznets
 * Implenyed UDP callback to clients
 *
 * Revision 1.3  2005/02/28 12:24:17  kuznets
 * New job status Returned, better error processing and queue management
 *
 * Revision 1.2  2005/02/23 19:16:38  kuznets
 * Implemented garbage collection thread
 *
 * Revision 1.1  2005/02/08 16:42:55  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */

#endif /* NETSCHEDULE_JOB_STATUS__HPP */

