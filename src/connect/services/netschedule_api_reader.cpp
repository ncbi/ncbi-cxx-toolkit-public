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
 *  Government have not placed any restriction on its use or reproduction.
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
 * Author:  Dmitry Kazimirov
 *
 * File Description:
 *   NetSchedule API - job reader.
 *
 */

#include <ncbi_pch.hpp>

#include "netschedule_api_impl.hpp"

#define NCBI_USE_ERRCODE_X   ConnServ_NetSchedule

BEGIN_NCBI_SCOPE

// True if a job is returned.
static bool s_ParseReadJobResponse(const string& response,
        CNetScheduleJob& job,
        CNetScheduleAPI::EJobStatus* job_status,
        bool* no_more_jobs)
{
    if (response.empty())
        return false;

    enum {
        eJobKey,
        eJobStatus,
        eAuthToken,
        eNumberOfRequiredFields
    };
    int job_fields = 0;
    CUrlArgs url_parser(response);
    ITERATE(CUrlArgs::TArgs, field, url_parser.GetArgs()) {
        switch (field->name[0]) {
        case 'j':
            if (field->name == "job_key") {
                job.job_id = field->value;
                job_fields |= (1 << eJobKey);
            }
            break;

        case 's':
            if (field->name == "status") {
                *job_status = CNetScheduleAPI::StringToStatus(field->value);
                job_fields |= (1 << eJobStatus);
            }
            break;

        case 'a':
            if (field->name == "auth_token") {
                job.auth_token = field->value;
                job_fields |= (1 << eAuthToken);
            } else if (field->name == "affinity")
                job.affinity = field->value;
            break;

        case 'c':
            if (field->name == "client_ip")
                job.client_ip = field->value;
            else if (field->name == "client_sid")
                job.session_id = field->value;
            break;

        case 'n':
            if (field->name == "ncbi_phid")
                job.page_hit_id = field->value;
            else if (field->name == "no_more_jobs") {
                _ASSERT(no_more_jobs);
                *no_more_jobs = NStr::StringToBool(field->value.c_str());
            }
        }
    }

    return job_fields == (1 << eNumberOfRequiredFields) - 1;
}

bool SNetScheduleJobReaderImpl::CImpl::CheckEntry(
        NNetScheduleGetJob::SEntry& entry,
        const string& prio_aff_list,
        CNetScheduleJob& job,
        CNetScheduleAPI::EJobStatus* job_status)
{
    CNetServer server(m_API.GetService().GetServer(entry.server_address));
    bool no_more_jobs = true;

    string cmd("READ2 reader_aff=0 ");
    bool prioritized_aff = false;

    if (!m_Affinity.empty()) {
        cmd.append("any_aff=0 aff=");
        cmd.append(m_Affinity);
    } else if (!prio_aff_list.empty()) {
        // If prioritized_aff flag is not supported (NS v4.22.0+)
        // TODO: Can be thrown out after all NS serves are updated to version 4.22.0+
        if (!server->m_VersionInfo.IsUpCompatible(CVersionInfo(4, 22, 0))) {
            return CheckEntryOld(server, entry, prio_aff_list, job, job_status);
        }
 
        prioritized_aff = true;
        cmd.append("any_aff=0 aff=");
        cmd.append(prio_aff_list);
    } else {
        cmd.append("any_aff=1");
    }

    m_API->m_NotificationThread->CmdAppendPortAndTimeout(&cmd, m_Timeout);

    if (!m_JobGroup.empty()) {
        cmd.append(" group=");
        cmd.append(m_JobGroup);
    }

    g_AppendClientIPSessionIDHitID(cmd);

    if (prioritized_aff) {
        cmd.append(" prioritized_aff=1");
    }

    CNetServer::SExecResult exec_result;

    server->ConnectAndExec(cmd, false, exec_result);

    bool ret = s_ParseReadJobResponse(exec_result.response, job,
            job_status, &no_more_jobs);

    if (!no_more_jobs)
        m_MoreJobs = true;

    // Cache the result for the server,
    // so we don't need to ask the server again about matching jobs
    // while waiting for its notifications
    entry.more_jobs = !no_more_jobs;

    return ret;
}

// XXX: Compatibility mode.
// TODO: Can be thrown out after all NS serves are updated to version 4.22.0+
bool SNetScheduleJobReaderImpl::CImpl::CheckEntryOld(
        CNetServer server,
        NNetScheduleGetJob::SEntry& entry,
        const string& prio_aff_list,
        CNetScheduleJob& job,
        CNetScheduleAPI::EJobStatus* job_status)
{

    list<CTempString> affinity_tokens;
    NStr::Split(prio_aff_list, ",", affinity_tokens);

    string affinity_list;
    list<CTempString>::const_iterator it = affinity_tokens.begin();
    bool no_more_jobs;
    bool ret = false;

    while (it != affinity_tokens.end()) {
        no_more_jobs = true;
        affinity_list += *it;
        const bool last_try = ++it == affinity_tokens.end();

        string cmd("READ2 reader_aff=0 any_aff=0 aff=");
        cmd.append(affinity_list);

        m_API->m_NotificationThread->CmdAppendPortAndTimeout(&cmd,
                last_try ? m_Timeout : 0);

        if (!m_JobGroup.empty()) {
            cmd.append(" group=");
            cmd.append(m_JobGroup);
        }

        g_AppendClientIPSessionIDHitID(cmd);

        CNetServer::SExecResult exec_result;

        server->ConnectAndExec(cmd, false, exec_result);

        if (s_ParseReadJobResponse(exec_result.response, job,
                job_status, &no_more_jobs)) {
            ret = true;
            break;
        }

        affinity_list += ',';
    }

    if (!no_more_jobs)
        m_MoreJobs = true;

    // Cache the result for the server,
    // so we don't need to ask the server again about matching jobs
    // while waiting for its notifications
    entry.more_jobs = !no_more_jobs;

    return ret;
}

