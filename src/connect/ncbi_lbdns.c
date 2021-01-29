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
 *   with the use of DNS.
 *
 */

#include "ncbi_ansi_ext.h"
#include "ncbi_lbdns.h"
#include "ncbi_once.h"

#ifdef NCBI_OS_UNIX

#include "ncbi_priv.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef NCBI_OS_DARWIN
#  define BIND_8_COMPAT  1
#endif /*NCBI_OS_DARWIN*/
#include <arpa/nameser.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NCBI_USE_ERRCODE_X   Connect_LBSM  /* errors: 31 and up */


#ifndef NS_DEFAULTPORT
#  ifdef NAMESERVER_PORT
#    define NS_DEFAULTPORT   NAMESERVER_PORT
#  else
#    define NS_DEFAULTPORT   53
#  endif /*NAMESERVER_PORT*/
#endif /*NS_DEFAULTPORT*/

#define SizeOf(a)            (sizeof(a) / sizeof((a)[0]))
#define LBDNS_INITIAL_ALLOC  32
#define LBDNS_DEBUG          1


#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/
static SSERV_Info* s_GetNextInfo(SERV_ITER, HOST_INFO*);
static void        s_Reset      (SERV_ITER);
static void        s_Close      (SERV_ITER);

static const SSERV_VTable s_op = {
    s_GetNextInfo, 0/*Feedback*/, 0/*Update*/, s_Reset, s_Close, "LBDNS"
};
#ifdef __cplusplus
} /* extern "C" */
#endif /*__cplusplus*/


struct SLBDNS_Data {
    unsigned int   host;     /* LB DNS server host */
    unsigned short port;     /* LB DNS server port */
    unsigned short flags;
    const char*    domain;   /* Domain name to use */
    size_t         a_info;   /* Allocated pointers */
    size_t         n_info;   /* Used pointers      */
    SSERV_Info*    info[1];  /* "a_info" pointers  */
};


static const char* x_TypeStr(ns_type atype, void* buf)
{
    switch (atype) {
    case ns_t_a:
        return "A";
    case ns_t_ns:
        return "NS";
    case ns_t_cname:
        return "CNAME";
    case ns_t_soa:
        return "SOA";
    case ns_t_null:
        return "NULL";
    case ns_t_ptr:
        return "PTR";
    case ns_t_hinfo:
        return "HINFO";
    case ns_t_mx:
        return "MX";
    case ns_t_txt:
        return "TXT";
    case ns_t_rp:
        return "RP";
    case ns_t_aaaa:
        return "AAAA";
    case ns_t_srv:
        return "SRV";
    case ns_t_a6:
        return "A6";
    case ns_t_opt:
        return "OPT";
    case ns_t_any:
        return "ANY";
#ifdef T_URI
    case ns_t_uri:
        return "URI";
#endif /*T_URI*/
    default:
        break;
    }
    sprintf((char*) buf, "TYPE(%d)", atype);
    return buf;
}


static const char* x_ClassStr(ns_class aclass, void* buf)
{
    switch (aclass) {
    case ns_c_in:
        return "IN";
    case ns_c_any:
        return "ANY";
    default:
        break;
    }
    sprintf((char*) buf, "CLASS(%d)", aclass);
    return buf;
}


static const char* strherror(int err)
{
    switch (err) {
#ifdef NETDB_INTERNAL
    case NETDB_INTERNAL:
        return "Internal error";
#endif /*NETDB_INTERNAL*/
#ifdef HOST_NOT_FOUND
    case HOST_NOT_FOUND:
        return "Host not found";
#endif /*HOST_NOT_FOUND*/
#ifdef TRY_AGAIN
    case TRY_AGAIN:
        return "Server failure";
#endif /*TRY_AGAIN*/
#ifdef NO_RECOVERY
    case NO_RECOVERY:
        return "Unrecoverable error";
#endif /*NO_RECOVERY*/
#ifdef NO_ADDRESS
#  if !defined(NO_DATA)  ||  NO_DATA != NO_ADDRESS
     case NO_ADDRESS:
        return "No address record found";
#  endif /*!NO_DATA || NO_DATA!=NO_ADDRESS*/
#endif /*NO_ADDRESS*/
#ifdef NO_DATA
    case NO_DATA:
        return "No data of requested type";
#endif /*NO_DATA*/
    default:
        break;
    }
    return hstrerror(err);
}


