#ifndef CONNECT__CONNECT_MISC_IMPL__HPP
#define CONNECT__CONNECT_MISC_IMPL__HPP

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
 * Authors: Rafael Sadyrov
 *
 */

#include "mbedtls/mbedtls/ncbicxx_rename_mbedtls.h"
#include "mbedtls/mbedtls/error.h"
#include "mbedtls/mbedtls/ctr_drbg.h"
#include "mbedtls/mbedtls/entropy.h"
#include "mbedtls/mbedtls/ssl.h"

#include <corelib/ncbimisc.hpp>

#include <connect/connect_export.h>

BEGIN_NCBI_SCOPE

NCBI_EXPORT_FUNC_DECLARE(XCONNECT, mbedtls_ctr_drbg_free);
NCBI_EXPORT_FUNC_DECLARE(XCONNECT, mbedtls_ctr_drbg_init);
NCBI_EXPORT_FUNC_DECLARE(XCONNECT, mbedtls_ctr_drbg_random);
NCBI_EXPORT_FUNC_DECLARE(XCONNECT, mbedtls_ctr_drbg_seed);
NCBI_EXPORT_FUNC_DECLARE(XCONNECT, mbedtls_entropy_free);
NCBI_EXPORT_FUNC_DECLARE(XCONNECT, mbedtls_entropy_func);
NCBI_EXPORT_FUNC_DECLARE(XCONNECT, mbedtls_entropy_init);
NCBI_EXPORT_FUNC_DECLARE(XCONNECT, mbedtls_ssl_close_notify);
NCBI_EXPORT_FUNC_DECLARE(XCONNECT, mbedtls_ssl_conf_alpn_protocols);
NCBI_EXPORT_FUNC_DECLARE(XCONNECT, mbedtls_ssl_conf_authmode);
NCBI_EXPORT_FUNC_DECLARE(XCONNECT, mbedtls_ssl_conf_rng);
NCBI_EXPORT_FUNC_DECLARE(XCONNECT, mbedtls_ssl_config_defaults);
NCBI_EXPORT_FUNC_DECLARE(XCONNECT, mbedtls_ssl_config_free);
NCBI_EXPORT_FUNC_DECLARE(XCONNECT, mbedtls_ssl_config_init);
NCBI_EXPORT_FUNC_DECLARE(XCONNECT, mbedtls_ssl_free);
NCBI_EXPORT_FUNC_DECLARE(XCONNECT, mbedtls_ssl_get_verify_result);
NCBI_EXPORT_FUNC_DECLARE(XCONNECT, mbedtls_ssl_handshake);
NCBI_EXPORT_FUNC_DECLARE(XCONNECT, mbedtls_ssl_init);
NCBI_EXPORT_FUNC_DECLARE(XCONNECT, mbedtls_ssl_read);
NCBI_EXPORT_FUNC_DECLARE(XCONNECT, mbedtls_ssl_session_reset);
NCBI_EXPORT_FUNC_DECLARE(XCONNECT, mbedtls_ssl_set_bio);
NCBI_EXPORT_FUNC_DECLARE(XCONNECT, mbedtls_ssl_set_hostname);
NCBI_EXPORT_FUNC_DECLARE(XCONNECT, mbedtls_ssl_setup);
NCBI_EXPORT_FUNC_DECLARE(XCONNECT, mbedtls_ssl_write);
NCBI_EXPORT_FUNC_DECLARE(XCONNECT, mbedtls_strerror);

END_NCBI_SCOPE

#endif
