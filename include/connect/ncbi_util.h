#ifndef NCBI_UTIL__H
#define NCBI_UTIL__H

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
 *   Auxiliary (optional) code for "ncbi_core.[ch]"
 *
 *********************************
 *    methods:    LOG_ComposeMessage(), LOG_ToFILE(), MessagePlusErrno(),
 *                CORE_SetLOCK(), CORE_GetLOCK(),
 *                CORE_SetLOG(),  CORE_GetLOG(),   CORE_SetLOGFILE()
 *    flags:      TLOG_FormatFlags, ELOG_FormatFlags
 *    macro:      LOG_Write(), LOG_Data(),
 *                LOG_WRITE(), LOG_DATA(),  THIS_FILE, THIS_MODULE
 *                LOG_WRITE_ERRNO()
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.11  2002/02/11 21:49:06  lavr
 * +CORE_GetPlatform()
 *
 * Revision 6.10  2001/08/09 16:23:34  lavr
 * Added: fLOG_OmitNoteLevel to log message format flags
 *
 * Revision 6.9  2001/07/30 14:39:47  lavr
 * Do not include date/time in default logging (for ncbidiag.cpp compatibility)
 *
 * Revision 6.8  2001/07/25 19:12:31  lavr
 * Added date/time stamp for message logging
 *
 * Revision 6.7  2001/05/17 18:10:22  vakatov
 * Moved the logging macros from <ncbi_core.h> to <ncbi_util.h>.
 * Logging::  always call the logger if severity is eLOG_Fatal.
 *
 * Revision 6.6  2001/01/12 23:50:37  lavr
 * "a+" -> "a" as a mode in fopen() for a logfile
 *
 * Revision 6.5  2000/06/23 19:34:41  vakatov
 * Added means to log binary data
 *
 * Revision 6.4  2000/05/30 23:23:24  vakatov
 * + CORE_SetLOGFILE_NAME()
 *
 * Revision 6.3  2000/04/07 19:56:06  vakatov
 * Get rid of <errno.h>
 *
 * Revision 6.2  2000/03/24 23:12:05  vakatov
 * Starting the development quasi-branch to implement CONN API.
 * All development is performed in the NCBI C++ tree only, while
 * the NCBI C tree still contains "frozen" (see the last revision) code.
 *
 * Revision 6.1  2000/02/23 22:30:40  vakatov
 * Initial revision
 *
 * ===========================================================================
 */

#include <connect/ncbi_core.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif


/******************************************************************************
 *  MT locking
 */

/* Set the MT critical section lock/unlock handler -- to be used by the core
 * internals to protect internal static variables and other MT-sensitive
 * code from being accessed/changed by several threads simultaneously.
 * It is also to protect fully the core log handler, including its setting up
 * (see CORE_SetLOG below), and its callback and cleanup functions.
 * NOTES:
 * This function itself is so NOT MT-safe!
 * If there is an active MT-lock handler set already, and it is different from
 * the new one, then MT_LOCK_Delete() is called for the old(replaced) handler.
 */
extern void    CORE_SetLOCK(MT_LOCK lk);
extern MT_LOCK CORE_GetLOCK(void);



/******************************************************************************
 *  ERROR HANDLING and LOGGING
 */


/* Slightly customized LOG_WriteInternal() -- to distinguish between
 * logging only a message vs. logging message with some data.
 */
#define LOG_Write(lg,level,module,file,line,message) \
  (void) ((lg  ||  level == eLOG_Fatal) ? \
  (LOG_WriteInternal(lg,level,module,file,line,message,0,0), 0) : 1)
#define LOG_Data(lg,level,module,file,line,data,size,message) \
  (void) ((lg  ||  level == eLOG_Fatal) ? \
  (LOG_WriteInternal(lg,level,module,file,line,message,data,size), 0) : 1)


/* Auxiliary plain macro to write message (maybe, with raw data) to the log
 */
#define LOG_WRITE(lg, level, message) \
  LOG_Write(lg, level, THIS_MODULE, THIS_FILE, __LINE__, message)

#define LOG_DATA(lg, data, size, message) \
  LOG_Data(lg, eLOG_Trace, 0, 0, 0, data, size, message)


