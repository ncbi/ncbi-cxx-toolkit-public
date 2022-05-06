#ifndef SRA__READER__BAM__BAMINDEX__HPP
#define SRA__READER__BAM__BAMINDEX__HPP
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

#include <corelib/ncbistd.hpp>
#include <util/range.hpp>
#include <sra/readers/bam/bgzf.hpp>
#include <objects/seqloc/Na_strand.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_annot;
class CBGZFStream;
class CBamString;

struct NCBI_BAMREAD_EXPORT SBamHeaderRefInfo
{
    void Read(CBGZFStream& in);

    string m_Name;
    TSeqPos m_Length;
};


class NCBI_BAMREAD_EXPORT CBamHeader
{
public:
    CBamHeader();
    explicit
    CBamHeader(const string& bam_file_name);
    ~CBamHeader();

    void Read(CBGZFStream& stream);
    void Read(const string& bam_file_name);

    const string& GetText() const
        {
            return m_Text;
        }

    typedef map<string, string> TSBamTags;
    typedef pair<string, TSBamTags> TSBamRecord;
    typedef list<TSBamRecord> TSBamRecords;
    // parse m_Text (SAM header), return number of records 
    size_t GetSBamRecords(TSBamRecords& records) const;

    typedef vector<SBamHeaderRefInfo> TRefs;
    const TRefs& GetRefs() const
        {
            return m_Refs;
        }

    size_t GetRefCount() const
        {
            return m_Refs.size();
        }
    const SBamHeaderRefInfo& GetRef(size_t ref_index) const;
    size_t GetRefIndex(const string& name) const;
    const string& GetRefName(size_t index) const
        {
            return m_Refs[index].m_Name;
        }
    TSeqPos GetRefLength(size_t index) const
        {
            return m_Refs[index].m_Length;
        }
    
    static SBamHeaderRefInfo ReadRef(CBGZFStream& in);

    CBGZFPos GetAlignStart() const
        {
            return m_AlignStart;
        }

private:
    string m_Text;
    map<string, size_t> m_RefByName;
    TRefs m_Refs;
    CBGZFPos m_AlignStart;
};


struct SBamIndexDefs
{
    enum ESearchMode {
        eSearchByOverlap,
        eSearchByStart
    };
    typedef uint32_t TBin;
    typedef uint8_t TIndexLevel;
    // Unfortunately, the bins and index levels are reversly ordered
    // smallest by size bins are at min index level (0) and has largest bin numbers
    //   bin size = smallest, index level = smallest (=0), bin numbers = largest
    // the largest by size bin is at max index level (variable) and has bin number = 0
    //   bin size = largest, index level = largest, bin number = smallest (=0)
    //
    // To avoid ambiguity we use Min and Max always with Bin and {Index}Level
    // MinBin refers to bins smallest size (unfortunately with largest bin numbers)
    // MaxBin refers to the bin with largest size (unfortunately with smallest bin number = 0)
    // Min{Index}Level refers to index level 0 (largest bin numbers, but smallest bin size)
    // Max{Index}Level refers to max index level (smallest bin number, but largest bin size)
    static const TBin kMaxBinNumber = 0; // single max bin (largest in size)
    static const TIndexLevel kMinBinIndexLevel = 0; // there're multiple min bins (smallest in size)
    
    typedef uint8_t TShift;
    static const TShift kLevelStepBinShift = 3;
    static const TShift kBAI_min_shift = 14;
    static const TIndexLevel kBAI_depth = 5;

    enum EIndexLevel : uint8_t {
        // number of index levels
        kMinLevel = 0, // bins smallest in size
        kLevel0 = kMinLevel,
        kLevel1 = kLevel0+1,
        kMaxLevel = kBAI_depth // special value, to be treated as actual max level
    };
};

#define BAM_SUPPORT_CSI

struct SBamIndexParams : public SBamIndexDefs
{
#ifdef BAM_SUPPORT_CSI
    bool is_CSI;
    TShift min_shift;
    TIndexLevel depth;
    constexpr TShift GetMinLevelBinShift() const
    {
        return min_shift;
    }
    constexpr TIndexLevel GetMaxIndexLevel() const
    {
        return depth;
    }
    constexpr TIndexLevel ToIndexLevel(EIndexLevel level) const
    {
        return level == kMaxLevel? GetMaxIndexLevel(): TIndexLevel(level);
    }
#else
    static const bool is_CSI = false;
    static constexpr TShift GetMinLevelBinShift()
    {
        return kBAI_min_shift;
    }
    static constexpr TIndexLevel GetMaxIndexLevel()
    {
        return kBAI_depth;
    }
    static constexpr TIndexLevel ToIndexLevel(EIndexLevel level)
    {
        return TIndexLevel(level); // direct mapping
    }
#endif

    // return bit shift for size of bin on a specific index level
    constexpr TShift GetLevelBinShift(TIndexLevel level) const
    {
        return GetMinLevelBinShift() + kLevelStepBinShift*level;
    }
    constexpr TShift GetLevelBinShift(EIndexLevel level) const
    {
        return GetLevelBinShift(ToIndexLevel(level));
    }
    // return size of bin on a specific index level
    constexpr TSeqPos GetBinSize(TIndexLevel level) const
    {
        return TSeqPos(1) << GetLevelBinShift(level);
    }
    constexpr TSeqPos GetBinSize(EIndexLevel level) const
    {
        return GetBinSize(ToIndexLevel(level));
    }
    constexpr TShift GetMinBinShift() const
    {
        return GetLevelBinShift(kMinBinIndexLevel);
    }
    constexpr TSeqPos GetMinBinSize() const
    {
        return GetBinSize(kMinBinIndexLevel);
    }
    constexpr TSeqPos GetMaxBinSize() const
    {
        return GetBinSize(GetMaxIndexLevel());
    }

    // Min bin size is page size
    constexpr TSeqPos GetPageSize() const
    {
        return GetMinBinSize();
    }
    // number of bits to shift to convert between page and position
    constexpr TShift GetPageShift() const
    {
        return GetMinBinShift();
    }

