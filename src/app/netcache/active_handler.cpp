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


#ifndef NCBI_OS_MSWIN
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <netinet/tcp.h>
#endif


BEGIN_NCBI_SCOPE


static string s_PeerAuthString;



CNCActiveHandler_Proxy::CNCActiveHandler_Proxy(CNCActiveHandler* handler)
    : m_Handler(handler)
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
}

void
CNCActiveHandler_Proxy::OnWrite(void)
{
    if (m_Handler)
        m_Handler->OnWrite();
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



inline
CNCActiveHandler::CDiagnosticsGuard
    ::CDiagnosticsGuard(CNCActiveHandler* handler)
    : m_Handler(handler)
{
    m_Handler->InitDiagnostics();
}

inline
CNCActiveHandler::CDiagnosticsGuard::~CDiagnosticsGuard(void)
{
    m_Handler->ResetDiagnostics();
}


CNCActiveHandler::CNCActiveHandler(Uint8 srv_id, CNCPeerControl* peer)
    : m_SrvId(srv_id),
      m_Peer(peer),
      m_Client(NULL),
      m_SyncCtrl(NULL),
      m_CntCmds(0),
      m_BlobAccess(NULL),
      m_State(eConnIdle),
      m_AddedToPool(false),
      m_GotAnyAnswer(false),
      m_NeedFlushBuff(false),
      m_WaitForThrottle(false)
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
    CMutexGuard guard(m_ObjLock);
    x_DetachFromClient();
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
    CNCActiveHandler* src_handler = m_Peer->GetPooledConn();
    if (src_handler) {
        if (!src_handler->m_AddedToPool)
            abort();
        m_AddedToPool = true;
        m_GotAnyAnswer = src_handler->m_GotAnyAnswer;
        m_GotCmdAnswer = src_handler->m_GotCmdAnswer;
        m_Proxy->SetHandler(NULL);
        m_Proxy = src_handler->m_Proxy;
        m_Proxy->SetHandler(this);
        m_SrvConn = src_handler->m_SrvConn;
        m_SockBuffer.ResetSocket(m_SrvConn, CNCDistributionConf::GetPeerTimeout());
        delete src_handler;
        return true;
    }
    else {
        return m_Peer->CreateNewSocket(this);
    }
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
    m_BlobSlot = slot;
    m_OrigRecNo = orig_rec_no;
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
    m_BlobSlot = slot;
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
    m_CurCmd = eSyncStart;

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
    m_BlobSlot = ctrl->GetSyncSlot();
    m_BlobKey = event->key;
    m_OrigRecNo = event->orig_rec_no;
    x_DoCopyPut();
}

void
CNCActiveHandler::SyncSend(CNCActiveSyncControl* ctrl, const string& key)
{
    m_SyncAction = eSynActionWrite;
    m_SyncCtrl = ctrl;
    m_CmdCtx = ctrl->GetDiagCtx();
    m_BlobSlot = ctrl->GetSyncSlot();
    m_BlobKey = key;
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
    m_CurCmd = eSyncProlongPeer;
    m_BlobAccess = g_NCStorage->GetBlobAccess(eNCRead, m_BlobKey,
                                              kEmptyStr, m_BlobSlot);

    CMutexGuard guard(m_ObjLock);
    x_SetState(eWaitForMetaInfo);
    if (!m_AddedToPool)
        x_AddConnToPool();
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
    m_BlobSlot = ctrl->GetSyncSlot();
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
    m_BlobSlot = ctrl->GetSyncSlot();
    m_SyncAction = eSynActionProlong;
    m_BlobKey = event->key;
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
    CDiagnosticsGuard guard(this);
    ERR_POST("Peer doesn't respond for too long");
    m_ErrMsg = "ERR:Peer doesn't respond";
}

void
CNCActiveHandler::OnClose(IServer_ConnectionHandler::EClosePeer peer)
{
    x_SetState(eConnClosed);
    if (m_DidFirstWrite  &&  !m_GotAnyAnswer) {
        m_Peer->RegisterConnError();
    }
    else if (x_ReplaceServerConn()) {
        if (x_GetState() == eWaitForMetaInfo) {
            if (!m_AddedToPool)
                x_AddConnToPool();
        }
        else {
            x_SendCmdToExecute();
        }
        return;
    }
    if (peer == IServer_ConnectionHandler::eClientClose) {
        m_ErrMsg = "ERR:Connection closed by peer";
    }
    x_FinishCommand(false);

    m_Peer->ReleaseConn(this);
    if (m_ObjLock.TryLock()) {
        m_ObjLock.Unlock();
        delete this;
    }
}