static int unpack_rr(const unsigned char* msg, const unsigned char* eom,
                     const unsigned char* ptr, ns_rr* rr, int/*bool*/ qn)
{
    const char* what = qn ? "QN" : "RR";
    int len, size;

    memset(rr, 0, sizeof(*rr));
    if ((len = dn_expand(msg, eom, ptr, rr->name, sizeof(rr->name))) < 0) {
        CORE_LOGF(eLOG_Error, ("Error expanding %s name", what));
        return -1;
    }
    ptr += len;
    size = qn ? NS_QFIXEDSZ : NS_RRFIXEDSZ;
    if (ptr + size > eom) {
        CORE_LOGF(eLOG_Error, ("Cannot access %s fields", what));
        return -1;
    }
    assert(NS_QFIXEDSZ  == NS_INT16SZ*2);
    assert(NS_RRFIXEDSZ == NS_INT16SZ*2 + NS_INT32SZ + NS_INT16SZ);
    GETSHORT(rr->type,     ptr);
    GETSHORT(rr->rr_class, ptr);
    if (!qn) {
        GETLONG (rr->ttl,      ptr);
        GETSHORT(rr->rdlength, ptr);
        if (ptr + rr->rdlength > eom) {
            CORE_LOG(eLOG_Error, "Cannot access RR data");
            return -1;
        }
        rr->rdata = ptr;
    }
    return len + size + rr->rdlength;
}


static const char* x_RcodeStr(unsigned short rcode, void* buf)
{
    switch (rcode) {
    case ns_r_noerror:
        return "NOERROR";
    case ns_r_formerr:
        return "FORMERR";
    case ns_r_servfail:
        return "SERVFAIL";
    case ns_r_nxdomain:
        return "NXDOMAIN";
    case ns_r_notimpl:
        return "NOTIMPL";
    case ns_r_refused:
        return "REFUSED";
    default:
        break;
    }
    sprintf((char*) buf, "RCODE(%hu)", rcode);
    return buf;
}


static const char* x_OpcodeStr(unsigned char opcode, void* buf)
{
    switch (opcode) {
    case ns_o_query:
        return "QUERY";
    case ns_o_iquery:
        return "IQUERY";
    case ns_o_status:
        return "STATUS";
    case ns_o_notify:
        return "NOTIFY";
    case ns_o_update:
        return "UPDATE";
    default:
        break;
    }
    sprintf((char*) buf, "OPCODE(%hu)", opcode);
    return buf;
}


static const char* x_FlagsStr(const HEADER* hdr, char* buf)
{
    char rcode[40];
    char*  ptr = (char*) x_OpcodeStr(hdr->opcode, buf);
    size_t len = strlen(ptr);
    if (ptr != buf) {
        memcpy(buf, ptr, len);
        ptr  = buf;
    }
    ptr += len;
    if (hdr->aa)
        ptr += sprintf(ptr, ", AA");
    if (hdr->tc)
        ptr += sprintf(ptr, ", TC");
    if (hdr->rd)
        ptr += sprintf(ptr, ", RD");
    if (hdr->ra)
        ptr += sprintf(ptr, ", RA");
    if (hdr->unused)
        ptr += sprintf(ptr, ", Z?");
    if (hdr->ad)
        ptr += sprintf(ptr, ", AD");
    if (hdr->cd)
        ptr += sprintf(ptr, ", CD");
    sprintf(ptr, ", %s", x_RcodeStr(hdr->rcode, rcode));
    return buf;
}


