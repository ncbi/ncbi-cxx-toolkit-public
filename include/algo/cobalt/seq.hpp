#ifndef ALGO_COBALT___SEQ__HPP
#define ALGO_COBALT___SEQ__HPP

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

#include <util/math/matrix.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <algo/blast/api/sseqloc.hpp>
#include <algo/align/nw/nw_aligner.hpp>
#include <algo/cobalt/base.hpp>
#include <algo/cobalt/exception.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)

/// Class for representing protein sequences
class NCBI_COBALT_EXPORT CSequence
{

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
    /// @param seq The input sequence.
    ///
    CSequence(const objects::CSeq_loc& seq, objects::CScope& scope);

    /// Replace the sequence represented by a CSequence object
    /// @param seq The new sequence [in]
    ///
    void Reset(const objects::CSeq_loc& seq, objects::CScope& scope);

    /// Replace the sequence with sequence of gaps of given length
    /// @param length Number of gaps [in]
    ///
    void Reset(int length);


    /// Access the list of position frequencies associated
    /// with a sequence
    /// @return The frequency matrix
    ///
    TFreqMatrix& GetFreqs() { return m_Freqs; }

    const TFreqMatrix& GetFreqs() const { return m_Freqs; }

    /// Access the raw sequence data, in ncbistdaa format
    /// @return Pointer to array of sequence data
    ///
    unsigned char* GetSequence() { return &m_Sequence[0]; }

    /// Get the raw sequence data in ncbistdaa format
    /// @return Pointer to array of sequence data
    ///
    const unsigned char* GetSequence() const { return &m_Sequence[0]; }

    /// Access the sequence letter at a specified position
    /// @param pos Position to access [in]
    /// @return The sequence letter
    ///
    unsigned char GetLetter(int pos) const { return m_Sequence[pos]; }

    /// Change letter in a given position to a given one
    /// @param pos Position in the sequence [in]
    /// @param letter Letter [in]
    ///
    void SetLetter(int pos, unsigned char letter) { m_Sequence[pos] = letter; }

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

    /// Insert gaps into a sequence. Gap is inserted before each given 
    /// location. The profile of the sequence is adjusted automatically, 
    /// and new gaps are assigned a column of all zero profile frequencies.
    /// @param gap_locations Locations of single gaps
    /// @param consider_gaps If false location n denotes before n-th letter,
    /// otherwise simple position in a string, each location considers gaps
    /// with smaller locations already added
    ///
    void InsertGaps(const vector<Uint4>& gap_locations, 
                    bool consider_gaps = false);


    /// Given a collection of sequences, remove all sequence positions
    /// where a subset of the sequences all contain a gap 
    /// @param seq The list of sequences [in]
    /// @param index_list List of elements of 'seq' that will be
    ///                   compressed. The other elements of 'seq' will
    ///                   not be changed [in]
    ///
    static void CompressSequences(vector<CSequence>& seq, 
                                  vector<int> index_list);

    /// Create a vector of CSequence objects that represents the alignment in
    /// given Seq_align
    /// @param seq_align Alignment in ASN.1 format [in]
    /// @param scope Scope [in]
    /// @param msa Alignment as strings of residues and gaps [out]
    ///
    static void CreateMsa(const objects::CSeq_align& seq_align,
                          objects::CScope& scope,
                          vector<CSequence>& msa);

private:
    vector<unsigned char> m_Sequence;  ///< The sequence (ncbistdaa format)
    TFreqMatrix m_Freqs;               ///< Position-specific frequency 
                                       ///  profile corresponding to sequence
};

END_SCOPE(cobalt)
END_NCBI_SCOPE

#endif // ALGO_COBALT___SEQ__HPP
