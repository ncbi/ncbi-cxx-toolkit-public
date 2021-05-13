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
 *   MBEDTLS support for SSL in connection library
 *
 */

#include "ncbi_ansi_ext.h"
#include "ncbi_connssl.h"
#include "ncbi_priv.h"
#include "ncbi_servicep.h"
#include <connect/ncbi_mbedtls.h>
#include <connect/ncbi_tls.h>
#include <stdlib.h>

#define NCBI_USE_ERRCODE_X   Connect_TLS


#if defined(HAVE_LIBMBEDTLS)  ||  defined(NCBI_CXX_TOOLKIT)

#  ifdef HAVE_LIBMBEDTLS  /* external */
#    include <mbedtls/ctr_drbg.h>
#    include <mbedtls/debug.h>
#    include <mbedtls/entropy.h>
#    include <mbedtls/error.h>
#    include <mbedtls/pk.h>
#    include <mbedtls/net_sockets.h>
#    include <mbedtls/ssl.h>
#    include <mbedtls/threading.h>
#    include <mbedtls/version.h>
#  else                   /* embedded */
#    include "mbedtls/mbedtls/ctr_drbg.h"
#    include "mbedtls/mbedtls/debug.h"
#    include "mbedtls/mbedtls/entropy.h"
#    include "mbedtls/mbedtls/error.h"
#    include "mbedtls/mbedtls/pk.h"
#    include "mbedtls/mbedtls/net_sockets.h"
#    include "mbedtls/mbedtls/ssl.h"
#    include "mbedtls/mbedtls/threading.h"
#    include "mbedtls/mbedtls/version.h"
#  endif /*HAVE_LIBMBEDTLS*/

#  if   defined(ENOTSUP)
#    define NCBI_NOTSUPPORTED  ENOTSUP
#  elif defined(ENOSYS)
#    define NCBI_NOTSUPPORTED  ENOSYS
#  else
#    define NCBI_NOTSUPPORTED  EINVAL
#  endif

#else

#  define mbedtls_x509_crt    void
#  define mbedtls_pk_context  void

#endif /*HAVE_LIBMBEDTLS || NCBI_CXX_TOOLKIT*/


struct SNcbiMbedTlsCred {
    mbedtls_x509_crt*   cert;
    mbedtls_pk_context* pkey;
};


#if defined(HAVE_LIBMBEDTLS)  ||  defined(NCBI_CXX_TOOLKIT)

#  if defined(MBEDTLS_THREADING_ALT)  &&  defined(NCBI_THREADS)
#    ifdef MBEDTLS_THREADING_PTHREAD
#      error "MBEDTLS_THREADING_ALT and MBEDTLS_THREADING_PTHREAD conflict"
#    endif /*MBEDTLS_THREADING_PTHREAD*/
static void mbtls_user_mutex_init(MT_LOCK* lock)
{
    *lock = MT_LOCK_AddRef(g_CORE_MT_Lock);
}
static void mbtls_user_mutex_deinit(MT_LOCK* lock)
{
    if (*lock) {
        g_CORE_MT_Lock = MT_LOCK_Delete(*lock);
        assert(g_CORE_MT_Lock);
        *lock = 0;
    } else
        CORE_LOG_X(50, eLOG_Warning, "NULL MT_LOCK deinit in MBEDTLS");
}
static int mbtls_user_mutex_lock(MT_LOCK* lock)
{
    if (lock) {
        switch (MT_LOCK_Do(*lock, eMT_Lock)) {
        case -1:
            return MBEDTLS_ERR_THREADING_FEATURE_UNAVAILABLE;
        case  0:
            return MBEDTLS_ERR_THREADING_MUTEX_ERROR;
        case  1:
            return 0;
        default:
            break;
        }
    }
    return MBEDTLS_ERR_THREADING_BAD_INPUT_DATA;
}
static int mbtls_user_mutex_unlock(MT_LOCK* lock)
{
    if (lock) {
        switch (MT_LOCK_Do(*lock, eMT_Unlock)) {
        case -1:
            return MBEDTLS_ERR_THREADING_FEATURE_UNAVAILABLE;
        case  0:
            return MBEDTLS_ERR_THREADING_MUTEX_ERROR;
        case  1:
            return 0;
        default:
            break;
        }
    }
    return MBEDTLS_ERR_THREADING_BAD_INPUT_DATA;
}
#  endif /*MBEDTLS_THREADING_ALT && NCBI_THREADS*/

