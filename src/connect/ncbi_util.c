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
 *   Auxiliary (optional) code mostly to support "ncbi_core.[ch]"
 *
 */

#include "ncbi_ansi_ext.h"
#include "ncbi_priv.h"
#ifndef NCBI_CXX_TOOLKIT
#  include <ncbistd.h>
#  include <ncbimisc.h>
#  include <ncbitime.h>
#else
#  include <ctype.h>
#  include <errno.h>
#  include <stdlib.h>
#  include <time.h>
#endif
#if !defined(NCBI_OS_DARWIN)  &&  defined(HAVE_POLL_H)
#  include <poll.h>
#endif /*!NCBI_OS_DARWIN && HAVE_POLL_H*/
#if defined(NCBI_OS_UNIX)
#  ifndef HAVE_GETPWUID
#    error "HAVE_GETPWUID is undefined on a UNIX system!"
#  endif /*!HAVE_GETPWUID*/
#  ifndef NCBI_OS_SOLARIS
#    include <limits.h>
#  endif /*!NCBI_OS_SOLARIS*/
#  ifdef NCBI_OS_LINUX
#    include <sys/user.h>
#  endif /*NCBI_OS_LINUX*/
#  include <pwd.h>
#  include <sys/time.h>
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <unistd.h>
#endif /*NCBI_OS_UNIX*/
#if defined(NCBI_OS_MSWIN)  ||  defined(NCBI_OS_CYGWIN)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <lmcons.h>
#endif /*NCBI_OS_MSWIN || NCBI_OS_CYGWIN*/
#if defined(PAGE_SIZE)  &&  !defined(NCBI_OS_CYGWIN)
#  define NCBI_DEFAULT_PAGE_SIZE  PAGE_SIZE
#else
#  define NCBI_DEFAULT_PAGE_SIZE  0/*unknown*/
#endif /*PAGE_SIZE && !NCBI_OS_CYGWIN*/

#define NCBI_USE_ERRCODE_X   Connect_Util

#define NCBI_USE_PRECOMPILED_CRC32_TABLES 1


/******************************************************************************
 *  MT locking
 */

extern void CORE_SetLOCK(MT_LOCK lk)
{
    MT_LOCK old_lk = g_CORE_MT_Lock;
    g_CORE_MT_Lock = lk;
    g_CORE_Set |= eCORE_SetLOCK;
    if (old_lk  &&  old_lk != lk)
        MT_LOCK_Delete(old_lk);
}


extern MT_LOCK CORE_GetLOCK(void)
{
    return g_CORE_MT_Lock;
}



/******************************************************************************
 *  ERROR HANDLING and LOGGING
 */


extern void CORE_SetLOG(LOG lg)
{
    LOG old_lg;
    CORE_LOCK_WRITE;
    old_lg = g_CORE_Log;
    g_CORE_Log = lg;
    g_CORE_Set |= eCORE_SetLOG;
    CORE_UNLOCK;
    if (old_lg  &&  old_lg != lg)
        LOG_Delete(old_lg);
}


extern LOG CORE_GetLOG(void)
{
    return g_CORE_Log;
}


extern void CORE_SetLOGFILE_Ex
(FILE*       fp,
 ELOG_Level  cut_off,
 ELOG_Level  fatal_err,
 int/*bool*/ auto_close
 )
{
    LOG lg = LOG_Create(0, 0, 0, 0);
    LOG_ToFILE_Ex(lg, fp, cut_off, fatal_err, auto_close);
    CORE_SetLOG(lg);
}


extern void CORE_SetLOGFILE
(FILE*       fp,
 int/*bool*/ auto_close)
{
    CORE_SetLOGFILE_Ex(fp, eLOG_Trace, eLOG_Fatal, auto_close);
}


extern int/*bool*/ CORE_SetLOGFILE_NAME_Ex
(const char* logfile,
 ELOG_Level  cut_off,
 ELOG_Level  fatal_err)
{
    FILE* fp = fopen(logfile, "a");
    if (!fp) {
        CORE_LOGF_ERRNO_X(1, eLOG_Error, errno,
                          ("Cannot open \"%s\"", logfile));
        return 0/*false*/;
    }

    CORE_SetLOGFILE_Ex(fp, cut_off, fatal_err, 1/*autoclose*/);
    return 1/*true*/;
}


extern int/*bool*/ CORE_SetLOGFILE_NAME
(const char* logfile
 )
{
    return CORE_SetLOGFILE_NAME_Ex(logfile, eLOG_Trace, eLOG_Fatal);
}


static TLOG_FormatFlags s_LogFormatFlags = fLOG_Default;

extern TLOG_FormatFlags CORE_SetLOGFormatFlags(TLOG_FormatFlags flags)
{
    TLOG_FormatFlags old_flags = s_LogFormatFlags;

    s_LogFormatFlags = flags;
    return old_flags;
}


extern size_t UTIL_PrintableStringSize(const char* data, size_t size)
{
    const unsigned char* c;
    size_t count;
    if (!data)
        return 0;
    if (!size)
        size = strlen(data);
    count = size;
    for (c = (const unsigned char*) data;  count;  --count, ++c) {
        if (*c == '\t'  ||  *c == '\v'  ||  *c == '\b'  ||
            *c == '\r'  ||  *c == '\f'  ||  *c == '\a'  ||
            *c == '\\'  ||  *c == '\''  ||  *c == '"') {
            size++;
        } else if (*c == '\n'  ||  !isascii(*c)  ||  !isprint(*c))
            size += 3;
    }
    return size;
}


