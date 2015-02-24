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
typedef pair<string, TNetStorageFlags> TKey;


#define APP_NAME                "test_netstorage"
#define SP_ST_LIST (DirectNetStorage, (DirectNetStorageByKey, BOOST_PP_NIL))

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
    return CDirectNetStorage(
            CNetICacheClient(nc_service.c_str(),
                    nst_app_domain.c_str(), APP_NAME),
            nst_app_domain);
}

template <>
inline CNetStorageByKey g_GetNetStorage<CNetStorageByKey>()
{
    string nc_service(TNetCache_ServiceName::GetDefault());
    string nst_app_domain(TNetStorage_AppDomain::GetDefault());
    return CDirectNetStorageByKey(
            CNetICacheClient(nc_service.c_str(),
                    nst_app_domain.c_str(), APP_NAME),
            nst_app_domain);
}

template <>
inline CDirectNetStorageByKey g_GetNetStorage<CDirectNetStorageByKey>()
{
    string nc_service(TNetCache_ServiceName::GetDefault());
    string nst_app_domain(TNetStorage_AppDomain::GetDefault());
    return CDirectNetStorageByKey(
            CNetICacheClient(nc_service.c_str(),
                    nst_app_domain.c_str(), APP_NAME),
            nst_app_domain);
}

inline void g_Sleep()
{
    SleepSec(2UL);
}


// Overloading is used to test specific API (CDirectNetStorage*)

inline string Relocate(CDirectNetStorage& netstorage, const string& object_loc,
        TNetStorageFlags flags)
{
    CDirectNetStorageObject object(netstorage.Open(object_loc));
    return object.Relocate(flags);
}

inline bool Exists(CDirectNetStorage& netstorage, const string& object_loc)
{
    CDirectNetStorageObject object(netstorage.Open(object_loc));
    return object.Exists();
}

inline void Remove(CDirectNetStorage& netstorage, const string& object_loc)
{
    CDirectNetStorageObject object(netstorage.Open(object_loc));
    object.Remove();
}

inline string Relocate(CDirectNetStorageByKey& netstorage, const string& unique_key,
        TNetStorageFlags flags, TNetStorageFlags old_flags)
{
    CDirectNetStorageObject object(netstorage.Open(unique_key, old_flags));
    return object.Relocate(flags);
}

inline bool Exists(CDirectNetStorageByKey& netstorage, const TKey& key)
{
    CDirectNetStorageObject object(netstorage.Open(key.first, key.second));
    return object.Exists();
}

inline void Remove(CDirectNetStorageByKey& netstorage, const TKey& key)
{
    CDirectNetStorageObject object(netstorage.Open(key.first, key.second));
    object.Remove();
}


END_NCBI_SCOPE

#endif
