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
 * Author:  Vlad Lebedev, Victor Joukov
 *
 * File Description: Library to retrieve mapped sequence coordinates of
 *                   the feature products given a sequence location
 *
 *
 */

#include <ncbi_pch.hpp>

#include <misc/hgvs/objcoords.hpp>

#include <misc/hgvs/sequtils.hpp>

#include <util/checksum.hpp>

#include <util/sequtil/sequtil.hpp>
#include <util/sequtil/sequtil_manip.hpp>

#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/seq_loc_mapper.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/annot_selector.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/feature.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_exon_chunk.hpp>

#include <math.h>


USING_SCOPE(ncbi);
USING_SCOPE(objects);


// size of sequence frame
static const TSeqPos kSeqFrame = 10;

// Service functions
//static string x_GetSequence(CSeqVector& seq_vec, TSignedSeqPos pos, bool complement=false);
static string x_GetSequence(CSeqVector& seq_vec, TSignedSeqPos pos, TSignedSeqPos adjustment=0);

static SAnnotSelector x_GetAnnotSelector();


/////////////////////////////////////////////////////////////////////////////
//  CReportEntry
//  Container for all information related to one logical entry - mRNA/CDS
//  combination or component.

class CReportEntry : public CObject
{
public:
    CReportEntry(CScope& scope, const CSeq_id& seq_id,
        feature::CFeatTree& feat_tree, const CMappedFeat& root_feat);
    CReportEntry(CScope& scope, const CSeq_id& seq_id, const CMappedFeat& gene,
        feature::CFeatTree& feat_tree, const CMappedFeat& root_feat);
    CReportEntry(CScope& scope, const CSeq_id& seq_id, const CSeq_align& align);
    CReportEntry() : m_cdr_length(0), m_cds_offset_set(false) {}
    void GetCoordinates(CScope& scope,
                        Uint4 type_flags,
                        TSeqPos pos,
                        CHGVS_Coordinate_Set& coords) const;
    bool IsValid() { return m_mrna || m_cds; }

    // Specific to feature/alignment based mRNA/CDS group
    void SetGene(CScope& scope, const CSeq_feat& feat);
    void SetMrna(CScope& scope, const CSeq_feat& feat);
    void SetCds(CScope& scope, const CSeq_feat& feat);
    void SetAlignment(CScope& scope, const CSeq_align& align);
    // ??? is it specific, or can be made generic
    CConstRef<CSeq_id> GetTranscriptId() { return m_transcript_id; }

private:
    CConstRef<CSeq_feat>  m_mrna; /// mRNA feature on main sequence (genomic or transcript)
    CConstRef<CSeq_feat>  m_cds;  /// CDS feature on main sequence
    CConstRef<CSeq_feat>  m_rna_cds; /// CDS feature on transcript, where main is genomic
    TSeqPos               m_cdr_length;
    bool                  m_cds_offset_set;
    TSeqPos               m_cds_offset;
    bool                  m_mrna_plus_strand;
    CConstRef<CSeq_align> m_mrna_align;
    CRef<CSeq_loc_Mapper> m_mrna_mapper;
    // Some features occur on many kinds of sequences, so
    // we have an alias m_main_id for such cases
    CConstRef<CSeq_id>    m_main_id;
    CConstRef<CSeq_id>    m_genomic_id;
    CConstRef<CSeq_id>    m_transcript_id;
    CConstRef<CSeq_id>    m_prot_id;
    string                m_locus;

    void x_SetId(CScope& scope, const CSeq_id& seq_id);
    void x_SetFeature(CScope& scope, feature::CFeatTree& feat_tree, const CMappedFeat& feat);
    void x_SetRnaCds(CScope& scope, const CSeq_feat& cds);
    void x_SetMrnaMapper(CScope& scope, const CSeq_feat& feat);

    CRef<CSeq_loc_Mapper> x_GetCdsMapper(CScope& scope, const CSeq_feat& cds) const;
    TSignedSeqPos x_GetAdjustment(TSeqPos pos) const;
    bool x_CheckExonGap(TSeqPos pos) const;
    TSeqPos x_GetCDSOffset() const;
    CRef<CHGVS_Coordinate> x_NewCoordinate(CScope& scope, const CSeq_id* id, TSeqPos pos) const;
    void x_SetSequence(CHGVS_Coordinate& ref, CScope& scope, const CSeq_id* id,
        TSignedSeqPos pos, TSignedSeqPos adjustment=0, bool plus_strand=true) const;
    void x_SetHgvs(CHGVS_Coordinate& ref, const char* prefix, TSignedSeqPos pos,
                   TSignedSeqPos adjustment = 0, bool utr3_tail = false,
                   bool in_gap = false) const;
    bool x_MapPos(const CSeq_loc_Mapper& mapper,
                  const CSeq_id& id, TSeqPos pos,
                  TSeqPos& mapped_pos) const;
    void x_CalculateUTRAdjustments(TSignedSeqPos& feature_pos,
                                   TSignedSeqPos& seq_pos,
                                   TSignedSeqPos& adjustment,
                                   bool& utr3_tail) const;
    void x_GetRCoordinate(CScope& scope, TSeqPos pos,
                          CHGVS_Coordinate_Set& coords) const;
    void x_GetCCoordinate(CScope& scope, TSeqPos pos, TSignedSeqPos adj,
                          CHGVS_Coordinate_Set& coords) const;
    void x_GetPCoordinate(CScope& scope, TSeqPos pos,
                          CHGVS_Coordinate_Set& coords) const;
};



CReportEntry::CReportEntry(CScope& scope, const CSeq_id& seq_id,
                           feature::CFeatTree& feat_tree, const CMappedFeat& root_feat)
    : m_cdr_length(0), m_cds_offset_set(false)
{
    x_SetId(scope, seq_id);
    x_SetFeature(scope, feat_tree, root_feat);
}


