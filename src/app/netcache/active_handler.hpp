#ifndef NETCACHE__ACTIVE_HANDLER__HPP
#define NETCACHE__ACTIVE_HANDLER__HPP
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


#include "message_handler.hpp"
#include "periodic_sync.hpp"


BEGIN_NCBI_SCOPE


class CNCMessageHandler;
class CNCActiveHandler;
class CNCActiveHandler_Proxy;
class CNCPeerControl;
class CNCActiveSyncControl;


enum ENCClientHubStatus {
    eNCHubError,
    eNCHubWaitForConn,
    eNCHubConnReady,
    eNCHubCmdInProgress,
    eNCHubSuccess,
    eNCHubDataReady
};

class CNCActiveClientHub
{
public:
    static CNCActiveClientHub* Create(Uint8 srv_id, CNCMessageHandler* client);

    ENCClientHubStatus GetStatus(void);
    const string& GetErrMsg(void);
    CNCActiveHandler* GetHandler(void);
    void Release(void);

public:
    void SetStatus(ENCClientHubStatus status);
    void SetErrMsg(const string& msg);
    void SetHandler(CNCActiveHandler* handler);
    CNCMessageHandler* GetClient(void);

private:
    CNCActiveClientHub(CNCMessageHandler* client);


    CNCMessageHandler* m_Client;
    CNCActiveHandler* m_Handler;
    ENCClientHubStatus m_Status;
    string m_ErrMsg;
};


class CNCActiveHandler : public INCBlockedOpListener
{
public:
    static void Initialize(void);

    void ClientReleased(void);

    void SearchMeta(CRequestContext* cmd_ctx, const string& key);
    bool IsBlobExists(void);
    const SNCBlobSummary& GetBlobSummary(void);

    void CopyPut(CRequestContext* cmd_ctx,
                 const string& key,
                 Uint2 slot,
                 Uint8 orig_rec_no);
    void CopyProlong(const string& key,
                     Uint2 slot,
                     Uint8 orig_rec_no,
                     Uint8 orig_time,
                     const SNCBlobSummary& blob_sum);
    void ProxyRemove(CRequestContext* cmd_ctx,
                     const string& key,
                     const string& password,
                     int version,
                     Uint1 quorum);
    void ProxyHasBlob(CRequestContext* cmd_ctx,
                      const string& key,
                      const string& password,
                      Uint1 quorum);
    void ProxyGetSize(CRequestContext* cmd_ctx,
                      const string& key,
                      const string& password,
                      int version,
                      Uint1 quorum,
                      bool search,
                      bool force_local);
    void ProxySetValid(CRequestContext* cmd_ctx,
                       const string& key,
                       const string& password,
                       int version);
    void ProxyRead(CRequestContext* cmd_ctx,
                   const string& key,
                   const string& password,
                   int version,
                   Uint8 start_pos,
                   Uint8 size,
                   Uint1 quorum,
                   bool search,
                   bool force_local);
    void ProxyReadLast(CRequestContext* cmd_ctx,
                       const string& key,
                       const string& password,
                       Uint8 start_pos,
                       Uint8 size,
                       Uint1 quorum,
                       bool search,
                       bool force_local);
    void ProxyGetMeta(CRequestContext* cmd_ctx,
                      const string& key,
                      Uint1 quorum,
                      bool force_local);
    void ProxyWrite(CRequestContext* cmd_ctx,
                    const string& key,
                    const string& password,
                    int version,
                    Uint4 ttl,
                    Uint1 quorum);

    CBufferedSockReaderWriter& GetSockBuffer(void);
    void ForceBufferFlush(void);
    bool IsBufferFlushed(void);
    void FinishBlobFromClient(void);

    void SyncStart(CNCActiveSyncControl* ctrl, Uint8 local_rec_no, Uint8 remote_rec_no);
    void SyncBlobsList(CNCActiveSyncControl* ctrl);
    void SyncSend(CNCActiveSyncControl* ctrl, SNCSyncEvent* event);
    void SyncSend(CNCActiveSyncControl* ctrl, const string& key);
    void SyncRead(CNCActiveSyncControl* ctrl, SNCSyncEvent* event);
    void SyncRead(CNCActiveSyncControl* ctrl,
                  const string& key,
                  Uint8 create_time);
    void SyncProlongPeer(CNCActiveSyncControl* ctrl, SNCSyncEvent* event);
    void SyncProlongPeer(CNCActiveSyncControl* ctrl,
                         const string& key,
                         const SNCBlobSummary& blob_sum);
    void SyncProlongOur(CNCActiveSyncControl* ctrl, SNCSyncEvent* event);
    void SyncProlongOur(CNCActiveSyncControl* ctrl,
                        const string& key,
                        const SNCBlobSummary& blob_sum);
    void SyncCancel(CNCActiveSyncControl* ctrl);
    void SyncCommit(CNCActiveSyncControl* ctrl,
                    Uint8 local_rec_no,
                    Uint8 remote_rec_no);

public:
    CNCActiveHandler(Uint8 srv_id, CNCPeerControl* peer);
    virtual ~CNCActiveHandler(void);

