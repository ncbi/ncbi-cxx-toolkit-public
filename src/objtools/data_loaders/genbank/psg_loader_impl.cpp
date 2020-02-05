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

protected:
    TReply& GetReply(void) { return m_Reply; }
    void SetStatus(EStatus status) { m_Status = status; }

    virtual void DoExecute(void) = 0;

    bool IsCancelled(void) {
        if (IsCancelRequested()) {
            SetStatus(eCanceled);
            return true;
        }
        return false;
    }

    bool CheckReplyStatus(void);
    void ReadReply(void);
    virtual void ProcessReplyItem(shared_ptr<CPSG_ReplyItem> item) = 0;

private:
    EStatus m_Status;
    TReply m_Reply;
    CPSG_TaskGroup& m_Group;
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
        }
        m_Pool.AddTask(task);
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
    m_Status = eCompleted;
    try {
        DoExecute();
    }
    catch (...) {}
    m_Group.PostFinished(*this);
    return m_Status;
}


bool CPSG_Task::CheckReplyStatus(void)
{
    EPSG_Status status = m_Reply->GetStatus(0);
    if (status != EPSG_Status::eSuccess && status != EPSG_Status::eInProgress) {
        ReportStatus(m_Reply, status);
        SetStatus(eFailed);
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
        if (!reply_item) {
            _TRACE("Request failed: null reply item");
            continue;
        }
        if (reply_item->GetType() == CPSG_ReplyItem::eEndOfReply) {
            break;
        }
        if (IsCancelled()) return;
        ProcessReplyItem(reply_item);
    }
    if (IsCancelled()) return;
    status = m_Reply->GetStatus(CDeadline::eInfinite);
    if (status != EPSG_Status::eSuccess) {
        ReportStatus(m_Reply, status);
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
        blob_id = x_ProcessBlobReply(reply, nullptr, idh).blob_id;
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
    CTSE_LoadLock load_lock = x_ProcessBlobReply(reply, data_source, idh).lock;

    if (!load_lock || !load_lock.IsLoaded()) {
        // FIXME: Exception?
        return locks;
        NCBI_THROW(CLoaderException, eLoaderFailed,
            "error loading blob for " + idh.AsString());
    }
    locks.insert(load_lock);
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
        blob_id = x_ProcessBlobReply(reply, nullptr, idh).blob_id;
    }
    CRef<CPsgBlobId> ret;
    if (!blob_id.empty()) {
        ret.Reset(new CPsgBlobId(blob_id));
    }
    return ret;
}


CTSE_LoadLock CPSGDataLoader_Impl::GetBlobById(CDataSource* data_source, const CPsgBlobId& blob_id)
{
    CPSG_BlobId bid(blob_id.ToPsgId());
    auto context = make_shared<CPsgClientContext>();
    auto request = make_shared<CPSG_Request_Blob>(bid, kEmptyStr, context);
    request->IncludeData(m_NoSplit ? CPSG_Request_Biodata::eOrigTSE : CPSG_Request_Biodata::eSmartTSE);
    auto reply = x_ProcessRequest(request);
    CTSE_LoadLock ret = x_ProcessBlobReply(reply, data_source, CSeq_id_Handle()).lock;
    if (!ret || !ret.IsLoaded()) {
        _TRACE("Failed to load blob for " << blob_id.ToPsgId());
    }
    return ret;
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
    if (GetStatus() == eFailed) return;

    if (!m_BlobInfo || !m_BlobData) {
        _TRACE("Failed to get chunk info or data for blob-id " << m_Chunk->GetBlobId());
        SetStatus(eFailed);
        return;
    }

    if (IsCancelled()) return;
    auto_ptr<CObjectIStream> in(CPSGDataLoader_Impl::GetBlobDataStream(*m_BlobInfo, *m_BlobData));
    if (!in.get()) {
        _TRACE("Failed to open chunk data stream for blob-id " << m_BlobInfo->GetId().Get());
        SetStatus(eFailed);
        return;
    }

    CRef<CID2S_Chunk> id2_chunk(new CID2S_Chunk);
    *in >> *id2_chunk;
    CSplitParser::Load(*m_Chunk, *id2_chunk);
    m_Chunk->SetLoaded();

    SetStatus(eCompleted);
}


