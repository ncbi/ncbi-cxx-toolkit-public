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

#include "nc_pch.hpp"

#include <corelib/request_ctx.hpp>

#include "netcached.hpp"
#include "active_handler.hpp"
#include "peer_control.hpp"
#include "periodic_sync.hpp"
#include "nc_storage.hpp"
#include "nc_storage_blob.hpp"
#include "message_handler.hpp"
#include "nc_stat.hpp"


BEGIN_NCBI_SCOPE


static string s_PeerAuthString;



//static Uint8 s_CntProxys = 0;

CNCActiveHandler_Proxy::CNCActiveHandler_Proxy(CNCActiveHandler* handler)
    : m_Handler(handler),
      m_NeedToProxy(false)
{
    //Uint8 cnt = AtomicAdd(s_CntProxys, 1);
    //INFO("CNCActiveHandler_Proxy, cnt=" << cnt);
}

CNCActiveHandler_Proxy::~CNCActiveHandler_Proxy(void)
{
    //Uint8 cnt = AtomicSub(s_CntProxys, 1);
    //INFO("~CNCActiveHandler_Proxy, cnt=" << cnt);
}

void
CNCActiveHandler_Proxy::ExecuteSlice(TSrvThreadNum thr_num)
{
    // Lots of checks for NULL follows because these pointers can be nullified
    // concurrently at any moment.
    CNCActiveHandler* handler = ACCESS_ONCE(m_Handler);
    if (handler) {
        // All events coming to this proxy should be transfered to
        // CNCActiveHandler except when we need to proxy data from this socket
        // (from another NC server) to the client.
        if (m_NeedToProxy) {
            CNCActiveClientHub* client = ACCESS_ONCE(handler->m_Client);
            if (client)
                StartProxyTo(client->GetClient(), handler->m_SizeToRead);
            m_NeedToProxy = false;
            if (IsProxyInProgress())
                return;
        }
        handler->SetRunnable();
    }
    else
        Terminate();
}

void
CNCActiveHandler_Proxy::NeedToProxySocket(void)
{
    m_NeedToProxy = true;
    SetRunnable();
}

inline bool
CNCActiveHandler_Proxy::SocketProxyDone(void)
{
    return !m_NeedToProxy  &&  !IsProxyInProgress();
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
    CallRCU();
}

string
CNCActiveClientHub::GetFullPeerName(void)
{
    if (m_Handler) {
        return  CNCDistributionConf::GetFullPeerName(m_Handler->GetSrvId());
    }
    return "";
}

void
CNCActiveClientHub::SetStatus(ENCClientHubStatus status)
{
    ACCESS_ONCE(m_Status) = status;
    if (status != eNCHubCmdInProgress)
        m_Client->SetRunnable();
}

void
CNCActiveClientHub::ExecuteRCU(void)
{
    delete this;
}


//static Uint8 s_CntHndls = 0;

CNCActiveHandler::CNCActiveHandler(Uint8 srv_id, CNCPeerControl* peer)
    : m_SrvId(srv_id),
      m_Peer(peer),
      m_Client(NULL),
      m_SyncCtrl(NULL),
      m_CntCmds(0),
      m_BlobAccess(NULL),
      m_ReservedForBG(false),
      m_ProcessingStarted(false),
      m_CmdStarted(false),
      m_GotAnyAnswer(false),
      m_CmdFromClient(false),
      m_Purge(false)
{
    // Right after creation CNCActiveHandler shouldn't become runnable until
    // it's assigned to CNCActiveHandlerHub or some command-executing method
    // (like CopyPut(), SyncStart() etc.) is called.
    SetState(&CNCActiveHandler::x_InvalidState);

    //Uint8 cnt = AtomicAdd(s_CntHndls, 1);
    //INFO("CNCActiveHandler, cnt=" << cnt);
    ResetSizeRdWr();
}

CNCActiveHandler::~CNCActiveHandler(void)
{
    while (GetDiagCtx()) {
        ReleaseDiagCtx();
    }
    //Uint8 cnt = AtomicSub(s_CntHndls, 1);
    //INFO("~CNCActiveHandler, cnt=" << cnt);
}

void
CNCActiveHandler::Initialize(void)
{
    s_PeerAuthString = kNCPeerClientName;
    s_PeerAuthString += " srv_id=";
    s_PeerAuthString += NStr::UInt8ToString(CNCDistributionConf::GetSelfID());
    s_PeerAuthString += "\n";
}

CNCActiveHandler::State
CNCActiveHandler::x_InvalidState(void)
{
    SRV_FATAL("CNCActiveHandler started in invalid state");
    return NULL;
}

void
CNCActiveHandler::SetClientHub(CNCActiveClientHub* hub)
{
    m_Client = hub;
    m_CmdFromClient = true;
    // Client can release this connection before giving any command and thus
    // before changing state to anything different from x_InvalidState.
    // To gracefully handle such situation we need to be in x_WaitClientRelease
    // state.
    SetState(&CNCActiveHandler::x_WaitClientRelease);
}

void 
CNCActiveHandler::Release(void)
{
    x_SetStateAndStartProcessing(&CNCActiveHandler::x_FinishCommand);
}

void
CNCActiveHandler::ClientReleased(void)
{
    m_Client = NULL;
    SetRunnable();
}

CNCActiveHandler::State
CNCActiveHandler::x_MayDeleteThis(void)
{
    if (!m_Client)
        Terminate();
    return NULL;
}

void
CNCActiveHandler::SetProxy(CNCActiveHandler_Proxy* proxy)
{
    m_Proxy = proxy;
    SetDiagCtx(proxy->GetDiagCtx());
    m_ProcessingStarted = false;
    m_GotAnyAnswer = false;
    m_GotCmdAnswer = false;
    m_GotClientResponse = false;
    // SetProxy() will be called only when socket (CNCActiveHandler_Proxy) have
    // been just created. So first line have to be client authentication. And
    // although we call WriteText() right here this text won't go to the socket
    // until we put first command in there and call Flush(). And we can't call
    // Flush() here (even if we wanted to) because socket could be not writable
    // yet.
    proxy->WriteText(s_PeerAuthString);
}

void
CNCActiveHandler::SetReservedForBG(bool value)
{
    m_ReservedForBG = value;
    SetPriority(value? CNCDistributionConf::GetSyncPriority(): 1);
}

bool
CNCActiveHandler::IsReservedForBG(void)
{
    return m_ReservedForBG;
}

CNCActiveHandler::State
CNCActiveHandler::x_ReplaceServerConn(void)
{
    // In the stack of diag contexts we always have socket's context first and
    // command's context second. Here we change socket thus we need to change
    // socket's context. To do that we need to "release" command's context
    // first (if there is any -- some commands execute without command's
    // context), then release (this time for real) socket's context, then put
    // new socket's context, then put command's context (if there is any).
    CRequestContext* proxy_ctx = m_Proxy->GetDiagCtx();
    CSrvRef<CRequestContext> ctx(GetDiagCtx());
    if (ctx == proxy_ctx)
        ctx.Reset();
    else
        ReleaseDiagCtx();
    ReleaseDiagCtx();

    m_Proxy->SetHandler(NULL);
    m_Proxy->SetRunnable();
    m_Proxy = NULL;

    CNCActiveHandler* src_handler = m_Peer->GetPooledConn();
    if (src_handler) {
        if (!src_handler->m_ProcessingStarted) {
            SRV_LOG(Critical, "src_handler in invalid state");
        }
        m_ProcessingStarted = true;
        m_GotAnyAnswer = src_handler->m_GotAnyAnswer;
        m_Proxy = src_handler->m_Proxy;
        m_Proxy->SetHandler(this);
        SetDiagCtx(m_Proxy->GetDiagCtx());
        if (ctx)
            SetDiagCtx(ctx);
        src_handler->ReleaseDiagCtx();
        m_Peer->ReleaseConn(src_handler);
        src_handler->Terminate();
        return &CNCActiveHandler::x_SendCmdToExecute;
    }
    if (m_Peer->CreateNewSocket(this)) {
        if (ctx)
            SetDiagCtx(ctx);
        x_StartProcessing();
        if (m_ProcessingStarted)
            return &CNCActiveHandler::x_SendCmdToExecute;
    }

    if (ctx)
        SetDiagCtx(ctx);
    return &CNCActiveHandler::x_CloseCmdAndConn;
}

