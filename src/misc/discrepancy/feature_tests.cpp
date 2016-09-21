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
#include "utils.hpp"

#include <objects/general/Dbtag.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/RNA_gen.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objtools/cleanup/cleanup.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/tse_handle.hpp>
#include <objtools/edit/cds_fix.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

DISCREPANCY_MODULE(feature_tests);


// PSEUDO_MISMATCH

const string kPseudoMismatch = "[n] CDSs, RNAs, and genes have mismatching pseudos.";

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(PSEUDO_MISMATCH, CSeq_feat_BY_BIOSEQ, eDisc | eOncaller | eSubmitter | eSmart, "Pseudo Mismatch")
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


// DISC_SHORT_RRNA
bool IsShortrRNA(const CSeq_feat& f, CScope* scope);

const string kShortRRNA = "[n] rRNA feature[s] [is] too short";

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(SHORT_RRNA, CSeq_feat_BY_BIOSEQ, eDisc | eOncaller | eSubmitter | eSmart, "Short rRNA Features")
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
DISCREPANCY_CASE(RBS_WITHOUT_GENE, CSeq_feat_BY_BIOSEQ, eOncaller, "RBS features should have an overlapping gene")
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
DISCREPANCY_CASE(MISSING_GENES, CSeq_feat_BY_BIOSEQ, eDisc | eSubmitter | eSmart, "Missing Genes")
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
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// EXTRA_GENES

