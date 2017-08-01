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
 * File Description: NetSchedule automation implementation.
 *
 */

#include <ncbi_pch.hpp>

#include "ns_automation.hpp"

USING_NCBI_SCOPE;

using namespace NAutomation;

const string SNetScheduleService::kName = "nssvc";
const string SNetScheduleServer::kName = "nssrv";

const void* SNetScheduleService::GetImplPtr() const
{
    return m_NetScheduleAPI;
}

SNetScheduleService::SNetScheduleService(
        CAutomationProc* automation_proc,
        CNetScheduleAPI ns_api, CNetService::EServiceType type) :
    SNetService(automation_proc, type),
    m_NetScheduleAPI(ns_api)
{
    m_Service = m_NetScheduleAPI.GetService();
    m_NetScheduleAPI.SetEventHandler(
            new CEventHandler(automation_proc, m_NetScheduleAPI));
}

SNetScheduleServer::SNetScheduleServer(
        CAutomationProc* automation_proc,
        CNetScheduleAPIExt ns_api, CNetServer::TInstance server) :
    SNetScheduleService(automation_proc, ns_api.GetServer(server),
            CNetService::eSingleServerService),
    m_NetServer(server)
{
    if (m_Service.IsLoadBalanced()) {
        NCBI_THROW(CAutomationException, eCommandProcessingError,
                "NetScheduleServer constructor: "
                "'server_address' must be a host:port combination");
    }
}

CCommand SNetScheduleService::NewCommand()
{
    return CCommand(kName, ExecNew<TSelf>, {
            { "service_name", "", },
            { "queue_name", "", },
            { "client_name", "", },
        });
}

CAutomationObject* SNetScheduleService::Create(const TArguments& args, CAutomationProc* automation_proc)
{
    _ASSERT(args.size() == 3);

    const auto service_name = args[0].AsString();
    const auto queue_name   = args[1].AsString();
    const auto client_name  = args[2].AsString();

    CNetScheduleAPIExt ns_api(CNetScheduleAPIExt::CreateNoCfgLoad(
                service_name, client_name, queue_name));

    return new SNetScheduleService(automation_proc, ns_api,
            CNetService::eLoadBalancedService);
}

CCommand SNetScheduleServer::NewCommand()
{
    return CCommand(kName, ExecNew<TSelf>, {
            { "service_name", "", },
            { "queue_name", "", },
            { "client_name", "", },
        });
}

CAutomationObject* SNetScheduleServer::Create(const TArguments& args, CAutomationProc* automation_proc)
{
    _ASSERT(args.size() == 3);

    const auto service_name = args[0].AsString();
    const auto queue_name   = args[1].AsString();
    const auto client_name  = args[2].AsString();

    CNetScheduleAPIExt ns_api(CNetScheduleAPIExt::CreateNoCfgLoad(
                service_name, client_name, queue_name));

    CNetServer server = ns_api.GetService().Iterate().GetServer();
    return new SNetScheduleServer(automation_proc, ns_api, server);
}

const void* SNetScheduleServer::GetImplPtr() const
{
    return m_NetServer;
}

TAutomationObjectRef CAutomationProc::ReturnNetScheduleServerObject(
        CNetScheduleAPI::TInstance ns_api,
        CNetServer::TInstance server)
{
    TAutomationObjectRef object(FindObjectByPtr(server));
    if (!object) {
        object = new SNetScheduleServer(this, ns_api, server);
        AddObject(object, server);
    }
    return object;
}

CCommand SNetScheduleServer::CallCommand()
{
    return CCommand(kName, TCommandGroup(CallCommands, CheckCall<TSelf>));
}

