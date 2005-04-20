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
 * Author:  Anton Lavrentiev, Denis Vakatov
 *
 * File Description:
 *   NCBI server meta-address info
 *
 */

#include "ncbi_ansi_ext.h"
#include "ncbi_server_infop.h"
#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_IP_ADDR_LEN  16 /* sizeof("255.255.255.255") */


/*****************************************************************************
 *  Attributes for the different server types::  Interface
 */

/* Table of virtual functions
 */
typedef struct {
    char*       (*Write )(size_t reserve, const USERV_Info* u);
    SSERV_Info* (*Read  )(const char** str);
    size_t      (*SizeOf)(const USERV_Info *u);
    int/*bool*/ (*Equal )(const USERV_Info *u1, const USERV_Info *u2);
} SSERV_Info_VTable;


/* Attributes
 */
typedef struct {
    ESERV_Type        type;
    const char*       tag;
    size_t            tag_len;
    SSERV_Info_VTable vtable;
} SSERV_Attr;


static const char* k_FlagTag[] = {
    "Regular",  /* fSERV_Regular */
    "Blast"     /* fSERV_Blast   */
};


/* Any server is not local by default.
 */
static int/*bool*/ s_LocalServerDefault = 0/*false*/;


int/*bool*/ SERV_SetLocalServerDefault(int/*bool*/ onoff)
{
    int/*bool*/ retval = s_LocalServerDefault;
    s_LocalServerDefault = onoff ? 1/*true*/ : 0/*false*/;
    return retval;
}


/* Attributes' lookup (by either type or tag)
 */
static const SSERV_Attr* s_GetAttrByType(ESERV_Type type);
static const SSERV_Attr* s_GetAttrByTag(const char* tag);


