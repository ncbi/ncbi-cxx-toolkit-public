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

File name: seqalign.cpp

Author: Jason Papadopoulos

Contents: Seqalign output for a multiple sequence alignment

******************************************************************************/

#include <ncbi_pch.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <algo/cobalt/cobalt.hpp>

/// @file seqalign.cpp
/// Contents: Seqalign output for a multiple sequence alignment

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)

USING_SCOPE(objects);

/// Produce a seqalign representing the specified alignment,
/// using a subset of the sequences.
/// @param align The alignment to convert
/// @param indices 0-based list of sequence numbers that will
///                participate in the resulting Seq-align. indices
///                may appear out of order and may be repeated
/// @return The generated seqalign
///
CRef<CSeq_align>
CMultiAligner::x_GetSeqalign(vector<CSequence>& align,
                             vector<int>& indices)
{
    int num_queries = align.size();
    int length = align[0].GetLength();

    // Seqalign is of global type

    CRef<CSeq_align> retval(new CSeq_align);
    retval->SetType(CSeq_align::eType_global);
    retval->SetDim(num_queries);

    // and contains a single denseg

    CRef<CDense_seg> denseg(new CDense_seg);
    denseg->SetDim(num_queries);

    // find the initial sequence offsets for use
    // in the denseg, and also fill in the ID's of
    // the sequences

    vector<int> seq_off(num_queries, 0);
    for (int i = 0; i < num_queries; i++) {
        const CSeq_loc& seqloc = *m_tQueries[indices[i]];
        if (seqloc.IsWhole()) {

            // the seqloc is const, but the constructor wants a
            // pointer to a non-const object

            CRef<CSeq_id> id(const_cast<CSeq_id *>(&seqloc.GetWhole()));
            denseg->SetIds().push_back(id);
        }
        else if (seqloc.IsInt()) {

            CRef<CSeq_id> id(const_cast<CSeq_id *>(&seqloc.GetInt().GetId()));
            denseg->SetIds().push_back(id);

            // in the case of a Seq-interval, the start offset
            // may not be at the beginning of the sequence

            seq_off[i] = seqloc.GetInt().GetFrom();
        }
    }

    // now build the rest of the denseg

    int num_seg = 0;
    int i, j, seg_len;

    for (i = 1, seg_len = 1; i < length; i++, seg_len++) {
        for (j = 0; j < num_queries; j++) {
            if ((align[j].GetLetter(i) == CSequence::kGapChar &&
                 align[j].GetLetter(i-1) != CSequence::kGapChar) ||
                (align[j].GetLetter(i) != CSequence::kGapChar &&
                 align[j].GetLetter(i-1) == CSequence::kGapChar)) 
                break;
        }

        // if the pattern of gaps in column i is different from
        // the pattern of gaps in column i-1, this column is the
        // beginning of the next segment, so finish the previous one
        
        if (j < num_queries) {

            // write out the start offsets of the previous segment,
            // then reset the segment length

            for (j = 0; j < num_queries; j++) {
                if (align[j].GetLetter(i-1) == CSequence::kGapChar) {
                    denseg->SetStarts().push_back(-1);
                }
                else {
                    denseg->SetStarts().push_back(seq_off[j]);
                    seq_off[j] += seg_len;
                }
            }
            denseg->SetLens().push_back(seg_len);
            num_seg++;
            seg_len = 0;
        }
    }

    // write out the last segment

    for (j = 0; j < num_queries; j++) {
        if (align[j].GetLetter(i-1) == CSequence::kGapChar)
            denseg->SetStarts().push_back(-1);
        else
            denseg->SetStarts().push_back(seq_off[j]);
    }
    denseg->SetLens().push_back(seg_len);
    denseg->SetNumseg(num_seg + 1);

    retval->SetSegs().SetDenseg(*denseg);
    return retval;
}


