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

BEGIN_NCBI_SCOPE

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

static bool IsInteger(const CTempString& value)
{
    if (value.empty())
        return false;

    const char* digit = value.end();

    while (--digit > value.begin())
        if (!isdigit(*digit))
            return false;

    return isdigit(*digit) || (*digit == '-' && value.length() > 1);
}

static inline string UnquoteIfQuoted(const CTempString& str)
{
    switch (str[0]) {
    case '"':
    case '\'':
        return NStr::ParseQuoted(str);
    default:
        return str;
    }
}

struct {
    const char* command;
    const char* record_prefix;
    const char* entity_name;
} static const s_StatTopics[eNumberOfStatTopics] = {
    // eNetScheduleStatJobGroups
    {"STAT GROUPS", "GROUP: ", "group"},
    // eNetScheduleStatClients
    {"STAT CLIENTS", "CLIENT: ", "client_node"},
    // eNetScheduleStatNotifications
    {"STAT NOTIFICATIONS", "CLIENT: ", "client_node"},
    // eNetScheduleStatAffinities
    {"STAT AFFINITIES", "AFFINITY: ", "affinity_token"}
};

CJsonNode GenericStatToJson(CNetServer server,
        ENetScheduleStatTopic topic, bool verbose)
{
    string stat_cmd(s_StatTopics[topic].command);
    CTempString prefix(s_StatTopics[topic].record_prefix);
    CTempString entity_name(s_StatTopics[topic].entity_name);

    if (verbose)
        stat_cmd.append(" VERBOSE");

    CNetServerMultilineCmdOutput output(server.ExecWithRetry(stat_cmd));

    CJsonNode entities(CJsonNode::NewArrayNode());
    CJsonNode entity_info;
    CJsonNode array_value;

    string line;

    while (output.ReadLine(line)) {
        if (NStr::StartsWith(line, prefix)) {
            if (entity_info)
                entities.PushNode(entity_info);
            entity_info = CJsonNode::NewObjectNode();
            entity_info.SetString(entity_name, UnquoteIfQuoted(
                    CTempString(line.data() + prefix.length(),
                    line.length() - prefix.length())));
        } else if (entity_info && NStr::StartsWith(line, "  ")) {
            if (NStr::StartsWith(line, "    ") && array_value) {
                array_value.PushString(UnquoteIfQuoted(
                        NStr::TruncateSpaces(line, NStr::eTrunc_Begin)));
            } else {
                if (array_value)
                    array_value = NULL;
                CTempString key, value;
                NStr::SplitInTwo(line, ":", key, value);
                NormalizeStatKeyName(key);
                string key_norm(key);
                value = NStr::TruncateSpaces(value, NStr::eTrunc_Begin);
                if (value.empty())
                    entity_info.SetNode(key_norm, array_value =
                            CJsonNode::NewArrayNode());
                else if (IsInteger(value))
                    entity_info.SetNumber(key_norm,
                            NStr::StringToInt8(value));
                else if (NStr::CompareNocase(value, "FALSE") == 0)
                    entity_info.SetBoolean(key_norm, false);
                else if (NStr::CompareNocase(value, "TRUE") == 0)
                    entity_info.SetBoolean(key_norm, true);
                else if (NStr::CompareNocase(value, "NONE") == 0)
                    entity_info.SetNull(key_norm);
                else
                    entity_info.SetString(key_norm, UnquoteIfQuoted(value));
            }
        }
    }
    if (entity_info)
        entities.PushNode(entity_info);

    return entities;
}

CJsonNode LegacyStatToJson(CNetServer server, bool verbose)
{
    const string stat_cmd(verbose ? "STAT ALL" : "STAT");

    CNetServerMultilineCmdOutput output(server.ExecWithRetry(stat_cmd));

    CJsonNode stat_info(CJsonNode::NewObjectNode());
    CJsonNode jobs_by_status(CJsonNode::NewObjectNode());;
    stat_info.SetNode("JobsByStatus", jobs_by_status);
    CJsonNode section_entries;

    string line;
    CTempString key, value;

    while (output.ReadLine(line)) {
        if (line.empty() || isspace(line[0]))
            continue;

        if (line[0] == '[') {
            size_t section_name_len = line.length();
            while (--section_name_len > 0 &&
                    (line[section_name_len] == ':' ||
                    line[section_name_len] == ']' ||
                    isspace(line[section_name_len])))
                ;
            line.erase(0, 1);
            line.resize(section_name_len);
            stat_info.SetNode(line,
                    section_entries = CJsonNode::NewArrayNode());
        } else if (section_entries)
            section_entries.PushString(line);
        else if (NStr::SplitInTwo(line, ":", key, value)) {
            value = NStr::TruncateSpaces(value, NStr::eTrunc_Begin);
            if (CNetScheduleAPI::StringToStatus(key) ==
                    CNetScheduleAPI::eJobNotFound)
                stat_info.SetString(key, value);
            else
                jobs_by_status.SetNumber(key, NStr::StringToInt8(value));
        }
    }

    return stat_info;
}

