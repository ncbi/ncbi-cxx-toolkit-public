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

#include <connect/services/netschedule_api.hpp>


BEGIN_NCBI_SCOPE

void CNetScheduleAdmin::SwitchToDrainMode(ESwitch on_off)
{
    string cmd(on_off != eOff ?
            "REFUSESUBMITS mode=1" : "REFUSESUBMITS mode=0");
    g_AppendClientIPSessionIDHitID(cmd);
    m_Impl->m_API->m_Service.ExecOnAllServers(cmd);
}

void CNetScheduleAdmin::ShutdownServer(
    CNetScheduleAdmin::EShutdownLevel level)
{
    string cmd(level == eDie ? "SHUTDOWN SUICIDE" :
            level == eShutdownImmediate ? "SHUTDOWN IMMEDIATE" :
                    level == eDrain ? "SHUTDOWN drain=1" : "SHUTDOWN");
    g_AppendClientIPSessionIDHitID(cmd);
    m_Impl->m_API->m_Service.ExecOnAllServers(cmd);
}


void CNetScheduleAdmin::ReloadServerConfig()
{
    string cmd("RECO");
    g_AppendClientIPSessionIDHitID(cmd);
    m_Impl->m_API->m_Service.ExecOnAllServers(cmd);
}

void CNetScheduleAdmin::CreateQueue(const string& qname, const string& qclass,
                                    const string& description)
{
    SNetScheduleAPIImpl::VerifyQueueNameAlphabet(qname);

    string cmd = "QCRE " + qname;
    cmd += ' ';
    cmd += qclass;

    if (!description.empty()) {
        cmd += " \"";
        cmd += description;
        cmd += '"';
    }

    g_AppendClientIPSessionIDHitID(cmd);

    m_Impl->m_API->m_Service.ExecOnAllServers(cmd);
}

void CNetScheduleAdmin::DeleteQueue(const string& qname)
{
    SNetScheduleAPIImpl::VerifyQueueNameAlphabet(qname);

    string cmd("QDEL " + qname);
    g_AppendClientIPSessionIDHitID(cmd);
    m_Impl->m_API->m_Service.ExecOnAllServers(cmd);
}

void CNetScheduleAdmin::DumpJob(CNcbiOstream& out, const string& job_key)
{
    CNetServerMultilineCmdOutput output(DumpJob(job_key));

    string line;

    while (output.ReadLine(line))
        out << line << "\n";
}

CNetServerMultilineCmdOutput CNetScheduleAdmin::DumpJob(const string& job_key)
{
    string cmd("DUMP " + job_key);
    g_AppendClientIPSessionIDHitID(cmd);
    return m_Impl->m_API->GetServer(job_key).ExecWithRetry(cmd, true);
}

void CNetScheduleAdmin::CancelAllJobs(const string& job_statuses)
{
    string cmd;
    if (job_statuses.empty()) {
        cmd.assign("CANCELQ");
    } else {
        cmd.assign("CANCEL status=");
        cmd.append(job_statuses);
    }
    g_AppendClientIPSessionIDHitID(cmd);
    m_Impl->m_API->m_Service.ExecOnAllServers(cmd);
}


void CNetScheduleAdmin::PrintServerVersion(CNcbiOstream& output_stream)
{
    string cmd("VERSION");
    g_AppendClientIPSessionIDHitID(cmd);
    m_Impl->m_API->m_Service.PrintCmdOutput(cmd,
        output_stream, CNetService::eSingleLineOutput);
}


void CNetScheduleAdmin::DumpQueue(
        CNcbiOstream& output_stream,
        const string& start_after_job,
        size_t job_count,
        const string& job_statuses,
        const string& job_group)
{
    string cmd("DUMP");
    if (!job_statuses.empty()) {
        cmd.append(" status=");
        cmd.append(job_statuses);
    }
    if (!start_after_job.empty()) {
        cmd.append(" start_after=");
        cmd.append(start_after_job);
    }
    if (job_count > 0) {
        cmd.append(" count=");
        cmd.append(NStr::NumericToString(job_count));
    }
    if (!job_group.empty()) {
        SNetScheduleAPIImpl::VerifyJobGroupAlphabet(job_group);
        cmd.append(" group=");
        cmd.append(job_group);
    }
    g_AppendClientIPSessionIDHitID(cmd);
    m_Impl->m_API->m_Service.PrintCmdOutput(cmd,
        output_stream, CNetService::eMultilineOutput);
}

