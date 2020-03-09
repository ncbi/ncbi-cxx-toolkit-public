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
 * Author: Eugene Vasilchenko, Aleksey Grichenko
 *
 * File Description: PSG data loader
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbi_param.hpp>
#include <corelib/plugin_manager_store.hpp>
#include <objects/seqsplit/ID2S_Split_Info.hpp>
#include <objects/seqsplit/ID2S_Chunk.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/tse_loadlock.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/tse_split_info.hpp>
#include <objmgr/impl/split_parser.hpp>
#include <objmgr/data_loader_factory.hpp>
#include <objmgr/annot_selector.hpp>
#include <objtools/data_loaders/genbank/impl/psg_loader_impl.hpp>
#include <util/compress/compress.hpp>
#include <util/compress/stream.hpp>
#include <util/compress/zlib.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>
#include <util/thread_pool.hpp>


#if defined(HAVE_PSG_LOADER)

BEGIN_NCBI_SCOPE

//#define NCBI_USE_ERRCODE_X   PSGLoader
//NCBI_DEFINE_ERR_SUBCODE_X(1);

BEGIN_SCOPE(objects)


/////////////////////////////////////////////////////////////////////////////
// CPsgClientContext
/////////////////////////////////////////////////////////////////////////////

class CPsgClientContext
{
public:
    CPsgClientContext(void);
    virtual ~CPsgClientContext(void) {}

    virtual void SetReply(shared_ptr<CPSG_Reply> reply);
    virtual shared_ptr<CPSG_Reply> GetReply(void);

protected:
    CSemaphore m_Sema;
private:
    shared_ptr<CPSG_Reply> m_Reply;
};


CPsgClientContext::CPsgClientContext(void)
    : m_Sema(0, kMax_UInt)
{
}


void CPsgClientContext::SetReply(shared_ptr<CPSG_Reply> reply)
{
    m_Reply = reply;
    m_Sema.Post();
}


shared_ptr<CPSG_Reply> CPsgClientContext::GetReply(void)
{
    m_Sema.Wait();
    return m_Reply;
}


class CPsgClientContext_Bulk : public CPsgClientContext
{
public:
    CPsgClientContext_Bulk(void) {}
    virtual ~CPsgClientContext_Bulk(void) {}

    void SetReply(shared_ptr<CPSG_Reply> reply) override;
    shared_ptr<CPSG_Reply> GetReply(void) override;

private:
    deque<shared_ptr<CPSG_Reply>> m_Replies;
    CFastMutex m_Lock;
};


void CPsgClientContext_Bulk::SetReply(shared_ptr<CPSG_Reply> reply)
{
    CFastMutexGuard guard(m_Lock);
    m_Replies.push_front(reply);
    m_Sema.Post();
}


shared_ptr<CPSG_Reply> CPsgClientContext_Bulk::GetReply(void)
{
    m_Sema.Wait();
    shared_ptr<CPSG_Reply> ret;
    CFastMutexGuard guard(m_Lock);
    _ASSERT(!m_Replies.empty());
    ret = m_Replies.back();
    m_Replies.pop_back();
    return ret;
}


/////////////////////////////////////////////////////////////////////////////
// CPsgClientThread
/////////////////////////////////////////////////////////////////////////////

class CPsgClientThread : public CThread
{
public:
    CPsgClientThread(shared_ptr<CPSG_Queue> queue) : m_Queue(queue), m_WakeSema(0, kMax_UInt) {}

    void Stop(void)
    {
        m_Stop = true;
        Wake();
    }

    void Wake()
    {
        m_WakeSema.Post();
    }

protected:
    void* Main(void) override;

private:
    bool m_Stop = false;
    shared_ptr<CPSG_Queue> m_Queue;
    CSemaphore m_WakeSema;
};


const unsigned int kMaxWaitSeconds = 3;
const unsigned int kMaxWaitMillisec = 0;

#define DEFAULT_DEADLINE CDeadline(kMaxWaitSeconds, kMaxWaitMillisec)

void* CPsgClientThread::Main(void)
{
    for (;;) {
        m_WakeSema.Wait();
        if (m_Stop) break;
        shared_ptr<CPSG_Reply> reply;
        do {
            reply = m_Queue->GetNextReply(DEFAULT_DEADLINE);
        }
        while (!reply && !m_Stop);
        if (m_Stop) break;
        auto context = reply->GetRequest()->GetUserContext<CPsgClientContext>();
        context->SetReply(reply);
    }
    return nullptr;
}


/////////////////////////////////////////////////////////////////////////////
// CPSGBioseqCache
/////////////////////////////////////////////////////////////////////////////


CSeq_id_Handle PsgIdToHandle(const CPSG_BioId& id)
{
    string sid = id.Get();
    if (sid.empty()) return CSeq_id_Handle();
    return CSeq_id_Handle::GetHandle(sid);
}


const int kMaxCacheLifespanSeconds = 300;
const size_t kMaxCacheSize = 10000;


class CPSGBioseqCache
{
public:
    CPSGBioseqCache(void) {}
    ~CPSGBioseqCache(void) {}

    shared_ptr<SPsgBioseqInfo> Get(const CSeq_id_Handle& idh);
    shared_ptr<SPsgBioseqInfo> Add(const CPSG_BioseqInfo& info, CSeq_id_Handle req_idh);

private:
    typedef map<CSeq_id_Handle, shared_ptr<SPsgBioseqInfo> > TIdMap;
    typedef list<shared_ptr<SPsgBioseqInfo> > TInfoQueue;

    mutable CFastMutex m_Mutex;
    size_t m_MaxSize = kMaxCacheSize;
    TIdMap m_Ids;
    TInfoQueue m_Infos;
};


shared_ptr<SPsgBioseqInfo> CPSGBioseqCache::Get(const CSeq_id_Handle& idh)
{
    CFastMutexGuard guard(m_Mutex);
    auto found = m_Ids.find(idh);
    if (found == m_Ids.end()) return nullptr;
    shared_ptr<SPsgBioseqInfo> ret = found->second;
    m_Infos.remove(ret);
    ret->deadline = CDeadline(kMaxCacheLifespanSeconds);
    m_Infos.push_back(ret);
    return ret;
}


shared_ptr<SPsgBioseqInfo> CPSGBioseqCache::Add(const CPSG_BioseqInfo& info, CSeq_id_Handle req_idh)
{
    CSeq_id_Handle idh = PsgIdToHandle(info.GetCanonicalId());
    if (!idh) return nullptr;
    // Try to find an existing entry (though this should not be a common case).
    CFastMutexGuard guard(m_Mutex);
    auto found = m_Ids.find(idh);
    if (found != m_Ids.end()) return found->second;
    // Create new entry.
    shared_ptr<SPsgBioseqInfo> ret = make_shared<SPsgBioseqInfo>(info);
    while (!m_Infos.empty() && (m_Infos.size() > m_MaxSize || m_Infos.front()->deadline.IsExpired())) {
        auto rm = m_Infos.front();
        m_Infos.pop_front();
        ITERATE(SPsgBioseqInfo::TIds, id, rm->ids) {
            m_Ids.erase(*id);
        }
    }
    m_Infos.push_back(ret);
    if (req_idh) {
        m_Ids[req_idh] = ret;
    }
    ITERATE(SPsgBioseqInfo::TIds, it, ret->ids) {
        m_Ids[*it] = ret;
    }
    return ret;
}


