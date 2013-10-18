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


#include "nc_utils.hpp"
#include "periodic_sync.hpp"
#include "distribution_conf.hpp"


BEGIN_NCBI_SCOPE


class CNCMessageHandler;
class CNCActiveHandler;
class CNCActiveHandler_Proxy;
class CNCPeerControl;
class CNCActiveSyncControl;
struct SNCBlobSummary;
struct SNCSyncEvent;
class CNCBlobAccessor;


enum ENCClientHubStatus {
    eNCHubError,
    eNCHubWaitForConn,
    eNCHubConnReady,
    eNCHubCmdInProgress,
    eNCHubSuccess,
    eNCHubDataReady
};

class CNCActiveClientHub : public CSrvRCUUser
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

    virtual void ExecuteRCU(void);


    CNCMessageHandler* m_Client;
    CNCActiveHandler* m_Handler;
    ENCClientHubStatus m_Status;
    string m_ErrMsg;
};


struct SActiveList_tag;
typedef intr::list_base_hook<intr::tag<SActiveList_tag> >   TActiveListHook;


/*
    sends command to other instances of NetCache.
    processes results.
    created by CNCPeerControl
    used by CNCPeerControl, CNCActiveSyncControl, CNCMessageHandler 

    begin: x_InvalidState
    -> x_InvalidState: abort

    Object becomes usable on SetClientHub (state becomes x_WaitClientRelease)
    or, somebody (peercontrol) calls CopyPut, or CopyProlong or other methods
    then, it sets proper State for itself.

    All processing goes approx like this:
    somebody calls a method (eg, ProxyRemove):
        initial preparation;
        create a command, goto x_SendCmdToExecute
        check for errors, send command, wait for reply (x_WaitOneLineAnswer)
        check for errors, read reply
        do cmd, goto x_FinishCommand
        clean resources, x_WaitClientRelease, x_PutSelfToPool (waiting for next cmd)
*/
class CNCActiveHandler : public CSrvStatesTask<CNCActiveHandler>,
                         public TActiveListHook
{
public:
    static void Initialize(void);

    void ClientReleased(void);

    void SearchMeta(CRequestContext* cmd_ctx, const string& key);
    bool IsBlobExists(void);
    const SNCBlobSummary& GetBlobSummary(void);

    void CopyPurge(CRequestContext* cmd_ctx,
                 const string& cache,
                 const CSrvTime& when);
    /*
        request meta info, x_WaitForMetaInfo ->  x_SendCopyPutCmd -> 
        on error, goto  x_FinishCommand
        create command, goto x_SendCmdToExecute
        -> x_SendCmdToExecute
            on error, goto x_CloseCmdAndConn, or  x_ConnClosedReplaceable
            write cmd, goto  x_WaitOneLineAnswer  -> x_ReadCopyPut
        -> x_ReadCopyPut
            on error, goto  x_FinishCommand, or x_FakeWritingBlob
            goto x_WriteBlobData -> x_FinishWritingBlob ->
                request confirmation -> x_WaitOneLineAnswer
    */
    void CopyPut(CRequestContext* cmd_ctx,
                 const string& key,
                 Uint2 slot,
                 Uint8 orig_rec_no);
    // x_SendCmdToExecute ->  x_WaitOneLineAnswer -> x_ReadCopyProlong -> x_FinishCommand
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
    void ProxyProlong(CRequestContext* cmd_ctx,
                      const string& raw_key,
                      const string& password,
                      unsigned int add_time,
                      Uint1 quorum,
                      bool search,
                      bool force_local);

    CSrvSocketTask* GetSocket(void);
    CTempString GetCmdResponse(void);
    bool GotClientResponse(void);

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

    void SetProxy(CNCActiveHandler_Proxy* proxy);
    void SetClientHub(CNCActiveClientHub* hub);
    void SetReservedForBG(bool value);
    bool IsReservedForBG(void);

    void CloseForShutdown(void);
    void CheckCommandTimeout(void);

private:
    friend class CNCActiveClientHub;
    friend class CNCActiveHandler_Proxy;


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


    State x_MayDeleteThis(void);
    State x_ReplaceServerConn(void);
    State x_ConnClosedReplaceable(void);
    State x_CloseCmdAndConn(void);
    State x_CloseConn(void);
    void x_StartProcessing(void);
    void x_SetStateAndStartProcessing(State state);
    State x_SendCmdToExecute(void);
    void x_StartWritingBlob(void);
    State x_FinishWritingBlob(void);
    State x_FakeWritingBlob(void);
    void x_FinishSyncCmd(ESyncResult result);
    void x_CleanCmdResources(void);
    State x_FinishCommand(void);
    State x_ProcessPeerError(void);
    State x_ProcessProtocolError(void);
    void x_DoCopyPut(void);
    void x_DoSyncGet(void);
    void x_SendCopyProlongCmd(const SNCBlobSummary& blob_sum);
    State x_ReadSizeToRead(void);
    void x_DoProlongOur(void);

    State x_InvalidState(void);
    State x_IdleState(void);
    State x_WaitClientRelease(void);
    State x_PutSelfToPool(void);
    State x_ReadFoundMeta(void);
    State x_SendCopyPutCmd(void);
    State x_ReadCopyPut(void);
    State x_ReadCopyProlong(void);
    State x_ReadConfirm(void);
    State x_ReadDataPrefix(void);
    State x_ReadDataForClient(void);
    State x_ReadWritePrefix(void);
    State x_FinishBlobFromClient(void);
    State x_WaitOneLineAnswer(void);
    State x_WaitForMetaInfo(void);
    State x_WriteBlobData(void);
    State x_PrepareSyncProlongCmd(void);
    State x_ReadSyncStartHeader(void);
    State x_ReadSyncStartAnswer(void);
    State x_ReadSyncStartExtra(void);
    State x_ReadEventsListKeySize(void);
    State x_ReadEventsListBody(void);
    State x_ReadBlobsListKeySize(void);
    State x_ReadBlobsListBody(void);
    State x_SendSyncGetCmd(void);
    State x_ReadSyncGetHeader(void);
    State x_ReadSyncGetAnswer(void);
    State x_ReadBlobData(void);
    State x_ReadSyncProInfoAnswer(void);
    State x_ExecuteProInfoCmd(void);

    void x_SetSlotAndBucketAndVerifySlot(Uint2 slot);


    Uint8   m_SrvId;
    string  m_CmdToSend;
    CTempString m_Response;
    CNCPeerControl* m_Peer;
    CNCActiveClientHub* m_Client;
    CNCActiveSyncControl* m_SyncCtrl;
    CNCActiveHandler_Proxy* m_Proxy;
    Uint8  m_CntCmds;
    string m_BlobKey;
    CNCBlobAccessor* m_BlobAccess;
    SNCBlobSummary* m_BlobSum;
    Uint8 m_OrigTime;
    Uint8 m_OrigRecNo;
    Uint8 m_OrigServer;
    ECommands m_CurCmd;
    Uint8 m_SizeToRead;
    Uint4 m_ChunkSize;
    Uint2 m_BlobSlot;
    Uint2 m_TimeBucket;
    ESynActionType m_SyncAction;
    bool m_ReservedForBG;
    bool m_ProcessingStarted;
    bool m_CmdStarted;
    bool m_GotAnyAnswer;
    bool m_GotCmdAnswer;
    bool m_GotClientResponse;
    bool m_BlobExists;
    bool m_CmdSuccess;
    bool m_CmdFromClient;
    bool m_Purge;
    Uint2 m_KeySize;
    string m_ErrMsg;
    string m_SyncStartExtra;
    TNCBufferType m_ReadBuf;
};

