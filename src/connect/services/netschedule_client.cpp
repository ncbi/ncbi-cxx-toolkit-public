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
#include <corelib/ncbitime.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <connect/ncbi_conn_exception.hpp>
#include <connect/services/netschedule_client.hpp>
#include <corelib/request_control.hpp>
#include <memory>
#include <stdio.h>


BEGIN_NCBI_SCOPE


const char* kNetSchedule_KeyPrefix = "JSID";

/// Request rate controller (one for all client instances)
/// Default limitation is 20000 requests per minute.
///
/// Use wrapper class because CSafeStaticPtr do not accept parameters
/// for quarded object.
///
/// @internal
///
class CNetScheduleThrottler {
public:
    CNetScheduleThrottler(void)
        : m_Throttler(20000, CTimeSpan(60,0)) { }
    bool Approve(CRequestRateControl::EThrottleAction action
                 = CRequestRateControl::eDefault) {
        return m_Throttler.Approve(action);
    }
    CRequestRateControl m_Throttler;
};
static CSafeStaticPtr<CNetScheduleThrottler> s_Throttler;


unsigned CNetSchedule_GetJobId(const string&  key_str)
{
    unsigned job_id;
    const char* ch = key_str.c_str();

    if (isdigit((unsigned char)(*ch))) {
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

/* moved to netschedule_api.cpp
CNetScheduleExceptionMap::CNetScheduleExceptionMap()
{
    m_Map["eInternalError"] = CNetScheduleException::eInternalError;
    m_Map["eProtocolSyntaxError"] = CNetScheduleException::eProtocolSyntaxError;
    m_Map["eAuthenticationError"] = CNetScheduleException::eAuthenticationError;
    m_Map["eKeyFormatError"] = CNetScheduleException::eKeyFormatError;
    m_Map["eInvalidJobStatus"] = CNetScheduleException::eInvalidJobStatus;
    m_Map["eUnknownQueue"] = CNetScheduleException::eUnknownQueue;
    m_Map["eUnknownQueueClass"] = CNetScheduleException::eUnknownQueueClass;
    m_Map["eTooManyPendingJobs"] = CNetScheduleException::eTooManyPendingJobs;
    m_Map["eDataTooLong"] = CNetScheduleException::eDataTooLong;
    m_Map["eInvalidClientOrVersion"] = CNetScheduleException::eInvalidClientOrVersion;
    m_Map["eOperationAccessDenied"] = CNetScheduleException::eOperationAccessDenied;
    m_Map["eDuplicateName"] = CNetScheduleException::eDuplicateName;
}

CException::TErrCode CNetScheduleExceptionMap::GetCode(const string& name)
{
    TMap::iterator it = m_Map.find(name);
    if (it == m_Map.end())
        return CException::eInvalid;
    return it->second;
}
*/

CNetScheduleExceptionMap CNetScheduleClient::sm_ExceptionMap;

CNetScheduleClient::CNetScheduleClient(const string& client_name,
                                       const string& queue_name)
    : CNetServiceClient(client_name),
      m_AuthenticationSent(false),
      m_Queue(queue_name),
      m_RequestRateControl(true),
      m_ConnMode(eCloseConnection)
{
}


CNetScheduleClient::CNetScheduleClient(const string&  host,
                                       unsigned short port,
                                       const string&  client_name,
                                       const string&  queue_name)
    : CNetServiceClient(host, port, client_name),
      m_AuthenticationSent(false),
      m_Queue(queue_name),
      m_RequestRateControl(true),
      m_ConnMode(eCloseConnection)
{
}


CNetScheduleClient::CNetScheduleClient(CSocket*      sock,
                                       const string& client_name,
                                       const string& queue_name)
    : CNetServiceClient(sock, client_name),
      m_AuthenticationSent(false),
      m_Queue(queue_name),
      m_RequestRateControl(true),
      m_ConnMode(eCloseConnection)
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

CNetScheduleClient::EJobStatus
CNetScheduleClient::StringToStatus(const string& status_str)
{
    if (NStr::CompareNocase(status_str, "Pending") == 0) {
        return ePending;
    }
    if (NStr::CompareNocase(status_str, "Running") == 0) {
        return eRunning;
    }
    if (NStr::CompareNocase(status_str, "Returned") == 0) {
        return eReturned;
    }
    if (NStr::CompareNocase(status_str, "Canceled") == 0) {
        return eCanceled;
    }
    if (NStr::CompareNocase(status_str, "Failed") == 0) {
        return eFailed;
    }
    if (NStr::CompareNocase(status_str, "Done") == 0) {
        return eDone;
    }

    // check acceptable synonyms

    if (NStr::CompareNocase(status_str, "Pend") == 0) {
        return ePending;
    }
    if (NStr::CompareNocase(status_str, "Run") == 0) {
        return eRunning;
    }
    if (NStr::CompareNocase(status_str, "Return") == 0) {
        return eReturned;
    }
    if (NStr::CompareNocase(status_str, "Cancel") == 0) {
        return eCanceled;
    }
    if (NStr::CompareNocase(status_str, "Fail") == 0) {
        return eFailed;
    }

    return eJobNotFound;
}


string CNetScheduleClient::GetConnectionInfo() const
{
    return m_Host + ':' + NStr::UIntToString(m_Port);
}


void CNetScheduleClient::ActivateRequestRateControl(bool on_off)
{
    m_RequestRateControl = on_off;
}


string CNetScheduleClient::SubmitJob(const string& input,
                                     const string& progress_msg,
                                     const string& affinity_token,
                                     const string& out,
                                     const string& err,
                                     unsigned      job_mask)
{
    if (input.length() > kNetScheduleMaxDataSize) {
        NCBI_THROW(CNetScheduleException, eDataTooLong,
            "Input data too long.");
    }
    if (m_RequestRateControl) {
        s_Throttler->Approve(CRequestRateControl::eSleep);
    }

    CheckConnect(kEmptyStr);
    CSockGuard sg(GetConnMode() == eKeepConnection ? 0 : m_Sock);


    //cerr << "Input: " << input << endl;
    MakeCommandPacket(&m_Tmp, "SUBMIT \"");
    m_Tmp.append(NStr::PrintableString(input));
    m_Tmp.append("\"");

    if (!progress_msg.empty()) {
        m_Tmp.append(" \"");
        m_Tmp.append(progress_msg);
        m_Tmp.append("\"");
    }

    if (!affinity_token.empty()) {
        m_Tmp.append(" aff=\"");
        m_Tmp.append(affinity_token);
        m_Tmp.append("\"");
    }

    if (!out.empty()) {
        m_Tmp.append(" out=\"");
        m_Tmp.append(out);
        m_Tmp.append("\"");
    }

    if (!err.empty()) {
        m_Tmp.append(" err=\"");
        m_Tmp.append(err);
        m_Tmp.append("\"");
    }

    if (job_mask != eEmptyMask) {
        m_Tmp.append(" msk=");
        m_Tmp.append(NStr::UIntToString(job_mask));
    } else {
        m_Tmp.append(" msk=0");
    }


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


void CNetScheduleClient::SubmitJobBatch(SJobBatch& subm)
{
    subm.host.erase();
    subm.port = 0;

    // veryfy the input data
    ITERATE(TBatchSubmitJobList, it, subm.job_list) {
        const string& input = it->input;

        if (input.length() > kNetScheduleMaxDataSize) {
            NCBI_THROW(CNetScheduleException, eDataTooLong,
                "Input data too long.");
        }
    }
    if (m_RequestRateControl) {
        s_Throttler->Approve(CRequestRateControl::eSleep);
    }

    CheckConnect(kEmptyStr);
    CSockGuard sg(GetConnMode() == eKeepConnection ? 0 : m_Sock);

    m_Sock->DisableOSSendDelay(true);

    // batch command
    MakeCommandPacket(&m_Tmp, "BSUB");
    WriteStr(m_Tmp.c_str(), m_Tmp.length() + 1);

    // check if server is ready for the batch submit
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Communication error");
    }
    TrimPrefix(&m_Tmp);

    m_Sock->DisableOSSendDelay(false);

    for (unsigned i = 0; i < subm.job_list.size(); ) {

        // Batch size should be reasonable not to trigger network timeout
        const unsigned kMax_Batch = 10000;

        unsigned batch_size = subm.job_list.size() - i;
        if (batch_size > kMax_Batch) {
            batch_size = kMax_Batch;
        }

        char buf[kNetScheduleMaxDataSize * 6];
        sprintf(buf, "BTCH %u", batch_size);

        WriteStr(buf, strlen(buf)+1);

        unsigned batch_start = i;
        string aff_prev;
        for (unsigned j = 0; j < batch_size; ++j,++i) {
            string input = NStr::PrintableString(subm.job_list[i].input);
            const string& aff = subm.job_list[i].affinity_token;
            if (aff[0]) {
                if (aff == aff_prev) { // exactly same affinity(sorted jobs)
                    sprintf(buf, "\"%s\" affp",
                            input.c_str()
                            );
                } else {
                    sprintf(buf, "\"%s\" aff=\"%s\"",
                            input.c_str(),
                            aff.c_str()
                            );
                    aff_prev = aff;
                }
            } else {
                aff_prev.erase();
                sprintf(buf, "\"%s\"", input.c_str());
            }
            WriteStr(buf, strlen(buf)+1);
        }

        WriteStr("ENDB", 5);

        WaitForServer();
        if (!ReadStr(*m_Sock, &m_Tmp)) {
            NCBI_THROW(CNetServiceException, eCommunicationError,
                    "Communication error");
        }
        TrimPrefix(&m_Tmp);

        if (m_Tmp.empty()) {
            NCBI_THROW(CNetServiceException, eProtocolError,
                    "Invalid server response. Empty key.");
        }

        // parse the batch answer
        // FORMAT:
        //  first_job_id host port

        {{
        const char* s = m_Tmp.c_str();
        unsigned first_job_id = ::atoi(s);

        if (subm.host.empty()) {
            for (; *s != ' '; ++s) {
                if (*s == 0) {
                    NCBI_THROW(CNetServiceException, eProtocolError,
                            "Invalid server response. Batch answer format.");
                }
            }
            ++s;
            if (*s == 0) {
                NCBI_THROW(CNetServiceException, eProtocolError,
                        "Invalid server response. Batch answer format.");
            }
            for (; *s != ' '; ++s) {
                if (*s == 0) {
                    NCBI_THROW(CNetServiceException, eProtocolError,
                            "Invalid server response. Batch answer format.");
                }
                subm.host.push_back(*s);
            }
            ++s;
            if (*s == 0) {
                NCBI_THROW(CNetServiceException, eProtocolError,
                        "Invalid server response. Batch answer format.");
            }

            subm.port = atoi(s);
            if (subm.port == 0) {
                NCBI_THROW(CNetServiceException, eProtocolError,
                        "Invalid server response. Port=0.");
            }
        }

        // assign job ids, protocol guarantees all jobs in batch will
        // receive sequential numbers, so server sends only first job id
        //
        for (unsigned j = 0; j < batch_size; ++j) {
            subm.job_list[batch_start].job_id = first_job_id;
            ++first_job_id;
            ++batch_start;
        }


        }}


    } // for

    m_Sock->DisableOSSendDelay(true);

    WriteStr("ENDS", 5);
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
        s_Throttler->Approve(CRequestRateControl::eSleep);
    }
    *output = kEmptyStr;

    {{
        CheckConnect(kEmptyStr);
        CSockGuard sg(GetConnMode() == eKeepConnection ? 0 : m_Sock);

        MakeCommandPacket(&m_Tmp, "SUBMIT \"");
        m_Tmp.append(NStr::PrintableString(input));
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

    for (;;) {
        curr_time = time(0);
        to.sec = (unsigned int) (end_time - curr_time);  // remaining
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
        s_Throttler->Approve(CRequestRateControl::eSleep);
    }

    CheckConnect(job_key);
    CSockGuard sg(GetConnMode() == eKeepConnection ? 0 : m_Sock);

    CommandInitiate("CANCEL ", job_key, &m_Tmp);

    CheckServerOK(&m_Tmp);
}


void CNetScheduleClient::DropJob(const string& job_key)
{
    if (m_RequestRateControl) {
        s_Throttler->Approve(CRequestRateControl::eSleep);
    }

    CheckConnect(job_key);
    CSockGuard sg(GetConnMode() == eKeepConnection ? 0 : m_Sock);

    CommandInitiate("DROJ ", job_key, &m_Tmp);

    CheckServerOK(&m_Tmp);
}


void CNetScheduleClient::Logging(bool on_off)
{
    CheckConnect(kEmptyStr);
    CSockGuard sg(GetConnMode() == eKeepConnection ? 0 : m_Sock);

    CommandInitiate("LOG ", on_off ? "ON" : "OFF", &m_Tmp);
    CheckServerOK(&m_Tmp);
}


void CNetScheduleClient::SetRunTimeout(const string& job_key,
                                       unsigned      time_to_run)
{
    if (m_RequestRateControl) {
        s_Throttler->Approve(CRequestRateControl::eSleep);
    }
    CheckConnect(job_key);
    CSockGuard sg(GetConnMode() == eKeepConnection ? 0 : m_Sock);

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

    CheckServerOK(&m_Tmp);
}


void CNetScheduleClient::JobDelayExpiration(const string& job_key,
                                            unsigned      runtime_inc)
{
    if (m_RequestRateControl) {
        s_Throttler->Approve(CRequestRateControl::eSleep);
    }
    CheckConnect(job_key);
    CSockGuard sg(GetConnMode() == eKeepConnection ? 0 : m_Sock);

    MakeCommandPacket(&m_Tmp, "JDEX ");

    m_Tmp.append(job_key);
    m_Tmp.append(" ");
    m_Tmp.append(NStr::IntToString(runtime_inc));

    WriteStr(m_Tmp.c_str(), m_Tmp.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Communication error");
    }

    CheckServerOK(&m_Tmp);
}


CNetScheduleClient::EJobStatus
CNetScheduleClient::GetStatus(const string& job_key,
                              int*          ret_code,
                              string*       output,
                              string*       err_msg,
                              string*       input)
{
    _ASSERT(ret_code);
    _ASSERT(output);

    if (m_RequestRateControl) {
        s_Throttler->Approve(CRequestRateControl::eSleep);
    }

    EJobStatus status;

    CheckConnect(job_key);
    CSockGuard sg(GetConnMode() == eKeepConnection ? 0 : m_Sock);
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

    if (status == eDone || status == eFailed
        || status == eRunning || status == ePending
        || status == eCanceled || status == eReturned
        || status == eFailed) {
        //cerr << str <<endl;
        for ( ;*str && isdigit((unsigned char)(*str)); ++str) {
        }

        for ( ; *str && isspace((unsigned char)(*str)); ++str) {
        }

        *ret_code = atoi(str);

        for ( ;*str && isdigit((unsigned char)(*str)); ++str) {
        }

        output->erase();

        for ( ; *str && isspace((unsigned char)(*str)); ++str) {
        }

        if (*str && *str == '"') {
            ++str;
            for( ;*str && *str; ++str) {
                if (*str == '"' && *(str-1) != '\\') break;
                output->push_back(*str);
            }
            /*
            for( ;*str && *str != '"'; ++str) {
                output->push_back(*str);
            }
            */
        }
        *output = NStr::ParseEscapes(*output);
        if (err_msg || input) {
            if (err_msg) err_msg->erase();
            if (!*str)
                return status;

            for (++str; *str && isspace((unsigned char)(*str)); ++str) {
            }

            if (!*str)
                return status;

            if (*str && *str == '"') {
                ++str;
                for( ;*str && *str; ++str) {
                    if (*str == '"' && *(str-1) != '\\') break;
                    if (err_msg)
                        err_msg->push_back(*str);
                }
            }

            if (err_msg) *err_msg = NStr::ParseEscapes(*err_msg);
        }

        if (input) {
            input->erase();
            if (!*str)
                return status;

            for (++str; *str && isspace((unsigned char)(*str)); ++str) {
            }

            if (!*str)
                return status;

            if (*str && *str == '"') {
                ++str;
                for( ;*str && *str; ++str) {
                    if (*str == '"' && *(str-1) != '\\') break;
                    input->push_back(*str);
                }
            }
            *input = NStr::ParseEscapes(*input);
        }

    } // if status done or failed

    return status;
}


bool CNetScheduleClient::GetJob(string*        job_key,
                                string*        input,
                                unsigned short udp_port,
                                string*        jout,
                                string*        jerr,
                                TJobMask*      job_mask)
{
    _ASSERT(job_key);
    _ASSERT(input);

    //cerr << ">>GetJob" << endl;
    if (m_RequestRateControl) {
        s_Throttler->Approve(CRequestRateControl::eSleep);
    }

    CheckConnect(kEmptyStr);
    CSockGuard sg(GetConnMode() == eKeepConnection ? 0 : m_Sock);

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
        //cerr << "<<GetJob--" << endl;
        return false;
    }

    TJobMask j_mask = eEmptyMask;
    if (jout != 0 && jerr != 0) {
        ParseGetJobResponse(job_key, input, jout, jerr, &j_mask, m_Tmp);
    } else {
        string tmp;
        ParseGetJobResponse(job_key, input,
                            jout ? jout : &tmp, jerr ? jerr : &tmp, &j_mask,
                            m_Tmp);
    }
    if (job_mask) {
        *job_mask = j_mask;
    }

    _ASSERT(!job_key->empty());
    //    _ASSERT(!input->empty());

    //cerr << "<<GetJob++" << endl;
    return true;
}


bool CNetScheduleClient::GetJobWaitNotify(string*    job_key,
                                          string*    input,
                                          unsigned   wait_time,
                                          unsigned short udp_port,
                                          string*    jout,
                                          string*    jerr,
                                          TJobMask*  job_mask)
{
    _ASSERT(job_key);
    _ASSERT(input);
    //cerr << ">>GetJobWaitNotify" << endl;

    if (m_RequestRateControl) {
        s_Throttler->Approve(CRequestRateControl::eSleep);
    }

    {{
    CheckConnect(kEmptyStr);
    CSockGuard sg(GetConnMode() == eKeepConnection ? 0 : m_Sock);

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
    TJobMask j_mask = eEmptyMask;
    if (!m_Tmp.empty()) {
        if (jout != 0 && jerr != 0) {
            ParseGetJobResponse(job_key, input, jout, jerr, &j_mask, m_Tmp);
        } else {
            string tmp;
            ParseGetJobResponse(job_key, input,
                                jout ? jout : &tmp, jerr ? jerr : &tmp, &j_mask,
                                m_Tmp);
        }
        if (job_mask) {
            *job_mask = j_mask;
        }

        _ASSERT(!job_key->empty());
        //_ASSERT(!input->empty());

        //cerr << ">>GetJobWaitNotify++" << endl;
        return true;
    }
    //cerr << ">>GetJobWaitNotify--" << endl;

    return false;
}


bool CNetScheduleClient::WaitJob(string*    job_key,
                                 string*    input,
                                 unsigned   wait_time,
                                 unsigned short udp_port,
                                 EWaitMode      wait_mode,
                                 string*        jout,
                                 string*        jerr,
                                 TJobMask*      job_mask)
{
    //cerr << ">>WaitJob" << endl;
    bool job_received =
        GetJobWaitNotify(job_key, input, wait_time, udp_port,
                         jout, jerr, job_mask);
    if (job_received) {
        return job_received;
    }

    if (wait_mode != eWaitNotification) {
        return 0;
    }
    WaitQueueNotification(wait_time, udp_port);

    // no matter is WaitResult we re-try the request
    // using reliable comm.level and notify server that
    // we no longer on the UDP socket

    bool ret = GetJob(job_key, input, udp_port, jout, jerr, job_mask);
    /*
    cerr << ">>WaitJob";
    if (ret)
        cerr << "++" << endl;
    else
        cerr << "--" << endl;
    */
    return ret;

}

bool
CNetScheduleClient::WaitNotification(const string&  queue_name,
                                     unsigned       wait_time,
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
        return false;
    }

    time_t curr_time, start_time, end_time;

    start_time = time(0);
    end_time = start_time + wait_time;

    // minilal length is prefix "NCBI_JSQ_" + queue length
    size_t min_msg_len = queue_name.length() + 9;
    //cerr << "WaitNotification : before for(;;)" <<  endl;

    for (;;) {
        curr_time = time(0);
        if (curr_time >= end_time) {
            //cerr << "WaitNotification :  for(;;) break" <<  endl;
            break;
        }
        to.sec = (unsigned int) (end_time - curr_time);  // remaining

        //cerr << "WaitNotification : " << start_time << " " << curr_time << " " << end_time << endl;
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
            //cerr << "WaitNotification : " << chr_buf << endl;
            if (strncmp(queue_name.c_str(), queue, queue_name.length()) == 0) {
                // Message from our queue
                return true;
            }
        }
    } // for

    return false;

}


void CNetScheduleClient::WaitQueueNotification(unsigned       wait_time,
                                               unsigned short udp_port)
{
    CNetScheduleClient::WaitNotification(GetQueueName(), wait_time, udp_port);
}


void CNetScheduleClient::ParseGetJobResponse(string*        job_key,
                                             string*        input,
                                             string*        jout,
                                             string*        jerr,
                                             TJobMask*      job_mask,
                                             const string&  response)
{
    // Server message format:
    //    JOB_KEY "input" ["out" ["err"]] [mask]

    _ASSERT(!response.empty());
    _ASSERT(input);
    _ASSERT(jout);
    _ASSERT(jerr);
    _ASSERT(job_mask);

    input->erase();
    job_key->erase();
    jout->erase();
    jerr->erase();

    const char* str = response.c_str();
    while (*str && isspace((unsigned char)(*str)))
        ++str;
    if (*str == 0) {
    throw_err:
        NCBI_THROW(CNetScheduleException, eProtocolSyntaxError,
                   "Internal error. Cannot parse server output.");
    }

    for(;*str && !isspace((unsigned char)(*str)); ++str) {
        job_key->push_back(*str);
    }
    if (*str == 0) {
        goto throw_err;
    }

    while (*str && isspace((unsigned char)(*str)))
        ++str;

    if (*str && *str == '"') {
        ++str;
        for( ;*str && *str; ++str) {
            if (*str == '"' && *(str-1) != '\\') { ++str; break; }
            input->push_back(*str);
        }
    } else {
        goto throw_err;
    }

    *input = NStr::ParseEscapes(*input);

    // parse "out"
    while (*str && isspace((unsigned char)(*str)))
        ++str;
    if (*str == 0)
        return;

    if (*str == '"') {
        ++str;
        for( ;*str && *str; ++str) {
            if (*str == '"' && *(str-1) != '\\') { ++str; break; }
            jout->push_back(*str);
        }
    } else {
        goto throw_err;
    }
    *jout = NStr::ParseEscapes(*jout);

    while (*str && isspace((unsigned char)(*str)))
        ++str;

    if (*str == 0) {
        return;
    }
    if (*str == '"') {
        ++str;
        for( ;*str && *str; ++str) {
            if (*str == '"' && *(str-1) != '\\') { ++str; break; }
            jerr->push_back(*str);
        }
    } else {
        goto throw_err;
    }
    *jerr = NStr::ParseEscapes(*jerr);

    // parse mask
    while (*str && isspace((unsigned char)(*str)))
        ++str;
    if (*str == 0)
        return;

    *job_mask = atoi(str);
}


void CNetScheduleClient::PutResult(const string& job_key,
                                   int           ret_code,
                                   const string& output)
{
    if (output.length() > kNetScheduleMaxDataSize) {
        NCBI_THROW(CNetScheduleException, eDataTooLong,
            "Output data too long.");
    }

    if (m_RequestRateControl) {
        s_Throttler->Approve(CRequestRateControl::eSleep);
    }

    CheckConnect(job_key);
    CSockGuard sg(GetConnMode() == eKeepConnection ? 0 : m_Sock);

    MakeCommandPacket(&m_Tmp, "PUT ");

    m_Tmp.append(job_key);
    m_Tmp.append(" ");
    m_Tmp.append(NStr::IntToString(ret_code));
    m_Tmp.append(" \"");
    m_Tmp.append(NStr::PrintableString(output));
    m_Tmp.append("\"");

    //cerr << output << endl << "==================" << endl;
    //cerr << output.size() << endl;
    //cerr << m_Tmp << endl;

    WriteStr(m_Tmp.c_str(), m_Tmp.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Communication error");
    }
    CheckServerOK(&m_Tmp);
}


bool CNetScheduleClient::PutResultGetJob(const string& done_job_key,
                                         int           done_ret_code,
                                         const string& done_output,
                                         string*       new_job_key,
                                         string*       new_input,
                                         string*       jout,
                                         string*       jerr,
                                         TJobMask*     job_mask)
{
    if (done_job_key.empty()) {
        return GetJob(new_job_key, new_input,
                      0, jout, jerr,
                      job_mask);
    }

    _ASSERT(new_job_key);
    _ASSERT(new_input);

    if (done_output.length() > kNetScheduleMaxDataSize) {
        NCBI_THROW(CNetScheduleException, eDataTooLong,
            "Output data too long.");
    }

    if (m_RequestRateControl) {
        s_Throttler->Approve(CRequestRateControl::eSleep);
    }
    CheckConnect(done_job_key);
    CSockGuard sg(GetConnMode() == eKeepConnection ? 0 : m_Sock);

    //cerr << done_output << endl << "==================" << endl;
    //cerr << done_output.size() << endl;

    MakeCommandPacket(&m_Tmp, "JXCG ");

    m_Tmp.append(done_job_key);
    m_Tmp.append(" ");
    m_Tmp.append(NStr::IntToString(done_ret_code));
    m_Tmp.append(" \"");
    m_Tmp.append(NStr::PrintableString(done_output));
    m_Tmp.append("\"");

    WriteStr(m_Tmp.c_str(), m_Tmp.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Communication error");
    }

    TrimPrefix(&m_Tmp);

    if (m_Tmp.empty()) {
        return false;
    }

    TJobMask j_mask = eEmptyMask;
    if (jout != 0 && jerr != 0) {
        ParseGetJobResponse(new_job_key, new_input, jout, jerr, &j_mask,
                            m_Tmp);
    } else {
        string tmp;
        ParseGetJobResponse(new_job_key, new_input,
                            jout ? jout : &tmp, jerr ? jerr : &tmp, &j_mask,
                            m_Tmp);
    }
    if (job_mask) {
        *job_mask = j_mask;
    }

    _ASSERT(!new_job_key->empty());

    return true;
}


void CNetScheduleClient::PutProgressMsg(const string& job_key,
                                        const string& progress_msg)
{
    if (m_RequestRateControl) {
        s_Throttler->Approve(CRequestRateControl::eSleep);
    }
    if (progress_msg.length() >= kNetScheduleMaxDataSize) {
        NCBI_THROW(CNetScheduleException, eDataTooLong,
                   "Progress message too long");
    }

    CheckConnect(job_key);
    CSockGuard sg(GetConnMode() == eKeepConnection ? 0 : m_Sock);

    MakeCommandPacket(&m_Tmp, "MPUT ");

    m_Tmp.append(job_key);
    m_Tmp.append(" \"");
    m_Tmp.append(NStr::PrintableString(progress_msg));
    m_Tmp.append("\"");

    WriteStr(m_Tmp.c_str(), m_Tmp.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Communication error");
    }
    CheckServerOK(&m_Tmp);
}


string CNetScheduleClient::GetProgressMsg(const string& job_key)
{
    if (m_RequestRateControl) {
        s_Throttler->Approve(CRequestRateControl::eSleep);
    }

    CheckConnect(job_key);
    CSockGuard sg(GetConnMode() == eKeepConnection ? 0 : m_Sock);

    MakeCommandPacket(&m_Tmp, "MGET ");

    m_Tmp.append(job_key);

    WriteStr(m_Tmp.c_str(), m_Tmp.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Communication error");
    }
    TrimPrefix(&m_Tmp);

    return NStr::ParseEscapes(m_Tmp);
}


void CNetScheduleClient::PutFailure(const string& job_key,
                                    const string& err_msg,
                                    const string& output,
                                    int           ret_code)
{
    if (m_RequestRateControl) {
        s_Throttler->Approve(CRequestRateControl::eSleep);
    }

    if (output.length() > kNetScheduleMaxDataSize) {
        NCBI_THROW(CNetScheduleException, eDataTooLong,
            "Output data too long.");
    }

    if (err_msg.length() >= kNetScheduleMaxErrSize) {
        NCBI_THROW(CNetScheduleException, eDataTooLong,
                   "Error message too long");
    }

    CheckConnect(job_key);
    CSockGuard sg(GetConnMode() == eKeepConnection ? 0 : m_Sock);

    MakeCommandPacket(&m_Tmp, "FPUT ");

    m_Tmp.append(job_key);
    m_Tmp.append(" ");
    m_Tmp.append(" \"");
    m_Tmp.append(NStr::PrintableString(err_msg));
    m_Tmp.append("\"");
    m_Tmp.append(" \"");
    m_Tmp.append(NStr::PrintableString(output));
    m_Tmp.append("\" ");
    m_Tmp.append(NStr::IntToString(ret_code));

    WriteStr(m_Tmp.c_str(), m_Tmp.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Communication error");
    }
    CheckServerOK(&m_Tmp);
}


void CNetScheduleClient::StatusSnapshot(TStatusMap*   status_map,
                                        const string& affinity_token)
{
    _ASSERT(status_map);

    if (m_RequestRateControl) {
        s_Throttler->Approve(CRequestRateControl::eSleep);
    }
    CheckConnect(kEmptyStr);
    CSockGuard sg(GetConnMode() == eKeepConnection ? 0 : m_Sock);

    MakeCommandPacket(&m_Tmp, "STSN ");
    m_Tmp.append(" aff=\"");
    m_Tmp.append(affinity_token);
    m_Tmp.append("\"");

    WriteStr(m_Tmp.c_str(), m_Tmp.length() + 1);
    WaitForServer();

    while (1) {
        if (!ReadStr(*m_Sock, &m_Tmp)) {
            break;
        }
        TrimPrefix(&m_Tmp);
        if (m_Tmp == "END")
            break;

        // parse the status message
        string st_str, cnt_str;
        bool delim = NStr::SplitInTwo(m_Tmp, " ", st_str, cnt_str);
        if (delim) {
            EJobStatus status = StringToStatus(st_str);
            unsigned cnt = NStr::StringToUInt(cnt_str);
            (*status_map)[status] += cnt;
        }
    }

}


void CNetScheduleClient::ReturnJob(const string& job_key)
{
    if (m_RequestRateControl) {
        s_Throttler->Approve(CRequestRateControl::eSleep);
    }

    CheckConnect(job_key);
    CSockGuard sg(GetConnMode() == eKeepConnection ? 0 : m_Sock);

    MakeCommandPacket(&m_Tmp, "RETURN ");
    m_Tmp.append(job_key);

    WriteStr(m_Tmp.c_str(), m_Tmp.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Communication error");
    }
    CheckServerOK(&m_Tmp);
}

void CNetScheduleClient::RegUnregClient(const string&  cmd,
                                        unsigned short udp_port)
{
    if (m_RequestRateControl) {
        s_Throttler->Approve(CRequestRateControl::eSleep);
    }
    CheckConnect(kEmptyStr);
    CSockGuard sg(GetConnMode() == eKeepConnection ? 0 : m_Sock);

    MakeCommandPacket(&m_Tmp, cmd);
    m_Tmp.append(NStr::IntToString(udp_port));
    WriteStr(m_Tmp.c_str(), m_Tmp.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Communication error");
    }
    CheckServerOK(&m_Tmp);
}


void CNetScheduleClient::RegisterClient(unsigned short udp_port)
{
    RegUnregClient("REGC ", udp_port);
}

void CNetScheduleClient::UnRegisterClient(unsigned short udp_port)
{
    RegUnregClient("URGC ", udp_port);
}


void CNetScheduleClient::ShutdownServer(CNetScheduleClient::EShutdownLevel level)
{
    CheckConnect(kEmptyStr);
    CSockGuard sg(GetConnMode() == eKeepConnection ? 0 : m_Sock);

    string cmd = "SHUTDOWN ";
    switch( level ) {
    case eDie :
        cmd = "SHUTDOWN SUICIDE ";
        break;
    case eShutdownImmidiate :
        cmd = "SHUTDOWN IMMEDIATE ";
        break;
    default:
        break;
    }

    MakeCommandPacket(&m_Tmp, cmd);
    WriteStr(m_Tmp.c_str(), m_Tmp.length() + 1);
    if (level == eDie)
        return;

    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Communication error");
    }
    CheckServerOK(&m_Tmp);
}


void CNetScheduleClient::ReloadServerConfig()
{
    if (m_RequestRateControl) {
        s_Throttler->Approve(CRequestRateControl::eSleep);
    }

    CheckConnect(kEmptyStr);
    CSockGuard sg(GetConnMode() == eKeepConnection ? 0 : m_Sock);

    MakeCommandPacket(&m_Tmp, "RECO ");
    WriteStr(m_Tmp.c_str(), m_Tmp.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Communication error");
    }
    TrimPrefix(&m_Tmp);
}


string CNetScheduleClient::ServerVersion()
{
    if (m_RequestRateControl) {
        s_Throttler->Approve(CRequestRateControl::eSleep);
    }

    CheckConnect(kEmptyStr);
    CSockGuard sg(GetConnMode() == eKeepConnection ? 0 : m_Sock);

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


void CNetScheduleClient::CreateQueue(const string& qname, const string& qclass,
                                     const string& comment)
{
    if (m_RequestRateControl) {
        s_Throttler->Approve(CRequestRateControl::eSleep);
    }

    CheckConnect(kEmptyStr);
    CSockGuard sg(GetConnMode() == eKeepConnection ? 0 : m_Sock);

    string param = qname + " " + qclass;
    if (!comment.empty()) {
        param.append(" \"");
        param.append(comment);
        param.append("\"");
    }
    CommandInitiate("QCRE ", param, &m_Tmp);

    CheckServerOK(&m_Tmp);
}


void CNetScheduleClient::DeleteQueue(const string& qname)
{
    if (m_RequestRateControl) {
        s_Throttler->Approve(CRequestRateControl::eSleep);
    }

    CheckConnect(kEmptyStr);
    CSockGuard sg(GetConnMode() == eKeepConnection ? 0 : m_Sock);

    CommandInitiate("QDEL ", qname, &m_Tmp);

    CheckServerOK(&m_Tmp);
}


void CNetScheduleClient::DumpQueue(CNcbiOstream& out,
                                   const string& job_key)
{
    if (m_RequestRateControl) {
        s_Throttler->Approve(CRequestRateControl::eSleep);
    }

    CheckConnect(job_key);
    CSockGuard sg(GetConnMode() == eKeepConnection ? 0 : m_Sock);

    MakeCommandPacket(&m_Tmp, "DUMP ");
    m_Tmp += job_key;
    WriteStr(m_Tmp.c_str(), m_Tmp.length() + 1);

    WaitForServer();
    PrintServerOut(out);
}

void CNetScheduleClient::PrintQueue(CNcbiOstream& out,
                                    EJobStatus    status)
{
    if (m_RequestRateControl) {
        s_Throttler->Approve(CRequestRateControl::eSleep);
    }

    CheckConnect(kEmptyStr);
    CSockGuard sg(GetConnMode() == eKeepConnection ? 0 : m_Sock);

    string status_str = CNetScheduleClient::StatusToString(status);

    MakeCommandPacket(&m_Tmp, "QPRT ");
    m_Tmp += status_str;
    WriteStr(m_Tmp.c_str(), m_Tmp.length() + 1);

    WaitForServer();
    PrintServerOut(out);
}



void CNetScheduleClient::PrintStatistics(CNcbiOstream & out,
                                         EStatisticsOptions opt)
{
    if (m_RequestRateControl) {
        s_Throttler->Approve(CRequestRateControl::eSleep);
    }

    CheckConnect(kEmptyStr);
    CSockGuard sg(GetConnMode() == eKeepConnection ? 0 : m_Sock);

    MakeCommandPacket(&m_Tmp, "STAT ");
    if (opt == eStatisticsAll) {
        m_Tmp.append("ALL");
    }
    WriteStr(m_Tmp.c_str(), m_Tmp.length() + 1);

    WaitForServer();
    PrintServerOut(out);
}


void CNetScheduleClient::Monitor(CNcbiOstream & out)
{
    CheckConnect(kEmptyStr);
    CSockGuard sg(*m_Sock);

    MakeCommandPacket(&m_Tmp, "MONI ");
    WriteStr(m_Tmp.c_str(), m_Tmp.length() + 1);
    m_Tmp = "QUIT";
    WriteStr(m_Tmp.c_str(), m_Tmp.length() + 1);

    STimeout rto;
    rto.sec = 1;
    rto.usec = 0;
    m_Sock->SetTimeout(eIO_Read, &rto);

    string line;
    while (1) {

        EIO_Status st = m_Sock->ReadLine(line);
        if (st == eIO_Success) {
            if (m_Tmp == "END")
                break;
            out << line << "\n" << flush;
        } else {
            EIO_Status st = m_Sock->GetStatus(eIO_Open);
            if (st != eIO_Success) {
                break;
            }
        }
    }

}


string CNetScheduleClient::GetQueueList()
{
    CheckConnect(kEmptyStr);
    CSockGuard sg(GetConnMode() == eKeepConnection ? 0 : m_Sock);

    CommandInitiate("QLST ", "", &m_Tmp);
    TrimPrefix(&m_Tmp);

    return m_Tmp;
}


void CNetScheduleClient::DropQueue()
{
    CheckConnect(kEmptyStr);
    CSockGuard sg(GetConnMode() == eKeepConnection ? 0 : m_Sock);

    CommandInitiate("DROPQ ", "", &m_Tmp);
    CheckServerOK(&m_Tmp);
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
    CheckServerOK(str);
}


void CNetScheduleClient::ProcessServerError(string* response, ETrimErr trim_err)
{
    if (trim_err == eTrimErr)
        TrimErr(response);
    string code;
    string msg;
    if (NStr::SplitInTwo(*response, ":", code, msg)) {
        // Map code into numeric value
        CException::TErrCode n_code = sm_ExceptionMap.GetCode(code);
        if (n_code != CException::eInvalid) {
            NCBI_THROW(CNetScheduleException, EErrCode(n_code), msg);
        }
    }
    CNetServiceClient::ProcessServerError(response, eNoTrimErr);
}


void CNetScheduleClient::MakeCommandPacket(string*       out_str,
                                           const string& cmd_str)
{
    // Check if authentication prefix has already been added
    // and sent to the server
    if (m_AuthenticationSent) {
        *out_str = cmd_str;
        return;
    }
    m_AuthenticationSent = true;

    // command with full connection establishment

    if (m_ClientName.length() < 3) {
        NCBI_THROW(CNetScheduleException,
                   eAuthenticationError, "Client name too short or empty");
    }
    if (m_Queue.empty()) {
        NCBI_THROW(CNetScheduleException,
                   eAuthenticationError, "Empty queue name");
    }

    *out_str = m_ClientName;

    if (!m_ProgramVersion.empty()) {
        out_str->append(" prog='");
        out_str->append(m_ProgramVersion);
        out_str->append("'");
    }
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


bool CNetScheduleClient::CheckConnect(const string& key)
{
    m_JobKey.id = 0;
    if (m_Sock && (eIO_Success == m_Sock->GetStatus(eIO_Open))) {
        bool expired = CheckConnExpired();
        if (!expired) {
            if (key.empty()) {
                return false; // we are connected, nothing to do
            }
            // key is not empty, check if we are at the correct server
            CNetSchedule_ParseJobKey(&m_JobKey, key);
            if (m_JobKey.port != m_Port ||
                NStr::CompareNocase(m_JobKey.hostname, m_Host) != 0) {
                    m_Sock->Close();
                    CreateSocket(m_JobKey.hostname, m_JobKey.port);
                    m_AuthenticationSent = false;
                    return true;
            } else {
                return false;
            }

        } else {
            m_Sock->Close();
            m_AuthenticationSent = false;
        }
    }

    if (!key.empty()) {
        CNetSchedule_ParseJobKey(&m_JobKey, key);
        CreateSocket(m_JobKey.hostname, m_JobKey.port);
        m_AuthenticationSent = false;
        return true;
    }

    // not connected

    if (!m_Host.empty()) { // we can restore connection
        CreateSocket(m_Host, m_Port);
        m_AuthenticationSent = false;
        return true;
    }

    NCBI_THROW(CNetServiceException, eCommunicationError,
            "Cannot establish connection with a server. Unknown host-port.");

    return false;
}


bool CNetScheduleClient::CheckConnExpired()
{
    _ASSERT(m_Sock);
    STimeout to = {0, 0};
    EIO_Status io_st = m_Sock->Wait(eIO_Read, &to);
    switch (io_st) {
    case eIO_Success:
        {
            string msg;
            io_st = m_Sock->ReadLine(msg);
            if (io_st == eIO_Closed) {
                return true;
            }
            if (!msg.empty()) {
                // TODO:
                // check the message
                return true;
            }
        }
        break;
    case eIO_Closed:
        return true;
    default:
        break;
    }
    return false;
}


void CNetScheduleClient::CloseConnection()
{
    if (m_Sock) {
        m_Sock->Close();
    }
}


bool CNetScheduleClient::IsError(const char* str)
{
    int cmp = NStr::strncasecmp(str, "ERR:", 4);
    return cmp == 0;
}


void CNetScheduleClient::MakeJobKey(string*       job_key,
                                    unsigned      job_id,
                                    const string& host,
                                    unsigned      port)
{
    _ASSERT(job_key);
    char buf[2048];
    sprintf(buf, NETSCHEDULE_JOBMASK,
            job_id, host.c_str(), port);
    *job_key = buf;
}


EIO_Status CNetScheduleClient::Connect(unsigned int   addr,
                                       unsigned short port)
{
    m_AuthenticationSent = false;
    if (m_Sock) {

        m_Host = CSocketAPI::gethostbyaddr(addr);
        m_Port = port;
        m_Sock->Connect(m_Host, m_Port);
    } else {
        SetSocket(new CSocket(addr, port), eTakeOwnership);
    }
    return m_Sock->GetStatus(eIO_Open);
}


END_NCBI_SCOPE
