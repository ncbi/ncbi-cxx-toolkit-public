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
 * Author:  Anatoliy Kuznetsov
 *
 * File Description:
 *   Implementation of NetSchedule client.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_conn_exception.hpp>
#include <connect/netschedule_client.hpp>
#include <util/request_control.hpp>
#include <memory>


BEGIN_NCBI_SCOPE


const string kNetSchedule_KeyPrefix = "JSID";

/// Request rate controller (one for all client instances)
/// Default limitation is 20000 requests per minute
///
/// @internal
///
static CRequestRateControl s_Throttler(20000, CTimeSpan(60,0));



unsigned CNetSchedule_GetJobId(const string&  key_str)
{
    unsigned job_id;
    const char* ch = key_str.c_str();

    if (isdigit(*ch)) {
        job_id = (unsigned) atoi(ch);
        if (job_id) {
            return job_id;
        }        
    }

    for (;*ch != 0 && *ch != '_'; ++ch) {
    }
    ++ch;
    for (;*ch != 0 && *ch != '_'; ++ch) {
    }
    ++ch;
    if (*ch) {
        job_id = (unsigned) atoi(ch);
        if (job_id) {
            return job_id;
        }
    }
    NCBI_THROW(CNetScheduleException, eKeyFormatError, "Key syntax error.");
}

void CNetSchedule_ParseJobKey(CNetSchedule_Key* key, const string& key_str)
{
    _ASSERT(key);

    // JSID_01_1_MYHOST_9000

    const char* ch = key_str.c_str();
    key->hostname = key->prefix = kEmptyStr;

    // prefix

    for (;*ch && *ch != '_'; ++ch) {
        key->prefix += *ch;
    }
    if (*ch == 0) {
        NCBI_THROW(CNetScheduleException, eKeyFormatError, "Key syntax error.");
    }
    ++ch;

    if (key->prefix != kNetSchedule_KeyPrefix) {
        NCBI_THROW(CNetScheduleException, eKeyFormatError, 
                                       "Key syntax error. Invalid prefix.");
    }

    // version
    key->version = atoi(ch);
    while (*ch && *ch != '_') {
        ++ch;
    }
    if (*ch == 0) {
        NCBI_THROW(CNetScheduleException, eKeyFormatError, "Key syntax error.");
    }
    ++ch;

    // id
    key->id = (unsigned) atoi(ch);
    while (*ch && *ch != '_') {
        ++ch;
    }
    if (*ch == 0 || key->id == 0) {
        NCBI_THROW(CNetScheduleException, eKeyFormatError, "Key syntax error.");
    }
    ++ch;


    // hostname
    for (;*ch && *ch != '_'; ++ch) {
        key->hostname += *ch;
    }
    if (*ch == 0) {
        NCBI_THROW(CNetScheduleException, eKeyFormatError, "Key syntax error.");
    }
    ++ch;

    // port
    key->port = atoi(ch);
}

void CNetSchedule_GenerateJobKey(string*        key, 
                                  unsigned       id, 
                                  const string&  host, 
                                  unsigned short port)
{
    string tmp;
    *key = "JSID_01";  

    NStr::IntToString(tmp, id);
    *key += "_";
    *key += tmp;

    *key += "_";
    *key += host;    

    NStr::IntToString(tmp, port);
    *key += "_";
    *key += tmp;
}




CNetScheduleClient::CNetScheduleClient(const string& client_name,
                                       const string& queue_name)
    : CNetServiceClient(client_name),
      m_Queue(queue_name)
{
}

CNetScheduleClient::CNetScheduleClient(const string&  host,
                                       unsigned short port,
                                       const string&  client_name,
                                       const string&  queue_name)
    : CNetServiceClient(host, port, client_name),
      m_Queue(queue_name)
{
}

CNetScheduleClient::CNetScheduleClient(CSocket*      sock,
                                       const string& client_name,
                                       const string& queue_name)
    : CNetServiceClient(sock, client_name),
      m_Queue(queue_name),
      m_RequestRateControl(true)
{
    if (m_Sock) {
        m_Sock->DisableOSSendDelay();
        RestoreHostPort();
    }
}

CNetScheduleClient::~CNetScheduleClient()
{
}

