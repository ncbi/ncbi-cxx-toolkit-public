static char const rcsid[] = "$Id$";

/*
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

File name: seq.cpp

Author: Jason Papadopoulos

Contents: implementation of CSequence class

******************************************************************************/

#include <ncbi_pch.hpp>
#include <objtools/data_loaders/blastdb/bdbloader.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include <algo/cobalt/seq.hpp>

/// @file seq.cpp
/// Implementation of CSequence class

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)

unsigned char
CSequence::GetPrintableLetter(int pos) const
{
    _ASSERT(pos >= 0 && pos < GetLength());

    int val;
    switch (m_Sequence[pos]) {
    case 0: val = '-'; break;
    case 1: val = 'A'; break;
    case 2: val = 'B'; break;
    case 3: val = 'C'; break;
    case 4: val = 'D'; break;
    case 5: val = 'E'; break;
    case 6: val = 'F'; break;
    case 7: val = 'G'; break;
    case 8: val = 'H'; break;
    case 9: val = 'I'; break;
    case 10: val = 'K'; break;
    case 11: val = 'L'; break;
    case 12: val = 'M'; break;
    case 13: val = 'N'; break;
    case 14: val = 'P'; break;
    case 15: val = 'Q'; break;
    case 16: val = 'R'; break;
    case 17: val = 'S'; break;
    case 18: val = 'T'; break;
    case 19: val = 'V'; break;
    case 20: val = 'W'; break;
    case 21: val = 'X'; break;
    case 22: val = 'Y'; break;
    case 23: val = 'Z'; break;
    case 24: val = 'U'; break;
    case 25: val = '*'; break;
    default: val = '?'; break;
    }

    return val;
}

void 
CSequence::Reset(const blast::SSeqLoc& seq_in)
{
    _ASSERT(seq_in.seqloc->IsSetWhole());

    CBioseq_Handle bhandle = seq_in.scope->GetBioseqHandle(
                                         seq_in.seqloc->GetWhole(),
                                         CScope::eGetBioseq_All);
    CConstRef<CBioseq> bioseq = bhandle.GetCompleteBioseq();

    _ASSERT(bioseq->IsSetInst());
    const CBioseq::TInst& inst = bioseq->GetInst();
    _ASSERT(inst.IsSetSeq_data());
    const CSeq_data& seqdata = inst.GetSeq_data(); 
    int seq_length;

    if (!seqdata.IsNcbistdaa()) {
        CSeq_data converted;
        CSeqportUtil::Convert(seqdata, &converted, CSeq_data::e_Ncbistdaa);

        seq_length = converted.GetNcbistdaa().Get().size();
        m_Sequence.resize(seq_length);
        for (int i = 0; i < seq_length; i++)
            m_Sequence[i] = converted.GetNcbistdaa().Get()[i];
    }
    else {
        seq_length = seqdata.GetNcbistdaa().Get().size();
        m_Sequence.resize(seq_length);
        for (int i = 0; i < seq_length; i++)
            m_Sequence[i] = seqdata.GetNcbistdaa().Get()[i];
    }

    m_Freqs.Resize(seq_length, kAlphabetSize);
    m_Freqs.Set(0.0);
}

CSequence::CSequence(const blast::SSeqLoc& sl) 
{
    Reset(sl);
}

void 
CSequence::PropagateGaps(const CNWAligner::TTranscript& transcript,
                         CNWAligner::ETranscriptSymbol gap_choice)
{
    int new_size = transcript.size();

    vector<unsigned char> new_seq(new_size);
    TFreqMatrix new_freq(new_size, kAlphabetSize, 0.0);

    for (int i = 0, j = 0; i < new_size; i++) {
        if (transcript[i] == gap_choice) {
            new_seq[i] = kGapChar;
        }
        else {
            new_seq[i] = m_Sequence[j];
            for (int k = 0; k < kAlphabetSize; k++)
                new_freq(i, k) = m_Freqs(j, k);
            _ASSERT(j < GetLength());
            j++;
        }
    }

    m_Sequence.swap(new_seq);
    m_Freqs.Swap(new_freq);
}

void CSequence::CompressSequences(vector<CSequence>& seq,
                                  vector<int> index_list)
{
    int align_length = seq[index_list[0]].GetLength();
    int num_seqs = index_list.size();
    int new_length = 0;

    for (int i = 0; i < align_length; i++) {
        int j;
        for (j = 0; j < num_seqs; j++) {
            if (seq[index_list[j]].m_Sequence[i] != kGapChar)
                break;
        }
        if (j < num_seqs) {
            for (int j = 0; j < num_seqs; j++) {
                seq[index_list[j]].m_Sequence[new_length] =
                                 seq[index_list[j]].m_Sequence[i];
                for (int k = 0; k < kAlphabetSize; k++) {
                    seq[index_list[j]].m_Freqs(new_length, k) =
                                seq[index_list[j]].m_Freqs(i, k);
                }
            }
            new_length++;
        }
    }
    if (new_length != align_length) {
        for (int i = 0; i < num_seqs; i++) {
            seq[index_list[i]].m_Sequence.resize(new_length);
            seq[index_list[i]].m_Freqs.Resize(new_length, kAlphabetSize);
        }
    }
}

int
CSequence::GetPairwiseScore(CSequence& seq1, 
                            CSequence& seq2,
                            SNCBIFullScoreMatrix& matrix,
                            int gap_open,
                            int gap_extend)
{
    _ASSERT(seq1.GetLength() == seq2.GetLength());

    int i = -1, j = -1;
    int len = seq1.GetLength();
    bool seq1_gap = false;
    bool seq2_gap = false;
    int score = 0;

    for (int k = 0; k < len; k++) {
        unsigned char letter1 = seq1.m_Sequence[k];
        unsigned char letter2 = seq2.m_Sequence[k];

        if (letter1 != kGapChar)
            i++;
        if (letter2 != kGapChar)
            j++;

        if (letter1 != kGapChar && letter2 != kGapChar) {
            seq1_gap = false;
            seq2_gap = false;
            score += matrix.s[letter1][letter2];
        }
        else if (letter1 == kGapChar && letter2 == kGapChar) {
            continue;
        }
        else if (letter1 == kGapChar) {
            if (seq1_gap == false) {
                score += gap_open;
                seq1_gap = true;
                seq2_gap = false;
            }
            score += gap_extend;
        }
        else {
            if (seq2_gap == false) {
                score += gap_open;
                seq1_gap = false;
                seq2_gap = true;
            }
            score += gap_extend;
        }
    }
    return score;
}

int
CSequence::GetSumOfPairs(vector<CSequence>& list,
                         SNCBIFullScoreMatrix& matrix,
                         int gap_open,
                         int gap_extend)
{
    int num_seq = list.size();
    int score = 0;

    for (int i = 0; i < num_seq - 1; i++) {
        for (int j = i + 1; j < num_seq; j++) {
            score += GetPairwiseScore(list[i], list[j], matrix,
                                      gap_open, gap_extend);
        }
    }
    return score;
}

END_SCOPE(cobalt)
END_NCBI_SCOPE

/*------------------------------------------------------------------------
  $Log$
  Revision 1.3  2005/11/08 18:42:16  papadopo
  assert -> _ASSERT

  Revision 1.2  2005/11/08 17:56:23  papadopo
  1. make header files self-sufficient
  2. ASSERT -> assert
  3. minor cleanup

  Revision 1.1  2005/11/07 18:14:01  papadopo
  Initial revision

------------------------------------------------------------------------*/
