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
 *   Hit an arbitrary URL using HTTP-based CONNECTOR
 *
 */

#include "../ncbi_ansi_ext.h"
#include "../ncbi_assert.h"
#include <connect/ncbi_http_connector.h>
#include <connect/ncbi_util.h>
/* This header must go last */
#include "test_assert.h"


/* Holder for the cmd.-line arg values describing the URL to hit
 * (used by the pseudo-registry getter "s_REG_Get")
 */
static struct {
    const char* host;
    const char* port;
    const char* path;
    const char* args;
} s_Args;


/* Getter for the pseudo-registry
 */
#if defined(__cplusplus)
extern "C" {
    static void s_REG_Get(void*user_data,
                          const char* section, const char* name,
                          char* value, size_t value_size);
}
#endif /* __cplusplus */

static void s_REG_Get
(void*       user_data,
 const char* section,
 const char* name,
 char*       value,
 size_t      value_size)
{
    if (strcmp(section, DEF_CONN_REG_SECTION) != 0) {
        assert(0);
        return;
    }

#define X_GET_VALUE(x_name, x_value)              \
    if (strcmp(name, x_name) == 0) {              \
        strncpy0(value, x_value, value_size - 1); \
        return;                                   \
    }

    X_GET_VALUE(REG_CONN_HOST,           s_Args.host);
    X_GET_VALUE(REG_CONN_PORT,           s_Args.port);
    X_GET_VALUE(REG_CONN_PATH,           s_Args.path);
    X_GET_VALUE(REG_CONN_ARGS,           s_Args.args);
    X_GET_VALUE(REG_CONN_DEBUG_PRINTOUT, "yes");
}



/*****************************************************************************
 *  MAIN
 */

int main(int argc, const char* argv[])
{
    const char* inp_file    = (argc > 5) ? argv[5] : "";
    const char* user_header = (argc > 6) ? argv[6] : "";

    CONNECTOR   connector;
    CONN        conn;
    EIO_Status  status;

    char   buffer[100];
    size_t n_read, n_written;


    /* Prepare to connect:  parse and check cmd.-line args, etc. */
    s_Args.host         = (argc > 1) ? argv[1] : "";
    s_Args.port         = (argc > 2) ? argv[2] : "";
    s_Args.path         = (argc > 3) ? argv[3] : "";
    s_Args.args         = (argc > 4) ? argv[4] : "";

    fprintf(stderr, "Running '%s':\n"
            "  URL host:        '%s'\n"
            "  URL port:        '%s'\n"
            "  URL path:        '%s'\n"
            "  URL args:        '%s'\n"
            "  Input data file: '%s'\n"
            "  User header:     '%s'\n"
            " Reply(if any) from the hit URL goes to the standard output.\n\n",
            argv[0],
            s_Args.host, s_Args.port, s_Args.path, s_Args.args,
            inp_file, user_header);

    /* Log stream */
    CORE_SetLOGFILE(stderr, 0/*false*/);

    /* Tune to the test URL using hard-coded pseudo-registry */
    CORE_SetREG( REG_Create(0, s_REG_Get, 0, 0, 0) );

    /* Usage */
    if (argc < 4) {
        fprintf(stderr,
                "Usage:   %s host port path [args] [inp_file] [user_header]\n"
                "Example: %s www.ncbi.nlm.nih.gov 80 "
                "/Service/bounce.cgi 'arg1+arg2+arg3'\n",
                argv[0], argv[0]);
        fprintf(stderr, "Too few arguments.\n");
        return 1;
    }

    /* Connect */
    connector = HTTP_CreateConnector(0, user_header, 0);
    assert(connector);
    verify(CONN_Create(connector, &conn) == eIO_Success);

    /* If the input file is specified,
     * then send its content (as the HTTP request body) to URL
     */
    if ( *inp_file ) {
        FILE* inp_fp = fopen(inp_file, "rb");
        if ( !inp_fp ) {
            fprintf(stderr, "Cannot open file '%s' for read", inp_file);
            assert(0);
        }

        for (;;) {
            n_read = fread(buffer, 1, sizeof(buffer), inp_fp);
            if (n_read <= 0) {
                assert(feof(inp_fp));
                break; /* EOF */
            }

            status = CONN_Write(conn, buffer, n_read,
                                &n_written, eIO_WritePersist);
            if (status != eIO_Success) {
                fprintf(stderr, "Error writing to URL (%s)",
                        IO_StatusStr(status));
                assert(0);
            }
            assert(n_written == n_read);
        }
        fclose(inp_fp);
    }

    /* Read reply from connection, write it to standard output */
    fprintf(stdout, "\n\n----- [BEGIN] HTTP Content -----\n");
    for (;;) {
        status = CONN_Read(conn,buffer,sizeof(buffer),&n_read,eIO_ReadPlain);
        if (status != eIO_Success)
            break;

        fwrite(buffer, 1, n_read, stdout);
        fflush(stdout);
    }
    fprintf(stdout, "\n----- [END] HTTP Content -----\n\n");

    if (status != eIO_Closed) {
        fprintf(stderr, "Error reading from URL (%s)", IO_StatusStr(status));
        assert(0);
    }

    /* Success:  close the connection, cleanup, and exit */
    CONN_Close(conn);
    CORE_SetREG(0);
    CORE_SetLOG(0);
    return 0;
}


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.16  2005/04/20 18:23:11  lavr
 * +"../ncbi_assert.h"
 *
 * Revision 6.15  2004/11/23 15:04:26  lavr
 * Use public bounce.cgi from "www"
 *
 * Revision 6.14  2004/11/22 20:24:53  lavr
 * "yar" replaced with "graceland"
 *
 * Revision 6.13  2004/04/01 14:14:02  lavr
 * Spell "occurred", "occurrence", and "occurring"
 *
 * Revision 6.12  2004/02/23 15:23:42  lavr
 * New (last) parameter "how" added in CONN_Write() API call
 *
 * Revision 6.11  2003/04/15 14:06:09  lavr
 * Changed ray.nlm.nih.gov -> ray.ncbi.nlm.nih.gov
 *
 * Revision 6.10  2002/11/22 15:09:40  lavr
 * Replace all occurrences of "ray" with "yar"
 *
 * Revision 6.9  2002/10/28 15:47:12  lavr
 * Use "ncbi_ansi_ext.h" privately and use strncpy0()
 *
 * Revision 6.8  2002/08/07 16:38:08  lavr
 * EIO_ReadMethod enums changed accordingly; log moved to end
 *
 * Revision 6.7  2002/03/22 19:45:55  lavr
 * Test_assert.h made last among the include files
 *
 * Revision 6.6  2002/01/16 21:23:14  vakatov
 * Utilize header "test_assert.h" to switch on ASSERTs in the Release mode too
 *
 * Revision 6.5  2001/01/11 16:42:45  lavr
 * Registry Get/Set methods got the 'user_data' argument, forgotten earlier
 *
 * Revision 6.4  2000/11/15 17:27:29  vakatov
 * Fixed path to the test CGI application.
 *
 * Revision 6.3  2000/09/27 16:00:24  lavr
 * Registry entries adjusted
 *
 * Revision 6.2  2000/05/30 23:24:40  vakatov
 * Cosmetic fix for the C++ compilation
 *
 * Revision 6.1  2000/04/21 19:56:28  vakatov
 * Initial revision
 *
 * ==========================================================================
 */