TCommands SNetScheduleServer::CallCommands()
{
    TCommands cmds =
    {
        { "server_status",               ExecMethod<TSelf, &TSelf::ExecServerStatus>, {
                { "verbose", false, },
            }},
        { "job_group_info",              ExecMethod<TSelf, &TSelf::ExecJobGroupInfo>, {
                { "verbose", false, },
            }},
        { "client_info",                 ExecMethod<TSelf, &TSelf::ExecClientInfo>, {
                { "verbose", false, },
            }},
        { "notification_info",           ExecMethod<TSelf, &TSelf::ExecNotificationInfo>, {
                { "verbose", false, },
            }},
        { "affinity_info",               ExecMethod<TSelf, &TSelf::ExecAffinityInfo>, {
                { "verbose", false, },
            }},
        { "change_preferred_affinities", ExecMethod<TSelf, &TSelf::ExecChangePreferredAffinities>, {
                { "affs_to_add", CJsonNode::eArray, },
                { "affs_to_del", CJsonNode::eArray, },
            }},
    };

    TCommands base_cmds = SNetScheduleService::CallCommands();
    cmds.insert(cmds.end(), base_cmds.begin(), base_cmds.end());

    return cmds;
}

void SNetScheduleServer::ExecServerStatus(const TArguments& args, SInputOutput& io)
{
    _ASSERT(args.size() == 1);

    auto& reply = io.reply;
    const auto verbose = args[0].AsBoolean();
    reply.Append(g_LegacyStatToJson(m_NetServer, verbose));
}

void SNetScheduleServer::ExecJobGroupInfo(const TArguments& args, SInputOutput& io)
{
    _ASSERT(args.size() == 1);

    auto& reply = io.reply;
    const auto verbose = args[0].AsBoolean();
    reply.Append(g_GenericStatToJson(m_NetServer, eNetScheduleStatJobGroups, verbose));
}

void SNetScheduleServer::ExecClientInfo(const TArguments& args, SInputOutput& io)
{
    _ASSERT(args.size() == 1);

    auto& reply = io.reply;
    const auto verbose = args[0].AsBoolean();
    reply.Append(g_GenericStatToJson(m_NetServer, eNetScheduleStatClients, verbose));
}

void SNetScheduleServer::ExecNotificationInfo(const TArguments& args, SInputOutput& io)
{
    _ASSERT(args.size() == 1);

    auto& reply = io.reply;
    const auto verbose = args[0].AsBoolean();
    reply.Append(g_GenericStatToJson(m_NetServer, eNetScheduleStatNotifications, verbose));
}

void SNetScheduleServer::ExecAffinityInfo(const TArguments& args, SInputOutput& io)
{
    _ASSERT(args.size() == 1);

    auto& reply = io.reply;
    const auto verbose = args[0].AsBoolean();
    reply.Append(g_GenericStatToJson(m_NetServer, eNetScheduleStatAffinities, verbose));
}

vector<string> s_ExtractVectorOfStrings(CJsonNode& arg)
{
    vector<string> result;

    if (!arg.IsNull()) {
        for (CJsonIterator it = arg.Iterate(); it; ++it) {
            CArgument affinity("affinity", CJsonNode::eString);
            affinity.Exec("change_preferred_affinities", it);
            result.push_back(affinity.AsString());
        }
    }

    return result;
}

void SNetScheduleServer::ExecChangePreferredAffinities(const TArguments& args, SInputOutput& io)
{
    _ASSERT(args.size() == 2);

    auto affs_to_add = args[0].Value();
    auto affs_to_del = args[1].Value();

    auto to_add = s_ExtractVectorOfStrings(affs_to_add);
    auto to_del = s_ExtractVectorOfStrings(affs_to_del);

    m_NetScheduleAPI.GetExecutor().ChangePreferredAffinities(&to_add, &to_del);
}

bool SNetScheduleServer::Call(const string& method, const TArguments& args, SInputOutput& io)
{
    if (method == "server_status")
        ExecServerStatus(args, io);
    else if (method == "job_group_info")
        ExecJobGroupInfo(args, io);
    else if (method == "client_info")
        ExecClientInfo(args, io);
    else if (method == "notification_info")
        ExecNotificationInfo(args, io);
    else if (method == "affinity_info")
        ExecAffinityInfo(args, io);
    else if (method == "change_preferred_affinities") {
        ExecChangePreferredAffinities(args, io);
    } else
        return SNetScheduleService::Call(method, args, io);

    return true;
}

