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
#include <util/compress/zlib.hpp>
#include <util/util_exception.hpp>
#include <util/timsort.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqres/seqres__.hpp>
#include <objects/seqloc/seqloc__.hpp>

BEGIN_NCBI_SCOPE

//#define NCBI_USE_ERRCODE_X   BAM2Graph
//NCBI_DEFINE_ERR_SUBCODE_X(6);

BEGIN_SCOPE(objects)

class CSeq_entry;


static const uint32_t kBlockBits = 14;
static const uint32_t kBlockSize = 1<<kBlockBits;
static const uint32_t kBlockBase = 4681;
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


void SBamIndexRefIndex::Read(CNcbiIstream& in)
{
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
    }
    gfx::timsort(m_Bins.begin(), m_Bins.end());
    m_Bins.erase(lower_bound(m_Bins.begin(), m_Bins.end(), kSpecialBin), m_Bins.end());
        
    int32_t n_intv = s_ReadInt32(in);
    m_Intervals.resize(n_intv);
    for ( int32_t i_intv = 0; i_intv < n_intv; ++i_intv ) {
        m_Intervals[i_intv] = s_ReadFilePos(in);
    }
}


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

    CNcbiIfstream in(index_file_name.c_str(), ios::binary);
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


CRef<CSeq_annot>
CBamIndex::MakeEstimatedCoverageAnnot(size_t ref_index,
                                      const CSeq_id& seq_id,
                                      const string& annot_name,
                                      TSeqPos length) const
{
    vector<uint64_t> vv;
    for ( auto& b : GetRef(ref_index).m_Bins ) {
        COpenRange<TSeqPos> range = b.GetSeqRange();
        uint32_t len = range.GetLength();
        if ( len == kBlockSize ) {
            size_t index = range.GetFrom()/kBlockSize;
            if ( index >= vv.size() )
                vv.resize(index+1);
            uint64_t value = 0;
            for ( auto& c : b.m_Chunks ) {
                value += s_EstimatedSize(c);
            }
            vv[index] = value;
        }
    }
    if ( vv.empty() ) vv.push_back(0);
    uint32_t count = uint32_t(vv.size());
    if ( length == 0 || length == kInvalidSeqPos ) {
        length = count*kBlockSize;
    }
    uint64_t max_value = *max_element(vv.begin(), vv.end());

    CRef<CSeq_annot> annot(new CSeq_annot);
    CRef<CSeq_graph> graph(new CSeq_graph);
    annot->SetData().SetGraph().push_back(graph);
    sx_SetTitle(*graph, *annot, annot_name, annot_name);

    graph->SetLoc().SetInt().SetId().Assign(seq_id);
    graph->SetLoc().SetInt().SetFrom(0);
    graph->SetLoc().SetInt().SetTo(length-1);
    graph->SetComp(kBlockSize);
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
    CNcbiIfstream file_stream(bam_file_name.c_str(), ios::binary);
    Read(file_stream);
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
}


size_t CBamHeader::GetRefIndex(const string& name) const
{
    auto iter = m_RefByName.find(name);
    if ( iter == m_RefByName.end() ) {
        return size_t(-1);
    }
    return iter->second;
}


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


inline
void CBamFileRangeSet::AddBinsRanges(vector<CBGZFRange>& ranges,
                                     const CBGZFRange& limit,
                                     const SBamIndexRefIndex& ref,
                                     TBin bin1, TBin bin2)
{
    for ( auto it = lower_bound(ref.m_Bins.begin(), ref.m_Bins.end(), bin1);
          it != ref.m_Bins.end() && it->m_Bin <= bin2; ++it ) {
        for ( auto c : it->m_Chunks ) {
            if ( c.first < limit.first ) {
                c.first = limit.first;
            }
            if ( limit.second < c.second ) {
                c.second = limit.second;
            }
            if ( c.first < c.second ) {
                ranges.push_back(c);
            }
        }
    }
}


