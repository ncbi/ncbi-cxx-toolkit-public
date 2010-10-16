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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   FTP CONNECTOR
 *   See also:  RFCs 959 (STD 9), 1123 (4.1), 1635 (FYI 24), 2428,
 *   3659 ("Extensions to FTP"), 5797 (FTP Command Registry).
 *
 *   Minimum FTP implementation: RFC 1123 (4.1.2.13)
 *
 *   See <connect/ncbi_connector.h> for the detailed specification of
 *   the connector's methods and structures.
 *
 */

#include "ncbi_ansi_ext.h"
#include "ncbi_priv.h"
#include <connect/ncbi_ftp_connector.h>
#include <connect/ncbi_socket.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>

#if !defined(NCBI_OS_MSWIN)  &&  !defined(__CYGWIN__)
#  define _timezone timezone
#  define _daylight daylight
#endif /*!NCBI_OS_MSWIN && !__CYGWIN__*/

#define NCBI_USE_ERRCODE_X   Connect_FTP


/***********************************************************************
 *  INTERNAL -- Auxiliary types and static functions
 ***********************************************************************/

typedef enum {
    fFtpFeature_NOOP = 0x01,     /* all implementations MUST support       */
    fFtpFeature_SYST = 0x02,
    fFtpFeature_SITE = 0x04,
    fFtpFeature_FEAT = 0x08,
    fFtpFeature_MDTM = 0x10,
    fFtpFeature_REST = 0x20,
    fFtpFeature_SIZE = 0x40,
    fFtpFeature_EPRT = 0x80,
    fFtpFeature_EPSV = 0x1000,
    fFtpFeature_APSV = 0x2000    /* EPSV ALL -- a la "APSV" from RFC 1579  */
} EFTP_Feature;
typedef unsigned int TFTP_Features; /* bitwise OR of EFtpFeature's */


/* All internal data necessary to perform I/O
 */
typedef struct {
    const char*           host;
    const char*           user;
    const char*           pass;
    const char*           path;
    unsigned short        port;
    unsigned char/*bool*/ send;  /* true when in send mode (STOR/APPE)     */
    unsigned char/*bool*/ sync;  /* true when last cmd acked (cntl synced) */
    TFTP_Features         feat;  /* FTP server features as discovered      */
    TFTP_Flags            flag;  /* connector flags per constructor        */
    SOCK                  cntl;  /* control connection                     */
    SOCK                  data;  /* data    connection                     */
    const char*           what;  /* goes to description                    */
    BUF                   rbuf;  /* read  buffer                           */
    BUF                   wbuf;  /* write buffer                           */
    SFTP_Callback         cmcb;  /* user-provided command callback         */
    size_t                size;  /* size of uploaded data                  */
    EIO_Status        r_status;
    EIO_Status        w_status;
} SFTPConnector;


static const STimeout kFailsafeTimeout = { 10, 0 };
static const STimeout kZeroTimeout = { 0, 0 };
static const char     kDigits[] = "0123456789";


typedef EIO_Status (*FFTPReplyParser)(SFTPConnector* xxx, int code,
                                      size_t lineno, const char* line);


static EIO_Status x_FTPParseReply(SFTPConnector* xxx, int* code,
                                  char* line, size_t maxlinelen,
                                  FFTPReplyParser parser)
{
    EIO_Status status;
    size_t     lineno;
    size_t     len;

    assert(xxx->cntl);
    for (lineno = 0; ; lineno++) {
        const char* msg;
        char buf[1024];
        int c, m;

        /* all FTP replies are at least '\n'-terminated, no ending with EOF */
        status = SOCK_ReadLine(xxx->cntl, buf, sizeof(buf), &len);
        if (status != eIO_Success)
            break;
        if (len == sizeof(buf)) {
            status = eIO_Unknown/*line too long*/;
            break;
        }
        msg = buf;
        if (!lineno  ||  isdigit((unsigned char)(*buf))) {
            if (sscanf(buf, "%d%n", &c, &m) < 1  ||  m != 3  ||  !c
                ||  (buf[m]  &&  buf[m] != ' '  &&  buf[m] != '-')
                ||  (lineno  &&  c != *code)) {
                status = eIO_Unknown;
                break;
            }
            msg += m + 1;
            if (buf[m] == '-')
                m = 0;
        } else {
            c = *code;
            m = 0;
        }
        msg += strspn(msg, " \t");
        if (status == eIO_Success  &&  parser)
            status = parser(xxx, lineno  &&  m ? 0 : c, lineno, msg);
        if (!lineno) {
            if (line)
                strncpy0(line, msg, maxlinelen);
            *code = c;
        }
        if (m)
            break;
    }
    if (SOCK_Wait(xxx->cntl, eIO_Read, &kZeroTimeout) == eIO_Success) {
        SOCK_Read(xxx->cntl, 0, 1<<20, &len, eIO_ReadPlain);
        if (status == eIO_Success  &&  len)
            status = eIO_Unknown;
    }
    return status;
}


typedef enum {
    eFTP_Close = 0,
    eFTP_Abort
} EFTPDataClose;

static EIO_Status s_FTPCloseData(SFTPConnector* xxx, EFTPDataClose how)
{
    static const STimeout kInstant = {0, 0};
    EIO_Status status;

    if (!xxx->data)
        return eIO_Success;
    if (xxx->flag & fFTP_LogControl)
        SOCK_SetDataLogging(xxx->data, eOn);
    if (how == eFTP_Abort) {
        status = SOCK_Abort(xxx->data);
        SOCK_Close(xxx->data);
    } else {
        SOCK_SetTimeout(xxx->data, eIO_Close, &kInstant);
        status = SOCK_Close(xxx->data);
    }
    xxx->data = 0;
    return status;
}


static EIO_Status s_FTPReply(SFTPConnector* xxx, int* code,
                             char* line, size_t maxlinelen,
                             FFTPReplyParser parser)
{
    int c = 0;
    EIO_Status status = eIO_Closed;
    if (xxx->cntl) {
        xxx->sync = 0/*false*/;
        status = x_FTPParseReply(xxx, &c, line, maxlinelen, parser);
        if (status != eIO_Timeout)
            xxx->sync = 1/*true*/;
        if (status == eIO_Success  &&  c == 421)
            status  = eIO_Closed;
        if (status == eIO_Closed  ||  (status == eIO_Success  &&  c == 221)) {
            s_FTPCloseData(xxx, status == eIO_Closed? eFTP_Abort : eFTP_Close);
            SOCK_Close(xxx->cntl);
            xxx->cntl = 0;
        }
    }
    if (code)
        *code = c;
    return status;
}


static EIO_Status s_FTPDrainReply(SFTPConnector* xxx, int* code, int cXX)
{
    int        c;
    EIO_Status status;
    int        quit = *code;
    while ((status = s_FTPReply(xxx, &c, 0, 0, 0)) == eIO_Success) {
        *code = c;
        if ((quit  &&  quit == c)  ||  (cXX  &&  c / 100 == cXX))
            break;
    }
    return status;
}


#define s_FTPCommand(x, c, a)  s_FTPCommandEx(x, c, a, 0/*false*/)

