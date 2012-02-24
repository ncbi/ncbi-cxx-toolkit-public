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
 * Author: Pavel Ivanov
 *
 */

#include <ncbi_pch.hpp>

#include <connect/impl/server_connection.hpp>

#include "active_handler.hpp"
#include "peer_control.hpp"
#include "periodic_sync.hpp"
#include "nc_storage.hpp"


#ifndef NCBI_OS_MSWIN
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <netinet/tcp.h>
#endif


BEGIN_NCBI_SCOPE


static string s_PeerAuthString;



CNCActiveHandler_Proxy::CNCActiveHandler_Proxy(CNCActiveHandler* handler)
    : m_Handler(handler),
      m_Socket(NULL)
{}

CNCActiveHandler_Proxy::~CNCActiveHandler_Proxy(void)
{}

void
CNCActiveHandler_Proxy::OnOpen(void)
{
    abort();
}

void
CNCActiveHandler_Proxy::OnTimeout(void)
{
    if (m_Handler)
        m_Handler->OnTimeout();
}

void
CNCActiveHandler_Proxy::OnClose(IServer_ConnectionHandler::EClosePeer peer)
{
    if (m_Handler)
        m_Handler->OnClose(peer);
}

void
CNCActiveHandler_Proxy::OnRead(void)
{
    if (m_Handler)
        m_Handler->OnRead();
    else
        abort();
}

void
CNCActiveHandler_Proxy::OnWrite(void)
{
    if (m_Handler)
        m_Handler->OnWrite();
    else
        g_NetcacheServer->CloseConnection(m_Socket);
}

EIO_Event
CNCActiveHandler_Proxy::GetEventsToPollFor(const CTime** alarm_time) const
{
    return m_Handler? m_Handler->GetEventsToPollFor(alarm_time): eIO_Write;
}

bool
CNCActiveHandler_Proxy::IsReadyToProcess(void) const
{
    return m_Handler? m_Handler->IsReadyToProcess(): true;
}


CNCActiveClientHub*
CNCActiveClientHub::Create(Uint8 srv_id, CNCMessageHandler* client)
{
    CNCActiveClientHub* hub = new CNCActiveClientHub(client);
    CNCPeerControl* peer = CNCPeerControl::Peer(srv_id);
    peer->AssignClientConn(hub);
    return hub;
}

void
CNCActiveClientHub::Release(void)
{
    if (m_Handler)
        m_Handler->ClientReleased();
    delete this;
}


inline CNCActiveHandler::EStates
CNCActiveHandler::x_GetState(void) const
{
    return EStates(m_State & ~eAllFlagsMask);
}

inline void
CNCActiveHandler::x_SetState(EStates new_state)
{
    m_State = (m_State & eAllFlagsMask) + new_state;
}

inline void
CNCActiveHandler::x_SetFlag(EFlags flag)
{
    m_State |= flag;
}

inline void
CNCActiveHandler::x_UnsetFlag(EFlags flag)
{
    m_State &= ~flag;
}

inline bool
CNCActiveHandler::x_IsFlagSet(EFlags flag) const
{
    _ASSERT(flag & eAllFlagsMask);

    return (m_State & flag) != 0;
}

inline void
CNCActiveHandler::x_DeferConnection(void)
{
    g_NetcacheServer->DeferConnectionProcessing((IServer_ConnectionBase*)m_SrvConn);
}

inline void
CNCActiveHandler::x_WaitForWouldBlock(void)
{
    x_SetFlag(fWaitForBlockedOp);
    x_DeferConnection();
}

inline void
CNCActiveHandler::InitDiagnostics(void)
{
    if (m_CmdCtx) {
        CDiagContext::SetRequestContext(m_CmdCtx);
    }
    else if (m_ConnCtx) {
        CDiagContext::SetRequestContext(m_ConnCtx);
    }
}

inline void
CNCActiveHandler::ResetDiagnostics(void)
{
    CDiagContext::SetRequestContext(NULL);
}

CNCActiveHandler::CNCActiveHandler(Uint8 srv_id, CNCPeerControl* peer)
    : m_SrvId(srv_id),
      m_Peer(peer),
      m_Client(NULL),
      m_SyncCtrl(NULL),
      m_CntCmds(0),
      m_BlobAccess(NULL),
      m_State(eConnIdle),
      m_ReservedForBG(false),
      m_InCmdPipeline(false),
      m_AddedToPool(false),
      m_GotAnyAnswer(false),
      m_NeedFlushBuff(false),
      m_WaitForThrottle(false),
      m_NeedDelete(false)
{}

CNCActiveHandler::~CNCActiveHandler(void)
{}

void
CNCActiveHandler::Initialize(void)
{
    s_PeerAuthString = kNCPeerClientName;
    s_PeerAuthString += " srv_id=";
    s_PeerAuthString += NStr::UInt8ToString(CNCDistributionConf::GetSelfID());
}

void
CNCActiveHandler::ClientReleased(void)
{
    m_ObjLock.Lock();
    m_CmdCtx.Reset();
    m_Client->SetHandler(NULL);
    m_Client = NULL;
    if (m_NeedDelete) {
        m_ObjLock.Unlock();
        delete this;
    }
    else {
        m_ObjLock.Unlock();
    }
}

void
CNCActiveHandler::x_MayDeleteThis(void)
{
    if (m_Client) {
        m_NeedDelete = true;
        m_ObjLock.Unlock();
    }
    else {
        m_ObjLock.Unlock();
        delete this;
    }
}

void
CNCActiveHandler::SetServerConn(CServer_Connection*     srv_conn,
                                CNCActiveHandler_Proxy* proxy)
{
    m_SrvConn = srv_conn;
    m_Proxy = proxy;
    m_AddedToPool = false;
    m_GotAnyAnswer = false;
    m_GotCmdAnswer = false;
    m_SockBuffer.ResetSocket(srv_conn, CNCDistributionConf::GetPeerTimeout());
    m_SockBuffer.WriteMessage("", s_PeerAuthString);
}

void
CNCActiveHandler::x_CloseConnection(void)
{
    m_SrvConn->Close();
    m_Proxy->SetHandler(NULL);
    OnClose(IServer_ConnectionHandler::eOurClose);
}

bool
CNCActiveHandler::x_ReplaceServerConn(void)
{
    m_Proxy->SetHandler(NULL);
    m_Proxy = NULL;

    CNCActiveHandler* src_handler = m_Peer->GetPooledConn();
    if (src_handler) {
        if (!src_handler->m_AddedToPool  ||  src_handler->x_GetState() != eConnIdle)
            abort();
        x_SetState(eConnIdle);
        m_AddedToPool = true;
        m_GotAnyAnswer = src_handler->m_GotAnyAnswer;
        m_Proxy = src_handler->m_Proxy;
        m_Proxy->SetHandler(this);
        m_SrvConn = src_handler->m_SrvConn;
        m_SockBuffer.ResetSocket(m_SrvConn, CNCDistributionConf::GetPeerTimeout());
        delete src_handler;
        return true;
    }
    else if (m_Peer->CreateNewSocket(this)) {
        x_SetState(eConnIdle);
        return true;
    }
    return false;
}

void
CNCActiveHandler::SearchMeta(CRequestContext* cmd_ctx, const string& raw_key)
{
    m_CmdCtx = cmd_ctx;
    m_CurCmd = eSearchMeta;

    string cache_name, key, subkey;
    g_NCStorage->UnpackBlobKey(raw_key, cache_name, key, subkey);

    m_CmdToSend.resize(0);
    m_CmdToSend += "PROXY_META \"";
    m_CmdToSend += cache_name;
    m_CmdToSend += "\" \"";
    m_CmdToSend += key;
    m_CmdToSend += "\" \"";
    m_CmdToSend += subkey;
    m_CmdToSend += "\" \"";
    m_CmdToSend += cmd_ctx->GetClientIP();
    m_CmdToSend += "\" \"";
    m_CmdToSend += cmd_ctx->GetSessionID();
    m_CmdToSend.append(1, '"');

    x_SendCmdToExecute();
}

void
CNCActiveHandler::CopyPut(CRequestContext* cmd_ctx,
                          const string& key,
                          Uint2 slot,
                          Uint8 orig_rec_no)
{
    m_CmdCtx = cmd_ctx;
    m_BlobKey = key;
    CNCDistributionConf::GetSlotByKey(key, m_BlobSlot, m_TimeBucket);
    if (m_BlobSlot != slot)
        abort();
    m_OrigRecNo = orig_rec_no;
    if (m_Client)
        m_Client->SetStatus(eNCHubCmdInProgress);
    x_DoCopyPut();
}

