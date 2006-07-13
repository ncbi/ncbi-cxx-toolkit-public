#ifndef CONNECT___NCBI_CORE__H
#define CONNECT___NCBI_CORE__H

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
 * @file
 * File Description:
 *   Types and code shared by all "ncbi_*.[ch]" modules.
 *
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


/** I/O read method
 */
typedef enum {
    eIO_ReadPlain,   /**< read presently available data only                 */
    eIO_ReadPeek,    /**< same eIO_ReadPlain but leave data in input queue   */
    eIO_ReadPersist, /**< try to read exactly "n" bytes; wait for enough data*/
    /* deprecated */
    eIO_Plain   = eIO_ReadPlain,
    eIO_Peek    = eIO_ReadPeek,
    eIO_Persist = eIO_ReadPersist
} EIO_ReadMethod;


/** I/O write method
 */
typedef enum {
    eIO_WritePlain,
    eIO_WritePersist,
    eIO_WriteOutOfBand
} EIO_WriteMethod;


/** I/O event (or direction)
 * @li <b>NOTE:</b> Internally, these constants are used as bit-values,
 *     and thus should not be changed in this header.  However, user code
 *     should not rely on the values of these constants.
 * @sa
 *  SOCK_Wait, SOCK_Poll, CONN_Wait, SOCK_SetTimeout, CONN_SetTimeout
 */
typedef enum {
    eIO_Open      = 0x0, /**< also serves as no-event indicator in SOCK_Poll */
    eIO_Read      = 0x1,
    eIO_Write     = 0x2,
    eIO_ReadWrite = 0x3, /**< eIO_Read | eIO_Write                           */
    eIO_Close     = 0x4  /**< also serves as error indicator in SOCK_Poll    */
} EIO_Event;


/** I/O status
 */
typedef enum {
    eIO_Success = 0,  /**< everything is fine, no errors occurred            */
    eIO_Timeout,      /**< timeout expired before any I/O succeeded          */
    eIO_Closed,       /**< peer has closed the connection                    */
    eIO_Interrupt,    /**< signal received while an I/O was in progress      */
    eIO_InvalidArg,   /**< bad argument value(s)                             */
    eIO_NotSupported, /**< the requested operation is not supported          */

    eIO_Unknown       /**< unknown (most probably -- fatal) error            */
} EIO_Status;


/** Get the text form of an enum status value
 * @param status
 *  An enum value to get the text form for
 * @return 
 *  Verbal description of the I/O status
 * @sa
 *  EIO_Status
 */
extern NCBI_XCONNECT_EXPORT const char* IO_StatusStr(EIO_Status status);



/******************************************************************************
 *  MT locking
 */


/** Lock handle -- keeps all data needed for the locking and for the cleanup.
 */
struct MT_LOCK_tag;
typedef struct MT_LOCK_tag* MT_LOCK;


/** Set the lock/unlock callback function and its data for MT critical section.
 * @li <b>TIP:</b> If the RW-lock functionality is not provided by the
 *     callback, then: eMT_LockRead <==> eMT_Lock
 */
typedef enum {
    eMT_Lock,      /**< lock   critical section             */
    eMT_LockRead,  /**< lock   critical section for reading */
    eMT_Unlock     /**< unlock critical section             */
} EMT_Lock;


/** MT locking callback (operates like a mutex or RW-lock).
 * @param user_data
 *  See "user_data" in MT_LOCK_Create()
 * @param how
 *  As passed to MT_LOCK_Do()
 * @return
 *  Non-zero value if the requested operation was successful.
 * @li <b>NOTE:</b>  the "-1" value is reserved for unset handler;  you also
 *     may want to return "-1" if your locking function does no locking, and
 *     you don't consider it as an error, but still want the caller to be
 *     aware of this "rightful non-doing" as opposed to the "rightful doing".
 * @sa
 *   MT_LOCK_Create, MT_LOCK_Delete
 */
typedef int/*bool*/ (*FMT_LOCK_Handler)
(void*    user_data,
 EMT_Lock how       
 );

/** MT lock cleanup callback.
 * @param user_data
 *  See "user_data" in MT_LOCK_Create()
 * @sa
 *  MT_LOCK_Create, MT_LOCK_Delete
 */
typedef void (*FMT_LOCK_Cleanup)
(void* user_data
 );


/** Create new MT lock (with an internal reference counter set to 1).
 * @param user_data
 *  Unspecified data to call "handler" and "cleanup" with
 * @param handler
 *  Locking callback
 * @param cleanup
 *  Cleanup callback 
 * @sa
 *  FMT_LOCK_Handler, FMT_LOCK_Cleanup, MT_LOCK_Delete
 */
