#ifndef UTIL___CACHE_ASYNC__HPP
#define UTIL___CACHE_ASYNC__HPP

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
 * Authors: Rafael Sadyrov
 *
 */

#include <corelib/ncbithr.hpp>

#include <util/cache/icache.hpp>
#include <util/thread_pool.hpp>

#include <memory>


BEGIN_NCBI_SCOPE


/// Class for deferred asynchronous writes in a separate thread.
///
/// @warning This class writes synchronously (immediately) in single thread builds
///
class NCBI_XUTIL_EXPORT CAsyncWriteCache : public ICache
{
public:

    /// Constructor
    ///
    /// @param main
    ///    Main ICache instance used for all but write/store operations
    /// @param writer
    ///    ICache instance used solely for write/store operations (in a separate thread, unless disabled)
    /// @param grace_period
    ///    Seconds to wait for the write thread before cancelling all its queued tasks.
    ///    Zero means cancelling all queued tasks immediately,
    ///    already executing task (if any) will try to finish.
    CAsyncWriteCache(ICache* main, ICache* writer, double grace_period = 0.0);

    /// ICache overrides.
    ///
    /// @sa ICache for details
    /// @{
    TFlags GetFlags() override;
    void SetFlags(TFlags flags) override;
    void SetTimeStampPolicy(TTimeStampFlags policy, unsigned int timeout, unsigned int max_timeout) override;
    TTimeStampFlags GetTimeStampPolicy() const override;
    int GetTimeout() const override;
    bool IsOpen() const override;
    void SetVersionRetention(EKeepVersions policy) override;
    EKeepVersions GetVersionRetention() const override;
    void Store(const string& key, TBlobVersion version, const string& subkey, const void* data, size_t size, unsigned int time_to_live = 0, const string& owner = kEmptyStr) override;
    size_t GetSize(const string& key, TBlobVersion version, const string& subkey) override;
    void GetBlobOwner(const string& key, TBlobVersion version, const string& subkey, string* owner) override;
    bool Read(const string& key, TBlobVersion version, const string& subkey, void* buf, size_t buf_size) override;
    IReader* GetReadStream(const string& key, TBlobVersion version, const string& subkey) override;
    IReader* GetReadStream(const string& key, const string& subkey, TBlobVersion* version, EBlobVersionValidity* validity) override;
    void SetBlobVersionAsCurrent(const string& key, const string& subkey, TBlobVersion version) override;
    void GetBlobAccess(const string& key, TBlobVersion version, const string& subkey, SBlobAccessDescr* blob_descr) override;
    IWriter* GetWriteStream(const string& key, TBlobVersion version, const string& subkey, unsigned int time_to_live = 0, const string& owner = kEmptyStr) override;
    void Remove(const string& key, TBlobVersion version, const string& subkey) override;
    time_t GetAccessTime(const string& key, TBlobVersion version, const string& subkey) override;
    bool HasBlobs(const string& key, const string& subkey) override;
    void Purge(time_t access_timeout) override;
    void Purge(const string& key, const string& subkey, time_t access_timeout) override;
    ~CAsyncWriteCache() override;
    bool SameCacheParams(const TCacheParams* params) const override;
    string GetCacheName(void) const override;
    /// @}

private:
    unique_ptr<ICache> m_Main;
    shared_ptr<ICache> m_Writer;
    shared_ptr<CThreadPool> m_ThreadPool;
    CTimeout m_GracePeriod;
};


END_NCBI_SCOPE

#endif
