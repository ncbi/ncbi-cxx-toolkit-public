/*  $Id$
 * =========================================================================
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
 * =========================================================================
 *
 * Authors: Colleen Bollin, based on similar discrepancy tests
 *
 */

#include <ncbi_pch.hpp>
#include "discrepancy_core.hpp"
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/RNA_gen.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objtools/cleanup/cleanup.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

DISCREPANCY_MODULE(feature_tests);


// PSEUDO_MISMATCH

const string kPseudoMismatch = "[n] CDSs, RNAs, and genes have mismatching pseudos.";

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(PSEUDO_MISMATCH, CSeq_feat_BY_BIOSEQ, eDisc | eOncaller, "Pseudo Mismatch")
//  ----------------------------------------------------------------------------
{
    if (obj.IsSetPseudo() && obj.GetPseudo() && 
        (obj.GetData().IsCdregion() || obj.GetData().IsRna())) {
        CConstRef<CSeq_feat> gene = CCleanup::GetGeneForFeature(obj, context.GetScope());
        if (gene && !CCleanup::IsPseudo(*gene, context.GetScope())) {
            m_Objs[kPseudoMismatch].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)), 
                    false).Fatal();
            m_Objs[kPseudoMismatch].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(gene)),
                    false).Fatal();
        }
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(PSEUDO_MISMATCH)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


static bool SetPseudo(const CSeq_feat* sf, CScope& scope)
{
    bool rval = false;
    if (!sf->IsSetPseudo() || !sf->GetPseudo()) {
        CRef<CSeq_feat> new_feat(new CSeq_feat());
        new_feat->Assign(*sf);
        new_feat->SetPseudo(true);
        CSeq_feat_EditHandle feh(scope.GetSeq_featHandle(*sf));
        feh.Replace(*new_feat);
        rval = true;
    }
    return rval;
}


DISCREPANCY_AUTOFIX(PSEUDO_MISMATCH)
{
    TReportObjectList list = item->GetDetails();
    unsigned int n = 0;
    NON_CONST_ITERATE (TReportObjectList, it, list) {
        const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer())->GetObject().GetPointer());
        if (sf && SetPseudo(sf, scope)) {
            n++;
        }
    }
    return CRef<CAutofixReport>(n ? new CAutofixReport("PSEUDO_MISMATCH: Set pseudo for [n] feature[s]", n) : 0);
}


// RNA_CDS_OVERLAP

typedef pair<size_t, bool> TRNALength;
typedef map<string, TRNALength > TRNALengthMap;

static const TRNALengthMap kTrnaLengthMap{
    { "16S", { 1000, false } },
    { "18S", { 1000, false } },
    { "23S", { 2000, false } },
    { "25S", { 1000, false } },
    { "26S", { 1000, false } },
    { "28S", { 1000, false } },
    { "28S", { 3300, false } },
    { "small", { 1000, false } },
    { "large", { 1000, false } },
    { "5.8S", { 130, true } },
    { "5S", { 90, true } } 
};


bool IsShortrRNA(const CSeq_feat& f, CScope* scope)
{
    if (f.GetData().GetSubtype() != CSeqFeatData::eSubtype_rRNA) {
        return false;
    }

    bool is_bad = false;

    size_t len = sequence::GetLength(f.GetLocation(), scope);

    string rrna_name = f.GetData().GetRna().GetRnaProductName();

    ITERATE (TRNALengthMap, it, kTrnaLengthMap) {
        if (NStr::FindNoCase(rrna_name, it->first) != string::npos &&
            len < it->second.first &&
            (!it->second.second || (f.IsSetPartial() && f.GetPartial())) ) {
            is_bad = true;
            break;
        }
    }

    return is_bad;
}


const string kCDSRNAAnyOverlap = "[n/2] coding region[s] overlap RNA feature[s]";
const string kCDSRNAExactMatch = "[n/2] coding region location[s] exactly match an RNA location";
const string kCDSRNAContainedIn = "[n/2] coding region[s] [is] completely contained in RNA[s]";
const string kCDSRNAContains = "[n/2] coding region[s] completely contain RNA[s]";
const string kCDSRNAContainstRNA = "[n/2] coding region[s] completely contain tRNA[s]";
const string kCDSRNAOverlapNoContain = "[n/2] coding regions overlap RNA[s] (no containment)";
const string kCDSRNAOverlapNoContainSameStrand = "[n/2] coding region[s] overlap RNA[s] on the same strand (no containment)";
const string kCDSRNAOverlapNoContainOppStrand = "[n/2] coding region[s] overlap RNA[s] on the opposite strand (no containment)";

