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
 * Authors:  Sergey Satskiy
 *
 * File Description: NetStorage service thread
 *
 *
 */

#include <ncbi_pch.hpp>

#include "nst_service_thread.hpp"
#include "nst_server.hpp"
#include "nst_util.hpp"


BEGIN_NCBI_SCOPE


// The thread does various service periodic checks
void  CNetStorageServiceThread::DoJob(void)
{
    time_t      current_time = time(0);

    // Check that the config file is the original one every 60 seconds
    if (current_time - m_LastConfigFileCheck >= 60) {
        x_CheckConfigFile();
        m_LastConfigFileCheck = current_time;
    }
}


void  CNetStorageServiceThread::x_CheckConfigFile(void)
{
    CNcbiApplication *      app = CNcbiApplication::Instance();
    vector<string>          config_checksum_warnings;
    string                  config_checksum = NST_GetConfigFileChecksum(
                                                    app->GetConfigPath(),
                                                    config_checksum_warnings);
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

