/* $Id$
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
 * Author:  Aleksey Grichenko
 *
 * File Description:
 *   RPC client for CDD annotations
 *
 */

#include <ncbi_pch.hpp>
#include <serial/objostrjson.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/id2/ID2_Blob_Id.hpp>
#include <objtools/data_loaders/cdd/cdd_access/CDD_Request.hpp>
#include <objtools/data_loaders/cdd/cdd_access/cdd_client.hpp>
#include <objtools/data_loaders/cdd/cdd_access/cdd_access__.hpp>

BEGIN_NCBI_SCOPE

NCBI_PARAM_ENUM_DECL(objects::CCDDClient::EDataFormat, CDD, data_format);
NCBI_PARAM_ENUM_ARRAY(objects::CCDDClient::EDataFormat, CDD, data_format)
{
    // Deliberately not covering eDefaultFormat!
    { "JSON",        objects::CCDDClient::eJSON       },
    { "semi-binary", objects::CCDDClient::eSemiBinary },
    { "binary",      objects::CCDDClient::eBinary     }
};

NCBI_PARAM_ENUM_DEF(objects::CCDDClient::EDataFormat, CDD, data_format,
                    objects::CCDDClient::eBinary);

typedef NCBI_PARAM_TYPE(CDD, data_format) TDataFormatParam;

BEGIN_objects_SCOPE // namespace ncbi::objects::

static
ESerialDataFormat s_GetSerialFormat(CCDDClient::EDataFormat& data_format)
{
    if (data_format == CCDDClient::eDefaultFormat) {
        data_format = TDataFormatParam::GetDefault();
    }
    return data_format == CCDDClient::eJSON ? eSerial_Json : eSerial_AsnBinary;
}

CCDDClient::CCDDClient(const string& service_name, EDataFormat data_format)
    : Tparent(service_name.empty() ? DEFAULT_CDD_SERVICE_NAME : service_name,
              s_GetSerialFormat(data_format)),
      m_DataFormat(data_format)
{
    if (data_format == eBinary) {
        SetArgs("binary=1");
    }
}


CCDDClient::~CCDDClient(void)
{
}


void CCDDClient::Ask(const CCDD_Request_Packet& request, CCDD_Reply& reply)
{
    m_Replies.clear();
    Tparent::Ask(request, reply);
}


void CCDDClient::WriteRequest(CObjectOStream& out,
                              const CCDD_Request_Packet& request)
{
    static const TSerial_Format_Flags kJSONFlags
        = fSerial_Json_NoIndentation | fSerial_Json_NoEol;
    if (m_DataFormat == eSemiBinary) {
        CNcbiOstrstream oss;
        CObjectOStreamJson joos(oss, eNoOwnership);
        joos.SetFormattingFlags(kJSONFlags);
        joos << request;
        string s = CNcbiOstrstreamToString(oss);
        SetArgs("binary=1&requestPacket="
                + NStr::URLEncode(s, NStr::eUrlEnc_URIQueryValue));
        x_Connect();
    } else {
        if (m_DataFormat == eJSON) {
            out.SetFormattingFlags(kJSONFlags);
        }
        Tparent::WriteRequest(out, request);
    }
}


void CCDDClient::ReadReply(CObjectIStream& in, CCDD_Reply& reply)
{
    m_Replies.clear();
    CRef<CCDD_Reply> next_reply;
    do {
        next_reply.Reset(new CCDD_Reply);
        in >> *next_reply;
        m_Replies.push_back(next_reply);
    } while (!next_reply->IsSetEnd_of_reply());

    if (!m_Replies.empty()) {
        reply.Assign(*m_Replies.back());
    }
}


CRef<CCDD_Reply> CCDDClient::AskBlobId(int serial_number, const CSeq_id& seq_id)
{
    CCDD_Request_Packet cdd_packet;
    CRef<CCDD_Request> cdd_request(new CCDD_Request);
    cdd_request->SetSerial_number(serial_number);
    cdd_request->SetRequest().SetGet_blob_id().Assign(seq_id);
    cdd_packet.Set().push_back(cdd_request);
    CRef<CCDD_Reply> cdd_reply(new CCDD_Reply);
    try {
        Ask(cdd_packet, *cdd_reply);
    }
    catch (exception& e) {
        ERR_POST(e.what());
        return null;
    }
    return cdd_reply;
}


