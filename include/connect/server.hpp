#ifndef CONNECT___SERVER__HPP
#define CONNECT___SERVER__HPP

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
 * Authors:  Aaron Ucko, Victor Joukov, Denis Vakatov
 *
 */

/// @file server.hpp
/// Framework to create multithreaded network servers with thread-per-request
/// scheduling.

#include <util/thread_pool.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_conn_exception.hpp>
#include <connect/ncbi_socket.hpp>


/** @addtogroup ThreadedServer
 *
 * @{
 */


BEGIN_NCBI_SCOPE


class  IServer_ConnectionFactory;
class  IServer_ConnectionBase;
struct SServer_Parameters;
class  CServer_ConnectionPool;
class  CServer_Connection;
class  CStdPoolOfThreads;


/// Extended copy of the type EIO_Event allowing to distinguish between
/// connection closing from client and from ourselves
enum EServIO_Event {
    eServIO_Open        = 0x00,
    eServIO_Read        = 0x01,
    eServIO_Write       = 0x02,
    eServIO_ReadWrite   = 0x03, /**< eIO_Read | eIO_Write */
    eServIO_ClientClose = 0x04,
    eServIO_OurClose    = 0x08,
    eServIO_Inactivity  = 0x10,
    eServIO_Delete      = 0x20
};


/// Transform EIO_Event type to EServIO_Event
inline EServIO_Event
IOEventToServIOEvent(EIO_Event event)
{
    return (EServIO_Event) event;
}


class CNetCacheServer;

/////////////////////////////////////////////////////////////////////////////
///
///  CServer::
///
/// Thread-pool based server. On every event it allocates one of threads
/// from pool to serve the event, thereby making possible to serve large
/// number of concurrent connections efficiently. You need to subclass it
/// only if you want to provide gentle shutdown ability (override
/// ShutdownRequested) or process data in main thread on timeout (override
/// ProcessTimeout and set parameter accept_timeout to non-zero value).
///

class NCBI_XCONNECT_EXPORT CServer : protected CConnIniter
{
public:
    // 'ctors
    CServer(void);
    virtual ~CServer();

    /// Register a listener
    void AddListener(IServer_ConnectionFactory* factory,
                     unsigned short             port);

    /// 
    void SetParameters(const SServer_Parameters& new_params);

    ///
    void GetParameters(SServer_Parameters* params);

    /// Start listening before the main loop. If called, tries
    /// to listen on all requested ports for all listeners, correcting
    /// errors by calling listeners' OnFailure
    void StartListening(void);

    /// Enter the main loop
    void Run(void);

    /// Submit request to be executed by the server thread pool
    void SubmitRequest(const CRef<CStdRequest>& request);

    /// Mark connection as deferred for processing, i.e. do not poll on it
    /// and wait when IsReadyToProcess() will return true.
    void DeferConnectionProcessing(IServer_ConnectionBase* conn);
    void DeferConnectionProcessing(CSocket* sock);

    /// Close connection. Method should be called only when closing is
    /// initiated by server itself, because it will generate then event
    /// eServIO_OurClose.
    ///
    /// @sa EServIO_Event
    void CloseConnection(CSocket* sock);

    /// Add externally created connection to the connection pool which server
    /// polls on. Throws exception if pool is full.
    /// NOTE: events to this connection can come theoretically even
    /// NOTE: if connection gets some error or its peer closes it then conn
    /// object will be deleted after processing OnClose event. If you don't
    /// want that you have to call RemoveConnectionFromPool while handling
    /// OnClose.
    void AddConnectionToPool(CServer_Connection* conn);
    /// Remove externally created connection from pool.
    void RemoveConnectionFromPool(CServer_Connection* conn);
    /// Force poll cycle to make another iteration.
    /// Should be called if IsReadyToProcess() for some connection handler
    /// became true.
    void WakeUpPollCycle(void);

protected:
    /// Initialize the server
    ///
    /// Called by Run method before poll cycle.
    virtual void Init();

    /// Cleanup the server
    ///
    /// Called by Run method after poll cycle when all processing threads
    /// are stopped, but before releasing listening ports. Here you're still
    /// guaranteed that another instance running on the same set of ports will
    /// fail at StartListening point.
    virtual void Exit();

    /// Runs synchronously when no socket activity has occurred in a
    /// while (as determined by m_AcceptTimeout).
    /// @sa m_Parameters->accept_timeout
    virtual void ProcessTimeout(void) { }

