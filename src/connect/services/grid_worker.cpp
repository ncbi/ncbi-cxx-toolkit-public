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
#include <connect/services/grid_worker.hpp>
#include <connect/ncbi_socket.hpp>
#include <util/thread_pool.hpp>

#include "grid_thread_context.hpp"
#include "grid_debug_context.hpp"


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
//     CWorkerNodeJobContext     -- 

CWorkerNodeJobContext* CWorkerNodeJobContext::m_Root = NULL;
DEFINE_CLASS_STATIC_MUTEX(CWorkerNodeJobContext::m_ListMutex);

CWorkerNodeJobContext::CWorkerNodeJobContext(CGridWorkerNode& worker_node,
                                             const string&    job_key,
                                             const string&    job_input,
                                             unsigned int     job_number,
                                             bool             log_requested)
    : m_WorkerNode(worker_node), m_JobKey(job_key), m_JobInput(job_input),
      m_JobCommitted(false), m_LogRequested(log_requested), 
      m_JobNumber(job_number), m_ThreadContext(NULL),
      m_Next(NULL), m_Prev(NULL), m_StartTime(CTime::eCurrent)
{
    CMutexGuard guard(m_ListMutex);
    if (!m_Root)
        m_Root = this;
    else {
        m_Next = m_Root;
        m_Root->m_Prev = this;
        m_Root = this;
    }    
}

CWorkerNodeJobContext::~CWorkerNodeJobContext()
{
    CMutexGuard guard(m_ListMutex);
    if (m_Root == this) {        
        m_Root = m_Root->m_Next;       
        if (m_Root) 
            m_Root->m_Prev = NULL;
    }
    else {
        m_Prev->m_Next = m_Next;
        if (m_Next)
            m_Next->m_Prev = m_Prev;
    }
}

void CWorkerNodeJobContext::CollectStatictics(vector<SJobStat>& stat)
{
    CMutexGuard guard(m_ListMutex);
    CWorkerNodeJobContext* curr = m_Root;
    stat.clear();
    while (curr) {
        SJobStat jstat;
        jstat.job_key = curr->GetJobKey();
        jstat.job_input = curr->GetJobInput();
        jstat.start_time = curr->m_StartTime;
        stat.push_back(jstat);
        curr = curr->m_Next;
    }
}

const string& CWorkerNodeJobContext::GetQueueName() const
{
    return m_WorkerNode.GetQueueName();
}
const string& CWorkerNodeJobContext::GetClientName()const
{
    return m_WorkerNode.GetClientName();
}

CNcbiIstream& CWorkerNodeJobContext::GetIStream()
{   
    _ASSERT(m_ThreadContext);
    return m_ThreadContext->GetIStream();
}
CNcbiOstream& CWorkerNodeJobContext::GetOStream()
{
    _ASSERT(m_ThreadContext);
    return m_ThreadContext->GetOStream();
}

void CWorkerNodeJobContext::PutProgressMessage(const string& msg)
{
    _ASSERT(m_ThreadContext);
    m_ThreadContext->PutProgressMessage(msg);
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

CNetScheduleClient::EShutdownLevel 
CWorkerNodeJobContext::GetShutdownLevel(void) const
{
    _ASSERT(m_ThreadContext);
    if (m_ThreadContext->IsJobCanceled())
        return CNetScheduleClient::eShutdownImmidiate;
    return m_WorkerNode.GetShutdownLevel();
}

/////////////////////////////////////////////////////////////////////////////
// 
///@internal
static void s_TlsCleanup(CGridThreadContext* p_value, void* /* data */ )
{
    delete p_value;
}
/// @internal
static CRef< CTls<CGridThreadContext> > s_tls(new CTls<CGridThreadContext>);

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
        context = new CGridThreadContext(*m_Context);
        s_tls->SetValue(context, s_TlsCleanup);
    } else {
        context->SetJobContext(*m_Context);
    }   
    return *context;
}

static void s_RunJob(CGridThreadContext& thr_context)
{    
    try {
        auto_ptr<IWorkerNodeJob> job( thr_context.CreateJob());
        int ret_code = job->Do(thr_context.GetJobContext());
        thr_context.CloseStreams();
        int try_count = 0;
        while(1) {
            try {
                if (thr_context.IsJobCommitted()) {
                    thr_context.PutResult(ret_code);
                }
                else {
                    thr_context.ReturnJob();
                }
                break;
            } catch (CNetServiceException& ex) {
                ERR_POST("Communication Error : " << ex.what());
                if (++try_count >= 2)
                    throw;
                SleepMilliSec(1000 + try_count*2000);
            }
        }
    }
    catch (exception& ex) {
        ERR_POST("Error in Job execution : "  << ex.what());
        try {
            thr_context.PutFailure(ex.what());
        } catch(exception& ex1) {
            ERR_POST("Failed to report an exception: " << ex1.what());
        }
    }
    catch(...) {
        ERR_POST( "Unknown Error in Job execution" );
        try {
            thr_context.PutFailure("Unknown Error in Job execution");
        } catch(exception& ex) {
            ERR_POST("Failed to report an unknown exception : " << ex.what());
        }
    }
    thr_context.Reset();

}
void CWorkerNodeRequest::Process(void)
{
    s_RunJob(x_GetThreadContext());
}



