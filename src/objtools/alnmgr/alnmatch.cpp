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
* Author:  Kamen Todorov, NCBI
*
* File Description:
*   Alignment matches
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objtools/alnmgr/alnmatch.hpp>
#include <objtools/alnmgr/alnexception.hpp>
#include <objtools/alnmgr/alnmap.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <algorithm>


BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::


CAlnMixMatches::CAlnMixMatches(CRef<CAlnMixSequences>& sequences,
                               TCalcScoreMethod calc_score)
    : m_DsCnt(0),
      m_AlnMixSequences(sequences),
      m_Seqs(m_AlnMixSequences->m_Seqs),
      x_CalculateScore(calc_score),
      m_ContainsAA(m_AlnMixSequences->m_ContainsAA),
      m_ContainsNA(m_AlnMixSequences->m_ContainsNA)
{
}


inline
bool
CAlnMixMatches::x_CompareAlnMatchScores(const CRef<CAlnMixMatch>& aln_match1, 
                                        const CRef<CAlnMixMatch>& aln_match2) 
{
    return aln_match1->m_Score > aln_match2->m_Score;
}


void
CAlnMixMatches::SortByScore()
{
    stable_sort(m_Matches.begin(), m_Matches.end(), x_CompareAlnMatchScores);
}


void
CAlnMixMatches::Add(const CDense_seg& ds, TAddFlags flags)
{
    m_DsCnt++;

    m_AddFlags = flags;

    int              seg_off = 0;
    TSignedSeqPos    start1, start2;
    TSeqPos          len;
    bool             single_chunk;
    CAlnMap::TNumrow first_non_gapped_row_found;
    bool             strands_exist =
        ds.GetStrands().size() == (size_t)ds.GetNumseg() * ds.GetDim();

    vector<CRef<CAlnMixSeq> >& ds_seq = m_AlnMixSequences->m_DsSeq[&ds];

    for (CAlnMap::TNumseg seg =0;  seg < ds.GetNumseg();  seg++) {
        len = ds.GetLens()[seg];
        single_chunk = true;

        for (CAlnMap::TNumrow row1 = 0;  row1 < ds.GetDim();  row1++) {
            if ((start1 = ds.GetStarts()[seg_off + row1]) >= 0) {
                //search for a match for the piece of seq on row1

                CRef<CAlnMixSeq> aln_seq1 = ds_seq[row1];

                for (CAlnMap::TNumrow row2 = row1+1;
                     row2 < ds.GetDim();  row2++) {
                    if ((start2 = ds.GetStarts()[seg_off + row2]) >= 0) {
                        //match found
                        if (single_chunk) {
                            single_chunk = false;
                            first_non_gapped_row_found = row1;
                        }
                        
                        //create the match
                        CRef<CAlnMixMatch> match(new CAlnMixMatch);

                        //add only pairs with the first_non_gapped_row_found
                        //still, calc the score to be added to the seqs' scores
                        if (row1 == first_non_gapped_row_found) {
                            m_Matches.push_back(match);
                        }

                        CRef<CAlnMixSeq> aln_seq2 = ds_seq[row2];


                        match->m_AlnSeq1 = aln_seq1;
                        match->m_Start1 = start1;
                        match->m_AlnSeq2 = aln_seq2;
                        match->m_Start2 = start2;
                        match->m_Len = len;
                        match->m_DsIdx = m_DsCnt;

                        // determine the strand
                        match->m_StrandsDiffer = false;
                        ENa_strand strand1 = eNa_strand_plus;
                        ENa_strand strand2 = eNa_strand_plus;
                        if (strands_exist) {
                            if (ds.GetStrands()[seg_off + row1] 
                                == eNa_strand_minus) {
                                strand1 = eNa_strand_minus;
                            }
                            if (ds.GetStrands()[seg_off + row2] 
                                == eNa_strand_minus) {
                                strand2 = eNa_strand_minus;
                            }

                            if (strand1 == eNa_strand_minus  &&
                                strand2 != eNa_strand_minus  ||
                                strand1 != eNa_strand_minus  &&
                                strand2 == eNa_strand_minus) {

                                match->m_StrandsDiffer = true;

                            }
                        }


                        //Determine the score
                        if (x_CalculateScore) {
                            // calc the score by seq comp
                            string s1, s2;
                            TSeqPos len1 = len * aln_seq1->m_Width;
                            TSeqPos len2 = len * aln_seq2->m_Width;

                            if (strand1 == eNa_strand_minus) {
                                CSeqVector seq_vec = 
                                    aln_seq1->m_BioseqHandle->GetSeqVector
                                    (CBioseq_Handle::eCoding_Iupac,
                                     CBioseq_Handle::eStrand_Minus);
                                TSeqPos size = seq_vec.size();
                                seq_vec.GetSeqData
                                    (size - (start1 + len1),
                                     size - start1, 
                                     s1);
                            } else {
                                aln_seq1->m_BioseqHandle->GetSeqVector
                                    (CBioseq_Handle::eCoding_Iupac,
                                     CBioseq_Handle::eStrand_Plus).
                                    GetSeqData(start1, 
                                               start1 + len1,
                                               s1);
                            }                                
                            if (strand2 ==  eNa_strand_minus) {
                                CSeqVector seq_vec = 
                                    aln_seq2->m_BioseqHandle->GetSeqVector
                                    (CBioseq_Handle::eCoding_Iupac,
                                     CBioseq_Handle::eStrand_Minus);
                                TSeqPos size = seq_vec.size();
                                seq_vec.GetSeqData
                                    (size - (start2 + len2),
                                     size - start2, 
                                     s2);
                            } else {
                                aln_seq2->m_BioseqHandle->GetSeqVector
                                    (CBioseq_Handle::eCoding_Iupac,
                                     CBioseq_Handle::eStrand_Plus).
                                    GetSeqData(start2,
                                               start2 + len2,
                                               s2);
                            }

                            //verify that we were able to load all data
                            if (s1.length() != len1 || s2.length() != len2) {

                                string symptoms  = "Input Dense-seg " +
                                    NStr::IntToString(m_DsCnt) + ":" +
                                    " Unable to load data for segment=" +
                                    NStr::IntToString(seg) +
                                    " (length=" + NStr::IntToString(len) + ") " +
                                    ", rows " + NStr::IntToString(row1) +
                                    " (seq-id=\"" + aln_seq1->m_SeqId->AsFastaString() +
                                    "\" start=" + NStr::IntToString(start1) + ")" +
                                    " and " + NStr::IntToString(row2) +
                                    " (seq-id\"" + aln_seq2->m_SeqId->AsFastaString() +
                                    "\" start=" + NStr::IntToString(start2) + ").";
                                
                                string diagnosis = "Looks like the sequence coords for row " +
                                    NStr::IntToString(s1.length() < len1 ? row1 : row2) +
                                    " are out of range.";

                                string errstr = string("CAlnMixMatches::Add(): ") +
                                    symptoms + " " + diagnosis;
                                NCBI_THROW(CAlnException, eInvalidSegment,
                                           errstr);
                            }

                            match->m_Score = x_CalculateScore(s1,
                                                              s2,
                                                              aln_seq1->m_IsAA,
                                                              aln_seq2->m_IsAA);
                        } else {
                            match->m_Score = len;
                        }
                        

                        // add to the sequences' scores
                        aln_seq1->m_Score += match->m_Score;
                        aln_seq2->m_Score += match->m_Score;

                        // in case of fForceTranslation, 
                        // check if strands are not mixed by
                        // comparing current strand to the prevailing one
                        if (flags & fForceTranslation  &&
                            (aln_seq1->m_StrandScore > 0  && 
                             strand1 == eNa_strand_minus ||
                             aln_seq1->m_StrandScore < 0  && 
                             strand1 != eNa_strand_minus ||
                             aln_seq2->m_StrandScore > 0  && 
                             strand2 == eNa_strand_minus ||
                             aln_seq2->m_StrandScore < 0  && 
                             strand2 != eNa_strand_minus)) {
                            NCBI_THROW(CAlnException, eMergeFailure,
                                       "CAlnMixMatches::Add(): "
                                       "Unable to mix strands when "
                                       "forcing translation!");
                        }
                        
                        // add to the prevailing strand
                        aln_seq1->m_StrandScore += (strand1 == eNa_strand_minus ?
                                                    - match->m_Score : match->m_Score);
                        aln_seq2->m_StrandScore += (strand2 == eNa_strand_minus ?
                                                    - match->m_Score : match->m_Score);

                    }
                }
                if (single_chunk) {
                    //record it
                    CRef<CAlnMixMatch> match(new CAlnMixMatch);
                    match->m_Score = 0;
                    match->m_AlnSeq1 = aln_seq1;
                    match->m_Start1 = start1;
                    match->m_AlnSeq2 = 0;
                    match->m_Start2 = 0;
                    match->m_Len = len;
                    match->m_StrandsDiffer = false;
                    match->m_DsIdx = m_DsCnt;
                    m_Matches.push_back(match);
                }
            }
        }
        seg_off += ds.GetDim();
    }
}


END_objects_SCOPE // namespace ncbi::objects::
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2005/03/01 17:28:49  todorov
* Rearranged CAlnMix classes
*
* ===========================================================================
*/
