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
 *   Send mail
 *
 */

#include "ncbi_ansi_ext.h"
#include "ncbi_priv.h"
#include <connect/ncbi_connutil.h>
#include <connect/ncbi_sendmail.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>

#define NCBI_USE_ERRCODE_X   Connect_Sendmail

#define MX_MAGIC_COOKIE  0xBA8ADEDA
#define MX_CRLF          "\r\n"

#define SMTP_READERR     -1      /* Read error from socket               */
#define SMTP_READTMO     -2      /* Read timed out                       */
#define SMTP_RESPERR     -3      /* Cannot read response prefix          */
#define SMTP_NOCODE      -4      /* No response code detected (letters?) */
#define SMTP_BADCODE     -5      /* Response code doesn't match in lines */
#define SMTP_BADRESP     -6      /* Malformed response                   */


/* Read SMTP response from the socket.
 * Return the response in the buffer provided,
 * and the response code (positive value) as a return value.
 * Return a negative code in case of problem (protocol response
 * read error or protocol violations).
 * Return 0 in case of a call error.
 */
static int s_SockRead(SOCK sock, char* response, size_t max_response_len)
{
    int/*bool*/ done = 0;
    size_t n = 0;
    int code = 0;

    assert(response  &&  max_response_len);
    do {
        EIO_Status status;
        size_t m = 0;
        char buf[4];

        status = SOCK_Read(sock, buf, 4, &m, eIO_ReadPersist);
        if (status != eIO_Success) {
            if (m == 3  &&  status == eIO_Closed)
                buf[m++] = ' ';
            else if (status == eIO_Timeout)
                return SMTP_READTMO;
            else if (m)
                return SMTP_RESPERR;
            else
                return SMTP_READERR;
        }
        assert(m == 4);

        if (buf[3] == '-'  ||  (done = isspace((unsigned char) buf[3]))) {
            buf[3]  = '\0';
            if (!code) {
                char* e;
                errno = 0;
                code = (int) strtol(buf, &e, 10);
                if (errno  ||  code <= 0  ||  e != buf + 3)
                    return SMTP_NOCODE;
            } else if (code != atoi(buf))
                return SMTP_BADCODE;
        } else
            return SMTP_BADRESP;

        if (status == eIO_Success) {
            do {
                status = SOCK_Read(sock, buf, 1, &m, eIO_ReadPlain);
                if (status == eIO_Closed) {
                    if (n < max_response_len)
                        response[n++] = '\n';
                    done = 1;
                    break;
                }
                if (!m)
                    return status == eIO_Timeout ? SMTP_READTMO : SMTP_READERR;
                if (*buf != '\r'  &&  n < max_response_len)
                    response[n++] = *buf;
                assert(status == eIO_Success);
            } while (*buf != '\n');

            /* At least '\n' should sit in the buffer */
            assert(n);
            if (done)
                response[n - 1] = '\0';
            else if (n < max_response_len)
                response[n] = ' ';
        } else {
            *response = '\0';
            assert(done);
            break;
        }
    } while (!done);

    assert(code > 0);
    return code;
}


static int/*bool*/ s_SockReadResponse(SOCK sock, int code, int alt_code,
                                      char* buf, size_t buf_size)
{
    int c = s_SockRead(sock, buf, buf_size);
    if (c <= 0) {
        const char* message = 0;
        switch (c) {
        case SMTP_READERR:
            message = "Read error";
            break;
        case SMTP_READTMO:
            message = "Read timed out";
            break;
        case SMTP_RESPERR:
            message = "Cannot read response prefix";
            break;
        case SMTP_NOCODE:
            message = "No response code detected";
            break;
        case SMTP_BADCODE:
            message = "Response code mismatch";
            break;
        case SMTP_BADRESP:
            message = "Malformed response";
            break;
        default:
            message = "Unknown error";
            assert(0);
            break;
        }
        assert(message);
        strncpy0(buf, message, buf_size - 1);
    } else if (c == code  ||  (alt_code  &&  c == alt_code))
        return 1/*success*/;
    return 0/*failure*/;
}


