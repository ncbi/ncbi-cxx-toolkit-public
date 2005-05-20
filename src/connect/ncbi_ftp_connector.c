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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   FTP CONNECTOR
 *   See also:  RFCs 959 (STD 9), 1634 (FYI 24),
 *   and IETF 9-2002 "Extensions to FTP".
 *
 *   See <connect/ncbi_connector.h> for the detailed specification of
 *   the connector's methods and structures.
 *
 */

#include "ncbi_ansi_ext.h"
#include "ncbi_assert.h"
#include "ncbi_priv.h"
#include <connect/ncbi_buffer.h>
#include <connect/ncbi_ftp_connector.h>
#include <connect/ncbi_socket.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>


/***********************************************************************
 *  INTERNAL -- Auxiliary types and static functions
 ***********************************************************************/


typedef enum {
    eFtpFeature_None = 0,
    eFtpFeature_MDTM = 1,
    eFtpFeature_SIZE = 2
} EFtpFeature;
typedef unsigned int TFtpFeatures; /* bitwise OR of individual EFtpFeature's */


/* All internal data necessary to perform the (re)connect and i/o
 */
typedef struct {
    const char*    host;
    unsigned short port;
    const char*    user;
    const char*    pass;
    const char*    path;
    const char*    name;
    TFtpFeatures   feat;
    TFCDC_Flags    flag;
    SOCK           cntl;  /* control connection */
    SOCK           data;  /* data    connection */
    BUF            wbuf;  /* write buffer       */
    EIO_Status     r_status;
    EIO_Status     w_status;
} SFTPConnector;


static const STimeout kFTPFailsafeTimeout = {10, 0};


static EIO_Status s_ReadReply(SOCK sock, int* code,
                              char* line, size_t maxlinelen)
{
    int/*bool*/ first = 1/*true*/;
    for (;;) {
        int c, m;
        size_t n;
        char buf[1024];
        EIO_Status status = SOCK_ReadLine(sock, buf, sizeof(buf), &n);
        /* All FTP replies are at least '\n'-terminated, no ending with EOF */
        if (status != eIO_Success)
            return status;
        if (n == sizeof(buf))
            return eIO_Unknown/*line too long*/;
        if (first  ||  isdigit((unsigned char) *buf)) {
            if (sscanf(buf, "%d%n", &c, &m) < 1)
                return eIO_Unknown;
        } else
            c = 0;
        if (first) {
            if (m != 3  ||  code == 0)
                return eIO_Unknown;
            if (line)
                strncpy0(line, &buf[m + 1], maxlinelen);
            *code = c;
            if (buf[m] != '-') {
                if (buf[m] == ' ')
                    break;
                return eIO_Unknown;
            }
            first = 0/*false*/;
        } else if (c == *code  &&  m == 3  &&  buf[m] == ' ')
            break;
    }
    return eIO_Success;
}


static EIO_Status s_FTPReply(SFTPConnector* xxx, int* code,
                             char* line, size_t maxlinelen)
{
    int c = 0;
    EIO_Status status = eIO_Closed;
    if (xxx->cntl) {
        status = s_ReadReply(xxx->cntl, &c, line, maxlinelen);
        if (status == eIO_Success  &&  c == 421)
            status =  eIO_Closed;
        if (status == eIO_Closed  ||  (status == eIO_Success  &&  c == 221)) {
            SOCK_Close(xxx->cntl);
            xxx->cntl = 0;
            if (xxx->data) {
                SOCK_Close(xxx->data);
                xxx->data = 0;
            }
        }
    }
    if (code)
        *code = c;
    return status;
}


static EIO_Status s_FTPDrainReply(SFTPConnector* xxx, int* code, int cXX)
{
    EIO_Status status;
    int        c;
    while ((status = s_FTPReply(xxx, &c, 0, 0)) == eIO_Success  &&
           (!cXX  ||  c/100 != cXX)) {
        *code = c;
    }
    return status;
}


