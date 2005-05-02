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
#include <connect/ncbi_connutil.h>
#include <connect/ncbi_socket.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
/* This header must go last */
#include "test_assert.h"

#if defined(NCBI_OS_BSD) || defined(NCBI_OS_OSF1) || defined(NCBI_OS_DARWIN)
   /* FreeBSD has this limit :-/ Source: `sysctl net.inet.udp.maxdgram` */
   /* For OSF1 (and FreeBSD) see also: /usr/include/netinet/udp_var.h   */
#  define MAX_DGRAM_SIZE (9*1024)
#elif defined(NCBI_OS_IRIX)
   /* This has been found experimentally on IRIX64 6.5 04101931 IP25 */
#  define MAX_DGRAM_SIZE (60*1024)
#elif defined(NCBI_OS_LINUX)
/* Larger sizes do not seem to work everywhere */
#  define MAX_DGRAM_SIZE 65000
#else
   /* This is the maximal datagram size defined by the UDP standard */
#  define MAX_DGRAM_SIZE 65535
#endif
/* NOTE: x86_64 (AMD) kernel does not allow dgrams bigger than MTU minus
 * small overhead;  for these we use the script and pass the MTU via argv.
 */

#define DEFAULT_PORT 55555


static unsigned short s_MTU = MAX_DGRAM_SIZE;


static int s_Usage(const char* prog)
{
    CORE_LOGF(eLOG_Error, ("Usage:\n%s {client|server} [port [mtu [seed]]]",
                           prog));
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
    size_t         msglen, n, len;
    char           minibuf[255];

    if (x_port <= 0) {
        CORE_LOG(eLOG_Error, "[Server]  Port wrongly specified");
        return 1;
    }

    port = (unsigned short) x_port;
    CORE_LOGF(eLOG_Note, ("[Server]  Opening DSOCK on port %hu", port));

    if ((status = DSOCK_Create(&server)) != eIO_Success) {
        CORE_LOGF(eLOG_Error, ("[Server]  Error creating DSOCK: %s",
                               IO_StatusStr(status)));
        return 1;
    }

    if ((status = DSOCK_Bind(server, port)) != eIO_Success) {
        CORE_LOGF(eLOG_Error, ("[Server]  Error binding DSOCK to port %hu: %s",
                               port, IO_StatusStr(status)));
        return 1;
    }

    for (;;) {
        if ((status = DSOCK_WaitMsg(server, 0/*infinite*/)) != eIO_Success) {
            CORE_LOGF(eLOG_Error, ("[Server]  Error waiting on DSOCK: %s",
                                   IO_StatusStr(status)));
            break;
        }

        timeout.sec  = 0;
        timeout.usec = 0;
        if ((status = SOCK_SetTimeout(server, eIO_Read, &timeout))
            != eIO_Success) {
            CORE_LOGF(eLOG_Error, ("[Server]  Error setting zero read tmo: %s",
                                   IO_StatusStr(status)));
            break;
        }

        len = (size_t)(((double) rand()/(double) RAND_MAX)*sizeof(minibuf));
        if ((status = DSOCK_RecvMsg(server, minibuf, len, 0, &msglen,
                                    &peeraddr, &peerport)) != eIO_Success) {
            CORE_LOGF(eLOG_Error, ("[Server]  Error reading from DSOCK: %s",
                                   IO_StatusStr(status)));
            continue;
        }
        if (len > msglen)
            len = msglen;

        if (SOCK_ntoa(peeraddr, addr, sizeof(addr)) != 0)
            strcpy(addr, "<unknown>");

        CORE_LOGF(eLOG_Note, ("[Server]  Message received from %s:%hu, "
                              "%lu bytes",
                              addr, peerport, (unsigned long) msglen));

        if (!(buf = (char*) malloc(msglen ? msglen : 1))) {
            CORE_LOG_ERRNO(eLOG_Error, errno,"[Server]  Cannot alloc msg buf");
            break;
        }
        if (len)
            memcpy(buf, minibuf, len);

        while (len < msglen) {
            n = (size_t)(((double)rand()/(double)RAND_MAX)*(msglen-len) + 0.5);
            if ((status = SOCK_Read(server, buf + len, n, &n, eIO_ReadPlain))
                != eIO_Success) {
                CORE_LOGF(eLOG_Error,("[Server]  Error reading msg @ byte %lu:"
                                      " %s", (unsigned long) len,
                                      IO_StatusStr(status)));
                free(buf);
                continue;
            }
            len += n;
        }
        assert(SOCK_Read(server, 0, 1, &n, eIO_ReadPlain) == eIO_Closed);

        CORE_LOG(eLOG_Note, "[Server]  Bouncing the message to sender");

        timeout.sec  = 1;
        timeout.usec = 0;
        if ((status = SOCK_SetTimeout(server, eIO_Write, &timeout))
            != eIO_Success){
            CORE_LOGF(eLOG_Error, ("[Server]  Error setting write tmo: %s",
                                   IO_StatusStr(status)));
            break;
        }

        for (len = 0; len < msglen; len += n) {
            n = (size_t)(((double)rand()/(double)RAND_MAX)*(msglen-len) + 0.5);
            if ((status = SOCK_Write(server, buf + len, n, &n, eIO_WritePlain))
                != eIO_Success) {
                CORE_LOGF(eLOG_Error,("[Server]  Error writing msg @ byte %lu:"
                                      " %s", (unsigned long) len,
                                      IO_StatusStr(status)));
                break;
            }
        }

        free(buf);

        if ((status = DSOCK_SendMsg(server, addr, peerport, "--Reply--", 9))
            != eIO_Success) {
            CORE_LOGF(eLOG_Error, ("[Server]  Error sending to DSOCK: %s",
                                   IO_StatusStr(status)));
            /*continue*/;
        }
    }

    /* On errors control reaches here */
    if ((status = SOCK_Close(server)) != eIO_Success) {
        CORE_LOGF(eLOG_Error, ("[Server]  Error closing DSOCK: %s",
                               IO_StatusStr(status)));
    }
    return 1;
}


