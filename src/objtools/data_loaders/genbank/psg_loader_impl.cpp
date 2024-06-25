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
#include <corelib/ncbi_url.hpp>
#include <objects/seqsplit/ID2S_Split_Info.hpp>
#include <objects/seqsplit/ID2S_Chunk.hpp>
#include <objects/seqsplit/ID2S_Feat_type_Info.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/general/Dbtag.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/tse_loadlock.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/tse_split_info.hpp>
#include <objmgr/impl/split_parser.hpp>
#include <objmgr/data_loader_factory.hpp>
#include <objmgr/annot_selector.hpp>
#include <objtools/data_loaders/genbank/impl/psg_loader_impl.hpp>
#include <objtools/data_loaders/genbank/gbloader_params.h>
#include <objtools/data_loaders/genbank/impl/wgsmaster.hpp>
#include <objtools/data_loaders/genbank/reader.hpp>
#include <util/compress/compress.hpp>
#include <util/compress/stream.hpp>
#include <util/compress/zlib.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>
#include <serial/iterator.hpp>
#include <util/thread_pool.hpp>
#include <sstream>

#if defined(HAVE_PSG_LOADER)

//#define LOCK4GET 1
#define GLOBAL_CHUNKS 1

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
    try {
        return CSeq_id_Handle::GetHandle(sid);
    }
    catch (exception& exc) {
        ERR_POST("CPSGDataLoader: cannot parse Seq-id "<<sid<<": "<<exc.what());
    }
    return CSeq_id_Handle();
}


const int kDefaultCacheLifespanSeconds = 2*3600;
const size_t kDefaultMaxCacheSize = 10000;
const unsigned int kDefaultRetryCount = 4;
const unsigned int kDefaultBulkRetryCount = 8;

#define DEFAULT_WAIT_TIME 1
#define DEFAULT_WAIT_TIME_MULTIPLIER 1.5
#define DEFAULT_WAIT_TIME_INCREMENT 1
#define DEFAULT_WAIT_TIME_MAX 30

static CIncreasingTime::SAllParams s_WaitTimeParams = {
    {
        "wait_time",
        0,
        DEFAULT_WAIT_TIME
    },
    {
        "wait_time_max",
        0,
        DEFAULT_WAIT_TIME_MAX
    },
    {
        "wait_time_multiplier",
        0,
        DEFAULT_WAIT_TIME_MULTIPLIER
    },
    {
        "wait_time_increment",
        0,
        DEFAULT_WAIT_TIME_INCREMENT
    }
};


class CPSGBioseqCache
{
public:
    CPSGBioseqCache(int lifespan, size_t max_size)
        : m_Lifespan(lifespan), m_MaxSize(max_size) {}
    ~CPSGBioseqCache(void) {}

    shared_ptr<SPsgBioseqInfo> Get(const CSeq_id_Handle& idh);
    shared_ptr<SPsgBioseqInfo> Add(const CPSG_BioseqInfo& info, CSeq_id_Handle req_idh);

private:
    typedef map<CSeq_id_Handle, shared_ptr<SPsgBioseqInfo> > TIdMap;
    typedef list<shared_ptr<SPsgBioseqInfo> > TInfoQueue;

    mutable CFastMutex m_Mutex;
    int m_Lifespan;
    size_t m_MaxSize;
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
    if (ret->deadline.IsExpired()) {
        ITERATE(SPsgBioseqInfo::TIds, id, ret->ids) {
            m_Ids.erase(*id);
        }
        return nullptr;
    }
    ret->deadline = CDeadline(m_Lifespan);
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
        if (!found->second->deadline.IsExpired()) {
            found->second->Update(info);
            return found->second;
        }
        ITERATE(SPsgBioseqInfo::TIds, id, found->second->ids) {
            m_Ids.erase(*id);
        }
        m_Infos.remove(found->second);
    }
    while (!m_Infos.empty() && (m_Infos.size() > m_MaxSize || m_Infos.front()->deadline.IsExpired())) {
        auto rm = m_Infos.front();
        m_Infos.pop_front();
        ITERATE(SPsgBioseqInfo::TIds, id, rm->ids) {
            m_Ids.erase(*id);
        }
    }
    // Create new entry.
    shared_ptr<SPsgBioseqInfo> ret = make_shared<SPsgBioseqInfo>(info, m_Lifespan);
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
// CPSGAnnotInfoCache
/////////////////////////////////////////////////////////////////////////////

struct SPsgAnnotInfo
{
    typedef CDataLoader::TIds TIds;
    typedef list<shared_ptr<CPSG_NamedAnnotInfo>> TInfos;

    SPsgAnnotInfo(const string& _name,
                  const TIds& _ids,
                  const TInfos& _infos,
                  int lifespan);

    string name;
    TIds ids;
    TInfos infos;
    CDeadline deadline;

private:
    SPsgAnnotInfo(const SPsgAnnotInfo&);
    SPsgAnnotInfo& operator=(const SPsgAnnotInfo&);
};


class CPSGAnnotCache
{
public:
    CPSGAnnotCache(int lifespan, size_t max_size)
        : m_Lifespan(lifespan), m_MaxSize(max_size) {}
    ~CPSGAnnotCache(void) {}

    typedef CDataLoader::TIds TIds;

    shared_ptr<SPsgAnnotInfo> Get(const string& name, const CSeq_id_Handle& idh);
    shared_ptr<SPsgAnnotInfo> Add(const SPsgAnnotInfo::TInfos& infos,
                                  const string& name,
                                  const TIds& ids);

private:
    typedef map<CSeq_id_Handle, shared_ptr<SPsgAnnotInfo>> TIdMap;
    typedef map<string, TIdMap> TNameMap;
    typedef list<shared_ptr<SPsgAnnotInfo>> TInfoQueue;

    mutable CFastMutex m_Mutex;
    int m_Lifespan;
    size_t m_MaxSize;
    TNameMap m_NameMap;
    TInfoQueue m_Infos;
};


SPsgAnnotInfo::SPsgAnnotInfo(
    const string& _name,
    const TIds& _ids,
    const TInfos& _infos,
    int lifespan)
    : name(_name), ids(_ids), infos(_infos), deadline(lifespan)
{
}


shared_ptr<SPsgAnnotInfo> CPSGAnnotCache::Get(const string& name, const CSeq_id_Handle& idh)
{
    CFastMutexGuard guard(m_Mutex);
    TNameMap::iterator found_name = m_NameMap.find(name);
    if (found_name == m_NameMap.end()) return nullptr;
    TIdMap& ids = found_name->second;
    TIdMap::iterator found_id = ids.find(idh);
    if (found_id == ids.end()) return nullptr;
    shared_ptr<SPsgAnnotInfo> ret = found_id->second;
    m_Infos.remove(ret);
    if (ret->deadline.IsExpired()) {
        for (auto& id : ret->ids) {
            ids.erase(id);
        }
        if (ids.empty()) {
            m_NameMap.erase(found_name);
        }
        return nullptr;
    }
    ret->deadline = CDeadline(m_Lifespan);
    m_Infos.push_back(ret);
    return ret;
}


shared_ptr<SPsgAnnotInfo> CPSGAnnotCache::Add(
    const SPsgAnnotInfo::TInfos& infos,
    const string& name,
    const TIds& ids)
{
    if (name.empty() || ids.empty()) return nullptr;
    CFastMutexGuard guard(m_Mutex);
    // Try to find an existing entry (though this should not be a common case).
    TNameMap::iterator found_name = m_NameMap.find(name);
    if (found_name != m_NameMap.end()) {
        TIdMap& idmap = found_name->second;
        TIdMap::iterator found = idmap.find(ids.front());
        if (found != idmap.end()) {
            if (!found->second->deadline.IsExpired()) {
                return found->second;
            }
            for (auto& id : found->second->ids) {
                idmap.erase(id);
            }
            if (idmap.empty()) {
                m_NameMap.erase(found_name);
            }
        }
    }
    while (!m_Infos.empty() && (m_Infos.size() > m_MaxSize || m_Infos.front()->deadline.IsExpired())) {
        auto rm = m_Infos.front();
        m_Infos.pop_front();
        TNameMap::iterator found_name = m_NameMap.find(rm->name);
        _ASSERT(found_name != m_NameMap.end());
        for (auto& id : rm->ids) {
            found_name->second.erase(id);
        }
        if (found_name->second.empty() && found_name->first != name) {
            m_NameMap.erase(found_name);
        }
    }
    // Create new entry.
    shared_ptr<SPsgAnnotInfo> ret = make_shared<SPsgAnnotInfo>(name, ids, infos, m_Lifespan);
    m_Infos.push_back(ret);
    TIdMap& idmap = m_NameMap[name];
    for (auto& id : ids) {
        idmap[id] = ret;
    }
    return ret;
}


/////////////////////////////////////////////////////////////////////////////
// CPSGCache_Base
/////////////////////////////////////////////////////////////////////////////

template<class TK, class TV>
class CPSGCache_Base
{
public:
    typedef TK TKey;
    typedef TV TValue;
    typedef CPSGCache_Base<TKey, TValue> TParent;

    CPSGCache_Base(int lifespan, size_t max_size, TValue def_val = TValue(nullptr))
        : m_Default(def_val), m_Lifespan(lifespan), m_MaxSize(max_size) {}

    TValue Find(const TKey& key) {
        CFastMutexGuard guard(m_Mutex);
        x_Expire();
        auto found = m_Values.find(key);
        return found != m_Values.end() ? found->second.value : m_Default;
    }
    
    void Add(const TKey& key, const TValue& value) {
        CFastMutexGuard guard(m_Mutex);
        auto iter = m_Values.lower_bound(key);
        if ( iter != m_Values.end() && key == iter->first ) {
            // erase old value
            x_Erase(iter++);
        }
        // insert
        iter = m_Values.insert(iter,
            typename TValues::value_type(key, SNode(value, m_Lifespan)));
        iter->second.remove_list_iterator = m_RemoveList.insert(m_RemoveList.end(), iter);
        x_LimitSize();
    }

protected:
    // Map blob-id to blob info
    typedef TKey key_type;
    typedef TValue mapped_type;
    struct SNode;
    typedef map<key_type, SNode> TValues;
    typedef typename TValues::iterator TValueIter;
    typedef list<TValueIter> TRemoveList;
    typedef typename TRemoveList::iterator TRemoveIter;
    struct SNode {
        SNode(const mapped_type& value, unsigned lifespan)
            : value(value),
              deadline(lifespan)
            {}
        mapped_type value;
        CDeadline deadline;
        TRemoveIter remove_list_iterator;
    };

    void x_Erase(TValueIter iter) {
        m_RemoveList.erase(iter->second.remove_list_iterator);
        m_Values.erase(iter);
    }
    void x_Expire() {
        while ( !m_RemoveList.empty() &&
                m_RemoveList.front()->second.deadline.IsExpired() ) {
            x_PopFront();
        }
    }
    void x_LimitSize() {
        while ( m_Values.size() > m_MaxSize ) {
            x_PopFront();
        }
    }
    void x_PopFront() {
        _ASSERT(!m_RemoveList.empty());
        _ASSERT(m_RemoveList.front() != m_Values.end());
        _ASSERT(m_RemoveList.front()->second.remove_list_iterator == m_RemoveList.begin());
        m_Values.erase(m_RemoveList.front());
        m_RemoveList.pop_front();
    }

    TValue m_Default;
    CFastMutex m_Mutex;
    int m_Lifespan;
    size_t m_MaxSize;
    TValues m_Values;
    TRemoveList m_RemoveList;
};


/////////////////////////////////////////////////////////////////////////////
// CPSGCDDInfoCache
/////////////////////////////////////////////////////////////////////////////

class CPSGCDDInfoCache : public CPSGCache_Base<string, bool>
{
public:
    CPSGCDDInfoCache(int lifespan, size_t max_size)
        : TParent(lifespan, max_size, false) {}
};


/////////////////////////////////////////////////////////////////////////////
// CPSGBlobMap
/////////////////////////////////////////////////////////////////////////////


class CPSGBlobMap : public CPSGCache_Base<string, shared_ptr<SPsgBlobInfo>>
{
public:
    CPSGBlobMap(int lifespan, size_t max_size)
        : TParent(lifespan, max_size) {}

    void DropBlob(const CPsgBlobId& blob_id) {
        //ERR_POST("DropBlob("<<blob_id.ToPsgId()<<")");
        CFastMutexGuard guard(m_Mutex);
        auto iter = m_Values.find(blob_id.ToPsgId());
        if ( iter != m_Values.end() ) {
            x_Erase(iter);
        }
    }
};


/////////////////////////////////////////////////////////////////////////////
// SPsgBioseqInfo
/////////////////////////////////////////////////////////////////////////////


SPsgBioseqInfo::SPsgBioseqInfo(const CPSG_BioseqInfo& bioseq_info, int lifespan)
    : included_info(0),
      molecule_type(CSeq_inst::eMol_not_set),
      length(0),
      state(0),
      chain_state(0),
      tax_id(INVALID_TAX_ID),
      hash(0),
      deadline(lifespan)
{
    Update(bioseq_info);
}


SPsgBioseqInfo::TIncludedInfo SPsgBioseqInfo::Update(const CPSG_BioseqInfo& bioseq_info)
{
#ifdef NCBI_ENABLE_SAFE_FLAGS
    TIncludedInfo got_info = bioseq_info.IncludedInfo().get();
#else
    TIncludedInfo got_info = bioseq_info.IncludedInfo();
#endif
    TIncludedInfo new_info = got_info & ~included_info;
    if ( !new_info ) {
        return new_info;
    }

    DEFINE_STATIC_FAST_MUTEX(s_Mutex);
    CFastMutexGuard guard(s_Mutex);
    new_info = got_info & ~included_info;
    if (new_info & CPSG_Request_Resolve::fMoleculeType)
        molecule_type = bioseq_info.GetMoleculeType();

    if (new_info & CPSG_Request_Resolve::fLength)
        length = bioseq_info.GetLength();

    if (new_info & CPSG_Request_Resolve::fState)
        state = bioseq_info.GetState();

    if (new_info & CPSG_Request_Resolve::fChainState)
        chain_state = bioseq_info.GetChainState();

    if (new_info & CPSG_Request_Resolve::fTaxId)
        tax_id = bioseq_info.GetTaxId();

    if (new_info & CPSG_Request_Resolve::fHash)
        hash = bioseq_info.GetHash();

    if (new_info & CPSG_Request_Resolve::fCanonicalId) {
        canonical = PsgIdToHandle(bioseq_info.GetCanonicalId());
        _ASSERT(canonical);
        ids.push_back(canonical);
    }
    if (new_info & CPSG_Request_Resolve::fGi) {
        gi = bioseq_info.GetGi();
        if ( gi == INVALID_GI ) {
            gi = ZERO_GI;
        }
    }

    if (new_info & CPSG_Request_Resolve::fOtherIds) {
        vector<CPSG_BioId> other_ids = bioseq_info.GetOtherIds();
        ITERATE(vector<CPSG_BioId>, other_id, other_ids) {
            // NOTE: Bioseq-info may contain unparseable ids which should be ignored (e.g "gnl|FPAA000046" for GI 132).
            auto other_idh = PsgIdToHandle(*other_id);
            if (other_idh) ids.push_back(other_idh);
        }
    }
    if (new_info & CPSG_Request_Resolve::fBlobId)
        blob_id = bioseq_info.GetBlobId().GetId();

    included_info |= new_info;
    return new_info;
}


