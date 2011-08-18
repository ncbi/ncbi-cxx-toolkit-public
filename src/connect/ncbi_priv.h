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
 * Author:  Denis Vakatov
 *
 * File Description:
 *    Private aux. code for the "ncbi_*.[ch]"
 *
 *********************************
 * Random generator seeding support
 *    private global:  g_NCBI_ConnectRandomSeed
 *    macro:           NCBI_CONNECT_SRAND_ADDEND
 * Critical section (basic multi-thread synchronization):
 *    private global:  g_CORE_MT_Lock
 *    macros:          CORE_LOCK_WRITE, CORE_LOCK_READ, CORE_UNLOCK
 * Tracing and logging:
 *    private global:  g_CORE_Log
 *    macros:          CORE_LOG[F](), CORE_DATA[F](), CORE_LOG[F]_ERRNO[_EX]()
 *
 */

#include "ncbi_assert.h"
#include <connect/ncbi_util.h>


#ifdef __cplusplus
extern "C" {
#endif


/******************************************************************************
 *  Random generator seeding support
 */

extern NCBI_XCONNECT_EXPORT int g_NCBI_ConnectRandomSeed;
extern NCBI_XCONNECT_EXPORT int g_NCBI_ConnectSrandAddend(void);
#define NCBI_CONNECT_SRAND_ADDEND g_NCBI_ConnectSrandAddend()


/******************************************************************************
 *  Multi-Thread SAFETY
 */

#ifdef _DEBUG
extern NCBI_XCONNECT_EXPORT int g_NCBI_CoreCheckLock  (void);
extern NCBI_XCONNECT_EXPORT int g_NCBI_CoreCheckUnlock(void);
#  define CORE_CHECK_LOCK   g_NCBI_CoreCheckLock()
#  define CORE_CHECK_UNLOCK g_NCBI_CoreCheckUnlock()
#else
#  define CORE_CHECK_LOCK   1/*TRUE*/
#  define CORE_CHECK_UNLOCK 1/*TRUE*/
#endif


/* Always use the following macros and functions to access "g_CORE_MT_Lock",
 * do not access/change it directly!
 */
extern NCBI_XCONNECT_EXPORT MT_LOCK g_CORE_MT_Lock;

#define CORE_LOCK_WRITE  verify(CORE_CHECK_LOCK  &&                     \
                                MT_LOCK_Do(g_CORE_MT_Lock, eMT_Lock    ))
#define CORE_LOCK_READ   verify(CORE_CHECK_LOCK  &&                     \
                                MT_LOCK_Do(g_CORE_MT_Lock, eMT_LockRead))
#define CORE_UNLOCK      verify(CORE_CHECK_UNLOCK  &&                   \
                                MT_LOCK_Do(g_CORE_MT_Lock, eMT_Unlock  ))


/******************************************************************************
 *  App name support
 */

#define NCBI_CORE_APPNAME_MAXLEN 80
extern NCBI_XCONNECT_EXPORT char g_CORE_AppName[NCBI_CORE_APPNAME_MAXLEN + 1];


/******************************************************************************
 *  ERROR HANDLING and LOGGING
 */

/* Always use the following macros and functions to access "g_CORE_Log",
 * dont access/change it directly!
 */
extern NCBI_XCONNECT_EXPORT LOG g_CORE_Log;

extern NCBI_XCONNECT_EXPORT const char* g_CORE_Sprintf(const char* fmt, ...)
#ifdef __GNUC__
         __attribute__((format(printf, 1, 2)))
#endif
;

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
            _mess.file        = THIS_FILE;                              \
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


#define DO_CORE_LOG_WRITE(code, subcode, level,                         \
                          message, dynamic)                             \
    DO_CORE_LOG_X(code, subcode, level, message, dynamic, 0, 0, 0, 0)

#define DO_CORE_LOG_DATA(code, subcode, level, data, size,              \
                         message, dynamic)                              \
    DO_CORE_LOG_X(code, subcode, level, message, dynamic, 0, 0, data, size)

#define DO_CORE_LOG_ERRNO(code, subcode, level, error, descr,           \
                          message, dynamic)                             \
    DO_CORE_LOG_X(code, subcode, level, message, dynamic, error, descr, 0, 0)

#define CORE_LOG_X(subcode, level, message)                             \
    DO_CORE_LOG_WRITE(NCBI_C_ERRCODE_X, subcode, level,                 \
                      message, 0)

#define CORE_LOGF_X(subcode, level, fmt_args)                           \
    DO_CORE_LOG_WRITE(NCBI_C_ERRCODE_X, subcode, level,                 \
                      g_CORE_Sprintf fmt_args, 1)

#define CORE_LOG(level, message)                                        \
    DO_CORE_LOG_WRITE(0, 0, level,                                      \
                      message, 0)

#define CORE_LOGF(level, fmt_args)                                      \
    DO_CORE_LOG_WRITE(0, 0, level,                                      \
                      g_CORE_Sprintf fmt_args, 1)

#ifdef _DEBUG
#  define CORE_TRACE(message)    CORE_LOG(eLOG_Trace, message)
#  define CORE_TRACEF(fmt_args)  CORE_LOGF(eLOG_Trace, fmt_args)
#  define CORE_DEBUG_ARG(arg)    arg
#else
#  define CORE_TRACE(message)    ((void) 0)
#  define CORE_TRACEF(fmt_args)  ((void) 0)
#  define CORE_DEBUG_ARG(arg)    /*arg*/
#endif /*_DEBUG*/

#define CORE_DATA_X(subcode, data, size, message)                       \
    DO_CORE_LOG_DATA(NCBI_C_ERRCODE_X, subcode, eLOG_Trace, data, size, \
                     message, 0)

