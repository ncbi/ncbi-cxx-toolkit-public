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

#include "myncbi_monitor.hpp"
#include "pubseq_gateway.hpp"

#include "shutdown_data.hpp"
extern SShutdownData    g_ShutdownData;

const size_t    kCheckPeriodSeconds = 60;   // Once per minute


void MyNCBIMonitorThreadedFunction(size_t  my_ncbi_dns_resolve_ok_period_sec,
                                   size_t  my_ncbi_dns_resolve_fail_period_sec,
                                   size_t  my_ncbi_test_ok_period_sec,
                                   size_t  my_ncbi_test_fail_period_sec,

                                   // The two variables below are updated in
                                   // the app members which are called
                                   // periodically
                                   bool *  last_my_ncbi_resolve_oK,
                                   bool *  last_my_ncbi_test_ok)
{
    auto        app = CPubseqGatewayApp::GetInstance();
    size_t      skip_count = 0;
    size_t      seconds_in_resolve_period = 0;
    size_t      seconds_in_test_period = 0;

    uv_loop_t   loop;
    if (uv_loop_init(&loop) != 0) {
        string  err_msg = "Error initialization of a uv loop "
                          "for the my ncbi monitoring thread. "
                          "There will be no my ncbi monitoring.";
        PSG_ERROR(err_msg);
        app->GetAlerts().Register(ePSGS_MyNCBIUvLoop,
                                  err_msg);
        return;
    }

    // Initial dns resolution
    app->DoMyNCBIDnsResolve();

    // Initial my ncbi test
    app->TestMyNCBI(&loop);

    for ( ; ; ) {
        // The thread should wake up once per minute; sleeping for 1 second is
        // to have the shutdown request checking more often
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (g_ShutdownData.m_ShutdownRequested)
            break;

        ++skip_count;
        if (skip_count < 10) {
            continue;
        }
        skip_count = 0;

        // Here: once per second
        ++seconds_in_resolve_period;
        ++seconds_in_test_period;

        size_t  resolve_period = 0;
        if (*last_my_ncbi_resolve_oK) {
            // Need to use the period (longer) when everything is fine
            resolve_period = my_ncbi_dns_resolve_ok_period_sec;
        } else {
            // Need to use the period (shorter) when there was a failure
            resolve_period = my_ncbi_dns_resolve_fail_period_sec;
        }

        if (resolve_period == 0) {
            // Special value: periodic resolve is disabled
            seconds_in_resolve_period = 0;
        } else {
            if (seconds_in_resolve_period % resolve_period == 0) {
                seconds_in_resolve_period = 0;
                app->DoMyNCBIDnsResolve();
            }
        }

        size_t  test_period = 0;
        if (*last_my_ncbi_test_ok) {
            // Need to use the period (longer) when everything is fine
            test_period = my_ncbi_test_ok_period_sec;
        } else {
            // Need to use the period (shorter) when there was a failure
            test_period = my_ncbi_test_fail_period_sec;
        }

        if (test_period == 0) {
            // Special value: periodic test is disabled
            seconds_in_test_period = 0;
        } else {
            if (seconds_in_test_period % test_period == 0) {
                seconds_in_test_period = 0;
                app->TestMyNCBI(&loop);
            }
        }
    }

    // Need to stop the uv_loop properly
    int     rc = uv_loop_close(&loop);
    if (rc != 0) {
        uv_run(&loop, UV_RUN_DEFAULT);
        rc = uv_loop_close(&loop);
        if (rc != 0) {
            PSG_ERROR("Error closing uv loop for my ncbi monitoring thread");
        }
    }
}

