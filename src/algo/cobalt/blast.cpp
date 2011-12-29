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

File name: blast.cpp

Author: Jason Papadopoulos

Contents: Find local alignments between sequences

******************************************************************************/

#include <ncbi_pch.hpp>
#include <util/range_coll.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqalign/Score.hpp>
#include <algo/blast/api/blast_prot_options.hpp>
#include <algo/blast/api/bl2seq.hpp>
#include <algo/cobalt/cobalt.hpp>

/// @file blast.cpp
/// Find local alignments between sequences

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)

USING_SCOPE(blast);
USING_SCOPE(objects);

/// Create a new query sequence that is a subset of a previous
/// query sequence
/// @param loc_list List of previously generated sequence fragments [in/out]
/// @param query Sequence that contains the current fragment [in]
/// @param from Start offset of fragment [in]
/// @param to End offset of fragment [in]
/// @param seg_list List of simplified representations of 
///                 previous fragments [in/out]
/// @param query_index Ordinal ID of 'query'
///
void 
CMultiAligner::x_AddNewSegment(vector< CRef<objects::CSeq_loc> >& loc_list,
                               const CRef<objects::CSeq_loc>& query,
                               TOffset from, TOffset to,
                               vector<SSegmentLoc>& seg_list,
                               int query_index)
{
    // Note that all offsets are zero-based

    CRef<CSeq_loc> seqloc(new CSeq_loc());
    seqloc->SetInt().SetFrom(from);
    seqloc->SetInt().SetTo(to);
    seqloc->SetInt().SetStrand(eNa_strand_unknown);
    seqloc->SetInt().SetId().Assign(sequence::GetId(*query, m_Scope));
    loc_list.push_back(seqloc);

    seg_list.push_back(SSegmentLoc(query_index, from, to));
}

/// Turn all fragments of selected query sequence not already covered by
/// a domain hit into a separate query sequence, used as input
/// to a blast search
/// @param blastp_indices Indices of query sequences selected for blastp
/// search [in]
/// @param filler_locs List of generated sequences [out]
/// @param filler_segs Simplified representation of filler_locs [out]
///
void
CMultiAligner::x_MakeFillerBlocks(const vector<int>& blastp_indices,
                               vector< CRef<objects::CSeq_loc> >& filler_locs,
                               vector<SSegmentLoc>& filler_segs)
{
    int num_queries = m_QueryData.size();
    vector<CRangeCollection<TOffset> > sorted_segs(num_queries);

    // Merge the offset ranges of all the current domain hits

    for (int i = 0; i < m_CombinedHits.Size(); i++) {
        CHit *hit = m_CombinedHits.GetHit(i);
        _ASSERT(hit->HasSubHits());

        ITERATE(CHit::TSubHit, subitr, hit->GetSubHit()) {
            CHit *subhit = *subitr;
            sorted_segs[hit->m_SeqIndex1].CombineWith(
                          static_cast<CRange<TOffset> >(subhit->m_SeqRange1));
            sorted_segs[hit->m_SeqIndex2].CombineWith(
                          static_cast<CRange<TOffset> >(subhit->m_SeqRange2));
        }
    }

    // For each query sequence, mark off the regions
    // not covered by a domain hit

    ITERATE(vector<int>, it, blastp_indices) {
        int i = *it;

        CRangeCollection<TOffset>& collection(sorted_segs[i]);
        TOffset seg_start = 0;

        // Note that fragments of size less than kMinHitSize
        // are ignored

        ITERATE(CRangeCollection<TOffset>, itr, collection) {
            if (itr->GetFrom() - seg_start > CHit::kMinHitSize) {
                x_AddNewSegment(filler_locs, m_tQueries[i], seg_start, 
                                itr->GetFrom() - 1, filler_segs, i);
            }
            seg_start = itr->GetToOpen();
        }

        // Handle the last fragment; this could actually
        // envelop the entire sequence

        int seq_length = sequence::GetLength(*m_tQueries[i], m_Scope);

        if (seq_length - seg_start > CHit::kMinHitSize) {
            x_AddNewSegment(filler_locs, m_tQueries[i], seg_start,
                            seq_length - 1, filler_segs, i);
        }
    }

    if (m_Options->GetVerbose()) {
        printf("Filler Segments:\n");
        for (int i = 0; i < (int)filler_segs.size(); i++) {
            printf("query %d %4d - %4d\n", 
                   filler_segs[i].seq_index, 
                   filler_segs[i].GetFrom(),
                   filler_segs[i].GetTo());
        }
        printf("\n\n");
    }
}