const string kExtraGene = "[n] gene feature[s] [is] not associated with a CDS or RNA feature.";
const string kExtraPseudo = "[n] pseudo gene feature[s] [is] not associated with a CDS or RNA feature.";
const string kExtraGeneNonPseudoNonFrameshift = "[n] non-pseudo gene feature[s] are not associated with a CDS or RNA feature and [does] not have frameshift in the comment.";

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(EXTRA_GENES, CSeq_feat_BY_BIOSEQ, eDisc | eSubmitter | eSmart, "Extra Genes")
//  ----------------------------------------------------------------------------
{
    // TODO: Do not collect if mRNA sequence in Gen-prod set

    // do not collect if pseudo
    if (!obj.GetData().IsGene()) {
        return;
    }
    bool gene_partial_start = obj.GetLocation().IsPartialStart(eExtreme_Biological);
    bool gene_partial_stop = obj.GetLocation().IsPartialStop(eExtreme_Biological);

    const string& locus = obj.GetData().GetGene().IsSetLocus() ? obj.GetData().GetGene().GetLocus() : kEmptyStr;

    // Are any "reportable" features under this gene?
    CFeat_CI fi(context.GetScope(), obj.GetLocation());
    bool found_reportable = false;
    while (fi && !found_reportable) {
        if (fi->GetData().IsCdregion() ||
            fi->GetData().IsRna()) {

            string ref_gene_locus;
            const CGene_ref* gene_ref = fi->GetGeneXref();
            const CSeq_loc& feature_loc = fi->GetLocation();

            if (gene_ref) {
                ref_gene_locus = gene_ref->IsSetLocus() ? gene_ref->GetLocus() : kEmptyStr;
            }
            else {
                CConstRef<CSeq_feat> gene = sequence::GetBestOverlappingFeat(feature_loc, CSeqFeatData::e_Gene, sequence::eOverlap_Contained, context.GetScope());
                if (gene.NotEmpty()) {
                    ref_gene_locus = (gene->GetData().GetGene().CanGetLocus()) ? gene->GetData().GetGene().GetLocus() : kEmptyStr;
                }
            }

            if (ref_gene_locus.empty() || ref_gene_locus == locus) {

                bool exclude_for_partials = false;

                sequence::ECompare cmp_res = sequence::Compare(obj.GetLocation(), feature_loc, &(context.GetScope()), sequence::fCompareOverlapping);
                bool location_appropriate = cmp_res == sequence::eSame || cmp_res == sequence::eContains;

                if (cmp_res == sequence::eSame) {
                    // check partials
                    if (!gene_partial_start && feature_loc.IsPartialStart(eExtreme_Biological)) {
                        exclude_for_partials = true;
                    }
                    else if (!gene_partial_stop && feature_loc.IsPartialStop(eExtreme_Biological)) {
                        exclude_for_partials = true;
                    }
                }

                if (location_appropriate && !exclude_for_partials) {
                    found_reportable = true;
                }
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
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


//SUPERFLUOUS_GENE
const string kSuperfluousGene = "[n] gene feature[s] [is] not associated with any feature and [is] not pseudo.";

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(SUPERFLUOUS_GENE, CSeq_feat_BY_BIOSEQ, eDisc | eOncaller, "Superfluous Genes")
//  ----------------------------------------------------------------------------
{
    // do not collect if pseudo
    if (!obj.GetData().IsGene()) {
        return;
    }

    // Are any "reportable" features under this gene?
    CFeat_CI fi(context.GetScope(), obj.GetLocation());
    bool found_reportable = false;
    while (fi && !found_reportable) {
        if (!fi->GetData().IsGene()) {
            CConstRef<CSeq_feat> reported_gene = CCleanup::GetGeneForFeature(*(fi->GetSeq_feat()), fi->GetScope());
            if (reported_gene.GetPointer() == &obj) {
                found_reportable = true;
            }
        }
        ++fi;
    }

    if (!found_reportable) {
        m_Objs[kSuperfluousGene].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)));
    }

}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(SUPERFLUOUS_GENE)
//  ----------------------------------------------------------------------------
{
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
DISCREPANCY_CASE(BACTERIAL_PARTIAL_NONEXTENDABLE_PROBLEMS, CSeq_feat_BY_BIOSEQ, eDisc | eSubmitter | eSmart, "Find partial feature ends on bacterial sequences that cannot be extended: on when non-eukaryote")
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
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


const string kBacterialPartialNonextendableException = "[n] feature[s] [has] partial ends that do not abut the end of the sequence or a gap, and cannot be extended by 3 or fewer nucleotides to do so, but [has] the correct exception";
//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(BACTERIAL_PARTIAL_NONEXTENDABLE_EXCEPTION, CSeq_feat_BY_BIOSEQ, eDisc | eSubmitter | eSmart, "Find partial feature ends on bacterial sequences that cannot be extended but have exceptions: on when non-eukaryote")
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
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


const string kPartialProblems = "[n] feature[s] [has] partial ends that do not abut the end of the sequence or a gap, but could be extended by 3 or fewer nucleotides to do so";
//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(PARTIAL_PROBLEMS, CSeq_feat_BY_BIOSEQ, eDisc | eOncaller | eSubmitter | eSmart, "Find partial feature ends on bacterial sequences that cannot be extended but have exceptions: on when non-eukaryote")
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
            if (IsExtendableRight(stop, *seq, &(context.GetScope()), extend_len)) {
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
DISCREPANCY_CASE(EUKARYOTE_SHOULD_HAVE_MRNA, CSeq_feat_BY_BIOSEQ, eDisc | eSubmitter | eSmart, "Eukaryote should have mRNA")
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
DISCREPANCY_CASE(NON_GENE_LOCUS_TAG, CSeq_feat, eDisc | eOncaller | eSubmitter | eSmart, "Nongene Locus Tag")
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
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// FIND_BADLEN_TRNAS
const string ktRNATooShort = "[n] tRNA[s] [is] too short";
const string ktRNATooLong = "[n] tRNA[s] [is] too long";
//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(FIND_BADLEN_TRNAS, CSeq_feat, eDisc | eOncaller | eSubmitter | eSmart, "Find short and long tRNAs")
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
DISCREPANCY_CASE(GENE_PARTIAL_CONFLICT, CSeq_feat_BY_BIOSEQ, eOncaller | eSubmitter | eSmart, "Feature partialness should agree with gene partialness if endpoints match")
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
                    bool found_utr5 = false;
                    for (CFeat_CI fi(bsh, CSeqFeatData::eSubtype_5UTR); fi; ++fi) {
                        found_utr5 = true;
                        if (fi->GetLocation().GetStart(eExtreme_Biological) == gene_start) {
                            found_start = true;
                            break;
                        }
                    }
                    if (found_utr5 && !found_start) {
                        conflict_start = true;
                    }
                }
                if (!conflict_stop) {
                    //look for 3' UTR
                    TSeqPos gene_stop = gene->GetLocation().GetStop(eExtreme_Biological);
                    bool found_stop = false;
                    bool found_utr3 = false;
                    for (CFeat_CI fi(bsh, CSeqFeatData::eSubtype_3UTR); fi; ++fi) {
                        found_utr3 = true;
                        if (fi->GetLocation().GetStop(eExtreme_Biological) == gene_stop) {
                            found_stop = true;
                            break;
                        }
                    }
                    if (found_utr3 && !found_stop) {
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
DISCREPANCY_CASE(BAD_GENE_STRAND, CSeq_feat_BY_BIOSEQ, eOncaller | eSubmitter | eSmart, "Genes and features that share endpoints should be on the same strand")
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
DISCREPANCY_CASE(CDS_HAS_NEW_EXCEPTION, CSeq_feat, eDisc | eOncaller | eSmart, "Coding region has new exception")
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
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


// SHORT_LNCRNA

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(SHORT_LNCRNA, CSeq_feat, eDisc | eOncaller | eSubmitter | eSmart, "Short lncRNA sequences")
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
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


// JOINED_FEATURES

const string& kJoinedFeatures = "[n] feature[s] [has] joined location[s].";
const string& kJoinedFeaturesNoException = "[n] feature[s] [has] joined location but no exception";
const string& kJoinedFeaturesException = "[n] feature[s] [has] joined location but exception '";
const string& kJoinedFeaturesBlankException = "[n] feature[s] [has] joined location but a blank exception";

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(JOINED_FEATURES, CSeq_feat_BY_BIOSEQ, eDisc | eSubmitter | eSmart, "Joined Features: on when non-eukaryote")
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
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


// SHORT_INTRON
const string kShortIntronTop = "[n] intron[s] [is] shorter than 10 nt";
const string kShortIntronExcept = "[n] intron[s] [is] shorter than 11 nt and [has] an exception";

//"Introns shorter than 10 nt", "DISC_SHORT_INTRON", FindShortIntrons, AddExceptionsToShortIntrons},

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(SHORT_INTRON, CSeq_feat, eDisc | eOncaller | eSubmitter | eSmart, "Introns shorter than 10 nt")
//  ----------------------------------------------------------------------------
{
    if (!obj.IsSetData() || !obj.GetData().IsCdregion() || !obj.IsSetLocation() || obj.IsSetExcept()) {
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
            m_Objs[kShortIntronTop][kShortIntronExcept].Ext().Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj), eKeepRef, true), false);
        } else {
            m_Objs[kShortIntronTop].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj), eKeepRef, true), false);
        }
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(SHORT_INTRON)
//  ----------------------------------------------------------------------------
{
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


static const string kPutativeFrameShift = "putative frameshift";

static void AddException(const CSeq_feat& sf, CScope& scope, const string& exception_text)
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

static void AdjustBacterialGeneForCodingRegionWithShortIntron(CSeq_feat& sf, CSeq_feat& gene, bool is_bacterial)
{
    if (sf.IsSetComment() && !NStr::IsBlank(sf.GetComment())) {
        if (gene.IsSetComment() && !NStr::IsBlank(gene.GetComment())) {
            gene.SetComment(sf.GetComment() + ';' + gene.GetComment());
        }
        else {
            gene.ResetComment();

            if (sf.IsSetComment()) {
                gene.SetComment(sf.GetComment());
                sf.ResetComment();
            }

            if (is_bacterial) {
                sf.SetComment("contains short intron that may represent a frameshift");
            }
        }
    }

    if (!gene.IsSetComment() || NStr::Find(gene.GetComment(), kPutativeFrameShift) == NPOS) {
        if (gene.IsSetComment() && !NStr::IsBlank(gene.GetComment())) {
            gene.SetComment(kPutativeFrameShift + ';' + gene.GetComment());
        }
        else {
            gene.ResetComment();

            if (sf.IsSetComment()) {
                gene.SetComment(sf.GetComment());
                sf.ResetComment();
            }

            if (is_bacterial) {
                sf.SetComment("contains short intron that may represent a frameshift");
            }
        }
    }
}

static void ConvertToMiscFeature(CSeq_feat& sf, CScope& scope)
{
    if (sf.IsSetData()) {

        if (sf.GetData().IsCdregion() || sf.GetData().IsRna()) {

            string prod_name;
            if (sf.GetData().IsCdregion()) {
                prod_name = GetProductName(sf, scope);
                sf.ResetProduct();
            }
            else {
                prod_name = sf.GetData().GetRna().GetRnaProductName();
            }

            if (!NStr::IsBlank(prod_name)) {
                if (sf.IsSetComment()) {
                    sf.SetComment(prod_name + ';' + sf.GetComment());
                }
                else {
                    sf.SetComment(prod_name);
                }
            }

            sf.ResetData();
            sf.SetData().SetImp().SetKey("misc_feature");
        }
    }
}

static bool AddExceptionsToShortIntron(const CSeq_feat& sf, CScope& scope, std::list<CConstRef<CSeq_loc>>& to_remove)
{
    bool rval = false;

    const CBioSource* source = nullptr;

    {
        CBioseq_Handle bsh;
        try {
            bsh = scope.GetBioseqHandle(sf.GetLocation());
        }
        catch (CException&) {
            return false;
        }
        CSeqdesc_CI src(bsh, CSeqdesc::e_Source);
        if (src) {
            source = &src->GetSource();
        }
    }

    if (source) {
        if (source->IsSetGenome() && source->GetGenome() == CBioSource::eGenome_mitochondrion) {
            return false;
        }
        
        bool is_bacterial = CDiscrepancyContext::HasLineage(*source, "", "Bacteria");
        if (is_bacterial || CDiscrepancyContext::HasLineage(*source, "", "Archea")) {

            CConstRef<CSeq_feat> gene = CCleanup::GetGeneForFeature(sf, scope);
            if (gene.NotEmpty()) {
                
                CSeq_feat* gene_edit = const_cast<CSeq_feat*>(gene.GetPointer());
                CSeq_feat& sf_edit = const_cast<CSeq_feat&>(sf);

                rval = true;

                gene_edit->SetPseudo(true);

                AdjustBacterialGeneForCodingRegionWithShortIntron(sf_edit, *gene_edit, is_bacterial);

                // Merge gene's location
                if (gene_edit->IsSetLocation()) {
                    CRef<CSeq_loc> new_loc = gene_edit->SetLocation().Merge(CSeq_loc::fMerge_All, nullptr);
                    if (new_loc.NotEmpty()) {
                        gene_edit->SetLocation().Assign(*new_loc);
                    }
                }

                if (sf.IsSetProduct()) {
                    
                    to_remove.push_back(CConstRef<CSeq_loc>(&sf.GetProduct()));
                }

                if (is_bacterial) {
                    ConvertToMiscFeature(sf_edit, scope);
                }
                else {
                    CSeq_feat_EditHandle sf_handle(scope.GetSeq_featHandle(sf));
                    sf_handle.Remove();
                }
            }

            return rval;
        }
    }

    if (!sf.IsSetExcept_text() || NStr::Find(sf.GetExcept_text(), "low-quality sequence region") == string::npos) {
        AddException(sf, scope, "low-quality sequence region");
        rval = true;
    }
    return rval;
}


DISCREPANCY_AUTOFIX(SHORT_INTRON)
{
    TReportObjectList list = item->GetDetails();
    unsigned int n = 0;

    std::list<CConstRef<CSeq_loc>> to_remove;

    NON_CONST_ITERATE(TReportObjectList, it, list) {
        const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer())->GetObject().GetPointer());
        if (sf && AddExceptionsToShortIntron(*sf, scope, to_remove)) {
            n++;
        }
    }

    ITERATE(std::list<CConstRef<CSeq_loc>>, loc, to_remove) {

        CBioseq_Handle bioseq_h = scope.GetBioseqHandle(**loc);
        CBioseq_EditHandle bioseq_edit = bioseq_h.GetEditHandle();
        bioseq_edit.Remove();
    }

    return CRef<CAutofixReport>(n ? new CAutofixReport("SHORT_INTRON: Set exception for [n] feature[s]", n) : 0);
}


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
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