CRef<CCDD_Reply> CCDDClient::AskBlob(int serial_number, const CID2_Blob_Id& blob_id)
{
    CCDD_Request_Packet cdd_packet;
    CRef<CCDD_Request> cdd_request(new CCDD_Request);
    cdd_request->SetSerial_number(serial_number);
    cdd_request->SetRequest().SetGet_blob().Assign(blob_id);
    cdd_packet.Set().push_back(cdd_request);
    CRef<CCDD_Reply> cdd_reply(new CCDD_Reply);
    try {
        Ask(cdd_packet, *cdd_reply);
    }
    catch (exception& e) {
        ERR_POST(e.what());
        return null;
    }
    return cdd_reply;
}


void CCDDClient::JustAsk(const CCDD_Request_Packet& request)
{
    static const STimeout kZeroTimeout = { 0, 0 };
    Connect();
    WriteRequest(*m_Out, request);
    dynamic_cast<CConn_ServiceStream&>(*m_Stream).Fetch(&kZeroTimeout);
}


/////////////////////////////////////////////////////////////////////////////
// CCDDBlobCache
/////////////////////////////////////////////////////////////////////////////


const int kMaxCacheLifespanSeconds = 300;
const size_t kMaxCacheSize = 1000;

struct SCDDCacheInfo
{
    typedef CCDDClientPool::SCDDBlob TBlob;

    SCDDCacheInfo(const CSeq_id_Handle idh, const TBlob& b)
        : id(idh), blob(b), deadline(kMaxCacheLifespanSeconds) {}

    CSeq_id_Handle id;
    TBlob blob;
    CDeadline deadline;

private:
    SCDDCacheInfo(const SCDDCacheInfo&);
    SCDDCacheInfo& operator=(const SCDDCacheInfo&);
};


class CCDDBlobCache
{
public:
    CCDDBlobCache(void) {}
    ~CCDDBlobCache(void) {}

    typedef CCDDClientPool::TBlobId TBlobId;
    typedef CCDDClientPool::SCDDBlob TBlob;

    TBlob Get(const CSeq_id_Handle& seq_id);
    TBlob Get(const TBlobId& blob_id);
    void Add(TBlob blob);

private:
    void x_UpdateDeadline(shared_ptr<SCDDCacheInfo>& info);
    typedef map<CSeq_id_Handle, shared_ptr<SCDDCacheInfo> > TSeqIdMap;
    typedef map <string, shared_ptr<SCDDCacheInfo>> TBlobIdMap;
    typedef list<shared_ptr<SCDDCacheInfo>> TInfoQueue;

    mutable CFastMutex m_Mutex;
    TSeqIdMap m_SeqIdMap;
    TBlobIdMap m_BlobIdMap;
    TInfoQueue m_Infos;
};


CCDDBlobCache::TBlob CCDDBlobCache::Get(const CSeq_id_Handle& seq_id)
{
    CFastMutexGuard guard(m_Mutex);
    auto found = m_SeqIdMap.find(seq_id);
    if (found == m_SeqIdMap.end()) return TBlob();
    shared_ptr<SCDDCacheInfo> info = found->second;
    x_UpdateDeadline(info);
    return info->blob;
}


CCDDBlobCache::TBlob CCDDBlobCache::Get(const TBlobId& blob_id)
{
    CFastMutexGuard guard(m_Mutex);
    string sid = CCDDClientPool::BlobIdToString(blob_id);
    auto found = m_BlobIdMap.find(sid);
    if (found == m_BlobIdMap.end()) return TBlob();
    shared_ptr<SCDDCacheInfo> info = found->second;
    x_UpdateDeadline(info);
    return info->blob;
}


void CCDDBlobCache::Add(TBlob blob)
{
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(blob.info->GetSeq_id());
    CFastMutexGuard guard(m_Mutex);
    auto found = m_SeqIdMap.find(idh);
    if (found != m_SeqIdMap.end()) return;
    // Create new entry.
    shared_ptr<SCDDCacheInfo> info = make_shared<SCDDCacheInfo>(idh, blob);
    while (!m_Infos.empty() && (m_Infos.size() > kMaxCacheSize || m_Infos.front()->deadline.IsExpired())) {
        auto rm = m_Infos.front();
        m_Infos.pop_front();
        m_SeqIdMap.erase(rm->id);
        m_BlobIdMap.erase(CCDDClientPool::BlobIdToString(rm->blob.info->GetBlob_id()));
    }
    m_Infos.push_back(info);
    m_SeqIdMap[idh] = info;
    m_BlobIdMap[CCDDClientPool::BlobIdToString(info->blob.info->GetBlob_id())] = info;
}


