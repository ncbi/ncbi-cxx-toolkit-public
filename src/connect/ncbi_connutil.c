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
 * Author:  Denis Vakatov
 *
 * File Description:
 *   Auxiliary API, mostly CONN-, URL-, and MIME-related
 *   (see in "ncbi_connutil.h" for more details).
 *
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.19  2001/04/24 21:29:43  lavr
 * Special text value "infinite" accepted as infinite timeout from environment
 *
 * Revision 6.18  2001/03/26 18:37:09  lavr
 * #include <ctype.h> not used, removed
 *
 * Revision 6.17  2001/03/02 20:08:05  lavr
 * Typo fixed
 *
 * Revision 6.16  2001/01/25 16:58:33  lavr
 * ConnNetInfo_SetUserHeader now used throughout to set/reset http_user_header
 *
 * Revision 6.15  2001/01/23 23:06:18  lavr
 * SConnNetInfo.debug_printout converted from boolean to enum
 * BUF_StripToPattern() introduced
 *
 * Revision 6.14  2001/01/12 00:01:27  lavr
 * CONN_PROXY_HOST was forgotten to init in ConnNetInfo_Create
 *
 * Revision 6.13  2001/01/11 23:07:08  lavr
 * ConnNetInfo_Print() prints user-header 'as is'; pretty-printing undone
 *
 * Revision 6.12  2001/01/08 23:46:27  lavr
 * REQUEST_METHOD -> REQ_METHOD to be consistent with SConnNetInfo
 *
 * Revision 6.11  2001/01/08 22:35:56  lavr
 * Client-Mode removed; replaced by 2 separate boolean fields:
 * stateless and firewall
 *
 * Revision 6.10  2000/12/29 17:54:11  lavr
 * NCBID stuff removed; ConnNetInfo_SetUserHeader added;
 * modifications to ConnNetInfo_Print output.
 *
 * Revision 6.9  2000/11/07 23:23:18  vakatov
 * In-sync with the C Toolkit "connutil.c:R6.15", "connutil.h:R6.13"
 * (with "eMIME_Dispatch" added).
 *
 * Revision 6.8  2000/10/20 17:08:40  lavr
 * All keys capitalized for registry access
 * Search for some keywords made case insensitive
 *
 * Revision 6.7  2000/10/11 22:29:44  lavr
 * Forgotten blank added after {GET|POST} request in URL_Connect
 *
 * Revision 6.6  2000/10/05 22:35:24  lavr
 * SConnNetInfo modified to contain 'client_mode' instead of just
 * 'firewall' boolean
 *
 * Revision 6.5  2000/09/27 19:37:40  lavr
 * ncbi_ansi_ext.h included
 *
 * Revision 6.4  2000/09/26 22:01:33  lavr
 * Registry entries changed, HTTP request method added
 *
 * Revision 6.3  2000/04/21 19:42:35  vakatov
 * Several minor typo/bugs fixed
 *
 * Revision 6.2  2000/03/29 16:36:09  vakatov
 * MIME_ParseContentType() -- a fix
 *
 * Revision 6.1  2000/03/24 22:53:34  vakatov
 * Initial revision
 *
 * ==========================================================================
 */

#include "ncbi_priv.h"
#include <connect/ncbi_socket.h>
#include <connect/ncbi_connection.h>
#include <connect/ncbi_connutil.h>
#include <connect/ncbi_ansi_ext.h>
#include <stdlib.h>
#include <string.h>


static const char* s_GetValue(const char* service, const char* param,
                              char* value, size_t value_size,
                              const char* def_value)
{
  char        key[250];
  char*       sec;
  const char* val;

  if (!value  ||  !param  ||  value_size <= 0)
      return 0;
  *value = '\0';

  if (service  &&  *service) {
      /* Service-specific inquiry */
      if (strlen(service) + 1 + sizeof(DEF_CONN_REG_SECTION) +
          strlen(param) + 1 > sizeof(key))
          return 0;
      /* First, environment search for 'service_CONN_param' */
      sprintf(key, "%s_" DEF_CONN_REG_SECTION "_%s", service, param);
      strupr(key);
      if ((val = getenv(key)) != 0) {
          strncpy(value, val, value_size);
          value[value_size - 1] = '\0';
          return value;
      }
      /* Next, search for 'CONN_param' in '[service]' registry section */
      sprintf(key, DEF_CONN_REG_SECTION "_%s", param);
      sec = key + strlen(key) + 1;
      strupr(key);
      strcpy(sec, service);
      strupr(sec);
      CORE_REG_GET(sec, key, value, value_size, 0);
      if ( *value ) {
          return value;
      }
  } else {
      /* Common case. Form 'CONN_param' */
      if (sizeof(DEF_CONN_REG_SECTION) + strlen(param) + 1 > sizeof(key))
          return 0;
      sprintf(key, DEF_CONN_REG_SECTION "_%s", param);
      strupr(key);
  }

  /* Environment search for 'CONN_param' */
  if ((val = getenv(key)) != 0) {
      strncpy(value, val, value_size);
      value[value_size - 1] = '\0';
      return value;
  }

  /* Last resort: Search for 'param' in default registry section */
  strcpy(key, param);
  strupr(key);
  CORE_REG_GET(DEF_CONN_REG_SECTION, key, value, value_size, def_value);

  return value;
}


