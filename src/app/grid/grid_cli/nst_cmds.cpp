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
 * File Description: NetStorage-specific commands of the grid_cli application.
 *
 */

#include <ncbi_pch.hpp>

#include "grid_cli.hpp"
#include "util.hpp"

#include <sstream>

USING_NCBI_SCOPE;

void CGridCommandLineInterfaceApp::SetUp_NetStorageCmd(EAPIClass api_class,
        CGridCommandLineInterfaceApp::EAdminCmdSeverity /*cmd_severity*/)
{
    if (IsOptionSet(eNetStorage) && IsOptionSet(eDirectMode)) {
        NCBI_THROW(CArgException, eExcludedValue, "'--" DIRECT_MODE_OPTION "' "
            "cannot be used together with '--" NETSTORAGE_OPTION "'");
    }

    if (IsOptionSet(eFileTrackAPIKey) && IsOptionSet(eFileTrackToken)) {
        NCBI_THROW(CArgException, eExcludedValue, "'--" FT_TOKEN_OPTION "' "
            "cannot be used together with '--" FT_API_KEY_OPTION "'");
    }

    if (IsOptionSet(eObjectKey)) {
        if (IsOptionSet(eUserKey)) {
            NCBI_THROW(CArgException, eExcludedValue, "'--" USER_KEY_OPTION "' "
                "cannot be used together with '--" OBJECT_KEY_OPTION "'");
        }

        if (IsOptionSet(eNamespace)) {
            NCBI_THROW(CArgException, eExcludedValue, "'--" NAMESPACE_OPTION "' "
                "cannot be used together with '--" OBJECT_KEY_OPTION "'");
        }

        size_t separator = m_Opts.id.find('-');

        if (separator == string::npos) {
            NCBI_THROW_FMT(CArgException, eInvalidArg, "Could not find "
                "namespace separator in object key: " << m_Opts.id);
        }

        MarkOptionAsSet(eUserKey);
        MarkOptionAsSet(eNamespace);
        m_Opts.app_domain = m_Opts.id.substr(0, separator);
        m_Opts.id.erase(0, separator + 1);

    } else if (IsOptionSet(eUserKey)) {
        if (!IsOptionSet(eNamespace)) {
            NCBI_THROW(CArgException, eNoArg, "'--" USER_KEY_OPTION "' "
                "requires '--" NAMESPACE_OPTION "'");
        }
    }

    m_APIClass = api_class;

    if (api_class == eNetStorageAdmin && !IsOptionSet(eNetStorage)) {
        NCBI_THROW(CArgException, eNoValue, "'--" NETSTORAGE_OPTION "' "
            "must be explicitly specified.");
    }

    string init_string("mode=direct");
   
    if (IsOptionSet(eNetStorage)) {
        init_string = "nst=";
        init_string += NStr::URLEncode(m_Opts.nst_service);

    } else if (!IsOptionSet(eDirectMode) && !IsOptionSet(eUserKey) &&
            (IsOptionSet(eOptionalID) || IsOptionSet(eID))) {

        CNetStorageObjectLoc locator(CCompoundIDPool(), m_Opts.id);

        if (locator.HasServiceName()) {
            init_string = "nst=";
            init_string += NStr::URLEncode(locator.GetServiceName());
        }
    }

    if (IsOptionSet(eFileTrackSite)) {
        init_string += "&ft_site=";
        init_string += m_Opts.ft_site;
    }

    if (IsOptionSet(eFileTrackAPIKey)) {
        init_string += "&ft_key=";
        init_string += NStr::URLEncode(m_Opts.ft_key);
    }

    if (IsOptionSet(eFileTrackToken)) {
        init_string += "&ft_token=";
        init_string += NStr::URLEncode(m_Opts.ft_key);
    }

    if (IsOptionSet(eNetCache)) {
        init_string += "&nc=";
        init_string += NStr::URLEncode(m_Opts.nc_service);
    }

    if (IsOptionSet(eNamespace)) {
        init_string += "&namespace=";
        init_string += NStr::URLEncode(m_Opts.app_domain);
    }

    string auth;

    if (IsOptionSet(eAuth)) {
        auth = m_Opts.auth;
    } else {
        auth = DEFAULT_APP_UID "::" + GetDiagContext().GetUsername() + '@' +
            GetDiagContext().GetHost();
    }

    init_string += "&client=";
    init_string += NStr::URLEncode(auth);

    if (IsOptionSet(eUserKey)) {
        m_NetStorageByKey = CCombinedNetStorageByKey(init_string,
                m_Opts.netstorage_flags);
    } else {
        m_NetStorage = CCombinedNetStorage(init_string, m_Opts.netstorage_flags);

        if (api_class == eNetStorageAdmin)
            m_NetStorageAdmin = CNetStorageAdmin(m_NetStorage);
    }
}

