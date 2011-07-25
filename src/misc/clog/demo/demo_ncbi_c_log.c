/* $Id$
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
 * File Description:  Demo application for NCBI C Logging (clog.lib).
 *
 */

#include <misc/clog/ncbi_c_log.h>

#include <stdlib.h>
#include <stdio.h>



/*****************************************************************************
 *  MT-locking
 */

/* Fake MT-lock handler -- for display purposes only */
static int Test_MT_Handler(void* user_data, ENcbiLog_MTLock_Action action)
{
    /*
        MT lock implementation goes here
    */
    return 1 /*true*/;
}


/*****************************************************************************
 *  MAIN
 */

int main(int argc, const char* argv[] /*, const char* envp[]*/)
{
    TNcbiLog_MTLock mt_lock = NcbiLog_MTLock_Create(NULL, NcbiLog_Default_MTLock_Handler);
    /* Or,
       TNcbiLog_MTLock mt_lock = NcbiLog_MTLock_Create(NULL, Test_MT_Handler); 
    */
    
    /* Initialize logging API 
    */
    NcbiLog_Init(argv[0], mt_lock, eNcbiLog_MT_TakeOwnership);
    /* Or,
       NcbiLog_InitMT(argv[0]); -- use default MT handler as above.
       NcbiLog_InitST(argv[0]); -- only for single-threaded applications
    */

    /* Set logging destination 
    */
    NcbiLog_SetDestination(eNcbiLog_Stdout);
    /* Or,
       NcbiLog_SetDestination(eNcbiLog_Default); -- default, can be skipped
       NcbiLog_SetDestination(eNcbiLog_Stdlog);
       NcbiLog_SetDestination(eNcbiLog_Stdout);
       NcbiLog_SetDestination(eNcbiLog_Stderr);
       NcbiLog_SetDestination(eNcbiLog_Disable);
    */
    
    /* Set host name
       NcbiLog_SetHost("SOMEHOSTNAME");
    */
    
    /* Set process/thread ID
       NcbiLog_SetProcessId(pid);
       NcbiLog_SetThreadId(tid);
    */

    /* Start application */
    NcbiLog_AppStart(argv);
    NcbiLog_AppRun();

    /* Standard messages */
    {{
        NcbiLog_SetPostLevel(eNcbiLog_Warning);
        NcbiLog_Trace("Message");
        NcbiLog_Warning("Message");
        NcbiLog_Error("Message");
        NcbiLog_Critical("Message");
        /* NcbiLog_Fatal("Message"); */
    }}

    /* Standard messages with user provided time */
    {{
        time_t timer;
        NcbiLog_SetPostLevel(eNcbiLog_Trace);
        timer = time(0);
        NcbiLog_SetTime(timer, 0);
        NcbiLog_Trace("Use user provided time (1)");
        NcbiLog_Trace("Use user provided time (2)");
        NcbiLog_SetTime(0,0);
        NcbiLog_Trace("Use system local time");
    }}

    /* Request without parameters */
    {{
        NcbiLog_ReqStart(NULL);
        NcbiLog_ReqStop(200, 1, 2);
    }}

    /* Request without parameters -- new ID */
    {{
        NcbiLog_SetRequestId(10);
        NcbiLog_ReqStart(NULL);
        NcbiLog_ReqStop(403, 5, 6);
    }}
    
    /* Request with parameters */
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
        NcbiLog_ReqStop(500, 3, 4);
    }}

    /* Extra & performance logging */
    {{
        double timespan = 1.2345678;
        static const SNcbiLog_Param params[] = {
            { "resource", "test" },
            { "key", "value" },
            { NULL, NULL }
        };
        NcbiLog_Extra(params);
        NcbiLog_Perf(200, timespan, params);
    }}
    
    /* Stop application with exit code 0
    */
    NcbiLog_AppStop(0);
    
    /* Deinitialize logging API
    */
    NcbiLog_Destroy();
    
    return 0;
}
