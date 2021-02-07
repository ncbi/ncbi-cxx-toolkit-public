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
#include "ncbi_once.h"
#include "ncbi_priv.h"
#include <stdlib.h>

#ifdef NCBI_OS_UNIX
#  include <unistd.h>
#endif /*NCBI_OS_UNIX*/

#ifdef NCBI_CXX_TOOLKIT
#  if   defined(NCBI_POSIX_THREADS)
#    include <pthread.h>
#  elif defined(NCBI_WIN32_THREADS)
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#  endif
#endif /*NCBI_CXX_TOOLKIT*/


/******************************************************************************
 *  I/O status
 */

extern const char* IO_StatusStr(EIO_Status status)
{
    static const char* kStatusStr[EIO_N_STATUS] = {
        "Success",
        "Timeout",
        "",
        "Interrupt",
        "Invalid argument",
        "Not supported",
        "Unknown",
        "Closed"
    };

    assert(eIO_Success <= (int) status  &&  (int) status < EIO_N_STATUS
           &&  kStatusStr[status]);
    return eIO_Success <= (int) status  &&  (int) status < EIO_N_STATUS
        ? kStatusStr[status]
        : 0;
}


/******************************************************************************
 *  MT locking
 */

/* Check the validity of the MT lock */
#define MT_LOCK_VALID  assert(lk->count  &&  lk->magic == kMT_LOCK_magic)


/* MT lock data and callbacks */
struct MT_LOCK_tag {
    volatile unsigned int count;    /* reference count                  */
    void*                 data;     /* for "handler()" and "cleanup()"  */
    FMT_LOCK_Handler      handler;  /* handler callback for [un]locking */
    FMT_LOCK_Cleanup      cleanup;  /* cleanup callback for "data"      */
    unsigned int          magic;    /* internal consistency assurance   */
};
#define kMT_LOCK_magic  0x7A96283F


#if defined(NCBI_CXX_TOOLKIT)  &&  defined(NCBI_THREADS)

#  if   defined(PTHREAD_RECURSIVE_MUTEX_INITIALIZER)
#    define NCBI_RECURSIVE_MUTEX_INIT  PTHREAD_RECURSIVE_MUTEX_INITIALIZER
#  elif defined(PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP)
#    define NCBI_RECURSIVE_MUTEX_INIT  PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP
#  endif      /*PTHREAD_RECURSIVE_MUTEX_INITIALIZER...*/


/*ARGSUSED*/
static int/*bool*/ s_CORE_MT_Lock_default_handler(void*    unused,
                                                  EMT_Lock action)
{

#  if   defined(NCBI_POSIX_THREADS)

    static pthread_mutex_t sx_Mutex
#    ifdef  NCBI_RECURSIVE_MUTEX_INIT
        =   NCBI_RECURSIVE_MUTEX_INIT
#    endif/*NCBI_RECURSIVE_MUTEX_INIT*/
        ;

#    ifndef NCBI_RECURSIVE_MUTEX_INIT
    static void* /*bool*/ sx_Init   = 0/*false*/;
    static int   /*bool*/ sx_Inited = 0/*false*/;
    if (CORE_Once(&sx_Init)) {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&sx_Mutex, &attr);
        pthread_mutexattr_destroy(&attr);
        sx_Inited = 1; /*go*/
    } else while (!sx_Inited)
        CORE_Msdelay(1/*ms*/); /*spin*/
#    endif /*!NCBI_RECURSIVE_MUTEX_INIT*/

    switch (action) {
    case eMT_Lock:
    case eMT_LockRead:
        return pthread_mutex_lock   (&sx_Mutex) == 0 ? 1/*ok*/ : 0/*fail*/;
    case eMT_Unlock:
        return pthread_mutex_unlock (&sx_Mutex) == 0 ? 1/*ok*/ : 0/*fail*/;
    case eMT_TryLock:
    case eMT_TryLockRead:
        return pthread_mutex_trylock(&sx_Mutex) == 0 ? 1/*ok*/ : 0/*fail*/;
    }
    return 0/*failure*/;

#  elif defined(NCBI_WIN32_THREADS)

    static CRITICAL_SECTION sx_Crit;
    static LONG /*bool*/    sx_Init   = 0/*false*/;
    static int  /*bool*/    sx_Inited = 0/*false*/;

    if (!InterlockedCompareExchange(&sx_Init, 1, 0)) {
        InitializeCriticalSection(&sx_Crit);
        sx_Inited = 1; /*go*/
    } else while (!sx_Inited)
        CORE_Msdelay(1/*ms*/); /*spin*/

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

    if (g_CORE_Log) {
        static void* /*bool*/ sx_Once = 0/*false*/;
        if (CORE_Once(&sx_Once))
            CORE_LOG(eLOG_Critical, "Using uninitialized CORE MT-LOCK");
    }
    return -1/*not implemented*/;

#  endif /*NCBI_..._THREADS*/
}

#endif /*NCBI_CXX_TOOLKIT && NCBI_THREADS*/


