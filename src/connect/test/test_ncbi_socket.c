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
 *   Test suite for "ncbi_socket.[ch]", the portable TCP/IP socket API
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.3  1999/11/26 19:05:21  vakatov
 * Initialize "log_fp" in "main()"...
 *
 * Revision 6.2  1999/10/19 16:16:03  vakatov
 * Try the NCBI C and C++ headers only if NCBI_OS_{UNIX, MSWIN, MAC} is
 * not #define'd
 *
 * Revision 6.1  1999/10/18 15:40:21  vakatov
 * Initial revision
 * ===========================================================================
 */

#include "ncbi_socket.h"

#if defined(NDEBUG)
#  undef NDEBUG
#endif 
#include <assert.h>

#include <stdlib.h>
#include <string.h>


/* Kludge to compile with the NCBI C or C++ toolkits -- when the platform
 * (NCBI_OS_{UNIX, MSWIN, MAC}) is not specified in the command-line
 */
#if !defined(NCBI_OS_UNIX) && !defined(NCBI_OS_MSWIN) && !defined(NCBI_OS_MAC)
#  if defined(NCBI_C)
#    include <ncbilcl.h>
#    if defined(OS_UNIX)
#      define NCBI_OS_UNIX 1
#      if !defined(HAVE_GETHOSTBYNAME_R)  &&  defined(OS_UNIX_SOL)
#        define HAVE_GETHOSTBYNAME_R 1
#      endif
#    elif defined(OS_MSWIN)
#      define NCBI_OS_MSWIN 1
#    elif defined(OS_MAC)
#      define NCBI_OS_MAC 1
#    else
#      error "Unknown OS, must be one of OS_UNIX, OS_MSWIN, OS_MAC!"
#    endif
#  else /* else!NCBI_C */
#    include <ncbiconf.h>
#  endif /* NCBI_C */
#endif /* !NCBI_OS_{UNIX, MSWIN, MAC} */


#if defined(NCBI_OS_UNIX)
#  include <unistd.h>
#  define X_SLEEP(x) ((void) sleep(x))
#elif defined(NCBI_OS_MSWIN)
#  include <windows.h>
#  define X_SLEEP(x) ((void) Sleep(1000 * x))
#else
#  define X_SLEEP(x) ((void) 0)
#endif


/* #define DO_CLIENT */
/* #define DO_SERVER */

#define TEST_BUFSIZE 8192

/* exit server before sending the data expected by client(Test 1) */
/*#define TEST_SRV1_SHUTDOWN*/

#ifndef TEST_SRV1_SHUTDOWN
/* exit server immediately after its first client is served(Test 1) */
/*#define TEST_SRV1_ONCE*/
#endif

/* test SOCK_Reconnect() */
#define DO_RECONNECT



/* Log stream
 */
static FILE* log_fp;


/* The simplest randezvous (a plain request-reply) test functions
 *      "TEST__client_1(SOCK sock)"
 *      "TEST__server_1(SOCK sock)"
 */

static const char s_C1[] = "C1";
static const char s_S1[] = "S1";

#define N_SUB_BLOB    10
#define SUB_BLOB_SIZE 7000
#define BIG_BLOB_SIZE (N_SUB_BLOB * SUB_BLOB_SIZE)


