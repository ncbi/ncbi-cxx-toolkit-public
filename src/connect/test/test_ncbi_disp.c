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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   Standard test for named service resolution facility
 *
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.1  2001/03/01 00:33:59  lavr
 * Initial revision
 *
 * ==========================================================================
 */

#include "../ncbi_priv.h"
#include <connect/ncbi_service.h>
#include <stdio.h>

/* One can define env.var. 'service'_CONN_HOST to reroute dispatching
 * information to particular dispatching host (instead of default).
 */
int main(int argc, const char* argv[])
{
    const char* service = argc > 1 ? argv[1] : "io_bounce";
    const SSERV_Info* info;
    SERV_ITER iter;

    CORE_SetLOGFILE(stderr, 0/*false*/);
    CORE_LOGF(eLOG_Note, ("Looking for service `%s'", service));

    if (!(iter = SERV_OpenSimple(service)))
        CORE_LOG(eLOG_Fatal, "Requested service not found");
    
    while ((info = SERV_GetNextInfo(iter)) != 0) {
        char* info_str = SERV_WriteInfo(info, 0/*do not skip host*/);
        if (!info_str)
            CORE_LOG(eLOG_Error, "Unable to print server info");
        else
            CORE_LOGF(eLOG_Note, ("Service `%s' = %s", service, info_str));
    }
    
    CORE_LOG(eLOG_Note, "Test complete");
    return 0;
}
