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
 * File Description:  Simple test for the NetStorage API
 *      (implemented via RPC to NetStorage servers).
 *
 */

#include <boost/type_traits/integral_constant.hpp>

#include <corelib/ncbi_system.hpp>
#include <connect/services/netstorage.hpp>

BEGIN_NCBI_SCOPE


typedef boost::integral_constant<bool, true> TAttrTesting;
typedef pair<string, TNetStorageFlags> TKey;


#define APP_NAME                    "test_netstorage_rpc"
#define ST_ST_LIST      BOOST_PP_NIL
#define LOC_LOC_LIST    (DirectNC, BOOST_PP_NIL)

NCBI_PARAM_DECL(string, netstorage, service_name);
typedef NCBI_PARAM_TYPE(netstorage, service_name) TNetStorage_ServiceName;

NCBI_PARAM_DECL(string, netcache, service_name);
typedef NCBI_PARAM_TYPE(netcache, service_name) TNetCache_ServiceName;

NCBI_PARAM_DECL(string, netstorage, app_domain);
typedef NCBI_PARAM_TYPE(netstorage, app_domain) TNetStorage_AppDomain;

struct SDirectNC
{
    static const TNetStorageFlags source = 0;

    static const TNetStorageFlags non_existent = 0;

    typedef boost::integral_constant<ENetStorageObjectLocation, eNFL_NetCache>
        TCreate;

    static const TNetStorageFlags create = 0;

    static const TNetStorageFlags immovable = 0;

    static const TNetStorageFlags readable = 0;

    typedef TCreate TRelocate;
    static const TNetStorageFlags relocate = 0;

    typedef boost::integral_constant<bool, false> TAttrTesting;

    static const bool check_relocate = false;
    static const bool loc_info = false;

    static const char* init_string() { return "&default_storage=nc"; }

    static const char* not_found()
    {
        return "o-c8TRwX3VuFSZnf5BuKLelYjHcUK5xKpizKXbIX8EmNDNle";
    }
};


template <class TNetStorage>
inline TNetStorage g_GetNetStorage(const char* mode)
{
    string nst_service(TNetStorage_ServiceName::GetDefault());
    string nc_service(TNetCache_ServiceName::GetDefault());
    string nst_app_domain(TNetStorage_AppDomain::GetDefault());
    string init_string(
            "nst="     + nst_service +
            "&nc="     + nc_service +
            "&domain=" + nst_app_domain +
            "&client="   APP_NAME +
            mode);
    return CNetStorage(init_string);
}

template <>
inline CNetStorageByKey g_GetNetStorage<CNetStorageByKey>(const char* mode)
{
    string nst_service(TNetStorage_ServiceName::GetDefault());
    string nc_service(TNetCache_ServiceName::GetDefault());
    string nst_app_domain(TNetStorage_AppDomain::GetDefault());
    string init_string(
            "nst="     + nst_service +
            "&nc="     + nc_service +
            "&domain=" + nst_app_domain +
            "&client="   APP_NAME +
            mode);
    return CNetStorageByKey(init_string);
}

inline void g_Sleep()
{
    SleepSec(1UL);
}

END_NCBI_SCOPE

#endif