static void TEST__client_1(SOCK sock)
{
    ESOCK_Status status;
    size_t       n_io, n_io_done;
    char         buf[TEST_BUFSIZE];

    fprintf(log_fp, "[INFO] TEST__client_1(TC1)\n");

    /* Send a short string */
    n_io = strlen(s_C1) + 1;
    status = SOCK_Write(sock, s_C1, n_io, &n_io_done);
    assert(status == eSOCK_Success  &&  n_io == n_io_done);

    n_io = strlen(s_S1) + 1;
    status = SOCK_Read(sock, buf, n_io, &n_io_done, eSOCK_Peek);
    status = SOCK_Read(sock, buf, n_io, &n_io_done, eSOCK_Read);
    if (status == eSOCK_Closed) {
        fprintf(log_fp, "[WARNING] TC1:: connection closed\n");
        assert(0);
    }
    assert(status == eSOCK_Success  &&  n_io == n_io_done);
    assert(strcmp(buf, s_S1) == 0);
    assert(SOCK_PushBack(sock, buf, n_io_done) == eSOCK_Success);
    memset(buf, '\xFF', n_io_done);
    assert(SOCK_Read(sock, buf, n_io_done, &n_io_done, eSOCK_Read)
           == eSOCK_Success);
    assert(strcmp(buf, s_S1) == 0);

    /* Send a very big binary blob */
    {{
        size_t i;
        char* blob = (char*) malloc(BIG_BLOB_SIZE);
        for (i = 0;  i < BIG_BLOB_SIZE;  blob[i] = (char)i, i++)
            continue;
        for (i = 0;  i < 10;  i++) {
            status = SOCK_Write(sock, blob + i * SUB_BLOB_SIZE, SUB_BLOB_SIZE,
                                &n_io_done);
            assert(status == eSOCK_Success  &&  n_io_done==SUB_BLOB_SIZE);
        }
        free(blob);
    }}
}


static void TEST__server_1(SOCK sock)
{
    ESOCK_Status status;
    size_t       n_io, n_io_done;
    char         buf[TEST_BUFSIZE];

    fprintf(log_fp, "[INFO] TEST__server_1(TS1)\n");

    /* Receive and send back a short string */
    n_io = strlen(s_C1) + 1;
    status = SOCK_Read(sock, buf, n_io, &n_io_done, eSOCK_Read);
    assert(status == eSOCK_Success  &&  n_io == n_io_done);
    assert(strcmp(buf, s_C1) == 0);

#ifdef TEST_SRV1_SHUTDOWN
    return 212;
#endif

    n_io = strlen(s_S1) + 1;
    status = SOCK_Write(sock, s_S1, n_io, &n_io_done);
    assert(status == eSOCK_Success  &&  n_io == n_io_done);

    /* Receive a very big binary blob, and check its content */
    {{
        char* blob = (char*) malloc(BIG_BLOB_SIZE);
        status = SOCK_Read(sock, blob, BIG_BLOB_SIZE, &n_io_done,
                           eSOCK_Persist);
        assert(status == eSOCK_Success  &&  n_io_done == BIG_BLOB_SIZE);
        for (n_io = 0;  n_io < BIG_BLOB_SIZE;  n_io++)
            assert( blob[n_io] == (char)n_io );
        free(blob);
    }}
}


/* More complicated randezvous test functions
 *      "TEST__client_2(SOCK sock)"
 *      "TEST__server_2(SOCK sock)"
 */

static void s_DoubleTimeout(SSOCK_Timeout *to) {
    if (!to->sec  &&  !to->usec) {
        to->usec = 1;
    } else {
        to->sec   = 2 * to->sec + (2 * to->usec) / 1000000;
        to->usec = (2 * to->usec) % 1000000;
    }
}

