#ifndef CONNECT___NCBI_SERVER_INFO__H
#define CONNECT___NCBI_SERVER_INFO__H

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
 *   Note that all server meta-addresses are allocated as
 *   single contiguous pieces of memory, which can be copied in whole
 *   with the use of 'SERV_SizeOfInfo' call. Dynamically allocated
 *   server infos can be freed with a direct call to 'free'.
 *   Assumptions on the fields: all fields in the server info come in
 *   host byte order except 'host', which comes in network byte order.
 *
 */

#include <connect/ncbi_connutil.h>


/** @addtogroup ServiceSupport
 *
 * @{
 */


#ifdef __cplusplus
extern "C" {
#endif


/* Server types
 */
typedef enum {
    fSERV_Ncbid           = 0x01,
    fSERV_Standalone      = 0x02,
    fSERV_HttpGet         = 0x04,
    fSERV_HttpPost        = 0x08,
    fSERV_Http            = fSERV_HttpGet | fSERV_HttpPost,
    fSERV_Firewall        = 0x10,
    fSERV_Dns             = 0x20
} ESERV_Type;


/* Flags to specify the algorithm for selecting the most preferred
 * server from the set of available servers
 */
typedef enum {
    fSERV_Regular = 0x0,
    fSERV_Blast   = 0x1
} ESERV_Flag;


#define SERV_DEFAULT_FLAG           fSERV_Regular
#define SERV_MIME_TYPE_UNDEFINED    ((EMIME_Type)(-1))
#define SERV_MIME_SUBTYPE_UNDEFINED ((EMIME_SubType)(-1))
                                 

/* Verbal representation of a server type (no internal spaces allowed)
 */
extern NCBI_XCONNECT_EXPORT const char* SERV_TypeStr
(ESERV_Type type
 );


/* Read server info type.
 * If successful, assign "type" and return pointer to the position
 * in the "str" immediately following the type tag.
 * On error, return NULL.
 */
extern NCBI_XCONNECT_EXPORT const char* SERV_ReadType
(const char* str,
 ESERV_Type* type
 );


/* Meta-addresses for various types of NCBI servers
 */
typedef struct {
    TNCBI_Size   args;
#define SERV_NCBID_ARGS(ui)     ((char*) (ui) + (ui)->args)
} SSERV_NcbidInfo;

typedef struct {
    char         dummy;         /* placeholder, not used                     */
} SSERV_StandaloneInfo;

typedef struct {
    TNCBI_Size   path;
    TNCBI_Size   args;
#define SERV_HTTP_PATH(ui)      ((char*) (ui) + (ui)->path)
#define SERV_HTTP_ARGS(ui)      ((char*) (ui) + (ui)->args)
} SSERV_HttpInfo;

typedef struct {
    ESERV_Type   type;          /* type of original server                   */
} SSERV_FirewallInfo;

typedef struct {
    char/*bool*/ name;          /* name presence flag                        */
    char         pad[7];        /* reserved for the future use, must be zero */
} SSERV_DnsInfo;


/* Generic NCBI server meta-address
 */
typedef union {
    SSERV_NcbidInfo      ncbid;
    SSERV_StandaloneInfo standalone;
    SSERV_HttpInfo       http;
    SSERV_FirewallInfo   firewall;
    SSERV_DnsInfo        dns;
} USERV_Info;

typedef struct {
    ESERV_Type            type; /* type of server                            */
    unsigned int          host; /* host the server running on, network b.o.  */
    unsigned short        port; /* port the server running on, host b.o.     */
    unsigned char/*bool*/ sful; /* true for stateful-only server (default=no)*/
    unsigned char/*bool*/ locl; /* true for local (LBSMD-only) server(def=no)*/
    TNCBI_Time            time; /* relaxation period / expiration time       */
    double                coef; /* bonus coefficient for server run locally  */
    double                rate; /* rate of the server                        */
    EMIME_Type          mime_t; /* type,                                     */
    EMIME_SubType       mime_s; /*     subtype,                              */
    EMIME_Encoding      mime_e; /*         and encoding for content-type     */
    ESERV_Flag            flag; /* algorithm flag for the server             */
    unsigned char reserved[14]; /* zeroed reserved area - do not use!        */
    unsigned short      quorum; /* quorum required to override this entry    */
    USERV_Info               u; /* server type-specific data/params          */
} SSERV_Info;


/* Constructors for the various types of NCBI server meta-addresses
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
(unsigned int   host            /* the only parameter                        */
 );


