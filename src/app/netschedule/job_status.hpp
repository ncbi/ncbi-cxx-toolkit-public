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

typedef CNetScheduleAPI::EJobStatus TJobStatus;

/// In-Memory storage to track status of all jobs
/// Syncronized thread safe class
///
/// @internal
class CJobStatusTracker
{
public:
    typedef vector<TNSBitVector*> TStatusStorage;

    /// Status to number of jobs map
    typedef map<TJobStatus, unsigned> TStatusSummaryMap;

public:
    CJobStatusTracker();
    ~CJobStatusTracker();

    TJobStatus GetStatus(unsigned job_id) const;

    /// Set job status. (Controls status change logic)
    ///
    ///  Status switching rules, [] - means request ignored
    ///  ePending   <- eRunning, "no status"
    ///  eRunning   <- ePending, [eCanceled]
    ///  eCanceled  <- ePending, eRunning, [eDone, eFailed (ignored if job is ready)]
    ///  eFailed    <- eRunning, [eCanceled]
    ///  eDone      <- ePending, eRunning, [eCanceled, eFailed], eReading (timeout)
    ///  eReading   <- eDone
    ///  eConfirmed <- eReading, eDone
    ///  eReadFailed <- eReading
    ///
    /// @param updated
    ///     (out) TRUE if database needs to be updated for job_id.
    ///
    /// @return old status. Job may be already canceled (or failed)
    /// in this case status change is ignored
    ///
    TJobStatus
    ChangeStatus(unsigned   job_id,
                 TJobStatus status,
                 bool*      updated = NULL);

    /// Add closed interval of ids to pending status
    void AddPendingBatch(unsigned job_id_from, unsigned job_id_to);

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

    /// Group switch up to 'count' jobs, not including ones from
    /// 'unwanted_jobs', from old_status to new_status
    void SwitchJobs(unsigned count,
                    TJobStatus old_status, TJobStatus new_status,
                    TNSBitVector& jobs,
                    const TNSBitVector* unwanted_jobs = NULL);

    /// Logical AND with statuses ORed cleans up a list from non-existing
    /// jobs
    void GetAliveJobs(TNSBitVector& ids);

    /// TRUE if we have pending jobs
    bool AnyPending() const;

    /// Get first job id from DONE status
    unsigned GetFirstDone() const;

    /// Get first job in the specified status
    unsigned GetFirst(TJobStatus status) const;

     /// Get next job in the specified status, or first if job_id is 0
    unsigned GetNext(TJobStatus status, unsigned job_id) const;

   /// Set job status without logic control.
    /// @param status
    ///     Status to set (all other statuses are cleared)
    ///     Non existing status code clears all statuses
    void SetStatus(unsigned   job_id,
                   TJobStatus status);

    // Erase the job
    void Erase(unsigned job_id);

    /// Set job status without any protection
    void SetExactStatusNoLock(
        unsigned   job_id,
        TJobStatus status,
        bool       set_clear);

    /// Return number of jobs in specified status
    unsigned CountStatus(TJobStatus status) const;

    /// Count all status vectors using candidate_set(optional) as a mask
    /// (AND_COUNT)
    void CountStatus(TStatusSummaryMap*  status_map,
                     const TNSBitVector* candidate_set);

    /// Count all jobs in any status
    unsigned Count(void);

    void StatusStatistics(TJobStatus                status,
                          TNSBitVector::statistics* st) const;

    /// Specified status is OR-ed with the target vector
    void StatusSnapshot(TJobStatus    status,
                        TNSBitVector* bv) const;

    static bool IsCancelCode(TJobStatus status)
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

protected:
    TJobStatus x_GetStatusNoLock(unsigned job_id) const;

    /// Check if job is in specified status
    /// @return -1 if no, status value otherwise
    TJobStatus
    IsStatusNoLock(unsigned   job_id,
                   TJobStatus st1,
                   TJobStatus st2 = CNetScheduleAPI::eJobNotFound,
                   TJobStatus st3 = CNetScheduleAPI::eJobNotFound
        ) const;

    void ReportInvalidStatus(unsigned   job_id,
                             TJobStatus status,
                             TJobStatus old_status);
    void x_SetClearStatusNoLock(unsigned   job_id,
                                TJobStatus status,
                                TJobStatus old_status);