void CCDDBlobCache::x_UpdateDeadline(shared_ptr<SCDDCacheInfo>& info)
{
    m_Infos.remove(info);
    info->deadline = CDeadline(kMaxCacheLifespanSeconds);
    m_Infos.push_back(info);
}


/////////////////////////////////////////////////////////////////////////////
// CCDDClientGuard
/////////////////////////////////////////////////////////////////////////////


class CCDDClientGuard
{
public:
    CCDDClientGuard(CCDDClientPool& pool)
        : m_Pool(pool)
    {
        m_Client = m_Pool.x_GetClient();
    }

    ~CCDDClientGuard(void) {
        m_Pool.x_ReleaseClient(m_Client);
    }

    CCDDClient& Get(void) {
        return *m_Client->second;
    }

    void Discard(void)
    {
        m_Pool.x_DiscardClient(m_Client);
    }

private:
    CCDDClientPool&         m_Pool;
    CCDDClientPool::TClient m_Client;
};


/////////////////////////////////////////////////////////////////////////////
// CCDDClientPool
/////////////////////////////////////////////////////////////////////////////


int CCDDClientPool::x_NextSerialNumber(void)
{
    static CAtomicCounter_WithAutoInit s_Counter;
    return int(s_Counter.Add(1));
}


CCDDClientPool::CCDDClientPool(const string& service_name,
    size_t pool_soft_limit,
    time_t pool_age_limit,
    bool exclude_nucleotides)
{
    m_ServiceName = service_name;
    m_PoolSoftLimit = pool_soft_limit;
    m_PoolAgeLimit = pool_age_limit;
    m_ExcludeNucleotides = exclude_nucleotides;
    m_Cache.reset(new CCDDBlobCache);
}


CCDDClientPool::~CCDDClientPool(void)
{
}


CCDDClientPool::SCDDBlob CCDDClientPool::GetBlobBySeq_id(CSeq_id_Handle idh)
{
    SCDDBlob ret = m_Cache->Get(idh);
    if (ret.data) return ret;

    if (ret.info) {
        // Have blob info only, request blob data.
        ret.data = x_RequestBlobData(ret.info->GetBlob_id());
        if (ret.data) {
            m_Cache->Add(ret);
        }
        return ret;
    }

    // Make a blob-by-seq-id request.
    int serial = x_NextSerialNumber();
    CCDD_Request_Packet cdd_packet;
    CRef<CCDD_Request> cdd_request(new CCDD_Request);
    cdd_request->SetSerial_number(serial);


    CConstRef<CSeq_id> id(idh.GetSeqId());
    if (!IsValidId(*id)) return ret;
    CRef<CSeq_id> nc_id(new CSeq_id);
    nc_id->Assign(*id);
    cdd_request->SetRequest().SetGet_blob_by_seq_id(*nc_id);
    cdd_packet.Set().push_back(cdd_request);

    CCDDClientGuard client(*this);
    CRef<CCDD_Reply> cdd_reply(new CCDD_Reply);
    try {
        client.Get().Ask(cdd_packet, *cdd_reply);
        if (!x_CheckReply(cdd_reply, serial, CCDD_Reply::TReply::e_Get_blob_by_seq_id)) {
            return ret;
        }
        auto& cdd_blob = cdd_reply->SetReply().SetGet_blob_by_seq_id();
        ret.info.Reset(&cdd_blob.SetBlob_id());
        ret.data.Reset(&cdd_blob.SetBlob());
        m_Cache->Add(ret);
    }
    catch (exception& e) {
        ERR_POST("CDD - get-blob-by-seq-id request failed: " << e.what());
        client.Discard();
    }
    catch (...) {
        client.Discard();
    }
    return ret;
}


