#ifndef NETCACHE__MESSAGE_HANDLER__HPP
#define NETCACHE__MESSAGE_HANDLER__HPP
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
#include <util/thread_pool.hpp>
#include <connect/server.hpp>
#include <connect/services/netservice_protocol_parser.hpp>

#include "netcached.hpp"
#include "sync_log.hpp"
#include "distribution_conf.hpp"


BEGIN_NCBI_SCOPE


class CNCActiveClientHub;
class CNCBlobAccessor;


enum {
    /// Initial size of the buffer used for reading/writing from socket.
    /// Reading buffer never changes its size from the initial, writing buffer
    /// can grow if there's need to.
    kReadNWBufSize = 4 * 1024,
    kWriteNWMinSize = 1000,
    kWriteNWBufSize = 2 * kWriteNWMinSize
};

typedef CSimpleBufferT<char> TNCBufferType;



/// Object reading/writing from socket and automatically buffering data
class CBufferedSockReaderWriter
{
public:
    CBufferedSockReaderWriter(void);

    /// Initialize object with socket and default timeout that should be
    /// always set as socket's read/write timeout when outside methods of
    /// the object.
    void ResetSocket(CSocket* socket, unsigned int def_timeout);
    ///
    void SetTimeout(double def_timeout);

    /// Check if there's something in the buffer to read from
    bool HasDataToRead(void) const;
    /// Check if there's data in writing buffer which will be written to
    /// socket on next call to Flush().
    ///
    /// @sa Flush
    bool IsWriteDataPending(void) const;
    bool HasError(void) const;

    bool ReadToBuf(void);
    size_t Read(void* buf, size_t size);
    /// Read one line from socket (or buffer). After reading this line will
    /// be erased from the reading buffer.
    ///
    /// @return
    ///  TRUE if line was successfully read, FALSE if end-of-line is not met
    ///  in data yet or there's no data in socket at all.
    bool ReadLine(CTempString* line);

    /// Get pointer to buffered data read from socket
    const void* GetReadBufData(void) const;
    /// Get size of buffered data available
    size_t GetReadReadySize(void) const;
    /// Erase first amount bytes from reading buffer - they're processed and
    /// no longer necessary.
    void EraseReadBufData(size_t amount);

    /// Write message to the writing buffer. Message is created by
    /// concatenating prefix and msg texts. Nothing is written directly to
    /// socket until Flush() is called.
    ///
    /// @sa Flush
    void   WriteMessage(CTempString prefix, CTempString msg);
    size_t Write(const void* buf, size_t size);
    void   WriteNoFail(const void* buf, size_t size);
    /// Write all data residing in writing buffer to the socket.
    /// If socket cannot accept all data immediately then leave what has left
    /// in the buffer as is until next call and return without waiting.
    void Flush(void);

    size_t WriteFromBuffer(CBufferedSockReaderWriter& buf, size_t max_size);

public:
    // For internal use only

    /// Set socket's read/write timeout to zero.
    void ZeroSocketTimeout(void);
    /// Set socket's read/write timeout to default value for outside world.
    void ResetSocketTimeout(void);

private:
    size_t x_ReadFromSocket(void* buf, size_t size);
    size_t x_ReadFromBuf(void* dest, size_t size);
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
    void x_CompactBuffer(TNCBufferType& buffer, size_t& pos);
    void x_CopyData(const void* buf, size_t size);
    size_t x_WriteToSocket(const void* buf, size_t size);
    size_t x_WriteNoPending(const void* buf, size_t size);


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
    TNCBufferType m_ReadBuff;
    /// Current position inside reading buffer
    size_t      m_ReadDataPos;
    /// Writing buffer
    TNCBufferType m_WriteBuff;
    /// Current position inside writing buffer
    size_t      m_WriteDataPos;
    /// Types of end-of-line bytes already read from buffer
    TCRLF_Bytes m_CRLFMet;
    bool        m_HasError;
};