/////////////////////////////////////////////////////////////////////////////
// CPSGBlobMap
/////////////////////////////////////////////////////////////////////////////


class CPSGBlobMap
{
public:
    CPSGBlobMap(void) {}
    ~CPSGBlobMap(void) {}

    shared_ptr<SPsgBlobInfo> FindBlob(const string& bid) {
        CFastMutexGuard guard(m_Mutex);
        auto found = m_Blobs.find(bid);
        return found != m_Blobs.end() ? found->second : nullptr;
    }

    void AddBlob(const string& bid, shared_ptr<SPsgBlobInfo> blob) {
        CFastMutexGuard guard(m_Mutex);
        m_Blobs[bid] = blob;
    }

    void DropBlob(const CPsgBlobId& blob_id) {
        CFastMutexGuard guard(m_Mutex);
        auto blob_it = m_Blobs.find(blob_id.ToPsgId());
        if (blob_it != m_Blobs.end()) {
            m_Blobs.erase(blob_it);
        }
    }

private:
    // Map blob-id to blob info
    typedef map<string, shared_ptr<SPsgBlobInfo> > TBlobs;

    CFastMutex m_Mutex;
    TBlobs m_Blobs;
};


/////////////////////////////////////////////////////////////////////////////
// SPsgBioseqInfo
/////////////////////////////////////////////////////////////////////////////


SPsgBioseqInfo::SPsgBioseqInfo(const CPSG_BioseqInfo& bioseq_info)
    : molecule_type(CSeq_inst::eMol_not_set),
      length(0),
      state(0),
      tax_id(0),
      hash(0),
      deadline(kMaxCacheLifespanSeconds)
{
    CPSG_Request_Resolve::TIncludeInfo inc_info = bioseq_info.IncludedInfo();
    if (inc_info & CPSG_Request_Resolve::fMoleculeType)
        molecule_type = bioseq_info.GetMoleculeType();

    if (inc_info & CPSG_Request_Resolve::fLength)
        length = bioseq_info.GetLength();

    if (inc_info & CPSG_Request_Resolve::fState)
        state = bioseq_info.GetState();

    if (inc_info & CPSG_Request_Resolve::fTaxId)
        tax_id = bioseq_info.GetTaxId();

    if (inc_info & CPSG_Request_Resolve::fHash)
        hash = bioseq_info.GetHash();

    if (inc_info & CPSG_Request_Resolve::fCanonicalId) {
        canonical = PsgIdToHandle(bioseq_info.GetCanonicalId());
        ids.push_back(canonical);
    }
    if (inc_info & CPSG_Request_Resolve::fGi)
        gi = bioseq_info.GetGi();

    if (inc_info & CPSG_Request_Resolve::fOtherIds) {
        vector<CPSG_BioId> other_ids = bioseq_info.GetOtherIds();
        ITERATE(vector<CPSG_BioId>, other_id, other_ids) {
            ids.push_back(PsgIdToHandle(*other_id));
        }
    }
    if (inc_info & CPSG_Request_Resolve::fBlobId)
blob_id = bioseq_info.GetBlobId().Get();
}


/////////////////////////////////////////////////////////////////////////////
// SPsgBlobInfo
/////////////////////////////////////////////////////////////////////////////


SPsgBlobInfo::SPsgBlobInfo(const CPSG_BlobInfo& blob_info)
    : blob_state(0), blob_version(0)
{
    blob_id_main = blob_info.GetId().Get();
    blob_id_split = blob_info.GetSplitInfoBlobId().Get();

    if (blob_info.IsDead()) blob_state |= CBioseq_Handle::fState_dead;
    if (blob_info.IsSuppressed()) blob_state |= CBioseq_Handle::fState_suppress_perm;
    if (blob_info.IsWithdrawn()) blob_state |= CBioseq_Handle::fState_withdrawn;

    blob_version = blob_info.GetVersion() / 60000;

    if (!blob_id_split.empty()) {
        for (int chunk_id = 1;; ++chunk_id) {
            string chunk_blob_id = blob_info.GetChunkBlobId(chunk_id).Get();
            if (chunk_blob_id.empty()) break;
            chunks.push_back(chunk_blob_id);
        }
    }
}


const string& SPsgBlobInfo::GetBlobIdForChunk(TChunkId chunk_id) const
{
    if (chunk_id < 1 || chunk_id > chunks.size()) return kEmptyStr;
    return chunks[chunk_id - 1];
}


/////////////////////////////////////////////////////////////////////////////
// CPSG_Task
/////////////////////////////////////////////////////////////////////////////

template<class TReply> void ReportStatus(TReply reply, EPSG_Status status)
{
    if (status == EPSG_Status::eSuccess) return;
    string sstatus;
    switch (status) {
    case EPSG_Status::eCanceled:
        sstatus = "Canceled";
        break;
    case EPSG_Status::eError:
        sstatus = "Error";
        break;
    case EPSG_Status::eInProgress:
        sstatus = "In progress";
        break;
    case EPSG_Status::eNotFound:
        sstatus = "Not found";
        break;
    default:
        sstatus = to_string((int)status);
        break;
    }
    while (true) {
        string msg = reply->GetNextMessage();
        if (msg.empty()) break;
        _TRACE("Request failed: " << sstatus << " - " << msg);
    }
}


class CPSG_TaskGroup;


class CPSG_Task : public CThreadPool_Task
{
public:
    typedef shared_ptr<CPSG_Reply> TReply;

    CPSG_Task(TReply reply, CPSG_TaskGroup& group);
    ~CPSG_Task(void) override {}

    EStatus Execute(void) override;
    virtual void Finish(void) = 0;

protected:
    void OnStatusChange(EStatus old) override;

    TReply& GetReply(void) { return m_Reply; }

    virtual void DoExecute(void) {
        if (!CheckReplyStatus()) return;
        ReadReply();
        if (m_Status == eExecuting) m_Status = eCompleted;
    }

    bool IsCancelled(void) {
        if (IsCancelRequested()) {
            m_Status = eFailed;
            return true;
        }
        return false;
    }

    bool CheckReplyStatus(void);
    void ReadReply(void);
    virtual void ProcessReplyItem(shared_ptr<CPSG_ReplyItem> item) = 0;

    TReply m_Reply;
    EStatus m_Status;
private:
    CPSG_TaskGroup& m_Group;
};


// It may happen that a CThreadPool's thread holds CRef to a task longer than
// the loader exists. In this case the task needs to release some data
// (e.g. load locks) before the loader is destroyed. The guard calls
// Finish() to do the cleanup.
class CPSG_Task_Guard
{
public:
    CPSG_Task_Guard(CPSG_Task& task) : m_Task(&task) {}
    ~CPSG_Task_Guard(void) { if (m_Task) m_Task->Finish(); }
    void Resease(void) { m_Task.Reset(); }
private:
    CPSG_Task_Guard(const CPSG_Task_Guard&);
    CPSG_Task_Guard& operator=(const CPSG_Task_Guard&);

    CRef<CPSG_Task> m_Task;
};


class CPSG_TaskGroup
{
public:
    CPSG_TaskGroup(CThreadPool& pool)
        : m_Pool(pool), m_Semaphore(0, kMax_UInt) {}

