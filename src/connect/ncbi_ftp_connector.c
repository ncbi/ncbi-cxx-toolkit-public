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
 *   See also:  RFCs 959 (STD 9), 1123 (4.1), 1635 (FYI 24),
 *   2389 (FTP Features), 2428 (Extended PORT commands),
 *   3659 (Extensions to FTP), 5797 (FTP Command Registry).
 *
 *   Minimum FTP implementation: RFC 1123 (4.1.2.13)
 *
 *   See <connect/ncbi_connector.h> for the detailed specification of
 *   the connector's methods and structures.
 *
 *   Note:  We do not implement transfers of files whose names include
 *          CR or LF characters:  for those to work, all FTP commands will
 *          have be required to terminate with CRLF at the user level
 *          (currently, LF alone acts as the command terminator), and all
 *          solitary CRs to be recoded as 'CR\0' (per the RFC), yet all
 *          solitary LFs to be passed through.  Nonetheless, we escape all
 *          IACs for the sake of safety of the control connection.
 *
 */

#include "ncbi_ansi_ext.h"
#include "ncbi_priv.h"
#include <connect/ncbi_ftp_connector.h>
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

enum EFTP_Feature {                /* NB: values must occupy 12 bits at most */
    fFtpFeature_NOOP =  0x001,     /* all implementations MUST support       */
    fFtpFeature_SYST =  0x002,
    fFtpFeature_SITE =  0x004,
    fFtpFeature_FEAT =  0x008,
    fFtpFeature_MDTM =  0x010,
    fFtpFeature_SIZE =  0x020,
    fFtpFeature_REST =  0x040,
    fFtpFeature_MLSx =  0x080,
    fFtpFeature_EPRT =  0x100,
    fFtpFeature_EPSV = 0x1000,
    fFtpFeature_APSV = 0x3000      /* EPSV ALL -- a la "APSV" from RFC 1579  */
};
typedef unsigned short TFTP_Features; /* bitwise OR of EFtpFeature */


/* All internal data necessary to perform I/O
 */
typedef struct {
    SConnNetInfo*         info;    /* connection parameters                  */
    unsigned              sync:1;  /* true when last cmd acked (cntl synced) */
    unsigned              send:1;  /* true when in send mode (STOR/APPE)     */
    unsigned              open:1;  /* true when data open ok in send mode    */
    unsigned              rclr:1;  /* true when "rest" to clear by next cmd  */
    unsigned              soft:12; /* learned server features (future ext)   */
    TFTP_Features         feat;    /* FTP server features as discovered      */
    TFTP_Flags            flag;    /* connector flags per constructor        */
    SFTP_Callback         cmcb;    /* user-provided command callback         */
    const char*           what;    /* goes to description                    */
    SOCK                  cntl;    /* control connection                     */
    SOCK                  data;    /* data    connection                     */
    BUF                   wbuf;    /* write buffer (command)                 */
    BUF                   rbuf;    /* read  buffer (response)                */
    TNCBI_BigCount        size;    /* size of data                           */
    TNCBI_BigCount        rest;    /* restart position                       */
    EIO_Status        r_status;
    EIO_Status        w_status;
} SFTPConnector;


static const STimeout kFailsafeTimeout = { 10, 0 };
static const STimeout kZeroTimeout     = {  0, 0 };
static const char     kDigits[] = "0123456789";


typedef EIO_Status (*FFTPReplyParser)(SFTPConnector* xxx, int code,
                                      size_t lineno, const char* line);


static EIO_Status x_FTPParseReply(SFTPConnector* xxx, int* code,
                                  char* line, size_t maxlinelen,
                                  FFTPReplyParser parser)
{
    EIO_Status status = eIO_Success;
    size_t     lineno;
    size_t     len;

    assert(xxx->cntl);

    for (lineno = 0; ; lineno++) {
        EIO_Status rdstat;
        const char* msg;
        char buf[1024];
        int c, m;

        /* all FTP replies are at least '\n'-terminated, no ending with EOF */
        rdstat = SOCK_ReadLine(xxx->cntl, buf, sizeof(buf), &len);
        if (rdstat != eIO_Success) {
            status  = rdstat;
            break;
        }
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
            status  = parser(xxx, lineno  &&  m ? 0 : c, lineno, msg);
        if (!lineno) {
            *code = c;
            if (line)
                strncpy0(line, msg, maxlinelen);
        }
        if (m)
            break;
    }
    return status;
}


/* Close data connection w/error messages
 * how == eIO_Open                        -- unexpected closure, log error
 * how == eIO_Read or eIO_Write (or both) -- normal close in the direction
 *                                           no size check with eIO_ReadWrite
 * how == eIO_Close                       -- pre-approved close, w/o log errs
 * Notes:
 * 1. eIO_Open and eIO_Close suppress log message if xxx->cntl is closed;
 * 2. timeout is ignored for both eIO_Open and eIO_Close;
 * 3. post-condition: !xxx->data.
 */
static EIO_Status x_FTPCloseData(SFTPConnector* xxx,
                                 EIO_Event how, const STimeout* timeout)
{
    EIO_Status status;

    assert(xxx->data);
    if (xxx->flag & fFTP_LogControl)
        SOCK_SetDataLogging(xxx->data, eOn);

    if (how & eIO_ReadWrite) {
        TNCBI_BigCount size = xxx->size  &&  how != eIO_ReadWrite
            ? SOCK_GetCount(xxx->data, how) : xxx->size;
        assert(!xxx->sync); /* still expecting close ack */
        SOCK_SetTimeout(xxx->data, eIO_Close, timeout);
        status = SOCK_Close(xxx->data);
        if (status != eIO_Success) {
            CORE_LOGF_X(7, eLOG_Error,
                        ("[FTP; %s]  Error closing data connection: %s",
                         xxx->what, IO_StatusStr(status)));
        } else if (xxx->size != size) {
            if (how == eIO_Write) {
                CORE_LOGF_X(9, eLOG_Error,
                            ("[FTP; %s]  Incomplete data transfer: "
                             "%" NCBI_BIGCOUNT_FORMAT_SPEC " out of "
                             "%" NCBI_BIGCOUNT_FORMAT_SPEC " byte%s uploaded",
                             xxx->what, size,
                             xxx->size, &"s"[xxx->size == 1]));
                status = eIO_Unknown;
            } else if (xxx->rest == (TNCBI_BigCount)(-1L)  ||
                       xxx->rest + size == xxx->size) {
                static const char* kWarningFmt[] =
                    { "[FTP; %s]  Server reports restarted download"
                      " size incorrectly: %" NCBI_BIGCOUNT_FORMAT_SPEC,
                      "[FTP; %s]  Restart parse error prevents download"
                      " size verification: %" NCBI_BIGCOUNT_FORMAT_SPEC };
                CORE_LOGF_X(11, eLOG_Warning,
                            (kWarningFmt[xxx->rest != (TNCBI_BigCount)(-1L)],
                             xxx->what, xxx->size));
            } else {
                CORE_LOGF_X(8, eLOG_Error,
                            ("[FTP; %s]  Premature EOF in data: "
                             "%" NCBI_BIGCOUNT_FORMAT_SPEC " byte%s expected, "
                             "%" NCBI_BIGCOUNT_FORMAT_SPEC " byte%s received",
                             xxx->what, xxx->size, &"s"[xxx->size == 1],
                             size, &"s"[size == 1]));
                status = eIO_Unknown;
            }
        } else if (size  &&  how != eIO_ReadWrite)
            CORE_TRACEF(("[FTP; %s]  Transfer size verified", xxx->what));
    } else {
        if (!xxx->cntl) {
            how = eIO_Open;
        } else if (xxx->what  &&  how != eIO_Close) {
            CORE_LOGF_X(1, xxx->send ? eLOG_Error : eLOG_Warning,
                        ("[FTP; %s]  Data connection transfer aborted",
                         xxx->what));
        }
        if (how != eIO_Close) {
            status = SOCK_Abort(xxx->data);
            SOCK_Close(xxx->data);
        } else {
            SOCK_SetTimeout(xxx->data, eIO_Close, &kZeroTimeout); 
            status = SOCK_Close(xxx->data);
        }
        xxx->open = 0/*false*/;
    }

    xxx->data = 0;
    return status;
}