/***********************************************************************
 *  EXTERNAL
 ***********************************************************************/


extern SConnNetInfo* ConnNetInfo_Create(const char* service)
{
#define REG_VALUE(name, value, def_value) \
    s_GetValue(service, name, value, sizeof(value), def_value)

    SConnNetInfo* info = (SConnNetInfo*) malloc(sizeof(SConnNetInfo));

    /* aux. storage for the string-to-int conversions, etc. */
    char   str[32];
    int    val;
    double dbl;

    /* client host */
    SOCK_gethostname(info->client_host, sizeof(info->client_host));

    /* dispatcher host name */
    REG_VALUE(REG_CONN_HOST, info->host, DEF_CONN_HOST);

    /* dispatcher port number */
    REG_VALUE(REG_CONN_PORT, str, 0);
    val = atoi(str);
    info->port = (unsigned short) (val > 0 ? val : DEF_CONN_PORT);

    /* service path */
    REG_VALUE(REG_CONN_PATH, info->path, DEF_CONN_PATH);

    /* service args */
    REG_VALUE(REG_CONN_ARGS, info->args, DEF_CONN_ARGS);

    /* request method */
    REG_VALUE(REG_CONN_REQ_METHOD, str, DEF_CONN_REQ_METHOD);
    if (strcasecmp(str, "ANY") == 0)
        info->req_method = eReqMethod_Any;
    else if (strcasecmp(str, "POST") == 0)
        info->req_method = eReqMethod_Post;
    else if (strcasecmp(str, "GET") == 0)
        info->req_method = eReqMethod_Get;

    /* connection timeout */
    REG_VALUE(REG_CONN_TIMEOUT, str, 0);
    if (*str && strncasecmp(str, "infinite", strlen(str)) == 0)
        info->timeout = 0;
    else {
        info->timeout = &info->tmo;
        dbl = atof(str);
        if (dbl <= 0.0)
            dbl = DEF_CONN_TIMEOUT;
        info->timeout->sec  = (unsigned int) dbl;
        info->timeout->usec = (unsigned int)
            ((dbl - info->timeout->sec) * 1000000);
    }
    
    /* max. # of attempts to establish a connection */
    REG_VALUE(REG_CONN_MAX_TRY, str, 0);
    val = atoi(str);
    info->max_try = (unsigned int) (val > 0 ? val : DEF_CONN_MAX_TRY);

    /* HTTP proxy server? */
    REG_VALUE(REG_CONN_HTTP_PROXY_HOST, info->http_proxy_host,
              DEF_CONN_HTTP_PROXY_HOST);
    if (*info->http_proxy_host) {
        /* yes, use the specified HTTP proxy server */
        REG_VALUE(REG_CONN_HTTP_PROXY_PORT, str, 0);
        val = atoi(str);
        info->http_proxy_port = (unsigned short)
            (val > 0 ? val : DEF_CONN_HTTP_PROXY_PORT);
    } else {
        info->http_proxy_port = DEF_CONN_HTTP_PROXY_PORT;
    }

    /* non-transparent CERN-like firewall proxy server? */
    REG_VALUE(REG_CONN_PROXY_HOST, info->proxy_host, DEF_CONN_PROXY_HOST);

    /* turn on debug printout? */
    REG_VALUE(REG_CONN_DEBUG_PRINTOUT, str, DEF_CONN_DEBUG_PRINTOUT);
    if (*str  &&
        (strcmp(str, "1") == 0 ||
         strcasecmp(str, "true") == 0 ||
         strcasecmp(str, "yes" ) == 0 ||
         strcasecmp(str, "some") == 0))
        info->debug_printout = eDebugPrintout_Some;
    else if (*str  &&  strcasecmp(str, "data") == 0)
        info->debug_printout = eDebugPrintout_Data;
    else 
        info->debug_printout = eDebugPrintout_None;

    /* stateless client? */
    REG_VALUE(REG_CONN_STATELESS, str, DEF_CONN_STATELESS);
    info->stateless = (*str  &&
                       (strcmp(str, "1") == 0  ||
                        strcasecmp(str, "true") == 0  ||
                        strcasecmp(str, "yes" ) == 0));

    /* firewall mode? */
    REG_VALUE(REG_CONN_FIREWALL, str, DEF_CONN_FIREWALL);
    info->firewall = (*str  &&
                      (strcmp(str, "1") == 0  ||
                       strcasecmp(str, "true") == 0  ||
                       strcasecmp(str, "yes" ) == 0));

    /* prohibit the use of local load balancer? */
    REG_VALUE(REG_CONN_LB_DISABLE, str, DEF_CONN_LB_DISABLE);
    info->lb_disable = (*str  &&
                        (strcmp(str, "1") == 0  ||
                         strcasecmp(str, "true") == 0  ||
                         strcasecmp(str, "yes" ) == 0));

    /* has no user header yet... */
    info->http_user_header = 0;
    /* not adjusted yet... */
    info->http_proxy_adjusted = 0/*false*/;

    /* done */
    return info;
#undef REG_VALUE
}


