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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   Standard test for named service resolution facility
 *
 */

#include "../ncbi_ansi_ext.h"
#include "../ncbi_lbsmd.h"
#include "../ncbi_priv.h"               /* CORE logging facilities */
#ifdef NCBI_OS_MSWIN
#  include "../ncbi_socketp.h"          /* gettimeofday() for MSWIN */
#endif /*NCBI_OS_MSWIN*/
#include <connect/ncbi_tls.h>
#include <stdlib.h>
#ifndef NCBI_OS_MSWIN
#  include <sys/time.h>                 /* gettimeofday() for others */
#endif /*!NCBI_OS_MSWIN*/

#include "test_assert.h"  /* This header must go last */


static unsigned short x_Msb(unsigned short x)
{
    unsigned int y;
    while ((y = x & (x - 1)) != 0)
        x = y;
    return x;
}


static const char* x_OS(TNcbiOSType ostype)
{
    static char buf[40];
    TNcbiOSType msb = x_Msb(ostype);
    switch (msb) {
    case fOS_Unknown:
        return "unknown";
    case fOS_IRIX:
        return "IRIX";
    case fOS_Solaris:
        return "Solaris";
    case fOS_BSD:
        return ostype == fOS_Darwin ? "Darwin" : "BSD";
    case fOS_Windows:
        return (ostype & fOS_WindowsServer) == fOS_WindowsServer
            ? "WindowsServer" : "Windows";
    case fOS_Linux:
        return "Linux";
    default:
        break;
    }
    sprintf(buf, "(%hu)", ostype);
    return buf;
}


static const char* x_Bits(TNcbiCapacity capacity)
{
    static char buf[40];
    switch (capacity) {
    case fCapacity_Unknown:
        return "unknown";
    case fCapacity_32:
        return "32";
    case fCapacity_64:
        return "64";
    case fCapacity_32_64:
        return "32+64";
    default:
        break;
    }
    sprintf(buf, "(%hu)", capacity);
    return buf;
}


static double x_TimeDiff(const struct timeval* end,
                         const struct timeval* beg)
{
    if (end->tv_sec < beg->tv_sec)
        return 0.0;
    if (end->tv_usec < beg->tv_usec) {
        if (end->tv_sec == beg->tv_sec)
            return 0.0;
        return (end->tv_sec - beg->tv_sec - 1)
            + (end->tv_usec - beg->tv_usec + 1000000) / 1000000.0;
    }
    return (end->tv_sec - beg->tv_sec)
        + (end->tv_usec - beg->tv_usec) / 1000000.0;
}


static int x_ComparePortUsage(const void* p1, const void* p2)
{
    const SHINFO_PortUsage* u1 = (const SHINFO_PortUsage*) p1;
    const SHINFO_PortUsage* u2 = (const SHINFO_PortUsage*) p2;
    assert(u1->port != u2->port);
    return u1->port < u2->port ? -1 : 1;
}


/* One can define env.var. 'service'_CONN_HOST to reroute dispatching
 * information to particular dispatching host (instead of default).
 */