static EIO_Status s_FTPReply(SFTPConnector* xxx, int* code,
                             char* line, size_t maxlinelen,
                             FFTPReplyParser parser)
{
    EIO_Status status;
    int        c = 0;

    if (xxx->cntl) {
        status = x_FTPParseReply(xxx, &c, line, maxlinelen, parser);
        if (status != eIO_Timeout)
            xxx->sync = 1/*true*/;
        if (status == eIO_Success) {
            if (c == 421)
                status = eIO_Closed;
            else if (c == 502)
                status = eIO_NotSupported;
            else if (c == 332  ||  c == 532)
                status = eIO_NotSupported/*account*/;
            else if (c == 110  &&  (xxx->data  ||  xxx->send))
                status = eIO_NotSupported/*restart mark*/;
        }
        if (status == eIO_Closed   ||  c == 221) {
            SOCK cntl = xxx->cntl;
            xxx->cntl = 0;
            if (status == eIO_Closed) {
                CORE_LOGF_X(10, eLOG_Error,
                            ("[FTP%s%s]  Lost connection to server @ %s:%hu",
                             xxx->what ? "; " : "", xxx->what ? xxx->what : "",
                             xxx->info->host, xxx->info->port));
            }
            if (xxx->data)
                x_FTPCloseData(xxx, eIO_Close/*silent close*/, 0);
            if (status == eIO_Closed)
                SOCK_Abort(cntl);
            else
                SOCK_SetTimeout(cntl, eIO_Close, &kZeroTimeout);
            SOCK_Close(cntl);
        }
        if (status == eIO_Success  &&  c == 530/*not logged in*/)
            status  = eIO_Closed;
    } else
        status = eIO_Closed;
    if (code)
        *code = c;
    return status;
}


static EIO_Status s_FTPDrainReply(SFTPConnector* xxx, int* code, int cXX)
{
    int        c;
    EIO_Status status;
    int        quit = *code;
    *code = 0;
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
    char*      line;
    EIO_Status status;
    char       x_buf[128];
    size_t     cmdlen, arglen, linelen;

    if (!xxx->cntl)
        return eIO_Closed;

    cmdlen  = strlen(cmd);
    arglen  = arg ? strlen(arg) : 0;
    linelen = cmdlen + 2;
    if (arg)
        linelen += 1 + arglen;
    line    = linelen < sizeof(x_buf) ? x_buf : (char*) malloc(linelen + 1);

    if (line) {
        ESwitch log = eDefault;
        memcpy(line, cmd, cmdlen);
        if (arg) {
            line[cmdlen++] = ' ';
            memcpy(line + cmdlen, arg, arglen);
            cmdlen += arglen;
        }
        line[cmdlen++] = '\r';
        line[cmdlen++] = '\n';
        line[cmdlen]   = '\0';
        log = off ? SOCK_SetDataLogging(xxx->cntl, eOff) : eOff;
        status = SOCK_Write(xxx->cntl, line, cmdlen, 0, eIO_WritePersist);
        if (off  &&  log != eOff) {
            SOCK_SetDataLogging(xxx->cntl, log);
            if (log == eOn  ||  SOCK_SetDataLoggingAPI(eDefault) == eOn)
                CORE_LOGF_X(4, eLOG_Trace,
                            ("Sending FTP %.*s command (%s)",
                             (int) strcspn(line, " \t"), line,
                             IO_StatusStr(status)));
        }
        if (line != x_buf)
            free(line);
        xxx->sync = 0/*false*/;
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
        assert(code == 211  ||  code == 214);
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
        if ((s = x_4Word(line, "SIZE")) != 0) {  /* RFC 3659 */
            if (s[4 + strspn(s + 4, " \t")] != '*')
                xxx->feat |=  fFtpFeature_SIZE;
            else
                xxx->feat &= ~fFtpFeature_SIZE;
        }
        if ((s = x_4Word(line, "REST")) != 0) {  /* RFC 3659, NB: FEAT */
            if (s[4 + strspn(s + 4, " \t")] != '*')
                xxx->feat |=  fFtpFeature_REST;
            else
                xxx->feat &= ~fFtpFeature_REST;
        }
        if ((s = x_4Word(line, "MLST")) != 0) {  /* RFC 3659 */
            if (s[4 + strspn(s + 4, " \t")] != '*')
                xxx->feat |=  fFtpFeature_MLSx;
            else
                xxx->feat &= ~fFtpFeature_MLSx;
        }
        if ((s = x_4Word(line, "MLSD")) != 0) {  /* RFC 3659 */
            if (s[4 + strspn(s + 4, " \t")] != '*')
                xxx->feat |=  fFtpFeature_MLSx;
            else
                xxx->feat &= ~fFtpFeature_MLSx;
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
    } /* else last line */
    return eIO_Success;
}


static EIO_Status x_FTPHelp(SFTPConnector* xxx)
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
    if (code  &&  strlen(line) >= 4
        &&  (!line[4]  ||  isspace((unsigned char) line[4]))) {
        assert(code == 211);
        if      (strncasecmp(line, "MDTM", 4) == 0)
            xxx->feat |= fFtpFeature_MDTM;
        else if (strncasecmp(line, "SIZE", 4) == 0)
            xxx->feat |= fFtpFeature_SIZE;
        else if (strncasecmp(line, "REST", 4) == 0)
            xxx->feat |= fFtpFeature_REST;  /* NB: "STREAM" must also follow */
        else if (strncasecmp(line, "MLST", 4) == 0)
            xxx->feat |= fFtpFeature_MLSx;
        else if (strncasecmp(line, "EPRT", 4) == 0)
            xxx->feat |= fFtpFeature_EPRT;
        else if (strncasecmp(line, "EPSV", 4) == 0)
            xxx->feat |= fFtpFeature_EPSV;
    }
    return eIO_Success;
}


static EIO_Status x_FTPFeat(SFTPConnector* xxx)
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
static EIO_Status x_FTPNoop(SFTPConnector* xxx)
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


static EIO_Status x_FTPFeatures(SFTPConnector* xxx)
{
    xxx->soft = 0;
    if (xxx->flag & fFTP_UseFeatures) {
        /* try to setup features */
        if (x_FTPHelp(xxx) == eIO_Closed)
            return eIO_Closed;
        if (x_FTPFeat(xxx) == eIO_Closed)
            return eIO_Closed;
        /* make sure the connection is still good */
        return x_FTPNoop(xxx);
    }
    return eIO_Success;
}


static EIO_Status x_FTPLogin(SFTPConnector* xxx)
{
    int code;
    EIO_Status status;

    xxx->feat = 0;
    status = s_FTPReply(xxx, &code, 0, 0, 0);
    if (status != eIO_Success)
        return status;
    if (code == 120)
        return eIO_Timeout;
    if (code != 220  ||  !*xxx->info->user)
        return eIO_Unknown;
    status = s_FTPCommand(xxx, "USER", xxx->info->user);
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &code, 0, 0, 0);
    if (status != eIO_Success)
        return status;
    if (code != 230) {
        if (code != 331)
            return code == 332 ? eIO_NotSupported : eIO_Unknown;
        status = s_FTPCommandEx(xxx, "PASS", xxx->info->pass, 1);
        if (status != eIO_Success)
            return status;
        status = s_FTPReply(xxx, &code, 0, 0, 0);
        if (status != eIO_Success)
            return status;
        if (code == 503)
            return eIO_Closed;
        if (code != 230  &&  code != 202)
            return code == 332 ? eIO_NotSupported : eIO_Unknown;
    }
    status = x_FTPFeatures(xxx);
    if (status != eIO_Success)
        return status;
    if (xxx->flag & fFTP_LogControl) {
        CORE_LOGF_X(3, eLOG_Trace,
                    ("[FTP]  Server ready @ %s:%hu, features = 0x%02X",
                     xxx->info->host, xxx->info->port,
                     (unsigned int) xxx->feat));
    }
    if (xxx->feat &  fFtpFeature_EPSV)
        xxx->feat |= fFtpFeature_APSV;
    assert(xxx->sync);
    return eIO_Success;
}


/* all implementations MUST support TYPE (I and/or L 8, RFC1123 4.1.5) */
/* Note that other transfer defaults are:  STRU F, MODE S (RFC959 5.1) */
static EIO_Status x_FTPBinary(SFTPConnector* xxx)
{
    int code;
    EIO_Status status;
    const char* type = xxx->flag & fFTP_UseTypeL8 ? "L8" : "I";
    status = s_FTPCommand(xxx, "TYPE", type);
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &code, 0, 0, 0);
    if (status != eIO_Success)
        return status;
    return code == 200 ? eIO_Success : eIO_Unknown;
}


