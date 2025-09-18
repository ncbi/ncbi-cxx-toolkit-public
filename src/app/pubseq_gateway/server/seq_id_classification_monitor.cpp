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
 * File Description: PSG server seq-id classification monitor
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <thread>
#include <chrono>

#include <objects/seqloc/Seq_id.hpp>

#include "seq_id_classification_monitor.hpp"
#include "pubseq_gateway.hpp"

#include "shutdown_data.hpp"
extern SShutdownData    g_ShutdownData;


static string   kSuccessfulRefreshMessage = "Seq id classification config files "
                                            "have been successfully refreshed";
static string   kUnknownRefreshErrorMessage = "Unknown error while refreshing "
                                              "seq id classification config files";


void SeqIdClassificationMonitorThreadedFunction(size_t seq_id_refresh_sec)
{
    auto        app = CPubseqGatewayApp::GetInstance();
    size_t      loop_count = 0;

    for ( ; ; ) {
        // The thread should wake up once per minute; sleeping for 1 second is
        // to have the shutdown request checking more often
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (g_ShutdownData.m_ShutdownRequested)
            break;

        ++loop_count;
        if (loop_count < seq_id_refresh_sec * 10) {
            continue;
        }
        loop_count = 0;

        try {
            if (CSeq_id::RefreshAccessionGuide()) {
                // The seq id classifier has been updated.
                PSG_MESSAGE(kSuccessfulRefreshMessage);
                app->GetAlerts().Register(ePSGS_SeqIdClassificationConfigFilesRefreshSuccess,
                                          kSuccessfulRefreshMessage);
            }
        } catch (const exception &  exc) {
            string  err_msg = "Error while refreshing seq id classification "
                              "config files: " + string(exc.what());
            PSG_CRITICAL(err_msg);
            app->GetAlerts().Register(
                    ePSGS_SeqIdClassificationConfigFilesRefreshFailure,
                    err_msg);
        } catch (...) {
            PSG_CRITICAL(kUnknownRefreshErrorMessage);
            app->GetAlerts().Register(
                    ePSGS_SeqIdClassificationConfigFilesRefreshFailure,
                    kUnknownRefreshErrorMessage);
        }
    }
}

