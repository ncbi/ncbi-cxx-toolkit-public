#ifndef NCBI_CONNUTIL__H
#define NCBI_CONNUTIL__H

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
 * Author:  Denis Vakatov
 *
 * File Description:
 *   Auxiliary API to:
 *    1.Retrieve connection related info from the registry:
 *       SConnNetInfo
 *       ConnNetInfo_Create()
 *       ConnNetInfo_AdjustForHttpProxy()
 *       ConnNetInfo_Clone()
 *       ConnNetInfo_Print()
 *       ConnNetInfo_Destroy()
 *       #define REG_CONN_***
 *       #define DEF_CONN_***
 *
 *    2.Make a connection to an URL:
 *       URL_Connect()
 *       
 *    3.Perform URL encoding/decoding of data:
 *       URL_Decode()
 *       URL_DecodeEx()
 *       URL_Encode()
 *
 *    4.Compose or parse NCBI-specific Content-Type's:
 *       EMIME_SubType
 *       EMIME_Encoding
 *       MIME_ComposeContentType()
 *       MIME_ParseContentType()
 *
 *    5.Search for a token in the input stream (either CONN or SOCK):
 *       CONN_StripToPattern()
 *       SOCK_StripToPattern()
 *
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.1  2000/03/24 22:52:48  vakatov
 * Initial revision
 *
 * ==========================================================================
 */

#include <connect/ncbi_core.h>
#include <connect/ncbi_buffer.h>
#include <connect/ncbi_socket.h>
#include <connect/ncbi_connection.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif


/* Network connection related configurable info struct
 */
typedef struct {
    char           client_host[64];  /* effective client hostname */
    char           host[64];         /* server: host */
    unsigned short port;             /* server: service port */
    char           path[1024];       /* server: path(e.g. to  a CGI script) */
    char           args[1024];       /* server: args(e.g. for a CGI script) */
    STimeout       timeout;          /* i/o timeout  */
    unsigned int   max_try;          /* max. # of attempts to establish conn */
    char           http_proxy_host[64];  /* hostname of HTTP proxy server */
    unsigned short http_proxy_port;      /* port #   of HTTP proxy server */
    char           proxy_host[64];   /* host of CERN-like firewall proxy srv */
    int/*bool*/    debug_printout;   /* printout some debug info */
    int/*bool*/    firewall;         /* if to work in the firewall mode */
    int/*bool*/    lb_disable;       /* to disable local load-balancing */
    unsigned short ncbid_port;       /* port of NCBID CGI script(used by LB) */
    char           ncbid_path[256];  /* path to NCBID CGI script(used by LB) */

    /* the following field(s) are for the internal use only! */
    int/*bool*/    http_proxy_adjusted;
} SConnNetInfo;


/* Defaults and the registry entry names for "SConnNetInfo" fields
 */
#define DEF_CONN_REG_SECTION      "CONN_NETWORK"
                                  
#define REG_CONN_ENGINE_HOST      "CONN_HOST"
#define DEF_CONN_ENGINE_HOST      "www.ncbi.nlm.nih.gov"
                                  
#define REG_CONN_ENGINE_PORT      "CONN_PORT"
#define DEF_CONN_ENGINE_PORT      80
                                  
#define REG_CONN_ENGINE_PATH      "CONN_PATH"
#define DEF_CONN_ENGINE_PATH      "/Service/dispd.cgi"
                                  
#define REG_CONN_ENGINE_ARGS      "CONN_ARGS"
#define DEF_CONN_ENGINE_ARGS      ""
                                  
#define REG_CONN_TIMEOUT          "CONN_TIMEOUT"
#define DEF_CONN_TIMEOUT          30.0
                                  
#define REG_CONN_MAX_TRY          "CONN_MAX_TRY"
#define DEF_CONN_MAX_TRY          3
                                  
#define REG_CONN_PROXY_HOST       "CONN_PROXY_HOST"
#define DEF_CONN_PROXY_HOST       ""
                                  
#define REG_CONN_HTTP_PROXY_HOST  "CONN_HTTP_PROXY_HOST"
#define DEF_CONN_HTTP_PROXY_HOST  ""
                                  
#define REG_CONN_HTTP_PROXY_PORT  "CONN_HTTP_PROXY_PORT"
#define DEF_CONN_HTTP_PROXY_PORT  80
                                  
#define REG_CONN_DEBUG_PRINTOUT   "CONN_DEBUG_PRINTOUT"
#define DEF_CONN_DEBUG_PRINTOUT   ""
                                  
#define REG_CONN_FIREWALL         "CONN_FIREWALL"
#define DEF_CONN_FIREWALL         ""
                                  
#define REG_CONN_LB_DISABLE       "CONN_LB_DISABLE"
#define DEF_CONN_LB_DISABLE       ""

#define REG_CONN_NCBID_PORT       "CONN_NCBID_PORT"
#define DEF_CONN_NCBID_PORT       80
                                  
#define REG_CONN_NCBID_PATH       "CONN_NCBID_PATH"
#define DEF_CONN_NCBID_PATH       "/Service/ncbid.cgi"
                                  