static EIO_Status x_FTPRest(SFTPConnector* xxx,
                            const char*    arg,
                            int/*bool*/    out)
{
    int code;
    EIO_Status status = s_FTPCommand(xxx, "REST", arg);
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &code, 0, 0, 0);
    if (status != eIO_Success)
        return status;
    if (code == 350) {
        return out  &&  !BUF_Write(&xxx->rbuf, "350", 3)
            ? eIO_Unknown : eIO_Success;
    }
    if (code == 501  ||  /* RFC1123 4.1.3.4: */ code == 554  ||  code == 555)
        return eIO_NotSupported;
    return xxx->feat & fFtpFeature_REST ? eIO_Unknown : eIO_NotSupported;
}


static char* x_FTPUnquote(char* str, size_t* len)
{
    char* s = ++str;
    assert(str[-1] == '"');
    for (;;) {
        size_t l = strcspn(s, "\"");
        if (!*(s += l))
            break;
        if (*++s != '"') {
            *--s  = '\0';
            *len  = (size_t)(s - str);
            return str;
        }
        memmove(s, s + 1, strlen(s + 1) + 1);
    }
    *len = 0;
    return 0;
}


/* (null), MKD, RMD, PWD, CWD, CDUP, XMKD, XRMD, XPWD, XCWD, XCUP */
static EIO_Status x_FTPDir(SFTPConnector* xxx,
                           const char*    cmd,
                           const char*    arg)
{
    static const char kCwd[] = "CWD";
    EIO_Status status;
    char buf[256];
    int code;

    assert(!arg  ||  *arg);
    assert(!cmd  ||  strlen(cmd) >= 3);
    assert( cmd  ||  (arg  &&  arg == xxx->info->path));
    status = s_FTPCommand(xxx, cmd ? cmd : kCwd, cmd ? 0 : arg);
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &code, buf, sizeof(buf) - 1, 0);
    if (status != eIO_Success  &&  (status != eIO_NotSupported || code != 502))
        return status;
    if (code == 500  ||  code == 502) {
        /* RFC1123 4.1.3.1 requires reissue the command using an X-form */
        char verb[5];
        if (!cmd)
            cmd = kCwd;
        else if (toupper((unsigned char)(*cmd))  == 'X')
            return code == 500 ? eIO_Unknown : eIO_NotSupported;
        else if (toupper((unsigned char) cmd[2]) == 'U') /* CDUP */
            cmd = "CUP";
        verb[0] = 'X';
        strupr(strncpy0(verb + 1, cmd, 3));
        status = s_FTPCommand(xxx, verb, arg);
        if (status != eIO_Success)
            return status;
        status = s_FTPReply(xxx, &code, buf, sizeof(buf) - 1, 0);
        if (status != eIO_Success)
            return status;
        if (code == 502)
            return eIO_NotSupported;
    } else if (!cmd) {
        cmd = kCwd;
    } else if (toupper((unsigned char)(*cmd)) == 'X')
        cmd++;
    if (toupper((unsigned char)(*cmd)) == 'R') { /* [X]RMD */
        if (code != 250)
            return eIO_Unknown;
    }else if (toupper((unsigned char)(*cmd)) == 'C') { /* [X]CWD, CDUP/XCUP */
        if (code != 200  &&  code != 250)
            return eIO_Unknown;
        /* fixup codes w/accordance to RFC959 */
        if (toupper((unsigned char) cmd[1]) != 'W') {
            /* CDUP, XCUP */
            if (code != 200)
                code  = 200;
        } else
            if (code != 250)
                code  = 250;
    } else { /* [X]MKD & [X]PWD */
        char*  dir;
        size_t len;
        if (code != 257) {
            /* [X]MKD: 521 "directory exists, no action taken" */
            return toupper((unsigned char)(*cmd)) != 'M'  ||  code != 521
                ? eIO_Unknown : eIO_Success;
        }
        dir = buf + strspn(buf, " ");
        return *dir != '"'
            ||  !(dir = x_FTPUnquote(dir, &len))
            ||  !BUF_Write(&xxx->rbuf, dir, len)
            ? eIO_Unknown : eIO_Success;
    }

    if (arg == xxx->info->path)
        return eIO_Success;
    code = sprintf(buf, "%d", code);
    assert((size_t) code < sizeof(buf));
    return !BUF_Write(&xxx->rbuf, buf, (size_t) code)
        ? eIO_Unknown : eIO_Success;
}


static EIO_Status x_FTPTelnetSynch(SFTPConnector* xxx)
{
    EIO_Status status;
    size_t     n;

    /* Send TELNET IAC/IP (Interrupt Process) command */
    status = SOCK_Write(xxx->cntl, "\377\364", 2, &n, eIO_WritePersist);
    if (status != eIO_Success)
        return status;
    assert(n == 2);
    /* Send TELNET IAC/DM (Data Mark) command to complete SYNCH, RFC 854 */
    status = SOCK_Write(xxx->cntl, "\377\362", 2, &n, eIO_WriteOutOfBand);
    if (status != eIO_Success)
        return status;
    return n == 2 ? eIO_Success : eIO_Unknown;
}


/*
 * how = 0 -- ABOR sequence only if data connection is open;
 * how = 1 -- just abort data connection, if any open;
 * how = 2 -- force full ABOR sequence;
 * how = 3 -- abort current command.
 *
 * Post-condition: !xxx->data
 */
static EIO_Status x_FTPAbort(SFTPConnector*  xxx,
                             int             how,
                             const STimeout* timeout)
{
    EIO_Status  status;

    if (!xxx->data  &&  how != 2)
        return eIO_Success;
    if (!xxx->cntl  ||  how == 1)
        return x_FTPCloseData(xxx, eIO_Close/*silent close*/, 0);
    if (!timeout)
        timeout = &kFailsafeTimeout;
    SOCK_SetTimeout(xxx->cntl, eIO_ReadWrite, timeout);
    status = x_FTPTelnetSynch(xxx);
    if (status == eIO_Success)
        status  = s_FTPCommand(xxx, "ABOR", 0);
    if (xxx->data) {
        if (status == eIO_Success  &&  !xxx->send) {
            /* this is not "data" per se, so go silent */
            if (xxx->flag & fFTP_LogData)
                SOCK_SetDataLogging(xxx->data, eDefault);
            SOCK_SetTimeout(xxx->data, eIO_ReadWrite, timeout);
            /* drain up data connection by discarding 1MB blocks repeatedly */
            while (SOCK_Read(xxx->data, 0, 1<<20, 0, eIO_ReadPlain)
                   == eIO_Success) {
                continue;
            }
        }
        x_FTPCloseData(xxx, how == 3
                       ||  SOCK_Status(xxx->data, eIO_Read) != eIO_Closed
                       ? eIO_Open/*warning*/ : eIO_Close/*silent*/, 0);
    }
    assert(!xxx->data);
    if (status == eIO_Success) {
        int         code = 426;
        int/*bool*/ sync = xxx->sync;
        status = s_FTPDrainReply(xxx, &code, 2/*2xx*/);
        if (status == eIO_Success) {
            /* Microsoft FTP is known to return 225 instead of 226 */
            if (code != 225  &&  code != 226  &&  code != 426)
                status = eIO_Unknown;
        } else if (status == eIO_Timeout  &&  !code)
            sync = 0/*false*/;
        xxx->sync = sync;
    }
    return status;
}


static EIO_Status x_FTPEpsv(SFTPConnector*  xxx,
                            unsigned int*   host,
                            unsigned short* port)
{
    EIO_Status status;
    char buf[128], d;
    unsigned int p;
    const char* s;
    int n;

    assert(port  ||  (xxx->feat & fFtpFeature_APSV) == fFtpFeature_APSV);
    if (xxx->flag & fFTP_NoExtensions)
        return eIO_NotSupported;
    status = s_FTPCommand(xxx, "EPSV", port ? 0 : "ALL");
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &n, buf, sizeof(buf) - 1, 0);
    if (status != eIO_Success)
        return status;
    if (!port)
        return n == 200 ? eIO_Success : eIO_NotSupported;
    if (n != 229)
        return xxx->feat & fFtpFeature_APSV ? eIO_Unknown : eIO_NotSupported;
    buf[sizeof(buf) - 1] = '\0';
    if (!(s = strchr(buf, '('))  ||  !(d = *++s)  ||  *++s != d  ||  *++s != d
        ||  sscanf(++s, "%u%c%n", &p, buf, &n) < 2  ||  p > 0xFFFF
        ||  *buf != d  ||  s[n] != ')') {
        return eIO_Unknown;
    }
    *host = 0;
    *port = (unsigned short) p;
    return eIO_Success;
}