    void SetServerConn(CServer_Connection*     srv_conn,
                       CNCActiveHandler_Proxy* proxy);
    void SetClientHub(CNCActiveClientHub* hub);
    void SetReservedForBG(bool value);
    bool IsReservedForBG(void);

    void OnTimeout(void);
    void OnClose(IServer_ConnectionHandler::EClosePeer peer);
    void OnRead(void);
    void OnWrite(void);
    EIO_Event GetEventsToPollFor(const CTime** alarm_time);
    bool IsReadyToProcess(void);

    /// Init diagnostics Client IP and Session ID for proper logging
    void InitDiagnostics(void);
    /// Reset diagnostics Client IP and Session ID to avoid logging
    /// not related to the request
    void ResetDiagnostics(void);

private:
    friend class CNCActiveClientHub;


    enum EStates {
        eConnIdle,
        eWaitClientRelease,
        eReadyForPool,
        eReadFoundMeta,
        eSendCopyPutCmd,
        eReadCopyPut,
        eReadCopyProlong,
        eReadConfirm,
        eReadDataPrefix,
        eReadDataForClient,
        eReadWritePrefix,
        eWriteDataForClient,
        eWaitOneLineAnswer,
        eWaitForMetaInfo,
        eWaitForFirstData,
        eWriteBlobData,
        eReadSyncStartAnswer,
        eReadEventsList,
        eReadBlobsList,
        eSendSyncGetCmd,
        eReadSyncGetAnswer,
        eReadBlobData,
        ePrepareSyncProlongCmd,
        eReadSyncProInfoAnswer,
        eExecuteProInfoCmd,
        eConnClosed
    };
    enum EFlags {
        fWaitForBlockedOp   = 0x10000,
        fWaitForClient      = 0x20000,
        eAllFlagsMask       = 0x30000
    };
    typedef int TStateFlags;

    enum ECommands {
        eSearchMeta = 1,
        eCopyPut,
        eCopyProlong,
        eNeedOnlyConfirm,
        eReadData,
        eWriteData,
        eSyncStart,
        eSyncBList,
        eSyncGet,
        eSyncProlongPeer,
        eSyncProInfo
    };


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

    /// Main dispatcher of state machine
    void x_ManageCmdPipeline(void);
    void x_MayDeleteThis(void);
    bool x_ReplaceServerConn(void);
    void x_DeferConnection(void);
    void x_CloseConnection(void);
    void x_WaitForWouldBlock(void);
    void x_AddConnToPool(void);
    void x_SetStateAndAddToPool(EStates state);
    void x_SendCmdToExecute(void);
    void x_StartWritingBlob(void);
    void x_FinishWritingBlob(void);
    void x_FakeWritingBlob(void);
    void x_FinishCommand(bool success);
    void x_ProcessPeerError(void);
    void x_ProcessProtocolError(void);
    void x_DoCopyPut(void);
    void x_DoSyncGet(void);
    void x_SendCopyProlongCmd(const SNCBlobSummary& blob_sum);
    bool x_ReadSizeToRead(void);
    void x_DoProlongOur(void);

    bool x_ConnIdle(void);
    bool x_ReadFoundMeta(void);
    bool x_SendCopyPutCmd(void);
    bool x_ReadCopyPut(void);
    bool x_ReadCopyProlong(void);
    bool x_ReadConfirm(void);
    bool x_ReadDataPrefix(void);
    bool x_ReadDataForClient(void);
    bool x_ReadWritePrefix(void);
    bool x_WriteDataForClient(void);
    bool x_WaitOneLineAnswer(void);
    bool x_WaitForMetaInfo(void);
    bool x_WaitForFirstData(void);
    bool x_WriteBlobData(void);
    bool x_PrepareSyncProlongCmd(void);
    bool x_ReadSyncStartAnswer(void);
    bool x_ReadEventsList(void);
    bool x_ReadBlobsList(void);
    bool x_SendSyncGetCmd(void);
    bool x_ReadSyncGetAnswer(void);
    bool x_ReadBlobData(void);
    bool x_ReadSyncProInfoAnswer(void);
    bool x_ExecuteProInfoCmd(void);