extern char* UTIL_PrintableString(const char* data, size_t size,
                                  char* buf, int/*bool*/ flags)
{
    const unsigned char* s;
    unsigned char* d;

    if (!data  ||  !buf)
        return 0;
    if (!size)
        size = strlen(data);

    d = (unsigned char*) buf;
    for (s = (const unsigned char*) data;  size;  --size, ++s) {
        switch (*s) {
        case '\t':
            *d++ = '\\';
            *d++ = 't';
            continue;
        case '\v':
            *d++ = '\\';
            *d++ = 'v';
            continue;
        case '\b':
            *d++ = '\\';
            *d++ = 'b';
            continue;
        case '\r':
            *d++ = '\\';
            *d++ = 'r';
            continue;
        case '\f':
            *d++ = '\\';
            *d++ = 'f';
            continue;
        case '\a':
            *d++ = '\\';
            *d++ = 'a';
            continue;
        case '\n':
            *d++ = '\\';
            *d++ = 'n';
            if (flags & eUTIL_PrintableNoNewLine)
                continue;
            /*FALLTHRU*/
        case '\\':
        case '\'':
        case '"':
            *d++ = '\\';
            break;
        default:
            if (!isascii(*s)  ||  !isprint(*s)) {
                int/*bool*/ reduce;
                unsigned char v;
                if (flags & eUTIL_PrintableFullOctal)
                    reduce = 0/*false*/;
                else {
                    reduce = (size == 1  ||
                              s[1] < '0' || s[1] > '7' ? 1/*T*/ : 0/*F*/);
                }
                *d++     = '\\';
                v =  *s >> 6;
                if (v  ||  !reduce) {
                    *d++ = (unsigned char)('0' + v);
                    reduce = 0;
                }
                v = (*s >> 3) & 7;
                if (v  ||  !reduce)
                    *d++ = (unsigned char)('0' + v);
                v = *s & 7;
                *d++ = (unsigned char)('0' + v);
                continue;
            }
            break;
        }
        *d++ = *s;
    }

    return (char*) d;
}


#ifdef NCBI_OS_MSWIN
static const char* s_WinStrerror(DWORD error)
{
    TCHAR* str;
    DWORD  rv;

    if (!error)
        return 0;
    str = NULL;
    rv  = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | 
                        FORMAT_MESSAGE_FROM_SYSTEM     |
                        FORMAT_MESSAGE_MAX_WIDTH_MASK  |
                        FORMAT_MESSAGE_IGNORE_INSERTS,
                        NULL, error,
                        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                        (LPTSTR) &str, 0, NULL);
    if (!rv  &&  str) {
        LocalFree((HLOCAL) str);
        return 0;
    }
    return UTIL_TcharToUtf8OnHeap(str);
}
#endif /*NCBI_OS_MSWIN*/


extern const char* NcbiMessagePlusError
(int/*bool*/ *dynamic,
 const char*  message,
 int          error,
 const char*  descr)
{
    int/*bool*/ release;
    char*  buf;
    size_t mlen;
    size_t dlen;

    /* Check for an empty addition */
    if (!error  &&  (!descr  ||  !*descr)) {
        if (message)
            return message;
        *dynamic = 0/*false*/;
        return "";
    }

    /* Adjust description, if necessary and possible */
    release = 0/*false*/;
    if (error > 0  &&  !descr) {
#if defined(NCBI_OS_MSWIN)  &&  defined(_UNICODE)
        descr = UTIL_TcharToUtf8(_wcserror(error));
        release = 1/*true*/;
#else
        descr = strerror(error);
#endif /*NCBI_OS_MSWIN && _UNICODE*/
#ifdef NCBI_OS_MSWIN
        if (!descr  ||  !*descr  ||  strncasecmp(descr, "Unknown ", 8) == 0) {
            if (release)
                UTIL_ReleaseBuffer(descr);
            descr = s_WinStrerror(error);
            release = -1/*on heap*/;
        }
#endif /*NCBI_OS_MSWIN*/
    }
    if (descr  &&  *descr) {
        dlen = strlen(descr);
        while (dlen  &&  isspace((unsigned char) descr[dlen - 1]))
            dlen--;
        if (dlen > 1  &&  descr[dlen - 1] == '.')
            dlen--;
    } else {
        descr = "";
        dlen = 0;
    }

    mlen = message ? strlen(message) : 0;

    if (!(buf = (char*)(*dynamic  &&  message
                        ? realloc((void*) message, mlen + dlen + 40)
                        : malloc (                 mlen + dlen + 40)))) {
        if (*dynamic  &&  message)
            free((void*) message);
        *dynamic = 0;
        if (release > 0)
            UTIL_ReleaseBuffer(descr);
        else if (release < 0)
            UTIL_ReleaseBufferOnHeap(descr);
        return "Ouch! Out of memory";
    }

    if (message) {
        if (!*dynamic)
            memcpy(buf, message, mlen);
        buf[mlen++] = ' ';
    }
    memcpy(buf + mlen, "{error=", 7);
    mlen += 7;

    if (error) {
        mlen += (size_t) sprintf(buf + mlen,
#ifdef NCBI_OS_MSWIN
                                 error < 0x10000 ? "%d%s" : "0x%08X%s",
#else
                                                   "%d%s",
#endif /*NCBI_OS_MSWIN*/
                                 error, &","[!*descr]);
    }

    memcpy((char*) memcpy(buf + mlen, descr, dlen) + dlen, "}", 2);
    if (release > 0)
        UTIL_ReleaseBuffer(descr);
    else if (release < 0)
        UTIL_ReleaseBufferOnHeap(descr);

    *dynamic = 1/*true*/;
    return buf;
}


