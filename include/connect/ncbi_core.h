#ifndef CONNECT___NCBI_CORE__H
#define CONNECT___NCBI_CORE__H

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
 *   Types and code shared by all "ncbi_*.[ch]" modules.
 *
 *********************************
 * I/O status and direction:
 *    enum:       EIO_ReadMethod
 *    enum:       EIO_WriteMethod
 *    enum:       EIO_Status,  verbal: IO_StatusStr()
 *    enum:       EIO_Event
 *
 * Critical section (basic multi-thread synchronization):
 *    handle:     MT_LOCK
 *    enum:       EMT_Lock
 *    callbacks:  (*FMT_LOCK_Handler)(),  (*FMT_LOCK_Cleanup)()
 *    methods:    MT_LOCK_Create(),  MT_LOCK_AddRef(),  MT_LOCK_Delete(),
 *                MT_LOCK_Do()
 *
 * Tracing and logging:
 *    handle:     LOG
 *    enum:       ELOG_Level,  verbal: LOG_LevelStr()
 *    flags:      TLOG_FormatFlags, ELOG_FormatFlags
 *    callbacks:  (*FLOG_Handler)(),  (*FLOG_Cleanup)()
 *    methods:    LOG_Create(),  LOG_Reset(),  LOG_AddRef(),  LOG_Delete(),
 *                LOG_WriteInternal()
 *
 * Registry:
 *    handle:     REG
 *    enum:       EREG_Storage
 *    callbacks:  (*FREG_Get)(),  (*FREG_Set)(),  (*FREG_Cleanup)()
 *    methods:    REG_Create(),  REG_Reset(),  REG_AddRef(),  REG_Delete(),
 *                REG_Get(),  REG_Set()
 *
 */

#include <connect/ncbi_types.h>


/** @addtogroup UtilityFunc
 *
 * @{
 */


