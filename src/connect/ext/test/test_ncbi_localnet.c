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
 *   Test suite for "ncbi_localnet.[ch]"
 *
 */

#include <ncbiconf.h>
#include "../../ncbi_ansi_ext.h"
#include "../../ncbi_priv.h"
#include <connect/ext/ncbi_localnet.h>
#include <connect/ncbi_connutil.h>
#include <connect/ncbi_tls.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/* This header must go last */
#include <common/test_assert.h>

#if defined(_DEBUG)  &&  !defined(NDEBUG)
#  define DEBUG_LEVEL  eLOG_Trace
#else
#  define DEBUG_LEVEL  eLOG_Note
#endif /*_DEBUG && !NDEBUG*/


int main(int argc, const char* argv[])
{
    char host[CONN_HOST_LEN + 1];
    TNCBI_IPv6Addr addr;
    const char* env[2];
    const char** e;
    int rv;

    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Short   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);
    CORE_SetLOGFILE_Ex(stderr, DEBUG_LEVEL, eLOG_Fatal, 0/*no auto-close*/);
    SOCK_SetupSSL(NcbiSetupTls);

    if (argc > 1) {
        static const char kRemoteAddr[] = "REMOTE_ADDR=";
        size_t len = strlen(argv[1]);
        char*  str = (char*) malloc(sizeof(kRemoteAddr) + len);
        if (str) {
            memcpy(str, kRemoteAddr, sizeof(kRemoteAddr) - 1);
            memcpy(str + sizeof(kRemoteAddr) - 1, argv[1], len + 1);
        }
        env[0] = str;
        env[1] = 0;
        e = env;
    } else
        e = 0;

    addr = NcbiGetCgiClientIPv6Ex(eCgiClientIP_TryAll, host, sizeof(host), e);
    if (!NcbiIsEmptyIPv6(&addr)) {
        char buf[128 + 40], *ptr;
        SNcbiDomainInfo info;
        int external = !NcbiIsLocalIPEx(&addr, &info);
        if (!(ptr = NcbiAddrToString(buf + 1, sizeof(buf) - 41, &addr)))
            ptr = (char*) memcpy(buf, "[?]", 3) + 3;
        else if (strcasecmp(buf + 1, host) == 0)
            ptr = buf;
        else {
            buf[0] = '[';
            *ptr++ = ']';
        }
        ptr += sprintf(ptr, " is %s", external ? "external" : "local");
        if (info.num) {
            assert(!external);
            sprintf(ptr, ", %s[%u]", info.sfx, info.num);
        }
        printf("Client %s%s\n", host, buf);
        rv = 0;
    } else {
        CORE_LOGF(eLOG_Error,
                  ("Cannot determine IP address of the client%s%s%s",
                   *host ? " \"" : "", host, &"\""[!*host]));
        rv = 1;
    }

    if (e)
        free((void*) e[0]);
    CORE_SetLOG(0);
    return rv;
}