    void AddTask(CPSG_Task* task) {
        {
            CFastMutexGuard guard(m_Mutex);
            m_Tasks.insert(Ref(task));
            m_Pool.AddTask(task);
        }
    }

    void PostFinished(CPSG_Task& task)
    {
        {
            CRef<CPSG_Task> ref(&task);
            CFastMutexGuard guard(m_Mutex);
            TTasks::iterator it = m_Tasks.find(ref);
            if (it == m_Tasks.end()) return;
            m_Done.insert(ref);
            m_Tasks.erase(it);
        }
        m_Semaphore.Post();
    }

    bool HasTasks(void) const
    {
        CFastMutexGuard guard(m_Mutex);
        return !m_Tasks.empty() || ! m_Done.empty();
    }

    void WaitAll(void) {
        while (HasTasks()) GetTask<CPSG_Task>();
    }

    template<class T>
    CRef<T> GetTask(void) {
        m_Semaphore.Wait();
        CRef<T> ret;
        CFastMutexGuard guard(m_Mutex);
        _ASSERT(!m_Done.empty());
        TTasks::iterator it = m_Done.begin();
        ret.Reset(dynamic_cast<T*>(it->GetNCPointerOrNull()));
        m_Done.erase(it);
        return ret;
    }

private:
    typedef set<CRef<CPSG_Task>> TTasks;

    CThreadPool& m_Pool;
    CSemaphore m_Semaphore;
    TTasks m_Tasks;
    TTasks m_Done;
    mutable CFastMutex m_Mutex;
};


CPSG_Task::CPSG_Task(TReply reply, CPSG_TaskGroup& group)
    : m_Status(eIdle), m_Reply(reply), m_Group(group)
{
}


CPSG_Task::EStatus CPSG_Task::Execute(void)
{
    m_Status = eExecuting;
    try {
        DoExecute();
    }
    catch (...) {
        return eFailed;
    }
    return m_Status;
}


void CPSG_Task::OnStatusChange(EStatus old)
{
    EStatus status = GetStatus();
    if (status == eCompleted || status == eFailed) {
        m_Group.PostFinished(*this);
    }
}

bool CPSG_Task::CheckReplyStatus(void)
{
    EPSG_Status status = m_Reply->GetStatus(0);
    if (status != EPSG_Status::eSuccess && status != EPSG_Status::eInProgress) {
        ReportStatus(m_Reply, status);
        m_Status = eFailed;
        return false;
    }
    return true;
}


void CPSG_Task::ReadReply(void)
{
    EPSG_Status status;
    for (;;) {
        if (IsCancelled()) return;
        auto reply_item = m_Reply->GetNextItem(DEFAULT_DEADLINE);
        if (!reply_item) continue;
        if (reply_item->GetType() == CPSG_ReplyItem::eEndOfReply) break;
        if (IsCancelled()) return;

        EPSG_Status status = reply_item->GetStatus(0);
        if (status != EPSG_Status::eSuccess && status != EPSG_Status::eInProgress) {
            ReportStatus(reply_item, status);
            m_Status = eFailed;
            return;
        }
        if (status == EPSG_Status::eInProgress) {
            status = reply_item->GetStatus(CDeadline::eInfinite);
            if (IsCancelled()) return;
        }
        if (status != EPSG_Status::eSuccess) {
            ReportStatus(reply_item, status);
            m_Status = eFailed;
            return;
        }
        ProcessReplyItem(reply_item);
    }
    if (IsCancelled()) return;
    status = m_Reply->GetStatus(CDeadline::eInfinite);
    if (status != EPSG_Status::eSuccess) {
        ReportStatus(m_Reply, status);
        m_Status = eFailed;
    }
}


/////////////////////////////////////////////////////////////////////////////
// CPSGDataLoader_Impl
/////////////////////////////////////////////////////////////////////////////

#define NCBI_PSGLOADER_NAME "psg_loader"
#define NCBI_PSGLOADER_SERVICE_NAME "service_name"
#define NCBI_PSGLOADER_NOSPLIT "no_split"

NCBI_PARAM_DECL(string, PSG_LOADER, SERVICE_NAME);
NCBI_PARAM_DEF_EX(string, PSG_LOADER, SERVICE_NAME, "PSG",
    eParam_NoThread, PSG_LOADER_SERVICE_NAME);
typedef NCBI_PARAM_TYPE(PSG_LOADER, SERVICE_NAME) TPSG_ServiceName;

NCBI_PARAM_DECL(bool, PSG_LOADER, NO_SPLIT);
NCBI_PARAM_DEF_EX(bool, PSG_LOADER, NO_SPLIT, false,
    eParam_NoThread, PSG_LOADER_NO_SPLIT);
typedef NCBI_PARAM_TYPE(PSG_LOADER, NO_SPLIT) TPSG_NoSplit;

NCBI_PARAM_DECL(unsigned int, PSG_LOADER, MAX_POOL_THREADS);
NCBI_PARAM_DEF_EX(unsigned int, PSG_LOADER, MAX_POOL_THREADS, 10,
    eParam_NoThread, PSG_LOADER_MAX_POOL_THREADS);
typedef NCBI_PARAM_TYPE(PSG_LOADER, MAX_POOL_THREADS) TPSG_MaxPoolThreads;


CPSGDataLoader_Impl::CPSGDataLoader_Impl(const CGBLoaderParams& params)
    : m_BlobMap(new CPSGBlobMap()),
      m_BioseqCache(new CPSGBioseqCache()),
      m_ThreadPool(new CThreadPool(kMax_UInt, TPSG_MaxPoolThreads::GetDefault()))
{
    auto_ptr<CPSGDataLoader::TParamTree> app_params;
    const CPSGDataLoader::TParamTree* psg_params = 0;
    if (params.GetParamTree()) {
        psg_params = CPSGDataLoader::GetParamsSubnode(params.GetParamTree(), NCBI_PSGLOADER_NAME);
    }
    else {
        CNcbiApplicationGuard app = CNcbiApplication::InstanceGuard();
        if (app) {
            app_params.reset(CConfig::ConvertRegToTree(app->GetConfig()));
            psg_params = CPSGDataLoader::GetParamsSubnode(app_params.get(), NCBI_PSGLOADER_NAME);
        }
    }

    string service_name;
    if (service_name.empty() && psg_params) {
        service_name = CPSGDataLoader::GetParam(psg_params, NCBI_PSGLOADER_SERVICE_NAME);
    }
    if (service_name.empty()) {
        service_name = params.GetPSGServiceName();
    }
    if (service_name.empty()) {
        service_name = TPSG_ServiceName::GetDefault();
    }

    m_NoSplit = params.GetPSGNoSplit();
    if (psg_params) {
        try {
            string value = CPSGDataLoader::GetParam(psg_params, NCBI_PSGLOADER_NOSPLIT);
            if (!value.empty()) {
                m_NoSplit = NStr::StringToBool(value);
            }
        }
        catch (CException&) {
        }
    }

    m_Queue = make_shared<CPSG_Queue>(service_name);
    m_Thread.Reset(new CPsgClientThread(m_Queue));
    m_Thread->Run();
}


CPSGDataLoader_Impl::~CPSGDataLoader_Impl(void)
{
    m_Thread->Stop();
    m_Thread->Join();
}


