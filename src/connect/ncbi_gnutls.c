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
#include <connect/ncbi_connutil.h>
#include <connect/ncbi_gnutls.h>
#include <stdlib.h>

#ifdef HAVE_LIBGNUTLS

#  include <gnutls/gnutls.h>

#  if   defined(ENOTSUP)
#    define NCBI_NOTSUPPORTED  ENOTSUP
#  elif defined(ENOSYS)
#    define NCBI_NOTSUPPORTED  ENOSYS
#  else
#    define NCBI_NOTSUPPORTED  EINVAL
#  endif /*not implemented*/

#  ifdef HAVE_LIBGCRYPT

#    include <gcrypt.h>

#    if   defined(NCBI_POSIX_THREADS)

#      include <pthread.h>
#      ifdef __cplusplus
extern "C" {
#      endif /*__cplusplus*/
    GCRY_THREAD_OPTION_PTHREAD_IMPL;
#      ifdef __cplusplus
} /* extern "C" */
#      endif /*__cplusplus*/

#    elif defined(NCBI_THREADS)

#      ifdef __cplusplus
extern "C" {
#      endif /*__cplusplus*/
static int gcry_user_mutex_init(void** lock)
{
    return !(*lock = CORE_GetLOCK()) ? NCBI_NOTSUPPORTED : 0;
}
static int gcry_user_mutex_destroy(void** lock)
{
    *lock = 0;
    return 0;
}
static int gcry_user_mutex_lock(void** lock)
{
    return MT_LOCK_Do((MT_LOCK)(*lock), eMT_Lock) > 0 ? 0 : NCBI_NOTSUPPORTED;
}
static int gcry_user_mutex_unlock(void** lock)
{
    return MT_LOCK_Do((MT_LOCK)(*lock), eMT_Unlock) ? 0 : NCBI_NOTSUPPORTED;
}
static struct gcry_thread_cbs gcry_threads_user = {
    GCRY_THREAD_OPTION_USER, NULL/*gcry_user_init*/,
    gcry_user_mutex_init, gcry_user_mutex_destroy,
    gcry_user_mutex_lock, gcry_user_mutex_unlock,
    NULL/*all other fields NULL-inited*/
};
#      ifdef __cplusplus
} /* extern "C" */
#      endif /*__cplusplus*/

#    endif /*NCBI_POSIX_THREADS*/

#  endif /*HAVE_LIBGCRYPT*/

#  ifndef LIBGNUTLS_VERSION_NUMBER
/* backport support for older gnutls */
#    define gnutls_session_t                  gnutls_session
#    define gnutls_transport_ptr_t            gnutls_transport_ptr
#    define gnutls_connection_end_t           gnutls_connection_end
#    define gnutls_anon_client_credentials_t  gnutls_anon_client_credentials
#    define gnutls_certificate_credentials_t  gnutls_certificate_credentials
#  endif /*LIBGNUTLS_VERSION_NUMBER*/


#  ifdef __cplusplus
extern "C" {
#  endif /*__cplusplus*/

static EIO_Status  s_GnuTlsInit  (FSSLPull pull, FSSLPush push);
static void*       s_GnuTlsCreate(ESOCK_Side side, SOCK sock,
                                  NCBI_CRED cred, int* error);
static EIO_Status  s_GnuTlsOpen  (void* session, int* error);
static EIO_Status  s_GnuTlsRead  (void* session,       void* buf,  size_t size,
                                  size_t* done, int* error);
static EIO_Status  s_GnuTlsWrite (void* session, const void* data, size_t size,
                                  size_t* done, int* error);
static EIO_Status  s_GnuTlsClose (void* session, int how, int* error);
static void        s_GnuTlsDelete(void* session);
static void        s_GnuTlsExit  (void);
static const char* s_GnuTlsError (void* session, int error);

static void        x_GnuTlsLogger(int level, const char* message);
static ssize_t     x_GnuTlsPull  (gnutls_transport_ptr_t,       void*, size_t);
static ssize_t     x_GnuTlsPush  (gnutls_transport_ptr_t, const void*, size_t);

#  ifdef __cplusplus
}
#  endif /*__cplusplus*/


#  if defined(LIBGNUTLS_VERSION_NUMBER)  ||  !defined(NCBI_OS_SOLARIS)
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
#  endif /*LIBGNUTLS_VERSION_NUMBER || !NCBI_OS_SOLARIS*/


static int                              s_GnuTlsLogLevel;
static gnutls_anon_client_credentials_t s_GnuTlsCredAnon;
static gnutls_certificate_credentials_t s_GnuTlsCredCert;
static FSSLPull                         s_Pull;
static FSSLPush                         s_Push;


