#ifndef CONNECT___NCBI_PRIV__H
#define CONNECT___NCBI_PRIV__H

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
 * Authors:  Denis Vakatov, Anton Lavrentiev, Pavel Ivanov
 *
 * File Description:
 *    Private aux. code for the "ncbi_*.[ch]"
 *
 *********************************
 * Error handling and logging (including tracing):
 *    C error codes:   NCBI_C_DEFINE_ERRCODE_X, NCBI_C_ERRCODE_X
 *    private global:  g_CORE_Log
 *    macros:          CORE_TRACE[F], CORE_LOG[F][_[EX]X],
 *                     CORE_LOG[F]_ERRNO[_[EX]X](), CORE_DATA[F][_[EX]X]
 * Multi-thread safety (critical section basic MT synchronization):
 *    private globals: g_CORE_MT_Lock[_default]
 *    macros:          CORE_LOCK_WRITE, CORE_LOCK_READ, CORE_UNLOCK
 * Registry:
 *    private global:  g_CORE_Registry
 *    macros:          CORE_REG_GET, CORE_REG_SET
 * Setup accounting:   ECORE_Set
 *    private global:  g_CORE_Set
 * Random generator seeding support
 *    private global:  g_NCBI_ConnectRandomSeed
 *    macro:           NCBI_CONNECT_SRAND_ADDEND
 * App name / NCBI ID / DTab support
 *    private globals: g_CORE_GetAppName
 *                     g_CORE_GetReferer
 *                     g_CORE_GetRequestID
 *                     g_CORE_GetRequestDtab
 *                     g_CORE_SkipPostForkChildUnlock
 *
 */

/*#define NCBI_MONKEY*/

#include "ncbi_assert.h"
#include <connect/ncbi_util.h>
#ifdef NCBI_MONKEY
#  ifdef NCBI_OS_MSWIN
#    include <WinSock2.h>
#  else
#    include <sys/socket.h>
#  endif /*NCBI_OS_MSWIN*/
#endif /*NCBI_MONKEY*/


#ifdef __cplusplus
extern "C" {
#endif


/******************************************************************************
 *  Error handling and logging
 *
 * Several macros brought here from ncbidiag.hpp.  The names slightly
 * changed (added _C) because some sources can include this header and
 * ncbidiag.hpp simultaneously.
 */

/** Define global error code name with given value (err_code) */
#define NCBI_C_DEFINE_ERRCODE_X(name, err_code, max_err_subcode)        \
    enum enum##name {                                                   \
        eErrCodeX_##name = err_code                                     \
        /* automatic subcode checking is not implemented in C code */   \
    }

/* Here are only error codes used in C sources.  For error codes used in
 * C++ sources (in C++ Toolkit) see include/connect/error_codes.hpp.
 */
NCBI_C_DEFINE_ERRCODE_X(Connect_Conn,          301,  37);
NCBI_C_DEFINE_ERRCODE_X(Connect_Socket,        302, 166);
NCBI_C_DEFINE_ERRCODE_X(Connect_Util,          303,  15);
NCBI_C_DEFINE_ERRCODE_X(Connect_LBSM,          304,  92);
NCBI_C_DEFINE_ERRCODE_X(Connect_FTP,           305,  14);
NCBI_C_DEFINE_ERRCODE_X(Connect_SMTP,          306,  33);
NCBI_C_DEFINE_ERRCODE_X(Connect_HTTP,          307,  26);
NCBI_C_DEFINE_ERRCODE_X(Connect_Service,       308,  10);
NCBI_C_DEFINE_ERRCODE_X(Connect_HeapMgr,       309,  34);
NCBI_C_DEFINE_ERRCODE_X(Connect_TLS,           310,  50);  /* mbedTLS: 1-20; GNUTLS: 21-40; TLS: 41-50 */
NCBI_C_DEFINE_ERRCODE_X(Connect_Mghbn,         311,  16);
NCBI_C_DEFINE_ERRCODE_X(Connect_Crypt,         312,   5);
NCBI_C_DEFINE_ERRCODE_X(Connect_LocalIP,       313,   5);
NCBI_C_DEFINE_ERRCODE_X(Connect_NamerdLinkerd, 314,  14);


