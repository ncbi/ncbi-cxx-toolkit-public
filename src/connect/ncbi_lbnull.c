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
 *   Low-level API to resolve an NCBI service name to server meta-addresses
 *   by short-cutting service names directly into DNS hostnames.
 *
 */

#include "ncbi_ansi_ext.h"
#include "ncbi_lbnull.h"
#include "ncbi_priv.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>

#define NCBI_USE_ERRCODE_X   Connect_LBSM
 

#define CONN_PORT_LBNULL            5555


/* LBNULL settings */

#define REG_CONN_LBNULL_DOMAIN      DEF_CONN_REG_SECTION "_" "LBNULL_DOMAIN"

#define REG_CONN_LBNULL_DEBUG       DEF_CONN_REG_SECTION "_" "LBNULL_DEBUG"

#define REG_CONN_LBNULL_PORT        DEF_CONN_REG_SECTION "_" "LBNULL_PORT"


#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/

static SSERV_Info* s_GetNextInfo(SERV_ITER, HOST_INFO*);
static void        s_Reset      (SERV_ITER);
static void        s_Close      (SERV_ITER);

static const SSERV_VTable kLbnullOp = {
    s_GetNextInfo, 0/*Feedback*/, 0/*Update*/, s_Reset, s_Close, "LBNULL"
};

#ifdef __cplusplus
} /* extern "C" */
#endif /*__cplusplus*/


struct SLBNULL_Data {
    unsigned       debug:1;  /* Debug output       */
    unsigned       reset:1;
    TSERV_TypeOnly type;     /* Target server type */
    SSERV_Info*    info;     /* Resolved info avail*/
    unsigned short port;     /* Default port#      */
    const char     host[1];  /* Host to resolve    */
};


static int/*bool*/ s_Resolve(SERV_ITER iter)
{
    struct SLBNULL_Data* data = (struct SLBNULL_Data*) iter->data;
    TSERV_TypeOnly type = data->type;
    TNCBI_IPv6Addr ipv6;
    unsigned int ipv4;
    SSERV_Info* info;
                                 
    if (!SOCK_gethostbynameEx6(&ipv6, data->host, data->debug ? eOn : eDefault))
        return 0;

    assert(!NcbiIsEmptyIPv6(&ipv6));
    if (!(ipv4 = NcbiIPv6ToIPv4(&ipv6, 0)))
        ipv4 = (unsigned int)(-1);

    if (type & fSERV_Dns)
        info = SERV_CreateDnsInfo(ipv4);
    else if (type &= fSERV_Http)
        info = SERV_CreateHttpInfo(type, ipv4, data->port, "/" , 0);
    else
        info = SERV_CreateStandaloneInfo(ipv4, data->port);
    if (!info) {
        CORE_LOGF_ERRNO_X(84, eLOG_Error, errno,
                          ("[%s]  Unable to create server info", iter->name));
        return 0;
    }
    info->time = LBSM_DEFAULT_TIME + iter->time;
    info->rate = LBSM_DEFAULT_RATE;
    memcpy(&info->addr, &ipv6, sizeof(info->addr)); 

    if (data->debug) {
        char* infostr = SERV_WriteInfo(info);
        CORE_LOGF(eLOG_Note,
                  ("[%s]  %s", iter->name, infostr ? infostr : "<NULL>"));
        if (infostr)
            free(infostr);
    }
        
    if (!(data->info = SERV_CopyInfoEx(info, iter->reverse_dns ? iter->name : ""))) {
        CORE_LOGF_ERRNO_X(85, eLOG_Error, errno,
                          ("[%s]  Unable to store server info", iter->name));
    }
    free(info);

    return data->info ? 1/*T*/ : 0/*F*/;
}


static SSERV_Info* s_GetNextInfo(SERV_ITER iter, HOST_INFO* host_info)
{
    struct SLBNULL_Data* data = (struct SLBNULL_Data*) iter->data;
    SSERV_Info* info;

    assert(data);

    CORE_TRACEF(("Enter LBNULL::s_GetNextInfo(\"%s\")", iter->name));
    if (!data->info  &&  !data->reset) {
        CORE_TRACEF(("Leave LBNULL::s_GetNextInfo(\"%s\"): EOF", iter->name));
        return 0;
    }

    data->reset = 0/*false*/;
    if (!data->info  &&  !s_Resolve(iter)) {
        CORE_LOGF_X(86, eLOG_Error,
                    ("[%s]  Unable to resolve", iter->name));
        return 0;
    }

    info = data->info;
    assert(info);
    data->info = 0;

    if (host_info)
        *host_info = 0;
    CORE_TRACEF(("Leave LBNULL::s_GetNextInfo(\"%s\"): \"%s\" %p",
                 iter->name, SERV_NameOfInfo(info), info));
    return info;
}


