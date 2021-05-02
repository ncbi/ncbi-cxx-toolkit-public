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
 *   GNUTLS support for SSL in connection library
 *
 */

#include "ncbi_ansi_ext.h"
#include "ncbi_connssl.h"
#include "ncbi_priv.h"
#include "ncbi_servicep.h"
#include <connect/ncbi_gnutls.h>
#include <connect/ncbi_tls.h>
#include <stdlib.h>

#define NCBI_USE_ERRCODE_X   Connect_TLS


#ifdef HAVE_LIBGNUTLS

#  include <gnutls/gnutls.h>
#  include <gnutls/x509.h>

#  if   defined(ENOTSUP)
#    define NCBI_NOTSUPPORTED  ENOTSUP
#  elif defined(ENOSYS)
#    define NCBI_NOTSUPPORTED  ENOSYS
#  else
#    define NCBI_NOTSUPPORTED  EINVAL
#  endif

#  if GNUTLS_VERSION_NUMBER < 0x020C00
#    ifdef HAVE_LIBGCRYPT

#      include <gcrypt.h>

#      if   defined(NCBI_POSIX_THREADS)

#        include <pthread.h>
#        ifdef __cplusplus
extern "C" {
#        endif /*__cplusplus*/
    GCRY_THREAD_OPTION_PTHREAD_IMPL;
#        ifdef __cplusplus
} /* extern "C" */
#        endif /*__cplusplus*/

#      elif defined(NCBI_THREADS)

#        ifdef __cplusplus
extern "C" {
#        endif /*__cplusplus*/
static int gcry_user_mutex_init(void** lock)
{
    return !(*lock = MT_LOCK_AddRef(g_CORE_MT_Lock)) ? NCBI_NOTSUPPORTED : 0;
}
static int gcry_user_mutex_destroy(void** lock)
{
    g_CORE_MT_Lock = MT_LOCK_Delete(*((MT_LOCK*) lock));
    *lock = 0;
    return 0;
}
static int gcry_user_mutex_lock(void** lock)
{
    return MT_LOCK_Do((MT_LOCK)(*lock), eMT_Lock) > 0 ? 0 : NCBI_NOTSUPPORTED;
}
static int gcry_user_mutex_unlock(void** lock)
{
    return MT_LOCK_Do((MT_LOCK)(*lock), eMT_Unlock)!=0 ? 0 : NCBI_NOTSUPPORTED;
}
static struct gcry_thread_cbs gcry_threads_user = {
    GCRY_THREAD_OPTION_USER, NULL/*gcry_user_init*/,
    gcry_user_mutex_init, gcry_user_mutex_destroy,
    gcry_user_mutex_lock, gcry_user_mutex_unlock,
    NULL/*all other fields NULL-inited*/
};
#        ifdef __cplusplus
} /* extern "C" */
#        endif /*__cplusplus*/

#      endif /*NCBI_..._THREADS*/

#    endif /*HAVE_LIBGCRYPT*/
#  elif defined(NCBI_THREADS)
#    ifdef __cplusplus
extern "C" {
#    endif /*__cplusplus*/
static int gtls_user_mutex_init(void** lock)
{
    return !(*lock = MT_LOCK_AddRef(g_CORE_MT_Lock)) ? NCBI_NOTSUPPORTED : 0;
}
static int gtls_user_mutex_deinit(void** lock)
{
    g_CORE_MT_Lock = MT_LOCK_Delete((MT_LOCK)(*lock));
    *lock = 0;
    return 0;
}
static int gtls_user_mutex_lock(void** lock)
{
    return MT_LOCK_Do((MT_LOCK)(*lock), eMT_Lock) > 0 ? 0 : NCBI_NOTSUPPORTED;
}
static int gtls_user_mutex_unlock(void** lock)
{
    return MT_LOCK_Do((MT_LOCK)(*lock), eMT_Unlock)!=0 ? 0 : NCBI_NOTSUPPORTED;
}
#    ifdef __cplusplus
}
#    endif /*__cplusplus*/
#  endif