struct MT_LOCK_tag g_CORE_MT_Lock_default = {
    1/* ref count */,
    0/* user data */,
#if defined(NCBI_CXX_TOOLKIT)  &&  defined(NCBI_THREADS)
    s_CORE_MT_Lock_default_handler,
#else
    0/* noop handler */,
#endif /*NCBI_CXX_TOOLKIT && NCBI_THREADS*/
    0/* cleanup */,
    kMT_LOCK_magic
};


extern MT_LOCK MT_LOCK_Create
(void*            data,
 FMT_LOCK_Handler handler,
 FMT_LOCK_Cleanup cleanup)
{
    MT_LOCK lk = (struct MT_LOCK_tag*) malloc(sizeof(struct MT_LOCK_tag));

    if (lk) {
        lk->count   = 1;
        lk->data    = data;
        lk->handler = handler;
        lk->cleanup = cleanup;
        lk->magic   = kMT_LOCK_magic;
        // CORE_TRACEF(("MT_LOCK_Create(%p)", lk));
    }
    return lk;
}


extern MT_LOCK MT_LOCK_AddRef(MT_LOCK lk)
{
    if (lk) {
        MT_LOCK_VALID;
        if (lk != &g_CORE_MT_Lock_default) {
            // CORE_DEBUG_ARG(unsigned int count;)
            MT_LOCK_Do(lk, eMT_Lock);
            // CORE_DEBUG_ARG(count =)
            ++lk->count;
            MT_LOCK_Do(lk, eMT_Unlock);
            // CORE_TRACEF(("MT_LOCK_AddRef(%p) = %u", lk, count));
        }
    }
    return lk;
}


extern MT_LOCK MT_LOCK_Delete(MT_LOCK lk)
{
    if (lk) {
        MT_LOCK_VALID;
        if (lk != &g_CORE_MT_Lock_default) {
            unsigned int count;
            if (lk->handler)
                verify(lk->handler(lk->data, eMT_Lock));
            count = --lk->count;
            if (lk->handler)
                verify(lk->handler(lk->data, eMT_Unlock));
            // CORE_TRACEF(("MT_LOCK_Delete(%p) = %u", lk, count));
            if (!count) {
                if (lk->cleanup)
                    lk->cleanup(lk->data);

                lk->magic++;
                free(lk);
                lk = 0;
            }
        }
    }
    return lk;
}


extern int/*bool*/ MT_LOCK_DoInternal(MT_LOCK lk, EMT_Lock how)
{
    MT_LOCK_VALID;
    return lk->handler ? lk->handler(lk->data, how) : -1/*rightful non-doing*/;
}



/******************************************************************************
 *  ERROR HANDLING and LOGGING
 */

/* Lock/unlock the logger */
#define LOG_LOCK_WRITE  verify(MT_LOCK_Do(lg->lock, eMT_Lock)     != 0)
#define LOG_LOCK_READ   verify(MT_LOCK_Do(lg->lock, eMT_LockRead) != 0)
#define LOG_UNLOCK      verify(MT_LOCK_Do(lg->lock, eMT_Unlock)   != 0)


/* Check the validity of the logger */
#define LOG_VALID  assert(lg->count  &&  lg->magic == kLOG_magic)


/* Logger data and callbacks */
struct LOG_tag {
    unsigned int count;
    void*        data;
    FLOG_Handler handler;
    FLOG_Cleanup cleanup;
    MT_LOCK      lock;
    unsigned int magic;
};
#define kLOG_magic  0x3FB97156


extern const char* LOG_LevelStr(ELOG_Level level)
{
    static const char* kPostSeverityStr[eLOG_Fatal + 1] = {
        "TRACE",
        "NOTE",
        "WARNING",
        "ERROR",
        "CRITICAL",
        "FATAL"
    };

    assert(eLOG_Trace <= level  &&  level <= eLOG_Fatal);
    return eLOG_Trace <= level  &&  level <= eLOG_Fatal
        ? kPostSeverityStr[level]
        : 0;
}


extern LOG LOG_Create
(void*        data,
 FLOG_Handler handler,
 FLOG_Cleanup cleanup,
 MT_LOCK      lock)
{
    LOG lg = (struct LOG_tag*) malloc(sizeof(struct LOG_tag));

    if (lg) {
        lg->count   = 1;
        lg->data    = data;
        lg->handler = handler;
        lg->cleanup = cleanup;
        lg->lock    = MT_LOCK_AddRef(lock);
        lg->magic   = kLOG_magic;
    }
    return lg;
}


extern LOG LOG_Reset
(LOG          lg,
 void*        data,
 FLOG_Handler handler,
 FLOG_Cleanup cleanup)
{
    LOG_LOCK_WRITE;
    LOG_VALID;

    if (lg->cleanup)
        lg->cleanup(lg->data);

    lg->data    = data;
    lg->handler = handler;
    lg->cleanup = cleanup;

    LOG_UNLOCK;
    return lg;
}


extern LOG LOG_AddRef(LOG lg)
{
    LOG_LOCK_WRITE;
    LOG_VALID;

    lg->count++;

    LOG_UNLOCK;
    return lg;
}