    // normal TIndexLevel=0 - has smallest bin sizes and bins numbers are biggest
    // for bin number calculation it's better to count from the maximal level with bin=0
    constexpr TBin GetBinNumberBaseReversed(int reversed_level) const
    {
        // (kAllowedLevels*kLevelStepBinShift+1) bits must fit in unsigned
        constexpr int kAllowedLevels = 10;
        constexpr unsigned kBaseBits =
            ((1u<<(kAllowedLevels*kLevelStepBinShift))-1)/((1<<kLevelStepBinShift)-1);
        return kBaseBits >> ((kAllowedLevels-reversed_level)*kLevelStepBinShift);
    }
    // base bin number of a specific index level
    constexpr TBin GetBinNumberBase(int level) const
    {
        return GetBinNumberBaseReversed(GetMaxIndexLevel()-level);
    }
    constexpr TBin GetBinNumberBase(EIndexLevel level) const
    {
        return GetBinNumberBase(ToIndexLevel(level));
    }
    // base for bin numbers calculation
    constexpr TBin GetMinBinNumberBase() const
    {
        // kBinNumberBase == 4681 == 011111 in octal for 5 levels with 3 bits per level
        return GetBinNumberBase(kMinBinIndexLevel);
    }
    constexpr TBin GetFirstOverflowBin(TIndexLevel level = 0) const
    {
        return GetBinNumberBase(level-1);
    }
    constexpr TBin GetFirstBin(TIndexLevel level) const
    {
        return GetBinNumberBase(level);
    }
    constexpr TBin GetLastBin(TIndexLevel level) const
    {
        return GetBinNumberBase(level-1)-1;
    }
    constexpr TBin GetPseudoBin() const
    {
        return GetFirstOverflowBin()+1;
    }
    bool IsOverflowBin(TBin bin, TIndexLevel level = 0) const
    {
        return bin >= GetFirstOverflowBin(level);
    }
    bool IsOverflowPos(TSeqPos pos) const
    {
        return pos < GetMaxBinSize();
    }
    TBin GetBinNumberOffset(TSeqPos pos, TIndexLevel level) const
    {
        return TBin(pos >> GetLevelBinShift(level));
    }
    TBin GetBinNumberOffset(TSeqPos pos, EIndexLevel level) const
    {
        return GetBinNumberOffset(pos, ToIndexLevel(level));
    }
    TBin GetBinNumber(TSeqPos pos, TIndexLevel level) const
    {
        return GetBinNumberBase(level) + GetBinNumberOffset(pos, level);
    }
    TBin GetBinNumber(TSeqPos pos, EIndexLevel level) const
    {
        return GetBinNumber(pos, ToIndexLevel(level));
    }
    // return range of bins from an index level covering a sequence range
    // the range may be empty (second < first) if sequence range is beyond index
    pair<TBin, TBin> GetBinRange(COpenRange<TSeqPos> ref_range,
                                 TIndexLevel index_level) const;
    TBin GetUpperBinNumber(TBin bin) const
    {
        _ASSERT(bin != 0);
        return IsOverflowBin(bin)? 0: (bin-1)>>kLevelStepBinShift;
    }
    TIndexLevel Bin2IndexLevel(TBin bin) const
    {
        TBin bin_start = GetMinBinNumberBase();
        for ( TIndexLevel level = 0; ; ++level, bin_start >>= kLevelStepBinShift ) {
            if ( bin >= bin_start ) {
                return level;
            }
        }
    }
    TIndexLevel GetRangeIndexLevel(CRange<TSeqPos> range) const
    {
        TIndexLevel level = 0;
        auto local_min_shift = GetMinLevelBinShift();
        TSeqPos pos1 = range.GetFrom() >> local_min_shift;
        TSeqPos pos2 = range.GetTo() >> local_min_shift;
        while ( level < GetMaxIndexLevel() && pos1 != pos2 ) {
            ++level;
            pos1 >>= kLevelStepBinShift;
            pos2 >>= kLevelStepBinShift;
        }
        return level;
    }

    COpenRange<TSeqPos> GetSeqRange(TBin bin) const
    {
        TIndexLevel level = Bin2IndexLevel(bin);
        TSeqPos len = GetBinSize(level);
        TSeqPos index = bin - GetBinNumberBase(level);
        TSeqPos pos = index*len;
        return COpenRange<TSeqPos>(pos, pos+len);
    }
};


struct NCBI_BAMREAD_EXPORT SBamIndexBinInfo : public SBamIndexDefs
{
    void Read(CNcbiIstream& in,
              SBamIndexParams params);
    const char* Read(const char* buffer_ptr, const char* buffer_end,
                     SBamIndexParams params);

    COpenRange<TSeqPos> GetSeqRange(SBamIndexParams params) const
    {
        return params.GetSeqRange(m_Bin);
    }

    TBin m_Bin;
#ifdef BAM_SUPPORT_CSI
    CBGZFPos m_Overlap;
#endif
    vector<CBGZFRange> m_Chunks;

    CBGZFPos GetStartFilePos() const
        {
            return m_Chunks.front().first;
        }
    CBGZFPos GetEndFilePos() const
        {
            return m_Chunks.back().second;
        }
};
static inline bool operator<(const SBamIndexBinInfo& b1, const SBamIndexBinInfo& b2)
{
    return b1.m_Bin < b2.m_Bin;
}
static inline bool operator<(const SBamIndexBinInfo& b1, SBamIndexBinInfo::TBin b2)
{
    return b1.m_Bin < b2;
}
static inline bool operator<(SBamIndexBinInfo::TBin b1, const SBamIndexBinInfo& b2)
{
    return b1 < b2.m_Bin;
}


struct NCBI_BAMREAD_EXPORT SBamIndexRefIndex : public SBamIndexParams
{
    const char* Read(const char* buffer_ptr, const char* buffer_end,
                     SBamIndexParams params,
                     int32_t ref_index);
    void Read(CNcbiIstream& in,
              SBamIndexParams params,
              int32_t ref_index);
    
    // return limits of data in file based on linear index
    // also adjusts argument ref_range to be within reference sequence
    CBGZFRange GetLimitRange(COpenRange<TSeqPos>& ref_range,
                             ESearchMode search_mode) const;

    CBGZFRange GetFileRange() const;
    vector<uint64_t> CollectEstimatedCoverage(TIndexLevel min_index_level,
                                              TIndexLevel max_index_level) const;
    vector<uint64_t> CollectEstimatedCoverage(EIndexLevel min_index_level,
                                              EIndexLevel max_index_level) const
        {
            return CollectEstimatedCoverage(ToIndexLevel(min_index_level),
                                            ToIndexLevel(max_index_level));
        }
    vector<Uint8> EstimateDataSizeByAlnStartPos(TSeqPos seqlen = kInvalidSeqPos) const;

    // return array of min start position of alignments overlapping with each page
    // may return shorter array if the remaining alignments are completely within their page
    vector<TSeqPos> GetAlnOverStarts(void) const;
    // return array of max end position of alignments overlapping with each page
    // may return shorter array if the remaining alignments are completely within their page
    vector<TSeqPos> GetAlnOverEnds(void) const;


    typedef vector<SBamIndexBinInfo> TBins;
    typedef TBins::const_iterator TBinsIter;
    pair<TBinsIter, TBinsIter> GetLevelBins(TIndexLevel level) const;
    pair<TBinsIter, TBinsIter> GetLevelBins(EIndexLevel level) const
        {
            return GetLevelBins(ToIndexLevel(level));
        }
    // add file ranges with alignments from specific index level
    // return first bin in the range, and first bin iter after the range
    // the TBinsIter range is always valid, if no bins in the range both iters are the same
    pair<TBinsIter, TBinsIter> AddLevelFileRanges(vector<CBGZFRange>& ranges,
                                                  CBGZFRange limit_file_range,
                                                  pair<TBin, TBin> bin_range) const;
    pair<TBinsIter, TBinsIter> GetBinsIterRange(pair<TBin, TBin> bin_range) const;