void
CNCActiveHandler::ProxyRemove(CRequestContext* cmd_ctx,
                              const string& raw_key,
                              const string& password,
                              int version,
                              Uint1 quorum)
{
    m_CmdCtx = cmd_ctx;
    m_CurCmd = eNeedOnlyConfirm;

    string cache_name, key, subkey;
    g_NCStorage->UnpackBlobKey(raw_key, cache_name, key, subkey);

    m_CmdToSend.resize(0);
    m_CmdToSend += "PROXY_RMV \"";
    m_CmdToSend += cache_name;
    m_CmdToSend += "\" \"";
    m_CmdToSend += key;
    m_CmdToSend += "\" \"";
    m_CmdToSend += subkey;
    m_CmdToSend += "\" ";
    m_CmdToSend += NStr::IntToString(version);
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UIntToString(quorum);
    m_CmdToSend += " \"";
    m_CmdToSend += cmd_ctx->GetClientIP();
    m_CmdToSend += "\" \"";
    m_CmdToSend += cmd_ctx->GetSessionID();
    m_CmdToSend.append(1, '"');
    if (!password.empty()) {
        m_CmdToSend += " \"";
        m_CmdToSend += password;
        m_CmdToSend.append(1, '"');
    }

    x_SendCmdToExecute();
}

void
CNCActiveHandler::ProxyHasBlob(CRequestContext* cmd_ctx,
                               const string& raw_key,
                               const string& password,
                               Uint1 quorum)
{
    m_CmdCtx = cmd_ctx;
    m_CurCmd = eNeedOnlyConfirm;

    string cache_name, key, subkey;
    g_NCStorage->UnpackBlobKey(raw_key, cache_name, key, subkey);

    m_CmdToSend.resize(0);
    m_CmdToSend += "PROXY_HASB \"";
    m_CmdToSend += cache_name;
    m_CmdToSend += "\" \"";
    m_CmdToSend += key;
    m_CmdToSend += "\" \"";
    m_CmdToSend += subkey;
    m_CmdToSend += "\" ";
    m_CmdToSend += NStr::UIntToString(quorum);
    m_CmdToSend += " \"";
    m_CmdToSend += cmd_ctx->GetClientIP();
    m_CmdToSend += "\" \"";
    m_CmdToSend += cmd_ctx->GetSessionID();
    m_CmdToSend.append(1, '"');
    if (!password.empty()) {
        m_CmdToSend += " \"";
        m_CmdToSend += password;
        m_CmdToSend.append(1, '"');
    }

    x_SendCmdToExecute();
}

void
CNCActiveHandler::ProxyGetSize(CRequestContext* cmd_ctx,
                               const string& raw_key,
                               const string& password,
                               int version,
                               Uint1 quorum,
                               bool search,
                               bool force_local)
{
    m_CmdCtx = cmd_ctx;
    m_CurCmd = eNeedOnlyConfirm;

    string cache_name, key, subkey;
    g_NCStorage->UnpackBlobKey(raw_key, cache_name, key, subkey);

    m_CmdToSend.resize(0);
    m_CmdToSend += "PROXY_GSIZ \"";
    m_CmdToSend += cache_name;
    m_CmdToSend += "\" \"";
    m_CmdToSend += key;
    m_CmdToSend += "\" \"";
    m_CmdToSend += subkey;
    m_CmdToSend += "\" ";
    m_CmdToSend += NStr::IntToString(version);
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UIntToString(quorum);
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UIntToString(Uint1(search));
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UIntToString(Uint1(force_local));
    m_CmdToSend += " \"";
    m_CmdToSend += cmd_ctx->GetClientIP();
    m_CmdToSend += "\" \"";
    m_CmdToSend += cmd_ctx->GetSessionID();
    m_CmdToSend.append(1, '"');
    if (!password.empty()) {
        m_CmdToSend += " \"";
        m_CmdToSend += password;
        m_CmdToSend.append(1, '"');
    }

    x_SendCmdToExecute();
}

void
CNCActiveHandler::ProxySetValid(CRequestContext* cmd_ctx,
                                const string& raw_key,
                                const string& password,
                                int version)
{
    m_CmdCtx = cmd_ctx;
    m_CurCmd = eNeedOnlyConfirm;

    string cache_name, key, subkey;
    g_NCStorage->UnpackBlobKey(raw_key, cache_name, key, subkey);

    m_CmdToSend.resize(0);
    m_CmdToSend += "PROXY_SETVALID \"";
    m_CmdToSend += cache_name;
    m_CmdToSend += "\" \"";
    m_CmdToSend += key;
    m_CmdToSend += "\" \"";
    m_CmdToSend += subkey;
    m_CmdToSend += "\" ";
    m_CmdToSend += NStr::IntToString(version);
    m_CmdToSend += " \"";
    m_CmdToSend += cmd_ctx->GetClientIP();
    m_CmdToSend += "\" \"";
    m_CmdToSend += cmd_ctx->GetSessionID();
    m_CmdToSend.append(1, '"');
    if (!password.empty()) {
        m_CmdToSend += " \"";
        m_CmdToSend += password;
        m_CmdToSend.append(1, '"');
    }

    x_SendCmdToExecute();
}

void
CNCActiveHandler::ProxyRead(CRequestContext* cmd_ctx,
                            const string& raw_key,
                            const string& password,
                            int version,
                            Uint8 start_pos,
                            Uint8 size,
                            Uint1 quorum,
                            bool search,
                            bool force_local)
{
    m_CmdCtx = cmd_ctx;
    m_CurCmd = eReadData;

    string cache_name, key, subkey;
    g_NCStorage->UnpackBlobKey(raw_key, cache_name, key, subkey);

    m_CmdToSend.resize(0);
    m_CmdToSend += "PROXY_GET \"";
    m_CmdToSend += cache_name;
    m_CmdToSend += "\" \"";
    m_CmdToSend += key;
    m_CmdToSend += "\" \"";
    m_CmdToSend += subkey;
    m_CmdToSend += "\" ";
    m_CmdToSend += NStr::IntToString(version);
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UInt8ToString(start_pos);
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::Int8ToString(Int8(size));
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UIntToString(quorum);
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UIntToString(Uint1(search));
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UIntToString(Uint1(force_local));
    m_CmdToSend += " \"";
    m_CmdToSend += cmd_ctx->GetClientIP();
    m_CmdToSend += "\" \"";
    m_CmdToSend += cmd_ctx->GetSessionID();
    m_CmdToSend.append(1, '"');
    if (!password.empty()) {
        m_CmdToSend += " \"";
        m_CmdToSend += password;
        m_CmdToSend.append(1, '"');
    }

    x_SendCmdToExecute();
}

void
CNCActiveHandler::ProxyReadLast(CRequestContext* cmd_ctx,
                                const string& raw_key,
                                const string& password,
                                Uint8 start_pos,
                                Uint8 size,
                                Uint1 quorum,
                                bool search,
                                bool force_local)
{
    m_CmdCtx = cmd_ctx;
    m_CurCmd = eReadData;

    string cache_name, key, subkey;
    g_NCStorage->UnpackBlobKey(raw_key, cache_name, key, subkey);

    m_CmdToSend.resize(0);
    m_CmdToSend += "PROXY_READLAST \"";
    m_CmdToSend += cache_name;
    m_CmdToSend += "\" \"";
    m_CmdToSend += key;
    m_CmdToSend += "\" \"";
    m_CmdToSend += subkey;
    m_CmdToSend += "\" ";
    m_CmdToSend += NStr::UInt8ToString(start_pos);
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::Int8ToString(Int8(size));
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UIntToString(quorum);
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UIntToString(Uint1(search));
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UIntToString(Uint1(force_local));
    m_CmdToSend += " \"";
    m_CmdToSend += cmd_ctx->GetClientIP();
    m_CmdToSend += "\" \"";
    m_CmdToSend += cmd_ctx->GetSessionID();
    m_CmdToSend.append(1, '"');
    if (!password.empty()) {
        m_CmdToSend += " \"";
        m_CmdToSend += password;
        m_CmdToSend.append(1, '"');
    }

    x_SendCmdToExecute();
}

void
CNCActiveHandler::ProxyGetMeta(CRequestContext* cmd_ctx,
                               const string& raw_key,
                               Uint1 quorum,
                               bool force_local)
{
    m_CmdCtx = cmd_ctx;
    m_CurCmd = eReadData;

    string cache_name, key, subkey;
    g_NCStorage->UnpackBlobKey(raw_key, cache_name, key, subkey);

    m_CmdToSend.resize(0);
    m_CmdToSend += "PROXY_GETMETA \"";
    m_CmdToSend += cache_name;
    m_CmdToSend += "\" \"";
    m_CmdToSend += key;
    m_CmdToSend += "\" \"";
    m_CmdToSend += subkey;
    m_CmdToSend += "\" ";
    m_CmdToSend += NStr::UIntToString(quorum);
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UIntToString(Uint1(force_local));
    m_CmdToSend += " \"";
    m_CmdToSend += cmd_ctx->GetClientIP();
    m_CmdToSend += "\" \"";
    m_CmdToSend += cmd_ctx->GetSessionID();
    m_CmdToSend.append(1, '"');

    x_SendCmdToExecute();
}