void CNetScheduleAdmin::DumpQueue(
        CNcbiOstream& output_stream,
        const string& start_after_job,
        size_t job_count,
        CNetScheduleAPI::EJobStatus status,
        const string& job_group)
{
    string job_statuses(CNetScheduleAPI::StatusToString(status));

    // Must be a rare case
    if (status == CNetScheduleAPI::eJobNotFound) {
        job_statuses.clear();
    }

    DumpQueue(output_stream, start_after_job, job_count, job_statuses, job_group);
}


static string s_MkQINFCmd(const string& queue_name)
{
    string qinf_cmd("QINF2 " + queue_name);
    g_AppendClientIPSessionIDHitID(qinf_cmd);
    return qinf_cmd;
}

static void s_ParseQueueInfo(const string& server_output,
        CNetScheduleAdmin::TQueueInfo& queue_info)
{
    CUrlArgs url_parser(server_output);

    ITERATE(CUrlArgs::TArgs, field, url_parser.GetArgs()) {
        queue_info[field->name] = field->value;
    }
}

void CNetScheduleAdmin::GetQueueInfo(CNetServer server,
        const string& queue_name, CNetScheduleAdmin::TQueueInfo& queue_info)
{
    CNetServer::SExecResult exec_result;

    server->ConnectAndExec(s_MkQINFCmd(queue_name), false, exec_result);

    s_ParseQueueInfo(exec_result.response, queue_info);
}

void CNetScheduleAdmin::GetQueueInfo(const string& queue_name,
        CNetScheduleAdmin::TQueueInfo& queue_info)
{
    GetQueueInfo(m_Impl->m_API->m_Service.Iterate().GetServer(),
            queue_name, queue_info);
}

void CNetScheduleAdmin::GetQueueInfo(CNetServer server,
        CNetScheduleAdmin::TQueueInfo& queue_info)
{
    GetQueueInfo(server, m_Impl->m_API->m_Queue, queue_info);
}

void CNetScheduleAdmin::GetQueueInfo(CNetScheduleAdmin::TQueueInfo& queue_info)
{
    GetQueueInfo(m_Impl->m_API->m_Queue, queue_info);
}

void CNetScheduleAdmin::PrintQueueInfo(const string& queue_name,
        CNcbiOstream& output_stream)
{
    bool print_headers = m_Impl->m_API->m_Service.IsLoadBalanced();

    for (CNetServiceIterator it = m_Impl->m_API->m_Service.Iterate(
            CNetService::eIncludePenalized); it; ++it) {
        if (print_headers)
            output_stream << '[' << (*it).GetServerAddress() << ']' << NcbiEndl;

        TQueueInfo queue_info;

        GetQueueInfo(*it, queue_name, queue_info);

        ITERATE(TQueueInfo, qi, queue_info) {
            output_stream << qi->first << ": " << qi->second << NcbiEndl;
        }

        if (print_headers)
            output_stream << NcbiEndl;
    }
}

