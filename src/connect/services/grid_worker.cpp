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
#include <util/thread_pool.hpp>

#include "grid_thread_context.hpp"
#include "grid_debug_context.hpp"


BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
//
//     IWorkerNodeInitContext     -- 

const IRegistry& IWorkerNodeInitContext::GetConfig() const
{
    NCBI_THROW(CCoreException,
               eInvalidArg, "Not Implemented.");
}
const CArgs& IWorkerNodeInitContext::GetArgs() const
{
    NCBI_THROW(CCoreException,
               eInvalidArg, "Not Implemented.");
}
const CNcbiEnvironment& IWorkerNodeInitContext::GetEnvironment() const
{
    NCBI_THROW(CCoreException,
               eInvalidArg, "Not Implemented.");
}

/////////////////////////////////////////////////////////////////////////////
//
//     CWorkerNodeJobContext     -- 
CWorkerNodeJobContext::CWorkerNodeJobContext(CGridWorkerNode& worker_node,
                                             const string&    job_key,
                                             const string&    job_input,
                                             unsigned int     job_number,
                                             bool             log_requested)
    : m_WorkerNode(worker_node), m_JobKey(job_key), m_JobInput(job_input),
      m_JobCommitted(false), m_LogRequested(log_requested), 
      m_JobNumber(job_number), m_ThreadContext(NULL)
{
}

