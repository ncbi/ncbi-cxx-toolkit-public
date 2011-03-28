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

    PrintJobMeta(CNetScheduleKey(m_Opts.id));

    CNetScheduleAPI::EJobStatus status;

    if (IsOptionSet(eDeferExpiration)) {
        status = m_NetScheduleAPI.GetSubmitter().GetJobStatus(m_Opts.id);
        if (IsOptionSet(eStatusOnly) ||
                status == CNetScheduleAPI::eJobNotFound) {
            printf("Status: %s\n",
                CNetScheduleAPI::StatusToString(status).c_str());
            if (status == CNetScheduleAPI::eJobNotFound)
                return 0;
        }
    } else if (IsOptionSet(eStatusOnly)) {
        status = m_NetScheduleAPI.GetExecuter().GetJobStatus(m_Opts.id);
        printf("Status: %s\n", CNetScheduleAPI::StatusToString(status).c_str());
            if (status == CNetScheduleAPI::eJobNotFound)
                return 0;
    }

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

        printf("Input size: %u\n", job.input.size());

        switch (status) {
        default:
            if (job.output.empty())
                break;
            /* FALL THROUGH */

        case CNetScheduleAPI::eDone:
        case CNetScheduleAPI::eReading:
        case CNetScheduleAPI::eConfirmed:
        case CNetScheduleAPI::eReadFailed:
            printf("Output size: %u\n", job.output.size());
            break;
        }

        if (!job.error_msg.empty())
            printf("Error message: %u\n", job.error_msg.c_str());
    }
    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_SubmitJob()
{
    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_GetJobOutput()
{
    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_CancelJob()
{
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
