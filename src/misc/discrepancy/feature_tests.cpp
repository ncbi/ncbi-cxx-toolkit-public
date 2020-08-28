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
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objtools/cleanup/cleanup.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/seq_annot_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/tse_handle.hpp>
#include <objtools/edit/cds_fix.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

DISCREPANCY_MODULE(feature_tests);


// PSEUDO_MISMATCH

const string kPseudoMismatch = "[n] CDSs, RNAs, and genes have mismatching pseudos.";

DISCREPANCY_CASE(PSEUDO_MISMATCH, FEAT, eDisc | eOncaller | eSubmitter | eSmart | eFatal, "Pseudo Mismatch")
{
    for (auto& feat : context.GetFeat()) {
        if (feat.IsSetPseudo() && feat.GetPseudo() && (feat.GetData().IsCdregion() || feat.GetData().IsRna())) {
            const CSeq_feat* gene = context.GetGeneForFeature(feat);
            if (gene && !context.IsPseudo(*gene)) {
                m_Objs[kPseudoMismatch].Add(*context.SeqFeatObjRef(feat, CDiscrepancyContext::eFixSelf), false).Fatal();
                m_Objs[kPseudoMismatch].Add(*context.SeqFeatObjRef(*gene), false).Fatal();
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(PSEUDO_MISMATCH)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_AUTOFIX(PSEUDO_MISMATCH)
{
    const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(context.FindObject(*obj));
    CRef<CSeq_feat> new_feat(new CSeq_feat());
    new_feat->Assign(*sf);
    new_feat->SetPseudo(true);
    context.ReplaceSeq_feat(*obj, *sf, *new_feat);
    obj->SetFixed();
    return CRef<CAutofixReport>(new CAutofixReport("PSEUDO_MISMATCH: Set pseudo for [n] feature[s]", 1));
}


// SHORT_RRNA

DISCREPANCY_CASE(SHORT_RRNA, FEAT, eDisc | eOncaller | eSubmitter | eSmart | eFatal, "Short rRNA Features")
{
    for (auto& feat : context.GetFeat()) {
        if (feat.IsSetData() && feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_rRNA && !feat.IsSetPartial() && IsShortrRNA(feat, &(context.GetScope()))) {
            m_Objs["[n] rRNA feature[s] [is] too short"].Add(*context.SeqFeatObjRef(feat)).Fatal();
        }
    }
}


DISCREPANCY_SUMMARIZE(SHORT_RRNA)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// DISC_RBS_WITHOUT_GENE

static bool IsRBS(const CSeq_feat& f)
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
    for (auto& it: f.GetQual()) {
        if (it->IsSetQual() && NStr::Equal(it->GetQual(), "regulatory_class") &&
            CSeqFeatData::GetRegulatoryClass(it->GetVal()) == CSeqFeatData::eSubtype_RBS) {
            return true;
        }
    }
    return false;
}


DISCREPANCY_CASE(RBS_WITHOUT_GENE, FEAT, eOncaller | eFatal, "RBS features should have an overlapping gene")
{
    bool has_genes = false;
    for (auto& feat : context.GetAllFeat()) {
        if (feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_gene) {
            has_genes = true;
            break;
        }
    }
    if (has_genes) {
        for (auto& feat : context.GetFeat()) {
            if (IsRBS(feat) && !context.GetGeneForFeature(feat)) {
                m_Objs["[n] RBS feature[s] [does] not have overlapping gene[s]"].Add(*context.SeqFeatObjRef(feat)).Fatal();
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(RBS_WITHOUT_GENE)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// MISSING_GENES

bool ReportGeneMissing(const CSeq_feat& f)
{
    CSeqFeatData::ESubtype subtype = f.GetData().GetSubtype();
    if (subtype == CSeqFeatData::eSubtype_regulatory) {
        return false;
    }
    if (IsRBS(f) ||
        f.GetData().IsCdregion() ||
        f.GetData().IsRna() ||
        subtype == CSeqFeatData::eSubtype_exon ||
        subtype == CSeqFeatData::eSubtype_intron) {
        return true;
    }
    return false;
}


DISCREPANCY_CASE(MISSING_GENES, FEAT, eDisc | eSubmitter | eSmart | eFatal, "Missing Genes")
{
    for (auto& feat : context.GetFeat()) {
        if (!feat.GetGeneXref() && feat.IsSetData() && ReportGeneMissing(feat)) {
            const CSeq_feat* gene_feat = context.GetGeneForFeature(feat);
            if (!gene_feat) {
                m_Objs["[n] feature[s] [has] no genes"].Add(*context.SeqFeatObjRef(feat)).Fatal();
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(MISSING_GENES)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// EXTRA_GENES

const string kExtraGene = "[n] gene feature[s] [is] not associated with a CDS or RNA feature.";
const string kExtraPseudo = "[n] pseudo gene feature[s] [is] not associated with a CDS or RNA feature.";
const string kExtraGeneNonPseudoNonFrameshift = "[n] non-pseudo gene feature[s] are not associated with a CDS or RNA feature and [does] not have frameshift in the comment.";

bool IsGeneInXref(const CSeq_feat& gene, const CSeq_feat& feat, bool& have_gene_ref)
{
    ITERATE (CSeq_feat::TXref, it, feat.GetXref ()) {
        if ((*it)->IsSetId()) {
            const CFeat_id& id = (*it)->GetId();
            if (gene.CanGetId() && gene.GetId().Equals(id)) {
                return true;
            }
        }
        if ((*it)->IsSetData() && (*it)->GetData().IsGene ()) {
            have_gene_ref = true;
            const CGene_ref& gene_ref = (*it)->GetData().GetGene();
            const string& locus = gene.GetData().GetGene().IsSetLocus() ? gene.GetData().GetGene().GetLocus() : kEmptyStr;
            const string& locus_tag = gene.GetData().GetGene().IsSetLocus_tag() ? gene.GetData().GetGene().GetLocus_tag() : kEmptyStr;
            if ((gene_ref.IsSetLocus() || gene_ref.IsSetLocus_tag())
                && (!gene_ref.IsSetLocus_tag() || gene_ref.GetLocus_tag() == locus_tag)
                && (gene_ref.IsSetLocus_tag() || locus_tag.empty())
                && (!gene_ref.IsSetLocus() || gene_ref.GetLocus() == locus)
                && (gene_ref.IsSetLocus() || locus.empty())) {
                return true;
            }
        }
    }
    return false;
}


DISCREPANCY_CASE(EXTRA_GENES, SEQUENCE, eDisc | eSubmitter | eSmart, "Extra Genes")
{
    // TODO: Do not collect if mRNA sequence in Gen-prod set
    auto& genes = context.FeatGenes();
    auto& all = context.FeatAll();
    for (auto& gene : genes) {
        if ((gene->IsSetComment() && !gene->GetComment().empty()) || (gene->GetData().GetGene().IsSetDesc() && !gene->GetData().GetGene().GetDesc().empty())) {
            continue;
        }
        const CSeq_loc& loc = gene->GetLocation();
        bool found = false;
        for (auto& feat : all) {
            if (feat->GetData().IsCdregion() || feat->GetData().IsRna()) {
                const CSeq_loc& loc_f = feat->GetLocation();
                sequence::ECompare cmp = context.Compare(loc, loc_f);
                if (cmp == sequence::eSame || cmp == sequence::eContains) {
                    bool have_gene_ref = false;
                    if (IsGeneInXref(*gene, *feat, have_gene_ref)) {
                        found = true;
                        break;
                    }
                    else if (!have_gene_ref) {
                        CConstRef<CSeq_feat> best_gene = sequence::GetBestOverlappingFeat(loc_f, CSeqFeatData::e_Gene, sequence::eOverlap_Contained, context.GetScope());
                        if (best_gene.NotEmpty() && &*best_gene == &*gene) {
                            found = true;
                            break;
                        }
                    }
                }
            }
        }
        if (!found) {
            m_Objs[kExtraGene][context.IsPseudo(*gene) ? kExtraPseudo : kExtraGeneNonPseudoNonFrameshift].Ext().Add(*context.SeqFeatObjRef(*gene));
        }
    }
}


DISCREPANCY_SUMMARIZE(EXTRA_GENES)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


//SUPERFLUOUS_GENE

DISCREPANCY_CASE(SUPERFLUOUS_GENE, SEQUENCE, eDisc | eOncaller, "Superfluous Genes")
{
    auto& genes = context.FeatGenes();
    auto& feats = context.FeatAll();
    for (size_t i = 0; i < genes.size(); i++) {
        if (genes[i]->IsSetPseudo() && genes[i]->GetPseudo()) {
            continue;
        }
        const CSeq_loc& loc_i = genes[i]->GetLocation();
        bool found = false;
        for (size_t j = 0; j < feats.size(); j++) {
            if (feats[j]->GetData().IsGene()) {
                continue;
            }
            const CSeq_loc& loc_j = feats[j]->GetLocation();
            sequence::ECompare compare = context.Compare(loc_j, loc_i);
            if (compare == sequence::eNoOverlap) {
                continue;
            }
            if (genes[i] == context.GetGeneForFeature(*feats[j])) {
                found = true;
                break;
            }
        }
        if (!found) {
            m_Objs["[n] gene feature[s] [is] not associated with any feature and [is] not pseudo."].Add(*context.SeqFeatObjRef(*genes[i]));
        }
    }
}


DISCREPANCY_SUMMARIZE(SUPERFLUOUS_GENE)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// BACTERIAL_PARTIAL_NONEXTENDABLE_PROBLEMS

enum EExtensibe {
    eExtensibe_none = 0,
    eExtensibe_fixable = 1,
    eExtensibe_abut = 2
};


EExtensibe IsExtendableLeft(TSeqPos left, const CBioseq& seq, CScope* scope, TSeqPos& extend_len, ENa_strand strand)
{
    bool circular = seq.IsSetInst() && seq.GetInst().GetTopology() == CSeq_inst::eTopology_circular;
    EExtensibe rval = eExtensibe_none;
    if (left < 3) {
        extend_len = left;
        rval = extend_len ? circular ? eExtensibe_none : eExtensibe_fixable : eExtensibe_abut;
    }
    else if (seq.IsSetInst() && seq.GetInst().IsSetRepr() && seq.GetInst().GetRepr() == CSeq_inst::eRepr_delta && seq.GetInst().IsSetExt() && seq.GetInst().GetExt().IsDelta()) {
        TSeqPos offset = 0;
        TSeqPos last_gap_stop = 0;
        bool gap = false;
        for (auto& it : seq.GetInst().GetExt().GetDelta().Get()) {
            if (it->IsLiteral()) {
                offset += it->GetLiteral().GetLength();
                if (!it->GetLiteral().IsSetSeq_data()) {
                    last_gap_stop = offset;
                    gap = true;
                }
                else if (it->GetLiteral().GetSeq_data().IsGap()) {
                    last_gap_stop = offset;
                    gap = true;
                }
            }
            else if (it->IsLoc()) {
                offset += sequence::GetLength(it->GetLoc(), scope);
            }
            if (offset > left) {
                break;
            }
        }
        if (left >= last_gap_stop && left - last_gap_stop <= 3) {
            extend_len = left - last_gap_stop;
            rval = extend_len ? (circular && !gap) ? eExtensibe_none : eExtensibe_fixable : eExtensibe_abut;
        }
    }
    if (rval == eExtensibe_abut) return rval;
    CSeqVector svec(seq, scope, CBioseq_Handle::CBioseq_Handle::eCoding_Iupac);
    string codon;
    size_t count = extend_len ? extend_len : 1;
    svec.GetSeqData(left - count, left, codon);
    for (unsigned i = 0; i < count; i++) {
        if (codon[i] == 'N') {
            count = i;
            break;
        }
    }
    if (!count) {
        extend_len = 0;
        return eExtensibe_abut;
    }
    if (rval == eExtensibe_fixable) {
        svec.GetSeqData(left - extend_len, left - extend_len + 3, codon);
        if (strand == eNa_strand_minus) {
            if (codon == "CTA" || codon == "TTA" || codon == "TCA") { // reverse TAG / TAA / TGA
                rval = eExtensibe_none;
            }
        }
        else {
            if (codon == "TAG" || codon == "TAA" || codon == "TGA") {
                rval = eExtensibe_none;
            }
        }
    }
    return rval;
}


EExtensibe IsExtendableRight(TSeqPos right, const CBioseq& seq, CScope* scope, TSeqPos& extend_len, ENa_strand strand)
{
    bool circular = seq.IsSetInst() && seq.GetInst().GetTopology() == CSeq_inst::eTopology_circular;
    EExtensibe rval = eExtensibe_none;
    if (right > seq.GetLength() - 4) {
        extend_len = seq.GetLength() - right - 1;
        rval = extend_len ? circular ? eExtensibe_none : eExtensibe_fixable : eExtensibe_abut;
    }
    else if (seq.IsSetInst() && seq.GetInst().IsSetRepr() && seq.GetInst().GetRepr() == CSeq_inst::eRepr_delta && seq.GetInst().IsSetExt() && seq.GetInst().GetExt().IsDelta()) {
        TSeqPos offset = 0;
        TSeqPos next_gap_start = 0;
        bool gap = false;
        for (auto& it : seq.GetInst().GetExt().GetDelta().Get()) {
            if (it->IsLiteral()) {
                if (!it->GetLiteral().IsSetSeq_data()) {
                    next_gap_start = offset;
                    gap = true;
                }
                else if (it->GetLiteral().GetSeq_data().IsGap()) {
                    next_gap_start = offset;
                    gap = true;
                }
                offset += it->GetLiteral().GetLength();
            }
            else if (it->IsLoc()) {
                offset += sequence::GetLength(it->GetLoc(), scope);
            }
            if (offset > right + 3) {
                break;
            }
        }
        if (next_gap_start > right && next_gap_start - right - 1 <= 3) {
            extend_len = next_gap_start - right - 1;
            rval = extend_len ? (circular && !gap) ? eExtensibe_none : eExtensibe_fixable : eExtensibe_abut;
        }
    }
    if (rval == eExtensibe_abut) return rval;
    CSeqVector svec(seq, scope, CBioseq_Handle::CBioseq_Handle::eCoding_Iupac);
    string codon;
    size_t count = extend_len ? extend_len : 1;
    svec.GetSeqData(right + 1, right + count + 1, codon);
    for (unsigned i = 0; i < count; i++) {
        if (codon[i] == 'N') {
            count = i;
            break;
        }
    }
    if (!count) {
        extend_len = 0;
        return eExtensibe_abut;
    }
    if (rval == eExtensibe_fixable) {
        svec.GetSeqData(right + extend_len - 3, right + extend_len, codon);
        if (strand == eNa_strand_minus) {
            if (codon == "CTA" || codon == "TTA" || codon == "TCA") { // reverse TAG / TAA / TGA
                rval = eExtensibe_none;
            }
        }
        else {
            if (codon == "TAG" || codon == "TAA" || codon == "TGA") {
                rval = eExtensibe_none;
            }
        }
    }
    return rval;
}


// Cannot be extended and not abut the end or the gap
bool IsNonExtendable(const CSeq_loc& loc, const CBioseq& seq, CScope* scope)
{
    bool rval = false;
    if (loc.IsPartialStart(eExtreme_Positional)) {
        TSeqPos start = loc.GetStart(eExtreme_Positional);
        if (start > 0) {
            TSeqPos extend_len = 0;
            if (IsExtendableLeft(start, seq, scope, extend_len, loc.GetStrand()) == eExtensibe_none) {
                rval = true;
            }
        }
    }
    if (!rval && loc.IsPartialStop(eExtreme_Positional)) {
        TSeqPos stop = loc.GetStop(eExtreme_Positional);
        if (stop < seq.GetLength() - 1) {
            TSeqPos extend_len = 0;
            if (IsExtendableRight(stop, seq, scope, extend_len, loc.GetStrand()) == eExtensibe_none) {
                rval = true;
            }
        }
    }
    return rval;
}


DISCREPANCY_CASE(BACTERIAL_PARTIAL_NONEXTENDABLE_PROBLEMS, SEQUENCE, eDisc | eSubmitter | eSmart | eFatal, "Find partial feature ends on bacterial sequences that cannot be extended: on when non-eukaryote")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    const CSeqdesc* biosrc = context.GetBiosource();
    if (!biosrc || context.IsEukaryotic(&biosrc->GetSource()) || context.IsOrganelle(&biosrc->GetSource()) || bioseq.IsAa()) {
        return;
    }
    for (auto& feat : context.GetAllFeat()) {
        if (feat.IsSetData() && feat.GetData().IsCdregion() && IsNonExtendable(feat.GetLocation(), bioseq, &(context.GetScope()))) {
            m_Objs["[n] feature[s] [has] partial ends that do not abut the end of the sequence or a gap, and cannot be extended by 3 or fewer nucleotides to do so"].Add(*context.SeqFeatObjRef(feat, &feat)).Fatal();
        }
    }
}


DISCREPANCY_SUMMARIZE(BACTERIAL_PARTIAL_NONEXTENDABLE_PROBLEMS)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


const string kNonExtendableException = "unextendable partial coding region";

DISCREPANCY_AUTOFIX(BACTERIAL_PARTIAL_NONEXTENDABLE_PROBLEMS)
{
    const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(context.FindObject(*obj));
    if (!sf->IsSetExcept_text() || sf->GetExcept_text().find(kNonExtendableException) == string::npos) {
        CRef<CSeq_feat> new_feat(new CSeq_feat());
        new_feat->Assign(*sf);
        if (new_feat->IsSetExcept_text()) {
            new_feat->SetExcept_text(sf->GetExcept_text() + "; " + kNonExtendableException);
        }
        else {
            new_feat->SetExcept_text(kNonExtendableException);
        }
        new_feat->SetExcept(true);
        context.ReplaceSeq_feat(*obj, *sf, *new_feat);
        obj->SetFixed();
        return CRef<CAutofixReport>(new CAutofixReport("BACTERIAL_PARTIAL_NONEXTENDABLE_PROBLEMS: Set exception for [n] feature[s]", 1));
    }
    return CRef<CAutofixReport>(0);
}


// BACTERIAL_PARTIAL_NONEXTENDABLE_EXCEPTION

DISCREPANCY_CASE(BACTERIAL_PARTIAL_NONEXTENDABLE_EXCEPTION, SEQUENCE, eDisc | eSubmitter | eSmart, "Find partial feature ends on bacterial sequences that cannot be extended but have exceptions: on when non-eukaryote")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    const CSeqdesc* biosrc = context.GetBiosource();
    if (!biosrc || context.IsEukaryotic(&biosrc->GetSource()) || context.IsOrganelle(&biosrc->GetSource()) || bioseq.IsAa()) {
        return;
    }
    for (auto& feat : context.GetAllFeat()) {
        if (feat.IsSetData() && feat.GetData().IsCdregion() && feat.IsSetExcept_text() && NStr::FindNoCase(feat.GetExcept_text(), kNonExtendableException) != string::npos && IsNonExtendable(feat.GetLocation(), bioseq, &(context.GetScope()))) {
            m_Objs["[n] feature[s] [has] partial ends that do not abut the end of the sequence or a gap, and cannot be extended by 3 or fewer nucleotides to do so, but [has] the correct exception"].Add(*context.SeqFeatObjRef(feat));
        }
    }
}


DISCREPANCY_SUMMARIZE(BACTERIAL_PARTIAL_NONEXTENDABLE_EXCEPTION)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// PARTIAL_PROBLEMS

DISCREPANCY_CASE(PARTIAL_PROBLEMS, SEQUENCE, eDisc | eOncaller | eSubmitter | eSmart | eFatal, "Find partial feature ends on bacterial sequences, but could be extended by 3 or fewer nucleotides")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    const CSeqdesc* biosrc = context.GetBiosource();
    if (!biosrc || context.IsEukaryotic(&biosrc->GetSource()) || context.IsOrganelle(&biosrc->GetSource()) || bioseq.IsAa()) {
        return;
    }
    //bool circular = bioseq.IsSetInst() && bioseq.GetInst().GetTopology() == CSeq_inst::eTopology_circular;
    for (auto& feat : context.GetAllFeat()) {
        if (feat.IsSetData() && feat.GetData().IsCdregion()) {
            bool add_this = false;
            if (feat.GetLocation().IsPartialStart(eExtreme_Positional)) {
                TSeqPos start = feat.GetLocation().GetStart(eExtreme_Positional);
                if (start > 0) {
                    TSeqPos extend_len = 0;
                    if (IsExtendableLeft(start, bioseq, &(context.GetScope()), extend_len, feat.GetLocation().GetStrand()) == eExtensibe_fixable) {
                        //cout << "extend start: " << extend_len << "\n";
                        add_this = extend_len > 0 && extend_len <= 3;
                    }
                }
            }
            if (!add_this && feat.GetLocation().IsPartialStop(eExtreme_Positional)) {
                TSeqPos stop = feat.GetLocation().GetStop(eExtreme_Positional);
                if (stop < bioseq.GetLength() - 1) {
                    TSeqPos extend_len = 0;
                    if (IsExtendableRight(stop, bioseq, &(context.GetScope()), extend_len, feat.GetLocation().GetStrand()) == eExtensibe_fixable) {
                        //cout << "extend end: " << extend_len << "\n";
                        add_this = extend_len > 0 && extend_len <= 3;
                    }
                }
            }
            if (add_this) {
                m_Objs["[n] feature[s] [has] partial ends that do not abut the end of the sequence or a gap, but could be extended by 3 or fewer nucleotides to do so"].Add(*context.SeqFeatObjRef(feat, CDiscrepancyContext::eFixSet)).Fatal();
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(PARTIAL_PROBLEMS)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


static bool ExtendToGapsOrEnds(const CSeq_feat& cds, CScope& scope)
{
    bool rval = false;

    CBioseq_Handle bsh = scope.GetBioseqHandle(cds.GetLocation());
    if (!bsh) {
        return rval;
    }
    CConstRef<CBioseq> seq = bsh.GetCompleteBioseq();
    //bool circular = seq->IsSetInst() && seq->GetInst().GetTopology() == CSeq_inst::eTopology_circular;

    CConstRef<CSeq_feat> gene;
    for (CFeat_CI gene_it(bsh, CSeqFeatData::eSubtype_gene); gene_it; ++gene_it) {
        if (gene_it->GetLocation().GetStart(eExtreme_Positional) == cds.GetLocation().GetStart(eExtreme_Positional) && gene_it->GetLocation().GetStop(eExtreme_Positional) == cds.GetLocation().GetStop(eExtreme_Positional)) {
            gene.Reset(&gene_it->GetMappedFeature());
            break;
        }
    }

    CRef<CSeq_feat> new_feat(new CSeq_feat());
    new_feat->Assign(cds);

    CRef<CSeq_feat> new_gene;
    if (gene) {
        new_gene.Reset(new CSeq_feat());
        new_gene->Assign(*gene);
    }

    if (cds.GetLocation().IsPartialStart(eExtreme_Positional)) {
        TSeqPos start = cds.GetLocation().GetStart(eExtreme_Positional);
        if (start > 0) {
            TSeqPos extend_len = 0;
            if (IsExtendableLeft(start, *seq, &scope, extend_len, cds.GetLocation().GetStrand()) && CCleanup::SeqLocExtend(new_feat->SetLocation(), start - extend_len, scope)) {
                if (gene) {
                    CCleanup::SeqLocExtend(new_gene->SetLocation(), start - extend_len, scope);
                }
                if (new_feat->GetData().GetCdregion().CanGetFrame() && cds.GetLocation().GetStrand() != eNa_strand_minus) {
                    CCdregion::EFrame frame = new_feat->GetData().GetCdregion().GetFrame();
                    if (frame != CCdregion::eFrame_not_set) {
                        //  eFrame_not_set = 0,  ///< not set, code uses one
                        //  eFrame_one     = 1,
                        //  eFrame_two     = 2,
                        //  eFrame_three   = 3  ///< reading frame
                        unsigned fr = (unsigned)frame - 1;
                        fr = (fr + extend_len) % 3;
                        frame = (CCdregion::EFrame)(fr + 1);
                        new_feat->SetData().SetCdregion().SetFrame() = frame;
                    }
                }
                rval = true;
            }
        }
    }

    if (cds.GetLocation().IsPartialStop(eExtreme_Positional)) {
        TSeqPos stop = cds.GetLocation().GetStop(eExtreme_Positional);
        if (stop > 0) {
            TSeqPos extend_len = 0;
            if (IsExtendableRight(stop, *seq, &scope, extend_len, cds.GetLocation().GetStrand()) && CCleanup::SeqLocExtend(new_feat->SetLocation(), stop + extend_len, scope)) {
                if (gene) {
                    CCleanup::SeqLocExtend(new_gene->SetLocation(), stop + extend_len, scope);
                }
                if (new_feat->GetData().GetCdregion().CanGetFrame() && cds.GetLocation().GetStrand() == eNa_strand_minus) {
                    CCdregion::EFrame frame = new_feat->GetData().GetCdregion().GetFrame();
                    if (frame != CCdregion::eFrame_not_set) {
                        unsigned fr = (unsigned)frame - 1;
                        fr = (fr + extend_len) % 3;
                        frame = (CCdregion::EFrame)(fr + 1);
                        new_feat->SetData().SetCdregion().SetFrame() = frame;
                    }
                }
                rval = true;
            }
        }
    }

    if (rval) {
        CSeq_feat_EditHandle feh(scope.GetSeq_featHandle(cds));
        feh.Replace(*new_feat);
        if (gene) {
            CSeq_feat_EditHandle geh(scope.GetSeq_featHandle(*gene));
            geh.Replace(*new_gene);
        }
    }
    return rval;
}


DISCREPANCY_AUTOFIX(PARTIAL_PROBLEMS)
{
    const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(context.FindObject(*obj));
    if (ExtendToGapsOrEnds(*sf, context.GetScope())) {
        obj->SetFixed();
        return CRef<CAutofixReport>(new CAutofixReport("PARTIAL_PROBLEMS: [n] feature[s] [is] extended to end or gap", 1));
    }
    return CRef<CAutofixReport>(0);
}


// EUKARYOTE_SHOULD_HAVE_MRNA

const string kEukaryoteShouldHavemRNA = "no mRNA present";
const string kEukaryoticCDSHasMrna = "Eukaryotic CDS has mRNA";

DISCREPANCY_CASE(EUKARYOTE_SHOULD_HAVE_MRNA, SEQUENCE, eDisc | eSubmitter | eSmart | eFatal, "Eukaryote should have mRNA")
{
    const CSeqdesc* molinfo = context.GetMolinfo();
    if (!molinfo || !molinfo->GetMolinfo().IsSetBiomol() || molinfo->GetMolinfo().GetBiomol() != CMolInfo::eBiomol_genomic) {
        return;
    }
    const CSeqdesc* biosrc = context.GetBiosource();
    if (!context.IsEukaryotic(biosrc ? &biosrc->GetSource() : 0)) {
        return;
    }
    for (auto& feat : context.GetAllFeat()) {
        if (feat.IsSetData() && feat.GetData().IsCdregion() && !context.IsPseudo(feat)) {
            CConstRef<CSeq_feat> mrna = sequence::GetmRNAforCDS(feat, context.GetScope());
            if (mrna) {
                m_Objs[kEukaryoticCDSHasMrna].Add(*context.SeqFeatObjRef(feat));
            }
            else if (m_Objs[kEukaryoteShouldHavemRNA].GetObjects().empty()) {
                m_Objs[kEukaryoteShouldHavemRNA].Add(*context.SeqFeatObjRef(feat)).Fatal();
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(EUKARYOTE_SHOULD_HAVE_MRNA)
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

DISCREPANCY_CASE(NON_GENE_LOCUS_TAG, FEAT, eDisc | eOncaller | eSubmitter | eSmart, "Nongene Locus Tag")
{
    for (auto& feat : context.GetFeat()) {
        if (feat.IsSetQual() && (!feat.IsSetData() || !feat.GetData().IsGene())) {
            for (auto& it : feat.GetQual()) {
                if (it->IsSetQual() && NStr::EqualNocase(it->GetQual(), "locus_tag")) {
                    m_Objs["[n] non-gene feature[s] [has] locus tag[s]."].Add(*context.SeqFeatObjRef(feat));
                    break;
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(NON_GENE_LOCUS_TAG)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// FIND_BADLEN_TRNAS

DISCREPANCY_CASE(FIND_BADLEN_TRNAS, FEAT, eDisc | eOncaller | eSubmitter | eSmart, "Find short and long tRNAs")
{
    for (auto& feat : context.GetFeat()) {
        if (feat.IsSetData() && feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_tRNA) {
            TSeqPos len = sequence::GetLength(feat.GetLocation(), &(context.GetScope()));
            if (!feat.IsSetPartial() && len < 50) {
                m_Objs["[n] tRNA[s] [is] too short"].Add(*context.SeqFeatObjRef(feat));
            }
            else if (len >= 150) {
                m_Objs["[n] tRNA[s] [is] too long - over 150 nucleotides"].Add(*context.SeqFeatObjRef(feat));
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(FIND_BADLEN_TRNAS)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// ORG_TRNAS

DISCREPANCY_CASE(ORG_TRNAS, FEAT, eDisc | eOncaller, "Find long tRNAs > 90nt except Ser/Leu/Sec")
{
    for (auto& feat : context.GetFeat()) {
        if (feat.IsSetData() && feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_tRNA) {
            TSeqPos len = sequence::GetLength(feat.GetLocation(), &(context.GetScope()));
            if (len > 90) {
                const string aa = context.GetAminoacidName(feat);
                if (aa != "Ser" && aa != "Sec" && aa != "Leu") {
                    m_Objs["[n] tRNA[s] [is] too long"].Add(*context.SeqFeatObjRef(feat));
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(ORG_TRNAS)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// GENE_PARTIAL_CONFLICT
bool IsPartialStartConflict(const CSeq_feat& feat, const CSeq_feat& gene, bool is_mrna = false)
{
    bool partial_feat = feat.GetLocation().IsPartialStart(eExtreme_Biological);
    bool partial_gene = gene.GetLocation().IsPartialStart(eExtreme_Biological);
    if (partial_feat != partial_gene) {
        if (is_mrna || feat.GetLocation().GetStart(eExtreme_Biological) == gene.GetLocation().GetStart(eExtreme_Biological)) {
            return true;
        }
    }
    return false;
}


bool IsPartialStopConflict(const CSeq_feat& feat, const CSeq_feat& gene, bool is_mrna = false)
{
    bool partial_feat = feat.GetLocation().IsPartialStop(eExtreme_Biological);
    bool partial_gene = gene.GetLocation().IsPartialStop(eExtreme_Biological);
    if (partial_feat != partial_gene) {
        if (is_mrna || feat.GetLocation().GetStop(eExtreme_Biological) == gene.GetLocation().GetStop(eExtreme_Biological)) {
            return true;
        }
    }
    return false;
}

const string kGenePartialConflictTop = "[n/2] feature location[s] conflict with partialness of overlapping gene";
const string kGenePartialConflictOther = "[n/2] feature[s] that [is] not coding region[s] or misc_feature[s] conflict with partialness of overlapping gene";
const string kGenePartialConflictCodingRegion = "[n/2] coding region location[s] conflict with partialness of overlapping gene";
const string kGenePartialConflictMiscFeat = "[n/2] misc_feature location[s] conflict with partialness of overlapping gene";
const string kConflictBoth = " feature partialness conflicts with gene on both ends";
const string kConflictStart = " feature partialness conflicts with gene on 5' end";
const string kConflictStop = " feature partialness conflicts with gene on 3' end";


DISCREPANCY_CASE(GENE_PARTIAL_CONFLICT, SEQUENCE, eOncaller | eSubmitter | eSmart, "Feature partialness should agree with gene partialness if endpoints match")
{
    const CSeqdesc* molinfo = context.GetMolinfo();
    const CSeqdesc* biosrc = context.GetBiosource();
    bool is_mrna = molinfo && molinfo->GetMolinfo().IsSetBiomol() && molinfo->GetMolinfo().GetBiomol() == CMolInfo::eBiomol_mRNA;
    bool is_eukaryotic = context.IsEukaryotic(biosrc ? &biosrc->GetSource() : 0);

    auto& all = context.FeatAll();
    for (auto feat : all) {
        if (!feat->IsSetData()) {
            continue;
        }
        const CSeq_feat* gene = context.GetGeneForFeature(*feat);
        if (!gene) {
            continue;
        }
        bool conflict_start = false;
        bool conflict_stop = false;
        CSeqFeatData::ESubtype subtype = feat->GetData().GetSubtype();
        string middle_label = kGenePartialConflictOther;
        if (feat->GetData().IsCdregion()) {
            if (!is_eukaryotic || is_mrna) {
                middle_label = kGenePartialConflictCodingRegion;
                conflict_start = IsPartialStartConflict(*feat, *gene, is_mrna);
                conflict_stop = IsPartialStopConflict(*feat, *gene, is_mrna);
                if (is_mrna) {
                    //look for 5' UTR
                    TSeqPos gene_start = gene->GetLocation().GetStart(eExtreme_Biological);
                    bool gene_start_partial = gene->GetLocation().IsPartialStart(eExtreme_Biological);
                    bool found_start = false;
                    bool found_utr5 = false;
                    for (auto fi : all) {
                        if (fi->IsSetData() && fi->GetData().GetSubtype() == CSeqFeatData::eSubtype_5UTR) {
                            found_utr5 = true;
                            if (fi->GetLocation().GetStart(eExtreme_Biological) == gene_start && fi->GetLocation().IsPartialStart(eExtreme_Biological) == gene_start_partial) {
                                found_start = true;
                                conflict_start = false;
                                break;
                            }
                        }
                    }
                    if (found_utr5 && !found_start) {
                        conflict_start = true;
                    }
                    //look for 3' UTR
                    TSeqPos gene_stop = gene->GetLocation().GetStop(eExtreme_Biological);
                    bool gene_stop_partial = gene->GetLocation().IsPartialStop(eExtreme_Biological);
                    bool found_stop = false;
                    bool found_utr3 = false;
                    for (auto fi : all) {
                        if (fi->IsSetData() && fi->GetData().GetSubtype() == CSeqFeatData::eSubtype_3UTR) {
                            found_utr3 = true;
                            if (fi->GetLocation().GetStop(eExtreme_Biological) == gene_stop && fi->GetLocation().IsPartialStop(eExtreme_Biological) == gene_stop_partial) {
                                found_stop = true;
                                conflict_stop = false;
                                break;
                            }
                        }
                    }
                    if (found_utr3 && !found_stop) {
                        conflict_stop = true;
                    }
                }
            }
        }
        else if (feat->GetData().IsRna() || subtype == CSeqFeatData::eSubtype_intron || subtype == CSeqFeatData::eSubtype_exon || subtype == CSeqFeatData::eSubtype_5UTR || subtype == CSeqFeatData::eSubtype_3UTR || subtype == CSeqFeatData::eSubtype_misc_feature) {
            conflict_start = IsPartialStartConflict(*feat, *gene);
            conflict_stop = IsPartialStopConflict(*feat, *gene);
            if (subtype == CSeqFeatData::eSubtype_misc_feature) {
                middle_label = kGenePartialConflictMiscFeat;
            }
        }
        if (conflict_start || conflict_stop) {
            string label = CSeqFeatData::SubtypeValueToName(subtype);
            label += conflict_start && conflict_stop ? kConflictBoth : conflict_start ? kConflictStart : kConflictStop;
            m_Objs[kGenePartialConflictTop][middle_label].Ext()[label].Ext().Add(*context.SeqFeatObjRef(*feat), false).Add(*context.SeqFeatObjRef(*gene), false);
        }
    }
}


DISCREPANCY_SUMMARIZE(GENE_PARTIAL_CONFLICT)
{
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


// BAD_GENE_STRAND

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


const string kBadGeneStrand = "[n/2] feature location[s] conflict with gene location strand[s]";


DISCREPANCY_CASE(BAD_GENE_STRAND, SEQUENCE, eOncaller | eSubmitter | eSmart, "Genes and features that share endpoints should be on the same strand")
{
    // note - use positional instead of biological, because we are *looking* for objects on the opposite strand
    auto& genes = context.FeatGenes();
    auto& feats = context.FeatAll();

    for (size_t j = 0; j < feats.size(); j++) {
        CSeqFeatData::ESubtype subtype = feats[j]->GetData().GetSubtype();
        if (subtype == CSeqFeatData::eSubtype_gene || subtype == CSeqFeatData::eSubtype_primer_bind) {
            continue;
        }
        const CSeq_loc& loc_j = feats[j]->GetLocation();
        TSeqPos feat_start = loc_j.GetStart(eExtreme_Positional);
        TSeqPos feat_stop = loc_j.GetStop(eExtreme_Positional);
        for (size_t i = 0; i < genes.size(); i++) {
            if (!genes[i]->IsSetLocation()) {
                continue;
            }
            const CSeq_loc& loc_i = genes[i]->GetLocation();
            ENa_strand strand_i = loc_i.GetStrand();
            TSeqPos gene_start = loc_i.GetStart(eExtreme_Positional);
            TSeqPos gene_stop = loc_i.GetStop(eExtreme_Positional);
            if (feat_start == gene_start || feat_stop == gene_stop) {
                bool all_ok = true;
                if (HasMixedStrands(loc_i)) {
                    // compare intervals, to make sure that for each pair of feature interval and gene interval
                    // where the gene interval contains the feature interval, the intervals are on the same strand
                    CSeq_loc_CI f_loc(loc_j);
                    bool found_bad = false;
                    while (f_loc && !found_bad) {
                        CConstRef<CSeq_loc> f_int = f_loc.GetRangeAsSeq_loc();
                        CSeq_loc_CI g_loc(loc_i);
                        while (g_loc && !found_bad) {
                            CConstRef<CSeq_loc> g_int = g_loc.GetRangeAsSeq_loc();
                            sequence::ECompare cmp = context.Compare(*f_int, *g_int);
                            if (cmp == sequence::eContained || cmp == sequence::eSame) {
                                if (!StrandsMatch(f_loc.GetStrand(), g_loc.GetStrand())) {
                                    found_bad = true;
                                }
                            }
                            ++g_loc;
                        }
                        ++f_loc;
                    }
                    all_ok = !found_bad;
                }
                else {
                    all_ok = StrandsMatch(loc_j.GetStrand(), strand_i);
                }
                if (!all_ok) {
                    size_t offset = m_Objs[kBadGeneStrand].GetMap().size() + 1;
                    string label = "Gene and feature strands conflict (pair " + NStr::NumericToString(offset) + ")";
                    m_Objs[kBadGeneStrand][label].Ext().Add(*context.SeqFeatObjRef(*genes[i]), false).Add(*context.SeqFeatObjRef(*feats[j]), false);
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(BAD_GENE_STRAND)
{
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


// MICROSATELLITE_REPEAT_TYPE

DISCREPANCY_CASE(MICROSATELLITE_REPEAT_TYPE, FEAT, eOncaller | eFatal, "Microsatellites must have repeat type of tandem")
{
    for (auto& feat : context.GetFeat()) {
        if (feat.IsSetData() && feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_repeat_region && feat.IsSetQual()) {
            bool is_microsatellite = false;
            bool is_tandem = false;
            const CSeq_feat::TQual& quals = feat.GetQual();
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
                m_Objs["[n] microsatellite[s] do not have a repeat type of tandem"].Add(*context.SeqFeatObjRef(feat, &feat)).Fatal();
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(MICROSATELLITE_REPEAT_TYPE)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_AUTOFIX(MICROSATELLITE_REPEAT_TYPE)
{
    const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(context.FindObject(*obj));
    CRef<CSeq_feat> new_feat(new CSeq_feat());
    new_feat->Assign(*sf);
    CRef<CGb_qual> new_qual(new CGb_qual("rpt_type", "tandem"));
    new_feat->SetQual().push_back(new_qual);
    context.ReplaceSeq_feat(*obj, *sf, *new_feat);
    obj->SetFixed();
    return CRef<CAutofixReport>(new CAutofixReport("MICROSATELLITE_REPEAT_TYPE: added repeat type of tandem to [n] microsatellite[s]", 1));
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

static void FindSuspiciousNotePhrases(const string& s, CDiscrepancyContext& context, CReportNode& rep, const CSeq_feat& feat)
{
    for (size_t k = 0; k < kNumSuspiciousNotePhrases; k++) {
        if (NStr::FindNoCase(s, kSuspiciousNotePhrases[k]) != string::npos) {
            rep["[n] note text[s] contain suspicious phrase[s]"]["[n] note text[s] contain '" + kSuspiciousNotePhrases[k] + "'"].Ext().Add(*context.SeqFeatObjRef(feat));
        }
    }
}


DISCREPANCY_CASE(SUSPICIOUS_NOTE_TEXT, FEAT, eOncaller, "Find Suspicious Phrases in Note Text")
{
    for (auto& feat : context.GetFeat()) {
        if (feat.IsSetData()) {
            switch (feat.GetData().GetSubtype()) {
                case CSeqFeatData::eSubtype_gene:
                    // look in gene comment and gene description
                    if (feat.IsSetComment()) {
                        FindSuspiciousNotePhrases(feat.GetComment(), context, m_Objs, feat);
                    }
                    if (feat.GetData().GetGene().IsSetDesc()) {
                        FindSuspiciousNotePhrases(feat.GetData().GetGene().GetDesc(), context, m_Objs, feat);
                    }
                    break;
                case CSeqFeatData::eSubtype_prot:
                    if (feat.GetData().GetProt().IsSetDesc()) {
                        FindSuspiciousNotePhrases(feat.GetData().GetProt().GetDesc(), context, m_Objs, feat);
                    }
                    break;
                case CSeqFeatData::eSubtype_cdregion:
                case CSeqFeatData::eSubtype_misc_feature:
                    if (feat.IsSetComment()) {
                        FindSuspiciousNotePhrases(feat.GetComment(), context, m_Objs, feat);
                    }
                    break;
                default:
                    break;
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(SUSPICIOUS_NOTE_TEXT)
{
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


// CDS_HAS_NEW_EXCEPTION

static const string kNewExceptions[] =
{
    "annotated by transcript or proteomic data",
    "heterogeneous population sequenced",
    "low-quality sequence region",
    "unextendable partial coding region",
};


DISCREPANCY_CASE(CDS_HAS_NEW_EXCEPTION, FEAT, eDisc | eOncaller | eSmart, "Coding region has new exception")
{
    static const size_t max = sizeof(kNewExceptions) / sizeof(string);
    for (auto& feat : context.GetFeat()) {
        if (feat.IsSetData() && feat.GetData().IsCdregion() && feat.IsSetExcept_text()) {
            for (size_t i = 0; i < max; i++) {
                if (NStr::FindNoCase(feat.GetExcept_text(), kNewExceptions[i]) != string::npos) {
                    m_Objs["[n] coding region[s] [has] new exception[s]"].Add(*context.SeqFeatObjRef(feat));
                    break;
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(CDS_HAS_NEW_EXCEPTION)
{
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


// SHORT_LNCRNA

DISCREPANCY_CASE(SHORT_LNCRNA, FEAT, eDisc | eOncaller | eSubmitter | eSmart, "Short lncRNA sequences")
{
    for (auto& feat : context.GetFeat()) {
        if (feat.IsSetData() && feat.GetData().IsRna() && feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_ncRNA
                && feat.GetData().GetRna().IsSetExt() && feat.GetData().GetRna().GetExt().IsGen() && feat.GetData().GetRna().GetExt().GetGen().IsSetClass()
                && NStr::EqualNocase(feat.GetData().GetRna().GetExt().GetGen().GetClass(), "lncrna") // only looking at lncrna features
                && !feat.GetLocation().IsPartialStart(eExtreme_Biological) && !feat.GetLocation().IsPartialStop(eExtreme_Biological) // ignore if partial
                && sequence::GetLength(feat.GetLocation(), &(context.GetScope())) < 200) {
            m_Objs["[n] lncRNA feature[s] [is] suspiciously short"].Add(*context.SeqFeatObjRef(feat));
        }
    }
}


DISCREPANCY_SUMMARIZE(SHORT_LNCRNA)
{
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


// JOINED_FEATURES

const string& kJoinedFeatures = "[n] feature[s] [has] joined location[s].";
const string& kJoinedFeaturesNoException = "[n] feature[s] [has] joined location but no exception";
const string& kJoinedFeaturesException = "[n] feature[s] [has] joined location but exception '";
const string& kJoinedFeaturesBlankException = "[n] feature[s] [has] joined location but a blank exception";

DISCREPANCY_CASE(JOINED_FEATURES, FEAT, eDisc | eSubmitter | eSmart, "Joined Features: on when non-eukaryote")
{
    const CSeqdesc* biosrc = context.GetBiosource();
    if (biosrc && !context.IsEukaryotic(&biosrc->GetSource()) && !context.IsOrganelle(&biosrc->GetSource())) {
        for (auto& feat : context.GetFeat()) {
            if (feat.IsSetLocation()) {
                if (feat.GetLocation().IsMix() || feat.GetLocation().IsPacked_int()) {
                    if (feat.IsSetExcept_text()) {
                        if (NStr::IsBlank(feat.GetExcept_text())) {
                            m_Objs[kJoinedFeatures][kJoinedFeaturesBlankException].Ext().Add(*context.SeqFeatObjRef(feat));
                        }
                        else {
                            m_Objs[kJoinedFeatures][kJoinedFeaturesException + feat.GetExcept_text() + "'"].Ext().Add(*context.SeqFeatObjRef(feat));
                        }
                    }
                    else if (feat.IsSetExcept() && feat.GetExcept()) {
                        m_Objs[kJoinedFeatures][kJoinedFeaturesBlankException].Ext().Add(*context.SeqFeatObjRef(feat));
                    }
                    else {
                        m_Objs[kJoinedFeatures][kJoinedFeaturesNoException].Ext().Add(*context.SeqFeatObjRef(feat));
                    }
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(JOINED_FEATURES)
{
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


// BACTERIAL_JOINED_FEATURES_NO_EXCEPTION

DISCREPANCY_CASE(BACTERIAL_JOINED_FEATURES_NO_EXCEPTION, SEQUENCE, eDisc | eSubmitter | eSmart, "Joined Features on prokaryote without exception")
{
    const CSeqdesc* biosrc = context.GetBiosource();
    if (biosrc && (context.IsEukaryotic(&biosrc->GetSource()) || context.IsOrganelle(&biosrc->GetSource()))) {
        return;
    }
    for (auto& feat : context.GetAllFeat()) {
        if (feat.IsSetLocation() && feat.CanGetData() && feat.GetData().IsCdregion() && !context.IsPseudo(feat)) {
            if (feat.GetLocation().IsMix() || feat.GetLocation().IsPacked_int()) {
                if ((feat.IsSetExcept_text() && !feat.GetExcept_text().empty()) || (feat.IsSetExcept() && feat.GetExcept())) {
                    continue;
                }
                bool bad = true;
                if (context.CurrentBioseq().CanGetInst()) {
                    const CSeq_inst& inst = context.CurrentBioseq().GetInst();
                    if (inst.GetTopology() == CSeq_inst::eTopology_circular) {
                        unsigned int len = inst.GetLength();
                        CSeq_loc_CI ci0(feat.GetLocation());
                        if (ci0) {
                            CSeq_loc_CI ci1 = ci0;
                            ++ci1;
                            if (ci1) {
                                CSeq_loc_CI ci2 = ci1;
                                ++ci2;
                                if (!ci2) { // location has exactly 2 intervals
                                    if (ci0.GetStrand() == eNa_strand_plus && ci1.GetStrand() == eNa_strand_plus) {
                                        if (ci0.GetRange().GetTo() == len - 1 && ci1.GetRange().GetFrom() == 0) {
                                            bad = false;
                                        }
                                    }
                                    else if (ci0.GetStrand() == eNa_strand_minus && ci1.GetStrand() == eNa_strand_minus) {
                                        if (ci1.GetRange().GetTo() == len - 1 && ci0.GetRange().GetFrom() == 0) {
                                            bad = false;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                m_Objs["[n] coding region[s] with joined location[s] [has] no exception[s]"][bad ? "[n] coding region[s] not over the origin of circular DNA" : "[n] coding region[s] over the origin of circular DNA"].Severity(bad ? CReportItem::eSeverity_error : CReportItem::eSeverity_warning).Add(*context.SeqFeatObjRef(feat));
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(BACTERIAL_JOINED_FEATURES_NO_EXCEPTION)
{
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


// RIBOSOMAL_SLIPPAGE

DISCREPANCY_CASE(RIBOSOMAL_SLIPPAGE, FEAT, eDisc | eSmart | eFatal, " Only a select number of proteins undergo programmed frameshifts due to ribosomal slippage")
{
    const CSeqdesc* biosrc = context.GetBiosource();
    if (biosrc && !context.IsEukaryotic(&biosrc->GetSource()) && !context.IsOrganelle(&biosrc->GetSource())) {
        for (auto& feat : context.GetFeat()) {
            if (feat.IsSetLocation() && feat.CanGetData() && feat.GetData().IsCdregion() && feat.IsSetExcept_text() && (feat.GetLocation().IsMix() || feat.GetLocation().IsPacked_int())) {
                if (feat.GetExcept_text().find("ribosomal slippage") != string::npos) {
                    //string product = GetProductForCDS(feat, context.GetScope()); // sema: may need to change when we start using CFeatTree
                    string product = context.GetProdForFeature(feat);
                    static string ignore1[] = { "transposase", "chain release" };
                    static string ignore2[] = { "IS150 protein InsAB", "PCRF domain-containing protein" };
                    static size_t len1 = sizeof(ignore1) / sizeof(ignore1[0]);
                    static size_t len2 = sizeof(ignore2) / sizeof(ignore2[0]);
                    for (size_t n = 0; n < len1; n++) {
                        if (product.find(ignore1[n]) != string::npos) {
                            return;
                        }
                    }
                    for (size_t n = 0; n < len2; n++) {
                        if (product == ignore2[n]) {
                            return;
                        }
                    }
                    m_Objs["[n] coding region[s] [has] unexpected ribosomal slippage"].Fatal().Add(*context.SeqFeatObjRef(feat));
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(RIBOSOMAL_SLIPPAGE)
{
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


// SHORT_INTRON

const string kShortIntronTop = "[n] intron[s] [is] shorter than 10 nt";
const string kShortIntronExcept = "[n] intron[s] [is] shorter than 11 nt and [has] an exception";

DISCREPANCY_CASE(SHORT_INTRON, FEAT, eDisc | eOncaller | eSubmitter | eSmart, "Introns shorter than 10 nt")
{
    for (auto& feat : context.GetFeat()) {
        if (feat.IsSetData() && feat.GetData().IsCdregion() && feat.IsSetLocation() && !feat.IsSetExcept() && !context.IsPseudo(feat)) {
            CSeq_loc_CI li(feat.GetLocation());
            if (li) {
                bool found_short = false;
                TSeqPos last_start = li.GetRange().GetFrom();
                TSeqPos last_stop = li.GetRange().GetTo();
                ++li;
                while (li && !found_short) {
                    TSeqPos start = li.GetRange().GetFrom();
                    TSeqPos stop = li.GetRange().GetTo();
                    if (start >= last_stop && start - last_stop < 11) {
                        found_short = true;
                    }
                    else if (last_stop >= start && last_stop - start < 11) {
                        found_short = true;
                    }
                    else if (stop >= last_start && stop - last_start < 11) {
                        found_short = true;
                    }
                    else if (last_start >= stop && last_start - stop < 11) {
                        found_short = true;
                    }
                    last_start = start;
                    last_stop = stop;
                    ++li;
                }
                if (found_short) {
                    //if (obj.IsSetExcept() && obj.GetExcept()) {
                    //    m_Objs[kShortIntronTop][kShortIntronExcept].Ext().Add(*context.DiscrObj(obj, true));
                    //}
                    m_Objs[kShortIntronTop].Add(*context.SeqFeatObjRef(feat, CDiscrepancyContext::eFixSet));
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(SHORT_INTRON)
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
        catch (CException&) { // LCOV_EXCL_START
            return false;
        } // LCOV_EXCL_STOP
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
            CConstRef<CSeq_feat> gene = sequence::GetGeneForFeature(sf, scope);
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
    unsigned int n = 0;
    const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(context.FindObject(*obj));
    std::list<CConstRef<CSeq_loc>> to_remove;
    if (AddExceptionsToShortIntron(*sf, context.GetScope(), to_remove)) {
        n++;
    }
    for (auto& loc : to_remove) {
        CBioseq_Handle bioseq_h = context.GetScope().GetBioseqHandle(*loc);
        CBioseq_EditHandle bioseq_edit = bioseq_h.GetEditHandle();
        bioseq_edit.Remove();
    }
    obj->SetFixed();
    return CRef<CAutofixReport>(n ? new CAutofixReport("SHORT_INTRON: Set exception for [n] feature[s]", n) : 0);
}


// UNNECESSARY_VIRUS_GENE

DISCREPANCY_CASE(UNNECESSARY_VIRUS_GENE, FEAT, eOncaller, "Unnecessary gene features on virus: on when lineage is not Picornaviridae,Potyviridae,Flaviviridae and Togaviridae")
{
    const CSeqdesc* biosrc = context.GetBiosource();
    if (biosrc) {
        const CBioSource* src = &biosrc->GetSource();
        if (context.HasLineage(src, "Picornaviridae") || context.HasLineage(src, "Potyviridae") || context.HasLineage(src, "Flaviviridae") || context.HasLineage(src, "Togaviridae")) {
            for (auto& feat : context.GetFeat()) {
                if (feat.IsSetData() && feat.GetData().IsGene()) {
                    m_Objs["[n] virus gene[s] need to be removed"].Add(*context.SeqFeatObjRef(feat));
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(UNNECESSARY_VIRUS_GENE)
{
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


// CDS_HAS_CDD_XREF

DISCREPANCY_CASE(CDS_HAS_CDD_XREF, FEAT, eDisc | eOncaller, "CDS has CDD Xref")
{
    for (auto& feat : context.GetFeat()) {
        if (feat.IsSetData() && feat.GetData().IsCdregion() && feat.IsSetDbxref()) {
            for (auto& x : feat.GetDbxref()) {
                if (x->IsSetDb() && NStr::EqualNocase(x->GetDb(), "CDD")) {
                    m_Objs["[n] feature[s] [has] CDD Xrefs"].Add(*context.SeqFeatObjRef(feat));
                    break;
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(CDS_HAS_CDD_XREF)
{
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


// SHOW_TRANSL_EXCEPT

DISCREPANCY_CASE(SHOW_TRANSL_EXCEPT, FEAT, eDisc | eSubmitter | eSmart, "Show translation exception")
{
    for (auto& feat : context.GetFeat()) {
        if (feat.IsSetData() && feat.GetData().IsCdregion() && feat.GetData().GetCdregion().IsSetCode_break()) {
             m_Objs["[n] coding region[s] [has] a translation exception"].Add(*context.SeqFeatObjRef(feat));
        }
    }
}


DISCREPANCY_SUMMARIZE(SHOW_TRANSL_EXCEPT)
{
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


// NO_PRODUCT_STRING

static const string kNoProductStr = "[n] product[s] [has] \"no product string in file\"";

DISCREPANCY_CASE(NO_PRODUCT_STRING, FEAT, eDisc, "Product has string \"no product string in file\"")
{
    for (auto& feat : context.GetFeat()) {
        if (feat.IsSetData() && feat.GetData().IsProt()) {
            const CProt_ref& prot = feat.GetData().GetProt();
            if (prot.IsSetName()) {
                const string* no_prot_str = NStr::FindNoCase(prot.GetName(), "no product string in file");
                if (no_prot_str != nullptr) {
                    const CSeq_feat* product = sequence::GetCDSForProduct(context.CurrentBioseq(), &context.GetScope());
                    if (product != nullptr) {
                        m_Objs[kNoProductStr].Add(*context.SeqFeatObjRef(*product), false);
                    }
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(NO_PRODUCT_STRING)
{
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


// UNWANTED_SPACER

static const string kIntergenicSpacerNames[] = {
    "trnL-trnF intergenic spacer",
    "trnH-psbA intergenic spacer",
    "trnS-trnG intergenic spacer",
    "trnF-trnL intergenic spacer",
    "psbA-trnH intergenic spacer",
    "trnG-trnS intergenic spacer" };

static const size_t kIntergenicSpacerNames_len = sizeof(kIntergenicSpacerNames) / sizeof(kIntergenicSpacerNames[0]);


DISCREPANCY_CASE(UNWANTED_SPACER, FEAT, eOncaller, "Intergenic spacer without plastid location")
{
    const CSeqdesc* biosrc = context.GetBiosource();
    if (biosrc && biosrc->GetSource().IsSetGenome() && (biosrc->GetSource().GetGenome() == CBioSource::eGenome_chloroplast || biosrc->GetSource().GetGenome() == CBioSource::eGenome_plastid)) {
        return;
    }
    if (biosrc && biosrc->GetSource().IsSetOrg() && biosrc->GetSource().GetOrg().IsSetTaxname() && CDiscrepancyContext::IsUnculturedNonOrganelleName(biosrc->GetSource().GetOrg().GetTaxname())) {
        return;
    }
    for (auto& feat : context.GetFeat()) {
        if (feat.IsSetData() && feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_misc_feature) {
            for (size_t i = 0; i < kIntergenicSpacerNames_len; i++) {
                if (NStr::FindNoCase(feat.GetComment(), kIntergenicSpacerNames[i]) != string::npos) {
                    m_Objs["[n] suspect intergenic spacer note[s] not organelle"].Add(*context.SeqFeatObjRef(feat));
                    break;
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(UNWANTED_SPACER)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// CHECK_RNA_PRODUCTS_AND_COMMENTS

DISCREPANCY_CASE(CHECK_RNA_PRODUCTS_AND_COMMENTS, FEAT, eOncaller, "Check for gene or genes in rRNA and tRNA products and comments")
{
    for (auto& feat : context.GetFeat()) {
        if (feat.IsSetData() && feat.GetData().IsRna()) {
            const CRNA_ref& rna = feat.GetData().GetRna();
            if ((rna.IsSetType() && rna.GetType() == CRNA_ref::eType_rRNA) || rna.GetType() == CRNA_ref::eType_tRNA) {
                string product = rna.GetRnaProductName();
                string comment;
                if (feat.IsSetComment()) {
                    comment = feat.GetComment();
                }
                if (NStr::FindNoCase(product, "gene") != NPOS || NStr::FindNoCase(comment, "gene") != NPOS) {
                    m_Objs["[n] RNA product_name or comment[s] contain[S] 'suspect phrase'"].Add(*context.SeqFeatObjRef(feat));
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(CHECK_RNA_PRODUCTS_AND_COMMENTS)
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
        ENa_strand gene_strand = gene_i.GetStrand();
        if (!StrandsMatch(feat_i.GetStrand(), gene_strand) ||
            feat_i.GetRangeAsSeq_loc()->GetStart(eExtreme_Biological) != gene_i.GetRangeAsSeq_loc()->GetStart(eExtreme_Biological)) {
            return false;
        }
        bool found_stop = false;
        while (!found_stop && feat_i && StrandsMatch(feat_i.GetStrand(), gene_strand)) {
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


bool StopAbutsGap(const CSeq_loc& loc, ENa_strand strand, CScope& scope)
{
    try {
        CBioseq_Handle bsh = scope.GetBioseqHandle(loc);
        TSeqPos stop = loc.GetStop(eExtreme_Biological);
        if (stop < 1 || stop > bsh.GetBioseqLength() - 2) {
            return false;
        }
        CRef<CSeq_loc> search(new CSeq_loc());
        search->SetInt().SetId().Assign(*(loc.GetId()));
        if (strand == eNa_strand_minus) {
            search->SetInt().SetFrom(stop - 1);
            search->SetInt().SetTo(stop - 1);
            search->SetInt().SetStrand(eNa_strand_minus);
        } else {
            search->SetInt().SetFrom(stop + 1);
            search->SetInt().SetTo(stop + 1);
        }
        CSeqVector vec(*search, scope);
        if (vec.size() && vec.IsInGap(0)) {
            return true;
        }
    }
    catch (CException& ) { // LCOV_EXCL_START
        // unable to calculate
    } // LCOV_EXCL_STOP
    return false;
}


bool StartAbutsGap(const CSeq_loc& loc, ENa_strand strand, CScope& scope)
{
    try {
        CBioseq_Handle bsh = scope.GetBioseqHandle(loc);
        TSeqPos start = loc.GetStart(eExtreme_Biological);
        if (start < 1 || start > bsh.GetBioseqLength() - 2) {
            return false;
        }
        CRef<CSeq_loc> search(new CSeq_loc());
        search->SetInt().SetId().Assign(*(loc.GetId()));
        if (strand == eNa_strand_minus) {
            search->SetInt().SetFrom(start + 1);
            search->SetInt().SetTo(start + 1);
            search->SetInt().SetStrand(eNa_strand_minus);
        } else {
            search->SetInt().SetFrom(start - 1);
            search->SetInt().SetTo(start - 1);
        }
        CSeqVector vec(*search, scope);
        if (vec.IsInGap(0)) {
            return true;
        }
    }
    catch (CException& ) { // LCOV_EXCL_START
        // unable to calculate
    } // LCOV_EXCL_STOP
    return false;
}


// location is ok if:
// 1. endpoints match exactly, or 
// 2. non-matching 5' endpoint can be extended by an RBS feature to match gene start, or
// 3. if coding region non-matching endpoints are partial and abut a gap
bool IsGeneLocationOk(const CSeq_loc& feat_loc, const CSeq_loc& gene_loc, ENa_strand feat_strand, ENa_strand gene_strand, bool is_coding_region, CScope& scope, const vector<const CSeq_feat*>& features)
{
    if (IsMixedStrand(feat_loc) || IsMixedStrand(gene_loc)) {
        // special handling for trans-spliced
        return IsMixedStrandGeneLocationOk(feat_loc, gene_loc);
    } else if (!StrandsMatch(feat_strand, gene_strand)) {
        return false;
    } else if (gene_loc.GetStop(eExtreme_Biological) != feat_loc.GetStop(eExtreme_Biological)) {
        if (is_coding_region && feat_loc.IsPartialStop(eExtreme_Biological) && StopAbutsGap(feat_loc, feat_strand, scope)) {
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
    TSeqPos rbs_start = rbs_search->GetStart(eExtreme_Biological);
    for (auto& feat : features) {
        if (feat->GetLocation().GetStart(eExtreme_Biological) == rbs_start && IsRBS(*feat)) {
            return true;
        }
    }
    if (is_coding_region && feat_loc.IsPartialStart(eExtreme_Biological) && StartAbutsGap(feat_loc, feat_strand, scope)) {
        // check to see if 5' end is partial and abuts gap
        return true;
    }
    return false;
}


bool GeneRefMatch(const CGene_ref& g1, const CGene_ref& g2)
{
    return g1.IsSetLocus() == g2.IsSetLocus() && (!g1.IsSetLocus() || g1.GetLocus() == g2.GetLocus())
        && g1.IsSetLocus_tag() == g2.IsSetLocus_tag() && (!g1.IsSetLocus_tag() || g1.GetLocus_tag() == g2.GetLocus_tag())
        && g1.IsSetAllele() == g2.IsSetAllele() && (!g1.IsSetAllele() || g1.GetAllele() == g2.GetAllele())
        && g1.IsSetDesc() == g2.IsSetDesc() && (!g1.IsSetDesc() || g1.GetDesc() == g2.GetDesc())
        && g1.IsSetMaploc() == g2.IsSetMaploc() && (!g1.IsSetMaploc() || g1.GetMaploc() == g2.GetMaploc())
        && g1.IsSetPseudo() == g2.IsSetPseudo()
    ;
}


static string GetNextSubitemId(size_t num)
{
    string ret = "[*";
    ret += NStr::SizetToString(num);
    ret += "*]";
    return ret;
}


DISCREPANCY_CASE(FEATURE_LOCATION_CONFLICT, SEQUENCE, eDisc | eSubmitter | eSmart, "Feature Location Conflict")
{
   if (context.InGenProdSet()) {
        return;
    }
    const CSeqdesc* biosrc = context.GetBiosource();
    bool eucariotic = context.IsEukaryotic(biosrc ? &biosrc->GetSource() : 0);
    auto& all = context.FeatAll();
    for (auto& feat : all) {
        if (feat->IsSetData() && feat->IsSetLocation() && (feat->GetData().IsRna() || (!eucariotic && feat->GetData().IsCdregion()))) {
            ENa_strand feat_strand = feat->GetLocation().GetStrand();
            const CGene_ref* gx = feat->GetGeneXref();
            const CSeq_feat* gene = context.GetGeneForFeature(*feat);
            if (!gene || (gx && !gx->IsSuppressed() && !GeneRefMatch(*gx, gene->GetData().GetGene()))) {
                if (feat->GetGeneXref()) {
                    string subitem_id = GetNextSubitemId(m_Objs[kFeatureLocationConflictTop].GetMap().size());
                    if (feat->GetData().IsCdregion()) {
                        m_Objs[kFeatureLocationConflictTop]["Coding region xref gene does not exist" + subitem_id].Ext().Add(*context.SeqFeatObjRef(*feat), false);
                    }
                    else {
                        m_Objs[kFeatureLocationConflictTop]["RNA feature xref gene does not exist" + subitem_id].Ext().Add(*context.SeqFeatObjRef(*feat), false);
                    }
                    m_Objs[kFeatureLocationConflictTop].Incr();
                }
            }
            else if (gene->IsSetLocation()) {
                ENa_strand gene_strand = gene->GetLocation().GetStrand();
                if (!IsGeneLocationOk(feat->GetLocation(), gene->GetLocation(), feat_strand, gene_strand, feat->GetData().IsCdregion(), context.GetScope(), all)) {
                    string subitem_id = GetNextSubitemId(m_Objs[kFeatureLocationConflictTop].GetMap().size());
                    if (feat->GetData().IsCdregion()) {
                        m_Objs[kFeatureLocationConflictTop][kFeatureLocationCodingRegion + subitem_id].Ext().Add(*context.SeqFeatObjRef(*feat), false).Add(*context.SeqFeatObjRef(*gene), false);
                    }
                    else {
                        m_Objs[kFeatureLocationConflictTop][kFeatureLocationRNA + subitem_id].Ext().Add(*context.SeqFeatObjRef(*feat), false).Add(*context.SeqFeatObjRef(*gene), false);
                    }
                    m_Objs[kFeatureLocationConflictTop].Incr();
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(FEATURE_LOCATION_CONFLICT)
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


DISCREPANCY_CASE(SUSPECT_PHRASES, FEAT, eDisc | eSubmitter | eSmart, "Suspect Phrases")
{
    for (auto& feat : context.GetFeat()) {
        if (feat.IsSetData()) {
            string check;
            if (feat.GetData().IsCdregion() && feat.IsSetComment()) {
                check = feat.GetComment();
            }
            else if (feat.GetData().IsProt() && feat.GetData().GetProt().IsSetDesc()) {
                check = feat.GetData().GetProt().GetDesc();
            }
            if (!check.empty()) {
                for (size_t i = 0; i < sizeof(suspect_phrases) / sizeof(string); i++) {
                    if (NStr::FindNoCase(check, suspect_phrases[i]) != string::npos) {
                        m_Objs["[n] cds comment[s] or protein description[s] contain[S] suspect_phrase[s]"]["[n] cds comment[s] or protein description[s] contain[S] '" + suspect_phrases[i] + "'"].Summ().Add(*context.SeqFeatObjRef(feat));
                        break;
                    }
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(SUSPECT_PHRASES)
{
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


// UNUSUAL_MISC_RNA

DISCREPANCY_CASE(UNUSUAL_MISC_RNA, FEAT, eDisc | eSubmitter | eSmart, "Unexpected misc_RNA features")
{
    for (auto& feat : context.GetFeat()) {
        if (feat.IsSetData() && feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_otherRNA) {
            const CRNA_ref& rna = feat.GetData().GetRna();
            string product = rna.GetRnaProductName();
            if (NStr::FindCase(product, "ITS", 0) == NPOS && NStr::FindCase(product, "internal transcribed spacer", 0) == NPOS) {
                m_Objs["[n] unexpected misc_RNA feature[s] found.  misc_RNAs are unusual in a genome, consider using ncRNA, misc_binding, or misc_feature as appropriate"].Add(*context.SeqFeatObjRef(feat));
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(UNUSUAL_MISC_RNA)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// CDS_WITHOUT_MRNA

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
    string rna_rest = rna_product.substr(pos_in_rna + kmRNAVariant.size()), cds_rest = cds_product.substr(pos_in_cds + kCDSVariant.size());
    return rna_rest == cds_rest;
}


DISCREPANCY_CASE(CDS_WITHOUT_MRNA, SEQUENCE, eDisc | eOncaller | eSmart, "Coding regions on eukaryotic genomic DNA should have mRNAs with matching products")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    const CSeqdesc* biosrc = context.GetBiosource();
    const CBioSource* src = biosrc ? &biosrc->GetSource() : 0;
    if (!context.IsEukaryotic(src) || context.IsOrganelle(src) || !bioseq.GetInst().IsSetMol() || bioseq.GetInst().GetMol() != CSeq_inst::eMol_dna) {
        return;
    }

    vector<const CSeq_feat*> cds = context.FeatCDS();
    vector<const CSeq_feat*> mrnas = context.FeatMRNAs();
    auto cds_it = cds.begin();
    while (cds_it != cds.end()) {
        if (context.IsPseudo(**cds_it)) {
            cds_it = cds.erase(cds_it);
            continue;
        }
        const CSeq_feat* mrna = 0;
        if ((*cds_it)->IsSetXref()) {
            auto rna_it = mrnas.cbegin();
            while (rna_it != mrnas.end()) {
                if ((*rna_it)->IsSetId()) {
                    auto& rnaid = (*rna_it)->GetId();
                    if (rnaid.IsLocal()) {
                        for (auto xref : (*cds_it)->GetXref()) {
                            if (xref->IsSetId()) {
                                auto& id = xref->GetId();
                                if (id.IsLocal()) {
                                    if (!id.GetLocal().Compare(rnaid.GetLocal())) {
                                        mrna = *rna_it;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    if (mrna) {
                        mrnas.erase(rna_it);
                        break;
                    }
                }
                ++rna_it;
            }
        }
        if (mrna) {
            string prod = context.GetProdForFeature(**cds_it);
            if (!IsProductMatch(prod, mrna->GetData().GetRna().GetRnaProductName())) {
                m_Objs["[n] coding region[s] [has] mismatching mRNA"].Add(*context.SeqFeatObjRef(**cds_it));
            }
            cds_it = cds.erase(cds_it);
            continue;
        }
        ++cds_it;
    }

    for (size_t i = 0; i < cds.size(); i++) {
        if (context.IsPseudo(*cds[i])) {
            continue;
        }
        bool found = false;
        string prod = context.GetProdForFeature(*cds[i]);
        const CSeq_loc& loc_i = cds[i]->GetLocation();
        for (size_t j = 0; j < mrnas.size(); j++) {
            const CSeq_loc& loc_j = mrnas[j]->GetLocation();
            sequence::ECompare compare = context.Compare(loc_j, loc_i);
            if (compare == sequence::eContains || compare == sequence::eSame) {
                if (IsProductMatch(prod, mrnas[j]->GetData().GetRna().GetRnaProductName())) {
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            m_Objs["[n] coding region[s] [does] not have an mRNA"].Add(*context.SeqFeatObjRef(*cds[i], CDiscrepancyContext::eFixSet));
        }
    }
}


DISCREPANCY_SUMMARIZE(CDS_WITHOUT_MRNA)
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
    else {
        CSeq_feat_EditHandle old_mRNA_edit(scope.GetSeq_featHandle(*old_mRNA));
        old_mRNA_edit.Replace(*new_mRNA);
    }
    return true;
}


static CSeq_annot_EditHandle GetAnnotHandle(CScope& scope, CBioseq_Handle bsh)
{
    CSeq_annot_Handle ftable;
    CSeq_annot_CI annot_ci(bsh.GetParentEntry(), CSeq_annot_CI::eSearch_entry);
    for (; annot_ci; ++annot_ci) {
        if (annot_ci->IsFtable()) {
            ftable = *annot_ci;
            break;
        }
    }
    if (!ftable) {
        CBioseq_EditHandle eh = bsh.GetEditHandle();
        CRef<CSeq_annot> new_annot(new CSeq_annot());
        ftable = eh.AttachAnnot(*new_annot);
    }
    CSeq_annot_EditHandle aeh(ftable);
    return aeh;
}


DISCREPANCY_AUTOFIX(CDS_WITHOUT_MRNA)
{
    const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(context.FindObject(*obj));
    CConstRef<CSeq_feat> old_mRNA = sequence::GetmRNAforCDS(*sf, context.GetScope());
    CRef<CSeq_feat> new_mRNA = edit::MakemRNAforCDS(*sf, context.GetScope());
    if (old_mRNA.Empty()) {
        CBioseq_Handle bh = context.GetScope().GetBioseqHandle(new_mRNA->GetLocation());
        CSeq_annot_EditHandle annot_handle = GetAnnotHandle(context.GetScope(), bh);
        annot_handle.AddFeat(*new_mRNA);
    }
    else {
        CSeq_feat_EditHandle old_mRNA_edit(context.GetScope().GetSeq_featHandle(*old_mRNA));
        old_mRNA_edit.Replace(*new_mRNA);
    }
    obj->SetFixed();
    return CRef<CAutofixReport>(new CAutofixReport("CDS_WITHOUT_MRNA: Add mRNA for [n] CDS feature[s]", 1));
}


// PROTEIN_NAMES

DISCREPANCY_CASE(PROTEIN_NAMES, FEAT, eDisc | eSubmitter | eSmart, "Frequently appearing proteins")
{
    for (auto& feat : context.GetFeat()) {
        if (feat.IsSetData() && feat.GetData().IsProt()) {
            const CProt_ref& prot = feat.GetData().GetProt();
            if (prot.IsSetName() && !prot.GetName().empty()) {
                m_Objs[feat.GetData().GetProt().GetName().front()].Incr();
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(PROTEIN_NAMES)
{
    static const size_t MIN_REPORTABLE_AMOUNT = 100;
    auto& M = m_Objs.GetMap();
    if (M.size() == 1 && M.begin()->second->GetCount() >= MIN_REPORTABLE_AMOUNT) {
        CReportNode rep;
        rep["All proteins have same name [(]\"" + M.begin()->first + "\""];
        m_ReportItems = rep.Export(*this)->GetSubitems();
    }
}


// MRNA_SHOULD_HAVE_PROTEIN_TRANSCRIPT_IDS

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


DISCREPANCY_CASE(MRNA_SHOULD_HAVE_PROTEIN_TRANSCRIPT_IDS, SEQUENCE, eDisc | eSubmitter | eSmart | eFatal, "mRNA should have both protein_id and transcript_id")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    const CSeqdesc* biosrc = context.GetBiosource();
    if (biosrc && context.IsEukaryotic(&biosrc->GetSource()) && bioseq.GetInst().IsSetMol() && bioseq.GetInst().GetMol() == CSeq_inst::eMol_dna) {
        for (auto& feat : context.GetAllFeat()) {
            if (feat.IsSetData() && feat.GetData().IsCdregion() && !context.IsPseudo(feat)) {
                CConstRef<CSeq_feat> mRNA = sequence::GetmRNAforCDS(feat, context.GetScope());
                if (mRNA && (!mRNA->IsSetQual() || !IsmRnaQualsPresent(mRNA->GetQual()))) {
                    m_Objs.Add(*context.SeqFeatObjRef(feat)).Fatal();
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(MRNA_SHOULD_HAVE_PROTEIN_TRANSCRIPT_IDS)
{
    if (!m_Objs.empty()) {
        CReportNode out;
        out["no protein_id and transcript_id present"];
        m_ReportItems = out.Export(*this)->GetSubitems();
    }
}


// FEATURE_LIST

static const string kFeatureList = "Feature List";

DISCREPANCY_CASE(FEATURE_LIST, FEAT, eDisc | eSubmitter, "Feature List")
{
    for (auto& feat : context.GetFeat()) {
        if (feat.IsSetData() && feat.GetData().GetSubtype() != CSeqFeatData::eSubtype_gap && feat.GetData().GetSubtype() != CSeqFeatData::eSubtype_prot) {
            string subitem = "[n] " + feat.GetData().GetKey();
            subitem += " feature[s]";
            m_Objs[kFeatureList].Info()[subitem].Info().Add(*context.SeqFeatObjRef(feat));
        }
    }
}


DISCREPANCY_SUMMARIZE(FEATURE_LIST)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// MULTIPLE_QUALS

DISCREPANCY_CASE(MULTIPLE_QUALS, FEAT, eDisc | eOncaller, "Multiple qualifiers")
{
    for (auto& feat : context.GetFeat()) {
        if (feat.IsSetQual()) {
            size_t num_of_number_quals = 0;
            for (auto& qual : feat.GetQual()) {
                if (qual->IsSetQual() && qual->GetQual() == "number") {
                    ++num_of_number_quals;
                    if (num_of_number_quals > 1) {
                        m_Objs["[n] feature[s] contain[S] multiple /number qualifiers"].Add(*context.SeqFeatObjRef(feat));
                        break;
                    }
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(MULTIPLE_QUALS)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}



// MISC_FEATURE_WITH_PRODUCT_QUAL

DISCREPANCY_CASE(MISC_FEATURE_WITH_PRODUCT_QUAL, FEAT, eDisc | eOncaller | eSubmitter | eSmart, "Misc features containing a product qualifier")
{
    for (auto& feat : context.GetFeat()) {
        if (feat.IsSetData() && feat.IsSetQual() && feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_misc_feature) {
            for (auto& qual : feat.GetQual()) {
                if (qual->IsSetQual() && qual->GetQual() == "product") {
                    m_Objs["[n] feature[s] [has] a product qualifier"].Add(*context.SeqFeatObjRef(feat));
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(MISC_FEATURE_WITH_PRODUCT_QUAL)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}



// CDS_HAS_NO_ADJACENT_TRNA

const string kCDShasNoTRNA = "[n] coding region[s] [does] not have adjacent tRNA";

static bool IsStopCodon(const CCode_break::C_Aa& aa)
{
    int aa_idx = -1;
    switch (aa.Which()) {
        case CCode_break::C_Aa::e_Ncbieaa:
            aa_idx = aa.GetNcbieaa();
            aa_idx = CSeqportUtil::GetMapToIndex(CSeq_data::e_Ncbieaa, CSeq_data::e_Ncbistdaa, aa_idx);
            break;
        case CCode_break::C_Aa::e_Ncbi8aa:
            aa_idx = aa.GetNcbi8aa();
            break;
        case CCode_break::C_Aa::e_Ncbistdaa:
            aa_idx = aa.GetNcbistdaa();
            break;
        default:
            break;
    }
    static const int STOP_CODON = 25;
    return aa_idx == STOP_CODON;
}


DISCREPANCY_CASE(CDS_HAS_NO_ADJACENT_TRNA, SEQUENCE, eDisc, "CDSs should have adjacent tRNA")
{
    const CSeqdesc* biosrc = context.GetBiosource();
    if (!biosrc || biosrc->GetSource().GetGenome() != CBioSource::eGenome_mitochondrion) {
        return;
    }
    auto& cds = context.FeatCDS();
    auto& trnas = context.FeatTRNAs();
    for (size_t i = 0; i < cds.size(); i++) {
        if (!cds[i]->GetData().GetCdregion().IsSetCode_break()) {
            continue;
        }
        const CCode_break& code_break = *cds[i]->GetData().GetCdregion().GetCode_break().front();
        if (!code_break.IsSetAa() || !IsStopCodon(code_break.GetAa())) {
            continue;
        }
        ENa_strand strand = cds[i]->GetLocation().IsSetStrand() ? cds[i]->GetLocation().GetStrand() : eNa_strand_unknown;
        TSeqPos stop = cds[i]->GetLocation().GetStop(eExtreme_Biological);
        const CSeq_feat* nearest_trna = nullptr;
        TSeqPos diff = UINT_MAX;
        for (auto& trna : trnas) {
            if (trna->IsSetLocation()) {
                TSeqPos start = trna->GetLocation().GetStart(eExtreme_Biological);
                TSeqPos cur_diff = UINT_MAX;
                if (strand == eNa_strand_minus) {
                    if (start <= stop) {
                        cur_diff = stop - start;
                    }
                }
                else {
                    if (start >= stop) {
                        cur_diff = start - stop;
                    }
                }
                if (cur_diff < diff) {
                    diff = cur_diff;
                    nearest_trna = trna;
                }
            }
        }
        if (nearest_trna) {
            ENa_strand trna_strand = nearest_trna->GetLocation().IsSetStrand() ? nearest_trna->GetLocation().GetStrand() : eNa_strand_unknown;
            if (trna_strand == strand && diff > 1) {
                m_Objs[kCDShasNoTRNA].Add(*context.SeqFeatObjRef(*cds[i]), false).Incr();
                m_Objs[kCDShasNoTRNA].Add(*context.SeqFeatObjRef(*nearest_trna), false);
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(CDS_HAS_NO_ADJACENT_TRNA)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// MITO_RRNA

DISCREPANCY_CASE(MITO_RRNA, SEQUENCE, eOncaller, "Non-mitochondrial rRNAs with 12S/16S")
{
    const CSeqdesc* biosrc = context.GetBiosource();
    if (context.IsEukaryotic(biosrc ? &biosrc->GetSource() : 0)) {
        auto& rnas = context.Feat_RNAs();
        for (size_t i = 0; i < rnas.size(); i++) {
            if (rnas[i]->GetData().GetSubtype() == CSeqFeatData::eSubtype_rRNA && rnas[i]->GetData().GetRna().IsSetExt() && rnas[i]->GetData().GetRna().GetExt().IsName()) {
                const string& name = rnas[i]->GetData().GetRna().GetExt().GetName();
                if (name.find("16S") != string::npos || name.find("12S") != string::npos) {
                    m_Objs["[n] non mitochondrial rRNA name[s] contain[S] 12S/16S"].Add(*context.SeqFeatObjRef(*rnas[i]));
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(MITO_RRNA)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