void CPSGDataLoader_Impl::GetIds(const CSeq_id_Handle& idh, TIds& ids)
{
    auto seq_info = x_GetBioseqInfo(idh);
    if (!seq_info) return;

    ITERATE(SPsgBioseqInfo::TIds, it, seq_info->ids) {
        ids.push_back(*it);
    }
}


int CPSGDataLoader_Impl::GetTaxId(const CSeq_id_Handle& idh)
{
    auto seq_info = x_GetBioseqInfo(idh);
    return seq_info ? seq_info->tax_id : -1;
}


TSeqPos CPSGDataLoader_Impl::GetSequenceLength(const CSeq_id_Handle& idh)
{
    auto seq_info = x_GetBioseqInfo(idh);
    return (seq_info && seq_info->length > 0) ? TSeqPos(seq_info->length) : kInvalidSeqPos;
}


CDataLoader::SHashFound
CPSGDataLoader_Impl::GetSequenceHash(const CSeq_id_Handle& idh)
{
    CDataLoader::SHashFound ret;
    auto seq_info = x_GetBioseqInfo(idh);
    if (seq_info && seq_info->hash) {
        ret.sequence_found = true;
        ret.hash_known = true;
        ret.hash = seq_info->hash;
    }
    return ret;
}


CDataLoader::STypeFound
CPSGDataLoader_Impl::GetSequenceType(const CSeq_id_Handle& idh)
{
    CDataLoader::STypeFound ret;
    auto seq_info = x_GetBioseqInfo(idh);
    if (seq_info && seq_info->molecule_type != CSeq_inst::eMol_not_set) {
        ret.sequence_found = true;
        ret.type = seq_info->molecule_type;
    }
    return ret;
}


int CPSGDataLoader_Impl::GetSequenceState(const CSeq_id_Handle& idh)
{
    const int kNotFound = (CBioseq_Handle::fState_not_found |
        CBioseq_Handle::fState_no_data);

    auto blob_id_ref = GetBlobId(idh);
    if (!blob_id_ref) {
        return kNotFound;
    }
    string blob_id = blob_id_ref->ToPsgId();
    auto blob_info = m_BlobMap->FindBlob(blob_id);
    if (!blob_info) {
        // Force load blob-info even if it's being loaded by another thread.
        CPSG_BioId bio_id = x_GetBioId(idh);
        auto context = make_shared<CPsgClientContext>();
        auto request = make_shared<CPSG_Request_Biodata>(move(bio_id), context);
        request->IncludeData(CPSG_Request_Biodata::eNoTSE);
        auto reply = x_ProcessRequest(request);
        blob_id = x_ProcessBlobReply(reply, nullptr, idh, true).blob_id;
        blob_info = m_BlobMap->FindBlob(blob_id);
    }
    return blob_info ? blob_info->blob_state : kNotFound;
}


CDataLoader::TTSE_LockSet
CPSGDataLoader_Impl::GetRecords(CDataSource* data_source,
    const CSeq_id_Handle& idh,
    CDataLoader::EChoice choice)
{
    CDataLoader::TTSE_LockSet locks;
    if (choice == CDataLoader::eExtAnnot ||
        choice == CDataLoader::eExtFeatures ||
        choice == CDataLoader::eExtAlign ||
        choice == CDataLoader::eExtGraph ||
        choice == CDataLoader::eOrphanAnnot) {
        // PSG loader doesn't provide external annotations ???
        return locks;
    }

    CPSG_BioId bio_id = x_GetBioId(idh);
    auto context = make_shared<CPsgClientContext>();
    auto request = make_shared<CPSG_Request_Biodata>(move(bio_id), context);
    
    CPSG_Request_Biodata::EIncludeData inc_data = CPSG_Request_Biodata::eNoTSE;
    if (data_source) {
        inc_data = m_NoSplit ? CPSG_Request_Biodata::eOrigTSE : CPSG_Request_Biodata::eSmartTSE;
        CDataSource::TLoadedBlob_ids loaded_blob_ids;
        data_source->GetLoadedBlob_ids(idh, CDataSource::fLoaded_bioseqs, loaded_blob_ids);
        ITERATE(CDataSource::TLoadedBlob_ids, loaded_blob_id, loaded_blob_ids) {
            const CPsgBlobId* pbid = dynamic_cast<const CPsgBlobId*>(&**loaded_blob_id);
            if (!pbid) continue;
            request->ExcludeTSE(CPSG_BlobId(pbid->ToPsgId()));
        }
    }
    request->IncludeData(inc_data);
    auto reply = x_ProcessRequest(request);
    CTSE_Lock tse_lock = x_ProcessBlobReply(reply, data_source, idh, true).lock;

    if (!tse_lock) {
        // FIXME: Exception?
        return locks;
        NCBI_THROW(CLoaderException, eLoaderFailed,
            "error loading blob for " + idh.AsString());
    }
    locks.insert(tse_lock);
    return locks;
}


CRef<CPsgBlobId> CPSGDataLoader_Impl::GetBlobId(const CSeq_id_Handle& idh)
{
    string blob_id;

    // Check cache first.
    auto seq_info = m_BioseqCache->Get(idh);
    if (seq_info && !seq_info->blob_id.empty()) {
        blob_id = seq_info->blob_id;
    }
    else {
        CPSG_BioId bio_id = x_GetBioId(idh);
        auto context = make_shared<CPsgClientContext>();
        auto request = make_shared<CPSG_Request_Biodata>(move(bio_id), context);
        request->IncludeData(CPSG_Request_Biodata::eNoTSE);
        auto reply = x_ProcessRequest(request);
        blob_id = x_ProcessBlobReply(reply, nullptr, idh, true).blob_id;
    }
    CRef<CPsgBlobId> ret;
    if (!blob_id.empty()) {
        ret.Reset(new CPsgBlobId(blob_id));
    }
    return ret;
}


CTSE_Lock CPSGDataLoader_Impl::GetBlobById(CDataSource* data_source, const CPsgBlobId& blob_id)
{
    CTSE_Lock ret;
    if (!data_source) return ret;

    CPSG_BlobId bid(blob_id.ToPsgId());
    auto context = make_shared<CPsgClientContext>();
    auto request = make_shared<CPSG_Request_Blob>(bid, kEmptyStr, context);
    request->IncludeData(m_NoSplit ? CPSG_Request_Biodata::eOrigTSE : CPSG_Request_Biodata::eSmartTSE);
    auto reply = x_ProcessRequest(request);
    ret = x_ProcessBlobReply(reply, data_source, CSeq_id_Handle(), true).lock;
    if (!ret) {
        _TRACE("Failed to load blob for " << blob_id.ToPsgId());
    }
    return ret;
}


class CPSG_Blob_Task : public CPSG_Task
{
public:
    CPSG_Blob_Task(
        TReply reply,
        CPSG_TaskGroup& group,
        const CSeq_id_Handle& idh,
        CDataSource* data_source,
        CPSGDataLoader_Impl& loader)
        : CPSG_Task(reply, group),
        m_Id(idh),
        m_DataSource(data_source),
        m_Loader(loader)
    {}

    ~CPSG_Blob_Task(void) override {}

    typedef map<string, shared_ptr<CPSG_BlobData>> TBlobDataMap;
    typedef map<string, shared_ptr<CPSG_BlobInfo>> TBlobInfoMap;