void CNetScheduleAdmin::GetWorkerNodes(list<SWorkerNodeInfo>& worker_nodes)
{
    worker_nodes.clear();

    set<string> unique_sessions;

    for (CNetServiceIterator iter = m_Impl->m_API->m_Service.Iterate(
            CNetService::eIncludePenalized); iter; ++iter) {

        CJsonNode client_info_array = g_GenericStatToJson(*iter,
                eNetScheduleStatClients, /*verbose*/ false);

        for (CJsonIterator client_info_iter = client_info_array.Iterate();
                client_info_iter; ++client_info_iter) {
            CJsonNode client_info = *client_info_iter;
            if (NStr::Find(client_info.GetString("type"),
                    "worker node") == NPOS)
                continue;

            string session = client_info.GetString("session");

            if (unique_sessions.insert(session).second) {
                worker_nodes.push_back(SWorkerNodeInfo());

                SWorkerNodeInfo& wn_info = worker_nodes.back();
                wn_info.node = client_info.GetString("client_node");
                wn_info.session = session;
                wn_info.host = client_info.GetString("client_host");
                wn_info.port =
                        client_info.GetInteger("worker_node_control_port");
                wn_info.last_access = CTime(
                        client_info.GetString("last_access"), "M/D/Y h:m:s.r");
            }
        }
    }
}

CJsonNode CNetScheduleAdmin::GetWorkerNodeInfo()
{
    CJsonNode result(CJsonNode::NewObjectNode());

    list<SWorkerNodeInfo> worker_nodes;
    GetWorkerNodes(worker_nodes);

    ITERATE(list<CNetScheduleAdmin::SWorkerNodeInfo>, wn_info, worker_nodes) {
        CNetScheduleAPI wn_api(wn_info->host + ':' +
                NStr::NumericToString(wn_info->port),
                m_Impl->m_API->m_Service->GetClientName(),
                kEmptyStr);

        result.SetByKey(wn_info->session, g_WorkerNodeInfoToJson(
                wn_api.GetService().Iterate().GetServer()));
    }

    return result;
}

void CNetScheduleAdmin::PrintConf(CNcbiOstream& output_stream)
{
    string cmd("GETCONF");
    g_AppendClientIPSessionIDHitID(cmd);
    m_Impl->m_API->m_Service.PrintCmdOutput(cmd,
        output_stream, CNetService::eMultilineOutput);
}

void CNetScheduleAdmin::PrintServerStatistics(CNcbiOstream& output_stream,
    EStatisticsOptions opt)
{
    string cmd(opt == eStatisticsBrief ? "STAT" :
            opt == eStatisticsClients ? "STAT CLIENTS" : "STAT ALL");
    g_AppendClientIPSessionIDHitID(cmd);
    m_Impl->m_API->m_Service.PrintCmdOutput(cmd,
        output_stream, CNetService::eMultilineOutput_NetCacheStyle);
}

void CNetScheduleAdmin::PrintHealth(CNcbiOstream& output_stream)
{
    string cmd("HEALTH");
    g_AppendClientIPSessionIDHitID(cmd);
    m_Impl->m_API->m_Service.PrintCmdOutput(cmd,
            output_stream, CNetService::eUrlEncodedOutput);
}

void CNetScheduleAdmin::GetQueueList(TQueueList& qlist)
{
    string cmd("STAT QUEUES");
    g_AppendClientIPSessionIDHitID(cmd);

    string output_line;

    for (CNetServiceIterator it = m_Impl->m_API->m_Service.Iterate(
            CNetService::eIncludePenalized); it; ++it) {
        CNetServer server = *it;

        qlist.push_back(SServerQueueList(server));

        CNetServerMultilineCmdOutput cmd_output((*it).ExecWithRetry(cmd, true));
        while (cmd_output.ReadLine(output_line))
            if (NStr::StartsWith(output_line, "[queue ") &&
                    output_line.length() > sizeof("[queue "))
                qlist.back().queues.push_back(output_line.substr(
                        sizeof("[queue ") - 1,
                        output_line.length() - sizeof("[queue ")));
    }
}