CRef<CSeq_align>
CMultiAligner::GetSeqalignResults(CSeq_align& align,
                                  vector<int>& indices)
{
    if (align.GetType() != CSeq_align::eType_global ||
        !align.IsSetSegs() || !align.GetSegs().IsDenseg()) {
        NCBI_THROW(CMultiAlignerException, eInvalidInput,
                   "Unsupported SeqAlign format");
    }
    if (indices.empty()) {
        NCBI_THROW(CMultiAlignerException, eInvalidInput,
                   "Sequences must be specified for output SeqAlign");
    }

    const CDense_seg& denseg = align.GetSegs().GetDenseg();
    int num_queries = denseg.GetDim();
    int num_segs = denseg.GetNumseg();

    int num_subset = indices.size();
    for (int i = 0; i < num_subset; i++) {
        if (indices[i] < 0 || indices[i] >= num_queries) {
            NCBI_THROW(CMultiAlignerException, eInvalidInput,
                            "Sequence index out of range");
        }
    }

    // output Seqalign is of global type

    CRef<CSeq_align> retval(new CSeq_align);
    retval->SetType(CSeq_align::eType_global);
    retval->SetDim(num_queries);

    // and contains a single denseg

    CRef<CDense_seg> new_denseg(new CDense_seg);
    new_denseg->SetDim(num_subset);

    // fill in the ID's of the sequences

    for (int i = 0; i < num_subset; i++) {
        CRef<CSeq_id> id(const_cast<CSeq_id *>
                         (&(*denseg.GetIds()[indices[i]])));
        new_denseg->SetIds().push_back(id);
    }

    // now build the rest of the denseg

    int num_new_segs = 0;
    const CDense_seg::TStarts& starts = denseg.GetStarts();
    const CDense_seg::TLens& lens = denseg.GetLens();
    CDense_seg::TStarts& new_starts = new_denseg->SetStarts();
    CDense_seg::TLens& new_lens = new_denseg->SetLens();

    for (int i = 0, j = 0; i < num_segs; i++) {

        // skip segments that contain only gaps in all sequences

        for (j = 0; j < num_subset; j++) {
            if (starts[i * num_queries + indices[j]] != -1)
                break;
        }
        if (j == num_subset)
            continue;

        // determine if the current segment has gaps in the same
        // positions as the previous one. If so, just update the
        // length of the previous segment

        if (num_new_segs > 0) {
            int off = i * num_queries;
            int new_off = (num_new_segs - 1) * num_subset;

            for (j = 0; j < num_subset; j++) {
                if ((new_starts[new_off + j] == -1 &&
                     starts[off + indices[j]] != -1) ||
                    (new_starts[new_off + j] != -1 &&
                     starts[off + indices[j]] == -1)) {
                    break;
                }
            }
            if (j == num_subset) {
                new_lens.back() += lens[i];
                continue;
            }
        }

        // append segment i to the new list of segments

        for (j = 0; j < num_subset; j++) {
            new_starts.push_back(starts[i * num_queries + indices[j]]);
        }
        new_lens.push_back(lens[i]);
        num_new_segs++;
    }

    new_denseg->SetNumseg(num_new_segs);
    retval->SetSegs().SetDenseg(*new_denseg);
    return retval;
}

CRef<CSeq_align>
CMultiAligner::GetSeqalignResults()
{
    int num_queries = m_Results.size();
    vector<int> indices(num_queries);

    // all sequences participate in the output

    for (int i = 0; i < num_queries; i++) {
        indices[i] = i;
    }
    return x_GetSeqalign(m_Results, indices);
}


CRef<CSeq_align>
CMultiAligner::GetSeqalignResults(vector<int>& indices)
{
    int num_selected = indices.size();
    vector<CSequence> new_align(num_selected);
    vector<int> new_indices(num_selected);

    // a specified subset of the sequences participate
    // in the output

    for (int i = 0; i < num_selected; i++) {
        if (indices[i] < 0 || indices[i] >= m_Results.size()) {
            NCBI_THROW(CMultiAlignerException, eInvalidInput,
                            "Sequence index out of range");
        }
        new_align[i] = m_Results[indices[i]];
        new_indices[i] = i;
    }
    
    // remove columns that have all gaps

    CSequence::CompressSequences(new_align, new_indices);

    return x_GetSeqalign(new_align, indices);
}

END_SCOPE(cobalt)
END_NCBI_SCOPE
