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
#  if LIBGNUTLS_VERSION_NUMBER >= 0x021000
#    include <gnutls/x509.h>
#  endif /*LIBGNUTLS_VERSION_NUMBER>=2.10.0*/
#  define GNUTLS_PKCS12_TYPE  "TEST_NCBI_HTTP_GET_TYPE"
#  define GNUTLS_PKCS12_FILE  "TEST_NCBI_HTTP_GET_CERT"
#  define GNUTLS_PKCS12_PASS  "TEST_NCBI_HTTP_GET_PASS"
#endif /*HAVE_LIBGNUTLS*/

#include "test_assert.h"  /* This header must go last */


#ifdef HAVE_LIBGNUTLS

#ifdef __GNUC__
inline
#endif /*__GNUC__*/
static int/*bool*/ x_IsPrintable(const char* buf, size_t size)
{
    size_t n;
    for (n = 0;  n < size;  ++n) {
        if (!buf[n]  ||  !isprint((unsigned char) buf[n]))
            return 0/*false*/;
    }
    return 1/*true*/;
}


/* Accept both plain and BASE-64 encoded, printable password */
static const char* x_GetPkcs12Pass(const char* val, char* buf, size_t bufsize)
{
    size_t in, out, len, half = bufsize >> 1;
    if (!(val = ConnNetInfo_GetValue(0, val, buf, half - 1, 0))  ||  !*val)
        return "";
    if (!x_IsPrintable(val, len = strlen(val)))
        return "";
    buf += half;
    if (BASE64_Decode(val, len, &in, buf, half - 1, &out)  &&  in == len) {
        assert(out  &&  out < half);
        if ((buf[out - 1] != '\n'  ||  --out)  &&
            (buf[out - 1] != '\r'  ||  --out)  &&
            x_IsPrintable(buf, out)) {
            buf[out] = '\0';
            return buf;
        }
    }
    return (const char*) memcpy(buf, val, ++len);
}


#if LIBGNUTLS_VERSION_NUMBER >= 0x021000
static int x_CertVfyCB(gnutls_session_t session)
{
	unsigned int n, cert_list_size = 0;
	const gnutls_datum_t* cert_list;
	
	cert_list = gnutls_certificate_get_peers(session, &cert_list_size);
	if (cert_list_size == 0) {
		CORE_LOG(eLOG_Error, "No certificates obtained from server");
		return 1;
	}
    assert(cert_list);

    CORE_LOGF(eLOG_Note,
              ("%u certificate%s received from server:",
               cert_list_size, &"s"[cert_list_size == 1]));
	for (n = 0;  n < cert_list_size;  ++n) {
        gnutls_x509_crt_t crt;
		gnutls_datum_t cinfo;
        int err;

		gnutls_x509_crt_init(&crt);
        err = gnutls_x509_crt_import(crt, &cert_list[n],
                                     GNUTLS_X509_FMT_DER);
		if (!err) {
            err = gnutls_x509_crt_print(crt, GNUTLS_CRT_PRINT_ONELINE, &cinfo);
            if (!err) {
                CORE_LOGF(eLOG_Note,
                          ("Certificate[%u]: %s", n + 1, cinfo.data));
                /*gnutls_*/free(cinfo.data);
            }
        }
        if (err) {
            CORE_LOGF(eLOG_Error,
                      ("Certificate[%u] error: %s",
                       n + 1, gnutls_strerror(err)));
        }
        gnutls_x509_crt_deinit(crt);
	}
    return 0;
}
#endif /*LIBGNUTLS_VERSION_NUMBER>=2.10.0*/


#if 0
static int x_CertRtrCB(gnutls_session_t session,
                       const gnutls_datum_t* req_ca_dn,    int n_req_ca_dn,
                       const gnutls_pk_algorithm_t* algos, int n_algos,
                       gnutls_pcert_st** pcert,  unsigned int* n_pcert,
                       gnutls_privkey_t* pkey)
{
    gnutls_certificate_type_t type = gnutls_certificate_type_get(session);
    const char* str;
    char buf[256];

    /* The very fact of receiving this callback is that
     * gnutls_certificate_client_get_request_status(session)
     * would return non-zero (i.e. certificate is required).
     */
    switch (type) {
    case GNUTLS_CRT_X509:
        str = "X.509";
        break;
    case GNUTLS_CRT_OPENPGP:
        str = "Open PGP";
        break;
    case GNUTLS_CRT_RAW:
        str = "Raw public key";
        break;
    default:
        sprintf(buf, "%u", (int) type);
        str = buf;
        break;
    }
    CORE_LOGF(eLOG_Note,
              ("Server requires %s certificate for client authentication",
               str));

    if (n_req_ca_dn > 0) {
        int n;
        CORE_LOGF(eLOG_Note,
                  ("Server's %d trusted certificate authorit%s:",
                   n_req_ca_dn, n_req_ca_dn == 1 ? "y" : "ies"));
        for (n = 0;  n < n_req_ca_dn;  ++n) {
            size_t len = sizeof(buf);
            int    err = gnutls_x509_rdn_get(&req_ca_dn[n], buf, &len);
            if (err >= 0) {
                CORE_LOGF(eLOG_Note,
                          ("CA[%d]: %s", n + 1, buf));
            } else {
                CORE_LOGF(eLOG_Error,
                          ("CA[%d]: requires %lu bytes: %s",
                           n + 1, (unsigned long) len, gnutls_strerror(err)));
            }
        }
    } else {
        CORE_LOG(eLOG_Note,
                 "Server does not specify any trusted CAs");
    }

    if (n_algos) {
        int n;
        CORE_LOGF(eLOG_Note,
                  ("Server supports %d signature algorithm%s:",
                   n_algos, &"s"[n_algos == 1]));
        for (n = 0;  n < n_algos;  ++n) {
            str = gnutls_pk_algorithm_get_name(algos[n]);
            CORE_LOGF(eLOG_Note,
                      ("PK algorithm[%d]: %s(%d)",
                       n + 1, !str ? "<NULL>" : *str ? str : "\"\"",
                       (int) algos[n]));
        }
    }

    if (type == GNUTLS_CRT_X509) {
        *pcert   = &pcrt;
        *n_pcert = 1;
        *pkey    = key;
    } else*/ {
        *pcert   = 0;
        *n_pcert = 0;
        *pkey    = 0;
    }
    return 0;
}
#endif/*0*/


