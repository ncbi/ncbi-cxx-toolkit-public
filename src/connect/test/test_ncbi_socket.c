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
 * Revision 6.7  2000/06/23 19:39:22  vakatov
 * Test the logging of socket I/O (incl. binary data)
 *
 * Revision 6.6  2000/03/24 23:12:13  vakatov
 * Starting the development quasi-branch to implement CONN API.
 * All development is performed in the NCBI C++ tree only, while
 * the NCBI C tree still contains "frozen" (see the last revision) code.
 *
 * Revision 6.5  2000/02/24 23:09:42  vakatov
 * Use C++ Toolkit specific wrapper "test_ncbi_socket_.c" for
 * "test_ncbi_socket.c"
 *
 * Revision 6.4  2000/02/23 22:34:37  vakatov
 * Can work both "standalone" and as a part of NCBI C++ or C toolkits
 *
 * Revision 6.3  1999/11/26 19:05:21  vakatov
 * Initialize "log_fp" in "main()"...
 *
 * Revision 6.2  1999/10/19 16:16:03  vakatov
 * Try the NCBI C and C++ headers only if NCBI_OS_{UNIX, MSWIN, MAC} is
 * not #define'd
 *
 * Revision 6.1  1999/10/18 15:40:21  vakatov
 * Initial revision
 *
 * ===========================================================================
 */

#if defined(NDEBUG)
#  undef NDEBUG
#endif 

#include <connect/ncbi_socket.h>
#include <connect/ncbi_util.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>


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
    EIO_Status status;
    size_t     n_io, n_io_done;
    char       buf[TEST_BUFSIZE];

    fprintf(log_fp, "[INFO] TEST__client_1(TC1)\n");

    /* Send a short string */
    SOCK_SetDataLoggingAPI(eOn);
    n_io = strlen(s_C1) + 1;
    status = SOCK_Write(sock, s_C1, n_io, &n_io_done);
    assert(status == eIO_Success  &&  n_io == n_io_done);

    SOCK_SetDataLoggingAPI(eOff);
    SOCK_SetDataLogging(sock, eOn);
    n_io = strlen(s_S1) + 1;
    status = SOCK_Read(sock, buf, n_io, &n_io_done, eIO_Peek);
    status = SOCK_Read(sock, buf, n_io, &n_io_done, eIO_Plain);
    if (status == eIO_Closed) {
        fprintf(log_fp, "[WARNING] TC1:: connection closed\n");
        assert(0);
    }
    assert(status == eIO_Success  &&  n_io == n_io_done);
    assert(strcmp(buf, s_S1) == 0);
    assert(SOCK_PushBack(sock, buf, n_io_done) == eIO_Success);
    memset(buf, '\xFF', n_io_done);
    assert(SOCK_Read(sock, buf, n_io_done, &n_io_done, eIO_Plain)
           == eIO_Success);
    assert(strcmp(buf, s_S1) == 0);

    SOCK_SetDataLoggingAPI(eDefault);
    SOCK_SetDataLogging(sock, eDefault);

    /* Send a very big binary blob */
    {{
        size_t i;
        unsigned char* blob = (unsigned char*) malloc(BIG_BLOB_SIZE);
        for (i = 0;  i < BIG_BLOB_SIZE;  blob[i] = (unsigned char) i, i++)
            continue;
        for (i = 0;  i < 10;  i++) {
            status = SOCK_Write(sock, blob + i * SUB_BLOB_SIZE, SUB_BLOB_SIZE,
                                &n_io_done);
            assert(status == eIO_Success  &&  n_io_done==SUB_BLOB_SIZE);
        }
        free(blob);
    }}
}


static void TEST__server_1(SOCK sock)
{
    EIO_Status status;
    size_t     n_io, n_io_done;
    char       buf[TEST_BUFSIZE];

    fprintf(log_fp, "[INFO] TEST__server_1(TS1)\n");

    /* Receive and send back a short string */
    SOCK_SetDataLogging(sock, eOn);
    n_io = strlen(s_C1) + 1;
    status = SOCK_Read(sock, buf, n_io, &n_io_done, eIO_Plain);
    assert(status == eIO_Success  &&  n_io == n_io_done);
    assert(strcmp(buf, s_C1) == 0);

#ifdef TEST_SRV1_SHUTDOWN
    return 212;
#endif

    SOCK_SetDataLogging(sock, eDefault);
    SOCK_SetDataLoggingAPI(eOn);
    n_io = strlen(s_S1) + 1;
    status = SOCK_Write(sock, s_S1, n_io, &n_io_done);
    assert(status == eIO_Success  &&  n_io == n_io_done);
    SOCK_SetDataLoggingAPI(eOff);

    /* Receive a very big binary blob, and check its content */
    {{
#define DO_LOG_SIZE    300
#define DONT_LOG_SIZE  BIG_BLOB_SIZE - DO_LOG_SIZE
        unsigned char* blob = (unsigned char*) malloc(BIG_BLOB_SIZE);

        status = SOCK_Read(sock, blob, DONT_LOG_SIZE, &n_io_done, eIO_Persist);
        assert(status == eIO_Success  &&  n_io_done == DONT_LOG_SIZE);

        SOCK_SetDataLogging(sock, eOn);
        status = SOCK_Read(sock, blob + DONT_LOG_SIZE, DO_LOG_SIZE,
                           &n_io_done, eIO_Persist);
        assert(status == eIO_Success  &&  n_io_done == DO_LOG_SIZE);
        SOCK_SetDataLogging(sock, eDefault);
        SOCK_SetDataLoggingAPI(eDefault);

        for (n_io = 0;  n_io < BIG_BLOB_SIZE;  n_io++)
            assert(blob[n_io] == (unsigned char) n_io);
        free(blob);
    }}
}