extern int/*bool*/ ConnNetInfo_AdjustForHttpProxy(SConnNetInfo* info)
{
    if (info->http_proxy_adjusted  ||  !*info->http_proxy_host) {
        return 0/*false*/;
    }

    if (strlen(info->host) + strlen(info->path) + 16 > sizeof(info->path)) {
        CORE_LOG(eLOG_Error,
                 "[ConnNetInfo_AdjustForHttpProxy]  Too long adjusted path");
        assert(0);
        return 0/*false*/;
    }

    {{
        char x_path[sizeof(info->path)];
        sprintf(x_path, "http://%s:%hu/%s",
                info->host, info->port, info->path);
        assert(strlen(x_path) < sizeof(x_path));
        strcpy(info->path, x_path);
    }}

    assert(sizeof(info->host) >= sizeof(info->http_proxy_host));
    strncpy(info->host, info->http_proxy_host, sizeof(info->host));
    info->host[sizeof(info->host) - 1] = '\0';
    info->port = info->http_proxy_port;
    info->http_proxy_adjusted = 1/*true*/;
    return 1/*true*/;
}


extern void ConnNetInfo_SetUserHeader(SConnNetInfo* info,
                                      const char*   user_header)
{
    if (info->http_user_header)
        free((void*) info->http_user_header);
    info->http_user_header = user_header ?
        strcpy((char*) malloc(strlen(user_header) + 1), user_header) : 0;
}


extern SConnNetInfo* ConnNetInfo_Clone(const SConnNetInfo* info)
{
    SConnNetInfo* x_info;
    if ( !info )
        return 0;

    x_info = (SConnNetInfo*) malloc(sizeof(SConnNetInfo));
    *x_info = *info;
    if (info->timeout && info->timeout != CONN_DEFAULT_TIMEOUT) {
        x_info->tmo = *info->timeout;
        x_info->timeout = &x_info->tmo;
    }
    x_info->http_user_header = 0;
    ConnNetInfo_SetUserHeader(x_info, info->http_user_header);
    return x_info;
}


static void s_PrintString(FILE* fp, const char* name, const char* str) {
    fprintf(fp, "%-16.16s: \"%s\"\n", name, str ? str : "<NULL>");
}
static void s_PrintULong(FILE* fp, const char* name, unsigned long lll) {
    fprintf(fp, "%-16.16s: %lu\n", name, lll);
}
static void s_PrintBool(FILE* fp, const char* name, int/*bool*/ bbb) {
    fprintf(fp, "%-16.16s: %s\n", name, bbb ? "TRUE" : "FALSE");
}