void CNetScheduleAdmin::StatusSnapshot(
        CNetScheduleAdmin::TStatusMap& status_map,
        const string& affinity_token,
        const string& job_group)
{
    string cmd = "STAT JOBS";

    if (!affinity_token.empty()) {
        SNetScheduleAPIImpl::VerifyAffinityAlphabet(affinity_token);
        cmd.append(" aff=");
        cmd.append(affinity_token);
    }

    if (!job_group.empty()) {
        SNetScheduleAPIImpl::VerifyJobGroupAlphabet(job_group);
        cmd.append(" group=");
        cmd.append(job_group);
    }

    g_AppendClientIPSessionIDHitID(cmd);

    string output_line;
    CTempString st_str, cnt_str;

    try {
        for (CNetServiceIterator it = m_Impl->m_API->m_Service.Iterate(
                CNetService::eIncludePenalized); it; ++it) {
            CNetServerMultilineCmdOutput cmd_output(
                    (*it).ExecWithRetry(cmd, true));

            while (cmd_output.ReadLine(output_line))
                if (NStr::SplitInTwo(output_line, ":", st_str, cnt_str))
                    status_map[st_str] +=
                        NStr::StringToUInt(NStr::TruncateSpaces_Unsafe
                                           (cnt_str, NStr::eTrunc_Begin));
        }
    }
    catch (CStringException& ex)
    {
        NCBI_RETHROW(ex, CNetScheduleException, eProtocolSyntaxError,
                "Error while parsing STAT JOBS response");
    }
}

struct {
    const char* command;
    const char* record_prefix;
    const char* entity_name;
} static const s_StatTopics[eNumberOfNetStheduleStatTopics] = {
    // eNetScheduleStatJobGroups
    {"STAT GROUPS", "GROUP: ", "group"},
    // eNetScheduleStatClients
    {"STAT CLIENTS", "CLIENT: ", "client_node"},
    // eNetScheduleStatNotifications
    {"STAT NOTIFICATIONS", "CLIENT: ", "client_node"},
    // eNetScheduleStatAffinities
    {"STAT AFFINITIES", "AFFINITY: ", "affinity_token"}
};

string g_UnquoteIfQuoted(const CTempString& str)
{
    switch (str[0]) {
    case '"':
    case '\'':
        return NStr::ParseQuoted(str);
    default:
        return str;
    }
}

string g_GetNetScheduleStatCommand(ENetScheduleStatTopic topic)
{
    return s_StatTopics[topic].command;
}

static void NormalizeStatKeyName(CTempString& key)
{
    char* begin = const_cast<char*>(key.data());
    char* end = begin + key.length();

    while (begin < end && !isalnum(*begin))
        ++begin;

    while (begin < end && !isalnum(end[-1]))
        --end;

    if (begin == end) {
        key = "_";
        return;
    }

    key.assign(begin, end - begin);

    for (; begin < end; ++begin)
        *begin = isalnum(*begin) ? tolower(*begin) : '_';
}

bool g_FixMisplacedPID(CJsonNode& stat_info, CTempString& executable_path,
        const char* pid_key)
{
    SIZE_TYPE misplaced_pid = NStr::Find(executable_path, "; PID: ");
    if (misplaced_pid == NPOS)
        return false;

    SIZE_TYPE pos = misplaced_pid + sizeof("; PID: ") - 1;
    stat_info.SetInteger(pid_key, NStr::StringToInt8(
            CTempString(executable_path.data() + pos,
                    executable_path.length() - pos)));
    executable_path.erase(misplaced_pid);
    return true;
}