#  ifdef __cplusplus
extern "C" {
#  endif /*__cplusplus*/

static EIO_Status  s_MbedTlsInit  (FSSLPull pull, FSSLPush push);
static void*       s_MbedTlsCreate(ESOCK_Side side, SNcbiSSLctx* ctx,
                                   int* error);
static EIO_Status  s_MbedTlsOpen  (void* session, int* error, char** desc);
static EIO_Status  s_MbedTlsRead  (void* session,       void* buf,
                                   size_t size, size_t* done, int* error);
static EIO_Status  s_MbedTlsWrite (void* session, const void* data,
                                   size_t size, size_t* done, int* error);
static EIO_Status  s_MbedTlsClose (void* session, int how, int* error);
static void        s_MbedTlsDelete(void* session);
static void        s_MbedTlsExit  (void);
static const char* s_MbedTlsError (void* session, int error,
                                   char* buf, size_t size);

static void        x_MbedTlsLogger(void* data, int level,
                                   const char* file, int line,
                                   const char* message);
static int         x_MbedTlsPull  (void*,       unsigned char*, size_t);
static int         x_MbedTlsPush  (void*, const unsigned char*, size_t);

#  ifdef __cplusplus
}
#  endif /*__cplusplus*/


static volatile int             s_MbedTlsLogLevel;
static mbedtls_entropy_context  s_MbedTlsEntropy;
static mbedtls_ctr_drbg_context s_MbedTlsCtrDrbg;
static mbedtls_ssl_config       s_MbedTlsConf;
static volatile FSSLPull        s_Pull;
static volatile FSSLPush        s_Push;


/*ARGSUSED*/
static void x_MbedTlsLogger(void* unused, int level,
                            const char* file, int line,
                            const char* message)
{
    /* do some basic filtering and EOL cut-offs */
    size_t len = message ? strlen(message) : 0;
    if (!len  ||  *message == '\n')
        return;
    if (message[len - 1] == '\n')
        --len;
    CORE_LOGF_X(1, eLOG_Note,
                ("MBEDTLS%d: %.*s", level, (int) len, message));
}


#  ifdef __GNUC__
inline
#  endif /*__GNUC__*/
static EIO_Status x_RetryStatus(SOCK sock, EIO_Event direction)
{
    EIO_Status status;
    if (direction == eIO_Open) {
        EIO_Status r_status = SOCK_Status(sock, eIO_Read);
        EIO_Status w_status = SOCK_Status(sock, eIO_Write);
        status = r_status != eIO_Closed  &&  w_status != eIO_Closed
            ? r_status > w_status ? r_status : w_status
            : eIO_Closed;
    } else
        status = SOCK_Status(sock, direction);
    return status == eIO_Success ? eIO_Timeout : status;
}


#  ifdef __GNUC__
inline
#  endif /*__GNUC__*/
static EIO_Status x_ErrorToStatus(int error, mbedtls_ssl_context* session,
                                  EIO_Event direction)
{
    SOCK       sock;
    EIO_Status status;

    assert(error <= 0);

    if (!error)
        return eIO_Success;
    sock = ((SNcbiSSLctx*)(session->p_bio))->sock;
    switch (error) {
    case MBEDTLS_ERR_SSL_WANT_READ:
        status = x_RetryStatus(sock, direction);
        break;
    case MBEDTLS_ERR_SSL_WANT_WRITE:
        status = x_RetryStatus(sock, direction);
        break;
    case MBEDTLS_ERR_SSL_TIMEOUT:
        status = eIO_Timeout;
        break;
    case MBEDTLS_ERR_THREADING_FEATURE_UNAVAILABLE:
    case MBEDTLS_ERR_SSL_NO_USABLE_CIPHERSUITE:
    case MBEDTLS_ERR_SSL_FEATURE_UNAVAILABLE:
    case MBEDTLS_ERR_SSL_UNKNOWN_CIPHER:
    case MBEDTLS_ERR_SSL_INTERNAL_ERROR:
        status = eIO_NotSupported;
        break;
    case MBEDTLS_ERR_THREADING_BAD_INPUT_DATA:
    case MBEDTLS_ERR_SSL_BAD_INPUT_DATA:
        status = eIO_InvalidArg;
        break;
    case MBEDTLS_ERR_SSL_CONN_EOF:
    case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
        status = eIO_Closed;
        break;
    case MBEDTLS_ERR_NET_CONN_RESET:
    case MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE:
        status = eIO_Unknown;
        break;
    case MBEDTLS_ERR_NET_RECV_FAILED:
        if (sock->r_status != eIO_Success  &&
            sock->r_status != eIO_Closed) {
            status = (EIO_Status) sock->r_status;
        } else
            status = eIO_Unknown;
        break;
    case MBEDTLS_ERR_NET_SEND_FAILED:
        if (sock->w_status != eIO_Success)
            status = (EIO_Status) sock->w_status;
        else
            status = eIO_Unknown;
        break;
    case MBEDTLS_ERR_SSL_NON_FATAL:
        /*return eIO_Interrupt;*/
    default:
        status = eIO_Unknown;
        break;
    }

    assert(status != eIO_Success);
    CORE_DEBUG_ARG(if (s_MbedTlsLogLevel))
        CORE_TRACEF(("MBEDTLS error %d -> CONNECT MBEDTLS status %s",
                     error, IO_StatusStr(status)));
    return status;
}


