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
 *   Read data from a file, and send these data line-by-line to a socket.
 *   Then, read data from the socket.
 *   Trace all I/O going through the socket in both directions.
 *
 */

#include <connect/ncbi_socket.h>
#include "../ncbi_priv.h"               /* CORE logging facilities */
#include <stdlib.h>
#include <string.h>
/* This header must go last */
#include "test_assert.h"


static void s_SendData(FILE* fp, SOCK sock)
{
    char   buf[2048];
    size_t n_written;

    while ( fgets(buf, sizeof(buf)-3, fp) ) {
        size_t len = strlen(buf);
        assert(buf[len - 1] == '\n');
        assert(buf[len]     == '\0');
        assert(!feof(fp)  &&  !ferror(fp));

        len--;
        if (buf[len-1] == '\\') {
            len--;
        } else {
            buf[len++] = '\r';
            buf[len++] = '\n';
        }

        assert(SOCK_Write(sock, buf, len, &n_written, eIO_WritePersist) ==
               eIO_Success);
    }
    assert(feof(fp));
}


static void s_ReadData(SOCK sock)
{
    char   buf[512];
    size_t n_read;

    while (SOCK_Read(sock, buf, sizeof(buf), &n_read, eIO_ReadPlain) ==
           eIO_Success) {
        continue;
    }
    assert(SOCK_Status(sock, eIO_Read) == eIO_Closed);
}



/* Main function
 */
extern int main(int argc, char** argv)
{
    EIO_Status status;
    SOCK  sock;
    int   port;
    FILE* fp;

    /* Cmd.-line args */
    if (argc != 4  ||
        (fp = fopen(argv[1], "r")) == 0  ||
        (port = atoi(argv[3])) <= 0) {
        perror("\nUSAGE:  test_fw <inp_file> <host> <port>\n");
        return 1;
    }

    /* Error logging */
    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);
    {{
        FILE* log_fp = fopen("test_fw.log", "a");
        if ( !fp ) {
            perror("Failed to open \"test_fw.log\" for writing");
            return 2;
        }
        CORE_SetLOGFILE(log_fp, 1/*true*/);
    }}
    SOCK_SetDataLoggingAPI(eOn);

    /* Connect to Web server */
    status = SOCK_Create(argv[2], (unsigned short) port, 0, &sock);
    assert(status == eIO_Success);

    /* Send data from inp.file to the socket */
    s_SendData(fp, sock);
    fclose(fp);

    /* Read up response from the socket */
    s_ReadData(sock);

    /* Close and exit */
    status = SOCK_Close(sock);
    assert(status == eIO_Success  ||  status == eIO_Closed);

    assert(SOCK_ShutdownAPI() == eIO_Success);

    CORE_LOG(eLOG_Note, "TEST completed successfully");
    CORE_SetLOG(0);
    return 0;
}
