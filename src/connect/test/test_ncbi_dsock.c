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
 *   Test suite for datagram socket API
 *
 */

#include "../ncbi_ansi_ext.h"
#include "../ncbi_priv.h"               /* CORE logging facilities */
#include <connect/ncbi_socket.h>
#include <errno.h>
#include <stdlib.h>
/* This header must go last */
#include "test_assert.h"

#define DEFAULT_PORT 55555


static int s_Usage(const char* prog)
{
    CORE_LOGF(eLOG_Error, ("Usage:\n%s {client|server} [port]", prog));
    return 1;
}


static int s_Server(int x_port)
{
    char           addr[32];
    char*          buf;
    SOCK           server;
    EIO_Status     status;
    STimeout       timeout;
    unsigned int   peeraddr;
    unsigned short peerport, port;
    size_t         msgsize, n, len;

    if (x_port <= 0) {
        CORE_LOG(eLOG_Error, "[Server]  Port wrongly specified");
        return 1;
    }

    port = (unsigned short) x_port;
    CORE_LOGF(eLOG_Note, ("[Server]  Opening DSOCK on port %hu", port));

    if ((status = DSOCK_Create(port, &server)) != eIO_Success) {
        CORE_LOGF(eLOG_Error, ("[Server]  Error creating server DSOCK: %s",
                               IO_StatusStr(status)));
        return 1;
    }

    if ((status = DSOCK_RecvMsg(server, 0, 0, 0, &msgsize,
                                &peeraddr, &peerport)) != eIO_Success) {
        CORE_LOGF(eLOG_Error, ("[Server]  Error reading from DSOCK: %s",
                               IO_StatusStr(status)));
        return 1;
    }

    if (SOCK_ntoa(peeraddr, addr, sizeof(addr)) != 0)
        strcpy(addr, "<unknown>");

    CORE_LOGF(eLOG_Note, ("[Server]  Message received from %s:%hu, %lu bytes",
                          addr, peerport, (unsigned long) msgsize));

    if (!(buf = malloc(msgsize ? msgsize : 1))) {
        CORE_LOG_ERRNO(eLOG_Error, errno, "[Server]  Cannot alloc msg buf");
        return 1;
    }

    len = 0;
    do {
        n = (size_t)(((double) rand()/(double) RAND_MAX)*(msgsize - len));
        if ((status = SOCK_Read(server, buf + len, n, &n, eIO_ReadPlain))
            != eIO_Success) {
            CORE_LOGF(eLOG_Error,("[Server]  Error reading msg @ byte %lu: %s",
                                  (unsigned long) len, IO_StatusStr(status)));
            return 1;
        }
        len += n;
    } while (len < msgsize);

    CORE_LOG(eLOG_Note, "[Server]  Bouncing the message to sender");

    timeout.sec  = 1;
    timeout.usec = 0;
    if ((status= SOCK_SetTimeout(server, eIO_Write, &timeout)) != eIO_Success){
        CORE_LOGF(eLOG_Error, ("[Server]  Error setting write tmo: %s",
                               IO_StatusStr(status)));
        return 1;
    }

    len = 0;
    do {
        n = (size_t)(((double) rand()/(double) RAND_MAX)*(msgsize - len));
        if ((status = SOCK_Write(server, buf + len, n, &n, eIO_WritePlain))
            != eIO_Success) {
            CORE_LOGF(eLOG_Error,("[Server]  Error writing msg @ byte %lu: %s",
                                  (unsigned long) len, IO_StatusStr(status)));
            return 1;
        }
        len += n;
    } while (len < msgsize);

    free(buf);

    if ((status = DSOCK_SendMsg(server, addr, peerport, "--Reply--", 9))
        != eIO_Success) {
        CORE_LOGF(eLOG_Error, ("[Server]  Error sending reply: %s",
                               IO_StatusStr(status)));
        return 1;
    }

    if ((status = SOCK_Close(server)) != eIO_Success) {
        CORE_LOGF(eLOG_Error, ("[Server]  Error closing socket: %s",
                               IO_StatusStr(status)));
        return 1;
    }

    return 0;
}