extern void ConnNetInfo_Print(const SConnNetInfo* info, FILE* fp)
{
    if ( !fp )
        return;

    fprintf(fp, "\n----- [BEGIN] ConnNetInfo_Print -----\n");

    if ( info ) {
        s_PrintString(fp, "client_host",     info->client_host);
        s_PrintString(fp, "host",            info->host);
        s_PrintULong (fp, "port",            info->port);
        s_PrintString(fp, "path",            info->path);
        s_PrintString(fp, "args",            info->args);
        s_PrintString(fp, "req_method",
                      info->req_method == eReqMethod_Any
                      ? DEF_CONN_REQ_METHOD :
                      (info->req_method == eReqMethod_Get
                       ? "GET" :
                       (info->req_method == eReqMethod_Post
                        ? "POST" : "Unknown")));
        if (info->timeout) {
            s_PrintULong (fp, "timeout(sec)", info->timeout->sec);
            s_PrintULong (fp, "timeout(usec)",info->timeout->usec);
        } else
            s_PrintString(fp, "timeout",     "infinite");
        s_PrintULong (fp, "max_try",         info->max_try);
        s_PrintString(fp, "http_proxy_host", info->http_proxy_host);
        s_PrintULong (fp, "http_proxy_port", info->http_proxy_port);
        s_PrintString(fp, "proxy_host",      info->proxy_host);
        s_PrintString(fp, "debug_printout", 
                      info->debug_printout == eDebugPrintout_None
                      ? "NONE" :
                      (info->debug_printout == eDebugPrintout_Some
                       ? "SOME" :
                       (info->debug_printout == eDebugPrintout_Data
                        ? "DATA" : "Unknown")));
        s_PrintBool  (fp, "stateless",       info->stateless);   
        s_PrintBool  (fp, "firewall",        info->firewall);
        s_PrintBool  (fp, "lb_disable",      info->lb_disable);
        s_PrintString(fp, "user_header",     info->http_user_header);
        s_PrintBool  (fp, "proxy_adjusted",  info->http_proxy_adjusted);
    } else {
        fprintf(fp, "<NULL>\n");
    }

    fprintf(fp, "----- [END] ConnNetInfo_Print -----\n\n");
}


extern void ConnNetInfo_Destroy(SConnNetInfo* info)
{
    if (!info)
        return;
    ConnNetInfo_SetUserHeader(info, 0);
    free(info);
}


extern SOCK URL_Connect
(const char*     host,
 unsigned short  port,
 const char*     path,
 const char*     args,
 EReqMethod      req_method,
 size_t          content_length,
 const STimeout* c_timeout,
 const STimeout* rw_timeout,
 const char*     user_header,
 int/*bool*/     encode_args,
 ESwitch         data_logging)
{
    static const char *X_REQ_R; /* "POST "/"GET " */
    static const char  X_REQ_Q[] = "?";
    static const char  X_REQ_E[] = " HTTP/1.0\r\n";

    SOCK  sock;
    char  buffer[128];
    char* x_args = 0;

    /* check the args */
    if (!host  ||  !*host  ||  !port  ||  !path  ||  !*path  ||
        (user_header  &&  *user_header  &&
         user_header[strlen(user_header) - 1] != '\n')) {
        CORE_LOG(eLOG_Error, "[URL_Connect]  Bad arguments");
        assert(0);
        return 0/*error*/;
    }

    switch (req_method) {
    case eReqMethod_Any:
        X_REQ_R = DEF_CONN_REQ_METHOD " ";
        break;
    case eReqMethod_Post:
        X_REQ_R = "POST ";
        break;
    case eReqMethod_Get:
        X_REQ_R = "GET ";
        if (content_length) {
            CORE_LOG(eLOG_Warning,
                     "[URL_Connect]  Content length ignored with GET");
            content_length = 0;
        }
        break;
    default:
        CORE_LOG(eLOG_Error, "[URL_Connect]  Unrecognized request method");
        assert(0);
        return 0/*error*/;
    }

    /* connect to HTTPD */
    if (SOCK_Create(host, port, c_timeout, &sock) != eIO_Success) {
        CORE_LOGF(eLOG_Error,
                  ("[URL_Connect]  Socket connect to %s:%hu failed",
                   host, port));
        return 0/*error*/;
    }
    
    SOCK_SetDataLogging(sock, data_logging);

    /* setup i/o timeout for the connection */
    if (SOCK_SetTimeout(sock, eIO_ReadWrite, rw_timeout) != eIO_Success) {
        CORE_LOG(eLOG_Error, "[URL_Connect]  Cannot setup connection timeout");
        SOCK_Close(sock);
        return 0;
    }
    
    /* URL-encode "args", if any specified */
    if (args  &&  *args) {
        size_t src_size = strlen(args);
        if ( encode_args ) {
            size_t dst_size = 3 * src_size;
            size_t src_read, dst_written;
            x_args = (char*) malloc(dst_size + 1);
            URL_Encode(args,   src_size, &src_read,
                       x_args, dst_size, &dst_written);
            x_args[dst_written] = '\0';
            assert(src_read == src_size);
        } else {
            x_args = (char*) malloc(src_size + 1);
            memcpy(x_args, args, src_size + 1);
        }
    }

    /* compose and send HTTP header */
    if (
        /* {POST|GET} <path>?<args> HTTP/1.0\r\n */
        SOCK_Write(sock, (const void*) X_REQ_R, strlen(X_REQ_R), 0)
        != eIO_Success  ||
        SOCK_Write(sock, (const void*) path, strlen(path), 0)
        != eIO_Success  ||
        (x_args  &&
         (SOCK_Write(sock, (const void*) X_REQ_Q, strlen(X_REQ_Q), 0)
          != eIO_Success  ||
          SOCK_Write(sock, (const void*) x_args, strlen(x_args), 0)
          != eIO_Success
          )
         )  ||
        SOCK_Write(sock, (const void*) X_REQ_E, strlen(X_REQ_E), 0)
        != eIO_Success  ||

        /*  <user_header> */
        (user_header  &&
         SOCK_Write(sock, (const void*) user_header, strlen(user_header), 0)
         != eIO_Success)  ||

        /*  Content-Length: <content_length>\r\n\r\n */
        (req_method != eReqMethod_Get  &&
         (sprintf(buffer, "Content-Length: %lu\r\n",
                  (unsigned long) content_length) <= 0  ||
          SOCK_Write(sock, (const void*) buffer, strlen(buffer), 0)
          != eIO_Success))  ||
        SOCK_Write(sock, (const void*) "\r\n", 2, 0)
        != eIO_Success)
        {
            CORE_LOGF(eLOG_Error,
                      ("[URL_Connect]  Error sending HTTP header to %s:%hu",
                       host, port));
            if ( x_args )
                free(x_args);
            SOCK_Close(sock);
            return 0/*error*/;
        }

    /* success */
    if (x_args)
        free(x_args);
    return sock;
}


