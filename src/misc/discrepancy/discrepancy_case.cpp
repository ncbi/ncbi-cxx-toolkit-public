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
 * Authors: Sema Kachalo
 *
 */

#include <ncbi_pch.hpp>
#include "discrepancy_core.hpp"
#include "utils.hpp"
#include <objects/general/Dbtag.hpp>
#include <objects/macro/Molecule_type.hpp>
#include <objects/macro/Molecule_class_type.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/RNA_gen.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/macro/String_constraint.hpp>
#include <objects/misc/sequence_macros.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <sstream>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

DISCREPANCY_MODULE(discrepancy_case);


// COUNT_NUCLEOTIDES

DISCREPANCY_CASE(COUNT_NUCLEOTIDES, SEQUENCE, eOncaller | eSubmitter | eSmart | eBig, "Count nucleotide sequences")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.CanGetInst() && bioseq.GetInst().IsNa()) {
        auto& node = m_Objs["[n] nucleotide Bioseq[s] [is] present"].Info();
        node.Incr();
        node.GetCount();
        node.Add(*context.BioseqObjRef());
    }
}


DISCREPANCY_SUMMARIZE(COUNT_NUCLEOTIDES)
{
    m_Objs["[n] nucleotide Bioseq[s] [is] present"]; // If no sequences found still report 0
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// COUNT_PROTEINS

DISCREPANCY_CASE(COUNT_PROTEINS, SEQUENCE, eDisc, "Count Proteins")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.CanGetInst() && bioseq.GetInst().IsAa()) {
        auto& node = m_Objs["[n] protein sequence[s] [is] present"].Info();
        node.Incr();
        node.GetCount();
        node.Add(*context.BioseqObjRef());
    }
}


DISCREPANCY_SUMMARIZE(COUNT_PROTEINS)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


static const CSeq_id* GetProteinId(const CBioseq& seq)
{
    for (auto& id_it : seq.GetId()) {
        const CSeq_id& seq_id = *id_it;
        if (seq_id.IsGeneral() && !seq_id.GetGeneral().IsSkippable()) {
            return &seq_id;
        }
    }
    return nullptr;
}


// MISSING_PROTEIN_ID
DISCREPANCY_CASE(MISSING_PROTEIN_ID, SEQUENCE, eDisc | eSubmitter | eSmart | eFatal, "Missing Protein ID")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.CanGetInst() && bioseq.GetInst().IsAa() && !GetProteinId(bioseq)) {
        m_Objs["[n] protein[s] [has] invalid ID[s]."].Add(*context.BioseqObjRef()).Fatal();
    }
}


DISCREPANCY_SUMMARIZE(MISSING_PROTEIN_ID)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// INCONSISTENT_PROTEIN_ID
DISCREPANCY_CASE(INCONSISTENT_PROTEIN_ID, SEQUENCE, eDisc | eSubmitter | eSmart | eFatal, "Inconsistent Protein ID")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.CanGetInst() && bioseq.GetInst().IsAa()) {
        const CSeq_id* protein_id = GetProteinId(bioseq);
        if (protein_id) {
            _ASSERT(protein_id->IsGeneral());
            CTempString protein_id_prefix(GET_STRING_FLD_OR_BLANK(protein_id->GetGeneral(), Db));
            if (protein_id_prefix.empty()) {
                return;
            }
            string protein_id_prefix_lowercase = protein_id_prefix;
            NStr::ToLower(protein_id_prefix_lowercase);
            // find (or create if none before) the canonical form of the
            // protein_id_prefix since case-insensitivity means it could have
            // multiple forms.  Here, the canonical form is the way it appears
            // the first time.
            CReportNode& canonical_forms_node = m_Objs["canonical forms"][protein_id_prefix_lowercase];
            string canonical_protein_id_prefix;
            if (canonical_forms_node.empty()) {
                // haven't seen this protein_id_prefix_lowercase before so we have
                // to set the canonical form.
                canonical_protein_id_prefix = protein_id_prefix;
                canonical_forms_node[protein_id_prefix];
            }
            else {
                // use previously set canonical form;
                canonical_protein_id_prefix = canonical_forms_node.GetMap().begin()->first;
            }
            _ASSERT(NStr::EqualNocase(protein_id_prefix, canonical_protein_id_prefix));
            m_Objs[kEmptyStr]["[n] sequence[s] [has] protein ID prefix [(]" + canonical_protein_id_prefix].Fatal().Add(*context.BioseqObjRef());
        }
    }
}


DISCREPANCY_SUMMARIZE(INCONSISTENT_PROTEIN_ID)
{
    // if _all_ are identical, don't report
    CReportNode& reports_collected = m_Objs[kEmptyStr];
    if( reports_collected.GetMap().size() <= 1 ) {
        // if there are no sequences or all sequences have the same
        // canonical protein id, then do not show any discrepancy
        return;
    }

    m_ReportItems = reports_collected.Export(*this)->GetSubitems();
}


// N_RUNS

DISCREPANCY_CASE(N_RUNS, SEQUENCE, eDisc | eSubmitter | eSmart | eBig | eFatal, "More than 10 Ns in a row")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.CanGetInst() && bioseq.GetInst().IsNa()) {
        const CSeqSummary& sum = context.GetSeqSummary();
        if (!sum.HasRef && sum.NRuns.size()) { // !context.SequenceHasFarPointers()
            string details;
            for (const auto& p: sum.NRuns) {
                details += (details.empty() ? " " : ", ") + to_string(p.first) + "-" + to_string(p.second);
            }
            m_Objs["[n] sequence[s] [has] runs of 10 or more Ns"][sum.Label + " has runs of Ns at the following locations: " + details].Ext().Fatal().Add(*context.BioseqObjRef());
        }
    }
}


