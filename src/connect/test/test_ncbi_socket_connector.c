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
 * Author:  Denis Vakatov
 *
 * File Description:
 *   Standard test for the the SOCK-based CONNECTOR
 *
 */

#include "ncbi_conntest.h"
#include "../ncbi_ansi_ext.h"
#include "../ncbi_priv.h"
#include <connect/ncbi_socket_connector.h>
#include <connect/ncbi_util.h>
#include <stdlib.h>
/* This header must go last */
#include "test_assert.h"


int main(int argc, const char* argv[])
{
    STimeout  timeout;
    CONNECTOR connector;
    FILE*     data_file;
#define MIN_PORT 5001
    const char*    env;
    const char*    host;
    unsigned short port;
    unsigned int   max_try;
    SOCK           sock;

    /* defaults */
    host         = 0;
    port         = 0;
    max_try      = 2;
    timeout.sec  = 5;
    timeout.usec = 123456;

    /* parse cmd.-line args */
    switch ( argc ) {
    case 5: { /* timeout */
        float fff = 0;
        if (sscanf(argv[4], "%f", &fff) != 1  ||  fff < 0)
            break;
        timeout.sec  = (unsigned int) fff;
        timeout.usec = (unsigned int) ((fff - timeout.sec) * 1000000);
    }
    case 4: { /* max_try  */
        long lll;
        if (sscanf(argv[3], "%ld", &lll) != 1  ||  lll <= 0)
            break;
        max_try = (unsigned int) lll;
    }
    case 3: { /* host, port */
        int iii;
        if (sscanf(argv[2], "%d", &iii) != 1  ||
            iii < MIN_PORT  ||  65535 <= iii)
            break;
        port = (unsigned short) iii;

        if ( !*argv[1] )
            break;
        host = argv[1];
    }
    default:
        break;
    }

    /* bad args? -- Usage */
    if ( !host ) {
        fprintf(stderr,
                "Usage: %s <host> <port> [max_try [timeout]]\n  where"
                "  <port> not less than %d; timeout is a float(in sec)\n",
                argv[0], (int) MIN_PORT);
        return 1 /*error*/;
    }

    /* log and data log streams */
    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);
    CORE_SetLOGFILE(stderr, 0/*false*/);
    data_file = fopen("test_ncbi_socket_connector.log", "ab");
    assert(data_file);

    if ((env = getenv("CONN_DEBUG_PRINTOUT")) != 0 &&
        (strcasecmp(env, "true") == 0 || strcasecmp(env, "1") == 0 ||
         strcasecmp(env, "data") == 0 || strcasecmp(env, "all") == 0)) {
        SOCK_SetDataLoggingAPI(eOn);
    }

    /* Tests for SOCKET CONNECTOR */
    fprintf(stderr,
            "Starting the SOCKET CONNECTOR test...\n"
            "%s:%hu,  timeout = %u.%06u, max # of retry = %u\n",
            host, port, timeout.sec, timeout.usec, max_try);

    connector = SOCK_CreateConnector(host, port, max_try);
    CONN_TestConnector(connector, &timeout, data_file, fTC_SingleBouncePrint);

    connector = SOCK_CreateConnector(host, port, max_try);
    CONN_TestConnector(connector, &timeout, data_file, fTC_SingleBounceCheck);

    connector = SOCK_CreateConnector(host, port, max_try);
    CONN_TestConnector(connector, &timeout, data_file, fTC_Everything);

    /* Tests for OnTop SOCKET CONNECTOR connector */
    fprintf(stderr,
            "Starting the SOCKET CONNECTOR test for \"OnTop\" connectors...\n"
            "%s:%hu,  timeout = %u.%06u, max # of retry = %u\n",
            host, port, timeout.sec, timeout.usec, max_try);

    if (SOCK_Create(host, port, &timeout, &sock) != eIO_Success)
        CORE_LOG(eLOG_Fatal, "Cannot create socket");

    connector = SOCK_CreateConnectorOnTop(sock, max_try);
    CONN_TestConnector(connector, &timeout, data_file, fTC_Everything);

    /* cleanup, exit */
    fclose(data_file);
    CORE_SetLOG(0);
    return 0/*okay*/;
}