CCDDClientPool::SCDDBlob CCDDClientPool::GetBlobBySeq_ids(const TSeq_idSet& ids)
{
    SCDDBlob ret;
    ITERATE(TSeq_idSet, id_it, ids) {
        ret = m_Cache->Get(*id_it);
        if (ret.info) break;
    }

    if (ret.data) return ret;

    if (ret.info) {
        // Have blob info only, request blob data.
        ret.data = x_RequestBlobData(ret.info->GetBlob_id());
        if (ret.data) {
            m_Cache->Add(ret);
        }
        return ret;
    }

    // Make a blob-by-seq-id request.
    int serial = x_NextSerialNumber();
    CCDD_Request_Packet cdd_packet;
    CRef<CCDD_Request> cdd_request(new CCDD_Request);
    cdd_request->SetSerial_number(serial);

    list<CRef<CSeq_id>>& req_ids = cdd_request->SetRequest().SetGet_blob_by_seq_ids();
    ITERATE(TSeq_idSet, id_it, ids) {
        CConstRef<CSeq_id> id(id_it->GetSeqId());
        if (!IsValidId(*id)) continue;
        CRef<CSeq_id> nc_id(new CSeq_id);
        nc_id->Assign(*id);
        req_ids.push_back(nc_id);
    }
    if (req_ids.empty()) return ret;
    cdd_packet.Set().push_back(cdd_request);

    CCDDClientGuard client(*this);
    CRef<CCDD_Reply> cdd_reply(new CCDD_Reply);
    try {
        client.Get().Ask(cdd_packet, *cdd_reply);
        if (!x_CheckReply(cdd_reply, serial, CCDD_Reply::TReply::e_Get_blob_by_seq_id)) {
            return ret;
        }
        auto& cdd_blob = cdd_reply->SetReply().SetGet_blob_by_seq_id();
        ret.info.Reset(&cdd_blob.SetBlob_id());
        ret.data.Reset(&cdd_blob.SetBlob());
        m_Cache->Add(ret);
    }
    catch (exception& e) {
        ERR_POST("CDD - get-blob-by-seq-ids request failed: " << e.what());
        client.Discard();
    }
    catch (...) {
        client.Discard();
    }
    return ret;
}


CCDDClientPool::TBlobInfo CCDDClientPool::GetBlobIdBySeq_id(CSeq_id_Handle idh)
{
    SCDDBlob blob = m_Cache->Get(idh);
    if (blob.info) return blob.info;

    int serial = x_NextSerialNumber();
    CCDD_Request_Packet cdd_packet;
    CRef<CCDD_Request> cdd_request(new CCDD_Request);
    cdd_request->SetSerial_number(serial);

    CConstRef<CSeq_id> id(idh.GetSeqId());
    if (!IsValidId(*id)) return blob.info;

    cdd_request->SetRequest().SetGet_blob_id().Assign(*id);
    cdd_packet.Set().push_back(cdd_request);

    CCDDClientGuard client(*this);
    CRef<CCDD_Reply> cdd_reply(new CCDD_Reply);
    try {
        client.Get().Ask(cdd_packet, *cdd_reply);
        if (!x_CheckReply(cdd_reply, serial, CCDD_Reply::TReply::e_Get_blob_id)) {
            return blob.info;
        }
        blob.info.Reset(&cdd_reply->SetReply().SetGet_blob_id());
        m_Cache->Add(blob);
    }
    catch (exception& e) {
        ERR_POST("CDD - get-blob-id request failed: " << e.what());
        client.Discard();
    }
    catch (...) {
        client.Discard();
    }
    return blob.info;
}


CCDDClientPool::TBlobData CCDDClientPool::GetBlobByBlobId(const TBlobId& blob_id)
{
    SCDDBlob blob = m_Cache->Get(blob_id);
    if (blob.data) return blob.data;

    blob.data = x_RequestBlobData(blob_id);
    if (blob.info && blob.data) {
        m_Cache->Add(blob);
    }
    return blob.data;
}


CCDDClientPool::TBlobData CCDDClientPool::x_RequestBlobData(const TBlobId& blob_id)
{
    TBlobData ret;
    int serial = x_NextSerialNumber();
    CCDD_Request_Packet cdd_packet;
    CRef<CCDD_Request> cdd_request(new CCDD_Request);
    cdd_request->SetSerial_number(serial);

    auto& req = cdd_request->SetRequest().SetGet_blob();
    req.SetSat(blob_id.GetSat());
    req.SetSub_sat(blob_id.GetSub_sat());
    req.SetSat_key(blob_id.GetSat_key());
    cdd_packet.Set().push_back(cdd_request);

    CCDDClientGuard client(*this);
    CRef<CCDD_Reply> cdd_reply(new CCDD_Reply);
    try {
        client.Get().Ask(cdd_packet, *cdd_reply);
        if (!x_CheckReply(cdd_reply, serial, CCDD_Reply::TReply::e_Get_blob)) {
            return ret;
        }
        ret.Reset(&cdd_reply->SetReply().SetGet_blob());
    }
    catch (exception& e) {
        ERR_POST("CDD - get-blob request failed: " << e.what());
        client.Discard();
    }
    catch (...) {
        client.Discard();
    }
    return ret;
}