string CNetScheduleClient::StatusToString(EJobStatus status)
{
    switch(status) {
    case eJobNotFound: return "NotFound";
    case ePending:     return "Pending";
    case eRunning:     return "Running";
    case eReturned:    return "Returned";
    case eCanceled:    return "Canceled";
    case eFailed:      return "Failed";
    case eDone:        return "Done";
    default: _ASSERT(0);
    }
    return kEmptyStr;
}


void CNetScheduleClient::ActivateRequestRateControl(bool on_off)
{
    m_RequestRateControl = on_off;
}

string CNetScheduleClient::SubmitJob(const string& input)
{
    if (input.length() > kNetScheduleMaxDataSize) {
        NCBI_THROW(CNetScheduleException, eDataTooLong, 
            "Input data too long.");
    }
    if (m_RequestRateControl) {
        s_Throttler.Approve(CRequestRateControl::eSleep);
    }

    CheckConnect(kEmptyStr);
    CSockGuard sg(*m_Sock);

    MakeCommandPacket(&m_Tmp, "SUBMIT \"");
    m_Tmp.append(input);
    m_Tmp.append("\"");

    WriteStr(m_Tmp.c_str(), m_Tmp.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Communication error");
    }
    TrimPrefix(&m_Tmp);

    if (m_Tmp.empty()) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Invalid server response. Empty key.");
    }

    return m_Tmp;
}

CNetScheduleClient::EJobStatus 
CNetScheduleClient::SubmitJobAndWait(const string&  input,
                                     string*        job_key,
                                     int*           ret_code,
                                     string*        output, 
                                     string*        err_msg,
                                     unsigned       wait_time,
                                     unsigned short udp_port)
{
    _ASSERT(job_key);
    _ASSERT(ret_code);
    _ASSERT(output);
    _ASSERT(wait_time);
    _ASSERT(udp_port);

    if (input.length() > kNetScheduleMaxDataSize) {
        NCBI_THROW(CNetScheduleException, eDataTooLong, 
            "Input data too long.");
    }
    if (m_RequestRateControl) {
        s_Throttler.Approve(CRequestRateControl::eSleep);
    }
    *output = kEmptyStr;

    {{
    CheckConnect(kEmptyStr);
    CSockGuard sg(*m_Sock);

    MakeCommandPacket(&m_Tmp, "SUBMIT \"");
    m_Tmp.append(input);
    m_Tmp.append("\" ");
    m_Tmp.append(NStr::UIntToString(udp_port));
    m_Tmp.append(" ");
    m_Tmp.append(NStr::UIntToString(wait_time));
    WriteStr(m_Tmp.c_str(), m_Tmp.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Communication error");
    }

    }}
    TrimPrefix(&m_Tmp);

    if (m_Tmp.empty()) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Invalid server response. Empty key.");
    }

    *job_key = m_Tmp;

    m_JobKey.id = 0;
    CNetSchedule_ParseJobKey(&m_JobKey, *job_key);

    WaitJobNotification(wait_time, udp_port, m_JobKey.id);

    return GetStatus(*job_key, ret_code, output, err_msg);
}

void CNetScheduleClient::WaitJobNotification(unsigned       wait_time,
                                             unsigned short udp_port,
                                             unsigned       job_id)
{
    _ASSERT(wait_time);

    EIO_Status status;
    int    sig_buf[4];
    const char* sig = "JNTF";
    memcpy(sig_buf, sig, 4);

    int    buf[1024/sizeof(int)];
    char*  chr_buf = (char*) buf;

    STimeout to;
    to.sec = wait_time;
    to.usec = 0;

    CDatagramSocket  udp_socket;
    udp_socket.SetReuseAddress(eOn);
    STimeout rto;
    rto.sec = rto.usec = 0;
    udp_socket.SetTimeout(eIO_Read, &rto);

    status = udp_socket.Bind(udp_port);
    if (eIO_Success != status) {
        return;
    }
    time_t curr_time, start_time, end_time;

    start_time = time(0);
    end_time = start_time + wait_time;

    for (;true;) {

        curr_time = time(0);
        to.sec = end_time - curr_time;  // remaining
        if (to.sec <= 0) {
            break;
        }
        status = udp_socket.Wait(&to);
        if (eIO_Success != status) {
            continue;
        }
        size_t msg_len;
        status = udp_socket.Recv(buf, sizeof(buf), &msg_len, &m_Tmp);
        _ASSERT(status != eIO_Timeout); // because we Wait()-ed
        if (eIO_Success == status) {
            if (buf[0] != sig_buf[0]) {
                continue;
            }

            const char* job = chr_buf + 5;
            unsigned notif_job_id = (unsigned)::atoi(job);
            if (notif_job_id == job_id) {
                return;
            }
        } 
    } // for

}



