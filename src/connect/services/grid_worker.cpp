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
 * Authors:  Maxim Didenko, Anatoliy Kuznetsov
 *
 * File Description:
 *    NetSchedule Worker Node implementation
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/services/grid_worker.hpp>
#include <connect/services/grid_globals.hpp>
#include <connect/services/grid_debug_context.hpp>
#include <connect/services/netschedule_api_expt.hpp>
#include <connect/services/error_codes.hpp>
#include <util/thread_pool.hpp>

#include "grid_thread_context.hpp"


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
static CSafeStaticRef< CTls<CGridThreadContext> > s_tls;

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
    CGridThreadContext* context = s_tls->GetValue();
    if (!context) {
        context = new CGridThreadContext(m_Context->GetWorkerNode(),
                                         m_Context->GetWorkerNode().GetCheckStatusPeriod());
        s_tls->SetValue(context, s_TlsCleanup);
    }
    context->SetJobContext(*m_Context);
    return *context;
}

static CSafeStaticPtr< auto_ptr<CGridThreadContext> > s_SingleTHreadContext;

static CGridThreadContext& s_GetSingleTHreadContext(CGridWorkerNode& node)
{
    if (!s_SingleTHreadContext->get())
        s_SingleTHreadContext->reset(new CGridThreadContext(node, node.GetCheckStatusPeriod()));
    return *s_SingleTHreadContext.Get();
}

inline void s_HandleRunJobError(CGridThreadContext& thr_context, exception* ex = NULL)
{
    string msg = " Error in Job execution";
    if (ex) msg += string(": ") + ex->what();
    ERR_POST_X(18, CTime(CTime::eCurrent).AsString()
               << " " << thr_context.GetJobContext().GetJobKey() << " "
               << msg);
    try {
        thr_context.PutFailure(ex ? ex->what() : "Unknown error");
    } catch(exception& ex1) {
        ERR_POST_X(19, CTime(CTime::eCurrent).AsString() << " Failed to report an exception: "
                   << thr_context.GetJobContext().GetJobKey() << " " << ex1.what());
    } catch(...) {
        ERR_POST_X(20, CTime(CTime::eCurrent).AsString() << " Failed to report an exception "
                   << " " << thr_context.GetJobContext().GetJobKey() << " " );
    }
}

static void s_RunJob(CGridThreadContext& thr_context)
{
    bool more_jobs;

    CWorkerNodeJobContext& job_context = thr_context.GetJobContext();

    do {
        more_jobs = false;

        SetDiagRequestId((int) job_context.GetWorkerNode().
            IncrementAndGetRequestCounter());

        CNetScheduleJob new_job;
        try {
            CRef<IWorkerNodeJob> job(thr_context.GetJob());
            int ret_code = 0;
            try {
                ret_code = job->Do(job_context);
            } catch (CGridWorkerNodeException& ex) {
                if (ex.GetErrCode() != CGridWorkerNodeException::eExclusiveModeIsAlreadySet)
                    throw;
                if (job_context.IsLogRequested()) {
                    LOG_POST_X(21, CTime(CTime::eCurrent).AsString()
                               << " Job " << job_context.GetJobKey()
                               << " has been returned back to the queue because it "
                               << "requested an Exclusive Mode but another job is "
                               << "already have the exclusive status.");
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
                    if (ex.GetErrCode() != CNetServiceException::eTimeout || ++try_count >= 2)
                        throw;
                    ERR_POST_X(22, CTime(CTime::eCurrent).AsString() << " Communication Error : " << ex.what());
                    SleepMilliSec(1000 + try_count*2000);
                }
            }
        }
        catch (exception& ex) { s_HandleRunJobError(thr_context, &ex); }
        catch(...) { s_HandleRunJobError(thr_context); }

        thr_context.CloseStreams();
        thr_context.Reset();
        if (more_jobs)
            thr_context.SetJobContext(job_context, new_job);
    } while (more_jobs);
}