/* all implementations MUST support PASV */
static EIO_Status x_FTPPasv(SFTPConnector*  xxx,
                            unsigned int*   host,
                            unsigned short* port)
{
    EIO_Status status;
    int  code, o[6];
    unsigned int i;
    char buf[128];

    status = s_FTPCommand(xxx, "PASV", 0);
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &code, buf, sizeof(buf) - 1, 0);
    if (status != eIO_Success  ||  code != 227)
        return eIO_Unknown;
    buf[sizeof(buf) - 1] = '\0';
    for (;;) {
        char* c;
        size_t len;
        /* RFC 1123 4.1.2.6 says that ()'s in PASV reply MUST NOT be assumed */
        for (c = buf;  *c;  ++c) {
            if (isdigit((unsigned char)(*c)))
                break;
        }
        if (!*c)
            return eIO_Unknown;
        len = 0;
        for (i = 0;  i < (unsigned int)(sizeof(o) / sizeof(o[0]));  i++) {
            if (sscanf(c + len, ",%d%n" + !i, &o[i], &code) < 1)
                break;
            len += code;
        }
        if (i >= (unsigned int)(sizeof(o) / sizeof(o[0])))
            break;
        if (!len) {
            len = strspn(c, kDigits);
            assert(len > 0);
        }
        memmove(buf, c + len, strlen(c + len) + 1);
    }
    for (i = 0;  i < (unsigned int)(sizeof(o) / sizeof(o[0]));  i++) {
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


static EIO_Status x_FTPPassive(SFTPConnector*  xxx,
                               const STimeout* timeout)
{
    EIO_Status   status;
    unsigned int   host;
    unsigned short port;
    char           addr[40];

    if ((xxx->feat & fFtpFeature_APSV) == fFtpFeature_APSV) {
        /* first time here, try to set EPSV ALL */
        if (x_FTPEpsv(xxx, 0, 0) == eIO_Success)
            xxx->feat &= ~fFtpFeature_EPSV;                     /* APSV mode */
        else
            xxx->feat &= ~fFtpFeature_APSV | fFtpFeature_EPSV;  /* EPSV mode */
    }
    if (xxx->feat & fFtpFeature_APSV) {
        status = x_FTPEpsv(xxx, &host, &port);
        switch (status) {
        case eIO_NotSupported:
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
    if (!port  &&  (status = x_FTPPasv(xxx, &host, &port)) != eIO_Success)
        return status;
    assert(port);
    if (( host  &&
          SOCK_ntoa(host, addr, sizeof(addr)) != 0)  ||
        (!host  &&
         !SOCK_GetPeerAddressStringEx(xxx->cntl, addr,sizeof(addr), eSAF_IP))){
        return eIO_Unknown;
    }
    status = SOCK_CreateEx(addr, port, timeout, &xxx->data, 0, 0,
                           xxx->flag & fFTP_LogControl
                           ? fSOCK_LogOn : fSOCK_LogDefault);
    if (status != eIO_Success) {
        assert(!xxx->data);
        CORE_LOGF_X(2, eLOG_Error,
                    ("[FTP; %s]  Cannot open data connection to %s:%hu (%s)",
                     xxx->what, addr, port, IO_StatusStr(status)));
        return status;
    }
    assert(xxx->data);
    SOCK_SetDataLogging(xxx->data, xxx->flag & fFTP_LogData ? eOn : eDefault);
    return eIO_Success;
}


static EIO_Status x_FTPEprt(SFTPConnector* xxx,
                            unsigned int   host,
                            unsigned short port)
{
    char       buf[80];
    EIO_Status status;
    int        code;

    if (xxx->flag & fFTP_NoExtensions)
        return eIO_NotSupported;
    memcpy(buf, "|1|", 3); /*IPv4*/
    SOCK_ntoa(host, buf + 3, sizeof(buf) - 3);
    sprintf(buf + 3 + strlen(buf + 3), "|%hu|", port);
    status = s_FTPCommand(xxx, "EPRT", buf);
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &code, 0, 0, 0);
    if (status != eIO_Success)
        return status;
    if (code == 500  ||  code == 501)
        return xxx->feat & fFtpFeature_EPRT ? eIO_Unknown : eIO_NotSupported;
    if (code == 522)
        return eIO_NotSupported;
    return code == 200 ? eIO_Success : eIO_Unknown;
}


/* all implementations MUST support PORT */
static EIO_Status x_FTPPort(SFTPConnector* xxx,
                            unsigned int   host,
                            unsigned short port)
{
    unsigned char octet[sizeof(host) + sizeof(port)];
    char          buf[80], *s = buf;
    EIO_Status    status;
    int           code;
    size_t        n;

    port = SOCK_HostToNetShort(port);
    memcpy(octet,                &host, sizeof(host));
    memcpy(octet + sizeof(host), &port, sizeof(port));
    for (n = 0;  n < sizeof(octet);  n++)
        s += sprintf(s, "%s%u", &","[!n], octet[n]);
    assert(s < buf + sizeof(buf));
    status = s_FTPCommand(xxx, "PORT", buf);
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &code, 0, 0, 0);
    if (status != eIO_Success)
        return status;
    return code == 200 ? eIO_Success : eIO_Unknown;
}


static EIO_Status x_FTPActive(SFTPConnector*  xxx,
                              LSOCK*          lsock,
                              const STimeout* timeout)
{
    EIO_Status   status;
    unsigned int   host;
    unsigned short port;

    /* NB: Apache FTP proxy re-uses SOCK_LocalPort(xxx->cntl);
     * which is the default port for user-end data port per RFC959,
     * other implementations don't do that leaving OS to decide,
     * since the PORT command will be issued, anyways... */
    status = LSOCK_CreateEx(0, 1, lsock, xxx->flag & fFTP_LogControl
                            ? fSOCK_LogOn : fSOCK_LogDefault);
    if (status != eIO_Success)
        return status;
    if (!(host = SOCK_GetLocalHostAddress(eDefault))  ||
        !(port = LSOCK_GetPort(*lsock, eNH_HostByteOrder))) {
        return eIO_Unknown;
    }
    if (xxx->feat & fFtpFeature_EPRT) {
        status = x_FTPEprt(xxx, host, port);
        if (status != eIO_NotSupported)
            return status;
        xxx->feat &= ~fFtpFeature_EPRT;
    }
    return x_FTPPort(xxx, host, port);
}


static EIO_Status x_FTPOpenData(SFTPConnector*  xxx,
                                LSOCK*          lsock,
                                const STimeout* timeout)
{
    EIO_Status status;

    *lsock = 0;
    if ((xxx->flag & fFTP_UsePassive)  ||  !(xxx->flag & fFTP_UseActive)) {
        status = x_FTPPassive(xxx, timeout);
        if (status == eIO_Success  ||
            (xxx->flag & (fFTP_UseActive|fFTP_UsePassive)) == fFTP_UsePassive){
            return status;
        }
        if ((xxx->feat & fFtpFeature_APSV) != (xxx->feat & fFtpFeature_EPSV)) {
            /* seems like an impossible case (EPSV ALL accepted but no EPSV);
             * still, better safe than sorry */
            return status;
        }
    }
    status = x_FTPActive(xxx, lsock, timeout);
    if (status != eIO_Success) {
        if (*lsock) {
            LSOCK_Close(*lsock);
            *lsock = 0;
        }
    } else
        assert(*lsock);
    return status;
}


/* LIST, NLST, RETR, STOR, APPE, MLSD */
static EIO_Status x_FTPXfer(SFTPConnector*  xxx,
                            const char*     cmd,
                            const STimeout* timeout,
                            FFTPReplyParser parser)
{
    int code;
    LSOCK lsock;
    EIO_Status status = x_FTPOpenData(xxx, &lsock, timeout);
    if (status != eIO_Success) {
        assert(!lsock  &&  !xxx->data);
        xxx->open = 0/*false*/;
        return status;
    }
    if (xxx->rest  &&  (xxx->flag & fFTP_DelayRestart)) {
        char buf[80];
        assert(xxx->rest != (TNCBI_BigCount)(-1L));
        sprintf(buf, "%" NCBI_BIGCOUNT_FORMAT_SPEC, xxx->rest);
        status = x_FTPRest(xxx, buf, 0/*false*/);
    }
    xxx->r_status = status;
    if (status == eIO_Success)
        status  = s_FTPCommand(xxx, cmd, 0);
    if (status == eIO_Success)
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
                    /* NB: data conn may have started at the server end */
                    code = 2/*full abort*/;
                } else {
                    SOCK_SetDataLogging(xxx->data, xxx->flag & fFTP_LogData
                                        ? eOn : eDefault);
                }
                LSOCK_Close(lsock);
                lsock = 0;
            }
            if (status == eIO_Success) {
                assert(xxx->data);
                if (xxx->send) {
                    if (!(xxx->flag & fFTP_UncorkUpload))
                        SOCK_SetCork(xxx->data, 1);
                    assert(xxx->open);
                    xxx->size = 0;
                }
                xxx->sync = 0/*false*/;
                return eIO_Success;
            }
        } else if (code == 450  ||  code == 550) {
            /* file processing errors: not a file, not a dir, etc */
            status = eIO_Closed;
            code = 1/*quick*/;
        } else {
            status = eIO_Unknown;
            code = 1/*quick*/;
        }
    } else
        code = 0/*regular*/;
    if (lsock)
        LSOCK_Close(lsock);
    x_FTPAbort(xxx, code, timeout);
    assert(status != eIO_Success);
    xxx->open = 0/*false*/;
    assert(!xxx->data);
    return status;
}