extern char* LOG_ComposeMessage
(const SLOG_Message* mess,
 TLOG_FormatFlags    flags)
{
    static const char kRawData_Beg[] =
        "\n#################### [BEGIN] Raw Data (%lu byte%s):\n";
    static const char kRawData_End[] =
        "\n#################### [_END_] Raw Data\n";

    const char *function, *level = 0;
    char *str, *s, datetime[32];

    /* Calculated length of ... */
    size_t datetime_len  = 0;
    size_t level_len     = 0;
    size_t module_len    = 0;
    size_t function_len  = 0;
    size_t file_line_len = 0;
    size_t message_len   = 0;
    size_t data_len      = 0;
    size_t total_len;

    /* Adjust formatting flags */
    if (mess->level == eLOG_Trace  &&  !(flags & fLOG_None))
        flags |= fLOG_Full;
    if (flags == fLOG_Default) {
#if !defined(_DEBUG)  ||  defined(NDEBUG)
        flags = fLOG_Short;
#else
        flags = fLOG_Full;
#endif /*!_DEBUG || NDEBUG*/
    }

    /* Pre-calculate total message length */
    if ((flags & fLOG_DateTime) != 0) {
#ifdef NCBI_OS_MSWIN /*Should be compiler-dependent but C-Tkit lacks it*/
        _strdate(&datetime[datetime_len]);
        datetime_len += strlen(&datetime[datetime_len]);
        datetime[datetime_len++] = ' ';
        _strtime(&datetime[datetime_len]);
        datetime_len += strlen(&datetime[datetime_len]);
        datetime[datetime_len++] = ' ';
        datetime[datetime_len]   = '\0';
#else /*NCBI_OS_MSWIN*/
        static const char timefmt[] = "%m/%d/%y %H:%M:%S ";
        struct tm* tm;
#  ifdef NCBI_CXX_TOOLKIT
        time_t t = time(0);
#    ifdef HAVE_LOCALTIME_R
        struct tm temp;
        localtime_r(&t, &temp);
        tm = &temp;
#    else /*HAVE_LOCALTIME_R*/
        tm = localtime(&t);
#    endif/*HAVE_LOCALTIME_R*/
#  else /*NCBI_CXX_TOOLKIT*/
        struct tm temp;
        Nlm_GetDayTime(&temp);
        tm = &temp;
#  endif /*NCBI_CXX_TOOLKIT*/
        datetime_len = strftime(datetime, sizeof(datetime), timefmt, tm);
#endif /*NCBI_OS_MSWIN*/
    }
    if ((flags & fLOG_Level) != 0
        &&  (mess->level != eLOG_Note  ||  !(flags & fLOG_OmitNoteLevel))) {
        level = LOG_LevelStr(mess->level);
        level_len = strlen(level) + 2;
    }
    if ((flags & fLOG_Module) != 0
        &&  mess->module  &&  *mess->module) {
        module_len = strlen(mess->module) + 3;
    }
    if ((flags & fLOG_Function) != 0
        &&  mess->func  &&  *mess->func) {
        function = mess->func;
        if (!module_len)
            function_len = 3;
        function_len += strlen(function) + 2;
        if (strncmp(function, "::", 2) == 0  &&  !*(function += 2))
            function_len = 0;
    } else
        function = 0;
    if ((flags & fLOG_FileLine) != 0
        &&  mess->file  &&  *mess->file) {
        file_line_len = 12 + strlen(mess->file) + 11;
    }
    if (mess->message  &&  *mess->message)
        message_len = strlen(mess->message);

    if (mess->raw_size) {
        data_len = (sizeof(kRawData_Beg)
                    + 20 + UTIL_PrintableStringSize((const char*)
                                                    mess->raw_data,
                                                    mess->raw_size) +
                    sizeof(kRawData_End));
    }

    /* Allocate memory for the resulting message */
    total_len = (datetime_len + file_line_len + module_len + function_len
                 + level_len + message_len + data_len);
    if (!(str = (char*) malloc(total_len + 1))) {
        assert(0);
        return 0;
    }

    s = str;
    /* Compose the message */
    if (datetime_len) {
        memcpy(s, datetime, datetime_len);
        s += datetime_len;
    }
    if (file_line_len) {
        s += sprintf(s, "\"%s\", line %d: ",
                     mess->file, (int) mess->line);
    }
    if (module_len | function_len)
        *s++ = '[';
    if (module_len) {
        memcpy(s, mess->module, module_len -= 3);
        s += module_len;
    }
    if (function_len) {
        memcpy(s, "::", 2);
        s += 2;
        memcpy(s, function, function_len -= (module_len ? 2 : 5));
        s += function_len;
    }
    if (module_len | function_len) {
        *s++ = ']';
        *s++ = ' ';
    }
    if (level_len) {
        memcpy(s, level, level_len -= 2);
        s += level_len;
        *s++ = ':';
        *s++ = ' ';
    }
    if (message_len) {
        memcpy(s, mess->message, message_len);
        s += message_len;
    }
    if (data_len) {
        s += sprintf(s, kRawData_Beg,
                     (unsigned long) mess->raw_size,
                     &"s"[mess->raw_size == 1]);

        s = UTIL_PrintableString((const char*)
                                 mess->raw_data,
                                 mess->raw_size,
                                 s, flags & fLOG_FullOctal
                                 ? eUTIL_PrintableFullOctal : 0);

        memcpy(s, kRawData_End, sizeof(kRawData_End));
    } else
        *s = '\0';

    assert(strlen(str) <= total_len);
    return str;
}


typedef struct {
    FILE*       fp;
    ELOG_Level  cut_off;
    ELOG_Level  fatal_err;
    int/*bool*/ auto_close;
} SLogData;


/* Callback for LOG_ToFILE[_Ex]() */
#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/
static void s_LOG_FileHandler(void* data, const SLOG_Message* mess)
{
    SLogData* logdata = (SLogData*) data;
    assert(logdata->fp);

    if (mess->level >= logdata->cut_off  ||
        mess->level >= logdata->fatal_err) {
        char* str = LOG_ComposeMessage(mess, s_LogFormatFlags);
        if (str) {
            size_t len = strlen(str);
            str[len++] = '\n';
            fwrite(str, len, 1, logdata->fp);
            fflush(logdata->fp);
            free(str);
        }
        if (mess->level >= logdata->fatal_err) {
#ifdef NDEBUG
            fflush(0);
            _exit(255);
#else
            abort();
#endif /*NDEBUG*/
        }
    }
}
#ifdef __cplusplus
}
#endif /*__cplusplus*/


/* Callback for LOG_ToFILE[_Ex]() */
#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/
static void s_LOG_FileCleanup(void* data)
{
    SLogData* logdata = (SLogData*) data;

    assert(logdata->fp);
    if (logdata->auto_close)
        fclose(logdata->fp);
    else
        fflush(logdata->fp);
    free(logdata);
}
#ifdef __cplusplus
}
#endif /*__cplusplus*/