#  ifdef __GNUC__
inline
#  endif /*__GNUC__*/
static int x_StatusToError(EIO_Status status, SOCK sock, EIO_Event direction)
{
    int error;

    assert(status != eIO_Success);
    assert(direction == eIO_Read  ||  direction == eIO_Write);

    switch (status) {
    case eIO_Timeout:
        error = EAGAIN;
        break;
    case eIO_Interrupt:
        error = SOCK_EINTR;
        break;
    case eIO_NotSupported:
        error = NCBI_NOTSUPPORTED;
        break;
    case eIO_Unknown:
        error = 0/*keep*/;
        break;
    case eIO_Closed:
        error = SOCK_ENOTCONN;
        break;
    default:
        /*NB:eIO_InvalidArg*/
        error = EINVAL;
        break;
    }

    {{
        int x_err = error ? error : errno;
        CORE_DEBUG_ARG(if (s_MbedTlsLogLevel))
            CORE_TRACEF(("CONNECT MBEDTLS status %s -> %s %d",
                         IO_StatusStr(status),
                         error ? "error" : "errno", x_err));
        if (!error)
            errno = x_err; /* restore errno that might have been clobbered */
    }}

    if (error)
        errno = error;
    else if (!(error = errno))
        error = EINVAL;

    switch (error) {
    case EAGAIN:
    case SOCK_EINTR:
        return direction == eIO_Read
            ? MBEDTLS_ERR_SSL_WANT_READ
            : MBEDTLS_ERR_SSL_WANT_WRITE;
    case SOCK_ENOTCONN:
        return MBEDTLS_ERR_NET_CONN_RESET;
    default:
        break;
    }
    return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}


static void* s_MbedTlsCreate(ESOCK_Side side, SNcbiSSLctx* ctx, int* error)
{
    int end = (side == eSOCK_Client
               ? MBEDTLS_SSL_IS_CLIENT
               : MBEDTLS_SSL_IS_SERVER);
    struct SNcbiMbedTlsCred* xcred;
    mbedtls_ssl_context* session;
    int err;

    if (end == MBEDTLS_SSL_IS_SERVER) {
        CORE_LOG_X(2, eLOG_Critical,
                   "Server-side SSL not yet supported with MBEDTLS");
        *error = NCBI_NOTSUPPORTED;
        return 0;
    }

    if (ctx->cred) {
        if (ctx->cred->type != eNcbiCred_MbedTls  ||  !ctx->cred->data) {
            /*FIXME: there's a NULL(data)-terminated array of credentials */
            CORE_LOGF_X(3, eLOG_Error,
                        ("%s credentials in MBEDTLS session",
                         ctx->cred->type != eNcbiCred_MbedTls
                         ? "Foreign"
                         : "Empty"));
            *error = EINVAL;
            return 0;
        }
        xcred = (struct SNcbiMbedTlsCred*) ctx->cred->data;
    } else
        xcred = 0;

    CORE_DEBUG_ARG(if (s_MbedTlsLogLevel))
        CORE_TRACE("MbedTlsCreate(): Enter");

    if (!(session = (mbedtls_ssl_context*) malloc(sizeof(*session)))) {
        *error = errno;
        return 0;
    }
    mbedtls_ssl_init(session);

    if ((err = mbedtls_ssl_setup(session, &s_MbedTlsConf))        != 0   ||
        (ctx->host  &&  *ctx->host
         &&  (err = mbedtls_ssl_set_hostname(session, ctx->host)) != 0)  ||
        (xcred
         &&  (err = mbedtls_ssl_set_hs_own_cert
              (session, xcred->cert, xcred->pkey))                != 0)) {
        mbedtls_ssl_free(session);
        free(session);
        *error = err;
        return 0;
    }

    mbedtls_ssl_set_bio(session, ctx, x_MbedTlsPush, x_MbedTlsPull, 0);
 
    CORE_DEBUG_ARG(if (s_MbedTlsLogLevel))
        CORE_TRACEF(("MbedTlsCreate(): Leave(%p)", session));

    return session;
}