static EIO_Status s_FTPCommandEx(SFTPConnector* xxx,
                                 const char*    cmd,
                                 const char*    arg,
                                 int/*bool*/    off)
{
    size_t cmdlen  = strlen(cmd);
    size_t arglen  = arg ? strlen(arg) : 0;
    size_t linelen = cmdlen + (arglen ? 1/* */ + arglen : 0) + 2/*\r\n*/;
    char*  line    = (char*) malloc(linelen + 1/*\0*/);
    EIO_Status status;

    assert(xxx->cntl);
    if (line) {
        ESwitch log = eDefault;
        memcpy(line, cmd, cmdlen);
        if (arglen) {
            line[cmdlen++] = ' ';
            memcpy(line + cmdlen, arg, arglen);
            cmdlen += arglen;
        }
        line[cmdlen++] = '\r';
        line[cmdlen++] = '\n';
        line[cmdlen]   = '\0';
        log = off ? SOCK_SetDataLogging(xxx->cntl, eOff) : eOff;
        status = SOCK_Write(xxx->cntl, line, linelen, 0, eIO_WritePersist);
        if (off  &&  log != eOff) {
            SOCK_SetDataLogging(xxx->cntl, log);
            if (log == eOn  ||  SOCK_SetDataLoggingAPI(eDefault) == eOn)
                CORE_LOGF_X(4, eLOG_Trace,
                            ("FTP %.*s command sent (%s)",
                             (int) strcspn(line, " \t"), line,
                             IO_StatusStr(status)));
        }
        free(line);
    } else
        status = eIO_Unknown;
    return status;
}


static const char* x_4Word(const char* line, const char word[4+1])
{
    const char* s = strstr(line, word);
    return !s  ||  ((s == line  ||  isspace((unsigned char) s[-1]))
                    &&  !isalpha((unsigned char) s[4])) ? s : 0;
}


static EIO_Status x_FTPParseHelp(SFTPConnector* xxx, int code,
                                 size_t lineno, const char* line)
{
    if (!lineno)
        return code == 211  ||  code == 214 ? eIO_Success : eIO_NotSupported;
    if (code) {
        const char* s;
        if ((s = x_4Word(line, "NOOP")) != 0) {  /* RFC 959 */
            if (s[4 + strspn(s + 4, " \t")] != '*')
                xxx->feat |=  fFtpFeature_NOOP;
            else
                xxx->feat &= ~fFtpFeature_NOOP;
        }
        if ((s = x_4Word(line, "SYST")) != 0) {  /* RFC 959 */
            if (s[4 + strspn(s + 4, " \t")] != '*')
                xxx->feat |=  fFtpFeature_SYST;
            else
                xxx->feat &= ~fFtpFeature_SYST;
        }
        if ((s = x_4Word(line, "SITE")) != 0) {  /* RFC 959, 1123 */
            if (s[4 + strspn(s + 4, " \t")] != '*')
                xxx->feat |=  fFtpFeature_SITE;
            else
                xxx->feat &= ~fFtpFeature_SITE;
        }
        if ((s = x_4Word(line, "FEAT")) != 0) {  /* RFC 3659 */
            if (s[4 + strspn(s + 4, " \t")] != '*')
                xxx->feat |=  fFtpFeature_FEAT;
            else
                xxx->feat &= ~fFtpFeature_FEAT;
        }
        if ((s = x_4Word(line, "MDTM")) != 0) {  /* RFC 3659 */
            if (s[4 + strspn(s + 4, " \t")] != '*')
                xxx->feat |=  fFtpFeature_MDTM;
            else
                xxx->feat &= ~fFtpFeature_MDTM;
        }
        if ((s = x_4Word(line, "REST")) != 0) {  /* RFC 3659 */
            if (s[4 + strspn(s + 4, " \t")] != '*')
                xxx->feat |=  fFtpFeature_REST;
            else
                xxx->feat &= ~fFtpFeature_REST;
        }
        if ((s = x_4Word(line, "SIZE")) != 0) {  /* RFC 3659 */
            if (s[4 + strspn(s + 4, " \t")] != '*')
                xxx->feat |=  fFtpFeature_SIZE;
            else
                xxx->feat &= ~fFtpFeature_SIZE;
        }
        if ((s = x_4Word(line, "EPRT")) != 0) {  /* RFC 2428 */
            if (s[4 + strspn(s + 4, " \t")] != '*')
                xxx->feat |=  fFtpFeature_EPRT;
            else
                xxx->feat &= ~fFtpFeature_EPRT;
        }
        if ((s = x_4Word(line, "EPSV")) != 0) {  /* RFC 2428 (cf 1579) */
            if (s[4 + strspn(s + 4, " \t")] != '*')
                xxx->feat |=  fFtpFeature_EPSV;
            else
                xxx->feat &= ~fFtpFeature_EPSV;
        }
    }
    return eIO_Success;
}


static EIO_Status s_FTPHelp(SFTPConnector* xxx)
{
    int code;
    TFTP_Features feat;
    EIO_Status status = s_FTPCommand(xxx, "HELP", 0);
    if (status != eIO_Success)
        return status;
    feat = xxx->feat;
    status = s_FTPReply(xxx, &code, 0, 0, x_FTPParseHelp);
    if (status != eIO_Success  ||  (code != 211  &&  code != 214)) {
        xxx->feat = feat;
        return status != eIO_Success ? status : eIO_NotSupported;
    }
    return eIO_Success;
}


static EIO_Status x_FTPParseFeat(SFTPConnector* xxx, int code,
                                 size_t lineno, const char* line)
{
    if (!lineno)
        return code == 211 ? eIO_Success : eIO_NotSupported;
    if (code  &&  strlen(line) >= 4  &&  isspace((unsigned char) line[4])) {
        assert(code == 211);
        if      (strncasecmp(line, "MDTM", 4) == 0)
            xxx->feat |= fFtpFeature_MDTM;
        else if (strncasecmp(line, "REST", 4) == 0)
            xxx->feat |= fFtpFeature_REST;
        else if (strncasecmp(line, "SIZE", 4) == 0)
            xxx->feat |= fFtpFeature_SIZE;
        else if (strncasecmp(line, "EPSV", 4) == 0)
            xxx->feat |= fFtpFeature_EPSV;
    }
    return eIO_Success;
}


static EIO_Status s_FTPFeat(SFTPConnector* xxx)
{
    int code;
    EIO_Status status;
    TFTP_Features feat;
    if (xxx->feat  &&  !(xxx->feat & fFtpFeature_FEAT))
        return eIO_NotSupported;
    status = s_FTPCommand(xxx, "FEAT", 0);
    if (status != eIO_Success)
        return status;
    feat = xxx->feat;
    status = s_FTPReply(xxx, &code, 0, 0, x_FTPParseFeat);
    if (status != eIO_Success  ||  code != 211) {
        xxx->feat = feat;
        return status != eIO_Success ? status : eIO_NotSupported;
    }
    return eIO_Success;
}


