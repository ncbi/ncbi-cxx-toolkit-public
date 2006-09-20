#ifndef ALGO_COBALT___HIT__HPP
#define ALGO_COBALT___HIT__HPP

/* $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

File name: hit.hpp

Author: Jason Papadopoulos

Contents: Interface for CHit class

******************************************************************************/

/// @file hit.hpp
/// Interface for CHit class, used to encapsulate
/// operations involving pairwise alignments.
/// <pre>
/// Given an alignment with its traceback, this class can
/// compute subsets of the alignment from subsets of the traceback.
/// This can sometimes be tricky; for example, given an alignment
/// described by
///
///                       11111
/// traceback   012345678901234
///
/// query   10  AAAAA---AAAAAAA 21
/// subject 50  AAAAAAAAA--AAAA 62
///
/// the traceback is in the range 0 to 14, the query range is 10 to 21,
/// and the subject range is 50 to 62. For a subject range of [50,54] the
/// query range is [10,14]. However, a subject range of [50,56] still has
/// a query range of [10,14]. Some other examples:
///
///  start with        of        compute        of
///  -------------   -------    -------------   -----
///  query range     [12,15]    subject range   [52,58]
///  query range     [15,16]    subject range   [58,59]
///  subject range   [55,56]    query range     [15,14] <- inverted!
///
/// In general, query ranges and subject ranges are assumed to exclude 
/// gaps at either end of the range. For scoring purposes, the traceback 
/// range is what's specified, as this is unambiguous.
/// </pre>


#include <objects/seqalign/Dense_seg.hpp>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/gapinfo.h>
#include <algo/align/nw/nw_aligner.hpp>

#include <algo/cobalt/base.hpp>
#include <algo/cobalt/seq.hpp>
#include <algo/cobalt/traceback.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)

/// A generalized representation of a pairwise alignment
class NCBI_COBALT_EXPORT CHit
{
public:
    /// Not always used, but useful to avoid 
    /// extremely small hits
    static const int kMinHitSize = 2;
 
    /// Hits can be grouped hierarchically
    typedef vector<CHit *> TSubHit;
 
    /// Numerical identifier for first sequence 
    /// in alignment
    int m_SeqIndex1;
 
    /// Numerical identifier for second sequence 
    /// in alignment
    int m_SeqIndex2;
 
    /// Score of alignment
    int m_Score;
 
    /// The range of offsets on the first sequence
    TRange m_SeqRange1;
 
    /// The range of offsets on the second sequence
    TRange m_SeqRange2;
 
    /// Create an empty alignment
    /// @param seq1_index Numerical identifier for first sequence [in]
    /// @param seq2_index Numerical identifier for second sequence [in]
    ///
    CHit(int seq1_index, int seq2_index) 
           : m_SeqIndex1(seq1_index), m_SeqIndex2(seq2_index), 
             m_Score(0), m_SeqRange1(0,0), m_SeqRange2(0,0) {}
 
    /// Create an alignment from a BLAST hit
    /// @param seq1_index Numerical identifier for first sequence [in]
    /// @param seq2_index Numerical identifier for second sequence [in]
    /// @param hsp A single pairwise alignment from a blast hit [in]
    ///
    CHit(int seq1_index, int seq2_index, BlastHSP *hsp)
           : m_SeqIndex1(seq1_index), m_SeqIndex2(seq2_index), 
             m_Score(hsp->score),
             m_SeqRange1(hsp->query.offset, hsp->query.end - 1),
             m_SeqRange2(hsp->subject.offset, hsp->subject.end - 1),
             m_EditScript(hsp->gap_info) { VerifyHit(); }
 
    /// Create an alignment from a Seq-align
    /// @param seq1_index Numerical identifier for first sequence [in]
    /// @param seq2_index Numerical identifier for second sequence [in]
    /// @param score The score of the pairwise alignment [in]
    /// @param sa Seq-align representing a single pairwise alignment 
    ///           from a blast hit [in]
    ///
    CHit(int seq1_index, int seq2_index, int score,
         const objects::CDense_seg& denseg);
 
    /// Create an alignment with all specified parameters
    /// @param seq1_index Numerical identifier for first sequence [in]
    /// @param seq2_index Numerical identifier for second sequence [in]
    /// @param seq_range1 Offsets on the first sequence [in]
    /// @param seq_range2 Offsets on the second sequence [in]
    /// @param score The score of the alignment [in]
    /// @param edit_script Traceback for the alignment (may be empty) [in]
    /// 
    CHit(int seq1_index, int seq2_index,
         TRange seq_range1, TRange seq_range2,
         int score, CEditScript edit_script)
        : m_SeqIndex1(seq1_index), m_SeqIndex2(seq2_index),
          m_Score(score),
          m_SeqRange1(seq_range1), m_SeqRange2(seq_range2),
          m_EditScript(edit_script) { VerifyHit(); }
 
