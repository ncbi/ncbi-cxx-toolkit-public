#ifndef CONNECT___NCBI_UTIL__H
#define CONNECT___NCBI_UTIL__H

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
 * Author:  Denis Vakatov, Anton Lavrentiev
 *
 * @file
 * File Description:
 *   Auxiliaries (mostly optional core to back and complement "ncbi_core.[ch]")
 * @sa
 *  ncbi_core.h
 *
 * 1. CORE support:
 *    macros:     LOG_Write(), LOG_Data(),
 *                LOG_WRITE(), LOG_DATA(),
 *                THIS_FILE, THIS_MODULE,
 *                LOG_WRITE_ERRNO_EX(), LOG_WRITE_ERRNO()
 *    flags:      TLOG_FormatFlags, ELOG_FormatFlags
 *    methods:    LOG_ComposeMessage(), LOG_ToFILE(), MessagePlusErrno(),
 *                CORE_SetLOCK(), CORE_GetLOCK(),
 *                CORE_SetLOG(),  CORE_GetLOG(),   CORE_SetLOGFILE()
 *
 * 2. Auxiliary API:
 *       CORE_GetPlatform()
 *       CORE_GetUsername()
 *       CORE_GetVMPageSize()
 *
 * 3. CRC32 support:
 *       CRC32_Update()
 *
 * 4. Miscellaneous:
 *       UTIL_MatchesMask[Ex]()
 *
 */

#include <connect/ncbi_core.h>
#include <stdio.h>


/** @addtogroup UtilityFunc
 *
 * @{
 */


