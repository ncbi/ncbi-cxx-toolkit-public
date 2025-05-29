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
 *   Test suite for "ncbi_dblb.[ch]"
 *
 *   NOTE:  Non-UNIX platforms may experience some lack of functionality.
 *
 */

#include "../../ncbi_ansi_ext.h"
#include "../../ncbi_priv.h"
#include "../../ncbi_servicep.h"
#include "../../ncbi_version.h"
#include <connect/ncbi_tls.h>
#include <connect/ncbi_util.h>
#include <connect/ext/ncbi_dblb.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef NCBI_OS_UNIX
#  include <unistd.h>
#endif /*NCBI_OS_UNIX*/
/* This header must go last */
#include <common/test_assert.h>

#if defined(_DEBUG)  &&  !defined(NDEBUG)
#  define DEBUG_LEVEL  eLOG_Trace
#else
#  define DEBUG_LEVEL  eLOG_Note
#endif /*_DEBUG && !NDEBUG*/


#ifdef NCBI_OS_UNIX

/* See "man -a getopt".
 */
#ifndef NCBI_OS_CYGWIN
extern int opterr, optind, optopt;
extern char* optarg;
#endif /*!NCBI_OS_CYGWIN*/

#else

#define getopt(a, b, c)  EOF
static int opterr, optind = 1, optopt;
static char* optarg;

#endif /*NCBI_OS_UNIX*/


static const char kRevision[] = "$Revision$";


static const char* s_Progname(const char* prog)
{
    const char* s;
    if (!prog  ||  !*prog)
        prog = "test_ncbi_dblb";
    if (!(s = strrchr(prog, '/')))
        s = prog;
    else
#ifdef NCBI_OS_MSWIN
        if (!(s = strrchr(prog, '\\')))
            s = prog;
        else
#endif /*NCBI_OS_MSWIN*/
            ++s;
    return s;
}


#ifdef __GNUC__
static void s_Usage(const char*) __attribute__((noreturn));
#endif /*__GNUC__*/

static void s_Usage(const char* prog)
{
    prog = s_Progname(prog);
    fprintf(stderr,
            "Usage:\n%s [-q] [-n] [-x] [-h target] [-p preference]"
            " server [skip...]\n\n"
            "    -q = Quiet mode (and output to stdout)\n"
            "    -n = Use dotted IP notations for output\n"
            "    -h = Host[:port] of preferred target\n"
            "    -p = Preference of preferred target [0..100]%%\n"
            "    -s = Allow standby servers to be included in result\n"
            "    -x = Return 1 for non-existent service (default: 0)\n"
            "Host:port specs of those to skip may follow the server name\n",
            prog);
    if (strncasecmp(prog, "test_", 5) != 0) {
        fprintf(stderr,
                "\nVersion %s build %s %s [%s]\n\n",
                g_VersionStr(kRevision), __DATE__, __TIME__, NCBI_COMPILER);
    }
    exit(-1);
}


/* returns 1 if the service exists, 0 otherwise */
static int s_ServiceExists(const char* serv)
{
    SSERV_Info* info = SERV_GetInfoP(serv,
                                     fSERV_Promiscuous |
                                     fSERV_ReverseDns |
                                     fSERV_Standalone,
                                     0, 0, 0, NULL,
                                     NULL, 0, 0/*not external*/,
                                     0, 0, 0);
    if (info)
        free(info);
    return NULL != info;
}


int main(int argc, char** argv)
{
    int/*bool*/ onefornonexistent = 0/*false*/; /* controls what main() returns
                                                   for non-existent services:
                                                   true  => returns 1
                                                   false => returns 0 */
    int/*bool*/ numeric = 0/*false*/;
    int/*bool*/ standby = 0/*false*/;
    int/*bool*/ quiet = 0/*false*/;
    const SDBLB_Preference* p = 0;
    SDBLB_Preference preference;
    EDBLB_Status result;
    char hostport[256];
    SDBLB_ConnPoint cp;
    const char* s, *e;
    char server[256];
    char errmsg[80];
    int i, n;
    char* c;

    if (UTIL_HelpRequested(argc, argv))
        s_Usage(argv[0]);

    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Short   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);
    CORE_SetLOGFILE_Ex(stderr, DEBUG_LEVEL, eLOG_Fatal, 0/*no auto-close*/);
    SOCK_SetupSSL(NcbiSetupTls);
    opterr = 0;
    n = optind;
    memset(&preference, 0, sizeof(preference));
    while ((i = getopt(argc, argv, "xqnlsh:p:")) != EOF) {
        switch (i) {
        case 'q':
            quiet = 1/*true*/;
            break;
        case 'n':
            numeric = 1/*true*/;
            break;
        case 's':
            standby = 1/*true*/;
            break;
        case 'x':
            onefornonexistent = 1/*true*/;
            break;
        case 'h':
            if (!(s = SOCK_StringToHostPort(optarg,
                                            &preference.host,
                                            &preference.port))  ||  *s) {
                s_Usage(argv[0]);
            }
            p = &preference;
            break;
        case 'p':
            errno = 0;
            preference.pref = strtod(optarg, &c);
            if (preference.pref < 0.0  ||  preference.pref > 100.0
                ||  errno  ||  *c) {
                s_Usage(argv[0]);
            }
            break;
        default:
            s_Usage(argv[0]);
            break;
        }
        n = optind;
    }
    if (argv[n]  &&  strcmp(argv[n], "--") == 0)
        n++;
    if (n != optind  ||  optind >= argc)
        s_Usage(argv[0]);

    s = DBLB_GetServer
        (argv[optind], standby
         ? fDBLB_AllowFallbackToStandby
         : fDBLB_None, p,
         optind + 1 < argc ? &((const char**) argv)[optind + 1] : 0,
         &cp, server, sizeof(server),
         &result);

    if (!(e = DBLB_StatusStr(result))) {
        sprintf(errmsg, "Unknown error %u", (unsigned int) result);
        e = errmsg;
        i = errno;
    } else
        i = 0;
    if (s) {
        if (cp.host  &&  !numeric
            &&  !SOCK_gethostbyaddr(cp.host, hostport, sizeof(hostport))) {
            numeric = 1;
        }
        if (!cp.host
            ||  (numeric
                 &&  !SOCK_HostPortToString(cp.host, cp.port,
                                            hostport, sizeof(hostport)))) {
            *hostport = '\0';
        } else if (cp.port  &&  !strchr(hostport, ':'))
            sprintf(hostport + strlen(hostport), ":%hu", cp.port);
        if (!quiet) {
            printf("%s -> %s%s%s%s%s%s%s%s%s\n", argv[optind], s,
                   *hostport ? " [" : "", hostport,
                   *hostport ? "]"  : "",
                   *e ? " (" : "", e,
                   e == errmsg ? ", "            : "",
                   e == errmsg ? "Unknown error" : "",
                   *e ? ")"  : "");
        } else if (numeric ? *hostport : *s)
            printf("%s\n", numeric ? hostport : s);
        if      (result == eDBLB_NotFound)
            n = s_ServiceExists(argv[optind]) ? 1 : onefornonexistent;
        else if (result != eDBLB_NoDNSEntry)
            n = !*hostport;
        else
            n = 1;
    } else if (!quiet) {
        CORE_LOGF_ERRNO(eLOG_Fatal, i,
                        ("Cannot resolve `%s': %s", argv[optind], e));
    } else
        n = 1;

    CORE_SetLOG(0);
    return n;
}
