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
#include <connect/services/grid_worker.hpp>
#include <util/thread_pool.hpp>

#include <signal.h>
BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
//
//     CWorkerNodeJobContext     -- 
CWorkerNodeJobContext::CWorkerNodeJobContext(CGridWorkerNode& worker_node,
                                             const string&    job_key,
                                             const string&    job_input)
  : m_WorkerNode(worker_node), m_JobKey(job_key), 
    m_JobInput(job_input), m_JobOutput(kEmptyStr), 
    m_JobStatus(false), 
    m_Reader(NULL), m_Writer(NULL), m_Reporter(NULL)
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
    if (m_Reader) {
        return m_Reader->GetIStream(m_JobInput, &m_InputBlobSize);
    }
    NCBI_THROW(CNetScheduleStorageException,
               eReader, "Reader is not set.");
    //    return *(CNcbiIstream*)NULL;
}
CNcbiOstream& CWorkerNodeJobContext::GetOStream()
{
    if (m_Writer) {
        return m_Writer->CreateOStream(m_JobOutput);
    }
    NCBI_THROW(CNetScheduleStorageException,
               eWriter, "Writer is not set.");
    //    return *(CNcbiOstream*)NULL;
}

void CWorkerNodeJobContext::Reset()
{
    if (m_Writer) m_Reader->Reset();
    if (m_Writer) m_Writer->Reset();
    m_Reader = NULL;
    m_Writer = NULL;
    m_Reporter = NULL;
}

void CWorkerNodeJobContext::SetJobRunTimeout(unsigned time_to_run)
{
    _ASSERT(m_Reporter);
    try {
        m_Reporter->SetRunTimeout(m_JobKey, time_to_run);
    }
    catch(exception& ex) {
        LOG_POST(Error << "CWorkerNodeJobContext::SetJobRunTimeout : " 
                       << ex.what());
    } 
}

CNetScheduleClient::EShutdownLevel 
CWorkerNodeJobContext::GetShutdownLevel(void) const
{
    return m_WorkerNode.GetShutdownLevel();
}


/////////////////////////////////////////////////////////////////////////////
// 
///@internal
struct SThreadContext
{
    auto_ptr<CNetScheduleClient> reporter;
    auto_ptr<INetScheduleStorage> reader;
    auto_ptr<INetScheduleStorage> writer;
};

///@internal
static void s_TlsCleanup(SThreadContext* p_value, void* /* data */ )
{
    delete p_value;
}

/// @internal
static CRef< CTls<SThreadContext> > s_tls(new CTls<SThreadContext>);

///@internal
class CWorkerNodeRequest : public CStdRequest
{
public: 
    CWorkerNodeRequest(auto_ptr<CWorkerNodeJobContext> context);
    virtual ~CWorkerNodeRequest(void);

    virtual void Process(void);

private:
    auto_ptr<CWorkerNodeJobContext> m_Context;
    auto_ptr<IWorkerNodeJob> m_Job;

    SThreadContext& x_GetThreadContext();

};


CWorkerNodeRequest::CWorkerNodeRequest(auto_ptr<CWorkerNodeJobContext> context)
  : m_Context(context)
{
    if (m_Context.get())
        m_Job.reset(m_Context->GetWorkerNode().CreateJob());

}


CWorkerNodeRequest::~CWorkerNodeRequest(void)
{
}

SThreadContext& CWorkerNodeRequest::x_GetThreadContext()
{
    SThreadContext* context = s_tls->GetValue();
    if (!context) {
        context = new SThreadContext();
        context->reporter.reset(m_Context->GetWorkerNode().CreateClient());
        context->reader.reset(m_Context->GetWorkerNode().CreateStorage());
        context->writer.reset(m_Context->GetWorkerNode().CreateStorage());
        s_tls->SetValue(context, s_TlsCleanup);
    }
    return *context;
}


void CWorkerNodeRequest::Process(void)
{
    if (m_Job.get()) {
        SThreadContext* thread_context = NULL;
        try {
            thread_context = &x_GetThreadContext();
            CNetScheduleClient& ns_client = *thread_context->reporter;
            m_Context->m_Reporter = thread_context->reporter.get();
            m_Context->m_Reader = thread_context->reader.get();
            m_Context->m_Writer = thread_context->writer.get();
            int ret_code = m_Job->Do(*m_Context);
            int try_count = 0;
            while(1) {
                try {
                    if (m_Context->IsJobCommited()) {
                        ns_client.PutResult(m_Context->GetJobKey(),
                                            ret_code,
                                            m_Context->m_JobOutput);
                    }
                    else
                        ns_client.ReturnJob(m_Context->GetJobKey());
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

            m_Context->Reset();
        }
        catch (exception& ex) {
            LOG_POST(Error << "Error in Job execution : " 
                           << ex.what());
            if (thread_context) {
                try {
                    thread_context->reporter->PutFailure(
                                            m_Context->GetJobKey(),
                                            ex.what());
                }
                catch(exception& ex) {
                    LOG_POST(Error << "Failed to report an exception : " 
                                   << ex.what());
                }
            }
            m_Context->Reset();
        }
    }
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
      m_UdpPort(9111), m_MaxThreads(4), m_InitThreads(20),
      m_NSTimeout(30), m_ThreadsPoolTimeout(30), 
      m_ShutdownLevel(CNetScheduleClient::eNoShutdown)
{
}


CGridWorkerNode::~CGridWorkerNode()
{
}


void CGridWorkerNode::Start()
{
    try {
        m_NSReadClient.reset(CreateClient());
        m_ThreadsPool.reset(new CStdPoolOfThreads(m_MaxThreads, m_MaxThreads));
        m_ThreadsPool->Spawn(m_InitThreads);
    }
    catch (exception& ex) {
        LOG_POST(Error << ex.what());
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
            if (m_ThreadsPool->IsFull()) {
                try {
                    m_ThreadsPool->WaitForRoom(m_ThreadsPoolTimeout);
                }
                catch (CBlockingQueueException&) {
                    continue;
                }
            }
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

            if (job_exists) {
                if (GetShutdownLevel() != CNetScheduleClient::eNoShutdown) {
                    m_NSReadClient->ReturnJob(job_key);
                    break;
                }
                auto_ptr<CWorkerNodeJobContext> 
                    context(new CWorkerNodeJobContext(*this, job_key, input));
                CRef<CStdRequest> job_req(new CWorkerNodeRequest(context));
                try {
                    m_ThreadsPool->AcceptRequest(job_req);
                }
                catch (CBlockingQueueException&) {
                    // that must not happen after CBlockingQueue is fixed
                    _ASSERT(0);
                    m_NSReadClient->ReturnJob(job_key);
                }
            }
        } 
        catch (exception& ex) {
            LOG_POST(Error << ex.what());
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
 
