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
 * Author:  Anatoliy Kuznetsov, Maxim Didenko, Dmitry Kazimirov
 *
 * File Description:
 *   Implementation of NetSchedule API.
 *
 */

#include <ncbi_pch.hpp>

#include "netschedule_api_impl.hpp"

#define NCBI_USE_ERRCODE_X   ConnServ_NetSchedule

BEGIN_NCBI_SCOPE

#define SKIP_SPACE(ptr) \
    while (isspace((unsigned char) *(ptr))) \
        ++ptr;

static bool s_DoParseGetJobResponse(
    CNetScheduleJob& job, const string& response)
{
    // Server message format:
    //    JOB_KEY "input" "affinity" "client_ip session_id" mask auth_token

    // 1. Extract job ID.
    const char* response_begin = response.c_str();
    const char* response_end = response_begin + response.length();

    SKIP_SPACE(response_begin);

    if (*response_begin == 0)
        return false;

    const char* ptr = response_begin;

    do
        if (*++ptr == 0)
            return false;
    while (!isspace((unsigned char) (*ptr)));

    job.job_id.assign(response_begin, ptr - response_begin);

    while (isspace((unsigned char) *++ptr))
        ;

    try {
        size_t field_len;

        // 2. Extract job input
        job.input = NStr::ParseQuoted(CTempString(ptr,
            response_end - ptr), &field_len);

        ptr += field_len;
        SKIP_SPACE(ptr);

        // 3. Extract optional job affinity.
        job.affinity = NStr::ParseQuoted(CTempString(ptr,
            response_end - ptr), &field_len);

        ptr += field_len;
        SKIP_SPACE(ptr);

        // 4. Extract optional "client_ip session_id".
        string client_ip_and_session_id(NStr::ParseQuoted(
            CTempString(ptr, response_end - ptr), &field_len));

        NStr::SplitInTwo(client_ip_and_session_id, " ",
            job.client_ip, job.session_id);

        ptr += field_len;
        SKIP_SPACE(ptr);

        // 5. Parse job mask.
        job.mask = atoi(ptr);

        while (!isspace((unsigned char) *ptr) && *ptr != '\0')
            ++ptr;

        SKIP_SPACE(ptr);

        if (*ptr == '\0') {
            ERR_POST("GET2: missing auth_token");
            return false;
        }

        // 6. Retrieve auth token.
        job.auth_token = ptr;
    }
    catch (CStringException& e) {
        ERR_POST("Error while parsing GET2 response " << e);
        return false;
    }

    return true;
}

static bool s_ParseGetJobResponse(CNetScheduleJob& job, const string& response)
{
    if (response.empty())
        return false;

    if (s_DoParseGetJobResponse(job, response))
        return true;

    NCBI_THROW(CNetScheduleException, eProtocolSyntaxError,
        "Cannot parse server output for " +
            job.job_id + ":\n" + response);
}

////////////////////////////////////////////////////////////////////////
void CNetScheduleExecutor::JobDelayExpiration(const string& job_key,
                                              unsigned      runtime_inc)
{
    m_Impl->m_API->x_SendJobCmdWaitResponse("JDEX" , job_key, runtime_inc);
}

class CGetJobCmdExecutor : public INetServerFinder
{
public:
    CGetJobCmdExecutor(const string& cmd, CNetScheduleJob& job) :
      m_Cmd(cmd), m_Job(job)
      {
      }

      virtual bool Consider(CNetServer server);

private:
    const string& m_Cmd;
    CNetScheduleJob& m_Job;
};

bool CGetJobCmdExecutor::Consider(CNetServer server)
{
    return s_ParseGetJobResponse(m_Job, server.ExecWithRetry(m_Cmd).response);
}

const CNetScheduleAPI::SServerParams& CNetScheduleExecutor::GetServerParams()
{
    return m_Impl->m_API->GetServerParams();
}

void CNetScheduleExecutor::UnRegisterClient()
{
    string cmd("CLRN");

    for (CNetServiceIterator it = m_Impl->m_API->m_Service.
        Iterate(CNetService::eIncludePenalized); it; ++it) {
            CNetServer server = *it;

            try {
                server.ExecWithRetry(cmd);
            } catch (CNetServiceException& ex) {
                if (ex.GetErrCode() != CNetServiceException::eCommunicationError)
                    throw;
                else {
                    ERR_POST_X(12, server->m_ServerInPool->m_Address.AsString() <<
                        ": " << ex.what());
                }
            }
    }
}