void
CNCActiveHandler::ProxyWrite(CRequestContext* cmd_ctx,
                             const string& raw_key,
                             const string& password,
                             int version,
                             Uint4 ttl,
                             Uint1 quorum)
{
    m_CmdCtx = cmd_ctx;
    m_CurCmd = eWriteData;

    string cache_name, key, subkey;
    g_NCStorage->UnpackBlobKey(raw_key, cache_name, key, subkey);

    m_CmdToSend.resize(0);
    m_CmdToSend += "PROXY_PUT \"";
    m_CmdToSend += cache_name;
    m_CmdToSend += "\" \"";
    m_CmdToSend += key;
    m_CmdToSend += "\" \"";
    m_CmdToSend += subkey;
    m_CmdToSend += "\" ";
    m_CmdToSend += NStr::IntToString(version);
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UIntToString(ttl);
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UIntToString(quorum);
    m_CmdToSend += " \"";
    m_CmdToSend += cmd_ctx->GetClientIP();
    m_CmdToSend += "\" \"";
    m_CmdToSend += cmd_ctx->GetSessionID();
    m_CmdToSend.append(1, '"');
    if (!password.empty()) {
        m_CmdToSend += " \"";
        m_CmdToSend += password;
        m_CmdToSend.append(1, '"');
    }

    x_SendCmdToExecute();
}

void
CNCActiveHandler::CopyProlong(const string& key,
                              Uint2 slot,
                              Uint8 orig_rec_no,
                              Uint8 orig_time,
                              const SNCBlobSummary& blob_sum)
{
    m_BlobKey = key;
    CNCDistributionConf::GetSlotByKey(key, m_BlobSlot, m_TimeBucket);
    if (m_BlobSlot != slot)
        abort();
    m_OrigTime = orig_time;
    m_OrigRecNo = orig_rec_no;
    m_OrigServer = CNCDistributionConf::GetSelfID();
    x_SendCopyProlongCmd(blob_sum);
}

void
CNCActiveHandler::SyncStart(CNCActiveSyncControl* ctrl,
                            Uint8 local_rec_no,
                            Uint8 remote_rec_no)
{
    m_SyncAction = eSynActionNone;
    m_SyncCtrl = ctrl;
    m_CmdCtx = ctrl->GetDiagCtx();
    m_CurCmd = eSyncStart;

    m_CmdToSend.resize(0);
    m_CmdToSend += "SYNC_START ";
    m_CmdToSend += NStr::UInt8ToString(CNCDistributionConf::GetSelfID());
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UIntToString(ctrl->GetSyncSlot());
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UInt8ToString(local_rec_no);
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UInt8ToString(remote_rec_no);

    x_SendCmdToExecute();
}

void
CNCActiveHandler::SyncBlobsList(CNCActiveSyncControl* ctrl)
{
    m_SyncAction = eSynActionNone;
    m_SyncCtrl = ctrl;
    m_CmdCtx = ctrl->GetDiagCtx();
    m_CurCmd = eSyncBList;

    m_CmdToSend.resize(0);
    m_CmdToSend += "SYNC_BLIST ";
    m_CmdToSend += NStr::UInt8ToString(CNCDistributionConf::GetSelfID());
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UIntToString(ctrl->GetSyncSlot());

    x_SendCmdToExecute();
}

void
CNCActiveHandler::SyncSend(CNCActiveSyncControl* ctrl, SNCSyncEvent* event)
{
    m_SyncAction = eSynActionWrite;
    m_SyncCtrl = ctrl;
    m_CmdCtx = ctrl->GetDiagCtx();
    m_BlobKey = event->key;
    CNCDistributionConf::GetSlotByKey(m_BlobKey, m_BlobSlot, m_TimeBucket);
    if (m_BlobSlot != ctrl->GetSyncSlot())
        abort();
    m_OrigRecNo = event->orig_rec_no;
    x_DoCopyPut();
}

void
CNCActiveHandler::SyncSend(CNCActiveSyncControl* ctrl, const string& key)
{
    m_SyncAction = eSynActionWrite;
    m_SyncCtrl = ctrl;
    m_CmdCtx = ctrl->GetDiagCtx();
    m_BlobKey = key;
    CNCDistributionConf::GetSlotByKey(key, m_BlobSlot, m_TimeBucket);
    if (m_BlobSlot != ctrl->GetSyncSlot())
        abort();
    m_OrigRecNo = 0;
    x_DoCopyPut();
}

void
CNCActiveHandler::SyncRead(CNCActiveSyncControl* ctrl, SNCSyncEvent* event)
{
    m_SyncCtrl = ctrl;
    m_BlobKey = event->key;
    m_OrigTime = event->orig_time;
    m_OrigRecNo = event->orig_rec_no;
    m_OrigServer = event->orig_server;
    x_DoSyncGet();
}

void
CNCActiveHandler::SyncRead(CNCActiveSyncControl* ctrl,
                           const string& key,
                           Uint8 create_time)
{
    m_SyncCtrl = ctrl;
    m_BlobKey = key;
    m_OrigTime = create_time;
    m_OrigRecNo = 0;
    x_DoSyncGet();
}

void
CNCActiveHandler::SyncProlongPeer(CNCActiveSyncControl* ctrl,
                                  SNCSyncEvent* event)
{
    m_SyncCtrl = ctrl;
    m_CmdCtx = ctrl->GetDiagCtx();
    m_SyncAction = eSynActionProlong;
    m_BlobKey = event->key;
    m_OrigTime = event->orig_time;
    m_OrigRecNo = event->orig_rec_no;
    m_OrigServer = event->orig_server;
    CNCDistributionConf::GetSlotByKey(m_BlobKey, m_BlobSlot, m_TimeBucket);
    if (m_BlobSlot != ctrl->GetSyncSlot())
        abort();
    m_CurCmd = eSyncProlongPeer;
    m_BlobAccess = g_NCStorage->GetBlobAccess(eNCRead, m_BlobKey,
                                              kEmptyStr, m_TimeBucket);

    x_SetStateAndAddToPool(eWaitForMetaInfo);
}

void
CNCActiveHandler::SyncProlongPeer(CNCActiveSyncControl* ctrl,
                                  const string& key,
                                  const SNCBlobSummary& blob_sum)
{
    m_SyncCtrl = ctrl;
    m_CmdCtx = ctrl->GetDiagCtx();
    m_SyncAction = eSynActionProlong;
    m_BlobKey = key;
    CNCDistributionConf::GetSlotByKey(key, m_BlobSlot, m_TimeBucket);
    if (m_BlobSlot != ctrl->GetSyncSlot())
        abort();
    m_OrigTime = 0;
    m_OrigRecNo = 0;
    m_OrigServer = 0;
    x_SendCopyProlongCmd(blob_sum);
}

void
CNCActiveHandler::SyncProlongOur(CNCActiveSyncControl* ctrl,
                                 SNCSyncEvent* event)
{
    m_SyncCtrl = ctrl;
    m_CmdCtx = ctrl->GetDiagCtx();
    m_SyncAction = eSynActionProlong;
    m_BlobKey = event->key;
    CNCDistributionConf::GetSlotByKey(m_BlobKey, m_BlobSlot, m_TimeBucket);
    if (m_BlobSlot != ctrl->GetSyncSlot())
        abort();
    m_OrigTime = event->orig_time;
    m_OrigRecNo = event->orig_rec_no;
    m_OrigServer = event->orig_server;
    m_CurCmd = eSyncProInfo;

    string cache_name, key, subkey;
    g_NCStorage->UnpackBlobKey(m_BlobKey, cache_name, key, subkey);

    m_CmdToSend.resize(0);
    m_CmdToSend += "SYNC_PROINFO ";
    m_CmdToSend += NStr::UInt8ToString(CNCDistributionConf::GetSelfID());
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UIntToString(m_BlobSlot);
    m_CmdToSend += " \"";
    m_CmdToSend += cache_name;
    m_CmdToSend += "\" \"";
    m_CmdToSend += key;
    m_CmdToSend += "\" \"";
    m_CmdToSend += subkey;
    m_CmdToSend += "\"";

    x_SendCmdToExecute();
}

void
CNCActiveHandler::SyncProlongOur(CNCActiveSyncControl* ctrl,
                                 const string& key,
                                 const SNCBlobSummary& blob_sum)
{
    m_SyncCtrl = ctrl;
    m_CmdCtx = ctrl->GetDiagCtx();
    m_SyncAction = eSynActionProlong;
    m_BlobKey = key;
    CNCDistributionConf::GetSlotByKey(key, m_BlobSlot, m_TimeBucket);
    if (m_BlobSlot != ctrl->GetSyncSlot())
        abort();
    m_OrigTime = 0;
    m_OrigRecNo = 0;
    m_OrigServer = 0;
    m_BlobSum = blob_sum;
    m_CurCmd = eSyncProInfo;

    x_DoProlongOur();
}

void
CNCActiveHandler::SyncCancel(CNCActiveSyncControl* ctrl)
{
    m_SyncCtrl = ctrl;
    m_CmdCtx = ctrl->GetDiagCtx();
    m_SyncAction = eSynActionNone;
    m_CurCmd = eNeedOnlyConfirm;

    m_CmdToSend.resize(0);
    m_CmdToSend += "SYNC_CANCEL ";
    m_CmdToSend += NStr::UInt8ToString(CNCDistributionConf::GetSelfID());
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UIntToString(ctrl->GetSyncSlot());

    x_SendCmdToExecute();
}

