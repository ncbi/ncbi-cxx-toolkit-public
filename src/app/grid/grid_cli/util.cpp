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
 * File Description: Utility functions - implementation.
 *
 */

#include <ncbi_pch.hpp>

#include "util.hpp"

#include <sstream>
#include <corelib/rwstream.hpp>
#include <cgi/ncbicgi.hpp>
#include <connect/services/ns_output_parser.hpp>
#include <connect/services/grid_rw_impl.hpp>
#include <connect/services/remote_app.hpp>
#include <misc/netstorage/netstorage.hpp>

#include <connect/ncbi_util.h>

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

class CExecAndParseStructuredOutput : public IExecToJson
{
public:
    CExecAndParseStructuredOutput(const string& cmd) :
        m_Cmd(cmd)
    {
        g_AppendClientIPSessionIDHitID(m_Cmd);
    }

protected:
    virtual CJsonNode ExecOn(CNetServer server);

private:
    string m_Cmd;
};

CJsonNode CExecAndParseStructuredOutput::ExecOn(CNetServer server)
{
    CNetScheduleStructuredOutputParser parser;

    return parser.ParseObject(server.ExecWithRetry(m_Cmd, false).response);
}

CJsonNode g_ExecStructuredNetScheduleCmdToJson(CNetScheduleAPI ns_api,
        const string& cmd, CNetService::EServiceType service_type)
{
    CExecAndParseStructuredOutput exec_and_parse_structured_output(cmd);

    return g_ExecToJson(exec_and_parse_structured_output,
            ns_api.GetService(), service_type, CNetService::eIncludePenalized);
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

string g_GetNetScheduleStatCommand(ENetScheduleStatTopic topic)
{
    return s_StatTopics[topic].command;
}

CJsonNode g_GenericStatToJson(CNetServer server,
        ENetScheduleStatTopic topic, bool verbose)
{
    string stat_cmd(s_StatTopics[topic].command);
    CTempString prefix(s_StatTopics[topic].record_prefix);
    CTempString entity_name(s_StatTopics[topic].entity_name);

    if (verbose)
        stat_cmd.append(" VERBOSE");

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

CJsonNode g_LegacyStatToJson(CNetServer server, bool verbose)
{
    const string stat_cmd(verbose ? "STAT ALL" : "STAT");

    CNetServerMultilineCmdOutput output(server.ExecWithRetry(stat_cmd, true));

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
        }
        else if (section_entries) {
            section_entries.AppendString(line);
        }
        else if (NStr::SplitInTwo(line, ":", key, value)) {
            value = NStr::TruncateSpaces_Unsafe(value, NStr::eTrunc_Begin);
            if (CNetScheduleAPI::StringToStatus(key) !=
                    CNetScheduleAPI::eJobNotFound)
                jobs_by_status.SetInteger(key, NStr::StringToInt8(value));
            else {
                if (key == "Executable path" &&
                        g_FixMisplacedPID(stat_info, value, "PID")) {
                    if (!stat_info.HasKey("Version"))
                        stat_info.SetString("Version", "Unknown");

                    if (!stat_info.HasKey("Build date"))
                        stat_info.SetString("Build date", "Unknown");
                }
                stat_info.SetByKey(key, CJsonNode::GuessType(value));
            }
        }
    }

    return stat_info;
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
        queue_info_node.SetByKey(qi->first, CJsonNode::GuessType(qi->second));
    }

    return queue_info_node;
}

struct SQueueInfoToJson : public IExecToJson
{
    SQueueInfoToJson(const string& cmd, const string& section_prefix) :
        m_Cmd(cmd), m_SectionPrefix(section_prefix)
    {
        g_AppendClientIPSessionIDHitID(m_Cmd);
    }

    virtual CJsonNode ExecOn(CNetServer server);

    string m_Cmd;
    string m_SectionPrefix;
};

CJsonNode SQueueInfoToJson::ExecOn(CNetServer server)
{
    CNetServerMultilineCmdOutput output(server.ExecWithRetry(m_Cmd, true));

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
            queue_params.SetByKey(param_name,
                    CJsonNode::GuessType(param_value));

    return queue_map;
}

