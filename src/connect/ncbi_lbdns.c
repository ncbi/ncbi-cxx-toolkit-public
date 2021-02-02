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

#include <connect/ncbi_ipv6.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef NCBI_OS_DARWIN
#  define BIND_8_COMPAT  1
#endif /*NCBI_OS_DARWIN*/
#include <arpa/nameser.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
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
#ifdef  max
#undef  max
#endif/*max*/
#define max(a, b)            ((a) > (b) ? (a) : (b))


#if defined(HAVE_SOCKLEN_T)  ||  defined(_SOCKLEN_T)
typedef socklen_t  TSOCK_socklen_t;
#else
typedef int        TSOCK_socklen_t;
#endif /*HAVE_SOCKLEN_T || _SOCKLEN_T*/


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
    unsigned       debug:1;  /* Debug output       */
    unsigned       empty:1;  /* No more data       */ 
    unsigned           :14;  /* Reserved           */
    const char*    domain;   /* Domain name to use:
                                no leading/trailing '.'
                             */
    size_t         domlen;   /* Domain name length */
    size_t         a_info;   /* Allocated pointers */
    size_t         n_info;   /* Used pointers      */
    SSERV_Info*    info[1];  /* "a_info" pointers  */
};


static const char* x_TypeStr(ns_type atype, char* buf)
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
    sprintf(buf, "TYPE(%hu)", (unsigned short) atype);
    return buf;
}


static const char* x_ClassStr(ns_class aclass, char* buf)
{
    switch (aclass) {
    case ns_c_in:
        return "IN";
    case ns_c_any:
        return "ANY";
    default:
        break;
    }
    sprintf(buf, "CLASS(%hu)", (unsigned short) aclass);
    return buf;
}


static const char* x_RcodeStr(unsigned short rcode, char* buf)
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
    sprintf(buf, "RCODE(%hu)", rcode);
    return buf;
}


static const char* x_OpcodeStr(unsigned short opcode, char* buf)
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
    sprintf(buf, "OPCODE(%hu)", opcode);
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


static void x_DumpHdr(const HEADER* hdr, const unsigned short count[4])
{
    char buf[128];
    CORE_LOGF(eLOG_Note,
              ("DNS %s (ID=%hu, 0x%04hX):\n"
               "\t%s\n"
               "\tQD: %hu, AN: %hu, NS: %hu, AR: %hu",
               hdr->qr ? "REPLY" : "QUERY",
               ntohs(hdr->id),
               ntohs(((const unsigned short*) hdr)[1]),
               x_FlagsStr(hdr, buf),
               count[0], count[1], count[2], count[3]));
}


static void x_DumpRR(const ns_rr* rr, const char* abbr, unsigned short n)
{
    char clbuf[40], tybuf[40], ttlbuf[40], szbuf[40];
    if (abbr) {
        sprintf(ttlbuf, " %lu", (unsigned long) ns_rr_ttl(*rr));
        sprintf(szbuf, " (%hu)", ns_rr_rdlen(*rr));
    } else 
        *ttlbuf = *szbuf = '\0';
    CORE_LOGF(eLOG_Note,
              ("%s%s %2hu: %s%s %s %s%s",
               abbr ? abbr  : "QN",
               abbr ? " RR" : "   ", n,
               ns_rr_name(*rr), ttlbuf,
               x_ClassStr(ns_rr_class(*rr), clbuf),
               x_TypeStr(ns_rr_type(*rr), tybuf), szbuf));
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
                     const unsigned char* ptr, ns_rr* rr, int/*bool*/ qn,
                     ELOG_Level level)
{
    const char* what = qn ? "QN" : "RR";
    int len, size;

    memset(rr, 0, sizeof(*rr));
    if ((len = dn_expand(msg, eom, ptr, rr->name, sizeof(rr->name))) <= 0) {
        CORE_LOGF(level, ("Error expanding %s name", what));
        return -1;
    }
    ptr += len;
    size = qn ? NS_QFIXEDSZ : NS_RRFIXEDSZ;
    if (ptr + size > eom) {
        CORE_LOGF(level, ("Cannot access %s fields", what));
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
            CORE_LOG(level, "Cannot access RR data");
            return -1;
        }
        rr->rdata = ptr;
        size += rr->rdlength;
    }
    return len + size;
}


