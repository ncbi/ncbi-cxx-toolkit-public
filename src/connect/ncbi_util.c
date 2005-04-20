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
 * Author:  Denis Vakatov, Anton Lavrentiev
 *
 * File Description:
 *   Auxiliary (optional) code for "ncbi_core.[ch]"
 *
 */

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


extern void CORE_SetLOGFILE
(FILE*       fp,
 int/*bool*/ auto_close)
{
    LOG lg = LOG_Create(0, 0, 0, 0);
    LOG_ToFILE(lg, fp, auto_close);
    CORE_SetLOG(lg);
}


extern int/*bool*/ CORE_SetLOGFILE_NAME(const char* filename)
{
    FILE* fp = fopen(filename, "a");
    if ( !fp ) {
        CORE_LOGF_ERRNO(eLOG_Error, errno, ("Cannot open \"%s\"", filename));
        return 0/*false*/;
    }

    CORE_SetLOGFILE(fp, 1/*true*/);
    return 1/*true*/;
}


static TLOG_FormatFlags s_LogFormatFlags = fLOG_Default;

extern TLOG_FormatFlags CORE_SetLOGFormatFlags(TLOG_FormatFlags flags)
{
    TLOG_FormatFlags old_flags = s_LogFormatFlags;

    s_LogFormatFlags = flags;
    return old_flags;
}