void
CNCActiveHandler::SyncCommit(CNCActiveSyncControl* ctrl,
                             Uint8 local_rec_no,
                             Uint8 remote_rec_no)
{
    m_SyncCtrl = ctrl;
    m_CmdCtx = ctrl->GetDiagCtx();
    m_SyncAction = eSynActionNone;
    m_CurCmd = eNeedOnlyConfirm;

    m_CmdToSend.resize(0);
    m_CmdToSend += "SYNC_COMMIT ";
    m_CmdToSend += NStr::UInt8ToString(CNCDistributionConf::GetSelfID());
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UIntToString(ctrl->GetSyncSlot());
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UInt8ToString(local_rec_no);
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UInt8ToString(remote_rec_no);

    x_SendCmdToExecute();
}

void
CNCActiveHandler::OnTimeout(void)
{
    InitDiagnostics();
    ERR_POST("Peer doesn't respond for too long");
    m_ErrMsg = "ERR:Peer doesn't respond";
    ResetDiagnostics();
}

void
CNCActiveHandler::OnClose(IServer_ConnectionHandler::EClosePeer peer)
{
    EStates was_state = x_GetState();
    if (was_state == eConnClosed)
        return;

    x_SetState(eConnClosed);
    if (m_DidFirstWrite  &&  !m_GotAnyAnswer) {
        m_Peer->RegisterConnError();
    }
    else if (!m_GotCmdAnswer  &&  !g_NetcacheServer->ShutdownRequested()
             &&  x_ReplaceServerConn())
    {
        if (was_state == eWaitForMetaInfo)
            x_SetStateAndAddToPool(eWaitForMetaInfo);
        else
            x_SendCmdToExecute();
        return;
    }
    if (peer == IServer_ConnectionHandler::eClientClose) {
        m_ErrMsg = "ERR:Connection closed by peer";
    }
    x_FinishCommand(false);

    if (was_state != eConnIdle)
        m_Peer->ReleaseConn(this);
    if (!m_InCmdPipeline) {
        m_ObjLock.Lock();
        x_MayDeleteThis();
    }
}

void
CNCActiveHandler::OnRead(void)
{
    x_ManageCmdPipeline();
}

void
CNCActiveHandler::OnWrite(void)
{
    x_ManageCmdPipeline();
}

EIO_Event
CNCActiveHandler::GetEventsToPollFor(const CTime** alarm_time)
{
    if (!m_DidFirstWrite)
        return eIO_Write;

    EStates state = x_GetState();
    switch (state) {
    case eConnClosed:
        abort();
    case eWaitOneLineAnswer:
    case eReadDataForClient:
    case eReadEventsList:
    case eReadBlobsList:
    case eReadBlobData:
        if (!m_SockBuffer.IsWriteDataPending())
            return eIO_Read;
        // fall through
    default:
        return eIO_Write;
    }
}

bool
CNCActiveHandler::IsReadyToProcess(void)
{
    EStates state = x_GetState();
    if (state == eConnIdle)
        return false;
    else if (state == eWaitClientRelease  ||  state == eConnClosed)
        return !m_Client;
    else if (m_WaitForThrottle)
        return CNetCacheServer::GetPreciseTime() >= m_ThrottleTime;
    else if (x_IsFlagSet(fWaitForClient)) {
        CMutexGuard guard(m_ObjLock);
        if (!m_Client)
            return true;
        if (x_GetState() == eReadDataForClient)
            return m_Client->GetClient()->IsBufferFlushed();
        else if (x_GetState() == eWriteDataForClient)
            return m_NeedFlushBuff;
        else if (x_IsFlagSet(fWaitForClient))
            abort();
    }
    else if (x_IsFlagSet(fWaitForBlockedOp))
        return false;

    return true;
}

void
CNCActiveHandler::OnBlockedOpFinish(void)
{
    m_ObjLock.Lock();
    x_UnsetFlag(fWaitForBlockedOp);
    m_ObjLock.Unlock();

    g_NetcacheServer->WakeUpPollCycle();
}

CBufferedSockReaderWriter&
CNCActiveHandler::GetSockBuffer(void)
{
    // This guard is to serialize access to SockBuffer from MessageHandler
    // with possibly parallel access from ManageCmdPipeline.
    CMutexGuard guard(m_ObjLock);
    return m_SockBuffer;
}

void
CNCActiveHandler::ForceBufferFlush(void)
{
    m_NeedFlushBuff = true;
}

bool
CNCActiveHandler::IsBufferFlushed(void)
{
    return !m_NeedFlushBuff;
}

void
CNCActiveHandler::FinishBlobFromClient(void)
{
    m_ObjLock.Lock();
    x_FinishWritingBlob();
    x_UnsetFlag(fWaitForClient);
    m_ObjLock.Unlock();

    g_NetcacheServer->WakeUpPollCycle();
}

void
CNCActiveHandler::x_DoCopyPut(void)
{
    m_CurCmd = eCopyPut;
    m_BlobAccess = g_NCStorage->GetBlobAccess(eNCReadData, m_BlobKey,
                                              kEmptyStr, m_TimeBucket);
    x_SetStateAndAddToPool(eWaitForMetaInfo);
}

void
CNCActiveHandler::x_DoSyncGet(void)
{
    m_SyncAction = eSynActionRead;
    m_CmdCtx = m_SyncCtrl->GetDiagCtx();
    CNCDistributionConf::GetSlotByKey(m_BlobKey, m_BlobSlot, m_TimeBucket);
    if (m_BlobSlot != m_SyncCtrl->GetSyncSlot())
        abort();
    m_CurCmd = eSyncGet;
    m_BlobAccess = g_NCStorage->GetBlobAccess(eNCCopyCreate, m_BlobKey,
                                              kEmptyStr, m_TimeBucket);
    x_SetStateAndAddToPool(eWaitForMetaInfo);
}

void
CNCActiveHandler::x_AddConnToPool(void)
{
    try {
        m_SockBuffer.SetTimeout(CNCDistributionConf::GetPeerConnTimeout());
        m_DidFirstWrite = false;
        m_AddedToPool = true;
        g_NetcacheServer->AddConnectionToPool(m_SrvConn);
    }
    catch (CServer_Exception& ex) {
        ERR_POST(Critical << "Too many open connections: " << ex);
        m_AddedToPool = false;
        m_SrvConn->Abort();
        m_ErrMsg = "ERR:Too many open connections";
        x_SetState(eConnClosed);
        x_FinishCommand(false);
        if (m_Client)
            ClientReleased();
        m_Peer->ReleaseConn(this);
        if (!m_InCmdPipeline) {
            m_ObjLock.Lock();
            x_MayDeleteThis();
        }
    }
}

void
CNCActiveHandler::x_SetStateAndAddToPool(EStates state)
{
    if (m_AddedToPool) {
        x_SetState(state);
        // We really shouldn't read anything from the object at this point.
        // Thus x_SetState() above cannot be before if.
    }
    else {
        x_SetState(state);
        x_AddConnToPool();
    }
}

void
CNCActiveHandler::x_SendCmdToExecute(void)
{
    if (m_Client)
        m_Client->SetStatus(eNCHubCmdInProgress);

    m_SockBuffer.WriteMessage("", m_CmdToSend);
    m_GotCmdAnswer = false;
    x_SetStateAndAddToPool(eWaitOneLineAnswer);
    g_NetcacheServer->WakeUpPollCycle();
}

inline void
CNCActiveHandler::x_FinishSyncCmd(ESyncResult result)
{
    if (m_SyncCtrl) {
        m_SyncCtrl->CmdFinished(result, m_SyncAction, this);
        m_SyncCtrl = NULL;
    }
}

void
CNCActiveHandler::x_FinishCommand(bool success)
{
    if (m_BlobAccess) {
        m_BlobAccess->Release();
        m_BlobAccess = NULL;
    }
    if (m_Client) {
        m_Client->SetErrMsg(m_ErrMsg);
        m_Client->SetStatus(success? eNCHubSuccess: eNCHubError);
        x_UnsetFlag(fWaitForClient);
        if (x_GetState() != eConnClosed)
            x_SetState(eWaitClientRelease);
    }
    else {
        if (!success)
            x_FinishSyncCmd(eSynNetworkError);
        else if (m_SyncCtrl)
            abort();
        if (x_GetState() != eConnClosed)
            x_SetState(eReadyForPool);
    }
}

void
CNCActiveHandler::x_ProcessPeerError(void)
{
    m_ErrMsg = m_Response;
    if (NStr::FindNoCase(m_Response, "BLOB not found") == NPOS) {
        LOG_POST(Warning << "Error from peer: " << m_ErrMsg);
    }
    x_FinishCommand(false);
}

void
CNCActiveHandler::x_ProcessProtocolError(void)
{
    ERR_POST(Critical << "Protocol error. Got response: '" << m_Response << "'");
    m_ErrMsg = "ERR:Protocol error";
    x_CloseConnection();
}

void
CNCActiveHandler::x_StartWritingBlob(void)
{
    Uint4 start_word = 0x01020304;
    m_SockBuffer.WriteNoFail(&start_word, sizeof(start_word));
    m_ChunkSize = 0;
}