#endif /*HAVE_LIBGNUTLS*/


int main(int argc, char* argv[])
{
#ifdef HAVE_LIBGNUTLS
    gnutls_certificate_credentials_t xcred = 0;
#endif /*HAVE_LIBGNUTLS*/
    CONNECTOR     connector;
    SConnNetInfo* net_info;
    char          blk[250];
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
    if (argc < 3)
        fp = 0;
    else if (!(fp = strcmp(argv[2], "-") == 0 ? stdin : fopen(argv[2], "rb")))
        CORE_LOGF_ERRNO(eLOG_Error, errno, ("Cannot open \"%s\"", argv[2]));

    ConnNetInfo_GetValue(0, "RECONNECT", blk, 32, 0);
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

    ConnNetInfo_GetValue(0, "HTTP11", blk, 32, 0);
    if (ConnNetInfo_Boolean(blk))
        net_info->version = 1;

    cred = 0;
    ConnNetInfo_GetValue(0, "USESSL", blk, 32, 0);
    if (net_info->scheme == eURL_Https  &&  ConnNetInfo_Boolean(blk)) {
#ifdef HAVE_LIBGNUTLS
        int err;
        char type[40];
        const char* file, *pass;
        status = SOCK_SetupSSLEx(NcbiSetupGnuTls);
        CORE_LOGF(eLOG_Note, ("SSL request acknowledged: %s",
                              IO_StatusStr(status)));
        if (!ConnNetInfo_GetValue(0, GNUTLS_PKCS12_TYPE, type, sizeof(type), 0)
            ||  !*type) {
            strncpy0(type, "PEM", sizeof(type));
        }
        pass = x_GetPkcs12Pass(GNUTLS_PKCS12_PASS, blk, sizeof(blk));
        file = ConnNetInfo_GetValue(0, GNUTLS_PKCS12_FILE,
                                    blk, sizeof(blk)/2 - 1, 0);
        if (file  &&  *file  &&  access(file, R_OK) == 0) {
            if ((err = gnutls_certificate_allocate_credentials(&xcred)) != 0 ||
                (err = gnutls_certificate_set_x509_simple_pkcs12_file
                 (xcred, file, strcasecmp(type, "PEM") == 0
                  ? GNUTLS_X509_FMT_PEM
                  : GNUTLS_X509_FMT_DER, pass)) != 0) {
                CORE_LOGF(eLOG_Fatal,
                          ("Cannot load PKCS#12 %s credentials from"
                           " \"%s\": %s", type, file, gnutls_strerror(err)));
            }
        } else if (net_info->debug_printout == eDebugPrintout_Data) {
            /* We don't have to create empty cert credentials, in general
             * (as the ncbi_gnutls shim does that for us); but if we want
             * callbacks, then they can only be associated with the
             * credentials handle (gnutls design???) so here it goes: */
            if ((err = gnutls_certificate_allocate_credentials(&xcred)) != 0) {
                CORE_LOGF(eLOG_Critical,
                          ("Cannot allocate certificate credentials: %s",
                           gnutls_strerror(err)));
                xcred = 0;
            }
            file = 0;
        } else
            file = 0;
        if (xcred) {
            if (!(cred = NcbiCredGnuTls(xcred)))
                CORE_LOG_ERRNO(eLOG_Fatal, errno, "Cannot create NCBI_CRED");
            net_info->credentials = cred;
            if (!file)
                CORE_LOG(eLOG_Note, "Debug certificate credentials set");
            else {
                CORE_LOGF(eLOG_Note, ("PKCS#12 %s credentials loaded from"
                                      " \"%s\"", type, file));
            }
            if (net_info->debug_printout == eDebugPrintout_Data) {
#if LIBGNUTLS_VERSION_NUMBER >= 0x021000
                gnutls_certificate_set_verify_function(xcred,
                                                       x_CertVfyCB);
#endif /*LIBGNUTLS_VERSION_NUMBER>=2.10.0*/
#if 0
                gnutls_certificate_set_retrieve_function2(xcred,
                                                          x_CertRtrCB);
#endif
            }
        }
#else
        CORE_LOG(eLOG_Warning, "SSL requested but may not be supported");
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
        static char block[20000/*to force non-default BUF allocations*/];
        n = fread(block, 1, sizeof(block), fp);
        status = CONN_Write(conn, block, n, &n, eIO_WritePersist);
        if (status != eIO_Success)
            CORE_LOGF(eLOG_Fatal, ("Write error: %s", IO_StatusStr(status)));
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
        if (n  ||  status == eIO_Success) {
            connector = 0/*as bool, visited*/;
            if (n)
                fwrite(blk, 1, n, stdout);
            if (status != eIO_Timeout)
                fputc('\n', stdout);
            if (status != eIO_Timeout  ||  n)
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
