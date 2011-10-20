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
 *      Test for NCBI C Logging (clog.lib) in MT environment (thread-specific context)
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
};


bool CTestRegApp::TestApp_Init(void)
{
    // API initialization -- call it here if possible
    // NcbiLog_InitForAttachedContext("SomeAppName");
    return true;
}


bool CTestRegApp::TestApp_Exit(void)
{
    // Deinitialize logging API -- call it heer if possible
    // NcbiLog_Destroy();
    return true;
}


bool CTestRegApp::Thread_Run(int idx)
{
    // This call can be skipped, if you don't need to set up an
    // application name. And it is better to make an initialization
    // on application start, before any child thread creation if possible.
    /*
        NcbiLog_InitForAttachedContext("SomeAppName");
    */

    // Create context
    TNcbiLog_Context ctx = NcbiLog_Context_Create();

    // This is just a test. In real application you may call many API's
    // functions only once, or when necessary, after NcbiLog_Init() or
    // first NcbiLog_Context_Create() call. It is an overkill to call
    // them for each context creation.
    NcbiLog_SetDestination(eNcbiLog_Cwd);
    
    for (int i=0; i<kRepeatCount; i++) {
        /* Attach context */
        int res = NcbiLog_Context_Attach(ctx);
        assert(res);

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

        /* Detach context */
        NcbiLog_Context_Detach();
    }

    // Destroy context
    NcbiLog_Context_Destroy(ctx);

    return true;
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[]) 
{
    CTestRegApp app;
    return app.AppMain(argc, argv, 0, eDS_Default, 0);
}
