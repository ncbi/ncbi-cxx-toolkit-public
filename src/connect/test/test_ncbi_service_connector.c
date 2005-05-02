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
 */

#include "../ncbi_ansi_ext.h"
#include "../ncbi_priv.h"
#include <connect/ncbi_service_connector.h>
#include <stdlib.h>
#include <time.h>
/* This header must go last */
#include "test_assert.h"


int main(int argc, const char* argv[])
{
    static char obuf[8192 + 2] = "UUUUUZZZZZZUUUUUUZUZUZZUZUZUZUZUZ\n";
    const char* service = argc > 1 && *argv[1] ? argv[1] : "bounce";
    const char* host = argc > 2 && *argv[2] ? argv[2] : "www.ncbi.nlm.nih.gov";
    SConnNetInfo* net_info;
    CONNECTOR connector;
    EIO_Status status;
    STimeout* timeout;
    char ibuf[1024];
    CONN conn;
    size_t n;

    g_NCBI_ConnectRandomSeed = (int) time(0) ^ NCBI_CONNECT_SRAND_ADDENT;
    srand(g_NCBI_ConnectRandomSeed);

    CORE_SetLOGFormatFlags(fLOG_Full | fLOG_DateTime);
    CORE_SetLOGFILE(stderr, 0/*false*/);

    net_info = ConnNetInfo_Create(service);
    strcpy(net_info->host, host);
    if (argc > 3) {
        strncpy0(obuf, argv[3], sizeof(obuf) - 2);
        obuf[n = strlen(obuf)] = '\n';
        obuf[++n]              = 0;
    }
    strcpy(net_info->args, "testarg=testval&service=none");
    timeout = net_info->timeout;

    connector = SERVICE_CreateConnectorEx(service, fSERV_Any, net_info, 0);

    if (!connector)
        CORE_LOG(eLOG_Fatal, "Failed to create service connector");

    if (CONN_Create(connector, &conn) != eIO_Success)
        CORE_LOG(eLOG_Fatal, "Failed to create connection");

#if 0
    for (n = 0; n < 10; n++) {
        int m;
        for (m = 0; m < sizeof(obuf) - 2; m++)
            obuf[m] = "01234567890\n"[rand() % 12];
        obuf[m++] = '\n';
        obuf[m]   = '\0';

        if (CONN_Write(conn, obuf, strlen(obuf), &m, eIO_WritePersist)
            != eIO_Success) {
            if (!n) {
                CONN_Close(conn);
                CORE_LOG(eLOG_Fatal, "Error writing to connection");
            } else
                break;
        }
        assert(m == strlen(obuf));
    }
#else
    if (CONN_Write(conn, obuf, strlen(obuf), &n, eIO_WritePersist)
        != eIO_Success) {
        CONN_Close(conn);
        CORE_LOG(eLOG_Fatal, "Error writing to connection");
    }
    assert(n == strlen(obuf));
#endif

    for (;;) {
       if (CONN_Wait(conn, eIO_Read, timeout) != eIO_Success) {
            CONN_Close(conn);
            CORE_LOG(eLOG_Fatal, "Error waiting for reading");
        }

        status = CONN_Read(conn, ibuf, sizeof(ibuf), &n, eIO_ReadPersist);
        if (n) {
            char* descr = CONN_Description(conn);
            CORE_DATAF(ibuf, n, ("%lu bytes read from service (%s%s%s):",
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
        CORE_LOG(eLOG_Fatal, "Error writing to service ID1");
    }
    assert(n == 7);

    if (CONN_Read(conn, ibuf, sizeof(ibuf), &n, eIO_ReadPlain) != eIO_Success){
        CONN_Close(conn);
        CORE_LOG(eLOG_Fatal, "Error reading from service ID1");
    }

    
    ConnNetInfo_Destroy(net_info);
    CORE_LOGF(eLOG_Note, ("%d bytes read from service ID1", n));
    CONN_Close(conn);
#endif

    CORE_SetLOG(0);
    return 0/*okay*/;
}


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.32  2005/05/02 16:12:43  lavr
 * Use global random seed
 *
 * Revision 6.31  2005/01/28 17:44:48  lavr
 * Fix:  forgotten to merge status variable relocation
 *
 * Revision 6.30  2005/01/28 17:41:22  lavr
 * Allow CONN_TIMEOUT from the environment to be used in Wait-on-Read
 *
 * Revision 6.29  2004/02/23 15:23:43  lavr
 * New (last) parameter "how" added in CONN_Write() API call
 *
 * Revision 6.28  2003/10/21 11:37:47  lavr
 * Set write timeout from the environment/registry instead of explicitly
 *
 * Revision 6.27  2003/07/24 16:38:59  lavr
 * Add conditional check for early connection drops (#if 0'd)
 *
 * Revision 6.26  2003/05/29 18:03:49  lavr
 * Extend read error message with a reason (if any)
 *
 * Revision 6.25  2003/05/14 03:58:43  lavr
 * Match changes in respective APIs of the tests
 *
 * Revision 6.24  2003/05/05 20:31:23  lavr
 * Add date/time stamp to each log message printed
 *
 * Revision 6.23  2003/04/04 21:01:06  lavr
 * Modify readout procedure
 *
 * Revision 6.22  2002/10/28 15:47:12  lavr
 * Use "ncbi_ansi_ext.h" privately and use strncpy0()
 *
 * Revision 6.21  2002/09/24 15:10:09  lavr
 * Fix test not to dereference NULL pointer resulting from failed connection
 *
 * Revision 6.20  2002/08/07 16:38:08  lavr
 * EIO_ReadMethod enums changed accordingly; log moved to end
 *
 * Revision 6.19  2002/03/22 19:47:41  lavr
 * Test_assert.h made last among the include files
 *
 * Revision 6.18  2002/03/21 22:02:16  lavr
 * Change default server from "ray" into "www.ncbi.nlm.nih.gov"
 *
 * Revision 6.17  2002/02/05 21:45:55  lavr
 * Included header files rearranged
 *
 * Revision 6.16  2002/01/16 21:23:15  vakatov
 * Utilize header "test_assert.h" to switch on ASSERTs in the Release mode too
 *
 * Revision 6.15  2001/09/24 20:36:22  lavr
 * Adjusted parameters in SERVICE_CreateConnectorEx()
 *
 * Revision 6.14  2001/06/11 22:17:28  lavr
 * Wait-for-reading timeout made finite
 *
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