// CDS_HAS_CDD_XREF
//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(CDS_HAS_CDD_XREF, CSeq_feat, eDisc | eOncaller, "CDS has CDD Xref")
//  ----------------------------------------------------------------------------
{
    if (!obj.IsSetData() || !obj.GetData().IsCdregion() || !obj.IsSetDbxref()) {
        return;
    }
    bool has_cdd_xref = false;
    ITERATE(CSeq_feat::TDbxref, x, obj.GetDbxref()) {
        if ((*x)->IsSetDb() && NStr::EqualNocase((*x)->GetDb(), "CDD")) {
            has_cdd_xref = true;
            break;
        }
    }

    if (has_cdd_xref) {
        m_Objs["[n] feature[s] [has] CDD Xrefs"].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)), false);
    }

}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(CDS_HAS_CDD_XREF)
//  ----------------------------------------------------------------------------
{
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


// SHOW_TRANSL_EXCEPT

static const string kShowTranslExc = "[n] coding region[s] [has] a translation exception";

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(SHOW_TRANSL_EXCEPT, CSeq_feat, eDisc | eSubmitter | eSmart, "Show translation exception")
//  ----------------------------------------------------------------------------
{
    if (!obj.IsSetData() || !obj.GetData().IsCdregion()) {
        return;
    }

    if (obj.GetData().GetCdregion().IsSetCode_break()) {
        m_Objs[kShowTranslExc].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)), false);
    }

}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(SHOW_TRANSL_EXCEPT)
//  ----------------------------------------------------------------------------
{
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


// NO_PRODUCT_STRING

static const string kNoProductStr = "[n] product[s] [has] \"no product string in file\"";

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(NO_PRODUCT_STRING, CSeq_feat, eDisc, "Product has string \"no product string in file\"")
//  ----------------------------------------------------------------------------
{
    if (!obj.IsSetData() || !obj.GetData().IsProt()) {
        return;
    }

    const CProt_ref& prot = obj.GetData().GetProt();

    if (prot.IsSetName()) {

        const string* no_prot_str = NStr::FindNoCase(prot.GetName(), "no product string in file");
        if (no_prot_str != nullptr) {
            const CSeq_feat* product = sequence::GetCDSForProduct(*context.GetCurrentBioseq(), &context.GetScope());
            if (product != nullptr) {
                m_Objs[kNoProductStr].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(product)), false);
            }
        }
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(NO_PRODUCT_STRING)
//  ----------------------------------------------------------------------------
{
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


// DISC_SHORT_RRNA
//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(UNWANTED_SPACER, CSeq_feat_BY_BIOSEQ, eOncaller, "Intergenic spacer without plastid location")
//  ----------------------------------------------------------------------------
{
    if (!obj.IsSetData() || obj.GetData().GetSubtype() != CSeqFeatData::eSubtype_misc_feature ||
        !obj.IsSetComment()) {
        return;
    }
    static string kIntergenicSpacerNames[] = {
        "trnL-trnF intergenic spacer",
        "trnH-psbA intergenic spacer",
        "trnS-trnG intergenic spacer",
        "trnF-trnL intergenic spacer",
        "psbA-trnH intergenic spacer",
        "trnG-trnS intergenic spacer"};

    size_t num_names = sizeof(kIntergenicSpacerNames) / sizeof(string);
    bool found = false;
    for (size_t i = 0; i < num_names && !found; i++) {
        if (NStr::FindNoCase(obj.GetComment(), kIntergenicSpacerNames[i]) != string::npos) {
            found = true;
        }
    }

    if (!found) {
        return;
    }

    const CBioSource* src = context.GetCurrentBiosource();
    if (src && src->IsSetGenome() &&
        (src->GetGenome() == CBioSource::eGenome_chloroplast || src->GetGenome() == CBioSource::eGenome_plastid)) {
        return;
    }
    if (src && src->IsSetOrg() && src->GetOrg().IsSetTaxname() &&
        CDiscrepancyContext::IsUnculturedNonOrganelleName(src->GetOrg().GetTaxname())) {
        return;
    }

    m_Objs["[n] suspect intergenic spacer note[s] not organelle"].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)), false);
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(UNWANTED_SPACER)
//  ----------------------------------------------------------------------------
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// CHECK_RNA_PRODUCTS_AND_COMMENTS

