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
 * File Description:
 *   Definitions of the configuration parameters for connect/services.
 *
 * Authors:
 *   Dmitry Kazimirov
 *
 */

#include <ncbi_pch.hpp>

#include <connect/services/netservice_params.hpp>

BEGIN_NCBI_SCOPE


NCBI_PARAM_DEF(bool, netservice_api, use_linger2, false);
NCBI_PARAM_DEF(unsigned int, netservice_api, connection_max_retries, 10);
NCBI_PARAM_DEF(double, netservice_api, communication_timeout, 12.0);
NCBI_PARAM_DEF(int, netservice_api, max_find_lbname_retries, 3);
NCBI_PARAM_DEF(string, netcache_api, fallback_server, kEmptyStr);
NCBI_PARAM_DEF(string, netcache_client, fallback_servers, kEmptyStr);
NCBI_PARAM_DEF(int, netservice_api, max_connection_pool_size, 0); // unlimited


static bool s_DefaultCommTimeout_Initialized = false;
static STimeout s_DefaultCommTimeout;

STimeout s_GetDefaultCommTimeout()
{
    if (s_DefaultCommTimeout_Initialized)
        return s_DefaultCommTimeout;
    double ftm = TServConn_CommTimeout::GetDefault();
    NcbiMsToTimeout(&s_DefaultCommTimeout, (unsigned long)(ftm * 1000.0 + 0.5));
    s_DefaultCommTimeout_Initialized = true;
    return s_DefaultCommTimeout;
}

void s_SetDefaultCommTimeout(const STimeout& tm)
{
    s_DefaultCommTimeout = tm;
    s_DefaultCommTimeout_Initialized = true;
}


END_NCBI_SCOPE