static int skip_rr(const unsigned char* ptr, const unsigned char* eom,
                   int/*bool*/ qn)
{
    const char* what = qn ? "QN" : "RR";
    int len, size;

    if ((len = dn_skipname(ptr, eom)) <= 0) {
        CORE_LOGF(eLOG_Error, ("Error skipping %s name", what));
        return -1;
    }
    ptr += len;
    size = qn ? NS_QFIXEDSZ : NS_RRFIXEDSZ;
    if (ptr + size > eom) {
        CORE_LOGF(eLOG_Error, ("Cannot skip %s fields", what));
        return -1;
    }
    if (!qn) {
        unsigned short rdlen;
        ptr += NS_INT16SZ*2 + NS_INT32SZ;
        GETSHORT(rdlen, ptr);
        if (ptr + rdlen > eom) {
            CORE_LOG(eLOG_Error, "Cannot skip RR data");
            return -1;
        }
        size += rdlen;
    }
    return len + size;
}


static int/*bool*/ x_SameDomain(const char* a, const char* b)
{
    size_t lena = strlen(a);
    size_t lenb = strlen(b);
    if (lena  &&  a[lena - 1] == '.')
        lena--;
    if (lenb  &&  b[lenb - 1] == '.')
        lenb--;
    return lena == lenb  &&  strncasecmp(a, b, lena) == 0 ? 1/*T*/ : 0/*F*/;
}


static void x_BlankInfo(SSERV_Info* info, TSERV_TypeOnly type)
{
    assert(type == fSERV_Dns  ||  type == fSERV_Standalone);
    memset(info, 0, sizeof(*info));
    info->type   = type;
    info->site   = fSERV_Local;
    info->time   = LBSM_DEFAULT_TIME;
    info->mime_t = eMIME_T_Undefined;
    info->mime_s = eMIME_Undefined;
    info->mime_e = eENCOD_None;
    info->algo   = SERV_DEFAULT_ALGO;
}


static int/*bool*/ x_AddInfo(SERV_ITER iter, SSERV_Info* info)
{
    struct SLBDNS_Data* data = (struct SLBDNS_Data*) iter->data;
    const char* name = SERV_NameOfInfo(info);
    size_t n;

    if (!name)
        return 0/*failure*/;
    assert(info);
    for (n = 0;  n < data->n_info;  ++n) {
        if (SERV_EqualInfo(info, data->info[n])
            &&  strcasecmp(name, SERV_NameOfInfo(data->info[n])) == 0) {
            char* infostr = SERV_WriteInfo(info);
            CORE_LOGF(eLOG_Warning,
                      ("LBDNS ignoring duplicate: %s%s%s %s",
                       &"\""[!name], name ? name : "NULL",
                       &"\""[!name], infostr ? infostr : "<NULL>"));
            if (infostr)
                free(infostr);
            free(info);
            return 1/*fake success*/;
        }
    }
    if (data->n_info == data->a_info) {
        n = data->a_info << 1;
        data = (struct SLBDNS_Data*) realloc(iter->data, sizeof(*data)
                                             + (n - 1) * sizeof(data->info));
        if (!data) {
            free(info);
            return 0/*failure*/;
        }
        iter->data = data;
        data->a_info = n;
    }
    assert(data->n_info < data->a_info);
    data->info[data->n_info++] = info;
    if (data->debug) {
        char* infostr = SERV_WriteInfo(info);
        CORE_LOGF(eLOG_Note,
                  ("LBDNS adding %p %s%s%s %s", info,
                   &"\""[!name], name ? name : "NULL",
                   &"\""[!name], infostr ? infostr : "<NULL>"));
        if (infostr)
            free(infostr);
    }
    return 1/*success*/;
}



