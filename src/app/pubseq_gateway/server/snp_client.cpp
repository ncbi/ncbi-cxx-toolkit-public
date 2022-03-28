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
// sat = 2001-2099 : SNP NA version 1 - 99
// or, for primary SNP track:
// sat = 3001-3099 : SNP NA version 1 - 99
// subsat : NA accession number
// or, for primary SNP graph track:
// NA accession number + kSNPSubSatGraph(=1000000000)
// satkey : SequenceIndex + 1000000*FilterIndex;
// satkey bits 24-30: 

// magic chunk ids
static const int kTSEId = 1;
static const int kChunkIdFeat = 0;
static const int kChunkIdGraph = 1;
static const int kChunkIdMul = 2;

static const TSeqPos kDefaultFeatChunkPages = 200;
static const TSeqPos kFeatChunksPerGraphChunk = 10;
static const TSeqPos kTargetFeatsPerChunk = 20000;

const int kSNPSatBase = 2000;
const int kSNPSatPrimary = 3000;
const int kSNPSubSatGraph = 1000000000;
const int kNAVersionMin = 1;
const int kNAVersionMax = 99;
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


template<class Values>
bool s_HasNonZero(const Values& values, TSeqPos index, TSeqPos count)
{
    TSeqPos end = min(index + count, TSeqPos(values.size()));
    for (TSeqPos i = index; i < end; ++i) {
        if (values[i]) {
            return true;
        }
    }
    return false;
}


template<class TValues>
void s_AddBits2(vector<char>& bits,
    TSeqPos bit_values,
    TSeqPos pos_index,
    const TValues& values)
{
    TSeqPos dst_ind = pos_index / bit_values;
    TSeqPos src_ind = 0;
    if (TSeqPos first_offset = pos_index % bit_values) {
        TSeqPos first_count = bit_values - first_offset;
        if (!bits[dst_ind]) {
            bits[dst_ind] = s_HasNonZero(values, 0, first_count);
        }
        dst_ind += 1;
        src_ind += first_count;
    }
    while (src_ind < values.size()) {
        if (!bits[dst_ind]) {
            bits[dst_ind] = s_HasNonZero(values, src_ind, bit_values);
        }
        ++dst_ind;
        src_ind += bit_values;
    }
}


static void s_AddBits(vector<char>& bits, TSeqPos kChunkSize, const CSeq_graph& graph)
{
    TSeqPos comp = graph.GetComp();
    _ASSERT(kChunkSize % comp == 0);
    TSeqPos bit_values = kChunkSize / comp;
    const CSeq_interval& loc = graph.GetLoc().GetInt();
    TSeqPos pos = loc.GetFrom();
    _ASSERT(pos % comp == 0);
    _ASSERT(graph.GetNumval() * comp == loc.GetLength());
    TSeqPos pos_index = pos / comp;
    if (graph.GetGraph().IsByte()) {
        auto& values = graph.GetGraph().GetByte().GetValues();
        _ASSERT(values.size() == graph.GetNumval());
        s_AddBits2(bits, bit_values, pos_index, values);
    }
    else {
        auto& values = graph.GetGraph().GetInt().GetValues();
        _ASSERT(values.size() == graph.GetNumval());
        s_AddBits2(bits, bit_values, pos_index, values);
    }
}


template<class Container>
typename Container::value_type::TObjectType& s_AddObject(Container& container)
{
    typename Container::value_type obj(new typename Container::value_type::TObjectType);
    container.push_back(obj);
    return *obj;
}


void s_SetZoomLevel(CSeq_annot& annot, int zoom_level)
{
    CUser_object& obj = s_AddObject(annot.SetDesc().Set()).SetUser();
    obj.SetType().SetStr("AnnotationTrack");
    obj.AddField("ZoomLevel", zoom_level);
}