static void x_GnuTlsLogger(int level, const char* message)
{
    /* do some basic filtering and EOL cut-offs */
    int len = message ? strlen(message) : 0;
    if (!len  ||  *message == '\n')
        return;
    if (strncasecmp(message, "ASSERT: ", 8) == 0)
        return;
    if (message[len - 1] == '\n')
        len--;
    CORE_LOGF(eLOG_Note, ("GNUTLS%d: %.*s", level, len, message));
}


#  ifdef __GNUC__
inline
#  endif /*__GNUC__*/
static int/*bool*/ x_IsTimeout(SOCK sock, EIO_Event direction)
{
    int retval;
    switch (direction) {
    case eIO_Read:
        retval = !sock->r_tv_set  ||  (sock->r_tv.tv_sec | sock->r_tv.tv_usec);
        break;
    case eIO_Write:
        retval = !sock->w_tv_set  ||  (sock->w_tv.tv_sec | sock->w_tv.tv_usec);
        break;
    default:
        retval = 0;
        assert(0);
        break;
    }
    return retval;
}


#  ifdef __GNUC__
inline
#  endif /*__GNUC__*/
static int x_StatusToError(EIO_Status status, SOCK sock, EIO_Event direction)
{
    int error;

    assert(status != eIO_Success);

    switch (status) {
    case eIO_Timeout:
        error = x_IsTimeout(sock, direction) ? SOCK_ETIMEDOUT : EAGAIN;
        break;
    case eIO_Closed:
        error = SOCK_ENOTCONN;
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
    default:
        /*NB:eIO_InvalidArg*/
        error = EINVAL;
        break;
    }
#if 0
    CORE_TRACEF(("CONNECT status %s -> errno %d%s", IO_StatusStr(status),
                 error ? error : errno,
                 error ? ""    : " (kept)"));
#endif
    return error;
}


#  ifdef __GNUC__
inline
#  endif /*__GNUC__*/
static EIO_Status x_RetryStatus(gnutls_session_t session, EIO_Event direction)
{
    SOCK sock = (SOCK) gnutls_session_get_ptr(session);
    EIO_Status status;
    if (direction == eIO_Open) {
        EIO_Status r_status = SOCK_Status(sock, eIO_Read);
        EIO_Status w_status = SOCK_Status(sock, eIO_Write);
        status = r_status > w_status ? r_status : w_status;
    } else
        status = SOCK_Status(sock, direction);
    return status == eIO_Success ? eIO_Timeout : status;
}


#  ifdef __GNUC__
inline
#  endif /*__GNUC__*/
static EIO_Status x_ErrorToStatus(int error,
                                  gnutls_session_t session,EIO_Event direction)
{
    EIO_Status status;

    assert(error <= 0);

    if (!error)
        return eIO_Success;
    else if (error == GNUTLS_E_AGAIN)
        status = x_RetryStatus(session, direction);
    else if (error == GNUTLS_E_INTERRUPTED)
        status = eIO_Interrupt;
    else if (gnutls_error_is_fatal(error))
        status = eIO_Closed;
    else
        status = eIO_Unknown;
#if 0
    CORE_TRACEF(("GNUTLS error %d -> CONNECT status %s",
                 error, IO_StatusStr(status)));
#endif
    return status;
}


static void* s_GnuTlsCreate(ESOCK_Side side, SOCK sock,
                            NCBI_CRED cred, int* error)
{
    gnutls_transport_ptr_t  ptr = (gnutls_transport_ptr_t) sock;
    gnutls_connection_end_t end = (side == eSOCK_Client
                                   ? GNUTLS_CLIENT
                                   : GNUTLS_SERVER);
    gnutls_session_t session;
    char val[128];
    int err;

    if (end == GNUTLS_SERVER) {
        /*FIXME: not yet supported*/
        *error = 0;
        return 0;
    }

    if (cred  &&  (cred->type != eNcbiCred_GnuTls  ||  !cred->data)) {
        /*FIXME: there's a NULL(data)-terminated array of credentials */
        *error = 0;
        return 0;
    }

    ConnNetInfo_GetValue(0, "GNUTLS_PRIORITY", val, sizeof(val), 0);

    if ((*error = gnutls_init(&session, end)) != GNUTLS_E_SUCCESS/*0*/)
        return 0;

    if ((err = gnutls_set_default_priority(session))                   != 0  ||
#  if defined(LIBGNUTLS_VERSION_NUMBER)  ||  !defined(NCBI_OS_SOLARIS)
        ( *val  &&
         (err = gnutls_priority_set_direct(session, val, 0))           != 0) ||
        (!*val  &&
         (err = gnutls_compression_set_priority(session,
                                                kGnuTlsCompPrio))      != 0) ||
        (!*val  &&
         (err = gnutls_certificate_type_set_priority(session,
                                                     kGnuTlsCertPrio)) != 0) ||
#  endif /*LIBGNUTLS_VERSION_NUMBER || !NCBI_OS_SOLARIS*/
        (err = gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE,
                                      cred ? cred->data :
                                      s_GnuTlsCredCert))               != 0  ||
        (err = gnutls_credentials_set(session, GNUTLS_CRD_ANON,
                                      s_GnuTlsCredAnon))               != 0) {
        gnutls_deinit(session);
        *error = err;
        return 0;
    }

    gnutls_transport_set_pull_function(session, x_GnuTlsPull);
    gnutls_transport_set_push_function(session, x_GnuTlsPush);
    gnutls_transport_set_ptr(session, ptr);
    gnutls_session_set_ptr(session, sock);
    return session;
}