CJsonNode g_QueueInfoToJson(CNetScheduleAPI ns_api,
        const string& queue_name, CNetService::EServiceType service_type)
{
    if (queue_name.empty()) {
        SQueueInfoToJson queue_info_proc("STAT QUEUES", "[queue ");

        return g_ExecToJson(queue_info_proc, ns_api.GetService(),
                service_type, CNetService::eIncludePenalized);
    }

    SSingleQueueInfoToJson single_queue_proc(ns_api.GetAdmin(), queue_name);

    return g_ExecToJson(single_queue_proc, ns_api.GetService(),
            service_type, CNetService::eIncludePenalized);
}

CJsonNode g_QueueClassInfoToJson(CNetScheduleAPI ns_api,
        CNetService::EServiceType service_type)
{
    SQueueInfoToJson queue_info_proc("STAT QCLASSES", "[qclass ");

    return g_ExecToJson(queue_info_proc, ns_api.GetService(),
            service_type, CNetService::eIncludePenalized);
}

CJsonNode g_ReconfAndReturnJson(CNetScheduleAPI ns_api,
        CNetService::EServiceType service_type)
{
    return g_ExecStructuredNetScheduleCmdToJson(ns_api, "RECO", service_type);
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
    m_CurrentEvent.SetByKey(attr_name, CJsonNode::GuessType(attr_value));
}

void CJobInfoToJSON::ProcessJobEventField(const CTempString& attr_name)
{
    m_CurrentEvent.SetNull(attr_name);
}

struct SDataDetector : protected CStringOrBlobStorageReader
{
protected:
    static CJsonNode x_CreateNode(EType type, const string& data)
    {
        CJsonNode node(CJsonNode::NewObjectNode());

        switch (type) {
        case eEmbedded:
            node.SetString("storage", "embedded");
            node.SetString("embedded_data", data);
            return node;

        case eNetCache:
            node.SetString("storage", "netcache");
            node.SetString("netcache_key", data);
            return node;

        default:
            node.SetString("storage", "raw");
            node.SetString("raw_data", data);
            return node;
        }
    }

public:
    static bool AddNode(CJsonNode parent, const string& name, string data)
    {
        // Do not combine calls to x_GetDataType and x_CreateNode,
        // as x_GetDataType modifies data
        EType type = x_GetDataType(data);
        parent.SetByKey(name, x_CreateNode(type, data));
        return type != eRaw && type != eEmpty;
    }

    static void AddNodes(CJsonNode parent, const string& name, const string& src)
    {
        if (AddNode(parent, name, src)) {
            parent.SetString("raw_" + name, src);
        }
    }
};

struct CJsonNodeUpdater
{
    CJsonNode node;

    CJsonNodeUpdater(const string& name, CJsonNode parent)
        : node(parent.GetByKeyOrNull(name)),
          m_Name(name),
          m_Parent(parent),
          m_NewNode(!node)
    {
        if (m_NewNode) node = CJsonNode::NewObjectNode();
    }


    ~CJsonNodeUpdater()
    {
        if (m_NewNode) m_Parent.SetByKey(m_Name, node);
    }

private:
    const string m_Name;
    CJsonNode m_Parent;
    const bool m_NewNode;
};

static const string s_JobType = "job_type";

struct SRemoteApp : private CBlobStreamHelper, private SDataDetector
{
private:
    static bool x_GetFileName(string& data);
    static CJsonNode x_CreateArgsNode(const string& data, CJsonNode files);
    static CJsonNode x_CreateDataNode(string data);

public:
    struct SRequest;

    static const string& Name() {
        static const string name = "remote_app";
        return name;
    }

    static void Input(CJsonNode& node, SRequest& request);
    static void Output(CJsonNode& node, CRStream& stream, const string&);
};

bool SRemoteApp::x_GetFileName(string& data)
{
    istringstream iss(data);
    string name;

    if (x_GetTypeAndName(iss, name) == eLocalFile) {
        data.swap(name);
        return true;
    }

    data.erase(0, iss.tellg());
    return false;
}

CJsonNode SRemoteApp::x_CreateArgsNode(const string& data, CJsonNode files)
{
    CJsonNode node(CJsonNode::NewObjectNode());
    node.SetString("cmdline_args", data);
    if (files) node.SetByKey("files", files);
    return node;
}