static void x_DumpHdr(const HEADER* hdr, unsigned short count[4])
{
    char buf[128];
    CORE_LOGF(eLOG_Note,
              ("DNS %s (ID=%d, 0x%04hu):\n"
               "\t%s\n"
               "\tQD: %hu, AN: %hu, NS: %hu, AR: %hu",
               hdr->qr ? "REPLY" : "QUERY",
               ntohs(hdr->id),
               ntohs(((unsigned short*) hdr)[1]),
               x_FlagsStr(hdr, buf),
               count[0], count[1], count[2], count[3]));
}


static void x_DumpRR(const ns_rr* rr, const char* abbr, unsigned short n)
{
    char clbuf[40], tybuf[40], ttlbuf[40];
    if (abbr)
        sprintf(ttlbuf, " %lu", (unsigned long) ns_rr_ttl(*rr));
    else
        *ttlbuf = '\0';
    CORE_LOGF(eLOG_Note,
              ("%s%s %2hu: %s%s %s %s",
               abbr ? abbr  : "QN",
               abbr ? " RR" : "   ", n,
               ns_rr_name(*rr), ttlbuf,
               x_ClassStr(ns_rr_class(*rr), clbuf),
               x_TypeStr(ns_rr_type(*rr), tybuf)));
}


static void x_DumpMsg(const unsigned char* msg, const unsigned char* eom)
{
    static const char* kSecAbbr[] = { 0, "AN", "NS", "AR" };
    const unsigned char* ptr = msg + NS_HFIXEDSZ;
    const HEADER* hdr = (const HEADER*) msg;
    unsigned short count[4];
    size_t n;

    assert(sizeof(*hdr) == NS_HFIXEDSZ);
    count[0] = ntohs(hdr->qdcount);
    count[1] = ntohs(hdr->ancount);
    count[2] = ntohs(hdr->nscount);
    count[3] = ntohs(hdr->arcount);
    x_DumpHdr(hdr, count);

    for (n = 0;  n < SizeOf(count);  ++n) {
        unsigned short c = 0;
        while (c < count[n]) {
            ns_rr rr;
            int rv = unpack_rr(msg, eom, ptr, &rr, !n);
            if (rv < 0)
                return;
            ptr += rv;
            x_DumpRR(&rr, kSecAbbr[n], ++c);
        }
    }
}


static const char* x_FormFQDN(char        fqdn[NS_MAXDNAME],
                              const char* prefix,
                              ns_type     type,
                              const char* domain)
{
    size_t pfxlen = strlen(prefix);
    size_t domlen = strlen(domain);
    const char* zone;
    size_t zonlen;
    char* ptr;

    assert(pfxlen  &&  domlen);
    if (type == ns_t_srv) {
        zone   = "._tcp.";
        zonlen = 6;
    } else {
        zone   = ".lb.";
        zonlen = 4;
    }
    if (pfxlen + (type == ns_t_srv) + zonlen +
        domlen + (domain[domlen - 1] != '.') >= NS_MAXDNAME) {
        return 0/*failure*/;
    }
    ptr = fqdn;
    if (type == ns_t_srv)
        *ptr++ = '_';
    memcpy(ptr, prefix, pfxlen);
    ptr += pfxlen;
    memcpy(ptr, zone,   zonlen);
    ptr += zonlen;
    memcpy(ptr, domain, domlen);
    ptr += domlen;
    if (domain[domlen - 1] != '.')
        *ptr++ = '.';
    *ptr = '\0';
    assert(ptr - fqdn <= NS_MAXDNAME);
    return strlwr(fqdn);
}


static int/*bool*/ x_ResolveType(SERV_ITER           iter,
                                 struct SLBDNS_Data* data,
                                 ns_type             type)
{
    unsigned char msg[2048];
    char fqdn[NS_MAXDNAME];
    int rv;

    if (!x_FormFQDN(fqdn, iter->name, type, data->domain)) {
        CORE_LOGF(eLOG_Error,
                  ("Cannot form FQDN for \"%s\" in \"%s\": Name too long",
                   iter->name, data->domain));
        return 0/*failure*/;
    }
    CORE_TRACEF(("LBDNS %s query \"%s\"", x_TypeStr(type, msg), fqdn));

    errno = 0;
    rv = res_search(fqdn, ns_c_in, type, msg, sizeof(msg));
    if (rv < 0) {
        int herr = h_errno, serr = errno;
        const char* herrstr = strherror(herr);
        if (!herrstr  ||  !*herrstr) {
            sprintf((char*) msg, "Error %d", herr);
            herrstr = (char*) msg;
        }
        CORE_LOGF_ERRNO(eLOG_Error, serr,
                        ("Error looking up \"%s\" in DNS: %s", fqdn, herrstr));
        return 0/*failure*/;
    }

    if (data->flags & LBDNS_DEBUG)
        x_DumpMsg(msg, msg + rv);
    return 1/*success*/;
}