static const string kFeatureHasWeirdStr = "[n] RNA product_name or comment[s] contain[S] 'suspect phrase'";

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(CHECK_RNA_PRODUCTS_AND_COMMENTS, CSeq_feat, eOncaller, "Check for gene or genes in rRNA and tRNA products and comments")
//  ----------------------------------------------------------------------------
{
    if (!obj.IsSetData() || !obj.GetData().IsRna()) {
        return;
    }

    const CRNA_ref& rna = obj.GetData().GetRna();
    if (rna.IsSetType() && rna.GetType() != CRNA_ref::eType_rRNA && rna.GetType() != CRNA_ref::eType_tRNA) {
        return;
    }

    string product = rna.GetRnaProductName();
    string comment;
    if (obj.IsSetComment()) {
        comment = obj.GetComment();
    }

    if (NStr::FindNoCase(product, "gene") != NPOS || NStr::FindNoCase(comment, "gene") != NPOS) {
        m_Objs[kFeatureHasWeirdStr].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)), false);
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(CHECK_RNA_PRODUCTS_AND_COMMENTS)
//  ----------------------------------------------------------------------------
{
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


// FEATURE_LOCATION_CONFLICT

const string kFeatureLocationConflictTop = "[n] feature[s] [has] inconsistent gene location[s].";
const string kFeatureLocationCodingRegion = "Coding region location does not match gene location";
const string kFeatureLocationRNA = "RNA feature location does not match gene location";

bool IsMixedStrand(const CSeq_loc& loc)
{
    CSeq_loc_CI li(loc);
    if (!li) {
        return false;
    }
    ENa_strand first_strand = li.GetStrand();
    if (first_strand == eNa_strand_unknown) {
        first_strand = eNa_strand_plus;
    }
    ++li;
    while (li) {
        ENa_strand this_strand = li.GetStrand();
        if (this_strand == eNa_strand_unknown) {
            this_strand = eNa_strand_plus;
        }
        if (this_strand != first_strand) {
            return true;
        }
        ++li;
    }
    return false;
}


bool IsMixedStrandGeneLocationOk(const CSeq_loc& feat_loc, const CSeq_loc& gene_loc)
{
    CSeq_loc_CI feat_i(feat_loc);
    CSeq_loc_CI gene_i(gene_loc);

    while (feat_i && gene_i) {
        if (!StrandsMatch(feat_i.GetStrand(), gene_i.GetStrand()) ||
            feat_i.GetRangeAsSeq_loc()->GetStart(eExtreme_Biological) != gene_i.GetRangeAsSeq_loc()->GetStart(eExtreme_Biological)) {
            return false;
        }
        bool found_stop = false;
        while (!found_stop && feat_i && StrandsMatch(feat_i.GetStrand(), gene_i.GetStrand())) {
            if (feat_i.GetRangeAsSeq_loc()->GetStop(eExtreme_Biological) == gene_i.GetRangeAsSeq_loc()->GetStop(eExtreme_Biological)) {
                found_stop = true;
            }
            ++feat_i;
        }
        if (!found_stop) {
            return false;
        }
        ++gene_i;
    }
    if ((feat_i && !gene_i) || (!feat_i && gene_i)) {
        return false;
    }

    return true;
}


bool StopAbutsGap(const CSeq_loc& loc, CScope& scope)
{
    try {
        CBioseq_Handle bsh = scope.GetBioseqHandle(loc);
        TSeqPos stop = loc.GetStop(eExtreme_Biological);
        if (stop < 2 || stop > bsh.GetBioseqLength() - 3) {
            return false;
        }
        CRef<CSeq_loc> search(new CSeq_loc());
        search->SetInt().SetId().Assign(*(loc.GetId()));
        if (loc.GetStrand() == eNa_strand_minus) {
            search->SetInt().SetFrom(stop - 1);
            search->SetInt().SetTo(stop - 2);
            search->SetInt().SetStrand(eNa_strand_minus);
        } else {
            search->SetInt().SetFrom(stop + 1);
            search->SetInt().SetTo(stop + 2);
        }
        CSeqVector vec(*search, scope);
        if (vec.IsInGap(0)) {
            return true;
        }
    } catch (CException& ) {
        // unable to calculate
    }
    return false;
}


bool StartAbutsGap(const CSeq_loc& loc, CScope& scope)
{
    try {
        CBioseq_Handle bsh = scope.GetBioseqHandle(loc);
        TSeqPos start = loc.GetStart(eExtreme_Biological);
        if (start < 2 || start > bsh.GetBioseqLength() - 3) {
            return false;
        }
        CRef<CSeq_loc> search(new CSeq_loc());
        search->SetInt().SetId().Assign(*(loc.GetId()));
        if (loc.GetStrand() == eNa_strand_minus) {
            search->SetInt().SetFrom(start + 1);
            search->SetInt().SetTo(start + 2);
            search->SetInt().SetStrand(eNa_strand_minus);
        } else {
            search->SetInt().SetFrom(start - 1);
            search->SetInt().SetTo(start - 2);
        }
        CSeqVector vec(*search, scope);
        if (vec.IsInGap(0)) {
            return true;
        }
    } catch (CException& ) {
        // unable to calculate
    }
    return false;
}


// location is ok if:
// 1. endpoints match exactly, or 
// 2. non-matching 5' endpoint can be extended by an RBS feature to match gene start, or
// 3. if coding region non-matching endpoints are partial and abut a gap
bool IsGeneLocationOk(const CSeq_loc& feat_loc, const CSeq_loc& gene_loc, bool is_coding_region, CScope& scope)
{
    if (IsMixedStrand(feat_loc) || IsMixedStrand(gene_loc)) {
        // special handling for trans-spliced
        return IsMixedStrandGeneLocationOk(feat_loc, gene_loc);
    } else if (!StrandsMatch(feat_loc.GetStrand(), gene_loc.GetStrand())) {
        return false;
    } else if (gene_loc.GetStop(eExtreme_Biological) != feat_loc.GetStop(eExtreme_Biological)) {
        if (is_coding_region && feat_loc.IsPartialStop(eExtreme_Biological) && StopAbutsGap(feat_loc, scope)) {
            // ignore for now
        } else {
            return false;
        }
    } 
    TSeqPos gene_start = gene_loc.GetStart(eExtreme_Biological);
    TSeqPos feat_start = feat_loc.GetStart(eExtreme_Biological);

    if (gene_start == feat_start) {
        return true;
    }

    CRef<CSeq_loc> rbs_search(new CSeq_loc());
    const CSeq_id* id = gene_loc.GetId();
    if (!id) {
        return false;
    }
    rbs_search->SetInt().SetId().Assign(*id);
    if (feat_loc.GetStrand() == eNa_strand_minus) {
        if (gene_start < feat_start) {
            return false;
        }
        rbs_search->SetInt().SetFrom(feat_start + 1);
        rbs_search->SetInt().SetTo(gene_start);
        rbs_search->SetStrand(eNa_strand_minus);
    } else {
        if (gene_start > feat_start) {
            return false;
        }
        rbs_search->SetInt().SetFrom(gene_start);
        rbs_search->SetInt().SetTo(feat_start - 1);
    }
    
    CFeat_CI rbs(scope, *rbs_search, SAnnotSelector(CSeqFeatData::eSubtype_regulatory));
    while (rbs) {
        // interval must match
        // must be actual RBS feature
        if (IsRBS(*(rbs->GetSeq_feat())) &&
            rbs->GetLocation().GetStart(eExtreme_Positional) == rbs_search->GetStart(eExtreme_Positional) &&
            rbs->GetLocation().GetStart(eExtreme_Positional) == rbs_search->GetStart(eExtreme_Positional)) {
            return true;
        }
        ++rbs;
    }
    if (is_coding_region && feat_loc.IsPartialStart(eExtreme_Biological) && StartAbutsGap(feat_loc, scope)) {
        // check to see if 5' end is partial and abuts gap
        return true;
    }
    return false;
}

static string GetNextSubitemId(size_t num)
{
    string ret = "[*";
    ret += NStr::SizetToString(num);
    ret += "*]";
    return ret;
}

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(FEATURE_LOCATION_CONFLICT, CSeq_feat_BY_BIOSEQ, eDisc | eSubmitter | eSmart, "Feature Location Conflict")
//  ----------------------------------------------------------------------------
{
    if (!obj.IsSetData() || context.IsCurrentRnaInGenProdSet() || !obj.IsSetLocation()) {
        return;
    }
    // handle RNA features always, coding regions only if not eukaryotic
    bool handle_feat = false;
    if (obj.GetData().IsRna()) {
        handle_feat = true;
    } else if (!context.IsEukaryotic() && obj.GetData().IsCdregion()) {
        handle_feat = true;
    }
    if (!handle_feat) {
        return;
    }

    const CGene_ref* gx = obj.GetGeneXref();
    if (gx) {
        if (!gx->IsSuppressed()) {
            
            CBioseq_Handle bsh = context.GetScope().GetBioseqHandle(*(context.GetCurrentBioseq()));
            CTSE_Handle tse_hl = bsh.GetTSE_Handle();
            CTSE_Handle::TSeq_feat_Handles gene_candidates = tse_hl.GetGenesByRef(*gx);

            bool found_match = false;
            ITERATE(CTSE_Handle::TSeq_feat_Handles, it, gene_candidates) {
                if (StrandsMatch((*it).GetLocation().GetStrand(), obj.GetLocation().GetStrand())) {
                    if (IsGeneLocationOk(obj.GetLocation(), (*it).GetLocation(), obj.GetData().IsCdregion(), context.GetScope())) {
                        found_match = true;
                    } else {

                        string subitem_id = GetNextSubitemId(m_Objs[kFeatureLocationConflictTop].GetMap().size());

                        if (obj.GetData().IsCdregion()) {
                            m_Objs[kFeatureLocationConflictTop]
                                [kFeatureLocationCodingRegion + subitem_id]
                            .Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)), false)
                            .Add(*context.NewDiscObj(it->GetSeq_feat()), false).Ext();
                        } else {
                            m_Objs[kFeatureLocationConflictTop]
                                [kFeatureLocationRNA + subitem_id]
                            .Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)), false)
                                .Add(*context.NewDiscObj(it->GetSeq_feat()), false).Ext();
                        }
                    }

                    m_Objs[kFeatureLocationConflictTop].Incr();
                }
            }

            if (!found_match) {
                if (obj.GetData().IsCdregion()) {
                    m_Objs[kFeatureLocationConflictTop]
                        ["Coding region xref gene does not exist"]
                    .Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)), false);
                } else {
                    m_Objs[kFeatureLocationConflictTop]
                        ["RNA feature xref gene does not exist"]
                    .Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)), false);
                }
                m_Objs[kFeatureLocationConflictTop].Incr();
            }
        }
    } else {
        const CSeq_feat* gene = context.GetCurrentGene();
        if (gene && gene->IsSetLocation()) {
            if (!IsGeneLocationOk(obj.GetLocation(), gene->GetLocation(), obj.GetData().IsCdregion(), context.GetScope())) {

                string subitem_id = GetNextSubitemId(m_Objs[kFeatureLocationConflictTop].GetMap().size());

                if (obj.GetData().IsCdregion()) {
                    m_Objs[kFeatureLocationConflictTop]
                        [kFeatureLocationCodingRegion + subitem_id]
                    .Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)), false)
                        .Add(*context.NewDiscObj(CConstRef<CSeq_feat>(gene)), false).Ext();
                } else {
                    m_Objs[kFeatureLocationConflictTop]
                        [kFeatureLocationRNA + subitem_id]
                    .Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)), false)
                        .Add(*context.NewDiscObj(CConstRef<CSeq_feat>(gene)), false).Ext();
                }

                m_Objs[kFeatureLocationConflictTop].Incr();
            }
        }
    }

}

