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

#include "../ncbi_priv.h"
#include "../ncbi_lbsmd.h"
#include "../ncbi_servicep.h"
#include <stdlib.h>
#include <string.h>
/* This header must go last */
#include "test_assert.h"


int main(int argc, char* argv[])
{
    SERV_ITER      iter;
    int            count;
    const char*    service = argv[1];

    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);
    CORE_SetLOGFILE(stderr, 0/*false*/);
    LBSMD_FastHeapAccess(eOn);

    iter = SERV_OpenP(service, fSERV_All
                      | (service  &&  (!*service  ||  strpbrk(service, "?*"))
                         ? fSERV_Promiscuous : 0), 0, 0, 0.0,
                      NULL, NULL, 0, 0, NULL, NULL);
    for (count = 0;  ; count++) {
        SSERV_InfoCPtr info;
        char*          str;
        if (!(info = SERV_GetNextInfo(iter)))
            break;
        /*if (!(str = SERV_WriteInfo(info)))
            continue;
        CORE_LOG(eLOG_Note, str);
        free(str);*/
    }
    SERV_Close(iter);

    CORE_LOGF(eLOG_Note, ("Servers found: %d", count));
    CORE_SetLOG(0);
    return !count;
}
