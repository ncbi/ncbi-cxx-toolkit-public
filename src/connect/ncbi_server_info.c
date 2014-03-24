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
 * Author:  Anton Lavrentiev, Denis Vakatov
 *
 * File Description:
 *   NCBI server meta-address info
 *
 */

#include "ncbi_ansi_ext.h"
#include "ncbi_assert.h"
#include "ncbi_server_infop.h"
#include "ncbi_socketp.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef   fabs
#  undef fabs
#endif /*fabs*/
#define  fabs(v)  ((v) < 0.0 ? -(v) : (v))

#define MAX_IP_ADDR_LEN  16 /* sizeof("255.255.255.255") */


/*****************************************************************************
 *  Attributes for the different server types::  Interface
 */

/* Table of virtual functions
 */
typedef struct {
    SSERV_Info* (*Read  )(const char** str, size_t add);
    char*       (*Write )(size_t reserve, const USERV_Info* u);
    int/*bool*/ (*Equal )(const USERV_Info *u1, const USERV_Info *u2);
    size_t      (*SizeOf)(const USERV_Info *u);
} SSERV_Info_VTable;


/* Attributes
 */
typedef struct {
    ESERV_Type        type;
    const char*       tag;
    size_t            tag_len;
    SSERV_Info_VTable vtable;
} SSERV_Attr;


/* Flags
 */
static const char kRegularInter[] = "RegularInter";
static const char kBlastInter[]   = "BlastInter";
static const char kRegular[]      = "Regular";
static const char kBlast[]        = "Blast";

static struct {
    const char* tag;
    size_t      len;
    ESERV_Flag  val;
} kFlags[] = {
    /* must be ordered longer-to-shorter */
    { kRegularInter, sizeof(kRegularInter)-1, eSERV_RegularInter },
    { kBlastInter,   sizeof(kBlastInter)-1,   eSERV_BlastInter   },
    { kRegular,      sizeof(kRegular)-1,      eSERV_Regular      },
    { kBlast,        sizeof(kBlast)-1,        eSERV_Blast        }
};


/* Any server is not local by default.
 */
static unsigned char/*bool*/ s_LocalServerDefault = 0/*false*/;


int/*bool*/ SERV_SetLocalServerDefault(int/*bool*/ onoff)
{
    int/*bool*/ retval = s_LocalServerDefault;
    s_LocalServerDefault = onoff ? 0x01/*true*/ : 0x00/*false*/;
    return retval;
}


/* Attributes' lookup (by either type or tag)
 */
static const SSERV_Attr* s_GetAttrByType(ESERV_Type type);
static const SSERV_Attr* s_GetAttrByTag (const char* tag);


extern const char* SERV_TypeStr(ESERV_Type type)
{
    const SSERV_Attr* attr = s_GetAttrByType(type);
    if (attr)
        return attr->tag;
    return "";
}


extern const char* SERV_ReadType(const char* str,
                                 ESERV_Type* type)
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

