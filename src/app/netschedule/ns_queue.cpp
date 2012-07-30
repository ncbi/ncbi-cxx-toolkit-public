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
CQueueEnumCursor::CQueueEnumCursor(CQueue *  queue, unsigned int  start_after)
  : CBDB_FileCursor(queue->m_QueueDbBlock->job_db)
{
    SetCondition(CBDB_FileCursor::eGT);
    From << start_after;
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
    m_RunTimeout(3600),
    m_RunTimeoutPrecision(-1),
    m_FailedRetries(0),
    m_BlacklistTime(0),
    m_EmptyLifetime(0),
    m_MaxInputSize(kNetScheduleMaxDBDataSize),
    m_MaxOutputSize(kNetScheduleMaxDBDataSize),
    m_DenyAccessViolations(false),
    m_WNodeTimeout(40),
    m_PendingTimeout(604800),
    m_KeyGenerator(server->GetHost(), server->GetPort()),
    m_Log(server->IsLog()),
    m_LogBatchEachJob(server->IsLogBatchEachJob()),
    m_RefuseSubmits(false),
    m_LastAffinityGC(0),
    m_MaxAffinities(server->GetMaxAffinities()),
    m_AffinityHighMarkPercentage(server->GetAffinityHighMarkPercentage()),
    m_AffinityLowMarkPercentage(server->GetAffinityLowMarkPercentage()),
    m_AffinityHighRemoval(server->GetAffinityHighRemoval()),
    m_AffinityLowRemoval(server->GetAffinityLowRemoval()),
    m_AffinityDirtPercentage(server->GetAffinityDirtPercentage()),
    m_NotificationsList(server->GetNodeID(), queue_name),
    m_NotifHifreqInterval(0.1),
    m_NotifHifreqPeriod(5),
    m_NotifLofreqMult(50),
    m_DumpBufferSize(100)
{
    _ASSERT(!queue_name.empty());
}


CQueue::~CQueue()
{
    delete m_RunTimeLine;
    Detach();
}


void CQueue::Attach(SQueueDbBlock* block)
{
    Detach();
    m_QueueDbBlock = block;
    m_AffinityRegistry.Attach(&m_QueueDbBlock->aff_dict_db);
    m_GroupRegistry.Attach(&m_QueueDbBlock->group_dict_db);

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
        LOG_POST(Message << Warning << "Clean up of db block "
                         << m_QueueDbBlock->pos << " complete, "
                         << sw.Elapsed() << " elapsed");
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

    m_Timeout    = params.timeout;
    m_RunTimeout = params.run_timeout;
    if (params.run_timeout && !m_RunTimeLine) {
        // One time only. Precision can not be reset.
        m_RunTimeLine =
            new CJobTimeLine(params.run_timeout_precision, 0);
        m_RunTimeoutPrecision = params.run_timeout_precision;
    }

    m_FailedRetries         = params.failed_retries;
    m_BlacklistTime         = params.blacklist_time;
    m_EmptyLifetime         = params.empty_lifetime;
    m_MaxInputSize          = params.max_input_size;
    m_MaxOutputSize         = params.max_output_size;
    m_DenyAccessViolations  = params.deny_access_violations;
    m_WNodeTimeout          = params.wnode_timeout;
    m_PendingTimeout        = params.pending_timeout;
    m_MaxPendingWaitTimeout = CNSPreciseTime(params.max_pending_wait_timeout);
    m_NotifHifreqInterval   = params.notif_hifreq_interval;
    m_NotifHifreqPeriod     = params.notif_hifreq_period;
    m_NotifLofreqMult       = params.notif_lofreq_mult;
    m_DumpBufferSize        = params.dump_buffer_size;

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


void CQueue::GetMaxIOSizes(unsigned int &  max_input_size,
                           unsigned int &  max_output_size) const
{
    CQueueParamAccessor     qp(*this);

    max_input_size = qp.GetMaxInputSize();
    max_output_size = qp.GetMaxOutputSize();
    return;
}


// Used while loading the status matrix.
// The access to the DB is not protected here.
time_t  CQueue::x_GetSubmitTime(unsigned int  job_id)
{
    CBDB_FileCursor     events_cur(m_QueueDbBlock->events_db);
    events_cur.SetCondition(CBDB_FileCursor::eEQ);
    events_cur.From << job_id;

    // The submit event is always first
    if (events_cur.FetchFirst() == eBDB_Ok)
        return m_QueueDbBlock->events_db.timestamp;
    return 0;
}


unsigned CQueue::LoadStatusMatrix()
{

    unsigned int        recs = 0;
    CFastMutexGuard     guard(m_OperationLock);

    // Load the known affinities and groups
    m_AffinityRegistry.LoadAffinityDictionary();
    m_GroupRegistry.LoadGroupDictionary();

    // scan the queue, load the state machine from DB
    CBDB_FileCursor     cur(m_QueueDbBlock->job_db);

    cur.InitMultiFetch(1024*1024);
    cur.SetCondition(CBDB_FileCursor::eGE);
    cur.From << 0;

    for (; cur.Fetch() == eBDB_Ok; ) {
        unsigned int    job_id = m_QueueDbBlock->job_db.id;
        unsigned int    group_id = m_QueueDbBlock->job_db.group_id;
        unsigned int    aff_id = m_QueueDbBlock->job_db.aff_id;
        time_t          last_touch = m_QueueDbBlock->job_db.last_touch;
        time_t          job_timeout = m_QueueDbBlock->job_db.timeout;
        time_t          job_run_timeout = m_QueueDbBlock->job_db.run_timeout;
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
        if (aff_id != 0)
            m_AffinityRegistry.AddJobToAffinity(job_id, aff_id);

        // Register the job in the group registry
        if (group_id != 0)
            m_GroupRegistry.AddJob(group_id, job_id);

        time_t      submit_time = 0;
        if (status == CNetScheduleAPI::ePending) {
            submit_time = x_GetSubmitTime(job_id);
            if (submit_time == 0)
                submit_time = time(0);  // Be on a safe side
        }

        // Register the loaded job with the garbage collector
        // Pending jobs will be processed later
        CNSPreciseTime      precise_submit(submit_time);
        m_GCRegistry.RegisterJob(job_id, precise_submit, aff_id, group_id,
                                 GetJobExpirationTime(last_touch, status,
                                                      submit_time,
                                                      job_timeout,
                                                      job_run_timeout,
                                                      m_Timeout,
                                                      m_RunTimeout,
                                                      m_PendingTimeout, 0));
        ++recs;
    }

    // Make sure that there are no affinity IDs in the registry for which there
    // are no jobs and initialize the next affinity ID counter.
    m_AffinityRegistry.FinalizeAffinityDictionaryLoading();

    // Make sure that there are no group IDs in the registry for which there
    // are no jobs and initialize the next group ID counter.
    m_GroupRegistry.FinalizeGroupDictionaryLoading();

    return recs;
}


// Used to log a single job
void CQueue::x_LogSubmit(const CJob &       job,
                         const string &     aff,
                         const string &     group)
{
    if (!m_Log)
        return;

    string                  group_token(group);

    if (group_token.empty())
        group_token = "n/a";

    CDiagContext_Extra  extra = GetDiagContext().Extra()
        .Print("job_key", MakeKey(job.GetId()))
        .Print("queue", GetQueueName())
        .Print("input", job.GetInput())
        .Print("aff", aff)
        .Print("group", group_token)
        .Print("mask", job.GetMask())
        .Print("subm_addr", CSocketAPI::gethostbyaddr(job.GetSubmAddr()))
        .Print("subm_notif_port", job.GetSubmNotifPort())
        .Print("subm_notif_timeout", job.GetSubmNotifTimeout())
        .Print("timeout", Uint8(job.GetTimeout()))
        .Print("run_timeout", job.GetRunTimeout())
        .Print("progress_msg", job.GetProgressMsg());

    extra.Flush();
    return;
}


unsigned int  CQueue::Submit(const CNSClientId &  client,
                             CJob &               job,
                             const string &       aff_token,
                             const string &       group)
{
    // the only config parameter used here is the max input size so there is no
    // need to have a safe parameters accessor.

    if (job.GetInput().size() > m_MaxInputSize)
        NCBI_THROW(CNetScheduleException, eDataTooLong, "Input is too long");

    unsigned int    aff_id = 0;
    unsigned int    group_id = 0;
    time_t          current_time = time(0);
    unsigned int    job_id = GetNextId();
    CJobEvent &     event = job.AppendEvent();

    job.SetId(job_id);
    job.SetPassport(rand());
    job.SetLastTouch(current_time);

    event.SetNodeAddr(client.GetAddress());
    event.SetStatus(CNetScheduleAPI::ePending);
    event.SetEvent(CJobEvent::eSubmit);
    event.SetTimestamp(current_time);
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
        CFastMutexGuard     guard(m_OperationLock);

        {{
            CNSTransaction      transaction(this);

            if (!group.empty()) {
                group_id = m_GroupRegistry.AddJob(group, job_id);
                job.SetGroupId(group_id);
            }
            if (!aff_token.empty()) {
                aff_id = m_AffinityRegistry.ResolveAffinityToken(aff_token,
                                                                 job_id, 0);
                job.SetAffinityId(aff_id);
            }
            job.Flush(this);

            transaction.Commit();
        }}

        m_StatusTracker.AddPendingJob(job_id);

        // Register the job with the client
        m_ClientsRegistry.AddToSubmitted(client, 1);

        // Make the decision whether to send or not a notification
        m_NotificationsList.Notify(job_id, aff_id,
                                   m_ClientsRegistry,
                                   m_AffinityRegistry,
                                   m_NotifHifreqPeriod);

        m_GCRegistry.RegisterJob(job_id, CNSPreciseTime::Current(),
                                 aff_id, group_id,
                                 job.GetExpirationTime(m_Timeout,
                                                       m_RunTimeout,
                                                       m_PendingTimeout,
                                                       0));
    }}

    m_StatisticsCounters.CountSubmit(1);
    x_LogSubmit(job, aff_token, group);
    return job_id;
}


