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

void CNetScheduleAdmin::ShutdownServer(
    CNetScheduleAdmin::EShutdownLevel level)
{
    string cmd;

    switch (level) {
    case eDie:
        cmd = "SHUTDOWN SUICIDE ";
        break;
    case eShutdownImmediate:
        cmd = "SHUTDOWN IMMEDIATE ";
        break;
    default:
        cmd = "SHUTDOWN ";
    }

    m_Impl->m_API->m_Service->
        RequireStandAloneServerSpec(cmd).ExecWithRetry(cmd);
}


void CNetScheduleAdmin::ReloadServerConfig()
{
    string cmd("RECO");

    m_Impl->m_API->m_Service->
        RequireStandAloneServerSpec(cmd).ExecWithRetry(cmd);
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

    for (CNetServiceIterator it =
            m_Impl->m_API->m_Service.Iterate(); it; ++it)
        (*it).ExecWithRetry(cmd);
}


void CNetScheduleAdmin::DeleteQueue(const string& qname)
{
    string cmd = "QDEL " + qname;

    for (CNetServiceIterator it =
            m_Impl->m_API->m_Service.Iterate(); it; ++it)
        (*it).ExecWithRetry(cmd);
}

void CNetScheduleAdmin::DumpJob(CNcbiOstream& out, const string& job_key)
{
    CNetServerMultilineCmdOutput output(
        m_Impl->m_API->GetServer(job_key).ExecWithRetry("DUMP " + job_key));

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
    string cmd = "CANCELQ";

    for (CNetServiceIterator it =
            m_Impl->m_API->m_Service.Iterate(); it; ++it)
        (*it).ExecWithRetry(cmd);
}

void CNetScheduleAdmin::DropQueue()
{
    string cmd = "DROPQ";

    for (CNetServiceIterator it =
            m_Impl->m_API->m_Service.Iterate(); it; ++it)
        (*it).ExecWithRetry(cmd);
}


void CNetScheduleAdmin::PrintServerVersion(CNcbiOstream& output_stream)
{
    m_Impl->m_API->m_Service.PrintCmdOutput("VERSION",
        output_stream, CNetService::eSingleLineOutput);
}


string CNetScheduleAdmin::GetServerVersion()
{
    string cmd("VERSION");

    return m_Impl->m_API->m_Service->
        RequireStandAloneServerSpec(cmd).ExecWithRetry(cmd).response;
}


void CNetScheduleAdmin::DumpQueue(CNcbiOstream& output_stream)
{
    m_Impl->m_API->m_Service.PrintCmdOutput("DUMP",
        output_stream, CNetService::eMultilineOutput);
}


void CNetScheduleAdmin::PrintQueue(CNcbiOstream& output_stream,
    CNetScheduleAPI::EJobStatus status)
{
    m_Impl->m_API->m_Service.PrintCmdOutput("QPRT " +
            CNetScheduleAPI::StatusToString(status),
        output_stream, CNetService::eMultilineOutput);
}

void CNetScheduleAdmin::PrintQueueInfo(CNcbiOstream& output_stream,
    const string& queue_name)
{
    string cmd("QINF " + queue_name);
    string cmd_output(m_Impl->m_API->m_Service->
        RequireStandAloneServerSpec(cmd).ExecWithRetry(cmd).response);
    output_stream << "Queue name: " << queue_name << NcbiEndl;
    switch (cmd_output[0]) {
    case '0':
        output_stream << "Type: static" << NcbiEndl;
        break;
    case '1':
        output_stream << "Type: dynamic" << NcbiEndl;
    }
    vector<CTempString> tokens;
    NStr::Tokenize(cmd_output, "\t", tokens);
    if (tokens.size() == 3 && tokens[0][0] == '1') {
        output_stream << "Model queue: " << tokens[1] << NcbiEndl;
        CTempString& description(tokens[2]);
        CTempString::size_type descr_pos = 0;
        CTempString::size_type descr_size = description.size();
        if (descr_size > 1)
            switch (description[descr_size - 1]) {
            case '"':
            case '\'':
                --descr_size;
            }
        if (descr_size > 0)
            switch (description[0]) {
            case '"':
            case '\'':
                ++descr_pos;
                --descr_size;
            }
        output_stream << "Description: " <<
            NStr::ParseEscapes(CTempString(tokens[2], descr_pos, descr_size)) <<
            NcbiEndl;
    }
}

unsigned CNetScheduleAdmin::CountActiveJobs()
{
    string cmd("ACNT");

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
    CNetScheduleAdmin::TStatusMap&  status_map,
    const string& affinity_token)
{
    string cmd = "STSN aff=\"";
    cmd.append(NStr::PrintableString(affinity_token));
    cmd.append("\"");

    string output_line, st_str, cnt_str;

    for (CNetServiceIterator it =
            m_Impl->m_API->m_Service.Iterate(); it; ++it) {
        CNetServerMultilineCmdOutput cmd_output((*it).ExecWithRetry(cmd));

        while (cmd_output.ReadLine(output_line))
            if (NStr::SplitInTwo(output_line, " ", st_str, cnt_str))
                status_map[CNetScheduleAPI::StringToStatus(st_str)] +=
                    NStr::StringToUInt(cnt_str);
    }
}

void CNetScheduleAdmin::AffinitySnapshot(
    CNetScheduleAdmin::TAffinityMap& affinity_map)
{
    string cmd = "AFLS";

    string aff_str, cnt_str;

    for (CNetServiceIterator it =
            m_Impl->m_API->m_Service.Iterate(); it; ++it) {
        string cmd_output((*it).ExecWithRetry(cmd).response);
        vector<CTempString> affinities;
        NStr::Tokenize(cmd_output, "&", affinities);
        ITERATE(vector<CTempString>, affinity, affinities) {
            if (NStr::SplitInTwo(*affinity, "=", aff_str, cnt_str)) {
                TWorkerNodeList worker_nodes;
                NStr::Tokenize(cnt_str, ",", worker_nodes);
                if (worker_nodes.size() > 1) {
                    cnt_str = worker_nodes.front();
                    TWorkerNodeList& wn_list = affinity_map[aff_str].second;
                    wn_list.insert(wn_list.begin(),
                        worker_nodes.begin() + 1, worker_nodes.end());
                }
                affinity_map[aff_str].first += NStr::StringToUInt(cnt_str);
            }
        }
    }
}

END_NCBI_SCOPE