/// Type of access to storage each NetCache command can have
enum ENCStorageAccess {
    eWithoutBlob     = 1,  ///< Storage existence is a must but no blob lock is
                           ///< necessary
    eWithBlob        = 2,  ///< Command must obtain lock on blob before
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
class CNCMessageHandler : public INCBlockedOpListener
{
public:
    /// Create handler for the given NetCache server object
    CNCMessageHandler(void);
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

    CBufferedSockReaderWriter& GetSockBuffer(void);
    void InitialAnswerWritten(void);
    void ForceBufferFlush(void);
    bool IsBufferFlushed(void);
    void WriteInitWriteResponse(const string& response);

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
        ENCAccessType    blob_access;
    };
    /// Type definitions for NetCache protocol parser
    typedef SNSProtoCmdDef<SCommandExtra>      SCommandDef;
    typedef SNSProtoParsedCmd<SCommandExtra>   SParsedCmd;
    typedef CNetServProtoParser<SCommandExtra> TProtoParser;

    /// Command processors
    bool x_DoCmd_Alive(void);
    bool x_DoCmd_Health(void);
    bool x_DoCmd_Shutdown(void);
    bool x_DoCmd_Version(void);
    bool x_DoCmd_GetConfig(void);
    bool x_DoCmd_GetServerStats(void);
    bool x_DoCmd_Reinit(void);
    bool x_DoCmd_Reconf(void);
    bool x_DoCmd_Put2(void);
    bool x_DoCmd_Put3(void);
    bool x_DoCmd_Get(void);
    bool x_DoCmd_GetLast(void);
    bool x_DoCmd_SetValid(void);
    bool x_DoCmd_GetSize(void);
    bool x_DoCmd_HasBlob(void);
    bool x_DoCmd_HasBlobImpl(void);
    bool x_DoCmd_Remove(void);
    bool x_DoCmd_Remove2(void);
    bool x_DoCmd_IC_Store(void);
    bool x_DoCmd_SyncStart(void);
    bool x_DoCmd_SyncBlobsList(void);
    bool x_DoCmd_CopyPut(void);
    bool x_DoCmd_SyncPut(void);
    bool x_DoCmd_CopyProlong(void);
    bool x_DoCmd_SyncProlong(void);
    bool x_DoCmd_SyncGet(void);
    bool x_DoCmd_SyncProlongInfo(void);
    bool x_DoCmd_SyncCommit(void);
    bool x_DoCmd_SyncCancel(void);
    bool x_DoCmd_GetMeta(void);
    bool x_DoCmd_ProxyMeta(void);
    bool x_DoCmd_GetBlobsList(void);
    /// Universal processor for all commands not implemented now (because they
    /// are not used by anybody and the very fact of introducing them was
    /// foolish).
    bool x_DoCmd_NotImplemented(void);

