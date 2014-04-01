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
 * Authors:  Vyacheslav Chetvernin
 *
 * File Description: Alignment transformations
 *
 */
#include <ncbi_pch.hpp>
#include <algo/sequence/gene_model.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <objects/general/User_object.hpp>

#include <objmgr/bioseq_handle.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/sequence.hpp>

#include "feature_generator.hpp"

BEGIN_NCBI_SCOPE

USING_SCOPE(objects);


namespace {

pair <ENa_strand, ENa_strand> GetSplicedStrands(const CSpliced_seg& spliced_seg)
{
    ENa_strand product_strand =
        spliced_seg.IsSetProduct_strand() ?
        spliced_seg.GetProduct_strand() :
        (spliced_seg.GetExons().front()->IsSetProduct_strand() ?
         spliced_seg.GetExons().front()->GetProduct_strand() :
         eNa_strand_unknown);
    ENa_strand genomic_strand =
        spliced_seg.IsSetGenomic_strand() ?
        spliced_seg.GetGenomic_strand() :
        (spliced_seg.GetExons().front()->IsSetGenomic_strand()?
         spliced_seg.GetExons().front()->GetGenomic_strand():
         eNa_strand_unknown);

    return make_pair(product_strand, genomic_strand);
}

void GetExonStructure(const CSpliced_seg& spliced_seg, vector<SExon>& exons, CScope& scope)
{
    pair <ENa_strand, ENa_strand> strands = GetSplicedStrands(spliced_seg);
    ENa_strand product_strand = strands.first;
    ENa_strand genomic_strand = strands.second;

    exons.resize(spliced_seg.GetExons().size());
    int i = 0;
    TSignedSeqPos prev_genomic_pos = 0;
    TSignedSeqPos offset = 0;
    ITERATE(CSpliced_seg::TExons, it, spliced_seg.GetExons()) {
        const CSpliced_exon& exon = **it;
        SExon& exon_struct = exons[i++];

        const CProduct_pos& prod_from = exon.GetProduct_start();
        const CProduct_pos& prod_to = exon.GetProduct_end();

        exon_struct.prod_from = prod_from.AsSeqPos();
        exon_struct.prod_to = prod_to.AsSeqPos();
        if (product_strand == eNa_strand_minus) {
            swap(exon_struct.prod_from, exon_struct.prod_to);
            exon_struct.prod_from = -exon_struct.prod_from;
            exon_struct.prod_to = -exon_struct.prod_to;
        }

        exon_struct.genomic_from = exon.GetGenomic_start();
        exon_struct.genomic_to = exon.GetGenomic_end();

        bool cross_the_origin = i > 1 && (
            genomic_strand != eNa_strand_minus
            ? (exon_struct.genomic_from < prev_genomic_pos)
            : (exon_struct.genomic_from > prev_genomic_pos));

        if (cross_the_origin) {
            offset = scope.GetSequenceLength(spliced_seg.GetGenomic_id());
        }

        prev_genomic_pos = exon_struct.genomic_from;
        
        if (genomic_strand == eNa_strand_minus) {
            swap(exon_struct.genomic_from, exon_struct.genomic_to);
            exon_struct.genomic_from = -exon_struct.genomic_from;
            exon_struct.genomic_to = -exon_struct.genomic_to;
        }

        if (offset) {
            exon_struct.genomic_from += offset;
            exon_struct.genomic_to += offset;
        }

    }

    _ASSERT( exons.size() == spliced_seg.GetExons().size() );
}

void SetProtpos(CProduct_pos &pos, int value)
{
    pos.SetProtpos().SetAmin(value/3);
    pos.SetProtpos().SetFrame((value % 3) +1);
}

}

