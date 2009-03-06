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
          { "ip",      eNSPT_Str,  eNSPA_Optchain },
          { "sid",     eNSPT_Str,  eNSPA_Optional } } },
    { "PUT3",     &CNetCache_MessageHandler::ProcessPut3,
        { { "timeout", eNSPT_Int,  eNSPA_Optional },
          { "id",      eNSPT_NCID, eNSPA_Optional },
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
      m_ReadBufSize(0),
      m_ReadBufPos(0),
      m_WriteBuff(NULL),
      m_WriteBufSize(0),
      m_WriteDataSize(0),
      m_InRequest(false),
      m_Buffer(0),
      m_SeenCR(false),
      m_InTransmission(false),
      m_DelayedWrite(false),
      m_OverflowWrite(false),
      m_DeferredCmd(false),
      m_StartPrinted(false),
      m_Parser(s_CommandMap),
      m_PutOK(false),
      m_SizeKnown(false),
      m_BlobSize(0)
{
    m_WriteBuff = static_cast<char*>(malloc(kNetworkBufferSize));
    m_WriteBufSize = kNetworkBufferSize;
}

CNetCache_MessageHandler::~CNetCache_MessageHandler(void)
{
    BUF_Destroy(m_Buffer);
    free(m_WriteBuff);
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
CNetCache_MessageHandler::GetEventsToPollFor(const CTime** alarm_time) const
{
    if (m_InRequest) {
        *alarm_time = &m_Stat.max_exec_time;
    }
    if (m_DelayedWrite  ||  m_OverflowWrite) {
        return eIO_Write;
    }
    return eIO_Read;
}

bool
CNetCache_MessageHandler::IsReadyToProcess(void) const
{
    return m_LockHolder.IsNull()  ||  m_LockHolder->IsLockAcquired();
}

inline void
CNetCache_MessageHandler::x_ZeroSocketTimeout(void)
{
    STimeout to = {0, 0};
    GetSocket().SetTimeout(eIO_ReadWrite, &to);
}

inline void
CNetCache_MessageHandler::x_ResetSocketTimeout(void)
{
    STimeout to = {m_Server->GetInactivityTimeout(), 0};
    GetSocket().SetTimeout(eIO_ReadWrite, &to);
}

void
CNetCache_MessageHandler::OnOpen(void)
{
    CSocket& socket = GetSocket();
    socket.DisableOSSendDelay();
    x_ResetSocketTimeout();

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
        msg += "Connection is opened.";

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

    if (m_DeferredCmd) {
        if (!IsReadyToProcess()) {
            x_DeinitDiagnostics();
            return;
        }

        m_DeferredCmd = false;
        x_ProcessCommand();
    }
    else {
        if (!m_InRequest) {
            m_Context.Reset(new CRequestContext());
            m_Context->SetRequestID();
            x_InitDiagnostics();
            m_Stat.InitRequest();
            m_Stat.max_exec_time.AddSecond(m_Server->GetRequestTimeout());
            m_InRequest = true;
        }
        if (m_ReadBufPos == m_ReadBufSize) {
            EIO_Status status;
            {{
                CTimeGuard time_guard(m_Stat.comm_elapsed);
                x_ZeroSocketTimeout();
                status = socket.Read(m_ReadBuff, sizeof(m_ReadBuff),
                                     &m_ReadBufSize, eIO_ReadPlain);
                x_ResetSocketTimeout();
                ++m_Stat.io_blocks;
            }}
            m_ReadBufPos = 0;
            bool need_return = false;
            switch (status) {
            case eIO_Success:
                //break;
            case eIO_Timeout:
                /*this->OnTimeout();
                need_return = true;*/
                break;
            case eIO_Closed:
                OnCloseExt(eClientClose);
                x_InitDiagnostics();
                need_return = true;
                break;
            default:
                // TODO: ??? OnError
                need_return = true;
                break;
            }
            if (need_return) {
                m_ReadBufSize = 0;
                m_InRequest = false;
                x_DeinitDiagnostics();
                return;
            }
        }
        int message_tail = 0;
        char *buf_ptr = m_ReadBuff;
        size_t buf_size = m_ReadBufSize - m_ReadBufPos;
        while (buf_size != 0  &&  message_tail >= 0  &&  IsReadyToProcess())
        {
            message_tail = this->CheckMessage(&m_Buffer, buf_ptr, buf_size);
            if (message_tail >= 0) {
                this->OnMessage(m_Buffer);

                int consumed = buf_size - message_tail;
                buf_ptr += consumed;
                m_ReadBufPos += consumed;
                buf_size -= consumed;
            }
            else {
                m_ReadBufPos = m_ReadBufSize;
                buf_size = 0;
            }
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
    if (!m_DelayedWrite  ||  !IsReadyToProcess()) {
        x_DeinitDiagnostics();
        return;
    }
    bool res = true;
    if (m_OverflowWrite) {
        x_WriteBuf(NULL, 0);
    }
    else if (m_Reader.get()) {
        res = x_ProcessWriteAndReport(0);
    }
    else {
        res = x_CreateBlobReader();
    }
    m_DelayedWrite = m_DelayedWrite && res;
    if (!m_DelayedWrite) {
        if (m_LockHolder.NotNull()) {
            m_LockHolder->ReleaseLock();
            m_LockHolder.Reset();
        }
        OnRequestEnd();
    }

    x_DeinitDiagnostics();
}

bool
CNetCache_MessageHandler::x_CreateBlobReader(void)
{
    ICache::SBlobAccessDescr ba_descr;
    ba_descr.buf = 0;
    ba_descr.buf_size = 0;

    CBDB_Cache* bdb_cache = m_Server->GetCache();
    for (int repeats = 0; repeats < 1000; ++repeats) {
        CTimeGuard time_guard(m_Stat.db_elapsed, &m_Stat);
        bdb_cache->GetBlobAccess(m_ReqId, 0, kEmptyStr, &ba_descr);

        if (!ba_descr.blob_found) {
            // check if id is locked maybe blob record not
            // yet committed
            /*{{
                if (bdb_cache->IsLocked(m_BlobId)) {
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
            }}*/
            x_WriteMsg("ERR:", string("BLOB not found. ") + m_ReqId);
            return false;
        } else {
            break;
        }
    }

    if (ba_descr.blob_size == 0) {
        x_WriteMsg("OK:", "BLOB found. SIZE=0");
        return false;
    }

    m_Stat.blob_size = ba_descr.blob_size;

    // re-translate reader to the network
    m_Reader.reset(ba_descr.reader.release());
    if (!m_Reader.get()) {
        x_WriteMsg("ERR:", string("BLOB not found. ") + m_ReqId);
        return false;
    }

    // Write first chunk right here
    return x_ProcessWriteAndReport(ba_descr.blob_size, &m_ReqId);
}

bool
CNetCache_MessageHandler::x_ProcessWriteAndReport(unsigned      blob_size,
                                                  const string* req_id)
{
    if (blob_size) {
        string msg("BLOB found. SIZE=");
        string sz;
        NStr::UIntToString(sz, blob_size);
        msg += sz;
        x_PrepareMsg("OK:", msg);
    }

    size_t bytes_read = 0;
    ERW_Result res;
    {{
        CTimeGuard time_guard(m_Stat.db_elapsed, &m_Stat);
        res = m_Reader->Read(m_WriteBuff + m_WriteDataSize,
                             m_WriteBufSize - m_WriteDataSize,
                             &bytes_read);
    }}
    if (res != eRW_Success || !bytes_read) {
        m_Reader.reset(0);
        if (blob_size) {
            m_WriteDataSize = 0;
            string msg("BLOB not found. ");
            if (req_id) msg += *req_id;
            x_WriteMsg("ERR:", msg);
        }
        return false;
    }
    m_WriteDataSize += bytes_read;
    {{
        CTimeGuard time_guard(m_Stat.comm_elapsed, &m_Stat);
        x_WriteBuf(NULL, 0);
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
CNetCache_MessageHandler::x_PrepareMsg(const string&  prefix,
                                       const string&  msg)
{
    string err_msg(prefix);
    err_msg.append(msg);
    err_msg.append("\r\n");
    _TRACE("x_PrepareMsg " << prefix << msg);
    x_EnsureWriteBufSize(m_WriteDataSize + err_msg.size());
    memcpy(m_WriteBuff + m_WriteDataSize, err_msg.data(), err_msg.size());
    m_WriteDataSize += err_msg.size();
}

void
CNetCache_MessageHandler::x_WriteMsg(const string&  prefix,
                                     const string&  msg)
{
    x_PrepareMsg(prefix, msg);
    x_WriteBuf(NULL, 0);
}

inline void
CNetCache_MessageHandler::x_EnsureWriteBufSize(size_t size)
{
    if (size > m_WriteBufSize) {
        size_t new_size = max(size, m_WriteBufSize + kNetworkBufferSize);
        m_WriteBuff = static_cast<char*>(realloc(m_WriteBuff, new_size));
        m_WriteBufSize = new_size;
    }
}

void
CNetCache_MessageHandler::x_WriteBuf(const char* buf, size_t bytes)
{
    if (m_WriteDataSize != 0) {
        if (bytes != 0) {
            x_EnsureWriteBufSize(m_WriteDataSize + bytes);
            memcpy(m_WriteBuff + m_WriteDataSize, buf, bytes);
        }
        buf = m_WriteBuff;
        bytes += m_WriteDataSize;
    }

    size_t n_written;
    string msg;
    bool is_error = false;

    x_ZeroSocketTimeout();
    EIO_Status io_st = GetSocket().Write(buf, bytes, &n_written, eIO_WritePlain);
    x_ResetSocketTimeout();

    switch (io_st) {
    case eIO_Success:
    case eIO_Timeout:
    case eIO_Interrupt:
        break;
    case eIO_Closed:
        msg = "Communication error: peer closed connection (cannot send)";
        is_error = true;
        break;
    case eIO_InvalidArg:
        msg = "Communication error: invalid argument (cannot send)";
        is_error = true;
        break;
    default:
        msg = "Communication error (cannot send)";
        is_error = true;
        break;
    } // switch

    if (is_error) {
        msg.append(" IO block size=");
        msg.append(NStr::UIntToString(bytes));

        NCBI_THROW(CNetServiceException, eCommunicationError, msg);
    }

    if (n_written == bytes) {
        m_WriteDataSize = 0;
        m_OverflowWrite = false;
    }
    else {
        _ASSERT(bytes > n_written);
        m_WriteDataSize = bytes - n_written;
        x_EnsureWriteBufSize(m_WriteDataSize);
        memmove(m_WriteBuff, buf + n_written, m_WriteDataSize);
        m_OverflowWrite = true;
    }
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
    if (m_LockHolder.NotNull()) {
        m_LockHolder->ReleaseLock();
        m_LockHolder.Reset();
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
CNetCache_MessageHandler::OnTimer(void)
{
    x_InitDiagnostics();

    if (!m_InRequest)
        return;

    LOG_POST("Request execution timed out. Closing connection.");
    GetSocket().Close();
    OnCloseExt(eOurClose);

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
        x_WriteMsg("ERR:",
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
        msg += CTime(CTime::eCurrent).AsString();
        msg += " (start - ";
        msg += m_Stat.req_time.AsString();
        msg += ")\n\t";
        msg += m_Auth;
        msg += " \"";
        msg += m_Stat.request;
        msg += "\" ";
        msg += m_Stat.peer_address;
        msg += "\n\t";
        msg += "ConnTime=" + m_Stat.conn_time.AsString();
        msg += " BLOB key=";
        msg += m_ReqId;
        msg += " size=";
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
CNetCache_MessageHandler::x_CreateBlobWriter(void)
{
    // create the reader up front to guarantee correct BLOB locking
    // the possible problem (?) here is that we have to do double buffering
    // of the input stream
    CTimeGuard time_guard(m_Stat.db_elapsed, &m_Stat);
    m_Writer.reset(m_Server->GetCache()->GetWriteStream(
                            m_BlobId, m_ReqId, 0, kEmptyStr,/* do_id_lock,*/
                            m_Timeout, m_Auth));
}

void
CNetCache_MessageHandler::ProcessMsgTransmission(BUF buffer)
{
    if (!m_Writer.get()) {
        x_CreateBlobWriter();
    }

    string data;
    s_ReadBufToString(buffer, data);

    const char* buf = data.data();
    size_t buf_size = data.size();
    size_t bytes_written;
    if (m_SizeKnown && (buf_size > m_BlobSize)) {
        m_BlobSize = 0;
        x_WriteMsg("ERR:", "Blob overflow");
        m_InTransmission = false;
    }
    else {
        if (m_InTransmission || buf_size) {
            ERW_Result res = m_Writer->Write(buf, buf_size, &bytes_written);
            m_BlobSize -= buf_size;
            m_Stat.blob_size += buf_size;

            if (res != eRW_Success) {
                _TRACE("Transmission failed, socket " << &GetSocket());
                x_WriteMsg("ERR:", "Server I/O error");
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
                    x_WriteMsg("ERR:", "eCommunicationError:Unexpected EOF");
                }
                if (m_PutOK) {
                    _TRACE("OK, socket " << &GetSocket());
                    CTimeGuard time_guard(m_Stat.comm_elapsed, &m_Stat);
                    x_WriteMsg("OK:", "");
                    m_PutOK = false;
                }
            }
            catch (CException& ex) {
                // We should successfully restore after exception
                ERR_POST(ex);
                if (m_PutOK  ||  m_SizeKnown) {
                    x_WriteMsg("ERR:", "Error writing blob: " + NStr::Replace(ex.what(), "\n", "; "));
                    m_PutOK = false;
                }
            }

            // Forcibly close transmission - client is not going
            // to send us EOT
            m_InTransmission = false;
            m_SizeKnown = false;
            m_LockHolder->ReleaseLock();
            m_LockHolder.Reset();
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
CNetCache_MessageHandler::x_ProcessCommand(void)
{
    try {
        (this->*m_CmdProcessor)();
    }
    catch (CNetCacheException &ex)
    {
        ReportException(ex, "NC Server error: ", m_Stat.request);
        m_Server->RegisterException(m_Stat.op_code, m_Auth, ex);
    }
    catch (CNetServiceException& ex)
    {
        ReportException(ex, "NC Service exception: ", m_Stat.request);
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
            ReportException(ex, "NC BerkeleyDB error: ", m_Stat.request);
            m_Server->RegisterInternalErr(m_Stat.op_code, m_Auth);
        }
        throw;
    }
    catch (CException& ex)
    {
        ReportException(ex, "NC CException: ", m_Stat.request);
        m_Server->RegisterInternalErr(m_Stat.op_code, m_Auth);
    }
    catch (exception& ex)
    {
        ReportException(ex, "NC std::exception: ", m_Stat.request);
        m_Server->RegisterInternalErr(m_Stat.op_code, m_Auth);
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
        m_CmdProcessor = cmd.command->extra.processor;
        x_ProcessCommand();
    }
    catch (CNSProtoParserException& ex)
    {
        ReportException(ex, "NC request parser error: ", request);
        x_WriteMsg("ERR:", ex.GetMsg());
        m_Server->RegisterProtocolErr(
                            SBDB_CacheUnitStatistics::eErr_Unknown, m_Auth);
    }
}

void
CNetCache_MessageHandler::ProcessAlive(void)
{
    x_WriteMsg("OK:", "");
}

void
CNetCache_MessageHandler::ProcessOK(void)
{
}

void
CNetCache_MessageHandler::ProcessMoni(void)
{
    GetSocket().DisableOSSendDelay(false);
    x_WriteMsg("OK:", "Monitor for " NETCACHED_VERSION "\n");
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
        x_WriteMsg("ERR:", "Server does not support sessions ");
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
        x_WriteMsg("OK:", "");
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
    x_WriteMsg("OK:", "");
}

void
CNetCache_MessageHandler::ProcessVersion(void)
{
    x_WriteMsg("OK:", NETCACHED_VERSION);
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
    x_WriteMsg("OK:", "");
}

void
CNetCache_MessageHandler::ProcessRemove(void)
{
    if (m_LockHolder.IsNull()) {
        m_BlobId = CNetCacheKey::GetBlobId(m_ReqId);
        m_LockHolder = m_Server->AcquireWriteIdLock(m_BlobId);
    }

    if (m_LockHolder->IsLockAcquired()) {
        m_Server->GetCache()->Remove(m_ReqId);

        m_LockHolder->ReleaseLock();
        m_LockHolder.Reset();
    }
    else {
        DeferProcessing();
    }
}

void
CNetCache_MessageHandler::ProcessRemove2(void)
{
    ProcessRemove();
    if (!m_DeferredCmd) {
        x_WriteMsg("OK:", "");
    }
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
    x_WriteMsg("OK:", "");
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

    x_WriteMsg("OK:", "");
}

void
CNetCache_MessageHandler::ProcessGet(void)
{
    if (m_LockHolder.IsNull()) {
        m_Stat.op_code = SBDB_CacheUnitStatistics::eErr_Get;
        m_Stat.blob_id = m_ReqId;

        m_BlobId = CNetCacheKey::GetBlobId(m_ReqId);

        m_LockHolder = m_Server->AcquireReadIdLock(m_BlobId);
    }

    if (m_LockHolder->IsLockAcquired()) {
        if (x_CreateBlobReader()) {
            BeginDelayedWrite();
        }
    }
    else {
        if (m_Policy /* m_NoLock */) {
            m_LockHolder->ReleaseLock();
            m_LockHolder.Reset();

            x_WriteMsg("ERR:", "BLOB locked by another client");
            return;
        }

        DeferProcessing();
    }
}

void
CNetCache_MessageHandler::ProcessPut(void)
{
    x_WriteMsg("ERR:", "Obsolete");
}

void
CNetCache_MessageHandler::ProcessPut2(void)
{
    if (m_LockHolder.IsNull()) {
        m_Stat.op_code = SBDB_CacheUnitStatistics::eErr_Put;

        //bool do_id_lock = true;
        _TRACE("Getting an id, socket " << &GetSocket());
        if (m_ReqId.empty()) {
            CTimeGuard time_guard(m_Stat.db_elapsed, &m_Stat);
            m_BlobId = m_Server->GetCache()->GetNextBlobId(/*do_id_lock*/);
            time_guard.Stop();
            CNetCacheKey::GenerateBlobKey(&m_ReqId, m_BlobId,
                                        m_Server->GetHost(), m_Server->GetPort());
            //do_id_lock = false;
        }
        else {
            m_BlobId = CNetCacheKey::GetBlobId(m_ReqId);
        }
        m_Stat.blob_id = m_ReqId;
        _TRACE("Got id " << m_BlobId);

        CTimeGuard time_guard(m_Stat.comm_elapsed, &m_Stat);
        x_WriteMsg("ID:", m_ReqId);

        m_LockHolder = m_Server->AcquireWriteIdLock(m_BlobId);
    }

    if (m_LockHolder->IsLockAcquired()) {
        x_CreateBlobWriter();

        _TRACE("Begin read transmission");
        BeginReadTransmission();
    }
    else {
        DeferProcessing();
    }
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
    x_WriteMsg("OK:", str);
}

void
CNetCache_MessageHandler::ProcessGetBlobOwner(void)
{
    string owner;
    m_Server->GetCache()->GetBlobOwner(m_ReqId, 0, kEmptyStr, &owner);

    x_WriteMsg("OK:", owner);
}

void
CNetCache_MessageHandler::ProcessIsLock(void)
{
    TRWLockHolderRef holder = m_Server->AcquireReadIdLock(
                                            CNetCacheKey::GetBlobId(m_ReqId));

    if (holder->IsLockAcquired()) {
        x_WriteMsg("OK:", "1");
    } else {
        x_WriteMsg("OK:", "0");
    }

    holder->ReleaseLock();
}

void
CNetCache_MessageHandler::ProcessGetSize(void)
{
    if (m_LockHolder.IsNull()) {
        m_BlobId = CNetCacheKey::GetBlobId(m_ReqId);
        m_LockHolder = m_Server->AcquireReadIdLock(m_BlobId);
    }

    if (m_LockHolder->IsLockAcquired()) {
        size_t size;
        if (!m_Server->GetCache()->GetSizeEx(m_ReqId, 0, kEmptyStr, &size)) {
            x_WriteMsg("ERR:", "BLOB not found. " + m_ReqId);
        } else {
            x_WriteMsg("OK:", NStr::UIntToString(size));
        }
    }
    else {
        DeferProcessing();
    }
}

void
CNetCache_MessageHandler::Process_IC_SetTimeStampPolicy(void)
{
    m_ICache->SetTimeStampPolicy(m_Policy, m_Timeout, m_MaxTimeout);
    x_WriteMsg("OK:", "");
}


void
CNetCache_MessageHandler::Process_IC_GetTimeStampPolicy(void)
{
    ICache::TTimeStampFlags flags = m_ICache->GetTimeStampPolicy();
    string str;
    NStr::UIntToString(str, flags);
    x_WriteMsg("OK:", str);
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
        x_WriteMsg("ERR:", "Invalid version retention code");
        return;
    }
    m_ICache->SetVersionRetention(policy);
    x_WriteMsg("OK:", "");
}

void
CNetCache_MessageHandler::Process_IC_GetVersionRetention(void)
{
    int p = m_ICache->GetVersionRetention();
    string str;
    NStr::IntToString(str, p);
    x_WriteMsg("OK:", str);
}

void
CNetCache_MessageHandler::Process_IC_GetTimeout(void)
{
    int to = m_ICache->GetTimeout();
    string str;
    NStr::UIntToString(str, to);
    x_WriteMsg("OK:", str);
}

void
CNetCache_MessageHandler::Process_IC_IsOpen(void)
{
    bool io = m_ICache->IsOpen();
    string str;
    NStr::UIntToString(str, (int)io);
    x_WriteMsg("OK:", str);
}

bool
CNetCache_MessageHandler::x_GetBlobId(const string&    key,
                                      int              version,
                                      const string&    subkey,
                                      bool             need_create,
                                      unsigned int*    blob_id)
{
    unsigned int volume_id;
    unsigned int split_id;
    unsigned int overflow;
    *blob_id = 0;
    CBDB_Cache::EBlobCheckinRes res
        = dynamic_cast<CBDB_Cache*>(m_ICache)
                  ->BlobCheckIn(*blob_id,
                                key,
                                version,
                                subkey,
                                need_create? CBDB_Cache::eBlobCheckIn_Create:
                                             CBDB_Cache::eBlobCheckIn,
                                &volume_id,
                                &split_id,
                                &overflow);
    return res != CBDB_Cache::EBlobCheckIn_NotFound;
}

void
CNetCache_MessageHandler::Process_IC_Store(void)
{
    if (m_LockHolder.IsNull()) {
        x_WriteMsg("OK:", "");

        x_GetBlobId(m_Key, m_Version, m_SubKey, true, &m_BlobId);
        m_LockHolder = m_Server->AcquireWriteIdLock(m_BlobId);
    }

    if (m_LockHolder->IsLockAcquired()) {
        m_Writer.reset(m_ICache->GetWriteStream(m_Key, m_Version, m_SubKey,
                                                m_Timeout, m_Auth));

        BeginReadTransmission();
    }
    else {
        DeferProcessing();
    }
}

void
CNetCache_MessageHandler::Process_IC_StoreBlob(void)
{
    if (m_LockHolder.IsNull()) {
        x_WriteMsg("OK:", "");

        x_GetBlobId(m_Key, m_Version, m_SubKey, true, &m_BlobId);
        m_LockHolder = m_Server->AcquireWriteIdLock(m_BlobId);
    }

    if (m_LockHolder->IsLockAcquired()) {
        if (m_BlobSize == 0) {
            m_ICache->Store(m_Key, m_Version, m_SubKey, 0, 0, m_Timeout, m_Auth);
            return;
        }

        m_SizeKnown = true;

        m_Writer.reset(m_ICache->GetWriteStream(m_Key, m_Version, m_SubKey,
                                                m_Timeout, m_Auth));

        BeginReadTransmission();
    }
    else {
        DeferProcessing();
    }
}


void
CNetCache_MessageHandler::Process_IC_GetSize(void)
{
    if (m_LockHolder.IsNull()
        &&  x_GetBlobId(m_Key, m_Version, m_SubKey, false, &m_BlobId))
    {
        m_LockHolder = m_Server->AcquireReadIdLock(m_BlobId);
    }

    if (m_LockHolder.NotNull() && !m_LockHolder->IsLockAcquired()) {
        DeferProcessing();
        return;
    }

    size_t sz = 0;
    if (m_LockHolder.NotNull()) {
        sz = m_ICache->GetSize(m_Key, m_Version, m_SubKey);
        m_LockHolder->ReleaseLock();
        m_LockHolder.Reset();
    }
    string str;
    NStr::UIntToString(str, sz);
    x_WriteMsg("OK:", str);
}

void
CNetCache_MessageHandler::Process_IC_GetBlobOwner(void)
{
    string owner;
    m_ICache->GetBlobOwner(m_Key, m_Version, m_SubKey, &owner);
    x_WriteMsg("OK:", owner);
}

void
CNetCache_MessageHandler::Process_IC_Read(void)
{
    bool blob_found = true;
    if (m_LockHolder.IsNull()) {
        blob_found = x_GetBlobId(m_Key, m_Version, m_SubKey, false, &m_BlobId);
        if (blob_found) {
            m_LockHolder = m_Server->AcquireReadIdLock(m_BlobId);
        }
    }

    if (!blob_found) {
        string msg = "BLOB not found. ";
        //msg += req_id;
        x_WriteMsg("ERR:", msg);
        return;
    }

    if (m_LockHolder->IsLockAcquired()) {
        ICache::SBlobAccessDescr ba_descr;
        ba_descr.buf = 0;
        ba_descr.buf_size = 0;
        m_ICache->GetBlobAccess(m_Key, m_Version, m_SubKey, &ba_descr);

        if (ba_descr.blob_size == 0) {
            x_WriteMsg("OK:", "BLOB found. SIZE=0");
            m_LockHolder->ReleaseLock();
            m_LockHolder.Reset();
            return;
        }

        // re-translate reader to the network

        m_Reader.reset(ba_descr.reader.release());
        if (!m_Reader.get()) {
            string msg = "BLOB not found. ";
            //msg += req_id;
            x_WriteMsg("ERR:", msg);
            m_LockHolder->ReleaseLock();
            m_LockHolder.Reset();
            return;
        }
        // Write first chunk right here
        char buf[4096];
        size_t bytes_read;
        ERW_Result io_res = m_Reader->Read(buf, sizeof(buf), &bytes_read);
        if (io_res != eRW_Success || !bytes_read) { // TODO: should we check here for bytes_read?
            string msg = "BLOB not found. ";
            x_WriteMsg("ERR:", msg);
            m_Reader.reset(0);
            m_LockHolder->ReleaseLock();
            m_LockHolder.Reset();
            return;
        }
        string msg("BLOB found. SIZE=");
        string sz;
        NStr::UIntToString(sz, ba_descr.blob_size);
        msg += sz;
        x_WriteMsg("OK:", msg);

        // translate BLOB fragment to the network
        x_WriteBuf(buf, bytes_read);

        // TODO: Can we check here that bytes_read is less than sizeof(buf)
        // and optimize out delayed write?
        BeginDelayedWrite();
    }
    else {
        DeferProcessing();
    }
}

void
CNetCache_MessageHandler::Process_IC_Remove(void)
{
    if (m_LockHolder.IsNull()
        &&  x_GetBlobId(m_Key, m_Version, m_SubKey, false, &m_BlobId))
    {
        m_LockHolder = m_Server->AcquireWriteIdLock(m_BlobId);
    }

    if (m_LockHolder.NotNull()  &&  !m_LockHolder->IsLockAcquired()) {
        DeferProcessing();
    }
    else {
        if (m_LockHolder.NotNull()) {
            m_ICache->Remove(m_Key, m_Version, m_SubKey);
            m_LockHolder->ReleaseLock();
            m_LockHolder.Reset();
        }
        x_WriteMsg("OK:", "");
    }
}

void
CNetCache_MessageHandler::Process_IC_RemoveKey(void)
{
    // TODO: Very bad idea to delete everything without any locks
    m_ICache->Remove(m_Key);
    x_WriteMsg("OK:", "");
}

void
CNetCache_MessageHandler::Process_IC_GetAccessTime(void)
{
    time_t t = m_ICache->GetAccessTime(m_Key, m_Version, m_SubKey);
    string str;
    NStr::UIntToString(str, static_cast<unsigned long>(t));
    x_WriteMsg("OK:", str);
}

void
CNetCache_MessageHandler::Process_IC_HasBlobs(void)
{
    bool hb = m_ICache->HasBlobs(m_Key, m_SubKey);
    string str;
    NStr::UIntToString(str, (int)hb);
    x_WriteMsg("OK:", str);
}

void
CNetCache_MessageHandler::Process_IC_Purge1(void)
{
    ICache::EKeepVersions keep_versions = (ICache::EKeepVersions)m_Policy;
    // TODO: Very bad idea to delete everything without any locks
    m_ICache->Purge(m_Timeout, keep_versions);
    x_WriteMsg("OK:", "");
}


END_NCBI_SCOPE
