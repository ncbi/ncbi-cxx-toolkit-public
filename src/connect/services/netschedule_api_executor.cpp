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

static bool s_DoParseGet2JobResponse(
    CNetScheduleJob& job, const string& response)
{
    enum {
        eJobKey,
        eJobInput,
        eJobAuthToken,
        eJobAffinity,
        eClientIP,
        eClientSessionID,
        eJobMask,
        eNumberOfJobBits
    };
    int job_bits = 0;
    CUrlArgs url_parser(response);
    ITERATE(CUrlArgs::TArgs, field, url_parser.GetArgs()) {
        switch (field->name[0]) {
        case 'j':
            if (field->name == "job_key") {
                job_bits |= (1 << eJobKey);
                job.job_id = field->value;
            }
            break;

        case 'i':
            if (field->name == "input") {
                job_bits |= (1 << eJobInput);
                job.input = field->value;
            }
            break;

        case 'a':
            if (field->name == "auth_token") {
                job_bits |= (1 << eJobAuthToken);
                job.auth_token = field->value;
            } else if (field->name == "affinity") {
                job_bits |= (1 << eJobAffinity);
                job.affinity = field->value;
            }
            break;

        case 'c':
            if (field->name == "client_ip") {
                job_bits |= (1 << eClientIP);
                job.client_ip = field->value;
            } else if (field->name == "client_sid") {
                job_bits |= (1 << eClientSessionID);
                job.session_id = field->value;
            }
            break;

        case 'm':
            if (field->name == "mask") {
                job_bits |= (1 << eJobMask);
                job.mask = atoi(field->value.c_str());
            }
        }
        if (job_bits == (1 << eNumberOfJobBits) - 1)
            break;
    }
    return !job.job_id.empty();
}

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

bool g_ParseGetJobResponse(CNetScheduleJob& job, const string& response)
{
    if (response.empty())
        return false;

    try {
        if (s_DoParseGet2JobResponse(job, response))
            return true;
    }
    catch (CUrlParserException&) {
        if (s_DoParseGetJobResponse(job, response))
            return true;
    }

    NCBI_THROW(CNetScheduleException, eProtocolSyntaxError,
            "Cannot parse server output for " +
                    job.job_id + ":\n" + response);
}

////////////////////////////////////////////////////////////////////////
void CNetScheduleExecutor::JobDelayExpiration(const string& job_key,
                                              unsigned      runtime_inc)
{
    string cmd(g_MakeBaseCmd("JDEX", job_key));
    cmd += ' ';
    cmd += NStr::NumericToString(runtime_inc);
    g_AppendClientIPAndSessionID(cmd);
    m_Impl->m_API->GetServer(job_key).ExecWithRetry(cmd);
}

class CGetJobCmdExecutor : public INetServerFinder
{
public:
    CGetJobCmdExecutor(const string& get_cmd,
            CNetScheduleJob& job, SNetScheduleExecutorImpl* executor) :
        m_GetCmd(get_cmd), m_Job(job), m_Executor(executor)
    {
    }

    virtual bool Consider(CNetServer server);

private:
    const string& m_GetCmd;
    CNetScheduleJob& m_Job;
    SNetScheduleExecutorImpl* m_Executor;
};

bool CGetJobCmdExecutor::Consider(CNetServer server)
{
    return m_Executor->ExecGET(server, m_GetCmd, m_Job);
}

const CNetScheduleAPI::SServerParams& CNetScheduleExecutor::GetServerParams()
{
    return m_Impl->m_API->GetServerParams();
}

void CNetScheduleExecutor::ClearNode()
{
    string cmd("CLRN");
    g_AppendClientIPAndSessionID(cmd);

    for (CNetServiceIterator it = m_Impl->m_API->m_Service.
            Iterate(CNetService::eIncludePenalized); it; ++it) {
        CNetServer server = *it;

        try {
            CNetServer::SExecResult exec_result;
            server->ConnectAndExec(cmd, exec_result);
        } catch (CNetSrvConnException& e) {
            if (m_Impl->m_API->m_Service.IsLoadBalanced()) {
                ERR_POST(server->m_ServerInPool->m_Address.AsString() <<
                    ": " << e.what());
            }
        } catch (CNetServiceException& e) {
            if (e.GetErrCode() != CNetServiceException::eCommunicationError)
                throw;
            else {
                ERR_POST_X(12, server->m_ServerInPool->m_Address.AsString() <<
                    ": " << e.what());
            }
        }
    }
}

