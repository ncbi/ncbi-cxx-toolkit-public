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
 *   Send mail
 *
 */

#include "ncbi_ansi_ext.h"
#include "ncbi_priv.h"
#include <connect/ncbi_sendmail.h>
#include <connect/ncbi_socket.h>
#include <ctype.h>
#include <stdlib.h>
#ifdef NCBI_OS_UNIX
#include <pwd.h>
#include <unistd.h>
#endif
#ifdef NCBI_CXX_TOOLKIT
#define NCBI_SENDMAIL_TOOLKIT "C++"
#else
#define NCBI_SENDMAIL_TOOLKIT "C"
#endif

#define MX_MAGIC_NUMBER 0xBA8ADEDA
#define MX_CRLF         "\r\n"

#define SMTP_READERR    -1      /* Error reading from socket         */
#define SMTP_REPLYERR   -2      /* Error reading reply prefix        */
#define SMTP_BADCODE    -3      /* Reply code doesn't match in lines */
#define SMTP_BADREPLY   -4      /* Malformed reply text              */
#define SMTP_NOCODE     -5      /* No reply code detected (letters?) */
#define SMTP_WRITERR    -6      /* Error writing to socket           */


/* Read SMTP reply from the socket.
 * Return reply in the buffer provided,
 * and reply code (positive value) as a return value.
 * Return a negative code in case of problem (protocol reply
 * read error or protocol violations).
 * Return 0 in case of call error.
 */
static int s_SockRead(SOCK sock, char* reply, size_t reply_len)
{
    int/*bool*/ done = 0;
    size_t n = 0;
    int code = 0;

    if (!reply  ||  !reply_len)
        return 0;

    do {
        size_t m = 0;
        char buf[4];

        if (SOCK_Read(sock, buf, 4, &m, eIO_ReadPersist) != eIO_Success)
            return SMTP_READERR;
        if (m != 4)
            return SMTP_REPLYERR;

        if (buf[3] == '-'  ||  (done = isspace((unsigned char) buf[3]))) {
            buf[3] = 0;
            if (!code) {
                if (!(code = atoi(buf)))
                    return SMTP_NOCODE;
            } else if (code != atoi(buf))
                return SMTP_BADCODE;
        } else
            return SMTP_BADREPLY;

        do {
            m = 0;
            if (SOCK_Read(sock,buf,1,&m,eIO_ReadPlain) != eIO_Success  ||  !m)
                return SMTP_READERR;

            if (buf[0] != '\r'  &&  n < reply_len)
                reply[n++] = buf[0];
        } while (buf[0] != '\n');

        /* At least '\n' should sit in buffer */
        assert(n);
        if (done)
            reply[n - 1] = 0;
        else if (n < reply_len)
            reply[n] = ' ';
        
    } while (!done);

    assert(code);
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
        case SMTP_REPLYERR:
            message = "Error reading reply prefix";
            break;
        case SMTP_BADCODE:
            message = "Reply code doesn't match in lines";
            break;
        case SMTP_BADREPLY:
            message = "Malformed reply text";
            break;
        case SMTP_NOCODE:
            message = "No reply code detected";
            break;
        case SMTP_WRITERR:
            message = "Write error";
            break;
        default:
            message = "Unknown error";
            break;
        }
        assert(message);
        strncpy0(buf, message, buf_size - 1);
    } else if (c == code  ||  (alt_code  &&  c == alt_code))
        return 1/*success*/;
    return 0/*failure*/;
}


static int/*bool*/ s_SockWrite(SOCK sock, const char* buf)
{
    size_t len = strlen(buf);
    size_t n;

    if (SOCK_Write(sock, buf, len, &n, eIO_WritePersist) == eIO_Success  &&
        n == len) {
        return 1/*success*/;
    }
    return 0/*failure*/;
}


static char* s_ComposeFrom(char* buf, size_t buf_size)
{
    size_t buf_len, hostname_len;
#ifdef NCBI_OS_UNIX
    /* Get the user login name. FIXME: not MT-safe */
    const char* login_name;
    CORE_LOCK_WRITE;
    login_name = getlogin();
    if (!login_name) {
        struct passwd* pwd = getpwuid(getuid());
        if (!pwd) {
            if (!(login_name = getenv("USER"))  &&
                !(login_name = getenv("LOGNAME"))) {
                CORE_UNLOCK;
                return 0;
            }
        } else
            login_name = pwd->pw_name;
    }
#else
    /* Temporary solution for login name */
    const char* login_name = "anonymous";
#  ifdef NCBI_OS_MSWIN
    const char* user_name = getenv("USERNAME");
    if (user_name)
        login_name = user_name;
#  endif
#endif
    strncpy0(buf, login_name, buf_size - 1);
#ifdef NCBI_OS_UNIX
    CORE_UNLOCK;
#endif
    buf_len = strlen(buf);
    hostname_len = buf_size - buf_len;
    if (hostname_len-- > 1) {
        buf[buf_len++] = '@';
        SOCK_gethostname(&buf[buf_len], hostname_len);
    }
    return buf;
}