extern char* SERV_WriteInfo(const SSERV_Info* info)
{
    char c_t[MAX_CONTENT_TYPE_LEN];    
    const SSERV_Attr* attr;
    size_t reserve;
    char* str;

    if (!(attr = s_GetAttrByType(info->type)))
        return 0;
    if (info->type != fSERV_Dns
        &&  MIME_ComposeContentTypeEx(info->mime_t, info->mime_s,
                                      info->mime_e, c_t, sizeof(c_t))) {
        char* p;
        assert(c_t[strlen(c_t) - 2] == '\r'  &&  c_t[strlen(c_t) - 1] == '\n');
        c_t[strlen(c_t) - 2] = 0;
        p = strchr(c_t, ' ');
        assert(p);
        p++;
        memmove(c_t, p, strlen(p) + 1);
    } else
        *c_t = 0;
    reserve = attr->tag_len+1 + MAX_IP_ADDR_LEN + 1+5/*port*/ + 1+12/*flag*/ +
        1+9/*coef*/ + 3+strlen(c_t)/*cont.type*/ + 1+5/*locl*/ + 1+5/*priv*/ +
        1+7/*quorum*/ + 1+14/*rate*/ + 1+5/*sful*/ + 1+12/*time*/ + 1/*EOL*/;
    /* write server-specific info */
    if ((str = attr->vtable.Write(reserve, &info->u)) != 0) {
        char* s = str;
        size_t n;

        memcpy(s, attr->tag, attr->tag_len);
        s += attr->tag_len;
        *s++ = ' ';
        s += SOCK_HostPortToString(info->host, info->port, s, reserve);
        if ((n = strlen(str + reserve)) != 0) {
            *s++ = ' ';
            memmove(s, str + reserve, n + 1);
            s = str + strlen(str);
        }

        assert(info->flag < (int)(sizeof(kFlags)/sizeof(kFlags[0])));
        for (n = 0;  n < sizeof(kFlags)/sizeof(kFlags[0]);  ++n) {
            if (info->flag == kFlags[n].val) {
                s += sprintf(s, " %s", kFlags[n].tag);
                break;
            }
        }
        if (info->coef)
            s  = NCBI_simple_ftoa(strcpy(s, " B=") + 3, info->coef, 2);
        if (*c_t)
            s += sprintf(s, " C=%s", c_t);
        s += sprintf(s, " L=%s", info->locl & 1 ? "yes" : "no");
        if (info->type != fSERV_Dns  &&  (info->locl & 0x10))
            s += sprintf(s, " P=yes");
        if (info->host && info->quorum) {
            if (info->quorum == (unsigned short)(-1))
                s += sprintf(s, " Q=yes");
            else
                s += sprintf(s, " Q=%hu", info->quorum);
        }
        s  = NCBI_simple_ftoa(strcpy(s," R=") + 3, info->rate,
                              fabs(info->rate) < 0.01 ? 3 : 2);
        if (!(info->type & fSERV_Http)  &&  info->type != fSERV_Dns)
            s += sprintf(s, " S=%s", info->sful ? "yes" : "no");
        s += sprintf(s, " T=%lu", (unsigned long) info->time);
    }
    return str;
}