    /// Statuses of commands to be set in diagnostics' request context
    /// Additional statuses can be taken from
    /// http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html
    enum EHTTPStatus {
        eStatus_OK          = 200,  ///< Command is ok and execution is good
        eStatus_Inactive    = 204,
        eStatus_SyncBList   = 205,
        eStatus_NewerBlob   = 206,
        eStatus_PUT2Used    = 301,
        eStatus_RaceCond    = 302,
        eStatus_SyncBusy    = 303,
        eStatus_CrossSync   = 305,
        eStatus_SyncAborted = 306,
        eStatus_GCNoAdvance = 307,
        eStatus_JustStarted = 308,
        eStatus_BadCmd      = 400,  ///< Command is incorrect
        eStatus_BadPassword = 401,  ///< Bad password for accessing the blob
        eStatus_Disabled    = 403,
        eStatus_NotFound    = 404,  ///< Blob was not found
        eStatus_NotAllowed  = 405,  ///< Operation not allowed with current
                                    ///< settings
        eStatus_CmdTimeout  = 408,  ///< Command timeout is exceeded
        eStatus_CondFailed  = 412,  ///< Precondition stated in command has
                                    ///< failed
        eStatus_ConnClosed  = 499,
        eStatus_ServerError = 500,  ///< Internal server error
        eStatus_NoImpl      = 501,  ///< Command is not implemented
        eStatus_StaleSync   = 502,
        eStatus_PeerError   = 503,
        eStatus_NoDiskSpace = 504
    };

private:
    /// States of handling finite state machine
    enum EStates {
        ePreAuthenticated,     ///< Connection is opened but not authenticated
        eReadyForCommand,      ///< Ready to receive next NetCache command
        eCommandReceived,      ///< Command received but not parsed yet
        eWaitForBlobAccess,    ///< Locking of blob needed for command is in
                               ///< progress
        ePasswordFailed,       ///< Processing of bad password is needed
        eReadBlobSignature,    ///< Reading of signature in blob transfer
                               ///< protocol
        eReadBlobChunkLength,  ///< Reading chunk length in blob transfer
                               ///< protocol
        eReadBlobChunk,        ///< Reading chunk data in blob transfer
                               ///< protocol
        eReadChunkToActive,
        eWaitForFirstData,     ///<
        eWriteBlobData,        ///< Writing data from blob to socket
        eWriteSendBuff,
        eProxyToNextPeer,
        eSendCmdAsProxy,
        eWaitForPeerAnswer,
        eWaitForActiveWrite,
        eReadMetaNextPeer,
        eSendGetMetaCmd,
        eReadMetaResults,
        ePutToNextPeer,
        eSendPutToPeerCmd,
        eReadPutResults,
        eSocketClosed          ///< Connection closed or not opened yet
    };
    /// Additional flags that could be applied to object states
    enum EFlags {
        fCommandStarted    = 0x0010000,  ///< Command startup code is executed
        fCommandPrinted    = 0x0020000,  ///< "request-start" message was
                                         ///< printed to diagnostics
        fConnStartPrinted  = 0x0040000,  ///< 
        fSwapLengthBytes   = 0x0080000,  ///< Byte order should be swapped when
                                         ///< reading length of chunks in blob
                                         ///< transfer protocol
        fConfirmBlobPut    = 0x0100000,  ///< After blob is written to storage
                                         ///< "OK" message should be written to
                                         ///< socket
        fReadExactBlobSize = 0x0200000,  ///< There is exact size of the blob
                                         ///< transferred to NetCache
        fWaitForBlockedOp  = 0x0400000,  ///< 
        fSkipBlobEOF       = 0x0800000,
        fCopyLogEvent      = 0x1000000,
        fNeedSyncFinish    = 0x2000000,
        fWaitForActive     = 0x4000000,
        eAllFlagsMask      = 0x7FF0000   ///< Sum of all flags above
    };
    /// Bit mask of EFlags plus one of EStates
    typedef int TStateFlags;

    ///
    virtual void OnBlockedOpFinish(void);

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
    ///
    unsigned int x_GetBlobTTL(void);

    // State machine implementation

    /// Main dispatcher of state machine
    void x_ManageCmdPipeline(void);
    /// Read authentication message from client
    bool x_ReadAuthMessage(void);
    /// Check whether current client name is administrative client, throw
    /// exception if not.
    bool x_CheckAdminClient(void);
    /// Read command and start it if it's available
    bool x_ReadCommand(void);
    /// Process current command
    bool x_DoCommand(void);
    /// Process "waiting" for blob locking. In fact just shift to next state
    /// if lock is acquired and just return if not.
    bool x_WaitForBlobAccess(void);
    /// Process the situation when password provided to access blob is incorrect
    bool x_ProcessBadPassword(void);
    /// Read signature in blob transfer protocol
    bool x_ReadBlobSignature(void);
    /// Read length of chunk in blob transfer protocol
    bool x_ReadBlobChunkLength(void);
    /// Read chunk data in blob transfer protocol
    bool x_ReadBlobChunk(void);
    bool x_ReadChunkToActive(void);
    bool x_WaitForFirstData(void);
    /// Write data from blob to socket
    bool x_WriteBlobData(void);
    bool x_WriteSendBuff(void);
    bool x_ProxyToNextPeer(void);
    bool x_SendCmdAsProxy(void);
    bool x_WaitForPeerAnswer(void);
    bool x_WaitForActiveWrite(void);
    bool x_ReadMetaNextPeer(void);
    bool x_SendGetMetaCmd(void);
    bool x_ReadMetaResults(void);
    bool x_PutToNextPeer(void);
    bool x_SendPutToPeerCmd(void);
    bool x_ReadPutResults(void);

