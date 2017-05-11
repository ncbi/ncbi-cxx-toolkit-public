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
 * Authors:  Eugene Vasilchenko
 *
 * File Description:
 *   Access to BAM index files
 *
 */

#include <ncbi_pch.hpp>
#include <sra/readers/bam/bamread.hpp> // for CBamException
#include <sra/readers/bam/bamindex.hpp>
#include <sra/readers/bam/vdbfile.hpp>
#include <util/compress/zlib.hpp>
#include <util/util_exception.hpp>
#include <util/timsort.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqres/seqres__.hpp>
#include <objects/seqloc/seqloc__.hpp>

#include <strstream>

#ifndef NCBI_THROW2_FMT
# define NCBI_THROW2_FMT(exception_class, err_code, message, extra)     \
    throw NCBI_EXCEPTION2(exception_class, err_code, FORMAT(message), extra)
#endif


BEGIN_NCBI_SCOPE

//#define NCBI_USE_ERRCODE_X   BAM2Graph
//NCBI_DEFINE_ERR_SUBCODE_X(6);

BEGIN_SCOPE(objects)

class CSeq_entry;


static const float kEstimatedCompression = 0.25;

static inline
void s_Read(CNcbiIstream& in, char* dst, size_t len)
{
    while ( len ) {
        in.read(dst, len);
        if ( !in ) {
            NCBI_THROW(CIOException, eRead, "Read failure");
        }
        size_t cnt = in.gcount();
        len -= cnt;
        dst += cnt;
    }
}


static inline
void s_Read(CBGZFStream& in, char* dst, size_t len)
{
    while ( len ) {
        size_t cnt = in.Read(dst, len);
        len -= cnt;
        dst += cnt;
    }
}


static inline
void s_ReadString(CNcbiIstream& in, string& ret, size_t len)
{
    ret.resize(len);
    s_Read(in, &ret[0], len);
}


static inline
void s_ReadString(CBGZFStream& in, string& ret, size_t len)
{
    ret.resize(len);
    s_Read(in, &ret[0], len);
}


static inline
void s_ReadMagic(CNcbiIstream& in, const char* magic)
{
    _ASSERT(strlen(magic) == 4);
    char buf[4];
    s_Read(in, buf, 4);
    if ( memcmp(buf, magic, 4) != 0 ) {
        NCBI_THROW_FMT(CBGZFException, eFormatError,
                       "Bad file magic: "<<NStr::PrintableString(string(buf, buf+4)));
    }
}


static inline
void s_ReadMagic(CBGZFStream& in, const char* magic)
{
    _ASSERT(strlen(magic) == 4);
    char buf[4];
    s_Read(in, buf, 4);
    if ( memcmp(buf, magic, 4) != 0 ) {
        NCBI_THROW_FMT(CBGZFException, eFormatError,
                       "Bad file magic: "<<NStr::PrintableString(string(buf, buf+4)));
    }
}


static inline
uint32_t s_ReadUInt32(CNcbiIstream& in)
{
    char buf[4];
    s_Read(in, buf, 4);
    return SBamUtil::MakeUint4(buf);
}


static inline
int32_t s_ReadInt32(CNcbiIstream& in)
{
    return int32_t(s_ReadUInt32(in));
}


static inline
uint64_t s_ReadUInt64(CNcbiIstream& in)
{
    char buf[8];
    s_Read(in, buf, 8);
    return SBamUtil::MakeUint8(buf);
}


static inline
CBGZFPos s_ReadFilePos(CNcbiIstream& in)
{
    return CBGZFPos(s_ReadUInt64(in));
}


static inline
CBGZFRange s_ReadFileRange(CNcbiIstream& in)
{
    CBGZFPos beg = s_ReadFilePos(in);
    CBGZFPos end = s_ReadFilePos(in);
    return CBGZFRange(beg, end);
}


static inline
uint32_t s_ReadUInt32(CBGZFStream& in)
{
    char buf[4];
    s_Read(in, buf, 4);
    return SBamUtil::MakeUint4(buf);
}


static inline
int32_t s_ReadInt32(CBGZFStream& in)
{
    return int32_t(s_ReadUInt32(in));
}


/////////////////////////////////////////////////////////////////////////////
// SBamIndexBinInfo
/////////////////////////////////////////////////////////////////////////////


COpenRange<TSeqPos> SBamIndexBinInfo::GetSeqRange(uint32_t bin)
{
    uint32_t pos = 0, len = 1<<29, count = 1;
    while ( bin >= count ) {
        bin -= count;
        len >>= 3;
        count <<= 3;
    }
    pos += bin*len;
    return COpenRange<TSeqPos>(pos, pos+len);
}