static void x_PatchInfo(SERV_ITER iter, const char* fqdn,
                        const TNCBI_IPv6Addr* addr)
{
    struct SLBDNS_Data* data = (struct SLBDNS_Data*) iter->data;
    int/*bool*/ done = 0/*false*/;
    unsigned int ipv4;
    size_t n;

    assert(!NcbiIsEmptyIPv6(addr));
    ipv4 = NcbiIPv6ToIPv4(addr, 0);
    for (n = 0;  n < data->n_info;  ++n) {
        char buf[INET6_ADDRSTRLEN];
        SSERV_Info* info = data->info[n];
        if (strcasecmp(fqdn, SERV_NameOfInfo(info)) != 0)
            continue;
        if (!info->host) {
            assert(NcbiIsEmptyIPv6(&info->addr));
            info->host = ipv4 ? ipv4 : (unsigned int)(-1);
            info->addr = *addr;
        } else if (!ipv4  &&  info->host == NcbiIPv6ToIPv4(&info->addr, 0)) {
            info->addr = *addr;
        } else if ( ipv4  &&  info->host == (unsigned int)(-1)) {
            info->host = ipv4;
        } else {
            NcbiAddrToString(buf, sizeof(buf), addr);
            CORE_LOGF(eLOG_Warning,
                      ("LBDNS ignoring duplicate \"%s\": %s", fqdn, buf));
            continue;
        }
        if (data->debug) {
            NcbiAddrToString(buf, sizeof(buf), addr);
            CORE_LOGF(eLOG_Note,
                      ("LBDNS patching \"%s\": %s", fqdn, buf));
        }
        if (done) {
            NcbiAddrToString(buf, sizeof(buf), addr);
            CORE_LOGF(eLOG_Warning,
                      ("LBDNS duplicate patch with %s", buf));
        }
        done = 1/*true*/;
    }
}


static double x_RoundUp(double rate)
{
    double tens = floor(rate / 10.0);
    return (tens + (rate - tens * 10.0 < 10.0 ? 1.0 : 0.0)) * 10.0;
}


typedef struct ns_rr_srv {
    unsigned short priority;
    unsigned short weight;
    unsigned short port;
} ns_rr_srv;


static int/*bool*/ add_srv(SERV_ITER iter, const unsigned char* msg,
                           const unsigned char* eom, const char* fqdn,
                           unsigned short rdlen, const unsigned char* rdata)
{
    const unsigned char* start = rdata;
    char target[NS_MAXDNAME];
    SSERV_Info x_info;
    ns_rr_srv srv;
    int rv;

    if (rdlen <= sizeof(ns_rr_srv)) {
        CORE_LOGF(eLOG_Error,
                  ("SRV record too short: %hu", rdlen));
        return 0/*false*/;
    }
    memset(&srv, 0, sizeof(srv));
    GETSHORT(srv.priority, rdata);
    GETSHORT(srv.weight,   rdata);
    GETSHORT(srv.port,     rdata);
    rv = dn_expand(msg, eom, rdata, target, sizeof(target));
    if (rv < 0) {
        CORE_LOG(eLOG_Error, "Cannot expand SRV target");
        return 0/*false*/;
    }
    if (rdata + rv < start + rdlen) {
        CORE_LOGF(eLOG_Warning,
                  ("DNS SRV RR has %lu/%hu byte(s) unparsed",
                   (unsigned long)(&start[rdlen] - &rdata[rv]),
                   rdlen));
    } else
        assert(&rdata[rv] == &start[rdlen]);
    if (((const struct SLBDNS_Data*) iter->data)->debug) {
        CORE_LOGF(eLOG_Note,
                  ("DNS SRV %s -> %s:%hu %hu %hu", fqdn,
                   *target ? target : ".",
                   srv.port, srv.priority, srv.weight));
    }
    if (!target[0]  ||  (target[0] == '.'  &&  !target[1])) {
        /* service down */
        if (srv.port | srv.priority | srv.weight)
            CORE_LOG(eLOG_Warning, "DNS SRV non-empty service down");
        return -1/*true*/;
    }
    if (!srv.port) {
        CORE_LOG(eLOG_Error, "DNS SRV zero port");
        return 0/*false*/;
    }
    x_BlankInfo(&x_info, fSERV_Standalone);
    x_info.port = srv.port;
    x_info.rate = srv.priority
        ? (11.0 - max(srv.priority, 10)) / LBSM_DEFAULT_RATE
        : x_RoundUp((srv.weight * SERV_MAXIMAL_RATE) / 0xFFFF);
    return x_AddInfo(iter, SERV_CopyInfoEx(&x_info, target));
}