/* all implementations MUST support NOOP */
static EIO_Status s_FTPNoop(SFTPConnector* xxx)
{
    int code;
    EIO_Status status = s_FTPCommand(xxx, "NOOP", 0);
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &code, 0, 0, 0);
    if (status != eIO_Success)
        return status;
    if (code != 200  &&  (code / 100 != 5  ||  (xxx->feat & fFtpFeature_NOOP)))
        return eIO_Unknown;
    return eIO_Success;
}


static EIO_Status s_FTPFeatures(SFTPConnector* xxx)
{
    if (xxx->flag & fFTP_UseFeatures) {
        /* try to setup features */
        if (s_FTPHelp(xxx) == eIO_Closed)
            return eIO_Closed;
        if (s_FTPFeat(xxx) == eIO_Closed)
            return eIO_Closed;
        /* make sure the connection is still good */
        return s_FTPNoop(xxx);
    }
    return eIO_Success;
}


static EIO_Status s_FTPLogin(SFTPConnector* xxx)
{
    int code;
    EIO_Status status;

    xxx->feat = 0;
    status = s_FTPReply(xxx, &code, 0, 0, 0);
    if (status != eIO_Success)
        return status;
    if (code != 220  ||  !xxx->user  ||  !*xxx->user)
        return eIO_Unknown;
    status = s_FTPCommand(xxx, "USER", xxx->user);
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &code, 0, 0, 0);
    if (status != eIO_Success)
        return status;
    if (code == 230)
        return eIO_Success;
    if (code != 331  ||  !xxx->pass)
        return eIO_Unknown;
    status = s_FTPCommandEx(xxx, "PASS", *xxx->pass ? xxx->pass : " ", 1);
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &code, 0, 0, 0);
    if (status != eIO_Success)
        return status;
    if (code != 230)
        return eIO_Unknown;
    status = s_FTPFeatures(xxx);
    if (status != eIO_Success)
        return status;
    if (xxx->flag & fFTP_LogControl) {
        CORE_LOGF_X(3, eLOG_Trace,
                    ("FTP server %s:%hu ready, features = 0x%02X",
                     xxx->host, xxx->port, (unsigned int) xxx->feat));
    }
    if (xxx->feat &  fFtpFeature_EPSV)
        xxx->feat |= fFtpFeature_APSV;
    return eIO_Success;
}


static EIO_Status s_FTPRename(SFTPConnector* xxx,
                              const char*    cmd)
{
    return eIO_NotSupported; /*yet*/
}


static EIO_Status s_FTPDir(SFTPConnector* xxx,
                           const char*    cmd)
{
    if (cmd  &&  (*cmd == 'P'/*PWD*/  ||
                  *cmd == 'M'/*MKD*/)) {
        return eIO_NotSupported; /*yet*/
    }
    if (cmd  ||  xxx->path) {
        int code;
        EIO_Status status = s_FTPCommand(xxx,
                                         cmd ? cmd : "CWD",
                                         cmd ? 0   : xxx->path);
        if (status != eIO_Success)
            return status;
        status = s_FTPReply(xxx, &code, 0, 0, 0);
        if (status != eIO_Success)
            return status;
        if (!cmd  ||  *cmd == 'C') { /* CWD, CDUP */
            if (code != 200  &&  code != 250)
                return eIO_Unknown;
        } else if (*cmd == 'R') { /* RMD */
            if (code != 250)
                return eIO_Unknown;
        } else {
            /* 257 is a success code: 257<SP>"directory"<SP>comment
             * For MKD there is 521 for "directory exits, no action taken" */
            return eIO_NotSupported; /*yet*/
        }
    }
    return eIO_Success;
}


/* all implementations MUST support TYPE I */
static EIO_Status s_FTPBinary(SFTPConnector* xxx)
{
    int code;
    /* all implementations MUST support TYPE */
    EIO_Status status = s_FTPCommand(xxx, "TYPE", "I");
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &code, 0, 0, 0);
    if (status != eIO_Success)
        return status;
    return code == 200 ? eIO_Success : eIO_Unknown;
}


#if 0 /*unused*/
static EIO_Status s_FTPSyst(SFTPConnector* xxx, char* buf, size_t bufsize)
{
    int code;
    EIO_Status status = s_FTPCommand(xxx, "SYST", 0);
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &code, buf, bufsize - 1, 0);
    if (status != eIO_Success)
        return status;
    return code == 215 ? eIO_Succes : eIO_Unknown;
}
#endif


static EIO_Status s_FTPRest(SFTPConnector* xxx,
                            const char*    cmd)
{
    int code;
    EIO_Status status = s_FTPCommand(xxx, cmd, 0);
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &code, 0, 0, 0);
    if (status != eIO_Success)
        return status;
    return code == 350 ? eIO_Success : eIO_Unknown;
}


static EIO_Status s_FTPDele(SFTPConnector* xxx,
                            const char*    cmd)
{
    int code;
    EIO_Status status = s_FTPCommand(xxx, cmd, 0);
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &code, 0, 0, 0);
    if (status != eIO_Success)
        return status;
    return code == 250 ? eIO_Success : eIO_Unknown;
}


static EIO_Status x_FTPParseMdtm(SFTPConnector* xxx, char* timestamp)
{
    static const int kDay[12] = {31,  0, 31, 30, 31, 30,
                                 31, 31, 30, 31, 30, 31};
    char* frac = strchr(timestamp, '.');
    int field[6], n;
    struct tm tm;
    char buf[40];
    size_t len;
    time_t t;

    if (frac) {
        *frac++ = '\0';
        if (strlen(frac) != strspn(frac, kDigits))
            return eIO_Unknown;
    }
    len = strlen(timestamp);
    if (len == 15  &&  strncmp(timestamp++, "191", 3) == 0) {
        /* Y2K problem with ftpd: 191xx as a "year" gets replaced with 20xx */
        timestamp[0] = '2';
        timestamp[1] = '0';
    } else if (len != 14)
        return eIO_Unknown;
    /* Can't use strptime() here, per the following reasons:
     * 1. Only GNU implementation is documented not to require spaces
     *    between input format specifiers in the format string (%-based);
     * 2. None to any spaces are allowed to match a space in the format,
     *    whilst the MDTM response must contain none.
     */
    for (n = 0;  n < 6;  n++) {
        size_t len = n ? 2 : 4/*year*/;
        if (len != strlen(strncpy0(buf, timestamp, len))  ||
            len != strspn(buf, kDigits)) {
            return eIO_Unknown;
        }
        field[n] = atoi(buf);
        timestamp += len;
    }
    memset(&tm, 0, sizeof(tm));
    if (field[0] < 1970)
        return eIO_Unknown;
    tm.tm_year  = field[0] - 1900;
    if (field[1] < 1  ||  field[1] > 12)
        return eIO_Unknown;
    tm.tm_mon   = field[1] - 1;
    if (field[2] < 1  ||  field[2] > (field[1] != 2
                                      ? kDay[tm.tm_mon]
                                      : 28 +
                                      (!(field[0] % 4)  &&
                                       (field[0] % 100
                                        ||  !(field[0] % 400))))) {
        return eIO_Unknown;
    }
    tm.tm_mday  = field[2];
    if (field[3] < 1  ||  field[3] > 23)
        return eIO_Unknown;
    tm.tm_hour  = field[3];
    if (field[4] < 1  ||  field[4] > 59)
        return eIO_Unknown;
    tm.tm_min   = field[4];
    if (field[5] < 1  ||  field[5] > 60) /* allow one leap second */
        return eIO_Unknown;
    tm.tm_sec   = field[5];
#ifdef HAVE_TIMEGM
    tm.tm_isdst = -1;
    if ((t = timegm(&tm)) == (time_t)(-1))
        return eIO_Unknown;
#else
    tm.tm_isdst =  0;
    if ((t = mktime(&tm)) == (time_t)(-1))
        return eIO_Unknown;
#  if !defined(NCBI_OS_DARWIN)  &&  !defined(NCBI_OS_BSD)
    /* NB: timezone information is unavailable on Darwin or BSD :-/ */
    if (t >= _timezone)
        t -= _timezone;
    if (t >= _daylight  &&  tm.tm_isdst > 0)
        t -= _daylight;
#  endif /*!NCBI_OS_DARWIN && !NCBI_OS_BSD*/
#endif /*HAVE_TIMEGM*/
    n = sprintf(buf, "%lu%s%-.9s", (unsigned long) t,
                frac  &&  *frac ? "." : "", frac ? frac : "");
    if (n <= 0  ||  !BUF_Write(&xxx->rbuf, buf, (size_t) n))
        return eIO_Unknown;
    return eIO_Success;
}