/* Code for the "*_StripToPattern()" functions
 */
typedef EIO_Status (*FDoRead)
     (void*          src,
      void*          dest,
      size_t         size,
      size_t*        n_read,
      EIO_ReadMethod how
      );

static EIO_Status s_StripToPattern
(void*       source,
 FDoRead     read_func,
 const void* pattern,
 size_t      pattern_size,
 BUF*        buf,
 size_t*     n_discarded)
{
    EIO_Status status;
    char*      buffer;
    size_t     buffer_size;
    size_t     n_read = 0;

    /* check args */
    if ( n_discarded )
        *n_discarded = 0;
    if (!source  ||  !pattern  ||  !pattern_size)
        return eIO_InvalidArg;

    /* allocate a temporary read buffer */
    buffer_size = 2 * pattern_size;
    if (buffer_size < 4096)
        buffer_size = 4096;
    buffer = (char*) malloc(buffer_size);

    /* peek/read;  search for the pattern;  maybe, store the discarded data */
    for (;;) {
        /* peek */
        size_t n_peeked, n_stored, x_discarded;
        assert(n_read < pattern_size);
        status = read_func(source, buffer + n_read, buffer_size - n_read,
                           &n_peeked, eIO_Peek);
        if ( !n_peeked ) {
            assert(status != eIO_Success);
            break; /*error*/
        }

        n_stored = n_read + n_peeked;

        if (n_stored >= pattern_size) {
            /* search for the pattern */
            size_t n_check = n_stored - pattern_size + 1;
            const char* b;
            for (b = buffer;  n_check;  b++, n_check--) {
                if (*b != *((const char*) pattern))
                    continue;
                if (memcmp(b, pattern, pattern_size) == 0)
                    break; /*found*/
            }
            /* pattern found */
            if ( n_check ) {
                size_t x_read = b - buffer + pattern_size;
                assert( memcmp(b, pattern, pattern_size) == 0 );
                status = read_func(source, buffer + n_read, x_read - n_read,
                                   &x_discarded, eIO_Plain);
                assert(status == eIO_Success);
                assert(x_discarded == x_read - n_read);
                if ( buf )
                    BUF_Write(buf, buffer + n_read, x_read - n_read);
                if ( n_discarded )
                    *n_discarded += x_read - n_read;
                break; /*success*/
            }
        }

        /* pattern not found yet */
        status = read_func(source, buffer + n_read, n_peeked,
                           &x_discarded, eIO_Plain);
        assert(status == eIO_Success);
        assert(x_discarded == n_peeked);
        if ( buf )
            BUF_Write(buf, buffer + n_read, n_peeked);
        if ( n_discarded )
            *n_discarded += n_peeked;
        n_read = n_stored;

        if (n_read > pattern_size) {
            size_t n_cut = n_read - pattern_size + 1;
            n_read = pattern_size - 1;
            memmove(buffer, buffer + n_cut, n_read);
        }
    }

    /* cleanup & exit */
    free(buffer);
    return status;
}