static int/*bool*/ add_a(SERV_ITER iter, ns_type type, const char* fqdn,
                         unsigned short rdlen, const unsigned char* rdata)
{
    const struct SLBDNS_Data* data = (const struct SLBDNS_Data*) iter->data;
    char buf[INET6_ADDRSTRLEN];
    TNCBI_IPv6Addr ipv6;
    unsigned int ipv4;
    SSERV_Info x_info;

    assert(sizeof(ipv4) == NS_INADDRSZ);
    assert(sizeof(ipv6) == NS_IN6ADDRSZ);
    switch (rdlen) {
    case NS_INADDRSZ:
        memcpy(&ipv4, rdata, sizeof(ipv4));
        if (data->debug) {
            CORE_LOGF(eLOG_Note,
                      ("DNS A %s @ %s", fqdn,
                       inet_ntop(AF_INET, &ipv4,
                                 buf, (TSOCK_socklen_t) sizeof(buf))));
        }
        if (!ipv4  ||  ipv4 == (unsigned int)(-1)) {
            SOCK_ntoa(ipv4, buf, sizeof(buf));
            CORE_LOGF(eLOG_Error,
                      ("DNS A bad IPv4 ignored: %s", buf));
            return 0/*failure*/;
        }
        verify(NcbiIPv4ToIPv6(&ipv6, ipv4, 0));
        break;
    case NS_IN6ADDRSZ:
        memcpy(&ipv6, rdata, sizeof(ipv6));
        if (data->debug) {
            CORE_LOGF(eLOG_Note,
                      ("DNS AAAA %s @ %s", fqdn,
                       inet_ntop(AF_INET6, &ipv6,
                                 buf, (TSOCK_socklen_t) sizeof(buf))));
        }
        if (NcbiIsEmptyIPv6(&ipv6)
            ||  NcbiIPv6ToIPv4(&ipv6, 0) == (unsigned int)(-1)) {
            NcbiIPv6ToString(buf, sizeof(buf), &ipv6);
            CORE_LOGF(eLOG_Error,
                      ("DNS AAAA bad IPv6 ignored: %s", buf));
            return 0/*failure*/;
        }
        break;
    default:
        CORE_LOGF(eLOG_Error,
                  ("DNS A/AAAA RR bad size %hu", rdlen));
        return 0/*failure*/;
    }
    if (type == ns_t_srv) {
        x_PatchInfo(iter, fqdn, &ipv6);
        return 1/*success*/;
    }
    x_BlankInfo(&x_info, fSERV_Dns);
    x_info.rate = LBSM_DEFAULT_RATE;
    x_info.addr = ipv6;
    if (!(x_info.host = NcbiIPv6ToIPv4(&ipv6, 0)))
        x_info.host = (unsigned int)(-1);
    return x_AddInfo(iter, SERV_CopyInfoEx(&x_info, fqdn));
}


static int/*bool*/ x_ProcessReply(SERV_ITER iter,
                                  const char* fqdn, ns_type type,
                                  const unsigned char* msg,
                                  const unsigned char* eom,
                                  const unsigned char* ptr,
                                  unsigned short count[3])
{
    int/*bool*/ retval = 0/*false*/;
    unsigned short c;
    int n;

    assert(count[0]/*ans*/  &&  (type == ns_t_srv  ||  type == ns_t_any));
    for (n = 0;  n < 3;  ++n) {
        for (c = 0;  c < count[n];  ++c) {
            ns_rr rr;
            int skip = n  &&  ((n & 1)/*auth*/  ||  type != ns_t_srv/*adtl*/);
            int rv = (skip
                      ? skip_rr(ptr, eom, 0)
                      : unpack_rr(msg, eom, ptr, &rr, 0, eLOG_Error));
            if (rv < 0)
                return retval;
            ptr += rv;
            if (skip)
                continue;
            if (ns_rr_class(rr) != ns_c_in)
                continue;
            if (!n  &&  type != ns_t_any  &&  type != ns_rr_type(rr))
                continue;
            if (!n  &&  !x_SameDomain(fqdn, ns_rr_name(rr)))
                continue;
            if (!n  &&  type == ns_t_srv) {
                rv = add_srv(iter, msg, eom,
                             fqdn, ns_rr_rdlen(rr), ns_rr_rdata(rr));
                if (rv) {
                    if (rv < 0  &&  !c) {
                        CORE_LOG(eLOG_Warning,
                                 "DNS SRV empty RR misplaced, ignored");
                    } else
                        retval = 1/*true*/;
                }
                continue;
            }
            if (ns_rr_type(rr) != ns_t_a  &&  ns_rr_type(rr) != ns_t_aaaa)
                continue;
            assert((!n  &&  type == ns_t_any)  ||  (n  &&  type == ns_t_srv));
            rv = add_a(iter, type,
                       ns_rr_name(rr), ns_rr_rdlen(rr), ns_rr_rdata(rr));
            if (rv)
                retval = 1/*true*/;
        }
    }
    return retval;
}


