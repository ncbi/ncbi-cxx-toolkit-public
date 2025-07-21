#ifndef CONNECT___NCBI_CONNUTIL__H
#define CONNECT___NCBI_CONNUTIL__H

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
 * Authors:  Denis Vakatov, Anton Lavrentiev
 *
 * File Description:
 *   Auxiliary API to:
 *     1.Retrieve connection related info from the registry:
 *       ConnNetInfo_GetValue()
 *       ConnNetInfo_Boolean()
 *       SConnNetInfo
 *       ConnNetInfo_Create()
 *       ConnNetInfo_Clone()
 *       ConnNetInfo_SetPath()
 *       ConnNetInfo_AddPath()
 *       ConnNetInfo_SetArgs()
 *       ConnNetInfo_SetFrag()
 *       ConnNetInfo_GetArgs()
 *       ConnNetInfo_AppendArg()
 *       ConnNetInfo_PrependArg()
 *       ConnNetInfo_DeleteArg()
 *       ConnNetInfo_DeleteAllArgs()
 *       ConnNetInfo_PreOverrideArg()
 *       ConnNetInfo_PostOverrideArg()
 *       ConnNetInfo_SetupStandardArgs()
 *       ConnNetInfo_SetUserHeader()
 *       ConnNetInfo_AppendUserHeader()
 *       ConnNetInfo_PrependUserHeader()
 *       ConnNetInfo_OverrideUserHeader()
 *       ConnNetInfo_PreOverrideUserHeader()
 *       ConnNetInfo_ExtendUserHeader()
 *       ConnNetInfo_DeleteUserHeader()
 *       ConnNetInfo_SetTimeout()
 *       ConnNetInfo_ParseURL()
 *       ConnNetInfo_URL()
 *       ConnNetInfo_Log()
 *       ConnNetInfo_Destroy()
 *       #define REG_CONN_***
 *       #define DEF_CONN_***
 *
 *     2.Make a connection via an URL:
 *       URL_Connect[Ex]()
 *       
 *     3.Search for a token in an input stream (either CONN, SOCK, or BUF):
 *       CONN_StripToPattern()
 *       SOCK_StripToPattern()
 *       BUF_StripToPattern()
 *
 *     4.Perform URL encoding/decoding of data:
 *       URL_Encode[Ex]()
 *       URL_Decode[Ex]()
 *
 *     5.Compose or parse NCBI-specific Content-Type's:
 *       EMIME_Type
 *       EMIME_SubType
 *       EMIME_Encoding
 *       MIME_ComposeContentType[Ex]()
 *       MIME_ParseContentType()
 *
 */

#include <connect/ncbi_buffer.h>
#include <connect/ncbi_connection.h>


/* Well-known port values */
#define CONN_PORT_FTP    21
#define CONN_PORT_SSH    22
#define CONN_PORT_SMTP   25
#define CONN_PORT_HTTP   80
#define CONN_PORT_HTTPS  443
#define CONN_PORT_MSSQL  1433

/* SConnNetInfo's field lengths NOT including the terminating '\0's */
#define CONN_USER_LEN    63
#define CONN_PASS_LEN    63
#define CONN_HOST_LEN    255
#define CONN_PATH_LEN    4095 /* including arguments and a fragment */


/** @addtogroup UtilityFunc
 *
 * @{
 */


#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
    eReqMethod_Any = 0,
    /* HTTP/1.0 */
    eReqMethod_Get,           /*  1 */
    eReqMethod_Post,          /*  2 */
    eReqMethod_Head,          /*  3 */
    eReqMethod_Connect,       /*  4 */
    /* HTTP/1.1 */
    eReqMethod_v1  = 8,
    eReqMethod_Any11              = eReqMethod_v1 | eReqMethod_Any,
    eReqMethod_Get11              = eReqMethod_v1 | eReqMethod_Get,
    eReqMethod_Post11             = eReqMethod_v1 | eReqMethod_Post,
    eReqMethod_Head11             = eReqMethod_v1 | eReqMethod_Head,
    eReqMethod_Connect11          = eReqMethod_v1 | eReqMethod_Connect,
    eReqMethod_Put = 16,      /* 16 */
    eReqMethod_Patch,         /* 17 */
    eReqMethod_Trace,         /* 18 */
    eReqMethod_Delete,        /* 19 */
    eReqMethod_Options        /* 20 */
} EReqMethod;

typedef unsigned TReqMethod;  /* EReqMethod or (EReqMethod | eReqMethod_v1) */


typedef enum {
    eURL_Unspec = 0,
    eURL_Https,
    eURL_File,
    eURL_Http,
    eURL_Ftp
} EURLScheme;

typedef unsigned EBURLScheme;


typedef enum {
    eFWMode_Legacy   = 0,  /**< Relay, no firewall                           */
    eFWMode_Adaptive = 1,  /**< Regular firewall ports first, then fallback  */
    eFWMode_Firewall = 2,  /**< Regular firewall ports only, no fallback     */
    eFWMode_Fallback = 3   /**< Fallback ports only (w/o trying any regular) */
} EFWMode;

typedef unsigned EBFWMode;


typedef enum {
    eDebugPrintout_None = 0,
    eDebugPrintout_Some,
    eDebugPrintout_Data
} EDebugPrintout;

typedef unsigned EBDebugPrintout;


typedef enum {
    fProxy_None = 0,  /**< No $http_proxy / $https_proxy used */
    fProxy_Http,      /**< $http_proxy  used                  */
    fProxy_Https      /**< $https_proxy used                  */
} EProxyType;

typedef unsigned EBProxyType;


/* Network connection-related configurable informational structure.
 * ATTENTION:  Do NOT fill out this structure (SConnNetInfo) "from scratch"!
 *             Instead, use ConnNetInfo_Create() described below to create it,
 *             and then fix (hard-code) some fields using the ConnNetInfo API,
 *             or directly as the last resort if really necessary.
 * NOTE1:      Not every field may be fully utilized throughout the library.
 * NOTE2:      HTTP passwords can be either clear text or Base-64 encoded value
 *             enclosed in square brackets [] (which are not Base-64 charset).
 *             For encoding / decoding, one can use the command-line OpenSSL:
 *             echo [-n] "password|base64value" | openssl enc {-e|-d} -base64
 *             or an online tool (search the Web for "base64 online").
 * NOTE3:      The NCBI_CRED credentials are stored by the handle value and are
 *             not otherwise managed (reallocated / duplicated) by the API:
 *             they are passed strictly "as-is" to the lower levels of the SSL.
 */