static EIO_Status s_CONN_Read
(void*          source,
 void*          dest,
 size_t         size,
 size_t*        n_read,
 EIO_ReadMethod how)
{
    return CONN_Read((CONN)source, dest, size, n_read, how);
}

extern EIO_Status CONN_StripToPattern
(CONN        conn,
 const void* pattern,
 size_t      pattern_size,
 BUF*        buf,
 size_t*     n_discarded)
{
    return s_StripToPattern
        (conn, s_CONN_Read, pattern, pattern_size, buf, n_discarded);
}


static EIO_Status s_SOCK_Read
(void*          source,
 void*          dest,
 size_t         size,
 size_t*        n_read,
 EIO_ReadMethod how)
{
    return SOCK_Read((SOCK)source, dest, size, n_read, how);
}

extern EIO_Status SOCK_StripToPattern
(SOCK        sock,
 const void* pattern,
 size_t      pattern_size,
 BUF*        buf,
 size_t*     n_discarded)
{
    return s_StripToPattern
        (sock, s_SOCK_Read, pattern, pattern_size, buf, n_discarded);
}


static EIO_Status s_BUF_Read
(void*          source,
 void*          dest,
 size_t         size,
 size_t*        n_read,
 EIO_ReadMethod how)
{
    size_t read = (how == eIO_Peek
                   ? BUF_Peek((BUF)source, dest, size)
                   : BUF_Read((BUF)source, dest, size));
    if (n_read)
        *n_read = read;
    return read ? eIO_Success : eIO_Closed;
}

extern EIO_Status BUF_StripToPattern
(BUF         buffer,
 const void* pattern,
 size_t      pattern_size,
 BUF*        buf,
 size_t*     n_discarded)
{
    return s_StripToPattern
        (buffer, s_BUF_Read, pattern, pattern_size, buf, n_discarded);
}


/* Return integer (0..15) corresponding to the "ch" as a hex digit
 * Return -1 on error
 */
static int s_HexChar(char ch)
{
    if ('0' <= ch  &&  ch <= '9')
        return ch - '0';
    if ('a' <= ch  &&  ch <= 'f')
        return 10 + (ch - 'a');
    if ('A' <= ch  &&  ch <= 'F')
        return 10 + (ch - 'A');
    return -1;
}

/* The URL-encoding table
 */
static const char s_Encode[256][4] = {
    "%00", "%01", "%02", "%03", "%04", "%05", "%06", "%07",
    "%08", "%09", "%0A", "%0B", "%0C", "%0D", "%0E", "%0F",
    "%10", "%11", "%12", "%13", "%14", "%15", "%16", "%17",
    "%18", "%19", "%1A", "%1B", "%1C", "%1D", "%1E", "%1F",
    "+",   "!",   "%22", "%23", "$",   "%25", "%26", "'",
    "(",   ")",   "*",   "%2B", ",",   "-",   ".",   "%2F",
    "0",   "1",   "2",   "3",   "4",   "5",   "6",   "7",
    "8",   "9",   "%3A", "%3B", "%3C", "%3D", "%3E", "%3F",
    "%40", "A",   "B",   "C",   "D",   "E",   "F",   "G",
    "H",   "I",   "J",   "K",   "L",   "M",   "N",   "O",
    "P",   "Q",   "R",   "S",   "T",   "U",   "V",   "W",
    "X",   "Y",   "Z",   "%5B", "%5C", "%5D", "%5E", "_",
    "%60", "a",   "b",   "c",   "d",   "e",   "f",   "g",
    "h",   "i",   "j",   "k",   "l",   "m",   "n",   "o",
    "p",   "q",   "r",   "s",   "t",   "u",   "v",   "w",
    "x",   "y",   "z",   "%7B", "%7C", "%7D", "%7E", "%7F",
    "%80", "%81", "%82", "%83", "%84", "%85", "%86", "%87",
    "%88", "%89", "%8A", "%8B", "%8C", "%8D", "%8E", "%8F",
    "%90", "%91", "%92", "%93", "%94", "%95", "%96", "%97",
    "%98", "%99", "%9A", "%9B", "%9C", "%9D", "%9E", "%9F",
    "%A0", "%A1", "%A2", "%A3", "%A4", "%A5", "%A6", "%A7",
    "%A8", "%A9", "%AA", "%AB", "%AC", "%AD", "%AE", "%AF",
    "%B0", "%B1", "%B2", "%B3", "%B4", "%B5", "%B6", "%B7",
    "%B8", "%B9", "%BA", "%BB", "%BC", "%BD", "%BE", "%BF",
    "%C0", "%C1", "%C2", "%C3", "%C4", "%C5", "%C6", "%C7",
    "%C8", "%C9", "%CA", "%CB", "%CC", "%CD", "%CE", "%CF",
    "%D0", "%D1", "%D2", "%D3", "%D4", "%D5", "%D6", "%D7",
    "%D8", "%D9", "%DA", "%DB", "%DC", "%DD", "%DE", "%DF",
    "%E0", "%E1", "%E2", "%E3", "%E4", "%E5", "%E6", "%E7",
    "%E8", "%E9", "%EA", "%EB", "%EC", "%ED", "%EE", "%EF",
    "%F0", "%F1", "%F2", "%F3", "%F4", "%F5", "%F6", "%F7",
    "%F8", "%F9", "%FA", "%FB", "%FC", "%FD", "%FE", "%FF"
};
#define VALID_URL_SYMBOL(ch)  (s_Encode[(unsigned char)ch][0] != '%')