static const unsigned char* x_DumpMsg(const unsigned char* msg,
                                      const unsigned char* eom)
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

    if (ptr >= eom)
        return ptr;
    for (n = 0;  n < SizeOf(count);  ++n) {
        unsigned short c = 0;
        while (c < count[n]) {
            ns_rr rr;
            int rv = unpack_rr(msg, eom, ptr, &rr, !n, eLOG_Trace);
            if (rv < 0)
                return ptr;
            x_DumpRR(&rr, kSecAbbr[n], ++c);
            ptr += rv;
        }
    }
    return ptr;
}


static const unsigned char* x_VerifyReply(const char* fqdn, ns_type type,
                                          const unsigned char* msg,
                                          const unsigned char* eom,
                                          unsigned short count[3])
{
    const HEADER* hdr = (const HEADER*) msg;
    const unsigned char* ptr;
    char buf[40];
    ns_rr rr;
    int rv;

    assert(eom - msg >= NS_HFIXEDSZ);
    if (hdr->rcode/*!=ns_r_noerror*/) {
        CORE_LOGF(eLOG_Error,
                  ("DNS reply indicates an error: %s",
                   x_RcodeStr(hdr->rcode, buf)));
        return 0/*failed*/;
    }
    if (!hdr->qr) {
        CORE_LOG(eLOG_Error,
                 ("DNS reply is a query, not a reply"));
        return 0/*failed*/;
    }
    if (hdr->opcode != ns_o_query) {
        CORE_LOGF(eLOG_Error,
                  ("DNS reply has unexpected opcode: %s",
                   x_OpcodeStr(hdr->opcode, buf)));
        return 0/*failed*/;
    }
    if ((count[0] = ntohs(hdr->qdcount)) != 1) {
        CORE_LOGF(eLOG_Error,
                  ("DNS reply has bad number of questions: %hu",
                   count[0]));
        return 0/*failed*/;
    }
    if (!(count[0] = ntohs(hdr->ancount))) {
        CORE_LOG(eLOG_Error, "DNS reply has no answers");
        return 0/*failed*/;
    }
    ptr = msg + NS_HFIXEDSZ;
    if (ptr == eom) {
        CORE_LOG(eLOG_Error, "DNS reply has no records");
        return 0/*failed*/;
    }
    rv = unpack_rr(msg, eom, ptr, &rr, 1/*QN*/, eLOG_Error);
    if (rv < 0)
        return 0/*failed*/;
    if (ns_rr_class(rr) != ns_c_in) {
        CORE_LOGF(eLOG_Error,
                  ("DNS reply for unsupported class: %s",
                   x_ClassStr(ns_rr_class(rr), buf)));
        return 0/*failed*/;
    }
    if (ns_rr_type(rr) != type) {
        char buf2[sizeof(buf)];
        CORE_LOGF(eLOG_Error,
                  ("DNS reply for unmatched type: %s vs. %s queried",
                   x_ClassStr(ns_rr_type(rr), buf), x_ClassStr(type, buf2)));
        return 0/*failed*/;
    }
    if (!x_SameDomain(ns_rr_name(rr), fqdn)) {
        CORE_LOGF(eLOG_Error,
                  ("DNS reply for unmatched name: \"%s\" vs. \"%s\" queried",
                   ns_rr_name(rr), fqdn));
        return 0/*failed*/;
    }
    ptr += rv;
    assert(ptr <= eom);
    if (ptr == eom) {
        CORE_LOG(eLOG_Error, "DNS reply too short to have any RRs");
        return 0/*failed*/;
    }
    count[1] = ntohs(hdr->nscount);
    count[2] = ntohs(hdr->arcount);
    return ptr/*succeeded*/;
}