static void TEST__client_2(SOCK sock)
{
#define W_FIELD  10
#define N_FIELD  1000
#define N_REPEAT 10
#define N_RECONNECT 3
    ESOCK_Status status;
    size_t       n_io, n_io_done, i;
    char         buf[W_FIELD * N_FIELD + 1];

    fprintf(log_fp, "[INFO] TEST__client_2(TC2)\n");

    /* fill out a buffer to send to server */
    memset(buf, 0, sizeof(buf));
    for (i = 0;  i < N_FIELD;  i++) {
        sprintf(buf + i * W_FIELD, "%10lu", (unsigned long)i);
    }

    /* send the buffer to server, then get it back */
    for (i = 0;  i < N_REPEAT;  i++) {
        char          buf1[sizeof(buf)];
        SSOCK_Timeout w_to, r_to;
        int/*bool*/   w_timeout_on = (int)(i%2); /* if to start from     */
        int/*bool*/   r_timeout_on = (int)(i%3); /* zero or inf. timeout */
        char*         x_buf;

        /* set timeout */
        w_to.sec  = 0;
        w_to.usec = 0;
        status = SOCK_SetTimeout(sock, eSOCK_OnWrite, w_timeout_on ? &w_to :0);
        assert(status == eSOCK_Success);

#ifdef DO_RECONNECT
        /* reconnect */
        if ((i % N_RECONNECT) == 0) {
            size_t j = i / N_RECONNECT;
            do {
                status = SOCK_Reconnect(sock, 0, 0, 0);
                fprintf(log_fp,
                        "[INFO] TEST__client_2::reconnect: i=%lu, status=%s\n",
                        (unsigned long)i, SOCK_StatusStr(status));
                assert(status == eSOCK_Success);
                assert(!SOCK_Eof(sock));

                /* give a break to let server reset the listening socket */
                X_SLEEP(1);
            } while ( j-- );
        }
#endif

        /* send */
        x_buf = buf;
        n_io = sizeof(buf);
        do {
            X_SLEEP(1);
            status = SOCK_Write(sock, x_buf, n_io, &n_io_done);
            if (status == eSOCK_Closed) {
                fprintf(log_fp,
                        "[ERROR] TC2::write: connection closed\n");
                assert(0);
            }
            fprintf(log_fp, "[INFO] TC2::write "
                    "i=%d, status=%7s, n_io=%5lu, n_io_done=%5lu"
                    "\ntimeout(%d): %5lu sec, %6lu msec\n",
                    (int)i, SOCK_StatusStr(status),
                    (unsigned long)n_io, (unsigned long)n_io_done,
                    (int)w_timeout_on,
                    (unsigned long)w_to.sec, (unsigned long)w_to.usec);
            if ( !w_timeout_on ) {
                assert(status == eSOCK_Success  &&  n_io_done == n_io);
            } else {
                const SSOCK_Timeout* x_to;
                assert(status == eSOCK_Success  ||  status == eSOCK_Timeout);
                x_to = SOCK_GetWriteTimeout(sock);
                assert(w_to.sec == x_to->sec  &&  w_to.usec == x_to->usec);
            }
            n_io  -= n_io_done;
            x_buf += n_io_done;
            if (status == eSOCK_Timeout)
                s_DoubleTimeout(&w_to);
            status = SOCK_SetTimeout(sock, eSOCK_OnWrite, &w_to);
            assert(status == eSOCK_Success);
            w_timeout_on = 1/*true*/;
        } while ( n_io );

        /* get back the just sent data */
        r_to.sec  = 0;
        r_to.usec = 0;
        status = SOCK_SetTimeout(sock, eSOCK_OnRead,
                                 (r_timeout_on ? &r_to : 0));
        assert(status == eSOCK_Success);

        x_buf = buf1;
        n_io = sizeof(buf1);
        do {
            if (i%2 == 0) {
                /* peek a little piece twice and compare */
                char   xx_buf1[128], xx_buf2[128];
                size_t xx_io_done1, xx_io_done2;
                if (SOCK_Read(sock, xx_buf1, sizeof(xx_buf1), &xx_io_done1,
                              eSOCK_Peek) == eSOCK_Success  &&
                    SOCK_Read(sock, xx_buf2, xx_io_done1, &xx_io_done2,
                              eSOCK_Peek) == eSOCK_Success) {
                    assert(xx_io_done1 >= xx_io_done2);
                    assert(memcmp(xx_buf1, xx_buf2, xx_io_done2) == 0);
                }
            }
            status = SOCK_Read(sock, x_buf, n_io, &n_io_done, eSOCK_Read);
            if (status == eSOCK_Closed) {
                fprintf(log_fp,
                        "[ERROR] TC2::read: connection closed\n");
                assert(SOCK_Eof(sock));
                assert(0);
            }
            fprintf(log_fp, "[INFO] TC2::read: "
                    "i=%d, status=%7s, n_io=%5lu, n_io_done=%5lu"
                    "\ntimeout(%d): %5u sec, %6u usec\n",
                    (int)i, SOCK_StatusStr(status),
                    (unsigned long)n_io, (unsigned long)n_io_done,
                    (int)r_timeout_on, r_to.sec, r_to.usec);
            if ( !r_timeout_on ) {
                assert(status == eSOCK_Success  &&  n_io_done > 0);
            } else {
                const SSOCK_Timeout* x_to;
                assert(status == eSOCK_Success  ||  status == eSOCK_Timeout);
                x_to = SOCK_GetReadTimeout(sock);
                assert(r_to.sec == x_to->sec  &&  r_to.usec == x_to->usec);
            }

            n_io  -= n_io_done;
            x_buf += n_io_done;
            if (status == eSOCK_Timeout)
                s_DoubleTimeout(&r_to);
            status = SOCK_SetTimeout(sock, eSOCK_OnRead, &r_to);
            assert(status == eSOCK_Success);
            r_timeout_on = 1/*true*/;
        } while ( n_io );

        assert(memcmp(buf, buf1, sizeof(buf)) == 0);
    }
}