    void IncDoneJobs();

private:
    CJobStatusTracker(const CJobStatusTracker&);
    CJobStatusTracker& operator=(const CJobStatusTracker&);

private:
    //friend class CJSGuard;
    friend class CNetSchedule_JSGroupGuard;
    TStatusStorage          m_StatusStor;
    mutable CRWLock         m_Lock;
    TNSBitVector            m_UsedIds; /// id access lock

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
        CJobStatusTracker& strack,
        unsigned           job_id,
        TJobStatus         status,
        bool*              updated = 0)
     : m_Tracker(strack),
       m_OldStatus(-1),
       m_JobId(job_id),
       m_Updated(updated)
    {
        if (!job_id)
            return;
        m_OldStatus = strack.ChangeStatus(job_id, status, updated);
    }

    ~CNetSchedule_JS_Guard()
    {
        // roll back to the old status
        if (m_OldStatus >= -1) {
            m_Tracker.SetStatus(m_JobId, TJobStatus(m_OldStatus));
        }
    }

    void Commit()
    {
        m_OldStatus = -2;
    }

    void SetStatus(TJobStatus status)
    {
        m_Tracker.SetStatus(m_JobId, status);
    }

    TJobStatus GetOldStatus() const
    {
        return TJobStatus(m_OldStatus);
    }


private:
    CNetSchedule_JS_Guard(const CNetSchedule_JS_Guard&);
    CNetSchedule_JS_Guard& operator=(CNetSchedule_JS_Guard&);

private:
    CJobStatusTracker& m_Tracker;
    int                m_OldStatus;
    unsigned           m_JobId;
    bool*              m_Updated;
};


// External/internal guard for group operations on jobs.
// Suggested pattern is that you make an undoable change on
// status tracker yourself, put revert action into this guard
// immediately after it and when you're happy with results, Commit
// the guard releaves this undo info.
// Another way is to supply new_status to the guard, so that it
// switches status itself in constructor.
class CNetSchedule_JSGroupGuard
{
public:
    CNetSchedule_JSGroupGuard(
        CJobStatusTracker&  strack,
        TJobStatus          old_status,
        const TNSBitVector& jobs,
        TJobStatus          new_status = CNetScheduleAPI::eJobNotFound);

    ~CNetSchedule_JSGroupGuard();

    void Commit()
    {
        m_Commited = true;
        m_Jobs.clear();
    }

private:
    CJobStatusTracker& m_Tracker;
    bool               m_Commited;
    TJobStatus         m_OldStatus;
    TJobStatus         m_NewStatusHint;
    TNSBitVector       m_Jobs;
};


/* Not finished yet
class CJSGuard
{
public:
    CJSGuard(CJobStatusTracker& strack,
             unsigned           job_id,
             TJobStatus         status,
             int                timeout_ms = -1);
    ~CJSGuard()
    {
        Release();
    }

    void Commit()
    {
        CWriteLockGuard guard(m_Tracker.m_Lock);
    }

    void Release()
    {
        CWriteLockGuard guard(m_Tracker.m_Lock);
        m_Tracker.m_UsedIds.set(m_JobId, false);
    }

private:
    CJSGuard(const CJSGuard&);
    CJSGuard& operator=(const CJSGuard&);

private:
    CJobStatusTracker& m_Tracker;
    unsigned int       m_JobId;
    TJobStatus         m_OldStatus;
    TJobStatus         m_NewStatus;
};
*/
/*
/// Mutex to guard vector of busy IDs
DEFINE_STATIC_FAST_MUTEX(x_NetSchedulerMutex_BusyID);


/// Class guards the id, guarantees exclusive access to the object
///
/// @internal
///
class CIdBusyGuard
{
public:
    CIdBusyGuard(TNSBitVector* id_set,
                 unsigned int  id,
                 unsigned      timeout)
        : m_IdSet(id_set), m_Id(id)
    {
        _ASSERT(id);
        unsigned cnt = 0; unsigned sleep_ms = 10;
        while (true) {
            {{
            CFastMutexGuard guard(x_NetSchedulerMutex_BusyID);
            if (!(*id_set)[id]) {
                id_set->set(id);
                break;
            }
            }}
            cnt += sleep_ms;
            if (cnt > timeout * 1000) {
                NCBI_THROW(CNetServiceException,
                           eTimeout, "Failed to lock object");
            }
            SleepMilliSec(sleep_ms);
        } // while
    }

    ~CIdBusyGuard()
    {
        Release();
    }

    void Release()
    {
        if (m_Id) {
            CFastMutexGuard guard(x_NetSchedulerMutex_BusyID);
            m_IdSet->set(m_Id, false);
            m_Id = 0;
        }
    }
private:
    CIdBusyGuard(const CIdBusyGuard&);
    CIdBusyGuard& operator=(const CIdBusyGuard&);
private:
    TNSBitVector* m_IdSet;
    unsigned int  m_Id;
};

*/

END_NCBI_SCOPE

#endif /* NETSCHEDULE_JOB_STATUS__HPP */