    void SetLengthFromHeader(TSeqPos length);
    void ProcessBin(const SBamIndexBinInfo& bin);
    bool ProcessPseudoBin(SBamIndexBinInfo& bin);
    
    TBins m_Bins;
    CBGZFRange m_UnmappedChunk;
    Uint8 m_MappedCount;
    Uint8 m_UnmappedCount;
    vector<CBGZFPos> m_Overlaps;
    // estimation of sequence length for practical use, rounded to min bin size
    TSeqPos m_EstimatedLength;
};


class NCBI_BAMREAD_EXPORT CBamIndex : public SBamIndexParams
{
public:
    CBamIndex();
    explicit
    CBamIndex(const string& index_file_name);
    ~CBamIndex();

    const string& GetFileName() const
        {
            return m_FileName;
        }

    void Read(const string& index_file_name);
    void Read(const char* buffer_ptr, size_t buffer_size);
    void Read(CNcbiIstream& in);

    typedef vector<SBamIndexRefIndex> TRefs;
    const TRefs& GetRefs() const
        {
            return m_Refs;
        }
    size_t GetRefCount() const
        {
            return m_Refs.size();
        }
    const SBamIndexRefIndex& GetRef(size_t ref_index) const;
    void SetLengthFromHeader(const CBamHeader& header);

    CBGZFRange GetTotalFileRange(size_t ref_index) const;

    CRef<CSeq_annot>
    MakeEstimatedCoverageAnnot(const CBamHeader& header,
                               const string& ref_name,
                               const string& seq_id,
                               const string& annot_name,
                               TIndexLevel min_index_level,
                               TIndexLevel max_index_level) const;
    CRef<CSeq_annot>
    MakeEstimatedCoverageAnnot(const CBamHeader& header,
                               const string& ref_name,
                               const string& seq_id,
                               const string& annot_name,
                               EIndexLevel min_index_level,
                               EIndexLevel max_index_level) const
    {
        return MakeEstimatedCoverageAnnot(header, ref_name, seq_id, annot_name,
                                          ToIndexLevel(min_index_level),
                                          ToIndexLevel(max_index_level));
    }
    CRef<CSeq_annot>
    MakeEstimatedCoverageAnnot(const CBamHeader& header,
                               const string& ref_name,
                               const string& seq_id,
                               const string& annot_name,
                               TIndexLevel min_index_level = 0) const
    {
        return MakeEstimatedCoverageAnnot(header, ref_name, seq_id, annot_name,
                                          min_index_level, GetMaxIndexLevel());
    }
    CRef<CSeq_annot>
    MakeEstimatedCoverageAnnot(const CBamHeader& header,
                               const string& ref_name,
                               const string& seq_id,
                               const string& annot_name,
                               EIndexLevel min_index_level) const
    {
        return MakeEstimatedCoverageAnnot(header, ref_name, seq_id, annot_name,
                                          ToIndexLevel(min_index_level));
    }
    CRef<CSeq_annot>
    MakeEstimatedCoverageAnnot(const CBamHeader& header,
                               const string& ref_name,
                               const CSeq_id& seq_id,
                               const string& annot_name,
                               TIndexLevel min_index_level,
                               TIndexLevel max_index_level) const;
    CRef<CSeq_annot>
    MakeEstimatedCoverageAnnot(const CBamHeader& header,
                               const string& ref_name,
                               const CSeq_id& seq_id,
                               const string& annot_name,
                               EIndexLevel min_index_level,
                               EIndexLevel max_index_level) const
    {
        return MakeEstimatedCoverageAnnot(header, ref_name, seq_id, annot_name,
                                          ToIndexLevel(min_index_level),
                                          ToIndexLevel(max_index_level));
    }
    CRef<CSeq_annot>
    MakeEstimatedCoverageAnnot(const CBamHeader& header,
                               const string& ref_name,
                               const CSeq_id& seq_id,
                               const string& annot_name,
                               TIndexLevel min_index_level = 0) const
    {
        return MakeEstimatedCoverageAnnot(header, ref_name, seq_id, annot_name,
                                          min_index_level, GetMaxIndexLevel());
    }
    CRef<CSeq_annot>
    MakeEstimatedCoverageAnnot(const CBamHeader& header,
                               const string& ref_name,
                               const CSeq_id& seq_id,
                               const string& annot_name,
                               EIndexLevel min_index_level) const
    {
        return MakeEstimatedCoverageAnnot(header, ref_name, seq_id, annot_name,
                                          ToIndexLevel(min_index_level));
    }


    CRef<CSeq_annot>
    MakeEstimatedCoverageAnnot(size_t ref_index,
                               const string& seq_id,
                               const string& annot_name,
                               TIndexLevel min_index_level,
                               TIndexLevel max_index_level) const;
    CRef<CSeq_annot>
    MakeEstimatedCoverageAnnot(size_t ref_index,
                               const string& seq_id,
                               const string& annot_name,
                               EIndexLevel min_index_level,
                               EIndexLevel max_index_level) const
    {
        return MakeEstimatedCoverageAnnot(ref_index, seq_id, annot_name,
                                          ToIndexLevel(min_index_level),
                                          ToIndexLevel(max_index_level));
    }
    CRef<CSeq_annot>
    MakeEstimatedCoverageAnnot(size_t ref_index,
                               const string& seq_id,
                               const string& annot_name,
                               TIndexLevel min_index_level = 0) const
    {
        return MakeEstimatedCoverageAnnot(ref_index, seq_id, annot_name,
                                          min_index_level, GetMaxIndexLevel());
    }
    CRef<CSeq_annot>
    MakeEstimatedCoverageAnnot(size_t ref_index,
                               const string& seq_id,
                               const string& annot_name,
                               EIndexLevel min_index_level) const
    {
        return MakeEstimatedCoverageAnnot(ref_index, seq_id, annot_name,
                                          ToIndexLevel(min_index_level));
    }
    CRef<CSeq_annot>
    MakeEstimatedCoverageAnnot(size_t ref_index,
                               const CSeq_id& seq_id,
                               const string& annot_name,
                               TIndexLevel min_index_level,
                               TIndexLevel max_index_level) const;
    CRef<CSeq_annot>
    MakeEstimatedCoverageAnnot(size_t ref_index,
                               const CSeq_id& seq_id,
                               const string& annot_name,
                               EIndexLevel min_index_level,
                               EIndexLevel max_index_level) const
    {
        return MakeEstimatedCoverageAnnot(ref_index, seq_id, annot_name,
                                          ToIndexLevel(min_index_level),
                                          ToIndexLevel(max_index_level));
    }
    CRef<CSeq_annot>
    MakeEstimatedCoverageAnnot(size_t ref_index,
                               const CSeq_id& seq_id,
                               const string& annot_name,
                               TIndexLevel min_index_level = 0) const
    {
        return MakeEstimatedCoverageAnnot(ref_index, seq_id, annot_name,
                                          min_index_level, GetMaxIndexLevel());
    }
    CRef<CSeq_annot>
    MakeEstimatedCoverageAnnot(size_t ref_index,
                               const CSeq_id& seq_id,
                               const string& annot_name,
                               EIndexLevel min_index_level) const
    {
        return MakeEstimatedCoverageAnnot(ref_index, seq_id, annot_name,
                                          ToIndexLevel(min_index_level));
    }

