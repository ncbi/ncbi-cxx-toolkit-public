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

using namespace grid::netschedule;

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
    const auto die = level == eDie;
    string cmd(die ? "SHUTDOWN SUICIDE" :
            level == eShutdownImmediate ? "SHUTDOWN IMMEDIATE" :
                    level == eDrain ? "SHUTDOWN drain=1" : "SHUTDOWN");
    g_AppendClientIPSessionIDHitID(cmd);

    const auto retry_mode = die ? SNetServiceImpl::SRetry::eNoRetry : SNetServiceImpl::SRetry::eDefault;
    auto retry_guard = m_Impl->m_API->m_Service->CreateRetryGuard(retry_mode);

    try {
        m_Impl->m_API->m_Service.ExecOnAllServers(cmd);
    }
    catch (CNetSrvConnException& ex)
    {
        if ((ex.GetErrCode() != CNetSrvConnException::eConnClosedByServer) || !die) throw;
    }
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
    limits::Check<limits::SQueueName>(qname);

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
    limits::Check<limits::SQueueName>(qname);

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
        limits::Check<limits::SJobGroup>(job_group);
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

void g_GetWorkerNodes(CNetScheduleAPI api, list<CNetScheduleAdmin::SWorkerNodeInfo>& worker_nodes);

void CNetScheduleAdmin::GetWorkerNodes(
    list<SWorkerNodeInfo>& worker_nodes)
{
    g_GetWorkerNodes(m_Impl->m_API, worker_nodes);
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
        limits::Check<limits::SAffinity>(affinity_token);
        cmd.append(" aff=");
        cmd.append(affinity_token);
    }

    if (!job_group.empty()) {
        limits::Check<limits::SJobGroup>(job_group);
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

END_NCBI_SCOPE