void CNetScheduleClient::CancelJob(const string& job_key)
{
    if (m_RequestRateControl) {
        s_Throttler.Approve(CRequestRateControl::eSleep);
    }

    CheckConnect(job_key);
    CSockGuard sg(*m_Sock);

    CommandInitiate("CANCEL ", job_key, &m_Tmp);

    CheckOK(&m_Tmp);
}

void CNetScheduleClient::DropJob(const string& job_key)
{
    if (m_RequestRateControl) {
        s_Throttler.Approve(CRequestRateControl::eSleep);
    }

    CheckConnect(job_key);
    CSockGuard sg(*m_Sock);

    CommandInitiate("DROJ ", job_key, &m_Tmp);

    CheckOK(&m_Tmp);
}


void CNetScheduleClient::SetRunTimeout(const string& job_key, 
                                       unsigned      time_to_run)
{
    if (m_RequestRateControl) {
        s_Throttler.Approve(CRequestRateControl::eSleep);
    }
    CheckConnect(job_key);
    CSockGuard sg(*m_Sock);

    MakeCommandPacket(&m_Tmp, "JRTO ");

    m_Tmp.append(job_key);
    m_Tmp.append(" ");
    m_Tmp.append(NStr::IntToString(time_to_run));

    WriteStr(m_Tmp.c_str(), m_Tmp.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Communication error");
    }

    CheckOK(&m_Tmp);
}


CNetScheduleClient::EJobStatus 
CNetScheduleClient::GetStatus(const string& job_key,
                              int*          ret_code,
                              string*       output,
                              string*       err_msg)
{
    _ASSERT(ret_code);
    _ASSERT(output);

    if (m_RequestRateControl) {
        s_Throttler.Approve(CRequestRateControl::eSleep);
    }

    EJobStatus status;

    CheckConnect(job_key);
    CSockGuard sg(*m_Sock);
/*
    if (m_JobKey.id) {
        char command[2048];
        sprintf(command, "%s\r\n%s\r\nSTATUS %u",
                m_ClientName.c_str(), 
                m_Queue.c_str(),
                m_JobKey.id);
        WriteStr(command, strlen(command)+1);
        
        WaitForServer();
        if (!ReadStr(*m_Sock, &m_Tmp)) {
            NCBI_THROW(CNetServiceException, eCommunicationError, 
                    "Communication error");
        }
    } else {
        CommandInitiate("STATUS ", job_key, &m_Tmp);
    }
*/
    CommandInitiate("STATUS ", job_key, &m_Tmp);

    TrimPrefix(&m_Tmp);

    if (m_Tmp.empty()) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Invalid server response. Empty key.");
    }

    const char* str = m_Tmp.c_str();

    int st = atoi(str);
    status = (EJobStatus) st;

    if ((status == eDone || status == eFailed)) {
        for ( ;*str && isdigit(*str); ++str) {
        }

        for ( ; *str && isspace(*str); ++str) {
        }

        *ret_code = atoi(str);

        for ( ;*str && isdigit(*str); ++str) {
        }

        output->erase();

        for ( ; *str && isspace(*str); ++str) {
        }

        if (*str && *str == '"') {
            ++str;
            for( ;*str && *str != '"'; ++str) {
                output->push_back(*str);
            }
        }

        if (err_msg) {

            err_msg->erase();

            if (!*str)
                return status;

            for (++str; *str && isspace(*str); ++str) {
            }
            
            if (!*str)
                return status;

            if (*str && *str == '"') {
                ++str;
                for( ;*str && *str != '"'; ++str) {
                    err_msg->push_back(*str);
                }
            }

            *err_msg = NStr::ParseEscapes(*err_msg);
        }
    }

    return status;
}

