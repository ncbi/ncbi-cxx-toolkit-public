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
 * Tracing and logging:
 *    methods:    LOG_ComposeMessage(), LOG_ToFILE(), MessagePlusErrno()
 *    flags:      TLOG_FormatFlags, ELOG_FormatFlags
 *    macro:      LOG_WRITE_ERRNO()
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.1  2000/02/23 22:30:40  vakatov
 * Initial revision
 *
 * ===========================================================================
 */

#include <connect/ncbi_core.h>
#include <stdio.h>
#include <errno.h>


#ifdef __cplusplus
extern "C" {
#endif


/******************************************************************************
 *  ERROR HANDLING and LOGGING
 */


/* Compose message using the "call_data" info.
 * Full format:
 *     "<file>", line <line>: [<module>] <level>: <message>
 * NOTE:  the returned string must be deallocated using "free()".
 */
typedef unsigned int TLOG_FormatFlags;  /* binary OR of "ELOG_FormatFlags" */
typedef enum {
  fLOG_Default = 0x0,  /* "fLOG_Short" if NDEBUG, else "fLOG_Full" */
  fLOG_Short   = 0x1,
  fLOG_Full    = 0x7,

  fLOG_Level    = 0x1,
  fLOG_Module   = 0x2,
  fLOG_FileLine = 0x4  /* (should always be printed for "eLOG_Trace" level) */
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
 *   <message> {errno:<errno> <descr>}
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


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* NCBI_UTIL__H */
