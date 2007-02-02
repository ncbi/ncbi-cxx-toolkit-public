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
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/services/grid_worker.hpp>
#include <connect/services/grid_globals.hpp>
#include <connect/services/grid_debug_context.hpp>
#include <util/thread_pool.hpp>

#include "grid_thread_context.hpp"


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
      //m_JobKey(job.job_id), m_JobInput(job.input),
      m_JobCommitted(eNotCommitted), m_LogRequested(log_requested), 
      m_JobNumber(job_number), m_ThreadContext(NULL), 
      m_ExclusiveJob(job.mask & CNetScheduleAPI::eExclusiveJob)
      //m_Mask(job.mask)
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
        return CNetScheduleAdmin::eShutdownImmidiate;
    return CGridGlobals::GetInstance().GetShutdownLevel();
}

void CWorkerNodeJobContext::Reset(const CNetScheduleJob& job,
                                  unsigned int  job_number)
{
    m_Job.Reset();
    m_Job = job;
    //m_JobKey = job_key;
    //m_JobInput = job_input;
    m_JobNumber = job_number;
    
    //m_JobOutput = "";
    //m_ProgressMsgKey = "";
    m_JobCommitted = eNotCommitted;
    m_InputBlobSize = 0;
    m_ExclusiveJob = m_Job.mask & CNetScheduleAPI::eExclusiveJob;
    //m_Mask = jmask;
}