static EIO_Status x_FTPRename(SFTPConnector* xxx,
                              const char*    src,
                              const char*    dst)
{
    int code;
    EIO_Status status = s_FTPCommand(xxx, "RNFR", src);
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &code, 0, 0, 0);
    if (status != eIO_Success)
        return status;
    if (code != 350)
        return code == 450  ||  code == 550 ? eIO_Closed : eIO_Unknown;
    status = s_FTPCommand(xxx, "RNTO", dst);
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &code, 0, 0, 0);
    if (status != eIO_Success)
        return status;
    if (code != 250)
        return code == 553 ? eIO_Closed  : eIO_Unknown;
    return BUF_Write(&xxx->rbuf, "250", 3) ? eIO_Success : eIO_Unknown;
}


/* REN */
static EIO_Status s_FTPRename(SFTPConnector* xxx,
                              const char*    arg)
{
    char *buf, *tmp;
    EIO_Status status;
    const char *src, *dst;
    size_t len = strcspn(arg, " \t");

    if (!arg[len]  ||  !(buf = strdup(arg)))
        return eIO_Unknown;

    tmp = buf;
    if (*tmp != '"') {
        src = tmp;
        tmp[len] = '\0';
    } else {
        src = x_FTPUnquote(tmp, &len);
        ++len;
    }
    tmp += ++len;
    tmp += strspn(tmp, " \t");
    if (*tmp != '"') {
        len = strcspn(tmp, " \t");
        dst = tmp;
        if (tmp[len])
            tmp[len++] = '\0';
    } else {
        dst = x_FTPUnquote(tmp, &len);
        len += 2;
    }
    tmp += len;

    status =
        src  &&  *src  &&  dst  &&  *dst  &&  !tmp[strspn(tmp, " \t")]
        ? x_FTPRename(xxx, src, dst)
        : eIO_Unknown;

    free(buf);
    return status;
}


static EIO_Status s_FTPDir(SFTPConnector* xxx,
                           const char*    cmd,
                           const char*    arg)
{
    assert(cmd  &&  arg  &&  arg != xxx->info->path  &&  !BUF_Size(xxx->rbuf));
    return x_FTPDir(xxx, cmd, *arg ? arg : 0);
}


/* SYST */
static EIO_Status s_FTPSyst(SFTPConnector* xxx,
                            const char*    cmd)
{
    int code;
    char buf[128];
    EIO_Status status = s_FTPCommand(xxx, cmd, 0);
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &code, buf, sizeof(buf) - 1, 0);
    if (status != eIO_Success)
        return status;
    if (code != 215  ||  !BUF_Write(&xxx->rbuf, buf, strlen(buf)))
        return eIO_Unknown;
    return eIO_Success;
}


static EIO_Status x_FTPParseStat(SFTPConnector* xxx, int code,
                                 size_t lineno, const char* line)
{
    if (!lineno  &&  code != 211  &&  code != 212  &&  code != 213)
        return code == 450 ? eIO_Closed : eIO_NotSupported;
    /* NB: STAT uses ASA (FORTRAN) vertical format control in 1st char */
    if (!BUF_Write(&xxx->rbuf, line, strlen(line))  ||
        !BUF_Write(&xxx->rbuf, "\n", 1)) {
        /* NB: leaving partial buffer, it's just an info */
        return eIO_Unknown;
    }
    return eIO_Success;
}


/* STAT */
static EIO_Status s_FTPStat(SFTPConnector* xxx,
                            const char*    cmd)
{
    EIO_Status status = s_FTPCommand(xxx, cmd, 0);
    if (status != eIO_Success)
        return status;
    return s_FTPReply(xxx, 0, 0, 0, x_FTPParseStat);
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


/* SIZE */
static EIO_Status s_FTPSize(SFTPConnector* xxx,
                            const char*    cmd)
{
    int code;
    char buf[128];
    EIO_Status status = s_FTPCommand(xxx, cmd, 0);
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &code, buf, sizeof(buf) - 1, 0);
    if (status == eIO_Success) {
        switch (code) {
        case 213:
            status = x_FTPParseSize(xxx, buf);
            break;
        case 550:
            /* file not plain or does not exist, EOF */
            break;
        default:
            status =
                xxx->feat & fFtpFeature_SIZE ? eIO_Unknown : eIO_NotSupported;
            break;
        }
    }
    return status;
}


static EIO_Status x_FTPParseMdtm(SFTPConnector* xxx, char* timestamp)
{
    static const int kDay[12] = {31,  0, 31, 30, 31, 30,
                                 31, 31, 30, 31, 30, 31};
    char* frac = strchr(timestamp, '.');
    int field[6], n;
    struct tm tm;
    char buf[80];
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
     *    whilst an MDTM response must not contain even a single space. */
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


/* MDTM */
static EIO_Status s_FTPMdtm(SFTPConnector* xxx,
                            const char*    cmd)
{
    int code;
    char buf[128];
    EIO_Status status = s_FTPCommand(xxx, cmd, 0);
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &code, buf, sizeof(buf) - 1, 0);
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


/* REST */
static EIO_Status s_FTPRestart(SFTPConnector* xxx,
                               const char*    arg)
{
    TNCBI_BigCount rest;
    int n;

    if (sscanf(arg, "%" NCBI_BIGCOUNT_FORMAT_SPEC "%n", &rest, &n) < 1
        ||  arg[n]) {
        if (xxx->flag & fFTP_DelayRestart) {
            return eIO_Unknown;
        }
        xxx->rest = (TNCBI_BigCount)(-1L);
        xxx->rclr = 0/*false*/;
    } else {
        xxx->rclr = 0/*false*/;
        xxx->rest = rest;
        if (xxx->flag & fFTP_DelayRestart) {
            if (rest)
                return eIO_Success;
            /* "REST 0" goes through right away */
        }
    }
    return x_FTPRest(xxx, arg, 1/*true*/);
}


static EIO_Status x_FTPSzcb(SFTPConnector* xxx, int code,
                            size_t lineno, const char* line)
{
    EIO_Status status = eIO_Success;
    if (!lineno  &&  (code == 125  ||  code == 150)) {
        const char* comment = strrchr(line, '(');
        size_t n, m;
        if (comment  &&  strchr(++comment, ')')
            &&  (n = strspn(comment, kDigits)) > 0
            &&  (m = strspn(comment + n, " \t")) > 0
            &&  strncasecmp(comment + n + m, "byte", 4) == 0) {
            TNCBI_BigCount size;
            int            k;
            if (sscanf(comment, "%" NCBI_BIGCOUNT_FORMAT_SPEC "%n",
                       &size, &k) < 1  &&  k != (int) n) {
                CORE_TRACEF(("[FTP; %s]  Error reading size from \"%.*s\"",
                             xxx->what, (int) n, comment));
            } else
                xxx->size = size;
            if (xxx->cmcb.func) {
                char* text = (char*) malloc(n + 1);
                if (text) {
                    status = xxx->cmcb.func(xxx->cmcb.data, xxx->what,
                                            strncpy0(text, comment, n));
                    free(text);
                } else
                    status = eIO_Unknown;
            }
        }
    }
    return status;
}


/* RETR, LIST, NLST */
/* all implementations MUST support RETR */
static EIO_Status s_FTPRetrieve(SFTPConnector*  xxx,
                                const char*     cmd,
                                const STimeout* timeout)
{
    xxx->size = 0;
    return x_FTPXfer(xxx, cmd, timeout, x_FTPSzcb);
}


/* STOR, APPE */
/* all implementations MUST support STOR */
static EIO_Status s_FTPStore(SFTPConnector*  xxx,
                             const char*     cmd,
                             const STimeout* timeout)
{
    xxx->send = xxx->open = 1/*true*/;
    return x_FTPXfer(xxx, cmd, timeout, 0);
}


/* DELE */
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
    if (code != 250  ||  !BUF_Write(&xxx->rbuf, "250", 3))
        return eIO_Unknown;
    return eIO_Success;
}


