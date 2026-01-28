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
 *   See <connect/ncbi_connector.h> for the detailed specification of the
 *   connector's methods and structures.
 *
 *   Note:  We do not implement transfers of files whose names include CR or LF
 *          character(s):  for those to work, all FTP commands will have to be
 *          required to terminate with CRLF at the user level (currently, LF
 *          alone acts as the command terminator), and all solitary CRs to be
 *          recoded as 'CR\0' (per the RFC), yet all solitary LFs to be passed
 *          through.  Nonetheless, we escape all IACs for the sake of safety of
 *          the control connection.
 *
 */

#include "ncbi_ansi_ext.h"
#include "ncbi_priv.h"
#include "ncbi_servicep.h"
#include "ncbi_socketp.h"
#include <connect/ncbi_ftp_connector.h>
#include <ctype.h>
#include <stdlib.h>

#define NCBI_USE_ERRCODE_X   Connect_FTP


/***********************************************************************
 *  INTERNAL -- Auxiliary types and static functions
 ***********************************************************************/

/* Per RFC2428, 2 (IANA assigned protocol numbers) */
#define FTP_EPRT_PROTO_IPv4     1  /* Dotted decimal: 130.14.29.110          */
#define FTP_EPRT_PROTO_IPv6     2  /* IPv6 string:    2607:f220:41e:4290::110*/


enum EFTP_Feature {                /* NB: values must occupy 12 bits at most */
    fFtpFeature_NOOP =  0x001,     /* all implementations MUST support RFC959*/
    fFtpFeature_SYST =  0x002,     /* RFC 959                                */
    fFtpFeature_SITE =  0x004,     /* RFC 959, 1123                          */
    fFtpFeature_FEAT =  0x008,     /* RFC 3659                               */
    fFtpFeature_MDTM =  0x010,     /* RFC 3659                               */
    fFtpFeature_SIZE =  0x020,     /* RFC 3659                               */
    fFtpFeature_REST =  0x040,     /* RFC 3659, NB: FEAT                     */
    fFtpFeature_MLSx =  0x080,     /* RFC 3659                               */
    fFtpFeature_EPRT =  0x100,     /* RFC 2428                               */
    fFtpFeature_MFF  =  0x200,     /* draft-somers-ftp-mfxx-04, NB: FEAT     */
    fFtpFeature_EPSV = 0x1000,     /* RFC 2428                               */
    fFtpFeature_APSV = 0x3000      /* EPSV ALL -- a la "APSV" from RFC 1579  */
};
typedef unsigned short TFTP_Features; /* bitwise OR of EFtpFeature */


/* All internal data necessary to perform I/O
 */