static int s_Client(int x_port)
{
    unsigned short port;
    size_t         msgsize, n;
    STimeout       timeout;
    EIO_Status     status;
    SOCK           client;
    char*          buf;

    if (x_port <= 0) {
        CORE_LOG(eLOG_Error, "[Client]  Port wrongly specified");
        return 1;
    }
 
    port = (unsigned short) x_port;
    CORE_LOGF(eLOG_Note, ("[Client]  Opening DSOCK on port %hu", port));

    if ((status = DSOCK_Create(0, &client)) != eIO_Success) {
        CORE_LOGF(eLOG_Error, ("[Client]  Error creating server DSOCK: %s",
                               IO_StatusStr(status)));
        return 1;
    }

    msgsize = (size_t)(((double) rand()/(double) RAND_MAX)*((1 << 16) - 10));
    CORE_LOGF(eLOG_Note, ("[Client]  Generating a message %lu bytes long",
                          (unsigned long) msgsize));

    if (!(buf = malloc(2*msgsize + 9))) {
        CORE_LOG_ERRNO(eLOG_Error, errno, "[Client]  Cannot alloc msg buf");
        return 1;        
    }

    for (n = 0; n < msgsize; n++)
        buf[n] = rand() % 0xFF;

    if ((status = DSOCK_SendMsg(client, "127.0.0.1", port, buf, msgsize))
        != eIO_Success) {
        CORE_LOGF(eLOG_Error, ("[Client]  Error sending message: %s",
                               IO_StatusStr(status)));
        return 1;
    }

    timeout.sec  = 1;
    timeout.usec = 0;
    if ((status = SOCK_SetTimeout(client, eIO_Read, &timeout)) != eIO_Success){
        CORE_LOGF(eLOG_Error, ("[Client]  Error setting read tmo: %s",
                               IO_StatusStr(status)));
        return 1;
    }

    if ((status = DSOCK_RecvMsg(client, 0, &buf[msgsize],
                                msgsize + 9, &n, 0, 0))
        != eIO_Success) {
        CORE_LOGF(eLOG_Error, ("[Client]  Error receiving message: %s",
                               IO_StatusStr(status)));
        return 1;
    }

    if (n != msgsize + 9) {
        CORE_LOGF(eLOG_Error, ("[Client]  Received wrong size message: %lu",
                               (unsigned long) n));
        return 1;
    } else {
        CORE_LOGF(eLOG_Note, ("[Client]  Received message back, %lu bytes",
                              (unsigned long) n));
    }

    for (n = 0; n < msgsize; n++) {
        if (buf[n] != buf[msgsize + n])
            break;
    }

    if (n < msgsize) {
        CORE_LOGF(eLOG_Error, ("[Client]  Bounced message corrupted, off %lu",
                               (unsigned long) n));
        return 1;
    }

    if (strncmp(&buf[msgsize*2], "--Reply--", 9) != 0) {
        CORE_LOGF(eLOG_Error, ("[Client]  No signature in the message: %.9s",
                               &buf[msgsize*2]));
        return 1;
    }

    free(buf);

    if ((status = SOCK_Close(client)) != eIO_Success) {
        CORE_LOGF(eLOG_Error, ("[Client]  Error closing socket: %s",
                               IO_StatusStr(status)));
        return 1;
    }

    return 0;
}


int main(int argc, const char* argv[])
{
    const char* env = getenv("CONN_DEBUG_PRINTOUT");

    CORE_SetLOGFILE(stderr, 0/*false*/);

    if (argc < 2 || argc > 3)
        return s_Usage(argv[0]);

    /* srand(time() + getpid()); */

    if (env && (strcasecmp(env, "1") == 0    || strcasecmp(env, "yes") == 0 ||
                strcasecmp(env, "some") == 0 || strcasecmp(env, "data") == 0)){
        SOCK_SetDataLoggingAPI(eOn);
    }

    if (strcasecmp(argv[1], "client") == 0)
        return s_Client(argv[2] ? atoi(argv[2]) : DEFAULT_PORT);

    if (strcasecmp(argv[1], "server") == 0)
        return s_Server(argv[2] ? atoi(argv[2]) : DEFAULT_PORT);

    return s_Usage(argv[0]);
}


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.1  2003/01/16 16:33:06  lavr
 * Initial revision
 *
 * ==========================================================================
 */