void CWorkerNodeJobContext::RequestExclusiveMode()
{
    CGridGlobals::GetInstance().SetExclusiveMode(true);
    m_ExclusiveJob = true;
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

static auto_ptr<CGridThreadContext> s_SingleTHreadContext;
static CGridThreadContext& s_GetSingleTHreadContext(CGridWorkerNode& node)
{
    if (!s_SingleTHreadContext.get())
        s_SingleTHreadContext.reset(new CGridThreadContext(node, node.GetCheckStatusPeriod()));
    return *s_SingleTHreadContext;
}

static void s_RunJob(CGridThreadContext& thr_context)
{    
    bool more_jobs = false;
    do {
        more_jobs = false;
        CNetScheduleJob new_job;
    try {
        CRef<IWorkerNodeJob> job(thr_context.GetJob());
        int ret_code = 0;
        try {
            ret_code = job->Do(thr_context.GetJobContext());
        }
        catch (CGridGlobalsException& ex) {
            if (ex.GetErrCode() != 
                CGridGlobalsException::eExclusiveModeIsAlreadySet) 
                throw;            
            if(thr_context.GetJobContext().IsLogRequested()) {
                LOG_POST(CTime(CTime::eCurrent).AsString()
                         << " Job " << thr_context.GetJobContext().GetJobKey() 
                         << " has been returned back to the queue because it " 
                         << "requested an Exclusive Mode but another job is "
                         << "already have the exclusive status.");
            }
        }
        thr_context.CloseStreams();
        int try_count = 0;
        while(1) {
            try {
                if (thr_context.IsJobCommitted()) {
                    more_jobs = thr_context.PutResult(ret_code,new_job);
                }
                else {
                    thr_context.ReturnJob();
                }
                break;
            } catch (CNetServiceException& ex) {
                if (ex.GetErrCode() != CNetServiceException::eTimeout) 
                    throw;
                ERR_POST(CTime(CTime::eCurrent).AsString()
                         << " Communication Error : " << ex.what());
                if (++try_count >= 2)
                    throw;
                SleepMilliSec(1000 + try_count*2000);
            }
        }
    }
    catch (exception& ex) {
        ERR_POST(CTime(CTime::eCurrent).AsString() << " Error in Job execution : "  << ex.what());
        try {
            thr_context.PutFailure(ex.what());
        } catch(exception& ex1) {
            ERR_POST(CTime(CTime::eCurrent).AsString() << " Failed to report an exception: " << ex1.what());
        }
    }
    catch(...) {
        string msg = "Unknown Error in Job execution";
        ERR_POST( CTime(CTime::eCurrent).AsString()<< " " << msg );
        try {
            thr_context.PutFailure(msg);
        } catch(exception& ex) {
            ERR_POST(CTime(CTime::eCurrent).AsString() 
                     << " Failed to report an unknown exception : " << ex.what());
        }
    }
    thr_context.CloseStreams();
    CWorkerNodeJobContext& job_context = thr_context.GetJobContext();
    thr_context.Reset();
    if (more_jobs)
        thr_context.SetJobContext(job_context, new_job);
    } while (more_jobs);


}
void CWorkerNodeRequest::Process(void)
{
    try {
        s_RunJob(x_GetThreadContext());
    } catch (...) {
        m_Context->GetWorkerNode().x_ReturnJob(m_Context->GetJobKey());
        CGridGlobals::GetInstance().
            RequestShutdown(CNetScheduleAdmin::eShutdownImmidiate);
        throw;
    }
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
      m_NSClientFactory(client_factory), 
      m_JobWatcher(job_watcher),
      m_UdpPort(9111), m_MaxThreads(4), m_InitThreads(4),
      m_NSTimeout(30), m_ThreadsPoolTimeout(30), 
      m_LogRequested(false), m_OnHold(false),
      m_HoldSem(0,10000), m_UseEmbeddedStorage(false),
      m_CheckStatusPeriod(2)
{
}


CGridWorkerNode::~CGridWorkerNode()
{
}

void CGridWorkerNode::Start()
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
            ERR_POST(CTime(CTime::eCurrent).AsString() << " " << ex.what());
            CGridGlobals::GetInstance().
                RequestShutdown(CNetScheduleAdmin::eShutdownImmidiate);
            return;
        }
        catch (...) {
            ERR_POST(CTime(CTime::eCurrent).AsString() << " Unknown error");
            CGridGlobals::GetInstance().
                RequestShutdown(CNetScheduleAdmin::eShutdownImmidiate);
            return;
        }
    }

    CNetScheduleJob job;
    bool      job_exists = false;

    while (1) {
        if (CGridGlobals::GetInstance().
            GetShutdownLevel() != CNetScheduleAdmin::eNoShutdown)
            break;
        try {
            
            if (m_MaxThreads > 1) {
                try {
                    m_ThreadsPool->WaitForRoom(m_ThreadsPoolTimeout);
                    /// Then CStdPoolOfThreads is fixed these 2 lines should be removed
                        //if (!m_ThreadsPool->HasImmediateRoom())
                        //continue;
                    //////
                } catch (CBlockingQueueException&) {
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
                        ERR_POST(CTime(CTime::eCurrent).AsString() << " " << ex.what());
                        // that must not happen after CBlockingQueue is fixed
                        _ASSERT(0);
                        x_ReturnJob(job.job_id);
                    }
                }
                else {
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
        } catch (exception& ex) {
            ERR_POST(CTime(CTime::eCurrent).AsString() << " " << ex.what());
            CGridGlobals::GetInstance().
                RequestShutdown(CNetScheduleAdmin::eShutdownImmidiate);
        }
    }
    LOG_POST(CTime(CTime::eCurrent).AsString() << " Shutting down...");   
    if (m_MaxThreads > 1 ) {
        m_ThreadsPool->KillAllThreads(true);
        m_ThreadsPool.reset(0);
    }
    try {
        m_NSReadClient->UnRegisterClient(m_UdpPort);
    } catch (CNetServiceException& e) {
        // if server does not understand this new command just ignore the error
        if (e.GetErrCode() != CNetServiceException::eCommunicationError 
            || NStr::Find(e.what(),"Server error:Unknown request") == NPOS)
            throw;
    }
    m_NSReadClient.reset(0);
    LOG_POST(CTime(CTime::eCurrent).AsString() << " Worker Node has been stopped.");
}