TSeqPos s_CalcFeatChunkPages(const CSNPDbSeqIterator& it)
{
    // get statistics
    Uint8 total_feat_count = it.GetSNPCount();
    const TSeqPos page_size = it.GetPageSize();
    CRange<TSeqPos> total_range = it.GetSNPRange();
    
    // all calculations are approximate, 1 is added to avoid zero division
    TSeqPos page_count = total_range.GetLength()/page_size+1;
    Uint8 feat_per_page = total_feat_count/page_count+1;
    TSeqPos chunk_pages = kTargetFeatsPerChunk/feat_per_page+1;

    // final formula with only one division is
    // chunk_pages = (kTargetFeatsPerChunk*total_range.GetLength())/(total_feat_count*page_size)
    return min(kDefaultFeatChunkPages, chunk_pages);
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
    CRange<TSeqPos> total_range = it.GetSNPRange();

    string base_name = GetAnnotName();
    if (split_enabled) {
        split_info.Reset(new CID2S_Split_Info);
        split_info->SetChunks();
        CBioseq_set& skeleton = split_info->SetSkeleton().SetSet();
        skeleton.SetId().SetId(kTSEId);
        skeleton.SetSeq_set();

        // split
        string overview_name = base_name;
        TSeqPos feat_chunk_pages = s_CalcFeatChunkPages(it);
        _ASSERT(feat_chunk_pages <= kDefaultFeatChunkPages);
        data.m_SplitVersion = feat_chunk_pages == kDefaultFeatChunkPages? 0: feat_chunk_pages;
        TSeqPos feat_chunk_size = feat_chunk_pages * it.GetPageSize();
        TSeqPos graph_chunk_size = feat_chunk_size * kFeatChunksPerGraphChunk;

        vector<char> feat_chunks(total_range.GetTo() / feat_chunk_size + 1);
        CRef<CSeq_annot> overview_annot = it.GetOverviewAnnot(total_range, overview_name);
        if (overview_annot) {
            for (auto& g : overview_annot->GetData().GetGraph()) {
                s_AddBits(feat_chunks, feat_chunk_size, *g);
            }
            if (!m_IsPrimaryTrack || m_IsPrimaryTrackGraph) {
                overview_annot->SetNameDesc(overview_name);
                if (!m_IsPrimaryTrack) {
                    s_SetZoomLevel(*overview_annot, it.GetOverviewZoom());
                }
                skeleton.SetAnnot().push_back(overview_annot);
            }
        }
        if (!m_IsPrimaryTrack || m_IsPrimaryTrackGraph) {
            //string graph_name = base_name + kGraphNameSuffix;
            string graph_name = CSeq_annot::CombineWithZoomLevel(base_name, it.GetCoverageZoom());
            _ASSERT(graph_chunk_size % feat_chunk_size == 0);
            const TSeqPos feat_per_graph = graph_chunk_size / feat_chunk_size;
            for (int i = 0; i * graph_chunk_size < total_range.GetToOpen(); ++i) {
                if (!s_HasNonZero(feat_chunks, i * feat_per_graph, feat_per_graph)) {
                    continue;
                }
                int chunk_id = i * kChunkIdMul + kChunkIdGraph;
                auto& chunk = s_AddObject(split_info->SetChunks());
                chunk.SetId().Set(chunk_id);
                auto& annot_info = s_AddObject(chunk.SetContent()).SetSeq_annot();
                annot_info.SetName(graph_name);
                annot_info.SetGraph();
                auto& interval = annot_info.SetSeq_loc().SetSeq_id_interval();
                interval.SetSeq_id(*it.GetSeqId());
                interval.SetStart(i * graph_chunk_size);
                interval.SetLength(graph_chunk_size);
            }
        }
        if (!m_IsPrimaryTrack || !m_IsPrimaryTrackGraph) {
            string feat_name = base_name;
            TSeqPos overflow = it.GetMaxSNPLength() - 1;
            for (int i = 0; i * feat_chunk_size < total_range.GetToOpen(); ++i) {
                if (!feat_chunks[i]) {
                    continue;
                }
                int chunk_id = i * kChunkIdMul + kChunkIdFeat;
                auto& chunk = s_AddObject(split_info->SetChunks());
                chunk.SetId().Set(chunk_id);
                auto& annot_info = s_AddObject(chunk.SetContent()).SetSeq_annot();
                annot_info.SetName(feat_name);
                auto& feat_type = s_AddObject(annot_info.SetFeat());
                feat_type.SetType(CSeqFeatData::e_Imp);
                feat_type.SetSubtypes().push_back(CSeqFeatData::eSubtype_variation);
                CID2S_Seq_id_Interval& interval = annot_info.SetSeq_loc().SetSeq_id_interval();
                interval.SetSeq_id(*it.GetSeqId());
                interval.SetStart(i * feat_chunk_size);
                interval.SetLength(feat_chunk_size + overflow);
            }
        }
    }
    else {
        tse.Reset(new CSeq_entry);
        tse->SetSet().SetSeq_set();
        const auto& annots = it.GetTableFeatAnnots(total_range, base_name);
        for (auto& annot : annots) {
            tse->SetSet().SetAnnot().push_back(annot);
        }
    }
    data.m_TSE = tse;
    data.m_SplitInfo = split_info;
}