static EIO_Status s_MbedTlsOpen(void* session, int* error, char** desc)
{
    EIO_Status status;
    int x_error;

    CORE_DEBUG_ARG(if (s_MbedTlsLogLevel))
        CORE_TRACEF(("MbedTlsOpen(%p): Enter", session));

    x_error = mbedtls_ssl_handshake((mbedtls_ssl_context*) session);

    if (x_error < 0) {
        status = x_ErrorToStatus(x_error, (mbedtls_ssl_context*) session,
                                 eIO_Open);
        *error = x_error;
        if (desc)
            *desc = 0;
    } else {
        if (desc) {
            const char* alpn
                = mbedtls_ssl_get_alpn_protocol((const mbedtls_ssl_context*)
                                                session);
            size_t alpn_len = alpn ? strlen(alpn) : 0;
            const char* sslv 
                = mbedtls_ssl_get_version      ((const mbedtls_ssl_context*)
                                                session);
            size_t sslv_len = sslv ? strlen(sslv) : 0;
            const char* ciph
                = mbedtls_ssl_get_ciphersuite  ((const mbedtls_ssl_context*)
                                                session);
            size_t ciph_len = ciph ? strlen(ciph) : 0;
            size_t len = alpn_len + sslv_len + ciph_len;
            if (!len)
                *desc = 0;
            else if ((*desc = (char*) malloc(len + 3/*seps+EOL*/)) != 0) {
                char* ptr = *desc;
                if (alpn_len) {
                    memcpy(ptr, alpn, alpn_len);
                    ptr += alpn_len;
                }
                if (sslv_len) {
                    if (ptr != *desc)
                        *ptr++ = '/';
                    memcpy(ptr, sslv, sslv_len);
                    ptr += sslv_len;
                }
                if (ciph_len) {
                    if (ptr != *desc)
                        *ptr++ = '/';
                    memcpy(ptr, ciph, ciph_len);
                    ptr += ciph_len;
                }
                *ptr = '\0';
            }
        }
        status = eIO_Success;
    }

    CORE_DEBUG_ARG(if (s_MbedTlsLogLevel))
        CORE_TRACEF(("MbedTlsOpen(%p): Leave(%d)", session, status));

    return status;
}


#ifdef __GNUC__
inline
#endif /*__GNUC__*/
static int/*bool*/ x_IfToLog(void)
{
    return 4 < s_MbedTlsLogLevel ? 1/*T*/ : 0/*F*/;
}


static int x_MbedTlsPull(void* ctx, unsigned char* buf, size_t size)
{
    SOCK sock = ((SNcbiSSLctx*) ctx)->sock;
    FSSLPull pull = s_Pull;
    EIO_Status status;

    if (pull) {
        size_t x_read = 0;
        status = pull(sock, buf, size, &x_read, x_IfToLog());
        if (x_read > 0  ||  status == eIO_Success/*&& x_read==0*/) {
            assert(status == eIO_Success);
            assert(x_read <= size);
            return (int) x_read;
        }
    } else
        status = eIO_NotSupported;

    return x_StatusToError(status, sock, eIO_Read);
}


static int x_MbedTlsPush(void* ctx, const unsigned char* data, size_t size)
{
    SOCK sock = ((SNcbiSSLctx*) ctx)->sock;
    FSSLPush push = s_Push;
    EIO_Status status;

    if (push) {
        ssize_t n_written = 0;
        do {
            size_t x_written = 0;
            status = push(sock, data, size, &x_written, x_IfToLog());
            if (!x_written) {
                assert(!size  ||  status != eIO_Success);
                if (size  ||  status != eIO_Success)
                    goto out;
            } else {
                assert(status == eIO_Success);
                assert(x_written <= size);
                n_written += (ssize_t) x_written;
                size      -=           x_written;
                data       = data  +   x_written;
            }
        } while (size);
        return (int) n_written;
    } else
        status = eIO_NotSupported;

 out:
    return x_StatusToError(status, sock, eIO_Write);
}