void CNetScheduleExecutor::SetAffinityPreference(
        CNetScheduleExecutor::EJobAffinityPreference aff_pref)
{
    m_Impl->m_AffinityPreference = aff_pref;
}

void SNetScheduleExecutorImpl::ClaimNewPreferredAffinity(
        CNetServer orig_server, const string& affinity)
{
    if (m_AffinityPreference != CNetScheduleExecutor::eClaimNewPreferredAffs ||
            affinity.empty())
        return;

    CFastMutexGuard guard(m_PreferredAffMutex);

    if (m_PreferredAffinities.find(affinity) == m_PreferredAffinities.end()) {
        m_PreferredAffinities.insert(affinity);
        string new_preferred_aff_cmd = "CHAFF add=" + affinity;
        g_AppendClientIPAndSessionID(new_preferred_aff_cmd);
        for (CNetServiceIterator it =
                m_API->m_Service.ExcludeServer(orig_server); it; ++it)
            try {
                (*it).ExecWithRetry(new_preferred_aff_cmd);
            }
            catch (CException& e) {
                ERR_POST("Error while notifying " <<
                        (*it).GetServerAddress() <<
                        " of a new affinity " << e);

                m_API->GetListener()->SetAffinitiesSynced(
                        it.GetServer(), false);
            }
    }
}

string SNetScheduleExecutorImpl::MkSETAFFCmd()
{
    CFastMutexGuard guard(m_PreferredAffMutex);

    string cmd("SETAFF aff=\"");
    const char* sep = "";
    ITERATE(set<string>, it, m_PreferredAffinities) {
        cmd += sep;
        cmd += *it;
        sep = ",";
    }
    cmd += '"';
    g_AppendClientIPAndSessionID(cmd);

    return cmd;
}

bool SNetScheduleExecutorImpl::ExecGET(SNetServerImpl* server,
        const string& get_cmd, CNetScheduleJob& job)
{
    CNetScheduleGETCmdListener get_cmd_listener(this);

    CNetServer::SExecResult exec_result;

    try {
        server->ConnectAndExec(get_cmd, exec_result, NULL, &get_cmd_listener);
    }
    catch (CNetScheduleException& e) {
        if (e.GetErrCode() != CNetScheduleException::ePrefAffExpired)
            throw;

        CNetScheduleServerListener* listener = m_API->GetListener();
        listener->SetAffinitiesSynced(server, false);
        server->ConnectAndExec(MkSETAFFCmd(), exec_result);
        listener->SetAffinitiesSynced(server, true);

        server->ConnectAndExec(get_cmd, exec_result, NULL, &get_cmd_listener);
    }

    if (!g_ParseGetJobResponse(job, exec_result.response))
        return false;

    // If a new preferred affinity is given by the server,
    // register it with the rest of servers.
    ClaimNewPreferredAffinity(server, job.affinity);

    return true;
}

bool CNetScheduleExecutor::GetJob(CNetScheduleJob& job,
        const string& affinity_list,
        CDeadline* deadline)
{
    string base_cmd(CNetScheduleNotificationHandler::MkBaseGETCmd(
            m_Impl->m_AffinityPreference, affinity_list));

    if (m_Impl->m_NotificationHandler.RequestJob(m_Impl, job,
            m_Impl->m_NotificationHandler.CmdAppendTimeoutAndClientInfo(
                    base_cmd, deadline)))
        return true;

    if (deadline == NULL)
        return false;

    while (m_Impl->m_NotificationHandler.WaitForNotification(*deadline)) {
        CNetServer server;
        if (m_Impl->m_NotificationHandler.CheckRequestJobNotification(m_Impl,
                &server) && g_ParseGetJobResponse(job, server.ExecWithRetry(
                    m_Impl->m_NotificationHandler.CmdAppendTimeoutAndClientInfo(
                        base_cmd, deadline)).response)) {
            // Notify the rest of NetSchedule servers that
            // we are no longer listening on the UDP socket.
            string cancel_wget_cmd("CWGET");
            g_AppendClientIPAndSessionID(cancel_wget_cmd);
            for (CNetServiceIterator it =
                    m_Impl->m_API->m_Service.ExcludeServer(server); it; ++it)
                (*it).ExecWithRetry(cancel_wget_cmd);

            // Also, if a new preferred affinity is given by
            // the server, register it with the rest of servers.
            m_Impl->ClaimNewPreferredAffinity(server, job.affinity);

            return true;
        }
    }

    return false;
}

