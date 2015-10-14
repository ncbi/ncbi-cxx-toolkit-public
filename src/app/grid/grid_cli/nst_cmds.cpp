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

#include <connect/services/netstorage_impl.hpp>

#include "grid_cli.hpp"
#include "util.hpp"

USING_NCBI_SCOPE;

void CGridCommandLineInterfaceApp::SetUp_NetStorageCmd(EAPIClass api_class,
        CGridCommandLineInterfaceApp::EAdminCmdSeverity /*cmd_severity*/)
{
    m_APIClass = api_class;

    if (api_class == eNetStorageAdmin && !IsOptionExplicitlySet(eNetStorage)) {
        NCBI_THROW(CArgException, eNoValue, "'--" NETSTORAGE_OPTION "' "
            "must be explicitly specified.");
    }

    if (IsOptionExplicitlySet(eNetCache) && IsOptionExplicitlySet(eNamespace) &&
            !IsOptionSet(eNetStorage)) {
        m_NetICacheClient = CNetICacheClient(m_Opts.nc_service,
                m_Opts.app_domain, m_Opts.auth);
    }

    if (!IsOptionSet(eNetStorage)) {
        SFileTrackConfig ft_config(m_Opts.ft_site, m_Opts.ft_key);

        m_NetStorage = CDirectNetStorage(ft_config,
                m_NetICacheClient, m_CompoundIDPool,
                m_Opts.app_domain, m_Opts.netstorage_flags);
        if (IsOptionSet(eNamespace))
            m_NetStorageByKey = CDirectNetStorageByKey(ft_config,
                    m_NetICacheClient, m_CompoundIDPool,
                    m_Opts.app_domain, m_Opts.netstorage_flags);
    } else {
        string init_string = "nst=" + NStr::URLEncode(m_Opts.nst_service);

        if (IsOptionExplicitlySet(eNetCache)) {
            init_string += "&nc=";
            init_string += NStr::URLEncode(m_Opts.nc_service);
            init_string += "&cache=";
            init_string += NStr::URLEncode(m_Opts.app_domain);
        }

        string auth;

        if (IsOptionSet(eAuth)) {
            auth = m_Opts.auth;
        } else {
            string user, host;
            g_GetUserAndHost(&user, &host);
            auth = user + '@';
            auth += host;
        }

        init_string += "&client=";
        init_string += NStr::URLEncode(auth);

        m_NetStorage = CNetStorage(init_string, m_Opts.netstorage_flags);

        if (api_class == eNetStorageAdmin)
            m_NetStorageAdmin = CNetStorageAdmin(m_NetStorage);

        if (IsOptionSet(eNamespace)) {
            init_string += "&namespace=";
            init_string += NStr::URLEncode(m_Opts.app_domain);
            m_NetStorageByKey = CNetStorageByKey(init_string,
                    m_Opts.netstorage_flags);
        }
    }

#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
    if (IsOptionExplicitlySet(eAllowXSiteConn))
        m_NetStorage->AllowXSiteConnections();
#endif
}