static EIO_Status s_MbedTlsRead(void* session, void* buf, size_t n_todo,
                               size_t* n_done, int* error)
{
    EIO_Status status;
    int        x_read;

    assert(session);

    x_read = mbedtls_ssl_read((mbedtls_ssl_context*) session,
                              (unsigned char*) buf, n_todo);
    assert(x_read < 0  ||  (size_t) x_read <= n_todo);

    if (x_read <= 0) {
        status = x_ErrorToStatus(x_read, (mbedtls_ssl_context*) session,
                                 eIO_Read);
        *error = x_read;
        x_read = 0;
    } else
        status = eIO_Success;

    *n_done = (size_t) x_read;
    return status;
}


static EIO_Status x_MbedTlsWrite(void* session, const void* data,
                                 size_t n_todo, size_t* n_done, int* error)
{
    EIO_Status status;
    int        x_written;

    assert(session);

    x_written = mbedtls_ssl_write((mbedtls_ssl_context*) session,
                                  (const unsigned char*) data, n_todo);
    assert(x_written < 0  ||  (size_t) x_written <= n_todo);

    if (x_written <= 0) {
        status = x_ErrorToStatus(x_written, (mbedtls_ssl_context*) session,
                                 eIO_Write);
        *error = x_written;
        x_written = 0;
    } else
        status = eIO_Success;

    *n_done = (size_t) x_written;
    return status;
}


static EIO_Status s_MbedTlsWrite(void* session, const void* data,
                                 size_t n_todo, size_t* n_done, int* error)
{
    size_t max_size
        = mbedtls_ssl_get_max_frag_len((mbedtls_ssl_context*) session);
    EIO_Status status;

    *n_done = 0;

    do {
        size_t x_todo = n_todo > max_size ? max_size : n_todo;
        size_t x_done;
        status = x_MbedTlsWrite(session, data, x_todo, &x_done, error);
        assert((status == eIO_Success) == (x_done > 0));
        assert(status == eIO_Success  ||  *error);
        assert(x_done <= x_todo);
        if (status != eIO_Success)
            break;
        *n_done += x_done;
        if (x_todo != x_done)
            break;
        n_todo  -= x_done;
        data     = (const char*) data + x_done;
    } while (n_todo);

    return *n_done ? eIO_Success : status;
}


/*ARGSUSED*/
static EIO_Status s_MbedTlsClose(void* session, int how/*unused*/, int* error)
{
    int x_error;

    assert(session);

    CORE_DEBUG_ARG(if (s_MbedTlsLogLevel))
        CORE_TRACEF(("MbedTlsClose(%p): Enter", session));

    x_error = mbedtls_ssl_close_notify((mbedtls_ssl_context*) session);

    CORE_DEBUG_ARG(if (s_MbedTlsLogLevel))
        CORE_TRACEF(("MbedTlsClose(%p): Leave", session));

    if (x_error) {
        *error = x_error;
        return eIO_Unknown;
    }
    return eIO_Success;
}


static void s_MbedTlsDelete(void* session)
{
    assert(session);

    CORE_DEBUG_ARG(if (s_MbedTlsLogLevel))
        CORE_TRACEF(("MbedTlsDelete(%p): Enter", session));

    mbedtls_ssl_free((mbedtls_ssl_context*) session);

    CORE_DEBUG_ARG(if (s_MbedTlsLogLevel))
        CORE_TRACEF(("MbedTlsDelete(%p): Leave", session));

    free(session);
}