    /// Runs synchronously between iterations.
    /// @return
    ///  whether to shut down service and return from Run.
    virtual bool ShutdownRequested(void) { return false; }

private:
    void x_AddRequests(const vector<CRef<CStdRequest> >& reqs);
    void x_DoRun(void);

    friend class CNetCacheServer;
    CStdPoolOfThreads* GetThreadPool(void) { return m_ThreadPool.get(); }

    auto_ptr<CStdPoolOfThreads> m_ThreadPool;
    SServer_Parameters*         m_Parameters;
    CServer_ConnectionPool*     m_ConnectionPool;
};


/////////////////////////////////////////////////////////////////////////////
///
///  Error codes for OnOverflow method in IServer_ConnectionHandler
enum EOverflowReason
{
    eOR_Unknown = 0,
    eOR_ConnectionPoolFull,
    eOR_RequestQueueFull,
    eOR_UnpollableSocket
};


/////////////////////////////////////////////////////////////////////////////
///
///  IServer_ConnectionHandler::
///
/// Implement this interface to provide server functionality.
///

class NCBI_XCONNECT_EXPORT IServer_ConnectionHandler
{
public:
    virtual ~IServer_ConnectionHandler() { }

    /// Following three methods are guaranteed to be called NOT
    /// at the same time as On*, so if you implement them
    /// you should not guard the variables which they can use with
    /// mutexes.
    /// @param alarm_time
    ///   Set this parameter to a pointer to a CTime object to recieve
    ///   an OnTimer event at the moment in time specified by this object.
    /// @return
    ///   Returns the set of events for which Poll should check.
    virtual EIO_Event GetEventsToPollFor(const CTime** /*alarm_time*/) const
        { return eIO_Read; }
    /// Returns the timeout for this connection
    virtual const STimeout* GetTimeout(void)
        { return kDefaultTimeout; }
    /// Returns connection handler's perception of whether we open or not.
    /// It is unsafe to just close underlying socket because of the race,
    /// emerging due to the fact that the socket can linger for a while.
    virtual bool IsOpen(void)
        { return true; }
    /// Returns the handler's readiness to process input data or to write
    /// some output data. OnRead() and OnWrite() are not called unless this
    /// method return true.
    virtual bool IsReadyToProcess(void) const
        { return true; }


    /// Runs in response to an external event [asynchronous].
    /// You can get socket by calling GetSocket(), if you close the socket
    /// this object will be destroyed.
    /// Individual events are:
    /// A client has just established this connection.
    virtual void OnOpen(void) = 0;
    /// The client has just sent data.
    virtual void OnRead(void) = 0;
    /// The client is ready to receive data.
    virtual void OnWrite(void) = 0;

    /// Type of connection closing
    enum EClosePeer {
        eOurClose,     ///< Connection closed by ourselves
        eClientClose   ///< Connection closed by other peer
    };

    /// The connection has closed (with information on type of closing)
    virtual void OnClose(EClosePeer /*peer*/) { }

    /// Runs when a client has been idle for too long, prior to
    /// closing the connection [synchronous].
    virtual void OnTimeout(void) { }

    /// This method is called at the moment in time specified earlier by the
    /// alarm_time parameter of the GetEventsToPollFor method [synchronous].
    virtual void OnTimer(void) { }

    /// Runs when there are insufficient resources to queue a
    /// connection, prior to closing it. Provides a reason why the
    /// connection is being close, which can be reported back to the client.
    // See comment for CAcceptRequest::Process and CServer::CreateRequest
    virtual void OnOverflow(EOverflowReason) { }

    /// Get underlying socket
    CSocket& GetSocket(void) { return *m_Socket; }

public: // TODO: make it protected. Public is for DEBUG purposes only
    friend class CServer_Connection;
    void SetSocket(CSocket *socket) { m_Socket = socket; }

private:
    CSocket* m_Socket;
};



/////////////////////////////////////////////////////////////////////////////
///
/// IServer_MessageHandler::
///
/// TODO:
class NCBI_XCONNECT_EXPORT IServer_MessageHandler :
    public IServer_ConnectionHandler
{
public:
    IServer_MessageHandler() :
        m_Buffer(0)
    { }
    virtual ~IServer_MessageHandler() { BUF_Destroy(m_Buffer); }
    virtual void OnRead(void);
    // You should implement this look-ahead function to decide, did you get
    // a message in the series of read events. If not, you should return -1.
    // If yes, return number of chars, beyond well formed message. E.g., if
    // your message spans all the buffer, return 0. If you returned non-zero
    // value, this piece of data will be used in the next CheckMessage to
    // simplify client state management.
    // You also need to copy bytes, comprising the message from data to buffer.
    virtual int CheckMessage(BUF* buffer, const void *data, size_t size) = 0;
    // Process incoming message in the buffer, by using
    // BUF_Read(buffer, your_data_buffer, BUF_Size(buffer)).
    virtual void OnMessage(BUF buffer) = 0;
private:
    BUF m_Buffer;
};


