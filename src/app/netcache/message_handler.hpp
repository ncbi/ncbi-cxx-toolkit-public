#ifndef NETCACHE_MESSAGE_HANDLER__HPP
#define NETCACHE_MESSAGE_HANDLER__HPP
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
 * Authors:  Pavel Ivanov
 *
 * File Description: Network cache daemon
 *
 */

#include <corelib/request_ctx.hpp>
#include <corelib/obj_pool.hpp>
#include <util/simple_buffer.hpp>
#include <connect/server.hpp>
#include <connect/services/netservice_protocol_parser.hpp>

#include "netcached.hpp"
#include "nc_storage.hpp"

BEGIN_NCBI_SCOPE

enum {
    /// Initial size of the buffer used for reading/writing from socket.
    /// Reading buffer never changes its size from the initial, writing buffer
    /// can grow if there's need to.
    kInitNetworkBufferSize = 64 * 1024
};


/// Object reading/writing from socket and automatically buffering data
class CBufferedSockReaderWriter
{
public:
    CBufferedSockReaderWriter(void);

    /// Initialize object with socket and default timeout that should be
    /// always set as socket's read/write timeout when outside methods of
    /// the object.
    void ResetSocket(CSocket* socket, STimeout* def_timeout);

    /// Check if there's something in the buffer to read from
    bool HasDataToRead(void) const;
    /// Check if there's data in writing buffer which will be written to
    /// socket on next call to Flush().
    ///
    /// @sa Flush
    bool IsWriteDataPending(void) const;

    /// Read data from socket to the buffer.
    ///
    /// @return
    ///   TRUE if some data is available in buffer, FALSE otherwise
    bool Read(void);
    /// Read one line from socket (or buffer). After reading this line will
    /// be erased from the reading buffer.
    ///
    /// @return
    ///  TRUE if line was successfully read, FALSE if end-of-line is not met
    ///  in data yet or there's no data in socket at all.
    bool ReadLine(string& line);

    /// Get pointer to buffered data read from socket
    const void* GetReadData(void) const;
    /// Get size of buffered data available
    size_t      GetReadSize(void) const;
    /// Erase first amount bytes from reading buffer - they're processed and
    /// no longer necessary.
    void        EraseReadData(size_t amount);

    /// Write message to the writing buffer. Message is created by
    /// concatenating prefix and msg texts. Nothing is written directly to
    /// socket until Flush() is called.
    ///
    /// @sa Flush
    void   WriteMessage(CTempString prefix, CTempString msg);
    /// Prepare writing buffer for external writing and return pointer to it
    void*  PrepareDirectWrite(void);
    /// Get number of bytes that can be written into writing buffer via
    /// external writing.
    size_t GetDirectWriteSize(void) const;
    /// Grow writing buffer by amount bytes because they were written directly
    /// to the buffer.
    void   AddDirectWritten(size_t amount);
    /// Write all data residing in writing buffer to the socket.
    /// If socket cannot accept all data immediately then leave what has left
    /// in the buffer as is until next call and return without waiting.
    void Flush(void);

public:
    // For internal use only

    /// Set socket's read/write timeout to zero.
    void ZeroSocketTimeout(void);
    /// Set socket's read/write timeout to default value for outside world.
    void ResetSocketTimeout(void);

private:
    typedef CSimpleBufferT<char> TBufferType;


    /// Read end-of-line from buffer, advance buffer pointer to pass read
    /// characters. Method supports all possible variants of CRLF including
    /// CR, LF, CRLF, LFCR and 0 byte.
    ///
    /// @param force_read
    ///   FALSE if we need just to finish reading of end-of-line characters
    ///   if started and do nothing if not started. TRUE if reading of
    ///   end-of-line is mandatory even if not started yet.
    void x_ReadCRLF(bool force_read);
    /// Remove already unnecessary data from the buffer.
    ///
    /// @param buffer
    ///   Buffer to compact (will be read or write buffer)
    /// @param pos
    ///   Variable with buffer position that will be reseted
    void x_CompactBuffer(TBufferType& buffer, size_t& pos);


    /// Special guard for setting and resetting value of read/write timeout
    /// on the socket.
    class CReadWriteTimeoutGuard
    {
    public:
        CReadWriteTimeoutGuard(CBufferedSockReaderWriter* parent);
        ~CReadWriteTimeoutGuard(void);

    private:
        CBufferedSockReaderWriter* m_Parent;
    };

