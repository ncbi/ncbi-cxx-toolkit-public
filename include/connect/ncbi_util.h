#ifndef CONNECT___NCBI_UTIL__H
#define CONNECT___NCBI_UTIL__H

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
 * Authors:  Denis Vakatov, Anton Lavrentiev
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
 *    flags:      TLOG_FormatFlags, ELOG_FormatFlags
 *    methods:    LOG_ComposeMessage(), LOG_ToFILE(), NcbiMessagePlusError(),
 *                CORE_SetLOCK(), CORE_GetLOCK(),
 *                CORE_SetLOG(),  CORE_GetLOG(),
 *                CORE_SetLOGFILE[_Ex](), CORE_SetLOGFILE_NAME[_Ex]()
 *                LOG_ToFILE[_Ex]()
 *
 * 2. Auxiliary API:
 *       CORE_GetNcbiSid()
 *       CORE_GetAppName()
 *       CORE_GetPlatform()
 *       CORE_GetUsername()
 *       CORE_GetVMPageSize()
 *
 * 3. Checksumming support:
 *       UTIL_CRC32_Update()
 *       UTIL_Adler32_Update()
 *
 * 4. Miscellaneous:
 *       UTIL_MatchesMask[Ex]()
 *       UTIL_NcbiLocalHostName()
 *       UTIL_PrintableString[Size]()
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


/** Auxiliary plain macros to write message (maybe, with raw data) to the log.
 * @sa
 *   LOG_Write
 */
#define  LOG_WRITE(lg, code, subcode, level, message)                   \
    LOG_Write(lg, code, subcode, level, THIS_MODULE, THIS_FILE, __LINE__, \
              message, 0, 0)

#ifdef   LOG_DATA
/* AIX's <pthread.h> defines LOG_DATA to be an integer constant;
   we must explicitly drop such definitions to avoid trouble */
#  undef LOG_DATA
#endif
#define  LOG_DATA(lg, code, subcode, level, data, size, message)        \
    LOG_Write(lg, code, subcode, level, THIS_MODULE, THIS_FILE, __LINE__, \
              message, data, size)


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
 * @return
 *  LOG handle as set by CORE_SetLOG or NULL if no logging is currently active
 *  @li <b>NOTE:</b>  You may not delete the handle (by means of LOG_Delete).
 * @sa
 *  CORE_SetLOG, LOG_Create, LOG_Delete
 */
extern NCBI_XCONNECT_EXPORT LOG  CORE_GetLOG(void);


/** Standard logging to the specified file stream.
 * @param fp
 *  The file stream to log to
 * @param cut_off
 *  Do not post messages with severity levels lower than specified
 * @param auto_close
 *  Do "fclose(fp)" when the LOG is reset/destroyed
 * @sa
 *  LOG_ToFILE_Ex, CORE_SetLOG
 */
extern NCBI_XCONNECT_EXPORT void CORE_SetLOGFILE_Ex
(FILE*       fp,
 ELOG_Level  cut_off,
 int/*bool*/ auto_close
 );


/** Same as CORE_SetLOGFILE_Ex() with last parameter passed as 0
 * (all messages get posted).
 * @sa
 *   CORE_SetLOGFILE_Ex, CORE_SetLOG
 */
extern NCBI_XCONNECT_EXPORT void CORE_SetLOGFILE
(FILE*       fp,
 int/*bool*/ auto_close
 );


/** Same as CORE_SetLOGFILE_Ex(fopen(filename, "a"), cut_off, TRUE).
 * @param filename
 *  Filename to write the log into
 * @param cut_off
 *  Do not post messages with severity levels lower than specified
 * @return
 *  Return zero on error, non-zero on success
 * @sa
 *  CORE_SetLOGFILE_Ex, CORE_SetLOG
 */
extern NCBI_XCONNECT_EXPORT int/*bool*/ CORE_SetLOGFILE_NAME_Ex
(const char* filename,
 ELOG_Level  cut_off
 );


/** Same as CORE_SetLOGFILE_NAME_Ex with last parameter passed as 0
 * (all messages pass).
 * @sa
 *   CORE_SetLOGFILE_NAME_Ex, CORE_SetLOG
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
    fLOG_FileLine      = 0x4,
    fLOG_DateTime      = 0x8,
    fLOG_FullOctal     = 0x2000, /**< do not do reduction in octal data bytes*/
    fLOG_OmitNoteLevel = 0x4000, /**< do not add NOTE if eLOG_Note is level  */
    fLOG_None          = 0x8000  /**< nothing but spec'd parts, msg and data */
} ELOG_Format;
typedef unsigned int TLOG_FormatFlags; /**< binary OR of "ELOG_FormatFlags"  */
#define fLOG_Short   fLOG_Level
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
 * @param cut_off
 *  Do not post messages with severity levels lower than specified
 * @param auto_close
 *  Whether to do "fclose(fp)" when the LOG is reset/destroyed
 * @sa
 *  LOG_Create, LOG_Reset, LOG_ComposeMessage, LOG_ToFILE
 */
