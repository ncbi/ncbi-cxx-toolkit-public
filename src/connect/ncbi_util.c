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
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.1  2000/02/23 22:36:17  vakatov
 * Initial revision
 *
 * ===========================================================================
 */

#include <connect/ncbi_util.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>


/* Static function pre-declarations to avoid C++ compiler warnings
 */
#if defined(__cplusplus)
extern "C" {
  static void s_LOG_FileHandler(void* user_data, SLOG_Handler* call_data);
  static void s_LOG_FileCleanup(void* user_data);
}
#endif /* __cplusplus */



/******************************************************************************
 *  ERROR HANDLING and LOGGING
 */

extern char* LOG_ComposeMessage
(const SLOG_Handler* call_data,
 TLOG_FormatFlags    format_flags)
{
  char* str;

  /* Calculated length of ... */
  size_t level_len     = 0;
  size_t file_line_len = 0;
  size_t module_len    = 0;
  size_t message_len   = 0;
  size_t total_len;

  /* Adjust formatting flags */
  if (call_data->level == eLOG_Trace) {
    format_flags = fLOG_Full;
  } else if (format_flags == fLOG_Default) {
#if defined(NDEBUG)
    format_flags = fLOG_Short;
#else
    format_flags = fLOG_Full;
#endif
  }

  /* Pre-calculate total message length */
  if ((format_flags & fLOG_Level) != 0) {
    level_len = strlen(LOG_LevelStr(call_data->level)) + 2;
  }
  if ((format_flags & fLOG_Module) != 0  &&
      call_data->module  &&  *call_data->module) {
    module_len = strlen(call_data->module) + 3;
  }
  if ((format_flags & fLOG_FileLine) != 0  &&
      call_data->file  &&  *call_data->file) {
    file_line_len = 12 + strlen(call_data->file) + 11;
  }
  if (call_data->message  &&  *call_data->message) {
    message_len = strlen(call_data->message);
  }

  /* Allocate memory for the resulting message */
  total_len = file_line_len + module_len + level_len + message_len;
  str = (char*) malloc(total_len + 1);
  if ( !str ) {
    assert(0);
    return 0;
  }

  /* Compose the message */
  str[0] = '\0';
  if ( file_line_len ) {
    sprintf(str, "\"%s\", line %d: ", call_data->file, (int) call_data->line);
  }
  if ( module_len ) {
    strcat(str, "[");
    strcat(str, call_data->module);
    strcat(str, "] ");
  }
  if ( level_len ) {
    strcat(str, LOG_LevelStr(call_data->level));
    strcat(str, ": ");
  }
  if ( message_len ) {
    strcat(str, call_data->message);
  }
  assert(strlen(str) <= total_len);
  return str;
}


/* Callback for LOG_Reset_FILE() */
static void s_LOG_FileHandler(void* user_data, SLOG_Handler* call_data)
{
  FILE* fp = (FILE*) user_data;
  assert(call_data);

  if ( fp ) {
    char* str = LOG_ComposeMessage(call_data, fLOG_Default);
    if ( str ) {
      fprintf(fp, "%s\n", str);
      free(str);
    }
  }
}


/* Callback for LOG_Reset_FILE() */
static void s_LOG_FileCleanup(void* user_data)
{
  FILE* fp = (FILE*) user_data;

  if ( fp )
    fclose(fp);
}


extern void LOG_ToFILE
(LOG         lg,
 FILE*       fp,
 int/*bool*/ auto_close)
{
  if ( fp ) {
    if ( auto_close ) {
      LOG_Reset(lg, fp, s_LOG_FileHandler, s_LOG_FileCleanup, 1/*true*/);
    } else {
      LOG_Reset(lg, fp, s_LOG_FileHandler, 0, 0/*true*/);
    }
  } else {
    LOG_Reset(lg, 0, 0, 0, 0/*true*/);
  }
}


/* Return non-zero value if "*beg" has reached the "end"
 */
static int/*bool*/ s_SafeCopy(const char* src, char** beg, const char* end)
{
  assert(*beg <= end);
  if ( src ) {
    for ( ;  *src  &&  *beg != end;  src++, (*beg)++) {
        **beg = *src;
    }
  }
  **beg = '\0';
  return (*beg == end);
}


extern char* MessagePlusErrno
(const char*  message,
 int          x_errno,
 const char*  descr,
 char*        buf,
 size_t       buf_size)
{
  char* beg;
  char* end;

  /* Check and init */
  if (!buf  ||  !buf_size)
    return 0;
  buf[0] = '\0';
  if (buf_size < 2)
    return buf;  /* empty */

  /* Adjust the description, if necessary and possible */
  if (x_errno  &&  !descr) {
    descr = strerror(x_errno);
    if ( !descr ) {
      static const char s_UnknownErrno[] = "Error code is out of range";
      descr = s_UnknownErrno;
    }
  }

  /* Check for an empty result, calculate string lengths */
  if ((!message  ||  !*message)  &&  !x_errno  &&  (!descr  ||  !*descr))
    return buf;  /* empty */

  /* Compose:   <message> {errno:<x_errno> <descr>} */
  beg = buf;
  end = buf + buf_size - 1;

  /* <message> */
  if ( s_SafeCopy(message, &beg, end) )
    return buf;

  /* {errno:<x_errno> <descr>} */
  if (!x_errno  &&  (!descr  ||  !*descr))
      return buf;

  /* "{errno:" */
  if ( s_SafeCopy(" {errno:", &beg, end) )
    return buf;

  /* <x_errno> */
  {{
    int/*bool*/ neg;
    /* calculate length */
    size_t len;
    int    mod;

    if (x_errno < 0) {
      neg = 1/*true*/;
      x_errno = -x_errno;
    } else {
      neg = 0/*false*/;
    }

    for (len = 1, mod = 1;  (x_errno / mod) > 9;  len++, mod *= 10);
    if ( neg )
      len++;

    /* ? not enough space */
    if (beg + len >= end) {
      s_SafeCopy("....", &beg, end);
      return buf;
    }

    /* ? add sign */ 
    if (x_errno < 0) {
      *beg++ = '-';
    }

    /* print error code */
    for ( ;  mod;  mod /= 10) {
      static const char s_Num[] = "0123456789";
      assert(x_errno / mod < 10);
      *beg++ = s_Num[x_errno / mod];
      x_errno %= mod;
    }
  }}

  /* " <descr>" */
  if (descr  &&  *descr  &&  beg != end)
    *beg++ = ' ';
  if ( s_SafeCopy(descr, &beg, end) )
    return buf;

  /* "}\0" */
  assert(beg <= end);
  if (beg != end)
    *beg++ = '}';
  *beg = '\0';

  return buf;
}

