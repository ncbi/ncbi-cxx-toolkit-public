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
 * Authors: Aleksey Grichenko, Eugene Vasilchenko
 *
 * File Description: client for reading SNP data
 *
 */

#include <ncbi_pch.hpp>

#include "snp_client.hpp"
#include "pubseq_gateway.hpp"
#include "pubseq_gateway_logging.hpp"
#include <objects/seqloc/seqloc__.hpp>
#include <objects/general/general__.hpp>
#include <objects/seqres/seqres__.hpp>
#include <objects/seqsplit/seqsplit__.hpp>
#include <objects/id2/ID2_Blob_Id.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqset/seqset__.hpp>


BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(psg);
BEGIN_NAMESPACE(snp);

USING_SCOPE(objects);


BEGIN_LOCAL_NAMESPACE;

// Blob id:
// sat = 2001-2099 : SNP NA version 1 - 999
// or, for primary SNP track:
// sat = 3001-3099 : SNP NA version 1 - 999
// subsat : NA accession number
// or, for primary SNP graph track:
// NA accession number + kSNPSubSatGraph(=1000000000)
// satkey : SequenceIndex + 1000000*FilterIndex;
// satkey bits 24-30: 


const int kSNPSatBase = 2000;
const int kSNPSatPrimary = 3000;
const int kSNPSubSatGraph = 1000000000;
const int kNAVersionMin = 1;
const int kNAVersionMax = 999;
const int kSeqIndexCount = 1000000;
const int kFilterIndexCount = 2000;
const int kFilterIndexMaxLength = 4;


static string s_GetAccVer(const CSeq_id_Handle& id)
{
    string ret;
    CConstRef<CSeq_id> seq_id(id.GetSeqId());
    if (!seq_id) return ret;
    CConstRef<CTextseq_id> text_id(id.GetSeqId()->GetTextseq_Id());
    if (!text_id) return ret;
    if (text_id->CanGetAccession() && text_id->CanGetVersion() &&
        !text_id->GetAccession().empty() && text_id->GetVersion() > 0) {
        ret = text_id->GetAccession() + '.' + NStr::NumericToString(text_id->GetVersion());
    }
    return ret;
}


static const char kFileEnd[] = "|||";
static const char kFilterPrefixChar = '#';


static size_t s_ExtractFilterIndex(string& s)
{
    size_t size = s.size();
    size_t pos = size;
    while (pos && isdigit(s[pos - 1])) {
        --pos;
    }
    size_t num_len = size - pos;
    if (!num_len || num_len > kFilterIndexMaxLength ||
        !pos || s[pos] == '0' || s[pos - 1] != kFilterPrefixChar) {
        return 0;
    }
    size_t index = NStr::StringToNumeric<size_t>(s.substr(pos));
    if (!CSNPBlobId::IsValidFilterIndex(index)) {
        return 0;
    }
    // internally filter index is zero-based, but in accession it's one-based
    --index;
    // remove filter index from accession
    s.resize(pos - 1);
    return index;
}


static string s_AddFilterIndex(const string& s, size_t filter_index)
{
    CNcbiOstrstream str;
    str << s << kFilterPrefixChar << (filter_index + 1);
    return CNcbiOstrstreamToString(str);
}


template<class Container>
typename Container::value_type::TObjectType& s_AddObject(Container& container)
{
    typename Container::value_type obj(new typename Container::value_type::TObjectType);
    container.push_back(obj);
    return *obj;
}


struct SParsedId2Info
{
    unique_ptr<CSNPBlobId> blob_id;
    int split_version = 0;

    SParsedId2Info(const string& str) {
        try {
            string id2info = str;
            size_t pos = id2info.find_last_of(".");
            if (pos == NPOS || pos + 1 >= id2info.size()) return;
            split_version = NStr::StringToNumeric<int>(id2info.substr(pos + 1));
            id2info.resize(pos);
            pos = id2info.find_last_of(".");
            if (pos == NPOS) return;
            NStr::StringToNumeric<int>(id2info.substr(pos + 1)); // tse-version is always 0
            id2info.resize(pos);
            blob_id.reset(new CSNPBlobId(id2info));
        }
        catch (...) {
            split_version = 0;
        }
    }
};


END_LOCAL_NAMESPACE;