void SBamIndexBinInfo::Read(CNcbiIstream& in)
{
    m_Bin = s_ReadUInt32(in);
    int32_t n_chunk = s_ReadInt32(in);
    m_Chunks.resize(n_chunk);
    for ( int32_t i_chunk = 0; i_chunk < n_chunk; ++i_chunk ) {
        m_Chunks[i_chunk] = s_ReadFileRange(in);
    }
}


/////////////////////////////////////////////////////////////////////////////
// SBamIndexRefIndex
/////////////////////////////////////////////////////////////////////////////


void SBamIndexRefIndex::Read(CNcbiIstream& in)
{
    m_EstimatedLength = 0;
    int32_t n_bin = s_ReadInt32(in);
    m_Bins.resize(n_bin);
    const SBamIndexBinInfo::TBin kSpecialBin = 37450;
    for ( int32_t i_bin = 0; i_bin < n_bin; ++i_bin ) {
        SBamIndexBinInfo& bin = m_Bins[i_bin];
        bin.Read(in);
        if ( bin.m_Bin == kSpecialBin ) {
            if ( bin.m_Chunks.size() != 2 ) {
                NCBI_THROW(CBamException, eOtherError,
                           "Bad unmapped bin format");
            }
            m_UnmappedChunk = bin.m_Chunks[0];
            m_MappedCount = bin.m_Chunks[1].first.GetVirtualPos();
            m_UnmappedCount = bin.m_Chunks[1].second.GetVirtualPos();
        }
        else {
            m_EstimatedLength = max(m_EstimatedLength, bin.GetSeqRange().GetToOpen());
        }
    }
    gfx::timsort(m_Bins.begin(), m_Bins.end());
    m_Bins.erase(lower_bound(m_Bins.begin(), m_Bins.end(), kSpecialBin), m_Bins.end());
        
    int32_t n_intv = s_ReadInt32(in);
    m_Intervals.resize(n_intv);
    for ( int32_t i_intv = 0; i_intv < n_intv; ++i_intv ) {
        m_Intervals[i_intv] = s_ReadFilePos(in);
    }
    m_EstimatedLength = max(m_EstimatedLength, n_intv*CBamIndex::kMinBinSize);
}


CBGZFRange SBamIndexRefIndex::GetLimitRange(COpenRange<TSeqPos>& ref_range) const
{
    CBGZFRange limit;
    if ( m_EstimatedLength < ref_range.GetToOpen() ) {
        ref_range.SetToOpen(m_EstimatedLength);
    }
    if ( ref_range.Empty() ) {
        return limit;
    }
    _ASSERT(ref_range.GetFrom() < CBamIndex::kMaxBinSize);
    _ASSERT(ref_range.GetToOpen() <= CBamIndex::kMaxBinSize);

    SBamIndexBinInfo::TBin beg_bin_offset = CBamIndex::GetBinNumberOffset(ref_range.GetFrom(), 0);
    // start limit is from intervals and beg position
    if ( beg_bin_offset < m_Intervals.size() ) {
        limit.first = m_Intervals[beg_bin_offset];
    }
    // end limit is from low-level block after end position
    limit.second = CBGZFPos::GetInvalid();
    SBamIndexBinInfo::TBin end_bin = CBamIndex::GetBinNumber(ref_range.GetTo(), 0)+1;
    auto end_it = lower_bound(m_Bins.begin(), m_Bins.end(), end_bin);
    if ( end_it != m_Bins.end() ) {
        limit.second = end_it->m_Chunks[0].first;
    }
    return limit;
}


