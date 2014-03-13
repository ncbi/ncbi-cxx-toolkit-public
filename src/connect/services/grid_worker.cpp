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

#include "wn_commit_thread.hpp"
#include "grid_debug_context.hpp"
#include "netschedule_api_impl.hpp"

#include <connect/services/grid_globals.hpp>
#include <connect/services/error_codes.hpp>
#include <connect/services/netschedule_api_expt.hpp>
#include <connect/services/grid_worker_app.hpp>
#include <connect/services/grid_control_thread.hpp>
#include <connect/services/grid_rw_impl.hpp>

#include <connect/ncbi_socket.hpp>

#include <util/thread_pool.hpp>

#include <corelib/ncbireg.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbi_process.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <corelib/rwstream.hpp>

#ifdef NCBI_OS_UNIX
#include <unistd.h>
#endif


#define NCBI_USE_ERRCODE_X   ConnServ_WorkerNode

#define DEFAULT_NS_TIMEOUT 30

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
//
IWorkerNodeJobWatcher::~IWorkerNodeJobWatcher()
{
}

/////////////////////////////////////////////////////////////////////////////
//
//     CWorkerNodeJobContext     --

namespace {

class CWorkerNodeCleanup : public IWorkerNodeCleanupEventSource
{
public:
    typedef set<IWorkerNodeCleanupEventListener*> TListeners;

    virtual void AddListener(IWorkerNodeCleanupEventListener* listener);
    virtual void RemoveListener(IWorkerNodeCleanupEventListener* listener);

    virtual void CallEventHandlers();

    void RemoveListeners(const TListeners& listeners);

protected:
    TListeners m_Listeners;
    CFastMutex m_ListenersLock;
};

void CWorkerNodeCleanup::AddListener(IWorkerNodeCleanupEventListener* listener)
{
    CFastMutexGuard g(m_ListenersLock);
    m_Listeners.insert(listener);
}

void CWorkerNodeCleanup::RemoveListener(
    IWorkerNodeCleanupEventListener* listener)
{
    CFastMutexGuard g(m_ListenersLock);
    m_Listeners.erase(listener);
}

void CWorkerNodeCleanup::CallEventHandlers()
{
    TListeners listeners;
    {
        CFastMutexGuard g(m_ListenersLock);
        listeners.swap(m_Listeners);
    }

    ITERATE(TListeners, it, listeners) {
        try {
            (*it)->HandleEvent(
                IWorkerNodeCleanupEventListener::eRegularCleanup);
            delete *it;
        }
        NCBI_CATCH_ALL_X(39, "Job clean-up error");
    }
}

void CWorkerNodeCleanup::RemoveListeners(
    const CWorkerNodeCleanup::TListeners& listeners)
{
    CFastMutexGuard g(m_ListenersLock);
    ITERATE(TListeners, it, listeners) {
        m_Listeners.erase(*it);
    }
}

class CWorkerNodeJobCleanup : public CWorkerNodeCleanup
{
public:
    CWorkerNodeJobCleanup(CWorkerNodeCleanup* worker_node_cleanup);

    virtual void AddListener(IWorkerNodeCleanupEventListener* listener);
    virtual void RemoveListener(IWorkerNodeCleanupEventListener* listener);

    virtual void CallEventHandlers();

private:
    CWorkerNodeCleanup* m_WorkerNodeCleanup;
};

CWorkerNodeJobCleanup::CWorkerNodeJobCleanup(
        CWorkerNodeCleanup* worker_node_cleanup) :
    m_WorkerNodeCleanup(worker_node_cleanup)
{
}

void CWorkerNodeJobCleanup::AddListener(
    IWorkerNodeCleanupEventListener* listener)
{
    CWorkerNodeCleanup::AddListener(listener);
    m_WorkerNodeCleanup->AddListener(listener);
}

void CWorkerNodeJobCleanup::RemoveListener(
    IWorkerNodeCleanupEventListener* listener)
{
    CWorkerNodeCleanup::RemoveListener(listener);
    m_WorkerNodeCleanup->RemoveListener(listener);
}

void CWorkerNodeJobCleanup::CallEventHandlers()
{
    {
        CFastMutexGuard g(m_ListenersLock);
        m_WorkerNodeCleanup->RemoveListeners(m_Listeners);
    }
    CWorkerNodeCleanup::CallEventHandlers();
}

} // end of anonymous namespace

CWorkerNodeJobContext::CWorkerNodeJobContext(CGridWorkerNode& worker_node) :
    m_WorkerNode(worker_node),
    m_CleanupEventSource(new CWorkerNodeJobCleanup(
        static_cast<CWorkerNodeCleanup*>(worker_node.GetCleanupEventSource()))),
    m_RequestContext(new CRequestContext),
    m_StatusThrottler(1, CTimeSpan(worker_node.GetCheckStatusPeriod(), 0)),
    m_ProgressMsgThrottler(1),
    m_NetScheduleExecutor(worker_node.GetNSExecutor()),
    m_NetCacheAPI(worker_node.GetNetCacheAPI()),
    m_CommitExpiration(0, 0)
{
    m_NetScheduleExecutor->m_WorkerNodeMode = true;
}

const string& CWorkerNodeJobContext::GetQueueName() const
{
    return m_WorkerNode.GetQueueName();
}
const string& CWorkerNodeJobContext::GetClientName() const
{
    return m_WorkerNode.GetClientName();
}

CNcbiIstream& CWorkerNodeJobContext::GetIStream()
{
    if (m_NetCacheAPI) {
        IReader* reader = new CStringOrBlobStorageReader(GetJobInput(),
                m_NetCacheAPI, &SetJobInputBlobSize());
        m_RStream.reset(new CRStream(reader, 0, 0,
                CRWStreambuf::fOwnReader | CRWStreambuf::fLeakExceptions));
        m_RStream->exceptions(IOS_BASE::badbit | IOS_BASE::failbit);
        return *m_RStream;
    }
    NCBI_THROW(CBlobStorageException, eReader, "Reader is not set.");
}
CNcbiOstream& CWorkerNodeJobContext::GetOStream()
{
    if (!m_NetCacheAPI) {
        NCBI_THROW(CBlobStorageException, eWriter, "Writer is not set.");
    }
    m_Writer.reset(new CStringOrBlobStorageWriter(GetMaxServerOutputSize(),
            m_NetCacheAPI, SetJobOutput()));
    m_WStream.reset(new CWStream(m_Writer.get(), 0, 0,
            CRWStreambuf::fLeakExceptions));
    m_WStream->exceptions(IOS_BASE::badbit | IOS_BASE::failbit);
    return *m_WStream;
}

void CWorkerNodeJobContext::CloseStreams()
{
    m_ProgressMsgThrottler.Reset(1);
    m_StatusThrottler.Reset(1,
            CTimeSpan(m_WorkerNode.GetCheckStatusPeriod(), 0));

    m_RStream.reset();
    m_WStream.reset();

    if (m_Writer.get() != NULL) {
        m_Writer->Close();
        m_Writer.reset();
    }

    CGridDebugContext* debug_context = CGridDebugContext::GetInstance();
    if (debug_context)
        debug_context->DumpOutput(GetJobKey(), GetJobOutput(), GetJobNumber());
}

void CWorkerNodeJobContext::CommitJob()
{
    CheckIfCanceled();
    m_JobCommitted = eDone;
}