CBioseq_Handle::TBioseqStateFlags SPsgBioseqInfo::GetBioseqStateFlags() const
{
    if ( included_info & CPSG_Request_Resolve::fState ) {
        if ( state != CPSG_BioseqInfo::eLive ) {
            return CBioseq_Handle::fState_dead;
        }
    }
    return CBioseq_Handle::fState_none;
}


CBioseq_Handle::TBioseqStateFlags SPsgBioseqInfo::GetChainStateFlags() const
{
    if ( included_info & CPSG_Request_Resolve::fChainState ) {
        if ( chain_state != CPSG_BioseqInfo::eLive ) {
            return CBioseq_Handle::fState_dead;
        }
    }
    return CBioseq_Handle::fState_none;
}


/////////////////////////////////////////////////////////////////////////////
// SPsgBlobInfo
/////////////////////////////////////////////////////////////////////////////


SPsgBlobInfo::SPsgBlobInfo(const CPSG_BlobInfo& blob_info)
    : blob_state_flags(CBioseq_Handle::fState_none)
{
    auto blob_id = blob_info.GetId<CPSG_BlobId>();
    _ASSERT(blob_id);
    blob_id_main = blob_id->GetId();
    id2_info = blob_info.GetId2Info();

    if (blob_info.IsDead()) blob_state_flags |= CBioseq_Handle::fState_dead;
    if (blob_info.IsSuppressed()) blob_state_flags |= CBioseq_Handle::fState_suppress_perm;
    if (blob_info.IsWithdrawn()) blob_state_flags |= CBioseq_Handle::fState_withdrawn;

    auto lm = blob_id->GetLastModified(); // last_modified is in milliseconds
    last_modified = lm.IsNull()? 0: lm.GetValue();
}


SPsgBlobInfo::SPsgBlobInfo(const CTSE_Info& tse)
    : blob_state_flags(tse.GetBlobState()),
      last_modified(tse.GetBlobVersion()*60000) // minutes to ms
{
    const CPsgBlobId& blob_id = dynamic_cast<const CPsgBlobId&>(*tse.GetBlobId());
    blob_id_main = blob_id.ToPsgId();
    id2_info = blob_id.GetId2Info();
}


class CPSGIpgTaxIdMap : public CPSGCache_Base<CSeq_id_Handle, TTaxId>
{
public:
    CPSGIpgTaxIdMap(int lifespan, size_t max_size)
        : TParent(lifespan, max_size, INVALID_TAX_ID) {}
};


/////////////////////////////////////////////////////////////////////////////
// CPSG_Task
/////////////////////////////////////////////////////////////////////////////


string s_ReplyStatusToString(EPSG_Status status)
{
    switch (status) {
    case EPSG_Status::eSuccess: return "Success";
    case EPSG_Status::eCanceled: return "Canceled";
    case EPSG_Status::eError: return "Error";
    case EPSG_Status::eInProgress: return "In progress";
    case EPSG_Status::eNotFound: return "Not found";
    case EPSG_Status::eForbidden: return "Forbidden";
    }
    return to_string((int)status);
}


const char* s_GetSkippedType(const CPSG_SkippedBlob& skipped)
{
    switch ( skipped.GetReason() ) {
    case CPSG_SkippedBlob::eInProgress:
        return "in progress";
    case CPSG_SkippedBlob::eSent:
        return "sent";
    case CPSG_SkippedBlob::eExcluded:
        return "excluded";
    default:
        return "unknown";
    }
}


