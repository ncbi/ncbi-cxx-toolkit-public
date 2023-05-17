#ifndef CONNECT_EXT___NCBI_IFCONF__H
#define CONNECT_EXT___NCBI_IFCONF__H

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
 *   Get host IP and related network configuration information
 *
 *   UNIX only!!!
 *
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifndef   INADDR_BROADCAST
#  define INADDR_BROADCAST  ((unsigned int)(-1))
#endif
#ifndef   INADDR_NONE
#  define INADDR_NONE       INADDR_BROADCAST
#endif
#ifndef   INADDR_ANY
#  define INADDR_ANY        0
#endif


#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
    unsigned int address;       /* Primary address, network byte order(b.o.) */
    unsigned int netmask;       /* Primary netmask, network byte order       */
    unsigned int broadcast;     /* Primary broadcast address, network b.o.   */
    int          nifs;          /* Number of network interfaces seen         */
    int          sifs;          /* Number of network interfaces skipped      */
    size_t       mtu;           /* MTU if known for the returned address     */
} SNcbiIfConf;


/* Fill out parameters of primary (first) network interface (NIF) that also has
 * "flags" (e.g. IFF_MULTICAST) set on it, and for which the socket "s" was
 * created ("s" must be >= 0 for the call to work).  Return non-zero if at
 * least one NIF has been found on the machine (not necessarily meeting the
 * conditions);  0 otherwise, with "errno" indicating the last OS error
 * encountered during the search.
 *
 * This call skips all non-running/non-IP NIFs (reflected in "sifs"), or those
 * having loopback address / flag set, being marked as "private", or otherwise
 * having flags and/or netmask unobtainable.
 *
 * In case of a non-zero return, NIF information returned can contain:
 * 1. INADDR_NONE as "address", if no NIF matching "flags" has been found;
   2. INADDR_LOOPBACK as "address", if only loopback NIF(s) have been found;
 *    but in either case (1, 2) "netmask" is guaranteed to have INADDR_ANY
 *    (may also want to check "errno" for more information).
 * 3. If "address" and "netmask" returned equal, it means there's been the same
 *    IP address detected on two or more interfaces ("broadcast" is set to 0
 *    in this case).
 * Note that "broadcast" is only set (non-zero) for a unique NIF found, whose
 * "address" and "netmask" are distinct from INADDR_NONE and INADDR_ANY,
 * respectively.
 *
 * "nifs" is filled with the total number of NIFs scanned during the call.
 *
 * "hint" may indicate a preferable address to return (ignored if APIPA or 0).
 *
 * NOTE:  Addresses returned are in network byte order, whilst INADDR_*
 * constants are always in host byte order [but by the virtue of values,
 * INADDR_NONE and INADDR_ANY are preserved across representations; but beware
 * of INADDR_LOOPBACK!].
 */
extern int/*bool*/ NcbiGetHostIfConfEx(SNcbiIfConf*  c,
                                       int/*socket*/ s,
                                       int/*ifflag*/ flag,
                                       unsigned int  hint);


/* A stream IP socket will be created and closed internally to obtain
 * NIF information.  No special flags will be selected.
 * @sa
 *  NcbiGetHostIfConfEx
 */
extern int/*bool*/ NcbiGetHostIfConf(SNcbiIfConf* c);


/* Equivalent of calling NcbiGetHostIfConf() and if successful,
 * printing out "address" field from NIF information structure.
 * Return "buf" on success, 0 on error.
 */
extern char* NcbiGetHostIP(char* buf, size_t bufsize);


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* CONNECT_EXT___NCBI_IFCONF__H */