static int s_Client(int x_port, unsigned int max_try)
{
    size_t         msglen, n;
    STimeout       timeout;
    EIO_Status     status;
    SOCK           client;
    unsigned short port;
    char*          buf;
    unsigned long  id;
    unsigned int   m;

    if (x_port <= 0) {
        CORE_LOG(eLOG_Error, "[Client]  Port wrongly specified");
        return 1;
    }
 
    port = (unsigned short) x_port;
    CORE_LOGF(eLOG_Note, ("[Client]  Opening DSOCK on port %hu", port));

    if ((status = DSOCK_Create(&client)) != eIO_Success) {
        CORE_LOGF(eLOG_Error, ("[Client]  Error creating DSOCK: %s",
                               IO_StatusStr(status)));
        return 1;
    }

    msglen = (size_t)(((double)rand()/(double)RAND_MAX)*(s_MTU - 10));
    if (msglen < sizeof(time_t))
        msglen = sizeof(time_t);

    CORE_LOGF(eLOG_Note, ("[Client]  Generating a message %lu bytes long",
                          (unsigned long) msglen));

    if (!(buf = (char*) malloc(2*msglen + 9))) {
        CORE_LOG_ERRNO(eLOG_Error, errno, "[Client]  Cannot alloc msg buf");
        return 1;
    }


    for (n = sizeof(unsigned long); n < msglen; n++)
        buf[n] = rand() % 0xFF;

    id = (unsigned long) time(0);

    for (m = 1; m <= max_try; m++) {
        unsigned long tmp;

        if (m != 1)
            CORE_LOGF(eLOG_Note, ("[Client]  Attempt #%u", (unsigned int) m));
        id++;

        *((unsigned long*) buf) = SOCK_htonl((unsigned long) id);

        if ((status = DSOCK_SendMsg(client, "127.0.0.1", port, buf, msglen))
            != eIO_Success) {
            CORE_LOGF(eLOG_Error, ("[Client]  Error sending to DSOCK: %s",
                                   IO_StatusStr(status)));
            return 1;
        }

        timeout.sec  = 1;
        timeout.usec = 0;
        if ((status = SOCK_SetTimeout(client, eIO_Read, &timeout))
            != eIO_Success) {
            CORE_LOGF(eLOG_Error, ("[Client]  Error setting read timeout: %s",
                                   IO_StatusStr(status)));
            return 1;
        }

    again:
        if ((status = DSOCK_RecvMsg(client, &buf[msglen], msglen+9,0, &n, 0,0))
            != eIO_Success) {
            CORE_LOGF(eLOG_Error, ("[Client]  Error reading from DSOCK: %s",
                                   IO_StatusStr(status)));
            continue;
        }

        if (n != msglen + 9) {
            CORE_LOGF(eLOG_Error, ("[Client]  Received message of wrong size: "
                                   "%lu", (unsigned long) n));
            return 1;
        }

        memcpy(&tmp, &buf[msglen], sizeof(tmp));
        if (SOCK_ntohl(tmp) != id) {
            m++;
            CORE_LOGF(m < max_try ? eLOG_Warning : eLOG_Error,
                      ("[Client]  Stale message received%s",
                       m <= max_try ? ", reattempting to fetch" : ""));
            if (m <= max_try)
                goto again;
            break;
        }

        CORE_LOGF(eLOG_Note, ("[Client]  Received the message back, %lu bytes",
                              (unsigned long) n));
        assert(SOCK_Read(client, 0, 1, &n, eIO_ReadPlain) == eIO_Closed);
        break;
    }
    if (m > max_try)
        return 1;

    for (n = sizeof(unsigned long); n < msglen; n++) {
        if (buf[n] != buf[msglen + n])
            break;
    }

    if (n < msglen) {
        CORE_LOGF(eLOG_Error, ("[Client]  Bounced message corrupted, off=%lu",
                               (unsigned long) n));
        return 1;
    }

    if (strncmp(&buf[msglen*2], "--Reply--", 9) != 0) {
        CORE_LOGF(eLOG_Error, ("[Client]  No signature in the message: %.9s",
                               &buf[msglen*2]));
        return 1;
    }

    free(buf);

    if ((status = SOCK_Close(client)) != eIO_Success) {
        CORE_LOGF(eLOG_Error, ("[Client]  Error closing DSOCK: %s",
                               IO_StatusStr(status)));
        return 1;
    }

    CORE_LOG(eLOG_Note, "[Client]  Completed successfully");
    return 0;
}


