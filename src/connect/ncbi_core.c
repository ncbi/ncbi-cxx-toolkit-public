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
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.1  2000/02/23 22:36:16  vakatov
 * Initial revision
 *
 * ===========================================================================
 */

#include <connect/ncbi_core.h>
#include <stdlib.h>


/******************************************************************************
 *  IO status
 */

extern const char* IO_StatusStr(EIO_Status status)
{
    static const char* s_StatusStr[eIO_Unknown+1] = {
        "Success",
        "Timeout",
        "Closed",
        "Invalid Argument",
        "Not Supported",
        "Unknown"
    };

    return s_StatusStr[status];
}



/******************************************************************************
 *  MT locking
 */

struct MT_LOCK_tag {
  unsigned int     ref_count;    /* reference counter */
  void*            user_data;    /* for "handler()" and "cleanup()" */
  FMT_LOCK_Handler handler;      /* locking function */
  FMT_LOCK_Cleanup cleanup;      /* cleanup function */
  unsigned int     magic_number; /* used internally to make sure it's init'd */
};
static const unsigned int s_MT_LOCK_magic_number = 0x7A96283F;


extern MT_LOCK MT_LOCK_Create
(void*            user_data,
 FMT_LOCK_Handler handler,
 FMT_LOCK_Cleanup cleanup)
{
  MT_LOCK lk = (struct MT_LOCK_tag*) malloc(sizeof(struct MT_LOCK_tag));

  lk->ref_count = 1;
  lk->user_data = user_data;
  lk->handler   = handler;
  lk->cleanup   = cleanup;
  lk->magic_number = s_MT_LOCK_magic_number;

  return lk;
}


extern MT_LOCK MT_LOCK_AddRef(MT_LOCK lk)
{
  assert(lk->ref_count  &&  lk->magic_number == s_MT_LOCK_magic_number);
  lk->ref_count++;
  return lk;
}


extern MT_LOCK MT_LOCK_Delete(MT_LOCK lk)
{
  if ( !lk )
    return 0;
  assert(lk->ref_count  &&  lk->magic_number == s_MT_LOCK_magic_number);

  if ( --lk->ref_count )
    return lk;

  if ( lk->handler ) {  /* weak extra protection */
    verify(lk->handler(lk->user_data, eMT_Lock));
    verify(lk->handler(lk->user_data, eMT_Unlock));
  }

  if ( lk->cleanup )
    lk->cleanup(lk->user_data);

  lk->magic_number++;
  free(lk);
  return 0;
}


extern int/*bool*/ MT_LOCK_DoInternal(MT_LOCK lk, EMT_Lock how)
{
  assert(lk->ref_count  &&  lk->magic_number == s_MT_LOCK_magic_number);

  if ( lk->handler )
    return lk->handler(lk->user_data, how);

  return -1 /* rightful non-doing */;
}



/******************************************************************************
 *  ERROR HANDLING and LOGGING
 */

#define LOG_LOCK_WRITE  verify(MT_LOCK_Do(lg->mt_lock, eMT_Lock))
#define LOG_LOCK_READ   verify(MT_LOCK_Do(lg->mt_lock, eMT_LockRead))
#define LOG_UNLOCK      verify(MT_LOCK_Do(lg->mt_lock, eMT_Unlock))


struct LOG_tag {
  unsigned int ref_count;
  void*        user_data;
  FLOG_Handler handler;
  FLOG_Cleanup cleanup;
  MT_LOCK      mt_lock;
  unsigned int magic_number;  /* used internally, to make sure it's init'd */
};
static const unsigned int s_LOG_magic_number = 0x3FB97156;


extern const char* LOG_LevelStr(ELOG_Level level)
{
    static const char* s_PostSeverityStr[eLOG_Fatal+1] = {
        "TRACE",
        "NOTE",
        "WARNING",
        "ERROR",
        "CRITICAL_ERROR",
        "FATAL_ERROR"
    };
    return s_PostSeverityStr[level];
}


extern LOG LOG_Create
(void*        user_data,
 FLOG_Handler handler,
 FLOG_Cleanup cleanup,
 MT_LOCK      mt_lock)
{
  LOG lg = (struct LOG_tag*) malloc(sizeof(struct LOG_tag));

  lg->ref_count = 1;
  lg->user_data = user_data;
  lg->handler   = handler;
  lg->cleanup   = cleanup;
  lg->mt_lock   = mt_lock;
  lg->magic_number = s_LOG_magic_number;

  return lg;
}


extern void LOG_Reset
(LOG          lg,
 void*        user_data,
 FLOG_Handler handler,
 FLOG_Cleanup cleanup,
 int/*bool*/  do_cleanup)
{
  LOG_LOCK_WRITE;
  assert(lg->ref_count  &&  lg->magic_number == s_LOG_magic_number);

  if (do_cleanup  &&  lg->cleanup)
    lg->cleanup(lg->user_data);

  lg->user_data = user_data;
  lg->handler   = handler;
  lg->cleanup   = cleanup;
  LOG_UNLOCK;
}


extern LOG LOG_AddRef(LOG lg)
{
  LOG_LOCK_WRITE;
  assert(lg->ref_count  &&  lg->magic_number == s_LOG_magic_number);

  lg->ref_count++;
  LOG_UNLOCK;

  return lg;
}


extern LOG LOG_Delete(LOG lg)
{
  if ( !lg )
    return 0;

  LOG_LOCK_WRITE;
  assert(lg->ref_count  &&  lg->magic_number == s_LOG_magic_number);

  if (lg->ref_count > 1) {
    lg->ref_count--;
    LOG_UNLOCK;
    return lg;
  }
  LOG_UNLOCK;

  LOG_Reset(lg, 0, 0, 0, 1/*true*/);
  lg->ref_count--;
  lg->magic_number++;
  if ( lg->mt_lock )
    MT_LOCK_Delete(lg->mt_lock);
  free(lg);
  return 0;
}


extern void LOG_WriteInternal
(LOG         lg,
 ELOG_Level  level,
 const char* module,
 const char* file,
 int         line,
 const char* message)
{
  if ( !lg )
    return;

  LOG_LOCK_READ;
  assert(lg->ref_count  &&  lg->magic_number == s_LOG_magic_number);

  if ( lg->handler ) {
    SLOG_Handler call_data;
    call_data.level   = level;
    call_data.module  = module;
    call_data.file    = file;
    call_data.line    = line;
    call_data.message = message;

    lg->handler(lg->user_data, &call_data);
  }
  LOG_UNLOCK;
}
