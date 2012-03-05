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
CAlnMixMatches::x_CompareScores(const CRef<CAlnMixMatch>& match1, 
                                const CRef<CAlnMixMatch>& match2) 
{
    return match1->m_Score > match2->m_Score;
}


inline
bool
CAlnMixMatches::x_CompareChainScores(const CRef<CAlnMixMatch>& match1, 
                                     const CRef<CAlnMixMatch>& match2) 
{
    return 
        match1->m_ChainScore == match2->m_ChainScore  &&
        match1->m_Score > match2->m_Score  ||
        match1->m_ChainScore > match2->m_ChainScore;
}


void
CAlnMixMatches::SortByScore()
{
    stable_sort(m_Matches.begin(), m_Matches.end(), x_CompareScores);
}


void
CAlnMixMatches::SortByChainScore()
{
    stable_sort(m_Matches.begin(), m_Matches.end(), x_CompareChainScores);
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
    int              total_aln_score = 0;

    vector<CRef<CAlnMixSeq> >& ds_seq = m_AlnMixSequences->m_DsSeq[&ds];

    size_t prev_matches_size = m_Matches.size();

    for (CAlnMap::TNumseg seg =0;  seg < ds.GetNumseg();  seg++) {
        len = ds.GetLens()[seg];
        single_chunk = true;

        for (CAlnMap::TNumrow row1 = 0;  row1 < ds.GetDim();  row1++) {
            if ((start1 = ds.GetStarts()[seg_off + row1]) >= 0) {
                //search for a match for the piece of seq on row1

                CAlnMixSeq* aln_seq1 = ds_seq[row1].GetNonNullPointer();

                for (CAlnMap::TNumrow row2 = row1+1;
                     row2 < ds.GetDim();  row2++) {
                    if ((start2 = ds.GetStarts()[seg_off + row2]) >= 0) {
                        //match found
                        if (single_chunk) {
                            single_chunk = false;
                            first_non_gapped_row_found = row1;
                        }
                        

                        //add only pairs with the first_non_gapped_row_found
                        //still, calc the score to be added to the seqs' scores

                        int score = 0;

                        CAlnMixSeq* aln_seq2 = ds_seq[row2].GetNonNullPointer();



                        // determine the strand
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
                        }


                        //Determine the score
                        if (flags & fCalcScore  &&  x_CalculateScore) {
                            // calc the score by seq comp
                            string s1, s2;
                            aln_seq1->GetSeqString(s1,
                                                   start1,
                                                   len * aln_seq1->m_Width,
                                                   strand1 != eNa_strand_minus);
                            aln_seq2->GetSeqString(s2,
                                                   start2,
                                                   len * aln_seq2->m_Width,
                                                   strand2 != eNa_strand_minus);

                            score = x_CalculateScore(s1,
                                                              s2,
                                                              aln_seq1->m_IsAA,
                                                              aln_seq2->m_IsAA,
                                                              1,
                                                              1);
                        } else {
                            score = len;
                        }
                        total_aln_score += score;

                        // add to the sequences' scores
                        aln_seq1->m_Score += score;
                        aln_seq2->m_Score += score;

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
                                                    - score : score);
                        aln_seq2->m_StrandScore += (strand2 == eNa_strand_minus ?
                                                    - score : score);

                        if (row1 == first_non_gapped_row_found) {
                            CRef<CAlnMixMatch> match(new CAlnMixMatch);
                            match->m_AlnSeq1 = ds_seq[row1];
                            match->m_MatchIter1 = match->m_AlnSeq1->m_MatchList.end();
                            match->m_Start1 = start1;
                            match->m_AlnSeq2 = ds_seq[row2];
                            match->m_MatchIter2 = match->m_AlnSeq2->m_MatchList.end();
                            match->m_Start2 = start2;
                            match->m_Len = len;
                            match->m_DsIdx = m_DsCnt;
                            match->m_StrandsDiffer = false;
                            if (strands_exist) {
                                if ((strand1 == eNa_strand_minus  &&
                                     strand2 != eNa_strand_minus)  ||
                                    (strand1 != eNa_strand_minus  &&
                                     strand2 == eNa_strand_minus)) {
                                    
                                    match->m_StrandsDiffer = true;
                                }
                            }
                            match->m_Score = score;
                            _ASSERT(match->IsGood());
                            m_Matches.push_back(match);
                            _ASSERT(match->IsGood());
                        }
                    }
                }
                if (single_chunk) {
                    //record it
                    CRef<CAlnMixMatch> match(new CAlnMixMatch);
                    match->m_Score = 0;
                    match->m_AlnSeq1 = ds_seq[row1];
                    match->m_MatchIter1 = match->m_AlnSeq1->m_MatchList.end();
                    match->m_Start1 = start1;
                    match->m_AlnSeq2 = 0;
                    match->m_Start2 = 0;
                    match->m_Len = len;
                    match->m_StrandsDiffer = false;
                    match->m_DsIdx = m_DsCnt;
                    _ASSERT(match->IsGood());
                    m_Matches.push_back(match);
                }
            }
        }
        seg_off += ds.GetDim();
    }


    // Update chain scores
    {{
        // iterate through the newly added matches to set the m_ChainScore
        size_t new_maches_size = m_Matches.size() - prev_matches_size;
        NON_CONST_REVERSE_ITERATE(TMatches, match_i, m_Matches) {
            _ASSERT((*match_i)->IsGood());
            (*match_i)-> m_ChainScore = total_aln_score;
            if ( !(--new_maches_size) ) {
                break;
            }
        }

        // update m_ChainScore in the participating sequences
        for (size_t row = 0;  row < ds_seq.size();  row++) {
            ds_seq[row]->m_ChainScore += total_aln_score;
        }
    }}
}


END_objects_SCOPE // namespace ncbi::objects::
END_NCBI_SCOPE
