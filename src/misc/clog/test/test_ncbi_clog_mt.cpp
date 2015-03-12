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
 * Author:  Vladimir Ivanov
 *
 * File Description:
 *      Test for NCBI C Logging (clog.lib) in MT environment
 *
 */


#include <ncbi_pch.hpp>
#include <corelib/test_mt.hpp>
#include <misc/clog/ncbi_c_log.h>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;


const int kRepeatCount = 10;


/////////////////////////////////////////////////////////////////////////////
//  Test application

class CTestRegApp : public CThreadedApp
{
public:
    virtual bool TestApp_Init(void);
    virtual bool TestApp_Exit(void);
    virtual bool Thread_Run(int idx);
    virtual bool Thread_Exit(int idx);
};


bool CTestRegApp::TestApp_Init(void)
{
    // Initialize the C logging API with default MT locking imlementation
    NcbiLog_InitMT(GetAppName().c_str());
    // Set output to files in current directory
    NcbiLog_SetDestination(eNcbiLog_Cwd);
    // Start application
    NcbiLog_AppStart(NULL);
    NcbiLog_AppRun();
    return true;
}


bool CTestRegApp::TestApp_Exit(void)
{
    // Stop application
    NcbiLog_AppStop(0);
    // Deinitialize logging API
    NcbiLog_Destroy();
    return true;
}


bool CTestRegApp::Thread_Run(int idx)
{
    for (int i=0; i<kRepeatCount; i++) {

        /* Standard messages */
        {{
            //NcbiLog_SetPostLevel(eNcbiLog_Trace); // allow Trace -- it is disable by default
            NcbiLog_Trace("Message");
            NcbiLog_Warning("Message");
            NcbiLog_Error("Message");
            NcbiLog_Critical("Message");
        }}

        /* Request without parameters */
        {{
            NcbiLog_ReqStart(NULL);
            NcbiLog_Trace  ("Request without parameters - trace");
            NcbiLog_Warning("Request without parameters - warning");
            NcbiLog_Error  ("Request without parameters - error");
            NcbiLog_ReqStop(200, 1, 2);
        }}

        /* Request with parameters */
        CStopWatch sw(CStopWatch::eStart);
        {{
            static const SNcbiLog_Param params[] = {
                { "k1", "v1" },
                { "k2", "v2" },
                { "",   "v3" },
                { "k4", ""   },
                { "",   ""   },
                { "k5", "v5" },
                { NULL, NULL }
            };
            NcbiLog_SetSession("session name");
            NcbiLog_SetClient("192.168.1.1");
            NcbiLog_ReqStart(params);
            NcbiLog_ReqRun();
            NcbiLog_Trace  ("Request with parameters - trace");
            NcbiLog_Warning("Request with parameters - warning");
            NcbiLog_Error  ("Request with parameters - error");
            NcbiLog_ReqStop(500, 3, 4);
        }}

        /* Extra & performance logging */
        {{
            double timespan = sw.Elapsed();
            static const SNcbiLog_Param params[] = {
                { "resource", "test" },
                { "key", "value" },
                { NULL, NULL }
            };
            NcbiLog_Extra(params);
            NcbiLog_Perf(200, timespan, params);
        }}
    }
    return true;
}


bool CTestRegApp::Thread_Exit(int idx)
{
    /* Destroy thread-specific NcbiLog API information */
    NcbiLog_Destroy_Thread();
    return true;
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[]) 
{
    CTestRegApp app;
    return app.AppMain(argc, argv);
}