/////////////////////////////////////////////////////////////////////////////
// CSNPBlobId
/////////////////////////////////////////////////////////////////////////////


CSNPBlobId::CSNPBlobId(const CTempString& str)
{
    FromString(str);
}


CSNPBlobId::CSNPBlobId(const CSNPFileInfo& file, const CSeq_id_Handle& seq_id, size_t filter_index)
    : m_NAIndex(0),
    m_NAVersion(0),
    m_IsPrimaryTrack(false),
    m_IsPrimaryTrackGraph(false),
    m_SeqIndex(0),
    m_FilterIndex(Uint4(filter_index)),
    m_Accession(file.GetAccession()),
    m_SeqId(seq_id)
{
    // non-SatId
}


CSNPBlobId::CSNPBlobId(const CSNPFileInfo& file, size_t seq_index, size_t filter_index)
    : m_NAIndex(0),
    m_NAVersion(0),
    m_IsPrimaryTrack(false),
    m_IsPrimaryTrackGraph(false),
    m_SeqIndex(Uint4(seq_index)),
    m_FilterIndex(Uint4(filter_index))
{
    if (file.IsValidNA()) {
        SetSatNA(file.GetAccession());
    }
    else {
        // non-SatId
        m_Accession = file.GetAccession();
    }
    SetSeqAndFilterIndex(seq_index, filter_index);
}


CSNPBlobId::CSNPBlobId(const CSNPDbSeqIterator& seq, size_t filter_index)
{
    SetSatNA(seq.GetDb().GetDbPath());
    SetSeqAndFilterIndex(seq.GetVDBSeqIndex(), filter_index);
}


CSNPBlobId::~CSNPBlobId(void)
{
}


bool CSNPBlobId::IsValidNAIndex(size_t na_index)
{
    return na_index > 0 && na_index < 1000000000;
}


bool CSNPBlobId::IsValidNAVersion(size_t na_version)
{
    return na_version >= kNAVersionMin && na_version <= kNAVersionMax;
}


bool CSNPBlobId::IsValidSeqIndex(size_t seq_index)
{
    return seq_index < kSeqIndexCount;
}


bool CSNPBlobId::IsValidFilterIndex(size_t filter_index)
{
    return filter_index < kFilterIndexCount;
}


void CSNPBlobId::SetNAIndex(size_t na_index)
{
    _ASSERT(IsValidNAIndex(na_index));
    m_NAIndex = Uint4(na_index);
}


bool CSNPBlobId::IsValidSubSat(void) const
{
    return IsValidNAIndex(GetNAIndex());
}


int CSNPBlobId::GetSatBase(void) const
{
    return IsPrimaryTrack() ? kSNPSatPrimary : kSNPSatBase;
}


int CSNPBlobId::GetSubSatBase(void) const
{
    return IsPrimaryTrackGraph() ? kSNPSubSatGraph : 0;
}


void CSNPBlobId::SetNAVersion(size_t na_version)
{
    _ASSERT(IsValidNAVersion(na_version));
    m_NAVersion = Uint2(na_version);
}


bool CSNPBlobId::IsSatId(void) const
{
    return m_NAIndex != 0;
}


Int4 CSNPBlobId::GetSat(void) const
{
    _ASSERT(IsValidNAVersion(GetNAVersion()));
    return Int4(GetSatBase() + GetNAVersion());
}


Int4 CSNPBlobId::GetSubSat(void) const
{
    _ASSERT(IsValidNAIndex(GetNAIndex()));
    return Int4(GetSubSatBase() + GetNAIndex());
}


Int4 CSNPBlobId::GetSatKey(void) const
{
    _ASSERT(IsValidSeqIndex(GetSeqIndex()));
    _ASSERT(IsValidFilterIndex(GetFilterIndex()));
    return Int4(GetSeqIndex() + GetFilterIndex() * kSeqIndexCount);
}


bool CSNPBlobId::IsValidSat(void) const
{
    return IsValidNAVersion(GetNAVersion());
}


