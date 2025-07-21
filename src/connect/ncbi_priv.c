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
 * Author:  Denis Vakatov
 *
 * File Description:
 *   Private aux. code for the "ncbi_*.[ch]"
 *
 */

#include "ncbi_priv.h"
#if   defined(NCBI_OS_UNIX)
#  include <unistd.h>
#elif defined(NCBI_OS_MSWIN)
#  include <windows.h>
#else
#  include <connect/ncbi_socket.h>
#endif /*NCBI_OS*/
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>


/* GLOBALS */
TCORE_Set           g_CORE_Set               = 0;
MT_LOCK             g_CORE_MT_Lock           = &g_CORE_MT_Lock_default;
LOG                 g_CORE_Log               = 0;
REG                 g_CORE_Registry          = 0;
unsigned int        g_NCBI_ConnectRandomSeed = 0;
FNcbiGetAppName     g_CORE_GetAppName        = 0;
FNcbiGetReferer     g_CORE_GetReferer        = 0;
FNcbiGetRequestID   g_CORE_GetRequestID      = 0;
FNcbiGetRequestDtab g_CORE_GetRequestDtab    = 0;

#ifdef NCBI_MONKEY
FMonkeySend         g_MONKEY_Send            = 0;
FMonkeyRecv         g_MONKEY_Recv            = 0;
FMonkeyConnect      g_MONKEY_Connect         = 0;
FMonkeyPoll         g_MONKEY_Poll            = 0;
FMonkeyClose        g_MONKEY_Close           = 0;
FSockHasSocket      g_MONKEY_SockHasSocket   = 0;
#endif /*NCBI_MONKEY*/


extern unsigned int g_NCBI_ConnectSrandAddend(void)
{
#if   defined(NCBI_OS_UNIX)
    return (unsigned int) getpid(); 
#elif defined(NCBI_OS_MSWIN)
    return (unsigned int) GetCurrentProcessId();
#else
    return SOCK_GetLocalHostAddress(eDefault);
#endif /*NCBI_OS*/ 
}


#ifdef _DEBUG

/* NB: Can only track the most recent lock / unlock;  cannot handle nested! */

static MT_LOCK s_CoreLock = 0;

extern int g_NCBI_CoreCheckLock(void)
{
    /* save last lock accessed */
    s_CoreLock = g_CORE_MT_Lock;
    return 1/*success*/;
}

extern int g_NCBI_CoreCheckUnlock(void)
{
    /* check that unlock operates on the same lock (excl. process run-down) */
    if (s_CoreLock != g_CORE_MT_Lock
        &&  (s_CoreLock != &g_CORE_MT_Lock_default  ||  g_CORE_MT_Lock)) {
        CORE_LOG(eLOG_Critical, "Inconsistent use of CORE MT-Lock");
        assert(0);
        return 0/*failure*/;
    }
    return 1/*success*/;
}

#endif /*_DEBUG*/


extern const char* g_CORE_Sprintf(const char* fmt, ...)
{
    static const size_t buf_size = 4096;
    va_list args;
    char*   buf;
    int     len;

    if (!(buf = (char*) malloc(buf_size)))
        return 0;
    *buf = '\0';

    va_start(args, fmt);
#ifndef HAVE_VSNPRINTF
    len = vsprintf (buf,           fmt, args);
#else
    len = vsnprintf(buf, buf_size, fmt, args);
    if (len < 0  ||  buf_size <= len) {
        memcpy(&buf[buf_size - 4], "...", 4);
        len = (int)(buf_size - 1);
    } else
#endif /*HAVE_VSNPRINTF*/
    if (len <= 0)
        *buf = '\0', len = 0;
    assert(0 <= len  &&  len < buf_size);
    va_end(args);
    return buf;
}


extern const char* g_CORE_RegistryGET
(const char*  section,
 const char*  name,
 char*        value,
 size_t       value_size,
 const char*  def_value)
{
    const char* retval;
    CORE_LOCK_READ;
    retval = REG_Get(g_CORE_Registry,
                     section, name, value, value_size, def_value);
    CORE_UNLOCK;
    return retval;
}


extern int/*bool*/ g_CORE_RegistrySET
(const char*  section,
 const char*  name,
 const char*  value,
 EREG_Storage storage)
{
    int/*bool*/ retval;
    CORE_LOCK_READ;
    retval = REG_Set(g_CORE_Registry,
                     section, name, value, storage);
    CORE_UNLOCK;
    return retval;
}
