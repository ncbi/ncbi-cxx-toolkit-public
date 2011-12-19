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
    m_KeyGenerator(server->GetHost(), server->GetPort()),
    m_Log(server->IsLog()),
    m_LogBatchEachJob(server->IsLogBatchEachJob()),
    m_LastAffinityGC(0),
    m_MaxAffinities(server->GetMaxAffinities()),
    m_AffinityHighMarkPercentage(server->GetAffinityHighMarkPercentage()),
    m_AffinityLowMarkPercentage(server->GetAffinityLowMarkPercentage()),
    m_AffinityHighRemoval(server->GetAffinityHighRemoval()),
    m_AffinityLowRemoval(server->GetAffinityLowRemoval()),
    m_AffinityDirtPercentage(server->GetAffinityDirtPercentage()),
    m_NotificationsList(queue_name),
    m_NotifHifreqInterval(0.1),
    m_NotifHifreqPeriod(5),
    m_NotifLofreqMult(50)
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

    m_Timeout    = params.timeout;
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
    m_NotifHifreqInterval  = params.notif_hifreq_interval;
    m_NotifHifreqPeriod    = params.notif_hifreq_period;
    m_NotifLofreqMult      = params.notif_lofreq_mult;

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


unsigned CQueue::LoadStatusMatrix()
{
    // Load the known affinities
    m_AffinityRegistry.LoadAffinityDictionary();

    // scan the queue, load the state machine from DB
    CBDB_FileCursor     cur(m_QueueDbBlock->job_db);

    cur.InitMultiFetch(1024*1024);
    cur.SetCondition(CBDB_FileCursor::eGE);
    cur.From << 0;

    unsigned    recs = 0;

    for (; cur.Fetch() == eBDB_Ok; ) {
        unsigned int    job_id = m_QueueDbBlock->job_db.id;
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
        CFastMutexGuard     guard(m_OperationLock);

        {{
            CNSTransaction      transaction(this);

            aff_id = m_AffinityRegistry.ResolveAffinityToken(aff_token,
                                                             job_id, 0);
            job.SetAffinityId(aff_id);
            job.Flush(this);

            transaction.Commit();
        }}

        m_StatusTracker.SetStatus(job_id, CNetScheduleAPI::ePending);

        // Register the job with the client
        m_ClientsRegistry.AddToSubmitted(client, job_id);

        // Make the decision whether to send or not a notification
        m_NotificationsList.Notify(aff_id,
                                   m_ClientsRegistry,
                                   m_AffinityRegistry,
                                   m_NotifHifreqPeriod);
    }}

    m_StatisticsCounters.CountSubmit(1);
    x_LogSubmit(job, 0, false);
    return job_id;
}


unsigned int  CQueue::SubmitBatch(const CNSClientId &             client,
                                  vector< pair<CJob, string> > &  batch)
{
    unsigned int    batch_size = batch.size();
    unsigned int    job_id = GetNextIdBatch(batch_size);
    TNSBitVector    affinities;

    {{
        unsigned int        job_id_cnt = job_id;
        time_t              curr_time = time(0);

        CFastMutexGuard     guard(m_OperationLock);

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
                    unsigned int    aff_id = m_AffinityRegistry.
                                                ResolveAffinityToken(aff_token,
                                                                     job_id_cnt,
                                                                     0);

                    job.SetAffinityId(aff_id);
                    affinities.set_bit(aff_id, true);
                }
                job.Flush(this);
                ++job_id_cnt;
            }

            transaction.Commit();
        }}

        m_StatusTracker.AddPendingBatch(job_id, job_id + batch_size - 1);
        m_ClientsRegistry.AddToSubmitted(client, job_id, batch_size);

        // Make a decision whether to notify clients or not
        m_NotificationsList.Notify(affinities,
                                   m_ClientsRegistry,
                                   m_AffinityRegistry,
                                   m_NotifHifreqPeriod);
    }}

    m_StatisticsCounters.CountSubmit(batch_size);
    for (size_t  k= 0; k < batch_size; ++k)
        x_LogSubmit(batch[k].first, job_id, true);

    return job_id;
}