typedef struct {  /* NCBI_FAKE_WARNING: ICC */
    char            client_host[CONN_HOST_LEN+1]; /*client hostname('\0'=def)*/
    TReqMethod      req_method:5;     /* method to use in the request (HTTP) */
    EBURLScheme     scheme:3;         /* only pre-defined types (limited)    */
    unsigned        external:1;       /* mark service request as external    */
    EBFWMode        firewall:2;       /* to use firewall (relay otherwise)   */
    unsigned        stateless:1;      /* to connect in HTTP-like fashion only*/
    unsigned        lb_disable:1;     /* to disable local load-balancing     */
    unsigned        http_version:1;   /* HTTP/1.v (or selected by req_method)*/
    EBDebugPrintout debug_printout:2; /* switch to printout some debug info  */
    unsigned        http_push_auth:1; /* push authorize tags even w/o 401/407*/
    unsigned        http_proxy_leak:1;/* TRUE when may fallback to direct    */
    unsigned        http_proxy_skip:1;/* TRUE when *NOT* to read $http_proxy */
    EBProxyType     http_proxy_mask:2;/* $http_proxy / $https_proxy          */
    unsigned        reserved:11;      /* MBZ                                 */
    char            user[CONN_USER_LEN+1];  /* username (if spec'd or req'd) */
    char            pass[CONN_PASS_LEN+1];  /* password (for non-empty user) */
    char            host[CONN_HOST_LEN+1];  /* host name to connect to       */
    unsigned short  port;                   /* port # (host byte order)      */
    char            path[CONN_PATH_LEN+1];  /* path (incl. args and frag)    */
    char            http_proxy_host[CONN_HOST_LEN+1]; /* HTTP proxy server   */
    unsigned short  http_proxy_port;        /* port # of HTTP proxy server   */
    char            http_proxy_user[CONN_USER_LEN+1]; /* HTTP proxy username */
    char            http_proxy_pass[CONN_PASS_LEN+1]; /* HTTP proxy password */
    unsigned short  max_try;          /* max. # of attempts to connect (>= 1)*/
    unsigned short  unused;           /* MBZ; 8-byte alignment               */
    const STimeout* timeout;          /* ptr to I/O timeout(infinite if NULL)*/
    const char*     http_user_header; /* user header to add to HTTP request  */
    const char*     http_referer;     /* request referrer (when spec'd)      */
    NCBI_CRED       credentials;      /* connection credentials (optional)   */

    /* the following fields are for internal use only -- look but don't touch*/
    unsigned int    magic;            /* to detect version skew              */
    STimeout        tmo;              /* default storage for finite timeout  */
    const char      svc[1];           /* service which this info created for */
} SConnNetInfo;


/* Defaults and the registry entry names for "SConnNetInfo" fields
 */
#define DEF_CONN_REG_SECTION        "CONN"

#define REG_CONN_REQ_METHOD         "REQ_METHOD"
#define DEF_CONN_REQ_METHOD         "ANY"

#define REG_CONN_USER               "USER"
#define DEF_CONN_USER               ""

#define REG_CONN_PASS               "PASS"
#define DEF_CONN_PASS               ""

#define REG_CONN_HOST               "HOST"
#define DEF_CONN_HOST               "www.ncbi.nlm.nih.gov"

#define REG_CONN_PORT               "PORT"
#define DEF_CONN_PORT               0/*default*/

#define REG_CONN_PATH               "PATH"
#define DEF_CONN_PATH               "/Service/dispd.cgi"

#define REG_CONN_ARGS               "ARGS"
#define DEF_CONN_ARGS               ""

#define REG_CONN_HTTP_PROXY_HOST    "HTTP_PROXY_HOST"
#define DEF_CONN_HTTP_PROXY_HOST    ""

#define REG_CONN_HTTP_PROXY_PORT    "HTTP_PROXY_PORT"
#define DEF_CONN_HTTP_PROXY_PORT    ""

#define REG_CONN_HTTP_PROXY_USER    "HTTP_PROXY_USER"
#define DEF_CONN_HTTP_PROXY_USER    ""

#define REG_CONN_HTTP_PROXY_PASS    "HTTP_PROXY_PASS"
#define DEF_CONN_HTTP_PROXY_PASS    ""

#define REG_CONN_HTTP_PROXY_LEAK    "HTTP_PROXY_LEAK"
#define DEF_CONN_HTTP_PROXY_LEAK    ""

#define REG_CONN_HTTP_PROXY_SKIP    "HTTP_PROXY_SKIP"
#define DEF_CONN_HTTP_PROXY_SKIP    ""

#define REG_CONN_HTTP_PUSH_AUTH     "HTTP_PUSH_AUTH"
#define DEF_CONN_HTTP_PUSH_AUTH     ""

#define REG_CONN_TIMEOUT            "TIMEOUT"
#define DEF_CONN_TIMEOUT            30.0

#define REG_CONN_MAX_TRY            "MAX_TRY"
#define DEF_CONN_MAX_TRY            3

#define REG_CONN_EXTERNAL           "EXTERNAL"
#define DEF_CONN_EXTERNAL           ""

#define REG_CONN_FIREWALL           "FIREWALL"
#define DEF_CONN_FIREWALL           ""

#define REG_CONN_STATELESS          "STATELESS"
#define DEF_CONN_STATELESS          ""

#define REG_CONN_LB_DISABLE         "LB_DISABLE"
#define DEF_CONN_LB_DISABLE         ""

#define REG_CONN_HTTP_VERSION       "HTTP_VERSION"
#define DEF_CONN_HTTP_VERSION       0

#define REG_CONN_DEBUG_PRINTOUT     "DEBUG_PRINTOUT"
#define DEF_CONN_DEBUG_PRINTOUT     ""

#define REG_CONN_HTTP_USER_HEADER   "HTTP_USER_HEADER"
#define DEF_CONN_HTTP_USER_HEADER   ""

#define REG_CONN_HTTP_REFERER       "HTTP_REFERER"
#define DEF_CONN_HTTP_REFERER       0