void SNetScheduleJobReaderImpl::CImpl::ReturnJob(CNetScheduleJob& job)
{
    SNetScheduleSubmitterImpl submitter_impl(m_API);
    submitter_impl.FinalizeRead("RDRB blacklist=0 job_key=", "ReadRollback",
            job.job_id, job.auth_token, kEmptyStr);
}

CNetServer SNetScheduleJobReaderImpl::CImpl::WaitForNotifications(const CDeadline& deadline)
{
    if (m_API->m_NotificationThread->m_ReadNotifications.Wait(deadline)) {
        return ReadNotifications();
    }

    return CNetServer();
}

CNetServer SNetScheduleJobReaderImpl::CImpl::ReadNotifications()
{
    string ns_node;
    CNetServer server;

    if (m_API->m_NotificationThread->m_ReadNotifications.GetNextNotification(
                &ns_node))
        m_API->GetServerByNode(ns_node, &server);

    return server;
}

NNetScheduleGetJob::EState SNetScheduleJobReaderImpl::CImpl::CheckState()
{
    return NNetScheduleGetJob::eWorking;
}

bool SNetScheduleJobReaderImpl::CImpl::MoreJobs(const NNetScheduleGetJob::SEntry& entry)
{
    if (m_MoreJobs) {
        m_MoreJobs = false;
        return true;
    }

    return entry.more_jobs;
}

void SNetScheduleJobReaderImpl::SetJobGroup(const string& group_name)
{
    if (m_Impl.m_JobGroup.empty()) {
        SNetScheduleAPIImpl::VerifyJobGroupAlphabet(group_name);
        m_Impl.m_JobGroup = group_name;
    } else {
        NCBI_THROW(CNetScheduleException, eInvalidParameter,
                "It's not allowed to change group");
    }
}

void SNetScheduleJobReaderImpl::SetAffinity(const string& affinity)
{
    if (m_Impl.m_Affinity.empty()) {
        SNetScheduleAPIImpl::VerifyAffinityAlphabet(affinity);
        m_Impl.m_Affinity = affinity;
    } else {
        NCBI_THROW(CNetScheduleException, eInvalidParameter,
                "It's not allowed to change affinity");
    }
}

CNetScheduleJobReader::EReadNextJobResult SNetScheduleJobReaderImpl::ReadNextJob(
        CNetScheduleJob* job,
        CNetScheduleAPI::EJobStatus* job_status,
        const CTimeout* timeout)
{
    _ASSERT(job);

    x_StartNotificationThread();
    CDeadline deadline(timeout ? *timeout : CTimeout(0, 0));

    switch (m_Timeline.GetJob(deadline, *job, job_status)) {
    case NNetScheduleGetJob::eJob:
        return CNetScheduleJobReader::eRNJ_JobReady;

    case NNetScheduleGetJob::eAgain:
        return CNetScheduleJobReader::eRNJ_NotReady;

    case NNetScheduleGetJob::eInterrupt:
        return CNetScheduleJobReader::eRNJ_Interrupt;

    default /* NNetScheduleGetJob::eNoJobs */:
        return CNetScheduleJobReader::eRNJ_NoMoreJobs;
    }
}

void SNetScheduleJobReaderImpl::InterruptReading()
{
    x_StartNotificationThread();
    m_Impl.m_API->m_NotificationThread->m_ReadNotifications.InterruptWait();
}

void CNetScheduleJobReader::SetJobGroup(const string& group_name)
{
    _ASSERT(m_Impl);
    m_Impl->SetJobGroup(group_name);
}

void CNetScheduleJobReader::SetAffinity(const string& affinity)
{
    _ASSERT(m_Impl);
    m_Impl->SetAffinity(affinity);
}

CNetScheduleJobReader::EReadNextJobResult CNetScheduleJobReader::ReadNextJob(
        CNetScheduleJob* job,
        CNetScheduleAPI::EJobStatus* job_status,
        const CTimeout* timeout)
{
    _ASSERT(m_Impl);
    return m_Impl->ReadNextJob(job, job_status, timeout);
}

void CNetScheduleJobReader::InterruptReading()
{
    _ASSERT(m_Impl);
    m_Impl->InterruptReading();
}

END_NCBI_SCOPE
