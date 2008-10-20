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
 *   Test suite for NCBI connector (CONNECTOR)
 *   (see also "ncbi_connection.[ch]", "ncbi_connector.h")
 *
 */

#include "ncbi_conntest.h"
#include "../ncbi_priv.h"
#include <connect/ncbi_connection.h>
#include <string.h>
/* This header must go last */
#include "test_assert.h"


/***********************************************************************
 *  INTERNAL
 */


/* Standard error report
 */
#define TEST_LOG(status, descr) \
  CORE_LOGF(status == eIO_Success ? eLOG_Note : \
            status == eIO_Closed  ? eLOG_Warning : \
                                    eLOG_Error, \
            ("%s (status: \"%s\")", descr, IO_StatusStr(status)))


/* TESTs
 */

static void s_SingleBouncePrint
(CONN  conn,
 FILE* data_file)
{
    static const char write_str[] = "This is a s_*BouncePrint test string.\n";
    size_t     n_written, n_read;
    char       buf[8192];
    EIO_Status status;

    TEST_LOG(eIO_Success, "[s_SingleBouncePrint]  Starting...");

    /* WRITE */
    status = CONN_Write(conn, write_str, strlen(write_str),
                        &n_written, eIO_WritePersist);
    if (status != eIO_Success  ||  n_written != strlen(write_str))
        TEST_LOG(status, "[s_SingleBouncePrint] Write failed!");
    assert(n_written == strlen(write_str));
    assert(status == eIO_Success);

    /* READ the "bounced" data from the connection */
    status = CONN_Read(conn, buf, sizeof(buf) - 1, &n_read, eIO_ReadPersist);
    TEST_LOG(status, "[s_SingleBouncePrint] after READ");

    /* Printout to data file, if any */
    if (data_file  &&  n_read) {
        fprintf(data_file, "\ns_SingleBouncePrint(BEGIN PRINT)\n");
        assert(fwrite(buf, n_read, 1, data_file) == 1);
        fprintf(data_file, "\ns_SingleBouncePrint(END PRINT)\n");
        fflush(data_file);
    }

    /* Check-up */
    assert(n_read >= n_written);
    buf[n_read] = '\0';
    assert(strstr(buf, write_str));
}


static void s_MultiBouncePrint
(CONN  conn,
 FILE* data_file)
{
    int i;

    TEST_LOG(eIO_Success, "[s_MultiBouncePrint]  Starting...");
    if ( data_file ) {
        fprintf(data_file, "\ns_MultiBouncePrint(BEGIN)\n");
        fflush(data_file);
    }

    for (i = 0;  i < 5;  i++) {
        s_SingleBouncePrint(conn, data_file);
    }

    TEST_LOG(eIO_Success, "[s_MultiBouncePrint]  ...finished");
    if ( data_file ) {
        fprintf(data_file, "\ns_MultiBouncePrint(END)\n");
        fflush(data_file);
    }
}


static void s_SingleBounceCheck
(CONN            conn,
 const STimeout* timeout,
 FILE*           data_file)
{
    EIO_Status status;
    static const char sym[] = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'
    };

