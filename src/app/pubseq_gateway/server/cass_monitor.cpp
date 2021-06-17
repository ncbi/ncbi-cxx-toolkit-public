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
 * File Description: PSG server Cassandra monitoring
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <thread>
#include <chrono>

#include "cass_monitor.hpp"
#include "pubseq_gateway.hpp"

#include "shutdown_data.hpp"
extern SShutdownData    g_ShutdownData;

void CassMonitorThreadedFunction(void)
{
    auto    app = CPubseqGatewayApp::GetInstance();

    for ( ; ; ) {
        // The thread should wake up once per minute
        std::this_thread::sleep_for(std::chrono::seconds(60));

        if (g_ShutdownData.m_ShutdownRequested)
            break;

        switch (app->GetStartupDataState()) {
            case ePSGS_NoCassConnection:
                if (app->OpenCass()) {
                    // Public comments mapping (populated once)
                    app->PopulatePublicCommentsMapping();

                    // true => the 'accept' alert is set if sucessfull
                    if (app->PopulateCassandraMapping(true)) {
                        app->OpenCache();
                    }
                }
                break;
            case ePSGS_NoValidCassMapping:
                // true => the 'accept' alert is set if sucessfull
                if (app->PopulateCassandraMapping(true))
                    app->OpenCache();
                break;
            case ePSGS_NoCassCache:
                app->OpenCache();
                break;
            case ePSGS_StartupDataOK:
                app->CheckCassMapping();
                break;
        }
    }
}