extern int/*bool*/ URL_DecodeEx
(const void* src_buf,
 size_t      src_size,
 size_t*     src_read,
 void*       dst_buf,
 size_t      dst_size,
 size_t*     dst_written,
 const char* allow_symbols)
{
    unsigned char* src = (unsigned char*) src_buf;
    unsigned char* dst = (unsigned char*) dst_buf;

    *src_read    = 0;
    *dst_written = 0;
    if (!src_size  ||  !dst_size)
        return 1/*true*/;

    for ( ;  *src_read != src_size  &&  *dst_written != dst_size;
          (*src_read)++, (*dst_written)++, src++, dst++) {
        switch ( *src ) {
        case '%': {
            int i1, i2;
            if (*src_read + 2 > src_size)
                return 1/*true*/;
            if ((i1 = s_HexChar(*(++src))) == -1)
                return *dst_written ? 1/*true*/ : 0/*false*/;
            if (*src_read + 3 > src_size)
                return 1/*true*/;
            if ((i2 = s_HexChar(*(++src))) == -1)
                return *dst_written ? 1/*true*/ : 0/*false*/;

            *dst = (unsigned char)((i1 << 4) + i2);
            *src_read += 2;
            break;
        }
        case '+': {
            *dst = ' ';
            break;
        }
        default: {
            if (VALID_URL_SYMBOL(*src)  ||
                (allow_symbols  &&  strchr(allow_symbols, *src)))
                *dst = *src;
            else
                return *dst_written ? 1/*true*/ : 0/*false*/;
        }
        }/*switch*/
    }

    assert(src == (unsigned char*) src_buf + *src_read   );
    assert(dst == (unsigned char*) dst_buf + *dst_written);
    return 1/*true*/;
}


extern int/*bool*/ URL_Decode
(const void* src_buf,
 size_t      src_size,
 size_t*     src_read,
 void*       dst_buf,
 size_t      dst_size,
 size_t*     dst_written)
{
    return URL_DecodeEx
        (src_buf, src_size, src_read, dst_buf, dst_size, dst_written, 0);
}


extern void URL_Encode
(const void* src_buf,
 size_t      src_size,
 size_t*     src_read,
 void*       dst_buf,
 size_t      dst_size,
 size_t*     dst_written)
{
    unsigned char* src = (unsigned char*) src_buf;
    unsigned char* dst = (unsigned char*) dst_buf;

    *src_read    = 0;
    *dst_written = 0;
    if (!src_size  ||  !dst_size)
        return;

    for ( ;  *src_read != src_size  &&  *dst_written != dst_size;
          (*src_read)++, (*dst_written)++, src++, dst++) {
        const char* subst = s_Encode[*src];
        if (*subst != '%') {
            *dst = *subst;
        } else if (*dst_written < dst_size - 2) {
            *dst = '%';
            *(++dst) = *(++subst);
            *(++dst) = *(++subst);
            *dst_written += 2;
        } else {
            return;
        }
    }
    assert(src == (unsigned char*) src_buf + *src_read   );
    assert(dst == (unsigned char*) dst_buf + *dst_written);
}



/****************************************************************************
 * NCBI-specific MIME content type and sub-types
 */

static const char* s_MIME_Type[eMIME_T_Unknown+1] = {
    "x-ncbi-data",
    "text",
    "application",
    "unknown"
};