static void TEST__server_2(SOCK sock, LSOCK lsock)
{
    ESOCK_Status  status;
    size_t        n_io, n_io_done;
    char          buf[TEST_BUFSIZE];
    SSOCK_Timeout r_to, w_to;
    size_t        i;

    fprintf(log_fp, "[INFO] TEST__server_2(TS2)\n");

    r_to.sec  = 0;
    r_to.usec = 0;
    w_to = r_to;

    /* goto */
 l_reconnect: /* reconnection loopback */

    status = SOCK_SetTimeout(sock, eSOCK_OnRead,  &r_to);
    assert(status == eSOCK_Success);
    status = SOCK_SetTimeout(sock, eSOCK_OnWrite, &w_to);
    assert(status == eSOCK_Success);

    for (i = 0;  ;  i++) {
        char* x_buf;

        /* read data from socket */
        n_io = sizeof(buf);
        status = SOCK_Read(sock, buf, n_io, &n_io_done, eSOCK_Read);
        switch ( status ) {
        case eSOCK_Success:
            fprintf(log_fp, "[INFO] TS2::read: "
                    "[%lu], status=%7s, n_io=%5lu, n_io_done=%5lu\n",
                    (unsigned long)i, SOCK_StatusStr(status),
                    (unsigned long)n_io, (unsigned long)n_io_done);
            assert(n_io_done > 0);
            break;

        case eSOCK_Closed:
            fprintf(log_fp, "[INFO] TS2::read: connection closed\n");
            assert(SOCK_Eof(sock));

            /* reconnect */
            if ( !lsock )
                return;

            fprintf(log_fp, "[INFO] TS2:: reconnect\n");
            SOCK_Close(sock);
            status = LSOCK_Accept(lsock, NULL, &sock);
            assert(status == eSOCK_Success);
            assert(!SOCK_Eof(sock));
            /* !!! */ 
            goto l_reconnect;

        case eSOCK_Timeout:
            fprintf(log_fp, "[INFO] TS2::read: "
                    "[%lu] timeout expired: %5u sec, %6u usec\n",
                    (unsigned long)i, r_to.sec, r_to.usec);
            assert(n_io_done == 0);
            s_DoubleTimeout(&r_to);
            status = SOCK_SetTimeout(sock, eSOCK_OnRead, &r_to);
            assert(status == eSOCK_Success);
            assert( !SOCK_Eof(sock) );
            break;

        default:
            assert(0);
            return;
        } /* switch */

        /* write(just the same) data back to client */
        n_io  = n_io_done;
        x_buf = buf;
        while ( n_io ) {
            status = SOCK_Write(sock, buf, n_io, &n_io_done);
            switch ( status ) {
            case eSOCK_Success:
                fprintf(log_fp, "[INFO] TS2::write: "
                        "[%lu], status=%7s, n_io=%5lu, n_io_done=%5lu\n",
                        (unsigned long)i, SOCK_StatusStr(status),
                        (unsigned long)n_io, (unsigned long)n_io_done);
                assert(n_io_done > 0);
                break;
            case eSOCK_Closed:
                fprintf(log_fp, "[ERROR] TS2::write: connection closed\n");
                assert(0);
                return;
            case eSOCK_Timeout:
                fprintf(log_fp, "[INFO] TS2::write: "
                        "[%lu] timeout expired: %5u sec, %6u usec\n",
                        (unsigned long)i, w_to.sec, w_to.usec);
                assert(n_io_done == 0);
                s_DoubleTimeout(&w_to);
                status = SOCK_SetTimeout(sock, eSOCK_OnWrite, &w_to);
                assert(status == eSOCK_Success);
                break;
            default:
                assert(0);
                return;
            } /* switch */

            n_io  -= n_io_done;
            x_buf += n_io_done;
        }
    }
}


