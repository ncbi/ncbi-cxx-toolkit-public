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
 * Author:  Denis Vakatov, Anton Lavrentiev
 *
 * File Description:
 *   Types and code shared by all "ncbi_*.[ch]" modules.
 *
 */

#include "ncbi_ansi_ext.h"
#include "ncbi_priv.h"
#include <stdlib.h>

#if defined(NCBI_CXX_TOOLKIT)  &&  defined(_MT)  &&  !defined(NCBI_WITHOUT_MT)
#  if defined(NCBI_OS_MSWIN)
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#    define NCBI_WIN32_THREADS
#  elif defined(NCBI_OS_UNIX)
#    include <pthread.h>
#    define NCBI_POSIX_THREADS
#  else
#    define NCBI_NO_THREADS
#  endif /*NCBI_OS*/
#else
#  define   NCBI_NO_THREADS
#endif /*NCBI_CXX_TOOLKT && _MT && !NCBI_WITHOUT_MT*/



/******************************************************************************
 *  IO status
 */

extern const char* IO_StatusStr(EIO_Status status)
{
    static const char* s_StatusStr[eIO_Unknown + 1] = {
        "Success",
        "Timeout",
        "Closed",
        "Interrupt",
        "Invalid argument",
        "Not supported",
        "Unknown"
    };

    assert(status >= eIO_Success  &&  status <= eIO_Unknown);
    return s_StatusStr[status];
}



/******************************************************************************
 *  MT locking
 */

/* Check the validity of the MT locker */
#define MT_LOCK_VALID  \
    assert(lk->ref_count  &&  lk->magic_number == kMT_LOCK_magic_number)


/* MT locker data and callbacks */
struct MT_LOCK_tag {
  unsigned int     ref_count;    /* reference counter */
  void*            user_data;    /* for "handler()" and "cleanup()" */
  FMT_LOCK_Handler handler;      /* locking function */
  FMT_LOCK_Cleanup cleanup;      /* cleanup function */
  unsigned int     magic_number; /* used internally to make sure it's init'd */
};
#define kMT_LOCK_magic_number 0x7A96283F


#ifndef NCBI_NO_THREADS
/*ARGSUSED*/
static int/*bool*/ s_CORE_MT_Lock_default_handler(void*    unused,
                                                  EMT_Lock action)
{
#  if   defined(NCBI_POSIX_THREADS)  &&  \
        defined(PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP)

    static pthread_mutex_t sx_Mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

    switch (action) {
    case eMT_Lock:
    case eMT_LockRead:
        return pthread_mutex_lock(&sx_Mutex)    == 0 ? 1/*ok*/ : 0/*fail*/;
    case eMT_Unlock:
        return pthread_mutex_unlock(&sx_Mutex)  == 0 ? 1/*ok*/ : 0/*fail*/;
    case eMT_TryLock:
    case eMT_TryLockRead:
        return pthread_mutex_trylock(&sx_Mutex) == 0 ? 1/*ok*/ : 0/*fail*/;
    }
    return 0/*failure*/;

#  elif defined(NCBI_WIN32_THREADS)

    static CRITICAL_SECTION sx_Crit;
    static LONG             sx_Init   = 0;
    static int/*bool*/      sx_Inited = 0/*false*/;

    LONG init = InterlockedCompareExchange(&sx_Init, 1, 0);
    if (!init) {
        InitializeCriticalSection(&sx_Crit);
        sx_Inited = 1; /*go*/
    } else while (!sx_Inited)
        Sleep(10/*ms*/); /*spin*/

    switch (action) {
    case eMT_Lock:
    case eMT_LockRead:
        EnterCriticalSection(&sx_Crit);
        return 1/*success*/;
    case eMT_Unlock:
        LeaveCriticalSection(&sx_Crit);
        return 1/*success*/;
    case eMT_TryLock:
    case eMT_TryLockRead:
        return TryEnterCriticalSection(&sx_Crit) ? 1/*ok*/ : 0/*fail*/;
    }
    return 0/*failure*/;

#  else

    return -1/*not implemented*/;

#  endif /*NCBI_..._THREADS*/
}
#endif /*!NCBI_NO_THREADS*/


struct MT_LOCK_tag g_CORE_MT_Lock_default = {
    1/* ref count */,
    0/* user data */,
#ifndef NCBI_NO_THREADS
    s_CORE_MT_Lock_default_handler,
#else
    0/* noop handler */,
#endif /*NCBI_NO_THREADS*/
    0/* cleanup */,
    kMT_LOCK_magic_number
};