static EIO_Status s_FTPMdtm(SFTPConnector* xxx,
                            const char*    cmd)
{
    int code;
    char buf[128];
    EIO_Status status = s_FTPCommand(xxx, cmd, 0);
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &code, buf, sizeof(buf)-1, 0);
    if (status == eIO_Success) {
        switch (code) {
        case 213:
            status = x_FTPParseMdtm(xxx, buf);
            break;
        case 550:
            /* file not plain or does not exist */
            break;
        default:
            status =
                xxx->feat & fFtpFeature_MDTM ? eIO_Unknown : eIO_NotSupported;
            break;
        }
    }
    return status;
}


static EIO_Status x_FTPParseSize(SFTPConnector* xxx,
                                 const char*    size)
{
    size_t n = strspn(size, kDigits);
    if (!n  ||  n != strlen(size))
        return eIO_Unknown;
    if (xxx->cmcb.func  &&  (xxx->flag & fFTP_NotifySize))
        return xxx->cmcb.func(xxx->cmcb.data, xxx->what, size);
    return BUF_Write(&xxx->rbuf, size, n) ? eIO_Success : eIO_Unknown;
}


static EIO_Status s_FTPSize(SFTPConnector* xxx,
                            const char*    cmd)
{
    int code;
    char buf[128];
    EIO_Status status = s_FTPCommand(xxx, cmd, 0);
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &code, buf, sizeof(buf)-1, 0);
    if (status == eIO_Success) {
        switch (code) {
        case 213:
            status = x_FTPParseSize(xxx, buf);
            break;
        case 550:
            /* file not plain or does not exist */
            break;
        default:
            status =
                xxx->feat & fFtpFeature_SIZE ? eIO_Unknown : eIO_NotSupported;
            break;
        }
    }
    return status;
}


static EIO_Status s_FTPPasv(SFTPConnector*  xxx,
                            unsigned int*   host,
                            unsigned short* port)
{
    EIO_Status status;
    int  code, o[6];
    unsigned int i;
    char buf[128];

    /* all implementations MUST support */
    status = s_FTPCommand(xxx, "PASV", 0);
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &code, buf, sizeof(buf) - 1, 0);
    if (status != eIO_Success  ||  code != 227)
        return eIO_Unknown;
    buf[sizeof(buf) - 1] = '\0';
    for (;;) {
        char* c;
        /* RFC 1123 4.1.2.6 says that ()'s in PASV reply MUST NOT be assumed */
        for (c = buf;  *c;  ++c) {
            if (isdigit((unsigned char)(*c)))
                break;
        }
        if (!*c)
            return eIO_Unknown;
        if (sscanf(c, "%d,%d,%d,%d,%d,%d%n",
                   &o[0], &o[1], &o[2], &o[3], &o[4], &o[5], &code) >= 6) {
            break;
        }
        memmove(buf, c + code, strlen(c + code) + 1);
    }
    for (i = 0;  i < (unsigned int)(sizeof(o)/sizeof(o[0]));  i++) {
        if (o[i] < 0  ||  o[i] > 255)
            return eIO_Unknown;
    }
    if (!(i = (((((o[0] << 8) | o[1]) << 8) | o[2]) << 8) | o[3]))
        return eIO_Unknown;
    *host = SOCK_HostToNetLong(i);
    if (!(i = (o[4] << 8) | o[5]))
        return eIO_Unknown;
    *port = (unsigned short) i;
    return eIO_Success;
}


static EIO_Status s_FTPEpsv(SFTPConnector*  xxx,
                            unsigned int*   host,
                            unsigned short* port)
{
    EIO_Status status;
    char buf[128], d;
    unsigned int p;
    const char* s;
    int n;

    assert(xxx->feat & (fFtpFeature_EPSV | fFtpFeature_APSV));
    status = s_FTPCommand(xxx, "EPSV", port ? 0 : "ALL");
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &n, buf, sizeof(buf) - 1, 0);
    if (status != eIO_Success)
        return status;
    if (!port  &&  n == 200)
        return eIO_Success;
    if (!port  ||  n != 229)
        return xxx->feat & ~fFtpFeature_EPSV ? eIO_Unknown : eIO_NotSupported;
    buf[sizeof(buf) - 1] = '\0';
    if (!(s = strchr(buf, '('))  ||  !(d = *++s)  ||  *++s != d  ||  *++s != d
        ||  sscanf(++s, "%u%c%n", &p, buf, &n) < 2  ||  p > 0xFFFF
        ||  *buf != d  ||  s[n] != ')'  ||  s[++n]) {
        return eIO_Unknown;
    }
    *host = 0;
    *port = (unsigned short) p;
    return eIO_Success;
}


/*
 * how = 0 -- ABOR sequence only if data connection is open;
 * how = 1 -- just abort data connection, if any open;
 * how = 2 -- force full ABOR sequence.
 */