#  ifdef __cplusplus
extern "C" {
#  endif /*__cplusplus*/

static EIO_Status  s_GnuTlsInit  (FSSLPull pull, FSSLPush push);
static void*       s_GnuTlsCreate(ESOCK_Side side, SNcbiSSLctx* ctx,
                                  int* error);
static EIO_Status  s_GnuTlsOpen  (void* session, int* error, char** desc);
static EIO_Status  s_GnuTlsRead  (void* session,       void* buf,  size_t size,
                                  size_t* done, int* error);
static EIO_Status  s_GnuTlsWrite (void* session, const void* data, size_t size,
                                  size_t* done, int* error);
static EIO_Status  s_GnuTlsClose (void* session, int how, int* error);
static void        s_GnuTlsDelete(void* session);
static void        s_GnuTlsExit  (void);
static const char* s_GnuTlsError (void* session, int error,
                                  char* buf, size_t size);

static void        x_GnuTlsLogger(int level, const char* message);
static ssize_t     x_GnuTlsPull  (gnutls_transport_ptr_t,       void*, size_t);
static ssize_t     x_GnuTlsPush  (gnutls_transport_ptr_t, const void*, size_t);

#  ifdef __cplusplus
}
#  endif /*__cplusplus*/


#  if LIBGNUTLS_VERSION_NUMBER < 0x030306
static const int kGnuTlsCertPrio[] = {
    GNUTLS_CRT_X509,
    /*GNUTLS_CRT_OPENPGP,*/
    0
};
static const int kGnuTlsCompPrio[] = {
    GNUTLS_COMP_ZLIB,
    GNUTLS_COMP_NULL,
    0
};
#  endif /*LIBGNUTLS_VERSION_NUMBER<3.3.6*/


static volatile int                     s_GnuTlsLogLevel;
static gnutls_anon_client_credentials_t s_GnuTlsCredAnon;
static gnutls_certificate_credentials_t s_GnuTlsCredCert;
static volatile FSSLPull                s_Pull;
static volatile FSSLPush                s_Push;


static void x_GnuTlsLogger(int level, const char* message)
{
    /* do some basic filtering and EOL cut-offs */
    size_t len = message ? strlen(message) : 0;
    if (!len  ||  *message == '\n')
        return;
    if (strncasecmp(message, "ASSERT: ", 8) == 0)
        return;
    if (message[len - 1] == '\n')
        --len;
    CORE_LOGF_X(21, eLOG_Note,
                ("GNUTLS%d: %.*s", level, (int) len, message));
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
static EIO_Status x_AlertToStatus(gnutls_alert_description_t alert,
                                int/*bool*/ fatal)
{
    EIO_Status status;
    switch (alert) {
    case GNUTLS_A_CLOSE_NOTIFY:
        status = fatal ? eIO_Unknown : eIO_Closed;
        break;
    case GNUTLS_A_USER_CANCELED:
        status = eIO_Interrupt;
        break;
    case GNUTLS_A_NO_APPLICATION_PROTOCOL:
        status = eIO_NotSupported;
        break;
    default:
        status = eIO_Unknown;
        break;
    }
    return status;
}


#  ifdef __GNUC__
inline
#  endif /*__GNUC__*/
static EIO_Status x_ErrorToStatus(int* error, gnutls_session_t session,
                                  EIO_Event direction)
{
    SOCK       sock;
    EIO_Status status;

    assert(error  &&  *error <= 0/*GNUTLS_E_SUCCESS*/);

    if (!*error)
        return eIO_Success;
    sock = ((SNcbiSSLctx*) gnutls_transport_get_ptr(session))->sock;
    if      (*error == GNUTLS_E_AGAIN)
        status = x_RetryStatus(sock, direction);
    else if (*error == GNUTLS_E_INTERRUPTED)
        status = eIO_Interrupt;
    else if (*error == GNUTLS_E_WARNING_ALERT_RECEIVED) {
        gnutls_alert_description_t alert = gnutls_alert_get(session);
        status = x_AlertToStatus(alert, 0/*non-fatal*/);
        *error = GNUTLS_E_APPLICATION_ERROR_MAX - alert;
    }
    else if (*error == GNUTLS_E_FATAL_ALERT_RECEIVED) {
        gnutls_alert_description_t alert = gnutls_alert_get(session);
        status = x_AlertToStatus(alert, 1/*fatal*/);
        *error = GNUTLS_E_APPLICATION_ERROR_MAX - alert;
    }
    else if (*error == GNUTLS_E_PULL_ERROR
             &&  sock->r_status != eIO_Success
             &&  sock->r_status != eIO_Closed) {
        status = (EIO_Status) sock->r_status;
    }
    else if (*error == GNUTLS_E_PUSH_ERROR
             &&  sock->w_status != eIO_Success) {
        status = (EIO_Status) sock->w_status;
    }
    else
        status = *error == GNUTLS_E_SESSION_EOF ? eIO_Closed : eIO_Unknown;

    assert(status != eIO_Success);
    CORE_TRACEF(("GNUTLS error %d%s -> CONNECT GNUTLS status %s",
                 *error, gnutls_error_is_fatal(*error) ? "(fatal)" : "",
                 IO_StatusStr(status)));
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
        CORE_DEBUG_ARG(if (s_GnuTlsLogLevel))
            CORE_TRACEF(("CONNECT GNUTLS status %s -> %s %d",
                         IO_StatusStr(status),
                         error ? "error" : "errno", x_err));
        if (!error)
            errno = x_err; /* restore errno that might have been clobbered */
    }}

    return error;
}