SSendMailInfo* SendMailInfo_Init(SSendMailInfo* info)
{
    if (info) {
        info->magic_number    = MX_MAGIC_NUMBER;
        info->cc              = 0;
        info->bcc             = 0;
        if (!s_ComposeFrom(info->from, sizeof(info->from)))
            info->from[0]     = 0;
        info->header          = 0;
        info->body_size       = 0;
        info->mx_host         = MX_HOST;
        info->mx_port         = MX_PORT;
        info->mx_timeout.sec  = MX_TIMEOUT;
        info->mx_timeout.usec = 0;
        info->mx_no_header    = 0/*false*/;
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
 * C compiler warned about unreachable end-of-loop condition
 * (well, it thinks "a condition" is there, dumb!), if we
 * used 'return' right before the end of 'while' statement.
 * So we now added "check" and "conditional" exit, which makes
 * the Sun compiler much happier, and less wordy :-)
 */

#define SENDMAIL_RETURN(reason)                                            \
    do {                                                                   \
        if (sock)                                                          \
            SOCK_Close(sock);                                              \
        CORE_LOGF(eLOG_Error, ("[SendMail]  %s", reason));                 \
        if (reason/*always true, though, to trick "smart" compiler*/)      \
            return reason;                                                 \
    } while (0)

#define SENDMAIL_RETURN2(reason, explanation)                              \
    do {                                                                   \
       if (sock)                                                           \
           SOCK_Close(sock);                                               \
       CORE_LOGF(eLOG_Error, ("[SendMail]  %s: %s", reason, explanation)); \
       if (reason/*always true, though, to trick "smart" compiler*/)       \
           return reason;                                                  \
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
            } else if (c == '"'  ||  c == '<'  ||  c == '\'') {
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
            SENDMAIL_RETURN("Recepient address is too long");
        buf[k] = 0;
        if (quote) {
            CORE_LOGF(eLOG_Warning, ("[SendMail]  Ubalanced delimiters in "
                                     "recepient %s for %s: \"%c\" expected",
                                     buf, what, quote));
        }
        if (!s_SockWrite(sock, "RCPT TO: <")  ||
            !s_SockWrite(sock, buf)           ||
            !s_SockWrite(sock, ">" MX_CRLF)) {
            SENDMAIL_RETURN(write_error);
        }
        if (!s_SockReadResponse(sock, 250, 251, buf, buf_size))
            SENDMAIL_RETURN2(proto_error, buf);
        if (!c)
            break;
    }
    return 0;
}


#define SENDMAIL_SENDRCPT(what, list, buffer)                              \
    s_SendRcpt(sock, list, buffer, sizeof(buffer), what,                   \
               "Write error in RCPT (" what ") command",                   \
               "Protocol error in RCPT (" what ") command")

#define SENDMAIL_READ_RESPONSE(code, altcode, buffer)                      \
    s_SockReadResponse(sock, code, altcode, buffer, sizeof(buffer))