CJsonNode SRemoteApp::x_CreateDataNode(string data)
{
    EType type = x_GetDataType(data);

    if (type == eEmbedded && x_GetFileName(data)) {
        CJsonNode node(CJsonNode::NewObjectNode());
        node.SetString("storage", "localfile");
        node.SetString("filename", data);
        return node;
    }

    return x_CreateNode(type, data);
}

struct SRemoteApp::SRequest : public CRemoteAppRequest
{
    SRequest(CNcbiIstream& is)
        : CRemoteAppRequest(NULL)
    {
        x_Deserialize(is, &m_Files);
    }

    CJsonNode CreateFilesNode() const
    {
        if (m_Files.empty()) return NULL;

        CJsonNode array(CJsonNode::NewArrayNode());
        for (TStoredFiles::const_iterator i = m_Files.begin();
                i != m_Files.end(); ++i) {
            CJsonNode file(CJsonNode::NewObjectNode());
            file.SetString("filename", i->first);

            if (i->second.empty()) {
                file.SetString("storage", "localfile");
            } else {
                file.SetString("storage", "netcache");
                file.SetString("netcache_key", i->second);
            }

            array.Append(file);
        }
        return array;
    }

private:
    TStoredFiles m_Files;
};

void SRemoteApp::Input(CJsonNode& node, SRequest& request)
{
    node.SetInteger("run_timeout", request.GetAppRunTimeout());
    node.SetBoolean("exclusive", request.IsExclusiveMode());
    CJsonNode files = request.CreateFilesNode();
    node.SetByKey("args", x_CreateArgsNode(request.GetCmdLine(), files));
    node.SetByKey("stdin", x_CreateDataNode(request.GetInBlobIdOrData()));
}

void SRemoteApp::Output(CJsonNode& node, CRStream& stream, const string&)
{
    CRemoteAppResult result(NULL);
    result.Receive(stream);

    node.SetByKey("stdout", x_CreateDataNode(result.GetOutBlobIdOrData()));
    node.SetByKey("stderr", x_CreateDataNode(result.GetErrBlobIdOrData()));
    node.SetInteger("exit_code", result.GetRetCode());
}

struct SRemoteCgi
{
    struct SRequest;

    static const string& Name() {
        static const string name = "remote_cgi";
        return name;
    }

    static void Input(CJsonNode& node, SRequest& request);
    static void Output(CJsonNode& node, CRStream& stream, const string& data);
};

struct SRemoteCgi::SRequest : public CCgiRequest
{
    SRequest(CRStream& stream)
        : CCgiRequest(stream, fIgnoreQueryString | fDoNotParseContent)
    {}
};

void SRemoteCgi::Input(CJsonNode& node, SRequest& request)
{
    node.SetString("method", request.GetRequestMethodName());

    const CNcbiEnvironment& env(request.GetEnvironment());
    list<string> names;
    env.Enumerate(names);
    CJsonNodeUpdater env_updater("env", node);

    for (list<string>::iterator i = names.begin(); i != names.end(); ++i) {
        env_updater.node.SetString(*i, env.Get(*i));
    }
}

void SRemoteCgi::Output(CJsonNode& node, CRStream& stream, const string& data)
{
    string field;
    getline(stream, field);
    stream >> field;
    int status = 200;
    if (field == "Status:") stream >> status;

    node.SetInteger("status", status);
    SDataDetector::AddNode(node, "stdout", data);
}

struct SInputOutputProcessor
{
private:
    struct SStringStream : private CStringOrBlobStorageReader, public CRStream
    {
        SStringStream(const string& data)
            : CStringOrBlobStorageReader(data, NULL),
            CRStream(this, 0, 0, CRWStreambuf::fLeakExceptions)
        {
            exceptions(IOS_BASE::badbit | IOS_BASE::failbit);
        }
    };

public:
    template <class TJobType>
    static bool Input(const string& data, CJsonNode& node)
    {
        try {
            SStringStream stream(data);
            typename TJobType::SRequest request(stream);

            CJsonNodeUpdater updater(TJobType::Name(), node);
            TJobType::Input(updater.node, request);

            node.SetString(s_JobType, TJobType::Name());
            return true;
        }
        catch (...) {
            return false;
        }
    }