static int/*bool*/ x_Resolve(SERV_ITER iter, struct SLBDNS_Data* data)
{
    int/*bool*/ rv = 0/*false*/;

    assert((void*) data == iter->data);
    if (!iter->types  ||  (iter->types & fSERV_Standalone))
        rv |= x_ResolveType(iter, data, ns_t_srv);
    if (iter->types & fSERV_Dns)
        rv |= x_ResolveType(iter, data, ns_t_any);
    return rv;
}


static int/*bool*/ s_Resolve(SERV_ITER iter)
{
    struct SLBDNS_Data* data = (struct SLBDNS_Data*) iter->data;
    struct sockaddr_in ns_save;
    int ns_count, ns_retry, rv;
    u_long ns_options;
    res_state r;

    if (data->host | data->port) {
        static void* /*bool*/ s_Init = 0/*false*/;
        struct sockaddr_in ns_addr;
        if (CORE_Once(&s_Init))
            res_init();
        memset(&ns_addr, 0, sizeof(ns_addr));
#ifdef HAVE_SIN_LEN
        ns_addr.sin_len         = sizeof(ns_addr);
#endif /*HAVE_SIN_LEN*/
        ns_addr.sin_family      = AF_INET;
        ns_addr.sin_addr.s_addr =       data->host;
        ns_addr.sin_port        = htons(data->port);
        CORE_LOCK_WRITE;
        r = &_res;
        assert(r);
        ns_retry   = r->retry;
        ns_options = r->options;
        ns_count   = r->nscount;
        ns_save    = r->nsaddr;
#ifdef RES_IGNTC
        r->options |= RES_IGNTC;
#endif /*RES_IGNTC*/
#ifdef RES_USE_EDNS0
#  if 0
        /* This option, in general, should allow sending max payload size (our
         * provided answer size) to the server -- that's a good thing!  But the
         * current glibc behavior for this option is to always override with
         * 1200 -- and that's bad! -- because servnsd would comply.  If nothing
         * is specified, servnsd uses 2048 (per RFC3226, 3) by default...
         */
        r->options |= RES_USE_EDNS0;
#  endif
#endif /*RES_USE_EDNS0*/
#ifdef RES_DEBUG
        if (data->flags & LBDNS_DEBUG)
            r->options |= RES_DEBUG;  /* will most likely be NOOP, though */
#endif /*RES_DEBUG*/
        r->nsaddr  = ns_addr;
        r->nscount = 1;
        r->retry   = 1/*# of tries*/;
    } else
        r = 0;

    rv = x_Resolve(iter, data);

    if (r) {
        r->options = ns_options;
        r->nsaddr  = ns_save;
        r->nscount = ns_count;
        r->retry   = ns_retry;
        CORE_UNLOCK;
    }
    return rv;
}


static SSERV_Info* s_GetNextInfo(SERV_ITER iter, HOST_INFO* host_info)
{
    struct SLBDNS_Data* data = (struct SLBDNS_Data*) iter->data;
    SSERV_Info* info;

    if (!data->n_info)
        return 0/*EOF*/;
    info = data->info[0];
    assert(info);
    if (--data->n_info) {
        memmove(data->info, data->info + 1,
                data->n_info * sizeof(data->info[0]));
    }
    if (host_info)
        *host_info = 0;
    return info;
}