static EIO_Status x_InitLocking(void)
{
    EIO_Status status;

#  ifdef NCBI_THREADS
    switch (mbedtls_version_check_feature("MBEDTLS_THREADING_C")) {
    case  0:
        break;
    case -1:
        return eIO_NotSupported;
    case -2:
        return eIO_InvalidArg;
    default:
        return eIO_Unknown;
    }
#  endif /*NCBI_THREADS*/

#  ifdef MBEDTLS_THREADING_PTHREAD
    status = eIO_Success;
#  elif defined(MBEDTLS_THREADING_ALT)  &&  defined(NCBI_THREADS)
    int locked;
    MT_LOCK lock = CORE_GetLOCK();
    if ((locked = MT_LOCK_Do(lock, eMT_Lock)) > 0) {
        mbedtls_threading_set_alt(mbtls_user_mutex_init,
                                  mbtls_user_mutex_deinit,
                                  mbtls_user_mutex_lock,
                                  mbtls_user_mutex_unlock);
        MT_LOCK_Do(lock, eMT_Unlock);
        status = eIO_Success;
    } else
        status = !locked ? eIO_Unknown : lock ? eIO_Success : eIO_NotSupported;
#  elif !defined(NCBI_NO_THREADS)  &&  defined(_MT)
    CORE_LOG_X(4, eLOG_Critical,
               "MBEDTLS locking uninited: Unknown threading model");
    status = eIO_NotSupported;
#  else
    status = eIO_Success;
#  endif

    return status;
}


static void x_MbedTlsExit(void)
{
    s_Push = 0;
    s_Pull = 0;

    mbedtls_ctr_drbg_free(&s_MbedTlsCtrDrbg);
    mbedtls_entropy_free(&s_MbedTlsEntropy);
    mbedtls_ssl_config_free(&s_MbedTlsConf);
    mbedtls_debug_set_threshold(s_MbedTlsLogLevel = 0);
    memset(&s_MbedTlsCtrDrbg, 0, sizeof(s_MbedTlsCtrDrbg));
    memset(&s_MbedTlsEntropy, 0, sizeof(s_MbedTlsEntropy));
    memset(&s_MbedTlsConf,    0, sizeof(s_MbedTlsConf));
#  if defined(MBEDTLS_THREADING_ALT)  &&  defined(NCBI_THREADS)
    mbedtls_threading_free_alt();
#  endif /*MBEDTLS_THREADING_ALT && NCBI_THREADS*/
}


/* NB: Called under a lock */
static EIO_Status s_MbedTlsInit(FSSLPull pull, FSSLPush push)
{
    static const char kMbedTls[] =
#  ifdef HAVE_LIBMBEDTLS
        "External "
#  else
        "Embedded "
#  endif /*HAVE_LIBMBEDTLS*/
        "MBEDTLS";
    EIO_Status status;
    char version[80];
    const char* val;
    char buf[32];

    mbedtls_version_get_string(version);
    if (strcasecmp(MBEDTLS_VERSION_STRING, version) != 0) {
        CORE_LOGF_X(5, eLOG_Critical,
                    ("%s version mismatch: %s headers vs. %s runtime",
                     kMbedTls, MBEDTLS_VERSION_STRING, version));
        assert(0);
    }

    CORE_TRACE("MbedTlsInit(): Enter");

    if (!pull  ||  !push)
        return eIO_InvalidArg;

    mbedtls_ssl_config_init(&s_MbedTlsConf);
    mbedtls_ssl_config_defaults(&s_MbedTlsConf,
                                MBEDTLS_SSL_IS_CLIENT,
                                MBEDTLS_SSL_TRANSPORT_STREAM,
                                MBEDTLS_SSL_PRESET_DEFAULT);
    mbedtls_ssl_conf_authmode(&s_MbedTlsConf, MBEDTLS_SSL_VERIFY_NONE);

    /* Check CONN_[MBED]TLS_LOGLEVEL or [CONN][MBED]TLS_LOGLEVEL */
    val = ConnNetInfo_GetValueInternal(0, "MBED" REG_CONN_TLS_LOGLEVEL,
                                       buf, sizeof(buf),
                                       DEF_CONN_TLS_LOGLEVEL);
    if (!val  ||  !*val) {
        val = ConnNetInfo_GetValueInternal(0, REG_CONN_TLS_LOGLEVEL,
                                           buf, sizeof(buf),
                                           DEF_CONN_TLS_LOGLEVEL);
    }
    CORE_LOCK_READ;
    if (val  &&  *val) {
        ELOG_Level level;
        s_MbedTlsLogLevel = atoi(val);
        CORE_UNLOCK;
        if (s_MbedTlsLogLevel) {
            mbedtls_debug_set_threshold(s_MbedTlsLogLevel);
            mbedtls_ssl_conf_dbg(&s_MbedTlsConf, x_MbedTlsLogger, 0);
            level = eLOG_Note;
        } else
            level = eLOG_Trace;
        CORE_LOGF_X(6, level, ("%s V%s (LogLevel=%d)",
                               kMbedTls, version, s_MbedTlsLogLevel));
    } else
        CORE_UNLOCK;

    CORE_DEBUG_ARG(if (s_MbedTlsLogLevel))
        CORE_TRACE("MbedTlsInit(): Go-on");

    if ((status = x_InitLocking()) != eIO_Success) {
        mbedtls_ssl_config_free(&s_MbedTlsConf);
        mbedtls_debug_set_threshold(s_MbedTlsLogLevel = 0);
        memset(&s_MbedTlsConf, 0, sizeof(s_MbedTlsConf));
        return status;
    }

    mbedtls_entropy_init(&s_MbedTlsEntropy);
    mbedtls_ctr_drbg_init(&s_MbedTlsCtrDrbg); 

    if (mbedtls_ctr_drbg_seed(&s_MbedTlsCtrDrbg, mbedtls_entropy_func,
                              &s_MbedTlsEntropy, 0, 0) != 0) {
        x_MbedTlsExit();
        return eIO_Unknown;
    }
    mbedtls_ssl_conf_rng(&s_MbedTlsConf,
                         mbedtls_ctr_drbg_random, &s_MbedTlsCtrDrbg);

    s_Pull = pull;
    s_Push = push;

    CORE_DEBUG_ARG(if (s_MbedTlsLogLevel))
        CORE_TRACE("MbedTlsInit(): Leave");
    return eIO_Success;
}