static int/*bool*/ s_SockWrite(SOCK sock, const char* buf, size_t len)
{
    size_t n;

    if (!len)
        len = strlen(buf);
    if (SOCK_Write(sock, buf, len, &n, eIO_WritePersist) == eIO_Success) {
        assert(n == len);
        return 1/*success*/;
    }
    return 0/*failure*/;
}


static void s_MakeFrom(char* buf, size_t size, const char* from,
                       ECORE_Username user)
{
    char x_buf[sizeof(((SSendMailInfo*) 0)->from)];
    const char* at;
    size_t len;

    if (from  &&  *from) {
        if (!(at = strchr(from, '@'))) {
            /* no "@", verbatim copy */
            if (buf != from)
                strncpy0(buf, from, size - 1);
            return;
        }
        if (at != from) {
            /* "user@[host]" */
            if (buf != from) {
                len = (size_t)(at - from);
                if (at[1]) {
                    /* "user@host" */
                    if (len < size) {
                        size_t tmp = len + strlen(at);
                        if (tmp < size)
                            len = tmp;
                    } else
                        len = size - 1;
                } /* else "user@" */
                strncpy0(buf, from, len);
            }
            if (*++at)
                return;  /* "user@host", all done */
            /* (*at) == '\0' */
        } else if (at[1]) {
            /* "@host", save host if it fits the temp buffer */
            if ((len = strlen(at)) < sizeof(x_buf)) {
                memcpy(x_buf, at, len + 1);
                at = x_buf;
            } else
                at = "@";
            *buf = '\0';
            /* (*at) != '\0' (=='@') */
        } else {
            /* "@" */
            *buf = '\0';
            return;
        }
    } else
        at = 0;
    if (!at  ||   *at) {
        if (!CORE_GetUsernameEx(buf, size, user)  ||  !*buf)
            strncpy0(buf, "anonymous", size - 1);
        len = strlen(buf);
    } else
        len = strlen(buf) - 1;
    size -= len;
    buf  += len;
    if (!at  ||  !*at) {
        if (size-- > 2) {
            *buf++ = '@';
            if ((!SOCK_gethostbyaddr(0, buf, size)  ||  !strchr(buf, '.'))
                &&  SOCK_gethostname(buf, size) != 0) {
                const char* host = getenv("HOSTNAME");
                if ((!host  &&  !(host = getenv("HOST")))
                    ||  (len = strlen(host)) >= size) {
                    *--buf = '\0';
                } else
                    strcpy(buf, host);
            }
        } else
            *buf   = '\0';
    } else if (1 < (len = strlen(at))  &&  len < size)
        memcpy(buf, at, len + 1);
}


static STimeout       s_MxTimeout;
static char           s_MxHost[256];
static unsigned short s_MxPort;


static void x_Sendmail_InitEnv(void)
{
    char         buf[sizeof(s_MxHost)], *e;
    unsigned int port;
    double       tmo;

    if (s_MxPort)
        return;

    if (!ConnNetInfo_GetValue(0, "MX_TIMEOUT", buf, sizeof(buf), 0)  ||  !*buf
        ||  (tmo = NCBI_simple_atof(buf, &e)) < 0.000001  ||  errno  ||  !*e) {
        tmo = 120.0/*2 min*/;
    }
    if (!ConnNetInfo_GetValue(0, "MX_PORT", buf, sizeof(buf), 0)
        ||  !(port = atoi(buf))  ||  port > 65535) {
        port = CONN_PORT_SMTP;
    }
    if (!ConnNetInfo_GetValue(0, "MX_HOST", buf, sizeof(buf), 0)
        ||  !*buf) {
#if defined(NCBI_OS_UNIX)  &&  !defined(NCBI_OS_CYGWIN)
        strcpy(buf, "localhost");
#else
        strcpy(buf, "mailgw");
#endif /*NCBI_OS_UNIX && !NCBI_OS_CYGWIN*/
    }

    CORE_LOCK_WRITE;
    s_MxTimeout.sec  = (unsigned int)  tmo;
    s_MxTimeout.usec = (unsigned int)((tmo - s_MxTimeout.sec) * 1000000.0);
    strcpy(s_MxHost, buf);
    s_MxPort = port;
    CORE_UNLOCK;
}


