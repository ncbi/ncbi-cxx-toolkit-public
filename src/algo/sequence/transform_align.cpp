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

BEGIN_NCBI_SCOPE

USING_SCOPE(objects);

BEGIN_SCOPE()

/// transform all positions to nucleotide, plus strand
void StitchSmallHoles(CSeq_align& align, CScope& scope);
void TrimHolesToCodons(CSeq_align& align, CScope& scope);


END_SCOPE()

//////////////////////////////////////////////////////////////////////////////
///

CConstRef<CSeq_align> CGeneModel::TrimAlignment(const CSeq_align& align_in,
                                                CScope& scope)
{
    if (!align_in.CanGetSegs() || !align_in.GetSegs().IsSpliced())
        return CConstRef<CSeq_align>(&align_in);

    CRef<CSeq_align> align(new CSeq_align);
    align->Assign(align_in);

    StitchSmallHoles(*align, scope);
    TrimHolesToCodons(*align, scope);

    return align;
}

BEGIN_SCOPE()

struct SExon {
    TSignedSeqPos prod_from;
    TSignedSeqPos prod_to;
    TSignedSeqPos genomic_from;
    TSignedSeqPos genomic_to;
};

void GetExonStructure(const CSpliced_seg& spliced_seg, vector<SExon>& exons)
{
    bool is_protein = (spliced_seg.GetProduct_type()==CSpliced_seg::eProduct_type_protein);

    ENa_strand product_strand = spliced_seg.IsSetProduct_strand() ? spliced_seg.GetProduct_strand() : eNa_strand_unknown;
    ENa_strand genomic_strand = spliced_seg.IsSetGenomic_strand() ? spliced_seg.GetGenomic_strand() : eNa_strand_unknown;

    exons.resize(spliced_seg.GetExons().size());
    int i = 0;
    ITERATE(CSpliced_seg::TExons, it, spliced_seg.GetExons()) {
        const CSpliced_exon& exon = **it;
        SExon& exon_struct = exons[i++];

        const CProduct_pos& prod_from = exon.GetProduct_start();
        const CProduct_pos& prod_to = exon.GetProduct_end();

        if (is_protein) {
            const CProt_pos& prot_from = prod_from.GetProtpos();
            exon_struct.prod_from = prot_from.GetAmin()*3;
            if (prot_from.IsSetFrame())
                exon_struct.prod_from += prot_from.GetFrame()-1;

            const CProt_pos& prot_to = prod_to.GetProtpos();
            exon_struct.prod_to = prot_to.GetAmin()*3;
                if (prot_to.IsSetFrame())
                exon_struct.prod_to += prot_to.GetFrame()-1;
        } else {
            exon_struct.prod_from = prod_from.GetNucpos();
            exon_struct.prod_to = prod_to.GetNucpos();
            if (product_strand == eNa_strand_minus) {
                swap(exon_struct.prod_from, exon_struct.prod_to);
                exon_struct.prod_from = -exon_struct.prod_from;
                exon_struct.prod_to = -exon_struct.prod_to;
            }
        }

        exon_struct.genomic_from = exon.GetGenomic_start();
        exon_struct.genomic_to = exon.GetGenomic_end();
        
        if (genomic_strand == eNa_strand_minus) {
            swap(exon_struct.genomic_from, exon_struct.genomic_to);
            exon_struct.genomic_from = -exon_struct.genomic_from;
            exon_struct.genomic_to = -exon_struct.genomic_to;
        }
    }
}