void CFeatureGenerator::SImplementation::StitchSmallHoles(CSeq_align& align)
{
    CSpliced_seg& spliced_seg = align.SetSegs().SetSpliced();

    if (!spliced_seg.CanGetExons() || spliced_seg.GetExons().size() < 2)
        return;

    vector<SExon> exons;
    GetExonStructure(spliced_seg, exons, *m_scope);

    bool is_protein = (spliced_seg.GetProduct_type()==CSpliced_seg::eProduct_type_protein);

    pair <ENa_strand, ENa_strand> strands = GetSplicedStrands(spliced_seg);
    ENa_strand product_strand = strands.first;
    ENa_strand genomic_strand = strands.second;

    int product_min_pos;
    int product_max_pos;
    if (product_strand != eNa_strand_minus) {
        product_min_pos = 0;
        if (spliced_seg.IsSetPoly_a()) {
            product_max_pos = spliced_seg.GetPoly_a()-1;
        } else if (spliced_seg.IsSetProduct_length()) {
            product_max_pos = spliced_seg.GetProduct_length()-1;
            if (is_protein)
                product_max_pos *= 3+2;
        } else {
            product_max_pos = exons.back().prod_to;
        }
    } else {
        if (spliced_seg.IsSetProduct_length()) {
            product_min_pos = -int(spliced_seg.GetProduct_length())+1;
            if (is_protein)
                product_min_pos = product_min_pos*3-2;
        } else {
            product_min_pos = exons[0].prod_from;
        }
        if (spliced_seg.IsSetPoly_a()) {
            product_max_pos = -int(spliced_seg.GetPoly_a())+1;
        } else {
            product_max_pos = 0;
        }
    }

    CSpliced_seg::TExons::iterator it = spliced_seg.SetExons().begin();
    CRef<CSpliced_exon> prev_exon = *it;
    size_t i = 1;
    for (++it; it != spliced_seg.SetExons().end();  ++i, prev_exon = *it++) {
        CSpliced_exon& exon = **it;

        bool donor_set = prev_exon->IsSetDonor_after_exon();
        bool acceptor_set = exon.IsSetAcceptor_before_exon();

        if(donor_set && acceptor_set && exons[i-1].prod_to + 1 == exons[i].prod_from) {
            continue;
        }

        _ASSERT( exons[i].prod_from > exons[i-1].prod_to );
        int prod_hole_len = exons[i].prod_from - exons[i-1].prod_to -1;
        _ASSERT( exons[i].genomic_from > exons[i-1].genomic_to );
        int genomic_hole_len = exons[i].genomic_from - exons[i-1].genomic_to -1;

        if (prod_hole_len >= (int)m_min_intron || genomic_hole_len >= (int)m_min_intron)
            continue;

        if (!prev_exon->IsSetParts() || prev_exon->GetParts().empty()) {
            CRef< CSpliced_exon_chunk > part(new CSpliced_exon_chunk);
            part->SetMatch(exons[i-1].prod_to-exons[i-1].prod_from+1);
            prev_exon->SetParts().push_back(part);
        }
        if (!exon.IsSetParts() || exon.GetParts().empty()) {
            CRef< CSpliced_exon_chunk > part(new CSpliced_exon_chunk);
            part->SetMatch(exons[i].prod_to-exons[i].prod_from+1);
            exon.SetParts().push_back(part);
        }

        int max_hole_len = max(prod_hole_len, genomic_hole_len);
        int min_hole_len = min(prod_hole_len, genomic_hole_len);
        int left_mismatch_len = 0;
        int right_mismatch_len = min_hole_len;
        if (prod_hole_len != genomic_hole_len) {
            // does not matter for transcripts, but for proteins ensures insersions at codon boundary
            int bases_needed_to_complete_codon = 2 - (exons[i-1].prod_to % 3);
        
            if (right_mismatch_len >= bases_needed_to_complete_codon) {
                left_mismatch_len = bases_needed_to_complete_codon + ((right_mismatch_len-bases_needed_to_complete_codon)/2/3)*3;
                right_mismatch_len -= left_mismatch_len;
            }
        }

        bool no_acceptor_before = i > 1 && !prev_exon->IsSetAcceptor_before_exon();
        bool no_donor_after = i < exons.size()-1 && !exon.IsSetDonor_after_exon();


        bool cross_the_origin =
            genomic_strand != eNa_strand_minus
            ? (prev_exon->GetGenomic_start() > exon.GetGenomic_start())
            : (prev_exon->GetGenomic_start() < exon.GetGenomic_start());

        if (cross_the_origin) {
            int genomic_size = m_scope->GetSequenceLength(spliced_seg.GetGenomic_id());

            prev_exon->SetPartial(product_min_pos < exons[i-1].prod_from  &&
                                  no_acceptor_before);

            exon.SetPartial(exons[i].prod_to < product_max_pos &&
                            no_donor_after);

            if (genomic_strand != eNa_strand_minus) {
                prev_exon->SetGenomic_end(genomic_size-1);
                exon.SetGenomic_start(0);
            } else {
                prev_exon->SetGenomic_start(0);
                exon.SetGenomic_end(genomic_size-1);
            }

            int origin = genomic_strand != eNa_strand_minus ? genomic_size : 1;
            int to_origin = origin - exons[i-1].genomic_to -1;
            if (prod_hole_len == genomic_hole_len) {
                left_mismatch_len = to_origin;
                right_mismatch_len -= left_mismatch_len;
            }

            if (left_mismatch_len > 0 && to_origin > 0) {
                int mismatch_len = min(left_mismatch_len, to_origin);
                CRef< CSpliced_exon_chunk > part(new CSpliced_exon_chunk);
                part->SetMismatch(mismatch_len);
                prev_exon->SetParts().push_back(part);
                prod_hole_len -= mismatch_len;
                genomic_hole_len -= mismatch_len;
                to_origin -= mismatch_len;
                exons[i-1].genomic_to += mismatch_len;
                exons[i-1].prod_to += mismatch_len;
                left_mismatch_len -= mismatch_len;
            }

            if (to_origin > 0) {
                _ASSERT(left_mismatch_len == 0);
                _ASSERT(prod_hole_len != genomic_hole_len);
                CRef< CSpliced_exon_chunk > part(new CSpliced_exon_chunk);
                if (prod_hole_len < genomic_hole_len) {
                    int genomic_ins = min(genomic_hole_len-prod_hole_len, to_origin);
                    part->SetGenomic_ins(genomic_ins);
                    genomic_hole_len -= genomic_ins;
                    to_origin -= genomic_ins;
                    exons[i-1].genomic_to += genomic_ins;
                } else {
                    part->SetProduct_ins(prod_hole_len-genomic_hole_len);
                    exons[i-1].prod_to += prod_hole_len-genomic_hole_len;
                    prod_hole_len = genomic_hole_len;
                }
                prev_exon->SetParts().push_back(part);
            }
            if (to_origin > 0) {
                _ASSERT(prod_hole_len == genomic_hole_len);
                _ASSERT(right_mismatch_len >= to_origin);
                int mismatch_len = to_origin;
                CRef< CSpliced_exon_chunk > part(new CSpliced_exon_chunk);
                part->SetMismatch(mismatch_len);
                prev_exon->SetParts().push_back(part);
                prod_hole_len -= mismatch_len;
                genomic_hole_len -= mismatch_len;
                to_origin = 0;
                exons[i-1].genomic_to += mismatch_len;
                exons[i-1].prod_to += mismatch_len;
                right_mismatch_len -= mismatch_len;
            }

            _ASSERT(to_origin == 0);
            _ASSERT(exons[i-1].genomic_to == origin-1);

            exons[i].prod_from = exons[i-1].prod_to+1;
            exons[i].genomic_from = exons[i-1].genomic_to+1;

            if (is_protein) {
                prev_exon->SetProduct_end().SetProtpos().SetAmin() = exons[i-1].prod_to/3;
                prev_exon->SetProduct_end().SetProtpos().SetFrame() = (exons[i-1].prod_to %3) +1;
                exon.SetProduct_start().SetProtpos().SetAmin() = exons[i].prod_from/3;
                exon.SetProduct_start().SetProtpos().SetFrame() = (exons[i].prod_from %3) +1;
            } else if (product_strand != eNa_strand_minus) {
                prev_exon->SetProduct_end().SetNucpos( exons[i-1].prod_to );
                exon.SetProduct_start().SetNucpos( exons[i].prod_from );
            } else {
                prev_exon->SetProduct_start().SetNucpos( -exons[i-1].prod_to );
                exon.SetProduct_end().SetNucpos( -exons[i].prod_from );
            }

            list <CRef< CSpliced_exon_chunk > >::iterator insertion_point = exon.SetParts().begin();

            if (left_mismatch_len > 0) {
                CRef< CSpliced_exon_chunk > part(new CSpliced_exon_chunk);
                part->SetMismatch(left_mismatch_len);
                insertion_point = exon.SetParts().insert(insertion_point, part);
                ++insertion_point;
            }
            if (prod_hole_len != genomic_hole_len) {
                CRef< CSpliced_exon_chunk > part(new CSpliced_exon_chunk);
                if (prod_hole_len < genomic_hole_len) {
                    part->SetGenomic_ins(genomic_hole_len - prod_hole_len);
                } else {
                    part->SetProduct_ins(prod_hole_len - genomic_hole_len);
                }
                insertion_point = exon.SetParts().insert(insertion_point, part);
                ++insertion_point;
            }
            if (right_mismatch_len > 0) {
                CRef< CSpliced_exon_chunk > part(new CSpliced_exon_chunk);
                part->SetMismatch(right_mismatch_len);
                exon.SetParts().insert(insertion_point, part);

            }

        } else {

            if (is_protein || product_strand != eNa_strand_minus) {
                prev_exon->SetProduct_end().Assign( exon.GetProduct_end() );
            } else {
                prev_exon->SetProduct_start().Assign( exon.GetProduct_start() );
            }
        
            if (genomic_strand != eNa_strand_minus) {
                prev_exon->SetGenomic_end() = exon.GetGenomic_end();
            } else {
                prev_exon->SetGenomic_start() = exon.GetGenomic_start();
            }

            if (left_mismatch_len > 0) {
                CRef< CSpliced_exon_chunk > part(new CSpliced_exon_chunk);
                part->SetMismatch(left_mismatch_len);
                prev_exon->SetParts().push_back(part);
            }
            if (prod_hole_len != genomic_hole_len) {
               CRef< CSpliced_exon_chunk > part(new CSpliced_exon_chunk);
                if (prod_hole_len < genomic_hole_len) {
                    part->SetGenomic_ins(max_hole_len - min_hole_len);
                } else {
                    part->SetProduct_ins(max_hole_len - min_hole_len);
                }
                prev_exon->SetParts().push_back(part);
            }
            if (right_mismatch_len > 0) {
                CRef< CSpliced_exon_chunk > part(new CSpliced_exon_chunk);
                part->SetMismatch(right_mismatch_len);
                prev_exon->SetParts().push_back(part);

            }
            prev_exon->SetParts().splice(prev_exon->SetParts().end(), exon.SetParts());

            // TO DO: Recalculate or approximate scores
            prev_exon->SetScores().Set().splice(prev_exon->SetScores().Set().end(), exon.SetScores().Set());
            if (exon.IsSetDonor_after_exon()) {
                prev_exon->SetDonor_after_exon().Assign( exon.GetDonor_after_exon() );
            } else {
                prev_exon->ResetDonor_after_exon();
            }

            exons[i].prod_from = exons[i-1].prod_from;
            exons[i].genomic_from = exons[i-1].genomic_from;

            prev_exon->SetPartial(
                                  (product_min_pos < exons[i-1].prod_from  && no_acceptor_before) ||
                                  (exons[i].prod_to < product_max_pos  && no_donor_after));

            if (exon.IsSetExt()) {
                prev_exon->SetExt().splice(prev_exon->SetExt().end(), exon.SetExt());
            }

            CSpliced_seg::TExons::iterator save_it = it;
            --save_it;
            spliced_seg.SetExons().erase(it);
            it = save_it;
        }
    }
}