SSERV_Info* SERV_ReadInfoEx(const char* str,
                            const char* name,
                            int/*bool*/ lazy)
{
    int/*bool*/    coef, mime, locl, priv, quor, rate, sful, time, flag;
    ESERV_Type     type;
    unsigned int   host;                /* network byte order       */
    unsigned short port;                /* host (native) byte order */
    SSERV_Info*    info;

    /* detect server type */
    str = SERV_ReadType(str, &type);
    if (!str  ||  (*str  &&  !isspace((unsigned char)(*str))))
        return 0;
    /* NB: "str" guarantees there is non-NULL attr */
    while (*str  &&  isspace((unsigned char)(*str)))
        str++;
    if (!ispunct((unsigned char)(*str))  ||  *str == ':') {
        if (!(str = SOCK_StringToHostPortEx(str, &host, &port, lazy)))
            return 0;
        while (*str  &&  isspace((unsigned char)(*str)))
            str++;
    } else {
        host = 0;
        port = 0;
    }
    /* read server-specific info according to the detected type */
    info = s_GetAttrByType(type)->vtable.Read(&str, name ? strlen(name)+1 : 0);
    if (!info)
        return 0;
    info->host = host;
    if (port)
        info->port = port;
    coef = mime = locl = priv = quor = rate = sful = time = flag = 0;/*false*/
    /* continue reading server info: optional parts: ... */
    while (*str  &&  isspace((unsigned char)(*str)))
        str++;
    while (*str) {
        if (str[1] == '='  &&  !isspace((unsigned char) str[2])) {
            char*          e;
            int            n;
            double         d;
            unsigned short h;
            unsigned long  t;
            char           s[4];
            EMIME_Type     mime_t;
            EMIME_SubType  mime_s;
            EMIME_Encoding mime_e;
            
            switch (toupper((unsigned char)(*str++))) {
            case 'B':
                if (coef)
                    break;
                coef = 1;
                d = NCBI_simple_atof(++str, &e);
                if (e > str) {
                    if      (fabs(d) < SERV_MINIMAL_BONUS / 2.0)
                        d = 0.0;
                    else if (fabs(d) < SERV_MINIMAL_BONUS)
                        d = d < 0.0 ? -SERV_MINIMAL_BONUS : SERV_MINIMAL_BONUS;
                    else if (fabs(d) > SERV_MAXIMAL_BONUS)
                        d = d < 0.0 ? -SERV_MAXIMAL_BONUS : SERV_MAXIMAL_BONUS;
                    info->coef = d;
                    str = e;
                }
                break;
            case 'C':
                if (mime  ||  type == fSERV_Dns)
                    break;
                mime = 1;
                if (MIME_ParseContentTypeEx(++str, &mime_t, &mime_s, &mime_e)){
                    info->mime_t = mime_t;
                    info->mime_s = mime_s;
                    info->mime_e = mime_e;
                    /* skip the entire token */
                    while (*str  &&  !isspace((unsigned char)(*str)))
                        str++;
                }
                break;
            case 'L':
                if (locl)
                    break;
                locl = 1;
                if (sscanf(++str, "%3s%n", s, &n) >= 1) {
                    if (strcasecmp(s, "YES") == 0) {
                        info->locl |=  0x01/*set*/;
                        str += n;
                    } else if (strcasecmp(s, "NO") == 0) {
                        info->locl &= ~0x01/*clear*/;
                        str += n;
                    }
                }
                break;
            case 'P':
                if (priv  ||  type == fSERV_Dns)
                    break;
                priv = 1;
                if (sscanf(++str, "%3s%n", s, &n) >= 1) {
                    if (strcasecmp(s, "YES") == 0) {
                        info->locl |=  0x10;/*set*/
                        str += n;
                    } else if (strcasecmp(s, "NO") == 0) {
                        info->locl &= ~0x10;/*clear*/
                        str += n;
                    }
                }
                break;
            case 'Q':
                if (quor  ||  type == fSERV_Firewall  ||  !info->host)
                    break;
                quor = 1;
                if (sscanf(++str, "%3s%n", s, &n) >= 1
                    &&  strcasecmp(s, "YES") == 0) {
                    info->quorum = (unsigned short)(-1);
                    str += n;
                } else if (sscanf(str, "%hu%n", &h, &n) >= 1) {
                    info->quorum = h;
                    str += n;
                }
                break;
            case 'R':
                if (rate)
                    break;
                rate = 1;
                d = NCBI_simple_atof(++str, &e);
                if (e > str) {
                    if      (fabs(d) < SERV_MINIMAL_RATE / 2.0)
                        d = 0.0;
                    else if (fabs(d) < SERV_MINIMAL_RATE)
                        d = d < 0.0 ? -SERV_MINIMAL_RATE : SERV_MINIMAL_RATE;
                    else if (fabs(d) > SERV_MAXIMAL_RATE)
                        d = d < 0.0 ? -SERV_MAXIMAL_RATE : SERV_MAXIMAL_RATE;
                    info->rate = d;
                    str = e;
                }
                break;
            case 'S':
                if (sful  ||  (type & fSERV_Http) != 0)
                    break;
                sful = 1;
                if (sscanf(++str, "%3s%n", s, &n) >= 1) {
                    if (strcasecmp(s, "YES") == 0) {
                        if (type == fSERV_Dns)
                            break; /*check only here for compatibility*/
                        info->sful = 1/*true*/;
                        str += n;
                    } else if (strcasecmp(s, "NO") == 0) {
                        info->sful = 0/*false*/;
                        str += n;
                    }
                }
                break;
            case 'T':
                if (time)
                    break;
                time = 1;
                if (sscanf(++str, "%lu%n", &t, &n) >= 1) {
                    info->time = (TNCBI_Time) t;
                    str += n;
                }
                break;
            }
        } else if (!flag) {
            size_t i;
            flag = 1;
            for (i = 0;  i < sizeof(kFlags)/sizeof(kFlags[0]);  ++i) {
                if (strncasecmp(str, kFlags[i].tag, kFlags[i].len) == 0) {
                    info->flag = kFlags[i].val;
                    str += kFlags[i].len;
                    break;
                }
            }
        } else
              break;
        if (*str  &&  !isspace((unsigned char)(*str)))
            break;
        while (*str  &&  isspace((unsigned char)(*str)))
            str++;
    }
    if (*str) {
        free(info);
        info = 0;
    } else if (name) {
        strcpy((char*) info + SERV_SizeOfInfo(info), name);
        if (info->type == fSERV_Dns)
            info->u.dns.name = 1/*true*/;
    } else if (info->type == fSERV_Dns)
        info->u.dns.name = 0/*false*/;
    return info;
}


