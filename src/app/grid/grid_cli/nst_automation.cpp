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

const string SNetStorageServiceAutomationObject::kName = "nstsvc";
const string SNetStorageServerAutomationObject::kName = "nstsrv";

NAutomation::CCommand SNetStorageServiceAutomationObject::NewCommand(
        const string& name)
{
    return NAutomation::CCommand(name, {
            { "service_name", CJsonNode::eString, },
            { "domain_name", "", },
            { "client_name", "", },
            { "metadata", "", },
            { "ticket", "", },
            { "hello_service", "", },
        });
}

string s_GetInitString(CArgArray& arg_array)
{
    const string service_name(arg_array.NextString());
    const string domain_name(arg_array.NextString(kEmptyStr));
    const string client_name(arg_array.NextString(kEmptyStr));
    const string metadata(arg_array.NextString(kEmptyStr));
    const string ticket(arg_array.NextString(kEmptyStr));
    const string hello_service(arg_array.NextString(kEmptyStr));
    const string init_string(
            "nst=" + service_name + "&domain=" + domain_name +
            "&client=" + client_name + "&metadata=" + metadata +
            "&ticket=" + ticket + "&hello_service=" + hello_service);

    return init_string;
}

CAutomationObject* SNetStorageServiceAutomationObject::Create(
        CArgArray& arg_array, const string& class_name,
        CAutomationProc* automation_proc)
{
    if (class_name != kName) return nullptr;

    CNetStorage nst_api(s_GetInitString(arg_array));
    CNetStorageAdmin nst_api_admin(nst_api);
    return new SNetStorageServiceAutomationObject(automation_proc, nst_api_admin,
            CNetService::eLoadBalancedService);
}

CAutomationObject* SNetStorageServerAutomationObject::Create(
        CArgArray& arg_array, const string& class_name,
        CAutomationProc* automation_proc)
{
    if (class_name != kName) return nullptr;

    CNetStorage nst_api(s_GetInitString(arg_array));
    CNetStorageAdmin nst_api_admin(nst_api);
    CNetServer server = nst_api_admin.GetService().Iterate().GetServer();
    return new SNetStorageServerAutomationObject(automation_proc, nst_api_admin, server);
}

SNetStorageServiceAutomationObject::SNetStorageServiceAutomationObject(
        CAutomationProc* automation_proc,
        CNetStorageAdmin nst_api, CNetService::EServiceType type) :
    TBase(automation_proc, type),
    m_NetStorageAdmin(nst_api)
{
    m_Service = m_NetStorageAdmin.GetService();
    m_NetStorageAdmin.SetEventHandler(
            new CEventHandler(automation_proc, m_NetStorageAdmin));
}

void SNetStorageServiceAutomationObject::CEventHandler::OnWarning(
        const string& warn_msg, CNetServer server)
{
    m_AutomationProc->SendWarning(warn_msg, m_AutomationProc->
            ReturnNetStorageServerObject(m_NetStorageAdmin, server));
}

const void* SNetStorageServiceAutomationObject::GetImplPtr() const
{
    return m_NetStorageAdmin;
}

SNetStorageServerAutomationObject::SNetStorageServerAutomationObject(
        CAutomationProc* automation_proc,
        CNetStorageAdmin nst_api, CNetServer::TInstance server) :
    TBase(automation_proc, nst_api.GetServer(server),
            CNetService::eSingleServerService),
    m_NetServer(server)
{
    if (m_Service.IsLoadBalanced()) {
        NCBI_THROW(CAutomationException, eCommandProcessingError,
                "NetStorageServer constructor: "
                "'server_address' must be a host:port combination");
    }
}

const void* SNetStorageServerAutomationObject::GetImplPtr() const
{
    return m_NetServer;
}

TAutomationObjectRef CAutomationProc::ReturnNetStorageServerObject(
        CNetStorageAdmin::TInstance nst_api,
        CNetServer::TInstance server)
{
    TAutomationObjectRef object(FindObjectByPtr(server));
    if (!object) {
        object = new SNetStorageServerAutomationObject(this, nst_api, server);
        AddObject(object, server);
    }
    return object;
}

NAutomation::CCommand SNetStorageServiceAutomationObject::CallCommand()
{
    return NAutomation::CCommand(kName, CallCommands);
}

NAutomation::TCommands SNetStorageServiceAutomationObject::CallCommands()
{
    NAutomation::TCommands cmds =
    {
        { "clients_info", },
        { "users_info", },
        { "client_objects", {
                { "client_name", CJsonNode::eString, },
                { "limit", 0, },
            }},
        { "user_objects", {
                { "user_name", CJsonNode::eString, },
                { "user_ns", "" },
                { "limit", 0, },
            }},
        { "object_info", {
                { "object_loc", CJsonNode::eString, },
            }},
        { "attr_list", {
                { "object_loc", CJsonNode::eString, },
            }},
        { "attr_value", {
                { "object_loc", CJsonNode::eString, },
                { "attr_name", CJsonNode::eString, },
            }},
        { "server_info", },
        { "get_servers", },
#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
        { "allow_xsite_connections", },
#endif
    };

    NAutomation::TCommands base_cmds = TBase::CallCommands();
    cmds.insert(cmds.end(), base_cmds.begin(), base_cmds.end());

    return cmds;
}

