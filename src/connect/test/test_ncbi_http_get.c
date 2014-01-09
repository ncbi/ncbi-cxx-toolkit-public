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
 *   Retrieve a Web document via HTTP
 *
 */

#include <connect/ncbi_base64.h>
#include <connect/ncbi_gnutls.h>
#include <connect/ncbi_http_connector.h>
#include <connect/ncbi_util.h>
#include "../ncbi_ansi_ext.h"
#include "../ncbi_priv.h"               /* CORE logging facilities */
#include <ctype.h>
#include <errno.h>
#ifdef NCBI_OS_MSWIN
#  include <io.h>      /* for access() */
#  define   access  _access
#  ifndef   R_OK
#    define R_OK    4  /* per MSDN     */
#  endif /*!R_OK*/
#endif /*NCBI_OS_MSWIN*/
#include <stdlib.h>
#include <time.h>
#ifdef NCBI_OS_UNIX
#  include <unistd.h>  /* for access() and maybe usleep() */
#endif /*NCBI_OS_UNIX*/
#ifdef HAVE_LIBGNUTLS
#  include <gnutls/gnutls.h>
#  define GNUTLS_PKCS12_TYPE  GNUTLS_X509_FMT_PEM/*or _DER*/
#  define GNUTLS_PKCS12_FILE  "TEST_NCBI_HTTP_GET_CERT"
#  define GNUTLS_PKCS12_PASS  "TEST_NCBI_HTTP_GET_PASS"
#endif /*HAVE_LIBGNUTLS*/

#include "test_assert.h"  /* This header must go last */


static const char* x_GetPkcs12Pass(const char* val, char* buf, size_t bufsize)
{
    size_t in, out, len, half = bufsize >> 1;
    if (!(val = ConnNetInfo_GetValue(0, val, buf, half - 1, 0))  ||  !*val)
        return "";
    buf += half;
    len  = strlen(val);
    if (!BASE64_Decode(val, len, &in, buf, half - 1, &out))
        return "";
    if (in != len)
        return "";
    assert(out  &&  out < half);
    if ((buf[out - 1] == '\n'  &&  !--out)  ||
        (buf[out - 1] == '\r'  &&  !--out)) {
        return "";
    }
    for (in = 0;  in < out;  ++in) {
        if (!buf[in]  ||  !isprint((unsigned char) buf[in]))
            return "";
    }
    buf[in] = '\0';
    return buf;
}


