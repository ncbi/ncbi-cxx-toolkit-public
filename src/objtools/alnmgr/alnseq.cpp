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
*   Alignment sequences
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objtools/alnmgr/alnmap.hpp>
#include <objtools/alnmgr/alnseq.hpp>
#include <objtools/alnmgr/alnsegments.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>

#include <algorithm>


BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::


CAlnMixSequences::CAlnMixSequences(void)
    : m_DsCnt(0),
      m_ContainsAA(false),
      m_ContainsNA(false)
{
}


CAlnMixSequences::CAlnMixSequences(CScope& scope)
    : m_DsCnt(0),
      m_Scope(&scope),
      m_ContainsAA(false),
      m_ContainsNA(false)
{
}


inline
bool
CAlnMixSequences::x_CompareScores(const CRef<CAlnMixSeq>& seq1,
                                  const CRef<CAlnMixSeq>& seq2) 
{
    return seq1->m_Score >seq2->m_Score;
}


inline
bool
CAlnMixSequences::x_CompareChainScores(const CRef<CAlnMixSeq>& seq1,
                                       const CRef<CAlnMixSeq>& seq2) 
{
    return 
        seq1->m_ChainScore == seq2->m_ChainScore  &&
        seq1->m_Score > seq2->m_Score  ||
        seq1->m_ChainScore > seq2->m_ChainScore;
}


void
CAlnMixSequences::SortByScore()
{
    stable_sort(m_Seqs.begin(), m_Seqs.end(), x_CompareScores);
}


void
CAlnMixSequences::SortByChainScore()
{
    stable_sort(m_Seqs.begin(), m_Seqs.end(), x_CompareChainScores);
}


void
CAlnMixSequences::Add(const CDense_seg& ds, TAddFlags flags)
{
    m_DsCnt++;

    vector<CRef<CAlnMixSeq> >& ds_seq = m_DsSeq[&ds];

    for (CAlnMap::TNumrow row = 0;  row < ds.GetDim();  row++) {

        CRef<CAlnMixSeq> aln_seq;

        if ( !m_Scope ) {
            // identify sequences by their seq ids as provided by
            // the dense seg (not as reliable as with OM, but faster)
            CRef<CSeq_id> seq_id(new CSeq_id);
            seq_id->Assign(*ds.GetIds()[row]);

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
                if (ds.IsSetWidths()) {
                    if (ds.GetWidths()[row] == 1) {
                        aln_seq->m_IsAA = true;
                        m_ContainsAA = true;
                    } else {
                        aln_seq->m_IsAA = false;
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
            if (ds.IsSetWidths()) {
                const int& width = ds.GetWidths()[row];
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
        // Set the width
        if (ds.IsSetWidths()) {
            aln_seq->m_Width = ds.GetWidths()[row];
        }


        // Preserve the row of the the original sequences if requested.
        // This is mostly used to allow a sequence to itself.
        // Create an additional sequence, pointed by m_AntoherRow,
        // if the row index differs.
        if (flags & fPreserveRows) {
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
                    another_row->m_ChildIdx     = aln_seq->m_ChildIdx + 1;
                    another_row->m_DsIdx        = m_DsCnt;
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
}


void
CAlnMixSequences::x_IdentifyAlnMixSeq(CRef<CAlnMixSeq>& aln_seq, const CSeq_id& seq_id)
{
    if ( !m_Scope ) {
        string errstr = string("CAlnMix::x_IdentifyAlnMixSeq(): ") 
            + "In order to use this functionality "
            "scope should be provided in CAlnMix constructor.";
        NCBI_THROW(CAlnException, eInvalidRequest, errstr);
    }
        
    CBioseq_Handle bioseq_handle = 
        m_Scope->GetBioseqHandle(seq_id);

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
        if (aln_seq->m_BioseqHandle->IsProtein()) {
            aln_seq->m_IsAA = true;
            m_ContainsAA = true;
        } else {
            aln_seq->m_IsAA = false;
            m_ContainsNA = true;
        }
    } else {
        aln_seq = it->second;
    }
}


void
CAlnMixSequences::BuildRows()
{
    m_Rows.clear();

    int count = 0;
    NON_CONST_ITERATE (TSeqs, i, m_Seqs) {
        CRef<CAlnMixSeq>& seq = *i;

        if ( !seq->GetStarts().empty() ) {
            m_Rows.push_back(seq);
            seq->m_RowIdx = count++;
            while ((seq = seq->m_ExtraRow) != NULL ) {
                seq->m_RowIdx = count++;
                m_Rows.push_back(seq);
            }
        }
    }
}


void
CAlnMixSequences::InitRowsStartIts()
{
    NON_CONST_ITERATE (TSeqs, row_i, m_Rows) {
        CAlnMixSeq * row = *row_i;
        if ( !row->GetStarts().empty() ) {
            if (row->m_PositiveStrand) {
                row->SetStarts().current = row->GetStarts().begin();
            } else {
                row->SetStarts().current = row->GetStarts().end();
                row->SetStarts().current--;
            }
        } else {
            row->SetStarts().current = row->GetStarts().end();
#if _DEBUG
            string errstr =
                string("CAlnMixSequences::InitRowsStartIts():") +
                " Internal error: no starts for row " +
                NStr::IntToString(row->m_RowIdx) +
                " (seq " +
                NStr::IntToString(row->m_SeqIdx) +
                " child " +
                NStr::IntToString(row->m_ChildIdx) +
                ").";
            NCBI_THROW(CAlnException, eMergeFailure, errstr);
#endif
        }
    }
}


void
CAlnMixSequences::InitExtraRowsStartIts()
{
    NON_CONST_ITERATE (list<CRef<CAlnMixSeq> >, row_i, m_ExtraRows) {
        CAlnMixSeq * row = *row_i;
        if ( !row->GetStarts().empty() ) {
            if (row->m_PositiveStrand) {
                row->SetStarts().current = row->GetStarts().begin();
            } else {
                row->SetStarts().current = row->GetStarts().end();
                row->SetStarts().current--;
            }
        } else {
            row->SetStarts().current = row->GetStarts().end();
#if _DEBUG
            string errstr =
                string("CAlnMixSequences::InitExtraRowStartIts():") +
                " Internal error: no starts for row " +
                NStr::IntToString(row->m_RowIdx) + ".";
            NCBI_THROW(CAlnException, eMergeFailure, errstr);
#endif
        }
    }
}


void
CAlnMixSequences::RowsStartItsContsistencyCheck(size_t match_idx)
{
    NON_CONST_ITERATE (TSeqs, row_i, m_Rows) {
        ITERATE (CAlnMixStarts, st_i, (*row_i)->GetStarts()) {
            st_i->second->
                StartItsConsistencyCheck(**row_i,
                                         st_i->first,
                                         match_idx);
        }
    }       
}


CAlnMixSeq::CAlnMixSeq(void)
    : m_DsCnt(0),
      m_BioseqHandle(0),
      m_Score(0),
      m_ChainScore(0),
      m_StrandScore(0),
      m_IsAA(false),
      m_Width(1),
      m_Frame(-1),
      m_PositiveStrand(true),
      m_RefBy(0),
      m_ExtraRow(0),
      m_ExtraRowIdx(0),
      m_AnotherRow(0),
      m_DsIdx(0),
      m_SeqIdx(-1),
      m_ChildIdx(0),
      m_RowIdx(-1),
      m_Starts(new CAlnMixStarts())
{
}


CAlnMixSeq::~CAlnMixSeq()
{
}

END_objects_SCOPE // namespace ncbi::objects::
END_NCBI_SCOPE
