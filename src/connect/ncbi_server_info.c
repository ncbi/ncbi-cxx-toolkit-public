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

#define MAX_IP_ADDR_LEN  80  /*IPv6 friendly*/

#define SERV_VHOSTABLE   (/*fSERV_Ncbid |*/ fSERV_Http | fSERV_Standalone)

#if CONN_HOST_LEN > 255
#  error "CONN_HOST_LEN may not be greater than 255"
#endif


/*
 * Server-info storage model:
 *   SSERV_Info's basic fields are stored per the structure;
 *   SSERV_Info::u is the last "fixed" field;
 *   SSERV_Info::u's string parameters are stored contiguously following the
 *                 "fixed" area (SSERV_Ops::SizeOf() returns the size of the
 *                 type-dependent "u" plus all the string parameters);
 *   SSERV_Info::vhost (if non-zero) is stored past the parameters (+'\0');
 *   SSERV_Info::extra defines the size of "extra" opaque bytes (which may be
 *                 used to keep private information) that follow vhost (or the
 *                 SSERV_Info::u's block if vhost is empty), zero means none;
 *   SERV_SizeOfInfo() returns the size that includes all of the above.
 * Service name may be stored right past the entire info, contiguously.
 */


/*****************************************************************************
 *  Attributes for the different server types::  Interface
 */

/* Table of operations: req = required; opt = optional
 */
typedef struct {
    SSERV_Info* (*Read  )(const char** str, size_t add);               /*req*/
    char*       (*Write )(size_t reserve, const USERV_Info* u);        /*req*/
    int/*bool*/ (*Equal )(const USERV_Info *u1, const USERV_Info *u2); /*opt*/
    size_t      (*SizeOf)(const USERV_Info *u);                        /*req*/
} SSERV_Ops;


/* Attributes
 */
typedef struct {
    ESERV_Type  type;
    const char* tag;
    size_t      len;
    SSERV_Ops   ops;
} SSERV_Attr;


/* Flags: to be removed, backward compatibility only
 */
static const char kRegularInter[] = "RegularInter";
static const char kBlastInter[]   = "BlastInter";
static const char kRegular[]      = "Regular";
static const char kBlast[]        = "Blast";

static struct {
    const char* tag;
    size_t      len;
    ESERV_Algo  algo;
    ESERV_Site  site;
} kFlags[] = {
    /* must be ordered longer-to-shorter */
    { kRegularInter, sizeof(kRegularInter)-1, eSERV_Regular, fSERV_Interzone },
    { kBlastInter,   sizeof(kBlastInter)-1,   eSERV_Blast,   fSERV_Interzone },
    { kRegular,      sizeof(kRegular)-1,      eSERV_Regular, (ESERV_Site) 0  },
    { kBlast,        sizeof(kBlast)-1,        eSERV_Blast,   (ESERV_Site) 0  }
};


/* Attributes' lookup (by either type or tag)
 */
static const SSERV_Attr* s_GetAttrByType(ESERV_Type type);
static const SSERV_Attr* s_GetAttrByTag (const char* tag);


extern const char* SERV_TypeStr(ESERV_Type type)
{
    const SSERV_Attr* attr = s_GetAttrByType(type);
    return attr ? attr->tag : "";
}


extern const char* SERV_ReadType(const char* str,
                                 ESERV_Type* type)
{
    const SSERV_Attr* attr = s_GetAttrByTag(str);
    if (!attr)
        return 0;
    *type = attr->type;
    return str + attr->len; 
}



/*****************************************************************************
 *  Generic methods based on the server info's virtual functions
 */