void
CNCActiveHandler::x_FinishWritingBlob(void)
{
    Uint4 finish_word = 0xFFFFFFFF;
    m_SockBuffer.WriteNoFail(&finish_word, sizeof(finish_word));
    m_SockBuffer.Flush();
    // HasError() is processed in ManageCmdPipeline()
    m_CurCmd = eNeedOnlyConfirm;
    x_SetState(eWaitOneLineAnswer);
}

void
CNCActiveHandler::x_FakeWritingBlob(void)
{
    x_StartWritingBlob();
    x_FinishWritingBlob();
}

bool
CNCActiveHandler::x_ReadFoundMeta(void)
{
    if (NStr::FindNoCase(m_Response, "BLOB not found") != NPOS) {
        m_BlobExists = false;
    }
    else {
        list<CTempString> params;
        NStr::Split(m_Response, " ", params);
        if (params.size() < 5) {
            x_ProcessProtocolError();
            return true;
        }
        else {
            list<CTempString>::const_iterator param_it = params.begin();
            ++param_it;
            try {
                m_BlobSum.create_time = NStr::StringToUInt8(*param_it);
                ++param_it;
                m_BlobSum.create_server = NStr::StringToUInt8(*param_it);
                ++param_it;
                m_BlobSum.create_id = NStr::StringToUInt(*param_it);
                ++param_it;
                m_BlobSum.dead_time = NStr::StringToInt(*param_it);
                ++param_it;
                m_BlobSum.expire = NStr::StringToInt(*param_it);
                ++param_it;
                m_BlobSum.ver_expire = NStr::StringToInt(*param_it);
                m_BlobExists = true;
            }
            catch (CStringException&) {
                x_ProcessProtocolError();
                return true;
            }
        }
    }
    x_FinishCommand(true);
    return true;
}

bool
CNCActiveHandler::x_SendCopyPutCmd(void)
{
    if (m_BlobAccess->HasError()) {
        x_FinishCommand(false);
        return true;
    }
    if (!m_BlobAccess->IsBlobExists()
        ||  m_BlobAccess->GetCurBlobDeadTime() < int(time(NULL)))
    {
        x_FinishSyncCmd(eSynOK);
        x_FinishCommand(true);
        return true;
    }

    string cache_name, key, subkey;
    g_NCStorage->UnpackBlobKey(m_BlobKey, cache_name, key, subkey);

    m_CmdToSend.resize(0);
    if (m_SyncCtrl) {
        m_CmdToSend += "SYNC_PUT ";
        m_CmdToSend += NStr::UInt8ToString(CNCDistributionConf::GetSelfID());
        m_CmdToSend.append(1, ' ');
        m_CmdToSend += NStr::UIntToString(m_BlobSlot);
    }
    else {
        m_CmdToSend += "COPY_PUT";
    }
    m_CmdToSend += " \"";
    m_CmdToSend += cache_name;
    m_CmdToSend += "\" \"";
    m_CmdToSend += key;
    m_CmdToSend += "\" \"";
    m_CmdToSend += subkey;
    m_CmdToSend += "\" ";
    m_CmdToSend += NStr::IntToString(m_BlobAccess->GetCurBlobVersion());
    m_CmdToSend += " \"";
    m_CmdToSend += m_BlobAccess->GetCurPassword();
    m_CmdToSend += "\" ";
    m_CmdToSend += NStr::UInt8ToString(m_BlobAccess->GetCurBlobCreateTime());
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UIntToString(Uint4(m_BlobAccess->GetCurBlobTTL()));
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::IntToString(m_BlobAccess->GetCurBlobDeadTime());
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::IntToString(m_BlobAccess->GetCurBlobExpire());
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UInt8ToString(m_BlobAccess->GetCurBlobSize());
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UIntToString(Uint4(m_BlobAccess->GetCurVersionTTL()));
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::IntToString(m_BlobAccess->GetCurVerExpire());
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UInt8ToString(m_BlobAccess->GetCurCreateServer());
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UInt8ToString(m_BlobAccess->GetCurCreateId());
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UInt8ToString(m_OrigRecNo);
    m_CmdToSend.append(" 1 ");
    if (m_CmdCtx) {
        m_CmdToSend += " \"";
        m_CmdToSend += m_CmdCtx->GetClientIP();
        m_CmdToSend += "\" \"";
        m_CmdToSend += m_CmdCtx->GetSessionID();
        m_CmdToSend.append(1, '"');
    }

    x_SendCmdToExecute();
    return true;
}

bool
CNCActiveHandler::x_ReadCopyPut(void)
{
    if (NStr::FindNoCase(m_Response, "NEED_ABORT") != NPOS) {
        x_FinishSyncCmd(eSynAborted);
        if (NStr::FindNoCase(m_Response, "NEED_ABORT1") != NPOS)
            x_FinishCommand(true);
        else
            x_FakeWritingBlob();
    }
    else if (NStr::FindNoCase(m_Response, "HAVE_NEWER") != NPOS) {
        x_FinishSyncCmd(eSynOK);
        if (NStr::FindNoCase(m_Response, "HAVE_NEWER1") != NPOS)
            x_FinishCommand(true);
        else
            x_FakeWritingBlob();
    }
    else {
        m_BlobAccess->SetPosition(0);
        x_SetState(eWaitForFirstData);
    }
    return true;
}

bool
CNCActiveHandler::x_PrepareSyncProlongCmd(void)
{
    if (m_BlobAccess->HasError()) {
        x_FinishCommand(false);
        return true;
    }
    if (!m_BlobAccess->IsBlobExists()  ||  m_BlobAccess->IsCurBlobExpired()) {
        x_FinishSyncCmd(eSynOK);
        x_FinishCommand(true);
        return true;
    }

    SNCBlobSummary blob_sum;
    blob_sum.create_time = m_BlobAccess->GetCurBlobCreateTime();
    blob_sum.create_server = m_BlobAccess->GetCurCreateServer();
    blob_sum.create_id = m_BlobAccess->GetCurCreateId();
    blob_sum.dead_time = m_BlobAccess->GetCurBlobDeadTime();
    blob_sum.expire = m_BlobAccess->GetCurBlobExpire();
    blob_sum.ver_expire = m_BlobAccess->GetCurVerExpire();

    x_SendCopyProlongCmd(blob_sum);
    return true;
}

void
CNCActiveHandler::x_SendCopyProlongCmd(const SNCBlobSummary& blob_sum)
{
    m_CurCmd = eCopyProlong;

    string cache_name, key, subkey;
    g_NCStorage->UnpackBlobKey(m_BlobKey, cache_name, key, subkey);

    m_CmdToSend.resize(0);
    if (m_SyncCtrl) {
        m_CmdToSend += "SYNC_PROLONG ";
        m_CmdToSend += NStr::UInt8ToString(CNCDistributionConf::GetSelfID());
        m_CmdToSend.append(1, ' ');
        m_CmdToSend += NStr::UIntToString(m_BlobSlot);
    }
    else {
        m_CmdToSend += "COPY_PROLONG";
    }
    m_CmdToSend += " \"";
    m_CmdToSend += cache_name;
    m_CmdToSend += "\" \"";
    m_CmdToSend += key;
    m_CmdToSend += "\" \"";
    m_CmdToSend += subkey;
    m_CmdToSend += "\" ";
    m_CmdToSend += NStr::UInt8ToString(blob_sum.create_time);
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UInt8ToString(blob_sum.create_server);
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UInt8ToString(blob_sum.create_id);
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::IntToString(blob_sum.dead_time);
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::IntToString(blob_sum.expire);
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::IntToString(blob_sum.ver_expire);
    if (m_OrigRecNo != 0) {
        m_CmdToSend.append(1, ' ');
        m_CmdToSend += NStr::UInt8ToString(m_OrigTime);
        m_CmdToSend.append(1, ' ');
        m_CmdToSend += NStr::UInt8ToString(m_OrigServer);
        m_CmdToSend.append(1, ' ');
        m_CmdToSend += NStr::UInt8ToString(m_OrigRecNo);
    }

    x_SendCmdToExecute();
}

bool
CNCActiveHandler::x_ReadCopyProlong(void)
{
    if (NStr::FindNoCase(m_Response, "BLOB not found") != NPOS) {
        CopyPut(m_CmdCtx, m_BlobKey, m_BlobSlot, 0);
    }
    else if (NStr::FindNoCase(m_Response, "NEED_ABORT") != NPOS) {
        x_FinishSyncCmd(eSynAborted);
        x_FinishCommand(true);
    }
    else {
        x_FinishSyncCmd(eSynOK);
        x_FinishCommand(true);
    }
    return true;
}

bool
CNCActiveHandler::x_ReadConfirm(void)
{
    m_ErrMsg = m_Response;
    x_FinishSyncCmd(eSynOK);
    x_FinishCommand(true);
    return true;
}

