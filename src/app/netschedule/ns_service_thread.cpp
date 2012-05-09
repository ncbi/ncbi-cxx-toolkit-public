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
 * Authors:  Denis Vakatov (design), Sergey Satskiy (implementation)
 *
 * File Description: NetSchedule service thread
 *
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/request_ctx.hpp>

#include "queue_database.hpp"
#include "ns_service_thread.hpp"
#include "ns_statistics_counters.hpp"
#include "ns_handler.hpp"
#include "ns_server.hpp"


BEGIN_NCBI_SCOPE


// The thread does 2 service things:
// - check for drained shutdown every 10 seconds
// - logging statistics counters every 100 seconds if logging is on
void  CServiceThread::DoJob(void)
{
    if (!m_Host.ShouldRun())
        return;

    // Check for shutdown is done every 10 seconds
    x_CheckDrainShutdown();

    if (!m_StatisticsLogging)
        return;

    time_t      current_time = time(0);
    if (current_time - m_LastStatisticsOutput < 100)
        return;     // Wait some more

    // Here: it's time to log counters
    m_LastStatisticsOutput = current_time;

    CRef<CRequestContext>   ctx;

    // Prepare the logging context
    ctx.Reset(new CRequestContext());
    ctx->SetRequestID();


    CDiagContext &      diag_context = GetDiagContext();

    diag_context.SetRequestContext(ctx);
    diag_context.PrintRequestStart()
                .Print("_type", "statistics_thread");

    // Print statistics for all the queues
    size_t      aff_count = 0;
    m_QueueDB.PrintStatistics(aff_count);
    CStatisticsCounters::PrintTotal(aff_count);

    // Close the logging context
    ctx->SetRequestStatus(CNetScheduleHandler::eStatus_OK);
    diag_context.PrintRequestStop();
    ctx.Reset();
    diag_context.SetRequestContext(NULL);
    return;
}


void  CServiceThread::x_CheckDrainShutdown(void)
{
    if (m_Server.IsDrainShutdown() == false)
        return;     // No request to shut down

    if (m_Server.GetCurrentSubmitsCounter() != 0)
        return;     // There are still submitters

    if (m_QueueDB.AnyJobs())
        return;     // There are still jobs

    m_Server.SetShutdownFlag();
    return;
}


END_NCBI_SCOPE