/// Run blastp, aligning the collection of filler fragments
/// against the entire input dataset
/// @param queries List of queries selected for blastp alignment [in]
/// @param indices List of indices of each selected query in the queries
/// array [in]
/// @param filler_locs List of generated sequences [in]
/// @param filler_segs Simplified representation of filler_locs [in]
///
void
CMultiAligner::x_AlignFillerBlocks(const TSeqLocVector& queries,
                                   const vector<int>& indices,
                                   vector< CRef<CSeq_loc> >& filler_locs, 
                                   vector<SSegmentLoc>& filler_segs)
{
    const int kBlastBatchSize = 10000;
    int num_full_queries = indices.size();

    if (filler_locs.empty())
        return;

    // Set the blast options

    double blastp_evalue = m_Options->GetBlastpEvalue();
    CRef<CBlastProteinOptionsHandle> blastp_opts(new CBlastProteinOptionsHandle);
    // deliberately set the cutoff e-value too high
    blastp_opts->SetEvalueThreshold(max(blastp_evalue, 10.0));
    //blastp_opts.SetGappedMode(false);
    blastp_opts->SetSegFiltering(false);

    // use blast on one batch of filler segments at a time

    int batch_start = 0; 
    while (batch_start < (int)filler_locs.size()) {

        TSeqLocVector curr_batch;
        int batch_size = 0;

        for (int i = batch_start; i < (int)filler_locs.size(); i++) {
            const CSeq_loc& curr_loc = *filler_locs[i];
            int fragment_size = curr_loc.GetInt().GetTo() -
                                curr_loc.GetInt().GetFrom() + 1;
            if (batch_size + fragment_size >= kBlastBatchSize && batch_size > 0)
                break;

            curr_batch.push_back(SSeqLoc(*filler_locs[i], *m_Scope));
            batch_size += fragment_size;
        }

        CBl2Seq blaster(curr_batch, queries, *blastp_opts);
        TSeqAlignVector v = blaster.Run();

        // check for interrupt
        if (m_Interrupt && (*m_Interrupt)(&m_ProgressMonitor)) {
            NCBI_THROW(CMultiAlignerException, eInterrupt,
                       "Alignment interrupted");
        }

        // Convert each resulting HSP into a CHit object

        // iterate over query sequence fragments for the current batch

        for (int i = 0; i < (int)curr_batch.size(); i++) {

            int list1_oid = filler_segs[batch_start + i].seq_index;

            for (int j = 0; j < num_full_queries; j++) {

                // skip hits that map to the same query sequence

                if (list1_oid == indices[j])
                    continue;

                // iterate over hitlists

                ITERATE(CSeq_align_set::Tdata, itr, 
                                   v[i * num_full_queries + j]->Get()) {

                    // iterate over hits

                    const CSeq_align& s = **itr;

                    if (s.GetSegs().Which() == CSeq_align::C_Segs::e_Denseg) {
                        // Dense-seg (1 hit)

                        const CDense_seg& denseg = s.GetSegs().GetDenseg();
                        int align_score = 0;
                        double evalue = 0;
            
                        ITERATE(CSeq_align::TScore, score_itr, s.GetScore()) {
                            const CScore& curr_score = **score_itr;
                            if (curr_score.GetId().GetStr() == "score")
                                align_score = curr_score.GetValue().GetInt();
                            else if (curr_score.GetId().GetStr() == "e_value")
                                evalue = curr_score.GetValue().GetReal();
                        }
            
                        // check if the hit is worth saving
                        if (evalue > blastp_evalue)
                            continue;
            
                        m_LocalHits.AddToHitList(new CHit(list1_oid, indices[j],
                                                      align_score, denseg));
                    }
                    else if (s.GetSegs().Which() == 
                                             CSeq_align::C_Segs::e_Dendiag) {
                        // Dense-diag (all hits)

                        ITERATE(CSeq_align::C_Segs::TDendiag, diag_itr, 
                                                    s.GetSegs().GetDendiag()) {
                            const CDense_diag& dendiag = **diag_itr;
                            int align_score = 0;
                            double evalue = 0;
                
                            // compute the score of the hit
              
                            ITERATE(CDense_diag::TScores, score_itr, 
                                                        dendiag.GetScores()) {
                                const CScore& curr_score = **score_itr;
                                if (curr_score.GetId().GetStr() == "score") {
                                    align_score = 
                                        curr_score.GetValue().GetInt();
                                }
                                else if (curr_score.GetId().GetStr() == 
                                                                "e_value") {
                                    evalue = curr_score.GetValue().GetReal();
                                }
                            }
                
                            // check if the hit is worth saving
                            if (evalue > blastp_evalue)
                                continue;
                
                            m_LocalHits.AddToHitList(new CHit(list1_oid,
                                             indices[j], align_score, dendiag));
                        }
                    }
                }
            }
        }

        // proceed to net batch of sequence fragments
        batch_start += curr_batch.size();
    }
}