void CWorkerNodeJobContext::CommitJobWithFailure(const string& err_msg)
{
    CheckIfCanceled();
    m_JobCommitted = eFailure;
    m_Job.error_msg = err_msg;
}

void CWorkerNodeJobContext::ReturnJob()
{
    CheckIfCanceled();
    m_JobCommitted = eReturn;
}

void CWorkerNodeJobContext::PutProgressMessage(const string& msg,
                                               bool send_immediately)
{
    CheckIfCanceled();
    if (!send_immediately &&
            !m_ProgressMsgThrottler.Approve(CRequestRateControl::eErrCode)) {
        LOG_POST(Warning << "Progress message \"" <<
                msg << "\" has been suppressed.");
        return;
    }

    if (m_WorkerNode.m_ProgressLogRequested) {
        LOG_POST(GetJobKey() << " progress: " <<
                NStr::TruncateSpaces(msg, NStr::eTrunc_End));
    }

    try {
        CGridDebugContext* debug_context = CGridDebugContext::GetInstance();
        if (!debug_context ||
            debug_context->GetDebugMode() != CGridDebugContext::eGDC_Execute) {

            if (m_Job.progress_msg.empty() ) {
                m_NetScheduleExecutor.GetProgressMsg(m_Job);
            }
            if (m_NetCacheAPI) {
                if (!m_Job.progress_msg.empty())
                    m_NetCacheAPI.PutData(m_Job.progress_msg,
                            msg.data(), msg.length());
                else {
                    m_Job.progress_msg =
                            m_NetCacheAPI.PutData(msg.data(), msg.length());

                    m_NetScheduleExecutor.PutProgressMsg(m_Job);
                }
            }
        }
        if (debug_context) {
            debug_context->DumpProgressMessage(GetJobKey(),
                                               msg,
                                               GetJobNumber());
        }
    }
    catch (exception& ex) {
        ERR_POST_X(6, "Couldn't send a progress message: " << ex.what());
    }
}

void CWorkerNodeJobContext::JobDelayExpiration(unsigned runtime_inc)
{
    try {
        m_NetScheduleExecutor.JobDelayExpiration(GetJobKey(), runtime_inc);
    }
    catch(exception& ex) {
        ERR_POST_X(8, "CWorkerNodeJobContext::JobDelayExpiration: " <<
                ex.what());
    }
}

bool CWorkerNodeJobContext::IsLogRequested() const
{
    return m_WorkerNode.m_LogRequested;
}

CNetScheduleAdmin::EShutdownLevel CWorkerNodeJobContext::GetShutdownLevel()
{
    CGridDebugContext* debug_context = CGridDebugContext::GetInstance();
    if ((!debug_context || debug_context->GetDebugMode() !=
                CGridDebugContext::eGDC_Execute) &&
            m_StatusThrottler.Approve(CRequestRateControl::eErrCode))
        try {
            switch (m_NetScheduleExecutor.GetJobStatus(GetJobKey())) {
            case CNetScheduleAPI::eRunning:
                break;

            case CNetScheduleAPI::eCanceled:
                x_SetCanceled();
                /* FALL THROUGH */

            default:
                return CNetScheduleAdmin::eShutdownImmediate;
            }
        }
        catch(exception& ex) {
            ERR_POST("Cannot retrieve job status for " << GetJobKey() <<
                    ": " << ex.what());
        }

    return CGridGlobals::GetInstance().GetShutdownLevel();
}

void CWorkerNodeJobContext::CheckIfCanceled()
{
    if (IsCanceled()) {
        NCBI_THROW_FMT(CGridWorkerNodeException, eJobIsCanceled,
            "Job " << m_Job.job_id << " has been canceled");
    }
}

void CWorkerNodeJobContext::Reset()
{
    m_JobNumber = CGridGlobals::GetInstance().GetNewJobNumber();

    m_JobCommitted = eNotCommitted;
    m_InputBlobSize = 0;
    m_ExclusiveJob = m_Job.mask & CNetScheduleAPI::eExclusiveJob;

    m_RequestContext->Reset();
}

void CWorkerNodeJobContext::RequestExclusiveMode()
{
    if (!m_ExclusiveJob) {
        if (!m_WorkerNode.EnterExclusiveMode()) {
            NCBI_THROW(CGridWorkerNodeException,
                eExclusiveModeIsAlreadySet, "");
        }
        m_ExclusiveJob = true;
    }
}

size_t CWorkerNodeJobContext::GetMaxServerOutputSize()
{
    return m_WorkerNode.GetServerOutputSize();
}

const char* CWorkerNodeJobContext::GetCommitStatusDescription(
        CWorkerNodeJobContext::ECommitStatus commit_status)
{
    switch (commit_status) {
    case eDone:
        return "done";
    case eFailure:
        return "failed";
    case eReturn:
        return "returned";
    case eCanceled:
        return "canceled";
    default:
        return "not committed";
    }
}

IWorkerNodeCleanupEventSource* CWorkerNodeJobContext::GetCleanupEventSource()
{
    return m_CleanupEventSource;
}


///@internal
class CWorkerNodeRequest : public CStdRequest
{
public:
    CWorkerNodeRequest(CWorkerNodeJobContext* context) :
        m_JobContext(context)
    {
    }

    virtual void Process();

private:
    CWorkerNodeJobContext* m_JobContext;
};


static CGridWorkerNode::EDisabledRequestEvents s_ReqEventsDisabled =
    CGridWorkerNode::eEnableStartStop;

void CWorkerNodeJobContext::x_PrintRequestStop()
{
    m_RequestContext->SetAppState(eDiagAppState_RequestEnd);

    if (!m_RequestContext->IsSetRequestStatus())
        m_RequestContext->SetRequestStatus(
            GetCommitStatus() == CWorkerNodeJobContext::eDone &&
                m_Job.ret_code == 0 ? 200 : 500);

    if (m_RequestContext->GetAppState() == eDiagAppState_Request)
        m_RequestContext->SetAppState(eDiagAppState_RequestEnd);

    switch (s_ReqEventsDisabled) {
    case CGridWorkerNode::eEnableStartStop:
    case CGridWorkerNode::eDisableStartOnly:
        GetDiagContext().PrintRequestStop();

    default:
        break;
    }
}

