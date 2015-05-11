/* $Id$
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
 * Authors:  Aaron Ucko, Victor Joukov
 *
 */

/// @file server.cpp
/// Framework for a multithreaded network server

#include <ncbi_pch.hpp>
#include <corelib/ncbi_param.hpp>
#include "connection_pool.hpp"
#include <connect/ncbi_buffer.h>
#include <connect/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Connect_ThrServer


BEGIN_NCBI_SCOPE


NCBI_PARAM_DECL(bool, server, Catch_Unhandled_Exceptions);
NCBI_PARAM_DEF_EX(bool, server, Catch_Unhandled_Exceptions, true, 0,
                  CSERVER_CATCH_UNHANDLED_EXCEPTIONS);
typedef NCBI_PARAM_TYPE(server, Catch_Unhandled_Exceptions) TParamServerCatchExceptions;
static CSafeStatic<TParamServerCatchExceptions> s_ServerCatchExceptions;


/////////////////////////////////////////////////////////////////////////////
// IServer_MessageHandler implementation
void IServer_MessageHandler::OnRead(void)
{
    CSocket &socket = GetSocket();
    CServer_Connection* conn = static_cast<CServer_Connection*>(&socket);
    char read_buf[4096];
    size_t n_read;
    EIO_Status status = socket.Read(read_buf, sizeof(read_buf), &n_read);
    switch (status) {
    case eIO_Success:
        break;
    case eIO_Timeout:
        this->OnTimeout();
        return;
    case eIO_Closed:
        this->OnClose(IServer_ConnectionHandler::eClientClose);
        return;
    default:
        // TODO: ??? OnError
        return;
    }
    int message_tail;
    char *buf_ptr = read_buf;
    for ( ;n_read > 0  &&  conn->type == eActiveSocket; ) {
        message_tail = this->CheckMessage(&m_Buffer, buf_ptr, n_read);
        // TODO: what should we do if message_tail > n_read?
        if (message_tail < 0) {
            return;
        } else {
            this->OnMessage(m_Buffer);
        }
        int consumed = int(n_read) - message_tail;
        buf_ptr += consumed;
        n_read -= consumed;
    }
}


/////////////////////////////////////////////////////////////////////////////
// Server_CheckLineMessage implementation
int Server_CheckLineMessage(BUF* buffer, const void *data, size_t size,
                            bool& seen_CR)
{
    size_t n, skip;
    const char * msg = (const char *) data;
    skip = 0;
    if (size && seen_CR && msg[0] == '\n') {
        ++skip;
    }
    seen_CR = false;
    for (n = skip; n < size; ++n) {
        if (msg[n] == '\r' || msg[n] == '\n' || msg[n] == '\0') {
            seen_CR = msg[n] == '\r';
            break;
        }
    }
    BUF_Write(buffer, msg+skip, n-skip);
    return int(size - n - 1);
}


CBlockingQueue_ForServer::TItemHandle
CBlockingQueue_ForServer::Put(const TRequest& data)
{
    CMutexGuard guard(m_Mutex);
    if (m_Queue.empty()) {
#ifdef NCBI_HAVE_CONDITIONAL_VARIABLE
        m_GetCond.SignalAll();
#else
        m_GetSem.TryWait(); // is this still needed?
        m_GetSem.Post();
#endif
    }
    TItemHandle handle(new CQueueItem(data));
    m_Queue.push_back(handle);
    return handle;
}

CBlockingQueue_ForServer::TItemHandle
CBlockingQueue_ForServer::GetHandle(void)
{
    CMutexGuard guard(m_Mutex);

    while (m_Queue.empty()) {
#ifdef NCBI_HAVE_CONDITIONAL_VARIABLE
        m_GetCond.WaitForSignal(m_Mutex);
#else
        m_GetSem.TryWait();
        m_GetSem.Post();
#endif
    }

    TItemHandle handle(m_Queue.front());
    m_Queue.pop_front();
#ifndef NCBI_HAVE_CONDITIONAL_VARIABLE
    if (!m_Queue.empty()) {
        m_GetSem.TryWait();
        m_GetSem.Post();
    }
#endif

    guard.Release(); // avoid possible deadlocks from x_SetStatus
    handle->x_SetStatus(CQueueItemBase::eActive);
    return handle;
}


CThreadInPool_ForServer::CAutoUnregGuard::CAutoUnregGuard(TThread* thr)
    : m_Thread(thr)
{}