//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(FEATURE_LOCATION_CONFLICT)
//  ----------------------------------------------------------------------------
{
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


// SUSPECT_PHRASES

const string suspect_phrases[] =
{
    "fragment",
    "frameshift",
    "%",
    "E-value",
    "E value",
    "Evalue",
    "..."
};


//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(SUSPECT_PHRASES, CSeq_feat, eDisc | eSubmitter | eSmart, "Suspect Phrases")
//  ----------------------------------------------------------------------------
{
    if (!obj.IsSetData()) {
        return;
    }
    string check = kEmptyStr;
    if (obj.GetData().IsCdregion() && obj.IsSetComment()) {
        check = obj.GetComment();
    } else if (obj.GetData().IsProt() && obj.GetData().GetProt().IsSetDesc()) {
        check = obj.GetData().GetProt().GetDesc();
    }
    if (NStr::IsBlank(check)) {
        return;
    }

    bool found = false;
    for (size_t i = 0; i < sizeof(suspect_phrases) / sizeof(string); i++) {
        if (NStr::FindNoCase(check, suspect_phrases[i]) != string::npos) {
            m_Objs["[n] cds comment[s] or protein description[s] contain[S] suspect_phrase[s]"]
                ["[n] cds comment[s] or protein description[s] contain[S] '" + suspect_phrases[i] + "'"].Summ().Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)), false);
            break;
        }
    }

}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(SUSPECT_PHRASES)
//  ----------------------------------------------------------------------------
{
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


// UNUSUAL_MISC_RNA

const string kUnusualMiscRNA = "[n] unexpected misc_RNA feature[s] found.  misc_RNAs are unusual in a genome, consider using ncRNA, misc_binding, or misc_feature as appropriate";

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(UNUSUAL_MISC_RNA, CSeq_feat, eDisc | eSubmitter | eSmart, "Unexpected misc_RNA features")
//  ----------------------------------------------------------------------------
{
    if (obj.IsSetData() && obj.GetData().GetSubtype() == CSeqFeatData::eSubtype_otherRNA) {

        const CRNA_ref& rna = obj.GetData().GetRna();
        string product = rna.GetRnaProductName();

        if (NStr::FindCase(product, "ITS", 0) == NPOS && NStr::FindCase(product, "internal transcribed spacer", 0) == NPOS) {
            m_Objs[kUnusualMiscRNA].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)), false);
        }
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(UNUSUAL_MISC_RNA)
//  ----------------------------------------------------------------------------
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// CDS_WITHOUT_MRNA