/* Dump server info to a string.
 * The server type goes first, and it is followed by a single space.
 * The returned string is '\0'-terminated, and must be deallocated by 'free()'.
 */
extern NCBI_XCONNECT_EXPORT char* SERV_WriteInfo
(const SSERV_Info* info
 );


/* Server specification consists of the following:
 * TYPE [host][:port] [server-specific_parameters] [tags]
 *
 * TYPE := { STANDALONE | NCBID | HTTP{|_GET|_POST} | FIREWALL | DNS }
 *
 * Host should be specified as either an IP address (in dotted notation),
 * or as a host name (using domain notation if necessary).
 * Port number must be preceded by a colon.
 * Both host and port get their default values if not specified.
 *
 * Server-specific parameters:
 *
 *    Standalone servers: None
 *                        Servers of this type do not take any arguments.
 *
 *    NCBID servers: Arguments to CGI in addition to specified by application.
 *                   Empty additional arguments denoted as '' (two single
 *                   quotes, back to back).  Note that the additional
 *                   arguments must not contain space characters.
 *
 *    HTTP* servers: Path (required) and args (optional) in the form
 *                   path[?args] (here brackets denote the optional part).
 *                   Note that no spaces are allowed within these parameters.
 *
 *    FIREWALL servers: Servers of this type are converted real servers of
 *                      the above types, when only accessible via FIREWALL
 *                      mode of NCBI dispatcher.  The purpose of this fake
 *                      server type is just to let the client know that
 *                      the service exists.  Additional parameter is optional
 *                      and if present, is the original type of the server
 *                      before conversion.  Note that servers of this type
 *                      cannot be configured in LBSMD.
 *
 *    DNS servers: Services for DNS and DB load-balancing, 
 *                 and dynamic ProxyPassing at the NCBI Web entry point.
 *                 Never exported to the outside world.
 *
 * Tags may follow in no particular order but no more than one instance
 * of each flag is allowed:
 *
 *    Load average calculation for the server:
 *       Regular        (default)
 *       Blast
 *
 *    Bonus coefficient:
 *       B=double       [0.0 = default]
 *           specifies a multiplicative bonus given to a server run locally,
 *           when calculating reachability rate.
 *           Special rules apply to negative/zero values:
 *           0.0 means not to use the described rate increase at all (default
 *           rate calculation is used, which only slightly increases rates
 *           of locally run servers).
 *           Negative value denotes that locally run server should
 *           be taken in first place, regardless of its rate, if that rate
 *           is larger than percent of expressed by the absolute value
 *           of this coefficient of the average rate coefficient of other
 *           servers for the same service. That is -5 instructs to
 *           ignore locally run server if its status is less than 5% of
 *           average status of remaining servers for the same service.
 *
 *    Content type indication:
 *       C=type/subtype [no default]
 *           specification of Content-Type (including encoding), which server
 *           accepts. The value of this flag gets added automatically to any
 *           HTTP packet sent to the server by SERVICE connector. However,
 *           in order to communicate, a client still has to know and generate
 *           the data type accepted by the server, i.e. a protocol, which
 *           server understands. This flag just helps insure that HTTP packets
 *           all get proper content type, defined at service configuration.
 *           This tag is not allowed in DNS server specifications.
 *
 *    Local server:
 *       L=no           (default for non-DNS specs)
 *       L=yes          (default for DNS specs)
 *           Local servers are accessible only by local clients (from within
 *           the Intranet) or direct clients of LBSMD, and are not accessible
 *           by the outside users (i.e. via network dispatching).
 *
 *    Private server:
 *       P=no           (default)
 *       P=yes
 *           specifies whether the server is private for the host.
 *           Private server cannot be used from anywhere else but
 *           this host. When non-private (default), the server lacks
 *           'P=no' in verbal representation resulted from SERV_WriteInfo().
 *           This tag is not allowed in DNS server specifications.
 *
 *    Quorum:
 *       Q=integer      [0 = default]
 *           specifies how many dynamic service entries have to be defined
 *           by respective hosts in order for this entry to be INACTIVE.
 *           Note that value 0 disables the quorum and the entry becomes
 *           effective immediately. The quorum flag is to create a backup
 *           configuration to be activated in case of multicast/daemon
 *           malfunction.
 *           Only static and non-FIREWALL server specs can have this tag.
 *
 *    Reachability base rate:
 *       R=double       [0.0 = default]
 *           specifies availability rate for the server, expressed as
 *           a floating point number with 0.0 meaning the server is down
 *           (unavailable) and 1000.0 meaning the server is up and running.
 *           Intermediate or higher values can be used to make the server less
 *           or more favorable for choosing by LBSM Daemon, as this coefficient
 *           is directly used as a multiplier in the load-average calculation
 *           for the entire family of servers for the same service.
 *           (If equal to 0.0 then defaulted by the LBSM Daemon to 1000.0.)
 *           Normally, LBSMD keeps track of server reachability, and
 *           dynamically switches this rate to be maximal specified when
 *           the server is up, and to be zero when the server is down.
 *           Note that negative values are reserved for LBSMD private use.
 *           To specify that a server as inactive in LBSMD configuration file,
 *           one can use any negative number (note that value "0" in the config
 *           file means "default" and gets replaced with the value 1000.0).
 *
 *    Stateful server:
 *       S=no           (default)
 *       S=yes
 *           Indication of stateful server, which allows only dedicated socket
 *           (stateful) connections.
 *           This tag is not allowed for HTTP* and DNS servers.
 *
 *    Validity period:
 *       T=integer      [0 = default]
 *           specifies the time in seconds this server entry is valid
 *           without update. (If equal to 0 then defaulted by
 *           the LBSM Daemon to some reasonable value.)
 *
 *
 * Note that optional arguments can be omitted along with all preceding
 * optional arguments, that is the following 2 server specifications are
 * both valid:
 *
 * NCBID ''
 * and
 * NCBID
 *
 * but they are not equal to the following specification:
 *
 * NCBID Regular
 *
 * because here 'Regular' is treated as an argument, not as a tag.
 * To make the latter specification equivalent to the former two, one has
 * to use the following form:
 *
 * NCBID '' Regular
 */