    CSeq_id_Handle m_Id;
    shared_ptr<CPSG_BlobInfo> m_MainBlobInfo;
    shared_ptr<CPSG_BlobInfo> m_SplitBlobInfo;
    shared_ptr<CPSG_SkippedBlob> m_Skipped;
    CPSGDataLoader_Impl::SReplyResult m_ReplyResult;
    shared_ptr<SPsgBlobInfo> m_PsgBlobInfo;

    shared_ptr<CPSG_BlobInfo> GetBlobInfo(const string& id) {
        shared_ptr<CPSG_BlobInfo> ret;
        TBlobInfoMap::iterator it = m_BlobInfo.find(id);
        if (it != m_BlobInfo.end()) ret = it->second;
        return ret;
    }

    shared_ptr<CPSG_BlobData> GetBlobData(const string& id) {
        shared_ptr<CPSG_BlobData> ret;
        TBlobDataMap::iterator it = m_BlobData.find(id);
        if (it != m_BlobData.end()) ret = it->second;
        return ret;
    }

    CPSGDataLoader_Impl::SReplyResult WaitForSkipped(void);

    void Finish(void) override
    {
        m_MainBlobInfo.reset();
        m_SplitBlobInfo.reset();
        m_Skipped.reset();
        m_ReplyResult = CPSGDataLoader_Impl::SReplyResult();
        m_PsgBlobInfo.reset();
        m_BlobInfo.clear();
        m_BlobData.clear();
    }

protected:
    void DoExecute(void) override;
    void ProcessReplyItem(shared_ptr<CPSG_ReplyItem> item) override;

private:
    CDataSource* m_DataSource;
    CPSGDataLoader_Impl& m_Loader;
    TBlobInfoMap m_BlobInfo;
    TBlobDataMap m_BlobData;
};


void CPSG_Blob_Task::DoExecute(void)
{
    if (!CheckReplyStatus()) return;
    ReadReply();
    if (m_Status == eFailed) return;
    if (m_Skipped) {
        m_Status = eCompleted;
        return;
    }

    if (m_ReplyResult.blob_id.empty()) {
        // If the source request was for blob rather than bioseq, there may be no bioseq info
        // and blob_id stays empty.
        if (m_Reply->GetRequest()->GetType() == "blob") {
            shared_ptr<const CPSG_Request_Blob> blob_request = static_pointer_cast<const CPSG_Request_Blob>(m_Reply->GetRequest());
            if (blob_request) {
                m_ReplyResult.blob_id = blob_request->GetId();
            }
        }
    }
    if (m_ReplyResult.blob_id.empty()) {
        m_Status = eFailed;
        return;
    }

    CRef<CPsgBlobId> psg_main_id(new CPsgBlobId(m_ReplyResult.blob_id));
    CDataLoader::TBlobId dl_blob_id = CDataLoader::TBlobId(psg_main_id);

    CTSE_LoadLock load_lock;

    m_MainBlobInfo = GetBlobInfo(m_ReplyResult.blob_id);
    if (!m_MainBlobInfo) {
        m_Status = eFailed;
        return;
    }

    string split_info_id = m_MainBlobInfo->GetSplitInfoBlobId().Get();
    if (!split_info_id.empty()) {
        m_SplitBlobInfo = GetBlobInfo(split_info_id);
    }

    // Find or create main blob-info entry.
    m_PsgBlobInfo = m_Loader.m_BlobMap->FindBlob(m_ReplyResult.blob_id);
    if (!m_PsgBlobInfo) {
        m_PsgBlobInfo = make_shared<SPsgBlobInfo>(*m_MainBlobInfo);
        m_Loader.m_BlobMap->AddBlob(m_ReplyResult.blob_id, m_PsgBlobInfo);
    }

    if (!m_DataSource) {
        // No data to load, just bioseq-info.
        m_Status = eCompleted;
        return;
    }

    // Read blob data (if any) and pass to the data source.
    load_lock = m_DataSource->GetTSE_LoadLock(dl_blob_id);
    if (!load_lock) {
        m_Status = eFailed;
        return;
    }
    if (load_lock && load_lock.IsLoaded()) {
        m_ReplyResult.lock = load_lock;
        m_Status = eCompleted;
        return;
    }

    shared_ptr<CPSG_BlobInfo> blob_info = m_SplitBlobInfo ? m_SplitBlobInfo : m_MainBlobInfo;
    auto blob_data = GetBlobData(blob_info->GetId().Get());
    if (blob_data) {
        m_Loader.x_ReadBlobData(*m_PsgBlobInfo, *blob_info, *blob_data, load_lock);
    }
    if (m_SplitBlobInfo) {
        CTSE_Split_Info& tse_split_info = load_lock->GetSplitInfo();
        CTSE_Chunk_Info::TChunkId cid = 0;
        while (true) {
            ++cid;
            string ch_blob_id = m_PsgBlobInfo->GetBlobIdForChunk(cid);
            if (ch_blob_id.empty()) break;
            auto chunk_info = GetBlobInfo(ch_blob_id);
            auto chunk_data = GetBlobData(ch_blob_id);
            if (!chunk_info || !chunk_data) continue;

            auto_ptr<CObjectIStream> in(CPSGDataLoader_Impl::GetBlobDataStream(*chunk_info, *chunk_data));
            CRef<CID2S_Chunk> id2_chunk(new CID2S_Chunk);
            *in >> *id2_chunk;
            CTSE_Chunk_Info& chunk = tse_split_info.GetChunk(cid);
            CSplitParser::Load(chunk, *id2_chunk);
            chunk.SetLoaded();
        }
    }
    m_ReplyResult.lock = load_lock;

    m_Status = eCompleted;
}


void CPSG_Blob_Task::ProcessReplyItem(shared_ptr<CPSG_ReplyItem> item)
{
    switch (item->GetType()) {
    case CPSG_ReplyItem::eBioseqInfo:
    {
        // Only one bioseq-info is allowed per reply.
        shared_ptr<CPSG_BioseqInfo> bioseq_info = static_pointer_cast<CPSG_BioseqInfo>(item);
        m_ReplyResult.blob_id = bioseq_info->GetBlobId().Get();
        m_Loader.m_BioseqCache->Add(*bioseq_info, m_Id);
        break;
    }
    case CPSG_ReplyItem::eBlobInfo:
    {
        auto blob_info = static_pointer_cast<CPSG_BlobInfo>(item);
        m_BlobInfo[blob_info->GetId().Get()] = blob_info;
        break;
    }
    case CPSG_ReplyItem::eBlobData:
    {
        shared_ptr<CPSG_BlobData> data = static_pointer_cast<CPSG_BlobData>(item);
        m_BlobData[data->GetId().Get()] = data;
        break;
    }
    case CPSG_ReplyItem::eSkippedBlob:
    {
        // Only main blob can be skipped.
        _ASSERT(!m_Skipped);
        m_Skipped = static_pointer_cast<CPSG_SkippedBlob>(item);
        break;
    }
    default:
    {
        break;
    }
    }
}