pair<size_t, size_t> CSNPBlobId::ParseNA(CTempString acc)
{
    pair<size_t, size_t> ret(0, 0);
    // NA123456789.1
    if (acc.size() < 13 || acc.size() > 15 ||
        acc[0] != 'N' || acc[1] != 'A' || acc[11] != '.') {
        return ret;
    }
    size_t na_index = NStr::StringToNumeric<size_t>(acc.substr(2, 9),
        NStr::fConvErr_NoThrow);
    if (!IsValidNAIndex(na_index)) {
        return ret;
    }
    size_t na_version = NStr::StringToNumeric<size_t>(acc.substr(12),
        NStr::fConvErr_NoThrow);
    if (!IsValidNAVersion(na_version)) {
        return ret;
    }
    ret.first = na_index;
    ret.second = na_version;
    return ret;
}


string CSNPBlobId::GetSatNA(void) const
{
    CNcbiOstrstream str;
    str << "NA" << setw(9) << setfill('0') << GetNAIndex()
        << '.' << GetNAVersion();
    return CNcbiOstrstreamToString(str);
}


void CSNPBlobId::SetSatNA(CTempString acc)
{
    pair<size_t, size_t> na = ParseNA(acc);
    SetNAIndex(na.first);
    SetNAVersion(na.second);
}


void CSNPBlobId::SetSeqAndFilterIndex(size_t seq_index,
    size_t filter_index)
{
    _ASSERT(IsValidSeqIndex(seq_index));
    _ASSERT(IsValidFilterIndex(filter_index));
    m_SeqIndex = Uint4(seq_index);
    m_FilterIndex = Uint4(filter_index);
}


bool CSNPBlobId::IsValidSatKey(void) const
{
    return IsValidSeqIndex(GetSeqIndex()) &&
        IsValidFilterIndex(GetFilterIndex());
}


CSeq_id_Handle CSNPBlobId::GetSeqId(void) const
{
    _ASSERT(!IsSatId());
    return m_SeqId;
}


string CSNPBlobId::GetAccession(void) const
{
    if (m_Accession.empty()) {
        return GetSatNA();
    }
    else {
        return m_Accession;
    }
}


void CSNPBlobId::SetPrimaryTrackFeat(void)
{
    _ASSERT(!IsPrimaryTrack());
    m_IsPrimaryTrack = true;
    m_IsPrimaryTrackGraph = false;
}


void CSNPBlobId::SetPrimaryTrackGraph(void)
{
    _ASSERT(!IsPrimaryTrack());
    m_IsPrimaryTrack = true;
    m_IsPrimaryTrackGraph = true;
}


string CSNPBlobId::ToString(void) const
{
    CNcbiOstrstream out;
    if (IsSatId()) {
        out << GetSat() << '/' << GetSubSat() << '.' << GetSatKey();
    }
    else {
        out << m_Accession;
        out << kFilterPrefixChar << (GetFilterIndex() + 1);
        out << kFileEnd << m_SeqId;
    }
    return CNcbiOstrstreamToString(out);
}


bool CSNPBlobId::FromSatString(CTempString str)
{
    if (str.empty() || !isdigit(Uchar(str[0]))) {
        return false;
    }

    size_t dot1 = str.find('/');
    if (dot1 == NPOS) {
        return false;
    }
    size_t dot2 = str.find('.', dot1 + 1);
    if (dot2 == NPOS) {
        return false;
    }
    size_t sat = NStr::StringToNumeric<size_t>(str.substr(0, dot1),
        NStr::fConvErr_NoThrow);
    bool is_primary_track = sat >= kSNPSatPrimary;
    size_t na_version = sat - (is_primary_track ? kSNPSatPrimary : kSNPSatBase);
    if (!IsValidNAVersion(na_version)) {
        return false;
    }
    size_t subsat = NStr::StringToNumeric<size_t>(str.substr(dot1 + 1, dot2 - dot1 - 1),
        NStr::fConvErr_NoThrow);
    bool is_primary_track_graph = is_primary_track && subsat >= kSNPSubSatGraph;
    size_t na_index = subsat - (is_primary_track_graph ? kSNPSubSatGraph : 0);
    if (!IsValidNAIndex(na_index)) {
        return false;
    }

    size_t satkey = NStr::StringToNumeric<size_t>(str.substr(dot2 + 1),
        NStr::fConvErr_NoThrow);
    size_t seq_index = satkey % kSeqIndexCount;
    size_t filter_index = satkey / kSeqIndexCount;
    if (!IsValidSeqIndex(seq_index) || !IsValidFilterIndex(filter_index)) {
        return false;
    }

    m_NAIndex = Uint4(na_index);
    m_NAVersion = Uint2(na_version);
    m_SeqIndex = Uint4(seq_index);
    m_FilterIndex = Uint4(filter_index);
    m_IsPrimaryTrack = is_primary_track;
    m_IsPrimaryTrackGraph = is_primary_track_graph;
    m_Accession.clear();
    m_SeqId.Reset();

    _ASSERT(IsSatId());
    return true;
}