void CFeatureGenerator::SImplementation::TrimHolesToCodons(CSeq_align& align)
{
    CSpliced_seg& spliced_seg = align.SetSegs().SetSpliced();

    if (!spliced_seg.CanGetExons())
        return;

    bool is_protein = (spliced_seg.GetProduct_type()==CSpliced_seg::eProduct_type_protein);

    pair <ENa_strand, ENa_strand> strands = GetSplicedStrands(spliced_seg);
    ENa_strand product_strand = strands.first;
    ENa_strand genomic_strand = strands.second;

    TSignedSeqRange cds;
    if (is_protein) {
        cds = TSignedSeqRange(0, spliced_seg.GetProduct_length()*3 - 1);
    } else {
        if (!spliced_seg.CanGetProduct_id())
            return;
        cds = GetCds(spliced_seg.GetProduct_id());
        if (cds.Empty())
            return;
        if (product_strand == eNa_strand_minus) {
            NCBI_THROW(CException, eUnknown,
                       "TrimHolesToCodons(): "
                       "Reversed mRNA with CDS");
        }
    }

    vector<SExon> exons;
    GetExonStructure(spliced_seg, exons, *m_scope);

    int frame_offset = (exons.back().prod_to/3+1)*3+cds.GetFrom(); // to make modulo operands always positive

    vector<SExon>::iterator right_exon_it = exons.begin();
    CSpliced_seg::TExons::iterator right_spl_exon_it = spliced_seg.SetExons().begin();

    for(;;++right_exon_it, ++right_spl_exon_it) {

        vector<SExon>::reverse_iterator left_exon_it(right_exon_it); 
        CSpliced_seg::TExons::reverse_iterator left_spl_exon_it(right_spl_exon_it);

        if (right_exon_it != exons.begin() && right_exon_it != exons.end()) {
            bool donor_set = left_spl_exon_it != spliced_seg.SetExons().rend() && (*left_spl_exon_it)->IsSetDonor_after_exon();
            bool acceptor_set = right_spl_exon_it != spliced_seg.SetExons().end() && (*right_spl_exon_it)->IsSetAcceptor_before_exon();

            if(((donor_set && acceptor_set) || left_exon_it->genomic_to + 1 == right_exon_it->genomic_from) && left_exon_it->prod_to + 1 == right_exon_it->prod_from) {
                continue;
            }
        }

        if (right_exon_it != exons.begin() && (right_exon_it != exons.end() || (m_flags & fTrimEnds)) &&
            cds.GetFrom() < left_exon_it->prod_to && left_exon_it->prod_to < cds.GetTo()
            )
            TrimLeftExon((left_exon_it->prod_to - cds.GetFrom() + 1) % 3, eTrimProduct,
                         exons.rend(), left_exon_it, left_spl_exon_it,
                         product_strand, genomic_strand);

        if (right_exon_it != exons.end() && (right_exon_it != exons.begin() || (m_flags & fTrimEnds)) &&
            cds.GetFrom() < right_exon_it->prod_from && right_exon_it->prod_from < cds.GetTo()
            )
            TrimRightExon((frame_offset-right_exon_it->prod_from) % 3, eTrimProduct,
                          right_exon_it, exons.end(), right_spl_exon_it,
                          product_strand, genomic_strand);
        
        if (left_exon_it.base() != right_exon_it) {
            right_exon_it = exons.erase(left_exon_it.base(), right_exon_it);
            right_spl_exon_it = spliced_seg.SetExons().erase(left_spl_exon_it.base(), right_spl_exon_it);
        }

        if (right_exon_it == exons.end())
            break;
    }
    _ASSERT(right_exon_it == exons.end() && right_spl_exon_it == spliced_seg.SetExons().end());
}