DISCREPANCY_SUMMARIZE(N_RUNS)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// PERCENT_N

DISCREPANCY_CASE(PERCENT_N, SEQUENCE, eDisc | eSubmitter | eSmart | eBig, "More than 5 percent Ns")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.CanGetInst() && bioseq.GetInst().IsNa()) {
        const CSeqSummary& sum = context.GetSeqSummary();
        if (!sum.HasRef && sum.N * 100. / sum.Len > 5) { // !context.SequenceHasFarPointers()
            m_Objs["[n] sequence[s] [has] more than 5% Ns"].Add(*context.BioseqObjRef());
        }
    }
}


DISCREPANCY_SUMMARIZE(PERCENT_N)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// INTERNAL_TRANSCRIBED_SPACER_RRNA

static const char* kRRNASpacer[] = { "internal", "transcribed", "spacer" };
static const size_t kRRNASpacer_len = ArraySize(kRRNASpacer);

DISCREPANCY_CASE(INTERNAL_TRANSCRIBED_SPACER_RRNA, FEAT, eOncaller, "Look for rRNAs that contain either 'internal', 'transcribed' or 'spacer'")
{
    for (const CSeq_feat& feat : context.GetFeat()) {
        if (feat.IsSetData() && feat.GetData().IsRna() && feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_rRNA) {
            const string rna_name = feat.GetData().GetRna().GetRnaProductName();
            for (size_t i = 0; i < kRRNASpacer_len; ++i) {
                if (NStr::FindNoCase(rna_name, kRRNASpacer[i]) != NPOS) {
                    m_Objs["[n] rRNA feature products contain 'internal', 'transcribed', or 'spacer'"].Add(*context.SeqFeatObjRef(feat));
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(INTERNAL_TRANSCRIBED_SPACER_RRNA)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// OVERLAPPING_CDS

static bool StrandsMatch(ENa_strand strand1, ENa_strand strand2)
{
    return (strand1 == eNa_strand_minus && strand2 == eNa_strand_minus) || (strand1 != eNa_strand_minus && strand2 != eNa_strand_minus);
}


static const char* kSimilarProductWords[] = { "transposase", "integrase" };
static const size_t kNumSimilarProductWords = ArraySize(kSimilarProductWords);

static const char* kIgnoreSimilarProductWords[] = { "hypothetical protein", "phage", "predicted protein" };
static const size_t kNumIgnoreSimilarProductWords = ArraySize(kIgnoreSimilarProductWords);


static bool ProductNamesAreSimilar(const string& product1, const string& product2)
{
    bool str1_has_similarity_word = false, str2_has_similarity_word = false;

    size_t i;
    // if both product names contain one of the special case similarity words, the product names are similar.
  
    for (i = 0; i < kNumSimilarProductWords; i++) {
        if (NPOS != NStr::FindNoCase(product1, kSimilarProductWords[i])) {
            str1_has_similarity_word = true;
        }

        if (NPOS != NStr::FindNoCase(product2, kSimilarProductWords[i])) {
            str2_has_similarity_word = true;
        }
    }
    if (str1_has_similarity_word && str2_has_similarity_word) {
        return true;
    }
  
    // otherwise, if one of the product names contains one of special ignore similarity
    // words, the product names are not similar.

    for (i = 0; i < kNumIgnoreSimilarProductWords; i++) {
        if (NPOS != NStr::FindNoCase(product1, kIgnoreSimilarProductWords[i]) || NPOS != NStr::FindNoCase(product2, kIgnoreSimilarProductWords[i])) {
            return false;
        }
    }

    return !NStr::CompareNocase(product1, product2);
}


static bool ShouldIgnore(const string& product)
{
    if (NStr::Find(product, "transposon") != string::npos || NStr::Find(product, "transposase") != string::npos) {
        return true;
    }
    CString_constraint constraint;
    constraint.SetMatch_text("ABC");
    constraint.SetCase_sensitive(true);
    constraint.SetWhole_word(true);
    return constraint.Match(product);
}


static const string kOverlappingCDSNoteText = "overlaps another CDS with the same product name";


static bool HasOverlapNote(const CSeq_feat& feat)
{
    return feat.IsSetComment() && NStr::Find(feat.GetComment(), kOverlappingCDSNoteText) != string::npos;
}


static bool SetOverlapNote(CSeq_feat& feat)
{
    if (feat.IsSetComment() && NStr::Find(feat.GetComment(), kOverlappingCDSNoteText) != string::npos) {
        return false;
    }
    AddComment(feat, (string)kOverlappingCDSNoteText);
    return true;
}


static const char* kOverlap0 = "[n] coding region[s] overlap[S] another coding region with a similar or identical name.";
static const char* kOverlap1 = "[n] coding region[s] overlap[S] another coding region with a similar or identical name, but [has] the appropriate note text.";
static const char* kOverlap2 = "[n] coding region[s] overlap[S] another coding region with a similar or identical name and [does] not have the appropriate note text.";

static string GetProdName(const CSeq_feat* feat, map<const CSeq_feat*, string>& products, CDiscrepancyContext& context)
{
    if (products.find(feat) == products.end()) {
        string name = context.GetProdForFeature(*feat);
        products[feat] = name.empty() || ShouldIgnore(name) ? kEmptyStr : name;
    }
    return products[feat];
}


DISCREPANCY_CASE(OVERLAPPING_CDS, SEQUENCE, eDisc, "Overlapping CDs")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.IsSetInst() && bioseq.GetInst().IsNa()) {
        const auto& cds = context.FeatCDS();
        map<const CSeq_feat*, string> products;
        for (size_t i = 0; i < cds.size(); i++) {
            const CSeq_loc& loc_i = cds[i]->GetLocation();
            ENa_strand strand_i = loc_i.GetStrand();
            for (size_t j = i + 1; j < cds.size(); j++) {
                const CSeq_loc& loc_j = cds[j]->GetLocation();
                if (!StrandsMatch(strand_i, loc_j.GetStrand()) || context.Compare(loc_i, loc_j) == sequence::eNoOverlap) {
                    continue;
                }
                string prod_i = GetProdName(cds[i], products, context);
                if (prod_i.empty()) {
                    break;
                }
                string prod_j = GetProdName(cds[j], products, context);
                if (prod_j.empty() || !ProductNamesAreSimilar(prod_i, prod_j)) {
                    continue;
                }
                bool has_note = HasOverlapNote(*cds[i]);
                m_Objs[kOverlap0][has_note ? kOverlap1 : kOverlap2].Add(*context.SeqFeatObjRef(*cds[i], has_note ? CDiscrepancyContext::eFixNone : CDiscrepancyContext::eFixSelf));
                has_note = HasOverlapNote(*cds[j]);
                m_Objs[kOverlap0][has_note ? kOverlap1 : kOverlap2].Add(*context.SeqFeatObjRef(*cds[j], has_note ? CDiscrepancyContext::eFixNone : CDiscrepancyContext::eFixSelf));
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(OVERLAPPING_CDS)
{
    if (m_Objs.Exist(kOverlap0)) {
        m_Objs[kOverlap0].Promote();
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_AUTOFIX(OVERLAPPING_CDS)
{
    const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(context.FindObject(*obj));
    CRef<CSeq_feat> new_feat(new CSeq_feat());
    new_feat->Assign(*sf);
    if (SetOverlapNote(*new_feat)) {
        context.ReplaceSeq_feat(*obj, *sf, *new_feat);
        obj->SetFixed();
        return CRef<CAutofixReport>(new CAutofixReport("OVERLAPPING_CDS: Set note[s] for [n] coding region[s]", 1));
    }
    return CRef<CAutofixReport>();
}


DISCREPANCY_CASE(PARTIAL_CDS_COMPLETE_SEQUENCE, FEAT, eDisc | eOncaller | eSubmitter | eSmart, "Partial CDSs in Complete Sequences")
{
    for (const CSeq_feat& feat : context.GetFeat()) {
        if (feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_cdregion) {
            // leave if this CDS is not at least in some way marked as partial
            if (!GET_FIELD_OR_DEFAULT(feat, Partial, false) && !(feat.IsSetLocation() && feat.GetLocation().IsPartialStart(eExtreme_Biological)) && !(feat.IsSetLocation() && feat.GetLocation().IsPartialStop(eExtreme_Biological))) {
                continue;
            }
            // leave if we're not on a complete sequence
            auto mol_info = context.GetMolinfo();
            if (!mol_info || !FIELD_EQUALS(mol_info->GetMolinfo(), Completeness, CMolInfo::eCompleteness_complete)) {
                continue;
            }
            // record the issue
            m_Objs["[n] partial CDS[s] in complete sequence[s]"].Add(*context.SeqFeatObjRef(feat));
        }
    }
}


DISCREPANCY_SUMMARIZE(PARTIAL_CDS_COMPLETE_SEQUENCE)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_CASE(RNA_NO_PRODUCT, FEAT, eDisc | eOncaller | eSubmitter | eSmart, "Find RNAs without Products")
{
    for (const CSeq_feat& feat : context.GetFeat()) {
        if (feat.GetData().IsRna() && !context.IsPseudo(feat)) {
            // for the given RNA subtype, see whether a product is required
            switch (feat.GetData().GetSubtype()) {
                case CSeqFeatData::eSubtype_otherRNA: {
                    CTempString comment(feat.IsSetComment() ? feat.GetComment() : kEmptyStr);
                    if (NStr::StartsWith(comment, "contains ", NStr::eNocase) || NStr::StartsWith(comment, "may contain", NStr::eNocase)) {
                        continue;
                    }
                    break;
                }
                case CSeqFeatData::eSubtype_tmRNA:
                    // don't require products for tmRNA
                    continue;
                case CSeqFeatData::eSubtype_ncRNA: {
                    // if ncRNA has a class other than "other", don't need a product
                    const CRNA_ref & rna_ref = feat.GetData().GetRna();
                    if (!FIELD_IS_SET_AND_IS(rna_ref, Ext, Gen)) {
                        // no RNA-gen, so no class, so needs a product
                        break;
                    }
                    const CTempString gen_class(
                        GET_STRING_FLD_OR_BLANK(rna_ref.GetExt().GetGen(), Class));
                    if (!gen_class.empty() && !NStr::EqualNocase(gen_class, "other")) {
                        // product has a product other than "other", so no
                        // explicit product needed.
                        continue;
                    }
                    break;
                }
                default:
                    // other kinds always need a product
                    break;
            }
            const CRNA_ref & rna_ref = feat.GetData().GetRna();
            if (!rna_ref.IsSetExt()) {
                // will try other ways farther below
            }
            else {
                const CRNA_ref::TExt & rna_ext = rna_ref.GetExt();
                switch (rna_ext.Which()) {
                    case CRNA_ref::TExt::e_Name: {
                        const string & ext_name = rna_ref.GetExt().GetName();
                        if (!ext_name.empty() && !NStr::EqualNocase(ext_name, "ncRNA") && !NStr::EqualNocase(ext_name, "tmRNA") && !NStr::EqualNocase(ext_name, "misc_RNA")) {
                            // ext.name can considered a product
                            continue;
                        }
                        break;
                    }
                    case CRNA_ref::TExt::e_TRNA:
                    case CRNA_ref::TExt::e_Gen:
                        if (!rna_ref.GetRnaProductName().empty()) {
                            // found a product
                            continue;
                        }
                        break;
                    default:
                        _TROUBLE;
                        break;
                }
            }
            // try to get it from a "product" qual
            if (!feat.GetNamedQual("product").empty()) {
                // gb-qual can be considered a product
                continue;
            }
            // could not find a product
            m_Objs["[n] RNA feature[s] [has] no product and [is] not pseudo"].Add(*context.SeqFeatObjRef(feat), false);  // not unique
        }
    }
}


DISCREPANCY_SUMMARIZE(RNA_NO_PRODUCT)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// CONTAINED_CDS

static bool HasContainedNote(const CSeq_feat& feat)
{
    return feat.IsSetComment() && NStr::EqualNocase(feat.GetComment(), "completely contained in another CDS");
}


static const char* kContained = "[n] coding region[s] [is] completely contained in another coding region.";
static const char* kContainedNote = "[n] coding region[s] [is] completely contained in another coding region, but have note.";
static const char* kContainedSame = "[n] coding region[s] [is] completely contained in another coding region on the same strand.";
static const char* kContainedOpps = "[n] coding region[s] [is] completely contained in another coding region, but on the opposite strand.";


DISCREPANCY_CASE(CONTAINED_CDS, SEQUENCE, eDisc | eSubmitter | eSmart | eFatal, "Contained CDs")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.IsSetInst() && bioseq.GetInst().IsNa()) {
        const CSeqdesc* biosrc = context.GetBiosource();
        if (!context.IsEukaryotic(biosrc ? &biosrc->GetSource() : nullptr)) {
            const auto& cds = context.FeatCDS();
            for (size_t i = 0; i < cds.size(); i++) {
                const CSeq_loc& loc_i = cds[i]->GetLocation();
                ENa_strand strand_i = loc_i.GetStrand();
                for (size_t j = i + 1; j < cds.size(); j++) {
                    const CSeq_loc& loc_j = cds[j]->GetLocation();
                    sequence::ECompare compare = context.Compare(loc_j, loc_i);
                    if (compare == sequence::eContains || compare == sequence::eSame || compare == sequence::eContained) {
                        const char* strand = StrandsMatch(strand_i, loc_j.GetStrand()) ? kContainedSame : kContainedOpps;
                        bool has_note = HasContainedNote(*cds[i]);
                        new CSimpleTypeObject<string>(context.GetProdForFeature(*cds[i]));
                        bool autofix = compare == sequence::eContained && !has_note;
                        m_Objs[kContained][has_note ? kContainedNote : strand].Fatal().Add(*context.SeqFeatObjRef(*cds[i], autofix ? cds[i] : nullptr, autofix ? new CSimpleTypeObject<string>(context.GetProdForFeature(*cds[i])) : nullptr));
                        has_note = HasContainedNote(*cds[j]);
                        autofix = compare == sequence::eContains && !has_note;
                        m_Objs[kContained][has_note ? kContainedNote : strand].Fatal().Add(*context.SeqFeatObjRef(*cds[j], autofix ? cds[j] : nullptr, autofix ? new CSimpleTypeObject<string>(context.GetProdForFeature(*cds[j])) : nullptr));
                    }
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(CONTAINED_CDS)
{
    if (m_Objs.Exist(kContained) && m_Objs[kContained].GetMap().size() == 1) {
        m_ReportItems = m_Objs[kContained].Export(*this)->GetSubitems();
    }
    else {
        m_ReportItems = m_Objs.Export(*this)->GetSubitems();
    }
}


DISCREPANCY_AUTOFIX(CONTAINED_CDS)
{
    const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(context.FindObject(*obj));
    CRef<CSeq_feat> new_feat(new CSeq_feat());
    new_feat->Assign(*sf);
    new_feat->SetData().SetImp().SetKey("misc_feature");
    const CSimpleTypeObject<string>* stringobj = dynamic_cast<const CSimpleTypeObject<string>*>(context.GetMore(*obj));
    if (stringobj && !stringobj->Value.empty()) {
        AddComment(*new_feat, stringobj->Value);
    }
    context.ReplaceSeq_feat(*obj, *sf, *new_feat);
    obj->SetFixed();
    return CRef<CAutofixReport>(new CAutofixReport("CONTAINED_CDS: Converted [n] coding region[s] to misc_feat", 1));
}


DISCREPANCY_CASE(ZERO_BASECOUNT, SEQUENCE, eDisc | eOncaller | eSubmitter | eSmart | eBig, "Zero Base Counts")
{
    static const char* kMsg = "[n] sequence[s] [has] a zero basecount for a nucleotide";
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.CanGetInst() && bioseq.GetInst().IsNa()) {
        const CSeqSummary& sum = context.GetSeqSummary();
        if (!sum.HasRef) {
            if (!sum.A) {
                m_Objs[kMsg]["[n] sequence[s] [has] no As"].Ext().Add(*context.BioseqObjRef());
            }
            if (!sum.C) {
                m_Objs[kMsg]["[n] sequence[s] [has] no Cs"].Ext().Add(*context.BioseqObjRef());
            }
            if (!sum.G) {
                m_Objs[kMsg]["[n] sequence[s] [has] no Gs"].Ext().Add(*context.BioseqObjRef());
            }
            if (!sum.T) {
                m_Objs[kMsg]["[n] sequence[s] [has] no Ts"].Ext().Add(*context.BioseqObjRef());
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(ZERO_BASECOUNT)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// NONWGS_SETS_PRESENT

DISCREPANCY_CASE(NONWGS_SETS_PRESENT, SEQ_SET, eDisc, "Eco, mut, phy or pop sets present")
{
    const CBioseq_set& set = context.CurrentBioseq_set();
    if (set.IsSetClass()) {
        switch (set.GetClass()) {
            case CBioseq_set::eClass_eco_set:
            case CBioseq_set::eClass_mut_set:
            case CBioseq_set::eClass_phy_set:
            case CBioseq_set::eClass_pop_set:
                // non-WGS set found
                m_Objs["[n] set[s] [is] of type eco, mut, phy or pop"].Add(*context.BioseqSetObjRef(true));
                break;
            default:
                break;
        }
    }
}


DISCREPANCY_SUMMARIZE(NONWGS_SETS_PRESENT)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_AUTOFIX(NONWGS_SETS_PRESENT)
{
    const CBioseq_set* bss = dynamic_cast<const CBioseq_set*>(context.FindObject(*obj));
    CBioseq_set_Handle set_h = context.GetBioseq_setHandle(*bss);
    CBioseq_set_EditHandle set_eh(set_h);
    set_eh.SetClass(CBioseq_set::eClass_genbank);
    obj->SetFixed();
    return CRef<CAutofixReport>(new CAutofixReport("NONWGS_SETS_PRESENT: Set class to GenBank for [n] set[s]", 1));
}


//NO_ANNOTATION

DISCREPANCY_CASE(NO_ANNOTATION, SEQUENCE, eDisc | eOncaller | eSubmitter | eSmart | eBig, "No annotation")
{
    auto all_feat = context.GetAllFeat();
    if (all_feat.begin() == all_feat.end()) {
        m_Objs["[n] bioseq[s] [has] no features"].Add(*context.BioseqObjRef());
    }
}


DISCREPANCY_SUMMARIZE(NO_ANNOTATION)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_CASE(LONG_NO_ANNOTATION, SEQUENCE, eDisc | eOncaller | eSubmitter | eSmart | eBig, "No annotation for LONG sequence")
{
    const int kSeqLength = 5000;
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.IsNa() && bioseq.IsSetLength() && bioseq.GetLength() > kSeqLength) {
        auto all_feat = context.GetAllFeat();
        if (all_feat.begin() == all_feat.end()) {
            m_Objs["[n] bioseq[s] [is] longer than 5000nt and [has] no features"].Add(*context.BioseqObjRef());
        }
    }
}


DISCREPANCY_SUMMARIZE(LONG_NO_ANNOTATION)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// POSSIBLE_LINKER

DISCREPANCY_CASE(POSSIBLE_LINKER, SEQUENCE, eOncaller, "Detect linker sequence after poly-A tail")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.CanGetInst() && !bioseq.GetInst().IsAa() && bioseq.GetInst().GetLength() >= 30) {
        const CSeqdesc* molinfo = context.GetMolinfo();
        if (molinfo && molinfo->GetMolinfo().IsSetBiomol() && molinfo->GetMolinfo().GetBiomol() == CMolInfo::eBiomol_mRNA) {
            CSeqVector seq_vec(bioseq, &context.GetScope(), CBioseq_Handle::eCoding_Iupac, eNa_strand_plus);
            static const size_t TAIL = 30;
            string seq_data(kEmptyStr);
            seq_vec.GetSeqData(bioseq.GetInst().GetLength() - TAIL, bioseq.GetInst().GetLength(), seq_data);
            size_t tail_len = 0;
            size_t cut = 0;
            for (size_t i = 0; i < seq_data.length(); i++) {
                if (seq_data[i] == 'A' || seq_data[i] == 'a') {
                    tail_len++;
                }
                else {
                    if (tail_len > 20) {
                        cut = i;
                    }
                    tail_len = 0;
                }
            }
            if (cut) {
                cut = TAIL - cut;
                m_Objs["[n] bioseq[s] may have linker sequence after the poly-A tail"].Add(*context.BioseqObjRef(cut > 0 ? CDiscrepancyContext::eFixSelf : CDiscrepancyContext::eFixNone, cut ? new CSimpleTypeObject<size_t>(cut) : nullptr));
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(POSSIBLE_LINKER)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_AUTOFIX(POSSIBLE_LINKER)
{
    const CBioseq* seq = dynamic_cast<const CBioseq*>(context.FindObject(*obj));
    size_t cut_from_end = dynamic_cast<const CSimpleTypeObject<size_t>*>(obj->GetMoreInfo().GetPointer())->Value;
    CBioseq_EditHandle besh(context.GetBioseqHandle(*seq));
    SSeqMapSelector selector;
    selector.SetFlags(CSeqMap::fFindData);
    CSeqMap_I seqmap_i(besh, selector);
    size_t start = 0;
    size_t stop = besh.GetInst_Length() - cut_from_end;
    while (seqmap_i) {
        TSeqPos len = seqmap_i.GetLength();
        if (start < stop && start + len > stop) {
            string seq_in;
            seqmap_i.GetSequence(seq_in, CSeqUtil::e_Iupacna);
            string seq_out = seq_in.substr(0, stop - start);
            seqmap_i.SetSequence(seq_out, CSeqUtil::e_Iupacna, seqmap_i.GetData().Which());
            ++seqmap_i;
        }
        else if (start >= stop) {
            seqmap_i = seqmap_i.Remove();
        }
        else {
            ++seqmap_i;
        }
        start += len;
    }
    obj->SetFixed();
    return CRef<CAutofixReport>(new CAutofixReport("POSSIBLE_LINKER: [n] sequence[s] trimmed", 1));
}


// ORDERED_LOCATION

DISCREPANCY_CASE(ORDERED_LOCATION, FEAT, eDisc | eOncaller | eSmart, "Location is ordered (intervals interspersed with gaps)")
{
    for (const CSeq_feat& feat : context.GetFeat()) {
        if (feat.IsSetLocation()) {
            CSeq_loc_CI loc_ci(feat.GetLocation(), CSeq_loc_CI::eEmpty_Allow);
            for (; loc_ci; ++loc_ci) {
                if (loc_ci.GetEmbeddingSeq_loc().IsNull()) {
                    m_Objs["[n] feature[s] [has] ordered location[s]"].Add(*context.SeqFeatObjRef(feat, &feat));
                    break;
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(ORDERED_LOCATION)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_AUTOFIX(ORDERED_LOCATION)
{
    const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(context.FindObject(*obj));
    CSeq_loc_I new_loc_creator(*SerialClone(sf->GetLocation()));
    while (new_loc_creator) {
        if (new_loc_creator.GetEmbeddingSeq_loc().IsNull()) {
            new_loc_creator.Delete();
        }
        else {
            ++new_loc_creator;
        }
    }
    if (!new_loc_creator.HasChanges()) {
        return CRef<CAutofixReport>();
    }
    CRef<CSeq_loc> new_seq_feat_loc = new_loc_creator.MakeSeq_loc(CSeq_loc_I::eMake_PreserveType);
    CRef<CSeq_feat> new_feat(SerialClone(*sf));
    new_feat->SetLocation(*new_seq_feat_loc);
    context.ReplaceSeq_feat(*obj, *sf, *new_feat);
    obj->SetFixed();
    return CRef<CAutofixReport>(new CAutofixReport("ORDERED_LOCATION: [n] features with ordered locations fixed", 1));
}


// MISSING_LOCUS_TAGS

DISCREPANCY_CASE(MISSING_LOCUS_TAGS, SEQUENCE, eDisc | eSubmitter | eSmart | eFatal, "Missing locus tags")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.IsSetInst() && bioseq.GetInst().IsNa()) {
        for (const CSeq_feat* feat : context.FeatGenes()) {
            const CGene_ref& gene_ref = feat->GetData().GetGene();
            if (!gene_ref.IsSetPseudo() || !gene_ref.GetPseudo()) {
                if (!gene_ref.IsSetLocus_tag() || NStr::IsBlank(gene_ref.GetLocus_tag())) {
                    m_Objs["[n] gene[s] [has] no locus tag[s]."].Fatal().Add(*context.SeqFeatObjRef(*feat));
                }
                else if (!m_Objs.Exist(kEmptyStr)) {
                    m_Objs[kEmptyStr].Incr();
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(MISSING_LOCUS_TAGS)
{
    if (m_Objs.Exist(kEmptyStr)) {
        m_Objs.GetMap().erase(kEmptyStr);
        m_ReportItems = m_Objs.Export(*this)->GetSubitems();
    }
}


// NO_LOCUS_TAGS

DISCREPANCY_CASE(NO_LOCUS_TAGS, FEAT, eDisc | eSubmitter | eSmart | eFatal, "No locus tags at all")
{
    for (const CSeq_feat& feat : context.GetFeat()) {
        if (feat.IsSetData() && feat.GetData().IsGene()) {
            const CGene_ref& gene_ref = feat.GetData().GetGene();
            if (gene_ref.IsSetPseudo() && gene_ref.GetPseudo()) {
                continue;
            }
            if (!gene_ref.IsSetLocus_tag() || NStr::IsBlank(gene_ref.GetLocus_tag())) {
                m_Objs["None of [n] gene[s] has locus tag."].Fatal().Add(*context.SeqFeatObjRef(feat));
            }
            else if (!m_Objs.Exist(kEmptyStr)) {
                m_Objs[kEmptyStr].Incr();
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(NO_LOCUS_TAGS)
{
    if (!m_Objs.Exist(kEmptyStr)) {
        m_ReportItems = m_Objs.Export(*this)->GetSubitems();
    }
}


// INCONSISTENT_LOCUS_TAG_PREFIX

DISCREPANCY_CASE(INCONSISTENT_LOCUS_TAG_PREFIX, FEAT, eDisc | eSubmitter | eSmart, "Inconsistent locus tag prefix")
{
    for (const CSeq_feat& feat : context.GetFeat()) {
        if (feat.IsSetData() && feat.GetData().Which() == CSeqFeatData::e_Gene) {
            const CGene_ref& gene_ref = feat.GetData().GetGene();
            // Skip pseudo-genes
            if (gene_ref.IsSetPseudo() && gene_ref.GetPseudo() == true) {
                continue;
            }
            // Skip missing locus tags
            if (!gene_ref.IsSetLocus_tag()) {
                continue;
            }
            // Report on good locus tags - are they consistent?
            string locus_tag = gene_ref.GetLocus_tag();
            if (!locus_tag.empty() && !context.IsBadLocusTagFormat(locus_tag)) {
                // Store each unique prefix in a bin
                // If there is more than 1 bin, the prefixes are inconsistent
                string prefix;
                string tagvalue;
                NStr::SplitInTwo(locus_tag, "_", prefix, tagvalue);
                stringstream ss;
                ss << "[n] feature[s] [has] locus tag prefix [(]" << prefix << ".";
                m_Objs[ss.str()].Add(*context.SeqFeatObjRef(feat));
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(INCONSISTENT_LOCUS_TAG_PREFIX)
{
    // If there is more than 1 bin, the prefixes are inconsistent
    if (m_Objs.GetMap().size() > 1) {
        m_ReportItems = m_Objs.Export(*this)->GetSubitems();
    }
}


static const string kInconsistent_Moltype = "[n] sequences have inconsistent moltypes";

DISCREPANCY_CASE(INCONSISTENT_MOLTYPES, SEQUENCE, eDisc | eOncaller | eSmart, "Inconsistent molecule types")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.CanGetInst() && !bioseq.GetInst().IsAa()) {
        string moltype;
        const CSeqdesc* molinfo = context.GetMolinfo();
        if (molinfo && molinfo->GetMolinfo().IsSetBiomol()) {
            moltype = CMolInfo::GetBiomolName(molinfo->GetMolinfo().GetBiomol());
        }
        if (NStr::IsBlank(moltype)) {
            moltype = "genomic";
        }
        if (bioseq.IsSetInst() && bioseq.GetInst().IsSetMol()) {
            moltype += string(" ") + CSeq_inst::GetMoleculeClass(bioseq.GetInst().GetMol());
        }
        m_Objs[kInconsistent_Moltype].Add(*context.BioseqObjRef());
        m_Objs[kInconsistent_Moltype]["[n] sequence[s] [has] moltype " + moltype].Ext().Add(*context.BioseqObjRef());
    }
}


DISCREPANCY_SUMMARIZE(INCONSISTENT_MOLTYPES)
{
    // If there is more than 1 key, the moltypes are inconsistent
    if (m_Objs[kInconsistent_Moltype].GetMap().size() > 1) {
        m_ReportItems = m_Objs.Export(*this)->GetSubitems();
    }
}


// BAD_LOCUS_TAG_FORMAT

DISCREPANCY_CASE(BAD_LOCUS_TAG_FORMAT, SEQUENCE, eDisc | eSubmitter | eSmart, "Bad locus tag format")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.IsSetId()) {
        for (const auto& id : bioseq.GetId()) {
            switch (id->Which()) {
                case CSeq_id::e_Genbank:
                case CSeq_id::e_Embl:
                case CSeq_id::e_Pir:
                case CSeq_id::e_Swissprot:
                case CSeq_id::e_Other:
                case CSeq_id::e_Ddbj:
                case CSeq_id::e_Prf:
                    return;
                default:
                    break;
            }
        }
    }
    const auto& genes = context.FeatGenes();
    for (const CSeq_feat* gene : genes) {
        const CGene_ref& gene_ref = gene->GetData().GetGene();
        if (gene_ref.IsSetPseudo() && gene_ref.GetPseudo() == true) {
            continue;
        }
        if (!gene_ref.IsSetLocus_tag()) {
            continue;
        }
        string locus_tag = gene_ref.GetLocus_tag();
        if (!locus_tag.empty() && context.IsBadLocusTagFormat(locus_tag)) {
            m_Objs["[n] locus tag[s] [is] incorrectly formatted."].Fatal().Add(*context.SeqFeatObjRef(*gene));
        }
    }
}


DISCREPANCY_SUMMARIZE(BAD_LOCUS_TAG_FORMAT)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// SEGSETS_PRESENT

DISCREPANCY_CASE(SEGSETS_PRESENT, SEQ_SET, eDisc | eSmart | eFatal, "Segsets present")
{
    const CBioseq_set& set = context.CurrentBioseq_set();
    if (set.IsSetClass() && set.GetClass() == CBioseq_set::eClass_segset) {
        m_Objs["[n] segset[s] [is] present"].Add(*context.BioseqSetObjRef());
    }
}


DISCREPANCY_SUMMARIZE(SEGSETS_PRESENT)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// QUALITY_SCORES

DISCREPANCY_CASE(QUALITY_SCORES, SEQUENCE, eDisc | eSmart, "Check for quality scores")
{
    const CBioseq& bioseq = context.CurrentBioseq();
    if (bioseq.IsSetInst() && bioseq.IsNa()) {
        m_Objs["t"].Incr(); // total num
        if (bioseq.CanGetAnnot()) {
            for (const auto& ann : bioseq.GetAnnot()) {
                if (ann->IsGraph()) {
                    m_Objs["q"].Incr();
                    return;
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(QUALITY_SCORES)
{
    size_t q = m_Objs["q"].GetCount();
    size_t n = m_Objs["t"].GetCount() - q;
    if (q && n) {
        CReportNode ret;
        ret["Quality scores are missing on some(" + to_string(n) + ") sequences"];
        m_ReportItems = ret.Export(*this)->GetSubitems();
    }
}


// BACTERIA_SHOULD_NOT_HAVE_MRNA

DISCREPANCY_CASE(BACTERIA_SHOULD_NOT_HAVE_MRNA, FEAT, eDisc | eOncaller | eSubmitter | eSmart, "Bacterial sequences should not have mRNA features")
{
    const CSeqdesc* biosrc = context.GetBiosource();
    if (biosrc && context.IsBacterial(&biosrc->GetSource())) {
        for (const CSeq_feat& feat : context.GetFeat()) {
            if (feat.IsSetData() && feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA) {
                m_Objs["[n] bacterial sequence[s] [has] mRNA features"].Add(*context.SeqFeatObjRef(feat));
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(BACTERIA_SHOULD_NOT_HAVE_MRNA)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// BAD_BGPIPE_QUALS

static const string kDiscMessage = "[n] feature[s] contain[S] invalid BGPIPE qualifiers";

DISCREPANCY_CASE(BAD_BGPIPE_QUALS, SEQUENCE, eDisc | eSmart, "Bad BGPIPE qualifiers")
{
    if (!context.IsRefseq() && context.IsBGPipe()) {
        for (const CSeq_feat& feat : context.GetAllFeat()) {
            if (STRING_FIELD_NOT_EMPTY(feat, Except_text)) {
                if (feat.GetExcept_text() == "ribosomal slippage" && feat.IsSetComment() && feat.GetComment().find("programmed frameshift") != string::npos) {
                    continue;
                }
                m_Objs[kDiscMessage].Add(*context.SeqFeatObjRef(feat));
                continue;
            }
            if (FIELD_IS_SET_AND_IS(feat, Data, Cdregion)) {
                bool skip = false;
                const CCdregion & cdregion = feat.GetData().GetCdregion();
                if (RAW_FIELD_IS_EMPTY_OR_UNSET(cdregion, Code_break)) {
                    continue;
                }
                if (GET_STRING_FLD_OR_BLANK(feat, Comment) == "ambiguity in stop codon") {
                    // check if any code break is a stop codon
                    FOR_EACH_CODEBREAK_ON_CDREGION(code_break_it, cdregion) {
                        const CCode_break & code_break = **code_break_it;
                        if (FIELD_IS_SET_AND_IS(code_break, Aa, Ncbieaa) && code_break.GetAa().GetNcbieaa() == 42 /* *:Stop codon */) {
                            skip = true;
                            break;
                        }
                    }
                    if (!skip) {
                        m_Objs[kDiscMessage].Add(*context.SeqFeatObjRef(feat));
                    }
                    continue;
                }
                FOR_EACH_CODEBREAK_ON_CDREGION(code_break_it, cdregion) {
                    const CCode_break & code_break = **code_break_it;
                    if (FIELD_IS_SET_AND_IS(code_break, Aa, Ncbieaa) && code_break.GetAa().GetNcbieaa() == 85 /* U:Sec */) {
                        skip = true;
                        break;
                    }
                }
                if (!skip) {
                    m_Objs[kDiscMessage].Add(*context.SeqFeatObjRef(feat));
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(BAD_BGPIPE_QUALS)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// GENE_PRODUCT_CONFLICT

DISCREPANCY_CASE(GENE_PRODUCT_CONFLICT, SEQUENCE, eDisc | eSubmitter | eSmart, "Gene Product Conflict")
{
    for (const CSeq_feat& feat : context.GetAllFeat()) {
        if (feat.IsSetData() && feat.GetData().IsCdregion()) {
            CConstRef<CSeq_feat> gene_feat(context.GetGeneForFeature(feat));
            if (gene_feat && gene_feat->IsSetData() && gene_feat->GetData().IsGene()) {
                const CGene_ref& gene = gene_feat->GetData().GetGene();
                if (gene.IsSetLocus()) {
                    TGeneLocusMap& genes = context.GetGeneLocusMap();
                    const string& locus = gene.GetLocus();
                    string product = context.GetProdForFeature(feat);
                    genes[locus].push_back(make_pair(context.SeqFeatObjRef(feat), product));
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(GENE_PRODUCT_CONFLICT)
{
    TGeneLocusMap& genes = context.GetGeneLocusMap();
    for (auto& gene : genes) {
        if (gene.second.size() > 1) {
            TGenesList::const_iterator cur_gene = gene.second.cbegin();
            const string& product = cur_gene->second;
            bool diff = false;
            for (++cur_gene; cur_gene != gene.second.cend(); ++cur_gene) {
                const string& cur_product = cur_gene->second;
                if (product != cur_product) {
                    diff = true;
                    break;
                }
            }
            if (diff) {
                string sub = "[n] coding regions have the same gene name (" + gene.first + ") as another coding region but a different product";
                for (auto& cur_gene : gene.second) {
                    m_Objs["[n] coding region[s] [has] the same gene name as another coding region but a different product"][sub].Ext().Add(*cur_gene.first, false);
                }
            }
        }
    }
    genes.clear();
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
