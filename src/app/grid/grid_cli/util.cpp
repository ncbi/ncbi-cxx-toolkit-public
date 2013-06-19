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

#include <connect/ncbi_util.h>

BEGIN_NCBI_SCOPE

static void Indent(FILE* output_stream, int indent)
{
    while (--indent >= 0)
        putc('\t', output_stream);
}

static void PrintJSONNode(FILE* output_stream, CJsonNode node,
        int struct_indent = 0, const char* struct_prefix = "",
        int scalar_indent = 0, const char* scalar_prefix = "")
{
    switch (node.GetNodeType()) {
    case CJsonNode::eObject:
        {
            fputs(struct_prefix, output_stream);
            Indent(output_stream, struct_indent);
            putc('{', output_stream);
            const char* prefix = "\n";
            int indent = struct_indent + 1;
            for (CJsonIterator it = node.Iterate(); it; ++it) {
                fputs(prefix, output_stream);
                Indent(output_stream, indent);
                fprintf(output_stream, "\"%s\":",
                        NStr::PrintableString(it.GetKey()).c_str());
                PrintJSONNode(output_stream, *it, indent, "\n", 0, " ");
                prefix = ",\n";
            }
            putc('\n', output_stream);
            Indent(output_stream, struct_indent);
            putc('}', output_stream);
        }
        break;
    case CJsonNode::eArray:
        {
            fputs(struct_prefix, output_stream);
            Indent(output_stream, struct_indent);
            putc('[', output_stream);
            const char* prefix = "\n";
            int indent = struct_indent + 1;
            for (CJsonIterator it = node.Iterate(); it; ++it) {
                fputs(prefix, output_stream);
                PrintJSONNode(output_stream, *it, indent, "", indent, "");
                prefix = ",\n";
            }
            putc('\n', output_stream);
            Indent(output_stream, struct_indent);
            putc(']', output_stream);
        }
        break;
    case CJsonNode::eString:
        fputs(scalar_prefix, output_stream);
        Indent(output_stream, scalar_indent);
        fprintf(output_stream, "\"%s\"",
                NStr::PrintableString(node.AsString()).c_str());
        break;
    case CJsonNode::eInteger:
        fputs(scalar_prefix, output_stream);
        Indent(output_stream, scalar_indent);
        fprintf(output_stream, "%lld", (long long) node.AsInteger());
        break;
    case CJsonNode::eDouble:
        fputs(scalar_prefix, output_stream);
        Indent(output_stream, scalar_indent);
        fprintf(output_stream, "%.10g", node.AsDouble());
        break;
    case CJsonNode::eBoolean:
        fputs(scalar_prefix, output_stream);
        Indent(output_stream, scalar_indent);
        fputs(node.AsBoolean() ? "true" : "false", output_stream);
        break;
    default: // CJsonNode::eNull
        fputs(scalar_prefix, output_stream);
        Indent(output_stream, scalar_indent);
        fputs("null", output_stream);
    }
}

void g_PrintJSON(FILE* output_stream, CJsonNode node)
{
    PrintJSONNode(output_stream, node);
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
    virtual CJsonNode ExecOn(CNetServer server);
};

CJsonNode SServerInfoToJson::ExecOn(CNetServer server)
{
    CJsonNode server_info_node(CJsonNode::NewObjectNode());

    CNetServerInfo server_info(server.GetServerInfo());

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
            if (attr_name == "NCBI NetSchedule server version" ||
                    attr_name == "NCBI NetCache server version")
            {
                old_format = eOn;
                attr_name = "server_version";
                break;
            } else
                old_format = eOff;
            /* FALL THROUGH */

        case eOff:
            if (attr_name == "version")
                attr_name = "server_version";
        }

        server_info_node.SetString(attr_name, attr_value);
    }

    return server_info_node;
}

CJsonNode g_ServerInfoToJson(CNetService service,
        CNetService::EServiceType service_type)
{
    SServerInfoToJson server_info_proc;

    return g_ExecToJson(server_info_proc, service,
            service_type, CNetService::eIncludePenalized);
}

void g_GetUserAndHost(string* user, string* host)
{
    char user_buf[64];
    if (CORE_GetUsername(user_buf, sizeof(user_buf)) != NULL)
        *user = user_buf;

    *host = CSocketAPI::gethostname();
}

END_NCBI_SCOPE
