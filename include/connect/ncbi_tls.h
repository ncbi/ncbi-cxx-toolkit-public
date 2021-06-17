#ifndef CONNECT___NCBI_TLS__H
#define CONNECT___NCBI_TLS__H

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


/** Setup a TLS (Transport Layer Security) provider library to support SSL
 *  in ncbi_socket.h[pp].
 *
 * Currently we support mbedTLS and GNUTLS as the providers.  This call selects
 * a library, which is either the default or requested via the registry (aka
 * .ini file), or through the process environment (takes precedence over the
 * registry):
 *
 * [CONN]
 * USESSL={1,0,MBEDTLS,GNUTLS}
 *
 * CONN_USESSL={1,0,MBEDTLS,GNUTLS}
 *
 * "Off", "No", "False", case-insensitively, are also accepted for "0";  and
 * "On", "Yes", "True" -- for "1".
 *
 * If no provider is present in the build, "0" is assumed.  "1" selects the
 * default provider as currently configured within the toolkit.  With "0", SSL
 * will not be availbale for sockets, and any secure session will fail.
 *
 * @note GNUTLS is only available as an external 3-rd party library, and must
 * be so configured --with-gnutls at the configuration stage of the build.
 * mbedTLS can also be used as an external installation, but the toolkit has
 * an embedded private copy of the library, which can be used transparently
 * without any additional dependencies.  That embedded copy will be used by
 * default, if no other provider is explicitly pre-set, selected, or disabled.
 */
extern NCBI_XCONNECT_EXPORT
SOCKSSL NcbiSetupTls(void);


/* Debugging (see also <connect/ncbi_connutil.h>): sets a log level in the TLS
 * provider library (using an empty setting disables the logging).  Levels are
 * provider-specific, but generally the higher the level the more verbose is
 * the output, and usually level 0 is used to stop the log output altogether.
 * Note that these are looked up as "CONN_TLS_LOGLEVEL" in the environment or
 * "[CONN]TLS_LOGLEVEL" in the registry.
 */
#define REG_CONN_TLS_LOGLEVEL  "TLS_LOGLEVEL"
#define DEF_CONN_TLS_LOGLEVEL  ""

/* Provider-specific log level can be turned on by using the all-capped 
 * provider name merged with the above.  For example, for mbedTLS the setting
 * would be "MBEDTLS_LOGLEVEL" and would have priority over the generic one.
 *
 * Note that GNUTLS has its own environment setting "GNUTLS_DEBUG_LEVEL", which
 * will be considered if none of the TLS_LOGLEVEL settings are encountered.
 */


/** Build NCBI_CRED from memory buffers containing X.509 certificate and
 *  private key, respectively, in either PEM or DER format (independently of
 *  each other).
 *
 *  The returned credentials handle must not be deleted while it's in use.
 *
 *  If a size is passed as 0, the corresponding pointer argument is assumed to
 *  be a '\0'-terminated string, and the size gets computed internally as
 *  strlen(ptr) + 1, to cover the trailing '\0' byte, see below.
 *
 *  To figure out the format of cert / key, the passed buffer gets analyzed to
 *  see whether the last byte is '\0' and whether the therefore properly
 *  terminated C-string happens to contain a "-----BEGIN " substring:  if both
 *  conditions are true, then the buffer is considered in PEM format;  or DER,
 *  otherwise.
 *
 * @note that the size value ("certsz" / "pkeysz"), when expliciltly specified,
 * MUST account for the terminating '\0' byte for PEM formatted buffers.  It
 * MUST always be specified exact for DER formatted buffers.
 *
 * @warning Calling free() on the returned handle will cause memory leaks;  so
 * always use NcbiDeleteTlsCertCredentials() when the handle is no longer
 * needed.
 *
 * @sa
 *  NcbiDeleteTlsCertCredentials
 */

extern NCBI_XCONNECT_EXPORT
NCBI_CRED NcbiCreateTlsCertCredentials(const void* cert,
                                       size_t      certsz,
                                       const void* pkey,
                                       size_t      pkeysz);


/** Delete a NCBI_CRED handle created by NcbiCreateTlsCertCredentials().
 *
 * @warning Do not call this routine while the handle can be potentially in
 * use.
 *
 * @note This routine can be safely used for handles returned by
 * NcbiCredMbedTls() and NcbiCredGnuTls(), to avoid having to delete the
 * underlying TLS-provider-specific handles passed to either call, explicitly.
 *
 * @sa
 *  NcbiCreateTlsCertCredentials, NcbiCredMbedTls, NcbiCredGnuTls
 */
extern NCBI_XCONNECT_EXPORT
void NcbiDeleteTlsCertCredentials(NCBI_CRED cred);


#ifdef __cplusplus
} /* extern "C" */
#endif


/* @} */

#endif /* CONNECT___NCBI_TLS_H */
