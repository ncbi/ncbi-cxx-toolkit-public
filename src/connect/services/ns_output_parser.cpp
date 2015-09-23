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
 * File Description: NetSchedule output parsers - implementation.
 *
 */

#include <ncbi_pch.hpp>

#include <sstream>
#include <corelib/rwstream.hpp>
#include <cgi/ncbicgi.hpp>
#include <connect/services/ns_output_parser.hpp>
#include <connect/services/grid_rw_impl.hpp>
#include <connect/services/remote_app.hpp>


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

#define INVALID_FORMAT_ERROR() \
    NCBI_THROW2(CStringException, eFormat, \
            (*m_Ch == '\0' ? "Unexpected end of NetSchedule output" : \
                    "Syntax error in structured NetSchedule output"), \
            GetPosition())

CJsonNode CNetScheduleStructuredOutputParser::ParseJSON(const string& json)
{
    m_Ch = (m_NSOutput = json).c_str();

    while (isspace((unsigned char) *m_Ch))
        ++m_Ch;

    CJsonNode root;

    switch (*m_Ch) {
    case '[':
        ++m_Ch;
        root = ParseArray(']');
        break;

    case '{':
        ++m_Ch;
        root = ParseObject('}');
        break;

    default:
        INVALID_FORMAT_ERROR();
    }

    while (isspace((unsigned char) *m_Ch))
        ++m_Ch;

    if (*m_Ch != '\0') {
        INVALID_FORMAT_ERROR();
    }

    return root;
}

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
    while (isspace((unsigned char) *m_Ch))
        ++m_Ch;
    if (*m_Ch != ',')
        return false;
    while (isspace((unsigned char) *++m_Ch))
        ;
    return true;
}