void
CNCActiveHandler::OnRead(void)
{
    CMutexGuard guard(m_ObjLock);
    x_ManageCmdPipeline();
}

void
CNCActiveHandler::OnWrite(void)
{
    CMutexGuard guard(m_ObjLock);
    x_ManageCmdPipeline();
}

EIO_Event
CNCActiveHandler::GetEventsToPollFor(const CTime** alarm_time)
{
    EStates state = x_GetState();
    switch (state) {
    case eWaitOneLineAnswer:
    case eReadDataForClient:
    case eReadEventsList:
    case eReadBlobsList:
    case eReadBlobData:
        return eIO_Read;
    default:
        return eIO_Write;
    }
}

bool
CNCActiveHandler::IsReadyToProcess(void)
{
    if (x_GetState() == eConnIdle)
        return false;
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
        else
            abort();
    }
    else if (x_IsFlagSet(fWaitForBlockedOp))
        return false;

    return true;
}

void
CNCActiveHandler::OnBlockedOpFinish(void)
{
    CMutexGuard guard(m_ObjLock);
    x_UnsetFlag(fWaitForBlockedOp);
    g_NetcacheServer->WakeUpPollCycle();
}

CBufferedSockReaderWriter&
CNCActiveHandler::GetSockBuffer(void)
{
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
    CMutexGuard guard(m_ObjLock);
    x_FinishWritingBlob();
    x_UnsetFlag(fWaitForClient);
}

void
CNCActiveHandler::x_DoCopyPut(void)
{
    m_CurCmd = eCopyPut;
    m_BlobAccess = g_NCStorage->GetBlobAccess(eNCReadData, m_BlobKey,
                                              kEmptyStr, m_BlobSlot);

    CMutexGuard guard(m_ObjLock);
    x_SetState(eWaitForMetaInfo);
    if (!m_AddedToPool)
        x_AddConnToPool();
}

void
CNCActiveHandler::x_DoSyncGet(void)
{
    m_SyncAction = eSynActionRead;
    m_CmdCtx = m_SyncCtrl->GetDiagCtx();
    m_BlobSlot = m_SyncCtrl->GetSyncSlot();
    m_CurCmd = eSyncGet;
    m_BlobAccess = g_NCStorage->GetBlobAccess(eNCCopyCreate, m_BlobKey,
                                              kEmptyStr, m_BlobSlot);

    CMutexGuard guard(m_ObjLock);
    x_SetState(eWaitForMetaInfo);
    if (!m_AddedToPool)
        x_AddConnToPool();
}

void
CNCActiveHandler::x_AddConnToPool(void)
{
    try {
        m_SockBuffer.SetTimeout(0.1);
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
        m_Peer->ReleaseConn(this);
    }
}

void
CNCActiveHandler::x_SendCmdToExecute(void)
{
    if (m_Client)
        m_Client->SetStatus(eNCHubCmdInProgress);

retry_cmd:
    m_SockBuffer.WriteMessage("", m_CmdToSend);
    m_GotCmdAnswer = false;
    if (m_AddedToPool) {
        m_SockBuffer.Flush();
        if (m_SockBuffer.HasError()) {
            if (!m_GotAnyAnswer)
                m_Peer->RegisterConnError();
            else if (x_ReplaceServerConn())
                goto retry_cmd;
            m_ErrMsg = "ERR:Cannot connect to peer";
            x_CloseConnection();
        }
        else {
            x_SetState(eWaitOneLineAnswer);
        }
    }
    else {
        x_SetState(eWaitOneLineAnswer);
        x_AddConnToPool();
    }
}

void
CNCActiveHandler::x_SetConnIdle(void)
{
    x_SetState(eConnIdle);
    x_DeferConnection();
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
        if (success) {
            m_Client->SetStatus(eNCHubSuccess);
        }
        else {
            m_Client->SetStatus(eNCHubError);
            x_DetachFromClient();
        }
    }
    else {
        if (m_SyncCtrl) {
            if (!success)
                m_SyncCtrl->CmdFinished(eSynNetworkError, m_SyncAction);
            m_SyncCtrl = NULL;
        }
        if (x_GetState() != eConnClosed)
            x_SetState(eReadyForPool);
    }
}