void
CNCActiveHandler::SearchMeta(CRequestContext* cmd_ctx, const CNCBlobKey& nc_key)
{
    SetDiagCtx(cmd_ctx);
    m_CurCmd = eSearchMeta;

    m_CmdToSend.resize(0);
    m_CmdToSend += "PROXY_META \"";
    m_CmdToSend += nc_key.Cache();
    m_CmdToSend += "\" \"";
    m_CmdToSend += nc_key.RawKey();
    m_CmdToSend += "\" \"";
    m_CmdToSend += nc_key.SubKey();
    m_CmdToSend += "\" \"";
    m_CmdToSend += cmd_ctx->GetClientIP();
    m_CmdToSend += "\" \"";
    m_CmdToSend += cmd_ctx->GetSessionID();
    m_CmdToSend.append(1, '"');

    x_SetStateAndStartProcessing(&CNCActiveHandler::x_SendCmdToExecute);
}

void CNCActiveHandler::AskPeerVersion(void)
{
    m_CurCmd = ePeerVersion;
    m_CmdToSend.resize(0);
    m_CmdToSend += "VERSION";
    x_SetStateAndStartProcessing(&CNCActiveHandler::x_SendCmdToExecute);
}

void
CNCActiveHandler::CopyPurge(CRequestContext* cmd_ctx,
                             const string& cache_name,
                             const CSrvTime& when)
{
    if (cmd_ctx)
        SetDiagCtx(cmd_ctx);
    m_CurCmd = eNeedOnlyConfirm;
    m_CmdToSend.resize(0);
    m_CmdToSend += "COPY_PURGE";
    m_CmdToSend += " \"";
    m_CmdToSend += cache_name;
    m_CmdToSend += "\" ";
    m_CmdToSend += NStr::UInt8ToString(when.AsUSec());
    x_SetStateAndStartProcessing(&CNCActiveHandler::x_SendCmdToExecute);
}

void
CNCActiveHandler::CopyUpdate(const CNCBlobKeyLight& nc_key, Uint8 create_time)
{
    m_CurCmd = eNeedOnlyConfirm;

    m_CmdToSend.resize(0);
    m_CmdToSend += "COPY_UPD \"";
    m_CmdToSend += nc_key.Cache();
    m_CmdToSend += "\" \"";
    m_CmdToSend += nc_key.RawKey();
    m_CmdToSend += "\" \"";
    m_CmdToSend += nc_key.SubKey();
    m_CmdToSend += "\" ";
    m_CmdToSend += NStr::UInt8ToString(create_time);
    m_CmdToSend += " ";
//    m_CmdToSend += NStr::UInt8ToString(blob_sum.create_server);
    m_CmdToSend += NStr::UInt8ToString( CNCDistributionConf::GetSelfID());

    x_SetStateAndStartProcessing(&CNCActiveHandler::x_SendCmdToExecute);
}

void CNCActiveHandler::CopyRemove(const CNCBlobKeyLight& nc_key, Uint8 create_time)
{
    m_CurCmd = eNeedOnlyConfirm;

    m_CmdToSend.resize(0);
    m_CmdToSend += "COPY_RMV \"";
    m_CmdToSend += nc_key.Cache();
    m_CmdToSend += "\" \"";
    m_CmdToSend += nc_key.RawKey();
    m_CmdToSend += "\" \"";
    m_CmdToSend += nc_key.SubKey();
    m_CmdToSend += "\" ";
    m_CmdToSend += NStr::UInt8ToString(create_time);
    m_CmdToSend += " ";
    m_CmdToSend += NStr::UInt8ToString( CNCDistributionConf::GetSelfID());

    x_SetStateAndStartProcessing(&CNCActiveHandler::x_SendCmdToExecute);
}

void
CNCActiveHandler::CopyPut(CRequestContext* cmd_ctx,
                          const CNCBlobKeyLight& key,
                          Uint2 slot,
                          Uint8 orig_rec_no)
{
    if (cmd_ctx)
        SetDiagCtx(cmd_ctx);
    m_BlobKey = key;
    x_SetSlotAndBucketAndVerifySlot(slot);
    m_OrigRecNo = orig_rec_no;
    if (m_Client)
        m_Client->SetStatus(eNCHubCmdInProgress);
    x_DoCopyPut();
}

void
CNCActiveHandler::ProxyRemove(CRequestContext* cmd_ctx,
                              const CNCBlobKey& nc_key,
                              const string& password,
                              int version,
                              Uint1 quorum)
{
    SetDiagCtx(cmd_ctx);
    m_CurCmd = eNeedOnlyConfirm;

    m_CmdToSend.resize(0);
    m_CmdToSend += "PROXY_RMV \"";
    m_CmdToSend += nc_key.Cache();
    m_CmdToSend += "\" \"";
    m_CmdToSend += nc_key.RawKey();
    m_CmdToSend += "\" \"";
    m_CmdToSend += nc_key.SubKey();
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

    x_SetStateAndStartProcessing(&CNCActiveHandler::x_SendCmdToExecute);
}

void
CNCActiveHandler::ProxyHasBlob(CRequestContext* cmd_ctx,
                               const CNCBlobKey& nc_key,
                               const string& password,
                               Uint1 quorum)
{
    SetDiagCtx(cmd_ctx);
    m_CurCmd = eNeedOnlyConfirm;

    m_CmdToSend.resize(0);
    m_CmdToSend += "PROXY_HASB \"";
    m_CmdToSend += nc_key.Cache();
    m_CmdToSend += "\" \"";
    m_CmdToSend += nc_key.RawKey();
    m_CmdToSend += "\" \"";
    m_CmdToSend += nc_key.SubKey();
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

    x_SetStateAndStartProcessing(&CNCActiveHandler::x_SendCmdToExecute);
}

void
CNCActiveHandler::ProxyGetSize(CRequestContext* cmd_ctx,
                               const CNCBlobKey& nc_key,
                               const string& password,
                               int version,
                               Uint1 quorum,
                               bool search,
                               bool force_local)
{
    SetDiagCtx(cmd_ctx);
    m_CurCmd = eNeedOnlyConfirm;

    m_CmdToSend.resize(0);
    m_CmdToSend += "PROXY_GSIZ \"";
    m_CmdToSend += nc_key.Cache();
    m_CmdToSend += "\" \"";
    m_CmdToSend += nc_key.RawKey();
    m_CmdToSend += "\" \"";
    m_CmdToSend += nc_key.SubKey();
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

    x_SetStateAndStartProcessing(&CNCActiveHandler::x_SendCmdToExecute);
}

void
CNCActiveHandler::ProxySetValid(CRequestContext* cmd_ctx,
                                const CNCBlobKey& nc_key,
                                const string& password,
                                int version)
{
    SetDiagCtx(cmd_ctx);
    m_CurCmd = eNeedOnlyConfirm;

    m_CmdToSend.resize(0);
    m_CmdToSend += "PROXY_SETVALID \"";
    m_CmdToSend += nc_key.Cache();
    m_CmdToSend += "\" \"";
    m_CmdToSend += nc_key.RawKey();
    m_CmdToSend += "\" \"";
    m_CmdToSend += nc_key.SubKey();
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

    x_SetStateAndStartProcessing(&CNCActiveHandler::x_SendCmdToExecute);
}

void
CNCActiveHandler::ProxyRead(CRequestContext* cmd_ctx,
                            const CNCBlobKey& nc_key,
                            const string& password,
                            int version,
                            Uint8 start_pos,
                            Uint8 size,
                            Uint1 quorum,
                            bool search,
                            bool force_local,
                            Uint8 age)
{
    SetDiagCtx(cmd_ctx);
    m_CurCmd = eReadData;

    m_CmdToSend.resize(0);
    m_CmdToSend += "PROXY_GET \"";
    m_CmdToSend += nc_key.Cache();
    m_CmdToSend += "\" \"";
    m_CmdToSend += nc_key.RawKey();
    m_CmdToSend += "\" \"";
    m_CmdToSend += nc_key.SubKey();
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
    if (age != 0) {
        m_CmdToSend += " age=";
        m_CmdToSend += NStr::UInt8ToString(age);
    }

    x_SetStateAndStartProcessing(&CNCActiveHandler::x_SendCmdToExecute);
}

void
CNCActiveHandler::ProxyReadLast(CRequestContext* cmd_ctx,
                                const CNCBlobKey& nc_key,
                                const string& password,
                                Uint8 start_pos,
                                Uint8 size,
                                Uint1 quorum,
                                bool search,
                                bool force_local,
                                Uint8 age)
{
    SetDiagCtx(cmd_ctx);
    m_CurCmd = eReadData;

    m_CmdToSend.resize(0);
    m_CmdToSend += "PROXY_READLAST \"";
    m_CmdToSend += nc_key.Cache();
    m_CmdToSend += "\" \"";
    m_CmdToSend += nc_key.RawKey();
    m_CmdToSend += "\" \"";
    m_CmdToSend += nc_key.SubKey();
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
    if (age != 0) {
        m_CmdToSend += " age=";
        m_CmdToSend += NStr::UInt8ToString(age);
    }

    x_SetStateAndStartProcessing(&CNCActiveHandler::x_SendCmdToExecute);
}