void StitchSmallHoles(CSeq_align& align, CScope& scope)
{
    CSpliced_seg& spliced_seg = align.SetSegs().SetSpliced();

    if (!spliced_seg.CanGetExons() || spliced_seg.GetExons().size() < 2)
        return;

    vector<SExon> exons;
    GetExonStructure(spliced_seg, exons);

    bool is_protein = (spliced_seg.GetProduct_type()==CSpliced_seg::eProduct_type_protein);

    ENa_strand product_strand = spliced_seg.IsSetProduct_strand() ? spliced_seg.GetProduct_strand() : eNa_strand_unknown;
    ENa_strand genomic_strand = spliced_seg.IsSetGenomic_strand() ? spliced_seg.GetGenomic_strand() : eNa_strand_unknown;

    int product_min_pos;
    int product_max_pos;
    if (product_strand != eNa_strand_minus) {
        product_min_pos = 0;
        if (spliced_seg.IsSetPoly_a()) {
            product_max_pos = spliced_seg.GetPoly_a()-1;
        } if (spliced_seg.IsSetProduct_length()) {
            product_max_pos = spliced_seg.GetProduct_length()-1;
            if (is_protein)
                product_max_pos *= 3;
        } else {
            product_max_pos = exons.back().prod_to;
        }
    } else {
        if (spliced_seg.IsSetProduct_length()) {
            product_min_pos = -spliced_seg.GetProduct_length()+1;
            if (is_protein)
                product_min_pos = product_min_pos*3-2;
        } else {
            product_min_pos = exons[0].prod_from;
        }
        if (spliced_seg.IsSetPoly_a()) {
            product_max_pos = -spliced_seg.GetPoly_a()-1;
        } else {
            product_max_pos = 0;
        }
    }

    CSpliced_seg::TExons::iterator it = spliced_seg.SetExons().begin();
    CRef<CSpliced_exon> prev_exon = *it;
    int i = 1;
    for (++it; it != spliced_seg.SetExons().end();  ++i, prev_exon = *it++) {
        CSpliced_exon& exon = **it;

        bool donor_set = prev_exon->IsSetDonor_after_exon();
        bool acceptor_set = exon.IsSetAcceptor_before_exon();

        if(donor_set && acceptor_set && exons[i-1].prod_to + 1 == exons[i].prod_from) {
            continue;
        }

        int prod_hole_len = exons[i].prod_from - exons[i-1].prod_to -1;
        int genomic_hole_len = exons[i].genomic_from - exons[i-1].genomic_to -1;

        if (prod_hole_len >= MIN_INTRON || genomic_hole_len >= MIN_INTRON)
            continue;

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

        if (!prev_exon->IsSetParts() || prev_exon->GetParts().empty()) {
            CRef< CSpliced_exon_chunk > part(new CSpliced_exon_chunk);
            part->SetMatch(exons[i-1].prod_to-exons[i-1].prod_from+1);
            prev_exon->SetParts().push_back(part);
        }

        if (prod_hole_len == genomic_hole_len) {
            CRef< CSpliced_exon_chunk > part(new CSpliced_exon_chunk);
            part->SetMismatch(prod_hole_len);
            prev_exon->SetParts().push_back(part);
        } else {
            int max_hole_len = max(prod_hole_len, genomic_hole_len);
            int min_hole_len = min(prod_hole_len, genomic_hole_len);
            if (min_hole_len > 1) {
                CRef< CSpliced_exon_chunk > part(new CSpliced_exon_chunk);
                part->SetMismatch(min_hole_len/2);
                prev_exon->SetParts().push_back(part);
            }
            {
                CRef< CSpliced_exon_chunk > part(new CSpliced_exon_chunk);
                if (prod_hole_len < genomic_hole_len) {
                    part->SetGenomic_ins(max_hole_len - min_hole_len);
                } else {
                    part->SetProduct_ins(max_hole_len - min_hole_len);
                }
                prev_exon->SetParts().push_back(part);
            }
            if (min_hole_len > 0) {
                CRef< CSpliced_exon_chunk > part(new CSpliced_exon_chunk);
                part->SetMismatch(min_hole_len-min_hole_len/2);
                prev_exon->SetParts().push_back(part);
            }
        }
        if (!exon.IsSetParts() || exon.GetParts().empty()) {
            CRef< CSpliced_exon_chunk > part(new CSpliced_exon_chunk);
            part->SetMatch(exons[i].prod_to-exons[i].prod_from+1);
            prev_exon->SetParts().push_back(part);
        } else {
            prev_exon->SetParts().splice(prev_exon->SetParts().end(), exon.SetParts());
        }

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
                             (product_min_pos < exons[i-1].prod_from  && !prev_exon->IsSetAcceptor_before_exon()) ||
                             (exons[i].prod_to < product_max_pos  && !prev_exon->IsSetDonor_after_exon()));

        if (exon.IsSetExt()) {
            prev_exon->SetExt().splice(prev_exon->SetExt().end(), exon.SetExt());
        }

        CSpliced_seg::TExons::iterator save_it = it;
        --save_it;
        spliced_seg.SetExons().erase(it);
        it = save_it;

    }
}

void TrimHolesToCodons(CSeq_align& align, CScope& scope)
{
}

END_SCOPE()

END_NCBI_SCOPE