void ProcessCDSRNAPair(CConstRef<CSeq_feat> cds_sf, CConstRef<CSeq_feat> rna_sf, CReportNode& m_Objs, CDiscrepancyContext& context)
{
    sequence::ECompare loc_compare =
        sequence::Compare(cds_sf->GetLocation(),
        rna_sf->GetLocation(),
        &(context.GetScope()),
        sequence::fCompareOverlapping);
    if (loc_compare == sequence::eSame) {
        m_Objs[kCDSRNAAnyOverlap][kCDSRNAExactMatch].Ext().Add(*context.NewDiscObj(cds_sf), false).Add(*context.NewDiscObj(rna_sf), false).Fatal();
    }
    else if (loc_compare == sequence::eContained) {
        m_Objs[kCDSRNAAnyOverlap][kCDSRNAContainedIn].Ext().Add(*context.NewDiscObj(cds_sf), false).Add(*context.NewDiscObj(rna_sf), false).Fatal();
    }
    else if (loc_compare == sequence::eContains) {
        if (rna_sf->GetData().GetSubtype() == CSeqFeatData::eSubtype_tRNA) {
            m_Objs[kCDSRNAAnyOverlap][kCDSRNAContainstRNA].Ext().Add(*context.NewDiscObj(cds_sf), false).Add(*context.NewDiscObj(rna_sf), false).Fatal();
        }
        else {
            m_Objs[kCDSRNAAnyOverlap][kCDSRNAContains].Ext().Add(*context.NewDiscObj(cds_sf), false).Add(*context.NewDiscObj(rna_sf), false).Fatal();
        }
    }
    else if (loc_compare != sequence::eNoOverlap) {
        ENa_strand cds_strand = cds_sf->GetLocation().GetStrand();
        ENa_strand rna_strand = rna_sf->GetLocation().GetStrand();
        if (cds_strand == eNa_strand_minus && rna_strand != eNa_strand_minus) {
            //ADD_CDS_RNA_TRIPLET(kCDSRNAOverlapNoContain, kCDSRNAOverlapNoContainOppStrand);
            m_Objs[kCDSRNAAnyOverlap][kCDSRNAOverlapNoContain].Ext()[kCDSRNAOverlapNoContainOppStrand].Ext().Add(*context.NewDiscObj(cds_sf), false).Add(*context.NewDiscObj(rna_sf), false).Fatal();
        }
        else if (cds_strand != eNa_strand_minus && rna_strand == eNa_strand_minus) {
            //ADD_CDS_RNA_TRIPLET(kCDSRNAOverlapNoContain, kCDSRNAOverlapNoContainOppStrand);
            m_Objs[kCDSRNAAnyOverlap][kCDSRNAOverlapNoContain].Ext()[kCDSRNAOverlapNoContainOppStrand].Ext().Add(*context.NewDiscObj(cds_sf), false).Add(*context.NewDiscObj(rna_sf), false).Fatal();
        }
        else {
            //ADD_CDS_RNA_TRIPLET(kCDSRNAOverlapNoContain, kCDSRNAOverlapNoContainSameStrand);
            m_Objs[kCDSRNAAnyOverlap][kCDSRNAOverlapNoContain].Ext()[kCDSRNAOverlapNoContainSameStrand].Ext().Add(*context.NewDiscObj(cds_sf), false).Add(*context.NewDiscObj(rna_sf), false).Fatal();
        }
    }
}

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(RNA_CDS_OVERLAP, CSeq_feat_BY_BIOSEQ, eDisc, "CDS RNA Overlap")
//  ----------------------------------------------------------------------------
{
    if (!obj.GetData().IsRna() && !obj.GetData().IsCdregion()) {
        return;
    }

    // See if we have moved to the "next" Bioseq
    if (m_Count != context.GetCountBioseq()) {
        m_Count = context.GetCountBioseq();
        m_Objs["RNAs"].clear();
        m_Objs["coding regions"].clear();
        m_Objs["tRNAs"].clear();
    }

    // We ask to keep the reference because we do need the actual object to stick around so we can deal with them later.
    CRef<CDiscrepancyObject> this_disc_obj(context.NewDiscObj(CConstRef<CSeq_feat>(&obj), eKeepRef));
    const CSeq_loc& this_location = obj.GetLocation();

    if (obj.GetData().IsCdregion()) {
        CConstRef<CSeq_feat> cds_sf(&obj);
        NON_CONST_ITERATE (TReportObjectList, robj, m_Objs["RNAs"].GetObjects()) {
            const CDiscrepancyObject* other_disc_obj = dynamic_cast<CDiscrepancyObject*>(robj->GetNCPointer());
            CConstRef<CSeq_feat> rna_sf(dynamic_cast<const CSeq_feat*>(other_disc_obj->GetObject().GetPointer()));
            if (rna_sf) {
                ProcessCDSRNAPair(cds_sf, rna_sf, m_Objs, context);
            }
        }
        NON_CONST_ITERATE (TReportObjectList, robj, m_Objs["tRNAs"].GetObjects()) {
            const CDiscrepancyObject* other_disc_obj = dynamic_cast<CDiscrepancyObject*>(robj->GetNCPointer());
            CConstRef<CSeq_feat> rna_sf(dynamic_cast<const CSeq_feat*>(other_disc_obj->GetObject().GetPointer()));
            if (rna_sf) {
                ProcessCDSRNAPair(cds_sf, rna_sf, m_Objs, context);
            }
        }
        m_Objs["coding regions"].Add(*this_disc_obj);
    } else if (obj.GetData().IsRna()) {
        bool do_check = false;
        CSeqFeatData::ESubtype subtype = obj.GetData().GetSubtype();
        if (subtype == CSeqFeatData::eSubtype_tRNA) {
            if (!context.IsEukaryotic()) {
                do_check = true;
            }
        } else if (subtype == CSeqFeatData::eSubtype_mRNA || subtype == CSeqFeatData::eSubtype_ncRNA) {
            //always ignore these
        } else if (subtype == CSeqFeatData::eSubtype_rRNA) {
            // check to see if these are "short"
            do_check = !IsShortrRNA(obj, &(context.GetScope()));
        } else {
            do_check = true;
        }
        if (do_check) {
            CConstRef<CSeq_feat> rna_sf(&obj);
            NON_CONST_ITERATE (TReportObjectList, cobj, m_Objs["coding regions"].GetObjects()) {
                const CDiscrepancyObject* other_disc_obj = dynamic_cast<CDiscrepancyObject*>(cobj->GetNCPointer());
                CConstRef<CSeq_feat> cds_sf(dynamic_cast<const CSeq_feat*>(other_disc_obj->GetObject().GetPointer()));
                if (cds_sf) {
                    ProcessCDSRNAPair(cds_sf, rna_sf, m_Objs, context);
                }
            }
            if (subtype == CSeqFeatData::eSubtype_tRNA) {
                m_Objs["tRNAs"].Add(*this_disc_obj);
            } else {
                m_Objs["RNAs"].Add(*this_disc_obj);
            }
        }
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(RNA_CDS_OVERLAP)
//  ----------------------------------------------------------------------------
{
    m_Objs.GetMap().erase("RNAs");
    m_Objs.GetMap().erase("coding regions");
    m_Objs.GetMap().erase("tRNAs");

    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


// DISC_SHORT_RRNA

const string kShortRRNA = "[n] rRNA feature[s] [is] too short";

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(SHORT_RRNA, CSeq_feat_BY_BIOSEQ, eDisc | eOncaller, "Short rRNA Features")
//  ----------------------------------------------------------------------------
{
    if (!obj.IsSetPartial() && IsShortrRNA(obj, &(context.GetScope()))) {
        m_Objs[kShortRRNA].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)),
            false).Fatal();
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(SHORT_RRNA)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// DISC_RBS_WITHOUT_GENE

const string kRBSWithoutGene = "[n] RBS feature[s] [does] not have overlapping gene[s]";
const string kRBS = "RBS";

bool IsRBS(const CSeq_feat& f)
{
    if (f.GetData().GetSubtype() == CSeqFeatData::eSubtype_RBS) {
        return true;
    }
    if (f.GetData().GetSubtype() != CSeqFeatData::eSubtype_regulatory) {
        return false;
    }
    if (!f.IsSetQual()) {
        return false;
    }
    ITERATE (CSeq_feat::TQual, it, f.GetQual()) {
        if ((*it)->IsSetQual() && NStr::Equal((*it)->GetQual(), "regulatory_class") &&
            CSeqFeatData::GetRegulatoryClass((*it)->GetVal()) == CSeqFeatData::eSubtype_RBS) {
            return true;
        }
    }
    return false;
}

void AddRBS(CReportNode& objs, CDiscrepancyContext& context) 
{
    if (!objs["genes"].empty()) {
        NON_CONST_ITERATE (TReportObjectList, robj, objs[kRBS].GetObjects()) {
            const CDiscrepancyObject* other_disc_obj = dynamic_cast<CDiscrepancyObject*>(robj->GetNCPointer());
            CConstRef<CSeq_feat> rbs(dynamic_cast<const CSeq_feat*>(other_disc_obj->GetObject().GetPointer()));
            objs[kRBSWithoutGene].Add(*context.NewDiscObj(rbs), false).Fatal();
        }
    }
}
//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(RBS_WITHOUT_GENE, CSeq_feat_BY_BIOSEQ, eDisc | eOncaller, "RBS features should have an overlapping gene")
//  ----------------------------------------------------------------------------
{
    // See if we have moved to the "next" Bioseq
    if (m_Count != context.GetCountBioseq()) {
        AddRBS(m_Objs, context);

        m_Count = context.GetCountBioseq();
        m_Objs["genes"].clear();
        m_Objs["RBS"].clear();
    }

    // We ask to keep the reference because we do need the actual object to stick around so we can deal with them later.
    CRef<CDiscrepancyObject> this_disc_obj(context.NewDiscObj(CConstRef<CSeq_feat>(&obj), eKeepRef));

    if (obj.GetData().GetSubtype() == CSeqFeatData::eSubtype_gene) {
        m_Objs["genes"].Add(*this_disc_obj);
    } else if (IsRBS(obj) && CCleanup::GetGeneForFeature(obj, context.GetScope()) == NULL) {
        m_Objs["RBS"].Add(*this_disc_obj);
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(RBS_WITHOUT_GENE)
//  ----------------------------------------------------------------------------
{
    AddRBS(m_Objs, context);
    m_Objs.GetMap().erase("genes");
    m_Objs.GetMap().erase("RBS");
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}

// MISSING_GENES

const string kMissingGene = "[n] feature[s] [has] no genes";

bool ReportGeneMissing(const CSeq_feat& f)
{
    CSeqFeatData::ESubtype subtype = f.GetData().GetSubtype();
    if (IsRBS(f) ||
        f.GetData().IsCdregion() ||
        f.GetData().IsRna() ||
        subtype == CSeqFeatData::eSubtype_exon ||
        subtype == CSeqFeatData::eSubtype_intron) {
        return true;
    } else {
        return false;
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(MISSING_GENES, CSeq_feat_BY_BIOSEQ, eDisc | eOncaller, "Missing Genes")
//  ----------------------------------------------------------------------------
{
    if (!ReportGeneMissing(obj)) {
        return;
    }
    const CGene_ref* gene = obj.GetGeneXref();
    if (gene) {
        return;
    }
    CConstRef<CSeq_feat> gene_feat = CCleanup::GetGeneForFeature(obj, context.GetScope());
    if (!gene_feat) {
        m_Objs[kMissingGene].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)),
            false).Fatal();
    }
}

//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(MISSING_GENES)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// EXTRA_GENES

const string kExtraGene = "[n] gene feature[s] [is] not associated with a CDS or RNA feature.";
const string kExtraPseudo = "[n] pseudo gene feature[s] [is] not associated with a CDS or RNA feature.";
const string kExtraGeneNonPseudoNonFrameshift = "[n] non-pseudo gene feature[s] are not associated with a CDS or RNA feature and [does] not have frameshift in the comment.";

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(EXTRA_GENES, CSeq_feat_BY_BIOSEQ, eDisc | eOncaller, "Extra Genes")
//  ----------------------------------------------------------------------------
{
    // TODO: Do not collect if mRNA sequence in Gen-prod set

    // do not collect if pseudo
    if (!obj.GetData().IsGene()) {
        return;
    }
    bool gene_partial_start = obj.GetLocation().IsPartialStart(eExtreme_Biological);
    bool gene_partial_stop = obj.GetLocation().IsPartialStop(eExtreme_Biological);

    // Are any "reportable" features under this gene?
    CFeat_CI fi(context.GetScope(), obj.GetLocation());
    bool found_reportable = false;
    while (fi && !found_reportable) {
        if (fi->GetData().IsCdregion() ||
            fi->GetData().IsRna()) {
            bool exclude_for_partials = false;
            if (sequence::Compare(obj.GetLocation(), fi->GetLocation(), &(context.GetScope()), sequence::fCompareOverlapping) == sequence::eSame) {
                // check partials
                if (!gene_partial_start && fi->GetLocation().IsPartialStart(eExtreme_Biological)) {
                    exclude_for_partials = true;
                } else if (!gene_partial_stop && fi->GetLocation().IsPartialStop(eExtreme_Biological)) {
                    exclude_for_partials = true;
                }
            }
            if (!exclude_for_partials) {
                found_reportable = true;
            }
        }
        ++fi;
    }

    if (found_reportable) {
        return;
    }
    
    if (CCleanup::IsPseudo(obj, context.GetScope())) {
        m_Objs[kExtraGene][kExtraPseudo].Ext().Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)));
    } else {
        // do not report if note or description
        if (obj.IsSetComment() && !NStr::IsBlank(obj.GetComment())) {
            // ignore genes with notes
        }
        else if (obj.GetData().GetGene().IsSetDesc() && !NStr::IsBlank(obj.GetData().GetGene().GetDesc())) {
            // ignore genes with descriptions
        }
        else {
            m_Objs[kExtraGene][kExtraGeneNonPseudoNonFrameshift].Ext().Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)));
        }
    }

}

