#ifndef TEST_NETSTORAGE_COMMON__HPP
#define TEST_NETSTORAGE_COMMON__HPP

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
 * Authors:  Dmitry Kazimirov, Rafael Sadyrov
 *
 * File Description:  Simple test for the "direct access" NetStorage API.
 *
 */

#include <boost/type_traits/integral_constant.hpp>

#include <corelib/ncbi_system.hpp>
#include <misc/netstorage/netstorage.hpp>

BEGIN_NCBI_SCOPE


typedef boost::integral_constant<bool, false> TAttrTesting;


#define APP_NAME                "test_netstorage"

// NB: This parameter is not used here, but required for compilation
NCBI_PARAM_DECL(string, netstorage, service_name);
typedef NCBI_PARAM_TYPE(netstorage, service_name) TNetStorage_ServiceName;

NCBI_PARAM_DECL(string, netcache, service_name);
typedef NCBI_PARAM_TYPE(netcache, service_name) TNetCache_ServiceName;

NCBI_PARAM_DECL(string, netstorage, app_domain);
typedef NCBI_PARAM_TYPE(netstorage, app_domain) TNetStorage_AppDomain;


template <class TNetStorage>
inline TNetStorage g_GetNetStorage()
{
    string nc_service(TNetCache_ServiceName::GetDefault());
    string nst_app_domain(TNetStorage_AppDomain::GetDefault());
    return g_CreateNetStorage(
            CNetICacheClient(nc_service.c_str(),
                    nst_app_domain.c_str(), APP_NAME),
            nst_app_domain);
}

template <>
inline CNetStorageByKey g_GetNetStorage<CNetStorageByKey>()
{
    string nc_service(TNetCache_ServiceName::GetDefault());
    string nst_app_domain(TNetStorage_AppDomain::GetDefault());
    return g_CreateNetStorageByKey(
            CNetICacheClient(nc_service.c_str(),
                    nst_app_domain.c_str(), APP_NAME),
            nst_app_domain);
}

inline void g_Sleep()
{
    SleepSec(2UL);
}

END_NCBI_SCOPE

#endif
