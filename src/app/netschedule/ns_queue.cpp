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
// CNSTagMap

CNSTagMap::CNSTagMap()
{}


CNSTagMap::~CNSTagMap()
{
    NON_CONST_ITERATE(TNSTagMap, it, m_TagMap) {
        if (it->second) {
            delete it->second;
            it->second = 0;
        }
    }
}


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
    m_HasNotificationPort(false),
    m_DeleteDatabase(false),
    m_Executor(executor),
    m_QueueName(queue_name),
    m_QueueClass(qclass_name),
    m_Kind(queue_kind),
    m_QueueDbBlock(0),

    m_BecameEmpty(-1),
    m_WorkerNodeList(queue_name),

    m_LastId(0),
    m_SavedId(s_ReserveDelta),

    m_AffWrapped(false),
    m_CurrAffId(0),
    m_LastAffId(0),

    m_ParamLock(CRWLock::fFavorWriters),
    m_Timeout(3600),
    m_NotifyTimeout(7),
    m_DeleteDone(false),
    m_RunTimeout(3600),
    m_RunTimeoutPrecision(-1),
    m_FailedRetries(0),
    m_BlacklistTime(0),
    m_EmptyLifetime(0),
    m_MaxInputSize(kNetScheduleMaxDBDataSize),
    m_MaxOutputSize(kNetScheduleMaxDBDataSize),
    m_DenyAccessViolations(false),
    m_LogAccessViolations(true),
    m_KeepAffinity(false),
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
    m_GroupLastId.Set(0);
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
    m_AffinityDict.Attach(&m_QueueDbBlock->aff_dict_db,
                          &m_QueueDbBlock->aff_dict_token_idx);

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
    m_AffinityDict.Detach();
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