    CRef<CSeq_annot>
    MakeEstimatedCoverageAnnot(size_t ref_index,
                               const string& seq_id,
                               const string& annot_name,
                               TSeqPos ref_length,
                               TIndexLevel min_index_level,
                               TIndexLevel max_index_level) const;
    CRef<CSeq_annot>
    MakeEstimatedCoverageAnnot(size_t ref_index,
                               const string& seq_id,
                               const string& annot_name,
                               TSeqPos ref_length,
                               EIndexLevel min_index_level,
                               EIndexLevel max_index_level) const
    {
        return MakeEstimatedCoverageAnnot(ref_index, seq_id, annot_name, ref_length,
                                          ToIndexLevel(min_index_level),
                                          ToIndexLevel(max_index_level));
    }
    CRef<CSeq_annot>
    MakeEstimatedCoverageAnnot(size_t ref_index,
                               const string& seq_id,
                               const string& annot_name,
                               TSeqPos ref_length,
                               TIndexLevel min_index_level = 0) const
    {
        return MakeEstimatedCoverageAnnot(ref_index, seq_id, annot_name, ref_length,
                                          min_index_level, GetMaxIndexLevel());
    }
    CRef<CSeq_annot>
    MakeEstimatedCoverageAnnot(size_t ref_index,
                               const string& seq_id,
                               const string& annot_name,
                               TSeqPos ref_length,
                               EIndexLevel min_index_level) const
    {
        return MakeEstimatedCoverageAnnot(ref_index, seq_id, annot_name, ref_length,
                                          ToIndexLevel(min_index_level));
    }
    CRef<CSeq_annot>
    MakeEstimatedCoverageAnnot(size_t ref_index,
                               const CSeq_id& seq_id,
                               const string& annot_name,
                               TSeqPos ref_length,
                               TIndexLevel min_index_level,
                               TIndexLevel max_index_level) const;
    CRef<CSeq_annot>
    MakeEstimatedCoverageAnnot(size_t ref_index,
                               const CSeq_id& seq_id,
                               const string& annot_name,
                               TSeqPos ref_length,
                               EIndexLevel min_index_level,
                               EIndexLevel max_index_level) const
    {
        return MakeEstimatedCoverageAnnot(ref_index, seq_id, annot_name, ref_length,
                                          ToIndexLevel(min_index_level),
                                          ToIndexLevel(max_index_level));
    }
    CRef<CSeq_annot>
    MakeEstimatedCoverageAnnot(size_t ref_index,
                               const CSeq_id& seq_id,
                               const string& annot_name,
                               TSeqPos ref_length,
                               TIndexLevel min_index_level = 0) const
    {
        return MakeEstimatedCoverageAnnot(ref_index, seq_id, annot_name, ref_length,
                                          min_index_level, GetMaxIndexLevel());
    }
    CRef<CSeq_annot>
    MakeEstimatedCoverageAnnot(size_t ref_index,
                               const CSeq_id& seq_id,
                               const string& annot_name,
                               TSeqPos ref_length,
                               EIndexLevel min_index_level) const
    {
        return MakeEstimatedCoverageAnnot(ref_index, seq_id, annot_name, ref_length,
                                          ToIndexLevel(min_index_level));
    }
    
    // collect estimated coverage from index level range
    // result bin size will be equal to bin size of min_index_level
    vector<uint64_t>
    CollectEstimatedCoverage(size_t ref_index,
                             TIndexLevel min_index_level,
                             TIndexLevel max_index_level) const;
    vector<uint64_t>
    CollectEstimatedCoverage(size_t ref_index,
                             EIndexLevel min_index_level,
                             EIndexLevel max_index_level) const
    {
        return CollectEstimatedCoverage(ref_index,
                                        ToIndexLevel(min_index_level),
                                        ToIndexLevel(max_index_level));
    }
    // collect estimated coverage from specified index level
    // result bin size will be equal to bin size of index_level
    vector<uint64_t>
    CollectEstimatedCoverage(size_t ref_index,
                             TIndexLevel index_level) const
        {
            return CollectEstimatedCoverage(ref_index, index_level, index_level);
        }
    vector<uint64_t>
    CollectEstimatedCoverage(size_t ref_index,
                             EIndexLevel index_level) const
        {
            return CollectEstimatedCoverage(ref_index, ToIndexLevel(index_level));
        }
    // collect estimated coverage from all index levels
    // result bin size will be equal to bin size of most detailed index level
    vector<uint64_t>
    CollectEstimatedCoverage(size_t ref_index) const
        {
            return CollectEstimatedCoverage(ref_index, 0, GetMaxIndexLevel());
        }
    // collect estimated coverage from all index levels
    // result bin size will be equal to bin size of most detailed index level
    vector<uint64_t>
    EstimateDataSizeByAlnStartPos(size_t ref_index) const
        {
            return GetRef(ref_index).EstimateDataSizeByAlnStartPos();
        }

    pair<Uint8, double> GetReadStatistics() const
        {
            return make_pair(m_TotalReadBytes, m_TotalReadSeconds);
       }

private:
    string m_FileName;
    TRefs m_Refs;
    Uint8 m_UnmappedCount;
    Uint8 m_TotalReadBytes;
    double m_TotalReadSeconds;
};


template<class Position>
class CRangeUnion
{
public:
    typedef Position    position_type;
    typedef CRangeUnion<position_type>  TThisType;

    typedef pair<position_type, position_type> TRange;
    typedef map<position_type, position_type>  TRanges;
    typedef typename TRanges::iterator   iterator;
    typedef typename TRanges::const_iterator   const_iterator;

    void clear()
        {
            m_Ranges.clear();
        }
    bool empty() const
        {
            return m_Ranges.empty();
        }
    const_iterator begin() const
        {
            return m_Ranges.begin();
        }
    const_iterator end() const
        {
            return m_Ranges.end();
        }

    void add_range(TRange range)
        {
            if ( !(range.first < range.second) ) {
                // empty range, do nothing
                return;
            }

            // find insertion point
            // iterator next points to ranges that start after new range start
            iterator next = m_Ranges.upper_bound(range.first);
            assert(next == m_Ranges.end() || (range.first < next->first));

            // check for overlapping with previous range
            iterator iter;
            if ( next != m_Ranges.begin() &&
                 !((iter = prev(next))->second < range.first) ) {
                // overlaps with previous range
                // update it if necessary
                if ( !(iter->second < range.second) ) {
                    // new range is completely within an old one
                    // no more work to do
                    return;
                }
                // need to extend previous range to include inserted range
                // next ranges may need to be removed
            }
            else {
                // new range, use found iterator as an insertion hint
                iter = m_Ranges.insert(next, range);
                // next ranges may need to be removed
            }
            assert(iter != m_Ranges.end() && next != m_Ranges.begin() &&
                   iter == prev(next) &&
                   !(range.first < iter->first) &&
                   !(range.second < iter->second));

            // erase all existing ranges that start within inserted range
            // and extend inserted range if necessary
            while ( next != m_Ranges.end() &&
                    !(range.second < next->first) ) {
                if ( range.second < next->second ) {
                    // range that start within inserted range is bigger,
                    // extend inserted range
                    range.second = next->second;
                }
                // erase completely covered range
                m_Ranges.erase(next++);
            }
            // update current range
            iter->second = range.second;
        }

