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
#include "ns_precise_time.hpp"
#include "ns_rollback.hpp"
#include "queue_database.hpp"
#include "ns_handler.hpp"
#include "ns_ini_params.hpp"
#include "ns_perf_logging.hpp"

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
               TQueueKind            queue_kind,
               CNetScheduleServer *  server,
               CQueueDataBase &      qdb)
  :
    m_Server(server),
    m_QueueDB(qdb),
    m_RunTimeLine(NULL),
    m_Executor(executor),
    m_QueueName(queue_name),
    m_Kind(queue_kind),
    m_QueueDbBlock(0),
    m_TruncateAtDetach(false),

    m_LastId(0),
    m_SavedId(s_ReserveDelta),

    m_JobsToDeleteOps(0),
    m_ReadJobsOps(0),

    m_Timeout(default_timeout),
    m_RunTimeout(default_run_timeout),
    m_ReadTimeout(default_read_timeout),
    m_FailedRetries(default_failed_retries),
    m_ReadFailedRetries(default_failed_retries),  // See CXX-5161, the same
                                                  // default as for
                                                  // failed_retries
    m_BlacklistTime(default_blacklist_time),
    m_ReadBlacklistTime(default_blacklist_time),  // See CXX-4993, the same
                                                  // default as for
                                                  // blacklist_time
    m_MaxInputSize(kNetScheduleMaxDBDataSize),
    m_MaxOutputSize(kNetScheduleMaxDBDataSize),
    m_WNodeTimeout(default_wnode_timeout),
    m_ReaderTimeout(default_reader_timeout),
    m_PendingTimeout(default_pending_timeout),
    m_KeyGenerator(server->GetHost(), server->GetPort(), queue_name),
    m_Log(server->IsLog()),
    m_LogBatchEachJob(server->IsLogBatchEachJob()),
    m_RefuseSubmits(false),
    m_StatisticsCounters(CStatisticsCounters::eQueueCounters),
    m_StatisticsCountersLastPrinted(CStatisticsCounters::eQueueCounters),
    m_StatisticsCountersLastPrintedTimestamp(0.0),
    m_MaxAffinities(server->GetMaxAffinities()),
    m_AffinityHighMarkPercentage(server->GetAffinityHighMarkPercentage()),
    m_AffinityLowMarkPercentage(server->GetAffinityLowMarkPercentage()),
    m_AffinityHighRemoval(server->GetAffinityHighRemoval()),
    m_AffinityLowRemoval(server->GetAffinityLowRemoval()),
    m_AffinityDirtPercentage(server->GetAffinityDirtPercentage()),
    m_NotificationsList(qdb, server->GetNodeID(), queue_name),
    m_NotifHifreqInterval(default_notif_hifreq_interval), // 0.1 sec
    m_NotifHifreqPeriod(default_notif_hifreq_period),
    m_NotifLofreqMult(default_notif_lofreq_mult),
    m_DumpBufferSize(default_dump_buffer_size),
    m_DumpClientBufferSize(default_dump_client_buffer_size),
    m_DumpAffBufferSize(default_dump_aff_buffer_size),
    m_DumpGroupBufferSize(default_dump_group_buffer_size),
    m_ScrambleJobKeys(default_scramble_job_keys),
    m_PauseStatus(eNoPause),
    m_ClientRegistryTimeoutWorkerNode(
                                default_client_registry_timeout_worker_node),
    m_ClientRegistryMinWorkerNodes(default_client_registry_min_worker_nodes),
    m_ClientRegistryTimeoutAdmin(default_client_registry_timeout_admin),
    m_ClientRegistryMinAdmins(default_client_registry_min_admins),
    m_ClientRegistryTimeoutSubmitter(default_client_registry_timeout_submitter),
    m_ClientRegistryMinSubmitters(default_client_registry_min_submitters),
    m_ClientRegistryTimeoutReader(default_client_registry_timeout_reader),
    m_ClientRegistryMinReaders(default_client_registry_min_readers),
    m_ClientRegistryTimeoutUnknown(default_client_registry_timeout_unknown),
    m_ClientRegistryMinUnknowns(default_client_registry_min_unknowns)
{
    _ASSERT(!queue_name.empty());
    m_ClientsRegistry.SetRegistries(&m_AffinityRegistry,
                                    &m_NotificationsList);
}


CQueue::~CQueue()
{
    delete m_RunTimeLine;
    x_Detach();
}


void CQueue::Attach(SQueueDbBlock* block)
{
    x_Detach();
    m_QueueDbBlock = block;
    m_AffinityRegistry.Attach(&m_QueueDbBlock->aff_dict_db);
    m_GroupRegistry.Attach(&m_QueueDbBlock->group_dict_db);

    // Here we have a db, so we can read the counter value we should start from
    m_LastId = m_Server->GetJobsStartID(m_QueueName);
    m_SavedId = m_LastId + s_ReserveDelta;
    if (m_SavedId < m_LastId) {
        // Overflow
        m_LastId = 0;
        m_SavedId = s_ReserveDelta;
    }
    m_Server->SetJobsStartID(m_QueueName, m_SavedId);
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
        LOG_POST(Note << "Clean up of db block "
                      << m_QueueDbBlock->pos << " complete, "
                      << sw.Elapsed() << " elapsed");
    }
private:
    SQueueDbBlock* m_QueueDbBlock;

};


void CQueue::x_Detach(void)
{
    m_AffinityRegistry.Detach();
    if (!m_QueueDbBlock)
        return;

    // We are here have synchronized access to m_QueueDbBlock without mutex
    // because we are here only when the last reference to CQueue is
    // destroyed. So as long m_QueueDbBlock->allocated is true it cannot
    // be allocated again and thus cannot be accessed as well.
    // As soon as we're done with detaching (here) or truncating (in separate
    // request, submitted for execution to the server thread pool), we can
    // set m_QueueDbBlock->allocated to false. Boolean write is atomic by
    // definition and test-and-set is executed from synchronized code in
    // CQueueDbBlockArray::Allocate.

    if (m_TruncateAtDetach && !m_Server->ShutdownRequested()) {
        CRef<CStdRequest> request(new CTruncateRequest(m_QueueDbBlock));
        m_Executor.SubmitRequest(request);
    } else {
        m_QueueDbBlock->allocated = false;
    }

    m_QueueDbBlock = 0;
}


void CQueue::SetParameters(const SQueueParameters &  params)
{
    // When modifying this, modify all places marked with PARAMETERS
    CFastMutexGuard     guard(m_ParamLock);

    m_Timeout    = params.timeout;
    m_RunTimeout = params.run_timeout;
    if (!m_RunTimeLine) {
        // One time only. Precision can not be reset.
        CNSPreciseTime  precision = params.CalculateRuntimePrecision();
        unsigned int    interval_sec = precision.Sec();
        if (interval_sec < 1)
            interval_sec = 1;
        m_RunTimeLine = new CJobTimeLine(interval_sec, 0);
    }

    m_ReadTimeout           = params.read_timeout;
    m_FailedRetries         = params.failed_retries;
    m_ReadFailedRetries     = params.read_failed_retries;
    m_BlacklistTime         = params.blacklist_time;
    m_ReadBlacklistTime     = params.read_blacklist_time;
    m_MaxInputSize          = params.max_input_size;
    m_MaxOutputSize         = params.max_output_size;
    m_WNodeTimeout          = params.wnode_timeout;
    m_ReaderTimeout         = params.reader_timeout;
    m_PendingTimeout        = params.pending_timeout;
    m_MaxPendingWaitTimeout = CNSPreciseTime(params.max_pending_wait_timeout);
    m_MaxPendingReadWaitTimeout =
                        CNSPreciseTime(params.max_pending_read_wait_timeout);
    m_NotifHifreqInterval   = params.notif_hifreq_interval;
    m_NotifHifreqPeriod     = params.notif_hifreq_period;
    m_NotifLofreqMult       = params.notif_lofreq_mult;
    m_HandicapTimeout       = CNSPreciseTime(params.notif_handicap);
    m_DumpBufferSize        = params.dump_buffer_size;
    m_DumpClientBufferSize  = params.dump_client_buffer_size;
    m_DumpAffBufferSize     = params.dump_aff_buffer_size;
    m_DumpGroupBufferSize   = params.dump_group_buffer_size;
    m_ScrambleJobKeys       = params.scramble_job_keys;
    m_LinkedSections        = params.linked_sections;

    m_ClientRegistryTimeoutWorkerNode =
                            params.client_registry_timeout_worker_node;
    m_ClientRegistryMinWorkerNodes = params.client_registry_min_worker_nodes;
    m_ClientRegistryTimeoutAdmin = params.client_registry_timeout_admin;
    m_ClientRegistryMinAdmins = params.client_registry_min_admins;
    m_ClientRegistryTimeoutSubmitter = params.client_registry_timeout_submitter;
    m_ClientRegistryMinSubmitters = params.client_registry_min_submitters;
    m_ClientRegistryTimeoutReader = params.client_registry_timeout_reader;
    m_ClientRegistryMinReaders = params.client_registry_min_readers;
    m_ClientRegistryTimeoutUnknown = params.client_registry_timeout_unknown;
    m_ClientRegistryMinUnknowns = params.client_registry_min_unknowns;

    m_ClientsRegistry.SetBlacklistTimeouts(m_BlacklistTime,
                                           m_ReadBlacklistTime);

    // program version control
    m_ProgramVersionList.Clear();
    if (!params.program_name.empty()) {
        m_ProgramVersionList.AddClientInfo(params.program_name);
    }
    m_SubmHosts.SetHosts(params.subm_hosts);
    m_WnodeHosts.SetHosts(params.wnode_hosts);
    m_ReaderHosts.SetHosts(params.reader_hosts);
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


void CQueue::GetMaxIOSizesAndLinkedSections(
                unsigned int &  max_input_size,
                unsigned int &  max_output_size,
                map< string, map<string, string> > & linked_sections) const
{
    CQueueParamAccessor     qp(*this);

    max_input_size = qp.GetMaxInputSize();
    max_output_size = qp.GetMaxOutputSize();
    GetLinkedSections(linked_sections);
}


void
CQueue::GetLinkedSections(map< string,
                               map<string, string> > &  linked_sections) const
{
    for (map<string, string>::const_iterator  k = m_LinkedSections.begin();
         k != m_LinkedSections.end(); ++k) {
        map< string, string >   values = m_QueueDB.GetLinkedSection(k->second);

        if (!values.empty())
            linked_sections[k->first] = values;
    }
}


// Used while loading the status matrix.
// The access to the DB is not protected here.
CNSPreciseTime  CQueue::x_GetSubmitTime(unsigned int  job_id)
{
    CBDB_FileCursor     events_cur(m_QueueDbBlock->events_db);
    events_cur.SetCondition(CBDB_FileCursor::eEQ);
    events_cur.From << job_id;

    // The submit event is always first
    if (events_cur.FetchFirst() == eBDB_Ok)
        return CNSPreciseTime(m_QueueDbBlock->events_db.timestamp_sec,
                              m_QueueDbBlock->events_db.timestamp_nsec);
    return kTimeZero;
}


// It is called only if there was no job for reading
bool CQueue::x_NoMoreReadJobs(const CNSClientId  &  client,
                              const TNSBitVector &  aff_ids,
                              bool                  reader_affinity,
                              bool                  any_affinity,
                              bool                  exclusive_new_affinity,
                              const string &        group,
                              bool                  affinity_may_change,
                              bool                  group_may_change)
{
    // This certain condition guarantees that there will be no job given
    if (!reader_affinity &&
        !aff_ids.any() &&
        !exclusive_new_affinity &&
        !any_affinity)
        return true;

    // Used only in the GetJobForReadingOrWait().
    // Operation lock has to be already taken.
    // Provides true if there are no more jobs for reading.
    vector<CNetScheduleAPI::EJobStatus>     from_state;
    TNSBitVector                            pending_running_jobs;
    TNSBitVector                            other_jobs;

    from_state.push_back(CNetScheduleAPI::ePending);
    from_state.push_back(CNetScheduleAPI::eRunning);
    pending_running_jobs = m_StatusTracker.GetJobs(from_state);

    from_state.clear();
    from_state.push_back(CNetScheduleAPI::eDone);
    from_state.push_back(CNetScheduleAPI::eFailed);
    from_state.push_back(CNetScheduleAPI::eCanceled);
    other_jobs = m_StatusTracker.GetJobs(from_state);

    // Remove those which have been read or in process of reading. This cannot
    // affect the pending and running jobs
    other_jobs -= m_ReadJobs;
    // Add those which are in a process of reading.
    // This needs to be done after '- m_ReadJobs' because that vector holds
    // both jobs which have been read and jobs which are in a process of
    // reading. When calculating 'no_more_jobs' the only already read jobs must
    // be excluded.
    TNSBitVector        reading_jobs =
                            m_StatusTracker.GetJobs(CNetScheduleAPI::eReading);
    if (!group.empty()) {
        // The pending and running jobs may change their group and or affinity
        // later via the RESCHEDULE command
        TNSBitVector                            group_jobs;
        group_jobs = m_GroupRegistry.GetJobs(group, false);
        if (!group_may_change)
            pending_running_jobs &= group_jobs;

        // The job group cannot be changed for the other job states
        other_jobs |= reading_jobs;
        other_jobs &= group_jobs;
    } else
        other_jobs |= reading_jobs;

    TNSBitVector    candidates = pending_running_jobs | other_jobs;
    if (!candidates.any())
        return true;


    // Deal with affinities
    // The weakest condition is if any affinity is suitable
    if (any_affinity)
        return !candidates.any();

    TNSBitVector        suitable_affinities;
    TNSBitVector        all_aff;
    TNSBitVector        all_pref_affs;
    TNSBitVector        all_aff_jobs;           // All jobs with an affinity
    TNSBitVector        no_aff_jobs;            // Jobs without any affinity

    all_aff = m_AffinityRegistry.GetRegisteredAffinities();
    all_pref_affs = m_ClientsRegistry.GetAllPreferredAffinities(eRead);
    all_aff_jobs = m_AffinityRegistry.GetJobsWithAffinities(all_aff);
    no_aff_jobs = candidates - all_aff_jobs;
    if (exclusive_new_affinity && no_aff_jobs.any())
        return false;

    if (exclusive_new_affinity)
        suitable_affinities = all_aff - all_pref_affs;
    if (reader_affinity)
        suitable_affinities |= m_ClientsRegistry.GetPreferredAffinities(client,
                                                                        eRead);
    suitable_affinities |= aff_ids;

    TNSBitVector    suitable_aff_jobs =
                        m_AffinityRegistry.GetJobsWithAffinities(
                                                        suitable_affinities);
    if (affinity_may_change)
        candidates = pending_running_jobs |
                     (other_jobs & suitable_aff_jobs);
    else
        candidates &= suitable_aff_jobs;
    return !candidates.any();
}


unsigned int  CQueue::LoadStatusMatrix(void)
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
        CNSPreciseTime  last_touch =
                            CNSPreciseTime(
                                    m_QueueDbBlock->job_db.last_touch_sec,
                                    m_QueueDbBlock->job_db.last_touch_nsec);
        CNSPreciseTime  job_timeout =
                            CNSPreciseTime(
                                    m_QueueDbBlock->job_db.timeout_sec,
                                    m_QueueDbBlock->job_db.timeout_nsec);
        CNSPreciseTime  job_run_timeout =
                            CNSPreciseTime(
                                    m_QueueDbBlock->job_db.run_timeout_sec,
                                    m_QueueDbBlock->job_db.run_timeout_nsec);
        CNSPreciseTime  job_read_timeout =
                            CNSPreciseTime(
                                    m_QueueDbBlock->job_db.read_timeout_sec,
                                    m_QueueDbBlock->job_db.read_timeout_nsec);
        TJobStatus      status = TJobStatus(static_cast<int>(
                                                m_QueueDbBlock->job_db.status));

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

        CNSPreciseTime      submit_time(0, 0);
        if (status == CNetScheduleAPI::ePending) {
            submit_time = x_GetSubmitTime(job_id);
            if (submit_time == kTimeZero)
                submit_time = CNSPreciseTime::Current();  // Be on a safe side
        }

        // Register the loaded job with the garbage collector
        // Pending jobs will be processed later
        CNSPreciseTime      precise_submit(submit_time);
        m_GCRegistry.RegisterJob(job_id, precise_submit, aff_id, group_id,
                                 GetJobExpirationTime(last_touch, status,
                                                      submit_time,
                                                      job_timeout,
                                                      job_run_timeout,
                                                      job_read_timeout,
                                                      m_Timeout,
                                                      m_RunTimeout,
                                                      m_ReadTimeout,
                                                      m_PendingTimeout,
                                                      kTimeZero));
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
void CQueue::x_LogSubmit(const CJob &  job)
{
    CDiagContext_Extra  extra = GetDiagContext().Extra()
        .Print("job_key", MakeJobKey(job.GetId()))
        .Print("queue", GetQueueName());

    extra.Flush();
}


unsigned int  CQueue::Submit(const CNSClientId &        client,
                             CJob &                     job,
                             const string &             aff_token,
                             const string &             group,
                             bool                       logging,
                             CNSRollbackInterface * &   rollback_action)
{
    // the only config parameter used here is the max input size so there is no
    // need to have a safe parameters accessor.

    if (job.GetInput().size() > m_MaxInputSize)
        NCBI_THROW(CNetScheduleException, eDataTooLong, "Input is too long");

    unsigned int    aff_id = 0;
    unsigned int    group_id = 0;
    CNSPreciseTime  op_begin_time = CNSPreciseTime::Current();
    unsigned int    job_id = GetNextId();
    CJobEvent &     event = job.AppendEvent();

    job.SetId(job_id);
    job.SetPassport(rand());
    job.SetLastTouch(op_begin_time);

    event.SetNodeAddr(client.GetAddress());
    event.SetStatus(CNetScheduleAPI::ePending);
    event.SetEvent(CJobEvent::eSubmit);
    event.SetTimestamp(op_begin_time);
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
                                                        job_id, 0, eUndefined);
                job.SetAffinityId(aff_id);
            }
            job.Flush(this);

            transaction.Commit();
        }}

        m_StatusTracker.AddPendingJob(job_id);

        // Register the job with the client
        m_ClientsRegistry.AddToSubmitted(client, 1);

        // Make the decision whether to send or not a notification
        if (m_PauseStatus == eNoPause)
            m_NotificationsList.Notify(job_id, aff_id,
                                       m_ClientsRegistry,
                                       m_AffinityRegistry,
                                       m_GroupRegistry,
                                       m_NotifHifreqPeriod,
                                       m_HandicapTimeout,
                                       eGet);

        m_GCRegistry.RegisterJob(job_id, op_begin_time,
                                 aff_id, group_id,
                                 job.GetExpirationTime(m_Timeout,
                                                       m_RunTimeout,
                                                       m_ReadTimeout,
                                                       m_PendingTimeout,
                                                       op_begin_time));
    }}

    rollback_action = new CNSSubmitRollback(client, job_id,
                                            op_begin_time,
                                            CNSPreciseTime::Current());

    m_StatisticsCounters.CountSubmit(1);
    if (logging)
        x_LogSubmit(job);

    return job_id;
}