extern SSendMailInfo* SendMailInfo_InitEx(SSendMailInfo* info,
                                          const char*    from,
                                          ECORE_Username user)
{
    if (info) {
        x_Sendmail_InitEnv();
        info->cc           = 0;
        info->bcc          = 0;
        s_MakeFrom(info->from, sizeof(info->from), from, user);
        info->header       = 0;
        info->body_size    = 0;
        info->mx_timeout   = s_MxTimeout;
        info->mx_host      = s_MxHost;
        info->mx_port      = s_MxPort;
        info->mx_options   = 0;
        info->magic_cookie = MX_MAGIC_COOKIE;
    }
    return info;
}


extern const char* CORE_SendMail(const char* to,
                                 const char* subject,
                                 const char* body)
{
    return CORE_SendMailEx(to, subject, body, 0);
}


/* In two macros below the smartest (or, weak-minded?) Sun
 * C compiler warns about unreachable end-of-loop condition
 * (well, it thinks "a condition" is there, dumb!).
 */

#define SENDMAIL_RETURN(subcode, reason)                                \
    do {                                                                \
        if (sock) {                                                     \
            SOCK_Close(sock);                                           \
            sock = 0;                                                   \
        }                                                               \
        CORE_LOGF_X(subcode, eLOG_Error, ("[SendMail]  %s", reason));   \
        if (!sock)                                                      \
            return reason;                                              \
        /*NOTREACHED*/                                                  \
    } while (0)

#define SENDMAIL_RETURN2(subcode, reason, explanation)                  \
    do {                                                                \
        if (sock) {                                                     \
            SOCK_Close(sock);                                           \
            sock = 0;                                                   \
        }                                                               \
        CORE_LOGF_X(subcode, eLOG_Error,                                \
                    ("[SendMail]  %s: %s", reason, explanation));       \
        if (!sock)                                                      \
            return reason;                                              \
        /*NOTREACHED*/                                                  \
    } while (0)


static const char* s_SendRcpt(SOCK sock, const char* to,
                              char buf[], size_t buf_size,
                              const char what[],
                              const char write_error[],
                              const char proto_error[])
{
    char c;
    while ((c = *to++) != 0) {
        char   quote = 0;
        size_t k = 0;
        if (isspace((unsigned char) c))
            continue;
        while (k < buf_size) {
            if (quote) {
                if (c == quote)
                    quote = 0;
            } else if (c == '"'  ||  c == '<') {
                quote = c == '<' ? '>' : c;
            } else if (c == ',')
                break;
            buf[k++] = c == '\t' ? ' ' : c;
            if (!(c = *to++))
                break;
            if (isspace((unsigned char) c)) {
                while (isspace((unsigned char)(*to)))
                    to++;
            }
        }
        if (k >= buf_size)
            SENDMAIL_RETURN(3, "Recipient address is too long");
        buf[k] = '\0'/*just in case*/;
        if (quote) {
            CORE_LOGF_X(1, eLOG_Warning,
                        ("[SendMail]  Unbalanced delimiters in"
                         " recipient %s for %s: \"%c\" expected",
                         buf, what, quote));
        }
        if (!s_SockWrite(sock, "RCPT TO: <", 10)  ||
            !s_SockWrite(sock, buf, k)            ||
            !s_SockWrite(sock, ">" MX_CRLF, sizeof(MX_CRLF))) {
            SENDMAIL_RETURN(4, write_error);
        }
        if (!s_SockReadResponse(sock, 250, 251, buf, buf_size))
            SENDMAIL_RETURN2(5, proto_error, buf);
        if (!c)
            break;
    }
    return 0;
}


static size_t s_FromSize(const SSendMailInfo* info)
{
    const char* at, *dot;
    size_t len = strlen(info->from);

    if (!*info->from  ||  !(info->mx_options & fSendMail_StripNonFQDNHost))
        return len;
    if (!(at = (const char*) memchr(info->from, '@', len))
        ||  at == info->from + len - 1) {
        return len - 1;
    }
    if (!(dot = (const char*) memchr(at + 1, '.',
                                     len - (size_t)(at - info->from) - 1))
        ||  dot == at + 1  ||  dot == info->from + len - 1) {
        return (size_t)(at - info->from);
    }
    return len;
}