    template <class TJobType>
    static void Output(const string& data, CJsonNode& node)
    {
        CJsonNodeUpdater updater(TJobType::Name(), node);
        const char* value = data.empty() ? "absent" : "gone";

        try {
            SStringStream stream(data);
            TJobType::Output(updater.node, stream, data);
            value = "ready";
        }
        catch (...) {
        }

        updater.node.SetString("output_data", value);
    }
};

void CJobInfoToJSON::ProcessInput(const string& data)
{
    SDataDetector::AddNodes(m_JobInfo, "input", data);

    if (!SInputOutputProcessor::Input<SRemoteApp>(data, m_JobInfo) &&
            !SInputOutputProcessor::Input<SRemoteCgi>(data, m_JobInfo))
        m_JobInfo.SetString(s_JobType, "generic");
}

void CJobInfoToJSON::ProcessOutput(const string& data)
{
    SDataDetector::AddNodes(m_JobInfo, "output", data);
    const string job_type(m_JobInfo.GetString(s_JobType));

    if (job_type == SRemoteApp::Name()) {
        SInputOutputProcessor::Output<SRemoteApp>(data, m_JobInfo);
    } else if (job_type == SRemoteCgi::Name()) {
        SInputOutputProcessor::Output<SRemoteCgi>(data, m_JobInfo);
    }
}

void CJobInfoToJSON::ProcessJobInfoField(const CTempString& field_name,
        const CTempString& field_value)
{
    m_JobInfo.SetByKey(field_name, CJsonNode::GuessType(field_value));
}

void CJobInfoToJSON::ProcessRawLine(const string& line)
{
    if (!m_UnparsableLines)
        m_JobInfo.SetByKey("unparsable_lines",
                m_UnparsableLines = CJsonNode::NewArrayNode());

    m_UnparsableLines.AppendString(line);
}