#define TEST_N_LINES 200
#define TEST_BUF_SIZE (TEST_N_LINES * (TEST_N_LINES + 3) / 2)
    char  buf[TEST_BUF_SIZE];

    TEST_LOG(eIO_Success, "[s_SingleBounceCheck]  Starting...");


    /* WRITE to the connection:  "0\n12\n345\n6789\n01234\n........"
     */
    {{
        size_t k = 0, j = 0;
        size_t i;
        for (i = 0;  k != sizeof(buf);  i++) {
            /* prepare output data */
            size_t n_write, n_written;
            for (n_write = 0;  n_write < i;  n_write++, k++) {
                assert(k < sizeof(buf));
                buf[n_write] = sym[j++ % sizeof(sym)];
            }
            assert(k < sizeof(buf));
            if ( n_write ) {
                buf[n_write++] = '\n';  k++;
            }
            buf[n_write] = '\0';

            do { /* persistently */
                /* WAIT... sometimes */
                if (n_write % 5 == 3) {
                    status = CONN_Wait(conn, eIO_Write, timeout);
                    if (status != eIO_Success) {
                        TEST_LOG(status, "The pre-WRITE CONN_Wait failed");
                        assert(status == eIO_Timeout);
                    }
                }

                /* WRITE */
                status = CONN_Write(conn, buf, n_write,
                                    &n_written, eIO_WritePersist);
                if (status != eIO_Success) {
                    TEST_LOG(status, "Write failed. Retrying...");
                    assert(n_written < n_write);
                    assert(status == eIO_Timeout);
                }
                else {
                    assert(n_written == n_write);
                }
            } while (status != eIO_Success);
        }
    }}

    /* READ the "bounced" data from the connection, the first TEST_BUF_SIZE
     * bytes must be:  "0\n12\n345\n6789\n01234\n........"
     */
    {{
        char* x_buf;
        size_t n_read, n_to_read;

        memset(buf, '\0', TEST_BUF_SIZE);

        /* PEEK until the 1st 1/3 of the "bounced" data is available */
        x_buf = buf;
        n_to_read = TEST_BUF_SIZE/3;

        do {
            status = CONN_Read(conn, x_buf, n_to_read, &n_read, eIO_ReadPeek);
            if (status != eIO_Success) {
                TEST_LOG(status, "The 1/3 PEEK failed. Retrying...");
                assert(n_read < n_to_read);
                assert(status == eIO_Timeout);
            }
            if (n_read < n_to_read) {
                TEST_LOG(status, "Not all of the expected data is peeked yet."
                         " Continue...");
            }
        } while (n_read != n_to_read);

        /* READ 1st 1/3 of "bounced" data, compare it with the PEEKed data */
        status = CONN_Read(conn, x_buf + n_to_read, n_to_read, &n_read,
                           eIO_ReadPlain);
        assert(status == eIO_Success);
        assert(n_read == n_to_read);
        assert(memcmp(x_buf, x_buf + n_to_read, n_to_read) == 0);
        memset(x_buf + n_to_read, '\0', n_to_read);

        /* WAIT on read */
        status = CONN_Wait(conn, eIO_Read, timeout);
        if (status != eIO_Success) {
            TEST_LOG(status, "The 2/3 pre-READ CONN_Wait failed");
            assert(status == eIO_Timeout);
        }

        /* READ the 2nd 1/3 of "bounced" data */
        x_buf = buf + TEST_BUF_SIZE/3;
        n_to_read = TEST_BUF_SIZE/3;

        while ( n_to_read ) {
            TEST_LOG(status, "2/3 READ...");
            status = CONN_Read(conn, x_buf, n_to_read, &n_read, eIO_ReadPlain);
            if (status != eIO_Success) {
                TEST_LOG(status, "The 2/3 READ failed. Retrying...");
                assert(n_read < n_to_read);
                assert(status == eIO_Timeout);
            } else {
                assert(n_read <= n_to_read);
            }
            n_to_read -= n_read;
            x_buf     += n_read;
        }
        assert(status == eIO_Success);

        /* Persistently READ the 3rd 1/3 of "bounced" data */
        n_to_read = TEST_BUF_SIZE - (x_buf - buf);

        status = CONN_Read(conn, x_buf, n_to_read, &n_read, eIO_ReadPersist);
        if (status != eIO_Success) {
            TEST_LOG(status, "The 3/3 (persistent) READ failed!");
            assert(n_read < n_to_read);
            assert(0);
        } else {
            assert(n_read == n_to_read);
        }
    }}

    /* Check for the received "bounced" data is identical to the sent data
     */
    {{
        const char* x_buf = buf;
        size_t k = 0, j = 0;
        size_t i;
        for (i = 1;  k != sizeof(buf);  i++) {
            size_t n;
            for (n = 0;  n < i;  n++, k++) {
                if (k == sizeof(buf))
                    break;
                assert(*x_buf++ == sym[j++ % sizeof(sym)]);
            }
            assert(*x_buf++ == '\n');  k++;
        }
    }}

    /* Now when the "bounced" data is read and tested, READ an arbitrary extra
     * data sent in by the peer and print it out to LOG file
     */
    if ( !data_file )
        return;

    fprintf(data_file, "\ns_SingleBounceCheck(BEGIN EXTRA DATA)\n");
    fflush(data_file);
    for (;;) {
        size_t n_read;

        status = CONN_Read(conn, buf, sizeof(buf), &n_read, eIO_ReadPersist);
        TEST_LOG(status, "s_SingleBounceCheck(The extra data READ...)");
        if ( n_read ) {
            assert(fwrite(buf, n_read, 1, data_file) == 1);
            fflush(data_file);
        }
        if (status == eIO_Closed  ||  status == eIO_Timeout)
            break; /* okay */

        assert(status == eIO_Success);
    }
    fprintf(data_file, "\ns_SingleBounceCheck(END EXTRA DATA)\n\n");
    fflush(data_file);
}



/***********************************************************************
 *  EXTERNAL
 */


extern void CONN_TestConnector
(CONNECTOR       connector,
 const STimeout* timeout,
 FILE*           data_file,
 TTestConnFlags  flags)
{
    EIO_Status status;
    CONN       conn;

    TEST_LOG(eIO_Success, "[CONN_TestConnector]  Starting...");


    /* CREATE new connection on the base of the connector, set
     * TIMEOUTs, try to RECONNECT, WAIT for the connection is writable
     */
    assert(CONN_Create(connector, &conn) == eIO_Success);

    CONN_SetTimeout(conn, eIO_Open,      timeout);
    CONN_SetTimeout(conn, eIO_Read,      timeout);
    CONN_SetTimeout(conn, eIO_ReadWrite, timeout);
    CONN_SetTimeout(conn, eIO_Close,     timeout);

    assert(CONN_ReInit(conn, connector) == eIO_Success);

    CONN_SetTimeout(conn, eIO_Write, timeout);

    status = CONN_Wait(conn, eIO_Write, timeout);
    if (status != eIO_Success) {
        TEST_LOG(status, "First CONN_Wait failed");
        assert(status == eIO_Timeout);
    }

    /* Run the specified TESTs
     */
    if ( !flags )
        flags = fTC_Everything;

    if (flags & fTC_SingleBouncePrint) {
        s_SingleBouncePrint(conn, data_file);
    }
    if (flags & fTC_MultiBouncePrint) {
        s_MultiBouncePrint(conn, data_file);
    }
    if (flags & fTC_SingleBounceCheck) {
        s_SingleBounceCheck(conn, timeout, data_file);
    }

    /* And CLOSE the connection...
     */
    assert(CONN_Close(conn) == eIO_Success);

    TEST_LOG(status, "Test completed successfully");
}
