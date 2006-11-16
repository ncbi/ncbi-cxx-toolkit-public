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
CMultiAligner::x_AddNewSegment(TSeqLocVector& loc_list, SSeqLoc& query, 
                TOffset from, TOffset to, vector<SSegmentLoc>& seg_list,
                int query_index)
{
    // Note that all offsets are zero-based

    CRef<CSeq_loc> seqloc(new CSeq_loc());
    seqloc->SetInt().SetFrom(from);
    seqloc->SetInt().SetTo(to);
    seqloc->SetInt().SetStrand(eNa_strand_unknown);
    seqloc->SetInt().SetId().Assign(sequence::GetId(*query.seqloc, 
                                                  query.scope));
    SSeqLoc sl(seqloc, query.scope);
    loc_list.push_back(sl);

    seg_list.push_back(SSegmentLoc(query_index, from, to));
}

/// Turn all fragments of query sequence not already covered by
/// a domain hit into a separate query sequence, used as input
/// to a blast search
/// @param filler_locs List of generated sequences [out]
/// @param filler_segs Simplified representation of filler_locs [out]
///
void
CMultiAligner::x_MakeFillerBlocks(TSeqLocVector& filler_locs,
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

    for (int i = 0; i < num_queries; i++) {
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

        int seq_length = sequence::GetLength(*m_tQueries[i].seqloc,
                                             m_tQueries[i].scope);

        if (seq_length - seg_start > CHit::kMinHitSize) {
            x_AddNewSegment(filler_locs, m_tQueries[i], seg_start,
                            seq_length - 1, filler_segs, i);
        }
    }

    if (m_Verbose) {
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
/// @param filler_locs List of generated sequences [in]
/// @param filler_segs Simplified representation of filler_locs [in]
///
void
CMultiAligner::x_AlignFillerBlocks(TSeqLocVector& filler_locs, 
                                   vector<SSegmentLoc>& filler_segs)
{
    if (filler_locs.empty())
        return;

    // Set the blast options

    CBlastProteinOptionsHandle blastp_opts;
    // deliberately set the cutoff e-value too high
    blastp_opts.SetEvalueThreshold(max(m_BlastpEvalue, 10.0));
    blastp_opts.SetSegFiltering(false);
    CBl2Seq blaster(filler_locs, m_tQueries, blastp_opts);
    TSeqAlignVector v = blaster.Run();

    // Convert each resulting HSP into a CHit object

    // iterate over query sequence fragments

    for (int i = 0; i < (int)filler_locs.size(); i++) {

        int list1_oid = filler_segs[i].seq_index;
        int list2_oid = 0;

        // iterate over hitlists

        ITERATE(CSeq_align_set::Tdata, itr, v[i]->Get()) {

            const CSeq_align& hitlist_sa = **itr;

            // skip hits that map to the same query sequence

            if (list1_oid == list2_oid) {
                list2_oid++;
                continue;
            }

            // iterate over hits

            ITERATE(CSeq_align_set::Tdata, sitr, 
                                 hitlist_sa.GetSegs().GetDisc().Get()) {

                const CSeq_align& s = **sitr;
                const CDense_seg& denseg = s.GetSegs().GetDenseg();
                int align_score = 0;
                double evalue = 0;

                // compute the score of the hit

                ITERATE(CSeq_align::TScore, score_itr, s.GetScore()) {
                    const CScore& curr_score = **score_itr;
                    if (curr_score.GetId().GetStr() == "score")
                        align_score = curr_score.GetValue().GetInt();
                    else if (curr_score.GetId().GetStr() == "e_value")
                        evalue = curr_score.GetValue().GetReal();
                }

                // check if the hit is worth saving
                if (evalue > m_BlastpEvalue)
                    continue;

                // save the hit

                m_LocalHits.AddToHitList(new CHit(list1_oid, list2_oid,
                                                    align_score, denseg));
            }

            list2_oid++;
        }
    }
}

void
CMultiAligner::FindLocalHits()
{
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

    TSeqLocVector filler_locs;
    vector<SSegmentLoc> filler_segs;
    x_MakeFillerBlocks(filler_locs, filler_segs);
    x_AlignFillerBlocks(filler_locs, filler_segs);

    //-------------------------------------------------------
    if (m_Verbose) {
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

END_SCOPE(cobalt)
END_NCBI_SCOPE

/* ====================================================================
 * $Log$
 * Revision 1.13  2006/11/16 17:46:33  papadopo
 * remove deprecated function, replace with code that iterates through seqaligns
 *
 * Revision 1.12  2006/06/15 17:43:03  papadopo
 * PartialRun -> RunWithoutSeqalignGeneration
 *
 * Revision 1.11  2006/03/22 19:23:17  dicuccio
 * Cosmetic changes: adjusted include guards; formatted CVS logs; added export
 * specifiers
 *
 * Revision 1.10  2006/01/27 19:13:02  papadopo
 * fix off-by-one error
 *
 * Revision 1.9  2005/11/17 22:28:09  papadopo
 * fix documentation, add doxygen
 *
 * Revision 1.8  2005/11/16 16:59:20  papadopo
 * replace += operator with Append member
 *
 * Revision 1.7  2005/11/14 16:17:08  papadopo
 * Assign default residue frequencies before domain residue frequencies,
 * not after. This guarantees residue frequencies get assigned properly
 * if an alignment process is repeated
 *
 * Revision 1.6  2005/11/10 16:18:31  papadopo
 * Allow hitlists to be regenerated cleanly
 *
 * Revision 1.5  2005/11/10 15:37:08  papadopo
 * Make AddNewSegmet into a member of CMultiAligner
 *
 * Revision 1.4  2005/11/08 18:42:16  papadopo
 * assert -> _ASSERT
 *
 * Revision 1.3  2005/11/08 17:51:15  papadopo
 * 1. Make header files self-sufficient
 * 2. ASSERT -> assert
 *
 * Revision 1.1  2005/11/07 18:14:00  papadopo
 * Initial revision
 *
 * ====================================================================
 */