void CBamFileRangeSet::AddRanges(const CBamIndex& index,
                                 size_t ref_index,
                                 COpenRange<TSeqPos> ref_range)
{
    if ( ref_range.Empty() ) {
        return;
    }
    const SBamIndexRefIndex& ref = index.GetRef(ref_index);
    TSeqPos beg = ref_range.GetFrom();
    TSeqPos end = min(ref_range.GetToOpen(),
                      TSeqPos(ref.m_Intervals.size()*kBlockSize));
    if ( end <= beg ) {
        return;
    }
    --end;
    _ASSERT((beg>>29) == 0);
    _ASSERT((end>>29) == 0);

    // set limits
    CBGZFRange limit;
    // start limit is from intervals and beg position
    _ASSERT((beg>>kBlockBits)<ref.m_Intervals.size());
    limit.first = ref.m_Intervals[beg>>kBlockBits];
    // end limit is from low-level block after end position
    limit.second = CBGZFPos(CBGZFPos::TVirtualPos(-1));
    SBamIndexBinInfo::TBin end_bin =
        SBamIndexBinInfo::TBin(kBlockBase + (end >> kBlockBits) + 1);
    auto end_it = lower_bound(ref.m_Bins.begin(), ref.m_Bins.end(), end_bin);
    if ( end_it != ref.m_Bins.end() ) {
        limit.second = end_it->m_Chunks[0].first;
    }

    vector<CBGZFRange> ranges;
    for ( unsigned k = 0; k <= 5; ++k ) {
        unsigned shift = kBlockBits+3*k;
        unsigned base = kBlockBase>>(3*k);
        AddBinsRanges(ranges, limit, ref,
                      base + (beg>>shift),
                      base + (end>>shift));
    }
    gfx::timsort(ranges.begin(), ranges.end());
    AddSortedRanges(ranges);
}


void CBamFileRangeSet::SetRanges(const CBamIndex& index,
                                 size_t ref_index,
                                 COpenRange<TSeqPos> ref_range)
{
    Clear();
    AddRanges(index, ref_index, ref_range);
}


CBamRawDb::~CBamRawDb()
{
}


void CBamRawDb::Open(const string& bam_path, const string& index_path)
{
    m_Index.Read(index_path);
    m_File = new CBGZFFile(bam_path);
    CBGZFStream stream(*m_File);
    m_Header.Read(stream);
}


string SBamAlignInfo::get_read() const
{
    string ret;
    const char* ptr = get_read_ptr();
    for ( uint32_t len = get_read_len(); len; ) {
        char c = *ptr++;
        uint32_t b1 = (c >> 4)&0xf;
        uint32_t b2 = (c     )&0xf;
        ret += "=ACMGRSVTWYHKDBN"[b1];
        if ( len == 1 ) {
            len = 0;
        }
        else {
            ret += "=ACMGRSVTWYHKDBN"[b2];
            len -= 2;
        }
    }
    return ret;
}


uint32_t SBamAlignInfo::get_cigar_ref_size() const
{
    uint32_t ret = 0;
    const char* ptr = get_cigar_ptr();
    for ( uint16_t count = get_cigar_ops_count(); count--; ) {
        uint32_t op = SBamUtil::MakeUint4(ptr);
        ptr += 4;
        switch ( op & 0xf ) {
        case 0: // M
        case 2: // D
        case 3: // N ?
        case 7: // =
        case 8: // X
            ret += op >> 4;
            break;
        default:
            break;
        }
    }
    return ret;
}


string SBamAlignInfo::get_cigar() const
{
    CNcbiOstrstream ret;
    const char* ptr = get_cigar_ptr();
    for ( uint16_t count = get_cigar_ops_count(); count--; ) {
        uint32_t op = SBamUtil::MakeUint4(ptr);
        ptr += 4;
        uint32_t len = op >> 4;
        op &= 0xf;
        if ( len != 1 )
            ret << len;
        ret << "MIDNSHP=X???????"[op];
    }
    return CNcbiOstrstreamToString(ret);
}


void SBamAlignInfo::Read(CBGZFStream& in)
{
    m_RecordSize = SBamUtil::MakeUint4(in.Read(4));
    m_RecordPtr = in.Read(m_RecordSize);
    _ASSERT(get_qual_ptr() <= get_qual_end());
}


void CBamRawAlignIterator::x_Select(const CBamIndex& index,
                                    size_t ref_index, CRange<TSeqPos> ref_range)
{
    m_RefIndex = ref_index;
    m_RefRange = ref_range;
    m_Ranges.SetRanges(index, ref_index, ref_range);
    m_NextRange = m_Ranges.begin();
    if ( x_UpdateRange() ) {
        x_Settle();
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
        m_Reader.Seek(m_NextRange->first);
        ++m_NextRange;
        return true;
    }
}


bool CBamRawAlignIterator::x_NeedToSkip()
{
    _ASSERT(*this);
    m_AlignInfo.Read(m_Reader);
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
    return false;
}


void CBamRawAlignIterator::x_Settle()
{
    while ( x_NeedToSkip() ) {
        if ( !x_NextAnnot() ) {
            return;
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
