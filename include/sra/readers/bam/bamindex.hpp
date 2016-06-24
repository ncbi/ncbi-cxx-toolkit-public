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

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_annot;

struct SBamRefHeaderInfo
{
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

    void Read(const string& bam_file_name);

    const string& GetText() const
        {
            return m_Text;
        }

    typedef vector<SBamRefHeaderInfo> TRefs;
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
    
    static SBamRefHeaderInfo ReadRef(CNcbiIstream& in);

private:
    string m_Text;
    map<string, size_t> m_RefByName;
    TRefs m_Refs;
};


struct NCBI_BAMREAD_EXPORT SBamIndexBinInfo
{
    typedef Uint4 TBin;

    static COpenRange<TSeqPos> GetSeqRange(TBin bin);
    COpenRange<TSeqPos> GetSeqRange() const
        {
            return GetSeqRange(m_Bin);
        }

    TBin m_Bin;
    vector<CBGZFRange> m_Chunks;
};


struct NCBI_BAMREAD_EXPORT SBamIndexRefIndex
{
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
    vector<SBamIndexRefIndex> m_Refs;
    Uint8 m_UnmappedCount;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // SRA__READER__BAM__BAMINDEX__HPP