static EIO_Status s_FTPAbort(SFTPConnector*  xxx,
                             const STimeout* timeout,
                             int             how)
{
    EIO_Status status;
    int        code;
    size_t     n;

    if (!xxx->data  &&  !(how & 2))
        return eIO_Success;
    if (!xxx->cntl  ||   (how & 1))
        return s_FTPCloseData(xxx, eFTP_Abort);
    if (!timeout)
        timeout = &kFailsafeTimeout;
    SOCK_SetTimeout(xxx->cntl, eIO_ReadWrite, timeout);
    if (/* Send TELNET IP (Interrupt Process) command */
        (status = SOCK_Write(xxx->cntl, "\377\364", 2, &n, eIO_WritePersist))
        != eIO_Success  ||
        /* Send TELNET DM (Data Mark) command to complete SYNCH, RFC 854 */
        (status = SOCK_Write(xxx->cntl, "\377\362", 2, &n, eIO_WriteOutOfBand))
        != eIO_Success  ||  n != 2  ||
        (status = s_FTPCommand(xxx, "ABOR", 0)) != eIO_Success) {
        s_FTPCloseData(xxx, eFTP_Abort);
        return status != eIO_Success ? status : eIO_Unknown;
    }
    if (xxx->data) {
        SOCK_SetTimeout(xxx->data, eIO_ReadWrite, timeout);
        /* drain up the data connection by discarding 1MB blocks repeatedly */
        while (SOCK_Read(xxx->data, 0, 1<<20, 0, eIO_ReadPlain) == eIO_Success)
            continue;
        s_FTPCloseData(xxx, SOCK_Status(xxx->data, eIO_Read) == eIO_Closed
                       ? eFTP_Close : eFTP_Abort);
    }
    assert(!xxx->data);
    n = status == eIO_Timeout ? 1 : 0; /*just a bool for timeout*/
    code = 426;
    if ((status = s_FTPDrainReply(xxx, &code, 2/*2xx*/)) == eIO_Success
        /* Microsoft FTP is known to return 225 instead of 226 */
        &&  code != 225  &&  code != 226  &&  code != 426) {
        status = eIO_Unknown;
    }
    if (n  ||  status == eIO_Timeout) {
        CORE_LOG_X(1, eLOG_Warning,
                   "[FTP]  Timed out while aborting data connection");
    }
    return status;
}


static EIO_Status s_FTPPassive(SFTPConnector*  xxx,
                               const STimeout* timeout)
{
    unsigned int   host;
    unsigned short port;
    EIO_Status     status;
    char           buf[40];

    if (xxx->feat & (fFtpFeature_EPSV | fFtpFeature_APSV)) {
        if ((xxx->feat & fFtpFeature_EPSV) && (xxx->feat & fFtpFeature_APSV)) {
            /* first time here, try to set EPSV ALL */
            if (s_FTPEpsv(xxx, 0, 0) == eIO_Success)
                xxx->feat &= ~fFtpFeature_EPSV; /* APSV mode */
            else
                xxx->feat &= ~fFtpFeature_APSV; /* EPSV mode */
        }
        status = s_FTPEpsv(xxx, &host, &port);
        switch (status) {
        case eIO_NotSupported:
            /* wild guess did not work */
            xxx->feat &= ~fFtpFeature_EPSV;
            port = 0;
            break;
        case eIO_Success:
            assert(port);
            break;
        default:
            return status;
        }
    } else
        port = 0;
    if (!port  &&  (status = s_FTPPasv(xxx, &host, &port)) != eIO_Success)
        return status;
    assert(port);
    if (( host  &&
          SOCK_ntoa(host, buf, sizeof(buf)) != 0)  ||
        (!host  &&
         !SOCK_GetPeerAddressStringEx(xxx->cntl, buf, sizeof(buf), eSAF_IP))) {
        return eIO_Unknown;
    }
    status = SOCK_CreateEx(buf, port, timeout, &xxx->data, 0, 0,
                           xxx->flag & fFTP_LogControl
                           ? fSOCK_LogOn : fSOCK_LogDefault);
    if (status != eIO_Success) {
        assert(!xxx->data);
        CORE_LOGF_X(2, eLOG_Error,
                    ("[FTP; %s]  Cannot open data connection to %s:%hu (%s)",
                     xxx->what, buf, port, IO_StatusStr(status)));
        return status;
    }
    assert(xxx->data);
    if (!(xxx->flag & fFTP_LogData))
        SOCK_SetDataLogging(xxx->data, eDefault);
    return eIO_Success;
}


/* all implementations MUST support PORT */
static EIO_Status s_FTPPort(SFTPConnector* xxx,
                            unsigned int   host,
                            unsigned short port)
{
    unsigned char octet[sizeof(unsigned int) + sizeof(unsigned short)];
    EIO_Status    status;
    char          buf[80];
    int           code;

    port = SOCK_HostToNetShort(port);
    memcpy(octet,     &host, sizeof(host));
    memcpy(octet + 4, &port, sizeof(port));
    sprintf(buf, "%u,%u,%u,%u,%u,%u",
            octet[0], octet[1], octet[2], octet[3], octet[4], octet[5]);
    status = s_FTPCommand(xxx, "PORT", buf);
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &code, buf, sizeof(buf) - 1, 0);
    if (status != eIO_Success)
        return status;
    return code == 200 ? eIO_Success : eIO_Unknown;
}


static EIO_Status s_FTPEprt(SFTPConnector* xxx,
                            unsigned int   host,
                            unsigned short port)
{
    EIO_Status status;
    char       buf[80];
    int        code;

    memcpy(buf, "|1|", 3);
    SOCK_ntoa(host, buf + 3, sizeof(buf) - 3);
    sprintf(buf + 3 + strlen(buf + 3), "|%hu|", port);
    status = s_FTPCommand(xxx, "EPRT", buf);
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &code, buf, sizeof(buf) - 1, 0);
    if (status != eIO_Success)
        return status;
    if (code == 500  ||  code == 501)
        return xxx->feat & fFtpFeature_EPRT ? eIO_Unknown : eIO_NotSupported;
    if (code == 522) {
        xxx->feat &= ~fFtpFeature_EPRT;
        return eIO_NotSupported;
    }
    return code == 200 ? eIO_Success : eIO_Unknown;
}


static EIO_Status s_FTPActive(SFTPConnector*  xxx,
                              LSOCK*          lsock,
                              const STimeout* timeout)
{
    EIO_Status status;
    /* NB: Apache FTP proxy re-uses SOCK_LocalPort(xxx->cntl);
     * other implementations don't do that leaving OS to decide */
    status = LSOCK_CreateEx(0, 1, lsock, xxx->flag & fFTP_LogControl
                            ? fSOCK_LogOn : fSOCK_LogDefault);
    if (status == eIO_Success) {
        unsigned int   host;
        unsigned short port;
        if (!(host = SOCK_GetLocalHostAddress(eDefault))  ||
            !(port = LSOCK_GetPort(*lsock, eNH_HostByteOrder))) {
            return eIO_Unknown;
        }
        if (xxx->feat  &&  (xxx->feat & fFtpFeature_EPRT))
            status  = s_FTPEprt(xxx, host, port);
        else
            status  = eIO_NotSupported;
        if (status == eIO_NotSupported)
            status  = s_FTPPort(xxx, host, port);
    }
    return status;
}