bool CNetScheduleExecutor::GetJob(CNetScheduleJob& job,
        CNetScheduleExecutor::EJobAffinityPreference affinity_preference,
        const string& affinity_list,
        CAbsTimeout* timeout)
{
    string cmd(m_Impl->m_NotificationHandler.MkBaseGETCmd(
            affinity_preference, affinity_list));

    m_Impl->m_NotificationHandler.CmdAppendTimeout(cmd, timeout);

    if (m_Impl->m_NotificationHandler.RequestJob(m_Impl,
            job, cmd, timeout))
        return true;

    if (timeout == NULL)
        return false;

    while (m_Impl->m_NotificationHandler.WaitForNotification(*timeout)) {
        CNetServer server;
        if (m_Impl->m_NotificationHandler.CheckRequestJobNotification(m_Impl,
                &server) && s_ParseGetJobResponse(job,
                        server.ExecWithRetry(cmd).response)) {
            // Notify the rest of NetSchedule servers that
            // we are no longer listening on the UDP socket.
            CNetServiceIterator it(
                    m_Impl->m_API->m_Service.Iterate(server));
            while (++it)
                (*it).ExecWithRetry("CWGET");
            return true;
        }
    }

    return false;
}

bool CNetScheduleExecutor::GetJob(CNetScheduleJob& job,
        unsigned wait_time,
        CNetScheduleExecutor::EJobAffinityPreference affinity_preference,
        const string& affinity_list)
{
    if (wait_time == 0)
        return GetJob(job, affinity_preference, affinity_list);
    else {
        CAbsTimeout abs_timeout(wait_time, 0);

        return GetJob(job, affinity_preference,
            affinity_list, &abs_timeout);
    }
}

string CNetScheduleNotificationHandler::MkBaseGETCmd(
    CNetScheduleExecutor::EJobAffinityPreference affinity_preference,
    const string& affinity_list)
{
    string cmd;

    switch (affinity_preference) {
    case CNetScheduleExecutor::ePreferredAffsOrAnyJob:
        cmd = "GET2 wnode_aff=1 any_aff=1";
        break;

    case CNetScheduleExecutor::ePreferredAffinities:
        cmd = "GET2 wnode_aff=1 any_aff=0";
        break;

    case CNetScheduleExecutor::eAnyJob:
        cmd = "GET2 wnode_aff=0 any_aff=1";
        break;

    case CNetScheduleExecutor::eExplicitAffinitiesOnly:
        cmd = "GET2 wnode_aff=0 any_aff=0";
    }

    if (!affinity_list.empty()) {
        list<CTempString> affinity_tokens;

        NStr::Split(affinity_list, ",", affinity_tokens);

        ITERATE(list<CTempString>, token, affinity_tokens) {
            SNetScheduleAPIImpl::VerifyAffinityAlphabet(*token);
        }

        cmd += " aff=";
        cmd += affinity_list;
    }

    return cmd;
}

void CNetScheduleNotificationHandler::CmdAppendTimeout(
        string& cmd, CAbsTimeout* timeout)
{
    if (timeout != NULL) {
        cmd += " port=";
        cmd += NStr::UIntToString(GetPort());

        cmd += " timeout=";
        cmd += NStr::UIntToString(s_GetRemainingSeconds(*timeout));
    }
}

bool CNetScheduleNotificationHandler::RequestJob(
        CNetScheduleExecutor::TInstance executor,
        CNetScheduleJob& job,
        string cmd,
        CAbsTimeout* timeout)
{
    if (timeout != NULL) {
        cmd += " port=";
        cmd += NStr::UIntToString(GetPort());

        cmd += " timeout=";
        cmd += NStr::UIntToString(s_GetRemainingSeconds(*timeout));
    }

    CGetJobCmdExecutor get_executor(cmd, job);

    CNetServiceIterator it(executor->m_API->m_Service.FindServer(&get_executor,
            CNetService::eIncludePenalized));

    if (!it)
        return false;

    while (--it)
        (*it).ExecWithRetry("CWGET");

    return true;
}

#define GET2_NOTIF_ATTR_COUNT 2

bool CNetScheduleNotificationHandler::CheckRequestJobNotification(
        CNetScheduleExecutor::TInstance executor, CNetServer* server)
{
    static const string attr_names[GET2_NOTIF_ATTR_COUNT] =
        {"ns_node", "queue"};

    string attr_values[GET2_NOTIF_ATTR_COUNT];

    if (ParseNotification(attr_names, attr_values, GET2_NOTIF_ATTR_COUNT) !=
                    GET2_NOTIF_ATTR_COUNT ||
            attr_values[1] != executor->m_API.GetQueueName())
        return false;

    CNetScheduleServerListener::TNodeIdToServerMap& server_map(
            executor->m_API->GetListener()->m_ServerByNSNodeId);

    CNetScheduleServerListener::TNodeIdToServerMap::iterator server_it(
            server_map.find(attr_values[0]));

    if (server_it == server_map.end())
        return false;

    *server = new SNetServerImpl(executor->m_API->m_Service, executor->
            m_API->m_Service->m_ServerPool->ReturnServer(server_it->second));

    return true;
}