extern LOG LOG_Delete(LOG lg)
{
    if (lg) {
        LOG_LOCK_WRITE;
        LOG_VALID;

        if (lg->count > 1) {
            lg->count--;
            LOG_UNLOCK;
            return lg;
        }

        LOG_UNLOCK;

        LOG_Reset(lg, 0, 0, 0);
        lg->count--;
        lg->magic++;

        lg->lock = MT_LOCK_Delete(lg->lock);
        free(lg);
    }
    return 0;
}


extern void LOG_WriteInternal
(LOG                 lg,
 const SLOG_Message* mess
 )
{
    assert(!mess->raw_size  ||  mess->raw_data);

    if (lg) {
        LOG_LOCK_READ;
        LOG_VALID;

        if (lg->handler)
            lg->handler(lg->data, mess);

        LOG_UNLOCK;

    }

    if (mess->dynamic  &&  mess->message)
        free((void*) mess->message);

    /* unconditional exit/abort on fatal error */
    if (mess->level == eLOG_Fatal) {
#ifdef NDEBUG
        fflush(0);
        _exit(255);
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
    SLOG_Message mess;

    mess.dynamic     = 0;
    mess.message     = message;
    mess.level       = level;
    mess.module      = module;
    mess.func        = func;
    mess.file        = file;
    mess.line        = line;
    mess.raw_data    = raw_data;
    mess.raw_size    = raw_size;
    mess.err_code    = code;
    mess.err_subcode = subcode;

    LOG_WriteInternal(lg, &mess);
}



/******************************************************************************
 *  REGISTRY
 */

/* Lock/unlock the registry  */
#define REG_LOCK_WRITE  verify(MT_LOCK_Do(rg->lock, eMT_Lock)     != 0)
#define REG_LOCK_READ   verify(MT_LOCK_Do(rg->lock, eMT_LockRead) != 0)
#define REG_UNLOCK      verify(MT_LOCK_Do(rg->lock, eMT_Unlock)   != 0)


/* Check the validity of the registry */
#define REG_VALID  assert(rg->count  &&  rg->magic == kREG_magic)


/* Logger data and callbacks */
struct REG_tag {
    unsigned int count;
    void*        data;
    FREG_Get     get;
    FREG_Set     set;
    FREG_Cleanup cleanup;
    MT_LOCK      lock;
    unsigned int magic;
};
#define kREG_magic  0xA921BC08


extern REG REG_Create
(void*        data,
 FREG_Get     get,
 FREG_Set     set,
 FREG_Cleanup cleanup,
 MT_LOCK      lock)
{
    REG rg = (struct REG_tag*) malloc(sizeof(struct REG_tag));

    if (rg) {
        rg->count   = 1;
        rg->data    = data;
        rg->get     = get;
        rg->set     = set;
        rg->cleanup = cleanup;
        rg->lock    = MT_LOCK_AddRef(lock);
        rg->magic   = kREG_magic;
    }
    return rg;
}


extern void REG_Reset
(REG          rg,
 void*        data,
 FREG_Get     get,
 FREG_Set     set,
 FREG_Cleanup cleanup,
 int/*bool*/  do_cleanup)
{
    REG_LOCK_WRITE;
    REG_VALID;

    if (rg->cleanup  &&  do_cleanup)
        rg->cleanup(rg->data);

    rg->data    = data;
    rg->get     = get;
    rg->set     = set;
    rg->cleanup = cleanup;

    REG_UNLOCK;
}


extern REG REG_AddRef(REG rg)
{
    REG_LOCK_WRITE;
    REG_VALID;

    rg->count++;

    REG_UNLOCK;
    return rg;
}


extern REG REG_Delete(REG rg)
{
    if (rg) {
        REG_LOCK_WRITE;
        REG_VALID;

        if (rg->count > 1) {
            rg->count--;
            REG_UNLOCK;
            return rg;
        }

        REG_UNLOCK;

        REG_Reset(rg, 0, 0, 0, 0, 1/*true*/);
        rg->count--;
        rg->magic++;

        rg->lock = MT_LOCK_Delete(rg->lock);
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
    int rv;
    if (!value  ||  value_size <= 0)
        return 0/*failed*/;

    *value = '\0';
    if (rg) {
        REG_LOCK_READ;
        REG_VALID;

        rv = (rg->get
              ? rg->get(rg->data, section, name, value, value_size)
              : -1/*default*/);

        REG_UNLOCK;
    } else
        rv = -1/*default*/;

    if ((rv < 0  ||  !*value)  &&  def_value  &&  *def_value) {
        size_t len = strlen(def_value);
        if (len >= value_size) {
            len  = value_size - 1;
            rv   = 0;
        } else
            rv   = 1;
        strncpy0(value, def_value, len);
    }
    return rv ? value : 0;
}


extern int/*bool*/ REG_Set
(REG          rg,
 const char*  section,
 const char*  name,
 const char*  value,
 EREG_Storage storage)
{
    int/*bool*/ rv;

    if (rg) {
        REG_LOCK_READ;
        REG_VALID;

        rv = (rg->set
              ? rg->set(rg->data, section, name, value, storage)
              : 0/*failed*/);

        REG_UNLOCK;
    } else
        rv = 0/*failed*/;

    return rv;
}