bool CNetScheduleClient::GetJob(string*        job_key, 
                                string*        input,
                                unsigned short udp_port)
{
    _ASSERT(job_key);
    _ASSERT(input);

    if (m_RequestRateControl) {
        s_Throttler.Approve(CRequestRateControl::eSleep);
    }

    CheckConnect(kEmptyStr);
    CSockGuard sg(*m_Sock);

    if (udp_port == 0) {
        CommandInitiate("GET ", kEmptyStr, &m_Tmp);
    } else {
        MakeCommandPacket(&m_Tmp, "GET ");
        m_Tmp.append(NStr::IntToString(udp_port));

        WriteStr(m_Tmp.c_str(), m_Tmp.length() + 1);
        WaitForServer();

        if (!ReadStr(*m_Sock, &m_Tmp)) {
            NCBI_THROW(CNetServiceException, eCommunicationError, 
                    "Communication error");
        }
    }

    TrimPrefix(&m_Tmp);

    if (m_Tmp.empty()) {
        return false;
    }

    ParseGetJobResponse(job_key, input, m_Tmp);

    _ASSERT(!job_key->empty());
    _ASSERT(!input->empty());

    return true;
}


bool CNetScheduleClient::GetJobWaitNotify(string*    job_key, 
                                          string*    input, 
                                          unsigned   wait_time,
                                          unsigned short udp_port)
{
    _ASSERT(job_key);
    _ASSERT(input);

    if (m_RequestRateControl) {
        s_Throttler.Approve(CRequestRateControl::eSleep);
    }

    {{
    CheckConnect(kEmptyStr);
    CSockGuard sg(*m_Sock);

    MakeCommandPacket(&m_Tmp, "WGET ");
    m_Tmp.append(NStr::IntToString(udp_port));
    m_Tmp.append(" ");
    m_Tmp.append(NStr::IntToString(wait_time));

    WriteStr(m_Tmp.c_str(), m_Tmp.length() + 1);
    WaitForServer();

    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Communication error");
    }

    }}

    TrimPrefix(&m_Tmp);
    if (!m_Tmp.empty()) {
        ParseGetJobResponse(job_key, input, m_Tmp);

        _ASSERT(!job_key->empty());
        _ASSERT(!input->empty());

        return true;
    }
    return false;
}



bool CNetScheduleClient::WaitJob(string*    job_key, 
                                 string*    input, 
                                 unsigned   wait_time,
                                 unsigned short udp_port)
{
    bool job_received = 
        GetJobWaitNotify(job_key, input, wait_time, udp_port);
    if (job_received) {
        return job_received;
    }

    WaitQueueNotification(wait_time, udp_port);

    // no matter is WaitResult we re-try the request
    // using reliable comm.level and notify server that
    // we no longer on the UDP socket

    return GetJob(job_key, input, udp_port);
}


void CNetScheduleClient::WaitQueueNotification(unsigned       wait_time,
                                               unsigned short udp_port)
{
    _ASSERT(wait_time);

    EIO_Status status;
    int    sig_buf[4];
    const char* sig = "NCBI_JSQ_";
    memcpy(sig_buf, sig, 8);

    int    buf[1024/sizeof(int)];
    char*  chr_buf = (char*) buf;

    STimeout to;
    to.sec = wait_time;
    to.usec = 0;


    CDatagramSocket  udp_socket;
    udp_socket.SetReuseAddress(eOn);
    STimeout rto;
    rto.sec = rto.usec = 0;
    udp_socket.SetTimeout(eIO_Read, &rto);

    status = udp_socket.Bind(udp_port);
    if (eIO_Success != status) {
        return;
    }

    time_t curr_time, start_time, end_time;

    start_time = time(0);
    end_time = start_time + wait_time;

    // minilal length is prefix "NCBI_JSQ_" + queue length
    size_t min_msg_len = m_Queue.length() + 9;

    for (;true;) {

        curr_time = time(0);
        to.sec = end_time - curr_time;  // remaining
        if (to.sec <= 0) {
            break;
        }

        status = udp_socket.Wait(&to);
        if (eIO_Success != status) {
            continue;
        }

        size_t msg_len;
        status = udp_socket.Recv(buf, sizeof(buf), &msg_len);
        _ASSERT(status != eIO_Timeout); // because we Wait()-ed
        if (eIO_Success == status) {

            // Analyse the message content
            //
            // this is a performance critical code, if this is not
            // our message we need to get back to the datagram wait ASAP
            // (int arithmetic XOR-OR comparison here...)
            if ((msg_len < min_msg_len) ||
                ((buf[0] ^ sig_buf[0]) | (buf[1] ^ sig_buf[1]))
                ) {
                continue;
            }

            const char* queue = chr_buf + 9;

            if (strncmp(m_Queue.c_str(), queue, m_Queue.length()) == 0) {
                // Message from our queue 
                return;
            }

        } 

    } // for

}