int main(int argc, const char* argv[])
{
    unsigned long seed;
    unsigned int  max_try = DEF_CONN_MAX_TRY;
    const char*   env = getenv("CONN_DEBUG_PRINTOUT");

    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);
    CORE_SetLOGFILE(stderr, 0/*false*/);

    if (argc < 2 || argc > 5)
        return s_Usage(argv[0]);

    if (argc <= 4)
        seed = (int) time(0) ^ NCBI_CONNECT_SRAND_ADDENT;
    else
        sscanf(argv[4], "%lu", &seed);
    CORE_LOGF(eLOG_Note, ("Random SEED = %lu", seed));
    g_NCBI_ConnectRandomSeed = seed;
    srand(g_NCBI_ConnectRandomSeed);

    if (env && (strcasecmp(env, "1") == 0     ||
                strcasecmp(env, "yes") == 0   ||
                strcasecmp(env, "true") == 0  ||
                strcasecmp(env, "some") == 0  ||
                strcasecmp(env, "data") == 0)) {
        SOCK_SetDataLoggingAPI(eOn);
    }

    if (argc > 3) {
        int mtu = atoi(argv[3]);
        if (mtu > 32  &&  mtu < (int) s_MTU)
            s_MTU = mtu - 32/*small protocol (IP/UDP) overhead*/;
    }

    if (!(env = getenv("CONN_MAX_TRY"))  ||  !(max_try = atoi(env)))
        max_try = DEF_CONN_MAX_TRY;

    if (strcasecmp(argv[1], "client") == 0)
        return s_Client(argv[2] ? atoi(argv[2]) : DEFAULT_PORT, max_try);

    if (strcasecmp(argv[1], "server") == 0)
        return s_Server(argv[2] ? atoi(argv[2]) : DEFAULT_PORT);

    return s_Usage(argv[0]);
}


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.16  2005/05/02 16:12:27  lavr
 * Use global random seed
 *
 * Revision 6.15  2005/01/05 21:33:58  lavr
 * Introduce MTU and use it on AMD-64 buggy kernels
 *
 * Revision 6.14  2003/12/11 15:34:29  lavr
 * Lower maximal datagram size for Linux - 65535 didn't seem to work everywhere
 *
 * Revision 6.13  2003/12/10 17:24:16  lavr
 * Reattempt to send/receive a datagram on I/O errors in client
 *
 * Revision 6.12  2003/10/27 19:00:32  lavr
 * Limit datagram size for Darwin (which is BSD based)
 *
 * Revision 6.11  2003/05/29 18:03:06  lavr
 * Changed one client's message
 *
 * Revision 6.10  2003/05/14 03:58:43  lavr
 * Match changes in respective APIs of the tests
 *
 * Revision 6.9  2003/04/30 17:04:01  lavr
 * Conformance to slightly modified datagram socket API
 *
 * Revision 6.8  2003/03/25 15:07:21  lavr
 * Protect macros' values by enclosing in parentheses
 *
 * Revision 6.7  2003/03/25 15:04:13  lavr
 * Add IRIX-specific datagram size limit (60K)
 *
 * Revision 6.6  2003/02/28 14:46:36  lavr
 * Explicit casts for malloc()'ed memory
 *
 * Revision 6.5  2003/02/14 15:41:26  lavr
 * Limit packet size on OSF1, too
 *
 * Revision 6.4  2003/02/06 04:34:28  lavr
 * Do not exceed maximal dgram size on BSD; trickier readout in s_Server()
 *
 * Revision 6.3  2003/02/04 22:04:47  lavr
 * Protection from truncation to 0 in double->int conversion
 *
 * Revision 6.2  2003/01/17 01:26:44  lavr
 * Test improved/extended
 *
 * Revision 6.1  2003/01/16 16:33:06  lavr
 * Initial revision
 *
 * ==========================================================================
 */