static EIO_Status s_FTPOpenData(SFTPConnector*  xxx,
                                LSOCK*          lsock,
                                const STimeout* timeout)
{
    EIO_Status status;

    *lsock = 0;
    if ((xxx->flag & fFTP_UsePassive)  ||  !(xxx->flag & fFTP_UseActive)) {
        status = s_FTPPassive(xxx, timeout);
        if (status == eIO_Success  ||  (xxx->flag & fFTP_UsePassive))
            return status;
    }
    if ((xxx->feat & (fFtpFeature_EPSV | fFtpFeature_APSV))
        == fFtpFeature_APSV) {
        /* seems like the impossible case; still, better safe than sorry */
        return eIO_Closed;
    }
    status = s_FTPActive(xxx, lsock, timeout);
    if (status != eIO_Success) {
        if (*lsock) {
            LSOCK_Close(*lsock);
            *lsock = 0;
        }
    } else
        assert(*lsock);
    return status;
}


typedef enum {
    eFTP_In = 0,
    eFTP_Out
} EFTPDirection;

static EIO_Status s_FTPXfer(SFTPConnector*  xxx,
                            const char*     cmd,
                            const STimeout* timeout,
                            EFTPDirection   dir,
                            FFTPReplyParser parser)
{
    int code;
    LSOCK lsock;
    EIO_Status status = s_FTPOpenData(xxx, &lsock, timeout);
    if (status != eIO_Success) {
        assert(!lsock  &&  !xxx->data);
        return status;
    }
    status = s_FTPCommand(xxx, cmd, 0);
    if (status == eIO_Success) {
        status  = s_FTPReply(xxx, &code, 0, 0, parser);
        if (status == eIO_Success) {
            if (code == 125  ||  code == 150) {
                if (lsock) {
                    assert(!xxx->data);
                    status = LSOCK_AcceptEx(lsock, timeout, &xxx->data,
                                            xxx->flag & fFTP_LogControl
                                            ? fSOCK_LogOn : fSOCK_LogDefault);
                    if (status != eIO_Success) {
                        assert(!xxx->data);
                        CORE_LOGF_X(5, eLOG_Error,
                                    ("[FTP; %s]  Cannot accept data connection"
                                     " @ :%hu (%s)", xxx->what,
                                     LSOCK_GetPort(lsock, eNH_HostByteOrder),
                                     IO_StatusStr(status)));
                        /* though it may have been started at the server end */
                        code = 2/*force*/;
                    } else if (!(xxx->flag & fFTP_LogData))
                        SOCK_SetDataLogging(xxx->data, eDefault);
                    LSOCK_Close(lsock);
                    lsock = 0;
                }
                if (status == eIO_Success) {
                    assert(xxx->data);
                    if (dir == eFTP_Out) {
                        xxx->send = 1/*true*/;
                        xxx->size = 0;
                    }
                    return eIO_Success;
                }
            } else if (dir == eFTP_In
                       /* w/o data connection, user gets eIO_Closed on read */
                       &&  code == 450/*no files*/  &&  (*cmd != 'N'/*NLST*/ ||
                                                         *cmd != 'L'/*LIST*/)){
                code = 1/*quick*/;
            } else {
                status = eIO_Unknown;
                code = 0/*regular*/;
            }
        } else
            code = 0/*regular*/;
    } else
        code = 0/*regular*/;
    if (lsock)
        LSOCK_Close(lsock);
    s_FTPAbort(xxx, timeout, code);
    assert(!xxx->data);
    return status;
}


/* all implementations MUST support STOR */
static EIO_Status s_FTPStore(SFTPConnector*  xxx,
                             const char*     cmd,
                             const STimeout* timeout)
{
    return s_FTPXfer(xxx, cmd, timeout, eFTP_Out, 0);
}


static EIO_Status x_FTPSzcb(SFTPConnector* xxx, int code,
                            size_t lineno, const char* line)
{
    EIO_Status status = eIO_Success;
    assert(xxx->cmcb.func);
    if (!lineno  &&  (code == 125  ||  code == 150)) {
        const char* comment = strrchr(line,  '(');
        size_t n, m;
        if (comment  &&  strchr(++comment, ')')
            &&  (n = strspn(comment, kDigits)) > 0
            &&  (m = strspn(comment + n, " \t")) > 0
            &&  strncasecmp(comment + n + m, "byte", 4) == 0) {
            char* size = (char*) malloc(n + 1);
            if (size) {
                status = xxx->cmcb.func(xxx->cmcb.data, xxx->what,
                                        strncpy0(size, comment, n));
                free(size);
            } else
                status = eIO_Unknown;
        }
    }
    return status;
}


/* all implementations MUST support RETR */
static EIO_Status s_FTPRetrieve(SFTPConnector*  xxx,
                                const char*     cmd,
                                const STimeout* timeout)
{
    return s_FTPXfer(xxx, cmd, timeout, eFTP_In, xxx->cmcb.func? x_FTPSzcb :0);
}


static EIO_Status s_FTPExecute(SFTPConnector* xxx, const STimeout* timeout)
{
    EIO_Status status;
    size_t     size;
    char*      s;

    BUF_Erase(xxx->rbuf);
    if (xxx->what) {
        free((void*) xxx->what);
        xxx->what = 0;
    }
    status = s_FTPAbort(xxx, timeout, 0/*regular*/);
    assert(!xxx->data);
    if (status != eIO_Success)
        goto out;
    if (!xxx->sync) {
        int dummy;
        status = x_FTPParseReply(xxx, &dummy, 0, 0, 0);
        if (status != eIO_Success)
            goto out;
        assert(xxx->sync);
    }
    assert(xxx->cntl);
    verify(size = BUF_Size(xxx->wbuf));
    if ((s = (char*) malloc(size + 1)) != 0
        &&  BUF_Read(xxx->wbuf, s, size) == size) {
        const char* c;
        assert(!memchr(s, '\n', size));
        if (s[size - 1] == '\r')
            --size;
        s[size] = '\0';
        if (!(c = (const char*) memchr(s, ' ', size)))
            c = s + size;
        else
            size = (size_t)(c - s);
        xxx->what = s;
        if (size == 3  ||  size == 4) {
            /* FIXME: todo X<dir> commands (should be supported per RFC1123) */
            SOCK_SetTimeout(xxx->cntl, eIO_ReadWrite, timeout);
            if        (size == 3  &&   strncasecmp(s, "REN",  3) == 0) {
                /* special-cased non-standard command */
                status = s_FTPRename(xxx, c);
            } else if (size == 3  &&  (strncasecmp(s, "CWD",  3) == 0  ||
                                       strncasecmp(s, "PWD",  3) == 0  ||
                                       strncasecmp(s, "MKD",  3) == 0  ||
                                       strncasecmp(s, "RMD",  3) == 0)) {
                status = s_FTPDir (xxx, s);
            } else if (size == 4  &&   strncasecmp(s, "SIZE", 4) == 0) {
                status = s_FTPSize(xxx, s);
            } else if (size == 4  &&   strncasecmp(s, "MDTM", 4) == 0) {
                status = s_FTPMdtm(xxx, s);
            } else if (size == 4  &&   strncasecmp(s, "REST", 4) == 0) {
                status = s_FTPRest(xxx, s);
            } else if (size == 4  &&  (strncasecmp(s, "LIST", 4) == 0  ||
                                       strncasecmp(s, "NLST", 4) == 0  ||
                                       strncasecmp(s, "RETR", 4) == 0)) {
                status = s_FTPRetrieve(xxx, s, timeout);
            } else if (size == 4  &&  (strncasecmp(s, "STOR", 4) == 0  ||
                                       strncasecmp(s, "APPE", 4) == 0)) {
                status = s_FTPStore(xxx, s, timeout);
            } else if (size == 4  &&   strncasecmp(s, "DELE", 4) == 0) {
                status = s_FTPDele(xxx, s);
            } else if (size == 4  &&   strncasecmp(s, "CDUP", 4) == 0) {
                status = s_FTPDir (xxx, s);
            } else
                status = eIO_NotSupported;
        } else
            status = eIO_NotSupported;
        s = 0;
    } else
        status = eIO_Unknown;
    if (s)
        free(s);
 out:
    BUF_Erase(xxx->wbuf);
    return status;
}


