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

#include "message_handler.hpp"
#include "netcache_version.hpp"
#include "nc_stat.hpp"
#include "error_codes.hpp"


BEGIN_NCBI_SCOPE


#define NCBI_USE_ERRCODE_X  NetCache_Handler


/// Definition of all NetCache commands
static CNCMessageHandler::SCommandDef s_CommandMap[] = {
    { "VERSION",
        {&CNCMessageHandler::x_DoCmd_Version,
            "VERSION"} },
    { "PUT3",
        {&CNCMessageHandler::x_DoCmd_Put3,
            "PUT3",          eWithAutoBlobKey, eNCCreate},
        { { "ttl",     eNSPT_Int,  eNSPA_Optional },
          { "key",     eNSPT_NCID, eNSPA_Optional },
          { "key_ver", eNSPT_Int,  eNSPA_Optional },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "GET2",
        {&CNCMessageHandler::x_DoCmd_Get,
            "GET2",          eWithBlob,        eNCReadData},
        { { "key",     eNSPT_NCID, eNSPA_Required },
          { "NW",      eNSPT_Id,   eNSPA_Obsolete | fNSPA_Match },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "RMV2",
        {&CNCMessageHandler::x_DoCmd_Remove2,
            "RMV2",          eWithBlob,        eNCDelete},
        { { "key",     eNSPT_NCID, eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "HASB",
        {&CNCMessageHandler::x_DoCmd_HasBlob,
            "HASB",          eWithoutBlob},
        { { "key",     eNSPT_NCID, eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "GSIZ",
        {&CNCMessageHandler::x_DoCmd_GetSize,
            "GetSIZe",       eWithBlob,        eNCRead},
        { { "key",     eNSPT_NCID, eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "STOR",
        {&CNCMessageHandler::x_DoCmd_IC_Store,
            "IC_STOR",       eWithBlob,        eNCCreate},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "ttl",     eNSPT_Int,  eNSPA_Required },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "version", eNSPT_Int,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "STRS",
        {&CNCMessageHandler::x_DoCmd_IC_StoreBlob,
            "IC_STRS",       eWithBlob,        eNCCreate},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "ttl",     eNSPT_Int,  eNSPA_Required },
          { "size",    eNSPT_Int,  eNSPA_Required },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "version", eNSPT_Int,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "READ",
        {&CNCMessageHandler::x_DoCmd_Get,
            "IC_READ",       eWithBlob,        eNCReadData},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "version", eNSPT_Int,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "HASB",
        {&CNCMessageHandler::x_DoCmd_HasBlob,
            "IC_HASB",       eWithoutBlob},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "version", eNSPT_Int,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "REMO",
        {&CNCMessageHandler::x_DoCmd_Remove2,
            "IC_REMOve",     eWithBlob,        eNCDelete},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "version", eNSPT_Int,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "GSIZ",
        {&CNCMessageHandler::x_DoCmd_IC_GetSize,
            "IC_GetSIZe",    eWithBlob,        eNCRead},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "version", eNSPT_Int,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "GETPART",
        {&CNCMessageHandler::x_DoCmd_Get,
            "GETPART",       eWithBlob,        eNCReadData},
        { { "key",     eNSPT_NCID, eNSPA_Required },
          { "start",   eNSPT_Int,  eNSPA_Required },
          { "size",    eNSPT_Int,  eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "READPART",
        {&CNCMessageHandler::x_DoCmd_Get,
            "IC_READPART",   eWithBlob,        eNCReadData},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "key",     eNSPT_Str,  eNSPA_Required },
          { "version", eNSPT_Int,  eNSPA_Required },
          { "subkey",  eNSPT_Str,  eNSPA_Required },
          { "start",   eNSPT_Int,  eNSPA_Required },
          { "size",    eNSPT_Int,  eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional },
          { "pass",    eNSPT_Str,  eNSPA_Optional } } },
    { "A?",     {&CNCMessageHandler::x_DoCmd_Alive,  "A?"} },
    { "HEALTH", {&CNCMessageHandler::x_DoCmd_Health, "HEALTH"} },
    { "PUT2",
        {&CNCMessageHandler::x_DoCmd_Put2,
            "PUT2",          eWithAutoBlobKey, eNCCreate},
        { { "ttl",     eNSPT_Int,  eNSPA_Optional },
          { "key",     eNSPT_NCID, eNSPA_Optional },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "GET",
        {&CNCMessageHandler::x_DoCmd_Get,
            "GET",           eWithBlob,        eNCReadData},
        { { "key",     eNSPT_NCID, eNSPA_Required },
          { "NW",      eNSPT_Id,   eNSPA_Obsolete | fNSPA_Match },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "REMOVE",
        {&CNCMessageHandler::x_DoCmd_Remove,
            "REMOVE",        eWithBlob,        eNCDelete},
        { { "key",     eNSPT_NCID, eNSPA_Required },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "SHUTDOWN",
        {&CNCMessageHandler::x_DoCmd_Shutdown,       "SHUTDOWN"},
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
    { "REINIT",
        {&CNCMessageHandler::x_DoCmd_Reinit,
            "REINIT",        eWithoutBlob},
        { { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "REINIT",
        {&CNCMessageHandler::x_DoCmd_Reinit,
            "IC_REINIT",     eWithoutBlob},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix },
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "RECONF",
        {&CNCMessageHandler::x_DoCmd_Reconf, "RECONF"},
        { { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "LOG",      {&CNCMessageHandler::x_DoCmd_NotImplemented, "LOG"} },
    { "STAT",     {&CNCMessageHandler::x_DoCmd_NotImplemented, "STAT"} },
    { "MONI",     {&CNCMessageHandler::x_DoCmd_NotImplemented, "MONITOR"} },
    { "DROPSTAT", {&CNCMessageHandler::x_DoCmd_NotImplemented, "DROPSTAT"} },
    { "GBOW",     {&CNCMessageHandler::x_DoCmd_NotImplemented, "GBOW"} },
    { "ISLK",     {&CNCMessageHandler::x_DoCmd_NotImplemented, "ISLK"} },
    { "SMR",      {&CNCMessageHandler::x_DoCmd_NotImplemented, "SMR"} },
    { "SMU",      {&CNCMessageHandler::x_DoCmd_NotImplemented, "SMU"} },
    { "OK",       {&CNCMessageHandler::x_DoCmd_NotImplemented, "OK"} },
    { "PUT",      {&CNCMessageHandler::x_DoCmd_NotImplemented, "PUT"} },
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
    { "GBLW",     {&CNCMessageHandler::x_DoCmd_NotImplemented, "IC_GBLW"},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix } } },
    { "ISOP",     {&CNCMessageHandler::x_DoCmd_NotImplemented, "IC_ISOP"},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix } } },
    { "GACT",     {&CNCMessageHandler::x_DoCmd_NotImplemented, "IC_GACT"},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix } } },
    { "GTOU",     {&CNCMessageHandler::x_DoCmd_NotImplemented, "IC_GTOU"},
        { { "cache",   eNSPT_Id,   eNSPA_ICPrefix } } },
    { NULL }
};

static SNSProtoArgument s_AuthArgs[] = {
    { "client", eNSPT_Str, eNSPA_Optional, "Empty client" },
    { "params", eNSPT_Str, eNSPA_Ellipsis },
    { NULL }
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


inline void
CBufferedSockReaderWriter::ResetSocket(CSocket* socket, unsigned int def_timeout)
{
    m_ReadDataPos  = 0;
    m_WriteDataPos = 0;
    m_CRLFMet = eNone;
    m_ReadBuff.resize(0);
    m_WriteBuff.resize(0);
    m_Socket = socket;
    m_DefTimeout.sec  = def_timeout;
    if (m_Socket) {
        ResetSocketTimeout();
    }
}

inline void
CBufferedSockReaderWriter::SetTimeout(unsigned int def_timeout)
{
    m_DefTimeout.sec = def_timeout;
    ResetSocketTimeout();
}

inline
CBufferedSockReaderWriter::CBufferedSockReaderWriter(void)
    : m_ReadBuff(kInitNetworkBufferSize),
      m_WriteBuff(kInitNetworkBufferSize)
{
    m_DefTimeout.usec = m_ZeroTimeout.sec = m_ZeroTimeout.usec = 0;
    ResetSocket(NULL, 0);
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
CBufferedSockReaderWriter::ReadLine(CTempString* line)
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

    line->assign(m_ReadBuff.data() + m_ReadDataPos, crlf_pos - m_ReadDataPos);
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
    g_NetcacheServer->CloseConnection(m_Socket);
}

inline void
CNCMessageHandler::InitDiagnostics(void)
{
    if (m_DiagContext) {
        CDiagContext::SetRequestContext(m_DiagContext);
    }
    else if (m_ConnContext) {
        CDiagContext::SetRequestContext(m_ConnContext);
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


CNCMessageHandler::CNCMessageHandler(void)
    : m_Socket(NULL),
      m_State(eSocketClosed),
      m_Parser(s_CommandMap),
      m_CmdProcessor(NULL),
      m_BlobAccess(NULL)
{}

CNCMessageHandler::~CNCMessageHandler(void)
{}

inline void
CNCMessageHandler::OnOpen(void)
{
    _ASSERT(x_GetState() == eSocketClosed);

    m_Socket->DisableOSSendDelay();
    m_SockBuffer.ResetSocket(m_Socket, g_NetcacheServer->GetDefConnTimeout());

    m_PrevCache.clear();
    m_ClientParams.clear();
    m_CntCmds = 0;

    m_ClientParams["peer"]  = m_Socket->GetPeerAddress(eSAF_IP);
    m_ClientParams["pport"] = m_Socket->GetPeerAddress(eSAF_Port);
    m_LocalPort             = m_Socket->GetLocalPort(eNH_HostByteOrder);
    m_ClientParams["port"]  = NStr::UIntToString(m_LocalPort);

    m_ConnContext.Reset(new CRequestContext());
    m_ConnReqId = NStr::IntToString(m_ConnContext->SetRequestID());

    CDiagnosticsGuard guard(this);

    if (g_NetcacheServer->IsLogCmds()) {
        CDiagContext_Extra diag_extra = GetDiagContext().PrintRequestStart();
        ITERATE(TStringMap, it, m_ClientParams) {
            diag_extra.Print(it->first, it->second);
        }
        x_SetFlag(fConnStartPrinted);
    }
    m_ConnContext->SetRequestStatus(eStatus_OK);

    m_ConnTime = g_NetcacheServer->GetFastTime();
    x_SetState(ePreAuthenticated);

    CNCStat::AddOpenedConnection();
}

inline void
CNCMessageHandler::OnTimeout(void)
{
    CDiagnosticsGuard guard(this);

    INFO_POST(Info << "Inactivity timeout expired, closing connection");
    if (m_DiagContext)
        m_ConnContext->SetRequestStatus(eStatus_CmdTimeout);
    else
        m_ConnContext->SetRequestStatus(eStatus_Inactive);
}

inline void
CNCMessageHandler::OnTimer(void)
{
    if (!x_IsFlagSet(fCommandStarted))
        return;

    CDiagnosticsGuard guard(this);

    INFO_POST("Command execution timed out. Closing connection.");
    CNCStat::AddTimedOutCommand();
    m_DiagContext->SetRequestStatus(eStatus_CmdTimeout);
    x_CloseConnection();
}

inline void
CNCMessageHandler::OnOverflow(void)
{
    // Max connection overflow
    ERR_POST("Max number of connections reached, closing connection");
    CNCStat::AddOverflowConnection();
}

inline void
CNCMessageHandler::OnClose(IServer_ConnectionHandler::EClosePeer peer)
{
    CDiagnosticsGuard guard(this);

    switch (peer)
    {
    case IServer_ConnectionHandler::eOurClose:
        if (m_DiagContext) {
            m_ConnContext->SetRequestStatus(m_DiagContext->GetRequestStatus());
        }
        else {
            m_ConnContext->SetRequestStatus(eStatus_Inactive);
        }
        break;
    case IServer_ConnectionHandler::eClientClose:
        if (m_DiagContext) {
            m_DiagContext->SetRequestStatus(eStatus_BadCmd);
            m_ConnContext->SetRequestStatus(eStatus_BadCmd);
        }
        break;
    }

    // Hack-ish fix to make cursed PUT2 command to work - it uses connection
    // closing as legal end-of-blob signal.
    if (x_GetState() == eReadBlobChunkLength
        &&  NStr::strcmp(m_CurCmd, "PUT2") == 0)
    {
        _ASSERT(m_DiagContext  &&  m_BlobAccess);
        m_DiagContext->SetRequestStatus(eStatus_OK);
        m_ConnContext->SetRequestStatus(eStatus_PUT2Used);
        x_FinishReadingBlob();
    }
    x_FinishCommand();

    if (x_GetState() == ePreAuthenticated) {
        CNCStat::SetFakeConnection();
    }
    else {
        double conn_span = (g_NetcacheServer->GetFastTime() - m_ConnTime).GetAsDouble();
        CNCStat::AddClosedConnection(conn_span,
                                     m_ConnContext->GetRequestStatus(),
                                     m_CntCmds);
    }

    if (x_IsFlagSet(fConnStartPrinted)) {
        GetDiagContext().PrintRequestStop();
        x_UnsetFlag(fConnStartPrinted);
    }
    m_ConnContext.Reset();

    x_SetState(eSocketClosed);
    m_SockBuffer.ResetSocket(NULL, 0);
}

inline void
CNCMessageHandler::OnRead(void)
{
    CSpinGuard guard(m_ObjLock);
    x_ManageCmdPipeline();
}

inline void
CNCMessageHandler::OnWrite(void)
{
    CSpinGuard guard(m_ObjLock);
    x_ManageCmdPipeline();
}

inline EIO_Event
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

inline bool
CNCMessageHandler::IsReadyToProcess(void) const
{
    if (x_IsFlagSet(fWaitForBlockedOp))
        return false;
    else if (x_GetState() == eWaitForStorageBlock)
        return g_NCStorage->IsBlockActive();

    return !m_ObjLock.IsLocked();
}

void
CNCMessageHandler::OnBlockedOpFinish(void)
{
    CSpinGuard guard(m_ObjLock);

    x_UnsetFlag(fWaitForBlockedOp);
    CNCStat::AddBlockedOperation();
    x_ManageCmdPipeline();
}

inline bool
CNCMessageHandler::x_ReadAuthMessage(void)
{
    CTempString auth_line;
    if (!m_SockBuffer.ReadLine(&auth_line)) {
        return false;
    }

    TNSProtoParams params;
    try {
        m_Parser.ParseArguments(auth_line, s_AuthArgs, &params);
    }
    catch (CNSProtoParserException& ex) {
        ERR_POST_X(6, "Error authenticating client: '" << auth_line
                      << "': " << ex);
    }
    ITERATE(TNSProtoParams, it, params) {
        m_ClientParams[it->first] = it->second;
    }
    if (x_IsFlagSet(fConnStartPrinted)) {
        CDiagContext_Extra diag_extra = GetDiagContext().Extra();
        ITERATE(TNSProtoParams, it, params) {
            diag_extra.Print(it->first, it->second);
        }
    }

    m_BaseAppSetup = m_AppSetup = g_NetcacheServer->GetAppSetup(m_ClientParams);
    x_SetState(eReadyForCommand);

    if (m_AppSetup->disable) {
        m_ConnContext->SetRequestStatus(eStatus_Disabled);
        ERR_POST_X(7, "Disabled client is being disconnected ('"
                      << auth_line << "').");
        x_CloseConnection();
    }
    else {
        m_SockBuffer.SetTimeout(m_AppSetup->conn_timeout);
    }
    return true;
}

inline void
CNCMessageHandler::x_CheckAdminClient(void)
{
    if (m_ClientParams["client"] != g_NetcacheServer->GetAdminClient()) {
        NCBI_THROW(CNSProtoParserException, eWrongCommand,
                   "Command is not accessible");
    }
}

inline void
CNCMessageHandler::x_AssignCmdParams(TNSProtoParams& params)
{
    CTempString blob_key, blob_subkey;
    unsigned int blob_version = 0;
    m_RawKey.clear();
    m_BlobPass.clear();
    m_KeyVersion = 1;
    m_BlobTTL = 0;
    m_StartPos = 0;
    m_Size = -1;

    CTempString cache_name;

    NON_CONST_ITERATE(TNSProtoParams, it, params) {
        const CTempString& key = it->first;
        CTempString& val = it->second;

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
                m_RawKey = blob_key = val;
            }
            else if (key == "key_ver") {
                m_KeyVersion = NStr::StringToUInt(val);
            }
            break;
        case 'p':
            if (key == "pass") {
                CMD5 md5;
                md5.Update(val.data(), val.size());
                m_BlobPass = md5.GetHexSum();
                val = m_BlobPass;
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
                blob_subkey = val;
            }
            else if (key == "size") {
                m_Size = NStr::StringToInt8(val);
            }
            else if (key == "start") {
                m_StartPos = NStr::StringToInt8(val);
            }
            break;
        case 't':
            if (key == "ttl") {
                m_BlobTTL = NStr::StringToUInt(val);
            }
            break;
        case 'v':
            if (key == "version") {
                blob_version = NStr::StringToUInt(val);
            }
            break;
        default:
            break;
        }
    }

    g_NCStorage->PackBlobKey(&m_BlobKey, cache_name,
                             blob_key, blob_subkey, blob_version);
    if (cache_name.empty()) {
        m_AppSetup = m_BaseAppSetup;
    }
    else if (cache_name == m_PrevCache) {
        m_AppSetup = m_PrevAppSetup;
    }
    else {
        m_PrevCache = cache_name;
        m_ClientParams["cache"] = cache_name;
        m_AppSetup = m_PrevAppSetup = g_NetcacheServer->GetAppSetup(m_ClientParams);
    }
}

inline void
CNCMessageHandler::x_PrintRequestStart(const SParsedCmd& cmd)
{
    if (!g_NetcacheServer->IsLogCmds())
        return;

    CDiagContext_Extra diag_extra = GetDiagContext().PrintRequestStart();
    diag_extra.Print("cmd",  cmd.command->cmd);
    diag_extra.Print("conn", m_ConnReqId);
    ITERATE(TNSProtoParams, it, cmd.params) {
        diag_extra.Print(it->first, it->second);
    }

    x_SetFlag(fCommandPrinted);
}

inline void
CNCMessageHandler::x_WaitForWouldBlock(void)
{
    g_NetcacheServer->DeferConnectionProcessing(m_Socket);
    x_SetFlag(fWaitForBlockedOp);
}

inline void
CNCMessageHandler::x_StartCommand(SParsedCmd& cmd)
{
    x_SetFlag(fCommandStarted);

    const SCommandExtra& cmd_extra = cmd.command->extra;
    m_CmdProcessor = cmd_extra.processor;
    m_CurCmd       = cmd_extra.cmd_name;

    m_MaxCmdTime = m_CmdStartTime = g_NetcacheServer->GetFastTime();
    m_MaxCmdTime.AddSecond(m_AppSetup->cmd_timeout, CTime::eIgnoreDaylight);

    x_PrintRequestStart(cmd);
    // PrintRequestStart() resets status value, so setting default status
    // should go here, though more logical is in x_ReadCommand()
    m_DiagContext->SetRequestStatus(eStatus_OK);

    if (cmd_extra.storage_access == eWithAutoBlobKey  &&  m_RawKey.empty()) {
        g_NetcacheServer->GenerateBlobKey(m_KeyVersion, m_LocalPort, &m_RawKey);
        g_NCStorage->PackBlobKey(&m_BlobKey, CTempString(), m_RawKey, CTempString(), 0);

        if (x_IsFlagSet(fCommandPrinted)) {
            CDiagContext_Extra diag_extra = GetDiagContext().Extra();
            diag_extra.Print("gen_key", m_RawKey);
        }
    }

    if (m_AppSetup->disable) {
        // We'll be here only if generally work for the client is enabled but
        // for current particular cache is disabled.
        m_DiagContext->SetRequestStatus(eStatus_Disabled);
        m_SockBuffer.WriteMessage("ERR:", "Cache is disabled");
        x_SetState(eReadyForCommand);
    }
    else if (cmd_extra.storage_access == eWithBlob
             ||  cmd_extra.storage_access == eWithAutoBlobKey)
    {
        if ((m_BlobPass.empty()  &&  m_AppSetup->pass_policy == eNCOnlyWithPass)
            ||  (!m_BlobPass.empty()  &&  m_AppSetup->pass_policy == eNCOnlyWithoutPass))
        {
            NCBI_THROW(CNSProtoParserException, eWrongParams,
                       "Password in the command doesn't match server settings.");
        }

        m_BlobAccess = g_NCStorage->GetBlobAccess(
                                    cmd_extra.blob_access, m_BlobKey, m_BlobPass);
        x_SetState(eWaitForBlobAccess);
    }
    else {
        x_SetState(eCommandReceived);
    }
}

inline bool
CNCMessageHandler::x_ReadCommand(void)
{
    CTempString cmd_line;
    if (!m_SockBuffer.ReadLine(&cmd_line)) {
        return false;
    }

    SParsedCmd cmd = m_Parser.ParseCommand(cmd_line);

    m_DiagContext.Reset(new CRequestContext());
    m_DiagContext->SetRequestID();
    CDiagContext::SetRequestContext(m_DiagContext);

    x_AssignCmdParams(cmd.params);
    x_StartCommand(cmd);

    return true;
}

inline bool
CNCMessageHandler::x_WaitForBlobAccess(void)
{
    if (m_BlobAccess->ObtainMetaInfo(this) == eNCWouldBlock) {
        x_WaitForWouldBlock();
        return false;
    }
    else if (m_BlobAccess->IsAuthorized()) {
        x_SetState(eCommandReceived);
        if (m_BlobAccess->IsBlobExists()  &&  m_AppSetup->prolong_on_read
            &&  (m_BlobAccess->GetAccessType() == eNCRead
                 ||  m_BlobAccess->GetAccessType() == eNCReadData))
        {
            m_BlobAccess->ProlongBlobsLife();
        }
    }
    else {
        x_SetState(ePasswordFailed);
    }
    return true;
}

inline bool
CNCMessageHandler::x_ProcessBadPassword(void)
{
    if (m_BlobAccess->GetAccessType() == eNCCreate) {
        m_SockBuffer.WriteMessage("ERR:", "Error occurred while writing blob data");
    }
    else {
        m_SockBuffer.WriteMessage("ERR:", "BLOB not found.");
    }
    m_SockBuffer.Flush();
    m_DiagContext->SetRequestStatus(eStatus_BadPassword);
    ERR_POST_X(5, Warning << "Incorrect password is used to access the blob");
    x_SetState(eReadyForCommand);
    if (m_BlobAccess->GetAccessType() == eNCCreate) {
        x_CloseConnection();
    }
    return true;
}

inline bool
CNCMessageHandler::x_WaitForStorageBlock(void)
{
    if (g_NCStorage->IsBlockActive()) {
        x_SetState(eCommandReceived);
        return true;
    }
    else {
        g_NetcacheServer->DeferConnectionProcessing(m_Socket);
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
        CDiagContext::SetRequestContext(m_ConnContext);
        return;
    }

    Int8 blob_size = 0;
    bool was_blob_access = (m_BlobAccess != NULL);
    if (was_blob_access) {
        if (!m_BlobAccess->IsBlobExists()
            ||  m_BlobAccess->GetAccessType() == eNCDelete)
        {
            // Blob was deleted or didn't exist, logging of size is not necessary
            was_blob_access = false;
        }
        else {
            blob_size = m_BlobAccess->GetBlobSize();
        }
        m_BlobAccess->Release();
        m_BlobAccess = NULL;
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
            diag_extra.Print("size", NStr::Int8ToString(blob_size));
        }
        GetDiagContext().PrintRequestStop();
        x_UnsetFlag(fCommandPrinted);
    }

    x_UnsetFlag(fCommandStarted);
    x_UnsetFlag(fConfirmBlobPut);
    x_UnsetFlag(fReadExactBlobSize);

    double cmd_span = (g_NetcacheServer->GetFastTime() - m_CmdStartTime).GetAsDouble();
    CNCStat::AddFinishedCmd(m_CurCmd, cmd_span, cmd_status);
    ++m_CntCmds;

    CDiagContext::SetRequestContext(m_ConnContext);
}



inline void
CNCMessageHandler::x_StartReadingBlob(void)
{
    m_SockBuffer.Flush();
    x_SetState(eReadBlobSignature);
}

void
CNCMessageHandler::x_FinishReadingBlob(void)
{
    if (x_IsFlagSet(fReadExactBlobSize)
        &&  m_BlobAccess->GetBlobSize() != m_Size)
    {
        m_DiagContext->SetRequestStatus(eStatus_CondFailed);
        NCBI_THROW_FMT(CNetServiceException, eProtocolError,
                       "Too few data for blob size "
                       << m_Size << " (received "
                       << m_BlobAccess->GetBlobSize() + " bytes)");
    }

    m_BlobAccess->Finalize();
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

    Uint4 sig = *static_cast<const Uint4*>(m_SockBuffer.GetReadData());
    m_SockBuffer.EraseReadData(4);

    if (sig == 0x04030201) {
        x_SetFlag(fSwapLengthBytes);
    }
    else if (sig == 0x01020304) {
        x_UnsetFlag(fSwapLengthBytes);
    }
    else {
        m_DiagContext->SetRequestStatus(eStatus_BadCmd);
        NCBI_THROW_FMT(CUtilException, eWrongData,
                       "Cannot determine the byte order. Got: "
                       << NStr::UIntToString(sig, 0, 16));
    }

    x_SetState(eReadBlobChunkLength);
    return true;
}

inline bool
CNCMessageHandler::x_ReadBlobChunkLength(void)
{
    if (x_IsFlagSet(fReadExactBlobSize)
        &&  m_BlobAccess->GetBlobSize() == m_Size)
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

        m_ChunkLen = *static_cast<const Uint4*>(m_SockBuffer.GetReadData());
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
             &&  m_BlobAccess->GetBlobSize() + m_ChunkLen > m_Size)
    {
        m_DiagContext->SetRequestStatus(eStatus_CondFailed);
        NCBI_THROW_FMT(CNetServiceException, eProtocolError,
                       "Too much data for blob size " << m_Size
                       << " (received at least "
                       << (m_BlobAccess->GetBlobSize() + m_ChunkLen) << " bytes)");
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

        Uint4 read_len = static_cast<Uint4>(min(m_SockBuffer.GetReadSize(),
                                                size_t(m_ChunkLen)));
        m_BlobAccess->WriteData(m_SockBuffer.GetReadData(), read_len);
        m_SockBuffer.EraseReadData(read_len);
        m_ChunkLen -= read_len;
    }

    x_SetState(eReadBlobChunkLength);
    return true;
}

inline bool
CNCMessageHandler::x_WaitForFirstData(void)
{
    if (m_BlobAccess->ObtainFirstData(this) == eNCWouldBlock) {
        x_WaitForWouldBlock();
        return false;
    }
    x_SetState(eWriteBlobData);
    return true;
}

inline bool
CNCMessageHandler::x_StartWritingBlob(void)
{
    if (!m_BlobAccess->IsBlobExists()) {
        m_DiagContext->SetRequestStatus(eStatus_NotFound);
        m_SockBuffer.WriteMessage("ERR:", "BLOB not found.");
        return true;
    }

    Int8 blob_size = m_BlobAccess->GetBlobSize();
    if (blob_size < m_StartPos)
        blob_size = 0;
    else
        blob_size -= m_StartPos;
    if (m_Size != -1  &&  m_Size < blob_size) {
        blob_size = m_Size;
    }
    m_SockBuffer.WriteMessage("OK:", "BLOB found. SIZE="
                                     + NStr::Int8ToString(blob_size));

    if (blob_size != 0) {
        m_BlobAccess->SetPosition(m_StartPos);
        x_SetState(eWaitForFirstData);
    }
    return true;
}

inline bool
CNCMessageHandler::x_WriteBlobData(void)
{
    do {
        void* write_buf  = m_SockBuffer.PrepareDirectWrite();
        size_t want_read = m_SockBuffer.GetDirectWriteSize();
        if (m_Size != -1  &&  m_Size < Int8(want_read))
            want_read = size_t(m_Size);
        size_t n_read = m_BlobAccess->ReadData(write_buf, want_read);
        if (n_read == 0) {
            x_SetState(eReadyForCommand);
            return true;
        }
        m_SockBuffer.AddDirectWritten(n_read);
        m_SockBuffer.Flush();
        if (m_Size != -1)
            m_Size -= n_read;
    }
    while (!m_SockBuffer.IsWriteDataPending());

    return false;
}

inline unsigned int
CNCMessageHandler::x_GetBlobTTL(void)
{
    return m_BlobTTL? m_BlobTTL: m_AppSetup->blob_ttl;
}

void
CNCMessageHandler::x_ManageCmdPipeline(void)
{
    CDiagnosticsGuard guard(this);

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
            case eWaitForBlobAccess:
                do_next_step = x_WaitForBlobAccess();
                break;
            case ePasswordFailed:
                do_next_step = x_ProcessBadPassword();
                break;
            case eWaitForStorageBlock:
                do_next_step = x_WaitForStorageBlock();
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
            case eWaitForFirstData:
                do_next_step = x_WaitForFirstData();
                break;
            case eWriteBlobData:
                do_next_step = x_WriteBlobData();
                break;
            default:
                ERR_POST_X(2, Fatal
                           << "Fatal: Handler is in unexpected state - "
                           << m_State << ". Closing connection.");
                x_CloseConnection();
            }

            m_SockBuffer.Flush();
        }
        catch (CIO_Exception& ex) {
            if (m_DiagContext) {
                m_DiagContext->SetRequestStatus(eStatus_ServerError);
            }
            else {
                m_ConnContext->SetRequestStatus(eStatus_ServerError);
            }
            ERR_POST_X(1, "IO exception in the command: " << ex);
            if (ex.GetErrCode() != CIO_Exception::eClosed) {
                x_CloseConnection();
            }
        }
        catch (CNCBlobStorageException& ex) {
            ERR_POST_X(1, "Server internal exception: " << ex);
            if (m_DiagContext) {
                m_DiagContext->SetRequestStatus(eStatus_ServerError);
            }
            else {
                m_ConnContext->SetRequestStatus(eStatus_ServerError);
            }
            x_CloseConnection();
        }
        catch (CException& ex) {
            ERR_POST_X(1, "Unknown exception in the command: " << ex);
            if (x_GetState() == eReadyForCommand
                ||  x_GetState() == eCommandReceived)
            {
                if (m_DiagContext) {
                    if (m_DiagContext->GetRequestStatus() == eStatus_OK) {
                        m_DiagContext->SetRequestStatus(eStatus_BadCmd);
                    }
                }
                else {
                    m_ConnContext->SetRequestStatus(eStatus_BadCmd);
                }
                m_SockBuffer.WriteMessage("ERR:",
                                          NStr::Replace(ex.what(), "\n", ";"));
                if (m_BlobAccess  &&  m_BlobAccess->GetAccessType() != eNCCreate)
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
                if (m_DiagContext) {
                    if (m_DiagContext->GetRequestStatus() == eStatus_OK)
                    {
                        m_DiagContext->SetRequestStatus(eStatus_ServerError);
                    }
                }
                else {
                    m_ConnContext->SetRequestStatus(eStatus_ServerError);
                }
                x_CloseConnection();
            }
        }
    }
    while (!m_SockBuffer.IsWriteDataPending()  &&  do_next_step
           &&  x_GetState() != eSocketClosed);
}

bool
CNCMessageHandler::x_DoCmd_Alive(void)
{
    m_SockBuffer.WriteMessage("OK:", "");
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
CNCMessageHandler::x_DoCmd_Shutdown(void)
{
    x_CheckAdminClient();
    g_NetcacheServer->RequestShutdown();
    m_SockBuffer.WriteMessage("OK:", "");
    return true;
}

bool
CNCMessageHandler::x_DoCmd_Version(void)
{
    m_SockBuffer.WriteMessage("OK:", NETCACHED_HUMAN_VERSION);
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
    CNcbiApplication::Instance()->GetConfig().Write(*x_PrepareSockStream());
    return true;
}

bool
CNCMessageHandler::x_DoCmd_GetServerStats(void)
{
    g_NetcacheServer->PrintServerStats(x_PrepareSockStream().get());
    return true;
}

bool
CNCMessageHandler::x_DoCmd_Reinit(void)
{
    x_CheckAdminClient();
    g_NCStorage->Block();
    x_SetState(eWaitForStorageBlock);
    m_CmdProcessor = &CNCMessageHandler::x_DoCmd_ReinitImpl;
    return true;
}

bool
CNCMessageHandler::x_DoCmd_ReinitImpl(void)
{
    g_NCStorage->ReinitializeCache(m_BlobKey);
    g_NCStorage->Unblock();
    m_SockBuffer.WriteMessage("OK:", "");
    return true;
}

bool
CNCMessageHandler::x_DoCmd_Reconf(void)
{
    x_CheckAdminClient();
    g_NCStorage->Block();
    x_SetState(eWaitForStorageBlock);
    m_CmdProcessor = &CNCMessageHandler::x_DoCmd_ReconfImpl;
    return true;
}

bool
CNCMessageHandler::x_DoCmd_ReconfImpl(void)
{
    g_NetcacheServer->Reconfigure();
    g_NCStorage->Unblock();
    m_SockBuffer.WriteMessage("OK:", "");
    return true;
}

bool
CNCMessageHandler::x_DoCmd_Put2(void)
{
    m_BlobAccess->SetBlobTTL(x_GetBlobTTL());
 
    m_SockBuffer.WriteMessage("ID:", m_RawKey);
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
    return x_StartWritingBlob();
}

bool
CNCMessageHandler::x_DoCmd_GetSize(void)
{
    if (!m_BlobAccess->IsBlobExists()) {
        m_DiagContext->SetRequestStatus(eStatus_NotFound);
        m_SockBuffer.WriteMessage("ERR:", "BLOB not found.");
    }
    else {
        Uint8 size = m_BlobAccess->GetBlobSize();
        m_SockBuffer.WriteMessage("OK:", NStr::UInt8ToString(size));
    }

    return true;
}

bool
CNCMessageHandler::x_DoCmd_HasBlob(void)
{
    bool hb = g_NCStorage->IsBlobFamilyExists(m_BlobKey);
    m_SockBuffer.WriteMessage("OK:", NStr::IntToString(int(hb)));
    return true;
}

bool
CNCMessageHandler::x_DoCmd_Remove(void)
{
    m_BlobAccess->DeleteBlob();
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
CNCMessageHandler::x_DoCmd_IC_Store(void)
{
    m_BlobAccess->SetBlobTTL(x_GetBlobTTL());
 
    m_SockBuffer.WriteMessage("OK:", "");
    x_StartReadingBlob();
    return true;
}

bool
CNCMessageHandler::x_DoCmd_IC_StoreBlob(void)
{
    m_SockBuffer.WriteMessage("OK:", "");

    m_BlobAccess->SetBlobTTL(x_GetBlobTTL());
    if (m_Size != 0) {
        x_SetFlag(fReadExactBlobSize);
        x_StartReadingBlob();
    }
    return true;
}

bool
CNCMessageHandler::x_DoCmd_IC_GetSize(void)
{
    Uint8 sz = 0;
    if (m_BlobAccess->IsBlobExists()) {
        sz = m_BlobAccess->GetBlobSize();
    }
    m_SockBuffer.WriteMessage("OK:", NStr::UInt8ToString(sz));
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


inline IServer_ConnectionHandler*
CNCMsgHandler_Factory::Create(void)
{
    return new CNCMsgHandler_Proxy(this);
}


CNCMsgHndlFactory_Proxy::~CNCMsgHndlFactory_Proxy(void)
{}

IServer_ConnectionHandler*
CNCMsgHndlFactory_Proxy::Create(void)
{
    return m_Factory->Create();
}

END_NCBI_SCOPE