/** Make one identifier from 2 parts */
#define NCBI_C_CONCAT_IDENTIFIER(prefix, postfix)  prefix##postfix

/** Return value of error code by its name defined by NCBI_DEFINE_ERRCODE_X
 *
 * @sa NCBI_C_DEFINE_ERRCODE_X
 */
#define NCBI_C_ERRCODE_X_NAME(name)             \
    NCBI_C_CONCAT_IDENTIFIER(eErrCodeX_, name)

/** Return currently set default error code.  Default error code is set by
 *  definition of NCBI_USE_ERRCODE_X with name of error code as its value.
 *
 * @sa NCBI_DEFINE_ERRCODE_X
 */
#define NCBI_C_ERRCODE_X                        \
    NCBI_C_ERRCODE_X_NAME(NCBI_USE_ERRCODE_X)


extern NCBI_XCONNECT_EXPORT LOG g_CORE_Log;

/* Always use the following macros and functions to access "g_CORE_Log",
 * do not access/change it directly!
 */

#ifdef _DEBUG
#  define CORE_TRACE(message)    CORE_LOG(eLOG_Trace, message)
#  define CORE_TRACEF(fmt_args)  CORE_LOGF(eLOG_Trace, fmt_args)
#  define CORE_DEBUG_ARG(arg)    arg
#else
#  define CORE_TRACE(message)    ((void) 0)
#  define CORE_TRACEF(fmt_args)  ((void) 0)
#  define CORE_DEBUG_ARG(arg)    /*arg*/
#endif /*_DEBUG*/

#define CORE_LOG_X(subcode, level, message)                             \
    DO_CORE_LOG(NCBI_C_ERRCODE_X, subcode, level,                       \
                message, 0)

#define CORE_LOGF_X(subcode, level, fmt_args)                           \
    DO_CORE_LOG(NCBI_C_ERRCODE_X, subcode, level,                       \
                g_CORE_Sprintf fmt_args, 1)

#define CORE_LOG(level, message)                                        \
    DO_CORE_LOG(0, 0, level,                                            \
                message, 0)

#define CORE_LOGF(level, fmt_args)                                      \
    DO_CORE_LOG(0, 0, level,                                            \
                g_CORE_Sprintf fmt_args, 1)

#define CORE_LOG_ERRNO_X(subcode, level, error, message)                \
    DO_CORE_LOG_ERRNO(NCBI_C_ERRCODE_X, subcode, level, error, 0,       \
                      message, 0)

#define CORE_LOGF_ERRNO_X(subcode, level, error, fmt_args)              \
    DO_CORE_LOG_ERRNO(NCBI_C_ERRCODE_X, subcode, level, error, 0,       \
                      g_CORE_Sprintf fmt_args, 1)

#define CORE_LOG_ERRNO(level, error, message)                           \
    DO_CORE_LOG_ERRNO(0, 0, level, error, 0,                            \
                      message, 0)

#define CORE_LOGF_ERRNO(level, error, fmt_args)                         \
    DO_CORE_LOG_ERRNO(0, 0, level, error, 0,                            \
                      g_CORE_Sprintf fmt_args, 1)

#define CORE_LOG_ERRNO_EXX(subcode, level, error, descr, message)       \
    DO_CORE_LOG_ERRNO(NCBI_C_ERRCODE_X, subcode, level, error, descr,   \
                      message, 0)

#define CORE_LOGF_ERRNO_EXX(subcode, level, error, descr, fmt_args)     \
    DO_CORE_LOG_ERRNO(NCBI_C_ERRCODE_X, subcode, level, error, descr,   \
                      g_CORE_Sprintf fmt_args, 1)

#define CORE_LOG_ERRNO_EX(level, error, descr, message)                 \
    DO_CORE_LOG_ERRNO(0, 0, level, error, descr,                        \
                      message, 0)