const string& CGridWorkerNode::GetQueueName() const 
{ 
    if (!m_NSReadClient.get())
        if ( !const_cast<CGridWorkerNode*>(this)->x_CreateNSReadClient() )
            return kEmptyStr;
    return m_NSReadClient->GetQueueName(); 
}

const string& CGridWorkerNode::GetClientName() const 
{ 
    if (!m_NSReadClient.get())
        if ( !const_cast<CGridWorkerNode*>(this)->x_CreateNSReadClient() )
            return kEmptyStr;
    return m_NSReadClient->GetClientName(); 
}

const string& CGridWorkerNode::GetServiceName() const
{
    if (!m_NSReadClient.get())
        const_cast<CGridWorkerNode*>(this)->x_CreateNSReadClient();
    return m_NSReadClient->GetServiceName();    
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
        if(m_NSReadClient.get() && !CGridGlobals::GetInstance().IsExclusiveMode()) {
            int try_count = 0;
            try {
                if (IsOnHold()) {
                    if (!m_HoldSem.TryWait(m_NSTimeout)) 
                        return false;
                }
                if ( x_AreMastersBusy()) {
                    job_exists = 
                        m_NSReadClient->WaitJob(job, m_NSTimeout, m_UdpPort,
                                                CNetScheduleExecuter::eNoWaitNotification);
                    if (job_exists && m_OnHold) {
                        x_ReturnJob(job.job_id);
                        return false;
                    }
                    if (job_exists)
                        return true;

                    job_exists = CNetScheduleExecuter::WaitNotification(GetQueueName(),
                                                                        m_NSTimeout,m_UdpPort);
                    if (m_OnHold)
                        return false;                    
                    if (job_exists)
                        job_exists = 
                            m_NSReadClient->GetJob(job, m_UdpPort);

                    if (job_exists && m_OnHold) {
                        x_ReturnJob(job.job_id);
                        return false;
                    }
                    
                } else {
                    SleepSec(m_NSTimeout);
                }
                {
                CFastMutexGuard guard(m_HoldMutex);
                m_HoldSem.TryWait(0,1);
                }
            }
            catch (CGridGlobalsException& ex) {
                if (ex.GetErrCode() != 
                    CGridGlobalsException::eExclusiveModeIsAlreadySet) 
                    throw;            
                if(m_LogRequested) {
                    LOG_POST(CTime(CTime::eCurrent).AsString() << " Job " << job.job_id 
                             << " has been returned back to the queue because it " 
                             << "requested an Exclusive Mode but another job is "
                             << "already have the exclusive status.");
                }
                x_ReturnJob(job.job_id);
            }
            catch (CNetServiceException& ex) {
                if (ex.GetErrCode() != CNetServiceException::eTimeout) 
                    throw;
                ERR_POST(CTime(CTime::eCurrent).AsString() 
                         << " Communication Error : " << ex.what());
                if (++try_count >= 2) {
                    CGridGlobals::GetInstance().
                        RequestShutdown(CNetScheduleAdmin::eShutdownImmidiate);
                }
                SleepMilliSec(1000 + try_count*2000);
            }
        }
    }
    return job_exists;
}

void CGridWorkerNode::x_ReturnJob(const string& job_key)
{
    if(m_NSReadClient.get()) {
        CGridDebugContext* debug_context = CGridDebugContext::GetInstance();
        if (!debug_context || 
            debug_context->GetDebugMode() != CGridDebugContext::eGDC_Execute) {
            m_NSReadClient->ReturnJob(job_key);
        }
    }
}

void CGridWorkerNode::x_FailJob(const string& job_key, const string& reason)
{
    if(m_NSReadClient.get()) {
        CGridDebugContext* debug_context = CGridDebugContext::GetInstance();
        if (!debug_context || 
            debug_context->GetDebugMode() != CGridDebugContext::eGDC_Execute) {
            CNetScheduleJob job(job_key);
            job.error_msg = reason;
            m_NSReadClient->PutFailure(job);
        }
    }
}