CReportEntry::CReportEntry(CScope& scope, const CSeq_id& seq_id, const CMappedFeat& gene,
                           feature::CFeatTree& feat_tree, const CMappedFeat& root_feat)
    : m_cdr_length(0), m_cds_offset_set(false)
{
    x_SetId(scope, seq_id);
    SetGene(scope, gene.GetMappedFeature());
    x_SetFeature(scope, feat_tree, root_feat);
}


CReportEntry::CReportEntry(CScope& scope, const CSeq_id& seq_id, const CSeq_align& align)
{
    x_SetId(scope, seq_id);
    SetAlignment(scope, align);
}


void CReportEntry::x_SetId(CScope& scope, const CSeq_id& seq_id)
{
    m_main_id.Reset(&seq_id);
    Uint4 type_flags = GetSequenceType(scope.GetBioseqHandle(seq_id));
    /// check for protein
    if (type_flags & CSeq_id::fAcc_prot) {
        m_prot_id.Reset(&seq_id);
    }
    /// check for mRNA
    else if (type_flags & CSeq_id::eAcc_mrna) {
        m_transcript_id.Reset(&seq_id);
    }
    /// check for genomic
    else if (type_flags & CSeq_id::fAcc_genomic) {
        m_genomic_id.Reset(&seq_id);
    }
}


void CReportEntry::x_SetFeature(CScope& scope, feature::CFeatTree& feat_tree, const CMappedFeat& feat)
{
    const CSeq_feat& seq_feat = feat.GetMappedFeature();
    CSeqFeatData::ESubtype subtype =
        seq_feat.GetData().GetSubtype();

    switch (subtype) {
    // The following case is useless, handled in different manner in calling code
    case CSeqFeatData::eSubtype_gene:
        SetGene(scope, seq_feat);
        {{
            vector<CMappedFeat> cc = feat_tree.GetChildren(feat);
            ITERATE (vector<CMappedFeat>, iter, cc) {
                x_SetFeature(scope, feat_tree, *iter);
            }
        }}
        break;
    case CSeqFeatData::eSubtype_mRNA:
        SetMrna(scope, seq_feat);
        {{
            vector<CMappedFeat> cc = feat_tree.GetChildren(feat);
            // TODO: We don't handle multiple CDS per mRNA. Do they actually happen?
            ITERATE (vector<CMappedFeat>, iter, cc) {
                const CSeq_feat& seq_feat = iter->GetMappedFeature();
                if (seq_feat.GetData().GetSubtype() ==
                    CSeqFeatData::eSubtype_cdregion)
                {
                    SetCds(scope, seq_feat);
                    break;
                }
            }
        }}
        break;
    case CSeqFeatData::eSubtype_cdregion:
        SetCds(scope, seq_feat);
        break;
    default:
        break;
    }
}


void CReportEntry::GetCoordinates(CScope& scope,
                                  Uint4 type_flags,
                                  TSeqPos pos,
                                  CHGVS_Coordinate_Set& coords) const
{
    // Fill out g. r. c. and p. coordinates based on the entry and type of
    // original sequence
    if (type_flags & CSeq_id::fAcc_prot) {
        // do nothing
        return;
    }
    // TODO: handle component here in the future

    // Calculate adjustment to the nearest exon, 0 if we are in exon
    TSignedSeqPos adjustment = x_GetAdjustment(pos);
    // check for genomic
    if (type_flags & CSeq_id::fAcc_genomic) {
        // r. c. and p. - r. here, c. and p. same as for mRNA
        if (!adjustment)
            x_GetRCoordinate(scope, pos, coords);
    }
    // check for mRNA, effectively union of cases with genomic
    if (type_flags & (CSeq_id::fAcc_genomic | CSeq_id::eAcc_mrna)) {
        // c. and p.
        x_GetCCoordinate(scope, pos, adjustment, coords);
        if (!adjustment)
            x_GetPCoordinate(scope, pos, coords);
    }
}


TSignedSeqPos CReportEntry::x_GetAdjustment(TSeqPos pos) const
{
    // Calculate minimal distance to the exon boundary
    if (!m_mrna_mapper) return 0;
    //bool plus_strand = true;
    if (m_mrna_align) {
        // TODO: Use alignment, take into account exons
        // const CSpliced_seg& seg = m_mrna_align->GetSegs().GetSpliced();
        // ??? what should we do if we're in exon gap? How to handle exon overlap
        // (ambiguous positions in an exon)
    }
    TSignedSeqPos signed_pos = static_cast<TSignedSeqPos>(pos);
    TSignedSeqPos adj = 0;
    const CMappingRanges& ranges = m_mrna_mapper->GetMappingRanges();
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*m_main_id);
    CMappingRanges::TRangeIterator rg_it = ranges.BeginMappingRanges(
                 idh,
                 CMappingRanges::TRange::GetWhole().GetFrom(),
                 CMappingRanges::TRange::GetWhole().GetTo());
    for ( ; rg_it; ++rg_it) {
        const CMappingRange& cvt = *rg_it->second;
        TSignedSeqPos f = cvt.GetSrc_from();
        TSignedSeqPos t = f + cvt.GetLength();
        // TODO: Is it reliable enough to test for mRNA strand?
        // plus_strand = !cvt.GetReverse();
//        LOG_POST("Pos " << pos << " from " << f << " to " << t);
        int adj_for_intron;
        if (signed_pos < f) {
            adj_for_intron = signed_pos - f;
        } else if (signed_pos > t) {
            adj_for_intron = signed_pos - t;
        } else {
            return 0; // no adjustment needed
        }
        if (adj == 0  ||  std::abs(adj_for_intron) < std::abs(adj)) {
            adj = adj_for_intron;
        }
    }