TJobStatus  CQueue::PutResultGetJob(const CNSClientId &        client,
                                    // PutResult parameters
                                    unsigned int               done_job_id,
                                    int                        ret_code,
                                    const string *             output,
                                    // GetJob parameters
                                    // in
                                    const list<string> *       aff_list,
                                    bool                       wnode_affinity,
                                    bool                       any_affinity,
                                    // out
                                    CJob *                     new_job)
{
    time_t  curr = time(0);

    TJobStatus      old_status = PutResult(client, curr, done_job_id,
                                           ret_code, output);
    GetJobOrWait(client, 0, 0, curr, aff_list,
                 wnode_affinity, any_affinity, new_job);
    return old_status;
}


static const size_t     k_max_dead_locks = 10;  // max. dead lock repeats

TJobStatus  CQueue::PutResult(const CNSClientId &  client,
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
                x_UpdateDB_PutResultNoLock(job_id, curr,
                                           ret_code, *output, job,
                                           client);
                transaction.Commit();
            }}

            m_StatusTracker.ChangeStatus(this, job_id, CNetScheduleAPI::eDone);
            m_ClientsRegistry.ClearExecuting(job_id);
            TimeLineRemove(job_id);

            if (job.ShouldNotify(curr))
                m_NotificationsList.NotifyJobStatus(job.GetSubmAddr(),
                                                    job.GetSubmNotifPort(),
                                                    MakeKey(job_id));
            return old_status;
        }
        catch (CBDB_ErrnoException &  ex) {
            if (ex.IsDeadLock()) {
                if (++dead_locks < k_max_dead_locks) {
                    ERR_POST("DeadLock repeat in CQueue::PutResult");
                    SleepMilliSec(100);
                    continue;
                }
                ERR_POST("Too many transaction repeats in CQueue::PutResult");
            } else if (ex.IsNoMem()) {
                if (++dead_locks < k_max_dead_locks) {
                    ERR_POST("No resource repeat in CQueue::PutResult");
                    SleepMilliSec(100);
                    continue;
                }
                ERR_POST("Too many transaction repeats in CQueue::PutResult");
            }
            throw;
        }
    }
}


