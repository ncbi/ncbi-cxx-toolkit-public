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
 *   A simple test program to listen on a socket and just bounce all
 *   incoming data (e.g., can be used to test "ncbi_socket_connector.[ch]")
 *
 */

#include <connect/ncbi_connutil.h>
#include <connect/ncbi_util.h>
#include "../ncbi_assert.h"
/* This header must go last */
#include "test_assert.h"


static FILE* s_LogFile;


/* Accept connections and bounce the incoming data back to the client
 * (only one client at a time).  Perform up to "n_cycle" sessions, then exit.
 */
static void s_DoServer(const char* sport, int n_cycle)
{
    int            i;
    unsigned short nport;
    LSOCK          lsock;
    EIO_Status     status;

    /* Create listening socket */
    if (sscanf(sport, "%hu%n", &nport, &i) < 1  ||  sport[i]) {
        nport = 0;
        i = 0;
    }
    status = LSOCK_Create(nport, 1, &lsock);
    if (status == eIO_Success  &&  !nport  &&  sport[i]) {
        FILE* fp;
        nport = LSOCK_GetPort(lsock, eNH_HostByteOrder);
        if (nport  &&  (fp = fopen(sport, "w")) != 0) {
            if (fprintf(fp, "%hu", nport) < 1  ||  fflush(fp) != 0)
                status = eIO_Unknown;
            fclose(fp);
        } else
            status = eIO_Unknown;
    }

    fprintf(s_LogFile, "DoServer(port = %hu, n_cycle = %u)\n", nport, n_cycle);
    fflush(s_LogFile);
    assert(status == eIO_Success);

    /* Accept connections from clients and run test sessions */
    while ( n_cycle-- ) {
        SOCK     sock;
        size_t   n_io, n_io_done;
        char     buf[1024];
        STimeout timeout;

        /* Accept connection */
        status = LSOCK_Accept(lsock, 0, &sock);
        assert(status == eIO_Success);

        /* Set some weird I/O timeouts for the accepted connection */
        timeout.sec  = 100;
        timeout.usec = 888;
        status = SOCK_SetTimeout(sock, eIO_Read,  &timeout);
        assert(status == eIO_Success);
        status = SOCK_SetTimeout(sock, eIO_Write, &timeout);
        assert(status == eIO_Success);

        /* Bounce all incoming data back to the client */
        while ((status = SOCK_Read(sock,buf,sizeof(buf),&n_io,eIO_ReadPlain))
               == eIO_Success) {
            status = SOCK_Write(sock, buf, n_io, &n_io_done, eIO_WritePersist);
            if (status != eIO_Success) {
                fprintf(s_LogFile,
                        "Failed to write -- DoServer(n_cycle = %d): %s\n",
                        n_cycle, IO_StatusStr(status));
                fflush(s_LogFile);
                break;
            }
        }
        assert(status == eIO_Closed);

        /* Close connection */
        status = SOCK_Close(sock);
        assert(status == eIO_Success  ||  status == eIO_Closed);
    }

    /* Close listening socket */
    status = LSOCK_Close(lsock);
    assert(status == eIO_Success);
}


int main(int argc, const char* argv[])
{
    SConnNetInfo* net_info;
    int n_cycle = 100;

    /* logging */
    s_LogFile = fopen("socket_io_bouncer.log", "ab");
    assert(s_LogFile);
    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);
    CORE_SetLOGFILE(s_LogFile, 1/*auto-close*/);

    assert((net_info = ConnNetInfo_Create(0)) != 0);

    /* cmd.-line args */
    if (argc > 3  ||  !argv[1]  ||  !*argv[1])
        n_cycle = -1;
    else if (argc == 3  &&  sscanf(argv[2], "%d", &n_cycle) != 1)
        n_cycle = -1;

    if (n_cycle <= 0) {
        fprintf(stderr,    "Usage: %s <port> [n_cycle]\n\n", argv[0]);
        fprintf(s_LogFile, "Usage: %s <port> [n_cycle]\n\n", argv[0]);
        return 1/*error*/;
    }

    if (net_info->debug_printout)
        SOCK_SetDataLoggingAPI(eOn);

    /* run */
    s_DoServer(argv[1], n_cycle);

    /* cleanup */
    verify(SOCK_ShutdownAPI() == eIO_Success);
    ConnNetInfo_Destroy(net_info);
    CORE_SetLOG(0);
    return 0;
}