inline void
CNCActiveHandler::x_SetSlotAndBucketAndVerifySlot(Uint2 slot)
{
    if (!CNCDistributionConf::GetSlotByKey(m_BlobKey,
            m_BlobSlot, m_TimeBucket) || m_BlobSlot != slot)
        abort();
}

class CNCActiveHandler_Proxy : public CSrvSocketTask
{
public:
    CNCActiveHandler_Proxy(CNCActiveHandler* handler);
    virtual ~CNCActiveHandler_Proxy(void);
    void SetHandler(CNCActiveHandler* handler);
    void NeedToProxySocket(void);
    bool SocketProxyDone(void);

    virtual void ExecuteSlice(TSrvThreadNum thr_num);

private:
    CNCActiveHandler_Proxy(CNCActiveHandler_Proxy&);
    CNCActiveHandler_Proxy& operator= (CNCActiveHandler_Proxy&);


    CNCActiveHandler* m_Handler;
    bool m_NeedToProxy;
};



//////////////////////////////////////////////////////////////////////////
// Inline functions
//////////////////////////////////////////////////////////////////////////

inline
CNCActiveClientHub::CNCActiveClientHub(CNCMessageHandler* client)
    : m_Client(client),
      m_Handler(NULL),
      m_Status(eNCHubWaitForConn)
{}

inline ENCClientHubStatus
CNCActiveClientHub::GetStatus(void)
{
    return ACCESS_ONCE(m_Status);
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


inline CSrvSocketTask*
CNCActiveHandler::GetSocket(void)
{
    return m_Proxy;
}

inline CTempString
CNCActiveHandler::GetCmdResponse(void)
{
    return m_Response;
}

inline bool
CNCActiveHandler::GotClientResponse(void)
{
    return m_GotClientResponse;
}

inline bool
CNCActiveHandler::IsBlobExists(void)
{
    return m_BlobExists;
}

inline const SNCBlobSummary&
CNCActiveHandler::GetBlobSummary(void)
{
    return *m_BlobSum;
}


inline void
CNCActiveHandler_Proxy::SetHandler(CNCActiveHandler* handler)
{
    m_Handler = handler;
}

END_NCBI_SCOPE

#endif /* NETCACHE__ACTIVE_HANDLER__HPP */
