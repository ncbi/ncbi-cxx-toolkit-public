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
 *   Standard test for the service connector
 *
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.13  2001/06/07 17:53:47  lavr
 * Persistent reading from test connection
 *
 * Revision 6.12  2001/06/01 16:19:10  lavr
 * Added (ifdef'ed out) an internal test connection to service ID1
 *
 * Revision 6.11  2001/05/11 16:05:41  lavr
 * Change log message corrected
 *
 * Revision 6.10  2001/05/11 15:38:01  lavr
 * Print connector type along with read data
 *
 * Revision 6.9  2001/04/24 21:42:43  lavr
 * Brushed code to use CORE_LOG facility only.
 *
 * Revision 6.8  2001/01/25 17:13:22  lavr
 * Added: close/free everything on program exit: useful to check memory leaks
 *
 * Revision 6.7  2001/01/23 23:22:34  lavr
 * debug_printout was given an enum value (instead of "boolean" 1)
 *
 * Revision 6.6  2001/01/09 15:35:20  lavr
 * Removed header <unistd.h>, unknown on WinNT
 *
 * Revision 6.5  2001/01/08 23:13:19  lavr
 * C/C++ -> C only
 *
 * Revision 6.4  2001/01/08 22:42:42  lavr
 * Further development of the test-suite
 *
 * Revision 6.3  2001/01/03 22:40:24  lavr
 * Minor adjustment
 *
 * Revision 6.2  2000/12/29 18:25:27  lavr
 * More tests added (still not yet complete).
 *
 * Revision 6.1  2000/10/20 17:31:07  lavr
 * Initial revision
 *
 * ==========================================================================
 */

#if defined(NDEBUG)
#  undef NDEBUG
#endif 

#include "../ncbi_priv.h"
#include <connect/ncbi_util.h>
#include <connect/ncbi_service_connector.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, const char* argv[])
{
    static char obuf[128] = "UUUUUZZZZZZUUUUUUZUZUZZUZUZUZUZUZ\n";
    const char* service = argc > 1 ? argv[1] : "bounce";
    const char* host = argc > 2 ? argv[2] : "ray";
    CONNECTOR connector;
    SConnNetInfo *info;
    STimeout  timeout;
    char ibuf[1024];
    CONN conn;
    size_t n;

    CORE_SetLOGFILE(stderr, 0/*false*/);

    info = ConnNetInfo_Create(service);
    strcpy(info->host, host);
    if (argc > 3) {
        strncpy(obuf, argv[3], sizeof(obuf) - 2);
        obuf[sizeof(obuf) - 2] = 0;
        obuf[n = strlen(obuf)] = '\n';
        obuf[++n]              = 0;
    }

    connector = SERVICE_CreateConnectorEx(service, fSERV_Any, info);
    ConnNetInfo_Destroy(info);

    if (!connector)
        CORE_LOG(eLOG_Fatal, "Failed to create service connector");

    if (CONN_Create(connector, &conn) != eIO_Success)
        CORE_LOG(eLOG_Fatal, "Failed to create connection");

    timeout.sec  = 5;
    timeout.usec = 123456;

    CONN_SetTimeout(conn, eIO_ReadWrite, &timeout);

    if (CONN_Write(conn, obuf, strlen(obuf), &n) != eIO_Success ||
        n != strlen(obuf)) {
        CONN_Close(conn);
        CORE_LOG(eLOG_Fatal, "Error writing to connection");
    }

    if (CONN_Wait(conn, eIO_Read, 0) != eIO_Success) {
        CONN_Close(conn);
        CORE_LOG(eLOG_Fatal, "Error waiting for reading");
    }

    if (CONN_Read(conn, ibuf, n, &n, eIO_Persist) != eIO_Success) {
        CONN_Close(conn);
        CORE_LOG(n ? eLOG_Error : eLOG_Fatal, "Error reading from connection");
    }

    CORE_LOGF(eLOG_Note,
              ("%d bytes read from service (%s):\n%.*s",
               (int)n, CONN_GetType(conn), (int)n, ibuf));
    CONN_Close(conn);

#if 0
    CORE_LOG(eLOG_Note, "Trying ID1 service");

    info = ConnNetInfo_Create(service);
    connector = SERVICE_CreateConnectorEx("ID1", fSERV_Any, info);
    ConnNetInfo_Destroy(info);

    if (!connector)
        CORE_LOG(eLOG_Fatal, "Service ID1 not available");

    if (CONN_Create(connector, &conn) != eIO_Success)
        CORE_LOG(eLOG_Fatal, "Failed to create connection");

    if (CONN_Write(conn, "\xA4\x80\x02\x01\x02\x00\x00", 7, &n) !=
        eIO_Success || n != 7) {
        CONN_Close(conn);
        CORE_LOG(eLOG_Fatal, "Error writing to service ID1");
    }

    if (CONN_Read(conn, ibuf, sizeof(ibuf), &n, eIO_Plain) != eIO_Success) {
        CONN_Close(conn);
        CORE_LOG(eLOG_Fatal, "Error reading from service ID1");
    }

    CORE_LOGF(eLOG_Note, ("%d bytes read from service ID1", n));
    CONN_Close(conn);
#endif

    return 0/*okay*/;
}