#ifdef __cplusplus
extern "C" {
#endif


/******************************************************************************
 *  MT locking
 */

/** Set the MT critical section lock/unlock handler -- to be used by the core
 * internals for protection of internal static variables and other MT-sensitive
 * code from being accessed/changed by several threads simultaneously.
 * It is also to fully protect the core log handler, including its setting up,
 * and its callback and cleanup functions.
 * @li <b>NOTES:</b>  This function itself is NOT MT-safe!
 * @li <b>NOTES:</b>  If there is an active CORE MT-lock set already, which
 *     is different from the new one, then MT_LOCK_Delete() is called for
 *     the old lock (i.e. the one being replaced).
 * @param lk
 *  MT-Lock as created by MT_LOCK_Create
 * @sa
 *  MT_LOCK_Create, CORE_SetLOG
 */
extern NCBI_XCONNECT_EXPORT void    CORE_SetLOCK(MT_LOCK lk);
extern NCBI_XCONNECT_EXPORT MT_LOCK CORE_GetLOCK(void);



/******************************************************************************
 *  ERROR HANDLING and LOGGING
 */


/** Slightly customized LOG_WriteInternal() -- to distinguish between
 * logging only a message vs logging a message with some data.
 * @sa
 *  LOG_WriteInternal
 */
#define LOG_Write(lg,level,module,file,line,message) \
  (void) (((lg)  ||  (level) == eLOG_Fatal) ?                         \
  (LOG_WriteInternal(lg,level,module,file,line,message,0,0), 0) : 1)
#define LOG_Data(lg,level,module,file,line,data,size,message) \
  (void) (((lg)  ||  (level) == eLOG_Fatal) ?                         \
  (LOG_WriteInternal(lg,level,module,file,line,message,data,size), 0) : 1)


/** Auxiliary plain macros to write message (maybe, with raw data) to the log.
 * @sa
 *   LOG_Write, LOG_Data
 */
#define  LOG_WRITE(lg, level, message) \
  LOG_Write(lg, level, THIS_MODULE, THIS_FILE, __LINE__, message)

#ifdef   LOG_DATA
/* AIX's <pthread.h> defines LOG_DATA to be an integer constant;
   we must explicitly drop such definitions to avoid trouble */
#  undef LOG_DATA
#endif
#define  LOG_DATA(lg, data, size, message) \
  LOG_Data(lg, eLOG_Trace, THIS_MODULE, THIS_FILE, __LINE__, \
           data, size, message)


/** Defaults for the THIS_FILE and THIS_MODULE macros (used by LOG_WRITE).
 */
#ifndef   THIS_FILE
#  define THIS_FILE __FILE__
#endif

#ifndef   THIS_MODULE
#  define THIS_MODULE 0
#endif


/** Set the log handle (no logging if "lg" is passed zero) -- to be used by
 * the core internals.
 * If there is an active log handler set already, and it is different from
 * the new one, then LOG_Delete is called for the old logger (that is,
 * the one being replaced).
 * @param lg
 *  LOG handle as returned by LOG_Create, or NULL to stop logging
 * @sa
 *  LOG_Create, LOG_Delete, CORE_GetLOG
 */
extern NCBI_XCONNECT_EXPORT void CORE_SetLOG(LOG lg);


/** Get the log handle which is be used by the core internals.
 *  @li <b>NOTES:</b>  You may not delete the handle (by means of LOG_Delete).
 * @return
 *  LOG handle as set by CORE_SetLOG or NULL if no logging is currently active
 * @sa
 *  CORE_SetLOG, LOG_Create, LOG_Delete
 */
extern NCBI_XCONNECT_EXPORT LOG  CORE_GetLOG(void);


/** Standard logging to the specified file stream.
 * @param fp
 *  The file stream to log to
 * @param auto_close
 *  Do "fclose(fp)" when the LOG is reset/destroyed.
 * @sa
 *  CORE_SetLOG
 */
extern NCBI_XCONNECT_EXPORT void CORE_SetLOGFILE
(FILE*       fp,
 int/*bool*/ auto_close
 );


/** Same as CORE_SetLOGFILE(fopen(filename, "a"), TRUE).
 * @param filename
 *  Filename to write the log into
 * @return
 *  Return zero on error, non-zero on success
 * @sa
 *  CORE_SetLOG
 */
extern NCBI_XCONNECT_EXPORT int/*bool*/ CORE_SetLOGFILE_NAME
(const char* filename
 );


/** LOG formatting flags: what parts of the message to actually appear.
 * @sa
 *   CORE_SetLOGFormatFlags
 */
typedef enum {
    fLOG_Default       = 0x0,    /**< fLOG_Short if NDEBUG, else fLOG_Full   */

    fLOG_Level         = 0x1,
    fLOG_Module        = 0x2,
    fLOG_FileLine      = 0x4,    /**< always here for eLOG_Trace level       */
    fLOG_DateTime      = 0x8,
    fLOG_OmitNoteLevel = 0x4000, /**< do not add NOTE if eLOG_Note is level  */
    fLOG_None          = 0x8000  /**< nothing but spec'd parts, msg and data */
} ELOG_Format;
typedef unsigned int TLOG_FormatFlags; /**< binary OR of "ELOG_FormatFlags"  */
#define fLOG_Short  (fLOG_Level)
#define fLOG_Full   (fLOG_Level | fLOG_Module | fLOG_FileLine)

extern NCBI_XCONNECT_EXPORT TLOG_FormatFlags CORE_SetLOGFormatFlags
(TLOG_FormatFlags
);


/** Compose message using the "call_data" info.
 * Full log record format:
 *     mm/dd/yy HH:MM:SS "<file>", line <line>: [<module>] <level>: <message>
 *     \n----- [BEGIN] Raw Data (<raw_size> bytes) -----\n
 *     <raw_data>
 *     \n----- [END] Raw Data -----\n
 *
 * @li <b>NOTE:</b>  the returned string must be deallocated using "free()".
 * @param call_data
 *  Parts of the message
 * @param format_flags
 *  Which fields of "call_data" to use
 * @sa
 *  CORE_SetLOG, CORE_SetLOGFormatFlags
 */
extern NCBI_XCONNECT_EXPORT char* LOG_ComposeMessage
(const SLOG_Handler* call_data,
 TLOG_FormatFlags    format_flags
 );


/** LOG_Reset specialized to log to a "FILE*" stream using LOG_ComposeMessage.
 * @param lg
 *  Created by LOG_Create
 * @param fp
 *  The file stream to log to
 * @param auto_close
 *  Whether to do "fclose(fp)" when the LOG is reset/destroyed
 * @sa
 *  LOG_Create, LOG_Reset, LOG_ComposeMessage
 */
extern NCBI_XCONNECT_EXPORT void LOG_ToFILE
(LOG         lg,
 FILE*       fp,
 int/*bool*/ auto_close
 );


/** Add current "errno" (and maybe its description) to the message:
 * <message> {errno=<errno>,<descr>}
 * @param message
 *  [in]  message text (can be NULL)
 * @param x_errno
 *  [in]  error code (if it is zero, then use "descr" only)
 * @param descr
 *  [in]  error description (if NULL, then use "strerror(x_errno)")
 * @param buf
 *  [out] buffer to put the composed message to
 * @param buf_size
 *  [in]  maximal buffer size
 * @return
 *  Return "buf"
 * @sa
 *  LOG_ComposeMessage
 */
extern NCBI_XCONNECT_EXPORT char* MessagePlusErrno
(const char*  message,
 int          x_errno,
 const char*  descr,
 char*        buf,
 size_t       buf_size
 );


/** Special log writing macro that makes use of a specified error number,
 * and an optional description thereof.
 * @sa
 *  LOG_WRITE_ERRNO
 */
#define LOG_WRITE_ERRNO_EX(lg, level, message, x_errno, x_descr)  do {   \
    if ((lg)  ||  (level) == eLOG_Fatal) {                               \
        char _buf[1024];                                                 \
        LOG_WRITE(lg, level, MessagePlusErrno(message, x_errno, x_descr, \
                                              _buf, sizeof(_buf)));      \
    }                                                                    \
} while (0)


