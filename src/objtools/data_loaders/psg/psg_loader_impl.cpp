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
#include <objtools/data_loaders/psg/impl/psg_loader_impl.hpp>
#include <util/compress/compress.hpp>
#include <util/compress/stream.hpp>
#include <util/compress/zlib.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>


BEGIN_NCBI_SCOPE

//#define NCBI_USE_ERRCODE_X   PSGLoader
//NCBI_DEFINE_ERR_SUBCODE_X(1);

BEGIN_SCOPE(objects)

NCBI_PARAM_DECL(string, PSG_LOADER, SERVICE_NAME);
NCBI_PARAM_DEF_EX(string, PSG_LOADER, SERVICE_NAME, "psg",
    eParam_NoThread, PSG_LOADER_SERVICE_NAME);

static string GetPSGServiceName(void)
{
    return NCBI_PARAM_TYPE(PSG_LOADER, SERVICE_NAME)::GetDefault();
}


/////////////////////////////////////////////////////////////////////////////
// CPSGBlobId
/////////////////////////////////////////////////////////////////////////////

CPsgBlobId::CPsgBlobId(CTempString str)
    : m_Id(str)
{
}


CPsgBlobId::~CPsgBlobId(void)
{
}


string CPsgBlobId::ToString(void) const
{
    return m_Id;
}


bool CPsgBlobId::operator<(const CBlobId& id) const
{
    const CPsgBlobId* psg2 = dynamic_cast<const CPsgBlobId*>(&id);
    if (!psg2) {
        return LessByTypeId(id);
    }
    return m_Id < psg2->m_Id;
}


bool CPsgBlobId::operator==(const CBlobId& id) const
{
    const CPsgBlobId* psg2 = dynamic_cast<const CPsgBlobId*>(&id);
    return psg2 && m_Id == psg2->m_Id;
}


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
    CPsgClientThread(shared_ptr<CPSG_Queue> queue) : m_Queue(queue), m_WakeSema(0, 1) {}

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
        auto reply = m_Queue->GetNextReply(DEFAULT_DEADLINE);
        if (!reply) {
            continue;
        }
        auto context = reply->GetRequest()->GetUserContext<CPsgClientContext>();
        context->SetReply(reply);
    }
    return nullptr;
}


/////////////////////////////////////////////////////////////////////////////
// CBioseqCache
/////////////////////////////////////////////////////////////////////////////


class CBioseqCache
{
public:
    CBioseqCache(void) {};
    ~CBioseqCache(void) {};

    shared_ptr<SPsgBioseqInfo> Get(const CSeq_id_Handle& idh) const;
    shared_ptr<SPsgBioseqInfo> Add(const CPSG_BioseqInfo& info);

private:
    typedef map<CSeq_id_Handle, shared_ptr<SPsgBioseqInfo> > TIdMap;
    typedef queue<shared_ptr<SPsgBioseqInfo> > TInfoQueue;

    mutable CFastMutex m_Mutex;
    size_t m_MaxSize = 1024;
    TIdMap m_Ids;
    TInfoQueue m_Infos;
};


shared_ptr<SPsgBioseqInfo> CBioseqCache::Get(const CSeq_id_Handle& idh) const
{
    CFastMutexGuard guard(m_Mutex);
    auto found = m_Ids.find(idh);
    return found != m_Ids.end() ? found->second : nullptr;
}