void CSNPBlobId::FromString(CTempString str)
{
    if (FromSatString(str)) {
        return;
    }
    m_NAIndex = 0;
    m_NAVersion = 0;
    m_SeqIndex = 0;
    m_FilterIndex = 0;
    m_IsPrimaryTrack = false;
    m_IsPrimaryTrackGraph = false;
    m_Accession.clear();
    m_SeqId.Reset();
    _ASSERT(!IsSatId());

    SIZE_TYPE div = str.rfind(kFileEnd);
    if (div == NPOS) {
        NCBI_THROW_FMT(CSraException, eOtherError,
            "Bad CSNPBlobId: " << str);
    }
    m_Accession = str.substr(0, div);
    m_SeqId = CSeq_id_Handle::GetHandle(str.substr(div + strlen(kFileEnd)));
    SetSeqAndFilterIndex(0, s_ExtractFilterIndex(m_Accession));
}


/////////////////////////////////////////////////////////////////////////////
// CSNPSeqInfo
/////////////////////////////////////////////////////////////////////////////


CSNPSeqInfo::CSNPSeqInfo(CSNPFileInfo* file, const CSNPDbSeqIterator& it)
    : m_File(file),
      m_SeqIndex(it.GetVDBSeqIndex()),
      m_FilterIndex(0),
      m_IsPrimaryTrack(false),
      m_IsPrimaryTrackGraph(false)
{
    if (!file->IsValidNA()) {
        m_SeqId = it.GetSeqIdHandle();
    }
}


CSNPBlobId CSNPSeqInfo::GetBlobId(void) const
{
    _ASSERT(m_File);
    if (!m_SeqId) {
        return CSNPBlobId(*m_File, m_SeqIndex, m_FilterIndex);
    }
    return CSNPBlobId(*m_File, m_SeqId, m_FilterIndex);
}


void CSNPSeqInfo::SetFilterIndex(size_t filter_index)
{
    if (!CSNPBlobId::IsValidFilterIndex(filter_index)) {
        filter_index = 0;
    }
    m_FilterIndex = filter_index;
}


void CSNPSeqInfo::SetFromBlobId(const CSNPBlobId& blob_id)
{
    SetFilterIndex(blob_id.GetFilterIndex());
    m_IsPrimaryTrack = blob_id.IsPrimaryTrack();
    m_IsPrimaryTrackGraph = blob_id.IsPrimaryTrackGraph();
}


CSNPDbSeqIterator CSNPSeqInfo::GetSeqIterator(void) const
{
    CSNPDbSeqIterator it;
    if (!m_SeqId) {
        it = CSNPDbSeqIterator(*m_File, m_SeqIndex);
    }
    else {
        it = CSNPDbSeqIterator(*m_File, m_SeqId);
    }
    if (m_FilterIndex) {
        it.SetTrack(CSNPDbTrackIterator(*m_File, m_FilterIndex));
    }
    return it;
}


string CSNPSeqInfo::GetAnnotName(void) const
{
    // primary SNP track features have hard-coded name from EADB
    if (m_IsPrimaryTrack) {
        return "SNP";
    }
    else {
        return m_File->GetSNPAnnotName(m_FilterIndex);
    }
}


void CSNPSeqInfo::LoadBlob(SSNPData& data, bool split_enabled)
{
    CRef<CSeq_entry> tse;
    CRef<CID2S_Split_Info> split_info;

    CSNPDbSeqIterator it = GetSeqIterator();
    string base_name = GetAnnotName();
    CSNPDbSeqIterator::TFlags flags = CSNPDbSeqIterator::fDefaultFlags;
    if ( m_IsPrimaryTrack ) {
        // primary track has graphs in a separate TSE
        if ( m_IsPrimaryTrackGraph ) {
            flags |= CSNPDbSeqIterator::fNoSNPFeat;
        }
        else {
            flags |= CSNPDbSeqIterator::fOnlySNPFeat;
        }
    }
    if (split_enabled) {
        auto split = it.GetSplitInfoAndVersion(base_name, flags);
        data.m_SplitInfo = split.first;
        data.m_SplitVersion = split.second;
    }
    else {
        data.m_TSE = it.GetEntry(base_name, flags);
    }
}