extern char* SERV_WriteInfo(const SSERV_Info* info)
{
    static const char* k_YN[] = { "yes", "no" };
    char c_t[CONN_CONTENT_TYPE_LEN+1];
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
        c_t[strlen(c_t) - 2] = '\0';
        p = strchr(c_t, ' ');
        assert(p);
        p++;
        memmove(c_t, p, strlen(p) + 1);
    } else
        *c_t = '\0';
    reserve = attr->len+1 + MAX_IP_ADDR_LEN + 1+5/*port*/ + 1+12/*algo*/
        + 1+9/*coef*/ + 3+strlen(c_t)/*cont.type*/ + 3*(1+5)/*site*/
        + 1+14/*rate*/ + 2*(1+5)/*mode*/ + 1+12/*time*/ + 2*(1+5)/*$,X*/
        + 3+info->vhost
        + 1/*EOL*/;
    /* write server-specific info */
    if ((str = attr->ops.Write(reserve, &info->u)) != 0) {
        char* s = str;
        size_t n;

        memcpy(s, attr->tag, attr->len);
        s += attr->len;
        *s++ = ' ';
        if (info->host == SOCK_HostToNetLong((unsigned int)(-1))) {
            int/*bool*/ ipv6 = !NcbiIsIPv4(&info->addr)  &&  info->port;
            if (ipv6)
                *s++ = '[';
            if (!(s = NcbiAddrToString(s, MAX_IP_ADDR_LEN, &info->addr))) {
                free(str);
                return 0;
            }
            if (ipv6)
                *s++ = ']';
            if (info->port)
                s += sprintf(s, ":%hu", info->port);
        } else
            s += SOCK_HostPortToString(info->host, info->port, s, reserve);
        if ((n = strlen(str + reserve)) != 0) {
            *s++ = ' ';
            memmove(s, str + reserve, n + 1);
            s = str + strlen(str);
        }
        if (info->algo != eSERV_Regular) {
            assert(info->algo == eSERV_Blast);
            strcpy(s, " A=B");
            s += 4;
        }
        if (info->coef)
            s  = NCBI_simple_ftoa(strcpy(s, " B=") + 3, info->coef, 2);
        if (*c_t)
            s += sprintf(s, " C=%s", c_t);
        if (info->vhost) {
            size_t size = attr->ops.SizeOf(&info->u);
            const char* vhost = (const char*) &info->u + size;
            s += sprintf(s, " H=%.*s", (int) info->vhost, vhost);
        }
        s += sprintf(s, " L=%s", k_YN[!(info->site & fSERV_Local)]);
        if (info->type != fSERV_Dns  &&  (info->site & fSERV_Private))
            s += sprintf(s, " P=yes");
        s  = NCBI_simple_ftoa(strcpy(s," R=") + 3, info->rate,
                              fabs(info->rate) < 0.01 ? 3 : 2);
        if (!(info->type & fSERV_Http)  &&  info->type != fSERV_Dns)
            s += sprintf(s, " S=%s", k_YN[!(info->mode & fSERV_Stateful)]);
        if (info->type != fSERV_Dns  &&  (info->mode & fSERV_Secure))
            s += sprintf(s, " $=yes");
        if (info->time)
            s += sprintf(s, " T=%lu", (unsigned long) info->time);
        if (info->site & fSERV_Interzone)
            sprintf(s, " X=yes");
    }
    return str;
}


