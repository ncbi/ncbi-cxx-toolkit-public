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
 *   Test suite for datagram socket API
 *
 */

#include <connect/ncbi_connutil.h>
#include <connect/ncbi_socket.h>
#include "../ncbi_ansi_ext.h"
#include "../ncbi_priv.h"               /* CORE logging facilities */
#include <errno.h>
#include <stdlib.h>
#include <time.h>

#include "test_assert.h"  /* This header must go last */

/* maximal UDP packet size as allowed by the standard (minus UDP header size)*/
#define MAX_UDP_DGRAM_SIZE  (65535 - 8)

#if defined(NCBI_OS_BSD) || defined(NCBI_OS_OSF1) || defined(NCBI_OS_DARWIN)
   /* FreeBSD has this limit :-/ Source: `sysctl net.inet.udp.maxdgram` */
   /* For OSF1 (and FreeBSD) see also: /usr/include/netinet/udp_var.h   */
#  define MAX_DGRAM_SIZE    (9*1024)
#elif defined(NCBI_OS_IRIX)
   /* This has been found experimentally on IRIX64 6.5 04101931 IP25 */
#  define MAX_DGRAM_SIZE    (60*1024)
#elif defined(NCBI_OS_LINUX)
   /* Larger sizes did not seem to work everywhere; although recent tests show
    * that 65507 is okay now (cf. SOLARIS/MSWIN right below) */
#  define MAX_DGRAM_SIZE    59550
#elif defined(NCBI_OS_SOLARIS)  ||  defined(NCBI_OS_MSWIN)
   /* 65508 was reported too large */
#  define MAX_DGRAM_SIZE    65507
#else
#  define MAX_DGRAM_SIZE    MAX_UDP_DGRAM_SIZE
#endif
/* NOTE: x86_64 (AMD) Linux kernel does not allow dgrams bigger than MTU minus
 * some small overhead;  for these we use the script and pass the MTU via argv.
 */

#define DEFAULT_PORT        55555


static int s_MTU = MAX_DGRAM_SIZE;


static int s_Usage(const char* prog)
{
    CORE_LOGF(eLOG_Error, ("Usage:\n%s {client|server} [port [mtu [seed]]]",
                           prog));
    return 1;
}


