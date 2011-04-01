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
 * File Description: NetSchedule-specific commands of the grid_cli application.
 *
 */

#include <ncbi_pch.hpp>

#include "grid_cli.hpp"

USING_NCBI_SCOPE;

void CGridCommandLineInterfaceApp::SetUp_NetScheduleCmd(
    CGridCommandLineInterfaceApp::EAPIClass api_class)
{
    if (m_Opts.auth.empty())
        m_Opts.auth = PROGRAM_NAME;

    string queue(!m_Opts.queue.empty() ? m_Opts.queue : "noname");

    m_NetScheduleAPI = CNetScheduleAPI(m_Opts.ns_service, m_Opts.auth, queue);

    if (IsOptionSet(eCompatMode))
        m_NetScheduleAPI.UseOldStyleAuth();

    // If api_class == eWorkerNode: m_NetScheduleAPI.EnableWorkerNodeCompatMode();

    if (api_class == eNetScheduleAdmin)
        m_NetScheduleAdmin = m_NetScheduleAPI.GetAdmin();
}

void CGridCommandLineInterfaceApp::SetUp_GridClient()
{
    SetUp_NetScheduleCmd(eNetScheduleAPI);
    SetUp_NetCacheCmd(eNetCacheAPI);
    m_GridClient.reset(new CGridClient(m_NetScheduleAPI.GetSubmitter(),
        m_NetCacheAPI, CGridClient::eManualCleanup,
            CGridClient::eProgressMsgOn));
}

void CGridCommandLineInterfaceApp::PrintJobMeta(const CNetScheduleKey& key)
{
    printf("Job number: %u\n"
        "Created by: %s:%u\n",
        key.id,
        g_NetService_TryResolveHost(key.host).c_str(),
        key.port);

    if (!key.queue.empty())
        printf("Queue name: %s\n", key.queue.c_str());
}

int CGridCommandLineInterfaceApp::Cmd_JobInfo()
{
    SetUp_NetScheduleCmd(eNetScheduleAdmin);

    CNetScheduleAPI::EJobStatus status;

    if (IsOptionSet(eDeferExpiration)) {
        status = m_NetScheduleAPI.GetSubmitter().GetJobStatus(m_Opts.id);
        if (IsOptionSet(eStatusOnly)) {
            PrintLine(CNetScheduleAPI::StatusToString(status));
            return 0;
        }
    } else if (IsOptionSet(eStatusOnly)) {
        PrintLine(CNetScheduleAPI::StatusToString(
            m_NetScheduleAPI.GetExecuter().GetJobStatus(m_Opts.id)));
        return 0;
    }

    if (IsOptionSet(eProgressMessageOnly)) {
        CNetScheduleJob job;
        job.job_id = m_Opts.id;
        m_NetScheduleAPI.GetProgressMsg(job);
        if (!job.progress_msg.empty())
            printf("%s\n", job.progress_msg.c_str());
        return 0;
    }

    PrintJobMeta(CNetScheduleKey(m_Opts.id));

    if (!IsOptionSet(eBrief))
        m_NetScheduleAdmin.DumpJob(NcbiCout, m_Opts.id);
    else
    {
        CNetScheduleJob job;
        job.job_id = m_Opts.id;
        status = m_NetScheduleAPI.GetJobDetails(job);

        printf("Status: %s\n", CNetScheduleAPI::StatusToString(status).c_str());

        if (status == CNetScheduleAPI::eJobNotFound)
            return 0;

        printf("Input size: %lu\n", (unsigned long) job.input.size());

        switch (status) {
        default:
            if (job.output.empty())
                break;
            /* FALL THROUGH */

        case CNetScheduleAPI::eDone:
        case CNetScheduleAPI::eReading:
        case CNetScheduleAPI::eConfirmed:
        case CNetScheduleAPI::eReadFailed:
            printf("Output size: %lu\n", (unsigned long) job.output.size());
            break;
        }

        if (!job.error_msg.empty())
            printf("Error message: %s\n", job.error_msg.c_str());
    }
    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_SubmitJob()
{
    SetUp_GridClient();

    CGridJobSubmitter& submitter(m_GridClient->GetJobSubmitter());

    CNcbiOstream& job_input_stream = submitter.GetOStream();

    char buffer[16 * 1024];
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1,
            sizeof(buffer), m_Opts.input_stream)) > 0) {
        job_input_stream.write(buffer, bytes_read);
        if (job_input_stream.bad()) {
            fprintf(stderr, PROGRAM_NAME ": error while writing job input.\n");
            return 3;
        }
        if (feof(m_Opts.input_stream))
            break;
    }

    printf("%s\n", submitter.Submit().c_str());

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_GetJobOutput()
{
    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_CancelJob()
{
    SetUp_NetScheduleCmd(eNetScheduleAPI);

    m_NetScheduleAPI.GetSubmitter().CancelJob(m_Opts.id);

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_Kill()
{
    SetUp_NetScheduleCmd(eNetScheduleAdmin);

    if (IsOptionSet(eAllJobs))
        m_NetScheduleAdmin.DropQueue();
    else {
        if (!IsOptionSet(eOptionalID)) {
            fprintf(stderr, "kill: job ID required\n");
            return 1;
        }
        m_NetScheduleAdmin.DropJob(m_Opts.id);
    }

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_RequestJob()
{
    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_CommitJob()
{
    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_ReturnJob()
{
    SetUp_NetScheduleCmd(eNetScheduleAPI);

    m_NetScheduleAPI.GetExecuter().ReturnJob(m_Opts.id);

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_UpdateJob()
{
    SetUp_NetScheduleCmd(eNetScheduleAdmin);

    if (IsOptionSet(eForceReschedule))
        m_NetScheduleAdmin.ForceReschedule(m_Opts.id);
    else if (IsOptionSet(eExtendLifetime))
        m_NetScheduleAPI.GetExecuter().JobDelayExpiration(
            m_Opts.id, (unsigned) m_Opts.extend_lifetime_by);

    if (IsOptionSet(eProgressMessage)) {
        CNetScheduleJob job;
        job.job_id = m_Opts.id;
        job.progress_msg = m_Opts.progress_message;
        m_NetScheduleAPI.GetExecuter().PutProgressMsg(job);
    }

    return 0;
}