int main(int argc, const char* argv[])
{
    const char* service = argc > 1 ? argv[1] : "bounce";
    TSERV_Type types = fSERV_All & ~fSERV_Firewall;
    const char* parameter = 0;
    SConnNetInfo* net_info;
    const SSERV_Info* info;
    const char* arg, *val;
    struct timeval start;
    const char* value;
    int n_found = 0;
    SERV_ITER iter;

    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Short   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);
    CORE_SetLOGFILE_Ex(stderr, eLOG_Trace, eLOG_Fatal, 0/*no auto-close*/);

    /* Make sure can talk HTTPS (DISPD) */
    SOCK_SetupSSL(NcbiSetupTls);

    arg = 0;
    if (argc > 2) {
        int a = 2;
        int/*bool*/ okay = 0/*false*/;
        if (strcasecmp(argv[a],"heap") == 0 || strcasecmp(argv[a],"all") == 0){
            HEAP_Options(eOff, eDefault);
            CORE_LOG(eLOG_Note, "Using slow heap walks with checks");
            okay = 1/*true*/;
        }
        if (strcasecmp(argv[a],"lbsm") == 0 || strcasecmp(argv[a],"all") == 0){
#ifdef NCBI_OS_MSWIN
            if (strcasecmp(argv[a],"lbsm") == 0)
                CORE_LOG(eLOG_Warning, "Option \"lbsm\" has no use on MSWIN");
#else
            LBSMD_FastHeapAccess(eOn);
            CORE_LOG(eLOG_Note, "Using live (faster) LBSM heap access");
#endif /*NCBI_OS_MSWIN*/
            okay = 1/*true*/;
        }
        if ((val = strchr(argv[a], '=')) != 0) {
            arg = argv[a];
            *((char*) val) = '\0';
            if (strcmp(++val, "-") == 0)
                val = 0;
            CORE_LOGF(eLOG_Note, ("Using argument affinity %s=%s%s%s", arg,
                                  val ? "\"" : "",
                                  val ? val  : "NULL",
                                  val ? "\"" : ""));
            okay = 1/*true*/;
        }
        for (a += okay;  a < argc;  ++a) {
            ESERV_Type type;
            if (strcasecmp(argv[a], "ANY") == 0) {
                CORE_LOG(eLOG_Note, "Resetting service type to ANY");
                types &= ~fSERV_All;
                continue;
            }
            if ((value = SERV_ReadType(argv[a], &type)) != 0
                &&  !*value  &&  type != fSERV_Firewall) {
                if (a == 2 + okay) {
                    CORE_LOGF(eLOG_Note,
                              ("Resetting service type to %s", argv[a]));
                    types  = type;
                } else {
                    CORE_LOGF(eLOG_Note,
                              ("Including service type %s", argv[a]));
                    types |= type;
                }
                continue;
            }
            if (strcmp(argv[a], "-r") == 0) {
                if (!(types & fSERV_ReverseDns)) {
                    types |= fSERV_ReverseDns;
                    CORE_LOG(eLOG_Note, "Using reverse DNS search");
                }
                continue;
            }
            if (!parameter) {
                parameter = argv[a];
                CORE_LOGF(eLOG_Note,
                          ("Taking `%s' for LBSM environment search",
                           parameter));
                continue;
            }
            CORE_LOGF(eLOG_Fatal, ("Unknown option `%s'", argv[a]));
        }
    } else
        val = 0;

    CORE_LOGF(eLOG_Note, ("Looking for service `%s' type 0x%04X",
                          service, types & fSERV_All));
    verify((net_info = ConnNetInfo_Create(service)));

    if (!net_info->lb_disable  &&  parameter) {
        value = LBSMD_GetHostParameter(SERV_LOCALHOST, parameter);
        CORE_LOGF(eLOG_Note, ("Querying LBSM host environment `%s': %s%s%s",
                              parameter,
                              &"`"[!value],
                              value ? value : "Not found",
                              &"'"[!value]));
        if (value)
            free((void*) value);
    }

    CORE_LOG(eLOG_Trace, "Opening service mapper");
    if (gettimeofday(&start, 0) != 0)
        memset(&start, 0, sizeof(start));
    iter = SERV_OpenP(service,
                      types | (strpbrk(service,"?*[") ? fSERV_Promiscuous : 0),
                      SERV_LOCALHOST, 0/*port*/, 0.0/*preference*/,
                      net_info, 0/*skip*/, 0/*n_skip*/,
                      getenv("NCBI_EXTERNAL")  ||  getenv("HTTP_NCBI_EXTERNAL")
                      ? 1 : 0, arg, val);
    ConnNetInfo_Destroy(net_info);
    if (iter) {
        HOST_INFO hinfo;
        CORE_LOGF(eLOG_Note,("Using %s service mapper",SERV_MapperName(iter)));
        while ((info = SERV_GetNextInfoEx(iter, &hinfo)) != 0) {
            struct timeval stop;
            double elapsed;
            char* info_str;
            if (gettimeofday(&stop, 0) != 0)
                memset(&stop, 0, sizeof(stop));
            elapsed = x_TimeDiff(&stop, &start);
            info_str = SERV_WriteInfo(info);
            CORE_LOGF(eLOG_Note, ("Server #%-2d (%.6fs) `%s' = %s", ++n_found,
                                  elapsed, SERV_CurrentName(iter),
                                  info_str ? info_str : "?"));
            if (hinfo) {
                static const char kTimeFormat[] = "%m/%d/%y %H:%M:%S";
                int n;
                time_t t;
                char buf[80];
                double array[5];
                SHINFO_Params params;
                const char* e = HINFO_Environment(hinfo);
                const char* a = HINFO_AffinityArgument(hinfo);
                const char* v = HINFO_AffinityArgvalue(hinfo);
                CORE_LOG(eLOG_Note, "  Host info available:");
                CORE_LOGF(eLOG_Note,     ("    CPUs:        %d",
                                          HINFO_CpuCount(hinfo)));
                CORE_LOGF(eLOG_Note,     ("    CPU units:   %d @ %.0fMHz",
                                          HINFO_CpuUnits(hinfo),
                                          HINFO_CpuClock(hinfo)));
                CORE_LOGF(eLOG_Note,     ("    Tasks:       %d",
                                          HINFO_TaskCount(hinfo)));
                if (HINFO_MachineParams(hinfo, &params)) {
                    CORE_LOGF(eLOG_Note, ("    Arch:        %d",
                                          params.arch));
                    CORE_LOGF(eLOG_Note, ("    OSType:      %s",
                                          x_OS(params.ostype)));
                    t = (time_t) params.bootup;
                    strftime(buf, sizeof(buf), kTimeFormat, localtime(&t));
                    CORE_LOGF(eLOG_Note, ("    Kernel:      %hu.%hu.%hu @ %s",
                                          params.kernel.major,
                                          params.kernel.minor,
                                          params.kernel.patch, buf));
                    CORE_LOGF(eLOG_Note, ("    Bits:        %s",
                                          x_Bits(params.bits)));
                    CORE_LOGF(eLOG_Note, ("    Page size:   %lu",
                                          (unsigned long) params.pgsize));
                    t = (time_t) params.startup;
                    strftime(buf, sizeof(buf), kTimeFormat, localtime(&t));
                    CORE_LOGF(eLOG_Note, ("    LBSMD:       %hu.%hu.%hu @ %s",
                                          params.daemon.major,
                                          params.daemon.minor,
                                          params.daemon.patch, buf));
                } else
                    CORE_LOG (eLOG_Note,  "    Mach. info:  unavailable");
                if (HINFO_Memusage(hinfo, array)) {
                    CORE_LOGF(eLOG_Note, ("    Total RAM:   %.2fMB",array[0]));
                    CORE_LOGF(eLOG_Note, ("    Cache RAM:   %.2fMB",array[1]));
                    CORE_LOGF(eLOG_Note, ("    Free  RAM:   %.2fMB",array[2]));
                    CORE_LOGF(eLOG_Note, ("    Total Swap:  %.2fMB",array[3]));
                    CORE_LOGF(eLOG_Note, ("    Free  Swap:  %.2fMB",array[4]));
                } else
                    CORE_LOG (eLOG_Note,  "    Mem. usage:  unavailable");
                if ((n = HINFO_PortUsage(hinfo, 0, 0)) > 0) {
                    SHINFO_PortUsage* ports, x_ports[16];
                    size_t c = sizeof(x_ports) / sizeof(x_ports[0]);
                    int i;
                    if ((size_t) n <= c  ||  !(ports = (SHINFO_PortUsage*)
                                               malloc(n * sizeof(*ports)))) {
                        ports = x_ports;
                    } else
                        c = (size_t) n;
                    verify(HINFO_PortUsage(hinfo, ports, c) == n);
                    if (n > (int) c)
                        n = (int) c;
                    qsort(ports, n, sizeof(*ports), x_ComparePortUsage);
                    for (i = 0;  i < n;  ++i) {
                        CORE_LOGF(eLOG_Note,
                                         ("    Port :%-5d  @ %.1f%%",
                                          ports[i].port, ports[i].used));
                    }
                    if (ports != x_ports)
                        free(ports);
                }
                if (HINFO_LoadAverage(hinfo, array)) {
                    CORE_LOGF(eLOG_Note, ("    Load avg:    %f, %f (BLAST)",
                                          array[0], array[1]));
                } else
                    CORE_LOG (eLOG_Note,  "    Load avg:    unavailable");
                if (a) {
                    assert(*a);
                    CORE_LOGF(eLOG_Note, ("    Affinity:    %s", a));
                }
                if (v) {
                    assert(a);
                    CORE_LOGF(eLOG_Note, ("    Aff. value:  %s%s%s",
                                          *v ? "" : "\"", v, *v ? "" : "\""));
                }
                CORE_LOGF(eLOG_Note,     ("    Environment: %s%s%s",
                                      e? "\"": "", e? e: "NULL", e? "\"": ""));
                free(hinfo);
            }
            if (info_str)
                free(info_str);
            if (gettimeofday(&start, 0) != 0)
                memcpy(&start, &stop, sizeof(start));
        }
        if (SERV_GetNextInfo(iter)  ||  SERV_GetNextInfo(iter))
            CORE_LOG(eLOG_Fatal, "Server entry after EOF");
        CORE_LOG(eLOG_Trace, "Resetting service mapper");
        SERV_Reset(iter);
        CORE_LOG(eLOG_Trace, "Service mapper has been reset");
        if (n_found  &&  !SERV_GetNextInfo(iter))
            CORE_LOG(eLOG_Fatal, "Service not found after reset");
        CORE_LOG(eLOG_Trace, "Closing service mapper");
        SERV_Close(iter);
    }

    if (n_found)
        CORE_LOGF(eLOG_Note, ("%d server(s) found", n_found));
    else
        CORE_LOG(eLOG_Fatal, "Requested service not found");

    CORE_LOG(eLOG_Note, "TEST completed successfully");
    CORE_SetLOG(0);
    return 0;
}