const string kCDSWithoutMRNA = "[n] coding region[s] [does] not have an mRNA";

static bool IsProductMatch(const string& rna_product, const string& cds_product)
{
    if (rna_product.empty() || cds_product.empty()) {
        return false;
    }

    if (rna_product == cds_product) {
        return true;
    }

    const string kmRNAVariant = ", transcript variant ";
    const string kCDSVariant = ", isoform ";

    size_t pos_in_rna = rna_product.find(kmRNAVariant);
    size_t pos_in_cds = cds_product.find(kCDSVariant);

    if (pos_in_rna == string::npos || pos_in_cds == string::npos || pos_in_rna != pos_in_cds ||
        !NStr::EqualCase(rna_product, 0, pos_in_rna, cds_product)) {
        return false;
    }

    string rna_rest = rna_product.substr(pos_in_rna + kmRNAVariant.size()),
        cds_rest = cds_product.substr(pos_in_cds + kCDSVariant.size());
    return rna_rest == cds_rest;
}

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(CDS_WITHOUT_MRNA, CSeq_feat, eDisc | eOncaller | eSmart, "Coding regions on eukaryotic genomic DNA should have mRNAs with matching products")
//  ----------------------------------------------------------------------------
{
    if (obj.IsSetData() && obj.GetData().IsCdregion() && !CCleanup::IsPseudo(obj, context.GetScope()) &&
        context.IsEukaryotic() && context.IsDNA()) {

        const CBioSource* bio_src = context.GetCurrentBiosource();
        if (bio_src && bio_src->IsSetGenome() && !context.IsOrganelle()) {

            CConstRef<CSeq_feat> mRNA = sequence::GetOverlappingmRNA(obj.GetLocation(), context.GetScope());
            if (mRNA.Empty()) {
                m_Objs[kCDSWithoutMRNA].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj), eKeepRef, true), false);
            }
            else {
                const CRNA_ref& rna = mRNA->GetData().GetRna();

                if (!IsProductMatch(rna.GetRnaProductName(), GetProductForCDS(obj, context.GetScope()))) {
                    m_Objs[kCDSWithoutMRNA].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj), eKeepRef, true), false);
                }
            }
        }
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(CDS_WITHOUT_MRNA)
//  ----------------------------------------------------------------------------
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}