    /// Close connection (or at least make CServer believe that we closed it
    /// by ourselves)
    void x_CloseConnection(void);
    /// Assign command parameters returned by protocol parser to handler
    /// member variables
    void x_AssignCmdParams(TNSProtoParams& params);
    /// Print "request_start" message into diagnostics with all parameters of
    /// the current command.
    void x_PrintRequestStart(const SParsedCmd& cmd,
                             auto_ptr<CDiagContext_Extra>& diag_extra);
    /// Start command returned by protocol parser
    bool x_StartCommand(SParsedCmd& cmd);
    ///
    void x_WaitForWouldBlock(void);
    /// Command execution is finished, do cleanup work
    void x_FinishCommand(bool do_sock_write);
    /// Start reading blob from socket
    void x_StartReadingBlob(void);
    /// Client finished sending blob, do cleanup
    void x_FinishReadingBlob(void);
    /// Transform socket for the handler to socket stream which will then be
    /// used to write something. Connection to client will be closed when
    /// created socket stream is deleted. Though message about closing will be
    /// printed during execution of this method, CServer will think that
    /// connection closed also inside this method.
    AutoPtr<CConn_SocketStream> x_PrepareSockStream(void);

    void x_ProlongBlobDeadTime(void);
    void x_ProlongVersionLife(void);
    bool x_CanStartSyncCommand(bool can_abort = true);
    void x_ReadFullBlobsList(void);
    void x_GetCurSlotServers(void);


    /// Special guard to properly initialize and reset diagnostics
    class CDiagnosticsGuard
    {
    public:
        CDiagnosticsGuard(CNCMessageHandler* handler);
        ~CDiagnosticsGuard(void);

    private:
        CNCMessageHandler* m_Handler;
    };


    CFastMutex                m_ObjLock;
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
    CNCRef<CRequestContext>   m_CmdCtx;
    ///
    CNCRef<CRequestContext>   m_ConnCtx;
    ///
    string                    m_ConnReqId;
    ///
    Uint8                     m_CntCmds;
    /// Holder of the lock for blob
    CNCBlobAccessor*          m_BlobAccess;
    /// Length of the current blob chunk to read from socket
    Uint4                     m_ChunkLen;

    ///
    unsigned short            m_LocalPort;
    /// 
    TStringMap                m_ClientParams;
    ///
    const SNCSpecificParams*  m_AppSetup;
    ///
    const SNCSpecificParams*  m_BaseAppSetup;
    ///
    string                    m_PrevCache;
    ///
    const SNCSpecificParams*  m_PrevAppSetup;
    /// Time when connection was opened
    CTime                     m_ConnTime;
    /// Time when command started execution
    CTime                     m_CmdStartTime;
    /// Maximum time when command should finish execution or it will be
    /// timed out
    CTime                     m_MaxCmdTime;

