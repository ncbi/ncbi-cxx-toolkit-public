#ifndef CONNECT___NCBI_SSL__H
#define CONNECT___NCBI_SSL__H

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
 *   SSL (Secure Socket Layer) support in connection library
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


/** Setup a TLS (Transport Layer Security) vendor library to support SSL
 *  in ncbi_socket.h[pp].
 *
 * Currently we support mbedTLS and GNUTLS as the vendors.  This call selects
 * the library, which is the default, or which is requested via the registry
 * (aka .ini file) or through the process environment (takes precedence over
 * the registry):
 *
 * [CONN]
 * USESSL={1,0,MBEDTLS,GNUTLS}
 *
 * CONN_USESSL={1,0,MBEDTLS,GNUTLS}
 *
 * "Off", "No", "False", case-insensitively, are also accepted for "0"; and
 * "On", "Yes", "True" -- for "1".
 *
 * If the vendor is not present in the build, "0" is assumed.  "1" selects the
 * default vendor as currently configured within the toolkit.  With "0", SSL
 * will not be availbale for sockets, and any secure session will fail.
 *
 * @note GNUTLS is only available as an external 3-rd party library, and must
 * be so configured --with-gnutls at the configuration stage of the build.
 * mbedTLS can also be used as an external installation, but the toolkit has
 * an embedded private copy of the library, which can be used transparently
 * without any additional dependencies.  That embedded copy will be used as
 * the default, if no other vendors are explicitly present.
 */
extern NCBI_XCONNECT_EXPORT
SOCKSSL NcbiSetupTls(void);


#ifdef __cplusplus
} /* extern "C" */
#endif


/* @} */

#endif /* CONNECT___NCBI_SSL_H */