extern SSERV_Info* SERV_ReadInfo(const char* str)
{
    return SERV_ReadInfoEx(str, 0, 0);
}


SSERV_Info* SERV_CopyInfoEx(const SSERV_Info* orig,
                            const char*       name)
{
    size_t      size = SERV_SizeOfInfo(orig);
    SSERV_Info* info;
    if (!size)
        return 0;
    if ((info = (SSERV_Info*)malloc(size + (name ? strlen(name)+1 : 0))) != 0){
        memcpy(info, orig, size);
        memset(&info->reserved, 0, sizeof(info->reserved));
        if (name) {
            strcpy((char*) info + size, name);
            if (orig->type == fSERV_Dns)
                info->u.dns.name = 1/*true*/;
        } else if (orig->type == fSERV_Dns)
            info->u.dns.name = 0/*false*/;
    }
    return info;
}


extern SSERV_Info* SERV_CopyInfo(const SSERV_Info* orig)
{
    return SERV_CopyInfoEx(orig, 0);
}


const char* SERV_NameOfInfo(const SSERV_Info* info)
{
    if (!info)
        return 0;
    return info->type != fSERV_Dns  ||  info->u.dns.name
        ? (const char*) info + SERV_SizeOfInfo(info) : "";
}


extern size_t SERV_SizeOfInfo(const SSERV_Info *info)
{
    const SSERV_Attr* attr = info ? s_GetAttrByType(info->type) : 0;
    return attr
        ? sizeof(*info) - sizeof(info->u) + attr->vtable.SizeOf(&info->u) : 0;
}


extern int/*bool*/ SERV_EqualInfo(const SSERV_Info *i1,
                                  const SSERV_Info *i2)
{
    const SSERV_Attr* attr;
    if (i1->type != i2->type || i1->host != i2->host || i1->port != i2->port)
        return 0;
    attr = s_GetAttrByType(i1->type/*==i2->type*/);
    return attr->vtable.Equal ? attr->vtable.Equal(&i1->u, &i2->u) : 1;
}



/*****************************************************************************
 *  NCBID::   constructor and virtual functions
 */

static char* s_Ncbid_Write(size_t reserve, const USERV_Info* u)
{
    const SSERV_NcbidInfo* info = &u->ncbid;
    const char* args = SERV_NCBID_ARGS(info);
    char* str = (char*) malloc(reserve + strlen(args) + 3);

    if (str)
        sprintf(str + reserve, "%s", *args ? args : "''");
    return str;
}


