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
#include "../ncbi_server_infop.h"
#include <stdio.h>
#include <stdlib.h>

#include "test_assert.h"  /* This header must go last */


int main(int argc, char* argv[])
{
    const char* server = argv[1];
    SSERV_Info* info;

    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);
    CORE_SetLOGFILE(stderr, 0/*false*/);

    if (!server)
        CORE_LOG(eLOG_Fatal, "Server info required");

    if (!(info = SERV_ReadInfoEx(server, 0, 1/*lazy*/)))
        CORE_LOGF(eLOG_Fatal, ("Cannot parse: %s", server));

    if (!(server = SERV_WriteInfo(info)))
        CORE_LOGF(eLOG_Fatal, ("Cannot dump: %s", argv[1]));

    CORE_LOGF(eLOG_Note, ("'%s' = '%s'", argv[1], server));
    free(info);

    if (!(info = SERV_ReadInfo(server)))
        CORE_LOG(eLOG_Fatal, "Cannot re-parse");

    free((void*) server);
    free(info);

    CORE_LOG(eLOG_Note, "TEST completed successfully");
    CORE_SetLOG(0);
    return 0;
}