CPSGDataLoader_Impl::SReplyResult CPSG_Blob_Task::WaitForSkipped(void)
{
    CPSGDataLoader_Impl::SReplyResult ret;
    ret.blob_id = m_ReplyResult.blob_id;
    if (!m_DataSource) return ret;

    CRef<CPsgBlobId> psg_main_id(new CPsgBlobId(ret.blob_id));
    CDataLoader::TBlobId dl_blob_id = CDataLoader::TBlobId(psg_main_id);
    CTSE_LoadLock load_lock;
    _ASSERT(m_Skipped);
    CPSG_SkippedBlob::EReason skip_reason = m_Skipped->GetReason();
    switch (skip_reason) {
    case CPSG_SkippedBlob::eInProgress:
    {
        // Try to wait for the blob to be loaded.
        load_lock = m_DataSource->GetLoadedTSE_Lock(dl_blob_id, CTimeout::eInfinite);
        break;
    }
    case CPSG_SkippedBlob::eExcluded:
    case CPSG_SkippedBlob::eSent:
        // Check if the blob is already loaded, force loading if necessary.
        load_lock = m_DataSource->GetTSE_LoadLock(dl_blob_id);
        break;
    default: // unknown
        return ret;
    }
    if (load_lock && load_lock.IsLoaded()) {
        m_Skipped.reset();
        ret.lock = load_lock;
    }
    return ret;
}


void CPSGDataLoader_Impl::GetBlobs(CDataSource* data_source, TTSE_LockSets& tse_sets)
{
    if (!data_source) return;
    auto context = make_shared<CPsgClientContext_Bulk>();
    CPSG_TaskGroup group(*m_ThreadPool);
    ITERATE(TTSE_LockSets, tse_set, tse_sets) {
        const CSeq_id_Handle& id = tse_set->first;
        CPSG_BioId bio_id = x_GetBioId(id);
        auto request = make_shared<CPSG_Request_Biodata>(move(bio_id), context);
        request->IncludeData(m_NoSplit ? CPSG_Request_Biodata::eOrigTSE : CPSG_Request_Biodata::eWholeTSE);
        auto reply = x_ProcessRequest(request);
        CRef<CPSG_Blob_Task> task(
            new CPSG_Blob_Task(reply, group, id, data_source, *this));
        group.AddTask(task);
    }
    // Waiting for skipped blobs can block all pool threads. To prevent this postpone
    // waiting until all other tasks are completed.
    typedef list<CRef<CPSG_Blob_Task>> TTasks;
    TTasks skipped_tasks;
    list<shared_ptr<CPSG_Task_Guard>> guards;
    while (group.HasTasks()) {
        CRef<CPSG_Blob_Task> task(group.GetTask<CPSG_Blob_Task>().GetNCPointerOrNull());
        _ASSERT(task);
        guards.push_back(make_shared<CPSG_Task_Guard>(*task));
        if (task->GetStatus() == CThreadPool_Task::eFailed) {
            _TRACE("Failed to get blob for " << task->m_Id.AsString());
            continue;
        }
        if (task->m_Skipped) {
            skipped_tasks.push_back(task);
            continue;
        }
        SReplyResult res = task->m_ReplyResult;
        if (task->m_ReplyResult.lock) tse_sets[task->m_Id].insert(task->m_ReplyResult.lock);
    }
    NON_CONST_ITERATE(TTasks, it, skipped_tasks) {
        CPSG_Blob_Task& task = **it;
        SReplyResult result = task.WaitForSkipped();
        if (!result.lock) {
            // Force reloading blob
            result = x_RetryBlobRequest(task.m_ReplyResult.blob_id, data_source, task.m_Id);
        }
        if (result.lock) tse_sets[task.m_Id].insert(result.lock);
    }
}


void CPSGDataLoader_Impl::LoadChunk(CTSE_Chunk_Info& chunk_info)
{
    CDataLoader::TChunkSet chunks;
    chunks.push_back(Ref(&chunk_info));
    LoadChunks(chunks);
}


class CPSG_LoadChunk_Task : public CPSG_Task
{
public:
    CPSG_LoadChunk_Task(TReply reply, CPSG_TaskGroup& group, CDataLoader::TChunk chunk)
        : CPSG_Task(reply, group), m_Chunk(chunk) {}

    ~CPSG_LoadChunk_Task(void) override {}

    void Finish(void) override {
        m_Chunk.Reset();
        m_BlobInfo.reset();
        m_BlobData.reset();
    }

protected:
    void DoExecute(void) override;
    void ProcessReplyItem(shared_ptr<CPSG_ReplyItem> item) override;

private:
    CDataLoader::TChunk m_Chunk;
    shared_ptr<CPSG_BlobInfo> m_BlobInfo;
    shared_ptr<CPSG_BlobData> m_BlobData;
};


void CPSG_LoadChunk_Task::DoExecute(void)
{
    if (!CheckReplyStatus()) return;
    ReadReply();
    if (m_Status == eFailed) return;

    if (!m_BlobInfo || !m_BlobData) {
        _TRACE("Failed to get chunk info or data for blob-id " << m_Chunk->GetBlobId());
        m_Status = eFailed;
        return;
    }

    if (IsCancelled()) return;
    auto_ptr<CObjectIStream> in(CPSGDataLoader_Impl::GetBlobDataStream(*m_BlobInfo, *m_BlobData));
    if (!in.get()) {
        _TRACE("Failed to open chunk data stream for blob-id " << m_BlobInfo->GetId().Get());
        m_Status = eFailed;
        return;
    }

    CRef<CID2S_Chunk> id2_chunk(new CID2S_Chunk);
    *in >> *id2_chunk;
    CSplitParser::Load(*m_Chunk, *id2_chunk);
    m_Chunk->SetLoaded();

    m_Status = eCompleted;
}


void CPSG_LoadChunk_Task::ProcessReplyItem(shared_ptr<CPSG_ReplyItem> item)
{
    switch (item->GetType()) {
    case CPSG_ReplyItem::eBlobInfo:
        m_BlobInfo = static_pointer_cast<CPSG_BlobInfo>(item);
        break;
    case CPSG_ReplyItem::eBlobData:
        m_BlobData = static_pointer_cast<CPSG_BlobData>(item);
        break;
    default:
        break;
    }
}


void CPSGDataLoader_Impl::LoadChunks(const CDataLoader::TChunkSet& chunks)
{
    if (chunks.empty()) return;

    typedef map<void*, CDataLoader::TChunk> TChunkMap;
    TChunkMap chunk_map;
    auto context = make_shared<CPsgClientContext_Bulk>();
    ITERATE(CDataLoader::TChunkSet, it, chunks) {
        const CTSE_Chunk_Info& chunk = **it;
        const CPsgBlobId& blob_id = dynamic_cast<const CPsgBlobId&>(*chunk.GetBlobId());

        // Load split blob-info
        auto psg_blob_found = m_BlobMap->FindBlob(blob_id.ToPsgId());
        if (!psg_blob_found) {
            _TRACE("Can not load chunk for unknown blob-id " << blob_id.ToPsgId());
            break; // Blob-id not yet resolved.
        }

        const SPsgBlobInfo& psg_blob_info = *psg_blob_found;
        const string& str_chunk_blob_id = psg_blob_info.GetBlobIdForChunk(chunk.GetChunkId());
        if (str_chunk_blob_id.empty()) {
            _TRACE("Chunk blob-id not found for " << blob_id.ToPsgId() << ":" << chunk.GetChunkId());
            break;
        }

        CPSG_BlobId chunk_blob_id(str_chunk_blob_id);
        auto request = make_shared<CPSG_Request_Blob>(chunk_blob_id, kEmptyStr, context);
        chunk_map[request.get()] = *it;
        x_SendRequest(request);
    }

    CPSG_TaskGroup group(*m_ThreadPool);
    list<shared_ptr<CPSG_Task_Guard>> guards;
    while (!chunk_map.empty()) {
        auto reply = context->GetReply();
        if (!reply) continue;
        TChunkMap::iterator chunk_it = chunk_map.find((void*)reply->GetRequest().get());
        _ASSERT(chunk_it != chunk_map.end());
        CDataLoader::TChunk chunk = chunk_it->second;
        chunk_map.erase(chunk_it);
        CRef<CPSG_LoadChunk_Task> task(new CPSG_LoadChunk_Task(reply, group, chunk));
        guards.push_back(make_shared<CPSG_Task_Guard>(*task));
        group.AddTask(task);
    }
    group.WaitAll();
}