/* Skeletons for the socket i/o test:
 *   TEST__client(...)
 *   TEST__server(...)
 *   establish and close connection;  call test i/o functions like
 *     TEST__[client|server]_[1|2|...] (...)
 */
static void TEST__client(const char*          server_host,
                         unsigned short       server_port,
                         const SSOCK_Timeout* timeout)
{
    SOCK         sock;
    ESOCK_Status status;

    fprintf(log_fp, "[INFO] TEST__client(host = \"%s\", port = %hu, ",
            server_host, server_port);
    if ( timeout )
        fprintf(log_fp, "timeout = %u.%u)\n", timeout->sec, timeout->usec);
    else
        fprintf(log_fp, "timeout = INFINITE)\n");

    /* Connect to server */
    status = SOCK_Create(server_host, server_port, timeout, &sock);
    assert(status == eSOCK_Success);

    /* Test the simplest randezvous(plain request-reply)
     * The two peer functions are:
     *      "TEST__[client|server]_1(SOCK sock)"
     */
    TEST__client_1(sock);

    /* Test a more complex case
     * The two peer functions are:
     *      "TEST__[client|server]_2(SOCK sock)"
     */
    TEST__client_2(sock);

    /* Close connection and exit */
    status = SOCK_Close(sock);
    assert(status == eSOCK_Success  ||  status == eSOCK_Closed);
}


static void TEST__server(unsigned short port)
{
    LSOCK lsock;
    ESOCK_Status status;

    fprintf(log_fp, "[INFO] TEST__server(port = %hu)\n", port);

    /* Create listening socket */
    status = LSOCK_Create(port, 1, &lsock);
    assert(status == eSOCK_Success);

    /* Accept connections from clients and run test sessions */
    for (;;) {
        /* Accept connection */
        SOCK sock;
        status = LSOCK_Accept(lsock, NULL, &sock);
        assert(status == eSOCK_Success);

        /* Test the simplest randezvous(plain request-reply)
         * The two peer functions are:
         *      "TEST__[client|server]_1(SOCK sock)"
         */
        TEST__server_1(sock);

        /* Test a more complex case
         * The two peer functions are:
         *      "TEST__[client|server]_2(SOCK sock)"
         */
#ifdef DO_RECONNECT
        TEST__server_2(sock, lsock);
#else
        TEST__server_2(sock, 0);
#endif

        /* Close connection */
        status = SOCK_Close(sock);
        assert(status == eSOCK_Success  ||  status == eSOCK_Closed);

#ifdef TEST_SRV1_ONCE
        /* finish after the first session */
        break;
#endif
    } /* for */

    /* Close listening socket */
    assert(LSOCK_Close(lsock) == eSOCK_Success);
}


/* Consistency...
 */
#if defined(DO_SERVER)  &&  defined(DO_CLIENT)
#  error "Only one of DO_SERVER, DO_CLIENT can be defined!"
#endif


/* Fake (printout only) MT critical section callback and data
 */
static char s_MT_FakeData[] = "s_MT_FakeData";