static void* s_GnuTlsCreate(ESOCK_Side side, SNcbiSSLctx* ctx, int* error)
{
    gnutls_connection_end_t end = (side == eSOCK_Client
                                   ? GNUTLS_CLIENT
                                   : GNUTLS_SERVER);
    gnutls_certificate_credentials_t xcred;
    gnutls_anon_client_credentials_t acred;
    gnutls_session_t session;
    char val[128];
    size_t len;
    int err;

    if (end == GNUTLS_SERVER) {
        CORE_LOG_X(22, eLOG_Critical,
                   "Server-side SSL not yet supported with GNUTLS");
        *error = NCBI_NOTSUPPORTED;
        return 0;
    }

    CORE_LOCK_READ;
    xcred = s_GnuTlsCredCert;
    acred = s_GnuTlsCredAnon;
    CORE_UNLOCK;

    if (!acred
        ||  (ctx->cred
             &&  (ctx->cred->type != eNcbiCred_GnuTls  ||  !ctx->cred->data))){
        CORE_LOGF_X(23, eLOG_Error,
                    ("Cannot %s GNUTLS credentials: %s",
                     acred ? "use"            : "set",
                     acred ? "Invalid format" : "Not initialized"));
        /*FIXME: there's a NULL(data)-terminated array of credentials */
        *error = acred ? EINVAL : NCBI_NOTSUPPORTED;
        return 0;
    }

    CORE_DEBUG_ARG(if (s_GnuTlsLogLevel))
        CORE_TRACE("GnuTlsCreate(): Enter");

    if ((err = gnutls_init(&session, end)) != GNUTLS_E_SUCCESS/*0*/) {
        *error = err;
        return 0;
    }

    /* Check CONN_GNUTLS_PRIORITY or [CONN]GNUTLS_PRIORITY */
    ConnNetInfo_GetValueInternal(0, "GNUTLS_PRIORITY", val, sizeof(val), 0);

    len = ctx->host ? strlen(ctx->host) : 0;

    if ((err = gnutls_set_default_priority(session))                   != 0  ||
#  if LIBGNUTLS_VERSION_NUMBER >= 0x020200
        ( *val  &&
          (err = gnutls_priority_set_direct(session, val, 0))          != 0) ||
#  endif /*LIBGNUTLS_VERSION_NUMBER>=2.2.0*/
#  if LIBGNUTLS_VERSION_NUMBER < 0x030306
        (!*val  &&
         (err = gnutls_compression_set_priority(session,
                                                kGnuTlsCompPrio))      != 0) ||
        (!*val  &&
         (err = gnutls_certificate_type_set_priority(session,
                                                     kGnuTlsCertPrio)) != 0) ||
#  endif /*LIBGNUTLS_VERSION_NUMBER<3.3.6*/
        (err = gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE,
                                      ctx->cred
                                      ? (gnutls_certificate_credentials_t)
                                         ctx->cred->data
                                      : xcred))                        != 0  ||
        (err = gnutls_credentials_set(session, GNUTLS_CRD_ANON, acred))!= 0  ||
        (len  &&  (err = gnutls_server_name_set(session, GNUTLS_NAME_DNS,
                                                ctx->host, len))       != 0)) {
        gnutls_deinit(session);
        *error = err;
        return 0;
    }

    gnutls_transport_set_pull_function(session, x_GnuTlsPull);
    gnutls_transport_set_push_function(session, x_GnuTlsPush);
    gnutls_transport_set_ptr(session, ctx);
    gnutls_session_set_ptr(session, ctx);