extern void LOG_ToFILE_Ex
(LOG         lg,
 FILE*       fp,
 ELOG_Level  cut_off,
 ELOG_Level  fatal_err,
 int/*bool*/ auto_close
 )
{
    SLogData* logdata;
    if (fp) {
        fflush(fp);
        logdata = (SLogData*) malloc(sizeof(*logdata));
    } else
        logdata = 0;
    if (logdata) {
        logdata->fp         = fp;
        logdata->cut_off    = cut_off;
        logdata->fatal_err  = fatal_err;
        logdata->auto_close = auto_close;
        LOG_Reset(lg, logdata, s_LOG_FileHandler, s_LOG_FileCleanup);
    } else {
        LOG_Reset(lg, 0/*data*/, 0/*handler*/, 0/*cleanup*/);
        if (fp  &&  auto_close)
            fclose(fp);
    }
}


extern void LOG_ToFILE
(LOG         lg,
 FILE*       fp,
 int/*bool*/ auto_close
 )
{
    LOG_ToFILE_Ex(lg, fp, eLOG_Trace, eLOG_Fatal, auto_close);
}



/******************************************************************************
 *  REGISTRY
 */

extern void CORE_SetREG(REG rg)
{
    REG old_rg;
    CORE_LOCK_WRITE;
    old_rg = g_CORE_Registry;
    g_CORE_Registry = rg;
    g_CORE_Set |= eCORE_SetREG;
    CORE_UNLOCK;
    if (old_rg  &&  old_rg != rg)
        REG_Delete(old_rg);
}


extern REG CORE_GetREG(void)
{
    return g_CORE_Registry;
}



/******************************************************************************
 *  CORE_GetAppName
 */

extern const char* CORE_GetAppName(void)
{
    const char* an;
    return !g_CORE_GetAppName  ||  !(an = g_CORE_GetAppName()) ? "" : an;
}



/******************************************************************************
 *  CORE_GetNcbiRequestID
 */

extern char* CORE_GetNcbiRequestID(ENcbiRequestID reqid)
{
    char* id;

    CORE_LOCK_READ;
    if (g_CORE_GetRequestID) {
        id = g_CORE_GetRequestID(reqid);
        assert(!id  ||  *id);
        if (id)
            goto out;
    }
    switch (reqid) {
    case eNcbiRequestID_SID:
        id = getenv("HTTP_NCBI_SID");
        if (id  &&  *id)
            break;
        id = getenv("NCBI_LOG_SESSION_ID");
        break;
    case eNcbiRequestID_HitID:
        id = getenv("HTTP_NCBI_PHID");
        if (id  &&  *id)
            break;
        id = getenv("NCBI_LOG_HIT_ID");
        break;
    default:
        id = 0;
        goto out;
    }
    id = id  &&  *id ? strdup(id) : 0;
 out:
    CORE_UNLOCK;

    return id;
}



/******************************************************************************
 *  CORE_GetPlatform
 */

extern const char* CORE_GetPlatform(void)
{
#ifndef NCBI_CXX_TOOLKIT
    return Nlm_PlatformName();
#else
    return HOST;
#endif /*NCBI_CXX_TOOLKIT*/
}



/****************************************************************************
 * CORE_GetUsername
 */

static char* x_Savestr(const char* str, char* buf, size_t bufsize)
{
    assert(str);
    if (buf) {
        size_t len = strlen(str);
        if (len++ < bufsize)
            return (char*) memcpy(buf, str, len);
        if (bufsize)
            *buf = '\0';
        errno = ERANGE;
    } else
        errno = EINVAL;
    return 0;
}