bool CGridWorkerNode::x_CreateNSReadClient()
{
    if (m_NSReadClient.get())
        return true;

    try {
        m_NSReadClient.reset(CreateClient());
        try {
            m_NSReadClient->RegisterClient(m_UdpPort);
        } catch (CNetServiceException& e) {
            // if server does not understand this new command just ignore the error
            if (e.GetErrCode() == CNetServiceException::eCommunicationError 
                && NStr::Find(e.what(),"Server error:Unknown request") != NPOS)
                return true;
            else throw;
        }
    } catch (exception& ex) {
        ERR_POST(CTime(CTime::eCurrent).AsString() << " " << ex.what());
        CGridGlobals::GetInstance().
            RequestShutdown(CNetScheduleAdmin::eShutdownImmidiate);
        return false;
    } catch (...) {
        ERR_POST(CTime(CTime::eCurrent).AsString() << " Unknown error");
        CGridGlobals::GetInstance().
            RequestShutdown(CNetScheduleAdmin::eShutdownImmidiate);
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
        if (m_NSReadClient.get()) {
            os << m_NSReadClient->GetQueueName()  <<";" 
               << m_NSReadClient->GetServiceName();
        }
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
            ERR_POST( CTime(CTime::eCurrent).AsString() 
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
            ERR_POST( CTime(CTime::eCurrent).AsString() 
                      << " Worker Node @ " << it->host << ":" << it->port 
                      << " returned unknown replay : " << replay);
        }
    }
    return true;
}

void CGridWorkerNode::PutOnHold(bool hold) 
{ 
    CFastMutexGuard guard(m_HoldMutex);
    m_OnHold = hold; 
    if (!m_OnHold)
        m_HoldSem.Post();
}

bool CGridWorkerNode::IsOnHold() const 
{ 
    CFastMutexGuard guard(m_HoldMutex);
    return m_OnHold; 
}