#ifdef __cplusplus
extern "C" {
#endif


/******************************************************************************
 *  I/O
 */


/* I/O read method
 */
typedef enum {
    eIO_ReadPlain, /* read presently available data only                     */
    eIO_ReadPeek,  /* eIO_ReadPeek but dont discard the data from input queue*/
    eIO_ReadPersist, /* try to read exactly "n" bytes; wait for enough data  */
    /* deprecated */
    eIO_Plain   = eIO_ReadPlain,
    eIO_Peek    = eIO_ReadPeek,
    eIO_Persist = eIO_ReadPersist
} EIO_ReadMethod;


/* I/O write method
 */
typedef enum {
    eIO_WritePlain,
    eIO_WritePersist,
    eIO_WriteOutOfBand
} EIO_WriteMethod;


/* I/O event (or direction)
 * Note: Internally, these constants are used as bit-values, and thus should
 *       not be changed in this header. However, user code should not rely
 *       on the values of these constants.
 */
typedef enum {
    eIO_Open      = 0x0, /* also serves as no-event indicator in SOCK_Poll() */
    eIO_Read      = 0x1,
    eIO_Write     = 0x2,
    eIO_ReadWrite = 0x3, /* eIO_Read | eIO_Write                             */
    eIO_Close     = 0x4  /* also serves as error indicator in SOCK_Poll()    */
} EIO_Event;


/* I/O status
 */
typedef enum {
    eIO_Success = 0,  /* everything is fine, no errors occurred              */
    eIO_Timeout,      /* timeout expired before the data could be i/o'd      */
    eIO_Closed,       /* peer has closed the connection                      */
    eIO_Interrupt,    /* signal received while the operation was in progress */
    eIO_InvalidArg,   /* bad argument value(s)                               */
    eIO_NotSupported, /* the requested operation is not supported            */

    eIO_Unknown       /* unknown (most probably -- fatal) error              */
} EIO_Status;


/* Return verbal description of the I/O status
 */
extern NCBI_XCONNECT_EXPORT const char* IO_StatusStr(EIO_Status status);



/******************************************************************************
 *  MT locking
 */


/* Lock handle -- keeps all data needed for the locking and for the cleanup
 */
struct MT_LOCK_tag;
typedef struct MT_LOCK_tag* MT_LOCK;


/* Set the lock/unlock callback function and its data for MT critical section.
 * TIP: If the RW-lock functionality is not provided by the callback, then:
 *   eMT_LockRead <==> eMT_Lock
 */
typedef enum {
    eMT_Lock,      /* lock    critical section             */
    eMT_LockRead,  /* lock    critical section for reading */
    eMT_Unlock     /* unlock  critical section             */
} EMT_Lock;


/* MT locking function (operates like Mutex or RW-lock)
 * Return non-zero value if the requested operation was successful.
 * NOTE:  the "-1" value is reserved for unset handler;  you also
 *   may want to return "-1" if your locking function does no locking, and
 *   you dont consider it as an error, but still want the caller to be
 *   aware of this "rightful non-doing" as opposed to the "rightful doing".
 */
typedef int/*bool*/ (*FMT_LOCK_Handler)
(void*    user_data,  /* see "user_data" in MT_LOCK_Create() */
 EMT_Lock how         /* as passed to MT_LOCK_Do() */
 );

/* MT lock cleanup function;  see "MT_LOCK_Delete()" for more details
 */
typedef void (*FMT_LOCK_Cleanup)
(void* user_data  /* see "user_data" in MT_LOCK_Create() */
 );


/* Create new MT locking object (with reference counter := 1)
 */
extern NCBI_XCONNECT_EXPORT MT_LOCK MT_LOCK_Create
(void*            user_data, /* to call "handler" and "cleanup" with */
 FMT_LOCK_Handler handler,   /* locking function */
 FMT_LOCK_Cleanup cleanup    /* cleanup function */
 );


/* Increment ref.counter by 1,  then return "lk"
 */
extern NCBI_XCONNECT_EXPORT MT_LOCK MT_LOCK_AddRef(MT_LOCK lk);


/* Decrement ref.counter by 1.
 * Now, if ref.counter becomes 0, then
 * destroy the handle, call "lk->cleanup(lk->user_data)", and return NULL.
 * Otherwise (if ref.counter is still > 0), return "lk".
 */
extern NCBI_XCONNECT_EXPORT MT_LOCK MT_LOCK_Delete(MT_LOCK lk);


/* Call "lk->handler(lk->user_data, how)".
 * Return value returned by the lock handler ("handler" in MT_LOCK_Create()).
 * If lock handler is not specified then always return "-1".
 * NOTE:  use MT_LOCK_Do() to avoid overhead!
 */
#define MT_LOCK_Do(lk,how)  (lk ? MT_LOCK_DoInternal(lk, how) : -1)
extern NCBI_XCONNECT_EXPORT int/*bool*/ MT_LOCK_DoInternal
(MT_LOCK  lk,
 EMT_Lock how
 );


/******************************************************************************
 *  ERROR HANDLING and LOGGING
 */


/* Log handle -- keeps all data needed for the logging and for the cleanup
 */
struct LOG_tag;
typedef struct LOG_tag* LOG;


/* Log severity level
 */
typedef enum {
    eLOG_Trace = 0,
    eLOG_Note,
    eLOG_Warning,
    eLOG_Error,
    eLOG_Critical,
    eLOG_Fatal
} ELOG_Level;


/* Return verbal description of the log level
 */
extern NCBI_XCONNECT_EXPORT const char* LOG_LevelStr(ELOG_Level level);


/* Message and miscellaneous data to pass to the log post callback FLOG_Handler
 * For more details, see LOG_WriteInternal().
 */
typedef struct {
    const char* message;   /* can be NULL */
    ELOG_Level  level;
    const char* module;    /* can be NULL */
    const char* file;      /* can be NULL */
    int         line;
    const void* raw_data;  /* raw data to log (usually NULL)*/
    size_t      raw_size;  /* size of the raw data (usually zero)*/
} SLOG_Handler;


/* Log post callback.
 */
typedef void (*FLOG_Handler)
(void*         user_data,  /* see "user_data" in LOG_Create() or LOG_Reset() */
 SLOG_Handler* call_data   /* composed from LOG_WriteInternal() args */
 );


/* Log cleanup function;  see "LOG_Reset()" for more details
 */
typedef void (*FLOG_Cleanup)
(void* user_data  /* see "user_data" in LOG_Create() or LOG_Reset() */
 );


/* Create new LOG (with reference counter := 1).
 * ATTENTION:  if non-NULL "lk" is specified then MT_LOCK_Delete() will be
 *             called when this LOG is destroyed -- be aware of it!
 */
extern NCBI_XCONNECT_EXPORT LOG LOG_Create
(void*        user_data, /* the data to call "handler" and "cleanup" with */
 FLOG_Handler handler,   /* handler */
 FLOG_Cleanup cleanup,   /* cleanup */
 MT_LOCK      mt_lock    /* protective MT lock (can be NULL) */
 );


/* Reset the "lg" to use the new "user_data", "handler" and "cleanup".
 * NOTE:  it does not change ref.counter.
 */
extern NCBI_XCONNECT_EXPORT void LOG_Reset
(LOG          lg,         /* created by LOG_Create() */
 void*        user_data,  /* new user data */
 FLOG_Handler handler,    /* new handler */
 FLOG_Cleanup cleanup     /* new cleanup */
 );


/* Increment ref.counter by 1,  then return "lg"
 */
extern NCBI_XCONNECT_EXPORT LOG LOG_AddRef(LOG lg);


/* Decrement ref.counter by 1.
 * Now, if ref.counter becomes 0, then
 * call "lg->cleanup(lg->user_data)", destroy the handle, and return NULL.
 * Otherwise (if ref.counter is still > 0), return "lg".
 */
extern NCBI_XCONNECT_EXPORT LOG LOG_Delete(LOG lg);


/* Write message (maybe, with raw data attached) to the log -- e.g. call:
 *   "lg->handler(lg->user_data, SLOG_Handler*)"
 * NOTE:  Do not call this function directly, if possible. Instead, use
 *        LOG_WRITE() and LOG_DATA() macros from <ncbi_util.h>!
 */
extern NCBI_XCONNECT_EXPORT void LOG_WriteInternal
(LOG         lg,        /* created by LOG_Create() */
 ELOG_Level  level,     /* severity */
 const char* module,    /* module name */
 const char* file,      /* source file */
 int         line,      /* source line */
 const char* message,   /* message content */
 const void* raw_data,  /* raw data to log (can be NULL)*/
 size_t      raw_size   /* size of the raw data (can be zero)*/
 );



/******************************************************************************
 *  REGISTRY
 */


/* Registry handle (keeps all data needed for the registry get/set/cleanup)
 */
struct REG_tag;
typedef struct REG_tag* REG;


/* Transient/Persistent storage
 */
typedef enum {
    eREG_Transient = 0,
    eREG_Persistent
} EREG_Storage;


/* Copy the registry value stored in "section" under name "name"
 * to buffer "value". Look for the matching entry first in the transient
 * storage, and then in the persistent storage.
 * If the specified entry is not found in the registry then just do not
 * modify "value" (leave it "as is", i.e. default).
 * Note:  always terminate value by '\0'.
 * Note:  do not put more than "value_size" bytes to "value".
 */
typedef void (*FREG_Get)
(void*       user_data,
 const char* section,
 const char* name,
 char*       value,      /* passed a default value, cut to "value_size" syms */
 size_t      value_size  /* always > 0 */
 );


/* Store the "value" to  the registry section "section" under name "name",
 * in storage "storage".
 */
typedef void (*FREG_Set)
(void*        user_data,
 const char*  section,
 const char*  name,
 const char*  value,
 EREG_Storage storage
 );


/* Registry cleanup function;  see "REG_Reset()" for more details
 */
typedef void (*FREG_Cleanup)
(void* user_data  /* see "user_data" in REG_Create() or REG_Reset() */
 );


/* Create new REG (with reference counter := 1).
 * ATTENTION:  if non-NULL "mt_lock" is specified then MT_LOCK_Delete() will be
 *             called when this REG is destroyed -- be aware of it!
 */
extern NCBI_XCONNECT_EXPORT REG REG_Create
(void*        user_data, /* the data to call "set", "get" and "cleanup" with */
 FREG_Get     get,       /* the get method */
 FREG_Set     set,       /* the set method */
 FREG_Cleanup cleanup,   /* cleanup */
 MT_LOCK      mt_lock    /* protective MT lock (can be NULL) */
 );


/* Reset the "rg" to use the new "user_data", "set", "get" and "cleanup".
 * NOTE:  it does not change ref.counter.
 */
extern NCBI_XCONNECT_EXPORT void REG_Reset
(REG          rg,         /* created by REG_Create() */
 void*        user_data,  /* new user data */
 FREG_Get     get,        /* the get method */
 FREG_Set     set,        /* the set method */
 FREG_Cleanup cleanup,    /* cleanup */
 int/*bool*/  do_cleanup  /* call old cleanup(if any specified) for old data */
 );


/* Increment ref.counter by 1,  then return "rg"
 */
extern NCBI_XCONNECT_EXPORT REG REG_AddRef(REG rg);


/* Decrement ref.counter by 1.
 * Now, if ref.counter becomes 0, then
 * call "rg->cleanup(rg->user_data)", destroy the handle, and return NULL.
 * Otherwise (if ref.counter is still > 0), return "rg".
 */
extern NCBI_XCONNECT_EXPORT REG REG_Delete(REG rg);


/* Copy the registry value stored in "section" under name "name"
 * to buffer "value";  if the entry is found in both transient and persistent
 * storages, then copy the one from the transient storage.
 * If the specified entry is not found in the registry (or if there is
 * no registry defined), and "def_value" is not NULL, then copy "def_value"
 * to "value".
 * Return "value" (however, if "value_size" is zero, then return NULL).
 * If non-NULL, the returned "value" will be terminated by '\0'.
 */
extern NCBI_XCONNECT_EXPORT char* REG_Get
(REG         rg,         /* created by REG_Create() */
 const char* section,    /* registry section name */
 const char* name,       /* registry entry name  */
 char*       value,      /* buffer to put the value of the requested entry to*/
 size_t      value_size, /* max. size of buffer "value" */
 const char* def_value   /* default value (none if passed NULL) */
 );


/* Store the "value" to  the registry section "section" under name "name",
 * in storage "storage".
 */
extern NCBI_XCONNECT_EXPORT void REG_Set
(REG          rg,        /* created by REG_Create() */
 const char*  section,
 const char*  name,
 const char*  value,
 EREG_Storage storage
 );


#ifdef __cplusplus
}  /* extern "C" */
#endif