unsigned int  CQueue::SubmitBatch(const CNSClientId &             client,
                                  vector< pair<CJob, string> > &  batch,
                                  const string &                  group)
{
    unsigned int    batch_size = batch.size();
    unsigned int    job_id = GetNextJobIdForBatch(batch_size);
    TNSBitVector    affinities;

    {{
        unsigned int        job_id_cnt = job_id;
        time_t              curr_time = time(0);
        unsigned int        group_id = 0;
        bool                no_aff_jobs = false;

        CFastMutexGuard     guard(m_OperationLock);

        {{
            CNSTransaction      transaction(this);

            // This might create a new record in the DB
            group_id = m_GroupRegistry.ResolveGroup(group);

            for (size_t  k = 0; k < batch_size; ++k) {

                CJob &              job = batch[k].first;
                const string &      aff_token = batch[k].second;
                CJobEvent &         event = job.AppendEvent();

                job.SetId(job_id_cnt);
                job.SetPassport(rand());
                job.SetGroupId(group_id);
                job.SetLastTouch(curr_time);

                event.SetNodeAddr(client.GetAddress());
                event.SetStatus(CNetScheduleAPI::ePending);
                event.SetEvent(CJobEvent::eBatchSubmit);
                event.SetTimestamp(curr_time);
                event.SetClientNode(client.GetNode());
                event.SetClientSession(client.GetSession());

                if (!aff_token.empty()) {
                    unsigned int    aff_id = m_AffinityRegistry.
                                                ResolveAffinityToken(aff_token,
                                                                     job_id_cnt,
                                                                     0);

                    job.SetAffinityId(aff_id);
                    affinities.set_bit(aff_id, true);
                }
                else
                    no_aff_jobs = true;

                job.Flush(this);
                ++job_id_cnt;
            }

            transaction.Commit();
        }}

        m_GroupRegistry.AddJobs(group_id, job_id, batch_size);
        m_StatusTracker.AddPendingBatch(job_id, job_id + batch_size - 1);
        m_ClientsRegistry.AddToSubmitted(client, batch_size);

        // Make a decision whether to notify clients or not
        TNSBitVector        jobs;
        jobs.set_range(job_id, job_id + batch_size - 1);
        m_NotificationsList.Notify(jobs, affinities, no_aff_jobs,
                                   m_ClientsRegistry,
                                   m_AffinityRegistry,
                                   m_NotifHifreqPeriod);

        CNSPreciseTime      current_precise = CNSPreciseTime::Current();
        for (size_t  k = 0; k < batch_size; ++k)
            m_GCRegistry.RegisterJob(batch[k].first.GetId(),
                                     current_precise,
                                     batch[k].first.GetAffinityId(),
                                     group_id,
                                     batch[k].first.GetExpirationTime(m_Timeout,
                                                                      m_RunTimeout,
                                                                      m_PendingTimeout,
                                                                      0));
    }}

    m_StatisticsCounters.CountSubmit(batch_size);
    if (m_LogBatchEachJob)
        for (size_t  k = 0; k < batch_size; ++k)
            x_LogSubmit(batch[k].first, batch[k].second, group);

    return job_id;
}



static const size_t     k_max_dead_locks = 10;  // max. dead lock repeats

TJobStatus  CQueue::PutResult(const CNSClientId &  client,
                              time_t               curr,
                              unsigned int         job_id,
                              const string &       auth_token,
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

            if (old_status == CNetScheduleAPI::eDone) {
                m_StatisticsCounters.CountTransition(CNetScheduleAPI::eDone,
                                                     CNetScheduleAPI::eDone);
                return old_status;
            }

            if (old_status != CNetScheduleAPI::ePending &&
                old_status != CNetScheduleAPI::eRunning)
                return old_status;

            {{
                CNSTransaction      transaction(this);
                x_UpdateDB_PutResultNoLock(job_id, auth_token, curr,
                                           ret_code, *output, job,
                                           client);
                transaction.Commit();
            }}

            m_StatusTracker.SetStatus(job_id, CNetScheduleAPI::eDone);
            m_StatisticsCounters.CountTransition(old_status,
                                                 CNetScheduleAPI::eDone);
            m_ClientsRegistry.ClearExecuting(job_id);

            m_GCRegistry.UpdateLifetime(job_id,
                                        job.GetExpirationTime(m_Timeout,
                                                              m_RunTimeout,
                                                              m_PendingTimeout,
                                                              0));

            TimeLineRemove(job_id);

            if (job.ShouldNotifySubmitter(curr))
                m_NotificationsList.NotifyJobStatus(job.GetSubmAddr(),
                                                    job.GetSubmNotifPort(),
                                                    MakeKey(job_id),
                                                    job.GetStatus());
            if (job.ShouldNotifyListener(curr))
                m_NotificationsList.NotifyJobStatus(job.GetListenerNotifAddr(),
                                                    job.GetListenerNotifPort(),
                                                    MakeKey(job_id),
                                                    job.GetStatus());
            return old_status;
        }
        catch (CBDB_ErrnoException &  ex) {
            if (ex.IsDeadLock()) {
                if (++dead_locks < k_max_dead_locks) {
                    ERR_POST("DeadLock repeat in CQueue::PutResult");
                    SleepMilliSec(100);
                    continue;
                }
            } else if (ex.IsNoMem()) {
                if (++dead_locks < k_max_dead_locks) {
                    ERR_POST("No resource repeat in CQueue::PutResult");
                    SleepMilliSec(100);
                    continue;
                }
            }

            if (ex.IsDeadLock() || ex.IsNoMem()) {
                string  message = "Too many transaction repeats in "
                                  "CQueue::PutResult";
                ERR_POST(message);
                NCBI_THROW(CNetScheduleException, eTryAgain, message);
            }

            throw;
        }
    }
}


bool  CQueue::GetJobOrWait(const CNSClientId &     client,
                           unsigned short          port, // Port the client
                                                         // will wait on
                           unsigned int            timeout, // If timeout != 0 => WGET
                           time_t                  curr,
                           const list<string> *    aff_list,
                           bool                    wnode_affinity,
                           bool                    any_affinity,
                           bool                    exclusive_new_affinity,
                           bool                    new_format,
                           CJob *                  new_job)
{
    // We need exactly 1 parameter - m_RunTimeout, so we can access it without
    // CQueueParamAccessor

    size_t      dead_locks = 0;                 // dead lock counter

    for (;;) {
        try {
            TNSBitVector        aff_ids;

            {{
                CFastMutexGuard     guard(m_OperationLock);

                if (wnode_affinity) {
                    // Check first that the were no preferred affinities reset for
                    // the client
                    if (m_ClientsRegistry.GetAffinityReset(client))
                        return false;   // Affinity was reset, so no job for the client
                }

                if (timeout != 0) {
                    // WGET:
                    // The affinities has to be resolved straight away, however at this
                    // point it is still unknown that the client will wait for them, so
                    // the client ID is passed as 0. Later on the client info will be
                    // updated in the affinity registry if the client really waits for
                    // this affinities.
                    CNSTransaction      transaction(this);
                    aff_ids = m_AffinityRegistry.ResolveAffinitiesForWaitClient(*aff_list, 0);
                    transaction.Commit();
                }
                else
                    // GET:
                    // No need to create aff records if they are not known
                    aff_ids = m_AffinityRegistry.GetAffinityIDs(*aff_list);

                x_UnregisterGetListener(client, port);
            }}

            for (;;) {
                // No lock here to make it possible to pick a job
                // simultaneously from many threads
                x_SJobPick  job_pick = x_FindPendingJob(client, aff_ids,
                                                        wnode_affinity,
                                                        any_affinity,
                                                        exclusive_new_affinity);
                {{
                    bool                outdated_job = false;
                    CFastMutexGuard     guard(m_OperationLock);

                    if (job_pick.job_id == 0) {
                        if (exclusive_new_affinity)
                            // Second try only if exclusive new aff is set on
                            job_pick = x_FindOutdatedPendingJob(client, 0);

                        if (job_pick.job_id == 0) {
                            if (timeout != 0) {
                                // WGET:
                                // There is no job, so the client might need to
                                // be registered
                                // in the waiting list
                                if (port > 0)
                                    x_RegisterGetListener(client, port, timeout, aff_ids,
                                                          wnode_affinity, any_affinity,
                                                          exclusive_new_affinity,
                                                          new_format);
                            }
                            return true;
                        }
                        outdated_job = true;
                    } else {
                        // Check that the job is still Pending; it could be
                        // grabbed by another WN or GC
                        if (GetJobStatus(job_pick.job_id) !=
                                CNetScheduleAPI::ePending)
                            continue;   // Try to pick a job again

                        if (exclusive_new_affinity) {
                            if (m_GCRegistry.IsOutdatedJob(job_pick.job_id,
                                                           m_MaxPendingWaitTimeout) == false) {
                                x_SJobPick  outdated_pick = x_FindOutdatedPendingJob(client,
                                                                                     job_pick.job_id);
                                if (outdated_pick.job_id != 0) {
                                    job_pick = outdated_pick;
                                    outdated_job = true;
                                }
                            }
                        }
                    }

                    // The job is still pending, check if it was received as
                    // with exclusive affinity
                    if (job_pick.exclusive && job_pick.aff_id != 0 && outdated_job == false) {
                        if (m_ClientsRegistry.IsPreferredByAny(job_pick.aff_id))
                            continue;   // Other WN grabbed this affinity already
                        m_ClientsRegistry.UpdatePreferredAffinities(client,
                                                                    job_pick.aff_id,
                                                                    0);
                    }
                    if (outdated_job && job_pick.aff_id != 0)
                        m_ClientsRegistry.UpdatePreferredAffinities(client,
                                                                    job_pick.aff_id,
                                                                    0);

                    if (x_UpdateDB_GetJobNoLock(client, curr,
                                                job_pick.job_id, *new_job)) {
                        // The job is not expired and successfully read
                        m_StatusTracker.SetStatus(job_pick.job_id, CNetScheduleAPI::eRunning);

                        m_StatisticsCounters.CountTransition(CNetScheduleAPI::ePending,
                                                             CNetScheduleAPI::eRunning);
                        if (outdated_job)
                            m_StatisticsCounters.CountOutdatedPick();

                        m_GCRegistry.UpdateLifetime(job_pick.job_id,
                                                    new_job->GetExpirationTime(m_Timeout,
                                                                               m_RunTimeout,
                                                                               m_PendingTimeout,
                                                                               0));
                        TimeLineAdd(job_pick.job_id, curr + m_RunTimeout);
                        m_ClientsRegistry.AddToRunning(client, job_pick.job_id);

                        if (new_job->ShouldNotifyListener(curr))
                            m_NotificationsList.NotifyJobStatus(new_job->GetListenerNotifAddr(),
                                                                new_job->GetListenerNotifPort(),
                                                                MakeKey(new_job->GetId()),
                                                                new_job->GetStatus());
                        return true;
                    }

                    // Reset job_id and try again
                    new_job->SetId(0);
                }}
            }
        }
        catch (CBDB_ErrnoException &  ex) {
            if (ex.IsDeadLock()) {
                if (++dead_locks < k_max_dead_locks) {
                    ERR_POST("DeadLock repeat in CQueue::GetJobOrWait");
                    SleepMilliSec(100);
                    continue;
                }
            } else if (ex.IsNoMem()) {
                if (++dead_locks < k_max_dead_locks) {
                    ERR_POST("No resource repeat in CQueue::GetJobOrWait");
                    SleepMilliSec(100);
                    continue;
                }
            }

            if (ex.IsDeadLock() || ex.IsNoMem()) {
                string  message = "Too many transaction repeats in "
                                  "CQueue::GetJobOrWait";
                ERR_POST(message);
                NCBI_THROW(CNetScheduleException, eTryAgain, message);
            }

            throw;
        }
    }
    return true;
}