inline
void static s_CheckOutputSize(const string& output, size_t max_output_size)
{
    if (output.length() > max_output_size) {
        NCBI_THROW(CNetScheduleException, eDataTooLong,
                   "Output data too long.");
    }
}


void CNetScheduleExecutor::PutResult(const CNetScheduleJob& job)
{
    s_CheckOutputSize(job.output,
        m_Impl->m_API->GetServerParams().max_output_size);

    string cmd("PUT2 job_key=" + job.job_id);

    SNetScheduleAPIImpl::VerifyAuthTokenAlphabet(job.auth_token);
    cmd.append(" auth_token=");
    cmd.append(job.auth_token);

    cmd.append(" job_return_code=");
    cmd.append(NStr::NumericToString(job.ret_code));

    cmd.append(" output=\"");
    cmd.append(NStr::PrintableString(job.output));
    cmd.push_back('\"');

    m_Impl->m_API->GetServer(job.job_id).ExecWithRetry(cmd);
}

void CNetScheduleExecutor::PutProgressMsg(const CNetScheduleJob& job)
{
    if (job.progress_msg.length() >= kNetScheduleMaxDataSize) {
        NCBI_THROW(CNetScheduleException, eDataTooLong,
                   "Progress message too long");
    }
    m_Impl->m_API->x_SendJobCmdWaitResponse("MPUT",
        job.job_id, job.progress_msg);
}

void CNetScheduleExecutor::GetProgressMsg(CNetScheduleJob& job)
{
    m_Impl->m_API.GetProgressMsg(job);
}

void CNetScheduleExecutor::PutFailure(const CNetScheduleJob& job)
{
    s_CheckOutputSize(job.output,
        m_Impl->m_API->GetServerParams().max_output_size);

    if (job.error_msg.length() >= kNetScheduleMaxErrSize) {
        NCBI_THROW(CNetScheduleException, eDataTooLong,
                   "Error message too long");
    }

    string cmd("FPUT2 job_key=" + job.job_id);

    SNetScheduleAPIImpl::VerifyAuthTokenAlphabet(job.auth_token);
    cmd.append(" auth_token=");
    cmd.append(job.auth_token);

    cmd.append(" err_msg=\"");
    cmd.append(NStr::PrintableString(job.error_msg));

    cmd.append("\" output=\"");
    cmd.append(NStr::PrintableString(job.output));

    cmd.append("\" job_return_code=");
    cmd.append(NStr::NumericToString(job.ret_code));

    m_Impl->m_API->GetServer(job.job_id).ExecWithRetry(cmd);
}

CNetScheduleAPI::EJobStatus
    CNetScheduleExecutor::GetJobStatus(const string& job_key)
{
    return m_Impl->m_API->x_GetJobStatus(job_key, false);
}

void CNetScheduleExecutor::ReturnJob(const string& job_key,
        const string& auth_token)
{
    string cmd("RETURN2 job_key=" + job_key);

    SNetScheduleAPIImpl::VerifyAuthTokenAlphabet(auth_token);
    cmd.append(" auth_token=");
    cmd.append(auth_token);

    m_Impl->m_API->GetServer(job_key).ExecWithRetry(cmd);
}

static void s_AppendAffinityTokens(string& cmd,
        const char* sep, const vector<string>& affs)
{
    if (!affs.empty()) {
        ITERATE(vector<string>, aff, affs) {
            cmd.append(sep);
            SNetScheduleAPIImpl::VerifyAffinityAlphabet(*aff);
            cmd.append(*aff);
            sep = ",";
        }
        cmd.push_back('"');
    }
}

void CNetScheduleExecutor::ChangePreferredAffinities(
    const vector<string>& affs_to_add, const vector<string>& affs_to_delete)
{
    string cmd("CHAFF");

    s_AppendAffinityTokens(cmd, " add=\"", affs_to_add);
    s_AppendAffinityTokens(cmd, " del=\"", affs_to_delete);

    m_Impl->m_API->m_Service.ExecOnAllServers(cmd);
}

const string& CNetScheduleExecutor::GetQueueName()
{
    return m_Impl->m_API.GetQueueName();
}

const string& CNetScheduleExecutor::GetClientName()
{
    return m_Impl->m_API->m_Service->m_ServerPool.GetClientName();
}

const string& CNetScheduleExecutor::GetServiceName()
{
    return m_Impl->m_API->m_Service.GetServiceName();
}

END_NCBI_SCOPE