void CSNPSeqInfo::LoadChunk(SSNPData& data, int chunk_id)
{
    CRef<CID2S_Chunk> chunk(new CID2S_Chunk);
    CID2S_Chunk_Data& id2_data = s_AddObject(chunk->SetData());

    int chunk_type = chunk_id % kChunkIdMul;
    int i = chunk_id / kChunkIdMul;
    id2_data.SetId().SetBioseq_set(kTSEId);

    string base_name = GetAnnotName();
    CSNPDbSeqIterator it = GetSeqIterator();
    TSeqPos feat_chunk_pages = data.m_SplitVersion ? data.m_SplitVersion : kDefaultFeatChunkPages;
    TSeqPos feat_chunk_size = feat_chunk_pages * it.GetPageSize();
    if (chunk_type == kChunkIdFeat) {
        CRange<TSeqPos> range;
        range.SetFrom(i * feat_chunk_size);
        range.SetToOpen((i + 1) * feat_chunk_size);
        string feat_name = base_name;
        for (auto& annot : it.GetTableFeatAnnots(range, base_name)) {
            id2_data.SetAnnots().push_back(annot);
        }
    }
    else if (chunk_type == kChunkIdGraph) {
        TSeqPos graph_chunk_size = feat_chunk_size * kFeatChunksPerGraphChunk;
        CRange<TSeqPos> range;
        range.SetFrom(i * graph_chunk_size);
        range.SetToOpen((i + 1) * graph_chunk_size);
        string graph_name = base_name/* + kGraphNameSuffix*/;
        if (auto annot = it.GetCoverageAnnot(range, graph_name)) {
            s_SetZoomLevel(*annot, it.GetCoverageZoom());
            id2_data.SetAnnots().push_back(annot);
        }
    }
    data.m_Chunk = chunk;
}


/////////////////////////////////////////////////////////////////////////////
// CSNPFileInfo
/////////////////////////////////////////////////////////////////////////////


CSNPFileInfo::CSNPFileInfo(CSNPClient& client, const string& acc)
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
    m_SNPDb = CSNPDb(*client.m_Mgr, m_FileName);
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


CSeq_id_Handle CSNPClient::GetRequestSeq_id(const SPSGS_AnnotRequest& request)
{
    try {
        return CSeq_id_Handle::GetHandle(request.m_SeqId);
    }
    catch (exception& e) {
    }
    return CSeq_id_Handle();
}


CSNPClient::CSNPClient(const SSNPProcessor_Config& config)
    : m_Config(config),
      m_Mgr(new CVDBMgr),
      m_FoundFiles(config.m_GCSize),
      m_MissingFiles(config.m_MissingGCSize)
{
    if (m_Config.m_AddPTIS) {
        m_PTISClient = CSnpPtisClient::CreateClient();
    }

    for (auto& file : m_Config.m_VDBFiles) {
        AddFixedFile(file);
    }
}


CSNPClient::~CSNPClient(void)
{
}


void CSNPClient::AddFixedFile(const string& file)
{
    CRef<CSNPFileInfo> info(new CSNPFileInfo(*this, file));
    string key = info->GetBaseAnnotName();
    s_ExtractFilterIndex(key);
    m_FixedFiles[key] = info;
}


CRef<CSNPFileInfo> CSNPClient::GetFixedFile(const string& acc)
{
    TFixedFiles::iterator it = m_FixedFiles.find(acc);
    if (it == m_FixedFiles.end()) {
        return null;
    }
    return it->second;
}


CRef<CSNPFileInfo> CSNPClient::FindFile(const string& acc)
{
    if (!m_FixedFiles.empty()) {
        // no dynamic accessions
        return null;
    }
    CMutexGuard guard(m_Mutex);
    TMissingFiles::iterator it2 = m_MissingFiles.find(acc);
    if (it2 != m_MissingFiles.end()) {
        return null;
    }
    TFoundFiles::iterator it = m_FoundFiles.find(acc);
    if (it != m_FoundFiles.end()) {
        return it->second;
    }
    CRef<CSNPFileInfo> info;
    try {
        info = new CSNPFileInfo(*this, acc);
    }
    catch (CSraException& exc) {
        if (exc.GetErrCode() == exc.eNotFoundDb ||
            exc.GetErrCode() == exc.eProtectedDb) {
            // no such SRA table
            return null;
        }
        PSG_ERROR("CSNPClient::FindFile(" << acc << "): accession not found: " << exc);
        m_MissingFiles[acc] = true;
        return null;
    }
    // store file in cache
    m_FoundFiles[acc] = info;
    return info;
}