//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(EXTRA_GENES)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// BACTERIAL_PARTIAL_NONEXTENDABLE_PROBLEMS

const string kNonExtendableException = "unextendable partial coding region";

const string kNonExtendable = "[n] feature[s] [has] partial ends that do not abut the end of the sequence or a gap, and cannot be extended by 3 or fewer nucleotides to do so";

bool IsExtendableLeft(TSeqPos left, const CBioseq& seq, CScope* scope, TSeqPos& extend_len)
{
    bool rval = false;
    if (left < 3) {
        rval = true;
        extend_len = left;
    } else if (seq.IsSetInst() && seq.GetInst().IsSetRepr() &&
               seq.GetInst().GetRepr() == CSeq_inst::eRepr_delta &&
               seq.GetInst().IsSetExt() &&
               seq.GetInst().GetExt().IsDelta()) {
        TSeqPos offset = 0;
        TSeqPos last_gap_stop = 0;
        ITERATE(CDelta_ext::Tdata, it, seq.GetInst().GetExt().GetDelta().Get()) {
            if ((*it)->IsLiteral()) {
                offset += (*it)->GetLiteral().GetLength();
                if (!(*it)->GetLiteral().IsSetSeq_data()) {
                    last_gap_stop = offset;
                } else if ((*it)->GetLiteral().GetSeq_data().IsGap()) {
                    last_gap_stop = offset;
                }
            } else if ((*it)->IsLoc()) {
                offset += sequence::GetLength((*it)->GetLoc(), scope);
            }
            if (offset > left) {
                break;
            }
        }
        if (left - last_gap_stop < 3) {
            rval = true;
            extend_len = left - last_gap_stop;
        }
    }
    return rval;
}


bool IsExtendableRight(TSeqPos right, const CBioseq& seq, CScope* scope, TSeqPos& extend_len)
{
    bool rval = false;
    if (right > seq.GetLength() - 4) {
        rval = true;
        extend_len = seq.GetLength() - right - 1;
    } else if (seq.IsSetInst() && seq.GetInst().IsSetRepr() &&
        seq.GetInst().GetRepr() == CSeq_inst::eRepr_delta &&
        seq.GetInst().IsSetExt() &&
        seq.GetInst().GetExt().IsDelta()) {
        TSeqPos offset = 0;
        TSeqPos next_gap_start = 0;
        ITERATE(CDelta_ext::Tdata, it, seq.GetInst().GetExt().GetDelta().Get()) {
            if ((*it)->IsLiteral()) {
                if (!(*it)->GetLiteral().IsSetSeq_data()) {
                    next_gap_start = offset;
                } else if ((*it)->GetLiteral().GetSeq_data().IsGap()) {
                    next_gap_start = offset;
                }
                offset += (*it)->GetLiteral().GetLength();
            } else if ((*it)->IsLoc()) {
                offset += sequence::GetLength((*it)->GetLoc(), scope);
            }
            if (offset > right + 3) {
                break;
            }
        }
        if (next_gap_start > right && next_gap_start - right < 3) {
            rval = true;
            extend_len = next_gap_start - right;
        }
    }
    return rval;
}


bool IsNonExtendable(const CSeq_loc& loc, const CBioseq& seq, CScope* scope)
{
    bool rval = false;
    if (loc.IsPartialStart(eExtreme_Positional)) {
        TSeqPos start = loc.GetStart(eExtreme_Positional);
        if (start > 0) {
            TSeqPos extend_len = 0;
            if (!IsExtendableLeft(start, seq, scope, extend_len)) {
                rval = true;
            }
        }
    }
    if (!rval && loc.IsPartialStop(eExtreme_Positional)) {
        TSeqPos stop = loc.GetStop(eExtreme_Positional);
        if (stop < seq.GetLength() - 1) {
            TSeqPos extend_len = 0;
            if (!IsExtendableRight(stop, seq, scope, extend_len)) {
                rval = true;
            }
        }
    }
    return rval;
}


//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(BACTERIAL_PARTIAL_NONEXTENDABLE_PROBLEMS, CSeq_feat_BY_BIOSEQ, eDisc, "Find partial feature ends on bacterial sequences that cannot be extended: on when non-eukaryote")
//  ----------------------------------------------------------------------------
{
    if (context.HasLineage("Eukaryota") || context.GetCurrentBioseq()->IsAa()) {
        return;
    }
    //only examine coding regions
    if (!obj.IsSetData() || !obj.GetData().IsCdregion()) {
        return;
    }
    //ignore if feature already has exception
    if (obj.IsSetExcept_text() && NStr::FindNoCase(obj.GetExcept_text(), kNonExtendableException) != string::npos) {
        return;
    }
    CConstRef<CBioseq> seq = context.GetCurrentBioseq();

    bool add_this = IsNonExtendable(obj.GetLocation(), *seq, &(context.GetScope()));

    if (add_this) {
        m_Objs[kNonExtendable].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)),
            false).Fatal();
    }
}