void
CNCActiveHandler::x_DetachFromClient(void)
{
    if (m_Client) {
        m_CmdCtx.Reset();
        m_Client->SetHandler(NULL);
        m_Client = NULL;
        if (x_GetState() != eConnClosed)
            x_SetState(eReadyForPool);
    }
}

void
CNCActiveHandler::x_ProcessPeerError(void)
{
    m_ErrMsg = m_Response;
    if (NStr::Find(m_Response, "BLOB not found") == NPOS) {
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
    m_SockBuffer.Write((const char*)&start_word, sizeof(start_word));
    m_ChunkSize = 0;
}

void
CNCActiveHandler::x_FinishWritingBlob(void)
{
    Uint4 finish_word = 0xFFFFFFFF;
    m_SockBuffer.Write((const char*)&finish_word, sizeof(finish_word));
    m_SockBuffer.Flush();
    // HasError() is processed in ManageCmdPipeline()
    m_CurCmd = eNeedOnlyConfirm;
    x_SetState(eWaitOneLineAnswer);
}

bool
CNCActiveHandler::x_ReadFoundMeta(void)
{
    if (NStr::StartsWith(m_Response, "OK:BLOB not found")) {
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
                m_BlobExists = true;
            }
            catch (CStringException&) {
                x_ProcessProtocolError();
                return true;
            }
        }
    }
    x_SetConnIdle();
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
    if (!m_BlobAccess->IsBlobExists()  ||  m_BlobAccess->IsCurBlobExpired()) {
        if (m_SyncCtrl)
            m_SyncCtrl->CmdFinished(eSynOK, m_SyncAction);
        x_FinishCommand(true);
        x_DetachFromClient();
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
        m_SyncCtrl->CmdFinished(eSynAborted, m_SyncAction);
        x_FinishCommand(true);
    }
    else if (NStr::FindNoCase(m_Response, "HAVE_NEWER") != NPOS) {
        if (m_SyncCtrl)
            m_SyncCtrl->CmdFinished(eSynOK, m_SyncAction);
        x_FinishCommand(true);
        x_DetachFromClient();
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
        m_SyncCtrl->CmdFinished(eSynOK, m_SyncAction);
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
    if (NStr::StartsWith(m_Response, "OK:BLOB not found")) {
        CopyPut(m_CmdCtx, m_BlobKey, m_BlobSlot, 0);
    }
    else if (NStr::FindNoCase(m_Response, "NEED_ABORT") != NPOS) {
        m_SyncCtrl->CmdFinished(eSynAborted, m_SyncAction);
        x_FinishCommand(true);
    }
    else {
        x_FinishCommand(true);
    }
    return true;
}

bool
CNCActiveHandler::x_ReadConfirm(void)
{
    m_ErrMsg = m_Response;
    if (m_SyncCtrl)
        m_SyncCtrl->CmdFinished(eSynOK, m_SyncAction);
    x_FinishCommand(true);
    x_DetachFromClient();
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
        if (clnt_buf.IsWriteDataPending()) {
            x_SetFlag(fWaitForClient);
            client->ForceBufferFlush();
            x_DeferConnection();
            return false;
        }
        if (n_written == 0)
            return false;
    }
    x_FinishCommand(true);
    x_DetachFromClient();
    return true;
}

