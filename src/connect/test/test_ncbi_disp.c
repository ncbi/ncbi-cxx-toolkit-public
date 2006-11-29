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

#include "../ncbi_ansi_ext.h"
#include "../ncbi_priv.h"               /* CORE logging facilities */
#include "../ncbi_servicep.h"
#include <connect/ncbi_heapmgr.h>
#include <stdlib.h>
/* This header must go last */
#include "test_assert.h"


/* One can define env.var. 'service'_CONN_HOST to reroute dispatching
 * information to particular dispatching host (instead of default).
 */
int main(int argc, const char* argv[])
{
    const char* service = argc > 1 ? argv[1] : "bounce";
    SConnNetInfo* net_info;
    const SSERV_Info* info;
    int n_found = 0;
    SERV_ITER iter;

    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);
    CORE_SetLOGFILE(stderr, 0/*false*/);
    if (argc > 2) {
        if (strcasecmp(argv[2],"heap") == 0 || strcasecmp(argv[2],"all") == 0){
            HEAP_Options(eOff, eDefault);
            CORE_LOG(eLOG_Note, "Using slow heap access (w/checks)");
        }
        if (strcasecmp(argv[2],"lbsm") == 0 || strcasecmp(argv[2],"all") == 0){
#ifdef NCBI_OS_MSWIN
            if (strcasecmp(argv[2],"lbsm") == 0) {
                CORE_LOG(eLOG_Warning,
                         "Option \"lbsm\" has no useful effect on MS-Windows");
            }
#else
            extern ESwitch LBSMD_FastHeapAccess(ESwitch);
            LBSMD_FastHeapAccess(eOn);
            CORE_LOG(eLOG_Note, "Using live (faster) LBSM heap access");
#endif /*NCBI_OS_MSWIN*/
        }
        if (strcasecmp(argv[2],"lbsm") != 0  &&
            strcasecmp(argv[2],"heap") != 0  &&
            strcasecmp(argv[2],"all")  != 0)
            CORE_LOGF(eLOG_Fatal, ("Unknown option `%s'", argv[2]));
    }

    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));
    net_info = ConnNetInfo_Create(service);
    CORE_LOG(eLOG_Trace, "Opening service mapper");
    /*LBSMD_FastHeapAccess(eOn);*/
    iter = SERV_OpenP(service, (fSERV_All & ~fSERV_Firewall) |
                      (strpbrk(service, "?*") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      0/*external*/, 0/*arg*/, 0/*val*/);
    ConnNetInfo_Destroy(net_info);
    if (iter) {
        HOST_INFO hinfo;
        CORE_LOGF(eLOG_Trace,("%s service mapper has been successfully opened",
                              SERV_MapperName(iter)));
        while ((info = SERV_GetNextInfoEx(iter, &hinfo)) != 0) {
            char* info_str = SERV_WriteInfo(info);
            CORE_LOGF(eLOG_Note, ("Server #%d `%s' = %s", ++n_found,
                                  SERV_CurrentName(iter), info_str));
            if (hinfo) {
                double array[2];
                const char* e = HINFO_Environment(hinfo);
                const char* a = HINFO_AffinityArgument(hinfo);
                const char* v = HINFO_AffinityArgvalue(hinfo);
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
                if (a) {
                    assert(*a);
                    CORE_LOGF(eLOG_Note, ("    Affinity argument: %s", a));
                }
                if (a  &&  v)
                    CORE_LOGF(eLOG_Note, ("    Affinity value:    %s%s%s",
                                          *v ? "" : "\"", v, *v ? "" : "\""));
                CORE_LOGF(eLOG_Note, ("    Host environment: %s%s%s",
                                      e? "\"": "", e? e: "NULL", e? "\"": ""));
                free(hinfo);
            }
            free(info_str);
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
 * Revision 6.30  2006/11/29 17:06:37  lavr
 * MS-Win's version of test to warn about using non-effective "lbsm" option
 *
 * Revision 6.29  2006/11/22 18:08:23  lavr
 * Do not use LBSMD_FastHeapAccess() on Windows (DLL mode requires a modifier)
 *
 * Revision 6.28  2006/11/22 04:07:27  lavr
 * Include "../ncbi_ansi_ext.h" for private implementation of strcasecmp()
 *
 * Revision 6.27  2006/11/22 03:27:55  lavr
 * Make unknown option fatal
 *
 * Revision 6.26  2006/11/21 17:33:58  lavr
 * Implement command line option to control heap access settings
 *
 * Revision 6.25  2006/11/21 14:47:16  lavr
 * Enumerate printed servers
 *
 * Revision 6.24  2006/11/08 20:04:12  lavr
 * Include (commented out) call to LBSMD_FastHeapAccess()
 *
 * Revision 6.23  2006/04/05 15:07:09  lavr
 * Print mapper name first
 *
 * Revision 6.22  2006/03/05 17:43:01  lavr
 * Log service mapper name; extract affinities (if any)
 *
 * Revision 6.21  2006/01/11 16:35:59  lavr
 * Open service iterator for everything (but FIREWALL)
 *
 * Revision 6.20  2005/12/23 18:20:33  lavr
 * Use new SERV_OpenP() for iterator opening (and thus allow service wildcards)
 *
 * Revision 6.19  2005/12/14 21:45:39  lavr
 * Adjust to use new SERV_OpenP() prototype
 *
 * Revision 6.18  2005/07/11 18:49:15  lavr
 * Hashed preference generation algorithm retired (proven to fail often)
 *
 * Revision 6.17  2005/07/11 18:26:05  lavr
 * Allow wildcard in local name searches
 *
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