static bool AddToException(const CSeq_feat* sf, CScope& scope, const string& except_text)
{
    bool rval = false;
    if (!sf->IsSetExcept_text() || NStr::Find(sf->GetExcept_text(), except_text) == string::npos) {
        CRef<CSeq_feat> new_feat(new CSeq_feat());
        new_feat->Assign(*sf);
        if (new_feat->IsSetExcept_text()) {
            new_feat->SetExcept_text(sf->GetExcept_text() + "; " + except_text);
        } else {
            new_feat->SetExcept_text(except_text);
        }
        new_feat->SetExcept(true);
        CSeq_feat_EditHandle feh(scope.GetSeq_featHandle(*sf));
        feh.Replace(*new_feat);
        rval = true;
    }
    return rval;
}


DISCREPANCY_AUTOFIX(BACTERIAL_PARTIAL_NONEXTENDABLE_PROBLEMS)
{
    TReportObjectList list = item->GetDetails();
    unsigned int n = 0;
    NON_CONST_ITERATE(TReportObjectList, it, list) {
        const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer())->GetObject().GetPointer());
        if (sf && AddToException(sf, scope, kNonExtendableException)) {
            n++;
        }
    }
    return CRef<CAutofixReport>(n ? new CAutofixReport("BACTERIAL_PARTIAL_NONEXTENDABLE_PROBLEMS: Set exception for [n] feature[s]", n) : 0);
}



//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(BACTERIAL_PARTIAL_NONEXTENDABLE_PROBLEMS)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


const string kBacterialPartialNonextendableException = "[n] feature[s] [has] partial ends that do not abut the end of the sequence or a gap, and cannot be extended by 3 or fewer nucleotides to do so, but [has] the correct exception";
//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(BACTERIAL_PARTIAL_NONEXTENDABLE_EXCEPTION, CSeq_feat_BY_BIOSEQ, eDisc, "Find partial feature ends on bacterial sequences that cannot be extended but have exceptions: on when non-eukaryote")
//  ----------------------------------------------------------------------------
{
    if (context.HasLineage("Eukaryota") || context.GetCurrentBioseq()->IsAa()) {
        return;
    }
    //only examine coding regions
    if (!obj.IsSetData() || !obj.GetData().IsCdregion()) {
        return;
    }
    //ignore if feature does not have exception
    if (!obj.IsSetExcept_text() || NStr::FindNoCase(obj.GetExcept_text(), kNonExtendableException) == string::npos) {
        return;
    }
    CConstRef<CBioseq> seq = context.GetCurrentBioseq();

    bool add_this = IsNonExtendable(obj.GetLocation(), *seq, &(context.GetScope()));

    if (add_this) {
        m_Objs[kBacterialPartialNonextendableException].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)),
            false);
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(BACTERIAL_PARTIAL_NONEXTENDABLE_EXCEPTION)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


const string kPartialProblems = "[n] feature[s] [has] partial ends that do not abut the end of the sequence or a gap, but could be extended by 3 or fewer nucleotides to do so";
//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(PARTIAL_PROBLEMS, CSeq_feat_BY_BIOSEQ, eDisc, "Find partial feature ends on bacterial sequences that cannot be extended but have exceptions: on when non-eukaryote")
//  ----------------------------------------------------------------------------
{
    if (context.HasLineage("Eukaryota") || context.GetCurrentBioseq()->IsAa()) {
        return;
    }
    //only examine coding regions
    if (!obj.IsSetData() || !obj.GetData().IsCdregion()) {
        return;
    }
    CConstRef<CBioseq> seq = context.GetCurrentBioseq();

    bool add_this = false;
    if (obj.GetLocation().IsPartialStart(eExtreme_Positional)) {
        TSeqPos start = obj.GetLocation().GetStart(eExtreme_Positional);
        if (start > 0) {
            TSeqPos extend_len = 0;
            if (IsExtendableLeft(start, *seq, &(context.GetScope()), extend_len)) {
                add_this = true;
            }
        }
    }
    if (!add_this && obj.GetLocation().IsPartialStop(eExtreme_Positional)) {
        TSeqPos stop = obj.GetLocation().GetStop(eExtreme_Positional);
        if (stop < seq->GetLength() - 1) {
            TSeqPos extend_len = 0;
            if (!IsExtendableRight(stop, *seq, &(context.GetScope()), extend_len)) {
                add_this = true;
            }
        }
    }

    if (add_this) {
        m_Objs[kPartialProblems].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)),
            false).Fatal();
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(PARTIAL_PROBLEMS)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


static bool ExtendToGapsOrEnds(const CSeq_feat& sf, CScope& scope)
{
    bool rval = false;

    CBioseq_Handle bsh = scope.GetBioseqHandle(sf.GetLocation());
    if (!bsh) {
        return rval;
    }
    CConstRef<CBioseq> seq = bsh.GetCompleteBioseq();

    CRef<CSeq_feat> new_feat(new CSeq_feat());
    new_feat->Assign(sf);

    if (sf.GetLocation().IsPartialStart(eExtreme_Positional)) {
        TSeqPos start = sf.GetLocation().GetStart(eExtreme_Positional);
        if (start > 0) {
            TSeqPos extend_len = 0;
            if (IsExtendableLeft(start, *seq, &scope, extend_len) &&
                CCleanup::SeqLocExtend(new_feat->SetLocation(), start - extend_len, scope)) {
                rval = true;
            }
        }
    }

    if (sf.GetLocation().IsPartialStop(eExtreme_Positional)) {
        TSeqPos stop = sf.GetLocation().GetStop(eExtreme_Positional);
        if (stop > 0) {
            TSeqPos extend_len = 0;
            if (IsExtendableRight(stop, *seq, &scope, extend_len) &&
                CCleanup::SeqLocExtend(new_feat->SetLocation(), stop + extend_len, scope)) {
                rval = true;
            }
        }
    }

    if (rval) {
        CSeq_feat_EditHandle feh(scope.GetSeq_featHandle(sf));
        feh.Replace(*new_feat);
        rval = true;
    }
    return rval;
}


DISCREPANCY_AUTOFIX(PARTIAL_PROBLEMS)
{
    TReportObjectList list = item->GetDetails();
    unsigned int n = 0;
    NON_CONST_ITERATE(TReportObjectList, it, list) {
        const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer())->GetObject().GetPointer());
        if (sf && ExtendToGapsOrEnds(*sf, scope)) {
            n++;
        }
    }
    return CRef<CAutofixReport>(n ? new CAutofixReport("BACTERIAL_PARTIAL_NONEXTENDABLE_PROBLEMS: Set exception for [n] feature[s]", n) : 0);
}


// EUKARYOTE_SHOULD_HAVE_MRNA

