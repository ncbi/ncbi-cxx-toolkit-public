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
 * File Description: Worker node automation implementation.
 *
 */

#include <ncbi_pch.hpp>

#include "wn_automation.hpp"

USING_NCBI_SCOPE;

using namespace NAutomation;

const string SWorkerNode::kName = "wn";

SWorkerNode::SWorkerNode(
        CAutomationProc* automation_proc, CNetScheduleAPI ns_api) :
    SNetService(automation_proc),
    m_NetScheduleAPI(ns_api)
{
    m_WorkerNode = GetServer();

    if (GetService().IsLoadBalanced()) {
        NCBI_THROW(CAutomationException, eCommandProcessingError,
                "WorkerNode constructor: 'wn_address' "
                "must be a host:port combination");
    }
}

CCommand SWorkerNode::NewCommand()
{
    return CCommand(kName, ExecNew<TSelf>, {
            { "wn_address", CJsonNode::eString, },
            { "client_name", "", },
        });
}

CAutomationObject* SWorkerNode::Create(const TArguments& args, CAutomationProc* automation_proc)
{
    _ASSERT(args.size() == 2);

    const auto wn_address  = args["wn_address"].AsString();
    const auto client_name = args["client_name"].AsString();

    CNetScheduleAPIExt ns_api(CNetScheduleAPIExt::CreateWnCompat(
                wn_address, client_name));

    return new SWorkerNode(automation_proc, ns_api);
}

CCommand SWorkerNode::CallCommand()
{
    return CCommand(kName, TCommandGroup(CallCommands(), CheckCall<TSelf>));
}

TCommands SWorkerNode::CallCommands()
{
    TCommands cmds =
    {
        { "version",  ExecMethod<TSelf, &TSelf::ExecVersion>, },
        { "wn_info",  ExecMethod<TSelf, &TSelf::ExecWnInfo>, },
        { "suspend",  ExecMethod<TSelf, &TSelf::ExecSuspend>, {
                { "pullback_mode", false, },
                { "timeout", 0, },
            }},
        { "resume",   ExecMethod<TSelf, &TSelf::ExecResume>, },
        { "shutdown", ExecMethod<TSelf, &TSelf::ExecShutdown>, {
                { "level", 0 },
            }},
    };

    TCommands base_cmds = SNetService::CallCommands();
    cmds.insert(cmds.end(), base_cmds.begin(), base_cmds.end());

    return cmds;
}

void SWorkerNode::ExecVersion(const TArguments&, SInputOutput& io)
{
    auto& reply = io.reply;
    reply.Append(g_ServerInfoToJson(GetService(), false));
}

void SWorkerNode::ExecWnInfo(const TArguments&, SInputOutput& io)
{
    auto& reply = io.reply;
    reply.Append(g_WorkerNodeInfoToJson(m_WorkerNode));
}

void SWorkerNode::ExecSuspend(const TArguments& args, SInputOutput&)
{
    _ASSERT(args.size() == 2);

    const auto pullback_mode = args["pullback_mode"].AsBoolean();
    const auto timeout       = args["timeout"].AsInteger<unsigned int>();
    g_SuspendWorkerNode(m_WorkerNode, pullback_mode, timeout);
}

void SWorkerNode::ExecResume(const TArguments&, SInputOutput&)
{
    g_ResumeWorkerNode(m_WorkerNode);
}

CNetScheduleAdmin::EShutdownLevel s_GetWorkerNodeShutdownMode(Int8 level)
{
    enum EWorkerNodeShutdownMode { eNormalShutdown, eShutdownNow, eKillNode };

    switch (level) {
    case eShutdownNow: return CNetScheduleAdmin::eShutdownImmediate;
    case eKillNode:    return CNetScheduleAdmin::eDie;
    default:           return CNetScheduleAdmin::eNormalShutdown;
    }
}

void SWorkerNode::ExecShutdown(const TArguments& args, SInputOutput&)
{
    _ASSERT(args.size() == 1);

    const auto level = args["level"].AsInteger();
    m_NetScheduleAPI.GetAdmin().ShutdownServer(s_GetWorkerNodeShutdownMode(level));
}
