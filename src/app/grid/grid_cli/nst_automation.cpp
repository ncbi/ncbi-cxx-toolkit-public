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
 * Authors: Rafael Sadyrov
 *
 * File Description: NetStorage automation implementation.
 *
 */

#include <ncbi_pch.hpp>

#include "nst_automation.hpp"

USING_NCBI_SCOPE;

using namespace NAutomation;

const string SNetStorageService::kName = "nstsvc";
const string SNetStorageServer::kName = "nstsrv";
const string SNetStorageObject::kName = "nstobj";

CCommand SNetStorageService::NewCommand()
{
    return CCommand(kName, ExecNew<TSelf>, {
            { "service_name", CJsonNode::eString, },
            { "domain_name", "", },
            { "client_name", "", },
            { "metadata", "", },
            { "ticket", "", },
            { "hello_service", "", },
        });
}

CCommand SNetStorageServer::NewCommand()
{
    return CCommand(kName, ExecNew<TSelf>, {
            { "service_name", CJsonNode::eString, },
            { "domain_name", "", },
            { "client_name", "", },
            { "metadata", "", },
            { "ticket", "", },
            { "hello_service", "", },
        });
}

string s_GetInitString(const TArguments& args)
{
    _ASSERT(args.size() == 6);

    const auto service_name  = args["service_name"].AsString();
    const auto domain_name   = args["domain_name"].AsString();
    const auto client_name   = args["client_name"].AsString();
    const auto metadata      = args["metadata"].AsString();
    const auto ticket        = args["ticket"].AsString();
    const auto hello_service = args["hello_service"].AsString();

    const string init_string(
            "nst=" + service_name + "&domain=" + domain_name +
            "&client=" + client_name + "&metadata=" + metadata +
            "&ticket=" + ticket + "&hello_service=" + hello_service);

    return init_string;
}

CAutomationObject* SNetStorageService::Create(const TArguments& args, CAutomationProc* automation_proc)
{
    CNetStorage nst_api(s_GetInitString(args));
    CNetStorageAdmin nst_api_admin(nst_api);
    return new SNetStorageService(automation_proc, nst_api_admin);
}

CAutomationObject* SNetStorageServer::Create(const TArguments& args, CAutomationProc* automation_proc)
{
    CNetStorage nst_api(s_GetInitString(args));
    CNetStorageAdmin nst_api_admin(nst_api);
    CNetServer server = nst_api_admin.GetService().Iterate().GetServer();
    return new SNetStorageServer(automation_proc, nst_api_admin, server);
}

SNetStorageService::SNetStorageService(
        CAutomationProc* automation_proc,
        CNetStorageAdmin nst_api) :
    SNetServiceBase(automation_proc),
    m_NetStorageAdmin(nst_api)
{
    auto warning_handler = [&](const string& m, CNetServer s) {
        auto o = m_AutomationProc->ReturnNetStorageServerObject(m_NetStorageAdmin, s);
        m_AutomationProc->SendWarning(m, o);
        return true;
    };

    GetService().SetWarningHandler(warning_handler);
}

SNetStorageServer::SNetStorageServer(
        CAutomationProc* automation_proc,
        CNetStorageAdmin nst_api, CNetServer::TInstance server) :
    SNetStorageService(automation_proc, nst_api.GetServer(server))
{
    if (GetService().IsLoadBalanced()) {
        NCBI_THROW(CAutomationException, eCommandProcessingError,
                "NetStorageServer constructor: "
                "'server_address' must be a host:port combination");
    }
}

TAutomationObjectRef CAutomationProc::ReturnNetStorageServerObject(
        CNetStorageAdmin::TInstance nst_api,
        CNetServer::TInstance server)
{
    TAutomationObjectRef object(new SNetStorageServer(this, nst_api, server));
    AddObject(object);
    return object;
}

CCommand SNetStorageService::CallCommand()
{
    return CCommand(kName, TCommandGroup(CallCommands(), CheckCall<TSelf>));
}