/* Environment/registry keys that are *not* kept in SConnNetInfo */
#define REG_CONN_LOCAL_ENABLE       "LOCAL_ENABLE"
#define REG_CONN_LBNULL_ENABLE      "LBNULL_ENABLE"
#define REG_CONN_LBSMD_DISABLE      "LBSMD_DISABLE"
#define REG_CONN_LBDNS_ENABLE       "LBDNS_ENABLE"
#define REG_CONN_LINKERD_ENABLE     "LINKERD_ENABLE"
#define REG_CONN_NAMERD_ENABLE      "NAMERD_ENABLE"
#define REG_CONN_DISPD_DISABLE      "DISPD_DISABLE"

/* Substitute (redirected) service name */
#define REG_CONN_SERVICE_NAME       DEF_CONN_REG_SECTION "_" "SERVICE_NAME"


/* Local service dispatcher */
#define REG_CONN_LOCAL_SERVICES     DEF_CONN_REG_SECTION "_" "LOCAL_SERVICES"
#define REG_CONN_LOCAL_SERVER       DEF_CONN_REG_SECTION "_" "LOCAL_SERVER"


/* Implicit server type (LBNULL/LINKERD/NAMERD) */
#define REG_CONN_IMPLICIT_SERVER_TYPE  "IMPLICIT_SERVER_TYPE"


/* Local IP table */
#define DEF_CONN_LOCAL_IPS          "LOCAL_IPS"
#define REG_CONN_LOCAL_IPS          DEF_CONN_REG_SECTION "_" DEF_CONN_LOCAL_IPS
#define DEF_CONN_LOCAL_IPS_DISABLE  "NONE"


/* Lookup "param" in the registry / environment.  "param" is usually all-caps.
 * If "param" does not begin with "CONN_", then "CONN_" or "_CONN_" gets 
 * automatically injected in all lookups listed below, unless otherwise noted.
 * The order of search is the following (the first match causes to return):
 * 1. Environment variable "service_CONN_param", where "service" gets dashes
 *    (if any) replaced with underscores: first looked up converted to all
 *    uppercase, and if that failed, then looked up with the original case in
 *    the "service" part;
 * 2. Registry key "CONN_param" in the section '["service"]';
 * 3. Environment setting "CONN_param" (in all upper-case);
 * 4. Registry key "param" (with the leading "CONN_" stripped) in the default
 *    section "[CONN]".
 * Steps 1 & 2 skipped for "service" passed as either NULL or empty ("").
 * Steps 3 & 4 skipped for a non-empty "service" and a "param" that already
 *             begins with "CONN_".
 * If the found match's value has enveloping quotes (either single '' or
 * double ""), then they are stripped from the result, which can then become
 * empty.
 * Up to "value_size" bytes (including the terminating '\0') of the result get
 * copied into the "value" buffer (which may cause truncation!).  When no match
 * is found, the "value" gets filled up to "value_size" bytes with "def_value"
 * (or an empty string).  Return 0 on out of memory (including "value_size"
 * given as 0) or value truncation;  otherwise, return "value".
 * @note
 *  "value" is always '\0'-terminated if "value_size" is greater than 0.
 */
extern NCBI_XCONNECT_EXPORT const char* ConnNetInfo_GetValue
(const char* service,
 const char* param,
 char*       value,
 size_t      value_size,
 const char* def_value    /* NB: both NULL and "" have the same effect */
 );


/* Return non-zero if "str" (when non-NULL, non-empty) represents a boolean
 * true value;  return 0 otherwise.
 */
extern NCBI_XCONNECT_EXPORT int/*bool*/ ConnNetInfo_Boolean
(const char* str
 );


/* This function fills out the "*info" structure using registry entries named
 * (see above) in the REG_CONN_<NAME> macros (defaults are in DEF_CONN_<NAME>):
 *
 *  -- INFO FIELD --  ----- NAME -----  ----------- REMARKS/EXAMPLES ----------
 *  client_host       (auto-assigned)   local host name filled in automatically
 *  req_method        REQ_METHOD        only "GET"/"POST"/"ANY" allowed
 *  user              USER
 *  pass              PASS
 *  host              HOST
 *  port              PORT
 *  path              PATH
 *                    ARGS              combined in "path" now -- use API calls
 *  http_version      HTTP_VERSION      HTTP version override
 *  http_proxy_host   HTTP_PROXY_HOST   if empty http_proxy_port is set to 0
 *  http_proxy_port   HTTP_PROXY_PORT   no HTTP proxy if 0
 *  http_proxy_user   HTTP_PROXY_USER
 *  http_proxy_pass   HTTP_PROXY_PASS
 *  http_proxy_leak   HTTP_PROXY_LEAK   1 means to also re-try w/o the proxy
 *  http_proxy_skip   HTTP_PROXY_SKIP   1 means to skip $http_proxy environment
 *  http_proxy_only   (auto-assigned)   1 means proxy loaded from $http_proxy
 *  http_push_auth    HTTP_PUSH_AUTH    1 if to send credentials pre-emptively
 *  timeout           TIMEOUT           "<sec>.<usec>": "3.00005", "infinite"
 *  max_try           MAX_TRY
 *  external          EXTERNAL          1 to mark requests as "external"
 *  firewall          FIREWALL
 *  stateless         STATELESS
 *  lb_disable        LB_DISABLE        obsolete, disables the "LBSMD" mapper
 *  debug_printout    DEBUG_PRINTOUT   "SOME"(eqv "TRUE") / "DATA"(eqv "ALL")
 *  http_user_header  HTTP_USER_HEADER  "\r\n" (if missing) is auto-appended
 *  http_referer      HTTP_REFERER      may be assigned automatically
 *  svc               SERVICE_NAME      no search/no value without service
 *
 * A value of the field NAME is first looked up in the environment variable of
 * the form <service>_CONN_<NAME>;  then in the current corelib registry, in
 * the section 'service' by using the CONN_<NAME> key;  then in the environment
 * variable again, but using the name CONN_<NAME>;  and finally, in the default
 * registry section (DEF_CONN_REG_SECTION), using just <NAME>.  If the service
 * is NULL or empty then the first two steps in the above lookup are skipped.
 *
 * For default values see right above, within macros DEF_CONN_<NAME>.
 *
 * @sa
 *  ConnNetInfo_GetValue
 */
extern NCBI_XCONNECT_EXPORT SConnNetInfo* ConnNetInfo_Create
(const char* service
 );