class CPSG_AnnotRecordsNA_Task : public CPSG_Task
{
public:
    CPSG_AnnotRecordsNA_Task( TReply reply, CPSG_TaskGroup& group)
        : CPSG_Task(reply, group) {}

    ~CPSG_AnnotRecordsNA_Task(void) override {}

    shared_ptr<CPSG_NamedAnnotInfo> m_AnnotInfo;

    void Finish(void) override {
        m_AnnotInfo.reset();
    }

protected:
    void ProcessReplyItem(shared_ptr<CPSG_ReplyItem> item) override {
        if (item->GetType() == CPSG_ReplyItem::eNamedAnnotInfo) {
            m_AnnotInfo = static_pointer_cast<CPSG_NamedAnnotInfo>(item);
        }
    }
};


CDataLoader::TTSE_LockSet CPSGDataLoader_Impl::GetAnnotRecordsNA(
    CDataSource* data_source,
    const CSeq_id_Handle& idh,
    const SAnnotSelector* sel,
    CDataLoader::TProcessedNAs* processed_nas)
{
    CDataLoader::TTSE_LockSet locks;
    if (!data_source || !sel || !sel->IsIncludedAnyNamedAnnotAccession()) {
        return locks;
    }
    const SAnnotSelector::TNamedAnnotAccessions& accs = sel->GetNamedAnnotAccessions();
    CPSG_BioId bio_id = x_GetBioId(idh);
    CPSG_Request_NamedAnnotInfo::TAnnotNames annot_names;
    ITERATE(SAnnotSelector::TNamedAnnotAccessions, it, accs) {
        annot_names.push_back(it->first);
    }
    auto context = make_shared<CPsgClientContext>();
    auto request = make_shared<CPSG_Request_NamedAnnotInfo>(move(bio_id), annot_names, context);
    auto reply = x_ProcessRequest(request);
    if (!reply) return locks;

    CPSG_TaskGroup group(*m_ThreadPool);
    CRef<CPSG_AnnotRecordsNA_Task> task(new CPSG_AnnotRecordsNA_Task(reply, group));
    CPSG_Task_Guard guard(*task);
    group.AddTask(task);
    group.WaitAll();

    if (task->GetStatus() == CThreadPool_Task::eCompleted) {
        shared_ptr<CPSG_NamedAnnotInfo> info = task->m_AnnotInfo;
        if (info) {
            CDataLoader::SetProcessedNA(info->GetName(), processed_nas);
            auto blob_id = info->GetBlobId();
            auto tse_lock = GetBlobById(data_source, CPsgBlobId(blob_id.Get()));
            if (tse_lock) locks.insert(tse_lock);
        }
    }
    else {
        _TRACE("Failed to load annotations for " << idh.AsString());
    }

    return locks;
}


void CPSGDataLoader_Impl::DropTSE(const CPsgBlobId& blob_id)
{
    m_BlobMap->DropBlob(blob_id);
}


void CPSGDataLoader_Impl::GetAccVers(const TIds& ids, TLoaded& loaded, TIds& ret)
{
    vector<shared_ptr<SPsgBioseqInfo>> infos;
    infos.resize(ret.size());
    x_GetBulkBioseqInfo(CPSG_Request_Resolve::fCanonicalId, ids, loaded, infos);
    for (size_t i = 0; i < infos.size(); ++i) {
        if (!infos[i].get()) continue;
        CSeq_id_Handle idh = infos[i]->canonical;
        if (idh.IsAccVer()) {
            ret[i] = idh;
        }
    }
}


void CPSGDataLoader_Impl::GetGis(const TIds& ids, TLoaded& loaded, TGis& ret)
{
    vector<shared_ptr<SPsgBioseqInfo>> infos;
    infos.resize(ret.size());
    x_GetBulkBioseqInfo(CPSG_Request_Resolve::fGi, ids, loaded, infos);
    for (size_t i = 0; i < infos.size(); ++i) {
        if (!infos[i].get()) continue;
        ret[i] = infos[i]->gi;
    }
}


CPSG_BioId CPSGDataLoader_Impl::x_GetBioId(const CSeq_id_Handle& idh)
{
    CConstRef<CSeq_id> id = idh.GetSeqId();
    string label = id->AsFastaString();
    return CPSG_BioId(label, id->Which());
}


void CPSGDataLoader_Impl::x_SendRequest(shared_ptr<CPSG_Request> request)
{
    m_Queue->SendRequest(request, DEFAULT_DEADLINE);
    m_Thread->Wake();
}


shared_ptr<CPSG_Reply> CPSGDataLoader_Impl::x_ProcessRequest(shared_ptr<CPSG_Request> request)
{
    x_SendRequest(request);
    auto context = request->GetUserContext<CPsgClientContext>();
    _ASSERT(context);
    return context->GetReply();
}


CPSGDataLoader_Impl::SReplyResult
CPSGDataLoader_Impl::x_RetryBlobRequest(const string& blob_id, CDataSource* data_source, CSeq_id_Handle req_idh)
{
    CPSG_BlobId req_blob_id(blob_id);
    auto context = make_shared<CPsgClientContext>();
    auto blob_request = make_shared<CPSG_Request_Blob>(req_blob_id, kEmptyStr, context);
    blob_request->IncludeData(m_NoSplit ? CPSG_Request_Biodata::eOrigTSE : CPSG_Request_Biodata::eSmartTSE);
    auto blob_reply = x_ProcessRequest(blob_request);
    return x_ProcessBlobReply(blob_reply, data_source, req_idh, false);
}


CPSGDataLoader_Impl::SReplyResult CPSGDataLoader_Impl::x_ProcessBlobReply(
    shared_ptr<CPSG_Reply> reply,
    CDataSource* data_source,
    CSeq_id_Handle req_idh,
    bool retry)
{
    SReplyResult ret;

    if (!reply) {
        _TRACE("Request failed: null reply");
        return ret;
    }

    CPSG_TaskGroup group(*m_ThreadPool);
    CRef<CPSG_Blob_Task> task(
        new CPSG_Blob_Task(reply, group, req_idh, data_source, *this));
    CPSG_Task_Guard guard(*task);
    group.AddTask(task);
    group.WaitAll();

    if (task->GetStatus() == CThreadPool_Task::eCompleted) {
        if (task->m_Skipped) {
            ret = task->WaitForSkipped();
            if (!ret.lock && retry) {
                // Force reloading blob
                ret = x_RetryBlobRequest(task->m_ReplyResult.blob_id, data_source, req_idh);
            }
        }
        else {
            ret = task->m_ReplyResult;
        }
    }
    else {
        _TRACE("Failed to load blob for " << req_idh.AsString());
    }
    return ret;
}


