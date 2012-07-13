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
 * Author:  Denis Vakatov
 *
 * File Description:
 *   Standard test for the the SOCK-based CONNECTOR
 *
 */

#include <connect/ncbi_socket_connector.h>
#include <connect/ncbi_connutil.h>
#include "../ncbi_ansi_ext.h"
#include "../ncbi_priv.h"               /* CORE logging facilities */
#include "ncbi_conntest.h"
#include <stdlib.h>
/* This header must go last */
#include "test_assert.h"

#define TEST_MAX_TRY 2
#define TEST_TIMEOUT 5.123456

#define _STR(n)     #n
#define  STR(n) _STR(n)


/*ARGSUSED*/
static void s_REG_Get(void* unused, const char* section,
                      const char* name, char* value, size_t value_size)
{
    if (strcasecmp(DEF_CONN_REG_SECTION, section) == 0) {
        if      (strcasecmp(REG_CONN_HOST,    name) == 0)
            *value = '\0';
        if      (strcasecmp(REG_CONN_MAX_TRY, name) == 0)
            strncpy0(value, STR(TEST_MAX_TRY), value_size);
        else if (strcasecmp(REG_CONN_TIMEOUT, name) == 0)
            strncpy0(value, STR(TEST_TIMEOUT), value_size);
    }
}


int main(int argc, const char* argv[])
{
    SConnNetInfo* net_info;
    CONNECTOR     connector;
    FILE*         data_file;
    char          tmo[32];
    SOCK          sock;

    /* log and data log streams */
    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);
    CORE_SetLOGFILE(stderr, 0/*false*/);

    /* registry */
    CORE_SetREG(REG_Create(0, s_REG_Get, 0, 0, 0));

    assert((net_info = ConnNetInfo_Create(0)) != 0);

    /* parse cmd.-line args */
    switch ( argc ) {
    case 5: { /* timeout */
        float fff = 0;
        if (sscanf(argv[4], "%f", &fff) != 1  ||  fff < 0)
            break;
        net_info->tmo.sec  = (unsigned int) fff;
        net_info->tmo.usec = (unsigned int)(fff - net_info->tmo.sec) * 1000000;
        net_info->timeout = &net_info->tmo;
    }
    case 4: { /* max_try  */
        long lll;
        if (sscanf(argv[3], "%ld", &lll) != 1  ||  lll <= 0)
            break;
        net_info->max_try = (unsigned int) lll;
    }
    case 3: { /* host, port */
        int iii;
        if (sscanf(argv[2], "%d", &iii) != 1  ||  iii < 0  || 65535 < iii)
            break;
        net_info->port = (unsigned short) iii;

        if (!*argv[1])
            break;
        strncpy0(net_info->host, argv[1], sizeof(net_info->host) - 1);
    }
    default:
        break;
    }

    /* bad args? -- Usage */
    if (!*net_info->host  ||  !net_info->port) {
        fprintf(stderr,
                "Usage: %s <host> <port> [max_try [timeout]]\n\n",
                argv[0]);
        return 1/*error*/;
    }

    data_file = fopen("test_ncbi_socket_connector.log", "ab");
    assert(data_file);

    if (net_info->debug_printout)
        SOCK_SetDataLoggingAPI(eOn);
    
    if (net_info->timeout) {
        sprintf(tmo, "%u.%06u",
                net_info->timeout->sec, net_info->timeout->usec);
    } else
        strcpy(tmo, "infinite");

    /* Tests for SOCKET CONNECTOR */
    fprintf(stderr,
            "Starting the SOCKET CONNECTOR test...\n"
            "%s:%hu, timeout = %s, max # of retries = %u\n",
            net_info->host, net_info->port, tmo, net_info->max_try);

    connector = SOCK_CreateConnector(net_info->host, net_info->port,
                                     net_info->max_try);
    CONN_TestConnector(connector, net_info->timeout,
                       data_file, fTC_SingleBouncePrint);

    connector = SOCK_CreateConnector(net_info->host, net_info->port,
                                     net_info->max_try);
    CONN_TestConnector(connector, net_info->timeout,
                       data_file, fTC_SingleBounceCheck);

    connector = SOCK_CreateConnector(net_info->host, net_info->port,
                                     net_info->max_try);
    CONN_TestConnector(connector, net_info->timeout,
                       data_file, fTC_Everything);

    /* Tests for OnTop SOCKET CONNECTOR connector */
    fprintf(stderr,
            "Starting the SOCKET CONNECTOR test for \"OnTop\" connectors...\n"
            "%s:%hu, timeout = %s, max # of retries = %u\n",
            net_info->host, net_info->port, tmo, net_info->max_try);

    if (SOCK_Create(net_info->host, net_info->port,
                    net_info->timeout, &sock) != eIO_Success) {
        CORE_LOG(eLOG_Fatal, "Cannot create socket");
    }

    connector = SOCK_CreateConnectorOnTop(sock, net_info->max_try);
    CONN_TestConnector(connector, net_info->timeout,
                       data_file, fTC_Everything);

    /* cleanup, exit */
    ConnNetInfo_Destroy(net_info);
    fclose(data_file);
    CORE_SetREG(0);

    CORE_LOG(eLOG_Note, "TEST completed successfully");
    CORE_SetLOG(0);
    return 0/*okay*/;
}
