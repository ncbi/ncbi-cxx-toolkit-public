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
 *   NCBI service meta-address info
 *
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.5  2000/05/17 14:22:32  lavr
 * Small cosmetic changes
 *
 * Revision 6.4  2000/05/16 15:09:02  lavr
 * Added explicit type casting to get "very smart" compilers happy
 *
 * Revision 6.3  2000/05/15 19:06:09  lavr
 * Use home-made ANSI extentions (NCBI_***)
 *
 * Revision 6.2  2000/05/12 21:42:59  lavr
 * Cleaned up for the C++ compilation, etc.
 *
 * Revision 6.1  2000/05/12 18:36:26  lavr
 * First working revision
 *
 * ==========================================================================
 */

#include <connect/ncbi_ansi_ext.h>
#include <connect/ncbi_service_info.h>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>


/*****************************************************************************
 *  Attributes for the different service types::  Interface
 */

/* Table of virtual functions
 */
typedef struct {
    char*       (*Write ) (size_t reserve, const USERV_Info* u);
    SSERV_Info* (*Read  ) (const char** str);
    size_t      (*SizeOf) (const USERV_Info *u);
    int/*bool*/ (*Equal ) (const USERV_Info *u1, const USERV_Info *u2);
} SSERV_Info_VTable;


/* Attributes
 */
typedef struct {
    ESERV_Type        type;
    const char*       tag;
    size_t            tag_len;
    SSERV_Info_VTable vtable;
} SSERV_Attr;


/* Attributes' lookup (by either type or tag)
 */
static const SSERV_Attr* s_GetAttrByType(ESERV_Type type);
static const SSERV_Attr* s_GetAttrByTag(const char* tag);


const char* SERV_TypeStr(ESERV_Type type)
{
    const SSERV_Attr* attr = s_GetAttrByType(type);

    if (attr)
        return attr->tag;

    assert(0);
    return "UNKNOWN";
}


const char* SERV_ReadType(const char* str, ESERV_Type* type)
{
    const SSERV_Attr* attr = s_GetAttrByTag(str);

    if (!attr)
        return 0;
    *type = attr->type;
    return str + attr->tag_len; 
}



/*****************************************************************************
 *  Utilities
 */

#define MAX_IP_ADDRESS_LEN      15 /* sizeof("255.255.255.255")-1 */


/* Utility routine to read host:port from a string.
 * The string has not to contain host if 'default_host' provided non-zero,
 * in which case the port number (with preceding ':') can be absent from the
 * string (return value in the case coincides with argument 'str', meaning no
 * information has been read, default value for the port number must be used).
 */
static const char* s_Read_HostPort(const char* str, unsigned int default_host,
                                   unsigned int* host, unsigned short* port)
{
    const char *s = strchr(str, ':');
    unsigned short p;
    unsigned int h;
    int n;

    if (!default_host) {
        char addrbuf[MAX_IP_ADDRESS_LEN + 1];
        size_t addrlen;

        if (!s || (addrlen = (size_t)(s - str)) > sizeof(addrbuf) - 1)
            return 0;
        strncpy(addrbuf, str, addrlen);
        addrbuf[addrlen] = '\0';
        if (strchr(addrbuf, ' ') || (h = inet_addr(str)) == (unsigned int)(-1))
            return 0;
    } else if (s && s != str) {
        return 0;
    } else /* default_host && (!s || s == str) */
        h = default_host;
    if (!s) {
        p = 0;
        s = str;
        n = 0;
    } else if (sscanf(++s, "%hu%n", &p, &n) < 1)
        return 0;
    *host = h;
    *port = htons(p);
    return str + (int)(s - str) + n;
}


/* Utility routine to print host:port to the string.
 * Suppress printing host if parameter 'host' is zero.
 */
static int s_Write_HostPort(char *str, unsigned int host, unsigned short port)
{
    struct sockaddr_in addr;

    addr.sin_addr.s_addr = host;
    return sprintf(str, "%s:%hu", host ? inet_ntoa(addr.sin_addr) : "",
                   (unsigned short)ntohs(port));
}

#define N_FLAG_TAGS 2
static const char *k_FlagTag[N_FLAG_TAGS] = {
    "Regular",  /* fSERV_Regular */
    "Blast"     /* fSERV_Blast   */
};



/*****************************************************************************
 *  Generic methods based on the service info's virtual functions
 */