void CNetScheduleClient::ParseGetJobResponse(string*        job_key, 
                                             string*        input, 
                                             const string&  response)
{
    _ASSERT(!response.empty());

    input->erase();
    job_key->erase();

    const char* str = response.c_str();
    while (*str && isspace(*str))
        ++str;

    for(;*str && !isspace(*str); ++str) {
        job_key->push_back(*str);
    }

    while (*str && isspace(*str))
        ++str;

    if (*str == '"')
        ++str;
    for (;*str && *str != '"'; ++str) {
        input->push_back(*str);
    }
}






void CNetScheduleClient::PutResult(const string& job_key, 
                                   int           ret_code, 
                                   const string& output)
{
    if (m_RequestRateControl) {
        s_Throttler.Approve(CRequestRateControl::eSleep);
    }

    CheckConnect(job_key);
    CSockGuard sg(*m_Sock);

    MakeCommandPacket(&m_Tmp, "PUT ");

    m_Tmp.append(job_key);
    m_Tmp.append(" ");
    m_Tmp.append(NStr::IntToString(ret_code));
    m_Tmp.append(" \"");
    m_Tmp.append(output);
    m_Tmp.append("\"");

    WriteStr(m_Tmp.c_str(), m_Tmp.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Communication error");
    }
    CheckOK(&m_Tmp);
}

void CNetScheduleClient::PutFailure(const string& job_key, 
                                    const string& err_msg)
{
    if (m_RequestRateControl) {
        s_Throttler.Approve(CRequestRateControl::eSleep);
    }

    if (err_msg.length() >= kNetScheduleMaxErrSize) {
        NCBI_THROW(CNetScheduleException, eDataTooLong, 
                   "Error message too long");
    }
    
    CheckConnect(job_key);
    CSockGuard sg(*m_Sock);

    MakeCommandPacket(&m_Tmp, "FPUT ");

    m_Tmp.append(job_key);
    m_Tmp.append(" ");
    m_Tmp.append(" \"");
    m_Tmp.append(NStr::PrintableString(err_msg));
    m_Tmp.append("\"");

    WriteStr(m_Tmp.c_str(), m_Tmp.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Communication error");
    }
    CheckOK(&m_Tmp);
}


void CNetScheduleClient::ReturnJob(const string& job_key)
{
    if (m_RequestRateControl) {
        s_Throttler.Approve(CRequestRateControl::eSleep);
    }

    CheckConnect(job_key);
    CSockGuard sg(*m_Sock);

    MakeCommandPacket(&m_Tmp, "RETURN ");
    m_Tmp.append(job_key);

    WriteStr(m_Tmp.c_str(), m_Tmp.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Communication error");
    }
    CheckOK(&m_Tmp);
}


void CNetScheduleClient::ShutdownServer()
{
    CheckConnect(kEmptyStr);
    CSockGuard sg(*m_Sock);

    MakeCommandPacket(&m_Tmp, "SHUTDOWN ");
    WriteStr(m_Tmp.c_str(), m_Tmp.length() + 1);
}

string CNetScheduleClient::ServerVersion()
{
    if (m_RequestRateControl) {
        s_Throttler.Approve(CRequestRateControl::eSleep);
    }

    CheckConnect(kEmptyStr);
    CSockGuard sg(*m_Sock);

    MakeCommandPacket(&m_Tmp, "VERSION ");
    WriteStr(m_Tmp.c_str(), m_Tmp.length() + 1);
    WaitForServer();

    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Communication error");
    }
    TrimPrefix(&m_Tmp);

    if (m_Tmp.empty()) {
        NCBI_THROW(CNetServiceException, 
                   eCommunicationError, 
                   "Invalid server response. Empty version.");
    }
    return m_Tmp;
}