void  CQueue::CancelWaitGet(const CNSClientId &  client)
{
    bool    result;

    {{
        CFastMutexGuard     guard(m_OperationLock);

        result = x_UnregisterGetListener(client, 0);
    }}

    if (result == false)
        LOG_POST(Message << Warning << "Attempt to cancel WGET for the client "
                                       "which does not wait anything (node: "
                         << client.GetNode() << " session: "
                         << client.GetSession() << ")");
    return;
}


string  CQueue::ChangeAffinity(const CNSClientId &     client,
                               const list<string> &    aff_to_add,
                               const list<string> &    aff_to_del)
{
    // It is guaranteed here that the client is a new style one.
    // I.e. it has both client_node and client_session.

    string          msg;    // Warning message for the socket
    unsigned int    client_id = client.GetID();
    TNSBitVector    current_affinities =
                         m_ClientsRegistry.GetPreferredAffinities(client);
    TNSBitVector    aff_id_to_add;
    TNSBitVector    aff_id_to_del;
    bool            any_to_add = false;
    bool            any_to_del = false;

    // Identify the affinities which should be deleted
    for (list<string>::const_iterator  k(aff_to_del.begin());
         k != aff_to_del.end(); ++k) {
        unsigned int    aff_id = m_AffinityRegistry.GetIDByToken(*k);

        if (aff_id == 0) {
            // The affinity is not known for NS at all
            LOG_POST(Message << Warning << "Client '" << client.GetNode()
                             << "' deletes unknown affinity '"
                             << *k << "'. Ignored.");
            if (!msg.empty())
                msg += "; ";
            msg += "unknown affinity to delete: " + *k;
            continue;
        }

        if (current_affinities[aff_id] == false) {
            // This a try to delete something which has not been added or
            // deleted before.
            LOG_POST(Message << Warning << "Client '" << client.GetNode()
                             << "' deletes affinity '" << *k
                             << "' which is not in the list of the "
                                "preferred client affinities. Ignored.");
            if (!msg.empty())
                msg += "; ";
            msg += "not registered affinity to delete: " + *k;
            continue;
        }

        // The affinity will really be deleted
        aff_id_to_del.set(aff_id, true);
        any_to_del = true;
    }


    // Check that the update of the affinities list will not exceed the limit
    // for the max number of affinities per client.
    // Note: this is not precise check. There could be non-unique affinities in
    // the add list or some of affinities to add could already be in the list.
    // The precise checking however requires more CPU and blocking so only an
    // approximate (but fast) checking is done.
    if (current_affinities.count() + aff_to_add.size()
                                   - aff_id_to_del.count() >
                                         m_MaxAffinities) {
        NCBI_THROW(CNetScheduleException, eTooManyPreferredAffinities,
                   "The client '" + client.GetNode() +
                   "' exceeds the limit (" +
                   NStr::UIntToString(m_MaxAffinities) +
                   ") of the preferred affinities. Changed request ignored.");
    }

    // To avoid logging under the lock
    vector<string>  already_added_affinities;

    {{

        CFastMutexGuard     guard(m_OperationLock);
        CNSTransaction      transaction(this);

        // Convert the aff_to_add to the affinity IDs
        for (list<string>::const_iterator  k(aff_to_add.begin());
             k != aff_to_add.end(); ++k ) {
            unsigned int    aff_id =
                                 m_AffinityRegistry.ResolveAffinityToken(*k,
                                                                 0, client_id);

            if (current_affinities[aff_id] == true) {
                already_added_affinities.push_back(*k);
                continue;
            }

            aff_id_to_add.set(aff_id, true);
            any_to_add = true;
        }

        transaction.Commit();
    }}

    // Log the warnings and add it to the warning message
    for (vector<string>::const_iterator  j(already_added_affinities.begin());
         j != already_added_affinities.end(); ++j) {
        // That was a try to add something which has already been added
        LOG_POST(Message << Warning << "Client '" << client.GetNode()
                         << "' adds affinity '" << *j
                         << "' which is already in the list of the "
                            "preferred client affinities. Ignored.");
        if (!msg.empty())
            msg += "; ";
        msg += "already registered affinity to add: " + *j;
    }

    if (any_to_del)
        m_AffinityRegistry.RemoveClientFromAffinities(client_id,
                                                      aff_id_to_del);

    if (any_to_add || any_to_del)
        m_ClientsRegistry.UpdatePreferredAffinities(client,
                                                    aff_id_to_add,
                                                    aff_id_to_del);
    return msg;
}


TJobStatus  CQueue::JobDelayExpiration(unsigned int     job_id,
                                       time_t           tm)
{
    CJob                job;
    unsigned            queue_run_timeout = GetRunTimeout();
    time_t              curr = time(0);

    {{
        CFastMutexGuard     guard(m_OperationLock);
        TJobStatus          status = GetJobStatus(job_id);

        if (status != CNetScheduleAPI::eRunning)
            return status;

        time_t          time_start = 0;
        time_t          run_timeout = 0;
        {{
            CNSTransaction      transaction(this);

            if (job.Fetch(this, job_id) != CJob::eJF_Ok)
                return CNetScheduleAPI::eJobNotFound;

            time_start = job.GetLastEvent()->GetTimestamp();
            run_timeout = job.GetRunTimeout();
            if (run_timeout == 0)
                run_timeout = queue_run_timeout;

            if (time_start + run_timeout > curr + tm)
                return CNetScheduleAPI::eRunning;   // Old timeout is enough to
                                                    // cover this request, so
                                                    // keep it.

            job.SetRunTimeout(curr + tm - time_start);
            job.SetLastTouch(curr);
            job.Flush(this);
            transaction.Commit();
        }}

        // No need to update the GC registry because the running (and reading)
        // jobs are skipped by GC

        time_t  exp_time = run_timeout == 0 ? 0 : time_start + run_timeout;

        TimeLineMove(job_id, exp_time, curr + tm);
    }}

    return CNetScheduleAPI::eRunning;
}


TJobStatus  CQueue::GetStatusAndLifetime(unsigned int  job_id,
                                         bool          need_touch,
                                         time_t *      lifetime)
{
    CFastMutexGuard     guard(m_OperationLock);
    TJobStatus          status = GetJobStatus(job_id);

    if (status == CNetScheduleAPI::eJobNotFound)
        return status;

    if (need_touch) {
        CJob                job;
        time_t              curr = time(0);

        {{
            CNSTransaction      transaction(this);

            if (job.Fetch(this, job_id) != CJob::eJF_Ok)
                NCBI_THROW(CNetScheduleException, eInternalError,
                           "Error fetching job: " + DecorateJobId(job_id));

            job.SetLastTouch(curr);
            job.Flush(this);
            transaction.Commit();
        }}

        m_GCRegistry.UpdateLifetime(job_id,
                                    job.GetExpirationTime(m_Timeout,
                                                          m_RunTimeout,
                                                          m_PendingTimeout,
                                                          curr));
    }

    *lifetime = x_GetEstimatedJobLifetime(job_id, status);
    return status;
}


TJobStatus  CQueue::SetJobListener(unsigned int     job_id,
                                   unsigned int     address,
                                   unsigned short   port,
                                   time_t           timeout)
{
    CJob                job;
    time_t              curr = time(0);
    TJobStatus          status = CNetScheduleAPI::eJobNotFound;

    {{
        CFastMutexGuard     guard(m_OperationLock);
        CNSTransaction      transaction(this);

        if (job.Fetch(this, job_id) != CJob::eJF_Ok)
            return status;

        status = job.GetStatus();
        if (status == CNetScheduleAPI::eCanceled)
            return status;  // Listener notifications are valid for all states
                            // except of Cancel - there are no transitions from
                            // Cancel.

        job.SetListenerNotifAddr(address);
        job.SetListenerNotifPort(port);
        job.SetListenerNotifAbsTime(curr + timeout);

        job.Flush(this);
        transaction.Commit();
    }}

    return status;
}


bool CQueue::PutProgressMessage(unsigned int    job_id,
                                const string &  msg)
{
    CJob                job;
    time_t              curr = time(0);

    {{
        CFastMutexGuard     guard(m_OperationLock);

        {{
            CNSTransaction      transaction(this);

            if (job.Fetch(this, job_id) != CJob::eJF_Ok)
                return false;

            job.SetProgressMsg(msg);
            job.SetLastTouch(curr);
            job.Flush(this);
            transaction.Commit();
        }}

        m_GCRegistry.UpdateLifetime(job_id,
                                    job.GetExpirationTime(m_Timeout,
                                                          m_RunTimeout,
                                                          m_PendingTimeout,
                                                          curr));
    }}

    return true;
}


TJobStatus  CQueue::ReturnJob(const CNSClientId &     client,
                              unsigned int            job_id,
                              const string &          auth_token,
                              string &                warning)
{
    CJob                job;
    time_t              current_time = time(0);
    CFastMutexGuard     guard(m_OperationLock);
    TJobStatus          old_status = GetJobStatus(job_id);

    if (old_status != CNetScheduleAPI::eRunning)
        return old_status;

    {{
         CNSTransaction      transaction(this);

        if (job.Fetch(this, job_id) != CJob::eJF_Ok)
            NCBI_THROW(CNetScheduleException, eInternalError,
                       "Error fetching job: " + DecorateJobId(job_id));
        if (job.GetStatus() != CNetScheduleAPI::eRunning)
            NCBI_THROW(CNetScheduleException, eInvalidJobStatus,
                       "Operation is applicable to eRunning job state only");

        if (!auth_token.empty()) {
            // Need to check authorization token first
            CJob::EAuthTokenCompareResult   token_compare_result =
                                            job.CompareAuthToken(auth_token);
            if (token_compare_result == CJob::eInvalidTokenFormat)
                NCBI_THROW(CNetScheduleException, eInvalidAuthToken,
                           "Invalid authorization token format");
            if (token_compare_result == CJob::eNoMatch)
                NCBI_THROW(CNetScheduleException, eInvalidAuthToken,
                           "Authorization token does not match");
            if (token_compare_result == CJob::ePassportOnlyMatch) {
                // That means the job has been given to another worker node
                // by whatever reason (expired/failed/returned before)
                LOG_POST(Message << Warning << "Received RETURN2 with only "
                                               "passport matched.");
                warning = "Only job passport matched. Command is ignored.";
                return old_status;
            }
            // Here: the authorization token is OK, we can continue
        }


        unsigned int    run_count = job.GetRunCount();
        CJobEvent *     event = job.GetLastEvent();

        if (!event)
            ERR_POST("No JobEvent for running job " << DecorateJobId(job_id));

        event = &job.AppendEvent();
        event->SetNodeAddr(client.GetAddress());
        event->SetStatus(CNetScheduleAPI::ePending);
        event->SetEvent(CJobEvent::eReturn);
        event->SetTimestamp(current_time);
        event->SetClientNode(client.GetNode());
        event->SetClientSession(client.GetSession());

        if (run_count)
            job.SetRunCount(run_count-1);

        job.SetStatus(CNetScheduleAPI::ePending);
        job.SetLastTouch(current_time);
        job.Flush(this);

        transaction.Commit();
    }}

    m_StatusTracker.SetStatus(job_id, CNetScheduleAPI::ePending);
    m_StatisticsCounters.CountTransition(old_status,
                                         CNetScheduleAPI::ePending);
    TimeLineRemove(job_id);
    m_ClientsRegistry.ClearExecuting(job_id);
    m_ClientsRegistry.AddToBlacklist(client, job_id);
    m_GCRegistry.UpdateLifetime(job_id, job.GetExpirationTime(m_Timeout,
                                                              m_RunTimeout,
                                                              m_PendingTimeout,
                                                              0));

    if (job.ShouldNotifyListener(current_time))
        m_NotificationsList.NotifyJobStatus(job.GetListenerNotifAddr(),
                                            job.GetListenerNotifPort(),
                                            MakeKey(job_id),
                                            job.GetStatus());

    m_NotificationsList.Notify(job_id,
                               job.GetAffinityId(), m_ClientsRegistry,
                               m_AffinityRegistry, m_NotifHifreqPeriod);
    return old_status;
}


