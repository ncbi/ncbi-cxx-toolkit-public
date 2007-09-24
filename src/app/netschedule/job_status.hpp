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
 * Authors:  Anatoliy Kuznetsov, Victor Joukov
 *
 * File Description:
 *   Net schedule job status states.
 *
 */

/// @file job_status.hpp
/// NetSchedule job status tracker. 
///
/// @internal

#include <corelib/ncbimtx.hpp>

#include <connect/services/netschedule_api.hpp>

#include <map>

#include "ns_types.hpp"

BEGIN_NCBI_SCOPE


/// In-Memory storage to track status of all jobs
/// Syncronized thread safe class
///
/// @internal
class CJobStatusTracker
{
public:
    typedef vector<TNSBitVector*> TStatusStorage;

    /// Status to number of jobs map
    typedef map<CNetScheduleAPI::EJobStatus, unsigned> TStatusSummaryMap;

public:
    CJobStatusTracker();
    ~CJobStatusTracker();

    CNetScheduleAPI::EJobStatus 
        GetStatus(unsigned job_id) const;

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
    /// @param updated
    ///     (out) TRUE if database needs to be updated for job_id. 
    ///
    /// @return old status. Job may be already canceled (or failed)
    /// in this case status change is ignored
    /// 
    CNetScheduleAPI::EJobStatus
    ChangeStatus(unsigned                    job_id, 
                 CNetScheduleAPI::EJobStatus status,
                 bool*                       updated = NULL);

    /// Add closed interval of ids to pending status
    void AddPendingBatch(unsigned job_id_from, unsigned job_id_to);

    /// Return job id (job is moved from pending to running)
    /// @return job id, 0 - no pending jobs
    //unsigned GetPendingJob();

    /// Get pending job out of a certain candidate set
    /// Method cleans candidates if they are no longer available 
    /// for scheduling (it means job was already dispatched)
    /// 
    /// @param job_id 
    ///     OUT job id (moved from pending to running)
    ///     job_id is taken out of candidates 
    ///     When 0 - means no pending candidates
    /// @return true - if there are any available pending jobs
    ///         false - queue has no pending jobs whatsoever 
    ///         (not just candidates)
    bool GetPendingJobFromSet(TNSBitVector* candidate_set,
                              unsigned*     job_id);

    /// Get any pending job, but it should NOT be in the unwanted list
    /// Presumed, that unwanted jobs are speculatively assigned to other
    /// worker nodes or postponed
    bool GetPendingJob(const TNSBitVector& unwanted_jobs,
                       unsigned*           job_id);

    /// Logical AND of candidates and pending jobs
    /// (candidate_set &= pending_set)
    void PendingIntersect(TNSBitVector* candidate_set);

    /// Logical AND with statuses ORed cleans up a list from non-existing
    /// jobs
    void GetAliveJobs(TNSBitVector& ids);

    /// TRUE if we have pending jobs
    bool AnyPending() const;

    /// Returned jobs come back to pending area
    void Returned2Pending();

    /// Get first job id from DONE status
    unsigned GetFirstDone() const;

    /// Get first job in the specified status
    unsigned GetFirst(CNetScheduleAPI::EJobStatus  status) const;

    /// Set job status without logic control.
    /// @param status
    ///     Status to set (all other statuses are cleared)
    ///     Non existing status code clears all statuses
    void SetStatus(unsigned                     job_id, 
                   CNetScheduleAPI::EJobStatus  status);

    // Erase the job
    void Erase(unsigned job_id);

    /// Set job status without any protection 
    void SetExactStatusNoLock(
        unsigned                    job_id, 
        CNetScheduleAPI::EJobStatus status,
        bool                        set_clear);

    /// Return number of jobs in specified status
    unsigned CountStatus(CNetScheduleAPI::EJobStatus status) const;

    /// Count all status vectors using candidate_set(optional) as a mask 
    /// (AND_COUNT)
    void CountStatus(TStatusSummaryMap*  status_map, 
                     const TNSBitVector* candidate_set);

    /// Count all jobs in any status
    unsigned Count(void);