#define CORE_LOGF_ERRNO_EX(level, error, descr, fmt_args)               \
    DO_CORE_LOG_ERRNO(0, 0, level, error, descr,                        \
                      g_CORE_Sprintf fmt_args, 1)

#define CORE_DATA_X(subcode, level, data, size, message)                \
    DO_CORE_LOG_DATA(NCBI_C_ERRCODE_X, subcode, level, data, size,      \
                     message, 0)

#define CORE_DATAF_X(subcode, level, data, size, fmt_args)              \
    DO_CORE_LOG_DATA(NCBI_C_ERRCODE_X, subcode, level, data, size,      \
                     g_CORE_Sprintf fmt_args, 1)

#define CORE_DATA(level, data, size, message)                           \
    DO_CORE_LOG_DATA(0, 0, level, data, size,                           \
                     message, 0)

#define CORE_DATAF(level, data, size, fmt_args)                         \
    DO_CORE_LOG_DATA(0, 0, level, data, size,                           \
                     g_CORE_Sprintf fmt_args, 1)

/* helpers follow */
#define DO_CORE_LOG_X(_code, _subcode, _level, _message, _dynamic,      \
                      _error, _descr, _raw_data, _raw_size)             \
    do {                                                                \
        ELOG_Level _xx_level = (_level);                                \
        if (g_CORE_Log  ||  _xx_level == eLOG_Fatal) {                  \
            SLOG_Message _mess;                                         \
            _mess.dynamic     = _dynamic;                               \
            _mess.message     = NcbiMessagePlusError(&_mess.dynamic,    \
                                                     (_message),        \
                                                     (_error),          \
                                                     (_descr));         \
            _mess.level       = _xx_level;                              \
            _mess.module      = THIS_MODULE;                            \
            _mess.func        = CORE_CURRENT_FUNCTION;                  \
            _mess.file        = __FILE__;                               \
            _mess.line        = __LINE__;                               \
            _mess.raw_data    = (_raw_data);                            \
            _mess.raw_size    = (_raw_size);                            \
            _mess.err_code    = (_code);                                \
            _mess.err_subcode = (_subcode);                             \
            CORE_LOCK_READ;                                             \
            LOG_WriteInternal(g_CORE_Log, &_mess);                      \
            CORE_UNLOCK;                                                \
        }                                                               \
    } while (0)

#define DO_CORE_LOG(code, subcode, level,                               \
                          message, dynamic)                             \
    DO_CORE_LOG_X(code, subcode, level, message, dynamic, 0, 0, 0, 0)

#define DO_CORE_LOG_ERRNO(code, subcode, level, error, descr,           \
                          message, dynamic)                             \
    DO_CORE_LOG_X(code, subcode, level, message, dynamic, error, descr, 0, 0)

#define DO_CORE_LOG_DATA(code, subcode, level, data, size,              \
                         message, dynamic)                              \
    DO_CORE_LOG_X(code, subcode, level, message, dynamic, 0, 0, data, size)

extern NCBI_XCONNECT_EXPORT const char* g_CORE_Sprintf(const char* fmt, ...)
#ifdef __GNUC__
    __attribute__((format(printf,1,2)))
#endif /*__GNUC__*/
;


/******************************************************************************
 *  Multi-thread safety
 */

extern struct MT_LOCK_tag g_CORE_MT_Lock_default;

extern NCBI_XCONNECT_EXPORT MT_LOCK g_CORE_MT_Lock;


/* Always use the following macros and functions to access "g_CORE_MT_Lock",
 * do not access/change it directly!
 */

#define CORE_LOCK_WRITE  verify(CORE_CHECK_LOCK   != 0  &&              \
                                MT_LOCK_Do(g_CORE_MT_Lock, eMT_Lock    ) != 0)
#define CORE_LOCK_READ   verify(CORE_CHECK_LOCK   != 0  &&              \
                                MT_LOCK_Do(g_CORE_MT_Lock, eMT_LockRead) != 0)