void CWorkerNodeRequest::x_HandleProcessError(exception* ex)
{
    string msg = " Error during job run";
    if (ex) msg += string(": ") + ex->what();
    ERR_POST_X(23, CTime(CTime::eCurrent).AsString() << msg);
    try {
        m_Context->GetWorkerNode().x_ReturnJob(m_Context->GetJobKey());
    } catch (exception& ex1) {
        ERR_POST_X(24, CTime(CTime::eCurrent).AsString()
                   << " Could not return job back to queue: " << ex1.what());
    } catch (...) {
        ERR_POST_X(25, CTime(CTime::eCurrent).AsString() << " Could not return job back to queue.");
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
    m_RequestCounter.Set(0);
    m_SharedNSClient.reset(client_factory.CreateInstance());
    m_SharedNSClient->SetProgramVersion(m_JobFactory.GetJobVersion());
    m_SharedNSClient->SetConnMode(CNetServiceAPI_Base::eKeepConnection);
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
            ERR_POST_X(26, CTime(CTime::eCurrent).AsString() << " " << ex.what());
            CGridGlobals::GetInstance().RequestShutdown(CNetScheduleAdmin::eShutdownImmediate);
            return;
        }
        catch (...) {
            ERR_POST_X(27, CTime(CTime::eCurrent).AsString() << " Unknown error");
            CGridGlobals::GetInstance().RequestShutdown(CNetScheduleAdmin::eShutdownImmediate);
            return;
        }
    }

    CNetScheduleJob job;
    bool      job_exists = false;

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

            job_exists = x_GetNextJob(job);

            if (job_exists) {
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
                        ERR_POST_X(28, CTime(CTime::eCurrent).AsString() << " " << ex.what());
                        // that must not happen after CBlockingQueue is fixed
                        _ASSERT(0);
                        x_ReturnJob(job.job_id);
                    }
                } else {
                    try {
                        CGridThreadContext& thr_context = s_GetSingleTHreadContext(*this);
                        thr_context.SetJobContext(*job_context);
                        s_RunJob(thr_context);
                    } catch (...) {
                        x_ReturnJob(job_context->GetJobKey());
                        throw;
                    }
                }
            }
        } catch (CNetServiceException& ex) {
            ERR_POST_X(40, CTime(CTime::eCurrent).AsString() << " " << ex.what());
            if (ex.GetErrCode() != CNetServiceException::eTimeout || ++try_count >= 2) {
                CGridGlobals::GetInstance().RequestShutdown(CNetScheduleAdmin::eShutdownImmediate);
            } else {
                SleepMilliSec(1000 + try_count*2000);
                continue;
            }
        } catch (exception& ex) {
            ERR_POST_X(29, CTime(CTime::eCurrent).AsString() << " " << ex.what());
            CGridGlobals::GetInstance().RequestShutdown(CNetScheduleAdmin::eShutdownImmediate);
        } catch (...) {
            ERR_POST_X(30, CTime(CTime::eCurrent).AsString() << " Unknown error");
            CGridGlobals::GetInstance().RequestShutdown(CNetScheduleAdmin::eShutdownImmediate);
        }
        try_count = 0;
    }
    LOG_POST_X(31, CTime(CTime::eCurrent).AsString() << " Shutting down...");
    if (m_MaxThreads > 1 ) {
        try {
            LOG_POST_X(32, CTime(CTime::eCurrent).AsString() << " Stopping worker threads...");
            m_ThreadsPool->KillAllThreads(true);
            m_ThreadsPool.reset(0);
        } catch (exception& ex) {
            ERR_POST_X(33, CTime(CTime::eCurrent).AsString() << " Could not stop worker threads: " << ex.what());
        } catch (...) {
            ERR_POST_X(34, CTime(CTime::eCurrent).AsString() << " Could not stop worker threads: Unknown error");
        }
    }
    try {
        GetNSExecuter().UnRegisterClient(m_UdpPort);
    } catch (CNetServiceException& ex) {
        // if server does not understand this new command just ignore the error
        if (ex.GetErrCode() != CNetServiceException::eCommunicationError
            || NStr::Find(ex.what(),"Server error:Unknown request") == NPOS) {
            ERR_POST_X(35, CTime(CTime::eCurrent).AsString() << " Could unregister from NetScehdule services: "
                       << ex.what());
        }
    } catch(exception& ex) {
        ERR_POST_X(36, CTime(CTime::eCurrent).AsString() << " Could unregister from NetScehdule services: "
                   << ex.what());
    } catch(...) {
        ERR_POST_X(37, CTime(CTime::eCurrent).AsString()
                   << " Could unregister from NetScehdule services: Unknown error." );
    }

    LOG_POST_X(38, CTime(CTime::eCurrent).AsString() << " Worker Node has been stopped.");
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

        if (!x_GetJobGetterSemaphore().TryWait(m_NSTimeout)) {
            ///cerr << "Exclusive Job is still running" <<  endl;
            return false;
        }
        ///cerr << "Get/Wait 1" <<  endl;
        job_exists =
                GetNSExecuter().WaitJob(job, m_NSTimeout, m_UdpPort,
                                        CNetScheduleExecuter::eNoWaitNotification);
        if (!job_exists) {
            ///cerr << "Post (WAIT)  : " << endl;
            x_GetJobGetterSemaphore().Post();

            job_exists = CNetScheduleExecuter::WaitNotification(GetQueueName(),
                                                                m_NSTimeout,m_UdpPort);

            ///cerr << "Before try 2 Get/Wait" <<  endl;
            if (!x_GetJobGetterSemaphore().TryWait()) {
                ///cerr << "Exclusive is already executed" << endl;
                return false;
            }
            ///cerr << "Get/Wait 2" <<  endl;
            if (job_exists) {
                job_exists = GetNSExecuter().GetJob(job, m_UdpPort);
            }
        }
        if (job_exists && (job.mask & CNetScheduleAPI::eExclusiveJob)) {
                ///cerr << "Get Exclusive job (GET/WAIT): " << job.job_id << endl;
        } else {
            ///cerr << "Post (GET/WAIT): " << job.job_id << endl;
            x_GetJobGetterSemaphore().Post();
        }
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
        ERR_POST_X(41, CTime(CTime::eCurrent).AsString() << " " << ex.what());
        CGridGlobals::GetInstance().RequestShutdown(CNetScheduleAdmin::eShutdownImmediate);
        return false;

    } catch (exception& ex) {
        ERR_POST_X(42, CTime(CTime::eCurrent).AsString() << " " << ex.what());
        CGridGlobals::GetInstance().RequestShutdown(CNetScheduleAdmin::eShutdownImmediate);
        return false;
    } catch (...) {
        ERR_POST_X(43, CTime(CTime::eCurrent).AsString() << " Unknown error");
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
           << GetNSClient().GetServiceName();
        os << endl;
        os << "GETLOAD" << ends;
        if (socket.Write(os.str(), os.pcount()) != eIO_Success) {
            os.freeze(false);
            continue;
        }
        os.freeze(false);
        string replay;
        if (socket.ReadLine(replay) != eIO_Success)
            continue;
        if (NStr::StartsWith(replay, "ERR:")) {
            string msg;
            NStr::Replace(replay, "ERR:", "", msg);
            ERR_POST_X(43, CTime(CTime::eCurrent).AsString()
                       << " Worker Node @ " << it->host << ":" << it->port
                       << " returned error : " << msg);
        } else if (NStr::StartsWith(replay, "OK:")) {
            string msg;
            NStr::Replace(replay, "OK:", "", msg);
            try {
                int load = NStr::StringToInt(msg);
                if (load > 0)
                    return false;
            } catch (...) {}
        } else {
            ERR_POST_X(44, CTime(CTime::eCurrent).AsString()
                       << " Worker Node @ " << it->host << ":" << it->port
                       << " returned unknown replay : " << replay);
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
