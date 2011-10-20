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
 * Authors:  Victor Joukov
 *
 * File Description:
 *   NetSchedule queue structure and parameters
 */
#include <ncbi_pch.hpp>

#include "ns_queue.hpp"
#include "ns_queue_param_accessor.hpp"
#include "background_host.hpp"
#include "ns_util.hpp"
#include "ns_format.hpp"
#include "ns_server.hpp"
#include "ns_handler.hpp"

#include <corelib/ncbi_system.hpp> // SleepMilliSec
#include <corelib/request_ctx.hpp>
#include <db/bdb/bdb_trans.hpp>
#include <util/qparse/query_parse.hpp>
#include <util/qparse/query_exec.hpp>
#include <util/qparse/query_exec_bv.hpp>
#include <util/bitset/bmalgo.h>


BEGIN_NCBI_SCOPE



//////////////////////////////////////////////////////////////////////////
CQueueEnumCursor::CQueueEnumCursor(CQueue* queue)
  : CBDB_FileCursor(queue->m_QueueDbBlock->job_db)
{
    SetCondition(CBDB_FileCursor::eFirst);
}


//////////////////////////////////////////////////////////////////////////
// CQueue


// Used together with m_SavedId. m_SavedId is saved in a DB and is used
// as to start from value for the restarted neschedule.
// s_ReserveDelta value is used to avoid to often DB updates
static const unsigned int       s_ReserveDelta = 10000;


CQueue::CQueue(CRequestExecutor&     executor,
               const string&         queue_name,
               const string&         qclass_name,
               TQueueKind            queue_kind,
               CNetScheduleServer *  server)
  :
    m_RunTimeLine(NULL),
    m_NotificationsList(queue_name),
    m_DeleteDatabase(false),
    m_Executor(executor),
    m_QueueName(queue_name),
    m_QueueClass(qclass_name),
    m_Kind(queue_kind),
    m_QueueDbBlock(0),

    m_BecameEmpty(-1),

    m_LastId(0),
    m_SavedId(s_ReserveDelta),

    m_ParamLock(CRWLock::fFavorWriters),
    m_Timeout(3600),
    m_NotifyTimeout(0.1),
    m_RunTimeout(3600),
    m_RunTimeoutPrecision(-1),
    m_FailedRetries(0),
    m_BlacklistTime(0),
    m_EmptyLifetime(0),
    m_MaxInputSize(kNetScheduleMaxDBDataSize),
    m_MaxOutputSize(kNetScheduleMaxDBDataSize),
    m_DenyAccessViolations(false),
    m_KeyGenerator(server->GetHost(), server->GetPort()),
    m_Log(server->IsLog()),
    m_LogBatchEachJob(server->IsLogBatchEachJob())
{
    _ASSERT(!queue_name.empty());
    for (TStatEvent n = 0; n < eStatNumEvents; ++n) {
        m_EventCounter[n].Set(0);
        m_Average[n] = 0;
    }
    m_StatThread.Reset(new CStatisticsThread(*this,
                                             server->IsLogStatisticsThread()));
    m_StatThread->Run();
}


CQueue::~CQueue()
{
    delete m_RunTimeLine;
    Detach();
    m_StatThread->RequestStop();
    m_StatThread->Join(NULL);
}


void CQueue::Attach(SQueueDbBlock* block)
{
    Detach();
    m_QueueDbBlock = block;
    m_AffinityRegistry.Attach(&m_QueueDbBlock->aff_dict_db);

    // Here we have a db, so we can read the counter value we should start from
    m_LastId = x_ReadStartFromCounter();
    m_SavedId = m_LastId + s_ReserveDelta;
    if (m_SavedId < m_LastId) {
        // Overflow
        m_LastId = 0;
        m_SavedId = s_ReserveDelta;
    }
    x_UpdateStartFromCounter();
}


class CTruncateRequest : public CStdRequest
{
public:
    CTruncateRequest(SQueueDbBlock* qdbblock)
        : m_QueueDbBlock(qdbblock)
    {}

    virtual void Process()
    {
        // See CQueue::Detach why we can do this without locking.
        CStopWatch      sw(CStopWatch::eStart);
        m_QueueDbBlock->Truncate();
        m_QueueDbBlock->allocated = false;
        LOG_POST(Info << "Clean up of db block "
                      << m_QueueDbBlock->pos
                      << " complete, "
                      << sw.Elapsed()
                      << " elapsed");
    }
private:
    SQueueDbBlock* m_QueueDbBlock;

};


void CQueue::Detach()
{
    // We are here have synchronized access to m_QueueDbBlock without mutex
    // because we are here only when the last reference to CQueue is
    // destroyed. So as long m_QueueDbBlock->allocated is true it cannot
    // be allocated again and thus cannot be accessed as well.
    // As soon as we're done with detaching (here) or truncating (in separate
    // request, submitted for execution to the server thread pool), we can
    // set m_QueueDbBlock->allocated to false. Boolean write is atomic by
    // definition and test-and-set is executed from synchronized code in
    // CQueueDbBlockArray::Allocate.
    m_AffinityRegistry.Detach();
    if (!m_QueueDbBlock)
        return;

    if (m_DeleteDatabase) {
        CRef<CStdRequest> request(new CTruncateRequest(m_QueueDbBlock));
        m_Executor.SubmitRequest(request);
        m_DeleteDatabase = false;
    } else
        m_QueueDbBlock->allocated = false;
    m_QueueDbBlock = 0;
}


void CQueue::SetParameters(const SQueueParameters &  params)
{
    // When modifying this, modify all places marked with PARAMETERS
    CWriteLockGuard     guard(m_ParamLock);

    m_Timeout       = params.timeout;
    m_NotifyTimeout = params.notif_timeout;

    m_RunTimeout = params.run_timeout;
    if (params.run_timeout && !m_RunTimeLine) {
        // One time only. Precision can not be reset.
        m_RunTimeLine =
            new CJobTimeLine(params.run_timeout_precision, 0);
        m_RunTimeoutPrecision = params.run_timeout_precision;
    }

    m_FailedRetries        = params.failed_retries;
    m_BlacklistTime        = params.blacklist_time;
    m_EmptyLifetime        = params.empty_lifetime;
    m_MaxInputSize         = params.max_input_size;
    m_MaxOutputSize        = params.max_output_size;
    m_DenyAccessViolations = params.deny_access_violations;

    // program version control
    m_ProgramVersionList.Clear();
    if (!params.program_name.empty()) {
        m_ProgramVersionList.AddClientInfo(params.program_name);
    }
    m_SubmHosts.SetHosts(params.subm_hosts);
    m_WnodeHosts.SetHosts(params.wnode_hosts);
}


CQueue::TParameterList CQueue::GetParameters() const
{
    TParameterList          parameters;
    CQueueParamAccessor     qp(*this);
    unsigned                nParams = qp.GetNumParams();

    for (unsigned n = 0; n < nParams; ++n) {
        parameters.push_back(
            pair<string, string>(qp.GetParamName(n), qp.GetParamValue(n)));
    }
    return parameters;
}


unsigned CQueue::LoadStatusMatrix()
{
    m_QueueDbBlock->deleted_jobs_db.id = 0;
    EBDB_ErrCode    err = m_QueueDbBlock->deleted_jobs_db.ReadVector(&m_JobsToDelete);
    if (err != eBDB_Ok && err != eBDB_NotFound) {
        // TODO: throw db error
    }
    m_JobsToDelete.optimize();

    // Load th known affinities
    m_AffinityRegistry.LoadAffinityDictionary();

    // scan the queue, load the state machine from DB
    CBDB_FileCursor     cur(m_QueueDbBlock->job_db);

    cur.InitMultiFetch(1024*1024);
    cur.SetCondition(CBDB_FileCursor::eGE);
    cur.From << 0;

    unsigned    recs = 0;

    for (; cur.Fetch() == eBDB_Ok; ) {
        unsigned int    job_id = m_QueueDbBlock->job_db.id;

        if (m_JobsToDelete.test(job_id)) {
            x_DeleteJobEvents(job_id);
            continue;
        }

        TJobStatus      status = TJobStatus(static_cast<int>(m_QueueDbBlock->job_db.status));
        m_StatusTracker.SetExactStatusNoLock(job_id, status, true);

        if ((status == CNetScheduleAPI::eRunning ||
             status == CNetScheduleAPI::eReading) &&
            m_RunTimeLine) {
            // Add object to the first available slot;
            // it is going to be rescheduled or dropped
            // in the background control thread
            // We can use time line without lock here because
            // the queue is still in single-use mode while
            // being loaded.
            m_RunTimeLine->AddObject(m_RunTimeLine->GetHead(), job_id);
        }

        // Register the job for the affinity if so
        if (m_QueueDbBlock->job_db.aff_id != 0)
            m_AffinityRegistry.AddJobToAffinity(job_id,
                                                m_QueueDbBlock->job_db.aff_id);

        ++recs;
    }

    // Make sure that there are no affinity IDs in the registry for which there
    // are no jobs and initialize the next affinity ID counter.
    m_AffinityRegistry.FinalizeAffinityDictionaryLoading();

    return recs;
}