/* NB: Called under a lock */
static void s_MbedTlsExit(void)
{
    CORE_DEBUG_ARG(if (s_MbedTlsLogLevel))
        CORE_TRACE("MbedTlsExit(): Enter");

    x_MbedTlsExit();

    CORE_TRACE("MbedTlsExit(): Leave");
}

 
static const char* s_MbedTlsError(void* session/*unused*/, int error,
                                  char* buf, size_t size)
{
    mbedtls_strerror(error, buf, size);
    return buf;
}

#else

/*ARGSUSED*/
static EIO_Status s_MbedTlsInit(FSSLPull unused_pull, FSSLPush unused_push)
{
    CORE_LOG_X(7, eLOG_Critical, "Unavailable feature MBEDTLS");
    return eIO_NotSupported;
}

#endif /*HAVE_LIBMBEDTLS || NCBI_CXX_TOOLKIT*/


extern SOCKSSL NcbiSetupMbedTls(void)
{
    static const struct SOCKSSL_struct kMbedTlsOps = {
        "MBEDTLS"
        , s_MbedTlsInit
#if defined(HAVE_LIBMBEDTLS)  ||  defined(NCBI_CXX_TOOLKIT)
        , s_MbedTlsCreate
        , s_MbedTlsOpen
        , s_MbedTlsRead
        , s_MbedTlsWrite
        , s_MbedTlsClose
        , s_MbedTlsDelete
        , s_MbedTlsExit
        , s_MbedTlsError
#endif /*HAVE_LIBMBEDTLS || NCBI_CXX_TOOLKIT*/
    };
#if !defined(HAVE_LIBMBEDTLS)  &&  !defined(NCBI_CXX_TOOLKIT)
    CORE_LOG_X(8, eLOG_Warning, "Unavailable feature MBEDTLS");
#endif /*!HAVE_LIBMBEDTLS && !NCBI_CXX_TOOLKIT*/
    return &kMbedTlsOps;
}


#define ALIGN2(s, a)  ((((s) + ((a) - 1)) / (a)) * (a))
#define ALIGN(s)      ALIGN2(s, sizeof(double))


extern NCBI_CRED NcbiCredMbedTls(void* xcert, void* xpkey)
{
    struct SNcbiCred*        cred;
    struct SNcbiMbedTlsCred* xcred;
    size_t size = ALIGN(sizeof(*cred));
    if (xcert  &&  xpkey) {
        size <<= 1;
        size  += sizeof(*xcred);
    }
    cred = (NCBI_CRED) calloc(1, size);
    if (cred) {
        cred->type = eNcbiCred_MbedTls;
        if (xcert  &&  xpkey) {
            xcred = (struct SNcbiMbedTlsCred*)
                ((char*) cred + 2*ALIGN(sizeof(*cred)));
            xcred->cert = (mbedtls_x509_crt*)   xcert;
            xcred->pkey = (mbedtls_pk_context*) xpkey;
            cred->data  = xcred;
        }
    }
    return cred;
}


#if defined(HAVE_LIBMBEDTLS)  ||  defined(NCBI_CXX_TOOLKIT)