void
CNCActiveHandler::ProxyGetMeta(CRequestContext* cmd_ctx,
                               const CNCBlobKey& nc_key,
                               Uint1 quorum,
                               bool force_local,
                               int http)
{
    SetDiagCtx(cmd_ctx);
    m_CurCmd = eReadData;

    m_CmdToSend.resize(0);
    m_CmdToSend += "PROXY_GETMETA \"";
    m_CmdToSend += nc_key.Cache();
    m_CmdToSend += "\" \"";
    m_CmdToSend += nc_key.RawKey();
    m_CmdToSend += "\" \"";
    m_CmdToSend += nc_key.SubKey();
    m_CmdToSend += "\" ";
    m_CmdToSend += NStr::UIntToString(quorum);
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UIntToString(Uint1(force_local));
    m_CmdToSend += " \"";
    m_CmdToSend += cmd_ctx->GetClientIP();
    m_CmdToSend += "\" \"";
    m_CmdToSend += cmd_ctx->GetSessionID();
    m_CmdToSend.append(1, '"');
    if (http != 0) {
        m_CmdToSend += " http=";
        m_CmdToSend += NStr::IntToString(http);
    }

    x_SetStateAndStartProcessing(&CNCActiveHandler::x_SendCmdToExecute);
}

void
CNCActiveHandler::ProxyWrite(CRequestContext* cmd_ctx,
                             const CNCBlobKey& nc_key,
                             const string& password,
                             int version,
                             Uint4 ttl,
                             Uint1 quorum)
{
    SetDiagCtx(cmd_ctx);
    m_CurCmd = eWriteData;

    m_CmdToSend.resize(0);
    m_CmdToSend += "PROXY_PUT \"";
    m_CmdToSend += nc_key.Cache();
    m_CmdToSend += "\" \"";
    m_CmdToSend += nc_key.RawKey();
    m_CmdToSend += "\" \"";
    m_CmdToSend += nc_key.SubKey();
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

    x_SetStateAndStartProcessing(&CNCActiveHandler::x_SendCmdToExecute);
}

void
CNCActiveHandler::ProxyProlong(CRequestContext* cmd_ctx,
                               const CNCBlobKey& nc_key,
                               const string& password,
                               unsigned int add_time,
                               Uint1 quorum,
                               bool search,
                               bool force_local)
{
    SetDiagCtx(cmd_ctx);
    m_CurCmd = eNeedOnlyConfirm;

    m_CmdToSend.resize(0);
    m_CmdToSend += "PROXY_PROLONG \"";
    m_CmdToSend += nc_key.Cache();
    m_CmdToSend += "\" \"";
    m_CmdToSend += nc_key.RawKey();
    m_CmdToSend += "\" \"";
    m_CmdToSend += nc_key.SubKey();
    m_CmdToSend += "\" ";
    m_CmdToSend += NStr::IntToString(add_time);
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

    x_SetStateAndStartProcessing(&CNCActiveHandler::x_SendCmdToExecute);
}

void
CNCActiveHandler::CopyProlong(const CNCBlobKeyLight& key,
                              Uint2 slot,
                              Uint8 orig_rec_no,
                              Uint8 orig_time,
                              const SNCBlobSummary& blob_sum)
{
    m_BlobSum = blob_sum;
    m_BlobKey = key;
    x_SetSlotAndBucketAndVerifySlot(slot);
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
    SetDiagCtx(ctrl->GetDiagCtx());
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

    x_SetStateAndStartProcessing(&CNCActiveHandler::x_SendCmdToExecute);
}

void
CNCActiveHandler::SyncBlobsList(CNCActiveSyncControl* ctrl)
{
    m_SyncAction = eSynActionNone;
    m_SyncCtrl = ctrl;
    SetDiagCtx(ctrl->GetDiagCtx());
    m_CurCmd = eSyncBList;

    m_CmdToSend.resize(0);
    m_CmdToSend += "SYNC_BLIST ";
    m_CmdToSend += NStr::UInt8ToString(CNCDistributionConf::GetSelfID());
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UIntToString(ctrl->GetSyncSlot());

    x_SetStateAndStartProcessing(&CNCActiveHandler::x_SendCmdToExecute);
}

void
CNCActiveHandler::SyncSend(CNCActiveSyncControl* ctrl, SNCSyncEvent* event)
{
    m_BlobSum.size = event->blob_size;
    m_SyncAction = eSynActionWrite;
    m_SyncCtrl = ctrl;
    SetDiagCtx(ctrl->GetDiagCtx());
    m_BlobKey = event->key;
    x_SetSlotAndBucketAndVerifySlot(ctrl->GetSyncSlot());
    m_OrigRecNo = event->orig_rec_no;
    x_DoCopyPut();
}

void
CNCActiveHandler::SyncSend(CNCActiveSyncControl* ctrl, const CNCBlobKeyLight& key)
{
    m_SyncAction = eSynActionWrite;
    m_SyncCtrl = ctrl;
    SetDiagCtx(ctrl->GetDiagCtx());
    m_BlobKey = key;
    x_SetSlotAndBucketAndVerifySlot(ctrl->GetSyncSlot());
    m_OrigRecNo = 0;
    x_DoCopyPut();
}

void
CNCActiveHandler::SyncRead(CNCActiveSyncControl* ctrl, SNCSyncEvent* event)
{
    m_BlobSum.size = event->blob_size;
    m_SyncCtrl = ctrl;
    m_BlobKey = event->key;
    m_OrigTime = event->orig_time;
    m_OrigRecNo = event->orig_rec_no;
    m_OrigServer = event->orig_server;
    x_DoSyncGet();
}

void
CNCActiveHandler::SyncRead(CNCActiveSyncControl* ctrl,
                           const CNCBlobKeyLight& key,
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
    m_BlobSum.size = event->blob_size;
    m_SyncCtrl = ctrl;
    SetDiagCtx(ctrl->GetDiagCtx());
    m_SyncAction = eSynActionProlong;
    m_BlobKey = event->key;
    m_OrigTime = event->orig_time;
    m_OrigRecNo = event->orig_rec_no;
    m_OrigServer = event->orig_server;
    x_SetSlotAndBucketAndVerifySlot(ctrl->GetSyncSlot());
    m_CurCmd = eSyncProlongPeer;
    m_BlobAccess = CNCBlobStorage::GetBlobAccess(eNCRead, m_BlobKey.PackedKey(),
                                                 kEmptyStr, m_TimeBucket);
    x_SetStateAndStartProcessing(&CNCActiveHandler::x_WaitForMetaInfo);
    m_BlobAccess->RequestMetaInfo(this);
}

void
CNCActiveHandler::SyncProlongPeer(CNCActiveSyncControl* ctrl,
                                  const CNCBlobKeyLight& key,
                                  const SNCBlobSummary& blob_sum)
{
    m_BlobSum = blob_sum;
    m_SyncCtrl = ctrl;
    SetDiagCtx(ctrl->GetDiagCtx());
    m_SyncAction = eSynActionProlong;
    m_BlobKey = key;
    x_SetSlotAndBucketAndVerifySlot(ctrl->GetSyncSlot());
    m_OrigTime = 0;
    m_OrigRecNo = 0;
    m_OrigServer = 0;
    x_SendCopyProlongCmd(blob_sum);
}

void
CNCActiveHandler::SyncProlongOur(CNCActiveSyncControl* ctrl,
                                 SNCSyncEvent* event)
{
    m_BlobSum.size = event->blob_size;
    m_SyncCtrl = ctrl;
    SetDiagCtx(ctrl->GetDiagCtx());
    m_SyncAction = eSynActionProlong;
    m_BlobKey = event->key;
    x_SetSlotAndBucketAndVerifySlot(ctrl->GetSyncSlot());
    m_OrigTime = event->orig_time;
    m_OrigRecNo = event->orig_rec_no;
    m_OrigServer = event->orig_server;
    m_CurCmd = eSyncProInfo;

    m_CmdToSend.resize(0);
    m_CmdToSend += "SYNC_PROINFO ";
    m_CmdToSend += NStr::UInt8ToString(CNCDistributionConf::GetSelfID());
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UIntToString(m_BlobSlot);
    m_CmdToSend += " \"";
    m_CmdToSend += m_BlobKey.Cache();
    m_CmdToSend += "\" \"";
    m_CmdToSend += m_BlobKey.RawKey();
    m_CmdToSend += "\" \"";
    m_CmdToSend += m_BlobKey.SubKey();
    m_CmdToSend += "\"";

    x_SetStateAndStartProcessing(&CNCActiveHandler::x_SendCmdToExecute);
}