static EIO_Status s_WriteCommand(SOCK sock,
                                 const char* cmd, const char* arg)
{
    size_t cmdlen = strlen(cmd);
    size_t arglen = arg ? strlen(arg) : 0;
    size_t linelen = cmdlen + (arglen ? 1/* */ + arglen : 0) + 2/*\r\n*/;
    char* line = (char*) malloc(linelen + 1/*\0*/);
    EIO_Status status = eIO_Unknown;
    if (line) {
        memcpy(line, cmd, cmdlen);
        if (arglen) {
            line[cmdlen++] = ' ';
            memcpy(line + cmdlen, arg, arglen);
            cmdlen += arglen;
        }
        line[cmdlen++] = '\r';
        line[cmdlen++] = '\n';
        line[cmdlen]   = '\0';
        status = SOCK_Write(sock, line, linelen, 0, eIO_WritePersist);
        free(line);
    }
    return status;
}


static EIO_Status s_FTPCommand(SFTPConnector* xxx,
                               const char* cmd, const char* arg)
{
    return xxx->cntl ? s_WriteCommand(xxx->cntl, cmd, arg) : eIO_Closed;
}


static EIO_Status s_FTPLogin(SFTPConnector* xxx, const STimeout* timeout)
{
    EIO_Status status;
    int code;

    assert(xxx->cntl  &&  xxx->user  &&  xxx->pass);
    status = SOCK_SetTimeout(xxx->cntl, eIO_ReadWrite, timeout);
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &code, 0, 0);
    if (status != eIO_Success)
        return status;
    if (code != 220  ||  !*xxx->user)
        return eIO_Unknown;
    status = s_FTPCommand(xxx, "USER", xxx->user);
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &code, 0, 0);
    if (status != eIO_Success)
        return status;
    if (code == 230)
        return eIO_Success;
    if (code != 331  ||  !*xxx->pass)
        return eIO_Unknown;
    status = s_FTPCommand(xxx, "PASS", xxx->pass);
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &code, 0, 0);
    if (status != eIO_Success)
        return status;
    if (code == 230)
        return eIO_Success;
    else
        return eIO_Unknown;
}


static EIO_Status s_FTPChdir(SFTPConnector* xxx, const char* cmd)
{
    if (cmd  ||  xxx->path) {
        int code;
        EIO_Status status = s_FTPCommand(xxx,
                                         cmd ? cmd : "CWD",
                                         cmd ? 0   : xxx->path);
        if (status != eIO_Success)
            return status;
        status = s_FTPReply(xxx, &code, 0, 0);
        if (status != eIO_Success)
            return status;
        if (code != 250)
            return eIO_Unknown;
    }
    return eIO_Success;
}


static EIO_Status s_FTPBinary(SFTPConnector* xxx)
{
    int code;
    EIO_Status status = s_FTPCommand(xxx, "TYPE", "I");
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &code, 0, 0);
    if (status != eIO_Success)
        return status;
    if (code == 200)
        return eIO_Success;
    else
        return eIO_Unknown;
}