    /// Type of end-of-line bytes that can be read from buffer
    enum ECRLF_Byte {
        eNone = 0,
        eCR   = 1,
        eLF   = 2,
        eCRLF = eCR + eLF
    };
    /// Bit mask containing ECRLF_Bytes
    typedef int TCRLF_Bytes;

    /// Socket connected to the object
    CSocket*    m_Socket;
    /// Timeout equal to 0 - just to not create it every time
    STimeout    m_ZeroTimeout;
    /// Default timeout for socket
    STimeout    m_DefTimeout;
    /// Reading buffer
    TBufferType m_ReadBuff;
    /// Current position inside reading buffer
    size_t      m_ReadDataPos;
    /// Writing buffer
    TBufferType m_WriteBuff;
    /// Current position inside writing buffer
    size_t      m_WriteDataPos;
    /// Types of end-of-line bytes already read from buffer
    TCRLF_Bytes m_CRLFMet;
};


/// Type of access to storage each NetCache command can have
enum ENCStorageAccess {
    eNoStorage       = 0,  ///< No access to storage is necessary
    eNoBlobLock      = 1,  ///< Storage existence is a must but no blob lock is
                           ///< necessary
    eWithBlobLock    = 2,  ///< Command must obtain lock on blob before
                           ///< execution, but doesn't need to generate blob
                           ///< key if it's empty
    eWithAutoBlobKey = 3   ///< Command needs lock on blob and needs to
                           ///< generate blob key if none provided
};

/// Handler of all NetCache incoming requests.
/// Handler written to be reusable object so that if one connection to
/// NetCache is closed then handler from it can go to serve for another newly
/// opened connection. All handling is implemented as finite state machine
/// with central place of state dispatching - x_ManageCmdPipeline - and with
/// transitions between states implemented inside state processors. So that
/// each state processor knows where it can go then, central part only knows
/// which method process which state.
class CNCMessageHandler
{
public:

#ifdef _DEBUG
    /// Get name of handler state. Method needed for logging and statistics
    ///
    /// @sa EStates
    static string GetStateName(int state);
#endif

    /// Create handler for the given NetCache server object
    CNCMessageHandler(CNetCacheServer* server);
    virtual ~CNCMessageHandler(void);

    /// Set socket which handler will work with
    void SetSocket(CSocket* socket);

    /// IServer_ConnectionHandler interface
    /// Will be redirected here via CNCMsgHandler_Proxy.
    EIO_Event GetEventsToPollFor(const CTime** alarm_time) const;
    bool IsReadyToProcess(void) const;
    void OnOpen(void);
    void OnRead(void);
    void OnWrite(void);
    void OnClose(IServer_ConnectionHandler::EClosePeer peer);
    void OnTimeout(void);
    void OnTimer(void);
    void OnOverflow(void);

    /// Init diagnostics Client IP and Session ID for proper logging
    void InitDiagnostics(void);
    /// Reset diagnostics Client IP and Session ID to avoid logging
    /// not related to the request
    void ResetDiagnostics(void);

    /// Type of method processing command and returning flag if shift to next
    /// state was made. If shift wasn't made then processing is postponed and
    /// dispatcher should return to caller and do nothing till next call.
    typedef bool (CNCMessageHandler::*TProcessor)(void);
    /// Extra information about each NetCache command
    struct SCommandExtra {
        /// Which method will process the command
        TProcessor       processor;
        /// Command name to present to user in statistics
        const char*      cmd_name;
        /// Type of access to storage command should have
        ENCStorageAccess storage_access;
        /// What access to the blob command should receive
        ENCBlobAccess    blob_access;
    };
    /// Type definitions for NetCache protocol parser
    typedef SNSProtoCmdDef<SCommandExtra>      SCommandDef;
    typedef SNSProtoParsedCmd<SCommandExtra>   SParsedCmd;
    typedef CNetServProtoParser<SCommandExtra> TProtoParser;