const char* SERV_TypeStr(ESERV_Type type)
{
    const SSERV_Attr* attr = s_GetAttrByType(type);
    if (attr)
        return attr->tag;
    return "";
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
 *  Generic methods based on the server info's virtual functions
 */

char* SERV_WriteInfo(const SSERV_Info* info)
{
    char c_t[MAX_CONTENT_TYPE_LEN];    
    const SSERV_Attr* attr;
    size_t reserve;
    char* str;

    if (info->type != fSERV_Dns &&
        info->mime_t != SERV_MIME_TYPE_UNDEFINED &&
        info->mime_s != SERV_MIME_SUBTYPE_UNDEFINED) {
        char* p;
        if (!MIME_ComposeContentTypeEx(info->mime_t, info->mime_s,
                                       info->mime_e, c_t, sizeof(c_t)))
            return 0;
        assert(c_t[strlen(c_t) - 2] == '\r' && c_t[strlen(c_t) - 1] == '\n');
        c_t[strlen(c_t) - 2] = 0;
        p = strchr(c_t, ' ');
        assert(p);
        p++;
        memmove(c_t, p, strlen(p) + 1);
    } else
        *c_t = 0;
    attr = s_GetAttrByType(info->type);
    reserve = attr->tag_len+1 + MAX_IP_ADDR_LEN + 1+5/*port*/ + 1+10/*flag*/ +
        1+9/*coef*/ + 3+strlen(c_t)/*cont.type*/ + 1+5/*locl*/ + 1+5/*priv*/ +
        1+7/*quorum*/ + 1+14/*rate*/ + 1+5/*sful*/ + 1+12/*time*/ + 1/*EOL*/;
    /* write server-specific info */
    if ((str = attr->vtable.Write(reserve, &info->u)) != 0) {
        char* s = str;
        size_t n;

        memcpy(s, attr->tag, attr->tag_len);
        s += attr->tag_len;
        *s++ = ' ';
        s += HostPortToString(info->host, info->port, s, reserve);
        if ((n = strlen(str + reserve)) != 0) {
            *s++ = ' ';
            memmove(s, str + reserve, n + 1);
            s = str + strlen(str);
        }

        assert(info->flag < (int)(sizeof(k_FlagTag)/sizeof(k_FlagTag[0])));
        if (k_FlagTag[info->flag] && *k_FlagTag[info->flag])
            s += sprintf(s, " %s", k_FlagTag[info->flag]);
        s += sprintf(s, " B=%.2f", info->coef);
        if (*c_t)
            s += sprintf(s, " C=%s", c_t);
        s += sprintf(s, " L=%s", info->locl & 0x0F ? "yes" : "no");
        if (info->type != fSERV_Dns && (info->locl & 0xF0))
            s += sprintf(s, " P=yes");
        if (info->host && info->quorum) {
            if (info->quorum == (unsigned short)(-1))
                s += sprintf(s, " Q=yes");
            else
                s += sprintf(s, " Q=%hu", info->quorum);
        }
        s += sprintf(s," R=%.*f", fabs(info->rate) < 0.01 ? 3 : 2, info->rate);
        if (!(info->type & fSERV_Http) && info->type != fSERV_Dns)
            s += sprintf(s, " S=%s", info->sful ? "yes" : "no");
        s += sprintf(s, " T=%lu", (unsigned long)info->time);
    }
    return str;
}


SSERV_Info* SERV_ReadInfo(const char* info_str)
{
    /* detect server type */
    ESERV_Type  type;
    const char* str = SERV_ReadType(info_str, &type);
    int/*bool*/ coef, mime, locl, priv, quorum, rate, sful, time;
    unsigned short port;                /* host (native) byte order */
    unsigned int host;                  /* network byte order       */
    SSERV_Info *info;

    if (!str || (*str && !isspace((unsigned char)(*str))))
        return 0;
    while (*str && isspace((unsigned char)(*str)))
        str++;
    if (!(str = StringToHostPort(str, &host, &port)))
        return 0;
    while (*str && isspace((unsigned char)(*str)))
        str++;
    /* read server-specific info according to the detected type */
    if (!(info = s_GetAttrByType(type)->vtable.Read(&str)))
        return 0;
    info->host = host;
    if (port)
        info->port = port;
    coef = mime = locl = priv = quorum = rate = sful = time = 0;/*unassigned*/
    /* continue reading server info: optional parts: ... */
    while (*str && isspace((unsigned char)(*str)))
        str++;
    while (*str) {
        if (*(str + 1) == '=') {
            int            n;
            double         d;
            unsigned short h;
            unsigned long  t;
            char           s[4];
            EMIME_Type     mime_t;
            EMIME_SubType  mime_s;
            EMIME_Encoding mime_e;
            
            switch (toupper(*str++)) {
            case 'B':
                if (!coef && sscanf(str, "=%lf%n", &d, &n) >= 1) {
                    if (d < -100.0)
                        d = -100.0;
                    else if (d < 0.0)
                        d = (d < -0.1 ? d : -0.1);
                    else if (d < 0.01)
                        d = 0.0;
                    else if (d > 1000.0)
                        d = 1000.0;
                    info->coef = d;
                    str += n;
                    coef = 1;
                }
                break;
            case 'C':
                if (type == fSERV_Dns)
                    break;
                if (!mime && MIME_ParseContentTypeEx(str + 1, &mime_t,
                                                     &mime_s, &mime_e)) {
                    info->mime_t = mime_t;
                    info->mime_s = mime_s;
                    info->mime_e = mime_e;
                    mime = 1;
                    while (*str && !isspace((unsigned char)(*str)))
                        str++;
                }
                break;
            case 'L':
                if (!locl && sscanf(str, "=%3s%n", s, &n) >= 1) {
                    if (strcasecmp(s, "YES") == 0) {
                        info->locl |=  0x01/*true in low nibble*/;
                        str += n;
                        locl = 1;
                    } else if (strcasecmp(s, "NO") == 0) {
                        info->locl &= ~0x0F/*false in low nibble*/;
                        str += n;
                        locl = 1;
                    }
                }
                break;
            case 'P':
                if (type == fSERV_Dns)
                    break;
                if (!priv && sscanf(str, "=%3s%n", s, &n) >= 1) {
                    if (strcasecmp(s, "YES") == 0) {
                        info->locl |=  0x10;/*true in high nibble*/
                        str += n;
                        priv = 1;
                    } else if (strcasecmp(s, "NO") == 0) {
                        info->locl &= ~0xF0;/*false in high nibble*/
                        str += n;
                        priv = 1;
                    }
                }
                break;
            case 'Q':
                if (type == fSERV_Firewall || !info->host || quorum)
                    break;
                if (sscanf(str,"=%3s%n",s,&n) >= 1 && strcasecmp(s, "YES")==0){
                    info->quorum = (unsigned short)(-1);
                    str += n;
                    quorum = 1;
                } else if (sscanf(str, "=%hu%n", &h, &n) >= 1) {
                    info->quorum = h;
                    str += n;
                    quorum = 1;
                }
                break;
            case 'R':
                if (!rate && sscanf(str, "=%lf%n", &d, &n) >= 1) {
                    if (fabs(d) < 0.001)
                        d = 0.0;
                    else if (fabs(d) > 100000.0)
                        d = (d < 0.0 ? -1.0 : 1.0)*100000.0;
                    info->rate = d;
                    str += n;
                    rate = 1;
                }
                break;
            case 'S':
                if ((type & fSERV_Http) != 0)
                    break;
                if (!sful && sscanf(str, "=%3s%n", s, &n) >= 1) {
                    if (strcasecmp(s, "YES") == 0) {
                        if (type == fSERV_Dns)
                            break; /*check only here for compatibility*/
                        info->sful = 1/*true */;
                        str += n;
                        sful = 1;
                    } else if (strcasecmp(s, "NO") == 0) {
                        info->sful = 0/* false */;
                        str += n;
                        sful = 1;
                    }
                }
                break;
            case 'T':
                if (!time && sscanf(str, "=%lu%n", &t, &n) >= 1) {
                    info->time = (TNCBI_Time) t;
                    str += n;
                    time = 1;
                }
                break;
            }
        } else {
            size_t i;
            for (i = 0; i < sizeof(k_FlagTag)/sizeof(k_FlagTag[0]); i++) {
                size_t n = strlen(k_FlagTag[i]);
                if (strncasecmp(str, k_FlagTag[i], n) == 0) {
                    info->flag = (ESERV_Flags) i;
                    str += n;
                    break;
                }
            }
        }
        if (*str && !isspace((unsigned char)(*str)))
            break;
        while (*str && isspace((unsigned char)(*str)))
            str++;
    }
    if (*str) {
        free(info);
        info = 0;
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
    char* str = (char*) malloc(reserve + strlen(SERV_NCBID_ARGS(info))+3);

    if (str)
        sprintf(str + reserve, "%s",
                *SERV_NCBID_ARGS(info) ? SERV_NCBID_ARGS(info) : "''");
    return str;
}


static SSERV_Info* s_Ncbid_Read(const char** str)
{
    SSERV_Info* info;
    char        *args, *c;

    if (!(args = strdup(*str)))
        return 0;
    for (c = args; *c; c++)
        if (isspace((unsigned char)(*c))) {
            *c++ = '\0';
            while (*c && isspace((unsigned char)(*c)))
                c++;
            break;
        }
    if ((info = SERV_CreateNcbidInfo(0, 80, args)) != 0)
        *str += c - args;
    free(args);
    return info;
}


static size_t s_Ncbid_SizeOf(const USERV_Info* u)
{
    return sizeof(u->ncbid) + strlen(SERV_NCBID_ARGS(&u->ncbid))+1;
}


static int/*bool*/ s_Ncbid_Equal(const USERV_Info* u1, const USERV_Info* u2)
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
        info->sful         = 0;
        info->locl         = s_LocalServerDefault & 0x0F;
        info->time         = 0;
        info->coef         = 0.0;
        info->rate         = 0.0;
        info->mime_t       = SERV_MIME_TYPE_UNDEFINED;
        info->mime_s       = SERV_MIME_SUBTYPE_UNDEFINED;
        info->mime_e       = eENCOD_None;
        info->flag         = SERV_DEFAULT_FLAG;
        memset(&info->reserved, 0, sizeof(info->reserved));
        info->quorum       = 0;
        info->u.ncbid.args = (TNCBI_Size) sizeof(info->u.ncbid);
        if (strcmp(args, "''") == 0) /* special case */
            args = 0;
        strcpy(SERV_NCBID_ARGS(&info->u.ncbid), args ? args : "");
    }
    return info;
}