TJobStatus  CQueue::ReadAndTouchJob(unsigned int  job_id,
                                    CJob &        job,
                                    time_t *      lifetime)
{
    CFastMutexGuard         guard(m_OperationLock);
    TJobStatus              status = GetJobStatus(job_id);

    if (status == CNetScheduleAPI::eJobNotFound)
        return status;

    time_t                  curr = time(0);
    {{
        CNSTransaction      transaction(this);

        if (job.Fetch(this, job_id) != CJob::eJF_Ok)
            NCBI_THROW(CNetScheduleException, eInternalError,
                       "Error fetching job: " + DecorateJobId(job_id));

        job.SetLastTouch(curr);
        job.Flush(this);
        transaction.Commit();
    }}

    m_GCRegistry.UpdateLifetime(job_id,
                                job.GetExpirationTime(m_Timeout,
                                                      m_RunTimeout,
                                                      m_PendingTimeout,
                                                      curr));
    *lifetime = x_GetEstimatedJobLifetime(job_id, status);
    return status;
}


// Deletes all the jobs from the queue
void CQueue::Truncate(void)
{
    CJob                                job;
    TNSBitVector                        bv;
    vector<CNetScheduleAPI::EJobStatus> statuses;
    TNSBitVector                        jobs_to_notify;
    time_t                              current_time = time(0);

    // Pending and running jobs should be notified if requested
    statuses.push_back(CNetScheduleAPI::ePending);
    statuses.push_back(CNetScheduleAPI::eRunning);

    {{
        CFastMutexGuard     guard(m_OperationLock);

        jobs_to_notify = m_StatusTracker.GetJobs(statuses);

        // Make a decision if the jobs should really be notified
        {{
            CNSTransaction              transaction(this);
            TNSBitVector::enumerator    en(jobs_to_notify.first());
            for (; en.valid(); ++en) {
                unsigned int    job_id = *en;

                if (job.Fetch(this, job_id) != CJob::eJF_Ok) {
                    ERR_POST("Cannot fetch job " << MakeKey(job_id) <<
                             " while dropping all jobs in the queue");
                    continue;
                }

                if (job.ShouldNotifySubmitter(current_time))
                    m_NotificationsList.NotifyJobStatus(job.GetSubmAddr(),
                                                        job.GetSubmNotifPort(),
                                                        MakeKey(job_id),
                                                        job.GetStatus());
            }

            // There is no need to commit transaction - there were no changes
        }}

        m_StatusTracker.ClearAll(&bv);
        m_AffinityRegistry.ClearMemoryAndDatabase();
        m_GroupRegistry.ClearMemoryAndDatabase();

        CWriteLockGuard     rtl_guard(m_RunTimeLineLock);
        m_RunTimeLine->ReInit(0);
    }}

    x_Erase(bv);

    // Next call updates 'm_BecameEmpty' timestamp
    IsExpired(); // locks CQueue lock
}


TJobStatus  CQueue::Cancel(const CNSClientId &  client,
                           unsigned int         job_id)
{
    TJobStatus          old_status;
    CJob                job;
    time_t              current_time = time(0);

    {{
        CFastMutexGuard     guard(m_OperationLock);

        old_status = m_StatusTracker.GetStatus(job_id);
        if (old_status == CNetScheduleAPI::eJobNotFound)
            return CNetScheduleAPI::eJobNotFound;
        if (old_status == CNetScheduleAPI::eCanceled) {
            m_StatisticsCounters.CountTransition(
                                        CNetScheduleAPI::eCanceled,
                                        CNetScheduleAPI::eCanceled);
            return CNetScheduleAPI::eCanceled;
        }

        {{
            CNSTransaction      transaction(this);
            if (job.Fetch(this, job_id) != CJob::eJF_Ok)
                return CNetScheduleAPI::eJobNotFound;

            CJobEvent *     event = &job.AppendEvent();

            event->SetNodeAddr(client.GetAddress());
            event->SetStatus(CNetScheduleAPI::eCanceled);
            event->SetEvent(CJobEvent::eCancel);
            event->SetTimestamp(current_time);
            event->SetClientNode(client.GetNode());
            event->SetClientSession(client.GetSession());

            job.SetStatus(CNetScheduleAPI::eCanceled);
            job.SetLastTouch(current_time);
            job.Flush(this);

            transaction.Commit();
        }}

        m_StatusTracker.SetStatus(job_id, CNetScheduleAPI::eCanceled);
        m_StatisticsCounters.CountTransition(old_status,
                                             CNetScheduleAPI::eCanceled);

        TimeLineRemove(job_id);
        if (old_status == CNetScheduleAPI::eRunning)
            m_ClientsRegistry.ClearExecuting(job_id);
        else if (old_status == CNetScheduleAPI::eReading)
            m_ClientsRegistry.ClearReading(job_id);

        m_GCRegistry.UpdateLifetime(job_id,
                                    job.GetExpirationTime(m_Timeout,
                                                          m_RunTimeout,
                                                          m_PendingTimeout,
                                                          0));

    }}

    if ((old_status == CNetScheduleAPI::eRunning ||
         old_status == CNetScheduleAPI::ePending) &&
         job.ShouldNotifySubmitter(current_time))
        m_NotificationsList.NotifyJobStatus(job.GetSubmAddr(),
                                            job.GetSubmNotifPort(),
                                            MakeKey(job_id),
                                            job.GetStatus());

    if (job.ShouldNotifyListener(current_time))
        m_NotificationsList.NotifyJobStatus(job.GetListenerNotifAddr(),
                                            job.GetListenerNotifPort(),
                                            MakeKey(job_id),
                                            job.GetStatus());

    return old_status;
}


void  CQueue::CancelAllJobs(const CNSClientId &  client)
{
    vector<CNetScheduleAPI::EJobStatus> statuses;

    // All except cancelled
    statuses.push_back(CNetScheduleAPI::ePending);
    statuses.push_back(CNetScheduleAPI::eRunning);
    statuses.push_back(CNetScheduleAPI::eFailed);
    statuses.push_back(CNetScheduleAPI::eDone);
    statuses.push_back(CNetScheduleAPI::eReading);
    statuses.push_back(CNetScheduleAPI::eConfirmed);
    statuses.push_back(CNetScheduleAPI::eReadFailed);

    CFastMutexGuard     guard(m_OperationLock);
    x_CancelJobs(client, m_StatusTracker.GetJobs(statuses));
    return;
}


void CQueue::x_CancelJobs(const CNSClientId &   client,
                          const TNSBitVector &  jobs_to_cancel)
{
    CJob                        job;
    time_t                      current_time = time(0);
    TNSBitVector::enumerator    en(jobs_to_cancel.first());
    for (; en.valid(); ++en) {
        unsigned int    job_id = *en;
        TJobStatus      old_status = m_StatusTracker.GetStatus(job_id);

        {{
            CNSTransaction              transaction(this);
            if (job.Fetch(this, job_id) != CJob::eJF_Ok) {
                ERR_POST("Cannot fetch job " << MakeKey(job_id) <<
                         " while cancelling jobs");
                continue;
            }

            CJobEvent *     event = &job.AppendEvent();

            event->SetNodeAddr(client.GetAddress());
            event->SetStatus(CNetScheduleAPI::eCanceled);
            event->SetEvent(CJobEvent::eCancel);
            event->SetTimestamp(current_time);
            event->SetClientNode(client.GetNode());
            event->SetClientSession(client.GetSession());

            job.SetStatus(CNetScheduleAPI::eCanceled);
            job.SetLastTouch(current_time);
            job.Flush(this);

            transaction.Commit();
        }}

        m_StatusTracker.SetStatus(job_id, CNetScheduleAPI::eCanceled);
        m_StatisticsCounters.CountTransition(old_status,
                                             CNetScheduleAPI::eCanceled);

        TimeLineRemove(job_id);
        if (old_status == CNetScheduleAPI::eRunning)
            m_ClientsRegistry.ClearExecuting(job_id);
        else if (old_status == CNetScheduleAPI::eReading)
            m_ClientsRegistry.ClearReading(job_id);

        m_GCRegistry.UpdateLifetime(job_id,
                                    job.GetExpirationTime(m_Timeout,
                                                          m_RunTimeout,
                                                          m_PendingTimeout,
                                                          0));
        if ((old_status == CNetScheduleAPI::eRunning ||
             old_status == CNetScheduleAPI::ePending) &&
             job.ShouldNotifySubmitter(current_time))
            m_NotificationsList.NotifyJobStatus(job.GetSubmAddr(),
                                                job.GetSubmNotifPort(),
                                                MakeKey(job_id),
                                                job.GetStatus());
        if (job.ShouldNotifyListener(current_time))
            m_NotificationsList.NotifyJobStatus(job.GetListenerNotifAddr(),
                                                job.GetListenerNotifPort(),
                                                MakeKey(job_id),
                                                job.GetStatus());
    }

    return;
}


// This function must be called under the operations lock.
// If called for not existing job then an exception is generated
time_t
CQueue::x_GetEstimatedJobLifetime(unsigned int   job_id,
                                  TJobStatus     status) const
{
    if (status == CNetScheduleAPI::eRunning ||
        status == CNetScheduleAPI::eReading)
        return time(0) + GetTimeout();
    return m_GCRegistry.GetLifetime(job_id);
}


void  CQueue::CancelGroup(const CNSClientId &  client,
                          const string &       group)
{
    CFastMutexGuard     guard(m_OperationLock);
    x_CancelJobs(client, m_GroupRegistry.GetJobs(group));
    return;
}