void CWorkerNodeJobContext::x_RunJob()
{
    CGridDebugContext* debug_context = CGridDebugContext::GetInstance();
    if (debug_context != NULL)
        debug_context->DumpInput(GetJobInput(), GetJobNumber());

    m_RequestContext->SetRequestID((int) GetJobNumber());

    if (!m_Job.client_ip.empty())
        m_RequestContext->SetClientIP(m_Job.client_ip);

    if (!m_Job.session_id.empty())
        m_RequestContext->SetSessionID(m_Job.session_id);

    m_RequestContext->SetAppState(eDiagAppState_RequestBegin);

    CRequestContextSwitcher request_state_guard(m_RequestContext);

    if (s_ReqEventsDisabled == CGridWorkerNode::eEnableStartStop)
        GetDiagContext().PrintRequestStart().Print("jid", m_Job.job_id);

    GetWorkerNode().x_NotifyJobWatchers(*this,
            IWorkerNodeJobWatcher::eJobStarted);

    m_RequestContext->SetAppState(eDiagAppState_Request);

    try {
        SetJobRetCode(m_WorkerNode.GetJobProcessor()->Do(*this));
    }
    catch (CGridWorkerNodeException& ex) {
        switch (ex.GetErrCode()) {
        case CGridWorkerNodeException::eJobIsCanceled:
            x_SetCanceled();
            break;

        case CGridWorkerNodeException::eExclusiveModeIsAlreadySet:
            if (IsLogRequested()) {
                LOG_POST_X(21, "Job " << GetJobKey() <<
                    " will be returned back to the queue "
                    "because it requested exclusive mode while "
                    "another job already has the exclusive status.");
            }
            if (!IsJobCommitted())
                ReturnJob();
            break;

        default:
            ERR_POST_X(62, ex);
            if (!IsJobCommitted())
                ReturnJob();
        }
    }
    catch (CNetScheduleException& e) {
        ERR_POST_X(20, "job" << GetJobKey() << " failed: " << e);
        if (e.GetErrCode() == CNetScheduleException::eJobNotFound)
            x_SetCanceled();
        else if (!IsJobCommitted())
            CommitJobWithFailure(e.what());
    }
    catch (exception& e) {
        ERR_POST_X(18, "job" << GetJobKey() << " failed: " << e.what());
        if (!IsJobCommitted())
            CommitJobWithFailure(e.what());
    }

    try {
        CloseStreams();
    }
    NCBI_CATCH_ALL_X(61, "Could not close IO streams");

    if (m_WorkerNode.IsExclusiveMode() && IsJobExclusive())
        m_WorkerNode.LeaveExclusiveMode();

    switch (GetCommitStatus()) {
    case eDone:
        m_WorkerNode.x_NotifyJobWatchers(*this,
                IWorkerNodeJobWatcher::eJobSucceeded);
        break;

    case eNotCommitted:
        if (TWorkerNode_AllowImplicitJobReturn::GetDefault() ||
                GetShutdownLevel() != CNetScheduleAdmin::eNoShutdown) {
            ReturnJob();
            m_WorkerNode.x_NotifyJobWatchers(*this,
                    IWorkerNodeJobWatcher::eJobReturned);
            break;
        }

        CommitJobWithFailure("Job was not explicitly committed");
        /* FALL THROUGH */

    case eFailure:
        m_WorkerNode.x_NotifyJobWatchers(*this,
                IWorkerNodeJobWatcher::eJobFailed);
        break;

    case eReturn:
        m_WorkerNode.x_NotifyJobWatchers(*this,
                IWorkerNodeJobWatcher::eJobReturned);
        break;

    default: // eCanceled - will be processed in x_SendJobResults().
        break;
    }

    GetWorkerNode().x_NotifyJobWatchers(*this,
            IWorkerNodeJobWatcher::eJobStopped);

    if (!CGridGlobals::GetInstance().IsShuttingDown())
        static_cast<CWorkerNodeJobCleanup*>(
                GetCleanupEventSource())->CallEventHandlers();

    m_WorkerNode.m_JobCommitterThread->PutJobContextBackAndCommitJob(this);
}

void CWorkerNodeJobContext::x_SendJobResults()
{
    switch (GetCommitStatus()) {
    case eDone:
        {
            CGridDebugContext* debug_context = CGridDebugContext::GetInstance();

            if (!debug_context || debug_context->GetDebugMode() !=
                    CGridDebugContext::eGDC_Execute) {
                m_NetScheduleExecutor.PutResult(m_Job);
            }
        }
        break;

    case eFailure:
        {
            CGridDebugContext* debug_context = CGridDebugContext::GetInstance();

            if (!debug_context || debug_context->GetDebugMode() !=
                    CGridDebugContext::eGDC_Execute)
                m_NetScheduleExecutor.PutFailure(m_Job);
        }
        break;

    case eReturn:
        m_WorkerNode.x_ReturnJob(m_Job);
        break;

    default: // Always eCanceled; eNotCommitted is overridden in x_RunJob().
        ERR_POST("Job " << GetJobKey() << " has been canceled");
    }
}

void CWorkerNodeRequest::Process()
{
    m_JobContext->x_RunJob();
}

/////////////////////////////////////////////////////////////////////////////
//
//     CGridControleThread
/// @internal
class CGridControlThread : public CThread
{
public:
    CGridControlThread(CGridWorkerNode* worker_node,
        unsigned int start_port, unsigned int end_port) : m_Control(
            new CWorkerNodeControlServer(worker_node, start_port, end_port)) {}

    void Prepare() { m_Control->StartListening(); }

    unsigned short GetControlPort() { return m_Control->GetControlPort(); }
    void Stop() { if (m_Control.get()) m_Control->RequestShutdown(); }

protected:
    virtual void* Main(void)
    {
        m_Control->Run();
        return NULL;
    }
    virtual void OnExit(void)
    {
        CThread::OnExit();
        CGridGlobals::GetInstance().RequestShutdown(
                CNetScheduleAdmin::eShutdownImmediate);
        LOG_POST_X(46, Info << "Control Thread has been stopped.");
    }

private:
    auto_ptr<CWorkerNodeControlServer> m_Control;
};

class CGridCleanupThread : public CThread
{
public:
    CGridCleanupThread(CGridWorkerNode* worker_node,
        IGridWorkerNodeApp_Listener* listener) :
            m_WorkerNode(worker_node),
            m_Listener(listener),
            m_Semaphore(0, 1)
    {
    }

    bool Wait(unsigned seconds) {return m_Semaphore.TryWait(seconds);}

protected:
    virtual void* Main();

private:
    CGridWorkerNode* m_WorkerNode;
    IGridWorkerNodeApp_Listener* m_Listener;
    CSemaphore m_Semaphore;
};

void* CGridCleanupThread::Main()
{
    m_WorkerNode->GetCleanupEventSource()->CallEventHandlers();
    m_Listener->OnGridWorkerStop();
    m_Semaphore.Post();

    return NULL;
}

/////////////////////////////////////////////////////////////////////////////
//
//     CWorkerNodeIdleThread      --
class CWorkerNodeIdleThread : public CThread
{
public:
    CWorkerNodeIdleThread(IWorkerNodeIdleTask*,
                          CGridWorkerNode& worker_node,
                          unsigned run_delay,
                          unsigned int auto_shutdown);

    void RequestShutdown()
    {
        m_ShutdownFlag = true;
        m_Wait1.Post();
        m_Wait2.Post();
    }
    void Schedule()
    {
        CFastMutexGuard guard(m_Mutex);
        m_AutoShutdownSW.Restart();
        if (m_StopFlag) {
            m_StopFlag = false;
            m_Wait1.Post();
        }
    }
    void Suspend()
    {
        CFastMutexGuard guard(m_Mutex);
        m_AutoShutdownSW.Restart();
        m_AutoShutdownSW.Stop();
        if (!m_StopFlag) {
            m_StopFlag = true;
            m_Wait2.Post();
        }
    }

    bool IsShutdownRequested() const { return m_ShutdownFlag; }


protected:
    virtual void* Main(void);
    virtual void OnExit(void);

    CWorkerNodeIdleTaskContext& GetContext();

private:

    unsigned int x_GetInterval() const
    {
        CFastMutexGuard guard(m_Mutex);
        return m_AutoShutdown > 0 ? min(m_AutoShutdown -
                (unsigned) m_AutoShutdownSW.Elapsed(),
                        m_RunInterval) : m_RunInterval;
    }
    bool x_GetStopFlag() const
    {
        CFastMutexGuard guard(m_Mutex);
        return m_StopFlag;
    }
    bool x_IsAutoShutdownTime() const
    {
        CFastMutexGuard guard(m_Mutex);
        return m_AutoShutdown > 0 &&
                m_AutoShutdownSW.Elapsed() > m_AutoShutdown;
    }

