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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   Standard test for named service resolution facility
 *
 */

#include "../ncbi_priv.h"               /* CORE logging facilities */
#include <connect/ncbi_service.h>
#include <stdlib.h>
/* This header must go last */
#include "test_assert.h"


/* One can define env.var. 'service'_CONN_HOST to reroute dispatching
 * information to particular dispatching host (instead of default).
 */
int main(int argc, const char* argv[])
{
    const char* service = argc > 1 ? argv[1] : "bounce";
    int/*bool*/ local = argc > 2;
    const SSERV_Info* info;
    int n_found = 0;
    SERV_ITER iter;

    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);
    CORE_SetLOGFILE(stderr, 0/*false*/);

    CORE_LOGF(eLOG_Note, ("Looking for service `%s' (%s)", service,
                          local ? "locally" : "randomly"));
    CORE_LOG(eLOG_Trace, "Opening service mapper");
    if ((local &&
         (iter = SERV_Open(service, fSERV_Any, SERV_LOCALHOST, 0)) != 0) ||
        (!local && (iter = SERV_OpenSimple(service)) != 0)) {
        HOST_INFO hinfo;
        CORE_LOG(eLOG_Trace, "Service mapper has been successfully opened");
        while ((info = SERV_GetNextInfoEx(iter, &hinfo)) != 0) {
            char* info_str = SERV_WriteInfo(info);
            CORE_LOGF(eLOG_Note, ("Service `%s' = %s", service, info_str));
            if (hinfo) {
                double array[2];
                const char* e = HINFO_Environment(hinfo);
                CORE_LOG(eLOG_Note, "  Host info available:");
                CORE_LOGF(eLOG_Note, ("    Number of CPUs: %d",
                                      HINFO_CpuCount(hinfo)));
                CORE_LOGF(eLOG_Note, ("    Number of tasks: %d",
                                      HINFO_TaskCount(hinfo)));
                if (HINFO_LoadAverage(hinfo, array)) {
                    CORE_LOGF(eLOG_Note, ("    Load averages: %f, %f (BLAST)",
                                          array[0], array[1]));
                } else
                    CORE_LOG (eLOG_Note,  "    Load average: unavailable");
                CORE_LOGF(eLOG_Note, ("    Host environment: %s%s%s",
                                      e? "\"": "", e? e: "NULL", e? "\"": ""));
                free(hinfo);
            }
            free(info_str);
            n_found++;
        }
        CORE_LOG(eLOG_Trace, "Resetting service mapper");
        SERV_Reset(iter);
        CORE_LOG(eLOG_Trace, "Service mapper has been reset");
        if (n_found && !(info = SERV_GetNextInfo(iter)))
            CORE_LOG(eLOG_Fatal, "Service not found after reset");
        CORE_LOG(eLOG_Trace, "Closing service mapper");
        SERV_Close(iter);
    }

    if (n_found != 0)
        CORE_LOGF(eLOG_Note, ("Test complete: %d server(s) found", n_found));
    else
        CORE_LOG(eLOG_Fatal, "Requested service not found");

#if 0
    {{
        SConnNetInfo* net_info;
        net_info = ConnNetInfo_Create(service);
        iter = SERV_Open(service, fSERV_Http, SERV_LOCALHOST, net_info);
        ConnNetInfo_Destroy(net_info);
    }}

    if (iter != 0) {
        while ((info = SERV_GetNextInfo(iter)) != 0) {
            char* info_str = SERV_WriteInfo(info);
            CORE_LOGF(eLOG_Note, ("Service `%s' = %s", service, info_str));
            free(info_str);
            n_found++;
        }
        SERV_Close(iter);
    }
#endif

    CORE_SetLOG(0);
    return 0;
}


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.16  2004/06/21 18:02:45  lavr
 * Test on service "bounce" by default ("io_bounce" is retired now)
 *
 * Revision 6.15  2003/05/14 03:58:43  lavr
 * Match changes in respective APIs of the tests
 *
 * Revision 6.14  2002/10/29 22:15:52  lavr
 * Host info output slightly modified
 *
 * Revision 6.13  2002/10/29 00:35:47  lavr
 * Added tests for host info API
 *
 * Revision 6.12  2002/04/15 19:21:44  lavr
 * +#include "../test/test_assert.h"
 *
 * Revision 6.11  2002/03/22 19:48:57  lavr
 * Removed <stdio.h>: included from ncbi_util.h or ncbi_priv.h
 *
 * Revision 6.10  2002/02/20 20:56:49  lavr
 * Added missing calls to free(server_info)
 *
 * Revision 6.9  2001/11/29 22:20:52  lavr
 * Flow control trace messages added
 *
 * Revision 6.8  2001/09/24 20:35:34  lavr
 * +Test for SERV_Reset()
 *
 * Revision 6.7  2001/07/18 17:44:18  lavr
 * Added parameter to switch to local test
 *
 * Revision 6.6  2001/03/20 22:14:08  lavr
 * Second test added to list service by server type (yet #if 0'ed out)
 *
 * Revision 6.5  2001/03/09 04:58:26  lavr
 * Typo (made of pretty styling by vakatov) corrected in comparison
 *
 * Revision 6.4  2001/03/08 17:56:25  lavr
 * Redesigned to show that SERV_Open can return SERV_ITER, that
 * in turn returns 0 even if used for the very first time.
 *
 * Revision 6.3  2001/03/05 23:21:11  lavr
 * SERV_WriteInfo take only one argument now
 *
 * Revision 6.2  2001/03/02 20:01:38  lavr
 * SERV_Close() shown; "../ncbi_priv.h" explained
 *
 * Revision 6.1  2001/03/01 00:33:59  lavr
 * Initial revision
 *
 * ==========================================================================
 */