static void s_NetStorage_RemoveStdReplyFields(CJsonNode& server_reply)
{
    server_reply.DeleteByKey("Type");
    server_reply.DeleteByKey("Status");
    server_reply.DeleteByKey("RE");
    server_reply.DeleteByKey("Warnings");
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

class CNetStorageExecToJson : public IExecToJson
{
public:
    CNetStorageExecToJson(CNetStorageAdmin::TInstance netstorage_admin,
            const string& command) :
        m_NetStorageAdmin(netstorage_admin),
        m_Command(command)
    {
    }

private:
    virtual CJsonNode ExecOn(CNetServer server)
    {
        CJsonNode server_reply(m_NetStorageAdmin.ExchangeJson(
                m_NetStorageAdmin.MkNetStorageRequest(m_Command), server));

        s_NetStorage_RemoveStdReplyFields(server_reply);
        return server_reply;
    }

    CNetStorageAdmin m_NetStorageAdmin;
    const string m_Command;
};

int CGridCommandLineInterfaceApp::PrintNetStorageServerInfo()
{
    CNetService service(m_NetStorageAdmin.GetService());

    if (m_Opts.output_format != eHumanReadable) {
        CNetStorageExecToJson info_to_json(m_NetStorageAdmin, "INFO");

        g_PrintJSON(stdout, g_ExecToJson(info_to_json, service,
                service.GetServiceType(), CNetService::eIncludePenalized));
    } else {
        bool print_server_address = service.IsLoadBalanced();

        for (CNetServiceIterator it =
                service.Iterate(CNetService::eIncludePenalized); it; ++it) {
            CNetServer server(*it);

            CJsonNode server_reply(m_NetStorageAdmin.ExchangeJson(
                    m_NetStorageAdmin.MkNetStorageRequest("INFO"), server));

            s_NetStorage_RemoveStdReplyFields(server_reply);

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
    CNetStorageExecToJson reconf_to_json(m_NetStorageAdmin, "RECONFIGURE");

    g_PrintJSON(stdout, g_ExecToJson(reconf_to_json, service,
            service.GetServiceType(), CNetService::eIncludePenalized));
    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_Upload()
{
    SetUp_NetStorageCmd(eNetStorageAPI);

    CNetStorageObject netstorage_object(IsOptionSet(eOptionalObjectLoc) ?
            m_NetStorage.Open(m_Opts.id) :
            m_NetStorage.Create(m_Opts.netstorage_flags));

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

    if (!IsOptionSet(eOptionalObjectLoc))
        PrintLine(netstorage_object.GetLoc());

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_Download()
{
    SetUp_NetStorageCmd(eNetStorageAPI);

    CNetStorageObject netstorage_object(m_NetStorage.Open(m_Opts.id));

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

    PrintLine(m_NetStorage.Relocate(m_Opts.id, m_Opts.netstorage_flags));

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_MkObjectLoc()
{
    SetUp_NetStorageCmd(eNetStorageAPI);

    CNetStorageObject netstorage_object;

    switch (IsOptionSet(eOptionalObjectLoc, OPTION_N(0)) |
            IsOptionSet(eObjectKey, OPTION_N(1)) |
            IsOptionSet(eNamespace, OPTION_N(2))) {
    case OPTION_N(0):
        netstorage_object = m_NetStorage.Open(m_Opts.id);
        break;

    case OPTION_N(1) + OPTION_N(2):
        netstorage_object = m_NetStorageByKey.Open(m_Opts.id,
                m_Opts.netstorage_flags);
        break;

    case 0:
        netstorage_object = m_NetStorage.Create(m_Opts.netstorage_flags);
        break;

    case OPTION_N(1):
        fprintf(stderr, GRID_APP_NAME " mkobjectloc: '--" OBJECT_KEY_OPTION
                "' requires '--" NAMESPACE_OPTION "'.\n");
        return 2;

    case OPTION_N(2):
        fprintf(stderr, GRID_APP_NAME " mkobjectloc: '--" NAMESPACE_OPTION
                "' requires '--" OBJECT_KEY_OPTION "'.\n");
        return 2;

    default:
        fprintf(stderr, GRID_APP_NAME " mkobjectloc: object locator cannot "
                "be combined with either '--" OBJECT_KEY_OPTION
                "' or '--" NAMESPACE_OPTION "'.\n");
        return 2;
    }

    netstorage_object.Close();

    PrintLine(netstorage_object.GetLoc());

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_NetStorageObjectInfo()
{
    SetUp_NetStorageCmd(eNetStorageAPI);

    CNetStorageObject netstorage_object(m_NetStorage.Open(m_Opts.id));

    g_PrintJSON(stdout, netstorage_object.GetInfo().ToJSON());

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_RemoveNetStorageObject()
{
    SetUp_NetStorageCmd(eNetStorageAPI);

    m_NetStorage.Remove(m_Opts.id);

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_GetAttr()
{
    SetUp_NetStorageCmd(eNetStorageAPI);

    PrintLine(m_NetStorage.Open(m_Opts.id).GetAttribute(m_Opts.attr_name));

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_SetAttr()
{
    SetUp_NetStorageCmd(eNetStorageAPI);

    m_NetStorage.Open(m_Opts.id).
            SetAttribute(m_Opts.attr_name, m_Opts.attr_value);

    return 0;
}
