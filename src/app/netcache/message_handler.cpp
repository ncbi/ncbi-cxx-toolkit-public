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
 * Authors:  Victor Joukov, Pavel Ivanov
 *
 * File Description: Network cache daemon
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbi_param.hpp>
#include <corelib/request_ctx.hpp>

#include <connect/services/netcache_key.hpp>

#include "message_handler.hpp"
#include "netcache_version.hpp"

// For emulation of CTransmissionReader
#include <util/util_exception.hpp>


BEGIN_NCBI_SCOPE


const size_t kNetworkBufferSize = 64 * 1024;


CNetCache_MessageHandler::SCommandDef s_CommandMap[] = {
    { "A?",       &CNetCache_MessageHandler::ProcessAlive },
    { "OK",       &CNetCache_MessageHandler::ProcessOK },
    { "MONI",     &CNetCache_MessageHandler::ProcessMoni,
        { { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "SMR",      &CNetCache_MessageHandler::ProcessSessionReg,
        { { "host",    eNSPT_Id,   eNSPA_Required },
          { "port",    eNSPT_Int,  eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "SMU",      &CNetCache_MessageHandler::ProcessSessionUnreg,
        { { "host",    eNSPT_Id,   eNSPA_Required },
          { "port",    eNSPT_Int,  eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "SHUTDOWN", &CNetCache_MessageHandler::ProcessShutdown,
        { { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "VERSION",  &CNetCache_MessageHandler::ProcessVersion },
    { "GETCONF",  &CNetCache_MessageHandler::ProcessGetConfig,
        { { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "GETSTAT",  &CNetCache_MessageHandler::ProcessGetStat,
        { { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "DROPSTAT", &CNetCache_MessageHandler::ProcessDropStat,
        { { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "REMOVE",   &CNetCache_MessageHandler::ProcessRemove,
        { { "id",      eNSPT_NCID, eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "RMV2",     &CNetCache_MessageHandler::ProcessRemove2,
        { { "id",      eNSPT_NCID, eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "LOG",      &CNetCache_MessageHandler::ProcessLog,
        { { "val",     eNSPT_Id,   eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "STAT",     &CNetCache_MessageHandler::ProcessStat,
        { { "val",     eNSPT_Id,   eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "GET",      &CNetCache_MessageHandler::ProcessGet,
        { { "id",      eNSPT_NCID, eNSPA_Required },
          { "NW",      eNSPT_Id,   eNSPA_Optional | fNSPA_Match },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "GET2",     &CNetCache_MessageHandler::ProcessGet,
        { { "id",      eNSPT_NCID, eNSPA_Required },
          { "NW",      eNSPT_Id,   eNSPA_Optional | fNSPA_Match },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "PUT",      &CNetCache_MessageHandler::ProcessPut },
    { "PUT2",     &CNetCache_MessageHandler::ProcessPut2,
        { { "timeout", eNSPT_Int,  eNSPA_Optional },
          { "id",      eNSPT_NCID, eNSPA_Optional },
          { "wait",    eNSPT_Str,  eNSPA_Optional, "no" },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "PUT3",     &CNetCache_MessageHandler::ProcessPut3,
        { { "timeout", eNSPT_Int,  eNSPA_Optional },
          { "id",      eNSPT_NCID, eNSPA_Optional },
          { "wait",    eNSPT_Str,  eNSPA_Optional, "no" },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "HASB",     &CNetCache_MessageHandler::ProcessHasBlob,
        { { "id",      eNSPT_NCID, eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "GBOW",     &CNetCache_MessageHandler::ProcessGetBlobOwner,
        { { "id",      eNSPT_NCID, eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "ISLK",     &CNetCache_MessageHandler::ProcessIsLock,
        { { "id",      eNSPT_NCID, eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "GSIZ",     &CNetCache_MessageHandler::ProcessGetSize,
        { { "id",      eNSPT_NCID, eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "STSP",     &CNetCache_MessageHandler::Process_IC_SetTimeStampPolicy,
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "policy",  eNSPT_Int,  eNSPA_Required },
          { "timeout", eNSPT_Int,  eNSPA_Required },
          { "maxtime", eNSPT_Int,  eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "GTSP",     &CNetCache_MessageHandler::Process_IC_GetTimeStampPolicy,
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "SVRP",     &CNetCache_MessageHandler::Process_IC_SetVersionRetention,
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "val",     eNSPT_Id,   eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "GVRP",     &CNetCache_MessageHandler::Process_IC_GetVersionRetention,
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "GTOU",     &CNetCache_MessageHandler::Process_IC_GetTimeout,
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "ISOP",     &CNetCache_MessageHandler::Process_IC_IsOpen,
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "STOR",     &CNetCache_MessageHandler::Process_IC_Store,
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "ttl",     eNSPT_Int,  eNSPA_Required },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "version", eNSPT_Int,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "STRS",     &CNetCache_MessageHandler::Process_IC_StoreBlob,
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "ttl",     eNSPT_Int,  eNSPA_Required },
          { "size",    eNSPT_Int,  eNSPA_Required },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "version", eNSPT_Int,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "GSIZ",     &CNetCache_MessageHandler::Process_IC_GetSize,
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "version", eNSPT_Int,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "GBLW",     &CNetCache_MessageHandler::Process_IC_GetBlobOwner,
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "version", eNSPT_Int,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "READ",     &CNetCache_MessageHandler::Process_IC_Read,
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "version", eNSPT_Int,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "REMO",     &CNetCache_MessageHandler::Process_IC_Remove,
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "version", eNSPT_Int,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "REMK",     &CNetCache_MessageHandler::Process_IC_RemoveKey,
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "GACT",     &CNetCache_MessageHandler::Process_IC_GetAccessTime,
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "version", eNSPT_Int,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "HASB",     &CNetCache_MessageHandler::Process_IC_HasBlobs,
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "version", eNSPT_Int,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "PRG1",     &CNetCache_MessageHandler::Process_IC_Purge1,
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "keepver", eNSPT_Int,  eNSPA_Required },
          { "timeout", eNSPT_Int,  eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { NULL }
};


/////////////////////////////////////////////////////////////////////////////
// CNetCache_MessageHandler implementation
CNetCache_MessageHandler::CNetCache_MessageHandler(CNetCacheServer* server)
    : m_Server(server),
      m_InRequest(false),
      m_Buffer(0),
      m_SeenCR(false),
      m_InTransmission(false),
      m_DelayedWrite(false),
      m_StartPrinted(false),
      m_Parser(s_CommandMap),
      m_PutOK(false),
      m_PutID(false),
      m_SizeKnown(false)
{
}

void
CNetCache_MessageHandler::BeginReadTransmission()
{
    m_InTransmission = true;
    m_SigRead = 0;
    m_Signature = 0;
    m_ProcessMessage = &CNetCache_MessageHandler::ProcessMsgTransmission;
}

void
CNetCache_MessageHandler::BeginDelayedWrite()
{
    m_DelayedWrite = true;
}

int 
CNetCache_MessageHandler::CheckMessage(BUF*        buffer,
                                       const void* data,
                                       size_t      size)
{
    if (!m_InTransmission)
        return Server_CheckLineMessage(buffer, data, size, m_SeenCR);
    unsigned start = 0;
    const unsigned char * msg = (const unsigned char *) data;
    if (size && m_SeenCR && msg[0] == '\n') {
        ++start;
    }
    m_SeenCR = false;
    for (size_t n = start; n < size; ++n) {
        if (m_SigRead < 4) {
            // Reading signature
            m_Signature = (m_Signature << 8) + msg[n];
            if (++m_SigRead == 4) {
                if (m_Signature == 0x01020304)
                    m_ByteSwap = false;
                else if (m_Signature == 0x04030201)
                    m_ByteSwap = true;
                else
                    NCBI_THROW(CUtilException, eWrongData,
                               "Cannot determine the byte order. Got: " +
                               NStr::UIntToString(m_Signature, 0, 16));
                // Signature read and verified, read chunk length
                m_LenRead = 0;
                m_Length = 0;
            }
        } else if (m_LenRead < 4) {
            // Reading length
            m_Length = (m_Length << 8) + msg[n];
            if (++m_LenRead == 4) {
                if (m_Length == 0xffffffff) {
                    _TRACE("Got end of transmission, tail is " << (size-n-1) << " buf size " << BUF_Size(*buffer));
                    m_InTransmission = false;
                    return size-n-1;
                }
                if (m_ByteSwap)
                    m_Length = CByteSwap::GetInt4((unsigned char*)&m_Length);
                // Length read, read data
                start = n + 1;
            }
        } else {
            unsigned chunk_len = min(m_Length, (unsigned)(size-start));
            _TRACE("Transmission buffer written " << chunk_len);
            BUF_Write(buffer, msg+start, chunk_len);
            m_Length -= chunk_len;
            n += chunk_len - 1;
            if (!m_Length) {
                // Read next length, in next CheckMessage invocation
                m_LenRead = 0;
                return size-n-1;
            }
        }
    }
    return -1;
}

EIO_Event
CNetCache_MessageHandler::GetEventsToPollFor(const CTime**) const
{
    if (m_DelayedWrite) return eIO_Write;
    return eIO_Read;
}

void
CNetCache_MessageHandler::OnOpen(void)
{
    CSocket& socket = GetSocket();
    socket.DisableOSSendDelay();
    STimeout to = {m_Server->GetInactivityTimeout(), 0};
    socket.SetTimeout(eIO_ReadWrite, &to);

    m_Stat.InitSession(m_Server->GetTimer().GetLocalTime(),
                       socket.GetPeerAddress());
                    
    m_ProcessMessage = &CNetCache_MessageHandler::ProcessMsgAuth;
    m_DelayedWrite = false;

    if (IsMonitoring()) {
        string msg;
        msg += CTime(CTime::eCurrent).AsString();
        msg += "\n\t";
        msg += m_Auth;
        msg += " ";
        msg += m_Stat.peer_address;
        msg += "\n\t";
        msg + "Connection is opened.";

        MonitorPost(msg);
    }
}

inline void
CNetCache_MessageHandler::x_InitDiagnostics(void)
{
    if (m_Context.NotNull()) {
        CDiagContext::SetRequestContext(m_Context);
    }
}

inline void
CNetCache_MessageHandler::x_DeinitDiagnostics(void)
{
    CDiagContext::SetRequestContext(NULL);
}

void
CNetCache_MessageHandler::OnRead(void)
{
    x_InitDiagnostics();

    CCounterGuard pending_req_guard(m_Server->GetRequestCounter());
    CSocket &socket = GetSocket();
    char read_buf[kNetworkBufferSize];
    size_t n_read;

    if (!m_InRequest) {
        m_Context.Reset(new CRequestContext());
        m_Context->SetRequestID();
        x_InitDiagnostics();
        m_Stat.InitRequest();
        m_InRequest = true;
    }
    EIO_Status status;
    {{
        CTimeGuard time_guard(m_Stat.comm_elapsed);
        status = socket.Read(read_buf, sizeof(read_buf), &n_read);
        ++m_Stat.io_blocks;
    }}
    switch (status) {
    case eIO_Success:
        break;
    case eIO_Timeout:
        this->OnTimeout();
        m_InRequest = false;
        return;
    case eIO_Closed:
        this->OnCloseExt(eClientClose);
        m_InRequest = false;
        return;
    default:
        // TODO: ??? OnError
        return;
    }
    int message_tail = 0;
    char *buf_ptr = read_buf;
    for ( ;n_read > 0 && message_tail >= 0; ) {
        message_tail = this->CheckMessage(&m_Buffer, buf_ptr, n_read);
        // TODO: what should we do if message_tail > n_read?
        if (message_tail >= 0) {
            this->OnMessage(m_Buffer);

            int consumed = n_read - message_tail;
            buf_ptr += consumed;
            n_read -= consumed;
        }
    }

    if (!m_InTransmission && !m_DelayedWrite) {
        OnRequestEnd();
    }

    x_DeinitDiagnostics();
}

void
CNetCache_MessageHandler::OnWrite(void)
{
    x_InitDiagnostics();

    CCounterGuard pending_req_guard(m_Server->GetRequestCounter());
    if (!m_DelayedWrite) {
        return;
    }
    bool res = x_ProcessWriteAndReport(0);
    m_DelayedWrite = m_DelayedWrite && res;
    if (!m_DelayedWrite) {
        OnRequestEnd();
    }

    x_DeinitDiagnostics();
}

bool
CNetCache_MessageHandler::x_ProcessWriteAndReport(unsigned      blob_size,
                                                  const string* req_id)
{
    char buf[kNetworkBufferSize];
    size_t bytes_read;
    ERW_Result res;
    {{
        CTimeGuard time_guard(m_Stat.db_elapsed, &m_Stat);
        res = m_Reader->Read(buf, sizeof(buf), &bytes_read);
    }}
    if (res != eRW_Success || !bytes_read) {
        if (blob_size) {
            string msg("BLOB not found. ");
            if (req_id) msg += *req_id;
            WriteMsg("ERR:", msg);
        }
        m_Reader.reset(0);
        return false;
    }
    if (blob_size) {
        string msg("BLOB found. SIZE=");
        string sz;
        NStr::UIntToString(sz, blob_size);
        msg += sz;
        WriteMsg("OK:", msg);
    }
    {{
        CTimeGuard time_guard(m_Stat.comm_elapsed, &m_Stat);
        CNetCacheServer::WriteBuf(GetSocket(), buf, bytes_read);
    }}
    ++m_Stat.io_blocks;

    // TODO: Check here that bytes_read is less than sizeof(buf)
    // and optimize out delayed write?
    // This code does not work properly, that is why it is commented
    //    if (blob_size && (blob_size <= bytes_read))
    //        return false;

    return true;
}

void
CNetCache_MessageHandler::OnCloseExt(EClosePeer peer)
{
    x_InitDiagnostics();
    if (IsMonitoring()) {
        string msg;
        msg += CTime(CTime::eCurrent).AsString();
        msg += "\n\t";
        msg += m_Auth;
        msg += " ";
        msg += m_Stat.peer_address;
        msg += "\n\t";
        msg += "ConnTime=" + m_Stat.conn_time.AsString();
        msg += "\n\t";

        switch (peer)
        {
        case eOurClose:
            MonitorPost(msg + "NetCache closing connection.");
            break;
        case eClientClose:
            MonitorPost(msg + "Client closed connection to NetCache.");
            break;
        default:
            break;
        }
    }
    x_DeinitDiagnostics();
}

void
CNetCache_MessageHandler::OnTimeout(void)
{
    x_InitDiagnostics();
    LOG_POST(Info << "Timeout, closing connection");
    x_DeinitDiagnostics();
}

void
CNetCache_MessageHandler::OnOverflow(void)
{
    // Max connection overflow
    ERR_POST("Max number of connections reached, closing connection");
}

void
CNetCache_MessageHandler::OnMessage(BUF buffer)
{
    if (m_Server->ShutdownRequested()) {
        WriteMsg("ERR:",
            "NetCache server is shutting down. Session aborted.");
        return;
    }

    // TODO: move exception handling here
    // try {
        (this->*m_ProcessMessage)(buffer);
    // }
}

static void
s_BufReadHelper(void* data, void* ptr, size_t size)
{
    ((string*) data)->append((const char *) ptr, size);
}

static void
s_ReadBufToString(BUF buf, string& str)
{
    str.erase();
    size_t size = BUF_Size(buf);
    str.reserve(size);
    BUF_PeekAtCB(buf, 0, s_BufReadHelper, &str, size);
    BUF_Read(buf, NULL, size);
}

void
CNetCache_MessageHandler::ProcessMsgAuth(BUF buffer)
{
    s_ReadBufToString(buffer, m_Auth);
    m_ProcessMessage = &CNetCache_MessageHandler::ProcessMsgRequest;
}


NCBI_PARAM_DECL(double, server, log_threshold);
static NCBI_PARAM_TYPE(server, log_threshold) s_LogThreshold;
NCBI_PARAM_DEF(double, server, log_threshold, 1.0);

enum ELogRequests {
    eLogAllReqs,
    eDoNotLogHasBlob,
    eDoNotLogRequests
};

NCBI_PARAM_ENUM_DECL(ELogRequests, server, log_requests);
typedef NCBI_PARAM_TYPE(server, log_requests) TLogRequests;
NCBI_PARAM_ENUM_ARRAY(ELogRequests, server, log_requests) {
    { "all",      eLogAllReqs },
    { "not_hasb", eDoNotLogHasBlob },
    { "no",       eDoNotLogRequests }
};
NCBI_PARAM_ENUM_DEF(ELogRequests, server, log_requests, eLogAllReqs);


void
CNetCache_MessageHandler::OnRequestEnd()
{
    int level = m_Server->GetLogLevel();
    if (level >= CNetCacheServer::kLogLevelRequest ||
        (level >= CNetCacheServer::kLogLevelBase &&
         m_Stat.elapsed > s_LogThreshold.Get())) {
#ifdef _DEBUG
        /*
        string trace;
        ITERATE(vector<STraceEvent>, it, m_Stat.events) {
            trace += NStr::DoubleToString(it->time_mark, 6);
            trace += " ";
            trace += it->message;
            trace += "\n";
        }
        */
#endif
#ifdef REPORT_UNACCOUNTED
        int unaccounted = (int)
            ((m_Stat.elapsed - m_Stat.comm_elapsed - m_Stat.db_elapsed)
                / m_Stat.elapsed * 100.0);
#endif
        unsigned pending_requests = m_Server->GetRequestCounter().Get();
        ostrstream msg;
        msg << m_Auth
            << " peer="      << m_Stat.peer_address
            << " request='"  << m_Stat.request << "'"    
            << " blob="      << m_Stat.blob_id
            << " blobsize="  << m_Stat.blob_size
            << " io blocks=" << m_Stat.io_blocks
            << " pending="   << pending_requests
            << " req time="  << NStr::DoubleToString(m_Stat.elapsed, 5) 
            << " comm time=" << NStr::DoubleToString(m_Stat.comm_elapsed, 5)
//            << " lock wait time=" << stat.lock_elapsed
            << " db time="   << NStr::DoubleToString(m_Stat.db_elapsed, 5)
//            << " db hits=" << stat.db_accesses
#ifdef REPORT_UNACCOUNTED
            << " unaccounted=" << unaccounted
#endif
#ifdef _DEBUG
//            << "\n" << trace
#endif
        ;
        LOG_POST(Error << (string) CNcbiOstrstreamToString(msg));
    }

    // Monitoring
    //
    if (IsMonitoring()) {
        string msg, tmp;
        msg += m_Stat.req_time.AsString();
        msg += " (finish - ";
        msg += CTime(CTime::eCurrent).AsString();
        msg += ")\n\t";
        msg += m_Auth;
        msg += " \"";
        msg += m_Stat.request;
        msg += "\" ";
        msg += m_Stat.peer_address;
        msg += "\n\t";
        msg += "ConnTime=" + m_Stat.conn_time.AsString();
        msg += " BLOB size=";
        NStr::UInt8ToString(tmp, m_Stat.blob_size);
        msg += tmp;
        msg += " elapsed=";
        msg += NStr::DoubleToString(m_Stat.elapsed, 5);
        msg += " comm.elapsed=";
        msg += NStr::DoubleToString(m_Stat.comm_elapsed, 5);
        msg += "\n\t";
        msg += m_Stat.type + ":" + m_Stat.details;

        MonitorPost(msg);
    }

    if (m_StartPrinted) {
        GetDiagContext().PrintRequestStop();
    }
    m_Stat.EndRequest();
    m_InRequest = false;
    m_Context.Reset();
}

void
CNetCache_MessageHandler::MonitorException(const std::exception& ex,
                                           const string& comment,
                                           const string& request)
{
    CSocket& socket = GetSocket();
    string msg = comment;
    msg.append(ex.what());
    msg.append(" client=");
    msg.append(m_Auth);
    msg.append(" request='");
    msg.append(request);
    msg.append("'");
    msg.append(" peer=");
    msg.append(socket.GetPeerAddress());
    msg.append(" blobsize=");
    msg.append(NStr::UIntToString(m_Stat.blob_size));
    msg.append(" io blocks=");
    msg.append(NStr::UIntToString(m_Stat.io_blocks));
    MonitorPost(msg);
}

void
CNetCache_MessageHandler::ReportException(const std::exception& ex,
                                          const string& comment,
                                          const string& request)
{
    CSocket& socket = GetSocket();
    ERR_POST(Error << ex.what()
        << " client=" << m_Auth
        << " request='" << request << "'"
        << " peer=" << socket.GetPeerAddress()
        << " blobsize=" << NStr::UIntToString(m_Stat.blob_size)
        << " io blocks=" << NStr::UIntToString(m_Stat.io_blocks));
    if (IsMonitoring()) {
        MonitorException(ex, comment, request);
    }
}

void
CNetCache_MessageHandler::ReportException(const CException& ex,
                                          const string& comment,
                                          const string& request)
{
    CSocket& socket = GetSocket();
    ERR_POST(Error << ex
        << " client=" << m_Auth
        << " request='" << request << "'"
        << " peer=" << socket.GetPeerAddress()
        << " blobsize=" << NStr::UIntToString(m_Stat.blob_size)
        << " io blocks=" << NStr::UIntToString(m_Stat.io_blocks));
    if (IsMonitoring()) {
        MonitorException(ex, comment, request);
    }
}

void
CNetCache_MessageHandler::ProcessMsgTransmission(BUF buffer)
{
    string data;
    s_ReadBufToString(buffer, data);

    const char* buf = data.data();
    size_t buf_size = data.size();
    size_t bytes_written;
    if (m_SizeKnown && (buf_size > m_BlobSize)) {
        m_BlobSize = 0;
        WriteMsg("ERR:", "Blob overflow");
        m_InTransmission = false;
    }
    else {
        if (m_InTransmission || buf_size) {
            ERW_Result res = m_Writer->Write(buf, buf_size, &bytes_written);
            m_BlobSize -= buf_size;
            m_Stat.blob_size += buf_size;

            if (res != eRW_Success) {
                _TRACE("Transmission failed, socket " << &GetSocket());
                WriteMsg("ERR:", "Server I/O error");
                m_Writer->Flush();
                m_Writer.reset(0);
                m_Server->RegisterInternalErr(
                                SBDB_CacheUnitStatistics::eErr_Put, m_Auth);
                m_InTransmission = false;
            }
        }
        if (!m_SizeKnown  &&  !m_InTransmission
            // Handle STRS implementation error - it does not
            // send EOT token, but relies on byte count
            ||  m_SizeKnown  &&  m_BlobSize == 0)
        {
            _TRACE("Flushing transmission, socket " << &GetSocket());
            try {
                {{
                    CTimeGuard time_guard(m_Stat.db_elapsed, &m_Stat);
                    m_Writer->Flush();
                    m_Writer.reset(0);
                }}

                if (m_SizeKnown && (m_BlobSize != 0)) {
                    WriteMsg("ERR:", "eCommunicationError:Unexpected EOF");
                }
                if (m_PutOK) {
                    _TRACE("OK, socket " << &GetSocket());
                    CTimeGuard time_guard(m_Stat.comm_elapsed, &m_Stat);
                    WriteMsg("OK:", "");
                    m_PutOK = false;
                }
                if (m_PutID) {
                    CTimeGuard time_guard(m_Stat.comm_elapsed, &m_Stat);
                    WriteMsg("ID:", m_ReqId);
                    m_PutID = false;
                }
            }
            catch (CException& ex) {
                // We should successfully restore after exception
                ERR_POST(ex);
                if (m_PutOK  ||  m_SizeKnown  ||  m_PutID) {
                    WriteMsg("ERR:", "Error writing blob: " + NStr::Replace(ex.what(), "\n", "; "));
                    m_PutOK = m_PutID = false;
                }
            }

            // Forcibly close transmission - client is not going
            // to send us EOT
            m_InTransmission = false;
            m_SizeKnown = false;
        }
    }

    if (!m_InTransmission) {
        m_ProcessMessage = &CNetCache_MessageHandler::ProcessMsgRequest;
    }
}

void
CNetCache_MessageHandler::x_AssignParams(const map<string, string>& params)
{
    m_Timeout = m_MaxTimeout = m_Version = m_Policy = m_Port = 0;
    m_ICache = NULL;
    m_Host.clear();
    m_ReqId.clear();
    m_ValueParam.clear();
    m_Key.clear();
    m_SubKey.clear();

    bool cache_set = false;
    string cache_name;

    typedef map<string, string> TMap;
    ITERATE(TMap, it, params) {
        const string& key = it->first;
        const string& val = it->second;

        switch (key[0]) {
        case 'c':
            if (key == "cache") {
                m_ICache = m_Server->GetLocalCache(val);
                cache_set = true;
                cache_name = val;
            }
            break;
        case 'h':
            if (key == "host") {
                m_Host = val;
            }
            break;
        case 'i':
            if (key == "ip") {
                _ASSERT(m_Context.NotNull());
                if (!val.empty()) {
                    m_Context->SetClientIP(val);
                }
            }
            else if (key == "id") {
                m_ReqId = val;
            }
            break;
        case 'k':
            if (key == "key") {
                m_Key = val;
            }
            else if (key == "keepver") {
                m_Policy = NStr::StringToUInt(val);
            }
            break;
        case 'm':
            if (key == "maxtime") {
                m_MaxTimeout = NStr::StringToUInt(val);
            }
            break;
        case 'N':
            if (key == "NW") {
                m_Policy = 1;
            }
            break;
        case 'p':
            if (key == "port") {
                m_Port = NStr::StringToUInt(val);
            }
            else if (key == "policy") {
                m_Policy = NStr::StringToUInt(val);
            }
            break;
        case 's':
            if (key == "sid") {
                _ASSERT(m_Context.NotNull());
                if (!val.empty()) {
                    m_Context->SetSessionID(NStr::URLDecode(val));
                }
            }
            else if (key == "subkey") {
                m_SubKey = val;
            }
            else if (key == "size") {
                m_BlobSize = NStr::StringToUInt(val);
            }
            break;
        case 't':
            if (key == "timeout"  ||  key == "ttl") {
                m_Timeout = NStr::StringToUInt(val);
            }
            break;
        case 'v':
            if (key == "val") {
                m_ValueParam = val;
            }
            else if (key == "version") {
                m_Version = NStr::StringToUInt(val);
            }
            break;
        case 'w':
            if (key == "wait") {
                if (val == "yes") {
                    m_Policy = 1;
                }
                else /*if (val == "no")*/ {
                    m_Policy = 0;
                }
            }
            break;
        default:
            break;
        }
    }

    x_InitDiagnostics();

    if (cache_set  &&  !m_ICache) {
        NCBI_THROW(CNSProtoParserException, eWrongParams,
                   "Cache unknown: " + cache_name);
    }
}

void
CNetCache_MessageHandler::ProcessMsgRequest(BUF buffer)
{
    string request;
    s_ReadBufToString(buffer, request);
    try {
        SParsedCmd cmd = m_Parser.ParseCommand(request);
        x_AssignParams(cmd.params);
        if (TLogRequests::GetDefault() == eLogAllReqs
            ||  TLogRequests::GetDefault() == eDoNotLogHasBlob
                &&  NStr::CompareCase(cmd.command->cmd, "HASB") != 0)
        {
            CDiagContext_Extra extra = GetDiagContext().PrintRequestStart();
            extra.Print("cmd", cmd.command->cmd);
            typedef map<string, string> TMap;
            ITERATE(TMap, it, cmd.params) {
                extra.Print(it->first, it->second);
            }

            m_StartPrinted = true;
        }

        m_Stat.request = request;
        (this->*cmd.command->extra.processor)();
    } 
    catch (CNSProtoParserException& ex)
    {
        ReportException(ex, "NC request parser error: ", request);
        WriteMsg("ERR:", ex.GetMsg());
        m_Server->RegisterProtocolErr(
                            SBDB_CacheUnitStatistics::eErr_Unknown, m_Auth);
    }
    catch (CNetCacheException &ex)
    {
        ReportException(ex, "NC Server error: ", request);
        m_Server->RegisterException(m_Stat.op_code, m_Auth, ex);
    }
    catch (CNetServiceException& ex)
    {
        ReportException(ex, "NC Service exception: ", request);
        m_Server->RegisterException(m_Stat.op_code, m_Auth, ex);
    }
    catch (CBDB_ErrnoException& ex)
    {
        if (ex.IsRecovery()) {
            string msg = "Fatal Berkeley DB error: DB_RUNRECOVERY. " 
                         "Emergency shutdown initiated!";
            ERR_POST(msg);
            if (IsMonitoring()) {
                MonitorPost(msg);
            }
            m_Server->SetShutdownFlag();
        } else {
            ReportException(ex, "NC BerkeleyDB error: ", request);
            m_Server->RegisterInternalErr(m_Stat.op_code, m_Auth);
        }
        throw;
    }
    catch (CException& ex)
    {
        ReportException(ex, "NC CException: ", request);
        m_Server->RegisterInternalErr(m_Stat.op_code, m_Auth);
    }
    catch (exception& ex)
    {
        ReportException(ex, "NC std::exception: ", request);
        m_Server->RegisterInternalErr(m_Stat.op_code, m_Auth);
    }
}

void
CNetCache_MessageHandler::ProcessAlive(void)
{
    WriteMsg("OK:", "");
}

void
CNetCache_MessageHandler::ProcessOK(void)
{
}

void
CNetCache_MessageHandler::ProcessMoni(void)
{
    GetSocket().DisableOSSendDelay(false);
    WriteMsg("OK:", "Monitor for " NETCACHED_VERSION "\n");
    m_Server->SetMonitorSocket(GetSocket());
}

void
CNetCache_MessageHandler::x_ProcessSM(bool reg)
{
    // SMR host pid     -- registration
    // or
    // SMU host pid     -- unregistration
    //
    m_Stat.type = "Session";

    if (!m_Server->IsManagingSessions()) {
        WriteMsg("ERR:", "Server does not support sessions ");
    }
    else {
        if (!m_Port || m_Host.empty()) {
            NCBI_THROW(CNSProtoParserException, eWrongParams,
                       "Invalid request for session management");
        }

        if (reg) {
            m_Server->RegisterSession(m_Host, m_Port);
        } else {
            m_Server->UnRegisterSession(m_Host, m_Port);
        }
        WriteMsg("OK:", "");
    };

    GetSocket().Close();
}

void
CNetCache_MessageHandler::ProcessSessionReg(void)
{
    x_ProcessSM(true);
}

void
CNetCache_MessageHandler::ProcessSessionUnreg(void)
{
    x_ProcessSM(false);
}

void
CNetCache_MessageHandler::ProcessShutdown(void)
{
    m_Server->SetShutdownFlag();
    WriteMsg("OK:", "");
}

void
CNetCache_MessageHandler::ProcessVersion(void)
{
    WriteMsg("OK:", NETCACHED_VERSION); 
}

void
CNetCache_MessageHandler::ProcessGetConfig(void)
{
    CSocket& sock = GetSocket();
    SOCK sk = sock.GetSOCK();
    sock.SetOwnership(eNoOwnership);
    sock.Reset(0, eTakeOwnership, eCopyTimeoutsToSOCK);

    CConn_SocketStream ios(sk);
    m_Server->GetRegistry().Write(ios);
}

void
CNetCache_MessageHandler::ProcessGetStat(void)
{
    //CNcbiRegistry reg;

    CSocket& sock = GetSocket();
    SOCK sk = sock.GetSOCK();
    sock.SetOwnership(eNoOwnership);
    sock.Reset(0, eTakeOwnership, eCopyTimeoutsToSOCK);

    CConn_SocketStream ios(sk);

    CBDB_Cache* bdb_cache = m_Server->GetCache();
    if (!bdb_cache) {
        return;
    }
    bdb_cache->Lock();

    try {

        const SBDB_CacheStatistics& cs = bdb_cache->GetStatistics();
        cs.PrintStatistics(ios);
        //cs.ConvertToRegistry(&reg);

    } catch(...) {
        bdb_cache->Unlock();
        throw;
    }
    bdb_cache->Unlock();

    //reg.Write(ios,  CNcbiRegistry::fTransient | CNcbiRegistry::fPersistent);
}

void
CNetCache_MessageHandler::ProcessDropStat(void)
{
    CBDB_Cache* bdb_cache = m_Server->GetCache();
    if (!bdb_cache) {
        return;
    }
	bdb_cache->InitStatistics();
    WriteMsg("OK:", "");
}

void
CNetCache_MessageHandler::ProcessRemove(void)
{
    m_Server->GetCache()->Remove(m_ReqId);
}

void
CNetCache_MessageHandler::ProcessRemove2(void)
{
    ProcessRemove();
    WriteMsg("OK:", "");
}

void
CNetCache_MessageHandler::ProcessLog(void)
{
    int level = CNetCacheServer::kLogLevelBase;
    if (m_ValueParam == "ON")
        level = CNetCacheServer::kLogLevelMax;
    else if (m_ValueParam == "OFF")
        level = CNetCacheServer::kLogLevelBase;
    else if (m_ValueParam == "Operation"  ||  m_ValueParam == "Op")
        level = CNetCacheServer::kLogLevelOp;
    else if (m_ValueParam == "Request")
        level = CNetCacheServer::kLogLevelRequest;
    else {
        try {
            level = NStr::StringToInt(m_ValueParam);
        } catch (...) {
            NCBI_THROW(CNSProtoParserException, eWrongParams,
                       "NetCache Log command: invalid parameter");
        }
    }
    m_Server->SetLogLevel(level);
    WriteMsg("OK:", "");
}

void
CNetCache_MessageHandler::ProcessStat(void)
{
    CBDB_Cache* bdb_cache = m_Server->GetCache();
    if (!bdb_cache) {
        return;
    }

    if (m_ValueParam == "ON")
        bdb_cache->SetSaveStatistics(true);
    else if (m_ValueParam == "OFF")
        bdb_cache->SetSaveStatistics(false);
    else {
        NCBI_THROW(CNSProtoParserException, eWrongParams,
                   "NetCache Stat command: invalid parameter");
    }

    WriteMsg("OK:", "");
}

void
CNetCache_MessageHandler::ProcessGet(void)
{
    m_Stat.op_code = SBDB_CacheUnitStatistics::eErr_Get;
    m_Stat.blob_id = m_ReqId;

    if (m_Policy /* m_NoLock */) {
        bool locked = m_Server->GetCache()->IsLocked(m_ReqId, 0, kEmptyStr);
        if (locked) {
            WriteMsg("ERR:", "BLOB locked by another client"); 
            return;
        }
    }

    ICache::SBlobAccessDescr ba_descr;
    ba_descr.buf = 0;
    ba_descr.buf_size = 0;

    unsigned blob_id = CNetCacheKey::GetBlobId(m_ReqId);

    CBDB_Cache* bdb_cache = m_Server->GetCache();
    for (int repeats = 0; repeats < 1000; ++repeats) {
        CTimeGuard time_guard(m_Stat.db_elapsed, &m_Stat);
        bdb_cache->GetBlobAccess(m_ReqId, 0, kEmptyStr, &ba_descr);

        if (!ba_descr.blob_found) {
            // check if id is locked maybe blob record not
            // yet committed
            {{
                if (bdb_cache->IsLocked(blob_id)) {
                    if (repeats < 999) {
                        SleepMilliSec(repeats);
                        continue;
                    }
                } else {
                    bdb_cache->GetBlobAccess(m_ReqId, 0, kEmptyStr, &ba_descr);
                    if (ba_descr.blob_found) {
                        break;
                    }

                }
            }}
            WriteMsg("ERR:", string("BLOB not found. ") + m_ReqId);
            return;
        } else {
            break;
        }
    }

    if (ba_descr.blob_size == 0) {
        WriteMsg("OK:", "BLOB found. SIZE=0");
        return;
    }

    m_Stat.blob_size = ba_descr.blob_size;

    // re-translate reader to the network
    m_Reader.reset(ba_descr.reader.release());
    if (!m_Reader.get()) {
        WriteMsg("ERR:", string("BLOB not found. ") + m_ReqId);
        return;
    }

    // Write first chunk right here
    if (!x_ProcessWriteAndReport(ba_descr.blob_size, &m_ReqId))
        return;

    BeginDelayedWrite();
}

void
CNetCache_MessageHandler::ProcessPut(void)
{
    WriteMsg("ERR:", "Obsolete");
}

void
CNetCache_MessageHandler::ProcessPut2(void)
{
    m_Stat.op_code = SBDB_CacheUnitStatistics::eErr_Put;

    bool do_id_lock = true;
    CBDB_Cache* bdb_cache = m_Server->GetCache();
    unsigned int id = 0;
    _TRACE("Getting an id, socket " << &GetSocket());
    if (m_ReqId.empty()) {
        CTimeGuard time_guard(m_Stat.db_elapsed, &m_Stat);
        id = bdb_cache->GetNextBlobId(do_id_lock);
        time_guard.Stop();
        CNetCacheKey::GenerateBlobKey(&m_ReqId, id,
                                    m_Server->GetHost(), m_Server->GetPort());
        do_id_lock = false;
    }
    else {
        id = CNetCacheKey::GetBlobId(m_ReqId);
    }
    m_Stat.blob_id = m_ReqId;
    _TRACE("Got id " << id);

    // BLOB already locked, it is safe to return BLOB id
    if (!m_Policy /* !m_WaitForWriting */  &&  !do_id_lock) {
        CTimeGuard time_guard(m_Stat.comm_elapsed, &m_Stat);
        WriteMsg("ID:", m_ReqId);
    }

    // create the reader up front to guarantee correct BLOB locking
    // the possible problem (?) here is that we have to do double buffering
    // of the input stream
    {{
        CTimeGuard time_guard(m_Stat.db_elapsed, &m_Stat);
        m_Writer.reset(
            bdb_cache->GetWriteStream(id, m_ReqId, 0, kEmptyStr, do_id_lock,
                                      m_Timeout, m_Auth));
    }}

    if (!m_Policy /* !m_WaitForWriting */  &&  do_id_lock) {
        CTimeGuard time_guard(m_Stat.comm_elapsed, &m_Stat);
        WriteMsg("ID:", m_ReqId);
    }
    else if (m_Policy /* m_WaitForWriting */) {
        m_PutID = true;
    }

    _TRACE("Begin read transmission");
    BeginReadTransmission();
}

void
CNetCache_MessageHandler::ProcessPut3(void)
{
    m_PutOK = true;
    ProcessPut2();
}

void
CNetCache_MessageHandler::ProcessHasBlob(void)
{
    bool hb = m_Server->GetCache()->HasBlobs(m_ReqId, kEmptyStr);
    string str;
    NStr::UIntToString(str, (int)hb);
    WriteMsg("OK:", str);
}

void
CNetCache_MessageHandler::ProcessGetBlobOwner(void)
{
    string owner;
    m_Server->GetCache()->GetBlobOwner(m_ReqId, 0, kEmptyStr, &owner);

    WriteMsg("OK:", owner);
}

void
CNetCache_MessageHandler::ProcessIsLock(void)
{
    if (m_Server->GetCache()->IsLocked(CNetCacheKey::GetBlobId(m_ReqId))) {
        WriteMsg("OK:", "1");
    } else {
        WriteMsg("OK:", "0");
    }

}

void
CNetCache_MessageHandler::ProcessGetSize(void)
{
    size_t size;
    if (!m_Server->GetCache()->GetSizeEx(m_ReqId, 0, kEmptyStr, &size)) {
        WriteMsg("ERR:", "BLOB not found. " + m_ReqId);
    } else {
        WriteMsg("OK:", NStr::UIntToString(size));
    }
}

void
CNetCache_MessageHandler::Process_IC_SetTimeStampPolicy(void)
{
    m_ICache->SetTimeStampPolicy(m_Policy, m_Timeout, m_MaxTimeout);
    WriteMsg("OK:", "");
}


void
CNetCache_MessageHandler::Process_IC_GetTimeStampPolicy(void)
{
    ICache::TTimeStampFlags flags = m_ICache->GetTimeStampPolicy();
    string str;
    NStr::UIntToString(str, flags);
    WriteMsg("OK:", str);
}

void
CNetCache_MessageHandler::Process_IC_SetVersionRetention(void)
{
    ICache::EKeepVersions policy;
    if (NStr::CompareNocase(m_ValueParam, "KA") == 0) {
        policy = ICache::eKeepAll;
    } else 
    if (NStr::CompareNocase(m_ValueParam, "DO") == 0) {
        policy = ICache::eDropOlder;
    } else 
    if (NStr::CompareNocase(m_ValueParam, "DA") == 0) {
        policy = ICache::eDropAll;
    } else {
        WriteMsg("ERR:", "Invalid version retention code");
        return;
    }
    m_ICache->SetVersionRetention(policy);
    WriteMsg("OK:", "");
}

void
CNetCache_MessageHandler::Process_IC_GetVersionRetention(void)
{
    int p = m_ICache->GetVersionRetention();
    string str;
    NStr::IntToString(str, p);
    WriteMsg("OK:", str);
}

void
CNetCache_MessageHandler::Process_IC_GetTimeout(void)
{
    int to = m_ICache->GetTimeout();
    string str;
    NStr::UIntToString(str, to);
    WriteMsg("OK:", str);
}

void
CNetCache_MessageHandler::Process_IC_IsOpen(void)
{
    bool io = m_ICache->IsOpen();
    string str;
    NStr::UIntToString(str, (int)io);
    WriteMsg("OK:", str);
}


void
CNetCache_MessageHandler::Process_IC_Store(void)
{
    WriteMsg("OK:", "");

    m_Writer.reset(m_ICache->GetWriteStream(m_Key, m_Version, m_SubKey,
                                            m_Timeout, m_Auth));

    BeginReadTransmission();
}

void
CNetCache_MessageHandler::Process_IC_StoreBlob(void)
{
    WriteMsg("OK:", "");

    if (m_BlobSize == 0) {
        m_ICache->Store(m_Key, m_Version, m_SubKey, 0, 0, m_Timeout, m_Auth);
        return;
    }

    m_SizeKnown = true;

    m_Writer.reset(m_ICache->GetWriteStream(m_Key, m_Version, m_SubKey,
                                            m_Timeout, m_Auth));

    BeginReadTransmission();
}


void
CNetCache_MessageHandler::Process_IC_GetSize(void)
{
    size_t sz = m_ICache->GetSize(m_Key, m_Version, m_SubKey);
    string str;
    NStr::UIntToString(str, sz);
    WriteMsg("OK:", str);
}

void
CNetCache_MessageHandler::Process_IC_GetBlobOwner(void)
{
    string owner;
    m_ICache->GetBlobOwner(m_Key, m_Version, m_SubKey, &owner);
    WriteMsg("OK:", owner);
}

void
CNetCache_MessageHandler::Process_IC_Read(void)
{
    ICache::SBlobAccessDescr ba_descr;
    ba_descr.buf = 0;
    ba_descr.buf_size = 0;
    m_ICache->GetBlobAccess(m_Key, m_Version, m_SubKey, &ba_descr);

    if (!ba_descr.blob_found) {
        string msg = "BLOB not found. ";
        //msg += req_id;
        WriteMsg("ERR:", msg);
        return;
    }
    if (ba_descr.blob_size == 0) {
        WriteMsg("OK:", "BLOB found. SIZE=0");
        return;
    }
 
    // re-translate reader to the network

    m_Reader.reset(ba_descr.reader.release());
    if (!m_Reader.get()) {
        string msg = "BLOB not found. ";
        //msg += req_id;
        WriteMsg("ERR:", msg);
        return;
    }
    // Write first chunk right here
    char buf[4096];
    size_t bytes_read;
    ERW_Result io_res = m_Reader->Read(buf, sizeof(buf), &bytes_read);
    if (io_res != eRW_Success || !bytes_read) { // TODO: should we check here for bytes_read?
        string msg = "BLOB not found. ";
        WriteMsg("ERR:", msg);
        m_Reader.reset(0);
        return;
    }
    string msg("BLOB found. SIZE=");
    string sz;
    NStr::UIntToString(sz, ba_descr.blob_size);
    msg += sz;
    WriteMsg("OK:", msg);
    
    // translate BLOB fragment to the network
    CNetCacheServer::WriteBuf(GetSocket(), buf, bytes_read);

    // TODO: Can we check here that bytes_read is less than sizeof(buf)
    // and optimize out delayed write?
    BeginDelayedWrite();
}

void
CNetCache_MessageHandler::Process_IC_Remove(void)
{
    m_ICache->Remove(m_Key, m_Version, m_SubKey);
    WriteMsg("OK:", "");
}

void
CNetCache_MessageHandler::Process_IC_RemoveKey(void)
{
    m_ICache->Remove(m_Key);
    WriteMsg("OK:", "");
}

void
CNetCache_MessageHandler::Process_IC_GetAccessTime(void)
{
    time_t t = m_ICache->GetAccessTime(m_Key, m_Version, m_SubKey);
    string str;
    NStr::UIntToString(str, static_cast<unsigned long>(t));
    WriteMsg("OK:", str);
}

void
CNetCache_MessageHandler::Process_IC_HasBlobs(void)
{
    bool hb = m_ICache->HasBlobs(m_Key, m_SubKey);
    string str;
    NStr::UIntToString(str, (int)hb);
    WriteMsg("OK:", str);
}

void
CNetCache_MessageHandler::Process_IC_Purge1(void)
{
    ICache::EKeepVersions keep_versions = (ICache::EKeepVersions)m_Policy;
    m_ICache->Purge(m_Timeout, keep_versions);
    WriteMsg("OK:", "");
}


END_NCBI_SCOPE
