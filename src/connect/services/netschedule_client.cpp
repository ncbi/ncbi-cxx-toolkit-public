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
#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_conn_exception.hpp>
#include <connect/netschedule_client.hpp>
#include <memory>


BEGIN_NCBI_SCOPE


const string kNetSchedule_KeyPrefix = "JSID";

unsigned CNetSchedule_GetJobId(const string&  key_str)
{
    unsigned job_id;
    const char* ch = key_str.c_str();

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
      m_Queue(queue_name)
{
    if (m_Sock) {
        m_Sock->DisableOSSendDelay();
        RestoreHostPort();
    }
}

CNetScheduleClient::~CNetScheduleClient()
{
}


string CNetScheduleClient::SubmitJob(const string& input)
{
    if (input.length() > kNetScheduleMaxDataSize) {
        NCBI_THROW(CNetScheduleException, eDataTooLong, 
            "Input data too long.");
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

void CNetScheduleClient::CancelJob(const string& job_key)
{
    CheckConnect(kEmptyStr);
    CSockGuard sg(*m_Sock);

    CommandInitiate("CANCEL ", job_key, &m_Tmp);

    CheckOK(&m_Tmp);
}

CNetScheduleClient::EJobStatus 
CNetScheduleClient::GetStatus(const string& job_key,
                              int*          ret_code,
                              string*       output)
{
    EJobStatus status;

    CheckConnect(kEmptyStr);
    CSockGuard sg(*m_Sock);

    CommandInitiate("STATUS ", job_key, &m_Tmp);

    TrimPrefix(&m_Tmp);

    if (m_Tmp.empty()) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Invalid server response. Empty key.");
    }

    const char* str = m_Tmp.c_str();

    int st = atoi(str);
    status = (EJobStatus) st;

    if (status == eDone && (ret_code || output)) {
        for ( ;*str && isdigit(*str); ++str) {
        }

        for ( ; *str && isspace(*str); ++str) {
        }

        if (ret_code) {
            *ret_code = atoi(str);
        }

        if (output) {
            output->erase();

            for ( ;*str && isdigit(*str); ++str) {
            }
            for ( ; *str && isspace(*str); ++str) {
            }

            if (*str && *str == '"') {
                ++str;
                for( ;*str && *str != '"'; ++str) {
                    output->push_back(*str);
                }
            }
        }
    }

    return status;
}

bool CNetScheduleClient::GetJob(string* job_key, string* input)
{
    CheckConnect(kEmptyStr);
    CSockGuard sg(*m_Sock);

    CommandInitiate("GET ", kEmptyStr, &m_Tmp);

    TrimPrefix(&m_Tmp);

    if (m_Tmp.empty()) {
        return false;
    }

    input->erase();
    job_key->erase();

    const char* str = m_Tmp.c_str();

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

    _ASSERT(!job_key->empty());
    _ASSERT(!input->empty());

    return true;
}

void CNetScheduleClient::PutResult(const string& job_key, 
                                   int           ret_code, 
                                   const string& output)
{
    CheckConnect(kEmptyStr);
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


void CNetScheduleClient::ShutdownServer()
{
    CheckConnect(kEmptyStr);
    CSockGuard sg(*m_Sock);

    MakeCommandPacket(&m_Tmp, "SHUTDOWN ");
    WriteStr(m_Tmp.c_str(), m_Tmp.length() + 1);
}

string CNetScheduleClient::ServerVersion()
{
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
    const string& client_name_comment = GetClientNameComment();
    if (!client_name_comment.empty()) {
        out_str->append(" ");
        out_str->append(client_name_comment);
    }
    out_str->append("\r\n");
    out_str->append(m_Queue);
    out_str->append("\r\n");
    out_str->append(cmd_str);
}


void CNetScheduleClient::CheckConnect(const string& key)
{
    if (m_Sock && (eIO_Success == m_Sock->GetStatus(eIO_Open))) {
        return; // we are connected, nothing to do
    }

    // not connected

    if (!m_Host.empty()) { // we can restore connection
        CreateSocket(m_Host, m_Port);
        return;
    }

    // no primary host information

    if (key.empty()) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
           "Cannot establish connection with a server. Unknown host name.");
    }

    CNetSchedule_Key job_key;
    CNetSchedule_ParseJobKey(&job_key, key);
    CreateSocket(job_key.hostname, job_key.port);
}

bool CNetScheduleClient::IsError(const char* str)
{
    int cmp = NStr::strncasecmp(str, "ERR:", 4);
    return cmp == 0;
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
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