void CFeatureGenerator::SImplementation::MaximizeTranslation(CSeq_align& align)
{
    CSpliced_seg& spliced_seg = align.SetSegs().SetSpliced();
    bool is_protein_align =
        spliced_seg.GetProduct_type() == CSpliced_seg::eProduct_type_protein;

    int aa_offset = 0;

    NON_CONST_ITERATE (CSpliced_seg::TExons, exon_it, spliced_seg.SetExons()) {
        CSpliced_exon& exon = **exon_it;
        if (aa_offset) {
            if (is_protein_align)
                exon.SetProduct_start().SetProtpos().SetAmin() += aa_offset;
            else
                exon.SetProduct_start().SetNucpos() += aa_offset*3;
        }
        if (exon.IsSetParts()) {
            ERASE_ITERATE (CSpliced_exon::TParts, part_it, exon.SetParts()) {
                CSpliced_exon_chunk& chunk = **part_it;
                switch ((*part_it)->Which()) {
                case CSpliced_exon_chunk::e_Genomic_ins: {
                    int len = chunk.GetGenomic_ins();
                    if (len % 3 == 0) {
                        chunk.SetDiag(len);
                    } else if (len > 3) {
                        CRef<CSpliced_exon_chunk> new_chunk(new CSpliced_exon_chunk);
                        new_chunk->SetGenomic_ins(len % 3);
                        exon.SetParts().insert(part_it, new_chunk);
                        chunk.SetDiag((len/3)*3);
                    }
                    aa_offset += len/3;
                }
                    break;
                case CSpliced_exon_chunk::e_Product_ins: {
                    int len = chunk.GetProduct_ins();
                    if (len % 3 == 0) {
                        exon.SetParts().erase(part_it);
                    } else {
                        chunk.SetProduct_ins(len % 3);
                    }
                    aa_offset -= len/3;
                }
                    break;
                default:
                    break;
                }
            }
        }
        if (aa_offset) {
            if (is_protein_align)
                exon.SetProduct_end().SetProtpos().SetAmin() += aa_offset;
            else
                exon.SetProduct_end().SetNucpos() += aa_offset*3;
        }
    }
    if (aa_offset) {
        spliced_seg.SetProduct_length() += is_protein_align ? aa_offset : aa_offset*3;
    }
}

CConstRef<CSeq_align> CFeatureGenerator::AdjustAlignment(const CSeq_align& align_in, TSeqRange range)
{
    return m_impl->AdjustAlignment(align_in, range);
}