static int/*bool*/ x_NoDataReply(const char* fqdn, ns_type type,
                                 const unsigned char* msg,
                                 const unsigned char* eom)
{
    ns_rr rr;
    const HEADER* hdr = (const HEADER*) msg;
    if (hdr->rcode   ||  !(hdr->qr & hdr->aa)  ||  hdr->opcode != ns_o_query)
        return 0/*false*/;
    if (ntohs(hdr->qdcount) != 1  ||  hdr->ancount)
        return 0/*false*/;
    if (unpack_rr(msg, eom, msg + NS_HFIXEDSZ, &rr, 1/*QN*/, eLOG_Trace) < 0)
        return 0/*false*/;
    if (ns_rr_class(rr) != ns_c_in  ||  ns_rr_type(rr) != type)
        return 0/*false*/;
    return x_SameDomain(ns_rr_name(rr), fqdn);
}


static const char* x_FormFQDN(char        fqdn[NS_MAXCDNAME + 1],
                              const char* prefix,
                              size_t      pfxlen,
                              ns_type     type,
                              const char* domain,
                              size_t      domlen)
{
    const char* zone;
    size_t zonlen;
    char* ptr;

    assert(pfxlen  &&  domlen);
    if (type == ns_t_srv) {
        zone   = "._tcp.lb.";
        zonlen = 9;
    } else {
        zone   = ".lb.";
        zonlen = 4;
    }
    if ((type == ns_t_srv) + pfxlen + zonlen + domlen + 1 > NS_MAXCDNAME)
        return 0/*failure*/;
    ptr = fqdn;
    if (type == ns_t_srv)
        *ptr++ = '_';
    memcpy(ptr, prefix, pfxlen);
    ptr += pfxlen;
    memcpy(ptr, zone,   zonlen);
    ptr += zonlen;
    memcpy(ptr, domain, domlen);
    ptr += domlen;
    *ptr++ = '.';
    *ptr = '\0';
    assert(ptr - fqdn <= NS_MAXCDNAME);
    return strlwr(fqdn);
}


static int/*bool*/ x_ResolveType(SERV_ITER iter, ns_type type)
{
    const struct SLBDNS_Data* data = (const struct SLBDNS_Data*) iter->data;
    size_t len = strlen(iter->name);
    const unsigned char* ptr, *eom;
    char fqdn[NS_MAXCDNAME + 1];
    unsigned short count[3];
    unsigned char msg[2048];
    int rv, err, x_error;
    const char* errstr;
    char errbuf[40];

    assert(sizeof(msg) > NS_HFIXEDSZ);
#if 0
    if (type != ns_t_srv
        &&  (len < 4  ||  strcasecmp(&iter->name[len -= 3], "_lb") != 0)) {
        return 0/*failure*/;
    }
#endif
    if (!x_FormFQDN(fqdn, iter->name, len, type, data->domain, data->domlen)) {
        CORE_LOGF(eLOG_Error,
                  ("Cannot form FQDN for \"%s\" in \"%s\": Name too long",
                   iter->name, data->domain));
        return 0/*failure*/;
    }
    CORE_TRACEF(("LBDNS %s query \"%s\"", x_TypeStr(type, errbuf), fqdn));

    errno = h_errno = 0;
    memset(msg, 0, NS_HFIXEDSZ);
    rv = res_query(fqdn, ns_c_in, type, msg, sizeof(msg));
    if (rv < 0) {
        int nodata;
        x_error = errno;
        errstr = strherror(err = h_errno);
        rv = memcchr(msg, 0, NS_HFIXEDSZ) ? (int) sizeof(msg) : 0;
        eom = msg + rv;
        /*FIXME*/
        nodata = rv  &&  x_NoDataReply(fqdn, type, msg, eom) ? 1/*T*/ : 0/*F*/;
#ifdef NO_DATA
        if (!nodata  &&  err == NO_DATA)
            nodata = 1/*true*/;
        assert(NO_DATA != -1);
#endif /*NO_DATA*/
        if (!errstr  ||  !*errstr) {
            sprintf(errbuf, "Error %d", err);
            errstr = errbuf;
        }
        err = nodata ? 1 : -1;
    } else {
        CORE_TRACEF(("LBDNS %s reply \"%s\": %d byte(s)",
                     x_TypeStr(type, errbuf), fqdn, rv));
        if (rv < NS_HFIXEDSZ) {
            CORE_LOGF(eLOG_Error,
                      ("DNS reply for \"%s\" too short %d", fqdn, rv));
            return 0/*failure*/;
        }
        if (rv >= (int) sizeof(msg)) {
            CORE_LOGF(rv > (int) sizeof(msg) ? eLOG_Error : eLOG_Warning,
                      ("DNS reply overflow: %d", rv));
            rv  = (int) sizeof(msg);
        }
        eom = msg + rv;
        err = 0;
    }

    assert((size_t)(eom - msg) <= sizeof(msg));
    if (data->debug  &&  rv  &&  (ptr = x_DumpMsg(msg, eom)) < eom  &&  !err) {
        assert(msg < ptr);
        CORE_LOGF(eLOG_Warning,
                  ("DNS reply has %lu/%d byte(s) unparsed",
                   (unsigned long)(eom - ptr), rv));
    }
    if (err) {
        if (err > 0)
            err = 0/*false*/;
        CORE_LOGF_ERRNO(err ? eLOG_Error : eLOG_Trace, err ? x_error : 0,
                        ("Error looking up \"%s\" in DNS: %s", fqdn, errstr));
        return !err/*failure/success(but nodata)*/;
    }
    assert(NS_HFIXEDSZ <= (size_t)(eom - msg));

    if (!(ptr = x_VerifyReply(fqdn, type, msg, eom, count)))
        return 0/*failure*/;

    CORE_TRACE("LBDNS processing DNS reply...");
    return x_ProcessReply(iter, fqdn, type, msg, eom, ptr, count);
}


