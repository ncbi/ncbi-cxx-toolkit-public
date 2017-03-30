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
#include <connect/ncbi_connutil.h>
#include <connect/ncbi_gnutls.h>
#include <connect/ncbi_mbedtls.h>
#include <connect/ncbi_tls.h>


/* Establish default here! */
#if   defined(HAVE_LIBGNUTLS)
#  define NcbiSetupDefaultTls  NcbiSetupGnuTls
#elif defined(HAVE_LIBMBEDTLS)  ||  defined(NCBI_CXX_TOOLKIT)
#  define NcbiSetupDefaultTls  NcbiSetupMbedTls
#else
#  define NcbiSetupDefaultTls  0
#endif


/*ARGSUSED*/
#ifdef __cplusplus
extern "C"
#endif /*__cplusplus*/
static EIO_Status s_NoTlsInit(FSSLPull unused_pull, FSSLPush unused_push)
{
    CORE_LOG(eLOG_Critical, "SSL has been explicitly disabled");
    return eIO_NotSupported;
}


static SOCKSSL x_SetupNoTls(void)
{
    static const struct SOCKSSL_struct kNoTlsOps = {
        s_NoTlsInit
    };
    CORE_LOG(eLOG_Trace, "SSL has been explicitly disabled");
    return &kNoTlsOps;
}


extern SOCKSSL NcbiSetupTls(void)
{
    static FSSLSetup s_Setup = (FSSLSetup)(-1L);
    if (s_Setup == (FSSLSetup)(-1L)) {
        char str[32];
        ConnNetInfo_GetValue(0, "USESSL", str, sizeof(str), 0);
        if (!ConnNetInfo_Boolean(str)  &&  *str) {
            if (strcmp    (str, "0")     == 0  ||
                strcasecmp(str, "no")    == 0  ||
                strcasecmp(str, "off")   == 0  ||
                strcasecmp(str, "false") == 0) {
                s_Setup = x_SetupNoTls;
            } else if (strcasecmp(str, "GNUTLS")  == 0) {
                s_Setup = NcbiSetupGnuTls;
            } else if (strcasecmp(str, "MBEDTLS") == 0) {
                s_Setup = NcbiSetupMbedTls;
            } else {
                CORE_LOGF(eLOG_Critical, ("Unknown TLS provider \"%s\"", str));
                s_Setup = 0/*unknown provider*/;
            }
        } else if (!(s_Setup = NcbiSetupDefaultTls))
            CORE_LOG(eLOG_Critical, "No TLS support included in this build");
    }
    return s_Setup ? s_Setup() : 0;
}