    IWorkerNodeIdleTask* m_Task;
    CGridWorkerNode& m_WorkerNode;
    auto_ptr<CWorkerNodeIdleTaskContext> m_TaskContext;
    mutable CSemaphore  m_Wait1;
    mutable CSemaphore  m_Wait2;
    volatile bool       m_StopFlag;
    volatile bool       m_ShutdownFlag;
    unsigned int        m_RunInterval;
    unsigned int        m_AutoShutdown;
    CStopWatch          m_AutoShutdownSW;
    mutable CFastMutex  m_Mutex;

    CWorkerNodeIdleThread(const CWorkerNodeIdleThread&);
    CWorkerNodeIdleThread& operator=(const CWorkerNodeIdleThread&);
};

CWorkerNodeIdleThread::CWorkerNodeIdleThread(IWorkerNodeIdleTask* task,
                                             CGridWorkerNode& worker_node,
                                             unsigned run_delay,
                                             unsigned int auto_shutdown)
    : m_Task(task), m_WorkerNode(worker_node),
      m_Wait1(0,100000), m_Wait2(0,1000000),
      m_StopFlag(false), m_ShutdownFlag(false),
      m_RunInterval(run_delay),
      m_AutoShutdown(auto_shutdown), m_AutoShutdownSW(CStopWatch::eStart)
{
}
void* CWorkerNodeIdleThread::Main()
{
    while (!m_ShutdownFlag) {
        if ( x_IsAutoShutdownTime() ) {
            LOG_POST_X(47, Info <<
                "There are no more jobs to be done. Exiting.");
            CGridGlobals::GetInstance().RequestShutdown(
                    CNetScheduleAdmin::eShutdownImmediate);
            break;
        }
        unsigned interval = m_AutoShutdown > 0 ? min (m_RunInterval,
                m_AutoShutdown) : m_RunInterval;
        if (m_Wait1.TryWait(interval, 0)) {
            if (m_ShutdownFlag)
                continue;
            interval = x_GetInterval();
            if (m_Wait2.TryWait(interval, 0)) {
                continue;
            }
        }
        if (m_Task && !x_GetStopFlag()) {
            try {
                do {
                    if (x_IsAutoShutdownTime()) {
                        LOG_POST_X(48, Info <<
                            "There are no more jobs to be done. Exiting.");
                        CGridGlobals::GetInstance().RequestShutdown(
                            CNetScheduleAdmin::eShutdownImmediate);
                        m_ShutdownFlag = true;
                        break;
                    }
                    GetContext().Reset();
                    m_Task->Run(GetContext());
                } while (GetContext().NeedRunAgain() && !m_ShutdownFlag);
            } NCBI_CATCH_ALL_X(58,
                "CWorkerNodeIdleThread::Main: Idle Task failed");
        }
    }
    return 0;
}

void CWorkerNodeIdleThread::OnExit(void)
{
    LOG_POST_X(49, Info << "Idle Thread has been stopped.");
}

CWorkerNodeIdleTaskContext& CWorkerNodeIdleThread::GetContext()
{
    if (!m_TaskContext.get())
        m_TaskContext.reset(new CWorkerNodeIdleTaskContext(*this));
    return *m_TaskContext;
}

/////////////////////////////////////////////////////////////////////////////
//
//     CWorkerNodeIdleTaskContext      --
CWorkerNodeIdleTaskContext::CWorkerNodeIdleTaskContext(
        CWorkerNodeIdleThread& thread) :
    m_Thread(thread), m_RunAgain(false)
{
}
bool CWorkerNodeIdleTaskContext::IsShutdownRequested() const
{
    return m_Thread.IsShutdownRequested();
}
void CWorkerNodeIdleTaskContext::Reset()
{
    m_RunAgain = false;
}

void CWorkerNodeIdleTaskContext::RequestShutdown()
{
    m_Thread.RequestShutdown();
    CGridGlobals::GetInstance().
        RequestShutdown(CNetScheduleAdmin::eShutdownImmediate);
}

/////////////////////////////////////////////////////////////////////////////
//
//    CIdleWatcher
/// @internal
class CIdleWatcher : public IWorkerNodeJobWatcher
{
public:
    CIdleWatcher(CWorkerNodeIdleThread& idle)
        : m_Idle(idle), m_RunningJobs(0) {}

    virtual ~CIdleWatcher() {};

    virtual void Notify(const CWorkerNodeJobContext& job, EEvent event)
    {
        if (event == eJobStarted) {
            ++m_RunningJobs;
            m_Idle.Suspend();
        } else if (event == eJobStopped) {
            --m_RunningJobs;
            if (m_RunningJobs == 0)
                m_Idle.Schedule();
        }
    }

private:
    CWorkerNodeIdleThread& m_Idle;
    volatile int m_RunningJobs;
};


/////////////////////////////////////////////////////////////////////////////
//

CGridWorkerNode::CGridWorkerNode(CNcbiApplication& app,
        IWorkerNodeJobFactory* job_factory) :
    m_JobProcessorFactory(job_factory),
    m_MaxThreads(1),
    m_NSTimeout(DEFAULT_NS_TIMEOUT),
    m_CommitJobInterval(COMMIT_JOB_INTERVAL_DEFAULT),
    m_CheckStatusPeriod(2),
    m_ExclusiveJobSemaphore(1, 1),
    m_IsProcessingExclusiveJob(false),
    m_TotalMemoryLimit(0),
    m_TotalTimeLimit(0),
    m_StartupTime(0),
    m_CleanupEventSource(new CWorkerNodeCleanup()),
    m_DiscoveryIteration(0),
    m_Listener(new CGridWorkerNodeApp_Listener()),
    m_App(app),
    m_SingleThreadForced(false)
{
    // Because m_TimelineSearchPattern.m_DiscoveryIteration == 0,
    // this is also the DiscoveryAction object; add it to the timeline.
    m_ImmediateActions.Push(&m_TimelineSearchPattern);

    if (!m_JobProcessorFactory.get())
        NCBI_THROW(CGridWorkerNodeException,
                 eJobFactoryIsNotSet, "The JobFactory is not set.");
}

CGridWorkerNode::~CGridWorkerNode()
{
    ITERATE(TTimelineEntries, it, m_TimelineEntryByAddress) {
        delete *it;
    }
}

void CGridWorkerNode::Init()
{
    m_Listener->OnInit(&m_App);

    IRWRegistry& reg = m_App.GetConfig();

    if (reg.GetBool("log", "merge_lines", false)) {
        SetDiagPostFlag(eDPF_PreMergeLines);
        SetDiagPostFlag(eDPF_MergeLines);
    }

    reg.Set(kNetScheduleAPIDriverName, "discover_low_priority_servers", "true");

    m_NetScheduleAPI = CNetScheduleAPI(reg);
    m_NetCacheAPI = CNetCacheAPI(reg, kEmptyStr, m_NetScheduleAPI);
}