void NcbiDeleteMbedTlsCertCredentials(NCBI_CRED cred)
{
    if (cred->type / 100 == eNcbiCred_MbedTls / 100  &&  !(cred->type % 100)) {
        struct SNcbiMbedTlsCred* xcred = (struct SNcbiMbedTlsCred*) cred->data;
        mbedtls_x509_crt_free(xcred->cert);
        mbedtls_pk_free      (xcred->pkey);
        memset(xcred, 0, sizeof(*xcred));
    } else {
        char who[80];
        switch (cred->type / 100) {
        case eNcbiCred_GnuTls / 100:
            strcpy(who, "GNUTLS");
            break;
        case eNcbiCred_MbedTls / 100:
            strcpy(who, "MBEDTLS");
            break;
        default:
            sprintf(who, "TLS 0x%08X", cred->type);
            break;
        }
        CORE_LOGF_X(9, eLOG_Critical,
                    ("Deleting unknown certificate credentials (%s/%u)",
                     who, cred->type % 100));
        assert(0);
    }
    cred->type = (ENcbiCred) 0;
    cred->data = 0;
    free(cred);
}


NCBI_CRED NcbiCreateMbedTlsCertCredentials(const void* cert,
                                           size_t      certsz,
                                           const void* pkey,
                                           size_t      pkeysz)
{
    struct SNcbiCred*    ncbi_cred;
    struct SNcbiMbedTlsCred* xcred;
    const size_t size = (2*ALIGN(sizeof(*ncbi_cred))
                         + ALIGN(sizeof(*xcred))
                         + ALIGN(sizeof(*xcred->cert))
                         +       sizeof(*xcred->pkey));
    CORE_DEBUG_ARG(char tmp[1024];)
    char   errbuf[80];
    int    err;

    if (!(ncbi_cred = (NCBI_CRED) calloc(1, size))) {
        CORE_LOGF_ERRNO_X(10, eLOG_Error, errno,
                          ("Cannot allocate NCBI_CRED (%lu bytes)",
                           (unsigned long) size));
        return 0;
    }

    xcred = (struct SNcbiMbedTlsCred*)
        ((char*) ncbi_cred   + 2*ALIGN(sizeof(*ncbi_cred)));
    xcred->cert = (mbedtls_x509_crt*)
        ((char*)     xcred   +   ALIGN(sizeof(*xcred)));
    xcred->pkey = (mbedtls_pk_context*)
        ((char*) xcred->cert +   ALIGN(sizeof(*xcred->cert)));
    ncbi_cred->type = eNcbiCred_MbedTls;
    ncbi_cred->data = xcred;

    /* these are not technically necessary as they just zero the memory */
    mbedtls_x509_crt_init(xcred->cert);
    mbedtls_pk_init      (xcred->pkey);

    err = mbedtls_x509_crt_parse(xcred->cert,
                                 (const unsigned char*) cert, certsz ? certsz
                                 : strlen((const char*) cert) + 1);
    if (err) {
        mbedtls_strerror(err, errbuf, sizeof(errbuf) - 1);
        CORE_LOG_ERRNO_EXX(11, eLOG_Error, err, errbuf,
                           "mbedTLS cannot parse X.509 certificate");
        goto out;
    }
    CORE_DEBUG_ARG(err = mbedtls_x509_crt_info(tmp, sizeof(tmp),
                                               "", xcred->cert));
    CORE_TRACEF(("Certificate loaded%s%s",
                 err > 0 ? ":\n" : "",
                 err > 0 ? tmp   : ""));

    err = mbedtls_pk_parse_key(xcred->pkey,
                               (const unsigned char*) pkey, pkeysz ? pkeysz
                               : strlen((const char*) pkey) + 1, 0, 0);
    if (err) {
        mbedtls_strerror(err, errbuf, sizeof(errbuf) - 1);
        CORE_LOG_ERRNO_EXX(12, eLOG_Error, err, errbuf,
                           "mbedTLS cannot parse private key");
        goto out;
    }
    CORE_TRACEF(("Private key loaded: %s",
                 mbedtls_pk_get_name(xcred->pkey)));

    return ncbi_cred;

 out:
    NcbiDeleteMbedTlsCertCredentials(ncbi_cred);
    return 0;
}

#endif /*HAVE_LIBMBEDTLS || NCBI_CXX_TOOLKIT*/