void CNetScheduleClient::PrintStatistics(CNcbiOstream & out)
{
    if (m_RequestRateControl) {
        s_Throttler.Approve(CRequestRateControl::eSleep);
    }

    CheckConnect(kEmptyStr);
    CSockGuard sg(*m_Sock);

    MakeCommandPacket(&m_Tmp, "STAT ");
    WriteStr(m_Tmp.c_str(), m_Tmp.length() + 1);

    WaitForServer();
    while (1) {
        if (!ReadStr(*m_Sock, &m_Tmp)) {
            break;
        }
        TrimPrefix(&m_Tmp);
        if (m_Tmp == "END")
            break;
        out << m_Tmp << "\n";
    }
}


void CNetScheduleClient::DropQueue()
{
    CheckConnect(kEmptyStr);
    CSockGuard sg(*m_Sock);

    MakeCommandPacket(&m_Tmp, "DROPQ ");
    WriteStr(m_Tmp.c_str(), m_Tmp.length() + 1);

    WaitForServer();

    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Communication error");
    }
    TrimPrefix(&m_Tmp);
}

void CNetScheduleClient::CommandInitiate(const string& command, 
                                         const string& job_key,
                                         string*       answer)
{
    _ASSERT(answer);
    MakeCommandPacket(&m_Tmp, command);
    m_Tmp.append(job_key);
    WriteStr(m_Tmp.c_str(), m_Tmp.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, answer)) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Communication error");
    }
}


void CNetScheduleClient::TrimPrefix(string* str)
{
    CheckOK(str);
    str->erase(0, 3); // "OK:"
}

void CNetScheduleClient::CheckOK(string* str)
{
    if (str->find("OK:") != 0) {
        // Answer is not in "OK:....." format
        string msg = "Server error:";
        TrimErr(str);
        msg += *str;
        NCBI_THROW(CNetServiceException, eCommunicationError, msg);
    }
}

void CNetScheduleClient::MakeCommandPacket(string* out_str, 
                                           const string& cmd_str)
{
    unsigned client_len = m_ClientName.length();
    if (client_len < 3) {
        NCBI_THROW(CNetScheduleException, 
                   eAuthenticationError, "Client name too small or empty");
    }
    if (m_Queue.empty()) {
        NCBI_THROW(CNetScheduleException, 
                   eAuthenticationError, "Empty queue name");
    }

    *out_str = m_ClientName;
/*
    const string& client_name_comment = GetClientNameComment();
    if (!client_name_comment.empty()) {
        out_str->append(" ");
        out_str->append(client_name_comment);
    }
*/
    out_str->append("\r\n");
    out_str->append(m_Queue);
    out_str->append("\r\n");
    out_str->append(cmd_str);
}


void CNetScheduleClient::CheckConnect(const string& key)
{
    m_JobKey.id = 0;
    if (m_Sock && (eIO_Success == m_Sock->GetStatus(eIO_Open))) {
        return; // we are connected, nothing to do
    }

    if (!key.empty()) {
        CNetSchedule_ParseJobKey(&m_JobKey, key);
        CreateSocket(m_JobKey.hostname, m_JobKey.port);
        return;        
    }

    // not connected

    if (!m_Host.empty()) { // we can restore connection
        CreateSocket(m_Host, m_Port);
        return;
    }

    NCBI_THROW(CNetServiceException, eCommunicationError,
        "Cannot establish connection with a server. Unknown host-port.");

}

bool CNetScheduleClient::IsError(const char* str)
{
    int cmp = NStr::strncasecmp(str, "ERR:", 4);
    return cmp == 0;
}

///////////////////////////////////////////////////////////////////////////////

const char* kNetScheduleDriverName = "netscheduleclient";

/// @internal
class CNetScheduleClientCF : public IClassFactory<CNetScheduleClient>
{
public:

    typedef CNetScheduleClient                 TDriver;
    typedef CNetScheduleClient                 IFace;
    typedef IFace                              TInterface;
    typedef IClassFactory<CNetScheduleClient>  TParent;
    typedef TParent::SDriverInfo               TDriverInfo;
    typedef TParent::TDriverList               TDriverList;

    /// Construction
    ///
    /// @param driver_name
    ///   Driver name string
    /// @param patch_level
    ///   Patch level implemented by the driver.
    ///   By default corresponds to interface patch level.
    CNetScheduleClientCF(const string& driver_name = kNetScheduleDriverName,
                         int patch_level = -1)
        : m_DriverVersionInfo
        (ncbi::CInterfaceVersion<IFace>::eMajor,
         ncbi::CInterfaceVersion<IFace>::eMinor,
         patch_level >= 0 ?
            patch_level : ncbi::CInterfaceVersion<IFace>::ePatchLevel),
          m_DriverName(driver_name)
    {
        _ASSERT(!m_DriverName.empty());
    }

