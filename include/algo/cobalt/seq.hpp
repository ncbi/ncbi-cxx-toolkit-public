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

File name: seq.hpp

Author: Jason Papadopoulos

Contents: Interface for CSequence class

******************************************************************************/

/// @file seq.hpp
/// Interface for CSequence class

#ifndef _ALGO_COBALT_SEQ_HPP_
#define _ALGO_COBALT_SEQ_HPP_

#include <util/math/matrix.hpp>
#include <algo/blast/api/sseqloc.hpp>
#include <algo/align/nw/nw_aligner.hpp>
#include <algo/cobalt/base.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)

/// Class for representing protein sequences
class CSequence {

public:
    /// The ncbistdaa code for a gap
    static const unsigned char kGapChar = 0;

    /// Represents the sequence as position-specific 
    /// amino acid frequencies. Matrix is of dimension
    /// (sequence length) x kAlphabetSize
    typedef CNcbiMatrix<double> TFreqMatrix;

    /// Default constructor: build an empty sequence
    ///
    CSequence() {}

    /// Build a sequence
    /// @param seq The input sequence. The seqloc within 'seq'
    ///            must be of type 'whole' [in]
    ///
    CSequence(const blast::SSeqLoc& seq);

    /// Replace the sequence represented by a CSequence object
    /// @param seq The new sequence [in]
    ///
    void Reset(const blast::SSeqLoc& seq);

    /// Access the list of position frequencies associated
    /// with a sequence
    /// @return The frequency matrix
    ///
    TFreqMatrix& GetFreqs() { return m_Freqs; }

    /// Access the raw sequence data, in ncbistdaa format
    /// @return Pointer to array of sequence data
    ///
    unsigned char *GetSequence() { return &m_Sequence[0]; }

    /// Access the sequence letter at a specified position
    /// @param pos Position to access [in]
    /// @return The sequence letter
    ///
    unsigned char GetLetter(int pos) const { return m_Sequence[pos]; }

    /// Access the sequence letter at a specified position,
    /// and return an ASCII representation of that letter
    /// @param pos Position to access [in]
    /// @return The ASCII sequence letter
    /// 
    unsigned char GetPrintableLetter(int pos) const;

    /// Get the length of the current sequence
    /// @return Sequence length
    ///
    int GetLength() const { return m_Sequence.size(); }

    /// Given an edit script, insert gaps into a sequence. The
    /// profile of the sequence is adjusted automatically, and
    /// new gaps are assigned a column of all zero profile frequencies
    /// @param transcript The edit script to apply [in]
    /// @param gap_choice Which edit script character, eTS_Delete or
    ///                   eTS_Insert, will cause a gap to be inserted [in]
    ///
    void PropagateGaps(const CNWAligner::TTranscript& transcript,
                       CNWAligner::ETranscriptSymbol gap_choice);

    /// Given a collection of sequences, remove all sequence positions
    /// where a subset of the sequences all contain a gap 
    /// @param seq The list of sequences [in]
    /// @param index_list List of elements of 'seq' that will be
    ///                   compressed. The other elements of 'seq' will
    ///                   not be changed [in]
    ///
    static void CompressSequences(vector<CSequence>& seq, 
                                  vector<int> index_list);

    /// Compute the score derived from aligning seq1 with seq2.
    /// Affine gap penalties are assessed
    /// @param seq1 The first sequence [in]
    /// @param seq2 The second sequence [in]
    /// @param matrix The score matrix to use [in]
    /// @param gap_open Penalty for starting a gap in one sequence [in]
    /// @param gap_open Penalty for extending a gap in one sequence [in]
    /// @return The alignment score
    ///
    static int GetPairwiseScore(CSequence& seq1, 
                            CSequence& seq2,
                            SNCBIFullScoreMatrix& matrix,
                            int gap_open,
                            int gap_extend);

    /// Compute the sum of the scores derived from aligning
    /// all sequence pairs in a list
    /// @param list The list of sequences [in]
    /// @param matrix The score matrix to use [in]
    /// @param gap_open Penalty for starting a gap in one sequence [in]
    /// @param gap_open Penalty for extending a gap in one sequence [in]
    /// @return The alignment score
    ///
    static int GetSumOfPairs(vector<CSequence>& list,
                         SNCBIFullScoreMatrix& matrix,
                         int gap_open,
                         int gap_extend);

private:
    vector<unsigned char> m_Sequence;  ///< The sequence (ncbistdaa format)
    TFreqMatrix m_Freqs;               ///< Position-specific frequency 
                                       ///  profile corresponding to sequence
};

END_SCOPE(cobalt)
END_NCBI_SCOPE

#endif // _ALGO_COBALT_SEQ_HPP_

/*--------------------------------------------------------------------
  $Log$
  Revision 1.4  2005/11/15 20:10:29  papadopo
  add doxygen

  Revision 1.3  2005/11/14 16:17:40  papadopo
  remove unneeded member

  Revision 1.2  2005/11/08 17:42:17  papadopo
  Rearrange includes to be self-sufficient

  Revision 1.1  2005/11/07 18:15:52  papadopo
  Initial revision

--------------------------------------------------------------------*/