const char* CORE_SendMailEx(const char*          to,
                            const char*          subject,
                            const char*          body,
                            const SSendMailInfo* uinfo)
{
    const SSendMailInfo* info;
    SSendMailInfo ainfo;
    char buffer[1024];
    SOCK sock = 0;

    info = uinfo ? uinfo : SendMailInfo_Init(&ainfo);
    if (info->magic_number != MX_MAGIC_NUMBER)
        SENDMAIL_RETURN("Invalid magic number");

    if ((!to         ||  !*to)        &&
        (!info->cc   ||  !*info->cc)  &&
        (!info->bcc  ||  !*info->bcc)) {
        SENDMAIL_RETURN("At least one message recipient must be specified");
    }

    /* Open connection to sendmail */
    if (SOCK_Create(info->mx_host, info->mx_port, &info->mx_timeout, &sock)
        != eIO_Success) {
        SENDMAIL_RETURN("Cannot connect to sendmail");
    }
    SOCK_SetTimeout(sock, eIO_ReadWrite, &info->mx_timeout);

    /* Follow the protocol conversation, RFC821 */
    if (!SENDMAIL_READ_RESPONSE(220, 0, buffer))
        SENDMAIL_RETURN2("Protocol error in connection init", buffer);

    if (SOCK_gethostname(buffer, sizeof(buffer)) != 0)
        SENDMAIL_RETURN("Unable to get local host name");
    if (!s_SockWrite(sock, "HELO ")         ||
        !s_SockWrite(sock, buffer)          ||
        !s_SockWrite(sock, MX_CRLF)) {
        SENDMAIL_RETURN("Write error in HELO command");
    }
    if (!SENDMAIL_READ_RESPONSE(250, 0, buffer))
        SENDMAIL_RETURN2("Protocol error in HELO command", buffer);

    if (!s_SockWrite(sock, "MAIL FROM: <")  ||
        !s_SockWrite(sock, info->from)      ||
        !s_SockWrite(sock, ">" MX_CRLF)) {
        SENDMAIL_RETURN("Write error in MAIL command");
    }
    if (!SENDMAIL_READ_RESPONSE(250, 0, buffer))
        SENDMAIL_RETURN2("Protocol error in MAIL command", buffer);

    if (to && *to) {
        const char* error = SENDMAIL_SENDRCPT("To", to, buffer);
        if (error)
            return error;
    }

    if (info->cc && *info->cc) {
        const char* error = SENDMAIL_SENDRCPT("Cc", info->cc, buffer);
        if (error)
            return error;
    }

    if (info->bcc && *info->bcc) {
        const char* error = SENDMAIL_SENDRCPT("Bcc", info->bcc, buffer);
        if (error)
            return error;
    }

    if (!s_SockWrite(sock, "DATA" MX_CRLF))
        SENDMAIL_RETURN("Write error in DATA command");
    if (!SENDMAIL_READ_RESPONSE(354, 0, buffer))
        SENDMAIL_RETURN2("Protocol error in DATA command", buffer);

    if (!info->mx_no_header) {
        /* Follow RFC822 to compose message headers. Note that
         * 'Date:'and 'From:' are both added by sendmail automatically.
         */ 
        if (!s_SockWrite(sock, "Subject: ")           ||
            (subject && !s_SockWrite(sock, subject))  ||
            !s_SockWrite(sock, MX_CRLF))
            SENDMAIL_RETURN("Write error in sending subject");

        if (to && *to) {
            if (!s_SockWrite(sock, "To: ")            ||
                !s_SockWrite(sock, to)                ||
                !s_SockWrite(sock, MX_CRLF))
                SENDMAIL_RETURN("Write error in sending To");
        }

        if (info->cc && *info->cc) {
            if (!s_SockWrite(sock, "Cc: ")            ||
                !s_SockWrite(sock, info->cc)          ||
                !s_SockWrite(sock, MX_CRLF))
                SENDMAIL_RETURN("Write error in sending Cc");
        }
    } else if (subject && *subject)
        CORE_LOG(eLOG_Warning,"[SendMail]  Subject ignored in as-is messages");

    if (!s_SockWrite(sock, "X-Mailer: CORE_SendMail (NCBI "
                     NCBI_SENDMAIL_TOOLKIT " Toolkit)" MX_CRLF)) {
        SENDMAIL_RETURN("Write error in sending mailer information");
    }

    assert(sizeof(buffer) > sizeof(MX_CRLF) && sizeof(MX_CRLF) >= 3);

    if (info->header && *info->header) {
        size_t n = 0, m = strlen(info->header);
        int/*bool*/ newline = 0/*false*/;
        while (n < m) {
            size_t k = 0;
            while (k < sizeof(buffer) - sizeof(MX_CRLF)) {
                if (info->header[n] == '\n') {
                    memcpy(&buffer[k], MX_CRLF, sizeof(MX_CRLF) - 1);
                    k += sizeof(MX_CRLF) - 1;
                    newline = 1/*true*/;
                } else {
                    if (info->header[n] != '\r'  ||  info->header[n+1] != '\n')
                        buffer[k++] = info->header[n];
                    newline = 0/*false*/;
                }
                if (++n >= m)
                    break;
            }
            buffer[k] = 0;
            if (!s_SockWrite(sock, buffer))
                SENDMAIL_RETURN("Write error while sending custom header");
        }
        if (!newline && !s_SockWrite(sock, MX_CRLF))
            SENDMAIL_RETURN("Write error while finalizing custom header");
    }

    if (body) {
        size_t n = 0, m = info->body_size ? info->body_size : strlen(body);
        int/*bool*/ newline = 0/*false*/;
        if (!info->mx_no_header  &&  m) {
            if (!s_SockWrite(sock, MX_CRLF))
                SENDMAIL_RETURN("Write error in message body delimiter");
        }
        while (n < m) {
            size_t k = 0;
            while (k < sizeof(buffer) - sizeof(MX_CRLF)) {
                if (body[n] == '\n') {
                    memcpy(&buffer[k], MX_CRLF, sizeof(MX_CRLF) - 1);
                    k += sizeof(MX_CRLF) - 1;
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
            buffer[k] = 0;
            if (!s_SockWrite(sock, buffer))
                SENDMAIL_RETURN("Write error while sending message body");
        }
        if ((!newline  &&  m  &&  !s_SockWrite(sock, MX_CRLF))
            ||  !s_SockWrite(sock, "." MX_CRLF)) {
            SENDMAIL_RETURN("Write error while finalizing message body");
        }
    } else if (!s_SockWrite(sock, "." MX_CRLF))
        SENDMAIL_RETURN("Write error while finalizing message");
    if (!SENDMAIL_READ_RESPONSE(250, 0, buffer))
        SENDMAIL_RETURN2("Protocol error in sending message", buffer);

    if (!s_SockWrite(sock, "QUIT" MX_CRLF))
        SENDMAIL_RETURN("Write error in QUIT command");
    if (!SENDMAIL_READ_RESPONSE(221, 0, buffer))
        SENDMAIL_RETURN2("Protocol error in QUIT command", buffer);

    SOCK_Close(sock);
    return 0;
}

#undef SENDMAIL_READ_RESPONSE
#undef SENDMAIL_SENDRCPT
#undef SENDMAIL_RETURN2
#undef SENDMAIL_RETURN


/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.23  2005/03/18 16:35:39  lavr
 * Fix \r\n parse bug in both header and body
 *
 * Revision 6.22  2004/01/07 19:51:36  lavr
 * Try to obtain user name from USERNAME env.var. on Windows, else fallback
 *
 * Revision 6.21  2003/12/09 15:38:39  lavr
 * Take advantage of SSendMailInfo::body_size;  few little makeup changes
 *
 * Revision 6.20  2003/12/04 14:55:09  lavr
 * Extend API with no-header and multiple recipient capabilities
 *
 * Revision 6.19  2003/04/18 20:59:51  lavr
 * Mixed up SMTP_BADCODE and SMTP_BADREPLY rearranged in order
 *
 * Revision 6.18  2003/03/24 19:46:17  lavr
 * Few minor changes; do not init SSendMailInfo passed as NULL
 *
 * Revision 6.17  2003/02/08 21:05:55  lavr
 * Unimportant change in comments
 *
 * Revision 6.16  2002/10/28 15:43:29  lavr
 * Use "ncbi_ansi_ext.h" privately and use strncpy0()
 *
 * Revision 6.15  2002/09/24 15:05:45  lavr
 * Log moved to end
 *
 * Revision 6.14  2002/08/14 18:55:39  lavr
 * Close socket on error return (was forgotten)
 *
 * Revision 6.13  2002/08/12 15:12:31  lavr
 * Use persistent SOCK_Write()
 *
 * Revision 6.12  2002/08/07 16:33:15  lavr
 * Changed EIO_ReadMethod enums accordingly; log moved to end
 *
 * Revision 6.11  2002/02/11 20:36:44  lavr
 * Use "ncbi_config.h"
 *
 * Revision 6.10  2001/07/13 20:15:12  lavr
 * Write lock then unlock when using not MT-safe s_ComposeFrom()
 *
 * Revision 6.9  2001/05/18 20:41:43  lavr
 * Beautifying: change log corrected
 *
 * Revision 6.8  2001/05/18 19:52:24  lavr
 * Tricks in macros to keep Sun C compiler silent from warnings (details below)
 *
 * Revision 6.7  2001/03/26 18:39:24  lavr
 * Casting to (unsigned char) instead of (int) for ctype char.class macros
 *
 * Revision 6.6  2001/03/06 04:32:00  lavr
 * Better custom header processing
 *
 * Revision 6.5  2001/03/02 20:09:06  lavr
 * Typo fixed
 *
 * Revision 6.4  2001/03/01 00:30:23  lavr
 * Toolkit configuration moved to ncbi_sendmail_.c
 *
 * Revision 6.3  2001/02/28 21:11:47  lavr
 * Bugfix: buffer overrun
 *
 * Revision 6.2  2001/02/28 17:48:53  lavr
 * Some fixes; larger intermediate buffer for message body
 *
 * Revision 6.1  2001/02/28 00:52:26  lavr
 * Initial revision
 *
 * ===========================================================================
 */