static SSERV_Info* s_Ncbid_Read(const char** str, size_t add)
{
    SSERV_Info* info;
    char        *args, *c;

    if (!(args = strdup(*str)))
        return 0;
    for (c = args;  *c;  c++) {
        if (isspace((unsigned char)(*c))) {
            *c++ = '\0';
            while (*c  &&  isspace((unsigned char)(*c)))
                c++;
            break;
        }
    }
    info = SERV_CreateNcbidInfoEx(0, CONN_PORT_HTTP, args, add);
    if (info)
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


SSERV_Info* SERV_CreateNcbidInfoEx(unsigned int   host,
                                   unsigned short port,
                                   const char*    args,
                                   size_t         add)
{
    SSERV_Info* info;

    add += args ? strlen(args) : 0;
    if ((info = (SSERV_Info*) malloc(sizeof(SSERV_Info) + add + 1)) != 0) {
        info->type         = fSERV_Ncbid;
        info->host         = host;
        info->port         = port;
        info->sful         = 0;
        info->locl         = s_LocalServerDefault;
        info->time         = 0;
        info->coef         = 0.0;
        info->rate         = 0.0;
        info->mime_t       = eMIME_T_Undefined;
        info->mime_s       = eMIME_Undefined;
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


extern SSERV_Info* SERV_CreateNcbidInfo(unsigned int   host,
                                        unsigned short port,
                                        const char*    args)
{
    return SERV_CreateNcbidInfoEx(host, port, args, 0);
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
static SSERV_Info* s_Standalone_Read(const char** str, size_t add)
{
    return SERV_CreateStandaloneInfoEx(0, 0, add);
}


static size_t s_Standalone_SizeOf(const USERV_Info* u)
{
    return sizeof(u->standalone);
}


SSERV_Info* SERV_CreateStandaloneInfoEx(unsigned int   host,
                                        unsigned short port,
                                        size_t         add)
{
    SSERV_Info *info = (SSERV_Info*) malloc(sizeof(SSERV_Info) + add);

    if (info) {
        info->type   = fSERV_Standalone;
        info->host   = host;
        info->port   = port;
        info->sful   = 0;
        info->locl   = s_LocalServerDefault;
        info->time   = 0;
        info->coef   = 0.0;
        info->rate   = 0.0;
        info->mime_t = eMIME_T_Undefined;
        info->mime_s = eMIME_Undefined;
        info->mime_e = eENCOD_None;
        info->flag   = SERV_DEFAULT_FLAG;
        memset(&info->reserved, 0, sizeof(info->reserved));
        info->quorum = 0;
        memset(&info->u.standalone, 0, sizeof(info->u.standalone));
    }
    return info;
}


extern SSERV_Info* SERV_CreateStandaloneInfo(unsigned int   host,
                                             unsigned short port)
{
    return SERV_CreateStandaloneInfoEx(host, port, 0);
}


/*****************************************************************************
 *  HTTP::   constructor and virtual functions
 */

static char* s_Http_Write(size_t reserve, const USERV_Info* u)
{
    const SSERV_HttpInfo* info = &u->http;
    const char* path = SERV_HTTP_PATH(info);
    const char* args = SERV_HTTP_ARGS(info);
    char* str = (char*) malloc(reserve + strlen(path) + strlen(args) + 2);
    if (str) {
        int n = sprintf(str + reserve, "%s", path);
        if (*args)
            sprintf(str + reserve + n, "%s%s", &"?"[!(*args != '#')], args);
    }
    return str;
}


static SSERV_Info* s_HttpAny_Read(ESERV_Type type,const char** str, size_t add)
{
    SSERV_Info* info;
    char       *path, *args, *c;

    if (!**str  ||  !(path = strdup(*str)))
        return 0;
    for (c = path;  *c;  c++) {
        if (isspace((unsigned char)(*c))) {
            *c++ = '\0';
            while (*c  &&  isspace((unsigned char)(*c)))
                c++;
            break;
        }
    }
    if ((args = strchr(path, '?')) != 0)
        *args++ = '\0';
    info = SERV_CreateHttpInfoEx(type, 0, CONN_PORT_HTTP, path, args, add);
    if (info)
        *str += c - path;
    free(path);
    return info;
}


static SSERV_Info *s_HttpGet_Read(const char** str, size_t add)
{
    return s_HttpAny_Read(fSERV_HttpGet, str, add);
}


static SSERV_Info *s_HttpPost_Read(const char** str, size_t add)
{
    return s_HttpAny_Read(fSERV_HttpPost, str, add);
}


static SSERV_Info *s_Http_Read(const char** str, size_t add)
{
    return s_HttpAny_Read(fSERV_Http, str, add);
}


static size_t s_Http_SizeOf(const USERV_Info* u)
{
    return sizeof(u->http) + strlen(SERV_HTTP_PATH(&u->http))+1 +
        strlen(SERV_HTTP_ARGS(&u->http))+1;
}


static int/*bool*/ s_Http_Equal(const USERV_Info* u1, const USERV_Info* u2)
{
    return
        strcmp(SERV_HTTP_PATH(&u1->http), SERV_HTTP_PATH(&u2->http)) == 0  &&
        strcmp(SERV_HTTP_ARGS(&u1->http), SERV_HTTP_ARGS(&u2->http)) == 0;
}


SSERV_Info* SERV_CreateHttpInfoEx(ESERV_Type     type,
                                  unsigned int   host,
                                  unsigned short port,
                                  const char*    path,
                                  const char*    args,
                                  size_t         add)
{
    SSERV_Info* info;

    if (type & ~fSERV_Http)
        return 0;
    add += (path ? strlen(path) : 0) + 1 + (args ? strlen(args) : 0);
    if ((info = (SSERV_Info*) malloc(sizeof(SSERV_Info) + add + 1)) != 0) {
        info->type        = type;
        info->host        = host;
        info->port        = port;
        info->sful        = 0;
        info->locl        = s_LocalServerDefault;
        info->time        = 0;
        info->coef        = 0.0;
        info->rate        = 0.0;
        info->mime_t      = eMIME_T_Undefined;
        info->mime_s      = eMIME_Undefined;
        info->mime_e      = eENCOD_None;
        info->flag        = SERV_DEFAULT_FLAG;
        memset(&info->reserved, 0, sizeof(info->reserved));
        info->quorum      = 0;
        info->u.http.path = (TNCBI_Size) sizeof(info->u.http);
        info->u.http.args = (TNCBI_Size) (info->u.http.path +
                                          (path ? strlen(path) : 0) + 1);
        strcpy(SERV_HTTP_PATH(&info->u.http), path ? path : "");
        strcpy(SERV_HTTP_ARGS(&info->u.http), args ? args : "");
    }
    return info;
}


extern SSERV_Info* SERV_CreateHttpInfo(ESERV_Type     type,
                                       unsigned int   host,
                                       unsigned short port,
                                       const char*    path,
                                       const char*    args)
{
    return SERV_CreateHttpInfoEx(type, host, port, path, args, 0);
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


static SSERV_Info* s_Firewall_Read(const char** str, size_t add)
{
    ESERV_Type type;
    const char* s;
    if (!(s = SERV_ReadType(*str, &type)))
        type = (ESERV_Type) 0/*fSERV_Any*/;
    else
        *str = s;
    return SERV_CreateFirewallInfoEx(0, 0, type, add);
}


static size_t s_Firewall_SizeOf(const USERV_Info* u)
{
    return sizeof(u->firewall);
}


static int/*bool*/ s_Firewall_Equal(const USERV_Info* u1, const USERV_Info* u2)
{
    return u1->firewall.type == u2->firewall.type;
}


SSERV_Info* SERV_CreateFirewallInfoEx(unsigned int   host,
                                      unsigned short port,
                                      ESERV_Type     type,
                                      size_t         add)
{
    SSERV_Info* info = (SSERV_Info*) malloc(sizeof(SSERV_Info) + add);

    if (info) {
        info->type   = fSERV_Firewall;
        info->host   = host;
        info->port   = port;
        info->sful   = 0;
        info->locl   = s_LocalServerDefault;
        info->time   = 0;
        info->coef   = 0.0;
        info->rate   = 0.0;
        info->mime_t = eMIME_T_Undefined;
        info->mime_s = eMIME_Undefined;
        info->mime_e = eENCOD_None;
        info->flag   = SERV_DEFAULT_FLAG;
        memset(&info->reserved, 0, sizeof(info->reserved));
        info->quorum = 0;
        info->u.firewall.type = type;
    }
    return info;
}


SSERV_Info* SERV_CreateFirewallInfo(unsigned int   host,
                                    unsigned short port,
                                    ESERV_Type     type)
{
    return SERV_CreateFirewallInfoEx(host, port, type, 0);
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
static SSERV_Info* s_Dns_Read(const char** str, size_t add)
{
    return SERV_CreateDnsInfoEx(0, add);
}


static size_t s_Dns_SizeOf(const USERV_Info* u)
{
    return sizeof(u->dns);
}


SSERV_Info* SERV_CreateDnsInfoEx(unsigned int host, size_t add)
{
    SSERV_Info* info = (SSERV_Info*) malloc(sizeof(SSERV_Info) + add);

    if (info) {
        info->type   = fSERV_Dns;
        info->host   = host;
        info->port   = 0;
        info->sful   = 0;
        info->locl   = s_LocalServerDefault;
        info->time   = 0;
        info->coef   = 0.0;
        info->rate   = 0.0;
        info->mime_t = eMIME_T_Undefined;
        info->mime_s = eMIME_Undefined;
        info->mime_e = eENCOD_None;
        info->flag   = SERV_DEFAULT_FLAG;
        memset(&info->reserved, 0, sizeof(info->reserved));
        info->quorum = 0;
        memset(&info->u.dns, 0, sizeof(info->u.dns));
    }
    return info;
}


SSERV_Info* SERV_CreateDnsInfo(unsigned int host)
{
    return SERV_CreateDnsInfoEx(host, 0);
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


static const SSERV_Attr kSERV_Attr[] = {
    { fSERV_Ncbid,
      kNCBID,      sizeof(kNCBID) - 1,
      {s_Ncbid_Read,      s_Ncbid_Write,
       s_Ncbid_Equal,     s_Ncbid_SizeOf} },

    { fSERV_Standalone,
      kSTANDALONE, sizeof(kSTANDALONE) - 1,
      {s_Standalone_Read, s_Standalone_Write,
       0,                 s_Standalone_SizeOf} },

    { fSERV_HttpGet,
      kHTTP_GET,   sizeof(kHTTP_GET) - 1,
      {s_HttpGet_Read,    s_Http_Write,
       s_Http_Equal,      s_Http_SizeOf} },

    { fSERV_HttpPost,
      kHTTP_POST,  sizeof(kHTTP_POST) - 1,
      {s_HttpPost_Read,   s_Http_Write,
       s_Http_Equal,      s_Http_SizeOf} },

    { fSERV_Http,
      kHTTP,       sizeof(kHTTP) - 1,
      {s_Http_Read,       s_Http_Write,
       s_Http_Equal,      s_Http_SizeOf} },

    { fSERV_Firewall,
      kFIREWALL,   sizeof(kFIREWALL) - 1,
      {s_Firewall_Read,   s_Firewall_Write,
       s_Firewall_Equal,  s_Firewall_SizeOf} },

    { fSERV_Dns,
      kDNS,        sizeof(kDNS) - 1,
      {s_Dns_Read,       s_Dns_Write,
       0,                s_Dns_SizeOf} }
};


static const SSERV_Attr* s_GetAttrByType(ESERV_Type type)
{
    size_t i;
    for (i = 0;  i < sizeof(kSERV_Attr)/sizeof(kSERV_Attr[0]);  ++i) {
        if (kSERV_Attr[i].type == type)
            return &kSERV_Attr[i];
    }
    return 0;
}


static const SSERV_Attr* s_GetAttrByTag(const char* tag)
{
    if (tag) {
        size_t i;
        for (i = 0;  i < sizeof(kSERV_Attr)/sizeof(kSERV_Attr[0]);  ++i) {
            size_t len = kSERV_Attr[i].tag_len;
            if (strncasecmp(tag, kSERV_Attr[i].tag, len) == 0
                &&  (!tag[len]  ||  isspace((unsigned char) tag[len])))
                return &kSERV_Attr[i];
        }
    }
    return 0;
}