TJobStatus  CQueue::GetJobStatus(unsigned int  job_id) const
{
    return m_StatusTracker.GetStatus(job_id);
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
                LOG_POST(Message << Warning << "Queue " << m_QueueName
                                 << " expired. Became empty: "
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
    CFastMutexGuard     guard(m_LastIdLock);

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


// Reserves the given number of the job IDs
unsigned int CQueue::GetNextJobIdForBatch(unsigned int  count)
{
    CFastMutexGuard     guard(m_LastIdLock);

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
                              const string &        group,
                              CJob *                job)
{
    time_t                                  curr(time(0));
    vector<CNetScheduleAPI::EJobStatus>     from_state;

    from_state.push_back(CNetScheduleAPI::eDone);
    from_state.push_back(CNetScheduleAPI::eFailed);


    {{
        CFastMutexGuard     guard(m_OperationLock);
        TNSBitVector        candidates = m_StatusTracker.GetJobs(from_state);

        // Exclude blacklisted jobs
        candidates -= m_ClientsRegistry.GetBlacklistedJobs(client);

        if (!group.empty())
            // Apply restrictions on the group jobs if so
            candidates &= m_GroupRegistry.GetJobs(group);

        if (!candidates.any())
            return;

        unsigned int        job_id = *candidates.first();
        TJobStatus          old_status = GetJobStatus(job_id);
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
            job->SetLastTouch(curr);
            job->Flush(this);

            transaction.Commit();
        }}

        if (read_timeout == 0)
            read_timeout = m_RunTimeout;

        if (read_timeout != 0)
            TimeLineAdd(job_id, curr + read_timeout);

        // Update the memory cache
        m_StatusTracker.SetStatus(job_id, CNetScheduleAPI::eReading);
        m_StatisticsCounters.CountTransition(old_status,
                                              CNetScheduleAPI::eReading);
        m_ClientsRegistry.AddToReading(client, job_id);

        m_GCRegistry.UpdateLifetime(job_id, job->GetExpirationTime(m_Timeout,
                                                                   m_RunTimeout,
                                                                   m_PendingTimeout,
                                                                   0));
    }}

    if (job->ShouldNotifyListener(curr))
        m_NotificationsList.NotifyJobStatus(job->GetListenerNotifAddr(),
                                            job->GetListenerNotifPort(),
                                            MakeKey(job->GetId()),
                                            job->GetStatus());
    return;
}


TJobStatus  CQueue::ConfirmReadingJob(const CNSClientId &  client,
                                      unsigned int         job_id,
                                      const string &       auth_token)
{
    TJobStatus      old_status = x_ChangeReadingStatus(
                                                client, job_id,
                                                auth_token,
                                                CNetScheduleAPI::eConfirmed);
    m_ClientsRegistry.ClearReading(client, job_id);
    return old_status;
}


TJobStatus  CQueue::FailReadingJob(const CNSClientId &  client,
                                   unsigned int         job_id,
                                   const string &       auth_token)
{
    TJobStatus      old_status = x_ChangeReadingStatus(
                                                client, job_id,
                                                auth_token,
                                                CNetScheduleAPI::eReadFailed);
    m_ClientsRegistry.ClearReadingSetBlacklist(job_id);
    return old_status;
}


TJobStatus  CQueue::ReturnReadingJob(const CNSClientId &  client,
                                     unsigned int         job_id,
                                     const string &       auth_token)
{
    TJobStatus      old_status = x_ChangeReadingStatus(
                                                client, job_id,
                                                auth_token,
                                                CNetScheduleAPI::eDone);
    m_ClientsRegistry.ClearReadingSetBlacklist(job_id);
    return old_status;
}


TJobStatus  CQueue::x_ChangeReadingStatus(const CNSClientId &  client,
                                          unsigned int         job_id,
                                          const string &       auth_token,
                                          TJobStatus           target_status)
{
    time_t                                      current_time = time(0);
    CJob                                        job;
    TJobStatus                                  new_status = target_status;
    CStatisticsCounters::ETransitionPathOption  path_option =
                                                    CStatisticsCounters::eNone;
    CFastMutexGuard                             guard(m_OperationLock);
    TJobStatus                                  old_status = GetJobStatus(job_id);

    if (old_status != CNetScheduleAPI::eReading)
        return old_status;

    {{
        CNSTransaction      transaction(this);

        if (job.Fetch(this, job_id) != CJob::eJF_Ok)
            NCBI_THROW(CNetScheduleException, eInternalError,
                       "Error fetching job: " + DecorateJobId(job_id));

        // Check that authorization token matches
        CJob::EAuthTokenCompareResult   token_compare_result =
                                            job.CompareAuthToken(auth_token);
        if (token_compare_result == CJob::eInvalidTokenFormat)
            NCBI_THROW(CNetScheduleException, eInvalidAuthToken,
                       "Invalid authorization token format");
        if (token_compare_result == CJob::eNoMatch)
            NCBI_THROW(CNetScheduleException, eInvalidAuthToken,
                       "Authorization token does not match");

        // Sanity check of the current job state
        if (job.GetStatus() != CNetScheduleAPI::eReading)
            NCBI_THROW(CNetScheduleException, eInternalError,
                "Internal inconsistency detected. The job " +
                DecorateJobId(job_id) + " state in memory is " +
                CNetScheduleAPI::StatusToString(CNetScheduleAPI::eReading) +
                " while in database it is " +
                CNetScheduleAPI::StatusToString(job.GetStatus()));

        // Add an event
        CJobEvent &     event = job.AppendEvent();
        event.SetTimestamp(current_time);
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
                if (job.GetReadCount() <= m_FailedRetries) {
                    // The job needs to be re-scheduled for reading
                    new_status = CNetScheduleAPI::eDone;
                    path_option = CStatisticsCounters::eFail;
                }
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
        job.SetLastTouch(current_time);

        job.Flush(this);
        transaction.Commit();
    }}

    TimeLineRemove(job_id);

    m_StatusTracker.SetStatus(job_id, new_status);
    m_GCRegistry.UpdateLifetime(job_id, job.GetExpirationTime(m_Timeout,
                                                              m_RunTimeout,
                                                              m_PendingTimeout,
                                                              0));
    m_StatisticsCounters.CountTransition(CNetScheduleAPI::eReading,
                                         new_status,
                                         path_option);
    if (job.ShouldNotifyListener(current_time))
        m_NotificationsList.NotifyJobStatus(job.GetListenerNotifAddr(),
                                            job.GetListenerNotifPort(),
                                            MakeKey(job_id),
                                            job.GetStatus());
    return CNetScheduleAPI::eReading;
}


// This function is called from places where the operations lock has been
// already taken. So there is no lock around memory status tracker
void CQueue::EraseJob(unsigned int  job_id)
{
    m_StatusTracker.Erase(job_id);

    {{
        // Request delayed record delete
        CFastMutexGuard     jtd_guard(m_JobsToDeleteLock);

        m_JobsToDelete.set_bit(job_id);
    }}
    TimeLineRemove(job_id);
}


void CQueue::x_Erase(const TNSBitVector &  job_ids)
{
    CFastMutexGuard     jtd_guard(m_JobsToDeleteLock);

    m_JobsToDelete |= job_ids;
}


void CQueue::OptimizeMem()
{
    m_StatusTracker.OptimizeMem();
}


CQueue::x_SJobPick
CQueue::x_FindPendingJob(const CNSClientId  &  client,
                         const TNSBitVector &  aff_ids,
                         bool                  wnode_affinity,
                         bool                  any_affinity,
                         bool                  exclusive_new_affinity)
{
    x_SJobPick      job_pick = { 0, false, 0 };
    bool            explicit_aff = aff_ids.any();
    unsigned int    wnode_aff_candidate = 0;
    unsigned int    exclusive_aff_candidate = 0;
    TNSBitVector    blacklisted_jobs =
                    m_ClientsRegistry.GetBlacklistedJobs(client);

    TNSBitVector    pref_aff;
    if (wnode_affinity) {
        pref_aff = m_ClientsRegistry.GetPreferredAffinities(client);
        wnode_affinity = wnode_affinity && pref_aff.any();
    }

    TNSBitVector    all_pref_aff;
    if (exclusive_new_affinity)
        all_pref_aff = m_ClientsRegistry.GetAllPreferredAffinities();

    if (explicit_aff || wnode_affinity || exclusive_new_affinity) {
        // Check all pending jobs
        TNSBitVector                pending_jobs =
                            m_StatusTracker.GetJobs(CNetScheduleAPI::ePending);
        TNSBitVector::enumerator    en(pending_jobs.first());

        for (; en.valid(); ++en) {
            unsigned int    job_id = *en;

            if (blacklisted_jobs[job_id] == true)
                continue;

            unsigned int    aff_id = m_GCRegistry.GetAffinityID(job_id);
            if (aff_id != 0 && explicit_aff) {
                if (aff_ids[aff_id] == true) {
                    job_pick.job_id = job_id;
                    job_pick.exclusive = false;
                    job_pick.aff_id = aff_id;
                    return job_pick;
                }
            }

            if (aff_id != 0 && wnode_affinity) {
                if (pref_aff[aff_id] == true) {
                    if (explicit_aff == false) {
                        job_pick.job_id = job_id;
                        job_pick.exclusive = false;
                        job_pick.aff_id = aff_id;
                        return job_pick;
                    }
                    if (wnode_aff_candidate == 0)
                        wnode_aff_candidate = job_id;
                    continue;
                }
            }

            if (exclusive_new_affinity) {
                if (aff_id == 0 || all_pref_aff[aff_id] == false) {
                    if (explicit_aff == false && wnode_affinity == false) {
                        job_pick.job_id = job_id;
                        job_pick.exclusive = true;
                        job_pick.aff_id = aff_id;
                        return job_pick;
                    }
                    if (exclusive_aff_candidate == 0)
                        exclusive_aff_candidate = job_id;
                }
            }
        }

        if (wnode_aff_candidate != 0) {
            job_pick.job_id = wnode_aff_candidate;
            job_pick.exclusive = false;
            job_pick.aff_id = 0;
            return job_pick;
        }

        if (exclusive_aff_candidate != 0) {
            job_pick.job_id = exclusive_aff_candidate;
            job_pick.exclusive = true;
            job_pick.aff_id = m_GCRegistry.GetAffinityID(exclusive_aff_candidate);
            return job_pick;
        }
    }


    if (any_affinity ||
        (!explicit_aff && !wnode_affinity && !exclusive_new_affinity)) {
        job_pick.job_id = m_StatusTracker.GetJobByStatus(
                                    CNetScheduleAPI::ePending,
                                    blacklisted_jobs,
                                    TNSBitVector());
        job_pick.exclusive = false;
        job_pick.aff_id = 0;
        return job_pick;
    }

    return job_pick;
}