#if defined(__cplusplus)
extern "C" {
  static void s_MT_FakeFunc(void* data, ESOCK_MT_Locking what);
}
#endif /* __cplusplus */

static void s_MT_FakeFunc(void* data, ESOCK_MT_Locking what)
{
    const char* what_str;
    switch ( what ) {
    case eSOCK_MT_Lock:
        what_str = "eSOCK_MT_Lock";
        break;
    case eSOCK_MT_Unlock:
        what_str = "eSOCK_MT_Unlock";
        break;
    case eSOCK_MT_Cleanup:
        what_str = "eSOCK_MT_Cleanup";
        break;
    default:
        assert(0);
        what_str = 0;
    }
    fprintf(log_fp, "s_MT_FakeFunc(\"%s\", %s)\n",
            data ? (char*)data : "<NULL>", what_str);
}




/* Main function
 * Parse command-line options, initialize and cleanup API internals;
 * run client or server test
 */
extern int main(int argc, char** argv)
{
#define MIN_PORT 5001
#define DEF_PORT 5555
#define DEF_HOST "localhost"

    /* Error log stream */
    log_fp = stderr;

    /* Test client or server using hard-coded parameters */
#if   defined(DO_SERVER)
    argc = 2;
#elif defined(DO_CLIENT)
    argc = 3;
#endif

    /* Setup a fake MT critical section callback
     */
    SOCK_MT_SetCriticalSection(s_MT_FakeFunc, s_MT_FakeData);
    SOCK_MT_SetCriticalSection(0, 0);
    SOCK_MT_SetCriticalSection(0, 0);
    SOCK_MT_SetCriticalSection(s_MT_FakeFunc, s_MT_FakeData);

    /* Setup the SOCK error post stream */
    SOCK_SetErrStream(log_fp, 0);

    /* Printout local hostname */
    {{
        char local_host[64];
        assert(SOCK_gethostname(local_host, sizeof(local_host)) == 0);
        fprintf(log_fp, "[INFO] Running NCBISOCK test on host \"%s\"\n",
                local_host);
    }}

    /* Parse cmd.-line args and decide whether it's a client or a server */
    switch ( argc ) {
    case 2: {
        /*** SERVER ***/
        int port;

#if defined(DO_SERVER)
        port = DEF_PORT;
#else
        if (sscanf(argv[1], "%d", &port) != 1  ||  port < MIN_PORT)
            break;
#endif /* DO_SERVER */

        TEST__server((unsigned short) port);
        assert(SOCK_ShutdownAPI() == eSOCK_Success);
        return 0;
    }

    case 3: case 4: {
        /*** CLIENT ***/
        const char*    server_host;
        int            server_port;
        SSOCK_Timeout* timeout = 0;

#if defined(DO_CLIENT)
        server_host = DEF_HOST;
        server_port = DEF_PORT;
#else
        SSOCK_Timeout  x_timeout;
        /* host */
        server_host = argv[1];

        /* port */
        if (sscanf(argv[2], "%d", &server_port) != 1  ||
            server_port < MIN_PORT)
            break;

        /* timeout */
        if (argc == 4) {
            double tm_out = atof(argv[3]);
            if (tm_out < 0)
                break;
            x_timeout.sec  = (unsigned int)tm_out;
            x_timeout.usec = (unsigned int)((tm_out - x_timeout.sec) *1000000);
            timeout = &x_timeout;
        };
#endif /* DO_CLIENT */

        TEST__client(server_host, (unsigned short)server_port, timeout);
        assert(SOCK_ShutdownAPI() == eSOCK_Success);
        return 0;
    }
    } /* switch */

    /*** USAGE ***/
    fprintf(log_fp,
            "Usage:\n"
            "  Client: %s <srv_host> <port> [conn_timeout]\n"
            "  Server: %s <port>\n"
            " where <port> is greater than %d, and [conn_timeout] is double\n",
            argv[0], argv[0], (int)MIN_PORT);
    return 1;
}