typedef struct {
    const SConnNetInfo*   info;    /* connection parameters                  */
    unsigned              sync:1;  /* true when last cmd acked (cntl synced) */
    unsigned              send:1;  /* true when in send mode (STOR/APPE)     */
    unsigned              open:1;  /* true when data opened ok in send mode  */
    unsigned              rclr:1;  /* true when "rest" to clear by next cmd  */
    unsigned              abor:1;  /* last cmd was ABOR (proftpd bug w/450)  */
    unsigned              free:1;  /* unused                                 */
    unsigned              soft:10; /* learned server features (future ext)   */
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
static const char     kDigits[]        = "0123456789";


/* FTP server reply parsing callback:
 * @param code
 *   FTP server code as received or 0 for when this is the last line
 *   of a multiline reply (a single-line reply always has a non-0 code)
 * @param lineno
 *   Line number, 0-based (line 0 is always supplied with a non-zero code)
 * @param line
 *   Current reply text line (with a reply code, if any was there, along
 *   with all the following white space, stripped)
 * @return
 *   eIO_Success to keep processing the reply;  or an error status
 *   to be returned to the upper level (reply processing continues
 *   without any further callbacks for this reply)
 */
typedef EIO_Status (*FFTPReplyCB)(SFTPConnector* xxx,  int   code,
                                  size_t lineno, const char* line);


/* Close data connection w/error messages
 * how == eIO_Open                        -- unexpected closure, log error
 * how == eIO_Read or eIO_Write (or both) -- normal close in the dir w/check,
 *                                           or no size ck with eIO_ReadWrite
 * how == eIO_Close                       -- pre-approved close, w/o err logs
 * Notes:
 * 1. eIO_Open and eIO_Close suppress log message if xxx->cntl is closed;
 * 2. arg is a description (eIO_Open), ignored (eIO_Close), or a timeout;
 * 3. post-condition: !xxx->data.
 */
static EIO_Status x_FTPCloseData(SFTPConnector* xxx,
                                 EIO_Event      how,
                                 const void*    arg)
{
    EIO_Status status;

    assert(xxx->data);
    if (xxx->flag & fFTP_LogControl)
        SOCK_SetDataLogging(xxx->data, eOn);

    if (how & eIO_ReadWrite) {
        const STimeout* timeout = (const STimeout*) arg;
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
                static const struct {
                    const int   err;
                    const char* fmt;
                } kWarning[] = {
                    { 13,
                      "[FTP; %s]  Server reports restarted download"
                      " size incorrectly: %" NCBI_BIGCOUNT_FORMAT_SPEC },
                    { 14,
                      "[FTP; %s]  Restart parse error prevents download"
                      " size verification: %" NCBI_BIGCOUNT_FORMAT_SPEC }
                };
                const int   err = kWarning[!(xxx->rest == (TNCBI_BigCount)(-1L))].err;
                const char* fmt = kWarning[!(xxx->rest == (TNCBI_BigCount)(-1L))].fmt;
                CORE_LOGF_X(err, eLOG_Warning,
                            (fmt, xxx->what, xxx->size));
            } else {
                CORE_LOGF_X(8, eLOG_Error,
                            ("[FTP; %s]  Premature EOF in data: received "
                             "%" NCBI_BIGCOUNT_FORMAT_SPEC " vs. "
                             "%" NCBI_BIGCOUNT_FORMAT_SPEC " expected, ",
                             xxx->what, size, xxx->size));
                status = eIO_Unknown;
            }
        } else if (size  &&  how != eIO_ReadWrite) {
            CORE_TRACEF(("[FTP; %s]  Transfer size verified ("
                         "%" NCBI_BIGCOUNT_FORMAT_SPEC " byte%s)",
                         xxx->what, size, &"s"[size == 1]));
        }
    } else {
        if (!xxx->cntl) {
            how = eIO_Open;
        } else if (how != eIO_Close) {
            const char* text = (const char*) arg;
            if (text  &&  !*text)
                text = 0;
            CORE_LOGF_X(1, xxx->send ? eLOG_Error : eLOG_Warning,
                        ("[FTP%s%s]  Data connection transfer aborted%s%s%s",
                         xxx->what ? ": " : "", xxx->what ? xxx->what : "",
                         text ? " (" : "", text ? text : "", &")"[!text]));
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


static void x_FTPCloseCntl(SFTPConnector* xxx, const char* drop)
{
    SOCK cntl = xxx->cntl;
    xxx->cntl = 0;
    if (drop) {
        CORE_LOGF_X(10, eLOG_Error,
                    ("[FTP%s%s]  Lost connection to %s:%hu (%s)",
                     xxx->what ? "; " : "", xxx->what ? xxx->what : "",
                     xxx->info->host, xxx->info->port, drop));
    }
    if (xxx->data)
        x_FTPCloseData(xxx, eIO_Close/*silent close*/, 0);
    if (drop)
        SOCK_Abort(cntl);
    else
        SOCK_SetTimeout(cntl, eIO_Close, &kZeroTimeout);
    SOCK_Close(cntl);
}


static EIO_Status x_FTPParseReply(SFTPConnector* xxx, int* code,
                                  char* line, size_t maxlinelen,
                                  FFTPReplyCB replycb)
{
    EIO_Status status = eIO_Success;
    size_t     lineno;
    size_t     len;

    assert(xxx->cntl  &&  code);

    for (lineno = 0;  ;  ++lineno) {
        EIO_Status rdstat;
        const char* msg;
        char buf[1024];
        int c, n;

        /* all FTP replies are at least '\n'-terminated, not ending with EOF */
        rdstat = SOCK_ReadLine(xxx->cntl, buf, sizeof(buf), &len);
        if (rdstat != eIO_Success) {
            status = SOCK_Status(xxx->cntl, eIO_Read) == eIO_Closed
                ? eIO_Closed : rdstat;
            break;
        }
        if (len == sizeof(buf)) {
            status = eIO_NotSupported/*line too long*/;
            break;
        }
        if (!lineno  ||  strchr(kDigits, (unsigned char)(*buf))) {
            if (sscanf(buf, "%d%n", &c, &n) < 1  ||  n != 3  ||  c < 100
                ||  (buf[3]  &&  buf[3] != ' '  &&  buf[3] != '-')
                ||  (lineno  &&  c != *code)) {
                status = eIO_Unknown;
                break;
            }
            msg = buf + 4/*n + 1*/;
            if (buf[3] == '-')
                n = 0;
        } else {
            msg = buf;
            c = *code;
            n = 0;
        }
        msg += strspn(msg, " \t");
        if (status == eIO_Success  &&  replycb  &&  !(c == 450  &&  xxx->abor))
            status  = replycb(xxx, !lineno  ||  !n ? c : 0, lineno, msg);
        if (!lineno) {
            *code = c;
            if (line)
                strncpy0(line, msg, maxlinelen);
        }
        if (n/*end of reply*/)
            break;
    }
    if (*code == 450  &&  xxx->abor) {
        xxx->abor = 0/*false*/;
        /* http://bugs.proftpd.org/show_bug.cgi?id=4252 */
        status = x_FTPParseReply(xxx, code, line, maxlinelen, replycb);
    }
    return status;
}


static EIO_Status s_FTPReply(SFTPConnector* xxx, int* code,
                             char* line, size_t maxlinelen,
                             FFTPReplyCB replycb)
{
    EIO_Status status;
    int        c = 0;

    if (xxx->cntl) {
        char reason[40];
        status = x_FTPParseReply(xxx, &c, line, maxlinelen, replycb);
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
            sprintf(reason, "code %d", c);
            CORE_TRACEF(("Got FTP %s", reason));
        } else
            strncpy0(reason, IO_StatusStr(status), sizeof(reason) - 1);
        if (status == eIO_Closed   ||  c == 221)
            x_FTPCloseCntl(xxx, status == eIO_Closed ? reason : 0);
        if (status == eIO_Success  &&  c == 530/*not logged in*/)
            status  = eIO_Closed;
    } else
        status = eIO_Closed;
    if (code)
        *code = c;
    return status;
}


/* Read replies while successful until the received response is: ether == *code
 * or its hundred's place == cXX;  return the response code via the pointer. */
static EIO_Status x_FTPDrainReply(SFTPConnector* xxx, int* code, int cXX)
{
    int        c;
    EIO_Status status;
    int        quit = *code;
    *code = 0;
    while ((status = s_FTPReply(xxx, &c, 0, 0, 0)) == eIO_Success) {
        if (c == 450  &&  xxx->abor) {
            xxx->abor = 0/*false*/;
            continue;
        }
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
    char       x_buf[256];
    size_t     cmdlen, arglen, linelen;

    if (!xxx->cntl)
        return eIO_Closed;

    cmdlen  = strlen(cmd);
    linelen = cmdlen + 2;
    if (arg) {
        arglen = *arg ? strlen(arg) : 0;
        linelen += 1 + arglen;
    } else
        arglen = 0/*unused*/;
    line = linelen < sizeof(x_buf) ? x_buf : (char*) malloc(linelen + 1);

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
        assert(cmdlen == linelen);
        log = off ? SOCK_SetDataLogging(xxx->cntl, eOff) : eOff;
        status = SOCK_Write(xxx->cntl, line, linelen, 0, eIO_WritePersist);
        if (off  &&  log != eOff) {
            SOCK_SetDataLogging(xxx->cntl, log);
            if (log == eOn  ||  SOCK_SetDataLoggingAPI(eDefault) == eOn) {
                CORE_LOGF_X(4, eLOG_Trace,
                            ("FTP %.*s command sent: %s",
                             (int) strcspn(line, " \t"), line,
                             IO_StatusStr(status)));
            }
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
    return s  &&  ((s == line  ||  isspace((unsigned char) s[-1]))
                   &&  !isalpha((unsigned char) s[4])) ? s : 0;
}


static EIO_Status x_FTPHelpCB(SFTPConnector* xxx, int code,
                              size_t lineno, const char* line)
{
    if (!lineno)
        return code == 211  ||  code == 214 ? eIO_Success : eIO_NotSupported;
    if (code/*not last line*/) {
        const char* s;
        assert(code == 211  ||  code == 214);
        if ((s = x_4Word(line, "NOOP")) != 0) {  /* RFC 959 */
            if (s[4 + strspn(s + 4, " \t")] != '*')
                xxx->feat |=                  fFtpFeature_NOOP;
            else
                xxx->feat &= (TFTP_Features)(~fFtpFeature_NOOP);
        }
        if ((s = x_4Word(line, "SYST")) != 0) {  /* RFC 959 */
            if (s[4 + strspn(s + 4, " \t")] != '*')
                xxx->feat |=                  fFtpFeature_SYST;
            else
                xxx->feat &= (TFTP_Features)(~fFtpFeature_SYST);
        }
        if ((s = x_4Word(line, "SITE")) != 0) {  /* RFC 959, 1123 */
            if (s[4 + strspn(s + 4, " \t")] != '*')
                xxx->feat |=                  fFtpFeature_SITE;
            else
                xxx->feat &= (TFTP_Features)(~fFtpFeature_SITE);
        }
        if ((s = x_4Word(line, "FEAT")) != 0) {  /* RFC 3659 */
            if (s[4 + strspn(s + 4, " \t")] != '*')
                xxx->feat |=                  fFtpFeature_FEAT;
            else
                xxx->feat &= (TFTP_Features)(~fFtpFeature_FEAT);
        }
        if ((s = x_4Word(line, "MDTM")) != 0) {  /* RFC 3659 */
            if (s[4 + strspn(s + 4, " \t")] != '*')
                xxx->feat |=                  fFtpFeature_MDTM;
            else
                xxx->feat &= (TFTP_Features)(~fFtpFeature_MDTM);
        }
        if ((s = x_4Word(line, "SIZE")) != 0) {  /* RFC 3659 */
            if (s[4 + strspn(s + 4, " \t")] != '*')
                xxx->feat |=                  fFtpFeature_SIZE;
            else
                xxx->feat &= (TFTP_Features)(~fFtpFeature_SIZE);
        }
        if ((s = x_4Word(line, "REST")) != 0) {  /* RFC 3659, NB: FEAT */
            if (s[4 + strspn(s + 4, " \t")] != '*')
                xxx->feat |=                  fFtpFeature_REST;
            else
                xxx->feat &= (TFTP_Features)(~fFtpFeature_REST);
        }
        if ((s = x_4Word(line, "MLST")) != 0) {  /* RFC 3659 */
            if (s[4 + strspn(s + 4, " \t")] != '*')
                xxx->feat |=                  fFtpFeature_MLSx;
            else
                xxx->feat &= (TFTP_Features)(~fFtpFeature_MLSx);
        }
        if ((s = x_4Word(line, "MLSD")) != 0) {  /* RFC 3659 */
            if (s[4 + strspn(s + 4, " \t")] != '*')
                xxx->feat |=                  fFtpFeature_MLSx;
            else
                xxx->feat &= (TFTP_Features)(~fFtpFeature_MLSx);
        }
        if ((s = x_4Word(line, "EPRT")) != 0) {  /* RFC 2428 */
            if (s[4 + strspn(s + 4, " \t")] != '*')
                xxx->feat |=                  fFtpFeature_EPRT;
            else
                xxx->feat &= (TFTP_Features)(~fFtpFeature_EPRT);
        }
        if ((s = x_4Word(line, "EPSV")) != 0) {  /* RFC 2428 (cf RFC 1579) */
            if (s[4 + strspn(s + 4, " \t")] != '*')
                xxx->feat |=                  fFtpFeature_EPSV;
            else
                xxx->feat &= (TFTP_Features)(~fFtpFeature_EPSV);
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
    xxx->feat = 0;
    status = s_FTPReply(xxx, &code, 0, 0, x_FTPHelpCB);
    if (status != eIO_Success  ||  (code != 211  &&  code != 214)) {
        xxx->feat = feat;
        return status != eIO_Success ? status : eIO_NotSupported;
    }
    CORE_TRACEF(("FTP features parsed from HELP: 0x%04hX", xxx->feat));
    xxx->feat |= feat;
    return eIO_Success;
}


static EIO_Status x_FTPFeatCB(SFTPConnector* xxx, int code,
                              size_t lineno, const char* line)
{
    if (!lineno)
        return code == 211 ? eIO_Success : eIO_NotSupported;
    if (code  &&  strlen(line) >= 4
        &&  (!line[4]
             ||  isspace((unsigned char) line[3])
             ||  isspace((unsigned char) line[4]))) {
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
        else if (strncasecmp(line, "MFCT", 4) == 0)
            xxx->feat |= fFtpFeature_MFF;   /* fFtpFeature_MFF not yet supp */
        else if (strncasecmp(line, "MFMT", 4) == 0)
            xxx->feat |= fFtpFeature_MFF;   /*   draft-somers-ftp-mfxx-04   */
        else if (strncasecmp(line, "MFF ", 4) == 0)
            xxx->feat |= fFtpFeature_MFF;   /* NB: modify facts must follow */
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
    xxx->feat = 0;
    status = s_FTPReply(xxx, &code, 0, 0, x_FTPFeatCB);
    if (status != eIO_Success  ||  code != 211) {
        xxx->feat = feat;
        return status != eIO_Success ? status : eIO_NotSupported;
    }
    CORE_TRACEF(("FTP features parsed from FEAT: 0x%04hX", xxx->feat));
    xxx->feat |= feat;
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
        /* try to setup features, ignore most errors */
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
    if (code != 220)
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
        status = s_FTPCommandEx(xxx, "PASS", xxx->info->pass, 1/*off*/);
        if (status != eIO_Success)
            return status;
        status = s_FTPReply(xxx, &code, 0, 0, 0);
        if (status != eIO_Success)
            return status;
        if (code != 230  &&  code != 202)
            return code == 332 ? eIO_NotSupported : eIO_Unknown/*incl 503*/;
    }
    status = x_FTPFeatures(xxx);
    if (status != eIO_Success)
        return status;
    if (xxx->flag & fFTP_LogControl) {
        CORE_LOGF_X(3, eLOG_Trace,
                    ("[FTP; %s:%hu]  Server ready, features = 0x%02X",
                     xxx->info->host, xxx->info->port, xxx->feat));
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
        return !out  ||  BUF_Write(&xxx->rbuf, "350", 3)
            ? eIO_Success : eIO_Unknown;
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


/* (null[internal]), MKD, RMD, PWD, CWD, CDUP, XMKD, XRMD, XPWD, XCWD, XCUP */
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
    status = s_FTPCommand(xxx, cmd ? cmd : kCwd, cmd ? 0/*arg embedded*/ : arg);
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &code, buf, sizeof(buf) - 1, 0);
    if (status != eIO_Success
        &&  (status != eIO_NotSupported  ||  (code != 500  &&  code != 502))) {
        return status;
    }
    if (code == 500  ||  code == 502) {
        /* RFC1123 4.1.3.1 requires to reissue the command using an X-form */
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
    } else if (!cmd) {
        cmd = kCwd;
    } else if (toupper((unsigned char)(*cmd)) == 'X')
        cmd++;
    if (toupper((unsigned char)(*cmd)) == 'R') { /* [X]RMD */
        if (code != 250)
            return eIO_Unknown;
    } else if (toupper((unsigned char)(*cmd)) == 'C') { /* [X]CWD, CDUP/XCUP */
        if (code != 200  &&  code != 250)
            return eIO_Unknown;
        /* fix up codes in accordance with RFC959 */
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

    if (cmd == kCwd)
        return eIO_Success;
    code = sprintf(buf, "%d", code);
    assert(0 < code  &&  (size_t) code < sizeof(buf));
    return !BUF_Write(&xxx->rbuf, buf, (size_t) code)
        ? eIO_Unknown : eIO_Success;
}


/* Worthwhile reading:
 * https://datatracker.ietf.org/doc/html/rfc793#section-3.1
 * https://datatracker.ietf.org/doc/html/rfc1122#page-84
 * https://datatracker.ietf.org/doc/html/draft-ietf-tcpm-urgent-data-05
 * https://datatracker.ietf.org/doc/html/rfc854
 * https://www.freesoft.org/CIE/RFC/854/5a.htm
 * http://www.ccplusplus.com/2011/09/tcp-urgent-pointer.html
 * Discussion:  The Urgent Pointer can be "seen" as both before
 * and after the "DM" byte, depending on the implementation's
 * way of generating the pointer value, which then can make either
 * the IAC or the DM byte interpreted as the end of urgent data.
 * However, per the TELNET Synch discussion (both RFC764 and RFC854):
   If TCP indicates the end of Urgent data before the DM is found,
   TELNET should continue the special handling of the data stream
   until the DM is found.
  */
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


static void x_StoreTimeoutNormalized(STimeout* dst, const STimeout* src)
{
    dst->sec  = src->sec;
    dst->sec += src->usec / 1000000;
    dst->usec = src->usec % 1000000;
}


/* Return "true" (non-zero) if "p1" is greater than a finite "p2" */
static int/*bool*/ x_IsLongerTimeout(const STimeout* p1,
                                     const STimeout* p2)
{
    STimeout t1, t2;
    assert(p2);
    if (!p1/*==kInfiniteTimeout*/)
        return 1/*T*/;
    x_StoreTimeoutNormalized(&t1, p1);
    x_StoreTimeoutNormalized(&t2, p2);
    return   t1.sec  > t2.sec
        ||  (t1.sec == t2.sec  &&  t1.usec > t2.usec) ? 1/*T*/ : 0/*F*/;
}


static EIO_Status x_DrainData(SFTPConnector* xxx, const STimeout* timeout)
{
    EIO_Status status;
    struct timeval deadline;

    gettimeofday(&deadline, 0);
    /* this is not "data" per se, so go silent */
    ESwitch log = xxx->flag & fFTP_LogData
        ? SOCK_SetDataLogging(xxx->data, eDefault) : eDefault;
    const STimeout* r_timeout = SOCK_GetTimeout(xxx->data, eIO_Read);
    if (x_IsLongerTimeout(r_timeout, timeout))
        SOCK_SetTimeout(xxx->data, eIO_Read, timeout);
    else
        timeout = r_timeout;
    deadline.tv_usec += timeout->usec;
    deadline.tv_sec  += timeout->sec + deadline.tv_usec / 10000000;
    deadline.tv_usec %= 10000000;

    /* drain up data connection by discarding (up to) 1MB blocks repeatedly */
    while ((status = SOCK_Read(xxx->data, 0, 1 << 20, 0, eIO_ReadPlain))
           == eIO_Success) {
        STimeout remain;
        struct timeval now;
        gettimeofday(&now, 0);
        if (now.tv_sec < deadline.tv_sec)
            continue;
        if (now.tv_sec > deadline.tv_sec)
            break;
        if (now.tv_usec >= deadline.tv_usec)
            break;
        remain.sec = deadline.tv_sec - now.tv_sec;
        if (deadline.tv_usec < now.tv_usec) {
            assert(remain.sec);
            --remain.sec;
            remain.usec = deadline.tv_usec + 1000000 - now.tv_usec;
        } else
            remain.usec = deadline.tv_usec - now.tv_usec;
        if (x_IsLongerTimeout(timeout, &remain))
            SOCK_SetTimeout(xxx->data, eIO_Read, &remain);
    }

    if (log != eDefault)
        SOCK_SetDataLogging(xxx->data, log);
    return status;
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
    if (x_IsLongerTimeout(timeout, &kFailsafeTimeout))
        timeout = &kFailsafeTimeout;
    else
        assert(timeout); /* finite small timeout */
    SOCK_SetTimeout(xxx->cntl, eIO_ReadWrite, timeout);
    status = x_FTPTelnetSynch(xxx);
    if (status == eIO_Success)
        status  = s_FTPCommand(xxx, "ABOR", 0);
    if (xxx->data) {
        const char* reason = 0;
        EIO_Status abort = status;
        EIO_Event event = eIO_Open/*warning close*/;
        if (abort == eIO_Success  &&  !xxx->send) {
            if (x_DrainData(xxx, timeout) == eIO_Success)
                abort = eIO_Timeout;
        }
        if (how == 3)
            reason = "Abandoned";
        else if (abort != eIO_Success)
            reason = IO_StatusStr(abort);
        else if ((abort = SOCK_Status(xxx->data, eIO_Read)) != eIO_Closed)
            reason = IO_StatusStr(abort);
        else
            event = eIO_Close/*silent close*/;
        x_FTPCloseData(xxx, event, reason);
    }
    assert(!xxx->data);
    xxx->abor = 1/*true*/;
    if (status == eIO_Success) {
        int         code = 426;
        int/*bool*/ sync = xxx->sync;
        status = x_FTPDrainReply(xxx, &code, 2/*2xx*/);
        if (status == eIO_Success) {
            /* Microsoft FTP is known to return 225 instead of 226 */
            if (code != 225  &&  code != 226  &&  code != 426)
                status = eIO_Unknown;
        } else if (status == eIO_Timeout  &&  !code)
            sync = 0/*false*/;
        xxx->sync = sync ? 1 : 0;
    }
    return status;
}


static EIO_Status x_FTPEpsv(SFTPConnector*  xxx,
                            unsigned int*   host,
                            unsigned short* port)
{
    EIO_Status status;
    char buf[132], d;
    unsigned int p;
    const char* s;
    int c;

    assert(port  ||  (xxx->feat & fFtpFeature_APSV) == fFtpFeature_APSV);
    if (xxx->flag & fFTP_NoExtensions)
        return eIO_NotSupported;
    status = s_FTPCommand(xxx, "EPSV", port ? 0 : "ALL");
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &c, buf, sizeof(buf) - 1, 0);
    if (status != eIO_Success)
        return status;
    if (!port)
        return c == 200 ? eIO_Success : eIO_NotSupported;
    if (c != 229)
        return xxx->feat & fFtpFeature_APSV ? eIO_Unknown : eIO_NotSupported;
    if (!(s = strchr(buf, '('))  ||  !(d = *++s)  ||  *++s != d  ||  *++s != d
        ||  sscanf(++s, "%u%c%n", &p, buf, &c) < 2  ||  p > 0xFFFF
        ||  *buf != d  ||  s[c] != ')') {
        return eIO_Unknown;
    }
    *host = 0;
    *port = (unsigned short) p;
    return eIO_Success;
}


/* all implementations MUST support PASV */
/* NB: RFC1639 extends PASV for various protocol families with LPSV */
static EIO_Status x_FTPPasv(SFTPConnector*  xxx,
                            unsigned int*   host,
                            unsigned short* port)
{
    EIO_Status status;
    unsigned int i;
    int code, o[6];
    char buf[132];

    status = s_FTPCommand(xxx, "PASV", 0);
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &code, buf, sizeof(buf) - 1, 0);
    if (status != eIO_Success  ||  code != 227)
        return eIO_Unknown;
    for (;;) {
        char* c, q;
        /* RFC 1123 4.1.2.6 says that ()'s in PASV reply MUST NOT be assumed */
        if (!*(c = buf + strcspn(buf, kDigits)))
            return eIO_Unknown;
        q = c > buf ? c[-1] : '\0';
        for (i = 0;  i < (unsigned int)(sizeof(o) / sizeof(o[0]));  ++i) {
            if (sscanf(c, &",%d%n"[!i], &o[i], &code) < 1)
                break;
            assert(code > 0);
            c += code;
        }
        if (i >= (unsigned int)(sizeof(o) / sizeof(o[0]))) {
            if (!q  ||  q != '('  ||  *c == ')')
                break;
        } else if (!i)
            return eIO_Unknown;
        assert(c > buf);
        memmove(buf, c, strlen(c) + 1);
    }
    assert(i == (unsigned int)(sizeof(o) / sizeof(o[0])));
    while (i--) {
        if (o[i] < 0  ||  o[i] > 255)
            return eIO_Unknown;
    }
    if (!(i = (unsigned int)((((((o[0]<<8) | o[1])<<8) | o[2])<<8) | o[3])))
        return eIO_Unknown;
    if (i == (unsigned int)(-1))
        return eIO_Unknown;
    *host = SOCK_HostToNetLong(i);
    if (!(i = (unsigned int)((o[4]<<8) | o[5])))
        return eIO_Unknown;
    assert(i <= 0xFFFF);
    *port = (unsigned short) i;
    return eIO_Success;
}


static EIO_Status x_FTPPassive(SFTPConnector*  xxx,
                               const STimeout* timeout)
{
    EIO_Status   status;
    unsigned int   host;
    unsigned short port;
    char           addr[132];

    if ((xxx->feat & fFtpFeature_APSV) == fFtpFeature_APSV) {
        /* first time here, try to set EPSV ALL */
        if (x_FTPEpsv(xxx, 0, 0) == eIO_Success)
            xxx->feat &= (TFTP_Features)(~fFtpFeature_EPSV); /* APSV mode */
        else                                                 /* EPSV mode */
            xxx->feat &= (TFTP_Features)(~fFtpFeature_APSV | fFtpFeature_EPSV);
    }
    if (xxx->feat & fFtpFeature_APSV) {
        status = x_FTPEpsv(xxx, &host, &port);
        switch (status) {
        case eIO_NotSupported:
            xxx->feat &= (TFTP_Features)(~fFtpFeature_EPSV);
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
    char       buf[132];
    EIO_Status status;
    int        c;

    if (xxx->flag & fFTP_NoExtensions)
        return eIO_NotSupported;
    c = sprintf(buf, "|%d|", FTP_EPRT_PROTO_IPv4);
    SOCK_ntoa(host, buf + c, sizeof(buf) - c);
    sprintf((buf + c) + strlen(buf + c), "|%hu|", port);
    status = s_FTPCommand(xxx, "EPRT", buf);
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &c, 0, 0, 0);
    if (status != eIO_Success)
        return status;
    if (c == 500  ||  c == 501)
        return xxx->feat & fFtpFeature_EPRT ? eIO_Unknown : eIO_NotSupported;
    if (c == 522)
        return eIO_NotSupported; /*NB: parse reply to see what IP supported*/
    return c == 200 ? eIO_Success : eIO_Unknown;
}


/* all implementations MUST support PORT */
/* NB: RFC1639 extends PORT for various protocol families with LPRT */
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
    for (n = 0;  n < sizeof(octet);  ++n)
        s += sprintf(s, &",%u"[!n], octet[n]);
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

    /* NB: Apache FTP proxy re-uses SOCK_GetLocalPort(xxx->cntl);
     * which is the default port for user-end data port per RFC959 3.2,
     * other implementations don't do that leaving for OS to decide,
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
        xxx->feat &= (TFTP_Features)(~fFtpFeature_EPRT);
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
        if (!xxx->cntl)
            return status;
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
                            FFTPReplyCB     replycb)
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
        status  = s_FTPReply(xxx, &code, 0, 0, replycb);
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
                                 " at :%hu (%s)", xxx->what,
                                 LSOCK_GetPort(lsock, eNH_HostByteOrder),
                                 IO_StatusStr(status)));
                    /* NB: data conn may have started at the server end */
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
                        SOCK_SetCork(xxx->data, 1/*true*/);
                    assert(xxx->open);
                    xxx->size = 0;
                }
                xxx->sync = 0/*false*/;
                return eIO_Success;
            }
            code = 2/*full abort*/;
        } else {
            /* file processing errors: not a file, not a dir, etc */
            status = code == 450  ||  code == 550 ? eIO_Closed : eIO_Unknown;
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


/* [X]CWD, [X]PWD, [X]MKD, [X]RMD, CDUP, XCUP */
static EIO_Status s_FTPDir(SFTPConnector* xxx,
                           const char*    cmd,
                           const char*    arg)
{
    assert(cmd  &&  arg  &&  !BUF_Size(xxx->rbuf));
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


static EIO_Status x_FTPStatCB(SFTPConnector* xxx, int code,
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
    return s_FTPReply(xxx, 0, 0, 0, x_FTPStatCB);
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
#ifndef HAVE_TIMEGM
    time_t tzdiff;
#endif /*HAVE_TIMEGM*/
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
     * 1. Only GNU implementation is documented not to require spaces between
     *    input format specifiers in the format string (%-based);
     * 2. None to any spaces are allowed to match a space in the format, whilst
     *    an MDTM response must not contain even a single space. */
    for (n = 0;  n < (int)(sizeof(field)/sizeof(field[0]));  ++n) {
        len = n ? 2 : 4/*year*/;
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
    tm.tm_year = field[0] - 1900;
    if (field[1] < 1  ||  field[1] > 12)
        return eIO_Unknown;
    tm.tm_mon  = field[1] - 1;
    if (field[2] < 1  ||  field[2] > (field[1] != 2/*Feb*/
                                      ? kDay[tm.tm_mon]
                                      : 28 +
                                      (!(field[0] % 4)  &&
                                       (field[0] % 100
                                        ||  !(field[0] % 400))))) {
        return eIO_Unknown;
    }
    tm.tm_mday = field[2];
    if (field[3] < 0  ||  field[3] > 23)
        return eIO_Unknown;
    tm.tm_hour = field[3];
    if (field[4] < 0  ||  field[4] > 59)
        return eIO_Unknown;
    tm.tm_min  = field[4];
    if (field[5] < 0  ||  field[5] > 60) /* allow one leap second */
        return eIO_Unknown;
    tm.tm_sec  = field[5];

#ifdef HAVE_TIMEGM
    if ((t = timegm(&tm)) == (time_t)(-1))
        return eIO_Unknown;
#else
    n = tm.tm_hour;
    tm.tm_isdst = -1;
    if ((t = mktime(&tm)) == (time_t)(-1))
        return eIO_Unknown;

    tzdiff = UTIL_Timezone();

    if (t >= tzdiff)
        t -= tzdiff;
    if (tm.tm_isdst > 0  &&  n == tm.tm_hour)
        t += 3600;
#endif /*HAVE_TIMEGM*/

    n = sprintf(buf, "%lu%s%-.9s", (unsigned long) t,
                &"."[!frac || !*frac], frac ? frac : "");
    assert(0 < n  &&  (size_t) n < sizeof(buf));
    if (!BUF_Write(&xxx->rbuf, buf, (size_t) n))
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
            /* file is not plain or does not exist */
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
        if (xxx->flag & fFTP_DelayRestart)
            return eIO_Unknown;
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


static EIO_Status x_FTPRetrieveCB(SFTPConnector* xxx, int code,
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
    return x_FTPXfer(xxx, cmd, timeout, x_FTPRetrieveCB);
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


/* DELE (NB: some implementations have equiv "XDEL", too) */
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


static EIO_Status x_FTPMlsdCB(SFTPConnector* xxx, int code,
                              size_t lineno, const char* line)
{
    if (lineno  ||  code != 501)
        return eIO_Success;
    return xxx->feat & fFtpFeature_MLSx ? eIO_Unknown : eIO_NotSupported;
}


static EIO_Status x_FTPMlstCB(SFTPConnector* xxx, int code,
                              size_t lineno, const char* line)
{
    if (!lineno) {
        return code == 250 ? eIO_Success :
            xxx->feat & fFtpFeature_MLSx ? eIO_Unknown : eIO_NotSupported;
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
static EIO_Status s_FTPMlsX(SFTPConnector*  xxx,
                            const char*     cmd,
                            const STimeout* timeout)
{
    if (cmd[3] == 'T') { /* MLST */
        EIO_Status status = s_FTPCommand(xxx, cmd, 0);
        if (status != eIO_Success)
            return status;
        status = s_FTPReply(xxx, 0/*code checked by cb*/, 0, 0, x_FTPMlstCB);
        if (status != eIO_Success)
            BUF_Erase(xxx->rbuf);
        return status;
    }
    /* MLSD */
    xxx->size = 0;  /* cf. s_FTPRetrieve() */
    return x_FTPXfer(xxx, cmd, timeout, x_FTPMlsdCB);
}


static EIO_Status x_FTPNegotiateCB(SFTPConnector* xxx, int code,
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
    status = s_FTPReply(xxx, &code, 0, 0, x_FTPNegotiateCB);
    if (status == eIO_Success) {
        if (*cmd == 'F') {  /* FEAT */
            if (code != 211)
                status = eIO_Closed;
        } else {            /* OPTS */
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
static EIO_Status x_FTPPollCntl(SFTPConnector* xxx, const STimeout* timeout)
{
    EIO_Status status = eIO_Success;
    int/*bool*/ abor = xxx->abor;
    xxx->abor = 0/*false*/;
    for (;;) {
        char       buf[80];
        int        code;
        EIO_Status wait = SOCK_Wait(xxx->cntl, eIO_Read, &kZeroTimeout);
        if (wait != eIO_Success) {
            if (wait  == eIO_Closed) {
                status = eIO_Closed;
                x_FTPCloseCntl(xxx, IO_StatusStr(status));
            }
            break;
        }
        if (timeout != &kZeroTimeout) {
            SOCK_SetTimeout(xxx->cntl, eIO_Read,
                            timeout ? timeout : &kFailsafeTimeout);
            timeout  = &kZeroTimeout;  /* set timeout external / only once */
        }
        status = s_FTPReply(xxx, &code, buf, sizeof(buf) - 1, 0);
        if (status == eIO_Success) {
            if (code == 450  &&  abor) {
                abor = 0/*false*/;
                continue;
            }
            assert(!xxx->data  ||  xxx->send);
            CORE_LOGF_X(12, xxx->data ? eLOG_Error : eLOG_Warning,
                        ("[FTP%s%s]  %spurious response %d from server%s%s",
                         xxx->what ? "; " : "", xxx->what ? xxx->what : "",
                         xxx->data ? "Aborting upload due to a s" : "S", code,
                         *buf ? ": " : "", buf));
            if (xxx->data) {
                x_FTPCloseData(xxx, eIO_Close/*silent close*/, 0);
                xxx->sync = 1/*true*/;
                status = eIO_Closed;
            }
        } else if (!xxx->cntl)
            status  = eIO_Closed;
        if (status == eIO_Closed)
            break;
    }
    xxx->abor = abor ? 1 : 0;
    return status;
}


static EIO_Status x_FTPSyncCntl(SFTPConnector* xxx, const STimeout* timeout)
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
    return x_FTPPollCntl(xxx, timeout);
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
        status  = x_FTPSyncCntl(xxx, timeout);
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
    size = BUF_Size(xxx->wbuf);
    if ((s = (char*) malloc(size + 1)) != 0
        &&  BUF_Read(xxx->wbuf, s, size) == size) {
        const char* c;
        assert(!memchr(s, '\n', size));
        if (size  &&  s[size - 1] == '\r')
            size--;
        s[size] = '\0';
        if (*s)
            xxx->what = s;
        if (!(c = (const char*) memchr(s, ' ', size)))
            c = s + size; /*@'\0'*/
        else
            size = (size_t)(c - s);
        assert(*c == ' '  ||  !*c);
        if (!size  ||  size == 3  ||  size == 4) {
            SOCK_SetTimeout(xxx->cntl, eIO_ReadWrite, timeout);
            if (!size || (size == 4 && strncasecmp(s, "NOOP", 4) == 0 && !*c)){
                /* Special, means to stop the current command and reach EOF */
                status = size ? x_FTPNoop(xxx) : eIO_Success;
            } else if  (size == 3  &&   strncasecmp(s, "REN",  3) == 0) {
                /* Special-case, non-standard command */
                status = s_FTPRename(xxx, c + strspn(c, " \t"));
            } else if ((size == 3  ||  toupper((unsigned char) c[-4]) == 'X')
                       &&          (strncasecmp(c - 3, "CWD",  3) == 0  ||
                                    strncasecmp(c - 3, "PWD",  3) == 0  ||
                                    strncasecmp(c - 3, "MKD",  3) == 0  ||
                                    strncasecmp(c - 3, "RMD",  3) == 0)) {
                status = s_FTPDir (xxx, s, *c ? c + 1 : c);
            } else if  (size == 4  &&  (strncasecmp(s, "CDUP", 4) == 0  ||
                                        strncasecmp(s, "XCUP", 4) == 0)) {
                status = s_FTPDir (xxx, s, *c ? c + 1 : c);
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
                status = s_FTPMlsX    (xxx, s, timeout);
            } else if  (size == 4  &&  (strncasecmp(s, "FEAT", 4) == 0  ||
                                        strncasecmp(s, "OPTS", 4) == 0)) {
                status = s_FTPNegotiate(xxx, s);
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
    if (status != eIO_Timeout)
        xxx->abor = 0/*false*/;
    BUF_Erase(xxx->wbuf);
    return status;
}


static EIO_Status x_FTPCompleteUpload(SFTPConnector*  xxx,
                                      const STimeout* timeout)
{
    EIO_Status status;
    int code;

    assert(!BUF_Size(xxx->rbuf));
    assert(xxx->cntl  &&  xxx->send  &&  xxx->open);
    if (xxx->data) {
        status = x_FTPCloseData(xxx, xxx->flag & fFTP_NoSizeChecks
                                ? eIO_ReadWrite : eIO_Write, timeout);
        assert(!xxx->data);
        xxx->w_status = status;
        if (status != eIO_Success)
            return status;
    }
    SOCK_SetTimeout(xxx->cntl, eIO_Read, timeout);
    status = s_FTPReply(xxx, &code, 0, 0, 0);
    if (status != eIO_Timeout) {
        xxx->send = 0/*false*/;
        if (status == eIO_Success) {
            if (code == 225/*Microsoft*/  ||  code == 226) {
                char buf[80];
                int n = sprintf(buf, "%" NCBI_BIGCOUNT_FORMAT_SPEC, xxx->size);
                assert(!BUF_Size(xxx->rbuf)  &&  n > 0);
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
        if (xxx->info->debug_printout) {
            CORE_LOCK_READ;
            ConnNetInfo_Log(xxx->info, eLOG_Note, CORE_GetLOG());
            CORE_UNLOCK;
        }
        status = SOCK_CreateEx(xxx->info->host, xxx->info->port, timeout,
                               &xxx->cntl, 0, 0, fSOCK_KeepAlive
                               | (xxx->flag & fFTP_LogControl
                                  ? fSOCK_LogOn : fSOCK_LogDefault));
        xxx->sync = 0/*false*/;
        if (status == eIO_Success) {
            SOCK_DisableOSSendDelay(xxx->cntl, 1/*yes,disable*/);
            SOCK_SetTimeout(xxx->cntl, eIO_ReadWrite, timeout);
            status  = x_FTPLogin(xxx);
        } else
            assert(!xxx->cntl);
        if (status == eIO_Success)
            status  = x_FTPBinary(xxx);
        if (status == eIO_Success  &&  *xxx->info->path
            &&  !(xxx->flag & fFTP_IgnorePath)) {
            status  = x_FTPDir(xxx, 0,  xxx->info->path);
        }
        if (status == eIO_Success) {
            xxx->send = xxx->open = xxx->rclr = xxx->abor = 0/*false*/;
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
    if (1 < xxx->info->max_try  &&  xxx->info->max_try <= ntry) {
        CORE_LOGF_X(11, eLOG_Error,
                    ("[FTP; %s:%hu]  Too many failed attempts (%hu), giving up"
                     , xxx->info->host, xxx->info->port, ntry));
    }
    assert(!(status == eIO_Success) == !xxx->cntl);
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
                return x_FTPCompleteUpload(xxx, timeout);
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
        assert(!BUF_Size(xxx->wbuf));
        if (!xxx->data)
            return eIO_Closed;
        assert(xxx->open);
        status = x_FTPPollCntl(xxx, timeout);
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
        goto out;
    }

    if (size) {
        const char* run = (const char*) memchr(buf, '\n', size);
        *n_written = size; /* by default, report the entire output consumed */
        if (run  &&  run < (const char*) buf + --size) {
            /* reject multiple commands */
            BUF_Erase(xxx->wbuf);
            status = eIO_Unknown;
        } else {
            size_t count = 0;
            if (xxx->flag & fFTP_UncleanIAC) {
                if (BUF_Write(&xxx->wbuf, buf, size))
                    count = size;
            } else {
                static const char kIAC[] = { '\377'/*IAC*/, '\377' };
                const char* s = (const char*) buf;
                while (count < size) {
                    const char* p;
                    size_t part;
                    if (count) {
                        /* Escaped IAC (Interpret As Command) char, RFC854 */
                        if (!BUF_Write(&xxx->wbuf, kIAC, sizeof(kIAC)))
                            break;
                        assert(*s == kIAC[0]);
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
                /* keep appending */
                status = eIO_Success;
            } else {
                status = s_FTPExecute(xxx, timeout);
                if (status == eIO_Closed  &&  !xxx->cntl)
                    *n_written = 0;
                return status;
            }
        }
        if (xxx->what  &&  status != eIO_Success) {
            free((void*) xxx->what);
            xxx->what = 0;
        }
    } else
        status = eIO_Success;

 out:
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

    if (xxx->send) {
        return xxx->open ? eIO_Success
            : xxx->r_status != eIO_Success ? xxx->r_status
            : xxx->w_status != eIO_Success ? xxx->w_status : eIO_Closed;
    }
    return BUF_Size(xxx->wbuf) ? s_FTPExecute(xxx, timeout) : eIO_Success;
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
        return eIO_Unknown;

    if (BUF_Size(xxx->wbuf)) {
        assert(!xxx->send);
        status = s_FTPExecute(xxx, timeout);
        if (status != eIO_Success)
            return status;
    }
    if (xxx->send) {
        if (xxx->open) {
            status = x_FTPCompleteUpload(xxx, timeout);
            if (status != eIO_Success)
                return status;
            assert(!xxx->send  &&  !xxx->data);
        } else {
            assert(!xxx->data);
            xxx->send = 0/*false*/;
            assert(!BUF_Size(xxx->wbuf));
            assert(!BUF_Size(xxx->rbuf));
        }
    } else if (xxx->data) {
        assert(!xxx->send  &&  !BUF_Size(xxx->rbuf));
        /* NB: Cannot use x_FTPPollCntl() here because a response about data
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
        return eIO_Closed;
    return eIO_Success;
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
        status = x_FTPCloseData(xxx, how, "Incomplete");
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
            if (x_IsLongerTimeout(timeout, &kFailsafeTimeout))
                timeout = &kFailsafeTimeout;
            SOCK_SetTimeout(xxx->cntl, eIO_ReadWrite, timeout);
            /* all implementations MUST support QUIT */
            status = s_FTPCommand(xxx, "QUIT", 0);
            if (status == eIO_Success) {
                status  = x_FTPDrainReply(xxx, &code, code);
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
    SFTPConnector*  xxx = (SFTPConnector*) connector->handle;
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
    CONN_SET_DEFAULT_TIMEOUT(meta, xxx->info->timeout);
}


static void s_Destroy
(CONNECTOR connector)
{
    SFTPConnector* xxx = (SFTPConnector*) connector->handle;
    connector->handle = 0;

    ConnNetInfo_Destroy((SConnNetInfo*) xxx->info);
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
    SConnNetInfo*  xinfo;
    CONNECTOR      ccc;
    SFTPConnector* xxx;

    if ((host  &&  strlen(host) >= sizeof(xinfo->host))  ||
        (user  &&  strlen(user) >= sizeof(xinfo->user))  ||
        (pass  &&  strlen(pass) >= sizeof(xinfo->pass))  ||
        (path  &&  strlen(path) >= sizeof(xinfo->path))  ||
        (info  &&  info->scheme != eURL_Unspec  &&  info->scheme != eURL_Ftp)){
        return 0;
    }
    if (!(ccc = (SConnector*) malloc(sizeof(SConnector))))
        return 0;
    if (!(xxx = (SFTPConnector*) malloc(sizeof(*xxx)))) {
        free(ccc);
        return 0;
    }
    xinfo = (info
             ? ConnNetInfo_Clone(info)
             : ConnNetInfo_CreateInternal("_FTP"));
    if (!(xxx->info = xinfo)) {
        free(ccc);
        free(xxx);
        return 0;
    }
    xinfo->scheme = eURL_Ftp;
    ConnNetInfo_SetArgs(xinfo, 0);
    if (!info) {
        if (host  &&  *host)
            strcpy(xinfo->host,               host);
        xinfo->port =                         port;
        strcpy(xinfo->user, user  &&  *user ? user : "ftp");
        strcpy(xinfo->pass, pass            ? pass : "-none@");
        strcpy(xinfo->path, path            ? path : "");
        flag &= (TFTP_Flags)(~fFTP_IgnorePath);
    } else if (!(flag & fFTP_LogAll)) {
        switch (info->debug_printout) {
        case eDebugPrintout_Some:
            flag |= fFTP_LogControl;
            break;
        case eDebugPrintout_Data:
            flag |= fFTP_LogAll;
            break;
        }
    }
    if (!xinfo->port)
        xinfo->port = CONN_PORT_FTP;
    xinfo->req_method = eReqMethod_Any;
    xinfo->stateless = 0;
    xinfo->lb_disable = 0;
    xinfo->http_proxy_leak = 0;
    if (!(flag & fFTP_UseProxy)  ||  xinfo->http_proxy_mask == fProxy_Http) {
        xinfo->http_proxy_host[0] = '\0';
        xinfo->http_proxy_port    =   0;
        xinfo->http_proxy_user[0] = '\0';
        xinfo->http_proxy_pass[0] = '\0';
    } else
        CORE_LOG(eLOG_Critical, "fFTP_UseProxy not yet implemented");
    ConnNetInfo_SetUserHeader(xinfo, 0);
    if (xinfo->http_referer) {
        free((void*) xinfo->http_referer);
        xinfo->http_referer = 0;
    }

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
