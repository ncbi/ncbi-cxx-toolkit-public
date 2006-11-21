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
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/IUPACna.hpp>
#include <objtools/alnmgr/alnvec.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/annot_selector.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_loc_mapper.hpp>
#include <algo/sequence/consensus_splice.hpp>

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


// Like CSeq_align::GetSeqStrand, but if the strand is not set,
// returns eNa_strand_plus rather than throwing an exception
static ENa_strand s_GetSeqStrand(const CSeq_align& aln, CSeq_align::TDim row)
{
    try {
        return aln.GetSeqStrand(row);
    } catch(CSeqalignException& e) {
        return eNa_strand_plus;
    }
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
    SAnnotSelector sel(CSeqFeatData::eSubtype_cdregion);
    sel.SetResolveDepth(0);
    CFeat_CI it(xcript_hand, sel);
    bool has_cds(it);
    TSeqPos cds_from;
    TSeqPos cds_to;
    CAlnVec::TSignedRange cds_range;
    const CSeq_id& genomic_id = disc.front()->GetSeq_id(1);
    CBioseq_Handle genomic_hand = scope.GetBioseqHandle(genomic_id);
    if (has_cds) {
        const CSeq_loc& cds_loc = it->GetLocation();
        cds_from = sequence::GetStart(cds_loc, 0);
        cds_to   = sequence::GetStop(cds_loc, 0);
        cds_range.SetFrom(static_cast<TSignedSeqPos>(cds_from));
        cds_range.SetTo(static_cast<TSignedSeqPos>(cds_to));

        // Assess whether differences between transcript and genome
        // would affect the protein produced
        CSeq_loc_Mapper mapper(*aln, 1, &scope);
        CRef<CSeq_loc> genomic_cds_loc = mapper.Map(cds_loc);
        const CGenetic_code* code = 0;
        if (it->GetData().GetCdregion().CanGetCode()) {
            code = &it->GetData().GetCdregion().GetCode();
        }
        string xcript_prot, genomic_prot;
        CSeqTranslator::Translate(cds_loc, xcript_hand,
                                  xcript_prot, code);
        CSeqTranslator::Translate(*genomic_cds_loc, genomic_hand,
                                  genomic_prot, code);
        // Say that they "can make same prot" iff the translations
        // are the same AND do not contain ambiguities
        bool can_make_same_prot = false;
        if (xcript_prot == genomic_prot &&
            xcript_prot.find_first_not_of("ACDEFGHIKLMNPQRSTVWY*") == NPOS) {
                                  // no need to check genomic (it's equal)

            can_make_same_prot = true;  // provisionally true, but...

            // Demand that any code-breaks annotated on transcript
            // are identical at the nucleotide level in the genomic
            if (it->GetData().GetCdregion().IsSetCode_break()) {
                ITERATE (CCdregion::TCode_break, cb,
                         it->GetMappedFeature().GetData().GetCdregion()
                         .GetCode_break()) {
                    const CCode_break& code_break = **cb;
                    const CSeq_loc& xcript_cb_loc = code_break.GetLoc();
                    CRef<CSeq_loc> genomic_cb_loc = mapper.Map(xcript_cb_loc);

                    CSeqVector gvec(*genomic_cb_loc, scope);
                    string gseq;
                    gvec.GetSeqData(0, gvec.size(), gseq);

                    CSeqVector xvec(xcript_cb_loc, scope);
                    string xseq;
                    xvec.GetSeqData(0, xvec.size(), xseq);

                    if (gseq != xseq) {
                        can_make_same_prot = false;
                        break;
                    }
                }
            }
        }
        result->SetOutput_data()
            .AddField("can_make_same_prot", can_make_same_prot);
    }


    TSeqPos last_genomic_end = 0;
    TSeqPos aligned_residue_count = 0;
    TSeqPos cds_aligned_residue_count = 0;
    TSeqPos match_count = 0, possible_match_count = 0;
    TSeqPos cds_match_count = 0, cds_possible_match_count = 0;
    vector<int> cds_match_count_by_frame(3);
    vector<int> cds_possible_match_count_by_frame(3);
    TSeqPos total_splices = 0;
    TSeqPos consensus_splices = 0;
    TSeqPos total_cds_splices = 0;
    TSeqPos consensus_cds_splices = 0;
    
    int exon_index = 0;
    TSeqPos min_exon_length = 0, max_exon_length = 0;      // initialize to
    TSeqPos min_intron_length = 0, max_intron_length = 0;  // avoid warnings
    TSeqPos exon_length, intron_length;
    TSeqPos exon_match_count, exon_possible_match_count;
    double worst_exon_match_frac = 1.1;  // guarantee that something is worse
    TSeqPos worst_exon_match_count;
    TSeqPos worst_exon_possible_match_count;
    TSeqPos worst_exon_length;
    int indel_count = 0;
    int cds_indel_count = 0;

    // These will be used for counting genomic ambiguities
    CSeq_data genomic_seq_data;
    string& genomic_seq = genomic_seq_data.SetIupacna().Set();
    CSeq_data genomic_cds_seq_data;
    string& genomic_cds_seq = genomic_cds_seq_data.SetIupacna().Set();

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

        if (s_GetSeqStrand(exon, 1) == eNa_strand_minus) {
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
            if (s_GetSeqStrand(exon, 1) == eNa_strand_minus) {
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

            if (s_GetSeqStrand(exon, 1) == eNa_strand_minus) {
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

            bool consensus_splice = IsConsensusSplice(splice5, splice3);
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

        if (s_GetSeqStrand(exon, 1) == eNa_strand_minus) {
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
            unsigned int downstream_intron_count =
                disc.size() - exon_index - 1;
            result->SetOutput_data()
                .AddField("introns_3_prime_of_stop",
                          (int) downstream_intron_count);
            if (downstream_intron_count > 0) {
                result->SetOutput_data()
                    .AddField("dist_stop_to_exon_end",
                              int(exon.GetSeqStop(0) - cds_to));
                result->SetOutput_data()
                    .AddField("dist_stop_to_last_intron",
                              int((*++disc.rbegin())->GetSeqStop(0) - cds_to));
            }
        }

        // count of transcript residues doing various things
        CAlnVec avec(exon.GetSegs().GetDenseg(), scope);
        avec.SetGapChar('-');
        avec.SetEndChar('-');
        exon_match_count = 0;
        exon_possible_match_count = 0;

        // need to be careful here because segment may begin with gap
        bool in_cds = has_cds
            && avec.GetSeqStart(0) > cds_from
            && avec.GetSeqStart(0) <= cds_to;

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
                            if (cds_match_count == 0
                                && cds_possible_match_count == 0) {
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
                    } else if (!gap_in_xcript) {
                        unsigned char cdna_res =
                            CAlnVec::FromIupac(avec.GetResidue(0, i));
                        unsigned char genomic_res =
                            CAlnVec::FromIupac(avec.GetResidue(1, i));
                        // count as a possible match if
                        // cdna is a subset of genomic
                        if (!(cdna_res & ~genomic_res)) {
                            ++exon_possible_match_count;
                            if (in_cds) {
                                if (cds_match_count == 0
                                    && cds_possible_match_count == 0) {
                                    // this is first cds match (probably start)
                                    lag = 0;
                                }
                                ++cds_possible_match_count;
                                frame = lag % 3;
                                if (frame < 0) {
                                    frame += 3;
                                }
                                ++cds_possible_match_count_by_frame[frame];
                            }
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
            if (!gap_in_genomic && in_cds) {
                genomic_cds_seq.push_back(avec.GetResidue(1, i));
            }
        }
        match_count += exon_match_count;
        possible_match_count += exon_possible_match_count;

        // look for "worst" exon
        double exon_match_frac =
            double(exon_match_count + exon_possible_match_count) / exon_length;
        if (exon_match_frac < worst_exon_match_frac) {
            worst_exon_match_frac = exon_match_frac;
            worst_exon_match_count = exon_match_count;
            worst_exon_possible_match_count = exon_possible_match_count;
            worst_exon_length = exon_length;
        }

        // count indels (count as 1, regardless of length)
        in_cds = has_cds
            && avec.GetSeqStart(0) > cds_from
            && avec.GetSeqStart(0) <= cds_to;  // only for mRNA gaps

        for (int seg = 0; seg < avec.GetNumSegs();  ++seg) {
            if (avec.GetStart(0, seg) == -1) {
                ++indel_count;
                if (in_cds) {
                    ++cds_indel_count;
                }
            } else {
                if (avec.GetStart(1, seg) == -1) {
                    ++indel_count;
                    // any overlap of segment with cds counts
                    if (has_cds
                        && cds_range.IntersectingWith(avec.GetRange(0, seg))) {
                        ++cds_indel_count;
                    }
                }
                in_cds = has_cds
                    && avec.GetStop(0, seg)
                    >= static_cast<TSignedSeqPos>(cds_from)
                    && avec.GetStop(0, seg)
                    < static_cast<TSignedSeqPos>(cds_to);
            }
        }

        string exon_genomic_seq;
        avec.GetSeqString(exon_genomic_seq, 1,
                          avec.GetSeqStart(1), avec.GetSeqStop(1));
        genomic_seq += exon_genomic_seq;

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
    result->SetOutput_data()
        .AddField("possibly_matching_residues", (int) possible_match_count);
    if (has_cds) {
        result->SetOutput_data()
            .AddField("cds_matching_residues", (int) cds_match_count);
        result->SetOutput_data()
            .AddField("cds_possibly_matching_residues",
                      (int) cds_possible_match_count);
        result->SetOutput_data()
            .AddField("cds_aligned_residues", (int) cds_aligned_residue_count);
        result->SetOutput_data()
            .AddField("in_frame_cds_matching_residues",
                      (int) cds_match_count_by_frame[0]);
        result->SetOutput_data()
            .AddField("in_frame_cds_possibly_matching_residues",
                      (int) cds_possible_match_count_by_frame[0]);
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
        .AddField("worst_exon_possible_matches",
                  int(worst_exon_possible_match_count));
    result->SetOutput_data()
        .AddField("worst_exon_length", int(worst_exon_length));

    result->SetOutput_data()
        .AddField("indel_count", indel_count);
    result->SetOutput_data()
        .AddField("cds_indel_count", cds_indel_count);

    CSeq_data out_seq;
    vector<TSeqPos> out_indices;
    TSeqPos gac =
        CSeqportUtil::GetAmbigs(genomic_seq_data, &out_seq, &out_indices);
    result->SetOutput_data()
        .AddField("genomic_ambiguity_count", int(gac));
    TSeqPos gcac =
        CSeqportUtil::GetAmbigs(genomic_cds_seq_data, &out_seq, &out_indices);
    result->SetOutput_data()
        .AddField("genomic_cds_ambiguity_count", int(gcac));


    // Some tests involving the genomic sequence

    const CSeq_align& first_exon = *disc.front();
    const CSeq_align& last_exon = *disc.back();
    bool is_minus = s_GetSeqStrand(first_exon, 1) == eNa_strand_minus;

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
 * Revision 1.23  2006/11/21 18:51:44  jcherry
 * Added genomic_ambiguity_count and genomic_cds_ambiguity_count
 *
 * Revision 1.22  2006/10/18 15:51:18  jcherry
 * Moved consensus splice determination into external function
 *
 * Revision 1.21  2006/07/19 22:29:49  jcherry
 * Fixed copy-and-paste error in previous commit
 *
 * Revision 1.20  2006/07/19 20:25:42  jcherry
 * For "can_make_same_prot", added additional check that any code-break
 * locations in the transcript are identical at the nucleotide level
 * to the genomic (this is conservative; if there are differences, a person
 * will have to determine whether they are consistent with the code-break)
 *
 * Revision 1.19  2006/07/18 19:18:52  jcherry
 * Added "can_make_same_prot" result
 *
 * Revision 1.18  2006/02/13 16:51:47  jcherry
 * Added ambiguous match tests, changed "worst exon" tests accordingly,
 * and added explicit indel count
 *
 * Revision 1.17  2006/02/07 17:49:35  jcherry
 * Added dist_stop_to_last_intron for NMD detection
 *
 * Revision 1.16  2005/12/12 20:19:46  jcherry
 * Added test "dist_stop_to_exon_end" for cases with introns
 * 3' of translational stop
 *
 * Revision 1.15  2005/11/21 14:46:41  jcherry
 * Fix for case where exon begins with gap
 *
 * Revision 1.14  2005/06/23 20:23:49  jcherry
 * Deal with missing strands (assume they're plus)
 *
 * Revision 1.13  2005/03/14 18:19:02  grichenk
 * Added SAnnotSelector(TFeatSubtype), fixed initialization of CFeat_CI and
 * SAnnotSelector.
 *
 * Revision 1.12  2005/02/28 16:10:17  jcherry
 * Initialize variables to eliminate compilation warnings
 *
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