static bool AddmRNAForCDS(const CSeq_feat& cds, CScope& scope)
{
    CConstRef<CSeq_feat> old_mRNA = sequence::GetmRNAforCDS(cds, scope);
    CRef<CSeq_feat> new_mRNA = edit::MakemRNAforCDS(cds, scope);

    if (old_mRNA.Empty()) {
        CSeq_feat_EditHandle cds_edit_handle(scope.GetSeq_featHandle(cds));
        CSeq_annot_EditHandle annot_handle = cds_edit_handle.GetAnnot();
        annot_handle.AddFeat(*new_mRNA);
    }
    else
    {
        CSeq_feat_EditHandle old_mRNA_edit(scope.GetSeq_featHandle(*old_mRNA));
        old_mRNA_edit.Replace(*new_mRNA);
    }
    return true;
}

DISCREPANCY_AUTOFIX(CDS_WITHOUT_MRNA)
{
    TReportObjectList list = item->GetDetails();
    unsigned int n = 0;
    NON_CONST_ITERATE(TReportObjectList, it, list) {
        const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer())->GetObject().GetPointer());
        if (sf && AddmRNAForCDS(*sf, scope)) {
            n++;
        }
    }
    return CRef<CAutofixReport>(n ? new CAutofixReport("CDS_WITHOUT_MRNA: Add mRNA for [n] CDS feature[s]", n) : 0);
}


// PROTEIN_NAMES

static const string kProteinNames = "All proteins have same name ";
static const string kProteins = "Proteins";

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(PROTEIN_NAMES, CSeq_feat, eDisc | eSubmitter | eSmart, "Frequently appearing proteins")
//  ----------------------------------------------------------------------------
{
    if (obj.IsSetData() && obj.GetData().IsProt()) {

        const CProt_ref& prot = obj.GetData().GetProt();
        if (prot.IsSetName() && !prot.GetName().empty()) {

            CConstRef<CBioseq> bioseq = context.GetCurrentBioseq();

            // const cast is possible here because bioseq will not be changed
            m_Objs[kProteins].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj), eKeepRef), false);
        }
    }
}

typedef list<CConstRef<CSeq_feat>> TSeqFeatList;
typedef map<string, TSeqFeatList> TProteinMap;

