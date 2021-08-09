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
#include <objects/seqsplit/ID2S_Feat_type_Info.hpp>
#include <objects/general/Dbtag.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/tse_loadlock.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/tse_split_info.hpp>
#include <objmgr/impl/split_parser.hpp>
#include <objmgr/data_loader_factory.hpp>
#include <objmgr/annot_selector.hpp>
#include <objtools/data_loaders/genbank/impl/psg_loader_impl.hpp>
#include <objtools/data_loaders/genbank/impl/wgsmaster.hpp>
#include <util/compress/compress.hpp>
#include <util/compress/stream.hpp>
#include <util/compress/zlib.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>
#include <util/thread_pool.hpp>
#include <sstream>

#if defined(HAVE_PSG_LOADER)

#define LOCK4GET 1

BEGIN_NCBI_SCOPE

//#define NCBI_USE_ERRCODE_X   PSGLoader
//NCBI_DEFINE_ERR_SUBCODE_X(1);

BEGIN_SCOPE(objects)

const int kSplitInfoChunkId = 999999999;

NCBI_PARAM_DECL(unsigned int, PSG_LOADER, DEBUG);
NCBI_PARAM_DEF_EX(unsigned int, PSG_LOADER, DEBUG, 1,
    eParam_NoThread, PSG_LOADER_DEBUG);
typedef NCBI_PARAM_TYPE(PSG_LOADER, DEBUG) TPSG_Debug;


static unsigned int s_GetDebugLevel()
{
    static auto value = TPSG_Debug::GetDefault();
    return value;
}

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


const unsigned int kMaxWaitSeconds = 3;
const unsigned int kMaxWaitMillisec = 0;

#define DEFAULT_DEADLINE CDeadline(kMaxWaitSeconds, kMaxWaitMillisec)

/////////////////////////////////////////////////////////////////////////////
// CPSGBioseqCache
/////////////////////////////////////////////////////////////////////////////