bool
CNCActiveHandler::x_ReadSizeToRead(void)
{
    size_t pos = m_Response.find("SIZE=");
    if (pos == string::npos) {
        ERR_POST(Critical << "SIZE is not found in peer response.");
        x_ProcessProtocolError();
        return false;
    }
    pos += sizeof("SIZE=") - 1;
    try {
        m_SizeToRead = NStr::StringToUInt8(m_Response.data() + pos,
                                           NStr::fAllowTrailingSymbols);
    }
    catch (CStringException& ex) {
        ERR_POST(Critical << "Cannot parse data size: " << ex);
        x_ProcessProtocolError();
        return false;
    }
    return true;
}

bool
CNCActiveHandler::x_ReadDataPrefix(void)
{
    if (!x_ReadSizeToRead())
        return true;
    CNCMessageHandler* client = m_Client->GetClient();
    CBufferedSockReaderWriter& clnt_buf = client->GetSockBuffer();
    clnt_buf.WriteMessage("", m_Response);
    client->InitialAnswerWritten();
    x_SetState(eReadDataForClient);
    return true;
}

bool
CNCActiveHandler::x_ReadDataForClient(void)
{
    x_UnsetFlag(fWaitForClient);

    if (!m_Client) {
        x_CloseConnection();
        return true;
    }

    CNCMessageHandler* client = m_Client->GetClient();
    CBufferedSockReaderWriter& clnt_buf = client->GetSockBuffer();
    while (m_SizeToRead != 0) {
        size_t n_written = clnt_buf.WriteFromBuffer(m_SockBuffer, m_SizeToRead);
        m_SizeToRead -= n_written;
        if (m_SockBuffer.HasError()) {
            m_ErrMsg = "ERR:Error writing to peer";
            x_CloseConnection();
            return true;
        }
        if (clnt_buf.HasError()) {
            m_ErrMsg = "ERR:Error writing to blob data to client";
            x_CloseConnection();
            return true;
        }
        if (clnt_buf.IsWriteDataPending()  ||  m_SockBuffer.HasDataToRead()) {
            x_SetFlag(fWaitForClient);
            client->ForceBufferFlush();
            x_DeferConnection();
            return false;
        }
        if (n_written == 0)
            return false;
    }
    x_FinishCommand(true);
    return true;
}

bool
CNCActiveHandler::x_ReadWritePrefix(void)
{
    x_StartWritingBlob();
    m_Client->GetClient()->WriteInitWriteResponse(m_Response);
    x_SetState(eWriteDataForClient);
    return false;
}

bool
CNCActiveHandler::x_WriteDataForClient(void)
{
    if (!m_Client) {
        x_CloseConnection();
        return true;
    }

    x_SetFlag(fWaitForClient);
    x_DeferConnection();
    return false;
}

bool
CNCActiveHandler::x_ReadSyncStartAnswer(void)
{
    if (NStr::FindNoCase(m_Response, "CROSS_SYNC") != NPOS) {
        x_FinishSyncCmd(eSynCrossSynced);
        x_FinishCommand(true);
    }
    else if (NStr::FindNoCase(m_Response, "IN_PROGRESS") != NPOS) {
        x_FinishSyncCmd(eSynServerBusy);
        x_FinishCommand(true);
    }
    else if (NStr::FindNoCase(m_Response, "NEED_ABORT") != NPOS) {
        x_FinishSyncCmd(eSynAborted);
        x_FinishCommand(true);
    }
    else {
        if (!x_ReadSizeToRead())
            return true;
        list<CTempString> tokens;
        NStr::Split(m_Response, " ", tokens);
        if (tokens.size() != 2  &&  tokens.size() != 3) {
            x_ProcessProtocolError();
            return true;
        }
        list<CTempString>::const_iterator it_tok = tokens.begin();
        Uint8 local_rec_no = 0, remote_rec_no = 0;
        try {
            ++it_tok;
            remote_rec_no = NStr::StringToUInt8(*it_tok);
            ++it_tok;
            if (it_tok != tokens.end())
                local_rec_no = NStr::StringToUInt8(*it_tok);
        }
        catch (CStringException&) {
            x_ProcessProtocolError();
            return true;
        }
        bool by_blobs = m_CurCmd  == eSyncBList
                        ||  NStr::FindNoCase(m_Response, "ALL_BLOBS") != NPOS;
        m_SyncCtrl->StartResponse(local_rec_no, remote_rec_no, by_blobs);
        x_SetState(by_blobs? eReadBlobsList: eReadEventsList);
    }
    return true;
}

bool
CNCActiveHandler::x_ReadEventsList(void)
{
    while (m_SizeToRead != 0) {
        if (m_SockBuffer.GetReadReadySize() < 2) {
            if (!m_SockBuffer.ReadToBuf()
                ||  m_SockBuffer.GetReadReadySize() < 2)
            {
                return false;
            }
        }

        Uint2 key_size = *(const Uint2*)m_SockBuffer.GetReadBufData();
        SNCSyncEvent* evt;
        Uint2 rec_size = sizeof(key_size) + key_size + 1
                         + sizeof(evt->rec_no)
                         + sizeof(evt->local_time)
                         + sizeof(evt->orig_rec_no)
                         + sizeof(evt->orig_server)
                         + sizeof(evt->orig_time);
        if (m_SockBuffer.GetReadReadySize() < rec_size) {
            if (!m_SockBuffer.ReadToBuf()
                ||  m_SockBuffer.GetReadReadySize() < rec_size)
            {
                return false;
            }
        }

        evt = new SNCSyncEvent;
        const char* data = (const char*)m_SockBuffer.GetReadBufData()
                                        + sizeof(key_size);
        evt->key.assign(data, key_size);
        data += key_size;
        evt->event_type = ENCSyncEvent(*data);
        ++data;
        memcpy(&evt->rec_no, data, sizeof(evt->rec_no));
        data += sizeof(evt->rec_no);
        memcpy(&evt->local_time, data, sizeof(evt->local_time));
        data += sizeof(evt->local_time);
        memcpy(&evt->orig_rec_no, data, sizeof(evt->orig_rec_no));
        data += sizeof(evt->orig_rec_no);
        memcpy(&evt->orig_server, data, sizeof(evt->orig_server));
        data += sizeof(evt->orig_server);
        memcpy(&evt->orig_time, data, sizeof(evt->orig_time));
        m_SockBuffer.EraseReadBufData(rec_size);

        m_SizeToRead -= rec_size;
        m_SyncCtrl->AddStartEvent(evt);
    }

    x_FinishSyncCmd(eSynOK);
    x_FinishCommand(true);
    return true;
}

bool
CNCActiveHandler::x_ReadBlobsList(void)
{
    while (m_SizeToRead != 0) {
        if (m_SockBuffer.GetReadReadySize() < 2) {
            if (!m_SockBuffer.ReadToBuf()
                ||  m_SockBuffer.GetReadReadySize() < 2)
            {
                return false;
            }
        }

        Uint2 key_size = *(const Uint2*)m_SockBuffer.GetReadBufData();
        SNCCacheData* blob_sum;
        Uint2 rec_size = key_size + sizeof(key_size)
                         + sizeof(blob_sum->create_time)
                         + sizeof(blob_sum->create_server)
                         + sizeof(blob_sum->create_id)
                         + sizeof(blob_sum->dead_time)
                         + sizeof(blob_sum->expire)
                         + sizeof(blob_sum->ver_expire);
        if (m_SockBuffer.GetReadReadySize() < rec_size) {
            if (!m_SockBuffer.ReadToBuf()
                ||  m_SockBuffer.GetReadReadySize() < rec_size)
            {
                return false;
            }
        }

        blob_sum = new SNCCacheData();
        const char* data = (const char*)m_SockBuffer.GetReadBufData()
                                        + sizeof(key_size);
        string key(key_size, '\0');
        memcpy(&key[0], data, key_size);
        data += key_size;
        memcpy(&blob_sum->create_time, data, sizeof(blob_sum->create_time));
        data += sizeof(blob_sum->create_time);
        memcpy(&blob_sum->create_server, data, sizeof(blob_sum->create_server));
        data += sizeof(blob_sum->create_server);
        memcpy(&blob_sum->create_id, data, sizeof(blob_sum->create_id));
        data += sizeof(blob_sum->create_id);
        memcpy(&blob_sum->dead_time, data, sizeof(blob_sum->dead_time));
        data += sizeof(blob_sum->dead_time);
        memcpy(&blob_sum->expire, data, sizeof(blob_sum->expire));
        data += sizeof(blob_sum->expire);
        memcpy(&blob_sum->ver_expire, data, sizeof(blob_sum->ver_expire));
        m_SockBuffer.EraseReadBufData(rec_size);

        m_SizeToRead -= rec_size;
        m_SyncCtrl->AddStartBlob(key, blob_sum);
    }

    x_FinishSyncCmd(eSynOK);
    x_FinishCommand(true);
    return true;
}