    /// Command processors
    bool x_DoCmd_Alive(void);
    bool x_DoCmd_OK(void);
    bool x_DoCmd_Health(void);
    bool x_DoCmd_Monitor(void);
    bool x_DoCmd_Shutdown(void);
    bool x_DoCmd_Version(void);
    bool x_DoCmd_GetConfig(void);
    bool x_DoCmd_GetServerStats(void);
    bool x_DoCmd_Reinit(void);
    bool x_DoCmd_ReinitImpl(void);
    bool x_DoCmd_Reconf(void);
    bool x_DoCmd_ReconfImpl(void);
    bool x_DoCmd_Put2(void);
    bool x_DoCmd_Put3(void);
    bool x_DoCmd_Get(void);
    bool x_DoCmd_GetSize(void);
    bool x_DoCmd_GetOwner(void);
    bool x_DoCmd_HasBlob(void);
    bool x_DoCmd_IsLock(void);
    bool x_DoCmd_Remove(void);
    bool x_DoCmd_Remove2(void);
    bool x_DoCmd_IC_GetBlobsTTL(void);
    bool x_DoCmd_IC_IsOpen(void);
    bool x_DoCmd_IC_Store(void);
    bool x_DoCmd_IC_StoreBlob(void);
    bool x_DoCmd_IC_GetSize(void);
    bool x_DoCmd_IC_GetAccessTime(void);
    /// Universal processor for all commands not implemented now (because they
    /// are not used by anybody and the very fact of introducing them was
    /// foolish).
    bool x_DoCmd_NotImplemented(void);

private:
    /// States of handling finite state machine
    enum EStates {
        ePreAuthenticated,     ///< Connection is opened but not authenticated
        eReadyForCommand,      ///< Ready to receive next NetCache command
        eCommandReceived,      ///< Command received but not parsed yet
        eWaitForBlobLock,      ///< Locking of blob needed for command is in
                               ///< progress
        eWaitForStorageBlock,  ///< Locking of storage needed for command is
                               ///< in progress
        eWaitForServerBlock,   ///< Locking of all storages in the server is
                               ///< in progress
        eReadBlobSignature,    ///< Reading of signature in blob transfer
                               ///< protocol
        eReadBlobChunkLength,  ///< Reading chunk length in blob transfer
                               ///< protocol
        eReadBlobChunk,        ///< Reading chunk data in blob transfer
                               ///< protocol
        eWriteBlobData,        ///< Writing data from blob to socket
        eSocketClosed          ///< Connection closed or not opened yet
    };
    /// Additional flags that could be applied to object states
    enum EFlags {
        fCommandStarted    = 0x010000,  ///< Command startup code is executed
        fCommandPrinted    = 0x020000,  ///< "request-start" message was
                                        ///< printed to diagnostics
        fSwapLengthBytes   = 0x040000,  ///< Byte order should be swapped when
                                        ///< reading length of chunks in blob
                                        ///< transfer protocol
        fConfirmBlobPut    = 0x080000,  ///< After blob is written to storage
                                        ///< "OK" message should be written to
                                        ///< socket
        fReadExactBlobSize = 0x100000,  ///< There is exact size of the blob
                                        ///< transferred to NetCache
        eAllFlagsMask      = 0x1F0000   ///< Sum of all flags above
    };
    /// Bit mask of EFlags plus one of EStates
    typedef int TStateFlags;

    /// Statuses of commands to be set in diagnostics' request context
    enum EHTTPStatus {
        eStatus_OK          = 200,  ///< Command is ok and execution is good
        eStatus_BadCmd      = 400,  ///< Command is incorrect
        eStatus_NotFound    = 404,  ///< Blob was not found
        eStatus_NotAllowed  = 405,  ///< Operation not allowed with current
                                    ///< settings
        eStatus_CmdTimeout  = 408,  ///< Command timeout is exceeded
        eStatus_CondFailed  = 412,  ///< Precondition stated in command has
                                    ///< failed
        eStatus_ServerError = 500,  ///< Internal server error
        eStatus_NoImpl      = 501   ///< Command is not implemented
    };


    /// Set machine state
    void    x_SetState(EStates state);
    /// Get machine state
    EStates x_GetState(void) const;

    /// Set additional machine state flag
    void x_SetFlag  (EFlags  flag);
    /// Remove additional machine state flag
    void x_UnsetFlag(EFlags  flag);
    /// Check if additional machine state flag is set
    bool x_IsFlagSet(EFlags  flag) const;

    // State machine implementation