static EIO_Status x_FTPMlsd(SFTPConnector* xxx, int code,
                            size_t lineno, const char* line)
{
    if (!lineno  ||  code != 501)
        return eIO_Success;
    return xxx->feat & fFtpFeature_MLSx ? eIO_Closed : eIO_NotSupported;
}


static EIO_Status x_FTPMlst(SFTPConnector* xxx, int code,
                            size_t lineno, const char* line)
{
    if (!lineno) {
        return code == 250 ? eIO_Success :
            xxx->feat & fFtpFeature_MLSx ? eIO_Closed : eIO_NotSupported;
    }
    if (code) {
        if (*line  && /* NB: RFC3659 7.2, the leading space has been skipped */
            (!BUF_Write(&xxx->rbuf, line, strlen(line))  ||
             !BUF_Write(&xxx->rbuf, "\n", 1))) {
            /* NB: must reset partial rbuf */
            return eIO_Unknown;
        }
    } /* else last line */
    return eIO_Success;
}


/* MLST, MLSD */
static EIO_Status s_FTPMlsx(SFTPConnector*  xxx,
                            const char*     cmd,
                            const STimeout* timeout)
{
    if (cmd[3] == 'T') { /*MLST*/
        EIO_Status status = s_FTPCommand(xxx, cmd, 0);
        if (status != eIO_Success)
            return status;
        status = s_FTPReply(xxx, 0/*code checked by cb*/, 0, 0, x_FTPMlst);
        if (status != eIO_Success)
            BUF_Erase(xxx->rbuf);
        return status;
    }
    xxx->size = 0;  /* cf. s_FTPRetrieve() */
    return x_FTPXfer(xxx, cmd, timeout, x_FTPMlsd);
}


static EIO_Status x_FTPNgcb(SFTPConnector* xxx, int code,
                            size_t lineno, const char* line)
{
    if (lineno  &&  code / 100 == 2) {
        if (*line &&/*NB: RFC2389 3.2 & 4, the leading space has been skipped*/
            (!BUF_Write(&xxx->rbuf, line, strlen(line))  ||
             !BUF_Write(&xxx->rbuf, "\n", 1))) {
            /* NB: must reset partial rbuf */
            return eIO_Unknown;
        }
    }
    return eIO_Success;
}


/* FEAT, OPTS */
static EIO_Status s_FTPNegotiate(SFTPConnector* xxx,
                                 const char*    cmd)
{
    int code;
    EIO_Status status = s_FTPCommand(xxx, cmd, 0);
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &code, 0, 0, x_FTPNgcb);
    if (status == eIO_Success) {
        if (*cmd == 'F') {
            if (code != 211)
                status = eIO_Closed;
        } else {
            if (code == 451)
                status = eIO_Unknown;
            else if (code != 200)
                status = eIO_Closed;
        }
    }
    if (status != eIO_Success)
        BUF_Erase(xxx->rbuf);
    return status;
}


/* NB: data connection (upload only) may end up closed */
static EIO_Status s_FTPPollCntl(SFTPConnector* xxx, const STimeout* timeout)
{
    EIO_Status status = eIO_Success;
    while (SOCK_Wait(xxx->cntl, eIO_Read, &kZeroTimeout) == eIO_Success) {
        char buf[80];
        int  code;
        if (timeout != &kZeroTimeout) {
            SOCK_SetTimeout(xxx->cntl, eIO_Read,
                            timeout ? timeout : &kFailsafeTimeout);
            timeout  = &kZeroTimeout;
        }
        status = s_FTPReply(xxx, &code, buf, sizeof(buf) - 1, 0);
        if (status == eIO_Success) {
            assert(!xxx->data  ||  xxx->send);
            CORE_LOGF_X(12, eLOG_Error,
                        ("[FTP%s%s]  %spurious response %d from server%s%s",
                         xxx->what ? "; " : "", xxx->what ? xxx->what : "",
                         xxx->data ? "Aborting upload due to a s" : "S", code,
                         *buf ? ": " : "", buf));
            if (xxx->data) {
                x_FTPCloseData(xxx, eIO_Close/*silent*/, 0);
                xxx->sync = 1/*true*/;
                status = eIO_Closed;
            }
        }
        if (status == eIO_Closed)
            break;
    }
    return status;
}


static EIO_Status s_FTPSyncCntl(SFTPConnector* xxx, const STimeout* timeout)
{
    if (!xxx->sync) {
        EIO_Status status;
        SOCK_SetTimeout(xxx->cntl, eIO_Read, timeout);
        status = s_FTPReply(xxx, 0, 0, 0, 0);
        if (status != eIO_Success)
            return status;
        timeout = &kZeroTimeout;
        assert(xxx->sync);
    }
    return s_FTPPollCntl(xxx, timeout);
}


