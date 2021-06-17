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
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_exon_chunk.hpp>
#include <objects/seqalign/Product_pos.hpp>
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
        // Can analyze a disc alignment...
        if (aln->GetType() == ncbi::CSeq_align::eType_disc) {
            return true;
        }
        // ...or a Spliced-seg where product is a transcript
        if (aln->GetSegs().IsSpliced()) {
            if (aln->GetSegs().GetSpliced().GetProduct_type()
                == CSpliced_seg::eProduct_type_transcript) {
                return true;
            }
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
    } catch(CSeqalignException&) {
        return eNa_strand_plus;
    }
}


static CRef<CSeq_align> s_SplicedToDisc(const CSeq_align& spliced_seg_aln);


//if neighborhood < 0 - search upstream of cleavage; otherwise downstream.
static size_t s_GetPolyA_genomic_priming(const CSeq_align& aln, CScope& scope, int neighborhood)
{
    CBioseq_Handle bsh = scope.GetBioseqHandle(aln.GetSeq_id(1));

    CRef<CSeq_loc> loc(new CSeq_loc); //upstream or downstream neighborhood loc
    {{
        ENa_strand strand = s_GetSeqStrand(aln, 1);

        //cleavage position
        TSeqPos pos = strand == eNa_strand_minus ? aln.GetSeqStart(1) : aln.GetSeqStop(1);

        //if searching downstream of cleavage, start with first pos past end of alignment
        if(neighborhood > 0 && pos > 0 && pos < bsh.GetInst_Length() - 1) {
            pos += strand == eNa_strand_minus ? -1 : +1;
        }

        loc->SetInt().SetId().Assign(aln.GetSeq_id(1));
        loc->SetInt().SetFrom(pos);
        loc->SetInt().SetTo(pos);
        loc->SetInt().SetStrand(strand);

        if(   (neighborhood >= 0 && strand != eNa_strand_minus)
           || (neighborhood < 0  && strand == eNa_strand_minus)) 
        {
            loc->SetInt().SetTo(min(bsh.GetInst_Length() - 1, pos + abs(neighborhood)));
        } else {
            loc->SetInt().SetFrom(pos < abs(neighborhood) ? 0 : pos - abs(neighborhood));
        }
    }}

    CSeqVector vec(*loc, scope, CBioseq_Handle::eCoding_Iupac);
    string seq("");

    vec.GetSeqData(vec.begin(), vec.end(), seq);
    if(neighborhood < 0) {
        reverse(seq.begin(), seq.end());
    }

    size_t best_pos(NPOS);
    {{
        static const int w_match = 1;
        static const int w_mismatch = -4;
        static const int x_dropoff = 15;

        int best_score = 0;
        int curr_score = 0;

        for(size_t curr_pos = 0; 
            curr_pos < seq.size() && curr_score + x_dropoff > best_score; 
            ++curr_pos) 
        {
            curr_score += seq[curr_pos] == 'A' ? w_match : w_mismatch;
            if(curr_score >= best_score) {
                best_score = curr_score;
                best_pos = curr_pos;
            }
        }
    }}
    size_t priming_length = best_pos == NPOS ? 0 : best_pos + 1;

    //string label;
    //loc->GetLabel(&label);
    //LOG_POST(aln.GetSeq_id(0).AsFastaString() << "\t" << label << "\t" << neighborhood << "\t" << seq << "\t" << priming_length);

    return priming_length;
}