CThreadInPool_ForServer::CAutoUnregGuard::~CAutoUnregGuard(void)
{
    m_Thread->x_UnregisterThread();
}

void
CThreadInPool_ForServer::CountSelf(void)
{
    _ASSERT( !m_Counted );
    m_Pool->m_ThreadCount.Add(1);
    m_Counted = true;
}

CThreadInPool_ForServer::~CThreadInPool_ForServer(void)
{
    if (m_Counted) {
        m_Pool->m_ThreadCount.Add(-1);
    }
}

void
CThreadInPool_ForServer::x_UnregisterThread(void)
{
    m_Pool->UnRegister(*this);
}

void
CThreadInPool_ForServer::x_HandleOneRequest(bool catch_all)
{
    TItemHandle handle(m_Pool->GetHandle());
    if (catch_all) {
        try {
            ProcessRequest(handle);
        } catch (std::exception& e) {
            handle->MarkAsForciblyCaught();
            NCBI_REPORT_EXCEPTION_X(9, "Exception from thread in pool: ", e);
            // throw;
        } catch (...) {
            handle->MarkAsForciblyCaught();
            // silently propagate non-standard exceptions because they're
            // likely to be CExitThreadException.
            throw;
        }
    }
    else {
        ProcessRequest(handle);
    }
}

void*
CThreadInPool_ForServer::Main(void)
{
    if (!m_Pool->m_ThrSuffix.empty()) {
        CMutexGuard guard(CNcbiApplication::GetInstanceMutex());
        string thr_name = CNcbiApplication::Instance()->GetProgramDisplayName();
        thr_name += m_Pool->m_ThrSuffix;
        SetCurrentThreadName(thr_name.c_str());
    }

    if ( !m_Pool->Register(*this) ) {
        ERR_POST(Warning << "New worker thread blocked at the last minute.");
        return NULL;
    }
    CAutoUnregGuard guard(this);

    bool catch_all = TParamThreadPoolCatchExceptions::GetDefault();
    for (;;) {
        x_HandleOneRequest(catch_all);
    }

    return NULL;
}

void
CThreadInPool_ForServer::ProcessRequest(TItemHandle handle)
{
    TCompletingHandle completer = handle;
    ProcessRequest(completer->GetRequest());
}


CPoolOfThreads_ForServer::CPoolOfThreads_ForServer(unsigned int max_threads,
                                                   const string& thr_suffix)
    : m_MaxThreads(max_threads),
      m_ThrSuffix(thr_suffix),
      m_KilledAll(false)
{
    m_ThreadCount.Set(0);
    m_PutQueueNum.Set(0);
    m_GetQueueNum.Set(0);

    // The original developer used malloc(...) here to allocate the required
    // number of bytes for an array. It is unknown why that decision was made.
    // Later on it was decided to replace malloc(...) with new to have a pure
    // C++ style in the code.
    m_Queues = (TQueue**) new char[m_MaxThreads * sizeof(m_Queues[0])];

    for (TACValue i = 0; i < m_MaxThreads; ++i) {
        m_Queues[i] = new TQueue();
    }
}

CPoolOfThreads_ForServer::~CPoolOfThreads_ForServer(void)
{
    try {
        KillAllThreads(false);
    } catch(...) {}    // Just to be sure that we will not throw from the destructor.

    CAtomicCounter::TValue n = m_ThreadCount.Get();
    if (n) {
        ERR_POST_X(10, Warning << "CPoolOfThreads_ForServer::~CPoolOfThreads_ForServer: "
                               << n << " thread(s) still active");
    } else {
        // It seems to be safe to destroy the allocated array if all the
        // threads were stopped.
        for (TACValue i = 0; i < m_MaxThreads; ++i) {
            delete m_Queues[i];
        }
        delete [] (char *)(m_Queues);
    }
    // Just in case let's deliberately not destroy all queues.
}

void
CPoolOfThreads_ForServer::Spawn(unsigned int num_threads)
{
    for (unsigned int i = 0; i < num_threads; i++)
    {
        CRef<TThread> thr(NewThread());
        thr->CountSelf();
        thr->Run();
    }
}

void
CPoolOfThreads_ForServer::AcceptRequest(const TRequest& req)
{
    Uint4 q_num = Uint4(m_PutQueueNum.Add(1)) % m_MaxThreads;
    m_Queues[q_num]->Put(req);
}