void CSNPSeqInfo::LoadChunk(SSNPData& data, int chunk_id)
{
    string base_name = GetAnnotName();
    CSNPDbSeqIterator it = GetSeqIterator();
    data.m_Chunk = it.GetChunkForVersion(base_name, chunk_id, data.m_SplitVersion);
}


/////////////////////////////////////////////////////////////////////////////
// CSNPFileInfo
/////////////////////////////////////////////////////////////////////////////


CSNPFileInfo::CSNPFileInfo(CSNPClient& client, const string& acc)
    : m_IsValidNA(false),
      m_RemainingOpenRetries(client.m_Config.m_FileOpenRetry)
{
    x_Initialize(client, acc);
}


void CSNPFileInfo::x_Initialize(CSNPClient& client, const string& csra)
{
    m_FileName = csra;
    s_ExtractFilterIndex(m_FileName);
    m_IsValidNA = CSNPBlobId::IsValidNA(m_FileName);
    m_Accession = m_FileName;
    if (!m_IsValidNA) {
        // remove directory part, if any
        SIZE_TYPE sep = m_Accession.find_last_of("/\\");
        if (sep != NPOS) {
            m_Accession.erase(0, sep + 1);
        }
    }
    m_AnnotName = client.m_Config.m_AnnotName;
    if (m_AnnotName.empty()) {
        m_AnnotName = m_Accession;
    }
}


string CSNPFileInfo::GetSNPAnnotName(size_t filter_index) const
{
    return s_AddFilterIndex(GetBaseAnnotName(), filter_index);
}


CRef<CSNPSeqInfo> CSNPFileInfo::GetSeqInfo(const CSeq_id_Handle& seq_id)
{
    CRef<CSNPSeqInfo> ret;
    CSNPDbSeqIterator seq_it(m_SNPDb, seq_id);
    if (seq_it) {
        ret = new CSNPSeqInfo(this, seq_it);
    }
    return ret;
}


CRef<CSNPSeqInfo> CSNPFileInfo::GetSeqInfo(size_t seq_index)
{
    CSNPDbSeqIterator seq_it(m_SNPDb, seq_index);
    _ASSERT(seq_it);
    CRef<CSNPSeqInfo> ret(new CSNPSeqInfo(this, seq_it));
    return ret;
}


CRef<CSNPSeqInfo> CSNPFileInfo::GetSeqInfo(const CSNPBlobId& blob_id)
{
    CRef<CSNPSeqInfo> ret;
    if (blob_id.IsSatId()) {
        ret = GetSeqInfo(blob_id.GetSeqIndex());
    }
    else {
        ret = GetSeqInfo(blob_id.GetSeqId());
    }
    if (ret) {
        ret->SetFromBlobId(blob_id);
    }
    return ret;
}


/////////////////////////////////////////////////////////////////////////////
// CSNPClient
/////////////////////////////////////////////////////////////////////////////


bool CSNPClient::HaveValidSeq_id(const SPSGS_AnnotRequest& request) const
{
    // If resolving is enabled, do not check ids now.
    if (request.m_SeqIdResolve &&
        (!request.m_SeqId.empty() || !request.m_SeqIds.empty())) return true;
    if (!request.m_SeqId.empty()) {
        try {
            if (IsValidSeqId(CSeq_id_Handle::GetHandle(request.m_SeqId))) return true;
        }
        catch (exception& e) {}
    }
    for (auto& id : request.m_SeqIds) {
        try {
            if (IsValidSeqId(CSeq_id_Handle::GetHandle(id))) return true;
        }
        catch (exception& e) {}
    }
    return false;
}


const unsigned int kRefSeqAccFlags = CSeq_id::eSeqId_other | CSeq_id::fAcc_nuc;

