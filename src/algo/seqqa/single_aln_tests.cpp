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
 * Authors:  Josh Cherry
 *
 * File Description:  Tests operating on an alignment of a transcript to
 *                      genomic DNA
 *
 */

#include <ncbi_pch.hpp>
#include <algo/seqqa/single_aln_tests.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objtools/alnmgr/alnvec.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/annot_selector.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

bool CTestSingleAln::CanTest(const CSerialObject& obj,
                             const CSeqTestContext* ctx) const
{
    const CSeq_align* aln = dynamic_cast<const CSeq_align*>(&obj);
    if (aln) {
        if (aln->GetType() == ncbi::CSeq_align::eType_disc) {
            return true;
        }
    }
    return false;
}


CRef<CSeq_test_result_set>
CTestSingleAln_All::RunTest(const CSerialObject& obj,
                            const CSeqTestContext* ctx)
{
    CRef<CSeq_test_result_set> ref;
    const CSeq_align* aln = dynamic_cast<const CSeq_align*>(&obj);
    if ( !aln  ||  !ctx ) {
        return ref;
    }

    ref.Reset(new CSeq_test_result_set());

    CRef<CSeq_test_result> result = x_SkeletalTestResult("singlealn_all");
    ref->Set().push_back(result);

    CScope& scope = ctx->GetScope();

    const CSeq_align_set::Tdata& disc = aln->GetSegs().GetDisc().Get();
    result->SetOutput_data()
        .AddField("exon_count", (int) disc.size());

    CBioseq_Handle xcript_hand
        = scope.GetBioseqHandle(disc.front()->GetSeq_id(0));
    SAnnotSelector sel;
    sel.SetFeatSubtype(CSeqFeatData::eSubtype_cdregion);
    sel.SetResolveDepth(0);
    CFeat_CI it(xcript_hand, sel);
    bool has_cds(it);
    TSeqPos cds_from;
    TSeqPos cds_to;
    if (has_cds) {
        const CSeq_loc& loc = it->GetLocation();
        cds_from = sequence::GetStart(loc, 0);
        cds_to   = sequence::GetStop(loc, 0);
    }


    TSeqPos last_genomic_end = 0;
    TSeqPos aligned_residue_count = 0;
    TSeqPos cds_aligned_residue_count = 0;
    TSeqPos match_count = 0;
    TSeqPos cds_match_count = 0;
    vector<int> cds_match_count_by_frame(3);
    TSeqPos total_splices = 0;
    TSeqPos consensus_splices = 0;
    TSeqPos total_cds_splices = 0;
    TSeqPos consensus_cds_splices = 0;
    
    int exon_index = 0;
    TSeqPos min_exon_length, max_exon_length;
    TSeqPos min_intron_length, max_intron_length;
    TSeqPos exon_length, intron_length;
    TSeqPos exon_match_count;
    double worst_exon_match_frac = 1.1;  // guarantee that something is worse
    TSeqPos worst_exon_match_count;
    TSeqPos worst_exon_length;

    int lag;  // cumulative gaps in xcript minus genomic in cds
    int frame;

    ITERATE (CSeq_align_set::Tdata, iter, disc) {
        const CSeq_align& exon = **iter;
        CRange<TSeqPos> r = exon.GetSeqRange(0);
        exon_length = r.GetLength();
        if (exon_index == 0 || exon_length > max_exon_length) {
            max_exon_length = exon_length;
        }
        if (exon_index == 0 || exon_length < min_exon_length) {
            min_exon_length = exon_length;
        }

        if (exon.GetSeqStrand(1) == eNa_strand_minus) {
            intron_length = last_genomic_end - exon.GetSeqStop(1)- 1;
        } else {
            intron_length = exon.GetSeqStart(1) - last_genomic_end - 1;
        }
        
        if (exon_index > 0) {
            // intron length
            if (exon_index == 1 || intron_length > max_intron_length) {
                max_intron_length = intron_length;
            }
            if (exon_index == 1 || intron_length < min_intron_length) {
                min_intron_length = intron_length;
            }

            // splice consensus
            CSeq_loc loc;
            loc.SetInt().SetId().Assign(exon.GetSeq_id(1));
            if (exon.GetSeqStrand(1) == eNa_strand_minus) {
                loc.SetInt().SetFrom(last_genomic_end - 2);
                loc.SetInt().SetTo  (last_genomic_end - 1);
                loc.SetInt().SetStrand(eNa_strand_minus);
            } else {
                loc.SetInt().SetFrom(last_genomic_end + 1);
                loc.SetInt().SetTo  (last_genomic_end + 2);
            }
            CSeqVector vec5(loc, scope);
            vec5.SetIupacCoding();
            string splice5;
            vec5.GetSeqData(0, 2, splice5);

            if (exon.GetSeqStrand(1) == eNa_strand_minus) {
                loc.SetInt().SetFrom(exon.GetSeqStop(1) + 1);
                loc.SetInt().SetTo  (exon.GetSeqStop(1) + 2);
                loc.SetInt().SetStrand(eNa_strand_minus);                
            } else {
                loc.SetInt().SetFrom(exon.GetSeqStart(1) - 2);
                loc.SetInt().SetTo  (exon.GetSeqStart(1) - 1);
            }
            CSeqVector vec3(loc, scope);
            vec3.SetIupacCoding();
            string splice3;
            vec3.GetSeqData(0, 2, splice3);

            bool consensus_splice;
            if ((splice5 == "GT" || splice5 == "GC") && splice3 == "AG") {
                consensus_splice = true;
            } else if (splice5 == "AT" && splice3 == "AC") {
                consensus_splice = true;
            } else {
                consensus_splice = false;
            }
            total_splices += 1;
            if (consensus_splice) {
                ++consensus_splices;
            }
            if (has_cds) {
                if (cds_from < exon.GetSeqStart(0)
                    && cds_to >= exon.GetSeqStart(0)) {
                    ++total_cds_splices;
                    if (consensus_splice) {
                        ++consensus_cds_splices;
                    }
                }
            }
        } // exon_index > 0

        if (exon.GetSeqStrand(1) == eNa_strand_minus) {
            last_genomic_end = exon.GetSeqStart(1);
        } else {
            last_genomic_end = exon.GetSeqStop(1);
        }

        if (cds_from >= exon.GetSeqStart(0)
            && cds_from <= exon.GetSeqStop(0)) {
            result->SetOutput_data()
                .AddField("introns_5_prime_of_start", exon_index);
        }
        if (cds_to >= exon.GetSeqStart(0)
            && cds_to <= exon.GetSeqStop(0)) {
            result->SetOutput_data()
                .AddField("introns_3_prime_of_stop",
                          (int) disc.size() - exon_index - 1);
        }

        // count of transcript residues doing various things
        CAlnVec avec(exon.GetSegs().GetDenseg(), scope);
        avec.SetGapChar('-');
        exon_match_count = 0;
        bool in_cds = false;
        for (TSeqPos i = 0;  i <= avec.GetAlnStop();  ++i) {
            bool gap_in_xcript  = avec.GetResidue(0, i) == '-';
            bool gap_in_genomic = avec.GetResidue(1, i) == '-';
            
            if (!gap_in_xcript) {
                TSeqPos pos = avec.GetSeqPosFromAlnPos(0, i);
                if (has_cds) {
                    in_cds = pos >= cds_from && pos <= cds_to;
                }
                if (!gap_in_genomic) {
                    ++aligned_residue_count;
                    if (in_cds) {
                        ++cds_aligned_residue_count;
                    }
                    if (avec.GetResidue(0, i) == avec.GetResidue(1, i)) {
                        ++exon_match_count;
                        if (in_cds) {
                            if (cds_match_count == 0) {
                                // this is first cds match (probably start)
                                lag = 0;
                            }
                            ++cds_match_count;
                            frame = lag % 3;
                            if (frame < 0) {
                                frame += 3;
                            }
                            ++cds_match_count_by_frame[frame];
                        }
                    }
                }
            }
            if (gap_in_xcript && in_cds) {
                ++lag;
            }
            if (gap_in_genomic && in_cds) {
                --lag;
            }
        }
        match_count += exon_match_count;

        // look for "worst" exon
        double exon_match_frac = double(exon_match_count) / exon_length;
        if (exon_match_frac < worst_exon_match_frac) {
            worst_exon_match_frac = exon_match_frac;
            worst_exon_match_count = exon_match_count;
            worst_exon_length = exon_length;
        }
        ++exon_index;
    }

    result->SetOutput_data()
        .AddField("max_exon_length", (int) max_exon_length);
    result->SetOutput_data()
        .AddField("min_exon_length", (int) min_exon_length);
    if (disc.size() > 1) {
        result->SetOutput_data()
            .AddField("max_intron_length", (int) max_intron_length);
        result->SetOutput_data()
            .AddField("min_intron_length", (int) min_intron_length);
    }
    result->SetOutput_data()
        .AddField("aligned_residues", (int) aligned_residue_count);
    result->SetOutput_data()
        .AddField("matching_residues", (int) match_count);
    if (has_cds) {
        result->SetOutput_data()
            .AddField("cds_matching_residues", (int) cds_match_count);
        result->SetOutput_data()
            .AddField("cds_aligned_residues", (int) cds_aligned_residue_count);
        result->SetOutput_data()
            .AddField("in_frame_cds_matching_residues",
                      (int) cds_match_count_by_frame[0]);
    }

    result->SetOutput_data()
        .AddField("total_splices_in_alignment", (int) total_splices);
    result->SetOutput_data()
        .AddField("consensus_splices_in_alignment", (int) consensus_splices);
    if (has_cds) {
        result->SetOutput_data()
            .AddField("total_cds_splices_in_alignment",
                      (int) total_cds_splices);
        result->SetOutput_data()
            .AddField("consensus_cds_splices_in_alignment",
                      (int) consensus_cds_splices);
    }
    result->SetOutput_data()
        .AddField("5_prime_bases_not_aligned",
                  (int) disc.front()->GetSeqStart(0));
    result->SetOutput_data()
        .AddField("3_prime_bases_not_aligned",
                  (int) (xcript_hand.GetBioseqLength()
                  - disc.back()->GetSeqStop(0) - 1));
    if (has_cds) {
        result->SetOutput_data()
            .AddField("start_codon_in_aligned_region",
                      disc.front()->GetSeqStart(0) <= cds_from);
        result->SetOutput_data()
            .AddField("stop_codon_in_aligned_region",
                      disc.back()->GetSeqStop(0) >= cds_to);
    }

    result->SetOutput_data()
        .AddField("worst_exon_matches", int(worst_exon_match_count));
    result->SetOutput_data()
        .AddField("worst_exon_length", int(worst_exon_length));


    // Some tests involving the genomic sequence

    const CSeq_align& first_exon = *disc.front();
    const CSeq_align& last_exon = *disc.back();
    const CSeq_id& genomic_id = first_exon.GetSeq_id(1);
    CBioseq_Handle genomic_hand = scope.GetBioseqHandle(genomic_id);
    bool is_minus = first_exon.GetSeqStrand(1) == eNa_strand_minus;

    // Distance from alignment limits to genomic sequence
    // gap or end

    TSeqPos genomic_earliest, genomic_latest;
    if (is_minus) {
        genomic_earliest = last_exon.GetSeqStart(1);
        genomic_latest = first_exon.GetSeqStop(1);
    } else {
        genomic_earliest = first_exon.GetSeqStart(1);
        genomic_latest = last_exon.GetSeqStop(1);
    }
    const CSeqMap& seq_map = genomic_hand.GetSeqMap();

    CSeqMap_CI map_iter = seq_map.Begin(0);
    TSeqPos last = 0;
    while (map_iter.GetType() != CSeqMap::eSeqEnd) {
        if (map_iter.GetPosition() > genomic_earliest) {
            break;
        }
        if (map_iter.GetType() == CSeqMap::eSeqGap) {
            last = map_iter.GetEndPosition() + 1;
        }
        ++map_iter;
    }
    result->SetOutput_data()
        .AddField(is_minus ? "downstream_dist_to_genomic_gap_or_end"
                  : "upstream_dist_to_genomic_gap_or_end",
                  int(genomic_earliest) - int(last));

    while (map_iter) {
        ++map_iter;
    }
    --map_iter;
    last = genomic_hand.GetBioseqLength() - 1;
    while (map_iter.GetType() != CSeqMap::eSeqEnd) {
        if (map_iter.GetEndPosition() < genomic_latest) {
            break;
        }
        if (map_iter.GetType() == CSeqMap::eSeqGap) {
            last = map_iter.GetPosition() - 1;
        }
        --map_iter;
    }
    result->SetOutput_data()
        .AddField(is_minus ? "upstream_dist_to_genomic_gap_or_end"
                  : "downstream_dist_to_genomic_gap_or_end",
                  int(last) - int(genomic_latest));


    // A nearby annotated upstream gene on same strand?
    // Do this only for single-"exon" alignments because
    // it's slow.
    if (disc.size() == 1) {
        TSeqPos kLargestGeneDist = 10000;  // how far upstream to look
        TSeqPos genomic_start;
        TSeqPos genomic_roi_from;  // region on genomic
        TSeqPos genomic_roi_to;    // that interests us

        if (is_minus) {
            genomic_start = first_exon.GetSeqStop(1);
            if (genomic_start < genomic_hand.GetBioseqLength() - 1) {
                genomic_roi_from = genomic_start + 1;
            } else {
                genomic_roi_from = genomic_start;
            }
            if (genomic_start + kLargestGeneDist + 1
                < genomic_hand.GetBioseqLength()) {
                genomic_roi_to = genomic_start + kLargestGeneDist + 1;
            } else {
                genomic_roi_to = genomic_hand.GetBioseqLength() - 1;
            }
        } else {
            genomic_start = first_exon.GetSeqStart(1);
            if (genomic_start > kLargestGeneDist) {
                genomic_roi_from = genomic_start - kLargestGeneDist - 1;
            } else {
                genomic_roi_from = 0;
            }
            if (genomic_start > 1) {
                genomic_roi_to = genomic_start - 1;
            } else {
                genomic_roi_to = 0;
            }
        }
        SAnnotSelector gene_sel(CSeqFeatData::e_Gene);
        gene_sel.SetResolveAll();
        CFeat_CI it(scope,
                    *genomic_hand.GetRangeSeq_loc(genomic_roi_from,
                                                  genomic_roi_to),
                    gene_sel);
        TSeqPos shortest_dist = kLargestGeneDist + 1;
        while (it) {
            const CSeq_loc& gene_loc = it->GetLocation();
            if (is_minus) {
                if (sequence::GetStrand(gene_loc) == eNa_strand_minus) {
                    if (sequence::GetStart(gene_loc, 0) > genomic_start) {
                        shortest_dist = sequence::GetStart(gene_loc, 0)
                            - genomic_start - 1;
                    }
                }
            } else {
                if (sequence::GetStrand(gene_loc) != eNa_strand_minus) {
                    if (sequence::GetStop(gene_loc, 0) < genomic_start) {
                        shortest_dist = genomic_start
                            - sequence::GetStop(gene_loc, 0) - 1;
                    }
                }
            }
            ++it;
        }
        result->SetOutput_data()
            .AddField("has_nearby_upstream_gene_same_strand",
                      shortest_dist <= kLargestGeneDist);
        if (shortest_dist <= kLargestGeneDist) {
            result->SetOutput_data()
                .AddField("distance_from_upstream_gene_same_strand",
                          int(shortest_dist));
        }

    }

    return ref;
}

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.11  2005/02/07 19:26:58  jcherry
 * Deal properly with negative result of modulo operator
 *
 * Revision 1.10  2005/02/01 19:41:35  jcherry
 * Set alignment vector gap character appropriately
 *
 * Revision 1.9  2004/11/18 21:27:40  grichenk
 * Removed default value for scope argument in seq-loc related functions.
 *
 * Revision 1.8  2004/11/16 21:41:11  grichenk
 * Removed default value for CScope* argument in CSeqMap methods
 *
 * Revision 1.7  2004/11/01 19:33:08  grichenk
 * Removed deprecated methods
 *
 * Revision 1.6  2004/10/14 17:06:34  jcherry
 * Deal with lack of CDS
 *
 * Revision 1.5  2004/10/13 15:49:06  jcherry
 * Use resolve depth of zero rather than adaptive depth
 *
 * Revision 1.4  2004/10/12 22:10:10  jcherry
 * Don't assume that a CDS location is an interval
 *
 * Revision 1.3  2004/10/10 21:56:45  jcherry
 * Don't call CMappedFeat::GetMappedFeature; as of recent changes, this
 * apparently doesn't work when the feature has not been mapped.
 *
 * Revision 1.2  2004/10/06 21:49:05  jcherry
 * Use "adaptive depth" (apparently will be necessary for use
 * of "fake" transcripts representing genomic annotation)
 *
 * Revision 1.1  2004/10/06 19:57:15  jcherry
 * Initial version
 *
 * ===========================================================================
 */