shared_ptr<SPsgBioseqInfo> CBioseqCache::Add(const CPSG_BioseqInfo& info)
{
    CFastMutexGuard guard(m_Mutex);
    shared_ptr<SPsgBioseqInfo> ret = make_shared<SPsgBioseqInfo>(info);
    while (m_Infos.size() > m_MaxSize) {
        auto rm = m_Infos.front();
        m_Infos.pop();
        ITERATE(SPsgBioseqInfo::TIds, id, rm->ids) {
            m_Ids.erase(*id);
        }
    }
    m_Infos.push(ret);
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
      hash(0)
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
        ids.push_back(CSeq_id_Handle::GetHandle(bioseq_info.GetCanonicalId().Get()));
    }
    if (bioseq_info.IncludedInfo() & CPSG_Request_Resolve::fOtherIds) {
        vector<CPSG_BioId> other_ids = bioseq_info.GetOtherIds();
        ITERATE(vector<CPSG_BioId>, other_id, other_ids) {
            ids.push_back(CSeq_id_Handle::GetHandle(other_id->Get()));
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
// SPsgBlobInfo
/////////////////////////////////////////////////////////////////////////////


SPsgBlobInfo::SPsgBlobInfo(const CPSG_BioseqInfo& bioseq_info, const CPSG_BlobInfo& blob_info)
{
    if (bioseq_info.IncludedInfo() & CPSG_Request_Resolve::fCanonicalId) {
        seq_ids.push_back(CSeq_id_Handle::GetHandle(bioseq_info.GetCanonicalId().Get()));
    }
    if (bioseq_info.IncludedInfo() & CPSG_Request_Resolve::fOtherIds) {
        vector<CPSG_BioId> other_ids = bioseq_info.GetOtherIds();
        ITERATE(vector<CPSG_BioId>, other_id, other_ids) {
            seq_ids.push_back(CSeq_id_Handle::GetHandle(other_id->Get()));
        }
    }

    blob_id_main = blob_info.GetId().Get();
    blob_id_split = blob_info.GetSplitInfoBlobId().Get();
    if (!blob_id_split.empty()) {
        for (int chunk_id = 1;; ++chunk_id) {
            string chunk_blob_id = blob_info.GetChunkBlobId(chunk_id).Get();
            if (chunk_blob_id.empty()) break;
            chunks.push_back(chunk_blob_id);
        }
        return;
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


CPSGDataLoader_Impl::CPSGDataLoader_Impl(const CPSGDataLoader::SLoaderParams& params)
    : m_BioseqCache(new CBioseqCache())
{
    string service_name = params.m_ServiceName;
    if (service_name.empty()) {
        service_name = GetPSGServiceName();
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

    shared_ptr<SPsgBlobInfo>  psg_blob_info = x_GetBlobInfo(idh);
    if (psg_blob_info) {
        CTSE_LoadLock lock = x_LoadBlob(*psg_blob_info, *data_source);
        if ((lock->GetBlobState() & CBioseq_Handle::fState_no_data) &&
            (lock->GetBlobState() != CBioseq_Handle::fState_no_data)) {
            NCBI_THROW2(CBlobStateException, eBlobStateError,
                "blob state error for blob " + psg_blob_info->GetDataBlobId(),
                lock->GetBlobState());
        }
        locks.insert(lock);
    }
    return locks;
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
    if (!seq_info) return -1;
    return seq_info->tax_id;
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


CRef<CPsgBlobId> CPSGDataLoader_Impl::GetBlobId(const CSeq_id_Handle& idh)
{
    CRef<CPsgBlobId> blob_id;
    auto psg_blob_info = x_GetBlobInfo(idh);
    if (psg_blob_info && !psg_blob_info->blob_id_main.empty()) {
        blob_id.Reset(new CPsgBlobId(psg_blob_info->blob_id_main));
    }
    return blob_id;
}


CTSE_LoadLock CPSGDataLoader_Impl::GetBlobById(CDataSource* data_source, const CPsgBlobId& blob_id)
{
    CTSE_LoadLock ret;
    auto psg_blob_info = x_GetBlobInfo(blob_id.ToString());
    if (psg_blob_info) {
        ret = x_LoadBlob(*psg_blob_info, *data_source);
    }
    return ret;
}


void CPSGDataLoader_Impl::LoadChunk(const CPsgBlobId& blob_id, CTSE_Chunk_Info& chunk_info)
{
    // Load split blob-info
    const auto psg_blob_found = m_Blobs.find(blob_id.ToString());
    if (psg_blob_found == m_Blobs.end()) return; // Blob-id not yet resolved.

    const SPsgBlobInfo& psg_blob_info = *psg_blob_found->second;
    const string& chunk_blob_id = psg_blob_info.GetBlobIdForChunk(chunk_info.GetChunkId());
    if (chunk_blob_id.empty()) return;

    shared_ptr<CPSG_BlobInfo> blob_info;
    shared_ptr<CPSG_BlobData> blob_data;
    x_GetBlobInfoAndData(chunk_blob_id, blob_info, blob_data);

    if (!blob_info || !blob_data) return;

    auto_ptr<CObjectIStream> in(x_GetBlobDataStream(*blob_info, *blob_data));
    if (!in.get()) return;

    CRef<CID2S_Chunk> chunk(new CID2S_Chunk);
    *in >> *chunk;
    CSplitParser::Load(chunk_info, *chunk);
    chunk_info.SetLoaded();
}


void CPSGDataLoader_Impl::DropTSE(const CPsgBlobId& blob_id)
{
    auto blob_it = m_Blobs.find(blob_id.ToString());
    if (blob_it == m_Blobs.end()) return;
    const SPsgBlobInfo& psg_blob_info = *blob_it->second;
    ITERATE(SPsgBlobInfo::TSeqIds, id_it, psg_blob_info.seq_ids) {
        m_BlobIds.erase(*id_it);
    }
    m_Blobs.erase(blob_it);
}


CPSG_BioId CPSGDataLoader_Impl::x_GetBioId(const CSeq_id_Handle& idh)
{
    CConstRef<CSeq_id> id = idh.GetSeqId();
    string label = id->AsFastaString();
    NStr::ToUpper(label);
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
    if (!x_CheckStatus(reply)) {
        // TODO: Report error.
        return nullptr;
    }

    shared_ptr<CPSG_BioseqInfo> bioseq_info;
    for (;;) {
        auto reply_item = reply->GetNextItem(DEFAULT_DEADLINE);
        if (!reply_item || reply_item->GetType() == CPSG_ReplyItem::eEndOfReply) break;
        if (!x_CheckStatus(reply_item)) {
            // TODO: Report error.
            break;
        }
        if (reply_item->GetType() == CPSG_ReplyItem::eBioseqInfo) {
            bioseq_info = static_pointer_cast<CPSG_BioseqInfo>(reply_item);
        }
    }
    if (!bioseq_info) return nullptr;

    return m_BioseqCache->Add(*bioseq_info);
}


shared_ptr<SPsgBlobInfo> CPSGDataLoader_Impl::x_GetBlobInfo(const CSeq_id_Handle& idh)
{
    auto blob_id_found = m_BlobIds.find(idh);
    if (blob_id_found != m_BlobIds.end()) {
        return x_GetBlobInfo(blob_id_found->second);
    }

    shared_ptr<SPsgBlobInfo> ret;

    CPSG_BioId bio_id = x_GetBioId(idh);
    auto context = make_shared<CPsgClientContext>();
    shared_ptr<CPSG_Request_Biodata> request = make_shared<CPSG_Request_Biodata>(move(bio_id), context);
    request->IncludeData(CPSG_Request_Biodata::fNoTSE);
    m_Queue->SendRequest(request, DEFAULT_DEADLINE);
    m_Thread->Wake();
    context->Wait();
    auto reply = context->GetReply();
    if (!x_CheckStatus(reply)) {
        // TODO: Report error.
        return nullptr;
    }

    shared_ptr<CPSG_BioseqInfo> bioseq_info;
    shared_ptr<CPSG_BlobInfo> blob_info;
    for (;;) {
        auto reply_item = reply->GetNextItem(DEFAULT_DEADLINE);
        if (!reply_item || reply_item->GetType() == CPSG_ReplyItem::eEndOfReply) break;
        if (!x_CheckStatus(reply_item)) {
            // TODO: Report error.
            break;
        }
        if (reply_item->GetType() == CPSG_ReplyItem::eBioseqInfo) {
            bioseq_info = static_pointer_cast<CPSG_BioseqInfo>(reply_item);
        }
        if (reply_item->GetType() == CPSG_ReplyItem::eBlobInfo) {
            blob_info = static_pointer_cast<CPSG_BlobInfo>(reply_item);
        }
    }
    if (!bioseq_info || !blob_info) return nullptr;

    m_BioseqCache->Add(*bioseq_info);

    shared_ptr<SPsgBlobInfo> psg_blob_info = make_shared<SPsgBlobInfo>(*bioseq_info, *blob_info);
    ITERATE(SPsgBlobInfo::TSeqIds, it, psg_blob_info->seq_ids) {
        m_BlobIds[*it] = psg_blob_info->blob_id_main;
    }
    m_Blobs[psg_blob_info->blob_id_main] = psg_blob_info;
    return psg_blob_info;
}


shared_ptr<SPsgBlobInfo> CPSGDataLoader_Impl::x_GetBlobInfo(const string& blob_id)
{
    auto blob_found = m_Blobs.find(blob_id);
    return blob_found != m_Blobs.end() ? blob_found->second : nullptr;
}


CTSE_LoadLock CPSGDataLoader_Impl::x_LoadBlob(const SPsgBlobInfo& psg_blob_info, CDataSource& data_source)
{
    CRef<CPsgBlobId> psg_main_id(new CPsgBlobId(psg_blob_info.blob_id_main));
    CDataLoader::TBlobId main_id(psg_main_id);
    CTSE_LoadLock load_lock = data_source.GetTSE_LoadLock(main_id);

    const string& psg_blob_id = psg_blob_info.GetDataBlobId();

    shared_ptr<CPSG_BlobInfo> blob_info;
    shared_ptr<CPSG_BlobData> blob_data;
    x_GetBlobInfoAndData(psg_blob_id, blob_info, blob_data);

    if (!blob_info || !blob_data) return load_lock;
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
    if (!x_CheckStatus(reply)) {
        // TODO: Report error.
        return;
    }

    for (;;) {
        auto reply_item = reply->GetNextItem(DEFAULT_DEADLINE);
        if (!reply_item || reply_item->GetType() == CPSG_ReplyItem::eEndOfReply) break;
        if (!x_CheckStatus(reply_item)) {
            // TODO: Report error.
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
}


void CPSGDataLoader_Impl::x_ReadBlobData(
    const SPsgBlobInfo& psg_blob_info,
    const CPSG_BlobInfo& blob_info,
    const CPSG_BlobData& blob_data,
    CTSE_LoadLock& load_lock)
{
    int state = 0;
    if (blob_info.IsDead()) state |= CBioseq_Handle::fState_dead;
    if (blob_info.IsSuppressed()) state |= CBioseq_Handle::fState_suppress;
    if (blob_info.IsWithdrawn()) state |= CBioseq_Handle::fState_withdrawn;
    Uint8 ver = blob_info.GetVersion() / 60000;
    load_lock->SetBlobVersion(CTSE_Info::TBlobVersion(ver));
    load_lock->SetBlobState(state);

    auto_ptr<CObjectIStream> in(x_GetBlobDataStream(blob_info, blob_data));
    if (!in.get()) return;

    if (psg_blob_info.IsSplit()) {
        CRef<CID2S_Split_Info> split_info(new CID2S_Split_Info);
        *in >> *split_info;
        CSplitParser::Attach(*load_lock, *split_info);
        load_lock.SetLoaded();
    }
    else {
        CRef<CSeq_entry> entry(new CSeq_entry);
        *in >> *entry;
        load_lock->SetSeq_entry(*entry);
        load_lock.SetLoaded();
    }
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
        ERR_POST("Unsupported data compression: '" << blob_info.GetCompression() << "'");
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
        ERR_POST("Unsupported data format: '" << blob_info.GetFormat() << "'");
        return nullptr;
    }
    _ASSERT(ret);
    z_stream.release();
    return ret;
}


END_SCOPE(objects)
END_NCBI_SCOPE