void CQueue::SetParameters(const SQueueParameters& params)
{
    // When modifying this, modify all places marked with PARAMETERS
    CWriteLockGuard     guard(m_ParamLock);

    m_Timeout       = params.timeout;
    m_NotifyTimeout = params.notif_timeout;
    m_DeleteDone    = params.delete_done;

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
    m_LogAccessViolations  = params.log_access_violations;
    m_KeepAffinity         = params.keep_affinity;

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
    unsigned        queue_run_timeout = GetRunTimeout();
    EBDB_ErrCode    err;

    static EVectorId    all_ids[] = { eVIJob, eVITag, eVIAffinity };
    TNSBitVector        all_vects[] =
        { m_JobsToDelete, m_DeletedJobs, m_AffJobsToDelete };

    for (size_t i = 0; i < sizeof(all_ids) / sizeof(all_ids[0]); ++i) {
        m_QueueDbBlock->deleted_jobs_db.id = all_ids[i];
        err = m_QueueDbBlock->deleted_jobs_db.ReadVector(&all_vects[i]);
        if (err != eBDB_Ok && err != eBDB_NotFound) {
            // TODO: throw db error
        }
        all_vects[i].optimize();
    }

    // scan the queue, load the state machine from DB
    TNSBitVector        running_jobs;
    CBDB_FileCursor     cur(m_QueueDbBlock->job_db);

    cur.InitMultiFetch(1024*1024);
    cur.SetCondition(CBDB_FileCursor::eGE);
    cur.From << 0;

    unsigned    recs = 0;
    unsigned    group_last_id = 0;

    for (; cur.Fetch() == eBDB_Ok; ) {
        unsigned    job_id = m_QueueDbBlock->job_db.id;

        if (m_JobsToDelete.test(job_id)) {
            x_DeleteJobEvents(job_id);
            continue;
        }

        int         i_status = m_QueueDbBlock->job_db.status;
        if (i_status <  (int) CNetScheduleAPI::ePending ||
            i_status >= (int) CNetScheduleAPI::eLastStatus)
        {
            // Invalid job, skip it
            ERR_POST("Job " << DecorateJobId(job_id) <<
                     " has invalid status " << i_status << ", ignored.");
            x_DeleteJobEvents(job_id);
            continue;
        }
        TJobStatus      status = TJobStatus(i_status);
        m_StatusTracker.SetExactStatusNoLock(job_id, status, true);

        if (status == CNetScheduleAPI::eReading) {
            unsigned group_id = m_QueueDbBlock->job_db.read_group;
            x_AddToReadGroupNoLock(group_id, job_id);
            if (group_last_id < group_id)
                group_last_id = group_id;
        }
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
        if (status == CNetScheduleAPI::eRunning)
            running_jobs.set(job_id);
        ++recs;
    }

    // Recover worker node info using job runs
    TNSBitVector::enumerator    en(running_jobs.first());
    for (; en.valid(); ++en) {
        unsigned                job_id = *en;
        CJob                    job;
        CJob::EJobFetchResult   res = job.Fetch(this, job_id);

        if (res != CJob::eJF_Ok) {
            // TODO: check for db error here
            ERR_POST("Can't read job " << DecorateJobId(job_id) <<
                     " while loading status matrix");
            continue;
        }

        const CJobEvent *       event = job.GetLastEvent();
        if (!event) {
            ERR_POST("No job event for Running job " << DecorateJobId(job_id) <<
                     " while loading status matrix");
            continue;
        }

        if (event->GetStatus() != CNetScheduleAPI::eRunning)
            continue;

        // FIXME Most likely, the following is not needed.
        if (m_WorkerNodeList.FindJobById(job_id) != NULL)
            continue;

        // We don't have auth in job event, but it is not that crucial.
        // Either the node is the old style one, and jobs are going to
        // be failed (or retried) on another node registration with the
        // same host:port, or for the new style nodes with same node id,
        // auth will be fixed on the INIT command.
        TWorkerNodeRef worker_node(m_WorkerNodeList.FindWorkerNodeByAddress(
            event->GetNodeAddr(), event->GetNodePort()));

        if (worker_node == NULL) {
            worker_node = new CWorkerNode_Real(event->GetNodeAddr());

            m_WorkerNodeList.RegisterNode(worker_node);
            m_WorkerNodeList.SetId(worker_node, event->GetNodeId());
            m_WorkerNodeList.SetPort(worker_node, event->GetNodePort());
        }

        unsigned    run_timeout = job.GetRunTimeout();
        if (run_timeout == 0)
            run_timeout = queue_run_timeout;
        unsigned    exp_time = event->GetTimestamp() + run_timeout;

        m_WorkerNodeList.AddJob(worker_node, job, exp_time);
    }
    m_GroupLastId.Set(group_last_id);
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
                        .Print("cmd", "BTCH-SUBMIT")
                        .Print("batch_id", batch_id);
        ctx->SetRequestStatus(CNetScheduleHandler::eStatus_OK);
    }

    if (!batch_submit || log_batch_each_job) {
        CDiagContext_Extra extra = GetDiagContext().Extra()
            .Print("job_key", MakeKey(job.GetId()))
            .Print("queue", GetQueueName())
            .Print("input", job.GetInput())
            .Print("aff", job.GetAffinityToken())
            .Print("mask", job.GetMask())
            .Print("subm_addr", CSocketAPI::gethostbyaddr(job.GetSubmAddr()))
            .Print("subm_port", job.GetSubmPort())
            .Print("subm_timeout", job.GetSubmTimeout())
            .Print("timeout", NStr::UIntToString(job.GetTimeout()))
            .Print("run_timeout", job.GetRunTimeout());
        ITERATE(TNSTagList, it, job.GetTags()) {
            extra.Print("tag_name", it->first);
            extra.Print("tag_value", it->second);
        }

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


unsigned CQueue::Submit(CJob &  job)
{
    unsigned    max_input_size;

    {{
        CQueueParamAccessor     qp(*this);
        max_input_size = qp.GetMaxInputSize();
    }}

    if (job.GetInput().size() > max_input_size)
        NCBI_THROW(CNetScheduleException, eDataTooLong, "Input is too long");

    bool        was_empty = !m_StatusTracker.AnyPending();
    unsigned    job_id = GetNextId();

    job.SetId(job_id);
    job.SetTimeSubmit(time(0));

    // Special treatment for system job masks
    if (job.GetMask() & CNetScheduleAPI::eOutOfOrder)
    {
        // NOT IMPLEMENTED YET: put job id into OutOfOrder list.
        // The idea is that there can be an urgent job, which
        // should be executed before jobs which were submitted
        // earlier, e.g. for some administrative purposes. See
        // CNetScheduleAPI::EJobMask in file netschedule_api.hpp
    }

    unsigned    affinity_id;
    {{
        CNS_Transaction     trans(this);
        CAffinityDictGuard  aff_dict_guard(GetAffinityDict(), trans);

        job.CheckAffinityToken(aff_dict_guard);
        affinity_id = job.GetAffinityId();
        trans.Commit();
    }}

    CNS_Transaction     trans(this);
    {{
        CQueueGuard     guard(this, &trans);

        job.Flush(this);
        // TODO: move event count to Flush
        CountEvent(CQueue::eStatDBWriteEvent, 1);
        //            need_aux_insert ? 2 : 1);

        // update affinity index
        if (affinity_id)
            AddJobsToAffinity(trans, affinity_id, job_id);

        // update tags
        CNSTagMap   tag_map;
        AppendTags(tag_map, job.GetTags(), job_id);
        FlushTags(tag_map, trans);
    }}

    trans.Commit();
    x_LogSubmit(job, 0, false);
    m_StatusTracker.SetStatus(job_id, CNetScheduleAPI::ePending);

    if (was_empty)
        NotifyListeners(true, affinity_id);
    return job_id;
}


unsigned CQueue::SubmitBatch(vector<CJob>& batch)
{
    unsigned    max_input_size;
    unsigned    job_id = GetNextIdBatch(batch.size());

    {{
        CQueueParamAccessor     qp(*this);
        max_input_size = qp.GetMaxInputSize();
    }}


    bool        was_empty = !m_StatusTracker.AnyPending();
    unsigned    batch_aff_id = 0; // if batch comes with the same affinity
    bool        batch_has_aff = false;

    // process affinity ids
    {{
        CNS_Transaction     trans(this);
        CAffinityDictGuard  aff_dict_guard(GetAffinityDict(), trans);
        for (unsigned i = 0; i < batch.size(); ++i) {
            CJob &      job = batch[i];
            if (job.GetAffinityId() == (unsigned)kMax_I4) { // take prev. token
                _ASSERT(i > 0);
                unsigned    prev_aff_id = 0;
                if (i > 0) {
                    prev_aff_id = batch[i-1].GetAffinityId();
                    if (!prev_aff_id) {
                        prev_aff_id = 0;
                        ERR_POST("Reference to empty previous affinity token");
                    }
                } else {
                    ERR_POST("First job in batch cannot have "
                             "reference to previous affinity token");
                }
                job.SetAffinityId(prev_aff_id);
            } else {
                if (job.HasAffinityToken()) {
                    job.CheckAffinityToken(aff_dict_guard);
                    batch_has_aff = true;
                    batch_aff_id = (i == 0 )? job.GetAffinityId() : 0;
                } else {
                    batch_aff_id = 0;
                }

            }
        }
        trans.Commit();
    }}

    CNS_Transaction     trans(this);
    {{
        CNSTagMap       tag_map;
        CQueueGuard     guard(this, &trans);
        unsigned        job_id_cnt = job_id;
        int             aux_inserts = 0;

        NON_CONST_ITERATE(vector<CJob>, it, batch) {
            if (it->GetInput().size() > max_input_size)
                NCBI_THROW(CNetScheduleException, eDataTooLong,
                           "Input is too long");

            unsigned    cur_job_id = job_id_cnt++;
            it->SetId(cur_job_id);
            it->SetTimeSubmit(time(0));
            it->Flush(this);

            AppendTags(tag_map, it->GetTags(), cur_job_id);
        }
        CountEvent(CQueue::eStatDBWriteEvent, batch.size() + aux_inserts);

        // Store the affinity index
        if (batch_has_aff) {
            if (batch_aff_id) {  // whole batch comes with the same affinity
                AddJobsToAffinity(trans, batch_aff_id, job_id,
                                  job_id + batch.size() - 1);
            } else
                AddJobsToAffinity(trans, batch);
        }
        FlushTags(tag_map, trans);
    }}
    trans.Commit();

    ITERATE(vector<CJob>, it, batch) {
        x_LogSubmit(*it, job_id, true);
    }

    m_StatusTracker.AddPendingBatch(job_id, job_id + batch.size() - 1);

    // This case is a bit complicated. If whole batch has the same
    // affinity, we include it in notification broadcast.
    // If not, or it has no affinity at all - 0.
    if (was_empty)
        NotifyListeners(true, batch_aff_id);

    return job_id;
}


static const unsigned k_max_dead_locks = 100;  // max. dead lock repeats

void CQueue::PutResultGetJob(CWorkerNode *              worker_node,
                             unsigned int               peer_addr,
                             // PutResult parameters
                             unsigned                   done_job_id,
                             int                        ret_code,
                             const string *             output,
                             // GetJob parameters
                             // in
                             const list<string> *       aff_list,
                             // out
                             CJob *                     new_job)
{
    // PutResult parameter check
    _ASSERT(!done_job_id || output);
    _ASSERT(!new_job || aff_list);

    bool        delete_done, keep_node_affinity;
    unsigned    max_output_size;
    unsigned    run_timeout;
    {{
        CQueueParamAccessor     qp(*this);

        delete_done = qp.GetDeleteDone();
        keep_node_affinity = qp.GetKeepAffinity() && done_job_id;
        max_output_size = qp.GetMaxOutputSize();
        run_timeout = qp.GetRunTimeout();
    }}

    if (done_job_id && output->size() > max_output_size)
        NCBI_THROW(CNetScheduleException, eDataTooLong,
                   "Output is too long");

    unsigned        dead_locks = 0; // dead lock counter
    time_t          curr = time(0);

    //
    bool            need_update = false;
    CQueueJSGuard   js_guard(this, done_job_id,
                             CNetScheduleAPI::eDone, &need_update);

    // FIXME: This code detects a case when a node reports results for the job
    // which already timeouted for the node and is being executed by another
    // node. This attempt succeeds, but the second legitimate result leads to
    // this error. It is not correct (but may be useful) to accept results from
    // the first node and we should better handle this case.
    // I commented out the code to reduce Error log output, but it needs to be
    // FIXED for real.
    // if (done_job_id  &&  ! need_update) {
    //    ERR_POST("Attempt to PUT already Done job "
    //             << q->DecorateJobId(done_job_id));
    //}

    // This is a HACK - if js_guard is not committed, it will rollback
    // to previous state, so it is safe to change status after the guard.
    //    if (delete_done) {
    //        q->Erase(done_job_id);
    //    }
    // TODO: implement transaction wrapper (a la js_guard above)
    // for FindPendingJob
    // TODO: move affinity assignment there as well

    // We request to not switch node affinity only if it is job exchange.
    // After node comes for a job second time, we satisfy this request
    // disregarding existing affinities so the queue is not to grow.
    unsigned        pending_job_id = 0;
    if (new_job)
        pending_job_id = FindPendingJob(worker_node, *aff_list,
                                        curr, keep_node_affinity);

    bool            done_rec_updated = false;
    CJob            job;

    // When working with the same database file concurrently there is
    // chance of internal Berkeley DB deadlock. (They say it's legal)
    // In this case Berkeley DB returns an error code(DB_LOCK_DEADLOCK)
    // and recovery is up to the application.
    // If it happens I repeat the transaction several times.
    //
    for (;;) {
        try {
            CNS_Transaction     trans(this);
            EGetJobUpdateStatus upd_status = eGetJobUpdate_Ok;

            {{
                CQueueGuard     guard(this, &trans);

                if (need_update) {
                    done_rec_updated = x_UpdateDB_PutResultNoLock(
                                            done_job_id, curr, delete_done,
                                            ret_code, *output, job,
                                            peer_addr);
                }

                if (pending_job_id) {
                    // NB Synchronized access to worker_node is required inside,
                    // and it's under queue lock already. Take out this access
                    // it is host, port, node_id read.
                    upd_status = x_UpdateDB_GetJobNoLock(worker_node,
                                                         curr, pending_job_id,
                                                         *new_job);
                }
            }}

            trans.Commit();
            js_guard.Commit();

            // TODO: commit FindPendingJob guard here
            switch (upd_status) {
                case eGetJobUpdate_JobFailed:
                    m_StatusTracker.ChangeStatus(pending_job_id,
                                                 CNetScheduleAPI::eFailed);
                    /* FALLTHROUGH */
                case eGetJobUpdate_JobStopped:
                case eGetJobUpdate_NotFound:
                    pending_job_id = 0;
                    break;
                case eGetJobUpdate_Ok:
                    break;
                default:
                    _ASSERT(0);
            }

            // NB BOTH Remove and Add lock worker node list
            if (done_rec_updated)
                RemoveJobFromWorkerNode(job, eNSCDone);
            if (pending_job_id)
                AddJobToWorkerNode(worker_node, *new_job, curr + run_timeout);
            break;
        }
        catch (CBDB_ErrnoException& ex) {
            if (ex.IsDeadLock()) {
                if (++dead_locks < k_max_dead_locks) {
                    ERR_POST("DeadLock repeat in CQueue::JobExchange");
                    SleepMilliSec(250);
                    continue;
                }
                ERR_POST("Too many transaction repeats in CQueue::JobExchange.");
            } else if (ex.IsNoMem()) {
                if (++dead_locks < k_max_dead_locks) {
                    ERR_POST("No resource repeat in CQueue::JobExchange");
                    SleepMilliSec(250);
                    continue;
                }
                ERR_POST("Too many transaction repeats in CQueue::JobExchange.");
            }
            throw;
        }
    }

    if (new_job) {
        unsigned int    job_aff_id = new_job->GetAffinityId();
        if (job_aff_id) {
            // CStopWatch      sw(CStopWatch::eStart);
            time_t          exp_time = run_timeout ? curr + 2*run_timeout : 0;

            AddAffinity(worker_node, job_aff_id, exp_time);
            // LOG_POST(Warning << "Added affinity: " << sw.Elapsed() * 1000 << "ms");
        }
    }


    TimeLineExchange(done_job_id, pending_job_id, curr + run_timeout);

    if (done_rec_updated  &&  job.ShouldNotify(curr))
        Notify(job.GetSubmAddr(), job.GetSubmPort(), done_job_id);

    // There is no need to throw exception here in case if there is no such a
    // job with the given affinity. Without exception it will be consistent
    // with GET command output which simply gives no key if there are no jobs.
}


void CQueue::JobDelayExpiration(CWorkerNode*     worker_node,
                                unsigned         job_id,
                                time_t           tm)
{
    if (tm <= 0)
        return;

    unsigned            queue_run_timeout = GetRunTimeout();
    time_t              run_timeout = 0;
    time_t              time_start = 0;

    TJobStatus          st = GetJobStatus(job_id);
    if (st != CNetScheduleAPI::eRunning)
        return;

    CJob                job;
    CNS_Transaction     trans(this);
    time_t              exp_time = 0;
    time_t              curr = time(0);
    bool                job_updated = false;

    {{
        CQueueGuard             guard(this, &trans);
        CJob::EJobFetchResult   res;

        if ((res = job.Fetch(this, job_id)) != CJob::eJF_Ok)
            return;

        CJobEvent *     event = job.GetLastEvent();
        if (!event) {
            ERR_POST("No JobEvent for running job " << DecorateJobId(job_id));
            // Fix it
            event = &job.AppendEvent();
            job_updated = true;
        }

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
            if (job_updated) job.Flush(this);
            return;
        }

        job.SetRunTimeout(curr + tm - time_start);
        job.Flush(this);
    }}

    trans.Commit();
    UpdateWorkerNodeJob(job_id, curr + tm);

    exp_time = run_timeout == 0 ? 0 : time_start + run_timeout;

    TimeLineMove(job_id, exp_time, curr + tm);

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


bool CQueue::PutProgressMessage(unsigned      job_id,
                                const string& msg)
{
    CJob                job;
    CNS_Transaction     trans(this);
    {{
        CQueueGuard             guard(this, &trans);

        if (job.Fetch(this, job_id) != CJob::eJF_Ok)
            return false;

        job.SetProgressMsg(msg);
        job.Flush(this);
    }}
    trans.Commit();
    return true;
}


TJobStatus  CQueue::ReturnJob(unsigned int  job_id, unsigned int  peer_addr)
{
    // FIXME: Provide fallback to
    // RegisterWorkerNodeVisit if unsuccessful
    if (!job_id)
        return CNetScheduleAPI::eJobNotFound;

    CJob                job;
    CNS_Transaction     trans(this);

    {{
        CQueueGuard             guard(this, &trans);
        CQueueJSGuard           js_guard(this, job_id, CNetScheduleAPI::ePending);


        if (job.Fetch(this, job_id) != CJob::eJF_Ok)
            return CNetScheduleAPI::eJobNotFound;


        unsigned        run_count = job.GetRunCount();
        CJobEvent *     event = job.GetLastEvent();

        if (!event)
            ERR_POST("No JobEvent for running job " << DecorateJobId(job_id));

        event = &job.AppendEvent();
        event->SetNodeAddr(peer_addr);
        event->SetStatus((TJobStatus) STemporaryEventCodes::eReturned);
        event->SetTimestamp(time(0));

        if (run_count)
            job.SetRunCount(run_count-1);

        job.SetStatus(CNetScheduleAPI::ePending);
        job.Flush(this);

        js_guard.Commit();
     }}

    trans.Commit();
    RemoveJobFromWorkerNode(job, eNSCReturned);
    TimeLineRemove(job_id);
    return (TJobStatus) STemporaryEventCodes::eReturned;
}


bool CQueue::GetJobDescr(unsigned   job_id,
                         int *      ret_code,
                         string *   input,
                         string *   output,
                         string *   err_msg,
                         string *   progress_msg,
                         TJobStatus expected_status)
{
    for (unsigned i = 0; i < 3; ++i) {
        if (i) {
            // failed to read the record (maybe looks like writer is late,
            // so we need to retry a bit later)
            ERR_POST("GetJobDescr sleep for job " + DecorateJobId(job_id));
            SleepMilliSec(300);
        }

        CJob                    job;
        CJob::EJobFetchResult   res;
        {{
            CQueueGuard         guard(this);
            res = job.Fetch(this, job_id);
        }}
        if (res == CJob::eJF_Ok) {
            if (expected_status != CNetScheduleAPI::eJobNotFound) {
                if (job.GetStatus() != expected_status) {
                    // Retry after sleep
                    continue;
                }
            }
            if (input)
                *input = job.GetInput();
            if (progress_msg)
                *progress_msg = job.GetProgressMsg();
            if (output)
                *output = job.GetOutput();

            const CJobEvent *   last_event = job.GetLastEvent();
            if (last_event) {
                if (ret_code)
                    *ret_code = last_event->GetRetCode();
                if (err_msg)
                    *err_msg = last_event->GetErrorMsg();
            }
            return true;
        }

        if (res == CJob::eJF_NotFound) {
            return false;
        } // else retry, or ?throw exception
    }

    return false; // job not found
}


void CQueue::Truncate(void)
{
    Clear();
    // Next call updates 'm_BecameEmpty' timestamp
    IsExpired(); // locks CQueue lock
}


TJobStatus  CQueue::Cancel(unsigned int  job_id, unsigned int  peer_addr)
{
    TJobStatus          old_status;
    CJob                job;
    CNS_Transaction     trans(this);

    {{
        CQueueGuard             guard(this, &trans);
        CQueueJSGuard           js_guard(this, job_id, CNetScheduleAPI::eCanceled);

        old_status = js_guard.GetOldStatus();
        if (old_status == CNetScheduleAPI::eJobNotFound ||
            old_status == CNetScheduleAPI::eCanceled) {
            // There is nothing to do if the job does not exist or it is already in
            // the canceled state
            return old_status;
        }


        if (job.Fetch(this, job_id) != CJob::eJF_Ok) {
            // The job might have just expired, i.e. does not exist any more
            return CNetScheduleAPI::eJobNotFound;
        }

        _ASSERT(job.GetStatus() == old_status);

        CJobEvent *     event = &job.AppendEvent();

        event->SetNodeAddr(peer_addr);
        event->SetStatus(CNetScheduleAPI::eCanceled);
        event->SetTimestamp(time(0));
        event->SetErrorMsg("Job canceled from " +
                           CNetScheduleAPI::StatusToString(old_status) +
                           " state");

        job.SetStatus(CNetScheduleAPI::eCanceled);
        job.Flush(this);

        js_guard.Commit();
    }}

    trans.Commit();

    TimeLineRemove(job_id);
    return old_status;
}


TJobStatus  CQueue::GetJobStatus(unsigned job_id) const
{
    return m_StatusTracker.GetStatus(job_id);
}


bool CQueue::CountStatus(CJobStatusTracker::TStatusSummaryMap*  status_map,
                         const string &                         affinity_token)
{
    unsigned        aff_id = 0;
    TNSBitVector    aff_jobs;

    if (!affinity_token.empty()) {
        aff_id = m_AffinityDict.GetTokenId(affinity_token);
        if (!aff_id)
            return false;
        GetJobsWithAffinity(aff_id, &aff_jobs);
    }

    m_StatusTracker.CountStatus(status_map, aff_id!=0 ? &aff_jobs : 0);
    return true;
}


void CQueue::SetPort(unsigned short port)
{
    if (port) {
        m_UdpSocket.SetReuseAddress(eOn);
        m_UdpSocket.Bind(port);
        m_HasNotificationPort = true;
    }
}


bool CQueue::IsExpired()
{
    time_t          empty_lifetime = GetEmptyLifetime();
    CQueueGuard     guard(this);
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


void CQueue::ReadJobs(unsigned          peer_addr,
                      unsigned          count,
                      unsigned          read_timeout,
                      unsigned &        read_id,
                      TNSBitVector &    jobs)
{
    unsigned    group_id = 0;
    time_t      curr = time(0); // TODO: move it to parameter???

    // Get jobs from status tracker here
    m_StatusTracker.SwitchJobs(count,
                               CNetScheduleAPI::eDone, CNetScheduleAPI::eReading,
                               jobs);

    CNetSchedule_JSGroupGuard   gr_guard(m_StatusTracker,
                                         CNetScheduleAPI::eDone, jobs);

    // Fix the db objects
    CJob                job;
    CNS_Transaction     trans(this);
    {{
        CQueueGuard                 guard(this, &trans);
        TNSBitVector::enumerator    en(jobs.first());
        for (; en.valid(); ++en) {
            unsigned                job_id = *en;

            if (job.Fetch(this, job_id) != CJob::eJF_Ok) {
                // TODO: check for db error here
                ERR_POST("Can't read job " << DecorateJobId(job_id));
                continue;
            }
            if (!group_id) {
                if (m_GroupLastId.Get() >= CAtomicCounter::TValue(kMax_I4))
                    m_GroupLastId.Set(0);
                group_id = (unsigned) m_GroupLastId.Add(1);
            }

            unsigned        read_count = job.GetReadCount() + 1;
            CJobEvent &     event = job.AppendEvent();

            event.SetStatus(CNetScheduleAPI::eReading);
            event.SetTimestamp(curr);
            event.SetNodeAddr(peer_addr);

            job.SetRunTimeout(read_timeout);
            job.SetStatus(CNetScheduleAPI::eReading);
            job.SetReadCount(read_count);
            job.SetReadGroup(group_id);

            job.Flush(this);

            if (m_RunTimeLine) {
                // TODO: Optimize locking of rtl lock for every object
                // hoist it out of the loop
                if (read_timeout == 0)
                    read_timeout = m_RunTimeout;

                if (read_timeout != 0) {
                    CWriteLockGuard rtl_guard(m_RunTimeLineLock);
                    m_RunTimeLine->AddObject(read_timeout, job_id);
                }
            }
        }
    }}
    trans.Commit();
    gr_guard.Commit();

    if (group_id)
        x_CreateReadGroup(group_id, jobs);
    read_id = group_id;
}


void CQueue::x_ChangeGroupStatus(unsigned               group_id,
                                 const TNSBitVector &   jobs,
                                 TJobStatus             status,
                                 unsigned int           peer_addr)
{
    time_t                  curr = time(0);
    TNSBitVector *          group_jobs = NULL;


    // Check input parameters
    {{
        CFastMutexGuard         guard(m_GroupMapLock);
        TGroupMap::iterator     reading_group = m_GroupMap.find(group_id);

        // Check that the group is valid
        if (reading_group == m_GroupMap.end())
            NCBI_THROW(CNetScheduleException, eInvalidParameter,
                        "No such read group");

        group_jobs = & reading_group->second;

        // Check that all jobs are in read group
        TNSBitVector        check(jobs);
        check -= (*group_jobs);
        if (check.any())
            NCBI_THROW(CNetScheduleException, eInvalidParameter,
                       "Jobs do not belong to group");
    }}


    // Make changes in the database and in memory status tracker
    {{
        CJob                            job;
        map<unsigned int, TJobStatus>   new_job_statuses;
        TNSBitVector::enumerator        en(jobs.first());

        CNS_Transaction                 trans(this);
        CQueueGuard                     guard(this, &trans);

        for (; en.valid(); ++en) {
            unsigned                job_id = *en;
            TJobStatus              new_status = status;

            if (job.Fetch(this, job_id) != CJob::eJF_Ok) {
                // TODO: check for db error here
                ERR_POST("Error fetching job while changing reading group "
                         "status. Job " << DecorateJobId(job_id));
                continue;
            }

            CJobEvent *     event = job.GetLastEvent();
            if (!event)
                ERR_POST("No JobEvent for reading job " << DecorateJobId(job_id));

            event = &job.AppendEvent();
            event->SetStatus(status);
            event->SetTimestamp(curr);
            event->SetNodeAddr(peer_addr);

            if (status == CNetScheduleAPI::eDone) {
                // The job is returned so the read counter needs to be clicked down
                job.SetReadCount(job.GetReadCount() - 1);
                event->SetErrorMsg("Moved to Done due to read rollback");
            }
            else if (status == CNetScheduleAPI::eReadFailed) {
                // Check the number of tries first
                if (job.GetReadCount() <= m_FailedRetries) {
                    // The job needs to be re-scheduled for reading
                    new_status = CNetScheduleAPI::eDone;
                }
            }
            job.SetStatus(new_status);
            new_job_statuses[ job_id ] = new_status;

            job.Flush(this);
            if (m_RunTimeLine) {
                // TODO: Optimize locking of rtl lock for every object
                // hoist it out of the loop
                CWriteLockGuard     rtl_guard(m_RunTimeLineLock);
                // TODO: Ineffective, better learn expiration time from
                // object, then remove it with another method
                m_RunTimeLine->RemoveObject(job_id);
            }
        }
        trans.Commit();

        // The DB has been updated, so update the memory cache with new job
        // statuses
        map<unsigned int, TJobStatus>::const_iterator  k = new_job_statuses.begin();
        for (; k != new_job_statuses.end(); ++k)
            m_StatusTracker.ChangeStatus(k->first, k->second);
    }}

    // Update the reading group and destroy it if there are no more jobs in it
    {{
        CFastMutexGuard         guard(m_GroupMapLock);
        TGroupMap::iterator     reading_group = m_GroupMap.find(group_id);

        // Remove jobs from read group, remove group if empty
        if (reading_group != m_GroupMap.end()) {
            // The group is still there
            (*group_jobs) -= jobs;
            if (!group_jobs->any())
                m_GroupMap.erase(reading_group);
        }
    }}

    return;
}


void CQueue::ConfirmJobs(unsigned               read_id,
                         const TNSBitVector &   jobs,
                         unsigned int           peer_addr)
{
    x_ChangeGroupStatus(read_id, jobs, CNetScheduleAPI::eConfirmed, peer_addr);
}


void CQueue::FailReadingJobs(unsigned               read_id,
                             const TNSBitVector &   jobs,
                             unsigned int           peer_addr)
{
    x_ChangeGroupStatus(read_id, jobs, CNetScheduleAPI::eReadFailed, peer_addr);
}


void CQueue::ReturnReadingJobs(unsigned                 read_id,
                               const TNSBitVector &     jobs,
                               unsigned int             peer_addr)
{
    x_ChangeGroupStatus(read_id, jobs, CNetScheduleAPI::eDone, peer_addr);
}


void CQueue::EraseJob(unsigned job_id)
{
    m_StatusTracker.Erase(job_id);

    {{
        // Request delayed record delete
        CFastMutexGuard     jtd_guard(m_JobsToDeleteLock);
        // TODO: Check that object is not already in these vectors,
        // saving us DB flush
        m_JobsToDelete.set_bit(job_id);
        m_DeletedJobs.set_bit(job_id);
        m_AffJobsToDelete.set_bit(job_id);

        // start affinity erase process
        m_AffWrapped = false;
        m_LastAffId  = m_CurrAffId;

        FlushDeletedVectors();
    }}
    TimeLineRemove(job_id);
}


void CQueue::Erase(const TNSBitVector& job_ids)
{
    CFastMutexGuard     jtd_guard(m_JobsToDeleteLock);
    m_JobsToDelete    |= job_ids;
    m_DeletedJobs     |= job_ids;
    m_AffJobsToDelete |= job_ids;

    // start affinity erase process
    m_AffWrapped = false;
    m_LastAffId = m_CurrAffId;

    FlushDeletedVectors();
}


void CQueue::Clear()
{
    TNSBitVector bv;

    {{
        CWriteLockGuard rtl_guard(m_RunTimeLineLock);
        // TODO: interdependency btw m_StatusTracker.lock and m_RunTimeLineLock
        m_StatusTracker.ClearAll(&bv);
        m_RunTimeLine->ReInit(0);
    }}

    Erase(bv);
}


void CQueue::FlushDeletedVectors(EVectorId vid)
{
    static EVectorId    all_ids[] = { eVIJob, eVITag, eVIAffinity };
    TNSBitVector        all_vects[] = { m_JobsToDelete, m_DeletedJobs, m_AffJobsToDelete };

    for (size_t i = 0; i < sizeof(all_ids) / sizeof(all_ids[0]); ++i) {
        if (vid != eVIAll && vid != all_ids[i])
            continue;

        m_QueueDbBlock->deleted_jobs_db.id = all_ids[i];
        TNSBitVector& bv = all_vects[i];
        bv.optimize();
        m_QueueDbBlock->deleted_jobs_db.WriteVector(bv, SDeletedJobsDB::eNoCompact);
    }
}


void CQueue::FilterJobs(TNSBitVector& ids)
{
    TNSBitVector    alive_jobs;
    m_StatusTracker.GetAliveJobs(alive_jobs);
    ids &= alive_jobs;
    //CFastMutexGuard guard(m_JobsToDeleteLock);
    //ids -= m_DeletedJobs;
}

void CQueue::ClearAffinityIdx()
{
    const unsigned  kDeletedJobsThreshold = 10000;
    const unsigned  kAffBatchSize = 1000;
    // thread-safe copies of progress pointers
    unsigned        curr_aff_id = 0;
    unsigned        last_aff_id = 0;
    {{
        // Ensure that we have some job to do
        CFastMutexGuard jtd_guard(m_JobsToDeleteLock);
        // TODO: calculate (job_to_delete_count * maturity) instead
        // of just job count. Provides more safe version - even a
        // single deleted job will be eventually cleaned up.
        if (m_AffJobsToDelete.count() < kDeletedJobsThreshold)
            return;
        curr_aff_id = m_CurrAffId;
        last_aff_id = m_LastAffId;
    }}

    TNSBitVector bv(bm::BM_GAP);

    // mark if we are wrapped and chasing the "tail"
    bool wrapped = curr_aff_id < last_aff_id;

    // get batch of affinity tokens in the index
    {{
        CFastMutexGuard guard(m_AffinityIdxLock);
        m_QueueDbBlock->affinity_idx.SetTransaction(NULL);
        CBDB_FileCursor cur(m_QueueDbBlock->affinity_idx);
        cur.SetCondition(CBDB_FileCursor::eGE);
        cur.From << curr_aff_id;

        unsigned n = 0;
        EBDB_ErrCode ret;
        for (; (ret = cur.Fetch()) == eBDB_Ok && n < kAffBatchSize; ++n) {
            curr_aff_id = m_QueueDbBlock->affinity_idx.aff_id;
            if (wrapped && curr_aff_id >= last_aff_id) // run over the tail
                break;
            bv.set(curr_aff_id);
        }
        if (ret != eBDB_Ok) {
            if (ret != eBDB_NotFound)
                ERR_POST("Error reading affinity index: " << ret);
            if (wrapped) {
                curr_aff_id = last_aff_id;
            } else {
                // wrap-around
                curr_aff_id = 0;
                wrapped = true;
                cur.SetCondition(CBDB_FileCursor::eGE);
                cur.From << curr_aff_id;
                for (; n < kAffBatchSize && (ret = cur.Fetch()) == eBDB_Ok; ++n) {
                    curr_aff_id = m_QueueDbBlock->affinity_idx.aff_id;
                    if (curr_aff_id >= last_aff_id) // run over the tail
                        break;
                    bv.set(curr_aff_id);
                }
                if (ret != eBDB_NotFound)
                    ERR_POST("Error reading affinity index");
            }
        }
    }}

    {{
        CFastMutexGuard jtd_guard(m_JobsToDeleteLock);
        m_AffWrapped = wrapped;
        m_CurrAffId = curr_aff_id;
    }}

    // clear all hanging references
    TNSBitVector::enumerator en(bv.first());
    for (; en.valid(); ++en) {
        unsigned            aff_id = *en;
        CNS_Transaction     trans(this);
        CFastMutexGuard     guard(m_AffinityIdxLock);

        m_QueueDbBlock->affinity_idx.SetTransaction(&trans);

        TNSBitVector        bvect(bm::BM_GAP);
        m_QueueDbBlock->affinity_idx.aff_id = aff_id;
        EBDB_ErrCode        ret = m_QueueDbBlock->affinity_idx.ReadVector(&bvect,
                                                                          bm::set_OR, NULL);
        if (ret != eBDB_Ok) {
            if (ret != eBDB_NotFound)
                ERR_POST("Error reading affinity index: " << ret);
            continue;
        }
        unsigned    old_count = bvect.count();
        bvect -= m_AffJobsToDelete;
        unsigned    new_count = bvect.count();
        if (new_count == old_count) {
            continue;
        }

        m_QueueDbBlock->affinity_idx.aff_id = aff_id;
        if (bvect.any()) {
            bvect.optimize();
            m_QueueDbBlock->affinity_idx.WriteVector(bvect, SAffinityIdx::eNoCompact);
        } else {
            // TODO: if there is no record in m_AffinityMap,
            // remove record from SAffinityDictDB
            // As of now, token stays indefinitely, even if empty.
            // NB: Potential DEADLOCK, see NB in FindPendingJob
            //{{
            //    CFastMutexGuard aff_guard(m_AffinityMapLock);
            //    if (!m_AffinityMap.CheckAffinity(aff_id);
            //        m_AffinityDict.RemoveToken(aff_id, trans);
            //}}
            m_QueueDbBlock->affinity_idx.Delete();
        }
        trans.Commit();
//        cout << aff_id << " cleaned" << endl;
    } // for

    {{
        CFastMutexGuard     jtd_guard(m_JobsToDeleteLock);
        if (m_AffWrapped && m_CurrAffId >= m_LastAffId) {
            m_AffJobsToDelete.clear(true);
            FlushDeletedVectors(eVIAffinity);
        }
    }}
}


void CQueue::Notify(unsigned addr, unsigned short port, unsigned job_id)
{
    if (!m_HasNotificationPort)
        return;

    char        msg[1024];
    sprintf(msg, "JNTF %u", job_id);

    CFastMutexGuard     guard(m_UdpSocketLock);
    m_UdpSocket.Send(msg, strlen(msg)+1, CSocketAPI::ntoa(addr), port);
}


void CQueue::OptimizeMem()
{
    m_StatusTracker.OptimizeMem();
}


void CQueue::AddJobsToAffinity(CBDB_Transaction&    trans,
                               unsigned             aff_id,
                               unsigned             job_id_from,
                               unsigned             job_id_to)

{
    CFastMutexGuard     guard(m_AffinityIdxLock);
    m_QueueDbBlock->affinity_idx.SetTransaction(&trans);
    TNSBitVector        bv(bm::BM_GAP);

    // check if vector is in the database

    // read vector from the file
    m_QueueDbBlock->affinity_idx.aff_id = aff_id;
    /*EBDB_ErrCode ret = */
    m_QueueDbBlock->affinity_idx.ReadVector(&bv, bm::set_OR, NULL);
    if (job_id_to == 0) {
        bv.set(job_id_from);
    } else {
        bv.set_range(job_id_from, job_id_to);
    }
    m_QueueDbBlock->affinity_idx.aff_id = aff_id;
    m_QueueDbBlock->affinity_idx.WriteVector(bv, SAffinityIdx::eNoCompact);
}


void CQueue::AddJobsToAffinity(CBDB_Transaction& trans,
                               const vector<CJob>& batch)
{
    CFastMutexGuard guard(m_AffinityIdxLock);
    m_QueueDbBlock->affinity_idx.SetTransaction(&trans);
    // TODO: Is not it easier to have map with auto_ptrs?
    // In this case we don't need this clumsy map freeing.
    typedef map<unsigned, TNSBitVector*> TBVMap;

    TBVMap      bv_map;
    try {
        unsigned    bsize = batch.size();
        for (unsigned i = 0; i < bsize; ++i) {
            const CJob&         job = batch[i];
            unsigned            aff_id = job.GetAffinityId();
            unsigned            job_id_start = job.GetId();

            TNSBitVector*       aff_bv;
            TBVMap::iterator    aff_it = bv_map.find(aff_id);

            if (aff_it == bv_map.end()) { // new element
                auto_ptr<TNSBitVector> bv(new TNSBitVector(bm::BM_GAP));
                m_QueueDbBlock->affinity_idx.aff_id = aff_id;
                /*EBDB_ErrCode ret = */
                m_QueueDbBlock->affinity_idx.ReadVector(bv.get(), bm::set_OR, NULL);
                aff_bv = bv.get();
                bv_map[aff_id] = bv.release();
            } else {
                aff_bv = aff_it->second;
            }


            // look ahead for the same affinity id
            unsigned    j;
            for (j=i+1; j < bsize; ++j) {
                if (batch[j].GetAffinityId() != aff_id) {
                    break;
                }
                _ASSERT(batch[j].GetId() == (batch[j-1].GetId()+1));
                //job_id_end = batch[j].GetId();
            }
            --j;

            if ((i!=j) && (aff_id == batch[j].GetAffinityId())) {
                unsigned job_id_end = batch[j].GetId();
                aff_bv->set_range(job_id_start, job_id_end);
                i = j;
            } else { // look ahead failed
                aff_bv->set(job_id_start);
            }

        } // for

        // save all changes to the database
        NON_CONST_ITERATE(TBVMap, it, bv_map) {
            unsigned        aff_id = it->first;
            TNSBitVector*   bv = it->second;
            bv->optimize();

            m_QueueDbBlock->affinity_idx.aff_id = aff_id;
            m_QueueDbBlock->affinity_idx.WriteVector(*bv, SAffinityIdx::eNoCompact);

            delete it->second;
            it->second = 0;
        }
    }
    catch (exception& )
    {
        NON_CONST_ITERATE(TBVMap, it, bv_map) {
            delete it->second;
            it->second = 0;
        }
        throw;
    }

}


void CQueue::x_ReadAffIdx_NoLock(unsigned      aff_id,
                                 TNSBitVector* jobs)
{
    m_QueueDbBlock->affinity_idx.aff_id = aff_id;
    m_QueueDbBlock->affinity_idx.ReadVector(jobs, bm::set_OR);
}


void CQueue::GetJobsWithAffinities(const TNSBitVector& aff_id_set,
                                   TNSBitVector*       jobs)
{
    CFastMutexGuard     guard(m_AffinityIdxLock);
    m_QueueDbBlock->affinity_idx.SetTransaction(NULL);
    TNSBitVector::enumerator en(aff_id_set.first());
    for (; en.valid(); ++en) {  // for each affinity id
        unsigned        aff_id = *en;
        x_ReadAffIdx_NoLock(aff_id, jobs);
    }
}


void CQueue::GetJobsWithAffinity(unsigned      aff_id,
                                 TNSBitVector* jobs)
{
    CFastMutexGuard guard(m_AffinityIdxLock);
    m_QueueDbBlock->affinity_idx.SetTransaction(NULL);
    x_ReadAffIdx_NoLock(aff_id, jobs);
}


class CAffinityResolver : public IAffinityResolver
{
public:
    CAffinityResolver(CQueue& queue) : m_Queue(queue)
    {}

    virtual void GetJobsWithAffinities(const TNSBitVector& aff_id_set,
                                       TNSBitVector*       jobs)
    {
        m_Queue.GetJobsWithAffinities(aff_id_set, jobs);
    }

private:
    CQueue&     m_Queue;
};


unsigned CQueue::FindPendingJob(CWorkerNode* worker_node,
                                const list<string>& aff_list,
                                time_t curr, bool keep_affinity)
{
    unsigned                job_id = 0;
    TNSBitVector            blacklisted_jobs;
    TNSBitVector            aff_ids;
    const TNSBitVector*     effective_aff_ids = 0;

    // request specified affinity explicitly - client managed affinity
    bool                    is_specific_aff = !aff_list.empty();
    m_AffinityDict.GetTokensIds(aff_list, aff_ids);
    if (is_specific_aff && !aff_ids.any())
        // Requested affinities are not known to the server at all
        return 0;

    // affinity: get list of job candidates
    // previous FindPendingJob() call may have precomputed candidate job ids
    {{
        CWorkerNodeAffinityGuard na(*worker_node);

        if (is_specific_aff)
            na.CleanCandidates(aff_ids);

        // If we have a specific affinity request, use it for job search,
        // otherwise use what we have for the node
        if (is_specific_aff) effective_aff_ids = &aff_ids;
        // else effective_aff_ids = &na.GetAffinities(curr);

        // 'blacklisted_jobs' are also required in the code, finding new
        // affinity association below
        blacklisted_jobs = na.GetBlacklistedJobs(curr);
        CAffinityResolver affinity_resolver(*this);
        job_id = na.GetJobWithAffinities(effective_aff_ids,
                                         m_StatusTracker,
                                         affinity_resolver);
        if (job_id == unsigned(-1) ||
            (!job_id && (is_specific_aff || keep_affinity))) {
                return 0;
        }
    }}

    // No affinity association or there are no more jobs with
    // established affinity

    // try to find a vacant(not taken by any other worker node) affinity id
    if (false && !job_id) { // DEBUG <- this is the most expensive affinity op, disable it for now
        TNSBitVector assigned_aff;
        GetAllAssignedAffinities(curr, assigned_aff);

        if (assigned_aff.any()) {
            // get all jobs belonging to other (already assigned) affinities,
            // ORing them with our own blacklisted jobs
            TNSBitVector assigned_candidate_jobs(blacklisted_jobs);
            // GetJobsWithAffinity actually ORs into second argument
            GetJobsWithAffinities(assigned_aff, &assigned_candidate_jobs);
            // we got list of jobs we do NOT want to schedule, use them as
            // blacklisted to get a job with possibly with unassigned affinity
            // (or without affinity at all).
            bool pending_jobs_avail =
                m_StatusTracker.GetPendingJob(assigned_candidate_jobs,
                                             &job_id);
            if (!job_id && !pending_jobs_avail)
                return 0;
        }
    }

    // We just take the first available job in the queue, taking into account
    // blacklisted jobs as usual.
    if (!job_id)
        m_StatusTracker.GetPendingJob(blacklisted_jobs, &job_id);

    return job_id;
}


struct SAffinityJobs
{
    string                  aff_token;
    unsigned                job_count;
    list<TWorkerNodeRef>    nodes;
};
typedef map<unsigned, SAffinityJobs> TAffinities;


string CQueue::GetAffinityList()
{
    unsigned        aff_id;
    unsigned        bv_cnt;
    TAffinities     affinities;
    time_t          now = time(0);

    {{
        CFastMutexGuard     guard(m_AffinityIdxLock);
        m_QueueDbBlock->affinity_idx.SetTransaction(NULL);
        CBDB_FileCursor cur(m_QueueDbBlock->affinity_idx);
        cur.SetCondition(CBDB_FileCursor::eGE);
        cur.From << 0;

        EBDB_ErrCode    ret;
        // for every affinity
        for (; (ret = cur.Fetch()) == eBDB_Ok;) {
            aff_id = m_QueueDbBlock->affinity_idx.aff_id;
            TNSBitVector bvect(bm::BM_GAP);
            ret = m_QueueDbBlock->affinity_idx.ReadVector(&bvect, bm::set_OR);
            m_StatusTracker.PendingIntersect(&bvect);
            // how many pending tasks with this affinity
            bv_cnt = bvect.count();
            if (ret != eBDB_Ok) {
                if (ret != eBDB_NotFound)
                    ERR_POST("Error reading affinity index: " << ret);
                continue;
            }
            SAffinityJobs   aff_jobs;

            aff_jobs.aff_token = m_AffinityDict.GetAffToken(aff_id);
            aff_jobs.job_count = bv_cnt;
            affinities[aff_id] = aff_jobs;
            // what are the nodes executing this affinity
        }
    }}

    {{
        CQueueWorkerNodeListGuard   wnl(m_WorkerNodeList);
        list<TWorkerNodeRef>        nodes;
        wnl.GetNodes(now, nodes);
        NON_CONST_ITERATE(list<TWorkerNodeRef>, node_it, nodes) {
            TNSBitVector::enumerator
                            en(wnl.GetNodeAffinities(now, *node_it).first());
            for (; en.valid(); ++en) {
                unsigned    aff_id = *en;
                affinities[aff_id].nodes.push_back(*node_it);
            }
        }
    }}

    string          aff_list;
    ITERATE(TAffinities, it, affinities) {
        if (!aff_list.empty())
            aff_list += "&";

        aff_list += NStr::URLEncode(it->second.aff_token) + '=' +
                    NStr::UIntToString(it->second.job_count);
        ITERATE(list<TWorkerNodeRef>, node_it, it->second.nodes) {
            aff_list += ',' + (*node_it)->GetId();
        }
    }
    return aff_list;
}


bool CQueue::FailJob(CWorkerNode *      worker_node,
                     unsigned int       peer_addr,
                     unsigned           job_id,
                     const string &     err_msg,
                     const string &     output,
                     int                ret_code)
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

    // We first change memory state to "Failed", it is safer because
    // there is only danger to find job in inconsistent state, and because
    // Failed is terminal, usually you can not allocate job or do anything
    // disturbing from this state.
    CQueueJSGuard       js_guard(this, job_id, CNetScheduleAPI::eFailed);
    CJob                job;
    CNS_Transaction     trans(this);
    time_t              curr = time(0);
    bool                rescheduled = false;
    {{
        CQueueGuard             guard(this, &trans);
        CJob::EJobFetchResult   res = job.Fetch(this, job_id);
        if (res != CJob::eJF_Ok) {
            // TODO: Integrity error or job just expired?
            if (m_Log)
                LOG_POST(Error << "Can not fetch job " << DecorateJobId(job_id));
            return false;
        }

        unsigned                run_count = job.GetRunCount();
        if (run_count <= failed_retries) {
            time_t  exp_time = blacklist_time ? curr + blacklist_time : 0;
            BlacklistJob(worker_node, job_id, exp_time);
            job.SetStatus(CNetScheduleAPI::ePending);
            js_guard.SetStatus(CNetScheduleAPI::ePending);
            rescheduled = true;
            if (m_Log)
                LOG_POST(Error <<"Job " << DecorateJobId(job_id)
                               << " rescheduled with "
                               << (failed_retries - run_count)
                               << " retries left");
        } else {
            job.SetStatus(CNetScheduleAPI::eFailed);
            rescheduled = false;
            if (m_Log)
                LOG_POST(Error << "Job " << DecorateJobId(job_id)
                               << " failed");
        }

        CJobEvent *     event = job.GetLastEvent();
        if (!event) {
            ERR_POST("No JobEvent for running job " << DecorateJobId(job_id));
        }

        event = &job.AppendEvent();
        event->SetStatus(CNetScheduleAPI::eFailed);
        event->SetTimestamp(curr);
        event->SetErrorMsg(err_msg);
        event->SetRetCode(ret_code);
        event->SetNodeAddr(peer_addr);
        job.SetOutput(output);

        job.Flush(this);
    }}
    RemoveJobFromWorkerNode(job, eNSCFailed);


    trans.Commit();
    js_guard.Commit();

    if (m_RunTimeLine) {
        CWriteLockGuard guard(m_RunTimeLineLock);
        m_RunTimeLine->RemoveObject(job_id);
    }

    if (!rescheduled  &&  job.ShouldNotify(curr)) {
        Notify(job.GetSubmAddr(), job.GetSubmPort(), job_id);
    }

    return true;
}


/// Specified status is OR-ed with the target vector
void CQueue::JobsWithStatus(TJobStatus    status,
                            TNSBitVector* bv) const
{
    m_StatusTracker.StatusSnapshot(status, bv);
}


// Functor for EQ
class CQueryFunctionEQ : public CQueryFunction_BV_Base<TNSBitVector>
{
public:
    CQueryFunctionEQ(CQueue& queue) :
      m_Queue(queue)
      {}

      typedef CQueryFunction_BV_Base<TNSBitVector>  TParent;
      typedef TParent::TBVContainer::TBuffer        TBuffer;
      typedef TParent::TBVContainer::TBitVector     TBitVector;
      virtual void Evaluate(CQueryParseTree::TNode& qnode);
private:
    void x_CheckArgs(const CQueryFunctionBase::TArgVector& args);
    CQueue& m_Queue;
};


void CQueryFunctionEQ::Evaluate(CQueryParseTree::TNode& qnode)
{
    //NcbiCout << "Key: " << key << " Value: " << val << NcbiEndl;
    CQueryFunctionBase::TArgVector      args;
    this->MakeArgVector(qnode, args);
    x_CheckArgs(args);
    const                   string& key = args[0]->GetValue().GetStrValue();
    const                   string& val = args[1]->GetValue().GetStrValue();
    auto_ptr<TNSBitVector>  bv;
    auto_ptr<TBuffer>       buf;

    if (key == "status") {
        // special case for status
        CNetScheduleAPI::EJobStatus status =
            CNetScheduleAPI::StringToStatus(val);
        if (status == CNetScheduleAPI::eJobNotFound)
            NCBI_THROW(CNetScheduleException,
            eQuerySyntaxError, string("Unknown status: ") + val);
        bv.reset(new TNSBitVector);
        m_Queue.JobsWithStatus(status, bv.get());
    } else if (key == "id") {
        unsigned job_id = CNetScheduleKey(val).id;
        bv.reset(new TNSBitVector);
        bv->set(job_id);
    } else {
        if (val == "*") {
            // wildcard
            bv.reset(new TNSBitVector);
            m_Queue.ReadTags(key, bv.get());
        } else {
            buf.reset(new TBuffer);
            if (!m_Queue.ReadTag(key, val, buf.get())) {
                // Signal empty set by setting empty bitvector
                bv.reset(new TNSBitVector());
                buf.reset(NULL);
            }
        }
    }
    if (qnode.GetValue().IsNot()) {
        // Apply NOT here
        if (bv.get()) {
            bv->invert();
        } else if (buf.get()) {
            bv.reset(new TNSBitVector());
            bm::operation_deserializer<TNSBitVector>::deserialize(*bv,
                &((*buf.get())[0]),
                0,
                bm::set_ASSIGN);
            bv.get()->invert();
        }
    }
    if (bv.get())
        this->MakeContainer(qnode)->SetBV(bv.release());
    else if (buf.get())
        this->MakeContainer(qnode)->SetBuffer(buf.release());
}


void CQueryFunctionEQ::x_CheckArgs(const CQueryFunctionBase::TArgVector& args)
{
    if (args.size() != 2 ||
        (args[0]->GetValue().GetType() != CQueryParseNode::eIdentifier  &&
        args[0]->GetValue().GetType() != CQueryParseNode::eString)) {
            NCBI_THROW(CNetScheduleException,
                eQuerySyntaxError, "Wrong arguments for '='");
    }
}


TNSBitVector* CQueue::ExecSelect(const string& query, list<string>& fields)
{
    CQueryParseTree qtree;
    try {
        qtree.Parse(query.c_str());
    } catch (CQueryParseException& ex) {
        NCBI_THROW(CNetScheduleException, eQuerySyntaxError, ex.GetMsg());
    }
    CQueryParseTree::TNode* top = qtree.GetQueryTree();
    if (!top)
        NCBI_THROW(CNetScheduleException,
        eQuerySyntaxError, "Query syntax error in parse");

    if (top->GetValue().GetType() == CQueryParseNode::eSelect) {
        // Find where clause here
        typedef CQueryParseTree::TNode::TNodeList_I TNodeIterator;
        for (TNodeIterator it = top->SubNodeBegin();
            it != top->SubNodeEnd(); ++it) {
                CQueryParseTree::TNode* node = *it;
                CQueryParseNode::EType node_type = node->GetValue().GetType();
                if (node_type == CQueryParseNode::eList) {
                    for (TNodeIterator it2 = node->SubNodeBegin();
                        it2 != node->SubNodeEnd(); ++it2) {
                            fields.push_back((*it2)->GetValue().GetStrValue());
                    }
                }
                if (node_type == CQueryParseNode::eWhere) {
                    TNodeIterator it2 = node->SubNodeBegin();
                    if (it2 == node->SubNodeEnd())
                        NCBI_THROW(CNetScheduleException,
                        eQuerySyntaxError,
                        "Query syntax error in WHERE clause");
                    top = (*it2);
                    break;
                }
        }
    }

    // Execute 'select' phase.
    CQueryExec qexec;
    qexec.AddFunc(CQueryParseNode::eAnd,
        new CQueryFunction_BV_Logic<TNSBitVector>(bm::set_AND));
    qexec.AddFunc(CQueryParseNode::eOr,
        new CQueryFunction_BV_Logic<TNSBitVector>(bm::set_OR));
    qexec.AddFunc(CQueryParseNode::eSub,
        new CQueryFunction_BV_Logic<TNSBitVector>(bm::set_SUB));
    qexec.AddFunc(CQueryParseNode::eXor,
        new CQueryFunction_BV_Logic<TNSBitVector>(bm::set_XOR));
    qexec.AddFunc(CQueryParseNode::eIn,
        new CQueryFunction_BV_In_Or<TNSBitVector>());
    qexec.AddFunc(CQueryParseNode::eNot,
        new CQueryFunction_BV_Not<TNSBitVector>());

    qexec.AddFunc(CQueryParseNode::eEQ,
        new CQueryFunctionEQ(*this));

    {{
        CFastMutexGuard guard(GetTagLock());
        SetTagDbTransaction(NULL);
        qexec.Evaluate(qtree, *top);
    }}

    IQueryParseUserObject* uo = top->GetValue().GetUserObject();
    if (!uo)
        NCBI_THROW(CNetScheduleException,
        eQuerySyntaxError, "Query syntax error in eval");
    typedef CQueryEval_BV_Value<TNSBitVector> BV_UserObject;
    BV_UserObject* result =
        dynamic_cast<BV_UserObject*>(uo);
    _ASSERT(result);
    auto_ptr<TNSBitVector> bv(result->ReleaseBV());
    if (!bv.get()) {
        bv.reset(new TNSBitVector());
        BV_UserObject::TBuffer *buf = result->ReleaseBuffer();
        if (buf && buf->size()) {
            bm::operation_deserializer<TNSBitVector>::deserialize(*bv,
                &((*buf)[0]),
                0,
                bm::set_ASSIGN);
        }
    }
    // Filter against deleted jobs
    FilterJobs(*(bv.get()));

    return bv.release();
}


void CQueue::PrepareFields(SFieldsDescription& field_descr,
                           const list<string>& fields)
{
    field_descr.Init();

    // Verify the fields, and convert them into field numbers
    ITERATE(list<string>, it, fields) {
        const string& field_name = NStr::TruncateSpaces(*it);
        string tag_name;
        SFieldsDescription::EFieldType ftype;
        int i;
        if ((i = CJob::GetFieldIndex(field_name)) >= 0) {
            ftype = SFieldsDescription::eFT_Job;
            if (field_name == "affinity")
                field_descr.has_affinity = true;
        } else if ((i = CJobEvent::GetFieldIndex(field_name)) >= 0) {
            ftype = SFieldsDescription::eFT_Run;
            // field_descr.has_run = true;
        } else if (field_name == "event") {
            ftype = SFieldsDescription::eFT_RunNum;
            field_descr.has_run = true;
        } else if (NStr::StartsWith(field_name, "tag.")) {
            ftype = SFieldsDescription::eFT_Tag;
            tag_name = field_name.substr(4);
            field_descr.has_tags = true;
        } else {
            NCBI_THROW(CNetScheduleException, eQuerySyntaxError,
                string("Unknown field: ") + (*it));
        }
        SFieldsDescription::FFormatter formatter = NULL;
        if (i >= 0) {
            if (field_name == "id") {
                formatter = FormatNSId;
            } else if (field_name == "node_addr" ||
                field_name == "subm_addr") {
                    formatter = FormatHostName;
            }
        }
        field_descr.field_types.push_back(ftype);
        field_descr.field_nums.push_back(i);
        field_descr.formatters.push_back(formatter);
        field_descr.pos_to_tag.push_back(tag_name);
    }
}


void CQueue::ExecProject(TRecordSet&               record_set,
                         const TNSBitVector&       ids,
                         const SFieldsDescription& field_descr)
{
    int                         record_size = field_descr.field_nums.size();
    // Retrieve fields
    CJob                        job;
    TNSBitVector::enumerator    en(ids.first());
    CQueueGuard                 guard(this);

    for ( ; en.valid(); ++en) {
        map<string, string>     tags;
        unsigned                job_id = *en;

        // FIXME: fetch minimal required part of the job based on
        // field_descr.has_* flags, may be add has_input_output flag
        CJob::EJobFetchResult res = job.Fetch(this, job_id);
        if (res != CJob::eJF_Ok) {
            if (res != CJob::eJF_NotFound)
                ERR_POST("Error reading queue job db");
            continue;
        }
        if (field_descr.has_affinity)
            job.FetchAffinityToken(this);
        if (field_descr.has_tags) {
            // Parse tags record
            ITERATE(TNSTagList, it, job.GetTags()) {
                tags[it->first] = it->second;
            }
        }
        // Fill out the record
        if (!field_descr.has_run) {
            vector<string> record(record_size);
            if (x_FillRecord(record, field_descr, job, tags, job.GetLastEvent(), 0))
                record_set.push_back(record);
        } else {
            int run_num = 1;
            ITERATE(vector<CJobEvent>, it, job.GetEvents()) {
                vector<string> record(record_size);
                if (x_FillRecord(record, field_descr, job, tags, &(*it), run_num++))
                    record_set.push_back(record);
            }
        }
    }
}


bool CQueue::x_FillRecord(vector<string> &            record,
                          const SFieldsDescription &  field_descr,
                          const CJob &                job,
                          map<string, string> &       tags,
                          const CJobEvent *           event,
                          int                         event_num)
{
    int         record_size = field_descr.field_nums.size();
    bool        complete = true;

    for (int i = 0; i < record_size; ++i) {
        int         fnum = field_descr.field_nums[i];

        switch(field_descr.field_types[i]) {
        case SFieldsDescription::eFT_Job:
            record[i] = job.GetField(fnum);
            break;
        case SFieldsDescription::eFT_Run:
            if (event)
                record[i] = event->GetField(fnum);
            else
                record[i] = "NULL";
            break;
        case SFieldsDescription::eFT_Tag:
            {
                map<string, string>::iterator it =
                    tags.find((field_descr.pos_to_tag)[i]);
                if (it == tags.end()) {
                    complete = false;
                } else {
                    record[i] = it->second;
                }
            }
            break;
        case SFieldsDescription::eFT_RunNum:
            record[i] = NStr::IntToString(event_num);
            break;
        }
        if (!complete) break;
    }
    return complete;
}


//////////////////////////////////////////////////////////////////////////
// Worker node


void CQueue::ClearWorkerNode(CWorkerNode *      worker_node,
                             unsigned int       peer_addr,
                             const string &     reason)
{
    TJobList jobs;
    m_WorkerNodeList.ClearNode(worker_node, jobs);
    x_FailJobs(jobs, worker_node, peer_addr, "Node closed, " + reason);
}


void CQueue::ClearWorkerNode(const string &     node_id,
                             unsigned int       peer_addr,
                             const string &     reason)
{
    TJobList        jobs;
    TWorkerNodeRef  worker_node = m_WorkerNodeList.ClearNode(node_id, jobs);

    if (worker_node)
        x_FailJobs(jobs, worker_node, peer_addr, "Node closed, " + reason);
}


void CQueue::x_FailJobs(const TJobList &    jobs,
                        CWorkerNode *       worker_node,
                        unsigned int        peer_addr,
                        const string &      err_msg)
{
    ITERATE(TJobList, it, jobs) {
        FailJob(worker_node, peer_addr, *it, err_msg, "", 0);
    }
}


void CQueue::NotifyListeners(bool unconditional, unsigned aff_id)
{
    // TODO: if affinity valency is full for this aff_id, notify only nodes
    // with this aff_id, otherwise notify all nodes in the hope that some
    // of them will pick up the task with this aff_id
    if (!m_HasNotificationPort)
        return;

    int                         notify_timeout = GetNotifyTimeout();
    time_t                      curr = time(0);
    list<TWorkerNodeHostPort>   notify_list;
    if ((unconditional || m_StatusTracker.AnyPending()) &&
        m_WorkerNodeList.GetNotifyList(unconditional, curr,
                                       notify_timeout, notify_list)) {

#define JSQ_PREFIX "NCBI_JSQ_"
        char msg[256];
        strcpy(msg, JSQ_PREFIX);
        strncat(msg, m_QueueName.c_str(), sizeof(msg) - sizeof(JSQ_PREFIX) - 1);
        size_t msg_len = sizeof(JSQ_PREFIX) + m_QueueName.length();

        unsigned i = 0;
        ITERATE(list<TWorkerNodeHostPort>, it, notify_list) {
            unsigned host = it->first;
            unsigned short port = it->second;
            {{
                CFastMutexGuard guard(m_UdpSocketLock);
                //EIO_Status status =
                    m_UdpSocket.Send(msg, msg_len,
                                       CSocketAPI::ntoa(host), port);
            }}
            // periodically check if we have no more jobs left
            if ((++i % 10 == 0) && !m_StatusTracker.AnyPending())
                break;
        }
    }
}


void CQueue::PrintWorkerNodeStat(CNetScheduleHandler &  handler,
                                 time_t                 curr,
                                 EWNodeFormat           fmt) const
{
    list<TWorkerNodeRef>        nodes;

    m_WorkerNodeList.GetNodes(curr, nodes);
    ITERATE(list<TWorkerNodeRef>, it, nodes) {
        handler.WriteMessage("OK:", (*it)->AsString(curr, fmt));
    }
}


void CQueue::UnRegisterNotificationListener(CWorkerNode *   worker_node,
                                            unsigned int    peer_addr)
{
    if (m_WorkerNodeList.UnRegisterNotificationListener(worker_node)) {
        // Clean affinity association only for old style worker nodes
        // New style nodes should explicitly call ClearWorkerNode
        ClearWorkerNode(worker_node, peer_addr, "URGC");
    }
}


void CQueue::AddJobToWorkerNode(CWorkerNode *   worker_node,
                                const CJob &    job,
                                time_t          exp_time)
{
    m_WorkerNodeList.AddJob(worker_node, job, exp_time);
    CountEvent(eStatGetEvent);
}


void CQueue::UpdateWorkerNodeJob(unsigned job_id, time_t exp_time)
{
    m_WorkerNodeList.UpdateJob(this, job_id, exp_time);
}


void CQueue::RemoveJobFromWorkerNode(const CJob&   job,
                                     ENSCompletion reason)
{
    m_WorkerNodeList.RemoveJob(this, job, reason);
    CountEvent(eStatPutEvent);
}


// Affinity

void CQueue::GetAllAssignedAffinities(time_t t, TNSBitVector& aff_ids)
{
    CQueueWorkerNodeListGuard wnl(m_WorkerNodeList);
    wnl.GetAffinities(t, aff_ids);
}


void CQueue::AddAffinity(CWorkerNode* worker_node,
                         unsigned     aff_id,
                         time_t       exp_time)
{
    CWorkerNodeAffinityGuard na(*worker_node);
    CStopWatch sw(CStopWatch::eStart);
    na.AddAffinity(aff_id,exp_time);
//    LOG_POST(Warning << "Added affinity1: " << sw.Elapsed() * 1000 << "ms");
}


void CQueue::BlacklistJob(CWorkerNode*  worker_node,
                          unsigned      job_id,
                          time_t        exp_time)
{
    CWorkerNodeAffinityGuard na(*worker_node);
    na.BlacklistJob(job_id, exp_time);
}


// Tags

void CQueue::SetTagDbTransaction(CBDB_Transaction* trans)
{
    m_QueueDbBlock->tag_db.SetTransaction(trans);
}


void CQueue::AppendTags(CNSTagMap&        tag_map,
                        const TNSTagList& tags,
                        unsigned          job_id)
{
    ITERATE(TNSTagList, it, tags) {
        /*
        auto_ptr<TNSBitVector> bv(new TNSBitVector(bm::BM_GAP));
        pair<TNSTagMap::iterator, bool> tag_map_it =
            (*tag_map).insert(TNSTagMap::value_type((*it), bv.get()));
        if (tag_map_it.second) {
            bv.release();
        }
        */
        TNSTagMap::iterator tag_it = (*tag_map).find((*it));
        if (tag_it == (*tag_map).end()) {
            pair<TNSTagMap::iterator, bool> tag_map_it =
//                (*tag_map).insert(TNSTagMap::value_type((*it), new TNSBitVector(bm::BM_GAP)));
                (*tag_map).insert(TNSTagMap::value_type((*it), new TNSShortIntSet));
            tag_it = tag_map_it.first;
        }
//        tag_it->second->set(job_id);
        tag_it->second->push_back(job_id);
    }
}


void CQueue::FlushTags(CNSTagMap& tag_map, CBDB_Transaction& trans)
{
    CFastMutexGuard guard(m_TagLock);
    m_QueueDbBlock->tag_db.SetTransaction(&trans);
    NON_CONST_ITERATE(TNSTagMap, it, *tag_map) {
        m_QueueDbBlock->tag_db.key = it->first.first;
        m_QueueDbBlock->tag_db.val = it->first.second;
        /*
        EBDB_ErrCode err = m_QueueDbBlock->tag_db.ReadVector(it->second, bm::set_OR);
        if (err != eBDB_Ok && err != eBDB_NotFound) {
            // TODO: throw db error
        }
        m_QueueDbBlock->tag_db.key = it->first.first;
        m_QueueDbBlock->tag_db.val = it->first.second;
        it->second->optimize();
        if (it->first.first == "transcript") {
            it->second->stat();
        }
        m_QueueDbBlock->tag_db.WriteVector(*(it->second), STagDB::eNoCompact);
        */

        TNSBitVector bv_tmp(bm::BM_GAP);
        EBDB_ErrCode err = m_QueueDbBlock->tag_db.ReadVector(&bv_tmp);
        if (err != eBDB_Ok && err != eBDB_NotFound) {
            // TODO: throw db error
        }
        bm::combine_or(bv_tmp, it->second->begin(), it->second->end());

        m_QueueDbBlock->tag_db.key = it->first.first;
        m_QueueDbBlock->tag_db.val = it->first.second;
        m_QueueDbBlock->tag_db.WriteVector(bv_tmp, STagDB::eNoCompact);

        delete it->second;
        it->second = 0;
    }
    (*tag_map).clear();
}


bool CQueue::ReadTag(const string& key,
                     const string& val,
                     TBuffer*      buf)
{
    // Guarded by m_TagLock through GetTagLock()
    CBDB_FileCursor cur(m_QueueDbBlock->tag_db);
    cur.SetCondition(CBDB_FileCursor::eEQ);
    cur.From << key << val;

    return cur.Fetch(buf) == eBDB_Ok;
}


void CQueue::ReadTags(const string& key, TNSBitVector* bv)
{
    // Guarded by m_TagLock through GetTagLock()
    CBDB_FileCursor     cur(m_QueueDbBlock->tag_db);
    cur.SetCondition(CBDB_FileCursor::eEQ);
    cur.From << key;

    TBuffer             buf;
    bm::set_operation   op_code = bm::set_ASSIGN;
    EBDB_ErrCode        err;
    while ((err = cur.Fetch(&buf)) == eBDB_Ok) {
        bm::operation_deserializer<TNSBitVector>::deserialize(
            *bv, &(buf[0]), 0, op_code);
        op_code = bm::set_OR;
    }
    if (err != eBDB_Ok  &&  err != eBDB_NotFound) {
        // TODO: signal disaster somehow, e.g. throw CBDB_Exception
    }
}


void CQueue::x_RemoveTags(CBDB_Transaction&   trans,
                          const TNSBitVector& ids)
{
    CFastMutexGuard guard(m_TagLock);
    m_QueueDbBlock->tag_db.SetTransaction(&trans);
    CBDB_FileCursor cur(m_QueueDbBlock->tag_db, trans,
                        CBDB_FileCursor::eReadModifyUpdate);
    // iterate over tags database, deleting ids from every entry
    cur.SetCondition(CBDB_FileCursor::eFirst);
    CBDB_RawFile::TBuffer buf;
    TNSBitVector bv;
    while (cur.Fetch(&buf) == eBDB_Ok) {
        bm::deserialize(bv, &buf[0]);
        unsigned before_remove = bv.count();
        bv -= ids;
        unsigned new_count;
        if ((new_count = bv.count()) != before_remove) {
            if (new_count) {
                TNSBitVector::statistics st;
                bv.optimize(0, TNSBitVector::opt_compress, &st);
                if (st.max_serialize_mem > buf.size()) {
                    buf.resize(st.max_serialize_mem);
                }

                size_t size = bm::serialize(bv, &buf[0]);
                cur.UpdateBlob(&buf[0], size);
            } else {
                cur.Delete(CBDB_File::eIgnoreError);
            }
        }
        bv.clear(true);
    }
}


//

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
    CJob                job;
    unsigned            time_start = 0;
    unsigned            run_timeout = 0;
    time_t              exp_time = 0;
    TJobStatus          status;

    CNS_Transaction     trans(this);

    {{
        CQueueGuard     guard(this, &trans);
        TJobStatus      new_status, event_status;

        status = GetJobStatus(job_id);
        if (status == CNetScheduleAPI::eRunning) {
            new_status = CNetScheduleAPI::ePending;
            event_status = (TJobStatus) STemporaryEventCodes::eTimeout;
        } else if (status == CNetScheduleAPI::eReading) {
            new_status = CNetScheduleAPI::eDone;
            event_status = (TJobStatus) STemporaryEventCodes::eReadTimeout;
        } else
            return; // Execution timeout is for Running and Reading jobs only


        if (job.Fetch(this, job_id) != CJob::eJF_Ok) {
            return;
        }

        if (job.GetStatus() != status) {
            ERR_POST("Job status mismatch between status tracker" <<
                     " and database for job " << DecorateJobId(job_id));
            return;
        }

        CJobEvent *     event = job.GetLastEvent();
        if (!event) {
            ERR_POST("No JobEvent for running/reading job " << DecorateJobId(job_id));
            // fix it here as well as we can
            event = &job.AppendEvent();
            event->SetTimestamp(curr_time);
        }
        time_start = event->GetTimestamp();
        _ASSERT(time_start);
        run_timeout = job.GetRunTimeout();
        if (run_timeout == 0)
            run_timeout = queue_run_timeout;

        exp_time = run_timeout ? time_start + run_timeout : 0;
        if (curr_time < exp_time) {
            // we need to register job in time line
            TimeLineAdd(job_id, exp_time);
            return;
        }

        if (status == CNetScheduleAPI::eReading)
            x_RemoveFromReadGroup(job.GetReadGroup(), job_id);

        job.SetStatus(new_status);

        event = &job.AppendEvent();
        event->SetStatus(event_status);
        event->SetTimestamp(curr_time);

        m_StatusTracker.SetStatus(job_id, new_status);
        job.Flush(this);
    }}

    trans.Commit();


    if (status == CNetScheduleAPI::eRunning)
        m_WorkerNodeList.RemoveJob(this, job, eNSCTimeout);

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
                .Print("msg_code", eNSCTimeout)
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
    unsigned queue_timeout, queue_run_timeout;
    {{
        CQueueParamAccessor qp(*this);
        queue_timeout = qp.GetTimeout();
        queue_run_timeout = qp.GetRunTimeout();
    }}

    TNSBitVector job_ids;
    TNSBitVector not_found_jobs;
    time_t curr = time(0);
    CJob job;
    unsigned del_count = 0;
    bool has_db_error = false;
#ifdef _DEBUG
    static bool debug_job_discrepancy = false;
#endif
    {{
        CQueueGuard guard(this);
        unsigned job_id = 0;
        for (unsigned n = 0; n < batch_size; ++n) {
            job_id = m_StatusTracker.GetNext(status, job_id);
            if (job_id == 0)
                break;
            // FIXME: should fetch only main part of the job and event info,
            // does not need potentially huge tags and input/output
            CJob::EJobFetchResult res = job.Fetch(this, job_id);
            if (res != CJob::eJF_Ok
#ifdef _DEBUG
                || debug_job_discrepancy
#endif
            ) {
                if (res != CJob::eJF_NotFound)
                    has_db_error = true;
                // ? already deleted - report as a batch later
                not_found_jobs.set_bit(job_id);
                m_StatusTracker.Erase(job_id);
                // Do not break the process of erasing expired jobs
                continue;
            }

            // Is the job expired?
            if (job.GetJobExpirationTime(queue_timeout,
                                         queue_run_timeout) <= curr) {
                m_StatusTracker.Erase(job_id);
                job_ids.set_bit(job_id);
                if (status == CNetScheduleAPI::eReading)
                    x_RemoveFromReadGroup(job.GetReadGroup(), job_id);
                ++del_count;
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
    CWriteLockGuard guard(m_RunTimeLineLock);
    m_RunTimeLine->MoveObject(old_time, new_time, job_id);
}


void CQueue::TimeLineAdd(unsigned job_id, time_t timeout)
{
    if (!job_id  ||  !m_RunTimeLine  ||  !timeout)
        return;

    CWriteLockGuard guard(m_RunTimeLineLock);
    m_RunTimeLine->AddObject(timeout, job_id);
}


void CQueue::TimeLineRemove(unsigned job_id)
{
    if (!m_RunTimeLine)
        return;

    CWriteLockGuard guard(m_RunTimeLineLock);
    m_RunTimeLine->RemoveObject(job_id);

    // I think it is extra
    // if (m_Log)
    //     LOG_POST(Error << "Job removed from time line, job: "
    //                    << DecorateJobId(job_id));
}


void CQueue::TimeLineExchange(unsigned remove_job_id,
                              unsigned add_job_id,
                              time_t   timeout)
{
    if (!m_RunTimeLine)
        return;

    CWriteLockGuard guard(m_RunTimeLineLock);
    if (remove_job_id)
        m_RunTimeLine->RemoveObject(remove_job_id);
    if (add_job_id)
        m_RunTimeLine->AddObject(timeout, add_job_id);


    // I think it is extra
    // if (m_Log) {
    //     if (remove_job_id)
    //         LOG_POST(Error << "Job removed from time line, job: "
    //                        << DecorateJobId(remove_job_id));
    //     if (add_job_id)
    //         LOG_POST(Error << "Job added to time line, job: "
    //                        << DecorateJobId(add_job_id));
    // }
}


unsigned CQueue::DeleteBatch(unsigned batch_size)
{
    TNSBitVector batch;
    {{
        CFastMutexGuard guard(m_JobsToDeleteLock);
        unsigned job_id = 0;
        for (unsigned n = 0; n < batch_size &&
                             (job_id = m_JobsToDelete.extract_next(job_id));
             ++n)
        {
            batch.set(job_id);
        }
        if (batch.any())
            FlushDeletedVectors(eVIJob);
    }}
    if (batch.none()) return 0;

    unsigned actual_batch_size = batch.count();
    unsigned chunks = (actual_batch_size + 999) / 1000;
    unsigned chunk_size = actual_batch_size / chunks;
    unsigned residue = actual_batch_size - chunks*chunk_size;

    TNSBitVector::enumerator en = batch.first();
    unsigned del_rec = 0;
    while (en.valid()) {
        unsigned txn_size = chunk_size;
        if (residue) {
            ++txn_size; --residue;
        }

        CNS_Transaction     trans(this);
        CQueueGuard         guard(this, &trans);

        unsigned n;
        for (n = 0; en.valid() && n < txn_size; ++en, ++n) {
            unsigned job_id = *en;
            m_QueueDbBlock->job_db.id = job_id;
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
        trans.Commit();
        // x_RemoveTags(trans, batch);
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
            m_QueueDbBlock->events_db.event = event_number;
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
    unsigned            subm_addr = job.GetSubmAddr();
    unsigned            aff_id = job.GetAffinityId();


    handler.WriteMessage("OK:", "id: " + NStr::IntToString(job.GetId()));
    handler.WriteMessage("OK:", "key: " + MakeKey(job.GetId()));
    handler.WriteMessage("OK:", "status: " +
                                CNetScheduleAPI::StatusToString(job.GetStatus()));

    NS_PRINT_TIME("time_submit: ", job.GetTimeSubmit());
    handler.WriteMessage("OK:", "timeout: " +
                                NStr::IntToString(job.GetTimeout()));
    handler.WriteMessage("OK:", "run_timeout: " +
                                NStr::IntToString(run_timeout));
    NS_PRINT_TIME("expiration_time: ",
                  job.GetJobExpirationTime(GetTimeout(), GetRunTimeout()));

    if (subm_addr == 0)
        handler.WriteMessage("OK:", "subm_addr: ");
    else
        handler.WriteMessage("OK:", "subm_addr: " +
                                    CSocketAPI::gethostbyaddr(subm_addr));

    handler.WriteMessage("OK:", "subm_port: " +
                                NStr::IntToString(job.GetSubmPort()));
    handler.WriteMessage("OK:", "subm_timeout: " +
                                NStr::IntToString(job.GetSubmTimeout()));

    // Print detailed information about the job events
    const vector<CJobEvent> &   events = job.GetEvents();
    int                         event = 1;

    handler.WriteMessage("OK:", "event_counter: " +
                                NStr::IntToString(events.size()) );
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

        // Port is not such a reliable worker node identification
        // so it is not printed anymore
        // message += ":";
        // if (port == 0)
        //     message += "unknown";
        // else
        //     message += NStr::IntToString(it->GetNodePort());
        // message += " ";

        message += "status=" +
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
        message += "nodeID=" + it->GetNodeId() + " "
                   "err_msg=" + it->GetQuotedErrorMsg();
        handler.WriteMessage("OK:", message);
    }

    handler.WriteMessage("OK:", "run_counter: " +
                                NStr::IntToString(job.GetRunCount()));
    handler.WriteMessage("OK:", "read_counter: " +
                                NStr::IntToString(job.GetReadCount()));

    if (aff_id)
        handler.WriteMessage("OK:", "aff_token: '" +
                                    job.GetAffinityToken() + "'");
    handler.WriteMessage("OK:", "aff_id: " + NStr::IntToString(aff_id));
    handler.WriteMessage("OK:", "mask: " + NStr::IntToString(job.GetMask()));
    handler.WriteMessage("OK:", "input: " + job.GetQuotedInput());
    handler.WriteMessage("OK:", "output: " + job.GetQuotedOutput());
    handler.WriteMessage("OK:", "progress_msg: '" + job.GetProgressMsg() + "'");

    const TNSTagList &   tags = job.GetTags();
    if (!tags.empty()) {
        string      encoded_tags;

        ITERATE(TNSTagList, it, tags) {
            if (!encoded_tags.empty())
                encoded_tags += "&";

            encoded_tags += NStr::URLEncode(it->first) + "=" +
                            NStr::URLEncode(it->second);
        }
        handler.WriteMessage("OK:", "tags: " + encoded_tags);
    }
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
        CQueueGuard             guard(this);
        CJob::EJobFetchResult   res = job.Fetch(this, job_id);

        if (res == CJob::eJF_Ok) {
            handler.WriteMessage("");
            job.FetchAffinityToken(this);
            x_PrintJobStat(handler, job, queue_run_timeout);
            ++print_count;
        }
    } else {
        TNSBitVector    bv;
        JobsWithStatus(status, &bv);

        TNSBitVector::enumerator en(bv.first());
        for (;en.valid(); ++en) {
            CQueueGuard             guard(this);
            CJob::EJobFetchResult   res = job.Fetch(this, *en);
            if (res == CJob::eJF_Ok) {
                handler.WriteMessage("");
                job.FetchAffinityToken(this);
                x_PrintJobStat(handler, job, queue_run_timeout);
                ++print_count;
            }
        }
    }
    return print_count;
}


void CQueue::PrintAllJobDbStat(CNetScheduleHandler &   handler)
{
    unsigned queue_run_timeout = GetRunTimeout();

    CJob                job;
    CQueueGuard         guard(this);
    CQueueEnumCursor    cur(this);

    while (cur.Fetch() == eBDB_Ok) {
        if (job.Fetch(this) == CJob::eJF_Ok) {
            handler.WriteMessage("");
            job.FetchAffinityToken(this);
            x_PrintJobStat(handler, job, queue_run_timeout);
        }
    }
}


void CQueue::PrintQueue(CNetScheduleHandler &   handler,
                        TJobStatus              job_status)
{
    TNSBitVector        bv;
    CJob                job;

    JobsWithStatus(job_status, &bv);
    for (TNSBitVector::enumerator en(bv.first()); en.valid(); ++en) {
        CQueueGuard     guard(this);

        if (job.Fetch(this, *en) == CJob::eJF_Ok)
            x_PrintShortJobStat(handler, job);
    }
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


unsigned CQueue::CountRecs()
{
    CQueueGuard guard(this);
    return m_QueueDbBlock->job_db.CountRecs();
}


void CQueue::PrintStat(CNcbiOstream& out)
{
    CQueueGuard guard(this);
    m_QueueDbBlock->job_db.PrintStat(out);
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
                            .Print("dblock_event_average",
                                   m_Container.m_Average[eStatDBLockEvent])
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


void CQueue::x_AddToReadGroupNoLock(unsigned group_id, unsigned job_id)
{
    TGroupMap::iterator i = m_GroupMap.find(group_id);
    if (i != m_GroupMap.end()) {
        TNSBitVector& bv = (*i).second;
        bv.set(job_id);
    } else {
        TNSBitVector bv;
        bv.set(job_id);
        m_GroupMap[group_id] = bv;
    }
}


void CQueue::x_CreateReadGroup(unsigned group_id, const TNSBitVector& bv_jobs)
{
    CFastMutexGuard guard(m_GroupMapLock);
    m_GroupMap[group_id] = bv_jobs;
}


void CQueue::x_RemoveFromReadGroup(unsigned group_id, unsigned job_id)
{
    CFastMutexGuard guard(m_GroupMapLock);
    TGroupMap::iterator i = m_GroupMap.find(group_id);
    if (i != m_GroupMap.end()) {
        TNSBitVector& bv = (*i).second;
        bv.set(job_id, false);
        if (!bv.any())
            m_GroupMap.erase(i);
    }
}


bool CQueue::x_UpdateDB_PutResultNoLock(unsigned             job_id,
                                        time_t               curr,
                                        bool                 delete_done,
                                        int                  ret_code,
                                        const string &       output,
                                        CJob &               job,
                                        unsigned int         peer_addr)
{
    if (job.Fetch(this, job_id) != CJob::eJF_Ok) {
        ERR_POST("Put results for non-existent job " << DecorateJobId(job_id));
        return false;
    }

    CJobEvent *     event = job.GetLastEvent();
    if (!event) {
        ERR_POST("Put results for a job without runs " << DecorateJobId(job_id));
        NCBI_THROW(CNetScheduleException, eInvalidJobStatus, "Job never ran");
    }

    if (delete_done) {
        job.Delete();
        return true;
    }

    // Append the event
    event = &job.AppendEvent();
    event->SetStatus(CNetScheduleAPI::eDone);
    event->SetTimestamp(curr);
    event->SetRetCode(ret_code);
    event->SetNodeAddr(peer_addr);

    job.SetStatus(CNetScheduleAPI::eDone);
    job.SetOutput(output);

    job.Flush(this);
    return true;
}


CQueue::EGetJobUpdateStatus CQueue::x_UpdateDB_GetJobNoLock(
                                            CWorkerNode*      worker_node,
                                            time_t            curr,
                                            unsigned          job_id,
                                            CJob&             job)
{
    unsigned        queue_timeout = GetTimeout();
    const unsigned  kMaxGetAttempts = 100;


    for (unsigned fetch_attempts = 0; fetch_attempts < kMaxGetAttempts;
        ++fetch_attempts)
    {
        CJob::EJobFetchResult res;
        if ((res = job.Fetch(this, job_id)) != CJob::eJF_Ok) {
            if (res == CJob::eJF_NotFound)
                return eGetJobUpdate_NotFound;
            // FIXME: does it make sense to retry right after DB error?
            continue;
        }
        TJobStatus status = job.GetStatus();

        // integrity check
        if (status != CNetScheduleAPI::ePending) {
            if (CJobStatusTracker::IsCancelCode(status)) {
                // this job has been canceled while I was fetching it
                return eGetJobUpdate_JobStopped;
            }
            ERR_POST("x_UpdateDB_GetJobNoLock: Status integrity violation, " <<
                     " job: " << DecorateJobId(job_id) <<
                     " status: " << status << " expected status: "
                     << (int)CNetScheduleAPI::ePending);
            return eGetJobUpdate_JobStopped;
        }

        time_t      time_submit = job.GetTimeSubmit();
        time_t      timeout = job.GetTimeout();

        if (timeout == 0)
            timeout = queue_timeout;

        _ASSERT(timeout);
        // check if job already expired
        if (timeout && (time_submit + timeout < curr)) {
            // Job expired, fail it
            CJobEvent &     event = job.AppendEvent();

            event.SetStatus(CNetScheduleAPI::eFailed);
            event.SetTimestamp(curr);
            event.SetErrorMsg("Job expired and cannot be scheduled.");
            job.SetStatus(CNetScheduleAPI::eFailed);
            m_StatusTracker.ChangeStatus(job_id, CNetScheduleAPI::eFailed);
            job.Flush(this);

            if (m_Log)
                LOG_POST(Error << "Job timeout expired, job: "
                               << DecorateJobId(job_id));

            return eGetJobUpdate_JobFailed;
        }

        // job not expired
        job.FetchAffinityToken(this);

        unsigned        run_count = job.GetRunCount() + 1;
        CJobEvent &     event = job.AppendEvent();

        event.SetStatus(CNetScheduleAPI::eRunning);
        event.SetTimestamp(curr);

        // We're setting host:port here using worker_node. It is faster
        // than looking up this info by node_id in worker node list and
        // should provide exactly same info.
        event.SetNodeId(worker_node->GetId());
        event.SetNodeAddr(worker_node->GetHost());
        event.SetNodePort(worker_node->GetPort());

        job.SetStatus(CNetScheduleAPI::eRunning);
        job.SetRunTimeout(0);
        job.SetRunCount(run_count);

        job.Flush(this);
        return eGetJobUpdate_Ok;
    }

    return eGetJobUpdate_NotFound;
}


END_NCBI_SCOPE

