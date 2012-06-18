/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *   This software/database is a "United States Government Work" under the
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
 * Author:  Vladimir Ivanov
 *
 * File Description:  Test program for CRequestRateControl class
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/request_control.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;


//////////////////////////////////////////////////////////////////////////////
//
// Test application
//

class CTest : public CNcbiApplication
{
public:
    void Init(void);
    int  Run(void);
};


void CTest::Init(void)
{
    SetDiagPostLevel(eDiag_Warning);
}


// Sleep little more than 'x' milliseconds to compensate for inaccuracy
// of Sleep*() methods on some platforms.
#define SLEEP(x) SleepMilliSec(x+100)

// Print elapsed time and restart timer
#define ELAPSED \
    e = sw.Restart(); \
    LOG_POST("elapsed: " + NStr::DoubleToString(e, -1, NStr::fDoubleFixed)); \
    SleepMilliSec(100)

// Start/stop test messages
#define START(x) \
    LOG_POST("Start test " << x)
#define DONE \
    LOG_POST("DONE")


int CTest::Run(void)
{
    double e;

    START(1);
    // Forbid zero requests per any time.
    {{
        CRequestRateControl mgr(0, CTimeSpan(0,0), CTimeSpan(1,0),
                                CRequestRateControl::eErrCode);
        assert( !mgr.Approve() );
    }}
    DONE;

    START(2);
    // Allow only 1 request per any period of time.
    {{
        CRequestRateControl mgr(1, CTimeSpan(0,0), CTimeSpan(0,0),
                                CRequestRateControl::eErrCode);
        assert( mgr.Approve() );
        assert( !mgr.Approve() );
    }}
    DONE;

    START(3);
    // Allow any number of requests with frequency 1 request per second
    // The behaviour is the same for both modes.
    {{
        CRequestRateControl mgr(1, CTimeSpan(0,0), CTimeSpan(1,0),
                                CRequestRateControl::eErrCode);
        assert( mgr.Approve() );
        assert( !mgr.Approve() );
        SLEEP(1000);
        assert( mgr.Approve() );
    }}
    DONE;

    START(4);
    // Allow 2 request per second with any frequency.
    // The behaviour is different for eContinuous/eDiscrete modes.
    {{
        {{
            CRequestRateControl mgr(2, CTimeSpan(1,0), CTimeSpan(0,0),
                                    CRequestRateControl::eErrCode);
            assert( mgr.Approve() );
            assert( mgr.Approve() );
            assert( !mgr.Approve() );
            SLEEP(1000);
            assert( mgr.Approve() );
            assert( mgr.Approve() );
            assert( !mgr.Approve() );
            SLEEP(1000);
            // Now, will try to split up time period
            assert( mgr.Approve() );
            SLEEP(500);
            assert( mgr.Approve() );
            assert( !mgr.Approve() );
            SLEEP(500);
            // Time period is float, so only first request will be approved.
            // See difference with eDiscrete mode below.
            assert( mgr.Approve() );
            assert( !mgr.Approve() );
        }}
        {{
            CRequestRateControl mgr(2, CTimeSpan(1,0), CTimeSpan(0,0),
                                    CRequestRateControl::eErrCode,
                                    CRequestRateControl::eDiscrete);
            assert( mgr.Approve() );
            assert( mgr.Approve() );
            assert( !mgr.Approve() );
            SLEEP(1000);
            assert( mgr.Approve() );
            assert( mgr.Approve() );
            assert( !mgr.Approve() );
            SLEEP(1000);
            // Now, will try to split up time period
            assert( mgr.Approve() );
            SLEEP(500);
            assert( mgr.Approve() );
            assert( !mgr.Approve() );
            SLEEP(500);
            // Time period ends, so both request will be approved.
            // See difference with eContinuous mode above
            assert( mgr.Approve() );
            assert( mgr.Approve() );
        }}
    }}
    DONE;

    START(5);
    // Allow 2 request per 3 seconds with frequency 1 request per second.
    // In this test behaviour of eContinuous and eDiscrete modes are the same
    // for 3 second time interval. 
    {{
        {{
            CRequestRateControl mgr(2, CTimeSpan(3,0), CTimeSpan(1,0),
                                    CRequestRateControl::eErrCode);
            assert( mgr.Approve() );
            assert( !mgr.Approve() );
            SLEEP(1000);
            assert( mgr.Approve() );
            assert( !mgr.Approve() );
            SLEEP(1000);
            assert( !mgr.Approve() );
            SLEEP(1000);
            assert( mgr.Approve() );
        }}
        {{
            CRequestRateControl mgr(2, CTimeSpan(3,0), CTimeSpan(1,0),
                                    CRequestRateControl::eErrCode,
                                    CRequestRateControl::eDiscrete);
            assert( mgr.Approve() );
            assert( !mgr.Approve() );
            SLEEP(1000);
            assert( mgr.Approve() );
            assert( !mgr.Approve() );
            SLEEP(1000);
            assert( !mgr.Approve() );
            SLEEP(1000);
            assert( mgr.Approve() );
        }}
    }}
    DONE;

    // eSleep

    START(6);
    // Allow any number of requests with frequency 1 request per second
    // with auto sleep beetween requests.
    // The behaviour is the same for both modes.
    {{
        {{
            CRequestRateControl mgr(1, CTimeSpan(0,0), CTimeSpan(1,0),
                                    CRequestRateControl::eSleep);
            CStopWatch sw(CStopWatch::eStart);
            assert( mgr.Approve() );
            assert( mgr.Approve() );
            assert( mgr.Approve() );
            ELAPSED;
            // The StopWatch is more accurate then Sleep methods.
            assert( e > 1.8);
        }}
        {{
            CRequestRateControl mgr(1, CTimeSpan(0,0), CTimeSpan(1,0),
                                    CRequestRateControl::eSleep,
                                    CRequestRateControl::eDiscrete);
            CStopWatch sw(CStopWatch::eStart);
            assert( mgr.Approve() );
            assert( mgr.Approve() );
            assert( mgr.Approve() );
            ELAPSED;
            // The StopWatch is more accurate then Sleep methods.
            assert( e > 1.8); 
        }}
    }}
    DONE;

    START(7);
    // Allow 2 request per 3 seconds with frequency 1 request per second
    // with auto sleep beetween requests.
    // The behaviour is different for both modes.
    {{
        CStopWatch sw(CStopWatch::eStart);
        {{
            LOG_POST("eContinuous");
            CRequestRateControl mgr(2, CTimeSpan(3,0), CTimeSpan(1,0),
                                    CRequestRateControl::eSleep);
            sw.Restart();
            // sleep 0
            assert( mgr.Approve() );
            ELAPSED;
            assert (e < 0.5);
            // sleep 1
            assert( mgr.Approve() );
            ELAPSED;
            assert (e > 0.8  &&  e < 1.2);
            // sleep 2
            assert( mgr.Approve() );
            ELAPSED;
            assert (e > 1.8  &&  e < 2.2);
            // sleep 1
            // See difference with eDiscrete mode below.
            assert( mgr.Approve() );
            ELAPSED;
            assert (e > 0.8  &&  e < 1.2);
            // sleep 2
            assert( mgr.Approve() );
            ELAPSED;
            assert (e > 1.8  &&  e < 2.2);
        }}
        {{
            LOG_POST("eDiscrete");
            CRequestRateControl mgr(2, CTimeSpan(3,0), CTimeSpan(1,0),
                                    CRequestRateControl::eSleep,
                                    CRequestRateControl::eDiscrete);
            sw.Restart();
            // sleep 0
            assert( mgr.Approve() );
            ELAPSED;
            assert (e < 0.5);
            // sleep 1
            assert( mgr.Approve() );
            ELAPSED;
            assert (e > 0.8  &&  e < 1.2);
            // sleep 2
            assert( mgr.Approve() );
            ELAPSED;
            assert (e > 1.8  &&  e < 2.2);
            // sleep 0
            // See difference with eContinuous mode above
            assert( mgr.Approve() );
            ELAPSED;
            assert (e < 0.5);
            // sleep 1
            assert( mgr.Approve() );
            ELAPSED;
            assert (e > 0.8  &&  e < 1.2);
        }}
    }}
    DONE;

    START(8);
    // Allow any number of requests (throtling is disabled)
    {{
        {{
            CRequestRateControl mgr(CRequestRateControl::kNoLimit);
            CStopWatch sw(CStopWatch::eStart);
            for (int i=0; i<1000; i++) {
                assert( mgr.Approve() );
            }
            ELAPSED;
            assert( e < 0.1); 
        }}
    }}
    DONE;

    START(9);
    // eDiscrete mode test with empty time between requests
    {{
        CRequestRateControl mgr(1000, CTimeSpan(1,0), CTimeSpan(0,0),
                                CRequestRateControl::eErrCode,
                                CRequestRateControl::eDiscrete);
        CStopWatch sw(CStopWatch::eStart);
        for (int i=0; i<1000; i++) {
            assert( mgr.Approve() );
        }
        ELAPSED;
        assert( e < 0.1); 
        assert( !mgr.Approve() );
        SLEEP(1000);
        assert( mgr.Approve() );
    }}

    return 0;
}


//////////////////////////////////////////////////////////////////////////////
//
// MAIN
//

int main(int argc, const char* argv[])
{
    return CTest().AppMain(argc, argv, 0, eDS_Default, 0);
}