CPoolOfThreads_ForServer::TItemHandle
CPoolOfThreads_ForServer::GetHandle(void)
{
    Uint4 q_num = Uint4(m_GetQueueNum.Add(1)) % m_MaxThreads;
    return m_Queues[q_num]->GetHandle();
}


class CFatalRequest_ForServer : public CStdRequest
{
protected:
    void Process(void) { CThread::Exit(0); } // Kill the current thread
};


void
CPoolOfThreads_ForServer::KillAllThreads(bool wait)
{
    m_KilledAll = true;

    CRef<CStdRequest> poison(new CFatalRequest_ForServer);

    for (TACValue i = 0;  i < m_MaxThreads; ++i) {
        AcceptRequest(poison);
    }
    NON_CONST_ITERATE(TThreads, it, m_Threads) {
        if (wait) {
            (*it)->Join();
        } else {
            (*it)->Detach();
        }
    }
    m_Threads.clear();
}


bool
CPoolOfThreads_ForServer::Register(TThread& thread)
{
    CMutexGuard guard(m_Mutex);
    if (m_KilledAll) {
        return false;
    } else {
        m_Threads.push_back(CRef<TThread>(&thread));
        return true;
    }
}

void
CPoolOfThreads_ForServer::UnRegister(TThread& thread)
{
    CMutexGuard guard(m_Mutex);
    if (!m_KilledAll) {
        TThreads::iterator it = find(m_Threads.begin(), m_Threads.end(),
                                     CRef<TThread>(&thread));
        if (it != m_Threads.end()) {
            (*it)->Detach();
            m_Threads.erase(it);
        }
    }
}



/////////////////////////////////////////////////////////////////////////////
// Abstract class for CAcceptRequest and CServerConnectionRequest
class CServer_Request : public CStdRequest
{
public:
    CServer_Request(EServIO_Event event,
                    CServer_ConnectionPool& conn_pool,
                    const STimeout* timeout)
        : m_Event(event), m_ConnPool(conn_pool), m_IdleTimeout(timeout) {}

    virtual void Cancel(void) = 0;

protected:
    EServIO_Event            m_Event;
    CServer_ConnectionPool&  m_ConnPool;
    const STimeout*          m_IdleTimeout;
} ;


/////////////////////////////////////////////////////////////////////////////
// CAcceptRequest
class CAcceptRequest : public CServer_Request
{
public:
    CAcceptRequest(EServIO_Event event,
                   CServer_ConnectionPool& conn_pool,
                   const STimeout* timeout,
                   CServer_Listener* listener);
    virtual void Process(void);
    virtual void Cancel(void);
private:
    void x_DoProcess(void);

    CServer_Connection* m_Connection;
} ;

CAcceptRequest::CAcceptRequest(EServIO_Event event,
                               CServer_ConnectionPool& conn_pool,
                               const STimeout* timeout,
                               CServer_Listener* listener)
    : CServer_Request(event, conn_pool, timeout),
      m_Connection(NULL)
{
    // Accept connection in main thread to avoid race for listening
    // socket's accept method, but postpone connection's OnOpen for
    // pool thread because it can be arbitrarily long.
    static const STimeout kZeroTimeout = { 0, 0 };
    auto_ptr<CServer_Connection> conn(
                        new CServer_Connection(listener->m_Factory->Create()));
    if (listener->Accept(*conn, &kZeroTimeout) != eIO_Success)
        return;
/*
#ifdef NCBI_OS_UNIX
    if (conn->Wait(eIO_Write, &kZeroTimeout) == eIO_Unknown) {
        int fd;
        _VERIFY(conn->GetOSHandle(&fd, sizeof(fd)) == eIO_Success);
        if (fd >= 1024) {
            ERR_POST(Error << "Accepted unpollable file descriptor "
                     << fd << ", aborting connection");
            conn->OnOverflow(eOR_UnpollableSocket);
            conn->Abort();
            return;
        }
    }
#endif
*/
    conn->SetTimeout(eIO_ReadWrite, m_IdleTimeout);
    m_Connection = conn.release();
}

void CAcceptRequest::x_DoProcess(void)
{
    if (m_ConnPool.Add(m_Connection, eActiveSocket)) {
        m_Connection->OnSocketEvent(eServIO_Open);
        m_ConnPool.SetConnType(m_Connection, eInactiveSocket);
    }
    else {
        // The connection pool is full
        // This place is the only one which can call OnOverflow now
        m_Connection->OnOverflow(eOR_ConnectionPoolFull);
        delete m_Connection;
    }
}