static EIO_Status s_GnuTlsOpen(void* session, int* error)
{
    EIO_Status status;
    int x_error = gnutls_handshake((gnutls_session_t) session);

    if (x_error < 0) {
        status = x_ErrorToStatus(x_error,
                                 (gnutls_session_t) session, eIO_Open);
        *error = x_error;
    } else
        status = eIO_Success;

    return status;
}


/*ARGSUSED*/
static void x_set_errno(gnutls_session_t session, int error)
{
#  ifdef LIBGNUTLS_VERSION_NUMBER
#    if                                         \
    LIBGNUTLS_VERSION_MAJOR > 1  ||             \
    LIBGNUTLS_VERSION_MINOR > 5  ||             \
    LIBGNUTLS_VERSION_PATCH > 3
    gnutls_transport_set_errno(session, error);
    return;
#    endif /*LIBGNUTLS_VERSION >= 1.5.4*/
#  endif /*LIBGNUTLS_VERSION_NUMBER*/
    /*NOTREACHED*/
    if (error)
        errno = error;
}


static ssize_t x_GnuTlsPull(gnutls_transport_ptr_t ptr,
                            void* buf, size_t size)
{
    int x_error;
    EIO_Status status;
    SOCK sock = (SOCK) ptr;

    if (s_Pull) {
        size_t x_read = 0;
        status = s_Pull(sock, buf, size, &x_read, s_GnuTlsLogLevel > 7);
        if (x_read > 0  ||  status == eIO_Success) {
            x_set_errno((gnutls_session_t) sock->session, 0);
            return x_read;
        }
    } else
        status = eIO_NotSupported;

    x_error = x_StatusToError(status, sock, eIO_Read);
    if (x_error)
        x_set_errno((gnutls_session_t) sock->session, x_error);
    return -1;
}


static ssize_t x_GnuTlsPush(gnutls_transport_ptr_t ptr,
                            const void* data, size_t size)
{
    int x_error;
    EIO_Status status;
    SOCK sock = (SOCK) ptr;

    if (s_Push) {
        size_t x_written = 0;
        status = s_Push(sock, data, size, &x_written, s_GnuTlsLogLevel > 7);
        if (x_written  ||  (!size  &&  status == eIO_Success)) {
            x_set_errno((gnutls_session_t) sock->session, 0);
            return x_written;
        }
    } else
        status = eIO_NotSupported;

    x_error = x_StatusToError(status, sock, eIO_Write);
    if (x_error)
        x_set_errno((gnutls_session_t) sock->session, x_error);
    return -1;
}


static EIO_Status s_GnuTlsRead(void* session, void* buf, size_t n_todo,
                               size_t* n_done, int* error)
{
    EIO_Status status;
    int        x_read;

    assert(session);

    x_read = gnutls_record_recv((gnutls_session_t) session, buf, n_todo);

    if (x_read <= 0) {
        status = x_ErrorToStatus(x_read,
                                 (gnutls_session_t) session, eIO_Read);
        *error = x_read;
        x_read = 0;
    } else
        status = eIO_Success;

    *n_done = x_read;
    return status;
}


static EIO_Status s_GnuTlsWrite(void* session, const void* data, size_t n_todo,
                                size_t* n_done, int* error)
{
    EIO_Status status;
    int        x_written;

    assert(session);

    x_written = gnutls_record_send((gnutls_session_t) session, data, n_todo);

    if (x_written <= 0) {
        status = x_ErrorToStatus(x_written,
                                 (gnutls_session_t) session, eIO_Write);
        *error = x_written;
        x_written = 0;
    } else
        status = eIO_Success;

    *n_done = x_written;
    return status;
}


