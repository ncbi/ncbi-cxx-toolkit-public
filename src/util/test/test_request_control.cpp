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
#include <util/request_control.hpp>

#include <test/test_assert.h>  /* This header must go last */

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


// Sleep little more than 'x' seconds to avoid race conditions,
// because SleepSec can sleep on some platforms a some microseconds
// less than 'x' seconds even.

#define SLEEP(x) SleepMilliSec(x*1000+100)


int CTest::Run(void)
{
    {{
        // Forbid zero requests per any time
        CRequestRateControl mgr(0, CTimeSpan(0,0), CTimeSpan(1,0),
                                CRequestRateControl::eErrCode);
        assert( !mgr.Approve() );
    }}
    {{
        // Allow only 1 request per any period of time
        CRequestRateControl mgr(1, CTimeSpan(0,0), CTimeSpan(0,0),
                                CRequestRateControl::eErrCode);
        assert( mgr.Approve() );
        assert( !mgr.Approve() );

    }}
    {{
        // Allow any number of requests with frequency 1 request per second
        CRequestRateControl mgr(1, CTimeSpan(0,0), CTimeSpan(1,0),
                                CRequestRateControl::eErrCode);
        assert( mgr.Approve() );
        assert( !mgr.Approve() );
        SLEEP(1);
        assert( mgr.Approve() );
    }}
    {{
        // Allow 2 request per second with any frequency
        CRequestRateControl mgr(2, CTimeSpan(1,0), CTimeSpan(0,0),
                                CRequestRateControl::eErrCode);
        assert( mgr.Approve() );
        assert( mgr.Approve() );
        assert( !mgr.Approve() );
        SLEEP(1);
        assert( mgr.Approve() );
        assert( mgr.Approve() );
        assert( !mgr.Approve() );
    }}
    {{
        // Allow 2 request per 3 seconds with frequency 1 request per second
        CRequestRateControl mgr(2, CTimeSpan(3,0), CTimeSpan(1,0),
                                CRequestRateControl::eErrCode);
        assert( mgr.Approve() );
        assert( !mgr.Approve() );
        SLEEP(1);
        assert( mgr.Approve() );
        assert( !mgr.Approve() );
        SLEEP(1);
        assert( !mgr.Approve() );
        SLEEP(1);
        assert( mgr.Approve() );
    }}

    // eSleep

    {{
        // Allow any number of requests with frequency 1 request per second
        // with auto sleep beetween requests.
        CRequestRateControl mgr(1, CTimeSpan(0,0), CTimeSpan(1,0),
                                CRequestRateControl::eSleep);
        CStopWatch sw(true);
        assert( mgr.Approve() );
        assert( mgr.Approve() );
        assert( mgr.Approve() );
        // The StopWatch is more accurate then Sleep methods.
        assert( sw.Elapsed() >= 1.9); 
    }}
    {{
        // Allow 2 request per 3 seconds with frequency 1 request per second
        // with auto sleep beetween requests.
        CRequestRateControl mgr(2, CTimeSpan(3,0), CTimeSpan(1,0),
                                CRequestRateControl::eSleep);
        CStopWatch sw(true);
        // sleep 0
        assert( mgr.Approve() );
        double e = sw.Elapsed();
        assert (e < 0.5);
        // sleep 1
        assert( mgr.Approve() );
        e = sw.Elapsed();
        assert (e >= 0.9  &&  e <= 1.1);
        // sleep 2
        assert( mgr.Approve() );
        e = sw.Elapsed();
        assert (e >= 2.9  &&  e <= 3.1);
        // sleep 1
        assert( mgr.Approve() );
        e = sw.Elapsed();
        assert (e >= 3.9  &&  e <= 4.1);
        // sleep 2
        assert( mgr.Approve() );
        e = sw.Elapsed();
        assert (e >= 5.9  &&  e <= 6.1);
    }}

    return 0;
}


//////////////////////////////////////////////////////////////////////////////
//
// MAIN
//

int main(int argc, const char* argv[])
{
    // Execute main application function
    return CTest().AppMain(argc, argv, 0, eDS_Default, 0);
}

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2005/03/02 18:58:38  ivanov
 * Renaming:
 *    file test_request_throttler.cpp -> test_request_control.cpp
 *    class CRequestThrottler -> CRequestRateControl
 *
 * Revision 1.1  2005/03/02 13:53:06  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