    TThisType& operator+=(const TRange& range)
        {
            add_range(range);
            return *this;
        }

private:
    TRanges m_Ranges;
};


class NCBI_BAMREAD_EXPORT CBamFileRangeSet : public SBamIndexDefs
{
public:
    CBamFileRangeSet();
    CBamFileRangeSet(const CBamIndex& index,
                     size_t ref_index, COpenRange<TSeqPos> ref_range,
                     ESearchMode search_mode = eSearchByOverlap);
    CBamFileRangeSet(const CBamIndex& index,
                     size_t ref_index, COpenRange<TSeqPos> ref_range,
                     TIndexLevel min_level, TIndexLevel max_level,
                     ESearchMode search_mode = eSearchByOverlap);
    CBamFileRangeSet(const CBamIndex& index,
                     size_t ref_index, COpenRange<TSeqPos> ref_range,
                     EIndexLevel min_level, EIndexLevel max_level,
                     ESearchMode search_mode = eSearchByOverlap);
    ~CBamFileRangeSet();

    void Clear()
        {
            m_Ranges.clear();
        }
    void SetRanges(const CBamIndex& index,
                   size_t ref_index, COpenRange<TSeqPos> ref_range,
                   ESearchMode search_mode = eSearchByOverlap);
    void AddRanges(const CBamIndex& index,
                   size_t ref_index, COpenRange<TSeqPos> ref_range,
                   ESearchMode search_mode = eSearchByOverlap);
    void SetRanges(const CBamIndex& index,
                   size_t ref_index, COpenRange<TSeqPos> ref_range,
                   TIndexLevel index_level,
                   ESearchMode search_mode = eSearchByOverlap);
    void SetRanges(const CBamIndex& index,
                   size_t ref_index, COpenRange<TSeqPos> ref_range,
                   EIndexLevel index_level,
                   ESearchMode search_mode = eSearchByOverlap)
        {
            SetRanges(index, ref_index, ref_range, index.ToIndexLevel(index_level), search_mode);
        }
    void AddRanges(const CBamIndex& index,
                   size_t ref_index, COpenRange<TSeqPos> ref_range,
                   TIndexLevel index_level,
                   ESearchMode search_mode = eSearchByOverlap);
    void AddRanges(const CBamIndex& index,
                   size_t ref_index, COpenRange<TSeqPos> ref_range,
                   EIndexLevel index_level,
                   ESearchMode search_mode = eSearchByOverlap)
        {
            AddRanges(index, ref_index, ref_range, index.ToIndexLevel(index_level), search_mode);
        }
    void SetRanges(const CBamIndex& index,
                   size_t ref_index, COpenRange<TSeqPos> ref_range,
                   TIndexLevel min_index_level, TIndexLevel max_index_level,
                   ESearchMode search_mode = eSearchByOverlap);
    void SetRanges(const CBamIndex& index,
                   size_t ref_index, COpenRange<TSeqPos> ref_range,
                   EIndexLevel min_index_level, EIndexLevel max_index_level,
                   ESearchMode search_mode = eSearchByOverlap)
        {
            SetRanges(index, ref_index, ref_range,
                      index.ToIndexLevel(min_index_level),
                      index.ToIndexLevel(max_index_level),
                      search_mode);
        }
    void AddRanges(const CBamIndex& index,
                   size_t ref_index, COpenRange<TSeqPos> ref_range,
                   TIndexLevel min_index_level, TIndexLevel max_index_level,
                   ESearchMode search_mode = eSearchByOverlap);
    void AddRanges(const CBamIndex& index,
                   size_t ref_index, COpenRange<TSeqPos> ref_range,
                   EIndexLevel min_index_level, EIndexLevel max_index_level,
                   ESearchMode search_mode = eSearchByOverlap)
        {
            AddRanges(index, ref_index, ref_range,
                      index.ToIndexLevel(min_index_level),
                      index.ToIndexLevel(max_index_level),
                      search_mode);
        }
    void SetWhole(const CBamHeader& header);
    void AddWhole(const CBamHeader& header);

    typedef CRangeUnion<CBGZFPos> TRanges;
    typedef TRanges::const_iterator const_iterator;

    const TRanges& GetRanges() const
        {
            return m_Ranges;
        }
    const_iterator begin() const
        {
            return m_Ranges.begin();
        }
    const_iterator end() const
        {
            return m_Ranges.end();
        }

    static Uint8 GetFileSize(CBGZFRange range);
    Uint8 GetFileSize() const;

protected:
    void AddSortedRanges(const vector<CBGZFRange>& ranges);
    
private:
    TRanges m_Ranges;
};


class NCBI_BAMREAD_EXPORT CBamRawDb
{
public:
    CBamRawDb()
        {
        }
    explicit
    CBamRawDb(const string& bam_path)
        {
            Open(bam_path);
        }
    CBamRawDb(const string& bam_path, const string& index_path)
        {
            Open(bam_path, index_path);
        }
    ~CBamRawDb();


    void Open(const string& bam_path);
    void Open(const string& bam_path, const string& index_path);


    const CBamHeader& GetHeader() const
        {
            return m_Header;
        }
    const CBamIndex& GetIndex() const
        {
            return m_Index;
        }
    const string& GetIndexName() const
        {
            return m_Index.GetFileName();
        }
    size_t GetRefCount() const
        {
            return GetHeader().GetRefCount();
        }
    size_t GetRefIndex(const string& ref_label) const
        {
            return GetHeader().GetRefIndex(ref_label);
        }
    const string& GetRefName(size_t ref_index) const
        {
            return GetHeader().GetRefName(ref_index);
        }
    TSeqPos GetRefSeqLength(size_t ref_index) const
        {
            return GetHeader().GetRefLength(ref_index);
        }


    CBGZFFile& GetFile()
        {
            return *m_File;
        }

    vector<Uint8> EstimateDataSizeByAlnStartPos(const string& ref_label) const
        {
            size_t ref_index = GetRefIndex(ref_label);
            return GetIndex().GetRef(ref_index).EstimateDataSizeByAlnStartPos(GetRefSeqLength(ref_index));
        }

    double GetEstimatedSecondsPerByte() const;
    
private:
    CRef<CBGZFFile> m_File;
    CBamHeader m_Header;
    CBamIndex m_Index;
};


class CBamAuxIterator;