static EIO_Status s_GnuTlsClose(void* session, int how, int* error)
{
    int x_error;

    assert(session);

    x_error = gnutls_bye((gnutls_session_t) session,
                         how == SOCK_SHUTDOWN_RDWR
                         ? GNUTLS_SHUT_RDWR
                         : GNUTLS_SHUT_WR);
    if (x_error != GNUTLS_E_SUCCESS) {
        *error = x_error;
        return eIO_Unknown;
    }

    return eIO_Success;
}


static void s_GnuTlsDelete(void* session)
{
    assert(session);

    gnutls_deinit((gnutls_session_t) session);
}


static EIO_Status s_GnuTlsInit(FSSLPull pull, FSSLPush push)
{
    gnutls_anon_client_credentials_t acred;
    gnutls_certificate_credentials_t xcred;
    char val[32];

    assert(!s_GnuTlsCredAnon);

    ConnNetInfo_GetValue(0, "GNUTLS_LOGLEVEL", val, sizeof(val), 0);
    if (*val) {
        int level = atoi(val);
        s_GnuTlsLogLevel = level < 0 ? 0 : level;
        if (s_GnuTlsLogLevel > 0) {
            gnutls_global_set_log_function(x_GnuTlsLogger);
            gnutls_global_set_log_level(s_GnuTlsLogLevel);
            CORE_LOGF(eLOG_Note, ("GNUTLS V%s (Loglevel=%d)",
                                  gnutls_check_version(0), s_GnuTlsLogLevel));
        }
    }

#  ifdef HAVE_LIBGCRYPT
#    if   defined(NCBI_POSIX_THREADS)
    if (gcry_control(GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread) != 0)
        goto errout;
#    elif defined(NCBI_THREADS)
    if (gcry_control(GCRYCTL_SET_THREAD_CBS, &gcry_threads_user) != 0)
        goto errout;
#    elif defined(_MT)
    CORE_LOG(eLOG_Critical,"LIBGCRYPT uninitialized: Unknown threading model");
#    endif /*NCBI_POSIX_THREADS*/
#  endif /*HAVE_LIBGCRYPT*/

    if (!pull  ||  !push  ||  !gnutls_check_version(LIBGNUTLS_VERSION)
        ||  gnutls_global_init() != GNUTLS_E_SUCCESS/*0*/) {
        goto errout;
    }
    if (gnutls_anon_allocate_client_credentials(&acred) != 0) {
        gnutls_global_deinit();
        goto errout;
    }
    if (gnutls_certificate_allocate_credentials(&xcred) != 0) {
        gnutls_anon_free_client_credentials(acred);
        gnutls_global_deinit();
        goto errout;
    }

    s_GnuTlsCredAnon = acred;
    s_GnuTlsCredCert = xcred;
    s_Pull           = pull;
    s_Push           = push;

    return eIO_Success;

 errout:
    gnutls_global_set_log_level(s_GnuTlsLogLevel = 0);
    gnutls_global_set_log_function(0);
    return eIO_NotSupported;
}


static void s_GnuTlsExit(void)
{
    gnutls_anon_client_credentials_t acred = s_GnuTlsCredAnon;
    gnutls_certificate_credentials_t xcred = s_GnuTlsCredCert;

    assert(acred);

    s_Push           = 0;
    s_Pull           = 0;
    s_GnuTlsCredCert = 0;
    s_GnuTlsCredAnon = 0;

    gnutls_certificate_free_credentials(xcred);
    gnutls_anon_free_client_credentials(acred);
    gnutls_global_deinit();

    gnutls_global_set_log_level(s_GnuTlsLogLevel = 0);
    gnutls_global_set_log_function(0);
}

 
static const char* s_GnuTlsError(void* session/*unused*/, int error)
{
    /* GNUTLS defines only negative error codes */
    return error < 0 ? gnutls_strerror(error) : 0;
}


#endif /*HAVE_LIBGNUTLS*/


extern SOCKSSL NcbiSetupGnuTls(void)
{
#ifdef HAVE_LIBGNUTLS
    static const struct SOCKSSL_struct kGnuTlsOps = {
        s_GnuTlsInit,
        s_GnuTlsCreate,
        s_GnuTlsOpen,
        s_GnuTlsRead,
        s_GnuTlsWrite,
        s_GnuTlsClose,
        s_GnuTlsDelete,
        s_GnuTlsExit,
        s_GnuTlsError
    };
    return &kGnuTlsOps;
#else
    CORE_LOG(eLOG_Critical, "Unavailable GNUTLS feature requested");
    return 0;
#endif /*HAVE_LIBGNUTLS*/
}


extern NCBI_CRED NcbiCredGnuTls(void* xcred)
{
    struct SNcbiCred* cred = calloc(xcred ? 2 : 1, sizeof(*cred));
    if (cred  &&  xcred) {
        cred->type = eNcbiCred_GnuTls;
        cred->data = xcred;
    }
    return cred;
}