/* Make an exact and independent copy of "*net_info".
 */
extern NCBI_XCONNECT_EXPORT SConnNetInfo* ConnNetInfo_Clone
(const SConnNetInfo* net_info
 );


/* Convenience routines to manipulate URL path, arguments, and fragment, all of
 * which are now stored combined in SConnNetInfo::path.
 *
 * @warning
 * Arguments provided to the SConnNetInfo::path manipulation routines below
 * may not point to anywhere inside the path stored within the SConnNetInfo
 * structure, which is being modified.
 *
 * All argument modification routines below assume that "arg" is either a
 * single arg name or an "arg=val" pair (a fragment part, separated by '#', if
 * any, is ignored).
 * In the former case, an additional "val" may be supplied separately (and will
 * be prepended with '=').  In the latter case, also having a non-zero string
 * in the "val" argument may result in an incorrect behavior.  The ampersand
 * ('&') gets automatically managed to keep the arg list consistent.
 *
 * Return values (if non-void):  non-zero(true) on success; 0(false) on error.
 */

/* Set the path part in the path element.
 * New path may also contain either or both the argument part (separated by
 * '?'), and the fragment part (separated by '#'), in this order.  The new path
 * will replace the existing one up to and including the last specified part,
 * preserving the remainder.  Thus, "" causes only the path part to be removed
 * (leaving any existing argument(s) and fragment alone).  However, "#" will
 * cause to clear the entire path (single trailing '#' is never stored, but is
 * always considered).  Also, NULL clears the entire path element. */
extern NCBI_XCONNECT_EXPORT int/*bool*/ ConnNetInfo_SetPath
(SConnNetInfo*       net_info,
 const char*         path);

/* Add the path part to the path element.
 * New path may also contain either or both the argument part (separated by
 * '?'), and the fragment part (separated by '#'), in this order.  The new path
 * will extend the existing one up to and including the last specified part,
 * preserving the remainder.  If the new path begins with a slash ('/'), the
 * extension starts at the end of the existing path, making sure the slash is
 * not doubled;  if the new path does not begin with a slash, the extension
 * starts after the last slash found in the existing path, or at the very
 * beginning, if there was no slash in the old path.  Note that NULL or ""
 * cause no effect.
 *
 * Corner cases:  if the new "path" begins with the arguments part ('?'), then
 * the path part will be retained but the arguments part (and possibly the
 * fragment, too, if specified) will be replaced.  If the new "path" begins
 * with '#', then only the fragment part is replaced ("#" causes the fragment
 * to be dropped, and the single trailing '#' will not be stored).
 *
 * Example: "/path1/path2?args1&args2#" extends the existing path part with
 * appending two more levels "/path1/path2" to it, replaces all existing
 * arguments with "args1&args2", and drops any exising fragment (not storing
 * the trailing '#').  If the trailing '#' wasn't specified, the old fragment
 * (if any) would have been kept intact.  "/path1/path2#frag" would work
 * similarly with path, but drop all existing args, and replace the existing
 * fragment.
 *
 * Note that incoming "path" is assumed to be minimally syntactically correct,
 * so passing "/path1/path2?#frag" is allowed but will create the path element
 * with a lone '?' character, just as given.
 */
extern NCBI_XCONNECT_EXPORT int/*bool*/ ConnNetInfo_AddPath
(SConnNetInfo*       net_info,
 const char*         path);

/* Set the args part (which may contain or just be a #frag part) in the path
 * element, adding the '?' separator automatically, if needed.  If the fragment
 * is missing from the new args, the existing one will be preserved;  and if
 * no arguments are provided before the new fragment, that part of the exising
 * arguments will not get modified.  Thus, "" causes all arguments but the
 * fragment to be removed.  NULL clears all existing args and frag. */
extern NCBI_XCONNECT_EXPORT int/*bool*/ ConnNetInfo_SetArgs
(SConnNetInfo*       net_info,
 const char*         args);

/* Set fragment part only;  delete if "frag" is NULL, "", or "#".  The passed
 * string may start with '#';  otherwise, the '#' separator will be added to
 * the path element. */
extern NCBI_XCONNECT_EXPORT int/*bool*/ ConnNetInfo_SetFrag
(SConnNetInfo*       net_info,
 const char*         frag);

/* Return args (without leading '?' but with leading '#' if fragment only. */
extern NCBI_XCONNECT_EXPORT const char* ConnNetInfo_GetArgs
(const SConnNetInfo* net_info
 );

/* Append an argument to the end of the list, preserving any #frag if there. */
extern NCBI_XCONNECT_EXPORT int/*bool*/ ConnNetInfo_AppendArg
(SConnNetInfo*       net_info,
 const char*         arg,
 const char*         val
 );

/* Insert an argument at the front of the list, preserving any #frag part. */
extern NCBI_XCONNECT_EXPORT int/*bool*/ ConnNetInfo_PrependArg
(SConnNetInfo*       net_info,
 const char*         arg,
 const char*         val
 );

/* Delete an argument from the list of arguments in the path element.  In case
 * the passed arg is specified as "arg=val&arg2...", the value as well as any
 * successive arguments are ignored in the search for the matching argument.
 * Return zero if no arg was found;  non-zero if it was found and deleted.
 * Any existing #frag part is not modified. */
extern NCBI_XCONNECT_EXPORT int/*bool*/ ConnNetInfo_DeleteArg
(SConnNetInfo*       net_info,
 const char*         arg
 );

/* Delete all arguments specified in "args" (regardless of their values) from
 * the path element.  Any existing #frag part if not modified. */
extern NCBI_XCONNECT_EXPORT void        ConnNetInfo_DeleteAllArgs
(SConnNetInfo*       net_info,
 const char*         args
 );

/* Same as sequence DeleteAll(arg) then Prepend(arg, val), see above. */
extern NCBI_XCONNECT_EXPORT int/*bool*/ ConnNetInfo_PreOverrideArg
(SConnNetInfo*       net_info,
 const char*         arg,
 const char*         val
 );

/* Same as sequence DeleteAll(arg) then Append(arg, val), see above. */
extern NCBI_XCONNECT_EXPORT int/*bool*/ ConnNetInfo_PostOverrideArg
(SConnNetInfo*       net_info,
 const char*         arg,
 const char*         val
 );