SSERV_Info* SERV_ReadInfoEx(const char* str,
                            const char* name,
                            int/*bool*/ lazy)
{
    int/*bool*/ algo, coef, mime, locl, priv, rate, sful, secu, time, cros, vh;
    const char*       vhost;
    const SSERV_Attr* attr;
    unsigned int      host;  /* network byte order       */
    unsigned short    port;  /* host (native) byte order */
    TNCBI_IPv6Addr    addr;
    SSERV_Info*       info;
    ESERV_Type        type;
    size_t            len;

    /* detect server type */
    str = SERV_ReadType(str, &type);
    if (!str  ||  (*str  &&  !isspace((unsigned char)(*str))))
        return 0;
    /* NB: "str" guarantees there is a non-NULL attr */
    verify((attr = s_GetAttrByType(type)) != 0);
    while (*str  &&  isspace((unsigned char)(*str)))
        ++str;

    /* read optional connection point */
    if (*str  &&
        (*str == ':'  ||  *str == '['  ||  !ispunct((unsigned char)(*str)))) {
        const char* end = str;
        while (*end  &&  !isspace((unsigned char)(*end)))
            ++end;
        verify((len = (size_t)(end - str)) > 0);

        if (!(*str == ':'  ||  *str == '[')) {
            if (strcspn(str, "=") >= len) {
                size_t i;
                for (i = 0;  i < sizeof(kFlags) / sizeof(kFlags[0]);  ++i) {
                    if (len == kFlags[i].len
                        &&  strncasecmp(str, kFlags[i].tag, len) == 0) {
                        end = 0;
                        break;
                    }
                }
            } else
                end = 0;
        }
        if (end) {
            int/*bool*/ ipv6 = *str == '['  &&  len > 2 ? 1/*T*/ : 0/*F*/;
            const char* tmp = NcbiIPToAddr(&addr, str + ipv6, len - ipv6);
            if (tmp  &&  (!ipv6  ||  (*tmp++ == ']'  &&  *tmp == ':'))) {
                if (tmp != end  &&  *tmp != ':')
                    return 0;
                str   = tmp;
                ipv6  = 1;
                vhost = 0;
            } else if (*str == '[') {
                /* not a valid IP(v4/v6) address */
                return 0;
            } else {
                assert(!ipv6);
                vhost = attr->type & SERV_VHOSTABLE ? str : 0;
            }
            if (str == end)
                port = 0/*NB: "ipv6" case only*/;
            else if (!(str = SOCK_StringToHostPortEx(str, &host, &port, lazy)))
                return 0;
            else if (str != end)
                return 0;
            else if (!lazy  &&  host == SOCK_HostToNetLong((unsigned int)(-1)))
                vhost = 0;
            if (!ipv6)
                NcbiIPv4ToIPv6(&addr, host, 0);
            else if (NcbiIsIPv4(&addr))
                host = NcbiIPv6ToIPv4(&addr, 0);
            else
                host = SOCK_HostToNetLong((unsigned int)(-1));
            while (*str  &&  isspace((unsigned char)(*str)))
                ++str;
            if (vhost) {
                /*NB: NcbiIPToAddr() can parse full-quad IPv4, so !vhost*/
                int/*bool*/ dot = 0/*false*/;
                for (len = 0;  len < (size_t)(end - vhost);  ++len) {
                    if (vhost[len] == ':')
                        break;
                    if (vhost[len] == '.')
                        dot = 1/*true*/;
                }
                if (1 < len  &&  len <= CONN_HOST_LEN  &&  dot) {
                    tmp = strndup(vhost, len);
                    assert(!tmp  ||  !SOCK_isipEx(tmp, 1/*full-quad*/));
                    if (tmp) {
                        if (SOCK_isip(tmp)/*NB: very unlikely*/)
                            vhost = 0;
                        free((void*) tmp);
                    } else
                        vhost = 0;
                } else
                    vhost = 0;
            }
        } else {
            host = 0;
            port = 0;
            memset(&addr, 0, sizeof(addr));
            vhost = 0;
            len = 0;
        }
    } else {
        host = 0;
        port = 0;
        memset(&addr, 0, sizeof(addr));
        vhost = 0;
        len = 0;
    }
    if (!port  &&  attr->type == fSERV_Standalone)
        return 0;

    /* read server-specific info according to the detected type... */
    info = attr->ops.Read(&str,
                          (attr->type & SERV_VHOSTABLE ? CONN_HOST_LEN+1 : 0) +
                          (name ? strlen(name) + 1 : 0));
    if (!info)
        return 0;
    assert(info->type == attr->type);
    info->host = host;
    info->port = port;
    info->addr = addr;
    algo = coef = mime = locl = priv = rate = sful = secu = time = cros = vh
        = 0/*false*/;

    /* ...continue reading server info: optional tags */
    while (*str  &&  isspace((unsigned char)(*str)))
        ++str;
    while (*str) {
        if (str[1] == '='  &&  str[2]  &&  !isspace((unsigned char) str[2])) {
            char*          e;
            int            n;
            double         d;
            unsigned long  t;
            char           s[4];
            EMIME_Type     mime_t;
            EMIME_SubType  mime_s;
            EMIME_Encoding mime_e;

            switch (toupper((unsigned char)(*str++))) {
            case 'A':
                if (algo)
                    break;
                algo = 1/*true*/;
                switch ((unsigned char)(*++str)) {
                case 'B':
                    info->algo = eSERV_Blast;
                    ++str;
                    break;
                case 'R':
                    info->algo = eSERV_Regular;
                    ++str;
                    break;
                default:
                    break;
                }
                break;
            case 'B':
                if (coef)
                    break;
                coef = 1/*true*/;
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
                mime = 1/*true*/;
                if (MIME_ParseContentTypeEx(++str, &mime_t, &mime_s, &mime_e)){
                    info->mime_t = mime_t;
                    info->mime_s = mime_s;
                    info->mime_e = mime_e;
                    /* skip the entire token */
                    while (*str  &&  !isspace((unsigned char)(*str)))
                        ++str;
                }
                break;
            case 'H':
                if (vh  ||  !(info->type & SERV_VHOSTABLE))
                    break;
                vh = 1/*true*/;
                for (len = 0, vhost = ++str;  vhost[len];  ++len) {
                    if (isspace((unsigned char) vhost[len]))
                        break;
                    if (len > CONN_HOST_LEN)
                        break;
                }
                assert(len);
                if (len > CONN_HOST_LEN)
                    break;
                str = vhost + len;
                if (len == 2  &&  memcmp(vhost, "''", 2) == 0)
                    vhost = 0;
                break;
            case 'L':
                if (locl)
                    break;
                locl = 1/*true*/;
                if (sscanf(++str, "%3s%n", s, &n) >= 1) {
                    if (strcasecmp(s, "YES") == 0) {
                        info->site |=               fSERV_Local;
                        str += n;
                    } else if (strcasecmp(s, "NO") == 0) {
                        info->site &= (TSERV_Site)(~fSERV_Local);
                        str += n;
                    }
                }
                break;
            case 'P':
                if (priv  ||  type == fSERV_Dns)
                    break;
                priv = 1/*true*/;
                if (sscanf(++str, "%3s%n", s, &n) >= 1) {
                    if (strcasecmp(s, "YES") == 0) {
                        info->site |=               fSERV_Private;
                        str += n;
                    } else if (strcasecmp(s, "NO") == 0) {
                        info->site &= (TSERV_Site)(~fSERV_Private);
                        str += n;
                    }
                }
                break;
            case 'R':
                if (rate)
                    break;
                rate = 1/*true*/;
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
                if (sful  ||  type == fSERV_Dns  ||  (type & fSERV_Http))
                    break;
                sful = 1/*true*/;
                if (sscanf(++str, "%3s%n", s, &n) >= 1) {
                    if (strcasecmp(s, "YES") == 0) {
                        info->mode |=               fSERV_Stateful;
                        str += n;
                    } else if (strcasecmp(s, "NO") == 0) {
                        info->mode &= (TSERV_Mode)(~fSERV_Stateful);
                        str += n;
                    }
                }
                break;
            case '$':
                if (secu  ||  type == fSERV_Dns)
                    break;
                secu = 1/*true*/;
                if (sscanf(++str, "%3s%n", s, &n) >= 1) {
                    if (strcasecmp(s, "YES") == 0) {
                        info->mode |=               fSERV_Secure;
                        str += n;
                    } else if (strcasecmp(s, "NO") == 0) {
                        info->mode &= (TSERV_Mode)(~fSERV_Secure);
                        str += n;
                    }
                }
                break;
            case 'T':
                if (time)
                    break;
                time = 1/*true*/;
                if (sscanf(++str, "%lu%n", &t, &n) >= 1) {
                    info->time = (TNCBI_Time) t;
                    str += n;
                }
                break;
            case 'X':
                if (cros)
                    break;
                cros = 1/*true*/;
                if (sscanf(++str, "%3s%n", s, &n) >= 1) {
                    if (strcasecmp(s, "YES") == 0) {
                        info->site |=               fSERV_Interzone;
                        str += n;
                    } else if (strcasecmp(s, "NO") == 0) {
                        info->site &= (TSERV_Site)(~fSERV_Interzone);
                        str += n;
                    }
                }
                break;
            }
        } else if (!algo) {
            size_t i;
            algo = 1/*true*/;
            for (i = 0;  i < sizeof(kFlags) / sizeof(kFlags[0]);  ++i) {
                if (strncasecmp(str, kFlags[i].tag, kFlags[i].len) == 0) {
                    if (kFlags[i].site) {
                        if (cros)
                            break;
                        cros = 1/*true*/;
                        info->site |= (TSERV_Site) kFlags[i].site;
                    }
                    info->algo = kFlags[i].algo;
                    str += kFlags[i].len;
                    break;
                }
            }
        } else
            break;
        if (*str  &&  !isspace((unsigned char)(*str)))
            break;
        while (*str  &&  isspace((unsigned char)(*str)))
            ++str;
    }

    if (!*str  &&
        (!vh  ||  (info->mode & fSERV_Secure)  ||  (info->type & fSERV_Http))){
        if (!vh
            &&  !((info->mode & fSERV_Secure)  ||  (info->type & fSERV_Http))){
            vhost = 0;
        }
        if (vhost) {
            assert(len  &&  len <= CONN_HOST_LEN);
            strncpy0((char*) &info->u + attr->ops.SizeOf(&info->u),
                     vhost, len);
            info->vhost = (unsigned char) len;
            assert((size_t) info->vhost == len);
        }
        if (name) {
            strcpy((char*) info + SERV_SizeOfInfo(info), name);
            if (info->type == fSERV_Dns)
                info->u.dns.name = 1/*true*/;
        } else if (info->type == fSERV_Dns)
            info->u.dns.name = 0/*false*/;
        if (!info->port
            &&  (info->type == fSERV_Ncbid  ||  (info->type & fSERV_Http))) {
            info->port = secu ? CONN_PORT_HTTPS : CONN_PORT_HTTP;
        }
    } else {
        free(info);
        info = 0;
    }
    return info;
}


