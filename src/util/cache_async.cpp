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
 
#include <ncbi_pch.hpp>

#include <corelib/ncbistre.hpp>
#include <corelib/request_ctx.hpp>
#include <corelib/rwstream.hpp>
#include <corelib/stream_utils.hpp>

#include <util/cache/cache_async.hpp>

#include <sstream>


BEGIN_NCBI_SCOPE


// If set to false, this parameter globally disables asynchronous writes for CAsyncWriteCache instances
// (enabling synchronous writes instead).
NCBI_PARAM_DECL(bool, ncbi, cache_async_write);
NCBI_PARAM_DEF(bool, ncbi, cache_async_write, true);
typedef NCBI_PARAM_TYPE(ncbi, cache_async_write) TCacheWriteAsync;


struct SMeta
{
    string key;
    ICache::TBlobVersion version;
    string subkey;
    unsigned time_to_live;
    string owner;
    CRef<CRequestContext> ctx;
};


struct SAsyncWriteTask : CThreadPool_Task
{
    stringstream data;

    SAsyncWriteTask(weak_ptr<ICache> cache, SMeta meta);
    EStatus Execute(void) override;

private:
    weak_ptr<ICache> m_Cache;
    SMeta m_Meta;
};


struct SDeferredExecutor
{
    CRef<SAsyncWriteTask> task;

    SDeferredExecutor(weak_ptr<CThreadPool> thread_pool, weak_ptr<ICache> cache, SMeta meta);
    ~SDeferredExecutor();

private:
    weak_ptr<CThreadPool> m_ThreadPool;
};


struct SDeferredWriter : SDeferredExecutor, CStreamWriter
{
    SDeferredWriter(weak_ptr<CThreadPool> thread_pool, weak_ptr<ICache> cache, SMeta meta);
};


SAsyncWriteTask::SAsyncWriteTask(weak_ptr<ICache> cache, SMeta meta) :
    m_Cache(cache),
    m_Meta(meta)
{
}


SAsyncWriteTask::EStatus SAsyncWriteTask::Execute(void)
{
    auto cache = m_Cache.lock();
    if (!cache) return eCanceled;

    auto& m = m_Meta; // Shortcut
    GetDiagContext().SetRequestContext(m.ctx);
    auto writer = cache->GetWriteStream(m.key, m.version, m.subkey, m.time_to_live, m.owner);

    CWStream os(writer, 0, 0, CRWStreambuf::fOwnWriter);

    NcbiStreamCopy(os, data);
    return eCompleted;
}


SDeferredExecutor::SDeferredExecutor(weak_ptr<CThreadPool> thread_pool, weak_ptr<ICache> cache, SMeta meta) :
    task(new SAsyncWriteTask(cache, meta)),
    m_ThreadPool(thread_pool)
{
}


SDeferredExecutor::~SDeferredExecutor()
{
    if (auto p = m_ThreadPool.lock()) p->AddTask(task.Release());
}


SDeferredWriter::SDeferredWriter(weak_ptr<CThreadPool> thread_pool, weak_ptr<ICache> cache, SMeta meta) :
    SDeferredExecutor(thread_pool, cache, meta),
    CStreamWriter(task->data)
{
}


CThreadPool* s_CreateThreadPool()
{
#ifdef NCBI_THREADS
    if (TCacheWriteAsync::GetDefault()) {
        // XXX: Thread pool can only use single thread, as writer would be shared between threads otherwise.
        return new CThreadPool(numeric_limits<unsigned>::max(), 1, 1);
    }
#endif

    return nullptr;
}

CAsyncWriteCache::CAsyncWriteCache(ICache* main, ICache* writer, double grace_period) :
    m_Main(main),
    m_Writer(writer),
    m_ThreadPool(s_CreateThreadPool()),
    m_GracePeriod(max(grace_period, 0.0))
{
    _ASSERT(main);
    _ASSERT(writer);
    _ASSERT(main != writer);
}


CAsyncWriteCache::~CAsyncWriteCache()
{
    if (!m_ThreadPool) return;

    CDeadline deadline(m_GracePeriod);

    while (m_ThreadPool->GetQueuedTasksCount() && !deadline.IsExpired()) {
        auto sleep = min(deadline.GetRemainingTime().GetAsMilliSeconds(), 100UL);
        SleepMilliSec(sleep);
    }
}


ICache::TFlags CAsyncWriteCache::GetFlags()
{
    return m_Main->GetFlags();
}


