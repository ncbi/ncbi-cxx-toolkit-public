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

class CSequence {

public:
    static const unsigned char kGapChar = 0;
    typedef CNcbiMatrix<double> TFreqMatrix;

    CSequence() {}
    CSequence(const blast::SSeqLoc& seq);

    void Reset(const blast::SSeqLoc& seq);

    TFreqMatrix& GetFreqs() { return m_Freqs; }
    unsigned char *GetSequence() { return &m_Sequence[0]; }
    unsigned char GetLetter(int pos) const { return m_Sequence[pos]; }
    unsigned char GetPrintableLetter(int pos) const;
    int GetLength() const { return m_Sequence.size(); }

    void PropagateGaps(const CNWAligner::TTranscript& transcript,
                       CNWAligner::ETranscriptSymbol gap_choice);

    static void CompressSequences(vector<CSequence>& seq, 
                                  vector<int> index_list);

    static int GetPairwiseScore(CSequence& seq1, 
                            CSequence& seq2,
                            SNCBIFullScoreMatrix& matrix,
                            int gap_open,
                            int gap_extend);

    static int GetSumOfPairs(vector<CSequence>& list,
                         SNCBIFullScoreMatrix& matrix,
                         int gap_open,
                         int gap_extend);

private:
    vector<unsigned char> m_Sequence;
    TFreqMatrix m_Freqs;
};

END_SCOPE(cobalt)
END_NCBI_SCOPE

#endif // _ALGO_COBALT_SEQ_HPP_

/*--------------------------------------------------------------------
  $Log$
  Revision 1.3  2005/11/14 16:17:40  papadopo
  remove unneeded member

  Revision 1.2  2005/11/08 17:42:17  papadopo
  Rearrange includes to be self-sufficient

  Revision 1.1  2005/11/07 18:15:52  papadopo
  Initial revision

--------------------------------------------------------------------*/
