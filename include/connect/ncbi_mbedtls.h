#ifndef CONNECT___NCBI_MBEDTLS__H
#define CONNECT___NCBI_MBEDTLS__H

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
 *   mbedTLS support for SSL (Secure Socket Layer) in connection library
 *
 */

#include <connect/ncbi_socket.h>


/** @addtogroup Sockets
 *
 * @{
 */


#ifdef __cplusplus
extern "C" {
#endif


/** Explicitly setup mbedTLS library to support SSL in ncbi_socket.h[pp].
 *
 * @note Do not use this call!  Instead use NcbiSetupTls declared in
 *       <connect/ncbi_tls.h>.
 *
 * @sa
 *  NcbiSetupTls
 */
extern NCBI_XCONNECT_EXPORT
SOCKSSL NcbiSetupMbedTls(void);


/** Convert native mbedTLS certificate credentials' handles into an abstract
 *  toolkit handle.  Two arguments are _required_:  a pointer to a parsed
 *  certificate (xcert), and a pointer to a parsed private key (xpkey).
 *
 * @note The returned handle should be free()'d when no longer needed (that is,
 * when it no longer can be accessed and used by any CONNECT API).  The
 * underlying mbedTLS certificate and private key must be released using an
 * appropriate native API.
 *
 * @warning The call does not create a copy of either xcert or xpkey, so they
 * must remain valid for the entire duration of a session (or sessions) that
 * they are being used with.
 */
extern NCBI_XCONNECT_EXPORT
NCBI_CRED NcbiCredMbedTls(void* xcert, void* xpkey);


#ifdef __cplusplus
} /* extern "C" */
#endif


/* @} */

#endif /* CONNECT___NCBI_MBEDTLS_H */