struct SBamAuxData
{
    SBamAuxData()
        : m_Tag(),
          m_DataType(),
          m_IsArray(false),
          m_ElementCount(),
          m_DataPtr(0)
        {
        }

    DECLARE_OPERATOR_BOOL(m_DataPtr);

    CTempString GetTag() const { return CTempString(m_Tag, 2); }
    bool IsTag(char c1, char c2) const { return m_Tag[0] == c1 && m_Tag[1] == c2; }

    char GetDataType() const { return m_DataType; }
    
    bool IsArray() const { return m_IsArray; }
    size_t size() const { return m_ElementCount; }

    bool IsChar() const { return m_DataType == 'A'; }
    bool IsString() const { return m_DataType == 'Z' || m_DataType == 'H'; }
    bool IsFloat() const { return m_DataType == 'f'; }
    bool IsInt() const { return !IsString() && !IsFloat() && !IsChar(); }
    
    NCBI_BAMREAD_EXPORT char GetChar() const;
    NCBI_BAMREAD_EXPORT CTempString GetString() const;
    NCBI_BAMREAD_EXPORT float GetFloat(size_t index = 0) const;
    NCBI_BAMREAD_EXPORT Int8 GetInt(size_t index = 0) const;

private:
    friend class CBamAuxIterator;
    
    char m_Tag[2];
    char m_DataType;
    bool m_IsArray;
    uint32_t m_ElementCount; // either string length or array element count
    const char* m_DataPtr;
};

class CBamAuxIterator
{
  public:
    CBamAuxIterator()
        : m_AuxPtr(0),
          m_AuxEnd(0)
        {
        }
    CBamAuxIterator(const char* aux_ptr, const char* aux_end)
        : m_AuxPtr(aux_ptr),
          m_AuxEnd(aux_end)
        {
            x_InitData();
        }

    CBamAuxIterator& operator++()
        {
            x_InitData();
            return *this;
        }
    
    typedef SBamAuxData value_type;
        
    DECLARE_OPERATOR_BOOL(m_AuxData);
        
    const SBamAuxData& operator*() const { return m_AuxData; }
    const SBamAuxData* operator->() const { return &m_AuxData; }
    
private:
    NCBI_BAMREAD_EXPORT void x_InitData();
    
    SBamAuxData m_AuxData;
    const char* m_AuxPtr;
    const char* m_AuxEnd;
};

struct NCBI_BAMREAD_EXPORT SBamAlignInfo
{
    void Read(CBGZFStream& in);

    CBGZFPos get_file_pos() const
        {
            return m_FilePos;
        }
    size_t get_record_size() const
        {
            return m_RecordSize;
        }
    const char* get_record_ptr() const
        {
            return m_RecordPtr;
        }
    const char* get_record_end() const
        {
            return get_record_ptr() + get_record_size();
        }

    int32_t get_ref_index() const
        {
            return SBamUtil::MakeUint4(get_record_ptr());
        }
    int32_t get_ref_pos() const
        {
            return SBamUtil::MakeUint4(get_record_ptr()+4);
        }

    uint8_t get_read_name_len() const
        {
            return get_record_ptr()[8];
        }
    uint8_t get_map_quality() const
        {
            return get_record_ptr()[9];
        }
    uint16_t get_bin() const
        {
            return SBamUtil::MakeUint2(get_record_ptr()+10);
        }
    static const char kCIGARSymbols[];
    enum ECIGARType { // matches to kCIGARSymbols
        kCIGAR_M,  // 0
        kCIGAR_I,  // 1
        kCIGAR_D,  // 2
        kCIGAR_N,  // 3
        kCIGAR_S,  // 4
        kCIGAR_H,  // 5
        kCIGAR_P,  // 6
        kCIGAR_eq, // 7
        kCIGAR_X   // 8
    };
    uint16_t get_cigar_ops_count() const
        {
            return SBamUtil::MakeUint2(get_record_ptr()+12);
        }
    enum EFlag {
        fAlign_WasPaired        = 1 <<  0,
        fAlign_IsMappedAsPair   = 1 <<  1,
        fAlign_SelfIsUnmapped   = 1 <<  2,
        fAlign_MateIsUnmapped   = 1 <<  3,
        fAlign_SelfIsReverse    = 1 <<  4,
        fAlign_MateIsReverse    = 1 <<  5,
        fAlign_IsFirst          = 1 <<  6,
        fAlign_IsSecond         = 1 <<  7,
        fAlign_IsNotPrimary     = 1 <<  8,
        fAlign_IsLowQuality     = 1 <<  9,
        fAlign_IsDuplicate      = 1 << 10,
        fAlign_IsSupplementary  = 1 << 11
    };
    uint16_t get_flag() const
        {
            return SBamUtil::MakeUint2(get_record_ptr()+14);
        }
    uint32_t get_read_len() const
        {
            return SBamUtil::MakeUint4(get_record_ptr()+16);
        }
    int32_t get_next_ref_index() const
        {
            return SBamUtil::MakeUint4(get_record_ptr()+20);
        }
    int32_t get_next_ref_pos() const
        {
            return SBamUtil::MakeUint4(get_record_ptr()+24);
        }
    int32_t get_tlen() const
        {
            return SBamUtil::MakeUint4(get_record_ptr()+28);
        }
    const char* get_read_name_ptr() const
        {
            return get_record_ptr()+32;
        }
    const char* get_read_name_end() const
        {
            return m_CIGARPtr;
        }
    const char* get_cigar_ptr() const
        {
            return get_read_name_end();
        }
    const char* get_cigar_end() const
        {
            return m_ReadPtr;
        }
    uint32_t get_cigar_op_data(uint16_t index) const
        {
            return SBamUtil::MakeUint4(get_cigar_ptr()+index*4);
        }
    void get_cigar(vector<uint32_t>& raw_cigar) const
        {
            size_t count = get_cigar_ops_count();
            raw_cigar.resize(count);
            uint32_t* dst = raw_cigar.data();
            memcpy(dst, get_cigar_ptr(), count*sizeof(uint32_t));
            for ( size_t i = 0; i < count; ++i ) {
                dst[i] = SBamUtil::MakeUint4(reinterpret_cast<const char*>(dst+i));
            }
        }
    void get_cigar(CBamString& dst) const;
    const char* get_read_ptr() const
        {
            return get_cigar_end();
        }
    const char* get_read_end() const
        {
            return get_read_ptr() + (get_read_len()+1)/2;
        }
    const char* get_phred_quality_ptr() const
        {
            return get_read_end();
        }
    const char* get_phred_quality_end() const
        {
            return get_phred_quality_ptr() + get_read_len();
        }
    const char* get_aux_data_ptr() const
        {
            return get_phred_quality_end();
        }
    const char* get_aux_data_end() const
        {
            return get_record_end();
        }