CConstRef<CSeq_align> CFeatureGenerator::SImplementation::AdjustAlignment(const CSeq_align& align_in, TSeqRange range)
{
    if (!align_in.CanGetSegs() || !align_in.GetSegs().IsSpliced())
        return CConstRef<CSeq_align>(&align_in);

    CRef<CSeq_align> align(new CSeq_align);
    align->Assign(align_in);

    CSpliced_seg& spliced_seg = align->SetSegs().SetSpliced();

    pair <ENa_strand, ENa_strand> strands = GetSplicedStrands(spliced_seg);
    ENa_strand product_strand = strands.first;
    ENa_strand genomic_strand = strands.second;

    if (product_strand == eNa_strand_minus) {
        NCBI_THROW(CException, eUnknown,
                   "AdjustAlignment(): "
                   "product minus strand not supported");
        
    }

    bool plus_strand = !(genomic_strand == eNa_strand_minus);

    TSeqRange align_range;
    if (plus_strand) {
        align_range = TSeqRange(spliced_seg.GetExons().front()->GetGenomic_start(),
                                spliced_seg.GetExons().back()->GetGenomic_end());
    } else {
        align_range = TSeqRange(spliced_seg.GetExons().back()->GetGenomic_start(),
                                spliced_seg.GetExons().front()->GetGenomic_end());
    }
    bool cross_the_origin = range.GetFrom() > range.GetTo() || align_range.GetFrom() > align_range.GetTo();
    TSeqPos genomic_size = 0;
    if (cross_the_origin) {
        genomic_size = m_scope->GetSequenceLength(spliced_seg.GetGenomic_id());


        if (range.GetFrom() > range.GetTo()) {
            range.SetTo(range.GetTo() + genomic_size);
        }
        if (align_range.GetFrom() > align_range.GetTo()) {
            align_range.SetTo(align_range.GetTo() + genomic_size);
        }

        if (range.GetTo() < align_range.GetFrom()) {
            range.SetFrom(range.GetFrom() + genomic_size);
            range.SetTo(range.GetTo() + genomic_size);
        }
        if (align_range.GetTo() < range.GetFrom()) {
            align_range.SetFrom(align_range.GetFrom() + genomic_size);
            align_range.SetTo(align_range.GetTo() + genomic_size);
        }

        TSeqPos outside_point = (min(range.GetFrom(), align_range.GetFrom())+max(range.GetTo(), align_range.GetTo())-genomic_size)/2;
        NON_CONST_ITERATE(CSpliced_seg::TExons, exon_it, spliced_seg.SetExons()) {
            CSpliced_exon& exon = **exon_it;
            if (exon.GetGenomic_start() < outside_point)
                exon.SetGenomic_start() += genomic_size;
            if (exon.GetGenomic_end() < outside_point)
                exon.SetGenomic_end() += genomic_size;
        }
    }

    _ASSERT(range.GetFrom() <= range.GetTo());
    _ASSERT(align_range.GetFrom() <= align_range.GetTo());
    _ASSERT(!(range.GetTo() < align_range.GetFrom()));
    _ASSERT(!(align_range.GetTo() < range.GetFrom()));

    vector<SExon> exons;
    GetExonStructure(spliced_seg, exons, *m_scope);

    bool is_protein_align =
        spliced_seg.GetProduct_type() == CSpliced_seg::eProduct_type_protein;

    vector<SExon>::iterator right_exon_it = exons.begin();
    CSpliced_seg::TExons::iterator right_spl_exon_it = spliced_seg.SetExons().begin();

    int range_left = plus_strand ? int(range.GetFrom()) : -int(range.GetTo());
    int range_right = plus_strand ? int(range.GetTo()) : -int(range.GetFrom());

    for(;;++right_exon_it, ++right_spl_exon_it) {

        vector<SExon>::reverse_iterator left_exon_it(right_exon_it); 
        CSpliced_seg::TExons::reverse_iterator left_spl_exon_it(right_spl_exon_it);

        if (right_exon_it == exons.end() &&
            left_exon_it->genomic_to > range_right
            )
            CFeatureGenerator::SImplementation::TrimLeftExon(left_exon_it->genomic_to - range_right, eTrimGenomic,
                         exons.rend(), left_exon_it, left_spl_exon_it,
                         product_strand, genomic_strand);

        if (right_exon_it == exons.begin() &&
            right_exon_it->genomic_from < range_left
            )
            CFeatureGenerator::SImplementation::TrimRightExon(range_left - right_exon_it->genomic_from, eTrimGenomic,
                          right_exon_it, exons.end(), right_spl_exon_it,
                          product_strand, genomic_strand);
        
        if (left_exon_it.base() != right_exon_it) {
            right_exon_it = exons.erase(left_exon_it.base(), right_exon_it);
            right_spl_exon_it = spliced_seg.SetExons().erase(left_spl_exon_it.base(), right_spl_exon_it);
        }

        if (right_exon_it == exons.end())
            break;
    }

    CSpliced_exon& first_exon = *spliced_seg.SetExons().front();
    CSpliced_exon& last_exon = *spliced_seg.SetExons().back();

    int first_exon_extension = 0;
    int last_exon_extension = 0;

    if (plus_strand) {

        first_exon_extension =
            first_exon.GetGenomic_start()
            - ((range.GetFrom() < genomic_size && genomic_size <= first_exon.GetGenomic_start())
               ? genomic_size
               : range.GetFrom());

        if (first_exon_extension > 0) {
            CRef<CSpliced_exon_chunk> chunk(new CSpliced_exon_chunk);
            chunk->SetDiag(first_exon.IsSetParts() ? first_exon_extension : (first_exon.GetGenomic_end() - range.GetFrom() + 1));
            first_exon.SetParts().insert(first_exon.SetParts().begin(), chunk);
            first_exon.SetGenomic_start() -= first_exon_extension;
        }

        last_exon_extension =
            ((last_exon.GetGenomic_end() <= genomic_size-1 && genomic_size-1 < range.GetTo())
             ? genomic_size-1
             : range.GetTo())
            - last_exon.GetGenomic_end();

        if (last_exon_extension > 0) {
            CRef<CSpliced_exon_chunk> chunk(new CSpliced_exon_chunk);
            chunk->SetDiag(last_exon.IsSetParts() ? last_exon_extension : (range.GetTo() - last_exon.GetGenomic_start() + 1));
            last_exon.SetParts().push_back(chunk);
            last_exon.SetGenomic_end() += last_exon_extension;
        }
    } else {
        last_exon_extension =
            last_exon.GetGenomic_start()
            - ((range.GetFrom() < genomic_size && genomic_size <= last_exon.GetGenomic_start())
               ? genomic_size
               : range.GetFrom());

        if (last_exon_extension > 0) {
            CRef<CSpliced_exon_chunk> chunk(new CSpliced_exon_chunk);
            chunk->SetDiag(last_exon.IsSetParts() ? last_exon_extension : (last_exon.GetGenomic_end() - range.GetFrom() + 1));
            last_exon.SetParts().push_back(chunk);
            last_exon.SetGenomic_start() -= last_exon_extension;
        }

        first_exon_extension =
            ((first_exon.GetGenomic_end() <= genomic_size-1 && genomic_size-1 < range.GetTo())
             ? genomic_size-1
             : range.GetTo())
            - first_exon.GetGenomic_end();
        if (first_exon_extension > 0) {
            CRef<CSpliced_exon_chunk> chunk(new CSpliced_exon_chunk);
            chunk->SetDiag(first_exon.IsSetParts() ? first_exon_extension : (range.GetTo() - first_exon.GetGenomic_start() + 1));
            first_exon.SetParts().insert(first_exon.SetParts().begin(), chunk);
            first_exon.SetGenomic_end() += first_exon_extension;
        }
    }

    exons.front().prod_from -= first_exon_extension;
    exons.front().genomic_from -= first_exon_extension;
    exons.back().prod_to += last_exon_extension;
    exons.back().genomic_to += last_exon_extension;


    if (plus_strand) {
        first_exon_extension = first_exon.GetGenomic_start() - range.GetFrom();

        if (first_exon_extension > 0) {
            CRef<CSpliced_exon> exon(new CSpliced_exon);
            exon->SetGenomic_start() = range.GetFrom();
            exon->SetGenomic_end() = genomic_size-1;
            spliced_seg.SetExons().push_front(exon);

            SExon exon_struct;
            exon_struct.prod_from = exons.front().prod_from - first_exon_extension;
            exon_struct.prod_to = exons.front().prod_from - 1;
            exon_struct.genomic_from = exons.front().genomic_from - first_exon_extension;
            exon_struct.genomic_to = exons.front().genomic_from - 1;

            exons.insert(exons.begin(), exon_struct);
        }

        last_exon_extension = range.GetTo() - last_exon.GetGenomic_end();

        if (last_exon_extension > 0) {
            CRef<CSpliced_exon> exon(new CSpliced_exon);
            exon->SetGenomic_start() = 0;
            exon->SetGenomic_end() = last_exon_extension - 1;
            spliced_seg.SetExons().push_back(exon);

            SExon exon_struct;
            exon_struct.prod_from = exons.back().prod_to + 1;
            exon_struct.prod_to = exons.back().prod_to + last_exon_extension;
            exon_struct.genomic_from = exons.back().genomic_to +1;
            exon_struct.genomic_to = exons.back().genomic_to + last_exon_extension;

            exons.push_back(exon_struct);
        }
    } else {
        last_exon_extension = last_exon.GetGenomic_start() - range.GetFrom();

        if (last_exon_extension > 0) {
            CRef<CSpliced_exon> exon(new CSpliced_exon);
            exon->SetGenomic_start() = range.GetFrom();
            exon->SetGenomic_end() = genomic_size-1;
            spliced_seg.SetExons().push_back(exon);

            SExon exon_struct;
            exon_struct.prod_from = exons.back().prod_to + 1;
            exon_struct.prod_to = exons.back().prod_to + last_exon_extension;
            exon_struct.genomic_from = exons.back().genomic_to +1;
            exon_struct.genomic_to = exons.back().genomic_to + last_exon_extension;

            exons.push_back(exon_struct);
        }

        first_exon_extension = range.GetTo() - first_exon.GetGenomic_end();

        if (first_exon_extension > 0) {
            CRef<CSpliced_exon> exon(new CSpliced_exon);
            exon->SetGenomic_start() = 0;
            exon->SetGenomic_end() = first_exon_extension - 1;
            spliced_seg.SetExons().push_front(exon);

            SExon exon_struct;
            exon_struct.prod_from = exons.front().prod_from - first_exon_extension;
            exon_struct.prod_to = exons.front().prod_from - 1;
            exon_struct.genomic_from = exons.front().genomic_from - first_exon_extension;
            exon_struct.genomic_to = exons.front().genomic_from - 1;

            exons.insert(exons.begin(), exon_struct);
        }
    }

    if (range_left != exons.front().genomic_from || range_right != exons.back().genomic_to) {
        NCBI_THROW(CException, eUnknown,
                   "AdjustAlignment(): "
                   "result's ends do not match the range. This is a bug in AdjustAlignment implementation");
    }

    int offset = is_protein_align ? int(exons.front().prod_from/3)*3 : exons.front().prod_from;
    if (offset > exons.front().prod_from) // negative division rounds toward zero
        offset -= 3;

    vector<SExon>::iterator exon_struct_it = exons.begin();

    if (is_protein_align) {
        NON_CONST_ITERATE (CSpliced_seg::TExons, exon_it, spliced_seg.SetExons()) {
            CSpliced_exon& exon = **exon_it;
            SetProtpos(exon.SetProduct_start(), exon_struct_it->prod_from - offset);
            SetProtpos(exon.SetProduct_end(), exon_struct_it->prod_to - offset);
            ++exon_struct_it;
        }
        spliced_seg.SetProduct_length((exons.back().prod_to - offset + 3)/3);
    } else {
        NON_CONST_ITERATE (CSpliced_seg::TExons, exon_it, spliced_seg.SetExons()) {
            CSpliced_exon& exon = **exon_it;
            exon.SetProduct_start().SetNucpos() = exon_struct_it->prod_from - offset;
            exon.SetProduct_end().SetNucpos() = exon_struct_it->prod_to - offset;
            ++exon_struct_it;
        }
        spliced_seg.SetProduct_length(exons.back().prod_to - offset + 1);
    }

    if (cross_the_origin) {
        NON_CONST_ITERATE(CSpliced_seg::TExons, exon_it, spliced_seg.SetExons()) {
            CSpliced_exon& exon = **exon_it;
            if (exon.GetGenomic_start() >= genomic_size)
                exon.SetGenomic_start() -= genomic_size;
            if (exon.GetGenomic_end() >= genomic_size)
                exon.SetGenomic_end() -= genomic_size;
        }
    }
    return align;
}

