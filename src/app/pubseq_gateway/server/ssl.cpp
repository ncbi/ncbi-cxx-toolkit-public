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
 * Authors: Sergey Satskiy
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>

#include "ssl.hpp"
#include "pubseq_gateway.hpp"


void SetupSSL(h2o_accept_ctx_t *  accept_ctx)
{
    static CPubseqGatewayApp *  app = CPubseqGatewayApp::GetInstance();
    static bool                 enabled = app->Settings().m_SSLEnable;

    if (!enabled)
        return;

    static string      cert_file = app->Settings().m_SSLCertFile;;
    static string      key_file = app->Settings().m_SSLKeyFile;
    static string      ciphers = app->Settings().m_SSLCiphers;

    accept_ctx->ssl_ctx = SSL_CTX_new(SSLv23_server_method());
    accept_ctx->expect_proxy_line = 0;

    const long  ssl_flags = SSL_OP_NO_SSLv2 |
                            SSL_OP_NO_SSLv3 |
                            SSL_OP_NO_TLSv1 |
                            SSL_OP_NO_TLSv1_1;
    SSL_CTX_set_options(accept_ctx->ssl_ctx, ssl_flags);
    SSL_CTX_set_ecdh_auto(accept_ctx->.ssl_ctx, 1);
    SSL_CTX_set_timeout(accept_ctx->ssl_ctx, 1);

    if (SSL_CTX_use_certificate_chain_file(accept_ctx->ssl_ctx,
                                           cert_file.c_str()) != 1) {
        NCBI_THROW(CPubseqGatewayException, eConfigurationError,
                   "Error loading SSL certificate from " +
                   cert_file);
    }

    if (SSL_CTX_use_PrivateKey_file(accept_ctx->ssl_ctx,
                                    key_file.c_str(),
                                    SSL_FILETYPE_PEM) != 1) {
        NCBI_THROW(CPubseqGatewayException, eConfigurationError,
                   "Error loading SSL private key from " +
                   key_file);
    }

    if (SSL_CTX_set_cipher_list(accept_ctx->ssl_ctx,
                                ciphers.c_str()) != 1) {
        NCBI_THROW(CPubseqGatewayException, eConfigurationError,
                   "Error setting SSL ciphers (" +
                   ciphers + ")");
    }

    // Note:
    // NPN support could also be added however it does not look necessary
    // because it is for SPDY while http/2 tends to use ALPN
    // #if H2O_USE_NPN
    //     h2o_ssl_register_npn_protocols(accept_ctx->ssl_ctx,
    //                                    h2o_http2_npn_protocols);
    // #endif

    #if H2O_USE_ALPN
        h2o_ssl_register_alpn_protocols(accept_ctx->ssl_ctx,
                                        h2o_http2_alpn_protocols);
    #endif
}