    /// Create instance of TDriver
    virtual TInterface*
    CreateInstance(const string& driver  = kEmptyStr,
                   CVersionInfo version = NCBI_INTERFACE_VERSION(IFace),
                   const TPluginManagerParamTree* params = 0) const
    {
        TDriver* drv = 0;
        if (params && (driver.empty() || driver == m_DriverName)) {
            if (version.Match(NCBI_INTERFACE_VERSION(IFace))
                                != CVersionInfo::eNonCompatible) {

            CConfig conf(params);
            const string& client_name = 
                conf.GetString(m_DriverName, 
                               "client_name", CConfig::eErr_Throw, "noname");
            const string& queue_name = 
                conf.GetString(m_DriverName, 
                               "queue_name", CConfig::eErr_Throw, "noname");
            const string& service = 
                conf.GetString(m_DriverName, 
                               "service", CConfig::eErr_NoThrow, "");
            if (!service.empty()) {
                unsigned int rebalance_time = conf.GetInt(m_DriverName, 
                                                "rebalance_time",
                                                CConfig::eErr_NoThrow, 10);
                unsigned int rebalance_requests = conf.GetInt(m_DriverName,
                                                "rebalance_requests",
                                                CConfig::eErr_NoThrow, 100);
                drv = new CNetScheduleClient_LB(client_name, 
                                                service, queue_name,
                                                rebalance_time, 
                                                rebalance_requests);
            } else { // non lb client
                const string& host = 
                    conf.GetString(m_DriverName, 
                                  "host", CConfig::eErr_Throw, "");
                unsigned int port = conf.GetInt(m_DriverName,
                                               "port",
                                               CConfig::eErr_Throw, 9001);

                drv = new CNetScheduleClient(host, port, 
                                             client_name, queue_name);
            }
            }                               
        }
        return drv;
    }

    void GetDriverVersions(TDriverList& info_list) const
    {
        info_list.push_back(TDriverInfo(m_DriverName, m_DriverVersionInfo));
    }
protected:
    CVersionInfo  m_DriverVersionInfo;
    string        m_DriverName;

};

void NCBI_XCONNECT_EXPORT NCBI_EntryPoint_xnetschedule(
     CPluginManager<CNetScheduleClient>::TDriverInfoList&   info_list,
     CPluginManager<CNetScheduleClient>::EEntryPointRequest method)
{
       CHostEntryPointImpl<CNetScheduleClientCF>::
       NCBI_EntryPointImpl(info_list, method);

}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.15  2005/03/21 16:46:35  didenko
 * + creating from PluginManager
 *
 * Revision 1.14  2005/03/21 13:05:58  kuznets
 * + PrintStatistics
 *
 * Revision 1.13  2005/03/17 20:36:22  kuznets
 * +PutFailure()
 *
 * Revision 1.12  2005/03/17 17:18:25  kuznets
 * Cosmetics
 *
 * Revision 1.11  2005/03/16 14:25:46  kuznets
 * Fixed connection establishment when job key is known
 *
 * Revision 1.10  2005/03/15 20:13:07  kuznets
 * +SubmitJobAndWait()
 *
 * Revision 1.9  2005/03/15 14:48:56  kuznets
 * +DropJob()
 *
 * Revision 1.8  2005/03/10 14:17:58  kuznets
 * +SetRunTimeout()
 *
 * Revision 1.7  2005/03/07 17:30:49  kuznets
 * Job key parsing changed
 *
 * Revision 1.6  2005/03/04 12:05:46  kuznets
 * Implemented WaitJob() method
 *
 * Revision 1.5  2005/02/28 18:39:43  kuznets
 * +ReturnJob()
 *
 * Revision 1.4  2005/02/28 12:19:40  kuznets
 * +DropQueue()
 *
 * Revision 1.3  2005/02/10 20:01:19  kuznets
 * +GetJob(), +PutResult()
 *
 * Revision 1.2  2005/02/09 18:59:08  kuznets
 * Implemented job submission part of the API
 *
 * Revision 1.1  2005/02/07 13:02:47  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
