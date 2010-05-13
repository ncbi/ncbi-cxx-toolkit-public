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

#include <corelib/ncbi_param.hpp>
#include <corelib/request_ctx.hpp>
#include <corelib/ncbi_bswap.hpp>
#include <util/md5.hpp>

#include <connect/services/netcache_key.hpp>

#include "message_handler.hpp"
#include "netcache_version.hpp"
#include "error_codes.hpp"

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif


BEGIN_NCBI_SCOPE


#define NCBI_USE_ERRCODE_X  NetCache_Handler


/// Definition of all NetCache commands
CNCMessageHandler::SCommandDef s_CommandMap[] = {
    { "VERSION",
        {&CNCMessageHandler::x_DoCmd_Version,
            "VERSION"} },
    { "PUT3",
        {&CNCMessageHandler::x_DoCmd_Put3,
            "PUT3",          eWithAutoBlobKey, eCreate},
        { { "ttl",     eNSPT_Int,  eNSPA_Optional, "0" },
          { "key",     eNSPT_NCID, eNSPA_Optional },
          { "key_ver", eNSPT_Int,  eNSPA_Optional, "1" },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "GET2",
        {&CNCMessageHandler::x_DoCmd_Get,
            "GET2",          eWithBlobLock, eRead  },
        { { "key",     eNSPT_NCID, eNSPA_Required },
          { "NW",      eNSPT_Id,   eNSPA_Obsolete | fNSPA_Match },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "RMV2",
        {&CNCMessageHandler::x_DoCmd_Remove2,
            "RMV2",          eWithBlobLock, eWrite },
        { { "key",     eNSPT_NCID, eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "HASB",
        {&CNCMessageHandler::x_DoCmd_HasBlob,
            "HASB",          eNoBlobLock},
        { { "key",     eNSPT_NCID, eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "GSIZ",
        {&CNCMessageHandler::x_DoCmd_GetSize,
            "GetSIZe",       eWithBlobLock, eRead  },
        { { "key",     eNSPT_NCID, eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "STOR",
        {&CNCMessageHandler::x_DoCmd_IC_Store,
            "IC_STOR",       eWithBlobLock, eCreate},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "s_ttl",   eNSPT_Int,  eNSPA_Required },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "version", eNSPT_Int,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "STRS",
        {&CNCMessageHandler::x_DoCmd_IC_StoreBlob,
            "IC_STRS",       eWithBlobLock, eCreate},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "s_ttl",   eNSPT_Int,  eNSPA_Required },
          { "size",    eNSPT_Int,  eNSPA_Required },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "version", eNSPT_Int,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "READ",
        {&CNCMessageHandler::x_DoCmd_Get,
            "IC_READ",       eWithBlobLock, eRead  },
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "version", eNSPT_Int,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "HASB",
        {&CNCMessageHandler::x_DoCmd_HasBlob,
            "IC_HASB",       eNoBlobLock},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "version", eNSPT_Int,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "REMO",
        {&CNCMessageHandler::x_DoCmd_Remove2,
            "IC_REMOve",     eWithBlobLock, eWrite },
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "version", eNSPT_Int,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "GSIZ",
        {&CNCMessageHandler::x_DoCmd_IC_GetSize,
            "IC_GetSIZe",    eWithBlobLock, eRead  },
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "version", eNSPT_Int,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "A?",     {&CNCMessageHandler::x_DoCmd_Alive,  "A?"} },
    { "HEALTH", {&CNCMessageHandler::x_DoCmd_Health, "HEALTH"} },
    { "PUT2",
        {&CNCMessageHandler::x_DoCmd_Put2,
            "PUT2",          eWithAutoBlobKey, eCreate},
        { { "ttl",     eNSPT_Int,  eNSPA_Optional, "0" },
          { "key",     eNSPT_NCID, eNSPA_Optional },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "GET",
        {&CNCMessageHandler::x_DoCmd_Get,
            "GET",           eWithBlobLock, eRead  },
        { { "key",     eNSPT_NCID, eNSPA_Required },
          { "NW",      eNSPT_Id,   eNSPA_Obsolete | fNSPA_Match },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "REMOVE",
        {&CNCMessageHandler::x_DoCmd_Remove,
            "REMOVE",        eWithBlobLock, eWrite },
        { { "key",     eNSPT_NCID, eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "GBOW",
        {&CNCMessageHandler::x_DoCmd_GetOwner,
            "GetBOWner",     eWithBlobLock, eRead  },
        { { "key",     eNSPT_NCID, eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "GBLW",
        {&CNCMessageHandler::x_DoCmd_GetOwner,
            "IC_GetBLoWner", eWithBlobLock, eRead  },
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "version", eNSPT_Int,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "SHUTDOWN",
        {&CNCMessageHandler::x_DoCmd_Shutdown,       "SHUTDOWN"},
        { { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "MONI",
        {&CNCMessageHandler::x_DoCmd_Monitor,        "MONITOR"},
        { { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "GETSTAT",
        {&CNCMessageHandler::x_DoCmd_GetServerStats, "GETSTAT"},
        { { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "GETCONF",
        {&CNCMessageHandler::x_DoCmd_GetConfig,      "GETCONF"},
        { { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "GTOU",
        {&CNCMessageHandler::x_DoCmd_IC_GetBlobsTTL,
            "IC_GetTimOUt",  eNoBlobLock},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "GACT",
        {&CNCMessageHandler::x_DoCmd_IC_GetAccessTime,
            "IC_GetACcTime", eWithBlobLock, eRead},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "version", eNSPT_Int,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "REINIT",
        {&CNCMessageHandler::x_DoCmd_Reinit,
            "REINIT",        eNoBlobLock},
        { { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "REINIT",
        {&CNCMessageHandler::x_DoCmd_Reinit,
            "IC_REINIT",     eNoBlobLock},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "RECONF",
        {&CNCMessageHandler::x_DoCmd_Reconf, "RECONF"},
        { { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "OK",
        {&CNCMessageHandler::x_DoCmd_OK,     "OK"} },
    { "ISOP",
        {&CNCMessageHandler::x_DoCmd_IC_IsOpen,
            "ISOpen",        eNoBlobLock},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "LOG",      {&CNCMessageHandler::x_DoCmd_NotImplemented, "LOG"} },
    { "STAT",     {&CNCMessageHandler::x_DoCmd_NotImplemented, "STAT"} },
    { "DROPSTAT", {&CNCMessageHandler::x_DoCmd_NotImplemented, "DROPSTAT"} },
    { "PUT",      {&CNCMessageHandler::x_DoCmd_NotImplemented, "PUT"} },
    { "SMR",      {&CNCMessageHandler::x_DoCmd_NotImplemented, "SMR"} },
    { "SMU",      {&CNCMessageHandler::x_DoCmd_NotImplemented, "SMU"} },
    { "STSP",     {&CNCMessageHandler::x_DoCmd_NotImplemented, "IC_STSP"},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix } } },
    { "GTSP",     {&CNCMessageHandler::x_DoCmd_NotImplemented, "IC_GTSP"},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix } } },
    { "SVRP",     {&CNCMessageHandler::x_DoCmd_NotImplemented, "IC_SVRP"},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix } } },
    { "GVRP",     {&CNCMessageHandler::x_DoCmd_NotImplemented, "IC_GVRP"},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix } } },
    { "PRG1",     {&CNCMessageHandler::x_DoCmd_NotImplemented, "IC_PRG1"},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix } } },
    { "REMK",     {&CNCMessageHandler::x_DoCmd_NotImplemented, "IC_REMK"},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix } } },
    { "ISLK",     {&CNCMessageHandler::x_DoCmd_NotImplemented, "ISLocKed"} },
    { NULL }
};




void
CBufferedSockReaderWriter::x_ReadCRLF(bool force_read)
{
    if (HasDataToRead()  &&  (m_CRLFMet != eNone  ||  force_read)) {
        for (; HasDataToRead()  &&  m_CRLFMet != eCRLF; ++m_ReadDataPos) {
            char c = m_ReadBuff[m_ReadDataPos];
            if (c == '\n'  &&  (m_CRLFMet & eLF) == 0)
                m_CRLFMet += eLF;
            else if (c == '\r'  &&  (m_CRLFMet & eCR) == 0)
                m_CRLFMet += eCR;
            else if (c == '\0')
                m_CRLFMet = eCRLF;
            else
                break;
        }
        if (m_CRLFMet == eCRLF  ||  HasDataToRead()) {
            m_CRLFMet = eNone;
        }
    }
}

inline void
CBufferedSockReaderWriter::x_CompactBuffer(TBufferType& buffer, size_t& pos)
{
    if (pos != 0) {
        size_t new_size = buffer.size() - pos;
        memmove(buffer.data(), buffer.data() + pos, new_size);
        buffer.resize(new_size);
        pos = 0;
    }
}

bool
CBufferedSockReaderWriter::Read(void)
{
    _ASSERT(m_Socket);

    x_CompactBuffer(m_ReadBuff, m_ReadDataPos);

    size_t cnt_read = 0;
    EIO_Status status;
    {{
        CReadWriteTimeoutGuard guard(this);
        status = m_Socket->Read(m_ReadBuff.data() + m_ReadBuff.size(),
                                m_ReadBuff.capacity() - m_ReadBuff.size(),
                                &cnt_read,
                                eIO_ReadPlain);
    }}

    switch (status) {
    case eIO_Success:
    case eIO_Timeout:
        break;
    case eIO_Closed:
        return m_ReadDataPos < m_ReadBuff.size();
    default:
        NCBI_IO_CHECK(status);
    }
    m_ReadBuff.resize(m_ReadBuff.size() + cnt_read);
    x_ReadCRLF(false);
    return HasDataToRead();
}

bool
CBufferedSockReaderWriter::ReadLine(string& line)
{
    if (!Read()) {
        return false;
    }

    size_t crlf_pos;
    for (crlf_pos = m_ReadDataPos; crlf_pos < m_ReadBuff.size(); ++crlf_pos)
    {
        char c = m_ReadBuff[crlf_pos];
        if (c == '\n'  ||  c == '\r'  ||  c == '\0') {
            break;
        }
    }
    if (crlf_pos >= m_ReadBuff.size()) {
        return false;
    }

    line.assign(m_ReadBuff.data() + m_ReadDataPos, crlf_pos - m_ReadDataPos);
    m_ReadDataPos = crlf_pos;
    x_ReadCRLF(true);
    return true;
}

void
CBufferedSockReaderWriter::Flush(void)
{
    if (!m_Socket  ||  !IsWriteDataPending()) {
        return;
    }

    size_t n_written = 0;
    EIO_Status status;
    {{
        CReadWriteTimeoutGuard guard(this);
        status = m_Socket->Write(m_WriteBuff.data() + m_WriteDataPos,
                                 m_WriteBuff.size() - m_WriteDataPos,
                                 &n_written,
                                 eIO_WritePlain);
    }}

    switch (status) {
    case eIO_Success:
    case eIO_Timeout:
    case eIO_Interrupt:
        break;
    default:
        NCBI_IO_CHECK(status);
        break;
    } // switch

    m_WriteDataPos += n_written;
    x_CompactBuffer(m_WriteBuff, m_WriteDataPos);
}

void
CBufferedSockReaderWriter::WriteMessage(CTempString prefix, CTempString msg)
{
    size_t msg_size = prefix.size() + msg.size() + 1;
    m_WriteBuff.resize(m_WriteBuff.size() + msg_size);
    void* data = m_WriteBuff.data() + m_WriteBuff.size() - msg_size;
    memcpy(data, prefix.data(), prefix.size());
    data = (char*)data + prefix.size();
    memcpy(data, msg.data(), msg.size());
    data = (char*)data + msg.size();
    *(char*)data = '\n';
}


/////////////////////////////////////////////////////////////////////////////
// CNCMessageHandler implementation
CNCMessageHandler::CNCMessageHandler(CNetCacheServer* server)
    : m_Server(server),
      m_Monitor(server->GetServerMonitor()),
      m_Socket(NULL),
      m_State(eSocketClosed),
      m_Parser(s_CommandMap),
      m_CmdProcessor(NULL),
      m_CurBlob(NULL)
{}

CNCMessageHandler::~CNCMessageHandler(void)
{}

void
CNCMessageHandler::OnOpen(void)
{
    _ASSERT(x_GetState() == eSocketClosed);

    m_Socket->DisableOSSendDelay();
    STimeout to = {m_Server->GetInactivityTimeout(), 0};
    m_SockBuffer.ResetSocket(m_Socket, &to);

    m_ClientAddress = m_Socket->GetPeerAddress();
    m_ConnTime = m_Server->GetFastTime();

    x_SetState(ePreAuthenticated);

    x_MonitorPost("Connection is opened");
    CNCServerStat::AddOpenedConnection();
}

void
CNCMessageHandler::OnTimeout(void)
{
    CDiagnosticsGuard guard(this);

    LOG_POST(Info << "Inactivity timeout expired, closing connection");
}

void
CNCMessageHandler::OnTimer(void)
{
    if (!x_IsFlagSet(fCommandStarted))
        return;

    CDiagnosticsGuard guard(this);

    LOG_POST("Command execution timed out. Closing connection.");
    CNCServerStat::AddTimedOutCommand();
    m_DiagContext->SetRequestStatus(eStatus_CmdTimeout);
    x_CloseConnection();
}

void
CNCMessageHandler::OnOverflow(void)
{
    // Max connection overflow
    ERR_POST("Max number of connections reached, closing connection");
    CNCServerStat::AddOverflowConnection();
}

void
CNCMessageHandler::OnClose(IServer_ConnectionHandler::EClosePeer peer)
{
    CDiagnosticsGuard guard(this);

    switch (peer)
    {
    case IServer_ConnectionHandler::eOurClose:
        x_MonitorPost("NetCache closing connection.");
        break;
    case IServer_ConnectionHandler::eClientClose:
        if (m_DiagContext.NotNull()) {
            m_DiagContext->SetRequestStatus(eStatus_BadCmd);
        }
        x_MonitorPost("Client closed connection to NetCache.");
        break;
    default:
        break;
    }

    // Hack-ish fix to make cursed PUT2 command to work - it uses connection
    // closing as legal end-of-blob signal.
    if (x_GetState() == eReadBlobChunkLength
        &&  NStr::strcmp(m_CurCmd, "PUT2") == 0)
    {
        _ASSERT(m_CurBlob  &&  !m_CurBlob->IsFinalized()  &&  m_DiagContext);
        m_DiagContext->SetRequestStatus(eStatus_OK);
        x_FinishReadingBlob();
    }
    x_FinishCommand();

    if (x_GetState() == ePreAuthenticated) {
        CNCServerStat::RemoveOpenedConnection();
    }
    else {
        double conn_span = (m_Server->GetFastTime() - m_ConnTime).GetAsDouble();
        CNCServerStat::AddClosedConnection(conn_span);
    }

    x_SetState(eSocketClosed);
    m_SockBuffer.ResetSocket(NULL, NULL);
    m_ClientName.clear();
}

void
CNCMessageHandler::OnRead(void)
{
    x_ManageCmdPipeline();
}

void
CNCMessageHandler::OnWrite(void)
{
    x_ManageCmdPipeline();
}

EIO_Event
CNCMessageHandler::GetEventsToPollFor(const CTime** alarm_time) const
{
    if (alarm_time  &&  x_IsFlagSet(fCommandStarted)) {
        *alarm_time = &m_MaxCmdTime;
    }
    if (x_GetState() == eWriteBlobData
        ||  m_SockBuffer.IsWriteDataPending())
    {
        return eIO_Write;
    }
    return eIO_Read;
}

bool
CNCMessageHandler::IsReadyToProcess(void) const
{
    EStates state = x_GetState();
    if (state == eWaitForStorageBlock) {
        return m_Storage->CanDoExclusive();
    }
    else if (state == eWaitForServerBlock) {
        return m_Server->CanReconfigure();
    }
    else {
        return m_BlobLock.IsNull()  ||  m_BlobLock->IsLockAcquired();
    }
}

inline bool
CNCMessageHandler::x_ReadAuthMessage(void)
{
    if (!m_SockBuffer.ReadLine(m_ClientName)) {
        return false;
    }

    m_State = eReadyForCommand;
    return true;
}

inline void
CNCMessageHandler::x_CheckAdminClient(void)
{
    if (m_ClientName != m_Server->GetAdminClient()) {
        NCBI_THROW(CNSProtoParserException, eWrongCommand,
                   "Command is not accessible");
    }
}

inline void
CNCMessageHandler::x_AssignCmdParams(map<string, string>& params)
{
    m_BlobKey.clear();
    m_BlobSubkey.clear();
    m_BlobVersion = 0;
    m_BlobPass.clear();
    m_KeyVersion = 1;

    string cache_name(kNCDefCacheName);

    typedef map<string, string> TMap;
    NON_CONST_ITERATE(TMap, it, params) {
        const string& key = it->first;
        string& val = it->second;

        switch (key[0]) {
        case 'c':
            if (key == "cache") {
                cache_name = val;
            }
            break;
        case 'i':
            if (key == "ip") {
                _ASSERT(m_DiagContext.NotNull());
                if (!val.empty()) {
                    m_DiagContext->SetClientIP(val);
                }
            }
            break;
        case 'k':
            if (key == "key") {
                m_BlobKey = val;
            }
            else if (key == "key_ver") {
                m_KeyVersion = NStr::StringToUInt(val);
            }
            break;
        case 'p':
            if (key == "pass") {
                CMD5 md5;
                md5.Update(val.data(), val.size());
                val = md5.GetHexSum();
                m_BlobPass = val;
            }
            break;
        case 's':
            if (key == "sid") {
                _ASSERT(m_DiagContext.NotNull());
                if (!val.empty()) {
                    m_DiagContext->SetSessionID(NStr::URLDecode(val));
                }
            }
            else if (key == "subkey") {
                m_BlobSubkey = val;
            }
            else if (key == "size") {
                m_StoreBlobSize = NStr::StringToUInt(val);
            }
            else if (key == "s_ttl") {
                m_BlobTTL = NStr::StringToUInt(val);
            }
            break;
        case 't':
            if (key == "ttl") {
                m_BlobTTL = NStr::StringToUInt(val);
            }
            break;
        case 'v':
            if (key == "version") {
                m_BlobVersion = NStr::StringToUInt(val);
            }
            break;
        default:
            break;
        }
    }
    InitDiagnostics();

    m_Storage = m_Server->GetBlobStorage(cache_name);
}

inline void
CNCMessageHandler::x_PrintRequestStart(const SParsedCmd& cmd)
{
    ENCLogCmdsType log_type = m_Server->GetLogCmdsType();
    if (log_type == eLogAllCmds
        ||  (log_type == eNoLogHasb
             &&  m_CmdProcessor != &CNCMessageHandler::x_DoCmd_HasBlob))
    {
        CDiagContext_Extra diag_extra = GetDiagContext().PrintRequestStart();
        diag_extra.Print("cmd",  cmd.command->cmd);
        diag_extra.Print("auth", m_ClientName);
        diag_extra.Print("peer", m_ClientAddress);
        typedef map<string, string> TStringMap;
        ITERATE(TStringMap, it, cmd.params) {
            diag_extra.Print(it->first, it->second);
        }

        x_SetFlag(fCommandPrinted);
    }
}

inline void
CNCMessageHandler::x_StartCommand(SParsedCmd& cmd)
{
    x_SetFlag(fCommandStarted);

    const SCommandExtra& cmd_extra = cmd.command->extra;
    m_CmdProcessor = cmd_extra.processor;
    m_CurCmd       = cmd_extra.cmd_name;

    m_MaxCmdTime = m_CmdStartTime = m_Server->GetFastTime();
    m_MaxCmdTime.AddSecond(m_Server->GetCmdTimeout(), CTime::eIgnoreDaylight);

    x_PrintRequestStart(cmd);
    // PrintRequestStart() resets status value, so setting default status
    // should go here, though more logical is in x_ReadCommand()
    m_DiagContext->SetRequestStatus(eStatus_OK);

    if (cmd_extra.storage_access != eNoStorage  &&  !m_Storage) {
        map<string, string>::const_iterator it = cmd.params.find("cache");
        string cache_name = (it == cmd.params.end()? kNCDefCacheName
                                                   : it->second);
        NCBI_THROW_FMT(CNSProtoParserException, eWrongParams,
                       "Cache unknown: '" << cache_name << "'");
    }
    if (cmd_extra.storage_access == eWithAutoBlobKey  &&  m_BlobKey.empty()) {
        CNetCacheKey::GenerateBlobKey(&m_BlobKey,
                                      m_Server->GetNextBlobId(),
                                      m_Server->GetHost(),
                                      m_Server->GetPort(),
                                      m_KeyVersion);

        if (x_IsFlagSet(fCommandPrinted)) {
            CDiagContext_Extra diag_extra = GetDiagContext().Extra();
            diag_extra.Print("gen_key", m_BlobKey);
        }
    }

    if (cmd_extra.storage_access == eWithBlobLock
        ||  cmd_extra.storage_access == eWithAutoBlobKey)
    {
        if ((m_BlobPass.empty()  &&  m_Server->GetBlobPassPolicy() == eNCOnlyWithPass)
            ||  (!m_BlobPass.empty()  &&  m_Server->GetBlobPassPolicy() == eNCOnlyWithoutPass))
        {
            NCBI_THROW(CNSProtoParserException, eWrongParams,
                       "Password in the command doesn't match server settings.");
        }

        m_BlobLock = m_Storage->GetBlobAccess(cmd_extra.blob_access,
                                              m_BlobKey,
                                              m_BlobSubkey,
                                              m_BlobVersion,
                                              m_BlobPass);
        x_SetState(eWaitForBlobLock);
    }
}

inline bool
CNCMessageHandler::x_ReadCommand(void)
{
    string cmd_line;
    if (!m_SockBuffer.ReadLine(cmd_line)) {
        return false;
    }

    x_MonitorPost("Incoming command: " + cmd_line);

    x_SetState(eCommandReceived);
    SParsedCmd cmd = m_Parser.ParseCommand(cmd_line);

    m_DiagContext.Reset(new CRequestContext());
    m_DiagContext->SetRequestID();

    x_AssignCmdParams(cmd.params);
    x_StartCommand(cmd);

    return true;
}

inline bool
CNCMessageHandler::x_WaitForBlobLock(void)
{
    if (m_BlobLock->IsLockAcquired()) {
        x_SetState(m_BlobLock->IsAuthorized()? eCommandReceived: ePasswordFailed);
        return true;
    }
    else {
        m_Server->DeferConnectionProcessing(m_Socket);
        return false;
    }
}

inline bool
CNCMessageHandler::x_ProcessBadPassword(void)
{
    if (m_BlobLock->GetAccessType() == eCreate) {
        m_SockBuffer.WriteMessage("ERR:", "Error occurred while writing blob data");
    }
    else {
        m_SockBuffer.WriteMessage("ERR:", "BLOB not found.");
    }
    m_SockBuffer.Flush();
    m_DiagContext->SetRequestStatus(eStatus_BadPassword);
    ERR_POST_X(5, Warning << "Incorrect password is used to access the blob");
    x_SetState(eReadyForCommand);
    if (m_BlobLock->GetAccessType() == eCreate) {
        x_CloseConnection();
    }
    return true;
}

inline bool
CNCMessageHandler::x_WaitForStorageBlock(void)
{
    if (m_Storage->CanDoExclusive()) {
        x_SetState(eCommandReceived);
        return true;
    }
    else {
        m_Server->DeferConnectionProcessing(m_Socket);
        return false;
    }
}

inline bool
CNCMessageHandler::x_WaitForServerBlock(void)
{
    if (m_Server->CanReconfigure()) {
        x_SetState(eCommandReceived);
        return true;
    }
    else {
        m_Server->DeferConnectionProcessing(m_Socket);
        return false;
    }
}

inline bool
CNCMessageHandler::x_DoCommand(void)
{
    bool do_next = (this->*m_CmdProcessor)();
    if (do_next  &&  x_GetState() == eCommandReceived) {
        x_SetState(eReadyForCommand);
    }
    return do_next;
}

void
CNCMessageHandler::x_FinishCommand(void)
{
    int cmd_status = 0;
    if (m_DiagContext.NotNull()) {
        cmd_status = m_DiagContext->GetRequestStatus();
        // Context is reseted early but it is still in diagnostics
        m_DiagContext.Reset();
    }

    if (!x_IsFlagSet(fCommandStarted)) {
        return;
    }

    if (x_IsMonitored()) {
        CQuickStrStream msg;
        msg << "Command finished";
        if (m_CurBlob) {
            msg << " for blob '" << m_BlobKey << "', '" << m_BlobSubkey
                << "', " << m_BlobVersion << ", size="
                << m_CurBlob->GetSize();
        }
        x_MonitorPost(msg);
    }

    size_t blob_size = 0;
    bool was_blob_access = m_BlobLock.NotNull();
    if (was_blob_access) {
        if (m_BlobLock->GetAccessType() == eWrite) {
            // Blob was deleted, logging of size is not necessary
            was_blob_access = false;
        }
        else {
            blob_size = m_BlobLock->GetBlobSize();
        }
        m_CurBlob = NULL;
        m_BlobLock->ReleaseLock();
        m_BlobLock.Reset();
    }

    if (x_IsFlagSet(fConfirmBlobPut)) {
        m_SockBuffer.WriteMessage("OK:", "");
    }

    if (x_IsFlagSet(fCommandPrinted)) {
        if (was_blob_access
            &&  (cmd_status == eStatus_OK  ||  (cmd_status == eStatus_BadCmd
                                                &&  blob_size != 0)))
        {
            CDiagContext_Extra diag_extra = GetDiagContext().Extra();
            diag_extra.Print("size", NStr::UInt8ToString(blob_size));
        }
        GetDiagContext().PrintRequestStop();
        x_UnsetFlag(fCommandPrinted);
    }

    x_UnsetFlag(fCommandStarted);
    x_UnsetFlag(fConfirmBlobPut);
    x_UnsetFlag(fReadExactBlobSize);

    double cmd_span = (m_Server->GetFastTime() - m_CmdStartTime).GetAsDouble();
    CNCServerStat::AddFinishedCmd(m_CurCmd, cmd_span, cmd_status);

    ResetDiagnostics();
}



inline void
CNCMessageHandler::x_StartReadingBlob(void)
{
    m_CurBlob = m_BlobLock->GetBlob();
    m_SockBuffer.Flush();

    x_SetState(eReadBlobSignature);
}

void
CNCMessageHandler::x_FinishReadingBlob(void)
{
    if (x_IsFlagSet(fReadExactBlobSize)
        &&  m_CurBlob->GetSize() != m_StoreBlobSize)
    {
        m_DiagContext->SetRequestStatus(eStatus_CondFailed);
        NCBI_THROW(CNetServiceException, eProtocolError,
                   "Too few data for blob size "
                   + NStr::UInt8ToString(m_StoreBlobSize) + " (received "
                   + NStr::UInt8ToString(m_CurBlob->GetSize()) + " bytes)");
    }

    m_CurBlob->Finalize();
    x_SetState(eReadyForCommand);
}

inline bool
CNCMessageHandler::x_ReadBlobSignature(void)
{
    if (m_SockBuffer.GetReadSize() < 4) {
        m_SockBuffer.Read();
    }
    if (m_SockBuffer.GetReadSize() < 4) {
        return false;
    }

    Uint4 sig = *reinterpret_cast<const Uint4*>(m_SockBuffer.GetReadData());
    m_SockBuffer.EraseReadData(4);

    if (sig == 0x04030201) {
        x_SetFlag(fSwapLengthBytes);
    }
    else if (sig == 0x01020304) {
        x_UnsetFlag(fSwapLengthBytes);
    }
    else {
        m_DiagContext->SetRequestStatus(eStatus_BadCmd);
        NCBI_THROW(CUtilException, eWrongData,
                   "Cannot determine the byte order. Got: " +
                   NStr::UIntToString(sig, 0, 16));
    }

    x_SetState(eReadBlobChunkLength);
    return true;
}

inline bool
CNCMessageHandler::x_ReadBlobChunkLength(void)
{
    if (x_IsFlagSet(fReadExactBlobSize)
        &&  m_CurBlob->GetSize() == m_StoreBlobSize)
    {
        // Workaround for old STRS
        m_ChunkLen = 0xFFFFFFFF;
    }
    else {
        if (m_SockBuffer.GetReadSize() < 4) {
            m_SockBuffer.Read();
        }
        if (m_SockBuffer.GetReadSize() < 4) {
            return false;
        }

        m_ChunkLen = *reinterpret_cast<const Uint4*>(
                                                  m_SockBuffer.GetReadData());
        m_SockBuffer.EraseReadData(4);
        if (x_IsFlagSet(fSwapLengthBytes)) {
            m_ChunkLen = CByteSwap::GetInt4(
                         reinterpret_cast<const unsigned char*>(&m_ChunkLen));
        }
    }

    if (m_ChunkLen == 0xFFFFFFFF) {
        x_FinishReadingBlob();
    }
    else if (x_IsFlagSet(fReadExactBlobSize)
             &&  m_CurBlob->GetSize() + m_ChunkLen > m_StoreBlobSize)
    {
        m_DiagContext->SetRequestStatus(eStatus_CondFailed);
        NCBI_THROW(CNetServiceException, eProtocolError,
                   "Too much data for blob size "
                   + NStr::UInt8ToString(m_StoreBlobSize)
                   + " (received at least "
                   + NStr::UInt8ToString(m_CurBlob->GetSize() + m_ChunkLen)
                   + " bytes)");
    }
    else {
        x_SetState(eReadBlobChunk);
    }
    return true;
}

inline bool
CNCMessageHandler::x_ReadBlobChunk(void)
{
    while (m_ChunkLen != 0) {
        if (!m_SockBuffer.HasDataToRead()  &&  !m_SockBuffer.Read()) {
            return false;
        }

        size_t read_len = min(m_SockBuffer.GetReadSize(), size_t(m_ChunkLen));
        m_CurBlob->Write(m_SockBuffer.GetReadData(), read_len);
        m_SockBuffer.EraseReadData(read_len);
        m_ChunkLen -= read_len;
    }

    x_SetState(eReadBlobChunkLength);
    return true;
}

inline void
CNCMessageHandler::x_StartWritingBlob(void)
{
    if (!m_BlobLock->IsBlobExists()) {
        m_DiagContext->SetRequestStatus(eStatus_NotFound);
        m_SockBuffer.WriteMessage("ERR:", "BLOB not found.");
        return;
    }

    m_CurBlob = m_BlobLock->GetBlob();
    size_t blob_size = m_CurBlob->GetSize();
    m_SockBuffer.WriteMessage("OK:", "BLOB found. SIZE="
                                     + NStr::UInt8ToString(blob_size));

    if (blob_size != 0) {
        x_SetState(eWriteBlobData);
    }
}

inline bool
CNCMessageHandler::x_WriteBlobData(void)
{
    do {
        size_t n_read = m_CurBlob->Read(m_SockBuffer.PrepareDirectWrite(),
                                        m_SockBuffer.GetDirectWriteSize());
        if (n_read == 0) {
            x_SetState(eReadyForCommand);
            return true;
        }
        m_SockBuffer.AddDirectWritten(n_read);
        m_SockBuffer.Flush();
    }
    while (!m_SockBuffer.IsWriteDataPending());

    return false;
}

void
CNCMessageHandler::x_MonitorPost(const string& msg, bool do_trace /*= true*/)
{
    if (!x_IsMonitored()) {
        return;
    }

    CQuickStrStream trace_msg;
    trace_msg << m_ClientName << " " << m_ClientAddress << " (from "
              << m_ConnTime << ")\n\t" << msg << "\n";

    if (do_trace) {
        LOG_POST(Trace << trace_msg);
    }
    if (m_Monitor  &&  m_Monitor->IsMonitorActive()) {
        CQuickStrStream send_msg;
        send_msg << m_Server->GetFastTime() << "\n\t" << trace_msg;
        m_Monitor->SendString(send_msg);
    }
}

inline void
CNCMessageHandler::x_ReportException(const exception& ex, CTempString msg)
{
    string post_msg(msg);
    post_msg += ": ";
    post_msg += ex.what();

    ERR_POST_X(3, post_msg);
    x_MonitorPost(post_msg, false);
}

#ifdef _DEBUG

inline string
CNCMessageHandler::GetStateName(int state)
{
    switch (EStates(state))
    {
    case ePreAuthenticated:    return "ePreAuthenticated";
    case eReadyForCommand:     return "eReadyForCommand";
    case eCommandReceived:     return "eCommandReceived";
    case eWaitForBlobLock:     return "eWaitForBlobLock";
    case ePasswordFailed:      return "ePasswordFailed";
    case eWaitForStorageBlock: return "eWaitForStorageBlock";
    case eWaitForServerBlock:  return "eWaitForServerBlock";
    case eReadBlobSignature:   return "eReadBlobSignature";
    case eReadBlobChunkLength: return "eReadBlobChunkLength";
    case eReadBlobChunk:       return "eReadBlobChunk";
    case eWriteBlobData:       return "eWriteBlobData";
    case eSocketClosed:        return "eSocketClosed";
    }

    return "Unknown state";
}

#endif  // _DEBUG

void
CNCMessageHandler::x_ManageCmdPipeline(void)
{
    CDiagnosticsGuard guard(this);

    _TRACE("Entered x_ManageCmdPipeline, state=" << GetStateName(x_GetState()));
    bool do_next_step = false;
    do {
        try {
            // These two commands will be effectively ignored on all cycles
            // other than first. But they should be here for unification
            // of code.
            m_SockBuffer.Flush();
            if (m_SockBuffer.IsWriteDataPending()) {
                return;
            }

            switch (x_GetState()) {
            case ePreAuthenticated:
                do_next_step = x_ReadAuthMessage();
                break;
            case eReadyForCommand:
                x_FinishCommand();
                do_next_step = x_ReadCommand();
                break;
            case eCommandReceived:
                do_next_step = x_DoCommand();
                break;
            case eWaitForBlobLock:
                do_next_step = x_WaitForBlobLock();
                break;
            case ePasswordFailed:
                do_next_step = x_ProcessBadPassword();
                break;
            case eWaitForStorageBlock:
                do_next_step = x_WaitForStorageBlock();
                break;
            case eWaitForServerBlock:
                do_next_step = x_WaitForServerBlock();
                break;
            case eReadBlobSignature:
                do_next_step = x_ReadBlobSignature();
                break;
            case eReadBlobChunkLength:
                do_next_step = x_ReadBlobChunkLength();
                break;
            case eReadBlobChunk:
                do_next_step = x_ReadBlobChunk();
                break;
            case eWriteBlobData:
                do_next_step = x_WriteBlobData();
                break;
            default:
                ERR_POST_X(2, Critical
                           << "Fatal: Handler is in unexpected state - "
                           << m_State << ". Closing connection.");
                x_CloseConnection();
            }

            m_SockBuffer.Flush();
        }
        catch (CIO_Exception& ex) {
            if (m_DiagContext.NotNull()) {
                m_DiagContext->SetRequestStatus(eStatus_ServerError);
            }
            x_ReportException(ex, "IO exception in command");
            if (ex.GetErrCode() != CIO_Exception::eClosed) {
                x_CloseConnection();
            }
        }
        catch (exception& ex) {
            x_ReportException(ex, "Non-IO exception in command");
            if (x_GetState() == eCommandReceived) {
                if (m_DiagContext.NotNull()
                    &&  m_DiagContext->GetRequestStatus() == eStatus_OK)
                {
                    m_DiagContext->SetRequestStatus(eStatus_BadCmd);
                }
                m_SockBuffer.WriteMessage("ERR:",
                                         NStr::Replace(ex.what(), "\n", ";"));
                if (m_BlobLock.NotNull()  &&  m_BlobLock->GetAccessType() != eCreate)
                {
                    x_SetState(eReadyForCommand);
                    do_next_step = true;
                }
                else {
                    m_SockBuffer.Flush();
                    x_CloseConnection();
                }
            }
            else {
                if (m_DiagContext.NotNull()
                    &&  m_DiagContext->GetRequestStatus() == eStatus_OK)
                {
                    m_DiagContext->SetRequestStatus(eStatus_ServerError);
                }
                x_CloseConnection();
            }
        }
    }
    while (!m_SockBuffer.IsWriteDataPending()  &&  do_next_step
           &&  x_GetState() != eSocketClosed);

    _TRACE("Leaving x_ManageCmdPipeline, state=" << GetStateName(x_GetState()));
}

bool
CNCMessageHandler::x_DoCmd_Alive(void)
{
    m_SockBuffer.WriteMessage("OK:", "");
    return true;
}

bool
CNCMessageHandler::x_DoCmd_OK(void)
{
    return true;
}

bool
CNCMessageHandler::x_DoCmd_Health(void)
{
    m_SockBuffer.WriteMessage("OK:", "100%");
    m_SockBuffer.WriteMessage("OK:", "END");
    return true;
}

bool
CNCMessageHandler::x_DoCmd_Monitor(void)
{
    m_Socket->DisableOSSendDelay(false);
    m_SockBuffer.WriteMessage("OK:", "Monitor for " NETCACHED_VERSION "\n");
    m_Monitor->SetSocket(*m_Socket);
    x_CloseConnection();
    return true;
}

bool
CNCMessageHandler::x_DoCmd_Shutdown(void)
{
    x_CheckAdminClient();
    m_Server->RequestShutdown();
    m_SockBuffer.WriteMessage("OK:", "");
    return true;
}

bool
CNCMessageHandler::x_DoCmd_Version(void)
{
    m_SockBuffer.WriteMessage("OK:", NETCACHED_VERSION);
    return true;
}

AutoPtr<CConn_SocketStream>
CNCMessageHandler::x_PrepareSockStream(void)
{
    SOCK sk = m_Socket->GetSOCK();
    m_Socket->SetOwnership(eNoOwnership);
    m_Socket->Reset(0, eTakeOwnership, eCopyTimeoutsToSOCK);
    x_CloseConnection();

    return new CConn_SocketStream(sk, eTakeOwnership);
}

bool
CNCMessageHandler::x_DoCmd_GetConfig(void)
{
    m_Server->GetRegistry().Write(*x_PrepareSockStream());
    return true;
}

bool
CNCMessageHandler::x_DoCmd_GetServerStats(void)
{
    m_Server->PrintServerStats(x_PrepareSockStream().get());
    return true;
}

bool
CNCMessageHandler::x_DoCmd_Reinit(void)
{
    x_CheckAdminClient();
    m_Storage->Block();
    x_SetState(eWaitForStorageBlock);
    m_CmdProcessor = &CNCMessageHandler::x_DoCmd_ReinitImpl;
    return true;
}

bool
CNCMessageHandler::x_DoCmd_ReinitImpl(void)
{
    m_Storage->Reinitialize();
    m_Storage->Unblock();
    m_SockBuffer.WriteMessage("OK:", "");
    return true;
}

bool
CNCMessageHandler::x_DoCmd_Reconf(void)
{
    x_CheckAdminClient();
    m_Server->BlockAllStorages();
    x_SetState(eWaitForServerBlock);
    m_CmdProcessor = &CNCMessageHandler::x_DoCmd_ReconfImpl;
    return true;
}

bool
CNCMessageHandler::x_DoCmd_ReconfImpl(void)
{
    m_Server->Reconfigure();
    m_Server->UnblockAllStorages();
    m_SockBuffer.WriteMessage("OK:", "");
    return true;
}

bool
CNCMessageHandler::x_DoCmd_Put2(void)
{
    m_BlobLock->SetBlobTTL(m_BlobTTL);
    m_BlobLock->SetBlobOwner(m_ClientName);

    m_SockBuffer.WriteMessage("ID:", m_BlobKey);
    x_StartReadingBlob();
    return true;
}

bool
CNCMessageHandler::x_DoCmd_Put3(void)
{
    x_SetFlag(fConfirmBlobPut);
    return x_DoCmd_Put2();
}

bool
CNCMessageHandler::x_DoCmd_Get(void)
{
    x_StartWritingBlob();
    return true;
}

bool
CNCMessageHandler::x_DoCmd_GetSize(void)
{
    if (!m_BlobLock->IsBlobExists()) {
        m_DiagContext->SetRequestStatus(eStatus_NotFound);
        m_SockBuffer.WriteMessage("ERR:", "BLOB not found.");
    }
    else {
        size_t size = m_BlobLock->GetBlobSize();
        m_SockBuffer.WriteMessage("OK:", NStr::UInt8ToString(size));
    }

    return true;
}

bool
CNCMessageHandler::x_DoCmd_GetOwner(void)
{
    m_SockBuffer.WriteMessage("OK:", m_BlobLock->GetBlobOwner());
    return true;
}

bool
CNCMessageHandler::x_DoCmd_HasBlob(void)
{
    bool hb = m_Storage->IsBlobFamilyExists(m_BlobKey, m_BlobSubkey);
    m_SockBuffer.WriteMessage("OK:", NStr::IntToString((int)hb));
    return true;
}

bool
CNCMessageHandler::x_DoCmd_Remove(void)
{
    m_BlobLock->DeleteBlob();
    return true;
}

bool
CNCMessageHandler::x_DoCmd_Remove2(void)
{
    x_DoCmd_Remove();
    m_SockBuffer.WriteMessage("OK:", "");
    return true;
}

bool
CNCMessageHandler::x_DoCmd_IC_GetBlobsTTL(void)
{
    int to = m_Storage->GetDefBlobTTL();
    m_SockBuffer.WriteMessage("OK:", NStr::IntToString(to));
    return true;
}

bool
CNCMessageHandler::x_DoCmd_IC_IsOpen(void)
{
    m_SockBuffer.WriteMessage("OK:", "1");
    return true;
}

bool
CNCMessageHandler::x_DoCmd_IC_Store(void)
{
    m_BlobLock->SetBlobTTL(m_BlobTTL);
    m_BlobLock->SetBlobOwner(m_ClientName);

    m_SockBuffer.WriteMessage("OK:", "");
    x_StartReadingBlob();
    return true;
}

bool
CNCMessageHandler::x_DoCmd_IC_StoreBlob(void)
{
    m_BlobLock->SetBlobTTL(m_BlobTTL);
    m_BlobLock->SetBlobOwner(m_ClientName);

    m_SockBuffer.WriteMessage("OK:", "");
    if (m_StoreBlobSize != 0) {
        x_SetFlag(fReadExactBlobSize);
        x_StartReadingBlob();
    }
    return true;
}

bool
CNCMessageHandler::x_DoCmd_IC_GetSize(void)
{
    size_t sz = 0;
    if (m_BlobLock->IsBlobExists()) {
        sz = m_BlobLock->GetBlobSize();
    }
    m_SockBuffer.WriteMessage("OK:", NStr::UInt8ToString(sz));
    return true;
}

bool
CNCMessageHandler::x_DoCmd_IC_GetAccessTime(void)
{
    int t = m_BlobLock->GetBlobAccessTime();
    m_SockBuffer.WriteMessage("OK:", NStr::IntToString(t));
    return true;
}

bool
CNCMessageHandler::x_DoCmd_NotImplemented(void)
{
    m_DiagContext->SetRequestStatus(eStatus_NoImpl);
    m_SockBuffer.WriteMessage("OK:", "Not implemented");
    return true;
}




CNCMsgHandler_Factory::~CNCMsgHandler_Factory(void)
{}


CNCMsgHndlFactory_Proxy::~CNCMsgHndlFactory_Proxy(void)
{}

IServer_ConnectionHandler*
CNCMsgHndlFactory_Proxy::Create(void)
{
    return m_Factory->Create();
}


CNCMsgHandler_Proxy::~CNCMsgHandler_Proxy(void)
{}

EIO_Event
CNCMsgHandler_Proxy::GetEventsToPollFor(const CTime** alarm_time) const
{
    return m_Handler->GetEventsToPollFor(alarm_time);
}

bool
CNCMsgHandler_Proxy::IsReadyToProcess(void) const
{
    return m_Handler->IsReadyToProcess();
}

void
CNCMsgHandler_Proxy::OnOpen(void)
{
    m_Handler->SetSocket(&GetSocket());
    m_Handler->OnOpen();
}

void
CNCMsgHandler_Proxy::OnRead(void)
{
    m_Handler->OnRead();
}

void
CNCMsgHandler_Proxy::OnWrite(void)
{
    m_Handler->OnWrite();
}

void
CNCMsgHandler_Proxy::OnClose(EClosePeer peer)
{
    m_Handler->OnClose(peer);
}

void
CNCMsgHandler_Proxy::OnTimeout(void)
{
    m_Handler->OnTimeout();
}

void
CNCMsgHandler_Proxy::OnTimer(void)
{
    m_Handler->OnTimer();
}

void
CNCMsgHandler_Proxy::OnOverflow(EOverflowReason)
{
    m_Handler->OnOverflow();
}

END_NCBI_SCOPE