bool
CNCActiveHandler::x_SendSyncGetCmd(void)
{
    if (m_BlobAccess->HasError()) {
        x_FinishCommand(false);
        return true;
    }
    if (m_BlobAccess->IsBlobExists()
        &&  m_BlobAccess->GetCurBlobCreateTime() > m_OrigTime)
    {
        x_FinishSyncCmd(eSynOK);
        x_FinishCommand(true);
        return true;
    }

    string cache_name, key, subkey;
    g_NCStorage->UnpackBlobKey(m_BlobKey, cache_name, key, subkey);

    m_CmdToSend.resize(0);
    m_CmdToSend += "SYNC_GET ";
    m_CmdToSend += NStr::UInt8ToString(CNCDistributionConf::GetSelfID());
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UIntToString(m_BlobSlot);
    m_CmdToSend += " \"";
    m_CmdToSend += cache_name;
    m_CmdToSend += "\" \"";
    m_CmdToSend += key;
    m_CmdToSend += "\" \"";
    m_CmdToSend += subkey;
    m_CmdToSend += "\" ";
    m_CmdToSend += NStr::UInt8ToString(m_OrigTime);
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UInt8ToString(m_BlobAccess->GetCurBlobCreateTime());
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UInt8ToString(m_BlobAccess->GetCurCreateServer());
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::Int8ToString(m_BlobAccess->GetCurCreateId());

    x_SendCmdToExecute();
    return true;
}

bool
CNCActiveHandler::x_ReadSyncGetAnswer(void)
{
    if (NStr::FindNoCase(m_Response, "BLOB not found") != NPOS) {
        x_FinishSyncCmd(eSynOK);
        x_FinishCommand(true);
    }
    else if (NStr::FindNoCase(m_Response, "NEED_ABORT") != NPOS) {
        x_FinishSyncCmd(eSynAborted);
        x_FinishCommand(true);
    }
    else if (NStr::FindNoCase(m_Response, "HAVE_NEWER") != NPOS) {
        x_FinishSyncCmd(eSynOK);
        x_FinishCommand(true);
    }
    else {
        if (!x_ReadSizeToRead())
            return true;
        list<CTempString> tokens;
        NStr::Split(m_Response, " ", tokens);
        if (tokens.size() != 11) {
            x_ProcessProtocolError();
            return true;
        }
        list<CTempString>::const_iterator it_tok = tokens.begin();
        try {
            ++it_tok;
            m_BlobAccess->SetBlobVersion(NStr::StringToInt(*it_tok));
            ++it_tok;
            m_BlobAccess->SetPassword(it_tok->substr(1, it_tok->size() - 2));
            ++it_tok;
            Uint8 create_time = NStr::StringToUInt8(*it_tok);
            m_BlobAccess->SetBlobCreateTime(create_time);
            ++it_tok;
            m_BlobAccess->SetBlobTTL(int(NStr::StringToUInt(*it_tok)));
            ++it_tok;
            int dead_time = NStr::StringToInt(*it_tok);
            ++it_tok;
            int expire = NStr::StringToInt(*it_tok);
            m_BlobAccess->SetNewBlobExpire(expire, dead_time);
            ++it_tok;
            m_BlobAccess->SetVersionTTL(int(NStr::StringToUInt(*it_tok)));
            ++it_tok;
            m_BlobAccess->SetNewVerExpire(NStr::StringToInt(*it_tok));
            ++it_tok;
            Uint8 create_server = NStr::StringToUInt8(*it_tok);
            ++it_tok;
            Uint4 create_id = NStr::StringToUInt(*it_tok);
            m_BlobAccess->SetCreateServer(create_server, create_id);
        }
        catch (CStringException&) {
            x_ProcessProtocolError();
            return true;
        }
        x_SetState(eReadBlobData);
    }
    return true;
}

bool
CNCActiveHandler::x_ReadBlobData(void)
{
    while (m_SizeToRead != 0) {
        Uint4 read_len = Uint4(m_BlobAccess->GetWriteMemSize());
        if (m_BlobAccess->HasError()) {
            x_CloseConnection();
            return true;
        }
        size_t n_read = m_SockBuffer.Read(m_BlobAccess->GetWriteMemPtr(), read_len);
        if (n_read == 0)
            return false;
        m_BlobAccess->MoveWritePos(n_read);
        m_SizeToRead -= n_read;
    }
    m_BlobAccess->Finalize();
    if (m_BlobAccess->HasError()) {
        x_FinishCommand(false);
    }
    else {
        if (m_OrigRecNo != 0) {
            SNCSyncEvent* event = new SNCSyncEvent();
            event->event_type = eSyncWrite;
            event->key = m_BlobKey;
            event->orig_server = m_OrigServer;
            event->orig_time = m_OrigTime;
            event->orig_rec_no = m_OrigRecNo;
            CNCSyncLog::AddEvent(m_BlobSlot, event);
        }
        x_FinishSyncCmd(eSynOK);
        x_FinishCommand(true);
    }
    return true;
}

bool
CNCActiveHandler::x_ReadSyncProInfoAnswer(void)
{
    if (NStr::FindNoCase(m_Response, "BLOB not found") != NPOS) {
        x_FinishSyncCmd(eSynOK);
        x_FinishCommand(true);
    }
    else if (NStr::FindNoCase(m_Response, "NEED_ABORT") != NPOS) {
        x_FinishSyncCmd(eSynAborted);
        x_FinishCommand(true);
    }
    else if (NStr::FindNoCase(m_Response, "HAVE_NEWER") != NPOS) {
        x_FinishSyncCmd(eSynOK);
        x_FinishCommand(true);
    }
    else {
        if (!x_ReadSizeToRead())
            return true;
        list<CTempString> tokens;
        NStr::Split(m_Response, " ", tokens);
        if (tokens.size() != 7) {
            x_ProcessProtocolError();
            return true;
        }
        list<CTempString>::const_iterator it_tok = tokens.begin();
        try {
            ++it_tok;
            m_BlobSum.create_time = NStr::StringToUInt8(*it_tok);
            ++it_tok;
            m_BlobSum.create_server = NStr::StringToUInt8(*it_tok);
            ++it_tok;
            m_BlobSum.create_id = NStr::StringToUInt(*it_tok);
            ++it_tok;
            m_BlobSum.dead_time = NStr::StringToInt(*it_tok);
            ++it_tok;
            m_BlobSum.expire = NStr::StringToInt(*it_tok);
            ++it_tok;
            m_BlobSum.ver_expire = NStr::StringToInt(*it_tok);
        }
        catch (CStringException&) {
            x_ProcessProtocolError();
            return true;
        }
        x_DoProlongOur();
    }
    return true;
}

void
CNCActiveHandler::x_DoProlongOur(void)
{
    m_BlobAccess = g_NCStorage->GetBlobAccess(eNCRead, m_BlobKey,
                                              kEmptyStr, m_TimeBucket);
    x_SetStateAndAddToPool(eWaitForMetaInfo);
}

bool
CNCActiveHandler::x_ExecuteProInfoCmd(void)
{
    if (m_BlobAccess->HasError()) {
        x_FinishCommand(false);
        return true;
    }
    if (!m_BlobAccess->IsBlobExists()) {
        m_BlobAccess->Release();
        m_BlobAccess = NULL;
        SyncRead(m_SyncCtrl, m_BlobKey, m_BlobSum.create_time);
        return true;
    }

    bool need_event = false;
    if (m_BlobAccess->GetCurBlobCreateTime() == m_BlobSum.create_time
        &&  m_BlobAccess->GetCurCreateServer() == m_BlobSum.create_server
        &&  m_BlobAccess->GetCurCreateId() == m_BlobSum.create_id)
    {
        if (m_BlobAccess->GetCurBlobExpire() < m_BlobSum.expire) {
            m_BlobAccess->SetCurBlobExpire(m_BlobSum.expire, m_BlobSum.dead_time);
            need_event = true;
        }
        if (m_BlobAccess->GetCurVerExpire() < m_BlobSum.ver_expire) {
            m_BlobAccess->SetCurVerExpire(m_BlobSum.ver_expire);
            need_event = true;
        }
    }

    if (need_event  &&  m_OrigRecNo != 0) {
        SNCSyncEvent* event = new SNCSyncEvent();
        event->event_type = eSyncProlong;
        event->key = m_BlobKey;
        event->orig_server = m_OrigServer;
        event->orig_time = m_OrigTime;
        event->orig_rec_no = m_OrigRecNo;
        CNCSyncLog::AddEvent(m_BlobSlot, event);
    }
    x_FinishSyncCmd(eSynOK);
    x_FinishCommand(true);
    return true;
}