/*ARGSUSED*/
extern const char* CORE_GetUsernameEx(char* buf, size_t bufsize,
                                      ECORE_Username username)
{
#if defined(NCBI_OS_UNIX)
    struct passwd* pwd;
    struct stat    st;
    uid_t          uid;
#  ifndef NCBI_OS_SOLARIS
#    define NCBI_GETUSERNAME_MAXBUFSIZE  1024
#    ifdef HAVE_GETLOGIN_R
#      ifndef LOGIN_NAME_MAX
#        ifdef _POSIX_LOGIN_NAME_MAX
#          define LOGIN_NAME_MAX _POSIX_LOGIN_NAME_MAX
#        else
#          define LOGIN_NAME_MAX 256
#        endif /*_POSIX_LOGIN_NAME_MAX*/
#      endif /*!LOGIN_NAME_MAX*/
#      define     NCBI_GETUSERNAME_BUFSIZE   LOGIN_NAME_MAX
#    endif /*HAVE_GETLOGIN_R*/
#    ifdef NCBI_HAVE_GETPWUID_R
#      ifndef     NCBI_GETUSERNAME_BUFSIZE
#        define   NCBI_GETUSERNAME_BUFSIZE   NCBI_GETUSERNAME_MAXBUFSIZE
#      else
#        if       NCBI_GETUSERNAME_BUFSIZE < NCBI_GETUSERNAME_MAXBUFSIZE
#          undef  NCBI_GETUSERNAME_BUFSIZE
#          define NCBI_GETUSERNAME_BUFSIZE   NCBI_GETUSERNAME_MAXBUFSIZE
#        endif /* NCBI_GETUSERNAME_BUFSIZE < NCBI_GETUSERNAME_MAXBUFSIZE */
#      endif /*NCBI_GETUSERNAME_BUFSIZE*/
#    endif /*NCBI_HAVE_GETPWUID_R*/
#    ifdef        NCBI_GETUSERNAME_BUFSIZE
    char temp    [NCBI_GETUSERNAME_BUFSIZE + sizeof(*pwd)];
#    endif /*     NCBI_GETUSERNAME_BUFSIZE    */
#  endif /*!NCBI_OS_SOLARIS*/
#elif defined(NCBI_OS_MSWIN)
#  ifdef   UNLEN
#    define       NCBI_GETUSERNAME_BUFSIZE  UNLEN
#  else
#    define       NCBI_GETUSERNAME_BUFSIZE  256
#  endif /*UNLEN*/
    TCHAR temp   [NCBI_GETUSERNAME_BUFSIZE + 2];
    DWORD size = sizeof(temp)/sizeof(temp[0]) - 1;
#endif /*NCBI_OS*/
    const char* login;

    assert(buf  &&  bufsize);

#ifndef NCBI_OS_UNIX

#  ifdef NCBI_OS_MSWIN
    if (GetUserName(temp, &size)) {
        assert(size < sizeof(temp)/sizeof(temp[0]) - 1);
        temp[size] = (TCHAR) '\0';
        login = UTIL_TcharToUtf8(temp);
        buf = x_Savestr(login, buf, bufsize);
        UTIL_ReleaseBuffer(login);
        return buf;
    }
    CORE_LOCK_READ;
    if ((login = getenv("USERNAME")) != 0) {
        buf = x_Savestr(login, buf, bufsize);
        CORE_UNLOCK;
        return buf;
    }
    CORE_UNLOCK;
#  endif /*NCBI_OS_MSWIN*/

#else

    /* NOTE:  getlogin() is not a very reliable call at least on Linux
     * especially if programs mess up with "utmp":  since getlogin() first
     * calls ttyname() to get the line name for FD 0, then searches "utmp"
     * for the record of this line and returns the user name, any discrepancy
     * can cause a false (stale) name to be returned.  So we use getlogin()
     * here only as a fallback.
     */
    switch (username) {
    case eCORE_UsernameCurrent:
        uid = geteuid();
        break;
    case eCORE_UsernameLogin:
        if (isatty(STDIN_FILENO)  &&  fstat(STDIN_FILENO, &st) == 0) {
            uid = st.st_uid;
            break;
        }
#  if defined(NCBI_OS_SOLARIS)  ||  !defined(HAVE_GETLOGIN_R)
        /* NB:  getlogin() is MT-safe on Solaris, yet getlogin_r() comes in two
         * flavors that differ only in return type, so to make things simpler,
         * use plain getlogin() here */
#    ifndef NCBI_OS_SOLARIS
        CORE_LOCK_WRITE;
#    endif /*!NCBI_OS_SOLARIS*/
        if ((login = getlogin()) != 0)
            buf = x_Savestr(login, buf, bufsize);
#    ifndef NCBI_OS_SOLARIS
        CORE_UNLOCK;
#    endif /*!NCBI_OS_SOLARIS*/
        if (login)
            return buf;
#  else
        if (getlogin_r(temp, sizeof(temp) - 1) == 0) {
            temp[sizeof(temp) - 1] = '\0';
            return x_Savestr(temp, buf, bufsize);
        }
#  endif /*NCBI_OS_SOLARIS || !HAVE_GETLOGIN_R*/
        /*FALLTHRU*/
    case eCORE_UsernameReal:
        uid = getuid();
        break;
    default:
        assert(0);
        uid = (uid_t)(-1);
        break;
    }

#  if defined(NCBI_OS_SOLARIS)                                          \
    ||  (defined(HAVE_GETPWUID)  &&  !defined(NCBI_HAVE_GETPWUID_R))
    /* NB:  getpwuid() is MT-safe on Solaris, so use it here, if available */
#    ifndef NCBI_OS_SOLARIS
    CORE_LOCK_WRITE;
#    endif /*!NCBI_OS_SOLARIS*/
    if ((pwd = getpwuid(uid)) != 0) {
        if (pwd->pw_name)
            buf = x_Savestr(pwd->pw_name, buf, bufsize);
        else
            pwd = 0;
    }
#    ifndef NCBI_OS_SOLARIS
    CORE_UNLOCK;
#    endif /*!NCBI_OS_SOLARIS*/
    if (pwd)
        return buf;
#  elif defined(NCBI_HAVE_GETPWUID_R)
#    if   NCBI_HAVE_GETPWUID_R == 4
    /* obsolete but still existent */
    pwd = getpwuid_r(uid, (struct passwd*) temp, temp + sizeof(*pwd),
                     sizeof(temp) - sizeof(*pwd));
#    elif NCBI_HAVE_GETPWUID_R == 5
    /* POSIX-conforming */
    if (getpwuid_r(uid, (struct passwd*) temp, temp + sizeof(*pwd),
                   sizeof(temp) - sizeof(*pwd), &pwd) != 0) {
        pwd = 0;
    }
#    else
#      error "Unknown value of NCBI_HAVE_GETPWUID_R: 4 or 5 expected."
#    endif /*NCBI_HAVE_GETPWUID_R*/
    if (pwd  &&  pwd->pw_name)
        return x_Savestr(pwd->pw_name, buf, bufsize);
#  endif /*NCBI_HAVE_GETPWUID_R*/

#endif /*!NCBI_OS_UNIX*/

    /* last resort */
    CORE_LOCK_READ;
    if (!(login = getenv("USER"))  &&  !(login = getenv("LOGNAME")))
        login = "";
    buf = x_Savestr(login, buf, bufsize);
    CORE_UNLOCK;
    return buf;
}


extern const char* CORE_GetUsername(char* buf, size_t bufsize)
{
    const char* rv = CORE_GetUsernameEx(buf, bufsize, eCORE_UsernameLogin);
    return rv  &&  *rv ? rv : 0;
}



/****************************************************************************
 * CORE_GetVMPageSize:  Get page size granularity
 * See also at corelib's ncbi_system.cpp::GetVirtualMemoryPageSize().
 */