extern SSERV_Info* SERV_ReadInfo(const char* str)
{
    return SERV_ReadInfoEx(str, 0, 0);
}


SSERV_Info* SERV_CopyInfoEx(const SSERV_Info* orig,
                            const char*       name)
{
    size_t      nlen, size = SERV_SizeOfInfo(orig);
    SSERV_Info* info;
    if (!size)
        return 0;
    nlen = name ? strlen(name) + 1 : 0;
    if ((info = (SSERV_Info*) malloc(size + nlen)) != 0){
        memcpy(info, orig, size);
        if (name) {
            memcpy((char*) info + size, name, nlen);
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
    return !info ? 0
        : info->type != fSERV_Dns  ||  info->u.dns.name
        ? (const char*) info + SERV_SizeOfInfo(info) : "";
}


extern size_t SERV_SizeOfInfo(const SSERV_Info *info)
{
    const SSERV_Attr* attr = info ? s_GetAttrByType(info->type) : 0;
    return attr
        ? (sizeof(*info) - sizeof(info->u)
           + attr->ops.SizeOf(&info->u)
           + (info->vhost ? (size_t) info->vhost + 1 : 0)
           + info->extra)
        : 0;
}


extern int/*bool*/ SERV_EqualInfo(const SSERV_Info *i1,
                                  const SSERV_Info *i2)
{
    const SSERV_Attr* attr;
    assert(i1  &&  i2);
    /* NB: IPv6 address causes the "host" field to be INADDR_NONE(-1) */
    if (i1->type != i2->type  ||
        i1->host != i2->host  ||
        i1->port != i2->port) {
        return 0/*false*/;
    }
    if (!NcbiIsEmptyIPv6(&i1->addr)  &&  !NcbiIsEmptyIPv6(&i2->addr)
        &&  memcmp(&i1->addr, &i2->addr, sizeof(i1->addr)) != 0) {
        return 0/*false*/;
    }
    if (!(attr = s_GetAttrByType(i1->type/*==i2->type*/)))
        return 0/*false*/;
    return attr->ops.Equal ? attr->ops.Equal(&i1->u, &i2->u) : 1;
}


extern const char* SERV_HostOfInfo(const SSERV_Info* info)
{
    const SSERV_Attr* attr;
    if (!info->vhost  ||  !(attr = s_GetAttrByType(info->type)))
        return 0;
    return (const char*) &info->u + attr->ops.SizeOf(&info->u);
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
    char*       args = 0;
    SSERV_Info* info;
    const char* c;

    assert(!isspace((unsigned char)(**str)));
    for (c = *str;  *c;  ++c) {
        if (isspace((unsigned char)(*c))) {
            size_t len = (size_t)(c - *str);
            assert(len);
            if (!(args = strndup(*str, len)))
                return 0;
            while (*c  &&  isspace((unsigned char)(*c)))
                ++c;
            break;
        }
    }
    info = SERV_CreateNcbidInfoEx(0, 0, args, add);
    if (info)
        *str = c;
    if (args)
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
    size_t args_len;
    SSERV_Info* info;

    if (!args)
        args_len = 1;
    else if (strcmp(args, "''"/*special case*/) == 0)
        args_len = 1, args = 0;
    else
        args_len = strlen(args) + 1;
    add += args_len;
    if ((info = (SSERV_Info*) malloc(sizeof(SSERV_Info) + add)) != 0) {
        info->type   = fSERV_Ncbid;
        info->host   = host;
        info->port   = port;
        info->mode   = 0;
        info->site   = fSERV_Local;
        info->time   = 0;
        info->coef   = 0.0;
        info->rate   = 0.0;
        info->mime_t = eMIME_T_Undefined;
        info->mime_s = eMIME_Undefined;
        info->mime_e = eENCOD_None;
        info->algo   = SERV_DEFAULT_ALGO;
        info->vhost  = 0;
        info->extra  = 0;
        memset(&info->addr, 0, sizeof(info->addr));
        info->u.ncbid.args = (TNCBI_Size) sizeof(info->u.ncbid);
        memcpy(SERV_NCBID_ARGS(&info->u.ncbid), args ? args : "", args_len);
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
static char* s_Standalone_Write(size_t reserve, const USERV_Info* u)
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
        info->mode   = 0;
        info->site   = fSERV_Local;
        info->time   = 0;
        info->coef   = 0.0;
        info->rate   = 0.0;
        info->mime_t = eMIME_T_Undefined;
        info->mime_s = eMIME_Undefined;
        info->mime_e = eENCOD_None;
        info->algo   = SERV_DEFAULT_ALGO;
        info->vhost  = 0;
        info->extra  = 0;
        memset(&info->addr, 0, sizeof(info->addr));
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
    char*       path, *args;
    SSERV_Info* info;
    const char* c;

    if (!**str)
        return 0;
    assert(!isspace((unsigned char)(**str)));
    for (c = *str;  ;  ++c) {
        if (!*c  ||  isspace((unsigned char)(*c))) {
            size_t len = (size_t)(c - *str);
            assert(len);
            if (!(path = strndup(*str, len)))
                return 0;
            while (*c  &&  isspace((unsigned char)(*c)))
                ++c;
            break;
        }
    }
    if ((args = strchr(path, '?')) != 0)
        *args++ = '\0';
    info = SERV_CreateHttpInfoEx(type, 0, 0, path, args, add);
    if (info)
        *str = c;
    free(path);
    return info;
}


static SSERV_Info* s_HttpGet_Read(const char** str, size_t add)
{
    return s_HttpAny_Read(fSERV_HttpGet, str, add);
}


static SSERV_Info* s_HttpPost_Read(const char** str, size_t add)
{
    return s_HttpAny_Read(fSERV_HttpPost, str, add);
}


static SSERV_Info* s_Http_Read(const char** str, size_t add)
{
    return s_HttpAny_Read(fSERV_Http, str, add);
}


static size_t s_Http_SizeOf(const USERV_Info* u)
{
    const char* path = SERV_HTTP_PATH(&u->http);
    const char* args = SERV_HTTP_ARGS(&u->http);
    return sizeof(u->http) + (size_t)((args + strlen(args) + 1) - path);
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
    size_t path_len, args_len;
    SSERV_Info* info;

    if (type & (unsigned int)(~fSERV_Http))
        return 0;
    if (!path  ||  !*path)
        return 0;
    else
        path_len =               strlen(path) + 1;
#if 1 /* NB: have to use this for ABI compatibility */
    args_len = args  &&  *args ? strlen(args) + 1 : 1;
#else
    if (!args  ||  !*args)
        path_len--, args_len = 1;
    else
        args_len =               strlen(args) + 1;
#endif
    add += path_len + args_len;
    if ((info = (SSERV_Info*) malloc(sizeof(SSERV_Info) + add)) != 0) {
        info->type   = type;
        info->host   = host;
        info->port   = port;
        info->mode   = 0;
        info->site   = fSERV_Local;
        info->time   = 0;
        info->coef   = 0.0;
        info->rate   = 0.0;
        info->mime_t = eMIME_T_Undefined;
        info->mime_s = eMIME_Undefined;
        info->mime_e = eENCOD_None;
        info->algo   = SERV_DEFAULT_ALGO;
        info->vhost  = 0;
        info->extra  = 0;
        memset(&info->addr, 0, sizeof(info->addr));
        info->u.http.path = (TNCBI_Size) sizeof(info->u.http);
        info->u.http.args = (TNCBI_Size)(info->u.http.path + path_len);
        memcpy(SERV_HTTP_PATH(&info->u.http), path ? path : "", path_len);
        memcpy(SERV_HTTP_ARGS(&info->u.http), args ? args : "", args_len);
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

static char* s_Firewall_Write(size_t reserve, const USERV_Info* u)
{
    const char* name = SERV_TypeStr(u->firewall.type);
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
        info->mode   = 0;
        info->site   = fSERV_Local;
        info->time   = 0;
        info->coef   = 0.0;
        info->rate   = 0.0;
        info->mime_t = eMIME_T_Undefined;
        info->mime_s = eMIME_Undefined;
        info->mime_e = eENCOD_None;
        info->algo   = SERV_DEFAULT_ALGO;
        info->vhost  = 0;
        info->extra  = 0;
        memset(&info->addr, 0, sizeof(info->addr));
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
static char* s_Dns_Write(size_t reserve, const USERV_Info* u)
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


SSERV_Info* SERV_CreateDnsInfoEx(unsigned int host,
                                 size_t       add)
{
    SSERV_Info* info = (SSERV_Info*) malloc(sizeof(SSERV_Info) + add);

    if (info) {
        info->type   = fSERV_Dns;
        info->host   = host;
        info->port   = 0;
        info->mode   = 0;
        info->site   = fSERV_Local;
        info->time   = 0;
        info->coef   = 0.0;
        info->rate   = 0.0;
        info->mime_t = eMIME_T_Undefined;
        info->mime_s = eMIME_Undefined;
        info->mime_e = eENCOD_None;
        info->algo   = SERV_DEFAULT_ALGO;
        info->vhost  = 0;
        info->extra  = 0;
        memset(&info->addr, 0, sizeof(info->addr));
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
            size_t len = kSERV_Attr[i].len;
            if (strncasecmp(tag, kSERV_Attr[i].tag, len) == 0
                &&  (!tag[len]  ||  isspace((unsigned char) tag[len])))
                return &kSERV_Attr[i];
        }
    }
    return 0;
}