/* post-condition: empties xxx->wbuf, updates xxx->w_status */
static EIO_Status s_FTPExecute(SFTPConnector* xxx, const STimeout* timeout)
{
    EIO_Status status;
    size_t     size;
    char*      s;

    BUF_Erase(xxx->rbuf);
    status = x_FTPAbort(xxx, 3, timeout);
    if (xxx->what) {
        free((void*) xxx->what);
        xxx->what = 0;
    }
    if (status == eIO_Success)
        status  = s_FTPSyncCntl(xxx, timeout);
    if (status != eIO_Success)
        goto out;
    if (xxx->rest) {
        if (xxx->rclr) {
            xxx->rest = 0;
            xxx->rclr = 0/*false*/;
        } else
            xxx->rclr = 1/*true*/;
    }
    assert(xxx->cntl);
    verify((size = BUF_Size(xxx->wbuf)) != 0);
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
        assert(*c == ' '  ||  !*c);
        if (*s)
            xxx->what = s;
        if (size == 3  ||  size == 4) {
            SOCK_SetTimeout(xxx->cntl, eIO_ReadWrite, timeout);
            if         (size == 3  &&   strncasecmp(s, "REN",  3) == 0) {
                /* a special-case, non-standard command */
                status = s_FTPRename(xxx, c + strspn(c, " \t"));
            } else if ((size == 3  ||  toupper((unsigned char) c[-4]) == 'X')
                       &&          (strncasecmp(c - 3, "CWD",  3) == 0  ||
                                    strncasecmp(c - 3, "PWD",  3) == 0  ||
                                    strncasecmp(c - 3, "MKD",  3) == 0  ||
                                    strncasecmp(c - 3, "RMD",  3) == 0)) {
                status = s_FTPDir(xxx, s, *c ? c + 1 : c);
            } else if  (size == 4  &&  (strncasecmp(s, "CDUP", 4) == 0  ||
                                        strncasecmp(s, "XCUP", 4) == 0)) {
                status = s_FTPDir(xxx, s, *c ? c + 1 : c);
            } else if  (size == 4  &&   strncasecmp(s, "SYST", 4) == 0) {
                status = s_FTPSyst(xxx, s);
            } else if  (size == 4  &&   strncasecmp(s, "STAT", 4) == 0) {
                status = s_FTPStat(xxx, s);
            } else if  (size == 4  &&   strncasecmp(s, "SIZE", 4) == 0) {
                status = s_FTPSize(xxx, s);
            } else if  (size == 4  &&   strncasecmp(s, "MDTM", 4) == 0) {
                status = s_FTPMdtm(xxx, s);
            } else if  (size == 4  &&   strncasecmp(s, "DELE", 4) == 0) {
                status = s_FTPDele(xxx, s);
            } else if  (size == 4  &&   strncasecmp(s, "REST", 4) == 0) {
                status = s_FTPRestart (xxx, *c ? c + 1 : c);
            } else if  (size == 4  &&  (strncasecmp(s, "RETR", 4) == 0  ||
                                        strncasecmp(s, "LIST", 4) == 0  ||
                                        strncasecmp(s, "NLST", 4) == 0)) {
                status = s_FTPRetrieve(xxx, s, timeout);
            } else if  (size == 4  &&  (strncasecmp(s, "STOR", 4) == 0  ||
                                        strncasecmp(s, "APPE", 4) == 0)) {
                status = s_FTPStore   (xxx, s, timeout);
            } else if  (size == 4  &&  (strncasecmp(s, "MLSD", 4) == 0  ||
                                        strncasecmp(s, "MLST", 4) == 0)) {
                status = s_FTPMlsx    (xxx, s, timeout);
            } else if  (size == 4  &&  (strncasecmp(s, "FEAT", 4) == 0  ||
                                        strncasecmp(s, "OPTS", 4) == 0)) {
                status = s_FTPNegotiate(xxx, s);
            } else if  (size == 4  &&   strncasecmp(s, "NOOP", 4) == 0 && !*c){
                /* Special, means to stop the current command and reach EOF */
                *s = '\0';
                xxx->what = 0;
                status = x_FTPNoop(xxx);
            } else
                status = eIO_NotSupported;
        } else
            status = eIO_NotSupported;
        if (*s)
            s = 0;
    } else
        status = eIO_Unknown;
    if (s)
        free(s);
 out:
    xxx->w_status = status;
    BUF_Erase(xxx->wbuf);
    return status;
}


static EIO_Status s_FTPCompleteUpload(SFTPConnector*  xxx,
                                      const STimeout* timeout)
{
    EIO_Status status;
    int code;

    assert(!BUF_Size(xxx->rbuf));
    assert(xxx->cntl  &&  xxx->send  &&  xxx->open);
    if (xxx->data) {
        status = x_FTPCloseData(xxx, xxx->flag & fFTP_NoSizeChecks
                                ? eIO_ReadWrite : eIO_Write, timeout);
        xxx->w_status = status;
        if (status != eIO_Success)
            return status;
        assert(!xxx->data);
    }
    SOCK_SetTimeout(xxx->cntl, eIO_Read, timeout);
    status = s_FTPReply(xxx, &code, 0, 0, 0);
    if (status != eIO_Timeout) {
        xxx->send = 0/*false*/;
        if (status == eIO_Success) {
            if (code == 225/*Microsoft*/  ||  code == 226) {
                char buf[80];
                int n = sprintf(buf, "%" NCBI_BIGCOUNT_FORMAT_SPEC, xxx->size);
                assert(!BUF_Size(xxx->rbuf)  &&  n);
                if (!BUF_Write(&xxx->rbuf, buf, (size_t) n))
                    status = eIO_Unknown;
                xxx->rest = 0;
            } else
                status = eIO_Unknown;
        }
    }
    xxx->r_status = status;
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
                                     EIO_Event       direction);
    static EIO_Status  s_VT_Close   (CONNECTOR       connector,
                                     const STimeout* timeout);
    static void        s_Setup      (CONNECTOR       connector);
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
    unsigned short ntry = 0;
    EIO_Status status;

    assert(!xxx->what  &&  !xxx->data  &&  !xxx->cntl);
    do {
        assert(!BUF_Size(xxx->wbuf)  &&  !BUF_Size(xxx->rbuf));
        status = SOCK_CreateEx(xxx->info->host, xxx->info->port, timeout,
                               &xxx->cntl, 0, 0, fSOCK_KeepAlive
                               | (xxx->flag & fFTP_LogControl
                                  ? fSOCK_LogOn : fSOCK_LogDefault));
        if (status == eIO_Success) {
            SOCK_DisableOSSendDelay(xxx->cntl, 1/*yes,disable*/);
            SOCK_SetTimeout(xxx->cntl, eIO_ReadWrite, timeout);
            status  = x_FTPLogin(xxx);
        } else
            assert(!xxx->cntl);
        if (status == eIO_Success)
            status  = x_FTPBinary(xxx);
        if (status == eIO_Success  &&  *xxx->info->path)
            status  = x_FTPDir(xxx, 0,  xxx->info->path);
        if (status == eIO_Success) {
            xxx->send = xxx->open = xxx->rclr = 0/*false*/;
            assert(xxx->sync);
            xxx->rest = 0;
            break;
        }
        if (xxx->cntl) {
            SOCK_Abort(xxx->cntl);
            SOCK_Close(xxx->cntl);
            xxx->cntl = 0;
        }
    } while (++ntry < xxx->info->max_try);
    assert(!xxx->what  &&  !xxx->data);
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

    if (xxx->send) {
        if (xxx->data) {
            assert(xxx->open);
            if (event == eIO_Read)
                return s_FTPCompleteUpload(xxx, timeout);
            return SOCK_Wait(xxx->data, eIO_Write, timeout);
        }
        if (event == eIO_Write  ||  !xxx->open)
            return eIO_Closed;
        return SOCK_Wait(xxx->cntl, eIO_Read, timeout);
    }
    if (event == eIO_Write)
        return eIO_Success;
    /* event == eIO_Read */
    if (!xxx->data) {
        EIO_Status status;
        if (!BUF_Size(xxx->wbuf))
            return BUF_Size(xxx->rbuf) ? eIO_Success : eIO_Closed;
        status = SOCK_Wait(xxx->cntl, eIO_Write, timeout);
        if (status != eIO_Success)
            return status;
        status = s_FTPExecute(xxx, timeout);
        if (status != eIO_Success)
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

    if (!xxx->cntl)
        return eIO_Closed;

    if (xxx->send) {
        if (!xxx->data)
            return eIO_Closed;
        status = s_FTPPollCntl(xxx, timeout);
        if (status == eIO_Success) {
            SOCK_SetTimeout(xxx->data, eIO_Write, timeout);
            status = SOCK_Write(xxx->data, buf, size,n_written,eIO_WritePlain);
            xxx->size += *n_written;
            if (status == eIO_Closed) {
                CORE_LOGF_X(6, eLOG_Error,
                            ("[FTP; %s]  Data connection lost", xxx->what));
                x_FTPCloseData(xxx, eIO_Close/*silent close*/, 0);
            }
        }
    } else if (size) {
        size_t count;
        const char* run = (const char*) memchr((const char*) buf, '\n', size);
        *n_written = size; /* by default report the entire command consumed */
        if (run  &&  run < (const char*) buf + --size) {
            /* reject multiple commands */
            BUF_Erase(xxx->wbuf);
            return eIO_Unknown;
        }
        count = 0;
        if (xxx->flag & fFTP_UncleanIAC) {
            if (BUF_Write(&xxx->wbuf, buf, size))
                count = size;
        } else {
            static const char kIAC[] = { '\377'/*IAC*/, '\377' };
            const char* s = (const char*) buf;
            while (count < size) {
                /* Escaped IAC (Interpret As Command) character, per RFC854 */
                const char* p;
                size_t part;
                if (count) {
                    if (!BUF_Write(&xxx->wbuf, kIAC, sizeof(kIAC)))
                        break;
                    ++count;
                    ++s;
                }
                if (!(p = (const char*) memchr(s, kIAC[0], size - count)))
                    part = size - count;
                else
                    part = (size_t)(p - s);
                if (!BUF_Write(&xxx->wbuf, s, part))
                    break;
                count += part;
                s     += part;
            }
        }
        if (count < size) {
            /* short write */
            *n_written = count;
            status = eIO_Unknown;
        } else if (!run) {
            status = eIO_Success;
        } else
            return s_FTPExecute(xxx, timeout);
    } else
        status = eIO_Success;
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

    if (xxx->send)
        return xxx->open ? eIO_Success : eIO_Closed;
    if (!BUF_Size(xxx->wbuf))
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
        if (!xxx->open) {
            assert(!xxx->data);
            xxx->send = 0/*false*/;
            assert(!BUF_Size(xxx->wbuf));
            assert(!BUF_Size(xxx->rbuf));
            return eIO_Closed;
        }
        status = s_FTPCompleteUpload(xxx, timeout);
        if (status != eIO_Success)
            return status;
        assert(!xxx->data  &&  !xxx->send);
    } else if (BUF_Size(xxx->wbuf)) {
        status = s_FTPExecute(xxx, timeout);
        if (status != eIO_Success)
            return status;
    } else
        status = eIO_Success;
    if (xxx->data) {
        assert(!xxx->send  &&  !BUF_Size(xxx->rbuf));
        /* NB: Cannot use s_FTPPollCntl() here because a response about data
         * connection closure may be seen before the actual EOF in the
         * (heavily loaded) data connection. */
        SOCK_SetTimeout(xxx->data, eIO_Read, timeout);
        status = SOCK_Read(xxx->data, buf, size, n_read, eIO_ReadPlain);
        if (status == eIO_Closed) {
            status  = x_FTPCloseData(xxx, xxx->flag & fFTP_NoSizeChecks
                                     ? eIO_ReadWrite : eIO_Read, timeout);
            if (status == eIO_Success) {
                status  = s_FTPReply(xxx, &code, 0, 0, 0);
                if (status == eIO_Success) {
                    if (code == 225/*Microsoft*/  ||  code == 226) {
                        status = eIO_Closed;
                        xxx->rest = 0;
                    } else
                        status = eIO_Unknown;
                }
            }
        }
        xxx->r_status = status;
        return status;
    }
    if (size  &&  !(*n_read = BUF_Read(xxx->rbuf, buf, size)))
        status = eIO_Closed;
    return status;
}


