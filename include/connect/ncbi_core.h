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
 * File Description:
 * @file ncbi_core.h
 *   Types and code shared by all "ncbi_*.[ch]" modules.
 *
 * I/O status and direction:
 *    enum:       EIO_ReadMethod
 *    enum:       EIO_WriteMethod
 *    enum:       EIO_Event
 *    enum:       EIO_Status,  verbal: IO_StatusStr()
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
 *                LOG_Write(), LOG_WriteInternal()
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


/** I/O read method.
 * @sa
 *  EIO_WriteMethod
 */
typedef enum {
    eIO_ReadPeek,       /**< do eIO_ReadPlain but leave data in input queue  */
    eIO_ReadPlain,      /**< read readily available data only, wait if none  */
    eIO_ReadPersist     /**< read exactly as much as requested, w/waits      */
} EIO_ReadMethod;


/** I/O write method.
 * @sa
 *  EIO_ReadMethod
 */
typedef enum {
    eIO_WriteNone,      /**< invalid reserved opcode, not for use!           */
    eIO_WritePlain,     /**< write as much as possible, report back how much */
    eIO_WritePersist,   /**< write exactly as much as specified, w/waits     */
    eIO_WriteOutOfBand  /**< write out-of-band chunk of urgent data (if supp)*/
} EIO_WriteMethod;


/** I/O event (or direction).
 * @note
 *  Internally, these constants are used as bit-values, and therefore must not
 *  be changed in this header.  On the other hand, user code should not rely
 *  on the values of these constants, either.
 * @warning
 *  Careful with an unfortunate naming similarity of eIO_Close from this enum
 *  and EIO_Status::eIO_Closed -- do not mix the two!
 * @sa
 *  SOCK_Wait, SOCK_Poll, CONN_Wait, SOCK_SetTimeout, CONN_SetTimeout
 */
typedef enum {
    eIO_Open      = 0,  /**< also serves as no-event indicator in SOCK_Poll  */
    eIO_Read      = 1,  /**< read                                            */
    eIO_Write     = 2,  /**< write                                           */
    eIO_ReadWrite = 3,  /**< eIO_Read | eIO_Write (also, eCONN_OnFlush)      */
    eIO_Close     = 4   /**< also serves as an error indicator in SOCK_Poll  */
} EIO_Event;


/** I/O status.
 * @warning
 *  Careful with an unfortunate naming similarity of eIO_Closed from this enum
 *  and EIO_Event::eIO_Close -- do not mix the two!
 */
typedef enum {
    eIO_Success = 0,    /**< everything is fine, no error occurred           */
    eIO_Timeout,        /**< timeout expired before any I/O succeeded        */
    eIO_Reserved,       /**< reserved status code -- DO NOT USE!             */
    eIO_Interrupt,      /**< signal arrival prevented any I/O to succeed     */
    eIO_InvalidArg,     /**< bad argument / parameter value(s) supplied      */
    eIO_NotSupported,   /**< operation is not supported or is not available  */
    eIO_Unknown,        /**< unknown I/O error (likely fatal but can retry)  */
    eIO_Closed          /**< connection is / has been closed, EOF condition  */
#define EIO_N_STATUS  8
} EIO_Status;


/** Get the text form of an enum status value.
 * @param status
 *  An enum value to get the text form for
 * @return 
 *  Verbal description of the I/O status
 * @warning
 *  Returns NULL on out-of-bound values
 * @note
 *  Reserved status code(s) returned as an empty string ("")
 * @sa
 *  EIO_Status
 */
extern NCBI_XCONNECT_EXPORT const char* IO_StatusStr(EIO_Status status);



/******************************************************************************
 *  MT locking
 */


/** Lock handle -- keeps all data needed for the locking and for the cleanup.
 * @sa
 *   CORE_SetLOCK
 */
struct MT_LOCK_tag;
typedef struct MT_LOCK_tag* MT_LOCK;


/** Set the lock/unlock callback function and its data for MT critical section.
 * @note
 *  If the RW-lock functionality is not provided by the callback, then:
 *  eMT_LockRead <==> eMT_Lock
 *
 */
typedef enum {
    eMT_Lock,        /**< lock   critical section                     */
    eMT_LockRead,    /**< lock   critical section for reading         */
    eMT_Unlock,      /**< unlock critical section                     */
    eMT_TryLock,     /**< try to lock,             return immediately */
    eMT_TryLockRead  /**< try to lock for reading, return immediately */
} EMT_Lock;


/** MT locking callback (operates like a [recursive] mutex or RW-lock).
 * @param data
 *  See "data" in MT_LOCK_Create()
 * @param how
 *  As passed to MT_LOCK_Do()
 * @return
 *  Non-zero value if the requested operation was successful.
 * @note
 *  The "-1" value is reserved for unset handler;  you also may want to return
 *  "-1" if your locking function does no locking, and you don't consider it as
 *  an error, but still want the caller to be aware of this "rightful
 *  non-doing" as opposed to the "rightful doing".
 * @sa
 *   MT_LOCK_Create, MT_LOCK_Delete
 */
