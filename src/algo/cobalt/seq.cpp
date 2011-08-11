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
#include <objmgr/seq_vector.hpp>
#include <objects/seqalign/Dense_seg.hpp>
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
CSequence::Reset(const objects::CSeq_loc& seq_in, objects::CScope& scope)
{
    if (!seq_in.IsWhole() && !seq_in.IsInt()) {
        NCBI_THROW(CMultiAlignerException, eInvalidInput,
                   "Unsupported SeqLoc encountered");
    }

    objects::CSeqVector sv(seq_in, scope);

    if (!sv.IsProtein()) {
        NCBI_THROW(CMultiAlignerException, eInvalidInput,
                   "Nucleotide sequences cannot be aligned");
    }

    // make a local copy of the sequence data

    int seq_length = sv.size();
    m_Sequence.resize(seq_length);
    for (int i = 0; i < seq_length; i++) {
        m_Sequence[i] = sv[i];
    }

    // residue frequencies start off empty

    m_Freqs.Resize(seq_length, kAlphabetSize);
    m_Freqs.Set(0.0);
}


void
CSequence::Reset(int length) 
{
    m_Sequence.resize(length); 
    for (int i=0;i < length;i++) {
        m_Sequence[i] = kGapChar;
    }
}


CSequence::CSequence(const objects::CSeq_loc& sl, objects::CScope& scope) 
{
    Reset(sl, scope);
}


void 
CSequence::PropagateGaps(const CNWAligner::TTranscript& transcript,
                         CNWAligner::ETranscriptSymbol gap_choice)
{
    int new_size = transcript.size();

    // no gaps means nothing needs updating

    if (new_size == GetLength()) {
        return;
    }

    vector<unsigned char> new_seq(new_size);
    TFreqMatrix new_freq(new_size, kAlphabetSize, 0.0);

    // expand the sequence data and the profile columns
    // to incorporate new gaps

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

    // replace class data

    m_Sequence.swap(new_seq);
    m_Freqs.Swap(new_freq);
}

void 
CSequence::InsertGaps(const vector<Uint4>& gap_locations, bool consider_gaps)
{
    Uint4 new_size = (Uint4)(GetLength() + gap_locations.size());
    bool is_freqs_set = m_Freqs.GetRows() > 0;

    // no gaps means nothing needs updating

    if (new_size == (Uint4)GetLength()) {
        return;
    }

    vector<unsigned char> new_seq(new_size);
    TFreqMatrix new_freq;
    if (is_freqs_set) {
        new_freq.Resize(new_size, kAlphabetSize, 0.0);
    }

    // expand the sequence data and the profile columns
    // to incorporate new gaps

    Uint4 location = 0, gap_ind = 0;
    for (size_t i = 0, j = 0; i < new_size; i++) {
        if (gap_ind < gap_locations.size() 
            && location == gap_locations[gap_ind]) {
            new_seq[i] = kGapChar;
            gap_ind++;

            if (consider_gaps) {
                location++;
            }
        }
        else if (j < m_Sequence.size()) {
            new_seq[i] = m_Sequence[j];
            if (is_freqs_set) {
                for (int k = 0; k < kAlphabetSize; k++)
                    new_freq(i, k) = m_Freqs(j, k);
            }

            if (m_Sequence[j] != kGapChar || consider_gaps) {
                location++;
            }

            j++;
        }
        else {
            // Gaps at the end of the sequence
            new_seq[i] = kGapChar;
            gap_ind++;
        }

    }
    _ASSERT(gap_ind == gap_locations.size());

    // replace class data

    m_Sequence.swap(new_seq);
    if (is_freqs_set) {
        m_Freqs.Swap(new_freq);
    }
}


void CSequence::CompressSequences(vector<CSequence>& seq,
                                  vector<int> index_list)
{
    int align_length = seq[index_list[0]].GetLength();
    int num_seqs = index_list.size();
    int new_length = 0;

    // for each alignment column

    for (int i = 0; i < align_length; i++) {
        int j;
        for (j = 0; j < num_seqs; j++) {
            if (seq[index_list[j]].m_Sequence[i] != kGapChar)
                break;
        }

        // if the specified list of sequences do not all 
        // have a gap character in column i, keep the column
        
        if (j < num_seqs) {
            for (j = 0; j < num_seqs; j++) {
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

    // if the length changed, shorten m_Sequence and m_Freqs

    if (new_length != align_length) {
        for (int i = 0; i < num_seqs; i++) {
            seq[index_list[i]].m_Sequence.resize(new_length);
            seq[index_list[i]].m_Freqs.Resize(new_length, kAlphabetSize);
        }
    }
}

void CSequence::CreateMsa(const objects::CSeq_align& seq_align,
                          objects::CScope& scope,
                          vector<CSequence>& msa)
{
    const objects::CDense_seg& denseg = seq_align.GetSegs().GetDenseg();
    const objects::CDense_seg::TStarts& starts = denseg.GetStarts();
    const objects::CDense_seg::TLens& lens = denseg.GetLens();

    int num_seqs = denseg.GetDim();
    int seq_length = 0;
    ITERATE (objects::CDense_seg::TLens, it, lens) {
        seq_length += *it;
    }

    // reserve memory for MSA
    msa.resize(num_seqs);
    NON_CONST_ITERATE (vector<CSequence>, it, msa) {
        it->m_Sequence.resize(seq_length);
        it->m_Freqs.Resize(seq_length, kAlphabetSize);
        it->m_Freqs.Set(0.0);
    }

    // get sequences
    vector< CRef<objects::CSeqVector> > seq_vectors;
    seq_vectors.reserve(num_seqs);
    ITERATE (objects::CDense_seg::TIds, it, denseg.GetIds()) {
        seq_vectors.push_back(CRef<objects::CSeqVector>(
                                     new objects::CSeqVector(
                                             scope.GetBioseqHandle(**it))));
    }

    // convert Seq_align to strings of residues and gaps
    // start column in MSA
    int from = 0;
    size_t seg_index = 0;

    // for each alignment segment
    while (seg_index < lens.size()) {

        TSeqPos seg_len = lens[seg_index];

        _ASSERT(from + seg_len - 1 < (int)seq_length);

        // for each sequence start position
        for (int i=0;i < num_seqs;i++) {

            // if gap in sequence then put gaps
            if (starts[seg_index * num_seqs + i] < 0) {
                for (TSeqPos k=0;k < seg_len;k++) {
                    msa[i].m_Sequence[from + k] = kGapChar;
                }
            }
            else {

                // else copy residues from seq_vector
                for (TSeqPos k=0;k < seg_len;k++) {
                    msa[i].m_Sequence[from + k] =
                        (*seq_vectors[i])[starts[seg_index * num_seqs + i] + k];
                }
            }
        }

        // move to the next segment
        from += seg_len;
        seg_index++;
    }
    
}

END_SCOPE(cobalt)
END_NCBI_SCOPE