//    LOG_POST("Adjustment calculated " << adj);
    return m_mrna_plus_strand ? adj : -adj;
}


bool CReportEntry::x_CheckExonGap(TSeqPos pos) const
{
    if (!m_mrna_align) return false;
    // Use alignment, take into account exons
    const CSpliced_seg& seg = m_mrna_align->GetSegs().GetSpliced();
    ITERATE (CSpliced_seg::TExons, exon_it, seg.GetExons()) {
        const CSpliced_exon& exon = **exon_it;
        if (!exon.IsSetGenomic_start() || pos < exon.GetGenomic_start() ||
            !exon.IsSetGenomic_end() || pos > exon.GetGenomic_end() ||
            !exon.IsSetParts()
        ) {
            continue;
        }
        TSeqPos exon_pos = m_mrna_plus_strand ?
                               pos - exon.GetGenomic_start() :
                               exon.GetGenomic_end() - pos;
        ITERATE (CSpliced_exon::TParts, part_it, exon.GetParts()) {
            TSeqPos l = 0;
            bool in_gap = false;
            const CSpliced_exon_chunk& part = **part_it;
            if (part.IsMatch()) {
                l = part.GetMatch();
            } else if (part.IsMismatch()) {
                l = part.GetMismatch();
            } else if (part.IsDiag()) {
                l = part.GetDiag();
            } else if (part.IsProduct_ins()) {
                l = 0; // redundant
            } else if (part.IsGenomic_ins()) {
                l = part.GetGenomic_ins();
                in_gap = true;
            }
            if (l > exon_pos) return in_gap;
            exon_pos -= l;
        }
    }
    return false;
}


TSeqPos CReportEntry::x_GetCDSOffset() const
{
    return m_cds_offset_set ? m_cds_offset : 0;
}


CRef<CHGVS_Coordinate> CReportEntry::x_NewCoordinate(CScope& scope, const CSeq_id* id, TSeqPos pos) const
{
    // get the title (best id)
    string title;
    if (id) {
        CSeq_id_Handle idh = sequence::GetId(*id, scope,
                                             sequence::eGetId_Best);
        idh.GetSeqId()->GetLabel(&title, CSeq_id::eContent);
    } else if (!m_locus.empty()) {
        title = m_locus;
    }
    CRef<CHGVS_Coordinate> ref(new CHGVS_Coordinate());
    ref->SetTitle(title);
    ref->SetObject_id("");
    ref->SetMarker_pos(pos);
    ref->SetSequence("");
    ref->SetHgvs_position("");
    return ref;
}


void CReportEntry::x_SetSequence(CHGVS_Coordinate& ref, CScope& scope, const CSeq_id* id,
                                 TSignedSeqPos pos, TSignedSeqPos adjustment, bool plus_strand) const
{
    if (!id) return;
    CBioseq_Handle prod_bsh =
        scope.GetBioseqHandle(*id);
    CSeqVector prod_vec(prod_bsh, CBioseq_Handle::eCoding_Iupac,
        plus_strand ? eNa_strand_plus : eNa_strand_minus);
    ref.SetSequence( x_GetSequence(prod_vec, plus_strand ? pos : prod_vec.size() - pos - 1, adjustment) );
}


void CReportEntry::x_SetHgvs(CHGVS_Coordinate& ref, const char* prefix, TSignedSeqPos pos,
                             TSignedSeqPos adjustment, bool utr3_tail,
                             bool in_gap) const
{
    string hgvs(prefix);
    if (in_gap) {
        TSignedSeqPos second_pos;
        if (adjustment > 0) {
            second_pos = pos + 1;
        } else {
            second_pos = pos;
            pos -= 1;
        }
        hgvs += "(" + NStr::IntToString(pos+1) + "_" + NStr::IntToString(second_pos+1) + ")";
        ref.SetHgvs_position(hgvs);
        return;
    }
    TSignedSeqPos pos_1_based =
        pos >= 0 ? pos + 1 : pos;
    if (adjustment == 0 || !utr3_tail) {
        hgvs += NStr::IntToString(pos_1_based);
    }
    if (adjustment != 0) {
        if (utr3_tail) {
            hgvs += '*';
        } else if (adjustment > 0) {
            hgvs += '+';
        }
        hgvs += NStr::IntToString(adjustment);
    }
    ref.SetHgvs_position(hgvs);
}


bool CReportEntry::x_MapPos(const CSeq_loc_Mapper& mapper,
                            const CSeq_id& id, TSeqPos pos,
                            TSeqPos& mapped_pos) const
{
    // fake location for seq_mapper
    CRef<CSeq_loc> fake_loc(new CSeq_loc());
    fake_loc->SetPnt().SetPoint(pos);
    fake_loc->SetId(id);

    // Get the feature position
    CRef<CSeq_loc> mapped_loc;
    // Due to defect in definition of Map (non-const) we need to use coercion
    mapped_loc = const_cast<CSeq_loc_Mapper*>(&mapper)->Map(fake_loc.GetObject());
    if (mapped_loc->Which() == CSeq_loc::e_Null) {
        LOG_POST(Warning << "loc mapping did not work");
        return false;
    }
    mapped_pos = mapped_loc->GetPnt().GetPoint();
    return true;
}


