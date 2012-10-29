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

void g_DetectTypeAndSet(CJsonNode& node, const string& key, const string& value)
{
    if (IsInteger(value))
        node.SetNumber(key, NStr::StringToInt8(value));
    else if (NStr::CompareNocase(value, "FALSE") == 0)
        node.SetBoolean(key, false);
    else if (NStr::CompareNocase(value, "TRUE") == 0)
        node.SetBoolean(key, true);
    else if (NStr::CompareNocase(value, "NONE") == 0)
        node.SetNull(key);
    else
        node.SetString(key, UnquoteIfQuoted(value));
}

class CStructuredNetScheduleOutputParser
{
public:
    CStructuredNetScheduleOutputParser(const string& ns_output) :
        m_NSOutput(ns_output), m_Ch(m_NSOutput.c_str())
    {
    }

    CJsonNode ParseObject(bool root_object);

private:
    size_t GetRemainder() const
    {
        return m_NSOutput.length() - (m_Ch - m_NSOutput.data());
    }

    string ParseString()
    {
        size_t len;
        string str(NStr::ParseQuoted(CTempString(m_Ch, GetRemainder()), &len));
        m_Ch += len;
        return str;
    }

    bool MoreNodes()
    {
        while (isspace(*m_Ch))
            ++m_Ch;
        if (*m_Ch != ',')
            return false;
        ++m_Ch;
        return true;
    }

    CJsonNode ParseArray();
    CJsonNode ParseNode();
    void ThrowUnexpectedCharError();

    const string m_NSOutput;
    const char* m_Ch;
};

CJsonNode CStructuredNetScheduleOutputParser::ParseObject(bool root_object)
{
    CJsonNode result(CJsonNode::NewObjectNode());

    CJsonNode new_node;

    do {
        while (isspace(*m_Ch))
            ++m_Ch;

        if (*m_Ch != '\'' && *m_Ch != '"')
            break;

        // New attribute/value pair
        string attr_name(ParseString());

        while (isspace(*m_Ch))
            ++m_Ch;
        if (*m_Ch == ':' || *m_Ch == '=')
            while (isspace(*++m_Ch))
                ;

        if (new_node = ParseNode())
            result.SetNode(attr_name, new_node);
        else
            ThrowUnexpectedCharError();
    } while (MoreNodes());

    if (root_object ? *m_Ch != '\0' : *m_Ch != '}')
        ThrowUnexpectedCharError();

    ++m_Ch;

    return result;
}

CJsonNode CStructuredNetScheduleOutputParser::ParseArray()
{
    CJsonNode result(CJsonNode::NewArrayNode());

    CJsonNode new_node;

    do {
        while (isspace(*m_Ch))
            ++m_Ch;

        if (new_node = ParseNode())
            result.PushNode(new_node);
        else
            break;
    } while (MoreNodes());

    if (*m_Ch != ']')
        ThrowUnexpectedCharError();

    ++m_Ch;

    return result;
}

CJsonNode CStructuredNetScheduleOutputParser::ParseNode()
{
    switch (*m_Ch) {
    case '[':
        ++m_Ch;
        return ParseArray();

    case '{':
        ++m_Ch;
        return ParseObject(false);

    case '\'': case '"':
        return CJsonNode::NewStringNode(ParseString());
    }

    size_t max_len = GetRemainder();
    size_t len = 1;

    switch (*m_Ch) {
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        while (len <= max_len && isdigit(m_Ch[len]))
            ++len;

        {
            CJsonNode::TNumber val(NStr::StringToInt8(CTempString(m_Ch, len)));
            m_Ch += len;
            return CJsonNode::NewNumberNode(val);
        }

    case 'F': case 'f': case 'N': case 'n':
    case 'T': case 't': case 'Y': case 'y':
        while (len <= max_len && isalpha(m_Ch[len]))
            ++len;

        {
            bool val(NStr::StringToBool(CTempString(m_Ch, len)));
            m_Ch += len;
            return CJsonNode::NewBooleanNode(val);
        }
    }

    return CJsonNode();
}

void CStructuredNetScheduleOutputParser::ThrowUnexpectedCharError()
{
    size_t position = m_Ch - m_NSOutput.data() + 1;

    if (*m_Ch == '\0') {
        NCBI_THROW2(CStringException, eFormat,
                "Unexpected end of NetSchedule output", position);
    } else {
        NCBI_THROW2(CStringException, eFormat,
                "Unexpected character in NetSchedule output", position);
    }
}