unsigned int
CQueue::SubmitBatch(const CNSClientId &             client,
                    vector< pair<CJob, string> > &  batch,
                    const string &                  group,
                    bool                            logging,
                    CNSRollbackInterface * &        rollback_action)
{
    unsigned int    batch_size = batch.size();
    unsigned int    job_id = GetNextJobIdForBatch(batch_size);
    TNSBitVector    affinities;
    CNSPreciseTime  curr_time = CNSPreciseTime::Current();

    {{
        unsigned int        job_id_cnt = job_id;
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
                                                                 0,
                                                                 eUndefined);

                    job.SetAffinityId(aff_id);
                    affinities.set_bit(aff_id);
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

        if (m_PauseStatus == eNoPause)
            m_NotificationsList.Notify(jobs, affinities, no_aff_jobs,
                                       m_ClientsRegistry,
                                       m_AffinityRegistry,
                                       m_GroupRegistry,
                                       m_NotifHifreqPeriod,
                                       m_HandicapTimeout,
                                       eGet);

        for (size_t  k = 0; k < batch_size; ++k) {
            m_GCRegistry.RegisterJob(
                        batch[k].first.GetId(), curr_time,
                        batch[k].first.GetAffinityId(), group_id,
                        batch[k].first.GetExpirationTime(m_Timeout,
                                                         m_RunTimeout,
                                                         m_ReadTimeout,
                                                         m_PendingTimeout,
                                                         curr_time));
        }
    }}

    m_StatisticsCounters.CountSubmit(batch_size);
    if (m_LogBatchEachJob && logging)
        for (size_t  k = 0; k < batch_size; ++k)
            x_LogSubmit(batch[k].first);

    rollback_action = new CNSBatchSubmitRollback(client, job_id, batch_size);
    return job_id;
}


TJobStatus  CQueue::PutResult(const CNSClientId &     client,
                              const CNSPreciseTime &  curr,
                              unsigned int            job_id,
                              const string &          job_key,
                              CJob &                  job,
                              const string &          auth_token,
                              int                     ret_code,
                              const string &          output)
{
    // The only one parameter (max output size) is required for the put
    // operation so there is no need to use CQueueParamAccessor

    if (output.size() > m_MaxOutputSize)
        NCBI_THROW(CNetScheduleException, eDataTooLong,
                   "Output is too long");

    CFastMutexGuard     guard(m_OperationLock);
    TJobStatus          old_status = GetJobStatus(job_id);

    if (old_status == CNetScheduleAPI::eDone) {
        m_StatisticsCounters.CountTransition(CNetScheduleAPI::eDone,
                                             CNetScheduleAPI::eDone);
        return old_status;
    }

    if (old_status != CNetScheduleAPI::ePending &&
        old_status != CNetScheduleAPI::eRunning &&
        old_status != CNetScheduleAPI::eFailed)
        return old_status;

    {{
        CNSTransaction      transaction(this);
        x_UpdateDB_PutResultNoLock(job_id, auth_token, curr,
                                   ret_code, output, job,
                                   client);
        transaction.Commit();
    }}

    m_StatusTracker.SetStatus(job_id, CNetScheduleAPI::eDone);
    m_StatisticsCounters.CountTransition(old_status,
                                         CNetScheduleAPI::eDone);
    g_DoPerfLogging(*this, job, 200);
    m_ClientsRegistry.UnregisterJob(job_id, eGet);

    m_GCRegistry.UpdateLifetime(job_id,
                                job.GetExpirationTime(m_Timeout,
                                                      m_RunTimeout,
                                                      m_ReadTimeout,
                                                      m_PendingTimeout,
                                                      curr));

    TimeLineRemove(job_id);

    if (job.ShouldNotifySubmitter(curr))
        m_NotificationsList.NotifyJobStatus(job.GetSubmAddr(),
                                            job.GetSubmNotifPort(),
                                            job_key,
                                            job.GetStatus(),
                                            job.GetLastEventIndex());
    if (job.ShouldNotifyListener(curr, m_JobsToNotify))
        m_NotificationsList.NotifyJobStatus(job.GetListenerNotifAddr(),
                                            job.GetListenerNotifPort(),
                                            job_key,
                                            job.GetStatus(),
                                            job.GetLastEventIndex());

    // Notify the readers if the job has not been given for reading yet
    if (!m_ReadJobs.get_bit(job_id)) {
        m_GCRegistry.UpdateReadVacantTime(job_id, curr);
        m_NotificationsList.Notify(job_id, job.GetAffinityId(),
                                   m_ClientsRegistry,
                                   m_AffinityRegistry,
                                   m_GroupRegistry,
                                   m_NotifHifreqPeriod,
                                   m_HandicapTimeout,
                                   eRead);
    }
    return old_status;
}


bool
CQueue::GetJobOrWait(const CNSClientId &       client,
                     unsigned short            port, // Port the client
                                                     // will wait on
                     unsigned int              timeout, // If timeout != 0 =>
                                                        // WGET
                     const CNSPreciseTime &    curr,
                     const list<string> *      aff_list,
                     bool                      wnode_affinity,
                     bool                      any_affinity,
                     bool                      exclusive_new_affinity,
                     bool                      prioritized_aff,
                     bool                      new_format,
                     const string &            group,
                     CJob *                    new_job,
                     CNSRollbackInterface * &  rollback_action,
                     string &                  added_pref_aff)
{
    // We need exactly 1 parameter - m_RunTimeout, so we can access it without
    // CQueueParamAccessor

    // This is a worker node command, so mark the node type as a worker
    // node
    m_ClientsRegistry.AppendType(client, CNSClient::eWorkerNode);

    vector<unsigned int>    aff_ids;
    TNSBitVector            aff_ids_vector;

    {{
        CFastMutexGuard     guard(m_OperationLock);

        if (wnode_affinity) {
            // Check that the preferred affinities were not reset
            if (m_ClientsRegistry.GetAffinityReset(client, eGet))
                return false;

            // Check that the client was garbage collected with preferred affs
            if (m_ClientsRegistry.WasGarbageCollected(client, eGet))
                return false;
        }

        // Resolve affinities and groups. It is supposed that the client knows
        // better what affinities and groups to expect i.e. even if they do not
        // exist yet, they may appear soon.
        {{
            CNSTransaction      transaction(this);

            m_GroupRegistry.ResolveGroup(group);
            aff_ids = m_AffinityRegistry.ResolveAffinities(*aff_list);
            transaction.Commit();
        }}

        x_UnregisterGetListener(client, port);
    }}

    for (vector<unsigned int>::const_iterator k = aff_ids.begin();
            k != aff_ids.end(); ++k)
        aff_ids_vector.set(*k, true);

    for (;;) {
        // No lock here to make it possible to pick a job
        // simultaneously from many threads
        x_SJobPick  job_pick = x_FindVacantJob(client, aff_ids,
                                               wnode_affinity,
                                               any_affinity,
                                               exclusive_new_affinity,
                                               prioritized_aff,
                                               group, eGet);
        {{
            bool                outdated_job = false;
            CFastMutexGuard     guard(m_OperationLock);

            if (job_pick.job_id == 0) {
                if (exclusive_new_affinity)
                    // Second try only if exclusive new aff is set on
                    job_pick = x_FindOutdatedPendingJob(client, 0);

                if (job_pick.job_id == 0) {
                    if (timeout != 0 && port > 0)
                        // WGET: // There is no job, so the client might need to
                        // be registered in the waiting list
                        x_RegisterGetListener( client, port, timeout,
                                               aff_ids_vector,
                                               wnode_affinity, any_affinity,
                                               exclusive_new_affinity,
                                               new_format, group);
                    return true;
                }
                outdated_job = true;
            } else {
                // Check that the job is still Pending; it could be
                // grabbed by another WN or GC
                if (GetJobStatus(job_pick.job_id) != CNetScheduleAPI::ePending)
                    continue;   // Try to pick a job again

                if (exclusive_new_affinity) {
                    if (m_GCRegistry.IsOutdatedJob(
                                    job_pick.job_id, eGet,
                                    m_MaxPendingWaitTimeout) == false) {
                        x_SJobPick  outdated_pick =
                                        x_FindOutdatedPendingJob(
                                                    client, job_pick.job_id);
                        if (outdated_pick.job_id != 0) {
                            job_pick = outdated_pick;
                            outdated_job = true;
                        }
                    }
                }
            }

            // The job is still pending, check if it was received as
            // with exclusive affinity
            if (job_pick.exclusive && job_pick.aff_id != 0 &&
                outdated_job == false) {
                if (m_ClientsRegistry.IsPreferredByAny(
                                                job_pick.aff_id, eGet))
                    continue;  // Other WN grabbed this affinity already
                bool added = m_ClientsRegistry.
                                UpdatePreferredAffinities(
                                    client, job_pick.aff_id, 0, eGet);
                if (added)
                    added_pref_aff = m_AffinityRegistry.GetTokenByID(
                                                    job_pick.aff_id);
            }
            if (outdated_job && job_pick.aff_id != 0) {
                bool added = m_ClientsRegistry.
                                UpdatePreferredAffinities(
                                    client, job_pick.aff_id, 0, eGet);
                if (added)
                    added_pref_aff = m_AffinityRegistry.GetTokenByID(
                                                    job_pick.aff_id);
            }

            x_UpdateDB_ProvideJobNoLock(client, curr, job_pick.job_id, eGet,
                                        *new_job);
            m_StatusTracker.SetStatus(job_pick.job_id,
                                      CNetScheduleAPI::eRunning);

            m_StatisticsCounters.CountTransition(
                                        CNetScheduleAPI::ePending,
                                        CNetScheduleAPI::eRunning);
            g_DoPerfLogging(*this, *new_job, 200);
            if (outdated_job)
                m_StatisticsCounters.CountOutdatedPick(eGet);

            m_GCRegistry.UpdateLifetime(
                        job_pick.job_id,
                        new_job->GetExpirationTime(m_Timeout,
                                                   m_RunTimeout,
                                                   m_ReadTimeout,
                                                   m_PendingTimeout,
                                                   curr));
            TimeLineAdd(job_pick.job_id, curr + m_RunTimeout);
            m_ClientsRegistry.RegisterJob(client, job_pick.job_id, eGet);

            if (new_job->ShouldNotifyListener(curr, m_JobsToNotify))
                m_NotificationsList.NotifyJobStatus(
                                new_job->GetListenerNotifAddr(),
                                new_job->GetListenerNotifPort(),
                                MakeJobKey(job_pick.job_id),
                                new_job->GetStatus(),
                                new_job->GetLastEventIndex());

            // If there are no more pending jobs, let's clear the
            // list of delayed exact notifications.
            if (!m_StatusTracker.AnyPending())
                m_NotificationsList.ClearExactGetNotifications();

            rollback_action = new CNSGetJobRollback(client, job_pick.job_id);
            return true;
        }}
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
        ERR_POST(Warning << "Attempt to cancel WGET for the client "
                            "which does not wait anything (node: "
                         << client.GetNode() << " session: "
                         << client.GetSession() << ")");
}


void  CQueue::CancelWaitRead(const CNSClientId &  client)
{
    bool    result;

    {{
        CFastMutexGuard     guard(m_OperationLock);

        result = m_ClientsRegistry.CancelWaiting(client, eRead);
    }}

    if (result == false)
        ERR_POST(Warning << "Attempt to cancel waiting READ for the client "
                            "which does not wait anything (node: "
                         << client.GetNode() << " session: "
                         << client.GetSession() << ")");
}


list<string>
CQueue::ChangeAffinity(const CNSClientId &     client,
                       const list<string> &    aff_to_add,
                       const list<string> &    aff_to_del,
                       ECommandGroup           cmd_group)
{
    // It is guaranteed here that the client is a new style one.
    // I.e. it has both client_node and client_session.
    if (cmd_group == eGet)
        m_ClientsRegistry.AppendType(client, CNSClient::eWorkerNode);
    else
        m_ClientsRegistry.AppendType(client, CNSClient::eReader);


    list<string>    msgs;   // Warning messages for the socket
    unsigned int    client_id = client.GetID();
    TNSBitVector    current_affinities =
                         m_ClientsRegistry.GetPreferredAffinities(client,
                                                                  cmd_group);
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
            ERR_POST(Warning << "Client '" << client.GetNode()
                             << "' deletes unknown affinity '"
                             << *k << "'. Ignored.");
            msgs.push_back("eAffinityNotFound:"
                           "unknown affinity to delete: " + *k);
            continue;
        }

        if (!current_affinities.get_bit(aff_id)) {
            // This a try to delete something which has not been added or
            // deleted before.
            ERR_POST(Warning << "Client '" << client.GetNode()
                             << "' deletes affinity '" << *k
                             << "' which is not in the list of the "
                                "preferred client affinities. Ignored.");
            msgs.push_back("eAffinityNotPreferred:not registered affinity "
                           "to delete: " + *k);
            continue;
        }

        // The affinity will really be deleted
        aff_id_to_del.set_bit(aff_id);
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
                   NStr::NumericToString(m_MaxAffinities) +
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
                                                                 0, client_id,
                                                                 cmd_group);

            if (current_affinities.get_bit(aff_id)) {
                already_added_affinities.push_back(*k);
                continue;
            }

            aff_id_to_add.set_bit(aff_id);
            any_to_add = true;
        }

        transaction.Commit();
    }}

    // Log the warnings and add it to the warning message
    for (vector<string>::const_iterator  j(already_added_affinities.begin());
         j != already_added_affinities.end(); ++j) {
        // That was a try to add something which has already been added
        ERR_POST(Warning << "Client '" << client.GetNode()
                         << "' adds affinity '" << *j
                         << "' which is already in the list of the "
                            "preferred client affinities. Ignored.");
        msgs.push_back("eAffinityAlreadyPreferred:already registered "
                       "affinity to add: " + *j);
    }

    if (any_to_add || any_to_del)
        m_ClientsRegistry.UpdatePreferredAffinities(client,
                                                    aff_id_to_add,
                                                    aff_id_to_del,
                                                    cmd_group);

    if (m_ClientsRegistry.WasGarbageCollected(client, cmd_group)) {
        ERR_POST(Warning << "Client '" << client.GetNode()
                         << "' has been garbage collected and tries to "
                            "update its preferred affinities.");
        msgs.push_back("eClientGarbageCollected:the client had been "
                       "garbage collected");
    }
    return msgs;
}


