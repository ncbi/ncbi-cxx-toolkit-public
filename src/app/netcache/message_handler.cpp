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
 * Authors:  Victor Joukov
 *
 * File Description: Network cache daemon
 *
 */
#include <ncbi_pch.hpp>

#include "message_handler.hpp"
#include "nc_handler.hpp"
#include "ic_handler.hpp"
#include "netcache_version.hpp"

// For emulation of CTransmissionReader
#include <util/util_exception.hpp>


BEGIN_NCBI_SCOPE

CNetCache_MessageHandler::CNetCache_MessageHandler(CNetCacheServer* server)
    : m_Server(server), m_SeenCR(false), m_InTransmission(false),
    m_ICHandler(new CICacheHandler(this)),
    m_NCHandler(new CNetCacheHandler(this)),
    m_LastHandler(0)
{
}


void CNetCache_MessageHandler::BeginReadTransmission()
{
    m_InTransmission = true;
    m_SigRead = 0;
    m_Signature = 0;
    m_ProcessMessage = &CNetCache_MessageHandler::ProcessMsgTransmission;
}


void CNetCache_MessageHandler::BeginDelayedWrite()
{
    m_DelayedWrite = true;
}


CNetCacheServer* CNetCache_MessageHandler::GetServer()
{
    return m_Server;
}


int CNetCache_MessageHandler::CheckMessage(BUF* buffer, const void *data, size_t size)
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
                    m_InTransmission = false;
                    return size-n-1;
                }
                if (m_ByteSwap)
                    m_Length = CByteSwap::GetInt4((unsigned char*)&m_Length);
                // Length read, read data
                start = n + 1;
            }
        } else {
            unsigned chunk_len = min(m_Length, size-start); 
            BUF_Write(buffer, msg+start, chunk_len);
            m_Length -= chunk_len;
            n += chunk_len - 1;
            if (!m_Length) {
                // Read next length, in next CheckMessage invokation
                m_LenRead = 0;
                return size-n-1;
            }
        }
    }
    return -1;
}


EIO_Event CNetCache_MessageHandler::GetEventsToPollFor(const CTime**) const
{
    if (m_DelayedWrite) return eIO_Write;
    return eIO_Read;
}


void CNetCache_MessageHandler::OnOpen(void)
{
    CSocket& socket = GetSocket();
    socket.DisableOSSendDelay();
    STimeout to = {m_Server->GetInactivityTimeout(), 0};
    socket.SetTimeout(eIO_ReadWrite, &to);

    if (IsLogging() || IsMonitoring()) {
        m_Stat.blob_size = m_Stat.io_blocks = 0;
        m_Stat.conn_time = m_Server->GetTimer().GetLocalTime();
        m_Stat.comm_elapsed = m_Stat.elapsed = 0;
        m_Stat.peer_address = socket.GetPeerAddress();
                    
        // get only host name
        string::size_type offs = m_Stat.peer_address.find_first_of(":");
        if (offs != string::npos) {
            m_Stat.peer_address.erase(offs, m_Stat.peer_address.length());
        }
        m_Stat.req_code = '?';

        m_StopWatch.Start();
    }

    m_ProcessMessage = &CNetCache_MessageHandler::ProcessMsgAuth;
    m_DelayedWrite = false;
}


void CNetCache_MessageHandler::OnWrite(void)
{
    if (!m_DelayedWrite) return;
    if (!m_LastHandler) {
        m_DelayedWrite = false;
        return;
    }
    bool res = m_LastHandler->ProcessWrite();
    m_DelayedWrite = m_DelayedWrite && res;
}


void CNetCache_MessageHandler::OnClose(void)
{
}


void CNetCache_MessageHandler::OnTimeout(void)
{
}


void CNetCache_MessageHandler::OnOverflow(void)
{
}


void CNetCache_MessageHandler::OnMessage(BUF buffer)
{
    if (m_Server->ShutdownRequested()) {
        WriteMsg("ERR:",
            "NetSchedule server is shutting down. Session aborted.");
        return;
    }

    // TODO: move exception handling here
    // try {
        (this->*m_ProcessMessage)(buffer);
    // }
}


static void s_BufReadHelper(void* data, void* ptr, size_t size)
{
    ((string*) data)->append((const char *) ptr, size);
}