CQueue::x_SJobPick
CQueue::x_FindOutdatedPendingJob(const CNSClientId &  client,
                                 unsigned int         picked_earlier)
{
    x_SJobPick      job_pick = { 0, false, 0 };

    if (m_MaxPendingWaitTimeout.tv_sec == 0 &&
        m_MaxPendingWaitTimeout.tv_nsec == 0)
        return job_pick;    // Not configured

    TNSBitVector    outdated_pending =
                        m_StatusTracker.GetOutdatedPendingJobs(
                                m_MaxPendingWaitTimeout,
                                m_GCRegistry);
    if (picked_earlier != 0)
        outdated_pending.set_bit(picked_earlier, false);

    if (!outdated_pending.any())
        return job_pick;

    outdated_pending -= m_ClientsRegistry.GetBlacklistedJobs(client);

    if (!outdated_pending.any())
        return job_pick;

    job_pick.job_id = *outdated_pending.first();
    job_pick.aff_id = m_GCRegistry.GetAffinityID(job_pick.job_id);
    job_pick.exclusive = job_pick.aff_id != 0;
    return job_pick;
}


string CQueue::GetAffinityList()
{
    list< SAffinityStatistics >
                statistics = m_AffinityRegistry.GetAffinityStatistics(m_StatusTracker);

    string          aff_list;
    for (list< SAffinityStatistics >::const_iterator  k = statistics.begin();
         k != statistics.end(); ++k) {
        if (!aff_list.empty())
            aff_list += "&";

        aff_list += NStr::URLEncode(k->m_Token) + '=' +
                    NStr::SizetToString(k->m_NumberOfPendingJobs) + "," +
                    NStr::SizetToString(k->m_NumberOfRunningJobs) + "," +
                    NStr::SizetToString(k->m_NumberOfPreferred) + "," +
                    NStr::SizetToString(k->m_NumberOfWaitGet);
    }
    return aff_list;
}


TJobStatus CQueue::FailJob(const CNSClientId &    client,
                           unsigned int           job_id,
                           const string &         auth_token,
                           const string &         err_msg,
                           const string &         output,
                           int                    ret_code,
                           string                 warning)
{
    unsigned        failed_retries;
    unsigned        max_output_size;
    // time_t          blacklist_time;
    {{
        CQueueParamAccessor     qp(*this);
        failed_retries  = qp.GetFailedRetries();
        max_output_size = qp.GetMaxOutputSize();
        // blacklist_time  = qp.GetBlacklistTime();
    }}

    if (output.size() > max_output_size) {
        NCBI_THROW(CNetScheduleException, eDataTooLong,
                   "Output is too long");
    }

    CJob                job;
    time_t              curr = time(0);
    bool                rescheduled = false;
    TJobStatus          old_status;

    {{
        CFastMutexGuard     guard(m_OperationLock);
        TJobStatus          new_status = CNetScheduleAPI::eFailed;

        old_status = GetJobStatus(job_id);
        if (old_status == CNetScheduleAPI::eFailed) {
            m_StatisticsCounters.CountTransition(CNetScheduleAPI::eFailed,
                                                 CNetScheduleAPI::eFailed);
            return old_status;
        }

        if (old_status != CNetScheduleAPI::eRunning) {
            // No job state change
            return old_status;
        }

        {{
            CNSTransaction     transaction(this);

            if (job.Fetch(this, job_id) != CJob::eJF_Ok)
                NCBI_THROW(CNetScheduleException, eInternalError,
                           "Error fetching job: " + DecorateJobId(job_id));

            if (!auth_token.empty()) {
                // Need to check authorization token first
                CJob::EAuthTokenCompareResult   token_compare_result =
                                            job.CompareAuthToken(auth_token);
                if (token_compare_result == CJob::eInvalidTokenFormat)
                    NCBI_THROW(CNetScheduleException, eInvalidAuthToken,
                               "Invalid authorization token format");
                if (token_compare_result == CJob::eNoMatch)
                    NCBI_THROW(CNetScheduleException, eInvalidAuthToken,
                               "Authorization token does not match");
                if (token_compare_result == CJob::ePassportOnlyMatch) {
                    // That means the job has been given to another worker node
                    // by whatever reason (expired/failed/returned before)
                    LOG_POST(Message << Warning << "Received FPUT2 with only "
                                                   "passport matched.");
                    warning = "Only job passport matched. Command is ignored.";
                    return old_status;
                }
                // Here: the authorization token is OK, we can continue
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
            } else {
                job.SetStatus(CNetScheduleAPI::eFailed);
                event->SetStatus(CNetScheduleAPI::eFailed);
                new_status = CNetScheduleAPI::eFailed;
                rescheduled = false;
                if (m_Log)
                    LOG_POST(Message << Warning << "Job " << DecorateJobId(job_id)
                                     << " failed, exceeded max number of retries ("
                                     << failed_retries << ")");
            }

            job.SetOutput(output);
            job.SetLastTouch(curr);
            job.Flush(this);
            transaction.Commit();
        }}

        m_StatusTracker.SetStatus(job_id, new_status);
        if (new_status == CNetScheduleAPI::ePending)
            m_StatisticsCounters.CountTransition(CNetScheduleAPI::eRunning,
                                                 new_status,
                                                 CStatisticsCounters::eFail);
        else
            m_StatisticsCounters.CountTransition(CNetScheduleAPI::eRunning,
                                                 new_status,
                                                 CStatisticsCounters::eNone);

        TimeLineRemove(job_id);

        // Replace it with ClearExecuting(client, job_id) when all clients provide
        // their credentials and job passport is checked strictly
        m_ClientsRegistry.ClearExecuting(job_id);
        m_ClientsRegistry.AddToBlacklist(client, job_id);

        m_GCRegistry.UpdateLifetime(job_id,
                                    job.GetExpirationTime(m_Timeout,
                                                          m_RunTimeout,
                                                          m_PendingTimeout,
                                                          0));

        if (!rescheduled  &&  job.ShouldNotifySubmitter(curr)) {
            m_NotificationsList.NotifyJobStatus(job.GetSubmAddr(),
                                                job.GetSubmNotifPort(),
                                                MakeKey(job_id),
                                                job.GetStatus());
        }

        if (rescheduled)
            m_NotificationsList.Notify(job_id,
                                       job.GetAffinityId(), m_ClientsRegistry,
                                       m_AffinityRegistry, m_NotifHifreqPeriod);

    }}

    if (job.ShouldNotifyListener(curr))
        m_NotificationsList.NotifyJobStatus(job.GetListenerNotifAddr(),
                                            job.GetListenerNotifPort(),
                                            MakeKey(job_id),
                                            job.GetStatus());

    return old_status;
}


string  CQueue::GetAffinityTokenByID(unsigned int  aff_id) const
{
    return m_AffinityRegistry.GetTokenByID(aff_id);
}


void CQueue::ClearWorkerNode(const CNSClientId &  client)
{
    // Get the running and reading jobs and move them to the corresponding
    // states (pending and done)

    TNSBitVector    running_jobs;
    TNSBitVector    reading_jobs;

    {{
        CFastMutexGuard     guard(m_OperationLock);
        unsigned short      wait_port = m_ClientsRegistry.ClearWorkerNode(
                                                client,
                                                m_AffinityRegistry,
                                                running_jobs,
                                                reading_jobs);

        if (wait_port != 0)
            m_NotificationsList.UnregisterListener(client, wait_port);
    }}

    if (running_jobs.any())
        x_ResetRunningDueToClear(client, running_jobs);
    if (reading_jobs.any())
        x_ResetReadingDueToClear(client, reading_jobs);
    return;
}


// Triggered from a notification thread only
void CQueue::NotifyListenersPeriodically(time_t  current_time)
{
    if (m_MaxPendingWaitTimeout.tv_sec != 0 ||
        m_MaxPendingWaitTimeout.tv_nsec != 0) {
        // Pending outdated timeout is configured, so check for
        // outdated jobs
        CFastMutexGuard     guard(m_OperationLock);
        TNSBitVector        outdated_pending =
                                    m_StatusTracker.GetOutdatedPendingJobs(
                                        m_MaxPendingWaitTimeout,
                                        m_GCRegistry);
        if (outdated_pending.any())
            m_NotificationsList.CheckOutdatedJobs(outdated_pending,
                                                  m_ClientsRegistry,
                                                  m_NotifHifreqPeriod);
    }


    // Check the configured notification interval
    static double   last_notif_timeout = -1.0;
    static size_t   skip_limit = 0;
    static size_t   skip_count;

    if (m_NotifHifreqInterval != last_notif_timeout) {
        last_notif_timeout = m_NotifHifreqInterval;
        skip_count = 0;
        skip_limit = size_t(m_NotifHifreqInterval/0.1);
    }

    ++skip_count;
    if (skip_count < skip_limit)
        return;

    skip_count = 0;

    // The NotifyPeriodically() and CheckTimeout() calls may need to modify
    // the clients and affinity registry so it is safer to take the queue lock.
    CFastMutexGuard     guard(m_OperationLock);
    if (m_StatusTracker.AnyPending())
        m_NotificationsList.NotifyPeriodically(current_time,
                                               m_NotifLofreqMult,
                                               m_ClientsRegistry,
                                               m_AffinityRegistry);
    else
        m_NotificationsList.CheckTimeout(current_time,
                                         m_ClientsRegistry,
                                         m_AffinityRegistry);

    return;
}


void CQueue::PrintClientsList(CNetScheduleHandler &  handler,
                              bool                   verbose) const
{
    m_ClientsRegistry.PrintClientsList(this, handler,
                                       m_AffinityRegistry, verbose);
}


void CQueue::PrintNotificationsList(CNetScheduleHandler &  handler,
                                    bool                   verbose) const
{
    m_NotificationsList.Print(handler,
                              m_ClientsRegistry,
                              m_AffinityRegistry, verbose);
}


void CQueue::PrintAffinitiesList(CNetScheduleHandler &  handler,
                                 bool                   verbose) const
{
    m_AffinityRegistry.Print(this,
                             handler,
                             m_ClientsRegistry,
                             verbose);
}