static const char* s_MIME_SubType[eMIME_Unknown+1] = {
    "x-dispatch",
    "x-asn-text",
    "x-asn-binary",
    "x-fasta",
    "x-www-form",
    "html",
    "x-unknown"
};

static const char* s_MIME_Encoding[eENCOD_None+1] = {
    "urlencoded",
    ""
};


extern char* MIME_ComposeContentTypeEx
(EMIME_Type     type,
 EMIME_SubType  subtype,
 EMIME_Encoding encoding,
 char*          buf,
 size_t         buflen)
{
    static const char s_ContentType[] = "Content-Type: ";
    const char*       x_Type          = s_MIME_Type    [(int) type];
    const char*       x_SubType       = s_MIME_SubType [(int) subtype];
    const char*       x_Encoding      = s_MIME_Encoding[(int) encoding];
    char              x_buf[MAX_CONTENT_TYPE_LEN];

    if ( *x_Encoding ) {
        assert(sizeof(s_ContentType) + strlen(x_Type) + strlen(x_SubType)
               + strlen(x_Encoding) + 4 < MAX_CONTENT_TYPE_LEN);
        sprintf(x_buf, "%s%s/%s-%s\r\n",
                s_ContentType, x_Type, x_SubType, x_Encoding);
    } else {
        assert(sizeof(s_ContentType) + strlen(x_Type) + strlen(x_SubType)
               + 3 < MAX_CONTENT_TYPE_LEN);
        sprintf(x_buf, "%s%s/%s\r\n", s_ContentType, x_Type, x_SubType);
    }

    assert(strlen(x_buf) < sizeof(x_buf));
    assert(strlen(x_buf) < buflen);
    strncpy(buf, x_buf, buflen);
    buf[buflen - 1] = '\0';
    return buf;
}


extern char* MIME_ComposeContentType
(EMIME_SubType  subtype,
 EMIME_Encoding encoding,
 char*          buf,
 size_t         buflen)
{
    return MIME_ComposeContentTypeEx(eMIME_T_NcbiData,
                                     subtype, encoding, buf, buflen);
}


extern int/*bool*/ MIME_ParseContentTypeEx
(const char*     str,
 EMIME_Type*     type,
 EMIME_SubType*  subtype,
 EMIME_Encoding* encoding)
{
    char* x_buf;
    char* x_type;
    char* x_subtype;
    int   i;

    if ( type )
        *type = eMIME_T_Unknown;
    if ( subtype )
        *subtype = eMIME_Unknown;
    if ( encoding )
        *encoding = eENCOD_None;

    if (!str  ||  !*str)
        return 0/*false*/;

    {{
        size_t x_size = strlen(str) + 1;
        x_buf  = (char*) malloc(2 * x_size);
        x_type = x_buf  + x_size;
    }}

    strcpy(x_buf, str);
    strlwr(x_buf);

    if ((sscanf(x_buf, " content-type: %s ", x_type) != 1  &&
         sscanf(x_buf, " %s ", x_type) != 1)  ||
        (x_subtype = strchr(x_type, '/')) == 0) {
        free(x_buf);
        return 0/*false*/;
    }
    *x_subtype++ = '\0';

    if ( type ) {
        for (i = 0;  i < (int) eMIME_T_Unknown;  i++) {
            if ( !strncmp(x_type, s_MIME_Type[i], strlen(s_MIME_Type[i])) ) {
                *type = (EMIME_Type) i;
            }
        }
    }

    if ( subtype ) {
        for (i = 0;  i < (int) eMIME_Unknown;  i++) {
            if ( !strncmp(x_subtype, s_MIME_SubType[i],
                          strlen(s_MIME_SubType[i])) ) {
                *subtype = (EMIME_SubType) i;
            }
        }
    }

    if ( encoding ) {
        for (i = 0;  i < (int) eENCOD_None;  i++) {
            if (strstr(x_subtype, s_MIME_Encoding[i]) != 0) {
                *encoding = (EMIME_Encoding) i;
            }
        }
    }
  
    free(x_buf);
    return 1/*true*/;
}


extern int/*bool*/ MIME_ParseContentType
(const char*     str,
 EMIME_SubType*  subtype,
 EMIME_Encoding* encoding)
{
    EMIME_Type type;
    if ( !MIME_ParseContentTypeEx(str, &type, subtype, encoding) )
        return 0/*false*/;

    if (type != eMIME_T_NcbiData) {
        if ( subtype )
            *subtype  = eMIME_Unknown;
        if ( encoding )
            *encoding = eENCOD_None;
        return 0/*false*/;
    }

    return 1/*true*/;
}