CJsonNode g_StructuredNetScheduleOutputToJson(const string& ns_output)
{
    CStructuredNetScheduleOutputParser parser(ns_output);

    return parser.ParseObject(true);
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

CJsonNode g_GenericStatToJson(CNetServer server,
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
                else
                    g_DetectTypeAndSet(entity_info, key_norm, value);
            }
        }
    }
    if (entity_info)
        entities.PushNode(entity_info);

    return entities;
}

CJsonNode g_LegacyStatToJson(CNetServer server, bool verbose)
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
    else if (IsOptionSet(eJobsByAffinity)) {
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
    } else if (IsOptionSet(eActiveJobCount) || IsOptionSet(eJobsByStatus)) {
        if (!IsOptionSet(eQueue) &&
                (IsOptionSet(eAffinity) || IsOptionSet(eGroup))) {
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

            for (CNetServiceIterator it =
                    m_NetScheduleAPI.GetService().Iterate(); it; ++it)
                result.SetNode((*it).GetServerAddress(),
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

        for (CNetServiceIterator it =
                m_NetScheduleAPI.GetService().Iterate(); it; ++it)
            result.SetNode((*it).GetServerAddress(),
                    g_GenericStatToJson(*it, topic, IsOptionSet(eVerbose)));

        g_PrintJSON(stdout, result);
    } else {
        string cmd(s_StatTopics[topic].command);

        if (IsOptionSet(eVerbose))
            cmd.append(" VERBOSE");

        m_NetScheduleAPI.GetService().PrintCmdOutput(cmd,
                NcbiCout, CNetService::eMultilineOutput);
    }
}

static void s_GetQueueInfo(CNetScheduleAPI ns_server_with_queue_set_up,
        CJsonNode& queue_info_node)
{
    CNetScheduleAdmin::TQueueInfo queue_info;
    ns_server_with_queue_set_up.GetAdmin().GetQueueInfo(
            *ns_server_with_queue_set_up.GetService().Iterate(), queue_info);
    ITERATE(CNetScheduleAdmin::TQueueInfo, qi, queue_info) {
        g_DetectTypeAndSet(queue_info_node, qi->first, qi->second);
    }
}

CJsonNode g_QueueInfoToJson(CNetScheduleAPI ns_api,
        const string& queue_name, bool group_by_server_addr)
{
    CJsonNode result(CJsonNode::NewObjectNode());
    CJsonNode target_map(result);

    string client_name(ns_api.GetService().GetServerPool().GetClientName());

    CNetScheduleAdmin::TQueueList qlist;
    ns_api.GetAdmin().GetQueueList(qlist);

    ITERATE(CNetScheduleAdmin::TQueueList, server_and_its_queues, qlist) {
        string server_address(server_and_its_queues->server.GetServerAddress());
        if (group_by_server_addr)
            result.SetNode(server_address,
                    target_map = CJsonNode::NewObjectNode());

        if (!queue_name.empty())
            s_GetQueueInfo(CNetScheduleAPI(server_address,
                    client_name, queue_name), target_map);
        else {
            ITERATE(list<string>, each_queue_name,
                    server_and_its_queues->queues) {
                CJsonNode queue_info_node(CJsonNode::NewObjectNode());
                s_GetQueueInfo(CNetScheduleAPI(server_address, client_name,
                            *each_queue_name), queue_info_node);
                target_map.SetNode(*each_queue_name, queue_info_node);
            }
        }
    }

    return result;
}

CJsonNode g_QueueClassInfoToJson(CNetScheduleAPI ns_api,
        bool group_by_server_addr)
{
    CJsonNode result(CJsonNode::NewObjectNode());
    CJsonNode queue_class_map(result);

    string cmd("STAT QCLASSES");

    for (CNetServiceIterator it = ns_api.GetService().Iterate(); it; ++it) {
        if (group_by_server_addr)
            result.SetNode((*it).GetServerAddress(),
                    queue_class_map = CJsonNode::NewObjectNode());

        CNetServerMultilineCmdOutput output((*it).ExecWithRetry(cmd));

        string line;
        CJsonNode queue_class_params;
        string param_name, param_value;

        while (output.ReadLine(line))
            if (NStr::StartsWith(line, "[qclass ") &&
                    line.length() > sizeof("[qclass ") - 1)
                queue_class_map.SetNode(line.substr(sizeof("[qclass ") - 1,
                        line.length() - sizeof("[qclass ")),
                        queue_class_params = CJsonNode::NewObjectNode());
            else if (queue_class_params && NStr::SplitInTwo(line, ": ",
                        param_name, param_value, NStr::eMergeDelims))
                g_DetectTypeAndSet(queue_class_params, param_name, param_value);
    }

    return result;
}

CAttrListParser::ENextAttributeType CAttrListParser::NextAttribute(
    CTempString& attr_name, string& attr_value)
{
    while (isspace(*m_Position))
        ++m_Position;

    if (*m_Position == '\0')
        return eNoMoreAttributes;

    const char* start_pos = m_Position;

    for (;;)
        if (*m_Position == '=') {
            attr_name.assign(start_pos, m_Position - start_pos);
            break;
        } else if (isspace(*m_Position)) {
            attr_name.assign(start_pos, m_Position - start_pos);
            while (isspace(*++m_Position))
                ;
            if (*m_Position == '=')
                break;
            else
                return eStandAloneAttribute;
        } else if (*++m_Position == '\0') {
            attr_name.assign(start_pos, m_Position - start_pos);
            return eStandAloneAttribute;
        }

    // Skip the equals sign and the spaces that may follow it.
    while (isspace(*++m_Position))
        ;

    start_pos = m_Position;

    switch (*m_Position) {
    case '\0':
        NCBI_THROW_FMT(CArgException, eInvalidArg,
            "empty attribute value must be specified as " <<
                attr_name << "=\"\"");
    case '\'':
    case '"':
        {
            size_t n_read;
            attr_value = NStr::ParseQuoted(CTempString(start_pos,
                m_EOL - start_pos), &n_read);
            m_Position += n_read;
        }
        break;
    default:
        while (*++m_Position != '\0' && !isspace(*m_Position))
            ;
        attr_value = NStr::ParseEscapes(
            CTempString(start_pos, m_Position - start_pos));
    }

    return eAttributeWithValue;
}

void CPrintJobInfo::ProcessJobMeta(const CNetScheduleKey& key)
{
    printf("server_address: %s:%hu\nid: %u\n",
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

void CPrintJobInfo::ProcessInputOutput(const string& data,
        const CTempString& input_or_output)
{
    if (NStr::StartsWith(data, "K "))
        printf("%.*s_storage: netcache, key=%s\n",
                (int) input_or_output.length(), input_or_output.data(),
                data.c_str() + 2);
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

        printf("%.*s_storage: %s, size=%lu\n",
            (int) input_or_output.length(), input_or_output.data(),
            format, (unsigned long) raw_data.length());

        if (data.length() <= MAX_VISIBLE_DATA_LENGTH)
            printf("%s_%.*s_data: \"%s\"\n", format,
                    (int) input_or_output.length(), input_or_output.data(),
                    NStr::PrintableString(raw_data).c_str());
        else
            printf("%s_%.*s_data_preview: \"%s\"...\n", format,
                    (int) input_or_output.length(), input_or_output.data(),
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

void CJobInfoToJSON::ProcessJobMeta(const CNetScheduleKey& key)
{
    m_JobInfo.SetString("server_host", g_NetService_TryResolveHost(key.host));
    m_JobInfo.SetNumber("server_port", key.port);

    m_JobInfo.SetNumber("id", key.id);

    if (!key.queue.empty())
        m_JobInfo.SetString("queue_name", UnquoteIfQuoted(key.queue));
}

void CJobInfoToJSON::BeginJobEvent(const CTempString& event_header)
{
    if (!m_JobEvents)
        m_JobInfo.SetNode("events", m_JobEvents = CJsonNode::NewArrayNode());

    m_JobEvents.PushNode(m_CurrentEvent = CJsonNode::NewObjectNode());
}

void CJobInfoToJSON::ProcessJobEventField(const CTempString& attr_name,
        const string& attr_value)
{
    g_DetectTypeAndSet(m_CurrentEvent, attr_name, attr_value);
}

void CJobInfoToJSON::ProcessJobEventField(const CTempString& attr_name)
{
    m_CurrentEvent.SetNull(attr_name);
}

void CJobInfoToJSON::ProcessInputOutput(const string& data,
        const CTempString& input_or_output)
{
    CJsonNode node(CJsonNode::NewObjectNode());

    if (NStr::StartsWith(data, "D ")) {
        node.SetString("storage", "embedded");
        node.SetString("embedded_data", data.substr(2));
    } else if (NStr::StartsWith(data, "K ")) {
        node.SetString("storage", "netcache");
        node.SetString("netcache_key", data.substr(2));
    } else {
        node.SetString("storage", "raw");
        node.SetString("raw_data", data);
    }

    m_JobInfo.SetNode(input_or_output, node);
}

void CJobInfoToJSON::ProcessJobInfoField(const CTempString& field_name,
        const CTempString& field_value)
{
    g_DetectTypeAndSet(m_JobInfo, field_name, field_value);
}

void CJobInfoToJSON::ProcessRawLine(const string& line)
{
    if (!m_UnparsableLines)
        m_JobInfo.SetNode("unparsable_lines",
                m_UnparsableLines = CJsonNode::NewArrayNode());

    m_UnparsableLines.PushString(line);
}

void ProcessJobInfo(CNetScheduleAPI ns_api, const string& job_key,
        IJobInfoProcessor* processor, bool verbose)
{
    processor->ProcessJobMeta(CNetScheduleKey(job_key));

    if (verbose) {
        CNetServerMultilineCmdOutput output(ns_api.GetAdmin().DumpJob(job_key));

        string line;

        while (output.ReadLine(line)) {
            if (!line.empty() && line[0] != '[' && !NStr::StartsWith(line,
                    TEMP_STRING_CTOR("NCBI NetSchedule"))) {
                CTempString field_name, field_value;

                if (!NStr::SplitInTwo(line, TEMP_STRING_CTOR(": "),
                        field_name, field_value, NStr::eMergeDelims))
                    processor->ProcessRawLine(line);
                else if (field_name == TEMP_STRING_CTOR("input") ||
                        field_name == TEMP_STRING_CTOR("output"))
                    processor->ProcessInputOutput(
                            NStr::ParseQuoted(field_value), field_name);
                else if (NStr::StartsWith(field_name,
                        TEMP_STRING_CTOR("event"))) {
                    processor->BeginJobEvent(field_name);

                    try {
                        CAttrListParser attr_parser;
                        attr_parser.Reset(field_value);
                        CAttrListParser::ENextAttributeType next_attr_type;
                        CTempString attr_name;
                        string attr_value;

                        while ((next_attr_type = attr_parser.NextAttribute(
                                attr_name, attr_value)) !=
                                CAttrListParser::eNoMoreAttributes)
                            if (next_attr_type ==
                                    CAttrListParser::eAttributeWithValue)
                                processor->ProcessJobEventField(attr_name,
                                        attr_value);
                            else // CAttrListParser::eStandAloneAttribute
                                processor->ProcessJobEventField(attr_name);
                    }
                    catch (CArgException&) {
                        // Ignore this exception type.
                    }
                } else if (field_name != TEMP_STRING_CTOR("id") &&
                        field_name != TEMP_STRING_CTOR("key"))
                    processor->ProcessJobInfoField(field_name, field_value);
            }
        }
    } else {
        CNetScheduleJob job;
        job.job_id = job_key;
        CNetScheduleAPI::EJobStatus status(ns_api.GetJobDetails(job));

        processor->ProcessJobInfoField(TEMP_STRING_CTOR("status"),
                CNetScheduleAPI::StatusToString(status));

        if (status == CNetScheduleAPI::eJobNotFound)
            return;

        processor->ProcessInputOutput(job.input, TEMP_STRING_CTOR("input"));

        switch (status) {
        default:
            if (job.output.empty())
                break;
            /* FALL THROUGH */

        case CNetScheduleAPI::eDone:
        case CNetScheduleAPI::eReading:
        case CNetScheduleAPI::eConfirmed:
        case CNetScheduleAPI::eReadFailed:
            processor->ProcessInputOutput(job.output,
                    TEMP_STRING_CTOR("output"));
            break;
        }

        if (!job.error_msg.empty())
            processor->ProcessJobInfoField(TEMP_STRING_CTOR("err_msg"),
                    job.error_msg);
    }
}

END_NCBI_SCOPE