void
CMultiAligner::x_FindLocalHits(const TSeqLocVector& queries,
                               const vector<int>& indices)
{
    m_ProgressMonitor.stage = eLocalHitsSearch;

    // Clear off previous state if it exists

    m_LocalHits.PurgeAllHits();
    if (m_DomainHits.Empty()) {

        m_CombinedHits.PurgeAllHits();

        // Initialize the profile columns of the input sequences

        x_AssignDefaultResFreqs();
    }

    // Produce another set of queries that consist of the 'filler'
    // in the input data, i.e. all stretches of all sequences not
    // covered by at least one domain hit. Then align all the
    // filler regions against each other and add any new HSPs to
    // 'results'

    vector< CRef<objects::CSeq_loc> > filler_locs;
    vector<SSegmentLoc> filler_segs;
    x_MakeFillerBlocks(indices, filler_locs, filler_segs);
    x_AlignFillerBlocks(queries, indices, filler_locs, filler_segs);

    //-------------------------------------------------------
    if (m_Options->GetVerbose()) {
        printf("blastp hits:\n");
        for (int i = 0; i < m_LocalHits.Size(); i++) {
            CHit *hit = m_LocalHits.GetHit(i);
            printf("query %d %4d - %4d query %d %4d - %4d score %d\n",
                         hit->m_SeqIndex1, 
                         hit->m_SeqRange1.GetFrom(), 
                         hit->m_SeqRange1.GetTo(),
                         hit->m_SeqIndex2,
                         hit->m_SeqRange2.GetFrom(), 
                         hit->m_SeqRange2.GetTo(),
                         hit->m_Score);
        }
        printf("\n\n");
    }
    //-------------------------------------------------------

    m_CombinedHits.Append(m_LocalHits);
}