bool CSNPClient::IsValidSeqId(const CSeq_id_Handle& idh) const
{
    if (!idh) return false;
    try {
        // check type
        if ( (idh.IdentifyAccession() & kRefSeqAccFlags) == kRefSeqAccFlags ) {
            // check version
            if ( idh.IsAccVer() ) {
                // fully qualified refseq seq-id (Seq-id.other)
                return true;
            }
        }
    }
    catch (...) {
    }
    if (m_Config.m_AllowNonRefSeq) return true;
    return false;
}


bool CSNPClient::IsValidSeqId(const string& id, int id_type, int version) const
{
    if ( id.empty() ) return false;
    // preliminary check type
    if ( id_type == CSeq_id::e_Other) {
        try {
            CSeq_id seq_id(id);
            // check type
            if ( (seq_id.IdentifyAccession() & kRefSeqAccFlags) == kRefSeqAccFlags ) {
                // check version
                if ( auto text_id = seq_id.GetTextseq_Id() ) {
                    if ( text_id->IsSetVersion() || version > 0 ) {
                        // fully qualified refseq seq-id (Seq-id.other)
                        return true;
                    }
                }
            }
        }
        catch (...) {
        }
    }
    if (m_Config.m_AllowNonRefSeq) return true;
    return false;
}


CSNPClient::CSNPClient(const SSNPProcessor_Config& config)
    : m_Config(config),
      m_Mgr(new CVDBMgr),
      m_SNPDbCache(config.m_GCSize, config.m_FileReopenTime, config.m_FileRecheckTime)
{
    if (m_Config.m_AddPTIS) {
        m_PTISClient = CSnpPtisClient::CreateClient();
    }
}


CSNPClient::~CSNPClient(void)
{
}


CRef<CSNPFileInfo> CSNPClient::GetFileInfo(const string& acc)
{
    CRef<CSNPFileInfo> info;
    {{
        CRef<CSNPFileInfo> delete_info; // delete stale file info after releasing mutex
        auto slot = m_SNPDbCache.GetSlot(acc);
        TSNPDbCache::CSlot::TSlotMutex::TWriteLockGuard guard(slot->GetSlotMutex());
        info = slot->GetObject<CSNPFileInfo>();
        if ( info && slot->IsExpired(m_SNPDbCache, acc) ) {
            PSG_INFO("PSGS_SNP: GetFileInfo: opened " << acc << " has expired");
            slot->ResetObject();
            delete_info.Swap(info);
        }
        if ( !info ) {
            info = new CSNPFileInfo(*this, acc);
            slot->UpdateExpiration(m_SNPDbCache, acc);
            slot->SetObject(info);
        }
        if ( !info->m_SNPDb && info->m_RemainingOpenRetries > 0 ) {
            try {
                --info->m_RemainingOpenRetries;
                psg_time_point_t start = psg_clock_t::now();
                info->m_SNPDb = CSNPDb(*m_Mgr, info->m_FileName);
                x_RegisterTiming(start, eVDBOpen, eOpStatusFound);
            }
            catch ( CSraException& exc ) {
                if ( exc.GetErrCode() == exc.eNotFoundDb ||
                     exc.GetErrCode() == exc.eProtectedDb ) {
                    // no such SNP table
                    info->m_RemainingOpenRetries = 0; // no more opening retries
                }
                else {
                    // problem in VDB or WGS reader
                    PSG_ERROR("PSGS_SNP: Exception while opening SNP DB " << acc << ": " << exc);
                    if ( info->m_RemainingOpenRetries > 0 ) {
                        throw;
                    }
                    else {
                        // assume the file is not SNP file
                        PSG_ERROR("PSGS_SNP: assume DB " << acc << " is not SNP");
                    }
                }
            }
            catch ( CException& exc ) {
                // problem in VDB or WGS reader
                PSG_ERROR("PSGS_SNP: Exception while opening SNP DB " << acc << ": " << exc);
                if ( info->m_RemainingOpenRetries > 0 ) {
                    throw;
                }
                else {
                    // assume the file is not SNP file
                    PSG_ERROR("PSGS_SNP: assume DB " << acc << " is not SNP");
                }
            }
            catch ( exception& exc ) {
                // problem in VDB or WGS reader
                PSG_ERROR("PSGS_SNP: Exception while opening SNP DB " << acc << ": " << exc.what());
                if ( info->m_RemainingOpenRetries > 0 ) {
                    throw;
                }
                else {
                    // assume the file is not SNP file
                    PSG_ERROR("PSGS_SNP: assume DB " << acc << " is not SNP");
                }
            }
        }
    }}
    if ( !info->m_SNPDb ) {
        return null;
    }
    return info;
}


