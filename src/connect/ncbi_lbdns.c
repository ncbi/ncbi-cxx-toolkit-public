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

#include "ncbi_lb.h"
#include <connect/ncbi_ipv6.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#ifdef NCBI_OS_DARWIN
#  define BIND_8_COMPAT  1
#endif /*NCBI_OS_DARWIN*/
#include <arpa/nameser.h>
#include <netinet/in.h>
#include <resolv.h>

#define NCBI_USE_ERRCODE_X   Connect_LBSM  /* errors: 31 and up */


#define SizeOf(a)                (sizeof(a) / sizeof((a)[0]))
#ifdef  max
#undef  max
#endif/*max*/
#define max(a, b)                ((a) > (b) ? (a) : (b))

#define SERVNSD_TXT_RR_PORT_LEN  (sizeof(SERVNSD_TXT_RR_PORT)-1)

#define LBDNS_INITIAL_ALLOC      32


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

static const SSERV_VTable kLbdnsOp = {
    s_GetNextInfo, 0/*Feedback*/, 0/*Update*/, s_Reset, s_Close, "LBDNS"
};

#ifdef __cplusplus
} /* extern "C" */
#endif /*__cplusplus*/


struct SLBDNS_Data {
    unsigned int   host;     /* LB DNS server host */
    unsigned short port;     /* LB DNS server port */
    unsigned       debug:1;  /* Debug output       */
    unsigned       check:1;  /* Private check mode */
    unsigned       empty:1;  /* No more data       */ 
    unsigned           :13;  /* Reserved           */
    const char*    domain;   /* Domain name to use:
                                no lead/trail '.'  */
    size_t         domlen;   /* Domain name length */
    size_t         a_cand;   /* Allocated elements */
    size_t         n_cand;   /* Used elements      */
    SLB_Candidate  cand[1];  /* "a_cand" elements  */
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
        sprintf(szbuf, " (%u)", ns_rr_rdlen(*rr));
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


static int unpack_rr(const unsigned char* msg, const unsigned char* eom,
                     const unsigned char* ptr, ns_rr* rr, int/*bool*/ qn,
                     ELOG_Level level)
{
    const char* what = qn ? "QN" : "RR";
    int len, size;

    memset(rr, 0, sizeof(*rr));
    if ((len = dn_expand(msg, eom, ptr, rr->name, sizeof(rr->name))) <= 0) {
        CORE_LOGF(level, ("DNS %s cannot expand name", what));
        return -1;
    }
    ptr += len;
    size = qn ? NS_QFIXEDSZ : NS_RRFIXEDSZ;
    if (ptr + size > eom) {
        CORE_LOGF(level, ("DNS %s overrun", what));
        return -1;
    }
    assert(NS_QFIXEDSZ  == NS_INT16SZ*2);
    assert(NS_RRFIXEDSZ == NS_INT16SZ*2 + NS_INT32SZ + NS_INT16SZ);
    NS_GET16(rr->type,     ptr);
    NS_GET16(rr->rr_class, ptr);
    if (!qn) {
        char buf[40];
        NS_GET32(rr->ttl,      ptr);
        NS_GET16(rr->rdlength, ptr);
        if (!rr->rdlength) {
            CORE_LOGF(level == eLOG_Trace ? eLOG_Trace : eLOG_Warning,
                      ("DNS RR %s RDATA empty", x_TypeStr(rr->type, buf)));
        } else if (ptr + rr->rdlength > eom) {
            CORE_LOGF(level,
                      ("DNS RR %s RDATA overrun",
                       x_TypeStr(rr->type, buf)));
            return -1;
        }
        size += rr->rdlength;
        rr->rdata = ptr;
    }
    return len + size;
}


static int skip_rr(const unsigned char* ptr, const unsigned char* eom,
                   int/*bool*/ qn)
{
    const char* what = qn ? "QN" : "RR";
    int len, size;

    if ((len = dn_skipname(ptr, eom)) <= 0) {
        CORE_LOGF(eLOG_Error, ("DNS %s cannot skip name", what));
        return -1;
    }
    ptr += len;
    size = qn ? NS_QFIXEDSZ : NS_RRFIXEDSZ;
    if (ptr + size > eom) {
        CORE_LOGF(eLOG_Error, ("DNS %s overrun", what));
        return -1;
    }
    if (!qn) {
        unsigned short rdlen;
        ptr += NS_INT16SZ*2 + NS_INT32SZ;
        NS_GET16(rdlen, ptr);
        if (ptr + rdlen > eom) {
            CORE_LOG(eLOG_Error, "DNS RR RDATA overrun");
            return -1;
        }
        size += rdlen;
    }
    return len + size;
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


static int/*bool*/ x_AddInfo(SERV_ITER iter, SSERV_Info* info)
{
    const char* name = SERV_NameOfInfo(info); /*name==NULL iff info==NULL*/
    struct SLBDNS_Data* data = (struct SLBDNS_Data*) iter->data;
    size_t n;

    if (!name) {
        assert(!info);
        CORE_LOGF_ERRNO(eLOG_Error, errno,
                        ("LBDNS cannot create entry for \"%s\"", iter->name));
        return 0/*failure(NULL info)*/;
    }
    assert(info);
    assert(info->port  ||  info->type == fSERV_Dns);
    assert(info->host  ||  info->type == fSERV_Standalone);
    assert(info->type == fSERV_Dns  ||  info->type == fSERV_Standalone);
    for (n = 0;  n < data->n_cand;  ++n) {
        if (SERV_EqualInfo(info, data->cand[n].info)
            &&  strcasecmp(name, SERV_NameOfInfo(data->cand[n].info)) == 0) {
            char* infostr = SERV_WriteInfo(info);
            CORE_LOGF(eLOG_Warning,
                      ("LBDNS ignoring duplicate entry: %s%s%s %s",
                       &"\""[!name], name ? name : "NULL",
                       &"\""[!name], infostr ? infostr : "<NULL>"));
            if (infostr)
                free(infostr);
            free(info);
            return 1/*fake success*/;
        }
    }
    if (data->n_cand == data->a_cand) {
        n = data->a_cand << 1;
        data = (struct SLBDNS_Data*) realloc(iter->data, sizeof(*data)
                                             + (n - 1) * sizeof(data->cand));
        if (!data) {
            CORE_LOGF_ERRNO(eLOG_Error, errno,
                            ("LBDNS cannot add entry for \"%s\"", iter->name));
            free(info);
            return 0/*failure*/;
        }
        iter->data = data;
        data->a_cand = n;
    }
    assert(data->n_cand < data->a_cand);
    data->cand[data->n_cand++].info = info;
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


static int/*bool*/ x_UpdateHost(SERV_ITER iter, const char* fqdn,
                                const TNCBI_IPv6Addr* addr)
{
    struct SLBDNS_Data* data = (struct SLBDNS_Data*) iter->data;
    int/*bool*/ done = 0/*false*/;
    unsigned int host;
    size_t n;

    assert(!NcbiIsEmptyIPv6(addr));
    host = NcbiIPv6ToIPv4(addr, 0);
    for (n = 0;  n < data->n_cand;  ++n) {
        SSERV_Info* x_info, *info = (SSERV_Info*) data->cand[n].info;
        const char* name = SERV_NameOfInfo(info);
        char buf[INET6_ADDRSTRLEN];
        if (strcasecmp(fqdn, name) != 0)
            continue;
        if (!info->host) {
            assert(NcbiIsEmptyIPv6(&info->addr));
            info->host = host ? host : (unsigned int)(-1);
            info->addr = *addr;
            x_info     = info;
        } else if (!host  &&  info->host == NcbiIPv6ToIPv4(&info->addr, 0)) {
            if ((x_info = SERV_CopyInfoEx(info, name)) != 0) {
                x_info->host = (unsigned int)(-1);
                x_info->addr = *addr;
            }
        } else if ( host  &&  info->host == (unsigned int)(-1)) {
            if ((x_info = SERV_CopyInfoEx(info, name)) != 0) {
                x_info->host = host;
                NcbiIPv4ToIPv6(&x_info->addr, host, 0);
            }
        } else {
            /* NB: "buf" is always '\0'-terminated, even on error */
            NcbiAddrToString(buf, sizeof(buf), addr);
            CORE_LOGF(eLOG_Warning,
                      ("LBDNS cannot re-update entry @%p with host \"%s\": %s",
                       info, fqdn, buf));
            continue;
        }
        if (x_info != info) {
            if (!x_AddInfo(iter, x_info))
                x_info = 0;
        } else if (data->debug) {
            NcbiAddrToString(buf, sizeof(buf), addr);
            CORE_LOGF(eLOG_Note,
                      ("LBDNS updating entry @%p with host \"%s\": %s",
                       info, fqdn, buf));
        }
        if (x_info) {
            if (done) {
                NcbiAddrToString(buf, sizeof(buf), addr);
                CORE_LOGF(eLOG_Warning,
                          ("LBDNS multiple entries updated with host \"%s\":"
                           " %s", fqdn, buf));
            } else
                done = 1/*true*/;
        }
    }
    return done;
}


static void x_UpdatePort(SERV_ITER iter, unsigned short port)
{
    struct SLBDNS_Data* data = (struct SLBDNS_Data*) iter->data;
    size_t n;
    assert(port);
    for (n = 0;  n < data->n_cand;  ++n) {
        SSERV_Info* info = (SSERV_Info*) data->cand[n].info;
        if (!info->port) {
            assert(info->host  &&  info->type == fSERV_Dns);
            info->port = port;
            if (data->debug) {
                CORE_LOGF(eLOG_Note,
                          ("LBDNS updating entry @%p with port \"%s\": %hu",
                           info, SERV_NameOfInfo(info), port));
            }
        }
    }
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


static int/*bool*/ dns_srv(SERV_ITER iter, const unsigned char* msg,
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
                  ("DNS SRV RR RDATA too short: %hu", rdlen));
        return 0/*false*/;
    }
    memset(&srv, 0, sizeof(srv));
    NS_GET16(srv.priority, rdata);
    NS_GET16(srv.weight,   rdata);
    NS_GET16(srv.port,     rdata);
    if ((rv = dn_expand(msg, eom, rdata, target, sizeof(target))) <= 0) {
        CORE_LOG(eLOG_Error, "DNS SRV RR cannot expand target");
        return 0/*false*/;
    }
    if (&rdata[rv] > &start[rdlen]) {
        CORE_LOG(eLOG_Error, "DNS SRV RR target overrun");
        return 0/*false*/;
    }
    if (((const struct SLBDNS_Data*) iter->data)->debug) {
        CORE_LOGF(eLOG_Note,
                  ("DNS SRV RR %s -> %s:%hu %hu %hu", fqdn,
                   *target ? target : ".",
                   srv.port, srv.priority, srv.weight));
    }
    if (&rdata[rv] != &start[rdlen]) {
        assert(&rdata[rv] < &start[rdlen]);
        CORE_LOGF(eLOG_Warning,
                  ("DNS SRV RR %lu/%hu byte(s) remain unparsed",
                   (unsigned long)(&start[rdlen] - &rdata[rv]), rdlen));
    }
    if (!target[0]  ||  (target[0] == '.'  &&  !target[1])) {
        /* service down */
        if (srv.port | srv.priority | srv.weight)
            CORE_LOG(eLOG_Warning, "DNS SRV RR blank target dirty");
        return -1/*true, special*/;
    } else if (!srv.port) {
        CORE_LOG(eLOG_Error, "DNS SRV RR zero port");
        return 0/*false*/;
    }
    x_BlankInfo(&x_info, fSERV_Standalone);
    x_info.time += iter->time;
    x_info.port  = srv.port;
    x_info.rate  = srv.priority
        ? (11.0 - max(srv.priority, 10)) / LBSM_DEFAULT_RATE   : srv.weight
        ? x_RoundUp((srv.weight * SERV_MAXIMAL_RATE) / 0xFFFF) : 1.0;
    return x_AddInfo(iter, SERV_CopyInfoEx(&x_info, target));
}


static void dns_txt(SERV_ITER iter, const char* fqdn,
                    unsigned short rdlen, const unsigned char* rdata)
{
    unsigned int len = 0;
    while (len < rdlen) {
        char buf[40];
        unsigned short slen = *rdata++;
        if ((len += 1 + slen) > rdlen) {
            CORE_LOG(eLOG_Error, "DNS TXT RR RDATA overrun");
            return;
        }
        if (slen > SERVNSD_TXT_RR_PORT_LEN 
            &&  slen - SERVNSD_TXT_RR_PORT_LEN < sizeof(buf)
            &&  isdigit(rdata[SERVNSD_TXT_RR_PORT_LEN])
            &&  strncasecmp((const char*) rdata, SERVNSD_TXT_RR_PORT,
                            SERVNSD_TXT_RR_PORT_LEN) == 0) {
            unsigned short port;
            int n;
            if (((const struct SLBDNS_Data*) iter->data)->debug) {
                CORE_LOGF(eLOG_Note,
                          ("DNS TXT RR %s: \"%.*s\"", fqdn,
                           (int) slen, (const char*) rdata));
            }
            rdata += SERVNSD_TXT_RR_PORT_LEN;
            slen  -= SERVNSD_TXT_RR_PORT_LEN;
            memcpy(buf, rdata, slen);
            buf[slen] = '\0';
            assert(strlen(buf) < sizeof(buf));
            if (sscanf(buf, "%hu%n", &port, &n) > 0 && n == (int) slen && port)
                x_UpdatePort(iter, port);
        }
        rdata += slen;
    }
}


static int/*bool*/ dns_a(SERV_ITER iter, ns_type qtype, ns_type rtype,
                         const char* fqdn,
                         unsigned short rdlen, const unsigned char* rdata)
{
    const struct SLBDNS_Data* data = (const struct SLBDNS_Data*) iter->data;
    char buf[INET6_ADDRSTRLEN];
    TNCBI_IPv6Addr ipv6;
    unsigned short len;
    unsigned int ipv4;
    SSERV_Info x_info;

    assert(sizeof(ipv4) == NS_INADDRSZ);
    assert(sizeof(ipv6) == NS_IN6ADDRSZ);
    if (rtype == ns_t_a  &&  NS_INADDRSZ <= rdlen) {
        memcpy(&ipv4, rdata, sizeof(ipv4));
        if (data->debug) {
            CORE_LOGF(eLOG_Note,
                      ("DNS A RR %s @ %s", fqdn,
                       inet_ntop(AF_INET, &ipv4,
                                 buf, (TSOCK_socklen_t) sizeof(buf))));
        }
        if (!ipv4  ||  ipv4 == (unsigned int)(-1)) {
            SOCK_ntoa(ipv4, buf, sizeof(buf));
            CORE_LOGF(eLOG_Error,
                      ("DNS A RR bad IPv4 ignored: %s", buf));
            return 0/*failure*/;
        }
        verify(NcbiIPv4ToIPv6(&ipv6, ipv4, 0));
        len = NS_INADDRSZ;
    } else if (rtype == ns_t_aaaa  &&  NS_IN6ADDRSZ <= rdlen) {
        memcpy(&ipv6, rdata, sizeof(ipv6));
        if (data->debug) {
            CORE_LOGF(eLOG_Note,
                      ("DNS AAAA RR %s @ %s", fqdn,
                       inet_ntop(AF_INET6, &ipv6,
                                 buf, (TSOCK_socklen_t) sizeof(buf))));
        }
        if (NcbiIsEmptyIPv6(&ipv6)
            ||  NcbiIPv6ToIPv4(&ipv6, 0) == (unsigned int)(-1)) {
            NcbiIPv6ToString(buf, sizeof(buf), &ipv6);
            CORE_LOGF(eLOG_Error,
                      ("DNS AAAA RR bad IPv6 ignored: %s", buf));
            return 0/*failure*/;
        }
        len = NS_IN6ADDRSZ;
    } else {
        CORE_LOGF(eLOG_Error,
                  ("DNS %s RR RDATA bad size: %hu",
                   x_TypeStr(rtype, buf), rdlen));
        return 0/*failure*/;
    }
    if (len < rdlen) {
        CORE_LOGF(eLOG_Warning,
                  ("DNS %s RR %u/%hu byte(s) remain unparsed",
                   x_TypeStr(rtype, buf), rdlen - len, rdlen));
    }
    if (qtype == ns_t_srv)
        return x_UpdateHost(iter, fqdn, &ipv6);
    x_BlankInfo(&x_info, fSERV_Dns);
    x_info.time += iter->time;
    x_info.rate  = LBSM_DEFAULT_RATE;
    x_info.addr  = ipv6;
    if (!(x_info.host = NcbiIPv6ToIPv4(&ipv6, 0)))
        x_info.host = (unsigned int)(-1);
    return x_AddInfo(iter, SERV_CopyInfoEx(&x_info, fqdn));
}


static const char* dns_cname(unsigned int/*bool*/ debug,
                             const unsigned char* msg,
                             const unsigned char* eom,
                             const char*          fqdn,
                             unsigned short       rdlen,
                             const unsigned char* rdata)
{
    char cname[NS_MAXDNAME];
    const char* retval;
    int rv;
    if ((rv = dn_expand(msg, eom, rdata, cname, sizeof(cname))) <= 0) {
        CORE_LOG(eLOG_Error, "DNS CNAME RR cannot expand cname");
        return 0/*failure*/;
    }
    if (rv > (int) rdlen) {
        CORE_LOG(eLOG_Error, "DNS CNAME RR cname overrun");
        return 0/*failure*/;
    }
    if (debug) {
        CORE_LOGF(eLOG_Note,
                  ("DNS CNAME RR %s -> %s",
                   fqdn, cname));
    }
    if (rv != (int) rdlen) {
        assert(rv < (int) rdlen);
        CORE_LOGF(eLOG_Warning,
                  ("DNS CNAME RR %d/%hu byte(s) remain unparsed",
                   (int) rdlen - rv, rdlen));
    }
    if (!(retval = strdup(strlwr(cname)))) {
        CORE_LOGF_ERRNO(eLOG_Error, errno,
                        ("DNS CNAME RR cannot store cname \"%s\" for \"%s\"",
                         cname, fqdn));
        return 0/*failure*/;
    }
    return retval;
}


static int/*bool*/ same_domain(const char* a, const char* b)
{
    size_t lena = strlen(a);
    size_t lenb = strlen(b);
    if (lena  &&  a[lena - 1] == '.')
        lena--;
    if (lenb  &&  b[lenb - 1] == '.')
        lenb--;
    return lena == lenb  &&  strncasecmp(a, b, lena) == 0 ? 1/*T*/ : 0/*F*/;
}


static const unsigned char* x_ProcessReply(SERV_ITER iter,
                                           const struct SLBDNS_Data* data,
                                           const char* fqdn, ns_type type,
                                           const unsigned char* msg,
                                           const unsigned char* eom,
                                           const unsigned char* ptr,
                                           unsigned short count[3])
{
    /* NB: we do not "check" authority section altogether, and cut corners on
     * doing thorough checks in other sections of the reply, for efficiency. */
    int/*bool*/ done = 0/*false*/;
    const char* cname = 0;
    int n;

    assert(iter->data == data);
    assert(count[0]/*ans*/  &&  (type == ns_t_srv  ||  type == ns_t_any));
    for (n = 0;  n < 3;  ++n) {
        unsigned short c;
        for (c = 0;  c < count[n];  ++c) {
            ns_rr rr;
            int skip = n  &&  ((n & 1)/*auth*/  ||  type != ns_t_srv/*adtl*/);
            int rv = (skip
                      ? skip_rr(ptr, eom, 0)
                      : unpack_rr(msg, eom, ptr, &rr, 0, eLOG_Error));
            if (rv < 0)
                goto out;
            ptr += rv;
            if (skip)
                continue;
            if (ns_rr_class(rr) != ns_c_in  ||  !ns_rr_rdlen(rr))
                continue;
            if (!n  &&  type == ns_t_srv  &&
                ns_rr_type(rr) != ns_t_srv  &&  ns_rr_type(rr) != ns_t_cname) {
                continue;
            }
            if (!n  &&  !same_domain(fqdn, ns_rr_name(rr))) {
                CORE_LOGF(eLOG_Warning,
                          ("DNS reply AN %u \"%s\" mismatch QN \"%s\"",
                           c + 1, ns_rr_name(rr), fqdn));
                continue;
            }
            if (ns_rr_type(rr) == ns_t_cname) {
                /* special CNAME processing: replace fqdn */
                if (!(n | c)) {
                    cname = dns_cname(data->debug, msg, eom, ns_rr_name(rr),
                                      ns_rr_rdlen(rr), ns_rr_rdata(rr));
                    if (!cname) {
                        assert(!done);
                        return 0/*failed*/;
                    }
                    fqdn = cname;
                    done = 1/*true*/;
                } else {
                    CORE_LOGF(eLOG_Warning,
                              ("DNS CNAME RR misplaced @A%c %u",
                               "RN"[!n], c + 1));
                }
                continue;
            }
            if (!n  &&  type == ns_t_srv) {
                assert(ns_rr_type(rr) == ns_t_srv);
                rv = dns_srv(iter, msg, eom,
                             ns_rr_name(rr), ns_rr_rdlen(rr), ns_rr_rdata(rr));
                if (rv) {
                    if (rv < 0  &&  data->n_cand) {
                        CORE_LOG(eLOG_Warning,
                                 "DNS SRV RR blank target misplaced");
                    } else
                        done = 1/*true*/;
                }
                continue;
            }
            if (!n  &&  type != ns_t_srv  &&  ns_rr_type(rr) == ns_t_txt) {
                dns_txt(iter,
                        ns_rr_name(rr), ns_rr_rdlen(rr), ns_rr_rdata(rr));
                continue;
            }
            if (ns_rr_type(rr) != ns_t_a  &&  ns_rr_type(rr) != ns_t_aaaa)
                continue;
            rv = dns_a(iter, type, ns_rr_type(rr),
                       ns_rr_name(rr), ns_rr_rdlen(rr), ns_rr_rdata(rr));
            if (rv)
                done = 1/*true*/;
        }
    }

 out:
    if (cname)
        free((void*) cname);
    assert(ptr  &&  ptr <= eom);
    return done ? ptr : 0/*failed*/;
}


/* This also advances the pointer to the beginning of the answer section */
static const unsigned char* x_VerifyReply(const char* fqdn,
                                          const unsigned char* msg,
                                          const unsigned char* eom,
                                          unsigned short count[3])
{
    const HEADER* hdr = (const HEADER*) msg;
    const unsigned char* ptr;
    char buf[40];
    ns_rr qn;
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
    rv = unpack_rr(msg, eom, ptr, &qn, 1/*QN*/, eLOG_Error);
    if (rv < 0)
        return 0/*failed*/;
    if (ns_rr_class(qn) != ns_c_in) {
        CORE_LOGF(eLOG_Error,
                  ("DNS reply for unsupported class: %s",
                   x_ClassStr(ns_rr_class(qn), buf)));
        return 0/*failed*/;
    }
    if (ns_rr_type(qn) != ns_t_any) {
        CORE_LOGF(eLOG_Error,
                  ("DNS reply for unmatching type: %s vs. ANY queried",
                   x_TypeStr(ns_rr_type(qn), buf)));
        return 0/*failed*/;
    }
    if (!same_domain(ns_rr_name(qn), fqdn)) {
        CORE_LOGF(eLOG_Error,
                  ("DNS reply for unmatching name: \"%s\" vs. \"%s\" queried",
                   ns_rr_name(qn), fqdn));
        return 0/*failed*/;
    }
    ptr += rv;
    assert(ptr <= eom);
    if (ptr == eom) {
        CORE_LOG(eLOG_Error, "DNS reply too short to include any RR(s)");
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
    ns_rr qn;
    const HEADER* hdr = (const HEADER*) msg;
    if (hdr->rcode   ||  !(hdr->qr & hdr->aa)  ||  hdr->opcode != ns_o_query)
        return 0/*false*/;
    if (ntohs(hdr->qdcount) != 1  ||  hdr->ancount)
        return 0/*false*/;
    if (unpack_rr(msg, eom, msg + NS_HFIXEDSZ, &qn, 1/*QN*/, eLOG_Trace) < 0)
        return 0/*false*/;
    if (ns_rr_class(qn) != ns_c_in  ||  ns_rr_type(qn) != type)
        return 0/*false*/;
    return same_domain(ns_rr_name(qn), fqdn);
}


static const char* x_FormFQDN(char        fqdn[NS_MAXCDNAME + 1],
                              const char* prefix,
                              size_t      pfxlen,
                              ns_type     type,
                              const char* domain,
                              size_t      domlen)
{
    const char* zone;
    size_t zlen;
    char* ptr;

    assert(type == ns_t_srv  ||  type == ns_t_any);
    assert(pfxlen  &&  domlen);
    if (type == ns_t_srv) {
        zone = "._tcp.lb.";
        zlen = 9;
    } else {
        zone = ".lb.";
        zlen = 4;
    }
    if ((type == ns_t_srv) + pfxlen + zlen + domlen + 1 > NS_MAXCDNAME)
        return 0/*failure*/;
    ptr = fqdn;
    if (type == ns_t_srv)
        *ptr++ = '_';
    memcpy(ptr, prefix, pfxlen);
    ptr += pfxlen;
    memcpy(ptr, zone,   zlen);
    ptr += zlen;
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
    assert(type == ns_t_srv  ||  type == ns_t_any);

    if (!data->check) {
        if (type != ns_t_srv
            &&  (len < 4  ||  strcasecmp(&iter->name[len -= 3], "_lb") != 0)) {
            return 0/*failure*/;
        }
    }
    if (!x_FormFQDN(fqdn, iter->name, len, type, data->domain, data->domlen)) {
        CORE_LOGF(eLOG_Error,
                  ("LBDNS FQDN for %s \"%s\" in \"%s\": Name too long",
                   x_TypeStr(type, errbuf), iter->name, data->domain));
        return 0/*failure*/;
    }
    CORE_TRACEF(("LBDNS query \"%s\"", fqdn));

    h_errno = errno = 0;
    memset(msg, 0, NS_HFIXEDSZ);
    rv = res_query(fqdn, ns_c_in, ns_t_any, msg, sizeof(msg));
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
        CORE_TRACEF(("LBDNS reply \"%s\": %d byte(s)", fqdn, rv));
        if (rv < NS_HFIXEDSZ) {
            CORE_LOGF(eLOG_Error,
                      ("DNS reply for \"%s\" too short: %d", fqdn, rv));
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

    if (data->debug  &&  rv)
        x_DumpMsg(msg, eom);
    if (err) {
        if (err > 0)
            err = 0/*false*/;
        CORE_LOGF_ERRNO(rv ? eLOG_Trace : eLOG_Error, err ? x_error : 0,
                        ("DNS lookup failure \"%s\": %s", fqdn, errstr));
        return !err/*failure/success(but nodata)*/;
    }
    assert(NS_HFIXEDSZ <= (size_t)(eom - msg));

    if (!(ptr = x_VerifyReply(fqdn, msg, eom, count)))
        return 0/*failure*/;
    assert(msg < ptr);

    CORE_TRACE("LBDNS processing DNS reply");
    if (!(ptr = x_ProcessReply(iter, data, fqdn, type, msg, eom, ptr, count)))
        return 0/*failure*/;
    if (ptr < eom) {
        assert(msg < ptr);
        CORE_LOGF(eLOG_Warning,
                  ("DNS reply %lu/%d byte(s) remain unparsed",
                   (unsigned long)(eom - ptr), rv));
    }
    return 1/*success*/;
}


static void x_Finalize(SERV_ITER iter)
{
    struct SLBDNS_Data* data = (struct SLBDNS_Data*) iter->data;
    size_t n = 0;

    CORE_TRACEF(("LBDNS finalizing result-set for \"%s\"", iter->name));
    while (n < data->n_cand) {
        SSERV_Info* info = (SSERV_Info*) data->cand[n].info;
        const char* drop = 0/*reason*/;
        if (info->host) {
            size_t s;
            const char* name = SERV_NameOfInfo(info);
            assert(name  &&  *name);
            if (iter->reverse_dns) {
                char* ptr;
                if ((ptr = strchr(name, '.')) != 0)
                    *ptr = '\0';
                strupr((char*) name);
                if (info->type == fSERV_Standalone) {
                    info->type  = fSERV_Dns;
                    info->u.dns.name = 1/*true*/;
                    assert(info->port);
                } else for (s = 0;  s < data->n_cand;  ++s) {
                    const SSERV_Info* skip = data->cand[s].info;
                    assert(skip->type == fSERV_Dns);
                    if (SERV_EqualInfo(skip, info)) {
                        drop = "duplicate";
                        break;
                    }
                }
                assert(*name);
            } else
                *((char*) name) = '\0';
            if (!drop) {
                for (s = 0;  s < iter->n_skip;  ++s) {
                    const SSERV_Info* skip = iter->skip[s];
                    if (*name) {
                        assert(iter->reverse_dns  &&  SERV_NameOfInfo(skip));
                        if (strcasecmp(SERV_NameOfInfo(skip), name) == 0
                            &&  ((skip->type == fSERV_Dns  &&  !skip->host)
                                 ||  SERV_EqualInfo(skip, info))) {
                            break;
                        }
                    } else if (SERV_EqualInfo(skip, info))
                        break;
                    if (iter->reverse_dns  &&  skip->type == fSERV_Dns
                        &&  skip->host == info->host
                        &&  (!skip->port  ||  skip->port == info->port)) {
                        break;
                    }
                }
                if (s >= iter->n_skip) {
                    data->cand[n++].status = info->rate; /*FIXME, temp*/
                    continue;
                }
                drop = "excluded";
            }
        } else {
            assert(info->type == fSERV_Standalone);
            drop = "incomplete";
        }
        verify(drop);
        CORE_TRACEF(("LBDNS dropping @%p: %s", info, drop));
        if (n < --data->n_cand) {
            memmove(data->cand + n, data->cand + n + 1,
                    (data->n_cand - n) * sizeof(data->cand));
        }
        free(info);
    }

    if (!data->n_cand  &&  (iter->types & fSERV_Dns)
        &&  !iter->last  &&  !iter->n_skip) {
        SSERV_Info x_info;
        x_BlankInfo(&x_info, fSERV_Dns);
        x_info.time += iter->time;
        if (!(data->cand[0].info = SERV_CopyInfoEx(&x_info, ""))) {
            CORE_LOGF(eLOG_Error,
                      ("LBDNS cannot create dummy entry for \"%s\"",
                       iter->name));
        } else {
            data->cand[0].status = 0.0;
            if (data->debug) {
                char* infostr = SERV_WriteInfo(data->cand[0].info);
                CORE_LOGF(eLOG_Note,
                          ("LBDNS adding dummy entry %p %s",
                           data->cand[0].info,
                           infostr ? infostr : "<NULL>"));
                if (infostr)
                    free(infostr);
            }
            data->n_cand = 1;
        }
    }
    CORE_TRACEF(("LBDNS done ready result-set for \"%s\": %lu",
                 iter->name, (unsigned long) data->n_cand));
}


static int/*bool*/ x_Resolve(SERV_ITER iter)
{
    int/*bool*/ rv = 0/*false*/;
    CORE_TRACEF(("LBDNS resolving \"%s\"", iter->name));
    if (!(iter->types & ~fSERV_Stateless) || (iter->types & fSERV_Standalone))
        rv |= x_ResolveType(iter, ns_t_srv);
    if (iter->types & fSERV_Dns)
        rv |= x_ResolveType(iter, ns_t_any);
    CORE_TRACEF(("LBDNS returning \"%s\": %s", iter->name,
                 rv ? "located" : "unknown"));
    if (rv)
        x_Finalize(iter);
    else
        assert(!((const struct SLBDNS_Data*) iter->data)->n_cand);
    return rv;
}


static int/*bool*/ s_Resolve(SERV_ITER iter)
{
    const struct SLBDNS_Data* data = (const struct SLBDNS_Data*) iter->data;
    struct sockaddr_in ns_save;
    int ns_count, ns_retry, rv;
    u_long ns_options;
    res_state r;

    assert(!data->n_cand  &&  !data->empty);

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
        /* This option, in general, should allow sending max payload size (our
         * provided answer size) to the server -- that's a good thing!  But the
         * current glibc behavior for this option is to always override with
         * 1200 -- and that's bad! -- because servnsd would comply.  If nothing
         * is specified, servnsd uses 2048 (per RFC3226, 3) by default, so...*/
#  if 0
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

    if (!data->n_cand)
        ((struct SLBDNS_Data*) data)->empty = 1/*true*/;
    return rv;
}


static SSERV_Info* s_GetNextInfo(SERV_ITER iter, HOST_INFO* host_info)
{
    struct SLBDNS_Data* data = (struct SLBDNS_Data*) iter->data;
    SSERV_Info* info;

    CORE_TRACEF(("LBDNS getnextinfo(\"%s\"): %lu%s", iter->name,
                 (unsigned long) data->n_cand, data->empty ? ", EOF" : ""));
    assert(!data->empty  ||  !data->n_cand);
    if (!data->n_cand) {
        if (!data->empty)
            s_Resolve(iter);
        if ( data->empty) {
            CORE_TRACEF(("LBDNS getnextinfo(\"%s\"): EOF", iter->name));
            assert(!data->n_cand);
            return 0/*EOF*/;
        }
    }
    info = (SSERV_Info*) data->cand[0].info;
    info->rate = data->cand[0].status;
    if (--data->n_cand) {
        memmove(data->cand, data->cand + 1,
                data->n_cand * sizeof(data->cand));
    } else
        data->empty = 1/*true*/;
    if (host_info)
        *host_info = 0;
    CORE_TRACEF(("LBDNS getnextinfo(\"%s\"): %p", iter->name, info));
    return info;
}


static void s_Reset(SERV_ITER iter)
{
    struct SLBDNS_Data* data = (struct SLBDNS_Data*) iter->data;
    CORE_TRACEF(("LBDNS reset(\"%s\"): %lu", iter->name,
                 (unsigned long) data->n_cand));
    assert(data);
    if (data->n_cand) {
        size_t n;
        for (n = 0;  n < data->n_cand;  ++n) {
            assert(data->cand[n].info);
            free((void*) data->cand[n].info);
        }
        data->n_cand = 0;
    }
    data->empty = 0/*false*/;
}


static void s_Close(SERV_ITER iter)
{
    struct SLBDNS_Data* data = (struct SLBDNS_Data*) iter->data;
    CORE_TRACEF(("LBDNS close(\"%s\")", iter->name));
    iter->data = 0;
    assert(data  &&  !data->n_cand); /*s_Reset() had to be called before*/
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


/* Assumes domain has been verified.  Skips the initial "[.]lb." (if any).
 * Return a copy of "domain" stripped of any leading and trailing dot(s). */
static const char* x_CopyDomain(const char* domain)
{
    size_t len;
    assert(*domain);
    if (*domain == '.')
        ++domain;
    len = strlen(domain);
    if (len > 1  &&  strncasecmp(domain, "lb", 2) == 0
        &&  (!domain[2]  ||  domain[2] == '.')) {
        if (!domain[2]  ||  !(len -= 3)) {
            errno = EINVAL;
            return 0/*failure*/;
        }
        domain += 3;
    } else
        assert(len);
    assert(*domain);
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
    TSERV_TypeOnly types;
    const char* domain;
    unsigned long port;

    assert(iter  &&  !iter->data  &&  !iter->op);
    /* No wildcard or external processing */
    if (iter->ismask)
        return 0;
    assert(iter->name  &&  *iter->name);
    if (iter->external)
        return 0;
    /* Can process fSERV_Any (basically meaning fSERV_Standalone), and explicit
     * fSERV_Standalone and/or fSERV_Dns only */
    types = iter->types & ~fSERV_Stateless;
    if (types != fSERV_Any/*0*/  &&  !(types & (fSERV_Standalone | fSERV_Dns)))
        return 0;
    if (types == fSERV_Dns) {
        /* DNS-only lookups are for the *_lb services */
        size_t len = strlen(iter->name);
        if (len < 4  ||  strcasecmp(&iter->name[len - 3], "_lb") != 0)
            return 0;
    }

    CORE_TRACEF(("LBDNS open(\"%s\")", iter->name));
    if (iter->arg) {
        assert(iter->arglen);
        CORE_LOGF(eLOG_Warning,
                  ("[%s]  Argument affinity lookup not supported by LBDNS:"
                   " %s%s%s%s%s", iter->name, iter->arg, &"="[!iter->val],
                   &"\""[!iter->val], iter->val ? iter->val : "",
                   &"\""[!iter->val]));
        goto out;
    }
    if (!(data = (struct SLBDNS_Data*) calloc(1, sizeof(*data)
                                              + (LBDNS_INITIAL_ALLOC - 1)
                                              * sizeof(data->cand)))) {
        CORE_LOG_ERRNO(eLOG_Error, errno,
                       "LBDNS failed to create private data structure");
        return 0;
    }
    data->debug = ConnNetInfo_Boolean(ConnNetInfo_GetValue
                                      (0, REG_CONN_LBDNS_DEBUG,
                                       val, sizeof(val), 0));
    data->check = ConnNetInfo_Boolean(ConnNetInfo_GetValue
                                      (0, "CONN_LBDNS_CHECK", /*private*/
                                       val, sizeof(val), 0));
    data->a_cand = LBDNS_INITIAL_ALLOC;
    iter->data = data;

    if (!ConnNetInfo_GetValue(0, REG_CONN_LBDNS_DOMAIN, val, sizeof(val), 0))
        goto out;
    if (!*val) {
        domain = s_SysGetDomainName(val, sizeof(val));
        if (!x_CheckDomain(domain)) {
            CORE_LOG(eLOG_Critical,
                     "LBDNS cannot figure out system domain name");
            goto out;
        }
        CORE_TRACEF(("LBDNS found system domain \"%s\"", domain));
    } else if (!x_CheckDomain(val)) {
        CORE_LOGF(eLOG_Error, ("LBDNS bad domain name \"%s\"", val));
        goto out;
    } else
        domain = val;
    if (!(data->domain = x_CopyDomain(domain))) {
        CORE_LOGF_ERRNO(eLOG_Error, errno,
                        ("LBDNS failed to store domain name \"%s\"", domain));
        goto out;
    }
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
        CORE_TRACEF(("LBDNS open(\"%s\") okay", iter->name));
        return &kLbdnsOp;
    }

 out:
    s_Reset(iter);
    s_Close(iter);
    CORE_TRACEF(("LBDNS open(\"%s\") failed", iter->name));
    return 0/*failure*/;
}


#else


/*ARGSUSED*/
const SSERV_VTable* SERV_LBDNS_Open(SERV_ITER iter, SSERV_Info** info)
{
    /* NB: This should never be called on a non-UNIX platform */
    static void* /*bool*/ s_Once = 0/*false*/;
    if (CORE_Once(&s_Once))
        CORE_LOG(eLOG_Critical, "LBDNS only available on UNIX platform(s)");
    return 0;
}


#endif /*NCBI_OS_UNIX*/
