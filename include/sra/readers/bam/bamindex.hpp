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
//#include <util/range_coll.hpp>
#include <sra/readers/bam/bgzf.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_annot;

struct NCBI_BAMREAD_EXPORT SBamHeaderRefInfo
{
    void Read(CNcbiIstream& in);
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
    explicit
    CBamHeader(CNcbiIstream& file_stream);
    ~CBamHeader();

    void Read(CBGZFStream& stream);
    void Read(const string& bam_file_name);
    void Read(CNcbiIstream& file_stream);

    const string& GetText() const
        {
            return m_Text;
        }

    typedef vector<SBamHeaderRefInfo> TRefs;
    const TRefs& GetRefs() const
        {
            return m_Refs;
        }

    size_t GetRefIndex(const string& name) const;
    const string& GetRefName(size_t index) const
        {
            return m_Refs[index].m_Name;
        }
    TSeqPos GetRefLength(size_t index) const
        {
            return m_Refs[index].m_Length;
        }
    
    static SBamHeaderRefInfo ReadRef(CNcbiIstream& in);
    static SBamHeaderRefInfo ReadRef(CBGZFStream& in);

private:
    string m_Text;
    map<string, size_t> m_RefByName;
    TRefs m_Refs;
};


struct NCBI_BAMREAD_EXPORT SBamIndexBinInfo
{
    typedef Uint4 TBin;

    void Read(CNcbiIstream& in);

    static COpenRange<TSeqPos> GetSeqRange(TBin bin);
    COpenRange<TSeqPos> GetSeqRange() const
        {
            return GetSeqRange(m_Bin);
        }

    TBin m_Bin;
    vector<CBGZFRange> m_Chunks;

    bool operator<(const SBamIndexBinInfo& b) const
        {
            return m_Bin < b.m_Bin;
        }
    bool operator<(TBin b) const
        {
            return m_Bin < b;
        }
};


struct NCBI_BAMREAD_EXPORT SBamIndexRefIndex
{
    void Read(CNcbiIstream& in);

    vector<SBamIndexBinInfo> m_Bins;
    CBGZFRange m_UnmappedChunk;
    Uint8 m_MappedCount;
    Uint8 m_UnmappedCount;
    vector<CBGZFPos> m_Intervals;
};


class NCBI_BAMREAD_EXPORT CBamIndex
{
public:
    CBamIndex();
    explicit
    CBamIndex(const string& index_file_name);
    ~CBamIndex();

    void Read(const string& index_file_name);

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

    CBGZFRange GetTotalFileRange(size_t ref_index) const;

    CRef<CSeq_annot>
    MakeEstimatedCoverageAnnot(const CBamHeader& header,
                               const string& ref_name,
                               const string& seq_id,
                               const string& annot_name) const;
    CRef<CSeq_annot>
    MakeEstimatedCoverageAnnot(const CBamHeader& header,
                               const string& ref_name,
                               const CSeq_id& seq_id,
                               const string& annot_name) const;


    CRef<CSeq_annot>
    MakeEstimatedCoverageAnnot(size_t ref_index,
                               const string& seq_id,
                               const string& annot_name,
                               TSeqPos ref_length = kInvalidSeqPos) const;
    CRef<CSeq_annot>
    MakeEstimatedCoverageAnnot(size_t ref_index,
                               const CSeq_id& seq_id,
                               const string& annot_name,
                               TSeqPos ref_length = kInvalidSeqPos) const;

private:
    TRefs m_Refs;
    Uint8 m_UnmappedCount;
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


class NCBI_BAMREAD_EXPORT CBamFileRangeSet
{
public:
    CBamFileRangeSet();
    CBamFileRangeSet(const CBamIndex& index,
                     size_t ref_index, COpenRange<TSeqPos> ref_range);
    ~CBamFileRangeSet();

    void Clear()
        {
            m_Ranges.clear();
        }
    void SetRanges(const CBamIndex& index,
                   size_t ref_index, COpenRange<TSeqPos> ref_range);
    void AddRanges(const CBamIndex& index,
                   size_t ref_index, COpenRange<TSeqPos> ref_range);

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

protected:
    typedef SBamIndexBinInfo::TBin TBin;
    
