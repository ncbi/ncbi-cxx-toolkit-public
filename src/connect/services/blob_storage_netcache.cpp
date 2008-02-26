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
 * Authors:  Maxim Didenko
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/rwstream.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>
#include <connect/services/blob_storage_netcache.hpp>

#include "blob_storage_netcache_impl.hpp"

BEGIN_NCBI_SCOPE

//////////////////////////////////////////////////////////////////////////////
//


typedef CBlobStorage_NetCache_Impl<CNetCacheAPI> TBSNCPersitent;
typedef CBlobStorage_NetCache_Impl<CNetCacheClient> TBSNCSimple;

CBlobStorage_NetCache::CBlobStorage_NetCache(CNetCacheAPI* nc_client,
                                       TCacheFlags flags,
                                       const string&  temp_dir)
    : m_Impl(new TBSNCPersitent(nc_client, (TBSNCPersitent::TCacheFlags)flags, temp_dir) )
{
}
CBlobStorage_NetCache::CBlobStorage_NetCache(CNetCacheClient* nc_client,
                                       TCacheFlags flags,
                                       const string&  temp_dir)
    : m_Impl(new TBSNCSimple(nc_client, (TBSNCSimple::TCacheFlags)flags, temp_dir) )
{
}


CBlobStorage_NetCache::CBlobStorage_NetCache()
{
    NCBI_THROW(CException, eInvalid,
               "Can not create an empty blob storage.");
}


CBlobStorage_NetCache::~CBlobStorage_NetCache()
{
}

bool CBlobStorage_NetCache::IsKeyValid(const string& str)
{
    return m_Impl->IsKeyValid(str);
}

CNcbiIstream& CBlobStorage_NetCache::GetIStream(const string& key,
                                             size_t* blob_size,
                                             ELockMode lockMode)
{
    return m_Impl->GetIStream(key, blob_size, lockMode);
}

string CBlobStorage_NetCache::GetBlobAsString(const string& data_id)
{
    return m_Impl->GetBlobAsString(data_id);
}

CNcbiOstream& CBlobStorage_NetCache::CreateOStream(string& key,
                                                   ELockMode lockMode)

{
    return m_Impl->CreateOStream(key, lockMode);
}

string CBlobStorage_NetCache::CreateEmptyBlob()
{
    return m_Impl->CreateEmptyBlob();
}

void CBlobStorage_NetCache::DeleteBlob(const string& data_id)
{
    m_Impl->DeleteBlob(data_id);
}

void CBlobStorage_NetCache::Reset()
{
    m_Impl->Reset();
}

//////////////////////////////////////////////////////////////////////////////
//


const char* kBlobStorageNetCacheDriverName = "netcache";


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

    virtual
    IBlobStorage* CreateInstance(
                   const string&    driver  = kEmptyStr,
                   CVersionInfo     version = NCBI_INTERFACE_VERSION(IBlobStorage),
                   const TPluginManagerParamTree* params = 0) const;
};

IBlobStorage* CBlobStorageNetCacheCF::CreateInstance(
           const string&                  driver,
           CVersionInfo                   version,
           const TPluginManagerParamTree* params) const
{
    if (driver.empty() || driver == m_DriverName) {
        if (version.Match(NCBI_INTERFACE_VERSION(IBlobStorage))
                            != CVersionInfo::eNonCompatible && params) {

            string temp_dir = GetParam(params, "tmp_dir", false, kEmptyStr);
            if (temp_dir == kEmptyStr)
                temp_dir = GetParam(params, "tmp_path", false, ".");
            vector<string> masks;
            masks.push_back( TBSNCPersitent::sm_InputBlobCachePrefix + "*" );
            masks.push_back( TBSNCPersitent::sm_OutputBlobCachePrefix + "*" );
            CDir curr_dir(temp_dir);
            CDir::TEntries dir_entries = curr_dir.GetEntries(masks,
                                                             CDir::eIgnoreRecursive);
            ITERATE( CDir::TEntries, it, dir_entries) {
                (*it)->Remove(CDirEntry::eNonRecursive);
            }
            bool cache_input =
                GetParamBool(params, "cache_input", false, false);
            bool cache_output =
                GetParamBool(params, "cache_output", false, false);

            CBlobStorage_NetCache::TCacheFlags flags = 0;
            if (cache_input) flags |= CBlobStorage_NetCache::eCacheInput;
            if (cache_output) flags |= CBlobStorage_NetCache::eCacheOutput;

            string protocol = GetParam(params, "protocol", false, "simple");
            if (NStr::CompareNocase(protocol, "simple") == 0 ) {
                typedef CPluginManager<CNetCacheClient> TPMNetCache;
                TPMNetCache                      PM_NetCache;
                PM_NetCache.RegisterWithEntryPoint(NCBI_EntryPoint_xnetcache);
                auto_ptr<CNetCacheClient> nc_client (
                                PM_NetCache.CreateInstance(
                                                kNetCacheDriverName,
                                                TPMNetCache::GetDefaultDrvVers(),
                                                params)
                            );
                if (nc_client.get())
                    return new CBlobStorage_NetCache(nc_client.release(),
                                                 flags,
                                                 temp_dir);
                else return NULL;

            } else if (NStr::CompareNocase(protocol, "persistent") == 0 ) {

                typedef CPluginManager<CNetCacheAPI> TPMNetCache;
                TPMNetCache                      PM_NetCache;
                PM_NetCache.RegisterWithEntryPoint(NCBI_EntryPoint_xnetcacheapi);
                auto_ptr<CNetCacheAPI> nc_client (
                                PM_NetCache.CreateInstance(
                                                kNetCacheAPIDriverName,
                                                TPMNetCache::GetDefaultDrvVers(),
                                                params)
                            );
                if (nc_client.get())
                    return new CBlobStorage_NetCache(nc_client.release(),
                                                 flags,
                                                 temp_dir);
                else return NULL;
            } else {
                throw runtime_error("Unsupported protocol");
            }
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
    RegisterEntryPoint<IBlobStorage>( NCBI_EntryPoint_xblobstorage_netcache );
}

END_NCBI_SCOPE