/* Defaults for the THIS_FILE and THIS_MODULE macros (used by LOG_WRITE)
 */
#if !defined(THIS_FILE)
#  define THIS_FILE __FILE__
#endif

#if !defined(THIS_MODULE)
#  define THIS_MODULE 0
#endif


/* Set the log handler (no logging if "lg" is passed zero) -- to be used by
 * the core internals.
 * If there is an active log handler set already, and it is different from
 * the new one, then LOG_Delete() is called for the old(replaced) logger.
 */
extern void CORE_SetLOG(LOG lg);
extern LOG  CORE_GetLOG(void);


/* Standard logging to the specified file stream
 */
extern void CORE_SetLOGFILE
(FILE*       fp,         /* the file stream to log to */
 int/*bool*/ auto_close  /* do "fclose(fp)" when the LOG is reset/destroyed */
 );


/* CORE_SetLOGFILE(fopen(filename, "a"), TRUE)
 * Return zero on error, non-zero on success
 */
extern int/*bool*/ CORE_SetLOGFILE_NAME
(const char* filename  /* log.-file name */
 );


typedef enum {
    fLOG_Default  = 0x0, /* "fLOG_Short" if NDEBUG, else "fLOG_Full"        */

    fLOG_Level    = 0x1,
    fLOG_Module   = 0x2,
    fLOG_FileLine = 0x4, /* (must always be printed for "eLOG_Trace" level) */
    fLOG_DateTime = 0x8,
    fLOG_OmitNoteLevel = 0x4000 /* Do not add "NOTE:" if eLOG_Note is level */
} ELOG_Format;
typedef unsigned int TLOG_FormatFlags;  /* binary OR of "ELOG_FormatFlags"  */
#define fLOG_Short  (fLOG_Level)
#define fLOG_Full   (fLOG_Level | fLOG_Module | fLOG_FileLine)

extern TLOG_FormatFlags CORE_SetLOGFormatFlags(TLOG_FormatFlags);


/* Compose message using the "call_data" info.
 * Full format:
 *     mm/dd/yy HH:MM:SS "<file>", line <line>: [<module>] <level>: <message>
 *     \n----- [BEGIN] Raw Data (<raw_size> bytes) -----\n
 *     <raw_data>
 *     \n----- [END] Raw Data -----\n
 *
 *
 * NOTE:  the returned string must be deallocated using "free()".
 */
extern char* LOG_ComposeMessage
(const SLOG_Handler* call_data,
 TLOG_FormatFlags    format_flags  /* which fields of "call_data" to use */
 );


/* LOG_Reset() specialized to log to a "FILE*" stream.
 */
extern void LOG_ToFILE
(LOG         lg,         /* created by LOG_Create() */
 FILE*       fp,         /* the file stream to log to */
 int/*bool*/ auto_close  /* do "fclose(fp)" when the LOG is reset/destroyed */
 );


/* Add current "errno" (and maybe its description) to the message:
 *   <message> {errno=<errno>,<descr>}
 * Return "buf".
 */
extern char* MessagePlusErrno
(const char*  message,  /* [in]  message (can be NULL) */
 int          x_errno,  /* [in]  error code (if it's zero, use "descr" only) */
 const char*  descr,    /* [in]  if NULL, then use "strerror(x_errno)" */
 char*        buf,      /* [out] buffer to put the composed message to */
 size_t       buf_size  /* [in]  max. buffer size */
 );

#define LOG_WRITE_ERRNO(lg, level, message)  do { \
  if (lg  ||  level == eLOG_Fatal) { \
    char buf[2048]; \
    LOG_WRITE(lg, level, \
              MessagePlusErrno(message, errno, 0, buf, sizeof(buf))); \
  } \
} while (0)



/******************************************************************************
 *  REGISTRY
 */

/* Set the registry (no registry if "rg" is passed zero) -- to be used by
 * the core internals.
 * If there is an active registry set already, and it is different from
 * the new one, then REG_Delete() is called for the old(replaced) registry.
 */
extern void CORE_SetREG(REG rg);
extern REG  CORE_GetREG(void);



/******************************************************************************
 *  MISC
 */

/* Return read-only textual but machine-readable platform description.
 */
extern const char* CORE_GetPlatform(void);


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* NCBI_UTIL__H */