INSCWrapper* CGridWorkerNode::CreateClient()
{
    CFastMutexGuard guard(m_NSClientFactoryMutex);
    if ( !m_SharedNSClient.get() ) {
        m_SharedNSClient.reset(m_NSClientFactory.CreateInstance());
        m_SharedNSClient->SetProgramVersion(m_JobFactory.GetJobVersion());
        //        m_SharedNSClient->ActivateRequestRateControl(false);
        m_SharedNSClient->SetConnMode(INetServiceAPI::eKeepConnection);
    }
    return new CNSCWrapperShared(m_SharedNSClient->GetExecuter(), 
                                 m_SharedClientMutex);
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.60  2006/11/30 20:44:57  didenko
 * Got rid of SEGV
 *
 * Revision 1.59  2006/11/30 15:33:33  didenko
 * Moved to a new log system
 *
 * Revision 1.58  2006/08/03 19:33:10  didenko
 * Added auto_shutdown_if_idle config file paramter
 * Added current date to messages in the log file.
 *
 * Revision 1.57  2006/07/13 14:27:26  didenko
 * Added access to the job's mask for grid's clients/wnodes
 *
 * Revision 1.56  2006/06/28 16:01:56  didenko
 * Redone job's exlusivity processing
 *
 * Revision 1.55  2006/06/22 15:02:16  didenko
 * Commented out the temporary fix again.
 *
 * Revision 1.54  2006/06/22 13:52:36  didenko
 * Returned back a temporary fix for CStdPoolOfThreads
 * Added check_status_period configuration paramter
 * Fixed exclusive mode checking
 *
 * Revision 1.53  2006/06/21 20:10:37  didenko
 * Commented out previous changes
 *
 * Revision 1.52  2006/06/21 19:37:48  didenko
 * Temporary workaround for a bug in CPoolOfThreads
 *
 * Revision 1.51  2006/05/30 16:41:05  didenko
 * Improved error handling
 *
 * Revision 1.50  2006/05/22 18:11:43  didenko
 * Added an option to fail a job if a remote app returns non zore code
 *
 * Revision 1.49  2006/05/15 15:26:53  didenko
 * Added support for running exclusive jobs
 *
 * Revision 1.48  2006/05/12 15:13:37  didenko
 * Added infinit loop detection mechanism in job executions
 *
 * Revision 1.47  2006/05/10 19:54:21  didenko
 * Added JobDelayExpiration method to CWorkerNodeContext class
 * Added keep_alive_period and max_job_run_time parmerter to the config
 * file of remote_app
 *
 * Revision 1.46  2006/04/12 19:03:49  didenko
 * Renamed parameter "use_embedded_input" to "use_embedded_storage"
 *
 * Revision 1.45  2006/03/15 17:30:12  didenko
 * Added ability to use embedded NetSchedule job's storage as a job's input/output data instead of using it as a NetCache blob key. This reduces network traffic and increases job submittion speed.
 *
 * Revision 1.44  2006/03/07 17:19:37  didenko
 * Perfomance and error handling improvements
 *
 * Revision 1.43  2006/03/03 14:14:59  vasilche
 * Used CSafeStaticRef.
 *
 * Revision 1.42  2006/02/27 14:50:21  didenko
 * Redone an implementation of IBlobStorage interface based on NetCache as a plugin
 *
 * Revision 1.41  2006/02/15 19:48:34  didenko
 * Added new optional config parameter "reuse_job_object" which allows reusing
 * IWorkerNodeJob objects in the jobs' threads instead of creating
 * a new object for each job.
 *
 * Revision 1.40  2006/02/15 15:19:03  didenko
 * Implemented an optional possibility for a worker node to have a permanent connection
 * to a NetSchedule server.
 * To expedite the job exchange between a worker node and a NetSchedule server,
 * a call to CNetScheduleClient::PutResult method is replaced to a
 * call to CNetScheduleClient::PutResultGetJob method.
 *
 * Revision 1.39  2006/02/01 19:10:22  didenko
 * Cosmetics
 *
 * Revision 1.38  2006/02/01 19:06:01  didenko
 * Improved main loop processing
 *
 * Revision 1.37  2006/02/01 16:39:01  didenko
 * Added Idle Task facility to the Grid Worker Node Framework
 *
 * Revision 1.36  2006/01/18 17:47:42  didenko
 * Added JobWatchers mechanism
 * Reimplement worker node statistics as a JobWatcher
 * Added JobWatcher for diag stream
 * Fixed a problem with PutProgressMessage method of CWorkerNodeThreadContext class
 *
 * Revision 1.35  2005/12/20 17:26:22  didenko
 * Reorganized netschedule storage facility.
 * renamed INetScheduleStorage to IBlobStorage and moved it to corelib
 * renamed INetScheduleStorageFactory to IBlobStorageFactory and moved it to corelib
 * renamed CNetScheduleNSStorage_NetCache to CBlobStorage_NetCache and moved it
 * to separate files
 * Moved CNetScheduleClientFactory to separate files
 *
 * Revision 1.34  2005/10/26 16:37:44  didenko
 * Added for non-blocking read for netschedule storage
 *
 * Revision 1.33  2005/09/30 14:58:56  didenko
 * Added optional parameter to PutProgressMessage methods which allows
 * sending progress messages regardless of the rate control.
 *
 * Revision 1.32  2005/08/10 15:51:22  didenko
 * Don't forget to close thread's streams then an execption is thrown from a job
 *
 * Revision 1.31  2005/07/07 15:05:14  didenko
 * Corrected handling of localhost from the admin_hosts configuration parameter
 *
 * Revision 1.30  2005/06/20 17:54:16  didenko
 * Fixed exception handling
 *
 * Revision 1.29  2005/05/27 14:46:06  didenko
 * Fixed a worker node statistics
 *
 * Revision 1.28  2005/05/23 15:51:54  didenko
 * Moved grid_control_thread.hpp grid_debug_context.hpp to
 * include/connect/service
 *
 * Revision 1.27  2005/05/19 15:15:24  didenko
 * Added admin_hosts parameter to worker nodes configurations
 *
 * Revision 1.26  2005/05/16 14:20:55  didenko
 * Added master/slave dependances between worker nodes.
 *
 * Revision 1.25  2005/05/13 13:41:42  didenko
 * Changed LOG_POST(Error << ... ) to ERR_POST(...)
 *
 * Revision 1.24  2005/05/13 00:26:18  ucko
 * Re-fix for SGI MIPSpro.
 * Correct more misspellings of "unknown" that seem to have materialized.
 *
 * Revision 1.23  2005/05/12 18:14:33  didenko
 * Cosmetics
 *
 * Revision 1.22  2005/05/12 16:20:42  ucko
 * Use portable DEFINE_CLASS_STATIC_MUTEX macro.
 * Tweak CGridWorkerNode::Start to build with SGI's compiler, which is
 * fussy about use of volatile members.
 * Fix some typos (misspellings in API left alone for now, though).
 *
 * Revision 1.21  2005/05/12 14:52:05  didenko
 * Added a worker node build time and start time to the statistic
 *
 * Revision 1.20  2005/05/11 18:57:39  didenko
 * Added worker node statictics
 *
 * Revision 1.19  2005/05/10 14:14:33  didenko
 * Added blob caching
 *
 * Revision 1.18  2005/05/06 13:08:06  didenko
 * Added check for a job cancelation in the GetShoutdownLevel method
 *
 * Revision 1.17  2005/05/05 15:57:35  didenko
 * Minor fixes
 *
 * Revision 1.16  2005/05/05 15:18:51  didenko
 * Added debugging facility to worker nodes
 *
 * Revision 1.15  2005/05/03 19:54:17  didenko
 * Don't start the threads pool then a worker node is running in a single threaded mode.
 *
 * Revision 1.14  2005/05/02 19:48:38  didenko
 * Removed unnecessary checking if the pool of threads is full.
 *
 * Revision 1.13  2005/04/28 18:46:55  didenko
 * Added Request rate control to PutProgressMessage method
 *
 * Revision 1.12  2005/04/27 16:17:20  didenko
 * Fixed logging system for worker node
 *
 * Revision 1.11  2005/04/27 15:16:29  didenko
 * Added rotating log
 * Added optional deamonize
 *
 * Revision 1.10  2005/04/27 14:10:36  didenko
 * Added max_total_jobs parameter to a worker node configuration file
 *
 * Revision 1.9  2005/04/21 19:10:01  didenko
 * Added IWorkerNodeInitContext
 * Added some convenient macros
 *
 * Revision 1.8  2005/04/20 19:25:59  didenko
 * Added support for progress messages passing from a worker node to a client
 *
 * Revision 1.7  2005/04/07 16:47:03  didenko
 * + Program Version checking
 *
 * Revision 1.6  2005/04/05 15:16:20  didenko
 * Fixed a bug that in some cases can lead to a core dump
 *
 * Revision 1.5  2005/03/28 14:40:17  didenko
 * Cosmetics
 *
 * Revision 1.4  2005/03/23 21:26:06  didenko
 * Class Hierarchy restructure
 *
 * Revision 1.3  2005/03/22 21:43:15  didenko
 * Got rid of warnning on Sun WorkShop
 *
 * Revision 1.2  2005/03/22 20:45:13  didenko
 * Got ride from warning on ICC
 *
 * Revision 1.1  2005/03/22 20:18:25  didenko
 * Initial version
 *
 * ===========================================================================
 */
 