    void AddSortedRanges(const vector<CBGZFRange>& ranges);
    void AddBinsRanges(vector<CBGZFRange>& ranges,
                       const CBGZFRange& limit,
                       const SBamIndexRefIndex& ref,
                       TBin bin1, TBin bin2);
    
private:
    TRanges m_Ranges;
};


class NCBI_BAMREAD_EXPORT CBamRawDb
{
public:
    CBamRawDb()
        {
        }
    CBamRawDb(const string& bam_path, const string& index_path)
        {
            Open(bam_path, index_path);
        }
    ~CBamRawDb();


    void Open(const string& bam_path, const string& index_path);


    const CBamHeader& GetHeader() const
        {
            return m_Header;
        }
    const CBamIndex& GetIndex() const
        {
            return m_Index;
        }
    size_t GetRefIndex(const string& ref_label) const
        {
            return GetHeader().GetRefIndex(ref_label);
        }


    CBGZFFile& GetFile()
        {
            return *m_File;
        }

private:
    CRef<CBGZFFile> m_File;
    CBamHeader m_Header;
    CBamIndex m_Index;
};


struct NCBI_BAMREAD_EXPORT SBamAlignInfo
{
    void Read(CBGZFStream& in);
    
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
    uint16_t get_cigar_ops_count() const
        {
            return SBamUtil::MakeUint2(get_record_ptr()+12);
        }
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
            return get_read_name_ptr()+get_read_name_len();
        }
    const char* get_cigar_ptr() const
        {
            return get_read_name_end();
        }
    const char* get_cigar_end() const
        {
            return get_cigar_ptr()+get_cigar_ops_count()*4;
        }
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
    const char* get_qual_ptr() const
        {
            return get_phred_quality_end();
        }
    const char* get_qual_end() const
        {
            return get_record_end();
        }

    string get_read() const;
    uint32_t get_cigar_ref_size() const;
    string get_cigar() const;

private:
    const char* m_RecordPtr;
    Uint4 m_RecordSize;
};


class NCBI_BAMREAD_EXPORT CBamRawAlignIterator
{
public:
    CBamRawAlignIterator()
        : m_CurrentRangeEnd(0)
        {
        }
    CBamRawAlignIterator(CBamRawDb& bam_db,
                         const string& ref_label,
                         CRange<TSeqPos> ref_range)
        : m_Reader(bam_db.GetFile())
        {
            Select(bam_db, ref_label, ref_range);
        }
    ~CBamRawAlignIterator()
        {
        }

    DECLARE_OPERATOR_BOOL(m_CurrentRangeEnd.GetVirtualPos() != 0);

    void Select(CBamRawDb& bam_db,
                const string& ref_label,
                CRange<TSeqPos> ref_range)
        {
            x_Select(bam_db.GetIndex(),
                     bam_db.GetRefIndex(ref_label), ref_range);
        }
    void Select(const CBamIndex& index,
                size_t ref_index,
                CRange<TSeqPos> ref_range)
        {
            x_Select(index, ref_index, ref_range);
        }
    void Next()
        {
            if ( x_NextAnnot() ) {
                x_Settle();
            }
        }

    CBamRawAlignIterator& operator++()
        {
            Next();
            return *this;
        }

    TSeqPos GetRefSeqPos() const
        {
            return m_AlignInfo.get_ref_pos();
        }
    TSeqPos GetCIGARRefSize() const
        {
            return m_AlignInfo.get_cigar_ref_size();
        }

protected:
    void x_Select(const CBamIndex& index,
                  size_t ref_index, CRange<TSeqPos> ref_range);
    bool x_UpdateRange();
    bool x_NextAnnot()
        {
            _ASSERT(*this);
            if ( !(m_Reader.GetPos() < m_CurrentRangeEnd) ) {
                if ( !x_UpdateRange() )
                    return false;
            }
            return true;
        }
    void x_Stop()
        {
            m_NextRange = m_Ranges.end();
            m_CurrentRangeEnd = CBGZFPos(0);
        }
    bool x_NeedToSkip();
    void x_Settle();
    
private:
    size_t m_RefIndex;
    CRange<TSeqPos> m_RefRange;
    SBamAlignInfo m_AlignInfo;
    CBamFileRangeSet m_Ranges;
    CBamFileRangeSet::const_iterator m_NextRange;
    CBGZFPos m_CurrentRangeEnd;
    CBGZFStream m_Reader;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // SRA__READER__BAM__BAMINDEX__HPP