void SNetScheduleService::CEventHandler::OnWarning(
        const string& warn_msg, CNetServer server)
{
    m_AutomationProc->SendWarning(warn_msg, m_AutomationProc->
            ReturnNetScheduleServerObject(m_NetScheduleAPI, server));
}

CCommand SNetScheduleService::CallCommand()
{
    return CCommand(kName, TCommandGroup(CallCommands, CheckCall<TSelf>));
}

TCommands SNetScheduleService::CallCommands()
{
    TCommands cmds =
    {
        { "set_client_type",  ExecMethod<TSelf, &TSelf::ExecSetClientType>, {
                { "client_type", 0, },
            }},
        { "set_node_session", ExecMethod<TSelf, &TSelf::ExecSetNodeSession>, {
                { "node", "", },
                { "session", "", },
            }},
        { "queue_info",       ExecMethod<TSelf, &TSelf::ExecQueueInfo>, {
                { "queue_name", "", },
            }},
        { "queue_class_info", ExecMethod<TSelf, &TSelf::ExecQueueClassInfo>, },
        { "reconf",           ExecMethod<TSelf, &TSelf::ExecReconf>, },
        { "suspend",          ExecMethod<TSelf, &TSelf::ExecSuspend>, {
                { "pullback_mode", false, },
            }},
        { "resume",           ExecMethod<TSelf, &TSelf::ExecResume>, },
        { "shutdown",         ExecMethod<TSelf, &TSelf::ExecShutdown>, {
                { "do_not_drain", false, },
            }},
        { "parse_key",        ExecMethod<TSelf, &TSelf::ExecParseKey>, {
                { "job_key", CJsonNode::eString, },
            }},
        { "job_info",         ExecMethod<TSelf, &TSelf::ExecJobInfo>, {
                { "job_key", CJsonNode::eString, },
                { "verbose", true, },
            }},
        { "job_counters",     ExecMethod<TSelf, &TSelf::ExecJobCounters>, {
                { " affinity", "", },
                { " job_group", "", },
            }},
        { "get_servers",      ExecMethod<TSelf, &TSelf::ExecGetServers>, },
    };

    TCommands base_cmds = SNetService::CallCommands();
    cmds.insert(cmds.end(), base_cmds.begin(), base_cmds.end());

    return cmds;
}

void SNetScheduleService::ExecSetClientType(const TArguments& args, SInputOutput& io)
{
    _ASSERT(args.size() == 1);

    const auto client_type = args[0].AsInteger<CNetScheduleAPI::EClientType>();
    m_NetScheduleAPI.SetClientType(client_type);
}

void SNetScheduleService::ExecSetNodeSession(const TArguments& args, SInputOutput& io)
{
    _ASSERT(args.size() == 2);

    const auto node    = args[0].AsString();
    const auto session = args[1].AsString();
    m_NetScheduleAPI.ReSetClientNode(node);
    m_NetScheduleAPI.ReSetClientSession(session);
}

void SNetScheduleService::ExecQueueInfo(const TArguments& args, SInputOutput& io)
{
    _ASSERT(args.size() == 1);

    auto& reply = io.reply;
    const auto queue_name = args[0].AsString();
    reply.Append(g_QueueInfoToJson(m_NetScheduleAPI, queue_name, m_ActualServiceType));
}

void SNetScheduleService::ExecQueueClassInfo(const TArguments& args, SInputOutput& io)
{
    auto& reply = io.reply;
    reply.Append(g_QueueClassInfoToJson(m_NetScheduleAPI, m_ActualServiceType));
}

void SNetScheduleService::ExecReconf(const TArguments& args, SInputOutput& io)
{
    auto& reply = io.reply;
    reply.Append(g_ReconfAndReturnJson(m_NetScheduleAPI, m_ActualServiceType));
}