#define CORE_DATAF_X(subcode, data, size, fmt_args)                     \
    DO_CORE_LOG_DATA(NCBI_C_ERRCODE_X, subcode, eLOG_Trace, data, size, \
                     g_CORE_Sprintf fmt_args, 1)

#define CORE_DATA_EXX(subcode, level, data, size, message)              \
    DO_CORE_LOG_DATA(NCBI_C_ERRCODE_X, subcode, level, data, size,      \
                     message, 0)
    
#define CORE_DATAF_EXX(subcode, level, data, size, fmt_args)            \
    DO_CORE_LOG_DATA(NCBI_C_ERRCODE_X, subcode, level, data, size,      \
                     g_CORE_Sprintf fmt_args, 1)

#define CORE_DATA(data, size, message)                                  \
    DO_CORE_LOG_DATA(0, 0, eLOG_Trace, data, size,                      \
                     message, 0)
    
#define CORE_DATAF(data, size, fmt_args)                                \
    DO_CORE_LOG_DATA(0, 0, eLOG_Trace, data, size,                      \
                     g_CORE_Sprintf fmt_args, 1)

#define CORE_DATA_EX(level, data, size, message)                        \
    DO_CORE_LOG_DATA(0, 0, level, data, size,                           \
                     message, 0)

#define CORE_DATAF_EX(level, data, size, fmt_args)                      \
    DO_CORE_LOG_DATA(0, 0, level, data, size,                           \
                     g_CORE_Sprintf fmt_args, 1)

#define CORE_LOG_ERRNO_X(subcode, level, error, message)                \
    DO_CORE_LOG_ERRNO(NCBI_C_ERRCODE_X, subcode, level, error, 0,       \
                      message, 0)

#define CORE_LOGF_ERRNO_X(subcode, level, error, fmt_args)              \
    DO_CORE_LOG_ERRNO(NCBI_C_ERRCODE_X, subcode, level, error, 0,       \
                      g_CORE_Sprintf fmt_args, 1)

#define CORE_LOG_ERRNO_EXX(subcode, level, error, descr, message)       \
    DO_CORE_LOG_ERRNO(NCBI_C_ERRCODE_X, subcode, level, error, descr,   \
                      message, 0)

#define CORE_LOGF_ERRNO_EXX(subcode, level, error, descr, fmt_args)     \
    DO_CORE_LOG_ERRNO(NCBI_C_ERRCODE_X, subcode, level, error, descr,   \
                      g_CORE_Sprintf fmt_args, 1)

#define CORE_LOG_ERRNO(level, error, message)                           \
    DO_CORE_LOG_ERRNO(0, 0, level, error, 0,                            \
                      message, 0)

#define CORE_LOGF_ERRNO(level, error, fmt_args)                         \
    DO_CORE_LOG_ERRNO(0, 0, level, error, 0,                            \
                      g_CORE_Sprintf fmt_args, 1)

#define CORE_LOG_ERRNO_EX(level, error, descr, message)                 \
    DO_CORE_LOG_ERRNO(0, 0, level, error, descr,                        \
                      message, 0)

#define CORE_LOGF_ERRNO_EX(level, error, descr, fmt_args)               \
    DO_CORE_LOG_ERRNO(0, 0, level, error, descr,                        \
                      g_CORE_Sprintf fmt_args, 1)


/******************************************************************************
 *  Error codes used throughout C-code
 */

/* Here are only error codes used in C sources. For error codes used in
 * C++ sources (in C++ Toolkit) see include/connect/error_codes.hpp.
 */
NCBI_C_DEFINE_ERRCODE_X(Connect_Conn,     301,  35);
NCBI_C_DEFINE_ERRCODE_X(Connect_LBSM,     302,  23);
NCBI_C_DEFINE_ERRCODE_X(Connect_Util,     303,   8);
NCBI_C_DEFINE_ERRCODE_X(Connect_Dispd,    304,   2);
NCBI_C_DEFINE_ERRCODE_X(Connect_FTP,      305,   9);
NCBI_C_DEFINE_ERRCODE_X(Connect_HeapMgr,  306,  33);
NCBI_C_DEFINE_ERRCODE_X(Connect_HTTP,     307,  18);
NCBI_C_DEFINE_ERRCODE_X(Connect_LBSMD,    308,   8);
NCBI_C_DEFINE_ERRCODE_X(Connect_Sendmail, 309,  31);
NCBI_C_DEFINE_ERRCODE_X(Connect_Service,  310,   8);
NCBI_C_DEFINE_ERRCODE_X(Connect_Socket,   311, 160);
NCBI_C_DEFINE_ERRCODE_X(Connect_Crypt,    312,   6);
NCBI_C_DEFINE_ERRCODE_X(Connect_LocalNet, 313,   4);
NCBI_C_DEFINE_ERRCODE_X(Connect_Mghbn,    314,  16);


/******************************************************************************
 *  REGISTRY
 */

/* Always use the following macros and functions to access "g_CORE_Registry",
 * dont access/change it directly!
 */
extern NCBI_XCONNECT_EXPORT REG g_CORE_Registry;

#define CORE_REG_GET(section, name, value, value_size, def_value) \
    g_CORE_RegistryGET(section, name, value, value_size, def_value)

#define CORE_REG_SET(section, name, value, storage)  do { \
    CORE_LOCK_READ; \
    REG_Set(g_CORE_Registry, section, name, value, storage); \
    CORE_UNLOCK; \
} while (0)


/* (private, to be used exclusively by the above macro CORE_REG_GET) */
extern NCBI_XCONNECT_EXPORT const char* g_CORE_RegistryGET
(const char* section,
 const char* name,
 char*       value,
 size_t      value_size,
 const char* def_value);


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* CONNECT___NCBI_PRIV__H */