    ///
    string                    m_RawKey;
    /// Blob key in current command
    string                    m_BlobKey;
    /// Blob version in current command
    int                       m_BlobVersion;
    /// "Password" to access the blob
    string                    m_BlobPass;
    /// Version of the blob key to generate
    unsigned int              m_KeyVersion;
    /// Time-to-live value for the blob
    unsigned int              m_BlobTTL;
    /// Offset to start blob reading from
    Uint8                      m_StartPos;
    /// Exact size of the blob sent by client
    Uint8                     m_Size;
    Uint8                     m_BlobSize;
    SNCBlobVerData            m_CopyBlobInfo;
    Uint8                     m_OrigRecNo;
    Uint8                     m_OrigSrvId;
    Uint8                     m_OrigTime;
    Uint8                     m_SrvId;
    Uint2                     m_Slot;
    Uint8                     m_LocalRecNo;
    Uint8                     m_RemoteRecNo;
    auto_ptr<TNCBufferType>   m_SendBuff;
    size_t                    m_SendPos;
    string                    m_RawBlobPass;
    Uint8                     m_SyncId;
    Uint2                     m_BlobSlot;
    Uint2                     m_TimeBucket;
    Uint1                     m_Quorum;
    bool                      m_SearchOnRead;
    bool                      m_ThisServerIsMain;
    bool                      m_LatestExist;
    bool                      m_InSyncCmd;
    bool                      m_WaitForThrottle;
    bool                      m_ForceLocal;
    bool                      m_ConfirmPut;
    bool                      m_GotInitialAnswer;
    bool                      m_NeedFlushBuff;
    Uint1                     m_SrvsIndex;
    int                       m_CmdVersion;
    Uint8                     m_ThrottleTime;
    Uint8                     m_LatestSrvId;
    SNCBlobSummary            m_LatestBlobSum;
    TServersList              m_CheckSrvs;
    CNCActiveClientHub*       m_ActiveHub;
};


/// Factory creating CNCMessageHandler objects.
/// Actually implements standard pool of objects.
class CNCMsgHandler_Factory : public CObject
{
public:
    CNCMsgHandler_Factory(void);
    virtual ~CNCMsgHandler_Factory(void);

    /// Create new NetCache handler.
    /// IServer_ConnectionFactory implementation.
    IServer_ConnectionHandler* Create(void);

public:
    // For internal use only
    typedef CObjPool<CNCMessageHandler>     THandlerPool;

    /// Get pool for creating CNCMessageHandler objects
    THandlerPool& GetHandlerPool(void);

private:
    /// Pool of CNCMessageHandler objects
    THandlerPool     m_Pool;
};

typedef CNCRef<CNCMsgHandler_Factory>  TNCMsgHandlerFactoryRef;


/// Special proxy for handler factory.
/// It's necessary because server have ownership over factory and it's
/// destroyed before all sockets are destroyed and thus before all handlers
/// can be destroyed.
class CNCMsgHndlFactory_Proxy : public IServer_ConnectionFactory
{
public:
    CNCMsgHndlFactory_Proxy(void);
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



//////////////////////////////////////////////////////////////////////////
// Inline functions
//////////////////////////////////////////////////////////////////////////

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

inline bool
CBufferedSockReaderWriter::HasError(void) const
{
    return m_HasError;
}

inline const void*
CBufferedSockReaderWriter::GetReadBufData(void) const
{
    return m_ReadBuff.data() + m_ReadDataPos;
}

inline size_t
CBufferedSockReaderWriter::GetReadReadySize(void) const
{
    return m_ReadBuff.size() - m_ReadDataPos;
}

inline void
CBufferedSockReaderWriter::EraseReadBufData(size_t amount)
{
    m_ReadDataPos = min(m_ReadDataPos + amount, m_ReadBuff.size());
}

inline void
CBufferedSockReaderWriter::WriteNoFail(const void* buf, size_t size)
{
    x_CopyData(buf, size);
}

inline void
CBufferedSockReaderWriter::SetTimeout(double def_timeout)
{
    m_DefTimeout.sec = int(def_timeout);
    m_DefTimeout.usec = int(def_timeout * 1000000);
    ResetSocketTimeout();
}


inline
CNCMsgHandler_Factory::CNCMsgHandler_Factory(void)
    : m_Pool(50)
{}


inline
CNCMsgHndlFactory_Proxy::CNCMsgHndlFactory_Proxy(void)
    : m_Factory(new CNCMsgHandler_Factory())
{}


END_NCBI_SCOPE

#endif /* NETCACHE__MESSAGE_HANDLER__HPP */
