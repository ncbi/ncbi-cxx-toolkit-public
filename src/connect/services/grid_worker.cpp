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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Maxim Didenko, Anatoliy Kuznetsov, Dmitry Kazimirov
 *
 * File Description:
 *    NetSchedule Worker Node implementation
 */

#include <ncbi_pch.hpp>

#include "grid_thread_context.hpp"

#include <connect/services/grid_globals.hpp>
#include <connect/services/grid_debug_context.hpp>
#include <connect/services/netschedule_api_expt.hpp>

#include <connect/ncbi_socket.hpp>

#include <util/thread_pool.hpp>

#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <corelib/request_ctx.hpp>


#define NCBI_USE_ERRCODE_X   ConnServ_WorkerNode


BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
//
//     IWorkerNodeInitContext     --

const IRegistry& IWorkerNodeInitContext::GetConfig() const
{
    NCBI_THROW(CCoreException, eInvalidArg, "Not Implemented.");
}
const CArgs& IWorkerNodeInitContext::GetArgs() const
{
    NCBI_THROW(CCoreException, eInvalidArg, "Not Implemented.");
}
const CNcbiEnvironment& IWorkerNodeInitContext::GetEnvironment() const
{
    NCBI_THROW(CCoreException, eInvalidArg, "Not Implemented.");
}

/////////////////////////////////////////////////////////////////////////////
//
IWorkerNodeJobWatcher::~IWorkerNodeJobWatcher()
{
}

/////////////////////////////////////////////////////////////////////////////
//
//     CWorkerNodeJobContext     --

CWorkerNodeJobContext::CWorkerNodeJobContext(CGridWorkerNode& worker_node,
                                             const CNetScheduleJob&    job,
                                             unsigned int     job_number,
                                             bool             log_requested)
    : m_WorkerNode(worker_node), m_Job(job),
      m_JobCommitted(eNotCommitted), m_LogRequested(log_requested),
      m_JobNumber(job_number), m_ThreadContext(NULL),
      m_ExclusiveJob(job.mask & CNetScheduleAPI::eExclusiveJob)
{
}

CWorkerNodeJobContext::~CWorkerNodeJobContext()
{
}
const string& CWorkerNodeJobContext::GetQueueName() const
{
    return m_WorkerNode.GetQueueName();
}
const string& CWorkerNodeJobContext::GetClientName() const
{
    return m_WorkerNode.GetClientName();
}

CNcbiIstream& CWorkerNodeJobContext::GetIStream(IBlobStorage::ELockMode mode)
{
    _ASSERT(m_ThreadContext);
    return m_ThreadContext->GetIStream(mode);
}
CNcbiOstream& CWorkerNodeJobContext::GetOStream()
{
    _ASSERT(m_ThreadContext);
    return m_ThreadContext->GetOStream();
}

void CWorkerNodeJobContext::PutProgressMessage(const string& msg,
                                               bool send_immediately)
{
    _ASSERT(m_ThreadContext);
    m_ThreadContext->PutProgressMessage(msg, send_immediately);
}

void CWorkerNodeJobContext::SetThreadContext(CGridThreadContext* thr_context)
{
    m_ThreadContext = thr_context;
}

void CWorkerNodeJobContext::SetJobRunTimeout(unsigned time_to_run)
{
    _ASSERT(m_ThreadContext);
    m_ThreadContext->SetJobRunTimeout(time_to_run);
}

void CWorkerNodeJobContext::JobDelayExpiration(unsigned runtime_inc)
{
    _ASSERT(m_ThreadContext);
    m_ThreadContext->JobDelayExpiration(runtime_inc);
}

CNetScheduleAdmin::EShutdownLevel
CWorkerNodeJobContext::GetShutdownLevel(void) const
{
    _ASSERT(m_ThreadContext);
    if (m_ThreadContext->IsJobCanceled())
        return CNetScheduleAdmin::eShutdownImmediate;
    return CGridGlobals::GetInstance().GetShutdownLevel();
}