void CReportEntry::x_CalculateUTRAdjustments(TSignedSeqPos& feature_pos,
                                             TSignedSeqPos& seq_pos,
                                             TSignedSeqPos& adjustment,
                                             bool& utr3_tail) const
{
    utr3_tail = false;
    // To calculate whether we are in UTR 3' region we need to know if our
    // unadjusted position would map beyond the coding feature length.
    // There are two cases:
    //   If the right flank is not zero, even adjusted position would map
    // after cdr_length.
    //   The second one is worse - its adjusted pos always maps to the last
    // valid position of last exon. For this case we check it and add
    // adjustment to be used in UTR 3' test.
    TSignedSeqPos utr3_adjusted_pos = feature_pos;
    if (m_cdr_length  &&  feature_pos >= TSignedSeqPos(m_cdr_length) - 1) {
        utr3_adjusted_pos += adjustment;
    }
    // If position is out of bounds of the feature, we just add
    // adjustment so it does not matter is it in intron or not.
    // According to Alex Astashin, if the position is inside mRNA,
    // it is reported in detail, so we check seq_pos, not feature_pos
    if (seq_pos <= 0) {
        feature_pos += adjustment;
        seq_pos += adjustment;
        adjustment = 0;
    } else
    if (m_cdr_length  &&  utr3_adjusted_pos >= TSignedSeqPos(m_cdr_length)) {
        feature_pos += adjustment;
        seq_pos += adjustment;
        // Adjustment signifies the offset from the stop codon into 3' UTR
        adjustment = feature_pos - m_cdr_length + 1;
        utr3_tail = true;
        feature_pos = m_cdr_length - 1;
    }
}


void CReportEntry::x_GetRCoordinate(CScope& scope, TSeqPos pos,
                                    CHGVS_Coordinate_Set& coords) const
{
    CRef<CHGVS_Coordinate> ref(x_NewCoordinate(scope, m_transcript_id, pos));

    if (!m_mrna_mapper) {
        LOG_POST(Warning << "mRNA mapper is empty");
        return;
    }
    TSeqPos mapped_pos;
    if (x_MapPos(*m_mrna_mapper, *m_genomic_id, pos, mapped_pos)) {
        ref->SetPos_mapped(mapped_pos);
        x_SetHgvs(*ref, "r.", mapped_pos);
        if (m_transcript_id) {
            x_SetSequence(*ref, scope, m_transcript_id, mapped_pos);
        } else if (m_genomic_id) {
            // Product-less mRNA feature - we're faking it a bit using
            // original sequence and original, not mapped position and
            // appropriate strand. Same trick works for c. coordinate below
            x_SetSequence(*ref, scope, m_genomic_id, pos, 0, m_mrna_plus_strand);
        }
        coords.Set().push_back(ref);
    }
}


void CReportEntry::x_GetCCoordinate(CScope& scope, TSeqPos pos, TSignedSeqPos adjustment,
                                    CHGVS_Coordinate_Set& coords) const
{
    if (!m_cds && !m_rna_cds) return;
    CRef<CHGVS_Coordinate> ref(x_NewCoordinate(scope, m_transcript_id, pos));
    bool mapped = false;

    // adjusted_pos is for safely mapping the coordinate, it is guaranteed to be in
    // exon, and even if the exon has a gap, not in the gap.
    // adjustment is opposite the product, that is why we need to know
    // the strand of the feature to find adjustment to mappable position
    TSeqPos adjusted_pos = pos + (m_mrna_plus_strand ? -adjustment : adjustment);

    TSignedSeqPos mapped_pos;
    TSeqPos seq_pos;
    TSeqPos offset = 0;
    bool utr3_tail = false;
    bool in_gap = false;
    if (m_mrna_mapper) {
        // To use mRNA mapper to map to CDS coordinate we need to calculate
        // CDS to mRNA offset here and use it to fix mapped coordinate
        offset = x_GetCDSOffset();
        if (x_MapPos(*m_mrna_mapper, *m_main_id, adjusted_pos, seq_pos)) {
            in_gap = x_CheckExonGap(pos);
            mapped_pos = seq_pos - offset;
            TSignedSeqPos utr_adjusted_pos = seq_pos;
            x_CalculateUTRAdjustments(mapped_pos, utr_adjusted_pos, adjustment, utr3_tail);
            if (m_transcript_id) {
                x_SetSequence(*ref, scope, m_transcript_id, utr_adjusted_pos, adjustment);
            } else if (m_genomic_id) {
                // Product-less mRNA feature - see comment for r. coordinate
                x_SetSequence(*ref, scope, m_genomic_id, pos, adjustment, m_mrna_plus_strand);
            }
            mapped = true;
        }
    } else if (m_cds) { // This condition is trivially true, but stays here to denote the case
        // CDS feature only - map through fake product
        CRef<CSeq_loc> fake_product(new CSeq_loc);
        fake_product->SetWhole().Assign(*m_main_id);
        CRef<CSeq_loc_Mapper> mapper(new CSeq_loc_Mapper
                     (m_cds->GetLocation(),
                      *fake_product,
                      &scope));
        if (x_MapPos(*mapper, *m_main_id, adjusted_pos, seq_pos)) {
            mapped_pos = seq_pos;
            // Sequence coming from original id, use original pos
            x_SetSequence(*ref, scope, m_main_id, pos, adjustment);
            mapped = true;
        }
    } else {
        // Sorry, not enough info to calculate the coordinate
        return;
    }
    x_SetHgvs(*ref, "c.", mapped_pos, adjustment, utr3_tail, in_gap);

    if (!adjustment) {
        ref->SetPos_mapped(mapped_pos);
    }
    if (mapped) coords.Set().push_back(ref);
}