CMappedFeat GetCdsOnMrna(const objects::CSeq_id& rna_id, CScope& scope)
{
    CMappedFeat cdregion_feat;
    CBioseq_Handle handle = scope.GetBioseqHandle(rna_id);
    if (handle) {
        CFeat_CI feat_iter(handle, CSeqFeatData::eSubtype_cdregion);
        if (feat_iter  &&  feat_iter.GetSize()) {
            cdregion_feat = *feat_iter;
            const CSeq_loc& cds_loc = cdregion_feat.GetLocation();
            const CSeq_id* cds_loc_seq_id  = cds_loc.GetId();
            if (cds_loc_seq_id == NULL || !sequence::IsSameBioseq(*cds_loc_seq_id, rna_id, &scope)) {
                cdregion_feat = CMappedFeat();
            }
        }
    }
    return cdregion_feat;
}

TSignedSeqRange CFeatureGenerator::SImplementation::GetCds(const objects::CSeq_id& rna_id)
{
    CMappedFeat cdregion = GetCdsOnMrna(rna_id, *m_scope);
    if (!cdregion) {
        return TSignedSeqRange();
    }

    TSeqRange cds = cdregion.GetLocation().GetTotalRange();

    return TSignedSeqRange(cds.GetFrom(), cds.GetTo());
}