#define SENDMAIL_SENDRCPT(what, list, buffer)                              \
    s_SendRcpt(sock, list, buffer, sizeof(buffer), what,                   \
               "Write error in RCPT (" what ") command",                   \
               "Protocol error in RCPT (" what ") command")

#define SENDMAIL_READ_RESPONSE(code, altcode, buffer)                      \
    s_SockReadResponse(sock, code, altcode, buffer, sizeof(buffer))


extern const char* CORE_SendMailEx(const char*          to,
                                   const char*          subject,
                                   const char*          body,
                                   const SSendMailInfo* uinfo)
{
    static const STimeout zero = {0, 0};
    const SSendMailInfo* info;
    SSendMailInfo ainfo;
    EIO_Status status;
    char buffer[1024];
    TSOCK_Flags log;
    SOCK sock = 0;

    info = uinfo ? uinfo : SendMailInfo_Init(&ainfo);
    if (info->magic_cookie != MX_MAGIC_COOKIE)
        SENDMAIL_RETURN(6, "Invalid magic cookie");

    if ((!to         ||  !*to)        &&
        (!info->cc   ||  !*info->cc)  &&
        (!info->bcc  ||  !*info->bcc)) {
        SENDMAIL_RETURN(7, "At least one message recipient must be specified");
    }

    /* Open connection to sendmail */
    log = info->mx_options & fSendMail_LogOn ? fSOCK_LogOn : fSOCK_LogDefault;
    if ((status = SOCK_CreateEx(info->mx_host, info->mx_port,
                                &info->mx_timeout, &sock,
                                0, 0, log)) != eIO_Success) {
        sprintf(buffer, "%s:%hu (%s)", info->mx_host, info->mx_port,
                IO_StatusStr(status));
        SENDMAIL_RETURN2(8, "Cannot connect to sendmail", buffer);
    }
    SOCK_SetTimeout(sock, eIO_ReadWrite, &info->mx_timeout);
    SOCK_SetTimeout(sock, eIO_Close,     &info->mx_timeout);

    /* Follow the protocol conversation, RFC821 */
    if (!SENDMAIL_READ_RESPONSE(220, 0, buffer))
        SENDMAIL_RETURN2(9, "Protocol error in connection init", buffer);

    if ((!(info->mx_options & fSendMail_StripNonFQDNHost)
         ||  !SOCK_gethostbyaddr(0, buffer, sizeof(buffer)))
        &&  SOCK_gethostname(buffer, sizeof(buffer)) != 0) {
        SENDMAIL_RETURN(10, "Unable to get local host name");
    }
    if (!s_SockWrite(sock, "HELO ", 5)  ||
        !s_SockWrite(sock, buffer, 0)   ||
        !s_SockWrite(sock, MX_CRLF, sizeof(MX_CRLF)-1)) {
        SENDMAIL_RETURN(11, "Write error in HELO command");
    }
    if (!SENDMAIL_READ_RESPONSE(250, 0, buffer))
        SENDMAIL_RETURN2(12, "Protocol error in HELO command", buffer);

    if (!s_SockWrite(sock, "MAIL FROM: <", 12)            ||
        !s_SockWrite(sock, info->from, s_FromSize(info))  ||
        !s_SockWrite(sock, ">" MX_CRLF, sizeof(MX_CRLF))) {
        SENDMAIL_RETURN(13, "Write error in MAIL command");
    }
    if (!SENDMAIL_READ_RESPONSE(250, 0, buffer))
        SENDMAIL_RETURN2(14, "Protocol error in MAIL command", buffer);

    if (to  &&  *to) {
        const char* error = SENDMAIL_SENDRCPT("To", to, buffer);
        if (error)
            return error;
    }

    if (info->cc  &&  *info->cc) {
        const char* error = SENDMAIL_SENDRCPT("Cc", info->cc, buffer);
        if (error)
            return error;
    }

    if (info->bcc  &&  *info->bcc) {
        const char* error = SENDMAIL_SENDRCPT("Bcc", info->bcc, buffer);
        if (error)
            return error;
    }

    if (!s_SockWrite(sock, "DATA" MX_CRLF, 4 + sizeof(MX_CRLF)-1))
        SENDMAIL_RETURN(15, "Write error in DATA command");
    if (!SENDMAIL_READ_RESPONSE(354, 0, buffer))
        SENDMAIL_RETURN2(16, "Protocol error in DATA command", buffer);

    (void) SOCK_SetCork(sock, eOn);

    if (!(info->mx_options & fSendMail_NoMxHeader)) {
        if (!(info->mx_options & fSendMail_Old822Headers)) {
            /* Locale-independent month names per RFC5322 and older */
            static const char* kMonth[] = { "Jan", "Feb", "Mar", "Apr",
                                            "May", "Jun", "Jul", "Aug",
                                            "Sep", "Oct", "Nov", "Dec" };
            /* Skip DoW: it's optional yet must also be locale-independent */
            static const char  kDateFmt[] = "%d %%s %Y %H:%M:%S %z" MX_CRLF;
            time_t now = time(0);
            char datefmt[80];
            struct tm* tm;
#if   defined(NCBI_OS_SOLARIS)
            /* MT safe */
            tm = localtime(&now);
#elif defined(HAVE_LOCALTIME_R)
            struct tm tmp;
            localtime_r(&now, tm = &tmp);
#else
            struct tm tmp;
            CORE_LOCK_WRITE;
            tm = (struct tm*) memcpy(&tmp, localtime(&now), sizeof(tmp));
            CORE_UNLOCK;
#endif /*NCBI_OS_SOLARIS*/
            if (strftime(datefmt, sizeof(datefmt), kDateFmt, tm)) {
                sprintf(buffer, datefmt, kMonth[tm->tm_mon]);
                if (!s_SockWrite(sock, "Date: ", 6)  ||
                    !s_SockWrite(sock, buffer, 0)) {
                    SENDMAIL_RETURN(32, "Write error in sending Date");
                }
            }
            if (*info->from) {
                if (!s_SockWrite(sock, "From: ", 6)    ||
                    !s_SockWrite(sock, info->from, 0)  ||
                    !s_SockWrite(sock, MX_CRLF, sizeof(MX_CRLF)-1)) {
                    SENDMAIL_RETURN(33, "Write error in sending From");
                }
            }
        }
        if (subject) {
            if (!s_SockWrite(sock, "Subject: ", 9)              ||
                (*subject  &&  !s_SockWrite(sock, subject, 0))  ||
                !s_SockWrite(sock, MX_CRLF, sizeof(MX_CRLF)-1)) {
                SENDMAIL_RETURN(17, "Write error in sending Subject");
            }
        }
        if (to  &&  *to) {
            if (!s_SockWrite(sock, "To: ", 4)  ||
                !s_SockWrite(sock, to, 0)      ||
                !s_SockWrite(sock, MX_CRLF, sizeof(MX_CRLF)-1)) {
                SENDMAIL_RETURN(18, "Write error in sending To");
            }
        }
        if (info->cc  &&  *info->cc) {
            if (!s_SockWrite(sock, "Cc: ", 4)    ||
                !s_SockWrite(sock, info->cc, 0)  ||
                !s_SockWrite(sock, MX_CRLF, sizeof(MX_CRLF)-1)) {
                SENDMAIL_RETURN(19, "Write error in sending Cc");
            }
        }
    } else if (subject  &&  *subject) {
        CORE_LOG_X(2, eLOG_Warning,
                   "[SendMail]  Subject ignored in as-is messages");
    }

    if (!s_SockWrite(sock, "X-Mailer: CORE_SendMail (NCBI "
#ifdef NCBI_CXX_TOOLKIT
                     "CXX Toolkit"
#else
                     "C Toolkit"
#endif /*NCBI_CXX_TOOLKIT*/
                     ")" MX_CRLF, 0)) {
        SENDMAIL_RETURN(20, "Write error in sending mailer information");
    }

    assert(sizeof(buffer) > sizeof(MX_CRLF)  &&  sizeof(MX_CRLF) >= 3);

    if (info->header  &&  *info->header) {
        size_t n = 0, m = strlen(info->header);
        int/*bool*/ newline = 0/*false*/;
        while (n < m) {
            size_t k = 0;
            if (SOCK_Wait(sock, eIO_Read, &zero) != eIO_Timeout)
                break;
            while (k < sizeof(buffer) - sizeof(MX_CRLF)) {
                if (info->header[n] == '\n') {
                    memcpy(&buffer[k], MX_CRLF, sizeof(MX_CRLF)-1);
                    k += sizeof(MX_CRLF)-1;
                    newline = 1/*true*/;
                } else {
                    if (info->header[n] != '\r'  ||  info->header[n+1] != '\n')
                        buffer[k++] = info->header[n];
                    newline = 0/*false*/;
                }
                if (++n >= m)
                    break;
            }
            buffer[k] = '\0'/*just in case*/;
            if (!s_SockWrite(sock, buffer, k))
                SENDMAIL_RETURN(21, "Write error while sending custom header");
        }
        if (n < m)
            SENDMAIL_RETURN(22, "Header write error");
        if (!newline  &&  !s_SockWrite(sock, MX_CRLF, sizeof(MX_CRLF)-1))
            SENDMAIL_RETURN(23, "Write error while finalizing custom header");
    }

    if (body) {
        size_t n = 0, m = info->body_size ? info->body_size : strlen(body);
        int/*bool*/ newline = 0/*false*/;
        if (!(info->mx_options & fSendMail_NoMxHeader)  &&  m) {
            if (!s_SockWrite(sock, MX_CRLF, sizeof(MX_CRLF)-1))
                SENDMAIL_RETURN(24, "Write error in message body delimiter");
        }
        while (n < m) {
            size_t k = 0;
            if (SOCK_Wait(sock, eIO_Read, &zero) != eIO_Timeout)
                break;
            while (k < sizeof(buffer) - sizeof(MX_CRLF)) {
                if (body[n] == '\n') {
                    memcpy(&buffer[k], MX_CRLF, sizeof(MX_CRLF)-1);
                    k += sizeof(MX_CRLF)-1;
                    newline = 1/*true*/;
                } else {
                    if (body[n] != '\r'  ||  (n+1 < m  &&  body[n+1] != '\n')){
                        if (body[n] == '.'  &&  (newline  ||  !n)) {
                            buffer[k++] = '.';
                            buffer[k++] = '.';
                        } else
                            buffer[k++] = body[n];
                    }
                    newline = 0/*false*/;
                }
                if (++n >= m)
                    break;
            }
            buffer[k] = '\0'/*just in case*/;
            if (!s_SockWrite(sock, buffer, k))
                SENDMAIL_RETURN(25, "Write error while sending message body");
        }
        if (n < m)
            SENDMAIL_RETURN(26, "Body write error");
        if ((!newline  &&  m  &&  !s_SockWrite(sock,MX_CRLF,sizeof(MX_CRLF)-1))
            ||  !s_SockWrite(sock, "." MX_CRLF, sizeof(MX_CRLF))) {
            SENDMAIL_RETURN(27, "Write error while finalizing message body");
        }
    } else if (!s_SockWrite(sock, "." MX_CRLF, sizeof(MX_CRLF)))
        SENDMAIL_RETURN(28, "Write error while finalizing message");

    (void) SOCK_SetCork(sock, eOff);

    if (!SENDMAIL_READ_RESPONSE(250, 0, buffer))
        SENDMAIL_RETURN2(29, "Protocol error in sending message", buffer);

    if (!s_SockWrite(sock, "QUIT" MX_CRLF, 4 + sizeof(MX_CRLF)-1))
        SENDMAIL_RETURN(30, "Write error in QUIT command");
    if (!SENDMAIL_READ_RESPONSE(221, 0, buffer))
        SENDMAIL_RETURN2(31, "Protocol error in QUIT command", buffer);

    SOCK_Close(sock);
    return 0;
}

#undef SENDMAIL_READ_RESPONSE
#undef SENDMAIL_SENDRCPT
#undef SENDMAIL_RETURN2
#undef SENDMAIL_RETURN