/*****************************************************************************
 *  STANDALONE::   constructor and virtual functions
 */

/*ARGSUSED*/
static char* s_Standalone_Write(size_t reserve, const USERV_Info* u_info)
{
    char* str = (char*) malloc(reserve + 1);

    if (str)
        str[reserve] = '\0';
    return str;
}


/*ARGSUSED*/
static SSERV_Info* s_Standalone_Read(const char** str)
{
    return SERV_CreateStandaloneInfo(0, 0);
}


static size_t s_Standalone_SizeOf(const USERV_Info* u)
{
    return sizeof(u->standalone);
}


/*ARGSUSED*/
static int/*bool*/ s_Standalone_Equal(const USERV_Info* u1,
                                      const USERV_Info* u2)
{
    return 1;
}


SSERV_Info* SERV_CreateStandaloneInfo
(unsigned int   host,
 unsigned short port)
{
    SSERV_Info *info = (SSERV_Info*) malloc(sizeof(SSERV_Info));

    if (info) {
        info->type   = fSERV_Standalone;
        info->host   = host;
        info->port   = port;
        info->sful   = 0;
        info->locl   = s_LocalServerDefault & 0x0F;
        info->time   = 0;
        info->coef   = 0.0;
        info->rate   = 0.0;
        info->mime_t = SERV_MIME_TYPE_UNDEFINED;
        info->mime_s = SERV_MIME_SUBTYPE_UNDEFINED;
        info->mime_e = eENCOD_None;
        info->flag   = SERV_DEFAULT_FLAG;
        memset(&info->reserved, 0, sizeof(info->reserved));
        info->quorum = 0;
        memset(&info->u.standalone, 0, sizeof(info->u.standalone));
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

    if (!**str || !(path = strdup(*str)))
        return 0;
    for (c = path; *c; c++)
        if (isspace((unsigned char)(*c))) {
            *c++ = '\0';
            while (*c && isspace((unsigned char)(*c)))
                c++;
            break;
        }
    if ((args = strchr(path, '?')) != 0)
        *args++ = '\0';
    /* Sanity check: no parameter delimiter allowed within path */
    if (!strchr(path, '&') &&
        (info = SERV_CreateHttpInfo(type, 0, 80, path, args)) != 0)
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
    return s_HttpAny_Read(fSERV_HttpPost, str);
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
        info->sful        = 0;
        info->locl        = s_LocalServerDefault & 0x0F;
        info->time        = 0;
        info->coef        = 0.0;
        info->rate        = 0.0;
        info->mime_t      = SERV_MIME_TYPE_UNDEFINED;
        info->mime_s      = SERV_MIME_SUBTYPE_UNDEFINED;
        info->mime_e      = eENCOD_None;
        info->flag        = SERV_DEFAULT_FLAG;
        memset(&info->reserved, 0, sizeof(info->reserved));
        info->quorum      = 0;
        info->u.http.path = (TNCBI_Size) sizeof(info->u.http);
        info->u.http.args = (TNCBI_Size) (info->u.http.path +
                                          strlen(path ? path : "")+1);
        strcpy(SERV_HTTP_PATH(&info->u.http), path ? path : "");
        strcpy(SERV_HTTP_ARGS(&info->u.http), args ? args : "");
    }
    return info;
}