void  CQueue::SetAffinity(const CNSClientId &     client,
                          const list<string> &    aff,
                          ECommandGroup           cmd_group)
{
    if (cmd_group == eGet)
        m_ClientsRegistry.AppendType(client, CNSClient::eWorkerNode);
    else
        m_ClientsRegistry.AppendType(client, CNSClient::eReader);

    if (aff.size() > m_MaxAffinities) {
        NCBI_THROW(CNetScheduleException, eTooManyPreferredAffinities,
                   "The client '" + client.GetNode() +
                   "' exceeds the limit (" +
                   NStr::NumericToString(m_MaxAffinities) +
                   ") of the preferred affinities. Set request ignored.");
    }

    unsigned int    client_id = client.GetID();
    TNSBitVector    aff_id_to_set;
    TNSBitVector    already_added_aff_id;


    TNSBitVector    current_affinities =
                         m_ClientsRegistry.GetPreferredAffinities(client,
                                                                  cmd_group);
    {{
        CFastMutexGuard     guard(m_OperationLock);
        CNSTransaction      transaction(this);

        // Convert the aff to the affinity IDs
        for (list<string>::const_iterator  k(aff.begin());
             k != aff.end(); ++k ) {
            unsigned int    aff_id =
                                 m_AffinityRegistry.ResolveAffinityToken(*k,
                                                                 0, client_id,
                                                                 cmd_group);

            if (current_affinities.get_bit(aff_id))
                already_added_aff_id.set_bit(aff_id);

            aff_id_to_set.set_bit(aff_id);
        }

        transaction.Commit();
    }}

    m_ClientsRegistry.SetPreferredAffinities(client, aff_id_to_set, cmd_group);
}


int CQueue::SetClientData(const CNSClientId &  client,
                          const string &  data, int  data_version)
{
    return m_ClientsRegistry.SetClientData(client, data, data_version);
}


TJobStatus  CQueue::JobDelayExpiration(unsigned int            job_id,
                                       CJob &                  job,
                                       const CNSPreciseTime &  tm)
{
    CNSPreciseTime      queue_run_timeout = GetRunTimeout();
    CNSPreciseTime      curr = CNSPreciseTime::Current();

    {{
        CFastMutexGuard     guard(m_OperationLock);
        TJobStatus          status = GetJobStatus(job_id);

        if (status != CNetScheduleAPI::eRunning)
            return status;

        CNSPreciseTime  time_start = kTimeZero;
        CNSPreciseTime  run_timeout = kTimeZero;
        {{
            CNSTransaction      transaction(this);

            if (job.Fetch(this, job_id) != CJob::eJF_Ok)
                return CNetScheduleAPI::eJobNotFound;

            time_start = job.GetLastEvent()->GetTimestamp();
            run_timeout = job.GetRunTimeout();
            if (run_timeout == kTimeZero)
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
        CNSPreciseTime      exp_time = kTimeZero;
        if (run_timeout != kTimeZero)
            exp_time = time_start + run_timeout;

        TimeLineMove(job_id, exp_time, curr + tm);
    }}

    return CNetScheduleAPI::eRunning;
}


TJobStatus  CQueue::JobDelayReadExpiration(unsigned int            job_id,
                                           CJob &                  job,
                                           const CNSPreciseTime &  tm)
{
    CNSPreciseTime      queue_read_timeout = GetReadTimeout();
    CNSPreciseTime      curr = CNSPreciseTime::Current();

    {{
        CFastMutexGuard     guard(m_OperationLock);
        TJobStatus          status = GetJobStatus(job_id);

        if (status != CNetScheduleAPI::eReading)
            return status;

        CNSPreciseTime  time_start = kTimeZero;
        CNSPreciseTime  read_timeout = kTimeZero;
        {{
            CNSTransaction      transaction(this);

            if (job.Fetch(this, job_id) != CJob::eJF_Ok)
                return CNetScheduleAPI::eJobNotFound;

            time_start = job.GetLastEvent()->GetTimestamp();
            read_timeout = job.GetReadTimeout();
            if (read_timeout == kTimeZero)
                read_timeout = queue_read_timeout;

            if (time_start + read_timeout > curr + tm)
                return CNetScheduleAPI::eReading;   // Old timeout is enough to
                                                    // cover this request, so
                                                    // keep it.

            job.SetReadTimeout(curr + tm - time_start);
            job.SetLastTouch(curr);
            job.Flush(this);
            transaction.Commit();
        }}

        // No need to update the GC registry because the running (and reading)
        // jobs are skipped by GC
        CNSPreciseTime      exp_time = kTimeZero;
        if (read_timeout != kTimeZero)
            exp_time = time_start + read_timeout;

        TimeLineMove(job_id, exp_time, curr + tm);
    }}

    return CNetScheduleAPI::eReading;
}



TJobStatus  CQueue::GetStatusAndLifetime(unsigned int      job_id,
                                         CJob &            job,
                                         bool              need_touch,
                                         CNSPreciseTime *  lifetime)
{
    CFastMutexGuard     guard(m_OperationLock);
    TJobStatus          status = GetJobStatus(job_id);

    if (status == CNetScheduleAPI::eJobNotFound)
        return status;

    CNSPreciseTime      curr = CNSPreciseTime::Current();

    {{
        CNSTransaction      transaction(this);

        if (job.Fetch(this, job_id) != CJob::eJF_Ok)
            NCBI_THROW(CNetScheduleException, eInternalError,
                       "Error fetching job");

        if (need_touch) {
            job.SetLastTouch(curr);
            job.Flush(this);
            transaction.Commit();
        }
    }}

    if (need_touch) {
        m_GCRegistry.UpdateLifetime(job_id,
                                    job.GetExpirationTime(m_Timeout,
                                                          m_RunTimeout,
                                                          m_ReadTimeout,
                                                          m_PendingTimeout,
                                                          curr));
    }

    *lifetime = x_GetEstimatedJobLifetime(job_id, status);
    return status;
}


TJobStatus  CQueue::SetJobListener(unsigned int            job_id,
                                   CJob &                  job,
                                   unsigned int            address,
                                   unsigned short          port,
                                   const CNSPreciseTime &  timeout,
                                   size_t *                last_event_index)
{
    CNSPreciseTime      curr = CNSPreciseTime::Current();
    TJobStatus          status = CNetScheduleAPI::eJobNotFound;
    CFastMutexGuard     guard(m_OperationLock);
    bool                switched_on = true;

    {{
        CNSTransaction      transaction(this);

        if (job.Fetch(this, job_id) != CJob::eJF_Ok)
            return status;

        *last_event_index = job.GetLastEventIndex();
        status = job.GetStatus();
        if (status == CNetScheduleAPI::eCanceled)
            return status;  // Listener notifications are valid for all states
                            // except of Cancel - there are no transitions from
                            // Cancel.

        if (address == 0 || port == 0 || timeout == kTimeZero) {
            // If at least one of the values is 0 => no notifications
            // So to make the job properly dumped put zeros everywhere.
            job.SetListenerNotifAddr(0);
            job.SetListenerNotifPort(0);
            job.SetListenerNotifAbsTime(kTimeZero);
            switched_on = false;
        } else {
            job.SetListenerNotifAddr(address);
            job.SetListenerNotifPort(port);
            job.SetListenerNotifAbsTime(curr + timeout);
        }

        job.SetLastTouch(curr);
        job.Flush(this);
        transaction.Commit();
    }}

    m_JobsToNotify.set_bit(job_id, switched_on);

    return status;
}


bool CQueue::PutProgressMessage(unsigned int    job_id,
                                CJob &          job,
                                const string &  msg)
{
    CNSPreciseTime      curr = CNSPreciseTime::Current();

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
                                                          m_ReadTimeout,
                                                          m_PendingTimeout,
                                                          curr));
    }}

    return true;
}


TJobStatus  CQueue::ReturnJob(const CNSClientId &     client,
                              unsigned int            job_id,
                              const string &          job_key,
                              CJob &                  job,
                              const string &          auth_token,
                              string &                warning,
                              TJobReturnOption        how)
{
    CNSPreciseTime      current_time = CNSPreciseTime::Current();
    CFastMutexGuard     guard(m_OperationLock);
    TJobStatus          old_status = GetJobStatus(job_id);

    if (old_status != CNetScheduleAPI::eRunning)
        return old_status;

    {{
         CNSTransaction      transaction(this);

        if (job.Fetch(this, job_id) != CJob::eJF_Ok)
            NCBI_THROW(CNetScheduleException, eInternalError,
                       "Error fetching job");

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
                ERR_POST(Warning << "Received RETURN2 with only "
                                    "passport matched.");
                warning = "eJobPassportOnlyMatch:Only job passport matched. "
                          "Command is ignored.";
                return old_status;
            }
            // Here: the authorization token is OK, we can continue
        }


        unsigned int    run_count = job.GetRunCount();
        CJobEvent *     event = job.GetLastEvent();

        if (!event)
            ERR_POST("No JobEvent for running job");

        event = &job.AppendEvent();
        event->SetNodeAddr(client.GetAddress());
        event->SetStatus(CNetScheduleAPI::ePending);
        switch (how) {
            case eWithBlacklist:
                event->SetEvent(CJobEvent::eReturn);
                break;
            case eWithoutBlacklist:
                event->SetEvent(CJobEvent::eReturnNoBlacklist);
                break;
            case eRollback:
                event->SetEvent(CJobEvent::eNSGetRollback);
                break;
        }
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
    switch (how) {
        case eWithBlacklist:
            m_StatisticsCounters.CountTransition(old_status,
                                                 CNetScheduleAPI::ePending);
            break;
        case eWithoutBlacklist:
            m_StatisticsCounters.CountToPendingWithoutBlacklist(1);
            break;
        case eRollback:
            m_StatisticsCounters.CountNSGetRollback(1);
            break;
    }
    g_DoPerfLogging(*this, job, 200);
    TimeLineRemove(job_id);
    m_ClientsRegistry.UnregisterJob(job_id, eGet);
    if (how == eWithBlacklist)
        m_ClientsRegistry.RegisterBlacklistedJob(client, job_id, eGet);
    m_GCRegistry.UpdateLifetime(job_id,
                                job.GetExpirationTime(m_Timeout,
                                                      m_RunTimeout,
                                                      m_ReadTimeout,
                                                      m_PendingTimeout,
                                                      current_time));

    if (job.ShouldNotifyListener(current_time, m_JobsToNotify))
        m_NotificationsList.NotifyJobStatus(job.GetListenerNotifAddr(),
                                            job.GetListenerNotifPort(),
                                            job_key,
                                            job.GetStatus(),
                                            job.GetLastEventIndex());

    if (m_PauseStatus == eNoPause)
        m_NotificationsList.Notify(job_id,
                                   job.GetAffinityId(), m_ClientsRegistry,
                                   m_AffinityRegistry, m_GroupRegistry,
                                   m_NotifHifreqPeriod, m_HandicapTimeout,
                                   eGet);

    return old_status;
}