static EIO_Status s_FTPAbort(SFTPConnector*  xxx,
                             const STimeout* timeout,
                             int/*bool*/     quit)
{
    EIO_Status status = eIO_Success;
    int        code;
    size_t     n;

    if (!xxx->data)
        return status;
    if (quit  ||  !xxx->cntl) {
        status = SOCK_Abort(xxx->data);
        xxx->data = 0;
        return status;
    }
    if (!timeout)
        timeout = &kFTPFailsafeTimeout;
    if (SOCK_SetTimeout(xxx->cntl, eIO_ReadWrite, timeout) != eIO_Success ||
        SOCK_SetTimeout(xxx->data, eIO_ReadWrite, timeout) != eIO_Success ||
        /* Send TELNET IP (Interrupt Process) command */
        (status = SOCK_Write(xxx->cntl, "\377\364", 2, &n, eIO_WritePersist))
        != eIO_Success  ||  n != 2                                        ||
        /* Send TELNET DM (Data Mark) command to complete SYNCH, RFC 854 */
        (status = SOCK_Write(xxx->cntl, "\377\362", 2, &n, eIO_WriteOutOfBand))
        != eIO_Success  ||  n != 2                                        ||
        (status = s_FTPCommand(xxx, "ABOR", 0)) != eIO_Success) {
        SOCK_Abort(xxx->data);
        xxx->data = 0;
        return status == eIO_Success ? eIO_Unknown : status;
    }
    while (SOCK_Read(xxx->data, 0, 1024*1024/*drain up*/, 0, eIO_ReadPlain)
           == eIO_Success);
    if (SOCK_Status(xxx->data, eIO_Read) == eIO_Closed) {
        SOCK_Close(xxx->data);
        xxx->data = 0;
    }
    if ((status = s_FTPDrainReply(xxx, &code, 2/*2xx*/)) == eIO_Success  &&
        /* Microsoft FTP is known to return 225 (instead of 226) */
        code != 225  &&  code != 226) {
        status = eIO_Unknown;
    }
    if (xxx->data) {
        if (status == eIO_Success)
            status = SOCK_Close(xxx->data);
        else {
            if (status == eIO_Timeout) {
                CORE_LOG(eLOG_Warning,
                         "[FTP]  Timed out on data connection abort");
            }
            SOCK_Abort(xxx->data);
        }
        xxx->data = 0;
    }
    return status;
}


static EIO_Status s_FTPPasv(SFTPConnector* xxx)
{
    static const STimeout instant = {0, 0};
    unsigned int   host, i;
    unsigned short port;
    int  code, o[6];
    char buf[128];

    EIO_Status status = s_FTPCommand(xxx, "PASV", 0);
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &code, buf, sizeof(buf) - 1);
    if (status != eIO_Success  ||  code != 227)
        return eIO_Unknown;
    buf[sizeof(buf) - 1] = '\0';
    for (;;) {
        char* c;
        /* RFC 1123 4.1.2.6 says that ()'s in PASV reply must not be assumed */
        for (c = buf; *c; c++) {
            if (isdigit((unsigned char)(*c)))
                break;
        }
        if (!*c)
            return eIO_Unknown;
        if (sscanf(c, "%d,%d,%d,%d,%d,%d%n",
                   &o[0], &o[1], &o[2], &o[3], &o[4], &o[5], &code) >= 6) {
            break;
        }
        strcpy(buf, c + code);
    }
    for (i = 0; i < (unsigned int)(sizeof(o)/sizeof(o[0])); i++) {
        if (o[i] < 0  ||  o[i] > 255)
            return eIO_Unknown;
    }
    i = (((((o[0] << 8) | o[1]) << 8) | o[2]) << 8) | o[3];
    host = SOCK_htonl(i);
    i = (o[4] << 8) | o[5];
    port = (unsigned short) i;
    if (SOCK_ntoa(host, buf, sizeof(buf)) == 0  &&
        SOCK_CreateEx(buf, port, &instant, &xxx->data, 0, 0,
                      xxx->flag & eFCDC_LogData ? eOn : eDefault)
        == eIO_Success) {
        return eIO_Success;
    }
    s_FTPAbort(xxx, 0, 0/*!quit*/);
    return eIO_Unknown;
}


static EIO_Status s_FTPRetrieve(SFTPConnector* xxx,
                                const char*    cmd)
{
    int code;
    EIO_Status status = s_FTPPasv(xxx);
    if (status != eIO_Success)
        return status;
    status = s_FTPCommand(xxx, cmd, 0);
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &code, 0, 0);
    if (status != eIO_Success)
        return status;
    if (code == 150)
        return eIO_Success;
    if (code == 450  &&  (strncasecmp(cmd, "NLST", 4) == 0  ||
                          strncasecmp(cmd, "LIST", 4) == 0)) {
        /* server usually drops data connection on 450: no files ...*/
        if (xxx->data) {
            /* ... so do we :-/ */
            SOCK_Abort(xxx->data);
            xxx->data = 0;
        }
        /* with no data connection open, user gets eIO_Closed on read */
        return eIO_Success;
    }
    s_FTPAbort(xxx, 0, 0/*!quit*/);
    return eIO_Unknown;
}