/* Set up standard arguments:  service(as passed), address, and platform.
 * Also, set up the "User-Agent:" HTTP header (using CORE_GetAppName()) in
 * SConnNetInfo::http_user_header.  Return non-zero on success; zero on error.
 * @note
 *  "service" passed as NULL does not add any service argument, yet "service"
 *  passed as empty string ("") removes the service argument.
 * @note
 *  The call can sacrifice the platform and address (in this order) in order to
 *  fit the service argument (when provided non-empty).
 * @sa
 *  CORE_GetAppName, CORE_GetPlatform
 */
extern NCBI_XCONNECT_EXPORT int/*bool*/ ConnNetInfo_SetupStandardArgs
(SConnNetInfo*       net_info,
 const char*         service
 );


/* Set new user header (discard previously set header, if any).  Remove the old
 * header (if any) only, if "header" is NULL or empty ("" or all blanks).
 * @warning
 *   New "header" may not be any part of the header kept in the structure.
 * Return non-zero if successful;  otherwise, return 0 to indicate an error.
 */
extern NCBI_XCONNECT_EXPORT int/*bool*/ ConnNetInfo_SetUserHeader
(SConnNetInfo*       net_info,
 const char*         header
 );


/* Append to user header (same as ConnNetInfo_SetUserHeader() if no previous
 * header was set);  do nothing if the provided "header" is NULL or empty.
 * @warning
 *   New "header" may not be any part of the header kept in the structure.
 * Return non-zero if successful;  otherwise, return 0 to indicate an error.
 */
extern NCBI_XCONNECT_EXPORT int/*bool*/ ConnNetInfo_AppendUserHeader
(SConnNetInfo*       net_info,
 const char*         header
 );


/* Prepend to user header (same as ConnNetInfo_SetUserHeader() if no previous
 * header was set);  do nothing if the provided "header" is NULL or empty.
 * @warning
 *   New "header" may not be any part of the header kept in the structure.
 * Return non-zero if successful;  otherwise, return 0 to indicate an error.
 */
extern NCBI_XCONNECT_EXPORT int/*bool*/ ConnNetInfo_PrependUserHeader
(SConnNetInfo*       net_info,
 const char*         header
 );


/* Override user header.
 * Tags replaced (case-insensitively), and tags with empty values effectively
 * delete all existing occurrences of the matching tags from the old user
 * header, e.g. "My-Tag:\r\n" deletes all appearences of "My-Tag: [<value>]"
 * from the user header, regardless of the "<value>" (if any).
 * Unmatched tags with non-empty values are simply added to the existing user
 * header (as with "Append" above).  No-op if "header" is NULL or empty.
 * @note
 *   New "header" may be part of the current header from the structure.
 * Return non-zero if successful, otherwise return 0 to indicate an error.
 */
extern NCBI_XCONNECT_EXPORT int/*bool*/ ConnNetInfo_OverrideUserHeader
(SConnNetInfo*       net_info,
 const char*         header
 );

#define ConnNetInfo_PostOverrideUserHeader  ConnNetInfo_OverrideUserHeader


/* Pre-override user header.
 * Tags replaced (case-insensitively), and tags with empty values effectively
 * delete all existing occurrences of the matching tags from the old user
 * header, e.g. "My-Tag:\r\n" deletes all appearences of "My-Tag: [<value>]"
 * from the user header, regardless of the "<value>" (if any).
 * Unmatched tags with non-empty values get prepended to the existing user
 * header (as with "Prepend" above).  No-op if "header" is NULL or empty.
 * @note
 *   New "header" may be part of the current header from the structure.
 * Return non-zero if successful, otherwise return 0 to indicate an error.
 */
extern NCBI_XCONNECT_EXPORT int/*bool*/ ConnNetInfo_PreOverrideUserHeader
(SConnNetInfo*       net_info,
 const char*         header
 );


/* Extend user header.
 * Existing tags matching (case-insensitively) first appearances of those
 * from "header" get appended with the new value (separated by a space) if the
 * added value is non-empty, otherwise, the tags are left untouched.  However,
 * if the new tag value matches (case-insensitively) tag's value already in the
 * header, the new value does not get added (to avoid duplicates).
 * All other new tags from "header" with non-empty values get added at the end
 * of the user header (as with "Append" above).
 * @note
 *   New "header" may be part of the current header from the structure.
 * Return non-zero if successful, otherwise return 0 to indicate an error.
 */
extern NCBI_XCONNECT_EXPORT int/*bool*/ ConnNetInfo_ExtendUserHeader
(SConnNetInfo*       net_info,
 const char*         header
 );


/* Delete entries from current user header, if their tags match those passed in
 * "header" (regardless of the values, if any).
 * @note
 *   New "header" may be part of the current header from the structure.
 */
extern NCBI_XCONNECT_EXPORT void        ConnNetInfo_DeleteUserHeader
(SConnNetInfo*       net_info,
 const char*         header
 );


/* Set the timeout.  Accepted values can include a valid pointer (to a finite
 * timeout) or kInfiniteTimeout (or 0) to denote the infinite timeout value.
 * Note that kDefaultTimeout as a pointer value is not accepted.
 * Return non-zero (TRUE) on success, or zero (FALSE) on error.
 */
extern NCBI_XCONNECT_EXPORT int/*bool*/ ConnNetInfo_SetTimeout
(SConnNetInfo*       net_info,
 const STimeout*     timeout
 );


/* Parse URL into "*net_info", using defaults provided via "*net_info".
 * In case of a relative URL, only those URL elements provided in it,
 * will get replaced in the resultant "*net_info".
 * Return non-zero if successful, otherwise return 0 to indicate an error.
 */
extern NCBI_XCONNECT_EXPORT int/*bool*/ ConnNetInfo_ParseURL
(SConnNetInfo*       net_info,
 const char*         url
 );


/* Reconstruct text URL out of the SConnNetInfo's components (excluding
 * username:password for safety reasons).  Return NULL on error.
 * Returned non-NULL string must be free()'d when no longer needed.
 */
extern NCBI_XCONNECT_EXPORT char*       ConnNetInfo_URL
(const SConnNetInfo* net_info
 );


/* Log the contents of "*net_info" into specified "log" with severity "sev".
 */
extern NCBI_XCONNECT_EXPORT void        ConnNetInfo_Log
(const SConnNetInfo* net_info,
 ELOG_Level          sev,
 LOG                 log
 );