static void s_ReadBufToString(BUF buf, string& str)
{
    str.erase();
    size_t size = BUF_Size(buf);
    str.reserve(size);
    BUF_PeekAtCB(buf, 0, s_BufReadHelper, &str, size);
    BUF_Read(buf, NULL, size);
}


void CNetCache_MessageHandler::ProcessMsgAuth(BUF buffer)
{
    s_ReadBufToString(buffer, m_Auth);
    m_ProcessMessage = &CNetCache_MessageHandler::ProcessMsgRequest;
}


void CNetCache_MessageHandler::ProcessSM(CSocket& socket, string& req)
{
    // SMR host pid     -- registration
    // or
    // SMU host pid     -- unregistration
    //
    if (!m_Server->IsManagingSessions()) {
        WriteMsg("ERR:", "Server does not support sessions ");
        return;
    }

    if (req[0] == 'S' && req[1] == 'M') {
        bool reg;
        if (req[2] == 'R') {
            reg = true;
        } else
        if (req[2] == 'U') {
            reg = false;
        } else {
            goto err;
        }
        req.erase(0, 3);
        NStr::TruncateSpacesInPlace(req, NStr::eTrunc_Begin);
        string host, port_str;
        bool split = NStr::SplitInTwo(req, " ", host, port_str);
        if (!split) {
            goto err;
        }
        unsigned port = NStr::StringToUInt(port_str);

        if (!port || host.empty()) {
            goto err;
        }

        if (reg) {
            m_Server->RegisterSession(host, port);
        } else {
            m_Server->UnRegisterSession(host, port);
        }
        WriteMsg("OK:", "");
    } else {
        err:
        WriteMsg("ERR:", "Invalid request ");
    }
}