    /// Main dispatcher of state machine
    void x_ManageCmdPipeline(void);
    /// Read authentication message from client
    bool x_ReadAuthMessage(void);
    /// Check whether current client name is administrative client, throw
    /// exception if not.
    void x_CheckAdminClient(void);
    /// Read command and start it if it's available
    bool x_ReadCommand(void);
    /// Process current command
    bool x_DoCommand(void);
    /// Process "waiting" for blob locking. In fact just shift to next state
    /// if lock is acquired and just return if not.
    bool x_WaitForBlobLock(void);
    /// Process "waiting" for storage blocking
    bool x_WaitForStorageBlock(void);
    /// Process "waiting" for blocking of all storages in the server
    bool x_WaitForServerBlock(void);
    /// Read signature in blob transfer protocol
    bool x_ReadBlobSignature(void);
    /// Read length of chunk in blob transfer protocol
    bool x_ReadBlobChunkLength(void);
    /// Read chunk data in blob transfer protocol
    bool x_ReadBlobChunk(void);
    /// Write data from blob to socket
    bool x_WriteBlobData(void);

    /// Close connection (or at least make CServer believe that we closed it
    /// by ourselves)
    void x_CloseConnection(void);
    /// Assign command parameters returned by protocol parser to handler
    /// member variables
    void x_AssignCmdParams(map<string, string>& params);
    /// Print "request_start" message into diagnostics with all parameters of
    /// the current command.
    void x_PrintRequestStart(const SParsedCmd& cmd);
    /// Start command returned by protocol parser
    void x_StartCommand(SParsedCmd& cmd);
    /// Command execution is finished, do cleanup work
    void x_FinishCommand(void);
    /// Start reading blob from socket
    void x_StartReadingBlob(void);
    /// Client finished sending blob, do cleanup
    void x_FinishReadingBlob(void);
    /// Start writing blob from storage to socket
    void x_StartWritingBlob(void);
    /// Check if NetCache is monitored by someone
    bool x_IsMonitored(void);
    /// Send message to the monitor, if do_trace == true then print this
    /// message to diagnostics as Trace too.
    void x_MonitorPost(const string& msg, bool do_trace = true);
    /// Report caught exception to diagnostics and monitor
    void x_ReportException(const exception& ex, CTempString msg);
    /// Transform socket for the handler to socket stream which will then be
    /// used to write something. Connection to client will be closed when
    /// created socket stream is deleted. Though message about closing will be
    /// printed during execution of this method, CServer will think that
    /// connection closed also inside this method.
    AutoPtr<CConn_SocketStream> x_PrepareSockStream(void);


    /// Special guard to properly initialize and de-initialize diagnostics
    class CDiagnosticsGuard
    {
    public:
        CDiagnosticsGuard(CNCMessageHandler* handler);
        ~CDiagnosticsGuard(void);

    private:
        CNCMessageHandler* m_Handler;
    };


    /// NetCache server this handler created for
    CNetCacheServer*          m_Server;
    /// Monitor of the NetCache server
    CServer_Monitor*          m_Monitor;
    /// Socket handler attached to
    CSocket*                  m_Socket;
    /// Handler state and state flags
    TStateFlags               m_State;
    /// Socket reader/writer
    CBufferedSockReaderWriter m_SockBuffer;
    /// NetCache protocol parser
    TProtoParser              m_Parser;
    /// Name of currently executing command as it can presented in stats
    const char*               m_CurCmd;
    /// Processor for the currently executed NetCache command
    TProcessor                m_CmdProcessor;
    /// Diagnostics context for the currently executed command
    CRef<CRequestContext>     m_DiagContext;
    /// Holder of the lock for blob
    TNCBlobLockHolderRef      m_BlobLock;
    /// Blob object for current reading/writing (lock holder has ownership,
    /// here it's just pointer)
    CNCBlob*                  m_CurBlob;
    /// Length of the current blob chunk to read from socket
    Uint4                     m_ChunkLen;

    /// Name of the client received in authorization string
    string                    m_ClientName;
    /// Address of the client
    string                    m_ClientAddress;
    /// Time when connection was opened
    CTime                     m_ConnTime;
    /// Time when command started execution
    CTime                     m_CmdStartTime;
    /// Maximum time when command should finish execution or it will be
    /// timed out
    CTime                     m_MaxCmdTime;

    /// Storage to execute current command on
    CNCBlobStorage*           m_Storage;
    /// Blob key in current command
    string                    m_BlobKey;
    /// Blob subkey in current command
    string                    m_BlobSubkey;
    /// Blob version in current command
    unsigned int              m_BlobVersion;
    /// "Password" to access the blob
    string                    m_BlobPass;
    /// Version of the blob key to generate
    unsigned int              m_KeyVersion;
    /// Time-to-live value for the blob
    unsigned int              m_BlobTTL;
    /// Exact size of the blob sent by client
    size_t                    m_StoreBlobSize;
};


