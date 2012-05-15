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
    CUrlArgs url_parser(response);
    ITERATE(CUrlArgs::TArgs, field, url_parser.GetArgs()) {
        switch (field->name[0]) {
        case 'j':
            if (field->name == "job_key")
                job.job_id = field->value;
            break;

        case 'i':
            if (field->name == "input")
                job.input = field->value;
            break;

        case 'a':
            if (field->name == "affinity")
                job.affinity = field->value;
            else if (field->name == "auth_token")
                job.auth_token = field->value;
            break;

        case 'c':
            if (field->name == "client_ip")
                job.client_ip = field->value;
            else if (field->name == "client_sid")
                job.session_id = field->value;
            break;

        case 'm':
            if (field->name == "mask")
                job.mask = atoi(field->value.c_str());
        }
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

static bool s_ParseGetJobResponse(CNetScheduleJob& job, const string& response)
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
    m_Impl->m_API->x_SendJobCmdWaitResponse("JDEX" , job_key, runtime_inc);
}

class CGetJobCmdExecutor : public INetServerFinder
{
public:
    CGetJobCmdExecutor(const string& cmd,
            CNetScheduleJob& job, SNetScheduleExecutorImpl* executor) :
        m_Cmd(cmd), m_Job(job), m_Executor(executor)
    {
    }

    virtual bool Consider(CNetServer server);

private:
    const string& m_Cmd;
    CNetScheduleJob& m_Job;
    SNetScheduleExecutorImpl* m_Executor;
};

bool CGetJobCmdExecutor::Consider(CNetServer server)
{
    try {
        return s_ParseGetJobResponse(m_Job,
                server.ExecWithRetry(m_Cmd).response);
    }
    catch (CNetScheduleException& e) {
        if (e.GetErrCode() != CNetScheduleException::ePrefAffExpired)
            throw;

        CFastMutexGuard guard(m_Executor->m_PreferredAffMutex);

        if (!m_Executor->m_PreferredAffinities.empty()) {
            string cmd;
            const char* sep = "CHAFF add=";
            ITERATE(set<string>, it, m_Executor->m_PreferredAffinities) {
                cmd.append(sep);
                cmd.append(*it);
                sep = ",";
            }
            server.ExecWithRetry(cmd);
        }
    }
    return false;
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

void CNetScheduleExecutor::SetAffinityPreference(
        CNetScheduleExecutor::EJobAffinityPreference aff_pref)
{
    m_Impl->m_AffinityPreference = aff_pref;
}

bool CNetScheduleExecutor::GetJob(CNetScheduleJob& job,
        const string& affinity_list,
        CAbsTimeout* timeout)
{
    string base_cmd(CNetScheduleNotificationHandler::MkBaseGETCmd(m_Impl,
            affinity_list));

    if (m_Impl->m_NotificationHandler.RequestJob(m_Impl, job,
            m_Impl->m_NotificationHandler.CmdAppendTimeout(base_cmd,
                    timeout)))
        return true;

    if (timeout == NULL)
        return false;

    while (m_Impl->m_NotificationHandler.WaitForNotification(*timeout)) {
        CNetServer server;
        if (m_Impl->m_NotificationHandler.CheckRequestJobNotification(m_Impl,
                &server) && s_ParseGetJobResponse(job, server.ExecWithRetry(
                    m_Impl->m_NotificationHandler.CmdAppendTimeout(base_cmd,
                        timeout)).response)) {
            // Notify the rest of NetSchedule servers that
            // we are no longer listening on the UDP socket.
            // Also, if a new preferred affinity is given by
            // the server, register it with the rest of servers.
            string new_preferred_aff_cmd;

            if (m_Impl->m_AffinityPreference == eClaimNewPreferredAffs &&
                    !job.affinity.empty()) {
                CFastMutexGuard guard(m_Impl->m_PreferredAffMutex);

                if (m_Impl->m_PreferredAffinities.find(job.affinity) ==
                        m_Impl->m_PreferredAffinities.end()) {
                    m_Impl->m_PreferredAffinities.insert(job.affinity);
                    new_preferred_aff_cmd = "CHAFF add=" + job.affinity;
                }
            }

            CNetServiceIterator it(m_Impl->m_API->m_Service.Iterate(server));
            while (++it) {
                (*it).ExecWithRetry("CWGET");
                if (!new_preferred_aff_cmd.empty())
                    (*it).ExecWithRetry(new_preferred_aff_cmd);
            }

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
        CAbsTimeout abs_timeout(wait_time, 0);

        return GetJob(job, affinity_list, &abs_timeout);
    }
}

string CNetScheduleNotificationHandler::MkBaseGETCmd(
    CNetScheduleExecutor::TInstance executor,
    const string& affinity_list)
{
    string cmd;

    switch (executor->m_AffinityPreference) {
    case CNetScheduleExecutor::eClaimNewPreferredAffs:
        cmd = !executor->m_PreferredAffinities.empty() ?
                "GET2 wnode_aff=1 any_aff=0 exclusive_new_aff=1" :
                "GET2 wnode_aff=0 any_aff=0 exclusive_new_aff=1";
        break;

    case CNetScheduleExecutor::ePreferredAffsOrAnyJob:
        if (!executor->m_PreferredAffinities.empty()) {
            cmd = "GET2 wnode_aff=1 any_aff=1";
            break;
        }
        /* FALL THROUGH */

    case CNetScheduleExecutor::eAnyJob:
        cmd = "GET2 wnode_aff=0 any_aff=1";
        break;

    case CNetScheduleExecutor::ePreferredAffinities:
        if (!executor->m_PreferredAffinities.empty()) {
            cmd = "GET2 wnode_aff=1 any_aff=0";
            break;
        }
        /* FALL THROUGH */

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

string CNetScheduleNotificationHandler::CmdAppendTimeout(
        const string& base_cmd, CAbsTimeout* timeout)
{
    if (timeout == NULL)
        return base_cmd;

    string cmd(base_cmd);

    cmd += " port=";
    cmd += NStr::UIntToString(GetPort());

    cmd += " timeout=";
    cmd += NStr::UIntToString(s_GetRemainingSeconds(*timeout));

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
                    SNetScheduleExecutorImpl::eDeleteAffs))
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