void g_ProcessJobInfo(CNetScheduleAPI ns_api, const string& job_key,
        IJobInfoProcessor* processor, bool verbose,
        CCompoundIDPool::TInstance id_pool)
{
    processor->ProcessJobMeta(CNetScheduleKey(job_key, id_pool));

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
                else if (field_name == TEMP_STRING_CTOR("input"))
                    processor->ProcessInput(NStr::ParseQuoted(field_value));
                else if (field_name == TEMP_STRING_CTOR("output"))
                    processor->ProcessOutput(NStr::ParseQuoted(field_value));
                else if (NStr::StartsWith(field_name,
                        TEMP_STRING_CTOR("event"))) {
                    processor->BeginJobEvent(field_name);

                    try {
                        CAttrListParser attr_parser;
                        attr_parser.Reset(field_value);
                        CAttrListParser::ENextAttributeType next_attr_type;
                        CTempString attr_name;
                        string attr_value;
                        size_t attr_column;

                        while ((next_attr_type = attr_parser.NextAttribute(
                                &attr_name, &attr_value, &attr_column)) !=
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

        processor->ProcessInput(job.input);

        switch (status) {
        default:
            if (job.output.empty())
                break;
            /* FALL THROUGH */

        case CNetScheduleAPI::eDone:
        case CNetScheduleAPI::eReading:
        case CNetScheduleAPI::eConfirmed:
        case CNetScheduleAPI::eReadFailed:
            processor->ProcessOutput(job.output);
            break;
        }

        if (!job.error_msg.empty())
            processor->ProcessJobInfoField(TEMP_STRING_CTOR("err_msg"),
                    job.error_msg);
    }
}

static void Indent(FILE* output_stream, int indent_depth, const char* indent)
{
    while (--indent_depth >= 0)
        fprintf(output_stream, "%s", indent);
}

static void PrintJSONNode(FILE* output_stream, CJsonNode node,
        const char* indent,
        int struct_indent_depth = 0, const char* struct_prefix = "",
        int scalar_indent_depth = 0, const char* scalar_prefix = "")
{
    switch (node.GetNodeType()) {
    case CJsonNode::eObject:
        {
            fputs(struct_prefix, output_stream);
            Indent(output_stream, struct_indent_depth, indent);
            putc('{', output_stream);
            const char* prefix = "\n";
            int indent_depth = struct_indent_depth + 1;
            for (CJsonIterator it = node.Iterate(); it; ++it) {
                fputs(prefix, output_stream);
                Indent(output_stream, indent_depth, indent);
                fprintf(output_stream, "\"%s\":",
                        NStr::PrintableString(it.GetKey()).c_str());
                PrintJSONNode(output_stream, *it, indent,
                        indent_depth, "\n", 0, " ");
                prefix = ",\n";
            }
            putc('\n', output_stream);
            Indent(output_stream, struct_indent_depth, indent);
            putc('}', output_stream);
        }
        break;
    case CJsonNode::eArray:
        {
            fputs(struct_prefix, output_stream);
            Indent(output_stream, struct_indent_depth, indent);
            putc('[', output_stream);
            const char* prefix = "\n";
            int indent_depth = struct_indent_depth + 1;
            for (CJsonIterator it = node.Iterate(); it; ++it) {
                fputs(prefix, output_stream);
                PrintJSONNode(output_stream, *it, indent,
                        indent_depth, "", indent_depth, "");
                prefix = ",\n";
            }
            putc('\n', output_stream);
            Indent(output_stream, struct_indent_depth, indent);
            putc(']', output_stream);
        }
        break;
    case CJsonNode::eString:
        fputs(scalar_prefix, output_stream);
        Indent(output_stream, scalar_indent_depth, indent);
        fprintf(output_stream, "\"%s\"",
                NStr::PrintableString(node.AsString()).c_str());
        break;
    case CJsonNode::eInteger:
        fputs(scalar_prefix, output_stream);
        Indent(output_stream, scalar_indent_depth, indent);
        fprintf(output_stream, "%lld", (long long) node.AsInteger());
        break;
    case CJsonNode::eDouble:
        fputs(scalar_prefix, output_stream);
        Indent(output_stream, scalar_indent_depth, indent);
        fprintf(output_stream, "%.10g", node.AsDouble());
        break;
    case CJsonNode::eBoolean:
        fputs(scalar_prefix, output_stream);
        Indent(output_stream, scalar_indent_depth, indent);
        fputs(node.AsBoolean() ? "true" : "false", output_stream);
        break;
    default: // CJsonNode::eNull
        fputs(scalar_prefix, output_stream);
        Indent(output_stream, scalar_indent_depth, indent);
        fputs("null", output_stream);
    }
}

void g_PrintJSON(FILE* output_stream, CJsonNode node, const char* indent)
{
    PrintJSONNode(output_stream, node, indent);
    putc('\n', output_stream);
}

struct SExecAnyCmdToJson : public IExecToJson
{
    SExecAnyCmdToJson(const string& cmd, bool multiline) :
        m_Cmd(cmd), m_Multiline(multiline)
    {
    }

    virtual CJsonNode ExecOn(CNetServer server);

    string m_Cmd;
    bool m_Multiline;
};

CJsonNode SExecAnyCmdToJson::ExecOn(CNetServer server)
{
    if (!m_Multiline)
        return CJsonNode::NewStringNode(server.ExecWithRetry(m_Cmd,
                false).response);

    CNetServerMultilineCmdOutput output(server.ExecWithRetry(m_Cmd, true));

    CJsonNode lines(CJsonNode::NewArrayNode());
    string line;

    while (output.ReadLine(line))
        lines.AppendString(line);

    return lines;
}

CJsonNode g_ExecAnyCmdToJson(CNetService service,
        CNetService::EServiceType service_type,
        const string& command, bool multiline)
{
    SExecAnyCmdToJson exec_any_cmd_proc(command, multiline);

    return g_ExecToJson(exec_any_cmd_proc, service, service_type);
}

struct SServerInfoToJson : public IExecToJson
{
    SServerInfoToJson(bool server_version_key) :
        m_ServerVersionKey(server_version_key)
    {
    }

    virtual CJsonNode ExecOn(CNetServer server);

    bool m_ServerVersionKey;
};

CJsonNode SServerInfoToJson::ExecOn(CNetServer server)
{
    return g_ServerInfoToJson(server.GetServerInfo(), m_ServerVersionKey);
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

CJsonNode g_ServerInfoToJson(CNetService service,
        CNetService::EServiceType service_type,
        bool server_version_key)
{
    SServerInfoToJson server_info_proc(server_version_key);

    return g_ExecToJson(server_info_proc, service,
            service_type, CNetService::eIncludePenalized);
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
    NStr::Split(str, TEMP_STRING_CTOR(" "), words);
    ITERATE(list<CTempString>, it, words) {
        array_node.AppendString(*it);
    }
    return array_node;
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
            wn_info = g_ServerInfoToJson(g_ServerInfoFromString(line), false);
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

void g_SuspendNetSchedule(CNetScheduleAPI netschedule_api, bool pullback_mode)
{
    string cmd(pullback_mode ? "QPAUSE pullback=1" : "QPAUSE");

    g_AppendClientIPSessionIDHitID(cmd);

    netschedule_api.GetService().ExecOnAllServers(cmd);
}

void g_ResumeNetSchedule(CNetScheduleAPI netschedule_api)
{
    string cmd("QRESUME");

    g_AppendClientIPSessionIDHitID(cmd);

    netschedule_api.GetService().ExecOnAllServers(cmd);
}

void g_SuspendWorkerNode(CNetServer worker_node,
        bool pullback_mode, unsigned timeout)
{
    string cmd("SUSPEND");
    if (pullback_mode)
        cmd += " pullback";
    if (timeout > 0) {
        cmd += " timeout=";
        cmd += NStr::NumericToString(timeout);
    }
    worker_node.ExecWithRetry(cmd, false);
}

void g_ResumeWorkerNode(CNetServer worker_node)
{
    worker_node.ExecWithRetry("RESUME", false);
}

void g_GetUserAndHost(string* user, string* host)
{
    char user_buf[64];
    if (CORE_GetUsername(user_buf, sizeof(user_buf)) != NULL)
        *user = user_buf;

    *host = CSocketAPI::gethostname();
}

CJsonNode s_GetBlobMeta(const CNetCacheKey& key)
{
    CJsonNode result(CJsonNode::NewObjectNode());
    result.SetString("type", TOKEN_TYPE__NETCACHE_BLOB_KEY);
    result.SetInteger("key_version", key.GetVersion());

    if (key.GetVersion() != 3) {
        const string server_host(g_NetService_TryResolveHost(key.GetHost()));
        result.SetString("server_host", server_host);
        result.SetInteger("server_port", key.GetPort());
    } else {
        result.SetInteger("server_address_crc32", key.GetHostPortCRC32());
    }

    result.SetInteger("id", key.GetId());

    CTime generation_time;
    generation_time.SetTimeT(key.GetCreationTime());
    result.SetString("key_generation_time", generation_time.AsString());
    result.SetInteger("random", key.GetRandomPart());

    const string service(key.GetServiceName());

    if (!service.empty()) {
        result.SetString("service_name", service);
    } else {
        result.SetNull("service_name");
    }

    return result;
}

CJsonNode g_WhatIs(const string& id, CCompoundIDPool id_pool)
{
    try {
        CNetStorageObjectLoc object_loc(id_pool, id);
        CJsonNode result(CJsonNode::NewObjectNode());

        result.SetString("type", TOKEN_TYPE__NETSTORAGEOBJECT_LOC);
        object_loc.ToJSON(result);
        return result;
    }
    catch (CCompoundIDException&) {
    }
    catch (CNetStorageException&) {
    }

    CNetCacheKey nc_key;

    if (CNetCacheKey::ParseBlobKey(id.c_str(), id.length(), &nc_key, id_pool)) {
        return s_GetBlobMeta(nc_key);
    }
    
    CNetScheduleKey ns_key;

    if (ns_key.ParseJobKey(id, id_pool)) {
        CJobInfoToJSON job_info_to_json;
        CJsonNode result(job_info_to_json.GetRootNode());

        result.SetString("type", TOKEN_TYPE__NETSCHEDULE_JOB_KEY);
        result.SetInteger("key_version", ns_key.version);
        job_info_to_json.ProcessJobMeta(ns_key);
        return result;
    }

    return NULL;
}

END_NCBI_SCOPE