typedef int/*bool*/ (*FMT_LOCK_Handler)
(void*    data,
 EMT_Lock how
 );

/** MT lock cleanup callback.
 * @param data
 *  See "data" in MT_LOCK_Create()
 * @sa
 *  MT_LOCK_Create, MT_LOCK_Delete
 */
typedef void (*FMT_LOCK_Cleanup)
(void* data
 );


/** Create a new MT lock (with an internal reference count set to 1).
 * @param data
 *  Unspecified data to call "handler" and "cleanup" with
 * @param handler
 *  Locking callback
 * @param cleanup
 *  Cleanup callback 
 * @sa
 *  FMT_LOCK_Handler, FMT_LOCK_Cleanup, MT_LOCK_Delete
 */
extern NCBI_XCONNECT_EXPORT MT_LOCK MT_LOCK_Create
(void*            data,
 FMT_LOCK_Handler handler,
 FMT_LOCK_Cleanup cleanup
 );


/** Increment internal reference count by 1, then return "lk".
 * @param lk
 *  A handle previously obtained from MT_LOCK_Create
 * @sa
 *  MT_LOCK_Create, MT_LOCK_Delete
 */
extern NCBI_XCONNECT_EXPORT MT_LOCK MT_LOCK_AddRef(MT_LOCK lk);


/** Decrement internal reference count by 1, and if it reaches 0, then
 * destroy the handle, call "lk->cleanup(lk->data)", and return NULL;
 * otherwise (if the reference count is still > 0), return "lk".
 * @param lk
 *  A handle previously obtained from MT_LOCK_Create
 * @sa
 *  MT_LOCK_Create, FMT_LOCK_Cleanup
 */
extern NCBI_XCONNECT_EXPORT MT_LOCK MT_LOCK_Delete(MT_LOCK lk);


/** Call "lk->handler(lk->data, how)".
 * @param lk
 *  A handle previously obtained from MT_LOCK_Create
 * @param how
 *  Whether to lock (and how: read, write) or to unlock
 * @return
 *  Value returned by the lock handler ("handler" in MT_LOCK_Create()).
 *  If the lock handler is not specified then always return "-1" (noop).
 * @note
 *  Use MT_LOCK_Do() to avoid overhead!
 * @sa
 *  MT_LOCK_Create, FMT_LOCK_Handler, EMT_Lock
 */
#define MT_LOCK_Do(lk, how)  ((lk) ? MT_LOCK_DoInternal((lk), (how)) : -1)
extern NCBI_XCONNECT_EXPORT int/*bool*/ MT_LOCK_DoInternal
(MT_LOCK  lk,
 EMT_Lock how
 );


/******************************************************************************
 *  Error handling and logging
 */


/** Log handle -- keeps all data needed for the logging and for the cleanup.
 * @sa
 *   CORE_SetLOG
 */
struct LOG_tag;
typedef struct LOG_tag* LOG;


/** Log severity level.
 */
