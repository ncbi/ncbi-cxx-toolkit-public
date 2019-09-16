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
    CPsgClientContext(void) : m_Sema(0, 1) {}
    ~CPsgClientContext(void) {}

    void Wait(void);
    void SetReply(shared_ptr<CPSG_Reply> reply);
    shared_ptr<CPSG_Reply> GetReply(void) { return m_Reply; }

private:
    CSemaphore m_Sema;
    shared_ptr<CPSG_Reply> m_Reply;
};


void CPsgClientContext::Wait(void)
{
    m_Sema.Wait();
}


void CPsgClientContext::SetReply(shared_ptr<CPSG_Reply> reply)
{
    m_Reply = reply;
    m_Sema.Post();
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
// CBioseqCache
/////////////////////////////////////////////////////////////////////////////


CSeq_id_Handle PsgIdToHandle(const CPSG_BioId& id)
{
    string sid = id.Get();
    if (sid.empty()) return CSeq_id_Handle();
    return CSeq_id_Handle::GetHandle(sid);
}


const int kMaxCacheLifespanSeconds = 300;
const size_t kMaxCacheSize = 10000;


class CBioseqCache
{
public:
    CBioseqCache(void) {};
    ~CBioseqCache(void) {};

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


shared_ptr<SPsgBioseqInfo> CBioseqCache::Get(const CSeq_id_Handle& idh)
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


shared_ptr<SPsgBioseqInfo> CBioseqCache::Add(const CPSG_BioseqInfo& info, CSeq_id_Handle req_idh)
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
    if (bioseq_info.IncludedInfo() & CPSG_Request_Resolve::fMoleculeType)
        molecule_type = bioseq_info.GetMoleculeType();

    if (bioseq_info.IncludedInfo() & CPSG_Request_Resolve::fLength)
        length = bioseq_info.GetLength();

    if (bioseq_info.IncludedInfo() & CPSG_Request_Resolve::fState)
        state = bioseq_info.GetState();

    if (bioseq_info.IncludedInfo() & CPSG_Request_Resolve::fTaxId)
        tax_id = bioseq_info.GetTaxId();

    if (bioseq_info.IncludedInfo() & CPSG_Request_Resolve::fHash)
        hash = bioseq_info.GetHash();

    if (bioseq_info.IncludedInfo() & CPSG_Request_Resolve::fCanonicalId) {
        ids.push_back(PsgIdToHandle(bioseq_info.GetCanonicalId()));
    }
    if (bioseq_info.IncludedInfo() & CPSG_Request_Resolve::fOtherIds) {
        vector<CPSG_BioId> other_ids = bioseq_info.GetOtherIds();
        ITERATE(vector<CPSG_BioId>, other_id, other_ids) {
            ids.push_back(PsgIdToHandle(*other_id));
        }
    }
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

CPSGDataLoader_Impl::CPSGDataLoader_Impl(const CGBLoaderParams& params)
    : m_BioseqCache(new CBioseqCache())
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
        catch (CException& ignored) {
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
    auto blob_info = x_FindBlob(blob_id);
    if (!blob_info) {
        // Force load blob-info even if it's being loaded by another thread.
        CPSG_BioId bio_id = x_GetBioId(idh);
        auto context = make_shared<CPsgClientContext>();
        auto request = make_shared<CPSG_Request_Biodata>(move(bio_id), context);
        request->IncludeData(CPSG_Request_Biodata::eNoTSE);
        auto reply = x_ProcessRequest(request);
        blob_id = x_ProcessReply(reply, nullptr, idh).blob_id;
        blob_info = x_FindBlob(blob_id);
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
    CTSE_LoadLock load_lock = x_ProcessReply(reply, data_source, idh).lock;

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
        blob_id = x_ProcessReply(reply, nullptr, idh).blob_id;
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
    CTSE_LoadLock ret = x_ProcessReply(reply, data_source, CSeq_id_Handle()).lock;
    if (!ret || !ret.IsLoaded()) {
        _TRACE("Failed to load blob for " << blob_id.ToPsgId());
    }
    return ret;
}


void CPSGDataLoader_Impl::LoadChunk(const CPsgBlobId& blob_id, CTSE_Chunk_Info& chunk_info)
{
    // Load split blob-info
    auto psg_blob_found = x_FindBlob(blob_id.ToPsgId());
    if (!psg_blob_found) {
        _TRACE("Can not load chunk for unknown blob-id " << blob_id.ToPsgId());
        return; // Blob-id not yet resolved.
    }

    const SPsgBlobInfo& psg_blob_info = *psg_blob_found;
    const string& chunk_blob_id = psg_blob_info.GetBlobIdForChunk(chunk_info.GetChunkId());
    if (chunk_blob_id.empty()) {
        _TRACE("Chunk blob-id not found for " << blob_id.ToPsgId() << ":" << chunk_info.GetChunkId());
        return;
    }

    shared_ptr<CPSG_BlobInfo> blob_info;
    shared_ptr<CPSG_BlobData> blob_data;
    x_GetBlobInfoAndData(chunk_blob_id, blob_info, blob_data);

    if (!blob_info || !blob_data) {
        _TRACE("Failed to get chunk info or data for blob-id " << chunk_blob_id);
        return;
    }

    auto_ptr<CObjectIStream> in(x_GetBlobDataStream(*blob_info, *blob_data));
    if (!in.get()) {
        _TRACE("Failed to open chunk data stream for blob-id " << blob_info->GetId().Get());
        return;
    }

    CRef<CID2S_Chunk> chunk(new CID2S_Chunk);
    *in >> *chunk;
    CSplitParser::Load(chunk_info, *chunk);
    chunk_info.SetLoaded();
}


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
    auto reply = x_ProcessRequest(request);
    if (!reply) {
        _TRACE("Request failed: null reply");
        return move(locks);
    }
    EPSG_Status status = reply->GetStatus(0);
    if (status != EPSG_Status::eSuccess && status != EPSG_Status::eInProgress) {
        x_ReportStatus(reply, status);
        return move(locks);
    }

    for (;;) {
        auto reply_item = reply->GetNextItem(DEFAULT_DEADLINE);
        if (!reply_item) {
            _TRACE("Request failed: null reply item");
            continue;
        }
        if (reply_item->GetType() == CPSG_ReplyItem::eEndOfReply) {
            break;
        }
        status = reply_item->GetStatus(CDeadline::eInfinite);
        if (status != EPSG_Status::eSuccess) {
            x_ReportStatus(reply_item, status);
            return move(locks);
        }

        if (reply_item->GetType() == CPSG_ReplyItem::eNamedAnnotInfo) {
            shared_ptr<CPSG_NamedAnnotInfo> info = static_pointer_cast<CPSG_NamedAnnotInfo>(reply_item);
            CDataLoader::SetProcessedNA(info->GetName(), processed_nas);
            auto blob_id = info->GetBlobId();
            auto tse_lock = GetBlobById(data_source, CPsgBlobId(blob_id.Get()));
            if (tse_lock.IsLoaded()) locks.insert(tse_lock);
        }
    }
    status = reply->GetStatus(CDeadline::eInfinite);
    if (status != EPSG_Status::eSuccess) {
        x_ReportStatus(reply, status);
    }

    return move(locks);
}


void CPSGDataLoader_Impl::DropTSE(const CPsgBlobId& blob_id)
{
    CFastMutexGuard guard(m_Mutex);
    auto blob_it = m_Blobs.find(blob_id.ToPsgId());
    if (blob_it == m_Blobs.end()) {
        return;
    }
    m_Blobs.erase(blob_it);
}


CPSG_BioId CPSGDataLoader_Impl::x_GetBioId(const CSeq_id_Handle& idh)
{
    CConstRef<CSeq_id> id = idh.GetSeqId();
    string label = id->AsFastaString();
    return CPSG_BioId(label, id->Which());
}


shared_ptr<CPSG_Reply> CPSGDataLoader_Impl::x_ProcessRequest(shared_ptr<CPSG_Request> request)
{
    m_Queue->SendRequest(request, DEFAULT_DEADLINE);
    m_Thread->Wake();
    auto context = request->GetUserContext<CPsgClientContext>();
    _ASSERT(context);
    context->Wait();
    return context->GetReply();
}


CPSGDataLoader_Impl::SReplyResult CPSGDataLoader_Impl::x_ProcessReply(
    shared_ptr<CPSG_Reply> reply,
    CDataSource* data_source,
    CSeq_id_Handle req_idh)
{
    SReplyResult ret;

    if (!reply) {
        _TRACE("Request failed: null reply");
        return move(ret);
    }
    EPSG_Status status = reply->GetStatus(0);
    if (status != EPSG_Status::eSuccess && status != EPSG_Status::eInProgress) {
        x_ReportStatus(reply, status);
        return move(ret);
    }

    shared_ptr<CPSG_BioseqInfo> bioseq_info;
    shared_ptr<CPSG_BlobInfo> main_blob_info;
    shared_ptr<CPSG_BlobInfo> split_blob_info;
    shared_ptr<CPSG_BlobInfo> next_blob_info;
    shared_ptr<CPSG_BlobData> blob_data;
    shared_ptr<CPSG_SkippedBlob> skipped;
    string main_blob_id;
    string next_blob_id;
    CDataLoader::TBlobId dl_blob_id;
    shared_ptr<SPsgBlobInfo> psg_blob_info;
    for (;;) {
        auto reply_item = reply->GetNextItem(DEFAULT_DEADLINE);
        if (!reply_item) {
            _TRACE("Request failed: null reply item");
            continue;
        }
        if (reply_item->GetType() == CPSG_ReplyItem::eEndOfReply) {
            break;
        }
        status = reply_item->GetStatus(0);
        if (status != EPSG_Status::eSuccess && status != EPSG_Status::eInProgress) {
            x_ReportStatus(reply_item, status);
            return move(ret);
        }

        switch (reply_item->GetType()) {
        case CPSG_ReplyItem::eBioseqInfo:
        {
            // Only one bioseq-info is allowed per reply.
            _ASSERT(!bioseq_info);
            if (status == EPSG_Status::eInProgress) {
                status = reply_item->GetStatus(CDeadline::eInfinite);
            }
            if (status != EPSG_Status::eSuccess) {
                x_ReportStatus(reply_item, status);
                return move(ret);
            }
            bioseq_info = static_pointer_cast<CPSG_BioseqInfo>(reply_item);
            m_BioseqCache->Add(*bioseq_info, req_idh);
            string id = bioseq_info->GetBlobId().Get();
            _ASSERT(!main_blob_info || main_blob_id == id);
            main_blob_id = id;
            break;
        }
        case CPSG_ReplyItem::eBlobInfo:
        {
            if (status == EPSG_Status::eInProgress) {
                status = reply_item->GetStatus(CDeadline::eInfinite);
            }
            if (status != EPSG_Status::eSuccess) {
                x_ReportStatus(reply_item, status);
                return move(ret);
            }
            auto blob_info = static_pointer_cast<CPSG_BlobInfo>(reply_item);
            string id = blob_info->GetId().Get();
            string split_blob_id = blob_info->GetSplitInfoBlobId().Get();
            if (!split_blob_id.empty()) {
                // Got blob with split info reference - it's the main blob.
                _ASSERT(!main_blob_info);
                main_blob_info = blob_info;
                main_blob_id = id;
            }
            else {
                // No split-info, can be the main blob-info (unsplit) or split-info. Save for later.
                // Only one blob without split info is allowed.
                _ASSERT(!next_blob_info);
                next_blob_info = blob_info;
                next_blob_id = id;
            }
            break;
        }
        case CPSG_ReplyItem::eBlobData:
        {
            blob_data = static_pointer_cast<CPSG_BlobData>(reply_item);
            break;
        }
        case CPSG_ReplyItem::eSkippedBlob:
        {
            if (status == EPSG_Status::eInProgress) {
                status = reply_item->GetStatus(CDeadline::eInfinite);
            }
            if (status != EPSG_Status::eSuccess) {
                x_ReportStatus(reply_item, status);
                return move(ret);
            }
            skipped = static_pointer_cast<CPSG_SkippedBlob>(reply_item);
            break;
        }
        default:
        {
            break;
        }
        }

        if (next_blob_info) {
            // Try to identify blob-info.
            if (main_blob_info) {
                // This is split blob-info.
                split_blob_info = next_blob_info;
                next_blob_info.reset();
            }
            else if (!main_blob_id.empty()) {
                if (next_blob_id == main_blob_id) {
                    main_blob_info = next_blob_info;
                }
                else {
                    split_blob_info = next_blob_info;
                }
                next_blob_info.reset();
            }
        }

        if (!dl_blob_id && !main_blob_id.empty()) {
            CRef<CPsgBlobId> psg_main_id(new CPsgBlobId(main_blob_id));
            dl_blob_id = CDataLoader::TBlobId(psg_main_id);
        }

        if (!main_blob_info) continue;

        // Find or create main blob-info entry.
        if (!psg_blob_info) {
            psg_blob_info = x_FindBlob(main_blob_id);
            if (!psg_blob_info) {
                psg_blob_info = make_shared<SPsgBlobInfo>(*main_blob_info);
                x_AddBlob(main_blob_id, psg_blob_info);
            }
        }

        if (!data_source) continue;
        if (blob_data && !ret.lock) {
            ret.lock = data_source->GetTSE_LoadLock(dl_blob_id);
        }
    }
    if (!main_blob_info && next_blob_info) {
        main_blob_info = next_blob_info;
        main_blob_id = next_blob_id;
        CRef<CPsgBlobId> psg_main_id(new CPsgBlobId(main_blob_id));
        dl_blob_id = CDataLoader::TBlobId(psg_main_id);
        psg_blob_info = x_FindBlob(main_blob_id);
        if (!psg_blob_info) {
            psg_blob_info = make_shared<SPsgBlobInfo>(*main_blob_info);
            x_AddBlob(main_blob_id, psg_blob_info);
        }
    }
    ret.blob_id = main_blob_id;

    status = reply->GetStatus(CDeadline::eInfinite);
    if (status != EPSG_Status::eSuccess) {
        x_ReportStatus(reply, status);
        return move(ret);
    }

    if (!data_source) return move(ret);

    if (skipped) {
        CPSG_SkippedBlob::EReason skip_reason = skipped->GetReason();
        switch (skip_reason) {
        case CPSG_SkippedBlob::eInProgress:
        {
            // Try to wait for the blob to be loaded.
            ret.lock = data_source->GetLoadedTSE_Lock(dl_blob_id, CTimeout::eInfinite);
            break;
        }
        case CPSG_SkippedBlob::eExcluded:
        case CPSG_SkippedBlob::eSent:
            // Check if the blob is already loaded, force loading if necessary.
            ret.lock = data_source->GetTSE_LoadLock(dl_blob_id);
            break;
        default: // unknown
            return move(ret);
        }
        if (ret.lock && ret.lock.IsLoaded()) return move(ret);
        // Request blob by blob-id.
        CPSG_BlobId req_blob_id(split_blob_info ? split_blob_info->GetId().Get() : main_blob_id);
        auto context = make_shared<CPsgClientContext>();
        auto blob_request = make_shared<CPSG_Request_Blob>(req_blob_id, kEmptyStr, context);
        blob_request->IncludeData(m_NoSplit ? CPSG_Request_Biodata::eOrigTSE : CPSG_Request_Biodata::eSmartTSE);
        auto blob_reply = x_ProcessRequest(blob_request);
        return x_ProcessReply(blob_reply, data_source, req_idh);
    }

    if (!main_blob_info) {
        _TRACE("Failed to get blob info for " << ret.blob_id);
        return move(ret);
    }
    // Read blob data (if any) and pass to the data source.
    if (!blob_data) {
        _TRACE("Failed to get blob data for " << ret.blob_id);
        return move(ret);
    }

    if (!ret.lock) {
        ret.lock = data_source->GetTSE_LoadLock(dl_blob_id);
    }
    if (ret.lock.IsLoaded()) return move(ret);
    shared_ptr<CPSG_BlobInfo> blob_info = split_blob_info ? split_blob_info : main_blob_info;
    x_ReadBlobData(*psg_blob_info, *blob_info, *blob_data, ret.lock);
    return move(ret);
}


shared_ptr<SPsgBioseqInfo> CPSGDataLoader_Impl::x_GetBioseqInfo(const CSeq_id_Handle& idh)
{
    shared_ptr<SPsgBioseqInfo> ret = m_BioseqCache->Get(idh);
    if (ret) return ret;

    CPSG_BioId bio_id = x_GetBioId(idh);
    auto context = make_shared<CPsgClientContext>();
    shared_ptr<CPSG_Request_Resolve> request = make_shared<CPSG_Request_Resolve>(move(bio_id), context);
    request->IncludeInfo(CPSG_Request_Resolve::fAllInfo);
    m_Queue->SendRequest(request, DEFAULT_DEADLINE);
    m_Thread->Wake();
    context->Wait();
    auto reply = context->GetReply();
    if (!reply) {
        _TRACE("Request failed: null reply");
        return nullptr;
    }
    EPSG_Status status = reply->GetStatus(0);
    if (status != EPSG_Status::eSuccess && status != EPSG_Status::eInProgress) {
        x_ReportStatus(reply, status);
        return nullptr;
    }

    shared_ptr<CPSG_BioseqInfo> bioseq_info;
    for (;;) {
        auto reply_item = reply->GetNextItem(DEFAULT_DEADLINE);
        if (!reply_item) {
            _TRACE("Request failed: null reply item");
            continue;
        }
        if (reply_item->GetType() == CPSG_ReplyItem::eEndOfReply) {
            break;
        }
        status = reply_item->GetStatus(CDeadline::eInfinite);
        if (status != EPSG_Status::eSuccess) {
            x_ReportStatus(reply_item, status);
            bioseq_info.reset();
            break;
        }

        if (reply_item->GetType() == CPSG_ReplyItem::eBioseqInfo) {
            bioseq_info = static_pointer_cast<CPSG_BioseqInfo>(reply_item);
        }
    }
    status = reply->GetStatus(CDeadline::eInfinite);
    if (status != EPSG_Status::eSuccess) {
        x_ReportStatus(reply, status);
        return nullptr;
    }

    if (!bioseq_info) {
        _TRACE("Failed to get bioseq info for seq-id " << idh.AsString());
        return nullptr;
    }

    return m_BioseqCache->Add(*bioseq_info, idh);
}


shared_ptr<SPsgBlobInfo> CPSGDataLoader_Impl::x_FindBlob(const string& bid)
{
    CFastMutexGuard guard(m_Mutex);
    auto found = m_Blobs.find(bid);
    return found != m_Blobs.end() ? found->second : nullptr;
}


void CPSGDataLoader_Impl::x_AddBlob(const string& bid, shared_ptr<SPsgBlobInfo> blob)
{
    CFastMutexGuard guard(m_Mutex);
    m_Blobs[bid] = blob;
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
    EPSG_Status status = reply->GetStatus(0);
    if (status != EPSG_Status::eSuccess && status != EPSG_Status::eInProgress) {
        x_ReportStatus(reply, status);
        return;
    }

    for (;;) {
        auto reply_item = reply->GetNextItem(DEFAULT_DEADLINE);
        if (!reply_item) {
            _TRACE("Request failed: null reply item");
            continue;
        }
        if (reply_item->GetType() == CPSG_ReplyItem::eEndOfReply) {
            break;
        }
        status = reply_item->GetStatus(CDeadline::eInfinite);
        if (status != EPSG_Status::eSuccess) {
            x_ReportStatus(reply_item, status);
            break;
        }

        switch (reply_item->GetType()) {
        case CPSG_ReplyItem::eBlobInfo:
            blob_info = static_pointer_cast<CPSG_BlobInfo>(reply_item);
            break;
        case CPSG_ReplyItem::eBlobData:
            blob_data = static_pointer_cast<CPSG_BlobData>(reply_item);
            break;
        default:
            continue;
        }
    }
    status = reply->GetStatus(CDeadline::eInfinite);
    if (status != EPSG_Status::eSuccess) {
        x_ReportStatus(reply, status);
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

    auto_ptr<CObjectIStream> in(x_GetBlobDataStream(blob_info, blob_data));
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


CObjectIStream* CPSGDataLoader_Impl::x_GetBlobDataStream(
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


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // HAVE_PSG_LOADER
