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
*   Alignment merger
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objtools/alnmgr/alnmix.hpp>

#include <objects/seqalign/Seq_align_set.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>

// Object Manager includes
#include <objmgr/scope.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::


CAlnMix::CAlnMix(void)
    : m_MergeFlags(0),
      m_SingleRefseq(false),
      m_ContainsAA(false),
      m_ContainsNA(false)
{
}


CAlnMix::CAlnMix(CScope& scope)
    : m_Scope(&scope),
      m_MergeFlags(0),
      m_SingleRefseq(false),
      m_ContainsAA(false),
      m_ContainsNA(false)
{
}


CAlnMix::~CAlnMix(void)
{
}


inline
bool CAlnMix::x_CompareAlnSeqScores(const CRef<CAlnMixSeq>& aln_seq1,
                                    const CRef<CAlnMixSeq>& aln_seq2) 
{
    return aln_seq1->m_Score > aln_seq2->m_Score;
}


inline
bool CAlnMix::x_CompareAlnMatchScores(const CRef<CAlnMixMatch>& aln_match1, 
                                      const CRef<CAlnMixMatch>& aln_match2) 
{
    return aln_match1->m_Score > aln_match2->m_Score;
}


void CAlnMix::x_Reset()
{
    if (m_DS) {
        m_DS.Reset();
    }
    if (m_Aln) {
        m_Aln.Reset();
    }
    m_Segments.clear();
    m_Rows.clear();
    m_ExtraRows.clear();
    NON_CONST_ITERATE (TSeqs, seq_i, m_Seqs) {
        (*seq_i)->m_Starts.clear();
        (*seq_i)->m_ExtraRow = 0;
    }
}


void CAlnMix::Merge(TMergeFlags flags)
{
    if (m_InputDSs.empty()) {
        NCBI_THROW(CAlnException, eMergeFailure,
                   "CAlnMix::Merge(): "
                   "No alignments were added for merging.");
    }
    if ( !m_DS  ||  m_MergeFlags != flags) {
        x_Reset();
        m_MergeFlags = flags;
        if (m_MergeFlags & fTryOtherMethodOnFail) {
            try {
                x_Merge();
            } catch(...) {
                if (m_MergeFlags & fGen2EST) {
                    m_MergeFlags &= !fGen2EST;
                } else {
                    m_MergeFlags |= fGen2EST;
                }
                try {
                    x_Merge();
                } catch(...) {
                    NCBI_THROW(CAlnException, eUnknownMergeFailure,
                               "CAlnMix::x_Merge(): "
                               "Both Gen2EST and Nucl2Nucl "
                               "merges failed.");
                }
            }
        } else {
            x_Merge();
        }
    }
}


void CAlnMix::Add(const CSeq_align& aln, TAddFlags flags)
{
    if (m_InputAlnsMap.find((void *)&aln) == m_InputAlnsMap.end()) {
        // add only if not already added
        m_InputAlnsMap[(void *)&aln] = &aln;
        m_InputAlns.push_back(CConstRef<CSeq_align>(&aln));

        if (aln.GetSegs().IsDenseg()) {
            Add(aln.GetSegs().GetDenseg(), flags);
        } else if (aln.GetSegs().IsStd()) {
            CRef<CSeq_align> sa = aln.CreateDensegFromStdseg
                (m_Scope ? this : 0);
            Add(*sa, flags);
        } else if (aln.GetSegs().IsDisc()) {
            ITERATE (CSeq_align_set::Tdata,
                     aln_it,
                     aln.GetSegs().GetDisc().Get()) {
                Add(**aln_it, flags);
            }
        }
    }
}


