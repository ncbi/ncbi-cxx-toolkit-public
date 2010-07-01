#ifndef CONNECT_SERVICES___NETICACHE_CLIENT__HPP
#define CONNECT_SERVICES___NETICACHE_CLIENT__HPP

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
 * Authors:  Anatoliy Kuznetsov, Dmitry Kazimirov
 *
 * File Description:
 *   Network client for ICache (NetCache).
 *
 */

/// @file neticache_client.hpp
/// NetCache ICache client specs.
///

#include <connect/services/netcache_api.hpp>

#include <connect/ncbi_core_cxx.hpp>

#include <util/resource_pool.hpp>
#include <util/cache/icache.hpp>

#include <corelib/request_control.hpp>
#include <corelib/plugin_manager_store.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbimtx.hpp>


BEGIN_NCBI_SCOPE


/** @addtogroup NetCacheClient
 *
 * @{
 */

struct SNetICacheClientImpl;

/// Client to NetCache server (implements ICache interface)
///
/// @note This implementation is thread safe and synchronized
///
class NCBI_NET_CACHE_EXPORT CNetICacheClient : public ICache
{
    NCBI_NET_COMPONENT_WITH_DEFAULT_CTOR(NetICacheClient);

    // Construct an instance using connection parameters
    // specified in the configuration file.
    CNetICacheClient(CConfig* config = NULL,
                     const string& driver_name = kEmptyStr);

    CNetICacheClient(const string& host,
                     unsigned short port,
                     const string& cache_name,
                     const string& client_name);

    CNetICacheClient(const string& service_name,
                     const string& cache_name,
                     const string& client_name);

    /// This constructor will be retired in favor of the
    /// constructor that accepts the CConfig parameter.
    NCBI_DEPRECATED
    CNetICacheClient(const string& service_name,
                     const string& cache_name,
                     const string& client_name,
                     const string& lbsm_affinity_name);

    /// Send session registration command
    void RegisterSession(unsigned pid);
    /// Send session unregistration command
    void UnRegisterSession(unsigned pid);

    // ICache interface implementation

    virtual void SetTimeStampPolicy(TTimeStampFlags policy,
                                    unsigned int    timeout,
                                    unsigned int    max_timeout = 0);
    virtual TTimeStampFlags GetTimeStampPolicy() const;
    virtual int GetTimeout() const;
    virtual bool IsOpen() const;
    virtual void SetVersionRetention(EKeepVersions policy);
    virtual EKeepVersions GetVersionRetention() const;
    virtual void Store(const string&  key,
                       int            version,
                       const string&  subkey,
                       const void*    data,
                       size_t         size,
                       unsigned int   time_to_live = 0,
                       const string&  owner = kEmptyStr);
    virtual size_t GetSize(const string&  key,
                           int            version,
                           const string&  subkey);
    virtual void GetBlobOwner(const string&  key,
                              int            version,
                              const string&  subkey,
                              string*        owner);
    virtual bool Read(const string& key,
                      int           version,
                      const string& subkey,
                      void*         buf,
                      size_t        buf_size);

    virtual IReader* GetReadStream(
        const string& key,
        int version,
        const string& subkey,
        size_t* blob_size_ptr,
        CNetCacheAPI::ECachingMode caching_mode);

    virtual IReader* GetReadStream(const string&  key,
                                   int            version,
                                   const string&  subkey);

    virtual void GetBlobAccess(const string&     key,
                               int               version,
                               const string&     subkey,
                               SBlobAccessDescr*  blob_descr);

    IEmbeddedStreamWriter* GetNetCacheWriter(
        const string& key,
        int version,
        const string& subkey,
        unsigned int time_to_live = 0,
        const string& owner = kEmptyStr,
        CNetCacheAPI::ECachingMode caching_mode =
            CNetCacheAPI::eCaching_AppDefault);

    virtual IWriter* GetWriteStream(
        const string& key,
        int version,
        const string& subkey,
        unsigned int time_to_live = 0,
        const string& owner = kEmptyStr);

    virtual void Remove(const string& key);
    virtual void Remove(const string&    key,
                        int              version,
                        const string&    subkey);
    virtual time_t GetAccessTime(const string&  key,
                                 int            version,
                                 const string&  subkey);
    virtual bool HasBlobs(const string&  key,
                          const string&  subkey);
    virtual void Purge(time_t           access_timeout,
                       EKeepVersions    keep_last_version = eDropAll);

    virtual void Purge(const string&    key,
                       const string&    subkey,
                       time_t           access_timeout,
                       EKeepVersions    keep_last_version = eDropAll);

    virtual bool SameCacheParams(const TCacheParams* params) const;
    virtual string GetCacheName(void) const;

    /// Set communication timeout
    void SetCommunicationTimeout(const STimeout& to);
    STimeout  GetCommunicationTimeout() const;
};

class NCBI_NET_CACHE_EXPORT CNetICachePasswordGuard
{
public:
    CNetICachePasswordGuard(CNetICacheClient::TInstance ic_client,
        const string& password);

    CNetICacheClient* operator ->() {return &m_NetICacheClient;}

private:
    CNetICacheClient m_NetICacheClient;
};

extern NCBI_NET_CACHE_EXPORT const char* const kNetICacheDriverName;

extern "C"
{

NCBI_NET_CACHE_EXPORT
void NCBI_EntryPoint_xcache_netcache(
     CPluginManager<ICache>::TDriverInfoList&   info_list,
     CPluginManager<ICache>::EEntryPointRequest method);

NCBI_NET_CACHE_EXPORT
void Cache_RegisterDriver_NetCache(void);

} // extern C

/* @} */


END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES___NETICACHE_CLIENT__HPP */