#define CORE_UNLOCK      verify(CORE_CHECK_UNLOCK != 0  &&              \
                                MT_LOCK_Do(g_CORE_MT_Lock, eMT_Unlock  ) != 0)

#ifdef _DEBUG
extern NCBI_XCONNECT_EXPORT int g_NCBI_CoreCheckLock  (void);
extern NCBI_XCONNECT_EXPORT int g_NCBI_CoreCheckUnlock(void);
#  define CORE_CHECK_LOCK       g_NCBI_CoreCheckLock()
#  define CORE_CHECK_UNLOCK     g_NCBI_CoreCheckUnlock()
#else
#  define CORE_CHECK_LOCK       (1/*TRUE*/)
#  define CORE_CHECK_UNLOCK     (1/*TRUE*/)
#endif /*_DEBUG*/


/******************************************************************************
 *  Registry
 */

extern NCBI_XCONNECT_EXPORT REG g_CORE_Registry;

/* Always use the following macros to access "g_CORE_Registry",
 * do not access/change it directly!
 */

#define CORE_REG_GET(section, name, value, value_size, def_value)   \
    g_CORE_RegistryGET(section, name, value, value_size, def_value)
    
#define CORE_REG_SET(section, name, value, storage)                 \
    g_CORE_RegistrySET(section, name, value, storage)

/* (private, to be used exclusively by the above macro CORE_REG_GET) */
extern NCBI_XCONNECT_EXPORT const char* g_CORE_RegistryGET
(const char*  section,
 const char*  name,
 char*        value,
 size_t       value_size,
 const char*  def_value
 );

/* (private, to be used exclusively by the above macro CORE_REG_SET) */
extern NCBI_XCONNECT_EXPORT int/*bool*/ g_CORE_RegistrySET
(const char*  section,
 const char*  name,
 const char*  value,
 EREG_Storage storage
 );


/******************************************************************************
 *  Setup accounting
 */

typedef enum {
    eCORE_SetSSL  = 1,
    eCORE_SetREG  = 2,
    eCORE_SetLOG  = 4,
    eCORE_SetLOCK = 8
} ECORE_Set;
typedef unsigned int TCORE_Set;

/* Track settings modified from the user-land */
extern TCORE_Set g_CORE_Set;


/******************************************************************************
 *  Random generator seeding support
 */

extern NCBI_XCONNECT_EXPORT unsigned int g_NCBI_ConnectRandomSeed;
extern NCBI_XCONNECT_EXPORT unsigned int g_NCBI_ConnectSrandAddend(void);
#define NCBI_CONNECT_SRAND_ADDEND        g_NCBI_ConnectSrandAddend()


/******************************************************************************
 *  App name support (may return NULL; gets converted to "" at the user level)
 *  The name is assumed to have static storage (not to be free()'d after use).
 */

typedef const char* (*FNcbiGetAppName)(void);
extern NCBI_XCONNECT_EXPORT FNcbiGetAppName g_CORE_GetAppName;


/******************************************************************************
 *  Referer support (return NULL or non-empty dynamically malloc()'ed string)
 */

typedef const char* (*FNcbiGetReferer)(void);
extern NCBI_XCONNECT_EXPORT FNcbiGetReferer g_CORE_GetReferer;


/******************************************************************************
 *  NCBI request ID support (return "as is" to the user)
 *  Return NULL on error;  otherwise, the returned ID is a non-empty string
 *  allocated on heap, and must be free()'d when no longer needed.
 */

typedef char* (*FNcbiGetRequestID)(ENcbiRequestID);
extern NCBI_XCONNECT_EXPORT FNcbiGetRequestID g_CORE_GetRequestID;


/******************************************************************************
 *  DTab-Local support (returned NULL gets converted to "" at the user level)
 */
typedef const char* (*FNcbiGetRequestDtab)(void);
extern NCBI_XCONNECT_EXPORT FNcbiGetRequestDtab g_CORE_GetRequestDtab;