/// Factory creating CNCMessageHandler objects.
/// Actually implements standard pool of objects.
class CNCMsgHandler_Factory : public CObject
{
public:
    CNCMsgHandler_Factory(CNetCacheServer* server);
    virtual ~CNCMsgHandler_Factory(void);

    /// Create new NetCache handler.
    /// IServer_ConnectionFactory implementation.
    IServer_ConnectionHandler* Create(void);

public:
    // For internal use only
    typedef CObjFactory_NewParam<CNCMessageHandler,
                                 CNetCacheServer*>   THandlerPoolFactory;
    typedef CObjPool<CNCMessageHandler,
                     THandlerPoolFactory>            THandlerPool;

    /// Get pool for creating CNCMessageHandler objects
    THandlerPool& GetHandlerPool(void);

private:
    /// Pool of CNCMessageHandler objects
    THandlerPool     m_Pool;
};

typedef CRef<CNCMsgHandler_Factory>  TNCMsgHandlerFactoryRef;


/// Special proxy for handler factory.
/// It's necessary because server have ownership over factory and it's
/// destroyed before all sockets are destroyed and thus before all handlers
/// can be destroyed.
class CNCMsgHndlFactory_Proxy : public IServer_ConnectionFactory
{
public:
    CNCMsgHndlFactory_Proxy(CNetCacheServer* server);
    virtual ~CNCMsgHndlFactory_Proxy(void);

    /// IServer_ConnectionFactory interface - create new handler for socket
    virtual IServer_ConnectionHandler* Create(void);

private:
    TNCMsgHandlerFactoryRef m_Factory;
};


/// Special proxy for NetCache handler to allow handlers pooling.
/// Class is needed because CServer wants ownership over handler object.
class CNCMsgHandler_Proxy : public IServer_ConnectionHandler
{
public:
    /// Create handler proxy for given factory
    CNCMsgHandler_Proxy(CNCMsgHandler_Factory* factory);
    virtual ~CNCMsgHandler_Proxy(void);

    /// Implementation and redirection of IServer_ConnectionHandler interface
    virtual EIO_Event GetEventsToPollFor(const CTime** alarm_time) const;
    virtual bool IsReadyToProcess(void) const;
    virtual void OnOpen(void);
    virtual void OnRead(void);
    virtual void OnWrite(void);
    virtual void OnClose(EClosePeer peer);
    virtual void OnTimeout(void);
    virtual void OnTimer(void);
    virtual void OnOverflow(EOverflowReason);

private:
    typedef CObjPoolGuard<CNCMsgHandler_Factory::THandlerPool>  THandlerGuard;

    // The order of elements is important here. Handler should be able to
    // return to factory while it still exists.
    TNCMsgHandlerFactoryRef m_Factory;
    THandlerGuard           m_Handler;
};



inline void
CBufferedSockReaderWriter::ZeroSocketTimeout(void)
{
    m_Socket->SetTimeout(eIO_ReadWrite, &m_ZeroTimeout);
}

inline void
CBufferedSockReaderWriter::ResetSocketTimeout(void)
{
    m_Socket->SetTimeout(eIO_ReadWrite, &m_DefTimeout);
}

inline void
CBufferedSockReaderWriter::ResetSocket(CSocket* socket, STimeout* def_timeout)
{
    m_ReadDataPos  = 0;
    m_WriteDataPos = 0;
    m_CRLFMet = eNone;
    m_ReadBuff.resize(0);
    m_WriteBuff.resize(0);
    m_Socket = socket;
    if (def_timeout) {
        m_DefTimeout = *def_timeout;
    }
    else {
        m_DefTimeout = m_ZeroTimeout;
    }
    if (m_Socket) {
        ResetSocketTimeout();
    }
}

inline
CBufferedSockReaderWriter::CBufferedSockReaderWriter(void)
    : m_ReadBuff(kInitNetworkBufferSize),
      m_WriteBuff(kInitNetworkBufferSize)
{
    m_ZeroTimeout.sec = m_ZeroTimeout.usec = 0;
    ResetSocket(NULL, NULL);
}

inline bool
CBufferedSockReaderWriter::HasDataToRead(void) const
{
    return m_ReadDataPos < m_ReadBuff.size();
}

inline bool
CBufferedSockReaderWriter::IsWriteDataPending(void) const
{
    return m_WriteDataPos < m_WriteBuff.size();
}