bool CCDDClientPool::x_CheckReply(CRef<CCDD_Reply>& reply, int serial, CCDD_Reply::TReply::E_Choice choice)
{
    if (!reply) return false;
    if (reply->GetReply().IsEmpty() && !reply->IsSetError()) return false;
    if (reply->IsSetError()) {
        const CCDD_Error& e = reply->GetError();
        ERR_POST("CDD - reply error: " << e.GetMessage() << " (code " << e.GetCode() << ", severity " << (int)e.GetSeverity() << ").");
        return false;
    }
    if (reply->GetSerial_number() != serial) {
        ERR_POST("CDD - serial number mismatch: " << serial << " != " << reply->GetSerial_number());
        return false;
    }
    if (reply->GetReply().Which() != choice) {
        ERR_POST("CDD - wrong reply type: " << reply->GetReply().Which() << " != " << choice);
        return false;
    }
    return true;
}


bool CCDDClientPool::IsValidId(const CSeq_id& id)
{
    switch (id.Which()) {
    case CSeq_id::e_not_set:
    case CSeq_id::e_Local:
    case CSeq_id::e_Gibbsq:
    case CSeq_id::e_Gibbmt:
    case CSeq_id::e_Giim:
    case CSeq_id::e_Patent:
    case CSeq_id::e_General:
    case CSeq_id::e_Gpipe:
    case CSeq_id::e_Named_annot_track:
        // These seq-ids are not used in CDD.
        return false;
    case CSeq_id::e_Gi:
    case CSeq_id::e_Pdb:
        // Non-text seq-ids present in CDD.
        return true;
    default:
        break;
    }
    // For text seq-ids check accession type.
    if (m_ExcludeNucleotides && (id.IdentifyAccession() & CSeq_id::fAcc_nuc) != 0) return false;
    return true;
}


CCDDClientPool::TClient CCDDClientPool::x_GetClient()
{
    TClientPool::iterator ret = m_InUse.end();
    time_t now;
    CTime::GetCurrentTimeT(&now);
    time_t cutoff = now - m_PoolAgeLimit;
    CFastMutexGuard guard(m_PoolLock);
    TClientPool::iterator it = m_NotInUse.lower_bound(cutoff);
    if (it == m_NotInUse.end()) {
        CRef<CCDDClient> client(new CCDDClient(m_ServiceName));
        ret = m_InUse.emplace(now, client);
    }
    else {
        ret = m_InUse.insert(*it);
        ++it;
    }
    m_NotInUse.erase(m_NotInUse.begin(), it);
    return ret;
}


void CCDDClientPool::x_ReleaseClient(TClientPool::iterator& client)
{
    time_t now;
    CTime::GetCurrentTimeT(&now);
    time_t cutoff = now - m_PoolAgeLimit;
    CFastMutexGuard guard(m_PoolLock);
    m_NotInUse.erase(m_NotInUse.begin(), m_NotInUse.lower_bound(cutoff));
    if (client != m_InUse.end()) {
        if (client->first >= cutoff
            && m_InUse.size() + m_NotInUse.size() <= m_PoolSoftLimit) {
            m_NotInUse.insert(*client);
        }
        m_InUse.erase(client);
        client = m_InUse.end();
    }
}


void CCDDClientPool::x_DiscardClient(TClient& client)
{
    CFastMutexGuard guard(m_PoolLock);
    m_InUse.erase(client);
    client = m_InUse.end();
}


string CCDDClientPool::BlobIdToString(const TBlobId& blob_id)
{
    ostringstream s;
    s << blob_id.GetSat() << '/' << blob_id.GetSub_sat() << '.' << blob_id.GetSat_key();
    return s.str();
}

CRef<CCDDClientPool::TBlobId> CCDDClientPool::StringToBlobId(const string& s)
{
    CRef<TBlobId> ret;
    try {
        vector<string> parts;
        NStr::Split(s, "/.", parts);
        if (parts.size() != 3) return ret;
        CRef<TBlobId> blob_id(new TBlobId);
        blob_id->SetSat(NStr::StringToNumeric<TBlobId::TSat>(parts[0]));
        blob_id->SetSub_sat(NStr::StringToNumeric<TBlobId::TSat>(parts[1]));
        blob_id->SetSat_key(NStr::StringToNumeric<TBlobId::TSat>(parts[2]));
        ret = blob_id;
    }
    catch (...) {}
    return ret;
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/* Original file checksum: lines: 57, chars: 1749, CRC32: ab618a22 */