bool
CNCActiveHandler::x_ReadWritePrefix(void)
{
    x_StartWritingBlob();
    m_Client->GetClient()->WriteInitWriteResponse(m_Response);
    x_SetState(eWriteDataForClient);
    x_DeferConnection();
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
        m_SyncCtrl->CmdFinished(eSynCrossSynced, eSynActionNone);
        x_FinishCommand(true);
    }
    else if (NStr::FindNoCase(m_Response, "IN_PROGRESS") != NPOS) {
        m_SyncCtrl->CmdFinished(eSynServerBusy, eSynActionNone);
        x_FinishCommand(true);
    }
    else if (NStr::FindNoCase(m_Response, "NEED_ABORT") != NPOS) {
        m_SyncCtrl->CmdFinished(eSynAborted, eSynActionNone);
        x_FinishCommand(true);
    }
    else {
        if (!x_ReadSizeToRead())
            return true;
        list<CTempString> tokens;
        NStr::Split(m_Response, " ", tokens);
        if (tokens.size() != 3) {
            x_ProcessProtocolError();
            return true;
        }
        list<CTempString>::const_iterator it_tok = tokens.begin();
        Uint8 local_rec_no, remote_rec_no;
        try {
            ++it_tok;
            remote_rec_no = NStr::StringToUInt8(*it_tok);
            ++it_tok;
            local_rec_no = NStr::StringToUInt8(*it_tok);
        }
        catch (CStringException&) {
            x_ProcessProtocolError();
            return true;
        }
        bool by_blobs = NStr::FindNoCase(m_Response, "ALL_BLOBS") != NPOS;
        m_SyncCtrl->StartResponse(local_rec_no, remote_rec_no, by_blobs);
        x_SetState(by_blobs? eReadBlobsList: eReadEventsList);
    }
    return true;
}

bool
CNCActiveHandler::x_ReadEventsList(void)
{
    while (m_SizeToRead != 0) {
        if (m_SockBuffer.GetReadSize() < 2) {
            m_SockBuffer.Read();
            if (m_SockBuffer.GetReadSize() < 2)
                return false;
        }

        Uint2 key_size = *(const Uint2*)m_SockBuffer.GetReadData();
        SNCSyncEvent* evt;
        Uint2 rec_size = key_size + sizeof(key_size) + 1
                         + sizeof(evt->rec_no) + sizeof(evt->local_time)
                         + sizeof(evt->orig_rec_no)
                         + sizeof(evt->orig_server)
                         + sizeof(evt->orig_time);
        if (m_SockBuffer.GetReadSize() < rec_size) {
            m_SockBuffer.Read();
            if (m_SockBuffer.GetReadSize() < rec_size)
                return false;
        }

        evt = new SNCSyncEvent;
        const char* data = (const char*)m_SockBuffer.GetReadData()
                                        + sizeof(key_size);
        evt->key.resize(key_size);
        memcpy(&evt->key[0], data, key_size);
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
        m_SockBuffer.EraseReadData(rec_size);

        m_SyncCtrl->AddStartEvent(evt);
    }

    m_SyncCtrl->CmdFinished(eSynOK, eSynActionNone);
    x_FinishCommand(true);
    return true;
}

bool
CNCActiveHandler::x_ReadBlobsList(void)
{
    while (m_SizeToRead != 0) {
        if (m_SockBuffer.GetReadSize() < 2) {
            m_SockBuffer.Read();
            if (m_SockBuffer.GetReadSize() < 2)
                return false;
        }

        Uint2 key_size = *(const Uint2*)m_SockBuffer.GetReadData();
        SNCCacheData* blob_sum;
        Uint2 rec_size = key_size + sizeof(key_size)
                         + sizeof(blob_sum->create_time)
                         + sizeof(blob_sum->create_server)
                         + sizeof(blob_sum->create_id)
                         + sizeof(blob_sum->dead_time)
                         + sizeof(blob_sum->expire)
                         + sizeof(blob_sum->ver_expire);
        if (m_SockBuffer.GetReadSize() < rec_size) {
            m_SockBuffer.Read();
            if (m_SockBuffer.GetReadSize() < rec_size)
                return false;
        }

        blob_sum = new SNCCacheData();
        const char* data = (const char*)m_SockBuffer.GetReadData()
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
        m_SockBuffer.EraseReadData(rec_size);

        m_SyncCtrl->AddStartBlob(key, blob_sum);
    }

    m_SyncCtrl->CmdFinished(eSynOK, eSynActionNone);
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
        m_SyncCtrl->CmdFinished(eSynOK, m_SyncAction);
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
    if (NStr::StartsWith(m_Response, "OK:BLOB not found")) {
        m_SyncCtrl->CmdFinished(eSynOK, m_SyncAction);
        x_FinishCommand(true);
    }
    else if (NStr::FindNoCase(m_Response, "NEED_ABORT") != NPOS) {
        m_SyncCtrl->CmdFinished(eSynAborted, m_SyncAction);
        x_FinishCommand(true);
    }
    else if (NStr::FindNoCase(m_Response, "HAVE_NEWER") != NPOS) {
        m_SyncCtrl->CmdFinished(eSynOK, m_SyncAction);
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
            TNCBlobId create_id = NStr::StringToUInt(*it_tok);
            m_BlobAccess->SetCreateServer(create_server, create_id, m_BlobSlot);
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
        if (!m_SockBuffer.Read())
            return false;
        size_t n_read = m_SockBuffer.GetReadSize();
        m_BlobAccess->WriteData(m_SockBuffer.GetReadData(), n_read);
        m_SizeToRead -= n_read;
        m_SockBuffer.EraseReadData(n_read);
        if (m_BlobAccess->HasError()) {
            x_CloseConnection();
            return true;
        }
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
        m_SyncCtrl->CmdFinished(eSynOK, m_SyncAction);
        x_FinishCommand(true);
    }
    return true;
}

