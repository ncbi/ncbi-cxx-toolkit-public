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
            CJsonNode::TObject object(node.GetObject());

            fputs(struct_prefix, output_stream);
            Indent(output_stream, struct_indent);
            putc('{', output_stream);
            const char* prefix = "\n";
            int indent = struct_indent + 1;
            ITERATE(CJsonNode::TObject, it, object) {
                fputs(prefix, output_stream);
                Indent(output_stream, indent);
                fprintf(output_stream, "\"%s\":",
                        NStr::PrintableString(it->first).c_str());
                PrintJSONNode(output_stream, it->second, indent, "\n", 0, " ");
                prefix = ",\n";
            }
            putc('\n', output_stream);
            Indent(output_stream, struct_indent);
            putc('}', output_stream);
        }
        break;
    case CJsonNode::eArray:
        {
            CJsonNode::TArray array(node.GetArray());

            fputs(struct_prefix, output_stream);
            Indent(output_stream, struct_indent);
            putc('[', output_stream);
            const char* prefix = "\n";
            int indent = struct_indent + 1;
            ITERATE(CJsonNode::TArray, it, array) {
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
                NStr::PrintableString(node.GetString()).c_str());
        break;
    case CJsonNode::eNumber:
        fputs(scalar_prefix, output_stream);
        Indent(output_stream, scalar_indent);
        fprintf(output_stream, "%ld", (long) node.GetNumber());
        break;
    case CJsonNode::eBoolean:
        fputs(scalar_prefix, output_stream);
        Indent(output_stream, scalar_indent);
        fputs(node.GetBoolean() ? "true" : "false", output_stream);
        break;
    default: // CJsonNode::eNull
        fputs(scalar_prefix, output_stream);
        Indent(output_stream, scalar_indent);
        fputs("null", output_stream);
    }
}

void PrintJSON(FILE* output_stream, CJsonNode node)
{
    PrintJSONNode(output_stream, node);
    putc('\n', output_stream);
}

CJsonNode ExecToJson(CNetService service, const string& command, bool multiline)
{
    CJsonNode result(CJsonNode::NewObjectNode());

    if (!multiline)
        for (CNetServiceIterator it = service.Iterate(); it; ++it)
            result.SetString((*it).GetServerAddress(),
                    (*it).ExecWithRetry(command).response);
    else
        for (CNetServiceIterator it = service.Iterate(); it; ++it) {
            CNetServerMultilineCmdOutput output(
                    (*it).ExecWithRetry(command));

            CJsonNode lines(CJsonNode::NewArrayNode());
            string line;

            while (output.ReadLine(line))
                lines.PushString(line);

            result.SetNode((*it).GetServerAddress(), lines);
        }

    return result;
}

void GetUserAndHost(string* user, string* host)
{
    char user_buf[64];
    if (CORE_GetUsername(user_buf, sizeof(user_buf)) != NULL)
        *user = user_buf;

    *host = CSocketAPI::gethostname();
}

END_NCBI_SCOPE