/** Variant of LOG_WRITE_ERRNO_EX that uses current errno.
 * @sa
 *   LOG_WRITE_ERRNO_EX
 */
#define LOG_WRITE_ERRNO(lg, level, message)                              \
     LOG_WRITE_ERRNO_EX(lg, level, message, errno, 0)



/******************************************************************************
 *  REGISTRY
 */

/** Set the registry (no registry if "rg" is passed zero) -- to be used by
 * the core internals.
 * If there is an active registry set already, and it is different from
 * the new one, then REG_Delete() is called for the old(replaced) registry.
 * @param rg
 *  
 */
extern NCBI_XCONNECT_EXPORT void CORE_SetREG(REG rg);
extern NCBI_XCONNECT_EXPORT REG  CORE_GetREG(void);



/******************************************************************************
 *  Auxiliary API
 */

/**
 * @return
 *  Return read-only textual but machine-readable platform description.
 */
extern NCBI_XCONNECT_EXPORT const char* CORE_GetPlatform(void);


/** Obtain and store current user's name in the buffer provided.
 * Note that resultant strlen(buf) is always guaranteed to be less
 * than "bufsize", extra non-fit characters discarded.
 * Both "buf" and "bufsize" must not be zeros.
 * @param buf
 *  Pointer to buffer to store the user name at
 * @param bufsize
 *  Size of buffer in bytes
 * @return
 *  Return 0 when the user name cannot be determined.
 *  Otherwise, return "buf".
 */
extern NCBI_XCONNECT_EXPORT const char* CORE_GetUsername
(char*        buf,
 size_t       bufsize
 );


/* Obtain virtual memory page size.
 * Return 0 if the page size cannot be determined.
 */
extern NCBI_XCONNECT_EXPORT size_t CORE_GetVMPageSize(void);



/******************************************************************************
 *  CRC32
 */

/** Calculate/Update CRC32
 * @param checksum
 *  Checksum to update (start with 0)
 * @param ptr
 *  Block of data
 * @param count
 *  Size of data
 * @return
 *  Return the checksum updated according to the contents of the block
 *  pointed to by "ptr" and having "count" bytes in it.
 */
extern NCBI_XCONNECT_EXPORT unsigned int CRC32_Update
(unsigned int checksum,
 const void*  ptr,
 size_t       count
 );



/******************************************************************************
 *  Miscellanea
 */
/**
 * @param name
 *  TODO!
 * @param mask
 *  TODO!
 * @param ignore_case
 *  TODO!
 */
extern NCBI_XCONNECT_EXPORT int/*bool*/ UTIL_MatchesMaskEx
(const char* name,
 const char* mask,
 int/*bool*/ ignore_case
);

/** Same as UTIL_MatchesMaskEx(name, mask, 1)
 * @param name
 *  TODO!
 * @param mask
 *  TODO!
*/
extern NCBI_XCONNECT_EXPORT int/*bool*/ UTIL_MatchesMask
(const char* name,
 const char* mask
);


#ifdef __cplusplus
}  /* extern "C" */
#endif


/* @} */

#endif /* CONNECT___NCBI_UTIL__H */