int CGridWorkerNode::Run(
#ifdef NCBI_OS_UNIX
    ESwitch daemonize,
#endif
    string procinfo_file_name)
{
    const string kServerSec("server");

    LOG_POST_X(50, Info << GetJobFactory().GetJobVersion() <<
            " build " WN_BUILD_DATE);

    const IRegistry& reg = m_App.GetConfig();
    CConfig conf(reg);

    unsigned init_threads = 1;

    if (!m_SingleThreadForced) {
        string max_threads = reg.GetString(kServerSec, "max_threads", "auto");
        if (NStr::CompareNocase(max_threads, "auto") == 0)
            m_MaxThreads = GetCpuCount();
        else {
            try {
                m_MaxThreads = NStr::StringToUInt(max_threads);
            }
            catch (exception&) {
                m_MaxThreads = GetCpuCount();
                ERR_POST_X(51, "Could not convert [" << kServerSec <<
                    "] max_threads parameter to number.\n"
                    "Using \'auto\' option (" << m_MaxThreads << ").");
            }
        }
        init_threads = reg.GetInt(kServerSec,
                "init_threads", 1, 0, IRegistry::eReturn);
    }
    if (init_threads > m_MaxThreads)
        init_threads = m_MaxThreads;

    m_NSTimeout = reg.GetInt(kServerSec,
            "job_wait_timeout", DEFAULT_NS_TIMEOUT, 0, IRegistry::eReturn);

    unsigned thread_pool_timeout = reg.GetInt(kServerSec,
            "thread_pool_timeout", 30, 0, IRegistry::eReturn);

    const CArgs& args = m_App.GetArgs();

    unsigned int start_port, end_port;

    {{
        string control_port_arg(args["control_port"] ?
                args["control_port"].AsString() :
                reg.GetString(kServerSec, "control_port", "9300"));

        CTempString from_port, to_port;

        NStr::SplitInTwo(control_port_arg, "- ",
                from_port, to_port, NStr::eMergeDelims);

        start_port = NStr::StringToUInt(from_port);
        end_port = to_port.empty() ? start_port : NStr::StringToUInt(to_port);
    }}

#ifdef NCBI_OS_UNIX
    bool is_daemon = daemonize != eDefault ? daemonize == eOn :
            reg.GetBool(kServerSec, "daemon", false, 0, CNcbiRegistry::eReturn);
#endif

    {{
        string memlimitstr(reg.GetString(kServerSec,
                "total_memory_limit", kEmptyStr, IRegistry::eReturn));

        if (!memlimitstr.empty())
            m_TotalMemoryLimit = NStr::StringToUInt8_DataSize(memlimitstr);
    }}

    m_TotalTimeLimit = reg.GetInt(kServerSec,
            "total_time_limit", 0, 0, IRegistry::eReturn);

    m_StartupTime = time(0);

    vector<string> vhosts;

    NStr::Tokenize(reg.GetString(kServerSec,
        "master_nodes", kEmptyStr), " ;,", vhosts);

    ITERATE(vector<string>, it, vhosts) {
        string host, port;
        NStr::SplitInTwo(NStr::TruncateSpaces(*it), ":", host, port);
        if (!host.empty() && !port.empty())
            m_Masters.insert(SServerAddress(g_NetService_gethostbyname(host),
                    (unsigned short) NStr::StringToUInt(port)));
    }

    vhosts.clear();

    NStr::Tokenize(reg.GetString(kServerSec,
        "admin_hosts", kEmptyStr), " ;,", vhosts);

    ITERATE(vector<string>, it, vhosts) {
        unsigned int ha = CSocketAPI::gethostbyname(*it);
        if (ha != 0)
            m_AdminHosts.insert(ha);
    }

    m_CommitJobInterval = reg.GetInt(kServerSec, "commit_job_interval",
            COMMIT_JOB_INTERVAL_DEFAULT, 0, IRegistry::eReturn);
    if (m_CommitJobInterval == 0)
        m_CommitJobInterval = 1;

    m_CheckStatusPeriod = reg.GetInt(kServerSec,
            "check_status_period", 2, 0, IRegistry::eReturn);
    if (m_CheckStatusPeriod == 0)
        m_CheckStatusPeriod = 1;

    if (reg.HasEntry(kServerSec,"wait_server_timeout")) {
        ERR_POST_X(52, "[" << kServerSec <<
            "] \"wait_server_timeout\" is not used anymore.\n"
            "Use [" << kNetScheduleAPIDriverName <<
            "] \"communication_timeout\" parameter instead.");
    }

    unsigned max_total_jobs = reg.GetInt(kServerSec,
            "max_total_jobs", 0, 0, IRegistry::eReturn);

    CGridDebugContext::eMode debug_mode = CGridDebugContext::eGDC_NoDebug;
    string dbg_mode = reg.GetString("gw_debug", "mode", kEmptyStr);
    if (NStr::CompareNocase(dbg_mode, "gather")==0) {
        debug_mode = CGridDebugContext::eGDC_Gather;
    } else if (NStr::CompareNocase(dbg_mode, "execute")==0) {
        debug_mode = CGridDebugContext::eGDC_Execute;
    }
    if (debug_mode != CGridDebugContext::eGDC_NoDebug) {
        CGridDebugContext& debug_context = CGridDebugContext::Create(
            debug_mode, m_NetCacheAPI);
        debug_context.SetRunName(reg.GetString("gw_debug",
                "run_name", m_App.GetProgramDisplayName()));
        if (debug_mode == CGridDebugContext::eGDC_Gather) {
            max_total_jobs =
                reg.GetInt("gw_debug","gather_nrequests",1,0,IRegistry::eReturn);
        } else if (debug_mode == CGridDebugContext::eGDC_Execute) {
            string files =
                reg.GetString("gw_debug", "execute_requests", kEmptyStr);
            max_total_jobs = 0;
            debug_context.SetExecuteList(files);
        }
#ifdef NCBI_OS_UNIX
        is_daemon = false;
#endif
    }

    if (!procinfo_file_name.empty()) {
        // Make sure the process info file is writable.
        CFile proc_info_file(procinfo_file_name);
        if (proc_info_file.Exists()) {
            // Already exists
            if (!proc_info_file.CheckAccess(CFile::fWrite)) {
                fprintf(stderr, "'%s' is not writable.\n",
                        procinfo_file_name.c_str());
                return 21;
            }
        } else {
            // The process info file does not exist yet.
            // Create a temporary file in the designated directory
            // to make sure the location is writable.
            string test_file_name = procinfo_file_name + ".TEST";
            FILE* f = fopen(test_file_name.c_str(), "w");
            if (f == NULL) {
                perror(("Cannot create " + test_file_name).c_str());
                return 22;
            }
            fclose(f);
            // The worker node may fail (e.g. if the control port is busy)
            // before the process info file can be written; remove the
            // empty file to avoid confusion.
            remove(test_file_name.c_str());
        }
    }

#ifdef NCBI_OS_UNIX
    if (is_daemon) {
        LOG_POST_X(53, "Entering UNIX daemon mode...");
        bool daemon = CProcess::Daemonize("/dev/null",
                                          CProcess::fDontChroot |
                                          CProcess::fKeepStdin  |
                                          CProcess::fKeepStdout);
        if (!daemon) {
            return 0;
        }
    }
#endif

    AddJobWatcher(CGridGlobals::GetInstance().GetJobWatcher());

    m_JobCommitterThread = new CJobCommitterThread(this);

    CRef<CGridControlThread> control_thread(
        new CGridControlThread(this, start_port, end_port));

    try {
        control_thread->Prepare();
    }
    catch (CServer_Exception& e) {
        if (e.GetErrCode() != CServer_Exception::eCouldntListen)
            throw;
        NCBI_THROW_FMT(CGridWorkerNodeException, ePortBusy,
            "Couldn't start a listener on a port from the specified "
            "control port range; last port tried: " <<
            control_thread->GetControlPort() << ". Another process "
            "(probably another instance of this worker node) is occupying "
            "the port(s).");
    }

    string control_port_str(
            NStr::NumericToString(control_thread->GetControlPort()));
    LOG_POST_X(60, "Control port: " << control_port_str);
    m_NetScheduleAPI.SetAuthParam("control_port", control_port_str);
    m_NetScheduleAPI.SetAuthParam("client_host", CSocketAPI::gethostname());

    if (m_NetScheduleAPI->m_ClientNode.empty()) {
        string client_node(m_NetScheduleAPI->
                m_Service->m_ServerPool->m_ClientName);
        client_node.append(2, ':');
        client_node.append(CSocketAPI::gethostname());
        client_node.append(1, ':');
        client_node.append(NStr::NumericToString(
            control_thread->GetControlPort()));
        m_NetScheduleAPI.SetClientNode(client_node);

        string session(NStr::NumericToString(CProcess::GetCurrentPid()) + '@');
        session += NStr::NumericToString(GetFastLocalTime().GetTimeT());
        session += ':';
        session += GetDiagContext().GetStringUID();

        m_NetScheduleAPI.SetClientSession(session);
    }

    m_NetScheduleAPI.SetProgramVersion(m_JobProcessorFactory->GetJobVersion());

    // Now that most of parameters have been checked, create the
    // "procinfo" file.
    if (!procinfo_file_name.empty()) {
        FILE* procinfo_file;
        if ((procinfo_file = fopen(procinfo_file_name.c_str(), "wt")) == NULL) {
            perror(procinfo_file_name.c_str());
            return 23;
        }
        fprintf(procinfo_file, "pid: %lu\nport: %s\n"
                "client_node: %s\nclient_session: %s\n",
                (unsigned long) CDiagContext::GetPID(),
                control_port_str.c_str(),
                m_NetScheduleAPI->m_ClientNode.c_str(),
                m_NetScheduleAPI->m_ClientSession.c_str());
        fclose(procinfo_file);
    }

    CRequestContext& request_context = CDiagContext::GetRequestContext();

    request_context.SetSessionID(m_NetScheduleAPI->m_ClientSession);

    m_NSExecutor = m_NetScheduleAPI.GetExecutor();

    CDeadline max_wait_for_servers(
            TWorkerNode_MaxWaitForServers::GetDefault());

    for (;;) {
        try {
            CNetScheduleAdmin::TQueueInfo queue_info;

            m_NetScheduleAPI.GetAdmin().GetQueueInfo(queue_info);

            m_QueueTimeout = NStr::StringToUInt(queue_info["timeout"]);

            m_QueueEmbeddedOutputSize = m_NetScheduleAPI->m_UseEmbeddedStorage ?
                    m_NetScheduleAPI.GetServerParams().max_output_size : 0;
            break;
        }
        catch (CException& e) {
            LOG_POST(Error << e);
            int s = (int) m_NSTimeout;
            do {
                if (CGridGlobals::GetInstance().IsShuttingDown() ||
                        max_wait_for_servers.GetRemainingTime().IsZero())
                    return 1;
                SleepSec(1);
            } while (--s > 0);
        }
    }

    CGridGlobals::GetInstance().SetReuseJobObject(reg.GetBool(kServerSec,
            "reuse_job_object", false, 0, CNcbiRegistry::eReturn));
    CWNJobWatcher& watcher(CGridGlobals::GetInstance().GetJobWatcher());
    watcher.SetMaxJobsAllowed(max_total_jobs);
    watcher.SetMaxFailuresAllowed(reg.GetInt(kServerSec,
            "max_failed_jobs", 0, 0, IRegistry::eReturn));
    watcher.SetInfiniteLoopTime(reg.GetInt(kServerSec,
            "infinite_loop_time", 0, 0, IRegistry::eReturn));
    CGridGlobals::GetInstance().SetWorker(*this);
    CGridGlobals::GetInstance().SetUDPPort(
            m_NSExecutor->m_NotificationHandler.GetPort());

    IWorkerNodeIdleTask* task = NULL;

    unsigned idle_run_delay = reg.GetInt(kServerSec,
        "idle_run_delay", 30, 0, IRegistry::eReturn);

    unsigned auto_shutdown = reg.GetInt(kServerSec,
        "auto_shutdown_if_idle", 0, 0, IRegistry::eReturn);

    if (idle_run_delay > 0)
        task = GetJobFactory().GetIdleTask();
    if (task || auto_shutdown > 0) {
        m_IdleThread.Reset(new CWorkerNodeIdleThread(task, *this,
                task ? idle_run_delay : auto_shutdown, auto_shutdown));
        m_IdleThread->Run();
        AddJobWatcher(*(new CIdleWatcher(*m_IdleThread)), eTakeOwnership);
    }

    m_JobCommitterThread->Run();
    control_thread->Run();

    LOG_POST_X(54, Info << "\n=================== NEW RUN : " <<
        CGridGlobals::GetInstance().GetStartTime().AsString() <<
            " ===================\n" <<
        GetJobFactory().GetJobVersion() << " build " WN_BUILD_DATE <<
        " is started.\n"
        "Waiting for control commands on " << CSocketAPI::gethostname() <<
            ":" << control_thread->GetControlPort() << "\n"
        "Queue name: " << GetQueueName() << "\n"
        "Maximum job threads: " << m_MaxThreads << "\n");

    m_Listener->OnGridWorkerStart();

    auto_ptr<CStdPoolOfThreads> thread_pool;

    _ASSERT(m_MaxThreads > 0);

    if (m_MaxThreads > 1) {
        thread_pool.reset(new CStdPoolOfThreads(m_MaxThreads, 0));
        try {
            thread_pool->Spawn(init_threads);
        }
        catch (exception& ex) {
            ERR_POST_X(26, ex.what());
            CGridGlobals::GetInstance().RequestShutdown(
                CNetScheduleAdmin::eShutdownImmediate);
            return 2;
        }
    }

    CNetScheduleJob job;
    max_wait_for_servers = CDeadline(
            TWorkerNode_MaxWaitForServers::GetDefault());

    m_LogRequested = reg.GetBool(kServerSec,
            "log", false, 0, IRegistry::eReturn);
    m_ProgressLogRequested = reg.GetBool(kServerSec,
            "log_progress", false, 0, IRegistry::eReturn);

    auto_ptr<CWorkerNodeJobContext> job_context(
            m_JobCommitterThread->AllocJobContext());

    unsigned try_count = 0;
    while (!CGridGlobals::GetInstance().IsShuttingDown()) {
        try {
            if (m_MaxThreads > 1) {
                try {
                    thread_pool->WaitForRoom(thread_pool_timeout);
                } catch (CBlockingQueueException&) {
                    // threaded pool is busy
                    continue;
                }
            }

            if (x_GetNextJob(job_context->GetJob())) {
                job_context->Reset();

                if (m_MaxThreads > 1) {
                    try {
                        thread_pool->AcceptRequest(CRef<CStdRequest>(
                                new CWorkerNodeRequest(job_context.get())));
                    } catch (CBlockingQueueException& ex) {
                        ERR_POST_X(28, ex);
                        // that must not happen after CBlockingQueue is fixed
                        _ASSERT(0);
                        job_context->ReturnJob();
                        m_JobCommitterThread->PutJobContextBackAndCommitJob(
                                job_context.get());
                    }
                } else
                    job_context->x_RunJob();
                job_context.release();
                job_context.reset(m_JobCommitterThread->AllocJobContext());
            }
            max_wait_for_servers =
                CDeadline(TWorkerNode_MaxWaitForServers::GetDefault());
        } catch (CNetSrvConnException& e) {
            SleepMilliSec(s_GetRetryDelay());
            if (e.GetErrCode() == CNetSrvConnException::eConnectionFailure &&
                    !max_wait_for_servers.GetRemainingTime().IsZero())
                continue;
            ERR_POST(Critical << "Could not connect to the "
                    "configured servers, exiting...");
            CGridGlobals::GetInstance().RequestShutdown(
                    CNetScheduleAdmin::eShutdownImmediate);
        } catch (CNetServiceException& ex) {
            ERR_POST_X(40, ex);
            if (++try_count >= TServConn_ConnMaxRetries::GetDefault()) {
                CGridGlobals::GetInstance().RequestShutdown(
                    CNetScheduleAdmin::eShutdownImmediate);
            } else {
                SleepMilliSec(s_GetRetryDelay());
                continue;
            }
        } catch (exception& ex) {
            if (TWorkerNode_StopOnJobErrors::GetDefault()) {
                ERR_POST_X(29, ex.what());
                CGridGlobals::GetInstance().RequestShutdown(
                    CNetScheduleAdmin::eShutdownImmediate);
            }
        }
        try_count = 0;
    }
    LOG_POST_X(31, Info << "Shutting down...");
    LOG_POST(Info << "Stopping Control thread...");
    control_thread->Stop();
    if (reg.GetBool(kServerSec,
            "force_exit", false, 0, CNcbiRegistry::eReturn)) {
        ERR_POST_X(45, "Force exit");
    } else if (m_MaxThreads > 1) {
        try {
            LOG_POST_X(32, Info << "Stopping worker threads...");
            thread_pool->KillAllThreads(true);
            thread_pool.reset(0);
        }
        catch (exception& ex) {
            ERR_POST_X(33, "Could not stop worker threads: " << ex.what());
        }
    }

    LOG_POST_X(55, Info << "Stopping Committer threads...");
    m_JobCommitterThread->Stop();

    CNcbiOstrstream os;
    CGridGlobals::GetInstance().GetJobWatcher().Print(os);
    LOG_POST_X(56, Info << string(CNcbiOstrstreamToString(os)));

    if (m_IdleThread) {
        if (!m_IdleThread->IsShutdownRequested()) {
            LOG_POST_X(57, "Stopping Idle thread...");
            m_IdleThread->RequestShutdown();
        }
        m_IdleThread->Join();
    }

    m_JobCommitterThread->Join();
    control_thread->Join();

    try {
        GetNSExecutor().ClearNode();
    }
    catch (CNetServiceException& ex) {
        // if server does not understand this new command just ignore the error
        if (ex.GetErrCode() != CNetServiceException::eCommunicationError
            || NStr::Find(ex.what(), "Server error:Unknown request") == NPOS) {
            ERR_POST_X(35, "Could not unregister from NetSchedule services: "
                       << ex);
        }
    }
    catch (exception& ex) {
        ERR_POST_X(36, "Could not unregister from NetSchedule services: " <<
                ex.what());
    }

    LOG_POST_X(38, Info << "Worker Node has been stopped.");

    CRef<CGridCleanupThread> cleanup_thread(
        new CGridCleanupThread(this, m_Listener.get()));

    cleanup_thread->Run();

    if (cleanup_thread->Wait(thread_pool_timeout)) {
        cleanup_thread->Join();
        LOG_POST_X(58, Info << "Cleanup thread finished");
    } else {
        ERR_POST_X(59, "Clean-up thread timed out");
    }

    return CGridGlobals::GetInstance().GetExitCode();
}