int main(int argc, char* argv[])
{
#ifdef HAVE_LIBGNUTLS
    gnutls_certificate_credentials_t xcred = 0;
#endif /*HAVE_LIBGNUTLS*/
    CONNECTOR     connector;
    SConnNetInfo* net_info;
    char          blk[512];
    EIO_Status    status;
    THTTP_Flags   flags;
    NCBI_CRED     cred;
    CONN          conn;
    FILE*         fp;
    time_t        t;
    size_t        n;

    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);
    CORE_SetLOGFILE(stderr, 0/*false*/);

    if (argc < 2  ||  !*argv[1])
        CORE_LOG(eLOG_Fatal, "URL has to be supplied on the command line");
    if (argc > 3)
        CORE_LOG(eLOG_Fatal, "Cannot take more than 2 arguments");
    if (argc == 3
        &&  !(fp = strcmp(argv[2], "-") == 0 ? stdin : fopen(argv[2], "rb"))) {
        CORE_LOGF_ERRNO(eLOG_Error, errno, ("Cannot open \"%s\"", argv[2]));
    } else
        fp = 0;

    ConnNetInfo_GetValue(0, "RECONNECT", blk, 32, "");
    if (ConnNetInfo_Boolean(blk)) {
        CORE_LOG(eLOG_Note, "Reconnect mode acknowledged");
        flags = fHTTP_AutoReconnect;
    } else
        flags = 0;

    CORE_LOG(eLOG_Note, "Creating network info structure");
    if (!(net_info = ConnNetInfo_Create(0)))
        CORE_LOG(eLOG_Fatal, "Cannot create network info structure");
    assert(!net_info->credentials);

    CORE_LOGF(eLOG_Note, ("Parsing URL \"%s\"", argv[1]));
    if (!ConnNetInfo_ParseURL(net_info, argv[1]))
        CORE_LOG(eLOG_Fatal, "Cannot parse URL");

    cred = 0;
    ConnNetInfo_GetValue(0, "USESSL", blk, 32, "");
    if (net_info->scheme == eURL_Https  &&  ConnNetInfo_Boolean(blk)) {
#ifdef HAVE_LIBGNUTLS
        const char* file;
        const char* pass;
        status = SOCK_SetupSSLEx(NcbiSetupGnuTls);
        CORE_LOGF(eLOG_Note, ("SSL request acknowledged: %s",
                              IO_StatusStr(status)));
        pass = x_GetPkcs12Pass(GNUTLS_PKCS12_PASS, blk, sizeof(blk));
        file = ConnNetInfo_GetValue(0, GNUTLS_PKCS12_FILE,
                                    blk, sizeof(blk)/2 - 1, 0);
        if (file  &&  access(file, R_OK) == 0) {
            int err;
            if ((err = gnutls_certificate_allocate_credentials(&xcred)) != 0 ||
                (err = gnutls_certificate_set_x509_simple_pkcs12_file
                 (xcred, file, GNUTLS_PKCS12_TYPE, pass)) != 0) {
                CORE_LOGF(eLOG_Fatal,
                          ("Cannot load PKCS#12 credentials from \"%s\": %s",
                           file, gnutls_strerror(err)));
            } else if ((cred = NcbiCredGnuTls(xcred)) != 0) {
                net_info->credentials = cred;
                CORE_LOGF(eLOG_Note, ("PKCS#12 credentials loaded from \"%s\"",
                                      file));
            } else
                CORE_LOG_ERRNO(eLOG_Fatal, errno, "Cannot create NCBI_CRED");
        }
#else
        CORE_LOG(eLOG_Warning, "SSL requested but may be not supported");
#endif /*HAVE_LIBGNUTLS*/
    }

    CORE_LOGF(eLOG_Note, ("Creating HTTP%s connector",
                          &"S"[net_info->scheme != eURL_Https]));
    if (!(connector = HTTP_CreateConnector(net_info, 0, flags)))
        CORE_LOG(eLOG_Fatal, "Cannot create HTTP connector");
    /* Could have destroyed net_info at this point here if we did not use the
     * timeout off of it below, so at least unlink the credentials, if any...
     */
    net_info->credentials = 0;

    CORE_LOG(eLOG_Note, "Creating connection");
    if (CONN_Create(connector, &conn) != eIO_Success)
        CORE_LOG(eLOG_Fatal, "Cannot create connection");
    CONN_SetTimeout(conn, eIO_Open,      net_info->timeout);
    CONN_SetTimeout(conn, eIO_ReadWrite, net_info->timeout);
    while (fp  &&  !feof(fp)) {
        n = fread(blk, 1, sizeof(blk), fp);
        status = CONN_Write(conn, blk, n, &n, eIO_WritePersist);
        if (status != eIO_Success) {
            CORE_LOGF(eLOG_Fatal, ("Write error: %s", IO_StatusStr(status)));
        }
    }
    if (fp)
        fclose(fp);

    t = time(0);
    do {
        status = CONN_Wait(conn, eIO_Read, net_info->timeout);
        if (status == eIO_Timeout) {
            if  ((net_info->timeout  &&
                  (net_info->timeout->sec | net_info->timeout->usec))
                 ||  (unsigned long)(time(0) - t) > DEF_CONN_TIMEOUT) {
                CORE_LOG(eLOG_Fatal, "Timed out");
            }
#if defined(NCBI_OS_UNIX)  &&  defined(HAVE_USLEEP)
            usleep(500);
#endif /*NCBI_OS_UNIX && HAVE_USLEEP*/
            continue;
        }
        if (status == eIO_Success)
            status  = CONN_ReadLine(conn, blk, sizeof(blk), &n);
        else
            n = 0;
        if (n) {
            connector = 0/*as bool, visited*/;
            fwrite(blk, 1, n, stdout);
            if (status != eIO_Timeout)
                fputc('\n', stdout);
            fflush(stdout);
            if (n == sizeof(blk)  &&  status != eIO_Closed)
                CORE_LOGF(eLOG_Warning, ("Line too long, continuing..."));
        }
        if (status == eIO_Timeout)
            continue;
        if (status != eIO_Success  &&  (status != eIO_Closed  ||  connector))
            CORE_LOGF(eLOG_Fatal, ("Read error: %s", IO_StatusStr(status)));

    } while (status == eIO_Success  ||  status == eIO_Timeout);

    ConnNetInfo_Destroy(net_info); /* done using the timeout field */

    CORE_LOG(eLOG_Note, "Closing connection");
    CONN_Close(conn);  /* this makes sure credentials are no longer accessed */

    if (cred)
        free(cred);
#ifdef HAVE_LIBGNUTLS
    if (xcred)
        gnutls_certificate_free_credentials(xcred);
#endif /*HAVE_LIBGNUTLS*/

    CORE_LOG(eLOG_Note, "Completed successfully");
    CORE_SetLOG(0);
    return 0;
}
