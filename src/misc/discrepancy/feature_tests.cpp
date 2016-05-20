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
#include <objtools/cleanup/cleanup.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/feat_ci.hpp>

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
        &(context.GetScope()));
    if (loc_compare == sequence::eSame) {
        m_Objs[kCDSRNAAnyOverlap][kCDSRNAExactMatch].Add(*context.NewDiscObj(cds_sf), false).Add(*context.NewDiscObj(rna_sf), false).Fatal();
    }
    else if (loc_compare == sequence::eContained) {
        m_Objs[kCDSRNAAnyOverlap][kCDSRNAContainedIn].Add(*context.NewDiscObj(cds_sf), false).Add(*context.NewDiscObj(rna_sf), false).Fatal();
    }
    else if (loc_compare == sequence::eContains) {
        if (rna_sf->GetData().GetSubtype() == CSeqFeatData::eSubtype_tRNA) {
            m_Objs[kCDSRNAAnyOverlap][kCDSRNAContainstRNA].Add(*context.NewDiscObj(cds_sf), false).Add(*context.NewDiscObj(rna_sf), false).Fatal();
        }
        else {
            m_Objs[kCDSRNAAnyOverlap][kCDSRNAContains].Add(*context.NewDiscObj(cds_sf), false).Add(*context.NewDiscObj(rna_sf), false).Fatal();
        }
    }
    else if (loc_compare != sequence::eNoOverlap) {
        ENa_strand cds_strand = cds_sf->GetLocation().GetStrand();
        ENa_strand rna_strand = rna_sf->GetLocation().GetStrand();
        if (cds_strand == eNa_strand_minus && rna_strand != eNa_strand_minus) {
            //ADD_CDS_RNA_TRIPLET(kCDSRNAOverlapNoContain, kCDSRNAOverlapNoContainOppStrand);
            m_Objs[kCDSRNAAnyOverlap][kCDSRNAOverlapNoContain][kCDSRNAOverlapNoContainOppStrand].Add(*context.NewDiscObj(cds_sf), false).Add(*context.NewDiscObj(rna_sf), false).Fatal();
        }
        else if (cds_strand != eNa_strand_minus && rna_strand == eNa_strand_minus) {
            //ADD_CDS_RNA_TRIPLET(kCDSRNAOverlapNoContain, kCDSRNAOverlapNoContainOppStrand);
            m_Objs[kCDSRNAAnyOverlap][kCDSRNAOverlapNoContain][kCDSRNAOverlapNoContainOppStrand].Add(*context.NewDiscObj(cds_sf), false).Add(*context.NewDiscObj(rna_sf), false).Fatal();
        }
        else {
            //ADD_CDS_RNA_TRIPLET(kCDSRNAOverlapNoContain, kCDSRNAOverlapNoContainSameStrand);
            m_Objs[kCDSRNAAnyOverlap][kCDSRNAOverlapNoContain][kCDSRNAOverlapNoContainSameStrand].Add(*context.NewDiscObj(cds_sf), false).Add(*context.NewDiscObj(rna_sf), false).Fatal();
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
    if (IsShortrRNA(obj, &(context.GetScope()))) {
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

    // Are any "reportable" features under this gene?
    CFeat_CI fi(context.GetScope(), obj.GetLocation());
    bool found_reportable = false;
    while (fi && !found_reportable) {
        if (fi->GetData().IsCdregion() ||
            fi->GetData().IsRna()) {
            found_reportable = true;
        }
        ++fi;
    }

    if (found_reportable) {
        return;
    }
    
    if (CCleanup::IsPseudo(obj, context.GetScope())) {
        m_Objs[kExtraGene][kExtraPseudo].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)));
    } else {
        // do not report if note or description
        if (obj.IsSetComment() && !NStr::IsBlank(obj.GetComment())) {
            // ignore genes with notes
        }
        else if (obj.GetData().GetGene().IsSetDesc() && !NStr::IsBlank(obj.GetData().GetGene().GetDesc())) {
            // ignore genes with descriptions
        }
        else {
            m_Objs[kExtraGene][kExtraGeneNonPseudoNonFrameshift].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)));
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


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