typedef enum {
    eLOG_Trace = 0,
    eLOG_Note,
    eLOG_Info = eLOG_Note,  /**< In C++ Toolkit "Info" is used, not "Note" */
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
 * @param dynamic
 *  if non-zero then LOG_WriteInternal() will call free(message) before return
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
 * @param err_code
 *  Error code of the message
 * @param err_subcode
 *  Error subcode of the message
 * @sa
 *  FLOG_Handler, LOG_Create, LOG_WriteInternal, LOG_Write
 */
typedef struct {
    int/*bool*/ dynamic;
    const char* message;
    ELOG_Level  level;
    const char* module;
    const char* func;
    const char* file;
    int         line;
    const void* raw_data;
    size_t      raw_size;
    int         err_code;
    int         err_subcode;
} SLOG_Message;


/** Log post callback.
 * @param data
 *  Unspeficied data as passed to LOG_Create() or LOG_Reset()
 * @param mess
 *  Composed from arguments passed to LOG_WriteInternal()
 * @sa
 *  SLOG_Message, LOG_Create, LOG_Reset, LOG_WriteInternal
 */
typedef void (*FLOG_Handler)
(void*               data,
 const SLOG_Message* mess
 );


/** Log cleanup callback.
 * @param data
 *   Unspeficied data as passed to LOG_Create() or LOG_Reset()
 * @sa
 *  LOG_Create, LOG_Reset
 * 
 */
typedef void (*FLOG_Cleanup)
(void* data
 );


/** Create a new LOG (with an internal reference count set to 1).
 * @warning
 *  If non-NULL "lock" is specified then MT_LOCK_AddRef() is called on it here,
 *  and MT_LOCK_Delete() will be called on it when this LOG gets deleted.
 * @param data
 *  Unspecified data to call "handler" and "cleanup" with
 * @param handler
 *  Log post callback
 * @param cleanup
 *  Cleanup callback
 * @param lock
 *  Protective MT lock (may be NULL)
 * @sa
 *  MT_LOCK, MT_LOCK_AddRef, FLOG_Handler, FLOG_Cleanup, LOG_Reset, LOG_Delete
 */
extern NCBI_XCONNECT_EXPORT LOG LOG_Create
(void*        data,
 FLOG_Handler handler,
 FLOG_Cleanup cleanup,
 MT_LOCK      lock
 );


/** Reset the "lg" to use the new "data", "handler" and "cleanup".
 * @note
 *  It does not change the reference count of the log.
 * @param lg
 *  A log handle previously obtained from LOG_Create
 * @param data
 *  New user data
 * @param handler
 *  New log post callback
 * @param cleanup
 *  New cleanup callback
 * @return
 *  lg (as passed in the first parameter)
 * @sa
 *  LOG_Create
 */
extern NCBI_XCONNECT_EXPORT LOG LOG_Reset
(LOG          lg,
 void*        data,
 FLOG_Handler handler,
 FLOG_Cleanup cleanup
 );


/** Increment internal reference count by 1, then return "lg".
 * @param lg
 *  A log handle previously obtained from LOG_Create
 * @sa
 *  LOG_Create
 */
extern NCBI_XCONNECT_EXPORT LOG LOG_AddRef(LOG lg);


/** Decrement internal reference count by 1, and if it reaches 0, then
 * call "lg->cleanup(lg->data)", destroy the handle, and return NULL;
 * otherwise (if reference count is still > 0), return "lg".
 * @param lg
 *  A log handle previously obtained from LOG_Create
 * @sa
 *  LOG_Create
 */
extern NCBI_XCONNECT_EXPORT LOG LOG_Delete(LOG lg);


/** Upon having filled SLOG_Message data from parameters, write a message
 * (perhaps with raw data attached) to the log by calling LOG_WriteInternal().
 * @note
 *  Do not call this function directly, if possible.  Instead, use the
 *  LOG_WRITE() and LOG_DATA() macros from <connect/ncbi_util.h>!
 * @param code
 *  Error code of the message
 * @param subcode
 *  Error subcode of the message
 * @param level
 *  The message severity
 * @param module
 *  Module name (can be NULL)
 * @param func
 *  Function name (can be NULL)
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
 *  LOG_Create, ELOG_Level, FLOG_Handler, LOG_WriteInternal
 */
extern NCBI_XCONNECT_EXPORT void LOG_Write
(LOG         lg,
 int         code,
 int         subcode,
 ELOG_Level  level,
 const char* module,
 const char* func,
 const char* file,
 int         line,
 const char* message,
 const void* raw_data,
 size_t      raw_size
);


/** Write message (perhaps with raw data attached) to the log by calling
 * "lg->handler(lg->data, mess)".
 * @note
 *  Do not call this function directly, if possible.  Instead, use the
 *  LOG_WRITE() and LOG_DATA() macros from <ncbi_util.h>!
 * @warning
 *  This call free()s "mess->message" when "mess->dynamic" is set non-zero!
 * @param lg
 *  A log handle previously obtained from LOG_Create
 * @sa
 *  LOG_Create, ELOG_Level, FLOG_Handler, LOG_Write
 */
extern NCBI_XCONNECT_EXPORT void LOG_WriteInternal
(LOG                 lg,
 const SLOG_Message* mess
 );


/******************************************************************************
 *  Registry
 */


/** Registry handle (keeps all data needed for the registry get/set/cleanup).
 * @sa
 *   CORE_SetReg
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
 * @note
 *  Always terminate value with '\0'.
 * @note
 *  Do not put more than "value_size" bytes to "value".
 * @param data
 *  Unspecified data as passed to REG_Create or REG_Reset
 * @param section
 *  Section name to search
 * @param name
 *  Key name to search within the section
 * @param value
 *  Default value passed in (cut to "value_size") symbols, found value out
 * @param value_size
 *  Size of "value" storage, must be greater than 0
 * @return
 *  1 if successfully found and stored;  -1 if not found (the default is to be
 *  used);  0 if an error (including truncation) occurred
 * @sa
 *  REG_Create, REG_Reset
 */
typedef int (*FREG_Get)
(void*       data,
 const char* section,
 const char* name,
 char*       value,    
 size_t      value_size
 );


/** Registry setter callback.
 * Store the "value" to  the registry section "section" under name "name",
 * and according to "storage".
 * @param data
 *  Unspecified data as passed to REG_Create or REG_Reset
 * @param section
 *  Section name to add the key to
 * @param name
 *  Key name to add to the section
 * @param value
 *  Key value to associate with the key (NULL to deassociate, i.e. unset)
 * @param storage
 *  How to store the new setting, temporarily or permanently
 * @return
 *  Non-zero if successful (including replacing a value with itself)
 * @sa
 *  REG_Create, REG_Reset, EREG_Storage
 */
typedef int/*bool*/ (*FREG_Set)
(void*        data,
 const char*  section,
 const char*  name,
 const char*  value,
 EREG_Storage storage
 );


/** Registry cleanup callback.
 * @param data
 *  Unspecified data as passed to REG_Create or REG_Reset
 * @sa
 *  REG_Reset, REG_Delete
 */
typedef void (*FREG_Cleanup)
(void* data
 );


/** Create a new registry (with an internal reference count set to 1).
 * @warning
 *  if non-NULL "lock" is specified then MT_LOCK_AddRef() is called on it here,
 *  and MT_LOCK_Delete() will be called on it when this REG gets destroyed.
 *  Passing NULL callbacks below causes limiting the functionality
 *  only to those operations that have the callbacks set for.
 * @param data
 *  Unspecified data to call "set", "get" and "cleanup" with 
 * @param get
 *  Getter callback
 * @param set
 *  Setter callback
 * @param cleanup
 *  Cleanup callback
 * @param lock
 *  Protective MT lock (may be NULL)
 * @sa
 *  MT_LOCK, MT_LOCK_AddRef, REG_Get, REG_Set, REG_Reset, REG_Delete
 */
extern NCBI_XCONNECT_EXPORT REG REG_Create
(void*        data,
 FREG_Get     get,    
 FREG_Set     set,    
 FREG_Cleanup cleanup, 
 MT_LOCK      lock
 );


/** Reset the registry handle to use the new "data", "set", "get",
 * and "cleanup".
 * @note
 *  No change to the internal reference count.
 * @param rg
 *  Registry handle as previously obtained from REG_Create
 * @param data
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
 void*        data,
 FREG_Get     get,   
 FREG_Set     set,  
 FREG_Cleanup cleanup,
 int/*bool*/  do_cleanup
 );