CSeq_id_Handle PsgIdToHandle(const CPSG_BioId& id)
{
    string sid = id.GetId();
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
    if (found != m_Ids.end()) {
        found->second->Update(info);
        return found->second;
    }
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
    : included_info(0),
      molecule_type(CSeq_inst::eMol_not_set),
      length(0),
      state(0),
      tax_id(INVALID_TAX_ID),
      hash(0),
      deadline(kMaxCacheLifespanSeconds)
{
    Update(bioseq_info);
}


SPsgBioseqInfo::TIncludedInfo SPsgBioseqInfo::Update(const CPSG_BioseqInfo& bioseq_info)
{
    TIncludedInfo inc_info = bioseq_info.IncludedInfo() & ~included_info;
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
        blob_id = bioseq_info.GetBlobId().GetId();

    included_info |= inc_info;
    return inc_info;
}


/////////////////////////////////////////////////////////////////////////////
// SPsgBlobInfo
/////////////////////////////////////////////////////////////////////////////


SPsgBlobInfo::SPsgBlobInfo(const CPSG_BlobInfo& blob_info)
    : blob_state(0)
{
    auto blob_id = blob_info.GetId<CPSG_BlobId>();
    _ASSERT(blob_id);
    blob_id_main = blob_id->GetId();
    id2_info = blob_info.GetId2Info();

    if (blob_info.IsDead()) blob_state |= CBioseq_Handle::fState_dead;
    if (blob_info.IsSuppressed()) blob_state |= CBioseq_Handle::fState_suppress_perm;
    if (blob_info.IsWithdrawn()) blob_state |= CBioseq_Handle::fState_withdrawn;

    auto lm = blob_id->GetLastModified(); // last_modified is in milliseconds
    last_modified = lm.IsNull()? 0: lm.GetValue();
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
        _TRACE("Request failed: " << sstatus << " - " << msg << " @ "<<CStackTrace());
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
    bool m_GotNoFound;
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

    void CancelAll(void)
    {
        {
            CFastMutexGuard guard(m_Mutex);
            for (CRef<CPSG_Task> task : m_Tasks) {
                task->RequestToCancel();
            }
        }
        WaitAll();
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
    : m_Reply(reply),
      m_Status(eIdle),
      m_GotNoFound(false),
      m_Group(group)
{
}


CPSG_Task::EStatus CPSG_Task::Execute(void)
{
    m_Status = eExecuting;
    try {
        DoExecute();
    }
    catch (CException& exc) {
        LOG_POST("CPSGDataLoader: exception in retrieval thread: "<<exc);
        return eFailed;
    }
    catch (exception& exc) {
        LOG_POST("CPSGDataLoader: exception in retrieval thread: "<<exc.what());
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
            if ( status == EPSG_Status::eNotFound ) {
                m_GotNoFound = true;
                continue;
            }
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
#define NCBI_PSGLOADER_ADD_WGS_MASTER "add_wgs_master"

NCBI_PARAM_DECL(string, PSG_LOADER, SERVICE_NAME);
NCBI_PARAM_DEF_EX(string, PSG_LOADER, SERVICE_NAME, "PSG2",
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
    unique_ptr<CPSGDataLoader::TParamTree> app_params;
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
    m_AddWGSMasterDescr = true;
    if ( psg_params ) {
        string param = CPSGDataLoader::GetParam(psg_params, NCBI_PSGLOADER_ADD_WGS_MASTER);
        if ( !param.empty() ) {
            try {
                m_AddWGSMasterDescr = NStr::StringToBool(param);
            }
            catch ( CException& exc ) {
                NCBI_RETHROW_FMT(exc, CLoaderException, eBadConfig,
                                 "Bad value of parameter "
                                 NCBI_PSGLOADER_ADD_WGS_MASTER
                                 ": \""<<param<<"\"");
            }
        }
    }

    m_Queue = make_shared<CPSG_Queue>(service_name);
}


static bool CannotProcess(const CSeq_id_Handle& sih)
{
    if ( !sih ) {
        return true;
    }
    if ( sih.Which() == CSeq_id::e_Local ) {
        return true;
    }
    if ( sih.Which() == CSeq_id::e_General ) {
        if ( NStr::EqualNocase(sih.GetSeqId()->GetGeneral().GetDb(), "SRA") ) {
            // SRA is good
            return false;
        }
        if ( NStr::StartsWith(sih.GetSeqId()->GetGeneral().GetDb(), "WGS:", NStr::eNocase) ) {
            // WGS is good
            return false;
        }
        // other general ids are good too(?)
        return false;
    }
    return false;
}


template<class Call>
typename std::result_of<Call()>::type
CPSGDataLoader_Impl::CallWithRetry(Call&& call,
                                   const char* name,
                                   int retry_count)
{
    if ( retry_count == 0 ) {
        retry_count = 4;
    }
    for ( int t = 1; t < retry_count; ++ t ) {
        try {
            return call();
        }
        catch ( CException& exc ) {
            LOG_POST(Warning<<"CPSGDataLoader::"<<name<<"() try "<<t<<" exception: "<<exc);
        }
        catch ( exception& exc ) {
            LOG_POST(Warning<<"CPSGDataLoader::"<<name<<"() try "<<t<<" exception: "<<exc.what());
        }
        catch ( ... ) {
            LOG_POST(Warning<<"CPSGDataLoader::"<<name<<"() try "<<t<<" exception");
        }
        double wait_sec = 1<<(t-1);
        LOG_POST(Warning<<"CPSGDataLoader: waiting "<<wait_sec<<"s before retry");
        SleepMilliSec(Uint4(wait_sec*1000));
    }
    return call();
}


void CPSGDataLoader_Impl::GetIds(const CSeq_id_Handle& idh, TIds& ids)
{
    CallWithRetry(bind(&CPSGDataLoader_Impl::GetIdsOnce, this,
                       cref(idh), ref(ids)),
                  "GetIds");
}


void CPSGDataLoader_Impl::GetIdsOnce(const CSeq_id_Handle& idh, TIds& ids)
{
    if ( CannotProcess(idh) ) {
        return;
    }
    auto seq_info = x_GetBioseqInfo(idh);
    if (!seq_info) return;

    ITERATE(SPsgBioseqInfo::TIds, it, seq_info->ids) {
        ids.push_back(*it);
    }
}


CDataLoader::SGiFound
CPSGDataLoader_Impl::GetGi(const CSeq_id_Handle& idh)
{
    return CallWithRetry(bind(&CPSGDataLoader_Impl::GetGiOnce, this,
                              cref(idh)),
                         "GetGi");
}


CDataLoader::SGiFound
CPSGDataLoader_Impl::GetGiOnce(const CSeq_id_Handle& idh)
{
    if ( CannotProcess(idh) ) {
        return CDataLoader::SGiFound();
    }
    CDataLoader::SGiFound ret;
    auto seq_info = x_GetBioseqInfo(idh);
    if (seq_info) {
        ret.sequence_found = true;
        if ( seq_info->gi != ZERO_GI ) {
            ret.gi = seq_info->gi;
        }
    }
    return ret;
}


CDataLoader::SAccVerFound
CPSGDataLoader_Impl::GetAccVer(const CSeq_id_Handle& idh)
{
    return CallWithRetry(bind(&CPSGDataLoader_Impl::GetAccVerOnce, this,
                              cref(idh)),
                         "GetAccVer");
}


CDataLoader::SAccVerFound
CPSGDataLoader_Impl::GetAccVerOnce(const CSeq_id_Handle& idh)
{
    if ( CannotProcess(idh) ) {
        return CDataLoader::SAccVerFound();
    }
    CDataLoader::SAccVerFound ret;
    auto seq_info = x_GetBioseqInfo(idh);
    if (seq_info) {
        ret.sequence_found = true;
        if ( seq_info->canonical.IsAccVer() ) {
            ret.acc_ver = seq_info->canonical;
        }
    }
    return ret;
}


TTaxId CPSGDataLoader_Impl::GetTaxId(const CSeq_id_Handle& idh)
{
    return CallWithRetry(bind(&CPSGDataLoader_Impl::GetTaxIdOnce, this,
                              cref(idh)),
                         "GetTaxId");
}


TTaxId CPSGDataLoader_Impl::GetTaxIdOnce(const CSeq_id_Handle& idh)
{
    if ( CannotProcess(idh) ) {
        return INVALID_TAX_ID;
    }
    auto seq_info = x_GetBioseqInfo(idh);
    return seq_info ? seq_info->tax_id : INVALID_TAX_ID;
}


TSeqPos CPSGDataLoader_Impl::GetSequenceLength(const CSeq_id_Handle& idh)
{
    return CallWithRetry(bind(&CPSGDataLoader_Impl::GetSequenceLengthOnce, this,
                              cref(idh)),
                         "GetSequenceLength");
}


TSeqPos CPSGDataLoader_Impl::GetSequenceLengthOnce(const CSeq_id_Handle& idh)
{
    if ( CannotProcess(idh) ) {
        return kInvalidSeqPos;
    }
    auto seq_info = x_GetBioseqInfo(idh);
    return (seq_info && seq_info->length > 0) ? TSeqPos(seq_info->length) : kInvalidSeqPos;
}


CDataLoader::SHashFound
CPSGDataLoader_Impl::GetSequenceHash(const CSeq_id_Handle& idh)
{
    return CallWithRetry(bind(&CPSGDataLoader_Impl::GetSequenceHashOnce, this,
                              cref(idh)),
                         "GetSequenceHash");
}


CDataLoader::SHashFound
CPSGDataLoader_Impl::GetSequenceHashOnce(const CSeq_id_Handle& idh)
{
    if ( CannotProcess(idh) ) {
        return CDataLoader::SHashFound();
    }
    CDataLoader::SHashFound ret;
    auto seq_info = x_GetBioseqInfo(idh);
    if (seq_info) {
        ret.sequence_found = true;
        if ( ret.hash ) {
            ret.hash_known = true;
            ret.hash = seq_info->hash;
        }
    }
    return ret;
}


CDataLoader::STypeFound
CPSGDataLoader_Impl::GetSequenceType(const CSeq_id_Handle& idh)
{
    return CallWithRetry(bind(&CPSGDataLoader_Impl::GetSequenceTypeOnce, this,
                              cref(idh)),
                         "GetSequenceType");
}


CDataLoader::STypeFound
CPSGDataLoader_Impl::GetSequenceTypeOnce(const CSeq_id_Handle& idh)
{
    if ( CannotProcess(idh) ) {
        return CDataLoader::STypeFound();
    }
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
    return CallWithRetry(bind(&CPSGDataLoader_Impl::GetSequenceStateOnce, this,
                              cref(idh)),
                         "GetSequenceState");
}


int CPSGDataLoader_Impl::GetSequenceStateOnce(const CSeq_id_Handle& idh)
{
    const int kNotFound = (CBioseq_Handle::fState_not_found |
                           CBioseq_Handle::fState_no_data);
    if ( CannotProcess(idh) ) {
        return kNotFound;
    }
    auto seq_info = x_GetBioseqInfo(idh);
    if (!seq_info) {
        return kNotFound;
    }
    if (seq_info->included_info & CPSG_Request_Resolve::fState) {
        switch (seq_info->state) {
        case CPSG_BioseqInfo::eDead:
            return CBioseq_Handle::fState_dead;
        case CPSG_BioseqInfo::eReserved:
            return CBioseq_Handle::fState_suppress_perm;
        case CPSG_BioseqInfo::eLive:
            return CBioseq_Handle::fState_none;
        default:
            LOG_POST(Warning << "CPSGDataLoader: uknown " << idh << " state: " << seq_info->state);
            return CBioseq_Handle::fState_none;
        }
    }
    return CBioseq_Handle::fState_none;
}


CDataLoader::TTSE_LockSet
CPSGDataLoader_Impl::GetRecords(CDataSource* data_source,
    const CSeq_id_Handle& idh,
    CDataLoader::EChoice choice)
{
    return CallWithRetry(bind(&CPSGDataLoader_Impl::GetRecordsOnce, this,
                              data_source, cref(idh), choice),
                         "GetRecords");
}


CDataLoader::TTSE_LockSet
CPSGDataLoader_Impl::GetRecordsOnce(CDataSource* data_source,
    const CSeq_id_Handle& idh,
    CDataLoader::EChoice choice)
{
    CDataLoader::TTSE_LockSet locks;
    if (choice == CDataLoader::eOrphanAnnot) {
        // PSG loader doesn't provide orphan annotations
        return locks;
    }
    if ( CannotProcess(idh) ) {
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
    CTSE_Lock tse_lock = x_ProcessBlobReply(reply, data_source, idh, true, true).lock;

    if (!tse_lock) {
        // TODO: return correct state with CBlobStateException
        if ( 0 ) {
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "error loading blob for " + idh.AsString());
        }
    }
    else {
        locks.insert(tse_lock);
    }
    return locks;
}


CRef<CPsgBlobId> CPSGDataLoader_Impl::GetBlobId(const CSeq_id_Handle& idh)
{
    return CallWithRetry(bind(&CPSGDataLoader_Impl::GetBlobIdOnce, this,
                              cref(idh)),
                         "GetBlobId");
}


CRef<CPsgBlobId> CPSGDataLoader_Impl::GetBlobIdOnce(const CSeq_id_Handle& idh)
{
    if ( CannotProcess(idh) ) {
        return null;
    }
    string blob_id;

    // Check cache first.
    auto seq_info = x_GetBioseqInfo(idh);
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


static bool x_IsLocalCDDEntryId(const CPsgBlobId& blob_id);
static bool x_ParseLocalCDDEntryId(const CPsgBlobId& blob_id,
                                   CSeq_id_Handle& gi, CSeq_id_Handle& acc_ver);
static CTSE_Lock x_CreateLocalCDDEntry(CDataSource* data_source,
                                       const CSeq_id_Handle& gi,
                                       const CSeq_id_Handle& acc_ver);



static bool s_GetBlobByIdShouldFail = false;

void CPSGDataLoader_Impl::SetGetBlobByIdShouldFail(bool value)
{
    s_GetBlobByIdShouldFail = value;
}


bool CPSGDataLoader_Impl::GetGetBlobByIdShouldFail()
{
    return s_GetBlobByIdShouldFail;
}


CTSE_Lock CPSGDataLoader_Impl::GetBlobById(CDataSource* data_source, const CPsgBlobId& blob_id)
{
    return CallWithRetry(bind(&CPSGDataLoader_Impl::GetBlobByIdOnce, this,
                              data_source, cref(blob_id)),
                         "GetBlobById",
                         GetGetBlobByIdShouldFail()? 1: 0);
}


CTSE_Lock CPSGDataLoader_Impl::GetBlobByIdOnce(CDataSource* data_source, const CPsgBlobId& blob_id)
{
    if (!data_source) return CTSE_Lock();

    if ( GetGetBlobByIdShouldFail() ) {
        _TRACE("GetBlobById("<<blob_id.ToPsgId()<<") should fail");
    }
#ifdef LOCK4GET
    CDataLoader::TBlobId dl_blob_id = CDataLoader::TBlobId(&blob_id);
    CTSE_LoadLock load_lock = data_source->GetTSE_LoadLock(dl_blob_id);
    if ( load_lock.IsLoaded() ) {
        _TRACE("GetBlobById() already loaded " << blob_id.ToPsgId());
        return load_lock;
    }
#else
    CTSE_LoadLock load_lock;
    {{
        CDataLoader::TBlobId dl_blob_id = CDataLoader::TBlobId(&blob_id);
        load_lock = data_source->GetTSE_LoadLockIfLoaded(dl_blob_id);
        if ( load_lock && load_lock.IsLoaded() ) {
            _TRACE("GetBlobById() already loaded " << blob_id.ToPsgId());
            return load_lock;
        }
    }}
#endif
    
    CTSE_Lock ret;
    if ( x_IsLocalCDDEntryId(blob_id) ) {
        if ( s_GetDebugLevel() >= 5 ) {
            LOG_POST(Info<<"PSG loader: Re-loading CDD blob: " << blob_id.ToString());
        }
        CSeq_id_Handle gi, acc_ver;
        if ( x_ParseLocalCDDEntryId(blob_id, gi, acc_ver) ) {
            ret = x_CreateLocalCDDEntry(data_source, gi, acc_ver);
        }
    }
    else {
        CPSG_BlobId bid(blob_id.ToPsgId());
        auto context = make_shared<CPsgClientContext>();
        auto request = make_shared<CPSG_Request_Blob>(bid, context);
        request->IncludeData(m_NoSplit ? CPSG_Request_Biodata::eOrigTSE : CPSG_Request_Biodata::eSmartTSE);
        auto reply = x_ProcessRequest(request);
        ret = x_ProcessBlobReply(reply, data_source, CSeq_id_Handle(), true, false, &load_lock).lock;
    }
    if (!ret) {
        _TRACE("Failed to load blob for " << blob_id.ToPsgId()<<" @ "<<CStackTrace());
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "CPSGDataLoader::GetBlobById("+blob_id.ToPsgId()+") failed");
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
        CPSGDataLoader_Impl& loader,
        bool lock_asap = false,
        CTSE_LoadLock* load_lock_ptr = nullptr)
        : CPSG_Task(reply, group),
        m_Id(idh),
        m_DataSource(data_source),
        m_Loader(loader),
        m_LockASAP(lock_asap),
        m_LoadLockPtr(load_lock_ptr)
    {
    }

    ~CPSG_Blob_Task(void) override {}

    struct SAutoReleaseLock {
        SAutoReleaseLock(bool lock_asap, CTSE_LoadLock*& lock_ptr)
            : m_LockPtr(lock_ptr)
            {
                if ( lock_asap && !m_LockPtr ) {
                    m_LockPtr = &m_LocalLock;
                }
            }
        ~SAutoReleaseLock()
            {
                m_LockPtr = 0;
            }

        CTSE_LoadLock m_LocalLock;
        CTSE_LoadLock*& m_LockPtr;
    };

    typedef pair<shared_ptr<CPSG_BlobInfo>, shared_ptr<CPSG_BlobData>> TBlobSlot;
    typedef map<string, TBlobSlot> TTSEBlobMap; // by PSG blob_id
    typedef map<string, map<TChunkId, TBlobSlot>> TChunkBlobMap; // by id2_info, id2_chunk

    CSeq_id_Handle m_Id;
    shared_ptr<CPSG_SkippedBlob> m_Skipped;
    CPSGDataLoader_Impl::SReplyResult m_ReplyResult;
    shared_ptr<SPsgBlobInfo> m_PsgBlobInfo;

    const TBlobSlot* GetTSESlot(const string& psg_id) const;
    const TBlobSlot* GetChunkSlot(const string& id2_info, TChunkId chunk_id) const;
    const TBlobSlot* GetBlobSlot(const CPSG_DataId& id) const;
    TBlobSlot* SetBlobSlot(const CPSG_DataId& id);
    CTSE_LoadLock& GetLoadLock() const
        {
            _ASSERT(m_LoadLockPtr);
            return *m_LoadLockPtr;
        }
    void ObtainLoadLock();
    bool GotBlobData(const string& psg_blob_id) const;
    CPSGDataLoader_Impl::SReplyResult WaitForSkipped(void);

    void Finish(void) override
    {
        m_Skipped.reset();
        m_ReplyResult = CPSGDataLoader_Impl::SReplyResult();
        m_PsgBlobInfo.reset();
        m_TSEBlobMap.clear();
        m_ChunkBlobMap.clear();
        m_BlobIdMap.clear();
    }

    void SetDLBlobId(const string& psg_blob_id, CDataLoader::TBlobId dl_blob_id)
    {
        m_BlobIdMap[psg_blob_id] = dl_blob_id;
    }
    
    CDataLoader::TBlobId GetDLBlobId(const string& psg_blob_id) const
    {
        auto iter = m_BlobIdMap.find(psg_blob_id);
        if ( iter != m_BlobIdMap.end() ) {
            return iter->second;
        }
        return CDataLoader::TBlobId(new CPsgBlobId(psg_blob_id));
    }

protected:
    void DoExecute(void) override;
    void ProcessReplyItem(shared_ptr<CPSG_ReplyItem> item) override;

private:
    CDataSource* m_DataSource;
    CPSGDataLoader_Impl& m_Loader;
    bool m_LockASAP;
    CTSE_LoadLock* m_LoadLockPtr;
    TTSEBlobMap m_TSEBlobMap;
    TChunkBlobMap m_ChunkBlobMap;
    map<string, CDataLoader::TBlobId> m_BlobIdMap;
};


const CPSG_Blob_Task::TBlobSlot* CPSG_Blob_Task::GetTSESlot(const string& blob_id) const
{
    auto iter = m_TSEBlobMap.find(blob_id);
    if ( iter != m_TSEBlobMap.end() ) {
        return &iter->second;
    }
    return 0;
}


const CPSG_Blob_Task::TBlobSlot* CPSG_Blob_Task::GetChunkSlot(const string& id2_info,
                                                              TChunkId chunk_id) const
{
    auto iter = m_ChunkBlobMap.find(id2_info);
    if ( iter != m_ChunkBlobMap.end() ) {
        auto iter2 = iter->second.find(chunk_id);
        if ( iter2 != iter->second.end() ) {
            return &iter2->second;
        }
    }
    return 0;
}


const CPSG_Blob_Task::TBlobSlot* CPSG_Blob_Task::GetBlobSlot(const CPSG_DataId& id) const
{
    if ( auto tse_id = dynamic_cast<const CPSG_BlobId*>(&id) ) {
        return GetTSESlot(tse_id->GetId());
    }
    else if ( auto chunk_id = dynamic_cast<const CPSG_ChunkId*>(&id) ) {
        return GetChunkSlot(chunk_id->GetId2Info(), chunk_id->GetId2Chunk());
    }
    return 0;
}


CPSG_Blob_Task::TBlobSlot* CPSG_Blob_Task::SetBlobSlot(const CPSG_DataId& id)
{
    if ( auto tse_id = dynamic_cast<const CPSG_BlobId*>(&id) ) {
        _TRACE("Blob slot for tse_id="<<tse_id->GetId());
        return &m_TSEBlobMap[tse_id->GetId()];
    }
    else if ( auto chunk_id = dynamic_cast<const CPSG_ChunkId*>(&id) ) {
        _TRACE("Blob slot for id2_info="<<chunk_id->GetId2Info()<<" chunk="<<chunk_id->GetId2Chunk());
        return &m_ChunkBlobMap[chunk_id->GetId2Info()][chunk_id->GetId2Chunk()];
    }
    return 0;
}


bool CPSG_Blob_Task::GotBlobData(const string& psg_blob_id) const
{
    const TBlobSlot* main_blob_slot = GetTSESlot(psg_blob_id);
    if ( !main_blob_slot || !main_blob_slot->first ) {
        // no TSE blob props yet
        if ( s_GetDebugLevel() >= 6 ) {
            LOG_POST("GotBlobData("<<psg_blob_id<<"): no TSE blob props");
        }
        return false;
    }
    if ( main_blob_slot->second ) {
        // got TSE blob data
        if ( s_GetDebugLevel() >= 6 ) {
            LOG_POST("GotBlobData("<<psg_blob_id<<"): got TSE blob data");
        }
        return true;
    }
    auto id2_info = main_blob_slot->first->GetId2Info();
    if ( id2_info.empty() ) {
        // TSE doesn't have split info
        if ( s_GetDebugLevel() >= 6 ) {
            LOG_POST("GotBlobData("<<psg_blob_id<<"): not split");
        }
        return false;
    }
    const TBlobSlot* split_blob_slot = GetChunkSlot(id2_info, kSplitInfoChunkId);
    if ( !split_blob_slot || !split_blob_slot->second ) {
        // no split info blob data yet
        if ( s_GetDebugLevel() >= 6 ) {
            LOG_POST("GotBlobData("<<psg_blob_id<<"): no split blob data");
        }
        return false;
    }
    else {
        // got split info blob data
        if ( s_GetDebugLevel() >= 6 ) {
            LOG_POST("GotBlobData("<<psg_blob_id<<"): got split blob data");
        }
        return true;
    }
}


void CPSG_Blob_Task::ObtainLoadLock()
{
    if ( !m_LockASAP ) {
        // load lock is not requested
        return;
    }
    if ( GetLoadLock() ) {
        // load lock already obtained
        return;
    }
    if ( m_ReplyResult.blob_id.empty() ) {
        // blob id is not known yet
        return;
    }
    if ( !GotBlobData(m_ReplyResult.blob_id) ) {
        return;
    }
    if ( s_GetDebugLevel() >= 6 ) {
        LOG_POST("ObtainLoadLock("<<m_ReplyResult.blob_id<<"): getting load lock");
    }
    GetLoadLock() = m_DataSource->GetTSE_LoadLock(GetDLBlobId(m_ReplyResult.blob_id));
    if ( s_GetDebugLevel() >= 6 ) {
        LOG_POST("ObtainLoadLock("<<m_ReplyResult.blob_id<<"): obtained load lock");
    }
}


void CPSG_Blob_Task::DoExecute(void)
{
    _TRACE("CPSG_Blob_Task::DoExecute()");
    if (!CheckReplyStatus()) return;
    SAutoReleaseLock lock_guard(m_LockASAP, m_LoadLockPtr);
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
        _TRACE("no blob_id");
        m_Status = eCompleted;
        return;
    }

    _TRACE("tse_id="<<m_ReplyResult.blob_id);
    if ( !m_LoadLockPtr ) {
        // to TSE requested
        m_Status = eCompleted;
        return;
    }

    const TBlobSlot* main_blob_slot = GetTSESlot(m_ReplyResult.blob_id);
    if ( !main_blob_slot || !main_blob_slot->first ) {
        _TRACE("No blob info for tse_id="<<m_ReplyResult.blob_id);
        m_Status = eFailed;
        return;
    }

    const TBlobSlot* split_blob_slot = 0;
    auto id2_info = main_blob_slot->first->GetId2Info();
    if ( !id2_info.empty() ) {
        split_blob_slot = GetChunkSlot(id2_info, kSplitInfoChunkId);
        if ( !split_blob_slot || !split_blob_slot->first ) {
            _TRACE("No split info tse_id="<<m_ReplyResult.blob_id<<" id2_info="<<id2_info);
        }
    }

    // Find or create main blob-info entry.
    m_PsgBlobInfo = m_Loader.m_BlobMap->FindBlob(m_ReplyResult.blob_id);
    if (!m_PsgBlobInfo) {
        m_PsgBlobInfo = make_shared<SPsgBlobInfo>(*main_blob_slot->first);
        m_Loader.m_BlobMap->AddBlob(m_ReplyResult.blob_id, m_PsgBlobInfo);
    }

    if (!m_DataSource) {
        _TRACE("No data source for tse_id="<<m_ReplyResult.blob_id);
        // No data to load, just bioseq-info.
        m_Status = eCompleted;
        return;
    }

    // Read blob data (if any) and pass to the data source.
    if ( CPSGDataLoader_Impl::GetGetBlobByIdShouldFail() ) {
        m_Status = eFailed;
        return;
    }
    CDataLoader::TBlobId dl_blob_id = GetDLBlobId(m_ReplyResult.blob_id);
    CTSE_LoadLock load_lock;
    if ( GetLoadLock() && GetLoadLock()->GetBlobId() == dl_blob_id ) {
        load_lock = GetLoadLock();
    }
    else {
        load_lock = m_DataSource->GetTSE_LoadLock(dl_blob_id);
    }
    if (!load_lock) {
        _TRACE("Cannot get TSE load lock for tse_id="<<m_ReplyResult.blob_id);
        m_Status = eFailed;
        return;
    }
    CPSGDataLoader_Impl::EMainChunkType main_chunk_type = CPSGDataLoader_Impl::eNoDelayedMainChunk;
    if ( load_lock.IsLoaded() ) {
        if ( load_lock->x_NeedsDelayedMainChunk() &&
             !load_lock->GetSplitInfo().GetChunk(kDelayedMain_ChunkId).IsLoaded() ) {
            main_chunk_type = CPSGDataLoader_Impl::eDelayedMainChunk;
        }
        else {
            _TRACE("Already loaded tse_id="<<m_ReplyResult.blob_id);
            m_ReplyResult.lock = load_lock;
            m_Status = eCompleted;
            return;
        }
    }

    if ( split_blob_slot && split_blob_slot->first && split_blob_slot->second ) {
        auto& blob_id = *load_lock->GetBlobId();
        dynamic_cast<CPsgBlobId&>(const_cast<CBlobId&>(blob_id)).SetId2Info(id2_info);
        m_Loader.x_ReadBlobData(*m_PsgBlobInfo,
                                *split_blob_slot->first,
                                *split_blob_slot->second,
                                load_lock,
                                CPSGDataLoader_Impl::eIsSplitInfo);
        CTSE_Split_Info& tse_split_info = load_lock->GetSplitInfo();
        for ( auto& chunk_slot : m_ChunkBlobMap[id2_info] ) {
            TChunkId chunk_id = chunk_slot.first;
            if ( chunk_id == kSplitInfoChunkId ) {
                continue;
            }
            if ( !chunk_slot.second.first || !chunk_slot.second.second ) {
                continue;
            }
            CTSE_Chunk_Info* chunk = 0;
            try {
                chunk = &tse_split_info.GetChunk(chunk_id);
            }
            catch ( CException& /*ignored*/ ) {
            }
            if ( !chunk || chunk->IsLoaded() ) {
                continue;
            }

            unique_ptr<CObjectIStream> in
                (CPSGDataLoader_Impl::GetBlobDataStream(*chunk_slot.second.first,
                                                        *chunk_slot.second.second));
            CRef<CID2S_Chunk> id2_chunk(new CID2S_Chunk);
            *in >> *id2_chunk;
            if ( s_GetDebugLevel() >= 8 ) {
                LOG_POST(Info<<"PSG loader: TSE "<<chunk->GetBlobId().ToString()<<" "<<
                         " chunk "<<chunk->GetChunkId()<<" "<<MSerial_AsnText<<*id2_chunk);
            }
            
            CSplitParser::Load(*chunk, *id2_chunk);
            chunk->SetLoaded();
        }
    }
    else if ( main_blob_slot && main_blob_slot->first && main_blob_slot->second ) {
        m_Loader.x_ReadBlobData(*m_PsgBlobInfo,
                                *main_blob_slot->first,
                                *main_blob_slot->second,
                                load_lock,
                                CPSGDataLoader_Impl::eNoSplitInfo);
    }
    else {
        _TRACE("No data for tse_id="<<m_ReplyResult.blob_id);
        load_lock.Reset();
    }
    if ( load_lock ) {
        m_Loader.x_SetLoaded(load_lock, main_chunk_type);
        m_ReplyResult.lock = load_lock;
        m_Status = eCompleted;
    }
    else {
        m_Status = eFailed;
    }
}


void CPSG_Blob_Task::ProcessReplyItem(shared_ptr<CPSG_ReplyItem> item)
{
    switch (item->GetType()) {
    case CPSG_ReplyItem::eBioseqInfo:
    {
        // Only one bioseq-info is allowed per reply.
        shared_ptr<CPSG_BioseqInfo> bioseq_info = static_pointer_cast<CPSG_BioseqInfo>(item);
        m_ReplyResult.blob_id = bioseq_info->GetBlobId().GetId();
        ObtainLoadLock();
        m_Loader.m_BioseqCache->Add(*bioseq_info, m_Id);
        break;
    }
    case CPSG_ReplyItem::eBlobInfo:
    {
        auto blob_info = static_pointer_cast<CPSG_BlobInfo>(item);
        _TRACE("Blob info: "<<blob_info->GetId()->Repr());
        if ( auto slot = SetBlobSlot(*blob_info->GetId()) ) {
            slot->first = blob_info;
            ObtainLoadLock();
        }
        break;
    }
    case CPSG_ReplyItem::eBlobData:
    {
        shared_ptr<CPSG_BlobData> data = static_pointer_cast<CPSG_BlobData>(item);
        _TRACE("Blob data: "<<data->GetId()->Repr());
        if ( auto slot = SetBlobSlot(*data->GetId()) ) {
            slot->second = data;
            ObtainLoadLock();
        }
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

    CDataLoader::TBlobId dl_blob_id = GetDLBlobId(ret.blob_id);
    CTSE_LoadLock load_lock;
    _ASSERT(m_Skipped);
    CPSG_SkippedBlob::EReason skip_reason = m_Skipped->GetReason();
    switch (skip_reason) {
    case CPSG_SkippedBlob::eInProgress:
        // Try to wait for the blob to be loaded.
        load_lock = m_DataSource->GetLoadedTSE_Lock(dl_blob_id, CTimeout(1));
        if ( !load_lock && s_GetDebugLevel() >= 6 ) {
            LOG_POST("CPSGDataLoader: 'in progress' blob is not loaded: "<<dl_blob_id.ToString());
        }
        break;
    case CPSG_SkippedBlob::eSent:
        // Try to wait for the blob to be loaded.
        load_lock = m_DataSource->GetLoadedTSE_Lock(dl_blob_id, CTimeout(.2));
        if ( !load_lock && s_GetDebugLevel() >= 6 ) {
            LOG_POST("CPSGDataLoader: 'sent' blob is not loaded: "<<dl_blob_id.ToString());
        }
        break;
    case CPSG_SkippedBlob::eExcluded:
        // Check if the blob is already loaded, force loading if necessary.
        load_lock = m_DataSource->GetTSE_LoadLockIfLoaded(dl_blob_id);
        if ( !load_lock && s_GetDebugLevel() >= 6 ) {
            LOG_POST("CPSGDataLoader: 'excluded' blob is not loaded: "<<dl_blob_id.ToString());
        }
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
    CallWithRetry(bind(&CPSGDataLoader_Impl::GetBlobsOnce, this,
                       data_source, ref(tse_sets)),
                  "GetBlobs");
}


void CPSGDataLoader_Impl::GetBlobsOnce(CDataSource* data_source, TTSE_LockSets& tse_sets)
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
            new CPSG_Blob_Task(reply, group, id, data_source, *this, true));
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
            _TRACE("Failed to get blob for " << task->m_Id);
            group.CancelAll();
            NCBI_THROW(CLoaderException, eLoaderFailed, "failed to load blobs for "+task->m_Id.AsString());
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


void CPSGDataLoader_Impl::LoadChunk(CDataSource* data_source,
                                    CTSE_Chunk_Info& chunk_info)
{
    CDataLoader::TChunkSet chunks;
    chunks.push_back(Ref(&chunk_info));
    LoadChunks(data_source, chunks);
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
    unique_ptr<CObjectIStream> in(CPSGDataLoader_Impl::GetBlobDataStream(*m_BlobInfo, *m_BlobData));
    if (!in.get()) {
        _TRACE("Failed to open chunk data stream for blob-id " << m_BlobInfo->GetId()->Repr());
        m_Status = eFailed;
        return;
    }

    CRef<CID2S_Chunk> id2_chunk(new CID2S_Chunk);
    *in >> *id2_chunk;
    if ( s_GetDebugLevel() >= 8 ) {
        LOG_POST(Info<<"PSG loader: TSE "<<m_Chunk->GetBlobId().ToString()<<" "<<
                 " chunk "<<m_Chunk->GetChunkId()<<" "<<MSerial_AsnText<<*id2_chunk);
    }
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


const char kCDDAnnotName[] = "CDD";
const bool kCreateLocalCDDEntries = true;
const char kLocalCDDEntryIdPrefix[] = "CDD:";
const char kLocalCDDEntryIdSeparator = '|';

static string x_MakeLocalCDDEntryId(const CSeq_id_Handle& gi, const CSeq_id_Handle& acc_ver)
{
    ostringstream str;
    _ASSERT(gi && gi.IsGi());
    str << kLocalCDDEntryIdPrefix << gi.GetGi();
    if ( acc_ver ) {
        str << kLocalCDDEntryIdSeparator << acc_ver;
    }
    return str.str();
}


static bool x_IsLocalCDDEntryId(const CPsgBlobId& blob_id)
{
    return NStr::StartsWith(blob_id.ToPsgId(), kLocalCDDEntryIdPrefix);
}


static bool x_ParseLocalCDDEntryId(const CPsgBlobId& blob_id,
                                   CSeq_id_Handle& gi, CSeq_id_Handle& acc_ver)
{
    if ( !x_IsLocalCDDEntryId(blob_id) ) {
        return false;
    }
    istringstream str(blob_id.ToPsgId().substr(strlen(kLocalCDDEntryIdPrefix)));
    TIntId gi_id = 0;
    str >> gi_id;
    if ( !gi_id ) {
        return false;
    }
    gi = CSeq_id_Handle::GetGiHandle(GI_FROM(TIntId, gi_id));
    if ( str.get() == kLocalCDDEntryIdSeparator ) {
        string extra;
        str >> extra;
        acc_ver = CSeq_id_Handle::GetHandle(extra);
    }
    return true;
}


static CPSG_BioId x_LocalCDDEntryIdToBioId(const CPsgBlobId& blob_id)
{
    const string& str = blob_id.ToPsgId();
    size_t start = strlen(kLocalCDDEntryIdPrefix);
    size_t end = str.find(kLocalCDDEntryIdSeparator, start);
    return { str.substr(start, end-start), CSeq_id::e_Gi };
}


static CRef<CTSE_Chunk_Info> x_CreateLocalCDDEntryChunk(const CSeq_id_Handle& id1,
                                                        const CSeq_id_Handle& id2)
{
    if ( !id1 && !id2 ) {
        return null;
    }
    CRange<TSeqPos> range = CRange<TSeqPos>::GetWhole();
    CRef<CTSE_Chunk_Info> chunk(new CTSE_Chunk_Info(kDelayedMain_ChunkId));
    // add main annot types
    CAnnotName name = kCDDAnnotName;
    CSeqFeatData::ESubtype subtypes[] = {
        CSeqFeatData::eSubtype_region,
        CSeqFeatData::eSubtype_site
    };
    for ( auto subtype : subtypes ) {
        SAnnotTypeSelector type(subtype);
        if ( id1 ) {
            chunk->x_AddAnnotType(name, type, id1, range);
        }
        if ( id2 ) {
            chunk->x_AddAnnotType(name, type, id2, range);
        }
    }
    return chunk;
}


static CTSE_Lock x_CreateLocalCDDEntry(CDataSource* data_source,
                                       const CSeq_id_Handle& gi,
                                       const CSeq_id_Handle& acc_ver)
{
    CRef<CPsgBlobId> blob_id(new CPsgBlobId(x_MakeLocalCDDEntryId(gi, acc_ver)));
    if ( auto chunk = x_CreateLocalCDDEntryChunk(gi, acc_ver) ) {
        CDataLoader::TBlobId dl_blob_id = CDataLoader::TBlobId(blob_id);
        CTSE_LoadLock load_lock = data_source->GetTSE_LoadLock(dl_blob_id);
        if ( load_lock ) {
            if ( !load_lock.IsLoaded() ) {
                if ( CPSGDataLoader_Impl::GetGetBlobByIdShouldFail() ) {
                    return CTSE_Lock();
                }
                load_lock->SetName(kCDDAnnotName);
                load_lock->GetSplitInfo().AddChunk(*chunk);
                _ASSERT(load_lock->x_NeedsDelayedMainChunk());
                load_lock.SetLoaded();
            }
            return load_lock;
        }
    }
    return CTSE_Lock();
}


static void x_CreateEmptyLocalCDDEntry(CDataSource* data_source,
                                       CDataLoader::TChunk chunk)
{
    CTSE_LoadLock load_lock = data_source->GetTSE_LoadLock(chunk->GetBlobId());
    _ASSERT(load_lock);
    _ASSERT(load_lock.IsLoaded());
    _ASSERT(load_lock->HasNoSeq_entry());
    CRef<CSeq_entry> entry(new CSeq_entry);
    entry->SetSet().SetSeq_set();
    if ( s_GetDebugLevel() >= 8 ) {
        LOG_POST(Info<<"PSG loader: TSE "<<load_lock->GetBlobId().ToString()<<" "<<
                 " created empty CDD entry");
    }
    load_lock->SetSeq_entry(*entry);
    chunk->SetLoaded();
}


static bool s_SameId(const CPSG_BlobId* id1, const CPSG_BlobId& id2)
{
    return id1 && id1->GetId() == id2.GetId();
}


bool CPSGDataLoader_Impl::x_ReadCDDChunk(CDataSource* data_source,
                                         CDataLoader::TChunk chunk,
                                         const CPSG_BlobInfo& blob_info,
                                         const CPSG_BlobData& blob_data)
{
    _ASSERT(chunk->GetChunkId() == kDelayedMain_ChunkId);
    _DEBUG_ARG(const CPsgBlobId& blob_id = dynamic_cast<const CPsgBlobId&>(*chunk->GetBlobId()));
    _ASSERT(x_IsLocalCDDEntryId(blob_id));
    _ASSERT(!chunk->IsLoaded());
    
    CTSE_LoadLock load_lock = data_source->GetTSE_LoadLock(chunk->GetBlobId());
    if ( !load_lock ||
         !load_lock.IsLoaded() ||
         !load_lock->x_NeedsDelayedMainChunk() ) {
        _TRACE("Cannot make CDD entry because of wrong TSE state id="<<blob_id.ToString());
        return false;
    }
    
    unique_ptr<CObjectIStream> in(GetBlobDataStream(blob_info, blob_data));
    if (!in.get()) {
        _TRACE("Failed to open blob data stream for blob-id " << blob_id.ToString());
        return false;
    }

    CRef<CSeq_entry> entry(new CSeq_entry);
    *in >> *entry;
    if ( s_GetDebugLevel() >= 8 ) {
        LOG_POST(Info<<"PSG loader: TSE "<<load_lock->GetBlobId().ToString()<<" "<<
                 MSerial_AsnText<<*entry);
    }
    load_lock->SetSeq_entry(*entry);
    chunk->SetLoaded();
    return true;
}


shared_ptr<CPSG_Request_Blob>
CPSGDataLoader_Impl::x_MakeLoadLocalCDDEntryRequest(CDataSource* data_source,
                                                    CDataLoader::TChunk chunk,
                                                    shared_ptr<CPsgClientContext_Bulk> context)
{
    _ASSERT(chunk->GetChunkId() == kDelayedMain_ChunkId);
    const CPsgBlobId& blob_id = dynamic_cast<const CPsgBlobId&>(*chunk->GetBlobId());
    _ASSERT(x_IsLocalCDDEntryId(blob_id));
    _ASSERT(!chunk->IsLoaded());
    
    bool failed = false;
    shared_ptr<CPSG_NamedAnnotInfo> cdd_info;
    
    // load CDD blob id
    {{
        CPSG_BioId bio_id = x_LocalCDDEntryIdToBioId(blob_id);
        CPSG_Request_NamedAnnotInfo::TAnnotNames names = { kCDDAnnotName };
        _ASSERT(bio_id.GetId().find('|') == NPOS);
        auto request = make_shared<CPSG_Request_NamedAnnotInfo>(bio_id, names, context);
        request->IncludeData(CPSG_Request_Biodata::eSmartTSE);
        auto reply = x_ProcessRequest(request);
        shared_ptr<CPSG_BioseqInfo> bioseq_info;
        shared_ptr<CPSG_BlobInfo> blob_info;
        shared_ptr<CPSG_BlobData> blob_data;
        for (;;) {
            auto reply_item = reply->GetNextItem(DEFAULT_DEADLINE);
            if (!reply_item) continue;
            if (reply_item->GetType() == CPSG_ReplyItem::eEndOfReply) break;
            EPSG_Status status = reply_item->GetStatus(0);
            if (status != EPSG_Status::eSuccess && status != EPSG_Status::eInProgress) {
                ReportStatus(reply_item, status);
                if ( status == EPSG_Status::eNotFound ) {
                    continue;
                }
                failed = true;
                break;
            }
            if (status == EPSG_Status::eInProgress) {
                status = reply_item->GetStatus(CDeadline::eInfinite);
            }
            if (status != EPSG_Status::eSuccess) {
                ReportStatus(reply_item, status);
                failed = true;
                break;
            }
            if (reply_item->GetType() == CPSG_ReplyItem::eBioseqInfo) {
                bioseq_info = static_pointer_cast<CPSG_BioseqInfo>(reply_item);
            }
            if (reply_item->GetType() == CPSG_ReplyItem::eNamedAnnotInfo) {
                auto na_info = static_pointer_cast<CPSG_NamedAnnotInfo>(reply_item);
                if ( NStr::EqualNocase(na_info->GetName(), kCDDAnnotName) ) {
                    cdd_info = na_info;
                }
            }
            if (reply_item->GetType() == CPSG_ReplyItem::eBlobInfo) {
                blob_info = static_pointer_cast<CPSG_BlobInfo>(reply_item);
            }
            if (reply_item->GetType() == CPSG_ReplyItem::eBlobData) {
                blob_data = static_pointer_cast<CPSG_BlobData>(reply_item);
            }
        }
        if ( failed ) {
            // TODO
            x_CreateEmptyLocalCDDEntry(data_source, chunk);
            return nullptr;
        }
        if ( !cdd_info ) {
            x_CreateEmptyLocalCDDEntry(data_source, chunk);
            return nullptr;
        }
        // see if we got blob already
        if ( blob_info && s_SameId(blob_info->GetId<CPSG_BlobId>(), cdd_info->GetBlobId()) &&
             blob_data && s_SameId(blob_data->GetId<CPSG_BlobId>(), cdd_info->GetBlobId()) ) {
            _TRACE("Got CDD entry: "<<cdd_info->GetBlobId().Repr());
            if ( x_ReadCDDChunk(data_source, chunk, *blob_info, *blob_data) ) {
                return nullptr;
            }
        }
    }}
    
    // load CDD blob request
    return make_shared<CPSG_Request_Blob>(cdd_info->GetBlobId(), context);
}


void CPSGDataLoader_Impl::LoadChunks(CDataSource* data_source,
                                     const CDataLoader::TChunkSet& chunks)
{
    CallWithRetry(bind(&CPSGDataLoader_Impl::LoadChunksOnce, this,
                       data_source, cref(chunks)),
                  "LoadChunks");
}


void CPSGDataLoader_Impl::LoadChunksOnce(CDataSource* data_source,
                                     const CDataLoader::TChunkSet& chunks)
{
    if (chunks.empty()) return;

    typedef map<void*, CDataLoader::TChunk> TChunkMap;
    TChunkMap chunk_map;
    auto context = make_shared<CPsgClientContext_Bulk>();
    ITERATE(CDataLoader::TChunkSet, it, chunks) {
        const CTSE_Chunk_Info& chunk = **it;
        if ( chunk.IsLoaded() ) {
            continue;
        }
        if ( chunk.GetChunkId() == kMasterWGS_ChunkId ) {
            CWGSMasterSupport::LoadWGSMaster(data_source->GetDataLoader(), *it);
            continue;
        }
        if ( chunk.GetChunkId() == kDelayedMain_ChunkId ) {
            const CPsgBlobId& blob_id = dynamic_cast<const CPsgBlobId&>(*chunk.GetBlobId());
            shared_ptr<CPSG_Request_Blob> request;
            if ( x_IsLocalCDDEntryId(blob_id) ) {
                request = x_MakeLoadLocalCDDEntryRequest(data_source, *it, context);
                if ( !request ) {
                    continue;
                }
            }
            else {
                request = make_shared<CPSG_Request_Blob>(blob_id.ToPsgId(), context);
            }
            request->IncludeData(CPSG_Request_Biodata::eSmartTSE);
            chunk_map[request.get()] = *it;
            x_SendRequest(request);
        }
        else {
            const CPsgBlobId& blob_id = dynamic_cast<const CPsgBlobId&>(*chunk.GetBlobId());
            auto request = make_shared<CPSG_Request_Chunk>(CPSG_ChunkId(chunk.GetChunkId(),
                                                                        blob_id.GetId2Info()),
                                                           context);
            chunk_map[request.get()] = *it;
            x_SendRequest(request);
        }
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
        if ( chunk->GetChunkId() == kDelayedMain_ChunkId ) {
            CRef<CPSG_Blob_Task> task(new CPSG_Blob_Task(reply, group, CSeq_id_Handle(), data_source, *this, true));
            task->SetDLBlobId(dynamic_cast<const CPSG_Request_Blob&>(*reply->GetRequest()).GetBlobId().GetId(),
                              chunk->GetBlobId());
            guards.push_back(make_shared<CPSG_Task_Guard>(*task));
            group.AddTask(task);
        }
        else {
            CRef<CPSG_LoadChunk_Task> task(new CPSG_LoadChunk_Task(reply, group, chunk));
            guards.push_back(make_shared<CPSG_Task_Guard>(*task));
            group.AddTask(task);
        }
    }
    group.WaitAll();
    // check if all chunks are loaded
    ITERATE(CDataLoader::TChunkSet, it, chunks) {
        const CTSE_Chunk_Info & chunk = **it;
        if (!chunk.IsLoaded()) {
            _TRACE("Failed to load chunk " << chunk.GetChunkId() << " of " << dynamic_cast<const CPsgBlobId&>(*chunk.GetBlobId()).ToPsgId());
            NCBI_THROW(CLoaderException, eLoaderFailed, "failed to load some chunks");
        }
    }
}


class CPSG_AnnotRecordsNA_Task : public CPSG_Task
{
public:
    CPSG_AnnotRecordsNA_Task( TReply reply, CPSG_TaskGroup& group)
        : CPSG_Task(reply, group) {}

    ~CPSG_AnnotRecordsNA_Task(void) override {}

    shared_ptr<CPSG_BioseqInfo> m_BioseqInfo;
    list<shared_ptr<CPSG_NamedAnnotInfo>> m_AnnotInfo;

    void Finish(void) override {
        m_BioseqInfo.reset();
        m_AnnotInfo.clear();
    }

protected:
    void ProcessReplyItem(shared_ptr<CPSG_ReplyItem> item) override {
        if (item->GetType() == CPSG_ReplyItem::eBioseqInfo) {
            m_BioseqInfo = static_pointer_cast<CPSG_BioseqInfo>(item);
        }
        if (item->GetType() == CPSG_ReplyItem::eNamedAnnotInfo) {
            m_AnnotInfo.push_back(static_pointer_cast<CPSG_NamedAnnotInfo>(item));
        }
    }
};

class CPSG_AnnotRecordsCDD_Task : public CPSG_Task
{
public:
    CPSG_AnnotRecordsCDD_Task( TReply reply, CPSG_TaskGroup& group)
        : CPSG_Task(reply, group) {}

    ~CPSG_AnnotRecordsCDD_Task(void) override {}

    shared_ptr<CPSG_BioseqInfo> m_BioseqInfo;
    list<shared_ptr<CPSG_NamedAnnotInfo>> m_AnnotInfo;

    void Finish(void) override {
        m_BioseqInfo.reset();
        m_AnnotInfo.clear();
    }

protected:
    void ProcessReplyItem(shared_ptr<CPSG_ReplyItem> item) override {
        if (item->GetType() == CPSG_ReplyItem::eBioseqInfo) {
            m_BioseqInfo = static_pointer_cast<CPSG_BioseqInfo>(item);
        }
        if (item->GetType() == CPSG_ReplyItem::eNamedAnnotInfo) {
            m_AnnotInfo.push_back(static_pointer_cast<CPSG_NamedAnnotInfo>(item));
        }
    }
};

static
pair<CRef<CTSE_Chunk_Info>, string>
s_CreateNAChunk(const CPSG_NamedAnnotInfo& psg_annot_info,
                const CPSG_BioseqInfo* bioseq_info)
{
    pair<CRef<CTSE_Chunk_Info>, string> ret;
    CRef<CTSE_Chunk_Info> chunk(new CTSE_Chunk_Info(kDelayedMain_ChunkId));
    unsigned main_count = 0;
    unsigned zoom_count = 0;
    // detailed annot info
    set<string> names;
    for ( auto& annot_info_ref : psg_annot_info.GetId2AnnotInfoList() ) {
        if ( s_GetDebugLevel() >= 8 ) {
            LOG_POST(Info<<"PSG loader: NA info "<<MSerial_AsnText<<*annot_info_ref);
        }
        const CID2S_Seq_annot_Info& annot_info = *annot_info_ref;
        // create special external annotations blob
        CAnnotName name(annot_info.GetName());
        if ( name.IsNamed() && !ExtractZoomLevel(name.GetName(), 0, 0) ) {
            //setter.GetTSE_LoadLock()->SetName(name);
            names.insert(name.GetName());
            ++main_count;
        }
        else {
            ++zoom_count;
        }
            
        vector<SAnnotTypeSelector> types;
        if ( annot_info.IsSetAlign() ) {
            types.push_back(SAnnotTypeSelector(CSeq_annot::C_Data::e_Align));
        }
        if ( annot_info.IsSetGraph() ) {
            types.push_back(SAnnotTypeSelector(CSeq_annot::C_Data::e_Graph));
        }
        if ( annot_info.IsSetFeat() ) {
            for ( auto feat_type_info_iter : annot_info.GetFeat() ) {
                const CID2S_Feat_type_Info& finfo = *feat_type_info_iter;
                int feat_type = finfo.GetType();
                if ( feat_type == 0 ) {
                    types.push_back(SAnnotTypeSelector
                                    (CSeq_annot::C_Data::e_Seq_table));
                }
                else if ( !finfo.IsSetSubtypes() ) {
                    types.push_back(SAnnotTypeSelector
                                    (CSeqFeatData::E_Choice(feat_type)));
                }
                else {
                    for ( auto feat_subtype : finfo.GetSubtypes() ) {
                        types.push_back(SAnnotTypeSelector
                                        (CSeqFeatData::ESubtype(feat_subtype)));
                    }
                }
            }
        }
            
        CTSE_Chunk_Info::TLocationSet loc;
        CSplitParser::x_ParseLocation(loc, annot_info.GetSeq_loc());
            
        ITERATE ( vector<SAnnotTypeSelector>, it, types ) {
            chunk->x_AddAnnotType(name, *it, loc);
        }
    }
    if ( names.size() == 1 ) {
        ret.second = *names.begin();
    }
    if ( s_GetDebugLevel() >= 5 ) {
        LOG_POST(Info<<"PSG loader: TSE "<<psg_annot_info.GetBlobId().GetId()<<
                 " annots: "<<ret.second<<" "<<main_count<<"+"<<zoom_count);
    }
    ret.first = chunk;
    return ret;
}


CDataLoader::TTSE_LockSet CPSGDataLoader_Impl::GetAnnotRecordsNA(
    CDataSource* data_source,
    const CSeq_id_Handle& idh,
    const SAnnotSelector* sel,
    CDataLoader::TProcessedNAs* processed_nas)
{
    return CallWithRetry(bind(&CPSGDataLoader_Impl::GetAnnotRecordsNAOnce, this,
                              data_source, cref(idh), sel, processed_nas),
                         "GetAnnotRecordsNA");
}


CDataLoader::TTSE_LockSet CPSGDataLoader_Impl::GetAnnotRecordsNAOnce(
    CDataSource* data_source,
    const CSeq_id_Handle& idh,
    const SAnnotSelector* sel,
    CDataLoader::TProcessedNAs* processed_nas)
{
    CDataLoader::TTSE_LockSet locks;
    if ( !data_source ) {
        return locks;
    }
    if ( CannotProcess(idh) ) {
        return locks;
    }
    if ( sel && sel->IsIncludedAnyNamedAnnotAccession() ) {
        CPSG_BioId bio_id = x_GetBioId(idh);
        CPSG_Request_NamedAnnotInfo::TAnnotNames annot_names;
        const SAnnotSelector::TNamedAnnotAccessions& accs = sel->GetNamedAnnotAccessions();
        ITERATE(SAnnotSelector::TNamedAnnotAccessions, it, accs) {
            if ( kCreateLocalCDDEntries && NStr::EqualNocase(it->first, kCDDAnnotName) ) {
                // CDDs are added as external annotations
                continue;
            }
            annot_names.push_back(it->first);
        }
        auto context = make_shared<CPsgClientContext>();
        //_ASSERT(PsgIdToHandle(bio_id));
        auto request = make_shared<CPSG_Request_NamedAnnotInfo>(move(bio_id), annot_names, context);
        if ( auto reply = x_ProcessRequest(request) ) {
            CPSG_TaskGroup group(*m_ThreadPool);
            CRef<CPSG_AnnotRecordsNA_Task> task(new CPSG_AnnotRecordsNA_Task(reply, group));
            CPSG_Task_Guard guard(*task);
            group.AddTask(task);
            group.WaitAll();

            if (task->GetStatus() == CThreadPool_Task::eCompleted) {
                for ( auto& info : task->m_AnnotInfo ) {
                    CDataLoader::SetProcessedNA(info->GetName(), processed_nas);
                    CRef<CPsgBlobId> blob_id(new CPsgBlobId(info->GetBlobId().GetId()));
                    auto chunk_info = s_CreateNAChunk(*info, task->m_BioseqInfo.get());
                    if ( chunk_info.first ) {
                        CDataLoader::TBlobId dl_blob_id = CDataLoader::TBlobId(blob_id);
                        CTSE_LoadLock load_lock = data_source->GetTSE_LoadLock(dl_blob_id);
                        if ( load_lock ) {
                            if ( !load_lock.IsLoaded() ) {
                                if ( !chunk_info.second.empty() ) {
                                    load_lock->SetName(chunk_info.second);
                                }
                                load_lock->GetSplitInfo().AddChunk(*chunk_info.first);
                                _ASSERT(load_lock->x_NeedsDelayedMainChunk());
                                load_lock.SetLoaded();
                            }
                            locks.insert(load_lock);
                        }
                    }
                    else {
                        // no annot info
                        if ( auto tse_lock = GetBlobById(data_source, *blob_id) ) {
                            locks.insert(tse_lock);
                        }
                    }
                }
            }
            else {
                _TRACE("Failed to load annotations for " << idh.AsString());
            }
        }
    }
    if ( kCreateLocalCDDEntries ) {
        CSeq_id_Handle gi;
        CSeq_id_Handle acc_ver;
        bool is_protein = true;
        TIds ids;
        GetIds(idh, ids);
        for ( auto id : ids ) {
            if ( id.IsGi() ) {
                gi = id;
                continue;
            }
            if ( id.Which() == CSeq_id::e_Pdb ) {
                if ( !acc_ver ) {
                    acc_ver = id;
                }
                continue;
            }
            auto seq_id = id.GetSeqId();
            if ( auto text_id = seq_id->GetTextseq_Id() ) {
                auto acc_type = seq_id->IdentifyAccession();
                if ( acc_type & CSeq_id::fAcc_nuc ) {
                    is_protein = false;
                    break;
                }
                else if ( text_id->IsSetAccession() && text_id->IsSetVersion() &&
                          (acc_type & CSeq_id::fAcc_prot) ) {
                    is_protein = true;
                    acc_ver = CSeq_id_Handle::GetHandle(text_id->GetAccession()+'.'+
                                                        NStr::NumericToString(text_id->GetVersion()));
                }
            }
        }
        if ( is_protein && gi ) {
            if ( auto tse_lock = x_CreateLocalCDDEntry(data_source, gi, acc_ver) ) {
                locks.insert(tse_lock);
            }
        }
    }
    return locks;
}


void CPSGDataLoader_Impl::DropTSE(const CPsgBlobId& blob_id)
{
    m_BlobMap->DropBlob(blob_id);
}


void CPSGDataLoader_Impl::GetAccVers(const TIds& ids, TLoaded& loaded, TIds& ret)
{
    CallWithRetry(bind(&CPSGDataLoader_Impl::GetAccVersOnce, this,
                       cref(ids), ref(loaded), ref(ret)),
                  "GetAccVers",
                  6);
}


void CPSGDataLoader_Impl::GetAccVersOnce(const TIds& ids, TLoaded& loaded, TIds& ret)
{
    vector<shared_ptr<SPsgBioseqInfo>> infos;
    infos.resize(ret.size());
    auto counts = x_GetBulkBioseqInfo(CPSG_Request_Resolve::fCanonicalId, ids, loaded, infos);
    if ( counts.first ) {
        // have loaded infos
        for (size_t i = 0; i < infos.size(); ++i) {
            if (loaded[i] || !infos[i].get()) continue;
            CSeq_id_Handle idh = infos[i]->canonical;
            if (idh.IsAccVer()) {
                ret[i] = idh;
            }
            loaded[i] = true;
        }
    }
    if ( counts.second ) {
        NCBI_THROW(CLoaderException, eLoaderFailed, "failed to load some acc.ver in bulk request");
    }
}


void CPSGDataLoader_Impl::GetGis(const TIds& ids, TLoaded& loaded, TGis& ret)
{
    CallWithRetry(bind(&CPSGDataLoader_Impl::GetGisOnce, this,
                       cref(ids), ref(loaded), ref(ret)),
                  "GetAccVers",
                  8);
}


void CPSGDataLoader_Impl::GetGisOnce(const TIds& ids, TLoaded& loaded, TGis& ret)
{
    vector<shared_ptr<SPsgBioseqInfo>> infos;
    infos.resize(ret.size());
    auto counts = x_GetBulkBioseqInfo(CPSG_Request_Resolve::fGi, ids, loaded, infos);
    if ( counts.first ) {
        // have loaded infos
        for (size_t i = 0; i < infos.size(); ++i) {
            if (loaded[i] || !infos[i].get()) continue;
            ret[i] = infos[i]->gi;
            loaded[i] = true;
        }
    }
    if ( counts.second ) {
        NCBI_THROW(CLoaderException, eLoaderFailed, "failed to load some acc.ver in bulk request");
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
    auto context = request->GetUserContext<CPsgClientContext>();
    auto reply = m_Queue->SendRequestAndGetReply(request, DEFAULT_DEADLINE);
    context->SetReply(reply);
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
#ifdef LOCK4GET
    CDataLoader::TBlobId dl_blob_id(new CPsgBlobId(blob_id));
    CTSE_LoadLock load_lock = data_source->GetTSE_LoadLock(dl_blob_id);
    if ( load_lock.IsLoaded() ) {
        _TRACE("x_RetryBlobRequest() already loaded " << blob_id);
        SReplyResult ret;
        ret.lock = load_lock;
        ret.blob_id = blob_id;
        return ret;
    }
#else
    CTSE_LoadLock load_lock;
    {{
        CDataLoader::TBlobId dl_blob_id(new CPsgBlobId(blob_id));
        CTSE_LoadLock load_lock = data_source->GetTSE_LoadLockIfLoaded(dl_blob_id);
        if ( load_lock && load_lock.IsLoaded() ) {
            _TRACE("x_RetryBlobRequest() already loaded " << blob_id);
            SReplyResult ret;
            ret.lock = load_lock;
            ret.blob_id = blob_id;
            return ret;
        }
    }}
#endif
    
    CPSG_BlobId req_blob_id(blob_id);
    auto context = make_shared<CPsgClientContext>();
    auto blob_request = make_shared<CPSG_Request_Blob>(req_blob_id, context);
    blob_request->IncludeData(m_NoSplit ? CPSG_Request_Biodata::eOrigTSE : CPSG_Request_Biodata::eSmartTSE);
    auto blob_reply = x_ProcessRequest(blob_request);
    return x_ProcessBlobReply(blob_reply, data_source, req_idh, false, false, &load_lock);
}


CPSGDataLoader_Impl::SReplyResult CPSGDataLoader_Impl::x_ProcessBlobReply(
    shared_ptr<CPSG_Reply> reply,
    CDataSource* data_source,
    CSeq_id_Handle req_idh,
    bool retry,
    bool lock_asap,
    CTSE_LoadLock* load_lock)
{
    SReplyResult ret;

    if (!reply) {
        _TRACE("Request failed: null reply");
        return ret;
    }

    CPSG_TaskGroup group(*m_ThreadPool);
    CRef<CPSG_Blob_Task> task(
        new CPSG_Blob_Task(reply, group, req_idh, data_source, *this, lock_asap, load_lock));
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
    else if ( !GetGetBlobByIdShouldFail() &&
              (lock_asap || load_lock) && !task->m_ReplyResult.blob_id.empty() ) {
        // blob is required, but not received, yet blob_id is known, so we retry
        ret = x_RetryBlobRequest(task->m_ReplyResult.blob_id, data_source, req_idh);
        if ( !ret.lock ) {
            _TRACE("Failed to load blob for " << req_idh.AsString()<<" @ "<<CStackTrace());
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "CPSGDataLoader::GetRecords("+req_idh.AsString()+") failed");
        }
    }
    else {
        _TRACE("Failed to load blob for " << req_idh.AsString()<<" @ "<<CStackTrace());
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "CPSGDataLoader::GetRecords("+req_idh.AsString()+") failed");
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
        NCBI_THROW(CLoaderException, eLoaderFailed, "null reply for "+idh.AsString());
        return nullptr;
    }

    CPSG_TaskGroup group(*m_ThreadPool);
    CRef<CPSG_BioseqInfo_Task> task(new CPSG_BioseqInfo_Task(reply, group));
    CPSG_Task_Guard guard(*task);
    group.AddTask(task);
    group.WaitAll();

    if (task->GetStatus() != CThreadPool_Task::eCompleted) {
        _TRACE("Failed to get bioseq info for " << idh.AsString() << " @ "<<CStackTrace());
        NCBI_THROW(CLoaderException, eLoaderFailed, "failed to get bioseq info for "+idh.AsString());
    }
    if (!task->m_BioseqInfo) {
        _TRACE("No bioseq info for " << idh.AsString());
        return nullptr;
    }

    return m_BioseqCache->Add(*task->m_BioseqInfo, idh);
}


void CPSGDataLoader_Impl::x_ReadBlobData(
    const SPsgBlobInfo& psg_blob_info,
    const CPSG_BlobInfo& blob_info,
    const CPSG_BlobData& blob_data,
    CTSE_LoadLock& load_lock,
    ESplitInfoType split_info_type)
{
    if ( !load_lock.IsLoaded() ) {
        load_lock->SetBlobVersion(psg_blob_info.GetBlobVersion());
        load_lock->SetBlobState(psg_blob_info.blob_state);
    }

    unique_ptr<CObjectIStream> in(GetBlobDataStream(blob_info, blob_data));
    if (!in.get()) {
        _TRACE("Failed to open blob data stream for blob-id " << blob_info.GetId()->Repr());
        return;
    }

    if ( split_info_type == eIsSplitInfo ) {
        CRef<CID2S_Split_Info> split_info(new CID2S_Split_Info);
        *in >> *split_info;
        if ( s_GetDebugLevel() >= 8 ) {
            LOG_POST(Info<<"PSG loader: TSE "<<load_lock->GetBlobId().ToString()<<" "<<
                     MSerial_AsnText<<*split_info);
        }
        CSplitParser::Attach(*load_lock, *split_info);
    }
    else {
        CRef<CSeq_entry> entry(new CSeq_entry);
        *in >> *entry;
        if ( s_GetDebugLevel() >= 8 ) {
            LOG_POST(Info<<"PSG loader: TSE "<<load_lock->GetBlobId().ToString()<<" "<<
                     MSerial_AsnText<<*entry);
        }
        load_lock->SetSeq_entry(*entry);
    }
    if ( m_AddWGSMasterDescr ) {
        CWGSMasterSupport::AddWGSMaster(load_lock);
    }
}


void CPSGDataLoader_Impl::x_SetLoaded(CTSE_LoadLock& load_lock,
                                      EMainChunkType main_chunk_type)
{
    if ( main_chunk_type == eDelayedMainChunk ) {
        load_lock->GetSplitInfo().GetChunk(kDelayedMain_ChunkId).SetLoaded();
        //_ASSERT(!load_lock->x_NeedsDelayedMainChunk());
    }
    else {
        load_lock.SetLoaded();
    }
}


CObjectIStream* CPSGDataLoader_Impl::GetBlobDataStream(
    const CPSG_BlobInfo& blob_info,
    const CPSG_BlobData& blob_data)
{
    istream& data_stream = blob_data.GetStream();
    CNcbiIstream* in = &data_stream;
    unique_ptr<CNcbiIstream> z_stream;
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


pair<size_t, size_t> CPSGDataLoader_Impl::x_GetBulkBioseqInfo(
    CPSG_Request_Resolve::EIncludeInfo info,
    const TIds& ids,
    const TLoaded& loaded,
    TBioseqInfos& ret)
{
    pair<size_t, size_t> counts(0, 0);
    TIdxMap idx_map;
    auto context = make_shared<CPsgClientContext_Bulk>();
    for (size_t i = 0; i < ids.size(); ++i) {
        if (loaded[i]) continue;
        if ( CannotProcess(ids[i]) ) {
            continue;
        }
        ret[i] = m_BioseqCache->Get(ids[i]);
        if (ret[i]) {
            counts.first += 1;
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
            counts.second += 1;
            continue;
        }
        if (!task->m_BioseqInfo) {
            _TRACE("No bioseq info for " << ids[it->second].AsString());
            // not loaded and no failure
            continue;
        }
        _ASSERT(task->m_BioseqInfo);
        ret[it->second] = make_shared<SPsgBioseqInfo>(*task->m_BioseqInfo);
        counts.first += 1;
    }
    return counts;
}


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // HAVE_PSG_LOADER