#  if LIBGNUTLS_VERSION_NUMBER >= 0x030000
    gnutls_handshake_set_timeout(session, 0);
#  endif /*LIBGNUTLS_VERSION_NUMBER>=3.0.0*/

    CORE_DEBUG_ARG(if (s_GnuTlsLogLevel))
        CORE_TRACEF(("GnuTlsCreate(): Leave(%p)", session));

    return session;
}


static EIO_Status s_GnuTlsOpen(void* session, int* error, char** desc)
{
    EIO_Status status;
    int x_error;

    CORE_DEBUG_ARG(if (s_GnuTlsLogLevel))
        CORE_TRACEF(("GnuTlsOpen(%p): Enter", session));

    do {
        x_error = gnutls_handshake((gnutls_session_t) session);
    } while (x_error == GNUTLS_E_REHANDSHAKE);

    if (x_error < 0) {
        status = x_ErrorToStatus(&x_error, (gnutls_session_t) session,
                                 eIO_Open);
        assert(status != eIO_Success);
        *error = x_error;
        if (desc)
            *desc = 0;
    } else {
        if (desc) {
#  if LIBGNUTLS_VERSION_NUMBER >= 0x030110
            char* temp = gnutls_session_get_desc((gnutls_session_t) session);
            if (temp) {
                *desc = strdup(temp);
                gnutls_free(temp);
            } else
#  endif /*LIBGNUTLS_VERSION_NUMBER>=3.1.10*/
                *desc = 0;
        }
        status = eIO_Success;
    }

    CORE_DEBUG_ARG(if (s_GnuTlsLogLevel))
        CORE_TRACEF(("GnuTlsOpen(%p): Leave(%d)", session, status));

    return status;
}


#ifdef __GNUC__
inline
#endif /*__GNUC__*/
static int/*bool*/ x_IfToLog(void)
{
    return 12 < s_GnuTlsLogLevel ? 1/*T*/ : 0/*F*/;
}


/*ARGSUSED*/
#ifdef __GNUC__
inline
#endif /*__GNUC__*/
static void x_set_errno(gnutls_session_t session, int error)
{
    assert(session);
#  if LIBGNUTLS_VERSION_NUMBER >= 0x010504
    gnutls_transport_set_errno(session, error);
#  else
    if (error)
        errno = error;
#  endif /*LIBGNUTLS_VERSION>=1.5.4*/
}


static ssize_t x_GnuTlsPull(gnutls_transport_ptr_t ptr,
                            void* buf, size_t size)
{
    SNcbiSSLctx* ctx = (SNcbiSSLctx*) ptr;
    FSSLPull pull = s_Pull;
    EIO_Status status;
    int x_error;

    if (pull) {
        size_t x_read = 0;
        status = pull(ctx->sock, buf, size, &x_read, x_IfToLog());
        if (x_read > 0  ||  status == eIO_Success/*&& x_read==0*/) {
            assert(status == eIO_Success);
            assert(x_read <= size);
            x_set_errno((gnutls_session_t) ctx->sess, 0);
            return x_read;
        }
    } else
        status = eIO_NotSupported;

    x_error = x_StatusToError(status, ctx->sock, eIO_Read);
    if (x_error)
        x_set_errno((gnutls_session_t) ctx->sess, x_error);
    return -1;
}