const string kEukaryoteShouldHavemRNA = "no mRNA present";
const string kEukaryoticCDSHasMrna = "Eukaryotic CDS has mRNA";

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(EUKARYOTE_SHOULD_HAVE_MRNA, CSeq_feat_BY_BIOSEQ, eDisc, "Eukaryote should have mRNA")
//  ----------------------------------------------------------------------------
{
    if (!obj.IsSetData() || !obj.GetData().IsCdregion() || CCleanup::IsPseudo(obj, context.GetScope())) {
        return;
    }
    if (!context.IsEukaryotic()) {
        return;
    }
    const CMolInfo* molinfo = context.GetCurrentMolInfo();
    if (!molinfo || !molinfo->IsSetBiomol() || molinfo->GetBiomol() != CMolInfo::eBiomol_genomic) {
        return;
    }
    
    CConstRef<CSeq_feat> mrna = sequence::GetmRNAforCDS(obj, context.GetScope());

    if (mrna) {
        m_Objs[kEukaryoticCDSHasMrna].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)),
            false);
    } else if (m_Objs[kEukaryoteShouldHavemRNA].GetObjects().empty()) {
        m_Objs[kEukaryoteShouldHavemRNA].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)),
            false).Fatal();
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(EUKARYOTE_SHOULD_HAVE_MRNA)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    if (!m_Objs[kEukaryoteShouldHavemRNA].GetObjects().empty() && m_Objs[kEukaryoticCDSHasMrna].GetObjects().empty()) {
        m_Objs.GetMap().erase(kEukaryoticCDSHasMrna);
        m_Objs[kEukaryoteShouldHavemRNA].clearObjs();
        m_ReportItems = m_Objs.Export(*this)->GetSubitems();
    }
}


// NON_GENE_LOCUS_TAG
//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(NON_GENE_LOCUS_TAG, CSeq_feat, eDisc | eOncaller, "Nongene Locus Tag")
//  ----------------------------------------------------------------------------
{
    if (obj.IsSetData() && obj.GetData().IsGene()) {
        return;
    }
    if (!obj.IsSetQual()) {
        return;
    }
    bool report = false;
    ITERATE(CSeq_feat::TQual, it, obj.GetQual()) {
        if ((*it)->IsSetQual() && NStr::EqualNocase((*it)->GetQual(), "locus_tag")) {
            report = true;
            break;
        }
    }
    if (report) {
        m_Objs["[n] non-gene feature[s] [has] locus tag[s]."].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)));
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(NON_GENE_LOCUS_TAG)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// FIND_BADLEN_TRNAS
const string ktRNATooShort = "[n] tRNA[s] [is] too short";
const string ktRNATooLong = "[n] tRNA[s] [is] too long";
//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(FIND_BADLEN_TRNAS, CSeq_feat, eDisc | eOncaller, "Find short and long tRNAs")
//  ----------------------------------------------------------------------------
{
    if (!obj.IsSetData() || obj.GetData().GetSubtype() != CSeqFeatData::eSubtype_tRNA) {
        return;
    }
    TSeqPos len = sequence::GetLength(obj.GetLocation(), &(context.GetScope()));
    if (!obj.IsSetPartial() && len < 50) {
        m_Objs[ktRNATooShort].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)));
    }
    else if (len > 100) {
        m_Objs[ktRNATooLong].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)));
    }
    else if (len > 90) {
        const string aa = context.GetAminoacidName(obj);
        if (aa != "Ser" && aa != "Sec" && aa != "Leu") {
            m_Objs[ktRNATooLong].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)));
        }
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(FIND_BADLEN_TRNAS)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// GENE_PARTIAL_CONFLICT
bool IsPartialStartConflict(const CSeq_feat& feat, const CSeq_feat& gene)
{
    bool rval = false;
    bool partial_feat = feat.GetLocation().IsPartialStart(eExtreme_Biological);
    bool partial_gene = gene.GetLocation().IsPartialStart(eExtreme_Biological);
    
    if ((partial_feat && !partial_gene) || (!partial_feat && partial_gene)) {
        if (feat.GetLocation().GetStart(eExtreme_Biological) == gene.GetLocation().GetStart(eExtreme_Biological)) {
            rval = true;
        }
    }
    return rval;
}


bool IsPartialStopConflict(const CSeq_feat& feat, const CSeq_feat& gene)
{
    bool rval = false;
    bool partial_feat = feat.GetLocation().IsPartialStop(eExtreme_Biological);
    bool partial_gene = gene.GetLocation().IsPartialStop(eExtreme_Biological);

    if ((partial_feat && !partial_gene) || (!partial_feat && partial_gene)) {
        if (feat.GetLocation().GetStop(eExtreme_Biological) == gene.GetLocation().GetStop(eExtreme_Biological)) {
            rval = true;
        }
    }
    return rval;
}

const string kGenePartialConflictTop = "[n/2] feature location[s] conflict with partialness of overlapping gene";
const string kGenePartialConflictOther = "[n/2] feature[s] that [is] not coding region[s] or misc_feature[s] conflict with partialness of overlapping gene";
const string kGenePartialConflictCodingRegion = "[n/2] coding region location[s] conflict with partialness of overlapping gene";
const string kGenePartialConflictMiscFeat = "[n/2] misc_feature location[s] conflict with partialness of overlapping gene";
const string kConflictBoth = " feature partialness conflicts with gene on both ends";
const string kConflictStart = " feature partialness conflicts with gene on 5' end";
const string kConflictStop = " feature partialness conflicts with gene on 3' end";


//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(GENE_PARTIAL_CONFLICT, CSeq_feat_BY_BIOSEQ, eOncaller, "Feature partialness should agree with gene partialness if endpoints match")
//  ----------------------------------------------------------------------------
{
    if (!obj.IsSetData()) {
        return;
    }
    const CSeq_feat* gene = context.GetCurrentGene();
    if (!gene) {
        return;
    }

    CSeqFeatData::ESubtype subtype = obj.GetData().GetSubtype();

    bool conflict_start = false;
    bool conflict_stop = false;

    string middle_label = kGenePartialConflictOther;

    if (obj.GetData().IsCdregion()) {
        bool is_mrna = context.IsCurrentSequenceMrna();
        if (!context.IsEukaryotic() || is_mrna) {
            middle_label = kGenePartialConflictCodingRegion;
            conflict_start = IsPartialStartConflict(obj, *gene);
            conflict_stop = IsPartialStopConflict(obj, *gene);
            if (is_mrna && (!conflict_start || !conflict_stop)) {                
                CBioseq_Handle bsh = context.GetScope().GetBioseqHandle(*(context.GetCurrentBioseq()));               
                if (!conflict_start) {
                    //look for 5' UTR
                    TSeqPos gene_start = gene->GetLocation().GetStart(eExtreme_Biological);
                    bool found_start = false;
                    for (CFeat_CI fi(bsh, CSeqFeatData::eSubtype_5UTR); fi; ++fi) {
                        if (fi->GetLocation().GetStart(eExtreme_Biological) == gene_start) {
                            found_start = true;
                            break;
                        }
                    }
                    if (!found_start) {
                        conflict_start = true;
                    }
                }
                if (!conflict_stop) {
                    //look for 3' UTR
                    TSeqPos gene_stop = gene->GetLocation().GetStop(eExtreme_Biological);
                    bool found_stop = false;
                    for (CFeat_CI fi(bsh, CSeqFeatData::eSubtype_3UTR); fi; ++fi) {
                        if (fi->GetLocation().GetStart(eExtreme_Biological) == gene_stop) {
                            found_stop = true;
                            break;
                        }
                    }
                    if (!found_stop) {
                        conflict_stop = true;
                    }
                }
            }
        }
    } else if (obj.GetData().IsRna() ||
        subtype == CSeqFeatData::eSubtype_intron ||
        subtype == CSeqFeatData::eSubtype_exon ||
        subtype == CSeqFeatData::eSubtype_5UTR ||
        subtype == CSeqFeatData::eSubtype_3UTR ||
        subtype == CSeqFeatData::eSubtype_misc_feature) {
        conflict_start = IsPartialStartConflict(obj, *gene);
        conflict_stop = IsPartialStopConflict(obj, *gene);
        if (subtype == CSeqFeatData::eSubtype_misc_feature) {
            middle_label = kGenePartialConflictMiscFeat;
        }
    }

    if (conflict_start || conflict_stop) {
        string label = CSeqFeatData::SubtypeValueToName(subtype);
        if (conflict_start && conflict_stop) {
            label += kConflictBoth;
        } else if (conflict_start) {
            label += kConflictStart;
        } else {
            label += kConflictStop;
        }
        m_Objs[kGenePartialConflictTop][middle_label].Ext()[label].Ext().Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)), false).Add(*context.NewDiscObj(CConstRef<CSeq_feat>(gene)), false);
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(GENE_PARTIAL_CONFLICT)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