TJobStatus  CQueue::RescheduleJob(const CNSClientId &     client,
                                  unsigned int            job_id,
                                  const string &          job_key,
                                  const string &          auth_token,
                                  const string &          aff_token,
                                  const string &          group,
                                  bool &                  auth_token_ok,
                                  CJob &                  job)
{
    CNSPreciseTime      current_time = CNSPreciseTime::Current();
    CFastMutexGuard     guard(m_OperationLock);
    TJobStatus          old_status = GetJobStatus(job_id);
    unsigned int        affinity_id = 0;
    unsigned int        group_id = 0;
    unsigned int        job_affinity_id;
    unsigned int        job_group_id;

    if (old_status != CNetScheduleAPI::eRunning)
        return old_status;

    // Resolve affinity and group in a separate transaction
    if (!aff_token.empty() || !group.empty()) {
        {{
            CNSTransaction      transaction(this);
            if (!aff_token.empty())
                affinity_id = m_AffinityRegistry.ResolveAffinity(aff_token);
            if (!group.empty())
                group_id = m_GroupRegistry.ResolveGroup(group);
            transaction.Commit();
        }}
    }

    {{
         CNSTransaction      transaction(this);

        if (job.Fetch(this, job_id) != CJob::eJF_Ok)
            NCBI_THROW(CNetScheduleException, eInternalError,
                       "Error fetching job");

        // Need to check authorization token first
        CJob::EAuthTokenCompareResult   token_compare_result =
                                            job.CompareAuthToken(auth_token);

        if (token_compare_result == CJob::eInvalidTokenFormat)
            NCBI_THROW(CNetScheduleException, eInvalidAuthToken,
                       "Invalid authorization token format");

        if (token_compare_result != CJob::eCompleteMatch) {
            auth_token_ok = false;
            return old_status;
        }

        // Here: the authorization token is OK, we can continue
        auth_token_ok = true;

        // Memorize the job group and affinity for the proper updates after
        // the transaction is finished
        job_affinity_id = job.GetAffinityId();
        job_group_id = job.GetGroupId();

        // Update the job affinity and group
        job.SetAffinityId(affinity_id);
        job.SetGroupId(group_id);

        unsigned int    run_count = job.GetRunCount();
        CJobEvent *     event = job.GetLastEvent();

        if (!event)
            ERR_POST("No JobEvent for running job");

        event = &job.AppendEvent();
        event->SetNodeAddr(client.GetAddress());
        event->SetStatus(CNetScheduleAPI::ePending);
        event->SetEvent(CJobEvent::eReschedule);
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

    // Job has been updated in the DB. Update the affinity and group
    // registries as needed.
    if (job_affinity_id != affinity_id) {
        if (job_affinity_id != 0)
            m_AffinityRegistry.RemoveJobFromAffinity(job_id, job_affinity_id);
        if (affinity_id != 0)
            m_AffinityRegistry.AddJobToAffinity(job_id, affinity_id);
    }
    if (job_group_id != group_id) {
        if (job_group_id != 0)
            m_GroupRegistry.RemoveJob(job_group_id, job_id);
        if (group_id != 0)
            m_GroupRegistry.AddJob(group_id, job_id);
    }
    if (job_affinity_id != affinity_id || job_group_id != group_id)
        m_GCRegistry.ChangeAffinityAndGroup(job_id, affinity_id, group_id);

    m_StatusTracker.SetStatus(job_id, CNetScheduleAPI::ePending);
    m_StatisticsCounters.CountToPendingRescheduled(1);
    g_DoPerfLogging(*this, job, 200);

    TimeLineRemove(job_id);
    m_ClientsRegistry.UnregisterJob(job_id, eGet);
    m_GCRegistry.UpdateLifetime(job_id,
                                job.GetExpirationTime(m_Timeout,
                                                      m_RunTimeout,
                                                      m_ReadTimeout,
                                                      m_PendingTimeout,
                                                      current_time));

    if (job.ShouldNotifyListener(current_time, m_JobsToNotify))
        m_NotificationsList.NotifyJobStatus(job.GetListenerNotifAddr(),
                                            job.GetListenerNotifPort(),
                                            job_key,
                                            job.GetStatus(),
                                            job.GetLastEventIndex());

    if (m_PauseStatus == eNoPause)
        m_NotificationsList.Notify(job_id,
                                   job.GetAffinityId(), m_ClientsRegistry,
                                   m_AffinityRegistry, m_GroupRegistry,
                                   m_NotifHifreqPeriod, m_HandicapTimeout,
                                   eGet);
    return old_status;
}


TJobStatus  CQueue::ReadAndTouchJob(unsigned int      job_id,
                                    CJob &            job,
                                    CNSPreciseTime *  lifetime)
{
    CFastMutexGuard         guard(m_OperationLock);
    TJobStatus              status = GetJobStatus(job_id);

    if (status == CNetScheduleAPI::eJobNotFound)
        return status;

    CNSPreciseTime          curr = CNSPreciseTime::Current();
    {{
        CNSTransaction      transaction(this);

        if (job.Fetch(this, job_id) != CJob::eJF_Ok)
            NCBI_THROW(CNetScheduleException, eInternalError,
                       "Error fetching job");

        job.SetLastTouch(curr);
        job.Flush(this);
        transaction.Commit();
    }}

    m_GCRegistry.UpdateLifetime(job_id,
                                job.GetExpirationTime(m_Timeout,
                                                      m_RunTimeout,
                                                      m_ReadTimeout,
                                                      m_PendingTimeout,
                                                      curr));
    *lifetime = x_GetEstimatedJobLifetime(job_id, status);
    return status;
}


// Deletes all the jobs from the queue
void CQueue::Truncate(bool  logging)
{
    CJob                                job;
    vector<CNetScheduleAPI::EJobStatus> statuses;
    TNSBitVector                        jobs_to_notify;
    CNSPreciseTime                      current_time =
                                                CNSPreciseTime::Current();

    // Jobs in certain states
    TNSBitVector                        bvPending;
    TNSBitVector                        bvRunning;
    TNSBitVector                        bvCanceled;
    TNSBitVector                        bvFailed;
    TNSBitVector                        bvDone;
    TNSBitVector                        bvReading;
    TNSBitVector                        bvConfirmed;
    TNSBitVector                        bvReadFailed;


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
                    ERR_POST("Cannot fetch job " << DecorateJob(job_id) <<
                             " while dropping all jobs in the queue");
                    continue;
                }

                if (job.ShouldNotifySubmitter(current_time))
                    m_NotificationsList.NotifyJobStatus(
                                                job.GetSubmAddr(),
                                                job.GetSubmNotifPort(),
                                                MakeJobKey(job_id),
                                                job.GetStatus(),
                                                job.GetLastEventIndex());

                if (logging)
                    GetDiagContext().Extra().Print("job_key",
                                                   MakeJobKey(job_id))
                                            .Print("job_phid",
                                                   job.GetNCBIPHID());
            }

            // There is no need to commit transaction - there were no changes
        }}

        m_StatusTracker.ClearStatus(CNetScheduleAPI::ePending, &bvPending);
        m_StatusTracker.ClearStatus(CNetScheduleAPI::eRunning, &bvRunning);
        m_StatusTracker.ClearStatus(CNetScheduleAPI::eCanceled, &bvCanceled);
        m_StatusTracker.ClearStatus(CNetScheduleAPI::eFailed, &bvFailed);
        m_StatusTracker.ClearStatus(CNetScheduleAPI::eDone, &bvDone);
        m_StatusTracker.ClearStatus(CNetScheduleAPI::eReading, &bvReading);
        m_StatusTracker.ClearStatus(CNetScheduleAPI::eConfirmed, &bvConfirmed);
        m_StatusTracker.ClearStatus(CNetScheduleAPI::eReadFailed, &bvReadFailed);

        m_AffinityRegistry.ClearMemoryAndDatabase();
        m_GroupRegistry.ClearMemoryAndDatabase();

        CWriteLockGuard     rtl_guard(m_RunTimeLineLock);
        m_RunTimeLine->ReInit(0);
    }}

    if (bvPending.count() > 0)
        x_Erase(bvPending, CNetScheduleAPI::ePending);
    if (bvRunning.count() > 0)
        x_Erase(bvRunning, CNetScheduleAPI::eRunning);
    if (bvCanceled.count() > 0)
        x_Erase(bvCanceled, CNetScheduleAPI::eCanceled);
    if (bvDone.count() > 0)
        x_Erase(bvFailed, CNetScheduleAPI::eFailed);
    if (bvDone.count() > 0)
        x_Erase(bvDone, CNetScheduleAPI::eDone);
    if (bvReading.count() > 0)
        x_Erase(bvReading, CNetScheduleAPI::eReading);
    if (bvConfirmed.count() > 0)
        x_Erase(bvConfirmed, CNetScheduleAPI::eConfirmed);
    if (bvReadFailed.count() > 0)
        x_Erase(bvReadFailed, CNetScheduleAPI::eReadFailed);
}


TJobStatus  CQueue::Cancel(const CNSClientId &  client,
                           unsigned int         job_id,
                           const string &       job_key,
                           CJob &               job,
                           bool                 is_ns_rollback)
{
    TJobStatus          old_status;
    CNSPreciseTime      current_time = CNSPreciseTime::Current();

    {{
        CFastMutexGuard     guard(m_OperationLock);

        old_status = m_StatusTracker.GetStatus(job_id);
        if (old_status == CNetScheduleAPI::eJobNotFound)
            return CNetScheduleAPI::eJobNotFound;
        if (old_status == CNetScheduleAPI::eCanceled) {
            if (is_ns_rollback)
                m_StatisticsCounters.CountNSSubmitRollback(1);
            else
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
            if (is_ns_rollback)
                event->SetEvent(CJobEvent::eNSSubmitRollback);
            else
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
        if (is_ns_rollback) {
            m_StatisticsCounters.CountNSSubmitRollback(1);
        } else {
            m_StatisticsCounters.CountTransition(old_status,
                                                 CNetScheduleAPI::eCanceled);
            g_DoPerfLogging(*this, job, 200);
        }

        TimeLineRemove(job_id);
        if (old_status == CNetScheduleAPI::eRunning)
            m_ClientsRegistry.UnregisterJob(job_id, eGet);
        else if (old_status == CNetScheduleAPI::eReading)
            m_ClientsRegistry.UnregisterJob(job_id, eRead);

        m_GCRegistry.UpdateLifetime(job_id,
                                    job.GetExpirationTime(m_Timeout,
                                                          m_RunTimeout,
                                                          m_ReadTimeout,
                                                          m_PendingTimeout,
                                                          current_time));

        if (job.ShouldNotifyListener(current_time, m_JobsToNotify))
            m_NotificationsList.NotifyJobStatus(job.GetListenerNotifAddr(),
                                                job.GetListenerNotifPort(),
                                                job_key,
                                                job.GetStatus(),
                                                job.GetLastEventIndex());

        // Notify the readers if the job has not been given for reading yet
        // and it was not a rollback due to a socket write error
        if (!m_ReadJobs.get_bit(job_id) && is_ns_rollback == false) {
            m_GCRegistry.UpdateReadVacantTime(job_id, current_time);
            m_NotificationsList.Notify(job_id, job.GetAffinityId(),
                                       m_ClientsRegistry,
                                       m_AffinityRegistry,
                                       m_GroupRegistry,
                                       m_NotifHifreqPeriod,
                                       m_HandicapTimeout,
                                       eRead);
        }
    }}

    if ((old_status == CNetScheduleAPI::eRunning ||
         old_status == CNetScheduleAPI::ePending) &&
         job.ShouldNotifySubmitter(current_time))
        m_NotificationsList.NotifyJobStatus(job.GetSubmAddr(),
                                            job.GetSubmNotifPort(),
                                            job_key,
                                            job.GetStatus(),
                                            job.GetLastEventIndex());

    return old_status;
}


unsigned int  CQueue::CancelAllJobs(const CNSClientId &  client,
                                    bool                 logging)
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
    return x_CancelJobs(client, m_StatusTracker.GetJobs(statuses),
                        logging);
}