static ssize_t x_GnuTlsPush(gnutls_transport_ptr_t ptr,
                            const void* data, size_t size)
{
    SNcbiSSLctx* ctx = (SNcbiSSLctx*) ptr;
    FSSLPush push = s_Push;
    EIO_Status status;
    int x_error;

    if (push) {
        ssize_t n_written = 0;
        do {
            size_t x_written = 0;
            status = push(ctx->sock, data, size, &x_written, x_IfToLog());
            if (!x_written) {
                assert(!size  ||  status != eIO_Success);
                if (size  ||  status != eIO_Success)
                    goto out;
            } else {
                assert(status == eIO_Success);
                assert(x_written <= size);
                n_written += (ssize_t)            x_written;
                size      -=                      x_written;
                data       = (const char*) data + x_written;
            }
        } while (size);
        x_set_errno((gnutls_session_t) ctx->sess, 0);
        return n_written;
    } else
        status = eIO_NotSupported;

 out:
    x_error = x_StatusToError(status, ctx->sock, eIO_Write);
    if (x_error)
        x_set_errno((gnutls_session_t) ctx->sess, x_error);
    return -1;
}


static EIO_Status s_GnuTlsRead(void* session, void* buf, size_t n_todo,
                               size_t* n_done, int* error)
{
    EIO_Status status;
    ssize_t    x_read;

    assert(session);

    x_read = gnutls_record_recv((gnutls_session_t) session, buf, n_todo);
    assert(x_read < 0  ||  (size_t) x_read <= n_todo);

    if (x_read <= 0) {
        int x_error = (int) x_read;
        status = x_ErrorToStatus(&x_error, (gnutls_session_t) session,
                                 eIO_Read);
        *error = x_error;
        x_read = 0;
    } else
        status = eIO_Success;

    *n_done = (size_t) x_read;
    return status;
}


static EIO_Status x_GnuTlsWrite(void* session, const void* data, size_t n_todo,
                                size_t* n_done, int* error)
{
    EIO_Status status;
    ssize_t    x_written;

    assert(session);

    x_written = gnutls_record_send((gnutls_session_t) session, data, n_todo);
    assert(x_written < 0  ||  (size_t) x_written <= n_todo);

    if (x_written <= 0) {
        int x_error = (int) x_written;
        status = x_ErrorToStatus(&x_error, (gnutls_session_t) session,
                                 eIO_Write);
        *error = x_error;
        x_written = 0;
    } else
        status = eIO_Success;

    *n_done = (size_t) x_written;
    return status;
}