char* SERV_WriteInfo(const SSERV_Info* info, int/*bool*/ skip_host)
{
    const SSERV_Attr* attr = s_GetAttrByType(info->type);
    size_t reserve = attr->tag_len+1 + MAX_IP_ADDRESS_LEN+1 + 5+1 +
        10+1/*time*/ + 10+1/*algorithm*/;
    char* str;

    /* write service-specific info */
    if ((str = attr->vtable.Write(reserve, &info->u)) != 0) {
        char* s = str;
        int n;

        memcpy(s, attr->tag, attr->tag_len);
        s += attr->tag_len;
        *s++ = ' ';
        n = s_Write_HostPort(s, skip_host ? 0 : info->host, info->port);
        s += n;
        *s++ = ' ';
        n = strlen(str + reserve);
        memmove(s, str + reserve, n+1);
        s = str + strlen(str);
        n = sprintf(s, "%s%lu", n ? " " : "", (unsigned long)info->time);
        s += n;
        assert(info->flag < N_FLAG_TAGS);
        if (k_FlagTag[info->flag])
            sprintf(s, " %s", k_FlagTag[info->flag]);
    }
    return str;
}


SSERV_Info* SERV_ReadInfo(const char* info_str, unsigned int default_host)
{
    /* detect service type */
    ESERV_Type  type;
    const char* str = SERV_ReadType(info_str, &type);
    unsigned short port;
    unsigned int host;
    int default_port;
    SSERV_Info *info;
    const char *s;

    if (!str || (*str && !isspace(*str)))
        return 0;
    while (*str && isspace(*str))
        str++;
    if (!(s = s_Read_HostPort(str, default_host, &host, &port)))
        return 0;
    default_port = (s == str);
    str = s;
    while (*str && isspace(*str))
        str++;
    /* read service-specific info according to the detected type */
    if ((info = s_GetAttrByType(type)->vtable.Read(&str)) != 0) {
        info->host = host;
        if (!default_port)
            info->port = port;
        /* continue reading service info: optional parts: ... */
        while (*str && isspace(*str))
            str++;
        if (*str) {
            char *c;

            if ((c = NCBI_strdup(str)) != 0) {
                /* ... str = time, ... */
                str = c;
                while (*c && !isspace(*c))
                    c++;
                if (*c)
                    *c++ = '\0';
                while (*c && isspace(*c))
                    c++;
                /* ... s = algorithm tag */
                s = c;
                while (*c && !isspace(*c))
                    c++;
                if (*c)
                    *c++ = '\0';
                while (*c && isspace(*c))
                    c++;
                if (*c) {
                    /* Something extra in the line - error */
                    free(info);
                    info = 0;
                } else {
                    unsigned long temp;

                    if (sscanf(str, "%lu", &temp) < 1) {
                        if (*s) {
                            /* First optional spec was not a number - error */
                            free(info);
                            info = 0;
                        } else {
                            /* The only optional spec may be a flag tag... */
                            s = str;
                        }
                    } else {
                        info->time = (time_t)temp;
                    }
                    if (*s) {
                        size_t i;
                        
                        for (i = 0; i < N_FLAG_TAGS; i++) {
                            if (NCBI_strcasecmp(s, k_FlagTag[i]) == 0)
                                break;
                        }
                        if (i == N_FLAG_TAGS) {
                            /* Flag tag not found - error */
                            free(info);
                            info = 0;
                        } else {
                            info->flag = (ESERV_Flags)i;
                        }
                    }
                }
                free((char *)str);
            } else {
                /* Allocation error */
                free(info);
                info = 0;
            }
        } else {
            /* Apply defaults */
            info->flag = SERV_DEFAULT_FLAG;
            info->time = 0;
        }
    }
    return info;
}


size_t SERV_SizeOfInfo(const SSERV_Info *info)
{
    return sizeof(*info) - sizeof(USERV_Info) +
        s_GetAttrByType(info->type)->vtable.SizeOf(&info->u);
}


int/*bool*/ SERV_EqualInfo(const SSERV_Info *i1, const SSERV_Info *i2)
{
    if (i1->type != i2->type || i1->host != i2->host || i1->port != i2->port)
        return 0;
    return s_GetAttrByType(i1->type)->vtable.Equal(&i1->u, &i2->u);
}



/*****************************************************************************
 *  NCBID::   constructor and virtual functions
 */

