#ifndef ALGO_SEQUENCE___GAP_ANALYSIS__HPP
#define ALGO_SEQUENCE___GAP_ANALYSIS__HPP

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
* Authors:  Michael Kornbluh, NCBI
*
*/

/// @file gap_analysis.hpp
/// Analyzes gaps and produces various statistics.

#include <objects/seq/Seq_inst.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <util/histogram_binning.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_id;
class CSeq_entry_Handle;
class CBioseq_Handle;

/// Give this gaps and then you can get statistics on those gaps.
class NCBI_XALGOSEQ_EXPORT CGapAnalysis {
public:

    typedef CConstRef<CSeq_id> TSeqIdConstRef;
    typedef Int8 TGapLength;

    /// Calls AddGap for each anywhere under the given CSeq_entry.  In
    /// most cases, much more convenient than raw AddGap calls.
    void AddSeqEntryGaps(const CSeq_entry_Handle & entry_h,
        CSeq_inst::EMol filter = CSeq_inst::eMol_not_set,
        CBioseq_CI::EBioseqLevelFlag level = CBioseq_CI::eLevel_All);

    /// Similar to AddSeqEntryGaps, but for one Bioseq.
    void AddBioseqGaps(const CBioseq_Handle & bioseq_h);

    /// AddSeqEntryGaps is more convenient, but if you want finer-grained
    /// control you can use this function to tell this class about 
    /// another gap that is found, updating the data.
    void AddGap( TSeqIdConstRef pSeqId, TGapLength iGapLength );

    /// Start analysis over again.
    void Clear(void);

    enum ESortGapLength {
        eSortGapLength_Length, ///< Sort by gap length
        eSortGapLength_NumSeqs, ///< Sort gap lengths by number of sequences that contain one or more gaps of the given length.
        eSortGapLength_NumGaps ///< Sort gap lengths by number of times they appear anywhere.
    };
    enum ESortDir {
        eSortDir_Ascending,
        eSortDir_Descending
    };

    struct NCBI_XALGOSEQ_EXPORT SGapLengthSummary {
        SGapLengthSummary(
            TGapLength gap_length_arg,
            size_t   num_seqs_arg,
            size_t   num_gaps_arg )
            : gap_length(gap_length_arg),
              num_seqs(num_seqs_arg),
              num_gaps(num_gaps_arg) { }

        TGapLength gap_length;
        size_t   num_seqs; ///< number of sequences which contain one or more gaps of the given length
        size_t   num_gaps; ///< number of times gaps of this length appear anywhere
    };
    typedef vector<SGapLengthSummary> TVectorGapLengthSummary;

    AutoPtr<TVectorGapLengthSummary> GetGapLengthSummary(
        ESortGapLength eSortGapLength = eSortGapLength_Length,
        ESortDir eSortDir = eSortDir_Ascending) const;

    typedef set<TSeqIdConstRef> TSetSeqIdConstRef;
    typedef map<TGapLength, TSetSeqIdConstRef> TMapGapLengthToSeqIds;

    /// Returns a map of gap_length to the set of all seq-ids that
    /// contain at least one gap of that length.
    const TMapGapLengthToSeqIds & GetGapLengthSeqIds(void) const {
        return m_mapGapLengthToSeqIds;
    }

    AutoPtr<CHistogramBinning::TListOfBins>
        GetGapHistogram(size_t num_bins = 0, 
        CHistogramBinning::EHistAlgo eHistAlgo = CHistogramBinning::eHistAlgo_IdentifyClusters);

private:
    /// For each unique gap length, this holds all the seq-ids which have one
    /// or more gaps of that length.
    TMapGapLengthToSeqIds m_mapGapLengthToSeqIds;

    typedef map<TGapLength, size_t> TMapGapLengthToNumAppearances;
    /// For each unique gap length, this holds how many times a gap
    /// of that length appears anywhere.
    TMapGapLengthToNumAppearances m_mapGapLengthToNumAppearances;

    CHistogramBinning m_histogramBinner;

    class SGapLengthSummarySorter {
    public:
        SGapLengthSummarySorter(
            ESortGapLength sort_gap_length_arg,
            ESortDir       sort_dir_arg );

        bool operator()(const SGapLengthSummary & lhs,
            const SGapLengthSummary & rhs ) const;

    private:
        ESortGapLength sort_gap_length;
        ESortDir       sort_dir;
    };
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* ALGO_SEQUENCE___GAP_ANALYSIS__HPP */