static EIO_Status s_FTPExecute(SFTPConnector* xxx, const STimeout* timeout)
{
    EIO_Status status;
    size_t     size;
    char*      s;

    if ((status = s_FTPAbort(xxx, timeout, 0/*!quit*/)) != eIO_Success)
        return status;
    verify((size = BUF_Size(xxx->wbuf)) != 0);
    if (!(s = (char*) malloc(size + 1)))
        return eIO_Unknown;
    if (BUF_Read(xxx->wbuf, s, size) == size  &&
        SOCK_SetTimeout(xxx->cntl, eIO_ReadWrite, timeout) == eIO_Success) {
        char* c;
        if ((c = memchr(s, '\n', size)) != 0) {
            if (c != s  &&  c[-1] == '\r')
                c--;
            *c = '\0';
        } else
            s[size] = '\0';
        if (!(c = strchr(s, ' ')))
            c = s + strlen(s);
        if (!(size = (size_t)(c - s))) {
            status = eIO_Unknown;
        } else if (strncasecmp(s, "CWD",  size) == 0) {
            status = s_FTPChdir(xxx, s);
        } else if (strncasecmp(s, "LIST", size) == 0  ||
                   strncasecmp(s, "NLST", size) == 0  ||
                   strncasecmp(s, "RETR", size) == 0) {
            status = s_FTPRetrieve(xxx, s);
        } else if (strncasecmp(s, "REST", size) == 0) {
            status = s_FTPCommand(xxx, s, 0);
            if (status == eIO_Success) {
                int code;
                status = s_FTPReply(xxx, &code, 0, 0);
                if (status == eIO_Success  &&  code != 350)
                    status = eIO_Unknown;
            }
        } else
            status = eIO_Unknown;
    } else
        status = eIO_Unknown;
    free(s);
    return status;
}


/***********************************************************************
 *  INTERNAL -- "s_VT_*" functions for the "virt. table" of connector methods
 ***********************************************************************/

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
    static const char* s_VT_GetType (CONNECTOR       connector);
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
#  ifdef IMPLEMENTED__CONN_WaitAsync
    static EIO_Status s_VT_WaitAsync(void*                   connector,
                                     FConnectorAsyncHandler  func,
                                     SConnectorAsyncHandler* data);
#  endif
#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */


/*ARGSUSED*/
static const char* s_VT_GetType
(CONNECTOR connector)
{
    return "FTP";
}


static EIO_Status s_VT_Open
(CONNECTOR       connector,
 const STimeout* timeout)
{
    SFTPConnector* xxx = (SFTPConnector*) connector->handle;
    EIO_Status status;

    assert(!xxx->data  &&  !xxx->cntl);
    status = SOCK_CreateEx(xxx->host, xxx->port, timeout, &xxx->cntl, 0, 0,
                           xxx->flag & eFCDC_LogControl ? eOn : eDefault);
    if (status == eIO_Success)
        status = s_FTPLogin(xxx, timeout);
    if (status == eIO_Success)
        status = s_FTPChdir(xxx, 0);
    if (status == eIO_Success)
        status = s_FTPBinary(xxx);
    if (status != eIO_Success) {
        if (xxx->cntl) {
            SOCK_Close(xxx->cntl);
            xxx->cntl = 0;
        }
    }
    xxx->r_status = status;
    xxx->w_status = status;
    return status;
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
    size_t s;

    xxx->w_status = eIO_Success;
    if (!size)
        return eIO_Success;
    if (!xxx->cntl)
        return eIO_Closed;

    if ((c = memchr((const char*) buf, '\n', size)) != 0  &&
        c < (const char*) buf + size - 1) {
        return eIO_Unknown;
    }
    status = BUF_Write(&xxx->wbuf, buf, size) ? eIO_Success : eIO_Unknown;
    if (status == eIO_Success  &&  c) {
        xxx->w_status = s_FTPExecute(xxx, timeout);
        status = xxx->w_status;
    }
    *n_written = size;
    return status;
}