void CWorkerNodeJobContext::Reset(const CNetScheduleJob& job,
                                  unsigned int  job_number)
{
    m_Job.Reset();
    m_Job = job;
    m_JobNumber = job_number;

    m_JobCommitted = eNotCommitted;
    m_InputBlobSize = 0;
    m_ExclusiveJob = m_Job.mask & CNetScheduleAPI::eExclusiveJob;
}

void CWorkerNodeJobContext::RequestExclusiveMode()
{
    if (!m_ExclusiveJob) {
        if (!m_WorkerNode.x_GetJobGetterSemaphore().TryWait())
            NCBI_THROW(CGridWorkerNodeException, eExclusiveModeIsAlreadySet, "");
    }
    m_ExclusiveJob = true;
}

size_t CWorkerNodeJobContext::GetMaxServerOutputSize() const
{
    return m_WorkerNode.GetServerOutputSize();
}


/////////////////////////////////////////////////////////////////////////////
//
///@internal
static void s_TlsCleanup(CGridThreadContext* p_value, void* /* data */ )
{
    delete p_value;
}
/// @internal
static CStaticTls<CGridThreadContext> s_tls;

///@internal
class CWorkerNodeRequest : public CStdRequest
{
public:
    CWorkerNodeRequest(auto_ptr<CWorkerNodeJobContext> context);
    virtual ~CWorkerNodeRequest(void);

    virtual void Process(void);

private:
    void x_HandleProcessError(exception* ex = NULL);

    auto_ptr<CWorkerNodeJobContext> m_Context;
    CGridThreadContext&             x_GetThreadContext();

};


CWorkerNodeRequest::CWorkerNodeRequest(auto_ptr<CWorkerNodeJobContext> context)
  : m_Context(context)
{
}


CWorkerNodeRequest::~CWorkerNodeRequest(void)
{
}

CGridThreadContext& CWorkerNodeRequest::x_GetThreadContext()
{
    CGridThreadContext* context = s_tls.GetValue();
    if (!context) {
        context = new CGridThreadContext(m_Context->GetWorkerNode(),
            m_Context->GetWorkerNode().GetCheckStatusPeriod());
        s_tls.SetValue(context, s_TlsCleanup);
    }
    context->SetJobContext(*m_Context);
    return *context;
}

static CSafeStaticPtr< auto_ptr<CGridThreadContext> > s_SingleTHreadContext;

static CGridThreadContext& s_GetSingleThreadContext(CGridWorkerNode& node)
{
    if (!s_SingleTHreadContext->get())
        s_SingleTHreadContext->reset(
            new CGridThreadContext(node, node.GetCheckStatusPeriod()));
    return *s_SingleTHreadContext.Get();
}

inline void s_HandleRunJobError(CGridThreadContext& thr_context,
    exception* ex = NULL)
{
    string msg = " Error in Job execution";
    if (ex) {
        msg += ": ";
        msg += ex->what();
    }
    ERR_POST_X(18, thr_context.GetJobContext().GetJobKey() << msg);
    try {
        thr_context.PutFailure(ex ? ex->what() : "Unknown error");
    } catch (std::exception& ex1) {
        ERR_POST_X(19, "Failed to report exception: " <<
            thr_context.GetJobContext().GetJobKey() << " " << ex1.what());
    } catch (...) {
        ERR_POST_X(20, "Failed to report exception while processing " <<
            thr_context.GetJobContext().GetJobKey());
    }
}

class CRequestStateGuard
{
public:
    CRequestStateGuard(CWorkerNodeJobContext& job_context);

    void RequestStart();
    void RequestStop();

    ~CRequestStateGuard();

private:
    CWorkerNodeJobContext& m_JobContext;
};

CRequestStateGuard::CRequestStateGuard(CWorkerNodeJobContext& job_context) :
    m_JobContext(job_context)
{
    CRequestContext& request_context = CDiagContext::GetRequestContext();

    request_context.SetRequestID((int) job_context.GetJobNumber());

    const CNetScheduleJob& job = job_context.GetJob();

    if (!job.client_ip.empty())
        request_context.SetClientIP(job.client_ip);

    if (!job.session_id.empty())
        request_context.SetSessionID(job.session_id);

    request_context.SetAppState(eDiagAppState_RequestBegin);

    GetDiagContext().PrintRequestStart().Print("jid", job.job_id);
}

