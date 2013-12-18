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
 * Tracing and logging
 *    C error codes:   NCBI_C_DEFINE_ERRCODE_X, NCBI_C_ERRCODE_X
 *    private global:  g_CORE_Log
 *    macros:          CORE_TRACE[F], CORE_LOG[F][_[EX]X],
 *                     CORE_LOG[F]_ERRNO[_[EX]X](), CORE_DATA[F][_[EX]X]
 * Critical section (basic multi-thread synchronization)
 *    private global:  g_CORE_MT_Lock
 *    macros:          CORE_LOCK_WRITE, CORE_LOCK_READ, CORE_UNLOCK
 * Registry:
 *    private global:  g_CORE_Registry
 *    macros:          CORE_REG_GET, CORE_REG_SET
 * Random generator seeding support
 *    private global:  g_NCBI_ConnectRandomSeed
 *    macro:           NCBI_CONNECT_SRAND_ADDEND
 * App name and SID support
 *    private globals: g_CORE_GetAppName
 *                     g_CORE_GetSid
 *
 */

#include "ncbi_assert.h"
#include <connect/ncbi_util.h>


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

/* Here are only error codes used in C sources. For error codes used in
 * C++ sources (in C++ Toolkit) see include/connect/error_codes.hpp.
 */
NCBI_C_DEFINE_ERRCODE_X(Connect_Conn,     301,  36);
NCBI_C_DEFINE_ERRCODE_X(Connect_LBSM,     302,  25);
NCBI_C_DEFINE_ERRCODE_X(Connect_Util,     303,   8);
NCBI_C_DEFINE_ERRCODE_X(Connect_Dispd,    304,   2);
NCBI_C_DEFINE_ERRCODE_X(Connect_FTP,      305,  13);
NCBI_C_DEFINE_ERRCODE_X(Connect_HeapMgr,  306,  33);
NCBI_C_DEFINE_ERRCODE_X(Connect_HTTP,     307,  21);
NCBI_C_DEFINE_ERRCODE_X(Connect_LBSMD,    308,   8);
NCBI_C_DEFINE_ERRCODE_X(Connect_Sendmail, 309,  33);
NCBI_C_DEFINE_ERRCODE_X(Connect_Service,  310,   9);
NCBI_C_DEFINE_ERRCODE_X(Connect_Socket,   311, 162);
NCBI_C_DEFINE_ERRCODE_X(Connect_Crypt,    312,   6);
NCBI_C_DEFINE_ERRCODE_X(Connect_LocalNet, 313,   4);
NCBI_C_DEFINE_ERRCODE_X(Connect_Mghbn,    314,  16);

/** Make one identifier from 2 parts */
#define NCBI_C_CONCAT_IDENTIFIER(prefix, postfix) prefix##postfix

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
#define NCBI_C_ERRCODE_X   NCBI_C_ERRCODE_X_NAME(NCBI_USE_ERRCODE_X)


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
        ELOG_Level xx_level = (_level);                                 \
        if (g_CORE_Log  ||  xx_level == eLOG_Fatal) {                   \
            SLOG_Handler _mess;                                         \
            _mess.dynamic     = _dynamic;                               \
            _mess.message     = NcbiMessagePlusError(&_mess.dynamic,    \
                                                     _message,          \
                                                     _error,            \
                                                     _descr);           \
            _mess.level       = xx_level;                               \
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
         __attribute__((format(printf, 1, 2)))
#endif
;


/******************************************************************************
 *  Multi-Thread SAFETY
 */

extern struct MT_LOCK_tag g_CORE_MT_Lock_default;

extern NCBI_XCONNECT_EXPORT MT_LOCK g_CORE_MT_Lock;


/* Always use the following macros and functions to access "g_CORE_MT_Lock",
 * do not access/change it directly!
 */

#define CORE_LOCK_WRITE  verify(CORE_CHECK_LOCK  &&                     \
                                MT_LOCK_Do(g_CORE_MT_Lock, eMT_Lock    ))
#define CORE_LOCK_READ   verify(CORE_CHECK_LOCK  &&                     \
                                MT_LOCK_Do(g_CORE_MT_Lock, eMT_LockRead))
#define CORE_UNLOCK      verify(CORE_CHECK_UNLOCK  &&                   \
                                MT_LOCK_Do(g_CORE_MT_Lock, eMT_Unlock  ))

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

/* Always use the following macros and functions to access "g_CORE_Registry",
 * do not access/change it directly!
 */

#define CORE_REG_GET(section, name, value, value_size, def_value)   \
    g_CORE_RegistryGET(section, name, value, value_size, def_value)
    
#define CORE_REG_SET(section, name, value, storage)  do {           \
    CORE_LOCK_READ;                                                 \
    REG_Set(g_CORE_Registry, section, name, value, storage);        \
    CORE_UNLOCK;                                                    \
} while (0)


/* (private, to be used exclusively by the above macro CORE_REG_GET) */
extern NCBI_XCONNECT_EXPORT const char* g_CORE_RegistryGET
(const char* section,
 const char* name,
 char*       value,
 size_t      value_size,
 const char* def_value
 );


/******************************************************************************
 *  Random generator seeding support
 */

extern NCBI_XCONNECT_EXPORT int   g_NCBI_ConnectRandomSeed;
extern NCBI_XCONNECT_EXPORT int   g_NCBI_ConnectSrandAddend(void);
#define NCBI_CONNECT_SRAND_ADDEND g_NCBI_ConnectSrandAddend()


/******************************************************************************
 *  App name support (may return NULL; gets converted to "" at the user level)
 */

typedef const char* (*FNcbiGetAppName)(void);
extern NCBI_XCONNECT_EXPORT FNcbiGetAppName g_CORE_GetAppName;


/******************************************************************************
 *  NCBI SID support (return "as is" to the user)
 */

typedef const char* (*FNcbiGetSid)(void);
extern NCBI_XCONNECT_EXPORT FNcbiGetSid g_CORE_GetSid;


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* CONNECT___NCBI_PRIV__H */