    /// Destructor
    ///
    ~CHit() 
    {
        // delete sub-hits
        for (int i = 0; i < (int)m_SubHit.size(); i++)
             delete m_SubHit[i];
    }
 
    /// Add a to a CHit's list of subhits
    /// @param hit The hit to add [in]
    ///
    void InsertSubHit(CHit *hit) { m_SubHit.push_back(hit); }
 
    /// Retrieve a list of subhits
    /// @return The list of subhits
    ///
    TSubHit& GetSubHit() { return m_SubHit; }
 
    /// Retrieve the traceback associated with a CHit
    /// @return The traceback
    ///
    CEditScript& GetEditScript() { return m_EditScript; }
 
    /// Query if a CHit has a hierarchy of subhits available
    /// @return true if subhits are available
    ///
    bool HasSubHits() { return !(m_SubHit.empty()); }
 
    /// Sum the score of all subhits, and make the sequence ranges
    /// the union of the ranges of all subhits. Traceback is ignored
    ///
    void AddUpSubHits();
 
    /// Produce an independent copy of a CHit
    /// @return Pointer to the copy
    ///
    CHit * Clone();

    /// Retrieve the seq1 range corresponding to a 
    /// specified seq2 range. Assumes traceback is valid
    /// @param seq_range2 The target range on the 
    ///                   second sequence [in]
    /// @param seq_range1 The corresponding range 
    ///                   on the first sequence [out]
    /// @param new_seq_range2 If seq_range2 starts or ends in a
    ///                    gap on seq_range1, the range is shortened
    ///                    to exclude the gap and seq_range2 is
    ///                    cropped to compensate [out]
    /// @param traceback_range The range of traceback operations
    ///                   corresponding to seq_range1 and 
    ///                   new_seq_range2 [out]
    ///
    void GetRangeFromSeq2(TRange seq_range2,
                          TRange& seq_range1,
                          TRange& new_seq_range2,
                          TRange& traceback_range);
    
    /// Retrieve the seq2 range corresponding to a 
    /// specified seq1 range. Assumes traceback is valid
    /// @param seq_range1 The target range on the 
    ///                   first sequence [in]
    /// @param new_seq_range1 If seq_range1 starts or ends in a
    ///                    gap on seq_range2, the range is shortened
    ///                    to exclude the gap and seq_range1 is
    ///                    cropped to compensate [out]
    /// @param seq_range2 The corresponding range 
    ///                   on the second sequence [out]
    /// @param traceback_range The range of traceback operations
    ///                   corresponding to seq_range1 and 
    ///                   new_seq_range2 [out]
    ///
    void GetRangeFromSeq1(TRange seq_range1,
                          TRange& new_seq_range1,
                          TRange& seq_range2,
                          TRange& traceback_range);

    /// Perform basic integrity checks on a CHit
    ///
    void VerifyHit();

    /// If pairs of subhits have overlapping ranges, either delete
    /// one or change one so that the overlap is avoided. Only the
    /// sequence 1 range is checked for overlap; in practice, the hits 
    /// refer to block alignments derived from RPS blast results, 
    /// and sequence 2 is an RPS database sequence. It is sequence 1 
    /// that matters for later processing
    /// @param seq1 The sequence data corresponding to the 
    ///             first sequence [in]
    /// @param seq2_pssm The PSSM for the second sequence [in]
    /// @param gap_open Penalty for opening a gap [in]
    /// @param gap_extend Penalty for extending a gap [in]
    ///
    void ResolveSubHitConflicts(CSequence& seq1,
                                int **seq2_pssm,
                                CNWAligner::TScore gap_open,
                                CNWAligner::TScore gap_extend);

private:
   CEditScript m_EditScript;  ///< Traceback for this alignment
   vector<CHit *> m_SubHit;   ///< Subhits for this alignment
};


END_SCOPE(cobalt)
END_NCBI_SCOPE

/* ====================================================================
 * $Log$
 * Revision 1.7  2006/09/20 19:43:51  papadopo
 * add member to convert from a CDense_seg
 *
 * Revision 1.6  2006/03/22 19:23:17  dicuccio
 * Cosmetic changes: adjusted include guards; formatted CVS logs; added export
 * specifiers
 *
 * Revision 1.5  2005/11/18 20:15:58  papadopo
 * remove m_BitScore
 *
 * Revision 1.4  2005/11/17 22:32:18  papadopo
 * remove hit rate member; also change Copy() to Clone()
 *
 * Revision 1.3  2005/11/15 23:24:15  papadopo
 * add doxygen
 *
 * Revision 1.2  2005/11/08 17:42:17  papadopo
 * Rearrange includes to be self-sufficient
 *
 * Revision 1.1  2005/11/07 18:15:52  papadopo
 * Initial revision
 *
 * ====================================================================
 */

#endif // ALGO_COBALT___HIT__HPP