/*****************************************************************************
 *  FIREWALL::   constructor and virtual functions
 */

static char* s_Firewall_Write(size_t reserve, const USERV_Info* u_info)
{
    const char* name = SERV_TypeStr(u_info->firewall.type);
    size_t namelen = strlen(name);
    char* str = (char*) malloc(reserve + (namelen ? namelen + 1 : 0));

    if (str)
        strcpy(str + reserve, name);
    return str;
}


static SSERV_Info* s_Firewall_Read(const char** str)
{
    ESERV_Type type;
    const char* s;
    if (!(s = SERV_ReadType(*str, &type)))
        type = (ESERV_Type) fSERV_Any;
    else
        *str = s;
    return SERV_CreateFirewallInfo(0, 0, type);
}


static size_t s_Firewall_SizeOf(const USERV_Info* u)
{
    return sizeof(u->firewall);
}


static int/*bool*/ s_Firewall_Equal(const USERV_Info* u1, const USERV_Info* u2)
{
    return u1->firewall.type == u2->firewall.type;
}


SSERV_Info* SERV_CreateFirewallInfo(unsigned int host, unsigned short port,
                                    ESERV_Type type)
{
    SSERV_Info* info = (SSERV_Info*) malloc(sizeof(SSERV_Info));

    if (info) {
        info->type   = fSERV_Firewall;
        info->host   = host;
        info->port   = port;
        info->sful   = 0;
        info->locl   = s_LocalServerDefault & 0x0F;
        info->time   = 0;
        info->coef   = 0.0;
        info->rate   = 0.0;
        info->mime_t = SERV_MIME_TYPE_UNDEFINED;
        info->mime_s = SERV_MIME_SUBTYPE_UNDEFINED;
        info->mime_e = eENCOD_None;
        info->flag   = SERV_DEFAULT_FLAG;
        memset(&info->reserved, 0, sizeof(info->reserved));
        info->quorum = 0;
        info->u.firewall.type = type;
    }
    return info;
}



