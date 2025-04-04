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
 *   Service API test for profiling
 *
 */

#include <connect/ncbi_tls.h>
#include "../ncbi_priv.h"
#include "../ncbi_lbsmd.h"
#include "../ncbi_servicep.h"
#include <stdlib.h>
#include <string.h>

#include "test_assert.h"  /* This header must go last */


int main(int argc, char* argv[])
{
    SERV_ITER      iter;
    int            count;
    int/*bool*/    wildcard;
    SConnNetInfo*  net_info;
    const char*    service = argv[1];

    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Short   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);
    CORE_SetLOGFILE_Ex(stderr, eLOG_Trace, eLOG_Fatal, 0/*no auto-close*/);
    LBSMD_FastHeapAccess(eOn);
    wildcard
        = service && (!*service  ||  strpbrk(service, "?*["))? 1/*T*/ : 0/*F*/;
    if (!wildcard) {
        net_info = ConnNetInfo_Create(service);
        SOCK_SetupSSL(NcbiSetupTls);
    } else
        net_info = 0;
    iter = SERV_OpenP(service,
                      (fSERV_All & ~fSERV_Firewall) |
                      (wildcard ? fSERV_Promiscuous : 0),
                      0, 0, 0.0, net_info, NULL, 0, 0, NULL, NULL);
    for (count = 0;  ;  ++count) {
        SSERV_InfoCPtr info;
        char*          str;
        if (!(info = SERV_GetNextInfo(iter)))
            break;
        if (!(str = SERV_WriteInfo(info)))
            continue;
        CORE_LOGF(eLOG_Note, ("`%s' %s",
                              wildcard ?SERV_NameOfInfo(info) : service, str));
        free(str);
    }
    SERV_Close(iter);
    if (net_info)
        ConnNetInfo_Destroy(net_info);

    CORE_LOGF(eLOG_Note, ("Servers found: %d", count));
    CORE_SetLOG(0);
    return !count;
}