unsigned int CQueue::x_CancelJobs(const CNSClientId &   client,
                                  const TNSBitVector &  jobs_to_cancel,
                                  bool                  logging)
{
    CJob                        job;
    CNSPreciseTime              current_time = CNSPreciseTime::Current();
    TNSBitVector::enumerator    en(jobs_to_cancel.first());
    unsigned int                count = 0;
    for (; en.valid(); ++en) {
        unsigned int    job_id = *en;
        TJobStatus      old_status = m_StatusTracker.GetStatus(job_id);

        {{
            CNSTransaction              transaction(this);
            if (job.Fetch(this, job_id) != CJob::eJF_Ok) {
                ERR_POST("Cannot fetch job " << DecorateJob(job_id) <<
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
        g_DoPerfLogging(*this, job, 200);

        TimeLineRemove(job_id);
        if (old_status == CNetScheduleAPI::eRunning)
            m_ClientsRegistry.UnregisterJob(job_id, eGet);
        else if (old_status == CNetScheduleAPI::eReading)
            m_ClientsRegistry.UnregisterJob(job_id, eRead);

        m_GCRegistry.UpdateLifetime(job_id,
                                    job.GetExpirationTime(m_Timeout,
                                                          m_RunTimeout,
                                                          m_ReadTimeout,
                                                          m_PendingTimeout,
                                                          current_time));

        if ((old_status == CNetScheduleAPI::eRunning ||
             old_status == CNetScheduleAPI::ePending) &&
             job.ShouldNotifySubmitter(current_time))
            m_NotificationsList.NotifyJobStatus(job.GetSubmAddr(),
                                                job.GetSubmNotifPort(),
                                                MakeJobKey(job_id),
                                                job.GetStatus(),
                                                job.GetLastEventIndex());
        if (job.ShouldNotifyListener(current_time, m_JobsToNotify))
            m_NotificationsList.NotifyJobStatus(job.GetListenerNotifAddr(),
                                                job.GetListenerNotifPort(),
                                                MakeJobKey(job_id),
                                                job.GetStatus(),
                                                job.GetLastEventIndex());

        // Notify the readers if the job has not been given for reading yet
        if (!m_ReadJobs.get_bit(job_id)) {
            m_GCRegistry.UpdateReadVacantTime(job_id, current_time);
            m_NotificationsList.Notify(job_id, job.GetAffinityId(),
                                       m_ClientsRegistry,
                                       m_AffinityRegistry,
                                       m_GroupRegistry,
                                       m_NotifHifreqPeriod,
                                       m_HandicapTimeout,
                                       eRead);
        }

        if (logging)
            GetDiagContext().Extra().Print("job_key", MakeJobKey(job_id))
                                    .Print("job_phid", job.GetNCBIPHID());

        ++count;
    }
    return count;
}


// This function must be called under the operations lock.
// If called for not existing job then an exception is generated
CNSPreciseTime
CQueue::x_GetEstimatedJobLifetime(unsigned int   job_id,
                                  TJobStatus     status) const
{
    if (status == CNetScheduleAPI::eRunning ||
        status == CNetScheduleAPI::eReading)
        return CNSPreciseTime::Current() + GetTimeout();
    return m_GCRegistry.GetLifetime(job_id);
}


unsigned int
CQueue::CancelSelectedJobs(const CNSClientId &         client,
                           const string &              group,
                           const string &              aff_token,
                           const vector<TJobStatus> &  job_statuses,
                           bool                        logging,
                           vector<string> &            warnings)
{
    if (group.empty() && aff_token.empty() && job_statuses.empty()) {
        // This possible if there was only 'Canceled' status and
        // it was filtered out. A warning for this case is already produced
        return 0;
    }

    TNSBitVector        jobs_to_cancel;
    vector<TJobStatus>  statuses;

    if (job_statuses.empty()) {
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
        // The user specified statuses explicitly
        // The list validity is checked by the caller.
        statuses = job_statuses;
    }

    CFastMutexGuard     guard(m_OperationLock);
    jobs_to_cancel = m_StatusTracker.GetJobs(statuses);

    if (!group.empty()) {
        try {
            jobs_to_cancel &= m_GroupRegistry.GetJobs(group);
        } catch (...) {
            jobs_to_cancel.clear();
            warnings.push_back("eGroupNotFound:job group " + group +
                               " is not found");
            if (logging)
                ERR_POST(Warning << "Job group '" + group +
                                    "' is not found. No jobs are canceled.");
        }
    }

    if (!aff_token.empty()) {
        unsigned int    aff_id = m_AffinityRegistry.GetIDByToken(aff_token);
        if (aff_id == 0) {
            jobs_to_cancel.clear();
            warnings.push_back("eAffinityNotFound:affinity " + aff_token +
                               " is not found");
            if (logging)
                ERR_POST(Warning << "Affinity '" + aff_token +
                                    "' is not found. No jobs are canceled.");
        }
        else
            jobs_to_cancel &= m_AffinityRegistry.GetJobsWithAffinity(aff_id);
    }

    return x_CancelJobs(client, jobs_to_cancel, logging);
}


TJobStatus  CQueue::GetJobStatus(unsigned int  job_id) const
{
    return m_StatusTracker.GetStatus(job_id);
}


bool CQueue::IsEmpty() const
{
    CFastMutexGuard     guard(m_OperationLock);
    return !m_StatusTracker.AnyJobs();
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
        m_Server->SetJobsStartID(m_QueueName, m_SavedId);
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
        m_Server->SetJobsStartID(m_QueueName, m_SavedId);
    }

    // There were no overflow, check the reserved value
    if (m_LastId >= m_SavedId) {
        m_SavedId += s_ReserveDelta;
        if (m_SavedId < m_LastId) {
            // Overflow for the saved ID
            m_LastId = count;
            m_SavedId = count + s_ReserveDelta;
        }
        m_Server->SetJobsStartID(m_QueueName, m_SavedId);
    }

    return m_LastId - count + 1;
}


bool
CQueue::GetJobForReadingOrWait(const CNSClientId &       client,
                               unsigned int              port,
                               unsigned int              timeout,
                               const list<string> *      aff_list,
                               bool                      reader_affinity,
                               bool                      any_affinity,
                               bool                      exclusive_new_affinity,
                               bool                      prioritized_aff,
                               const string &            group,
                               bool                      affinity_may_change,
                               bool                      group_may_change,
                               CJob *                    job,
                               bool *                    no_more_jobs,
                               CNSRollbackInterface * &  rollback_action,
                               string &                  added_pref_aff)
{
    CNSPreciseTime          curr = CNSPreciseTime::Current();
    vector<unsigned int>    aff_ids;

    // This is a reader command, so mark the node type as a reader
    m_ClientsRegistry.AppendType(client, CNSClient::eReader);

    *no_more_jobs = false;

    {{
        CFastMutexGuard     guard(m_OperationLock);

        if (reader_affinity) {
            // Check that the preferred affinities were not reset
            if (m_ClientsRegistry.GetAffinityReset(client, eRead))
                return false;

            // Check that the client was garbage collected with preferred affs
            if (m_ClientsRegistry.WasGarbageCollected(client, eRead))
                return false;
        }

        // Resolve affinities and groups. It is supposed that the client knows
        // better what affinities and groups to expect i.e. even if they do not
        // exist yet, they may appear soon.
        {{
            CNSTransaction      transaction(this);

            m_GroupRegistry.ResolveGroup(group);
            aff_ids = m_AffinityRegistry.ResolveAffinities(*aff_list);
            transaction.Commit();
        }}

        m_ClientsRegistry.CancelWaiting(client, eRead);
    }}

    TNSBitVector    aff_ids_vector;
    for (vector<unsigned int>::const_iterator  k = aff_ids.begin();
            k != aff_ids.end(); ++k)
        aff_ids_vector.set(*k, true);

    for (;;) {
        // No lock here to make it possible to pick a job
        // simultaneously from many threads
        x_SJobPick  job_pick = x_FindVacantJob(client, aff_ids, reader_affinity,
                                               any_affinity,
                                               exclusive_new_affinity,
                                               prioritized_aff,
                                               group, eRead);

        {{
            bool                outdated_job = false;
            TJobStatus          old_status;
            CFastMutexGuard     guard(m_OperationLock);

            if (job_pick.job_id == 0) {
                if (exclusive_new_affinity)
                    job_pick = x_FindOutdatedJobForReading(client, 0);

                if (job_pick.job_id == 0) {
                    *no_more_jobs = x_NoMoreReadJobs(client, aff_ids_vector,
                                                 reader_affinity, any_affinity,
                                                 exclusive_new_affinity, group,
                                                 affinity_may_change,
                                                 group_may_change);
                    if (timeout != 0 && port > 0)
                        x_RegisterReadListener(client, port, timeout,
                                           aff_ids_vector,
                                           reader_affinity, any_affinity,
                                           exclusive_new_affinity, group);
                    return true;
                }
                outdated_job = true;
            } else {
                // Check that the job is still Done/Failed/Canceled
                // it could be grabbed by another reader or GC
                old_status = GetJobStatus(job_pick.job_id);
                if (old_status != CNetScheduleAPI::eDone &&
                    old_status != CNetScheduleAPI::eFailed &&
                    old_status != CNetScheduleAPI::eCanceled)
                    continue;   // try to pick another job

                if (exclusive_new_affinity) {
                    if (m_GCRegistry.IsOutdatedJob(
                                    job_pick.job_id, eRead,
                                    m_MaxPendingReadWaitTimeout) == false) {
                        x_SJobPick  outdated_pick =
                                        x_FindOutdatedJobForReading(
                                                client, job_pick.job_id);
                        if (outdated_pick.job_id != 0) {
                            job_pick = outdated_pick;
                            outdated_job = true;
                        }
                    }
                }
            }

            // The job is still in acceptable state. Check if it was received
            // with exclusive affinity
            if (job_pick.exclusive && job_pick.aff_id != 0 &&
                outdated_job == false) {
                if (m_ClientsRegistry.IsPreferredByAny(job_pick.aff_id, eRead))
                    continue;   // Other reader grabbed this affinity already

                bool added = m_ClientsRegistry.UpdatePreferredAffinities(
                                            client, job_pick.aff_id, 0, eRead);
                if (added)
                    added_pref_aff = m_AffinityRegistry.
                                            GetTokenByID(job_pick.aff_id);
            }

            if (outdated_job && job_pick.aff_id != 0) {
                bool added = m_ClientsRegistry.
                                UpdatePreferredAffinities(
                                        client, job_pick.aff_id, 0, eRead);
                if (added)
                    added_pref_aff = m_AffinityRegistry.GetTokenByID(
                                                            job_pick.aff_id);
            }

            old_status = GetJobStatus(job_pick.job_id);
            x_UpdateDB_ProvideJobNoLock(client, curr, job_pick.job_id,
                                        eRead, *job);
            m_StatusTracker.SetStatus(job_pick.job_id,
                                      CNetScheduleAPI::eReading);

            m_StatisticsCounters.CountTransition(old_status,
                                                 CNetScheduleAPI::eReading);
            g_DoPerfLogging(*this, *job, 200);

            if (outdated_job)
                m_StatisticsCounters.CountOutdatedPick(eRead);

            m_GCRegistry.UpdateLifetime(job_pick.job_id,
                                        job->GetExpirationTime(m_Timeout,
                                                               m_RunTimeout,
                                                               m_ReadTimeout,
                                                               m_PendingTimeout,
                                                               curr));
            TimeLineAdd(job_pick.job_id, curr + m_ReadTimeout);
            m_ClientsRegistry.RegisterJob(client, job_pick.job_id, eRead);


            if (job->ShouldNotifyListener(curr, m_JobsToNotify))
                m_NotificationsList.NotifyJobStatus(job->GetListenerNotifAddr(),
                                                    job->GetListenerNotifPort(),
                                                    MakeJobKey(job_pick.job_id),
                                                    job->GetStatus(),
                                                    job->GetLastEventIndex());

            rollback_action = new CNSReadJobRollback(client, job_pick.job_id,
                                                     old_status);
            m_ReadJobs.set_bit(job_pick.job_id);
            ++m_ReadJobsOps;
            return true;
        }}
    }
    return true;    // unreachable
}


TJobStatus  CQueue::ConfirmReadingJob(const CNSClientId &  client,
                                      unsigned int         job_id,
                                      const string &       job_key,
                                      CJob &               job,
                                      const string &       auth_token)
{
    TJobStatus      old_status = x_ChangeReadingStatus(
                                                client, job_id, job_key,
                                                job, auth_token, "",
                                                CNetScheduleAPI::eConfirmed,
                                                false, false);
    m_ClientsRegistry.UnregisterJob(client, job_id, eRead);
    return old_status;
}


TJobStatus  CQueue::FailReadingJob(const CNSClientId &  client,
                                   unsigned int         job_id,
                                   const string &       job_key,
                                   CJob &               job,
                                   const string &       auth_token,
                                   const string &       err_msg,
                                   bool                 no_retries)
{
    TJobStatus      old_status = x_ChangeReadingStatus(
                                                client, job_id, job_key,
                                                job, auth_token, err_msg,
                                                CNetScheduleAPI::eReadFailed,
                                                false, no_retries);
    m_ClientsRegistry.MoveJobToBlacklist(job_id, eRead);
    return old_status;
}


TJobStatus  CQueue::ReturnReadingJob(const CNSClientId &  client,
                                     unsigned int         job_id,
                                     const string &       job_key,
                                     CJob &               job,
                                     const string &       auth_token,
                                     bool                 is_ns_rollback,
                                     bool                 blacklist,
                                     TJobStatus           target_status)
{
    TJobStatus      old_status = x_ChangeReadingStatus(
                                                client, job_id, job_key,
                                                job, auth_token, "",
                                                target_status,
                                                is_ns_rollback,
                                                false);
    if (is_ns_rollback || blacklist == false)
        m_ClientsRegistry.UnregisterJob(job_id, eRead);
    else
        m_ClientsRegistry.MoveJobToBlacklist(job_id, eRead);
    return old_status;
}


TJobStatus  CQueue::x_ChangeReadingStatus(const CNSClientId &  client,
                                          unsigned int         job_id,
                                          const string &       job_key,
                                          CJob &               job,
                                          const string &       auth_token,
                                          const string &       err_msg,
                                          TJobStatus           target_status,
                                          bool                 is_ns_rollback,
                                          bool                 no_retries)
{
    CNSPreciseTime                              current_time =
                                                    CNSPreciseTime::Current();
    CStatisticsCounters::ETransitionPathOption  path_option =
                                                    CStatisticsCounters::eNone;
    CFastMutexGuard                             guard(m_OperationLock);
    TJobStatus                                  old_status =
                                                    GetJobStatus(job_id);

    if (old_status != CNetScheduleAPI::eReading)
        return old_status;

    {{
        CNSTransaction      transaction(this);

        if (job.Fetch(this, job_id) != CJob::eJF_Ok)
            NCBI_THROW(CNetScheduleException, eInternalError,
                       "Error fetching job");

        // Check that authorization token matches
        if (is_ns_rollback == false) {
            CJob::EAuthTokenCompareResult   token_compare_result =
                                            job.CompareAuthToken(auth_token);
            if (token_compare_result == CJob::eInvalidTokenFormat)
                NCBI_THROW(CNetScheduleException, eInvalidAuthToken,
                           "Invalid authorization token format");
            if (token_compare_result == CJob::eNoMatch)
                NCBI_THROW(CNetScheduleException, eInvalidAuthToken,
                           "Authorization token does not match");
        }

        // Sanity check of the current job state
        if (job.GetStatus() != CNetScheduleAPI::eReading)
            NCBI_THROW(CNetScheduleException, eInternalError,
                "Internal inconsistency detected. The job state in memory is " +
                CNetScheduleAPI::StatusToString(CNetScheduleAPI::eReading) +
                " while in database it is " +
                CNetScheduleAPI::StatusToString(job.GetStatus()));

        if (target_status == CNetScheduleAPI::eJobNotFound)
            target_status = job.GetStatusBeforeReading();


        // Add an event
        CJobEvent &     event = job.AppendEvent();
        event.SetTimestamp(current_time);
        event.SetNodeAddr(client.GetAddress());
        event.SetClientNode(client.GetNode());
        event.SetClientSession(client.GetSession());
        event.SetErrorMsg(err_msg);

        if (is_ns_rollback) {
            event.SetEvent(CJobEvent::eNSReadRollback);
            job.SetReadCount(job.GetReadCount() - 1);
        } else {
            switch (target_status) {
                case CNetScheduleAPI::eFailed:
                case CNetScheduleAPI::eDone:
                case CNetScheduleAPI::eCanceled:
                    event.SetEvent(CJobEvent::eReadRollback);
                    job.SetReadCount(job.GetReadCount() - 1);
                    break;
                case CNetScheduleAPI::eReadFailed:
                    if (no_retries) {
                        event.SetEvent(CJobEvent::eReadFinalFail);
                    } else {
                        event.SetEvent(CJobEvent::eReadFail);
                        // Check the number of tries first
                        if (job.GetReadCount() <= m_ReadFailedRetries) {
                            // The job needs to be re-scheduled for reading
                            target_status = CNetScheduleAPI::eDone;
                            path_option = CStatisticsCounters::eFail;
                        }
                    }
                    break;
                case CNetScheduleAPI::eConfirmed:
                    event.SetEvent(CJobEvent::eReadDone);
                    break;
                default:
                    _ASSERT(0);
                    break;
            }
        }

        event.SetStatus(target_status);
        job.SetStatus(target_status);
        job.SetLastTouch(current_time);

        job.Flush(this);
        transaction.Commit();
    }}

    if (target_status != CNetScheduleAPI::eConfirmed &&
        target_status != CNetScheduleAPI::eReadFailed) {
        m_ReadJobs.set_bit(job_id, false);
        ++m_ReadJobsOps;

        m_GCRegistry.UpdateReadVacantTime(job_id, current_time);

        // Notify the readers
        m_NotificationsList.Notify(job_id, job.GetAffinityId(),
                                   m_ClientsRegistry,
                                   m_AffinityRegistry,
                                   m_GroupRegistry,
                                   m_NotifHifreqPeriod,
                                   m_HandicapTimeout,
                                   eRead);
    }

    TimeLineRemove(job_id);

    m_StatusTracker.SetStatus(job_id, target_status);
    m_GCRegistry.UpdateLifetime(job_id, job.GetExpirationTime(m_Timeout,
                                                              m_RunTimeout,
                                                              m_ReadTimeout,
                                                              m_PendingTimeout,
                                                              current_time));
    if (is_ns_rollback)
        m_StatisticsCounters.CountNSReadRollback(1);
    else
        m_StatisticsCounters.CountTransition(CNetScheduleAPI::eReading,
                                             target_status,
                                             path_option);
    g_DoPerfLogging(*this, job, 200);
    if (job.ShouldNotifyListener(current_time, m_JobsToNotify))
        m_NotificationsList.NotifyJobStatus(job.GetListenerNotifAddr(),
                                            job.GetListenerNotifPort(),
                                            job_key,
                                            job.GetStatus(),
                                            job.GetLastEventIndex());
    return CNetScheduleAPI::eReading;
}


// This function is called from places where the operations lock has been
// already taken. So there is no lock around memory status tracker
void CQueue::EraseJob(unsigned int  job_id, TJobStatus  status)
{
    m_StatusTracker.Erase(job_id);

    {{
        // Request delayed record delete
        CFastMutexGuard     jtd_guard(m_JobsToDeleteLock);

        m_JobsToDelete.set_bit(job_id);
        ++m_JobsToDeleteOps;
    }}
    TimeLineRemove(job_id);
    m_StatisticsCounters.CountTransitionToDeleted(status, 1);
}


// status - the job status from which the job was deleted
void CQueue::x_Erase(const TNSBitVector &  job_ids, TJobStatus  status)
{
    CFastMutexGuard     jtd_guard(m_JobsToDeleteLock);
    size_t              job_count = job_ids.count();

    m_JobsToDelete |= job_ids;
    m_JobsToDeleteOps += job_count;
    m_StatisticsCounters.CountTransitionToDeleted(status, job_count);
}


void CQueue::OptimizeMem()
{
    m_StatusTracker.OptimizeMem();
}


CQueue::x_SJobPick
CQueue::x_FindVacantJob(const CNSClientId  &          client,
                        const vector<unsigned int> &  aff_ids,
                        bool                          use_pref_affinity,
                        bool                          any_affinity,
                        bool                          exclusive_new_affinity,
                        bool                          prioritized_aff,
                        const string &                group,
                        ECommandGroup                 cmd_group)
{
    x_SJobPick      job_pick = { 0, false, 0 };
    bool            explicit_aff = !aff_ids.empty();
    bool            effective_use_pref_affinity = use_pref_affinity;
    unsigned int    pref_aff_candidate_job_id = 0;
    unsigned int    exclusive_aff_candidate = 0;
    TNSBitVector    group_jobs = m_GroupRegistry.GetJobs(group, false);
    TNSBitVector    blacklisted_jobs =
                    m_ClientsRegistry.GetBlacklistedJobs(client, cmd_group);

    TNSBitVector    pref_aff;
    if (use_pref_affinity) {
        pref_aff = m_ClientsRegistry.GetPreferredAffinities(client, cmd_group);
        effective_use_pref_affinity = use_pref_affinity && pref_aff.any();
    }

    TNSBitVector    all_pref_aff;
    if (exclusive_new_affinity)
        all_pref_aff = m_ClientsRegistry.GetAllPreferredAffinities(cmd_group);

    if (explicit_aff || effective_use_pref_affinity || exclusive_new_affinity) {
        // Check all vacant jobs: pending jobs for eGet,
        //                        done/failed/cancel jobs for eRead
        TNSBitVector    vacant_jobs;
        if (cmd_group == eGet)
            vacant_jobs = m_StatusTracker.GetJobs(CNetScheduleAPI::ePending);
        else {
            vector<CNetScheduleAPI::EJobStatus>     from_state;
            from_state.push_back(CNetScheduleAPI::eDone);
            from_state.push_back(CNetScheduleAPI::eFailed);
            from_state.push_back(CNetScheduleAPI::eCanceled);
            vacant_jobs = m_StatusTracker.GetJobs(from_state);
        }

        // Exclude blacklisted jobs
        vacant_jobs -= blacklisted_jobs;
        // Keep only the group jobs if the group is provided
        if (!group.empty())
            vacant_jobs &= group_jobs;

        // Exclude jobs which have been read or in a process of reading
        if (cmd_group == eRead)
            vacant_jobs -= m_ReadJobs;

        if (prioritized_aff) {
            // The only criteria here is a list of explicit affinities
            // respecting their order
            for (vector<unsigned int>::const_iterator  k = aff_ids.begin();
                    k != aff_ids.end(); ++k) {
                TNSBitVector    aff_jobs = m_AffinityRegistry.
                                                    GetJobsWithAffinity(*k);
                TNSBitVector    candidates = vacant_jobs & aff_jobs;
                if (candidates.any()) {
                    job_pick.job_id = *(candidates.first());
                    job_pick.exclusive = false;
                    job_pick.aff_id = *k;
                    return job_pick;
                }
            }
            return job_pick;
        }

        // HERE: no prioritized affinities
        TNSBitVector    explicit_aff_vector;
        for (vector<unsigned int>::const_iterator  k = aff_ids.begin();
                k != aff_ids.end(); ++k)
            explicit_aff_vector.set(*k, true);

        TNSBitVector::enumerator    en(vacant_jobs.first());
        for (; en.valid(); ++en) {
            unsigned int    job_id = *en;

            unsigned int    aff_id = m_GCRegistry.GetAffinityID(job_id);
            if (aff_id != 0 && explicit_aff) {
                if (explicit_aff_vector.get_bit(aff_id)) {
                    job_pick.job_id = job_id;
                    job_pick.exclusive = false;
                    job_pick.aff_id = aff_id;
                    return job_pick;
                }
            }

            if (aff_id != 0 && effective_use_pref_affinity) {
                if (pref_aff.get_bit(aff_id)) {
                    if (explicit_aff == false) {
                        job_pick.job_id = job_id;
                        job_pick.exclusive = false;
                        job_pick.aff_id = aff_id;
                        return job_pick;
                    }
                    if (pref_aff_candidate_job_id == 0)
                        pref_aff_candidate_job_id = job_id;
                    continue;
                }
            }

            if (exclusive_new_affinity) {
                if (aff_id == 0 || all_pref_aff.get_bit(aff_id) == false) {
                    if (explicit_aff == false &&
                        effective_use_pref_affinity == false) {
                        job_pick.job_id = job_id;
                        job_pick.exclusive = true;
                        job_pick.aff_id = aff_id;
                        return job_pick;
                    }
                    if (exclusive_aff_candidate == 0)
                        exclusive_aff_candidate = job_id;
                }
            }
        } // end for

        if (pref_aff_candidate_job_id != 0) {
            job_pick.job_id = pref_aff_candidate_job_id;
            job_pick.exclusive = false;
            job_pick.aff_id = 0;
            return job_pick;
        }

        if (exclusive_aff_candidate != 0) {
            job_pick.job_id = exclusive_aff_candidate;
            job_pick.exclusive = true;
            job_pick.aff_id = m_GCRegistry.GetAffinityID(
                                                exclusive_aff_candidate);
            return job_pick;
        }
    }


    // The second condition looks strange and it covers a very specific
    // scenario: some (older) worker nodes may originally come with the only
    // flag set - use preferred affinities - while they have nothing in the
    // list of preferred affinities yet. In this case a first pending job
    // should be provided.
    if (any_affinity ||
        (!explicit_aff &&
         use_pref_affinity && !effective_use_pref_affinity &&
         !exclusive_new_affinity &&
         cmd_group == eGet)) {
        if (cmd_group == eGet)
            job_pick.job_id = m_StatusTracker.GetJobByStatus(
                                        CNetScheduleAPI::ePending,
                                        blacklisted_jobs,
                                        group_jobs, !group.empty());
        else {
            vector<CNetScheduleAPI::EJobStatus>     from_state;
            from_state.push_back(CNetScheduleAPI::eDone);
            from_state.push_back(CNetScheduleAPI::eFailed);
            from_state.push_back(CNetScheduleAPI::eCanceled);
            job_pick.job_id = m_StatusTracker.GetJobByStatus(
                                        from_state,
                                        blacklisted_jobs | m_ReadJobs,
                                        group_jobs, !group.empty());
        }
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

    if (m_MaxPendingWaitTimeout == kTimeZero)
        return job_pick;    // Not configured

    TNSBitVector    outdated_pending =
                        m_StatusTracker.GetOutdatedPendingJobs(
                                m_MaxPendingWaitTimeout,
                                m_GCRegistry);
    if (picked_earlier != 0)
        outdated_pending.set_bit(picked_earlier, false);

    if (!outdated_pending.any())
        return job_pick;

    outdated_pending -= m_ClientsRegistry.GetBlacklistedJobs(client, eGet);

    if (!outdated_pending.any())
        return job_pick;

    job_pick.job_id = *outdated_pending.first();
    job_pick.aff_id = m_GCRegistry.GetAffinityID(job_pick.job_id);
    job_pick.exclusive = job_pick.aff_id != 0;
    return job_pick;
}


CQueue::x_SJobPick
CQueue::x_FindOutdatedJobForReading(const CNSClientId &  client,
                                   unsigned int         picked_earlier)
{
    x_SJobPick      job_pick = { 0, false, 0 };

    if (m_MaxPendingReadWaitTimeout == kTimeZero)
        return job_pick;    // Not configured

    TNSBitVector    outdated_read_jobs =
                        m_StatusTracker.GetOutdatedReadVacantJobs(
                                m_MaxPendingReadWaitTimeout,
                                m_ReadJobs, m_GCRegistry);
    if (picked_earlier != 0)
        outdated_read_jobs.set_bit(picked_earlier, false);

    if (!outdated_read_jobs.any())
        return job_pick;

    outdated_read_jobs -= m_ClientsRegistry.GetBlacklistedJobs(client, eRead);

    if (!outdated_read_jobs.any())
        return job_pick;

    job_pick.job_id = *outdated_read_jobs.first();
    job_pick.aff_id = m_GCRegistry.GetAffinityID(job_pick.job_id);
    job_pick.exclusive = job_pick.aff_id != 0;
    return job_pick;
}


string CQueue::GetAffinityList(const CNSClientId &    client)
{
    m_ClientsRegistry.MarkAsAdmin(client);

    list< SAffinityStatistics >
                statistics = m_AffinityRegistry.GetAffinityStatistics(
                                                            m_StatusTracker);

    string          aff_list;
    for (list< SAffinityStatistics >::const_iterator  k = statistics.begin();
         k != statistics.end(); ++k) {
        if (!aff_list.empty())
            aff_list += "&";

        aff_list += NStr::URLEncode(k->m_Token) + '=' +
                    NStr::NumericToString(k->m_NumberOfPendingJobs) + "," +
                    NStr::NumericToString(k->m_NumberOfRunningJobs) + "," +
                    NStr::NumericToString(k->m_NumberOfWNPreferred) + "," +
                    NStr::NumericToString(k->m_NumberOfWNWaitGet);

        // NS 4.20.0 has more information: Read preferred affinities and read
        // wait affinities. It was decided however that the AFLS command will
        // not output them as they could break the old clients.
    }
    return aff_list;
}


TJobStatus CQueue::FailJob(const CNSClientId &    client,
                           unsigned int           job_id,
                           const string &         job_key,
                           CJob &                 job,
                           const string &         auth_token,
                           const string &         err_msg,
                           const string &         output,
                           int                    ret_code,
                           bool                   no_retries,
                           string                 warning)
{
    unsigned        failed_retries;
    unsigned        max_output_size;
    {{
        CQueueParamAccessor     qp(*this);
        failed_retries  = qp.GetFailedRetries();
        max_output_size = qp.GetMaxOutputSize();
    }}

    if (output.size() > max_output_size) {
        NCBI_THROW(CNetScheduleException, eDataTooLong,
                   "Output is too long");
    }

    CNSPreciseTime      curr = CNSPreciseTime::Current();
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
                           "Error fetching job");

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
                    ERR_POST(Warning << "Received FPUT2 with only "
                                        "passport matched.");
                    warning = "eJobPassportOnlyMatch:Only job passport "
                              "matched. Command is ignored.";
                    return old_status;
                }
                // Here: the authorization token is OK, we can continue
            }

            CJobEvent *     event = job.GetLastEvent();
            if (!event)
                ERR_POST("No JobEvent for running job");

            event = &job.AppendEvent();
            if (no_retries)
                event->SetEvent(CJobEvent::eFinalFail);
            else
                event->SetEvent(CJobEvent::eFail);
            event->SetStatus(CNetScheduleAPI::eFailed);
            event->SetTimestamp(curr);
            event->SetErrorMsg(err_msg);
            event->SetRetCode(ret_code);
            event->SetNodeAddr(client.GetAddress());
            event->SetClientNode(client.GetNode());
            event->SetClientSession(client.GetSession());

            if (no_retries) {
                job.SetStatus(CNetScheduleAPI::eFailed);
                event->SetStatus(CNetScheduleAPI::eFailed);
                rescheduled = false;
                if (m_Log)
                    ERR_POST(Warning << "Job failed "
                                        "unconditionally, no_retries = 1");
            } else {
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
                        ERR_POST(Warning << "Job failed, exceeded "
                                            "max number of retries ("
                                         << failed_retries << ")");
                }
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
        g_DoPerfLogging(*this, job, 200);

        TimeLineRemove(job_id);

        // Replace it with ClearExecuting(client, job_id) when all clients
        // provide their credentials and job passport is checked strictly
        m_ClientsRegistry.UnregisterJob(job_id, eGet);
        m_ClientsRegistry.RegisterBlacklistedJob(client, job_id, eGet);

        m_GCRegistry.UpdateLifetime(job_id,
                                    job.GetExpirationTime(m_Timeout,
                                                          m_RunTimeout,
                                                          m_ReadTimeout,
                                                          m_PendingTimeout,
                                                          curr));

        if (!rescheduled  &&  job.ShouldNotifySubmitter(curr)) {
            m_NotificationsList.NotifyJobStatus(job.GetSubmAddr(),
                                                job.GetSubmNotifPort(),
                                                job_key,
                                                job.GetStatus(),
                                                job.GetLastEventIndex());
        }

        if (rescheduled && m_PauseStatus == eNoPause)
            m_NotificationsList.Notify(job_id,
                                       job.GetAffinityId(), m_ClientsRegistry,
                                       m_AffinityRegistry, m_GroupRegistry,
                                       m_NotifHifreqPeriod, m_HandicapTimeout,
                                       eGet);

        if (new_status == CNetScheduleAPI::eFailed)
            if (!m_ReadJobs.get_bit(job_id)) {
                m_GCRegistry.UpdateReadVacantTime(job_id, curr);
                m_NotificationsList.Notify(job_id, job.GetAffinityId(),
                                           m_ClientsRegistry,
                                           m_AffinityRegistry,
                                           m_GroupRegistry,
                                           m_NotifHifreqPeriod,
                                           m_HandicapTimeout,
                                           eRead);
            }

        if (job.ShouldNotifyListener(curr, m_JobsToNotify))
            m_NotificationsList.NotifyJobStatus(job.GetListenerNotifAddr(),
                                                job.GetListenerNotifPort(),
                                                job_key,
                                                job.GetStatus(),
                                                job.GetLastEventIndex());
    }}

    return old_status;
}