void  CQueue::GetJobOrWait(const CNSClientId &     client,
                           unsigned short          port, // Port the client
                                                         // will wait on
                           unsigned int            timeout, // If timeout != 0 => WGET
                           time_t                  curr,
                           const list<string> *    aff_list,
                           bool                    wnode_affinity,
                           bool                    any_affinity,
                           CJob *                  new_job)
{
    // We need exactly 1 parameter - m_RunTimeout, so we can access it without
    // CQueueParamAccessor

    size_t      dead_locks = 0;                 // dead lock counter

    for (;;) {
        try {
            TNSBitVector        aff_ids;
            CFastMutexGuard     guard(m_OperationLock);

            if (timeout != 0)
                // WGET:
                // The affinities has to be resolved straight away, however at this
                // point it is still unknown that the client will wait for them, so
                // the client ID is passed as 0. Later on the client info will be
                // updated in the affinity registry if the client really waits for
                // this affinities.
                aff_ids = m_AffinityRegistry.ResolveAffinitiesForWaitClient(*aff_list, 0);
            else
                // GET:
                // No need to create aff records if they are not known
                aff_ids = m_AffinityRegistry.GetAffinityIDs(*aff_list);

            x_UnregisterGetListener(client, port);
            for (;;) {
                unsigned int        job_id = x_FindPendingJob(client,
                                                              aff_ids,
                                                              wnode_affinity,
                                                              any_affinity);
                if (!job_id) {
                    if (timeout != 0) {
                        // WGET:
                        // There is no job, so the client might need to be registered
                        // in the waiting list
                        if (port > 0)
                            x_RegisterGetListener(client, port, timeout, aff_ids,
                                                  wnode_affinity, any_affinity);
                    }
                    return;
                }

                if (x_UpdateDB_GetJobNoLock(client, curr,
                                            job_id, *new_job)) {
                    // The job is not expired and successfully read
                    m_StatusTracker.ChangeStatus(this, job_id, CNetScheduleAPI::eRunning);
                    TimeLineAdd(job_id, curr + m_RunTimeout);
                    m_ClientsRegistry.AddToRunning(client, job_id);
                    return;
                }

                // Reset job_id and try again
                new_job->SetId(0);
            }
        }
        catch (CBDB_ErrnoException &  ex) {
            if (ex.IsDeadLock()) {
                if (++dead_locks < k_max_dead_locks) {
                    ERR_POST("DeadLock repeat in CQueue::GetJobOrWait");
                    SleepMilliSec(100);
                    continue;
                }
                ERR_POST("Too many transaction repeats in CQueue::GetJobOrWait");
            } else if (ex.IsNoMem()) {
                if (++dead_locks < k_max_dead_locks) {
                    ERR_POST("No resource repeat in CQueue::GetJobOrWait");
                    SleepMilliSec(100);
                    continue;
                }
                ERR_POST("Too many transaction repeats in CQueue::GetJobOrWait");
            }
            throw;
        }
    }
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
            ERR_POST(Warning << "Client '" << client.GetNode()
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
            ERR_POST(Warning << "Client '" << client.GetNode()
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

            aff_id_to_add.set(aff_id, true );
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
            job.Flush(this);
            transaction.Commit();
        }}

        time_t  exp_time = run_timeout == 0 ? 0 : time_start + run_timeout;

        TimeLineMove(job_id, exp_time, curr + tm);
    }}

    return CNetScheduleAPI::eRunning;
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


TJobStatus  CQueue::ReturnJob(const CNSClientId &     client,
                             unsigned int            job_id)
{
    CJob                job;
    CFastMutexGuard     guard(m_OperationLock);
    TJobStatus          old_status = GetJobStatus(job_id);

    if (old_status != CNetScheduleAPI::eRunning)
        return old_status;

    {{
         CNSTransaction      transaction(this);

        if (job.Fetch(this, job_id) != CJob::eJF_Ok)
            return CNetScheduleAPI::eJobNotFound;


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

    m_StatusTracker.ChangeStatus(this, job_id, CNetScheduleAPI::ePending);
    TimeLineRemove(job_id);
    m_ClientsRegistry.ClearExecuting(job_id);
    m_ClientsRegistry.AddToBlacklist(client, job_id);

    m_NotificationsList.Notify(job.GetAffinityId(), m_ClientsRegistry,
                               m_AffinityRegistry, m_NotifHifreqPeriod);
    return old_status;
}


unsigned int  CQueue::ReadJobFromDB(unsigned int  job_id, CJob &  job)
{
    // Check first that the job has not been deleted yet
    {{
        CFastMutexGuard     guard(m_JobsToDeleteLock);
        if (m_JobsToDelete[job_id])
            return 0;
    }}


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

                if (job.ShouldNotify(current_time))
                    m_NotificationsList.NotifyJobStatus(job.GetSubmAddr(),
                                                        job.GetSubmNotifPort(),
                                                        MakeKey(job_id));
            }

            // There is no need to commit transaction - there were no changes
        }}

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
            job.Flush(this);

            transaction.Commit();
        }}

        m_StatusTracker.ChangeStatus(this, job_id, CNetScheduleAPI::eCanceled);

        TimeLineRemove(job_id);
        if (old_status == CNetScheduleAPI::eRunning)
            m_ClientsRegistry.ClearExecuting(job_id);
        else if (old_status == CNetScheduleAPI::eReading)
            m_ClientsRegistry.ClearReading(job_id);

    }}

    if ((old_status == CNetScheduleAPI::eRunning ||
         old_status == CNetScheduleAPI::ePending) && job.ShouldNotify(current_time))
        m_NotificationsList.NotifyJobStatus(job.GetSubmAddr(),
                                            job.GetSubmNotifPort(),
                                            MakeKey(job_id));

    return old_status;
}