extern MT_LOCK MT_LOCK_Create
(void*            user_data,
 FMT_LOCK_Handler handler,
 FMT_LOCK_Cleanup cleanup)
{
    MT_LOCK lk = (struct MT_LOCK_tag*) malloc(sizeof(struct MT_LOCK_tag));

    if (lk) {
        lk->ref_count    = 1;
        lk->user_data    = user_data;
        lk->handler      = handler;
        lk->cleanup      = cleanup;
        lk->magic_number = kMT_LOCK_magic_number;
    }
    return lk;
}


extern MT_LOCK MT_LOCK_AddRef(MT_LOCK lk)
{
    MT_LOCK_VALID;
    if (lk != &g_CORE_MT_Lock_default)
        lk->ref_count++;
    return lk;
}


extern MT_LOCK MT_LOCK_Delete(MT_LOCK lk)
{
    if (lk  &&  lk != &g_CORE_MT_Lock_default) {
        MT_LOCK_VALID;

        if (!--lk->ref_count) {
            if (lk->handler) {  /* weak extra protection */
                verify(lk->handler(lk->user_data, eMT_Lock));
                verify(lk->handler(lk->user_data, eMT_Unlock));
            }

            if (lk->cleanup)
                lk->cleanup(lk->user_data);

            lk->magic_number++;
            free(lk);
            lk = 0;
        }
    }
    return lk;
}


extern int/*bool*/ MT_LOCK_DoInternal(MT_LOCK lk, EMT_Lock how)
{
    MT_LOCK_VALID;

    return lk->handler
        ? lk->handler(lk->user_data, how)
        : -1/* rightful non-doing */;
}



/******************************************************************************
 *  ERROR HANDLING and LOGGING
 */

/* Lock/unlock the logger */
#define LOG_LOCK_WRITE  verify(MT_LOCK_Do(lg->mt_lock, eMT_Lock))
#define LOG_LOCK_READ   verify(MT_LOCK_Do(lg->mt_lock, eMT_LockRead))
#define LOG_UNLOCK      verify(MT_LOCK_Do(lg->mt_lock, eMT_Unlock))


/* Check the validity of the logger */
#define LOG_VALID  \
    assert(lg->ref_count  &&  lg->magic_number == kLOG_magic_number)


/* Logger data and callbacks */
struct LOG_tag {
    unsigned int ref_count;
    void*        user_data;
    FLOG_Handler handler;
    FLOG_Cleanup cleanup;
    MT_LOCK      mt_lock;
    unsigned int magic_number;  /* used internally, to make sure it's init'd */
};
#define kLOG_magic_number 0x3FB97156


extern const char* LOG_LevelStr(ELOG_Level level)
{
    static const char* s_PostSeverityStr[eLOG_Fatal+1] = {
        "TRACE",
        "NOTE",
        "WARNING",
        "ERROR",
        "CRITICAL",
        "FATAL"
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

    if (lg) {
        lg->ref_count    = 1;
        lg->user_data    = user_data;
        lg->handler      = handler;
        lg->cleanup      = cleanup;
        lg->mt_lock      = mt_lock;
        lg->magic_number = kLOG_magic_number;
    }
    return lg;
}


extern LOG LOG_Reset
(LOG          lg,
 void*        user_data,
 FLOG_Handler handler,
 FLOG_Cleanup cleanup)
{
    LOG_LOCK_WRITE;
    LOG_VALID;

    if (lg->cleanup)
        lg->cleanup(lg->user_data);

    lg->user_data = user_data;
    lg->handler   = handler;
    lg->cleanup   = cleanup;

    LOG_UNLOCK;
    return lg;
}


extern LOG LOG_AddRef(LOG lg)
{
    LOG_LOCK_WRITE;
    LOG_VALID;

    lg->ref_count++;

    LOG_UNLOCK;
    return lg;
}


extern LOG LOG_Delete(LOG lg)
{
    if (lg) {
        LOG_LOCK_WRITE;
        LOG_VALID;

        if (lg->ref_count > 1) {
            lg->ref_count--;
            LOG_UNLOCK;
            return lg;
        }

        LOG_UNLOCK;

        LOG_Reset(lg, 0, 0, 0);
        lg->ref_count--;
        lg->magic_number++;

        if (lg->mt_lock)
            MT_LOCK_Delete(lg->mt_lock);
        free(lg);
    }
    return 0;
}


extern void LOG_WriteInternal
(LOG           lg,
 SLOG_Handler* call_data
 )
{
    assert(!call_data->raw_size  ||  call_data->raw_data);

    if (lg) {
        LOG_LOCK_READ;
        LOG_VALID;

        if (lg->handler)
            lg->handler(lg->user_data, call_data);

        LOG_UNLOCK;

        if (call_data->dynamic  &&  call_data->message)
            free((void*) call_data->message);
    }

    /* unconditional exit/abort on fatal error */
    if (call_data->level == eLOG_Fatal) {
#ifdef NDEBUG
        exit(1);
#else
        abort();
#endif /*NDEBUG*/
    }
}


extern void LOG_Write
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
 )
{
    SLOG_Handler call_data;

    call_data.dynamic     = 0;
    call_data.message     = message;
    call_data.level       = level;
    call_data.module      = module;
    call_data.func        = func;
    call_data.file        = file;
    call_data.line        = line;
    call_data.raw_data    = raw_data;
    call_data.raw_size    = raw_size;
    call_data.err_code    = code;
    call_data.err_subcode = subcode;

    LOG_WriteInternal(lg, &call_data);
}