void CAsyncWriteCache::SetFlags(TFlags flags)
{
    return m_Main->SetFlags(flags);
}


void CAsyncWriteCache::SetTimeStampPolicy(TTimeStampFlags policy, unsigned int timeout, unsigned int max_timeout)
{
    return m_Main->SetTimeStampPolicy(policy, timeout, max_timeout);
}


ICache::TTimeStampFlags CAsyncWriteCache::GetTimeStampPolicy() const
{
    return m_Main->GetTimeStampPolicy();
}


int CAsyncWriteCache::GetTimeout() const
{
    return m_Main->GetTimeout();
}


bool CAsyncWriteCache::IsOpen() const
{
    return m_Main->IsOpen();
}


void CAsyncWriteCache::SetVersionRetention(EKeepVersions policy)
{
    return m_Main->SetVersionRetention(policy);
}


ICache::EKeepVersions CAsyncWriteCache::GetVersionRetention() const
{
    return m_Main->GetVersionRetention();
}


void CAsyncWriteCache::Store(const string& key, TBlobVersion version, const string& subkey, const void* data, size_t size, unsigned int time_to_live, const string& owner)
{
    unique_ptr<IWriter> writer(GetWriteStream(key, version, subkey, time_to_live, owner));

    size_t bytes_written;

    while (size > 0) {
        writer->Write(data, size, &bytes_written);
        data = static_cast<const char*>(data) + bytes_written;
        size -= bytes_written;
    }
}


size_t CAsyncWriteCache::GetSize(const string& key, TBlobVersion version, const string& subkey)
{
    return m_Main->GetSize(key, version, subkey);
}


void CAsyncWriteCache::GetBlobOwner(const string& key, TBlobVersion version, const string& subkey, string* owner)
{
    return m_Main->GetBlobOwner(key, version, subkey, owner);
}


bool CAsyncWriteCache::Read(const string& key, TBlobVersion version, const string& subkey, void* buf, size_t buf_size)
{
    return m_Main->Read(key, version, subkey, buf, buf_size);
}


IReader* CAsyncWriteCache::GetReadStream(const string& key, TBlobVersion version, const string& subkey)
{
    return m_Main->GetReadStream(key, version, subkey);
}


IReader* CAsyncWriteCache::GetReadStream(const string& key, const string& subkey, TBlobVersion* version, EBlobVersionValidity* validity)
{
    return m_Main->GetReadStream(key, subkey, version, validity);
}


void CAsyncWriteCache::SetBlobVersionAsCurrent(const string& key, const string& subkey, TBlobVersion version)
{
    return m_Main->SetBlobVersionAsCurrent(key, subkey, version);
}


void CAsyncWriteCache::GetBlobAccess(const string& key, TBlobVersion version, const string& subkey, SBlobAccessDescr* blob_descr)
{
    return m_Main->GetBlobAccess(key, version, subkey, blob_descr);
}


IWriter* CAsyncWriteCache::GetWriteStream(const string& key, TBlobVersion version, const string& subkey, unsigned int time_to_live, const string& owner)
{
    if (m_ThreadPool) {
        auto ctx = GetDiagContext().GetRequestContext().Clone();
        SMeta meta{key, version, subkey, time_to_live, owner, ctx};
        return new SDeferredWriter(m_ThreadPool, m_Writer, move(meta));
    }

    return m_Writer->GetWriteStream(key, version, subkey, time_to_live, owner);
}


void CAsyncWriteCache::Remove(const string& key, TBlobVersion version, const string& subkey)
{
    return m_Main->Remove(key, version, subkey);
}


time_t CAsyncWriteCache::GetAccessTime(const string& key, TBlobVersion version, const string& subkey)
{
    return m_Main->GetAccessTime(key, version, subkey);
}


bool CAsyncWriteCache::HasBlobs(const string& key, const string& subkey)
{
    return m_Main->HasBlobs(key, subkey);
}


void CAsyncWriteCache::Purge(time_t access_timeout)
{
    return m_Main->Purge(access_timeout);
}


void CAsyncWriteCache::Purge(const string& key, const string& subkey, time_t access_timeout)
{
    return m_Main->Purge(key, subkey, access_timeout);
}


bool CAsyncWriteCache::SameCacheParams(const TCacheParams* params) const
{
    return m_Main->SameCacheParams(params);
}


string CAsyncWriteCache::GetCacheName(void) const
{
    return m_Main->GetCacheName();
}


END_NCBI_SCOPE