static void CollectProteins(TReportObjectList& prot_feats, TProteinMap& proteins)
{
    NON_CONST_ITERATE(TReportObjectList, prots, prot_feats) {

        CDiscrepancyObject* dobj = dynamic_cast<CDiscrepancyObject*>(prots->GetNCPointer());
        const CSeq_feat* cur_feat = dynamic_cast<const CSeq_feat*>(dobj->GetObject().GetPointer());

        if (cur_feat) {

            string first_name = cur_feat->GetData().GetProt().GetName().front();
            proteins[first_name].push_back(CConstRef<CSeq_feat>(cur_feat));
        }
    }
}

//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(PROTEIN_NAMES)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }

    TProteinMap proteins;
    CollectProteins(m_Objs[kProteins].GetObjects(), proteins);

    static const size_t MIN_REPORTABLE_AMOUNT = 100;

    if (proteins.size() == 1) {

        const TSeqFeatList& feats = proteins.begin()->second;
        const string& name = proteins.begin()->first;

        if (feats.size() >= MIN_REPORTABLE_AMOUNT) {

            CReportNode report;
            string subcat = kProteinNames + '\"' + name + '\"';
            report[subcat];

            m_ReportItems = report.Export(*this)->GetSubitems();
        }
    }
}


// MRNA_SHOULD_HAVE_PROTEIN_TRANSCRIPT_IDS

static const string kNoLackOfmRnaData = "[n] mRNA[s] [does] not have both protein_id and transcript_id";

static bool IsmRnaQualsPresent(const CSeq_feat::TQual& quals)
{
    bool protein_id = false,
         transcript_id = false;

    ITERATE(CSeq_feat::TQual, qual, quals) {
        if ((*qual)->IsSetQual()) {

            if ((*qual)->GetQual() == "orig_protein_id") {
                protein_id = true;
            }

            if ((*qual)->GetQual() == "orig_transcript_id") {
                transcript_id = true;
            }

            if (protein_id && transcript_id) {
                break;
            }
        }
    }

    return protein_id && transcript_id;
}

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(MRNA_SHOULD_HAVE_PROTEIN_TRANSCRIPT_IDS, CSeq_feat, eDisc | eSubmitter | eSmart, "mRNA should have both protein_id and transcript_id")
//  ----------------------------------------------------------------------------
{
    if (obj.IsSetData() && obj.GetData().IsCdregion() && !CCleanup::IsPseudo(obj, context.GetScope()) &&
        context.IsEukaryotic() && context.IsDNA()) {

        const CBioSource* bio_src = context.GetCurrentBiosource();
        if (bio_src) {

            CConstRef<CSeq_feat> mRNA = sequence::GetmRNAforCDS(obj, context.GetScope());
            if (mRNA.NotEmpty()) {

                if (mRNA->IsSetQual() && !IsmRnaQualsPresent(mRNA->GetQual())) {
                
                    m_Objs[kNoLackOfmRnaData].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)), false).Fatal();
                }
            }
        }
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(MRNA_SHOULD_HAVE_PROTEIN_TRANSCRIPT_IDS)
//  ----------------------------------------------------------------------------
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// FEATURE_LIST

static const string kFeatureList = "Feature List";

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(FEATURE_LIST, CSeq_feat, eDisc, "Feature List")
//  ----------------------------------------------------------------------------
{
    if (obj.IsSetData()) {
        
        CSeqFeatData::ESubtype subtype = obj.GetData().GetSubtype();

        if (subtype != CSeqFeatData::eSubtype_gap && subtype != CSeqFeatData::eSubtype_prot) {
            string subitem = "[n] " + obj.GetData().GetKey();
            subitem += " feature[s]";
            m_Objs[kFeatureList][subitem].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)), false);
        }
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(FEATURE_LIST)
//  ----------------------------------------------------------------------------
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}



// MULTIPLE_QUALS

static const string kMultiQuals = "[n] feature[s] contain[S] multiple /number qualifiers";

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(MULTIPLE_QUALS, CSeq_feat, eDisc | eOncaller, "Multiple qualifiers")
//  ----------------------------------------------------------------------------
{
    if (obj.IsSetQual()) {

        size_t num_of_number_quals = 0;
        ITERATE(CSeq_feat::TQual, qual, obj.GetQual()) {
            if ((*qual)->IsSetQual() && (*qual)->GetQual() == "number") {
                ++num_of_number_quals;
                if (num_of_number_quals > 1) {
                    break;
                }
            }
        }

        if (num_of_number_quals > 1) {
            m_Objs[kMultiQuals].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)), false);
        }
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(MULTIPLE_QUALS)
//  ----------------------------------------------------------------------------
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}



// MISC_FEATURE_WITH_PRODUCT_QUAL

static const string kMiscFeatWithProduct = "[n] feature[s] [has] a product qualifier";

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(MISC_FEATURE_WITH_PRODUCT_QUAL, CSeq_feat, eDisc | eOncaller | eSubmitter | eSmart, "Misc features containing a product qualifier")
//  ----------------------------------------------------------------------------
{
    if (obj.IsSetData() && obj.IsSetQual()) {

        CSeqFeatData::ESubtype subtype = obj.GetData().GetSubtype();

        if (subtype == CSeqFeatData::eSubtype_misc_feature) {

            ITERATE(CSeq_feat::TQual, qual, obj.GetQual()) {
                if ((*qual)->IsSetQual() && (*qual)->GetQual() == "product") {
                    m_Objs[kMiscFeatWithProduct].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj)), false);
                }
            }
        }
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(MISC_FEATURE_WITH_PRODUCT_QUAL)
//  ----------------------------------------------------------------------------
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}



END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