void CAcceptRequest::Process(void)
{
    if (!m_Connection) return;
    if (s_ServerCatchExceptions->Get()) {
        try {
            x_DoProcess();
        } STD_CATCH_ALL_X(5, "CAcceptRequest::Process");
    }
    else {
        x_DoProcess();
    }
}

void CAcceptRequest::Cancel(void)
{
    // As of now, Cancel can not be called.
    // See comment at CServer::CreateRequest
    if (m_Connection) {
        m_Connection->OnOverflow(eOR_RequestQueueFull);
        delete m_Connection;
    }
}

/////////////////////////////////////////////////////////////////////////////
// CServerConnectionRequest
class CServerConnectionRequest : public CServer_Request
{
public:
    CServerConnectionRequest(EServIO_Event           event,
                             CServer_ConnectionPool& conn_pool,
                             const STimeout*         timeout,
                             CServer_Connection*     connection)
        : CServer_Request(event, conn_pool, timeout),
          m_Connection(connection)
    {}
    virtual void Process(void);
    virtual void Cancel(void);
private:
    void x_Process(void);
    CServer_Connection* m_Connection;
} ;


void CServerConnectionRequest::x_Process(void)
{
    try {
        m_Connection->OnSocketEvent(m_Event);
    } catch (...) {
        m_ConnPool.CloseConnection(m_Connection);
        throw;
    }
}


void CServerConnectionRequest::Process(void)
{
    if (s_ServerCatchExceptions->Get()) {
        try {
            x_Process();
        } NCBI_CATCH_ALL_X(6, "CServerConnectionRequest::Process");
    }
    else {
        x_Process();
    }
    if (m_Event != eServIO_Inactivity  &&  m_Event != eServIO_Delete) {
        // Return socket to poll vector
        m_ConnPool.SetConnType(m_Connection, eInactiveSocket);
    }
}


void CServerConnectionRequest::Cancel(void)
{
    // As of now, Cancel can not be called.
    // See comment at CServer::CreateRequest
    m_Connection->OnOverflow(eOR_RequestQueueFull);
    // Return socket to poll vector
    m_ConnPool.SetConnType(m_Connection, eInactiveSocket);
}


/////////////////////////////////////////////////////////////////////////////
// CServer_Listener
CStdRequest*
CServer_Listener::CreateRequest(EServIO_Event event,
                                CServer_ConnectionPool& conn_pool,
                                const STimeout* timeout)
{
    return new CAcceptRequest(event, conn_pool, timeout, this);
}


/////////////////////////////////////////////////////////////////////////////
// CServer_Connection
CStdRequest*
CServer_Connection::CreateRequest(EServIO_Event           event,
                                  CServer_ConnectionPool& conn_pool,
                                  const STimeout*         timeout)
{
    return new CServerConnectionRequest(event, conn_pool, timeout, this);
}

bool CServer_Connection::IsOpen(void)
{
    return m_Open;
}

void CServer_Connection::OnSocketEvent(EServIO_Event event)
{
    if (event == (EServIO_Event) -1) {
        m_Handler->OnTimer();
        return;
    }
    switch (event) {
    case eServIO_Open:
        m_Handler->OnOpen();
        break;
    case eServIO_OurClose:
        m_Handler->OnClose(IServer_ConnectionHandler::eOurClose);
        m_Open = false;
        break;
    case eServIO_ClientClose:
        m_Handler->OnClose(IServer_ConnectionHandler::eClientClose);
        m_Open = false;
        break;
    case eServIO_Inactivity:
        OnTimeout();
        m_Handler->OnClose(IServer_ConnectionHandler::eOurClose);
        //m_Open = false;
        // fall through
    case eServIO_Delete:
        delete this;
        break;
    default:
        if (eServIO_Read & event)
            m_Handler->OnRead();
        if (eServIO_Write & event)
            m_Handler->OnWrite();
        break;
    }
}

CServer_Connection::~CServer_Connection()
{
    static const STimeout zero_timeout = {0, 0};

    // Set zero timeout to prevent the socket from sitting in
    // TIME_WAIT state on the server.
    SetTimeout(eIO_Close, &zero_timeout);
}

/////////////////////////////////////////////////////////////////////////////
// CServer implementation

CServer::CServer(void) :
    m_Parameters(new SServer_Parameters()),
    m_ConnectionPool(NULL),
    m_ThreadPool(NULL)
{
    try {
        m_ConnectionPool = new CServer_ConnectionPool(
            m_Parameters->max_connections);
    } catch (...) {
        delete m_Parameters;
        throw;
    }
}