/* Destroy and deallocate "net_info" (if not NULL;  otherwise, do nothing).
 */
extern NCBI_XCONNECT_EXPORT void        ConnNetInfo_Destroy
(SConnNetInfo*       net_info
 );


/** Extra URL_ConnectEx() parameters. */
typedef struct {
    NCBI_CRED   cred;  /**< SSL credentials (if any)                       */
    const char* host;  /**< SSL host id (aka SNI) (if differs from "host") */
} SURLExtra;

/* Very low-level HTTP initiation routine.  Regular use is highly discouraged.
 * Instead, please consider using higher level APIs such as HTTP connections
 * or streams:
 * @sa
 *  HTTP_CreateConnector, CConn_HttpStream
 * 
 * Hit a URL "http[s]://host[:port]/path[?args]" with the following request
 * (argument substitution is shown as enclosed in angle brackets (not present
 * in the actual request), and all optional parts shown in square brackets):
 *
 *    METHOD <path>[?<args>] HTTP/1.x\r\n
 *    [Content-Length: <content_length>\r\n]
 *    [<user_header>\r\n]\r\n
 *
 * If "port" is not specified (0) it will be assigned automatically to a
 * well-known standard value depending on the "fSOCK_Secure" bit in the "flags"
 * parameter, when connecting to an HTTP server.
 *
 * A protocol version "1.x" is selected by the "req_method"'s value, and can be
 * either 1.0 or 1.1.  METHOD can be any of those of EReqMethod.
 *
 * @note that unlike the now-deprecated URL_Connect(), this call never encodes
 * any "args", and never auto-inserts any "Host:" tag into the headers (unless
 * provided by the "user_header" argument).
 *
 * Request method "eReqMethod_Any/11" selects an appropriate method depending
 * on the value of "content_length":  results in GET when no content is
 * expected ("content_length"==0), and POST when "content_length" is non-zero.
 *
 * The "content_length" parameter must specify the exact(!) amount of data that
 * is going to be sent (0 if none) to HTTP server.  The "Content-Length" header
 * with the specified value gets added to all legal requests (but GET / HEAD /
 * CONNECT).  Special case of (-1L) suppresses the header.
 * Note that if "user_header" contains a "Transfer-Encoding:" header tag, then
 * "content_length" *must* be passed as (-1L) (RFC7230, 3.3.2).
 *
 * Alternatively, "content_length" can specify the amount of initial data to be
 * sent with a CONNECT request into the established tunnel, and held by the
 * "args" parameter (although, no header tag will get added to the HTTP header
 * in that case, see below).  Note, however, that RFC7231, 4.3.6 warns that the
 * behavior of sending a payload with CONNECT is not well-defined.
 *
 * If the string "user_header" is not NULL/empty, it will be stripped off any
 * surrounding white space (including "\r\n"), then get added to the HTTP
 * header, followed by the header termination sequence of "\r\n\r\n".
 * @note that any interior whitespace in "user_header" is not analyzed/guarded.
 *
 * The "extra" parameter is only used to pass additional connection credentials
 * and server name info for secure connections, and is ignored otherwise.
 *
 * If the request method contains "eReqMethod_Connect", then the connection is
 * assumed to be tunneled via a proxy, so "path" must specify a "host:port"
 * pair to connect to;  "content_length" can provide initial size of the data
 * to be tunneled, in which case "args" must be a pointer to such data, but no
 * "Content-Length:" header tag will get added.
 *
 * If "*sock" is non-NULL, the call _does not_ create a new socket, but builds
 * an HTTP(S) data stream on top of the passed socket.  Regardless of the
 * completion status, the original SOCK handle will be closed as if with
 * SOCK_Destroy().  In case of success, a new SOCK handle will be returned via
 * the same last parameter;  yet in case of errors, the last parameter will be
 * updated to read as NULL.
 *
 * On success, return eIO_Success and non-NULL handle of a socket via the last
 * parameter.
 *
 * The returned socket must be exipicitly closed by "SOCK_Close[Ex]()" when no
 * longer needed.
 *
 * NOTE: The returned socket may not be immediately readable/writeable if
 *       either open or read/write timeouts were passed as {0,0}, meaning that
 *       both connection and HTTP header write operation may still be pending
 *       in the resultant socket.  It is responsibility of the application to
 *       analyze the actual socket state in this case (see "ncbi_socket.h").
 * @sa
 *  SOCK_Create, SOCK_CreateOnTop, SOCK_Wait, SOCK_Status,
 *  SOCK_Abort, SOCK_Destroy, SOCK_CloseEx
 */
extern NCBI_XCONNECT_EXPORT EIO_Status URL_ConnectEx
(const char*     host,            /* must be provided                        */
 unsigned short  port,            /* may be 0, defaulted to either 80 or 443 */
 const char*     path,            /* must be provided                        */
 const char*     args,            /* may be NULL or empty                    */
 TReqMethod      req_method,      /* ANY selects method by "content_length"  */
 size_t          content_length,  /* may not be used with HEAD or GET        */
 const STimeout* o_timeout,       /* timeout for an OPEN stage               */
 const STimeout* rw_timeout,      /* timeout for READ and WRITE              */
 const char*     user_header,     /* should include "Host:" in most cases    */
 SURLExtra*      extra,           /* additional connection params, if any    */
 TSOCK_Flags     flags,           /* additional socket requirements          */
 SOCK*           sock             /* returned socket (on eIO_Success only)   */
 );


/* Equivalent to the above except that it returns a non-NULL socket handle
 * on success, and NULL on error without providing a reason for the failure.
 *
 * @note that only HTTP/1.0 methods can be used with this call.  For HTTP/1.1
 * see URL_ConnectEx().
 *
 * For GET/POST(or ANY) methods the call attempts to provide a "Host:" HTTP tag
 * using information from the "host" and "port" parameters if such a tag is not
 * found within "user_header" (notwithstanding below, the port part of the tag
 * does not get added for non-specified "port" passed as 0).  Note that the
 * result of the above-said auto-magic can be incorrect for HTTP retrievals
 * through a proxy server (since the built tag would correspond to the proxy
 * connection point, but not the actual server as it should have):
 *
 *    Host: host[:port]\r\n
 *
 * @warning DO NOT USE THIS CALL!
 *
 * CAUTION:  If requested, "args" can get encoded but will do that as a whole!
 *
 * @sa
 *  URL_ConnectEx
 */