inline void CRequestStateGuard::RequestStart()
{
    CDiagContext::GetRequestContext().SetAppState(eDiagAppState_Request);
}

inline void CRequestStateGuard::RequestStop()
{
    CDiagContext::GetRequestContext().SetAppState(eDiagAppState_RequestEnd);
}

CRequestStateGuard::~CRequestStateGuard()
{
    CRequestContext& request_context = CDiagContext::GetRequestContext();

    if (!request_context.IsSetRequestStatus())
        request_context.SetRequestStatus(
            m_JobContext.GetCommitStatus() == CWorkerNodeJobContext::eDone &&
                m_JobContext.GetJob().ret_code == 0 ? 200 : 500);

    switch (request_context.GetAppState()) {
    case eDiagAppState_Request:
        request_context.SetAppState(eDiagAppState_RequestEnd);
        /* FALL THROUGH */

    default:
        GetDiagContext().PrintRequestStop();
        request_context.SetAppState(eDiagAppState_NotSet);
        request_context.UnsetSessionID();
        request_context.UnsetClientIP();
    }
}

static void s_RunJob(CGridThreadContext& thr_context)
{
    bool more_jobs;

    CWorkerNodeJobContext& job_context = thr_context.GetJobContext();

    do {
        more_jobs = false;

        CRequestStateGuard request_state_guard(job_context);

        CNetScheduleJob new_job;
        try {
            CRef<IWorkerNodeJob> job(thr_context.GetJob());
            int ret_code = 0;
            try {
                request_state_guard.RequestStart();
                ret_code = job->Do(job_context);
                request_state_guard.RequestStop();
            } catch (CGridWorkerNodeException& ex) {
                if (ex.GetErrCode() !=
                    CGridWorkerNodeException::eExclusiveModeIsAlreadySet)
                    throw;

                if (job_context.IsLogRequested()) {
                    LOG_POST_X(21, "Job " << job_context.GetJobKey() <<
                        " has been returned back to the queue "
                        "because it requested exclusive mode but "
                        "another job already has the exclusive status.");
                }
            }
            thr_context.CloseStreams();
            int try_count = 0;
            for (;;) {
                try {
                    if (thr_context.IsJobCommitted()) {
                        more_jobs = thr_context.PutResult(ret_code,new_job);
                    } else {
                        thr_context.ReturnJob();
                    }
                    break;
                } catch (CNetServiceException& ex) {
                    if (ex.GetErrCode() != CNetServiceException::eTimeout ||
                        ++try_count >= 2)
                        throw;
                    ERR_POST_X(22, "Communication Error : " << ex.what());
                    SleepMilliSec(1000 + try_count*2000);
                }
            }
        }
        catch (exception& ex) {
            s_HandleRunJobError(thr_context, &ex);
        }
        catch (...) {
            s_HandleRunJobError(thr_context);
        }

        thr_context.CloseStreams();
        thr_context.Reset();
        if (more_jobs)
            thr_context.SetJobContext(job_context, new_job);
    } while (more_jobs);
}

void CWorkerNodeRequest::x_HandleProcessError(exception* ex)
{
    string msg = " Error during job run";
    if (ex) {
        msg += ": ";
        msg += ex->what();
    }
    ERR_POST_X(23, msg);
    try {
        m_Context->GetWorkerNode().x_ReturnJob(m_Context->GetJobKey());
    } catch (exception& ex1) {
        ERR_POST_X(24, "Could not return job back to queue: " << ex1.what());
    } catch (...) {
        ERR_POST_X(25, "Could not return job back to queue.");
    }
    CGridGlobals::GetInstance().
        RequestShutdown(CNetScheduleAdmin::eShutdownImmediate);

}
void CWorkerNodeRequest::Process(void)
{
    try { s_RunJob(x_GetThreadContext()); }
    catch (exception& ex) { x_HandleProcessError(&ex);  }
    catch (...) { x_HandleProcessError(); }
}