/******************************************************************************
 *  REGISTRY
 */

/* Lock/unlock the registry  */
#define REG_LOCK_WRITE  verify(MT_LOCK_Do(rg->mt_lock, eMT_Lock))
#define REG_LOCK_READ   verify(MT_LOCK_Do(rg->mt_lock, eMT_LockRead))
#define REG_UNLOCK      verify(MT_LOCK_Do(rg->mt_lock, eMT_Unlock))


/* Check the validity of the registry */
#define REG_VALID  \
    assert(rg->ref_count  &&  rg->magic_number == kREG_magic_number)


/* Logger data and callbacks */
struct REG_tag {
    unsigned int ref_count;
    void*        user_data;
    FREG_Get     get;
    FREG_Set     set;
    FREG_Cleanup cleanup;
    MT_LOCK      mt_lock;
    unsigned int magic_number;  /* used internally, to make sure it's init'd */
};
#define kREG_magic_number 0xA921BC08


extern REG REG_Create
(void*        user_data,
 FREG_Get     get,
 FREG_Set     set,
 FREG_Cleanup cleanup,
 MT_LOCK      mt_lock)
{
    REG rg = (struct REG_tag*) malloc(sizeof(struct REG_tag));

    if (rg) {
        rg->ref_count    = 1;
        rg->user_data    = user_data;
        rg->get          = get;
        rg->set          = set;
        rg->cleanup      = cleanup;
        rg->mt_lock      = mt_lock;
        rg->magic_number = kREG_magic_number;
    }
    return rg;
}


extern void REG_Reset
(REG          rg,
 void*        user_data,
 FREG_Get     get,
 FREG_Set     set,
 FREG_Cleanup cleanup,
 int/*bool*/  do_cleanup)
{
    REG_LOCK_WRITE;
    REG_VALID;

    if (do_cleanup  &&  rg->cleanup)
        rg->cleanup(rg->user_data);

    rg->user_data = user_data;
    rg->get       = get;
    rg->set       = set;
    rg->cleanup   = cleanup;

    REG_UNLOCK;
}


extern REG REG_AddRef(REG rg)
{
    REG_LOCK_WRITE;
    REG_VALID;

    rg->ref_count++;

    REG_UNLOCK;
    return rg;
}


extern REG REG_Delete(REG rg)
{
    if (rg) {
        REG_LOCK_WRITE;
        REG_VALID;

        if (rg->ref_count > 1) {
            rg->ref_count--;
            REG_UNLOCK;
            return rg;
        }

        REG_UNLOCK;

        REG_Reset(rg, 0, 0, 0, 0, 1/*true*/);
        rg->ref_count--;
        rg->magic_number++;

        if (rg->mt_lock)
            MT_LOCK_Delete(rg->mt_lock);
        free(rg);
    }
    return 0;
}


extern const char* REG_Get
(REG         rg,
 const char* section,
 const char* name,
 char*       value,
 size_t      value_size,
 const char* def_value)
{
    if (!value  ||  value_size <= 0)
        return 0;

    if (def_value)
        strncpy0(value, def_value, value_size - 1);
    else
        *value = '\0';

    if (rg) {
        REG_LOCK_READ;
        REG_VALID;

        if (rg->get)
            rg->get(rg->user_data, section, name, value, value_size);

        REG_UNLOCK;
    }

    return value;
}


extern int/*bool*/ REG_Set
(REG          rg,
 const char*  section,
 const char*  name,
 const char*  value,
 EREG_Storage storage)
{
    int result;

    if (rg) {
        REG_LOCK_READ;
        REG_VALID;

        result = (rg->set
                  ? rg->set(rg->user_data, section, name, value, storage)
                  : 0);

        REG_UNLOCK;
    } else
        result = 0;

    return result;
}
