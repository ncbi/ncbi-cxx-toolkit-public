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
 *   Standard test for the FILE-based CONNECTOR
 *
 */

#include "../ncbi_assert.h"
#include <connect/ncbi_connection.h>
#include <connect/ncbi_file_connector.h>
#include <connect/ncbi_util.h>
#include <stdlib.h>
/* This header must go last */
#include "test_assert.h"


#define OUT_FILE "test_ncbi_file_connector.out"

static const char* s_ProgramName;

static void Usage(const char* message)
{
    fprintf(stderr,
            "\nUsage: %s <input_file>\n"
            "  (copy <input_file> to \"" OUT_FILE "\")\n"
            "\nERROR:  %s!\n",
            s_ProgramName, message);
    abort();
}


int main(int argc, const char* argv[])
{
    CONN        conn;
    CONNECTOR   connector;
    EIO_Status  status;
    const char* inp_file;

    /* cmd.-line args */
    s_ProgramName = argv[0];
    if (argc != 2) {
        Usage("Must specify the input file name");
    }
    inp_file = argv[1];

    /* log and data log streams */
    CORE_SetLOGFILE(stderr, 0/*false*/);

    /* run the test */
    fprintf(stderr,
            "Starting the FILE CONNECTOR test...\n"
            "Copy data from file \"%s\" to file \"%s\".\n\n",
            inp_file, OUT_FILE);

    /* create connector, and bind it to the connection */
    connector = FILE_CreateConnector(inp_file, OUT_FILE);
    if ( !connector ) {
        Usage("Failed to create FILE connector");
    }

    verify(CONN_Create(connector, &conn) == eIO_Success);
 
    /* pump the data from one file to another */
    for (;;) {
        char buf[100];
        size_t n_read, n_written;

        /* read */
        status = CONN_Read(conn, buf, sizeof(buf), &n_read, eIO_ReadPlain);
        if (status != eIO_Success) {
            fprintf(stderr, "CONN_Read() failed (status: %s)\n",
                    IO_StatusStr(status));
            break;
        }
        fprintf(stderr, "READ: %ld  bytes\n", (long) n_read);

        /* write */
        status = CONN_Write(conn, buf, n_read, &n_written, eIO_WritePersist);
        if (status != eIO_Success) {
            fprintf(stderr, "CONN_Write() failed (status: %s)\n",
                    IO_StatusStr(status));
            assert(0);
            break;
        }
        assert(n_written == n_read);
    }
    assert(status == eIO_Closed);
    
    /* cleanup, exit */
    verify(CONN_Close(conn) == eIO_Success);
    CORE_SetLOG(0);
    return 0;
}


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.6  2005/04/20 18:23:11  lavr
 * +"../ncbi_assert.h"
 *
 * Revision 6.5  2004/02/23 15:23:43  lavr
 * New (last) parameter "how" added in CONN_Write() API call
 *
 * Revision 6.4  2002/08/07 16:38:08  lavr
 * EIO_ReadMethod enums changed accordingly; log moved to end
 *
 * Revision 6.3  2002/03/22 19:47:09  lavr
 * Test_assert.h made last among the include files
 *
 * Revision 6.2  2002/01/16 21:23:15  vakatov
 * Utilize header "test_assert.h" to switch on ASSERTs in the Release mode too
 *
 * Revision 6.1  2000/04/12 15:22:43  vakatov
 * Initial revision
 *
 * ==========================================================================
 */
