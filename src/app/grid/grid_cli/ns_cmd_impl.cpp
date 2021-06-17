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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Dmitry Kazimirov
 *
 * File Description: NetSchedule-specific commands - implementation
 *                   helpers.
 *
 */

#include <ncbi_pch.hpp>

#include "ns_cmd_impl.hpp"
#include "util.hpp"

#define MAX_VISIBLE_DATA_LENGTH 50


BEGIN_NCBI_SCOPE

int CGridCommandLineInterfaceApp::PrintNetScheduleStats()
{
    if (IsOptionSet(eJobGroupInfo))
        PrintNetScheduleStats_Generic(eNetScheduleStatJobGroups);
    else if (IsOptionSet(eClientInfo))
        PrintNetScheduleStats_Generic(eNetScheduleStatClients);
    else if (IsOptionSet(eNotificationInfo))
        PrintNetScheduleStats_Generic(eNetScheduleStatNotifications);
    else if (IsOptionSet(eAffinityInfo))
        PrintNetScheduleStats_Generic(eNetScheduleStatAffinities);
    else if (IsOptionSet(eActiveJobCount) || IsOptionSet(eJobsByStatus)) {
        if (!IsOptionSet(eQueue) &&
                (IsOptionSet(eAffinity) || IsOptionSet(eJobGroup))) {
            fprintf(stderr, GRID_APP_NAME ": invalid option combination.\n");
            return 2;
        }

        if (IsOptionSet(eActiveJobCount)) {
            CNetScheduleAdmin::TStatusMap st_map;

            m_NetScheduleAdmin.StatusSnapshot(st_map,
                    m_Opts.affinity, m_Opts.job_group);

            printf(m_Opts.output_format == eHumanReadable ?
                    "Total number of Running and Pending jobs: %u\n" :
                        m_Opts.output_format == eRaw ? "%u\n" :
                            "{\n\t\"active_job_count\": %u\n}\n",
                    st_map["Running"] + st_map["Pending"]);
        } else if (m_Opts.output_format == eRaw) {
            string cmd = "STAT JOBS";

            if (!m_Opts.affinity.empty()) {
                cmd.append(" aff=");
                cmd.append(NStr::PrintableString(m_Opts.affinity));
            }

            if (!m_Opts.job_group.empty()) {
                cmd.append(" group=");
                cmd.append(NStr::PrintableString(m_Opts.job_group));
            }

            m_NetScheduleAPI.GetService().PrintCmdOutput(cmd,
                    NcbiCout, CNetService::eMultilineOutput);
        } else {
            CNetScheduleAdmin::TStatusMap st_map;

            m_NetScheduleAdmin.StatusSnapshot(st_map,
                    m_Opts.affinity, m_Opts.job_group);

            const char* format;
            const char* format_cont;

            if (m_Opts.output_format == eHumanReadable)
                format_cont = format = "%s: %u\n";
            else { // Output format is eJSON.
                printf("{");
                format = "\n\t\"%s\": %u";
                format_cont = ",\n\t\"%s\": %u";
            }

            ITERATE(CNetScheduleAdmin::TStatusMap, it, st_map) {
                if (it->second > 0 || it->first == "Total") {
                    printf(format, it->first.c_str(), it->second);
                    format = format_cont;
                }
            }

            if (m_Opts.output_format == eJSON)
                printf("\n}\n");
        }
    } else {
        if (m_Opts.output_format != eJSON)
            m_NetScheduleAdmin.PrintServerStatistics(NcbiCout,
                IsOptionSet(eBrief) ? CNetScheduleAdmin::eStatisticsBrief :
                    CNetScheduleAdmin::eStatisticsAll);
        else {
            CJsonNode result(CJsonNode::NewObjectNode());

            for (CNetServiceIterator it = m_NetScheduleAPI.GetService().Iterate(
                    CNetService::eIncludePenalized); it; ++it)
                result.SetByKey((*it).GetServerAddress(),
                        g_LegacyStatToJson(*it, !IsOptionSet(eBrief)));

            g_PrintJSON(stdout, result);
        }
    }

    return 0;
}

void CGridCommandLineInterfaceApp::PrintNetScheduleStats_Generic(
        ENetScheduleStatTopic topic)
{
    if (m_Opts.output_format == eJSON) {
        CJsonNode result(CJsonNode::NewObjectNode());

        for (CNetServiceIterator it = m_NetScheduleAPI.GetService().Iterate(
                CNetService::eIncludePenalized); it; ++it)
            result.SetByKey((*it).GetServerAddress(),
                    g_GenericStatToJson(*it, topic, IsOptionSet(eVerbose)));

        g_PrintJSON(stdout, result);
    } else {
        string cmd(g_GetNetScheduleStatCommand(topic));

        if (IsOptionSet(eVerbose))
            cmd.append(" VERBOSE");

        m_NetScheduleAPI.GetService().PrintCmdOutput(cmd,
                NcbiCout, CNetService::eMultilineOutput);
    }
}

void CPrintJobInfo::ProcessJobMeta(const CNetScheduleKey& key)
{
    printf("server_address: %s:%hu\njob_id: %u\n",
        g_NetService_TryResolveHost(key.host).c_str(), key.port, key.id);

    if (!key.queue.empty())
        printf("queue_name: %s\n", key.queue.c_str());
}

void CPrintJobInfo::BeginJobEvent(const CTempString& event_header)
{
    printf("[%.*s]\n", int(event_header.length()), event_header.data());
}

void CPrintJobInfo::ProcessJobEventField(const CTempString& attr_name,
        const string& attr_value)
{
    printf("    %.*s: %s\n", int(attr_name.length()),
            attr_name.data(), attr_value.c_str());
}

void CPrintJobInfo::ProcessJobEventField(const CTempString& attr_name)
{
    printf("    %.*s\n", int(attr_name.length()), attr_name.data());
}

void CPrintJobInfo::PrintXput(const string& data, const char* prefix)
{
    _ASSERT(prefix);

    if (NStr::StartsWith(data, "K "))
        printf("%s_storage: netcache, key=%s\n", prefix, data.c_str() + 2);
    else {
        const char* format;
        CTempString raw_data;

        if (NStr::StartsWith(data, "D ")) {
            format = "embedded";
            raw_data.assign(data.data() + 2, data.length() - 2);
        } else {
            format = "raw";
            raw_data = data;
        }

        printf("%s_storage: %s, size=%lu\n", prefix,
            format, (unsigned long) raw_data.length());

        if (data.length() <= MAX_VISIBLE_DATA_LENGTH)
            printf("%s_%s_data: \"%s\"\n", format, prefix,
                    NStr::PrintableString(raw_data).c_str());
        else
            printf("%s_%s_data_preview: \"%s\"...\n", format, prefix,
                    NStr::PrintableString(CTempString(raw_data.data(),
                            MAX_VISIBLE_DATA_LENGTH)).c_str());
    }
}

void CPrintJobInfo::ProcessJobInfoField(const CTempString& field_name,
        const CTempString& field_value)
{
    printf("%.*s: %.*s\n", (int) field_name.length(), field_name.data(),
            (int) field_value.length(), field_value.data());
}

void CPrintJobInfo::ProcessRawLine(const string& line)
{
    CGridCommandLineInterfaceApp::PrintLine(line.c_str());
}

END_NCBI_SCOPE