void CAlnMix::Add(const CDense_seg &ds, TAddFlags flags)
{
    const CDense_seg* dsp = &ds;

    if (m_InputDSsMap.find((void *)dsp) != m_InputDSsMap.end()) {
        return; // it has already been added
    }
    x_Reset();
#if _DEBUG
    dsp->Validate(true);
#endif    

    // translate (extend with widths) the dense-seg if necessary
    if (flags & fForceTranslation  &&  !dsp->IsSetWidths()) {
        if ( !m_Scope ) {
            string errstr = string("CAlnMix::Add(): ") 
                + "Cannot force translation for Dense_seg "
                + NStr::IntToString(m_InputDSs.size() + 1) + ". "
                + "Neither CDense_seg::m_Widths are supplied, "
                + "nor OM is used to identify molecule type.";
            NCBI_THROW(CAlnException, eMergeFailure, errstr);
        } else {
            m_InputDSs.push_back(x_ExtendDSWithWidths(*dsp));
            dsp = m_InputDSs.back();
        }
    } else {
        m_InputDSs.push_back(CConstRef<CDense_seg>(dsp));
    }

    if ( !m_Scope  &&  flags & fCalcScore) {
        NCBI_THROW(CAlnException, eMergeFailure, "CAlnMix::Add(): "
                   "fCalcScore cannot be used without providing "
                   "a scope in the CAlnMix constructor.");
    }
    m_AddFlags = flags;

    m_InputDSsMap[(void *)dsp] = dsp;
    int ds_index = m_InputDSs.size();
    vector<CRef<CAlnMixSeq> > ds_seq;

    // check the widths
    if (dsp->IsSetWidths()) {
        if (dsp->GetWidths().size() != (size_t) dsp->GetDim()) {
            string errstr = string("CAlnMix::Add(): ")
                + "Dense-seg "
                + NStr::IntToString(ds_index)
                + " has incorrect widths size ("
                + NStr::IntToString(dsp->GetWidths().size())
                + "). Should be equal to its dim ("
                + NStr::IntToString(dsp->GetDim()) + ").";

            NCBI_THROW(CAlnException, eMergeFailure, errstr);
        }
    }

    //store the seqs
    for (CAlnMap::TNumrow row = 0;  row < dsp->GetDim();  row++) {

        CRef<CAlnMixSeq> aln_seq;

        if ( !m_Scope ) {
            // identify sequences by their seq ids as provided by
            // the dense seg (not as reliable as with OM, but faster)
            CRef<CSeq_id> seq_id(new CSeq_id);
            seq_id->Assign(*dsp->GetIds()[row]);

            TSeqIdMap::iterator it = m_SeqIds.find(seq_id);
            if (it == m_SeqIds.end()) {
                // add this seq id
                aln_seq = new CAlnMixSeq();
                m_SeqIds[seq_id] = aln_seq;
                aln_seq->m_SeqId = seq_id;
                aln_seq->m_DsCnt = 0;

                // add this sequence
                m_Seqs.push_back(aln_seq);
            
                // AA or NA?
                if (dsp->IsSetWidths()) {
                    if (dsp->GetWidths()[row] == 1) {
                        aln_seq->m_IsAA = true;
                        m_ContainsAA = true;
                    } else {
                        m_ContainsNA = true;
                    }
                }
                 
            } else {
                aln_seq = it->second;
            }
            
        } else {
            // uniquely identify the bioseq
            x_IdentifyAlnMixSeq(aln_seq, *(ds.GetIds())[row]);
#if _DEBUG
            // Verify the widths (if exist)
            if (dsp->IsSetWidths()) {
                const int& width = dsp->GetWidths()[row];
                if (width == 1  &&  aln_seq->m_IsAA != true  ||
                    width == 3  &&  aln_seq->m_IsAA != false) {
                    string errstr = string("CAlnMix::Add(): ")
                        + "Incorrect width(" 
                        + NStr::IntToString(width) +
                        ") or molecule type(" + 
                        (aln_seq->m_IsAA ? "AA" : "NA") +
                        ").";
                    NCBI_THROW(CAlnException, eInvalidSegment,
                               errstr);
                }
            }
#endif
        }

        // Preserve the row of the the original sequences if requested.
        // This is mostly used to allow a sequence to itself.
        // Create an additional sequence, pointed by m_AntoherRow,
        // if the row index differs.
        if (m_AddFlags & fPreserveRows) {
            int row_index = aln_seq->m_RowIdx;
            if (row_index == -1) {
                // initialization
                aln_seq->m_RowIdx = row;
            } else while (row_index != row) {
                if (aln_seq->m_AnotherRow) {
                    aln_seq   = aln_seq->m_AnotherRow;
                    row_index = aln_seq->m_RowIdx;
                } else {
                    CRef<CAlnMixSeq> another_row (new CAlnMixSeq);

                    another_row->m_BioseqHandle = aln_seq->m_BioseqHandle;
                    another_row->m_SeqId        = aln_seq->m_SeqId;
                    another_row->m_Width        = aln_seq->m_Width;
                    another_row->m_SeqIdx       = aln_seq->m_SeqIdx;
                    another_row->m_DsIdx        = ds_index;
                    another_row->m_RowIdx       = row;

                    m_Seqs.push_back(another_row);

                    aln_seq = aln_seq->m_AnotherRow = another_row;

                    break;
                }
            }
        }

        aln_seq->m_DsCnt++;
        ds_seq.push_back(aln_seq);
    }


    //record all alignment relations
    int              seg_off = 0;
    TSignedSeqPos    start1, start2;
    TSeqPos          len;
    bool             single_chunk;
    CAlnMap::TNumrow first_non_gapped_row_found;
    bool             strands_exist = 
        dsp->GetStrands().size() == (size_t)dsp->GetNumseg() * dsp->GetDim();

    for (CAlnMap::TNumseg seg =0;  seg < dsp->GetNumseg();  seg++) {
        len = dsp->GetLens()[seg];
        single_chunk = true;

        for (CAlnMap::TNumrow row1 = 0;  row1 < dsp->GetDim();  row1++) {
            if ((start1 = dsp->GetStarts()[seg_off + row1]) >= 0) {
                //search for a match for the piece of seq on row1

                CRef<CAlnMixSeq> aln_seq1 = ds_seq[row1];

                for (CAlnMap::TNumrow row2 = row1+1;
                     row2 < dsp->GetDim();  row2++) {
                    if ((start2 = dsp->GetStarts()[seg_off + row2]) >= 0) {
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
                        match->m_DsIdx = ds_index;

                        // determine the strand
                        match->m_StrandsDiffer = false;
                        ENa_strand strand1 = eNa_strand_plus;
                        ENa_strand strand2 = eNa_strand_plus;
                        if (strands_exist) {
                            if (dsp->GetStrands()[seg_off + row1] 
                                == eNa_strand_minus) {
                                strand1 = eNa_strand_minus;
                            }
                            if (dsp->GetStrands()[seg_off + row2] 
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
                        if (m_AddFlags & fCalcScore) {
                            // calc the score by seq comp
                            string s1, s2;

                            if (strand1 == eNa_strand_minus) {
                                CSeqVector seq_vec = 
                                    aln_seq1->m_BioseqHandle->GetSeqVector
                                    (CBioseq_Handle::eCoding_Iupac,
                                     CBioseq_Handle::eStrand_Minus);
                                TSeqPos size = seq_vec.size();
                                seq_vec.GetSeqData(size - (start1 + len),
                                                   size - start1, 
                                                   s1);
                            } else {
                                aln_seq1->m_BioseqHandle->GetSeqVector
                                    (CBioseq_Handle::eCoding_Iupac,
                                     CBioseq_Handle::eStrand_Plus).
                                    GetSeqData(start1, start1 + len, s1);
                            }                                
                            if (strand2 ==  eNa_strand_minus) {
                                CSeqVector seq_vec = 
                                    aln_seq2->m_BioseqHandle->GetSeqVector
                                    (CBioseq_Handle::eCoding_Iupac,
                                     CBioseq_Handle::eStrand_Minus);
                                TSeqPos size = seq_vec.size();
                                seq_vec.GetSeqData(size - (start2 + len),
                                                   size - start2, 
                                                   s2);
                            } else {
                                aln_seq2->m_BioseqHandle->GetSeqVector
                                    (CBioseq_Handle::eCoding_Iupac,
                                     CBioseq_Handle::eStrand_Plus).
                                    GetSeqData(start2, start2 + len, s2);
                            }

                            //verify that we were able to load all data
                            if (s1.length() != len || s2.length() != len) {

                                string symptoms  = "Input Dense-seg " +
                                    NStr::IntToString(m_InputDSs.size()) + ":" +
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
                                    NStr::IntToString(s1.length() < + len ? row1 : row2) +
                                    " are out of range.";

                                string errstr = string("CAlnMix::Add(): ") +
                                    symptoms + " " + diagnosis;
                                NCBI_THROW(CAlnException, eInvalidSegment,
                                           errstr);
                            }

                            match->m_Score = 
                                CAlnVec::CalculateScore
                                (s1, s2, aln_seq1->m_IsAA, aln_seq2->m_IsAA);
                        } else {
                            match->m_Score = len;
                        }
                        

                        // add to the sequences' scores
                        aln_seq1->m_Score += match->m_Score;
                        aln_seq2->m_Score += match->m_Score;

                        // in case of fForceTranslation, 
                        // check if strands are not mixed by
                        // comparing current strand to the prevailing one
                        if (m_AddFlags & fForceTranslation  &&
                            (aln_seq1->m_StrandScore > 0  && 
                             strand1 == eNa_strand_minus ||
                             aln_seq1->m_StrandScore < 0  && 
                             strand1 != eNa_strand_minus ||
                             aln_seq2->m_StrandScore > 0  && 
                             strand2 == eNa_strand_minus ||
                             aln_seq2->m_StrandScore < 0  && 
                             strand2 != eNa_strand_minus)) {
                            NCBI_THROW(CAlnException, eMergeFailure,
                                       "CAlnMix::Add(): "
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
                    match->m_DsIdx = m_InputDSs.size();
                    m_Matches.push_back(match);
                }
            }
        }
        seg_off += dsp->GetDim();
    }
}


void CAlnMix::x_Merge()
{
    bool first_refseq = true; // mark the first loop

    if (m_MergeFlags & fSortSeqsByScore) {
        stable_sort(m_Seqs.begin(), m_Seqs.end(), x_CompareAlnSeqScores);
    }

    // Find the refseq (if such exists)
    {{
        m_SingleRefseq = false;
        m_IndependentDSs = m_InputDSs.size() > 1;

        unsigned int ds_cnt;
        NON_CONST_ITERATE (TSeqs, it, m_Seqs){
            ds_cnt = (*it)->m_DsCnt;
            if (ds_cnt > 1) {
                m_IndependentDSs = false;
            }
            if (ds_cnt == m_InputDSs.size()) {
                m_SingleRefseq = true;
                if ( !first_refseq ) {
                    CRef<CAlnMixSeq> refseq = *it;
                    m_Seqs.erase(it);
                    m_Seqs.insert(m_Seqs.begin(), refseq);
                }
                break;
            }
            first_refseq = false;
        }
    }}

    // Index the sequences
    {{
        int seq_idx=0;
        NON_CONST_ITERATE (TSeqs, seq_i, m_Seqs) {
            (*seq_i)->m_SeqIdx = seq_idx++;
        }
    }}

    // Set the widths if the mix contains both AA & NA
    // or in case we force translation
    if (m_ContainsNA  &&  m_ContainsAA  ||  m_AddFlags & fForceTranslation) {
        NON_CONST_ITERATE (TSeqs, seq_i, m_Seqs) {
            (*seq_i)->m_Width = (*seq_i)->m_IsAA ? 1 : 3;
        }
    }

    // Sort matches by score
    stable_sort(m_Matches.begin(), m_Matches.end(), x_CompareAlnMatchScores);

    CAlnMixSeq * refseq = 0, * seq1 = 0, * seq2 = 0;
    TSeqPos start, start1, start2, len, curr_len;
    int width1, width2;
    CAlnMixMatch * match;
    CAlnMixSeq::TMatchList::iterator match_list_iter1, match_list_iter2;
    CAlnMixSeq::TMatchList::iterator match_list_i;
    TSecondRowFits second_row_fits;

    refseq = *(m_Seqs.begin());
    TMatches::iterator match_i = m_Matches.begin();
    m_MatchIdx = 0;
    
    CRef<CAlnMixSegment> seg;
    CAlnMixSeq::TStarts::iterator start_i, lo_start_i, hi_start_i, tmp_start_i;

    first_refseq = true; // mark the first loop

    while (true) {
        // reached the end?
        if (first_refseq ?
            match_i == m_Matches.end()  &&  m_Matches.size() :
            match_list_i == refseq->m_MatchList.end()) {

            // move on to the next refseq
            refseq->m_RefBy = 0;

            // try to find the best scoring 'connected' candidate
            NON_CONST_ITERATE (TSeqs, it, m_Seqs){
                if ( !((*it)->m_MatchList.empty())  &&
                     (*it)->m_RefBy == refseq) {
                    refseq = *it;
                    break;
                }
            }
            if (refseq->m_RefBy == 0) {
                // no candidate was found 'connected' to the refseq
                // continue with the highest scoring candidate
                NON_CONST_ITERATE (TSeqs, it, m_Seqs){
                    if ( !((*it)->m_MatchList.empty()) ) {
                        refseq = *it;
                        break;
                    }
                }
            }

            if (refseq->m_MatchList.empty()) {
                break; // done
            } else {
                first_refseq = false;
                match_list_i = refseq->m_MatchList.begin();
            }
            continue;
        } else {
            // iterate
            match = first_refseq ? *(match_i++) : *(match_list_i++);
        }

        curr_len = len = match->m_Len;

        // is it a match with this refseq?
        if (match->m_AlnSeq1 == refseq) {
            seq1 = match->m_AlnSeq1;
            start1 = match->m_Start1;
            match_list_iter1 = match->m_MatchIter1;
            seq2 = match->m_AlnSeq2;
            start2 = match->m_Start2;
            match_list_iter2 = match->m_MatchIter2;
        } else if (match->m_AlnSeq2 == refseq) {
            seq1 = match->m_AlnSeq2;
            start1 = match->m_Start2;
            match_list_iter1 = match->m_MatchIter2;
            seq2 = match->m_AlnSeq1;
            start2 = match->m_Start1;
            match_list_iter2 = match->m_MatchIter1;
        } else {
            seq1 = match->m_AlnSeq1;
            seq2 = match->m_AlnSeq2;

            // mark the two refseqs, they are candidates for next refseq(s)
            seq1->m_MatchList.push_back(match);
            (match->m_MatchIter1 = seq1->m_MatchList.end())--;
            if (seq2) {
                seq2->m_MatchList.push_back(match);
                (match->m_MatchIter2 = seq2->m_MatchList.end())--;
            }

            // mark that there's no match with this refseq
            seq1 = 0;
        }

        // save the match info into the segments map
        if (seq1) {
            m_MatchIdx++;

            // order the match
            match->m_AlnSeq1 = seq1;
            match->m_Start1 = start1;
            match->m_AlnSeq2 = seq2;
            match->m_Start2 = start2;

            width1 = seq1->m_Width;
            if (seq2) {
                width2 = seq2->m_Width;
            }

            // in case of translated refseq
            if (width1 == 3) {
                x_SetSeqFrame(match, match->m_AlnSeq1);
                {{
                    // reset the ones below,
                    // since match may have been modified
                    seq1 = match->m_AlnSeq1;
                    start1 = match->m_Start1;
                    match_list_iter1 = match->m_MatchIter1;
                    seq2 = match->m_AlnSeq2;
                    start2 = match->m_Start2;
                    match_list_iter2 = match->m_MatchIter2;
                    curr_len = len = match->m_Len;
                }}
            }

            // this match is used, erase from seq1 list
            if ( !first_refseq ) {
                if ( !refseq->m_MatchList.empty() ) {
                    refseq->m_MatchList.erase(match_list_iter1);
                }
            }

            // if a subject sequence place it in the proper row
            if ( !first_refseq  &&  m_MergeFlags & fQuerySeqMergeOnly) {
                bool proper_row_found = false;
                while (true) {
                    if (seq1->m_DsIdx == match->m_DsIdx) {
                        proper_row_found = true;
                        break;
                    } else {
                        if (seq1->m_ExtraRow) {
                            seq1 = match->m_AlnSeq1 = seq1->m_ExtraRow;
                        } else {
                            break;
                        }
                    }
                }
                if ( !proper_row_found ) {
                    NCBI_THROW(CAlnException, eMergeFailure,
                               "CAlnMix::x_Merge(): "
                               "Proper row not found for the match. "
                               "Cannot use fQuerySeqMergeOnly?");
                }
            }
            
            CAlnMixSeq::TStarts& starts = seq1->m_Starts;
            if (seq2) {
                // mark it, it is linked to the refseq
                seq2->m_RefBy = refseq;

                // this match is used erase from seq2 list
                if ( !first_refseq ) {
                    if ( !seq2->m_MatchList.empty() ) {
                        seq2->m_MatchList.erase(match_list_iter2);
                    }
                }
            }

            start_i = starts.end();
            lo_start_i = starts.end();
            hi_start_i = starts.end();


            if (seq2) {
                if (width2 == 3) {
                    // Set the frame if necessary
                    x_SetSeqFrame(match, match->m_AlnSeq2);
                }
                // check if the second row fits
                // this will truncate the match if 
                // there's an inconsistent overlap
                // and truncation was requested
                second_row_fits = x_SecondRowFits(match);
                if (second_row_fits == eIgnoreMatch) {
                    continue;
                }
                {{
                    // reset the ones below,
                    // since match may have been modified
                    seq1 = match->m_AlnSeq1;
                    start1 = match->m_Start1;
                    match_list_iter1 = match->m_MatchIter1;
                    seq2 = match->m_AlnSeq2;
                    start2 = match->m_Start2;
                    match_list_iter2 = match->m_MatchIter2;
                    curr_len = len = match->m_Len;
                }}
            }


            if (!starts.size()) {
                // no starts exist yet

                if ( !m_IndependentDSs ) {
                    // TEMPORARY, for the single refseq version of mix,
                    // clear all MatchLists and exit
                    if ( !(m_SingleRefseq  ||  first_refseq) ) {
                        NON_CONST_ITERATE (TSeqs, it, m_Seqs){
                            if ( !((*it)->m_MatchList.empty()) ) {
                                (*it)->m_MatchList.clear();
                            }
                        }
                        break; 
                    }
                }

                // this seq has not yet been used, set the strand
                if (m_AddFlags & fForceTranslation) {
                    seq1->m_PositiveStrand = (seq1->m_StrandScore >= 0);
                } else {
                    seq1->m_PositiveStrand = ! (m_MergeFlags & fNegativeStrand);
                }

                //create the first one
                seg = new CAlnMixSegment;
                seg->m_Len = len;
                seg->m_DsIdx = match->m_DsIdx;
                starts[start1] = seg;
                seg->m_StartIts[seq1] = 
                    lo_start_i = hi_start_i = starts.begin();

                if (m_MergeFlags & fQuerySeqMergeOnly) {
                    seq2->m_DsIdx = match->m_DsIdx;
                }
                // DONE!
            } else {
                // some starts exist already

                // look ahead
                if ((lo_start_i = start_i = starts.lower_bound(start1))
                    == starts.end()  ||
                    start1 < start_i->first) {
                    // the start position does not exist
                    if (lo_start_i != starts.begin()) {
                        --lo_start_i;
                    }
                }

                // look back
                if (hi_start_i == starts.end()  &&  start_i != lo_start_i) {
                    CAlnMixSegment * prev_seg = lo_start_i->second;
                    if (lo_start_i->first + prev_seg->m_Len * width1 >
                        start1) {
                        // x----..   becomes  x-x--..
                        //   x--..
                        
                        TSeqPos len1 = (start1 - lo_start_i->first) / width1;
                        TSeqPos len2 = prev_seg->m_Len - len1;
                        
                        // create the second seg
                        seg = new CAlnMixSegment;
                        seg->m_Len = len2;
                        seg->m_DsIdx = match->m_DsIdx;
                        starts[start1] = seg;
                        
                        // create rows info
                        ITERATE (CAlnMixSegment::TStartIterators, it, 
                                 prev_seg->m_StartIts) {
                            CAlnMixSeq * seq = it->first;
                            tmp_start_i = it->second;
                            if (seq->m_PositiveStrand ==
                                seq1->m_PositiveStrand) {
                                seq->m_Starts
                                    [tmp_start_i->first + len1 * seq->m_Width]
                                    = seg;
                                if (tmp_start_i != seq->m_Starts.end()) {
                                    seg->m_StartIts[seq] = ++tmp_start_i;
                                } else {
                                    NCBI_THROW(CAlnException, eMergeFailure,
                                               "CAlnMix::x_Merge(): "
                                               "Internal error: tmp_start_i == seq->m_Starts.end()");
                                }
                            } else {
                                seq->m_Starts
                                    [tmp_start_i->first + len2 * seq->m_Width]
                                    = prev_seg;
                                seq->m_Starts[tmp_start_i->first] = seg;
                                seg->m_StartIts[seq] = tmp_start_i;
                                if (tmp_start_i != seq->m_Starts.end()) {
                                    prev_seg->m_StartIts[seq] = ++tmp_start_i;
                                } else {
                                    NCBI_THROW(CAlnException, eMergeFailure,
                                               "CAlnMix::x_Merge(): "
                                               "Internal error: tmp_start_i == seq->m_Starts.end()");
                                }
                            }
                        }
                        
                        // truncate the first seg
                        prev_seg->m_Len = len1;
                        
                        if (start_i != starts.begin()) {
                            start_i--; // point to the newly created start
                        }
                    }
                    if (lo_start_i != starts.end()) {
                        lo_start_i++;
                    }
                }
            }

            // loop through overlapping segments
            start = start1;
            while (hi_start_i == starts.end()) {
                if (start_i != starts.end()  &&  start_i->first == start) {
                    CAlnMixSegment * prev_seg = start_i->second;
                    if (prev_seg->m_Len > curr_len) {
                        // x-------)  becomes  x----)x--)
                        // x----)
                        
                        // create the second seg
                        seg = new CAlnMixSegment;
                        TSeqPos len1 = 
                            seg->m_Len = prev_seg->m_Len - curr_len;
                        start += curr_len * width1;

                        // truncate the first seg
                        prev_seg->m_Len = curr_len;
                        
                        // create rows info
                        ITERATE (CAlnMixSegment::TStartIterators, it, 
                                prev_seg->m_StartIts) {
                            CAlnMixSeq * seq = it->first;
                            tmp_start_i = it->second;
                            if (seq->m_PositiveStrand ==
                                seq1->m_PositiveStrand) {
                                seq->m_Starts[tmp_start_i->first +
                                             curr_len * seq->m_Width]
                                    = seg;
                                if (tmp_start_i != seq->m_Starts.end()) {
                                    seg->m_StartIts[seq] = ++tmp_start_i;
                                } else {
                                    NCBI_THROW(CAlnException, eMergeFailure,
                                               "CAlnMix::x_Merge(): "
                                               "Internal error: tmp_start_i == seq->m_Starts.end()");
                                }
                            } else{
                                seq->m_Starts[tmp_start_i->first +
                                             len1 * seq->m_Width]
                                    = prev_seg;
                                seq->m_Starts[tmp_start_i->first] = seg;
                                seg->m_StartIts[seq] = tmp_start_i;
                                if (tmp_start_i != seq->m_Starts.end()) {
                                    prev_seg->m_StartIts[seq] = ++tmp_start_i;
                                } else {
                                    NCBI_THROW(CAlnException, eMergeFailure,
                                               "CAlnMix::x_Merge(): "
                                               "Internal error: tmp_start_i == seq->m_Starts.end()");
                                }
                            }
                        }
#if _DEBUG && _ALNMGR_DEBUG                        
                        x_SegmentStartItsConsistencyCheck(*prev_seg,
                                                          *seq1,
                                                          start);
#endif


                        hi_start_i = start_i; // DONE!
                    } else if (curr_len == prev_seg->m_Len) {
                        // x----)
                        // x----)
                        hi_start_i = start_i; // DONE!
                    } else {
                        // x----)     becomes  x----)x--)
                        // x-------)
                        start += prev_seg->m_Len * width1;
                        curr_len -= prev_seg->m_Len;
                        if (start_i != starts.end()) {
                            start_i++;
                        }
                    }
                } else {
                    seg = new CAlnMixSegment;
                    starts[start] = seg;
                    tmp_start_i = start_i;
                    if (tmp_start_i != starts.begin()) {
                        tmp_start_i--;
                    }
                    seg->m_StartIts[seq1] = tmp_start_i;
                    if (start_i != starts.end()  &&
                        start + curr_len * width1 > start_i->first) {
                        //       x--..
                        // x--------..
                        seg->m_Len = (start_i->first - start) / width1;
                        seg->m_DsIdx = match->m_DsIdx;
                    } else {
                        //       x-----)
                        // x---)
                        seg->m_Len = curr_len;
                        seg->m_DsIdx = match->m_DsIdx;
                        hi_start_i = start_i;
                        if (hi_start_i != starts.begin()) {
                            hi_start_i--; // DONE!
                        }
                    }
                    start += seg->m_Len * width1;
                    curr_len -= seg->m_Len;
                    if (lo_start_i == start_i) {
                        if (lo_start_i != starts.begin()) {
                            lo_start_i--;
                        }
                    }
                }
            }
                 
            // try to resolve the second row
            if (seq2) {
                // create a copy of the match,
                // which we could work with temporarily
                // without modifying the original
                CAlnMixMatch tmp_match = *match;
                match = &tmp_match; // point to the new tmp_match

                if (second_row_fits == eFirstRowOverlapBelow  ||
                    second_row_fits == eFirstRowOverlapAbove) {
                    // try it again, it may fit this time
                    // since the second row may have been
                    // cut into smaller segments
                    // or new frame was set
                    second_row_fits = x_SecondRowFits(match);
                }
                while (second_row_fits != eSecondRowFitsOk  &&
                       second_row_fits != eIgnoreMatch) {
                    if (!seq2->m_ExtraRow) {
                        // create an extra row
                        CRef<CAlnMixSeq> row (new CAlnMixSeq);
                        row->m_BioseqHandle = seq2->m_BioseqHandle;
                        row->m_SeqId = seq2->m_SeqId;
                        row->m_Width = seq2->m_Width;
                        row->m_Frame = start2 % 3;
                        row->m_SeqIdx = seq2->m_SeqIdx;
                        if (m_MergeFlags & fQuerySeqMergeOnly) {
                            row->m_DsIdx = match->m_DsIdx;
                        }
                        m_ExtraRows.push_back(row);
                        row->m_ExtraRowIdx = seq2->m_ExtraRowIdx + 1;
                        seq2 = match->m_AlnSeq2 = seq2->m_ExtraRow = row;
                        break;
                    }
                    seq2 = match->m_AlnSeq2 = seq2->m_ExtraRow;

                    second_row_fits = x_SecondRowFits(match);
                    {{
                        // reset the ones below,
                        // since match may have been modified
                        seq1 = match->m_AlnSeq1;
                        start1 = match->m_Start1;
                        match_list_iter1 = match->m_MatchIter1;
                        seq2 = match->m_AlnSeq2;
                        start2 = match->m_Start2;
                        match_list_iter2 = match->m_MatchIter2;
                        curr_len = len = match->m_Len;
                    }}
                }
                if (second_row_fits == eIgnoreMatch) {
                    continue;
                }

                if (m_MergeFlags & fTruncateOverlaps) {
                    // we need to reset these shtorcuts
                    // in case the match was truncated
                    start1 = match->m_Start1;
                    start2 = match->m_Start2;
                    len = match->m_Len;
                }

                // set the strand if first time
                if (seq2->m_Starts.empty()) {
                    seq2->m_PositiveStrand = 
                        (seq1->m_PositiveStrand ?
                         !match->m_StrandsDiffer :
                         match->m_StrandsDiffer);
                }

                // create row info
                CAlnMixSeq::TStarts& starts2 = match->m_AlnSeq2->m_Starts;
                start = start2;
                CAlnMixSeq::TStarts::iterator start2_i
                    = starts2.lower_bound(start2);
                start_i = match->m_StrandsDiffer ? hi_start_i : lo_start_i;

                while(start < start2 + len * width2) {
                    if (start2_i != starts2.end() &&
                        start2_i->first == start) {
                        // this position already exists

                        if (start2_i->second != start_i->second) {
                            // merge the two segments

                            // store the seg in a CRef to delay its deletion
                            // until after the iteration on it is finished
                            CRef<CAlnMixSegment> tmp_seg = start2_i->second;

                            ITERATE (CAlnMixSegment::TStartIterators,
                                     it, 
                                     tmp_seg->m_StartIts) {
                                CAlnMixSeq * tmp_seq = it->first;
                                tmp_start_i = it->second;
                                tmp_start_i->second = start_i->second;
                                start_i->second->m_StartIts[tmp_seq] =
                                    tmp_start_i;
                            }
#if _DEBUG && _ALNMGR_DEBUG                        
                            x_SegmentStartItsConsistencyCheck(*start_i->second,
                                                              *seq2,
                                                              start);
#endif
                        }
                    } else {
                        // this position does not exist, create it
                        seq2->m_Starts[start] = start_i->second;

                        // start2_i != starts.begin() because we just 
                        // made an insertion, so decrement is ok
                        start2_i--;
                        
                        // point this segment's row start iterator
                        start_i->second->m_StartIts[seq2] = start2_i;
#if _DEBUG && _ALNMGR_DEBUG                        
                        x_SegmentStartItsConsistencyCheck(*(start_i->second),
                                                          *seq2,
                                                          start);
#endif
                    }

                    // increment values
                    start += start_i->second->m_Len * width2;
                    if (start2_i != starts2.end()) {
                        start2_i++;
                    }
                    if (match->m_StrandsDiffer) {
                        if (start_i != starts.begin()) {
                            start_i--;
                        }
                    } else {
                        if (start_i != starts.end()) {
                            start_i++;
                        }
                    }
                }
            }
        }
    }
    x_CreateRowsVector();
    x_CreateSegmentsVector();
    x_CreateDenseg();
}


void
CAlnMix::x_SetSeqFrame(CAlnMixMatch* match, CAlnMixSeq*& seq)
{
    int frame;
    if (seq == match->m_AlnSeq1) {
        frame = match->m_Start1 % 3;
    } else {
        frame = match->m_Start2 % 3;
    }
    if (seq->m_Starts.empty()) {
        seq->m_Frame = frame;
    } else {
        while (seq->m_Frame != frame) {
            if (!seq->m_ExtraRow) {
                // create an extra frame
                CRef<CAlnMixSeq> new_seq (new CAlnMixSeq);
                new_seq->m_BioseqHandle = seq->m_BioseqHandle;
                new_seq->m_SeqId = seq->m_SeqId;
                new_seq->m_PositiveStrand = seq->m_PositiveStrand;
                new_seq->m_Width = seq->m_Width;
                new_seq->m_Frame = frame;
                new_seq->m_SeqIdx = seq->m_SeqIdx;
                if (m_MergeFlags & fQuerySeqMergeOnly) {
                    new_seq->m_DsIdx = match->m_DsIdx;
                }
                m_ExtraRows.push_back(new_seq);
                new_seq->m_ExtraRowIdx = seq->m_ExtraRowIdx + 1;
                seq = seq->m_ExtraRow = new_seq;
                break;
            }
            seq = seq->m_ExtraRow;
        }
    }
}


CAlnMix::TSecondRowFits
CAlnMix::x_SecondRowFits(CAlnMixMatch * match) const
{
    CAlnMixSeq*&                  seq1    = match->m_AlnSeq1;
    CAlnMixSeq*&                  seq2    = match->m_AlnSeq2;
    TSeqPos&                      start1  = match->m_Start1;
    TSeqPos&                      start2  = match->m_Start2;
    TSeqPos&                      len     = match->m_Len;
    const int&                    width1  = seq1->m_Width;
    const int&                    width2  = seq2->m_Width;
    CAlnMixSeq::TStarts::iterator start_i;
    TSignedSeqPos                 delta, delta1, delta2;

    // subject sequences go on separate rows if requested
    if (m_MergeFlags & fQuerySeqMergeOnly) {
        if (seq2->m_DsIdx) {
            if ( !(m_MergeFlags & fTruncateOverlaps) ) {
                if (seq2->m_DsIdx == match->m_DsIdx) {
                    return eSecondRowFitsOk;
                } else {
                    return eForceSeparateRow;
                }
            }
        } else {
            seq2->m_DsIdx = match->m_DsIdx;
            if ( !(m_MergeFlags & fTruncateOverlaps) ) {
                return eSecondRowFitsOk;
            }
        }
    }

    if ( !seq2->m_Starts.empty() ) {

        // check strand
        while (true) {
            if (seq2->m_PositiveStrand !=
                (seq1->m_PositiveStrand ?
                 !match->m_StrandsDiffer :
                 match->m_StrandsDiffer)) {
                if (seq2->m_ExtraRow) {
                    seq2 = seq2->m_ExtraRow;
                } else {
                    return eInconsistentStrand;
                }
            } else {
                break;
            }
        }

        // check frame
        if (seq2->m_Width == 3  &&  seq2->m_Frame != start2 % 3) {
            return eInconsistentFrame;
        }

        start_i = seq2->m_Starts.lower_bound(start2);

        // check below
        if (start_i != seq2->m_Starts.begin()) {
            start_i--;
            
            // check for inconsistency on the first row
            if ( !m_IndependentDSs ) {
                CAlnMixSegment::TStartIterators::iterator start_it_i =
                    start_i->second->m_StartIts.find(seq1);
                if (start_it_i != start_i->second->m_StartIts.end()) {
                    if (match->m_StrandsDiffer) {
                        delta = start1 + len * width1 - start_it_i->second->first;
                        if (delta > 0) {
                            // target above
                            // x----- x-)-)
                            // (----x (---x
                            // target below
                            if (m_MergeFlags & fTruncateOverlaps) {
                                delta /= width1;
                                if (len > delta) {
                                    len -= delta;
                                    start2 += delta * width2;
                                } else {
                                    return eIgnoreMatch;
                                }
                            } else {
                                return eFirstRowOverlapAbove;
                            }
                        }
                    } else {
                        delta = start_it_i->second->first
                            + start_i->second->m_Len * width1
                            - start1;
                        if (delta > 0) {
                            // below target
                            // x---- x-)--)
                            // x---) x----)
                            // below target
                            if (m_MergeFlags & fTruncateOverlaps) {
                                delta /= width1;
                                if (len > delta) {
                                    len -= delta;
                                    start1 += delta * width1;
                                    start2 += delta * width2;
                                } else {
                                    return eIgnoreMatch;
                                }
                            } else {
                                return eFirstRowOverlapBelow;
                            }
                        }
                    }
                }
            }

            // check for overlap with the segment below on second row
            if ((delta = start_i->first + start_i->second->m_Len * width2
                 - start2) > 0) {
                //       target
                // ----- ------
                // x---- x-)--)
                // below target
                if (m_MergeFlags & fTruncateOverlaps) {
                    delta /= width2;
                    if (len > delta) {
                        len -= delta;
                        start2 += delta * width2;
                        if ( !match->m_StrandsDiffer ) {
                            start1 += delta * width1;
                        }
                    } else {
                        return eIgnoreMatch;
                    }
                } else {
                    return eSecondRowOverlap;
                }
            }
            if (start_i != seq2->m_Starts.end()) {
                start_i++;
            }
        }

        // check the overlapping area for consistency
        while (start_i != seq2->m_Starts.end()  &&  
               start_i->first < start2 + len * width2) {
            if ( !m_IndependentDSs ) {
                CAlnMixSegment::TStartIterators::iterator start_it_i =
                    start_i->second->m_StartIts.find(seq1);
                if (start_it_i != start_i->second->m_StartIts.end()) {
                    if (match->m_StrandsDiffer) {
                        // x---..- x---..--)
                        // (---..- (--x..--x
                        delta1 = (start1 - start_it_i->second->first) / width1 +
                            len - start_i->second->m_Len;
                    } else {
                        // x--..- x---...-)
                        // x--..- x---...-)
                        delta1 = (start_it_i->second->first - start1) / width1;
                    }
                    delta2 = (start_i->first - start2) / width2;
                    if (delta1 != delta2) {
                        if (m_MergeFlags & fTruncateOverlaps) {
                            delta = (delta1 < delta2 ? delta1 : delta2);
                            if (delta > 0) {
                                if (match->m_StrandsDiffer) {
                                    start1 += (len - delta) * width1;
                                }
                                len = delta;
                            } else {
                                return eIgnoreMatch;
                            }
                        } else {
                            return eInconsistentOverlap;
                        }
                    }
                }
            }
            start_i++;
        }

        // check above for consistency
        if (start_i != seq2->m_Starts.end()) {
            if ( !m_IndependentDSs ) {
                CAlnMixSegment::TStartIterators::iterator start_it_i =
                    start_i->second->m_StartIts.find(seq1);
                if (start_it_i != start_i->second->m_StartIts.end()) {
                    if (match->m_StrandsDiffer) {
                        delta = start_it_i->second->first + 
                            start_i->second->m_Len * width1 - start1;
                        if (delta > 0) {
                            // below target
                            // x---- x-)--)
                            // (---x (----x
                            // above target
                            if (m_MergeFlags & fTruncateOverlaps) {
                                if (len > delta / width1) {
                                    len -= delta / width1;
                                    start1 += delta;
                                } else {
                                    return eIgnoreMatch;
                                }
                            } else {
                                return eFirstRowOverlapBelow;
                            }
                        }
                    } else {
                        delta = start1 + len * width1 - start_it_i->second->first;
                        if (delta > 0) {
                            // target above
                            // x--x-) ----)
                            // x----) x---)
                            // target above
                            if (m_MergeFlags & fTruncateOverlaps) {
                                if (len <= delta / width1) {
                                    return eIgnoreMatch;
                                } else {
                                    len -= delta / width1;
                                }
                            } else {
                                return eFirstRowOverlapAbove;
                            }
                        }
                    }
                }
            }
        }

        // check for inconsistent matches
        if ((start_i = seq1->m_Starts.find(start1)) == seq1->m_Starts.end() ||
            start_i->first != start1) {
            // commenting out for now, since moved the function call ahead            
//             NCBI_THROW(CAlnException, eMergeFailure,
//                        "CAlnMix::x_SecondRowFits(): "
//                        "Internal error: seq1->m_Starts do not match");
        } else {
            CAlnMixSegment::TStartIterators::iterator it;
            TSeqPos tmp_start =
                match->m_StrandsDiffer ? start2 + len * width2 : start2;
            while (start_i != seq1->m_Starts.end()  &&
                   start_i->first < start1 + len * width1) {

                CAlnMixSegment::TStartIterators& its = 
                    start_i->second->m_StartIts;

                if (match->m_StrandsDiffer) {
                    tmp_start -= start_i->second->m_Len * width2;
                }

                if ((it = its.find(seq2)) != its.end()) {
                    if (it->second->first != tmp_start) {
                        // found an inconsistent prev match
                        return eSecondRowInconsistency;
                    }
                }

                if ( !match->m_StrandsDiffer ) {
                    tmp_start += start_i->second->m_Len * width2;
                }

                start_i++;
            }
        }
    }
    if (m_MergeFlags & fQuerySeqMergeOnly) {
        _ASSERT(m_MergeFlags & fTruncateOverlaps);
        if (seq2->m_DsIdx == match->m_DsIdx) {
            return eSecondRowFitsOk;
        } else {
            return eForceSeparateRow;
        }
    }
    return eSecondRowFitsOk;
}

void CAlnMix::x_CreateRowsVector()
{
    m_Rows.clear();

    int count = 0;
    NON_CONST_ITERATE (TSeqs, i, m_Seqs) {
        CRef<CAlnMixSeq>& seq = *i;
        m_Rows.push_back(seq);
        seq->m_RowIdx = count++;
        while ((seq = seq->m_ExtraRow) != NULL ) {
            seq->m_RowIdx = count++;
            m_Rows.push_back(seq);
        }
    }
}


void CAlnMix::x_CreateSegmentsVector()
{
    TSegmentsContainer gapped_segs;

    // init the start iterators for each row
    NON_CONST_ITERATE (TSeqs, row_i, m_Rows) {
        CAlnMixSeq * row = *row_i;
        if ( !row->m_Starts.empty() ) {
            if (row->m_PositiveStrand) {
                row->m_StartIt = row->m_Starts.begin();
            } else {
                row->m_StartIt = row->m_Starts.end();
                row->m_StartIt--;
            }
        } else {
            row->m_StartIt = row->m_Starts.end();
#if _DEBUG
            string errstr =
                string("CAlnMix::x_CreateSegmentsVector():") +
                " Internal error: no starts for row " +
                NStr::IntToString(row->m_RowIdx) +
                " (seq " +
                NStr::IntToString(row->m_SeqIdx) + ").";
            NCBI_THROW(CAlnException, eMergeFailure, errstr);
#endif
        }
    }

    // init the start iterators for each extra row
    NON_CONST_ITERATE (list<CRef<CAlnMixSeq> >, row_i, m_ExtraRows) {
        CAlnMixSeq * row = *row_i;
        if ( !row->m_Starts.empty() ) {
            if (row->m_PositiveStrand) {
                row->m_StartIt = row->m_Starts.begin();
            } else {
                row->m_StartIt = row->m_Starts.end();
                row->m_StartIt--;
            }
        } else {
            row->m_StartIt = row->m_Starts.end();
#if _DEBUG
            string errstr =
                string("CAlnMix::x_CreateSegmentsVector():") +
                " Internal error: no starts for row " +
                NStr::IntToString(row->m_RowIdx) + ".";
            NCBI_THROW(CAlnException, eMergeFailure, errstr);
#endif
        }
    }

#if _DEBUG
    ITERATE (TSeqs, row_i, m_Rows) {
        ITERATE (CAlnMixSegment::TStarts, st_i, (*row_i)->m_Starts) {
            x_SegmentStartItsConsistencyCheck(*(st_i->second),
                                              **row_i,
                                              st_i->first);
        }
         
    }       
#endif

    TSeqs::iterator refseq_it = m_Rows.begin();
    bool orig_refseq = true;
    while (true) {
        CAlnMixSeq * refseq = 0;
        while (refseq_it != m_Rows.end()) {
            refseq = *(refseq_it++);
            if (refseq->m_StartIt != refseq->m_Starts.end()) {
                break;
            } else {
                refseq = 0;
            }
        }
        if ( !refseq ) {
            // Done

            // add the gapped segments if any
            if (gapped_segs.size()) {
                if (m_MergeFlags & fGapJoin) {
                    // request to try to align
                    // gapped segments w/ equal len
                    x_ConsolidateGaps(gapped_segs);
                } else if (m_MergeFlags & fMinGap) {
                    // request to try to align 
                    // all gapped segments
                    x_MinimizeGaps(gapped_segs);
                }
                NON_CONST_ITERATE (TSegmentsContainer,
                                   seg_i, gapped_segs) {
                    m_Segments.push_back(&**seg_i);
                }
                gapped_segs.clear();
            }
            break; // from the main loop
        }
#if _ALNMGR_TRACE
        cerr << "refseq is on row " << refseq->m_RowIdx
             << " seq " << refseq->m_SeqIdx << "\n";
#endif
        // for each refseq segment
        while (refseq->m_StartIt != refseq->m_Starts.end()) {
            stack< CRef<CAlnMixSegment> > seg_stack;
            seg_stack.push(refseq->m_StartIt->second);
#if _ALNMGR_TRACE
            cerr << "  [row " << refseq->m_RowIdx
                 << " seq " << refseq->m_SeqIdx
                 << " start " << refseq->m_StartIt->first
                 << " was pushed into stack\n";
#endif
            
            while ( !seg_stack.empty() ) {
                
                bool pop_seg = true;
                // check the gapped segments on the left
                ITERATE (CAlnMixSegment::TStartIterators, start_its_i,
                         seg_stack.top()->m_StartIts) {

                    CAlnMixSeq * row = start_its_i->first;

                    if (row->m_StartIt != start_its_i->second) {
#if _DEBUG
                        if (row->m_PositiveStrand ?
                            row->m_StartIt->first >
                            start_its_i->second->first :
                            row->m_StartIt->first <
                            start_its_i->second->first) {
                            string errstr =
                                string("CAlnMix::x_CreateSegmentsVector():")
                                + " Internal error: Integrity broken" +
                                " row=" + NStr::IntToString(row->m_RowIdx) +
                                " seq=" + NStr::IntToString(row->m_SeqIdx)
                                + " row->m_StartIt->first="
                                + NStr::IntToString(row->m_StartIt->first)
                                + " start_its_i->second->first=" +
                                NStr::IntToString(start_its_i->second->first)
                                + " refseq->m_StartIt->first=" +
                                NStr::IntToString(refseq->m_StartIt->first)
                                + " strand=" +
                                (row->m_PositiveStrand ? "plus" : "minus");
                            NCBI_THROW(CAlnException, eMergeFailure, errstr);
                        }
#endif
                        seg_stack.push(row->m_StartIt->second);
#if _ALNMGR_TRACE
                        cerr << "  [row " << row->m_RowIdx
                             << " seq " << row->m_SeqIdx
                             << " start " << row->m_StartIt->first
                             << " (left of start " << start_its_i->second->first << ") "
                             << "was pushed into stack\n";
#endif
#if _DEBUG
                        if (row == refseq) {
                            string errstr =
                                string("CAlnMix::x_CreateSegmentsVector():")
                                + " Internal error: Infinite loop detected.";
                            NCBI_THROW(CAlnException, eMergeFailure, errstr);
                        }                            
#endif
                        pop_seg = false;
                        break;
                    }
                }

                if (pop_seg) {

                    // inc/dec iterators for each row of the seg
                    ITERATE (CAlnMixSegment::TStartIterators, start_its_i,
                             seg_stack.top()->m_StartIts) {
                        CAlnMixSeq * row = start_its_i->first;

#if _DEBUG
                        if (row->m_PositiveStrand  &&
                            row->m_StartIt->first > 
                            start_its_i->second->first  ||
                            !row->m_PositiveStrand  &&
                            row->m_StartIt->first <
                            start_its_i->second->first) {
                            string errstr =
                                string("CAlnMix::x_CreateSegmentsVector():")
                                + " Internal error: Integrity broken" +
                                " row=" + NStr::IntToString(row->m_RowIdx) +
                                " seq=" + NStr::IntToString(row->m_SeqIdx)
                                + " row->m_StartIt->first="
                                + NStr::IntToString(row->m_StartIt->first)
                                + " start_its_i->second->first=" +
                                NStr::IntToString(start_its_i->second->first)
                                + " refseq->m_StartIt->first=" +
                                NStr::IntToString(refseq->m_StartIt->first)
                                + " strand=" +
                                (row->m_PositiveStrand ? "plus" : "minus");
                            NCBI_THROW(CAlnException, eMergeFailure, errstr);
                        }
#endif

                        if (row->m_PositiveStrand) {
                            row->m_StartIt++;
                        } else {
                            if (row->m_StartIt == row->m_Starts.begin()) {
                                row->m_StartIt = row->m_Starts.end();
                            } else {
                                row->m_StartIt--;
                            }
                        }
                    }

                    if (seg_stack.size() > 1) {
                        // add to the gapped segments
                        gapped_segs.push_back(seg_stack.top());
                        seg_stack.pop();
#if _ALNMGR_TRACE
                        cerr << "  seg popped].\n";
#endif
                    } else {
                        // add the gapped segments if any
                        if (gapped_segs.size()) {
                            if (m_MergeFlags & fGapJoin) {
                                // request to try to align
                                // gapped segments w/ equal len
                                x_ConsolidateGaps(gapped_segs);
                            } else if (m_MergeFlags & fMinGap) {
                                // request to try to align 
                                // all gapped segments
                                x_MinimizeGaps(gapped_segs);
                            }
                            if (orig_refseq) {
                                NON_CONST_ITERATE (TSegmentsContainer,
                                                   seg_i, gapped_segs) {
                                    m_Segments.push_back(&**seg_i);
                                }
                                gapped_segs.clear();
                            }
                        }
                        // add the refseq segment
                        if (orig_refseq) {
                            m_Segments.push_back(seg_stack.top());
                        } else {
                            gapped_segs.push_back(seg_stack.top());
                        }
                        seg_stack.pop();
#if _ALNMGR_TRACE
                        cerr << "  refseq seg popped].\n";
#endif
                    } // if (seg_stack.size() > 1)
                } // if (popseg)
            } // while ( !seg_stack.empty() )
        } // while (refseq->m_StartIt != refseq->m_Starts.end())
        orig_refseq = false;
    } // while (true)

    if (m_MergeFlags & fFillUnalignedRegions) {
        vector<TSignedSeqPos> starts;
        vector<TSeqPos> lens;
        starts.resize(m_Rows.size(), -1);
        lens.resize(m_Rows.size(), 0);

        TSeqPos len = 0;
        CAlnMap::TNumrow rowidx;

        TSegments::iterator seg_i = m_Segments.begin();
        while (seg_i != m_Segments.end()) {
            len = (*seg_i)->m_Len;
            ITERATE (CAlnMixSegment::TStartIterators, start_its_i,
                     (*seg_i)->m_StartIts) {
                CAlnMixSeq * row = start_its_i->first;
                rowidx = row->m_RowIdx;
                TSignedSeqPos& prev_start = starts[rowidx];
                TSeqPos& prev_len = lens[rowidx];
                TSeqPos start = start_its_i->second->first;
                const bool plus = row->m_PositiveStrand;
                const int& width = row->m_Width;
                TSeqPos prev_start_plus_len = prev_start + prev_len * width;
                TSeqPos start_plus_len = start + len * width;
                if (prev_start >= 0) {
                    if (plus  &&  prev_start_plus_len < start  ||
                        !plus  &&  start_plus_len < (TSeqPos) prev_start) {
                        // create a new seg
                        CRef<CAlnMixSegment> seg (new CAlnMixSegment);
                        TSeqPos new_start;
                        if (row->m_PositiveStrand) {
                            new_start = prev_start + prev_len * width;
                            seg->m_Len = (start - new_start) / width;
                        } else {
                            new_start = start_plus_len;
                            seg->m_Len = (prev_start - new_start) / width;
                        }                            
                        row->m_Starts[new_start] = seg;
                        CAlnMixSeq::TStarts::iterator start_i =
                            start_its_i->second;
                        seg->m_StartIts[row] = 
                            row->m_PositiveStrand ?
                            --start_i :
                            ++start_i;
                            
                        seg_i = m_Segments.insert(seg_i, seg);
                        seg_i++;
                    }
                }
                prev_start = start;
                prev_len = len;
            }
            seg_i++;
        }
    }
}


void CAlnMix::x_ConsolidateGaps(TSegmentsContainer& gapped_segs)
{
    TSegmentsContainer::iterator seg1_i, seg2_i;

    seg2_i = seg1_i = gapped_segs.begin();
    if (seg2_i != gapped_segs.end()) {
        seg2_i++;
    }

    bool         cache = false;
    string       s1;
    TSeqPos      start1;
    int          score1;
    CAlnMixSeq * seq1;
    CAlnMixSeq * seq2;

    while (seg2_i != gapped_segs.end()) {

        CAlnMixSegment * seg1 = *seg1_i;
        CAlnMixSegment * seg2 = *seg2_i;

        // check if this seg possibly aligns with the previous one
        bool possible = true;
            
        if (seg2->m_Len == seg1->m_Len  && 
            seg2->m_StartIts.size() == 1) {

            seq2 = seg2->m_StartIts.begin()->first;

            // check if this seq was already used
            ITERATE (CAlnMixSegment::TStartIterators,
                     st_it,
                     (*seg1_i)->m_StartIts) {
                if (st_it->first == seq2) {
                    possible = false;
                    break;
                }
            }

            // check if score is sufficient
            if (possible  &&  m_AddFlags & fCalcScore) {
                if (!cache) {

                    seq1 = seg1->m_StartIts.begin()->first;
                    
                    start1 = seg1->m_StartIts[seq1]->first;
                    TSeqPos start1_plus_len = 
                        start1 + seg1->m_Len * seq1->m_Width;
                        
                    if (seq1->m_PositiveStrand) {
                        seq1->m_BioseqHandle->GetSeqVector
                            (CBioseq_Handle::eCoding_Iupac,
                             CBioseq_Handle::eStrand_Plus).
                            GetSeqData(start1, start1_plus_len, s1);
                    } else {
                        CSeqVector seq_vec = 
                            seq1->m_BioseqHandle->GetSeqVector
                            (CBioseq_Handle::eCoding_Iupac,
                             CBioseq_Handle::eStrand_Minus);
                        TSeqPos size = seq_vec.size();
                        seq_vec.GetSeqData(size - start1_plus_len,
                                           size - start1, 
                                           s1);
                    }                                

                    score1 = 
                        CAlnVec::CalculateScore(s1, s1,
                                                seq1->m_IsAA,
                                                seq1->m_IsAA);
                    cache = true;
                }
                
                string s2;

                const TSeqPos& start2 = seg2->m_StartIts[seq2]->first;
                TSeqPos start2_plus_len = 
                    start2 + seg2->m_Len * seq2->m_Width;
                            
                if (seq2->m_PositiveStrand) {
                    seq2->m_BioseqHandle->GetSeqVector
                        (CBioseq_Handle::eCoding_Iupac,
                         CBioseq_Handle::eStrand_Plus).
                        GetSeqData(start2, start2_plus_len, s2);
                } else {
                    CSeqVector seq_vec = 
                        seq2->m_BioseqHandle->GetSeqVector
                        (CBioseq_Handle::eCoding_Iupac,
                         CBioseq_Handle::eStrand_Minus);
                    TSeqPos size = seq_vec.size();
                    seq_vec.GetSeqData(size - start2_plus_len,
                                       size - start2, 
                                       s2);
                }                                

                int score2 = 
                    CAlnVec::CalculateScore(s1, s2, seq1->m_IsAA, seq2->m_IsAA);

                if (score2 < 75 * score1 / 100) {
                    possible = false;
                }
            }
            
        } else {
            possible = false;
        }

        if (possible) {
            // consolidate the ones so far
            
            // add the new row
            seg1->m_StartIts[seq2] = seg2->m_StartIts.begin()->second;
            
            // point the row's start position to the beginning seg
            seg2->m_StartIts.begin()->second->second = seg1;
            
            seg2_i = gapped_segs.erase(seg2_i);
        } else {
            cache = false;
            seg1_i++;
            seg2_i++;
        }
    }
}


void CAlnMix::x_MinimizeGaps(TSegmentsContainer& gapped_segs)
{
    TSegmentsContainer::iterator  seg_i, seg_i_end, seg_i_begin;
    CAlnMixSegment       * seg1, * seg2;
    CRef<CAlnMixSegment> seg;
    CAlnMixSeq           * seq;
    TSegmentsContainer            new_segs;

    seg_i_begin = seg_i_end = seg_i = gapped_segs.begin();

    typedef map<TSeqPos, CRef<CAlnMixSegment> > TLenMap;
    TLenMap len_map;

    while (seg_i_end != gapped_segs.end()) {

        len_map[(*seg_i_end)->m_Len];
        
        // check if this seg can possibly be minimized
        bool possible = true;

        seg_i = seg_i_begin;
        while (seg_i != seg_i_end) {
            seg1 = *seg_i;
            seg2 = *seg_i_end;
            
            ITERATE (CAlnMixSegment::TStartIterators,
                     st_it,
                     seg2->m_StartIts) {
                seq = st_it->first;
                // check if this seq was already used
                if (seg1->m_StartIts.find(seq) != seg1->m_StartIts.end()) {
                    possible = false;
                    break;
                }
            }
            if ( !possible ) {
                break;
            }
            seg_i++;
        }
        seg_i_end++;

        if ( !possible  ||  seg_i_end == gapped_segs.end()) {
            // use the accumulated len info to create the new segs

            // create new segs with appropriate len
            TSeqPos len_so_far = 0;
            TLenMap::iterator len_i = len_map.begin();
            while (len_i != len_map.end()) {
                len_i->second = new CAlnMixSegment();
                len_i->second->m_Len = len_i->first - len_so_far;
                len_so_far += len_i->second->m_Len;
                len_i++;
            }
                
            // loop through the accumulated orig segs.
            // copy info from them into the new segs
            TLenMap::iterator len_i_end;
            seg_i = seg_i_begin;
            while (seg_i != seg_i_end) {
                TSeqPos orig_len = (*seg_i)->m_Len;

                // determine the span of the current seg
                len_i_end = len_map.find(orig_len);
                len_i_end++;

                // loop through its sequences
                NON_CONST_ITERATE (CAlnMixSegment::TStartIterators,
                                   st_it,
                                   (*seg_i)->m_StartIts) {

                    seq = st_it->first;
                    TSeqPos orig_start = st_it->second->first;

                    len_i = len_map.begin();
                    len_so_far = 0;
                    // loop through the new segs
                    while (len_i != len_i_end) {
                        seg = len_i->second;
                    
                        // calc the start
                        TSeqPos this_start = orig_start + 
                            (seq->m_PositiveStrand ? 
                             len_so_far :
                             orig_len - len_so_far - seg->m_Len) *
                            seq->m_Width;

                        // create the bindings:
                        seq->m_Starts[this_start] = seg;
                        seg->m_StartIts[seq] = seq->m_Starts.find(this_start);
                        len_i++;
                        len_so_far += seg->m_Len;
                    }
                }
                seg_i++;
            }
            NON_CONST_ITERATE (TLenMap, len_it, len_map) {
                new_segs.push_back(len_it->second);
            }
            len_map.clear();
            seg_i_begin = seg_i_end;
        }
    }
    gapped_segs.clear();
    ITERATE (TSegmentsContainer, new_seg_i, new_segs) {
        gapped_segs.push_back(*new_seg_i);
    }
}


void CAlnMix::x_CreateDenseg()
{
    int numrow  = 0,
        numrows = m_Rows.size();
    int numseg  = 0,
        numsegs = m_Segments.size();
    int num     = numrows * numsegs;

    m_DS = new CDense_seg();
    m_DS->SetDim(numrows);
    m_DS->SetNumseg(numsegs);

    m_Aln = new CSeq_align();
    m_Aln->SetType(CSeq_align::eType_not_set);
    m_Aln->SetSegs().SetDenseg(*m_DS);
    m_Aln->SetDim(numrows);

    CDense_seg::TIds&     ids     = m_DS->SetIds();
    CDense_seg::TStarts&  starts  = m_DS->SetStarts();
    CDense_seg::TStrands& strands = m_DS->SetStrands();
    CDense_seg::TLens&    lens    = m_DS->SetLens();

    ids.resize(numrows);
    lens.resize(numsegs);
    starts.resize(num, -1);
    strands.resize(num, eNa_strand_minus);

    // ids
    numrow = 0;
    ITERATE(TSeqs, row_i, m_Rows) {
        ids[numrow++] = (*row_i)->m_SeqId;
    }

    int offset = 0;
    numseg = 0;
    ITERATE(TSegments, seg_i, m_Segments) {
        // lens
        lens[numseg] = (*seg_i)->m_Len;

        // starts
        ITERATE (CAlnMixSegment::TStartIterators, start_its_i,
                (*seg_i)->m_StartIts) {
            starts[offset + start_its_i->first->m_RowIdx] =
                start_its_i->second->first;
        }

        // strands
        numrow = 0;
        ITERATE(TSeqs, row_i, m_Rows) {
            if ((*row_i)->m_PositiveStrand) {
                strands[offset + numrow] = eNa_strand_plus;
            }
            numrow++;
        }

        // next segment
        numseg++; offset += numrows;
    }

    // widths
    if (m_ContainsNA  &&  m_ContainsAA  ||  m_AddFlags & fForceTranslation) {
        CDense_seg::TWidths&  widths  = m_DS->SetWidths();
        widths.resize(numrows);
        numrow = 0;
        ITERATE (TSeqs, row_i, m_Rows) {
            widths[numrow++] = (*row_i)->m_Width;
        }
    }
#if _DEBUG
    m_DS->Validate(true);
#endif    
}


CRef<CDense_seg> CAlnMix::x_ExtendDSWithWidths(const CDense_seg& ds)
{
    if (ds.IsSetWidths()) {
        NCBI_THROW(CAlnException, eMergeFailure,
                   "CAlnMix::x_ExtendDSWithWidths(): "
                   "Widths already exist for the input alignment");
    }

    bool contains_AA = false, contains_NA = false;
    CRef<CAlnMixSeq> aln_seq;
    for (size_t numrow = 0;  numrow < ds.GetDim();  numrow++) {
        x_IdentifyAlnMixSeq(aln_seq, *ds.GetIds()[numrow]);
        if (aln_seq->m_IsAA) {
            contains_AA = true;
        } else {
            contains_NA = true;
        }
    }
    if (contains_AA  &&  contains_NA) {
        NCBI_THROW(CAlnException, eMergeFailure,
                   "CAlnMix::x_ExtendDSWithWidths(): "
                   "Incorrect input Dense-seg: Contains both AAs and NAs but "
                   "widths do not exist!");
    }        

    CRef<CDense_seg> new_ds(new CDense_seg());

    // copy from the original
    new_ds->Assign(ds);

    if (contains_NA) {
        // fix the lengths
        const CDense_seg::TLens& lens     = ds.GetLens();
        CDense_seg::TLens&       new_lens = new_ds->SetLens();
        for (size_t numseg = 0; numseg < ds.GetNumseg(); numseg++) {
            if (lens[numseg] % 3) {
                string errstr =
                    string("CAlnMix::x_ExtendDSWithWidths(): ") +
                    "Length of segment " + NStr::IntToString(numseg) +
                    " is not divisible by 3.";
                NCBI_THROW(CAlnException, eMergeFailure, errstr);
            } else {
                new_lens[numseg] = lens[numseg] / 3;
            }
        }
    }

    // add the widths
    CDense_seg::TWidths&  new_widths  = new_ds->SetWidths();
    new_widths.resize(ds.GetDim(), contains_NA ? 3 : 1);
#if _DEBUG
    new_ds->Validate(true);
#endif
    return new_ds;
}


void CAlnMix::x_IdentifyAlnMixSeq(CRef<CAlnMixSeq>& aln_seq, const CSeq_id& seq_id)
{
    if ( !m_Scope ) {
        string errstr = string("CAlnMix::x_IdentifyAlnMixSeq(): ") 
            + "In order to use this functionality "
            "scope should be provided in CAlnMix constructor.";
        NCBI_THROW(CAlnException, eInvalidRequest, errstr);
    }
        
    CBioseq_Handle bioseq_handle = 
        GetScope().GetBioseqHandle(seq_id);

    if ( !bioseq_handle ) {
        string errstr = string("CAlnMix::x_IdentifyAlnMixSeq(): ") 
            + "Seq-id cannot be resolved: "
            + (seq_id.AsFastaString());
        
        NCBI_THROW(CAlnException, eInvalidSeqId, errstr);
    }

    TBioseqHandleMap::iterator it = m_BioseqHandles.find(bioseq_handle);
    if (it == m_BioseqHandles.end()) {
        // add this bioseq handle
        aln_seq = new CAlnMixSeq();
        m_BioseqHandles[bioseq_handle] = aln_seq;
        aln_seq->m_BioseqHandle = 
            &m_BioseqHandles.find(bioseq_handle)->first;
        
        CRef<CSeq_id> seq_id(new CSeq_id);
        seq_id->Assign(*aln_seq->m_BioseqHandle->GetSeqId());
        aln_seq->m_SeqId = seq_id;
        aln_seq->m_DsCnt = 0;

        // add this sequence
        m_Seqs.push_back(aln_seq);
            
        // AA or NA?
        if (aln_seq->m_BioseqHandle->GetBioseqCore()
            ->GetInst().GetMol() == CSeq_inst::eMol_aa) {
            aln_seq->m_IsAA = true;
            m_ContainsAA = true;
        } else {
            m_ContainsNA = true;
        }
    } else {
        aln_seq = it->second;
    }
}


void CAlnMix::ChooseSeqId(CSeq_id& id1, const CSeq_id& id2)
{
    CRef<CAlnMixSeq> aln_seq1, aln_seq2;
    x_IdentifyAlnMixSeq(aln_seq1, id1);
    x_IdentifyAlnMixSeq(aln_seq2, id2);
    if (aln_seq1->m_BioseqHandle != aln_seq2->m_BioseqHandle) {
        string errstr = 
            string("CAlnMix::ChooseSeqId(CSeq_id& id1, const CSeq_id& id2):")
            + " Seq-ids: " + id1.AsFastaString() 
            + " and " + id2.AsFastaString() 
            + " do not resolve to the same bioseq handle,"
            " but are used on the same 'row' in different segments."
            " This is legally allowed in a Std-seg, but conversion to"
            " Dense-seg cannot be performed.";
        NCBI_THROW(CAlnException, eInvalidSeqId, errstr);
    }
    CRef<CSeq_id> id1cref(&id1);
    CRef<CSeq_id> id2cref(&(const_cast<CSeq_id&>(id2)));
    if (CSeq_id::BestRank(id1cref) > CSeq_id::BestRank(id2cref)) {
        id1.Reset();
        SerialAssign<CSeq_id>(id1, id2);
    }
}    


void CAlnMix::x_SegmentStartItsConsistencyCheck(const CAlnMixSegment& seg,
                                                const CAlnMixSeq&     seq,
                                                const TSeqPos&        start)
{
    ITERATE(CAlnMixSegment::TStartIterators,
            st_it_i, seg.m_StartIts) {
        // both should point to the same seg
        if ((*st_it_i).second->second != &seg) {
            string errstr =
                string("CAlnMix::x_SegmentStartItsConsistencyCheck")
                + " [match_idx=" + NStr::IntToString(m_MatchIdx) + "]"
                + " The internal consistency check failed for"
                + " the segment containing ["
                + " row=" + NStr::IntToString((*st_it_i).first->m_RowIdx)
                + " seq=" + NStr::IntToString((*st_it_i).first->m_SeqIdx)
                + " strand=" +
                ((*st_it_i).first->m_PositiveStrand ? "plus" : "minus")
                + " start=" + NStr::IntToString((*st_it_i).second->first)
                + "] aligned to: ["
                + " row=" + NStr::IntToString(seq.m_RowIdx)
                + " seq=" + NStr::IntToString(seq.m_SeqIdx)
                + " strand=" +
                (seq.m_PositiveStrand ? "plus" : "minus")
                + " start=" + NStr::IntToString(start)
                + "].";
            NCBI_THROW(CAlnException, eMergeFailure, errstr);
        }
    }
}


END_objects_SCOPE // namespace ncbi::objects::
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.116  2005/02/08 15:58:37  todorov
* Added handling for best reciprocal hits.
*
* Revision 1.115  2004/11/02 18:31:01  todorov
* Changed (mostly shortened) a few internal members' names
* for convenience and consistency
*
* Revision 1.114  2004/11/02 18:04:36  todorov
* CAlnMixSeq += m_ExtraRowIdx
* x_CreateSegmentsVector() += conditionally compiled (_ALNMGR_TRACE) sections
*
* Revision 1.113  2004/10/26 17:46:32  todorov
* Prevented iterator invalidation in case refseq needs to be moved to first row
*
* Revision 1.112  2004/10/18 15:10:45  todorov
* Use of OM now only depends on whether scope was provided at construction time
*
* Revision 1.111  2004/10/13 17:51:33  todorov
* rm conditional compilation logic
*
* Revision 1.110  2004/10/13 16:29:17  todorov
* 1) Added x_SegmentStartItsConsistencyCheck.
* 2) Within x_SecondRowFits, iterate through seq2->m_ExtraRow to find
*    the proper strand, if such exists.
*
* Revision 1.109  2004/10/12 19:55:14  rsmith
* make x_CompareAlnSeqScores arguments match the container it compares on.
*
* Revision 1.108  2004/09/27 16:18:16  todorov
* + truncate segments of sequences on multiple frames
*
* Revision 1.107  2004/09/23 18:55:31  todorov
* 1) avoid an introduced m_DsCnt mismatch; 2) + reeval x_SecondRowFits for some cases
*
* Revision 1.106  2004/09/22 17:00:53  todorov
* +CAlnMix::fPreserveRows
*
* Revision 1.105  2004/09/22 14:26:35  todorov
* changed TSegments & TSegmentsContainer from vectors to lists
*
* Revision 1.104  2004/09/09 23:10:41  todorov
* Fixed the multi-framed ref sequence case
*
* Revision 1.103  2004/09/01 17:36:07  todorov
* extended the unable-to-load-data exception msg even further
*
* Revision 1.102  2004/09/01 15:38:05  todorov
* extended exception msg in case unable to load data for segment
*
* Revision 1.101  2004/06/28 20:09:27  todorov
* Initialize m_DsIdx. Also bug fix when shifting to extra row
*
* Revision 1.100  2004/06/23 22:22:23  todorov
* Fixed condition logic
*
* Revision 1.99  2004/06/23 18:31:10  todorov
* Calculate the prevailing strand per row (using scores)
*
* Revision 1.98  2004/06/22 01:24:04  ucko
* Restore subtraction accidentally dropped in last commit; should
* resolve infinite loops.
*
* Revision 1.97  2004/06/21 20:35:48  todorov
* Fixed a signed/unsigned bug when truncating the len with delta
*
* Revision 1.96  2004/06/14 21:01:59  todorov
* 1) added iterators bounds checks; 2) delta check
*
* Revision 1.95  2004/05/25 19:16:33  todorov
* initialize second_row_fits
*
* Revision 1.94  2004/05/25 16:00:10  todorov
* remade truncation of overlaps
*
* Revision 1.93  2004/05/21 21:42:51  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.92  2004/04/13 18:02:10  todorov
* Added some limit checks when inc/dec iterators
*
* Revision 1.91  2004/03/30 23:27:32  todorov
* Switch from CAlnMix::x_RankSeqId() to CSeq_id::BestRank()
*
* Revision 1.90  2004/03/30 20:41:29  todorov
* Rearranged the rank of seq-ids according to the C toolkit rearrangement
*
* Revision 1.89  2004/03/29 17:05:06  todorov
* extended exception msg
*
* Revision 1.88  2004/03/09 17:40:07  kuznets
* Compilation bug fix CAlnMix::ChooseSeqId
*
* Revision 1.87  2004/03/09 17:16:16  todorov
* + SSeqIdChooser implementation
*
* Revision 1.86  2004/01/16 23:59:45  todorov
* + missing width in seg calcs
*
* Revision 1.85  2003/12/22 19:30:35  kuznets
* Fixed compilation error (operator ambiguity) (MSVC)
*
* Revision 1.84  2003/12/22 18:30:37  todorov
* ObjMgr is no longer created internally. Scope should be passed as a reference in the ctor
*
* Revision 1.83  2003/12/18 19:46:27  todorov
* iterate -> ITERATE
*
* Revision 1.82  2003/12/12 22:42:53  todorov
* Init frames and use refseq instead of seq1 which may be refseqs child
*
* Revision 1.81  2003/12/10 16:14:38  todorov
* + exception when merge with no input
*
* Revision 1.80  2003/12/08 21:28:03  todorov
* Forced Translation of Nucleotide Sequences
*
* Revision 1.79  2003/11/24 17:11:32  todorov
* SetWidths only if necessary
*
* Revision 1.78  2003/11/03 14:43:44  todorov
* Use CDense_seg::Validate()
*
* Revision 1.77  2003/10/03 19:22:55  todorov
* Extended exception msg in case of empty row
*
* Revision 1.76  2003/09/16 14:45:56  todorov
* more informative exception strng
*
* Revision 1.75  2003/09/12 16:18:36  todorov
* -unneeded checks (">=" for unsigned)
*
* Revision 1.74  2003/09/12 15:42:36  todorov
* +CRef to delay obj deletion while iterating on it
*
* Revision 1.73  2003/09/08 20:44:21  todorov
* expression bug fix
*
* Revision 1.72  2003/09/08 19:48:55  todorov
* signed vs unsigned warnings fixed
*
* Revision 1.71  2003/09/08 19:24:04  todorov
* fix strand
*
* Revision 1.70  2003/09/08 19:18:32  todorov
* plus := all strands != minus
*
* Revision 1.69  2003/09/06 02:39:34  todorov
* OBJECTS_ALNMGR___ALNMIX__DBG -> _DEBUG
*
* Revision 1.68  2003/08/28 19:54:58  todorov
* trailing gap on master fix
*
* Revision 1.67  2003/08/20 19:35:32  todorov
* Added x_ValidateDenseg
*
* Revision 1.66  2003/08/20 14:34:58  todorov
* Support for NA2AA Densegs
*
* Revision 1.65  2003/08/01 20:49:26  todorov
* Changed the control of the main Merge() loop
*
* Revision 1.64  2003/07/31 18:50:29  todorov
* Fixed a couple of truncation problems; Added a match deletion
*
* Revision 1.63  2003/07/15 20:54:01  todorov
* exception type fixed
*
* Revision 1.62  2003/07/09 22:58:12  todorov
* row index bug fix in case of fillunaln
*
* Revision 1.61  2003/07/01 17:40:20  todorov
* fFillUnaligned bug fix
*
* Revision 1.60  2003/06/26 21:35:48  todorov
* + fFillUnalignedRegions
*
* Revision 1.59  2003/06/25 15:17:31  todorov
* truncation consistent for the whole segment now
*
* Revision 1.58  2003/06/24 15:24:14  todorov
* added optional truncation of overlaps
*
* Revision 1.57  2003/06/20 03:06:39  todorov
* Setting the seqalign type
*
* Revision 1.56  2003/06/19 18:37:19  todorov
* typo fixed
*
* Revision 1.55  2003/06/09 20:54:20  todorov
* Use of ObjMgr is now optional
*
* Revision 1.54  2003/06/03 20:56:52  todorov
* Bioseq handle validation
*
* Revision 1.53  2003/06/03 16:07:05  todorov
* Fixed overlap consistency check in case strands differ
*
* Revision 1.52  2003/06/03 14:38:26  todorov
* warning fixed
*
* Revision 1.51  2003/06/02 17:39:40  todorov
* Changes in rev 1.49 were lost. Reapplying them
*
* Revision 1.50  2003/06/02 16:06:40  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.49  2003/05/30 17:42:03  todorov
* 1) Bug fix in x_SecondRowFits
* 2) x_CreateSegmentsVector now uses a stack to order the segs
*
* Revision 1.48  2003/05/23 18:11:42  todorov
* More detailed exception txt
*
* Revision 1.47  2003/05/20 21:20:59  todorov
* mingap minus strand multiple segs bug fix
*
* Revision 1.46  2003/05/15 23:31:26  todorov
* Minimize gaps bug fix
*
* Revision 1.45  2003/05/09 16:41:27  todorov
* Optional mixing of the query sequence only
*
* Revision 1.44  2003/04/24 16:15:57  vasilche
* Added missing includes and forward class declarations.
*
* Revision 1.43  2003/04/15 14:21:12  vasilche
* Fixed order of member initializers.
*
* Revision 1.42  2003/04/14 18:03:19  todorov
* reuse of matches bug fix
*
* Revision 1.41  2003/04/01 20:25:31  todorov
* fixed iterator-- behaviour at the begin()
*
* Revision 1.40  2003/03/28 16:47:28  todorov
* Introduced TAddFlags (fCalcScore for now)
*
* Revision 1.39  2003/03/26 16:38:24  todorov
* mix independent densegs
*
* Revision 1.38  2003/03/18 17:16:05  todorov
* multirow inserts bug fix
*
* Revision 1.37  2003/03/12 15:39:47  kuznets
* iterate -> ITERATE
*
* Revision 1.36  2003/03/10 22:12:02  todorov
* fixed x_CompareAlnMatchScores callback
*
* Revision 1.35  2003/03/06 21:42:38  todorov
* bug fix in x_Reset()
*
* Revision 1.34  2003/03/05 21:53:24  todorov
* clean up miltiple mix case
*
* Revision 1.33  2003/03/05 17:42:41  todorov
* Allowing multiple mixes + general case speed optimization
*
* Revision 1.32  2003/03/05 16:18:17  todorov
* + str len err check
*
* Revision 1.31  2003/02/24 19:01:31  vasilche
* Use faster version of CSeq_id::Assign().
*
* Revision 1.30  2003/02/24 18:46:38  todorov
* Fixed a - strand row info creation bug
*
* Revision 1.29  2003/02/11 21:32:44  todorov
* fMinGap optional merging algorithm
*
* Revision 1.28  2003/02/04 00:05:16  todorov
* GetSeqData neg strand range bug
*
* Revision 1.27  2003/01/27 22:30:30  todorov
* Attune to seq_vector interface change
*
* Revision 1.26  2003/01/23 16:30:18  todorov
* Moved calc score to alnvec
*
* Revision 1.25  2003/01/22 21:00:21  dicuccio
* Fixed compile error - transposed #endif and }
*
* Revision 1.24  2003/01/22 19:39:13  todorov
* 1) Matches w/ the 1st non-gapped row added only; the rest used to calc seqs'
* score only
* 2) Fixed a couple of potential problems
*
* Revision 1.23  2003/01/17 18:56:26  todorov
* Perform merge algorithm even if only one input denseg
*
* Revision 1.22  2003/01/16 17:40:19  todorov
* Fixed strand param when calling GetSeqVector
*
* Revision 1.21  2003/01/10 17:37:28  todorov
* Fixed a bug in fNegativeStrand
*
* Revision 1.20  2003/01/10 15:12:13  dicuccio
* Ooops.  Methinks I should read my e-mail more thoroughly - thats '!= NULL', not
* '== NULL'.
*
* Revision 1.19  2003/01/10 15:11:12  dicuccio
* Small bug fixes: changed 'iterate' -> 'non_const_iterate' in x_Merge(); changed
* logic of while() in x_CreateRowsVector() to avoid compiler warning.
*
* Revision 1.18  2003/01/10 00:42:53  todorov
* Optional sorting of seqs by score
*
* Revision 1.17  2003/01/10 00:11:37  todorov
* Implemented fNegativeStrand
*
* Revision 1.16  2003/01/07 17:23:49  todorov
* Added conditionally compiled validation code
*
* Revision 1.15  2003/01/02 20:03:48  todorov
* Row strand init & check
*
* Revision 1.14  2003/01/02 16:40:11  todorov
* Added accessors to the input data
*
* Revision 1.13  2003/01/02 15:30:17  todorov
* Fixed the order of checks in x_SecondRowFits
*
* Revision 1.12  2002/12/30 20:55:47  todorov
* Added range fitting validation to x_SecondRowFits
*
* Revision 1.11  2002/12/30 18:08:39  todorov
* 1) Initialized extra rows' m_StartIt
* 2) Fixed a bug in x_SecondRowFits
*
* Revision 1.10  2002/12/27 23:09:56  todorov
* Additional inconsistency checks in x_SecondRowFits
*
* Revision 1.9  2002/12/27 17:27:13  todorov
* Force positive strand in all cases but negative
*
* Revision 1.8  2002/12/27 16:39:13  todorov
* Fixed a bug in the single Dense-seg case.
*
* Revision 1.7  2002/12/23 18:03:51  todorov
* Support for putting in and getting out Seq-aligns
*
* Revision 1.6  2002/12/19 00:09:23  todorov
* Added optional consolidation of segments that are gapped on the query.
*
* Revision 1.5  2002/12/18 18:58:17  ucko
* Tweak syntax to avoid confusing MSVC.
*
* Revision 1.4  2002/12/18 03:46:00  todorov
* created an algorithm for mixing alignments that share a single sequence.
*
* Revision 1.3  2002/10/25 20:02:41  todorov
* new fTryOtherMethodOnFail flag
*
* Revision 1.2  2002/10/24 21:29:13  todorov
* adding Dense-segs instead of Seq-aligns
*
* Revision 1.1  2002/08/23 14:43:52  ucko
* Add the new C++ alignment manager to the public tree (thanks, Kamen!)
*
*
* ===========================================================================
*/