CServer::~CServer()
{
    delete m_ThreadPool;
    m_ThreadPool = NULL;
    delete m_ConnectionPool;
    m_ConnectionPool = NULL;
    delete m_Parameters;
    m_Parameters = NULL;
}


void CServer::AddListener(IServer_ConnectionFactory* factory,
                          unsigned short port)
{
    m_ConnectionPool->Add(new CServer_Listener(factory, port), eListener);
}


void CServer::SetParameters(const SServer_Parameters& new_params)
{
    if (new_params.init_threads <= 0  ||
        new_params.max_threads  < new_params.init_threads  ||
        new_params.max_threads > 1000) {
        NCBI_THROW(CServer_Exception, eBadParameters,
                   "CServer::SetParameters: Bad parameters");
    }
    *m_Parameters = new_params;
    m_ConnectionPool->SetMaxConnections(m_Parameters->max_connections);
}


void CServer::GetParameters(SServer_Parameters* params)
{
    *params = *m_Parameters;
}


void CServer::StartListening(void)
{
    m_ConnectionPool->StartListening();
}


void CServer::CloseConnection(CSocket* sock)
{
    m_ConnectionPool->CloseConnection(static_cast<IServer_ConnectionBase*>(
                                      static_cast<CServer_Connection*>(sock)));
}


static inline bool operator <(const STimeout& to1, const STimeout& to2)
{
    return to1.sec != to2.sec ? to1.sec < to2.sec : to1.usec < to2.usec;
}


void CServer::x_DoRun(void)
{
    m_ThreadPool->Spawn(m_Parameters->max_threads);

    Init();

    vector<CSocketAPI::SPoll> polls;
    size_t     count;
    typedef vector<IServer_ConnectionBase*> TConnsList;
    typedef vector<CRef<CStdRequest> > TReqsList;
    TConnsList timer_requests;
    TConnsList revived_conns;
    TConnsList to_close_conns;
    TConnsList to_delete_conns;
    TReqsList to_add_reqs;
    STimeout timer_timeout;
    const STimeout* timeout;

    while (!ShutdownRequested()) {
        bool has_timer = m_ConnectionPool->GetPollAndTimerVec(
                                           polls, timer_requests, &timer_timeout,
                                           revived_conns, to_close_conns,
                                           to_delete_conns);

        ITERATE(TConnsList, it, revived_conns) {
            IServer_ConnectionBase* conn_base = *it;
            EServIO_Event evt = IOEventToServIOEvent(conn_base->GetEventsToPollFor(NULL));
            CRef<CStdRequest> req(conn_base->CreateRequest(
                                                evt, *m_ConnectionPool,
                                                m_Parameters->idle_timeout));
            to_add_reqs.push_back(req);
        }
        ITERATE(TConnsList, it, to_close_conns) {
            IServer_ConnectionBase* conn_base = *it;
            CRef<CStdRequest> req(conn_base->CreateRequest(
                                                eServIO_Inactivity, *m_ConnectionPool,
                                                m_Parameters->idle_timeout));
            to_add_reqs.push_back(req);
        }
        ITERATE(TConnsList, it, to_delete_conns) {
            IServer_ConnectionBase* conn_base = *it;
            CRef<CStdRequest> req(conn_base->CreateRequest(
                                                eServIO_Delete, *m_ConnectionPool,
                                                m_Parameters->idle_timeout));
            to_add_reqs.push_back(req);
        }
        x_AddRequests(to_add_reqs);
        to_add_reqs.clear();

        timeout = m_Parameters->accept_timeout;

        if (has_timer &&
            (timeout == kDefaultTimeout ||
             timeout == kInfiniteTimeout ||
             timer_timeout < *timeout)) {
            timeout = &timer_timeout;
        }

        EIO_Status status = CSocketAPI::Poll(polls, timeout, &count);

        if (status != eIO_Success  &&  status != eIO_Timeout) {
            int x_errno = errno;
            const char* temp = IO_StatusStr(status);
            string ststr(temp
                         ? temp
                         : NStr::UIntToString((unsigned int) status));
            string erstr;
            if (x_errno) {
                erstr = ", {" + NStr::IntToString(x_errno);
                if (temp  &&  *temp) {
                    erstr += ',';
                    erstr += temp;
                }
                erstr += '}';
            }
            ERR_POST_X(8, Critical << "Poll failed with status "
                       << ststr << erstr);
            continue;
        }

        if (count == 0) {
            if (timeout != &timer_timeout) {
                ProcessTimeout();
            }
            else {
                m_ConnectionPool->SetAllActive(timer_requests);
                ITERATE (vector<IServer_ConnectionBase*>, it, timer_requests)
                {
                    IServer_ConnectionBase* conn_base = *it;
                    CRef<CStdRequest> req(conn_base->CreateRequest(
                                          (EServIO_Event)-1, *m_ConnectionPool, timeout));
                    to_add_reqs.push_back(req);
                }
                x_AddRequests(to_add_reqs);
                to_add_reqs.clear();
            }
            continue;
        }

        m_ConnectionPool->SetAllActive(polls);
        ITERATE (vector<CSocketAPI::SPoll>, it, polls) {
            if (!it->m_REvent) continue;
            IServer_ConnectionBase* conn_base =
                dynamic_cast<IServer_ConnectionBase*>(it->m_Pollable);
            _ASSERT(conn_base);
            CRef<CStdRequest> req(conn_base->CreateRequest(
                                                IOEventToServIOEvent(it->m_REvent),
                                                *m_ConnectionPool,
                                                m_Parameters->idle_timeout));
            if (req)
                to_add_reqs.push_back(req);
        }
        x_AddRequests(to_add_reqs);
        to_add_reqs.clear();
    }
}