extern NCBI_XCONNECT_EXPORT MT_LOCK MT_LOCK_Create
(void*            user_data,
 FMT_LOCK_Handler handler,
 FMT_LOCK_Cleanup cleanup
 );


/** Increment internal reference counter by 1, then return "lk".
 * @param lk
 *  A handle previously obtained from MT_LOCK_Create
 * @sa
 *  MT_LOCK_Create, MT_LOCK_Delete
 */
extern NCBI_XCONNECT_EXPORT MT_LOCK MT_LOCK_AddRef(MT_LOCK lk);


/** Decrement internal reference counter by 1, and if it reaches 0, then
 * destroy the handle, call "lk->cleanup(lk->user_data)", and return NULL;
 * otherwise (if the reference counter is still > 0), return "lk".
 * @param lk
 *  A handle previously obtained from MT_LOCK_Create
 * @sa
 *  MT_LOCK_Create, FMT_LOCK_Cleanup
 */
extern NCBI_XCONNECT_EXPORT MT_LOCK MT_LOCK_Delete(MT_LOCK lk);


/** Call "lk->handler(lk->user_data, how)".
 * @param lk
 *  A handle previously obtained from MT_LOCK_Create
 * @param how
 *  Whether to lock (and how: read, write) or to unlock
 * @return
 *  Value returned by the lock handler ("handler" in MT_LOCK_Create()).
 * If lock handler is not specified then always return "-1".
 * @li <b>NOTE:</b>  use MT_LOCK_Do() to avoid overhead!
 * @sa
 *  MT_LOCK_Create, FMT_LOCK_Handler, EMT_Lock
 */
#define MT_LOCK_Do(lk, how)  (lk ? MT_LOCK_DoInternal(lk, how) : -1)
extern NCBI_XCONNECT_EXPORT int/*bool*/ MT_LOCK_DoInternal
(MT_LOCK  lk,
 EMT_Lock how
 );


/******************************************************************************
 *  ERROR HANDLING and LOGGING
 */


/** Log handle -- keeps all data needed for the logging and for the cleanup.
 */
struct LOG_tag;
typedef struct LOG_tag* LOG;


/** Log severity level
 */
typedef enum {
    eLOG_Trace = 0,
    eLOG_Note,
    eLOG_Warning,
    eLOG_Error,
    eLOG_Critical,
    eLOG_Fatal
} ELOG_Level;


/** Obtain verbal representation of an enum level value.
 * @param level
 *  An enum value to get the text form for
 * @return
 *  Verbal description of the log level
 * @sa
 *  ELOG_Level
 */
extern NCBI_XCONNECT_EXPORT const char* LOG_LevelStr(ELOG_Level level);


/** Message and miscellaneous data to pass to log post callback FLOG_Handler.
 * @param message
 *  A message to post, can be NULL
 * @param level
 *  A message level
 * @param module
 *  A module string to post, can be NULL
 * @param file
 *  A file name to post, can be NULL
 * @param line
 *  A line number within the file (above) to post, can be 0
 * @param raw_data
 *  Raw data to log (usually NULL)
 * @param raw_size
 *  Size of the raw data (usually zero)
 * @sa
 *  FLOG_Handler, LOG_Create, LOG_WriteInternal
 */
typedef struct {
    const char* message;
    ELOG_Level  level;
    const char* module;  
    const char* file;  
    int         line;
    const void* raw_data;
    size_t      raw_size;
} SLOG_Handler;


/** Log post callback.
 * @param user_data
 *  Unspeficied data as passed to LOG_Create() or LOG_Reset()
 * @param call_data
 *  Composed from arguments passed to LOG_WriteInternal()
 * @sa
 *  SLOG_Handler, LOG_Create, LOG_Reset, LOG_WriteInternal
 */
typedef void (*FLOG_Handler)
(void*         user_data,
 SLOG_Handler* call_data
 );


/** Log cleanup callback.
 * @param user_data
 *   Unspeficied data as passed to LOG_Create() or LOG_Reset()
 * @sa
 *  LOG_Create, LOG_Reset
 * 
 */
typedef void (*FLOG_Cleanup)
(void* user_data
 );


/** Create new LOG (with an internal reference counter set to 1).
 * @li <b>ATTENTION:</b>  if non-NULL "mt_lock" is specified then
 *     MT_LOCK_Delete() will be called on it when this LOG gets deleted
 *     -- be aware of it (hence, if the lock is also to be used with something
 *     else, then call MT_LOCK_AddRef() on it before passing to LOG_Create)!
 * @param user_data
 *  Unspecified data to call "handler" and "cleanup" with
 * @param handler
 *  Log post callback
 * @param cleanup
 *  Cleanup callback
 * @param mt_lock
 *  Protective MT lock (can be NULL)
 * @sa
 *  MT_LOCK, MT_LOCK_AddRef, FLOG_Handler, FLOG_Cleanup, LOG_Reset, LOG_Delete
 */