int NCBI_XCONNECT_EXPORT
Server_CheckLineMessage(BUF* buffer, const void *data, size_t size,
                        bool& seen_CR);

/////////////////////////////////////////////////////////////////////////////
///
/// IServer_LineMessageHandler::
///
/// TODO:
class NCBI_XCONNECT_EXPORT IServer_LineMessageHandler :
    public IServer_MessageHandler
{
public:
    IServer_LineMessageHandler() :
        IServer_MessageHandler(), m_SeenCR(false)
    { }
    virtual int CheckMessage(BUF* buffer, const void *data, size_t size) {
        return Server_CheckLineMessage(buffer, data, size, m_SeenCR);
    }
private:
    bool m_SeenCR;
};



/////////////////////////////////////////////////////////////////////////////
///
/// IServer_StreamHandler::
///
/// TODO:
class NCBI_XCONNECT_EXPORT IServer_StreamHandler :
    public IServer_ConnectionHandler
{
public:
    CNcbiIostream &GetStream();
private:
    CConn_SocketStream m_Stream;
};



/////////////////////////////////////////////////////////////////////////////
///
///  IServer_ConnectionFactory::
///
/// Factory to be registered with CServer instance. You usually do not
/// need to implement it, default template CServer_ConnectionFactory will
/// suffice. You NEED to implement it manually to pass server-wide parameters
/// to ConnectionHandler instances, e.g. for implementation of gentle shutdown.
///

class NCBI_XCONNECT_EXPORT IServer_ConnectionFactory
{
public:
    /// What to do if the port is busy
    enum EListenAction {
        eLAFail   = 0,   // Can not live without this port, default
        eLAIgnore = 1,   // Do nothing, throw away this listener
        eLARetry  = 2    // Listener should provide another port to try
    };
    virtual ~IServer_ConnectionFactory() { }

    /// @return
    ///  a new instance of handler for connection
    virtual IServer_ConnectionHandler* Create(void) = 0;
    /// Return desired action if the port, mentioned in AddListener is busy.
    /// If the action is eLARetry, provide new port. The
    virtual EListenAction OnFailure(unsigned short* /* port */)
        { return eLAFail; }
};



/////////////////////////////////////////////////////////////////////////////
///
///  CServer_ConnectionFactory::
///
/// Reasonable default implementation for IServer_ConnectionFactory
///

template <class TServer_ConnectionHandler>
class CServer_ConnectionFactory : public IServer_ConnectionFactory
{
public:
    virtual IServer_ConnectionHandler* Create()
        { return new TServer_ConnectionHandler(); }
};



/////////////////////////////////////////////////////////////////////////////
///
///  SServer_Parameters::
///
/// Settings for CServer
///

struct NCBI_XCONNECT_EXPORT SServer_Parameters
{
    /// Maximum # of open connections
    unsigned int    max_connections;
    /// Temporarily close listener when queue fills?
    bool            temporarily_stop_listening;
    /// Maximum t between exit checks
    const STimeout* accept_timeout;
    /// For how long to keep inactive non-listening sockets open
    /// (default:  10 minutes)
    const STimeout* idle_timeout;

    // (settings for the thread pool)
    unsigned int    init_threads;    ///< Number of initial threads
    unsigned int    max_threads;     ///< Maximum simultaneous threads
    unsigned int    spawn_threshold; ///< Controls when to spawn more threads

    /// Create structure with the default set of parameters
    SServer_Parameters();
};



/////////////////////////////////////////////////////////////////////////////
///
///  CServer_Exception::
///
/// Exceptions thrown by CServer::Run()
///

class NCBI_XCONNECT_EXPORT CServer_Exception
    : EXCEPTION_VIRTUAL_BASE public CConnException
{
public:
    enum EErrCode {
        eBadParameters, ///< Out-of-range parameters given
        eCouldntListen, ///< Unable to bind listening port
        ePoolOverflow   ///< Connection pool overflowed
    };
    virtual const char* GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CServer_Exception, CConnException);
};



END_NCBI_SCOPE


/* @} */

#endif  /* CONNECT___SERVER__HPP */