// Used to log a job submit.
// if batch_submit == true => it was a bach submit and batch_id should also
// be logged.
void CQueue::x_LogSubmit(const CJob &   job,
                         unsigned int   batch_id,
                         bool           batch_submit)
{
    if (!m_Log)
        return;

    CRef<CRequestContext>   current_context(& CDiagContext::GetRequestContext());
    bool                    log_batch_each_job = m_LogBatchEachJob;
    bool                    create_sep_request = batch_submit && log_batch_each_job;

    if (create_sep_request) {
        CRequestContext *   ctx(new CRequestContext());
        ctx->SetRequestID();
        GetDiagContext().SetRequestContext(ctx);
        GetDiagContext().PrintRequestStart()
                        .Print("_type", "cmd")
                        .Print("cmd", "BATCH-SUBMIT")
                        .Print("batch_id", batch_id);
        ctx->SetRequestStatus(CNetScheduleHandler::eStatus_OK);
    }

    if (!batch_submit || log_batch_each_job) {
        CDiagContext_Extra extra = GetDiagContext().Extra()
            .Print("job_key", MakeKey(job.GetId()))
            .Print("queue", GetQueueName())
            .Print("input", job.GetInput())
            .Print("aff", m_AffinityRegistry.GetTokenByID(job.GetAffinityId()))
            .Print("mask", job.GetMask())
            .Print("subm_addr", CSocketAPI::gethostbyaddr(job.GetSubmAddr()))
            .Print("subm_notif_port", job.GetSubmNotifPort())
            .Print("subm_notif_timeout", job.GetSubmNotifTimeout())
            .Print("timeout", NStr::ULongToString(job.GetTimeout()))
            .Print("run_timeout", job.GetRunTimeout());

        const string &  progress_msg = job.GetProgressMsg();
        if (!progress_msg.empty())
            extra.Print("progress_msg", progress_msg);
        extra.Flush();
    }

    if (create_sep_request) {
        GetDiagContext().PrintRequestStop();
        GetDiagContext().SetRequestContext(current_context);
    }
}


unsigned int  CQueue::Submit(const CNSClientId &  client,
                             CJob &               job,
                             const string &       aff_token)
{
    // the only config parameter used here is the max input size so there is no
    // need to have a safe parameters accessor.

    if (job.GetInput().size() > m_MaxInputSize)
        NCBI_THROW(CNetScheduleException, eDataTooLong, "Input is too long");

    unsigned int    aff_id = 0;
    unsigned int    job_id = GetNextId();
    CJobEvent &     event = job.AppendEvent();

    job.SetId(job_id);
    job.SetPassport(rand());

    event.SetNodeAddr(client.GetAddress());
    event.SetStatus(CNetScheduleAPI::ePending);
    event.SetEvent(CJobEvent::eSubmit);
    event.SetTimestamp(time(0));
    event.SetClientNode(client.GetNode());
    event.SetClientSession(client.GetSession());

    // Special treatment for system job masks
    if (job.GetMask() & CNetScheduleAPI::eOutOfOrder)
    {
        // NOT IMPLEMENTED YET: put job id into OutOfOrder list.
        // The idea is that there can be an urgent job, which
        // should be executed before jobs which were submitted
        // earlier, e.g. for some administrative purposes. See
        // CNetScheduleAPI::EJobMask in file netschedule_api.hpp
    }

    // Take the queue lock and start the operation
    {{
        bool                was_empty = false;
        CFastMutexGuard     guard(m_OperationLock);

        was_empty = !m_StatusTracker.AnyPending();
        {{
            CNSTransaction      transaction(this);

            aff_id = m_AffinityRegistry.ResolveAffinityToken(aff_token, job_id);
            job.SetAffinityId(aff_id);
            job.Flush(this);

            transaction.Commit();
        }}

        m_StatusTracker.SetStatus(job_id, CNetScheduleAPI::ePending);

        // Register the job with the client
        m_ClientsRegistry.AddToSubmitted(client, job_id);

        // Make the decision whether to send or not a notification
        if (aff_id == 0) {
            // Send only if there were no pending jobs
            if (was_empty)
                NotifyListeners(true, 0);
        } else {
            // Send only if there were no pending with this affinity
            TNSBitVector    jobs_with_aff = m_AffinityRegistry.GetJobsWithAffinity(aff_id);

            // Reset the current job_id bit
            jobs_with_aff.set(job_id, false);
            if (!jobs_with_aff.any())
                NotifyListeners(true, aff_id);
            else {
                m_StatusTracker.PendingIntersect(&jobs_with_aff);
                if (!jobs_with_aff.any())
                    NotifyListeners(true, aff_id);
            }
        }
    }}

    x_LogSubmit(job, 0, false);
    return job_id;
}


unsigned int  CQueue::SubmitBatch(const CNSClientId &             client,
                                  vector< pair<CJob, string> > &  batch)
{
    unsigned int    max_input_size;
    unsigned int    batch_size = batch.size();
    unsigned int    job_id = GetNextIdBatch(batch_size);
    size_t          aff_count = 0;

    {{
        CQueueParamAccessor     qp(*this);
        max_input_size = qp.GetMaxInputSize();
    }}

    {{
        unsigned int        job_id_cnt = job_id;
        time_t              curr_time = time(0);

        CFastMutexGuard     guard(m_OperationLock);
        bool                was_empty = !m_StatusTracker.AnyPending();

        {{
            CNSTransaction      transaction(this);

            for (size_t  k = 0; k < batch_size; ++k) {

                CJob &              job = batch[k].first;
                const string &      aff_token = batch[k].second;
                CJobEvent &         event = job.AppendEvent();

                job.SetId(job_id_cnt);
                job.SetPassport(rand());

                event.SetNodeAddr(client.GetAddress());
                event.SetStatus(CNetScheduleAPI::ePending);
                event.SetEvent(CJobEvent::eBatchSubmit);
                event.SetTimestamp(curr_time);
                event.SetClientNode(client.GetNode());
                event.SetClientSession(client.GetSession());

                if (!aff_token.empty()) {
                    job.SetAffinityId(
                        m_AffinityRegistry.ResolveAffinityToken(
                            aff_token, job_id_cnt)
                                     );
                    ++aff_count;
                }
                job.Flush(this);
                ++job_id_cnt;
            }

            transaction.Commit();
        }}

        m_StatusTracker.AddPendingBatch(job_id, job_id + batch_size - 1);
        m_ClientsRegistry.AddToSubmitted(client, job_id, batch_size);

        // Make a decision whether to notify clients or not
        if (aff_count == 0) {
            // Send only if there were no pending jobs
            if (was_empty)
                NotifyListeners(true, 0);
        } else
            m_NotificationsList.NotifySomeAffinity();
    }}

    CountEvent(CQueue::eStatDBWriteEvent, batch_size);

    for (size_t  k= 0; k < batch_size; ++k)
        x_LogSubmit(batch[k].first, job_id, true);

    return job_id;
}



void CQueue::PutResultGetJob(const CNSClientId &        client,
                             // PutResult parameters
                             unsigned int               done_job_id,
                             int                        ret_code,
                             const string *             output,
                             // GetJob parameters
                             // in
                             const list<string> *       aff_list,
                             // out
                             CJob *                     new_job)
{
    time_t  curr = time(0);

    PutResult(client, curr, done_job_id, ret_code, output);
    GetJob(client, curr, aff_list, new_job);
}


static const size_t     k_max_dead_locks = 10;  // max. dead lock repeats

void CQueue::PutResult(const CNSClientId &  client,
                       time_t               curr,
                       unsigned int         job_id,
                       int                  ret_code,
                       const string *       output)
{
    _ASSERT(job_id && output);

    // The only one parameter (max output size) is required for the put
    // operation so there is no need to use CQueueParamAccessor

    if (output->size() > m_MaxOutputSize)
        NCBI_THROW(CNetScheduleException, eDataTooLong,
                   "Output is too long");

    CJob                job;
    size_t              dead_locks = 0; // dead lock counter

    for (;;) {
        try {
            CFastMutexGuard     guard(m_OperationLock);
            TJobStatus          old_status = GetJobStatus(job_id);

            if (old_status != CNetScheduleAPI::ePending &&
                old_status != CNetScheduleAPI::eRunning)
                return;

            {{
                CNSTransaction      transaction(this);
                x_UpdateDB_PutResultNoLock(job_id, curr,
                                           ret_code, *output, job,
                                           client);
                transaction.Commit();
            }}

            m_StatusTracker.ChangeStatus(job_id, CNetScheduleAPI::eDone);
            m_ClientsRegistry.ClearExecuting(job_id);
            TimeLineRemove(job_id);

            if (job.ShouldNotify(curr))
                m_NotificationsList.NotifyJobStatus(job.GetSubmAddr(),
                                                    job.GetSubmNotifPort(),
                                                    MakeKey(job_id));
            return;
        }
        catch (CBDB_ErrnoException &  ex) {
            if (ex.IsDeadLock()) {
                if (++dead_locks < k_max_dead_locks) {
                    ERR_POST("DeadLock repeat in CQueue::JobExchange");
                    SleepMilliSec(100);
                    continue;
                }
                ERR_POST("Too many transaction repeats in CQueue::JobExchange.");
            } else if (ex.IsNoMem()) {
                if (++dead_locks < k_max_dead_locks) {
                    ERR_POST("No resource repeat in CQueue::JobExchange");
                    SleepMilliSec(100);
                    continue;
                }
                ERR_POST("Too many transaction repeats in CQueue::JobExchange.");
            }
            throw;
        }
    }
}