/* This function to fill out the "*info" structure using
 * registry entries named (see above) in macros REG_CONN_<NAME>:
 *
 *  -- INFO FIELD  ----- NAME --------------- REMARKS/EXAMPLES ---------
 *   client_host        <will be assigned to the local host name>
 *   host               HOST
 *   port               PORT
 *   path               PATH
 *   args               ARGS
 *   timeout            TIMEOUT             "<sec>.<usec>": "30.0", "0.005"
 *   max_try            MAX_TRY  
 *   http_proxy_host    HTTP_PROXY_HOST     no HTTP proxy if empty/NULL
 *   http_proxy_port    HTTP_PROXY_PORT
 *   proxy_host         PROXY_HOST
 *   debug_printout     DEBUG_PRINTOUT
 *   firewall           FIREWALL
 *   lb_disable         LB_DISABLE
 *   ncbid_port         NCBID_PORT
 *   ncbid_path         NCBID_PATH
 *
 * For default values see right above, in macros DEF_CONN_<NAME>.
 */
extern SConnNetInfo* ConnNetInfo_Create
(const char* reg_section  /* if NULL/empty then DEF_CONN_REG_SECTION */
 );


/* Adjust the "host:port" to "proxy_host:proxy_port", and
 * "path" to "http://host:port/path" to connect through a HTTP proxy.
 * Return FALSE if already adjusted(see the NOTE), or if cannot adjust
 * (e.g. if "host" + "path" are too long).
 * NOTE:  it does nothing if applied more then once to the same "info"(or its
 *        clone), or when "http_proxy_host" is NULL.
 */
extern int/*bool*/ ConnNetInfo_AdjustForHttpProxy
(SConnNetInfo* info
 );


/* Make an exact and independent copy of "*info".
 */
extern SConnNetInfo* ConnNetInfo_Clone
(const SConnNetInfo* info
 );


/* Printout the "*info" to file "fp".
 */
extern void ConnNetInfo_Print
(const SConnNetInfo* info,
 FILE*               fp
 );


/* Destroy "**info" and deallocate "*info" if "*info" is not NULL.
 * Assign "*info" to NULL.
 */
extern void ConnNetInfo_Destroy
(SConnNetInfo** info
 );


/* Hit URL "http://host:port/path?args" with:
 *    POST <path>?<args> HTTP/1.0\r\n
 *    <user_header\r\n>
 *    Content-Length: <content_length>\r\n\r\n
 * If "encode_args" is TRUE then URL-encode the "args".
 * "args" can be NULL/empty -- then the '?' symbol does not get added.
 * The "content_length" is mandatory, and it specifies an exact(!) amount
 * of data that you are planning to sent to the resultant socket.
 * If string "user_header" is not NULL/empty, then it must be terminated by a
 * single '\r\n'.
 * 
 * On success, return non-NULL handle to a readable & writable NCBI socket.
 * ATTENTION:  due to the very essence of the HTTP connection, you can
 *             perform only one { WRITE, ..., WRITE, READ, ..., READ } cycle.
 * On error, return NULL.
 *
 * NOTE: the socket must be closed by "ncbi_sock.h:SOCK_Close()" when not
 *       needed anymore.
 */

extern SOCK URL_Connect
(const char*     host,
 unsigned short  port,
 const char*     path,
 const char*     args,
 size_t          content_length,
 const STimeout* c_timeout,       /* timeout for the CONNECT stage */
 const STimeout* rw_timeout,      /* timeout for READ and WRITE */
 const char*     user_header,
 int/*bool*/     encode_args      /* URL-encode the "args", if any */
 );


/* Discard all input data before(and including) the first occurence of
 * "pattern". If "buf" is not NULL then add the discarded data(including
 * the "pattern") to it. If "n_discarded" is not NULL then "*n_discarded"
 * will return # of discarded bytes.
 */
extern EIO_Status CONN_StripToPattern
(CONN        conn,
 const void* pattern,
 size_t      pattern_size,
 BUF*        buf,
 size_t*     n_discarded
 );

extern EIO_Status SOCK_StripToPattern
(SOCK        sock,
 const void* pattern,
 size_t      pattern_size,
 BUF*        buf,
 size_t*     n_discarded
 );


/* URL-decode up to "src_size" symbols(bytes) from buffer "src_buf".
 * Write the decoded data to buffer "dst_buf", but no more than "dst_size"
 * bytes.
 * Assign "*src_read" to the # of bytes succesfully decoded from "src_buf".
 * Assign "*dst_written" to the # of bytes written to buffer "dst_buf".
 * Return FALSE only if cannot decode nothing, and an unrecoverable
 * URL-encoding error (such as an invalid symbol or a bad "%.." sequence)
 * has occured.
 * NOTE:  the unfinished "%.." sequence is fine -- return TRUE, but dont
 *        "read" it.
 */