/////////////////////////////////////////////////////////////////////////////
//
//     CGridWorkerNode       --
CGridWorkerNode::CGridWorkerNode(IWorkerNodeJobFactory&     job_factory,
                                 IBlobStorageFactory&       storage_factory,
                                 INetScheduleClientFactory& client_factory,
                                 IWorkerNodeJobWatcher*     job_watcher)
    : m_JobFactory(job_factory),
      m_NSStorageFactory(storage_factory),
      m_JobWatcher(job_watcher),
      m_UdpPort(9111), m_MaxThreads(4), m_InitThreads(4),
      m_NSTimeout(30), m_ThreadsPoolTimeout(30),
      m_LogRequested(false),
      m_UseEmbeddedStorage(false), m_CheckStatusPeriod(2),
      m_JobGetterSemaphore(1,1)
{
    m_SharedNSClient = client_factory.CreateInstance();
    m_SharedNSClient.SetProgramVersion(m_JobFactory.GetJobVersion());
}


CGridWorkerNode::~CGridWorkerNode()
{
}

void CGridWorkerNode::Run()
{
    _ASSERT(m_MaxThreads > 0);
    if (!x_CreateNSReadClient())
        return;

    if (m_MaxThreads > 1 ) {
        m_ThreadsPool.reset(new CStdPoolOfThreads(m_MaxThreads, 0/*m_MaxThreads-1*/));
        try {
            m_ThreadsPool->Spawn(m_InitThreads);
        }
        catch (exception& ex) {
            ERR_POST_X(26, ex.what());
            CGridGlobals::GetInstance().RequestShutdown(
                CNetScheduleAdmin::eShutdownImmediate);
            return;
        }
        catch (...) {
            ERR_POST_X(27, "Unknown error");
            CGridGlobals::GetInstance().RequestShutdown(
                CNetScheduleAdmin::eShutdownImmediate);
            return;
        }
    }

    CNetScheduleJob job;

    int try_count = 0;
    for (;;) {
        if (CGridGlobals::GetInstance().
            GetShutdownLevel() != CNetScheduleAdmin::eNoShutdown)
            break;
        try {

            if (m_MaxThreads > 1) {
                try {
                    m_ThreadsPool->WaitForRoom(m_ThreadsPoolTimeout);
                } catch (CBlockingQueueException&) {
                    // threaded pool is busy
                    continue;
                }
            }

            if (x_GetNextJob(job)) {
                if (CGridGlobals::GetInstance().
                    GetShutdownLevel() != CNetScheduleAdmin::eNoShutdown) {
                    x_ReturnJob(job.job_id);
                    break;
                }
                int job_number = CGridGlobals::GetInstance().GetNewJobNumber();
                auto_ptr<CWorkerNodeJobContext>
                    job_context(new CWorkerNodeJobContext(*this,
                                                          job,
                                                          job_number,
                                                          m_LogRequested));
                if (m_MaxThreads > 1 ) {
                    CRef<CStdRequest> job_req(
                                    new CWorkerNodeRequest(job_context));
                    try {
                        m_ThreadsPool->AcceptRequest(job_req);
                    } catch (CBlockingQueueException& ex) {
                        ERR_POST_X(28, ex.what());
                        // that must not happen after CBlockingQueue is fixed
                        _ASSERT(0);
                        x_ReturnJob(job.job_id);
                    }
                } else {
                    try {
                        CGridThreadContext& thr_context =
                            s_GetSingleThreadContext(*this);
                        thr_context.SetJobContext(*job_context);
                        s_RunJob(thr_context);
                    } catch (...) {
                        x_ReturnJob(job_context->GetJobKey());
                        throw;
                    }
                }
            }
        } catch (CNetServiceException& ex) {
            ERR_POST_X(40, ex.what());
            if (ex.GetErrCode() != CNetServiceException::eTimeout || ++try_count >= 2) {
                CGridGlobals::GetInstance().RequestShutdown(CNetScheduleAdmin::eShutdownImmediate);
            } else {
                SleepMilliSec(1000 + try_count*2000);
                continue;
            }
        } catch (exception& ex) {
            ERR_POST_X(29, ex.what());
            CGridGlobals::GetInstance().RequestShutdown(CNetScheduleAdmin::eShutdownImmediate);
        } catch (...) {
            ERR_POST_X(30, "Unknown error");
            CGridGlobals::GetInstance().RequestShutdown(CNetScheduleAdmin::eShutdownImmediate);
        }
        try_count = 0;
    }
    LOG_POST_X(31, "Shutting down...");
    if (CGridGlobals::GetInstance().IsForceExitEnabled()) {
        LOG_POST("Force exit");
        _exit(0);
    }
    if (m_MaxThreads > 1 ) {
        try {
            LOG_POST_X(32, "Stopping worker threads...");
            m_ThreadsPool->KillAllThreads(true);
            m_ThreadsPool.reset(0);
        } catch (exception& ex) {
            ERR_POST_X(33, "Could not stop worker threads: " << ex.what());
        } catch (...) {
            ERR_POST_X(34, "Could not stop worker threads: Unknown error");
        }
    }
    try {
        GetNSExecuter().UnRegisterClient(m_UdpPort);
    } catch (CNetServiceException& ex) {
        // if server does not understand this new command just ignore the error
        if (ex.GetErrCode() != CNetServiceException::eCommunicationError
            || NStr::Find(ex.what(),"Server error:Unknown request") == NPOS) {
            ERR_POST_X(35, "Could unregister from NetScehdule services: "
                       << ex.what());
        }
    } catch(exception& ex) {
        ERR_POST_X(36, "Could unregister from NetScehdule services: "
                   << ex.what());
    } catch(...) {
        ERR_POST_X(37, "Could unregister from NetScehdule services: "
            "Unknown error." );
    }

    LOG_POST_X(38, "Worker Node has been stopped.");
}