/******************************************************************************
 * Prevent pthread_atfork() from unlocking in the child fork handler.  This may
 * be necessary if the parent is an MT process and the child is headed for exec()
 * (as in this case the repertoir of calls limited to only async-signal-safe ones
 * because the child might have inherited inconsistent locks due to activity
 * in the other threads -- so having unlock in place can lead to a deadlock).
 */
extern int/*bool*/ g_CORE_SkipPostForkChildUnlock/* = 0(Off) by default*/;



/******************************************************************************
 *  Miscellanea
 */

#ifdef __GNUC__
#  define likely(x)    __builtin_expect(!!(x),1)
#  define unlikely(x)  __builtin_expect(!!(x),0)
#else
#  define likely(x)    (x)
#  define unlikely(x)  (x)
#endif /*__GNUC__*/



/******************************************************************************
 *  NCBI Crazy Monkey support
 */

#ifdef NCBI_MONKEY
/* UNIX and Windows have different prototypes for send(), recv(), etc., so
 * some types have to be pre-selected based on current OS
 */
#   ifdef NCBI_OS_MSWIN
#       define MONKEY_RETTYPE      int
#       define MONKEY_SOCKTYPE     SOCKET
#       define MONKEY_DATATYPE     char*
#       define MONKEY_LENTYPE      int
#       define MONKEY_SOCKLENTYPE  int
#       define MONKEY_STDCALL      __stdcall /*in Windows, socket functions
                                               have prototypes with __stdcall*/
#   else
#       define MONKEY_RETTYPE      ssize_t
#       define MONKEY_SOCKTYPE     int
#       define MONKEY_DATATYPE     void*
#       define MONKEY_LENTYPE      size_t
#       define MONKEY_SOCKLENTYPE  socklen_t
#       define MONKEY_STDCALL      /*empty*/ 
#   endif /*NCBI_OS_MSWIN*/

/******************************************************************************
 *  Socket functions via Crazy Monkey
 */
typedef MONKEY_RETTYPE
           (MONKEY_STDCALL *FMonkeyRecv)   (MONKEY_SOCKTYPE        socket,
                                            MONKEY_DATATYPE        buf,
                                            MONKEY_LENTYPE         size,
                                            int                    flags,
                                            void* /* SOCK* */      sock_ptr);
typedef MONKEY_RETTYPE
           (MONKEY_STDCALL *FMonkeySend)   (MONKEY_SOCKTYPE        socket,
                                            const MONKEY_DATATYPE  data,
                                            MONKEY_LENTYPE         size,
                                            int                    flags,
                                            void* /* SOCK* */      sock_ptr);
typedef int(MONKEY_STDCALL *FMonkeyConnect)(MONKEY_SOCKTYPE        socket,
                                            const struct sockaddr* name,
                                            MONKEY_SOCKLENTYPE     namelen);

typedef int /*bool*/      (*FMonkeyPoll)   (size_t*                n,
                                            void* /*SSOCK_Poll[]**/polls,
                                            EIO_Status*            ret_status);
typedef void              (*FMonkeyClose)  (MONKEY_SOCKTYPE        socket); 
typedef void              (*FSockHasSocket)(void* /* SOCK */       sock, 
                                            MONKEY_SOCKTYPE        socket); 


extern NCBI_XCONNECT_EXPORT FMonkeySend     g_MONKEY_Send;
extern NCBI_XCONNECT_EXPORT FMonkeyRecv     g_MONKEY_Recv;
extern NCBI_XCONNECT_EXPORT FMonkeyPoll     g_MONKEY_Poll;
extern NCBI_XCONNECT_EXPORT FMonkeyConnect  g_MONKEY_Connect;
extern NCBI_XCONNECT_EXPORT FMonkeyClose    g_MONKEY_Close;
extern NCBI_XCONNECT_EXPORT FSockHasSocket  g_MONKEY_SockHasSocket;
#endif /*NCBI_MONKEY*/


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* CONNECT___NCBI_PRIV__H */