extern NCBI_XCONNECT_EXPORT LOG LOG_Create
(void*        user_data,
 FLOG_Handler handler,
 FLOG_Cleanup cleanup,
 MT_LOCK      mt_lock
 );


/** Reset the "lg" to use the new "user_data", "handler" and "cleanup".
 * @li <b>NOTE:</b>  it does not change the reference counter of the log.
 * @param lg
 *  A log handle previously obtained from LOG_Create
 * @param user_data
 *  New user data
 * @param handler
 *  New log post callback
 * @param cleanup
 *  New cleanup callback
 * @sa
 *  LOG_Create
 */
extern NCBI_XCONNECT_EXPORT void LOG_Reset
(LOG          lg,     
 void*        user_data,
 FLOG_Handler handler,
 FLOG_Cleanup cleanup
 );


/** Increment internal reference counter by 1, then return "lg".
 * @param lg
 *  A log handle previously obtained from LOG_Create
 * @sa
 *  LOG_Create
 */
extern NCBI_XCONNECT_EXPORT LOG LOG_AddRef(LOG lg);


/** Decrement internal reference counter by 1, and if it reaches 0, then
 * call "lg->cleanup(lg->user_data)", destroy the handle, and return NULL;
 * otherwise (if reference counter is still > 0), return "lg".
 * @param lg
 *  A log handle previously obtained from LOG_Create
 * @sa
 *  LOG_Create
 */
extern NCBI_XCONNECT_EXPORT LOG LOG_Delete(LOG lg);


/** Write message (perhaps with raw data attached) to the log by doing
 * "lg->handler(lg->user_data, SLOG_Handler*)".
 * @li <b>NOTE:</b>  Do not call this function directly, if possible.
 *     Instead, use LOG_WRITE() and LOG_DATA() macros from <ncbi_util.h>!
 * @param lg
 *  A log handle previously obtained from LOG_Create
 * @param level
 *  Severity of the message
 * @param module
 *  Module name (can be NULL)
 * @param file
 *  Source file name (can be NULL)
 * @param line
 *  Source line within the file (can be 0 to omit the line number)
 * @param message
 *  Message content
 * @param raw_data
 *  Raw data to log (can be NULL)
 * @param raw_size
 *  Size of the raw data (can be zero)
 * @sa
 *  LOG_Create, FLOG_Handler, LOG_WRITE, LOG_DATA
 */
extern NCBI_XCONNECT_EXPORT void LOG_WriteInternal
(LOG         lg,   
 ELOG_Level  level,
 const char* module,
 const char* file, 
 int         line,  
 const char* message,
 const void* raw_data,
 size_t      raw_size
 );



/******************************************************************************
 *  REGISTRY
 */


/** Registry handle (keeps all data needed for the registry get/set/cleanup).
 */
struct REG_tag;
typedef struct REG_tag* REG;


/** Transient/Persistent storage.
 * @sa
 *  REG_Get, REG_Set
 */
typedef enum {
    eREG_Transient = 0,  /**< only in-memory storage while program runs */
    eREG_Persistent      /**< hard-copy storage across program runs     */
} EREG_Storage;


/** Registry getter callback.
 * Copy registry value stored in "section" under name "name" to buffer "value".
 * Look for the matching entry first in the transient storage, and then in
 * the persistent storage.  Do not modify the "value" (leave it "as is",
 * i.e. default) if the requested entry is not found in the registry.
 * @li <b>NOTE:</b>  always terminate value by '\0'.
 * @li <b>NOTE:</b>  do not put more than "value_size" bytes to "value".
 * @param user_data
 *  Unspecified data as passed to REG_Create or REG_Reset
 * @param section
 *  Section name to search
 * @param name
 *  Key name to search within the section
 * @param value
 *  Default value passed in (cut to "value_size") symbols, found value out
 * @param value_size
 *  Size of "value" storage, must be greater than 0
 * @sa
 *  REG_Create, REG_Reset
 */
typedef void (*FREG_Get)
(void*       user_data,
 const char* section,
 const char* name,
 char*       value,    
 size_t      value_size
 );


/** Registry setter callback.
 * Store the "value" to  the registry section "section" under name "name",
 * and according to "storage".
 * @param user_data
 *  Unspecified data as passed to REG_Create or REG_Reset
 * @param section
 *  Section name to add the key to
 * @param name
 *  Key name to add to the section
 * @param value
 *  Key value to associate with the key
 * @param storage
 *  How to store the new setting, temporarily or permanently.
 * @sa
 *  REG_Create, REG_Reset, EREG_Storage
 */
