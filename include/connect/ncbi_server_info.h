#ifndef NCBI_SERVER_INFO__H
#define NCBI_SERVER_INFO__H

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
 * Author:  Denis Vakatov, Anton Lavrentiev
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
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.12  2000/12/06 22:17:02  lavr
 * Binary host addresses are now explicitly stated to be in network byte
 * order, whereas binary port addresses now use native (host) representation
 *
 * Revision 6.11  2000/10/20 17:05:48  lavr
 * TServType made 'unsigned'
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
 * Use home-made ANSI extentions (NCBI_***)
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

#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif


/* Bit-mask of server types
 */
typedef enum {
    fSERV_Ncbid      = 0x1,
    fSERV_Standalone = 0x2,
    fSERV_HttpGet    = 0x4,
    fSERV_HttpPost   = 0x8,
    fSERV_Http       = fSERV_HttpGet | fSERV_HttpPost
#define fSERV_StatelessOnly 0x80
} ESERV_Type;
typedef unsigned TSERV_Type;  /* bit-wise OR of "ESERV_Type" flags */


/* Flags to specify the algorithm for selecting the most preferred
 * server from the set of available servers
 */
typedef enum {
    fSERV_Regular = 0x0,
    fSERV_Blast   = 0x1
} ESERV_Flags;
typedef int TSERV_Flags;

#define SERV_DEFAULT_FLAG       fSERV_Regular


/* Verbal representation of a server type (no internal spaces allowed)
 */
const char* SERV_TypeStr(ESERV_Type type);


/* Read server info type.
 * If successful, assign "type" and return pointer to the position
 * in the "str" immediately following the type tag.
 * On error, return NULL.
 */
const char* SERV_ReadType(const char* str, ESERV_Type* type);


/* Meta-addresses for various types of NCBI servers
 */
typedef struct {
    size_t         args;
#define SERV_NCBID_ARGS(i)      ((char *)(i) + (i)->args)
} SSERV_NcbidInfo;

typedef struct {
    int            dummy;       /* placeholder, not used */
} SSERV_StandaloneInfo;

typedef struct {
    size_t         path;
    size_t         args;
#define SERV_HTTP_PATH(i)       ((char *)(i) + (i)->path)
#define SERV_HTTP_ARGS(i)       ((char *)(i) + (i)->args)
} SSERV_HttpInfo;


/* Generic NCBI server meta-address
 */
typedef union {
    SSERV_NcbidInfo      ncbid;
    SSERV_StandaloneInfo standalone;
    SSERV_HttpInfo       http;
} USERV_Info;

typedef struct {
    ESERV_Type     type;        /* type of server                           */
    unsigned int   host;        /* host the server running on, network b.o. */
    unsigned short port;        /* port the server running on, host b.o.    */
    unsigned short sful;        /* true if this is a stateful server        */
    ESERV_Flags    flag;        /* algorithm flag for the server            */
    time_t         time;        /* relaxation/expiration time/period        */
    double         rate;        /* rate of the server                       */
    USERV_Info     u;           /* server type-specific data/params         */
} SSERV_Info;


/* Constructors for the various types of NCBI server meta-addresses
 */
SSERV_Info* SERV_CreateNcbidInfo
(unsigned int   host,           /* network byte order */
 unsigned short port,           /* host byte order    */
 const char*    args
 );

SSERV_Info* SERV_CreateStandaloneInfo
(unsigned int   host,           /* network byte order */
 unsigned short port            /* host byte order    */
 );

SSERV_Info* SERV_CreateHttpInfo
(ESERV_Type     type,           /* verified, must be one of fSERV_Http* */
 unsigned int   host,           /* network byte order */
 unsigned short port,           /* host byte order    */
 const char*    path,
 const char*    args
 );


/* Dump server info to a string.
 * The server type goes first, and it is followed by a single space.
 * The returned string is '\0'-terminated, and must be deallocated by 'free()'.
 * If 'skip_host' is true, no host address is put to the string
 * (neither it will be if the server info has the host address as 0).
 */
char* SERV_WriteInfo(const SSERV_Info* info, int/*bool*/ skip_host);


/* Read full server info (including type) from string "str"
 * (e.g. composed by SERV_WriteInfo). Result can be later freed by 'free()'.
 * If 'default_host' is not 0, the server info will be assigned this
 * value, and a host address should NOT be specified in the input textual
 * server representation. 'default_host' must be in network byte order.
 */
SSERV_Info* SERV_ReadInfo(const char* info_str, unsigned int default_host);


/* Return an actual size (in bytes) the server info occupies
 * (to be used for copying info structures in whole).
 */
size_t SERV_SizeOfInfo(const SSERV_Info* info);


/* Return 'true' if two server infos are equal.
 */
int/*bool*/ SERV_EqualInfo(const SSERV_Info* info1, const SSERV_Info* info2);


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* NCBI_SERVER_INFO__H */