extern char* LOG_ComposeMessage
(const SLOG_Handler* call_data,
 TLOG_FormatFlags    format_flags)
{
    static const char s_RawData_Begin[] =
        "\n#################### [BEGIN] Raw Data (%lu byte%s):\n";
    static const char s_RawData_End[] =
        "\n#################### [END] Raw Data\n";

    char* str, datetime[32];

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
    if ((format_flags & fLOG_Level) != 0  &&
        (call_data->level != eLOG_Note ||
         !(format_flags & fLOG_OmitNoteLevel))) {
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

    if ( call_data->raw_size ) {
        const unsigned char* d = (const unsigned char*) call_data->raw_data;
        size_t      i = call_data->raw_size;
        for (data_len = 0;  i;  i--, d++) {
            if (*d == '\\'  ||  *d == '\r'  ||  *d == '\t') {
                data_len++;
            } else if (!isprint(*d)  &&  *d != '\n') {
                data_len += 2;
            }
        }
        data_len += sizeof(s_RawData_Begin) + 20 + call_data->raw_size +
            sizeof(s_RawData_End);
    }

    /* Allocate memory for the resulting message */
    total_len = datetime_len + file_line_len + module_len + level_len +
        message_len + data_len;
    str = (char*) malloc(total_len + 1);
    if ( !str ) {
        assert(0);
        return 0;
    }

    /* Compose the message */
    str[0] = '\0';
    if ( datetime_len ) {
        strcpy(str, datetime);
    }
    if ( file_line_len ) {
        sprintf(str + strlen(str), "\"%s\", line %d: ",
                call_data->file, (int) call_data->line);
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
    if ( data_len ) {
        size_t i;
        char* s;
        const unsigned char* d;

        s = str + strlen(str);
        sprintf(s, s_RawData_Begin, (unsigned long) call_data->raw_size,
                call_data->raw_size == 1 ? "" : "s");
        s += strlen(s);

        d = (const unsigned char*) call_data->raw_data;
        for (i = call_data->raw_size;  i;  i--, d++) {
            if (*d == '\\') {
                *s++ = '\\';
                *s++ = '\\';
            } else if (*d == '\r') {
                *s++ = '\\';
                *s++ = 'r';
            } else if (*d == '\t') {
                *s++ = '\\';
                *s++ = 't';
            } else if (!isprint(*d)  &&  *d != '\n') {
                static const char s_Hex[16] = {
                    '0', '1', '2', '3', '4', '5', '6', '7',
                    '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
                };
                *s++ = '\\';
                *s++ = s_Hex[*d / 16];
                *s++ = s_Hex[*d % 16];
            } else {
                *s++ = (char) *d;
            }
        }

        strcpy(s, s_RawData_End);
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
        char* str = LOG_ComposeMessage(call_data, s_LogFormatFlags);
        if ( str ) {
            fprintf(fp, "%s\n", str);
            fflush(fp);
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
            LOG_Reset(lg, fp, s_LOG_FileHandler, s_LOG_FileCleanup);
        } else {
            LOG_Reset(lg, fp, s_LOG_FileHandler, 0/*no cleaning up*/);
        }
    } else {
        LOG_Reset(lg, 0/*data*/, 0/*handler*/, 0/*cleanup*/);
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
            static const char s_Num[] = "0123456789";
            assert(x_errno / mod < 10);
            *beg++ = s_Num[x_errno / mod];
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
 *  MISCELLANEOUS
 */

extern const char* CORE_GetPlatform(void)
{
#ifndef NCBI_CXX_TOOLKIT
    return Nlm_PlatformName();
#else
    return HOST;
#endif /*NCBI_CXX_TOOLKIT*/
}


/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.32  2005/04/20 18:15:42  lavr
 * -"ncbi_config.h"
 *
 * Revision 6.31  2003/11/14 13:04:38  lavr
 * Little changes in comments [no code changes]
 *
 * Revision 6.30  2003/11/13 19:53:41  rsmith
 * Took out metrowerks specific #ifdef's (COMP_METRO). Not needed anymore.
 *
 * Revision 6.29  2003/09/02 21:05:14  lavr
 * Proper indentation of compilation conditionals
 *
 * Revision 6.28  2003/05/05 20:19:13  lavr
 * LOG_ComposeMessage() to check raw_size instead of raw_data ptr
 *
 * Revision 6.27  2003/05/05 11:41:09  rsmith
 * added defines and declarations to allow cross compilation Mac->Win32
 * using Metrowerks Codewarrior.
 *
 * Revision 6.26  2003/01/17 15:55:13  lavr
 * Fix errno reporting (comma was missing if errno == 0)
 *
 * Revision 6.25  2003/01/17 01:23:07  lavr
 * Always print full message for TRACE log in Debug mode
 *
 * Revision 6.24  2002/12/04 21:00:53  lavr
 * -CORE_LOG[F]_SYS_ERRNO()
 *
 * Revision 6.23  2002/12/04 19:51:12  lavr
 * No change
 *
 * Revision 6.22  2002/10/11 19:52:10  lavr
 * Log moved to end
 *
 * Revision 6.21  2002/06/18 17:07:44  lavr
 * Employ _strdate() & _strtime() if compiled by MSVC
 *
 * Revision 6.20  2002/05/07 18:22:10  lavr
 * Use fLOG_None in LOG_ComposeMessage()
 *
 * Revision 6.19  2002/02/11 20:36:44  lavr
 * Use "ncbi_config.h"
 *
 * Revision 6.18  2002/02/05 22:02:17  lavr
 * Minor tweak
 *
 * Revision 6.17  2002/01/28 20:22:39  lavr
 * Get rid of GCC warning about "'%D' yields only 2 last digits of year"
 *
 * Revision 6.16  2001/08/28 17:49:45  thiessen
 * oops, sorry - incorrect fix; reverted
 *
 * Revision 6.15  2001/08/28 17:21:22  thiessen
 * need ncbiconf.h for NCBI_CXX_TOOLKIT
 *
 * Revision 6.14  2001/08/09 16:25:06  lavr
 * Remove last (unneeded) parameter from LOG_Reset()
 * Added: fLOG_OmitNoteLevel format flag handling
 *
 * Revision 6.13  2001/07/30 14:41:37  lavr
 * Added: CORE_SetLOGFormatFlags()
 *
 * Revision 6.12  2001/07/26 15:13:02  lavr
 * Always do stream flush after message output (previously was in DEBUG only)
 *
 * Revision 6.11  2001/07/25 20:27:23  lavr
 * Included header files rearranged
 *
 * Revision 6.10  2001/07/25 19:12:57  lavr
 * Added date/time stamp for message logging
 *
 * Revision 6.9  2001/04/24 21:24:59  lavr
 * Make log flush in DEBUG mode
 *
 * Revision 6.8  2001/01/23 23:20:14  lavr
 * Comments added to some "boolean" 1s and 0s
 *
 * Revision 6.7  2001/01/12 23:50:38  lavr
 * "a+" -> "a" as a mode in fopen() for a logfile
 *
 * Revision 6.6  2000/08/28 20:05:51  vakatov
 * CORE_SetLOGFILE() -- typo fixed
 *
 * Revision 6.5  2000/06/23 19:34:45  vakatov
 * Added means to log binary data
 *
 * Revision 6.4  2000/05/30 23:23:26  vakatov
 * + CORE_SetLOGFILE_NAME()
 *
 * Revision 6.3  2000/03/31 17:19:11  kans
 * added continue statement to for loop to suppress missing body warning
 *
 * Revision 6.2  2000/03/24 23:12:09  vakatov
 * Starting the development quasi-branch to implement CONN API.
 * All development is performed in the NCBI C++ tree only, while
 * the NCBI C tree still contains "frozen" (see the last revision) code.
 *
 * Revision 6.1  2000/02/23 22:36:17  vakatov
 * Initial revision
 *
 * ===========================================================================
 */