bool StrandsMatch(ENa_strand s1, ENa_strand s2)
{
    if (s1 == eNa_strand_minus && s2 != eNa_strand_minus) {
        return false;
    } else if (s1 != eNa_strand_minus && s2 == eNa_strand_minus) {
        return false;
    } else {
        return true;
    }
}


bool HasMixedStrands(const CSeq_loc& loc)
{
    CSeq_loc_CI li(loc);
    if (!li) {
        return false;
    }
    ENa_strand first_strand = li.GetStrand();
    ++li;
    while (li) {
        if (!StrandsMatch(li.GetStrand(), first_strand)) {
            return true;
        }
        ++li;
    }
    return false;
}


// BAD_GENE_STRAND
const string kBadGeneStrand = "[n/2] feature location[s] conflict with gene location strand[s]";

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(BAD_GENE_STRAND, CSeq_feat_BY_BIOSEQ, eOncaller, "Genes and features that share endpoints should be on the same strand")
//  ----------------------------------------------------------------------------
{
    if (!obj.IsSetData() || !obj.GetData().IsGene() || !obj.IsSetLocation()) {
        return;
    }
    // note - use positional instead of biological, because we are *looking* for
    // objects on the opposite strand
    TSeqPos gene_start = obj.GetLocation().GetStart(eExtreme_Positional);
    TSeqPos gene_stop = obj.GetLocation().GetStop(eExtreme_Positional);
    bool gene_mixed_strands = HasMixedStrands(obj.GetLocation());

    CBioseq_Handle bsh = context.GetScope().GetBioseqHandle(*(context.GetCurrentBioseq()));
    for (CFeat_CI fi(bsh); fi; ++fi) {
        if (fi->GetData().GetSubtype() == CSeqFeatData::eSubtype_primer_bind ||
            fi->GetData().IsGene()) {
            continue;
        }
        if (fi->GetLocation().GetStart(eExtreme_Positional) > gene_stop) {
            break;
        }
        TSeqPos feat_start = fi->GetLocation().GetStart(eExtreme_Positional);
        TSeqPos feat_stop = fi->GetLocation().GetStop(eExtreme_Positional);
        if (feat_start == gene_start || feat_stop == gene_stop) {
            bool all_ok = true;
            if (gene_mixed_strands) {
                // compare intervals, to make sure each interval is covered by a gene interval on the correct strand
                CSeq_loc_CI f_loc(fi->GetLocation());
                while (f_loc && all_ok) {
                    CSeq_loc_CI g_loc(obj.GetLocation());
                    bool found = false;
                    while (g_loc && !found) {
                        if (StrandsMatch(f_loc.GetStrand(), g_loc.GetStrand())) {
                            sequence::ECompare cmp = sequence::Compare(*(f_loc.GetRangeAsSeq_loc()), *(g_loc.GetRangeAsSeq_loc()), &(context.GetScope()), sequence::fCompareOverlapping);
                            if (cmp == sequence::eContained || cmp == sequence::eSame) {
                                found = true;
                            }
                        }
                        ++g_loc;
                    }
                    if (!found) {
                        all_ok = false;
                    }
                    ++f_loc;
                }
            } else {
                all_ok = StrandsMatch(fi->GetLocation().GetStrand(), obj.GetLocation().GetStrand());
            }
            if (!all_ok) {
                size_t offset = m_Objs[kBadGeneStrand].GetMap().size() + 1;
                string label = "Gene and feature strands conflict (pair " + NStr::NumericToString(offset) + ")";
                m_Objs[kBadGeneStrand][label].Ext().Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)), false).Add(*context.NewDiscObj(fi->GetSeq_feat()), false);
            }
        }
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(BAD_GENE_STRAND)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


//MICROSATELLITE_REPEAT_TYPE
//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(MICROSATELLITE_REPEAT_TYPE, CSeq_feat_BY_BIOSEQ, eOncaller, "Microsatellites must have repeat type of tandem")
//  ----------------------------------------------------------------------------
{
    if (obj.GetData().GetSubtype() != CSeqFeatData::eSubtype_repeat_region || !obj.IsSetQual())
        return;

    bool is_microsatellite = false;
    bool is_tandem = false;

    const CSeq_feat::TQual& quals = obj.GetQual();
    for (auto it = quals.begin(); it != quals.end() && (!is_microsatellite || !is_tandem); ++it) {
        const CGb_qual& qual = **it;
        if (NStr::EqualCase(qual.GetQual(), "satellite")) {
            if (NStr::EqualNocase(qual.GetVal(), "microsatellite") ||
                NStr::StartsWith(qual.GetVal(), "microsatellite:", NStr::eNocase)) {
                is_microsatellite = true;
            }
        }
        else if (NStr::EqualCase(qual.GetQual(), "rpt_type")) {
            is_tandem = NStr::EqualCase(qual.GetVal(), "tandem");
        }
    }

    if (is_microsatellite && !is_tandem) {
        m_Objs["[n] microsatellite[s] do not have a repeat type of tandem"].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)), false).Fatal();
    }
}

// ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(MICROSATELLITE_REPEAT_TYPE)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}

//  ----------------------------------------------------------------------------
DISCREPANCY_AUTOFIX(MICROSATELLITE_REPEAT_TYPE)
    //  ----------------------------------------------------------------------------
{
    TReportObjectList list = item->GetDetails();
    unsigned int n = 0;
    NON_CONST_ITERATE(TReportObjectList, it, list) {
        const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer())->GetObject().GetPointer());
        if (sf) {
            CRef<CSeq_feat> new_feat(new CSeq_feat());
            new_feat->Assign(*sf);
            CRef<CGb_qual> new_qual(new CGb_qual("rpt_type", "tandem"));
            new_feat->SetQual().push_back(new_qual);
            CSeq_feat_EditHandle feh(scope.GetSeq_featHandle(*sf));
            feh.Replace(*new_feat);
            n++;
        }
    }
    return CRef<CAutofixReport>(n ? new CAutofixReport("MICROSATELLITE_REPEAT_TYPE: added repeat type of tandem to [n] microsatellite[s]", n) : 0);
}


// SUSPICIOUS_NOTE_TEXT
static const string kSuspiciousNotePhrases[] =
{
    "characterised",
    "recognised",
    "characterisation",
    "localisation",
    "tumour",
    "uncharacterised",
    "oxydase",
    "colour",
    "localise",
    "faecal",
    "orthologue",
    "paralogue",
    "homolog",
    "homologue",
    "intronless gene"
};

const size_t kNumSuspiciousNotePhrases = sizeof(kSuspiciousNotePhrases) / sizeof(string);

