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
 *    Send mail
 *
 * --------------------------------------------------------------------------
 * $Log$
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
 * ==========================================================================
 */

#include "ncbi_priv.h"
#include <connect/ncbi_sendmail.h>
#include <connect/ncbi_socket.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#ifdef NCBI_OS_UNIX
#include <pwd.h>
#include <unistd.h>
#endif


#define MX_MAGIC_NUMBER 0xBABADEDA
#define MX_CRLF         "\r\n"

#define SMTP_READERR    -1      /* Error reading from socket         */
#define SMTP_REPLYERR   -2      /* Error reading reply prefix        */
#define SMTP_BADREPLY   -4      /* Malformed reply text              */
#define SMTP_BADCODE    -3      /* Reply code doesn't match in lines */
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

    if (!reply || !reply_len)
        return 0;

    do {
        size_t m = 0;
        char buf[4];

        if (SOCK_Read(sock, buf, 4, &m, eIO_Persist) != eIO_Success)
            return SMTP_READERR;
        if (m != 4)
            return SMTP_REPLYERR;

        if (buf[3] == '-' || (done = isspace((unsigned char)buf[3]))) {
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
            if (SOCK_Read(sock, buf, 1, &m, eIO_Plain) != eIO_Success || !m)
                return SMTP_READERR;

            if (buf[0] != '\r' && n < reply_len)
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
                                      char* buffer, size_t buffer_len)
{
    int c = s_SockRead(sock, buffer, buffer_len);
    if (c <= 0) {
        const char* message = 0;
        switch (c) {
        case SMTP_READERR:
            message = "Read error";
            break;
        case SMTP_REPLYERR:
            message = "Error reading reply prefix";
            break;
        case SMTP_BADREPLY:
            message = "Malformed reply text";
            break;
        case SMTP_BADCODE:
            message = "Reply code doesn't match in lines";
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
        strncpy(buffer, message, buffer_len - 1);
        buffer[buffer_len - 1] = '\0';
    } else if (c == code || (alt_code && c == alt_code))
        return 1/*success*/;
    return 0/*failure*/;
}


static int/*bool*/ s_SockWrite(SOCK sock, const char* buf)
{
    size_t len = strlen(buf);
    size_t n;

    if (SOCK_Write(sock, buf, len, &n) != eIO_Success || n != len)
        return 0/*failed*/;
    return 1/*success*/;
}


static char* s_ComposeFrom(char* buf, size_t buf_size)
{
    size_t buf_len, hostname_len;
#ifdef NCBI_OS_UNIX
    struct passwd *pwd;
    /* Get the user login name. FIXME: not MT-safe */
    const char* login_name = getlogin();
    if (!login_name) {
        pwd = getpwuid(getuid());
        if (!pwd) {
            if (!(login_name = getenv("USER")) &&
                !(login_name = getenv("LOGNAME")))
                return 0;
        } else
            login_name = pwd->pw_name;
    }
#else
    /* Temporary solution for login name */
    const char* login_name = "anonymous";
#endif
    strncpy(buf, login_name, buf_size - 1);
    buf[buf_size - 1] = '\0';
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
    info->magic_number = MX_MAGIC_NUMBER;
    info->cc = 0;
    info->bcc = 0;
    if (!s_ComposeFrom(info->from, sizeof(info->from)))
        info->from[0] = 0;
    info->header = 0;
    info->mx_host = MX_HOST;
    info->mx_port = MX_PORT;
    info->mx_timeout.sec = MX_TIMEOUT;
    info->mx_timeout.usec = 0;
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
 * (well, it thinks the "condition" is there, dumb!), if we
 * used 'return' right before the end of 'while' statement.
 * So we now added "check" and "conditional" exit, which makes
 * the Sun compiler much happier, and less wordy :-)
 */

#define SENDMAIL_RETURN(reason)                                            \
    do {                                                                   \
        CORE_LOGF(eLOG_Error, ("[SendMail]  %s", reason));                 \
        if (reason/*always true, though, to trick "smart" compiler*/)      \
            return reason;                                                 \
    } while (0)

#define SENDMAIL_RETURN2(reason, explanation)                              \
    do {                                                                   \
       CORE_LOGF(eLOG_Error, ("[SendMail]  %s: %s", reason, explanation)); \
       if (reason/*always true, though, to trick "smart" compiler*/)       \
           return reason;                                                  \
    } while (0)

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
    SOCK sock;

    info = uinfo ? uinfo : SendMailInfo_Init(&ainfo);
    if (info->magic_number != MX_MAGIC_NUMBER)
        SENDMAIL_RETURN("Invalid magic number");

    if ((!to || !*to) &&
        (!info->cc || !*info->cc) &&
        (!info->bcc || !*info->bcc))
        SENDMAIL_RETURN("At least one message recipient must be specified");
    
    /* Open connection to sendmail */
    if (SOCK_Create(info->mx_host, info->mx_port, &info->mx_timeout, &sock)
        != eIO_Success)
        SENDMAIL_RETURN("Cannot connect to sendmail");
    SOCK_SetTimeout(sock, eIO_ReadWrite, &info->mx_timeout);
    
    /* Follow the protocol conversation, RFC821 */
    if (!SENDMAIL_READ_RESPONSE(220, 0, buffer))
        SENDMAIL_RETURN2("Protocol error in connection init", buffer);
    
    if (SOCK_gethostname(buffer, sizeof(buffer)) != 0)
        SENDMAIL_RETURN("Unable to get local host name");
    if (!s_SockWrite(sock, "HELO ") ||
        !s_SockWrite(sock, buffer) ||
        !s_SockWrite(sock, MX_CRLF))
        SENDMAIL_RETURN("Write error in HELO command");
    if (!SENDMAIL_READ_RESPONSE(250, 0, buffer))
        SENDMAIL_RETURN2("Protocol error in HELO command", buffer);
    
    if (!s_SockWrite(sock, "MAIL FROM: <") ||
        !s_SockWrite(sock, info->from) ||
        !s_SockWrite(sock, ">" MX_CRLF))
        SENDMAIL_RETURN("Write error in MAIL command");
    if (!SENDMAIL_READ_RESPONSE(250, 0, buffer))
        SENDMAIL_RETURN2("Protocol error in MAIL command", buffer);

    if (to && *to) {
        if (!s_SockWrite(sock, "RCPT TO: <") ||
            !s_SockWrite(sock, to) ||
            !s_SockWrite(sock, ">" MX_CRLF))
            SENDMAIL_RETURN("Write error in RCPT (To) command");
        if (!SENDMAIL_READ_RESPONSE(250, 251, buffer))
            SENDMAIL_RETURN2("Protocol error in RCPT (To) command", buffer);
    }

    if (info->cc && *info->cc) {
        if (!s_SockWrite(sock, "RCPT TO: <") ||
            !s_SockWrite(sock, info->cc) ||
            !s_SockWrite(sock, ">" MX_CRLF))
            SENDMAIL_RETURN("Write error in RCPT (Cc) command");
        if (!SENDMAIL_READ_RESPONSE(250, 251, buffer))
            SENDMAIL_RETURN2("Protocol error in RCPT (Cc) command", buffer);
    }

    if (info->bcc && *info->bcc) {
        if (!s_SockWrite(sock, "RCPT TO: <") ||
            !s_SockWrite(sock, info->bcc) ||
            !s_SockWrite(sock, ">" MX_CRLF))
            SENDMAIL_RETURN("Write error in RCPT (Bcc) command");
        if (!SENDMAIL_READ_RESPONSE(250, 251, buffer))
            SENDMAIL_RETURN2("Protocol error in RCPT (Bcc) command", buffer);
    }
    
    if (!s_SockWrite(sock, "DATA" MX_CRLF))
        SENDMAIL_RETURN("Write error in DATA command");
    if (!SENDMAIL_READ_RESPONSE(354, 0, buffer))
        SENDMAIL_RETURN2("Protocol error in DATA command", buffer);
    
    /* Follow RFC822 to compose message headers. Note that
     * 'Date:'and 'From:' are both added by sendmail automatically.
     */ 
    if (!s_SockWrite(sock, "Subject: ") ||
        (subject && !s_SockWrite(sock, subject)) ||
        !s_SockWrite(sock, MX_CRLF))
        SENDMAIL_RETURN("Write error in sending subject");

    if (to && *to) {
        if (!s_SockWrite(sock, "To: ") ||
            !s_SockWrite(sock, to) ||
            !s_SockWrite(sock, MX_CRLF))
            SENDMAIL_RETURN("Write error in sending To");
    }

    if (info->cc && *info->cc) {
        if (!s_SockWrite(sock, "Cc: ") ||
            !s_SockWrite(sock, info->cc) ||
            !s_SockWrite(sock, MX_CRLF))
            SENDMAIL_RETURN("Write error in sending Cc");
    }

    if (!s_SockWrite(sock,
                     "X-Mailer: CORE_SendMail (NCBI "
                     NCBI_SENDMAIL_TOOLKIT " Toolkit)" MX_CRLF))
        SENDMAIL_RETURN("Write error in sending mailer information");

    assert(sizeof(buffer) > sizeof(MX_CRLF) && sizeof(MX_CRLF) >= 3);

    if (info->header && *info->header) {
        int/*bool*/ newline = 0/*false*/;
        size_t n = 0, m = strlen(info->header);
        while (n < m) {
            size_t k = 0;
            while (k < sizeof(buffer) - sizeof(MX_CRLF)) {
                if (info->header[n] == '\n') {
                    memcpy(&buffer[k], MX_CRLF, sizeof(MX_CRLF) - 1);
                    k += sizeof(MX_CRLF) - 1;
                    newline = 1/*true*/;
                } else {
                    buffer[k++] = info->header[n];
                    newline = 0/*false*/;
                }
                if (++n >= m)
                    break;
            }
            buffer[k] = 0;
            if (!s_SockWrite(sock, buffer))
                SENDMAIL_RETURN("Write error in sending custom header");
        }
        if (!newline && !s_SockWrite(sock, MX_CRLF))
            SENDMAIL_RETURN("Write error in finalizing custom header");
    }
    
    if (body && *body) {
        int/*bool*/ newline = 0/*false*/;
        size_t n = 0, m = strlen(body);
        
        if (!s_SockWrite(sock, MX_CRLF))
            SENDMAIL_RETURN("Write error in sending text body delimiter");
        while (n < m) {
            size_t k = 0;
            
            while (k < sizeof(buffer) - sizeof(MX_CRLF)) {
                if (body[n] == '\n') {
                    memcpy(&buffer[k], MX_CRLF, sizeof(MX_CRLF) - 1);
                    k += sizeof(MX_CRLF) - 1;
                    newline = 1/*true*/;
                } else {
                    if (body[n] == '.' && (newline || !n)) {
                        buffer[k++] = '.';
                        buffer[k++] = '.';
                    } else
                        buffer[k++]   = body[n];
                    newline = 0/*false*/;
                }
                if (++n >= m)
                    break;
            }
            buffer[k] = 0;
            if (!s_SockWrite(sock, buffer))
                SENDMAIL_RETURN("Write error in sending text body");
        }
        if ((!newline && !s_SockWrite(sock, MX_CRLF)) ||
            !s_SockWrite(sock, "." MX_CRLF))
            SENDMAIL_RETURN("Write error in finishing text body");
    } else if (!s_SockWrite(sock, MX_CRLF "." MX_CRLF))
        SENDMAIL_RETURN("Write error in finishing message");
    if (!SENDMAIL_READ_RESPONSE(250, 0, buffer))
        SENDMAIL_RETURN2("Protocol error in sending message body", buffer);
    
    if (!s_SockWrite(sock, "QUIT" MX_CRLF))
        SENDMAIL_RETURN("Write error in QUIT command");
    if (!SENDMAIL_READ_RESPONSE(221, 0, buffer))
        SENDMAIL_RETURN2("Protocol error in QUIT command", buffer);
    
    SOCK_Close(sock);
    return 0;
}

#undef SENDMAIL_READ_RESPONSE
#undef SENDMAIL_RETURN2
#undef SENDMAIL_RETURN