/*****************************************************************************
 *  DNS::   constructor and virtual functions
 */

/*ARGSUSED*/
static char* s_Dns_Write(size_t reserve, const USERV_Info* u_info)
{
    char* str = (char*) malloc(reserve + 1);

    if (str)
        str[reserve] = '\0';
    return str;
}


/*ARGSUSED*/
static SSERV_Info* s_Dns_Read(const char** str)
{
    return SERV_CreateDnsInfo(0);
}


static size_t s_Dns_SizeOf(const USERV_Info* u)
{
    return sizeof(u->dns);
}


/*ARGSUSED*/
static int/*bool*/ s_Dns_Equal(const USERV_Info* u1, const USERV_Info* u2)
{
    return 1;
}


SSERV_Info* SERV_CreateDnsInfo(unsigned int host)
{
    SSERV_Info* info = (SSERV_Info*) malloc(sizeof(SSERV_Info));

    if (info) {
        info->type   = fSERV_Dns;
        info->host   = host;
        info->port   = 0;
        info->sful   = 0;
        info->locl   = s_LocalServerDefault & 0x0F;
        info->time   = 0;
        info->coef   = 0.0;
        info->rate   = 0.0;
        info->mime_t = SERV_MIME_TYPE_UNDEFINED;
        info->mime_s = SERV_MIME_SUBTYPE_UNDEFINED;
        info->mime_e = eENCOD_None;
        info->flag   = SERV_DEFAULT_FLAG;
        memset(&info->reserved, 0, sizeof(info->reserved));
        info->quorum = 0;
        memset(&info->u.dns.pad, 0, sizeof(info->u.dns.pad));
    }
    return info;
}


/*****************************************************************************
 *  Attributes for the different server types::  Implementation
 */

static const char kNCBID     [] = "NCBID";
static const char kSTANDALONE[] = "STANDALONE";
static const char kHTTP_GET  [] = "HTTP_GET";
static const char kHTTP_POST [] = "HTTP_POST";
static const char kHTTP      [] = "HTTP";
static const char kFIREWALL  [] = "FIREWALL";
static const char kDNS       [] = "DNS";


/* Note: be aware of "prefixness" of tag constants and order of
 * their appearance in the table below, as comparison is done via
 * "strncmp", which can result 'true' on a smaller fit fragment.
 */
static const SSERV_Attr s_SERV_Attr[] = {
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

    { fSERV_Firewall,
      kFIREWALL, sizeof(kFIREWALL) - 1,
      {s_Firewall_Write,    s_Firewall_Read,
       s_Firewall_SizeOf,   s_Firewall_Equal} },

    { fSERV_Dns,
      kDNS, sizeof(kDNS) - 1,
      {s_Dns_Write,         s_Dns_Read,
       s_Dns_SizeOf,        s_Dns_Equal} }
};


static const SSERV_Attr* s_GetAttrByType(ESERV_Type type)
{
    size_t i;
    for (i = 0;  i < sizeof(s_SERV_Attr)/sizeof(s_SERV_Attr[0]);  i++) {
        if (s_SERV_Attr[i].type == type)
            return &s_SERV_Attr[i];
    }
    return 0;
}