bool CGridWorkerNode::x_GetNextJob(CNetScheduleJob& job)
{
    bool job_exists = false;

    CGridDebugContext* debug_context = CGridDebugContext::GetInstance();

    if (debug_context &&
        debug_context->GetDebugMode() == CGridDebugContext::eGDC_Execute) {
        job_exists = debug_context->GetNextJob(job.job_id, job.input);
        if (!job_exists) {
            CGridGlobals::GetInstance().
                RequestShutdown(CNetScheduleAdmin::eNormalShutdown);
        }
    } else {
        if (!x_AreMastersBusy()) {
            SleepSec(m_NSTimeout);
            return false;
        }

        if (!x_GetJobGetterSemaphore().TryWait(m_NSTimeout))
            return false;

        job_exists = GetNSExecuter().WaitJob(job, m_NSTimeout, m_UdpPort,
            CNetScheduleExecuter::eNoWaitNotification);

        if (!job_exists) {
            x_GetJobGetterSemaphore().Post();

            job_exists = CNetScheduleExecuter::WaitNotification(GetQueueName(),
                m_NSTimeout, m_UdpPort);

            if (!x_GetJobGetterSemaphore().TryWait())
                return false;

            if (job_exists)
                job_exists = GetNSExecuter().GetJob(job, m_UdpPort);
        }

        if (!job_exists || (job.mask & CNetScheduleAPI::eExclusiveJob) == 0)
            x_GetJobGetterSemaphore().Post();
    }
    return job_exists;
}

void CGridWorkerNode::x_ReturnJob(const string& job_key)
{
    CGridDebugContext* debug_context = CGridDebugContext::GetInstance();
    if (!debug_context || debug_context->GetDebugMode() != CGridDebugContext::eGDC_Execute) {
         GetNSExecuter().ReturnJob(job_key);
    }
}

void CGridWorkerNode::x_FailJob(const string& job_key, const string& reason)
{
    CGridDebugContext* debug_context = CGridDebugContext::GetInstance();
    if (!debug_context ||
            debug_context->GetDebugMode() != CGridDebugContext::eGDC_Execute) {
         CNetScheduleJob job(job_key);
         job.error_msg = reason;
         GetNSExecuter().PutFailure(job);
    }
}