void CFeatureGenerator::SImplementation::TrimLeftExon(int trim_amount, ETrimSide side,
                                                      vector<SExon>::reverse_iterator left_edge,
                                                      vector<SExon>::reverse_iterator& exon_it,
                                                      CSpliced_seg::TExons::reverse_iterator& spl_exon_it,
                                                      ENa_strand product_strand,
                                                      ENa_strand genomic_strand)
{
    bool is_protein = (*spl_exon_it)->GetProduct_start().IsProtpos();

    while (trim_amount > 0) {
        int exon_len = side==eTrimProduct
            ? (exon_it->prod_to - exon_it->prod_from + 1)
            : (exon_it->genomic_to - exon_it->genomic_from + 1);
        if (exon_len <= trim_amount) {
            ++exon_it;
            ++spl_exon_it;
            trim_amount -= exon_len;
            if (exon_it == left_edge)
                break;
        } else {
            (*spl_exon_it)->SetPartial(true);
            (*spl_exon_it)->ResetDonor_after_exon();

            int genomic_trim_amount = 0;
            int product_trim_amount = 0;

            if ((*spl_exon_it)->CanGetParts() && !(*spl_exon_it)->GetParts().empty()) {
                CSpliced_exon::TParts& parts = (*spl_exon_it)->SetParts();
                CSpliced_exon_Base::TParts::iterator chunk = parts.end();
                while (--chunk, (trim_amount>0 ||
                                 (side==eTrimProduct
                                  ? (*chunk)->IsGenomic_ins()
                                  : (*chunk)->IsProduct_ins()))) {
                    int product_chunk_len = 0;
                    int genomic_chunk_len = 0;
                    switch((*chunk)->Which()) {
                    case CSpliced_exon_chunk::e_Match:
                        product_chunk_len = (*chunk)->GetMatch();
                        genomic_chunk_len = product_chunk_len;
                        if (product_chunk_len > trim_amount) {
                            (*chunk)->SetMatch(product_chunk_len - trim_amount);
                        }
                        break;
                    case CSpliced_exon_chunk::e_Mismatch:
                        product_chunk_len = (*chunk)->GetMismatch();
                        genomic_chunk_len = product_chunk_len;
                        if (product_chunk_len > trim_amount) {
                            (*chunk)->SetMismatch(product_chunk_len - trim_amount);
                        }
                        break;
                    case CSpliced_exon_chunk::e_Diag:
                        product_chunk_len = (*chunk)->GetDiag();
                        genomic_chunk_len = product_chunk_len;
                        if (product_chunk_len > trim_amount) {
                            (*chunk)->SetDiag(product_chunk_len - trim_amount);
                        }
                        break;
                        
                    case CSpliced_exon_chunk::e_Product_ins:
                        product_chunk_len = (*chunk)->GetProduct_ins();
                        if (side==eTrimProduct && product_chunk_len > trim_amount) {
                            (*chunk)->SetProduct_ins(product_chunk_len - trim_amount);
                        }
                        break;
                    case CSpliced_exon_chunk::e_Genomic_ins:
                        genomic_chunk_len = (*chunk)->GetGenomic_ins();
                        if (side==eTrimGenomic && genomic_chunk_len > trim_amount) {
                            (*chunk)->SetGenomic_ins(genomic_chunk_len - trim_amount);
                        }
                        break;
                    default:
                        _ASSERT(false);
                        break;
                    }
                    
                    if (side==eTrimProduct && product_chunk_len <= trim_amount) {
                        genomic_trim_amount += genomic_chunk_len;
                        product_trim_amount += product_chunk_len;
                        trim_amount -= product_chunk_len;
                    } else if (side==eTrimGenomic && genomic_chunk_len <= trim_amount) {
                        genomic_trim_amount += genomic_chunk_len;
                        product_trim_amount += product_chunk_len;
                        trim_amount -= genomic_chunk_len;
                    } else {
                        genomic_trim_amount += min(trim_amount, genomic_chunk_len);
                        product_trim_amount += min(trim_amount, product_chunk_len);
                        trim_amount = 0;
                        break;
                    }
                    chunk = parts.erase(chunk);
                }
                
            } else {
                genomic_trim_amount += trim_amount;
                product_trim_amount += trim_amount;
                trim_amount = 0;
            }
            
            exon_it->prod_to -= product_trim_amount;
            exon_it->genomic_to -= genomic_trim_amount;

            if (is_protein) {
                CProduct_pos& prot_pos = (*spl_exon_it)->SetProduct_end();
                SetProtpos(prot_pos, exon_it->prod_to);
            } else {
                if (product_strand != eNa_strand_minus) {
                    (*spl_exon_it)->SetProduct_end().SetNucpos() -= product_trim_amount;
                } else {
                    (*spl_exon_it)->SetProduct_start().SetNucpos() += product_trim_amount;
                }
            }

            if (genomic_strand != eNa_strand_minus) {
                (*spl_exon_it)->SetGenomic_end() -= genomic_trim_amount;
            } else {
                (*spl_exon_it)->SetGenomic_start() += genomic_trim_amount;
            }
        }
    }
}
void CFeatureGenerator::SImplementation::TrimRightExon(int trim_amount, ETrimSide side,
                                                       vector<SExon>::iterator& exon_it,
                                                       vector<SExon>::iterator right_edge,
                                                       CSpliced_seg::TExons::iterator& spl_exon_it,
                                                       ENa_strand product_strand,
                                                       ENa_strand genomic_strand)
{
    bool is_protein = (*spl_exon_it)->GetProduct_start().IsProtpos();

    while (trim_amount > 0) {
        int exon_len = side==eTrimProduct
            ? (exon_it->prod_to - exon_it->prod_from + 1)
            : (exon_it->genomic_to - exon_it->genomic_from + 1);
        if (exon_len <= trim_amount) {
            ++exon_it;
            ++spl_exon_it;
            trim_amount -= exon_len;
            if (exon_it == right_edge)
                break;
        } else {
            (*spl_exon_it)->SetPartial(true);
            (*spl_exon_it)->ResetAcceptor_before_exon();

            int genomic_trim_amount = 0;
            int product_trim_amount = 0;

            if ((*spl_exon_it)->CanGetParts() && !(*spl_exon_it)->GetParts().empty()) {
                CSpliced_exon::TParts& parts = (*spl_exon_it)->SetParts();
                CSpliced_exon_Base::TParts::iterator chunk = parts.begin();
                for (; trim_amount>0 ||
                         (side==eTrimProduct
                          ? (*chunk)->IsGenomic_ins()
                          : (*chunk)->IsProduct_ins());
                     ) {
                    int product_chunk_len = 0;
                    int genomic_chunk_len = 0;
                    switch((*chunk)->Which()) {
                    case CSpliced_exon_chunk::e_Match:
                        product_chunk_len = (*chunk)->GetMatch();
                        genomic_chunk_len = product_chunk_len;
                        if (product_chunk_len > trim_amount) {
                            (*chunk)->SetMatch(product_chunk_len - trim_amount);
                        }
                        break;
                    case CSpliced_exon_chunk::e_Mismatch:
                        product_chunk_len = (*chunk)->GetMismatch();
                        genomic_chunk_len = product_chunk_len;
                        if (product_chunk_len > trim_amount) {
                            (*chunk)->SetMismatch(product_chunk_len - trim_amount);
                        }
                        break;
                    case CSpliced_exon_chunk::e_Diag:
                        product_chunk_len = (*chunk)->GetDiag();
                        genomic_chunk_len = product_chunk_len;
                        if (product_chunk_len > trim_amount) {
                            (*chunk)->SetDiag(product_chunk_len - trim_amount);
                        }
                        break;
                        
                    case CSpliced_exon_chunk::e_Product_ins:
                        product_chunk_len = (*chunk)->GetProduct_ins();
                        if (side==eTrimProduct && product_chunk_len > trim_amount) {
                            (*chunk)->SetProduct_ins(product_chunk_len - trim_amount);
                        }
                        break;
                    case CSpliced_exon_chunk::e_Genomic_ins:
                        genomic_chunk_len = (*chunk)->GetGenomic_ins();
                        if (side==eTrimGenomic && genomic_chunk_len > trim_amount) {
                            (*chunk)->SetGenomic_ins(genomic_chunk_len - trim_amount);
                        }
                        break;
                    default:
                        _ASSERT(false);
                        break;
                    }
                    
                    if (side==eTrimProduct && product_chunk_len <= trim_amount) {
                        genomic_trim_amount += genomic_chunk_len;
                        product_trim_amount += product_chunk_len;
                        trim_amount -= product_chunk_len;
                    } else if (side==eTrimGenomic && genomic_chunk_len <= trim_amount) {
                        genomic_trim_amount += genomic_chunk_len;
                        product_trim_amount += product_chunk_len;
                        trim_amount -= genomic_chunk_len;
                    } else {
                        genomic_trim_amount += min(trim_amount, genomic_chunk_len);
                        product_trim_amount += min(trim_amount, product_chunk_len);
                        trim_amount = 0;
                        break;
                    }
                    chunk = parts.erase(chunk);
                }
                
            } else {
                genomic_trim_amount += trim_amount;
                product_trim_amount += trim_amount;
                trim_amount = 0;
            }
            
            exon_it->prod_from += product_trim_amount;
            exon_it->genomic_from += genomic_trim_amount;

            if (is_protein) {
                CProduct_pos& prot_pos = (*spl_exon_it)->SetProduct_start();
                SetProtpos(prot_pos, exon_it->prod_from);
            } else {
                if (product_strand != eNa_strand_minus) {
                    (*spl_exon_it)->SetProduct_start().SetNucpos() += product_trim_amount;
                } else {
                    (*spl_exon_it)->SetProduct_end().SetNucpos() -= product_trim_amount;
                }
            }

            if (genomic_strand != eNa_strand_minus) {
                (*spl_exon_it)->SetGenomic_start() += genomic_trim_amount;
            } else {
                (*spl_exon_it)->SetGenomic_end() -= genomic_trim_amount;
            }
        }
    }
}
END_NCBI_SCOPE