static void s_Reset(SERV_ITER iter)
{
    struct SLBNULL_Data* data = (struct SLBNULL_Data*) iter->data;
    assert(data);
    CORE_TRACEF(("Enter LBNULL::s_Reset(\"%s\"): %u", iter->name,
                 data->info ? 1 : 0));
    if (data->info) {
        free(data->info);
        data->info = 0;
    }
    data->reset = 1/*true*/;
    CORE_TRACEF(("Leave LBNULL::s_Reset(\"%s\")", iter->name));
}


static void s_Close(SERV_ITER iter)
{
    struct SLBNULL_Data* data = (struct SLBNULL_Data*) iter->data;
    assert(data  &&  !data->info); /*s_Reset() had to be called before*/
    CORE_TRACEF(("Enter LBNULL::s_Close(\"%s\")", iter->name));
    iter->data = 0;
    free(data);
    CORE_TRACEF(("Leave LBNULL::s_Close(\"%s\")", iter->name));
}


#define isdash(s)  ((s) == '-'  ||  (s) == '_')

static int/*bool*/ x_CheckDomain(const char* domain)
{
    int/*bool*/ dot = *domain == '.' ? 1/*true*/ : 0/*false*/;
    const char* ptr = dot ? ++domain : domain;
    int/*bool*/ alpha = 0/*false*/;
    size_t len;

    if (!*ptr)
        return 0/*false: just dot(root) or empty*/;
    for ( ;  *ptr;  ++ptr) {
        if (*ptr == '.') {
            if (dot  ||  (alpha  &&  isdash(ptr[-1])))
                return 0/*false: double dot or trailing dash*/;
            dot = 1/*true*/;
            continue;
        }
        if ((dot  ||  ptr == domain)  &&  isdash(*ptr))
            return 0/*false: leading dash */;
        dot = 0/*false*/;
        if (isdigit((unsigned char)(*ptr)))
            continue;
        if (!isalpha((unsigned char)(*ptr))  &&  !isdash(*ptr))
            return 0/*false: bad character*/;
        /* at least one regular "letter" seen */
        alpha = 1/*true*/;
    }
    len = (size_t)(ptr - domain);
    assert(len);
    if (domain[len - 1] == '.')
        verify(--len);
    if (!alpha) {
        unsigned int temp;
        ptr = NcbiStringToIPv4(&temp, domain, len);
        assert(!ptr  ||  ptr > domain);
        if (ptr == domain + len)
            return 0/*false: IPv4 instead of domain*/;
    } else if (isdash(ptr[-1]))
        return 0/*false: trailing dash*/;
    return 1/*true*/;
}


/***********************************************************************
 *  EXTERNAL
 ***********************************************************************/

