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

#include "../ncbi_ansi_ext.h"
#include "../ncbi_priv.h"               /* CORE logging facilities */
#include <connect/ncbi_service_connector.h>
#include <connect/ncbi_tls.h>
#include <stdlib.h>
#include <time.h>

#include "test_assert.h"  /* This header must go last */


static const char kBounce[] = "bounce";


int main(int argc, const char* argv[])
{
    const char* service = argc > 1 && *argv[1] ? argv[1] : kBounce;
    static char obuf[8192 + 2];
    SConnNetInfo* net_info;
    CONNECTOR connector;
    EIO_Status status;
    char ibuf[1024];
    size_t n, m, k;
    char* descr;
    CONN conn;
    SOCK sock;

    g_NCBI_ConnectRandomSeed
        = (unsigned int) time(0) ^ NCBI_CONNECT_SRAND_ADDEND;
    srand(g_NCBI_ConnectRandomSeed);

    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Short   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);
    CORE_SetLOGFILE_Ex(stderr, eLOG_Trace, eLOG_Fatal, 0/*no auto-close*/);

    SOCK_SetupSSL(NcbiSetupTls);

    net_info = ConnNetInfo_Create(service);
    /* Some bogus values / extras (to check overrides) */
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

    if (argc <= 2) {
        m = 0;
        for (n = 0;  n < 10;  ++n) {
            for (k = 0;  k < sizeof(obuf) - 2;  ++k)
                obuf[k] = "0123456789\n"[rand() % 11];
            obuf[k++] = '\n';
            obuf[k]   = '\0';
            k = strlen(obuf);
            if (CONN_Write(conn, obuf, k, &k, eIO_WritePersist) != eIO_Success) {
                if (!n) {
                    CONN_Close(conn);
                    CORE_LOG(eLOG_Fatal, "Cannot write to connection");
                }
                m += k;
                break;
            }
            assert(k == strlen(obuf));
            m += k;
        }
    } else if (strcmp(argv[2], "") != 0) {
        strncpy0(obuf, argv[2], sizeof(obuf) - 2);
        obuf[m = strlen(obuf)] = '\n';
        obuf[++m]              = '\0';
        if (CONN_Write(conn, obuf, strlen(obuf), &m, eIO_WritePersist) != eIO_Success) {
            CONN_Close(conn);
            CORE_LOG(eLOG_Fatal, "Cannot write to connection");
        }
        assert(m == strlen(obuf));
    } else
        m = 0;
    if (CONN_GetSOCK(conn, &sock) == eIO_Success)
        verify(SOCK_Shutdown(sock, eIO_Write) == eIO_Success);
    descr = CONN_Description(conn);
    if (m) {
        CORE_LOGF(eLOG_Note,
                  ("Total byte(s) sent to (%s%s%s): %lu",
                   CONN_GetType(conn),
                   descr ? ", " : "", descr ? descr : "",
                   (unsigned long) m));
    }

    n = 0;
    for (;;) {
        status = CONN_Wait(conn, eIO_Read, net_info->timeout);
        if (status != eIO_Success) {
            CORE_LOGF(eLOG_Fatal,
                      ("Failed to wait for read from (%s%s%s) at offset %lu: %s",
                       CONN_GetType(conn),
                       descr ? ", " : "", descr ? descr : "",
                       (unsigned long) n,
                       IO_StatusStr(status)));
            /*NOTREACHED*/
            break;
        }

        status = CONN_Read(conn, ibuf, sizeof(ibuf), &k, eIO_ReadPersist);
        if (k) {
            CORE_DATAF(eLOG_Note, ibuf, k,
                       ("Got data from (%s%s%s) at offset %lu",
                        CONN_GetType(conn),
                        descr ? ", " : "", descr ? descr : "",
                        (unsigned long) n));
            n += k;
        }
        if (status != eIO_Success) {
            CORE_LOGF(eLOG_Note,
                      ("Total byte(s) sent to / received from (%s%s%s): %lu / %lu",
                       CONN_GetType(conn),
                       descr ? ", " : "", descr ? descr : "",
                       (unsigned long) m, (unsigned long) n));
            if (status != eIO_Closed) {
                CORE_LOGF(eLOG_Fatal,
                          ("Read error from (%s%s%s) at offset %lu: %s",
                           CONN_GetType(conn),
                           descr ? ", " : "", descr ? descr : "",
                           (unsigned long) n,
                           IO_StatusStr(status)));
            }
            if (service == kBounce  &&  m != n) {
                CORE_LOGF(eLOG_Fatal,
                          ("Not entirely bounced (%s%s%s)",
                           CONN_GetType(conn),
                           descr ? ", " : "", descr ? descr : ""));
            }
            break;
        }
    }
    if (descr)
        free(descr);

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