void CQueue::PrintGroupsList(CNetScheduleHandler &  handler,
                             bool                   verbose) const
{
    m_GroupRegistry.Print(this, handler, verbose);
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
            job.SetLastTouch(curr_time);

            event = &job.AppendEvent();
            event->SetStatus(new_status);
            event->SetEvent(event_type);
            event->SetTimestamp(curr_time);

            job.Flush(this);
            transaction.Commit();
        }}


        m_StatusTracker.SetStatus(job_id, new_status);
        m_GCRegistry.UpdateLifetime(job_id,
                                    job.GetExpirationTime(m_Timeout,
                                                          m_RunTimeout,
                                                          m_PendingTimeout,
                                                          0));

        if (status == CNetScheduleAPI::eRunning) {
            if (new_status == CNetScheduleAPI::ePending) {
                // Timeout and reschedule, put to blacklist as well
                m_ClientsRegistry.ClearExecutingSetBlacklist(job_id);
                m_StatisticsCounters.CountTransition(CNetScheduleAPI::eRunning,
                                                     CNetScheduleAPI::ePending,
                                                     CStatisticsCounters::eTimeout);
            }
            else {
                m_ClientsRegistry.ClearExecuting(job_id);
                m_StatisticsCounters.CountTransition(CNetScheduleAPI::eRunning,
                                                     CNetScheduleAPI::eFailed,
                                                     CStatisticsCounters::eTimeout);
            }
        } else {
            if (new_status == CNetScheduleAPI::eDone) {
                // Timeout and reschedule for reading, put to the blacklist
                m_ClientsRegistry.ClearReadingSetBlacklist(job_id);
                m_StatisticsCounters.CountTransition(CNetScheduleAPI::eReading,
                                                     CNetScheduleAPI::eDone,
                                                     CStatisticsCounters::eTimeout);
            }
            else {
                m_ClientsRegistry.ClearReading(job_id);
                m_StatisticsCounters.CountTransition(CNetScheduleAPI::eReading,
                                                     CNetScheduleAPI::eReadFailed,
                                                     CStatisticsCounters::eTimeout);
            }
        }

        if (new_status == CNetScheduleAPI::ePending)
            m_NotificationsList.Notify(job_id,
                                       job.GetAffinityId(), m_ClientsRegistry,
                                       m_AffinityRegistry, m_NotifHifreqPeriod);

    }}

    if (new_status == CNetScheduleAPI::eFailed &&
        job.ShouldNotifySubmitter(curr_time))
        m_NotificationsList.NotifyJobStatus(job.GetSubmAddr(),
                                            job.GetSubmNotifPort(),
                                            MakeKey(job_id),
                                            job.GetStatus());

    if (job.ShouldNotifyListener(curr_time))
        m_NotificationsList.NotifyJobStatus(job.GetListenerNotifAddr(),
                                            job.GetListenerNotifPort(),
                                            MakeKey(job_id),
                                            job.GetStatus());

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


// Checks up to given # of jobs at the given status for expiration and
// marks up to given # of jobs for deletion.
// Returns the # of performed scans, the # of jobs marked for deletion and
// the last scanned job id.
SPurgeAttributes  CQueue::CheckJobsExpiry(time_t             current_time,
                                          SPurgeAttributes   attributes,
                                          unsigned int       last_job,
                                          TJobStatus         status)
{
    TNSBitVector        job_ids;
    SPurgeAttributes    result;
    unsigned int        job_id;
    unsigned int        aff;
    unsigned int        group;

    result.job_id = attributes.job_id;
    result.deleted = 0;
    {{
        CFastMutexGuard     guard(m_OperationLock);

        for (result.scans = 0; result.scans < attributes.scans; ++result.scans) {
            job_id = m_StatusTracker.GetNext(status, result.job_id);
            if (job_id == 0)
                break;  // No more jobs in the state
            if (last_job != 0 && job_id >= last_job)
                break;  // The job in the state is above the limit

            result.job_id = job_id;

            if (m_GCRegistry.DeleteIfTimedOut(job_id, current_time,
                                              &aff, &group))
            {
                // The job is expired and needs to be marked for deletion
                m_StatusTracker.Erase(job_id);
                job_ids.set_bit(job_id);
                ++result.deleted;

                // check if the affinity should also be updated
                if (aff != 0)
                    m_AffinityRegistry.RemoveJobFromAffinity(job_id, aff);

                // Check if the group registry should also be updated
                if (group != 0)
                    m_GroupRegistry.RemoveJob(group, job_id);

                if (result.deleted >= attributes.deleted)
                    break;
            }
        }
    }}

    if (result.deleted > 0)
        x_Erase(job_ids);

    return result;
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


unsigned int  CQueue::DeleteBatch(unsigned int  max_deleted)
{
    // Copy the vector with deleted jobs
    TNSBitVector    jobs_to_delete;

    {{
         CFastMutexGuard     guard(m_JobsToDeleteLock);
         jobs_to_delete = m_JobsToDelete;
    }}

    static const size_t         chunk_size = 100;
    unsigned int                del_rec = 0;
    TNSBitVector::enumerator    en = jobs_to_delete.first();
    TNSBitVector                deleted_jobs;

    while (en.valid() && del_rec < max_deleted) {
        {{
            CFastMutexGuard     guard(m_OperationLock);
            CNSTransaction      transaction(this);

            for (size_t n = 0;
                 en.valid() && n < chunk_size && del_rec < max_deleted;
                 ++en, ++n) {
                unsigned int    job_id = *en;

                try {
                    m_QueueDbBlock->job_db.id = job_id;
                    m_QueueDbBlock->job_db.Delete();
                    ++del_rec;
                    deleted_jobs.set_bit(job_id, true);
                } catch (CBDB_ErrnoException& ex) {
                    ERR_POST("BDB error " << ex.what());
                }

                try {
                    m_QueueDbBlock->job_info_db.id = job_id;
                    m_QueueDbBlock->job_info_db.Delete();
                } catch (CBDB_ErrnoException& ex) {
                    ERR_POST("BDB error " << ex.what());
                }

                x_DeleteJobEvents(job_id);
            }

            transaction.Commit();
        }}
    }

    if (del_rec > 0) {
        m_StatisticsCounters.CountDBDeletion(del_rec);

        TNSBitVector::enumerator    en = deleted_jobs.first();
        CFastMutexGuard             guard(m_JobsToDeleteLock);
        for (; en.valid(); ++en)
            m_JobsToDelete.set_bit(*en, false);
    }
    return del_rec;
}


// See CXX-2838 for the description of how affinities garbage collection is
// going  to work.
unsigned int  CQueue::PurgeAffinities(void)
{
    unsigned int    aff_dict_size = m_AffinityRegistry.size();

    if (aff_dict_size < (m_AffinityLowMarkPercentage / 100.0) * m_MaxAffinities)
        // Did not reach the dictionary low mark
        return 0;

    unsigned int    del_limit = m_AffinityHighRemoval;
    if (aff_dict_size < (m_AffinityHighMarkPercentage / 100.0) * m_MaxAffinities) {
        // Here: check the percentage of the affinities that have no references
        // to them
        unsigned int    candidates_size = m_AffinityRegistry.CheckRemoveCandidates();

        if (candidates_size < (m_AffinityDirtPercentage / 100.0) * m_MaxAffinities)
            // The number of candidates to be deleted is low
            return 0;

        del_limit = m_AffinityLowRemoval;
    }


    // Here: need to delete affinities from the memory and DB
    CFastMutexGuard     guard(m_OperationLock);
    CNSTransaction      transaction(this);

    unsigned int        del_count = m_AffinityRegistry.CollectGarbage(del_limit);
    transaction.Commit();

    return del_count;
}


unsigned int  CQueue::PurgeGroups(void)
{
    CFastMutexGuard     guard(m_OperationLock);
    CNSTransaction      transaction(this);

    unsigned int        del_count = m_GroupRegistry.CollectGarbage(100);
    transaction.Commit();

    return del_count;
}


void  CQueue::PurgeWNodes(time_t  current_time)
{
    // Clears the worker nodes affinities if the workers are inactive for
    // certain time
    CFastMutexGuard     guard(m_OperationLock);

    vector< pair< unsigned int,
                  unsigned short > >  notif_to_reset =
                                        m_ClientsRegistry.Purge(
                                                current_time, m_WNodeTimeout,
                                                m_AffinityRegistry);
    for (vector< pair< unsigned int,
                       unsigned short > >::const_iterator  k = notif_to_reset.begin();
         k != notif_to_reset.end(); ++k)
        m_NotificationsList.UnregisterListener(k->first, k->second);

    return;
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
    string      hosts;

    {{
        CQueueParamAccessor     qp(*this);
        hosts = qp.GetSubmHosts().Print("OK:", "\n");
    }}

    if (!hosts.empty())
        hosts += "\n";
    handler.WriteMessage(hosts);
}


void CQueue::PrintWNodeHosts(CNetScheduleHandler &  handler) const
{
    string      hosts;

    {{
         CQueueParamAccessor     qp(*this);
         hosts = qp.GetWnodeHosts().Print("OK:", "\n");
    }}

    if (!hosts.empty())
        hosts += "\n";
    handler.WriteMessage(hosts);
}


void CQueue::x_PrintShortJobStat(CNetScheduleHandler &  handler,
                                 const CJob &           job)
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
                              unsigned                  job_id)
{
    // Check first that the job has not been deleted yet
    {{
        CFastMutexGuard     guard(m_JobsToDeleteLock);
        if (m_JobsToDelete[job_id])
            return 0;
    }}

    CJob                job;
    {{
        CFastMutexGuard     guard(m_OperationLock);

        m_QueueDbBlock->job_db.SetTransaction(NULL);
        m_QueueDbBlock->events_db.SetTransaction(NULL);
        m_QueueDbBlock->job_info_db.SetTransaction(NULL);

        CJob::EJobFetchResult   res = job.Fetch(this, job_id);
        if (res != CJob::eJF_Ok)
            return 0;
    }}

    job.Print(handler, *this, m_AffinityRegistry, m_GroupRegistry);
    return 1;
}


void CQueue::PrintAllJobDbStat(CNetScheduleHandler &   handler,
                               const string &          group,
                               TJobStatus              job_status,
                               unsigned int            start_after_job_id,
                               unsigned int            count)
{
    // Form a bit vector of all jobs to dump
    vector<CNetScheduleAPI::EJobStatus>     statuses;
    TNSBitVector                            jobs_to_dump;

    if (job_status == CNetScheduleAPI::eJobNotFound) {
        // All statuses
        statuses.push_back(CNetScheduleAPI::ePending);
        statuses.push_back(CNetScheduleAPI::eRunning);
        statuses.push_back(CNetScheduleAPI::eCanceled);
        statuses.push_back(CNetScheduleAPI::eFailed);
        statuses.push_back(CNetScheduleAPI::eDone);
        statuses.push_back(CNetScheduleAPI::eReading);
        statuses.push_back(CNetScheduleAPI::eConfirmed);
        statuses.push_back(CNetScheduleAPI::eReadFailed);
    }
    else {
        // The user specified one state explicitly
        statuses.push_back(job_status);
    }


    {{
        CFastMutexGuard     guard(m_OperationLock);

        jobs_to_dump = m_StatusTracker.GetJobs(statuses);
    }}

    // Check if a certain group has been specified
    if (!group.empty())
        jobs_to_dump &= m_GroupRegistry.GetJobs(group);

    x_DumpJobs(handler, jobs_to_dump, start_after_job_id, count);
    return;
}