extern size_t CORE_GetVMPageSize(void)
{
    static size_t s_PS = 0;

    if (!s_PS) {
#if defined(NCBI_OS_MSWIN)  ||  defined(NCBI_OS_CYGWIN)
        /* NB: CYGWIN's PAGESIZE (== PAGE_SIZE) is actually the granularity */
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        s_PS = (size_t) si.dwPageSize;
#elif defined(NCBI_OS_UNIX) 
#  if   defined(_SC_PAGESIZE)
#    define NCBI_SC_PAGESIZE  _SC_PAGESIZE
#  elif defined(_SC_PAGE_SIZE)
#    define NCBI_SC_PAGESIZE  _SC_PAGE_SIZE
#  elif defined(NCBI_SC_PAGESIZE)
#    undef  NCBI_SC_PAGESIZE
#  endif
#  ifndef   NCBI_SC_PAGESIZE
        long x = 0;
#  else
        long x = sysconf(NCBI_SC_PAGESIZE);
#    undef  NCBI_SC_PAGESIZE
#  endif
        if (x <= 0) {
#  ifdef HAVE_GETPAGESIZE
            if ((x = getpagesize()) <= 0)
                return NCBI_DEFAULT_PAGE_SIZE;
#  else
            return NCBI_DEFAULT_PAGE_SIZE;
#  endif /*HAVE_GETPAGESIZE*/
        }
        s_PS = (size_t) x;
#endif /*OS_TYPE*/
    }
    return s_PS;
}



/****************************************************************************
 * CORE_Msdelay
 */

extern void CORE_Msdelay(unsigned long ms)
{
#if   defined(NCBI_OS_MSWIN)
    Sleep(ms);
#elif defined(NCBI_OS_UNIX)
#  if    defined(HAVE_NANOSLEEP)
    struct timespec ts;
    ts.tv_sec  = (time_t)(ms / 1000);
    ts.tv_nsec = (long) ((ms % 1000) * 1000000);
    nanosleep(&ts, 0);
#  elif !defined(NCBI_OS_DARWIN)  &&  defined(HAVE_POLL_H)
    poll(0, 0, (int) ms);
#  else
    struct timeval tv;
    tv.tv_sec  = (long) (ms / 1000);
    tv.tv_usec = (long)((ms % 1000) * 1000);
    select(0, 0, 0, 0, &tv);
#  endif /*HAVE_NANOSLEEP*/
#else
#  error "Unsupported platform."
#endif /*NCBI_OS*/
}



/****************************************************************************
 * CRC32
 */

/* Standard Ethernet/ZIP polynomial */
#define CRC32_POLY  0x04C11DB7U


#ifdef NCBI_USE_PRECOMPILED_CRC32_TABLES

static const unsigned int s_CRC32Table[256] = {
    0x00000000, 0x04C11DB7, 0x09823B6E, 0x0D4326D9,
    0x130476DC, 0x17C56B6B, 0x1A864DB2, 0x1E475005,
    0x2608EDB8, 0x22C9F00F, 0x2F8AD6D6, 0x2B4BCB61,
    0x350C9B64, 0x31CD86D3, 0x3C8EA00A, 0x384FBDBD,
    0x4C11DB70, 0x48D0C6C7, 0x4593E01E, 0x4152FDA9,
    0x5F15ADAC, 0x5BD4B01B, 0x569796C2, 0x52568B75,
    0x6A1936C8, 0x6ED82B7F, 0x639B0DA6, 0x675A1011,
    0x791D4014, 0x7DDC5DA3, 0x709F7B7A, 0x745E66CD,
    0x9823B6E0, 0x9CE2AB57, 0x91A18D8E, 0x95609039,
    0x8B27C03C, 0x8FE6DD8B, 0x82A5FB52, 0x8664E6E5,
    0xBE2B5B58, 0xBAEA46EF, 0xB7A96036, 0xB3687D81,
    0xAD2F2D84, 0xA9EE3033, 0xA4AD16EA, 0xA06C0B5D,
    0xD4326D90, 0xD0F37027, 0xDDB056FE, 0xD9714B49,
    0xC7361B4C, 0xC3F706FB, 0xCEB42022, 0xCA753D95,
    0xF23A8028, 0xF6FB9D9F, 0xFBB8BB46, 0xFF79A6F1,
    0xE13EF6F4, 0xE5FFEB43, 0xE8BCCD9A, 0xEC7DD02D,
    0x34867077, 0x30476DC0, 0x3D044B19, 0x39C556AE,
    0x278206AB, 0x23431B1C, 0x2E003DC5, 0x2AC12072,
    0x128E9DCF, 0x164F8078, 0x1B0CA6A1, 0x1FCDBB16,
    0x018AEB13, 0x054BF6A4, 0x0808D07D, 0x0CC9CDCA,
    0x7897AB07, 0x7C56B6B0, 0x71159069, 0x75D48DDE,
    0x6B93DDDB, 0x6F52C06C, 0x6211E6B5, 0x66D0FB02,
    0x5E9F46BF, 0x5A5E5B08, 0x571D7DD1, 0x53DC6066,
    0x4D9B3063, 0x495A2DD4, 0x44190B0D, 0x40D816BA,
    0xACA5C697, 0xA864DB20, 0xA527FDF9, 0xA1E6E04E,
    0xBFA1B04B, 0xBB60ADFC, 0xB6238B25, 0xB2E29692,
    0x8AAD2B2F, 0x8E6C3698, 0x832F1041, 0x87EE0DF6,
    0x99A95DF3, 0x9D684044, 0x902B669D, 0x94EA7B2A,
    0xE0B41DE7, 0xE4750050, 0xE9362689, 0xEDF73B3E,
    0xF3B06B3B, 0xF771768C, 0xFA325055, 0xFEF34DE2,
    0xC6BCF05F, 0xC27DEDE8, 0xCF3ECB31, 0xCBFFD686,
    0xD5B88683, 0xD1799B34, 0xDC3ABDED, 0xD8FBA05A,
    0x690CE0EE, 0x6DCDFD59, 0x608EDB80, 0x644FC637,
    0x7A089632, 0x7EC98B85, 0x738AAD5C, 0x774BB0EB,
    0x4F040D56, 0x4BC510E1, 0x46863638, 0x42472B8F,
    0x5C007B8A, 0x58C1663D, 0x558240E4, 0x51435D53,
    0x251D3B9E, 0x21DC2629, 0x2C9F00F0, 0x285E1D47,
    0x36194D42, 0x32D850F5, 0x3F9B762C, 0x3B5A6B9B,
    0x0315D626, 0x07D4CB91, 0x0A97ED48, 0x0E56F0FF,
    0x1011A0FA, 0x14D0BD4D, 0x19939B94, 0x1D528623,
    0xF12F560E, 0xF5EE4BB9, 0xF8AD6D60, 0xFC6C70D7,
    0xE22B20D2, 0xE6EA3D65, 0xEBA91BBC, 0xEF68060B,
    0xD727BBB6, 0xD3E6A601, 0xDEA580D8, 0xDA649D6F,
    0xC423CD6A, 0xC0E2D0DD, 0xCDA1F604, 0xC960EBB3,
    0xBD3E8D7E, 0xB9FF90C9, 0xB4BCB610, 0xB07DABA7,
    0xAE3AFBA2, 0xAAFBE615, 0xA7B8C0CC, 0xA379DD7B,
    0x9B3660C6, 0x9FF77D71, 0x92B45BA8, 0x9675461F,
    0x8832161A, 0x8CF30BAD, 0x81B02D74, 0x857130C3,
    0x5D8A9099, 0x594B8D2E, 0x5408ABF7, 0x50C9B640,
    0x4E8EE645, 0x4A4FFBF2, 0x470CDD2B, 0x43CDC09C,
    0x7B827D21, 0x7F436096, 0x7200464F, 0x76C15BF8,
    0x68860BFD, 0x6C47164A, 0x61043093, 0x65C52D24,
    0x119B4BE9, 0x155A565E, 0x18197087, 0x1CD86D30,
    0x029F3D35, 0x065E2082, 0x0B1D065B, 0x0FDC1BEC,
    0x3793A651, 0x3352BBE6, 0x3E119D3F, 0x3AD08088,
    0x2497D08D, 0x2056CD3A, 0x2D15EBE3, 0x29D4F654,
    0xC5A92679, 0xC1683BCE, 0xCC2B1D17, 0xC8EA00A0,
    0xD6AD50A5, 0xD26C4D12, 0xDF2F6BCB, 0xDBEE767C,
    0xE3A1CBC1, 0xE760D676, 0xEA23F0AF, 0xEEE2ED18,
    0xF0A5BD1D, 0xF464A0AA, 0xF9278673, 0xFDE69BC4,
    0x89B8FD09, 0x8D79E0BE, 0x803AC667, 0x84FBDBD0,
    0x9ABC8BD5, 0x9E7D9662, 0x933EB0BB, 0x97FFAD0C,
    0xAFB010B1, 0xAB710D06, 0xA6322BDF, 0xA2F33668,
    0xBCB4666D, 0xB8757BDA, 0xB5365D03, 0xB1F740B4
};

