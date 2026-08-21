#ifndef CONNECT___NCBI_SERVICE__H
#define CONNECT___NCBI_SERVICE__H

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
 * @file ncbi_service.h
 *   Top-level API to resolve NCBI service names into server meta-addresses.
 *
 */

#include <connect/ncbi_server_info.h>
#include <connect/ncbi_host_info.h>


/** @addtogroup ServiceSupport
 *
 * @{
 */

/** Revision 7.0 */
#define SERV_CLIENT_REVISION_MAJOR  7
#define SERV_CLIENT_REVISION_MINOR  0

/** Special values for the "preferred_host" parameter.
 * @sa
 *  SERV_OpenEx, SERV_GetInfoEx
 */
#define SERV_LOCALHOST  ((unsigned int)(~0UL))
#define SERV_ANYHOST    0


#ifdef __cplusplus
extern "C" {
#endif


/* Fwdecl of an opaque type */
struct SSERV_IterTag;
/** Iterator over resolved servers */
typedef struct SSERV_IterTag* SERV_ITER;


/** Simplified (uncluttered) SSERV_Info pointer type */
typedef const SSERV_Info* SSERV_InfoCPtr;


/** Special "type" bit values that may be combined with server types.
 * @note MSW should be maintained compatible with EMGHBN_Option.
 * @sa
 *  ESERV_Type, ESERV_OpenEx, SERV_GetInfoEx
 */
enum ESERV_TypeSpecial {
    fSERV_Any               = 0,           /**< Any type but fSERV_Dns       */
    fSERV_All               = 0x00007FFF,  /**< Server type mask, really any */
    fSERV_Stateless         = 0x00008000,  /**< Stateless servers only       */
    fSERV_Reserved          = 0x00100000,  /**< Reserved, MBZ                */
    fSERV_DelayOpen         = 0x00400000,  /**< Don't open service until use */
    fSERV_ReverseDns        = 0x00800000,  /**< Reverse convert to DNS-type  */
    /* The following allow to get otherwise excluded server-infos            */
    fSERV_IncludeDown       = 0x08000000,
    fSERV_IncludeStandby    = 0x10000000,
    fSERV_IncludeReserved   = 0x20000000,  /**< @note Not yet implemented    */
    fSERV_IncludeSuppressed = 0x40000000,
    fSERV_IncludeInactive   = 0x70000000,
    fSERV_IncludePrivate    = 0x80000000,
    fSERV_Promiscuous       = 0xF8000000   /**< Evrthng and the kitchen sink */
};
typedef unsigned int   TSERV_Type;      /**<Bitwise OR of ESERV_Type[Special]*/
typedef unsigned short TSERV_TypeOnly;  /**<Server type only, w/o specials   */


/** Create an iterator for sequential server lookup.
 *
 * @param service
 *  A service name; may not be NULL or empty.
 *
 * @note A valid service name consists of a sequence of special identifiers
 *       separated by single slashes.  An identifier may contain only
 *       alphanumeric characters (including underscores) and embedded dashes
 *       (minus signs); a dash may not be adjacent to another dash or an
 *       underscore.  The first identifier must start with a letter or an
 *       underscore and must contain at least one letter.  Subsequent
 *       identifiers, if any, are not required to contain a letter.
 *
 *       A service name consisting of two or more identifiers is called a
 *       compound (or cataloged) name and can be processed only by the LOCAL,
 *       LINKERD, and NAMERD service mappers.
 *
 *       A single-identifier service name may be followed by a dot and a
 *       DNS-like domain part (in dotted notation).  The domain part is
 *       insignificant for service-related parameter lookups in either the
 *       registry or the environment.  The single (leading) identifier in such
 *       a name may neither start nor end with an underscore, nor contain
 *       consecutive underscores.  No slashes are allowed in such service
 *       names.
 *
 *       Compound service names may not have domain parts.
 *
 * @note A domain-suffixed service name may _also_ include a prefix consisting
 *       of dash-separated words and terminated by one of the recognized
 *       marker words:
 *
 *           "-legacy-", "-solr-", "-mssql-", "-mongodb-", or "-postgres-".
 *
 *       Any such marker triggers the same special treatment: the entire
 *       prefix, including the marker itself, is stripped.  Without a
 *       recognized marker, the entire label preceding the domain part
 *       constitutes the service name and no prefix stripping is performed.
 *       The domain part itself remains insignificant for service-related
 *       parameter lookups in the registry or the environment, as described
 *       above.
 *
 *       For all _legacy_ resolvers, both the prefix (including its marker) and
 *       the domain suffix are stripped from the service name.  For the new
 *       LBNULL resolver designed for the P2 environment, however, both parts
 *       remain significant.
 *
 *       The name remaining after stripping constitutes the legacy service
 *       name.  For lookups performed by legacy mappers, any dashes in that
 *       name are converted to underscores, unless the name is redirected
 *       (see below), case-insensitively, to the same name with some (or all)
 *       of the underscores replaced back with dashes.  In that case, the name
 *       is used "as is", with whatever dashes it contains.  The case is also
 *       preserved for such same-name redirects.
 *
 * @note A service name may be prefixed with a server type, analogous to a URL
 *       scheme.  The scheme consists of a recognized server type followed by
 *       "+ncbilb://".  The entire scheme is case-insensitive.  Additionally,
 *       "tcp+ncbilb://" is equivalent to "standalone+ncbilb://".
 *
 *       The specified server type restricts the search to servers of that
 *       particular type.  If this type conflicts with the in-code type
 *       selection established by the "types" argument, the search fails.
 *
 * @note Examples of valid service names:
 *
 *       "echo"
 *           A plain legacy service name, "echo".
 *
 *       "sutils201.be-md"
 *           A domain-enabled legacy service name, "sutils201"; the domain part
 *           is insignificant for parameter lookups.
 *
 *       "a-simple-dashed-service"
 *           A literal service name, "a-simple-dashed-service".
 *
 *       "a-simple-dashed-service.domain"
 *           The same service name, "a-simple-dashed-service"; the domain part
 *           is insignificant for parameter lookups but is used by LBNULL for
 *           routing.
 *
 *       "cxxtk-tech-legacy-nc-test"
 *           Another dashed service name, "cxxtk-tech-legacy-nc-test"; because
 *           it has no domain part, "-legacy-" is not recognized as a marker
 *           and no name stripping is performed.
 *
 *       "cxxtk-tech-legacy-nc-test.bethesda-dev.consul.be-md"
 *           A "decorated" P2 service name corresponding to the legacy service
 *           name "nc_test".
 *
 *       "http+ncbilb://cxxtk-legacy-cxx-monitor-fcgi.bethesda-prod.consul"
 *           The legacy service "cxx_monitor_fcgi", suitable for P2 and pinned
 *           to HTTP servers only.
 *
 *       "tcp+ncbilb://taxservice"
 *           The legacy service "taxservice", restricted to STANDALONE servers.
 *
 *       "PmQuerySrv/pubmed"
 *           A compound service name (used mainly in P1).
 *
 * @note As an extension to the valid service names described above, the API
 *       also accepts URL-like strings in place of service names.  Based on
 *       their syntax, such strings can be interpreted as STANDALONE, HTTP, or
 *       DNS endpoints, depending on the server types allowed by the call.
 *       This interpretation is attempted only when the input does not appear
 *       to be a valid service name.
 *
 *       To disable this fallback behavior, use the global Boolean setting
 *       [CONN]SERVICE_ENDPOINT_FALLBACK_DISABLE in the registry or, with
 *       precedence, CONN_SERVICE_ENDPOINT_FALLBACK_DISABLE in the environment.
 *
 * @note When a URL-like service name is parsed through the fallback mechanism
 *       described above, the global setting CONN_IMPLICIT_SERVER_TYPE=type
 *       can be used to hint how an incomplete URL is to be interpreted.  For
 *       example, "//hostname/" is interpreted as a DNS-type endpoint when
 *       CONN_IMPLICIT_SERVER_TYPE is set to "dns" and the "types" parameter
 *       permits all server types (fSERV_All).
 *
 *       If a service name is also known, such as when service name redirection
 *       (see below) produces a URL-like target, a service-specific setting is
 *       consulted first:
 *
 *           [service]
 *           CONN_IMPLICIT_SERVER_TYPE=type
 *
 *       or the equivalent, higher-precedence environment setting
 *
 *           service_CONN_IMPLICIT_SERVER_TYPE=type
 *
 *       If no applicable service-specific setting is found, the global,
 *       non-service-specific CONN_IMPLICIT_SERVER_TYPE setting is used.
 *
 *       CONN_IMPLICIT_SERVER_TYPE is only a hint and is ignored if it
 *       conflicts with the server types allowed by the "types" parameter.
 *       Unlike a server type specified in the service-name scheme (see above)
 *       or CONN_SERVER_TYPE (see below), it never overrides the allowed type
 *       selection and cannot, by itself, cause the search to fail.
 *
 * @note Finally, service names can be redirected through the registry or the
 *       environment.  A registry entry
 *
 *           CONN_SERVICE_NAME=newvalue
 *
 *       in the "[service]" section, or the equivalent environment setting
 *
 *           service_CONN_SERVICE_NAME=newvalue
 *
 *       (which takes precedence), redirects the input service name to
 *       "newvalue".  In the environment variable name, all non-alphanumeric
 *       characters in "service" must be replaced with underscores.
 *
 *       Redirection is recursive and stops when any of the following occurs:
 *
 *       1. no further redirection is found; or
 *
 *       2. the redirection target "newvalue" is the same as the source name,
 *          case-insensitively and, for a domain-enabled source name,
 *          disregarding dash/underscore differences and the marker-terminated
 *          prefix and domain parts.  In this case, "newvalue" is used in its
 *          exact form, "as is" (case and dash conversions are disabled); or
 *
 *       3. a redirection produces a "newvalue" that is not a valid service
 *          name.  In this case, the service name itself does not change.
 *          If "newvalue" is a recognized URL-like string, that value is used
 *          to construct ("cook") an endpoint rather than resolve the service
 *          through service mappers (see the note above).  If not, the service
 *          search fails immediately; or
 *
 *       4. the redirection depth reaches the limit of 10, preventing possible
 *          infinite redirection loops.  In this case, the service search
 *          fails immediately.
 *
 *       Any server type "scheme" encountered in an intermediate redirection
 *       target is ignored; only the scheme, if any, of the final "newvalue"
 *       is retained.
 *
 *       Examples (using the environment, for simplicity):
 *
 *       id2_CONN_SERVICE_NAME="id2_internal"
 *           Redirects service "ID2" to "ID2_INTERNAL".
 *
 *       bounce_CONN_SERVICE_NAME="ncbid+ncbilb://bounce"
 *           Redirects service "BOUNCE" to itself but restricts the server type
 *           to NCBID.  As a side effect, the case of "bounce" is also
 *           preserved by any mappers that support that.
 *
 *       echo_CONN_SERVICE_NAME="host:port"
 *           Resolves service "ECHO" to "host" and "port" (provided that the
 *           lookup allows the STANDALONE and/or DNS server types).
 *
 *       svc_CONN_SERVICE_NAME="ncbid+ncbilb://cxxtk-legacy-svc2.be-md"
 *       svc2_CONN_SERVICE_NAME="http+ncbilb://svc3"
 *       svc3_CONN_SERVER_TYPE="tcp"
 *           Redirects service "svc" via "svc2" to "svc3" and restricts the
 *           server type to STANDALONE (synonymous with "tcp").  Note that all
 *           intermediate server-type schemes and marker-based decorations
 *           (such as those for "svc2") are ignored.  Without the final
 *           CONN_SERVER_TYPE setting, the resulting server type would be HTTP,
 *           as specified alongside the terminal service name "svc3".
 *
 *       svc_CONN_SERVICE_NAME="demo-legacy-svc-dashed.st-va"
 *       svc_dashed_CONN_SERVICE_NAME="svc-simple.be-md"
 *           Redirects "svc" via the decorated target
 *           "demo-legacy-svc-dashed.st-va", which reduces to the legacy
 *           service name "svc-dashed", and then to the literal service name
 *           "svc-simple".  The first redirection target is stripped of its
 *           marker-terminated prefix and domain, while the second is a regular
 *           dashed service name and remains "svc-simple".  The domain parts
 *           are not part of either service name.
 *
 *           If the target of svc_dashed_CONN_SERVICE_NAME were instead
 *           "svc-dashed.be-md", it would redirect "svc" to "svc-dashed", with
 *           that name used by all legacy mappers without case or
 *           dash-to-underscore conversions.
 *
 *           The same redirection applies to an equivalent marker-decorated
 *           input, such as "demo-mssql-svc.be-md", which is first stripped
 *           to "svc".
 *
 *       PmQuerySrv_CONN_SERVICE_NAME="PmQuerySrv"
 *       or
 *       PMQUERYSRV_CONN_SERVICE_NAME="PmQuerySrv"
 *           Both ensure that "PmQuerySrv" is used "as is" (no uppercasing).
 *
 *       Name redirection is performed internally by all service-aware APIs
 *       (e.g. ConnNetInfo_Create() and ConnNetInfo_GetValue()) and allows
 *       service-related parameters to be obtained from a parameter section
 *       other than that associated with the original service name.  If no
 *       previous service name is available, service-related parameters
 *       default to the global [CONN] section (or the global,
 *       non-service-specific CONN environment).  The global settings are also
 *       used when redirection terminates with an error, as in cases 3 (for a
 *       non-URL target) and 4 above, rather than settings associated with the
 *       last successfully processed service name in the redirection chain.
 *
 *       See <connect/ncbi_connutil.h>.
 *
 * @param types
 *  A bitset specifying the allowed server type(s) and additional optional
 *  lookup flags.
 *
 * @note A server type, if any, used as a prefix to the service name
 *       (see above) restricts the default selection implied by this "types"
 *       parameter, but must be compatible with it; otherwise, the search
 *       fails.  The server type can be further overridden at run time by the
 *       registry entry
 *
 *           [service]
 *           CONN_SERVER_TYPE=type
 *
 *       or the equivalent, higher-precedence environment setting
 *
 *           service_CONN_SERVER_TYPE=type
 *
 *       where "service" is the terminal service name obtained after all
 *       service name redirections, if any.  An unrecognized or invalid "type"
 *       is ignored (and an error is logged).  A valid "type" takes final
 *       precedence, but must likewise be compatible with this "types"
 *       parameter; otherwise, the search fails.
 * 
 * @param preferred_host
 *  Preferred host on which to use the service, in network byte order.
 *
 * @param net_info
 *  Connection information used to contact network-based service name mappers.
 *  A NULL value prevents dispatch through LINKERD, NAMERD, and DISPD.
 *
 * @note Service mappers are tried in the sequence listed below, and the search
 *       stops as soon as any mapper yields a result:
 *
 *          LOCAL, LBNULL, LBSMD, LBDNS, LINKERD, NAMERD, and DISPD.
 *
 *       LOCAL, LBNULL, LBDNS, LINKERD, and NAMERD participate only when
 *       enabled, whereas LBSMD and DISPD participate unless disabled (see
 *       below).  If "net_info" is NULL, only LOCAL, LBNULL, LBSMD, and LBDNS
 *       are eligible.  Otherwise, LINKERD, NAMERD, and DISPD may also be used,
 *       with the supplied connection information.
 *
 * @note If "net_info" is not NULL, a non-zero value of "net_info->stateless"
 *       implicitly adds the fSERV_Stateless bit to "types".
 *
 * @param skip
 *  An array of servers not to select.  It contains server-info elements whose
 *  matching server-infos must _not_ be returned by the search.
 *
 * @note Special additional rules apply to elements of the "skip" array when
 *       the fSERV_ReverseDns bit is set in the "types" parameter.  A potential
 *       result is excluded if either of the following applies:
 *
 *       1. an fSERV_Dns entry in the "skip" array matches its host[:port]
 *          (any port matches if the skip entry's port is 0); or
 *
 *       2. reverse lookup of its host:port yields an fSERV_Dns-type server
 *          whose name matches that of an fSERV_Dns-type server in "skip".
 *
 * @param n_skip
 *  Number of entries in the "skip" array.
 *
 * @note The following keys in the [CONN] registry section:
 *
 *          LOCAL_ENABLE, LBNULL_ENABLE, LBSMD_DISABLE, LBDNS_ENABLE,
 *          LINKERD_ENABLE, NAMERD_ENABLE, and DISPD_DISABLE
 *
 *       can be overridden by the corresponding environment variables:
 *
 *          CONN_LOCAL_ENABLE, CONN_LBNULL_ENABLE, CONN_LBSMD_DISABLE,
 *          CONN_LBDNS_ENABLE, CONN_LINKERD_ENABLE, CONN_NAMERD_ENABLE,
 *          and CONN_DISPD_DISABLE.
 *
 *       These settings can be used to enable LOCAL, LBNULL, LBDNS, LINKERD,
 *       and NAMERD, or to disable LBSMD and DISPD.  This mechanism permits any
 *       combination of the service mappers (LOCAL/LBNULL/LBSMD/LBDNS/LINKERD/
 *       NAMERD/DISPD), whether network-aware or not.
 *
 *       The corresponding identifiers can also be used as keys in the
 *       "[service]" registry section for a more granular, per-service effect,
 *       as described in <connect/ncbi_connutil.h>, or can be prefixed with the
 *       service name in the process environment to affect only that particular
 *       service.
 *
 * @return
 *  A non-NULL iterator, or NULL if the service does not exist.
 *
 * @note A non-NULL iterator acknowledges the existence of the service but does
 *       not guarantee that any usable server endpoints are available.  Thus,
 *       retrieving server-info entries from the iterator may yield none.
 * @sa
 *  SERV_GetNextInfoEx, SERV_Reset, SERV_Close, ConnNetInfo_Create
 */
extern NCBI_XCONNECT_EXPORT SERV_ITER SERV_OpenEx
(const char*          service,
 TSERV_Type           types,
 unsigned int         preferred_host,
 const SConnNetInfo*  net_info,
 SSERV_InfoCPtr       skip[], 
 size_t               n_skip
 );

/** Same as SERV_OpenEx(., ., ., ., 0, 0), i.e. without a "skip" array.
 * @sa
 *  SERV_OpenEx
 */
extern NCBI_XCONNECT_EXPORT SERV_ITER SERV_Open
(const char*          service,
 TSERV_Type           types,
 unsigned int         preferred_host,
 const SConnNetInfo*  net_info
);


/** Allocate an iterator for the specified service using default lookup
 * parameters obtained from the registry and the environment.  This is
 * equivalent to using connection information created implicitly by
 * ConnNetInfo_Create(service), with no preferred host, no exclusions, and
 * with fSERV_Any assumed for the server types.
 *
 * @note fSERV_Any permits any server type except fSERV_Dns; unlike fSERV_All,
 *       it does not explicitly enable all server-type bits.
 *
 * Service mappers are tried according to their normal configuration and
 * precedence, and the search stops as soon as any mapper yields a result.
 *
 * @param service
 *  A service name; may not be NULL or empty.
 *
 * @return
 *  A non-NULL iterator, or NULL if the service does not exist.
 *
 * @note A non-NULL iterator acknowledges the existence of the service but does
 *       not guarantee that any usable server endpoints are available.  Thus,
 *       retrieving server-info entries from the iterator may yield none.
 * @sa
 *  SERV_GetNextInfoEx, SERV_OpenEx, SERV_Reset, SERV_Close, ConnNetInfo_Create
 */
extern NCBI_XCONNECT_EXPORT SERV_ITER SERV_OpenSimple
(const char*          service
 );


/** Get the next server meta-address (server-info), optionally accompanied by
 * host parameters made available by the LB daemon (LBSMD).
 *
 * @param iter
 *  An iterator handle obtained from a SERV_Open*() call.
 *
 * @note NULL is accepted for "iter" and causes NULL to be returned.
 *
 * @param host_info
 *  An optional pointer to receive host information; may be NULL.
 *
 * @return
 *  NULL if no more servers are available for the requested service; in this
 *  case, "*host_info", if provided, is left unchanged.  Otherwise, returns a
 *  non-NULL pointer to the next server meta-address.
 *
 * @note The returned server-info remains valid only until either of the
 *       following occurs:
 *
 *       1. SERV_GetNextInfo[Ex]() is called again with the same iterator; or
 *
 *       2. the iterator is reset or closed by SERV_Reset() or SERV_Close().
 *
 * @note The application must NOT destroy the returned server-info; it is
 *       managed automatically by the iterator.
 *
 * @note A special DNS-type server-info may be returned only as the first
 *       result after the iterator is opened or reset.  Such an entry has 0 in
 *       its host field, indicating that the service name exists but the
 *       service is currently unavailable (not serving).
 *
 *       This special entry is returned only when no usable service endpoints
 *       are available and is therefore the only server-info produced for that
 *       iteration sequence.  It cannot appear after any regular server-info
 *       has already been returned.
 *
 * @note Only on successful completion, i.e. when a non-NULL server-info is
 *       returned, can this call also provide host information.  If "host_info"
 *       is non-NULL and host information is available, a copy is allocated and
 *       its handle is stored in "*host_info".  If no host information is
 *       available, "*host_info" is set to NULL.
 *
 *       A non-NULL host-information handle can be used to retrieve parameters
 *       such as load, environment, number of CPUs, and others; see
 *       <connect/ncbi_host_info.h>.  The returned handle must be explicitly
 *       free()'d when no longer needed.
 * @sa
 *  SERV_Reset, SERV_Close, SERV_GetInfoEx, <connect/ncbi_host_info.h>
 */
extern NCBI_XCONNECT_EXPORT SSERV_InfoCPtr SERV_GetNextInfoEx
(SERV_ITER            iter,
 HOST_INFO*           host_info
 );

/** Same as SERV_GetNextInfoEx(., 0), i.e. without host information.
 * @sa
 *  SERV_GetNextInfoEx
 */
extern NCBI_XCONNECT_EXPORT SSERV_InfoCPtr SERV_GetNextInfo
(SERV_ITER            iter
 );


/** A "fast-track" routine for obtaining a single server-info entry without
 * explicitly creating and iterating over a service iterator.  It is
 * functionally equivalent to opening an iterator with SERV_OpenEx(), obtaining
 * the first result with SERV_GetNextInfoEx(), and then closing the iterator,
 * but is optimized for callers that need only the first matching server.
 *
 * Both the returned server-info and any returned host information are owned by
 * the caller and must be explicitly free()'d when no longer needed.
 *
 * @param service
 *  A service name; may not be NULL or empty.
 *
 * @param types
 *  A bitset specifying the allowed server type(s) and additional optional
 *  lookup flags.
 *
 * @param preferred_host
 *  Preferred host on which to use the service, in network byte order.
 *
 * @param net_info
 *  Connection information used to contact network-based service name mappers.
 *  A NULL value prevents dispatch through LINKERD, NAMERD, and DISPD.
 *
 * @param skip
 *  An array of servers not to select; see SERV_OpenEx() for details.
 *
 * @param n_skip
 *  Number of entries in the "skip" array.
 *
 * @param host_info
 *  An optional pointer to receive host information; may be NULL.
 *
 * @note Host information can be returned only when this call itself returns a
 *       non-NULL server-info; see SERV_GetNextInfoEx() for details.
 *
 * @return
 *  A non-NULL pointer to the first matching server-info, or NULL if no matching
 *  server instance is found.
 * @sa
 *  SERV_GetInfo, SERV_OpenEx, SERV_GetNextInfoEx
 */
extern NCBI_XCONNECT_EXPORT SSERV_Info* SERV_GetInfoEx
(const char*          service,
 TSERV_Type           types,
 unsigned int         preferred_host,
 const SConnNetInfo*  net_info,
 SSERV_InfoCPtr       skip[],
 size_t               n_skip,
 HOST_INFO*           host_info
 );

/** Same as SERV_GetInfoEx(., ., ., ., 0, 0, 0), i.e. without a "skip" array
 * or host information.
 * @sa
 *  SERV_GetInfoEx, SERV_Open
 */
extern NCBI_XCONNECT_EXPORT SSERV_Info* SERV_GetInfo
(const char*          service,
 TSERV_Type           types,
 unsigned int         preferred_host,
 const SConnNetInfo*  net_info
 );

/** Equivalent to
 *
 *      SERV_GetInfo(., fSERV_Any, SERV_ANYHOST, ConnNetInfo_Create(service))
 *
 * except that this call creates and releases the SConnNetInfo structure
 * internally.
 * @sa
 *  SERV_GetInfo, SERV_OpenSimple
 */
extern NCBI_XCONNECT_EXPORT SSERV_Info* SERV_GetInfoSimple
(const char*          service
 );


/** Penalize the server most recently returned by SERV_GetNextInfo[Ex]().
 *
 * @param iter
 *  An iterator handle obtained from a SERV_Open*() call.
 *
 * @param fine
 *  A penalty value in the inclusive range [0=min..100=max] (%%).
 *
 * @return
 *  Non-zero on success, or 0 on failure.
 * @sa
 *  SERV_OpenEx, SERV_GetNextInfoEx
 */
extern NCBI_XCONNECT_EXPORT int/*bool*/ SERV_Penalize
(SERV_ITER            iter,
 double               fine
 );


/** Same as SERV_Penalize(), with an additional penalty hold time.
 *
 * @param iter
 *  An iterator handle obtained from a SERV_Open*() call.
 *
 * @param fine
 *  A penalty value in the inclusive range [0=min..100=max] (%%).
 *
 * @param time
 *  Time, in seconds, for which to retain the penalty.
 *
 * @return
 *  Non-zero on success, or 0 on failure.
 * @sa
 *  SERV_Penalize, SERV_OpenEx, SERV_GetNextInfoEx
 */
extern NCBI_XCONNECT_EXPORT int/*bool*/ SERV_PenalizeEx
(SERV_ITER            iter,
 double               fine,
 TNCBI_Time           time
 );


/** Rerate the server most recently returned by SERV_GetNextInfo[Ex]().
 *
 * @note This is an experimental API.
 *
 * @param iter
 *  An iterator handle obtained from a SERV_Open*() call.
 *
 * @param rate
 *  A new rate value; 0.0 turns the server off, while
 *  fabs(rate) >= LBSM_RERATE_DEFAULT restores the default rate.
 *
 * @return
 *  Non-zero on success, or 0 on failure.
 * @sa
 *  SERV_OpenEx, SERV_GetNextInfoEx
 */
extern NCBI_XCONNECT_EXPORT int/*bool*/ SERV_Rerate
(SERV_ITER            iter,
 double               rate
 );


/** Reset the iterator to its post-open state.
 *
 * @warning Invalidates all previously returned server-info descriptors
 *          (SSERV_Info*).
 *
 * @param iter
 *  An iterator handle obtained from a SERV_Open*() call.
 *
 * @note NULL is accepted for "iter" and causes no action.
 * @sa
 *  SERV_OpenEx, SERV_GetNextInfoEx, SERV_Close
 */
extern NCBI_XCONNECT_EXPORT void SERV_Reset
(SERV_ITER            iter
 );


/** Close and deallocate the iterator, completing the lookup process.  The call
 * must be made when the iterator is no longer needed to release its associated
 * resources.
 *
 * @warning Invalidates all previously returned server-info descriptors
 *          (SSERV_Info*).
 *
 * @param iter
 *  An iterator handle obtained from a SERV_Open*() call.
 *
 * @note NULL is accepted for "iter" and causes no action.
 * @sa
 *  SERV_OpenEx, SERV_Reset
 */
extern NCBI_XCONNECT_EXPORT void SERV_Close
(SERV_ITER            iter
 );


/** Obtain the port number for a named standalone service at the specified host,
 * as declared by the LB configuration.
 *
 * @param name
 *  Name of the fSERV_Standalone service to look up.
 *
 * @param host
 *  Host address, in network byte order, at which to look up the service.
 *  SERV_LOCALHOST and 0 are equivalent.
 *
 * @return
 *  The port number, or 0 on error or if no suitable service is found.
 *
 * @note The call returns the first match and does not check whether an
 *       application is actually running at the returned port; thus, the
 *       service need not currently be up.
 * @sa
 *  ESERV_Type, SERV_OpenEx, LSOCK_CreateEx
 */
extern NCBI_XCONNECT_EXPORT unsigned short SERV_ServerPort
(const char*          name,
 unsigned int         host
 );


/** Set the implicit server type used for typeless entries returned by service
 * mappers for the specified service.  Entries that already have an explicit
 * type retain that type unchanged.
 *
 * @note The implicit server type is also used to disambiguate incomplete
 *       URL-like strings encountered during service name resolution (see
 *       above).
 *
 * @note The current implementation first attempts to store the association as
 *       a transient setting in the application's registry.  If that fails, it
 *       attempts to store the association in the application environment.
 *
 * @note The implicit server type is managed like other service-related
 *       parameters described in <connect/ncbi_connutil.h>, using the
 *       REG_CONN_IMPLICIT_SERVER_TYPE key.
 *
 * @return
 *  Non-zero on success, or 0 on failure.
 * @sa
 *  ConnNetInfo_GetValue, SERV_GetImplicitServerType
 */
extern NCBI_XCONNECT_EXPORT int/*bool*/ SERV_SetImplicitServerType
(const char* service,
 ESERV_Type  type
 );


/** Get the implicit server type that would be assigned to typeless entries for
 * the specified service.
 * @sa
 *  ConnNetInfo_GetValue, SERV_SetImplicitServerType,
 *  SERV_GetImplicitServerTypeDefault
 */
extern NCBI_XCONNECT_EXPORT ESERV_Type SERV_GetImplicitServerType
(const char* service
 );


#ifdef __cplusplus
}  /* extern "C" */
#endif


/* @} */

#endif /* CONNECT___NCBI_SERVICE__H */
