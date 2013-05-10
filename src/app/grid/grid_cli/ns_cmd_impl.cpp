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

void g_DetectTypeAndSet(CJsonNode& node,
        const CTempString& key, const CTempString& value)
{
    const char* ch = value.begin();
    const char* end = value.end();

    switch (*ch) {
    case '"':
    case '\'':
        node.SetString(key, NStr::ParseQuoted(value));
        return;

    case '-':
        if (++ch >= end || !isdigit(*ch)) {
            node.SetString(key, value);
            return;
        }

    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        do
            if (++ch >= end) {
                node.SetInteger(key, NStr::StringToInt8(value));
                return;
            }
        while (isdigit(*ch));

        switch (*ch) {
        case '.':
            if (++ch == end || !isdigit(*ch)) {
                node.SetString(key, value);
                return;
            }
            for (;;) {
                if (++ch == end) {
                    node.SetDouble(key, NStr::StringToDouble(value));
                    return;
                }

                if (!isdigit(*ch)) {
                    if (*ch == 'E' || *ch == 'e')
                        break;

                    node.SetString(key, value);
                    return;
                }
            }
            /* FALL THROUGH */

        case 'E':
        case 'e':
            if (++ch < end && (*ch == '-' || *ch == '+' ?
                    ++ch < end && isdigit(*ch) : isdigit(*ch)))
                do
                    if (++ch == end) {
                        node.SetDouble(key, NStr::StringToDouble(value));
                        return;
                    }
                while (isdigit(*ch));
            /* FALL THROUGH */

        default:
            node.SetString(key, value);
            return;
        }
    }

    if (NStr::CompareNocase(value, "FALSE") == 0)
        node.SetBoolean(key, false);
    else if (NStr::CompareNocase(value, "TRUE") == 0)
        node.SetBoolean(key, true);
    else if (NStr::CompareNocase(value, "NONE") == 0)
        node.SetNull(key);
    else
        node.SetString(key, value);
}

#define INVALID_FORMAT_ERROR() \
    NCBI_THROW2(CStringException, eFormat, \
            (*m_Ch == '\0' ? "Unexpected end of NetSchedule output" : \
                    "Syntax error in structured NetSchedule output"), \
            GetPosition())

string CNetScheduleStructuredOutputParser::ParseString(size_t max_len)
{
    size_t len;
    string val(NStr::ParseQuoted(CTempString(m_Ch, max_len), &len));

    m_Ch += len;
    return val;
}

Int8 CNetScheduleStructuredOutputParser::ParseInt(size_t len)
{
    Int8 val = NStr::StringToInt8(CTempString(m_Ch, len));

    if (*m_Ch == '-') {
        ++m_Ch;
        --len;
    }
    if (*m_Ch == '0' && len > 1) {
        NCBI_THROW2(CStringException, eFormat,
                "Leading zeros are not allowed", GetPosition());
    }

    m_Ch += len;
    return val;
}

double CNetScheduleStructuredOutputParser::ParseDouble(size_t len)
{
    double val = NStr::StringToDouble(CTempString(m_Ch, len));

    m_Ch += len;
    return val;
}

bool CNetScheduleStructuredOutputParser::MoreNodes()
{
    while (isspace(*m_Ch))
        ++m_Ch;
    if (*m_Ch != ',')
        return false;
    while (isspace(*++m_Ch))
        ;
    return true;
}

CJsonNode CNetScheduleStructuredOutputParser::ParseObject(char closing_char)
{
    CJsonNode result(CJsonNode::NewObjectNode());

    while (isspace(*m_Ch))
        ++m_Ch;

    if (*m_Ch == closing_char) {
        ++m_Ch;
        return result;
    }

    while (*m_Ch == '\'' || *m_Ch == '"') {
        // New attribute/value pair
        string attr_name(ParseString(GetRemainder()));

        while (isspace(*m_Ch))
            ++m_Ch;
        if (*m_Ch == ':' || *m_Ch == '=')
            while (isspace(*++m_Ch))
                ;

        result.SetByKey(attr_name, ParseValue());

        if (!MoreNodes()) {
            if (*m_Ch != closing_char)
                break;
            ++m_Ch;
            return result;
        }
    }

    INVALID_FORMAT_ERROR();
}

CJsonNode CNetScheduleStructuredOutputParser::ParseArray(char closing_char)
{
    CJsonNode result(CJsonNode::NewArrayNode());

    while (isspace(*m_Ch))
        ++m_Ch;

    if (*m_Ch == closing_char) {
        ++m_Ch;
        return result;
    }

    do
        result.Append(ParseValue());
    while (MoreNodes());

    if (*m_Ch == closing_char) {
        ++m_Ch;
        return result;
    }

    INVALID_FORMAT_ERROR();
}