/***********************************************************************
 *  INTERNAL -- "s_VT_*" functions for the "virt. table" of connector methods
 ***********************************************************************/

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
    static const char* s_VT_GetType (CONNECTOR       connector);
    static char*       s_VT_Descr   (CONNECTOR       connector);
    static EIO_Status  s_VT_Open    (CONNECTOR       connector,
                                     const STimeout* timeout);
    static EIO_Status  s_VT_Wait    (CONNECTOR       connector,
                                     EIO_Event       event,
                                     const STimeout* timeout);
    static EIO_Status  s_VT_Write   (CONNECTOR       connector,
                                     const void*     buf,
                                     size_t          size,
                                     size_t*         n_written,
                                     const STimeout* timeout);
    static EIO_Status  s_VT_Read    (CONNECTOR       connector,
                                     void*           buf,
                                     size_t          size,
                                     size_t*         n_read,
                                     const STimeout* timeout);
    static EIO_Status  s_VT_Flush   (CONNECTOR       connector,
                                     const STimeout* timeout);
    static EIO_Status  s_VT_Status  (CONNECTOR       connector,
                                     EIO_Event       dir);
    static EIO_Status  s_VT_Close   (CONNECTOR       connector,
                                     const STimeout* timeout);
    static void        s_Setup      (SMetaConnector* meta,
                                     CONNECTOR       connector);
    static void        s_Destroy    (CONNECTOR       connector);
#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */


/*ARGSUSED*/
static const char* s_VT_GetType
(CONNECTOR connector)
{
    return "FTP";
}


static char* s_VT_Descr
(CONNECTOR connector)
{
    SFTPConnector* xxx = (SFTPConnector*) connector->handle;
    return xxx->what ? strdup(xxx->what) : 0;
}


static EIO_Status s_VT_Open
(CONNECTOR       connector,
 const STimeout* timeout)
{
    SFTPConnector* xxx = (SFTPConnector*) connector->handle;
    EIO_Status status;

    assert(!xxx->data  &&  !xxx->cntl);

    status = SOCK_CreateEx(xxx->host, xxx->port, timeout, &xxx->cntl, 0, 0,
                           fSOCK_KeepAlive |
                           (xxx->flag & fFTP_LogControl
                            ? fSOCK_LogOn : fSOCK_LogDefault));
    if (status == eIO_Success) {
        SOCK_DisableOSSendDelay(xxx->cntl, 1/*yes,disable*/);
        SOCK_SetTimeout(xxx->cntl, eIO_ReadWrite, timeout);
        status  = s_FTPLogin(xxx);
    }
    if (status == eIO_Success)
        status  = s_FTPDir(xxx, 0);
    if (status == eIO_Success)
        status  = s_FTPBinary(xxx);
    if (status == eIO_Success)
        xxx->send = 0/*false*/;
    else if (xxx->cntl) {
        SOCK_Abort(xxx->cntl);
        SOCK_Close(xxx->cntl);
        xxx->cntl = 0;
    }
    xxx->r_status = status;
    xxx->w_status = status;
    return status;
}
 

static EIO_Status s_VT_Wait
(CONNECTOR       connector,
 EIO_Event       event,
 const STimeout* timeout)
{
    SFTPConnector* xxx = (SFTPConnector*) connector->handle;
    assert(event == eIO_Read  ||  event == eIO_Write);

    if (!xxx->cntl)
        return eIO_Closed;

    if (xxx->send  &&  xxx->data) {
        if (event & eIO_Read)
            return eIO_Success;
        return SOCK_Wait(xxx->data, eIO_Write, timeout);
    }
    if (event & eIO_Write)
        return xxx->send ? eIO_Closed : eIO_Success;
    if (!xxx->data) {
        EIO_Status status;
        if (BUF_Size(xxx->rbuf))
            return eIO_Success;
        if (!BUF_Size(xxx->wbuf))
            return eIO_Closed;
        if ((status = s_FTPExecute(xxx, timeout)) != eIO_Success)
            return status;
        if (BUF_Size(xxx->rbuf))
            return eIO_Success;
    }
    return xxx->data ? SOCK_Wait(xxx->data, eIO_Read, timeout) : eIO_Closed;
}


static EIO_Status s_VT_Write
(CONNECTOR       connector,
 const void*     buf,
 size_t          size,
 size_t*         n_written,
 const STimeout* timeout)
{
    SFTPConnector* xxx = (SFTPConnector*) connector->handle;
    EIO_Status status;
    const char* c;

    if (!xxx->cntl)
        return eIO_Closed;

    if (xxx->send) {
        if (!xxx->data)
            return eIO_Closed;
        SOCK_SetTimeout(xxx->data, eIO_Write, timeout);
        status = SOCK_Write(xxx->data, buf, size, n_written, eIO_WritePlain);
        xxx->size += *n_written;
        if (status == eIO_Closed)
            s_FTPCloseData(xxx, eFTP_Abort);
    } else if (!size) {
        return eIO_Success;
    } else if (((c = (const char*) memchr((const char*) buf, '\n', size)) != 0
                &&  c < (const char*) buf + size - 1)
               ||  !BUF_Write(&xxx->wbuf, buf, size - (c ? 1 : 0))) {
        return eIO_Unknown;
    } else {
        status = c ? s_FTPExecute(xxx, timeout) : eIO_Success;
        *n_written = size;
    }
    if (status != eIO_Timeout)
        xxx->w_status = status;
    return status;
}


static EIO_Status s_VT_Flush
(CONNECTOR       connector,
 const STimeout* timeout)
{
    SFTPConnector* xxx = (SFTPConnector*) connector->handle;
    if (!xxx->cntl)
        return eIO_Closed;

    if (xxx->send  ||  !BUF_Size(xxx->wbuf))
        return eIO_Success;
    return s_FTPExecute(xxx, timeout);
}


