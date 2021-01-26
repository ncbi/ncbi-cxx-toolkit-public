#ifndef CONNECT___NCBI_SERVER_INFO__H
#define CONNECT___NCBI_SERVER_INFO__H

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
 * Authors:  Anton Lavrentiev, Denis Vakatov
 *
 * File Description:
 * @file ncbi_server_info.h
 *   NCBI server meta-address info
 *
 *   Note that all server meta-addresses are allocated as single contiguous
 *   pieces of memory, which can be copied in whole with the use of
 *   'SERV_SizeOfInfo' call.  Dynamically allocated server infos can be freed
 *   with a direct call to 'free'.
 *
 *   Assumptions on the fields: all fields in the server info come in host byte
 *   order except 'host', which comes in network byte order.
 *
 */

#include <connect/ncbi_connutil.h>
#include <connect/ncbi_ipv6.h>


/** @addtogroup ServiceSupport
 *
 * @{
 */

#define SERV_DEFAULT_ALGO   eSERV_Regular
#define SERV_MINIMAL_RATE   0.001
#define SERV_MAXIMAL_RATE   100000.0
#define SERV_MINIMAL_BONUS  0.01
#define SERV_MAXIMAL_BONUS  100.0


#ifdef __cplusplus
extern "C" {
#endif


/* Server types
 */
typedef enum {
    fSERV_Ncbid      = 0x01,
    fSERV_Standalone = 0x02,
    fSERV_HttpGet    = 0x04,
    fSERV_HttpPost   = 0x08,
    fSERV_Http       = fSERV_HttpGet | fSERV_HttpPost,
    fSERV_Firewall   = 0x10,
    fSERV_Dns        = 0x20
} ESERV_Type;


/* Flag to specify an algorithm for selecting the most preferred server from
 * a set of available servers (NB: binary properties of the enumerated values
 * may be in use!).
 */
typedef enum {
    eSERV_Regular = 0,       /* Server either tied up to lavg or static      */
    eSERV_Blast = 1          /* Server is tied up to an instant host load    */
} ESERV_Algo;
typedef unsigned char TSERV_Algo;


typedef enum {
    fSERV_Stateful = 1,      /* Server requires persistent connection        */
    fSERV_Secure = 2         /* Server requires secure connection            */
} ESERV_Mode;
typedef unsigned char TSERV_Mode;


typedef enum {
    fSERV_Local = 1,         /* Server accessible only for NCBI local clients*/
    fSERV_Private = 2,       /* Server accessible only by localhost clients  */
    fSERV_Interzone = 4,     /* Server record spans LB zones within the site */
    fSERV_ZoneMask = 0xF0    /* Local zone number for the server             */
} ESERV_Site;
typedef unsigned char TSERV_Site;


/* Verbal representation of a server type (no internal spaces allowed)
 */
extern NCBI_XCONNECT_EXPORT const char* SERV_TypeStr
(ESERV_Type type
 );


/* Read server info type.
 * If successful, assign "type" and return pointer to the position in the "str"
 * immediately following the type tag.  On error, return NULL.
 */
extern NCBI_XCONNECT_EXPORT const char* SERV_ReadType
(const char* str,
 ESERV_Type* type
 );


/* Meta-addresses for various types of NCBI servers
 */
typedef struct {
    TNCBI_Size            args;
#define SERV_NCBID_ARGS(ui)     ((char*)(ui) + (ui)->args)
} SSERV_NcbidInfo;

typedef struct {
    char                  _pad; /* placeholder, not used                     */
} SSERV_StandaloneInfo;

typedef struct {
    TNCBI_Size            path;
    TNCBI_Size            args;
#define SERV_HTTP_PATH(ui)      ((char*)(ui) + (ui)->path)
#define SERV_HTTP_ARGS(ui)      ((char*)(ui) + (ui)->args)
} SSERV_HttpInfo;

typedef struct {
    ESERV_Type            type; /* type of the original server               */
} SSERV_FirewallInfo;

typedef struct {
    unsigned char/*bool*/ name; /* name presence flag                        */
} SSERV_DnsInfo;


/* Generic NCBI server meta-address
 */
typedef union {
    SSERV_NcbidInfo       ncbid;
    SSERV_StandaloneInfo  standalone;
    SSERV_HttpInfo        http;
    SSERV_FirewallInfo    firewall;
    SSERV_DnsInfo         dns;
} USERV_Info;

typedef struct {
    ESERV_Type            type; /* type of server                            */
    unsigned int          host; /* host the server running on, network b.o.  */
    unsigned short        port; /* port the server running on, host b.o.     */
    TSERV_Mode            mode; /* connection mode (stateful, secure, etc)   */
    TSERV_Site            site; /* site info (LSB: local, private; MSB: @#)  */
    TNCBI_Time            time; /* relaxation period / expiration time       */
    double                coef; /* bonus coefficient for server run locally  */
    double                rate; /* rate of the server                        */
    EMIME_Type          mime_t; /* type,                                     */
    EMIME_SubType       mime_s; /*     subtype,                              */
    EMIME_Encoding      mime_e; /*         and encoding for content-type     */
    TSERV_Algo            algo; /* rate algorithm for the server             */
    TNCBI_IPv6Addr        addr; /* IPv6 address (for host == INADDR_NONE(-1))*/
    unsigned char        vhost; /* extra (v)host namelen if non-zero (+'\0') */
    unsigned short       extra; /* extra data size if non-zero               */
    USERV_Info               u; /* server type-specific data/params          */
} SSERV_Info;


/* Constructors for the various types of NCBI server meta-addressesr45
 */
extern NCBI_XCONNECT_EXPORT SSERV_Info* SERV_CreateNcbidInfo
(unsigned int   host,           /* network byte order                        */
 unsigned short port,           /* host byte order                           */
 const char*    args
 );

extern NCBI_XCONNECT_EXPORT SSERV_Info* SERV_CreateStandaloneInfo
(unsigned int   host,           /* network byte order                        */
 unsigned short port            /* host byte order                           */
 );

extern NCBI_XCONNECT_EXPORT SSERV_Info* SERV_CreateHttpInfo
(ESERV_Type     type,           /* verified, must be one of fSERV_Http*      */
 unsigned int   host,           /* network byte order                        */
 unsigned short port,           /* host byte order                           */
 const char*    path,
 const char*    args
 );

extern NCBI_XCONNECT_EXPORT SSERV_Info* SERV_CreateFirewallInfo
(unsigned int   host,           /* original server's host in net byte order  */
 unsigned short port,           /* original server's port in host byte order */
 ESERV_Type     type            /* type of original server, wrapped into     */
 );

extern NCBI_XCONNECT_EXPORT SSERV_Info* SERV_CreateDnsInfo
(unsigned int   host            /* network byte order, the only parameter    */
 );


/* Dump server info to a string.
 * The server type goes first, and it is followed by a single space.
 * The returned string is '\0'-terminated, and must be deallocated by 'free()'.
 */
extern NCBI_XCONNECT_EXPORT char*       SERV_WriteInfo
(const SSERV_Info* info
 );


/* Server specification consists of the following:
 * TYPE [host][:port] [server-specific_parameters] [tags]
 *
 * TYPE := { STANDALONE | NCBID | HTTP[{_GET|_POST}] | FIREWALL | DNS }
 *
 * Host must be specified as either an IP address (in dotted notation), as an
 * IPv6 address (enclosed in square brackets), or as a host name (using domain
 * notation if necessary).  Port number must be preceded by a colon.  Both host
 * and port defaults to server-specific values if omitted.  For null
 * specification ":0" can be used.
 *
 * Server-specific parameters:
 *
 *    STANDALONE servers:  None
 *                         Servers of this type do not take any arguments.
 *
 *    NCBID servers:       Arguments to CGI in addition to those specified by
 *                         the application.  Empty additional arguments should
 *                         be denoted as '' (two single quotes, back to back)
 *                         in order for the tags (that may follow) not be
 *                         treated as the additional arguments.  Note that the
 *                         arguments  must not contain any space characters.
 *
 *    HTTP* servers:       Path (required) and args (optional) in the form
 *                         path[?args] (brackets denote the optional part).
 *                         Note that no spaces are allowed within these
 *                         parameters.
 *
 *    FIREWALL servers:    Servers of this type are converted real servers of
 *                         the above types, when only accessible via FIREWALL
 *                         mode of NCBI dispatcher.  The purpose of this fake
 *                         server type is just to let the client know that the
 *                         service exists.  Additional parameter is optional
 *                         and if present, is the original type of the server
 *                         before the conversion.  Note that servers of this
 *                         type cannot be configured in LBSMD.
 *
 *    DNS servers:         Services for DNS, DB load-balancing, and dynamic
 *                         reverse-proxying (the ProxyPass directive of the
 *                         Apache HTTP daemon) at the NCBI Web entry points.
 *                         Never exposed to the outside.
 *
 * Tags may follow in no particular order but no more than one instance of each
 * tag is allowed:
 *
 *    Load average calculation algorithm for the server:
 *       A=R            (Regular=default)
 *       A=B            (Blast)
 *
 *    Bonus coefficient:
 *       B=double       [0.0 = default]
 *           specifies a multiplicative bonus given to a server run locally,
 *           when calculating its reachability rate.
 *           Special rules apply to negative/zero values:
 *           0.0 means not to use the described rate increase at all (default
 *           rate calculation is used, which only slightly increases the rates
 *           of locally run servers).
 *           Negative value denotes that a locally run server should be taken
 *           in the first place, regardless of its rate, yet if that rate is as
 *           large as the percentage (expressed by the absolute value of this
 *           coefficient) of the average rate coefficient of all other servers
 *           for the same service.  In other words, -5 instructs not to pick on
 *           the locally run server only if its rate is less than 5% of average
 *           status of the remaining servers for the same service.
 *
 *    Content type indication:
 *       C=type/subtype [no default]
 *           specification of Content-Type (including encoding), which server
 *           accepts.  The value of this flag gets added automatically to any
 *           HTTP packet sent to the server by SERVICE connector.  However, in
 *           order to communicate, a client still has to know and generate the
 *           data type accepted by the server, i.e. a protocol, which server
 *           understands.  This flag just helps ensure that all HTTP packets
 *           get proper content type, defined at service configuration.  This
 *           tag is not allowed in the DNS type server specifications.
 *
 *    (V)Host name:
 *       H=hostname     (optional)
 *           specifies the host name to use when accessing the server.  Can be
 *           implicitly set by server configuration if a hostname (instead of
 *           an IP) is provided as the connection point (the host:port pair).
 *           Value '' (two single quotes back-to-back) denotes no (v)host for
 *           cases when it might have been captured from the connection point.
 *
 *    Local server:
 *       L={yes|no}     [default is set by SERV_SetLocalServerDefault()]
 *           Local servers are accessible only by local clients (from within
 *           the Intranet) or direct clients of LBSMD, and are not accessible
 *           by the outside users (e.g. via network dispatching).
 *
 *    Private server:
 *       P=no           (default)
 *       P=yes
 *           specifies whether the server is private for the host.  Private
 *           server cannot be used from anywhere else but this host.  When
 *           non-private (default), the server lacks 'P=no' in verbal
 *           representation resulted from SERV_WriteInfo().  This tag is not
 *           allowed in the DNS type server specifications.
 *
 *    Reachability base rate:
 *       R=double       [0.0 = default]
 *           specifies availability rate for the server, expressed as a
 *           floating point number with 0.0 meaning the server is down
 *           (unavailable) and 1000.0 meaning the server is up and running.
 *           Intermediate or higher values can be used to make the server less
 *           or more favorable for choosing by LBSM Daemon, as this coefficient
 *           is directly used as a multiplier in the load-average calculation
 *           for the entire family of servers for the same service.  Normally,
 *           LBSMD keeps track of server reachability, and dynamically changes
 *           this rate to be the maximal specified when the server is up, and
 *           to be zero when the server is down.  Note that negative values are
 *           reserved for LBSMD private use.  To specify a server as inactive
 *           in LBSMD configuration file, one can use any negative number (note
 *           that value "0" in the config file means "default" and gets
 *           replaced with the value 1000.0).  Values less than 0.01 define
 *           standby server entries, which are used by the clients only if
 *           there are no working entries with a higher initial rate available.
 *           Standby entries are not governed by the host load but are used up
 *           according to the values of rates in descending order (for
 *           same-rate entries, an entry is taken at random).
 *
 *    Stateful server:
 *       S=no           (default)
 *       S=yes
 *           indicates whether a server is stateful, which only allows context
 *           bearing, dedicated socket (stateful) connections.  This tag is not
 *           allowed for HTTP* and DNS servers.
 *
 *    Secure connection:
 *       $=no           (default)
 *       $=yes
 *           indicates that connection to the server must be made over a secure
 *           protocol (HTTPS or SSL).
 *
 *    Validity period:
 *       T=integer      [0=default]
 *           specifies the time in seconds that this server entry is valid for
 *           without an update.  (If equal to 0 then defaulted to the LBSM
 *           Daemon to some reasonable value.)
 *
 *    Interzone server:
 *       X=no           (default)
 *       X=yes
 *           indicates that the server definition can span zones within the
 *           current site (by default, a server info is local to a zone).
 *
 *
 * Note that optional arguments can be omitted along with all preceding
 * optional arguments;  that is, the following 2 server specifications are both
 * valid:
 *
 * NCBID ''
 * and
 * NCBID
 *
 * but they are not equal to the following specification:
 *
 * NCBID A=R
 *
 * because here 'A=R' is treated as an argument, not as a tag.  To make the
 * latter specification equivalent to the former two (since 'A=R' is the
 * default tag), one has to use the following form:
 *
 * NCBID '' A=R
 */


/* Read full server info (including type) from string "str" (e.g. composed by
 * SERV_WriteInfo).  Result can be later freed by 'free()'.  If host is not
 * provided in the server specification, info->host is set to 0;  if port is
 * missing, a type-specific default value is used.
 */
extern NCBI_XCONNECT_EXPORT SSERV_Info* SERV_ReadInfo
(const char* info_str
 );


/* Make (a free()'able) copy of a server info.
 */
extern NCBI_XCONNECT_EXPORT SSERV_Info* SERV_CopyInfo
(const SSERV_Info* info
 );


/* Return an actual size (in bytes) the server info occupies (to be used for
 * copying info structures in whole).
 */
extern NCBI_XCONNECT_EXPORT size_t      SERV_SizeOfInfo
(const SSERV_Info* info
 );


/* Return non-zero('true') if two server infos are equal.
 */
extern NCBI_XCONNECT_EXPORT int/*bool*/ SERV_EqualInfo
(const SSERV_Info* info1,
 const SSERV_Info* info2
 );


#ifdef __cplusplus
}  /* extern "C" */
#endif


/* @} */

#endif /* CONNECT___NCBI_SERVER_INFO__H */