string  CQueue::GetAffinityTokenByID(unsigned int  aff_id) const
{
    return m_AffinityRegistry.GetTokenByID(aff_id);
}


void CQueue::ClearWorkerNode(const CNSClientId &  client,
                             bool &               client_was_found,
                             string &             old_session,
                             bool &               had_wn_pref_affs,
                             bool &               had_reader_pref_affs)
{
    // Get the running and reading jobs and move them to the corresponding
    // states (pending and done)

    TNSBitVector    running_jobs;
    TNSBitVector    reading_jobs;

    {{
        CFastMutexGuard     guard(m_OperationLock);
        m_ClientsRegistry.ClearClient(client, running_jobs, reading_jobs,
                                      client_was_found, old_session,
                                      had_wn_pref_affs, had_reader_pref_affs);

    }}

    if (running_jobs.any())
        x_ResetRunningDueToClear(client, running_jobs);
    if (reading_jobs.any())
        x_ResetReadingDueToClear(client, reading_jobs);
    return;
}


// Triggered from a notification thread only
void CQueue::NotifyListenersPeriodically(const CNSPreciseTime &  current_time)
{
    if (m_MaxPendingWaitTimeout != kTimeZero) {
        // Pending outdated timeout is configured, so check outdated jobs
        CFastMutexGuard     guard(m_OperationLock);
        TNSBitVector        outdated_jobs =
                                    m_StatusTracker.GetOutdatedPendingJobs(
                                        m_MaxPendingWaitTimeout,
                                        m_GCRegistry);
        if (outdated_jobs.any())
            m_NotificationsList.CheckOutdatedJobs(outdated_jobs,
                                                  m_ClientsRegistry,
                                                  m_NotifHifreqPeriod,
                                                  eGet);
    }

    if (m_MaxPendingReadWaitTimeout != kTimeZero) {
        // Read pending timeout is configured, so check read outdated jobs
        CFastMutexGuard     guard(m_OperationLock);
        TNSBitVector        outdated_jobs =
                                    m_StatusTracker.GetOutdatedReadVacantJobs(
                                        m_MaxPendingReadWaitTimeout,
                                        m_ReadJobs, m_GCRegistry);
        if (outdated_jobs.any())
            m_NotificationsList.CheckOutdatedJobs(outdated_jobs,
                                                  m_ClientsRegistry,
                                                  m_NotifHifreqPeriod,
                                                  eRead);
    }


    // Check the configured notification interval
    static CNSPreciseTime   last_notif_timeout = kTimeNever;
    static size_t           skip_limit = 0;
    static size_t           skip_count;

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
                                               m_ClientsRegistry);
    else
        m_NotificationsList.CheckTimeout(current_time, m_ClientsRegistry, eGet);
}


CNSPreciseTime CQueue::NotifyExactListeners(void)
{
    return m_NotificationsList.NotifyExactListeners();
}


string CQueue::PrintClientsList(bool verbose) const
{
    return m_ClientsRegistry.PrintClientsList(this,
                                              m_DumpClientBufferSize, verbose);
}


string CQueue::PrintNotificationsList(bool verbose) const
{
    return m_NotificationsList.Print(m_ClientsRegistry, m_AffinityRegistry,
                                     verbose);
}


string CQueue::PrintAffinitiesList(bool verbose) const
{
    return m_AffinityRegistry.Print(this, m_ClientsRegistry,
                                    m_DumpAffBufferSize, verbose);
}


string CQueue::PrintGroupsList(bool verbose) const
{
    return m_GroupRegistry.Print(this, m_DumpGroupBufferSize, verbose);
}


void CQueue::CheckExecutionTimeout(bool  logging)
{
    if (!m_RunTimeLine)
        return;

    CNSPreciseTime  queue_run_timeout = GetRunTimeout();
    CNSPreciseTime  queue_read_timeout = GetReadTimeout();
    CNSPreciseTime  curr = CNSPreciseTime::Current();
    TNSBitVector    bv;
    {{
        CReadLockGuard  guard(m_RunTimeLineLock);
        m_RunTimeLine->ExtractObjects(curr.Sec(), &bv);
    }}

    TNSBitVector::enumerator en(bv.first());
    for ( ;en.valid(); ++en) {
        x_CheckExecutionTimeout(queue_run_timeout, queue_read_timeout,
                                *en, curr, logging);
    }
}