void CQueue::GetJob(const CNSClientId &     client,
                    time_t                  curr,
                    const list<string> *    aff_list,
                    CJob *                  new_job)
{
    unsigned    run_timeout;
    size_t      dead_locks = 0; // dead lock counter

    {{
        CQueueParamAccessor     qp(*this);

        run_timeout = qp.GetRunTimeout();
    }}

    for (;;) {
        try {
            CFastMutexGuard     guard(m_OperationLock);
            unsigned int        job_id = FindPendingJob(client,
                                                        *aff_list, curr);
            if (!job_id)
                return;

            job_id =  x_UpdateDB_GetJobNoLock(client, curr,
                                              job_id, *new_job);

            if (job_id) {
                m_StatusTracker.ChangeStatus(job_id, CNetScheduleAPI::eRunning);
                TimeLineAdd(job_id, curr + run_timeout);
                m_ClientsRegistry.AddToRunning(client, job_id);
            }
            return;
        }
        catch (CBDB_ErrnoException &  ex) {
            if (ex.IsDeadLock()) {
                if (++dead_locks < k_max_dead_locks) {
                    ERR_POST("DeadLock repeat in CQueue::JobExchange");
                    SleepMilliSec(100);
                    continue;
                }
                ERR_POST("Too many transaction repeats in CQueue::JobExchange.");
            } else if (ex.IsNoMem()) {
                if (++dead_locks < k_max_dead_locks) {
                    ERR_POST("No resource repeat in CQueue::JobExchange");
                    SleepMilliSec(100);
                    continue;
                }
                ERR_POST("Too many transaction repeats in CQueue::JobExchange.");
            }
            throw;
        }
    }
}


void CQueue::JobDelayExpiration(unsigned int     job_id,
                                time_t           tm)
{
    if (tm <= 0)
        return;

    unsigned            queue_run_timeout = GetRunTimeout();
    time_t              run_timeout = 0;
    time_t              time_start = 0;
    time_t              exp_time = 0;
    time_t              curr = time(0);
    bool                job_updated = false;
    CJob                job;

    {{
        CFastMutexGuard     guard(m_OperationLock);

        if (GetJobStatus(job_id) != CNetScheduleAPI::eRunning)
            return;


        {{
            CNSTransaction      transaction(this);

            if (job.Fetch(this, job_id) != CJob::eJF_Ok)
                return;

            CJobEvent *         event = &job.AppendEvent();

            time_start = event->GetTimestamp();
            if (time_start == 0) {
                // Impossible
                ERR_POST("Internal error: time_start == 0 for running job " <<
                         DecorateJobId(job_id));
                // Fix it just in case
                time_start = curr;
                event->SetTimestamp(curr);
                job_updated = true;
            }
            run_timeout = job.GetRunTimeout();
            if (run_timeout == 0)
                run_timeout = queue_run_timeout;

            if (time_start + run_timeout > curr + tm) {
                // Old timeout is enough to cover this request, keep it.
                // If we already changed job object (fixing it), we flush it.
                if (job_updated)
                    job.Flush(this);
                return;
            }

            job.SetRunTimeout(curr + tm - time_start);
            job.Flush(this);
            transaction.Commit();
        }}

        exp_time = run_timeout == 0 ? 0 : time_start + run_timeout;

        TimeLineMove(job_id, exp_time, curr + tm);
    }}

    if (m_Log) {
        CTime       tmp_t(GetFastLocalTime());
        tmp_t.SetTimeT(curr + tm);
        tmp_t.ToLocalTime();
        LOG_POST(Error << "Job: " << DecorateJobId(job_id)
                       << " new_expiration_time: " << tmp_t.AsString()
                       << " job_timeout(sec): " << run_timeout);
    }
    return;
}


bool CQueue::PutProgressMessage(unsigned int    job_id,
                                const string &  msg)
{
    CJob                job;
    CFastMutexGuard     guard(m_OperationLock);

    {{
        CNSTransaction      transaction(this);

        if (job.Fetch(this, job_id) != CJob::eJF_Ok)
            return false;

        job.SetProgressMsg(msg);
        job.Flush(this);
        transaction.Commit();
    }}
    return true;
}


bool  CQueue::ReturnJob(const CNSClientId &     client,
                        unsigned int            job_id)
{
    // FIXME: Provide fallback to
    // RegisterWorkerNodeVisit if unsuccessful
    if (!job_id)
        NCBI_THROW(CNetScheduleException, eInvalidParameter, "Invalid job ID");

    CJob                job;
    CFastMutexGuard     guard(m_OperationLock);

    if (GetJobStatus(job_id) != CNetScheduleAPI::eRunning)
        return true;

    {{
         CNSTransaction      transaction(this);

        if (job.Fetch(this, job_id) != CJob::eJF_Ok)
            return false;   // Job not found


        unsigned int    run_count = job.GetRunCount();
        CJobEvent *     event = job.GetLastEvent();

        if (!event)
            ERR_POST("No JobEvent for running job " << DecorateJobId(job_id));

        event = &job.AppendEvent();
        event->SetNodeAddr(client.GetAddress());
        event->SetStatus(CNetScheduleAPI::ePending);
        event->SetEvent(CJobEvent::eReturn);
        event->SetTimestamp(time(0));
        event->SetClientNode(client.GetNode());
        event->SetClientSession(client.GetSession());

        if (run_count)
            job.SetRunCount(run_count-1);

        job.SetStatus(CNetScheduleAPI::ePending);
        job.Flush(this);

        transaction.Commit();
    }}

    m_StatusTracker.ChangeStatus(job_id, CNetScheduleAPI::ePending);
    TimeLineRemove(job_id);
    m_ClientsRegistry.ClearExecuting(job_id);
    m_ClientsRegistry.AddToBlacklist(client, job_id);
    return true;
}


unsigned int  CQueue::ReadJobFromDB(unsigned int  job_id, CJob &  job)
{
    CJob::EJobFetchResult   res;

    {{
        CFastMutexGuard     guard(m_OperationLock);

        m_QueueDbBlock->job_db.SetTransaction(NULL);
        m_QueueDbBlock->events_db.SetTransaction(NULL);
        m_QueueDbBlock->job_info_db.SetTransaction(NULL);
        res = job.Fetch(this, job_id);
    }}

    if (res == CJob::eJF_Ok)
        return job_id;
    return 0;
}


// Deletes all the jobs from the queue
void CQueue::Truncate(void)
{
    TNSBitVector bv;

    {{
        CFastMutexGuard     guard(m_OperationLock);

        m_StatusTracker.ClearAll(&bv);
        m_AffinityRegistry.ClearMemoryAndDatabase();

        CWriteLockGuard     rtl_guard(m_RunTimeLineLock);
        m_RunTimeLine->ReInit(0);
    }}

    Erase(bv);

    // Next call updates 'm_BecameEmpty' timestamp
    IsExpired(); // locks CQueue lock
}


TJobStatus  CQueue::Cancel(const CNSClientId &  client,
                           unsigned int         job_id)
{
    TJobStatus          old_status;
    CJob                job;

    {{
        CFastMutexGuard     guard(m_OperationLock);

        old_status = m_StatusTracker.GetStatus(job_id);
        if (old_status == CNetScheduleAPI::eJobNotFound ||
            old_status == CNetScheduleAPI::eCanceled) {
            return old_status;
        }

        {{
            CNSTransaction      transaction(this);
            if (job.Fetch(this, job_id) != CJob::eJF_Ok)
                return CNetScheduleAPI::eJobNotFound;

            CJobEvent *     event = &job.AppendEvent();

            event->SetNodeAddr(client.GetAddress());
            event->SetStatus(CNetScheduleAPI::eCanceled);
            event->SetEvent(CJobEvent::eCancel);
            event->SetTimestamp(time(0));
            event->SetClientNode(client.GetNode());
            event->SetClientSession(client.GetSession());

            job.SetStatus(CNetScheduleAPI::eCanceled);
            job.Flush(this);

            transaction.Commit();
        }}

        m_StatusTracker.ChangeStatus(job_id, CNetScheduleAPI::eCanceled);

        TimeLineRemove(job_id);
        if (old_status == CNetScheduleAPI::eRunning)
            m_ClientsRegistry.ClearExecuting(job_id);
        else if (old_status == CNetScheduleAPI::eReading)
            m_ClientsRegistry.ClearReading(job_id);

    }}

    if ((old_status == CNetScheduleAPI::eRunning ||
         old_status == CNetScheduleAPI::ePending) && job.ShouldNotify(time(0)))
        m_NotificationsList.NotifyJobStatus(job.GetSubmAddr(),
                                            job.GetSubmNotifPort(),
                                            MakeKey(job_id));

    return old_status;
}