bool
CNCActiveHandler::x_ReadSyncProInfoAnswer(void)
{
    if (NStr::StartsWith(m_Response, "OK:BLOB not found")) {
        m_SyncCtrl->CmdFinished(eSynOK, m_SyncAction);
        x_FinishCommand(true);
    }
    else if (NStr::FindNoCase(m_Response, "NEED_ABORT") != NPOS) {
        m_SyncCtrl->CmdFinished(eSynAborted, m_SyncAction);
        x_FinishCommand(true);
    }
    else if (NStr::FindNoCase(m_Response, "HAVE_NEWER") != NPOS) {
        m_SyncCtrl->CmdFinished(eSynOK, m_SyncAction);
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
                                              kEmptyStr, m_BlobSlot);

    CMutexGuard guard(m_ObjLock);
    x_SetState(eWaitForMetaInfo);
    if (!m_AddedToPool)
        x_AddConnToPool();
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
    m_SyncCtrl->CmdFinished(eSynOK, m_SyncAction);
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
        x_ProcessPeerError();
    }
    else if (!NStr::StartsWith(m_Response, "OK:")) {
        x_ProcessProtocolError();
    }
    else {
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
    do {
        if (m_ChunkSize == 0) {
            m_ChunkSize = Uint4(min(m_BlobAccess->GetCurBlobSize()
                                        - m_BlobAccess->GetPosition(),
                                    Uint8(0xFFFFFFFE)));
            if (m_ChunkSize == 0) {
                x_FinishWritingBlob();
                return true;
            }
            m_SockBuffer.Write((const char*)&m_ChunkSize, sizeof(m_ChunkSize));
        }

        void* write_buf  = m_SockBuffer.PrepareDirectWrite();
        size_t want_read = m_SockBuffer.GetDirectWriteSize();
        if (m_ChunkSize < want_read)
            want_read = m_ChunkSize;
        size_t n_read = 0;
        n_read = m_BlobAccess->ReadData(write_buf, want_read);
        if (m_BlobAccess->HasError()) {
            m_ErrMsg = "ERR:Blob data is corrupted";
            x_CloseConnection();
            return true;
        }
        m_SockBuffer.AddDirectWritten(n_read);
        m_SockBuffer.Flush();
        // HasError is processed in ManageCmdPipeline
        m_ChunkSize -= Uint4(n_read);
    }
    while (!m_SockBuffer.HasError()  &&  !m_SockBuffer.IsWriteDataPending());

    return false;
}

void
CNCActiveHandler::x_ManageCmdPipeline(void)
{
    CDiagnosticsGuard guard(this);

    if (m_SyncCtrl) {
        Uint4 to_wait = CNCPeriodicSync::BeginTimeEvent(m_SrvId);
        if (to_wait != 0) {
            CNCPeriodicSync::EndTimeEvent(m_SrvId, 0);
            Uint8 now = CNetCacheServer::GetPreciseTime();
            m_ThrottleTime = now + to_wait;
            if (g_NetcacheServer->IsLogCmds()) {
                GetDiagContext().Extra().Print("throt", NStr::UIntToString(to_wait));
            }
            m_WaitForThrottle = true;
            x_DeferConnection();
            return;
        }
        else {
            m_WaitForThrottle = false;
        }
    }

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

    if (x_GetState() == eReadyForPool) {
        x_DeferConnection();
        x_SetState(eConnIdle);
        m_Peer->PutConnToPool(this);
    }
    else if (x_GetState() == eConnClosed) {
        delete this;
    }
}

END_NCBI_SCOPE