TCommands SNetStorageService::CallCommands()
{
    TCommands cmds =
    {
        { "clients_info",   ExecMethod<TSelf, &TSelf::ExecClientsInfo>, },
        { "users_info",     ExecMethod<TSelf, &TSelf::ExecUsersInfo>, },
        { "client_objects", ExecMethod<TSelf, &TSelf::ExecClientObjects>, {
                { "client_name", CJsonNode::eString, },
                { "limit", 0, },
            }},
        { "user_objects",   ExecMethod<TSelf, &TSelf::ExecUserObjects>, {
                { "user_name", CJsonNode::eString, },
                { "user_ns", "" },
                { "limit", 0, },
            }},
        { "open_object",    ExecMethod<TSelf, &TSelf::ExecOpenObject>, {
                { "object_loc", CJsonNode::eString, },
            }},
        { "server_info",    ExecMethod<TSelf, &TSelf::ExecServerInfo>, },
        { "get_servers",    ExecMethod<TSelf, &TSelf::ExecGetServers>, },
    };

    TCommands base_cmds = SNetServiceBase::CallCommands();
    cmds.insert(cmds.end(), base_cmds.begin(), base_cmds.end());

    return cmds;
}

void SNetStorageService::ExecRequest(const string& request_type, SInputOutput& io)
{
    auto& reply = io.reply;
    CJsonNode request(m_NetStorageAdmin.MkNetStorageRequest(request_type));
    CJsonNode response(m_NetStorageAdmin.ExchangeJson(request));
    NNetStorage::RemoveStdReplyFields(response);
    reply.Append(response);
}

void SNetStorageService::ExecClientsInfo(const TArguments&, SInputOutput& io)
{
    ExecRequest("GETCLIENTSINFO", io);
}

void SNetStorageService::ExecUsersInfo(const TArguments&, SInputOutput& io)
{
    ExecRequest("GETUSERSINFO", io);
}

void SNetStorageService::ExecClientObjects(const TArguments& args, SInputOutput& io)
{
    _ASSERT(args.size() == 2);

    auto& reply = io.reply;
    const auto client_name = args["client_name"].AsString();
    const auto limit       = args["limit"].AsInteger();
    CJsonNode request(m_NetStorageAdmin.MkNetStorageRequest("GETCLIENTOBJECTS"));
    request.SetString("ClientName", client_name);
    if (limit) request.SetInteger("Limit", limit);

    CJsonNode response(m_NetStorageAdmin.ExchangeJson(request));
    NNetStorage::RemoveStdReplyFields(response);
    reply.Append(response);
}

void SNetStorageService::ExecUserObjects(const TArguments& args, SInputOutput& io)
{
    _ASSERT(args.size() == 3);

    auto& reply = io.reply;
    const auto user_name = args["user_name"].AsString();
    const auto user_ns   = args["user_ns"].AsString();
    const auto limit     = args["limit"].AsInteger();
    CJsonNode request(m_NetStorageAdmin.MkNetStorageRequest("GETUSEROBJECTS"));
    request.SetString("UserName", user_name);
    if (!user_ns.empty()) request.SetString("UserNamespace", user_ns);
    if (limit) request.SetInteger("Limit", limit);

    CJsonNode response(m_NetStorageAdmin.ExchangeJson(request));
    NNetStorage::RemoveStdReplyFields(response);
    reply.Append(response);
}

void SNetStorageService::ExecServerInfo(const TArguments&, SInputOutput& io)
{
    auto& reply = io.reply;
    NNetStorage::CExecToJson info_to_json(m_NetStorageAdmin, "INFO");
    CNetService service(m_NetStorageAdmin.GetService());

    CJsonNode response(g_ExecToJson(info_to_json, service,
            CNetService::eIncludePenalized));
    reply.Append(response);
}

void SNetStorageService::ExecOpenObject(const TArguments& args, SInputOutput& io)
{
    _ASSERT(args.size() == 1);

    auto& reply = io.reply;
    const auto object_loc = args["object_loc"].AsString();
    CNetStorageObject object(m_NetStorageAdmin.Open(object_loc));
    TAutomationObjectRef automation_object(
            new SNetStorageObject(m_AutomationProc, object));

    TObjectID response(m_AutomationProc->AddObject(automation_object));
    reply.AppendInteger(response);
}