/* @} */


/*
 * ===========================================================================
 *
 * $Log$
 * Revision 6.27  2005/04/20 18:10:53  lavr
 * verify() moved away into a private header (ncbi_assert.h)
 *
 * Revision 6.26  2004/12/27 15:30:08  lavr
 * +eIO_WriteOutOfBand
 *
 * Revision 6.25  2004/03/12 23:26:22  gorelenk
 * Added export prefixes.
 *
 * Revision 6.24  2003/09/02 20:45:45  lavr
 * -<connect/connect_export.h> -- now included from <connect/ncbi_types.h>
 *
 * Revision 6.23  2003/04/09 17:58:48  siyan
 * Added doxygen support
 *
 * Revision 6.22  2003/03/06 19:42:47  lavr
 * Isolate special inclusion of <stdio.h>, <stdlib.h> in DEBUG branch only
 *
 * Revision 6.21  2003/03/06 01:42:14  lavr
 * Include <stdio.h> and <stdlib.h> required by <assert.h> under Codewarrior
 *
 * Revision 6.20  2003/01/08 01:59:32  lavr
 * DLL-ize CONNECT library for MSVC (add NCBI_XCONNECT_EXPORT)
 *
 * Revision 6.19  2002/09/19 18:00:33  lavr
 * Header file guard macro changed
 *
 * Revision 6.18  2002/08/13 19:26:39  lavr
 * Add eIO_Interrupt to EIO_Status
 *
 * Revision 6.17  2002/08/08 14:21:11  ucko
 * Fix C++-style comment; really move CVS log to end.
 *
 * Revision 6.16  2002/08/07 16:28:25  lavr
 * Added EIO_WriteMethod; EIO_ReadMethod enums changed; log moved to end
 *
 * Revision 6.15  2002/05/06 19:07:48  lavr
 * Added notes to EIO_Event enum type
 *
 * Revision 6.14  2002/03/28 13:28:43  kans
 * undef verify if already defined, such as on Mac OS X Darwin (EN)
 *
 * Revision 6.13  2001/09/27 22:29:35  vakatov
 * Always define "NDEBUG" if "_DEBUG" is not defined (mostly for the C Toolkit)
 *
 * Revision 6.12  2001/08/09 16:22:51  lavr
 * Remove last (unneeded) parameter from LOG_Reset()
 *
 * Revision 6.11  2001/06/19 20:16:18  lavr
 * Added #include <connect/ncbi_types.h>
 *
 * Revision 6.10  2001/06/19 19:08:28  lavr
 * STimeout and ESwitch moved to <connect/ncbi_types.h>
 *
 * Revision 6.9  2001/05/17 18:10:22  vakatov
 * Moved the logging macros from <ncbi_core.h> to <ncbi_util.h>.
 * Logging::  always call the logger if severity is eLOG_Fatal.
 *
 * Revision 6.8  2001/03/02 20:06:54  lavr
 * Typos fixed
 *
 * Revision 6.7  2001/01/23 23:07:30  lavr
 * A make-up change
 *
 * Revision 6.6  2001/01/11 16:41:18  lavr
 * Registry Get/Set methods got the 'user_data' argument, forgotten earlier
 *
 * Revision 6.5  2000/10/18 20:29:41  vakatov
 * REG_Get::  pass in the default value (rather than '\0')
 *
 * Revision 6.4  2000/06/23 19:34:41  vakatov
 * Added means to log binary data
 *
 * Revision 6.3  2000/04/07 19:55:14  vakatov
 * Standard indentation
 *
 * Revision 6.2  2000/03/24 23:12:03  vakatov
 * Starting the development quasi-branch to implement CONN API.
 * All development is performed in the NCBI C++ tree only, while
 * the NCBI C tree still contains "frozen" (see the last revision) code.
 *
 * Revision 6.1  2000/02/23 22:30:40  vakatov
 * Initial revision
 *
 * ===========================================================================
 */

#endif /* CONNECT___NCBI_CORE__H */
