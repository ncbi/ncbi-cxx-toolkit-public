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
 *   Standard test for the service connector
 *
 */

#include <connect/ncbi_service_connector.h>
#include "../ncbi_ansi_ext.h"
#include "../ncbi_priv.h"               /* CORE logging facilities */
#include <locale.h>
#include <stdlib.h>
#include <time.h>
/* This header must go last */
#include "test_assert.h"


int main(int argc, const char* argv[])
{
    const char* service = argc > 1 && *argv[1] ? argv[1] : "bounce";
    static char obuf[8192 + 2];
    SConnNetInfo* net_info;
    CONNECTOR connector;
    EIO_Status status;
    char ibuf[1024];
    CONN conn;
    size_t n;

    setlocale(LC_ALL, "");
    g_NCBI_ConnectRandomSeed = (int) time(0) ^ NCBI_CONNECT_SRAND_ADDEND;
    srand(g_NCBI_ConnectRandomSeed);

    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);
    CORE_SetLOGFILE(stderr, 0/*false*/);

    net_info = ConnNetInfo_Create(service);
    ConnNetInfo_AppendArg(net_info, "testarg",  "val");
    ConnNetInfo_AppendArg(net_info, "service",  "none");
    ConnNetInfo_AppendArg(net_info, "platform", "none");
    ConnNetInfo_AppendArg(net_info, "address",  "2010");
    ConnNetInfo_Log(net_info, eLOG_Note, CORE_GetLOG());

    connector = SERVICE_CreateConnectorEx(service, fSERV_Any, net_info, 0);

    if (!connector)
        CORE_LOG(eLOG_Fatal, "Failed to create service connector");

    if (CONN_Create(connector, &conn) != eIO_Success)
        CORE_LOG(eLOG_Fatal, "Failed to create connection");

    if (argc > 2) {
        strncpy0(obuf, argv[2], sizeof(obuf) - 2);
        obuf[n = strlen(obuf)] = '\n';
        obuf[++n]              = '\0';
        if (CONN_Write(conn, obuf, strlen(obuf), &n, eIO_WritePersist)
            != eIO_Success) {
            CONN_Close(conn);
            CORE_LOG(eLOG_Fatal, "Cannot write to connection");
        }
        assert(n == strlen(obuf));
    } else {
        for (n = 0; n < 10; n++) {
            size_t m;
            for (m = 0; m < sizeof(obuf) - 2; m++)
                obuf[m] = "0123456789\n"[rand() % 11];
            obuf[m++] = '\n';
            obuf[m]   = '\0';

            if (CONN_Write(conn, obuf, strlen(obuf), &m, eIO_WritePersist)
                != eIO_Success) {
                if (!n) {
                    CONN_Close(conn);
                    CORE_LOG(eLOG_Fatal, "Cannot write to connection");
                } else
                    break;
            }
            assert(m == strlen(obuf));
        }
    }

    for (;;) {
       if (CONN_Wait(conn, eIO_Read, net_info->timeout) != eIO_Success) {
            CONN_Close(conn);
            CORE_LOG(eLOG_Fatal, "Failed to wait for reading");
        }

        status = CONN_Read(conn, ibuf, sizeof(ibuf), &n, eIO_ReadPersist);
        if (n) {
            char* descr = CONN_Description(conn);
            CORE_DATAF(eLOG_Note, ibuf, n,
                       ("%lu bytes read from service (%s%s%s):",
                        (unsigned long) n, CONN_GetType(conn),
                        descr ? ", " : "", descr ? descr : ""));
            if (descr)
                free(descr);
        }
        if (status != eIO_Success) {
            if (status != eIO_Closed)
                CORE_LOGF(n ? eLOG_Error : eLOG_Fatal,
                          ("Read error: %s", IO_StatusStr(status)));
            break;
        }
    }

    ConnNetInfo_Destroy(net_info);
    CONN_Close(conn);

#if 0
    CORE_LOG(eLOG_Note, "Trying ID1 service");

    net_info = ConnNetInfo_Create(service);
    connector = SERVICE_CreateConnectorEx("ID1", fSERV_Any, net_info);
    ConnNetInfo_Destroy(net_info);

    if (!connector)
        CORE_LOG(eLOG_Fatal, "Service ID1 not available");

    if (CONN_Create(connector, &conn) != eIO_Success)
        CORE_LOG(eLOG_Fatal, "Failed to create connection");

    if (CONN_Write(conn, "\xA4\x80\x02\x01\x02\x00", 7, &n, eIO_WritePersist)
        != eIO_Success) {
        CONN_Close(conn);
        CORE_LOG(eLOG_Fatal, "Cannot write to service ID1");
    }
    assert(n == 7);

    if (CONN_Read(conn, ibuf, sizeof(ibuf), &n, eIO_ReadPlain) != eIO_Success){
        CONN_Close(conn);
        CORE_LOG(eLOG_Fatal, "Cannot read from service ID1");
    }

    CORE_LOGF(eLOG_Note, ("%d bytes read from service ID1", n));
    CONN_Close(conn);
#endif

    CORE_LOG(eLOG_Note, "TEST completed successfully");
    CORE_SetLOG(0);
    return 0/*okay*/;
}