class CPSG_BioseqInfo_Task : public CPSG_Task
{
public:
    CPSG_BioseqInfo_Task(TReply reply, CPSG_TaskGroup& group)
        : CPSG_Task(reply, group) {}

    ~CPSG_BioseqInfo_Task(void) override {}

    shared_ptr<CPSG_BioseqInfo> m_BioseqInfo;

    void Finish(void) override {
        m_BioseqInfo.reset();
    }

protected:
    void ProcessReplyItem(shared_ptr<CPSG_ReplyItem> item) override {
        if (item->GetType() == CPSG_ReplyItem::eBioseqInfo) {
            m_BioseqInfo = static_pointer_cast<CPSG_BioseqInfo>(item);
        }
    }
};


shared_ptr<SPsgBioseqInfo> CPSGDataLoader_Impl::x_GetBioseqInfo(const CSeq_id_Handle& idh)
{
    shared_ptr<SPsgBioseqInfo> ret = m_BioseqCache->Get(idh);
    if (ret) {
        return ret;
    }

    CPSG_BioId bio_id = x_GetBioId(idh);
    auto context = make_shared<CPsgClientContext>();
    shared_ptr<CPSG_Request_Resolve> request = make_shared<CPSG_Request_Resolve>(move(bio_id), context);
    request->IncludeInfo(CPSG_Request_Resolve::fAllInfo);
    x_SendRequest(request);
    auto reply = context->GetReply();
    if (!reply) {
        _TRACE("Request failed: null reply");
        return nullptr;
    }

    CPSG_TaskGroup group(*m_ThreadPool);
    CRef<CPSG_BioseqInfo_Task> task(new CPSG_BioseqInfo_Task(reply, group));
    CPSG_Task_Guard guard(*task);
    group.AddTask(task);
    group.WaitAll();

    if (task->GetStatus() != CThreadPool_Task::eCompleted || !task->m_BioseqInfo) {
        _TRACE("Failed to get bioseq info for " << idh.AsString());
        return nullptr;
    }

    return m_BioseqCache->Add(*task->m_BioseqInfo, idh);
}


void CPSGDataLoader_Impl::x_ReadBlobData(
    const SPsgBlobInfo& psg_blob_info,
    const CPSG_BlobInfo& blob_info,
    const CPSG_BlobData& blob_data,
    CTSE_LoadLock& load_lock)
{
    if (load_lock.IsLoaded()) return;
    load_lock->SetBlobVersion(CTSE_Info::TBlobVersion(psg_blob_info.blob_version));
    load_lock->SetBlobState(psg_blob_info.blob_state);

    auto_ptr<CObjectIStream> in(GetBlobDataStream(blob_info, blob_data));
    if (!in.get()) {
        _TRACE("Failed to open blob data stream for blob-id " << blob_info.GetId().Get());
        return;
    }

    if (!m_NoSplit && psg_blob_info.IsSplit()) {
        CRef<CID2S_Split_Info> split_info(new CID2S_Split_Info);
        *in >> *split_info;
        CSplitParser::Attach(*load_lock, *split_info);
    }
    else {
        CRef<CSeq_entry> entry(new CSeq_entry);
        *in >> *entry;
        load_lock->SetSeq_entry(*entry);
    }
    load_lock.SetLoaded();
}


CObjectIStream* CPSGDataLoader_Impl::GetBlobDataStream(
    const CPSG_BlobInfo& blob_info,
    const CPSG_BlobData& blob_data)
{
    istream& data_stream = blob_data.GetStream();
    CNcbiIstream* in = &data_stream;
    auto_ptr<CNcbiIstream> z_stream;
    CObjectIStream* ret = nullptr;

    if (blob_info.GetCompression() == "gzip") {
        z_stream.reset(new CCompressionIStream(data_stream,
            new CZipStreamDecompressor(CZipCompression::fGZip), 0));
        in = z_stream.get();
    }
    else if (!blob_info.GetCompression().empty()) {
        _TRACE("Unsupported data compression: '" << blob_info.GetCompression() << "'");
        return nullptr;
    }

    EOwnership own = z_stream.get() ? eTakeOwnership : eNoOwnership;
    if (blob_info.GetFormat() == "asn.1") {
        ret = CObjectIStream::Open(eSerial_AsnBinary, *in, own);
    }
    else if (blob_info.GetFormat() == "asn1-text") {
        ret = CObjectIStream::Open(eSerial_AsnText, *in, own);
    }
    else if (blob_info.GetFormat() == "xml") {
        ret = CObjectIStream::Open(eSerial_Xml, *in, own);
    }
    else if (blob_info.GetFormat() == "json") {
        ret = CObjectIStream::Open(eSerial_Json, *in, own);
    }
    else {
        _TRACE("Unsupported data format: '" << blob_info.GetFormat() << "'");
        return nullptr;
    }
    _ASSERT(ret);
    z_stream.release();
    return ret;
}


void CPSGDataLoader_Impl::x_GetBulkBioseqInfo(
    CPSG_Request_Resolve::EIncludeInfo info,
    const TIds& ids,
    TLoaded& loaded,
    TBioseqInfos& ret)
{
    TIdxMap idx_map;
    auto context = make_shared<CPsgClientContext_Bulk>();
    for (size_t i = 0; i < ids.size(); ++i) {
        if (loaded[i]) continue;
        ret[i] = m_BioseqCache->Get(ids[i]);
        if (ret[i]) {
            loaded[i] = true;
            continue;
        }
        CPSG_BioId bio_id = x_GetBioId(ids[i]);
        shared_ptr<CPSG_Request_Resolve> request = make_shared<CPSG_Request_Resolve>(move(bio_id), context);
        idx_map[request.get()] = i;
        request->IncludeInfo(info);
        x_SendRequest(request);
    }

    CPSG_TaskGroup group(*m_ThreadPool);
    typedef  map<CRef<CPSG_BioseqInfo_Task>, size_t> TTasks;
    TTasks tasks;
    list<shared_ptr<CPSG_Task_Guard>> guards;
    while (!idx_map.empty()) {
        auto reply = context->GetReply();
        if (!reply) continue;
        TIdxMap::iterator idx_it = idx_map.find((void*)reply->GetRequest().get());
        size_t idx = ret.size();
        if (idx_it != idx_map.end()) {
            idx = idx_it->second;
            idx_map.erase(idx_it);
        }

        CRef<CPSG_BioseqInfo_Task> task(new CPSG_BioseqInfo_Task(reply, group));
        guards.push_back(make_shared<CPSG_Task_Guard>(*task));
        tasks[task] = idx;
        group.AddTask(task);
    }
    while (group.HasTasks()) {
        CRef<CPSG_BioseqInfo_Task> task = group.GetTask<CPSG_BioseqInfo_Task>();
        _ASSERT(task);
        TTasks::const_iterator it = tasks.find(task);
        _ASSERT(it != tasks.end());
        if (task->GetStatus() == CThreadPool_Task::eFailed) {
            _TRACE("Failed to load bioseq info for " << ids[it->second].AsString());
            continue;
        }
        _ASSERT(task->m_BioseqInfo);
        ret[it->second] = make_shared<SPsgBioseqInfo>(*task->m_BioseqInfo);
        loaded[it->second] = true;
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // HAVE_PSG_LOADER