CRef<CSeq_test_result_set>
CTestSingleAln_All::RunTest(const CSerialObject& obj,
                            const CSeqTestContext* ctx)
{
    CRef<CSeq_test_result_set> ref;
    CConstRef<CSeq_align> aln(dynamic_cast<const CSeq_align*>(&obj));
    if ( !aln  ||  !ctx) {
        return ref;
    }

    // Handle a Spliced-seg by converting it to a disc
    if (aln->GetSegs().IsSpliced()) {
        aln = s_SplicedToDisc(*aln);
    } else if(!aln->GetSegs().IsDisc()) {
        NCBI_THROW(CException, eUnknown, "Expected spliced or disc alignment");
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
    TSeqPos cds_from = 0, cds_to = 0;  // initialize to avoid compiler warning
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
        CSeqTranslator::Translate(cds_loc, scope, xcript_prot, code);
        CSeqTranslator::Translate(*genomic_cds_loc, scope, genomic_prot, code);
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
    TSeqPos worst_exon_match_count = 0;           // initialize to
    TSeqPos worst_exon_possible_match_count = 0;  // avoid
    TSeqPos worst_exon_length = 0;                // compiler warnings
    int indel_count = 0;
    int cds_indel_count = 0;
    TSeqPos exon_length_3p = 0;
    TSeqPos exon_length_5p = 0;

    // These will be used for counting genomic ambiguities
    CSeq_data genomic_seq_data;
    string& genomic_seq = genomic_seq_data.SetIupacna().Set();
    CSeq_data genomic_cds_seq_data;
    string& genomic_cds_seq = genomic_cds_seq_data.SetIupacna().Set();

    int lag = 0;  // cumulative gaps in xcript minus genomic in cds;
                  // initialize to avoid compiler warnings
    int frame;

    ITERATE (CSeq_align_set::Tdata, iter, disc) {
        const CSeq_align& exon = **iter;
        CRange<TSeqPos> r = exon.GetSeqRange(0);
        exon_length = r.GetLength();

        if(exon_index == 0) {
            exon_length_5p = exon_length;
        }
        exon_length_3p = exon_length;

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

    result->SetOutput_data()
        .AddField("5p_terminal_exon_length", (int) exon_length_5p);
    result->SetOutput_data()
        .AddField("3p_terminal_exon_length", (int) exon_length_3p);

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


    result->SetOutput_data()
        .AddField("upstream_polya_priming", 
                  static_cast<int>(s_GetPolyA_genomic_priming(*aln, scope, -200)));
    result->SetOutput_data()
        .AddField("downstream_polya_priming", 
                  static_cast<int>(s_GetPolyA_genomic_priming(*aln, scope, 200)));
    
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


// Conversion from Spliced-seg to disc.
// This should probably eventually be moved, e.g., to CSeq_align.

static vector<TSignedSeqPos>
s_CalculateStarts(const vector<TSeqPos>& lens, ENa_strand strand,
                  TSeqPos start, TSeqPos end)
{
    vector<TSignedSeqPos> rv;
    rv.reserve(lens.size());
    TSignedSeqPos offset = 0;
    ITERATE (vector<TSeqPos>, len, lens) {
        if (*len == 0) {
            // a gap
            rv.push_back(-1);
        } else {
            if (IsReverse(strand)) {
                offset += *len;
                rv.push_back((end + 1) - offset);
            } else {
                rv.push_back(start + offset);
                offset += *len;
            }
        }
    }
    return rv;
}


static CRef<CDense_seg>
s_ExonToDenseg(const CSpliced_exon& exon,
               ENa_strand product_strand, ENa_strand genomic_strand,
               const CSeq_id& product_id, const CSeq_id& genomic_id)
{
    CRef<CDense_seg> ds(new CDense_seg);

    vector<TSeqPos> product_lens;
    vector<TSeqPos> genomic_lens;
    ITERATE (CSpliced_exon::TParts, iter, exon.GetParts()) {
        const CSpliced_exon_chunk& part = **iter;
        if (part.IsMatch()) {
            product_lens.push_back(part.GetMatch());
            genomic_lens.push_back(part.GetMatch());
        } else if (part.IsMismatch()) {
            product_lens.push_back(part.GetMismatch());
            genomic_lens.push_back(part.GetMismatch());
        } else if (part.IsDiag()) {
            product_lens.push_back(part.GetDiag());
            genomic_lens.push_back(part.GetDiag());
        } else if (part.IsProduct_ins()) {
            product_lens.push_back(part.GetProduct_ins());
            genomic_lens.push_back(0);
        } else if (part.IsGenomic_ins()) {
            product_lens.push_back(0);
            genomic_lens.push_back(part.GetGenomic_ins());
        } else {
            throw runtime_error("unhandled part type in Spliced-enon");
        }
    }

    CDense_seg::TLens& lens = ds->SetLens();
    lens.reserve(product_lens.size());
    for (unsigned int i = 0; i < product_lens.size(); ++i) {
        lens.push_back(max(product_lens[i], genomic_lens[i]));
    }
    vector<TSignedSeqPos> product_starts =
        s_CalculateStarts(product_lens, product_strand,
                          exon.GetProduct_start().GetNucpos(),
                          exon.GetProduct_end().GetNucpos());
    vector<TSignedSeqPos> genomic_starts =
        s_CalculateStarts(genomic_lens, genomic_strand,
                          exon.GetGenomic_start(),
                          exon.GetGenomic_end());

    CDense_seg::TStarts& starts = ds->SetStarts();
    starts.reserve(product_starts.size() + genomic_starts.size());
    for (unsigned int i = 0; i < lens.size(); ++i) {
        starts.push_back(product_starts[i]);  // product row first
        starts.push_back(genomic_starts[i]);
    }
    ds->SetIds().push_back(CRef<CSeq_id>(SerialClone(product_id)));
    ds->SetIds().push_back(CRef<CSeq_id>(SerialClone(genomic_id)));

    // Set strands, unless they're both plus
    if (!(product_strand == eNa_strand_plus
          && genomic_strand == eNa_strand_plus)) {
        CDense_seg::TStrands& strands = ds->SetStrands();
        for (unsigned int i = 0; i < lens.size(); ++i) {
            strands.push_back(product_strand);
            strands.push_back(genomic_strand);
        }
    }

    ds->SetNumseg(lens.size());

    ds->Compact();  // join adjacent match/mismatch/diag parts
    return ds;
}


static CRef<CSeq_align> s_SplicedToDisc(const CSeq_align& spliced_seg_aln)
{

    CRef<CSeq_align> disc(new CSeq_align);
    disc->SetType(CSeq_align::eType_disc);

    const CSpliced_seg& spliced_seg = spliced_seg_aln.GetSegs().GetSpliced();

    ENa_strand product_strand = spliced_seg.GetProduct_strand();
    ENa_strand genomic_strand = spliced_seg.GetGenomic_strand();
    const CSeq_id& product_id = spliced_seg.GetProduct_id();
    const CSeq_id& genomic_id = spliced_seg.GetGenomic_id();

    //for exon in spliced_seg.GetSegs().GetSpliced().GetExons()[:] {
    ITERATE (CSpliced_seg::TExons, exon, spliced_seg.GetExons()) {
        CRef<CDense_seg> ds = s_ExonToDenseg(**exon,
                                             product_strand, genomic_strand,
                                             product_id, genomic_id);
        CRef<CSeq_align> ds_align(new CSeq_align);
        ds_align->SetSegs().SetDenseg(*ds);
        ds_align->SetType(CSeq_align::eType_partial);
        disc->SetSegs().SetDisc().Set().push_back(ds_align);
    }
    return disc;
}


END_NCBI_SCOPE