CRef<CSNPSeqInfo> CSNPClient::GetSeqInfo(const CSNPBlobId& blob_id)
{
    return GetFileInfo(blob_id.GetAccession())->GetSeqInfo(blob_id);
}


vector<string> CSNPClient::WhatNACanProcess(SPSGS_AnnotRequest& annot_request,
                                            TProcessorPriority priority) const
{
    vector<string> can_process;
    if (HaveValidSeq_id(annot_request)) {
        for (const auto& name : (priority==-1?
                                 annot_request.m_Names:
                                 annot_request.GetNotProcessedName(priority)) ) {
            if (m_Config.m_AddPTIS && name == "SNP") {
                can_process.push_back(name);
                continue;
            }
            string acc = name;
            size_t filter_index = s_ExtractFilterIndex(acc);
            if (filter_index == 0 && acc.size() == name.size()) {
                // filter specification is required
                continue;
            }
            if (CSNPBlobId::IsValidNA(acc)) {
                can_process.push_back(name);
            }
        }
    }
    return can_process;
}


bool CSNPClient::CanProcessRequest(CPSGS_Request& request, TProcessorPriority priority) const
{
    switch (request.GetRequestType()) {
    case CPSGS_Request::ePSGS_AnnotationRequest: {
        return !WhatNACanProcess(request.GetRequest<SPSGS_AnnotRequest>(), priority).empty();
    }
    case CPSGS_Request::ePSGS_BlobBySatSatKeyRequest: {
        SPSGS_BlobBySatSatKeyRequest& blob_request = request.GetRequest<SPSGS_BlobBySatSatKeyRequest>();
        try {
            CSNPBlobId blob_id(blob_request.m_BlobId.GetId());
        }
        catch (CSraException&) {
            // Not a valid SNP blob id
            return false;
        }
        return true;
    }
    case CPSGS_Request::ePSGS_TSEChunkRequest: {
        SPSGS_TSEChunkRequest& chunk_request = request.GetRequest<SPSGS_TSEChunkRequest>();
        if (chunk_request.m_Id2Chunk < 0) return false;
        SParsedId2Info parsed_info(chunk_request.m_Id2Info);
        return parsed_info.blob_id.get();
    }
    default:
        return false;
    }
    return false;
}