auto_ptr< vector<int> > CMultiAligner::x_AlignClusterQueries(
                                                     const TPhyTreeNode* node)
{
    // Traverse cluster tree

    // if leaf node, then create one-element list of node id
    if (node->IsLeaf()) {
        auto_ptr< vector<int> > result(new vector<int>());
        result->push_back(node->GetValue().GetId());
        return result;
    }

    // Traverse left and right subtree and gather node ids in the subtrees
    TPhyTreeNode::TNodeList_CI child(node->SubNodeBegin());
    
    auto_ptr< vector<int> > left_inds = x_AlignClusterQueries(*child);
    child++;

    _ASSERT(*child);
    auto_ptr< vector<int> > right_inds = x_AlignClusterQueries(*child);
    child++;
    _ASSERT(child == node->SubNodeEnd());

    // Get most similar sequences from different subtrees
    const CClusterer::TDistMatrix& dmat = m_Clusterer.GetDistMatrix();
    
    int left = -1, right = -1;
    double dist = 0.0;
    for (size_t i=0;i < left_inds->size();i++) {
        for (size_t j=0;j < right_inds->size();j++) {
            if (dist > dmat((*left_inds)[i], (*right_inds)[j]) || left < 0) {
                left = (*left_inds)[i];
                right = (*right_inds)[j];
                dist = dmat(left, right);
            }
        }
    }
    _ASSERT(left != right);
    
    // Align the found pair of sequences - one from each subtree
    double blastp_evalue = m_Options->GetBlastpEvalue();
    CRef<CBlastProteinOptionsHandle> blastp_opts(new CBlastProteinOptionsHandle);
    // deliberately set the cutoff e-value too high
    blastp_opts->SetEvalueThreshold(max(blastp_evalue, 10.0));
    blastp_opts->SetSegFiltering(false);

    SSeqLoc left_query(*m_tQueries[left], *m_Scope);
    SSeqLoc right_query(*m_tQueries[right], *m_Scope);

    CBl2Seq blaster(left_query, right_query, *blastp_opts);
    CRef<CSearchResultSet> v = blaster.RunEx();

    // Add hit to hitlist
    ITERATE(CSeq_align_set::Tdata, itr, v->GetResults(0, 0).GetSeqAlign()->Get()) {
        
        // iterate over hits

        const CSeq_align& s = **itr;

        if (s.GetSegs().Which() == CSeq_align::C_Segs::e_Denseg) {
            // Dense-seg (1 hit)

            const CDense_seg& denseg = s.GetSegs().GetDenseg();
            int align_score = 0;
            double evalue = 0;
            
            ITERATE(CSeq_align::TScore, score_itr, s.GetScore()) {
                const CScore& curr_score = **score_itr;
                if (curr_score.GetId().GetStr() == "score")
                    align_score = curr_score.GetValue().GetInt();
                else if (curr_score.GetId().GetStr() == "e_value")
                    evalue = curr_score.GetValue().GetReal();
            }
            
            // check if the hit is worth saving
            if (evalue > blastp_evalue)
                continue;
            
            m_LocalInClusterHits.AddToHitList(new CHit(left, right, align_score,
                                                       denseg));
        }
        else if (s.GetSegs().Which() == CSeq_align::C_Segs::e_Dendiag) {
            // Dense-diag (all hits)
            
            ITERATE(CSeq_align::C_Segs::TDendiag, diag_itr, 
                    s.GetSegs().GetDendiag()) {
                const CDense_diag& dendiag = **diag_itr;
                int align_score = 0;
                double evalue = 0;
                
                // compute the score of the hit
              
                ITERATE(CDense_diag::TScores, score_itr, dendiag.GetScores()) {
                    const CScore& curr_score = **score_itr;
                    if (curr_score.GetId().GetStr() == "score") {
                        align_score = 
                            curr_score.GetValue().GetInt();
                    }
                    else if (curr_score.GetId().GetStr() == 
                             "e_value") {
                        evalue = curr_score.GetValue().GetReal();
                    }
                }
                
                // check if the hit is worth saving
                if (evalue > blastp_evalue)
                    continue;
                
                m_LocalInClusterHits.AddToHitList(new CHit(left, right,
                                                   align_score, dendiag));
            }
        }
    }


    // Return sequences from this subtree
    ITERATE(vector<int>, it, *right_inds) {
        left_inds->push_back(*it);
    }
    return left_inds;
}



void CMultiAligner::x_FindLocalInClusterHits(
                                     const vector<TPhyTreeNode*>& cluster_trees)
{
    m_LocalInClusterHits.PurgeAllHits();

    // Traverse cluster trees and find local constraints for each left and
    // right subtree of each substree
    ITERATE(vector<TPhyTreeNode*>, it, cluster_trees) {
        // NULL trees denore one-element clusters, nothing to do in such cases
        if (*it) {
            x_AlignClusterQueries(*it);
        }
    }

    //-------------------------------------------------------
    if (m_Options->GetVerbose()) {
        printf("in-cluster blastp hits:\n");
        for (int i = 0; i < m_LocalInClusterHits.Size(); i++) {
            CHit *hit = m_LocalInClusterHits.GetHit(i);
            printf("query %d %4d - %4d query %d %4d - %4d score %d\n",
                         hit->m_SeqIndex1, 
                         hit->m_SeqRange1.GetFrom(), 
                         hit->m_SeqRange1.GetTo(),
                         hit->m_SeqIndex2,
                         hit->m_SeqRange2.GetFrom(), 
                         hit->m_SeqRange2.GetTo(),
                         hit->m_Score);
        }
        printf("\n\n");
    }
    //-------------------------------------------------------
}

                                  

END_SCOPE(cobalt)
END_NCBI_SCOPE