void  CQueue::CancelAllJobs(const CNSClientId &  client)
{
    CJob                                job;
    TNSBitVector                        jobs_to_cancel;
    vector<CNetScheduleAPI::EJobStatus> statuses;
    time_t                              current_time = time(0);

    // All except cancelled
    statuses.push_back(CNetScheduleAPI::ePending);
    statuses.push_back(CNetScheduleAPI::eRunning);
    statuses.push_back(CNetScheduleAPI::eFailed);
    statuses.push_back(CNetScheduleAPI::eDone);
    statuses.push_back(CNetScheduleAPI::eReading);
    statuses.push_back(CNetScheduleAPI::eConfirmed);
    statuses.push_back(CNetScheduleAPI::eReadFailed);

    CFastMutexGuard     guard(m_OperationLock);

    jobs_to_cancel = m_StatusTracker.GetJobs(statuses);

    TNSBitVector::enumerator    en(jobs_to_cancel.first());
    for (; en.valid(); ++en) {
        unsigned int    job_id = *en;
        TJobStatus      old_status = m_StatusTracker.GetStatus(job_id);

        {{
            CNSTransaction              transaction(this);
            if (job.Fetch(this, job_id) != CJob::eJF_Ok) {
                ERR_POST("Cannot fetch job " << MakeKey(job_id) <<
                         " while cancelling all jobs in the queue");
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
            job.Flush(this);

            transaction.Commit();
        }}

        m_StatusTracker.ChangeStatus(this, job_id, CNetScheduleAPI::eCanceled);

        TimeLineRemove(job_id);
        if (old_status == CNetScheduleAPI::eRunning)
            m_ClientsRegistry.ClearExecuting(job_id);
        else if (old_status == CNetScheduleAPI::eReading)
            m_ClientsRegistry.ClearReading(job_id);

        if ((old_status == CNetScheduleAPI::eRunning ||
             old_status == CNetScheduleAPI::ePending) && job.ShouldNotify(current_time))
            m_NotificationsList.NotifyJobStatus(job.GetSubmAddr(),
                                                job.GetSubmNotifPort(),
                                                MakeKey(job_id));
    }

    return;
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
    m_StatusTracker.ChangeStatus(this, job_id, CNetScheduleAPI::eReading);
    m_ClientsRegistry.AddToReading(client, job_id);
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
    CJob                                        job;
    TJobStatus                                  new_status = target_status;
    CStatisticsCounters::ETransitionPathOption  path_option =
                                                    CStatisticsCounters::eNone;
    CFastMutexGuard                             guard(m_OperationLock);
    TJobStatus                                  old_status = GetJobStatus(job_id);

    if (old_status == CNetScheduleAPI::eJobNotFound ||
        old_status != CNetScheduleAPI::eReading)
        return old_status;

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

        job.Flush(this);
        transaction.Commit();
    }}

    TimeLineRemove(job_id);

    m_StatusTracker.SetStatus(job_id, new_status);
    m_StatisticsCounters.CountTransition(CNetScheduleAPI::eReading,
                                         new_status,
                                         path_option);
    return CNetScheduleAPI::eReading;
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

        m_JobsToDelete.set_bit(job_id);
    }}
    TimeLineRemove(job_id);
}


void CQueue::Erase(const TNSBitVector &  job_ids)
{
    CFastMutexGuard     jtd_guard(m_JobsToDeleteLock);

    m_JobsToDelete |= job_ids;
}


void CQueue::OptimizeMem()
{
    m_StatusTracker.OptimizeMem();
}