CJsonNode CNetScheduleStructuredOutputParser::ParseObject(char closing_char)
{
    CJsonNode result(CJsonNode::NewObjectNode());

    while (isspace((unsigned char) *m_Ch))
        ++m_Ch;

    if (*m_Ch == closing_char) {
        ++m_Ch;
        return result;
    }

    while (*m_Ch == '\'' || *m_Ch == '"') {
        // New attribute/value pair
        string attr_name(ParseString(GetRemainder()));

        while (isspace((unsigned char) *m_Ch))
            ++m_Ch;
        if (*m_Ch == ':' || *m_Ch == '=')
            while (isspace((unsigned char) *++m_Ch))
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

    while (isspace((unsigned char) *m_Ch))
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
        if (max_len <= 1 || !isdigit((unsigned char) m_Ch[1])) {
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
        while (isdigit((unsigned char) m_Ch[len]));

        // Stumbled upon a non-digit character -- check
        // if it's a fraction part or an exponent part.
        switch (m_Ch[len]) {
        case '.':
            if (++len == max_len || !isdigit((unsigned char) m_Ch[len])) {
                NCBI_THROW2(CStringException, eFormat,
                        "At least one digit after the decimal "
                        "point is required", GetPosition());
            }
            for (;;) {
                if (++len == max_len)
                    return CJsonNode::NewDoubleNode(ParseDouble(len));

                if (!isdigit((unsigned char) m_Ch[len])) {
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
                            ++len == max_len ||
                                    !isdigit((unsigned char) m_Ch[len]) :
                            !isdigit((unsigned char) m_Ch[len]))) {
                m_Ch += len;
                NCBI_THROW2(CStringException, eFormat,
                        "Invalid exponent specification", GetPosition());
            }
            while (++len < max_len && isdigit((unsigned char) m_Ch[len]))
                ;
            return CJsonNode::NewDoubleNode(ParseDouble(len));

        default:
            return CJsonNode::NewIntegerNode(ParseInt(len));
        }

    /* Constant */
    case 'F': case 'f': case 'N': case 'n':
    case 'T': case 't': case 'Y': case 'y':
        while (len <= max_len && isalpha((unsigned char) m_Ch[len]))
            ++len;

        {
            CTempString val(m_Ch, len);
            m_Ch += len;
            return val == "null" ? CJsonNode::NewNullNode() :
                CJsonNode::NewBooleanNode(NStr::StringToBool(val));
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

CAttrListParser::ENextAttributeType CAttrListParser::NextAttribute(
    CTempString* attr_name, string* attr_value, size_t* attr_column)
{
    while (isspace(*m_Position))
        ++m_Position;

    if (*m_Position == '\0')
        return eNoMoreAttributes;

    const char* attr_start = m_Position;
    *attr_column = attr_start - m_Start + 1;

    for (;;)
        if (*m_Position == '=') {
            attr_name->assign(attr_start, m_Position - attr_start);
            break;
        } else if (isspace(*m_Position)) {
            attr_name->assign(attr_start, m_Position - attr_start);
            while (isspace(*++m_Position))
                ;
            if (*m_Position == '=')
                break;
            else
                return eStandAloneAttribute;
        } else if (*++m_Position == '\0') {
            attr_name->assign(attr_start, m_Position - attr_start);
            return eStandAloneAttribute;
        }

    // Skip the equals sign and the spaces that may follow it.
    while (isspace(*++m_Position))
        ;

    attr_start = m_Position;

    switch (*m_Position) {
    case '\0':
        NCBI_THROW_FMT(CArgException, eInvalidArg,
            "empty attribute value must be specified as " <<
                attr_name << "=\"\"");
    case '\'':
    case '"':
        {
            size_t n_read;
            *attr_value = NStr::ParseQuoted(CTempString(attr_start,
                m_EOL - attr_start), &n_read);
            m_Position += n_read;
        }
        break;
    default:
        while (*++m_Position != '\0' && !isspace(*m_Position))
            ;
        *attr_value = NStr::ParseEscapes(
            CTempString(attr_start, m_Position - attr_start));
    }

    return eAttributeWithValue;
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

struct SEmbeddedDataParser : public CBlobStreamHelper
{
    static bool GetFileName(string& data)
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
};

struct SDataDetector : public CStringOrBlobStorageReader
{
private:
    static CJsonNode x_FillNode(EType type,
            const string& data)
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
        // Do not combine calls to x_GetDataType and x_FillNode,
        // as x_GetDataType modifies data
        EType type = x_GetDataType(data);
        parent.SetByKey(name, x_FillNode(type, data));
        return type != eRaw && type != eEmpty;
    }

    static void AddNodes(CJsonNode parent, const string& name, const string& src)
    {
        if (AddNode(parent, name, src)) {
            parent.SetString("raw_" + name, src);
        }
    }

    static CJsonNode CreateRemoteAppNode(string data)
    {
        EType type = x_GetDataType(data);

        if (type == eEmbedded && SEmbeddedDataParser::GetFileName(data)) {
            CJsonNode node(CJsonNode::NewObjectNode());
            node.SetString("storage", "localfile");
            node.SetString("filename", data);
            return node;
        }

        return x_FillNode(type, data);
    }

    static CJsonNode CreateArgsNode(const string& data, CJsonNode files)
    {
        CJsonNode node(CJsonNode::NewObjectNode());
        node.SetString("storage", "embedded");
        node.SetString("embedded_data", data);
        if (files) node.SetByKey("files", files);
        return node;
    }
};

struct SPartialDeserializer : public CRemoteAppRequest
{
    SPartialDeserializer(CNcbiIstream& is)
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

struct SStringStream : private CStringOrBlobStorageReader, public CRStream
{
    SStringStream(const string& data)
        : CStringOrBlobStorageReader(data, NULL),
          CRStream(this, 0, 0, CRWStreambuf::fLeakExceptions)
    {
        exceptions(IOS_BASE::badbit | IOS_BASE::failbit);
    }
};

static const char* s_JobType = "job_type";
static const char* s_RemoteApp = "remote_app";
static const char* s_RemoteCgi = "remote_cgi";

void CJobInfoToJSON::ProcessInput(const string& data)
{
    SDataDetector::AddNodes(m_JobInfo, "input", data);

    try {
        SStringStream stream(data);
        SPartialDeserializer request(stream);

        CJsonNodeUpdater updater(s_RemoteApp, m_JobInfo);
        updater.node.SetInteger("run_timeout", request.GetAppRunTimeout());
        updater.node.SetBoolean("exclusive", request.IsExclusiveMode());
        updater.node.SetByKey("args",
                SDataDetector::CreateArgsNode(
                    request.GetCmdLine(), request.CreateFilesNode()));
        updater.node.SetByKey("stdin",
                SDataDetector::CreateRemoteAppNode(request.GetInBlobIdOrData()));
        m_JobInfo.SetString(s_JobType, s_RemoteApp);
        return;
    }
    catch (...) {
    }
    
    try {
        SStringStream stream(data);
        CCgiRequest request(stream,
                CCgiRequest::fIgnoreQueryString |
                CCgiRequest::fDoNotParseContent);

        CJsonNodeUpdater updater(s_RemoteCgi, m_JobInfo);
        updater.node.SetString("method", request.GetRequestMethodName());

        const CNcbiEnvironment& env(request.GetEnvironment());
        list<string> names;
        env.Enumerate(names);
        CJsonNodeUpdater env_updater("env", updater.node);

        for (list<string>::iterator i = names.begin(); i != names.end(); ++i) {
            env_updater.node.SetString(*i, env.Get(*i));
        }

        m_JobInfo.SetString(s_JobType, s_RemoteCgi);
    }
    catch (...) {
        m_JobInfo.SetString(s_JobType, "generic");
    }
}

void CJobInfoToJSON::ProcessOutput(const string& data)
{
    SDataDetector::AddNodes(m_JobInfo, "output", data);

    try {
        SStringStream stream(data);
        const string job_type(m_JobInfo.GetString(s_JobType));

        if (job_type == s_RemoteApp) {
            CRemoteAppResult result(NULL);
            result.Receive(stream);

            CJsonNodeUpdater updater(s_RemoteApp, m_JobInfo);
            updater.node.SetByKey("stdout",
                    SDataDetector::CreateRemoteAppNode(result.GetOutBlobIdOrData()));
            updater.node.SetByKey("stderr",
                    SDataDetector::CreateRemoteAppNode(result.GetErrBlobIdOrData()));
            updater.node.SetInteger("exit_code", result.GetRetCode());
        } else if (job_type == s_RemoteCgi) {
            string field;
            getline(stream, field);
            stream >> field;
            int status = 200;
            if (field == "Status:") stream >> status;

            CJsonNodeUpdater updater(s_RemoteCgi, m_JobInfo);
            updater.node.SetInteger("status", status);
            SDataDetector::AddNode(updater.node, "stdout", data);
        }
    }
    catch (...) {
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

END_NCBI_SCOPE