static EIO_Status s_GnuTlsWrite(void* session, const void* data, size_t n_todo,
                                size_t* n_done, int* error)
{
    size_t max_size = gnutls_record_get_max_size((gnutls_session_t) session);
    EIO_Status status;

    *n_done = 0;

    do {
        size_t x_todo = n_todo > max_size ? max_size : n_todo;
        size_t x_done;
        status = x_GnuTlsWrite(session, data, x_todo, &x_done, error);
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


static EIO_Status s_GnuTlsClose(void* session, int how, int* error)
{
    int x_error;

    assert(session);

    CORE_DEBUG_ARG(if (s_GnuTlsLogLevel))
        CORE_TRACEF(("GnuTlsClose(%p): Enter", session));

    x_error = gnutls_bye((gnutls_session_t) session,
                         how == SOCK_SHUTDOWN_RDWR
                         ? GNUTLS_SHUT_RDWR
                         : GNUTLS_SHUT_WR);

    CORE_DEBUG_ARG(if (s_GnuTlsLogLevel))
        CORE_TRACEF(("GnuTlsClose(%p): Leave", session));

    if (x_error != GNUTLS_E_SUCCESS) {
        *error = x_error;
        return eIO_Unknown;
    }
    return eIO_Success;
}


static void s_GnuTlsDelete(void* session)
{
    assert(session);

    CORE_DEBUG_ARG(if (s_GnuTlsLogLevel))
        CORE_TRACEF(("GnuTlsDelete(%p): Enter", session));

    gnutls_deinit((gnutls_session_t) session);

    CORE_DEBUG_ARG(if (s_GnuTlsLogLevel))
        CORE_TRACEF(("GnuTlsDelete(%p): Leave", session));
}


static EIO_Status x_InitLocking(void)
{
    EIO_Status status;

#  if GNUTLS_VERSION_NUMBER < 0x020C00
#    ifdef HAVE_LIBGCRYPT
#      if   defined(NCBI_POSIX_THREADS)
    status = gcry_control(GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread) == 0
        ? eIO_Success
        : eIO_NotSupported;
#      elif defined(NCBI_THREADS)
    MT_LOCK lk = CORE_GetLOCK();
    if (MT_LOCK_Do(lk, eMT_Lock) != -1) {
        status = gcry_control(GCRYCTL_SET_THREAD_CBS, &gcry_threads_user) == 0
            ? eIO_Success
            : eIO_NotSupported;
        MT_LOCK_Do(lk, eMT_Unlock);
    } else
        status = lk ? eIO_Success : eIO_NotSupported;
#      elif !defined(NCBI_NO_THREADS)  &&  defined(_MT)
    CORE_LOG_X(24, eLOG_Critical,
               "LIBGCRYPT uninitialized: Unknown threading model");
    status = eIO_NotSupported;
#      endif /*NCBI_POSIX_THREADS*/
#    endif /*HAVE_LIBGCRYPT*/
#  elif defined(NCBI_THREADS)
    MT_LOCK lk = CORE_GetLOCK();
    if (MT_LOCK_Do(lk, eMT_Lock) != -1) {
        /* NB: calls global_deinit/global_init internally */
        gnutls_global_set_mutex(gtls_user_mutex_init, gtls_user_mutex_deinit,
                                gtls_user_mutex_lock, gtls_user_mutex_unlock);
        MT_LOCK_Do(lk, eMT_Unlock);
        status = eIO_Success;
    } else
        status = lk ? eIO_Success : eIO_NotSupported;
#  elif !defined(NCBI_NO_THREADS)  &&  defined(_MT)
    CORE_LOG_X(25, eLOG_Critical,
               "GNUTLS locking uninited: Unknown threading model");
    status = eIO_NotSupported;
#  else
    status = eIO_Success;
#  endif

    return status;
}


/* NB: Called under a lock */
static EIO_Status s_GnuTlsInit(FSSLPull pull, FSSLPush push)
{
    gnutls_anon_client_credentials_t acred;
    gnutls_certificate_credentials_t xcred;
    const char* version;
    EIO_Status status;
    const char* val;
    char buf[32];

    assert(!s_GnuTlsCredAnon);

    version = gnutls_check_version(0);
    if (strcasecmp(GNUTLS_VERSION, version) != 0) {
        CORE_LOGF_X(26, eLOG_Critical,
                    ("GNUTLS version mismatch: %s headers vs. %s runtime",
                     GNUTLS_VERSION, version));
        assert(0);
    }

    CORE_TRACE("GnuTlsInit(): Enter");

    if (!pull  ||  !push)
        return eIO_InvalidArg;

    /* Check CONN_[GNU]TLS_LOGLEVEL or [CONN][GNU]TLS_LOGLEVEL */
    val = ConnNetInfo_GetValueInternal(0, "GNU" REG_CONN_TLS_LOGLEVEL,
                                       buf, sizeof(buf),
                                       DEF_CONN_TLS_LOGLEVEL);
    if (!val  ||  !*val) {
        val = ConnNetInfo_GetValueInternal(0, REG_CONN_TLS_LOGLEVEL,
                                           buf, sizeof(buf),
                                           DEF_CONN_TLS_LOGLEVEL);
    }
    CORE_LOCK_READ;
    if (!val  ||  !*val)
        val = getenv("GNUTLS_DEBUG_LEVEL"); /* GNUTLS proprietary setting */
    if (val  &&  *val) {
        ELOG_Level level;
        s_GnuTlsLogLevel = atoi(val);
        CORE_UNLOCK;
        if (s_GnuTlsLogLevel) {
            gnutls_global_set_log_function(x_GnuTlsLogger);
            if (val == buf)
                gnutls_global_set_log_level(s_GnuTlsLogLevel);
            level = eLOG_Note;
        } else
            level = eLOG_Trace;
        CORE_LOGF_X(27, level, ("GNUTLS V%s (LogLevel=%d)",
                                version, s_GnuTlsLogLevel));
    } else
        CORE_UNLOCK;

    CORE_DEBUG_ARG(if (s_GnuTlsLogLevel))
        CORE_TRACE("GnuTlsInit(): Go-on");

    if ((status = x_InitLocking()) != eIO_Success)
        goto out;

    if (!gnutls_check_version(LIBGNUTLS_VERSION)  ||
        gnutls_global_init() != GNUTLS_E_SUCCESS/*0*/) {
        status = eIO_NotSupported;
        goto out;
    }
    if (gnutls_anon_allocate_client_credentials(&acred) != 0) {
        gnutls_global_deinit();
        status = eIO_Unknown;
        goto out;
    }
    if (gnutls_certificate_allocate_credentials(&xcred) != 0) {
        gnutls_anon_free_client_credentials(acred);
        gnutls_global_deinit();
        status = eIO_Unknown;
        goto out;
    }
    assert(acred  &&  xcred);

    s_GnuTlsCredAnon = acred;
    s_GnuTlsCredCert = xcred;
    s_Pull           = pull;
    s_Push           = push;

    CORE_DEBUG_ARG(if (s_GnuTlsLogLevel))
        CORE_TRACE("GnuTlsInit(): Leave");

    return eIO_Success;

 out:
    gnutls_global_set_log_level(s_GnuTlsLogLevel = 0);
    gnutls_global_set_log_function(0);
    return status;
}


/* NB: Called under a lock */
static void s_GnuTlsExit(void)
{
    gnutls_anon_client_credentials_t acred = s_GnuTlsCredAnon;
    gnutls_certificate_credentials_t xcred = s_GnuTlsCredCert;

    assert(acred  &&  xcred);

    CORE_DEBUG_ARG(if (s_GnuTlsLogLevel))
        CORE_TRACE("GnuTlsExit(): Enter");

    s_Push           = 0;
    s_Pull           = 0;
    s_GnuTlsCredCert = 0;
    s_GnuTlsCredAnon = 0;

    gnutls_certificate_free_credentials(xcred);
    gnutls_anon_free_client_credentials(acred);
    gnutls_global_deinit();

    gnutls_global_set_log_level(s_GnuTlsLogLevel = 0);
    gnutls_global_set_log_function(0);

    /* If GNUTLS is loaded as a DLL, it still has init count 1, so make sure
     * cleanup worked completely (MSVC2015 ReleaseDLL build breaks if not) */
    gnutls_global_deinit();

    CORE_TRACE("GnuTlsExit(): Leave");
}


/*ARGSUSED*/
static const char* s_GnuTlsError(void* session, int error,
                                 char* buf/*unused*/, size_t size/*unused*/)
{
    /* GNUTLS defines only negative error codes */
    return error >= 0 ? 0 :
        error == GNUTLS_E_WARNING_ALERT_RECEIVED  ||
        error == GNUTLS_E_FATAL_ALERT_RECEIVED
        ? gnutls_alert_get_strname(gnutls_alert_get((gnutls_session_t)session))
        : gnutls_strerror(error);
}

#else

/*ARGSUSED*/
static EIO_Status s_GnuTlsInit(FSSLPull unused_pull, FSSLPush unused_push)
{
    CORE_LOG_X(28, eLOG_Critical, "Unavailable feature GNUTLS");
    return eIO_NotSupported;
}

#endif /*HAVE_LIBGNUTLS*/


extern SOCKSSL NcbiSetupGnuTls(void)
{
    static const struct SOCKSSL_struct kGnuTlsOps = {
        "GNUTLS"
        , s_GnuTlsInit
#ifdef HAVE_LIBGNUTLS
        , s_GnuTlsCreate
        , s_GnuTlsOpen
        , s_GnuTlsRead
        , s_GnuTlsWrite
        , s_GnuTlsClose
        , s_GnuTlsDelete
        , s_GnuTlsExit
        , s_GnuTlsError
#endif /*HAVE_LIBGNUTLS*/
    };
#ifndef HAVE_LIBGNUTLS
    CORE_LOG_X(29, eLOG_Warning, "Unavailable feature GNUTLS");
#endif /*!HAVE_LIBGNUTLS*/
    return &kGnuTlsOps;
}


extern NCBI_CRED NcbiCredGnuTls(void* xcred)
{
    struct SNcbiCred* cred = (NCBI_CRED) calloc(xcred ? 2 : 1, sizeof(*cred));
    if (cred) {
        cred->type = eNcbiCred_GnuTls;
        cred->data = xcred;
    }
    return cred;
}


#ifdef HAVE_LIBGNUTLS

static gnutls_x509_crt_fmt_t x_GnuTlsX509Format(const gnutls_datum_t* data)
{
    assert(data);
    /* A la the logic in MbedTLS */
    if (data->size  &&  data->data  &&  !data->data[data->size - 1]
        &&  strstr((const char*) data->data, "-----BEGIN ")) {
        return GNUTLS_X509_FMT_PEM;
    }
    return GNUTLS_X509_FMT_DER;
}


void NcbiDeleteGnuTlsCertCredentials(NCBI_CRED cred)
{
    if (cred->type / 100 == eNcbiCred_GnuTls / 100  &&  !(cred->type % 100)) {
        if (cred->data) {
            gnutls_certificate_free_credentials
                ((gnutls_certificate_credentials_t) cred->data);
            cred->data = 0;
        }
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
        CORE_LOGF_X(30, eLOG_Critical,
                    ("Deleting unknown certificate credentials (%s/%u)",
                     who, cred->type % 100));
        assert(0);
    }
    cred->type = (ENcbiCred) 0;
    free(cred);
}


NCBI_CRED NcbiCreateGnuTlsCertCredentials(const void* cert,
                                          size_t      certsz,
                                          const void* pkey,
                                          size_t      pkeysz)
{
    struct SNcbiCred*                 ncbi_cred;
    gnutls_certificate_credentials_t* gtls_xcred;
    gnutls_x509_crt_t                 gtls_cert = 0;
    gnutls_x509_privkey_t             gtls_pkey = 0;
    gnutls_datum_t data;
    int err;

    if (!(ncbi_cred = (NCBI_CRED) calloc(2, sizeof(*ncbi_cred)))) {
        CORE_LOGF_ERRNO_X(31, eLOG_Error, errno,
                          ("Cannot allocate NCBI_CRED (%lu bytes)",
                           (unsigned long)(2 * sizeof(*ncbi_cred))));
        return 0;
    }

    ncbi_cred->type = eNcbiCred_GnuTls;
    gtls_xcred = (gnutls_certificate_credentials_t*) &ncbi_cred->data;

    err = gnutls_certificate_allocate_credentials(gtls_xcred);
    if (err) {
        CORE_LOG_ERRNO_EXX(32, eLOG_Error, err, gnutls_strerror(err),
                           "GNUTLS cannot allocate certificate credentials");
        goto out;
    }

    data.data = (unsigned char*) cert;
    data.size = certsz ? certsz : strlen((const char*) cert) + 1;
    if ((err = gnutls_x509_crt_init (&gtls_cert)) != 0  ||
        (err = gnutls_x509_crt_import(gtls_cert, &data,
                                      x_GnuTlsX509Format(&data))) != 0) {
        CORE_LOGF_ERRNO_EXX(33, eLOG_Error, err, gnutls_strerror(err),
                            ("GNUTLS cannot %s X.509 certificate",
                             gtls_cert ? "build" : "parse"));
        goto out;
    }

    data.data = (unsigned char*) pkey;
    data.size = pkeysz ? pkeysz : strlen((const char*) pkey) + 1;
    if ((err = gnutls_x509_privkey_init (&gtls_pkey)) != 0  ||
        (err = gnutls_x509_privkey_import(gtls_pkey, &data,
                                          x_GnuTlsX509Format(&data))) != 0) {
        CORE_LOGF_ERRNO_EXX(34, eLOG_Error, err, gnutls_strerror(err),
                            ("GNUTLS cannot %s private key",
                             gtls_pkey ? "build" : "parse"));
        goto out;
    }

    err = gnutls_certificate_set_x509_key(*gtls_xcred,
                                          &gtls_cert, 1, gtls_pkey);
    if (err) {
        CORE_LOG_ERRNO_EXX(35, eLOG_Error, err, gnutls_strerror(err),
                           "GNUTLS cannot setup certificate credentials");
        goto out;
    }

    /* once they have been copied into xcred, they can be safely disposed of */
    gnutls_x509_crt_deinit    (gtls_cert);
    gnutls_x509_privkey_deinit(gtls_pkey);

    return ncbi_cred;

 out:
    if (gtls_cert)
        gnutls_x509_crt_deinit    (gtls_cert);
    if (gtls_pkey)
        gnutls_x509_privkey_deinit(gtls_pkey);
    NcbiDeleteGnuTlsCertCredentials(ncbi_cred);
    return 0;
}

#endif /*HAVE_LIBGNUTLS*/