extern int/*bool*/ URL_Decode
(const void* src_buf,    /* [in]     non-NULL */
 size_t      src_size,   /* [in]              */
 size_t*     src_read,   /* [out]    non-NULL */
 void*       dst_buf,    /* [in/out] non-NULL */
 size_t      dst_size,   /* [in]              */
 size_t*     dst_written /* [out]    non-NULL */
 );


/* Act just like URL_Decode (see above) but caller can allow the specified
 * non-standard URL symbols in the input buffer to be decoded "as is".
 * The extra allowed symbols are passed in a '\0'-terminated string
 * "allow_symbols" (it can be NULL or empty -- then this will be an exact
 * equivalent of URL_Decode).
 */
extern int/*bool*/ URL_DecodeEx
(const void* src_buf,      /* [in]     non-NULL  */
 size_t      src_size,     /* [in]               */
 size_t*     src_read,     /* [out]    non-NULL  */
 void*       dst_buf,      /* [in/out] non-NULL  */
 size_t      dst_size,     /* [in]               */
 size_t*     dst_written,  /* [out]    non-NULL  */
 const char* allow_symbols /* [in]     '\0'-term */
 );


/* URL-encode up to "src_size" symbols(bytes) from buffer "src_buf".
 * Write the encoded data to buffer "dst_buf", but no more than "dst_size"
 * bytes.
 * Assign "*src_read" to the # of bytes succesfully encoded from "src_buf".
 * Assign "*dst_written" to the # of bytes written to buffer "dst_buf".
 */
extern void URL_Encode
(const void* src_buf,    /* [in]     non-NULL */
 size_t      src_size,   /* [in]              */
 size_t*     src_read,   /* [out]    non-NULL */
 void*       dst_buf,    /* [in/out] non-NULL */
 size_t      dst_size,   /* [in]              */
 size_t*     dst_written /* [out]    non-NULL */
 );


/****************************************************************************
 * NCBI-specific MIME content type and sub-types
 * (the API to compose and parse them)
 *    Content-Type: x-ncbi-data/<MIME_ComposeSubType()>\r\n
 *
 *    Content-Type: x-ncbi-data/x-<subtype>-<encoding>\r\n
 *
 * where  MIME_ComposeSubType(EMIME_SubType subtype, EMIME_Encoding encoding):
 *   "x-<subtype>-<encoding>":
 *     "x-<subtype>",   "x-<subtype>-url-encoded",   "x-<subtype>-<encoding>",
 *     "x-asn-text",    "x-asn-text-url-encoded",    "x-asn-text-<encoding>
 *     "x-asn-binary",  "x-asn-binary-url-encoded",  "x-asn-binary-<encoding>"
 *     "x-unknown",     "x-unknown-url-encoded",     "x-unknown-<encoding>"
 *
 *  Note:  <subtype> and <encoding> are expected to contain only
 *         alphanumeric symbols, '-' and '_'. They are case-insensitive.
 ****************************************************************************/

/* SubType
 */
typedef enum {
  eMIME_AsnText = 0,  /* "x-asn-text"    (text ASN.1 data) */
  eMIME_AsnBinary,    /* "x-asn-binary"  (binary ASN.1 data) */
  eMIME_Fasta,        /* "x-fasta"       (data in FASTA format) */
  eMIME_Dispatch,     /* "x-dispatch"    (request for a service) */
  /* eMIME_???,          "x-<subtype>"   here go other NCBI subtypes */
  eMIME_Unknown       /* "x-unknown"     (an arbitrary binary data) */
} EMIME_SubType;

/* Encoding
 */
typedef enum {
  eENCOD_Url = 0,  /* "-url-encoded"  (the content is URL-encoded) */
  /* eENCOD_???,      "-<encoding>"   here go other NCBI encodings */
  eENCOD_None      /* ""              (the content is passed "as is") */
} EMIME_Encoding;


/* Write up to "buflen" bytes to "buf":
 *   Content-Type: x-ncbi-data/x-<subtype>-<encoding>\r\n
 * Return pointer to the "buf".
 */
#define MAX_CONTENT_TYPE_LEN 64
extern char* MIME_ComposeContentType
(EMIME_SubType  subtype,
 EMIME_Encoding encoding,
 char*          buf,
 size_t         buflen    /* must be at least MAX_CONTENT_TYPE_LEN */
 );

/* Parse the NCBI-specific content-type; the (case-insensitive) "str"
 * can be in the following two formats:
 *   Content-Type: x-ncbi-data/x-<subtype>-<encoding>
 *   x-ncbi-data/x-<subtype>-<encoding>
 * If it does not match any of NCBI MIME types then return TRUE,
 * eMIME_Unknown and eENCOD_None.
 * If the passed "str" has an invalid(non-HTTP ContentType) format
 * (or if it is NULL/empty) then return FALSE, eMIME_Unknown and eENCOD_None.
 */
extern int/*bool*/ MIME_ParseContentType
(const char*     str,      /* the HTTP "Content-Type:" header to parse */
 EMIME_SubType*  subtype,  /* can be NULL */
 EMIME_Encoding* encoding  /* can be NULL */
 );


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* NCBI_CONNUTIL__H */