unsigned CQueue::x_FindPendingJob(const CNSClientId  &  client,
                                  const TNSBitVector &  aff_ids,
                                  bool                  wnode_affinity,
                                  bool                  any_affinity)
{
    TNSBitVector    blacklisted_jobs =
                            m_ClientsRegistry.GetBlacklistedJobs(client);

    if (aff_ids.any()) {
        unsigned int    job_id = x_FindPendingWithAffinity(aff_ids,
                                                           blacklisted_jobs);

        if (job_id != 0)
            return job_id;
    }


    if (wnode_affinity) {
        // Check that I know something about this client
        TNSBitVector    client_aff =
                            m_ClientsRegistry.GetPreferredAffinities(client);

        if (!client_aff.any())
            ERR_POST(Warning << "The client '" << client.GetNode()
                             << "' requests jobs considering the node "
                                "preferred affinities while the node "
                                "preferred affinities list is empty.");

        unsigned int    job_id = x_FindPendingWithAffinity(client_aff,
                                                           blacklisted_jobs);

        if (job_id != 0)
            return job_id;
    }

    if (any_affinity || (!aff_ids.any() && !wnode_affinity))
        return m_StatusTracker.GetJobByStatus(
                                    CNetScheduleAPI::ePending,
                                    blacklisted_jobs);

    return 0;
}


unsigned int
CQueue::x_FindPendingWithAffinity(const TNSBitVector &  aff_ids,
                                  const TNSBitVector &  blacklist_ids)
{
    if (!aff_ids.any())
        return 0;

    TNSBitVector    candidate_jobs =
                         m_AffinityRegistry.GetJobsWithAffinity(aff_ids);
    if (!candidate_jobs.any())
        return 0;

    m_StatusTracker.PendingIntersect(&candidate_jobs);
    candidate_jobs -= blacklist_ids;

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


TJobStatus CQueue::FailJob(const CNSClientId &    client,
                           unsigned int           job_id,
                           const string &         err_msg,
                           const string &         output,
                           int                    ret_code)
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


            CJob::EJobFetchResult   res = job.Fetch(this, job_id);
            if (res != CJob::eJF_Ok) {
                // TODO: Integrity error or job just expired?
                if (m_Log)
                    LOG_POST(Error << "Can not fetch job " << DecorateJobId(job_id));
                return CNetScheduleAPI::eJobNotFound;
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

        if (!rescheduled  &&  job.ShouldNotify(curr)) {
            m_NotificationsList.NotifyJobStatus(job.GetSubmAddr(),
                                                job.GetSubmNotifPort(),
                                                MakeKey(job_id));
        }

        if (rescheduled)
            m_NotificationsList.Notify(job.GetAffinityId(), m_ClientsRegistry,
                                       m_AffinityRegistry, m_NotifHifreqPeriod);

    }}

    return old_status;
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

    unsigned short  wait_port = m_ClientsRegistry.ClearWorkerNode(
                                                        client, this,
                                                        m_AffinityRegistry);
    if (wait_port != 0)
        m_NotificationsList.UnregisterListener(client, wait_port);
    return;
}


