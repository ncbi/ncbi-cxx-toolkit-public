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

#include <connect/ncbi_service_connector.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, const char* argv[])
{
    const char buffer[] = "UUUUUZZZZZZUUUUUUZUZUZZUZUZUZUZUZ\n";
    CONNECTOR connector;
    STimeout  timeout;
    char buf[1024];
    CONN conn;
    size_t n;

    SConnNetInfo *info = ConnNetInfo_Create("stnd1");
    strcpy(info->host, "ray");
    info->debug_printout = 1;
    info->client_mode = eClientModeStatefulCapable;

    connector = SERVICE_CreateConnectorEx("io_bounce", fSERV_Any, info);

    if (!connector) {
        printf("Failed to create service connector\n");
        exit(-1);
    }

    if (CONN_Create(connector, &conn) != eIO_Success) {
        printf("Connection creation failed\n");
        exit(-1);
    }

    timeout.sec  = 5;
    timeout.usec = 123456;

    CONN_SetTimeout(conn, eIO_ReadWrite, &timeout);
    
    if (CONN_Write(conn, buffer, sizeof(buffer) - 1, &n) != eIO_Success ||
        n != sizeof(buffer) - 1) {
        printf("Error writing to connection\n");
        exit (-1);
    }
    
    if (CONN_Wait(conn, eIO_Read, 0) != eIO_Success) {
        printf("Error waiting for reading\n");
        exit(-1);
    }

    if (CONN_Read(conn, buf, sizeof(buf), &n, eIO_Plain) != eIO_Success) {
        printf("Error reading from connection\n");
        exit(-1);
    }
    printf("%d bytes read from service:\n%.*s\n", (int)n, (int)n, buf);

    return 0/*okay*/;
}