void CNetCache_MessageHandler::ProcessMsgRequest(BUF buffer)
{
    CSocket& socket = GetSocket();
    unsigned buf_size = 64*1024; // TODO: server buf_size or buffer pool
    AutoPtr<char, ArrayDeleter<char> >
        buf(new char[buf_size + 256]);

    NetCache_RequestInfo info;

    string request;
    s_ReadBufToString(buffer, request);
    try {
        NetCache_RequestInfo* inf = 0;
        if (IsLogging() || IsMonitoring()) {
            info.Init();
            inf = &info;
        }
        if (request.length() < 2) { 
            WriteMsg("ERR:", "Invalid request");
            m_Server->RegisterProtocolErr(
                SBDB_CacheUnitStatistics::eErr_Unknown, request);
            return;
        }

        // check if it is NC or IC

        if (request[0] == 'I' && request[1] == 'C') { // ICache request
            m_LastHandler = m_ICHandler.get();
            m_ICHandler->SetSocket(&socket);
            m_ICHandler->ProcessRequest(request, m_Auth,
                buf.get(), buf_size,
                m_Stat, inf);
        } else 
        if (request[0] == 'A' && request[1] == '?') { // Alive?
            WriteMsg("OK:", "");
        } else
        if (request[0] == 'S' && request[1] == 'M') { // Session management
            if (inf) inf->type = "Session";
            ProcessSM(socket, request);
            // need to disconnect after reg-unreg
            socket.Close();
        } else 
        if (request[0] == 'O' && request[1] == 'K') {
        } else
        if (request == "MONI") {
            socket.DisableOSSendDelay(false);
            WriteMsg("OK:", "Monitor for " NETCACHED_VERSION "\n");
            m_Server->SetMonitorSocket(socket);
            // Avoid handling closing connection
            return;
        } else {
            m_LastHandler = m_NCHandler.get();
            m_NCHandler->SetSocket(&socket);
            m_NCHandler->ProcessRequest(request, m_Auth,
                buf.get(), buf_size,
                m_Stat, inf);
        }

        // Monitoring
        //
        if (IsMonitoring()) {

            m_Stat.elapsed = m_StopWatch.Elapsed();

            string msg, tmp;
            msg += m_Auth;
            msg += "\"";
            msg += request;
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
            msg += info.type + ":" + info.details;

            MonitorPost(msg);
        }

/*
        // cleaning up the input wire, in case if there is some
        // trailing input.
        // i don't want to know wnat is it, but if i don't read it
        // some clients will receive PEER RESET error trying to read
        // servers answer.
        //
        // I'm reading fixed length sized buffer, so any huge ill formed 
        // request still will end up with an error on the client side
        // (considered a client's fault (DDOS attempt))
        to.sec = to.usec = 0;
        socket.SetTimeout(eIO_Read, &to);
        socket.Read(NULL, 1024);
*/

        //
        // Logging.
        //
        if (IsLogging()) {
            m_Stat.elapsed = m_StopWatch.Elapsed();
            CNetCache_Logger* log = m_Server->GetLogger();
            _ASSERT(log);
            log->Put(m_Auth, m_Stat, info.blob_id);
        }
    } 
    catch (CNetCacheException &ex)
    {
        string msg;
        msg = "NC Server error: ";
        msg.append(ex.what());
        msg.append(" client=");  msg.append(m_Auth);
        msg.append(" request='");msg.append(request); msg.append("'");
        msg.append(" peer="); msg.append(socket.GetPeerAddress());
        msg.append(" blobsize=");
            msg.append(NStr::UIntToString(m_Stat.blob_size));
        msg.append(" io blocks=");
            msg.append(NStr::UIntToString(m_Stat.io_blocks));

        ERR_POST(msg);

        m_Server->RegisterException(m_Stat.op_code, m_Auth, ex);

        if (IsMonitoring()) {
            MonitorPost(msg);
        }
    }
    catch (CNetServiceException& ex)
    {
        string msg;
        msg = "NC Service exception: ";
        msg.append(ex.what());
        msg.append(" client=");  msg.append(m_Auth);
        msg.append(" request='");msg.append(request); msg.append("'");
        msg.append(" peer="); msg.append(socket.GetPeerAddress());
        msg.append(" blobsize=");
            msg.append(NStr::UIntToString(m_Stat.blob_size));
        msg.append(" io blocks=");
            msg.append(NStr::UIntToString(m_Stat.io_blocks));

        ERR_POST(msg);

        m_Server->RegisterException(m_Stat.op_code, m_Auth, ex);

        if (IsMonitoring()) {
            MonitorPost(msg);
        }
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
            string msg;
            msg = "NC BerkeleyDB error: ";
            msg.append(ex.what());
            msg.append(" client=");  msg.append(m_Auth);
            msg.append(" request='");msg.append(request); msg.append("'");
            msg.append(" peer="); msg.append(socket.GetPeerAddress());
            msg.append(" blobsize=");
                msg.append(NStr::UIntToString(m_Stat.blob_size));
            msg.append(" io blocks=");
                msg.append(NStr::UIntToString(m_Stat.io_blocks));
            ERR_POST(msg);

            if (IsMonitoring()) {
                MonitorPost(msg);
            }

            m_Server->RegisterInternalErr(m_Stat.op_code, m_Auth);
        }
        throw;
    }
    catch (exception& ex)
    {
        string msg;
        msg = "NC std::exception: ";
        msg.append(ex.what());
        msg.append(" client="); msg.append(m_Auth);
        msg.append(" request='"); msg.append(request); msg.append("'");
        msg.append(" peer="); msg.append(socket.GetPeerAddress());
        msg.append(" blobsize=");
            msg.append(NStr::UIntToString(m_Stat.blob_size));
        msg.append(" io blocks=");
            msg.append(NStr::UIntToString(m_Stat.io_blocks));

        ERR_POST(msg);

        m_Server->RegisterInternalErr(m_Stat.op_code, m_Auth);

        if (IsMonitoring()) {
            MonitorPost(msg);
        }
    }
}


void CNetCache_MessageHandler::ProcessMsgTransmission(BUF buffer)
{
    if (m_LastHandler) {
        string data;
        s_ReadBufToString(buffer, data);
        bool res = m_LastHandler->ProcessTransmission(
            data.data(),
            data.size(),
            m_InTransmission ?
                CNetCache_RequestHandler::eTransmissionMoreBuffers :
                CNetCache_RequestHandler::eTransmissionLastBuffer);
        m_InTransmission = m_InTransmission && res;
    }
    if (!m_InTransmission)
        m_ProcessMessage = &CNetCache_MessageHandler::ProcessMsgRequest;
}


END_NCBI_SCOPE