void CQueue::x_DumpJobs(CNetScheduleHandler &   handler,
                        const TNSBitVector &    jobs_to_dump,
                        unsigned int            start_after_job_id,
                        unsigned int            count)
{
    // Skip the jobs which should not be dumped
    TNSBitVector::enumerator    en(jobs_to_dump.first());
    while (en.valid() && *en <= start_after_job_id)
        ++en;

    // Identify the required buffer size for jobs
    size_t      buffer_size = m_DumpBufferSize;
    if (count != 0 && count < buffer_size)
        buffer_size = count;

    {{
        CJob    buffer[buffer_size];

        size_t      read_jobs = 0;
        size_t      printed_count = 0;

        for ( ; en.valid(); ) {
            {{
                CFastMutexGuard     guard(m_OperationLock);
                m_QueueDbBlock->job_db.SetTransaction(NULL);
                m_QueueDbBlock->events_db.SetTransaction(NULL);
                m_QueueDbBlock->job_info_db.SetTransaction(NULL);

                for ( ; en.valid() && read_jobs < buffer_size; ++en )
                    if (buffer[read_jobs].Fetch(this, *en) == CJob::eJF_Ok) {
                        ++read_jobs;
                        ++printed_count;

                        if (count != 0)
                            if (printed_count >= count)
                                break;
                    }
            }}

            // Print what was read
            for (size_t  index = 0; index < read_jobs; ++index) {
                handler.WriteMessage("");
                buffer[index].Print(handler, *this,
                                    m_AffinityRegistry, m_GroupRegistry);
                CTime  gc_exp(m_GCRegistry.GetLifetime(buffer[index].GetId()));
                gc_exp.ToLocalTime();

                handler.WriteMessage("OK:GC erase time: " + gc_exp.AsString());
            }

            if (count != 0)
                if (printed_count >= count)
                    break;

            read_jobs = 0;
        }
    }}

    return;
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


void CQueue::TouchClientsRegistry(CNSClientId &  client)
{
    TNSBitVector        running_jobs;
    TNSBitVector        reading_jobs;

    {{
        CFastMutexGuard     guard(m_OperationLock);
        unsigned short      wait_port = m_ClientsRegistry.Touch(
                                                client,
                                                m_AffinityRegistry,
                                                running_jobs,
                                                reading_jobs);
        if (wait_port != 0)
            m_NotificationsList.UnregisterListener(client, wait_port);
    }}


    if (running_jobs.any())
        x_ResetRunningDueToNewSession(client, running_jobs);
    if (reading_jobs.any())
        x_ResetReadingDueToNewSession(client, reading_jobs);
    return;
}


// Moves the job to Pending/Failed or to Done/ReadFailed
// when event event_type has come
TJobStatus  CQueue::x_ResetDueTo(const CNSClientId &   client,
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
            return CNetScheduleAPI::eJobNotFound;
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
        job.SetLastTouch(current_time);

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

    // Count the transition
    CStatisticsCounters::ETransitionPathOption
                                transition_path = CStatisticsCounters::eNone;
    if (event_type == CJobEvent::eClear)
        transition_path = CStatisticsCounters::eClear;
    else if (event_type == CJobEvent::eSessionChanged)
        transition_path = CStatisticsCounters::eNewSession;
    m_StatisticsCounters.CountTransition(status_from, new_status,
                                         transition_path);

    m_GCRegistry.UpdateLifetime(job_id, job.GetExpirationTime(m_Timeout,
                                                              m_RunTimeout,
                                                              m_PendingTimeout,
                                                              0));

    // remove the job from the time line
    TimeLineRemove(job_id);

    // Notify those who wait for the jobs if needed
    if (new_status == CNetScheduleAPI::ePending)
        m_NotificationsList.Notify(job_id, job.GetAffinityId(),
                                   m_ClientsRegistry,
                                   m_AffinityRegistry,
                                   m_NotifHifreqPeriod);

    if (job.ShouldNotifyListener(current_time))
        m_NotificationsList.NotifyJobStatus(job.GetListenerNotifAddr(),
                                            job.GetListenerNotifPort(),
                                            MakeKey(job_id),
                                            job.GetStatus());

    return new_status;
}


void CQueue::x_ResetRunningDueToClear(const CNSClientId &   client,
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


void CQueue::x_ResetReadingDueToClear(const CNSClientId &   client,
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


void CQueue::x_ResetRunningDueToNewSession(const CNSClientId &   client,
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


void CQueue::x_ResetReadingDueToNewSession(const CNSClientId &   client,
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


void CQueue::x_RegisterGetListener(const CNSClientId &   client,
                                   unsigned short        port,
                                   unsigned int          timeout,
                                   const TNSBitVector &  aff_ids,
                                   bool                  wnode_aff,
                                   bool                  any_aff,
                                   bool                  exclusive_new_affinity,
                                   bool                  new_format)
{
    // Add to the notification list and save the wait port
    m_NotificationsList.RegisterListener(client, port, timeout,
                                         wnode_aff, any_aff,
                                         exclusive_new_affinity, new_format);
    if (client.IsComplete())
        m_ClientsRegistry.SetWaiting(client, port, aff_ids, m_AffinityRegistry);
    return;
}


bool CQueue::x_UnregisterGetListener(const CNSClientId &  client,
                                     unsigned short       port)
{
    unsigned short      notif_port = port;

    if (client.IsComplete())
        // New clients have the port in their registry record
        notif_port = m_ClientsRegistry.ResetWaiting(client, m_AffinityRegistry);

    if (notif_port > 0) {
        m_NotificationsList.UnregisterListener(client, notif_port);
        return true;
    }

    return false;
}


void CQueue::PrintStatistics(size_t &  aff_count)
{
    size_t      affinities = m_AffinityRegistry.size();
    aff_count += affinities;

    // The member is called only if there is a request context
    CDiagContext_Extra extra = GetDiagContext().Extra()
        .Print("queue", GetQueueName())
        .Print("affinities", affinities)
        .Print("pending", CountStatus(CNetScheduleAPI::ePending))
        .Print("running", CountStatus(CNetScheduleAPI::eRunning))
        .Print("canceled", CountStatus(CNetScheduleAPI::eCanceled))
        .Print("failed", CountStatus(CNetScheduleAPI::eFailed))
        .Print("done", CountStatus(CNetScheduleAPI::eDone))
        .Print("reading", CountStatus(CNetScheduleAPI::eReading))
        .Print("confirmed", CountStatus(CNetScheduleAPI::eConfirmed))
        .Print("readfailed", CountStatus(CNetScheduleAPI::eReadFailed));
    m_StatisticsCounters.PrintTransitions(extra);
    extra.Flush();
}


void CQueue::PrintTransitionCounters(CNetScheduleHandler &  handler)
{
    m_StatisticsCounters.PrintTransitions(handler);
    handler.WriteMessage("OK:garbage_jobs: " +
                         NStr::IntToString(m_JobsToDelete.count()));
    handler.WriteMessage("OK:affinity_registry_size: " +
                         NStr::SizetToString(m_AffinityRegistry.size()));
    handler.WriteMessage("OK:client_registry_size: " +
                         NStr::SizetToString(m_ClientsRegistry.size()));
    return;
}


void CQueue::PrintJobsStat(CNetScheduleHandler &  handler,
                           const string &         group_token,
                           const string &         aff_token)
{
    size_t              jobs_per_state[g_ValidJobStatusesSize];
    TNSBitVector        group_jobs;
    TNSBitVector        aff_jobs;

    {{
        CFastMutexGuard     guard(m_OperationLock);

        if (!group_token.empty())
            group_jobs = m_GroupRegistry.GetJobs(group_token);
        if (!aff_token.empty()) {
            unsigned int  aff_id = m_AffinityRegistry.GetIDByToken(aff_token);
            if (aff_id == 0)
                NCBI_THROW(CNetScheduleException, eAffinityNotFound,
                           "Unknown affinity token \"" + aff_token + "\"");

            aff_jobs = m_AffinityRegistry.GetJobsWithAffinity(aff_id);
        }

        for (size_t  index(0); index < g_ValidJobStatusesSize; ++index) {
            TNSBitVector  candidates =
                            m_StatusTracker.GetJobs(g_ValidJobStatuses[index]);

            if (!group_token.empty())
                candidates &= group_jobs;
            if (!aff_token.empty())
                candidates &= aff_jobs;

            jobs_per_state[index] = candidates.count();
        }
    }}

    size_t              total = 0;
    for (size_t  index(0); index < g_ValidJobStatusesSize; ++index) {
        handler.WriteMessage("OK:" + CNetScheduleAPI::StatusToString(g_ValidJobStatuses[index]) +
                             ": " + NStr::SizetToString(jobs_per_state[index]));
        total += jobs_per_state[index];
    }
    handler.WriteMessage("OK:Total: " +
                         NStr::SizetToString(total));
    return;
}


unsigned int CQueue::CountActiveJobs(void) const
{
    vector<CNetScheduleAPI::EJobStatus>     statuses;

    statuses.push_back(CNetScheduleAPI::ePending);
    statuses.push_back(CNetScheduleAPI::eRunning);
    return m_StatusTracker.CountStatus(statuses);
}


void CQueue::x_UpdateDB_PutResultNoLock(unsigned             job_id,
                                        const string &       auth_token,
                                        time_t               curr,
                                        int                  ret_code,
                                        const string &       output,
                                        CJob &               job,
                                        const CNSClientId &  client)
{
    if (job.Fetch(this, job_id) != CJob::eJF_Ok)
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "Error fetching job: " + DecorateJobId(job_id));

    if (!auth_token.empty()) {
        // Need to check authorization token first
        CJob::EAuthTokenCompareResult   token_compare_result =
                                        job.CompareAuthToken(auth_token);
        if (token_compare_result == CJob::eInvalidTokenFormat)
            NCBI_THROW(CNetScheduleException, eInvalidAuthToken,
                       "Invalid authorization token format");
        if (token_compare_result == CJob::eNoMatch)
            NCBI_THROW(CNetScheduleException, eInvalidAuthToken,
                       "Authorization token does not match");
        if (token_compare_result == CJob::ePassportOnlyMatch) {
            // That means that the job has been executing by another worker
            // node at the moment, but we can accept the results anyway
            LOG_POST(Message << Warning << "Received PUT2 with only "
                                           "passport matched.");
        }
        // Here: the authorization token is OK, we can continue
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
    job.SetLastTouch(curr);

    job.Flush(this);
    return;
}


// If the job.job_id != 0 => the job has been read successfully
// Exception => DB errors
// Return: true  => job is ok to get
//         false => job is expired and was marked for deletion,
//                  need to get another job
bool  CQueue::x_UpdateDB_GetJobNoLock(const CNSClientId &  client,
                                      time_t               curr,
                                      unsigned int         job_id,
                                      CJob &               job)
{
    CNSTransaction      transaction(this);

    if (job.Fetch(this, job_id) != CJob::eJF_Ok)
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "Cannot read job info from DB");

    // The job has been read successfully, check if it is still within the
    // expiration timeout; note: run_timeout is not applicable because the
    // job is guaranteed in the pending state
    if (job.GetExpirationTime(m_Timeout, 0, m_PendingTimeout, 0) <= curr) {
        // The job has expired, so mark it for deletion
        EraseJob(job_id);

        if (job.GetAffinityId() != 0)
            m_AffinityRegistry.RemoveJobFromAffinity(
                                    job_id, job.GetAffinityId());
        if (job.GetGroupId() != 0)
            m_GroupRegistry.RemoveJob(job.GetGroupId(), job_id);

        return false;
    }

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
    job.SetLastTouch(curr);

    job.Flush(this);
    transaction.Commit();

    return true;
}


END_NCBI_SCOPE