void CGridCommandLineInterfaceApp::PrintNetScheduleStats()
{
    if (IsOptionSet(eJobGroupInfo))
        PrintNetScheduleStats_Generic(eNetScheduleStatJobGroups);
    else if (IsOptionSet(eClientInfo))
        PrintNetScheduleStats_Generic(eNetScheduleStatClients);
    else if (IsOptionSet(eNotificationInfo))
        PrintNetScheduleStats_Generic(eNetScheduleStatNotifications);
    else if (IsOptionSet(eAffinityInfo))
        PrintNetScheduleStats_Generic(eNetScheduleStatAffinities);
    else if (IsOptionSet(eActiveJobCount)) {
        printf(m_Opts.output_format == eHumanReadable ?
                "Total number of running and pending jobs in all queues: %u\n" :
                    m_Opts.output_format == eRaw ? "%u\n" :
                        "{\n\t\"active_job_count\": %u\n}\n",
                m_NetScheduleAdmin.CountActiveJobs());
    } else if (IsOptionSet(eJobsByAffinity)) {
        CNetScheduleAdmin::TAffinityMap affinity_map;

        m_NetScheduleAdmin.AffinitySnapshot(affinity_map);

        const char* format;
        const char* format_cont;
        string human_readable_format_buf;

        if (m_Opts.output_format == eHumanReadable) {
            size_t max_affinity_token_len = sizeof("Affinity");
            ITERATE(CNetScheduleAdmin::TAffinityMap, it, affinity_map) {
                if (max_affinity_token_len < it->first.length())
                    max_affinity_token_len = it->first.length();
            }
            string sep(max_affinity_token_len + 47, '-');
            printf("%-*s    Pending     Running   Dedicated   Tentative\n"
                   "%-*s  job count   job count     workers     workers\n"
                   "%s\n",
                   int(max_affinity_token_len), "Affinity",
                   int(max_affinity_token_len), "token",
                   sep.c_str());

            human_readable_format_buf = "%-" +
                NStr::NumericToString(max_affinity_token_len);
            human_readable_format_buf.append("s%11u %11u %11u %11u\n");
            format_cont = format = human_readable_format_buf.c_str();
        } else if (m_Opts.output_format == eRaw)
            format_cont = format = "%s: %u, %u, %u, %u\n";
        else { // Output format is eJSON.
            printf("{");
            format = "\n\t\"%s\": [%u, %u, %u, %u]";
            format_cont = ",\n\t\"%s\": [%u, %u, %u, %u]";
        }

        ITERATE(CNetScheduleAdmin::TAffinityMap, it, affinity_map) {
            printf(format, it->first.c_str(),
                    it->second.pending_job_count,
                    it->second.running_job_count,
                    it->second.dedicated_workers,
                    it->second.tentative_workers);
            format = format_cont;
        }

        if (m_Opts.output_format == eJSON)
            printf("\n}\n");
    } else if (IsOptionSet(eJobsByStatus)) {
        if (m_Opts.output_format == eRaw) {
            string cmd = "STAT JOBS";

            if (!m_Opts.affinity.empty()) {
                cmd.append(" aff=");
                cmd.append(m_Opts.affinity);
            }

            if (!m_Opts.job_group.empty()) {
                cmd.append(" group=");
                cmd.append(m_Opts.job_group);
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
                if (it->second > 0) {
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

            for (CNetServiceIterator it =
                    m_NetScheduleAPI.GetService().Iterate(); it; ++it)
                result.SetNode((*it).GetServerAddress(),
                        LegacyStatToJson(*it, !IsOptionSet(eBrief)));

            PrintJSON(stdout, result);
        }
    }
}

void CGridCommandLineInterfaceApp::PrintNetScheduleStats_Generic(
        ENetScheduleStatTopic topic)
{
    if (m_Opts.output_format == eJSON) {
        CJsonNode result(CJsonNode::NewObjectNode());

        for (CNetServiceIterator it =
                m_NetScheduleAPI.GetService().Iterate(); it; ++it)
            result.SetNode((*it).GetServerAddress(),
                    GenericStatToJson(*it, topic, IsOptionSet(eVerbose)));

        PrintJSON(stdout, result);
    } else {
        string cmd(s_StatTopics[topic].command);

        if (IsOptionSet(eVerbose))
            cmd.append(" VERBOSE");

        m_NetScheduleAPI.GetService().PrintCmdOutput(cmd,
                NcbiCout, CNetService::eMultilineOutput);
    }
}

END_NCBI_SCOPE