static int s_Server(const char* sport)
{
    int            i;
    char*          buf;
    unsigned int   host;
    unsigned short port;
    unsigned short myport;
    char           addr[32];
    SOCK           server;
    EIO_Status     status;
    STimeout       timeout;
    char           minibuf[255];
    size_t         msglen, n, len;

    if ((status = DSOCK_Create(&server)) != eIO_Success) {
        CORE_LOGF(eLOG_Error, ("[Server]  Cannot create DSOCK: %s",
                               IO_StatusStr(status)));
        return 1;
    }
    if (sscanf(sport, "%hu%n", &port, &i) < 1  ||  sport[i]) {
        port = 0;
        i = 0;
    }
    if ((status = DSOCK_Bind(server, port)) != eIO_Success) {
        CORE_LOGF(eLOG_Error, ("[Server]  Cannot bind DSOCK to port %hu: %s",
                               port, IO_StatusStr(status)));
        return 1;
    }
    errno = 0;
    myport = port ? port : SOCK_GetLocalPort(server, eNH_HostByteOrder);
    if (!port  &&  sport[i]) {
        FILE* fp;
        if (myport  &&  (fp = fopen(sport, "w")) != 0) {
            if (fprintf(fp, "%hu", myport) < 1  ||  fflush(fp) != 0)
                status = eIO_Unknown;
            fclose(fp);
        } else
            status = eIO_Unknown;
    }
    
    CORE_LOGF_ERRNO(eLOG_Note, errno, ("[Server]  DSOCK on port %hu: %s",
                                       myport, IO_StatusStr(status)));
    assert(status == eIO_Success);

    for (;;) {
        if ((status = DSOCK_WaitMsg(server, 0/*infinite*/)) != eIO_Success) {
            CORE_LOGF(eLOG_Error, ("[Server]  Failed waiting for DSOCK: %s",
                                   IO_StatusStr(status)));
            break;
        }

        timeout.sec  = 0;
        timeout.usec = 0;
        if ((status = SOCK_SetTimeout(server, eIO_Read, &timeout))
            != eIO_Success) {
            CORE_LOGF(eLOG_Error,("[Server]  Cannot set zero read timeout: %s",
                                   IO_StatusStr(status)));
            break;
        }

        len = (size_t)(((double) rand() / (double) RAND_MAX)
                       * (double) sizeof(minibuf));
        if ((status = DSOCK_RecvMsg(server, minibuf, len, 0, &msglen,
                                    &host, &port)) != eIO_Success) {
            CORE_LOGF(eLOG_Error, ("[Server]  Cannot read from DSOCK: %s",
                                   IO_StatusStr(status)));
            continue;
        }
        if (len > msglen)
            len = msglen;

        if (SOCK_ntoa(host, addr, sizeof(addr)) != 0)
            strcpy(addr, "<unknown>");

        CORE_LOGF(eLOG_Note, ("[Server]  Message received from %s:%hu, "
                              "%lu bytes",
                              addr, port, (unsigned long) msglen));

        if (!(buf = (char*) malloc(msglen < 10 ? 10 : msglen))) {
            CORE_LOG_ERRNO(eLOG_Error, errno,
                           "[Server]  Cannot allocate message buffer");
            break;
        }
        if (len)
            memcpy(buf, minibuf, len);

        while (len < msglen) {
            n = (size_t)(((double) rand() / (double) RAND_MAX)
                         * (double)(msglen - len) + 0.5);
            if ((status = SOCK_Read(server, buf + len, n, &n, eIO_ReadPlain))
                != eIO_Success) {
                CORE_LOGF(eLOG_Error,("[Server]  Cannot read msg @ byte %lu:"
                                      " %s", (unsigned long) len,
                                      IO_StatusStr(status)));
                if (status == eIO_Closed)
                    break;
                continue;
            }
            len += n;
        }
        assert(SOCK_Read(server, 0, 1, &n, eIO_ReadPlain) == eIO_Closed);
        assert(memcmp(buf + msglen - 10, "\0\0\0\0\0\0\0\0\0", 10) == 0);

        CORE_LOG(eLOG_Note, "[Server]  Bouncing the message to sender");

        timeout.sec  = 1;
        timeout.usec = 0;
        if ((status = SOCK_SetTimeout(server, eIO_Write, &timeout))
            != eIO_Success){
            CORE_LOGF(eLOG_Error, ("[Server]  Cannot set write timeout: %s",
                                   IO_StatusStr(status)));
            free(buf);
            break;
        }

        msglen -= 10;
        for (len = 0;  len < msglen;  len += n) {
            n = (size_t)(((double) rand() / (double) RAND_MAX)
                         * (double)(msglen - len) + 0.5);
            if ((status = SOCK_Write(server, buf + len, n, &n, eIO_WritePlain))
                != eIO_Success) {
                CORE_LOGF(eLOG_Error,("[Server]  Cannot write msg @ byte %lu:"
                                      " %s", (unsigned long) len,
                                      IO_StatusStr(status)));
                break;
            }
        }

        free(buf);

        if ((status = DSOCK_SendMsg(server, addr, port, "--Reply--", 10))
            != eIO_Success) {
            CORE_LOGF(eLOG_Error, ("[Server]  Cannot send to DSOCK: %s",
                                   IO_StatusStr(status)));
            /*continue*/;
        }
    }

    /* On errors control reaches here */
    if ((status = SOCK_Close(server)) != eIO_Success) {
        CORE_LOGF(eLOG_Error, ("[Server]  Cannot close DSOCK: %s",
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
        CORE_LOGF(eLOG_Error, ("[Client]  Port malformed (%d)", x_port));
        return 1;
    }
 
    if ((status = DSOCK_Create(&client)) != eIO_Success) {
        CORE_LOGF(eLOG_Error, ("[Client]  Cannot create DSOCK: %s",
                               IO_StatusStr(status)));
        return 1;
    }
    port = (unsigned short) x_port;

    CORE_LOGF(eLOG_Note, ("[Client]  DSOCK on port %hu", port));

    msglen = s_MTU <= 0
        ? (size_t)(-s_MTU)
        : (size_t)(((double) rand() / (double) RAND_MAX) * (double) s_MTU);
    if (msglen < sizeof(id) + 10)
        msglen = sizeof(id) + 10;
    if (msglen == MAX_UDP_DGRAM_SIZE  &&  s_MTU > 0)
        msglen--;

    CORE_LOGF(eLOG_Note, ("[Client]  Generating a message %lu bytes long",
                          (unsigned long) msglen));

    if (!(buf = (char*) malloc(2 * msglen))) {
        CORE_LOG_ERRNO(eLOG_Error, errno,
                       "[Client]  Cannot allocate message buffer");
        SOCK_Close(client);
        return 1;
    }

    for (n = sizeof(id);  n < msglen - 10;  ++n)
        buf[n] = (char)(rand() % 0xFF);
    memcpy(buf + msglen - 10, "\0\0\0\0\0\0\0\0\0", 10);

    id = (unsigned long) time(0);

    for (m = 1;  m <= max_try;  ++m) {
        unsigned long tmp;
        unsigned int  k;

        if (m != 1)
            CORE_LOGF(eLOG_Note, ("[Client]  Attempt #%u", (unsigned int) m));
        ++id;

        memcpy(buf, &id, sizeof(id));

        if ((status = DSOCK_SendMsg(client, "127.0.0.1", port, buf, msglen))
            != eIO_Success) {
            CORE_LOGF(eLOG_Error, ("[Client]  Cannot send to DSOCK: %s",
                                   IO_StatusStr(status)));
            goto errout;
        }

        timeout.sec  = 1;
        timeout.usec = 0;
        if ((status = SOCK_SetTimeout(client, eIO_Read, &timeout))
            != eIO_Success) {
            CORE_LOGF(eLOG_Error, ("[Client]  Cannot set read timeout: %s",
                                   IO_StatusStr(status)));
            goto errout;
        }

        k = 0;
    again:
        if ((status = DSOCK_RecvMsg(client, buf + msglen, msglen, 0, &n, 0, 0))
            != eIO_Success) {
            CORE_LOGF(eLOG_Error, ("[Client]  Cannot read from DSOCK: %s",
                                   IO_StatusStr(status)));
            continue;
        }

        if (n != msglen) {
            CORE_LOGF(eLOG_Error, ("[Client]  Received message of wrong size: "
                                   "%lu", (unsigned long) n));
            goto errout;
        }

        memcpy(&tmp, buf + msglen, sizeof(tmp));
        if (tmp != id) {
            ++k;
            CORE_LOGF(k < max_try ? eLOG_Warning : eLOG_Error,
                      ("[Client]  Stale message received%s",
                       k < max_try ? ", re-attempting to fetch" : ""));
            if (k < max_try)
                goto again;
            break;
        }

        CORE_LOGF(eLOG_Note, ("[Client]  Received the message back, %lu bytes",
                              (unsigned long) n));
        assert(SOCK_Read(client, 0, 1, &n, eIO_ReadPlain) == eIO_Closed);
        break;
    }
    if (m > max_try)
        goto errout;

    for (n = sizeof(unsigned long);  n < msglen - 10;  ++n) {
        if (buf[n] != buf[msglen + n])
            break;
    }
    if (n < msglen - 10) {
        CORE_LOGF(eLOG_Error, ("[Client]  Bounced message corrupt, offset=%lu",
                               (unsigned long) n));
        goto errout;
    }

    if (memcmp(buf + msglen*2 - 10, "--Reply--", 10) != 0) {
        CORE_LOGF(eLOG_Error, ("[Client]  No signature in the message: %.9s",
                               buf + msglen*2 - 10));
        goto errout;
    }

    free(buf);

    if ((status = SOCK_Close(client)) != eIO_Success) {
        CORE_LOGF(eLOG_Error, ("[Client]   Cannot close DSOCK: %s",
                               IO_StatusStr(status)));
        return 1;
    }

    CORE_LOG(eLOG_Note, "TEST completed successfully");
    CORE_SetLOG(0);
    return 0;

errout:
    SOCK_Close(client);
    free(buf);
    return 1;
}


#define _STR(x)     #x
#define  STR(x) _STR(x)


int main(int argc, const char* argv[])
{
    unsigned short max_try;
    SConnNetInfo* net_info;

    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Short   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);
    CORE_SetLOGFILE_Ex(stderr, eLOG_Trace, eLOG_Fatal, 0/*no auto-close*/);

    if (argc < 2  ||  argc > 5)
        return s_Usage(argv[0]);

    if (argc <= 4) {
        g_NCBI_ConnectRandomSeed
            = (unsigned int) time(0) ^ NCBI_CONNECT_SRAND_ADDEND;
    } else
        g_NCBI_ConnectRandomSeed = (unsigned int) atoi(argv[4]);
    CORE_LOGF(eLOG_Note, ("Random SEED = %u", g_NCBI_ConnectRandomSeed));
    srand(g_NCBI_ConnectRandomSeed);

    assert((net_info = ConnNetInfo_Create(0)) != 0);
    if (net_info->debug_printout)
        SOCK_SetDataLoggingAPI(eOn);
    max_try = net_info->max_try;
    ConnNetInfo_Destroy(net_info);

    if (argc > 3) {
        int mtu = atoi(argv[3]);
        s_MTU = mtu > 28
            ? mtu - 28/*IP(20)/UDP(8) overhead*/
            : mtu;
        CORE_LOGF(eLOG_Note, ("MTU = %d/%d/%d",
                              s_MTU, MAX_DGRAM_SIZE, MAX_UDP_DGRAM_SIZE));
    }

    if (strcasecmp(argv[1], "client") == 0) {
        return s_Client(argv[2] ? atoi(argv[2]) : DEFAULT_PORT, max_try);
    }
    if (strcasecmp(argv[1], "server") == 0)
        return s_Server(argv[2] ? argv[2] : STR(DEFAULT_PORT));

    return s_Usage(argv[0]);
}