TJobStatus  CQueue::GetJobStatus(unsigned int  job_id) const
{
    return m_StatusTracker.GetStatus(job_id);
}


bool CQueue::CountStatus(CJobStatusTracker::TStatusSummaryMap*  status_map,
                         const string &                         affinity_token)
{
    unsigned        aff_id = 0;
    TNSBitVector    aff_jobs;

    if (!affinity_token.empty()) {
        aff_id = m_AffinityRegistry.GetIDByToken(affinity_token);
        if (aff_id == 0)
            return false;

        aff_jobs = m_AffinityRegistry.GetJobsWithAffinity(aff_id);
    }

    m_StatusTracker.CountStatus(status_map, aff_id!=0 ? &aff_jobs : 0);
    return true;
}


bool CQueue::IsExpired()
{
    time_t              empty_lifetime = GetEmptyLifetime();
    CFastMutexGuard     guard(m_OperationLock);

    if (m_Kind && empty_lifetime > 0) {
        if (m_StatusTracker.Count()) {
            m_BecameEmpty = -1;
        } else {
            if (m_BecameEmpty != -1 &&
                m_BecameEmpty + empty_lifetime < time(0))
            {
                LOG_POST(Info << "Queue " << m_QueueName << " expired."
                              << " Became empty: "
                              << CTime(m_BecameEmpty).ToLocalTime().AsString()
                              << " Empty lifetime: " << empty_lifetime
                              << " sec." );
                return true;
            }
            if (m_BecameEmpty == -1)
                m_BecameEmpty = time(0);
        }
    }
    return false;
}


unsigned int CQueue::GetNextId()
{
    CFastMutexGuard     guard(m_lastIdLock);

    // Job indexes are expected to start from 1,
    // the m_LastId is 0 at the very beginning
    ++m_LastId;
    if (m_LastId >= m_SavedId) {
        m_SavedId += s_ReserveDelta;
        if (m_SavedId < m_LastId) {
            // Overflow for the saved ID
            m_LastId = 1;
            m_SavedId = s_ReserveDelta;
        }
        x_UpdateStartFromCounter();
    }
    return m_LastId;
}


unsigned int CQueue::GetNextIdBatch(unsigned int  count)
{
    CFastMutexGuard     guard(m_lastIdLock);

    // Job indexes are expected to start from 1 and be monotonously growing
    unsigned int    start_index = m_LastId;

    m_LastId += count;
    if (m_LastId < start_index ) {
        // Overflow
        m_LastId = count;
        m_SavedId = count + s_ReserveDelta;
        x_UpdateStartFromCounter();
    }

    // There were no overflow, check the reserved value
    if (m_LastId >= m_SavedId) {
        m_SavedId += s_ReserveDelta;
        if (m_SavedId < m_LastId) {
            // Overflow for the saved ID
            m_LastId = count;
            m_SavedId = count + s_ReserveDelta;
        }
        x_UpdateStartFromCounter();
    }

    return m_LastId - count + 1;
}


void CQueue::x_UpdateStartFromCounter(void)
{
    // Updates the start_from value in the Berkley DB file
    if (m_QueueDbBlock) {
        // The only record is expected and its key is always 1
        m_QueueDbBlock->start_from_db.pseudo_key = 1;
        m_QueueDbBlock->start_from_db.start_from = m_SavedId;
        m_QueueDbBlock->start_from_db.UpdateInsert();
    }
    return;
}


unsigned int  CQueue::x_ReadStartFromCounter(void)
{
    // Reads the start_from value from the Berkley DB file
    if (m_QueueDbBlock) {
        // The only record is expected and its key is always 1
        m_QueueDbBlock->start_from_db.pseudo_key = 1;
        m_QueueDbBlock->start_from_db.Fetch();
        return m_QueueDbBlock->start_from_db.start_from;
    }
    return 1;
}


void CQueue::GetJobForReading(const CNSClientId &   client,
                              unsigned int          read_timeout,
                              CJob *                job)
{
    time_t              curr(time(0));

    CFastMutexGuard     guard(m_OperationLock);

    TNSBitVector        unwanted_jobs = m_ClientsRegistry.GetBlacklistedJobs(client);
    unsigned int        job_id = m_StatusTracker.GetJobByStatus(
                                                CNetScheduleAPI::eDone,
                                                unwanted_jobs);

    if (!job_id)
        return;

    {{
         CNSTransaction     transaction(this);

        // Fetch the job and update it and flush
        job->Fetch(this, job_id);

        unsigned int    read_count = job->GetReadCount() + 1;
        CJobEvent *     event = &job->AppendEvent();

        event->SetStatus(CNetScheduleAPI::eReading);
        event->SetEvent(CJobEvent::eRead);
        event->SetTimestamp(curr);
        event->SetNodeAddr(client.GetAddress());
        event->SetClientNode(client.GetNode());
        event->SetClientSession(client.GetSession());

        job->SetRunTimeout(read_timeout);
        job->SetStatus(CNetScheduleAPI::eReading);
        job->SetReadCount(read_count);
        job->Flush(this);

        transaction.Commit();
    }}

    if (read_timeout == 0)
        read_timeout = m_RunTimeout;

    if (read_timeout != 0)
        TimeLineAdd(job_id, curr + read_timeout);

    // Update the memory cache
    m_StatusTracker.ChangeStatus(job_id, CNetScheduleAPI::eReading);
    m_ClientsRegistry.AddToReading(client, job_id);
    return;
}


void CQueue::ConfirmReadingJob(const CNSClientId &  client,
                               unsigned int         job_id,
                               const string &       auth_token)
{
    x_ChangeReadingStatus(client, job_id, auth_token,
                          CNetScheduleAPI::eConfirmed);
    m_ClientsRegistry.ClearReading(client, job_id);
}


void CQueue::FailReadingJob(const CNSClientId &  client,
                            unsigned int         job_id,
                            const string &       auth_token)
{
    x_ChangeReadingStatus(client, job_id, auth_token,
                          CNetScheduleAPI::eReadFailed);
    m_ClientsRegistry.ClearReadingSetBlacklist(job_id);
}


void CQueue::ReturnReadingJob(const CNSClientId &  client,
                              unsigned int         job_id,
                              const string &       auth_token)
{
    x_ChangeReadingStatus(client, job_id, auth_token,
                          CNetScheduleAPI::eDone);
    m_ClientsRegistry.ClearReadingSetBlacklist(job_id);
}


void CQueue::x_ChangeReadingStatus(const CNSClientId &  client,
                                   unsigned int         job_id,
                                   const string &       auth_token,
                                   TJobStatus           target_status)
{
    CJob                job;
    TJobStatus          new_status = target_status;
    CFastMutexGuard     guard(m_OperationLock);


    {{
        CNSTransaction      transaction(this);

        if (job.Fetch(this, job_id) != CJob::eJF_Ok)
            NCBI_THROW(CNetScheduleException, eInternalError,
                       "Error fetching job: " + DecorateJobId(job_id));
        if (job.GetStatus() != CNetScheduleAPI::eReading)
            NCBI_THROW(CNetScheduleException, eInvalidJobStatus,
                       "Operation is applicable to eReading job state only");

        // Check that authorization token matches
        CJob::EAuthTokenCompareResult   token_compare_result =
                                            job.CompareAuthToken(auth_token);
        if (token_compare_result == CJob::eInvalidTokenFormat)
            NCBI_THROW(CNetScheduleException, eInvalidAuthToken,
                       "Invalid authorization token format");
        if (token_compare_result == CJob::eNoMatch)
            NCBI_THROW(CNetScheduleException, eInvalidAuthToken,
                       "Authorization token does not match");

        // Add an event
        CJobEvent &     event = job.AppendEvent();
        event.SetTimestamp(time(0));
        event.SetNodeAddr(client.GetAddress());
        event.SetClientNode(client.GetNode());
        event.SetClientSession(client.GetSession());

        switch (target_status) {
            case CNetScheduleAPI::eDone:
                event.SetEvent(CJobEvent::eReadRollback);
                job.SetReadCount(job.GetReadCount() - 1);
                break;
            case CNetScheduleAPI::eReadFailed:
                event.SetEvent(CJobEvent::eReadFail);
                // Check the number of tries first
                if (job.GetReadCount() <= m_FailedRetries)
                    // The job needs to be re-scheduled for reading
                    new_status = CNetScheduleAPI::eDone;
                break;
            case CNetScheduleAPI::eConfirmed:
                event.SetEvent(CJobEvent::eReadDone);
                break;
            default:
                _ASSERT(0);
                break;
        }

        event.SetStatus(new_status);
        job.SetStatus(new_status);

        job.Flush(this);
        transaction.Commit();
    }}

    TimeLineRemove(job_id);

    m_StatusTracker.ChangeStatus(job_id, new_status);
    return;
}


void CQueue::EraseJob(unsigned int  job_id)
{
    {{
        CFastMutexGuard     guard(m_OperationLock);
        m_StatusTracker.Erase(job_id);
    }}

    {{
        // Request delayed record delete
        CFastMutexGuard     jtd_guard(m_JobsToDeleteLock);

        if (!m_JobsToDelete[job_id]) {
            m_JobsToDelete.set_bit(job_id);
            FlushDeletedVector();
        }
    }}
    TimeLineRemove(job_id);
}