static EIO_Status s_VT_Status
(CONNECTOR connector,
 EIO_Event direction)
{
    SFTPConnector* xxx = (SFTPConnector*) connector->handle;

    switch (direction) {
    case eIO_Read:
        return !xxx->cntl ? eIO_Closed : xxx->r_status;
    case eIO_Write:
        return !xxx->cntl ? eIO_Closed : xxx->w_status;
    default:
        assert(0); /* should never happen (verified by connection) */
        break;
    }
    return eIO_InvalidArg;
}


static EIO_Status s_VT_Close
(CONNECTOR       connector,
 const STimeout* timeout)
{
    SFTPConnector* xxx  = (SFTPConnector*) connector->handle;
    SOCK           data = xxx->data;
    EIO_Status     status;

    BUF_Erase(xxx->wbuf);
    BUF_Erase(xxx->rbuf);
    if (data) {
        EIO_Event how;
        assert(!xxx->send  ||  xxx->open);
        if (xxx->cntl  &&  !(xxx->r_status | xxx->w_status)  &&  xxx->send)
            how = eIO_Open/*warning close*/;
        else
            how = eIO_Close/*silent close*/;
        status = x_FTPCloseData(xxx, how, 0);
        if (status == eIO_Success  &&  how == eIO_Open)
            status  = eIO_Unknown;
    } else
        status = eIO_Success;
    assert(!xxx->data);
    if (xxx->what) {
        free((void*) xxx->what);
        xxx->what = 0;
    }
    if (xxx->cntl) {
        if (!data  &&  status == eIO_Success) {
            int code = 0/*any*/;
            if (!timeout)
                timeout = &kFailsafeTimeout;
            SOCK_SetTimeout(xxx->cntl, eIO_ReadWrite, timeout);
            /* all implementations MUST support QUIT */
            status = s_FTPCommand(xxx, "QUIT", 0);
            if (status == eIO_Success) {
                status  = s_FTPDrainReply(xxx, &code, code);
                if (status == eIO_Success)
                    status  = eIO_Unknown;
                if (status == eIO_Closed  &&  code == 221)
                    status  = eIO_Success;
            }
        }
    } else
        status = eIO_Closed;
    if (xxx->cntl) {
        SOCK_Abort(xxx->cntl);
        SOCK_Close(xxx->cntl);
        xxx->cntl = 0;
    }
    return status;
}


static void s_Setup
(CONNECTOR connector)
{
    SMetaConnector* meta = connector->meta;

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

    ConnNetInfo_Destroy(xxx->info);
    assert(!xxx->what  &&  !xxx->cntl  &&  !xxx->data);
    assert(!BUF_Size(xxx->wbuf)  &&  !BUF_Size(xxx->rbuf));
    BUF_Destroy(xxx->wbuf);
    xxx->wbuf = 0;
    BUF_Destroy(xxx->rbuf);
    xxx->rbuf = 0;
    free(xxx);
    free(connector);
}


extern CONNECTOR s_CreateConnector(const SConnNetInfo*  info,
                                   const char*          host,
                                   unsigned short       port,
                                   const char*          user,
                                   const char*          pass,
                                   const char*          path,
                                   TFTP_Flags           flag,
                                   const SFTP_Callback* cmcb)
{
    static const SFTP_Callback kNoCmcb = { 0 };
    CONNECTOR      ccc;
    SFTPConnector* xxx;

    if ((host  &&  strlen(host) >= sizeof(xxx->info->host))  ||
        (user  &&  strlen(user) >= sizeof(xxx->info->user))  ||
        (pass  &&  strlen(pass) >= sizeof(xxx->info->pass))  ||
        (path  &&  strlen(path) >= sizeof(xxx->info->path))  ||
        (info  &&  info->scheme != eURL_Unspec  &&  info->scheme != eURL_Ftp)){
        return 0;
    }
    if (!(ccc = (SConnector*) malloc(sizeof(SConnector))))
        return 0;
    if (!(xxx = (SFTPConnector*) malloc(sizeof(*xxx)))) {
        free(ccc);
        return 0;
    }
    xxx->info = info ? ConnNetInfo_Clone(info) : ConnNetInfo_Create("_FTP");
    if (!xxx->info) {
        free(ccc);
        free(xxx);
        return 0;
    }
    if (xxx->info->scheme == eURL_Unspec)
        xxx->info->scheme  = eURL_Ftp;
    if (host  &&  *host)
        strcpy(xxx->info->host, host);
    xxx->info->port = port ? port : CONN_PORT_FTP;
    strcpy(xxx->info->user, user  &&  *user ? user : "ftp");
    strcpy(xxx->info->pass, pass            ? pass : "-none");
    strcpy(xxx->info->path, path            ? path : "");
    *xxx->info->args = '\0';

    /* some uninited fields are taken care of in s_VT_Open */
    xxx->cmcb    = cmcb ? *cmcb : kNoCmcb;
    xxx->flag    = flag;
    xxx->what    = 0;
    xxx->cntl    = 0;
    xxx->data    = 0;
    xxx->wbuf    = 0;
    xxx->rbuf    = 0;

    /* initialize connector data */
    ccc->handle  = xxx;
    ccc->next    = 0;
    ccc->meta    = 0;
    ccc->setup   = s_Setup;
    ccc->destroy = s_Destroy;

    return ccc;
}


/***********************************************************************
 *  EXTERNAL -- the connector's "constructors"
 ***********************************************************************/

extern CONNECTOR FTP_CreateConnectorSimple(const char*          host,
                                           unsigned short       port,
                                           const char*          user,
                                           const char*          pass,
                                           const char*          path,
                                           TFTP_Flags           flag,
                                           const SFTP_Callback* cmcb)
{
    return s_CreateConnector(0,    host, port, user, pass, path, flag, cmcb);
}


extern CONNECTOR FTP_CreateConnector(const SConnNetInfo*  info,
                                     TFTP_Flags           flag,
                                     const SFTP_Callback* cmcb)
{
    return s_CreateConnector(info, 0,    0,    0,    0,    0,    flag, cmcb);
}


/*DEPRECATED*/
extern CONNECTOR FTP_CreateDownloadConnector(const char*    host,
                                             unsigned short port,
                                             const char*    user,
                                             const char*    pass,
                                             const char*    path,
                                             TFTP_Flags     flag)
{
    return s_CreateConnector(0,    host, port, user, pass, path, flag, 0);
}