CNetStorageObject CGridCommandLineInterfaceApp::GetNetStorageObject()
{
    SetUp_NetStorageCmd(eNetStorageAPI);

    if (IsOptionSet(eID) || IsOptionSet(eOptionalID)) {
        if (IsOptionSet(eUserKey)) {
            return m_NetStorageByKey.Open(m_Opts.id);
        } else {
            return m_NetStorage.Open(m_Opts.id);
        }
    } else {
        return m_NetStorage.Create(m_Opts.netstorage_flags);
    }
}

void CGridCommandLineInterfaceApp::NetStorage_PrintServerReply(
        CJsonNode& server_reply)
{
    for (CJsonIterator it =
            server_reply.Iterate(CJsonNode::eFlatten); it; ++it) {
        printf("%s: %s\n", it.GetKey().c_str(),
                it.GetNode().Repr(CJsonNode::fVerbatimIfString |
                        CJsonNode::fOmitOutermostBrackets).c_str());
    }
}

int CGridCommandLineInterfaceApp::PrintNetStorageServerInfo()
{
    CNetService service(m_NetStorageAdmin.GetService());

    if (m_Opts.output_format != eHumanReadable) {
        NNetStorage::CExecToJson info_to_json(m_NetStorageAdmin, "INFO");

        g_PrintJSON(stdout, g_ExecToJson(info_to_json, service,
                service.GetServiceType(), CNetService::eIncludePenalized));
    } else {
        bool print_server_address = service.IsLoadBalanced();

        for (CNetServiceIterator it =
                service.Iterate(CNetService::eIncludePenalized); it; ++it) {
            CNetServer server(*it);

            CJsonNode server_reply(m_NetStorageAdmin.ExchangeJson(
                    m_NetStorageAdmin.MkNetStorageRequest("INFO"), server));

            NNetStorage::RemoveStdReplyFields(server_reply);

            if (print_server_address)
                printf("[%s]\n", server.GetServerAddress().c_str());

            NetStorage_PrintServerReply(server_reply);

            if (print_server_address)
                printf("\n");
        }
    }

    return 0;
}

int CGridCommandLineInterfaceApp::PrintNetStorageServerConfig()
{
    CNetService service(m_NetStorageAdmin.GetService());

    for (CNetServiceIterator it =
            service.Iterate(CNetService::eIncludePenalized); it; ++it) {
        CNetServer server(*it);

        CJsonNode server_reply(m_NetStorageAdmin.ExchangeJson(
                m_NetStorageAdmin.MkNetStorageRequest("CONFIGURATION"),
                server));

        printf("[[server=%s; config_pathname=%s]]\n%s\n",
                server.GetServerAddress().c_str(),
                server_reply.GetString("ConfigurationFilePath").c_str(),
                server_reply.GetString("Configuration").c_str());
    }

    return 0;
}

int CGridCommandLineInterfaceApp::ShutdownNetStorageServer()
{
    CNetService service(m_NetStorageAdmin.GetService());

    CJsonNode shutdown_request =
            m_NetStorageAdmin.MkNetStorageRequest("SHUTDOWN");

    shutdown_request.SetString("Mode",
            IsOptionSet(eNow) || IsOptionSet(eDie) ? "hard" : "soft");

    for (CNetServiceIterator it =
            service.Iterate(CNetService::eIncludePenalized); it; ++it) {
        CNetServer server(*it);

        m_NetStorageAdmin.ExchangeJson(shutdown_request, server);
    }

    return 0;
}