void
CNCActiveHandler::SyncProlongOur(CNCActiveSyncControl* ctrl,
                                 const string& key,
                                 const SNCBlobSummary& blob_sum)
{
    m_BlobSum = blob_sum;
    m_SyncCtrl = ctrl;
    SetDiagCtx(ctrl->GetDiagCtx());
    m_SyncAction = eSynActionProlong;
    m_BlobKey = key;
    x_SetSlotAndBucketAndVerifySlot(ctrl->GetSyncSlot());
    m_OrigTime = 0;
    m_OrigRecNo = 0;
    m_OrigServer = 0;
    m_CurCmd = eSyncProInfo;

    x_DoProlongOur();
}

void
CNCActiveHandler::SyncCancel(CNCActiveSyncControl* ctrl)
{
    m_SyncCtrl = ctrl;
    SetDiagCtx(ctrl->GetDiagCtx());
    m_SyncAction = eSynActionNone;
    m_CurCmd = eNeedOnlyConfirm;

    m_CmdToSend.resize(0);
    m_CmdToSend += "SYNC_CANCEL ";
    m_CmdToSend += NStr::UInt8ToString(CNCDistributionConf::GetSelfID());
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UIntToString(ctrl->GetSyncSlot());

    x_SetStateAndStartProcessing(&CNCActiveHandler::x_SendCmdToExecute);
}

void
CNCActiveHandler::SyncCommit(CNCActiveSyncControl* ctrl,
                             Uint8 local_rec_no,
                             Uint8 remote_rec_no)
{
    m_SyncCtrl = ctrl;
    SetDiagCtx(ctrl->GetDiagCtx());
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

    x_SetStateAndStartProcessing(&CNCActiveHandler::x_SendCmdToExecute);
}

CNCActiveHandler::State
CNCActiveHandler::x_ConnClosedReplaceable(void)
{
    if (m_Proxy->NeedToClose())
        return &CNCActiveHandler::x_CloseCmdAndConn;
    if (m_GotAnyAnswer) {
        // In this case we already talked to server for some time and then got
        // a disconnect. With old NC server this could mean that connection is
        // closed because of inactivity timeout, in this NC server this can
        // mean only some network glitch. i.e. normally it shouldn't happen.
        return &CNCActiveHandler::x_ReplaceServerConn;
    }

    // Here we will be if we attempted to connect to another NC server and got
    // some error as a result. This is a serious error, we cannot do anything
    // about that.
    m_Peer->RegisterConnError();
    return &CNCActiveHandler::x_CloseCmdAndConn;
}

CNCActiveHandler::State
CNCActiveHandler::x_CloseCmdAndConn(void)
{
    if (m_ErrMsg.empty()) {
        if (m_Proxy  &&  m_Proxy->NeedToClose())
            m_ErrMsg = "ERR:Command aborted";
        else
            m_ErrMsg = "ERR:Connection closed by peer";
    }
    m_CmdSuccess = false;
    x_CleanCmdResources();
    return &CNCActiveHandler::x_CloseConn;
}

CNCActiveHandler::State
CNCActiveHandler::x_CloseConn(void)
{
    m_Peer->ReleaseConn(this);
    if (m_Proxy) {
        // m_Proxy->CloseSocket() cannot be called here as it can interfere
        // with parallel execution of flushing
        m_Proxy->SetHandler(NULL);
        m_Proxy->SetRunnable();
        m_Proxy = NULL;
        ReleaseDiagCtx();
    }
    return &CNCActiveHandler::x_MayDeleteThis;
}

CNCActiveHandler::State
CNCActiveHandler::x_FinishBlobFromClient(void)
{
    CNCActiveClientHub* hub = ACCESS_ONCE(m_Client);
    if (!hub)
        return &CNCActiveHandler::x_CloseCmdAndConn;
    if (!hub->GetClient()->IsBlobWritingFinished())
        return NULL;
    if (m_Proxy->NeedEarlyClose())
        return &CNCActiveHandler::x_CloseCmdAndConn;

    return &CNCActiveHandler::x_FinishWritingBlob;
}

void
CNCActiveHandler::x_DoCopyPut(void)
{
    m_CurCmd = eCopyPut;
    m_BlobAccess = CNCBlobStorage::GetBlobAccess(eNCReadData, m_BlobKey.PackedKey(),
                                                 kEmptyStr, m_TimeBucket);
    x_SetStateAndStartProcessing(&CNCActiveHandler::x_WaitForMetaInfo);
    m_BlobAccess->RequestMetaInfo(this);
}

void
CNCActiveHandler::x_DoSyncGet(void)
{
    m_SyncAction = eSynActionRead;
    SetDiagCtx(m_SyncCtrl->GetDiagCtx());
    x_SetSlotAndBucketAndVerifySlot(m_SyncCtrl->GetSyncSlot());
    m_CurCmd = eSyncGet;
    m_BlobAccess = CNCBlobStorage::GetBlobAccess(eNCCopyCreate, m_BlobKey.PackedKey(),
                                                 kEmptyStr, m_TimeBucket);
    x_SetStateAndStartProcessing(&CNCActiveHandler::x_WaitForMetaInfo);
    m_BlobAccess->RequestMetaInfo(this);
}

void
CNCActiveHandler::x_StartProcessing(void)
{
    m_ProcessingStarted = true;
    if (!m_Proxy->StartProcessing()) {
        m_ProcessingStarted = false;
        m_Proxy->AbortSocket();
        m_ErrMsg = "ERR:Error in TaskServer";
        SetState(&CNCActiveHandler::x_CloseCmdAndConn);
    }
}

void
CNCActiveHandler::x_SetStateAndStartProcessing(State state)
{
    if (m_Client)
        m_Client->SetStatus(eNCHubCmdInProgress);
    // Theoretically there's a race here and m_ProcessingStarted should be checked
    // before setting new state. But if processing was already started and
    // m_ProcessingStarted was already changed to TRUE it will never be changed
    // back to FALSE again. And even if as a result of changing state this object
    // will become runnable on other thread, will proceed with execution and
    // will terminate itself, even in this case object won't be physically deleted
    // yet and reading of m_ProcessingStarted will still be valid. And calling
    // SetRunnable() in this case won't do any harm. OTOH if processing for this
    // object wasn't started yet and m_ProcessingStarted is FALSE then this
    // object's socket wasn't added to main TaskServer's epoll yet. And in this
    // case it's not possible for it to become runnable on the other thread.
    SetState(state);
    if (m_ProcessingStarted)
        SetRunnable();
    else
        x_StartProcessing();
}