void CGridWorkerNode::RequestShutdown()
{
    CGridGlobals::GetInstance().
        RequestShutdown(CNetScheduleAdmin::eShutdownImmediate);
}


void CGridWorkerNode::AddJobWatcher(IWorkerNodeJobWatcher& job_watcher,
                                           EOwnership owner)
{
    if (m_Watchers.find(&job_watcher) == m_Watchers.end())
        m_Watchers[&job_watcher] = owner == eTakeOwnership ?
                AutoPtr<IWorkerNodeJobWatcher>(&job_watcher) :
                AutoPtr<IWorkerNodeJobWatcher>();
};

void CGridWorkerNode::SetListener(IGridWorkerNodeApp_Listener* listener)
{
    m_Listener.reset(listener ? listener : new CGridWorkerNodeApp_Listener());
}


void CGridWorkerNode::x_NotifyJobWatchers(const CWorkerNodeJobContext& job,
    IWorkerNodeJobWatcher::EEvent event)
{
    CFastMutexGuard guard(m_JobWatcherMutex);
    NON_CONST_ITERATE(TJobWatchers, it, m_Watchers) {
        try {
            const_cast<IWorkerNodeJobWatcher*>(it->first)->Notify(job, event);
        }
        NCBI_CATCH_ALL_X(66, "Error while notifying a job watcher");
    }
}