static EIO_Status s_VT_Read
(CONNECTOR       connector,
 void*           buf,
 size_t          size,
 size_t*         n_read,
 const STimeout* timeout)
{
    SFTPConnector* xxx = (SFTPConnector*) connector->handle;
    EIO_Status status;
    int code;

    if (!xxx->cntl)
        return eIO_Closed;

    if (xxx->send) {
        s_FTPCloseData(xxx, eFTP_Close);
        SOCK_SetTimeout(xxx->cntl, eIO_Read, timeout);
        status = s_FTPReply(xxx, &code, 0, 0, 0);
        if (status == eIO_Timeout)
            return status;
        xxx->send = 0/*false*/;
        if (status != eIO_Success  ||  code != 226)
            status  = eIO_Unknown;
        if (status == eIO_Success) {
            char buf[40];
            int n = sprintf(buf, "%lu", (unsigned long) xxx->size);
            if (!BUF_Write(&xxx->rbuf, buf, (size_t) n))
                status = eIO_Unknown;
        }
        xxx->w_status = status;
        assert(!xxx->data);
    } else
        status = eIO_Success;
    if (xxx->data) {
        assert(!xxx->send  &&  !BUF_Size(xxx->rbuf));
        SOCK_SetTimeout(xxx->data, eIO_Read, timeout);
        status = SOCK_Read(xxx->data, buf, size, n_read, eIO_ReadPlain);
        if (status == eIO_Closed) {
            s_FTPCloseData(xxx, eFTP_Close);
            SOCK_SetTimeout(xxx->cntl, eIO_Read, timeout);
            if (s_FTPReply(xxx, &code, 0, 0, 0) != eIO_Success
                ||  (code != 225/*Microsoft*/  &&  code != 226)) {
                status = eIO_Unknown;
            }
        }
        if (status != eIO_Timeout)
            xxx->r_status = status;
    } else if (size  &&  !(*n_read = BUF_Read(xxx->rbuf, buf, size)))
        status = eIO_Closed;
    return status;
}


static EIO_Status s_VT_Status
(CONNECTOR connector,
 EIO_Event dir)
{
    SFTPConnector* xxx = (SFTPConnector*) connector->handle;

    switch (dir) {
    case eIO_Read:
        return xxx->cntl ? xxx->r_status : eIO_Closed;
    case eIO_Write:
        return xxx->cntl ? xxx->w_status : eIO_Closed;
    default:
        assert(0); /* should never happen as checked by connection */
        break;
    }
    return eIO_InvalidArg;
}


static EIO_Status s_VT_Close
(CONNECTOR       connector,
 const STimeout* timeout)
{
    SFTPConnector* xxx = (SFTPConnector*) connector->handle;
    EIO_Status status;

    if (xxx->what) {
        free((void*) xxx->what);
        xxx->what = 0;
    }
    status = s_FTPAbort(xxx, timeout, 0/*regular*/);
    assert(!xxx->data);
    if (xxx->cntl  &&  status == eIO_Success) {
        int code = 0/*any*/;
        if (!timeout)
            timeout = &kFailsafeTimeout;
        SOCK_SetTimeout(xxx->cntl, eIO_ReadWrite, timeout);
        /* all implementations MUST support QUIT */
        status = s_FTPCommand(xxx, "QUIT", 0);
        if (status == eIO_Success)
            status  = s_FTPDrainReply(xxx, &code, 0/*any*/);
        if (status != eIO_Closed  ||  code != 221)
            status  = eIO_Unknown;
    }
    if (xxx->cntl) {
        assert(status != eIO_Success);
        if (status == eIO_Timeout)
            SOCK_Abort(xxx->cntl);
        status = SOCK_Close(xxx->cntl);
        xxx->cntl = 0;
    }
    return status;
}


static void s_Setup
(SMetaConnector* meta,
 CONNECTOR       connector)
{
    /* initialize virtual table */
    CONN_SET_METHOD(meta, get_type, s_VT_GetType, connector);
    CONN_SET_METHOD(meta, descr,    s_VT_Descr,   connector);
    CONN_SET_METHOD(meta, open,     s_VT_Open,    connector);
    CONN_SET_METHOD(meta, wait,     s_VT_Wait,    connector);
    CONN_SET_METHOD(meta, write,    s_VT_Write,   connector);
    CONN_SET_METHOD(meta, flush,    s_VT_Flush,   connector);
    CONN_SET_METHOD(meta, read,     s_VT_Read,    connector);
    CONN_SET_METHOD(meta, status,   s_VT_Status,  connector);
    CONN_SET_METHOD(meta, close,    s_VT_Close,   connector);
    meta->default_timeout = kInfiniteTimeout;
}


static void s_Destroy
(CONNECTOR connector)
{
    SFTPConnector* xxx = (SFTPConnector*) connector->handle;
    connector->handle = 0;

    if (xxx->host) {
        free((void*) xxx->host);
        xxx->host = 0;
    }
    if (xxx->user) {
        free((void*) xxx->user);
        xxx->user = 0;
    }
    if (xxx->pass) {
        free((void*) xxx->pass);
        xxx->pass = 0;
    }
    if (xxx->path) {
        free((void*) xxx->path);
        xxx->path = 0;
    }
    BUF_Destroy(xxx->rbuf);
    xxx->rbuf = 0;
    BUF_Destroy(xxx->wbuf);
    xxx->wbuf = 0;
    if (xxx->what) {
        free((void*) xxx->what);
        xxx->what = 0;
    }
    free(xxx);
    free(connector);
}


/***********************************************************************
 *  EXTERNAL -- the connector's "constructors"
 ***********************************************************************/

extern CONNECTOR FTP_CreateConnectorEx(const char*          host,
                                       unsigned short       port,
                                       const char*          user,
                                       const char*          pass,
                                       const char*          path,
                                       TFTP_Flags           flag,
                                       const SFTP_Callback* cmcb)
{
    static const SFTP_Callback kNoCmcb = { 0 };

    CONNECTOR      ccc = (SConnector*)    malloc(sizeof(SConnector));
    SFTPConnector* xxx = (SFTPConnector*) malloc(sizeof(*xxx));

    xxx->data     = 0;
    xxx->cntl     = 0;
    xxx->rbuf     = 0;
    xxx->wbuf     = 0;
    xxx->what     = 0;
    xxx->host     = strdup(host);
    xxx->port     = port            ? port         : 21;
    xxx->user     = strdup(user     ? user         : "ftp");
    xxx->pass     = strdup(pass     ? pass         : "-none");
    xxx->path     = path  &&  *path ? strdup(path) : 0;
    xxx->flag     = flag;
    xxx->cmcb     = cmcb            ? *cmcb        : kNoCmcb;

    /* initialize connector data */
    ccc->handle   = xxx;
    ccc->next     = 0;
    ccc->meta     = 0;
    ccc->setup    = s_Setup;
    ccc->destroy  = s_Destroy;

    return ccc;
}


extern CONNECTOR FTP_CreateConnector(const char*    host,
                                     unsigned short port,
                                     const char*    user,
                                     const char*    pass,
                                     const char*    path,
                                     TFTP_Flags     flag)
{
    return FTP_CreateConnectorEx(host, port, user, pass, path, flag, 0);
}


/*DEPRECATED*/
extern CONNECTOR FTP_CreateDownloadConnector(const char*    host,
                                             unsigned short port,
                                             const char*    user,
                                             const char*    pass,
                                             const char*    path,
                                             TFTP_Flags     flag)
{
    return FTP_CreateConnectorEx(host, port, user, pass, path, flag, 0);
}