/* Read full server info (including type) from string "str"
 * (e.g. composed by SERV_WriteInfo). Result can be later freed by 'free()'.
 * If host is not found in the server specification, info->host is
 * set to 0; if port is not found, type-specific default value is used.
 */
extern NCBI_XCONNECT_EXPORT SSERV_Info* SERV_ReadInfo
(const char* info_str
 );


/* Make (a free()'able) copy of a server info.
 */
extern NCBI_XCONNECT_EXPORT SSERV_Info* SERV_CopyInfo
(const SSERV_Info* info
 );


/* Return an actual size (in bytes) the server info occupies
 * (to be used for copying info structures in whole).
 */
extern NCBI_XCONNECT_EXPORT size_t SERV_SizeOfInfo
(const SSERV_Info* info
 );


/* Return 'true' if two server infos are equal.
 */
extern NCBI_XCONNECT_EXPORT int/*bool*/ SERV_EqualInfo
(const SSERV_Info* info1,
 const SSERV_Info* info2
 );


#ifdef __cplusplus
}  /* extern "C" */
#endif


/* @} */


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.40  2006/03/04 17:02:14  lavr
 * NCBI_TIME_INFINITE moved to ncbi_types.h
 *
 * Revision 6.39  2005/12/23 18:06:53  lavr
 * fSERV_StatelessOnly -> fSERV_Stateless
 * fSERV_Any, fSERV_Stateless -> <connect/ncbi_service.h>
 * +SERV_TIME_INFINITE
 *
 * Revision 6.38  2005/12/14 21:20:59  lavr
 * +SERV_CopyInfo(); DNS type to have name presence flag
 *
 * Revision 6.37  2003/04/09 19:05:48  siyan
 * Added doxygen support
 *
 * Revision 6.36  2003/01/08 01:59:33  lavr
 * DLL-ize CONNECT library for MSVC (add NCBI_XCONNECT_EXPORT)
 *
 * Revision 6.35  2002/11/01 20:10:40  lavr
 * Note that FIREWALL server specs cannot have Q flag set
 *
 * Revision 6.34  2002/09/19 18:04:48  lavr
 * Header file guard macro changed
 *
 * Revision 6.34  2002/09/19 18:01:24  lavr
 * Header file guard macro changed; log moved to the end
 *
 * Revision 6.33  2002/09/17 15:39:07  lavr
 * SSERV_Info::quorum moved past the reserved area
 *
 * Revision 6.32  2002/09/16 14:59:04  lavr
 * Rename SSERV_DnsInfo to follow naming convention for other Info's
 *
 * Revision 6.31  2002/09/04 15:09:17  lavr
 * Add quorum field to SSERV_Info::, log moved to end
 *
 * Revision 6.31  2002/08/16 17:52:27  lavr
 * PUBSYNC, PUBSEEK* macros; SEEKOFF obsoleted; some formatting done
 *
 * Revision 6.30  2002/03/19 22:10:59  lavr
 * Better description for flags 'P' and 'C'
 *
 * Revision 6.29  2002/03/11 21:51:18  lavr
 * Modified server info to include MIME encoding and to prepare for
 * BLAST dispatching to be phased out. Also, new DNS server type defined.
 *
 * Revision 6.28  2001/09/25 14:47:49  lavr
 * TSERV_Flags reverted to 'int'
 *
 * Revision 6.27  2001/09/24 20:22:31  lavr
 * TSERV_Flags changed from 'int' to 'unsigned int'
 *
 * Revision 6.26  2001/09/19 15:57:27  lavr
 * Server descriptor flag "L=" documented more precisely
 *
 * Revision 6.25  2001/09/10 21:16:27  lavr
 * FIREWALL server type added, documented
 *
 * Revision 6.24  2001/06/19 19:09:35  lavr
 * Type change: size_t -> TNCBI_Size; time_t -> TNCBI_Time
 *
 * Revision 6.23  2001/06/12 20:45:23  lavr
 * Less ambiguous comment for SSERV_Info::time
 *
 * Revision 6.22  2001/06/05 14:10:20  lavr
 * SERV_MIME_UNDEFINED split into 2 (typed) constants:
 * SERV_MIME_TYPE_UNDEFINED and SERV_MIME_SUBTYPE_UNDEFINED
 *
 * Revision 6.21  2001/06/04 16:59:56  lavr
 * MIME type/subtype added to server descriptor
 *
 * Revision 6.20  2001/05/17 14:49:46  lavr
 * Typos corrected
 *
 * Revision 6.19  2001/05/03 16:35:34  lavr
 * Local bonus coefficient modified: meaning of negative value changed
 *
 * Revision 6.18  2001/04/24 21:24:05  lavr
 * New server attributes added: locality and bonus coefficient
 *
 * Revision 6.17  2001/03/06 23:52:57  lavr
 * SERV_ReadInfo can now consume either hostname or IP address
 *
 * Revision 6.16  2001/03/05 23:09:20  lavr
 * SERV_WriteInfo & SERV_ReadInfo both take only one argument now
 *
 * Revision 6.15  2001/03/02 20:06:26  lavr
 * Typos fixed
 *
 * Revision 6.14  2001/03/01 18:47:45  lavr
 * Verbal representation of server info is wordly documented
 *
 * Revision 6.13  2000/12/29 17:39:42  lavr
 * Pretty printed
 *
 * Revision 6.12  2000/12/06 22:17:02  lavr
 * Binary host addresses are now explicitly stated to be in network byte
 * order, whereas binary port addresses now use native (host) representation
 *
 * Revision 6.11  2000/10/20 17:05:48  lavr
 * TSERV_Type made 'unsigned int'
 *
 * Revision 6.10  2000/10/05 21:25:45  lavr
 * ncbiconf.h removed
 *
 * Revision 6.9  2000/10/05 21:10:11  lavr
 * ncbiconf.h included
 *
 * Revision 6.8  2000/05/31 23:12:14  lavr
 * First try to assemble things together to get working service mapper
 *
 * Revision 6.7  2000/05/23 19:02:47  lavr
 * Server-info now includes rate; verbal representation changed
 *
 * Revision 6.6  2000/05/22 16:53:07  lavr
 * Rename service_info -> server_info everywhere (including
 * file names) as the latter name is more relevant
 *
 * Revision 6.5  2000/05/15 19:06:05  lavr
 * Use home-made ANSI extensions (NCBI_***)
 *
 * Revision 6.4  2000/05/12 21:42:11  lavr
 * SSERV_Info::  use ESERV_Type, ESERV_Flags instead of TSERV_Type, TSERV_Flags
 *
 * Revision 6.3  2000/05/12 18:31:48  lavr
 * First working revision
 *
 * Revision 6.2  2000/05/09 15:31:29  lavr
 * Minor changes
 *
 * Revision 6.1  2000/05/05 20:24:00  lavr
 * Initial revision
 *
 * ==========================================================================
 */

#endif /* CONNECT___NCBI_SERVER_INFO__H */