/** Increment internal reference count by 1, then return "rg".
 * @param rg
 *  Registry handle as previously obtained from REG_Create
 * @sa
 *  REG_Create
 */
extern NCBI_XCONNECT_EXPORT REG REG_AddRef(REG rg);


/** Decrement internal reference count by 1, and if it reaches 0, then
 * call "rg->cleanup(rg->data)", destroy the handle, and return NULL;
 * otherwise (if the reference count is still > 0), return "rg".
 * @param rg
 *  Registry handle as previously obtained from REG_Create
 * @sa
 *  REG_Create
 */
extern NCBI_XCONNECT_EXPORT REG REG_Delete(REG rg);


/** Copy the registry value stored in "section" under name "name" to buffer
 * "value";  if the entry is found in both transient and persistent storages,
 * then copy the one from the transient storage.
 * If the specified entry is not found in the registry (or if there is no
 * registry defined), and "def_value" is not NULL, then copy "def_value" to
 * "value" (although, only up to "value_size" characters).
 * @param rg
 *  Registry handle as previously obtained from REG_Create
 * @param section
 *  Registry section name
 * @param name
 *  Registry entry name
 * @param value
 *  Buffer to receive the value of the requested entry, must be non-NULL
 * @param value_size
 *  Maximal size of buffer "value", must be greater than 0
 * @param def_value
 *  Default value (none if passed NULL or "")
 * @return
 *  Return "value" if the found value, including the default, and with its '\0'
 *  terminator, fits entirely within "value_size";  return NULL if there was an
 *  error retrieving the value, or if it had to be truncated (but regardless,
 *  "value" must always be kept '\0'-terminated unless "value_size" was zero).
 * @sa
 *  REG_Create, REG_Set
 */
extern NCBI_XCONNECT_EXPORT const char* REG_Get
(REG         rg, 
 const char* section,
 const char* name,
 char*       value,
 size_t      value_size,
 const char* def_value
 );


/** Store the "value" into the registry section "section" under the key "name",
 * and according to "storage".
 * @param rg
 *  Registry handle as previously obtained from REG_Create
 * @param section
 *  Section name to store the value into
 * @param name
 *  Name to store the value under
 * @param value
 *  The value to store (NULL to unset the parameter)
 * @param storage
 *  Whether to store temporarily or permanently
 * @return
 *  Non-zero if successful (including replacing a value with itself)
 * @sa
 *  REG_Create, EREG_Storage, REG_Get
 */
extern NCBI_XCONNECT_EXPORT int REG_Set
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

#endif /* CONNECT___NCBI_CORE__H */