/* More complicated randezvous test functions
 *      "TEST__client_2(SOCK sock)"
 *      "TEST__server_2(SOCK sock)"
 */

static void s_DoubleTimeout(STimeout *to) {
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
    EIO_Status status;
    size_t     n_io, n_io_done, i;
    char       buf[W_FIELD * N_FIELD + 1];

    fprintf(log_fp, "[INFO] TEST__client_2(TC2)\n");

    /* fill out a buffer to send to server */
    memset(buf, 0, sizeof(buf));
    for (i = 0;  i < N_FIELD;  i++) {
        sprintf(buf + i * W_FIELD, "%10lu", (unsigned long)i);
    }

    /* send the buffer to server, then get it back */
    for (i = 0;  i < N_REPEAT;  i++) {
        char        buf1[sizeof(buf)];
        STimeout    w_to, r_to;
        int/*bool*/ w_timeout_on = (int)(i%2); /* if to start from     */
        int/*bool*/ r_timeout_on = (int)(i%3); /* zero or inf. timeout */
        char*       x_buf;

        /* set timeout */
        w_to.sec  = 0;
        w_to.usec = 0;
        status = SOCK_SetTimeout(sock, eIO_Write, w_timeout_on ? &w_to : 0);
        assert(status == eIO_Success);

#ifdef DO_RECONNECT
        /* reconnect */
        if ((i % N_RECONNECT) == 0) {
            size_t j = i / N_RECONNECT;
            do {
                status = SOCK_Reconnect(sock, 0, 0, 0);
                fprintf(log_fp,
                        "[INFO] TEST__client_2::reconnect: i=%lu, status=%s\n",
                        (unsigned long)i, IO_StatusStr(status));
                assert(status == eIO_Success);
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
            if (status == eIO_Closed) {
                fprintf(log_fp,
                        "[ERROR] TC2::write: connection closed\n");
                assert(0);
            }
            fprintf(log_fp, "[INFO] TC2::write "
                    "i=%d, status=%7s, n_io=%5lu, n_io_done=%5lu"
                    "\ntimeout(%d): %5lu sec, %6lu msec\n",
                    (int)i, IO_StatusStr(status),
                    (unsigned long)n_io, (unsigned long)n_io_done,
                    (int)w_timeout_on,
                    (unsigned long)w_to.sec, (unsigned long)w_to.usec);
            if ( !w_timeout_on ) {
                assert(status == eIO_Success  &&  n_io_done == n_io);
            } else {
                const STimeout* x_to;
                assert(status == eIO_Success  ||  status == eIO_Timeout);
                x_to = SOCK_GetTimeout(sock, eIO_Write);
                assert(w_to.sec == x_to->sec  &&  w_to.usec == x_to->usec);
            }
            n_io  -= n_io_done;
            x_buf += n_io_done;
            if (status == eIO_Timeout)
                s_DoubleTimeout(&w_to);
            status = SOCK_SetTimeout(sock, eIO_Write, &w_to);
            assert(status == eIO_Success);
            w_timeout_on = 1/*true*/;
        } while ( n_io );

        /* get back the just sent data */
        r_to.sec  = 0;
        r_to.usec = 0;
        status = SOCK_SetTimeout(sock, eIO_Read, (r_timeout_on ? &r_to : 0));
        assert(status == eIO_Success);

        x_buf = buf1;
        n_io = sizeof(buf1);
        do {
            if (i%2 == 0) {
                /* peek a little piece twice and compare */
                char   xx_buf1[128], xx_buf2[128];
                size_t xx_io_done1, xx_io_done2;
                if (SOCK_Read(sock, xx_buf1, sizeof(xx_buf1), &xx_io_done1,
                              eIO_Peek) == eIO_Success  &&
                    SOCK_Read(sock, xx_buf2, xx_io_done1, &xx_io_done2,
                              eIO_Peek) == eIO_Success) {
                    assert(xx_io_done1 >= xx_io_done2);
                    assert(memcmp(xx_buf1, xx_buf2, xx_io_done2) == 0);
                }
            }
            status = SOCK_Read(sock, x_buf, n_io, &n_io_done, eIO_Plain);
            if (status == eIO_Closed) {
                fprintf(log_fp,
                        "[ERROR] TC2::read: connection closed\n");
                assert(SOCK_Eof(sock));
                assert(0);
            }
            fprintf(log_fp, "[INFO] TC2::read: "
                    "i=%d, status=%7s, n_io=%5lu, n_io_done=%5lu"
                    "\ntimeout(%d): %5u sec, %6u usec\n",
                    (int)i, IO_StatusStr(status),
                    (unsigned long)n_io, (unsigned long)n_io_done,
                    (int)r_timeout_on, r_to.sec, r_to.usec);
            if ( !r_timeout_on ) {
                assert(status == eIO_Success  &&  n_io_done > 0);
            } else {
                const STimeout* x_to;
                assert(status == eIO_Success  ||  status == eIO_Timeout);
                x_to = SOCK_GetTimeout(sock, eIO_Read);
                assert(r_to.sec == x_to->sec  &&  r_to.usec == x_to->usec);
            }

            n_io  -= n_io_done;
            x_buf += n_io_done;
            if (status == eIO_Timeout)
                s_DoubleTimeout(&r_to);
            status = SOCK_SetTimeout(sock, eIO_Read, &r_to);
            assert(status == eIO_Success);
            r_timeout_on = 1/*true*/;
        } while ( n_io );

        assert(memcmp(buf, buf1, sizeof(buf)) == 0);
    }
}


static void TEST__server_2(SOCK sock, LSOCK lsock)
{
    EIO_Status status;
    size_t     n_io, n_io_done;
    char       buf[TEST_BUFSIZE];
    STimeout   r_to, w_to;
    size_t     i;

    fprintf(log_fp, "[INFO] TEST__server_2(TS2)\n");

    r_to.sec  = 0;
    r_to.usec = 0;
    w_to = r_to;

    /* goto */
 l_reconnect: /* reconnection loopback */

    status = SOCK_SetTimeout(sock, eIO_Read,  &r_to);
    assert(status == eIO_Success);
    status = SOCK_SetTimeout(sock, eIO_Write, &w_to);
    assert(status == eIO_Success);

    for (i = 0;  ;  i++) {
        char* x_buf;

        /* read data from socket */
        n_io = sizeof(buf);
        status = SOCK_Read(sock, buf, n_io, &n_io_done, eIO_Plain);
        switch ( status ) {
        case eIO_Success:
            fprintf(log_fp, "[INFO] TS2::read: "
                    "[%lu], status=%7s, n_io=%5lu, n_io_done=%5lu\n",
                    (unsigned long)i, IO_StatusStr(status),
                    (unsigned long)n_io, (unsigned long)n_io_done);
            assert(n_io_done > 0);
            break;

        case eIO_Closed:
            fprintf(log_fp, "[INFO] TS2::read: connection closed\n");
            assert(SOCK_Eof(sock));

            /* reconnect */
            if ( !lsock )
                return;

            fprintf(log_fp, "[INFO] TS2:: reconnect\n");
            SOCK_Close(sock);
            status = LSOCK_Accept(lsock, NULL, &sock);
            assert(status == eIO_Success);
            assert(!SOCK_Eof(sock));
            /* !!! */ 
            goto l_reconnect;

        case eIO_Timeout:
            fprintf(log_fp, "[INFO] TS2::read: "
                    "[%lu] timeout expired: %5u sec, %6u usec\n",
                    (unsigned long)i, r_to.sec, r_to.usec);
            assert(n_io_done == 0);
            s_DoubleTimeout(&r_to);
            status = SOCK_SetTimeout(sock, eIO_Read, &r_to);
            assert(status == eIO_Success);
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
            case eIO_Success:
                fprintf(log_fp, "[INFO] TS2::write: "
                        "[%lu], status=%7s, n_io=%5lu, n_io_done=%5lu\n",
                        (unsigned long)i, IO_StatusStr(status),
                        (unsigned long)n_io, (unsigned long)n_io_done);
                assert(n_io_done > 0);
                break;
            case eIO_Closed:
                fprintf(log_fp, "[ERROR] TS2::write: connection closed\n");
                assert(0);
                return;
            case eIO_Timeout:
                fprintf(log_fp, "[INFO] TS2::write: "
                        "[%lu] timeout expired: %5u sec, %6u usec\n",
                        (unsigned long)i, w_to.sec, w_to.usec);
                assert(n_io_done == 0);
                s_DoubleTimeout(&w_to);
                status = SOCK_SetTimeout(sock, eIO_Write, &w_to);
                assert(status == eIO_Success);
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
static void TEST__client(const char*     server_host,
                         unsigned short  server_port,
                         const STimeout* timeout)
{
    SOCK       sock;
    EIO_Status status;

    fprintf(log_fp, "[INFO] TEST__client(host = \"%s\", port = %hu, ",
            server_host, server_port);
    if ( timeout )
        fprintf(log_fp, "timeout = %u.%u)\n", timeout->sec, timeout->usec);
    else
        fprintf(log_fp, "timeout = INFINITE)\n");

    /* Connect to server */
    status = SOCK_Create(server_host, server_port, timeout, &sock);
    assert(status == eIO_Success);

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
    assert(status == eIO_Success  ||  status == eIO_Closed);
}


static void TEST__server(unsigned short port)
{
    LSOCK      lsock;
    EIO_Status status;

    fprintf(log_fp, "[INFO] TEST__server(port = %hu)\n", port);

    /* Create listening socket */
    status = LSOCK_Create(port, 1, &lsock);
    assert(status == eIO_Success);

    /* Accept connections from clients and run test sessions */
    for (;;) {
        /* Accept connection */
        SOCK sock;
        status = LSOCK_Accept(lsock, NULL, &sock);
        assert(status == eIO_Success);

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
        assert(status == eIO_Success  ||  status == eIO_Closed);

#ifdef TEST_SRV1_ONCE
        /* Close listening socket */
        assert(LSOCK_Close(lsock) == eIO_Success);
        /* Finish after the first session */
        break;
#endif
    } /* for */
}


/* Consistency...
 */
#if defined(DO_SERVER)  &&  defined(DO_CLIENT)
#  error "Only one of DO_SERVER, DO_CLIENT can be defined!"
#endif


/* Fake (printout only) MT critical section callback and data
 */
static char TEST_LockUserData[] = "TEST_LockUserData";

#if defined(__cplusplus)
extern "C" {
    static int/*bool*/ TEST_LockHandler(void* user_data, EMT_Lock how);
    static void        TEST_LockCleanup(void* user_data);
}
#endif /* __cplusplus */


static int/*bool*/ TEST_LockHandler(void* user_data, EMT_Lock how)
{
    const char* what_str = 0;
    switch ( how ) {
    case eMT_Lock:
        what_str = "eMT_Lock";
        break;
    case eMT_LockRead:
        what_str = "eMT_LockRead";
        break;
    case eMT_Unlock:
        what_str = "eMT_Unlock";
        break;
    }
    fprintf(log_fp, "TEST_LockHandler(\"%s\", %s)\n",
            user_data ? (char*)user_data : "<NULL>", what_str);
    return 1/*true*/;
}


static void TEST_LockCleanup(void* user_data)
{
    fprintf(log_fp, "TEST_LockCleanup(\"%s\")\n",
            user_data ? (char*)user_data : "<NULL>");
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

    /* Try to set various fake MT safety locks
     */
    CORE_SetLOCK( MT_LOCK_Create(0, TEST_LockHandler, TEST_LockCleanup) );
    CORE_SetLOCK(0);
    CORE_SetLOCK(0);
    CORE_SetLOCK( MT_LOCK_Create(&TEST_LockUserData,
                                 TEST_LockHandler, TEST_LockCleanup) );

    /* Setup log stream
     */
    CORE_SetLOGFILE(stderr, 0/*false*/);


    /* Printout local hostname
     */
    {{
        char local_host[64];
        assert(SOCK_gethostname(local_host, sizeof(local_host)) == 0);
        fprintf(log_fp, "[INFO] Running NCBISOCK test on host \"%s\"\n",
                local_host);
    }}

    /* Parse cmd.-line args and decide whether it's a client or a server
     */
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
        assert(SOCK_ShutdownAPI() == eIO_Success);
        CORE_SetLOG(0);
        CORE_SetLOCK(0);
        return 0;
    }

    case 3: case 4: {
        /*** CLIENT ***/
        const char* server_host;
        int         server_port;
        STimeout*   timeout = 0;

#if defined(DO_CLIENT)
        server_host = DEF_HOST;
        server_port = DEF_PORT;
#else
        STimeout    x_timeout;
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
        assert(SOCK_ShutdownAPI() == eIO_Success);
        CORE_SetLOG(0);
        CORE_SetLOCK(0);
        return 0;
    }
    } /* switch */


    /* USAGE
     */
    fprintf(log_fp,
            "\nUSAGE:\n"
            "  Client: %s <srv_host> <port> [conn_timeout]\n"
            "  Server: %s <port>\n"
            " where <port> is greater than %d, and [conn_timeout] is double\n",
            argv[0], argv[0], (int)MIN_PORT);
    CORE_SetLOG(0);
    CORE_SetLOCK(0);
    return 1;
}