bool CGridWorkerNode::x_GetJobWithAffinityList(SNetServerImpl* server,
        const CDeadline* timeout, CNetScheduleJob& job,
        CNetScheduleExecutor::EJobAffinityPreference affinity_preference,
        const string& affinity_list)
{
    return m_NSExecutor->ExecGET(server, m_NSExecutor->
            m_NotificationHandler.CmdAppendTimeoutAndClientInfo(
                    CNetScheduleNotificationHandler::MkBaseGETCmd(
                            affinity_preference, affinity_list),
                    timeout), job);
}

bool CGridWorkerNode::x_GetJobWithAffinityLadder(SNetServerImpl* server,
        const CDeadline* timeout, CNetScheduleJob& job)
{
    if (m_NSExecutor->m_API->m_AffinityLadder.empty())
        return x_GetJobWithAffinityList(server, timeout, job,
                m_NSExecutor->m_AffinityPreference, kEmptyStr);

    list<string>::const_iterator it =
            m_NSExecutor->m_API->m_AffinityLadder.begin();

    for (;;) {
        string affinity_list = *it;
        if (++it == m_NSExecutor->m_API->m_AffinityLadder.end())
            return x_GetJobWithAffinityList(server, timeout, job,
                    m_NSExecutor->m_AffinityPreference, affinity_list);
        else if (x_GetJobWithAffinityList(server, NULL, job,
                CNetScheduleExecutor::ePreferredAffinities, affinity_list))
            return true;
    }
}