/////////////////////////////////////////////////////////////////////////////
//
//     CGridWorkerNode       -- 
CGridWorkerNode::CGridWorkerNode(IWorkerNodeJobFactory&      job_factory,
                                 INetScheduleStorageFactory& storage_factory,
                                 INetScheduleClientFactory&  client_factory)
    : m_JobFactory(job_factory),
      m_NSStorageFactory(storage_factory),
      m_NSClientFactory(client_factory), 
      m_UdpPort(9111), m_MaxThreads(4), m_InitThreads(4),
      m_NSTimeout(30), m_ThreadsPoolTimeout(30), 
      m_ShutdownLevel(CNetScheduleClient::eNoShutdown),
      m_MaxProcessedJob(0), m_JobsStarted(0),
      m_LogRequested(false), m_StartTime(CTime::eCurrent)
{
    m_JobsSucceed.Set(0);
    m_JobsFailed.Set(0);
    m_JobsReturned.Set(0);
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
            ERR_POST(ex.what());
            RequestShutdown(CNetScheduleClient::eShutdownImmidiate);
            return;
        }
        catch (...) {
            ERR_POST("Unknown error");
            RequestShutdown(CNetScheduleClient::eShutdownImmidiate);
            return;
        }
    }

    string    job_key;
    string    input;
    bool      job_exists = false;

    while (1) {
        if (GetShutdownLevel() != CNetScheduleClient::eNoShutdown)
            break;
        try {
            
            if (m_MaxThreads > 1) {
                try {
                    m_ThreadsPool->WaitForRoom(m_ThreadsPoolTimeout);
                } catch (CBlockingQueueException&) {
                    continue;
                }
            }

            job_exists = x_GetNextJob(job_key, input);

            if (job_exists) {
                if (GetShutdownLevel() != CNetScheduleClient::eNoShutdown) {
                    x_ReturnJob(job_key);
                    break;
                }
                auto_ptr<CWorkerNodeJobContext> 
                    job_context(new CWorkerNodeJobContext(*this, 
                                                          job_key, 
                                                          input, 
                                                          m_JobsStarted,
                                                          m_LogRequested));
                ++const_cast<unsigned int&>(m_JobsStarted);
                if (m_MaxThreads > 1 ) {
                    CRef<CStdRequest> job_req(
                                    new CWorkerNodeRequest(job_context));
                    try {
                        m_ThreadsPool->AcceptRequest(job_req);
                    } catch (CBlockingQueueException& ex) {
                        ERR_POST(ex.what());
                        // that must not happen after CBlockingQueue is fixed
                        _ASSERT(0);
                        x_ReturnJob(job_key);
                    }
                }
                else {
                    CGridThreadContext thr_context(*job_context);
                    s_RunJob(thr_context);
                }
                if (m_MaxProcessedJob > 0 && m_JobsStarted > m_MaxProcessedJob - 1) {
                    RequestShutdown(CNetScheduleClient::eNormalShutdown);
                }
            }
        } catch (exception& ex) {
            ERR_POST(ex.what());
            RequestShutdown(CNetScheduleClient::eShutdownImmidiate);
        }
    }
    LOG_POST(Info << "Shutting down...");
    if (m_MaxThreads > 1 ) {
        m_ThreadsPool->KillAllThreads(true);
        m_ThreadsPool.reset(0);
    }
    m_NSReadClient.reset(0);
    LOG_POST(Info << "Worker Node has been stopped.");
}

unsigned int CGridWorkerNode::GetJobsRunningNumber() const
{
    return m_JobsStarted - m_JobsSucceed.Get() - 
                           m_JobsFailed.Get() - m_JobsReturned.Get();
}


const string& CGridWorkerNode::GetQueueName() const 
{ 
    if (!m_NSReadClient.get())
        return kEmptyStr;
    return m_NSReadClient->GetQueueName(); 
}

const string& CGridWorkerNode::GetClientName() const 
{ 
    if (!m_NSReadClient.get())
        return kEmptyStr;
    return m_NSReadClient->GetClientName(); 
}

string CGridWorkerNode::GetConnectionInfo() const
{
    if (!m_NSReadClient.get())
        return kEmptyStr;
    return m_NSReadClient->GetConnectionInfo();    
}

bool CGridWorkerNode::x_GetNextJob(string& job_key, string& input)
{
    bool job_exists = false;
    CGridDebugContext* debug_context = CGridDebugContext::GetInstance();
    if (debug_context && 
        debug_context->GetDebugMode() == CGridDebugContext::eGDC_Execute) {
        job_exists = debug_context->GetNextJob(job_key, input);
        if (!job_exists) {
            RequestShutdown(CNetScheduleClient::eNormalShutdown);
        }         
    } else {
        if(m_NSReadClient.get()) {
            int try_count = 0;
            try {
                if (x_AreMastersBusy()) {
                    job_exists = 
                        m_NSReadClient->WaitJob(&job_key, &input,
                                                m_NSTimeout, m_UdpPort);
                } else {
                    SleepSec(m_NSTimeout);
                }
            }
            catch (CNetServiceException& ex) {
                ERR_POST("Communication Error : " << ex.what());
                if (++try_count >= 2) {
                    RequestShutdown(CNetScheduleClient::eShutdownImmidiate);
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

bool CGridWorkerNode::x_CreateNSReadClient()
{
    if (m_NSReadClient.get())
        return true;

    try {
        m_NSReadClient.reset(CreateClient());
    } catch (exception& ex) {
        ERR_POST(ex.what());
        RequestShutdown(CNetScheduleClient::eShutdownImmidiate);
        return false;
    } catch (...) {
        ERR_POST("Unknown error");
        RequestShutdown(CNetScheduleClient::eShutdownImmidiate);
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
               << m_NSReadClient->GetConnectionInfo();
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
        //        cerr << it->host << ":" << it->port << "  " << replay << endl;           
        if (NStr::StartsWith(replay, "ERR:")) {
            string msg;
            NStr::Replace(replay, "ERR:", "", msg);
            ERR_POST( "Worker Node @ " << it->host << ":" << it->port 
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
            ERR_POST( "Worker Node @ " << it->host << ":" << it->port 
                      << " returned unknown replay : " << replay);
        }
    }
    return true;
}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
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
 