int CGridCommandLineInterfaceApp::ReconfigureNetStorageServer()
{
    CNetService service(m_NetStorageAdmin.GetService());
    NNetStorage::CExecToJson reconf_to_json(m_NetStorageAdmin, "RECONFIGURE");

    g_PrintJSON(stdout, g_ExecToJson(reconf_to_json, service,
            service.GetServiceType(), CNetService::eIncludePenalized));
    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_Upload()
{
    CNetStorageObject netstorage_object(GetNetStorageObject());

    if (IsOptionSet(eInput))
        netstorage_object.Write(m_Opts.input);
    else {
        char buffer[IO_BUFFER_SIZE];

        do {
            m_Opts.input_stream->read(buffer, sizeof(buffer));

            if (m_Opts.input_stream->fail() && !m_Opts.input_stream->eof()) {
                NCBI_THROW(CIOException, eRead,
                        "Error while reading input data");
            }

            netstorage_object.Write(buffer,
                    (size_t) m_Opts.input_stream->gcount());
        } while (!m_Opts.input_stream->eof());
    }

    netstorage_object.Close();

    if (m_Opts.ttl) {
        netstorage_object.SetExpiration(CTimeout(m_Opts.ttl, 0));
    }

    const string new_loc(netstorage_object.GetLoc());

    if (!IsOptionSet(eOptionalID) || new_loc != m_Opts.id)
        PrintLine(new_loc);

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_Download()
{
    CNetStorageObject netstorage_object(GetNetStorageObject());

    char buffer[IO_BUFFER_SIZE];
    size_t bytes_read;

    while (!netstorage_object.Eof()) {
        bytes_read = netstorage_object.Read(buffer, sizeof(buffer));
        fwrite(buffer, 1, bytes_read, m_Opts.output_stream);
    }

    netstorage_object.Close();

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_Relocate()
{
    SetUp_NetStorageCmd(eNetStorageAPI);

    string new_loc;

    if (IsOptionSet(eUserKey)) {
        new_loc = m_NetStorageByKey.Relocate(m_Opts.id, m_Opts.netstorage_flags);
    } else {
        new_loc = m_NetStorage.Relocate(m_Opts.id, m_Opts.netstorage_flags);
    }

    PrintLine(new_loc);

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_NetStorageObjectInfo()
{
    CNetStorageObject netstorage_object(GetNetStorageObject());

    g_PrintJSON(stdout, netstorage_object.GetInfo().ToJSON());

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_RemoveNetStorageObject()
{
    SetUp_NetStorageCmd(eNetStorageAPI);

    if (IsOptionSet(eUserKey)) {
        m_NetStorageByKey.Remove(m_Opts.id);
    } else {
        m_NetStorage.Remove(m_Opts.id);
    }

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_GetAttrList()
{
    CNetStorageObject netstorage_object(GetNetStorageObject());

    for(const string& name : netstorage_object.GetAttributeList()) {
        fwrite(name.data(), name.size(), 1, m_Opts.output_stream);
        fwrite("\n", 1, 1, m_Opts.output_stream);
    }

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_GetAttr()
{
    CNetStorageObject netstorage_object(GetNetStorageObject());
    const string value(netstorage_object.GetAttribute(m_Opts.attr_name));

    // Either output file or cout
    fwrite(value.data(), value.size(), 1, m_Opts.output_stream);

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_SetAttr()
{
    CNetStorageObject netstorage_object(GetNetStorageObject());

    string value;

    if (IsOptionSet(eAttrValue)) {
        if (IsOptionSet(eInput)) {
            fprintf(stderr, GRID_APP_NAME ": option '--" INPUT_OPTION
                    "' and argument '" ATTR_VALUE_ARG
                    "' are mutually exclusive.\n");
            return 2;
        } else if (IsOptionSet(eInputFile)) {
            fprintf(stderr, GRID_APP_NAME ": option '--" INPUT_FILE_OPTION
                    "' and argument '" ATTR_VALUE_ARG
                    "' are mutually exclusive.\n");
            return 2;
        }

        value = m_Opts.attr_value;

    } else if (IsOptionSet(eInput)) {
        value = m_Opts.input;

    } else {
        // Either input file or cin
        ostringstream ostr;
        ostr << m_Opts.input_stream->rdbuf();
        value = ostr.str();
    }

    netstorage_object.SetAttribute(m_Opts.attr_name, value);

    return 0;
}