extern NCBI_XCONNECT_EXPORT void LOG_ToFILE_Ex
(LOG         lg,
 FILE*       fp,
 ELOG_Level  cut_off,
 int/*bool*/ auto_close
 );


/** Same as LOG_ToFILEx with "cut_off" parameter passed as 0
 * (all messages pass).
 * @sa
 *   LOG_ToFILE_Ex
 */
extern NCBI_XCONNECT_EXPORT void LOG_ToFILE
(LOG         lg,
 FILE*       fp,
 int/*bool*/ auto_close
 );


/** Add current "error" (and maybe its description) to the message:
 * <message>[ {error=[[<error>][,]][<descr>]}]
 * @param dynamic
 *  [inout] non-zero pointed value means message was allocated from heap
 * @param message
 *  [in]    message text (can be NULL)
 * @param error
 *  [in]    error code (if it is zero, then use "descr" only if non-NULL/empty)
 * @param descr
 *  [in]    error description (if NULL, then use "strerror(error)" if error!=0)
 * @return
 *  Always non-NULL message (perhaps, "") and re-set "*dynamic" as appropriate.
 * @li <b>NOTE:</b>  this routine may call "free(message)" if it had
 * to reallocate the original message that had been allocated dynamically
 * before the call (and "*dynamic" thus had been passed non-zero).
 * @sa
 *  LOG_ComposeMessage
 */
extern NCBI_XCONNECT_EXPORT const char* NcbiMessagePlusError
(int/*bool*/ *dynamic,
 const char*  message,
 int          error,
 const char*  descr
 );


/* Several macros brought here from ncbidiag.hpp.  The names slightly
 * changed (added _C) because some sources can include this header and
 * ncbidiag.hpp simultaneously.
 */

/** Defines global error code name with given value (err_code)
 */
#define NCBI_C_DEFINE_ERRCODE_X(name, err_code, max_err_subcode)        \
    enum enum##name {                                                   \
        eErrCodeX_##name = err_code                                     \
        /* automatic subcode checking is not implemented in C code */   \
    }

/** Makes one identifier from 2 parts
 */
#define NCBI_C_CONCAT_IDENTIFIER(prefix, postfix) prefix##postfix

/** Returns value of error code by its name defined by NCBI_DEFINE_ERRCODE_X
 *
 * @sa NCBI_C_DEFINE_ERRCODE_X
 */
#define NCBI_C_ERRCODE_X_NAME(name)   \
    NCBI_C_CONCAT_IDENTIFIER(eErrCodeX_, name)

/** Returns currently set default error code. Default error code is set by
 *  definition of NCBI_USE_ERRCODE_X with name of error code as its value.
 *
 * @sa NCBI_DEFINE_ERRCODE_X
 */
#define NCBI_C_ERRCODE_X   NCBI_C_ERRCODE_X_NAME(NCBI_USE_ERRCODE_X)




/******************************************************************************
 *  REGISTRY
 */

/** Set the registry (no registry if "rg" is passed zero) -- to be used by
 * the core internals.
 * If there is an active registry set already, and it is different from
 * the new one, then REG_Delete() is called for the old(replaced) registry.
 * @param rg
 *  Registry handle as returned by REG_Create()
 * @sa
 *  REG_Create, CORE_GetREG
 */
extern NCBI_XCONNECT_EXPORT void CORE_SetREG(REG rg);


/** Get the registry previously set by CORE_SetREG().
 * @return
 *  Registry handle.
 *  <li>NOTE:</li> You may not delete the handle with REG_Delete().
 * @sa
 *  CORE_SetREG
 */
extern NCBI_XCONNECT_EXPORT REG  CORE_GetREG(void);



/******************************************************************************
 *  Auxiliary API
 */

/** Obtain current NCBI SID (if known, per thread).
 * @return
 *  Return 0 when the SID cannot be determined;
 *  otherwise, return a NULL-terminated string.
 */
extern NCBI_XCONNECT_EXPORT const char* CORE_GetNcbiSid(void);


/** Obtain current application name (toolkit dependent).
 * @return
 *  Return 0 when the application name cannot be determined;
 *  otherwise, return a NULL-terminated string
 *
 * NOTE that setting an application name concurrently with this
 * call can cause undefined behavior or a stale pointer returned.
 */
extern NCBI_XCONNECT_EXPORT const char* CORE_GetAppName(void);


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
 *  Return 0 when the user name cannot be determined;
 *  otherwise, return "buf".
 */
extern NCBI_XCONNECT_EXPORT const char* CORE_GetUsername
(char*  buf,
 size_t bufsize
 );


/** Obtain virtual memory page size.
 * @return
 *  0 if the page size cannot be determined.
 */
extern NCBI_XCONNECT_EXPORT size_t CORE_GetVMPageSize(void);



/******************************************************************************
 *  CRC32
 */