typedef void (*FREG_Set)
(void*        user_data,
 const char*  section,
 const char*  name,
 const char*  value,
 EREG_Storage storage
 );


/** Registry cleanup callback.
 * @param user_data
 *  Unspecified data as passed to REG_Create or REG_Reset
 * @sa
 *  REG_Reset, REG_Delete
 */
typedef void (*FREG_Cleanup)
(void* user_data
 );


/** Create new registry (with an internal reference counter set to 1).
 * @li <b>ATTENTION:</b>  if non-NULL "mt_lock" is specified then
 *     MT_LOCK_Delete() will be called on it when this REG gets destroyed
 *     -- be aware of it (hence, if the lock is also to be used with something
 *     else, then call MT_LOCK_AddRef() on it before passing to REG_Create)!
 * @param user_data
 *  Unspecified data to call "set", "get" and "cleanup" with 
 * @param get
 *  Getter callback
 * @param set
 *  Setter callback
 * @param cleanup
 *  Cleanup callback
 * @param mt_lock
 *  Protective MT lock (can be NULL)
 * @sa
 *  MT_LOCK, MT_LOCK_AddRef, REG_Get, REG_Set, REG_Reset, REG_Delete
 */
extern NCBI_XCONNECT_EXPORT REG REG_Create
(void*        user_data,
 FREG_Get     get,    
 FREG_Set     set,    
 FREG_Cleanup cleanup, 
 MT_LOCK      mt_lock
 );


/** Reset the registry handle to use the new "user_data", "set", "get",
 * and "cleanup".
 * @li <b>NOTE:</b>  no change to the internal reference counter.
 * @param rg
 *  Registry handle as previously obtained from REG_Create
 * @param user_data
 *  New user data
 * @param get
 *  New getter callback
 * @param set
 *  New setter callback
 * @param cleanup
 *  New cleanup callback
 * @param do_cleanup
 *  Whether to call old cleanup (if any specified) for old data
 * @sa
 *  REG_Create, REG_Delete
 */
extern NCBI_XCONNECT_EXPORT void REG_Reset
(REG          rg,
 void*        user_data,
 FREG_Get     get,   
 FREG_Set     set,  
 FREG_Cleanup cleanup,
 int/*bool*/  do_cleanup
 );


/** Increment internal reference counter by 1, then return "rg"
 * @param rg
 *  Registry handle as previously obtained from REG_Create
 * @sa
 *  REG_Create
 */
extern NCBI_XCONNECT_EXPORT REG REG_AddRef(REG rg);


/** Decrement internal referecne counter by 1, and if it reaches 0, then
 * call "rg->cleanup(rg->user_data)", destroy the handle, and return NULL;
 * otherwise (if the reference counter is still > 0), return "rg".
 * @param rg
 *  Registry handle as previously obtained from REG_Create
 * @sa
 *  REG_Create
 */
extern NCBI_XCONNECT_EXPORT REG REG_Delete(REG rg);


/** Copy the registry value stored in "section" under name "name"
 * to buffer "value";  if the entry is found in both transient and persistent
 * storages, then copy the one from the transient storage.
 * If the specified entry is not found in the registry (or if there is
 * no registry defined), and "def_value" is not NULL, then copy "def_value"
 * to "value" (although, only up to "value_size" characters).
 * @param rg
 *  Registry handle as previously obtained from REG_Create
 * @param section
 *  Registry section name
 * @param name
 *  Registry entry name
 * @param value
 *  Buffer to put the value of the requested entry to
 * @param value_size
 *  Maximal size of buffer "value"
 * @param def_value
 *  Default value (none if passed NULL)
 * @return
 *  Return "value" (however, if "value_size" is zero, then return NULL).
 *  If non-NULL, the returned "value" will be terminated by '\0'.
 * @sa
 *  REG_Create, REG_Set
 */
extern NCBI_XCONNECT_EXPORT char* REG_Get
(REG         rg, 
 const char* section,
 const char* name,
 char*       value,
 size_t      value_size,
 const char* def_value
 );


/** Store the "value" to the registry section "section" under name "name",
 * and according with "storage".
 * @param rg
 *  Registry handle as previously obtained from REG_Create
 * @param section
 *  Section name to store the value into
 * @param name
 *  Name to store the value under
 * @param value
 *  The value to store
 * @param storage
 *  Whether to store temporarily or permanently
 * @sa
 *  REG_Create, EREG_Storage, REG_Get
 */
extern NCBI_XCONNECT_EXPORT void REG_Set
(REG          rg,
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
 * Revision 6.28  2006/07/13 21:00:22  lavr
 * Doxygenization
 *
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