void CReportEntry::x_GetPCoordinate(CScope& scope, TSeqPos pos,
                                    CHGVS_Coordinate_Set& coords) const
{
    TSeqPos mapped_pos = pos;
    if (m_rna_cds  &&  m_mrna_mapper) {
        // We have both DNA to RNA and RNA to protein mappings, so map 2 times
        if (!x_MapPos(*m_mrna_mapper, *m_genomic_id, pos, mapped_pos))
            return;
        if (!x_MapPos(*x_GetCdsMapper(scope, *m_rna_cds), *m_transcript_id, mapped_pos, mapped_pos))
            return;
    } else if (m_cds) {
        if (!x_MapPos(*x_GetCdsMapper(scope, *m_cds), *m_main_id, pos, mapped_pos))
            return;
    } else {
        return;
    }
    CRef<CHGVS_Coordinate> ref(x_NewCoordinate(scope, m_prot_id, pos));
    ref->SetPos_mapped(mapped_pos);
    x_SetHgvs(*ref, "p.", mapped_pos);
    x_SetSequence(*ref, scope, m_prot_id, mapped_pos);
    coords.Set().push_back(ref);
}


void CReportEntry::SetGene(CScope& scope, const CSeq_feat& feat)
{
    if (feat.GetData().GetGene().IsSetLocus_tag()) {
        m_locus = feat.GetData().GetGene().GetLocus_tag();
    } else if (feat.GetData().GetGene().IsSetLocus()) {
        m_locus = feat.GetData().GetGene().GetLocus();
    }
    // CLabel::GetLabel(feat, &m_locus, CLabel::eContent, &scope);
}


void CReportEntry::SetMrna(CScope& scope, const CSeq_feat& feat)
{
    m_mrna.Reset(&feat);
    if (feat.IsSetProduct())
        // mRNA feature's product always points to transcript
        m_transcript_id.Reset(feat.GetProduct().GetId());
    m_mrna_plus_strand =
        sequence::GetStrand(feat.GetLocation()) != eNa_strand_minus;
    x_SetMrnaMapper(scope, feat);
}


void CReportEntry::SetCds(CScope& scope, const CSeq_feat& cds)
{
    m_cds.Reset(&cds);
//    if (!m_mrna) m_main_id.Reset(cds.GetLocation().GetId());
    m_prot_id.Reset(cds.GetProduct().GetId());
    if (!cds.GetLocation().IsPnt() && (!cds.IsSetPartial() || !cds.GetPartial())) {
        m_cdr_length = sequence::GetLength(cds.GetLocation(), &scope);
    }

    // Prefer direct calculation (see x_SetRnaCds) and check
    // that mRNA is already set
    if (m_cds_offset_set || !m_mrna) return;
    // Offset is inferred given the
    // locations of the mRNA and CDS feature
    CSeq_loc_CI loc_iter(m_mrna->GetLocation());
    TSeqPos start = cds.GetLocation().GetStart(eExtreme_Biological);
    ENa_strand strand = cds.GetLocation().GetStrand();
    TSeqRange r(start, start);
    TSeqPos frame = 0;
    if (cds.GetData().GetCdregion().IsSetFrame()) {
        frame = cds.GetData().GetCdregion().GetFrame();
        if (frame) {
            frame -= 1;
        }
    }

    TSeqPos offset_acc = 0;
    for (;  loc_iter;  ++loc_iter) {
        if (loc_iter.GetRange()
            .IntersectingWith(r)) {
            if (strand == eNa_strand_minus) {
                m_cds_offset = offset_acc +
                    loc_iter.GetRange().GetTo() - start - frame;
            } else {
                m_cds_offset = offset_acc +
                    start - loc_iter.GetRange().GetFrom() + frame;
            }
            m_cds_offset_set = true;
            break;
        }
        offset_acc += loc_iter.GetRange().GetLength();
    }
}


void CReportEntry::x_SetRnaCds(CScope& scope, const CSeq_feat& cds)
{
    m_rna_cds.Reset(&cds);
    m_prot_id.Reset(cds.GetProduct().GetId());
    if (!cds.GetLocation().IsPnt() && (!cds.IsSetPartial() || !cds.GetPartial())) {
        m_cdr_length = sequence::GetLength(cds.GetLocation(), &scope);
    }
    // Offset is determined by the
    // placement of a CDS feature
    // directly on the RNA sequence
    m_cds_offset = cds.GetLocation().GetTotalRange().GetFrom();
    if (cds.GetData().GetCdregion().IsSetFrame()) {
        TSeqPos frame = cds.GetData().GetCdregion().GetFrame();
        if (frame) {
            m_cds_offset += frame - 1;
        }
    }
    m_cds_offset_set = true;
}


void CReportEntry::x_SetMrnaMapper(CScope& scope, const CSeq_feat& feat)
{
    if (feat.IsSetProduct()) {
        m_mrna_mapper.Reset(new CSeq_loc_Mapper
                             (feat,
                              CSeq_loc_Mapper::eLocationToProduct,
                              &scope));
    } else {
        CRef<CSeq_loc> fake_product(new CSeq_loc);
        fake_product->SetWhole().Assign(*m_main_id);
        m_mrna_mapper.Reset(new CSeq_loc_Mapper
                     (feat.GetLocation(),
                      *fake_product,
                      &scope));
    }
}