CWorkerNodeJobContext::~CWorkerNodeJobContext()
{
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
        int try_count = 0;
        while(1) {
            try {
                if (thr_context.IsJobCommitted()) {
                    thr_context.PutResult(ret_code);
                }
                else
                    thr_context.ReturnJob();
                break;
            }
            catch (CNetServiceException& ex) {
                LOG_POST(Error << "Communication Error : " 
                         << ex.what());
                if (++try_count >= 2)
                    throw;
                SleepMilliSec(1000 + try_count*2000);
            }
        }
    }
    catch (exception& ex) {
        ERR_POST("Error in Job execution : " 
                 << ex.what());
        try {
            thr_context.PutFailure(ex.what());
        }
        catch(exception& ex1) {
            ERR_POST("Failed to report an exception : " 
                     << ex1.what());
        }
    }
    catch(...) {
        ERR_POST( "Unkown Error in Job execution" );
        try {
            thr_context.PutFailure("Unkown Error in Job execution");
        }
        catch(exception& ex) {
            ERR_POST("Failed to report an exception : " 
                     << ex.what());
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
      m_MaxProcessedJob(0), m_ProcessedJob(0),
      m_LogRequested(false)
{
}


CGridWorkerNode::~CGridWorkerNode()
{
}

void CGridWorkerNode::StartSingleThreaded()
{
    try {
        _ASSERT(m_MaxThreads == 1);
        m_NSReadClient.reset(CreateClient());
    }
    catch (exception& ex) {
        LOG_POST(Error << ex.what());
        RequestShutdown(CNetScheduleClient::eShutdownImmidiate);
        return;
    }
    catch (...) {
        LOG_POST(Error << "Unkown error");
        RequestShutdown(CNetScheduleClient::eShutdownImmidiate);
        return;
    }


    string    job_key;
    string    input;
    bool      job_exists = false;
    while (1) {
        if (GetShutdownLevel() != CNetScheduleClient::eNoShutdown)
            break;

        CGridDebugContext* debug_context = CGridDebugContext::GetInstance();
        if (debug_context && 
            debug_context->GetDebugMode() == CGridDebugContext::eGDC_Execute) {
            job_exists = debug_context->GetNextJob(job_key, input);
            if (!job_exists) {
                RequestShutdown(CNetScheduleClient::eNormalShutdown);
                continue;
            }         
        } else {
            int try_count = 0;
            try {
                job_exists = 
                    m_NSReadClient->WaitJob(&job_key, &input, 
                                            m_NSTimeout, m_UdpPort);
            }
            catch (CNetServiceException& ex) {
                ERR_POST("Communication Error : " 
                         << ex.what());
                if (++try_count >= 2) {
                    LOG_POST(Error << ex.what());
                    RequestShutdown(CNetScheduleClient::eShutdownImmidiate);
                    continue;
                }
                SleepMilliSec(1000 + try_count*2000);
            }
        }
        if (job_exists) {
            if (GetShutdownLevel() != CNetScheduleClient::eNoShutdown) {
                if (!debug_context || 
                    debug_context->GetDebugMode() != CGridDebugContext::eGDC_Execute) {
                    m_NSReadClient->ReturnJob(job_key);
                }
                break;
            }
            LOG_POST( "Got Job : " << job_key << "  " << input);
            CWorkerNodeJobContext job_context(*this, 
                                              job_key, 
                                              input, 
                                              m_ProcessedJob,
                                              m_LogRequested);
            CGridThreadContext thr_context(job_context);
            s_RunJob(thr_context);
            
            if (m_MaxProcessedJob > 0 && ++m_ProcessedJob > m_MaxProcessedJob - 1) {
                RequestShutdown(CNetScheduleClient::eNormalShutdown);
            }
        }
    }
    LOG_POST(Info << "Worker Node has been stopped.");
    m_NSReadClient.reset(0);
}

void CGridWorkerNode::Start()
{
    if (m_MaxThreads == 1) {
        StartSingleThreaded();
        return;
    }
    try {
        _ASSERT(m_MaxThreads > 0);
        m_NSReadClient.reset(CreateClient());
        m_ThreadsPool.reset(new CStdPoolOfThreads(m_MaxThreads, 0/*m_MaxThreads-1*/));
        m_ThreadsPool->Spawn(m_InitThreads);
    }
    catch (exception& ex) {
        LOG_POST(Error << ex.what());
        RequestShutdown(CNetScheduleClient::eShutdownImmidiate);
        return;
    }
    catch (...) {
        LOG_POST(Error << "Unkown error");
        RequestShutdown(CNetScheduleClient::eShutdownImmidiate);
        return;
    }

    string    job_key;
    string    input;
    bool      job_exists = false;

    while (1) {
        if (GetShutdownLevel() != CNetScheduleClient::eNoShutdown)
            break;
        try {
            try {
                m_ThreadsPool->WaitForRoom(m_ThreadsPoolTimeout);
            }
            catch (CBlockingQueueException&) {
                continue;
            }

            CGridDebugContext* debug_context = CGridDebugContext::GetInstance();
            if (debug_context && 
                debug_context->GetDebugMode() == CGridDebugContext::eGDC_Execute) {
                job_exists = debug_context->GetNextJob(job_key, input);
                if (!job_exists) {
                    RequestShutdown(CNetScheduleClient::eNormalShutdown);
                    continue;
                }         
            } else {
                int try_count = 0;
                while(1) {
                    try {
                        job_exists = 
                            m_NSReadClient->WaitJob(&job_key, &input, 
                                                    m_NSTimeout, m_UdpPort);
                        break;
                    }
                    catch (CNetServiceException& ex) {
                        LOG_POST(Error << "Communication Error : " 
                                 << ex.what());
                        if (++try_count >= 2)
                            throw;
                        SleepMilliSec(1000 + try_count*2000);
                    }
                }
            }
            if (job_exists) {
                if (GetShutdownLevel() != CNetScheduleClient::eNoShutdown) {
                    if (!debug_context || 
                        debug_context->GetDebugMode() != CGridDebugContext::eGDC_Execute) {
                        m_NSReadClient->ReturnJob(job_key);
                    }
                    break;
                }
                //       LOG_POST( "Got Job : " << job_key << "  " << input);
                auto_ptr<CWorkerNodeJobContext> 
                    context(new CWorkerNodeJobContext(*this, 
                                                      job_key, 
                                                      input, 
                                                      m_ProcessedJob,
                                                      m_LogRequested));
                CRef<CStdRequest> job_req(new CWorkerNodeRequest(context));
                try {
                    m_ThreadsPool->AcceptRequest(job_req);
                }
                catch (CBlockingQueueException& ex) {
                    // that must not happen after CBlockingQueue is fixed
                    ERR_POST(ex.what());
                    _ASSERT(0);
                    if (!debug_context || 
                        debug_context->GetDebugMode() != CGridDebugContext::eGDC_Execute) {
                        m_NSReadClient->ReturnJob(job_key);
                    }
                }
                if (m_MaxProcessedJob > 0 && ++m_ProcessedJob > m_MaxProcessedJob - 1) {
                    RequestShutdown(CNetScheduleClient::eNormalShutdown);
                }
            }
        } 
        catch (exception& ex) {
            ERR_POST(ex.what());
            RequestShutdown(CNetScheduleClient::eShutdownImmidiate);
        }
    }
    LOG_POST(Info << "Shutting down...");
    m_ThreadsPool->KillAllThreads(true);
    m_ThreadsPool.reset(0);
    m_NSReadClient.reset(0);
    LOG_POST(Info << "Worker Node Threads Pool stopped.");
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



END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
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
 