#ifndef NCBI_DEPRECATED
#  define NCBI_CONNUTIL_DEPRECATED
#else
#  define NCBI_CONNUTIL_DEPRECATED NCBI_DEPRECATED
#endif
extern NCBI_XCONNECT_EXPORT NCBI_CONNUTIL_DEPRECATED SOCK URL_Connect
(const char*     host,            /* must be provided                        */
 unsigned short  port,            /* may be 0, defaulted to either 80 or 443 */
 const char*     path,            /* must be provided                        */
 const char*     args,            /* may be NULL or empty                    */
 EReqMethod      req_method,      /* ANY selects method by "content_length"  */
 size_t          content_length,  /* may not be used with HEAD or GET        */
 const STimeout* o_timeout,       /* timeout for an OPEN stage               */
 const STimeout* rw_timeout,      /* timeout for READ and WRITE              */
 const char*     user_header,     /* may get auto-extended with a "Host" tag */
 int/*bool*/     encode_args,     /* URL-encode "args" entirely (CAUTION!)   */
 TSOCK_Flags     flags            /* additional socket requirements          */
 );


/** Discard all input data before (and including) the first occurrence of a
 * "pattern".  If "discard" is not NULL then add the stripped data (including
 * the "pattern") to it.  If "n_discarded" is not NULL then "*n_discarded"
 * will get the number of actually stripped bytes.  If there was some excess
 * read, push it back to the original source (and not count as discarded).
 * NOTE:  If "pattern_size" == 0, then "pattern" is ignored (and is assumed to
 * be NULL), and the stripping continues until EOF;  if "pattern" is NULL and
 * "pattern_size" is not 0, then exactly "pattern_size" bytes will have
 * attempted to be stripped (unless an I/O error occurs prematurely).
 *
 * @return eIO_Success when the requested operation has completed successfully
 * (pattern found, or the requested number of bytes was skipped, including when
 * either was followed by the end-of-file condition), and if the "discard"
 * buffer was provided, then everything skipped has been successfully stored in
 * it.  Otherwise, return other error code, and store everything read / skipped
 * thus far in the "discard" buffer, if provided.  Naturally, it never returns
 * eIO_Success when requested to skip through the end of file, but eIO_Closed
 * when the EOF has been reached.  Note that memory allocation errors (such as
 * unable to save skipped data in the "discard" buffer) will be assigned the
 * eIO_Unknown code -- which as well can be returned from the failing I/O).
 *
 * @note To distiguish the nature of eIO_Unknown (whether it is I/O or memory)
 * one can check the grow of the buffer size after the call, and compare it to
 * the value of "*n_discarded": if they agree, there was an I/O error, not the
 * buffer's.
 */
extern NCBI_XCONNECT_EXPORT EIO_Status CONN_StripToPattern
(CONN        conn,
 const void* pattern,
 size_t      pattern_size,
 BUF*        discard,
 size_t*     n_discarded
 );


extern NCBI_XCONNECT_EXPORT EIO_Status SOCK_StripToPattern
(SOCK        sock,
 const void* pattern,
 size_t      pattern_size,
 BUF*        discard,
 size_t*     n_discarded
 );


extern NCBI_XCONNECT_EXPORT EIO_Status BUF_StripToPattern
(BUF         buffer,
 const void* pattern,
 size_t      pattern_size,
 BUF*        discard,
 size_t*     n_discarded
 );


/* URL-encode up to "src_size" symbols(bytes) from buffer "src_buf".
 * Write the encoded data to buffer "dst_buf", but no more than "dst_size"
 * bytes.
 * Assign "*src_read" to the # of bytes successfully encoded from "src_buf".
 * Assign "*dst_written" to the # of bytes written to buffer "dst_buf".
 * @note "dst_buf" is not getting '\0'-terminated.
 */
extern NCBI_XCONNECT_EXPORT void URL_Encode
(const void* src_buf,      /* [in]     non-NULL  */
 size_t      src_size,     /* [in]               */
 size_t*     src_read,     /* [out]    non-NULL  */
 void*       dst_buf,      /* [in/out] non-NULL  */
 size_t      dst_size,     /* [in]               */
 size_t*     dst_written   /* [out]    non-NULL  */
 );


/* Act just like URL_Encode (see above) but caller can allow the specified
 * non-standard URL symbols in the input buffer to be encoded "as is".
 * The extra allowed symbols are passed in a '\0'-terminated string
 * "allow_symbols" (it can be NULL or empty -- then this will be an exact
 * equivalent of URL_Encode).
 * @note "dst_buf" is not getting '\0'-terminated.
 */
extern NCBI_XCONNECT_EXPORT void URL_EncodeEx
(const void* src_buf,      /* [in]     non-NULL  */
 size_t      src_size,     /* [in]               */
 size_t*     src_read,     /* [out]    non-NULL  */
 void*       dst_buf,      /* [in/out] non-NULL  */
 size_t      dst_size,     /* [in]               */
 size_t*     dst_written,  /* [out]    non-NULL  */
 const char* allow_symbols /* [in]     '\0'-term */
 );


/* URL-decode up to "src_size" symbols(bytes) from buffer "src_buf".
 * Write the decoded data to buffer "dst_buf", but no more than "dst_size"
 * bytes.
 * Assign "*src_read" to the # of bytes successfully decoded from "src_buf".
 * Assign "*dst_written" to the # of bytes written to buffer "dst_buf".
 * Return FALSE (0) only if cannot decode anything, and an unrecoverable
 * URL-encoding error (such as an invalid symbol or a bad "%.." sequence)
 * has occurred.
 * NOTE:  the unfinished "%.." sequence is fine -- return TRUE, but dont
 *        "read" it.
 */
extern NCBI_XCONNECT_EXPORT int/*bool*/ URL_Decode
(const void* src_buf,      /* [in]     non-NULL  */
 size_t      src_size,     /* [in]               */
 size_t*     src_read,     /* [out]    non-NULL  */
 void*       dst_buf,      /* [in/out] non-NULL  */
 size_t      dst_size,     /* [in]               */
 size_t*     dst_written   /* [out]    non-NULL  */
 );


