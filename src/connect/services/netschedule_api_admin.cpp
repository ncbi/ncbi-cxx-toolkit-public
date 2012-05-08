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
    m_Impl->m_API->m_Service.ExecOnAllServers(on_off != eOff ?
            "REFUSESUBMITS mode=1" : "REFUSESUBMITS mode=0");
}

void CNetScheduleAdmin::ShutdownServer(
    CNetScheduleAdmin::EShutdownLevel level)
{
    m_Impl->m_API->m_Service.ExecOnAllServers(
            level == eDie ? "SHUTDOWN SUICIDE" :
                    level == eShutdownImmediate ? "SHUTDOWN IMMEDIATE" :
                            level == eDrain ? "SHUTDOWN drain=1" : "SHUTDOWN");
}


void CNetScheduleAdmin::ReloadServerConfig()
{
    m_Impl->m_API->m_Service.ExecOnAllServers("RECO");
}

void CNetScheduleAdmin::CreateQueue(const string& qname, const string& qclass,
                                    const string& comment)
{
    string cmd = "QCRE " + qname;
    cmd += ' ';
    cmd += qclass;

    if (!comment.empty()) {
        cmd += " \"";
        cmd += comment;
        cmd += '"';
    }

    m_Impl->m_API->m_Service.ExecOnAllServers(cmd);
}


void CNetScheduleAdmin::DeleteQueue(const string& qname)
{
    m_Impl->m_API->m_Service.ExecOnAllServers("QDEL " + qname);
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
    return m_Impl->m_API->GetServer(job_key).ExecWithRetry("DUMP " + job_key);
}

void CNetScheduleAdmin::CancelAllJobs()
{
    m_Impl->m_API->m_Service.ExecOnAllServers("CANCELQ");
}


void CNetScheduleAdmin::PrintServerVersion(CNcbiOstream& output_stream)
{
    m_Impl->m_API->m_Service.PrintCmdOutput("VERSION",
        output_stream, CNetService::eSingleLineOutput);
}