void CServer::Run(void)
{
    StartListening(); // detect unavailable ports ASAP

    m_ThreadPool = new CPoolOfThreads_ForServer(m_Parameters->max_threads,
                                                m_ThreadSuffix);
    if (s_ServerCatchExceptions->Get()) {
        try {
            x_DoRun();
        } catch (CException& ex) {
            ERR_POST(ex);
            // Avoid collateral damage from destroying the thread pool
            // while worker threads are active (or, worse, initializing).
            m_ThreadPool->KillAllThreads(true);
            m_ConnectionPool->Erase();
            throw;
        }
    }
    else {
        x_DoRun();
    }

    // We need to kill all processing threads first, so that there
    // is no request with already destroyed connection left.
    m_ThreadPool->KillAllThreads(true);
    Exit();
    // We stop listening only here to provide port lock until application
    // cleaned up after execution.
    m_ConnectionPool->StopListening();
    // Here we finally free to erase connection pool.
    m_ConnectionPool->Erase();
}


void CServer::SubmitRequest(const CRef<CStdRequest>& request)
{
    if (m_ThreadPool != NULL)
        m_ThreadPool->AcceptRequest(request);
}


void CServer::DeferConnectionProcessing(IServer_ConnectionBase* conn)
{
    m_ConnectionPool->SetConnType(conn, ePreDeferredSocket);
}


void CServer::DeferConnectionProcessing(CSocket* sock)
{
    DeferConnectionProcessing(dynamic_cast<IServer_ConnectionBase*>(sock));
}


void CServer::Init()
{
}


void CServer::Exit()
{
}


void CServer::x_AddRequests(const vector<CRef<CStdRequest> >& reqs)
{
    ITERATE(vector<CRef<CStdRequest> >, it, reqs) {
        m_ThreadPool->AcceptRequest(*it);
    }
}

void CServer::AddConnectionToPool(CServer_Connection* conn)
{
    if (!m_ConnectionPool->Add(conn, eInactiveSocket)) {
        NCBI_THROW(CServer_Exception, ePoolOverflow,
                   "Cannot add connection, pool has overflowed.");
    }
}

void CServer::RemoveConnectionFromPool(CServer_Connection* conn)
{
    m_ConnectionPool->Remove(conn);
}

void CServer::WakeUpPollCycle(void)
{
    m_ConnectionPool->PingControlConnection();
}

/////////////////////////////////////////////////////////////////////////////
// SServer_Parameters implementation

static const STimeout k_DefaultIdleTimeout = { 600, 0 };

SServer_Parameters::SServer_Parameters() :
    max_connections(10000),
    temporarily_stop_listening(false),
    accept_timeout(kInfiniteTimeout),
    idle_timeout(&k_DefaultIdleTimeout),
    init_threads(5),
    max_threads(10),
    spawn_threshold(1)
{
}

const char* CServer_Exception::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eBadParameters: return "eBadParameters";
    case eCouldntListen: return "eCouldntListen";
    case ePoolOverflow:  return "ePoolOverflow";
    default:             return CException::GetErrCodeString();
    }
}

END_NCBI_SCOPE