CRef<CSNPFileInfo> CSNPClient::GetFileInfo(const string& acc)
{
    if (!m_FixedFiles.empty()) {
        return GetFixedFile(acc);
    }
    else {
        return FindFile(acc);
    }
}


CRef<CSNPSeqInfo> CSNPClient::GetSeqInfo(const CSNPBlobId& blob_id)
{
    return GetFileInfo(blob_id.GetAccession())->GetSeqInfo(blob_id);
}


bool CSNPClient::CanProcessRequest(CPSGS_Request& request, TProcessorPriority priority) const
{
    switch (request.GetRequestType()) {
    case CPSGS_Request::ePSGS_AnnotationRequest: {
        SPSGS_AnnotRequest& annot_request = request.GetRequest<SPSGS_AnnotRequest>();
        CSeq_id_Handle id = GetRequestSeq_id(annot_request);
        if (!id) return false;
        vector<string> names = annot_request.GetNotProcessedName(priority);
        for (const auto& name : names) {
            if (m_Config.m_AddPTIS && name == "SNP") return true;
            string acc = name;
            size_t filter_index = s_ExtractFilterIndex(acc);
            if (filter_index == 0 && acc.size() == name.size()) {
                // filter specification is required
                continue;
            }
            if (CSNPBlobId::IsValidNA(acc)) return true;
        }
        return false;
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


vector<SSNPData> CSNPClient::GetAnnotInfo(const CSeq_id_Handle& id, const vector<string>& names)
{
    vector<SSNPData> ret;
    {
        CMutexGuard guard(m_Mutex);
        if (m_FixedFiles.empty()) {
            if (m_FoundFiles.get_size_limit() < names.size()) {
                // increase VDB cache size
                m_FoundFiles.set_size_limit(names.size() + m_Config.m_GCSize);
            }
            if (m_MissingFiles.get_size_limit() < names.size()) {
                // increase VDB cache size
                m_MissingFiles.set_size_limit(names.size() + m_Config.m_MissingGCSize);
            }
        }
    }
    for (const auto& name : names) {
        if (m_Config.m_AddPTIS && name == "SNP") {
            // default SNP track
            string acc_ver = s_GetAccVer(id);
            if (!acc_ver.empty()) {
                string na_acc;
                try {
                    // add default SNP track
                    na_acc = m_PTISClient->GetPrimarySnpTrackForAccVer(acc_ver);
                }
                catch (CException& exc) {
                    PSG_ERROR("CSNPClient: failed to add PTIS track for " << acc_ver << ": " << exc);
                }
                if (!na_acc.empty()) {
                    size_t filter_index = s_ExtractFilterIndex(na_acc);
                    if (CRef<CSNPFileInfo> info = GetFileInfo(na_acc)) {
                        if (CRef<CSNPSeqInfo> seq = info->GetSeqInfo(id)) {
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
                                data.m_AnnotInfo.push_back(x_GetGraphInfo(
                                    CSeq_annot::CombineWithZoomLevel(name, info->GetDb().GetCoverageZoom()), id));
                                ret.push_back(data);
                            }
                        }
                    }
                }
            }
            continue;
        }
        string acc = name;
        size_t filter_index = s_ExtractFilterIndex(acc);
        if (filter_index == 0 && acc.size() == name.size()) {
            // filter specification is required
            continue;
        }
        if (CRef<CSNPFileInfo> info = GetFileInfo(acc)) {
            if (CRef<CSNPSeqInfo> seq = info->GetSeqInfo(id)) {
                seq->SetFilterIndex(filter_index);
                auto blob_id = seq->GetBlobId();
                SSNPData data;
                data.m_BlobId = blob_id.ToString();
                data.m_Name = name;
                data.m_AnnotInfo.push_back(x_GetFeatInfo(name, id));
                data.m_AnnotInfo.push_back(x_GetGraphInfo(
                    CSeq_annot::CombineWithZoomLevel(name, info->GetDb().GetOverviewZoom()), id));
                data.m_AnnotInfo.push_back(x_GetGraphInfo(
                    CSeq_annot::CombineWithZoomLevel(name, info->GetDb().GetCoverageZoom()), id));
                ret.push_back(data);
            }
        }
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


END_NAMESPACE(wgs);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;
