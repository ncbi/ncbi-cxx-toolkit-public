#ifndef NCBI_PRIV__H
#define NCBI_PRIV__H

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
 *    Private aux. code for the "ncbi_*.[ch]"
 *
 *********************************
 * Critical section (basic multi-thread synchronization):
 *    private:    g_CORE_MT_Lock
 *    macro:      CORE_LOCK_WRITE, CORE_LOCK_READ, CORE_UNLOCK
 * Tracing and logging:
 *    private:    g_CORE_Log
 *    macro:      CORE_LOG(), CORE_DATA(),
 *                CORE_LOG_SYS_ERRNO(), CORE_LOG_ERRNO()
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.3  2001/01/08 22:37:36  lavr
 * Added GNU attribute to g_CORE_sprintf for compiler to trace
 * format specifier/parameter correspondence
 *
 * Revision 6.2  2000/06/23 19:34:44  vakatov
 * Added means to log binary data
 *
 * Revision 6.1  2000/03/24 22:53:35  vakatov
 * Initial revision
 *
 * ===========================================================================
 */

#include <connect/ncbi_util.h>


#ifdef __cplusplus
extern "C" {
#endif


/******************************************************************************
 *  Multi-Thread SAFETY
 */

/* Always use the following macros and functions to access "g_CORE_MT_Lock",
 * dont access/change it directly!
 */
extern MT_LOCK g_CORE_MT_Lock;

#define CORE_LOCK_WRITE  verify( MT_LOCK_Do(g_CORE_MT_Lock, eMT_Lock    ) )
#define CORE_LOCK_READ   verify( MT_LOCK_Do(g_CORE_MT_Lock, eMT_LockRead) )
#define CORE_UNLOCK      verify( MT_LOCK_Do(g_CORE_MT_Lock, eMT_Unlock  ) )



/******************************************************************************
 *  ERROR HANDLING and LOGGING
 */

/* Always use the following macros and functions to access "g_CORE_Log",
 * dont access/change it directly!
 */
extern LOG g_CORE_Log;

#define CORE_LOG(level, message)  do { \
    if ( g_CORE_Log ) { \
        CORE_LOCK_READ; \
        LOG_WRITE(g_CORE_Log, level, message); \
        CORE_UNLOCK; \
    } \
} while (0)

#define CORE_DATA(data, size, message)  do { \
    if ( g_CORE_Log ) { \
        CORE_LOCK_READ; \
        LOG_DATA(g_CORE_Log, data, size, message); \
        CORE_UNLOCK; \
    } \
} while (0)

extern const char* g_CORE_Sprintf(const char* fmt, ...)
#ifdef __GNUC__
         __attribute__((format(printf, 1, 2)))
#endif
;

#define CORE_LOGF(level, fmt_args)  do { \
    if ( g_CORE_Log ) { \
        CORE_LOCK_READ; \
        LOG_WRITE(g_CORE_Log, level, g_CORE_Sprintf fmt_args); \
        CORE_UNLOCK; \
    } \
} while (0)

#define CORE_DATAF(data, size, fmt_args)  do { \
    if ( g_CORE_Log ) { \
        CORE_LOCK_READ; \
        LOG_DATA(g_CORE_Log, data, size, g_CORE_Sprintf fmt_args); \
        CORE_UNLOCK; \
    } \
} while (0)

#define CORE_LOG_SYS_ERRNO(level, message)  do { \
    if ( g_CORE_Log ) { \
        CORE_LOCK_READ; \
        LOG_WRITE_ERRNO(g_CORE_Log, level, message); \
        CORE_UNLOCK; \
    } \
} while (0)

#define CORE_LOG_ERRNO(x_errno, level, message)  do { \
    if ( g_CORE_Log ) { \
        char buf[2048]; \
        int xx_errno = x_errno; \
        CORE_LOCK_READ; \
        LOG_WRITE(g_CORE_Log, level, \
                  MessagePlusErrno(message, xx_errno, "", buf, sizeof(buf))); \
        CORE_UNLOCK; \
    } \
} while (0)



/******************************************************************************
 *  REGISTRY
 */

/* Always use the following macros and functions to access "g_CORE_Registry",
 * dont access/change it directly!
 */
extern REG g_CORE_Registry;

#define CORE_REG_GET(section, name, value, value_size, def_value) \
    g_CORE_RegistryGET(section, name, value, value_size, def_value)

#define CORE_REG_SET(section, name, value, storage)  do { \
    CORE_LOCK_WRITE; \
    REG_Set(g_CORE_Registry, section, name, value, storage); \
    CORE_UNLOCK; \
} while (0)


/* (private, to be used exclusively by the above macro CORE_REG_GET) */
extern char* g_CORE_RegistryGET
(const char* section,
 const char* name,
 char*       value,
 size_t      value_size,
 const char* def_value);


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* NCBI_PRIV__H */