void SNetScheduleService::ExecSuspend(const TArguments& args, SInputOutput& io)
{
    _ASSERT(args.size() == 1);

    const auto pullback_mode = args[0].AsBoolean();
    g_SuspendNetSchedule(m_NetScheduleAPI, pullback_mode);
}

void SNetScheduleService::ExecResume(const TArguments& args, SInputOutput& io)
{
    g_ResumeNetSchedule(m_NetScheduleAPI);
}

void SNetScheduleService::ExecShutdown(const TArguments& args, SInputOutput& io)
{
    _ASSERT(args.size() == 1);

    const auto do_not_drain = args[0].AsBoolean();
    auto level = do_not_drain ? CNetScheduleAdmin::eNormalShutdown : CNetScheduleAdmin::eDrain;
    m_NetScheduleAPI.GetAdmin().ShutdownServer(level);
}

void SNetScheduleService::ExecParseKey(const TArguments& args, SInputOutput& io)
{
    _ASSERT(args.size() == 1);

    auto& reply = io.reply;
    const auto job_key = args[0].AsString();
    CJobInfoToJSON job_key_to_json;
    job_key_to_json.ProcessJobMeta(CNetScheduleKey(job_key, m_NetScheduleAPI.GetCompoundIDPool()));
    reply.Append(job_key_to_json.GetRootNode());
}

void SNetScheduleService::ExecJobInfo(const TArguments& args, SInputOutput& io)
{
    _ASSERT(args.size() == 2);

    auto& reply = io.reply;
    const auto job_key = args[0].AsString();
    const auto verbose = args[1].AsBoolean();
    CJobInfoToJSON job_info_to_json;
    g_ProcessJobInfo(m_NetScheduleAPI, job_key, &job_info_to_json, verbose, m_NetScheduleAPI.GetCompoundIDPool());
    reply.Append(job_info_to_json.GetRootNode());
}

void SNetScheduleService::ExecJobCounters(const TArguments& args, SInputOutput& io)
{
    _ASSERT(args.size() == 2);

    auto& reply = io.reply;
    const auto affinity  = args[0].AsString();
    const auto job_group = args[1].AsString();
    CNetScheduleAdmin::TStatusMap status_map;
    m_NetScheduleAPI.GetAdmin().StatusSnapshot(status_map, affinity, job_group);
    CJsonNode jobs_by_status(CJsonNode::NewObjectNode());

    ITERATE(CNetScheduleAdmin::TStatusMap, it, status_map) {
        jobs_by_status.SetInteger(it->first, it->second);
    }
    reply.Append(jobs_by_status);
}

void SNetScheduleService::ExecGetServers(const TArguments& args, SInputOutput& io)
{
    auto& reply = io.reply;

    CJsonNode object_ids(CJsonNode::NewArrayNode());
    for (CNetServiceIterator it = m_NetScheduleAPI.GetService().Iterate(
            CNetService::eIncludePenalized); it; ++it)
        object_ids.AppendInteger(m_AutomationProc->
                ReturnNetScheduleServerObject(m_NetScheduleAPI, *it)->
                GetID());
    reply.Append(object_ids);
}

bool SNetScheduleService::Call(const string& method, const TArguments& args, SInputOutput& io)
{
    if (method == "set_client_type")
        ExecSetClientType(args, io);
    else if (method == "set_node_session") {
        ExecSetNodeSession(args, io);
    } else if (method == "queue_info")
        ExecQueueInfo(args, io);
    else if (method == "queue_class_info")
        ExecQueueClassInfo(args, io);
    else if (method == "reconf")
        ExecReconf(args, io);
    else if (method == "suspend")
        ExecSuspend(args, io);
    else if (method == "resume")
        ExecResume(args, io);
    else if (method == "shutdown")
        ExecShutdown(args, io);
    else if (method == "parse_key") {
        ExecParseKey(args, io);
    } else if (method == "job_info") {
        ExecJobInfo(args, io);
    } else if (method == "job_counters") {
        ExecJobCounters(args, io);
    } else if (method == "get_servers") {
        ExecGetServers(args, io);
    } else
        return SNetService::Call(method, args, io);

    return true;
}
