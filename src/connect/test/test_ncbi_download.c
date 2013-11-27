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
 *   Download speed test for a named service
 *
 */

#include <connect/ncbi_service_connector.h>
#include "../ncbi_ansi_ext.h"
#include "../ncbi_priv.h"               /* CORE logging facilities */
#include <stdlib.h>
#include <time.h>

#include "test_assert.h"  /* This header must go last */


int main(int argc, const char* argv[])
{
    const char* service = argc > 1 ? argv[1] : 0;
    static char buf[1 << 16];
    const char* description;
    SConnNetInfo* net_info;
    CONNECTOR connector;
    EIO_Status status;
    time_t elapsed;
    size_t total;
    CONN conn;

    g_NCBI_ConnectRandomSeed = (int) time(0) ^ NCBI_CONNECT_SRAND_ADDEND;
    srand(g_NCBI_ConnectRandomSeed);

    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);
    CORE_SetLOGFILE(stderr, 0/*false*/);

    net_info = ConnNetInfo_Create(service);
    if (argc > 2)
        strncpy0(net_info->host, argv[2], sizeof(net_info->host) - 1);

    connector = SERVICE_CreateConnectorEx(service, fSERV_Any, net_info, 0);

    if (!connector) {
        CORE_LOGF(eLOG_Fatal, ("Failed to create service connector for \"%s\"",
                               service ? service : "<NULL>"));
    }

    if ((status = CONN_Create(connector, &conn)) != eIO_Success) {
        CORE_LOGF(eLOG_Fatal, ("Failed to create connection to \"%s\": %s",
                               service, IO_StatusStr(status)));
    }

    /* force connection establishment */
    if ((status = CONN_Wait(conn, eIO_Read, net_info->timeout))
        != eIO_Success) {
        CONN_Close(conn);
        CORE_LOGF(eLOG_Fatal, ("Failed waiting for read from \"%s\": %s",
                               service, IO_StatusStr(status)));
    }

    description = CONN_Description(conn);
    CORE_LOGF(eLOG_Note, ("Reading from \"%s\" (%s%s%s) please wait...",
                          service, CONN_GetType(conn),
                          description ? ", "        : "",
                          description ? description : ""));

    elapsed = time(0);
    total = 0;

    do {
        size_t n;
        status = CONN_Read(conn, buf, sizeof(buf), &n, eIO_ReadPersist);
        assert(n  ||  status != eIO_Success);
        total += n;
    } while (status == eIO_Success);

    elapsed = time(0) - elapsed;

    if (status == eIO_Closed)
        sprintf(buf, "%.2f KiB/s", elapsed ? total / (1024.0 * elapsed) : 0.0);

    CORE_LOGF(status != eIO_Closed ? eLOG_Fatal :
              total ? eLOG_Note : eLOG_Error,
              ("%lu byte%s read in %lus: %s",
               (unsigned long) total, &"s"[total == 1],
               (unsigned long) elapsed,
               status == eIO_Closed ? buf : IO_StatusStr(status)));

    if (description)
        free((void*) description);
    ConnNetInfo_Destroy(net_info);
    CONN_Close(conn);

    CORE_LOG(eLOG_Note, "TEST completed successfully");
    CORE_SetLOG(0);
    return 0/*okay*/;
}