CNCActiveHandler::State
CNCActiveHandler::x_SendCmdToExecute(void)
{
    if (m_Proxy->NeedToClose())
        return &CNCActiveHandler::x_CloseCmdAndConn;
    if (m_Proxy->NeedEarlyClose())
        return &CNCActiveHandler::x_ConnClosedReplaceable;

    m_Proxy->WriteText(m_CmdToSend).WriteText("\n");
    m_Proxy->RequestFlush();
    m_CmdSuccess = true;
    m_CmdStarted = true;
    return &CNCActiveHandler::x_WaitOneLineAnswer;
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
CNCActiveHandler::x_CleanCmdResources(void)
{
    if (m_BlobAccess) {
        m_BlobAccess->Release();
        m_BlobAccess = NULL;
    }
    CNCActiveClientHub* hub = ACCESS_ONCE(m_Client);
    if (hub) {
        hub->SetErrMsg(m_ErrMsg);
        // SetStatus MUST be called after SetErrMsg because after SetStatus
        // CNCMessageHandler can immediately check error message without giving
        // us a chance to set it.
        hub->SetStatus(m_CmdSuccess? eNCHubSuccess: eNCHubError);
    }
    else {
        if (!m_CmdSuccess)
            x_FinishSyncCmd(eSynNetworkError);
        else if (m_SyncCtrl) {
            SRV_FATAL("Unfinished sync command");
        }
    }
    m_ErrMsg.clear();
    m_Response.clear();
    m_CmdStarted = false;
    m_BlobSum.reset();

    // Releasing diag context only if we have command-related one. If we only
    // have socket-related context we leave it intact.
    if (GetDiagCtx()  &&  (!m_Proxy  ||  GetDiagCtx() != m_Proxy->GetDiagCtx()))
        ReleaseDiagCtx();
}

CNCActiveHandler::State
CNCActiveHandler::x_FinishCommand(void)
{
    x_CleanCmdResources();
    if (m_Client)
        return &CNCActiveHandler::x_WaitClientRelease;

    return &CNCActiveHandler::x_PutSelfToPool;
}

CNCActiveHandler::State
CNCActiveHandler::x_ProcessPeerError(void)
{
    m_ErrMsg = m_Response;
    m_CmdSuccess = true;
    if (m_SyncCtrl != NULL) {
        x_FinishSyncCmd(eSynAborted);
    }
    GetDiagCtx()->SetRequestStatus(eStatus_OK);
    return &CNCActiveHandler::x_FinishCommand;
}

CNCActiveHandler::State
CNCActiveHandler::x_ProcessProtocolError(void)
{
    SRV_LOG(Critical, "Error from peer "
        << CNCDistributionConf::GetFullPeerName(m_SrvId) << ": "
        << "Protocol error. Got response: '"
        << m_Response << "'");
    m_ErrMsg = "ERR:Protocol error";
    return &CNCActiveHandler::x_CloseCmdAndConn;
}

void
CNCActiveHandler::x_StartWritingBlob(void)
{
    Uint4 start_word = 0x01020304;
    m_Proxy->WriteData(&start_word, sizeof(start_word));
    m_ChunkSize = 0;
}

CNCActiveHandler::State
CNCActiveHandler::x_FinishWritingBlob(void)
{
    if (m_Proxy->NeedEarlyClose()  ||  (m_CmdFromClient  &&  !m_Client))
        return &CNCActiveHandler::x_CloseCmdAndConn;

    Uint4 finish_word = 0xFFFFFFFF;
    m_Proxy->WriteData(&finish_word, sizeof(finish_word));
    m_Proxy->RequestFlush();
    m_CurCmd = eNeedOnlyConfirm;
    return &CNCActiveHandler::x_WaitOneLineAnswer;
}

CNCActiveHandler::State
CNCActiveHandler::x_FakeWritingBlob(void)
{
    x_StartWritingBlob();
    return &CNCActiveHandler::x_FinishWritingBlob;
}

CNCActiveHandler::State
CNCActiveHandler::x_ReadFoundMeta(void)
{
    if (!m_BlobExists)
        return &CNCActiveHandler::x_FinishCommand;

    list<CTempString> params;
    NStr::Split(m_Response, " ", params);
    if (params.size() < 5)
        return &CNCActiveHandler::x_ProcessProtocolError;

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
        return &CNCActiveHandler::x_FinishCommand;
    }
    catch (CStringException&) {
        return &CNCActiveHandler::x_ProcessProtocolError;
    }
}

CNCActiveHandler::State
CNCActiveHandler::x_SendCopyPutCmd(void)
{
    if (m_BlobAccess->HasError()) {
        m_CmdSuccess = false;
        return &CNCActiveHandler::x_FinishCommand;
    }
    if (!m_BlobAccess->IsBlobExists() ||  m_BlobAccess->IsCurBlobDead()) {
        x_FinishSyncCmd(eSynOK);
        return &CNCActiveHandler::x_FinishCommand;
    }
    if (m_Proxy->NeedEarlyClose()  ||  (m_CmdFromClient  &&  !m_Client))
        return &CNCActiveHandler::x_CloseCmdAndConn;

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
    m_CmdToSend += m_BlobKey.Cache();
    m_CmdToSend += "\" \"";
    m_CmdToSend += m_BlobKey.RawKey();
    m_CmdToSend += "\" \"";
    m_CmdToSend += m_BlobKey.SubKey();
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
    m_CmdToSend += " \"";
    m_CmdToSend += GetDiagCtx()->GetClientIP();
    m_CmdToSend += "\" \"";
    m_CmdToSend += GetDiagCtx()->GetSessionID();
    m_CmdToSend.append(1, '"');

    return &CNCActiveHandler::x_SendCmdToExecute;
}

CNCActiveHandler::State
CNCActiveHandler::x_ReadCopyPut(void)
{
    SIZE_TYPE pos = NStr::FindCase(m_Response, "NEED_ABORT");
    if (pos != NPOS) {
        x_FinishSyncCmd(eSynAborted);
        pos += sizeof("NEED_ABORT") - 1;
        if (m_Response.size() > pos  &&  m_Response[pos] == '1')
            return &CNCActiveHandler::x_FinishCommand;
        else
            return &CNCActiveHandler::x_FakeWritingBlob;
    }
    pos = NStr::FindCase(m_Response, "HAVE_NEWER");
    if (pos != NPOS) {
        x_FinishSyncCmd(eSynOK);
        pos += sizeof("HAVE_NEWER") - 1;
        if (m_Response.size() > pos  &&  m_Response[pos] == '1')
            return &CNCActiveHandler::x_FinishCommand;
        else
            return &CNCActiveHandler::x_FakeWritingBlob;
    }

    if (m_Proxy->NeedEarlyClose()  ||  (m_CmdFromClient  &&  !m_Client))
        return &CNCActiveHandler::x_CloseCmdAndConn;

    CWriteBackControl::StartSyncBlob(m_BlobAccess->GetCurBlobCreateTime());
    m_BlobAccess->SetPosition(0);
    x_StartWritingBlob();
    return &CNCActiveHandler::x_WriteBlobData;
}

CNCActiveHandler::State
CNCActiveHandler::x_PrepareSyncProlongCmd(void)
{
    if (m_BlobAccess->HasError()) {
        m_CmdSuccess = false;
        return &CNCActiveHandler::x_FinishCommand;
    }
    if (!m_BlobAccess->IsBlobExists()  ||  m_BlobAccess->IsCurBlobDead()) {
        x_FinishSyncCmd(eSynOK);
        return &CNCActiveHandler::x_FinishCommand;
    }
    if (m_Proxy->NeedEarlyClose())
        return &CNCActiveHandler::x_CloseCmdAndConn;

    SNCBlobSummary blob_sum;
    blob_sum.size          = m_BlobAccess->GetCurBlobSize();
    blob_sum.create_time   = m_BlobAccess->GetCurBlobCreateTime();
    blob_sum.create_server = m_BlobAccess->GetCurCreateServer();
    blob_sum.create_id     = m_BlobAccess->GetCurCreateId();
    blob_sum.dead_time     = m_BlobAccess->GetCurBlobDeadTime();
    blob_sum.expire        = m_BlobAccess->GetCurBlobExpire();
    blob_sum.ver_expire    = m_BlobAccess->GetCurVerExpire();

    // m_BlobAccess is not needed anymore. It will be re-created later if
    // blob won't be found on peer and it will be necessary to execute
    // SYNC_PUT on this blob
    m_BlobAccess->Release();
    m_BlobAccess = NULL;

    x_SendCopyProlongCmd(blob_sum);
    return NULL;
}

void
CNCActiveHandler::x_SendCopyProlongCmd(const SNCBlobSummary& blob_sum)
{
    m_CurCmd = eCopyProlong;
    m_BlobSum = blob_sum;

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
    m_CmdToSend += m_BlobKey.Cache();
    m_CmdToSend += "\" \"";
    m_CmdToSend += m_BlobKey.RawKey();
    m_CmdToSend += "\" \"";
    m_CmdToSend += m_BlobKey.SubKey();
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

    x_SetStateAndStartProcessing(&CNCActiveHandler::x_SendCmdToExecute);
}

CNCActiveHandler::State
CNCActiveHandler::x_ReadCopyProlong(void)
{
    if (!m_BlobExists) {
        if (m_Proxy->NeedEarlyClose())
            return &CNCActiveHandler::x_CloseCmdAndConn;
        // Extract command-related context and pass it to CopyPut where it will
        // be set as current again.
        CSrvRef<CRequestContext> ctx(GetDiagCtx());
        ReleaseDiagCtx();
        CopyPut(ctx, m_BlobKey, m_BlobSlot, 0);
        return NULL;
    }

    if (NStr::FindCase(m_Response, "NEED_ABORT") != NPOS)
        x_FinishSyncCmd(eSynAborted);
    else
        x_FinishSyncCmd(eSynOK);

    return &CNCActiveHandler::x_FinishCommand;
}

CNCActiveHandler::State
CNCActiveHandler::x_ReadConfirm(void)
{
    m_ErrMsg = m_Response;
    x_FinishSyncCmd(eSynOK);
    return &CNCActiveHandler::x_FinishCommand;
}