void CQueue::x_CheckExecutionTimeout(const CNSPreciseTime &  queue_run_timeout,
                                     const CNSPreciseTime &  queue_read_timeout,
                                     unsigned int            job_id,
                                     const CNSPreciseTime &  curr_time,
                                     bool                    logging)
{
    CJob                    job;
    CNSPreciseTime          time_start = kTimeZero;
    CNSPreciseTime          run_timeout = kTimeZero;
    CNSPreciseTime          read_timeout = kTimeZero;
    CNSPreciseTime          exp_time = kTimeZero;
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
            if (run_timeout == kTimeZero)
                run_timeout = queue_run_timeout;

            if (status == CNetScheduleAPI::eRunning &&
                run_timeout == kTimeZero)
                // 0 timeout means the job never fails
                return;

            read_timeout = job.GetReadTimeout();
            if (read_timeout == kTimeZero)
                read_timeout = queue_read_timeout;

            if (status == CNetScheduleAPI::eReading &&
                read_timeout == kTimeZero)
                // 0 timeout means the job never fails
                return;

            // Calculate the expiration time
            if (status == CNetScheduleAPI::eRunning)
                exp_time = time_start + run_timeout;
            else
                exp_time = time_start + read_timeout;

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
                if (job.GetReadCount() > m_ReadFailedRetries)
                    new_status = CNetScheduleAPI::eReadFailed;
                else
                    new_status = job.GetStatusBeforeReading();
                m_ReadJobs.set_bit(job_id, false);
                ++m_ReadJobsOps;
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
                                                          m_ReadTimeout,
                                                          m_PendingTimeout,
                                                          curr_time));

        if (status == CNetScheduleAPI::eRunning) {
            if (new_status == CNetScheduleAPI::ePending) {
                // Timeout and reschedule, put to blacklist as well
                m_ClientsRegistry.MoveJobToBlacklist(job_id, eGet);
                m_StatisticsCounters.CountTransition(
                                            CNetScheduleAPI::eRunning,
                                            new_status,
                                            CStatisticsCounters::eTimeout);
            } else {
                m_ClientsRegistry.UnregisterJob(job_id, eGet);
                m_StatisticsCounters.CountTransition(
                                            CNetScheduleAPI::eRunning,
                                            CNetScheduleAPI::eFailed,
                                            CStatisticsCounters::eTimeout);
            }
        } else {
            if (new_status == CNetScheduleAPI::eReadFailed) {
                m_ClientsRegistry.UnregisterJob(job_id, eRead);
                m_StatisticsCounters.CountTransition(
                                            CNetScheduleAPI::eReading,
                                            new_status,
                                            CStatisticsCounters::eTimeout);
            } else {
                // The target status could be Done, Failed, Canceled.
                // The job could be read again by another reader.
                m_ClientsRegistry.MoveJobToBlacklist(job_id, eRead);
                m_StatisticsCounters.CountTransition(
                                            CNetScheduleAPI::eReading,
                                            new_status,
                                            CStatisticsCounters::eTimeout);
            }
        }
        g_DoPerfLogging(*this, job, 200);

        if (new_status == CNetScheduleAPI::ePending &&
            m_PauseStatus == eNoPause)
            m_NotificationsList.Notify(job_id,
                                       job.GetAffinityId(), m_ClientsRegistry,
                                       m_AffinityRegistry, m_GroupRegistry,
                                       m_NotifHifreqPeriod, m_HandicapTimeout,
                                       eGet);

        if (new_status == CNetScheduleAPI::eDone ||
            new_status == CNetScheduleAPI::eFailed ||
            new_status == CNetScheduleAPI::eCanceled)
            if (!m_ReadJobs.get_bit(job_id)) {
                m_GCRegistry.UpdateReadVacantTime(job_id, curr_time);
                m_NotificationsList.Notify(job_id, job.GetAffinityId(),
                                           m_ClientsRegistry,
                                           m_AffinityRegistry,
                                           m_GroupRegistry,
                                           m_NotifHifreqPeriod,
                                           m_HandicapTimeout,
                                           eRead);
            }

        if (job.ShouldNotifyListener(curr_time, m_JobsToNotify))
            m_NotificationsList.NotifyJobStatus(job.GetListenerNotifAddr(),
                                                job.GetListenerNotifPort(),
                                                MakeJobKey(job_id),
                                                job.GetStatus(),
                                                job.GetLastEventIndex());
    }}

    if (status == CNetScheduleAPI::eRunning &&
        new_status == CNetScheduleAPI::eFailed &&
        job.ShouldNotifySubmitter(curr_time))
        m_NotificationsList.NotifyJobStatus(job.GetSubmAddr(),
                                            job.GetSubmNotifPort(),
                                            MakeJobKey(job_id),
                                            job.GetStatus(),
                                            job.GetLastEventIndex());

    if (logging)
    {
        string      purpose;
        if (status == CNetScheduleAPI::eRunning)
            purpose = "execution";
        else
            purpose = "reading";

        GetDiagContext().Extra()
                .Print("msg", "Timeout expired, rescheduled for " + purpose)
                .Print("msg_code", "410")   // The code is for
                                            // searching in applog
                .Print("job_key", MakeJobKey(job_id))
                .Print("queue", m_QueueName)
                .Print("run_counter", job.GetRunCount())
                .Print("read_counter", job.GetReadCount())
                .Print("time_start", NS_FormatPreciseTime(time_start))
                .Print("exp_time", NS_FormatPreciseTime(exp_time))
                .Print("run_timeout", run_timeout)
                .Print("read_timeout", read_timeout);
    }
}


// Checks up to given # of jobs at the given status for expiration and
// marks up to given # of jobs for deletion.
// Returns the # of performed scans, the # of jobs marked for deletion and
// the last scanned job id.
SPurgeAttributes
CQueue::CheckJobsExpiry(const CNSPreciseTime &  current_time,
                        SPurgeAttributes        attributes,
                        unsigned int            last_job,
                        TJobStatus              status)
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

        for (result.scans = 0;
             result.scans < attributes.scans; ++result.scans) {
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

    if (result.deleted > 0) {
        x_Erase(job_ids, status);

        CFastMutexGuard     guard(m_OperationLock);
        TNSBitVector        jobs_to_notify = m_JobsToNotify & job_ids;
        if (jobs_to_notify.any()) {
            TNSBitVector::enumerator    en(jobs_to_notify.first());
            CNSTransaction              transaction(this);

            for (; en.valid(); ++en) {
                unsigned int    id = *en;
                CJob            job;

                if (job.Fetch(this, id) == CJob::eJF_Ok) {
                    if (job.ShouldNotifyListener(current_time, m_JobsToNotify))
                        m_NotificationsList.NotifyJobStatus(
                                            job.GetListenerNotifAddr(),
                                            job.GetListenerNotifPort(),
                                            MakeJobKey(id),
                                            CNetScheduleAPI::eDeleted,
                                            job.GetLastEventIndex());

                }
                m_JobsToNotify.set_bit(id, false);
            }
        }

        if (!m_StatusTracker.AnyPending())
            m_NotificationsList.ClearExactGetNotifications();
    }


    return result;
}


void CQueue::TimeLineMove(unsigned int            job_id,
                          const CNSPreciseTime &  old_time,
                          const CNSPreciseTime &  new_time)
{
    if (!job_id || !m_RunTimeLine)
        return;

    CWriteLockGuard guard(m_RunTimeLineLock);
    m_RunTimeLine->MoveObject(old_time.Sec(), new_time.Sec(), job_id);
}


void CQueue::TimeLineAdd(unsigned int            job_id,
                         const CNSPreciseTime &  job_time)
{
    if (!job_id  ||  !m_RunTimeLine  ||  !job_time)
        return;

    CWriteLockGuard guard(m_RunTimeLineLock);
    m_RunTimeLine->AddObject(job_time.Sec(), job_id);
}


void CQueue::TimeLineRemove(unsigned int  job_id)
{
    if (!m_RunTimeLine)
        return;

    CWriteLockGuard guard(m_RunTimeLineLock);
    m_RunTimeLine->RemoveObject(job_id);
}


