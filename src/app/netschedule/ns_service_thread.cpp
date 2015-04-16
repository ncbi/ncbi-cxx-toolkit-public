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

    time_t      current_time = time(0);

    // Check for shutdown is done every 10 seconds
    if (current_time - m_LastDrainCheck >= 10) {
        x_CheckDrainShutdown();
        m_LastDrainCheck = current_time;
    }

    // Check that the config file is the original one every 60 seconds
    if (current_time - m_LastConfigFileCheck >= 60) {
        x_CheckConfigFile();
        m_LastConfigFileCheck = current_time;
    }

    if (!m_StatisticsLogging)
        return;

    if (current_time - m_LastStatisticsOutput >= m_StatisticsInterval) {
        // Here: it's time to log counters
        m_LastStatisticsOutput = current_time;

        // Print statistics for all the queues
        size_t      aff_count = 0;
        m_QueueDB.PrintStatistics(aff_count);
        CStatisticsCounters::PrintServerWide(aff_count);
    }
}


void  CServiceThread::x_CheckDrainShutdown(void)
{
    if (m_Server.IsDrainShutdown() == false)
        return;     // No request to shut down

    if (m_Server.GetCurrentSubmitsCounter() != 0)
        return;     // There are still submitters

    if (m_QueueDB.AnyJobs())
        return;     // There are still jobs

    // 0 is a signal number
    // true means the DB has been drained successfully
    m_Server.SetShutdownFlag(0, true);
}


void  CServiceThread::x_CheckConfigFile(void)
{
    CNcbiApplication *      app = CNcbiApplication::Instance();
    vector<string>          config_checksum_warnings;
    string                  config_checksum = NS_GetConfigFileChecksum(
                        app->GetConfigPath(), config_checksum_warnings);
    if (config_checksum == m_Server.GetDiskConfigFileChecksum()) {
        // The current is the same as it was last time
        return;
    }

    // Memorize the current disk config file checksum
    m_Server.SetDiskConfigFileChecksum(config_checksum);
    if (!config_checksum_warnings.empty()) {
        string  alert_msg;
        for (vector<string>::const_iterator
                k = config_checksum_warnings.begin();
                k != config_checksum_warnings.end(); ++k) {
            if (!alert_msg.empty())
                alert_msg += "\n";
            alert_msg += *k;
            ERR_POST(*k);
        }
        m_Server.RegisterAlert(eConfigOutOfSync, alert_msg);
        return;
    }


    // Here: the sum has been calculated properly. Compare it with the RAM
    // version
    if (config_checksum != m_Server.GetRAMConfigFileChecksum()) {
        string      msg = "The configuration file on the disk "
                          "does not match the currently loaded one: " +
                          app->GetConfigPath();
        ERR_POST(msg);
        m_Server.RegisterAlert(eConfigOutOfSync, msg);
    }
}

END_NCBI_SCOPE

