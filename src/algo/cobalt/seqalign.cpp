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
#include <algo/cobalt/cobalt.hpp>

/// @file seqalign.cpp
/// Contents: Seqalign output for a multiple sequence alignment

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)

USING_SCOPE(objects);

CRef<CSeq_align>
CMultiAligner::x_GetSeqalign(vector<CSequence>& align,
                             vector<int>& indices)
{
    int num_queries = align.size();
    int length = align[0].GetLength();

    CRef<CSeq_align> retval(new CSeq_align);
    retval->SetType(CSeq_align::eType_global);
    retval->SetDim(num_queries);

    CRef<CDense_seg> denseg(new CDense_seg);
    denseg->SetDim(num_queries);

    for (int i = 0; i < num_queries; i++) {
        CRef<CSeq_id> id(const_cast<CSeq_id *>(
                       &m_tQueries[indices[i]].seqloc->GetWhole()));
        denseg->SetIds().push_back(id);
    }

    vector<int> seq_off(num_queries, 0);
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
        if (j < num_queries) {
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
CMultiAligner::GetSeqalignResults()
{
    int num_queries = m_Results.size();
    vector<int> indices(num_queries);

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

    for (int i = 0; i < num_selected; i++) {
        _ASSERT(indices[i] >= 0 && indices[i] < m_Results.size());
        new_align[i] = m_Results[indices[i]];
        new_indices[i] = i;
    }
    CSequence::CompressSequences(new_align, new_indices);
    return x_GetSeqalign(new_align, indices);
}

END_SCOPE(cobalt)
END_NCBI_SCOPE

/*-----------------------------------------------------------------------
  $Log$
  Revision 1.4  2005/11/18 20:25:22  papadopo
  Make main seqalign generator private, and add extra member to extract a subset of the complete aligned results

  Revision 1.3  2005/11/08 19:49:19  papadopo
  fix solaris compile warnings

  Revision 1.2  2005/11/08 17:35:00  papadopo
  fix file description

  Revision 1.1  2005/11/08 17:32:25  papadopo
  Initial revision

-----------------------------------------------------------------------*/