bool CNetScheduleExecutor::GetJob(CNetScheduleJob& job,
        unsigned wait_time,
        const string& affinity_list)
{
    if (wait_time == 0)
        return GetJob(job, affinity_list);
    else {
        CDeadline deadline(wait_time, 0);

        return GetJob(job, affinity_list, &deadline);
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

    case CNetScheduleExecutor::eClaimNewPreferredAffs:
        cmd = "GET2 wnode_aff=1 any_aff=0 exclusive_new_aff=1";
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

string CNetScheduleNotificationHandler::CmdAppendTimeoutAndClientInfo(
        const string& base_cmd, const CDeadline* deadline)
{
    string cmd(base_cmd);

    if (deadline != NULL) {
        unsigned remaining_seconds = s_GetRemainingSeconds(*deadline);

        if (remaining_seconds > 0) {
            cmd += " port=";
            cmd += NStr::UIntToString(GetPort());

            cmd += " timeout=";
            cmd += NStr::UIntToString(remaining_seconds);
        }
    }

    g_AppendClientIPAndSessionID(cmd);

    return cmd;
}

bool CNetScheduleNotificationHandler::RequestJob(
        CNetScheduleExecutor::TInstance executor,
        CNetScheduleJob& job,
        const string& cmd)
{
    CGetJobCmdExecutor get_cmd_executor(cmd, job, executor);

    CNetServiceIterator it(executor->m_API->m_Service.FindServer(
            &get_cmd_executor, CNetService::eIncludePenalized));

    if (!it)
        return false;

    string cancel_wget_cmd("CWGET");
    g_AppendClientIPAndSessionID(cancel_wget_cmd);

    while (--it)
        (*it).ExecWithRetry(cancel_wget_cmd);

    return true;
}

#define GET2_NOTIF_ATTR_COUNT 2

bool CNetScheduleNotificationHandler::CheckRequestJobNotification(
        CNetScheduleExecutor::TInstance executor, CNetServer* server)
{
    static const char* const attr_names[GET2_NOTIF_ATTR_COUNT] =
        {"ns_node", "queue"};

    string attr_values[GET2_NOTIF_ATTR_COUNT];

    if (g_ParseNSOutput(m_Message,
            attr_names, attr_values, GET2_NOTIF_ATTR_COUNT) !=
                    GET2_NOTIF_ATTR_COUNT ||
            attr_values[1] != executor->m_API.GetQueueName())
        return false;

    SNetServerInPool* known_server; /* NCBI_FAKE_WARNING */

    {{
        CNetScheduleServerListener* listener = executor->m_API->GetListener();

        CFastMutexGuard guard(listener->m_ServerByNodeMutex);

        CNetScheduleServerListener::TServerByNode::iterator
            server_props_it(listener->m_ServerByNode.find(attr_values[0]));

        if (server_props_it == listener->m_ServerByNode.end())
            return false;

        known_server = server_props_it->second;
    }}

    *server = new SNetServerImpl(executor->m_API->m_Service, executor->
            m_API->m_Service->m_ServerPool->ReturnServer(known_server));

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

void SNetScheduleExecutorImpl::ExecWithOrWithoutRetry(const string& job_key,
        const string& cmd)
{
    CNetServer server(m_API->GetServer(job_key));

    if (!m_WorkerNodeMode)
        server.ExecWithRetry(cmd);
    else {
        CNetServer::SExecResult exec_result;

        server->ConnectAndExec(cmd, exec_result);
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

    g_AppendClientIPAndSessionID(cmd);

    m_Impl->ExecWithOrWithoutRetry(job.job_id, cmd);
}

void CNetScheduleExecutor::PutProgressMsg(const CNetScheduleJob& job)
{
    if (job.progress_msg.length() >= kNetScheduleMaxDBDataSize) {
        NCBI_THROW(CNetScheduleException, eDataTooLong,
                   "Progress message too long");
    }
    string cmd(g_MakeBaseCmd("MPUT", job.job_id));
    cmd += " \"";
    cmd += NStr::PrintableString(job.progress_msg);
    cmd += '\"';
    g_AppendClientIPAndSessionID(cmd);
    CNetServer::SExecResult exec_result;
    m_Impl->m_API->GetServer(job.job_id)->ConnectAndExec(cmd, exec_result);
}

void CNetScheduleExecutor::GetProgressMsg(CNetScheduleJob& job)
{
    m_Impl->m_API.GetProgressMsg(job);
}

void CNetScheduleExecutor::PutFailure(const CNetScheduleJob& job)
{
    s_CheckOutputSize(job.output,
        m_Impl->m_API->GetServerParams().max_output_size);

    if (job.error_msg.length() >= kNetScheduleMaxDBErrSize) {
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

    g_AppendClientIPAndSessionID(cmd);

    m_Impl->ExecWithOrWithoutRetry(job.job_id, cmd);
}

CNetScheduleAPI::EJobStatus CNetScheduleExecutor::GetJobStatus(
        const string& job_key, time_t* job_exptime)
{
    return m_Impl->m_API->GetJobStatus("WST2", job_key, job_exptime);
}

void CNetScheduleExecutor::ReturnJob(const string& job_key,
        const string& auth_token)
{
    string cmd("RETURN2 job_key=" + job_key);

    SNetScheduleAPIImpl::VerifyAuthTokenAlphabet(auth_token);
    cmd.append(" auth_token=");
    cmd.append(auth_token);

    g_AppendClientIPAndSessionID(cmd);

    m_Impl->ExecWithOrWithoutRetry(job_key, cmd);
}

int SNetScheduleExecutorImpl::AppendAffinityTokens(string& cmd,
        const vector<string>* affs,
        SNetScheduleExecutorImpl::EChangeAffAction action)
{
    if (affs == NULL || affs->empty())
        return 0;

    const char* sep = action == eAddAffs ? " add=\"" : " del=\"";

    ITERATE(vector<string>, aff, *affs) {
        cmd.append(sep);
        SNetScheduleAPIImpl::VerifyAffinityAlphabet(*aff);
        cmd.append(*aff);
        sep = ",";
    }
    cmd.push_back('"');

    CFastMutexGuard guard(m_PreferredAffMutex);

    if (action == eAddAffs)
        ITERATE(vector<string>, aff, *affs) {
            m_PreferredAffinities.insert(*aff);
        }
    else
        ITERATE(vector<string>, aff, *affs) {
            m_PreferredAffinities.erase(*aff);
        }

    return 1;
}

void CNetScheduleExecutor::ChangePreferredAffinities(
    const vector<string>* affs_to_add, const vector<string>* affs_to_delete)
{
    string cmd("CHAFF");

    if (m_Impl->AppendAffinityTokens(cmd, affs_to_add,
                    SNetScheduleExecutorImpl::eAddAffs) |
            m_Impl->AppendAffinityTokens(cmd, affs_to_delete,
                    SNetScheduleExecutorImpl::eDeleteAffs)) {
        g_AppendClientIPAndSessionID(cmd);

        m_Impl->m_API->m_Service.ExecOnAllServers(cmd);
    }
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

void CNetScheduleGETCmdListener::OnExec(
        CNetServerConnection::TInstance conn_impl, const string& /*cmd*/)
{
    switch (m_Executor->m_AffinityPreference) {
    case CNetScheduleExecutor::ePreferredAffsOrAnyJob:
    case CNetScheduleExecutor::ePreferredAffinities:
    case CNetScheduleExecutor::eClaimNewPreferredAffs:
        {
            CNetServerConnection conn(conn_impl);

            CNetScheduleServerListener* listener =
                    m_Executor->m_API->GetListener();

            if (listener->NeedToSubmitAffinities(conn_impl->m_Server)) {
                conn.Exec(m_Executor->MkSETAFFCmd());
                listener->SetAffinitiesSynced(conn_impl->m_Server, true);
            }
        }
        break;

    default:
        // Preferred affinities are not used -- there's no need
        // to send the SETAFF command to the server.
        break;
    }
}

END_NCBI_SCOPE
