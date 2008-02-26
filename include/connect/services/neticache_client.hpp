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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description:
 *   Network client for ICache (NetCache).
 *
 */

/// @file neticache_client.hpp
/// NetCache ICache client specs.
///

#include <corelib/plugin_manager_store.hpp>
#include <connect/services/netcache_client.hpp>
#include <util/request_control.hpp>
#include <util/cache/icache.hpp>



BEGIN_NCBI_SCOPE


/** @addtogroup NetCacheClient
 *
 * @{
 */

/// Client to NetCache server (implements ICache interface)
///
/// @note This implementation is thread safe and syncronized
///
class NCBI_NET_CACHE_EXPORT CNetICacheClient : public CNetCacheClientBase,
                                               public ICache
{
public:
    typedef CNetCacheClientBase TParent;
public:
    CNetICacheClient();
    CNetICacheClient(const string&  host,
                     unsigned short port,
                     const string&  cache_name,
                     const string&  client_name);
    virtual ~CNetICacheClient();

    void SetConnectionParams(const string&  host,
                             unsigned short port,
                             const string&  cache_name,
                             const string&  client_name);

	virtual
    void ReturnSocket(CSocket* sock, const string& blob_comments);

    void RegisterSession(unsigned pid);
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
    virtual IReader* GetReadStream(const string&  key,
                                   int            version,
                                   const string&  subkey);

    virtual void GetBlobAccess(const string&     key,
                               int               version,
                               const string&     subkey,
                               SBlobAccessDescr*  blob_descr);
    virtual IWriter* GetWriteStream(const string&    key,
                                    int              version,
                                    const string&    subkey,
                                    unsigned int     time_to_live = 0,
                                    const string&    owner = kEmptyStr);
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

protected:
    /// Connect to server
    /// Function returns true if connection has been re-established
    /// flase if connection has been established before and
    /// throws an exception if it cannot establish connection
    bool CheckConnect();

    void MakeCommandPacket(string* out_str,
                           const string& cmd_str,
                           bool          connected) const;
    void AddKVS(string*          out_str,
                const string&    key,
                int              version,
                const string&    subkey) const;

    virtual
    void TrimPrefix(string* str) const;
    virtual
    void CheckOK(string* str) const;

    IReader* GetReadStream_NoLock(const string&  key,
                                  int            version,
                                  const string&  subkey);

private:
    CNetICacheClient(const CNetICacheClient&);
    CNetICacheClient& operator=(const CNetICacheClient&);

protected:
    string              m_CacheName;
    size_t              m_BlobSize;
    CRequestRateControl m_Throttler;
    mutable CFastMutex  m_Lock;     ///< Client access lock
};


extern NCBI_NET_CACHE_EXPORT const char* kNetICacheDriverName;

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