static EIO_Status s_VT_Read
(CONNECTOR       connector,
 void*           buf,
 size_t          size,
 size_t*         n_read,
 const STimeout* timeout)
{
    SFTPConnector* xxx = (SFTPConnector*) connector->handle;
    EIO_Status status = eIO_Closed;
    xxx->r_status = eIO_Success;
    if (xxx->cntl  &&  xxx->data) {
        status = SOCK_SetTimeout(xxx->data, eIO_Read, timeout);
        if (status == eIO_Success) {
            xxx->r_status = SOCK_Read(xxx->data,
                                      buf, size, n_read, eIO_ReadPlain);
            if (xxx->r_status == eIO_Closed) {
                int code;
                SOCK_Close(xxx->data);
                xxx->data = 0;
                SOCK_SetTimeout(xxx->cntl, eIO_Read, timeout);
                if (s_FTPReply(xxx, &code, 0, 0) != eIO_Success  ||
                    (code != 225  &&  code != 226)) {
                    xxx->r_status = eIO_Unknown;
                }
            }
            status = xxx->r_status;
        }
    }
    return status;
}


static EIO_Status s_VT_Wait
(CONNECTOR       connector,
 EIO_Event       event,
 const STimeout* timeout)
{
    SFTPConnector* xxx = (SFTPConnector*) connector->handle;
    if (!xxx->cntl)
        return eIO_Closed;
    if (event & eIO_Write)
        return eIO_Success;
    if (!xxx->data) {
        EIO_Status status;
        if (!BUF_Size(xxx->wbuf))
            return eIO_Closed;
        if ((status = s_FTPExecute(xxx, timeout)) != eIO_Success)
            return status;
        if (!xxx->data)
            return eIO_Closed;
    }
    return SOCK_Wait(xxx->data, eIO_Read, timeout);
}


static EIO_Status s_VT_Flush
(CONNECTOR       connector,
 const STimeout* timeout)
{
    SFTPConnector* xxx = (SFTPConnector*) connector->handle;
    return BUF_Size(xxx->wbuf) ? s_FTPExecute(xxx, timeout) : eIO_Success;
}


static EIO_Status s_VT_Status
(CONNECTOR connector,
 EIO_Event dir)
{
    SFTPConnector* xxx = (SFTPConnector*) connector->handle;

    switch (dir) {
    case eIO_Read:
        return xxx->r_status;
    case eIO_Write:
        return xxx->w_status;
    default:
        assert(0); /* should never happen as checked by connection */
        return eIO_InvalidArg;
    }
}


static EIO_Status s_VT_Close
(CONNECTOR       connector,
 const STimeout* timeout)
{
    SFTPConnector* xxx = (SFTPConnector*) connector->handle;
    EIO_Status status;

    if ((status = s_FTPAbort(xxx, timeout, 1/*quit*/)) == eIO_Success) {
        if (xxx->cntl) {
            int code;
            if (!timeout)
                timeout = &kFTPFailsafeTimeout;
            SOCK_SetTimeout(xxx->cntl, eIO_ReadWrite, timeout);
            status = s_FTPCommand(xxx, "QUIT", 0);
            if (status == eIO_Success)
                status = s_FTPDrainReply(xxx, &code, 0);
            if (status != eIO_Closed  ||  code != 221)
                status = eIO_Unknown;
        }
    }
    if (xxx->cntl) {
        assert(status != eIO_Success);
        if (status == eIO_Timeout)
            SOCK_Abort(xxx->cntl);
        else
            SOCK_Close(xxx->cntl);
        xxx->cntl = 0;
    }
    return status != eIO_Closed ? status : eIO_Success;
}