void CReportEntry::SetAlignment(CScope& scope, const CSeq_align& align)
{
    // cerr << MSerial_AsnText << align;

    if (align.GetSegs().Which() != CSeq_align::TSegs::e_Spliced)
        return;

    m_main_id.Reset(&align.GetSeq_id(1));
    m_genomic_id.Reset(&align.GetSeq_id(1));
    m_transcript_id.Reset(&align.GetSeq_id(0));

    m_mrna_mapper.Reset(new CSeq_loc_Mapper
// Work-around for CXX-1555 - pass id of the sequence as a reliable way
// to indicate desirable direction of the mapping.
//      (*align, 0, scope));
        (align, align.GetSeq_id(0), &scope));
    m_mrna_align.Reset(&align);
    const CSpliced_seg& seg = m_mrna_align->GetSegs().GetSpliced();
    m_mrna_plus_strand = !(seg.IsSetGenomic_strand() && seg.GetGenomic_strand() == eNa_strand_minus);

    if (!m_cds) {
        // There is no CDS feature because this entry is derived from
        // alignment. Try to use CDS feature if any on target mRNA
        CBioseq_Handle product_bsh =
            scope.GetBioseqHandle
            (align.GetSeq_id(0));
        if (product_bsh) {
            //
            // our offset is determined by the
            // placement of a CDS feature
            // directly on the product RNA sequence
            //
//            LOG_POST("Deriving CDS mapping from alignment");
            SAnnotSelector sel  = x_GetAnnotSelector();
            sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_cdregion);
            // TODO: ??? Now we prefer LAST of CDSs for a given RNA. Can there be
            // more than one, and what is the meaning for it?
            for (CFeat_CI feat_iter(product_bsh, sel);
                 feat_iter;  ++feat_iter)
            {
                x_SetRnaCds(scope, feat_iter->GetMappedFeature());
            }
        }
    } else {
        // Recalculate CDS offset using the alignment
        // Check that first exon is not partial
        const CSpliced_exon &first_exon = *(seg.GetExons().front());
        if (!(first_exon.IsSetPartial() && first_exon.GetPartial())) {
            TSeqPos cds_start_on_dna = m_cds->GetLocation().GetStart(eExtreme_Biological);
            ENa_strand strand = m_cds->GetLocation().GetStrand();
            TSeqPos rna_start_on_dna;
            if (strand == eNa_strand_minus) {
                rna_start_on_dna = seg.GetSeqStop(1);
            } else {
                rna_start_on_dna = seg.GetSeqStart(1);
            }
            TSeqPos rna_start, cds_start;
            x_MapPos(*m_mrna_mapper, *m_main_id, rna_start_on_dna, rna_start);
            x_MapPos(*m_mrna_mapper, *m_main_id, cds_start_on_dna, cds_start);
            m_cds_offset = cds_start - rna_start;
            m_cds_offset_set = true;
        }
    }
}


CRef<CSeq_loc_Mapper> CReportEntry::x_GetCdsMapper(CScope& scope, const CSeq_feat& cds) const
{
    CRef<CSeq_loc_Mapper> cds_mapper(new CSeq_loc_Mapper
                 (cds,
                  CSeq_loc_Mapper::eLocationToProduct,
                  &scope));
    return cds_mapper;
}


typedef vector<CRef<CReportEntry> > TReportEntryList;

static
TReportEntryList s_GetFeaturesForRange(CScope& scope, const CBioseq_Handle& bsh, TSeqRange search_range);
static
void s_ExtendEntriesFromAlignments(CScope& scope, const CBioseq_Handle& bsh, TSeqRange search_range, TReportEntryList& entries);
static
bool s_IsRefSeqGene(const CBioseq_Handle& bsh);


/////////////////////////////////////////////////////////////////////////////
//  CObjectCoords::
//

CObjectCoords::CObjectCoords(CScope& scope) :
    m_Scope(&scope)
{
}


CRef<CHGVS_Coordinate_Set> CObjectCoords::GetCoordinates(const CSeq_id& id, TSeqPos pos, TSeqPos window)
{
    CRef<CHGVS_Coordinate_Set> coords(new CHGVS_Coordinate_Set);
    GetCoordinates(*coords, id, pos, window);
    return coords;
}

void
CObjectCoords::GetCoordinates(CHGVS_Coordinate_Set& coords, const CSeq_id& id, TSeqPos pos, TSeqPos window)
{
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(id);
    if ( !bsh ) {
        NCBI_THROW(CException, eUnknown,
                   "failed to retrieve sequence: " + id.AsFastaString());
    }
    CSeqVector seq_vec =
        bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);

    string title;
    CSeq_id_Handle idh = sequence::GetId(id, *m_Scope, sequence::eGetId_Best);
    idh.GetSeqId()->GetLabel(&title, CSeq_id::eContent);

    // First entry is for the gi itself
    CRef<CHGVS_Coordinate> ref(new CHGVS_Coordinate());

    // identify sequence
    Uint4 type_flags = GetSequenceType(bsh);
    string hgvs;

    /// check for protein
    if (type_flags & CSeq_id::fAcc_prot) {
        hgvs = "p.";
    }
    /// check for mRNA
    else if (type_flags & CSeq_id::eAcc_mrna) {
        hgvs = "r.";
    }
    /// check for genomic
    else if (type_flags & CSeq_id::fAcc_genomic) {
        hgvs = "g.";
    }

    // Get Main sequence
    ref->SetTitle(title);
    ref->SetHgvs_position( hgvs + NStr::IntToString(pos + 1) );
    ref->SetSequence( x_GetSequence(seq_vec, pos) );
    ref->SetMarker_pos(pos);
    ref->SetPos_mapped(pos);

    coords.Set().push_back(ref);

    // Get all RNAs and CDSs in the +-2k (default) vicinity of the
    //   position in question and build a CFeatTree.
    // Main cases are:
    //   1. Genomic sequence. Tree will contain many mRNA->CDS chains
    //      E.g. NT_011515 pos 1585115 (alignment present)
    //           NC_000007.13 pos 65338429 (no alignment)
    //           NC_000019.9 pos 45016991 - forward ribosomal slippage
    //           NC_000007.13 v 94283650:94301032 - back ribosomal slippage,
    //                        encoded through weird mRNA/CDS relation
    //           NG_005895.1 - RefSeq Gene, features through alignment only
    //   1a. mRNA can be w/o product (GenBank DNA)
    //      E.g. BX571818.3
    //   1b. No mRNA
    //      E.g. NC_000913.2 pos 2282398
    //   2. Transcript. There will be CDSs alone
    //      E.g. NM_002020
    //   2a. There can be mRNA w/o product (GenBank mRNA)
    //      E.g. M11313
    //   2b. No CDS
    //      E.g. NR_002767
    //   3. Protein sequence
    // Overall procedure is following:
    // Get all pairwise alignments for position into map by product id.
    // Sort all top-level into ReportEntries, adding alignment for mRNA
    //   products from the map (removing from map).
    // Convert all remaning alignment to ReportEntries
    // Get all components for position into GeneInfos
    // Process all ReportEntries getting r., c., and p. (g. for components)
    TSeqPos min_pos = pos >= window ? pos - window : pos;
    TSeqPos max_pos = pos + window;
    TSeqRange search_range(min_pos, max_pos);
    // Per SV-487, if we deal with RefSeq Gene record only use
    // alignments to get features
    bool is_ref_seq_gene = s_IsRefSeqGene(bsh);
    // is_ref_seq_gene = false; // DEBUG
    TReportEntryList entries;
    if (!is_ref_seq_gene) entries = s_GetFeaturesForRange(*m_Scope, bsh, search_range);
    s_ExtendEntriesFromAlignments(*m_Scope, bsh, search_range, entries);
    ///
    /// now, evaluate each feature
    /// we may need to inject an artificial mRNA feature to capture
    /// the 'c.' coordinate.
    ///
    ITERATE(TReportEntryList, iter, entries) {
        (*iter)->GetCoordinates(
            *m_Scope,
            type_flags,
            pos,
            coords);

    }
}


