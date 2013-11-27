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
 *   Test case for FTP-based CONNECTOR
 *
 */

#include <connect/ncbi_connutil.h>
#include <connect/ncbi_ftp_connector.h>
#include "../ncbi_ansi_ext.h"
#include "../ncbi_priv.h"               /* CORE logging facilities */
#include <stdlib.h>
#ifdef HAVE_GETTIMEOFDAY
#  include <sys/time.h>
#endif /*HAVE_GETTIMEOFDAY*/
#include <time.h>

#include "test_assert.h"  /* This header must go last */

#define TEST_HOST  "ftp.ncbi.nlm.nih.gov"
#define TEST_PORT  0
#define TEST_USER  "ftp"
#define TEST_PASS  "none"
#define TEST_PATH  ((char*) 0)


static double s_GetTime(void)
{
#ifdef HAVE_GETTIMEOFDAY
    struct timeval t;
    return gettimeofday(&t, 0) == 0 ? t.tv_sec + t.tv_usec / 1000000.0 : 0.0;
#else
    time_t t = time(0);
    return (double)((unsigned long) t);
#endif /*HAVE_GETTIMEOFDAY*/
}


int main(int argc, char* argv[])
{
    static const char kChdir[] = "CWD /toolbox/ncbi_tools++\n";
    static const char kFile[] = "DATA/Misc/test_ncbi_conn_stream.FTP.data";
    int/*bool*/   cancel = 0, first;
    TFTP_Flags    flag = 0;
    SConnNetInfo* net_info;
    char          buf[1024];
    CONNECTOR     connector;
    FILE*         data_file;
    size_t        size, n;
    double        elapsed;
    EIO_Status    status;
    CONN          conn;

    g_NCBI_ConnectRandomSeed = (int) time(0) ^ NCBI_CONNECT_SRAND_ADDEND;
    srand(g_NCBI_ConnectRandomSeed);

    /* Log and data-log streams */
    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);
    CORE_SetLOGFILE(stderr, 0/*false*/);
    data_file = fopen("test_ncbi_ftp_connector.dat", "wb");
    assert(data_file);

    assert((net_info = ConnNetInfo_Create(0)) != 0);
    if (net_info->debug_printout == eDebugPrintout_Some)
        flag |= fFTP_LogControl;
    else if (net_info->debug_printout == eDebugPrintout_Data) {
        char val[32];
        ConnNetInfo_GetValue(0, REG_CONN_DEBUG_PRINTOUT, val, sizeof(val),
                             DEF_CONN_DEBUG_PRINTOUT);
        flag |= strcasecmp(val, "all") == 0 ? fFTP_LogAll : fFTP_LogData;
    }
    flag |= fFTP_UseFeatures;

    if (TEST_PORT) {
        sprintf(buf, ":%hu", TEST_PORT);
    } else {
        *buf = 0;
    }
    CORE_LOGF(eLOG_Note, ("Connecting to ftp://%s:%s@%s%s/",
                          TEST_USER, TEST_PASS, TEST_HOST, buf));
    /* Run the tests */
    connector = FTP_CreateConnectorSimple(TEST_HOST, TEST_PORT,
                                          TEST_USER, TEST_PASS,
                                          TEST_PATH, flag, 0);

    if (CONN_CreateEx(connector,
                      fCONN_Supplement | fCONN_Untie, &conn) != eIO_Success) {
        CORE_LOG(eLOG_Fatal, "Cannot create FTP download connection");
    }

    assert(CONN_SetTimeout(conn, eIO_Open,      net_info->timeout)
           == eIO_Success);
    assert(CONN_SetTimeout(conn, eIO_ReadWrite, net_info->timeout)
           == eIO_Success);
    assert(CONN_SetTimeout(conn, eIO_Close,     net_info->timeout)
           == eIO_Success);

    if (CONN_Read(conn, buf, sizeof(buf), &n, eIO_ReadPlain) != eIO_Closed)
        CORE_LOG(eLOG_Fatal, "Test failed in empty READ");

    if (CONN_Write(conn, "aaa", 3, &n, eIO_WritePlain) != eIO_Success)
        CORE_LOG(eLOG_Fatal, "Cannot write FTP command");

    if (CONN_Wait(conn, eIO_Read, net_info->timeout) != eIO_NotSupported)
        CORE_LOG(eLOG_Fatal, "Test failed in waiting on READ");
    CORE_LOG(eLOG_Note, "Unrecognized command correctly rejected");

    if (CONN_Write(conn, "LIST\nSIZE", 9, &n, eIO_WritePlain) != eIO_Unknown)
        CORE_LOG(eLOG_Fatal, "Test failed to reject multiple commands");
    CORE_LOG(eLOG_Note, "Multiple commands correctly rejected");

    status = CONN_Write(conn, "SIZE 1GB", 9, &n, eIO_WritePlain);
    if (status == eIO_Success) {
        char buf[128];
        CONN_ReadLine(conn, buf, sizeof(buf) - 1, &n);
        CORE_LOGF(eLOG_Note, ("SIZE file: %.*s", (int) n, buf));
    } else {
        CORE_LOGF(eLOG_Note, ("SIZE command not accepted: %s",
                              IO_StatusStr(status)));
    }

    if (CONN_Write(conn, "LIST", 4, &n, eIO_WritePlain) != eIO_Success)
        CORE_LOG(eLOG_Fatal, "Cannot write LIST command");

    CORE_LOG(eLOG_Note, "LIST command output:");
    first = 1/*true*/;
    do {
        status = CONN_Read(conn, buf, sizeof(buf), &n, eIO_ReadPlain);
        if (n != 0) {
            printf("%.*s", (int) n, buf);
            first = 0/*false*/;
            fflush(stdout);
        }
    } while (status == eIO_Success);
    if (first  ||  status != eIO_Closed) {
        printf("<%s>\n", status != eIO_Success ? IO_StatusStr(status) : "EOF");
        fflush(stdout);
    }

    if (CONN_Write(conn, "NLST\r\n", 6, &n, eIO_WritePlain) != eIO_Success)
        CORE_LOG(eLOG_Fatal, "Cannot write NLST command");

    CORE_LOG(eLOG_Note, "NLST command output:");
    first = 1/*true*/;
    do {
        status = CONN_Read(conn, buf, sizeof(buf), &n, eIO_ReadPlain);
        if (n != 0) {
            printf("%.*s", (int) n, buf);
            first = 0/*false*/;
            fflush(stdout);
        } else
            assert(status != eIO_Success);
    } while (status == eIO_Success);
    if (first  ||  status != eIO_Closed) {
        printf("<%s>\n", status != eIO_Success ? IO_StatusStr(status) : "EOF");
        fflush(stdout);
    }

    if (CONN_Write(conn, "SIZE ", 5, &n, eIO_WritePersist) != eIO_Success)
        CORE_LOG(eLOG_Fatal, "Cannot write SIZE directory command");
    size = strlen(kChdir + 4);
    if (CONN_Write(conn, kChdir + 4, size, &n, eIO_WritePlain) == eIO_Success){
        CONN_ReadLine(conn, buf, sizeof(buf) - 1, &n);
        CORE_LOGF(eLOG_Note, ("SIZE directory returned: %.*s", (int) n, buf));
    } else {
        CORE_LOGF(eLOG_Note, ("SIZE directory not accepted: %s",
                              IO_StatusStr(status)));
    }

    if (CONN_Write(conn, kChdir, sizeof(kChdir) - 1, &n, eIO_WritePlain)
        != eIO_Success) {
        CORE_LOGF(eLOG_Fatal, ("Cannot execute %.*s",
                               (int) sizeof(kChdir) - 2, kChdir));
    }
    if (CONN_Write(conn, "PWD\n", 4, &n, eIO_WritePlain) == eIO_Success) {
        CONN_ReadLine(conn, buf, sizeof(buf) - 1, &n);
        CORE_LOGF(eLOG_Note, ("PWD returned: %.*s", (int) n, buf));
    } else
        CORE_LOG(eLOG_Fatal, "Cannot execute PWD");

    if (CONN_Write(conn, "CDUP\n", 5, &n, eIO_WritePlain) != eIO_Success)
        CORE_LOG(eLOG_Fatal, "Cannot execute CDUP"); 
    if (CONN_Write(conn, "XCUP\n", 5, &n, eIO_WritePlain) != eIO_Success)
        CORE_LOG(eLOG_Fatal, "Cannot execute XCUP");

    if (CONN_Write(conn, kChdir, sizeof(kChdir) - 1, &n, eIO_WritePlain)
        != eIO_Success) {
        CORE_LOGF(eLOG_Fatal, ("Cannot re-execute %.*s",
                               (int) sizeof(kChdir) - 2, kChdir));
    }

    size = sizeof(kFile) - 1;
    if ((status = CONN_Write(conn, "MDTM ", 5, &n, eIO_WritePersist))
        == eIO_Success  &&  n == 5  &&
        (status = CONN_Write(conn, kFile, size, &n, eIO_WritePlain))
        == eIO_Success  &&  n == size) {
        unsigned long val;
        char buf[128];
        CONN_ReadLine(conn, buf, sizeof(buf) - 1, &n);
        if (n  &&  sscanf(buf, "%lu", &val) > 0) {
            struct tm* tm;
            time_t t = (time_t) val;
            if ((tm = localtime(&t)) != 0)
                n = strftime(buf, sizeof(buf), "%m/%d/%Y %H:%M:%S", tm);
        }
        CORE_LOGF(eLOG_Note, ("MDTM returned: %.*s", (int) n, buf));
    } else {
        CORE_LOGF(eLOG_Note, ("MDTM command not accepted: %s",
                              IO_StatusStr(status)));
    }

    if ((status = CONN_Write(conn, "RETR ", 5, &n, eIO_WritePersist))
        != eIO_Success  ||  n != 5  ||
        (status = CONN_Write(conn, kFile, size, &n, eIO_WritePersist))
        != eIO_Success  ||  n != size) {
        CORE_LOGF(eLOG_Fatal, ("Cannot write 'RETR %s': %s",
                               kFile, IO_StatusStr(status)));
    }

    size = 0;
    elapsed = s_GetTime();
    do {
        status = CONN_Read(conn, buf, sizeof(buf), &n, eIO_ReadPlain);
        if (n != 0) {
            fwrite(buf, n, 1, data_file);
            fflush(data_file);
            size += n;
            rand();
            if (argc > 1  &&  rand() % 100000 == 55555) {
                cancel = 1;
                break;
            }
        } else {
            assert(status != eIO_Success);
            if (status != eIO_Closed  ||  !size)
                CORE_LOGF(eLOG_Error, ("Read error: %s",IO_StatusStr(status)));
        }
    } while (status == eIO_Success);
    if (status != eIO_Success)
        cancel = 0;
    elapsed = s_GetTime() - elapsed;

    if (!cancel  ||  !(rand() & 1)) {
        if (cancel)
            CORE_LOG(eLOG_Note, "Cancelling download by a command");
        if (CONN_Write(conn, "NLST blah*", 10, &n, eIO_WritePlain)
            != eIO_Success) {
            CORE_LOG(eLOG_Fatal, "Cannot write garbled NLST command");
        }

        CORE_LOG(eLOG_Note, "Garbled NLST command output (expected empty):");
        first = 1/*true*/;
        do {
            status = CONN_Read(conn, buf, sizeof(buf), &n, eIO_ReadPlain);
            if (n != 0) {
                printf("%.*s", (int) n, buf);
                first = 0/*false*/;
                fflush(stdout);
            }
        } while (status == eIO_Success);
        if (first) {
            printf("<EOF>\n");
            fflush(stdout);
        }
    } else if (cancel)
        CORE_LOG(eLOG_Note, "Closing with download still in progress");

    if (CONN_Close(conn) != eIO_Success)
        CORE_LOG(eLOG_Fatal, "Error in closing FTP connection");

    /* Cleanup and exit */
    fclose(data_file);
    if (!cancel) {
        CORE_LOGF(size ? eLOG_Note : eLOG_Fatal,
                  ("%lu byte(s) downloaded in %.2f second(s) @ %.2fKB/s",
                   (unsigned long) size, elapsed,
                   (unsigned long) size / (1024 * (elapsed ? elapsed : 1.0))));
    } else
        remove("test_ncbi_ftp_connector.dat");
    ConnNetInfo_Destroy(net_info);

    CORE_LOG(eLOG_Note, "TEST completed successfully");
    CORE_SetLOG(0);
    return 0;
}