void SNetStorageService::ExecGetServers(const TArguments&, SInputOutput& io)
{
    auto& reply = io.reply;
    CJsonNode object_ids(CJsonNode::NewArrayNode());
    for (CNetServiceIterator it = m_NetStorageAdmin.GetService().Iterate(
            CNetService::eIncludePenalized); it; ++it)
        object_ids.AppendInteger(m_AutomationProc->
                ReturnNetStorageServerObject(m_NetStorageAdmin, *it)->GetID());
    reply.Append(object_ids);
}

CCommand SNetStorageServer::CallCommand()
{
    return CCommand(kName, TCommandGroup(CallCommands(), CheckCall<TSelf>));
}

TCommands SNetStorageServer::CallCommands()
{
    TCommands cmds =
    {
        { "health",        ExecMethod<TSelf, &TSelf::ExecHealth>, },
        { "conf",          ExecMethod<TSelf, &TSelf::ExecConf>, },
        { "metadata_info", ExecMethod<TSelf, &TSelf::ExecMetadataInfo>, },
        { "reconf",        ExecMethod<TSelf, &TSelf::ExecReconf>, },
        { "ackalert",      ExecMethod<TSelf, &TSelf::ExecAckAlert>, {
                { "name", CJsonNode::eString, },
                { "user", CJsonNode::eString, },
            }},
    };

    TCommands base_cmds = SNetStorageService::CallCommands();
    cmds.insert(cmds.end(), base_cmds.begin(), base_cmds.end());

    return cmds;
}

void SNetStorageServer::ExecHealth(const TArguments&, SInputOutput& io)
{
    ExecRequest("HEALTH", io);
}

void SNetStorageServer::ExecConf(const TArguments&, SInputOutput& io)
{
    ExecRequest("CONFIGURATION", io);
}

void SNetStorageServer::ExecMetadataInfo(const TArguments&, SInputOutput& io)
{
    ExecRequest("GETMETADATAINFO", io);
}

void SNetStorageServer::ExecReconf(const TArguments&, SInputOutput& io)
{
    ExecRequest("RECONFIGURE", io);
}

void SNetStorageServer::ExecAckAlert(const TArguments& args, SInputOutput& io)
{
    _ASSERT(args.size() == 2);

    auto& reply = io.reply;
    const auto name = args["name"].AsString();
    const auto user = args["user"].AsString();

    CJsonNode request(m_NetStorageAdmin.MkNetStorageRequest("ACKALERT"));
    request.SetString("Name", name);
    request.SetString("User", user);

    CJsonNode response(m_NetStorageAdmin.ExchangeJson(request));
    NNetStorage::RemoveStdReplyFields(response);
    reply.Append(response);
}

SNetStorageObject::SNetStorageObject(
        CAutomationProc* automation_proc, CNetStorageObject::TInstance object) :
    CAutomationObject(automation_proc),
    m_Object(object)
{
}

CCommand SNetStorageObject::CallCommand()
{
    return CCommand(kName, TCommandGroup(CallCommands(), CheckCall<TSelf>));
}

TCommands SNetStorageObject::CallCommands()
{
    TCommands cmds =
    {
        { "info",      ExecMethod<TSelf, &TSelf::ExecInfo>, },
        { "attr_list", ExecMethod<TSelf, &TSelf::ExecAttrList>, },
        { "get_attr",  ExecMethod<TSelf, &TSelf::ExecGetAttr>, {
                { "attr_name", CJsonNode::eString, },
            }},
    };

    return cmds;
}

void SNetStorageObject::ExecInfo(const TArguments&, SInputOutput& io)
{
    auto& reply = io.reply;
    CNetStorageObjectInfo object_info(m_Object.GetInfo());
    CJsonNode response(object_info.ToJSON());
    reply.Append(response);
}

void SNetStorageObject::ExecAttrList(const TArguments&, SInputOutput& io)
{
    auto& reply = io.reply;
    CNetStorageObject::TAttributeList attr_list(m_Object.GetAttributeList());
    CJsonNode response(CJsonNode::eArray);
    for (auto& attr : attr_list) response.AppendString(attr);
    reply.Append(response);
}

void SNetStorageObject::ExecGetAttr(const TArguments& args, SInputOutput& io)
{
    _ASSERT(args.size() == 1);

    auto& reply = io.reply;
    const auto attr_name = args["attr_name"].AsString();
    CJsonNode response(m_Object.GetAttribute(attr_name));
    reply.Append(response);
}