/** Calculate/Update CRC-32 checksum
 * NB:  Initial checksum is "0".
 * @param checksum
 *  Checksum to update (start with 0)
 * @param ptr
 *  Block of data
 * @param len
 *  Size of block of data
 * @return
 *  Return the checksum updated according to the contents of the block
 *  pointed to by "ptr" and having "len" bytes in it.
 */
extern NCBI_XCONNECT_EXPORT unsigned int UTIL_CRC32_Update
(unsigned int checksum,
 const void*  ptr,
 size_t       len
 );

/* FIXME: Compatibility (to remove in the future) */
#define CRC32_Update(c, p, l)  UTIL_CRC32_Update(c, p, l)


/** Calculate/Update Adler-32 checksum
 * NB:  Initial checksum is "1".
 * @param checksum
 *  Checksum to update (start with 1)
 * @param ptr
 *  Block of data
 * @param len
 *  Size of block of data
 * @return
 *  Return the checksum updated according to the contents of the block
 *  pointed to by "ptr" and having "len" bytes in it.
 */
extern NCBI_XCONNECT_EXPORT unsigned int UTIL_Adler32_Update
(unsigned int checksum,
 const void*  ptr,
 size_t       len
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


/** Cut off well-known NCBI domain suffix out of the passed "hostname".
 * @param hostname
 *  Hostname to shorten (if possible)
 * @return 0 if the hostname wasn't modified, otherwise return "hostname".
 */
extern NCBI_XCONNECT_EXPORT char* UTIL_NcbiLocalHostName
(char* hostname
 );


/** Calculate size of buffer needed to store printable representation of the
 * block of data of the specified size (or, if size is 0, strlen(data)).
 * NOTE:  The calculated size does not include terminating '\0'.
 * @param data
 *  Block of data (NULL causes 0 to return regardless of "size")
 * @param size
 *  Size of block (0 causes strlen(data) to be used)
 * @return the buffer size needed (0 for NULL or empty (size==0) block).
 * @sa UTIL_PrintableString
 */
extern NCBI_XCONNECT_EXPORT size_t UTIL_PrintableStringSize
(const char* data,
 size_t      size
 );


/** Create printable representation of the block of data of the specified size
 * (or, if size is 0, strlen(data), and return buffer pointer past the last
 * stored character (non '\0'-terminated).
 * NOTE:  The input buffer "buf" where to store the printable representation
 * is assumed to be of adequate size to hold the resultant string.
 * Non-printable characters can be represented in a reduced octal form
 * as long as the result is unambiguous (unless "full" passed true (non-zero),
 * in which case all non-printable characters get represented by full
 * octal tetrads).  NB: Hexadecimal output is not used because it is
 * ambiguous by the standard (can contain undefined number of hex digits).
 * @param data
 *  Block of data (NULL causes NULL to return regardless of "size" or "buf")
 * @param size
 *  Size of block (0 causes strlen(data) to be used)
 * @buf
 *  Buffer to store the result (NULL always causes NULL to return)
 * @full
 *  Whether to print full octal representation of non-printable characters
 * @return next position in the buffer past the last stored character.
 * @sa UTIL_PrintableStringSize
 */
extern NCBI_XCONNECT_EXPORT char* UTIL_PrintableString
(const char* data,
 size_t      size,
 char*       buf,
 int         full
 );


/** Conversion from Unicode to UTF8, and back.
 *  MSWIN-specific.
 *  NOTE:
 *    The caller must always use UTIL_ReleaseBufferOnHeap()
 *    to free the buffer returned from UTIL_TcharToUtf8OnHeap(),  and
 *    ReleaseBuffer() to free the one returned from UTIL_TcharToUtf8().
 */

#if defined(NCBI_OS_MSWIN)  &&  defined(_UNICODE)
extern const char*    UTIL_TcharToUtf8OnHeap(const wchar_t* buffer);
extern const char*    UTIL_TcharToUtf8      (const wchar_t* buffer);
extern const wchar_t* UTIL_Utf8ToTchar      (const    char* buffer);
#  define             UTIL_ReleaseBuffer(x)  UTIL_ReleaseBufferOnHeap(x)
#else
#  define             UTIL_TcharToUtf8OnHeap(x)  (x)
#  define             UTIL_TcharToUtf8(x)        (x)
#  define             UTIL_Utf8ToTchar(x)        (x)
#  define             UTIL_ReleaseBuffer(x)      /*void*/
#endif /*NCBI_OS_MSWIN && _UNICODE*/

#ifdef NCBI_OS_MSWIN
extern void           UTIL_ReleaseBufferOnHeap(const void* buffer);
#else
#  define             UTIL_ReleaseBufferOnHeap(x)  /*void*/
#endif /*NCBI_OS_MSWIN*/


#ifdef __cplusplus
}  /* extern "C" */
#endif


/* @} */

#endif /* CONNECT___NCBI_UTIL__H */