vector<SSNPData> CSNPClient::GetAnnotInfo(const CSeq_id_Handle& id,
    const string& name, CSeq_id::ESNPScaleLimit scale_limit)
{
    vector<SSNPData> ret;
    try {
        if (scale_limit == CSeq_id::eSNPScaleLimit_Default) {
            scale_limit = m_Config.m_SNPScaleLimit;
        }
        if (!id.IsAllowedSNPScaleLimit(scale_limit)) return ret;
        if (m_Config.m_AddPTIS && name == "SNP") {
            // default SNP track
            string acc_ver = s_GetAccVer(id);
            if (acc_ver.empty()) {
                return ret;
            }
            // find default SNP track
            psg_time_point_t start = psg_clock_t::now();
            string na_acc = m_PTISClient->GetPrimarySnpTrackForAccVer(acc_ver);
            x_RegisterTiming(start, eSNP_PTISLookup,
                             na_acc.empty()? eOpStatusNotFound: eOpStatusFound);
            if (na_acc.empty()) {
                // no default SNP track
                return ret;
            }
            size_t filter_index = s_ExtractFilterIndex(na_acc);
            CRef<CSNPFileInfo> info = GetFileInfo(na_acc);
            if ( !info ) {
                return ret; // should it be an error since PTIS says SNPs should exist
            }
            CRef<CSNPSeqInfo> seq = info->GetSeqInfo(id);
            if ( !seq ) {
                return ret; // should it be an error since PTIS says SNPs should exist
            }
            seq->SetFilterIndex(filter_index);
            {
                CSNPBlobId blob_id = seq->GetBlobId();
                blob_id.SetPrimaryTrackFeat();
                SSNPData data;
                data.m_BlobId = blob_id.ToString();
                data.m_Name = name;
                data.m_AnnotInfo.push_back(x_GetFeatInfo(name, id));
                ret.push_back(data);
            }
            {
                CSNPBlobId blob_id = seq->GetBlobId();
                blob_id.SetPrimaryTrackGraph();
                SSNPData data;
                data.m_BlobId = blob_id.ToString();
                data.m_Name = name;
                // add SNP overview graph type info
                data.m_AnnotInfo.push_back(x_GetGraphInfo(name, id));
                // add SNP graph type info
                string graph_name = CSeq_annot::CombineWithZoomLevel(name, info->GetDb().GetCoverageZoom());
                data.m_AnnotInfo.push_back(x_GetGraphInfo(graph_name, id));
                ret.push_back(data);
            }
        }
        else {
            string acc = name;
            size_t filter_index = s_ExtractFilterIndex(acc);
            if (filter_index == 0 && acc.size() == name.size()) {
                // filter specification is required
                return ret;
            }
            CRef<CSNPFileInfo> info = GetFileInfo(acc);
            if ( !info ) {
                return ret;
            }
            CRef<CSNPSeqInfo> seq = info->GetSeqInfo(id);
            if ( !seq ) {
                return ret;
            }
            seq->SetFilterIndex(filter_index);
            auto blob_id = seq->GetBlobId();
            SSNPData data;
            data.m_BlobId = blob_id.ToString();
            data.m_Name = name;
            data.m_AnnotInfo.push_back(x_GetFeatInfo(name, id));
            string overview_name =
                CSeq_annot::CombineWithZoomLevel(name, info->GetDb().GetOverviewZoom());
            data.m_AnnotInfo.push_back(x_GetGraphInfo(overview_name, id));
            string coverage_name =
                CSeq_annot::CombineWithZoomLevel(name, info->GetDb().GetCoverageZoom());
            data.m_AnnotInfo.push_back(x_GetGraphInfo(coverage_name, id));
            ret.push_back(data);
        }
    }
    catch ( exception& exc ) {
        SSNPData data;
        data.m_Name = name;
        data.m_Error = "Exception when handling get_na request: " + string(exc.what());
        ret.push_back(data);
    }
    return ret;
}


SSNPData CSNPClient::GetBlobByBlobId(const string& blob_id)
{
    SSNPData ret;
    CSNPBlobId snp_blob_id(blob_id);
    GetSeqInfo(snp_blob_id)->LoadBlob(ret, m_Config.m_Split);
    return ret;
}


SSNPData CSNPClient::GetChunk(const string& id2info, int chunk_id)
{
    SSNPData ret;
    SParsedId2Info parsed_info(id2info);
    if (!parsed_info.blob_id) return ret;
    ret.m_SplitVersion = parsed_info.split_version;
    GetSeqInfo(*parsed_info.blob_id)->LoadChunk(ret, chunk_id);
    return ret;
}


CRef<CID2S_Seq_annot_Info> CSNPClient::x_GetFeatInfo(const string& name, const objects::CSeq_id_Handle& id)
{
    CRef<CID2S_Seq_annot_Info> annot_info(new CID2S_Seq_annot_Info);
    annot_info->SetName(name);
    auto& feat_type = s_AddObject(annot_info->SetFeat());
    feat_type.SetType(CSeqFeatData::e_Imp);
    feat_type.SetSubtypes().push_back(CSeqFeatData::eSubtype_variation);
    annot_info->SetSeq_loc().SetWhole_seq_id().Assign(*id.GetSeqId());
    return annot_info;
}


CRef<CID2S_Seq_annot_Info> CSNPClient::x_GetGraphInfo(const string& name, const objects::CSeq_id_Handle& id)
{
    CRef<CID2S_Seq_annot_Info> annot_info(new CID2S_Seq_annot_Info);
    annot_info->SetName(name);
    annot_info->SetGraph();
    annot_info->SetSeq_loc().SetWhole_seq_id().Assign(*id.GetSeqId());
    return annot_info;
}


void CSNPClient::x_RegisterTiming(psg_time_point_t start,
                                  EPSGOperation operation,
                                  EPSGOperationStatus status)
{
    CPubseqGatewayApp::GetInstance()->
        GetTiming().Register(nullptr, operation, status, start, 0);
}


END_NAMESPACE(wgs);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;
