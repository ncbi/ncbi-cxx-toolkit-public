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

void CNetScheduleJobReader::SetJobGroup(const string& group_name)
{
    SNetScheduleAPIImpl::VerifyJobGroupAlphabet(group_name);
    m_Impl->m_JobGroup = group_name;
}

void CNetScheduleJobReader::SetAffinity(const string& affinity)
{
    SNetScheduleAPIImpl::VerifyAffinityAlphabet(affinity);
    m_Impl->m_Affinity = affinity;
}

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

bool SNetScheduleJobReaderImpl::x_ReadJob(SNetServerImpl* server,
        const CDeadline& timeout, CNetScheduleJob& job,
        CNetScheduleAPI::EJobStatus* job_status,
        bool* no_more_jobs)
{
    string cmd("READ2 reader_aff=0 ");

    if (m_Affinity.empty())
        cmd.append("any_aff=1");
    else {
        cmd.append("any_aff=0 aff=");
        cmd.append(m_Affinity);
    }

    m_API->m_NotificationThread->CmdAppendPortAndTimeout(&cmd, timeout);

    if (!m_JobGroup.empty()) {
        cmd.append(" group=");
        cmd.append(m_JobGroup);
    }

    g_AppendClientIPSessionIDHitID(cmd);

    CNetServer::SExecResult exec_result;

    server->ConnectAndExec(cmd, false, exec_result);

    bool ret = s_ParseReadJobResponse(exec_result.response, job,
            job_status, no_more_jobs);

    if (!no_more_jobs)
        m_MoreJobs = true;

    return ret;
}

CNetServer SNetScheduleJobReaderImpl::ReadNotifications()
{
    return x_ProcessReadJobNotifications();
}

CNetServer SNetScheduleJobReaderImpl::WaitForNotifications(const CDeadline& deadline)
{
    if (m_API->m_NotificationThread->m_ReadNotifications.Wait(deadline)) {
        return x_ProcessReadJobNotifications();
    }

    return CNetServer();
}

CNetServer SNetScheduleJobReaderImpl::x_ProcessReadJobNotifications()
{
    string ns_node;
    CNetServer server;

    if (m_API->m_NotificationThread->m_ReadNotifications.GetNextNotification(
                &ns_node))
        m_API->GetServerByNode(ns_node, &server);

    return server;
}

SNetScheduleJobReaderImpl::EState SNetScheduleJobReaderImpl::CheckState()
{
    return eWorking;
}

bool SNetScheduleJobReaderImpl::MoreJobs()
{
    bool ret = m_MoreJobs;
    m_MoreJobs = false;
    return ret;
}

bool SNetScheduleJobReaderImpl::CheckEntry(
        CNetScheduleTimeline::SEntry& entry,
        CNetScheduleJob& job,
        CNetScheduleAPI::EJobStatus* job_status)
{
    CNetServer server(m_API.GetService().GetServer(entry.server_address));
    bool no_more_jobs = true;
    bool ret = x_ReadJob(server, m_Timeout, job, job_status, &no_more_jobs);

    // Cache the result for the server,
    // so we don't need to ask the server again about matching jobs
    // while waiting for its notifications
    entry.more_jobs = !no_more_jobs;

    return ret;
}

SNetScheduleJobReaderImpl::EResult SNetScheduleJobReaderImpl::GetJob(
        const CDeadline& deadline,
        CNetScheduleJob& job,
        CNetScheduleAPI::EJobStatus* job_status)
{
    for (;;) {
        while (m_Timeline.HasImmediateActions()) {
            CNetScheduleTimeline::SEntry timeline_entry(m_Timeline.PullImmediateAction());

            if (m_Timeline.IsDiscoveryAction(timeline_entry)) {
                if (CheckState() == eWorking) {
                    m_Timeline.NextDiscoveryIteration(m_API);
                }

                m_Timeline.PushScheduledAction(timeline_entry, m_Timeout);
            } else if (CheckState() == eWorking) {
                try {
                    if (CheckEntry(timeline_entry, job, job_status)) {
                        // A job has been returned; add the server to
                        // immediate actions because there can be more
                        // jobs in the queue.
                        m_Timeline.PushImmediateAction(timeline_entry);
                        return eJob;
                    } else {
                        // No job has been returned by this server;
                        // query the server later.
                        m_Timeline.PushScheduledAction(timeline_entry, m_Timeout);
                    }
                }
                catch (CNetSrvConnException& e) {
                    // Because a connection error has occurred, do not
                    // put this server back to the timeline.
                    LOG_POST(Warning << e.GetMsg());
                }
            }

            m_Timeline.CheckScheduledActions();

            // Check if there's a notification in the UDP socket.
            while (CNetServer server = ReadNotifications()) {
                m_Timeline.MoveToImmediateActions(server);
            }
        }

        if (CheckState() == eStop)
            return eInterrupt;

        if (!MoreJobs() && !m_Timeline.MoreJobs())
            return eNoJobs;

        if (deadline.IsExpired())
            return eAgain;

        // At least, the discovery action must be there
        _ASSERT(m_Timeline.HasScheduledActions());

        // There's still time. Wait for notifications and query the servers.
        CDeadline next_event_time = m_Timeline.GetNextTimeout();
        bool last_wait = deadline < next_event_time;
        if (last_wait) next_event_time = deadline;

        if (CNetServer server = WaitForNotifications(next_event_time)) {
            do {
                m_Timeline.MoveToImmediateActions(server);
            } while (server = ReadNotifications());
        } else if (last_wait) {
            return eAgain;
        } else {
            m_Timeline.PushImmediateAction(m_Timeline.PullScheduledAction());
        }
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

    switch (GetJob(deadline, *job, job_status)) {
        case eJob:              return CNetScheduleJobReader::eRNJ_JobReady;
        case eAgain:            return CNetScheduleJobReader::eRNJ_NotReady;
        case eInterrupt:        return CNetScheduleJobReader::eRNJ_Interrupt;
        default /* eNoJobs */:  return CNetScheduleJobReader::eRNJ_NoMoreJobs;
    }
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
    m_Impl->x_StartNotificationThread();

    m_Impl->m_API->m_NotificationThread->m_ReadNotifications.InterruptWait();
}

END_NCBI_SCOPE