void CQueue::Erase(const TNSBitVector &  job_ids)
{
    CFastMutexGuard     jtd_guard(m_JobsToDeleteLock);

    m_JobsToDelete |= job_ids;
    FlushDeletedVector();
}


void CQueue::FlushDeletedVector(void)
{
    m_JobsToDelete.optimize();
    m_QueueDbBlock->deleted_jobs_db.id = 0;
    m_QueueDbBlock->deleted_jobs_db.WriteVector(m_JobsToDelete,
                                                SDeletedJobsDB::eNoCompact);
}


void CQueue::OptimizeMem()
{
    m_StatusTracker.OptimizeMem();
}


unsigned CQueue::FindPendingJob(const CNSClientId  &  client,
                                const list<string> &  aff_list,
                                time_t                curr)
{
    // It is straight forward if no affinity is given
    if (aff_list.empty())
        return m_StatusTracker.GetJobByStatus(
                                CNetScheduleAPI::ePending,
                                m_ClientsRegistry.GetBlacklistedJobs(client));

    // Here: affinity is provided; pick a job respecting the given affinities

    TNSBitVector    aff_ids = m_AffinityRegistry.GetAffinityIDs(aff_list);
    if (!aff_ids.any())
        return 0;

    TNSBitVector    candidate_jobs = m_AffinityRegistry.GetJobsWithAffinity(aff_ids);
    if (!candidate_jobs.any())
        return 0;

    m_StatusTracker.PendingIntersect(&candidate_jobs);
    candidate_jobs -= m_ClientsRegistry.GetBlacklistedJobs(client);

    if (!candidate_jobs.any())
        return 0;

    return m_StatusTracker.GetPendingJobFromSet(candidate_jobs);
}


string CQueue::GetAffinityList()
{
    map< string, unsigned int >
                    jobs_per_token = m_AffinityRegistry.GetJobsPerToken();

    string          aff_list;
    for (map< string, unsigned int >::const_iterator  k = jobs_per_token.begin();
         k != jobs_per_token.end(); ++k) {
        if (!aff_list.empty())
            aff_list += "&";

        aff_list += NStr::URLEncode(k->first) + '=' +
                    NStr::UIntToString(k->second);
    }
    return aff_list;
}


bool CQueue::FailJob(const CNSClientId &    client,
                     unsigned int           job_id,
                     const string &         err_msg,
                     const string &         output,
                     int                    ret_code)
{
    unsigned        failed_retries;
    unsigned        max_output_size;
    time_t          blacklist_time;
    {{
        CQueueParamAccessor     qp(*this);
        failed_retries  = qp.GetFailedRetries();
        max_output_size = qp.GetMaxOutputSize();
        blacklist_time  = qp.GetBlacklistTime();
    }}

    if (output.size() > max_output_size) {
        NCBI_THROW(CNetScheduleException, eDataTooLong,
                   "Output is too long");
    }

    CJob                job;
    time_t              curr = time(0);
    bool                rescheduled = false;

    {{
        CFastMutexGuard     guard(m_OperationLock);
        TJobStatus          old_status = GetJobStatus(job_id);
        TJobStatus          new_status = CNetScheduleAPI::eFailed;

        if (old_status != CNetScheduleAPI::eRunning) {
            // No job state change
            return false;
        }

        {{
             CNSTransaction     transaction(this);


            CJob::EJobFetchResult   res = job.Fetch(this, job_id);
            if (res != CJob::eJF_Ok) {
                // TODO: Integrity error or job just expired?
                if (m_Log)
                    LOG_POST(Error << "Can not fetch job " << DecorateJobId(job_id));
                return false;
            }

            CJobEvent *     event = job.GetLastEvent();
            if (!event)
                ERR_POST("No JobEvent for running job " << DecorateJobId(job_id));

            event = &job.AppendEvent();
            event->SetEvent(CJobEvent::eFail);
            event->SetStatus(CNetScheduleAPI::eFailed);
            event->SetTimestamp(curr);
            event->SetErrorMsg(err_msg);
            event->SetRetCode(ret_code);
            event->SetNodeAddr(client.GetAddress());
            event->SetClientNode(client.GetNode());
            event->SetClientSession(client.GetSession());

            unsigned                run_count = job.GetRunCount();
            if (run_count <= failed_retries) {
                job.SetStatus(CNetScheduleAPI::ePending);
                event->SetStatus(CNetScheduleAPI::ePending);

                new_status = CNetScheduleAPI::ePending;

                rescheduled = true;
                if (m_Log)
                    LOG_POST(Error <<"Job " << DecorateJobId(job_id)
                                   << " rescheduled with "
                                   << (failed_retries - run_count)
                                   << " retries left");
            } else {
                job.SetStatus(CNetScheduleAPI::eFailed);
                event->SetStatus(CNetScheduleAPI::eFailed);
                new_status = CNetScheduleAPI::eFailed;
                rescheduled = false;
                if (m_Log)
                    LOG_POST(Error << "Job " << DecorateJobId(job_id)
                                   << " failed");
            }

            job.SetOutput(output);
            job.Flush(this);
            transaction.Commit();
        }}

        m_StatusTracker.SetStatus(job_id, new_status);

        TimeLineRemove(job_id);

        // Replace it with ClearExecuting(client, job_id) when all clients provide
        // their credentials and job passport is checked strictly
        m_ClientsRegistry.ClearExecuting(job_id);
        m_ClientsRegistry.AddToBlacklist(client, job_id);

        if (!rescheduled  &&  job.ShouldNotify(curr)) {
            m_NotificationsList.NotifyJobStatus(job.GetSubmAddr(),
                                                job.GetSubmNotifPort(),
                                                MakeKey(job_id));
        }
    }}

    return true;
}


string  CQueue::GetAffinityTokenByID(unsigned int  aff_id) const
{
    return m_AffinityRegistry.GetTokenByID(aff_id);
}



/// Specified status is OR-ed with the target vector
void CQueue::JobsWithStatus(TJobStatus    status,
                            TNSBitVector* bv) const
{
    m_StatusTracker.StatusSnapshot(status, bv);
}


void CQueue::ClearWorkerNode(const CNSClientId &  client)
{
    // Get the running and reading jobs and move them to the corresponding
    // states (pending and done)
    m_ClientsRegistry.ClearWorkerNode(client, this);
    return;
}


void CQueue::NotifyListeners(bool unconditional, unsigned aff_id)
{
    if (unconditional == false) {
        // This also means that the call comes from a notification thread.
        // Test first that we should not skip this call due to the required
        // notification frequency.
        static double   last_notif_timeout = -1.0;
        static size_t   skip_limit = 0;
        static size_t   skip_count;

        if (m_NotifyTimeout != last_notif_timeout) {
            last_notif_timeout = m_NotifyTimeout;
            skip_count = 0;
            skip_limit = size_t(m_NotifyTimeout/0.1);
        }

        ++skip_count;
        if (skip_count < skip_limit)
            return;
    }

    // TODO: if affinity valency is full for this aff_id, notify only nodes
    // with this aff_id, otherwise notify all nodes in the hope that some
    // of them will pick up the task with this aff_id

    if (unconditional || m_StatusTracker.AnyPending()) {
        if (aff_id == 0)
            m_NotificationsList.Notify("");
        else
            m_NotificationsList.Notify(m_AffinityRegistry.GetTokenByID(aff_id));
    }
    else
        m_NotificationsList.CheckTimeout();
    return;
}


void CQueue::PrintClientsList(CNetScheduleHandler &  handler,
                              bool                   verbose) const
{
    m_ClientsRegistry.PrintClientsList(this, handler, verbose);
}


void CQueue::CheckExecutionTimeout(bool  logging)
{
    if (!m_RunTimeLine)
        return;

    unsigned        queue_run_timeout = GetRunTimeout();
    time_t          curr = time(0);
    TNSBitVector    bv;
    {{
        CReadLockGuard  guard(m_RunTimeLineLock);
        m_RunTimeLine->ExtractObjects(curr, &bv);
    }}

    TNSBitVector::enumerator en(bv.first());
    for ( ;en.valid(); ++en) {
        x_CheckExecutionTimeout(queue_run_timeout, *en, curr, logging);
    }
}