static void s_Reset(SERV_ITER iter)
{
    struct SLBDNS_Data* data = (struct SLBDNS_Data*) iter->data;
    if (data  &&  data->n_info) {
        size_t n;
        for (n = 0;  n < data->n_info;  ++n) {
            assert(data->info[n]);
            free(data->info[n]);
        }
        data->n_info = 0;
    }
}


static void s_Close(SERV_ITER iter)
{
    struct SLBDNS_Data* data = (struct SLBDNS_Data*) iter->data;
    iter->data = 0;
    assert(!data->n_info); /*s_Reset() had to be called before*/
    if (data->domain)
        free((void*) data->domain);
    free(data);
}


/***********************************************************************
 *  EXTERNAL
 ***********************************************************************/

const SSERV_VTable* SERV_LBDNS_Open(SERV_ITER iter, SSERV_Info** info)
{
    char val[CONN_HOST_LEN + 1];
    struct SLBDNS_Data* data;
    unsigned long port;

    if (iter->ismask)
        return 0;
    /* Can process fSERV_Any (basically meaning fSERV_Standalone), and explicit
     * fSERV_Standalone and/or fSERV_Dns only */
    if  (iter->types != fSERV_Any
         &&  !(iter->types & (fSERV_Standalone | fSERV_Dns))) {
        return 0;
    }
    if (!(data = (struct SLBDNS_Data*) calloc(1, sizeof(*data)
                                              + (LBDNS_INITIAL_ALLOC - 1)
                                              * sizeof(data->info)))) {
        return 0;
    }
    data->a_info = LBDNS_INITIAL_ALLOC;
    iter->data = data;

    if (!ConnNetInfo_GetValue(0, REG_CONN_LBDNS_DOMAIN, val, sizeof(val), 0))
        goto out;
    if (!*val) {
        const char* p;
        if (!SOCK_gethostbyaddr(0, val, sizeof(val)))
            goto out;
        if (!(p = strchr(val, '.')))
            goto out;
        if (!(data->domain = strdup(++p)))
            goto out;
        CORE_TRACEF(("LBDNS using domain \"%s\"", data->domain));
    } else if (!(data->domain = strdup(val)))
        goto out;
    if (!ConnNetInfo_GetValue(0, REG_CONN_LBDNS_PORT, val, sizeof(val), 0))
        goto out;
    if (!*val)
        port = 0;
    else if (!isdigit((unsigned char)(*val)))
        goto out;
    else {
        char* end;
        errno = 0;
        port = strtoul(val, &end, 0);
        if (errno  ||  *end  ||  port > 0xFFFF)
            goto out;
        if (!port)
            port = NS_DEFAULTPORT;
    }
    if (port) {
        if (!ConnNetInfo_GetValue(0, REG_CONN_LBDNS_HOST, val, sizeof(val), 0))
            goto out;
        if (!*val)
            port = 0;
        else if (!(data->host = SOCK_gethostbyname(val)))
            goto out;
        else {
            CORE_DEBUG_ARG(SOCK_HostPortToString(data->host, (unsigned short)
                                                 port, val, sizeof(val)));
            CORE_TRACEF(("LBDNS using server @ %s", val));
        }
        data->port = (unsigned short) port;
    }
    assert(!data->host == !data->port);
    if (ConnNetInfo_Boolean(ConnNetInfo_GetValue(0, REG_CONN_LBDNS_DEBUG,
                                                 val, sizeof(val), 0))) {
        data->flags |= LBDNS_DEBUG;
    }

    if (s_Resolve(iter)) {
        /* call GetNextInfo subsequently if info is actually needed */
        if (info)
            *info = 0;
        return &s_op;
    }

 out:
    s_Reset(iter);
    s_Close(iter);
    return 0/*failure*/;
}


#else


/*ARGSUSED*/
const SSERV_VTable* SERV_LBDNS_Open(SERV_ITER iter, SSERV_Info** info)
{
    /* NB: This should never be called on a non-UNIX platform */
    static void* /*bool*/ s_Once = 0/*false*/;
    if (CORE_Once(&s_Once))
        CORE_LOG(eLOG_Critical, "LB DNS only available on UNIX platform(s)");
    return 0;
}


#endif /*NCBI_OS_UNIX*/
