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

const string& SNetScheduleServiceAutomationObject::GetType() const
{
    static const string object_type("nssvc");

    return object_type;
}

const void* SNetScheduleServiceAutomationObject::GetImplPtr() const
{
    return m_NetScheduleAPI;
}

SNetScheduleServerAutomationObject::SNetScheduleServerAutomationObject(
        CAutomationProc* automation_proc,
        const string& service_name,
        const string& queue_name,
        const string& client_name) :
    SNetScheduleServiceAutomationObject(automation_proc,
            CNetScheduleAPIExt::CreateNoCfgLoad(
                service_name, client_name, queue_name))
{
    if (m_Service.IsLoadBalanced()) {
        NCBI_THROW(CAutomationException, eCommandProcessingError,
                "NetScheduleServer constructor: "
                "'server_address' must be a host:port combination");
    }

    m_NetServer = m_Service.Iterate().GetServer();

    m_NetScheduleAPI.SetEventHandler(
            new CEventHandler(automation_proc, m_NetScheduleAPI));
}

const string& SNetScheduleServerAutomationObject::GetType() const
{
    static const string object_type("nssrv");

    return object_type;
}

const void* SNetScheduleServerAutomationObject::GetImplPtr() const
{
    return m_NetServer;
}

TAutomationObjectRef CAutomationProc::ReturnNetScheduleServerObject(
        CNetScheduleAPI::TInstance ns_api,
        CNetServer::TInstance server)
{
    TAutomationObjectRef object(FindObjectByPtr(server));
    if (!object) {
        object = new SNetScheduleServerAutomationObject(this, ns_api, server);
        AddObject(object, server);
    }
    return object;
}

static void ExtractVectorOfStrings(CArgArray& arg_array,
        vector<string>& result)
{
    CJsonNode arg(arg_array.NextNode());
    if (!arg.IsNull())
        for (CJsonIterator it = arg.Iterate(); it; ++it)
            result.push_back(arg_array.GetString(*it));
}

bool SNetScheduleServerAutomationObject::Call(const string& method,
        CArgArray& arg_array, CJsonNode& reply)
{
    if (method == "server_status")
        reply.Append(g_LegacyStatToJson(m_NetServer,
                arg_array.NextBoolean(false)));
    else if (method == "job_group_info")
        reply.Append(g_GenericStatToJson(m_NetServer,
                eNetScheduleStatJobGroups, arg_array.NextBoolean(false)));
    else if (method == "client_info")
        reply.Append(g_GenericStatToJson(m_NetServer,
                eNetScheduleStatClients, arg_array.NextBoolean(false)));
    else if (method == "notification_info")
        reply.Append(g_GenericStatToJson(m_NetServer,
                eNetScheduleStatNotifications, arg_array.NextBoolean(false)));
    else if (method == "affinity_info")
        reply.Append(g_GenericStatToJson(m_NetServer,
                eNetScheduleStatAffinities, arg_array.NextBoolean(false)));
    else if (method == "change_preferred_affinities") {
        vector<string> affs_to_add;
        ExtractVectorOfStrings(arg_array, affs_to_add);
        vector<string> affs_to_del;
        ExtractVectorOfStrings(arg_array, affs_to_del);
        m_NetScheduleAPI.GetExecutor().ChangePreferredAffinities(
                &affs_to_add, &affs_to_del);
    } else
        return SNetScheduleServiceAutomationObject::Call(
                method, arg_array, reply);

    return true;
}

void SNetScheduleServiceAutomationObject::CEventHandler::OnWarning(
        const string& warn_msg, CNetServer server)
{
    m_AutomationProc->SendWarning(warn_msg, m_AutomationProc->
            ReturnNetScheduleServerObject(m_NetScheduleAPI, server));
}

bool SNetScheduleServiceAutomationObject::Call(const string& method,
        CArgArray& arg_array, CJsonNode& reply)
{
    if (method == "set_client_type")
        m_NetScheduleAPI.SetClientType(
                (CNetScheduleAPI::EClientType) arg_array.NextInteger(0));
    else if (method == "set_node_session") {
        CJsonNode arg(arg_array.NextNode());
        const string node(arg.IsNull() ? kEmptyStr : arg_array.GetString(arg));
        m_NetScheduleAPI.SetClientNode(node);
        arg = arg_array.NextNode();
        const string session(arg.IsNull() ? kEmptyStr : arg_array.GetString(arg));
        m_NetScheduleAPI.SetClientSession(session);
    } else if (method == "queue_info")
        reply.Append(g_QueueInfoToJson(m_NetScheduleAPI,
                arg_array.NextString(kEmptyStr), m_ActualServiceType));
    else if (method == "queue_class_info")
        reply.Append(g_QueueClassInfoToJson(m_NetScheduleAPI,
                m_ActualServiceType));
    else if (method == "reconf")
        reply.Append(g_ReconfAndReturnJson(m_NetScheduleAPI,
                m_ActualServiceType));
    else if (method == "suspend")
        g_SuspendNetSchedule(m_NetScheduleAPI, arg_array.NextBoolean(false));
    else if (method == "resume")
        g_ResumeNetSchedule(m_NetScheduleAPI);
    else if (method == "shutdown")
        m_NetScheduleAPI.GetAdmin().ShutdownServer(
                arg_array.NextBoolean(false) ?
                        CNetScheduleAdmin::eNormalShutdown :
                        CNetScheduleAdmin::eDrain);
    else if (method == "parse_key") {
        CJobInfoToJSON job_key_to_json;
        job_key_to_json.ProcessJobMeta(CNetScheduleKey(arg_array.NextString(),
                m_NetScheduleAPI.GetCompoundIDPool()));
        reply.Append(job_key_to_json.GetRootNode());
    } else if (method == "job_info") {
        CJobInfoToJSON job_info_to_json;
        string job_key(arg_array.NextString());
        g_ProcessJobInfo(m_NetScheduleAPI, job_key,
            &job_info_to_json, arg_array.NextBoolean(true),
            m_NetScheduleAPI.GetCompoundIDPool());
        reply.Append(job_info_to_json.GetRootNode());
    } else if (method == "job_counters") {
        CNetScheduleAdmin::TStatusMap status_map;
        string affinity(arg_array.NextString(kEmptyStr));
        string job_group(arg_array.NextString(kEmptyStr));
        m_NetScheduleAPI.GetAdmin().StatusSnapshot(status_map,
                affinity, job_group);
        CJsonNode jobs_by_status(CJsonNode::NewObjectNode());
        ITERATE(CNetScheduleAdmin::TStatusMap, it, status_map) {
            jobs_by_status.SetInteger(it->first, it->second);
        }
        reply.Append(jobs_by_status);
    } else if (method == "get_servers") {
        CJsonNode object_ids(CJsonNode::NewArrayNode());
        for (CNetServiceIterator it = m_NetScheduleAPI.GetService().Iterate(
                CNetService::eIncludePenalized); it; ++it)
            object_ids.AppendInteger(m_AutomationProc->
                    ReturnNetScheduleServerObject(m_NetScheduleAPI, *it)->
                    GetID());
        reply.Append(object_ids);
    } else
        return SNetServiceAutomationObject::Call(method, arg_array, reply);

    return true;
}