static const SSERV_Attr* s_GetAttrByTag(const char* tag)
{
    if (tag) {
        size_t i;
        for (i = 0;  i < sizeof(s_SERV_Attr)/sizeof(s_SERV_Attr[0]);  i++) {
            size_t len = s_SERV_Attr[i].tag_len;
            if (strncmp(s_SERV_Attr[i].tag, tag, len) == 0)
                if (!tag[len] || /* avoid prefix-only match */
                    !(isalnum((unsigned char) tag[len]) || tag[len] == '_'))
                    return &s_SERV_Attr[i];
        }
    }
    return 0;
}


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.54  2005/04/20 18:15:59  lavr
 * +<assert.h>
 *
 * Revision 6.53  2003/09/02 21:21:42  lavr
 * Cleanup included headers
 *
 * Revision 6.52  2003/06/26 15:21:43  lavr
 * Use server's default locality for fSERV_Dns infos
 *
 * Revision 6.51  2003/06/16 15:58:50  lavr
 * Minor code format changes
 *
 * Revision 6.50  2003/05/31 05:16:28  lavr
 * Add ARGSUSED where args are meant to be unused
 *
 * Revision 6.49  2003/04/25 15:21:05  lavr
 * Explicit cast to avoid int->enum warning
 *
 * Revision 6.48  2003/03/13 19:08:26  lavr
 * Allow missing type in Firewall server info specification
 *
 * Revision 6.47  2003/03/07 22:21:31  lavr
 * Heed 'uninitted use' (false) warning by moving lines around
 *
 * Revision 6.46  2003/02/28 14:48:38  lavr
 * String type match for size_t and int in few expressions
 *
 * Revision 6.45  2002/11/01 20:15:36  lavr
 * Do not allow FIREWALL server specs to have Q flag
 *
 * Revision 6.44  2002/10/28 20:15:06  lavr
 * -<connect/ncbi_server_info.h> ("ncbi_server_infop.h" should suffice)
 *
 * Revision 6.43  2002/10/28 15:46:21  lavr
 * Use "ncbi_ansi_ext.h" privately
 *
 * Revision 6.42  2002/10/21 19:19:23  lavr
 * 2(was:3)-digit precision if R is exactly 0.01
 *
 * Revision 6.41  2002/09/24 15:05:23  lavr
 * Increase precision in SERV_Write() when R is small
 *
 * Revision 6.40  2002/09/17 15:39:33  lavr
 * SSERV_Info::quorum moved past the reserved area
 *
 * Revision 6.39  2002/09/04 15:09:47  lavr
 * Handle quorum field in SSERV_Info::, log moved to end
 *
 * Revision 6.38  2002/05/06 19:16:16  lavr
 * +#include <stdio.h>
 *
 * Revision 6.37  2002/03/22 19:52:18  lavr
 * Do not include <stdio.h>: included from ncbi_util.h or ncbi_priv.h
 *
 * Revision 6.36  2002/03/19 22:13:58  lavr
 * Do not use home-made ANSI-extensions if the platform provides
 *
 * Revision 6.35  2002/03/11 21:59:00  lavr
 * Support for changes in ncbi_server_info.h: DNS server type added as
 * well as support for MIME encoding in server specifications
 *
 * Revision 6.34  2001/12/06 16:07:44  lavr
 * More accurate string length estimation in SERV_WriteInfo()
 *
 * Revision 6.33  2001/11/25 22:12:00  lavr
 * Replaced g_SERV_LocalServerDefault -> SERV_SetLocalServerDefault()
 *
 * Revision 6.32  2001/11/16 20:25:53  lavr
 * +g_SERV_LocalServerDefault as a private global parameter
 *
 * Revision 6.31  2001/09/24 20:43:58  lavr
 * TSERV_Flags reverted to 'int'; expilict cast added in comparison
 *
 * Revision 6.30  2001/09/10 21:17:10  lavr
 * Support written for FIREWALL server type
 *
 * Revision 6.29  2001/06/19 19:12:01  lavr
 * Type change: size_t -> TNCBI_Size; time_t -> TNCBI_Time
 *
 * Revision 6.28  2001/06/05 14:11:29  lavr
 * SERV_MIME_UNDEFINED split into 2 (typed) constants:
 * SERV_MIME_TYPE_UNDEFINED and SERV_MIME_SUBTYPE_UNDEFINED
 *
 * Revision 6.27  2001/06/04 17:01:06  lavr
 * MIME type/subtype added to server descriptor
 *
 * Revision 6.26  2001/05/03 16:35:46  lavr
 * Local bonus coefficient modified: meaning of negative value changed
 *
 * Revision 6.25  2001/04/24 21:38:21  lavr
 * Additions to code to support new locality and bonus attributes of servers.
 *
 * Revision 6.24  2001/03/26 18:39:38  lavr
 * Casting to (unsigned char) instead of (int) for ctype char.class macros
 *
 * Revision 6.23  2001/03/06 23:53:07  lavr
 * SERV_ReadInfo can now consume either hostname or IP address
 *
 * Revision 6.22  2001/03/05 23:10:11  lavr
 * SERV_WriteInfo & SERV_ReadInfo both take only one argument now
 *
 * Revision 6.21  2001/03/02 20:09:14  lavr
 * Typo fixed
 *
 * Revision 6.20  2001/03/01 18:48:19  lavr
 * NCBID allowed to have '' (special case) as an empty argument
 *
 * Revision 6.19  2001/01/03 22:34:44  lavr
 * MAX_IP_ADDRESS_LEN -> MAX_IP_ADDR_LEN (as everywhere else)
 *
 * Revision 6.18  2000/12/29 17:59:38  lavr
 * Reading and writing of SERV_Info now use SOCK_* utility functions
 * SOCK_gethostaddr and SOCK_ntoa. More clean code for reading.
 *
 * Revision 6.17  2000/12/06 22:19:02  lavr
 * Binary host addresses are now explicitly stated to be in network byte
 * order, whereas binary port addresses now use native (host) representation
 *
 * Revision 6.16  2000/12/04 17:34:19  beloslyu
 * the order of include files is important, especially on other Unixes!
 * Look the man on inet_ntoa
 *
 * Revision 6.15  2000/10/20 17:13:30  lavr
 * Service descriptor parse bug fixed
 * Return empty string on unknown type (instead of abort) in 'SERV_TypeStr'
 *
 * Revision 6.14  2000/10/05 21:31:23  lavr
 * Standalone connection marked "stateful" by default
 *
 * Revision 6.13  2000/06/05 20:21:20  lavr
 * Eliminated gcc warning: "subscript has type `char'" in calls to
 * classification macros (<ctype.h>) by explicit casting to unsigned chars
 *
 * Revision 6.12  2000/05/31 23:12:22  lavr
 * First try to assemble things together to get working service mapper
 *
 * Revision 6.11  2000/05/24 16:45:15  lavr
 * Introduced replacement for inet_ntoa: my_ntoa
 *
 * Revision 6.10  2000/05/23 21:05:33  lavr
 * Memory leaks fixed (appeared after server-info structure rearrangement)
 *
 * Revision 6.9  2000/05/23 19:02:49  lavr
 * Server-info now includes rate; verbal representation changed
 *
 * Revision 6.8  2000/05/22 16:53:11  lavr
 * Rename service_info -> server_info everywhere (including
 * file names) as the latter name is more relevant
 *
 * Revision 6.7  2000/05/18 14:12:43  lavr
 * Cosmetic change
 *
 * Revision 6.6  2000/05/17 16:15:13  lavr
 * NCBI_* (for ANSI ext) undone - now "ncbi_ansi_ext.h" does good prototyping
 *
 * Revision 6.5  2000/05/17 14:22:32  lavr
 * Small cosmetic changes
 *
 * Revision 6.4  2000/05/16 15:09:02  lavr
 * Added explicit type casting to get "very smart" compilers happy
 *
 * Revision 6.3  2000/05/15 19:06:09  lavr
 * Use home-made ANSI extensions (NCBI_***)
 *
 * Revision 6.2  2000/05/12 21:42:59  lavr
 * Cleaned up for the C++ compilation, etc.
 *
 * Revision 6.1  2000/05/12 18:36:26  lavr
 * First working revision
 *
 * ==========================================================================
 */