bool
CNCActiveHandler::x_WaitOneLineAnswer(void)
{
    bool has_line = m_SockBuffer.ReadLine(&m_Response);
    if (m_SockBuffer.HasError()) {
        m_ErrMsg = "ERR:Connection closed by peer";
        if (!m_GotCmdAnswer  &&  !m_SockBuffer.HasDataToRead()) {
            if (!m_GotAnyAnswer)
                m_Peer->RegisterConnError();
            else if (x_ReplaceServerConn())
                x_SendCmdToExecute();
        }
        return false;
    }
    else if (!has_line) {
        return false;
    }

    if (!m_GotAnyAnswer) {
        m_GotAnyAnswer = true;
        m_Peer->RegisterConnSuccess();
    }

    m_GotCmdAnswer = true;
    if (NStr::StartsWith(m_Response, "ERR:")) {
        if (NStr::FindNoCase(m_Response, "BLOB not found") != NPOS
            &&  (m_CurCmd == eCopyProlong  ||  m_CurCmd == eSearchMeta
                 ||  m_CurCmd == eSyncGet  ||  m_CurCmd == eSyncProInfo))
        {
            goto still_process_as_ok;
        }
        x_ProcessPeerError();
    }
    else if (!NStr::StartsWith(m_Response, "OK:")) {
        x_ProcessProtocolError();
    }
    else {
still_process_as_ok:
        switch (m_CurCmd) {
        case eSearchMeta:
            x_SetState(eReadFoundMeta);
            break;
        case eCopyPut:
            x_SetState(eReadCopyPut);
            break;
        case eCopyProlong:
            x_SetState(eReadCopyProlong);
            break;
        case eNeedOnlyConfirm:
            x_SetState(eReadConfirm);
            break;
        case eReadData:
            x_SetState(eReadDataPrefix);
            break;
        case eWriteData:
            x_SetState(eReadWritePrefix);
            break;
        case eSyncStart:
        case eSyncBList:
            x_SetState(eReadSyncStartAnswer);
            break;
        case eSyncGet:
            x_SetState(eReadSyncGetAnswer);
            break;
        case eSyncProInfo:
            x_SetState(eReadSyncProInfoAnswer);
            break;
        default:
            abort();
        }
    }
    return true;
}

bool
CNCActiveHandler::x_WaitForMetaInfo(void)
{
    if (m_BlobAccess->ObtainMetaInfo(this) == eNCWouldBlock) {
        x_WaitForWouldBlock();
        return false;
    }
    switch (m_CurCmd) {
    case eCopyPut:
        x_SetState(eSendCopyPutCmd);
        break;
    case eSyncGet:
        x_SetState(eSendSyncGetCmd);
        break;
    case eSyncProlongPeer:
        x_SetState(ePrepareSyncProlongCmd);
        break;
    case eSyncProInfo:
        x_SetState(eExecuteProInfoCmd);
        break;
    default:
        abort();
    }
    return true;
}

bool
CNCActiveHandler::x_WaitForFirstData(void)
{
    if (m_BlobAccess->ObtainFirstData(this) == eNCWouldBlock) {
        x_WaitForWouldBlock();
        return false;
    }
    if (m_BlobAccess->HasError()) {
        m_ErrMsg = "ERR:Blob data is corrupted";
        x_CloseConnection();
    }
    else {
        switch (m_CurCmd) {
        case eCopyPut:
            x_StartWritingBlob();
            x_SetState(eWriteBlobData);
            break;
        default:
            abort();
        }
    }
    return true;
}

bool
CNCActiveHandler::x_WriteBlobData(void)
{
    size_t n_written;
    do {
        if (m_ChunkSize == 0) {
            m_ChunkSize = Uint4(min(m_BlobAccess->GetCurBlobSize()
                                        - m_BlobAccess->GetPosition(),
                                    Uint8(0xFFFFFFFE)));
            if (m_ChunkSize == 0) {
                x_FinishWritingBlob();
                return true;
            }
            m_SockBuffer.WriteNoFail(&m_ChunkSize, sizeof(m_ChunkSize));
        }

        size_t want_read = m_BlobAccess->GetDataSize();
        if (m_ChunkSize < want_read)
            want_read = m_ChunkSize;
        if (m_BlobAccess->HasError()) {
            m_ErrMsg = "ERR:Blob data is corrupted";
            x_CloseConnection();
            return true;
        }
        if (want_read == 0)
            abort();

        n_written = m_SockBuffer.Write(m_BlobAccess->GetDataPtr(), want_read);
        // HasError is processed in ManageCmdPipeline
        m_ChunkSize -= Uint4(n_written);
        m_BlobAccess->MoveCurPos(n_written);
    }
    while (!m_SockBuffer.HasError()  &&  n_written != 0);

    return false;
}

void
CNCActiveHandler::x_ManageCmdPipeline(void)
{
    m_ObjLock.Lock();
    InitDiagnostics();

    if (m_SyncCtrl) {
        Uint4 to_wait = CNCPeriodicSync::BeginTimeEvent(m_SrvId);
        if (to_wait != 0) {
            CNCPeriodicSync::EndTimeEvent(m_SrvId, 0);
            Uint8 now = CNetCacheServer::GetPreciseTime();
            m_ThrottleTime = now + to_wait;
            if (g_NetcacheServer->IsLogCmds()) {
                GetDiagContext().Extra().Print("throt", to_wait);
            }
            m_WaitForThrottle = true;
            x_DeferConnection();
            ResetDiagnostics();
            m_ObjLock.Unlock();
            return;
        }
        else {
            m_WaitForThrottle = false;
        }
    }

    m_InCmdPipeline = true;
    m_SockBuffer.Flush();
    if (!m_DidFirstWrite) {
        m_DidFirstWrite = true;
        m_SockBuffer.SetTimeout(CNCDistributionConf::GetPeerTimeout());
    }
    if (m_NeedFlushBuff  &&  !m_SockBuffer.IsWriteDataPending()) {
        m_NeedFlushBuff = false;
    }
    bool do_next_step = true;
    while (!m_SockBuffer.HasError()  &&  !m_SockBuffer.IsWriteDataPending()
           &&  do_next_step)
    {
        switch (x_GetState()) {
        case eConnIdle:
            abort();
        case eWaitClientRelease:
        case eReadyForPool:
        case eConnClosed:
            goto out_of_cycle;
        case eReadFoundMeta:
            do_next_step = x_ReadFoundMeta();
            break;
        case eSendCopyPutCmd:
            do_next_step = x_SendCopyPutCmd();
            break;
        case eReadCopyPut:
            do_next_step = x_ReadCopyPut();
            break;
        case eReadCopyProlong:
            do_next_step = x_ReadCopyProlong();
            break;
        case eReadConfirm:
            do_next_step = x_ReadConfirm();
            break;
        case eReadDataPrefix:
            do_next_step = x_ReadDataPrefix();
            break;
        case eReadDataForClient:
            do_next_step = x_ReadDataForClient();
            break;
        case eReadWritePrefix:
            do_next_step = x_ReadWritePrefix();
            break;
        case eWriteDataForClient:
            do_next_step = x_WriteDataForClient();
            break;
        case eWaitOneLineAnswer:
            do_next_step = x_WaitOneLineAnswer();
            break;
        case eWaitForMetaInfo:
            do_next_step = x_WaitForMetaInfo();
            break;
        case eWaitForFirstData:
            do_next_step = x_WaitForFirstData();
            break;
        case eWriteBlobData:
            do_next_step = x_WriteBlobData();
            break;
        case eReadSyncStartAnswer:
            do_next_step = x_ReadSyncStartAnswer();
            break;
        case eReadEventsList:
            do_next_step = x_ReadEventsList();
            break;
        case eReadBlobsList:
            do_next_step = x_ReadBlobsList();
            break;
        case eSendSyncGetCmd:
            do_next_step = x_SendSyncGetCmd();
            break;
        case eReadSyncGetAnswer:
            do_next_step = x_ReadSyncGetAnswer();
            break;
        case eReadBlobData:
            do_next_step = x_ReadBlobData();
            break;
        case ePrepareSyncProlongCmd:
            do_next_step = x_PrepareSyncProlongCmd();
            break;
        case eReadSyncProInfoAnswer:
            do_next_step = x_ReadSyncProInfoAnswer();
            break;
        case eExecuteProInfoCmd:
            do_next_step = x_ExecuteProInfoCmd();
            break;
        default:
            abort();
        }

        if (!m_SockBuffer.HasError()  &&  m_SockBuffer.IsWriteDataPending()) {
            m_SockBuffer.Flush();
            do_next_step = !m_SockBuffer.IsWriteDataPending();
        }
    }

out_of_cycle:
    if (m_SockBuffer.HasError()  &&  x_GetState() != eConnClosed) {
        m_ErrMsg = "ERR:Error writing to socket";
        x_CloseConnection();
    }
#ifdef NCBI_OS_LINUX
    else if (x_GetState() != eConnClosed) {
        int fd = 0, val = 1;
        m_SrvConn->GetOSHandle(&fd, sizeof(fd));
        setsockopt(fd, IPPROTO_TCP, TCP_QUICKACK, &val, sizeof(val));
    }
#endif

    if (m_SyncCtrl) {
        CNCPeriodicSync::EndTimeEvent(m_SrvId, 0);
    }
    m_InCmdPipeline = false;

    ResetDiagnostics();
    EStates state = x_GetState();
    if (state == eConnClosed) {
        x_MayDeleteThis();
    }
    else  {
        if (state == eWaitClientRelease) {
            if (m_Client)
                x_DeferConnection();
            else
                state = eReadyForPool;
        }
        if (state == eReadyForPool) {
            x_SetState(eConnIdle);
            x_DeferConnection();
            m_ObjLock.Unlock();
            m_Peer->PutConnToPool(this);
        }
        else {
            m_ObjLock.Unlock();
        }
    }
}

END_NCBI_SCOPE