void CNetScheduleAdmin::DumpQueue(
        CNcbiOstream& output_stream,
        const string& start_after_job,
        size_t job_count,
        CNetScheduleAPI::EJobStatus status,
        const string& job_group)
{
    string cmd("DUMP");
    if (status != CNetScheduleAPI::eJobNotFound) {
        cmd.append(" status=");
        cmd.append(CNetScheduleAPI::StatusToString(status));
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
    m_Impl->m_API->m_Service.PrintCmdOutput(cmd,
        output_stream, CNetService::eMultilineOutput);
}


void CNetScheduleAdmin::PrintQueueInfo(CNcbiOstream& output_stream)
{
    CTempString queue_info, dyn_queue, model_queue;

    string cmd("QINF " + m_Impl->m_API->m_Queue);

    bool print_headers = m_Impl->m_API->m_Service.IsLoadBalanced();

    for (CNetServiceIterator it =
            m_Impl->m_API->m_Service.Iterate(); it; ++it) {
        if (print_headers)
            output_stream << '[' << (*it).GetServerAddress() << ']' << NcbiEndl;

        string cmd_output((*it).ExecWithRetry(cmd).response);

        NStr::SplitInTwo(cmd_output, "\t", queue_info, dyn_queue);
        switch (queue_info[0]) {
        case '0':
            output_stream << "queue type: static" << NcbiEndl;
            break;
        case '1':
            output_stream << "queue_type: dynamic" << NcbiEndl;

            if (NStr::SplitInTwo(dyn_queue, "\t", model_queue, queue_info))
                output_stream <<
                        "model_queue: " << model_queue << NcbiEndl <<
                        "description: " <<
                                NStr::ParseQuoted(queue_info) << NcbiEndl;
        }

        CNetServer::SExecResult exec_result((*it).ExecWithRetry("GETC"));

        CNetServerMultilineCmdOutput output(exec_result);

        string line;
        static const string eq("=");
        static const string field_sep(": ");

        while (output.ReadLine(line)) {
            NStr::ReplaceInPlace(line, eq, field_sep, 0, 1);
            output_stream << line << NcbiEndl;
        }

        if (print_headers)
            output_stream << NcbiEndl;
    }
}

unsigned CNetScheduleAdmin::CountActiveJobs()
{
    static const string cmd("ACNT");

    unsigned counter = 0;

    for (CNetServiceIterator it = m_Impl->m_API->m_Service.Iterate(); it; ++it)
        counter += NStr::StringToUInt((*it).ExecWithRetry(cmd).response);

    return counter;
}

void CNetScheduleAdmin::GetWorkerNodes(
    list<SWorkerNodeInfo>& worker_nodes)
{
    worker_nodes.clear();

    set<pair<string, unsigned short> > m_Unique;

    for (CNetServiceIterator it =
            m_Impl->m_API->m_Service.Iterate(); it; ++it) {
        CNetServer::SExecResult exec_result((*it).ExecWithRetry("STAT"));
        CNetServerMultilineCmdOutput output(exec_result);

        bool nodes_info = false;
        string response;

        while (output.ReadLine(response)) {
            if (NStr::StartsWith(response, "[Configured"))
                break;
            if (!nodes_info) {
                if (NStr::Compare(response, "[Worker node statistics]:") == 0)
                    nodes_info = true;
                else
                    continue;
            } else {
                if (response.empty())
                    continue;

                string name;
                NStr::SplitInTwo(response, " ", name, response);
                NStr::TruncateSpacesInPlace(response);
                string prog;
                NStr::SplitInTwo(response, "@", prog, response);
                NStr::TruncateSpacesInPlace(response);
                prog = prog.substr(6,prog.size()-8);
                string host;
                NStr::SplitInTwo(response, " ", host, response);

                if (NStr::Compare(host, "localhost") == 0)
                    host = g_NetService_gethostname((*it).GetHost());

                NStr::TruncateSpacesInPlace(response);

                string sport, stime;

                NStr::SplitInTwo(response, " ", sport, stime);
                NStr::SplitInTwo(sport, ":", response, sport);

                unsigned short port = (unsigned short) NStr::StringToInt(sport);

                if (m_Unique.insert(make_pair(host,port)).second) {
                    worker_nodes.push_back(SWorkerNodeInfo());

                    SWorkerNodeInfo& wn_info = worker_nodes.back();
                    wn_info.name = name;
                    wn_info.prog = prog;
                    wn_info.host = host;
                    wn_info.port = port;

                    NStr::TruncateSpacesInPlace(stime);
                    wn_info.last_access = CTime(stime, "M/D/Y h:m:s");
                }
            }
        }
    }
}

void CNetScheduleAdmin::PrintConf(CNcbiOstream& output_stream)
{
    m_Impl->m_API->m_Service.PrintCmdOutput("GETCONF",
        output_stream, CNetService::eMultilineOutput);
}

void CNetScheduleAdmin::PrintQueueConf(CNcbiOstream& output_stream)
{
    m_Impl->m_API->m_Service.PrintCmdOutput("GETC",
        output_stream, CNetService::eMultilineOutput);
}

void CNetScheduleAdmin::PrintServerStatistics(CNcbiOstream& output_stream,
    EStatisticsOptions opt)
{
    m_Impl->m_API->m_Service.PrintCmdOutput(opt == eStatisticsBrief ? "STAT" :
        opt == eStatisticsClients ? "STAT CLIENTS" : "STAT ALL",
        output_stream, CNetService::eMultilineOutput_NetCacheStyle);
}


void CNetScheduleAdmin::GetQueueList(TQueueList& qlist)
{
    for (CNetServiceIterator it =
            m_Impl->m_API->m_Service.Iterate(); it; ++it) {
        CNetServer server = *it;

        qlist.push_back(SServerQueueList(server));

        NStr::Split(server.ExecWithRetry("QLST").response,
            ",;", qlist.back().queues);
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

    string output_line, st_str, cnt_str;

    for (CNetServiceIterator it =
            m_Impl->m_API->m_Service.Iterate(); it; ++it) {
        CNetServerMultilineCmdOutput cmd_output((*it).ExecWithRetry(cmd));

        while (cmd_output.ReadLine(output_line))
            if (NStr::SplitInTwo(output_line, ": ",
                    st_str, cnt_str, NStr::eMergeDelims))
                status_map[st_str] += NStr::StringToUInt(cnt_str);
    }
}

void CNetScheduleAdmin::AffinitySnapshot(
    CNetScheduleAdmin::TAffinityMap& affinity_map)
{
    static const SAffinityInfo new_affinity_info = {0, 0, 0, 0};

    string cmd = "AFLS";

    string affinity_token, cnt_str;

    for (CNetServiceIterator it =
            m_Impl->m_API->m_Service.Iterate(); it; ++it) {
        string cmd_output((*it).ExecWithRetry(cmd).response);
        vector<CTempString> affinities;
        NStr::Tokenize(cmd_output, "&", affinities);
        ITERATE(vector<CTempString>, affinity, affinities) {
            if (!NStr::SplitInTwo(*affinity, "=", affinity_token, cnt_str))
                continue;

            SAffinityInfo& affinity_info = affinity_map.insert(
                TAffinityMap::value_type(affinity_token,
                    new_affinity_info)).first->second;

            vector<CTempString> counters;
            NStr::Tokenize(cnt_str, ",", counters);

            if (counters.size() != 4)
                continue;

            affinity_info.pending_job_count += NStr::StringToUInt(counters[0]);
            affinity_info.running_job_count += NStr::StringToUInt(counters[1]);
            affinity_info.dedicated_workers += NStr::StringToUInt(counters[2]);
            affinity_info.tentative_workers += NStr::StringToUInt(counters[3]);
        }
    }
}

END_NCBI_SCOPE
