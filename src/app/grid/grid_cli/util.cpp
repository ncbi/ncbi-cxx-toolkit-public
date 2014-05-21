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

#include <connect/services/ns_output_parser.hpp>

#include <connect/ncbi_util.h>

BEGIN_NCBI_SCOPE

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
        return CJsonNode::NewStringNode(server.ExecWithRetry(m_Cmd).response);

    CNetServerMultilineCmdOutput output(server.ExecWithRetry(m_Cmd));

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
        else if (c == ' ')
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
    CNetServerMultilineCmdOutput output(worker_node.ExecWithRetry("STAT"));

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
            if ((pos = NStr::Find(line, TEMP_STRING_CTOR("running for "),
                    0, NPOS, NStr::eLast, NStr::eNocase)) != NPOS) {
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

    g_AppendClientIPAndSessionID(cmd);

    netschedule_api.GetService().ExecOnAllServers(cmd);
}

void g_ResumeNetSchedule(CNetScheduleAPI netschedule_api)
{
    string cmd("QRESUME");

    g_AppendClientIPAndSessionID(cmd);

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
    worker_node.ExecWithRetry(cmd);
}

void g_ResumeWorkerNode(CNetServer worker_node)
{
    worker_node.ExecWithRetry("RESUME");
}

void g_GetUserAndHost(string* user, string* host)
{
    char user_buf[64];
    if (CORE_GetUsername(user_buf, sizeof(user_buf)) != NULL)
        *user = user_buf;

    *host = CSocketAPI::gethostname();
}

END_NCBI_SCOPE