void CQueue::x_CheckExecutionTimeout(unsigned   queue_run_timeout,
                                     unsigned   job_id,
                                     time_t     curr_time,
                                     bool       logging)
{
    CJob                    job;
    unsigned                time_start = 0;
    unsigned                run_timeout = 0;
    time_t                  exp_time = 0;
    TJobStatus              status;
    TJobStatus              new_status;
    CJobEvent::EJobEvent    event_type;


    {{
        CFastMutexGuard         guard(m_OperationLock);

        status = GetJobStatus(job_id);
        if (status == CNetScheduleAPI::eRunning) {
            new_status = CNetScheduleAPI::ePending;
            event_type = CJobEvent::eTimeout;
        } else if (status == CNetScheduleAPI::eReading) {
            new_status = CNetScheduleAPI::eDone;
            event_type = CJobEvent::eReadTimeout;
        } else
            return; // Execution timeout is for Running and Reading jobs only


        {{
            CNSTransaction      transaction(this);

            if (job.Fetch(this, job_id) != CJob::eJF_Ok)
                return;

            CJobEvent *     event = job.GetLastEvent();
            time_start = event->GetTimestamp();
            run_timeout = job.GetRunTimeout();
            if (run_timeout == 0)
                run_timeout = queue_run_timeout;

            exp_time = run_timeout ? time_start + run_timeout : 0;
            if (curr_time < exp_time) {
                // we need to register job in time line
                TimeLineAdd(job_id, exp_time);
                return;
            }

            // The job timeout (running or reading) is expired.
            // Check the try counter, we may need to fail the job.
            if (status == CNetScheduleAPI::eRunning) {
                // Running state
                if (job.GetRunCount() > m_FailedRetries)
                    new_status = CNetScheduleAPI::eFailed;
            } else {
                // Reading state
                if (job.GetReadCount() > m_FailedRetries)
                    new_status = CNetScheduleAPI::eReadFailed;
            }

            job.SetStatus(new_status);

            event = &job.AppendEvent();
            event->SetStatus(new_status);
            event->SetEvent(event_type);
            event->SetTimestamp(curr_time);

            job.Flush(this);
            transaction.Commit();
        }}


        m_StatusTracker.SetStatus(job_id, new_status);

        if (status == CNetScheduleAPI::eRunning) {
            if (new_status == CNetScheduleAPI::ePending)
                // Timeout and reschedule, put to blacklist as well
                m_ClientsRegistry.ClearExecutingSetBlacklist(job_id);
            else
                m_ClientsRegistry.ClearExecuting(job_id);
        } else {
            if (new_status == CNetScheduleAPI::eDone)
                // Timeout and reschedule for reading, put to the blacklist
                m_ClientsRegistry.ClearReadingSetBlacklist(job_id);
            else
                m_ClientsRegistry.ClearReading(job_id);
        }
    }}

    if (new_status == CNetScheduleAPI::eFailed && job.ShouldNotify(time(0)))
        m_NotificationsList.NotifyJobStatus(job.GetSubmAddr(),
                                            job.GetSubmNotifPort(),
                                            MakeKey(job_id));

    if (logging)
    {
        CTime       tm_start;
        tm_start.SetTimeT(time_start);
        tm_start.ToLocalTime();

        CTime       tm_exp;
        tm_exp.SetTimeT(exp_time);
        tm_exp.ToLocalTime();

        string      purpose;
        if (status == CNetScheduleAPI::eRunning)
            purpose = "execution";
        else
            purpose = "reading";

        GetDiagContext().Extra()
                .Print("msg", "Timeout expired, rescheduled for " + purpose)
                .Print("msg_code", "410")   // The code is for searching in applog
                .Print("job_key", MakeKey(job.GetId()))
                .Print("queue", m_QueueName)
                .Print("run_counter", job.GetRunCount())
                .Print("read_counter", job.GetReadCount())
                .Print("time_start", tm_start.AsString())
                .Print("exp_time", tm_exp.AsString())
                .Print("run_timeout", run_timeout);
    }
}


unsigned CQueue::CheckJobsExpiry(unsigned batch_size, TJobStatus status)
{
    unsigned int    queue_timeout;
    unsigned int    queue_run_timeout;

    {{
        CQueueParamAccessor     qp(*this);

        queue_timeout = qp.GetTimeout();
        queue_run_timeout = qp.GetRunTimeout();
    }}

    TNSBitVector        job_ids;
    TNSBitVector        not_found_jobs;
    time_t              curr = time(0);
    CJob                job;
    unsigned            del_count = 0;
    bool                has_db_error = false;
    {{
        unsigned int        job_id = 0;
        CFastMutexGuard     guard(m_OperationLock);

        for (unsigned n = 0; n < batch_size; ++n) {
            job_id = m_StatusTracker.GetNext(status, job_id);
            if (job_id == 0)
                break;

            {{
                 m_QueueDbBlock->job_db.SetTransaction(NULL);
                 m_QueueDbBlock->events_db.SetTransaction(NULL);
                 m_QueueDbBlock->job_info_db.SetTransaction(NULL);

                // FIXME: should fetch only main part of the job and event info,
                // does not need potentially huge input/output
                CJob::EJobFetchResult       res = job.Fetch(this, job_id);
                if (res != CJob::eJF_Ok) {
                    if (res != CJob::eJF_NotFound)
                        has_db_error = true;
                    // ? already deleted - report as a batch later
                    not_found_jobs.set_bit(job_id);
                    m_StatusTracker.Erase(job_id);
                    // Do not break the process of erasing expired jobs
                    continue;
                }
            }}

            // Is the job expired?
            if (job.GetJobExpirationTime(queue_timeout,
                                         queue_run_timeout) <= curr) {
                m_StatusTracker.Erase(job_id);
                job_ids.set_bit(job_id);
                ++del_count;

                // check if the affinity should also be updated
                if (job.GetAffinityId() != 0) {
                    CNSTransaction      transaction(this);
                    m_AffinityRegistry.RemoveJobFromAffinity(
                                                    job_id,
                                                    job.GetAffinityId());
                    transaction.Commit();
                }
            }
        }
    }}

    if (m_Log  &&  not_found_jobs.any()) {
        if (has_db_error) {
            LOG_POST(Error << "While deleting, errors in database"
                              ", status " << CNetScheduleAPI::StatusToString(status)
                           << ", queue "  << m_QueueName
                           << ", job(s) " << NS_EncodeBitVector(this, not_found_jobs));
        } else {
            LOG_POST(Error << "While deleting, jobs not found in database"
                              ", status " << CNetScheduleAPI::StatusToString(status)
                           << ", queue "  << m_QueueName
                           << ", job(s) " << NS_EncodeBitVector(this, not_found_jobs));
        }
    }
    if (del_count)
        Erase(job_ids);

    return del_count;
}


void CQueue::TimeLineMove(unsigned job_id, time_t old_time, time_t new_time)
{
    if (!job_id || !m_RunTimeLine)
        return;

    CWriteLockGuard guard(m_RunTimeLineLock);
    m_RunTimeLine->MoveObject(old_time, new_time, job_id);
}


void CQueue::TimeLineAdd(unsigned job_id, time_t job_time)
{
    if (!job_id  ||  !m_RunTimeLine  ||  !job_time)
        return;

    CWriteLockGuard guard(m_RunTimeLineLock);
    m_RunTimeLine->AddObject(job_time, job_id);
}


void CQueue::TimeLineRemove(unsigned job_id)
{
    if (!m_RunTimeLine)
        return;

    CWriteLockGuard guard(m_RunTimeLineLock);
    m_RunTimeLine->RemoveObject(job_id);
}


void CQueue::TimeLineExchange(unsigned  remove_job_id,
                              unsigned  add_job_id,
                              time_t    new_time)
{
    if (!m_RunTimeLine)
        return;

    CWriteLockGuard guard(m_RunTimeLineLock);
    if (remove_job_id)
        m_RunTimeLine->RemoveObject(remove_job_id);
    if (add_job_id)
        m_RunTimeLine->AddObject(new_time, add_job_id);
}


unsigned CQueue::DeleteBatch(unsigned batch_size)
{
    TNSBitVector    batch;
    {{
        CFastMutexGuard     guard(m_JobsToDeleteLock);
        unsigned int    job_id = 0;
        for (unsigned int  n = 0; n < batch_size &&
                             (job_id = m_JobsToDelete.extract_next(job_id));
             ++n)
        {
            batch.set(job_id);
        }
        if (batch.any())
            FlushDeletedVector();
    }}
    if (batch.none())
        return 0;

    unsigned int  actual_batch_size = batch.count();
    unsigned int  chunks = (actual_batch_size + 999) / 1000;
    unsigned int  chunk_size = actual_batch_size / chunks;
    unsigned int  residue = actual_batch_size - chunks*chunk_size;

    unsigned int                del_rec = 0;
    TNSBitVector::enumerator    en = batch.first();
    while (en.valid()) {
        unsigned int    txn_size = chunk_size;
        if (residue) {
            ++txn_size;
            --residue;
        }

        CFastMutexGuard     guard(m_OperationLock);
        CNSTransaction      transaction(this);

        unsigned int        n;
        for (n = 0; en.valid() && n < txn_size; ++en, ++n) {
            unsigned int    job_id = *en;
            unsigned int    aff_id = 0;

            m_QueueDbBlock->job_db.id = job_id;
            if (m_QueueDbBlock->job_db.Fetch() == eBDB_Ok)
                aff_id = m_QueueDbBlock->job_db.aff_id;

            try {
                m_QueueDbBlock->job_db.Delete();
                ++del_rec;
            } catch (CBDB_ErrnoException& ex) {
                ERR_POST("BDB error " << ex.what());
            }

            m_QueueDbBlock->job_info_db.id = job_id;
            try {
                m_QueueDbBlock->job_info_db.Delete();
            } catch (CBDB_ErrnoException& ex) {
                ERR_POST("BDB error " << ex.what());
            }
            x_DeleteJobEvents(job_id);
        }

        transaction.Commit();
    }

    if (m_Log  &&  del_rec > 0)
        LOG_POST(Error << del_rec << " job(s) deleted");

    return del_rec;
}