/* Act just like URL_Decode (see above) but caller can allow the specified
 * non-standard URL symbols in the input buffer to be decoded "as is".
 * The extra allowed symbols are passed in a '\0'-terminated string
 * "allow_symbols" (it can be NULL or empty -- then this will be an exact
 * equivalent of URL_Decode).
 */
extern NCBI_XCONNECT_EXPORT int/*bool*/ URL_DecodeEx
(const void* src_buf,      /* [in]     non-NULL  */
 size_t      src_size,     /* [in]               */
 size_t*     src_read,     /* [out]    non-NULL  */
 void*       dst_buf,      /* [in/out] non-NULL  */
 size_t      dst_size,     /* [in]               */
 size_t*     dst_written,  /* [out]    non-NULL  */
 const char* allow_symbols /* [in]     '\0'-term */
 );



/****************************************************************************
 * NCBI-specific MIME content type and sub-types
 * (the API to compose and parse them)
 *    Content-Type: <type>/<MIME_ComposeSubType()>\r\n
 *
 *    Content-Type: <type>/<subtype>-<encoding>\r\n
 *
 * where  MIME_ComposeSubType(EMIME_SubType subtype, EMIME_Encoding encoding):
 *   "x-<subtype>-<encoding>":
 *     "x-<subtype>",   "x-<subtype>-urlencoded",   "x-<subtype>-<encoding>",
 *     "x-dispatch",    "x-dispatch-urlencoded",    "x-dispatch-<encoding>
 *     "x-asn-text",    "x-asn-text-urlencoded",    "x-asn-text-<encoding>
 *     "x-asn-binary",  "x-asn-binary-urlencoded",  "x-asn-binary-<encoding>"
 *     "x-www-form",    "x-www-form-urlencoded",    "x-www-form-<encoding>"
 *     "html",          "html-urlencoded",          "html-<encoding>"
 *     "x-unknown",     "x-unknown-urlencoded",     "x-unknown-<encoding>"
 *
 *  Note:  <subtype> and <encoding> are expected to contain only
 *         alphanumeric symbols, '-' and '_'. They are case-insensitive.
 ****************************************************************************/


/* Type
 */
typedef enum {
    eMIME_T_Undefined = -1,
    eMIME_T_NcbiData = 0,  /* "x-ncbi-data"   (NCBI-specific data)           */
    eMIME_T_Text,          /* "text"                                         */
    eMIME_T_Application,   /* "application"                                  */
    /* eMIME_T_???,           "<type>" here go other types                   */
    eMIME_T_Unknown        /* "unknown"                                      */
} EMIME_Type;


/* SubType
 */
typedef enum {
    eMIME_Undefined = -1,
    eMIME_Dispatch = 0,    /* "x-dispatch"    (dispatcher info)              */
    eMIME_AsnText,         /* "x-asn-text"    (text ASN.1 data)              */
    eMIME_AsnBinary,       /* "x-asn-binary"  (binary ASN.1 data)            */
    eMIME_Fasta,           /* "x-fasta"       (data in FASTA format)         */
    eMIME_WwwForm,         /* "x-www-form"    (Web form submission)          */
    /* standard MIMEs */
    eMIME_Html,            /* "html"                                         */
    eMIME_Plain,           /* "plain"                                        */
    eMIME_Xml,             /* "xml"                                          */
    eMIME_XmlSoap,         /* "xml+soap"                                     */
    eMIME_OctetStream,     /* "octet-stream"                                 */
    /* eMIME_???,             "<subtype>" here go other NCBI subtypes        */
    eMIME_Unknown          /* "x-unknown"     (arbitrary binary data)        */
} EMIME_SubType;


/* Encoding
 */
typedef enum {
    eENCOD_None = 0,       /* ""              (the content is passed "as is")*/
    eENCOD_Url,            /* "-urlencoded"   (the content is URL-encoded)   */
    /* eENCOD_???,            "-<encoding>" here go other NCBI encodings     */
    eENCOD_Unknown         /* "-encoded"      (unknown encoding)             */
} EMIME_Encoding;


#define CONN_CONTENT_TYPE_LEN  63
/* unfortunate naming, better not to use, as it is actually a size, not len */
#define  MAX_CONTENT_TYPE_LEN  (CONN_CONTENT_TYPE_LEN+1)


/* Write up to "bufsize" bytes (including the '\0' terminator) to "buf":
 *   Content-Type: <type>/[x-]<subtype>-<encoding>\r\n
 * Return the "buf" pointer when successful, or NULL when failed.
 */
extern NCBI_XCONNECT_EXPORT char* MIME_ComposeContentTypeEx
(EMIME_Type     type,
 EMIME_SubType  subtype,
 EMIME_Encoding encoding,
 char*          buf,
 size_t         bufsize    /* should be at least CONN_CONTENT_TYPE_LEN+1     */
 );

/* Parse the NCBI-specific content-type; the (case-insensitive) "str"
 * can be in the following two formats:
 *   Content-Type: <type>/x-<subtype>-<encoding>
 *   <type>/x-<subtype>-<encoding>
 *
 * NOTE:  all leading spaces and all trailing spaces (and any trailing symbols,
 *        if they separated from the content type by at least one space) will
 *        be ignored, e.g. these are valid content type strings:
 *           "   Content-Type: text/plain  foobar"
 *           "  text/html \r\n  barfoo baz ....\n etc"
 *
 * PERFORMANCE NOTE:  this call uses dynamic heap allocations internally.
 *
 * If it does not match any of NCBI MIME type/subtypes/encodings, then
 * return TRUE, eMIME_T_Unknown, eMIME_Unknown or eENCOD_None, respectively.
 * If the passed "str" has an invalid (non-HTTP ContentType) format
 * (or if it is NULL/empty), then
 * return FALSE, eMIME_T_Undefined, eMIME_Undefined, and eENCOD_None
 */
extern NCBI_XCONNECT_EXPORT int/*bool*/ MIME_ParseContentTypeEx
(const char*     str,      /* the HTTP "Content-Type:" header to parse */
 EMIME_Type*     type,     /* can be NULL */
 EMIME_SubType*  subtype,  /* can be NULL */
 EMIME_Encoding* encoding  /* can be NULL */
 );


#ifdef __cplusplus
}  /* extern "C" */
#endif


/* @} */

#endif /* CONNECT___NCBI_CONNUTIL__H */