static const string kSuspiciousNoteTop = "[n] note text[s] contain suspicious phrase[s]";

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(SUSPICIOUS_NOTE_TEXT, CSeq_feat, eOncaller, "Find Suspicious Phrases in Note Text")
//  ----------------------------------------------------------------------------
{
    if (!obj.IsSetData()) {
        return;
    }
    for (size_t k = 0; k < kNumSuspiciousNotePhrases; k++) {
        bool has_phrase = false;
        switch (obj.GetData().GetSubtype()) {
            case CSeqFeatData::eSubtype_gene:
                // look in gene comment and gene description
                if (obj.IsSetComment() &&
                    NStr::FindNoCase(obj.GetComment(), kSuspiciousNotePhrases[k]) != string::npos) {
                    has_phrase = true;
                } else if (obj.GetData().GetGene().IsSetDesc() &&
                    NStr::FindNoCase(obj.GetData().GetGene().GetDesc(), kSuspiciousNotePhrases[k]) != string::npos) {
                    has_phrase = true;
                }
                break;
            case CSeqFeatData::eSubtype_prot:
                if (obj.GetData().GetProt().IsSetDesc() &&
                    NStr::FindNoCase(obj.GetData().GetProt().GetDesc(), kSuspiciousNotePhrases[k]) != string::npos) {
                    has_phrase = true;
                }
                break;
            case CSeqFeatData::eSubtype_cdregion:
            case CSeqFeatData::eSubtype_misc_feature:
                if (obj.IsSetComment() &&
                    NStr::FindNoCase(obj.GetComment(), kSuspiciousNotePhrases[k]) != string::npos) {
                    has_phrase = true;
                }
                break;
            default:
                break;
        }
        if (has_phrase) {
            m_Objs[kSuspiciousNoteTop]["[n] note text[s] contain '" + kSuspiciousNotePhrases[k] + "'"].Ext().Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)), false);
        }
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(SUSPICIOUS_NOTE_TEXT)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


// CDS_HAS_NEW_EXCEPTION

const string kCDSHasNewException = "[n] coding region[s] [has] new exception[s]";

static const string kNewExceptions[] =
{
    "annotated by transcript or proteomic data",
    "heterogeneous population sequenced",
    "low-quality sequence region",
    "unextendable partial coding region",
};


//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(CDS_HAS_NEW_EXCEPTION, CSeq_feat, eOncaller|eDisc, "Coding region has new exception")
//  ----------------------------------------------------------------------------
{
    if (!obj.IsSetData() || !obj.GetData().IsCdregion() || !obj.IsSetExcept_text()) {
        return;
    }
    size_t max = sizeof(kNewExceptions) / sizeof(string);
    bool found = false;
    for (size_t i = 0; i < max; i++) {
        if (NStr::FindNoCase(obj.GetExcept_text(), kNewExceptions[i]) != string::npos) {
            found = true;
            break;
        }
    }
    if (found) {
        m_Objs[kCDSHasNewException].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)), false);
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(CDS_HAS_NEW_EXCEPTION)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


// SHORT_LNCRNA

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(SHORT_LNCRNA, CSeq_feat, eOncaller | eDisc, "Short lncRNA sequences")
//  ----------------------------------------------------------------------------
{
    // only looking at lncrna features
    if (!obj.IsSetData() || 
        obj.GetData().GetSubtype() != CSeqFeatData::eSubtype_ncRNA ||
        !obj.GetData().IsRna() ||
        !obj.GetData().GetRna().IsSetExt() ||
        !obj.GetData().GetRna().GetExt().IsGen() || 
        !obj.GetData().GetRna().GetExt().GetGen().IsSetClass() ||
        !NStr::EqualNocase(obj.GetData().GetRna().GetExt().GetGen().GetClass(), "lncrna")) {
        return;
    }
    // ignore if partial
    if (obj.GetLocation().IsPartialStart(eExtreme_Biological) ||
        obj.GetLocation().IsPartialStop(eExtreme_Biological)) {
        return;
    }

    if (sequence::GetLength(obj.GetLocation(), &(context.GetScope())) < 200) {
        m_Objs["[n] lncRNA feature[s] [is] suspiciously short"].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)), false);
    }
}

//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(SHORT_LNCRNA)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


// JOINED_FEATURES

const string& kJoinedFeatures = "[n] feature[s] [has] joined location[s].";
const string& kJoinedFeaturesNoException = "[n] feature[s] [has] joined location but no exception";
const string& kJoinedFeaturesException = "[n] feature[s] [has] joined location but exception '";
const string& kJoinedFeaturesBlankException = "[n] feature[s] [has] joined location but a blank exception";

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(JOINED_FEATURES, CSeq_feat_BY_BIOSEQ, eDisc, "Joined Features: on when non-eukaryote")
//  ----------------------------------------------------------------------------
{
    if (context.IsEukaryotic() || !obj.IsSetLocation()) {
        return;
    }

    if (obj.GetLocation().IsMix() || obj.GetLocation().IsPacked_int()) {
        if (obj.IsSetExcept_text()) {
            if (NStr::IsBlank(obj.GetExcept_text())) {
                m_Objs[kJoinedFeatures][kJoinedFeaturesBlankException].Ext().Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)), false);
            } else {
                m_Objs[kJoinedFeatures][kJoinedFeaturesException + obj.GetExcept_text() + "'"].Ext().Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)), false);
            }
        } else if (obj.IsSetExcept() && obj.GetExcept()) {
            m_Objs[kJoinedFeatures][kJoinedFeaturesBlankException].Ext().Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)), false);
        } else {
            m_Objs[kJoinedFeatures][kJoinedFeaturesNoException].Ext().Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)), false);
        }
    }
}

//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(JOINED_FEATURES)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


// SHORT_INTRON
const string kShortIntronTop = "[n] intron[s] [is] shorter than 10 nt";
const string kShortIntronExcept = "[n] intron[s] [is] shorter than 11 nt and [has] an exception";

//"Introns shorter than 10 nt", "DISC_SHORT_INTRON", FindShortIntrons, AddExceptionsToShortIntrons},

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(SHORT_INTRON, CSeq_feat, eDisc | eOncaller, "Introns shorter than 10 nt")
//  ----------------------------------------------------------------------------
{
    if (!obj.IsSetData() || !obj.GetData().IsCdregion() || !obj.IsSetLocation()) {
        return;
    }
    if (CCleanup::IsPseudo(obj, context.GetScope())) {
        return;
    }
    // looking at space between coding region intervals

    CSeq_loc_CI li(obj.GetLocation());
    if (!li) {
        return;
    }
    bool found_short = false;
    TSeqPos last_start = li.GetRange().GetFrom();
    TSeqPos last_stop = li.GetRange().GetTo();
    ++li;
    while (li && !found_short) {
        TSeqPos start = li.GetRange().GetFrom();
        TSeqPos stop = li.GetRange().GetTo();
        if (start >= last_stop && start - last_stop < 11) {
            found_short = true;
        } else if (last_stop >= start && last_stop - start < 11) {
            found_short = true;
        } else if (stop >= last_start && stop - last_start < 11) {
            found_short = true;
        } else if (last_start >= stop && last_start - stop < 11) {
            found_short = true;
        }
        last_start = start;
        last_stop = stop;
        ++li;
    }
    if (found_short) {
        if (obj.IsSetExcept() && obj.GetExcept()) {
            m_Objs[kShortIntronTop][kShortIntronExcept].Ext().Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)), false);
        } else {
            m_Objs[kShortIntronTop].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)), false);
        }
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(SHORT_INTRON)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


#if 0
// waiting for clarification from Karen Clark before writing autofix
const string kPutativeFrameShift = "putative frameshift";


void AddException(const CSeq_feat& sf, CScope& scope, const string& exception_text)
{
    CRef<CSeq_feat> new_feat(new CSeq_feat());
    new_feat->Assign(sf);
    if (new_feat->IsSetExcept_text() && !NStr::IsBlank(new_feat->GetExcept_text())) {
        new_feat->SetExcept_text(new_feat->GetExcept_text() + "; " + exception_text);
    } else {
        new_feat->SetExcept_text(exception_text);
    }
    new_feat->SetExcept(true);
    CSeq_feat_EditHandle feh(scope.GetSeq_featHandle(sf));
    feh.Replace(*new_feat);
}