void CQueue::NotifyListenersPeriodically(time_t  current_time)
{
    // Triggered from a notification thread only, so check first the
    // configured notification interval
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
            m_NotificationsList.Notify(job.GetAffinityId(), m_ClientsRegistry,
                                       m_AffinityRegistry, m_NotifHifreqInterval);

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


// Checks up to given # of jobs at the given status for expiration and
// marks up to given # of jobs for deletion.
// Returns the # of performed scans, the # of jobs marked for deletion and
// the last scanned job id.
SPurgeAttributes  CQueue::CheckJobsExpiry(time_t             current_time,
                                          SPurgeAttributes   attributes,
                                          TJobStatus         status)
{
    unsigned int    queue_timeout;
    unsigned int    queue_run_timeout;

    {{
        CQueueParamAccessor     qp(*this);

        queue_timeout = qp.GetTimeout();
        queue_run_timeout = qp.GetRunTimeout();
    }}

    TNSBitVector        job_ids;
    CJob                job;
    SPurgeAttributes    result;

    result.job_id = attributes.job_id;
    result.deleted = 0;
    {{
        CFastMutexGuard     guard(m_OperationLock);

        for (result.scans = 1; result.scans <= attributes.scans; ++result.scans) {
            result.job_id = m_StatusTracker.GetNext(status, result.job_id);
            if (result.job_id == 0)
                break;

            {{
                 m_QueueDbBlock->job_db.SetTransaction(NULL);
                 m_QueueDbBlock->events_db.SetTransaction(NULL);
                 m_QueueDbBlock->job_info_db.SetTransaction(NULL);

                if (job.FetchToTestExpiration(this,
                                              result.job_id) != CJob::eJF_Ok) {
                    ERR_POST("Error fetching job " << result.job_id <<
                             ". Memory matrix reports it is in " <<
                             CNetScheduleAPI::StatusToString(status) <<
                             " status but it is not found in the DB.");
                    m_StatusTracker.Erase(result.job_id);
                    continue;
                }
            }}

            // Is the job expired?
            if (job.GetJobExpirationTime(queue_timeout,
                                         queue_run_timeout) <= current_time) {
                m_StatusTracker.Erase(result.job_id);
                job_ids.set_bit(result.job_id);
                ++result.deleted;

                // check if the affinity should also be updated
                if (job.GetAffinityId() != 0)
                    m_AffinityRegistry.RemoveJobFromAffinity(
                                                    result.job_id,
                                                    job.GetAffinityId());

                if (result.deleted >= attributes.deleted)
                    break;
            }
        }
    }}

    if (result.deleted > 0)
        Erase(job_ids);

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


unsigned int  CQueue::DeleteBatch(void)
{
    // Copy the vector with deleted jobs
    TNSBitVector    jobs_to_delete;

    {{
         CFastMutexGuard     guard(m_JobsToDeleteLock);
         jobs_to_delete = m_JobsToDelete;
         m_JobsToDelete.clear();
    }}

    static const size_t         chunk_size = 100;
    unsigned int                del_rec = 0;
    TNSBitVector::enumerator    en = jobs_to_delete.first();

    while (en.valid()) {
        {{
            CFastMutexGuard     guard(m_OperationLock);
            CNSTransaction      transaction(this);

            for (size_t n = 0; en.valid() && n < chunk_size; ++en, ++n) {
                unsigned int    job_id = *en;

                try {
                    m_QueueDbBlock->job_db.id = job_id;
                    m_QueueDbBlock->job_db.Delete();
                    ++del_rec;
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

    m_StatisticsCounters.CountDBDeletion(del_rec);
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
    handler.WriteMessage("OK:", "remote_client_sid: " + job.GetClientSID());
    handler.WriteMessage("OK:", "remote_client_ip: " + job.GetClientIP());
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
                              unsigned                  job_id)
{
    // Check first that the job has not been deleted yet
    {{
        CFastMutexGuard     guard(m_JobsToDeleteLock);
        if (m_JobsToDelete[job_id])
            return 0;
    }}

    size_t              print_count = 0;
    unsigned            queue_run_timeout = GetRunTimeout();
    CJob                job;
    CFastMutexGuard     guard(m_OperationLock);

    m_QueueDbBlock->job_db.SetTransaction(NULL);
    m_QueueDbBlock->events_db.SetTransaction(NULL);
    m_QueueDbBlock->job_info_db.SetTransaction(NULL);

    CJob::EJobFetchResult   res = job.Fetch(this, job_id);
    if (res == CJob::eJF_Ok) {
        handler.WriteMessage("");
        x_PrintJobStat(handler, job, queue_run_timeout);
        ++print_count;
    }

    return print_count;
}


void CQueue::PrintAllJobDbStat(CNetScheduleHandler &   handler)
{
    unsigned            queue_run_timeout = GetRunTimeout();
    CJob                job;

    // Make a copy of the deleted jobs vector
    TNSBitVector        deleted_jobs;
    {{
        CFastMutexGuard     guard(m_JobsToDeleteLock);
        deleted_jobs = m_JobsToDelete;
    }}


    CFastMutexGuard     guard(m_OperationLock);

    m_QueueDbBlock->job_db.SetTransaction(NULL);
    m_QueueDbBlock->events_db.SetTransaction(NULL);
    m_QueueDbBlock->job_info_db.SetTransaction(NULL);

    CQueueEnumCursor    cur(this);

    while (cur.Fetch() == eBDB_Ok) {
        if (job.Fetch(this) == CJob::eJF_Ok) {
            if (!deleted_jobs[job.GetId()]) {
                handler.WriteMessage("");
                x_PrintJobStat(handler, job, queue_run_timeout);
            }
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


void CQueue::TouchClientsRegistry(CNSClientId &  client)
{
    unsigned short      wait_port = m_ClientsRegistry.Touch(client,
                                                            this,
                                                            m_AffinityRegistry);
    if (wait_port != 0)
        m_NotificationsList.UnregisterListener(client, wait_port);
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
    return new_status;
}


void CQueue::ResetRunningDueToClear(const CNSClientId &   client,
                                    const TNSBitVector &  jobs)
{
    time_t      current_time = time(0);
    for (TNSBitVector::enumerator  en(jobs.first()); en.valid(); ++en) {
        try {
            TJobStatus  new_status = x_ResetDueTo(client, *en,
                                                  current_time,
                                                  CNetScheduleAPI::eRunning,
                                                  CJobEvent::eClear);
            if (new_status != CNetScheduleAPI::eJobNotFound)
                m_StatisticsCounters.CountTransition(
                                            CNetScheduleAPI::eRunning,
                                            new_status,
                                            CStatisticsCounters::eClear);
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
            TJobStatus  new_status = x_ResetDueTo(client, *en,
                                                  current_time,
                                                  CNetScheduleAPI::eReading,
                                                  CJobEvent::eClear);
            if (new_status != CNetScheduleAPI::eJobNotFound)
                m_StatisticsCounters.CountTransition(
                                            CNetScheduleAPI::eReading,
                                            new_status,
                                            CStatisticsCounters::eClear);
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
            TJobStatus  new_status = x_ResetDueTo(client, *en,
                                                  current_time,
                                                  CNetScheduleAPI::eRunning,
                                                  CJobEvent::eSessionChanged);
            if (new_status != CNetScheduleAPI::eJobNotFound)
                m_StatisticsCounters.CountTransition(
                                            CNetScheduleAPI::eRunning,
                                            new_status,
                                            CStatisticsCounters::eNewSession);
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
            TJobStatus  new_status = x_ResetDueTo(client, *en,
                                                  current_time,
                                                  CNetScheduleAPI::eReading,
                                                  CJobEvent::eSessionChanged);
            if (new_status != CNetScheduleAPI::eJobNotFound)
                m_StatisticsCounters.CountTransition(
                                            CNetScheduleAPI::eReading,
                                            new_status,
                                            CStatisticsCounters::eNewSession);
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
                                   bool                  any_aff)
{
    // Add to the notification list and save the wait port
    m_NotificationsList.RegisterListener(client, port, timeout,
                                         wnode_aff, any_aff);
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
    // expiration timeout; note: run_timeout is not applicable because the job
    // is guaranteed in the pending state
    if (job.GetJobExpirationTime(m_Timeout, 0) <= curr) {
        // The job has expired, so mark it for deletion
        EraseJob(job_id);

        if (job.GetAffinityId() != 0)
            m_AffinityRegistry.RemoveJobFromAffinity(
                                    job_id, job.GetAffinityId());
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

    job.Flush(this);
    transaction.Commit();

    return true;
}


END_NCBI_SCOPE