void CPSG_LoadChunk_Task::ProcessReplyItem(shared_ptr<CPSG_ReplyItem> item)
{
    EPSG_Status status = item->GetStatus(CDeadline::eInfinite);
    if (status != EPSG_Status::eSuccess) {
        ReportStatus(item, status);
        SetStatus(eFailed);
        return;
    }
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
    while (!chunk_map.empty()) {
        auto reply = context->GetReply();
        if (!reply) continue;
        TChunkMap::iterator chunk_it = chunk_map.find((void*)reply->GetRequest().get());
        _ASSERT(chunk_it != chunk_map.end());
        CDataLoader::TChunk chunk = chunk_it->second;
        chunk_map.erase(chunk_it);
        group.AddTask(new CPSG_LoadChunk_Task(reply, group, chunk));
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

protected:
    void DoExecute(void) override {
        if (!CheckReplyStatus()) return;
        ReadReply();
        if (GetStatus() == eFailed) return;
        SetStatus(eCompleted);
    }

    void ProcessReplyItem(shared_ptr<CPSG_ReplyItem> item) override {
        EPSG_Status status = item->GetStatus(CDeadline::eInfinite);
        if (status != EPSG_Status::eSuccess) {
            ReportStatus(item, status);
            SetStatus(eFailed);
            return;
        }
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
        return move(locks);
    }
    const SAnnotSelector::TNamedAnnotAccessions& accs = sel->GetNamedAnnotAccessions();
    CPSG_BioId bio_id = x_GetBioId(idh);
    CPSG_Request_NamedAnnotInfo::TAnnotNames annot_names;
    ITERATE(SAnnotSelector::TNamedAnnotAccessions, it, accs) {
        annot_names.push_back(it->first);
    }
    auto context = make_shared<CPsgClientContext>();
    auto request = make_shared<CPSG_Request_NamedAnnotInfo>(move(bio_id), annot_names, context);
    x_SendRequest(request);
    auto reply = x_ProcessRequest(request);
    if (!reply) return move(locks);

    CPSG_TaskGroup group(*m_ThreadPool);
    CRef<CPSG_AnnotRecordsNA_Task> task(new CPSG_AnnotRecordsNA_Task(reply, group));
    group.AddTask(task);
    group.WaitAll();

    if (task->GetStatus() == CThreadPool_Task::eCompleted) {
        shared_ptr<CPSG_NamedAnnotInfo> info = task->m_AnnotInfo;
        if (info) {
            CDataLoader::SetProcessedNA(info->GetName(), processed_nas);
            auto blob_id = info->GetBlobId();
            auto tse_lock = GetBlobById(data_source, CPsgBlobId(blob_id.Get()));
            if (tse_lock.IsLoaded()) locks.insert(tse_lock);
        }
    }

    return move(locks);
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
        ret[i] = infos[i]->canonical;
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


class CPSG_Blob_Task : public CPSG_Task
{
public:
    CPSG_Blob_Task(
        TReply reply,
        CPSG_TaskGroup& group,
        const CSeq_id_Handle& idh,
        CPSGBlobMap& blob_map,
        CPSGBioseqCache& bioseq_cache)
        : CPSG_Task(reply, group),
          m_Id(idh),
          m_BlobMap(blob_map),
          m_BioseqCache(bioseq_cache)
    {}

    ~CPSG_Blob_Task(void) override {}

    shared_ptr<CPSG_BioseqInfo> m_BioseqInfo;
    shared_ptr<CPSG_BlobInfo> m_MainBlobInfo;
    shared_ptr<CPSG_BlobInfo> m_SplitBlobInfo;
    shared_ptr<CPSG_BlobInfo> m_NextBlobInfo;
    shared_ptr<CPSG_BlobData> m_BlobData;
    shared_ptr<CPSG_SkippedBlob> m_Skipped;
    string m_MainBlobId;
    string m_NextBlobId;
    CDataLoader::TBlobId m_DataLoaderBlobId;
    shared_ptr<SPsgBlobInfo> m_PsgBlobInfo;

protected:
    void DoExecute(void) override;
    void ProcessReplyItem(shared_ptr<CPSG_ReplyItem> item) override;

private:
    CSeq_id_Handle m_Id;
    CDataSource* m_DataSource;
    CPSGBlobMap& m_BlobMap;
    CPSGBioseqCache& m_BioseqCache;
};


void CPSG_Blob_Task::DoExecute(void)
{
    if (!CheckReplyStatus()) return;
    ReadReply();
    if (GetStatus() == eFailed) return;

    if (!m_MainBlobInfo && m_NextBlobInfo) {
        m_MainBlobInfo = m_NextBlobInfo;
        m_MainBlobId = m_NextBlobId;
        CRef<CPsgBlobId> psg_main_id(new CPsgBlobId(m_MainBlobId));
        m_DataLoaderBlobId = CDataLoader::TBlobId(psg_main_id);
        m_PsgBlobInfo = m_BlobMap.FindBlob(m_MainBlobId);
        if (!m_PsgBlobInfo) {
            m_PsgBlobInfo = make_shared<SPsgBlobInfo>(*m_MainBlobInfo);
            m_BlobMap.AddBlob(m_MainBlobId, m_PsgBlobInfo);
        }
    }

    SetStatus(eCompleted);
}


void CPSG_Blob_Task::ProcessReplyItem(shared_ptr<CPSG_ReplyItem> item)
{
    EPSG_Status status = item->GetStatus(0);
    if (status != EPSG_Status::eSuccess && status != EPSG_Status::eInProgress) {
        ReportStatus(item, status);
        SetStatus(eFailed);
        return;
    }

    switch (item->GetType()) {
    case CPSG_ReplyItem::eBioseqInfo:
    {
        // Only one bioseq-info is allowed per reply.
        _ASSERT(!m_BioseqInfo);
        if (status == EPSG_Status::eInProgress) {
            status = item->GetStatus(CDeadline::eInfinite);
        }
        if (status != EPSG_Status::eSuccess) {
            ReportStatus(item, status);
            SetStatus(eFailed);
            return;
        }
        m_BioseqInfo = static_pointer_cast<CPSG_BioseqInfo>(item);
        m_BioseqCache.Add(*m_BioseqInfo, m_Id);
        string id = m_BioseqInfo->GetBlobId().Get();
        _ASSERT(!m_MainBlobInfo || m_MainBlobId == id);
        m_MainBlobId = id;
        break;
    }
    case CPSG_ReplyItem::eBlobInfo:
    {
        if (status == EPSG_Status::eInProgress) {
            status = item->GetStatus(CDeadline::eInfinite);
        }
        if (status != EPSG_Status::eSuccess) {
            ReportStatus(item, status);
            SetStatus(eFailed);
            return;
        }
        auto blob_info = static_pointer_cast<CPSG_BlobInfo>(item);
        string id = blob_info->GetId().Get();
        string split_blob_id = blob_info->GetSplitInfoBlobId().Get();
        if (!split_blob_id.empty()) {
            // Got blob with split info reference - it's the main blob.
            _ASSERT(!m_MainBlobInfo);
            m_MainBlobInfo = blob_info;
            m_MainBlobId = id;
        }
        else {
            // No split-info, can be the main blob-info (unsplit) or split-info. Save for later.
            // Only one blob without split info is allowed.
            _ASSERT(!m_NextBlobInfo);
            m_NextBlobInfo = blob_info;
            m_NextBlobId = id;
        }
        break;
    }
    case CPSG_ReplyItem::eBlobData:
    {
        m_BlobData = static_pointer_cast<CPSG_BlobData>(item);
        break;
    }
    case CPSG_ReplyItem::eSkippedBlob:
    {
        if (status == EPSG_Status::eInProgress) {
            status = item->GetStatus(CDeadline::eInfinite);
        }
        if (status != EPSG_Status::eSuccess) {
            ReportStatus(item, status);
            SetStatus(eFailed);
            return;
        }
        m_Skipped = static_pointer_cast<CPSG_SkippedBlob>(item);
        break;
    }
    default:
    {
        break;
    }
    }

    if (m_NextBlobInfo) {
        // Try to identify blob-info.
        if (m_MainBlobInfo) {
            // This is split blob-info.
            m_SplitBlobInfo = m_NextBlobInfo;
            m_NextBlobInfo.reset();
        }
        else if (!m_MainBlobId.empty()) {
            if (m_NextBlobId == m_MainBlobId) {
                m_MainBlobInfo = m_NextBlobInfo;
            }
            else {
                m_SplitBlobInfo = m_NextBlobInfo;
            }
            m_NextBlobInfo.reset();
        }
    }

    if (!m_DataLoaderBlobId && !m_MainBlobId.empty()) {
        CRef<CPsgBlobId> psg_main_id(new CPsgBlobId(m_MainBlobId));
        m_DataLoaderBlobId = CDataLoader::TBlobId(psg_main_id);
    }

    if (!m_MainBlobInfo) return;

    // Find or create main blob-info entry.
    if (!m_PsgBlobInfo) {
        m_PsgBlobInfo = m_BlobMap.FindBlob(m_MainBlobId);
        if (!m_PsgBlobInfo) {
            m_PsgBlobInfo = make_shared<SPsgBlobInfo>(*m_MainBlobInfo);
            m_BlobMap.AddBlob(m_MainBlobId, m_PsgBlobInfo);
        }
    }
}


CPSGDataLoader_Impl::SReplyResult CPSGDataLoader_Impl::x_ProcessBlobReply(
    shared_ptr<CPSG_Reply> reply,
    CDataSource* data_source,
    CSeq_id_Handle req_idh)
{
    SReplyResult ret;

    if (!reply) {
        _TRACE("Request failed: null reply");
        return move(ret);
    }

    CPSG_TaskGroup group(*m_ThreadPool);
    CRef<CPSG_Blob_Task> task(
        new CPSG_Blob_Task(reply, group, req_idh, *m_BlobMap, *m_BioseqCache));
    group.AddTask(task);
    group.WaitAll();

    ret.blob_id = task->m_MainBlobId;
    if (!data_source) return move(ret);

    if (task->m_Skipped) {
        CPSG_SkippedBlob::EReason skip_reason = task->m_Skipped->GetReason();
        switch (skip_reason) {
        case CPSG_SkippedBlob::eInProgress:
        {
            // Try to wait for the blob to be loaded.
            ret.lock = data_source->GetLoadedTSE_Lock(task->m_DataLoaderBlobId, CTimeout::eInfinite);
            break;
        }
        case CPSG_SkippedBlob::eExcluded:
        case CPSG_SkippedBlob::eSent:
            // Check if the blob is already loaded, force loading if necessary.
            ret.lock = data_source->GetTSE_LoadLock(task->m_DataLoaderBlobId);
            break;
        default: // unknown
            return move(ret);
        }
        if (ret.lock && ret.lock.IsLoaded()) return move(ret);
        // Request blob by blob-id.
        CPSG_BlobId req_blob_id(task->m_SplitBlobInfo ? task->m_SplitBlobInfo->GetId().Get() : task->m_MainBlobId);
        auto context = make_shared<CPsgClientContext>();
        auto blob_request = make_shared<CPSG_Request_Blob>(req_blob_id, kEmptyStr, context);
        blob_request->IncludeData(m_NoSplit ? CPSG_Request_Biodata::eOrigTSE : CPSG_Request_Biodata::eSmartTSE);
        auto blob_reply = x_ProcessRequest(blob_request);
        return x_ProcessBlobReply(blob_reply, data_source, req_idh);
    }

    if (!task->m_MainBlobInfo) {
        _TRACE("Failed to get blob info for " << ret.blob_id);
        return move(ret);
    }
    // Read blob data (if any) and pass to the data source.
    if (!task->m_BlobData) {
        _TRACE("Failed to get blob data for " << ret.blob_id);
        return move(ret);
    }

    if (!ret.lock) {
        ret.lock = data_source->GetTSE_LoadLock(task->m_DataLoaderBlobId);
    }
    if (ret.lock.IsLoaded()) return move(ret);
    shared_ptr<CPSG_BlobInfo> blob_info = task->m_SplitBlobInfo ? task->m_SplitBlobInfo : task->m_MainBlobInfo;
    x_ReadBlobData(*task->m_PsgBlobInfo, *blob_info, *task->m_BlobData, ret.lock);
    return move(ret);
}


class CPSG_BioseqInfo_Task : public CPSG_Task
{
public:
    CPSG_BioseqInfo_Task(TReply reply, CPSG_TaskGroup& group)
        : CPSG_Task(reply, group) {}

    ~CPSG_BioseqInfo_Task(void) override {}

    shared_ptr<CPSG_BioseqInfo> m_BioseqInfo;

protected:
    void DoExecute(void) override {
        if (!CheckReplyStatus()) return;
        ReadReply();
        if (GetStatus() == eFailed) return;
        SetStatus(eCompleted);
    }

    void ProcessReplyItem(shared_ptr<CPSG_ReplyItem> item) override {
        EPSG_Status status = item->GetStatus(CDeadline::eInfinite);
        if (status != EPSG_Status::eSuccess) {
            ReportStatus(item, status);
            SetStatus(eFailed);
            return;
        }
        if (item->GetType() == CPSG_ReplyItem::eBioseqInfo) {
            m_BioseqInfo = static_pointer_cast<CPSG_BioseqInfo>(item);
        }
    }
};


shared_ptr<SPsgBioseqInfo> CPSGDataLoader_Impl::x_GetBioseqInfo(const CSeq_id_Handle& idh)
{
    shared_ptr<SPsgBioseqInfo> ret = m_BioseqCache->Get(idh);
    if (ret) return ret;

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
    group.AddTask(task);
    group.WaitAll();

    if (task->GetStatus() != CThreadPool_Task::eCompleted || !task->m_BioseqInfo) {
        _TRACE("Failed to get bioseq info for seq-id " << idh.AsString());
        return nullptr;
    }

    return m_BioseqCache->Add(*task->m_BioseqInfo, idh);
}


CTSE_LoadLock CPSGDataLoader_Impl::x_LoadBlob(const SPsgBlobInfo& psg_blob_info, CDataSource& data_source)
{
    CRef<CPsgBlobId> psg_main_id(new CPsgBlobId(psg_blob_info.blob_id_main));
    CDataLoader::TBlobId main_id(psg_main_id);
    CTSE_LoadLock load_lock = data_source.GetTSE_LoadLock(main_id);
    if (load_lock.IsLoaded()) return load_lock;

    const string& psg_blob_id = psg_blob_info.GetDataBlobId();

    shared_ptr<CPSG_BlobInfo> blob_info;
    shared_ptr<CPSG_BlobData> blob_data;
    x_GetBlobInfoAndData(psg_blob_id, blob_info, blob_data);

    if (!blob_info || !blob_data) {
        _TRACE("Failed to get blob info or data for blob-id " << psg_blob_id);
        return load_lock;
    }
    x_ReadBlobData(psg_blob_info, *blob_info, *blob_data, load_lock);
    return load_lock;
}


class CPSG_BlobInfoAndData_Task : public CPSG_Task
{
public:
    CPSG_BlobInfoAndData_Task(TReply reply, CPSG_TaskGroup& group)
        : CPSG_Task(reply, group) {}

    ~CPSG_BlobInfoAndData_Task(void) override {}

    shared_ptr<CPSG_BlobInfo> m_BlobInfo;
    shared_ptr<CPSG_BlobData> m_BlobData;

protected:
    void DoExecute(void) override {
        if (!CheckReplyStatus()) return;
        ReadReply();
        if (GetStatus() == eFailed) return;
        SetStatus(eCompleted);
    }

    void ProcessReplyItem(shared_ptr<CPSG_ReplyItem> item) override {
        EPSG_Status status = item->GetStatus(CDeadline::eInfinite);
        if (status != EPSG_Status::eSuccess) {
            ReportStatus(item, status);
            SetStatus(eFailed);
            return;
        }
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
};


void CPSGDataLoader_Impl::x_GetBlobInfoAndData(
    const string& psg_blob_id,
    shared_ptr<CPSG_BlobInfo>& blob_info,
    shared_ptr<CPSG_BlobData>& blob_data)
{
    CPSG_BlobId blob_id(psg_blob_id);
    auto context = make_shared<CPsgClientContext>();
    auto request = make_shared<CPSG_Request_Blob>(blob_id, kEmptyStr, context);
    auto reply = x_ProcessRequest(request);
    if (!reply) {
        _TRACE("Request failed: null reply");
        return;
    }

    CPSG_TaskGroup group(*m_ThreadPool);
    CRef<CPSG_BlobInfoAndData_Task> task(new CPSG_BlobInfoAndData_Task(reply, group));
    group.AddTask(task);
    group.WaitAll();

    if (task->GetStatus() == CThreadPool_Task::eCompleted) {
        blob_info = task->m_BlobInfo;
        blob_data = task->m_BlobData;
    }
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
        tasks[task] = idx;
        group.AddTask(task);
    }
    while (group.HasTasks()) {
        CRef<CPSG_BioseqInfo_Task> task = group.GetTask<CPSG_BioseqInfo_Task>();
        if (!task) {
            ERR_POST("Failed to load bulk bioseq info: null task.");
            return;
        }
        TTasks::const_iterator it = tasks.find(task);
        if (it == tasks.end()) {
            ERR_POST("Failed to load bulk bioseq info: unknown task.");
            return;
        }
        ret[it->second] = make_shared<SPsgBioseqInfo>(*task->m_BioseqInfo);
        loaded[it->second] = true;
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // HAVE_PSG_LOADER
