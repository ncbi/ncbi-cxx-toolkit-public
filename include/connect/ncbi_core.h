#ifndef NCBI_CORE__H
#define NCBI_CORE__H

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
 * Timeout:
 *    struct STimeout
 *
 * I/O status and direction:
 *    enum:  EIO_ReadMethod
 *    enum:  EIO_Status,  verbal: IO_StatusStr()
 *    enum:  EIO_Event
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
 *    methods:    LOG_Greate(),  LOG_Reset(),
 *                LOG_AddRef(),  LOG_Delete(),  LOG_Write()
 *    macro:      LOG_WRITE(),  THIS_FILE, THIS_MODULE
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.1  2000/02/23 22:30:40  vakatov
 * Initial revision
 *
 * ===========================================================================
 */

#include <assert.h>
#if defined(NDEBUG)
#  define verify(expr)  (void)(expr)
#else
#  define verify(expr)  assert(expr)
#endif


#ifdef __cplusplus
extern "C" {
#endif


/* Timeout structure
 */
typedef struct {
  unsigned int sec;  /* seconds (gets truncated to platform-dep. max. limit) */
  unsigned int usec; /* microseconds (always get truncated by mod 1,000,000) */
} STimeout;



/******************************************************************************
 *  I/O
 */


/* I/O read method
 */
typedef enum {
  eIO_Plain,  /* read the presently available data only */
  eIO_Peek,   /* eREAD_Plain, but dont discard the data from input queue */
  eIO_Persist /* try to read exactly "size" bytes;  wait for enough data */
} EIO_ReadMethod;


/* I/O event (or direction)
 */
typedef enum {
  eIO_Open,
  eIO_Read,
  eIO_Write,
  eIO_ReadWrite,
  eIO_Close
} EIO_Event;


/* I/O status
 */
typedef enum {
  eIO_Success = 0,  /* everything is fine, no errors occured          */
  eIO_Timeout,      /* timeout expired before the data could be i/o'd */
  eIO_Closed,       /* peer has closed the connection                 */
  eIO_InvalidArg,   /* bad argument value(s)                          */
  eIO_NotSupported, /* the requested operation is not supported       */

  eIO_Unknown       /* unknown (most probably -- fatal) error         */
} EIO_Status;


/* Return verbal description of the I/O status
 */
extern const char* IO_StatusStr(EIO_Status status);



/******************************************************************************
 *  MT locking
 */


/* Lock handle (keeps all data needed for the locking and for the cleanup)
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
extern MT_LOCK MT_LOCK_Create
(void*            user_data, /* to call "handler" and "cleanup" with */
 FMT_LOCK_Handler handler,   /* locking function */
 FMT_LOCK_Cleanup cleanup    /* cleanup function */
 );


/* Increment ref.counter by 1,  then return "lk". 
 */
extern MT_LOCK MT_LOCK_AddRef(MT_LOCK lk);


/* Decrement ref.counter by 1.
 * Now, if ref.counter becomes 0, then
 * destroy the handle, call "lk->cleanup(lk->user_data)", and return NULL.
 * Otherwise (if ref.counter is still > 0), return "lk".
 */
extern MT_LOCK MT_LOCK_Delete(MT_LOCK lk);


/* Call "lk->handler(lk->user_data, how)".
 * Return value returned by the lock handler ("handler" in MT_LOCK_Create()).
 * If lock handler is not specified then always return "-1".
 * NOTE:  use MT_LOCK_Do() to avoid overhead!
 */
#define MT_LOCK_Do(lk,how)  (lk ? MT_LOCK_DoInternal(lk, how) : -1)
extern int/*bool*/ MT_LOCK_DoInternal
(MT_LOCK  lk,
 EMT_Lock how
 );


/******************************************************************************
 *  ERROR HANDLING and LOGGING
 */


/* Log handle (keeps all data needed for the logging and for the cleanup).
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


/* Return verbal description of the log level.
 */
extern const char* LOG_LevelStr(ELOG_Level level);


/* Message and miscellaneous data to pass to the log post callback FLOG_Handler
 * For more details, see LOG_Write().
 */
typedef struct {
  const char* message;  /* can be NULL */
  ELOG_Level  level;
  const char* module;   /* can be NULL */
  const char* file;     /* can be NULL */
  int         line;
} SLOG_Handler;


/* Log post callback.
 */
typedef void (*FLOG_Handler)
(void*         user_data,  /* see "user_data" in LOG_Create() or LOG_Reset() */
 SLOG_Handler* call_data   /* composed from LOG_Write() args */
 );


/* Log cleanup function;  see "LOG_Reset()" for more details.
 */
typedef void (*FLOG_Cleanup)
(void* user_data  /* see "user_data" in LOG_Create() or LOG_Reset() */
 );


/* Create new LOG (with reference counter := 1).
 * ATTENTION:  if non-NULL "lk" is specified then MT_LOCK_Delete() will be
 *             called when this LOG is destroyed -- be aware of it!
 */
extern LOG LOG_Create
(void*        user_data, /* the data to call "handler" and "cleanup" with */
 FLOG_Handler handler,   /* handler */
 FLOG_Cleanup cleanup,   /* cleanup */
 MT_LOCK      mt_lock    /* protective MT lock (can be NULL) */
 );


/* Reset the "lg" to use the new "user_data", "handler" and "cleanup".
 * NOTE:  it does not change ref.counter.
 */
extern void LOG_Reset
(LOG          lg,         /* created by LOG_Create() */
 void*        user_data,  /* new user data */
 FLOG_Handler handler,    /* new handler */
 FLOG_Cleanup cleanup,    /* new cleanup */
 int/*bool*/  do_cleanup  /* call cleanup(if any specified) for the old data */
 );


/* Increment ref.counter by 1,  then return "lg". 
 */
extern LOG LOG_AddRef(LOG lg);


/* Decrement ref.counter by 1.
 * Now, if ref.counter becomes 0, then
 * call "lg->cleanup(lg->user_data)", destroy the handle, and return NULL.
 * Otherwise (if ref.counter is still > 0), return "lg".
 */
extern LOG LOG_Delete(LOG lg);


/* Write message to the log -- e.g. call:
 *   "lg->handler(lg->user_data, SLOG_Handler*)"
 * NOTE: use the LOG_Write()  to avoid overhead!
 */
#define LOG_Write(lg,level,module,file,line,message) \
  (void)(lg ? (LOG_WriteInternal(lg,level,module,file,line,message),0) : 1)
extern void LOG_WriteInternal
(LOG         lg,      /* created by LOG_Create() */
 ELOG_Level  level,   /* severity */
 const char* module,  /* module name */
 const char* file,    /* source file */
 int         line,    /* source line */
 const char* message  /* message content */
 );

/* Auxiliary plain macro to write message to the log
 */
#define LOG_WRITE(lg, level, message) \
  LOG_Write(lg, level, THIS_MODULE, THIS_FILE, __LINE__, message)


/* Defaults for the THIS_FILE and THIS_MODULE macros (used by LOG_WRITE)
 */
#if !defined(THIS_FILE)
#  define THIS_FILE __FILE__
#endif

#if !defined(THIS_MODULE)
#  define THIS_MODULE 0
#endif


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* NCBI_CORE__H */
