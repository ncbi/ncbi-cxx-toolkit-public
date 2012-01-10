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
#include <connect/server.hpp>
#include "connection_pool.hpp"
#include <connect/ncbi_buffer.h>
#include <connect/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Connect_ThrServer


BEGIN_NCBI_SCOPE


NCBI_PARAM_DECL(bool, server, Catch_Unhandled_Exceptions);
NCBI_PARAM_DEF_EX(bool, server, Catch_Unhandled_Exceptions, true, 0,
                  CSERVER_CATCH_UNHANDLED_EXCEPTIONS);
typedef NCBI_PARAM_TYPE(server, Catch_Unhandled_Exceptions) TParamServerCatchExceptions;



/////////////////////////////////////////////////////////////////////////////
// IServer_MessageHandler implementation
void IServer_MessageHandler::OnRead(void)
{
    CSocket &socket = GetSocket();
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
    for ( ;n_read > 0; ) {
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
    if (m_ConnPool.Add(m_Connection,
                       CServer_ConnectionPool::eActiveSocket))
    {
        m_Connection->OnSocketEvent(eServIO_Open);
        m_ConnPool.SetConnType(m_Connection,
                               CServer_ConnectionPool::eInactiveSocket);
    }
    else {
        // The connection pool is full
        // This place is the only one which can call OnOverflow now
        m_Connection->OnOverflow(eOR_ConnectionPoolFull);
        // Abort connection here to prevent it sitting in TIME_WAIT state
        // on the server.
        m_Connection->Abort();
        delete m_Connection;
    }
}

void CAcceptRequest::Process(void)
{
    if (!m_Connection) return;
    if (TParamServerCatchExceptions::GetDefault()) {
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
    CServer_Connection* m_Connection;
} ;


void CServerConnectionRequest::Process(void)
{
    if (TParamServerCatchExceptions::GetDefault()) {
        try {
            m_Connection->OnSocketEvent(m_Event);
        } NCBI_CATCH_ALL_X(6, "CServerConnectionRequest::Process");
    }
    else {
        m_Connection->OnSocketEvent(m_Event);
    }
    // Return socket to poll vector
    m_ConnPool.SetConnType(m_Connection, CServer_ConnectionPool::eInactiveSocket, false);
}


void CServerConnectionRequest::Cancel(void)
{
    // As of now, Cancel can not be called.
    // See comment at CServer::CreateRequest
    m_Connection->OnOverflow(eOR_RequestQueueFull);
    // Return socket to poll vector
    m_ConnPool.SetConnType(m_Connection,
                           CServer_ConnectionPool::eInactiveSocket);
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
    // Pull out socket from poll vector
    // See CServerConnectionRequest::Process and
    // CServer_ConnectionPool::GetPollAndTimerVec
    conn_pool.SetConnType(this, CServer_ConnectionPool::eActiveSocket,
                          (event != eServIO_Delete  &&  event != eServIO_Inactivity));
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
        Abort();
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


/////////////////////////////////////////////////////////////////////////////
// CServer implementation

CServer::CServer(void)
{
    // TODO: auto_ptr-based initialization
    m_Parameters = new SServer_Parameters();
    m_ConnectionPool = new CServer_ConnectionPool(
        m_Parameters->max_connections);
}


CServer::~CServer()
{
    delete m_Parameters;
    delete m_ConnectionPool;
}


void CServer::AddListener(IServer_ConnectionFactory* factory,
                          unsigned short port)
{
    m_ConnectionPool->Add(new CServer_Listener(factory, port),
                          CServer_ConnectionPool::eListener);
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
    m_ThreadPool->Spawn(m_Parameters->init_threads);

    Init();

    vector<CSocketAPI::SPoll> polls;
    size_t     count;
    typedef vector<IServer_ConnectionBase*> TConnsList;
    TConnsList timer_requests;
    TConnsList revived_conns;
    TConnsList to_close_conns;
    TConnsList to_delete_conns;
    STimeout   timer_timeout;
    const STimeout* timeout;

    while (!ShutdownRequested()) {
        bool has_timer = m_ConnectionPool->GetPollAndTimerVec(
                                           polls, timer_requests, &timer_timeout,
                                           revived_conns, to_close_conns,
                                           to_delete_conns);

        ITERATE(TConnsList, it, revived_conns) {
            EIO_Event evt = (*it)->GetEventsToPollFor(NULL);
            CreateRequest(*it, IOEventToServIOEvent(evt), m_Parameters->idle_timeout);
        }
        ITERATE(TConnsList, it, to_close_conns) {
            CreateRequest(*it, eServIO_Inactivity, m_Parameters->idle_timeout);
        }
        ITERATE(TConnsList, it, to_delete_conns) {
            CreateRequest(*it, eServIO_Delete, m_Parameters->idle_timeout);
        }

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
                ITERATE (vector<IServer_ConnectionBase*>, it, timer_requests)
                {
                    CreateRequest(*it, (EServIO_Event) -1, timeout);
                }
            }
            continue;
        }

        ITERATE (vector<CSocketAPI::SPoll>, it, polls) {
            if (!it->m_REvent) continue;
            IServer_ConnectionBase* conn_base =
                dynamic_cast<IServer_ConnectionBase*>(it->m_Pollable);
            _ASSERT(conn_base);
            CreateRequest(conn_base,
                          IOEventToServIOEvent(it->m_REvent),
                          m_Parameters->idle_timeout);
        }
    }
}

void CServer::Run(void)
{
    StartListening(); // detect unavailable ports ASAP

    m_ThreadPool.reset(new CStdPoolOfThreads(m_Parameters->max_threads,
                                             kMax_UInt,
                                             m_Parameters->spawn_threshold));
    if (TParamServerCatchExceptions::GetDefault()) {
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
    m_ThreadPool->AcceptRequest(request);
}


void CServer::DeferConnectionProcessing(IServer_ConnectionBase* conn)
{
    m_ConnectionPool->SetConnType(conn, CServer_ConnectionPool::ePreDeferredSocket);
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


void CServer::CreateRequest(IServer_ConnectionBase* conn_base,
                            EServIO_Event event,
                            const STimeout* timeout)
{
    CRef<CStdRequest> request(conn_base->CreateRequest(
                                         event, *m_ConnectionPool, timeout));

    if (request) {
        try {
            m_ThreadPool->AcceptRequest(request);
        } catch (CBlockingQueueException&) {
            // The size of thread pool queue is set to kMax_UInt, so
            // this is impossible event, but we handle it gently
            ERR_POST_X(1, Critical << "Thread pool queue full");
            CServer_Request* req = dynamic_cast<CServer_Request*>(
                                                        request.GetPointer());
            _ASSERT(req);
            // Queue is full, drop request, indirectly dropping incoming
            // connection (see also CAcceptRequest::Cancel)
            // ??? What should we do if conn_base is CServerConnection?
            // Should we close it? (see also CServerConnectionRequest::Cancel)
            req->Cancel();
        }
    }
    else {
        _ASSERT(event == eServIO_Read);
    }
}

void CServer::AddConnectionToPool(CServer_Connection* conn)
{
    if (!m_ConnectionPool->Add(conn, CServer_ConnectionPool::eInactiveSocket)) {
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