void CQueue::x_DeleteJobEvents(unsigned int  job_id)
{
    try {
        for (unsigned int  event_number = 0; ; ++event_number) {
            m_QueueDbBlock->events_db.id = job_id;
            m_QueueDbBlock->events_db.event_id = event_number;
            if (m_QueueDbBlock->events_db.Delete() == eBDB_NotFound)
                break;
        }
    } catch (CBDB_ErrnoException& ex) {
        ERR_POST("BDB error " << ex.what());
    }
}


CBDB_FileCursor& CQueue::GetEventsCursor()
{
    CBDB_Transaction* trans = m_QueueDbBlock->events_db.GetBDBTransaction();
    CBDB_FileCursor* cur = m_EventsCursor.get();
    if (!cur) {
        cur = new CBDB_FileCursor(m_QueueDbBlock->events_db);
        m_EventsCursor.reset(cur);
    } else {
        cur->Close();
    }
    cur->ReOpen(trans);
    return *cur;
}


void CQueue::PrintSubmHosts(CNetScheduleHandler &  handler) const
{
    CQueueParamAccessor     qp(*this);
    qp.GetSubmHosts().PrintHosts(handler);
}


void CQueue::PrintWNodeHosts(CNetScheduleHandler &  handler) const
{
    CQueueParamAccessor     qp(*this);
    qp.GetWnodeHosts().PrintHosts(handler);
}


#define NS_PRINT_TIME(field, t) \
do { \
    if (t == 0) \
        handler.WriteMessage("OK:", field); \
    else { \
        CTime   _t(t); \
        _t.ToLocalTime(); \
        handler.WriteMessage("OK:", field + _t.AsString()); \
    } \
} while(0)


void CQueue::x_PrintJobStat(CNetScheduleHandler &   handler,
                            const CJob &            job,
                            unsigned                queue_run_timeout)
{
    unsigned            run_timeout = job.GetRunTimeout();
    unsigned            aff_id = job.GetAffinityId();


    handler.WriteMessage("OK:", "id: " + NStr::IntToString(job.GetId()));
    handler.WriteMessage("OK:", "key: " + MakeKey(job.GetId()));
    handler.WriteMessage("OK:", "status: " +
                                CNetScheduleAPI::StatusToString(job.GetStatus()));

    handler.WriteMessage("OK:", "timeout: " +
                                NStr::ULongToString(job.GetTimeout()));
    handler.WriteMessage("OK:", "run_timeout: " +
                                NStr::IntToString(run_timeout));
    NS_PRINT_TIME("expiration_time: ",
                  job.GetJobExpirationTime(GetTimeout(), GetRunTimeout()));

    handler.WriteMessage("OK:", "subm_notif_port: " +
                                NStr::IntToString(job.GetSubmNotifPort()));
    handler.WriteMessage("OK:", "subm_notif_timeout: " +
                                NStr::IntToString(job.GetSubmNotifTimeout()));

    // Print detailed information about the job events
    const vector<CJobEvent> &   events = job.GetEvents();
    int                         event = 1;

    ITERATE(vector<CJobEvent>, it, events) {
        string          message("event" + NStr::IntToString(event++) + ": ");
        unsigned int    addr = it->GetNodeAddr();
        // unsigned short  port = it->GetNodePort();

        // Address part
        message += "client=";
        if (addr == 0)
            message += "ns ";
        else
            message += CSocketAPI::gethostbyaddr(addr) + " ";

        message += "event=" + CJobEvent::EventToString(it->GetEvent()) + " "
                   "status=" +
                   CNetScheduleAPI::StatusToString(it->GetStatus()) + " "
                   "ret_code=" + NStr::IntToString(it->GetRetCode()) + " ";

        // Time part
        message += "timestamp=";
        unsigned int    start = it->GetTimestamp();
        if (start == 0) {
            message += "n/a ";
        }
        else {
            CTime   startTime(start);
            startTime.ToLocalTime();
            message += "'" + startTime.AsString() + "' ";
        }

        // The rest
        message += "node=" + it->GetClientNode() + " "
                   "session=" + it->GetClientSession() + " "
                   "err_msg=" + it->GetQuotedErrorMsg();
        handler.WriteMessage("OK:", message);
    }

    handler.WriteMessage("OK:", "run_counter: " +
                                NStr::IntToString(job.GetRunCount()));
    handler.WriteMessage("OK:", "read_counter: " +
                                NStr::IntToString(job.GetReadCount()));

    if (aff_id) {
        string  token = m_AffinityRegistry.GetTokenByID(job.GetAffinityId());
        handler.WriteMessage("OK:", "aff_token: '" + 
                                    NStr::PrintableString(token) + "'");
    }
    handler.WriteMessage("OK:", "aff_id: " + NStr::IntToString(aff_id));
    handler.WriteMessage("OK:", "mask: " + NStr::IntToString(job.GetMask()));
    handler.WriteMessage("OK:", "input: " + job.GetQuotedInput());
    handler.WriteMessage("OK:", "output: " + job.GetQuotedOutput());
    handler.WriteMessage("OK:", "progress_msg: '" + job.GetProgressMsg() + "'");
}


void CQueue::x_PrintShortJobStat(CNetScheduleHandler &  handler,
                                 const CJob&            job)
{
    string      reply = MakeKey(job.GetId()) + "\t" +
                        CNetScheduleAPI::StatusToString(job.GetStatus()) + "\t" +
                        job.GetQuotedInput() + "\t" +
                        job.GetQuotedOutput() + "\t";

    const CJobEvent *   last_event = job.GetLastEvent();
    if (last_event)
        reply += last_event->GetQuotedErrorMsg();
    else
        reply += "''";
    handler.WriteMessage("OK:", reply);
}


size_t CQueue::PrintJobDbStat(CNetScheduleHandler &     handler,
                              unsigned                  job_id,
                              TJobStatus                status)
{
    size_t      print_count = 0;
    unsigned    queue_run_timeout = GetRunTimeout();
    CJob        job;

    if (status == CNetScheduleAPI::eJobNotFound) {
        CFastMutexGuard         guard(m_OperationLock);

        m_QueueDbBlock->job_db.SetTransaction(NULL);
        m_QueueDbBlock->events_db.SetTransaction(NULL);
        m_QueueDbBlock->job_info_db.SetTransaction(NULL);

        CJob::EJobFetchResult   res = job.Fetch(this, job_id);
        if (res == CJob::eJF_Ok) {
            handler.WriteMessage("");
            x_PrintJobStat(handler, job, queue_run_timeout);
            ++print_count;
        }
    } else {
        TNSBitVector        bv;
        CFastMutexGuard     guard(m_OperationLock);

        JobsWithStatus(status, &bv);
        m_QueueDbBlock->job_db.SetTransaction(NULL);
        m_QueueDbBlock->events_db.SetTransaction(NULL);
        m_QueueDbBlock->job_info_db.SetTransaction(NULL);
        for (TNSBitVector::enumerator en(bv.first());en.valid(); ++en) {
            CJob::EJobFetchResult   res = job.Fetch(this, *en);
            if (res == CJob::eJF_Ok) {
                handler.WriteMessage("");
                x_PrintJobStat(handler, job, queue_run_timeout);
                ++print_count;
            }
        }
    }
    return print_count;
}


void CQueue::PrintAllJobDbStat(CNetScheduleHandler &   handler)
{
    unsigned            queue_run_timeout = GetRunTimeout();
    CJob                job;
    CFastMutexGuard     guard(m_OperationLock);

    m_QueueDbBlock->job_db.SetTransaction(NULL);
    m_QueueDbBlock->events_db.SetTransaction(NULL);
    m_QueueDbBlock->job_info_db.SetTransaction(NULL);

    CQueueEnumCursor    cur(this);

    while (cur.Fetch() == eBDB_Ok) {
        if (job.Fetch(this) == CJob::eJF_Ok) {
            handler.WriteMessage("");
            x_PrintJobStat(handler, job, queue_run_timeout);
        }
    }
}


void CQueue::PrintQueue(CNetScheduleHandler &   handler,
                        TJobStatus              job_status)
{
    TNSBitVector        bv;
    CJob                job;
    CFastMutexGuard     guard(m_OperationLock);

    m_QueueDbBlock->job_db.SetTransaction(NULL);
    m_QueueDbBlock->events_db.SetTransaction(NULL);
    m_QueueDbBlock->job_info_db.SetTransaction(NULL);

    JobsWithStatus(job_status, &bv);
    for (TNSBitVector::enumerator en(bv.first()); en.valid(); ++en)
        if (job.Fetch(this, *en) == CJob::eJF_Ok)
            x_PrintShortJobStat(handler, job);
}


unsigned CQueue::CountStatus(TJobStatus st) const
{
    return m_StatusTracker.CountStatus(st);
}


void CQueue::StatusStatistics(TJobStatus                status,
                              TNSBitVector::statistics* st) const
{
    m_StatusTracker.StatusStatistics(status, st);
}