CJsonNode g_GenericStatToJson(CNetServer server,
        ENetScheduleStatTopic topic, bool verbose)
{
    string stat_cmd(s_StatTopics[topic].command);
    CTempString prefix(s_StatTopics[topic].record_prefix);
    CTempString entity_name(s_StatTopics[topic].entity_name);

    if (verbose)
        stat_cmd.append(" VERBOSE");

    g_AppendClientIPSessionIDHitID(stat_cmd);

    CNetServerMultilineCmdOutput output(server.ExecWithRetry(stat_cmd, true));

    CJsonNode entities(CJsonNode::NewArrayNode());
    CJsonNode entity_info;
    CJsonNode array_value;

    string line;

    while (output.ReadLine(line)) {
        if (NStr::StartsWith(line, prefix)) {
            if (entity_info)
                entities.Append(entity_info);
            entity_info = CJsonNode::NewObjectNode();
            entity_info.SetString(entity_name, g_UnquoteIfQuoted(
                    CTempString(line.data() + prefix.length(),
                    line.length() - prefix.length())));
        } else if (entity_info && NStr::StartsWith(line, "  ")) {
            if (NStr::StartsWith(line, "    ") && array_value) {
                array_value.AppendString(g_UnquoteIfQuoted(
                        NStr::TruncateSpaces(line, NStr::eTrunc_Begin)));
            } else {
                if (array_value)
                    array_value = NULL;
                CTempString key, value;
                NStr::SplitInTwo(line, ":", key, value);
                NormalizeStatKeyName(key);
                string key_norm(key);
                value = NStr::TruncateSpaces_Unsafe(value, NStr::eTrunc_Begin);
                if (value.empty())
                    entity_info.SetByKey(key_norm, array_value =
                            CJsonNode::NewArrayNode());
                else
                    entity_info.SetByKey(key_norm, CJsonNode::GuessType(value));
            }
        }
    }
    if (entity_info)
        entities.Append(entity_info);

    return entities;
}

CJsonNode g_ServerInfoToJson(CNetServerInfo server_info,
        bool server_version_key)
{
    CJsonNode server_info_node(CJsonNode::NewObjectNode());

    string attr_name, attr_value;

    ESwitch old_format = eDefault;

    while (server_info.GetNextAttribute(attr_name, attr_value)) {
        switch (old_format) {
        case eOn:
            if (attr_name == "Build")
                attr_name = "build_date";
            else
                NStr::ReplaceInPlace(NStr::ToLower(attr_name), " ", "_");
            break;

        case eDefault:
            if (NStr::EndsWith(attr_name, " version"))
            {
                old_format = eOn;
                attr_name = server_version_key ? "server_version" : "version";
                break;
            } else
                old_format = eOff;
            /* FALL THROUGH */

        case eOff:
            if (server_version_key && attr_name == "version")
                attr_name = "server_version";
        }

        server_info_node.SetString(attr_name, attr_value);
    }

    return server_info_node;
}

static bool s_ExtractKey(const CTempString& line,
        string& key, CTempString& value)
{
    key.erase(0, key.length());
    SIZE_TYPE line_len = line.length();
    key.reserve(line_len);

    for (SIZE_TYPE i = 0; i < line_len; ++i)
    {
        char c = line[i];
        if (isalnum(c))
            key += tolower(c);
        else if (c == ' ' || c == '_' || c == '-')
            key += '_';
        else if (c != ':' || key.empty())
            break;
        else {
            if (++i < line_len && line[i] == ' ')
                ++i;
            value.assign(line, i, line_len - i);
            return true;
        }
    }
    return false;
}