CNCActiveHandler::State
CNCActiveHandler::x_ReadSizeToRead(void)
{
    size_t pos = m_Response.find("SIZE=");
    if (pos == string::npos) {
        SRV_LOG(Critical, "Error from peer "
            << CNCDistributionConf::GetFullPeerName(m_SrvId) << ": "
            << "SIZE is not found in peer response");
        return &CNCActiveHandler::x_ProcessProtocolError;
    }

    pos += sizeof("SIZE=") - 1;
    try {
        m_SizeToReadReq =
        m_SizeToRead = NStr::StringToUInt8(m_Response.substr(pos),
                                           NStr::fAllowTrailingSymbols);
    }
    catch (CStringException& ex) {
        SRV_LOG(Critical, "Error from peer "
            << CNCDistributionConf::GetFullPeerName(m_SrvId) << ": "
            << "Cannot parse data size: " << ex);
        return &CNCActiveHandler::x_ProcessProtocolError;
    }

    switch (m_CurCmd) {
    case eReadData:
        return &CNCActiveHandler::x_ReadDataPrefix;
    case eSyncStart:
    case eSyncBList:
        return &CNCActiveHandler::x_ReadSyncStartAnswer;
    case eSyncGet:
        return &CNCActiveHandler::x_ReadSyncGetAnswer;
    default:
        SRV_FATAL("Unexpected command: " << m_CurCmd);
    }

    return NULL;
}

CNCActiveHandler::State
CNCActiveHandler::x_ReadDataPrefix(void)
{
    if (m_Proxy->NeedEarlyClose())
        return &CNCActiveHandler::x_CloseCmdAndConn;

    CNCActiveClientHub* hub = ACCESS_ONCE(m_Client);
    if (!hub)
        return &CNCActiveHandler::x_CloseCmdAndConn;

    CNCMessageHandler* client = hub->GetClient();
    client->BeginProxyResponse(m_Response, m_SizeToReadReq);
    // After setting m_GotClientResponse to TRUE CNCMessageHandler can immediately
    // continue its job. Thus we MUST set it after we (asynchronously) messed up
    // with the CNCMessageHandler's socket.
    m_GotClientResponse = true;
    // Make sure that m_Proxy does proxying with the correct diag context set
    // (in case if it needs to log something).
    m_Proxy->SetDiagCtx(GetDiagCtx());
    m_Proxy->NeedToProxySocket();
    return &CNCActiveHandler::x_ReadDataForClient;
}

CNCActiveHandler::State
CNCActiveHandler::x_ReadHttpDataPrefix(void)
// similar to x_ReadDataPrefix
// we just have to figure out content length
{
    if (m_Proxy->NeedEarlyClose())
        return &CNCActiveHandler::x_CloseCmdAndConn;

    CNCActiveClientHub* hub = ACCESS_ONCE(m_Client);
    if (!hub)
        return &CNCActiveHandler::x_CloseCmdAndConn;

    CNCMessageHandler* client = hub->GetClient();
    client->BeginProxyResponse(m_Response, m_SizeToReadReq);

    const char* keywd = "Content-Length:";
    while (m_Proxy->ReadLine(&m_Response)) {
        client->WriteText(m_Response).WriteText("\r\n");
        if (m_Response.empty()) {
            break;
        }
        CTempString::size_type pos = m_Response.find(keywd);
        if (pos != CTempString::npos) {
            try {
                pos += strlen(keywd);
                m_SizeToReadReq =
                m_SizeToRead = NStr::StringToUInt8(m_Response.substr(pos),
                    NStr::fAllowTrailingSymbols | NStr::fAllowLeadingSpaces);
            }
            catch (CStringException& ) {
                return &CNCActiveHandler::x_ProcessProtocolError;
            }
        }
    }
    m_GotClientResponse = true;
    m_Proxy->SetDiagCtx(GetDiagCtx());
    m_Proxy->NeedToProxySocket();
    return &CNCActiveHandler::x_ReadDataForClient;
}

CNCActiveHandler::State
CNCActiveHandler::x_ReadDataForClient(void)
{
    if (!m_Proxy->SocketProxyDone())
        return NULL;

    // Don't forget to remove from m_Proxy diag context set in x_ReadDataPrefix().
    m_Proxy->ReleaseDiagCtx();
    if (!m_Proxy->ProxyHadError()) {
        CNCStat::PeerDataWrite(m_SizeToRead);
        CNCStat::ClientDataRead(m_SizeToRead);
        return &CNCActiveHandler::x_FinishCommand;
    }

    if (m_Proxy->NeedEarlyClose())
        m_ErrMsg = "ERR:Error writing to peer";
    else
        m_ErrMsg = "ERR:Error writing blob data to client";

    m_CmdSuccess = false;
    return &CNCActiveHandler::x_CloseCmdAndConn;
}

CNCActiveHandler::State
CNCActiveHandler::x_ReadWritePrefix(void)
{
    x_StartWritingBlob();
    CNCActiveClientHub* hub = ACCESS_ONCE(m_Client);
    if (!hub)
        return &CNCActiveHandler::x_CloseCmdAndConn;

    m_GotClientResponse = true;
    CNCMessageHandler* client = hub->GetClient();
    client->SetRunnable();

    return &CNCActiveHandler::x_FinishBlobFromClient;
}

CNCActiveHandler::State
CNCActiveHandler::x_ReadSyncStartHeader(void)
{
    if (NStr::FindCase(m_Response, "CROSS_SYNC") != NPOS) {
        x_FinishSyncCmd(eSynCrossSynced);
        return &CNCActiveHandler::x_FinishCommand;
    }
    if (NStr::FindCase(m_Response, "IN_PROGRESS") != NPOS) {
        x_FinishSyncCmd(eSynServerBusy);
        return &CNCActiveHandler::x_FinishCommand;
    }
    if (NStr::FindCase(m_Response, "NEED_ABORT") != NPOS) {
        m_Peer->AbortInitialSync();
        x_FinishSyncCmd(eSynAborted);
        return &CNCActiveHandler::x_FinishCommand;
    }

    return &CNCActiveHandler::x_ReadSizeToRead;
}

CNCActiveHandler::State
CNCActiveHandler::x_ReadSyncStartAnswer(void)
{
    list<CTempString> tokens;
    NStr::Split(m_Response, " ", tokens);
    if (tokens.size() != 2  &&  tokens.size() != 3)
        return &CNCActiveHandler::x_ProcessProtocolError;

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
        return &CNCActiveHandler::x_ProcessProtocolError;
    }

    bool by_blobs = m_CurCmd  == eSyncBList
                    ||  NStr::FindCase(m_Response, "ALL_BLOBS") != NPOS;

    m_SyncCtrl->StartResponse(local_rec_no, remote_rec_no, by_blobs);
    if (by_blobs)
        return &CNCActiveHandler::x_ReadBlobsListKeySize;
    else
        return &CNCActiveHandler::x_ReadEventsListKeySize;
}

CNCActiveHandler::State
CNCActiveHandler::x_ReadSyncStartExtra(void)
{
    if (m_Proxy->NeedEarlyClose()) {
        return &CNCActiveHandler::x_CloseCmdAndConn;
    }
    CTempString line;
    if (!m_Purge) {
        if (!m_Proxy->ReadLine(&line)) {
            return NULL;
        }
        m_Purge = (line == "PURGE:");
    }
    while (m_Proxy->ReadLine(&line)) {
        if (line.empty() || line == ";") {
            if (CNCBlobAccessor::UpdatePurgeData(m_SyncStartExtra)) {
                CNCBlobStorage::SavePurgeData();
            }
            m_Purge = false;
            m_SyncStartExtra.clear();
            return &CNCActiveHandler::x_FinishCommand;
        }
        m_SyncStartExtra += line;
        m_SyncStartExtra += '\n';
    }
    return NULL;
}

CNCActiveHandler::State
CNCActiveHandler::x_ReadEventsListKeySize(void)
{
    if (m_SizeToRead == 0) {
        x_FinishSyncCmd(eSynOK);
        if (m_CurCmd == eSyncStart) {
            return &CNCActiveHandler::x_ReadSyncStartExtra;
        }
        return &CNCActiveHandler::x_FinishCommand;
    }
    if (m_Proxy->NeedEarlyClose())
        return &CNCActiveHandler::x_CloseCmdAndConn;

    m_KeySize = 0;
    if (!m_Proxy->ReadNumber(&m_KeySize))
        return NULL;

    m_SizeToRead -= sizeof(m_KeySize);
    return &CNCActiveHandler::x_ReadEventsListBody;
}