static const unsigned kMeasureInterval = 1;
// for informational purposes only, see kDecayExp below
static const unsigned kDecayInterval = 10;
static const unsigned kFixedShift = 7;
static const unsigned kFixed_1 = 1 << kFixedShift;
// kDecayExp = 2 ^ kFixedShift / 2 ^ ( kMeasureInterval * log2(e) / kDecayInterval)
static const unsigned kDecayExp = 116;

CQueue::CStatisticsThread::CStatisticsThread(TContainer &  container,
                                             const bool &  logging)
    : CThreadNonStop(kMeasureInterval),
        m_Container(container), m_RunCounter(0),
        m_StatisticsLogging(logging)
{}


void CQueue::CStatisticsThread::DoJob(void)
{
    ++m_RunCounter;

    int     counter;
    for (TStatEvent n = 0; n < eStatNumEvents; ++n) {
        counter = m_Container.m_EventCounter[n].Get();
        m_Container.m_EventCounter[n].Add(-counter);
        m_Container.m_Average[n] =
            (kDecayExp * m_Container.m_Average[n] +
                (kFixed_1-kDecayExp) * ((unsigned) counter << kFixedShift)) >>
                    kFixedShift;
    }

    if (m_RunCounter > 10) {
        m_RunCounter = 0;

        if (m_StatisticsLogging) {
            CRef<CRequestContext>       ctx(new CRequestContext());
            ctx->SetRequestID();
            GetDiagContext().SetRequestContext(ctx);
            GetDiagContext().PrintRequestStart()
                            .Print("_type", "statistics_thread")
                            .Print("queue", m_Container.m_QueueName)
                            .Print("get_event_average",
                                   m_Container.m_Average[eStatGetEvent])
                            .Print("put_event_average",
                                   m_Container.m_Average[eStatPutEvent])
                            .Print("dbtransaction_event_average",
                                   m_Container.m_Average[eStatDBTransactionEvent])
                            .Print("dbwrite_event_average",
                                   m_Container.m_Average[eStatDBWriteEvent]);
            ctx->SetRequestStatus(CNetScheduleHandler::eStatus_OK);
            GetDiagContext().PrintRequestStop();
            ctx.Reset();
            GetDiagContext().SetRequestContext(NULL);
        }
    }
}


void CQueue::CountEvent(TStatEvent event, int num)
{
    m_EventCounter[event].Add(num);
}


double CQueue::GetAverage(TStatEvent n_event)
{
    return m_Average[n_event] / double(kFixed_1 * kMeasureInterval);
}


void CQueue::TouchClientsRegistry(const CNSClientId &  client)
{
    m_ClientsRegistry.Touch(client, this);
    return;
}


// Moves the job to Pending/Failed or to Done/ReadFailed
// when event event_type has come
void CQueue::x_ResetDueTo(const CNSClientId &   client,
                          unsigned int          job_id,
                          time_t                current_time,
                          TJobStatus            status_from,
                          CJobEvent::EJobEvent  event_type)
{
    CJob                job;
    TJobStatus          new_status;

    CFastMutexGuard     guard(m_OperationLock);

    {{
        CNSTransaction      transaction(this);

        if (job.Fetch(this, job_id) != CJob::eJF_Ok) {
            ERR_POST("Cannot fetch job to reset it due to " <<
                     CJobEvent::EventToString(event_type) <<
                     ". Job: " << DecorateJobId(job_id));
            return;
        }

        if (status_from == CNetScheduleAPI::eRunning) {
            // The job was running
            if (job.GetRunCount() > m_FailedRetries)
                new_status = CNetScheduleAPI::eFailed;
            else
                new_status = CNetScheduleAPI::ePending;
        } else {
            // The job was reading
            if (job.GetReadCount() > m_FailedRetries)
                new_status = CNetScheduleAPI::eReadFailed;
            else
                new_status = CNetScheduleAPI::eDone;
        }

        job.SetStatus(new_status);

        CJobEvent *     event = &job.AppendEvent();
        event->SetStatus(new_status);
        event->SetEvent(event_type);
        event->SetTimestamp(current_time);
        event->SetClientNode(client.GetNode());
        event->SetClientSession(client.GetSession());

        job.Flush(this);
        transaction.Commit();
    }}

    // Update the memory map
    m_StatusTracker.SetStatus(job_id, new_status);

    // remove the job from the time line
    TimeLineRemove(job_id);
    return;
}


void CQueue::ResetRunningDueToClear(const CNSClientId &   client,
                                    const TNSBitVector &  jobs)
{
    time_t      current_time = time(0);
    for (TNSBitVector::enumerator  en(jobs.first()); en.valid(); ++en) {
        try {
            x_ResetDueTo(client, *en, current_time,
                         CNetScheduleAPI::eRunning, CJobEvent::eClear);
        } catch (...) {
            ERR_POST("Error resetting a running job when worker node is "
                     "cleared. Job: " << DecorateJobId(*en));
        }
    }
}


void CQueue::ResetReadingDueToClear(const CNSClientId &   client,
                                    const TNSBitVector &  jobs)
{
    time_t      current_time = time(0);
    for (TNSBitVector::enumerator  en(jobs.first()); en.valid(); ++en) {
        try {
            x_ResetDueTo(client, *en, current_time,
                         CNetScheduleAPI::eReading, CJobEvent::eClear);
        } catch (...) {
            ERR_POST("Error resetting a reading job when worker node is "
                     "cleared. Job: " << DecorateJobId(*en));
        }
    }
}


void CQueue::ResetRunningDueToNewSession(const CNSClientId &   client,
                                         const TNSBitVector &  jobs)
{
    time_t      current_time = time(0);
    for (TNSBitVector::enumerator  en(jobs.first()); en.valid(); ++en) {
        try {
            x_ResetDueTo(client, *en, current_time,
                         CNetScheduleAPI::eRunning, CJobEvent::eSessionChanged);
        } catch (...) {
            ERR_POST("Error resetting a running job when worker node "
                     "changed session. Job: " << DecorateJobId(*en));
        }
    }
}


void CQueue::ResetReadingDueToNewSession(const CNSClientId &   client,
                                         const TNSBitVector &  jobs)
{
    time_t      current_time = time(0);
    for (TNSBitVector::enumerator  en(jobs.first()); en.valid(); ++en) {
        try {
            x_ResetDueTo(client, *en, current_time,
                         CNetScheduleAPI::eReading, CJobEvent::eSessionChanged);
        } catch (...) {
            ERR_POST("Error resetting a reading job when worker node "
                     "changed session. Job: " << DecorateJobId(*en));
        }
    }
}


void CQueue::RegisterGetListener(const CNSClientId &  client,
                                 unsigned short       port,
                                 unsigned int         timeout)
{
    // Add to the notification list and save the wait port
    m_NotificationsList.RegisterListener(client, port, timeout);
    m_ClientsRegistry.SetWaitPort(client, port);
    return;
}


void CQueue::UnregisterGetListener(const CNSClientId &  client)
{
    // Get the port and remove the notification
    unsigned short  port = m_ClientsRegistry.GetAndResetWaitPort(client);
    if (port > 0)
        m_NotificationsList.UnregisterListener(client, port);
    return;
}


bool CQueue::x_UpdateDB_PutResultNoLock(unsigned             job_id,
                                        time_t               curr,
                                        int                  ret_code,
                                        const string &       output,
                                        CJob &               job,
                                        const CNSClientId &  client)
{
    if (job.Fetch(this, job_id) != CJob::eJF_Ok) {
        ERR_POST("Put results for non-existent job " << DecorateJobId(job_id));
        return false;
    }

    // Append the event
    CJobEvent *     event = &job.AppendEvent();
    event->SetStatus(CNetScheduleAPI::eDone);
    event->SetEvent(CJobEvent::eDone);
    event->SetTimestamp(curr);
    event->SetRetCode(ret_code);

    event->SetClientNode(client.GetNode());
    event->SetClientSession(client.GetSession());
    event->SetNodeAddr(client.GetAddress());

    job.SetStatus(CNetScheduleAPI::eDone);
    job.SetOutput(output);

    job.Flush(this);
    return true;
}


unsigned int  CQueue::x_UpdateDB_GetJobNoLock(const CNSClientId &  client,
                                              time_t               curr,
                                              unsigned int         job_id,
                                              CJob &               job)
{
    CNSTransaction      transaction(this);

    if (job.Fetch(this, job_id) != CJob::eJF_Ok)
        return 0;

    unsigned        run_count = job.GetRunCount() + 1;
    CJobEvent &     event = job.AppendEvent();

    event.SetStatus(CNetScheduleAPI::eRunning);
    event.SetEvent(CJobEvent::eRequest);
    event.SetTimestamp(curr);

    event.SetClientNode(client.GetNode());
    event.SetClientSession(client.GetSession());
    event.SetNodeAddr(client.GetAddress());

    job.SetStatus(CNetScheduleAPI::eRunning);
    job.SetRunTimeout(0);
    job.SetRunCount(run_count);

    job.Flush(this);
    transaction.Commit();

    return job_id;
}


END_NCBI_SCOPE

