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
 *   Standard test for the MEMORY CONNECTOR
 *
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.1  2002/02/20 19:14:40  lavr
 * Initial revision
 *
 * ==========================================================================
 */

#include "test_assert.h"

#include <connect/ncbi_connection.h>
#include <connect/ncbi_memory_connector.h>
#include <connect/ncbi_util.h>
#include <stdio.h>
#include <stdlib.h>


static const char* s_ProgramName;

static void Usage(const char* message)
{
    fprintf(stderr,
            "\nUsage: %s <input_file>\n"
            "\nERROR:  %s!\n",
            s_ProgramName, message);
    abort();
}


int main(int argc, const char* argv[])
{
    CONNECTOR  connector;
    EIO_Status status;
    CONN       conn;
    FILE*      fp;

    /* cmd.-line args */
    s_ProgramName = argv[0];
    if (argc != 2) {
        Usage("Input file name required");
    }

    if (!(fp = fopen(argv[1], "r"))) {
        Usage("Input file must exist and be readable");
    }

    /* log and data log streams */
    CORE_SetLOGFILE(stderr, 0/*false*/);
    fprintf(stderr, "Starting the MEMORY CONNECTOR test...\n");

    /* create connector, and bind it to the connection */
    connector = MEMORY_CreateConnector(0);
    if ( !connector ) {
        Usage("Failed to create MEMORY connector");
    }

    verify(CONN_Create(connector, &conn) == eIO_Success);
 
    /* pump the data from file thru connection and compare */
    while (!feof(fp)) {
        char buf[1000];
        char fub[sizeof(buf)];
        size_t n_read, n_written;

        /* read from the source */
        if (!(n_read = fread(buf, 1, sizeof(buf), fp))) {
            if (!feof(fp))
                fprintf(stderr, "File read error\n");
            break;
        }

        /* write to the connection */
        status = CONN_Write(conn, buf, n_read, &n_written);
        if (status != eIO_Success || n_written != n_read) {
            fprintf(stderr, "CONN_Write() failed (status: %s)\n",
                    IO_StatusStr(status));
            return 1;
        }

        /* read back from the connection */
        status = CONN_Read(conn, fub, sizeof(fub), &n_read, eIO_Plain);
        if (status != eIO_Success || n_read != n_written) {
            fprintf(stderr, "CONN_Write() failed (status: %s)\n",
                    IO_StatusStr(status));
            return 1;
        }

        /* compare */
        if (memcmp(buf, fub, n_read) != 0) {
            fprintf(stderr, "MISMATCH");
            return 1;
        }

        fprintf(stderr, "PROCESSED: %ld  bytes\n", (long) n_read);
    }
    
    /* cleanup, exit */
    fclose(fp);
    verify(CONN_Close(conn) == eIO_Success);
    CORE_SetLOG(0);
    return 0;
}