#else

static unsigned int s_CRC32Table[256];

static void s_CRC32_Init(void)
{
    size_t i;

    if (s_CRC32Table[255])
        return;

    for (i = 0;  i < 256;  ++i) {
        unsigned int byteCRC = (unsigned int) i << 24;
        int j;
        for (j = 0;  j < 8;  ++j) {
            if (byteCRC & 0x80000000U) {
                byteCRC <<= 1;
                byteCRC  ^= CRC32_POLY;
            } else
                byteCRC <<= 1;
        }
        s_CRC32Table[i] = byteCRC;
    }
}

#endif /*NCBI_USE_PRECOMPILED_CRC32_TABLES*/


extern unsigned int UTIL_CRC32_Update(unsigned int checksum,
                                      const void *ptr, size_t len)
{
    const unsigned char* data = (const unsigned char*) ptr;
    size_t i;

#ifndef NCBI_USE_PRECOMPILED_CRC32_TABLES
    s_CRC32_Init();
#endif /*NCBI_USE_PRECOMPILED_CRC32_TABLES*/

    for (i = 0;  i < len;  ++i) {
        size_t k = ((checksum >> 24) ^ *data++) & 0xFF;
        checksum <<= 8;
        checksum  ^= s_CRC32Table[k];
    }

    return checksum;
}


#define MOD_ADLER          65521
#define MAXLEN_ADLER       5548  /* max len to run without overflows */
#define ADJUST_ADLER(a)    a = (a & 0xFFFF) + (a >> 16) * (0x10000 - MOD_ADLER)
#define FINALIZE_ADLER(a)  if (a >= MOD_ADLER) a -= MOD_ADLER

unsigned int UTIL_Adler32_Update(unsigned int checksum,
                                 const void* ptr, size_t len)
{
    const unsigned char* data = (const unsigned char*) ptr;
    unsigned int a = checksum & 0xFFFF, b = checksum >> 16;

    while (len) {
        size_t i;
        if (len >= MAXLEN_ADLER) {
            len -= MAXLEN_ADLER;
            for (i = 0;  i < MAXLEN_ADLER/4;  ++i) {
                b += a += data[0];
                b += a += data[1];
                b += a += data[2];
                b += a += data[3];
                data += 4;
            }
        } else {
            for (i = len >> 2;  i;  --i) {
                b += a += data[0];
                b += a += data[1];
                b += a += data[2];
                b += a += data[3];
                data += 4;
            }
            for (len &= 3;  len;  --len) {
                b += a += *data++;
            }
        }
        ADJUST_ADLER(a);
        ADJUST_ADLER(b);
    }
    /* It can be shown that a <= 0x1013A here, so a single subtract will do. */
    FINALIZE_ADLER(a);
    /* It can be shown that b can reach 0xFFEF1 here. */
    ADJUST_ADLER(b);
    FINALIZE_ADLER(b);
    return (b << 16) | a;
}

#undef MOD_ADLER
#undef MAXLEN_ADLER
#undef ADJUST_ADLER
#undef FINALIZE_ADLER