    void StatusStatistics(CNetScheduleAPI::EJobStatus status,
                          TNSBitVector::statistics*      st) const;

    /// Specified status is OR-ed with the target vector
    void StatusSnapshot(CNetScheduleAPI::EJobStatus status,
                        TNSBitVector*                  bv) const;

    static
    bool IsCancelCode(CNetScheduleAPI::EJobStatus status)
    {
        return (status == CNetScheduleAPI::eCanceled) ||
               (status == CNetScheduleAPI::eFailed);
    }

    /// Clear status storage
    ///
    /// @param bv
    ///    If not NULL all ids from the matrix are OR-ed with this vector 
    ///    (bv is not cleared)
    void ClearAll(TNSBitVector* bv = 0);

    /// Optimize bitvectors memory
    void OptimizeMem();

    void PrintStatusMatrix(CNcbiOstream& out) const;


protected:

    CNetScheduleAPI::EJobStatus 
        x_GetStatusNoLock(unsigned job_id) const;

    /// Check if job is in specified status
    /// @return -1 if no, status value otherwise
    CNetScheduleAPI::EJobStatus 
    IsStatusNoLock(unsigned         job_id, 
        CNetScheduleAPI::EJobStatus st1,
        CNetScheduleAPI::EJobStatus st2 = CNetScheduleAPI::eJobNotFound,
        CNetScheduleAPI::EJobStatus st3 = CNetScheduleAPI::eJobNotFound
        ) const;

    /// Check if job is in specified status and switch to new status
    /// @return TRUE if switched
    bool
    SwitchStatusNoLock(unsigned     job_id,
        CNetScheduleAPI::EJobStatus new_st,
        CNetScheduleAPI::EJobStatus st1,
        CNetScheduleAPI::EJobStatus st2 = CNetScheduleAPI::eJobNotFound,
        CNetScheduleAPI::EJobStatus st3 = CNetScheduleAPI::eJobNotFound
        );


    void ReportInvalidStatus(unsigned    job_id, 
             CNetScheduleAPI::EJobStatus status,
             CNetScheduleAPI::EJobStatus old_status);
    void x_SetClearStatusNoLock(unsigned job_id,
             CNetScheduleAPI::EJobStatus status,
             CNetScheduleAPI::EJobStatus old_status);

    void Returned2PendingNoLock();
    unsigned GetPendingJobNoLock();
    void FreeUnusedMemNoLock();
    void IncDoneJobs();

private:
    CJobStatusTracker(const CJobStatusTracker&);
    CJobStatusTracker& operator=(const CJobStatusTracker&);
private:

    TStatusStorage          m_StatusStor;
    mutable CRWLock         m_Lock;
    /// Last pending id
    bm::id_t                m_LastPending;
    /// Done jobs counter
    unsigned                m_DoneCnt;
};


/// @internal
class CNetSchedule_JS_Guard
{
public:
    CNetSchedule_JS_Guard(
        CJobStatusTracker&          strack,
        unsigned                    job_id,
        CNetScheduleAPI::EJobStatus status,
        bool*                       updated = 0)
     : m_Tracker(strack),
       m_OldStatus(-1),
       m_JobId(job_id)
    {

        if (job_id)
            m_OldStatus = strack.ChangeStatus(job_id, status, updated);
    }

    ~CNetSchedule_JS_Guard()
    {
        // roll back to the old status
        if (m_OldStatus >= -1) {
            m_Tracker.SetStatus(m_JobId, 
              (CNetScheduleAPI::EJobStatus) m_OldStatus);
        } 
    }

    void Commit() { m_OldStatus = -2; }

    CNetScheduleAPI::EJobStatus GetOldStatus() const
    {
        return (CNetScheduleAPI::EJobStatus) m_OldStatus;
    }


private:
    CNetSchedule_JS_Guard(const CNetSchedule_JS_Guard&);
    CNetSchedule_JS_Guard& operator=(CNetSchedule_JS_Guard&);
private:
    CJobStatusTracker& m_Tracker;
    int                m_OldStatus;
    unsigned           m_JobId;
};


END_NCBI_SCOPE

#endif /* NETSCHEDULE_JOB_STATUS__HPP */

