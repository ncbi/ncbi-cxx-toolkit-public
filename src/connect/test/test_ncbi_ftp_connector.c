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
 *   Test case for FTP-based CONNECTOR
 *
 */

#include "../ncbi_ansi_ext.h"
#include "../ncbi_priv.h"
#include <connect/ncbi_connection.h>
#include <connect/ncbi_ftp_connector.h>
/* This header must go last */
#include "test_assert.h"


#define TEST_HOST            "ftp.ncbi.nlm.nih.gov"
#define TEST_PORT            0
#define TEST_USER            "ftp"
#define TEST_PASS            "none"
#define TEST_PATH            "/"


int main(void)
{
    static const char chdir[] = "CWD /toolbox/ncbi_tools\n";
    static const char file[] = "RETR CURRENT/ncbi.tar.gz";
    const char* env = getenv("CONN_DEBUG_PRINTOUT");
    STimeout    timeout;
    CONNECTOR   connector;
    FILE*       data_file;
    CONN        conn;
    ESwitch     log = eDefault;
    size_t      n;
    char        buf[1024];
    EIO_Status  status;

    /* Log and data-log streams */
    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);
    CORE_SetLOGFILE(stderr, 0/*false*/);
    data_file = fopen("test_ncbi_ftp_connector.log", "wb");
    assert(data_file);

    timeout.sec = 3;
    timeout.usec = 0;

    if (env  &&  (strcasecmp(env, "1")    == 0  ||
                  strcasecmp(env, "TRUE") == 0  ||
                  strcasecmp(env, "SOME") == 0  ||
                  strcasecmp(env, "DATA") == 0)) {
        log = eOn;
    }
    if (env  &&  (strcasecmp(env, "0")     == 0  ||
                  strcasecmp(env, "NONE")  == 0  ||
                  strcasecmp(env, "FALSE") == 0)) {
        log = eOff;
    }

    /* Run the tests */
    connector = FTP_CreateDownloadConnector(TEST_HOST, TEST_PORT,
                                            TEST_USER, TEST_PASS,
                                            TEST_PATH, log);

    if (CONN_Create(connector, &conn) != eIO_Success)
        CORE_LOG(eLOG_Fatal, "Cannot create FTP download connection");

    if (CONN_Read(conn, buf, sizeof(buf), &n, eIO_ReadPlain) != eIO_Closed)
        CORE_LOG(eLOG_Fatal, "Test failed in empty READ");

    if (CONN_Write(conn, "aaa", 3, &n, eIO_WritePlain) != eIO_Success)
        CORE_LOG(eLOG_Fatal, "Cannot write FTP command");

    if (CONN_Wait(conn, eIO_Read, &timeout) != eIO_Unknown)
        CORE_LOG(eLOG_Fatal, "Test failed in waiting on READ");

    if (CONN_Write(conn, "LIST", 4, &n, eIO_WritePlain) != eIO_Success)
        CORE_LOG(eLOG_Fatal, "Cannot write LIST command");

    do {
        status = CONN_Read(conn, buf, sizeof(buf), &n, eIO_ReadPlain);
        if (n != 0)
            printf("%.*s", (int) n, buf);
    } while (status == eIO_Success);

    if (CONN_Write(conn, chdir, sizeof(chdir) - 1, &n, eIO_WritePlain)
        != eIO_Success) {
        CORE_LOGF(eLOG_Fatal, ("Cannot execute %.*s",
                               (int)sizeof(chdir) - 2, chdir));
    }

    if (CONN_Write(conn, file, sizeof(file) - 1, &n, eIO_WritePersist)
        != eIO_Success) {
        CORE_LOGF(eLOG_Fatal, ("Cannot write %s", file));
    }

     do {
        status = CONN_Read(conn, buf, sizeof(buf), &n, eIO_ReadPlain);
        if (n != 0)
            fwrite(buf, n, 1, data_file);
    } while (status == eIO_Success);
   
    if (CONN_Close(conn) != eIO_Success)
        CORE_LOG(eLOG_Fatal, "Error closing FTP connection");

    /* Cleanup and Exit */

    fclose(data_file);
    CORE_SetLOG(0);
    return 0;
}


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 1.1  2004/12/06 17:49:00  lavr
 * Initial revision
 *
 * ==========================================================================
 */