void CQueue::TimeLineExchange(unsigned int            remove_job_id,
                              unsigned int            add_job_id,
                              const CNSPreciseTime &  new_time)
{
    if (!m_RunTimeLine)
        return;

    CWriteLockGuard guard(m_RunTimeLineLock);
    if (remove_job_id)
        m_RunTimeLine->RemoveObject(remove_job_id);
    if (add_job_id)
        m_RunTimeLine->AddObject(new_time.Sec(), add_job_id);
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
                    deleted_jobs.set_bit(job_id);
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

                // The job might be the one which was given for reading
                // so the garbage should be collected
                m_ReadJobs.set_bit(job_id, false);
                ++m_ReadJobsOps;
            }

            transaction.Commit();
        }}
    }

    if (del_rec > 0) {
        m_StatisticsCounters.CountDBDeletion(del_rec);

        {{
            TNSBitVector::enumerator    en = deleted_jobs.first();
            CFastMutexGuard             guard(m_JobsToDeleteLock);
            for (; en.valid(); ++en) {
                m_JobsToDelete.set_bit(*en, false);
                ++m_JobsToDeleteOps;
            }

            if (m_JobsToDeleteOps >= 1000000) {
                m_JobsToDeleteOps = 0;
                m_JobsToDelete.optimize(0, TNSBitVector::opt_free_0);
            }
        }}

        CFastMutexGuard     guard(m_OperationLock);
        if (m_ReadJobsOps >= 1000000) {
            m_ReadJobsOps = 0;
            m_ReadJobs.optimize(0, TNSBitVector::opt_free_0);
        }
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
    if (aff_dict_size <
            (m_AffinityHighMarkPercentage / 100.0) * m_MaxAffinities) {
        // Here: check the percentage of the affinities that have no references
        // to them
        unsigned int    candidates_size =
                                    m_AffinityRegistry.CheckRemoveCandidates();

        if (candidates_size <
                (m_AffinityDirtPercentage / 100.0) * m_MaxAffinities)
            // The number of candidates to be deleted is low
            return 0;

        del_limit = m_AffinityLowRemoval;
    }


    // Here: need to delete affinities from the memory and DB
    CFastMutexGuard     guard(m_OperationLock);
    CNSTransaction      transaction(this);

    unsigned int        del_count =
                                m_AffinityRegistry.CollectGarbage(del_limit);
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


void  CQueue::StaleNodes(const CNSPreciseTime &  current_time)
{
    // Clears the worker nodes affinities if the workers are inactive for
    // the configured timeout
    CFastMutexGuard     guard(m_OperationLock);
    m_ClientsRegistry.StaleNodes(current_time, 
                                 m_WNodeTimeout, m_ReaderTimeout, m_Log);
}


void CQueue::PurgeBlacklistedJobs(void)
{
    m_ClientsRegistry.GCBlacklistedJobs(m_StatusTracker, eGet);
    m_ClientsRegistry.GCBlacklistedJobs(m_StatusTracker, eRead);
}


void  CQueue::PurgeClientRegistry(const CNSPreciseTime &  current_time)
{
    CFastMutexGuard     guard(m_OperationLock);
    m_ClientsRegistry.Purge(current_time,
                            m_ClientRegistryTimeoutWorkerNode,
                            m_ClientRegistryMinWorkerNodes,
                            m_ClientRegistryTimeoutAdmin,
                            m_ClientRegistryMinAdmins,
                            m_ClientRegistryTimeoutSubmitter,
                            m_ClientRegistryMinSubmitters,
                            m_ClientRegistryTimeoutReader,
                            m_ClientRegistryMinReaders,
                            m_ClientRegistryTimeoutUnknown,
                            m_ClientRegistryMinUnknowns, m_Log);
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


string CQueue::PrintJobDbStat(const CNSClientId &  client,
                              unsigned int         job_id)
{
    m_ClientsRegistry.MarkAsAdmin(client);

    // Check first that the job has not been deleted yet
    {{
        CFastMutexGuard     guard(m_JobsToDeleteLock);
        if (m_JobsToDelete.get_bit(job_id))
            return "";
    }}

    CJob                job;
    {{
        CFastMutexGuard     guard(m_OperationLock);

        m_QueueDbBlock->job_db.SetTransaction(NULL);
        m_QueueDbBlock->events_db.SetTransaction(NULL);
        m_QueueDbBlock->job_info_db.SetTransaction(NULL);

        CJob::EJobFetchResult   res = job.Fetch(this, job_id);
        if (res != CJob::eJF_Ok)
            return "";
    }}

    return job.Print(*this, m_AffinityRegistry, m_GroupRegistry);
}


string CQueue::PrintAllJobDbStat(const CNSClientId &         client,
                                 const string &              group,
                                 const string &              aff_token,
                                 const vector<TJobStatus> &  job_statuses,
                                 unsigned int                start_after_job_id,
                                 unsigned int                count,
                                 bool                        logging)
{
    m_ClientsRegistry.MarkAsAdmin(client);

    // Form a bit vector of all jobs to dump
    vector<TJobStatus>      statuses;
    TNSBitVector            jobs_to_dump;

    if (job_statuses.empty()) {
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
        // The user specified statuses explicitly
        // The list validity is checked by the caller.
        statuses = job_statuses;
    }


    {{
        CFastMutexGuard     guard(m_OperationLock);
        jobs_to_dump = m_StatusTracker.GetJobs(statuses);

        // Check if a certain group has been specified
        if (!group.empty()) {
            try {
                jobs_to_dump &= m_GroupRegistry.GetJobs(group);
            } catch (...) {
                jobs_to_dump.clear();
                if (logging)
                    ERR_POST(Warning << "Job group '" + group +
                                        "' is not found. No jobs to dump.");
            }
        }

        if (!aff_token.empty()) {
            unsigned int    aff_id = m_AffinityRegistry.GetIDByToken(aff_token);
            if (aff_id == 0) {
                jobs_to_dump.clear();
                if (logging)
                    ERR_POST(Warning << "Affinity '" + aff_token +
                                        "' is not found. No jobs to dump.");
            } else
                jobs_to_dump &= m_AffinityRegistry.GetJobsWithAffinity(aff_id);
        }
    }}

    return x_DumpJobs(jobs_to_dump, start_after_job_id, count);
}


string CQueue::x_DumpJobs(const TNSBitVector &    jobs_to_dump,
                          unsigned int            start_after_job_id,
                          unsigned int            count)
{
    if (!jobs_to_dump.any())
        return "";

    // Skip the jobs which should not be dumped
    TNSBitVector::enumerator    en(jobs_to_dump.first());
    while (en.valid() && *en <= start_after_job_id)
        ++en;

    // Identify the required buffer size for jobs
    size_t      buffer_size = m_DumpBufferSize;
    if (count != 0 && count < buffer_size)
        buffer_size = count;

    string  result;
    result.reserve(2048*buffer_size);

    {{
        vector<CJob>    buffer(buffer_size);
        size_t          read_jobs = 0;
        size_t          printed_count = 0;

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
                result += "\n" + buffer[index].Print(*this,
                                                     m_AffinityRegistry,
                                                     m_GroupRegistry) +
                          "OK:GC erase time: " +
                          NS_FormatPreciseTime(
                                  m_GCRegistry.GetLifetime(
                                                buffer[index].GetId())) +
                          "\n";
            }

            if (count != 0)
                if (printed_count >= count)
                    break;

            read_jobs = 0;
        }
    }}

    return result;
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


string CQueue::MakeJobKey(unsigned int  job_id) const
{
    if (m_ScrambleJobKeys)
        return m_KeyGenerator.GenerateCompoundID(job_id,
                                                 m_Server->GetCompoundIDPool());
    return m_KeyGenerator.Generate(job_id);
}


void CQueue::TouchClientsRegistry(CNSClientId &  client,
                                  bool &         client_was_found,
                                  bool &         session_was_reset,
                                  string &       old_session,
                                  bool &         had_wn_pref_affs,
                                  bool &         had_reader_pref_affs)
{
    TNSBitVector        running_jobs;
    TNSBitVector        reading_jobs;

    {{
        CFastMutexGuard     guard(m_OperationLock);
        m_ClientsRegistry.Touch(client, running_jobs, reading_jobs,
                                client_was_found, session_was_reset,
                                old_session, had_wn_pref_affs,
                                had_reader_pref_affs);
    }}


    if (session_was_reset) {
        if (running_jobs.any())
            x_ResetRunningDueToNewSession(client, running_jobs);
        if (reading_jobs.any())
            x_ResetReadingDueToNewSession(client, reading_jobs);
    }
}


void CQueue::MarkClientAsAdmin(const CNSClientId &  client)
{
    m_ClientsRegistry.MarkAsAdmin(client);
}


void CQueue::RegisterSocketWriteError(const CNSClientId &  client)
{
    CFastMutexGuard     guard(m_OperationLock);
    m_ClientsRegistry.RegisterSocketWriteError(client);
}


// Moves the job to Pending/Failed or to Done/ReadFailed
// when event event_type has come
TJobStatus  CQueue::x_ResetDueTo(const CNSClientId &     client,
                                 unsigned int            job_id,
                                 const CNSPreciseTime &  current_time,
                                 TJobStatus              status_from,
                                 CJobEvent::EJobEvent    event_type)
{
    CJob                job;
    TJobStatus          new_status;

    CFastMutexGuard     guard(m_OperationLock);

    {{
        CNSTransaction      transaction(this);

        if (job.Fetch(this, job_id) != CJob::eJF_Ok) {
            ERR_POST("Cannot fetch job to reset it due to " <<
                     CJobEvent::EventToString(event_type) <<
                     ". Job: " << DecorateJob(job_id));
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
            if (job.GetReadCount() > m_ReadFailedRetries)
                new_status = CNetScheduleAPI::eReadFailed;
            else
                new_status = job.GetStatusBeforeReading();
            m_ReadJobs.set_bit(job_id, false);
            ++m_ReadJobsOps;
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

    // Count the transition and do a performance logging
    if (event_type == CJobEvent::eClear)
        m_StatisticsCounters.CountTransition(status_from, new_status,
                                             CStatisticsCounters::eClear);
    else
        // It is a new session case
        m_StatisticsCounters.CountTransition(status_from, new_status,
                                             CStatisticsCounters::eNewSession);
    g_DoPerfLogging(*this, job, 200);

    m_GCRegistry.UpdateLifetime(job_id,
                                job.GetExpirationTime(m_Timeout,
                                                      m_RunTimeout,
                                                      m_ReadTimeout,
                                                      m_PendingTimeout,
                                                      current_time));

    // remove the job from the time line
    TimeLineRemove(job_id);

    // Notify those who wait for the jobs if needed
    if (new_status == CNetScheduleAPI::ePending &&
        m_PauseStatus == eNoPause)
        m_NotificationsList.Notify(job_id, job.GetAffinityId(),
                                   m_ClientsRegistry,
                                   m_AffinityRegistry,
                                   m_GroupRegistry,
                                   m_NotifHifreqPeriod,
                                   m_HandicapTimeout,
                                   eGet);
    // Notify readers if they wait for jobs
    if (new_status == CNetScheduleAPI::eDone ||
        new_status == CNetScheduleAPI::eFailed ||
        new_status == CNetScheduleAPI::eCanceled)
        if (!m_ReadJobs.get_bit(job_id)) {
            m_GCRegistry.UpdateReadVacantTime(job_id, current_time);
            m_NotificationsList.Notify(job_id, job.GetAffinityId(),
                                       m_ClientsRegistry,
                                       m_AffinityRegistry,
                                       m_GroupRegistry,
                                       m_NotifHifreqPeriod,
                                       m_HandicapTimeout,
                                       eRead);
        }

    if (job.ShouldNotifyListener(current_time, m_JobsToNotify))
        m_NotificationsList.NotifyJobStatus(job.GetListenerNotifAddr(),
                                            job.GetListenerNotifPort(),
                                            MakeJobKey(job_id),
                                            job.GetStatus(),
                                            job.GetLastEventIndex());

    return new_status;
}


void CQueue::x_ResetRunningDueToClear(const CNSClientId &   client,
                                      const TNSBitVector &  jobs)
{
    CNSPreciseTime  current_time = CNSPreciseTime::Current();
    for (TNSBitVector::enumerator  en(jobs.first()); en.valid(); ++en) {
        try {
            x_ResetDueTo(client, *en, current_time,
                         CNetScheduleAPI::eRunning, CJobEvent::eClear);
        } catch (...) {
            ERR_POST("Error resetting a running job when worker node is "
                     "cleared. Job: " << DecorateJob(*en));
        }
    }
}


void CQueue::x_ResetReadingDueToClear(const CNSClientId &   client,
                                      const TNSBitVector &  jobs)
{
    CNSPreciseTime  current_time = CNSPreciseTime::Current();
    for (TNSBitVector::enumerator  en(jobs.first()); en.valid(); ++en) {
        try {
            x_ResetDueTo(client, *en, current_time,
                         CNetScheduleAPI::eReading, CJobEvent::eClear);
        } catch (...) {
            ERR_POST("Error resetting a reading job when worker node is "
                     "cleared. Job: " << DecorateJob(*en));
        }
    }
}


void CQueue::x_ResetRunningDueToNewSession(const CNSClientId &   client,
                                           const TNSBitVector &  jobs)
{
    CNSPreciseTime  current_time = CNSPreciseTime::Current();
    for (TNSBitVector::enumerator  en(jobs.first()); en.valid(); ++en) {
        try {
            x_ResetDueTo(client, *en, current_time,
                         CNetScheduleAPI::eRunning, CJobEvent::eSessionChanged);
        } catch (...) {
            ERR_POST("Error resetting a running job when worker node "
                     "changed session. Job: " << DecorateJob(*en));
        }
    }
}


void CQueue::x_ResetReadingDueToNewSession(const CNSClientId &   client,
                                           const TNSBitVector &  jobs)
{
    CNSPreciseTime  current_time = CNSPreciseTime::Current();
    for (TNSBitVector::enumerator  en(jobs.first()); en.valid(); ++en) {
        try {
            x_ResetDueTo(client, *en, current_time,
                         CNetScheduleAPI::eReading, CJobEvent::eSessionChanged);
        } catch (...) {
            ERR_POST("Error resetting a reading job when worker node "
                     "changed session. Job: " << DecorateJob(*en));
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
                                   bool                  new_format,
                                   const string &        group)
{
    // Add to the notification list and save the wait port
    m_NotificationsList.RegisterListener(client, port, timeout,
                                         wnode_aff, any_aff,
                                         exclusive_new_affinity, new_format,
                                         group, eGet);
    if (client.IsComplete())
        m_ClientsRegistry.SetNodeWaiting(client, port,
                                         aff_ids, eGet);
    return;
}


void
CQueue::x_RegisterReadListener(const CNSClientId &   client,
                               unsigned short        port,
                               unsigned int          timeout,
                               const TNSBitVector &  aff_ids,
                               bool                  reader_aff,
                               bool                  any_aff,
                               bool                  exclusive_new_affinity,
                               const string &        group)
{
    // Add to the notification list and save the wait port
    m_NotificationsList.RegisterListener(client, port, timeout,
                                         reader_aff, any_aff,
                                         exclusive_new_affinity, true,
                                         group, eRead);
    m_ClientsRegistry.SetNodeWaiting(client, port, aff_ids, eRead);
}


bool CQueue::x_UnregisterGetListener(const CNSClientId &  client,
                                     unsigned short       port)
{
    if (client.IsComplete())
        return m_ClientsRegistry.CancelWaiting(client, eGet);

    if (port > 0) {
        m_NotificationsList.UnregisterListener(client, port, eGet);
        return true;
    }
    return false;
}


void CQueue::PrintStatistics(size_t &  aff_count) const
{
    CStatisticsCounters counters_copy = m_StatisticsCounters;

    // Do not print the server wide statistics the very first time
    CNSPreciseTime      current = CNSPreciseTime::Current();

    if (double(m_StatisticsCountersLastPrintedTimestamp) == 0.0) {
        m_StatisticsCountersLastPrinted = counters_copy;
        m_StatisticsCountersLastPrintedTimestamp = current;
        return;
    }

    // Calculate the delta since the last time
    CNSPreciseTime  delta = current - m_StatisticsCountersLastPrintedTimestamp;

    CRef<CRequestContext>   ctx;
    ctx.Reset(new CRequestContext());
    ctx->SetRequestID();


    CDiagContext &      diag_context = GetDiagContext();

    diag_context.SetRequestContext(ctx);
    CDiagContext_Extra      extra = diag_context.PrintRequestStart();

    size_t      affinities = m_AffinityRegistry.size();
    aff_count += affinities;

    // The member is called only if there is a request context
    extra.Print("_type", "statistics_thread")
         .Print("queue", GetQueueName())
         .Print("time_interval", NS_FormatPreciseTimeAsSec(delta))
         .Print("affinities", affinities)
         .Print("pending", CountStatus(CNetScheduleAPI::ePending))
         .Print("running", CountStatus(CNetScheduleAPI::eRunning))
         .Print("canceled", CountStatus(CNetScheduleAPI::eCanceled))
         .Print("failed", CountStatus(CNetScheduleAPI::eFailed))
         .Print("done", CountStatus(CNetScheduleAPI::eDone))
         .Print("reading", CountStatus(CNetScheduleAPI::eReading))
         .Print("confirmed", CountStatus(CNetScheduleAPI::eConfirmed))
         .Print("readfailed", CountStatus(CNetScheduleAPI::eReadFailed));
    counters_copy.PrintTransitions(extra);
    counters_copy.PrintDelta(extra, m_StatisticsCountersLastPrinted);
    extra.Flush();

    ctx->SetRequestStatus(CNetScheduleHandler::eStatus_OK);
    diag_context.PrintRequestStop();
    ctx.Reset();
    diag_context.SetRequestContext(NULL);

    m_StatisticsCountersLastPrinted = counters_copy;
    m_StatisticsCountersLastPrintedTimestamp = current;
}


unsigned int CQueue::GetJobsToDeleteCount(void) const
{
    CFastMutexGuard     jtd_guard(m_JobsToDeleteLock);
    return m_JobsToDelete.count();
}


string CQueue::PrintTransitionCounters(void) const
{
    return m_StatisticsCounters.PrintTransitions() +
           "OK:garbage_jobs: " +
           NStr::NumericToString(GetJobsToDeleteCount()) + "\n"
           "OK:affinity_registry_size: " +
           NStr::NumericToString(m_AffinityRegistry.size()) + "\n"
           "OK:client_registry_size: " +
           NStr::NumericToString(m_ClientsRegistry.size()) + "\n";
}


void CQueue::GetJobsPerState(const string &    group_token,
                             const string &    aff_token,
                             size_t *          jobs,
                             vector<string> &  warnings) const
{
    TNSBitVector        group_jobs;
    TNSBitVector        aff_jobs;
    CFastMutexGuard     guard(m_OperationLock);

    if (!group_token.empty()) {
        try {
            group_jobs = m_GroupRegistry.GetJobs(group_token);
        } catch (...) {
            warnings.push_back("eGroupNotFound:job group " + group_token +
                               " is not found");
        }
    }
    if (!aff_token.empty()) {
        unsigned int  aff_id = m_AffinityRegistry.GetIDByToken(aff_token);
        if (aff_id == 0)
            warnings.push_back("eAffinityNotFound:affinity " + aff_token +
                               " is not found");
        else
            aff_jobs = m_AffinityRegistry.GetJobsWithAffinity(aff_id);
    }

    if (!warnings.empty())
        return;

    for (size_t  index(0); index < g_ValidJobStatusesSize; ++index) {
        TNSBitVector  candidates =
                        m_StatusTracker.GetJobs(g_ValidJobStatuses[index]);

        if (!group_token.empty())
            candidates &= group_jobs;
        if (!aff_token.empty())
            candidates &= aff_jobs;

        jobs[index] = candidates.count();
    }
}


string CQueue::PrintJobsStat(const string &    group_token,
                             const string &    aff_token,
                             vector<string> &  warnings) const
{
    size_t              total = 0;
    string              result;
    size_t              jobs_per_state[g_ValidJobStatusesSize];

    GetJobsPerState(group_token, aff_token, jobs_per_state, warnings);

    // Warnings could be about non existing affinity or group. If so there are
    // no jobs to be printed.
    if (warnings.empty()) {
        for (size_t  index(0); index < g_ValidJobStatusesSize; ++index) {
            result += "OK:" +
                      CNetScheduleAPI::StatusToString(g_ValidJobStatuses[index]) +
                      ": " + NStr::NumericToString(jobs_per_state[index]) + "\n";
            total += jobs_per_state[index];
        }
        result += "OK:Total: " + NStr::NumericToString(total) + "\n";
    }
    return result;
}


unsigned int CQueue::CountActiveJobs(void) const
{
    vector<CNetScheduleAPI::EJobStatus>     statuses;

    statuses.push_back(CNetScheduleAPI::ePending);
    statuses.push_back(CNetScheduleAPI::eRunning);
    return m_StatusTracker.CountStatus(statuses);
}


void CQueue::SetPauseStatus(const CNSClientId &  client, TPauseStatus  status)
{
    m_ClientsRegistry.MarkAsAdmin(client);

    bool        need_notifications = (status == eNoPause &&
                                      m_PauseStatus != eNoPause);

    m_PauseStatus = status;
    if (need_notifications)
        m_NotificationsList.onQueueResumed(m_StatusTracker.AnyPending());
}


void CQueue::RegisterQueueResumeNotification(unsigned int  address,
                                             unsigned short  port,
                                             bool  new_format)
{
    m_NotificationsList.AddToQueueResumedNotifications(address, port,
                                                       new_format);
}


void CQueue::x_UpdateDB_PutResultNoLock(unsigned                job_id,
                                        const string &          auth_token,
                                        const CNSPreciseTime &  curr,
                                        int                     ret_code,
                                        const string &          output,
                                        CJob &                  job,
                                        const CNSClientId &     client)
{
    if (job.Fetch(this, job_id) != CJob::eJF_Ok)
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "Error fetching job");

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
            ERR_POST(Warning << "Received PUT2 with only "
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
void CQueue::x_UpdateDB_ProvideJobNoLock(const CNSClientId &     client,
                                         const CNSPreciseTime &  curr,
                                         unsigned int            job_id,
                                         ECommandGroup           cmd_group,
                                         CJob &                  job)
{
    CNSTransaction      transaction(this);

    if (job.Fetch(this, job_id) != CJob::eJF_Ok)
        NCBI_THROW(CNetScheduleException, eInternalError, "Error fetching job");

    CJobEvent &     event = job.AppendEvent();
    event.SetTimestamp(curr);
    event.SetNodeAddr(client.GetAddress());
    event.SetClientNode(client.GetNode());
    event.SetClientSession(client.GetSession());

    if (cmd_group == eGet) {
        event.SetStatus(CNetScheduleAPI::eRunning);
        event.SetEvent(CJobEvent::eRequest);
    } else {
        event.SetStatus(CNetScheduleAPI::eReading);
        event.SetEvent(CJobEvent::eRead);
    }

    job.SetLastTouch(curr);
    if (cmd_group == eGet) {
        job.SetStatus(CNetScheduleAPI::eRunning);
        job.SetRunTimeout(kTimeZero);
        job.SetRunCount(job.GetRunCount() + 1);
    } else {
        job.SetStatus(CNetScheduleAPI::eReading);
        job.SetReadTimeout(kTimeZero);
        job.SetReadCount(job.GetReadCount() + 1);
    }

    job.Flush(this);
    transaction.Commit();
}


END_NCBI_SCOPE

