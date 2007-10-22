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
#  include <string.h>
#  include <time.h>
#endif
#if defined(NCBI_OS_UNIX)
#  ifndef NCBI_OS_SOLARIS
#    include <limits.h>
#  endif
#  if defined(HAVE_GETPWUID)  ||  defined(NCBI_HAVE_GETPWUID_R)
#    include <pwd.h>
#  endif
#  include <unistd.h>
#elif defined(NCBI_OS_MSWIN)
#  if defined(_MSC_VER)  &&  (_MSC_VER > 1200)
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

#define NCBI_USE_PRECOMPILED_CRC32_TABLES 1

#define NCBI_USE_ERRCODE_X   Connect_Util


/* Static function pre-declarations to avoid C++ compiler warnings
 */
#if defined(__cplusplus)
extern "C" {
    static void s_LOG_FileHandler(void* user_data, SLOG_Handler* call_data);
    static void s_LOG_FileCleanup(void* user_data);
}
#endif /* __cplusplus */


/******************************************************************************
 *  MT locking
 */

extern void CORE_SetLOCK(MT_LOCK lk)
{
    if (g_CORE_MT_Lock  &&  lk != g_CORE_MT_Lock) {
        MT_LOCK_Delete(g_CORE_MT_Lock);
    }
    g_CORE_MT_Lock = lk;
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
    CORE_LOCK_WRITE;
    if (g_CORE_Log  &&  lg != g_CORE_Log) {
        LOG_Delete(g_CORE_Log);
    }
    g_CORE_Log = lg;
    CORE_UNLOCK;
}


extern LOG CORE_GetLOG(void)
{
    return g_CORE_Log;
}


extern void CORE_SetLOGFILE_Ex
(FILE*       fp,
 ELOG_Level  cut_off,
 int/*bool*/ auto_close
 )
{
    LOG lg = LOG_Create(0, 0, 0, 0);
    LOG_ToFILE_Ex(lg, fp, cut_off, auto_close);
    CORE_SetLOG(lg);
}


extern void CORE_SetLOGFILE
(FILE*       fp,
 int/*bool*/ auto_close)
{
    CORE_SetLOGFILE_Ex(fp, 0, auto_close);
}


extern int/*bool*/ CORE_SetLOGFILE_NAME_Ex
(const char* filename,
 ELOG_Level  cut_off)
{
    FILE* fp = fopen(filename, "a");
    if ( !fp ) {
        CORE_LOGF_ERRNO_X(9, eLOG_Error, errno,
                          ("Cannot open \"%s\"", filename));
        return 0/*false*/;
    }

    CORE_SetLOGFILE_Ex(fp, cut_off, 1/*true*/);
    return 1/*true*/;
}


extern int/*bool*/ CORE_SetLOGFILE_NAME
(const char* filename
 )
{
    return CORE_SetLOGFILE_NAME_Ex(filename, 0);
}


static TLOG_FormatFlags s_LogFormatFlags = fLOG_Default;

extern TLOG_FormatFlags CORE_SetLOGFormatFlags(TLOG_FormatFlags flags)
{
    TLOG_FormatFlags old_flags = s_LogFormatFlags;

    s_LogFormatFlags = flags;
    return old_flags;
}


#ifdef __GNUC__
inline
#endif /*__GNUC__*/
static int/*bool*/ s_IsQuoted(unsigned char c)
{
    return (c == '\t'  ||   c == '\v'  ||  c == '\b'  ||
            c == '\r'  ||   c == '\f'  ||  c == '\a'  ||
            c == '\n'  ||   c == '\\'  ||  c == '\''  ||
            c == '"' ? 1/*true*/ : 0/*false*/);
}