bool SNetStorageServiceAutomationObject::Call(const string& method,
        CArgArray& arg_array, CJsonNode& reply)
{
    map<string, string> no_param_commands =
    {
        { "clients_info",   "GETCLIENTSINFO" },
        { "users_info",     "GETUSERSINFO" },
    };

    auto found = no_param_commands.find(method);
    if (found != no_param_commands.end()) {
        CJsonNode request(m_NetStorageAdmin.MkNetStorageRequest(found->second));

        CJsonNode response(m_NetStorageAdmin.ExchangeJson(request));
        NNetStorage::RemoveStdReplyFields(response);
        reply.Append(response);
    } else if (method == "client_objects") {
        string client_name(arg_array.NextString());
        Int8 limit(arg_array.NextInteger(0));

        CJsonNode request(m_NetStorageAdmin.MkNetStorageRequest("GETCLIENTOBJECTS"));
        request.SetString("ClientName", client_name);
        if (limit) request.SetInteger("Limit", limit);

        CJsonNode response(m_NetStorageAdmin.ExchangeJson(request));
        NNetStorage::RemoveStdReplyFields(response);
        reply.Append(response);
    } else if (method == "user_objects") {
        string user_name(arg_array.NextString());
        string user_ns(arg_array.NextString(kEmptyStr));
        Int8 limit(arg_array.NextInteger(0));

        CJsonNode request(m_NetStorageAdmin.MkNetStorageRequest("GETUSEROBJECTS"));
        request.SetString("UserName", user_name);
        if (!user_ns.empty()) request.SetString("UserNamespace", user_ns);
        if (limit) request.SetInteger("Limit", limit);

        CJsonNode response(m_NetStorageAdmin.ExchangeJson(request));
        NNetStorage::RemoveStdReplyFields(response);
        reply.Append(response);
    } else if (method == "server_info") {
        NNetStorage::CExecToJson info_to_json(m_NetStorageAdmin, "INFO");
        CNetService service(m_NetStorageAdmin.GetService());

        CJsonNode response(g_ExecToJson(info_to_json, service,
                m_ActualServiceType, CNetService::eIncludePenalized));
        reply.Append(response);
    } else if (method == "object_info") {
        const string object_loc(arg_array.NextString());
        CNetStorageObject object(m_NetStorageAdmin.Open(object_loc));
        CNetStorageObjectInfo object_info(object.GetInfo());
        CJsonNode response(object_info.ToJSON());
        reply.Append(response);
    } else if (method == "attr_list") {
        const string object_loc(arg_array.NextString());
        CNetStorageObject object(m_NetStorageAdmin.Open(object_loc));
        CNetStorageObject::TAttributeList attr_list(object.GetAttributeList());
        CJsonNode response(CJsonNode::eArray);
        for (auto& attr : attr_list) response.AppendString(attr);
        reply.Append(response);
    } else if (method == "attr_value") {
        const string object_loc(arg_array.NextString());
        const string attr_name(arg_array.NextString());
        CNetStorageObject object(m_NetStorageAdmin.Open(object_loc));
        CJsonNode response(object.GetAttribute(attr_name));
        reply.Append(response);
    } else if (method == "get_servers") {
        CJsonNode object_ids(CJsonNode::NewArrayNode());
        for (CNetServiceIterator it = m_NetStorageAdmin.GetService().Iterate(
                CNetService::eIncludePenalized); it; ++it)
            object_ids.AppendInteger(m_AutomationProc->
                    ReturnNetStorageServerObject(m_NetStorageAdmin, *it)->GetID());
        reply.Append(object_ids);
    } else
#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
        if (method == "allow_xsite_connections")
            g_AllowXSiteConnections(m_NetStorageAdmin);
        else
#endif
        return TBase::Call(method, arg_array, reply);

    return true;
}

NAutomation::CCommand SNetStorageServerAutomationObject::CallCommand()
{
    return NAutomation::CCommand(kName, CallCommands);
}

NAutomation::TCommands SNetStorageServerAutomationObject::CallCommands()
{
    NAutomation::TCommands cmds =
    {
        { "health", },
        { "conf", },
        { "metadata_info", },
        { "reconf", },
        { "ackalert", {
                { "name", CJsonNode::eString, },
                { "user", CJsonNode::eString, },
            }},
    };

    NAutomation::TCommands base_cmds = TBase::CallCommands();
    cmds.insert(cmds.end(), base_cmds.begin(), base_cmds.end());

    return cmds;
}

bool SNetStorageServerAutomationObject::Call(const string& method,
        CArgArray& arg_array, CJsonNode& reply)
{
    map<string, string> no_param_commands =
    {
        { "health",         "HEALTH" },
        { "conf",           "CONFIGURATION" },
        { "metadata_info",  "GETMETADATAINFO" },
        { "reconf",         "RECONFIGURE" },
    };

    auto found = no_param_commands.find(method);
    if (found != no_param_commands.end()) {
        CJsonNode request(m_NetStorageAdmin.MkNetStorageRequest(found->second));

        CJsonNode response(m_NetStorageAdmin.ExchangeJson(request));
        NNetStorage::RemoveStdReplyFields(response);
        reply.Append(response);
    } else if (method == "ackalert") {
        string name(arg_array.NextString());
        string user(arg_array.NextString());

        CJsonNode request(m_NetStorageAdmin.MkNetStorageRequest("ACKALERT"));
        request.SetString("Name", name);
        request.SetString("User", user);

        CJsonNode response(m_NetStorageAdmin.ExchangeJson(request));
        NNetStorage::RemoveStdReplyFields(response);
        reply.Append(response);
    } else
        return TBase::Call(method, arg_array, reply);

    return true;
}
