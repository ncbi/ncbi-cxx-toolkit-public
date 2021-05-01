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
 *   SSL support in connection library
 *
 */

#include "ncbi_ansi_ext.h"
#include "ncbi_connssl.h"
#include "ncbi_priv.h"
#include "ncbi_servicep.h"
#include <connect/ncbi_gnutls.h>
#include <connect/ncbi_mbedtls.h>
#include <connect/ncbi_tls.h>
#include <stdlib.h>

#define NCBI_USE_ERRCODE_X   Connect_TLS


/* Establish default here! */
#if   defined(HAVE_LIBMBEDTLS)  ||  defined(NCBI_CXX_TOOLKIT)
#  define NcbiSetupDefaultTls  NcbiSetupMbedTls
#elif defined(HAVE_LIBGNUTLS)
#  define NcbiSetupDefaultTls  NcbiSetupGnuTls
#else
#  define NcbiSetupDefaultTls  0
#endif


/*ARGSUSED*/
#ifdef __cplusplus
extern "C" {
    static EIO_Status s_NoTlsInit(FSSLPull unused_pull, FSSLPush unused_push);
}
#endif /*__cplusplus*/
static EIO_Status s_NoTlsInit(FSSLPull unused_pull, FSSLPush unused_push)
{
    CORE_LOG_X(41, eLOG_Critical, "SSL has been explicitly disabled");
    return eIO_NotSupported;
}


static SOCKSSL x_SetupNoTls(void)
{
    static const struct SOCKSSL_struct kNoTlsOps = {
        "NONE",
        s_NoTlsInit
    };
    CORE_LOG_X(42, eLOG_Trace, "SSL has been explicitly disabled");
    return &kNoTlsOps;
}


static FSSLSetup x_NcbiSetupTls(void)
{
    static FSSLSetup s_Setup = (FSSLSetup)(-1L);
    if (s_Setup == (FSSLSetup)(-1L)) {
        char str[32];
        ConnNetInfo_GetValueInternal(0, "USESSL", str, sizeof(str), 0);
        if      (strcasecmp(str, "MBEDTLS") == 0)
            s_Setup = NcbiSetupMbedTls;
        else if (strcasecmp(str, "GNUTLS")  == 0)
            s_Setup = NcbiSetupGnuTls;
        else if (ConnNetInfo_Boolean(str)  ||  !*str) {
            s_Setup = NcbiSetupDefaultTls;
            if (!s_Setup) {
                CORE_LOG_X(44, eLOG_Critical,
                           "No TLS support included in this build");
            }
        } else if (strcmp    (str, "0")     == 0  ||
                   strcasecmp(str, "no")    == 0  ||
                   strcasecmp(str, "off")   == 0  ||
                   strcasecmp(str, "false") == 0) {
            s_Setup = x_SetupNoTls;
        } else {
            CORE_LOGF_X(43, eLOG_Critical,
                        ("Unknown TLS provider \"%s\"", str));
            s_Setup = 0/*unknown provider*/;
        }
    }
    return s_Setup;
}


extern SOCKSSL NcbiSetupTls(void)
{
    FSSLSetup setup = x_NcbiSetupTls();
    return setup ? setup() : 0;
}


extern NCBI_CRED NcbiCreateTlsCertCredentials(const void* cert,
                                              size_t      certsz,
                                              const void* pkey,
                                              size_t      pkeysz)
{
    unsigned int ssltype;
    const char*  sslname;
    if (!cert  ||  !pkey) {
        CORE_LOG_X(45, eLOG_Error,
                   "Cannot create certificate credentials from NULL");
        return 0;
    }
    ssltype = 0;
    if (!(sslname = SOCK_SSLName())) {
        FSSLSetup setup = x_NcbiSetupTls();
#if defined(HAVE_LIBMBEDTLS)  ||  defined(NCBI_CXX_TOOLKIT)
        if (setup == NcbiSetupMbedTls)
            ssltype = eNcbiCred_MbedTls;
#endif /*HAVE_LIBMBEDTLS || NCBI_CXX_TOOLKIT*/
#ifdef HAVE_LIBGNUTLS
        if (setup == NcbiSetupGnuTls)
            ssltype = eNcbiCred_GnuTls;
#endif /*HAVE_LIBGNUTLS*/
        (void) setup;
    } else {
#if defined(HAVE_LIBMBEDTLS)  ||  defined(NCBI_CXX_TOOLKIT)
        if (strcmp(sslname, "MBEDTLS") == 0)
            ssltype = eNcbiCred_MbedTls;
#endif /*HAVE_LIBMBEDTLS || NCBI_CXX_TOOLKIT*/
#ifdef HAVE_LIBGNUTLS
        if (strcmp(sslname, "GNUTLS") == 0)
            ssltype = eNcbiCred_GnuTls;
#endif /*HAVE_LIBGNUTLS*/
    }
    switch (ssltype) {
#if defined(HAVE_LIBMBEDTLS)  ||  defined(NCBI_CXX_TOOLKIT)
    case eNcbiCred_MbedTls:
        return NcbiCreateMbedTlsCertCredentials(cert, certsz, pkey, pkeysz);
#endif /*HAVE_LIBMBEDTLS || NCBI_CXX_TOOLKIT*/
#ifdef HAVE_LIBGNUTLS
    case eNcbiCred_GnuTls:
        return NcbiCreateGnuTlsCertCredentials (cert, certsz, pkey, pkeysz);
#endif /*HAVE_LIBGNUTLS*/
    default:
        CORE_LOG_X(46, eLOG_Critical,
                   "Cannot build certificate credentials: no TLS configured");
        break;
    }
    return 0;
}


extern void NcbiDeleteTlsCertCredentials(NCBI_CRED cred)
{
    char who[40];
    switch (cred->type / 100) {
#if defined(HAVE_LIBMBEDTLS)  ||  defined(NCBI_CXX_TOOLKIT)
    case eNcbiCred_MbedTls / 100:
        NcbiDeleteMbedTlsCertCredentials(cred);
        break;
#endif /*HAVE_LIBMBEDTLS || NCBI_CXX_TOOLKIT*/
#ifdef HAVE_LIBGNUTLS
    case eNcbiCred_GnuTls / 100:
        NcbiDeleteGnuTlsCertCredentials (cred);
        break;
#endif /*HAVE_LIBGNUTLS*/
    default:
        break;
    }
    switch (cred->type / 100) {
#ifndef HAVE_LIBGNUTLS
    case eNcbiCred_GnuTls / 100:
        strcpy(who, "GNUTLS");
        break;
#endif /*!HAVE_LIBGNUTLS*/
#if !defined(HAVE_LIBMBEDTLS)  &&  !defined(NCBI_CXX_TOOLKIT)
    case eNcbiCred_MbedTls / 100:
        strcpy(who, "MBEDTLS");
        break;
#endif /*!HAVE_LIBMBEDTLS && !NCBI_CXX_TOOLKIT*/
    default:
        sprintf(who, "TLS 0x%08X", cred->type);
        break;
    }
    CORE_LOGF_X(47, eLOG_Error,
                ("Deleting unknown certificate credentials (%s/%u)",
                 who, cred->type % 100));
    cred->type = (ENcbiCred) 0;
    free(cred);
}