static CJsonNode s_WordsToJsonArray(const CTempString& str)
{
    CJsonNode array_node(CJsonNode::NewArrayNode());
    list<CTempString> words;
    NStr::Split(str, TEMP_STRING_CTOR(" "), words,
            NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
    ITERATE(list<CTempString>, it, words) {
        array_node.AppendString(*it);
    }
    return array_node;
}

inline static CNetServerInfo s_ServerInfoFromString(const string& server_info)
{
    return new SNetServerInfoImpl(server_info);
}

CJsonNode g_WorkerNodeInfoToJson(CNetServer worker_node)
{
    CNetServerMultilineCmdOutput output(
            worker_node.ExecWithRetry("STAT", true));

    CJsonNode wn_info(CJsonNode::NewObjectNode());

    string line;
    string key;
    CTempString value;
    CJsonNode job_counters(CJsonNode::NewObjectNode());
    CJsonNode running_jobs(CJsonNode::NewArrayNode());
    SIZE_TYPE pos;
    int running_job_count = 0;
    int free_worker_threads = 1;
    bool suspended = false;
    bool shutting_down = false;
    bool exclusive_job = false;
    bool working = false;

    while (output.ReadLine(line)) {
        if (line.empty() || isspace(line[0]))
            continue;

        if (running_job_count > 0) {
            if ((pos = NStr::Find(CTempString(line),
                            TEMP_STRING_CTOR("running for "),
                            NStr::eNocase, NStr::eReverseSearch)) != NPOS) {
                --running_job_count;

                pos += sizeof("running for ") - 1;
                Uint8 run_time = NStr::StringToUInt8(
                        CTempString(line.data() + pos, line.length() - pos),
                                NStr::fConvErr_NoThrow |
                                NStr::fAllowLeadingSpaces |
                                NStr::fAllowTrailingSymbols);

                if ((pos = NStr::Find(line, TEMP_STRING_CTOR(" "))) != NPOS) {
                    CJsonNode running_job(CJsonNode::NewObjectNode());

                    running_job.SetString("key", CTempString(line.data(), pos));
                    running_job.SetInteger("run_time", (Int8) run_time);
                    running_jobs.Append(running_job);
                }
            }
        } else if (NStr::StartsWith(line, TEMP_STRING_CTOR("Jobs "))) {
            if (s_ExtractKey(CTempString(line.data() + sizeof("Jobs ") - 1,
                    line.length() - (sizeof("Jobs ") - 1)), key, value)) {
                job_counters.SetInteger(key, NStr::StringToInt8(value));
                if (key == "running") {
                    running_job_count = NStr::StringToInt(value,
                            NStr::fConvErr_NoThrow |
                            NStr::fAllowLeadingSpaces |
                            NStr::fAllowTrailingSymbols);
                    working = running_job_count > 0;
                    free_worker_threads -= running_job_count;
                }
            }
        } else if (s_ExtractKey(line, key, value)) {
            if (key == "host_name")
                key = "hostname";
            else if (key == "node_started_at")
                key = "started";
            else if (key == "executable_path")
                g_FixMisplacedPID(wn_info, value, "pid");
            else if (key == "maximum_job_threads") {
                int maximum_job_threads = NStr::StringToInt(value,
                            NStr::fConvErr_NoThrow |
                            NStr::fAllowLeadingSpaces |
                            NStr::fAllowTrailingSymbols);
                free_worker_threads += maximum_job_threads - 1;
                wn_info.SetInteger(key, maximum_job_threads);
                continue;
            } else if (key == "netschedule_servers") {
                if (value != "N/A")
                    wn_info.SetByKey(key, s_WordsToJsonArray(value));
                else
                    wn_info.SetByKey(key, CJsonNode::NewArrayNode());
                continue;
            } else if (key == "preferred_affinities") {
                wn_info.SetByKey(key, s_WordsToJsonArray(value));
                continue;
            }
            wn_info.SetByKey(key, CJsonNode::GuessType(value));
        } else if (wn_info.GetSize() == 0)
            wn_info = g_ServerInfoToJson(s_ServerInfoFromString(line), false);
        else if (NStr::Find(line, TEMP_STRING_CTOR("suspended")) != NPOS)
            suspended = true;
        else if (NStr::Find(line, TEMP_STRING_CTOR("shutting down")) != NPOS)
            shutting_down = true;
        else if (NStr::Find(line, TEMP_STRING_CTOR("exclusive job")) != NPOS)
            exclusive_job = true;
    }

    if (!wn_info.HasKey("maximum_job_threads"))
        wn_info.SetInteger("maximum_job_threads", 1);
    if (!wn_info.HasKey("version"))
        wn_info.SetString("version", kEmptyStr);
    if (!wn_info.HasKey("build_date"))
        wn_info.SetString("build_date", kEmptyStr);

    wn_info.SetString("status",
            shutting_down ? "shutting_down" :
            suspended ? "suspended" :
            exclusive_job ? "processing_exclusive_job" :
            free_worker_threads == 0 ? "busy" :
            working ? "working" : "ready");

    wn_info.SetByKey("job_counters", job_counters);
    wn_info.SetByKey("running_jobs", running_jobs);

    return wn_info;
}

END_NCBI_SCOPE