// Action on a *detached* timeline entry.
bool CGridWorkerNode::x_PerformTimelineAction(
        CGridWorkerNode::SNotificationTimelineEntry* timeline_entry,
        CNetScheduleJob& job)
{
    if (timeline_entry->IsDiscoveryAction()) {
        ++m_DiscoveryIteration;
        for (CNetServiceIterator it = m_NetScheduleAPI.GetService().Iterate(
                CNetService::eIncludePenalized); it; ++it) {
            m_TimelineSearchPattern.m_ServerAddress =
                    (*it)->m_ServerInPool->m_Address;
            TTimelineEntries::iterator existing_entry(
                    m_TimelineEntryByAddress.find(&m_TimelineSearchPattern));

            if (existing_entry != m_TimelineEntryByAddress.end()) {
                (*existing_entry)->m_DiscoveryIteration = m_DiscoveryIteration;
                if (!(*existing_entry)->IsInTimeline())
                    m_ImmediateActions.Push(*existing_entry);
            } else {
                SNotificationTimelineEntry* new_entry =
                        new SNotificationTimelineEntry(
                                m_TimelineSearchPattern.m_ServerAddress,
                                        m_DiscoveryIteration);

                m_TimelineEntryByAddress.insert(new_entry);
                m_ImmediateActions.Push(new_entry);
            }
        }

        timeline_entry->ResetTimeout(m_NSTimeout);
        m_Timeline.Push(timeline_entry);
        return false;
    }

    // Skip servers that disappeared from LBSM.
    if (timeline_entry->m_DiscoveryIteration != m_DiscoveryIteration)
        return false;

    CNetServer server(m_NetScheduleAPI->m_Service.GetServer(
            timeline_entry->m_ServerAddress));

    timeline_entry->ResetTimeout(m_NSTimeout);

    try {
        if (x_GetJobWithAffinityLadder(server,
                &timeline_entry->GetTimeout(), job)) {
            // A job has been returned; add the server to
            // m_ImmediateActions because there can be more
            // jobs in the queue.
            m_ImmediateActions.Push(timeline_entry);
            return true;
        } else {
            // No job has been returned by this server;
            // query the server later.
            m_Timeline.Push(timeline_entry);
            return false;
        }
    }
    catch (CNetSrvConnException& e) {
        // Because a connection error has occurred, do not
        // put this server back to the timeline.
        LOG_POST(Warning << e.GetMsg());
        return false;
    }
}

void CGridWorkerNode::x_ProcessRequestJobNotification()
{
    CNetServer server;

    if (m_NSExecutor->m_NotificationHandler.CheckRequestJobNotification(
            m_NSExecutor, &server)) {
        m_TimelineSearchPattern.m_ServerAddress =
                server->m_ServerInPool->m_Address;

        TTimelineEntries::iterator it(
                m_TimelineEntryByAddress.find(&m_TimelineSearchPattern));

        if (it == m_TimelineEntryByAddress.end()) {
            SNotificationTimelineEntry* new_entry =
                    new SNotificationTimelineEntry(
                            m_TimelineSearchPattern.m_ServerAddress,
                                    m_DiscoveryIteration);

            m_ImmediateActions.Push(new_entry);
            m_TimelineEntryByAddress.insert(new_entry);
        } else if (!(*it)->IsInTimeline(&m_ImmediateActions)) {
            (*it)->Cut();
            m_ImmediateActions.Push(*it);
        }
    }
}

bool CGridWorkerNode::x_WaitForNewJob(CNetScheduleJob& job)
{
    for (;;) {
        while (!m_ImmediateActions.IsEmpty()) {
            if (x_PerformTimelineAction(m_ImmediateActions.Shift(), job))
                return true;

            while (!m_Timeline.IsEmpty() && m_Timeline.GetHead()->TimeHasCome())
                m_ImmediateActions.Push(m_Timeline.Shift());

            // Check if there's a notification in the UDP socket.
            while (m_NSExecutor->m_NotificationHandler.ReceiveNotification())
                x_ProcessRequestJobNotification();
        }

        if (CGridGlobals::GetInstance().IsShuttingDown())
            return false;

        if (!m_Timeline.IsEmpty()) {
            if (m_NSExecutor->m_NotificationHandler.WaitForNotification(
                    m_Timeline.GetHead()->GetTimeout()))
                x_ProcessRequestJobNotification();
            else if (x_PerformTimelineAction(m_Timeline.Shift(), job))
                return true;
        }
    }
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
            return false;
        }
    } else {
        if (!x_AreMastersBusy()) {
            SleepSec(m_NSTimeout);
            return false;
        }

        if (!WaitForExclusiveJobToFinish())
            return false;

        job_exists = x_WaitForNewJob(job);

        if (job_exists && job.mask & CNetScheduleAPI::eExclusiveJob) {
            if (EnterExclusiveMode())
                job_exists = true;
            else {
                x_ReturnJob(job);
                job_exists = false;
            }
        }
    }
    if (job_exists && CGridGlobals::GetInstance().IsShuttingDown()) {
        x_ReturnJob(job);
        return false;
    }
    return job_exists;
}

void CGridWorkerNode::x_ReturnJob(const CNetScheduleJob& job)
{
    CGridDebugContext* debug_context = CGridDebugContext::GetInstance();
    if (!debug_context ||
            debug_context->GetDebugMode() != CGridDebugContext::eGDC_Execute) {
         GetNSExecutor().ReturnJob(job.job_id, job.auth_token);
    }
}

size_t CGridWorkerNode::GetServerOutputSize()
{
    return m_QueueEmbeddedOutputSize;
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
    ITERATE(set<SServerAddress>, it, m_Masters) {
        STimeout tmo = {0, 500};
        CSocket socket(it->host, it->port, &tmo, eOff);
        if (socket.GetStatus(eIO_Open) != eIO_Success)
            continue;

        CNcbiOstrstream os;
        os << GetClientName() << endl <<
                GetNetScheduleAPI().GetQueueName()  << ";" <<
                GetServiceName() << endl <<
                "GETLOAD" << endl << ends;

        if (socket.Write(os.str(), (size_t)os.pcount()) != eIO_Success) {
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
            ERR_POST_X(43, "Worker Node at " << it->AsString() <<
                " returned error: " << msg);
        } else if (NStr::StartsWith(reply, "OK:")) {
            string msg;
            NStr::Replace(reply, "OK:", "", msg);
            try {
                int load = NStr::StringToInt(msg);
                if (load > 0)
                    return false;
            } catch (exception&) {}
        } else {
            ERR_POST_X(44, "Worker Node at " << it->AsString() <<
                " returned unknown reply: " << reply);
        }
    }
    return true;
}

bool CGridWorkerNode::EnterExclusiveMode()
{
    if (m_ExclusiveJobSemaphore.TryWait()) {
        _ASSERT(!m_IsProcessingExclusiveJob);

        m_IsProcessingExclusiveJob = true;
        return true;
    }
    return false;
}

void CGridWorkerNode::LeaveExclusiveMode()
{
    _ASSERT(m_IsProcessingExclusiveJob);

    m_IsProcessingExclusiveJob = false;
    m_ExclusiveJobSemaphore.Post();
}

bool CGridWorkerNode::WaitForExclusiveJobToFinish()
{
    if (m_ExclusiveJobSemaphore.TryWait(m_NSTimeout)) {
        m_ExclusiveJobSemaphore.Post();
        return true;
    }
    return false;
}

void CGridWorkerNode::DisableDefaultRequestEventLogging(
    CGridWorkerNode::EDisabledRequestEvents disabled_events)
{
    s_ReqEventsDisabled = disabled_events;
}

IWorkerNodeCleanupEventSource* CGridWorkerNode::GetCleanupEventSource()
{
    return m_CleanupEventSource;
}

END_NCBI_SCOPE
