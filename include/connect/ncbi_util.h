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
 *    macro:      LOG_WRITE_ERRNO()
 *
 * ---------------------------------------------------------------------------
 * $Log$
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


/* Compose message using the "call_data" info.
 * Full format:
 *     "<file>", line <line>: [<module>] <level>: <message>
 *     \n----- [BEGIN] Raw Data (<raw_size> bytes) -----\n
 *     <raw_data>
 *     \n----- [END] Raw Data -----\n
 *
 *
 * NOTE:  the returned string must be deallocated using "free()".
 */
typedef unsigned int TLOG_FormatFlags;  /* binary OR of "ELOG_FormatFlags" */
typedef enum {
    fLOG_Default = 0x0,  /* "fLOG_Short" if NDEBUG, else "fLOG_Full" */
    fLOG_Short   = 0x1,
    fLOG_Full    = 0x7,

    fLOG_Level    = 0x1,
    fLOG_Module   = 0x2,
    fLOG_FileLine = 0x4  /* (must always be printed for "eLOG_Trace" level) */
} ELOG_Format;

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
  if ( lg ) { \
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


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* NCBI_UTIL__H */
