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
#include <connect/ncbi_connutil.h>
#include <connect/ncbi_gnutls.h>
#include <connect/ncbi_mbedtls.h>
#include <connect/ncbi_tls.h>


/* Establish default here! */
#if   defined(HAVE_LIBGNUTLS)
#  define NcbiSetupDefaultTls  NcbiSetupGnuTls
#elif defined(HAVE_LIBMBEDTLS) || defined(NCBI_CXX_TOOLKIT)
#  define NcbiSetupDefaultTls  NcbiSetupMbedTls
#else
#  define NcbiSetupDefaultTls  0
#endif


extern SOCKSSL NcbiSetupTls(void)
{
    static FSSLSetup s_Setup = (FSSLSetup)(-1L);
    if (s_Setup == (FSSLSetup)(-1L)) {
        char str[32];
        ConnNetInfo_GetValue(0, "USESSL", str, sizeof(str), 0);
        if (!ConnNetInfo_Boolean(str)) {
            if (strcmp    (str, "0")     == 0  ||
                strcasecmp(str, "no")    == 0  ||
                strcasecmp(str, "off")   == 0  ||
                strcasecmp(str, "false") == 0) {
                s_Setup = 0;
            } else if (strcasecmp(str, "GNUTLS")  == 0) {
                s_Setup = NcbiSetupGnuTls;
            } else if (strcasecmp(str, "MBEDTLS") == 0) {
                s_Setup = NcbiSetupMbedTls;
            } else
                s_Setup = NcbiSetupDefaultTls;
        } else
            s_Setup = NcbiSetupDefaultTls;
    }
    return s_Setup ? s_Setup() : 0;
}