CJsonNode CNetScheduleStructuredOutputParser::ParseValue()
{
    size_t max_len = GetRemainder();
    size_t len = 0;

    switch (*m_Ch) {
    /* Array */
    case '[':
        ++m_Ch;
        return ParseArray(']');

    /* Object */
    case '{':
        ++m_Ch;
        return ParseObject('}');

    /* String */
    case '\'':
    case '"':
        return CJsonNode::NewStringNode(ParseString(max_len));

    /* Number */
    case '-':
        // Check that there's at least one digit after the minus sign.
        if (max_len <= 1 || !isdigit(m_Ch[1])) {
            ++m_Ch;
            break;
        }
        len = 1;

    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        // Skim through the integer part.
        do
            if (++len >= max_len)
                return CJsonNode::NewIntegerNode(ParseInt(len));
        while (isdigit(m_Ch[len]));

        // Stumbled upon a non-digit character -- check
        // if it's a fraction part or an exponent part.
        switch (m_Ch[len]) {
        case '.':
            if (++len == max_len || !isdigit(m_Ch[len])) {
                NCBI_THROW2(CStringException, eFormat,
                        "At least one digit after the decimal "
                        "point is required", GetPosition());
            }
            for (;;) {
                if (++len == max_len)
                    return CJsonNode::NewDoubleNode(ParseDouble(len));

                if (!isdigit(m_Ch[len])) {
                    if (m_Ch[len] == 'E' || m_Ch[len] == 'e')
                        break;

                    return CJsonNode::NewDoubleNode(ParseDouble(len));
                }
            }
            /* FALL THROUGH */

        case 'E':
        case 'e':
            if (++len == max_len ||
                    (m_Ch[len] == '-' || m_Ch[len] == '+' ?
                            ++len == max_len || !isdigit(m_Ch[len]) :
                            !isdigit(m_Ch[len]))) {
                m_Ch += len;
                NCBI_THROW2(CStringException, eFormat,
                        "Invalid exponent specification", GetPosition());
            }
            while (++len < max_len && isdigit(m_Ch[len]))
                ;
            return CJsonNode::NewDoubleNode(ParseDouble(len));

        default:
            return CJsonNode::NewIntegerNode(ParseInt(len));
        }

    /* Boolean */
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

    INVALID_FORMAT_ERROR();
}

class CExecAndParseStructuredOutput : public IExecToJson
{
public:
    CExecAndParseStructuredOutput(const string& cmd) :
        m_Cmd(cmd)
    {
        g_AppendClientIPAndSessionID(m_Cmd);
    }

protected:
    virtual CJsonNode ExecOn(CNetServer server);

private:
    string m_Cmd;
};

CJsonNode CExecAndParseStructuredOutput::ExecOn(CNetServer server)
{
    CNetScheduleStructuredOutputParser parser;

    return parser.ParseObject(server.ExecWithRetry(m_Cmd).response);
}

CJsonNode g_ExecStructuredNetScheduleCmdToJson(CNetScheduleAPI ns_api,
        const string& cmd, CNetService::EServiceType service_type)
{
    CExecAndParseStructuredOutput exec_and_parse_structured_output(cmd);

    return g_ExecToJson(exec_and_parse_structured_output,
            ns_api.GetService(), service_type);
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
                entities.Append(entity_info);
            entity_info = CJsonNode::NewObjectNode();
            entity_info.SetString(entity_name, UnquoteIfQuoted(
                    CTempString(line.data() + prefix.length(),
                    line.length() - prefix.length())));
        } else if (entity_info && NStr::StartsWith(line, "  ")) {
            if (NStr::StartsWith(line, "    ") && array_value) {
                array_value.AppendString(UnquoteIfQuoted(
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
                    entity_info.SetByKey(key_norm, array_value =
                            CJsonNode::NewArrayNode());
                else
                    g_DetectTypeAndSet(entity_info, key_norm, value);
            }
        }
    }
    if (entity_info)
        entities.Append(entity_info);

    return entities;
}

CJsonNode g_LegacyStatToJson(CNetServer server, bool verbose)
{
    const string stat_cmd(verbose ? "STAT ALL" : "STAT");

    CNetServerMultilineCmdOutput output(server.ExecWithRetry(stat_cmd));

    CJsonNode stat_info(CJsonNode::NewObjectNode());
    CJsonNode jobs_by_status(CJsonNode::NewObjectNode());;
    stat_info.SetByKey("JobsByStatus", jobs_by_status);
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
            stat_info.SetByKey(line,
                    section_entries = CJsonNode::NewArrayNode());
        } else if (section_entries)
            section_entries.AppendString(line);
        else if (NStr::SplitInTwo(line, ":", key, value)) {
            value = NStr::TruncateSpaces(value, NStr::eTrunc_Begin);
            if (CNetScheduleAPI::StringToStatus(key) ==
                    CNetScheduleAPI::eJobNotFound)
                stat_info.SetString(key, value);
            else
                jobs_by_status.SetInteger(key, NStr::StringToInt8(value));
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

        for (CNetServiceIterator it =
                m_NetScheduleAPI.GetService().Iterate(); it; ++it)
            result.SetByKey((*it).GetServerAddress(),
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

struct SSingleQueueInfoToJson : public IExecToJson
{
    SSingleQueueInfoToJson(const CNetScheduleAdmin& ns_admin,
            const string& queue_name) :
        m_NetScheduleAdmin(ns_admin), m_QueueName(queue_name)
    {
    }

    virtual CJsonNode ExecOn(CNetServer server);

    CNetScheduleAdmin m_NetScheduleAdmin;
    string m_QueueName;
};

CJsonNode SSingleQueueInfoToJson::ExecOn(CNetServer server)
{
    CNetScheduleAdmin::TQueueInfo queue_info;
    m_NetScheduleAdmin.GetQueueInfo(server, m_QueueName, queue_info);

    CJsonNode queue_info_node(CJsonNode::NewObjectNode());

    ITERATE(CNetScheduleAdmin::TQueueInfo, qi, queue_info) {
        g_DetectTypeAndSet(queue_info_node, qi->first, qi->second);
    }

    return queue_info_node;
}

struct SQueueInfoToJson : public IExecToJson
{
    SQueueInfoToJson(const string& cmd, const string& section_prefix) :
        m_Cmd(cmd), m_SectionPrefix(section_prefix)
    {
        g_AppendClientIPAndSessionID(m_Cmd);
    }

    virtual CJsonNode ExecOn(CNetServer server);

    string m_Cmd;
    string m_SectionPrefix;
};

CJsonNode SQueueInfoToJson::ExecOn(CNetServer server)
{
    CNetServerMultilineCmdOutput output(server.ExecWithRetry(m_Cmd));

    CJsonNode queue_map(CJsonNode::NewObjectNode());
    CJsonNode queue_params;

    string line;
    CTempString param_name, param_value;

    while (output.ReadLine(line))
        if (NStr::StartsWith(line, m_SectionPrefix) &&
                line.length() > m_SectionPrefix.length())
            queue_map.SetByKey(line.substr(m_SectionPrefix.length(),
                    line.length() - m_SectionPrefix.length() - 1),
                    queue_params = CJsonNode::NewObjectNode());
        else if (queue_params && NStr::SplitInTwo(line, ": ",
                    param_name, param_value, NStr::eMergeDelims))
            g_DetectTypeAndSet(queue_params, param_name, param_value);

    return queue_map;
}

CJsonNode g_QueueInfoToJson(CNetScheduleAPI ns_api,
        const string& queue_name, CNetService::EServiceType service_type)
{
    if (queue_name.empty()) {
        SQueueInfoToJson queue_info_proc("STAT QUEUES", "[queue ");

        return g_ExecToJson(queue_info_proc, ns_api.GetService(), service_type);
    }

    SSingleQueueInfoToJson single_queue_proc(ns_api.GetAdmin(), queue_name);

    return g_ExecToJson(single_queue_proc, ns_api.GetService(), service_type);
}

CJsonNode g_QueueClassInfoToJson(CNetScheduleAPI ns_api,
        CNetService::EServiceType service_type)
{
    SQueueInfoToJson queue_info_proc("STAT QCLASSES", "[qclass ");

    return g_ExecToJson(queue_info_proc, ns_api.GetService(), service_type);
}

CJsonNode g_ReconfAndReturnJson(CNetScheduleAPI ns_api,
        CNetService::EServiceType service_type)
{
    return g_ExecStructuredNetScheduleCmdToJson(ns_api, "RECO", service_type);
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
    m_JobInfo.SetInteger("server_port", key.port);

    m_JobInfo.SetInteger("job_id", key.id);

    if (!key.queue.empty())
        m_JobInfo.SetString("queue_name", UnquoteIfQuoted(key.queue));
}

void CJobInfoToJSON::BeginJobEvent(const CTempString& event_header)
{
    if (!m_JobEvents)
        m_JobInfo.SetByKey("events", m_JobEvents = CJsonNode::NewArrayNode());

    m_JobEvents.Append(m_CurrentEvent = CJsonNode::NewObjectNode());
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

    m_JobInfo.SetByKey(input_or_output, node);
}

void CJobInfoToJSON::ProcessJobInfoField(const CTempString& field_name,
        const CTempString& field_value)
{
    g_DetectTypeAndSet(m_JobInfo, field_name, field_value);
}

void CJobInfoToJSON::ProcessRawLine(const string& line)
{
    if (!m_UnparsableLines)
        m_JobInfo.SetByKey("unparsable_lines",
                m_UnparsableLines = CJsonNode::NewArrayNode());

    m_UnparsableLines.AppendString(line);
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