static int/*bool*/ x_Resolve(SERV_ITER iter)
{
    int/*bool*/ rv = 0/*false*/;
    if (!iter->types  ||  (iter->types & fSERV_Standalone))
        rv |= x_ResolveType(iter, ns_t_srv);
    if (iter->types & fSERV_Dns)
        rv |= x_ResolveType(iter, ns_t_any);
    return rv;
}


static int/*bool*/ s_Resolve(SERV_ITER iter)
{
    const struct SLBDNS_Data* data = (const struct SLBDNS_Data*) iter->data;
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
#  endif /*0*/
#endif /*RES_USE_EDNS0*/
#ifdef RES_DEBUG
        if (data->debug)
            r->options |= RES_DEBUG;  /* will most likely be a NOOP, though */
#endif /*RES_DEBUG*/
        r->nsaddr  = ns_addr;
        r->nscount = 1;
        r->retry   = 1/*# of tries*/;
    } else
        r = 0;

    rv = x_Resolve(iter);

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

    if (data->empty)
        return 0/*EOF*/;
    if (!data->n_info) {
        s_Resolve(iter);
        if (!data->n_info) {
            data->empty = 1;
            return 0/*EOF*/;
        }
    }
    info = data->info[0];
    assert(info);
    if (--data->n_info) {
        memmove(data->info, data->info + 1,
                data->n_info * sizeof(data->info[0]));
    } else
        data->empty = 1;
    if (host_info)
        *host_info = 0;
    return info;
}


static void s_Reset(SERV_ITER iter)
{
    struct SLBDNS_Data* data = (struct SLBDNS_Data*) iter->data;
    assert(data);
    if (data->n_info) {
        size_t n;
        for (n = 0;  n < data->n_info;  ++n) {
            assert(data->info[n]);
            free(data->info[n]);
        }
        data->n_info = 0;
    }
    data->empty = 0;
}


static void s_Close(SERV_ITER iter)
{
    struct SLBDNS_Data* data = (struct SLBDNS_Data*) iter->data;
    iter->data = 0;
    assert(data  &&  !data->n_info); /*s_Reset() had to be called before*/
    if (data->domain)
        free((void*) data->domain);
    free(data);
}