static
bool s_IsRefSeqGene(const CBioseq_Handle& bsh)
{
    // TODO: Use CSeq_descr_CI on bhs itself here
    bool is_ref_seq_gene = false;
    CSeq_entry_Handle seh = bsh.GetParentEntry();
    if (seh.IsSetDescr()) {
        const CSeq_descr& descr = seh.GetDescr();
        CSeq_descr::Tdata descr_list = descr.Get();
        ITERATE(CSeq_descr::Tdata, it, descr_list) {
            if (it->GetObject().IsGenbank()) {
                const CSeqdesc::TGenbank& gb = it->GetObject().GetGenbank();
                if (gb.IsSetKeywords()) {
                    ITERATE(list<string>, it1, gb.GetKeywords()) {
                        if (*it1 == "RefSeqGene") {
                            is_ref_seq_gene = true;
                            break;
                        }
                    }
                }
            }
            if (is_ref_seq_gene) break;
        }
    }
    return is_ref_seq_gene;
}


static
TReportEntryList s_GetFeaturesForRange(
    CScope& scope,
    const CBioseq_Handle& bsh,
    TSeqRange search_range)
{
    TReportEntryList entries;
    // find features on the sequence
    SAnnotSelector sel  = x_GetAnnotSelector();
    // We don't need gene record per se, only mRNA and CDS
    sel.IncludeFeatType(CSeqFeatData::e_Gene);
    sel.IncludeFeatType(CSeqFeatData::e_Rna);
    sel.IncludeFeatType(CSeqFeatData::e_Cdregion);

    CFeat_CI feat_iter(bsh, search_range, sel);
    feature::CFeatTree feat_tree;
    feat_tree.AddFeatures(feat_iter);

    /*
     Here we have following structures:
     Gene
     |\
     | mRNA
     |   \
     |    CDS
     |\
     | mRNA
     |\
     | CDS
     We convert them into list of ReportEntries which hold Gene, mRNA, and CDS
     records (possibly empty). We need to pass only hierarchy root to ReportEntry
     constructor except in case of Gene because Gene can have multiple distinct
     mRNA descendants.
    */
    vector<CMappedFeat> feat_roots =
        feat_tree.GetChildren(CMappedFeat());
    CConstRef<CSeq_id> seq_id(bsh.GetSeqId());
    ITERATE (vector<CMappedFeat>, iter, feat_roots) {
        const CSeq_feat& seq_feat = iter->GetMappedFeature();
        CSeqFeatData::ESubtype subtype =
            seq_feat.GetData().GetSubtype();
        if (subtype == CSeqFeatData::eSubtype_gene) {
            vector<CMappedFeat> cc = feat_tree.GetChildren(*iter);
            ITERATE (vector<CMappedFeat>, iter1, cc) {
                CRef<CReportEntry> entry(new CReportEntry(
                    scope,
                    *seq_id,
                    *iter, // pass Gene explicitly
                    feat_tree,
                    *iter1));
                if (entry->IsValid()) entries.push_back(entry);
            }
        } else {
            CRef<CReportEntry> entry(new CReportEntry(
                scope,
                *seq_id,
                feat_tree,
                *iter));
            if (entry->IsValid()) entries.push_back(entry);
        }
    }
    return entries;
}


static
void s_ExtendEntriesFromAlignments(
    CScope& scope,
    const CBioseq_Handle& bsh,
    TSeqRange search_range,
    TReportEntryList& entries)
{
    CConstRef<CSeq_id> seq_id(bsh.GetSeqId());
    TReportEntryList new_entries;
    SAnnotSelector sel;
    //sel.SetAdaptiveDepth(true);
    // Slow, but otherwise it can't find all relevant alignments
    // for composite sequences
    sel.SetResolveAll();
    for (CAlign_CI align_it(bsh, search_range, sel); align_it; ++align_it) {
        const CSeq_align& al = *align_it;
        if (al.CheckNumRows() == 2) {
            string label; al.GetSeq_id(0).GetLabel(&label);
            bool found = false;
            NON_CONST_ITERATE(TReportEntryList, it, entries) {
                CConstRef<CSeq_id> product_id((*it)->GetTranscriptId());
                string entry_label; product_id->GetLabel(&entry_label);
//                LOG_POST("Check match for alignment " + label + " with existing entry " + entry_label);
                if (sequence::IsSameBioseq(
                        *product_id, al.GetSeq_id(0),
                        &scope))
                {
                    // Found exising entry - extend it with alignment
                    (*it)->SetAlignment(scope, al);
//                    LOG_POST("Entry for " + label + " extended by alignment");
                    found = true;
                    break;
                }
            }
            // If we did not match this alignment to an existing entry, make a new one.
            if (!found) {
                CBioseq_Handle target_bsh = scope.GetBioseqHandle(al.GetSeq_id(0));
                if (target_bsh && (GetSequenceType(target_bsh) & CSeq_id::eAcc_mrna)) {
                    // Make new entry
                    CRef<CReportEntry> entry(new CReportEntry(scope, *seq_id, al));
                    new_entries.push_back(entry);
//                    LOG_POST("New entry " + label + " created from alignment");
                }
            }
        }
    }
    entries.insert(entries.end(), new_entries.begin(), new_entries.end());
}


