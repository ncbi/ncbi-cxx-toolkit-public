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
 * Author: Christiam Camacho
 *
 */

/** @file seqalign_cmp.hpp
 * API to compare CSeq-aligns produced by BLAST
 */

#ifndef _SEQALIGN_CMP_HPP
#define _SEQALIGN_CMP_HPP

#include <corelib/ncbistl.hpp>
#include <string>
#include "neutral_seqalign.hpp"

BEGIN_SCOPE(ncbi)

// Forward declarations
BEGIN_SCOPE(objects)
    class CSeq_align_set;
END_SCOPE(objects)

BEGIN_SCOPE(blast)
BEGIN_SCOPE(qa)

/// Configuration options for CSeqAlignCmp class
class CSeqAlignCmpOpts {
public:
    /** Configuration options for CSeqAlignCmp class.
     * @param conf_opts bitwise OR of EConf values
     * @param min_evalue Minimum evalue to trigger comparison of an alignment
     * @param max_evalue Maximum evalue to trigger comparison of an alignment
     * @param max_evalue_diffs Maximum acceptable difference in evalues
     * @param max_score_diffs Maximum acceptable difference in scores and bit
     * scores
     * @param max_lengths_diffs Maximum acceptable difference in alignment
     * lengths
     * @param max_offset_diffs Maximum acceptable difference in alignment
     * offsets
     */
    CSeqAlignCmpOpts(double max_evalue_diffs    = 1e-3,
                     double min_evalue          = 0.0,
                     double max_evalue          = 10.0,
                     int max_score_diffs        = 1,
                     int max_lengths_diffs      = 1,
                     int max_offset_diffs       = 1)
    : m_MinEvalue(min_evalue), m_MaxEvalue(max_evalue), 
    m_MaxEvalueDiffs(max_evalue_diffs), m_MaxScoreDiffs(max_score_diffs),
    m_MaxLengthsDiffs(max_lengths_diffs), m_MaxOffsetDiffs(max_offset_diffs)
    {}

    double GetMinEvalue() const {
        return m_MinEvalue;
    }
    double GetMaxEvalue() const {
        return m_MaxEvalue;
    }
    double GetMaxEvalueDiff() const {
        return m_MaxEvalueDiffs;
    }
    int GetMaxScoreDiff() const {
        return m_MaxScoreDiffs;
    }
    int GetMaxLengthDiff() const {
        return m_MaxLengthsDiffs;
    }
    int GetMaxOffsetDiff() const {
        return m_MaxOffsetDiffs;
    }

private:
    double m_MinEvalue;
    double m_MaxEvalue;
    double m_MaxEvalueDiffs;
    int m_MaxScoreDiffs;
    int m_MaxLengthsDiffs;
    int m_MaxOffsetDiffs;
};

/// Class to perform BLAST sequence alignment comparisons
class CSeqAlignCmp {
public:
    /// Parametrized constructor
    ///
    /// @param ref 
    ///     Reference Seq-align-set (assumed correct) [in]
    /// @param test
    ///     Test Seq-align-set (to be compared against ref) [in]
    /// @param conf
    ///     Configuration for this object [in]
    CSeqAlignCmp(const TSeqAlignSet& ref,
                 const TSeqAlignSet& test,
                 const CSeqAlignCmpOpts& options);
                 
    /// Main function for this object, compare the input Seq-aligns
    /// 
    /// @param errors
    ///     Optional argument which will contain a description of the errors
    ///     found [in|out]
    /// @return
    ///     True if alignments are equivalent else false
    bool Run(string* errors = NULL);

private:
    /// The sequence alignment to be used as reference (assumed correct)
    const TSeqAlignSet& m_Ref;

    /// The sequence alignment to be used as test (compared with reference)
    const TSeqAlignSet& m_Test;

    /// Our configuration options
    const CSeqAlignCmpOpts& m_Opts;

    // Need list of pairs of matched SeqAligns (HSPs believed to correspond to
    // one another and a list of unmatched SeqAligns (HSPs which didn't match)
    typedef pair<SeqAlign*, SeqAlign*> TMatch;
    typedef vector<TMatch> TMatchedAlignments;

    typedef vector<SeqAlign*> TUnmatchedAlignments;

    /// Prohibit copy constructor
    CSeqAlignCmp(const CSeqAlignCmp& rhs);

    /// Prohibit assignment operator
    CSeqAlignCmp& operator=(const CSeqAlignCmp& rhs);

    /** Compare alignment ref with alignment test (which correspond to entry
     * index in the TMatchedAlignments.
     * @param allow_fuzziness if false, exact comparisons are made, this should
     * be used in the algorithm to match the HSPs in the reference set with
     * those in the test set (i.e.: the least number of differences are likely
     * to correspond to the same match).
     * @return the number of fields which differs between test and ref
     */
    int x_CompareOneAlign(const SeqAlign* ref, 
                          const SeqAlign* test, 
                          int index,
                          string* errors = NULL,
                          bool allow_fuzziness = true);

    /** If reference and test alignments do not fall into the evalue range
     * specified, don't perform the comparison 
     * @param reference evalue assumed correct [in]
     * @param test evalue to be compared with reference [in]
     */
    bool x_MeetsEvalueRequirements(double reference, double test);
};

END_SCOPE(qa)
END_SCOPE(blast)
END_SCOPE(ncbi)

#endif /* _SEQALIGN_CMP_HPP */
