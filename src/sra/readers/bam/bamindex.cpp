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
#include <corelib/rwstream.hpp>
#include <util/util_exception.hpp>
#include <util/timsort.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqres/seqres__.hpp>
#include <objects/seqloc/seqloc__.hpp>

#ifndef NCBI_THROW2_FMT
# define NCBI_THROW2_FMT(exception_class, err_code, message, extra)     \
    throw NCBI_EXCEPTION2(exception_class, err_code, FORMAT(message), extra)
#endif


BEGIN_NCBI_SCOPE

//#define NCBI_USE_ERRCODE_X   BAM2Graph
//NCBI_DEFINE_ERR_SUBCODE_X(6);

BEGIN_SCOPE(objects)

class CSeq_entry;

static const size_t kGZipMagicLength = 2;
static const char kGZipMagic[] = "\x1f\x8b";

static const char kBamExt[] = ".bam";

static const size_t kIndexMagicLength = 4;
static const char kBaiExt[] = ".bai";
static const char kIndexMagicBAI[] = "BAI\1";
#ifdef BAM_SUPPORT_CSI
static const char kCsiExt[] = ".csi";
static const char kIndexMagicCSI[] = "CSI\1";
NCBI_PARAM_DECL(bool, BAM, PREFER_CSI);
NCBI_PARAM_DEF_EX(bool, BAM, PREFER_CSI, false, eParam_NoThread, BAM_PREFER_CSI);
#endif
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
const char* s_Read(const char*& buffer_ptr, const char* buffer_end, size_t len)
{
    const char* ret_ptr = buffer_ptr;
    const char* ret_end = ret_ptr + len;
    if ( ret_end > buffer_end ) {
        NCBI_THROW(CIOException, eRead, "BAM index EOF");
    }
    buffer_ptr = ret_end;
    return ret_ptr;
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
void s_ReadString(CBGZFStream& in, string& ret, size_t len)
{
    ret.resize(len);
    s_Read(in, &ret[0], len);
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


void SBamIndexBinInfo::Read(CNcbiIstream& in,
                            SBamIndexParams params)
{
    m_Bin = s_ReadUInt32(in);
#ifdef BAM_SUPPORT_CSI
    if ( params.is_CSI ) {
        m_Overlap = s_ReadFilePos(in);
    }
    else {
        m_Overlap = CBGZFPos();
    }
#endif
    int32_t n_chunks = s_ReadInt32(in);
    m_Chunks.resize(n_chunks);
    for ( int32_t i_chunk = 0; i_chunk < n_chunks; ++i_chunk ) {
        m_Chunks[i_chunk] = s_ReadFileRange(in);
    }
}


const char* SBamIndexBinInfo::Read(const char* ptr, const char* end,
                                   SBamIndexParams params)
{
    size_t n_chunks;
#ifdef BAM_SUPPORT_CSI
    if ( params.is_CSI ) {
        const char* header = s_Read(ptr, end, 16);
        m_Bin = SBamUtil::MakeUint4(header);
        m_Overlap = CBGZFPos(SBamUtil::MakeUint8(header+4));
        n_chunks = SBamUtil::MakeUint4(header+12);
    }
    else {
        const char* header = s_Read(ptr, end, 8);
        m_Bin = SBamUtil::MakeUint4(header);
        m_Overlap = CBGZFPos();
        n_chunks = SBamUtil::MakeUint4(header+4);
    }
#endif
    m_Chunks.reserve(n_chunks);
    const char* data = s_Read(ptr, end, n_chunks*16);
    for ( size_t i = 0; i < n_chunks; ++i ) {
        Uint8 start = SBamUtil::MakeUint8(data+i*16);
        Uint8 end = SBamUtil::MakeUint8(data+i*16+8);
        m_Chunks.push_back(CBGZFRange(CBGZFPos(start), CBGZFPos(end)));
    }
    return ptr;
}


/////////////////////////////////////////////////////////////////////////////
// SBamIndexRefIndex
/////////////////////////////////////////////////////////////////////////////


pair<SBamIndexRefIndex::TBinsIter, SBamIndexRefIndex::TBinsIter>
inline
SBamIndexRefIndex::GetLevelBins(TIndexLevel level) const
{
    pair<TBinsIter, TBinsIter> ret;
    if ( level == 0 ) {
        ret.second = m_Bins.end();
    }
    else {
        ret.second = lower_bound(m_Bins.begin(), m_Bins.end(), GetBinNumberBase(level-1));
    }
    ret.first = lower_bound(m_Bins.begin(), ret.second, GetBinNumberBase(level));
    return ret;
}

struct PByStartFilePos {
    bool operator()(const CBGZFPos p1, const SBamIndexBinInfo& p2) const
        {
            return p1 < p2.GetStartFilePos();
        }
    bool operator()(const SBamIndexBinInfo& p1, const CBGZFPos p2) const
        {
            return p1.GetStartFilePos() < p2;
        }
};


struct PByEndFilePos {
    bool operator()(const CBGZFPos p1, const CBGZFRange& p2) const
        {
            return p1 < p2.second;
        }
    bool operator()(const CBGZFRange& p1, const CBGZFPos p2) const
        {
            return p1.second < p2;
        }
};


void SBamIndexRefIndex::SetLengthFromHeader(TSeqPos length)
{
    if ( length != kInvalidSeqPos ) {
        TSeqPos rounded_length = (length+GetMinBinSize()-1)&~(GetMinBinSize()-1);
        m_EstimatedLength = max(m_EstimatedLength, rounded_length);
    }
}


bool SBamIndexRefIndex::ProcessPseudoBin(SBamIndexBinInfo& bin)
{
    if ( bin.m_Chunks.size() != 2 ) {
        NCBI_THROW(CBamException, eInvalidBAIFormat,
                   "Bad unmapped bin format");
    }
    m_UnmappedChunk = bin.m_Chunks[0];
    m_MappedCount = bin.m_Chunks[1].first.GetVirtualPos();
    m_UnmappedCount = bin.m_Chunks[1].second.GetVirtualPos();
    bin.m_Chunks.erase(bin.m_Chunks.begin(), bin.m_Chunks.begin()+2);
    return bin.m_Chunks.empty();
}


void SBamIndexRefIndex::ProcessBin(const SBamIndexBinInfo& bin)
{
    if ( bin.m_Chunks.empty() ) {
        NCBI_THROW_FMT(CBamException, eInvalidBAIFormat,
                       "No chunks in bin "<<bin.m_Bin);
    }
    for ( size_t i = 0; i < bin.m_Chunks.size(); ++i ) {
        auto& range = bin.m_Chunks[i];
        if ( range.first >= range.second ) {
            NCBI_THROW_FMT(CBamException, eInvalidBAIFormat,
                           "Empty BAM BGZF range in bin "<<bin.m_Bin<<
                           ": "<<range.first<<" - "<<range.second);
        }
        if ( i && bin.m_Chunks[i-1].second >= range.first ) {
            NCBI_THROW_FMT(CBamException, eInvalidBAIFormat,
                           "Overlapping BAM BGZF ranges in bin "<<bin.m_Bin<<
                           ": "<<bin.m_Chunks[i-1].second<<" over "<<range.first);
        }
    }
    auto range = bin.GetSeqRange(*this);
    TSeqPos min_end = range.GetFrom();
    if ( range.GetLength() != GetMinBinSize() ) {
        // at least 1 sub-range
        min_end += range.GetLength() >> kLevelStepBinShift;
    }
    // at least 1 minimal page
    min_end += GetMinBinSize();
    m_EstimatedLength = max(m_EstimatedLength, min_end);
}


void SBamIndexRefIndex::Read(CNcbiIstream& in,
                             SBamIndexParams params,
                             int32_t ref_index)
{
    SBamIndexParams::operator=(params);
    m_EstimatedLength = GetMinBinSize();
    size_t bin_count = 0;
    int32_t n_bin = s_ReadInt32(in);
    m_Bins.resize(n_bin);
    const TBin kPseudoBin = GetPseudoBin();
    for ( int32_t i_bin = 0; i_bin < n_bin; ++i_bin ) {
        SBamIndexBinInfo& bin = m_Bins[bin_count++];
        bin.Read(in, *this);
        if ( bin.m_Bin == kPseudoBin && ProcessPseudoBin(bin) ) {
            --bin_count;
            continue;
        }
        ProcessBin(bin);
    }
    m_Bins.resize(bin_count);
    gfx::timsort(m_Bins.begin(), m_Bins.end());

    if ( !is_CSI ) {
        int32_t n_intv = s_ReadInt32(in);
        m_Overlaps.resize(n_intv);
        for ( int32_t i = 0; i < n_intv; ++i ) {
            m_Overlaps[i] = s_ReadFilePos(in);
        }
        m_EstimatedLength = max(m_EstimatedLength, n_intv*GetMinBinSize());
    }
    _ASSERT(m_EstimatedLength >= GetMinBinSize());
}


const char* SBamIndexRefIndex::Read(const char* buffer_ptr, const char* buffer_end,
                                    SBamIndexParams params,
                                    int32_t ref_index)
{
    SBamIndexParams::operator=(params);
    m_EstimatedLength = GetMinBinSize();
    size_t bin_count = 0;
    size_t n_bin = SBamUtil::MakeUint4(s_Read(buffer_ptr, buffer_end, 4));
    m_Bins.resize(n_bin);
    const TBin kPseudoBin = GetPseudoBin();
    for ( size_t i_bin = 0; i_bin < n_bin; ++i_bin ) {
        SBamIndexBinInfo& bin = m_Bins[bin_count++];
        buffer_ptr = bin.Read(buffer_ptr, buffer_end, *this);
        if ( bin.m_Bin == kPseudoBin && ProcessPseudoBin(bin) ) {
            --bin_count;
            continue;
        }
        ProcessBin(bin);
    }
    m_Bins.resize(bin_count);
    gfx::timsort(m_Bins.begin(), m_Bins.end());

    if ( !is_CSI ) {
        size_t n_intv = SBamUtil::MakeUint4(s_Read(buffer_ptr, buffer_end, 4));
        m_Overlaps.resize(n_intv);
        const char* data = s_Read(buffer_ptr, buffer_end, n_intv*8);
        for ( size_t i = 0; i < n_intv; ++i ) {
            m_Overlaps[i] = CBGZFPos(SBamUtil::MakeUint8(data+i*8));
        }
        m_EstimatedLength = max(m_EstimatedLength, TSeqPos(n_intv*GetMinBinSize()));
    }
    _ASSERT(m_EstimatedLength >= GetMinBinSize());
    return buffer_ptr;
}


static
COpenRange<TSeqPos>
s_GetSeqRange(SBamIndexParams params,
              const pair<SBamIndexRefIndex::TBinsIter, SBamIndexRefIndex::TBinsIter>& iters)
{
    if ( iters.first == iters.second ) {
        return COpenRange<TSeqPos>::GetEmpty();
    }
    else if ( !params.is_CSI && iters.first->m_Bin == params.kMaxBinNumber ) {
        // special case for BAI index of too long sequence
        return COpenRange<TSeqPos>::GetWhole();
    }
    else {
        return iters.first->GetSeqRange(params);
    }
}


static
CBGZFPos
s_GetOverlap(const pair<SBamIndexRefIndex::TBinsIter, SBamIndexRefIndex::TBinsIter>& iters)
{
    if ( iters.first == iters.second ) {
        return CBGZFPos::GetInvalid();
    }
    else {
        return iters.first->m_Overlap;
    }
}


static
CBGZFPos
s_GetFilePos(const pair<SBamIndexRefIndex::TBinsIter, SBamIndexRefIndex::TBinsIter>& iters)
{
    auto iter = iters.first;
    if ( iter == iters.second ) {
        return CBGZFPos::GetInvalid();
    }
    return iter->GetStartFilePos();
}


static
CBGZFPos
s_GetNextFilePos(const pair<SBamIndexRefIndex::TBinsIter, SBamIndexRefIndex::TBinsIter>& iters)
{
    auto iter = iters.first;
    if ( iter == iters.second ) {
        return CBGZFPos::GetInvalid();
    }
    ++iter;
    if ( iter == iters.second ) {
        return CBGZFPos::GetInvalid();
    }
    return iter->GetStartFilePos();
}

/*
static
CBGZFPos
s_GetFileEnd(const pair<SBamIndexRefIndex::TBinsIter, SBamIndexRefIndex::TBinsIter>& iters)
{
    if ( iters.first == iters.second ) {
        return CBGZFPos::GetInvalid();
    }
    else {
        return iters.first->GetEndFilePos();
    }
}
*/

NCBI_PARAM_DECL(int, BAM, OVERLAP_MODE);
NCBI_PARAM_DEF_EX(int, BAM, OVERLAP_MODE, 2, eParam_NoThread, BAM_OVERLAP_MODE);


static int s_GetOverlapMode()
{
    static int value = NCBI_PARAM_TYPE(BAM, OVERLAP_MODE)::GetDefault();
    return value;
}


vector<TSeqPos> SBamIndexRefIndex::GetAlnOverStarts() const
{
if ( s_GetOverlapMode() == 0 ) {
    TSeqPos nBins = m_EstimatedLength >> GetMinLevelBinShift();
    vector<TSeqPos> aln_over_starts(nBins);
    for ( TSeqPos i = 0; i < nBins; ++i ) {
        // set limits
        COpenRange<TSeqPos> ref_range;
        ref_range.SetFrom(i*GetMinBinSize()).SetLength(GetMinBinSize());
        CBGZFRange limit = GetLimitRange(ref_range, eSearchByOverlap);
        CBGZFPos min_fp = CBGZFPos::GetInvalid();
        for ( TIndexLevel level = 0; level <= GetMaxIndexLevel(); ++level ) {
            TBin bin = GetBinNumberBase(level) + (i>>(level*kLevelStepBinShift));
            auto it = lower_bound(m_Bins.begin(), m_Bins.end(), bin);
            if ( it != m_Bins.end() && it->m_Bin == bin ) {
                for ( auto c : it->m_Chunks ) {
                    if ( c.first >= min_fp ) {
                        break;
                    }
                    if ( c.first >= limit.second ) {
                        break;
                    }
                    if ( c.second <= limit.first ) {
                        continue;
                    }
                    if ( c.first < limit.first ) {
                        c.first = limit.first;
                    }
                    _ASSERT(c.first >= limit.first);
                    _ASSERT(c.first < limit.second);
                    _ASSERT(c.first < c.second);
                    if ( c.first < min_fp ) {
                        min_fp = c.first;
                    }
                    break;
                }
            }
        }
        TSeqPos min_aln_start;
        if ( min_fp.IsInvalid() ) {
            min_aln_start = ref_range.GetFrom();
        }
        else {
            min_aln_start = 0;
            for ( TIndexLevel level = 0; level <= GetMaxIndexLevel(); ++level ) {
                auto level_bins = GetLevelBins(level);
                auto it = lower_bound(level_bins.first, level_bins.second, min_fp, PByStartFilePos());
                if ( it == level_bins.first ) {
                    continue;
                }
                --it;
                min_aln_start = max(min_aln_start, it->GetSeqRange(*this).GetFrom());
                if ( it->GetEndFilePos() > min_fp ) {
                    // found exact bin containing the alignment
                    // since we start with the narrowest range there is no point to continue
                    break;
                }
            }
        }
        aln_over_starts[i] = min_aln_start;
    }
    return aln_over_starts;
}
else if ( s_GetOverlapMode() == 1 ) {
    size_t nBins = m_Overlaps.size();
    vector<TSeqPos> aln_over_starts(nBins);
    // next_bin_it points to a low-level bin that starts after current position
    auto bin_it_start = GetLevelBins(0).first, next_bin_it = bin_it_start;
    for ( size_t i = 0; i < nBins; ++i ) {
        TSeqPos ref_pos = TSeqPos(i * GetMinBinSize());
        CBGZFPos min_fp = m_Overlaps[i];
        if ( !min_fp ) {
            // no overspan
            aln_over_starts[i] = ref_pos;
            continue;
        }
        // update next_bin_it to point to the next bin after current refseq position
        while ( next_bin_it != m_Bins.end() && next_bin_it->GetStartFilePos() <= min_fp ) {
            ++next_bin_it;
        }
        TSeqPos min_aln_start = i? aln_over_starts[i-1]: 0;
        bool inside_min_bin = false;
        if ( next_bin_it != bin_it_start ) {
            auto& bin = next_bin_it[-1];
            _ASSERT(bin.GetStartFilePos() <= min_fp);
            inside_min_bin = bin.GetEndFilePos() > min_fp;
            min_aln_start = max(min_aln_start, (bin.m_Bin-GetBinNumberBase(0))*GetMinBinSize());
        }
        if ( min_aln_start+GetMinBinSize() < ref_pos && !inside_min_bin ) {
            // more than 1 page before -> lookup all levels for better estimate
            for ( TIndexLevel level = 1; level <= GetMaxIndexLevel(); ++level ) {
                auto level_bins = GetLevelBins(level);
                auto it = upper_bound(level_bins.first, level_bins.second, min_fp, PByStartFilePos());
                if ( it == level_bins.first ) {
                    continue;
                }
                --it;
                min_aln_start = max(min_aln_start, it->GetSeqRange(*this).GetFrom());
                if ( it->GetEndFilePos() > min_fp ) {
                    // found exact bin containing the alignment
                    // since we start with the narrowest range there is no point to continue
                    break;
                }
            }
        }
        if ( min_aln_start > ref_pos ) {
            NCBI_THROW_FMT(CBamException, eInvalidBAIFormat,
                           "Inconsistent linear index at ref pos "<<ref_pos<<
                           ": align starts after end bin start "<<min_aln_start);
        }
        aln_over_starts[i] = min_aln_start;
    }
    return aln_over_starts;
}
else {
    TSeqPos nBins = m_EstimatedLength >> GetMinLevelBinShift();
    vector<TSeqPos> aln_over_starts(nBins);
    vector<pair<TBinsIter, TBinsIter>> levelBins;
    vector<COpenRange<TSeqPos>> levelBinSeqRange;
    vector<CBGZFPos> levelPrevOverlap;
    if ( is_CSI ) {
        levelPrevOverlap.resize(GetMaxIndexLevel()+1);
    }
    CBGZFPos minfp = CBGZFPos::GetInvalid();
    for ( TIndexLevel level = 0; level <= GetMaxIndexLevel(); ++level ) {
        levelBins.push_back(GetLevelBins(level));
        levelBinSeqRange.push_back(s_GetSeqRange(*this, levelBins.back()));
        minfp = min(minfp, s_GetFilePos(levelBins.back()));
    }
    if ( minfp.IsInvalid() ) {
        // no file data -> no overlaps
        return aln_over_starts;
    }
    map<TSeqPos, CBGZFPos> sp2minfp; // map seqpos to the earliest filepos it could appear
    for ( auto& bin : m_Bins ) {
        auto sp = bin.GetSeqRange(*this).GetFrom();
        auto fp = bin.GetStartFilePos();
        auto ins = sp2minfp.insert(make_pair(sp, fp));
        if ( !ins.second ) {
            // uptade with minimum
            auto& minfp = ins.first->second;
            minfp = min(minfp, fp);
        }
    }
    map<CBGZFPos, TSeqPos> fp2sp; // map filepos to seqpos that certainly appear at or after
    for ( auto p : sp2minfp ) {
        auto ins = fp2sp.insert(make_pair(p.second, p.first));
        if ( ins.second ) {
            auto iter = ins.first;
            ++iter;
            while ( iter != fp2sp.end() && iter->second < p.first ) {
                iter = fp2sp.erase(iter);
            }
        }
    }
    for ( TSeqPos b = 0; b < nBins; ++b ) {
        TSeqPos seqPos = b << GetMinLevelBinShift();
        CBGZFPos overlap_fp = CBGZFPos::GetInvalid();
        if ( b < m_Overlaps.size() && m_Overlaps[b] ) { // BAI overlap table
            overlap_fp = m_Overlaps[b];
        }
        CBGZFPos prev_overlap_fp; // max overlap of previous bins on all levels
        for ( TIndexLevel level = 0; level <= GetMaxIndexLevel(); ++level ) {
            // advance to next bin on level if necessary
            while ( levelBinSeqRange[level].GetToOpen() <= seqPos ) {
                if ( is_CSI ) {
                    levelPrevOverlap[level] = s_GetOverlap(levelBins[level]);
                }
                ++(levelBins[level].first);
                levelBinSeqRange[level] = s_GetSeqRange(*this, levelBins[level]);
            }
            if ( is_CSI ) {
                CBGZFPos overlap_fp;
                if ( seqPos >= levelBinSeqRange[level].GetFrom() ) {
                    overlap_fp = s_GetOverlap(levelBins[level]);
                }
                else {
                    overlap_fp = levelPrevOverlap[level];
                }
                prev_overlap_fp = max(prev_overlap_fp, overlap_fp);
            }
        }        
        CBGZFPos found_fp = CBGZFPos::GetInvalid(); // earliest filepos of overlapping alignment
        CBGZFPos limit_fp = CBGZFPos::GetInvalid(); // filepos after this page to break the lookup
        for ( TIndexLevel level = 0; level <= GetMaxIndexLevel(); ++level ) {
            // advance to next bin on level if necessary
            while ( levelBinSeqRange[level].GetToOpen() <= seqPos ) {
                if ( is_CSI ) {
                    levelPrevOverlap[level] = s_GetOverlap(levelBins[level]);
                }
                ++(levelBins[level].first);
                levelBinSeqRange[level] = s_GetSeqRange(*this, levelBins[level]);
            }
            if ( seqPos < levelBinSeqRange[level].GetFrom() ) {
                // not in the bin yet
                continue;
            }
            if ( is_CSI && overlap_fp.IsInvalid() ) {
                // CSI overlap info from bin
                overlap_fp = max(prev_overlap_fp, levelBins[level].first->m_Overlap);
            }
            // update limit file pos from next bin on the level
            limit_fp = min(limit_fp, s_GetNextFilePos(levelBins[level]));
            // locate overlapping chunk
            auto& chunks = levelBins[level].first->m_Chunks;
            auto it = upper_bound(chunks.begin(), chunks.end(), overlap_fp, PByEndFilePos());
            if ( it != chunks.end() && it->first < min(found_fp, limit_fp) ) {
                // found suitable chunk
                found_fp = max(it->first, overlap_fp);
                if ( found_fp <= overlap_fp ) {
                    // found minimum, no more searching
                    break;
                }
            }
        }
        if ( found_fp.IsInvalid() ) {
            aln_over_starts[b] = seqPos;
        }
        else {
            // find minmal seq pos at this file pos
            auto iter = fp2sp.upper_bound(found_fp);
            _ASSERT(iter != fp2sp.begin());
            // it could be after current page
            auto osp = min(seqPos, prev(iter)->second);
            if ( b > 0 ) {
                // overlap may overshot in case of empty previous bins
                // that were explicitly marked as having no overlap
                osp = max(osp, aln_over_starts[b-1]);
            }
            aln_over_starts[b] = osp;
        }
    }
    return aln_over_starts;
}
}


vector<TSeqPos> SBamIndexRefIndex::GetAlnOverEnds() const
{
    TSeqPos bin_size = GetMinBinSize();
    vector<TSeqPos> starts = GetAlnOverStarts();
    TSeqPos count = TSeqPos(starts.size());
    vector<TSeqPos> ends(count);
    TSeqPos si = 0, ei = 0;
    for ( ; ei < count; ++ei ) {
        while ( si*bin_size < starts[ei] ) {
            ends[si++] = ei*bin_size-1;
        }
    }
    while ( si < count ) {
        ends[si++] = ei*bin_size-1;
    }
    return ends;
}


inline
Uint8 s_EstimatedPos(CBGZFPos pos)
{
    return pos.GetFileBlockPos() + Uint8(pos.GetByteOffset()*kEstimatedCompression);
}


inline
Uint8 s_EstimatedSize(CBGZFPos file_pos1, CBGZFPos file_pos2)
{
    if ( file_pos1 >= file_pos2 ) {
        // empty file region
        return 0;
    }
    Uint8 pos1 = s_EstimatedPos(file_pos1);
    Uint8 pos2 = s_EstimatedPos(file_pos2);
    if ( pos1 < pos2 )
        return pos2 - pos1;
    else
        return 1; // report non-zero size of non-empty region
}


inline
Uint8 s_EstimatedSize(const CBGZFRange& range)
{
    return s_EstimatedSize(range.first, range.second);
}


CBGZFRange SBamIndexRefIndex::GetLimitRange(COpenRange<TSeqPos>& ref_range,
                                            ESearchMode search_mode) const
{
    CBGZFRange limit;
    if ( m_EstimatedLength < ref_range.GetToOpen() ) {
        ref_range.SetToOpen(m_EstimatedLength);
    }
    if ( ref_range.Empty() ) {
        return limit;
    }

    if ( search_mode == eSearchByOverlap ) {
        if ( !m_Overlaps.empty() ) {
            TBin beg_bin_offset = GetBinNumberOffset(ref_range.GetFrom(), 0);
            // start limit is from intervals and beg position
            if ( beg_bin_offset < m_Overlaps.size() ) {
                limit.first = m_Overlaps[beg_bin_offset];
            }
        }
#ifdef BAM_SUPPORT_CSI
        else if ( is_CSI ) {
            for ( TIndexLevel level = 0; level <= GetMaxIndexLevel(); ++level ) {
                TBin bin_num = GetBinNumber(ref_range.GetFrom(), level);
                TBin bin_num_last = GetBinNumber(ref_range.GetTo(), level);
                auto bins = GetLevelBins(level);
                auto it = lower_bound(bins.first, bins.second, bin_num);
                if ( it != bins.second && it->m_Bin <= bin_num_last ) {
                    if ( it->m_Overlap ) {
                        if ( !limit.first || it->m_Overlap < limit.first ) {
                            limit.first = it->m_Overlap;
                        }
                    }
                    if ( it->m_Bin == bin_num ) {
                        break;
                    }
                }
            }
        }
#endif
    }
    else {
        // start limit is determined by alignment start position
        // for each level we'll take end position of previous existing bin
        for ( TIndexLevel level = 0; level <= GetMaxIndexLevel(); ++level ) {
            TBin bin_num = GetBinNumber(ref_range.GetFrom(), level);
            auto bins = GetLevelBins(level);
            auto it = lower_bound(bins.first, bins.second, bin_num);
            if ( it != bins.first ) {
                limit.first = max(limit.first, prev(it)->GetEndFilePos());
            }
        }
    }
    limit.second = CBGZFPos::GetInvalid();
    for ( TIndexLevel level = 0; level <= GetMaxIndexLevel(); ++level ) {
        // next bin on each level is clearly after the range
        TBin bin_num = GetBinNumber(ref_range.GetTo(), level)+1;
        auto bins = GetLevelBins(level);
        auto it = lower_bound(bins.first, bins.second, bin_num);
        if ( it != bins.second ) {
            limit.second = min(limit.second, it->GetStartFilePos());
        }
    }
    return limit;
}


pair<SBamIndexRefIndex::TBin, SBamIndexRefIndex::TBin>
SBamIndexParams::GetBinRange(COpenRange<TSeqPos> ref_range,
                             TIndexLevel index_level) const
{
    pair<TBin, TBin> bin_range;
    bin_range.first = GetBinNumber(ref_range.GetFrom(), index_level);
    if ( IsOverflowBin(bin_range.first, index_level) ) {
        // position is beyond index limit (can happen with BAI index)
        // only min and max levels exist, and max level is always root bin
        if ( index_level == GetMaxIndexLevel() ) {
            bin_range.first = kMaxBinNumber;
            bin_range.second = kMaxBinNumber;
            return bin_range;
        }
        else if ( index_level != kMinBinIndexLevel ) {
            // start bin is neither min nor max level - no bins to scan
            bin_range.second = bin_range.first-1;
            return bin_range;
        }
    }
    bin_range.second = GetBinNumber(ref_range.GetTo(), index_level);
    if ( IsOverflowBin(bin_range.second, index_level) ) {
        // position is beyond index limit (can happen with BAI index)
        // only min and max levels exist, and max level is always root bin
        if ( index_level == GetMaxIndexLevel() ) {
            bin_range.second = kMaxBinNumber;
        }
        else if ( index_level != kMinBinIndexLevel ) {
            // end bin is neither min nor max level - scan to the end of bins of the level
            bin_range.second = GetLastBin(index_level);
        }
    }
    return bin_range;
}


pair<SBamIndexRefIndex::TBinsIter, SBamIndexRefIndex::TBinsIter>
SBamIndexRefIndex::AddLevelFileRanges(vector<CBGZFRange>& ranges,
                                      CBGZFRange limit_file_range,
                                      pair<TBin, TBin> bin_range) const
{
    TBinsIter first = lower_bound(m_Bins.begin(), m_Bins.end(), bin_range.first);
    TBinsIter it = first;
    for ( ; it != m_Bins.end() && it->m_Bin <= bin_range.second; ++it ) {
        for ( auto c : it->m_Chunks ) {
            if ( c.first < limit_file_range.first ) {
                c.first = limit_file_range.first;
            }
            if ( limit_file_range.second && limit_file_range.second < c.second ) {
                c.second = limit_file_range.second;
            }
            if ( c.first < c.second ) {
                ranges.push_back(c);
            }
        }
    }
    return make_pair(first, it);
}


pair<SBamIndexRefIndex::TBinsIter, SBamIndexRefIndex::TBinsIter>
SBamIndexRefIndex::GetBinsIterRange(pair<TBin, TBin> bin_range) const
{
    TBinsIter first = lower_bound(m_Bins.begin(), m_Bins.end(), bin_range.first);
    TBinsIter it = upper_bound(first, m_Bins.end(), bin_range.second);
    return make_pair(first, it);
}


struct SBamRangeBlock {
    size_t block_beg, block_end; // range of low-level pages
    size_t fill_beg_to, fill_end_to; // uncertainty about start and end positions
    CBGZFPos file_beg, file_end; // included BAM file range

    static
    void x_AddDataSize(vector<Uint8>& vv, size_t beg_pos, size_t end_pos,
                       CBGZFPos file_beg, CBGZFPos file_end)
        {
            _ASSERT(beg_pos < vv.size());
            _ASSERT(beg_pos <= end_pos);
            _ASSERT(end_pos < vv.size());
            Uint8 file_size = s_EstimatedSize(file_beg, file_end);
            if ( !file_size ) {
                return;
            }
            size_t page_count = end_pos - beg_pos + 1;
            Uint8 add_size = (file_size + page_count/2) / page_count;
            if ( add_size ) {
                for ( size_t i = beg_pos; i <= end_pos; ++i ) {
                    vv[i] += add_size;
                }
            }
            else {
                // rounding produced zero, but the original data size was non-zero,
                // so make resulting esimated sizes at least non-zero
                for ( size_t i = beg_pos; i <= end_pos; ++i ) {
                    if ( !vv[i] ) {
                        vv[i] = 1;
                    }
                }
            }
        }
    
    void Init(size_t index)
        {
            block_beg = block_end = index;
        }

    void InitData(vector<Uint8>& vv, const SBamIndexBinInfo& bin)
        {
            if ( bin.m_Chunks.empty() ) {
                return;
            }
            size_t i = block_beg;
            _ASSERT(block_end == i);
            _ASSERT(!file_end);
            fill_beg_to = fill_end_to = i;
            file_beg = bin.GetStartFilePos();
            file_end = bin.GetEndFilePos();
            _ASSERT(file_beg < file_end);
            vv[i] = s_EstimatedSize(file_beg, file_end);
        }
    void ExpandData(vector<Uint8>& vv, const SBamIndexBinInfo& bin)
        {
            if ( bin.m_Chunks.empty() ) {
                return;
            }
            CBGZFPos new_file_beg = bin.GetStartFilePos();
            CBGZFPos new_file_end = bin.GetEndFilePos();
            _ASSERT(new_file_beg < new_file_end);
            if ( !file_end ) {
                // start BAM file range
                x_AddDataSize(vv, block_beg, block_end, new_file_beg, new_file_end);
                file_beg = new_file_beg;
                file_end = new_file_end;
                // pages are completely uncertain
                fill_beg_to = block_end; // beg/end cross assignment is intentional
                fill_end_to = block_beg; // beg/end cross assignment is intentional
            }
            else {
                // expand BAM file range
                if ( new_file_beg < file_beg ) {
                    x_AddDataSize(vv, block_beg, fill_beg_to, new_file_beg, file_beg);
                    file_beg = new_file_beg;
                }
                if ( new_file_end > file_end ) {
                    x_AddDataSize(vv, fill_end_to, block_end, file_end, new_file_end);
                    file_end = new_file_end;
                }
            }
        }
    
    SBamRangeBlock()
        {
        }
    SBamRangeBlock(vector<Uint8>& vv,
                   const vector<SBamRangeBlock>& bb, size_t bb_beg, size_t bb_end)
        {
            for ( size_t i = bb_beg; i <= bb_end; ++i ) {
                const SBamRangeBlock& b = bb[i];
                if ( !b.file_end ) {
                    continue;
                }
                if ( !file_end ) {
                    // start BAM file range
                    *this = b;
                }
                else {
                    // include gap
                    _ASSERT(file_end <= b.file_beg);
                    x_AddDataSize(vv, fill_end_to, b.fill_beg_to, file_end, b.file_beg);
                    fill_end_to = b.fill_end_to;
                    file_end = b.file_end;
                }
            }
            block_beg = bb[bb_beg].block_beg;
            block_end = bb[bb_end].block_end;
        }
};


CBGZFRange SBamIndexRefIndex::GetFileRange() const
{
    CBGZFRange range;
    range.first = CBGZFPos::GetInvalid();
    for ( TIndexLevel level = 0; level <= GetMaxIndexLevel(); ++level ) {
        auto bins = GetLevelBins(level);
        if ( bins.first != bins.second ) {
            CBGZFPos pos_beg = bins.first->GetStartFilePos();
            CBGZFPos pos_end = prev(bins.second)->GetEndFilePos();
            if ( pos_beg < range.first ) {
                range.first = pos_beg;
            }
            if ( pos_end > range.second ) {
                range.second = pos_end;
            }
        }
    }
    if ( range.first.IsInvalid() ) {
        range.first = CBGZFPos();
    }
    return range;
}


vector<Uint8> SBamIndexRefIndex::EstimateDataSizeByAlnStartPos(TSeqPos seqlen) const
{
    size_t bin_count;
    if ( seqlen == kInvalidSeqPos ) {
        seqlen = m_EstimatedLength;
    }
    else {
        seqlen = max(seqlen, m_EstimatedLength);
    }
    bin_count = (seqlen+GetMinBinSize()-1) >> GetMinLevelBinShift();
    _ASSERT(bin_count);
    vector<Uint8> vv(bin_count);
    // init blocks
    vector<SBamRangeBlock> bb(bin_count);
    size_t bb_end = bin_count-1;
    for ( size_t i = 0; i <= bb_end; ++i ) {
        bb[i].Init(i);
    }
    // fill smallest bins
    {
        TBin bin_number_base = GetBinNumberBase(0);
        auto level_bins = GetLevelBins(0);
        for ( auto bin_it = level_bins.first; bin_it != level_bins.second; ++bin_it ) {
            size_t i = bin_it->m_Bin - bin_number_base;
            _ASSERT(i <= bb_end);
            bb[i].InitData(vv, *bin_it);
        }
    }
    for ( TIndexLevel level = 1; level <= GetMaxIndexLevel(); ++level ) {
        
        // merge
        for ( size_t i = 0; (i<<kLevelStepBinShift) <= bb_end; ++i ) {
            size_t src_beg = i<<kLevelStepBinShift;
            size_t src_end = min(bb_end, src_beg+(1<<kLevelStepBinShift)-1);
            bb[i] = SBamRangeBlock(vv, bb, src_beg, src_end);
        }
        bb_end >>= kLevelStepBinShift;
        
        // add next level bins
        TBin bin_number_base = GetBinNumberBase(level);
        auto level_bins = GetLevelBins(level);
        for ( auto bin_it = level_bins.first; bin_it != level_bins.second; ++bin_it ) {
            size_t i = bin_it->m_Bin - bin_number_base;
            _ASSERT(i <= bb_end);
            bb[i].ExpandData(vv, *bin_it);
        }
    }
    _ASSERT(bb_end == 0);
    return vv;
}


vector<uint64_t> SBamIndexRefIndex::CollectEstimatedCoverage(TIndexLevel min_index_level,
                                                             TIndexLevel max_index_level) const
{
    vector<uint64_t> vv(((m_EstimatedLength-GetMinBinSize()) >> GetLevelBinShift(min_index_level))+1);
    for ( TIndexLevel level = min_index_level; level <= max_index_level; ++level ) {
        uint32_t vv_bin_shift = (level-min_index_level)*kLevelStepBinShift;
        uint32_t vv_bin_count = 1 << vv_bin_shift;
        auto level_bins = GetLevelBins(level);
        TBin bin_base = GetBinNumberBase(level);
        for ( auto it = level_bins.first; it != level_bins.second; ++it ) {
            uint64_t value = 0;
            for ( auto& c : it->m_Chunks ) {
                value += s_EstimatedSize(c);
            }
            if ( !value ) {
                continue;
            }
            uint32_t pos = (it->m_Bin - bin_base) << vv_bin_shift;
            _ASSERT(pos < vv.size());
            uint64_t add = value;
            uint32_t cnt = min(vv_bin_count, uint32_t(vv.size()-pos));
            if ( cnt > 1 ) {
                // distribute
                add = (add+cnt/2)/cnt;
            }
            if ( !add ) {
                for ( uint32_t i = 0; i < cnt; ++i ) {
                    vv[pos+i] = max(uint64_t(1), vv[pos+i]);
                }
            }
            else {
                for ( uint32_t i = 0; i < cnt; ++i ) {
                    vv[pos+i] += add;
                }
            }
        }
    }
    return vv;
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
    : m_UnmappedCount(0),
      m_TotalReadBytes(0),
      m_TotalReadSeconds(0)
{
}


CBamIndex::CBamIndex(const string& index_file_name)
    : m_UnmappedCount(0),
      m_TotalReadBytes(0),
      m_TotalReadSeconds(0)
{
    Read(index_file_name);
}


CBamIndex::~CBamIndex()
{
}


void CBamIndex::Read(const string& index_file_name)
{
    m_FileName = index_file_name;
    m_Refs.clear();
    m_UnmappedCount = 0;

    AutoArray<char> data;
    CStopWatch sw(CStopWatch::eStart);
    size_t size = ReadVDBFile(data, index_file_name);
    m_TotalReadBytes = size;
    m_TotalReadSeconds = sw.Elapsed();
    if ( CBamDb::GetDebugLevel() >= 3 ) {
        LOG_POST("BAM: read index "<<size/double(1<<20)<<" MB"
                 " speed: "<<size/(m_TotalReadSeconds*(1<<20))<<" MB/s");
    }
    Read(data.get(), size);
}

BEGIN_LOCAL_NAMESPACE;
class CMemoryReader : public IReader
{
public:
    CMemoryReader(const char* ptr, size_t size)
        : m_Ptr(ptr),
          m_Size(size)
        {
        }
    
    ERW_Result Read(void*   buf,
                    size_t  count,
                    size_t* bytes_read)
        {
            if ( !m_Size ) {
                if ( bytes_read ) {
                    *bytes_read = 0;
                }
                return eRW_Eof;
            }
            count = min(m_Size, count);
            memcpy(buf, m_Ptr, count);
            m_Ptr += count;
            m_Size -= count;
            if ( bytes_read ) {
                *bytes_read = count;
            }
            return eRW_Success;
        }
    
    ERW_Result PendingCount(size_t* count)
        {
            *count = m_Size;
            return eRW_Success;
        }
    
private:
    const char* m_Ptr;
    size_t m_Size;
};
END_LOCAL_NAMESPACE;

void CBamIndex::Read(CNcbiIstream& in)
{
#ifdef BAM_SUPPORT_CSI
    is_CSI = false;
    min_shift = kBAI_min_shift;
    depth = kBAI_depth;
#endif

    char magic[kIndexMagicLength];
    s_Read(in, magic, kIndexMagicLength);
    if ( memcmp(magic, kIndexMagicBAI, kIndexMagicLength) == 0 ) {
        // BAI, no extra parameters
    }
#ifdef BAM_SUPPORT_CSI
    else if ( memcmp(magic, kIndexMagicCSI, kIndexMagicLength) == 0 ) {
        // CSI
        is_CSI = true;
        min_shift = s_ReadUInt32(in);
        depth = s_ReadUInt32(in);
        size_t l_aux = s_ReadUInt32(in);
        while ( l_aux ) {
            char buf[256];
            size_t count = min(l_aux, sizeof(buf));
            s_Read(in, buf, count);
            l_aux -= count;
        }
    }
#endif
    else {
        NCBI_THROW_FMT(CBamException, eInvalidBAIFormat,
                       "Bad file magic: "<<NStr::PrintableString(string(magic, magic+kIndexMagicLength)));
    }
    int32_t n_ref = s_ReadInt32(in);
    m_Refs.resize(n_ref);
    for ( int32_t i_ref = 0; i_ref < n_ref; ++i_ref ) {
        m_Refs[i_ref].Read(in, *this, i_ref);
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
        ERR_POST(Warning<<
                 "Extra "<<(end_pos-extra_pos)<<" bytes in BAM index");
    }
}


void CBamIndex::Read(const char* buffer_ptr, size_t buffer_size)
{
    if ( buffer_size >= kGZipMagicLength &&
         memcmp(buffer_ptr, kGZipMagic, kGZipMagicLength) == 0 ) {
        // gzipped index
        unique_ptr<CNcbiIstream> data_stream =
            make_unique<CRStream>(new CMemoryReader(buffer_ptr, buffer_size),
                                  0, nullptr, CRWStreambuf::fOwnReader);
        unique_ptr<CNcbiIstream> z_stream =
            make_unique<CCompressionIStream>(*data_stream,
                                             new CZipStreamDecompressor(CZipCompression::fGZip),
                                             CCompressionIStream::fOwnProcessor);
        Read(*z_stream);
        return;
    }
    
    const char* buffer_end = buffer_ptr + buffer_size;

#ifdef BAM_SUPPORT_CSI
    is_CSI = false;
    min_shift = kBAI_min_shift;
    depth = kBAI_depth;
#endif
    
    const char* magic = s_Read(buffer_ptr, buffer_end, kIndexMagicLength);
    if ( memcmp(magic, kIndexMagicBAI, kIndexMagicLength) == 0 ) {
        // BAI
    }
#ifdef BAM_SUPPORT_CSI
    else if ( memcmp(magic, kIndexMagicCSI, kIndexMagicLength) == 0 ) {
        // CSI
        is_CSI = true;
        const char* header = s_Read(buffer_ptr, buffer_end, 12);
        min_shift = SBamUtil::MakeUint4(header);
        depth = SBamUtil::MakeUint4(header+4);
        auto l_aux = SBamUtil::MakeUint4(header+8);
        s_Read(buffer_ptr, buffer_end, l_aux);
    }
#endif
    else {
        NCBI_THROW_FMT(CBamException, eInvalidBAIFormat,
                       "Bad file magic: "<<NStr::PrintableString(string(magic, magic+kIndexMagicLength)));
    }
    const char* header = s_Read(buffer_ptr, buffer_end, 4);
    uint32_t n_ref = SBamUtil::MakeUint4(header);
    m_Refs.resize(n_ref);
    for ( uint32_t i = 0; i < n_ref; ++i ) {
        buffer_ptr = m_Refs[i].Read(buffer_ptr, buffer_end, *this, i);
    }
    if ( buffer_end - buffer_ptr >= 8 ) {
        m_UnmappedCount = SBamUtil::MakeUint8(buffer_ptr);
        buffer_ptr += 8;
    }
    if ( buffer_ptr != buffer_end ) {
        ERR_POST(Warning<<
                 "Extra "<<(buffer_end-buffer_ptr)<<" bytes in BAM index");
    }
}


const SBamIndexRefIndex& CBamIndex::GetRef(size_t ref_index) const
{
    if ( ref_index >= GetRefCount() ) {
        NCBI_THROW(CBamException, eInvalidArg,
                   "Bad reference sequence index");
    }
    return m_Refs[ref_index];
}


void CBamIndex::SetLengthFromHeader(const CBamHeader& header)
{
    if ( GetRefCount() != header.GetRefCount() ) {
        NCBI_THROW_FMT(CBamException, eInvalidBAIFormat,
                       "Wrong index ref count: "<<
                       GetRefCount()<<" <> "<<header.GetRefCount());
    }
    for ( size_t i = 0; i < GetRefCount(); ++i ) {
        m_Refs[i].SetLengthFromHeader(header.GetRef(i).m_Length);
    }
}


CBGZFRange CBamIndex::GetTotalFileRange(size_t ref_index) const
{
    CBGZFRange total_range(CBGZFPos(-1), CBGZFPos(0));
    for ( auto& b : GetRef(ref_index).m_Bins ) {
        CBGZFPos start_pos = b.GetStartFilePos();
        if ( start_pos < total_range.first )
            total_range.first = start_pos;
        CBGZFPos end_pos = b.GetEndFilePos();
        if ( total_range.second < end_pos )
            total_range.second = end_pos;
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
                                      const string& annot_name,
                                      TIndexLevel min_index_level,
                                      TIndexLevel max_index_level) const
{
    CSeq_id id(seq_id);
    return MakeEstimatedCoverageAnnot(header, ref_name, id, annot_name, min_index_level, max_index_level);
}


CRef<CSeq_annot>
CBamIndex::MakeEstimatedCoverageAnnot(const CBamHeader& header,
                                      const string& ref_name,
                                      const CSeq_id& seq_id,
                                      const string& annot_name,
                                      TIndexLevel min_index_level,
                                      TIndexLevel max_index_level) const
{
    size_t ref_index = header.GetRefIndex(ref_name);
    if ( ref_index == size_t(-1) ) {
        NCBI_THROW_FMT(CBamException, eInvalidArg,
                       "Cannot find RefSeq: "<<ref_name);
    }
    return MakeEstimatedCoverageAnnot(ref_index, seq_id, annot_name,
                                      header.GetRefLength(ref_index), min_index_level, max_index_level);
}


CRef<CSeq_annot>
CBamIndex::MakeEstimatedCoverageAnnot(size_t ref_index,
                                      const string& seq_id,
                                      const string& annot_name,
                                      TSeqPos length,
                                      TIndexLevel min_index_level,
                                      TIndexLevel max_index_level) const
{
    CSeq_id id(seq_id);
    return MakeEstimatedCoverageAnnot(ref_index, id, annot_name, length, min_index_level, max_index_level);
}


CRef<CSeq_annot>
CBamIndex::MakeEstimatedCoverageAnnot(size_t ref_index,
                                      const string& seq_id,
                                      const string& annot_name,
                                      TIndexLevel min_index_level,
                                      TIndexLevel max_index_level) const
{
    return MakeEstimatedCoverageAnnot(ref_index, seq_id, annot_name, kInvalidSeqPos, min_index_level, max_index_level);
}


CRef<CSeq_annot>
CBamIndex::MakeEstimatedCoverageAnnot(size_t ref_index,
                                      const CSeq_id& seq_id,
                                      const string& annot_name,
                                      TIndexLevel min_index_level,
                                      TIndexLevel max_index_level) const
{
    return MakeEstimatedCoverageAnnot(ref_index, seq_id, annot_name, kInvalidSeqPos, min_index_level, max_index_level);
}


vector<uint64_t>
CBamIndex::CollectEstimatedCoverage(size_t ref_index,
                                    TIndexLevel min_index_level,
                                    TIndexLevel max_index_level) const
{
    return GetRef(ref_index).CollectEstimatedCoverage(min_index_level, max_index_level);
}


CRef<CSeq_annot>
CBamIndex::MakeEstimatedCoverageAnnot(size_t ref_index,
                                      const CSeq_id& seq_id,
                                      const string& annot_name,
                                      TSeqPos length,
                                      TIndexLevel min_index_level,
                                      TIndexLevel max_index_level) const
{
    TSeqPos bin_size = GetBinSize(min_index_level);
    vector<uint64_t> vv = CollectEstimatedCoverage(ref_index, min_index_level, max_index_level);
    if ( vv.empty() ) vv.push_back(0);
    uint32_t count = uint32_t(vv.size());
    if ( length == 0 || length == kInvalidSeqPos ) {
        length = count*bin_size;
    }

    CRef<CSeq_annot> annot(new CSeq_annot);
    CRef<CSeq_graph> graph(new CSeq_graph);
    annot->SetData().SetGraph().push_back(graph);
    sx_SetTitle(*graph, *annot, annot_name, annot_name);

    graph->SetLoc().SetInt().SetId().Assign(seq_id);
    graph->SetLoc().SetInt().SetFrom(0);
    graph->SetLoc().SetInt().SetTo(length-1);
    graph->SetComp(bin_size);
    graph->SetNumval(count);
    CByte_graph& bgraph = graph->SetGraph().SetByte();
    CByte_graph::TValues& bvalues = bgraph.SetValues();
    bvalues.resize(count);
    Uint1 bmax = 0;
    uint64_t max_value = *max_element(vv.begin(), vv.end());
    double mul = min(1., 255./max_value);
    for ( size_t i = 0; i < count; ++i ) {
        if ( auto v = vv[i] ) {
            Uint1 b = Uint1(v*mul+.5);
            // ensure non-zero value be still non-zero after scaling
            if ( !b ) {
                b = 1;
            }
            bvalues[i] = b;
            bmax = max(bmax, b);
        }
    }
    bgraph.SetAxis(0);
    bgraph.SetMin(1);
    bgraph.SetMax(bmax);
    if ( mul != 1 ) {
        graph->SetA(1/mul);
    }
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


CBamHeader::~CBamHeader()
{
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
    CBGZFFile file(bam_file_name);
    CBGZFStream file_stream(file);
    Read(file_stream);
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


const SBamHeaderRefInfo& CBamHeader::GetRef(size_t ref_index) const
{
    if ( ref_index >= GetRefCount() ) {
        NCBI_THROW(CBamException, eInvalidArg,
                   "Bad reference sequence index");
    }
    return m_Refs[ref_index];
}


size_t CBamHeader::GetRefIndex(const string& name) const
{
    auto iter = m_RefByName.find(name);
    if ( iter == m_RefByName.end() ) {
        return size_t(-1);
    }
    return iter->second;
}

size_t CBamHeader::GetSBamRecords(TSBamRecords& records) const
{
    CTempString record;
    enum { eNone, eTag, eRecord, eValue} state = eNone;
    bool state_changed = true;
    const char *p, *p0, *pend;

    for (p = m_Text.data(), pend = p + m_Text.size(); p < pend; ++p) {
        if (state_changed) {
            state_changed = false;
            for (; p < pend && iswspace(*p); ++p)
                ;
            p0 = p;
        }
        if (*p == '@') {
            state = eTag;
            p0 = p;
        }
        else if (*p == ':') {
            if (state == eRecord) {
                record.assign(p0, p-p0);
                state = eValue;
                state_changed = true;
                p0 = p;
            }
        }
        else if ( iswspace(*p) ) {
            if (state == eTag) {
                records.push_back( TSBamRecord(string(p0, p-p0), TSBamTags()));
                state = eRecord;
                state_changed = true;
            }
            else if (state == eValue) {
                records.back().second[record] = string(p0, p-p0);
                state = eRecord;
                state_changed = true;
            }
        }
    }
    if (state == eValue) {
        records.back().second[record] = string(p0, p-p0);
    }
    return records.size();
}


/////////////////////////////////////////////////////////////////////////////
// CBamFileRangeSet
/////////////////////////////////////////////////////////////////////////////


CBamFileRangeSet::CBamFileRangeSet()
{
}


CBamFileRangeSet::CBamFileRangeSet(const CBamIndex& index,
                                   size_t ref_index,
                                   COpenRange<TSeqPos> ref_range,
                                   ESearchMode search_mode)
{
    AddRanges(index, ref_index, ref_range, search_mode);
}


CBamFileRangeSet::CBamFileRangeSet(const CBamIndex& index,
                                   size_t ref_index,
                                   COpenRange<TSeqPos> ref_range,
                                   TIndexLevel min_level, TIndexLevel max_level,
                                   ESearchMode search_mode)
{
    AddRanges(index, ref_index, ref_range, min_level, max_level, search_mode);
}


CBamFileRangeSet::CBamFileRangeSet(const CBamIndex& index,
                                   size_t ref_index,
                                   COpenRange<TSeqPos> ref_range,
                                   EIndexLevel min_level, EIndexLevel max_level,
                                   ESearchMode search_mode)
{
    AddRanges(index, ref_index, ref_range, min_level, max_level, search_mode);
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


void CBamFileRangeSet::Clear()
{
    m_Ranges.clear();
}


inline
void CBamFileRangeSet::AddSortedRanges(const vector<CBGZFRange>& ranges,
                                       const CBGZFPos* file_pos)
{
    CBGZFPos min_pos = file_pos? *file_pos: CBGZFPos();
    for ( auto iter = ranges.begin(); iter != ranges.end(); ) {
        CBGZFPos start = iter->first, end = iter->second;
        for ( ++iter; iter != ranges.end() && !(end < iter->first); ++iter ) {
            if ( end < iter->second ) {
                end = iter->second;
            }
        }
        if ( start < min_pos ) {
            // the range starts before the requested starting file position
            if ( end <= min_pos ) {
                // the range is fully before the starting file position - skip it
                continue;
            }
            // otherwise trim it
            start = min_pos;
        }
        m_Ranges += CBGZFRange(start, end);
    }
}


NCBI_PARAM_DECL(int, BAM, RANGES_MODE);
NCBI_PARAM_DEF_EX(int, BAM, RANGES_MODE, 1, eParam_NoThread, BAM_RANGES_MODE);


static int s_GetRangesMode()
{
    static int value = NCBI_PARAM_TYPE(BAM, RANGES_MODE)::GetDefault();
    return value;
}


void CBamFileRangeSet::AddRanges(const CBamIndex& index,
                                 size_t ref_index,
                                 COpenRange<TSeqPos> ref_range,
                                 TIndexLevel min_index_level,
                                 TIndexLevel max_index_level,
                                 ESearchMode search_mode,
                                 const CBGZFPos* file_pos)
{
    vector<CBGZFRange> ranges;
    const SBamIndexRefIndex& ref = index.GetRef(ref_index);
    if ( s_GetRangesMode() == 0 ) {
        // set limits
        CBGZFRange limit = ref.GetLimitRange(ref_range, search_mode);
        if ( ref_range.Empty() ) {
            return;
        }
        for ( TIndexLevel level = min_index_level; level <= index.GetMaxIndexLevel(); ++level ) {
            ref.AddLevelFileRanges(ranges, limit, index.GetBinRange(ref_range, level));
        }
    }
    else {
        CBGZFRange limit(CBGZFPos(), CBGZFPos::GetInvalid());
        // iterate index levels starting with 0 to set limits correctly
        // iterate index levels till the end because alignments may be moved up
        TSeqPos set_limit_by_overlap_at = 0;
        for ( TIndexLevel level = 0; level <= index.GetMaxIndexLevel(); ++level ) {
            // omit ranges from lower index levels because they contain only low-level alignments
            auto bin_range = index.GetBinRange(ref_range, level);
            pair<SBamIndexRefIndex::TBinsIter, SBamIndexRefIndex::TBinsIter> iter_range;
            if ( level >= min_index_level ) {
                iter_range = ref.AddLevelFileRanges(ranges, limit, bin_range);
                _ASSERT(iter_range == ref.GetBinsIterRange(bin_range));
            }
            else {
                iter_range = ref.GetBinsIterRange(bin_range);
            }
            // set file range limit from overlap fields
            // the most limiting overlap is on the lowest existing index level, so set it once
            // this limit is valid for both search modes
            if ( index.is_CSI ) {
                // CSI overlaps are in bins
                auto first_bin = iter_range.first;
                if ( (first_bin == ref.m_Bins.end() ||
                      first_bin->m_Bin != bin_range.first) &&
                     first_bin != ref.m_Bins.begin() ) {
                    --first_bin;
                }
                if ( first_bin != ref.m_Bins.end() &&
                     first_bin->m_Bin <= bin_range.first &&
                     first_bin->m_Bin >= index.GetFirstBin(level) ) {
                    // the bin is at or before the first one and at the same level
                    TSeqPos pos = first_bin->GetSeqRange(index).GetFrom();
                    if ( pos > set_limit_by_overlap_at ) {
                        // better limit
                        set_limit_by_overlap_at = pos;
                        limit.first = max(limit.first, first_bin->m_Overlap);
                    }
                }
            }
            else {
                // BAI overlaps are in a separate array, reflecting min level
                if ( level == kMinBinIndexLevel && !ref.m_Overlaps.empty() ) {
                    size_t bin_index = bin_range.first-index.GetFirstBin(kMinBinIndexLevel);
                    if ( bin_index < ref.m_Overlaps.size() ) {
                        limit.first = max(limit.first, ref.m_Overlaps[bin_index]);
                    }
                }
            }
            // in eSearchByStart mode we can set lower limit of file positions
            // from the end of previous bin on the same level
            // these limits are combined
            if ( search_mode == eSearchByStart ) {
                // set file range limit from previous bins
                // limits from all levels matter, choose the most limiting one
                auto first_bin = iter_range.first;
                if ( first_bin != ref.m_Bins.begin() ) {
                    auto prev_bin = prev(first_bin);
                    _ASSERT(prev_bin->m_Bin < bin_range.first);
                    if ( prev_bin->m_Bin >= index.GetFirstBin(level) ) {
                        // prev bin is on the same level
                        limit.first = max(limit.first, prev_bin->GetEndFilePos());
                    }
                }
            }
            // in all search modes we can limit end of search range by next bin on the same level
            // update cutoff file pos from the first next bin
            auto next_bin = iter_range.second;
            if ( next_bin != ref.m_Bins.end() &&
                 next_bin->m_Bin < index.GetFirstBin(level-1) ) {
                // next bin is on the same level
                limit.second = min(limit.second, next_bin->GetStartFilePos());
            } 
        }
    }
    gfx::timsort(ranges.begin(), ranges.end());
    AddSortedRanges(ranges, file_pos);
}


void CBamFileRangeSet::AddRanges(const CBamIndex& index,
                                 size_t ref_index,
                                 COpenRange<TSeqPos> ref_range,
                                 ESearchMode search_mode,
                                 const CBGZFPos* file_pos)
{
    AddRanges(index, ref_index, ref_range, 0, index.GetMaxIndexLevel(), search_mode, file_pos);
}


void CBamFileRangeSet::AddRanges(const CBamIndex& index,
                                 size_t ref_index,
                                 COpenRange<TSeqPos> ref_range,
                                 TIndexLevel index_level,
                                 ESearchMode search_mode,
                                 const CBGZFPos* file_pos)
{
    AddRanges(index, ref_index, ref_range, index_level, index_level, search_mode, file_pos);
}


void CBamFileRangeSet::AddFrom(CBGZFPos file_pos)
{
    CBGZFRange whole;
    whole.first = file_pos;
    whole.second = CBGZFPos::GetInvalid();
    m_Ranges += whole;
}


void CBamFileRangeSet::AddWhole(const CBamHeader& header)
{
    AddFrom(header.GetAlignStart());
}


void CBamFileRangeSet::AddFrom(const CBamHeader& header, const CBGZFPos* file_pos)
{
    if ( file_pos && *file_pos ) {
        AddFrom(*file_pos);
    }
    else {
        AddWhole(header);
    }
}


void CBamFileRangeSet::SetRanges(const CBamIndex& index,
                                 size_t ref_index,
                                 COpenRange<TSeqPos> ref_range,
                                 TIndexLevel min_index_level,
                                 TIndexLevel max_index_level,
                                 ESearchMode search_mode,
                                 const CBGZFPos* file_pos)
{
    Clear();
    AddRanges(index, ref_index, ref_range, min_index_level, max_index_level, search_mode, file_pos);
}


void CBamFileRangeSet::SetRanges(const CBamIndex& index,
                                 size_t ref_index,
                                 COpenRange<TSeqPos> ref_range,
                                 ESearchMode search_mode,
                                 const CBGZFPos* file_pos)
{
    SetRanges(index, ref_index, ref_range, 0, index.GetMaxIndexLevel(), search_mode, file_pos);
}


void CBamFileRangeSet::SetRanges(const CBamIndex& index,
                                 size_t ref_index,
                                 COpenRange<TSeqPos> ref_range,
                                 TIndexLevel index_level,
                                 ESearchMode search_mode,
                                 const CBGZFPos* file_pos)
{
    SetRanges(index, ref_index, ref_range, index_level, index_level, search_mode, file_pos);
}


Uint8 CBamFileRangeSet::GetFileSize(CBGZFRange range)
{
    return s_EstimatedSize(range);
}


Uint8 CBamFileRangeSet::GetFileSize() const
{
    Uint8 size = 0;
    for ( auto& c : m_Ranges ) {
        size += GetFileSize(c);
    }
    return size;
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


static void s_AddReplacedExt(vector<string>& dst,
                             const string& base_name,
                             CTempString old_ext,
                             CTempString new_ext)
{
    if ( NStr::EndsWith(base_name, old_ext) ) {
        dst.push_back(base_name.substr(0, base_name.size()-old_ext.size())+new_ext);
    }
}


void CBamRawDb::Open(const string& bam_path, const string& index_path)
{
    vector<string> index_name_candidates;
    if ( index_path.empty() || index_path == bam_path ) {
#ifdef BAM_SUPPORT_CSI
        bool prefer_csi = NCBI_PARAM_TYPE(BAM, PREFER_CSI)::GetDefault();
        if ( prefer_csi ) {
            index_name_candidates.push_back(bam_path+kCsiExt);
            s_AddReplacedExt(index_name_candidates, bam_path, kBamExt, kCsiExt);
        }
#endif
        index_name_candidates.push_back(bam_path+kBaiExt);
        s_AddReplacedExt(index_name_candidates, bam_path, kBamExt, kBaiExt);
#ifdef BAM_SUPPORT_CSI
        if ( !prefer_csi ) {
            index_name_candidates.push_back(bam_path+kCsiExt);
            s_AddReplacedExt(index_name_candidates, bam_path, kBamExt, kCsiExt);
        }
#endif
    }
    else {
        index_name_candidates.push_back(index_path);
    }
    for ( size_t i = 0; i < index_name_candidates.size(); ++i ) {
        try {
            m_Index.Read(index_name_candidates[i]);
            break;
        }
        catch ( CBamException& exc ) {
            if ( i < index_name_candidates.size()-1 &&
                 exc.GetErrCode() == CBamException::eFileNotFound ) {
                // try next index file name candidate
                continue;
            }
            else {
                throw;
            }
        }
    }
    m_File = new CBGZFFile(bam_path);
    m_File->SetPreviousReadStatistics(m_Index.GetReadStatistics());
    CBGZFStream stream(*m_File);
    m_Header.Read(stream);
    m_Index.SetLengthFromHeader(m_Header);
}


double CBamRawDb::GetEstimatedSecondsPerByte() const
{
    // adjustments
    const double index_read_weight = 10;
    const Uint8 add_read_bytes = 100000; // 100KB
    const double add_read_bytes_per_second = 80e6; // 80 MBps
    const Uint8 add_unzip_bytes = 100000; // 100KB
    const double add_unzip_bytes_per_second = 80e6; // 80 MBps
    
    pair<Uint8, double> index_read_stat = m_Index.GetReadStatistics();
    pair<Uint8, double> data_read_stat = m_File->GetReadStatistics();
    pair<Uint8, double> data_unzip_stat = m_File->GetUncompressStatistics();
    Uint8 read_bytes =
        Uint8(index_read_stat.first*index_read_weight) +
        data_read_stat.first +
        add_read_bytes;
    double read_seconds =
        index_read_stat.second*index_read_weight +
        data_read_stat.second +
        add_read_bytes/add_read_bytes_per_second;
    
    Uint8 unzip_bytes = data_unzip_stat.first + add_unzip_bytes;
    double unzip_seconds = data_unzip_stat.second + add_unzip_bytes/add_unzip_bytes_per_second;

    return read_seconds/read_bytes + unzip_seconds/unzip_bytes;
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


const char SBamAlignInfo::kCIGARSymbols[] = "MIDNSHP=X???????";
const char SBamAlignInfo::kBaseSymbols[] = "=ACMGRSVTWYHKDBN";


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


bool SBamAlignInfo::has_ambiguous_match() const
{
    const char* ptr = get_cigar_ptr();
    for ( uint16_t count = get_cigar_ops_count(); count--; ) {
        uint32_t op = SBamUtil::MakeUint4(ptr);
        ptr += 4;
        switch ( op & 0xf ) {
        case kCIGAR_M:
            return true;
        default:
            break;
        }
    }
    return false;
}


static inline char* s_format(char* dst, uint32_t v)
{
    if ( v < 10 ) {
        *dst = '0'+v;
        return dst+1;
    }
    if ( v >= 100 ) {
        dst = s_format(dst, v/100);
        v %= 100;
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


void CBamAuxIterator::x_InitData()
{
    const char* ptr = m_AuxPtr;
    const char* end = m_AuxEnd;
    if ( ptr == end ) {
        // end of tags
        m_AuxData = SBamAuxData();
        return;
    }
    ptr += 3; // skip tag name and type
    if ( ptr <= end ) {
        m_AuxData.m_Tag[0] = ptr[-3];
        m_AuxData.m_Tag[1] = ptr[-2];
        m_AuxData.m_DataType = ptr[-1];
        m_AuxData.m_IsArray = false;
        m_AuxData.m_ElementCount = 1;
        m_AuxData.m_DataPtr = ptr;
        switch ( m_AuxData.m_DataType ) {
        case 'A':
        case 'c':
        case 'C':
            // 1-byte value
            ptr += 1;
            if ( ptr <= end ) {
                // fits
                m_AuxPtr = ptr;
                return;
            }
            // fallback to error
            break;
        case 's':
        case 'S':
            // 2-byte value
            ptr += 2;
            if ( ptr <= end ) {
                // fits
                m_AuxPtr = ptr;
                return;
            }
            // fallback to error
            break;
        case 'i':
        case 'I':
        case 'f':
            // 4-byte value
            ptr += 4;
            if ( ptr <= end ) {
                // fits
                m_AuxPtr = ptr;
                return;
            }
            // fallback to error
            break;
        case 'Z':
        case 'H':
            // zero-terminated string
            ptr = static_cast<const char*>(memchr(ptr, 0, end-ptr));
            if ( ptr ) {
                // found zero termination
                m_AuxData.m_ElementCount = uint32_t(ptr-m_AuxData.m_DataPtr);
                m_AuxPtr = ptr + 1; // skip zero-termination too
                return;
            }
            // fallback to error
            break;
        case 'B':
            // array of fixed-size elements
            ptr += 5; // skip element type and count
            if ( ptr <= end ) {
                m_AuxData.m_IsArray = true;
                m_AuxData.m_DataType = ptr[-5];
                m_AuxData.m_ElementCount = SBamUtil::MakeUint4(ptr-4);
                m_AuxData.m_DataPtr = ptr;
                size_t element_size;
                switch ( m_AuxData.m_DataType ) {
                case 'c':
                case 'C':
                    element_size = 1;
                    break;
                case 's':
                case 'S':
                    element_size = 2;
                    break;
                case 'i':
                case 'I':
                case 'f':
                    element_size = 4;
                    break;
                default:
                    element_size = 0;
                    break;
                }
                if ( element_size == 0 ) {
                    // fallback to error
                    break;
                }
                ptr += m_AuxData.m_ElementCount*element_size;
                if ( ptr <= end ) {
                    // fits
                    m_AuxPtr = ptr;
                    return;
                }
            }
            // fallback to error
            break;
        default:
            // fallback to error
            break;
        }
    }
    // bad aux format, cannot continue parsing aux data
    ERR_POST("BAM: Alignment aux tag parse error");
    m_AuxData = SBamAuxData();
    m_AuxPtr = end;
}


char SBamAuxData::GetChar() const
{
    if ( !IsChar() ) {
        NCBI_THROW_FMT(CBamException, eOtherError,
                       "Conversion error: "
                       "type "<<GetDataType()<<" cannot be converted to char");
    }
    return m_DataPtr[0];
}


CTempString SBamAuxData::GetString() const
{
    if ( !IsString() ) {
        NCBI_THROW_FMT(CBamException, eOtherError,
                       "Conversion error: "
                       "type "<<GetDataType()<<" cannot be converted to string");
    }
    return CTempString(m_DataPtr, size());
}


Int8 SBamAuxData::GetInt(size_t index) const
{
    if ( !IsInt() ) {
        NCBI_THROW_FMT(CBamException, eOtherError,
                       "Conversion error: "
                       "type "<<GetDataType()<<" cannot be converted to int");
    }
    if ( index >= size() ) {
        NCBI_THROW_FMT(CBamException, eInvalidArg,
                       "Index overflow: "<<index<<" >= "<<size());
        return false;
    }
    switch ( GetDataType() ) {
    case 'c': // signed byte
        return Int1(m_DataPtr[index]);
    case 'C': // unsigned byte
        return Uint1(m_DataPtr[index]);
    case 's': // signed 2-byte int
        return Int2(SBamUtil::MakeUint2(m_DataPtr+2*index));
    case 'S': // unsigned 2-byte int
        return Uint2(SBamUtil::MakeUint2(m_DataPtr+2*index));
    case 'i': // signed 4-byte int
        return Int4(SBamUtil::MakeUint4(m_DataPtr+4*index));
    case 'I': // unsigned 4-byte int
        return Uint4(SBamUtil::MakeUint4(m_DataPtr+4*index));
    default:
        // couldn't be here because IsInt() == true
        return 0;
    }
}


float SBamAuxData::GetFloat(size_t index) const
{
    if ( !IsFloat() ) {
        NCBI_THROW_FMT(CBamException, eOtherError,
                       "Conversion error: "
                       "type "<<GetDataType()<<" cannot be converted to float");
    }
    if ( index >= size() ) {
        NCBI_THROW_FMT(CBamException, eInvalidArg,
                       "Index overflow: "<<index<<" >= "<<size());
        return false;
    }
    return SBamUtil::MakeFloat(m_DataPtr+4*index);
}


SBamAuxData SBamAlignInfo::get_aux_data(char c1, char c2, bool allow_missing) const
{
    for ( CBamAuxIterator iter(get_aux_data_ptr(), get_aux_data_end()); iter; ++iter ) {
        if ( iter->IsTag(c1, c2) ) {
            return *iter;
        }
    }
    if ( !allow_missing ) {
        NCBI_THROW_FMT(CBamException, eNoData,
                       "Tag "<<c1<<c2<<" not found");
    }
    return SBamAuxData();
}


CTempString SBamAlignInfo::get_short_seq_accession_id() const
{
    if ( auto data = get_aux_data('R', 'G', true) ) {
        return data.GetString();
    }
    return CTempString();
}


void SBamAlignInfo::Read(CBGZFStream& in)
{
    in.GetNextAvailableBytes(); // update position if it's at the end of block
    m_FilePos = in.GetPos();
    m_RecordSize = SBamUtil::MakeUint4(in.Read(4));
    m_RecordPtr = in.Read(m_RecordSize);
    m_CIGARPtr = get_read_name_ptr()+get_read_name_len();
    m_ReadPtr = get_cigar_ptr()+get_cigar_ops_count()*4;
    _ASSERT(get_aux_data_ptr() <= get_aux_data_end());
}


/////////////////////////////////////////////////////////////////////////////
// CBamRawAlignIterator
/////////////////////////////////////////////////////////////////////////////


CBamRawAlignIterator::CBamRawAlignIterator(CBamRawDb& bam_db,
                                           const string& ref_label,
                                           TSeqPos ref_pos,
                                           TSeqPos window,
                                           ESearchMode search_mode,
                                           const CBGZFPos* file_pos)
    : m_Reader(bam_db.GetFile())
{
    CRange<TSeqPos> ref_range(ref_pos, ref_pos);
    if ( window && ref_pos < kInvalidSeqPos-window ) {
        ref_range.SetToOpen(ref_pos+window);
    }
    else {
        ref_range.SetToOpen(kInvalidSeqPos);
    }
    Select(bam_db, ref_label, ref_range, search_mode, file_pos);
}


CBamRawAlignIterator::CBamRawAlignIterator(CBamRawDb& bam_db,
                                           const string& ref_label,
                                           TSeqPos ref_pos,
                                           TSeqPos window,
                                           TIndexLevel min_index_level,
                                           TIndexLevel max_index_level,
                                           ESearchMode search_mode,
                                           const CBGZFPos* file_pos)
    : m_Reader(bam_db.GetFile())
{
    CRange<TSeqPos> ref_range(ref_pos, ref_pos);
    if ( window && ref_pos < kInvalidSeqPos-window ) {
        ref_range.SetToOpen(ref_pos+window);
    }
    else {
        ref_range.SetToOpen(kInvalidSeqPos);
    }
    Select(bam_db, ref_label, ref_range, min_index_level, max_index_level, search_mode, file_pos);
}


CBamRawAlignIterator::CBamRawAlignIterator(CBamRawDb& bam_db,
                                           const string& ref_label,
                                           TSeqPos ref_pos,
                                           TSeqPos window,
                                           EIndexLevel min_index_level,
                                           EIndexLevel max_index_level,
                                           ESearchMode search_mode,
                                           const CBGZFPos* file_pos)
    : m_Reader(bam_db.GetFile())
{
    CRange<TSeqPos> ref_range(ref_pos, ref_pos);
    if ( window && ref_pos < kInvalidSeqPos-window ) {
        ref_range.SetToOpen(ref_pos+window);
    }
    else {
        ref_range.SetToOpen(kInvalidSeqPos);
    }
    Select(bam_db, ref_label, ref_range, min_index_level, max_index_level, search_mode, file_pos);
}


void CBamRawAlignIterator::x_Select(const CBamHeader& header,
                                    const CBGZFPos* file_pos)
{
    m_RefIndex = size_t(-1);
    m_QueryRefRange = CRange<TSeqPos>::GetEmpty();
    if ( file_pos && *file_pos ) {
        m_Ranges.SetFrom(*file_pos);
    }
    else {
        m_Ranges.SetWhole(header);
    }
    m_NextRange = m_Ranges.begin();
    m_MinIndexLevel = 0;
    m_MaxIndexLevel = 0;
    if ( x_UpdateRange() ) {
        Next();
    }
}


void CBamRawAlignIterator::x_Select(const CBamIndex& index,
                                    size_t ref_index,
                                    CRange<TSeqPos> ref_range,
                                    TIndexLevel min_index_level,
                                    TIndexLevel max_index_level,
                                    ESearchMode search_mode,
                                    const CBGZFPos* file_pos)
{
    SBamIndexParams::operator=(index);
    m_RefIndex = ref_index;
    m_QueryRefRange = ref_range;
    m_Ranges.SetRanges(index, ref_index, ref_range, min_index_level, max_index_level, search_mode, file_pos);
    m_NextRange = m_Ranges.begin();
    m_MinIndexLevel = min_index_level;
    m_MaxIndexLevel = max_index_level;
    m_SearchMode = search_mode;
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
        // check for alignment validity
        if ( size_t(m_AlignInfo.get_ref_index()) != m_RefIndex ) {
            // wrong reference sequence
            return true;
        }
        if ( !IsMapped() ) {
            // unaligned read
            return true;
        }
        if ( GetCIGAROpsCount() == 0 ) {
            // empty CIGAR string
            return true;
        }
    }
    auto alignment = m_AlignInfo.get_cigar_alignment();
    m_AlignRefRange = alignment.first;
    m_AlignReadRange = alignment.second;
    if ( m_RefIndex == size_t(-1) ) {
        // unfiltered alignments
        return false;
    }
    if ( m_AlignRefRange.GetFrom() >= m_QueryRefRange.GetToOpen() ) {
        // after search range
        x_Stop();
        return false;
    }
    if ( m_SearchMode == eSearchByOverlap ) {
        // any overlapping alignment
        if ( m_AlignRefRange.GetToOpen() <= m_QueryRefRange.GetFrom() ) {
            // before search range
            return true;
        }
    }
    else {
        // only starting within the range
        if ( m_AlignRefRange.GetFrom() < m_QueryRefRange.GetFrom() ) {
            // before search range
            return true;
        }
    }
    if ( m_MinIndexLevel != 0 || m_MaxIndexLevel != GetMaxIndexLevel() ) {
        TIndexLevel index_level = GetIndexLevel();
        if ( index_level < m_MinIndexLevel || index_level > m_MaxIndexLevel ) {
            // this index level is not requested
            return true;
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