//static string x_GetSequence(CSeqVector& seq_vec, TSignedSeqPos pos, bool complement)
static string x_GetSequence(CSeqVector& seq_vec, TSignedSeqPos pos, TSignedSeqPos adjustment)
{
    string seq_before, seq_pos, seq_after;
    TSignedSeqPos seq_len = seq_vec.size();
    TSignedSeqPos max_seq_frame = TSignedSeqPos(kSeqFrame);

    pos += adjustment;

    if (adjustment >=0) {
        TSeqPos before_from = pos <= max_seq_frame ?
            0 : max(0, pos - max_seq_frame);
        seq_vec.GetSeqData(before_from, max(0, pos), seq_before);
        // This filler takes into account both before and after, so we don't
        // need to repeat it twice. We need to compensate for very negative
        // position to not overfill it, so we limit overfill by 2*kSeqFrame+1
        // which is total length of reported sequence context.
        if (pos < max_seq_frame) {
            string filler;
            int max_filler_len = min(2*max_seq_frame+1, max_seq_frame - pos);
            for (int i = 0; i < max_filler_len; i++) filler += "&nbsp;";
            seq_before = filler + seq_before;
        }
        if (adjustment != 0) {
            int fill_len = min(int(adjustment-1), int(seq_before.size()));
            seq_before.replace(seq_before.size()-fill_len, fill_len, fill_len, '-');
        }
    } else {
        for (int i = 0; i < max_seq_frame; i++) seq_before += "-";
    }
    if (adjustment == 0) {
        seq_vec.GetSeqData(pos, pos+1, seq_pos);
    } else {
        seq_pos = "-";
    }
    if (adjustment <= 0) {
        // If pos is less than -1, it starts eating into seq_after. The filler is taken
        // care of, now we ensure that we're not generating negative positions.
    
        seq_vec.GetSeqData(max(0, pos+1),
            max(0, min(seq_len, pos+max_seq_frame+1)), seq_after);
        if (adjustment != 0) {
            int fill_len = min(int(-adjustment-1), int(seq_after.size()));
            seq_after.replace(0, fill_len, fill_len, '-');
        }
    } else {
        for (int i = 0; i < max_seq_frame; i++) seq_after += "-";
    }
/*
    if (complement) {
        string comp;
        CSeqManip::Complement(seq_before, CSeqUtil::e_Iupacna, 0, seq_before.size(), comp);
        CSeqManip::Complement(seq_after, CSeqUtil::e_Iupacna, 0, seq_after.size(), seq_before);
        seq_before = comp;
        CSeqManip::Complement(seq_pos, CSeqUtil::e_Iupacna, 0, seq_pos.size(), seq_pos);
    }
*/

    return
        seq_before + "<span class='xsv-seq_pos_highlight'>" +
        seq_pos + "</span>" + seq_after;
}

static SAnnotSelector x_GetAnnotSelector()
{
    #ifdef __GUI_SEQ_UTILS_DEFINED__
    return CSeqUtils::GetAnnotSelector();
    #endif

    SAnnotSelector sel;
    sel
        // consider overlaps by total range...
        .SetOverlapTotalRange()
        // resolve all segments...
        .SetResolveAll()
    ;

    sel.SetExcludeExternal(false);

    ///
    /// known external annotations
    ///
    sel.ExcludeNamedAnnots("SNP");  /// SNPs = variation features
    sel.ExcludeNamedAnnots("CDD");  /// CDD  = conserved domains
    sel.ExcludeNamedAnnots("STS");  /// STS  = sequence tagged sites

    sel.SetAdaptiveDepth(true);
    sel.SetResolveAll();

    return sel;
}

/*
/////////////////////////////////////////////////////////////////////////////
// Service functions

static
TSignedSeqPos x_GetAdjustment(const CSeq_feat& feat, TSeqPos pos)
{
    // Calculate minimal distance to the exon boundary
    int adj = 0;
    bool plus_strand =
        sequence::GetStrand(feat.GetLocation()) != eNa_strand_minus;
    TSignedSeqPos signed_pos = static_cast<TSignedSeqPos>(pos);
    CSeq_loc_CI loc_iter(feat.GetLocation());
    for ( ;  loc_iter;  ++loc_iter) {
        const TSeqRange& curr = loc_iter.GetRange();
        TSignedSeqPos f = curr.GetFrom();
        TSignedSeqPos t = curr.GetTo();

        int adj_for_intron;
        if (signed_pos < f) {
            adj_for_intron = signed_pos - f;
        } else if (signed_pos > t) {
            adj_for_intron = signed_pos - t;
        } else {
            return 0; // no adjustment needed
        }
        if (adj == 0  ||  std::abs(adj_for_intron) < std::abs(adj)) {
            adj = adj_for_intron;
        }
    }
    return plus_strand ? adj : -adj;
}
*/


// END_NCBI_SCOPE