static const char* s_SysGetDomainName(char* domain, size_t domainsize)
{
    char* p;
    assert(domain  &&  domainsize);

#if defined(NCBI_OS_CYGWIN)  ||  defined(NCBI_OS_IRIX)
    if (getdomainname(domain, domainsize) == 0
        &&  domain[0]  &&  domain[1]
        &&  strcasecmp(domain, "(none)") != 0) {
        return domain;
    }
#endif /*NCBI_OS_CYGWIN || NCBI_OS_IRIX*/

    if (SOCK_gethostbyaddr(0, domain, domainsize)
        &&  (p = strchr(domain, '.')) != 0  &&  p[1]) {
        return p;
    }

    CORE_LOCK_READ;
    if ((p = getenv("LOCALDOMAIN")) != 0) {
        size_t n = strlen(p);
        if (1 < n  &&  n < domainsize)
            memcpy(domain, p, n + 1);
        else
            domain = 0;
    } else
        domain = 0;
    CORE_UNLOCK;

    return domain;
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
    return 1 < len  &&  len <= NS_MAXCDNAME ? 1/*true*/ : 0/*false: bad size*/;
}


/* Assumes domain has been verified.
 * Return a copy of "domain" stripped of leading and trailing dot(s), if any */
static const char* x_CopyDomain(const char* domain)
{
    size_t len;
    if (domain[0] == '.')
        ++domain;
    len = strlen(domain);
    assert(len);
    if (domain[len - 1] == '.')
        --len;
    assert(len  &&  domain[0] != '.'  &&  domain[len - 1] != '.');
    return strndup(domain, len);
}



/***********************************************************************
 *  EXTERNAL
 ***********************************************************************/

const SSERV_VTable* SERV_LBDNS_Open(SERV_ITER iter, SSERV_Info** info)
{
    char val[CONN_HOST_LEN + 1];
    struct SLBDNS_Data* data;
    const char* domain;
    unsigned long port;

    /* No wildcard procesing */
    if (iter->ismask)
        return 0;
    /* Can process fSERV_Any (basically meaning fSERV_Standalone), and explicit
     * fSERV_Standalone and/or fSERV_Dns only */
    if  (iter->types != fSERV_Any
         &&  !(iter->types & (fSERV_Standalone | fSERV_Dns))) {
        return 0;
    }
    if (iter->types == fSERV_Dns) {
        /* DNS-only lookups are for the *_lb services */
        size_t len = strlen(iter->name);
        if (len < 4  ||  strcasecmp(&iter->name[len - 3], "_lb") != 0)
            return 0;
    }

    if (!(data = (struct SLBDNS_Data*) calloc(1, sizeof(*data)
                                              + (LBDNS_INITIAL_ALLOC - 1)
                                              * sizeof(data->info)))) {
        return 0;
    }
    data->debug = ConnNetInfo_Boolean(ConnNetInfo_GetValue
                                      (0, REG_CONN_LBDNS_DEBUG,
                                       val, sizeof(val), 0));
    data->a_info = LBDNS_INITIAL_ALLOC;
    iter->data = data;

    if (!ConnNetInfo_GetValue(0, REG_CONN_LBDNS_DOMAIN, val, sizeof(val), 0))
        goto out;
    if (!*val) {
        domain = s_SysGetDomainName(val, sizeof(val));
        if (!x_CheckDomain(domain)) {
            CORE_LOG(eLOG_Critical, "Cannot figure out system domain name");
            goto out;
        }
    } else if (!x_CheckDomain(val)) {
        CORE_LOGF(eLOG_Error, ("Bad LBDNS domain name \"%s\"", val));
        goto out;
    } else
        domain = val;
    if (!(data->domain = x_CopyDomain(domain)))
        goto out;
    data->domlen = strlen(data->domain);
    assert(1 < data->domlen  &&  data->domlen <= NS_MAXCDNAME);
    assert(data->domain[0] != '.'  &&  data->domain[data->domlen - 1] != '.');

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
            SOCK_HostPortToString(data->host, (unsigned short) port,
                                  val, sizeof(val));
            CORE_LOGF(data->debug ? eLOG_Note : eLOG_Trace,
                      ("LBDNS using server @ %s", val));
        }
        data->port = (unsigned short) port;
    }
    assert(!data->host == !data->port);
    CORE_LOGF(data->debug ? eLOG_Note : eLOG_Trace,
              ("LBDNS using domain name \"%s\"", data->domain));

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