template<class TReply> void ReportStatus(TReply reply, EPSG_Status status)
{
    if (status == EPSG_Status::eSuccess) return;
    string sstatus = s_ReplyStatusToString(status);
    while (true) {
        string msg = reply->GetNextMessage();
        if (msg.empty()) break;
        _TRACE("Request failed: " << sstatus << " - " << msg << " @ "<<CStackTrace());
        if ( s_GetDebugLevel() >= 4 ) {
            LOG_POST("Request failed: " << sstatus << " - " << msg);
        }
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

    bool GotNotFound() const {
        return m_GotNotFound;
    }
    bool GotForbidden() const {
        return m_GotForbidden;
    }

    EPSG_Status GetPSGStatus(void) const {
        return m_PSGStatus;
    }

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
    void LogReplyItem(shared_ptr<CPSG_ReplyItem> item) const;

    void SetPSGStatus(EPSG_Status status) {
        // Save the first non-successful status.
        if (m_PSGStatus == EPSG_Status::eSuccess) m_PSGStatus = status;
    }

    TReply m_Reply;
    EStatus m_Status;
    bool m_GotNotFound;
    bool m_GotForbidden;

private:
    CPSG_TaskGroup& m_Group;
    EPSG_Status m_PSGStatus = EPSG_Status::eSuccess;
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
            CMutexGuard guard(m_Mutex);
            m_Tasks.insert(Ref(task));
            m_Pool.AddTask(task);
        }
    }

    void PostFinished(CPSG_Task& task)
    {
        {
            CRef<CPSG_Task> ref(&task);
            CMutexGuard guard(m_Mutex);
            TTasks::iterator it = m_Tasks.find(ref);
            if (it == m_Tasks.end()) return;
            m_Done.insert(ref);
            m_Tasks.erase(it);
        }
        m_Semaphore.Post();
    }

    bool HasTasks(void) const
    {
        CMutexGuard guard(m_Mutex);
        return !m_Tasks.empty() || ! m_Done.empty();
    }

    void WaitAll(void) {
        while (HasTasks()) GetTask<CPSG_Task>();
    }

    template<class T>
    CRef<T> GetTask(void) {
        m_Semaphore.Wait();
        CRef<T> ret;
        CMutexGuard guard(m_Mutex);
        _ASSERT(!m_Done.empty());
        TTasks::iterator it = m_Done.begin();
        ret.Reset(dynamic_cast<T*>(it->GetNCPointerOrNull()));
        m_Done.erase(it);
        return ret;
    }

    void CancelAll(void)
    {
        {
            TTasks tasks;
            {
                CMutexGuard guard(m_Mutex);
                tasks = m_Tasks;
            }
            for (CRef<CPSG_Task> task : tasks) {
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
    mutable CMutex m_Mutex;
};


CPSG_Task::CPSG_Task(TReply reply, CPSG_TaskGroup& group)
    : m_Reply(reply),
      m_Status(eIdle),
      m_GotNotFound(false),
      m_GotForbidden(false),
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
    if (status == eCompleted || status == eFailed || status == eCanceled) {
        m_Group.PostFinished(*this);
    }
}

bool CPSG_Task::CheckReplyStatus(void)
{
    EPSG_Status status = m_Reply->GetStatus(CDeadline::eInfinite);
    if (status != EPSG_Status::eSuccess) {
        SetPSGStatus(status);
        ReportStatus(m_Reply, status);
        if ( status == EPSG_Status::eNotFound ) {
            m_GotNotFound = true;
            m_Status = eCompleted;
            return false;
        }
        if ( status == EPSG_Status::eForbidden ) {
            m_GotForbidden = true;
            m_Status = eCompleted;
            return false;
        }
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

        LogReplyItem(reply_item);

        EPSG_Status status = reply_item->GetStatus(CDeadline::eInfinite);
        if (IsCancelled()) return;
        if (status != EPSG_Status::eSuccess) {
            SetPSGStatus(status);
            ReportStatus(reply_item, status);
            if ( status == EPSG_Status::eNotFound ) {
                m_GotNotFound = true;
                continue;
            }
            if ( status == EPSG_Status::eForbidden ) {
                m_GotForbidden = true;
                continue;
            }
            m_Status = eFailed;
            return;
        }
        ProcessReplyItem(reply_item);
    }
    if (IsCancelled()) return;
    status = m_Reply->GetStatus(CDeadline::eInfinite);
    if (status == EPSG_Status::eNotFound) {
        m_GotNotFound = true;
        ReportStatus(m_Reply, status);
        return;
    }
    if (status != EPSG_Status::eSuccess) {
        SetPSGStatus(status);
        ReportStatus(m_Reply, status);
        m_Status = eFailed;
    }
}


void CPSG_Task::LogReplyItem(shared_ptr<CPSG_ReplyItem> item) const
{
    if (s_GetDebugLevel() < 7) return;
    auto status = item->GetStatus(CDeadline::eInfinite);
    if (status == EPSG_Status::eSuccess) return;
    string info;
    switch (item->GetType()) {
    case CPSG_ReplyItem::eBlobData:
        info = "BlobData";
        break;
    case CPSG_ReplyItem::eBlobInfo:
        info = "BlobInfo";
        break;
    case CPSG_ReplyItem::eSkippedBlob:
        info = "SkippedBlob";
        break;
    case CPSG_ReplyItem::eBioseqInfo:
        info = "BioseqInfo";
        break;
    case CPSG_ReplyItem::eNamedAnnotInfo:
        info = "NamedAnnotInfo";
        break;
    case CPSG_ReplyItem::ePublicComment:
        info = "PublicComment";
        break;
    case CPSG_ReplyItem::eProcessor:
        info = "Processor";
        break;
    case CPSG_ReplyItem::eIpgInfo:
        info = "IpgInfo";
        break;
    case CPSG_ReplyItem::eNamedAnnotStatus:
        info = "NamedAnnotStatus";
        break;
    case CPSG_ReplyItem::eEndOfReply:
        info = "EndOfReply";
        break;
    default: info = "unknown";
    }
    info += ", status: " + s_ReplyStatusToString(status);
    LOG_POST(Info << "PSG loader - processing reply item: " << info);
}


class CPSG_PrefetchCDD_Task : public CThreadPool_Task
{
public:
    CPSG_PrefetchCDD_Task(CPSGDataLoader_Impl& loader)
        : m_Semaphore(0, kMax_UInt), m_Loader(loader)
    {}
    ~CPSG_PrefetchCDD_Task(void) override
    {}

    EStatus Execute(void) override;

    void AddRequest(const CDataLoader::TIds& ids)
    {
        CFastMutexGuard guard(m_Mutex);
        m_Ids.push_back(ids);
        m_Semaphore.Post();
    }

    void Cancel(void)
    {
        RequestToCancel();
        m_Semaphore.Post();
    }

private:
    CSemaphore m_Semaphore;
    CFastMutex m_Mutex;
    CPSGDataLoader_Impl& m_Loader;
    list<CDataLoader::TIds> m_Ids;
};


CPSG_Task::EStatus CPSG_PrefetchCDD_Task::Execute(void)
{
    while (true) {
        m_Semaphore.Wait();
        if (IsCancelRequested()) return eCanceled;
        CDataLoader::TIds ids;
        {
            CFastMutexGuard guard(m_Mutex);
            if (m_Ids.empty()) continue;
            ids = m_Ids.front();
            m_Ids.pop_front();
        }
        try {
            m_Loader.PrefetchCDD(ids);
        }
        catch (CException& exc) {
            LOG_POST("CPSGDataLoader: exception in CDD prefetch thread: "<<exc);
        }
        catch (exception& exc) {
            LOG_POST("CPSGDataLoader: exception in CDD prefetch thread: "<<exc.what());
        }
    }
    // Never executed
    return eCompleted;
}


/////////////////////////////////////////////////////////////////////////////
// CPSGDataLoader_Impl
/////////////////////////////////////////////////////////////////////////////

#define NCBI_PSGLOADER_NAME "psg_loader"
#define NCBI_PSGLOADER_SERVICE_NAME "service_name"
#define NCBI_PSGLOADER_NOSPLIT "no_split"
#define NCBI_PSGLOADER_WHOLE_TSE "whole_tse"
#define NCBI_PSGLOADER_WHOLE_TSE_BULK "whole_tse_bulk"
#define NCBI_PSGLOADER_ADD_WGS_MASTER "add_wgs_master"
//#define NCBI_PSGLOADER_RETRY_COUNT "retry_count"

NCBI_PARAM_DECL(string, PSG_LOADER, SERVICE_NAME);
NCBI_PARAM_DEF_EX(string, PSG_LOADER, SERVICE_NAME, "PSG2",
    eParam_NoThread, PSG_LOADER_SERVICE_NAME);
typedef NCBI_PARAM_TYPE(PSG_LOADER, SERVICE_NAME) TPSG_ServiceName;

NCBI_PARAM_DECL(bool, PSG_LOADER, WHOLE_TSE);
NCBI_PARAM_DEF_EX(bool, PSG_LOADER, WHOLE_TSE, false,
    eParam_NoThread, PSG_LOADER_WHOLE_TSE);
typedef NCBI_PARAM_TYPE(PSG_LOADER, WHOLE_TSE) TPSG_WholeTSE;

NCBI_PARAM_DECL(bool, PSG_LOADER, WHOLE_TSE_BULK);
NCBI_PARAM_DEF_EX(bool, PSG_LOADER, WHOLE_TSE_BULK, false,
    eParam_NoThread, PSG_LOADER_WHOLE_TSE_BULK);
typedef NCBI_PARAM_TYPE(PSG_LOADER, WHOLE_TSE_BULK) TPSG_WholeTSEBulk;

NCBI_PARAM_DECL(unsigned int, PSG_LOADER, MAX_POOL_THREADS);
NCBI_PARAM_DEF_EX(unsigned int, PSG_LOADER, MAX_POOL_THREADS, 10,
    eParam_NoThread, PSG_LOADER_MAX_POOL_THREADS);
typedef NCBI_PARAM_TYPE(PSG_LOADER, MAX_POOL_THREADS) TPSG_MaxPoolThreads;

NCBI_PARAM_DECL(bool, PSG_LOADER, PREFETCH_CDD);
NCBI_PARAM_DEF_EX(bool, PSG_LOADER, PREFETCH_CDD, false,
    eParam_NoThread, PSG_LOADER_PREFETCH_CDD);
typedef NCBI_PARAM_TYPE(PSG_LOADER, PREFETCH_CDD) TPSG_PrefetchCDD;

NCBI_PARAM_DECL(unsigned int, PSG_LOADER, RETRY_COUNT);
NCBI_PARAM_DEF_EX(unsigned int, PSG_LOADER, RETRY_COUNT, kDefaultRetryCount,
    eParam_NoThread, PSG_LOADER_RETRY_COUNT);
typedef NCBI_PARAM_TYPE(PSG_LOADER, RETRY_COUNT) TPSG_RetryCount;

NCBI_PARAM_DECL(unsigned int, PSG_LOADER, BULK_RETRY_COUNT);
NCBI_PARAM_DEF_EX(unsigned int, PSG_LOADER, BULK_RETRY_COUNT, kDefaultBulkRetryCount,
    eParam_NoThread, PSG_LOADER_BULK_RETRY_COUNT);
typedef NCBI_PARAM_TYPE(PSG_LOADER, BULK_RETRY_COUNT) TPSG_BulkRetryCount;

NCBI_PARAM_DECL(bool, PSG_LOADER, IPG_TAX_ID);
NCBI_PARAM_DEF_EX(bool, PSG_LOADER, IPG_TAX_ID, false, eParam_NoThread, PSG_LOADER_IPG_TAX_ID);
typedef NCBI_PARAM_TYPE(PSG_LOADER, IPG_TAX_ID) TPSG_IpgTaxIdEnabled;


template<class TParamType>
static void s_ConvertParamValue(TParamType& value, const string& str)
{
    value = NStr::StringToNumeric<TParamType>(str);
}


template<>
void s_ConvertParamValue<bool>(bool& value, const string& str)
{
    value = NStr::StringToBool(str);
}


static const TPluginManagerParamTree* s_FindSubNode(const TPluginManagerParamTree* params,
                                                    const string& name)
{
    return params? params->FindSubNode(name): 0;
}


template<class TParamDescription>
static typename TParamDescription::TValueType s_GetParamValue(const TPluginManagerParamTree* config)
{
    typedef CParam<TParamDescription> TParam;
    typename TParam::TValueType value = TParam::GetDefault();
    CParamBase::EParamSource source = CParamBase::eSource_NotSet;
    TParam::GetState(0, &source);
    if ( config && (source == CParamBase::eSource_NotSet ||
                    source == CParamBase::eSource_Default ||
                    source == CParamBase::eSource_Config) ) {
        if ( const TPluginManagerParamTree* node = s_FindSubNode(config, TParamDescription::sm_ParamDescription.name) ) {
            s_ConvertParamValue<typename TParam::TValueType>(value, node->GetValue().value);
        }
    }
    return value;
}


CPSGDataLoader_Impl::CPSGDataLoader_Impl(const CGBLoaderParams& params)
    : m_ThreadPool(new CThreadPool(kMax_UInt, TPSG_MaxPoolThreads::GetDefault())),
      m_WaitTime(s_WaitTimeParams)
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

    string service_name = params.GetPSGServiceName();
    if (service_name.empty() && psg_params) {
        service_name = CPSGDataLoader::GetParam(psg_params, NCBI_PSGLOADER_SERVICE_NAME);
    }
    if (service_name.empty()) {
        service_name = TPSG_ServiceName::GetDefault();
    }

    bool no_split = params.GetPSGNoSplit();
    if (psg_params) {
        try {
            string value = CPSGDataLoader::GetParam(psg_params, NCBI_PSGLOADER_NOSPLIT);
            if (!value.empty()) {
                no_split = NStr::StringToBool(value);
            }
        }
        catch (CException&) {
        }
    }
    if ( no_split ) {
        m_TSERequestMode = CPSG_Request_Biodata::eOrigTSE;
        m_TSERequestModeBulk = CPSG_Request_Biodata::eOrigTSE;
    }
    else {
        m_TSERequestMode = (s_GetParamValue<X_NCBI_PARAM_DECLNAME(PSG_LOADER, WHOLE_TSE)>(psg_params)?
                            CPSG_Request_Biodata::eWholeTSE:
                            CPSG_Request_Biodata::eSmartTSE);
        m_TSERequestModeBulk = (s_GetParamValue<X_NCBI_PARAM_DECLNAME(PSG_LOADER, WHOLE_TSE_BULK)>(psg_params)?
                                CPSG_Request_Biodata::eWholeTSE:
                                CPSG_Request_Biodata::eSmartTSE);
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

    m_CacheLifespan = kDefaultCacheLifespanSeconds;
    size_t cache_max_size = kDefaultMaxCacheSize;
    if (psg_params) {
        try {
            string value = CPSGDataLoader::GetParam(psg_params, NCBI_GBLOADER_PARAM_ID_EXPIRATION_TIMEOUT);
            if (!value.empty()) {
                m_CacheLifespan = NStr::StringToNumeric<int>(value);
            }
        }
        catch (CException&) {
        }
        try {
            string value = CPSGDataLoader::GetParam(psg_params, NCBI_GBLOADER_PARAM_ID_GC_SIZE);
            if (!value.empty()) {
                cache_max_size = NStr::StringToNumeric<size_t>(value);
            }
        }
        catch (CException&) {
        }
    }

    m_RetryCount =
        s_GetParamValue<X_NCBI_PARAM_DECLNAME(PSG_LOADER, RETRY_COUNT)>(psg_params);
    m_BulkRetryCount =
        s_GetParamValue<X_NCBI_PARAM_DECLNAME(PSG_LOADER, BULK_RETRY_COUNT)>(psg_params);
    if ( psg_params ) {
        CConfig conf(psg_params);
        m_WaitTime.Init(conf, NCBI_PSGLOADER_NAME, s_WaitTimeParams);
    }

    m_BioseqCache.reset(new CPSGBioseqCache(m_CacheLifespan, cache_max_size));
    m_AnnotCache.reset(new CPSGAnnotCache(m_CacheLifespan, cache_max_size));
    m_BlobMap.reset(new CPSGBlobMap(m_CacheLifespan, cache_max_size));

    {{
        m_Queue = make_shared<CPSG_Queue>(service_name);
        m_Queue->SetRequestFlags(params.HasHUPIncluded()? CPSG_Request::fIncludeHUP: CPSG_Request::fExcludeHUP);
        if ( !params.GetWebCookie().empty() ) {
            m_RequestContext = new CRequestContext();
            m_RequestContext->SetProperty("auth_token", params.GetWebCookie());
        }
    }}

    m_CDDInfoCache.reset(new CPSGCDDInfoCache(m_CacheLifespan, cache_max_size));
    if (TPSG_PrefetchCDD::GetDefault()) {
        m_CDDPrefetchTask.Reset(new CPSG_PrefetchCDD_Task(*this));
        m_ThreadPool->AddTask(m_CDDPrefetchTask);
    }

    if (TPSG_IpgTaxIdEnabled::GetDefault()) {
        m_IpgTaxIdMap.reset(new CPSGIpgTaxIdMap(m_CacheLifespan, cache_max_size));
    }

    CUrlArgs args;
    if (params.IsSetEnableSNP()) {
        args.AddValue(params.GetEnableSNP() ? "enable_processor" : "disable_processor", "snp");
    }
    if (params.IsSetEnableWGS()) {
        args.AddValue(params.GetEnableWGS() ? "enable_processor" : "disable_processor", "wgs");
    }
    if (params.IsSetEnableCDD()) {
        args.AddValue(params.GetEnableCDD() ? "enable_processor" : "disable_processor", "cdd");
    }
    if (!args.GetArgs().empty()) {
        m_Queue->SetUserArgs(SPSG_UserArgs(args));
    }
}


CPSGDataLoader_Impl::~CPSGDataLoader_Impl(void)
{
    if (m_CDDPrefetchTask) {
        m_CDDPrefetchTask->Cancel();
    }
    // Make sure thread pool is destroyed before any tasks (e.g. CDD prefetch)
    // and stops them all before the loader is destroyed.
    m_ThreadPool.reset();
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
typename std::invoke_result<Call>::type
CPSGDataLoader_Impl::CallWithRetry(Call&& call,
                                   const char* name,
                                   int retry_count)
{
    if ( retry_count == 0 ) {
        retry_count = m_RetryCount;
    }
    for ( int t = 1; t < retry_count; ++ t ) {
        try {
            return call();
        }
        catch ( CBlobStateException& ) {
            // no retry
            throw;
        }
        catch ( CLoaderException& exc ) {
            if ( exc.GetErrCode() == exc.eConnectionFailed ||
                 exc.GetErrCode() == exc.eLoaderFailed ) {
                // can retry
                LOG_POST(Warning<<"CPSGDataLoader::"<<name<<"() try "<<t<<" exception: "<<exc);
            }
            else {
                // no retry
                throw;
            }
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
        if ( t >= 2 ) {
            double wait_sec = m_WaitTime.GetTime(t-2);
            LOG_POST(Warning<<"CPSGDataLoader: waiting "<<wait_sec<<"s before retry");
            SleepMilliSec(Uint4(wait_sec*1000));
        }
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
    auto tax_id = x_GetIpgTaxId(idh);
    if (tax_id != INVALID_TAX_ID) return tax_id;
    auto seq_info = x_GetBioseqInfo(idh);
    return seq_info ? seq_info->tax_id : INVALID_TAX_ID;
}


void CPSGDataLoader_Impl::GetTaxIds(const TIds& ids, TLoaded& loaded, TTaxIds& ret)
{
    return CallWithRetry(bind(&CPSGDataLoader_Impl::GetTaxIdsOnce, this,
                              cref(ids), ref(loaded), ref(ret)), "GetTaxId");
}


void CPSGDataLoader_Impl::GetTaxIdsOnce(const TIds& ids, TLoaded& loaded, TTaxIds& ret)
{
    x_GetIpgTaxIds(ids, loaded, ret);
    for (size_t i = 0; i < ids.size(); ++i) {
        if (loaded[i]) continue;
        TTaxId taxid = GetTaxId(ids[i]);
        if ( taxid != INVALID_TAX_ID ) {
            ret[i] = taxid;
            loaded[i] = true;
        }
    }
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
        if ( seq_info->hash ) {
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


int CPSGDataLoader_Impl::GetSequenceState(CDataSource* data_source, const CSeq_id_Handle& idh)
{
    return CallWithRetry(bind(&CPSGDataLoader_Impl::GetSequenceStateOnce, this,
                              data_source, cref(idh)),
                         "GetSequenceState");
}


int CPSGDataLoader_Impl::GetSequenceStateOnce(CDataSource* data_source, const CSeq_id_Handle& idh)
{
    const int kNotFound = (CBioseq_Handle::fState_not_found |
                           CBioseq_Handle::fState_no_data);
    if ( CannotProcess(idh) ) {
        return kNotFound;
    }
    auto info = x_GetBioseqAndBlobInfo(data_source, idh);
    if ( !info.first ) {
        return kNotFound;
    }
    CBioseq_Handle::TBioseqStateFlags state = info.first->GetBioseqStateFlags();
    if ( info.second ) {
        state |= info.second->blob_state_flags;
        if (!(info.first->GetBioseqStateFlags() & CBioseq_Handle::fState_dead) &&
            !(info.first->GetChainStateFlags() & CBioseq_Handle::fState_dead)) {
            state &= ~CBioseq_Handle::fState_dead;
        }
    }
    return state;
}


struct SCDDIds
{
    CSeq_id_Handle gi;
    CSeq_id_Handle acc_ver;
    bool empty() const {
        return !gi; // TODO: && !acc_ver;
    }
    DECLARE_OPERATOR_BOOL(!empty());
};

static SCDDIds x_GetCDDIds(const CDataLoader::TIds& ids);
static bool x_IsLocalCDDEntryId(const CPsgBlobId& blob_id);
static bool x_ParseLocalCDDEntryId(const CPsgBlobId& blob_id, SCDDIds& ids);
static CTSE_Lock x_CreateLocalCDDEntry(CDataSource* data_source, const SCDDIds& ids);
static string x_MakeLocalCDDEntryId(const SCDDIds& cdd_ids);


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

    CPSG_Request_Biodata::EIncludeData inc_data = CPSG_Request_Biodata::eNoTSE;
    if (data_source) {
        inc_data = m_TSERequestMode;
    }
    
    CPSG_BioId bio_id(idh);
    auto request = make_shared<CPSG_Request_Biodata>(std::move(bio_id));
    if (data_source) {
        CDataSource::TLoadedBlob_ids loaded_blob_ids;
        data_source->GetLoadedBlob_ids(idh, CDataSource::fKnown_bioseqs, loaded_blob_ids);
        ITERATE(CDataSource::TLoadedBlob_ids, loaded_blob_id, loaded_blob_ids) {
            const CPsgBlobId* pbid = dynamic_cast<const CPsgBlobId*>(&**loaded_blob_id);
            if (!pbid) continue;
            request->ExcludeTSE(CPSG_BlobId(pbid->ToPsgId()));
        }
    }
    request->IncludeData(inc_data);
    auto reply = x_SendRequest(request);
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
        if (m_CDDPrefetchTask) {
            auto bioseq_info = m_BioseqCache->Get(idh);
            if (bioseq_info) {
                auto cdd_ids = x_GetCDDIds(bioseq_info->ids);
                if (cdd_ids && !m_CDDInfoCache->Find(x_MakeLocalCDDEntryId(cdd_ids))) {
                    m_CDDPrefetchTask->AddRequest(bioseq_info->ids);
                }
            }
        }
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
    string blob_id = x_GetCachedBlobId(idh);
    if ( blob_id.empty() ) {
        CPSG_BioId bio_id(idh);
        auto request = make_shared<CPSG_Request_Biodata>(std::move(bio_id));
        request->IncludeData(CPSG_Request_Biodata::eNoTSE);
        auto reply = x_SendRequest(request);
        blob_id = x_ProcessBlobReply(reply, nullptr, idh, true).blob_id;
    }
    CRef<CPsgBlobId> ret;
    if ( !blob_id.empty() ) {
        ret.Reset(new CPsgBlobId(blob_id));
    }
    return ret;
}


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
        SCDDIds cdd_ids;
        if ( x_ParseLocalCDDEntryId(blob_id, cdd_ids) ) {
            ret = x_CreateLocalCDDEntry(data_source, cdd_ids);
        }
    }
    else {
        CPSG_BlobId bid(blob_id.ToPsgId());
        auto request = make_shared<CPSG_Request_Blob>(bid);
        request->IncludeData(m_TSERequestMode);
        auto reply = x_SendRequest(request);
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

    ~CPSG_Blob_Task(void) override;

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
    unique_ptr<CDeadline> m_SkippedWaitDeadline;
    
    CPSGDataLoader_Impl::SReplyResult m_ReplyResult;

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
    unique_ptr<CDeadline> GetWaitDeadline(const CPSG_SkippedBlob& skipped) const;
    void x_TraceChunkBlobMap(const char* msg) const;

    void Finish(void) override
    {
        m_Skipped.reset();
        m_ReplyResult = CPSGDataLoader_Impl::SReplyResult();
        m_TSEBlobMap.clear();
        x_TraceChunkBlobMap("");
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
    void CreateLoadedChunks(CTSE_LoadLock& load_lock);
    static bool IsChunk(const CPSG_DataId* id);
    static bool IsChunk(const CPSG_DataId& id);
    static bool IsChunk(const CPSG_SkippedBlob& skipped);
    
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
        _TRACE("CPSG_Blob_Task("<<m_Id<<"): Blob slot for tse_id="<<tse_id->GetId());
        return &m_TSEBlobMap[tse_id->GetId()];
    }
    else if ( auto chunk_id = dynamic_cast<const CPSG_ChunkId*>(&id) ) {
        _TRACE("CPSG_Blob_Task("<<m_Id<<"): Blob slot for id2_info="<<chunk_id->GetId2Info()<<" chunk="<<chunk_id->GetId2Chunk());
        return &m_ChunkBlobMap[chunk_id->GetId2Info()][chunk_id->GetId2Chunk()];
    }
    return 0;
}


void CPSG_Blob_Task::x_TraceChunkBlobMap(const char* _DEBUG_ARG(msg)) const
{
#ifdef _DEBUG
    for ( auto& blob_slot : m_ChunkBlobMap ) {
        for ( auto& chunk_slot : blob_slot.second ) {
            _TRACE("CPSG_Blob_Task("<<m_Id<<"): chunk not processed"<<msg<<": "<<blob_slot.first<<"."<<chunk_slot.first);
        }
    }
#endif
}


bool CPSG_Blob_Task::GotBlobData(const string& psg_blob_id) const
{
    const TBlobSlot* main_blob_slot = GetTSESlot(psg_blob_id);
    if ( !main_blob_slot || !main_blob_slot->first ) {
        // no TSE blob props yet
        if ( s_GetDebugLevel() >= 7 ) {
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
        if ( s_GetDebugLevel() >= 7 ) {
            LOG_POST("GotBlobData("<<psg_blob_id<<"): not split");
        }
        return false;
    }
    const TBlobSlot* split_blob_slot = GetChunkSlot(id2_info, kSplitInfoChunkId);
    if ( !split_blob_slot || !split_blob_slot->second ) {
        // no split info blob data yet
        if ( s_GetDebugLevel() >= 7 ) {
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
    _TRACE("CPSG_Blob_Task("<<m_Id<<")::DoExecute()");
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
        if (m_Reply->GetRequest()->GetType() == CPSG_Request::eBlob) {
            shared_ptr<const CPSG_Request_Blob> blob_request = static_pointer_cast<const CPSG_Request_Blob>(m_Reply->GetRequest());
            if (blob_request) {
                m_ReplyResult.blob_id = blob_request->GetId();
            }
        }
    }
    if (m_ReplyResult.blob_id.empty()) {
        _TRACE("CPSG_Blob_Task("<<m_Id<<"): no blob_id");
        m_Status = eCompleted;
        return;
    }

    _TRACE("CPSG_Blob_Task("<<m_Id<<"): tse_id="<<m_ReplyResult.blob_id);
    const TBlobSlot* main_blob_slot = GetTSESlot(m_ReplyResult.blob_id);
    if ( main_blob_slot && main_blob_slot->first ) {
        // create and save new main blob-info entry
        m_ReplyResult.blob_info = make_shared<SPsgBlobInfo>(*main_blob_slot->first);
        m_Loader.x_AdjustBlobState(*m_ReplyResult.blob_info, m_Id);
        m_Loader.m_BlobMap->Add(m_ReplyResult.blob_id, m_ReplyResult.blob_info);
    }
    
    if ( !m_LoadLockPtr ) {
        // to TSE requested
        m_Status = eCompleted;
        return;
    }

    if ( !main_blob_slot || !main_blob_slot->first ) {
        _TRACE("CPSG_Blob_Task("<<m_Id<<"): No blob info for tse_id="<<m_ReplyResult.blob_id);
        m_Status = eFailed;
        return;
    }

    const TBlobSlot* split_blob_slot = 0;
    auto id2_info = main_blob_slot->first->GetId2Info();
    if ( !id2_info.empty() ) {
        split_blob_slot = GetChunkSlot(id2_info, kSplitInfoChunkId);
        if ( !split_blob_slot || !split_blob_slot->first ) {
            _TRACE("CPSG_Blob_Task("<<m_Id<<"): No split info tse_id="<<m_ReplyResult.blob_id<<" id2_info="<<id2_info);
        }
    }

    if (!m_DataSource) {
        _TRACE("CPSG_Blob_Task("<<m_Id<<"): No data source for tse_id="<<m_ReplyResult.blob_id);
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
        _TRACE("CPSG_Blob_Task("<<m_Id<<"): Cannot get TSE load lock for tse_id="<<m_ReplyResult.blob_id);
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
            _TRACE("CPSG_Blob_Task("<<m_Id<<"): Already loaded tse_id="<<m_ReplyResult.blob_id);
#ifdef GLOBAL_CHUNKS
            CreateLoadedChunks(load_lock);
#endif
            m_ReplyResult.lock = load_lock;
            m_Status = eCompleted;
            return;
        }
    }

    if ( split_blob_slot && split_blob_slot->first && split_blob_slot->second ) {
        auto& blob_id = *load_lock->GetBlobId();
        dynamic_cast<CPsgBlobId&>(const_cast<CBlobId&>(blob_id)).SetId2Info(id2_info);
        m_Loader.x_ReadBlobData(*m_ReplyResult.blob_info,
                                *split_blob_slot->first,
                                *split_blob_slot->second,
                                load_lock,
                                CPSGDataLoader_Impl::eIsSplitInfo);
        load_lock->GetSplitInfo();
    }
    else if ( main_blob_slot && main_blob_slot->first && main_blob_slot->second ) {
        m_Loader.x_ReadBlobData(*m_ReplyResult.blob_info,
                                *main_blob_slot->first,
                                *main_blob_slot->second,
                                load_lock,
                                CPSGDataLoader_Impl::eNoSplitInfo);
    }
    else if ( GotForbidden() ) {
        _TRACE("CPSG_Blob_Task("<<m_Id<<"): Got forbidden for tse_id="<<m_ReplyResult.blob_id);
        load_lock.Reset();
        m_Status = eCompleted;
        return;
    }
    else {
        _TRACE("CPSG_Blob_Task("<<m_Id<<"): No data for tse_id="<<m_ReplyResult.blob_id);
        load_lock.Reset();
    }
    if ( load_lock ) {
#ifdef GLOBAL_CHUNKS
        m_Loader.x_SetLoaded(load_lock, main_chunk_type);
        CreateLoadedChunks(load_lock);
#else
        CreateLoadedChunks(load_lock);
        m_Loader.x_SetLoaded(load_lock, main_chunk_type);
#endif
        m_ReplyResult.lock = load_lock;
        m_Status = eCompleted;
    }
    else {
        m_Status = eFailed;
    }
}


CPSG_Blob_Task::~CPSG_Blob_Task()
{
    x_TraceChunkBlobMap("");
}


void CPSG_Blob_Task::CreateLoadedChunks(CTSE_LoadLock& load_lock)
{
    if ( m_ChunkBlobMap.empty() ) {
        return;
    }
    if ( !load_lock || !load_lock->HasSplitInfo() ) {
        x_TraceChunkBlobMap(" without load lock");
        return;
    }
    auto blob_id = dynamic_cast<const CPsgBlobId*>(&*load_lock->GetBlobId());
    if ( !blob_id ) {
        x_TraceChunkBlobMap(" without blob id");
        return;
    }
    CTSE_Split_Info& tse_split_info = load_lock->GetSplitInfo();
    for ( auto& chunk_slot : m_ChunkBlobMap[blob_id->GetId2Info()] ) {
        TChunkId chunk_id = chunk_slot.first;
        if ( chunk_id == kSplitInfoChunkId ) {
            continue;
        }
        if ( !chunk_slot.second.first || !chunk_slot.second.second ) {
            _TRACE("CPSG_Blob_Task("<<m_Id<<"): chunk not processed incomplete: "<<blob_id->GetId2Info()<<"."<<chunk_slot.first);
            continue;
        }
        CTSE_Chunk_Info* chunk = 0;
        try {
            chunk = &tse_split_info.GetChunk(chunk_id);
        }
        catch ( CException& /*ignored*/ ) {
        }
        if ( !chunk ) {
            _TRACE("CPSG_Blob_Task("<<m_Id<<"): chunk not processed without info: "<<blob_id->GetId2Info()<<"."<<chunk_slot.first);
            continue;
        }
        if ( chunk->IsLoaded() ) {
            _TRACE("CPSG_Blob_Task("<<m_Id<<"): chunk already loaded: "<<blob_id->GetId2Info()<<"."<<chunk_slot.first);
            continue;
        }
        AutoPtr<CInitGuard> guard;
        if ( load_lock.IsLoaded() ) {
            guard = chunk->GetLoadInitGuard();
            if ( !guard.get() || !*guard.get() ) {
                _TRACE("CPSG_Blob_Task("<<m_Id<<"): chunk already loaded 2: "<<blob_id->GetId2Info()<<"."<<chunk_slot.first);
                continue;
            }
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
    m_ChunkBlobMap.erase(blob_id->GetId2Info());
}


bool CPSG_Blob_Task::IsChunk(const CPSG_DataId* id)
{
    if ( auto chunk_id = dynamic_cast<const CPSG_ChunkId*>(id) ) {
        return chunk_id->GetId2Chunk() != kSplitInfoChunkId;
    }
    return false;
}


bool CPSG_Blob_Task::IsChunk(const CPSG_DataId& id)
{
    return IsChunk(&id);
}


bool CPSG_Blob_Task::IsChunk(const CPSG_SkippedBlob& skipped)
{
    return IsChunk(skipped.GetId());
}


unique_ptr<CDeadline> CPSG_Blob_Task::GetWaitDeadline(const CPSG_SkippedBlob& skipped) const
{
    double timeout = 0;
    switch ( skipped.GetReason() ) {
    case CPSG_SkippedBlob::eInProgress:
        timeout = 1;
        break;
    case CPSG_SkippedBlob::eSent:
        if ( skipped.GetTimeUntilResend().IsNull() ) {
            timeout = 0.2;
        }
        else {
            timeout = skipped.GetTimeUntilResend().GetValue();
        }
        break;
    default:
        return nullptr;
    }
    return make_unique<CDeadline>(CTimeout(timeout));
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
        _TRACE("CPSG_Blob_Task("<<m_Id<<"): Blob info: "<<blob_info->GetId()->Repr());
        if ( auto slot = SetBlobSlot(*blob_info->GetId()) ) {
            slot->first = blob_info;
            ObtainLoadLock();
        }
        break;
    }
    case CPSG_ReplyItem::eBlobData:
    {
        shared_ptr<CPSG_BlobData> data = static_pointer_cast<CPSG_BlobData>(item);
        _TRACE("CPSG_Blob_Task("<<m_Id<<"): Blob data: "<<data->GetId()->Repr());
        if ( auto slot = SetBlobSlot(*data->GetId()) ) {
            slot->second = data;
            ObtainLoadLock();
        }
        break;
    }
    case CPSG_ReplyItem::eSkippedBlob:
    {
        // Only main blob can be skipped.
        if ( !m_Skipped && !IsChunk(*static_pointer_cast<CPSG_SkippedBlob>(item)) ) {
            shared_ptr<CPSG_SkippedBlob> skipped = static_pointer_cast<CPSG_SkippedBlob>(item);
            m_Skipped = skipped;
            m_SkippedWaitDeadline = GetWaitDeadline(*skipped);
        }
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
    if ( m_SkippedWaitDeadline ) {
        load_lock = m_DataSource->GetLoadedTSE_Lock(dl_blob_id, *m_SkippedWaitDeadline);
    }
    else {
        load_lock = m_DataSource->GetTSE_LoadLockIfLoaded(dl_blob_id);
    }
    if ( load_lock && load_lock.IsLoaded() ) {
#ifdef GLOBAL_CHUNKS
        CreateLoadedChunks(load_lock);
#endif
        ret.lock = load_lock;
    }
    else {
        if ( s_GetDebugLevel() >= 6 ) {
            LOG_POST("CPSGDataLoader: '"<<s_GetSkippedType(*m_Skipped)<<"' blob is not loaded: "<<dl_blob_id.ToString());
        }
    }
    return ret;
}


void CPSGDataLoader_Impl::GetBlobs(CDataSource* data_source, TTSE_LockSets& tse_sets)
{
    TLoadedSeqIds loaded;
    CallWithRetry(bind(&CPSGDataLoader_Impl::GetBlobsOnce, this,
                       data_source, ref(loaded), ref(tse_sets)),
                  "GetBlobs",
                  m_BulkRetryCount);
}


void CPSGDataLoader_Impl::GetBlobsOnce(CDataSource* data_source, TLoadedSeqIds& loaded, TTSE_LockSets& tse_sets)
{
    if (!data_source) return;
    CPSG_TaskGroup group(*m_ThreadPool);
    ITERATE(TTSE_LockSets, tse_set, tse_sets) {
        const CSeq_id_Handle& idh = tse_set->first;
        if ( loaded.count(idh) ) {
            continue;
        }
        CPSG_BioId bio_id(idh);
        auto request = make_shared<CPSG_Request_Biodata>(std::move(bio_id));
        CPSG_Request_Biodata::EIncludeData inc_data = CPSG_Request_Biodata::eNoTSE;
        if (data_source) {
            inc_data = m_TSERequestModeBulk;
            CDataSource::TLoadedBlob_ids loaded_blob_ids;
            data_source->GetLoadedBlob_ids(idh, CDataSource::fKnown_bioseqs, loaded_blob_ids);
            ITERATE(CDataSource::TLoadedBlob_ids, loaded_blob_id, loaded_blob_ids) {
                const CPsgBlobId* pbid = dynamic_cast<const CPsgBlobId*>(&**loaded_blob_id);
                if (!pbid) continue;
                request->ExcludeTSE(CPSG_BlobId(pbid->ToPsgId()));
            }
        }
        request->IncludeData(inc_data);
        auto reply = x_SendRequest(request);
        CRef<CPSG_Blob_Task> task(
            new CPSG_Blob_Task(reply, group, idh, data_source, *this, true));
        group.AddTask(task);
    }
    size_t failed_count = 0;
    EPSG_Status psg_status = EPSG_Status::eSuccess;
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
            if (psg_status == EPSG_Status::eSuccess) psg_status = task->GetPSGStatus();
            ++failed_count;
            continue;
        }
        if (task->m_Skipped) {
            skipped_tasks.push_back(task);
            continue;
        }
        SReplyResult res = task->m_ReplyResult;
        if (task->m_ReplyResult.lock) {
            tse_sets[task->m_Id].insert(task->m_ReplyResult.lock);
        }
        loaded.insert(task->m_Id);
    }
    NON_CONST_ITERATE(TTasks, it, skipped_tasks) {
        CPSG_Blob_Task& task = **it;
        SReplyResult result = task.WaitForSkipped();
        if (!result.lock) {
            // Force reloading blob
            try {
                result = x_RetryBlobRequest(task.m_ReplyResult.blob_id, data_source, task.m_Id);
            }
            catch ( CException& /*doesn't matter*/ ) {
                ++failed_count;
                continue;
            }
        }
        if (result.lock) {
            tse_sets[task.m_Id].insert(result.lock);
        }
        loaded.insert(task.m_Id);
    }
    if ( failed_count ) {
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "failed to load "<<failed_count<<" blobs, status=" << s_ReplyStatusToString(psg_status));
    }
}


void CPSGDataLoader_Impl::GetCDDAnnots(CDataSource* data_source,
    const TSeqIdSets& id_sets, TLoaded& loaded, TCDD_Locks& ret)
{
    CallWithRetry(bind(&CPSGDataLoader_Impl::GetCDDAnnotsOnce, this,
                       data_source, id_sets, ref(loaded), ref(ret)),
                  "GetCDDAnnots",
                  m_BulkRetryCount);
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


static SCDDIds x_GetCDDIds(const CDataLoader::TIds& ids)
{
    SCDDIds ret;
    bool is_protein = true;
    for ( auto id : ids ) {
        if ( id.IsGi() ) {
            ret.gi = id;
            continue;
        }
        if ( id.Which() == CSeq_id::e_Pdb ) {
            if ( !ret.acc_ver ) {
                ret.acc_ver = id;
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
                ret.acc_ver = CSeq_id_Handle::GetHandle(text_id->GetAccession()+'.'+
                    NStr::NumericToString(text_id->GetVersion()));
            }
        }
    }
    if (!is_protein) {
        ret.gi.Reset();
        ret.acc_ver.Reset();
    }
    return ret;
}


static string x_MakeLocalCDDEntryId(const SCDDIds& cdd_ids)
{
    ostringstream str;
    _ASSERT(cdd_ids.gi && cdd_ids.gi.IsGi());
    str << kLocalCDDEntryIdPrefix << cdd_ids.gi.GetGi();
    if ( cdd_ids.acc_ver ) {
        str << kLocalCDDEntryIdSeparator << cdd_ids.acc_ver;
    }
    return str.str();
}


static bool x_IsLocalCDDEntryId(const CPsgBlobId& blob_id)
{
    return NStr::StartsWith(blob_id.ToPsgId(), kLocalCDDEntryIdPrefix);
}


static bool x_ParseLocalCDDEntryId(const CPsgBlobId& blob_id, SCDDIds& cdd_ids)
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
    cdd_ids.gi = CSeq_id_Handle::GetGiHandle(GI_FROM(TIntId, gi_id));
    if ( str.get() == kLocalCDDEntryIdSeparator ) {
        string extra;
        str >> extra;
        cdd_ids.acc_ver = CSeq_id_Handle::GetHandle(extra);
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


static CRef<CTSE_Chunk_Info> x_CreateLocalCDDEntryChunk(const SCDDIds& cdd_ids)
{
    if ( !cdd_ids.gi && !cdd_ids.acc_ver ) {
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
    set<CSeq_id_Handle> ids;
    for ( int i = 0; i < 2; ++i ) {
        const CSeq_id_Handle& id = i ? cdd_ids.acc_ver : cdd_ids.gi;
        if ( !id ) {
            continue;
        }
        ids.insert(id);
    }
    if ( s_GetDebugLevel() >= 6 ) {
        for ( auto& id : ids ) {
            LOG_POST(Info<<"CPSGDataLoader: CDD synthetic id "<<MSerial_AsnText<<*id.GetSeqId());
        }
    }
    for ( auto subtype : subtypes ) {
        SAnnotTypeSelector type(subtype);
        for ( auto& id : ids ) {
            chunk->x_AddAnnotType(name, type, id, range);
        }
    }
    return chunk;
}


static CTSE_Lock x_CreateLocalCDDEntry(CDataSource* data_source, const SCDDIds& cdd_ids)
{
    CRef<CPsgBlobId> blob_id(new CPsgBlobId(x_MakeLocalCDDEntryId(cdd_ids)));
    if ( auto chunk = x_CreateLocalCDDEntryChunk(cdd_ids) ) {
        CDataLoader::TBlobId dl_blob_id = CDataLoader::TBlobId(blob_id);
        CTSE_LoadLock load_lock = data_source->GetTSE_LoadLock(dl_blob_id);
        _ASSERT(load_lock);
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


static bool s_HasFailedStatus(const CPSG_NamedAnnotStatus& na_status)
{
    for ( auto& s : na_status.GetId2AnnotStatusList() ) {
        if ( s.second == EPSG_Status::eError ) {
            return true;
        }
    }
    return false;
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
    if ( s_GetDebugLevel() >= 6 ) {
        set<CSeq_id_Handle> annot_ids;
        for ( CTypeConstIterator<CSeq_id> it = ConstBegin(*entry); it; ++it ) {
            annot_ids.insert(CSeq_id_Handle::GetHandle(*it));
        }
        for ( auto& id : annot_ids ) {
            LOG_POST(Info<<"CPSGDataLoader: CDD actual id "<<MSerial_AsnText<<*id.GetSeqId());
        }
    }
    load_lock->SetSeq_entry(*entry);
    chunk->SetLoaded();
    return true;
}


shared_ptr<CPSG_Request_Blob>
CPSGDataLoader_Impl::x_MakeLoadLocalCDDEntryRequest(CDataSource* data_source,
                                                    CDataLoader::TChunk chunk)
{
    _ASSERT(chunk->GetChunkId() == kDelayedMain_ChunkId);
    const CPsgBlobId& blob_id = dynamic_cast<const CPsgBlobId&>(*chunk->GetBlobId());
    _ASSERT(x_IsLocalCDDEntryId(blob_id));
    _ASSERT(!chunk->IsLoaded());
    bool failed = false;
    shared_ptr<CPSG_NamedAnnotInfo> cdd_info;
    shared_ptr<CPSG_NamedAnnotStatus> cdd_status;
    
    // load CDD blob id
    {{
        CPSG_BioId bio_id = x_LocalCDDEntryIdToBioId(blob_id);
        CPSG_Request_NamedAnnotInfo::TAnnotNames names = { kCDDAnnotName };
        _ASSERT(bio_id.GetId().find('|') == NPOS);
        auto request = make_shared<CPSG_Request_NamedAnnotInfo>(bio_id, names);
        request->IncludeData(m_TSERequestMode);
        auto reply = x_SendRequest(request);
        shared_ptr<CPSG_BioseqInfo> bioseq_info;
        shared_ptr<CPSG_BlobInfo> blob_info;
        shared_ptr<CPSG_BlobData> blob_data;
        EPSG_Status psg_status = EPSG_Status::eSuccess;
        for (;;) {
            auto reply_item = reply->GetNextItem(DEFAULT_DEADLINE);
            if (!reply_item) continue;
            if (reply_item->GetType() == CPSG_ReplyItem::eEndOfReply) break;
            EPSG_Status status = reply_item->GetStatus(CDeadline::eInfinite);
            if (status != EPSG_Status::eSuccess) {
                if (psg_status == EPSG_Status::eSuccess) psg_status = status;
                ReportStatus(reply_item, status);
                if ( status == EPSG_Status::eNotFound ) {
                    continue;
                }
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
            if (reply_item->GetType() == CPSG_ReplyItem::eNamedAnnotStatus) {
                cdd_status = static_pointer_cast<CPSG_NamedAnnotStatus>(reply_item);
                if ( s_HasFailedStatus(*cdd_status) ) {
                    failed = true;
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
            NCBI_THROW(CLoaderException, eLoaderFailed, "failed to get CDD blob-id for " + bio_id.Repr()
                + ", status=" + s_ReplyStatusToString(psg_status));
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
    return make_shared<CPSG_Request_Blob>(cdd_info->GetBlobId());
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

    CPSG_TaskGroup group(*m_ThreadPool);
    list<shared_ptr<CPSG_Task_Guard>> guards;
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
                if (m_CDDInfoCache->Find(blob_id.ToPsgId())) {
                    x_CreateEmptyLocalCDDEntry(data_source, *it);
                    continue;
                }
                request = x_MakeLoadLocalCDDEntryRequest(data_source, *it);
                if ( !request ) {
                    continue;
                }
            }
            else {
                request = make_shared<CPSG_Request_Blob>(blob_id.ToPsgId());
            }
            request->IncludeData(m_TSERequestMode);
            auto reply = x_SendRequest(request);
            CRef<CPSG_Blob_Task> task(new CPSG_Blob_Task(reply, group, CSeq_id_Handle(), data_source, *this, true));
            task->SetDLBlobId(dynamic_cast<const CPSG_Request_Blob&>(*reply->GetRequest()).GetBlobId().GetId(),
                              chunk.GetBlobId());
            guards.push_back(make_shared<CPSG_Task_Guard>(*task));
            group.AddTask(task);
        }
        else {
            const CPsgBlobId& blob_id = dynamic_cast<const CPsgBlobId&>(*chunk.GetBlobId());
            auto request = make_shared<CPSG_Request_Chunk>(CPSG_ChunkId(chunk.GetChunkId(),
                                                                        blob_id.GetId2Info()));
            auto reply = x_SendRequest(request);
            CRef<CPSG_LoadChunk_Task> task(new CPSG_LoadChunk_Task(reply, group, *it));
            guards.push_back(make_shared<CPSG_Task_Guard>(*task));
            group.AddTask(task);
        }
    }
    group.WaitAll();
    // check if all chunks are loaded
    size_t failed_count = 0;
    ITERATE(CDataLoader::TChunkSet, it, chunks) {
        const CTSE_Chunk_Info & chunk = **it;
        if (!chunk.IsLoaded()) {
            _TRACE("Failed to load chunk " << chunk.GetChunkId() << " of " << dynamic_cast<const CPsgBlobId&>(*chunk.GetBlobId()).ToPsgId());
            ++failed_count;
        }
    }
    if ( failed_count ) {
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "failed to load "<<failed_count<<" chunks");
    }
}


class CPSG_AnnotRecordsNA_Task : public CPSG_Task
{
public:
    CPSG_AnnotRecordsNA_Task( TReply reply, CPSG_TaskGroup& group)
        : CPSG_Task(reply, group) {}

    ~CPSG_AnnotRecordsNA_Task(void) override {}

    list<shared_ptr<CPSG_NamedAnnotInfo>> m_AnnotInfo;
    shared_ptr<CPSG_NamedAnnotStatus> m_AnnotStatus;

    void Finish(void) override {
        m_AnnotInfo.clear();
        m_AnnotStatus.reset();
    }

protected:
    void ProcessReplyItem(shared_ptr<CPSG_ReplyItem> item) override {
        if (item->GetType() == CPSG_ReplyItem::eNamedAnnotInfo) {
            m_AnnotInfo.push_back(static_pointer_cast<CPSG_NamedAnnotInfo>(item));
        }
        if (item->GetType() == CPSG_ReplyItem::eNamedAnnotStatus) {
            m_AnnotStatus = static_pointer_cast<CPSG_NamedAnnotStatus>(item);
            if ( s_HasFailedStatus(*m_AnnotStatus) ) {
                m_Status = eFailed;
                RequestToCancel();
            }
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
    shared_ptr<CPSG_NamedAnnotStatus> m_AnnotStatus;

    void Finish(void) override {
        m_BioseqInfo.reset();
        m_AnnotInfo.clear();
        m_AnnotStatus.reset();
    }

protected:
    void ProcessReplyItem(shared_ptr<CPSG_ReplyItem> item) override {
        if (item->GetType() == CPSG_ReplyItem::eBioseqInfo) {
            m_BioseqInfo = static_pointer_cast<CPSG_BioseqInfo>(item);
        }
        if (item->GetType() == CPSG_ReplyItem::eNamedAnnotInfo) {
            m_AnnotInfo.push_back(static_pointer_cast<CPSG_NamedAnnotInfo>(item));
        }
        if (item->GetType() == CPSG_ReplyItem::eNamedAnnotStatus) {
            m_AnnotStatus = static_pointer_cast<CPSG_NamedAnnotStatus>(item);
            if ( s_HasFailedStatus(*m_AnnotStatus) ) {
                m_Status = eFailed;
                RequestToCancel();
            }
        }
    }
};

static
pair<CRef<CTSE_Chunk_Info>, string>
s_CreateNAChunk(const CPSG_NamedAnnotInfo& psg_annot_info)
{
    pair<CRef<CTSE_Chunk_Info>, string> ret;
    CRef<CTSE_Chunk_Info> chunk(new CTSE_Chunk_Info(kDelayedMain_ChunkId));
    unsigned main_count = 0;
    unsigned zoom_count = 0;
    // detailed annot info
    set<string> names;
    for ( auto& annot_info_ref : psg_annot_info.GetId2AnnotInfoList() ) {
        if ( s_GetDebugLevel() >= 8 ) {
            LOG_POST(Info<<"PSG loader: "<<psg_annot_info.GetBlobId().GetId()<<" NA info "
                     <<MSerial_AsnText<<*annot_info_ref);
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
    if ( !names.empty() ) {
        ret.first = chunk;
    }
    return ret;
}


CDataLoader::TTSE_LockSet CPSGDataLoader_Impl::GetAnnotRecordsNA(
    CDataSource* data_source,
    const TIds& ids,
    const SAnnotSelector* sel,
    CDataLoader::TProcessedNAs* processed_nas)
{
    return CallWithRetry(bind(&CPSGDataLoader_Impl::GetAnnotRecordsNAOnce, this,
                              data_source, cref(ids), sel, processed_nas),
                         "GetAnnotRecordsNA");
}


bool CPSGDataLoader_Impl::x_CheckAnnotCache(
    const string& name,
    const TIds& ids,
    CDataSource* data_source,
    CDataLoader::TProcessedNAs* processed_nas,
    CDataLoader::TTSE_LockSet& locks)
{
    auto cached = m_AnnotCache->Get(name, *ids.begin());
    if (cached) {
        for (auto& info : cached->infos) {
            CDataLoader::SetProcessedNA(name, processed_nas);
            auto chunk_info = s_CreateNAChunk(*info);
            CRef<CPsgBlobId> blob_id(new CPsgBlobId(info->GetBlobId().GetId()));
            CDataLoader::TBlobId dl_blob_id = CDataLoader::TBlobId(blob_id);
            CTSE_LoadLock load_lock = data_source->GetTSE_LoadLock(dl_blob_id);
            if ( load_lock ) {
                if ( !load_lock.IsLoaded() ) {
                    load_lock->SetName(cached->name);
                    load_lock->GetSplitInfo().AddChunk(*chunk_info.first);
                    _ASSERT(load_lock->x_NeedsDelayedMainChunk());
                    load_lock.SetLoaded();
                }
                locks.insert(load_lock);
            }
        }
        return true;
    }
    return false;
}


CDataLoader::TTSE_LockSet CPSGDataLoader_Impl::GetAnnotRecordsNAOnce(
    CDataSource* data_source,
    const TIds& ids,
    const SAnnotSelector* sel,
    CDataLoader::TProcessedNAs* processed_nas)
{
    CDataLoader::TTSE_LockSet locks;
    if ( !data_source  ||  ids.empty() ) {
        return locks;
    }
    CPSG_Request_NamedAnnotInfo::TAnnotNames annot_names;
    if ( !kCreateLocalCDDEntries && !x_CheckAnnotCache(kCDDAnnotName, ids, data_source, processed_nas, locks) ) {
        annot_names.push_back(kCDDAnnotName);
    }
    auto snp_scale_limit = CSeq_id::eSNPScaleLimit_Default;
    string snp_name = "SNP"; // name used for caching SNP annots with scale-limit.
    if ( sel && sel->IsIncludedAnyNamedAnnotAccession() ) {
        CPSG_BioIds bio_ids;
        for (auto& id : ids) {
            bio_ids.push_back(CPSG_BioId(id));
        }
        const SAnnotSelector::TNamedAnnotAccessions& accs = sel->GetNamedAnnotAccessions();
        ITERATE(SAnnotSelector::TNamedAnnotAccessions, it, accs) {
            if ( kCreateLocalCDDEntries && NStr::EqualNocase(it->first, kCDDAnnotName) ) {
                // CDDs are added as external annotations
                continue;
            }
            string name = it->first;
            if (name == "SNP") {
                snp_scale_limit = sel->GetSNPScaleLimit();
                if (snp_scale_limit == CSeq_id::eSNPScaleLimit_Default) {
                    snp_scale_limit = CPSGDataLoader::GetSNP_Scale_Limit();
                }
                if (snp_scale_limit != CSeq_id::eSNPScaleLimit_Default) {
                    snp_name = "SNP::" + NStr::NumericToString((int)snp_scale_limit);
                    name = snp_name;
                }
            }
            if ( !x_CheckAnnotCache(name, ids, data_source, processed_nas, locks) ) {
                annot_names.push_back(it->first);
            }
        }

        if ( !annot_names.empty() ) {
            auto request = make_shared<CPSG_Request_NamedAnnotInfo>(std::move(bio_ids), annot_names);
            request->SetSNPScaleLimit(snp_scale_limit);
            auto reply = x_SendRequest(request);
            CPSG_TaskGroup group(*m_ThreadPool);
            CRef<CPSG_AnnotRecordsNA_Task> task(new CPSG_AnnotRecordsNA_Task(reply, group));
            CPSG_Task_Guard guard(*task);
            group.AddTask(task);
            group.WaitAll();

            if (task->GetStatus() == CThreadPool_Task::eCompleted) {
                map<string, SPsgAnnotInfo::TInfos> infos_by_name;
                for ( auto& info : task->m_AnnotInfo ) {
                    CDataLoader::SetProcessedNA(info->GetName(), processed_nas);
                    CRef<CPsgBlobId> blob_id(new CPsgBlobId(info->GetBlobId().GetId()));
                    auto chunk_info = s_CreateNAChunk(*info);
                    if ( chunk_info.first ) {
                        infos_by_name[info->GetName()].push_back(info);
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
                if (!ids.empty() && !infos_by_name.empty()) {
                    for(auto infos : infos_by_name) {
                        m_AnnotCache->Add(infos.second, (infos.first == "SNP" ? snp_name : infos.first), ids);
                    }
                }
            }
            else {
                _TRACE("Failed to load annotations for " << ids.begin()->AsString());
                NCBI_THROW(CLoaderException, eLoaderFailed,
                           "CPSGDataLoader::GetAnnotRecordsNA() failed, status=" + s_ReplyStatusToString(task->GetPSGStatus()));
            }
        }
    }
    if ( kCreateLocalCDDEntries ) {
        if ( SCDDIds cdd_ids = x_GetCDDIds(ids) ) {
            if ( auto tse_lock = x_CreateLocalCDDEntry(data_source, cdd_ids) ) {
                locks.insert(tse_lock);
            }
        }
    }
    return locks;
}


void CPSGDataLoader_Impl::PrefetchCDD(const TIds& ids)
{
    if (ids.empty()) return;
    _ASSERT(m_CDDPrefetchTask);

    SCDDIds cdd_ids = x_GetCDDIds(ids);
    if (!cdd_ids) {
        // not a protein or no suitable ids
        return;
    }
    string blob_id = x_MakeLocalCDDEntryId(cdd_ids);

    if (m_CDDInfoCache->Find(blob_id)) {
        // known for no CDDs
        return;
    }
    auto cached = m_AnnotCache->Get(kCDDAnnotName, ids.front());
    if (cached) {
        return;
    }

    CPSG_BioIds bio_ids;
    for (auto& id : ids) {
        bio_ids.push_back(CPSG_BioId(id));
    }
    CPSG_Request_NamedAnnotInfo::TAnnotNames annot_names({kCDDAnnotName});
    auto request = make_shared<CPSG_Request_NamedAnnotInfo>(std::move(bio_ids), annot_names);
    auto reply = x_SendRequest(request);
    for (;;) {
        if (m_CDDPrefetchTask->IsCancelRequested()) return;
        auto reply_item = reply->GetNextItem(DEFAULT_DEADLINE);
        if (!reply_item) continue;
        if (reply_item->GetType() == CPSG_ReplyItem::eEndOfReply) break;
        if (m_CDDPrefetchTask->IsCancelRequested()) return;
        EPSG_Status status = reply_item->GetStatus(CDeadline::eInfinite);
        if (m_CDDPrefetchTask->IsCancelRequested()) return;
        if (status != EPSG_Status::eSuccess) {
            ReportStatus(reply_item, status);
            if ( status == EPSG_Status::eNotFound ) {
                continue;
            }
            if ( status == EPSG_Status::eForbidden ) {
                continue;
            }
            return;
        }
        if (reply_item->GetType() == CPSG_ReplyItem::eNamedAnnotInfo) return;
    }
    // No named annot info returned, mark the bioseq as having no CDDs.
    m_CDDInfoCache->Add(blob_id, true);
}


class CPSG_CDDAnnotBulk_Task : public CPSG_Task
{
public:
    CPSG_CDDAnnotBulk_Task(TReply reply, CPSG_TaskGroup& group, size_t idx)
        : CPSG_Task(reply, group), m_Idx(idx) {}

    ~CPSG_CDDAnnotBulk_Task(void) override {}

    size_t m_Idx;
    shared_ptr<CPSG_NamedAnnotInfo> m_AnnotInfo;
    shared_ptr<CPSG_NamedAnnotStatus> m_AnnotStatus;
    shared_ptr<CPSG_BlobInfo> m_BlobInfo;
    shared_ptr<CPSG_BlobData> m_BlobData;

    void Finish(void) override
    {
        m_AnnotInfo.reset();
        m_BlobInfo.reset();
        m_BlobData.reset();
    }

protected:
    void ProcessReplyItem(shared_ptr<CPSG_ReplyItem> item) override {
        switch (item->GetType()) {
        case CPSG_ReplyItem::eNamedAnnotInfo:
            m_AnnotInfo = static_pointer_cast<CPSG_NamedAnnotInfo>(item);
            break;
        case CPSG_ReplyItem::eNamedAnnotStatus:
            m_AnnotStatus = static_pointer_cast<CPSG_NamedAnnotStatus>(item);
            if ( s_HasFailedStatus(*m_AnnotStatus) ) {
                m_Status = eFailed;
                RequestToCancel();
            }
            break;
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


static bool x_IsEmptyCDD(const CTSE_Info& tse)
{
    // check core Seq-entry content
    auto core = tse.GetTSECore();
    if ( !core->IsSet() ) {
        // wrong entry type
        return false;
    }
    auto& seqset = core->GetSet();
    return seqset.GetSeq_set().empty() && seqset.GetAnnot().empty();
}


void CPSGDataLoader_Impl::GetCDDAnnotsOnce(CDataSource* data_source,
    const TSeqIdSets& id_sets, TLoaded& loaded, TCDD_Locks& ret)
{
    if (id_sets.empty()) return;
    _ASSERT(id_sets.size() == loaded.size());
    _ASSERT(id_sets.size() == ret.size());

    CPSG_Request_NamedAnnotInfo::TAnnotNames annot_names{kCDDAnnotName};
    CPSG_TaskGroup group(*m_ThreadPool);
    vector<SCDDIds> cdd_ids(id_sets.size());
    vector<CRef<CPSG_CDDAnnotBulk_Task>> tasks;
    for (size_t i = 0; i < id_sets.size(); ++i) {
        if ( loaded[i] ) {
            continue;
        }
        const TIds& ids = id_sets[i];
        if ( ids.empty() ) {
            continue;
        }
        cdd_ids[i] = x_GetCDDIds(ids);
        if ( !cdd_ids[i] ) {
            // not a protein or no suitable ids
            loaded[i] = true;
            continue;
        }
        // Skip if it's known that the bioseq has no CDDs.
        if (m_CDDInfoCache->Find(x_MakeLocalCDDEntryId(cdd_ids[i]))) {
            // no CDDs for this Seq-id
            loaded[i] = true;
            continue;
        }
        // Check if there's a loaded CDD blob.
        CTSE_LoadLock cdd_tse;
        for (auto& id : ids) {
            CDataSource::TLoadedBlob_ids blob_ids;
            data_source->GetLoadedBlob_ids(id, CDataSource::fLoaded_bioseq_annots, blob_ids);
            for (auto& bid : blob_ids) {
                if (x_IsLocalCDDEntryId(dynamic_cast<const CPsgBlobId&>(*bid))) {
                    cdd_tse = data_source->GetTSE_LoadLockIfLoaded(bid);
                    break;
                }
            }
            if (cdd_tse) {
                break;
            }
        }
        if (cdd_tse) {
            ret[i] = cdd_tse;
            loaded[i] = true;
            continue;
        }
        CDataLoader::TTSE_LockSet locks;
        if ( x_CheckAnnotCache(kCDDAnnotName, ids, data_source, nullptr, locks) ) {
            _ASSERT(locks.size() == 1);
            const CTSE_Lock& tse = *locks.begin();
            // check if delayed TSE chunk is loaded
            if ( tse->x_NeedsDelayedMainChunk() ) {
                // not loaded yet, need to request CDDs
            }
            else {
                if ( !x_IsEmptyCDD(*tse) ) {
                    ret[i] = tse;
                }
                loaded[i] = true;
                continue;
            }
        }
 
        CPSG_BioIds bio_ids;
        for (auto& id : ids) {
            bio_ids.push_back(CPSG_BioId(id));
        }
        auto request = make_shared<CPSG_Request_NamedAnnotInfo>(
            std::move(bio_ids), annot_names, make_shared<size_t>(i));
        request->IncludeData(CPSG_Request_Biodata::eWholeTSE);
        auto reply = x_SendRequest(request);
        tasks.push_back(Ref(new CPSG_CDDAnnotBulk_Task(reply, group, i)));
        group.AddTask(tasks.back());
    }

    size_t failed_count = 0;
    typedef list<CRef<CPSG_CDDAnnotBulk_Task>> TTasks;
    TTasks skipped_tasks;
    list<shared_ptr<CPSG_Task_Guard>> guards;
    while (group.HasTasks()) {
        CRef<CPSG_CDDAnnotBulk_Task> task(group.GetTask<CPSG_CDDAnnotBulk_Task>().GetNCPointerOrNull());
        _ASSERT(task);
        guards.push_back(make_shared<CPSG_Task_Guard>(*task));
        if (task->GetStatus() == CThreadPool_Task::eFailed) {
            ++failed_count;
            continue;
        }
        auto idx = task->m_Idx;
        if (!task->m_AnnotInfo || !task->m_BlobInfo || !task->m_BlobData) {
            // no CDDs
            m_CDDInfoCache->Add(x_MakeLocalCDDEntryId(cdd_ids[idx]), true);
            loaded[idx] = true;
            continue;
        }
        auto annot_info = task->m_AnnotInfo;
        auto blob_info = task->m_BlobInfo;
        auto blob_data = task->m_BlobData;
        if ( s_SameId(blob_info->GetId<CPSG_BlobId>(), annot_info->GetBlobId()) &&
             s_SameId(blob_data->GetId<CPSG_BlobId>(), annot_info->GetBlobId()) ) {

            CDataLoader::TBlobId dl_blob_id;
            if ( kCreateLocalCDDEntries ) {
                if (!cdd_ids[idx]) {
                    continue;
                }
                dl_blob_id = new CPsgBlobId(x_MakeLocalCDDEntryId(cdd_ids[idx]));
            }
            else {
                SPsgAnnotInfo::TInfos infos{annot_info};
                m_AnnotCache->Add(infos, kCDDAnnotName, id_sets[idx]);
                dl_blob_id = new CPsgBlobId(annot_info->GetBlobId().GetId());
            }

            CTSE_LoadLock load_lock = data_source->GetTSE_LoadLock(dl_blob_id);
            _ASSERT(load_lock);
            if (load_lock.IsLoaded()) {
                if ( load_lock->x_NeedsDelayedMainChunk() ) {
                    // not loaded yet
                }
                else {
                    if ( !x_IsEmptyCDD(*load_lock) ) {
                        ret[idx] = load_lock;
                    }
                    loaded[idx] = true;
                    continue;
                }
            }
            else {
                if ( kCreateLocalCDDEntries ) {
                    x_CreateLocalCDDEntry(data_source, cdd_ids[idx]);
                }
                else {
                    SPsgAnnotInfo::TInfos infos{annot_info};
                    m_AnnotCache->Add(infos, kCDDAnnotName, id_sets[idx]);
                    dl_blob_id = new CPsgBlobId(annot_info->GetBlobId().GetId());
                    m_BlobMap->Add(annot_info->GetBlobId().GetId(), make_shared<SPsgBlobInfo>(*blob_info));
                }
            }
            
            unique_ptr<CObjectIStream> in(GetBlobDataStream(*blob_info, *blob_data));
            if (!in.get()) {
                ++failed_count;
                continue;
            }
            CRef<CSeq_entry> entry(new CSeq_entry);
            *in >> *entry;
            if ( load_lock.IsLoaded() ) {
                // may need to create main chunk
                if ( load_lock->x_NeedsDelayedMainChunk() ) {
                    CTSE_Chunk_Info& chunk = load_lock->GetSplitInfo().GetChunk(kDelayedMain_ChunkId);
                    AutoPtr<CInitGuard> chunk_load_lock = chunk.GetLoadInitGuard();
                    if ( chunk_load_lock.get() && *chunk_load_lock.get() ) {
                        load_lock->SetSeq_entry(*entry);
                        chunk.SetLoaded();
                    }
                }
            }
            else {
                load_lock->SetSeq_entry(*entry);
                load_lock.SetLoaded();
            }
            if ( !x_IsEmptyCDD(*load_lock) ) {
                ret[idx] = load_lock;
            }
            loaded[idx] = true;
        }
    }
    group.WaitAll();
    /*
    NON_CONST_ITERATE(TTasks, it, skipped_tasks) {
        CPSG_CDDAnnotBulk_Task& task = **it;
        SReplyResult result = task.WaitForSkipped();
        if (!result.lock) {
            // Force reloading blob
            result = x_RetryBlobRequest(task.m_ReplyResult.blob_id, data_source, task.m_Id);
        }
        if (result.lock) ret[task.m_Idx] = result.lock;
    }
    */
    if ( failed_count ) {
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "failed to load "<<failed_count<<" CDD annots in bulk request");
    }
}


void CPSGDataLoader_Impl::DropTSE(const CPsgBlobId& /*blob_id*/)
{
}


void CPSGDataLoader_Impl::GetBulkIds(const TIds& ids, TLoaded& loaded, TBulkIds& ret)
{
    CallWithRetry(bind(&CPSGDataLoader_Impl::GetBulkIdsOnce, this,
                       cref(ids), ref(loaded), ref(ret)),
                  "GetBulkIds",
                  m_BulkRetryCount);
}


void CPSGDataLoader_Impl::GetBulkIdsOnce(const TIds& ids, TLoaded& loaded, TBulkIds& ret)
{
    vector<shared_ptr<SPsgBioseqInfo>> infos;
    infos.resize(ret.size());
    auto counts = x_GetBulkBioseqInfo(ids, loaded, infos);
    if ( counts.first ) {
        // have loaded infos
        for (size_t i = 0; i < infos.size(); ++i) {
            if (loaded[i] || !infos[i].get()) continue;
            ITERATE(SPsgBioseqInfo::TIds, it, infos[i]->ids) {
                ret[i].push_back(*it);
            }
            loaded[i] = true;
        }
    }
    if ( counts.second ) {
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "failed to load "<<counts.second<<" seq-ids in bulk request");
    }
}


void CPSGDataLoader_Impl::GetAccVers(const TIds& ids, TLoaded& loaded, TIds& ret)
{
    CallWithRetry(bind(&CPSGDataLoader_Impl::GetAccVersOnce, this,
                       cref(ids), ref(loaded), ref(ret)),
                  "GetAccVers",
                  m_BulkRetryCount);
}


void CPSGDataLoader_Impl::GetAccVersOnce(const TIds& ids, TLoaded& loaded, TIds& ret)
{
    vector<shared_ptr<SPsgBioseqInfo>> infos;
    infos.resize(ret.size());
    auto counts = x_GetBulkBioseqInfo(ids, loaded, infos);
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
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "failed to load "<<counts.second<<" acc.ver in bulk request");
    }
}


void CPSGDataLoader_Impl::GetGis(const TIds& ids, TLoaded& loaded, TGis& ret)
{
    CallWithRetry(bind(&CPSGDataLoader_Impl::GetGisOnce, this,
                       cref(ids), ref(loaded), ref(ret)),
                  "GetGis",
                  m_BulkRetryCount);
}


void CPSGDataLoader_Impl::GetGisOnce(const TIds& ids, TLoaded& loaded, TGis& ret)
{
    vector<shared_ptr<SPsgBioseqInfo>> infos;
    infos.resize(ret.size());
    auto counts = x_GetBulkBioseqInfo(ids, loaded, infos);
    if ( counts.first ) {
        // have loaded infos
        for (size_t i = 0; i < infos.size(); ++i) {
            if (loaded[i] || !infos[i].get()) continue;
            ret[i] = infos[i]->gi;
            loaded[i] = true;
        }
    }
    if ( counts.second ) {
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "failed to load "<<counts.second<<" acc.ver in bulk request");
    }
}


void CPSGDataLoader_Impl::GetLabels(const TIds& ids, TLoaded& loaded, TLabels& ret)
{
    CallWithRetry(bind(&CPSGDataLoader_Impl::GetLabelsOnce, this,
                       cref(ids), ref(loaded), ref(ret)),
                  "GetLabels",
                  m_BulkRetryCount);
}


void CPSGDataLoader_Impl::GetLabelsOnce(const TIds& ids, TLoaded& loaded, TLabels& ret)
{
    vector<shared_ptr<SPsgBioseqInfo>> infos;
    infos.resize(ret.size());
    auto counts = x_GetBulkBioseqInfo(ids, loaded, infos);
    if ( counts.first ) {
        // have loaded infos
        for (size_t i = 0; i < infos.size(); ++i) {
            if (loaded[i] || !infos[i].get()) continue;
            ret[i] = objects::GetLabel(infos[i]->ids);
            if (!ret[i].empty()) loaded[i] = true;
        }
    }
    if ( counts.second ) {
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "failed to load "<<counts.second<<" labels in bulk request");
    }
}


void CPSGDataLoader_Impl::GetSequenceLengths(const TIds& ids, TLoaded& loaded, TSequenceLengths& ret)
{
    CallWithRetry(bind(&CPSGDataLoader_Impl::GetSequenceLengthsOnce, this,
                       cref(ids), ref(loaded), ref(ret)),
                  "GetSequenceLengths",
                  m_BulkRetryCount);
}


void CPSGDataLoader_Impl::GetSequenceLengthsOnce(const TIds& ids, TLoaded& loaded, TSequenceLengths& ret)
{
    vector<shared_ptr<SPsgBioseqInfo>> infos;
    infos.resize(ret.size());
    auto counts = x_GetBulkBioseqInfo(ids, loaded, infos);
    if ( counts.first ) {
        // have loaded infos
        for (size_t i = 0; i < infos.size(); ++i) {
            if (loaded[i] || !infos[i].get()) continue;
            auto length = infos[i]->length;
            ret[i] = length > 0? TSeqPos(length): kInvalidSeqPos;
            loaded[i] = true;
        }
    }
    if ( counts.second ) {
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "failed to load "<<counts.second<<" sequence lengths in bulk request");
    }
}


void CPSGDataLoader_Impl::GetSequenceTypes(const TIds& ids, TLoaded& loaded, TSequenceTypes& ret)
{
    CallWithRetry(bind(&CPSGDataLoader_Impl::GetSequenceTypesOnce, this,
                       cref(ids), ref(loaded), ref(ret)),
                  "GetSequenceTypes",
                  m_BulkRetryCount);
}


void CPSGDataLoader_Impl::GetSequenceTypesOnce(const TIds& ids, TLoaded& loaded, TSequenceTypes& ret)
{
    vector<shared_ptr<SPsgBioseqInfo>> infos;
    infos.resize(ret.size());
    auto counts = x_GetBulkBioseqInfo(ids, loaded, infos);
    if ( counts.first ) {
        // have loaded infos
        for (size_t i = 0; i < infos.size(); ++i) {
            if (loaded[i] || !infos[i].get()) continue;
            ret[i] = infos[i]->molecule_type;
            loaded[i] = true;
        }
    }
    if ( counts.second ) {
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "failed to load "<<counts.second<<" sequence types in bulk request");
    }
}


void CPSGDataLoader_Impl::GetSequenceStates(CDataSource* data_source, const TIds& ids, TLoaded& loaded, TSequenceStates& ret)
{
    CallWithRetry(bind(&CPSGDataLoader_Impl::GetSequenceStatesOnce, this,
                       data_source, cref(ids), ref(loaded), ref(ret)),
                  "GetSequenceStates",
                  m_BulkRetryCount);
}


void CPSGDataLoader_Impl::GetSequenceStatesOnce(CDataSource* data_source, const TIds& ids, TLoaded& loaded, TSequenceStates& ret)
{
    TBioseqAndBlobInfos infos;
    infos.resize(ret.size());
    auto counts = x_GetBulkBioseqAndBlobInfo(data_source, ids, loaded, infos);
    if ( counts.first ) {
        // have loaded infos
        for (size_t i = 0; i < infos.size(); ++i) {
            if (loaded[i] || (!infos[i].first.get() || !infos[i].second.get())) continue;
            auto bioseq_info = infos[i].first;
            auto blob_info = infos[i].second;
            ret[i] = bioseq_info->GetBioseqStateFlags();
            ret[i] |= blob_info->blob_state_flags;
            if (!(bioseq_info->GetBioseqStateFlags() & CBioseq_Handle::fState_dead) &&
                !(bioseq_info->GetChainStateFlags() & CBioseq_Handle::fState_dead)) {
                ret[i] &= ~CBioseq_Handle::fState_dead;
            }
            loaded[i] = true;
        }
    }
    if ( counts.second ) {
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "failed to load "<<counts.second<<" sequence states in bulk request");
    }
}


void CPSGDataLoader_Impl::GetSequenceHashes(const TIds& ids, TLoaded& loaded, TSequenceHashes& ret, THashKnown& known)
{
    CallWithRetry(bind(&CPSGDataLoader_Impl::GetSequenceHashesOnce, this,
                       cref(ids), ref(loaded), ref(ret), ref(known)),
                  "GetSequenceHashes",
                  m_BulkRetryCount);
}


void CPSGDataLoader_Impl::GetSequenceHashesOnce(const TIds& ids, TLoaded& loaded, TSequenceHashes& ret, THashKnown& known)
{
    vector<shared_ptr<SPsgBioseqInfo>> infos;
    infos.resize(ret.size());
    auto counts = x_GetBulkBioseqInfo(ids, loaded, infos);
    if ( counts.first ) {
        // have loaded infos
        for (size_t i = 0; i < infos.size(); ++i) {
            if (loaded[i] || !infos[i].get()) continue;
            ret[i] = infos[i]->hash;
            known[i] = infos[i]->hash != 0;
            loaded[i] = true;
        }
    }
    if ( counts.second ) {
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "failed to load "<<counts.second<<" sequence hashes in bulk request");
    }
}


shared_ptr<CPSG_Reply> CPSGDataLoader_Impl::x_SendRequest(shared_ptr<CPSG_Request> request)
{
    if ( m_RequestContext ) {
        request->SetRequestContext(m_RequestContext);
    }
    return m_Queue->SendRequestAndGetReply(request, DEFAULT_DEADLINE);
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
    auto blob_request = make_shared<CPSG_Request_Blob>(req_blob_id);
    blob_request->IncludeData(m_TSERequestMode);
    auto blob_reply = x_SendRequest(blob_request);
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
            if (ret.blob_info) {
                x_AdjustBlobState(*ret.blob_info, req_idh);
            }
        }
    }
    else if ( !GetGetBlobByIdShouldFail() &&
              (lock_asap || load_lock) &&
              !task->m_ReplyResult.blob_id.empty() &&
              retry &&
              !task->GotNotFound() &&
              !task->GotForbidden() ) {
        // blob is required, but not received, yet blob_id is known, so we retry
        ret = x_RetryBlobRequest(task->m_ReplyResult.blob_id, data_source, req_idh);
        if ( !ret.lock ) {
            _TRACE("Failed to load blob for " << req_idh.AsString()<<" @ "<<CStackTrace());
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "CPSGDataLoader::GetRecords("+req_idh.AsString()+") failed");
        }
    }
    else if ( task->GetStatus() == CThreadPool_Task::eFailed ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "CPSGDataLoader::GetRecords("+req_idh.AsString()+") failed");
    }
    else if ( task->GotNotFound() ) {
        NCBI_THROW_FMT(CLoaderException, eNoData,
                       "CPSGDataLoader: No blob for seq_id="<<req_idh<<" blob_id="<<task->m_ReplyResult.blob_id);
        /*
        // not sure about reaction on timed out replies after eNotFound reply was received
        // most probably it still should be treated as error
        // here's eNoData reply code in case it will be decided to be acceptable
        CBioseq_Handle::TBioseqStateFlags state =
            CBioseq_Handle::fState_no_data;
        if ( task->m_ReplyResult.blob_info ) {
            state |= task->m_ReplyResult.blob_info->blob_state_flags;
        }
        NCBI_THROW2(CBlobStateException, eBlobStateError,
                    "blob state error for "+req_idh.AsString(), state);
        */
    }
    else if ( task->GotForbidden() ) {
        CBioseq_Handle::TBioseqStateFlags state =
            CBioseq_Handle::fState_no_data|CBioseq_Handle::fState_withdrawn;
        if ( task->m_ReplyResult.blob_info ) {
            state |= task->m_ReplyResult.blob_info->blob_state_flags;
        }
        NCBI_THROW2(CBlobStateException, eBlobStateError,
                    "blob state error for "+req_idh.AsString(), state);
    }
    else {
        _TRACE("Failed to load blob for " << req_idh.AsString()<<" @ "<<CStackTrace());
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "CPSGDataLoader::GetRecords("+req_idh.AsString()+") failed");
    }
    if ( !ret.lock && task->GotForbidden() ) {
        CBioseq_Handle::TBioseqStateFlags state =
            CBioseq_Handle::fState_no_data|CBioseq_Handle::fState_withdrawn;
        if ( task->m_ReplyResult.blob_info ) {
            state |= task->m_ReplyResult.blob_info->blob_state_flags;
        }
        NCBI_THROW2(CBlobStateException, eBlobStateError,
                    "blob state error for "+req_idh.AsString(), state);
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


string CPSGDataLoader_Impl::x_GetCachedBlobId(const CSeq_id_Handle& idh)
{
    // Check cache
    if ( auto seq_info = m_BioseqCache->Get(idh) ) {
        return seq_info->blob_id;
    }
    return string();
}


shared_ptr<SPsgBioseqInfo> CPSGDataLoader_Impl::x_GetBioseqInfo(const CSeq_id_Handle& idh)
{
    if ( shared_ptr<SPsgBioseqInfo> ret = m_BioseqCache->Get(idh) ) {
        return ret;
    }

    CPSG_BioId bio_id(idh);
    shared_ptr<CPSG_Request_Resolve> request = make_shared<CPSG_Request_Resolve>(std::move(bio_id));
    request->IncludeInfo(CPSG_Request_Resolve::fAllInfo);
    auto reply = x_SendRequest(request);
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


class CPSG_IpgTaxId_Task : public CPSG_Task
{
public:
    CPSG_IpgTaxId_Task(size_t idx, bool is_wp_acc, TReply reply, CPSG_TaskGroup& group)
        : CPSG_Task(reply, group), m_Idx(idx), m_IsWPAcc(is_wp_acc) {}

    ~CPSG_IpgTaxId_Task(void) override {}

    size_t m_Idx = 0;
    bool m_IsWPAcc = false;
    TTaxId m_TaxId = INVALID_TAX_ID;

    void Finish(void) override {
        m_TaxId = INVALID_TAX_ID;
    }

protected:
    void ProcessReplyItem(shared_ptr<CPSG_ReplyItem> item) override {
        if (m_TaxId != INVALID_TAX_ID) return;
        if (item->GetType() == CPSG_ReplyItem::eIpgInfo) {
            auto ipg_info = static_pointer_cast<CPSG_IpgInfo>(item);
            if (!m_IsWPAcc) {
                m_TaxId = ipg_info->GetTaxId();
            }
            else if (ipg_info->GetNucleotide().empty()) {
                m_TaxId = ipg_info->GetTaxId();
            }
        }
    }
};


static bool s_IsIpgAccession(const CSeq_id_Handle& idh, string& acc_ver, bool& is_wp_acc)
{
    if (!idh) return false;
    auto seq_id = idh.GetSeqId();
    auto text_id = seq_id->GetTextseq_Id();
    if (!text_id) return false;
    CSeq_id::EAccessionInfo acc_info = idh.IdentifyAccession();
    const int kAccFlags = CSeq_id::fAcc_prot | CSeq_id::fAcc_vdb_only;
    if ((acc_info & kAccFlags) != kAccFlags) return false;
    if ( !text_id->IsSetAccession() || !text_id->IsSetVersion() ) return false;
    acc_ver = text_id->GetAccession()+'.'+NStr::NumericToString(text_id->GetVersion());
    is_wp_acc = (acc_info & (CSeq_id::eAcc_division_mask | CSeq_id::eAcc_flag_mask)) == CSeq_id::eAcc_refseq_unique_prot;
    return true;
}


TTaxId CPSGDataLoader_Impl::x_GetIpgTaxId(const CSeq_id_Handle& idh)
{
    if (!m_IpgTaxIdMap) return INVALID_TAX_ID;

    TTaxId cached = m_IpgTaxIdMap->Find(idh);
    if (cached != INVALID_TAX_ID) return cached;

    string acc_ver;
    bool is_wp_acc = false;
    if (!s_IsIpgAccession(idh, acc_ver, is_wp_acc)) return INVALID_TAX_ID;

    shared_ptr<CPSG_Request_IpgResolve> request = make_shared<CPSG_Request_IpgResolve>(acc_ver);
    auto reply = x_SendRequest(request);
    if (!reply) {
        _TRACE("Request failed: null reply");
        NCBI_THROW(CLoaderException, eLoaderFailed, "null reply for "+idh.AsString());
        return INVALID_TAX_ID;
    }

    CPSG_TaskGroup group(*m_ThreadPool);
    CRef<CPSG_IpgTaxId_Task> task(new CPSG_IpgTaxId_Task(0, is_wp_acc, reply, group));
    CPSG_Task_Guard guard(*task);
    group.AddTask(task);
    group.WaitAll();

    if (task->GetStatus() != CThreadPool_Task::eCompleted) {
        _TRACE("Failed to get ipg info for " << idh.AsString() << " @ "<<CStackTrace());
        NCBI_THROW(CLoaderException, eLoaderFailed, "failed to get ipg info for "+idh.AsString());
    }
    m_IpgTaxIdMap->Add(idh, task->m_TaxId);
    return task->m_TaxId;
}


void CPSGDataLoader_Impl::x_GetIpgTaxIds(const TIds& ids, TLoaded& loaded, TTaxIds& ret)
{
    if (!m_IpgTaxIdMap) return;

    CPSG_TaskGroup group(*m_ThreadPool);
    for (size_t i = 0; i < ids.size(); ++i) {
        TTaxId cached = m_IpgTaxIdMap->Find(ids[i]);
        if (cached != INVALID_TAX_ID) {
            ret[i] = cached;
            loaded[i] = true;
            continue;
        }

        string acc_ver;
        bool is_wp_acc = false;
        if (!s_IsIpgAccession(ids[i], acc_ver, is_wp_acc)) continue;

        shared_ptr<CPSG_Request_IpgResolve> request = make_shared<CPSG_Request_IpgResolve>(acc_ver);
        auto reply = x_SendRequest(request);
        if (!reply) {
            _TRACE("Request failed: null reply");
            NCBI_THROW(CLoaderException, eLoaderFailed, "null reply for "+ids[i].AsString());
            continue;
        }

        CRef<CPSG_IpgTaxId_Task> task(new CPSG_IpgTaxId_Task(i, is_wp_acc, reply, group));
        group.AddTask(task);
    }

    list<shared_ptr<CPSG_Task_Guard>> guards;
    int failed_count = 0;
    while (group.HasTasks()) {
        CRef<CPSG_IpgTaxId_Task> task(group.GetTask<CPSG_IpgTaxId_Task>().GetNCPointerOrNull());
        _ASSERT(task);
        guards.push_back(make_shared<CPSG_Task_Guard>(*task));

        if (task->GetStatus() == CThreadPool_Task::eFailed) {
            ++failed_count;
            continue;
        }
        if (task->m_TaxId != INVALID_TAX_ID) {
            m_IpgTaxIdMap->Add(ids[task->m_Idx], task->m_TaxId);
            ret[task->m_Idx] = task->m_TaxId;
            loaded[task->m_Idx] = true;
        }
    }
    if ( failed_count ) {
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "failed to load "<<failed_count<<" tax-ids");
    }
}


CPSGDataLoader_Impl::TBioseqAndBlobInfo
CPSGDataLoader_Impl::x_GetBioseqAndBlobInfo(CDataSource* data_source,
                                            const CSeq_id_Handle& idh)
{
    shared_ptr<SPsgBioseqInfo> bioseq_info = m_BioseqCache->Get(idh);
    shared_ptr<SPsgBlobInfo> blob_info;
    if ( bioseq_info && !bioseq_info->blob_id.empty() ) {
        // get by blob id
        blob_info = x_GetBlobInfo(data_source, bioseq_info->blob_id);
        if (blob_info) {
            x_AdjustBlobState(*blob_info, idh);
        }
    }
    else {
        CPSG_BioId bio_id(idh);
        auto request1 = make_shared<CPSG_Request_Resolve>(bio_id);
        request1->IncludeInfo(CPSG_Request_Resolve::fAllInfo);
        auto request2 = make_shared<CPSG_Request_Biodata>(std::move(bio_id));
        request2->IncludeData(CPSG_Request_Biodata::eNoTSE);
        
        auto reply1 = x_SendRequest(request1);
        auto reply2 = x_SendRequest(request2);
        if ( !reply1 || !reply2 ) {
            NCBI_THROW(CLoaderException, eLoaderFailed, "null reply for "+idh.AsString());
        }

        CPSG_TaskGroup group(*m_ThreadPool);
        CRef<CPSG_BioseqInfo_Task> task1(new CPSG_BioseqInfo_Task(reply1, group));
        CPSG_Task_Guard guard1(*task1);
        group.AddTask(task1);
        CRef<CPSG_Blob_Task> task2(new CPSG_Blob_Task(reply2, group, idh, data_source, *this));
        CPSG_Task_Guard guard2(*task2);
        group.AddTask(task2);
        group.WaitAll();

        if ( task1->GetStatus() != CThreadPool_Task::eCompleted ) {
            _TRACE("Failed to get bioseq info for " << idh.AsString() << " @ "<<CStackTrace());
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "failed to get bioseq info for "+idh.AsString());
        }
        if ( !task1->m_BioseqInfo ) {
            return TBioseqAndBlobInfo();
        }
        bioseq_info = m_BioseqCache->Add(*task1->m_BioseqInfo, idh);

        if ( task2->GetStatus() != CThreadPool_Task::eCompleted ) {
            _TRACE("Failed to get blob info for " << idh.AsString() << " @ "<<CStackTrace());
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "failed to get blob info for "+idh.AsString());
        }
        if ( task2->m_Skipped ) {
            blob_info = task2->WaitForSkipped().blob_info;
            if ( !blob_info ) {
                blob_info = x_GetBlobInfo(data_source, bioseq_info->blob_id);
            }
        }
        else {
            blob_info = task2->m_ReplyResult.blob_info;
        }
    }
    return make_pair(bioseq_info, blob_info);
}


shared_ptr<SPsgBlobInfo> CPSGDataLoader_Impl::x_GetBlobInfo(CDataSource* data_source,
                                                            const string& blob_id)
{
    // lookup cached blob info
    if ( shared_ptr<SPsgBlobInfo> ret = m_BlobMap->Find(blob_id) ) {
        return ret;
    }
    // use blob info from already loaded TSE
    if ( data_source ) {
        CDataLoader::TBlobId dl_blob_id(new CPsgBlobId(blob_id));
        auto load_lock = data_source->GetTSE_LoadLockIfLoaded(dl_blob_id);
        if ( load_lock && load_lock.IsLoaded() ) {
            return make_shared<SPsgBlobInfo>(*load_lock);
        }
    }
    // load blob info from PSG
    CPSG_BlobId bid(blob_id);
    auto request = make_shared<CPSG_Request_Blob>(bid);
    request->IncludeData(CPSG_Request_Biodata::eNoTSE);
    auto reply = x_SendRequest(request);
    return x_ProcessBlobReply(reply, nullptr, CSeq_id_Handle(), false).blob_info;
}


void CPSGDataLoader_Impl::x_AdjustBlobState(SPsgBlobInfo& blob_info, const CSeq_id_Handle idh)
{
    if (!idh) return;
    if (!(blob_info.blob_state_flags & CBioseq_Handle::fState_dead)) return;
    auto seq_info = m_BioseqCache->Get(idh);
    if (!seq_info) return;
    auto seq_state = seq_info->GetBioseqStateFlags();
    auto chain_state = seq_info->GetChainStateFlags();
    if (seq_state == CBioseq_Handle::fState_none &&
        chain_state == CBioseq_Handle::fState_none) {
        blob_info.blob_state_flags &= ~CBioseq_Handle::fState_dead;
    }
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
        load_lock->SetBlobState(psg_blob_info.blob_state_flags);
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
        if ( s_GetDebugLevel() >= 6 ) {
            LOG_POST("calling SetLoaded("<<load_lock->GetBlobId().ToString()<<")");
        }
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
                                               new CZipStreamDecompressor(CZipCompression::fGZip),
                                               CCompressionIStream::fOwnProcessor));
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
    const TIds& ids,
    const TLoaded& loaded,
    TBioseqInfos& ret)
{
    pair<size_t, size_t> counts(0, 0);
    CPSG_TaskGroup group(*m_ThreadPool);
    typedef  map<CRef<CPSG_BioseqInfo_Task>, size_t> TTasks;
    TTasks tasks;
    list<shared_ptr<CPSG_Task_Guard>> guards;
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
        CPSG_BioId bio_id(ids[i]);
        shared_ptr<CPSG_Request_Resolve> request = make_shared<CPSG_Request_Resolve>(std::move(bio_id));
        request->IncludeInfo(CPSG_Request_Resolve::fAllInfo);
        auto reply = x_SendRequest(request);
        CRef<CPSG_BioseqInfo_Task> task(new CPSG_BioseqInfo_Task(reply, group));
        guards.push_back(make_shared<CPSG_Task_Guard>(*task));
        tasks[task] = i;
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
        ret[it->second] = m_BioseqCache->Add(*task->m_BioseqInfo, ids[it->second]);
        counts.first += 1;
    }
    return counts;
}


pair<size_t, size_t> CPSGDataLoader_Impl::x_GetBulkBioseqAndBlobInfo(
    CDataSource* data_source,
    const TIds& ids,
    const TLoaded& loaded,
    TBioseqAndBlobInfos& ret)
{
    pair<size_t, size_t> counts(0, 0);
    CPSG_TaskGroup group(*m_ThreadPool);
    typedef  map<CRef<CPSG_Task>, size_t> TTasks;
    TTasks tasks;
    vector<bool> errors(ids.size());
    list<shared_ptr<CPSG_Task_Guard>> guards;

    for (size_t i = 0; i < ids.size(); ++i) {
        if (loaded[i]) continue;
        if (CannotProcess(ids[i])) continue;
        CPSG_BioId bio_id(ids[i]);
        shared_ptr<SPsgBioseqInfo> bioseq_info = m_BioseqCache->Get(ids[i]);
        shared_ptr<SPsgBlobInfo> blob_info;
        if (bioseq_info && !bioseq_info->blob_id.empty()) {
            ret[i].first = bioseq_info;
            blob_info = m_BlobMap->Find(bioseq_info->blob_id);
            if (blob_info) {
                ret[i].second = blob_info;
                counts.first += 1;
                continue;
            }
        }
        else {
            auto bioseq_request = make_shared<CPSG_Request_Resolve>(bio_id);
            bioseq_request->IncludeInfo(CPSG_Request_Resolve::fAllInfo);
            auto bioseq_reply = x_SendRequest(bioseq_request);
            CRef<CPSG_BioseqInfo_Task> bioseq_task(new CPSG_BioseqInfo_Task(bioseq_reply, group));
            guards.push_back(make_shared<CPSG_Task_Guard>(*bioseq_task));
            tasks[bioseq_task] = i;
            group.AddTask(bioseq_task);
        }
        auto blob_request = make_shared<CPSG_Request_Biodata>(std::move(bio_id));
        blob_request->IncludeData(CPSG_Request_Biodata::eNoTSE);
        auto blob_reply = x_SendRequest(blob_request);
        CRef<CPSG_Blob_Task> blob_task(new CPSG_Blob_Task(blob_reply, group, ids[i], data_source, *this));
        guards.push_back(make_shared<CPSG_Task_Guard>(*blob_task));
        tasks[blob_task] = i;
        group.AddTask(blob_task);
    }
    while (group.HasTasks()) {
        CRef<CPSG_Task> task = group.GetTask<CPSG_Task>();
        _ASSERT(task);
        TTasks::iterator it = tasks.find(task);
        _ASSERT(it != tasks.end());
        size_t i = it->second;

        CRef<CPSG_BioseqInfo_Task> bioseq_task = Ref(dynamic_cast<CPSG_BioseqInfo_Task*>(task.GetNCPointerOrNull()));
        if (bioseq_task) {
            _ASSERT(!ret[i].first);
            if (bioseq_task->GetStatus() == CThreadPool_Task::eFailed) {
                _TRACE("Failed to load bioseq info for " << ids[i].AsString());
                errors[i] = true;
                continue;
            }
            if (!bioseq_task->m_BioseqInfo) {
                _TRACE("No bioseq info for " << ids[i].AsString());
                // not loaded and no failure
                continue;
            }
            ret[i].first = m_BioseqCache->Add(*bioseq_task->m_BioseqInfo, ids[i]);
        }
        else {
            CRef<CPSG_Blob_Task> blob_task = Ref(dynamic_cast<CPSG_Blob_Task*>(task.GetNCPointerOrNull()));
            _ASSERT(blob_task);
            _ASSERT(!ret[i].second);
            if (blob_task->GetStatus() == CThreadPool_Task::eFailed) {
                _TRACE("Failed to load blob info for " << ids[i].AsString());
                errors[i] = true;
                continue;
            }
            if (blob_task->m_Skipped) {
                ret[i].second = blob_task->WaitForSkipped().blob_info;
            }
        }
    }

    for (size_t i = 0; i < ids.size(); ++i) {
        if (errors[i]) {
            counts.second += 1;
            continue;
        }
        if (!ret[i].first) continue;
        if (!ret[i].second) {
            ret[i].second = x_GetBlobInfo(data_source, ret[i].first->blob_id);
            if (!ret[i].second) continue;
        }
        counts.first += 1;
    }
    return counts;
}


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // HAVE_PSG_LOADER