    CTempString get_read_raw() const
        {
            return CTempString(get_read_ptr(), (get_read_len()+1)/2);
        }
    static const char kBaseSymbols[];
    string get_read() const;
    void get_read(CBamString& str) const;
    uint32_t get_cigar_pos() const;
    uint32_t get_cigar_ref_size() const;
    uint32_t get_cigar_read_size() const;
    pair< COpenRange<uint32_t>, COpenRange<uint32_t> > get_cigar_alignment(void) const;
    string get_cigar() const;
    bool has_ambiguous_match() const;

    SBamAuxData get_aux_data(char c1, char c2, bool allow_missing = false) const;
    CTempString get_short_seq_accession_id() const;

private:
    CBGZFPos m_FilePos;
    const char* m_RecordPtr;
    const char* m_CIGARPtr;
    const char* m_ReadPtr;
    Uint4 m_RecordSize;
};


class NCBI_BAMREAD_EXPORT CBamRawAlignIterator : public SBamIndexParams
{
public:
    CBamRawAlignIterator()
        : m_CurrentRangeEnd(0)
        {
        }
    explicit
    CBamRawAlignIterator(CBamRawDb& bam_db)
        : m_Reader(bam_db.GetFile())
        {
            Select(bam_db);
        }
    CBamRawAlignIterator(CBamRawDb& bam_db,
                         const string& ref_label,
                         CRange<TSeqPos> ref_range,
                         ESearchMode search_mode = eSearchByOverlap)
        : m_Reader(bam_db.GetFile())
        {
            Select(bam_db, ref_label, ref_range, search_mode);
        }
    CBamRawAlignIterator(CBamRawDb& bam_db,
                         const string& ref_label,
                         CRange<TSeqPos> ref_range,
                         TIndexLevel index_level,
                         ESearchMode search_mode = eSearchByOverlap)
        : m_Reader(bam_db.GetFile())
        {
            Select(bam_db, ref_label, ref_range, index_level, search_mode);
        }
    CBamRawAlignIterator(CBamRawDb& bam_db,
                         const string& ref_label,
                         CRange<TSeqPos> ref_range,
                         EIndexLevel index_level,
                         ESearchMode search_mode)
        : m_Reader(bam_db.GetFile())
        {
            Select(bam_db, ref_label, ref_range, index_level, search_mode);
        }
    CBamRawAlignIterator(CBamRawDb& bam_db,
                         const string& ref_label,
                         CRange<TSeqPos> ref_range,
                         TIndexLevel min_index_level,
                         TIndexLevel max_index_level,
                         ESearchMode search_mode = eSearchByOverlap)
        : m_Reader(bam_db.GetFile())
        {
            Select(bam_db, ref_label, ref_range, min_index_level, max_index_level, search_mode);
        }
    CBamRawAlignIterator(CBamRawDb& bam_db,
                         const string& ref_label,
                         CRange<TSeqPos> ref_range,
                         EIndexLevel min_index_level,
                         EIndexLevel max_index_level,
                         ESearchMode search_mode)
        : m_Reader(bam_db.GetFile())
        {
            Select(bam_db, ref_label, ref_range, min_index_level, max_index_level, search_mode);
        }
    CBamRawAlignIterator(CBamRawDb& bam_db,
                         const string& ref_label,
                         TSeqPos ref_pos,
                         TSeqPos window = 0,
                         ESearchMode search_mode = eSearchByOverlap);
    CBamRawAlignIterator(CBamRawDb& bam_db,
                         const string& ref_label,
                         TSeqPos ref_pos,
                         TSeqPos window,
                         TIndexLevel min_index_level,
                         TIndexLevel max_index_level,
                         ESearchMode search_mode = eSearchByOverlap);
    CBamRawAlignIterator(CBamRawDb& bam_db,
                         const string& ref_label,
                         TSeqPos ref_pos,
                         TSeqPos window,
                         EIndexLevel min_index_level,
                         EIndexLevel max_index_level,
                         ESearchMode search_mode);
    ~CBamRawAlignIterator()
        {
        }

    DECLARE_OPERATOR_BOOL(m_CurrentRangeEnd);

    void Select(CBamRawDb& bam_db)
        {
            x_Select(bam_db.GetHeader());
        }
    void Select(CBamRawDb& bam_db,
                const string& ref_label,
                CRange<TSeqPos> ref_range,
                ESearchMode search_mode = eSearchByOverlap)
        {
            x_Select(bam_db.GetIndex(),
                     bam_db.GetRefIndex(ref_label), ref_range, search_mode);
        }
    void Select(CBamRawDb& bam_db,
                const string& ref_label,
                CRange<TSeqPos> ref_range,
                TIndexLevel index_level,
                ESearchMode search_mode = eSearchByOverlap)
        {
            x_Select(bam_db.GetIndex(),
                     bam_db.GetRefIndex(ref_label), ref_range, index_level, search_mode);
        }
    void Select(CBamRawDb& bam_db,
                const string& ref_label,
                CRange<TSeqPos> ref_range,
                EIndexLevel index_level,
                ESearchMode search_mode)
        {
            x_Select(bam_db.GetIndex(),
                     bam_db.GetRefIndex(ref_label), ref_range, index_level, search_mode);
        }
    void Select(CBamRawDb& bam_db,
                const string& ref_label,
                CRange<TSeqPos> ref_range,
                TIndexLevel min_index_level,
                TIndexLevel max_index_level,
                ESearchMode search_mode = eSearchByOverlap)
        {
            x_Select(bam_db.GetIndex(),
                     bam_db.GetRefIndex(ref_label), ref_range, min_index_level, max_index_level, search_mode);
        }
    void Select(CBamRawDb& bam_db,
                const string& ref_label,
                CRange<TSeqPos> ref_range,
                EIndexLevel min_index_level,
                EIndexLevel max_index_level,
                ESearchMode search_mode = eSearchByOverlap)
        {
            x_Select(bam_db.GetIndex(),
                     bam_db.GetRefIndex(ref_label), ref_range, min_index_level, max_index_level, search_mode);
        }
    void Select(const CBamIndex& index,
                size_t ref_index,
                CRange<TSeqPos> ref_range,
                ESearchMode search_mode = eSearchByOverlap)
        {
            x_Select(index, ref_index, ref_range, search_mode);
        }
    void Select(const CBamIndex& index,
                size_t ref_index,
                CRange<TSeqPos> ref_range,
                TIndexLevel index_level,
                ESearchMode search_mode = eSearchByOverlap)
        {
            x_Select(index, ref_index, ref_range, index_level, search_mode);
        }
    void Select(const CBamIndex& index,
                size_t ref_index,
                CRange<TSeqPos> ref_range,
                EIndexLevel index_level,
                ESearchMode search_mode = eSearchByOverlap)
        {
            x_Select(index, ref_index, ref_range, index_level, search_mode);
        }
    void Next();

    CBamRawAlignIterator& operator++()
        {
            Next();
            return *this;
        }

    CBGZFPos GetFilePos() const
        {
            return m_AlignInfo.get_file_pos();
        }
    
    int32_t GetRefSeqIndex() const
        {
            return m_AlignInfo.get_ref_index();
        }
    TSeqPos GetRefSeqPos() const
        {
            return m_AlignRefRange.GetFrom();
        }