void SBamIndexRefIndex::AddLevelFileRanges(vector<CBGZFRange>& ranges,
                                           CBGZFRange limit_file_range,
                                           COpenRange<TSeqPos> ref_range,
                                           uint32_t index_level) const
{
    CBamIndex::TBin bin1 = CBamIndex::GetBinNumber(ref_range.GetFrom(), index_level);
    CBamIndex::TBin bin2 = CBamIndex::GetBinNumber(ref_range.GetTo(), index_level);
    for ( auto it = lower_bound(m_Bins.begin(), m_Bins.end(), bin1); it != m_Bins.end() && it->m_Bin <= bin2; ++it ) {
        for ( auto c : it->m_Chunks ) {
            if ( c.first < limit_file_range.first ) {
                c.first = limit_file_range.first;
            }
            if ( limit_file_range.second < c.second ) {
                c.second = limit_file_range.second;
            }
            if ( c.first < c.second ) {
                ranges.push_back(c);
            }
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
// CCached
/////////////////////////////////////////////////////////////////////////////


static size_t ReadVDBFile(AutoArray<char>& data, const string& path)
{
    CBamVDBFile file(path);
    size_t fsz = file.GetSize();
    data.reset(new char[fsz]);
    file.ReadExactly(0, data.get(), fsz);
    return fsz;
}


/////////////////////////////////////////////////////////////////////////////
// CBamIndex
/////////////////////////////////////////////////////////////////////////////


CBamIndex::CBamIndex()
    : m_UnmappedCount(0)
{
}


CBamIndex::CBamIndex(const string& index_file_name)
    : m_UnmappedCount(0)
{
    Read(index_file_name);
}


CBamIndex::~CBamIndex()
{
}


void CBamIndex::Read(const string& index_file_name)
{
    m_Refs.clear();
    m_UnmappedCount = 0;

    AutoArray<char> data;
    size_t size = ReadVDBFile(data, index_file_name);
    istrstream in(data.get(), size);

    s_ReadMagic(in, "BAI\1");

    int32_t n_ref = s_ReadInt32(in);
    m_Refs.resize(n_ref);
    for ( int32_t i_ref = 0; i_ref < n_ref; ++i_ref ) {
        m_Refs[i_ref].Read(in);
    }

    streampos extra_pos = in.tellg();
    in.seekg(0, ios::end);
    streampos end_pos = in.tellg();
    in.seekg(extra_pos);

    if ( end_pos-extra_pos >= 8 ) {
        m_UnmappedCount = s_ReadUInt64(in);
        extra_pos += 8;
    }

    if ( end_pos != extra_pos ) {
        LOG_POST(Warning<<
                 "Extra "<<(end_pos-extra_pos)<<" bytes in BAM index");
    }
}


static inline
Uint8 s_EstimatedPos(const CBGZFPos& pos)
{
    return pos.GetFileBlockPos() + Uint8(pos.GetByteOffset()*kEstimatedCompression);
}


static inline
Uint8 s_EstimatedSize(const CBGZFRange& range)
{
    Uint8 pos1 = s_EstimatedPos(range.first);
    Uint8 pos2 = s_EstimatedPos(range.second);
    if ( pos1 >= pos2 )
        return 0;
    else
        return pos2-pos1;
}


const SBamIndexRefIndex& CBamIndex::GetRef(size_t ref_index) const
{
    if ( ref_index >= GetRefCount() ) {
        NCBI_THROW(CBamException, eInvalidArg,
                   "Bad reference sequence index");
    }
    return m_Refs[ref_index];
}


CBGZFRange CBamIndex::GetTotalFileRange(size_t ref_index) const
{
    CBGZFRange total_range(CBGZFPos(-1), CBGZFPos(0));
    for ( auto& b : GetRef(ref_index).m_Bins ) {
        for ( auto& c : b.m_Chunks ) {
            if ( c.first < total_range.first )
                total_range.first = c.first;
            if ( total_range.second < c.second )
                total_range.second = c.second;
        }
    }
    return total_range;
}


static void sx_SetTitle(CSeq_graph& graph, CSeq_annot& annot,
                        string title, string name)
{
    if ( name.empty() ) {
        name = "BAM coverage";
    }
    if ( title.empty() ) {
        title = name;
    }
    graph.SetTitle(title);
    annot.SetNameDesc(name);
}


CRef<CSeq_annot>
CBamIndex::MakeEstimatedCoverageAnnot(const CBamHeader& header,
                                      const string& ref_name,
                                      const string& seq_id,
                                      const string& annot_name) const
{
    CSeq_id id(seq_id);
    return MakeEstimatedCoverageAnnot(header, ref_name, id, annot_name);
}


CRef<CSeq_annot>
CBamIndex::MakeEstimatedCoverageAnnot(const CBamHeader& header,
                                      const string& ref_name,
                                      const CSeq_id& seq_id,
                                      const string& annot_name) const
{
    size_t ref_index = header.GetRefIndex(ref_name);
    if ( ref_index == size_t(-1) ) {
        NCBI_THROW_FMT(CBamException, eInvalidArg,
                       "Cannot find RefSeq: "<<ref_name);
    }
    return MakeEstimatedCoverageAnnot(ref_index, seq_id, annot_name,
                                      header.GetRefLength(ref_index));
}


CRef<CSeq_annot>
CBamIndex::MakeEstimatedCoverageAnnot(size_t ref_index,
                                      const string& seq_id,
                                      const string& annot_name,
                                      TSeqPos length) const
{
    CSeq_id id(seq_id);
    return MakeEstimatedCoverageAnnot(ref_index, id, annot_name, length);
}


vector<uint64_t>
CBamIndex::CollectEstimatedCoverage(size_t ref_index) const
{
    vector<uint64_t> vv;
    for ( auto& b : GetRef(ref_index).m_Bins ) {
        COpenRange<TSeqPos> range = b.GetSeqRange();
        uint32_t len = range.GetLength();
        if ( len == kMinBinSize ) {
            size_t index = range.GetFrom()/kMinBinSize;
            if ( index >= vv.size() )
                vv.resize(index+1);
            uint64_t value = 0;
            for ( auto& c : b.m_Chunks ) {
                value += s_EstimatedSize(c);
            }
            vv[index] = value;
        }
    }
    return vv;
}


CRef<CSeq_annot>
CBamIndex::MakeEstimatedCoverageAnnot(size_t ref_index,
                                      const CSeq_id& seq_id,
                                      const string& annot_name,
                                      TSeqPos length) const
{
    vector<uint64_t> vv = CollectEstimatedCoverage(ref_index);
    if ( vv.empty() ) vv.push_back(0);
    uint32_t count = uint32_t(vv.size());
    if ( length == 0 || length == kInvalidSeqPos ) {
        length = count*kMinBinSize;
    }
    uint64_t max_value = *max_element(vv.begin(), vv.end());

    CRef<CSeq_annot> annot(new CSeq_annot);
    CRef<CSeq_graph> graph(new CSeq_graph);
    annot->SetData().SetGraph().push_back(graph);
    sx_SetTitle(*graph, *annot, annot_name, annot_name);

    graph->SetLoc().SetInt().SetId().Assign(seq_id);
    graph->SetLoc().SetInt().SetFrom(0);
    graph->SetLoc().SetInt().SetTo(length-1);
    graph->SetComp(kMinBinSize);
    graph->SetNumval(count);
    CByte_graph& bgraph = graph->SetGraph().SetByte();
    bgraph.SetAxis(0);
    vector<char>& bvalues = bgraph.SetValues();
    bvalues.resize(count);
    Uint1 bmin = 0xff, bmax = 0;
    for ( size_t i = 0; i < count; ++i ) {
        Uint1 b = Uint1(vv[i]*255./max_value+.5);
        bvalues[i] = b;
        bmin = min(bmin, b);
        bmax = max(bmax, b);
    }
    bgraph.SetMin(bmin);
    bgraph.SetMax(bmax);
    return annot;
}


/////////////////////////////////////////////////////////////////////////////
// CBamHeader
/////////////////////////////////////////////////////////////////////////////


CBamHeader::CBamHeader()
{
}


CBamHeader::CBamHeader(const string& bam_file_name)
{
    Read(bam_file_name);
}


CBamHeader::CBamHeader(CNcbiIstream& file_stream)
{
    Read(file_stream);
}


CBamHeader::~CBamHeader()
{
}


void SBamHeaderRefInfo::Read(CNcbiIstream& in)
{
    int32_t l_name = s_ReadInt32(in);
    s_ReadString(in, m_Name, l_name);
    m_Name.resize(l_name-1);
    m_Length = s_ReadInt32(in);
}


void SBamHeaderRefInfo::Read(CBGZFStream& in)
{
    int32_t l_name = s_ReadInt32(in);
    s_ReadString(in, m_Name, l_name);
    m_Name.resize(l_name-1);
    m_Length = s_ReadInt32(in);
}


void CBamHeader::Read(const string& bam_file_name)
{
    if ( 1 ) {
        CBGZFFile file(bam_file_name);
        CBGZFStream file_stream(file);
        Read(file_stream);
    }
    else {
        CNcbiIfstream file_stream(bam_file_name.c_str(), ios::binary);
        Read(file_stream);
    }
}


void CBamHeader::Read(CNcbiIstream& file_stream)
{
    m_RefByName.clear();
    m_Refs.clear();
    CCompressionIStream in(file_stream,
                           new CZipStreamDecompressor(CZipCompression::fGZip),
                           CCompressionIStream::fOwnProcessor);
    s_ReadMagic(in, "BAM\1");
    int32_t l_text = s_ReadInt32(in);
    s_ReadString(in, m_Text, l_text);
    int32_t n_ref = s_ReadInt32(in);
    m_Refs.resize(n_ref);
    for ( int32_t i_ref = 0; i_ref < n_ref; ++i_ref ) {
        m_Refs[i_ref].Read(in);
        m_RefByName[m_Refs[i_ref].m_Name] = i_ref;
    }
    m_AlignStart = CBGZFPos::GetInvalid();
}


void CBamHeader::Read(CBGZFStream& stream)
{
    m_RefByName.clear();
    m_Refs.clear();
    s_ReadMagic(stream, "BAM\1");
    int32_t l_text = s_ReadInt32(stream);
    s_ReadString(stream, m_Text, l_text);
    int32_t n_ref = s_ReadInt32(stream);
    m_Refs.resize(n_ref);
    for ( int32_t i_ref = 0; i_ref < n_ref; ++i_ref ) {
        m_Refs[i_ref].Read(stream);
        m_RefByName[m_Refs[i_ref].m_Name] = i_ref;
    }
    m_AlignStart = stream.GetSeekPos();
}


size_t CBamHeader::GetRefIndex(const string& name) const
{
    auto iter = m_RefByName.find(name);
    if ( iter == m_RefByName.end() ) {
        return size_t(-1);
    }
    return iter->second;
}


/////////////////////////////////////////////////////////////////////////////
// CBamFileRangeSet
/////////////////////////////////////////////////////////////////////////////


CBamFileRangeSet::CBamFileRangeSet()
{
}


CBamFileRangeSet::CBamFileRangeSet(const CBamIndex& index,
                                   size_t ref_index,
                                   COpenRange<TSeqPos> ref_range)
{
    AddRanges(index, ref_index, ref_range);
}


CBamFileRangeSet::~CBamFileRangeSet()
{
}


ostream& operator<<(ostream& out, const CBamFileRangeSet& ranges)
{
    cout << '(';
    for ( auto& r : ranges ) {
        cout << " (" << r.first<<" "<<r.second<<")";
    }
    return cout << " )";
}


inline
void CBamFileRangeSet::AddSortedRanges(const vector<CBGZFRange>& ranges)
{
    for ( auto iter = ranges.begin(); iter != ranges.end(); ) {
        CBGZFPos start = iter->first, end = iter->second;
        for ( ++iter; iter != ranges.end() && !(end < iter->first); ++iter ) {
            if ( end < iter->second ) {
                end = iter->second;
            }
        }
        m_Ranges += CBGZFRange(start, end);
    }
}


void CBamFileRangeSet::AddRanges(const CBamIndex& index,
                                 size_t ref_index,
                                 COpenRange<TSeqPos> ref_range)
{
    const SBamIndexRefIndex& ref = index.GetRef(ref_index);
    // set limits
    CBGZFRange limit = ref.GetLimitRange(ref_range);
    if ( ref_range.Empty() ) {
        return;
    }
    vector<CBGZFRange> ranges;
    for ( uint32_t k = 0; k <= CBamIndex::kMaxLevel; ++k ) {
        ref.AddLevelFileRanges(ranges, limit, ref_range, k);
    }
    gfx::timsort(ranges.begin(), ranges.end());
    AddSortedRanges(ranges);
}


void CBamFileRangeSet::AddWhole(const CBamHeader& header)
{
    CBGZFRange whole;
    whole.first = header.GetAlignStart();
    whole.second = CBGZFPos::GetInvalid();
    m_Ranges += whole;
}


void CBamFileRangeSet::SetWhole(const CBamHeader& header)
{
    Clear();
    AddWhole(header);
}


void CBamFileRangeSet::SetRanges(const CBamIndex& index,
                                 size_t ref_index,
                                 COpenRange<TSeqPos> ref_range)
{
    Clear();
    AddRanges(index, ref_index, ref_range);
}


/////////////////////////////////////////////////////////////////////////////
// CBamRawDb
/////////////////////////////////////////////////////////////////////////////


CBamRawDb::~CBamRawDb()
{
}


void CBamRawDb::Open(const string& bam_path)
{
    m_File = new CBGZFFile(bam_path);
    CBGZFStream stream(*m_File);
    m_Header.Read(stream);
}


void CBamRawDb::Open(const string& bam_path, const string& index_path)
{
    m_Index.Read(index_path);
    m_File = new CBGZFFile(bam_path);
    CBGZFStream stream(*m_File);
    m_Header.Read(stream);
}


/////////////////////////////////////////////////////////////////////////////
// SBamAlignInfo
/////////////////////////////////////////////////////////////////////////////

string SBamAlignInfo::get_read() const
{
    string ret;
    if ( uint32_t len = get_read_len() ) {
        ret.resize(len);
        char* dst = &ret[0];
        const char* src = get_read_ptr();
        for ( uint32_t len = get_read_len(); len; ) {
            char c = *src++;
            uint32_t b1 = (c >> 4)&0xf;
            uint32_t b2 = (c     )&0xf;
            *dst = kBaseSymbols[b1];
            if ( len == 1 ) {
                break;
            }
            dst[1] = kBaseSymbols[b2];
            dst += 2;
            len -= 2;
        }
    }
    return ret;
}


void SBamAlignInfo::get_read(CBamString& str) const
{
    uint32_t len = get_read_len();
    str.reserve(len+1);
    str.resize(len);
    char* dst = str.data();
    const char* src = get_read_ptr();
    for ( uint32_t len = get_read_len(); len; ) {
        char c = *src++;
        uint32_t b1 = (c >> 4)&0xf;
        uint32_t b2 = (c     )&0xf;
        *dst = kBaseSymbols[b1];
        if ( len == 1 ) {
            break;
        }
        dst[1] = kBaseSymbols[b2];
        dst += 2;
        len -= 2;
    }
}


uint32_t SBamAlignInfo::get_cigar_pos() const
{
    // ignore optional starting hard break
    // return optional starting soft break
    // or 0 if there is no soft break
    const char* ptr = get_cigar_ptr();
    for ( uint16_t count = get_cigar_ops_count(); count--; ) {
        uint32_t op = SBamUtil::MakeUint4(ptr);
        ptr += 4;
        switch ( op & 0xf ) {
        case kCIGAR_H:
            continue;
        case kCIGAR_S:
            return op >> 4;
        default:
            return 0;
        }
    }
    return 0;
}


uint32_t SBamAlignInfo::get_cigar_ref_size() const
{
    // ignore hard and soft breaks, ignore insertions
    // only match/mismatch, deletes, and skips remain
    uint32_t ret = 0;
    const char* ptr = get_cigar_ptr();
    for ( uint16_t count = get_cigar_ops_count(); count--; ) {
        uint32_t op = SBamUtil::MakeUint4(ptr);
        ptr += 4;
        uint32_t seglen = op >> 4;
        switch ( op & 0xf ) {
        case kCIGAR_M:
        case kCIGAR_eq:
        case kCIGAR_X:
        case kCIGAR_D:
        case kCIGAR_N:
            ret += seglen;
            break;
        default:
            break;
        }
    }
    return ret;
}


uint32_t SBamAlignInfo::get_cigar_read_size() const
{
    // ignore hard and soft breaks, ignore deletions and skips
    // only match/mismatch and inserts remain
    uint32_t ret = 0;
    const char* ptr = get_cigar_ptr();
    for ( uint16_t count = get_cigar_ops_count(); count--; ) {
        uint32_t op = SBamUtil::MakeUint4(ptr);
        ptr += 4;
        uint32_t seglen = op >> 4;
        switch ( op & 0xf ) {
        case kCIGAR_M:
        case kCIGAR_eq:
        case kCIGAR_X:
        case kCIGAR_I:
            ret += seglen;
            break;
        default:
            break;
        }
    }
    return ret;
}


pair< COpenRange<uint32_t>, COpenRange<uint32_t> > SBamAlignInfo::get_cigar_alignment(void) const
{
    // ignore hard and soft breaks, ignore deletions and skips
    // only match/mismatch and inserts remain
    uint32_t ref_pos = get_ref_pos(), ref_size = 0, read_pos = 0, read_size = 0;
    bool first = true;
    const char* ptr = get_cigar_ptr();
    for ( uint16_t count = get_cigar_ops_count(); count--; ) {
        uint32_t op = SBamUtil::MakeUint4(ptr);
        ptr += 4;
        uint32_t seglen = op >> 4;
        switch ( op & 0xf ) {
        case kCIGAR_M:
        case kCIGAR_eq:
        case kCIGAR_X:
            ref_size += seglen;
            read_size += seglen;
            break;
        case kCIGAR_D:
        case kCIGAR_N:
            ref_size += seglen;
            break;
        case kCIGAR_I:
            read_size += seglen;
            break;
        case kCIGAR_S:
            if ( first ) {
                read_pos = seglen;
            }
            break;
        default:
            break;
        }
        first = false;
    }
    pair< COpenRange<uint32_t>, COpenRange<uint32_t> > ret;
    ret.first.SetFrom(ref_pos).SetLength(ref_size);
    ret.second.SetFrom(read_pos).SetLength(read_size);
    return ret;
}


string SBamAlignInfo::get_cigar() const
{
    // ignore hard and soft breaks
    CNcbiOstrstream ret;
    const char* ptr = get_cigar_ptr();
    for ( uint16_t count = get_cigar_ops_count(); count--; ) {
        uint32_t op = SBamUtil::MakeUint4(ptr);
        ptr += 4;
        switch ( op & 0xf ) {
        case kCIGAR_H:
        case kCIGAR_S:
            continue;
        default:
            break;
        }
        uint32_t seglen = op >> 4;
        ret << kCIGARSymbols[op & 0xf] << seglen;
    }
    return CNcbiOstrstreamToString(ret);
}


static inline char* s_format(char* dst, uint32_t v)
{
    if ( v < 10 ) {
        *dst = '0'+v;
        return dst+1;
    }
    if ( v >= 100 ) {
        dst = s_format(dst, v/100);
    }
    dst[0] = '0'+(v/10);
    dst[1] = '0'+(v%10);
    return dst+2;
}


void SBamAlignInfo::get_cigar(CBamString& str) const
{
    // it takes at most 10 symbols per op - op char + 9-symbols number up to 2^28
    size_t count = get_cigar_ops_count();
    str.reserve(count*10+1);
    char* dst = str.data();
    const char* src = get_cigar_ptr();
    for ( ; count--; ) {
        uint32_t op = SBamUtil::MakeUint4(src);
        src += 4;
        switch ( op & 0xf ) {
        case kCIGAR_H:
        case kCIGAR_S:
            continue;
        default:
            break;
        }
        uint32_t seglen = op >> 4;
        *dst = kCIGARSymbols[op & 0xf];
        dst = s_format(dst+1, seglen);
    }
    str.resize(dst-str.data());
}


static inline int sx_TagDataSize(char type)
{
    switch (type) {
    case 'A':
    case 'c':
    case 'C':
        return 1;
    case 's':
    case 'S':
        return 2;
    case 'i':
    case 'I':
    case 'f':
        return 4;
    case 'Z':
    case 'H':
        return -'Z';
    case 'B':
        return -'B';
    default:
        return 0;
    }
}


static inline int sx_GetStringLen(const char* beg, const char* end)
{
    for ( const char* ptr = beg; ptr != end; ++ptr ) {
        if ( !*ptr ) {
            return ptr-beg;
        }
    }
    // no zero termination -> bad string
    return -1;
}


const char* SBamAlignInfo::get_aux_data(char c1, char c2) const
{
    const char* end = get_aux_data_end();
    const char* ptr = get_aux_data_ptr();
    for ( ;; ) {
        if ( ptr+3 > end ) {
            // end of data
            return 0;
        }
        if ( ptr[0] == c1 && ptr[1] == c2 ) {
            return ptr;
        }
        int size = sx_TagDataSize(ptr[2]);
        ptr += 3;
        if ( size > 0 ) {
            if ( ptr+size > end ) {
                // end of data
                return 0;
            }
            ptr += size;
        }
        else if ( size == -'Z' ) {
            // zero terminated
            size = sx_GetStringLen(ptr, end);
            if ( size < 0 ) {
                // no termination, cannot continue parsing
                return 0;
            }
            ptr += size+1;
        }
        else if ( size == -'B' ) {
            if ( ptr+5 > end ) {
                // end of data
                return 0;
            }
            size = sx_TagDataSize(ptr[0]);
            uint32_t count = SBamUtil::MakeUint4(ptr+1);
            ptr += 5;
            // array
            if ( size > 0 ) {
                size *= count;
                if ( ptr+size > end ) {
                    // end of data
                    return 0;
                }
                ptr += size;
            }
            else if ( size == -'Z' ) {
                // zero terminated
                for ( uint32_t i = 0; i < count; ++i ) {
                    size = sx_GetStringLen(ptr, end);
                    if ( size < 0 ) {
                        // no termination, cannot continue parsing
                        return 0;
                    }
                    ptr += size+1;
                }
            }
            else {
                // bad element type, cannot continue parsing
                return 0;
            }
        }
        else {
            // bad type, cannot continue parsing
            return 0;
        }
    }        
    return 0;
}


CTempString SBamAlignInfo::get_aux_data_string(char c1, char c2) const
{
    const char* data = get_aux_data(c1, c2);
    if ( !data || data[2] != 'Z' ) {
        return CTempString();
    }
    data += 3;
    int len = sx_GetStringLen(data, get_aux_data_end());
    if ( len < 0 ) {
        return CTempString();
    }
    return CTempString(data, len);
}


CTempString SBamAlignInfo::get_short_seq_accession_id() const
{
    return get_aux_data_string('R', 'G');
}


void SBamAlignInfo::Read(CBGZFStream& in)
{
    m_RecordSize = SBamUtil::MakeUint4(in.Read(4));
    m_RecordPtr = in.Read(m_RecordSize);
    _ASSERT(get_aux_data_ptr() <= get_aux_data_end());
}


/////////////////////////////////////////////////////////////////////////////
// CBamRawAlignIterator
/////////////////////////////////////////////////////////////////////////////


CBamRawAlignIterator::CBamRawAlignIterator(CBamRawDb& bam_db,
                                           const string& ref_label,
                                           TSeqPos ref_pos,
                                           TSeqPos window)
    : m_Reader(bam_db.GetFile())
{
    CRange<TSeqPos> ref_range(ref_pos, ref_pos);
    if ( window && ref_pos < kInvalidSeqPos-window ) {
        ref_range.SetToOpen(ref_pos+window);
    }
    else {
        ref_range.SetToOpen(kInvalidSeqPos);
    }
    Select(bam_db, ref_label, ref_range);
}


void CBamRawAlignIterator::x_Select(const CBamHeader& header)
{
    m_RefIndex = size_t(-1);
    m_RefRange = CRange<TSeqPos>::GetEmpty();
    m_Ranges.SetWhole(header);
    m_NextRange = m_Ranges.begin();
    if ( x_UpdateRange() ) {
        Next();
    }
}


void CBamRawAlignIterator::x_Select(const CBamIndex& index,
                                    size_t ref_index,
                                    CRange<TSeqPos> ref_range)
{
    m_RefIndex = ref_index;
    m_RefRange = ref_range;
    m_Ranges.SetRanges(index, ref_index, ref_range);
    m_NextRange = m_Ranges.begin();
    if ( x_UpdateRange() ) {
        Next();
    }
}


bool CBamRawAlignIterator::x_UpdateRange()
{
    if ( m_NextRange == m_Ranges.end() ) {
        m_CurrentRangeEnd = CBGZFPos(0);
        return false;
    }
    else {
        m_CurrentRangeEnd = m_NextRange->second;
        m_Reader.Seek(m_NextRange->first, m_NextRange->second);
        ++m_NextRange;
        return true;
    }
}


bool CBamRawAlignIterator::x_NeedToSkip()
{
    _ASSERT(*this);
    m_AlignInfo.Read(m_Reader);
    if ( m_RefIndex != size_t(-1) ) {
        if ( size_t(m_AlignInfo.get_ref_index()) != m_RefIndex ) {
            // wrong reference sequence
            return true;
        }
        TSeqPos pos = m_AlignInfo.get_ref_pos();
        if ( pos >= m_RefRange.GetToOpen() ) {
            // after search range
            x_Stop();
            return false;
        }
        if ( pos < m_RefRange.GetFrom() ) {
            TSeqPos end = pos + m_AlignInfo.get_cigar_ref_size();
            if ( end <= m_RefRange.GetFrom() ) {
                // before search range
                return true;
            }
        }
    }
    return false;
}


void CBamRawAlignIterator::Next()
{
    while ( x_NextAnnot() && x_NeedToSkip() ) {
        // continue
    }
}


void CBamRawAlignIterator::GetSegments(vector<int>& starts, vector<TSeqPos>& lens) const
{
    TSeqPos refpos = GetRefSeqPos();
    TSeqPos seqpos = 0;

    // ignore hard breaks
    // omit soft breaks in the alignment
    const char* ptr = m_AlignInfo.get_cigar_ptr();
    for ( uint16_t count = m_AlignInfo.get_cigar_ops_count(); count--; ) {
        uint32_t op = SBamUtil::MakeUint4(ptr);
        ptr += 4;
        TSeqPos seglen = op >> 4;
        int refstart, seqstart;
        switch ( op & 0xf ) {
        case SBamAlignInfo::kCIGAR_H:
        case SBamAlignInfo::kCIGAR_P: // ?
            continue;
        case SBamAlignInfo::kCIGAR_S:
            seqpos += seglen;
            continue;
        case SBamAlignInfo::kCIGAR_M:
        case SBamAlignInfo::kCIGAR_eq:
        case SBamAlignInfo::kCIGAR_X:
            refstart = refpos;
            refpos += seglen;
            seqstart = seqpos;
            seqpos += seglen;
            break;
        case SBamAlignInfo::kCIGAR_I:
            refstart = kInvalidSeqPos;
            seqstart = seqpos;
            seqpos += seglen;
            break;
        case SBamAlignInfo::kCIGAR_D:
        case SBamAlignInfo::kCIGAR_N:
            refstart = refpos;
            refpos += seglen;
            seqstart = kInvalidSeqPos;
            break;
        default:
            NCBI_THROW_FMT(CBamException, eBadCIGAR,
                           "Bad CIGAR segment: " << (op & 0xf) << " in " <<GetCIGAR());
        }
        if ( seglen == 0 ) {
            NCBI_THROW_FMT(CBamException, eBadCIGAR,
                           "Zero CIGAR segment: in " << GetCIGAR());
        }
        starts.push_back(refstart);
        starts.push_back(seqstart);
        lens.push_back(seglen);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