bool AddExceptionsToShortIntron(const CSeq_feat& sf, CScope& scope, const string& default_lineage)
{
    bool rval = false;
    CBioseq_Handle bsh;
    try {
        bsh = scope.GetBioseqHandle(sf.GetLocation());
    } catch (CException& ex) {
        return false;
    }
    CSeqdesc_CI src(bsh, CSeqdesc::e_Source);

    if (src) {
        if (src->GetSource().IsSetGenome() && src->GetSource().GetGenome() == CBioSource::eGenome_mitochondrion) {
            return false;
        } else if (CDiscrepancyContext::HasLineage(src->GetSource(), default_lineage, "Bacteria") ||
            CDiscrepancyContext::HasLineage(src->GetSource(), default_lineage, "Archea")) {
            return false;
        }
    }
    if (!sf.IsSetExcept_text() || NStr::Find(sf.GetExcept_text(), "low-quality sequence region") == string::npos) {
        AddException(sf, scope, "low-quality sequence region");
        rval = true;
    }
    return rval;
}


void AdjustBacterialGeneForCodingRegionWithShortIntron(const CSeq_feat& sf, const CSeq_feat& gene, CScope& scope)
{
    bool any_change = false;

    CRef<CSeq_feat> new_gene(new CSeq_feat());
    new_gene->Assign(gene);

    if (sf.IsSetComment() && !NStr::IsBlank(sf.GetComment())) {
        if (new_gene->IsSetComment() && !NStr::IsBlank(new_gene->GetComment())) {
            new_gene->SetComment(new_gene->GetComment() + "; " + sf.GetComment());
        } else {
            new_gene->SetComment(sf.GetComment());
        }
        any_change = true;
    }
    if (!new_gene->IsSetComment()) {
    }

    if (any_change) {

    }
}


#if 0
    if (src && CDiscrepancyContext::HasLineage(src->GetSource(), default_lineage, "Bacteria")) {

    }

    for (vnp = item_list; vnp != NULL; vnp = vnp->next) {
        if (vnp->choice != OBJ_SEQFEAT) continue;
        sfp = (SeqFeatPtr)vnp->data.ptrvalue;
        if (sfp == NULL) continue;
        bsp = BioseqFindFromSeqLoc(sfp->location);
        if (bsp == NULL) continue;

        sdp = SeqMgrGetNextDescriptor(bsp, NULL, Seq_descr_source, &context);
        if (sdp != NULL) biop = sdp->data.ptrvalue;
        if (biop != NULL && IsBacterialBioSource(biop)) {
            is_bac_src = TRUE;
        } else is_bac_src = FALSE;
        if (biop != NULL && biop->genome == GENOME_mitochondrion) {
            // no change
        } else if (biop != NULL && (is_bac_src || IsArchaealBioSource(biop))) {
            if (sfp->idx.subtype == FEATDEF_CDS) {
                if (is_bac_src) {
                    ValNodeAddPointerToEnd(&to_convert, OBJ_SEQFEAT, sfp);
                }

                gene = GetGeneForFeature(sfp);
                if (gene != NULL) {
                    gene->pseudo = TRUE;
                    if (StringDoesHaveText(sfp->comment)) {
                        if (StringDoesHaveText(gene->comment)) {
                            len = StringLen(sfp->comment) + StringLen(gene->comment) + 10;
                            str = (CharPtr)MemNew(sizeof(Char) * len);
                            if (str != NULL) {
                                StringCpy(str, sfp->comment);
                                StringCat(str, "; ");
                                StringCat(str, gene->comment);
                                gene->comment = MemFree(gene->comment);
                                gene->comment = str;
                            }
                        } else {
                            gene->comment = sfp->comment;
                            sfp->comment = NULL;
                            if (is_bac_src) {
                                sfp->comment = StringSave("contains short intron that may represent a frameshift");
                            }
                        }
                    }
                    if (StringSearch(gene->comment, kPutativeFrameShift) == NULL) {
                        if (StringDoesHaveText(gene->comment)) {
                            len = StringLen(kPutativeFrameShift) + StringLen(gene->comment) + 10;
                            str = (CharPtr)MemNew(sizeof(Char) * len);
                            if (str != NULL) {
                                StringCpy(str, kPutativeFrameShift);
                                StringCat(str, "; ");
                                StringCat(str, gene->comment);
                                gene->comment = MemFree(gene->comment);
                                gene->comment = str;
                            }
                        } else {
                            gene->comment = sfp->comment;
                            sfp->comment = NULL;
                            if (is_bac_src) {
                                sfp->comment = StringSave("contains short intron that may represent a frameshift");
                            }
                        }
                    }
                    slp = SeqLocMerge(bsp, gene->location, NULL, TRUE, FALSE, FALSE);
                    if (slp != NULL) {
                        gene->location = SeqLocFree(gene->location);
                        gene->location = slp;
                    }
                    pbsp = BioseqFindFromSeqLoc(sfp->product);
                    if (pbsp != NULL) {
                        pbsp->idx.deleteme = TRUE;
                    }
                    if (!is_bac_src) {
                        sfp->idx.deleteme = TRUE;
                        ValNodeAddInt(&entityIDList, 0, bsp->idx.entityID);
                    }
                }
            }
        } else if (StringStr(sfp->except_text, "low-quality sequence region") == NULL) {
            SetStringValue(&(sfp->except_text), "low-quality sequence region", ExistingTextOption_append_semi);
            sfp->excpt = TRUE;
            if (lip != NULL && lip->fp != NULL) {
                txt = GetDiscrepancyItemText(vnp);
                fprintf(lip->fp, "Added low-quality sequence region exception to %s\n", txt);
                txt = MemFree(txt);
            }
        }
    }

    entityIDList = ValNodeSort(entityIDList, SortByIntvalue);
    ValNodeUnique(&entityIDList, SortByIntvalue, ValNodeFree);

    if (to_convert.head != NULL) {
        to_convert.head = ValNodeSort(to_convert.head, SortVnpByChoiceAndPtrvalue);
        ValNodeUnique(&(to_convert.head), SortVnpByChoiceAndPtrvalue, ValNodeFree);

        ConvertListToMiscFeat(to_convert.head, FALSE, lip);
        if (lip != NULL) {
            if (lip->fp != NULL) {
                fprintf(lip->fp, "Converted %d contained coding regions to misc_features\n", ValNodeLen(to_convert.head));
            }
            lip->data_in_log = TRUE;
        }

        to_convert.head = ValNodeFree(to_convert.head);
    }
#endif


DISCREPANCY_AUTOFIX(SHORT_INTRON)
{
    TReportObjectList list = item->GetDetails();
    unsigned int n = 0;
    NON_CONST_ITERATE(TReportObjectList, it, list) {
        const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer())->GetObject().GetPointer());
        if (sf && AddExceptionsToShortIntron(*sf, scope, "")) {
            n++;
        }
    }
    return CRef<CAutofixReport>(n ? new CAutofixReport("SHORT_INTRON: Set exception for [n] feature[s]", n) : 0);
}
#endif


// UNNECESSARY_VIRUS_GENE
//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(UNNECESSARY_VIRUS_GENE, CSeq_feat, eOncaller, "Unnecessary gene features on virus: on when lineage is not Picornaviridae,Potyviridae,Flaviviridae and Togaviridae")
//  ----------------------------------------------------------------------------
{
    if (!obj.IsSetData() || !obj.GetData().IsGene()) {
        return;
    }
    if (context.HasLineage("Picornaviridae") ||
        context.HasLineage("Potyviridae") ||
        context.HasLineage("Flaviviridae") ||
        context.HasLineage("Togaviridae")) {
        m_Objs["[n] virus gene[s] need to be removed"].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)), false);
    }

}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(UNNECESSARY_VIRUS_GENE)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