    // next segment in template (mate)
    int32_t GetNextRefSeqIndex() const
        {
            return m_AlignInfo.get_next_ref_index();
        }
    TSeqPos GetNextRefSeqPos() const
        {
            return m_AlignInfo.get_next_ref_pos();
        }

    CTempString GetShortSeqId() const
        {
            return CTempString(m_AlignInfo.get_read_name_ptr(),
                               m_AlignInfo.get_read_name_len()-1); // exclude trailing zero
        }
    CTempString GetShortSeqAcc() const
        {
            return m_AlignInfo.get_short_seq_accession_id();
        }
    TSeqPos GetShortSequenceLength(void) const
        {
            return m_AlignInfo.get_read_len();
        }
    string GetShortSequence() const
        {
            return m_AlignInfo.get_read();
        }
    CTempString GetShortSequenceRaw() const
        {
            return m_AlignInfo.get_read_raw();
        }
    void GetShortSequence(CBamString& str) const
        {
            return m_AlignInfo.get_read(str);
        }

    Uint2 GetCIGAROpsCount() const
        {
            return m_AlignInfo.get_cigar_ops_count();
        }
    Uint4 GetCIGAROp(Uint2 index) const
        {
            return m_AlignInfo.get_cigar_op_data(index);
        }
    void GetCIGAR(vector<Uint4>& raw_cigar) const
        {
            return m_AlignInfo.get_cigar(raw_cigar);
        }
    void GetCIGAR(CBamString& dst) const
        {
            m_AlignInfo.get_cigar(dst);
        }
    TSeqPos GetCIGARPos() const
        {
            return m_AlignReadRange.GetFrom();
        }
    TSeqPos GetCIGARShortSize() const
        {
            return m_AlignReadRange.GetLength();
        }
    TSeqPos GetCIGARRefSize() const
        {
            return m_AlignRefRange.GetLength();
        }
    pair< COpenRange<TSeqPos>, COpenRange<TSeqPos> > GetCIGARAlignment(void) const
        {
            return make_pair(m_AlignRefRange, m_AlignReadRange);
        }
    bool HasAmbiguousMatch() const
        {
            return m_AlignInfo.has_ambiguous_match();
        }
    
    string GetCIGAR() const
        {
            return m_AlignInfo.get_cigar();
        }

    Uint2 GetIndexBin() const
        {
            return m_AlignInfo.get_bin();
        }
    TIndexLevel GetIndexLevel() const
        {
            return Bin2IndexLevel(GetIndexBin());
        }
    
    Uint2 GetFlags() const
        {
            return m_AlignInfo.get_flag();
        }
    // returns false if BAM flags are not available
    bool TryGetFlags(Uint2& flags) const
        {
            flags = GetFlags();
            return true;
        }

    bool IsSetStrand() const
        {
            return true;
        }
    ENa_strand GetStrand() const
        {
            return (GetFlags() & m_AlignInfo.fAlign_SelfIsReverse)?
                eNa_strand_minus: eNa_strand_plus;
        }

    bool IsMapped() const
        {
            return (GetFlags() & m_AlignInfo.fAlign_SelfIsUnmapped) == 0;
        }
    
    Uint1 GetMapQuality() const
        {
            return IsMapped()? m_AlignInfo.get_map_quality(): 0;
        }

    bool IsPaired() const
        {
            return (GetFlags() & m_AlignInfo.fAlign_IsMappedAsPair) != 0;
        }
    bool IsFirstInPair() const
        {
            return (GetFlags() & m_AlignInfo.fAlign_IsFirst) != 0;
        }
    bool IsSecondInPair() const
        {
            return (GetFlags() & m_AlignInfo.fAlign_IsSecond) != 0;
        }
    bool IsSecondary() const
        {
            return (GetFlags() & m_AlignInfo.fAlign_IsNotPrimary) != 0;
        }

    void GetSegments(vector<int>& starts, vector<TSeqPos>& lens) const;

    CBamAuxIterator GetAuxIterator() const
       {
           return CBamAuxIterator(m_AlignInfo.get_aux_data_ptr(), m_AlignInfo.get_aux_data_end());
       }
    SBamAuxData GetAuxData(char c1, char c2, bool allow_missing = false) const
        {
            return m_AlignInfo.get_aux_data(c1, c2, allow_missing);
        }
    Int8 GetAuxInt(char c1, char c2, size_t index = 0) const
        {
            return GetAuxData(c1, c2).GetInt(index);
        }
    
protected:
    void x_Select(const CBamHeader& header);
    void x_Select(const CBamIndex& index,
                  size_t ref_index, CRange<TSeqPos> ref_range,
                  TIndexLevel min_index_level, TIndexLevel max_index_level,
                  ESearchMode search_mode);
    void x_Select(const CBamIndex& index,
                  size_t ref_index, CRange<TSeqPos> ref_range,
                  EIndexLevel min_index_level, EIndexLevel max_index_level,
                  ESearchMode search_mode)
        {
            x_Select(index, ref_index, ref_range,
                     index.ToIndexLevel(min_index_level),
                     index.ToIndexLevel(max_index_level),
                     search_mode);
        }
    void x_Select(const CBamIndex& index,
                  size_t ref_index, CRange<TSeqPos> ref_range,
                  ESearchMode search_mode)
        {
            x_Select(index, ref_index, ref_range, 0, index.GetMaxIndexLevel(), search_mode);
        }
    void x_Select(const CBamIndex& index,
                  size_t ref_index, CRange<TSeqPos> ref_range,
                  TIndexLevel index_level,
                  ESearchMode search_mode)
        {
            x_Select(index, ref_index, ref_range, index_level, index_level, search_mode);
        }
    void x_Select(const CBamIndex& index,
                  size_t ref_index, CRange<TSeqPos> ref_range,
                  EIndexLevel index_level,
                  ESearchMode search_mode)
        {
            x_Select(index, ref_index, ref_range, index_level, index_level, search_mode);
        }
    bool x_UpdateRange();
    bool x_NextAnnot()
        {
            _ASSERT(*this);
            return m_Reader.HaveNextAvailableBytes() || x_UpdateRange();
        }
    void x_Stop()
        {
            m_NextRange = m_Ranges.end();
            m_CurrentRangeEnd = CBGZFPos(0);
        }
    bool x_NeedToSkip();
    
private:
    size_t m_RefIndex;
    COpenRange<TSeqPos> m_QueryRefRange;
    TIndexLevel m_MinIndexLevel, m_MaxIndexLevel;
    ESearchMode m_SearchMode;
    SBamAlignInfo m_AlignInfo;
    COpenRange<TSeqPos> m_AlignRefRange;
    COpenRange<TSeqPos> m_AlignReadRange;
    CBamFileRangeSet m_Ranges;
    CBamFileRangeSet::const_iterator m_NextRange;
    CBGZFPos m_CurrentRangeEnd;
    CBGZFStream m_Reader;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // SRA__READER__BAM__BAMINDEX__HPP
