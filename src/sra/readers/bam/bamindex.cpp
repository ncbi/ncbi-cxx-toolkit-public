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
#include <util/util_exception.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqres/seqres__.hpp>
#include <objects/seqloc/seqloc__.hpp>

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
string s_ReadString(CNcbiIstream& in, size_t len)
{
    string ret(len, ' ');
    s_Read(in, &ret[0], len);
    return ret;
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
uint16_t s_MakeUint16(const char* buf)
{
    return uint16_t(uint8_t(buf[0]))|
        (uint16_t(uint8_t(buf[1]))<<8);
}


static inline
uint32_t s_MakeUint32(const char* buf)
{
    return uint32_t(uint8_t(buf[0]))|
        (uint32_t(uint8_t(buf[1]))<<8)|
        (uint32_t(uint8_t(buf[2]))<<16)|
        (uint32_t(uint8_t(buf[3]))<<24);
}


static inline
uint64_t s_MakeUint64(const char* buf)
{
    return uint64_t(uint8_t(buf[0]))|
        (uint64_t(uint8_t(buf[1]))<<8)|
        (uint64_t(uint8_t(buf[2]))<<16)|
        (uint64_t(uint8_t(buf[3]))<<24)|
        (uint64_t(uint8_t(buf[4]))<<32)|
        (uint64_t(uint8_t(buf[5]))<<40)|
        (uint64_t(uint8_t(buf[6]))<<48)|
        (uint64_t(uint8_t(buf[7]))<<56);
}


static inline
uint32_t s_ReadUInt32(CNcbiIstream& in)
{
    char buf[4];
    s_Read(in, buf, 4);
    return s_MakeUint32(buf);
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
    return s_MakeUint64(buf);
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

static
SBamIndexBinInfo x_ReadBin(CNcbiIstream& in)
{
    SBamIndexBinInfo bin;
    bin.m_Bin = s_ReadUInt32(in);
    int32_t n_chunk = s_ReadInt32(in);
    bin.m_Chunks.reserve(n_chunk);
    for ( int32_t i_chunk = 0; i_chunk < n_chunk; ++i_chunk ) {
        bin.m_Chunks.push_back(s_ReadFileRange(in));
    }
    return bin;
}


static
SBamIndexRefIndex x_ReadRef(CNcbiIstream& in)
{
    SBamIndexRefIndex ref;
    int32_t n_bin = s_ReadInt32(in);
    ref.m_Bins.reserve(n_bin);
    for ( int32_t i_bin = 0; i_bin < n_bin; ++i_bin ) {
        SBamIndexBinInfo bin = x_ReadBin(in);
        if ( bin.m_Bin == 37450 ) {
            if ( bin.m_Chunks.size() != 2 ) {
                NCBI_THROW(CBamException, eOtherError,
                           "Bad unmapped bin format");
            }
            ref.m_UnmappedChunk = bin.m_Chunks[0];
            ref.m_MappedCount = bin.m_Chunks[1].first.GetVirtualPos();
            ref.m_UnmappedCount = bin.m_Chunks[1].second.GetVirtualPos();
        }
        else {
            ref.m_Bins.push_back(bin);
        }
    }
        
    int32_t n_intv = s_ReadInt32(in);
    ref.m_Intervals.reserve(n_intv);
    for ( int32_t i_intv = 0; i_intv < n_intv; ++i_intv ) {
        ref.m_Intervals.push_back(s_ReadFilePos(in));
    }
    return ref;
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
    m_Refs.reserve(n_ref);
    for ( int32_t i_ref = 0; i_ref < n_ref; ++i_ref ) {
        m_Refs.push_back(x_ReadRef(in));
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


CBGZFRange CBamIndex::GetTotalFileRange(size_t ref_index) const
{
    CBGZFRange total_range(CBGZFPos(-1), CBGZFPos(0));
    for ( auto& b : m_Refs[ref_index].m_Bins ) {
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
    if ( ref_index >= m_Refs.size() ) {
        NCBI_THROW(CBamException, eInvalidArg,
                   "Bad reference sequence index");
    }
    const uint32_t kBlockSize = 1<<14;
    vector<uint64_t> vv;
    for ( auto& b : m_Refs[ref_index].m_Bins ) {
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


CBamHeader::~CBamHeader()
{
}


SBamRefHeaderInfo CBamHeader::ReadRef(CNcbiIstream& in)
{
    SBamRefHeaderInfo ref;
    int32_t l_name = s_ReadInt32(in);
    ref.m_Name = s_ReadString(in, l_name);
    ref.m_Name.resize(l_name-1);
    ref.m_Length = s_ReadInt32(in);
    return ref;
}


void CBamHeader::Read(const string& bam_file_name)
{
    m_RefByName.clear();
    m_Refs.clear();
    CNcbiIfstream file_stream(bam_file_name.c_str(), ios::binary);
    CCompressionIStream in(file_stream,
                           new CZipStreamDecompressor(CZipCompression::fGZip),
                           CCompressionIStream::fOwnProcessor);
    s_ReadMagic(in, "BAM\1");
    int32_t l_text = s_ReadInt32(in);
    m_Text = s_ReadString(in, l_text);
    int32_t n_ref = s_ReadInt32(in);
    m_Refs.reserve(n_ref);
    for ( int32_t i_ref = 0; i_ref < n_ref; ++i_ref ) {
        m_Refs.push_back(ReadRef(in));
        m_RefByName[m_Refs.back().m_Name] = m_RefByName.size()-1;
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


END_SCOPE(objects)
END_NCBI_SCOPE