bool CGridWorkerNode::x_CreateNSReadClient()
{
    try {
        GetNSExecuter().RegisterClient(m_UdpPort);
    } catch (CNetServiceException& ex) {
        // if server does not understand this new command just ignore the error
        if (ex.GetErrCode() == CNetServiceException::eCommunicationError
               && NStr::Find(ex.what(),"Server error:Unknown request") != NPOS)
            return true;
        ERR_POST_X(41, ex.what());
        CGridGlobals::GetInstance().RequestShutdown(CNetScheduleAdmin::eShutdownImmediate);
        return false;

    } catch (exception& ex) {
        ERR_POST_X(42, ex.what());
        CGridGlobals::GetInstance().RequestShutdown(CNetScheduleAdmin::eShutdownImmediate);
        return false;
    } catch (...) {
        ERR_POST_X(43, "Unknown error");
        CGridGlobals::GetInstance().RequestShutdown(CNetScheduleAdmin::eShutdownImmediate);
        return false;
    }
    return true;
}

void CGridWorkerNode::SetMasterWorkerNodes(const string& hosts)
{
    vector<string> vhosts;
    NStr::Tokenize(hosts, " ;,", vhosts);
    m_Masters.clear();
    ITERATE(vector<string>, it, vhosts) {
        string host, sport;
        NStr::SplitInTwo(NStr::TruncateSpaces(*it), ":", host, sport);
        if (host.empty() || sport.empty())
            continue;
        try {
            unsigned int port = NStr::StringToUInt(sport);
            m_Masters.insert(SHost(NStr::ToLower(host),port));
        } catch(...) {}
    }
}

size_t CGridWorkerNode::GetServerOutputSize() const
{
    return IsEmeddedStorageUsed() ?
        GetNSClient().GetServerParams().max_output_size : 0;
}

void CGridWorkerNode::SetAdminHosts(const string& hosts)
{
    vector<string> vhosts;
    NStr::Tokenize(hosts, " ;,", vhosts);
    m_AdminHosts.clear();
    ITERATE(vector<string>, it, vhosts) {
        unsigned int ha = CSocketAPI::gethostbyname(*it);
        if (ha != 0)
            m_AdminHosts.insert(ha);
    }
}

bool CGridWorkerNode::IsHostInAdminHostsList(const string& host) const
{
    if (m_AdminHosts.empty())
        return true;
    unsigned int ha = CSocketAPI::gethostbyname(host);
    if (m_AdminHosts.find(ha) != m_AdminHosts.end())
        return true;
    unsigned int ha_lh = CSocketAPI::gethostbyname("");
    if (ha == ha_lh) {
        ha = CSocketAPI::gethostbyname("localhost");
        if (m_AdminHosts.find(ha) != m_AdminHosts.end())
            return true;
    }
    return false;
}

bool CGridWorkerNode::x_AreMastersBusy() const
{
    ITERATE(set<SHost>, it, m_Masters) {
        STimeout tmo = {0, 500};
        CSocket socket(it->host, it->port, &tmo, eOff);
        if (socket.GetStatus(eIO_Open) != eIO_Success)
            continue;

        CNcbiOstrstream os;
        os << GetJobVersion() << endl;
        os << GetNSClient().GetQueueName()  <<";"
           << GetNSClient().GetService().GetServiceName();
        os << endl;
        os << "GETLOAD" << ends;
        if (socket.Write(os.str(), os.pcount()) != eIO_Success) {
            os.freeze(false);
            continue;
        }
        os.freeze(false);
        string reply;
        if (socket.ReadLine(reply) != eIO_Success)
            continue;
        if (NStr::StartsWith(reply, "ERR:")) {
            string msg;
            NStr::Replace(reply, "ERR:", "", msg);
            ERR_POST_X(43, "Worker Node at " << it->host << ":" << it->port <<
                " returned error: " << msg);
        } else if (NStr::StartsWith(reply, "OK:")) {
            string msg;
            NStr::Replace(reply, "OK:", "", msg);
            try {
                int load = NStr::StringToInt(msg);
                if (load > 0)
                    return false;
            } catch (...) {}
        } else {
            ERR_POST_X(44, "Worker Node at " << it->host << ":" << it->port <<
                " returned unknown reply: " << reply);
        }
    }
    return true;
}

bool CGridWorkerNode::IsExclusiveMode()
{
    if (x_GetJobGetterSemaphore().TryWait()) {
        x_GetJobGetterSemaphore().Post();
        return false;
    }
    return true;
}


END_NCBI_SCOPE
