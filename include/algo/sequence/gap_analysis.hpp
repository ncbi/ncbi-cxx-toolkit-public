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

/// Give this gaps, or handles containing gaps and then you can get 
/// statistics on those gaps.
class NCBI_XALGOSEQ_EXPORT CGapAnalysis {
public:

    /// Convenience typedef.
    typedef CConstRef<CSeq_id> TSeqIdConstRef;
    /// Use typedef in case we change the underlying
    typedef Uint8 TGapLength;

    /// Calls AddGap for each gap anywhere under the given CSeq_entry.  In
    /// most cases, much more convenient than raw AddGap calls.
    ///
    /// @param entry_h
    ///   The Seq-entry to explore
    /// @param filter
    ///   Pick exactly which molecule types to look at when exploring.
    ///   It works the same way as it does for CBioseq_CI.
    /// @param level
    ///   Filter what bioseq levels to explore.
    void AddSeqEntryGaps(const CSeq_entry_Handle & entry_h,
        CSeq_inst::EMol filter = CSeq_inst::eMol_not_set,
        CBioseq_CI::EBioseqLevelFlag level = CBioseq_CI::eLevel_All);

    /// Similar to AddSeqEntryGaps, but for one Bioseq.
    ///
    /// @param bioseq_h
    ///   This is the bioseq to explore for gaps.
    void AddBioseqGaps(const CBioseq_Handle & bioseq_h);

    /// AddSeqEntryGaps is more convenient, but if you want finer-grained
    /// control you can use this function to tell this class about 
    /// another gap that is found, updating the data.
    ///
    /// @param pSeqId
    ///   This is the Seq-id  that contains the given gap
    /// @param iGapLength
    ///   This is, of course, the length of the gap.
    void AddGap( TSeqIdConstRef pSeqId, TGapLength iGapLength );

    /// Start analysis over again.  This is pretty much the same
    /// as destroying this CGapAnalysis object and creating it anew.
    void clear(void);

    /// Use this to control what results are sorted on.
    /// (For ties, "length" is the tie-breaker, since it's
    /// guaranteed unique)
    enum ESortGapLength {
        eSortGapLength_Length, ///< Sort by gap length
        eSortGapLength_NumSeqs, ///< Sort gap lengths by number of sequences that contain one or more gaps of the given length.
        eSortGapLength_NumGaps ///< Sort gap lengths by number of times they appear anywhere.
    };
    /// This controls the direction of sort order for functions
    /// that also take ESortGapLength.
    enum ESortDir {
        eSortDir_Ascending,
        eSortDir_Descending
    };

    /// This holds information about a given gap_length.
    /// See the fields of this struct for details.
    struct NCBI_XALGOSEQ_EXPORT SOneGapLengthSummary {
        SOneGapLengthSummary(
            TGapLength gap_length_arg,
            Uint8   num_seqs_arg,
            Uint8   num_gaps_arg )
            : gap_length(gap_length_arg),
              num_seqs(num_seqs_arg),
              num_gaps(num_gaps_arg) { }

        TGapLength gap_length;
        Uint8   num_seqs; ///< number of sequences which contain one or more gaps of the given length
        Uint8   num_gaps; ///< number of times gaps of this length appear anywhere
    };
    /// This holds the information for every encountered gap length.
    typedef vector<SOneGapLengthSummary> TVectorGapLengthSummary;

    /// This gives summary information about every gap-length
    /// encountered so far.
    ///
    /// NOTE: Caller must free returned object!
    ///
    /// @return
    ///   newly-allocated summary information.  Caller must destroy this.
    /// @param eSortGapLength
    ///   what to sort the return value on
    /// @param
    ///   direction of sorting for return value
    TVectorGapLengthSummary * GetGapLengthSummary(
        ESortGapLength eSortGapLength = eSortGapLength_Length,
        ESortDir eSortDir = eSortDir_Ascending) const;

    /// Use this to compare the contents of TSeqIdConstRef instead
    /// of the pointer itself.
    struct SSeqIdRefLessThan {
        /// Performs less-than on referenced CSeq_ids
        bool operator()( const TSeqIdConstRef & lhs,
            const TSeqIdConstRef & rhs ) const
        {
            return *lhs < *rhs;
        }
    };

    /// Holds a set of unique CSeq_ids
    typedef set<TSeqIdConstRef, SSeqIdRefLessThan> TSetSeqIdConstRef;
    /// For each gap-length, maps to all the CSeq_ids that have at
    /// least one gap of that length.
    typedef map<TGapLength, TSetSeqIdConstRef> TMapGapLengthToSeqIds;

    /// Returns a map of gap_length to the set of all seq-ids that
    /// contain at least one gap of that length.
    const TMapGapLengthToSeqIds & GetGapLengthSeqIds(void) const {
        return m_mapGapLengthToSeqIds;
    }

    /// This returns a histogram of gap length vs. number of times
    /// a gap of a length in that range appears.
    ///
    /// NOTE: Caller must free returned object!
    /// 
    /// @return
    ///   the resulting histogram.  Caller must free this.
    /// @param num_bins
    ///   The target number of bins for the histogram.  This is only
    ///   a suggestion and the actual result may have somewhat more
    ///   or fewer bins.  0 means to automatically pick a reasonable
    ///   number of bins.
    /// @param eHistAlgo
    ///   This is the algorithm to use for histogram binning.  The
    ///   default should usually be fine.
    CHistogramBinning::TListOfBins *
        GetGapHistogram(Uint8 num_bins = 0, 
        CHistogramBinning::EHistAlgo eHistAlgo = 
            CHistogramBinning::eHistAlgo_Default);

private:
    /// For each unique gap length, this holds all the seq-ids which have one
    /// or more gaps of that length.
    TMapGapLengthToSeqIds m_mapGapLengthToSeqIds;

    typedef map<TGapLength, Uint8> TMapGapLengthToNumAppearances;
    /// For each unique gap length, this holds how many times a gap
    /// of that length appears anywhere.
    TMapGapLengthToNumAppearances m_mapGapLengthToNumAppearances;

    /// This holds data for purposes of histogram creation.
    CHistogramBinning m_histogramBinner;

    /// This class does comparison of SOneGapLengthSummary,
    /// and it is adjustable which field to sort on
    /// and which direction to sort.
    class SOneGapLengthSummarySorter {
    public:
        SOneGapLengthSummarySorter(
            ESortGapLength sort_gap_length_arg,
            ESortDir       sort_dir_arg );

        /// less-than
        bool operator()(const SOneGapLengthSummary & lhs,
            const SOneGapLengthSummary & rhs ) const;

    private:
        ESortGapLength sort_gap_length;
        ESortDir       sort_dir;
    };
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* ALGO_SEQUENCE___GAP_ANALYSIS__HPP */