static char* s_Ncbid_Write(size_t reserve, const USERV_Info* u)
{
    const SSERV_NcbidInfo* info = &u->ncbid;
    char* str = (char*) malloc(reserve + strlen(SERV_NCBID_ARGS(info))+1);
    
    if (str)
        sprintf(str + reserve, "%s", SERV_NCBID_ARGS(info));
    return str;
}


static SSERV_Info* s_Ncbid_Read(const char** str)
{
    SSERV_Info    *info;
    char          *args, *c;
    
    if (!(args = NCBI_strdup(*str)))
        return 0;
    for (c = args; *c; c++)
        if (isspace(*c)) {
            *c++ = '\0';
            while (*c && isspace(*c))
                c++;
            break;
        }
    if ((info = SERV_CreateNcbidInfo(0, htons(80), args)) != 0)
        *str += c - args;
    free(args);
    return info;
}


static size_t s_Ncbid_SizeOf(const USERV_Info *u)
{
    return sizeof(u->ncbid) + strlen(SERV_NCBID_ARGS(&u->ncbid))+1;
}


static int/*bool*/ s_Ncbid_Equal(const USERV_Info *u1, const USERV_Info *u2)
{
    return
        strcmp(SERV_NCBID_ARGS(&u1->ncbid), SERV_NCBID_ARGS(&u2->ncbid)) == 0;
}

SSERV_Info* SERV_CreateNcbidInfo
(unsigned int   host,
 unsigned short port,
 const char*    args)
{
    size_t args_len = args ? strlen(args) : 0;
    SSERV_Info* info = (SSERV_Info*) malloc(sizeof(SSERV_Info) + args_len + 1);

    if (info) {
        info->type         = fSERV_Ncbid;
        info->host         = host;
        info->port         = port;
        info->flag         = fSERV_Regular;
        info->time         = 0;
        info->u.ncbid.args = sizeof(info->u.ncbid);
        strcpy(SERV_NCBID_ARGS(&info->u.ncbid), args ? args : "");
    }
    return info;
}



/*****************************************************************************
 *  STANDALONE::   constructor and virtual functions
 */

static char* s_Standalone_Write(size_t reserve, const USERV_Info* u_info)
{
    char* str = (char*) malloc(reserve + 1);

    if (str)
        str[reserve] = '\0';
    return str;
}


static SSERV_Info* s_Standalone_Read(const char** str)
{
    return SERV_CreateStandaloneInfo(0, 0);
}


static size_t s_Standalone_SizeOf(const USERV_Info *u)
{
    return sizeof(u->standalone);
}


static int/*bool*/ s_Standalone_Equal(const USERV_Info *u1,
                                      const USERV_Info *u2)
{
    return 1;
}


SSERV_Info* SERV_CreateStandaloneInfo
(unsigned int   host,
 unsigned short port)
{
    SSERV_Info *info = (SSERV_Info*) malloc(sizeof(SSERV_Info));

    if (info) {
        info->type = fSERV_Standalone;
        info->host = host;
        info->port = port;
        info->flag = fSERV_Regular;
        info->time = 0;
    }
    return info;
}



/*****************************************************************************
 *  HTTP::   constructor and virtual functions
 */

static char* s_Http_Write(size_t reserve, const USERV_Info* u)
{
    const SSERV_HttpInfo* info = &u->http;
    char* str = (char*) malloc(reserve + strlen(SERV_HTTP_PATH(info))+1 +
                               strlen(SERV_HTTP_ARGS(info))+1);
    if (str) {
        int n = sprintf(str + reserve, "%s", SERV_HTTP_PATH(info));
        
        if (*SERV_HTTP_ARGS(info))
            sprintf(str + reserve + n, "?%s", SERV_HTTP_ARGS(info));
    }
    return str;
}


static SSERV_Info* s_HttpAny_Read(ESERV_Type type, const char** str)
{
    SSERV_Info*    info;
    char           *path, *args, *c;

    if (!(path = NCBI_strdup(*str)))
        return 0;
    for (c = path; *c; c++)
        if (isspace(*c)) {
            *c++ = '\0';
            while (*c && isspace(*c))
                c++;
            break;
        }
    if ((args = strchr(path, '?')) != 0)
        *args++ = '\0';
    /* Sanity check: no parameter delimiter allowed within path */
    if (!strchr(path, '&') &&
        (info = SERV_CreateHttpInfo(type, 0, htons(80), path, args)) != 0)
        *str += c - path;
    else
        info = 0;
    free(path);
    return info;
}