extern void* UTIL_GenerateHMAC(const SHASH_Descriptor* hash,
                               const void*             text,
                               size_t                  text_len,
                               const void*             key,
                               size_t                  key_len,
                               void*                   digest)
{
    unsigned char* pad;
    void* ctx;
    size_t i;

    if (!hash  ||  !text  ||  !key  ||  !digest)
        return 0;

    if (!(pad = (unsigned char*) malloc(hash->block_len + hash->digest_len)))
        return 0;

    if (key_len > hash->block_len) {
        void* tmp;
        if (!hash->init(&ctx)) {
            free(pad);
            return 0;
        }
        tmp = pad + hash->block_len;
        hash->update(ctx, key, key_len);
        hash->fini(ctx, tmp);
        key     = tmp;
        key_len = hash->digest_len;
    }

    if (!hash->init(&ctx)) {
        free(pad);
        return 0;
    }

    for (i = 0;  i < key_len;  ++i)
        pad[i] = 0x36 ^ ((unsigned char*) key)[i];
    for (;  i < hash->block_len;  ++i)
        pad[i] = 0x36;

    hash->update(ctx, pad,  hash->block_len);
    hash->update(ctx, text, text_len);

    hash->fini(ctx, digest);

    if (!hash->init(&ctx)) {
        free(pad);
        return 0;
    }

    for (i = 0;  i < key_len;  ++i)
        pad[i] = 0x5C ^ ((unsigned char*) key)[i];
    for (;  i < hash->block_len;  ++i)
        pad[i] = 0x5C;

    hash->update(ctx, pad,    hash->block_len);
    hash->update(ctx, digest, hash->digest_len);

    hash->fini(ctx, digest);

    free(pad);
    return digest;
}



/******************************************************************************
 *  MISCELLANEOUS
 */


/*  1 = match;
 *  0 = no match;
 * -1 = no match, stop search
 */
static int/*tri-state*/ x_MatchesMask(const char* text, const char* mask,
                                      int/*bool*/ ignore_case)
{
    char a, b, c, p;
    for (;  (p = *mask++);  ++text) {
        c = *text;
        if (!c  &&  p != '*')
            return -1/*mismatch, stop*/;
        switch (p) {
        case '?':
            assert(c);
            continue;
        case '*':
            p = *mask;
            while (p == '*')
                p = *++mask;
            if (!p)
                return 1/*match*/;
            while (*text) {
                int matches = x_MatchesMask(text++, mask, ignore_case);
                if (matches/*!=0*/)
                    return matches;
            }
            return -1/*mismatch, stop*/;
        case '[':
            if (!(p = *mask))
                return -1/*mismatch, pattern error*/;
            if (p == '!') {
                p  = 1/*complement*/;
                ++mask;
            } else
                p  = 0;
            if (ignore_case)
                c = (char) tolower((unsigned char) c);
            assert(c);
            do {
                if (!(a = *mask++))
                    return -1/*mismatch, pattern error*/;
                if (*mask == '-'  &&  mask[1] != ']') {
                    ++mask;
                    if (!(b = *mask++))
                        return -1/*mismatch, pattern error*/;
                } else
                    b = a;
                if (c) {
                    if (ignore_case) {
                        a = (char) tolower((unsigned char) a);
                        b = (char) tolower((unsigned char) b);
                    }
                    if (a <= c  &&  c <= b)
                        c = 0/*mark as found*/;
                }
            } while (*mask != ']');
            if (p == !c)
                return 0/*mismatch*/;
            ++mask/*skip ']'*/;
            continue;
        case '\\':
            if (!(p = *mask++))
                return -1/*mismatch, pattern error*/;
            /*FALLTHRU*/
        default: 
            assert(c  &&  p);
            if (ignore_case) {
                c = (char) tolower((unsigned char) c);
                p = (char) tolower((unsigned char) p);
            }
            if (c != p)
                return 0/*mismatch*/;
            continue;
        }
    }
    return !*text;
}


extern int/*bool*/ UTIL_MatchesMaskEx(const char* text, const char* mask,
                                      int/*bool*/ ignore_case)
{
    return x_MatchesMask(text, mask, ignore_case) == 1 ? 1/*T*/ : 0/*F*/;
}


extern int/*bool*/ UTIL_MatchesMask(const char* text, const char* mask)
{
    return UTIL_MatchesMaskEx(text, mask, 1/*ignore case*/);
}


extern char* UTIL_NcbiLocalHostName(char* hostname)
{
    static const struct {
        const char*  end;
        const size_t len;
    } kEndings[] = {
        {".ncbi.nlm.nih.gov", 17},
        {".ncbi.nih.gov",     13}
    };
    size_t len = hostname ? strlen(hostname) : 0;
    if (len  &&  hostname[len - 1] == '.')
        len--;
    if (len) {
        size_t i;
        for (i = 0;  i < sizeof(kEndings) / sizeof(kEndings[0]);  ++i) {
            assert(strlen(kEndings[i].end) == kEndings[i].len);
            if (len > kEndings[i].len) {
                size_t beg = len - kEndings[i].len;
                if (hostname[beg - 1] != '.'
                    &&  strncasecmp(hostname + beg,
                                    kEndings[i].end,
                                    kEndings[i].len) == 0) {
                    hostname[beg] = '\0';
                    return hostname;
                }
            }
        }
    }
    return 0;
}


#ifdef NCBI_OS_MSWIN


#  ifdef _UNICODE

extern const char* UTIL_TcharToUtf8OnHeap(const TCHAR* str)
{
    const char* s = UTIL_TcharToUtf8(str);
    UTIL_ReleaseBufferOnHeap(str);
    return s;
}


/*
 * UTIL_TcharToUtf8() is defined in ncbi_strerror.c
 */


extern const TCHAR* UTIL_Utf8ToTchar(const char* str)
{
    TCHAR* s = NULL;
    if (str) {
        /* Note "-1" means to consume all input including the trailing NUL */
        int n = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
        if (n > 0) {
            s = (wchar_t*) LocalAlloc(LMEM_FIXED, n * sizeof(*s));
            if (s)
                MultiByteToWideChar(CP_UTF8, 0, str, -1, s,    n);
        }
    }
    return s;
}

#  endif /*_UNICODE*/


/*
 * UTIL_ReleaseBufferOnHeap() is defined in ncbi_strerror.c
 */


#endif /*NCBI_OS_MSWIN*/