    CMutex  m_ObjLock;
    Uint8   m_SrvId;
    string  m_CmdToSend;
    CTempString m_Response;
    CNCPeerControl* m_Peer;
    CNCActiveClientHub* m_Client;
    CServer_Connection* m_SrvConn;
    CNCActiveSyncControl* m_SyncCtrl;
    CNCActiveHandler_Proxy* m_Proxy;
    CBufferedSockReaderWriter m_SockBuffer;
    CRef<CRequestContext> m_CmdCtx;
    CRef<CRequestContext> m_ConnCtx;
    string m_ConnReqId;
    Uint8  m_CntCmds;
    string m_BlobKey;
    CNCBlobAccessor* m_BlobAccess;
    SNCBlobSummary m_BlobSum;
    Uint8 m_OrigTime;
    Uint8 m_OrigRecNo;
    Uint8 m_OrigServer;
    volatile TStateFlags m_State;
    ECommands   m_CurCmd;
    Uint8 m_ThrottleTime;
    Uint8 m_SizeToRead;
    Uint4 m_ChunkSize;
    Uint2 m_BlobSlot;
    ESynActionType m_SyncAction;
    bool m_ReservedForBG;
    bool m_InCmdPipeline;
    volatile bool m_AddedToPool;
    bool m_DidFirstWrite;
    bool m_GotAnyAnswer;
    bool m_GotCmdAnswer;
    bool m_NeedFlushBuff;
    bool m_BlobExists;
    bool m_WaitForThrottle;
    bool m_NeedDelete;
    string m_ErrMsg;
};


class CNCActiveHandler_Proxy : public IServer_ConnectionHandler
{
public:
    CNCActiveHandler_Proxy(CNCActiveHandler* handler);
    virtual ~CNCActiveHandler_Proxy(void);
    void SetSocket(CSocket* sock);
    void SetHandler(CNCActiveHandler* handler);

    virtual void OnOpen(void);
    virtual void OnTimeout(void);
    virtual void OnClose(IServer_ConnectionHandler::EClosePeer);
    virtual void OnRead(void);
    virtual void OnWrite(void);
    virtual EIO_Event GetEventsToPollFor(const CTime** alarm_time) const;
    virtual bool IsReadyToProcess(void) const;

private:
    CNCActiveHandler_Proxy(CNCActiveHandler_Proxy&);
    CNCActiveHandler_Proxy& operator= (CNCActiveHandler_Proxy&);


    CNCActiveHandler* m_Handler;
    CSocket* m_Socket;
};



//////////////////////////////////////////////////////////////////////////
// Inline functions
//////////////////////////////////////////////////////////////////////////

inline
CNCActiveClientHub::CNCActiveClientHub(CNCMessageHandler* client)
    : m_Client(client),
      m_Handler(NULL),
      m_Status(eNCHubSuccess)
{}

inline void
CNCActiveClientHub::SetStatus(ENCClientHubStatus status)
{
    m_Status = status;
}

inline ENCClientHubStatus
CNCActiveClientHub::GetStatus(void)
{
    return m_Status;
}

inline void
CNCActiveClientHub::SetErrMsg(const string& msg)
{
    m_ErrMsg = msg;
}

inline const string&
CNCActiveClientHub::GetErrMsg(void)
{
    return m_ErrMsg;
}

inline void
CNCActiveClientHub::SetHandler(CNCActiveHandler* handler)
{
    m_Handler = handler;
}

inline CNCActiveHandler*
CNCActiveClientHub::GetHandler(void)
{
    return m_Handler;
}

inline CNCMessageHandler*
CNCActiveClientHub::GetClient(void)
{
    return m_Client;
}


inline void
CNCActiveHandler::SetClientHub(CNCActiveClientHub* hub)
{
    m_Client = hub;
}

inline void
CNCActiveHandler::SetReservedForBG(bool value)
{
    m_ReservedForBG = value;
}

inline bool
CNCActiveHandler::IsReservedForBG(void)
{
    return m_ReservedForBG;
}

inline bool
CNCActiveHandler::IsBlobExists(void)
{
    return m_BlobExists;
}

inline const SNCBlobSummary&
CNCActiveHandler::GetBlobSummary(void)
{
    return m_BlobSum;
}


inline void
CNCActiveHandler_Proxy::SetSocket(CSocket* sock)
{
    m_Socket = sock;
}

inline void
CNCActiveHandler_Proxy::SetHandler(CNCActiveHandler* handler)
{
    m_Handler = handler;
}

END_NCBI_SCOPE

#endif /* NETCACHE__ACTIVE_HANDLER__HPP */
