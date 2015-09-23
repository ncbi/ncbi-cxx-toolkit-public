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
 * File Description: Utility functions - declarations.
 *
 */

#ifndef UTIL__HPP
#define UTIL__HPP

#include <connect/services/netschedule_api.hpp>
#include <connect/services/ns_output_parser.hpp>

BEGIN_NCBI_SCOPE

enum ENetScheduleStatTopic {
    eNetScheduleStatJobGroups,
    eNetScheduleStatClients,
    eNetScheduleStatNotifications,
    eNetScheduleStatAffinities,
    eNumberOfNetStheduleStatTopics
};

string g_GetNetScheduleStatCommand(ENetScheduleStatTopic topic);

CJsonNode g_GenericStatToJson(CNetServer server,
        ENetScheduleStatTopic topic, bool verbose);

bool g_FixMisplacedPID(CJsonNode& stat_info, CTempString& executable_path,
        const char* pid_key);

CJsonNode g_LegacyStatToJson(CNetServer server, bool verbose);

CJsonNode g_QueueInfoToJson(CNetScheduleAPI ns_api,
        const string& queue_name, CNetService::EServiceType service_type);

CJsonNode g_QueueClassInfoToJson(CNetScheduleAPI ns_api,
        CNetService::EServiceType service_type);

CJsonNode g_ReconfAndReturnJson(CNetScheduleAPI ns_api,
        CNetService::EServiceType service_type);

class IJobInfoProcessor
{
public:
    virtual void ProcessJobMeta(const CNetScheduleKey& key) = 0;

    virtual void BeginJobEvent(const CTempString& event_header) = 0;
    virtual void ProcessJobEventField(const CTempString& attr_name,
            const string& attr_value) = 0;
    virtual void ProcessJobEventField(const CTempString& attr_name) = 0;
    virtual void ProcessInput(const string& data) = 0;
    virtual void ProcessOutput(const string& data) = 0;

    virtual void ProcessJobInfoField(const CTempString& field_name,
            const CTempString& field_value) = 0;

    virtual void ProcessRawLine(const string& line) = 0;

    virtual ~IJobInfoProcessor() {}
};

class CJobInfoToJSON : public IJobInfoProcessor
{
public:
    CJobInfoToJSON() : m_JobInfo(CJsonNode::NewObjectNode()) {}

    virtual void ProcessJobMeta(const CNetScheduleKey& key);

    virtual void BeginJobEvent(const CTempString& event_header);
    virtual void ProcessJobEventField(const CTempString& attr_name,
            const string& attr_value);
    virtual void ProcessJobEventField(const CTempString& attr_name);
    virtual void ProcessInput(const string& data);
    virtual void ProcessOutput(const string& data);

    virtual void ProcessJobInfoField(const CTempString& field_name,
        const CTempString& field_value);

    virtual void ProcessRawLine(const string& line);

    CJsonNode GetRootNode() const {return m_JobInfo;}

private:
    CJsonNode m_JobInfo;
    CJsonNode m_JobEvents;
    CJsonNode m_CurrentEvent;
    CJsonNode m_UnparsableLines;
};

void g_ProcessJobInfo(CNetScheduleAPI ns_api, const string& job_key,
        IJobInfoProcessor* processor, bool verbose,
        CCompoundIDPool::TInstance id_pool = NULL);

void g_PrintJSON(FILE* output_stream, CJsonNode node,
        const char* indent = "\t");

CJsonNode g_ExecAnyCmdToJson(CNetService service,
        CNetService::EServiceType service_type,
        const string& command, bool multiline);

CJsonNode g_ServerInfoToJson(CNetServerInfo server_info,
        bool server_version_key);

CJsonNode g_ServerInfoToJson(CNetService service,
        CNetService::EServiceType service_type,
        bool server_version_key);

CJsonNode g_WorkerNodeInfoToJson(CNetServer worker_node);

void g_SuspendNetSchedule(CNetScheduleAPI netschedule_api, bool pullback_mode);
void g_ResumeNetSchedule(CNetScheduleAPI netschedule_api);

void g_SuspendWorkerNode(CNetServer worker_node,
        bool pullback_mode, unsigned timeout);
void g_ResumeWorkerNode(CNetServer worker_node);

void g_GetUserAndHost(string* user, string* host);

END_NCBI_SCOPE

#endif // UTIL__HPP