extern char* LOG_ComposeMessage
(const SLOG_Handler* call_data,
 TLOG_FormatFlags    format_flags)
{
    static const char kRawData_Begin[] =
        "\n#################### [BEGIN] Raw Data (%lu byte%s):\n";
    static const char kRawData_End[] =
        "\n#################### [END] Raw Data\n";

    char *str, *s, datetime[32];
    const char* level = 0;

    /* Calculated length of ... */
    size_t datetime_len  = 0;
    size_t level_len     = 0;
    size_t file_line_len = 0;
    size_t module_len    = 0;
    size_t message_len   = 0;
    size_t data_len      = 0;
    size_t total_len;

    /* Adjust formatting flags */
    if (call_data->level == eLOG_Trace) {
#if defined(NDEBUG)  &&  !defined(_DEBUG)
        if (!(format_flags & fLOG_None))
#endif /*NDEBUG && !_DEBUG*/
            format_flags |= fLOG_Full;
    }
    if (format_flags == fLOG_Default) {
#if defined(NDEBUG)  &&  !defined(_DEBUG)
        format_flags = fLOG_Short;
#else
        format_flags = fLOG_Full;
#endif /*NDEBUG && !_DEBUG*/
    }

    /* Pre-calculate total message length */
    if ((format_flags & fLOG_DateTime) != 0) {
#ifdef NCBI_OS_MSWIN /*Should be compiler-dependent but C-Tkit lacks it*/
        _strdate(&datetime[datetime_len]);
        datetime_len += strlen(&datetime[datetime_len]);
        datetime[datetime_len++] = ' ';
        _strtime(&datetime[datetime_len]);
        datetime_len += strlen(&datetime[datetime_len]);
        datetime[datetime_len++] = ' ';
        datetime[datetime_len]   = '\0';
#else /*NCBI_OS_MSWIN*/
        static const char timefmt[] = "%D %T ";
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
#  endif/*NCBI_CXX_TOOLKIT*/
        datetime_len = strftime(datetime, sizeof(datetime), timefmt, tm);
#endif/*NCBI_OS_MSWIN*/
    }
    if ((format_flags & fLOG_Level) != 0
        &&  (call_data->level != eLOG_Note
             ||  !(format_flags & fLOG_OmitNoteLevel))) {
        level = LOG_LevelStr(call_data->level);
        level_len = strlen(level) + 2;
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

    if ( call_data->raw_size ) {
        const unsigned char* d = (const unsigned char*) call_data->raw_data;
        size_t i = call_data->raw_size;
        for (data_len = 0;  i;  i--, d++) {
            if (s_IsQuoted(*d)) {
                data_len++;
            } else if (!isprint(*d)) {
                data_len += 3;
            }
        }
        data_len += (sizeof(kRawData_Begin) + 20 + call_data->raw_size +
                     sizeof(kRawData_End));
    }

    /* Allocate memory for the resulting message */
    total_len = (datetime_len + file_line_len + module_len
                 + level_len + message_len + data_len);
    if (!(str = (char*) malloc(total_len + 1))) {
        assert(0);
        return 0;
    }

    s = str;
    /* Compose the message */
    if ( datetime_len ) {
        memcpy(s, datetime, datetime_len);
        s += datetime_len;
    }
    if ( file_line_len ) {
        s += sprintf(s, "\"%s\", line %d: ",
                     call_data->file, (int) call_data->line);
    }
    if ( module_len ) {
        *s++ = '[';
        memcpy(s, call_data->module, module_len -= 3);
        s += module_len;
        *s++ = ']';
        *s++ = ' ';
    }
    if ( level_len ) {
        memcpy(s, level, level_len -= 2);
        s += level_len;
        *s++ = ':';
        *s++ = ' ';
    }
    if ( message_len ) {
        memcpy(s, call_data->message, message_len);
        s += message_len;
    }
    if ( data_len ) {
        const unsigned char* d;
        size_t i;

        s += sprintf(s, kRawData_Begin, (unsigned long) call_data->raw_size,
                     &"s"[call_data->raw_size == 1]);

        d = (const unsigned char*) call_data->raw_data;
        for (i = call_data->raw_size;  i;  i--, d++) {
            switch (*d) {
            case '\t':
                *s++ = '\\';
                *s++ = 't';
                continue;
            case '\v':
                *s++ = '\\';
                *s++ = 'v';
                continue;
            case '\b':
                *s++ = '\\';
                *s++ = 'b';
                continue;
            case '\r':
                *s++ = '\\';
                *s++ = 'r';
                continue;
            case '\f':
                *s++ = '\\';
                *s++ = 'f';
                continue;
            case '\a':
                *s++ = '\\';
                *s++ = 'a';
                continue;
            case '\n':
            case '\\':
            case '\'':
            case '"':
                *s++ = '\\';
                break;
            default:
                if (!isprint(*d)) {
                    int/*bool*/ reduce;
                    unsigned char v;
                    reduce = (i == 1          ||  s_IsQuoted(d[1])  ||
                              !isprint(d[1])  ||  d[1] < '0'  ||  d[1] > '7');
                    *s++ = '\\';
                    v =  *d >> 6;
                    if (v  ||  !reduce) {
                        *s++ = '0' + v;
                        reduce = 0;
                    }
                    v = (*d >> 3) & 7;
                    if (v  ||  !reduce)
                        *s++ = '0' + v;
                    v =  *d & 7;
                    *s++ =     '0' + v;
                    continue;
                }
                break;
            }
            *s++ = (char) *d;
        }

        memcpy(s, kRawData_End, sizeof(kRawData_End));
    } else
        *s = '\0';

    assert(strlen(str) <= total_len);
    return str;
}


typedef struct {
    FILE*       fp;
    int/*bool*/ cut_off;
    int/*bool*/ auto_close;
} SLogData;


/* Callback for LOG_ToFILE[_Ex]() */
static void s_LOG_FileHandler(void* user_data, SLOG_Handler* call_data)
{
    SLogData* data = (SLogData*) user_data;
    assert(data  &&  data->fp);
    assert(call_data);

    if (call_data->level >= data->cut_off  ||  call_data->level == eLOG_Fatal){
        char* str = LOG_ComposeMessage(call_data, s_LogFormatFlags);
        if ( str ) {
            fprintf(data->fp, "%s\n", str);
            fflush(data->fp);
            free(str);
        }
    }
}


/* Callback for LOG_ToFILE[_Ex]() */
static void s_LOG_FileCleanup(void* user_data)
{
    SLogData* data = (SLogData*) user_data;

    assert(data  &&  data->fp);
    if (data->auto_close)
        fclose(data->fp);
    else
        fflush(data->fp);
    free(user_data);
}


extern void LOG_ToFILE_Ex
(LOG         lg,
 FILE*       fp,
 ELOG_Level  cut_off,
 int/*bool*/ auto_close
 )
{
    SLogData* data = (SLogData*)(fp ? malloc(sizeof(*data)) : 0);
    if ( data ) {
        data->fp         = fp;
        data->cut_off    = cut_off;
        data->auto_close = auto_close;
        LOG_Reset(lg, data, s_LOG_FileHandler, s_LOG_FileCleanup);
    } else {
        LOG_Reset(lg, 0/*data*/, 0/*handler*/, 0/*cleanup*/);
    }
}


extern void LOG_ToFILE
(LOG         lg,
 FILE*       fp,
 int/*bool*/ auto_close
 )
{
    LOG_ToFILE_Ex(lg, fp, 0, auto_close);
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

    /* Compose:   <message> {errno=<x_errno>,<descr>} */
    beg = buf;
    end = buf + buf_size - 1;

    /* <message> */
    if ( s_SafeCopy(message, &beg, end) )
        return buf;

    /* {errno=<x_errno>,<descr>} */
    if (!x_errno  &&  (!descr  ||  !*descr))
        return buf;

    /* "{errno=" */
    if ( s_SafeCopy(" {errno=", &beg, end) )
        return buf;

    /* <x_errno> */
    if ( x_errno ) {
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

        for (len = 1, mod = 1;  (x_errno / mod) > 9;  len++, mod *= 10)
            continue;
        if ( neg )
            len++;

        /* ? not enough space */
        if (beg + len >= end) {
            s_SafeCopy("...", &beg, end);
            return buf;
        }

        /* ? add sign */ 
        if (x_errno < 0) {
            *beg++ = '-';
        }

        /* print error code */
        for ( ;  mod;  mod /= 10) {
            assert(x_errno / mod < 10);
            *beg++ = '0' + x_errno / mod;
            x_errno %= mod;
        }
        /* "," before "<descr>" */
        if (descr  &&  *descr  &&  beg != end)
            *beg++ = ',';
    }

    /* "<descr>" */
    if ( s_SafeCopy(descr, &beg, end) )
        return buf;

    /* "}\0" */
    assert(beg <= end);
    if (beg != end)
        *beg++ = '}';
    *beg = '\0';

    return buf;
}



/******************************************************************************
 *  REGISTRY
 */

extern void CORE_SetREG(REG rg)
{
    CORE_LOCK_WRITE;
    if (g_CORE_Registry  &&  rg != g_CORE_Registry) {
        REG_Delete(g_CORE_Registry);
    }
    g_CORE_Registry = rg;
    CORE_UNLOCK;
}


extern REG CORE_GetREG(void)
{
    return g_CORE_Registry;
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

extern const char* CORE_GetUsername(char* buf, size_t bufsize)
{
#if defined(NCBI_OS_UNIX)
#  if !defined(NCBI_OS_SOLARIS)  &&  defined(HAVE_GETLOGIN_R)
#    ifndef LOGIN_NAME_MAX
#      ifdef _POSIX_LOGIN_NAME_MAX
#        define LOGIN_NAME_MAX _POSIX_LOGIN_NAME_MAX
#      else
#        define LOGIN_NAME_MAX 256
#      endif
#    endif
    char loginbuf[LOGIN_NAME_MAX + 1];
#  endif
    struct passwd* pw;
#  if !defined(NCBI_OS_SOLARIS)  &&  defined(NCBI_HAVE_GETPWUID_R)
    struct passwd pwd;
    char pwdbuf[256];
#  endif
#elif defined(NCBI_OS_MSWIN)
    char  loginbuf[256 + 1];
    DWORD loginbufsize = sizeof(loginbuf) - 1;
#endif
    const char* login;

    assert(buf  &&  bufsize);

#ifndef NCBI_OS_UNIX

#  ifdef NCBI_OS_MSWIN
    if (GetUserName(loginbuf, &loginbufsize)) {
        assert(loginbufsize < sizeof(loginbuf));
        loginbuf[loginbufsize] = '\0';
        strncpy0(buf, loginbuf, bufsize - 1);
        return buf;
    }
    if ((login = getenv("USERNAME")) != 0) {
        strncpy0(buf, login, bufsize - 1);
        return buf;
    }
#  endif

#else /*!NCBI_OS_UNIX*/

#  if defined(NCBI_OS_SOLARIS)  ||  !defined(HAVE_GETLOGIN_R)
    /* NB:  getlogin() is MT-safe on Solaris, yet getlogin_r() comes in two
     * flavors that differ only in return type, so to make things simpler,
     * use plain getlogin() here */
#    ifndef NCBI_OS_SOLARIS
    CORE_LOCK_WRITE;
#    endif
    if ((login = getlogin()) != 0)
        strncpy0(buf, login, bufsize - 1);
#    ifndef NCBI_OS_SOLARIS
    CORE_UNLOCK;
#    endif
    if (login)
        return buf;
#  else
    if (getlogin_r(loginbuf, sizeof(loginbuf) - 1) == 0) {
        loginbuf[sizeof(loginbuf) - 1] = '\0';
        strncpy0(buf, loginbuf, bufsize - 1);
        return buf;
    }
#  endif

#  if defined(NCBI_OS_SOLARIS)  ||  \
    (!defined(NCBI_HAVE_GETPWUID_R)  &&  defined(HAVE_GETPWUID))
    /* NB:  getpwuid() is MT-safe on Solaris, so use it here, if available */
#  ifndef NCBI_OS_SOLARIS
    CORE_LOCK_WRITE;
#  endif
    if ((pw = getpwuid(getuid())) != 0  &&  pw->pw_name)
        strncpy0(buf, pw->pw_name, bufsize - 1);
#  ifndef NCBI_OS_SOLARIS
    CORE_UNLOCK;
#  endif
    if (pw  &&  pw->pw_name)
        return buf;
#  elif defined(NCBI_HAVE_GETPWUID_R)
#    if   NCBI_HAVE_GETPWUID_R == 4
    /* obsolete but still existent */
    pw = getpwuid_r(getuid(), &pwd, pwdbuf, sizeof(pwdbuf));
#    elif NCBI_HAVE_GETPWUID_R == 5
    /* POSIX-conforming */
    if (getpwuid_r(getuid(), &pwd, pwdbuf, sizeof(pwdbuf), &pw) != 0)
        pw = 0;
#    else
#      error "Unknown value of NCBI_HAVE_GETPWUID_R, 4 or 5 expected."
#    endif
    if (pw  &&  pw->pw_name) {
        assert(pw == &pwd);
        strncpy0(buf, pw->pw_name, bufsize - 1);
        return buf;
    }
#  endif /*NCBI_HAVE_GETPWUID_R*/

#endif /*!NCBI_OS_UNIX*/

    /* last resort */
    if (!(login = getenv("USER"))  &&  !(login = getenv("LOGNAME"))) {
        buf[0] = '\0';
        return 0;
    }
    strncpy0(buf, login, bufsize - 1);
    return buf;
}



/****************************************************************************
 * CORE_GetVMPageSize:  Get page size granularity
 * See also at corelib's ncbi_system.cpp::GetVirtualMemoryPageSize().
 */

size_t CORE_GetVMPageSize(void)
{
    static size_t ps = 0;

    if (!ps) {
#if defined(NCBI_OS_MSWIN)
        SYSTEM_INFO si;
        GetSystemInfo(&si); 
        ps = (size_t) si.dwAllocationGranularity;
#elif defined(NCBI_OS_UNIX) 
#  if   defined(_SC_PAGESIZE)
#    define NCBI_SC_PAGESIZE _SC_PAGESIZE
#  elif defined(_SC_PAGE_SIZE)
#    define NCBI_SC_PAGESIZE _SC_PAGE_SIZE
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
                return 0;
#  else
            return 0;
#  endif
        }
        ps = (size_t) x;
#endif /*OS_TYPE*/
    }
    return ps;
}



/****************************************************************************
 * CRC32
 */

/* Standard Ethernet/ZIP polynomial */
#define CRC32_POLY 0x04C11DB7UL


#ifdef NCBI_USE_PRECOMPILED_CRC32_TABLES

static const unsigned int s_CRC32[256] = {
    0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9,
    0x130476dc, 0x17c56b6b, 0x1a864db2, 0x1e475005,
    0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
    0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd,
    0x4c11db70, 0x48d0c6c7, 0x4593e01e, 0x4152fda9,
    0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
    0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011,
    0x791d4014, 0x7ddc5da3, 0x709f7b7a, 0x745e66cd,
    0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
    0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5,
    0xbe2b5b58, 0xbaea46ef, 0xb7a96036, 0xb3687d81,
    0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
    0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49,
    0xc7361b4c, 0xc3f706fb, 0xceb42022, 0xca753d95,
    0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
    0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d,
    0x34867077, 0x30476dc0, 0x3d044b19, 0x39c556ae,
    0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
    0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16,
    0x018aeb13, 0x054bf6a4, 0x0808d07d, 0x0cc9cdca,
    0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
    0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02,
    0x5e9f46bf, 0x5a5e5b08, 0x571d7dd1, 0x53dc6066,
    0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
    0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e,
    0xbfa1b04b, 0xbb60adfc, 0xb6238b25, 0xb2e29692,
    0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
    0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a,
    0xe0b41de7, 0xe4750050, 0xe9362689, 0xedf73b3e,
    0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
    0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686,
    0xd5b88683, 0xd1799b34, 0xdc3abded, 0xd8fba05a,
    0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
    0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb,
    0x4f040d56, 0x4bc510e1, 0x46863638, 0x42472b8f,
    0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
    0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47,
    0x36194d42, 0x32d850f5, 0x3f9b762c, 0x3b5a6b9b,
    0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
    0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623,
    0xf12f560e, 0xf5ee4bb9, 0xf8ad6d60, 0xfc6c70d7,
    0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
    0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f,
    0xc423cd6a, 0xc0e2d0dd, 0xcda1f604, 0xc960ebb3,
    0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
    0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b,
    0x9b3660c6, 0x9ff77d71, 0x92b45ba8, 0x9675461f,
    0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
    0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640,
    0x4e8ee645, 0x4a4ffbf2, 0x470cdd2b, 0x43cdc09c,
    0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
    0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24,
    0x119b4be9, 0x155a565e, 0x18197087, 0x1cd86d30,
    0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
    0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088,
    0x2497d08d, 0x2056cd3a, 0x2d15ebe3, 0x29d4f654,
    0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
    0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c,
    0xe3a1cbc1, 0xe760d676, 0xea23f0af, 0xeee2ed18,
    0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
    0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0,
    0x9abc8bd5, 0x9e7d9662, 0x933eb0bb, 0x97ffad0c,
    0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
    0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

#else

static unsigned int s_CRC32[256];

static void s_CRC32_Init(void)
{
    size_t i;

    if (s_CRC32[255])
        return;

    for (i = 0;  i < 256;  i++) {
        unsigned int byteCRC = (unsigned int) i << 24;
        int j;
        for (j = 0;  j < 8;  j++) {
            if (byteCRC & 0x80000000UL) {
                byteCRC <<= 1;
                byteCRC  ^= CRC32_POLY;
            } else
                byteCRC <<= 1;
        }
        s_CRC32[i] = byteCRC;
    }
}

#endif /*NCBI_USE_PRECOMPILED_CRC32_TABLES*/


extern unsigned int CRC32_Update(unsigned int checksum,
                                 const void *ptr, size_t count)
{
    size_t j;
    const char* str = (const char*) ptr;

#ifndef NCBI_USE_PRECOMPILED_CRC32_TABLES
    s_CRC32_Init();
#endif /*NCBI_USE_PRECOMPILED_CRC32_TABLES*/

    for (j = 0;  j < count;  j++) {
        size_t i = ((checksum >> 24) ^ *str++) & 0xFF;
        checksum <<= 8;
        checksum  ^= s_CRC32[i];
    }

    return checksum;
}



/******************************************************************************
 *  MISCELLANEOUS
 */

extern int/*bool*/ UTIL_MatchesMaskEx(const char* name, const char* mask,
                                      int/*bool*/ ignore_case)
{
    for (;;) {
        char c = *mask++;
        char d;
        if (!c) {
            break;
        } else if (c == '?') {
            if (!*name++)
                return 0/*false*/;
        } else if (c == '*') {
            c = *mask;
            while (c == '*')
                c = *++mask;
            if (!c)
                return 1/*true*/;
            while (*name) {
                if (UTIL_MatchesMaskEx(name, mask, ignore_case))
                    return 1/*true*/;
                name++;
            }
            return 0/*false*/;
        } else {
            d = *name++;
            if (ignore_case) {
                c = tolower((unsigned char) c);
                d = tolower((unsigned char) d);
            }
            if (c != d)
                return 0/*false*/;
        }
    }
    return !*name;
}


extern int/*bool*/ UTIL_MatchesMask(const char* name, const char* mask)
{
    return UTIL_MatchesMaskEx(name, mask, 1/*ignore case*/);
}