const SSERV_VTable* SERV_LBNULL_Open(SERV_ITER iter, SSERV_Info** info)
{
    char host[CONN_HOST_LEN + 1];
    TSERV_TypeOnly type, types;
    struct SLBNULL_Data* data;
    size_t len, domlen;
    unsigned long port;
    char* domain;
    char val[40];

    assert(iter  &&  !iter->data  &&  !iter->op);
    /* No wildcard or external processing */
    if (iter->ismask)
        return 0;
    assert(iter->name  &&  *iter->name);
    if (iter->external)
        return 0;

    type = SERV_GetImplicitServerTypeInternalEx(iter->name, fSERV_Standalone);
    if (!(type & (fSERV_Standalone | fSERV_Http | fSERV_Dns)))
        return 0;

    /* Can process fSERV_Any (basically meaning fSERV_Standalone),
     * and explicit fSERV_Standalone, fSERV_Http and fSERV_Dns only */
    types = iter->types & ~fSERV_Stateless;
    if (types  &&  !(type &= types))
        return 0;

    CORE_TRACEF(("SERV_LBNULL_Open(\"%s\")", iter->name));

    if (iter->arg) {
        assert(iter->arglen);
        CORE_LOGF_X(87, eLOG_Error,
                    ("[%s]  Argument affinity lookup not supported by LBNULL:"
                     " %s%s%s%s%s", iter->name, iter->arg, &"="[!iter->val],
                     &"\""[!iter->val], iter->val ? iter->val : "",
                     &"\""[!iter->val]));
        return 0;
    }
    CORE_TRACEF(("[%s]  LBNULL using server type \"%s\"",
                 iter->name, SERV_TypeStr(type)));

    port = 0;
    if (!ConnNetInfo_GetValueService(iter->name, REG_CONN_PORT,
                                     val, sizeof(val), 0)) {
        port = (unsigned long)(-1L);
    } else if (!*val  &&
               !ConnNetInfo_GetValueInternal(0, REG_CONN_LBNULL_PORT,
                                             val, sizeof(val), 0)) {
        port = (unsigned long)(-1L);
    }
    if (port/*== (unsigned long)(-1L)*/) {
        CORE_LOGF_X(88, eLOG_Error,
                    ("[%s]  Cannot obtain default port number from registry",
                     iter->name));
        return 0;
    }
    assert(!port);
    if (*val) {
        if (isdigit((unsigned char)(*val))) {
            char* end;
            errno = 0;
            port = strtoul(val, &end, 0);
            if (errno  ||  *end  ||  port > 0xFFFF)
                port = 0;
        }
        if (!port) {
            CORE_LOGF_X(89, eLOG_Error,
                        ("[%s]  Bad default port number \"%s\" for LBNULL",
                         iter->name, val));
            return 0;
        }
        assert(port);
    } else {
        if (type & fSERV_Dns)
            ; /*none*/
        else if (type & fSERV_Http)
            port = CONN_PORT_HTTP;
        else if ((!type  ||  type == fSERV_Standalone)  &&  iter->reverse_dns)
            port = CONN_PORT_MSSQL;
        else
            port = CONN_PORT_LBNULL;
    }
    CORE_TRACEF(("[%s]  LBNULL using port number %lu", iter->name, port));

    if ((len = strlen(iter->name)) >= sizeof(host)) {
        CORE_LOGF_X(90, eLOG_Error,
                    ("[%s]  Name too long for LBNULL",
                     iter->name));
        return 0;
    }
    memcpy(host, iter->name, ++len);
    domlen = sizeof(host) - len;
    domain = host + len;

    if (!ConnNetInfo_GetValueInternal(0, REG_CONN_LBNULL_DOMAIN,
                                      domain, domlen, 0)) {
        CORE_LOGF_X(91, eLOG_Error,
                    ("[%s]  Cannot obtain domain name from registry",
                     iter->name));
        return 0;
    }
    if (!*domain) {
        domlen = 0;
    } else if (!x_CheckDomain(domain)) {
        CORE_LOGF_X(92, eLOG_Error,
                    ("[%s]  Bad domain name \"%s\" for LBNULL",
                     iter->name, domain));
        return 0;
    } else {
        domlen = strlen(domain);
        assert(domlen > 1  ||  *domain != '.');
        if (domain[domlen - 1] == '.')
            domlen--;
        if (*domain == '.')
            memmove(domain - 1, domain, domlen--);
        else
            domain[-1] = '.';
        len += domlen;
        assert(len < sizeof(host));
        host[len] = '\0';
    }
    strlwr(host);
    CORE_TRACEF(("[%s]  LBNULL using host name \"%s\"", iter->name, host));

    if (!(data = (struct SLBNULL_Data*) calloc(1, sizeof(*data) + len))) {
        CORE_LOGF_ERRNO_X(93, eLOG_Error, errno,
                          ("[%s]  LBNULL failed to allocate for SLBNULL_Data",
                           iter->name));
        return 0;
    }

    data->debug = ConnNetInfo_Boolean(ConnNetInfo_GetValueInternal
                                      (0, REG_CONN_LBNULL_DEBUG,
                                       val, sizeof(val), 0));
    data->type =                  type;
    data->port = (unsigned short) port;
    memcpy((char*) data->host, host, len + 1);

    iter->data = data;

    if (!s_Resolve(iter)) {
        CORE_LOGF(eLOG_Trace,
                  ("SERV_LBNULL_Open(\"%s\"): Service not found",
                   iter->name));
        s_Close(iter);
        return 0;
    }

    /* call GetNextInfo subsequently if info is actually needed */
    if (info)
        *info = 0;
    CORE_TRACEF(("SERV_LBNULL_Open(\"%s\"): success", iter->name));
    return &kLbnullOp;
}