static SSERV_Info *s_HttpGet_Read(const char** str)
{
    return s_HttpAny_Read(fSERV_HttpGet, str);
}


static SSERV_Info *s_HttpPost_Read(const char** str)
{
    return s_HttpAny_Read(fSERV_HttpGet, str);
}


static SSERV_Info *s_Http_Read(const char** str)
{
    return s_HttpAny_Read(fSERV_Http, str);
}


static size_t s_Http_SizeOf(const USERV_Info* u)
{
    return sizeof(u->http) + strlen(SERV_HTTP_PATH(&u->http))+1 +
        strlen(SERV_HTTP_ARGS(&u->http))+1;
}


static int/*bool*/ s_Http_Equal(const USERV_Info* u1, const USERV_Info* u2)
{
    return
        strcmp(SERV_HTTP_PATH(&u1->http), SERV_HTTP_PATH(&u2->http)) == 0 &&
        strcmp(SERV_HTTP_ARGS(&u1->http), SERV_HTTP_ARGS(&u2->http)) == 0;
}


SSERV_Info* SERV_CreateHttpInfo
(ESERV_Type     type,
 unsigned int   host,
 unsigned short port,
 const char*    path,
 const char*    args)
{
    size_t add_len = (path ? strlen(path) : 0)+1 + (args ? strlen(args) : 0);
    SSERV_Info* info;

    if (type & ~fSERV_Http)
        return 0;
    if ((info = (SSERV_Info*) malloc(sizeof(SSERV_Info) + add_len+1)) != 0) {
        info->type        = type;
        info->host        = host;
        info->port        = port;
        info->flag        = fSERV_Regular;
        info->time        = 0;
        info->u.http.path = sizeof(info->u.http);
        info->u.http.args = info->u.http.path + strlen(path ? path : "")+1;
        strcpy(SERV_HTTP_PATH(&info->u.http), path ? path : "");
        strcpy(SERV_HTTP_ARGS(&info->u.http), args ? args : "");
    }
    return info;
}



/*****************************************************************************
 *  Attributes for the different service types::  Implementation
 */

static const char kNCBID     [] = "NCBID";
static const char kSTANDALONE[] = "STANDALONE";
static const char kHTTP_GET  [] = "HTTP_GET";
static const char kHTTP_POST [] = "HTTP_POST";
static const char kHTTP      [] = "HTTP";


#define N_SERV_ATTR 5

/* Note: be aware of "prefixness" of tag constants and order of
 * their appearance in the table below, as comparison is done via
 * "strncmp", which can result 'true' on a smaller fit fragment.
 */
static const SSERV_Attr s_SERV_Attr[N_SERV_ATTR] = {
    { fSERV_Ncbid,
      kNCBID,      sizeof(kNCBID) - 1,
      {s_Ncbid_Write,       s_Ncbid_Read,
       s_Ncbid_SizeOf,      s_Ncbid_Equal} },

    { fSERV_Standalone,
      kSTANDALONE, sizeof(kSTANDALONE) - 1,
      {s_Standalone_Write,  s_Standalone_Read,
       s_Standalone_SizeOf, s_Standalone_Equal} },

    { fSERV_HttpGet,
      kHTTP_GET,   sizeof(kHTTP_GET) - 1,
      {s_Http_Write,        s_HttpGet_Read,
       s_Http_SizeOf,       s_Http_Equal} },

    { fSERV_HttpPost,
      kHTTP_POST,  sizeof(kHTTP_POST) - 1,
      {s_Http_Write,        s_HttpPost_Read,
       s_Http_SizeOf,       s_Http_Equal} },

    { fSERV_Http,
      kHTTP,  sizeof(kHTTP) - 1,
      {s_Http_Write,        s_Http_Read,
       s_Http_SizeOf,       s_Http_Equal} },
};


static const SSERV_Attr* s_GetAttrByType(ESERV_Type type)
{
    size_t i;
    for (i = 0;  i < N_SERV_ATTR;  i++) {
        if (s_SERV_Attr[i].type == type)
            return &s_SERV_Attr[i];
    }
    return 0;
}


static const SSERV_Attr* s_GetAttrByTag(const char* tag)
{
    size_t i;
    for (i = 0;  i < N_SERV_ATTR;  i++) {
        if (strncmp(s_SERV_Attr[i].tag, tag, s_SERV_Attr[i].tag_len) == 0)
            return &s_SERV_Attr[i];
    }
    return 0;
}