inline const void*
CBufferedSockReaderWriter::GetReadData(void) const
{
    return m_ReadBuff.data() + m_ReadDataPos;
}

inline size_t
CBufferedSockReaderWriter::GetReadSize(void) const
{
    return m_ReadBuff.size() - m_ReadDataPos;
}

inline void
CBufferedSockReaderWriter::EraseReadData(size_t amount)
{
    m_ReadDataPos = min(m_ReadDataPos + amount, m_ReadBuff.size());
}

inline void*
CBufferedSockReaderWriter::PrepareDirectWrite(void)
{
    return m_WriteBuff.data() + m_WriteBuff.size();
}

inline size_t
CBufferedSockReaderWriter::GetDirectWriteSize(void) const
{
    return m_WriteBuff.capacity() - m_WriteBuff.size();
}

inline void
CBufferedSockReaderWriter::AddDirectWritten(size_t amount)
{
    _ASSERT(amount <= m_WriteBuff.capacity() - m_WriteBuff.size());

    m_WriteBuff.resize(m_WriteBuff.size() + amount);
}



inline
CBufferedSockReaderWriter::CReadWriteTimeoutGuard
    ::CReadWriteTimeoutGuard(CBufferedSockReaderWriter* parent)
    : m_Parent(parent)
{
    m_Parent->ZeroSocketTimeout();
}

inline
CBufferedSockReaderWriter::CReadWriteTimeoutGuard
    ::~CReadWriteTimeoutGuard(void)
{
    m_Parent->ResetSocketTimeout();
}



inline bool
CNCMessageHandler::x_IsMonitored(void)
{
    return IsVisibleDiagPostLevel(eDiag_Trace)
           ||  m_Monitor  &&  m_Monitor->IsMonitorActive();
}

inline void
CNCMessageHandler::SetSocket(CSocket* socket)
{
    m_Socket = socket;
}

inline CNCMessageHandler::EStates
CNCMessageHandler::x_GetState(void) const
{
    return EStates(m_State & ~eAllFlagsMask);
}

inline void
CNCMessageHandler::x_SetState(EStates new_state)
{
    m_State = (m_State & eAllFlagsMask) + new_state;
}

inline void
CNCMessageHandler::x_SetFlag(EFlags flag)
{
    m_State |= flag;
}

inline void
CNCMessageHandler::x_UnsetFlag(EFlags flag)
{
    m_State &= ~flag;
}

inline bool
CNCMessageHandler::x_IsFlagSet(EFlags flag) const
{
    _ASSERT(flag & eAllFlagsMask);

    return (m_State & flag) != 0;
}

inline void
CNCMessageHandler::x_CloseConnection(void)
{
    m_Server->CloseConnection(m_Socket);
}

inline void
CNCMessageHandler::InitDiagnostics(void)
{
    if (m_DiagContext.NotNull()) {
        CDiagContext::SetRequestContext(m_DiagContext);
    }
}

inline void
CNCMessageHandler::ResetDiagnostics(void)
{
    CDiagContext::SetRequestContext(NULL);
}



inline
CNCMessageHandler::CDiagnosticsGuard
    ::CDiagnosticsGuard(CNCMessageHandler* handler)
    : m_Handler(handler)
{
    m_Handler->InitDiagnostics();
}

inline
CNCMessageHandler::CDiagnosticsGuard::~CDiagnosticsGuard(void)
{
    m_Handler->ResetDiagnostics();
}


inline
CNCMsgHandler_Factory::CNCMsgHandler_Factory(CNetCacheServer* server)
    : m_Pool(THandlerPoolFactory(server))
{}

inline CNCMsgHandler_Factory::THandlerPool&
CNCMsgHandler_Factory::GetHandlerPool(void)
{
    return m_Pool;
}


inline
CNCMsgHandler_Proxy::CNCMsgHandler_Proxy(CNCMsgHandler_Factory* factory)
    : m_Factory(factory),
      m_Handler(factory->GetHandlerPool())
{}


inline IServer_ConnectionHandler*
CNCMsgHandler_Factory::Create(void)
{
    return new CNCMsgHandler_Proxy(this);
}


inline
CNCMsgHndlFactory_Proxy::CNCMsgHndlFactory_Proxy(CNetCacheServer* server)
    : m_Factory(new CNCMsgHandler_Factory(server))
{}

END_NCBI_SCOPE

#endif /* NETCACHE_MESSAGE_HANDLER__HPP */