static void s_Setup
(SMetaConnector* meta,
 CONNECTOR       connector)
{
    /* initialize virtual table */
    CONN_SET_METHOD(meta, get_type,   s_VT_GetType,   connector);
    CONN_SET_METHOD(meta, open,       s_VT_Open,      connector);
    CONN_SET_METHOD(meta, wait,       s_VT_Wait,      connector);
    CONN_SET_METHOD(meta, write,      s_VT_Write,     connector);
    CONN_SET_METHOD(meta, flush,      s_VT_Flush,     connector);
    CONN_SET_METHOD(meta, read,       s_VT_Read,      connector);
    CONN_SET_METHOD(meta, status,     s_VT_Status,    connector);
    CONN_SET_METHOD(meta, close,      s_VT_Close,     connector);
#ifdef IMPLEMENTED__CONN_WaitAsync
    CONN_SET_METHOD(meta, wait_async, s_VT_WaitAsync, connector);
#endif
    meta->default_timeout = 0/*infinite*/;
}


static void s_Destroy
(CONNECTOR connector)
{
    SFTPConnector* xxx = (SFTPConnector*) connector->handle;
    free((void*) xxx->host);
    free((void*) xxx->user);
    free((void*) xxx->pass);
    if (xxx->path)
        free((void*) xxx->path);
    if (xxx->name)
        free((void*) xxx->name);
    BUF_Destroy(xxx->wbuf);
    free(xxx);
    connector->handle = 0;
    free(connector);
}


/***********************************************************************
 *  EXTERNAL -- the connector's "constructors"
 ***********************************************************************/

extern CONNECTOR FTP_CreateDownloadConnector(const char*    host,
                                             unsigned short port,
                                             const char*    user,
                                             const char*    pass,
                                             const char*    path,
                                             TFCDC_Flags    flag)
{
    CONNECTOR      ccc = (SConnector*) malloc(sizeof(SConnector));
    SFTPConnector* xxx = (SFTPConnector*) malloc(sizeof(*xxx));

    assert(!(flag & ~eFCDC_LogAll));

    xxx->data    = 0;
    xxx->cntl    = 0;
    xxx->wbuf    = 0;
    xxx->host    = strdup(host);
    xxx->port    = port ? port : 21;
    xxx->user    = strdup(user ? user : "ftp");
    xxx->pass    = strdup(pass ? pass : "none");
    xxx->path    = path  &&  *path ? strdup(path) : 0;
    xxx->name    = 0;
    xxx->flag    = flag;
    /* initialize connector data */
    ccc->handle  = xxx;
    ccc->next    = 0;
    ccc->meta    = 0;
    ccc->setup   = s_Setup;
    ccc->destroy = s_Destroy;

    return ccc;
}


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 1.12  2005/05/20 13:02:13  lavr
 * s_VT_Write() not to cut '\r' -- instead, do this in s_FTPExecute()
 *
 * Revision 1.11  2005/05/20 12:11:00  lavr
 * ABOR sequence reimplemented to work even with buggy FTPDs
 *
 * Revision 1.10  2005/05/18 20:56:45  lavr
 * Use assert() to test flags for validity in constructor
 *
 * Revision 1.9  2005/05/18 18:16:41  lavr
 * Add EFCDC_Flags and TFCDC_Flags to better control underlying SOCK logs
 * Fix s_VT_Write() not to use strchr() -- memchr() must be used there!
 *
 * Revision 1.8  2005/05/11 20:00:25  lavr
 * Empty NLST result list bug fixed
 *
 * Revision 1.7  2005/04/20 18:15:59  lavr
 * +<assert.h>
 *
 * Revision 1.6  2005/01/27 18:59:52  lavr
 * Explicit cast of malloc()ed memory
 *
 * Revision 1.5  2005/01/05 17:40:13  lavr
 * FEAT extensions and fixes for protocol compliance
 *
 * Revision 1.4  2004/12/27 15:31:27  lavr
 * Implement telnet SYNCH and FTP ABORT according to the standard
 *
 * Revision 1.3  2004/12/08 21:03:26  lavr
 * Fixes for default ctor parameters
 *
 * Revision 1.2  2004/12/07 14:21:55  lavr
 * Init wbuf in ctor
 *
 * Revision 1.1  2004/12/06 17:48:38  lavr
 * Initial revision
 *
 * ==========================================================================
 */