CNCActiveHandler::State
CNCActiveHandler::x_ReadEventsListBody(void)
{
    if (m_Proxy->NeedEarlyClose())
        return &CNCActiveHandler::x_CloseCmdAndConn;

    SNCSyncEvent* evt;
    Uint2 rec_size = m_KeySize + 1
                     + sizeof(evt->rec_no)
                     + sizeof(evt->local_time)
                     + sizeof(evt->orig_rec_no)
                     + sizeof(evt->orig_server)
                     + sizeof(evt->orig_time);
    m_ReadBuf.resize_mem(rec_size);
    if (!m_Proxy->ReadData(m_ReadBuf.data(), rec_size))
        return NULL;

    const char* data = m_ReadBuf.data();
    evt = new SNCSyncEvent;
    evt->key = CTempString(data, m_KeySize);
    data += m_KeySize;
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

    m_SizeToRead -= rec_size;
    if (!m_SyncCtrl->AddStartEvent(evt)) {
        x_FinishSyncCmd(eSynAborted);
        return &CNCActiveHandler::x_FinishCommand;
    }

    return &CNCActiveHandler::x_ReadEventsListKeySize;
}

CNCActiveHandler::State
CNCActiveHandler::x_ReadBlobsListKeySize(void)
{
    if (m_SizeToRead == 0) {
        x_FinishSyncCmd(eSynOK);
        if (m_CurCmd == eSyncStart) {
            return &CNCActiveHandler::x_ReadSyncStartExtra;
        }
        return &CNCActiveHandler::x_FinishCommand;
    }
    if (m_Proxy->NeedEarlyClose())
        return &CNCActiveHandler::x_CloseCmdAndConn;

    m_KeySize = 0;
    if (!m_Proxy->ReadNumber(&m_KeySize))
        return NULL;

    m_SizeToRead -= sizeof(m_KeySize);
    return &CNCActiveHandler::x_ReadBlobsListBody;
}

CNCActiveHandler::State
CNCActiveHandler::x_ReadBlobsListBody(void)
{
    if (m_Proxy->NeedEarlyClose())
        return &CNCActiveHandler::x_CloseCmdAndConn;

    SNCBlobSummary* blob_sum;
    Uint2 rec_size = m_KeySize
                     + sizeof(blob_sum->create_time)
                     + sizeof(blob_sum->create_server)
                     + sizeof(blob_sum->create_id)
                     + sizeof(blob_sum->dead_time)
                     + sizeof(blob_sum->expire)
                     + sizeof(blob_sum->ver_expire);
    m_ReadBuf.resize_mem(rec_size);
    if (!m_Proxy->ReadData(m_ReadBuf.data(), rec_size))
        return NULL;

    const char* data = m_ReadBuf.data();
    blob_sum = new SNCBlobSummary();
    string key(data, m_KeySize);
    data += m_KeySize;
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

    m_SizeToRead -= rec_size;
    if (!m_SyncCtrl->AddStartBlob(key, blob_sum)) {
        x_FinishSyncCmd(eSynAborted);
        return &CNCActiveHandler::x_FinishCommand;
    }

    return &CNCActiveHandler::x_ReadBlobsListKeySize;
}

CNCActiveHandler::State
CNCActiveHandler::x_SendSyncGetCmd(void)
{
    if (m_BlobAccess->HasError()) {
        m_CmdSuccess = false;
        return &CNCActiveHandler::x_FinishCommand;
    }
    if (m_BlobAccess->IsBlobExists()
        &&  m_BlobAccess->GetCurBlobCreateTime() > m_OrigTime)
    {
        x_FinishSyncCmd(eSynOK);
        return &CNCActiveHandler::x_FinishCommand;
    }
    if (m_Proxy->NeedEarlyClose())
        return &CNCActiveHandler::x_CloseCmdAndConn;

    m_CmdToSend.resize(0);
    m_CmdToSend += "SYNC_GET ";
    m_CmdToSend += NStr::UInt8ToString(CNCDistributionConf::GetSelfID());
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UIntToString(m_BlobSlot);
    m_CmdToSend += " \"";
    m_CmdToSend += m_BlobKey.Cache();
    m_CmdToSend += "\" \"";
    m_CmdToSend += m_BlobKey.RawKey();
    m_CmdToSend += "\" \"";
    m_CmdToSend += m_BlobKey.SubKey();
    m_CmdToSend += "\" ";
    m_CmdToSend += NStr::UInt8ToString(m_OrigTime);
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UInt8ToString(m_BlobAccess->GetCurBlobCreateTime());
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::UInt8ToString(m_BlobAccess->GetCurCreateServer());
    m_CmdToSend.append(1, ' ');
    m_CmdToSend += NStr::Int8ToString(m_BlobAccess->GetCurCreateId());

    return &CNCActiveHandler::x_SendCmdToExecute;
}

CNCActiveHandler::State
CNCActiveHandler::x_ReadSyncGetHeader(void)
{
    if (!m_BlobExists) {
        x_FinishSyncCmd(eSynOK);
        return &CNCActiveHandler::x_FinishCommand;
    }

    if (NStr::FindCase(m_Response, "NEED_ABORT") != NPOS) {
        x_FinishSyncCmd(eSynAborted);
        return &CNCActiveHandler::x_FinishCommand;
    }
    if (NStr::FindCase(m_Response, "HAVE_NEWER") != NPOS) {
        x_FinishSyncCmd(eSynOK);
        return &CNCActiveHandler::x_FinishCommand;
    }

    return &CNCActiveHandler::x_ReadSizeToRead;
}

CNCActiveHandler::State
CNCActiveHandler::x_ReadSyncGetAnswer(void)
{
    list<CTempString> tokens;
    NStr::Split(m_Response, " ", tokens);
    if (tokens.size() != 11) {
        return &CNCActiveHandler::x_ProcessProtocolError;
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
        return &CNCActiveHandler::x_ProcessProtocolError;
    }

    return &CNCActiveHandler::x_ReadBlobData;
}

CNCActiveHandler::State
CNCActiveHandler::x_ReadBlobData(void)
{
    if (m_Proxy->NeedEarlyClose())
        return &CNCActiveHandler::x_CloseCmdAndConn;

    while (m_SizeToRead != 0) {
        Uint4 read_len = Uint4(m_BlobAccess->GetWriteMemSize());
        if (m_BlobAccess->HasError()) {
            m_ErrMsg = "ERR:Error writing blob to database";
            return &CNCActiveHandler::x_CloseCmdAndConn;
        }

        Uint4 n_read = Uint4(m_Proxy->Read(m_BlobAccess->GetWriteMemPtr(), read_len));
        if (n_read != 0)
            CNCStat::PeerDataWrite(n_read);
        if (m_Proxy->NeedEarlyClose())
            return &CNCActiveHandler::x_CloseCmdAndConn;
        if (n_read == 0)
            return NULL;

        m_BlobAccess->MoveWritePos(n_read);
        m_SizeToRead -= n_read;
    }

    m_BlobAccess->Finalize();
    if (m_BlobAccess->HasError()) {
        m_CmdSuccess = false;
        return &CNCActiveHandler::x_FinishCommand;
    }

    if (m_OrigRecNo != 0) {
        SNCSyncEvent* event = new SNCSyncEvent();
        event->blob_size = m_BlobAccess->GetCurBlobSize();
        event->event_type = eSyncWrite;
        event->key = m_BlobKey;
        event->orig_server = m_OrigServer;
        event->orig_time = m_OrigTime;
        event->orig_rec_no = m_OrigRecNo;
        CNCSyncLog::AddEvent(m_BlobSlot, event);
    }

    x_FinishSyncCmd(eSynOK);
    return &CNCActiveHandler::x_FinishCommand;
}

CNCActiveHandler::State
CNCActiveHandler::x_ReadSyncProInfoAnswer(void)
{
    if (!m_BlobExists) {
        x_FinishSyncCmd(eSynOK);
        return &CNCActiveHandler::x_FinishCommand;
    }

    if (NStr::FindCase(m_Response, "NEED_ABORT") != NPOS) {
        x_FinishSyncCmd(eSynAborted);
        return &CNCActiveHandler::x_FinishCommand;
    }
    if (NStr::FindCase(m_Response, "HAVE_NEWER") != NPOS) {
        x_FinishSyncCmd(eSynOK);
        return &CNCActiveHandler::x_FinishCommand;
    }

    list<CTempString> tokens;
    NStr::Split(m_Response, " ", tokens);
    if (tokens.size() != 7)
        return &CNCActiveHandler::x_ProcessProtocolError;

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
        return &CNCActiveHandler::x_ProcessProtocolError;
    }

    if (m_Proxy->NeedEarlyClose())
        return &CNCActiveHandler::x_CloseCmdAndConn;

    // x_DoProlongOur will change our state to continue processing.
    x_DoProlongOur();
    return NULL;
}

