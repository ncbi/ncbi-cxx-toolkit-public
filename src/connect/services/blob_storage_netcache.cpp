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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Maxim Didenko, Dmitry Kazimirov
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>

#include <connect/services/blob_storage_netcache.hpp>

#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>


BEGIN_NCBI_SCOPE


//////////////////////////////////////////////////////////////////////////////
//

const char* const kBlobStorageNetCacheDriverName = "netcache";


class CBlobStorageNetCacheCF
    : public CSimpleClassFactoryImpl<IBlobStorage,CBlobStorage_NetCache>
{
public:
    typedef CSimpleClassFactoryImpl<IBlobStorage,
        CBlobStorage_NetCache> TParent;

public:
    CBlobStorageNetCacheCF() : TParent(kBlobStorageNetCacheDriverName, 0)
    {
    }

    virtual ~CBlobStorageNetCacheCF()
    {
    }

    virtual IBlobStorage* CreateInstance(
        const string& driver  = kEmptyStr,
        CVersionInfo version = NCBI_INTERFACE_VERSION(IBlobStorage),
        const TPluginManagerParamTree* params = 0) const;
};

IBlobStorage* CBlobStorageNetCacheCF::CreateInstance(
    const string& driver,
    CVersionInfo version,
    const TPluginManagerParamTree* params) const
{
    if (driver.empty() || driver == m_DriverName) {
        if (version.Match(NCBI_INTERFACE_VERSION(IBlobStorage))
            != CVersionInfo::eNonCompatible && params) {

            CConfig config(params);

            CNetCacheAPI nc_client(&config, kNetCacheAPIDriverName);

            return new CBlobStorage_NetCache(nc_client);
        }
    }
    return 0;
}

void NCBI_EntryPoint_xblobstorage_netcache(
     CPluginManager<IBlobStorage>::TDriverInfoList&   info_list,
     CPluginManager<IBlobStorage>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CBlobStorageNetCacheCF>
        ::NCBI_EntryPointImpl(info_list, method);
}

void BlobStorage_RegisterDriver_NetCache(void)
{
    RegisterEntryPoint<IBlobStorage>(NCBI_EntryPoint_xblobstorage_netcache);
}

END_NCBI_SCOPE
