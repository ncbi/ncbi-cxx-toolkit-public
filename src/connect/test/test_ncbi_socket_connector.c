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
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.1  2000/04/07 20:06:45  vakatov
 * Initial revision
 *
 * ==========================================================================
 */

#if defined(NDEBUG)
#  undef NDEBUG
#endif 

#include "ncbi_conntest.h"
#include <connect/ncbi_socket_connector.h>
#include <connect/ncbi_util.h>


int main(int argc, const char* argv[])
{
    STimeout  timeout;
    CONNECTOR connector;
    FILE*     data_file;
#define MIN_PORT 5001
    const char*    host;
    unsigned short port;
    unsigned int   max_try;

    /* log and data streams */
    CORE_SetLOGFILE(stderr, 0/*false*/);
    data_file = fopen("test_ncbi_socket_connector.out", "wb");
    assert(data_file);

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
                "Usage: %s <host> <port> [max_try] [timeout]\n  where"
                "  <port> not less than %d; timeout is a float(in sec)\n",
                argv[0], (int) MIN_PORT);
        fclose(data_file);
        CORE_SetLOG(0);
        return 1 /*error*/;
    }

    /* run the tests */
    fprintf(stderr,
            "Starting the CON_SOCK test...\n"
            "%s:%hu,  timeout = %u.%06u, max # of retry = %u\n",
            host, port, timeout.sec, timeout.usec, max_try);

    connector = SOCK_CreateConnector(host, port, max_try);
    CONN_TestConnector(connector, &timeout, data_file,
                       fTC_SingleBouncePrint);

    connector = SOCK_CreateConnector(host, port, max_try);
    CONN_TestConnector(connector, &timeout, data_file,
                       fTC_SingleBounceCheck);

    connector = SOCK_CreateConnector(host, port, max_try);
    CONN_TestConnector(connector, &timeout, data_file,
                       fTC_Everything);

    /* cleanup, exit */
    fclose(data_file);
    CORE_SetLOG(0);
    return 0/*okay*/;
}