CNCActiveHandler::State
CNCActiveHandler::x_ReadPeerVersion(void)
{
    Uint8 ver = 0;
    try {
        list<CTempString> tokens;
        NStr::Split(m_Response, "&", tokens);
        ITERATE( list<CTempString>, t, tokens) {
            if (NStr::StartsWith(*t, "protocol_version")) {
                CTempString one, two;
                if (NStr::SplitInTwo(*t, "=", one, two)) {
                    list<CTempString> v;
                    NStr::Split(two, ".", v);
                    if (v.size() >= 3) {
                        for (int i=0; i<3; ++i) {
                            ver = ver * 100 + NStr::StringToUInt8( v.front());
                            v.pop_front();
                        }
                    }
                }
                break;
            }
        }
    } catch (...) {
    }
    m_Peer->SetHostProtocol(ver);
    return &CNCActiveHandler::x_FinishCommand;
}

void
CNCActiveHandler::x_DoProlongOur(void)
{
    m_BlobAccess = CNCBlobStorage::GetBlobAccess(eNCRead, m_BlobKey.PackedKey(),
                                                 kEmptyStr, m_TimeBucket);
    x_SetStateAndStartProcessing(&CNCActiveHandler::x_WaitForMetaInfo);
    m_BlobAccess->RequestMetaInfo(this);
}

CNCActiveHandler::State
CNCActiveHandler::x_ExecuteProInfoCmd(void)
{
    if (m_BlobAccess->HasError()) {
        m_CmdSuccess = false;
        return &CNCActiveHandler::x_FinishCommand;
    }
    if (m_Proxy->NeedEarlyClose())
        return &CNCActiveHandler::x_CloseCmdAndConn;
    if (!m_BlobAccess->IsBlobExists()) {
        m_BlobAccess->Release();
        m_BlobAccess = NULL;
        SyncRead(m_SyncCtrl, m_BlobKey, m_BlobSum.create_time);
        return NULL;
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
        event->blob_size = m_BlobAccess->GetCurBlobSize();
        event->event_type = eSyncProlong;
        event->key = m_BlobKey;
        event->orig_server = m_OrigServer;
        event->orig_time = m_OrigTime;
        event->orig_rec_no = m_OrigRecNo;
        CNCSyncLog::AddEvent(m_BlobSlot, event);
    }

    x_FinishSyncCmd(eSynOK);
    return &CNCActiveHandler::x_FinishCommand;
}

CNCActiveHandler::State
CNCActiveHandler::x_WaitOneLineAnswer(void)
{
    if (m_CmdFromClient  &&  !m_Client)
        return &CNCActiveHandler::x_CloseCmdAndConn;
    if (!m_Proxy->FlushIsDone())
        return NULL;

    bool has_line = m_Proxy->ReadLine(&m_Response);
    if (!has_line  &&  m_Proxy->NeedEarlyClose()) {
        if (m_GotCmdAnswer)
            return &CNCActiveHandler::x_CloseCmdAndConn;
        return &CNCActiveHandler::x_ConnClosedReplaceable;
    }
    if (!has_line)
        return NULL;

    if (!m_GotAnyAnswer) {
        m_GotAnyAnswer = true;
        m_Peer->RegisterConnSuccess();
    }

    m_GotCmdAnswer = true;
    m_BlobExists = true;
    if (NStr::StartsWith(m_Response, "ERR:")) {
        m_BlobExists = false;
        return &CNCActiveHandler::x_ProcessPeerError;
    }
    else if (NStr::StartsWith(m_Response, "HTTP") && m_CurCmd == eReadData) {
        return &CNCActiveHandler::x_ReadHttpDataPrefix;
    }
    else if (!NStr::StartsWith(m_Response, "OK:")) {
        return &CNCActiveHandler::x_ProcessProtocolError;
    }

    switch (m_CurCmd) {
    case eSearchMeta:
        return &CNCActiveHandler::x_ReadFoundMeta;
    case eCopyPut:
        return &CNCActiveHandler::x_ReadCopyPut;
    case eCopyProlong:
        return &CNCActiveHandler::x_ReadCopyProlong;
    case eReadData:
        if (m_BlobExists)
            return &CNCActiveHandler::x_ReadSizeToRead;
        // fall through
    case eNeedOnlyConfirm:
        return &CNCActiveHandler::x_ReadConfirm;
    case eWriteData:
        return &CNCActiveHandler::x_ReadWritePrefix;
    case eSyncStart:
    case eSyncBList:
        return &CNCActiveHandler::x_ReadSyncStartHeader;
    case eSyncGet:
        return &CNCActiveHandler::x_ReadSyncGetHeader;
    case eSyncProInfo:
        return &CNCActiveHandler::x_ReadSyncProInfoAnswer;
    case ePeerVersion:
        return &CNCActiveHandler::x_ReadPeerVersion;
    default:
        SRV_FATAL("Unexpected command: " << m_CurCmd);
    }

    return NULL;
}

CNCActiveHandler::State
CNCActiveHandler::x_WaitForMetaInfo(void)
{
    if (!m_BlobAccess->IsMetaInfoReady())
        return NULL;
    if (m_Proxy->NeedEarlyClose())
        return &CNCActiveHandler::x_CloseCmdAndConn;

    switch (m_CurCmd) {
    case eCopyPut:
        return &CNCActiveHandler::x_SendCopyPutCmd;
    case eSyncGet:
        return &CNCActiveHandler::x_SendSyncGetCmd;
    case eSyncProlongPeer:
        return &CNCActiveHandler::x_PrepareSyncProlongCmd;
    case eSyncProInfo:
        return &CNCActiveHandler::x_ExecuteProInfoCmd;
    default:
        SRV_FATAL("Unexpected command: " << m_CurCmd);
    }

    return NULL;
}

CNCActiveHandler::State
CNCActiveHandler::x_WriteBlobData(void)
{
    for(;;) {
        if (m_ChunkSize == 0) {
            m_ChunkSize = Uint4(min(m_BlobAccess->GetCurBlobSize()
                                        - m_BlobAccess->GetPosition(),
                                    Uint8(0xFFFFFFFE)));
            if (m_ChunkSize == 0)
                return &CNCActiveHandler::x_FinishWritingBlob;

            m_Proxy->WriteData(&m_ChunkSize, sizeof(m_ChunkSize));
        }

        Uint4 want_read = m_BlobAccess->GetReadMemSize();
        if (m_BlobAccess->HasError()) {
            m_ErrMsg = "ERR:Blob data is corrupted";
            return &CNCActiveHandler::x_CloseCmdAndConn;
        }
        if (m_ChunkSize < want_read)
            want_read = m_ChunkSize;

        Uint4 n_written = Uint4(m_Proxy->Write(m_BlobAccess->GetReadMemPtr(), want_read));
        if (n_written != 0)
            CNCStat::PeerDataRead(n_written);
        if (m_Proxy->NeedEarlyClose()  ||  (m_CmdFromClient  &&  !m_Client))
            return &CNCActiveHandler::x_CloseCmdAndConn;
        if (n_written == 0)
            return NULL;

        m_ChunkSize -= n_written;
        m_BlobAccess->MoveReadPos(n_written);
    }
}

CNCActiveHandler::State
CNCActiveHandler::x_WaitClientRelease(void)
{
    if (m_Client)
        return NULL;
    return &CNCActiveHandler::x_PutSelfToPool;
}

CNCActiveHandler::State
CNCActiveHandler::x_PutSelfToPool(void)
{
    if (m_Proxy->NeedEarlyClose())
        return &CNCActiveHandler::x_CloseConn;

    m_CmdFromClient = false;
    m_GotCmdAnswer = false;
    m_GotClientResponse = false;
    SetState(&CNCActiveHandler::x_IdleState);
    m_Peer->PutConnToPool(this);
    return NULL;
}

CNCActiveHandler::State
CNCActiveHandler::x_IdleState(void)
{
    return NULL;
}

void
CNCActiveHandler::CloseForShutdown(void)
{
    m_Proxy->SetHandler(NULL);
    m_Proxy->SetRunnable();
    m_Proxy = NULL;
    Terminate();
}

void
CNCActiveHandler::CheckCommandTimeout(void)
{
    if (!m_CmdStarted)
        return;

    CNCActiveHandler_Proxy* proxy = ACCESS_ONCE(m_Proxy);
    if (!proxy)
        return;

    int delay_time = CSrvTime::CurSecs() - proxy->m_LastActive;
    if (delay_time > CNCDistributionConf::GetPeerTimeout()
        &&  ((m_CurCmd != eSyncBList  &&  m_CurCmd != eSyncStart)
             ||  delay_time > CNCDistributionConf::GetBlobListTimeout()))
    {
        proxy->m_NeedToClose = true;
        proxy->SetRunnable();
    }
}

END_NCBI_SCOPE
