/* $Id$
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
 * Author:  Robert Smith
 *
 * File Description:
 *   Basic Cleanup of CSeq_entries.
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <serial/serialbase.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_annot.hpp>
// included for GetPubdescLabels and GetCitationList
#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/biblio/Author.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/general/Person_id.hpp>
#include <objects/general/Name_std.hpp>
#include <objects/misc/sequence_macros.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqset/seqset_macros.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/taxon3/taxon3.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/autodef.hpp>
#include <objmgr/seq_annot_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seq_vector_ci.hpp>
#include <objtools/edit/cds_fix.hpp>
#include <objtools/cleanup/cleanup.hpp>
#include "cleanup_utils.hpp"
#include <objtools/cleanup/cleanup_message.hpp>

#include <util/strsearch.hpp>

#include "newcleanupp.hpp"

#include <objtools/logging/listener.hpp>

#include <objtools/cleanup/influenza_set.hpp>
#include <utility>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

enum EChangeType {
    eChange_UNKNOWN
};

// *********************** CCleanup implementation **********************


CCleanup::CCleanup(CScope* scope, EScopeOptions scope_handling)
{
    if (scope && scope_handling == eScope_UseInPlace) {
        m_Scope = scope;
    }
    else {
        m_Scope = new CScope(*(CObjectManager::GetInstance()));
        if (scope) {
            m_Scope->AddScope(*scope);
        }
    }
}


CCleanup::~CCleanup(void)
{
}


void CCleanup::SetScope(CScope* scope)
{
    m_Scope.Reset(new CScope(*(CObjectManager::GetInstance())));
    if (scope) {
        m_Scope->AddScope(*scope);
    }
}


static
CRef<CCleanupChange> makeCleanupChange(Uint4 options)
{
    CRef<CCleanupChange> changes;
    if (! (options  &  CCleanup::eClean_NoReporting)) {
        changes.Reset(new CCleanupChange);
    }
    return changes;
}

#define CLEANUP_SETUP \
    auto changes = makeCleanupChange(options); \
    CNewCleanup_imp clean_i(changes, options); \
    clean_i.SetScope(*m_Scope);

CCleanup::TChanges CCleanup::BasicCleanup(CSeq_entry& se, Uint4 options)
{
    CLEANUP_SETUP
    clean_i.BasicCleanupSeqEntry(se);
    return changes;
}


CCleanup::TChanges CCleanup::BasicCleanup(CSeq_submit& ss, Uint4 options)
{
    CLEANUP_SETUP
    clean_i.BasicCleanupSeqSubmit(ss);
    return changes;
}


CCleanup::TChanges CCleanup::BasicCleanup(CSubmit_block& block, Uint4 options)
{
    CLEANUP_SETUP
    clean_i.BasicCleanupSubmitblock(block);
    return changes;
}


CCleanup::TChanges CCleanup::BasicCleanup(CBioseq_set& bss, Uint4 options)
{
    CLEANUP_SETUP
    clean_i.BasicCleanupBioseqSet(bss);
    return changes;
}


CCleanup::TChanges CCleanup::BasicCleanup(CSeq_annot& sa, Uint4 options)
{
    CLEANUP_SETUP
    clean_i.BasicCleanupSeqAnnot(sa);
    return changes;
}


CCleanup::TChanges CCleanup::BasicCleanup(CSeq_feat& sf, Uint4 options)
{
    CLEANUP_SETUP
    clean_i.BasicCleanupSeqFeat(sf);
    return changes;
}


CCleanup::TChanges CCleanup::BasicCleanup(CBioSource& src, Uint4 options)
{
    CLEANUP_SETUP
    clean_i.BasicCleanupBioSource(src);
    return changes;
}


CCleanup::TChanges CCleanup::BasicCleanup(CSeq_entry_Handle& seh, Uint4 options)
{
    auto changes = makeCleanupChange(options);
    CNewCleanup_imp clean_i(changes, options);
    clean_i.SetScope(seh.GetScope());
    clean_i.BasicCleanupSeqEntryHandle(seh);
    return changes;
}


CCleanup::TChanges CCleanup::BasicCleanup(CBioseq_Handle& bsh,    Uint4 options)
{
    auto changes = makeCleanupChange(options);
    CNewCleanup_imp clean_i(changes, options);
    clean_i.SetScope(bsh.GetScope());
    clean_i.BasicCleanupBioseqHandle(bsh);
    return changes;
}


CCleanup::TChanges CCleanup::BasicCleanup(CBioseq_set_Handle& bssh, Uint4 options)
{
    auto changes = makeCleanupChange(options);
    CNewCleanup_imp clean_i(changes, options);
    clean_i.SetScope(bssh.GetScope());
    clean_i.BasicCleanupBioseqSetHandle(bssh);
    return changes;
}


CCleanup::TChanges CCleanup::BasicCleanup(CSeq_annot_Handle& sah, Uint4 options)
{
    auto changes = makeCleanupChange(options);
    CNewCleanup_imp clean_i(changes, options);
    clean_i.SetScope(sah.GetScope());
    clean_i.BasicCleanupSeqAnnotHandle(sah);
    return changes;
}


CCleanup::TChanges CCleanup::BasicCleanup(CSeq_feat_Handle& sfh,  Uint4 options)
{
    auto changes = makeCleanupChange(options);
    CNewCleanup_imp clean_i(changes, options);
    clean_i.SetScope(sfh.GetScope());
    clean_i.BasicCleanupSeqFeatHandle(sfh);
    return changes;
}


CCleanup::TChanges CCleanup::BasicCleanup(CSeqdesc& desc, Uint4 options)
{
    CLEANUP_SETUP
    clean_i.BasicCleanup(desc);
    return changes;

}


CCleanup::TChanges CCleanup::BasicCleanup(CSeq_descr & desc, Uint4 options)
{
    CLEANUP_SETUP

    for (auto& it : desc.Set()) {
        clean_i.BasicCleanup(*it);
    }
    return changes;
}


// *********************** Extended Cleanup implementation ********************
CCleanup::TChanges CCleanup::ExtendedCleanup(CSeq_entry& se,  Uint4 options)
{
    CLEANUP_SETUP
    clean_i.ExtendedCleanupSeqEntry(se);

    return changes;
}


CCleanup::TChanges CCleanup::ExtendedCleanup(CSeq_submit& ss,  Uint4 options)
{
    CLEANUP_SETUP
    clean_i.ExtendedCleanupSeqSubmit(ss);
    return changes;
}


CCleanup::TChanges CCleanup::ExtendedCleanup(CSeq_annot& sa,  Uint4 options)
{
    CLEANUP_SETUP
    clean_i.ExtendedCleanupSeqAnnot(sa); // (m_Scope->GetSeq_annotHandle(sa));
    return changes;
}

CCleanup::TChanges CCleanup::ExtendedCleanup(CSeq_entry_Handle& seh,  Uint4 options)
{
    auto changes = makeCleanupChange(options);
    CNewCleanup_imp clean_i(changes, options);
    clean_i.ExtendedCleanupSeqEntryHandle(seh); // (m_Scope->GetSeq_annotHandle(sa));
    return changes;
}


// *********************** CCleanupChange implementation **********************


vector<CCleanupChangeCore::EChanges> CCleanupChangeCore::GetAllChanges() const
{
    return m_Changes;
}

vector<string_view> CCleanupChangeCore::GetDescriptions() const
{
    vector<string_view> result;
    result.reserve(m_Changes.size());
    for (auto it : m_Changes) {
        result.push_back(GetDescription(it));
    }
    return result;
}

// corresponds to the values in CCleanupChange::EChanges.
// They must be edited together.
static constexpr std::array<string_view, CCleanupChangeCore::eNumberofChangeTypes> sm_ChangeDesc = {
    "Invalid Change Code",
    // set when strings are changed.
    "Trim Spaces",
    "Clean Double Quotes",
    "Append To String",
    // set when lists are sorted or uniqued.
    "Clean Qualifiers List",
    "Clean Dbxrefs List",
    "Clean CitonFeat List",
    "Clean Keywords List",
    "Clean Subsource List",
    "Clean Orgmod List",
    // Set when fields are moved or have content changes
    "Repair BioseqMol", //10
    "Change Feature Key",
    "Normalize Authors",
    "Change Publication",
    "Change Qualifiers",
    "Change Dbxrefs",
    "Change Keywords",
    "Change Subsource",
    "Change Orgmod",
    "Change Exception",
    "Change Comment", //20
    // Set when fields are rescued
    "Change tRna",
    "Change rRna",
    "Change ITS",
    "Change Anticodon",
    "Change Code Break",
    "Change Genetic Code",
    "Copy GeneXref",
    "Copy ProtXref",
    // set when locations are repaired
    "Change Seqloc",
    "Change Strand", //30
    "Change WholeLocation",
    // set when MolInfo descriptors are affected
    "Change MolInfo Descriptor",
    // set when prot-xref is removed
    "Remove ProtXref",
    // set when gene-xref is removed
    "Remove GeneXref",
    // set when protein feature is added
    "Add Protein Feature",
    // set when feature is removed
    "Remove Feature",
    // set when feature is moved
    "Move Feature",
    // set when qualifier is removed
    "Remove Qualifier",
    // set when Gene Xref is created
    "Add GeneXref",
    // set when descriptor is removed
    "Remove Descriptor", //40
    "Remove Keyword",
    "Add Descriptor",
    "Move Descriptor",
    "Convert Feature to Descriptor",
    "Collapse Set",
    "Change Feature Location",
    "Remove Annotation",
    "Convert Feature",
    "Remove Comment",
    "Add BioSource OrgMod", //50
    "Add BioSource SubSource",
    "Change BioSource Genome",
    "Change BioSource Origin",
    "Change BioSource Other",
    "Change SeqId",
    "Remove Empty Publication",
    "Add Qualifier",
    "Cleanup Date",
    "Change BioseqInst",
    "Remove SeqID", // 60
    "Add ProtXref",
    "Change Partial",
    "Change Prot Names",
    "Change Prot Activities",
    "Change Site",
    "Change PCR Primers",
    "Change RNA-ref",
    "Move To Prot Xref",
    "Compress Spaces",
    "Strip serial", // 70
    "Remove Orgmod",
    "Remove SubSource",
    "Create Gene Nomenclature",
    "Clean Seq-feat xref",
    "Clean User-Object Or -Field",
    "Letter Case Change",
    "Change Bioseq-set Class",
    "Unique Without Sort",
    "Add RNA-ref",
    "Change Gene-ref", // 80
    "Clean Dbtag",
    "Change Biomol",
    "Change Cdregion",
    "Clean EC Number",
    "Remove Exception",
    "Add NcbiCleanupObject",
    "Clean Delta-ext",
    "Trim Flanking Quotes",
    "Clean Bioseq Title",
    "Decode XML", // 90
    "Remove Dup BioSource",
    "Clean Org-ref",
    "Trim Internal Semicolons",
    "Add SeqFeatXref",
    "Convert Unstructured Org-ref Modifier",
    "Change taxname",
    "Move GO term to GeneOntology object",

    // set when any other change is made.
    "Change Other",
};

string_view CCleanupChangeCore::GetDescription(EChanges e)
{
    if (e <= eNoChange  ||  e >= eNumberofChangeTypes) {
        return sm_ChangeDesc[eNoChange]; // this is "Invalid Change Code"
    }
    return sm_ChangeDesc[e];
}

CProt_ref::EProcessed s_ProcessedFromKey(const string& key)
{
    if (NStr::Equal(key, "sig_peptide")) {
        return CProt_ref::eProcessed_signal_peptide;
    } else if (NStr::Equal(key, "mat_peptide")) {
        return CProt_ref::eProcessed_mature;
    } else if (NStr::Equal(key, "transit_peptide")) {
        return CProt_ref::eProcessed_transit_peptide;
    } else if (NStr::Equal(key, "preprotein") || NStr::Equal(key, "proprotein")) {
        return CProt_ref::eProcessed_preprotein;
    } else if (NStr::Equal(key, "propeptide")) {
        return CProt_ref::eProcessed_propeptide;
    } else {
        return CProt_ref::eProcessed_not_set;
    }
}

string s_KeyFromProcessed(CProt_ref::EProcessed processed)
{
    switch (processed) {
    case CProt_ref::eProcessed_mature:
        return "mat_peptide";
        break;
    case CProt_ref::eProcessed_preprotein:
        return "preprotein";
        break;
    case CProt_ref::eProcessed_signal_peptide:
        return "sig_peptide";
        break;
    case CProt_ref::eProcessed_transit_peptide:
        return "transit_peptide";
        break;
    case CProt_ref::eProcessed_propeptide:
        return "propeptide";
        break;
    case CProt_ref::eProcessed_not_set:
        return kEmptyStr;
        break;
    }
    return kEmptyStr;
}


bool ConvertProteinToImp(CSeq_feat_Handle fh)
{
    if (fh.GetData().IsProt() && fh.GetData().GetProt().IsSetProcessed()) {
        string key = s_KeyFromProcessed(fh.GetData().GetProt().GetProcessed());
        if (!NStr::IsBlank(key)) {
            CRef<CSeq_feat> new_feat(new CSeq_feat());
            new_feat->Assign(*(fh.GetSeq_feat()));
            if (fh.GetData().GetProt().IsSetName() && !fh.GetData().GetProt().GetName().empty()) {
                CRef<CGb_qual> q(new CGb_qual());
                q->SetQual("product");
                q->SetVal(fh.GetData().GetProt().GetName().front());
                new_feat->SetQual().push_back(q);
            }
            new_feat->SetData().SetImp().SetKey(key);
            CSeq_feat_EditHandle efh(fh);
            efh.Replace(*new_feat);
            return true;
        }
    }
    return false;
}


bool s_IsPreprotein(CSeq_feat_Handle fh)
{
    if (!fh.IsSetData()) {
        return false;
    } else if (fh.GetData().IsProt() &&
        fh.GetData().GetProt().IsSetProcessed() &&
        fh.GetData().GetProt().GetProcessed() == CProt_ref::eProcessed_preprotein) {
        return true;
    } else if (fh.GetData().IsImp() &&
        fh.GetData().GetImp().IsSetKey() &&
        s_ProcessedFromKey(fh.GetData().GetImp().GetKey()) == CProt_ref::eProcessed_preprotein) {
        return true;
    } else {
        return false;
    }
}


void RescueProtProductQual(CSeq_feat& feat)
{
    if (!feat.IsSetQual() ||
        !feat.IsSetData() ||
        !feat.GetData().IsProt() ||
        feat.GetData().GetProt().IsSetName()) {
        return;
    }
    CSeq_feat::TQual::iterator it = feat.SetQual().begin();
    while (it != feat.SetQual().end()) {
        if ((*it)->IsSetQual() &&
            NStr::Equal((*it)->GetQual(), "product")) {
            if ((*it)->IsSetVal() && !NStr::IsBlank((*it)->GetVal())) {
                feat.SetData().SetProt().SetName().push_back((*it)->GetVal());
            }
            it = feat.SetQual().erase(it);
        } else {
            ++it;
        }
    }

    if (feat.SetQual().empty()) {
        feat.ResetQual();
    }
}


static CConstRef<CSeq_feat> s_GetCdsByProduct(CScope& scope, const CSeq_loc& product)
{
    const bool feat_by_product = true;
    SAnnotSelector sel(CSeqFeatData::e_Cdregion, feat_by_product);
    CFeat_CI fi(scope, product, sel);
    if (fi) {
        return ConstRef(&(fi->GetOriginalFeature()));
    }
    return CConstRef<CSeq_feat>();
};

static CConstRef<CSeq_feat> s_GetCdsByLocation(CScope& scope, const CSeq_loc& feat_loc)
{
    sequence::TFeatScores cdsScores;
    sequence::GetOverlappingFeatures(
            feat_loc,
            CSeqFeatData::e_Cdregion,
            CSeqFeatData::eSubtype_cdregion,
            sequence::eOverlap_Contained,
            cdsScores,
            scope);

    if (cdsScores.empty()) {
        return CConstRef<CSeq_feat>();
    }

    if (!feat_loc.IsPartialStart(eExtreme_Biological)) {
        for (auto cdsScore : cdsScores) {
            if (feature::IsLocationInFrame(scope.GetSeq_featHandle(*cdsScore.second), feat_loc)
                    == feature::eLocationInFrame_InFrame) {
                return cdsScore.second;
            }
        }
    }

    return cdsScores.front().second;
}



bool CCleanup::MoveFeatToProtein(CSeq_feat_Handle fh)
{
    CProt_ref::EProcessed processed = CProt_ref::eProcessed_not_set;
    if (fh.GetData().IsImp()) {
        if (!fh.GetData().GetImp().IsSetKey()) {
            return false;
        }
        processed = s_ProcessedFromKey(fh.GetData().GetImp().GetKey());
        if (processed == CProt_ref::eProcessed_not_set || processed == CProt_ref::eProcessed_preprotein) {
            return false;
        }
    } else if (s_IsPreprotein(fh)) {
        return ConvertProteinToImp(fh);
    }

    CBioseq_Handle parent_bsh = fh.GetScope().GetBioseqHandle(fh.GetLocation());

    if (!parent_bsh) {
        // feature is mispackaged
        return false;
    }
    if (parent_bsh.IsAa()) {
        // feature is already on protein sequence
        return false;
    }

    CConstRef<CSeq_feat> cds;
    bool matched_by_product = false;

    if (fh.IsSetProduct() &&
        fh.GetData().IsProt() &&
        fh.GetData().GetProt().IsSetProcessed() &&
        fh.GetData().GetProt().GetProcessed() == CProt_ref::eProcessed_mature) {
        cds = s_GetCdsByProduct(fh.GetScope(), fh.GetProduct());
        if (cds) {
            matched_by_product = true;
        }
    }
    if (!matched_by_product) {
        cds = s_GetCdsByLocation(fh.GetScope(), fh.GetLocation());
    }
    if (!cds || !cds->IsSetProduct()) {
        // there is no overlapping coding region feature, so there is no appropriate
        // protein sequence to move to
        return ConvertProteinToImp(fh);
    }

    bool require_frame = false;
    if (!require_frame) {
        ITERATE(CBioseq::TId, id_it, parent_bsh.GetBioseqCore()->GetId()) {
            if ((*id_it)->IsEmbl() || (*id_it)->IsDdbj()) {
                require_frame = true;
                break;
            }
        }
    }

    CRef<CSeq_loc> prot_loc = GetProteinLocationFromNucleotideLocation(fh.GetLocation(), *cds, fh.GetScope(), require_frame);

    if (!prot_loc) {
        return false;
    }

    CConstRef<CSeq_feat> orig_feat = fh.GetSeq_feat();
    CRef<CSeq_feat> new_feat(new CSeq_feat());
    new_feat->Assign(*orig_feat);
    if (new_feat->GetData().Which() == CSeqFeatData::e_Imp) {
        new_feat->SetData().SetProt().SetProcessed(processed);
        // if possible, rescue product qual
        RescueProtProductQual(*new_feat);
        if (processed == CProt_ref::eProcessed_mature &&
            !new_feat->GetData().GetProt().IsSetName()) {
            if (orig_feat->IsSetComment() && !NStr::IsBlank(orig_feat->GetComment())) {
                new_feat->SetData().SetProt().SetName().push_back(orig_feat->GetComment());
                new_feat->ResetComment();
            } else {
                new_feat->SetData().SetProt().SetName().push_back("unnamed");
            }
        }
    }

    // change location to protein
    new_feat->ResetLocation();
    new_feat->SetLocation(*prot_loc);
    SetFeaturePartial(*new_feat);
    if (matched_by_product) {
        new_feat->ResetProduct();
    }

    CSeq_feat_EditHandle edh(fh);
    edh.Replace(*new_feat);
    auto changes= makeCleanupChange(0);
    CNewCleanup_imp clean_i(changes, 0);
    clean_i.SetScope(fh.GetScope());
    clean_i.BasicCleanupSeqFeat(*new_feat);

    CSeq_annot_Handle ah = fh.GetAnnot();

    CBioseq_Handle target_bsh = fh.GetScope().GetBioseqHandle(new_feat->GetLocation());
    if (!target_bsh) {
        return false;
    }

    CBioseq_EditHandle eh = target_bsh.GetEditHandle();

    // Find a feature table on the protein sequence to add the feature to.
    CSeq_annot_Handle ftable;
    if (target_bsh.GetCompleteBioseq()->IsSetAnnot()) {
        ITERATE(CBioseq::TAnnot, annot_it, target_bsh.GetCompleteBioseq()->GetAnnot()) {
            if ((*annot_it)->IsFtable()) {
                ftable = fh.GetScope().GetSeq_annotHandle(**annot_it);
            }
        }
    }

    // If there is no feature table present, make one
    if (!ftable) {
        CRef<CSeq_annot> new_annot(new CSeq_annot());
        ftable = eh.AttachAnnot(*new_annot);
    }

    // add feature to the protein bioseq
    CSeq_annot_EditHandle aeh(ftable);
    aeh.TakeFeat(edh);

    // remove old annot if now empty
    if (CNewCleanup_imp::ShouldRemoveAnnot(*(ah.GetCompleteSeq_annot()))) {
        CSeq_annot_EditHandle orig(ah);
        orig.Remove();
    }

    return true;
}


bool CCleanup::MoveProteinSpecificFeats(CSeq_entry_Handle seh)
{
    bool any_change = false;
    CBioseq_CI bi(seh, CSeq_inst::eMol_na);
    while (bi) {
        SAnnotSelector sel(CSeqFeatData::e_Prot);
        sel.IncludeFeatType(CSeqFeatData::e_Psec_str);
        sel.IncludeFeatType(CSeqFeatData::e_Bond);
        for (CFeat_CI prot_it(*bi, sel); prot_it; ++prot_it) {
            any_change |= MoveFeatToProtein(*prot_it);
        }
        for (CFeat_CI imp_it(*bi, CSeqFeatData::e_Imp); imp_it; ++imp_it) {
            any_change |= MoveFeatToProtein(*imp_it);
        }
        ++bi;
    }
    return any_change;
}


bool CCleanup::IsGeneXrefUnnecessary(const CSeq_feat& sf, CScope& scope, const CGene_ref& gene_xref)
{
    if (gene_xref.IsSuppressed()) {
        return false;
    }

    CConstRef<CSeq_feat> gene = sequence::GetOverlappingGene(sf.GetLocation(), scope);
    if (!gene || !gene->IsSetData() || !gene->GetData().IsGene()) {
        return false;
    }

    if (!gene->GetData().GetGene().RefersToSameGene(gene_xref)) {
        return false;
    }

    // see if other gene might also match
    sequence::TFeatScores scores;
    sequence::GetOverlappingFeatures(sf.GetLocation(), CSeqFeatData::e_Gene, CSeqFeatData::eSubtype_gene,
        sequence::eOverlap_Contained, scores, scope);
    if (scores.size() == 1) {
        return true;
    } else if (scores.size() == 0) {
        return false;
    }

    ITERATE(sequence::TFeatScores, g, scores) {
        if (g->second.GetPointer() != gene.GetPointer() &&
            sequence::Compare(g->second->GetLocation(), gene->GetLocation(), &scope, sequence::fCompareOverlapping) == sequence::eSame) {
            return false;
        }
    }
    return true;
}


bool CCleanup::RemoveUnnecessaryGeneXrefs(CSeq_feat& f, CScope& scope)
{
    if (!f.IsSetXref()) {
        return false;
    }
    bool any_removed = false;
    CSeq_feat::TXref::iterator xit = f.SetXref().begin();
    while (xit != f.SetXref().end()) {
        if ((*xit)->IsSetData() && (*xit)->GetData().IsGene() &&
            IsGeneXrefUnnecessary(f, scope, (*xit)->GetData().GetGene())) {
            xit = f.SetXref().erase(xit);
            any_removed = true;
        } else {
            ++xit;
        }
    }
    if (any_removed) {
        if (f.IsSetXref() && f.GetXref().empty()) {
            f.ResetXref();
        }
    }
    return any_removed;
}


bool CCleanup::RemoveUnnecessaryGeneXrefs(CSeq_entry_Handle seh)
{
    bool any_change = false;
    CScope& scope = seh.GetScope();

    for (CFeat_CI fi(seh); fi; ++fi) {
        if (fi->IsSetXref()) {
            CRef<CSeq_feat> new_feat(new CSeq_feat());
            new_feat->Assign(*(fi->GetOriginalSeq_feat()));
            bool any_removed = RemoveUnnecessaryGeneXrefs(*new_feat, scope);
            if (any_removed) {
                CSeq_feat_EditHandle edh(*fi);
                edh.Replace(*new_feat);
                any_change = true;
            }
        }
    }

    return any_change;
}


//LCOV_EXCL_START
//not used by asn_cleanup but used by other applications
bool CCleanup::RemoveNonsuppressingGeneXrefs(CSeq_feat& f)
{
    if (!f.IsSetXref()) {
        return false;
    }
    bool any_removed = false;
    CSeq_feat::TXref::iterator xit = f.SetXref().begin();
    while (xit != f.SetXref().end()) {
        if ((*xit)->IsSetData() && (*xit)->GetData().IsGene() &&
            !(*xit)->GetData().GetGene().IsSuppressed()) {
            xit = f.SetXref().erase(xit);
            any_removed = true;
        } else {
            ++xit;
        }
    }
    if (any_removed) {
        if (f.IsSetXref() && f.GetXref().empty()) {
            f.ResetXref();
        }
    }
    return any_removed;
}
//LCOV_EXCL_STOP


bool CCleanup::RepairXrefs(const CSeq_feat& src, CSeq_feat_Handle& dst, const CTSE_Handle& tse)
{
    if (!src.IsSetId() || !src.GetId().IsLocal()) {
        // can't create xref if no ID
        return false;
    }
    if (!CSeqFeatData::AllowXref(src.GetData().GetSubtype(), dst.GetData().GetSubtype())) {
        // only create reciprocal xrefs if permitted
        return false;
    }
    // don't create xref if already have xref or if dst not gene and already has
    // xref to feature of same type as src
    bool has_xref = false;
    if (dst.IsSetXref()) {
        ITERATE(CSeq_feat::TXref, xit, dst.GetXref()) {
            if ((*xit)->IsSetId() && (*xit)->GetId().IsLocal()) {
                if ((*xit)->GetId().Equals(src.GetId())) {
                    // already have xref
                    has_xref = true;
                    break;
                } else if (!dst.GetData().IsGene()) {
                    const CTSE_Handle::TFeatureId& feat_id = (*xit)->GetId().GetLocal();
                    CTSE_Handle::TSeq_feat_Handles far_feats = tse.GetFeaturesWithId(CSeqFeatData::e_not_set, feat_id);
                    ITERATE(CTSE_Handle::TSeq_feat_Handles, fit, far_feats) {
                        if (fit->GetData().GetSubtype() == src.GetData().GetSubtype()) {
                            has_xref = true;
                            break;
                        }
                    }
                    if (has_xref) {
                        break;
                    }
                }
            }
        }
    }
    bool rval = false;
    if (!has_xref) {
        // to put into "editing mode"
        dst.GetAnnot().GetEditHandle();
        CSeq_feat_EditHandle eh(dst);
        CRef<CSeq_feat> cpy(new CSeq_feat());
        cpy->Assign(*(dst.GetSeq_feat()));
        cpy->AddSeqFeatXref(src.GetId());
        eh.Replace(*cpy);
        rval = true;
    }
    return rval;
}


bool CCleanup::RepairXrefs(const CSeq_feat& f, const CTSE_Handle& tse)
{
    bool rval = false;

    if (!f.IsSetId() || !f.IsSetXref()) {
        return rval;
    }

    ITERATE(CSeq_feat::TXref, xit, f.GetXref()) {
        if ((*xit)->IsSetId() && (*xit)->GetId().IsLocal()) {
            const CTSE_Handle::TFeatureId& x_id = (*xit)->GetId().GetLocal();
            CTSE_Handle::TSeq_feat_Handles far_feats = tse.GetFeaturesWithId(CSeqFeatData::e_not_set, x_id);
            if (far_feats.size() == 1) {
                rval |= RepairXrefs(f, far_feats[0], tse);
            }
        }
    }
    return rval;
}


bool CCleanup::RepairXrefs(CSeq_entry_Handle seh)
{
    bool rval = false;
    const CTSE_Handle& tse = seh.GetTSE_Handle();

    CFeat_CI fi(seh);
    while (fi) {
        rval |= RepairXrefs(*(fi->GetSeq_feat()), tse);
        ++fi;
    }
    return rval;
}


//LCOV_EXCL_START
//not used by asn_cleanup but used by other applications
bool CCleanup::FindMatchingLocusGene(CSeq_feat& f, const CGene_ref& gene_xref, CBioseq_Handle bsh)
{
    bool match = false;
    string locus1;
    if (gene_xref.IsSetLocus())
        locus1 = gene_xref.GetLocus();
    for (CFeat_CI feat_ci(bsh, SAnnotSelector(CSeqFeatData::eSubtype_gene)); feat_ci; ++feat_ci)
    {
        string locus2;
        if ( !f.Equals(*feat_ci->GetSeq_feat()) && feat_ci->GetSeq_feat()->IsSetData() && feat_ci->GetSeq_feat()->GetData().IsGene()
             && feat_ci->GetSeq_feat()->GetData().GetGene().IsSetLocus())
        {
            locus2 = feat_ci->GetSeq_feat()->GetData().GetGene().GetLocus();
        }
        if (!locus1.empty() && !locus2.empty() && locus1 == locus2)
        {
            match = true;
            break;
        }
    }
    return match;
}

bool CCleanup::RemoveOrphanLocusGeneXrefs(CSeq_feat& f, CBioseq_Handle bsh)
{
    if (!f.IsSetXref()) {
        return false;
    }
    bool any_removed = false;
    CSeq_feat::TXref::iterator xit = f.SetXref().begin();
    while (xit != f.SetXref().end()) {
        if ((*xit)->IsSetData() && (*xit)->GetData().IsGene() &&
            !(*xit)->GetData().GetGene().IsSuppressed() && !FindMatchingLocusGene(f, (*xit)->GetData().GetGene(), bsh)) {
            xit = f.SetXref().erase(xit);
            any_removed = true;
        } else {
            ++xit;
        }
    }
    if (any_removed) {
        if (f.IsSetXref() && f.GetXref().empty()) {
            f.ResetXref();
        }
    }
    return any_removed;
}


bool CCleanup::FindMatchingLocus_tagGene(CSeq_feat& f, const CGene_ref& gene_xref, CBioseq_Handle bsh)
{
    bool match = false;
    string locus_tag1;
    if (gene_xref.IsSetLocus_tag())
        locus_tag1 = gene_xref.GetLocus_tag();
    for (CFeat_CI feat_ci(bsh, SAnnotSelector(CSeqFeatData::eSubtype_gene)); feat_ci; ++feat_ci)
    {
        string locus_tag2;
        if ( !f.Equals(*feat_ci->GetSeq_feat()) && feat_ci->GetSeq_feat()->IsSetData() && feat_ci->GetSeq_feat()->GetData().IsGene()
             && feat_ci->GetSeq_feat()->GetData().GetGene().IsSetLocus_tag())
        {
            locus_tag2 = feat_ci->GetSeq_feat()->GetData().GetGene().GetLocus_tag();
        }
        if (!locus_tag1.empty() && !locus_tag2.empty() && locus_tag1 == locus_tag2)
        {
            match = true;
            break;
        }
    }
    return match;
}

bool CCleanup::RemoveOrphanLocus_tagGeneXrefs(CSeq_feat& f, CBioseq_Handle bsh)
{
    if (!f.IsSetXref()) {
        return false;
    }
    bool any_removed = false;
    CSeq_feat::TXref::iterator xit = f.SetXref().begin();
    while (xit != f.SetXref().end()) {
        if ((*xit)->IsSetData() && (*xit)->GetData().IsGene() &&
            !(*xit)->GetData().GetGene().IsSuppressed() && !FindMatchingLocus_tagGene(f, (*xit)->GetData().GetGene(), bsh)) {
            xit = f.SetXref().erase(xit);
            any_removed = true;
        } else {
            ++xit;
        }
    }
    if (any_removed) {
        if (f.IsSetXref() && f.GetXref().empty()) {
            f.ResetXref();
        }
    }
    return any_removed;
}


bool CCleanup::SeqLocExtend(CSeq_loc& loc, size_t pos_, CScope& scope)
{
    TSeqPos pos = static_cast<TSeqPos>(pos_);
    TSeqPos loc_start = loc.GetStart(eExtreme_Positional);
    TSeqPos loc_stop = loc.GetStop(eExtreme_Positional);
    bool partial_start = loc.IsPartialStart(eExtreme_Positional);
    bool partial_stop = loc.IsPartialStop(eExtreme_Positional);
    ENa_strand strand = loc.GetStrand();
    CRef<CSeq_loc> new_loc;
    bool changed = false;

    if (pos < loc_start) {
        CRef<CSeq_id> id(new CSeq_id());
        id->Assign(*(loc.GetId()));
        CRef<CSeq_loc> add(new CSeq_loc(*id, pos, loc_start - 1, strand));
        add->SetPartialStart(partial_start, eExtreme_Positional);
        new_loc = sequence::Seq_loc_Add(loc, *add, CSeq_loc::fSort | CSeq_loc::fMerge_AbuttingOnly, &scope);
        changed = true;
    } else if (pos > loc_stop) {
        CRef<CSeq_id> id(new CSeq_id());
        id->Assign(*(loc.GetId()));
        CRef<CSeq_loc> add(new CSeq_loc(*id, loc_stop + 1, pos, strand));
        add->SetPartialStop(partial_stop, eExtreme_Positional);
        new_loc = sequence::Seq_loc_Add(loc, *add, CSeq_loc::fSort | CSeq_loc::fMerge_AbuttingOnly, &scope);
        changed = true;
    }
    if (changed) {
        loc.Assign(*new_loc);
    }
    return changed;
}
//LCOV_EXCL_STOP


bool CCleanup::ExtendStopPosition(CSeq_feat& f, const CSeq_feat* cdregion, size_t extension_)
{
    TSeqPos extension = static_cast<TSeqPos>(extension_);
    CRef<CSeq_loc> new_loc(&f.SetLocation());

    CRef<CSeq_loc> last_interval;
    if (new_loc->IsMix()) {
        last_interval = new_loc->SetMix().SetLastLoc();
    }
    else if (new_loc->IsPacked_int()) {
        last_interval = Ref(new CSeq_loc());
        last_interval->SetInt(*(new_loc->SetPacked_int().Set().back()));
    }
    else     
    {
        last_interval = new_loc;
    }

    CConstRef<CSeq_id> id(last_interval->GetId());

    TSeqPos new_start;
    TSeqPos new_stop;

    // the last element of the mix or the single location MUST be converted into interval
    // whether it's whole or point, etc
    if (last_interval->IsSetStrand() && last_interval->GetStrand() == eNa_strand_minus) {
        new_start = (cdregion ? cdregion->GetLocation().GetStart(eExtreme_Positional) :
              last_interval->GetStart(eExtreme_Positional)) - extension;

        new_stop = last_interval->GetStop(eExtreme_Positional);
        last_interval->SetInt().SetStrand(eNa_strand_minus);
    }
    else {
        new_start = last_interval->GetStart(eExtreme_Positional);
        new_stop = (cdregion ? cdregion->GetLocation().GetStop(eExtreme_Positional) :
            last_interval->GetStop(eExtreme_Positional)) + extension;
    }
    last_interval->SetInt().SetFrom(new_start);
    last_interval->SetInt().SetTo(new_stop);
    last_interval->SetInt().SetId().Assign(*id);

    new_loc->SetPartialStop(false, eExtreme_Biological);

    return true;
}

bool CCleanup::ExtendToStopCodon(CSeq_feat& f, CBioseq_Handle bsh, size_t limit)
{
    const CSeq_loc& loc = f.GetLocation();

    CCdregion::TFrame frame = CCdregion::eFrame_not_set;
    const CGenetic_code* code = nullptr;
    // we need to extract frame and cd_region from linked cd_region
    if (f.IsSetData() && f.GetData().IsCdregion())
    {
        if (f.GetData().GetCdregion().IsSetCode())
           code = &(f.GetData().GetCdregion().GetCode());
        if (f.GetData().GetCdregion().IsSetFrame())
           frame = f.GetData().GetCdregion().GetFrame();
    }

    TSeqPos stop = loc.GetStop(eExtreme_Biological);
    if (stop < 1 || stop > bsh.GetBioseqLength() - 1) {
        // no room to extend
        return false;
    }
    // figure out if we have a partial codon at the end
    size_t orig_len = sequence::GetLength(loc, &(bsh.GetScope()));
    size_t len = orig_len;

    if (frame == CCdregion::eFrame_two) {
        len -= 1;
    } else if (frame == CCdregion::eFrame_three) {
        len -= 2;
    }

    TSeqPos mod = len % 3;
    CRef<CSeq_loc> vector_loc(new CSeq_loc());
    vector_loc->SetInt().SetId().Assign(*(bsh.GetId().front().GetSeqId()));

    if (loc.IsSetStrand() && loc.GetStrand() == eNa_strand_minus) {
        vector_loc->SetInt().SetFrom(0);
        vector_loc->SetInt().SetTo(stop + mod - 1);
        vector_loc->SetStrand(eNa_strand_minus);
    } else {
        vector_loc->SetInt().SetFrom(stop - mod + 1);
        vector_loc->SetInt().SetTo(bsh.GetInst_Length() - 1);
    }

    CSeqVector seq(*vector_loc, bsh.GetScope(), CBioseq_Handle::eCoding_Iupac);
    // reserve our space
    size_t usable_size = seq.size();

    if (limit > 0 && usable_size > limit) {
        usable_size = limit;
    }

    // get appropriate translation table
    const CTrans_table & tbl =
        (code ? CGen_code_table::GetTransTable(*code) :
        CGen_code_table::GetTransTable(1));

    // main loop through bases
    CSeqVector::const_iterator start = seq.begin();

    size_t i;
    size_t k;
    int state = 0;
    size_t length = usable_size / 3;

    for (i = 0; i < length; ++i) {
        // loop through one codon at a time
        for (k = 0; k < 3; ++k, ++start) {
            state = tbl.NextCodonState(state, *start);
        }

        if (tbl.GetCodonResidue(state) == '*') {
            TSeqPos extension = static_cast<TSeqPos>(((i + 1) * 3) - mod);
            ExtendStopPosition(f, 0, extension);
            return true;
        }
    }

    return false;
}


bool CCleanup::SetBestFrame(CSeq_feat& cds, CScope& scope)
{
    bool changed = false;
    CCdregion::TFrame frame = CCdregion::eFrame_not_set;
    if (cds.GetData().GetCdregion().IsSetFrame()) {
        frame = cds.GetData().GetCdregion().GetFrame();
    }

    CCdregion::TFrame new_frame = CSeqTranslator::FindBestFrame(cds, scope);
    if (frame != new_frame) {
        cds.SetData().SetCdregion().SetFrame(new_frame);
        changed = true;
    }
    return changed;
}

// like C's function GetFrameFromLoc, but better
bool CCleanup::SetFrameFromLoc(CCdregion::EFrame &frame, const CSeq_loc& loc, CScope& scope)
{
    if (!loc.IsPartialStart(eExtreme_Biological)) {
        if (frame != CCdregion::eFrame_one) {
            frame = CCdregion::eFrame_one;
            return true;
        }
        return false;
    }
    if (loc.IsPartialStop(eExtreme_Biological)) {
        // cannot make a determination if both ends are partial
        return false;
    }

    const TSeqPos seq_len = sequence::GetLength(loc, &scope);

    CCdregion::EFrame desired_frame = CCdregion::eFrame_not_set;

    // have complete last codon, get frame from length
    switch( (seq_len % 3) + 1 ) {
        case 1:
            desired_frame = CCdregion::eFrame_one;
            break;
        case 2:
            desired_frame = CCdregion::eFrame_two;
            break;
        case 3:
            desired_frame = CCdregion::eFrame_three;
            break;
        default:
            // mathematically impossible
            _ASSERT(false);
            return false;
    }
    if (frame != desired_frame) {
        frame = desired_frame;
        return true;
    }
    return false;
}


bool CCleanup::SetFrameFromLoc(CCdregion &cdregion, const CSeq_loc& loc, CScope& scope)
{
    CCdregion::EFrame frame = CCdregion::eFrame_not_set;
    if (cdregion.IsSetFrame()) {
        frame = cdregion.GetFrame();
    }
    if (SetFrameFromLoc(frame, loc, scope)) {
        cdregion.SetFrame(frame);
        return true;
    } else {
        return false;
    }
}


bool s_IsLocationEndAtOtherLocationInternalEndpoint(const CSeq_loc& loc, const CSeq_loc& other_loc)
{
    size_t loc_end = loc.GetStop(eExtreme_Biological);
    CSeq_loc_CI other_int(other_loc);
    while (other_int) {
        if (other_int.IsSetStrand() &&
            other_int.GetStrand() == eNa_strand_minus) {
            if (loc.IsSetStrand() && loc.GetStrand() == eNa_strand_minus &&
                loc_end == other_int.GetRange().GetFrom()) {
                return true;
            }
        } else {
            if ((!loc.IsSetStrand() || loc.GetStrand() != eNa_strand_minus) &&
                loc_end == other_int.GetRange().GetTo()) {
                return true;
            }
        }
        ++other_int;
    }
    return false;
}


bool CCleanup::ExtendToStopIfShortAndNotPartial(CSeq_feat& f, CBioseq_Handle bsh, bool check_for_stop)
{
    if (!f.GetData().IsCdregion()) {
        // not coding region
        return false;
    }
    if (sequence::IsPseudo(f, bsh.GetScope())) {
        return false;
    }
    if (f.GetLocation().IsPartialStop(eExtreme_Biological)) {
        return false;
    }
    CConstRef<CSeq_feat> mrna = sequence::GetmRNAforCDS(f, bsh.GetScope());
    if (mrna) {
        if (mrna->GetLocation().GetStop(eExtreme_Biological) == f.GetLocation().GetStop(eExtreme_Biological)) {
            //ok
        } else if (s_IsLocationEndAtOtherLocationInternalEndpoint(f.GetLocation(), mrna->GetLocation())) {
            return false;
        }
    }

    if (check_for_stop) {
        string translation;
        try {
            CSeqTranslator::Translate(f, bsh.GetScope(), translation, true);
        } catch (CSeqMapException&) {
            //unable to translate
            return false;
        } catch (CSeqVectorException&) {
            //unable to translate
            return false;
        }
        if (NStr::EndsWith(translation, "*")) {
            //already has stop codon
            return false;
        }
    }

    return ExtendToStopCodon(f, bsh, 3);
}


bool CCleanup::LocationMayBeExtendedToMatch(const CSeq_loc& orig, const CSeq_loc& improved)
{
    if ((orig.GetStrand() == eNa_strand_minus &&
        orig.GetStop(eExtreme_Biological) > improved.GetStop(eExtreme_Biological)) ||
        (orig.GetStrand() != eNa_strand_minus &&
        orig.GetStop(eExtreme_Biological) < improved.GetStop(eExtreme_Biological))) {
        return true;
    }

    return false;
}

void CCleanup::SetProteinName(CProt_ref& prot_ref, const string& protein_name, bool append)
{
    if (append && prot_ref.IsSetName() && prot_ref.GetName().size() > 0) {
        if (!NStr::IsBlank(prot_ref.GetName().front())) {
            prot_ref.SetName().front() += "; ";
        }
        prot_ref.SetName().front() += protein_name;
    } else {
        prot_ref.SetName().push_back(protein_name);
    }
}


void CCleanup::SetMrnaName(CSeq_feat& mrna, const string& protein_name)
{
    bool used_qual = false;
    if (mrna.IsSetQual()) {
        for (auto it = mrna.SetQual().begin(); it != mrna.SetQual().end(); it++) {
            if ((*it)->IsSetQual() && NStr::EqualNocase((*it)->GetQual(), "product")) {
                (*it)->SetVal(protein_name);
                used_qual = true;
                break;
            }
        }
    }
    if (!used_qual || (mrna.IsSetData() && mrna.GetData().IsRna() && mrna.GetData().GetRna().IsSetExt())) {
        string remainder;
        mrna.SetData().SetRna().SetRnaProductName(protein_name, remainder);
    }
}


//LCOV_EXCL_START
//seems to be unused
bool CCleanup::s_IsProductOnFeat(const CSeq_feat& cds)
{
    if (cds.IsSetXref()) {
        for (auto it = cds.GetXref().begin(); it != cds.GetXref().end(); it++) {
            if ((*it)->IsSetData() && (*it)->GetData().IsProt()) {
                return true;
            }
        }
    }
    if (cds.IsSetQual()) {
        for (auto it = cds.GetQual().begin(); it != cds.GetQual().end(); it++) {
            if ((*it)->IsSetQual() && NStr::EqualNocase((*it)->GetQual(), "product")) {
                return true;
            }
        }
    }
    return false;
}
//LCOV_EXCL_STOP


void CCleanup::s_SetProductOnFeat(CSeq_feat& feat, const string& protein_name, bool append)
{
    if (feat.IsSetXref()) {
        // see if this seq-feat already has a prot xref
        for (auto it = feat.SetXref().begin(); it != feat.SetXref().end(); it++) {
            if ((*it)->IsSetData() && (*it)->GetData().IsProt()) {
                SetProteinName((*it)->SetData().SetProt(), protein_name, append);
                break;
            }
        }
    }
    if (feat.IsSetQual()) {
        for (auto it = feat.SetQual().begin(); it != feat.SetQual().end(); it++) {
            if ((*it)->IsSetQual() && NStr::EqualNocase((*it)->GetQual(), "product")) {
                if ((*it)->IsSetVal() && !NStr::IsBlank((*it)->GetVal()) && append) {
                    (*it)->SetVal((*it)->GetVal() + "; " + protein_name);
                } else {
                    (*it)->SetVal(protein_name);
                }
            }
        }
    }
}


void CCleanup::SetProteinName(CSeq_feat& cds, const string& protein_name, bool append, CScope& scope)
{
    s_SetProductOnFeat(cds, protein_name, append);
    bool added = false;
    if (cds.IsSetProduct()) {
        CBioseq_Handle prot = scope.GetBioseqHandle(cds.GetProduct());
        if (prot) {
            // find main protein feature
            CFeat_CI feat_ci(prot, CSeqFeatData::eSubtype_prot);
            if (feat_ci) {
                CRef<CSeq_feat> new_prot(new CSeq_feat());
                new_prot->Assign(feat_ci->GetOriginalFeature());
                SetProteinName(new_prot->SetData().SetProt(), protein_name, append);
                CSeq_feat_EditHandle feh(feat_ci->GetSeq_feat_Handle());
                feh.Replace(*new_prot);
            } else {
                // make new protein feature
                feature::AddProteinFeature(*(prot.GetCompleteBioseq()), protein_name, cds, scope);
            }
            added = true;
        }
    }
    if (!added) {
        if (cds.IsSetXref()) {
            // see if this seq-feat already has a prot xref
            NON_CONST_ITERATE(CSeq_feat::TXref, it, cds.SetXref()) {
                if ((*it)->IsSetData() && (*it)->GetData().IsProt()) {
                    SetProteinName((*it)->SetData().SetProt(), protein_name, append);
                    added = true;
                    break;
                }
            }
        }
        if (!added) {
            CRef<CSeqFeatXref> xref(new CSeqFeatXref());
            xref->SetData().SetProt().SetName().push_back(protein_name);
            cds.SetXref().push_back(xref);
        }
    }
}


const string& CCleanup::GetProteinName(const CProt_ref& prot)
{
    if (prot.IsSetName() && !prot.GetName().empty()) {
        return prot.GetName().front();
    } else {
        return kEmptyStr;
    }
}


static const string& s_GetProteinNameFromXrefOrQual(const CSeq_feat& cds) {
    if (cds.IsSetXref()) {
        ITERATE(CSeq_feat::TXref, it, cds.GetXref()) {
            if ((*it)->IsSetData() && (*it)->GetData().IsProt()) {
                return CCleanup::GetProteinName((*it)->GetData().GetProt());
            }
        }
    }
    if (cds.IsSetQual()) {
        for (auto it = cds.GetQual().begin(); it != cds.GetQual().end(); it++) {
            if ((*it)->IsSetQual() && (*it)->IsSetVal() && NStr::EqualNocase((*it)->GetQual(), "product")) {
                return (*it)->GetVal();
            }
        }
    }

    return kEmptyStr;
}


const string& CCleanup::GetProteinName(const CSeq_feat& cds, CSeq_entry_Handle seh)
{
    if (cds.IsSetProduct() && cds.GetProduct().GetId()) {
        CBioseq_Handle prot = seh.GetBioseqHandle(*(cds.GetProduct().GetId()));
        if (prot) {
            CFeat_CI f(prot, CSeqFeatData::eSubtype_prot);
            if (f) {
                return GetProteinName(f->GetData().GetProt());
            }
        }
    }

    return s_GetProteinNameFromXrefOrQual(cds);
}


bool CCleanup::SetCDSPartialsByFrameAndTranslation(CSeq_feat& cds, CScope& scope)
{
    bool any_change = false;

    if (!cds.GetLocation().IsPartialStart(eExtreme_Biological) &&
        cds.GetData().GetCdregion().IsSetFrame() &&
        cds.GetData().GetCdregion().GetFrame() != CCdregion::eFrame_not_set &&
        cds.GetData().GetCdregion().GetFrame() != CCdregion::eFrame_one) {
        cds.SetLocation().SetPartialStart(true, eExtreme_Biological);
        any_change = true;
    }

    if (!cds.GetLocation().IsPartialStart(eExtreme_Biological) || !cds.GetLocation().IsPartialStop(eExtreme_Biological)) {
        // look for start and stop codon
        string transl_prot;
        try {
            CSeqTranslator::Translate(cds, scope, transl_prot,
                true,   // include stop codons
                false);  // do not remove trailing X/B/Z

        } catch (const runtime_error&) {
        }
        if (!NStr::IsBlank(transl_prot)) {
            if (!cds.GetLocation().IsPartialStart(eExtreme_Biological) && !NStr::StartsWith(transl_prot, "M")) {
                cds.SetLocation().SetPartialStart(true, eExtreme_Biological);
                any_change = true;
            }
            if (!cds.GetLocation().IsPartialStop(eExtreme_Biological) && !NStr::EndsWith(transl_prot, "*")) {
                cds.SetLocation().SetPartialStop(true, eExtreme_Biological);
                any_change = true;
            }
        }
    }

    any_change |= feature::AdjustFeaturePartialFlagForLocation(cds);

    return any_change;
}


bool CCleanup::ClearInternalPartials(CSeq_loc& loc, bool is_first, bool is_last)
{
    bool rval = false;
    switch (loc.Which()) {
        case CSeq_loc::e_Mix:
            rval |= ClearInternalPartials(loc.SetMix(), is_first, is_last);
            break;
        case CSeq_loc::e_Packed_int:
            rval |= ClearInternalPartials(loc.SetPacked_int(), is_first, is_last);
            break;
        default:
            break;
    }
    return rval;
}


bool CCleanup::ClearInternalPartials(CSeq_loc_mix& mix, bool is_first, bool is_last)
{
    bool rval = false;
    NON_CONST_ITERATE(CSeq_loc::TMix::Tdata, it, mix.Set()) {
        bool this_is_last = is_last && (*it == mix.Set().back());
        if ((*it)->IsMix() || (*it)->IsPacked_int()) {
            rval |= ClearInternalPartials(**it, is_first, this_is_last);
        } else {
            if (!is_first &&
                (*it)->IsPartialStart(eExtreme_Biological)) {
                (*it)->SetPartialStart(false, eExtreme_Biological);
                rval = true;
            }
            if (!this_is_last &&
                (*it)->IsPartialStop(eExtreme_Biological)) {
                (*it)->SetPartialStop(false, eExtreme_Biological);
                rval = true;
            }
        }
        is_first = false;
    }
    return rval;
}


bool CCleanup::ClearInternalPartials(CPacked_seqint& pint, bool is_first, bool is_last)
{
    bool rval = false;

    NON_CONST_ITERATE(CSeq_loc::TPacked_int::Tdata, it, pint.Set()) {
        bool this_is_last = is_last && (*it == pint.Set().back());
        if (!is_first && (*it)->IsPartialStart(eExtreme_Biological)) {
            (*it)->SetPartialStart(false, eExtreme_Biological);
            rval = true;
        }
        if (!this_is_last && (*it)->IsPartialStop(eExtreme_Biological)) {
            (*it)->SetPartialStop(false, eExtreme_Biological);
            rval = true;
        }
        is_first = false;
    }
    return rval;
}


bool CCleanup::ClearInternalPartials(CSeq_entry_Handle seh)
{
    bool rval = false;
    CFeat_CI f(seh);
    while (f) {
        CRef<CSeq_feat> new_feat(new CSeq_feat());
        new_feat->Assign(*(f->GetSeq_feat()));
        if (ClearInternalPartials(new_feat->SetLocation())) {
            CSeq_feat_EditHandle eh(f->GetSeq_feat_Handle());
            eh.Replace(*new_feat);
        }
        ++f;
    }

    return rval;
}


bool CCleanup::SetFeaturePartial(CSeq_feat& f)
{
    if (!f.IsSetLocation()) {
        return false;
    }
    bool partial = false;
    CSeq_loc_CI li(f.GetLocation());
    while (li && !partial) {
        if (li.GetFuzzFrom() || li.GetFuzzTo()) {
            partial = true;
            break;
        }
        ++li;
    }
    bool changed = false;
    if (f.IsSetPartial() && f.GetPartial()) {
        if (!partial) {
            f.ResetPartial();
            changed = true;
        }
    } else {
        if (partial) {
            f.SetPartial(true);
            changed = true;
        }
    }
    return changed;
}


bool CCleanup::UpdateECNumbers(CProt_ref::TEc & ec_num_list)
{
    bool changed = false;
    // CProt_ref::TEc is a list, so the iterator stays valid even if we
    // add new entries after the current one
    NON_CONST_ITERATE(CProt_ref::TEc, ec_num_iter, ec_num_list) {
        string & ec_num = *ec_num_iter;
        size_t tlen = ec_num.length();
        CleanVisStringJunk(ec_num);
        if (tlen != ec_num.length()) {
            changed = true;
        }
        if (CProt_ref::GetECNumberStatus(ec_num) == CProt_ref::eEC_replaced &&
            !CProt_ref::IsECNumberSplit(ec_num)) {
            string new_val = CProt_ref::GetECNumberReplacement(ec_num);
            if (!NStr::IsBlank(new_val)) {
                ec_num = new_val;
                changed = true;
            }
        }

    }
    return changed;
}


bool CCleanup::RemoveBadECNumbers(CProt_ref::TEc & ec_num_list)
{
    bool changed = false;
    CProt_ref::TEc::iterator ec_num_iter = ec_num_list.begin();
    while (ec_num_iter != ec_num_list.end()) {
        string & ec_num = *ec_num_iter;
        size_t tlen = ec_num.length();
        CleanVisStringJunk(ec_num);
        if (tlen != ec_num.length()) {
            changed = true;
        }
        CProt_ref::EECNumberStatus ec_status = CProt_ref::GetECNumberStatus(ec_num);
        if (ec_status == CProt_ref::eEC_deleted || ec_status == CProt_ref::eEC_unknown || CProt_ref::IsECNumberSplit(ec_num)) {
            ec_num_iter = ec_num_list.erase(ec_num_iter);
            changed = true;
        } else {
            ++ec_num_iter;
        }

    }
    return changed;
}


bool CCleanup::FixECNumbers(CSeq_entry_Handle entry)
{
    bool any_change = false;
    CFeat_CI f(entry, CSeqFeatData::e_Prot);
    while (f) {
        if (f->GetData().GetProt().IsSetEc()) {
            bool this_change = false;
            CRef<CSeq_feat> new_feat(new CSeq_feat());
            new_feat->Assign(*(f->GetSeq_feat()));
            this_change = UpdateECNumbers(new_feat->SetData().SetProt().SetEc());
            this_change |= RemoveBadECNumbers(new_feat->SetData().SetProt().SetEc());
            if (new_feat->GetData().GetProt().GetEc().empty()) {
                new_feat->SetData().SetProt().ResetEc();
                this_change = true;
            }
            if (this_change) {
                CSeq_feat_EditHandle efh(*f);
                efh.Replace(*new_feat);
            }
        }
        ++f;
    }
    return any_change;
}


static bool s_SetGenePartialFromChildren(CSeq_feat& gene, const list<CMappedFeat>& children)
{
    bool partial5{ false };
    bool partial3{ false };

    // RW-2569
    // If any child feature is 5' or 3' partial, the gene is also partial
    for (const auto& child : children) {
        if (partial5 && partial3) { // No need to consider additional features
            break;
        }
        const auto& child_loc = child.GetLocation();
        if (! partial5) {
            partial5 = child_loc.IsPartialStart(eExtreme_Biological);
        }
        if (! partial3) {
            partial3 = child_loc.IsPartialStop(eExtreme_Biological);
        }
    }

    bool  any_change{ false };
    auto& gene_loc = gene.SetLocation();
    if (partial5 != gene_loc.IsPartialStart(eExtreme_Biological)) {
        gene_loc.SetPartialStart(partial5, eExtreme_Biological);
        any_change = true;
    }

    if (partial3 != gene_loc.IsPartialStop(eExtreme_Biological)) {
        gene_loc.SetPartialStop(partial3, eExtreme_Biological);
        any_change = true;
    }

    any_change |= feature::AdjustFeaturePartialFlagForLocation(gene);

    return any_change;
}


static bool s_SetGenePartialsFromChildren(const CSeq_entry_Handle& seh)
{
    bool any_changes{ false };

    feature::CFeatTree feat_tree(seh, SAnnotSelector().ExcludeFeatType(CSeqFeatData::e_Prot));
    for (CFeat_CI gene_it(seh, SAnnotSelector(CSeqFeatData::e_Gene)); gene_it; ++gene_it) {
        list<CMappedFeat> descendents;
        const auto&       children = feat_tree.GetChildren(*gene_it);
        for (auto child : children) {
            descendents.push_back(child);
            const auto& grandchildren = feat_tree.GetChildren(child);
            for (auto grandchild : grandchildren) {
                descendents.push_back(grandchild);
            }
        }
        auto new_gene = Ref(new CSeq_feat());
        new_gene->Assign(*(gene_it->GetSeq_feat()));
        if (s_SetGenePartialFromChildren(*new_gene, descendents)) {
            CSeq_feat_EditHandle gene_h(*gene_it);
            gene_h.Replace(*new_gene);
            any_changes = true;
        }
    }

    return any_changes;
}


bool CCleanup::SetMolinfoTech(CBioseq_Handle bsh, CMolInfo::ETech tech)
{
    CSeqdesc_CI di(bsh, CSeqdesc::e_Molinfo);
    if (di) {
        if (di->GetMolinfo().IsSetTech() && di->GetMolinfo().GetTech() == tech) {
            // no change necessary
            return false;
        } else {
            CSeqdesc* d = const_cast<CSeqdesc*>(&(*di));
            d->SetMolinfo().SetTech(tech);
            return true;
        }
    }
    CRef<CSeqdesc> m(new CSeqdesc());
    m->SetMolinfo().SetTech(tech);
    if (bsh.IsSetInst() && bsh.GetInst().IsSetMol() && bsh.IsAa()) {
        m->SetMolinfo().SetBiomol(CMolInfo::eBiomol_peptide);
    }
    CBioseq_EditHandle eh = bsh.GetEditHandle();
    eh.AddSeqdesc(*m);
    return true;
}


//LCOV_EXCL_START
//does not appear to be used
bool CCleanup::SetMolinfoBiomol(CBioseq_Handle bsh, CMolInfo::EBiomol biomol)
{
    CSeqdesc_CI di(bsh, CSeqdesc::e_Molinfo);
    if (di) {
        if (di->GetMolinfo().IsSetTech() && di->GetMolinfo().GetBiomol() == biomol) {
            // no change necessary
            return false;
        } else {
            CSeqdesc* d = const_cast<CSeqdesc*>(&(*di));
            d->SetMolinfo().SetBiomol(biomol);
            return true;
        }
    }
    CRef<CSeqdesc> m(new CSeqdesc());
    m->SetMolinfo().SetBiomol(biomol);
    CBioseq_EditHandle eh = bsh.GetEditHandle();
    eh.AddSeqdesc(*m);
    return true;
}
//LCOV_EXCL_STOP



bool CCleanup::AddProteinTitle(CBioseq_Handle bsh)
{
    if (!bsh.IsSetInst() || !bsh.GetInst().IsSetMol() || !bsh.IsAa()) {
        return false;
    }
    if (bsh.IsSetId()) {
        ITERATE(CBioseq_Handle::TId, it, bsh.GetId()) {
            // do not add titles for sequences with certain IDs
            switch (it->Which()) {
                case CSeq_id::e_Pir:
                case CSeq_id::e_Swissprot:
                case CSeq_id::e_Patent:
                case CSeq_id::e_Prf:
                case CSeq_id::e_Pdb:
                    return false;
                    break;
                default:
                    break;
            }
        }
    }

    string new_defline = sequence::CDeflineGenerator().GenerateDefline(bsh, sequence::CDeflineGenerator::fIgnoreExisting);

    CAutoAddDesc title_desc(bsh.GetEditHandle().SetDescr(), CSeqdesc::e_Title);

    bool modified = title_desc.Set().SetTitle() != new_defline; // get or create a title
    if (modified)
      title_desc.Set().SetTitle().swap(new_defline);
    return modified;
}



static bool s_IsCleanupObject(const CSeqdesc& desc)
{
    return (desc.IsUser() && 
            (desc.GetUser().GetObjectType() == CUser_object::eObjectType_Cleanup));
}


bool CCleanup::RemoveNcbiCleanupObject(CSeq_descr& descr)
{
    if (!descr.IsSet() || descr.Get().empty()) {
        return false;
    }

    auto& elements = descr.Set();
    auto it = elements.begin();

    bool rval = false;
    while (it != elements.end()) {
        if (s_IsCleanupObject(**it)) {
            it = elements.erase(it);
            rval = true;
        } 
        else {
            ++it;
        }
    }
    return rval;
}


bool CCleanup::RemoveNcbiCleanupObject(CSeq_entry &seq_entry)
{
    bool rval = false;

    if (seq_entry.IsSetDescr()) {
        rval = RemoveNcbiCleanupObject(seq_entry.SetDescr());
        if (rval && seq_entry.GetDescr().Get().empty()) {
            if (seq_entry.IsSeq()) {
                seq_entry.SetSeq().ResetDescr();
            }
            else if (seq_entry.IsSet()) {
                seq_entry.SetSet().ResetDescr();
            }
        }
    }

    if (seq_entry.IsSet() && seq_entry.GetSet().IsSetSeq_set()) {
        for (auto pEntry : seq_entry.SetSet().SetSeq_set()) {
            rval |= RemoveNcbiCleanupObject(*pEntry);
        }
    }
    return rval;
}


void CCleanup::AddNcbiCleanupObject(int ncbi_cleanup_version, CSeq_descr& descr)
{
    // update existing
    if (descr.IsSet()) {
        for (auto pDesc : descr.Set()) {
            if (s_IsCleanupObject(*pDesc)) {
                pDesc->SetUser().UpdateNcbiCleanup(ncbi_cleanup_version);
                return;
            }
        }
    }

    // create new cleanup object
    auto pCleanupObject = Ref(new CSeqdesc());
    auto& user = pCleanupObject->SetUser();
    user.UpdateNcbiCleanup(ncbi_cleanup_version);
    descr.Set().push_back(pCleanupObject);
}


//LCOV_EXCL_START
//not used by asn_cleanup but used by functions used by other applications
void GetSourceDescriptors(const CSeq_entry& se, vector<const CSeqdesc* >& src_descs)
{
    if (se.IsSetDescr()) {
        ITERATE(CBioseq::TDescr::Tdata, it, se.GetDescr().Get()) {
            if ((*it)->IsSource() && (*it)->GetSource().IsSetOrg()) {
                src_descs.push_back(*it);
            }
        }
    }

    if (se.IsSet() && se.GetSet().IsSetSeq_set()) {
        ITERATE(CBioseq_set::TSeq_set, it, se.GetSet().GetSeq_set()) {
            GetSourceDescriptors(**it, src_descs);
        }
    }
}
//LCOV_EXCL_STOP


//LCOV_EXCL_START
//not used by asn_cleanup
bool CCleanup::TaxonomyLookup(CSeq_entry_Handle seh)
{
    bool any_changes = false;

    vector<CRef<COrg_ref> > rq_list;
    vector<const CSeqdesc* > src_descs;
    vector<CConstRef<CSeq_feat> > src_feats;

    GetSourceDescriptors(*(seh.GetCompleteSeq_entry()), src_descs);
    vector<const CSeqdesc* >::iterator desc_it = src_descs.begin();
    while (desc_it != src_descs.end()) {
        // add org ref for descriptor to request list
        CRef<COrg_ref> org(new COrg_ref());
        org->Assign((*desc_it)->GetSource().GetOrg());
        rq_list.push_back(org);

        ++desc_it;
    }

    CFeat_CI feat(seh, SAnnotSelector(CSeqFeatData::e_Biosrc));
    while (feat) {
        if (feat->GetData().GetBiosrc().IsSetOrg()) {
            // add org ref for feature to request list
            CRef<COrg_ref> org(new COrg_ref());
            org->Assign(feat->GetData().GetBiosrc().GetOrg());
            rq_list.push_back(org);
            // add feature to list
            src_feats.push_back(feat->GetOriginalSeq_feat());
        }
        ++feat;
    }

    if (rq_list.size() > 0) {
        CTaxon3 taxon3(CTaxon3::initialize::yes);
        CRef<CTaxon3_reply> reply = taxon3.SendOrgRefList(rq_list);
        if (reply) {
            CTaxon3_reply::TReply::const_iterator reply_it = reply->GetReply().begin();

            // process descriptor responses
            desc_it = src_descs.begin();

            while (reply_it != reply->GetReply().end()
                && desc_it != src_descs.end()) {
                if ((*reply_it)->IsData() &&
                    !(*desc_it)->GetSource().GetOrg().Equals((*reply_it)->GetData().GetOrg())) {
                    any_changes = true;
                    CSeqdesc* desc = const_cast<CSeqdesc*>(*desc_it);
                    desc->SetSource().SetOrg().Assign((*reply_it)->GetData().GetOrg());
                    desc->SetSource().SetOrg().CleanForGenBank();
                }
                ++reply_it;
                ++desc_it;
            }

            // process feature responses
            vector<CConstRef<CSeq_feat> >::iterator feat_it = src_feats.begin();
            while (reply_it != reply->GetReply().end()
                && feat_it != src_feats.end()) {
                if ((*reply_it)->IsData() &&
                    !(*feat_it)->GetData().GetBiosrc().GetOrg().Equals((*reply_it)->GetData().GetOrg())) {
                    any_changes = true;
                    CRef<CSeq_feat> new_feat(new CSeq_feat());
                    new_feat->Assign(**feat_it);
                    new_feat->SetData().SetBiosrc().SetOrg().Assign((*reply_it)->GetData().GetOrg());
                    CSeq_feat_Handle fh = seh.GetScope().GetSeq_featHandle(**feat_it);
                    CSeq_feat_EditHandle efh(fh);
                    efh.Replace(*new_feat);
                }
                ++reply_it;
                ++feat_it;
            }
        }
    }

    return any_changes;
}
//LCOV_EXCL_STOP


CRef<CSeq_entry> CCleanup::AddProtein(const CSeq_feat& cds, CScope& scope)
{
    CBioseq_Handle cds_bsh = scope.GetBioseqHandle(cds.GetLocation());
    if (!cds_bsh) {
        return CRef<CSeq_entry>();
    }
    CSeq_entry_Handle seh = cds_bsh.GetSeq_entry_Handle();
    if (!seh) {
        return CRef<CSeq_entry>();
    }

    CRef<CBioseq> new_product = CSeqTranslator::TranslateToProtein(cds, scope);
    if (new_product.Empty()) {
        return CRef<CSeq_entry>();
    }

    CRef<CSeqdesc> molinfo(new CSeqdesc());
    molinfo->SetMolinfo().SetBiomol(CMolInfo::eBiomol_peptide);
    molinfo->SetMolinfo().SetTech(CMolInfo::eTech_concept_trans);
    new_product->SetDescr().Set().push_back(molinfo);

    if (cds.IsSetProduct()) {
        CRef<CSeq_id> prot_id(new CSeq_id());
        prot_id->Assign(*(cds.GetProduct().GetId()));
        new_product->SetId().push_back(prot_id);
    }
    CRef<CSeq_entry> prot_entry(new CSeq_entry());
    prot_entry->SetSeq(*new_product);

    CSeq_entry_EditHandle eh = seh.GetEditHandle();
    if (!eh.IsSet()) {
        CBioseq_set_Handle nuc_parent = eh.GetParentBioseq_set();
        if (nuc_parent && nuc_parent.IsSetClass() && nuc_parent.GetClass() == CBioseq_set::eClass_nuc_prot) {
            eh = nuc_parent.GetParentEntry().GetEditHandle();
        }
    }
    if (!eh.IsSet()) {
        eh.ConvertSeqToSet();
        // move all descriptors on nucleotide sequence except molinfo, title, and create-date to set
        eh.SetSet().SetClass(CBioseq_set::eClass_nuc_prot);
        CConstRef<CBioseq_set> set = eh.GetSet().GetCompleteBioseq_set();
        if (set && set->IsSetSeq_set()) {
            CConstRef<CSeq_entry> nuc = set->GetSeq_set().front();
            if (nuc->IsSetDescr()) {
               auto neh = eh.GetScope().GetSeq_entryEditHandle(*nuc);
               auto it = nuc->GetDescr().Get().begin();
               while (it != nuc->GetDescr().Get().end()) {
                  if (!(*it)->IsMolinfo() && !(*it)->IsTitle() && !(*it)->IsCreate_date()) {
                     CRef<CSeqdesc> copy(new CSeqdesc());
                     copy->Assign(**it);
                     eh.AddSeqdesc(*copy);
                     neh.RemoveSeqdesc(**it);
                     if (nuc->IsSetDescr()) {
                       it = nuc->GetDescr().Get().begin();
                     }
                     else {
                        break;
                     } 
                  }   
                  else {
                    ++it;
                  }
               }
            }
        }
    }

    CSeq_entry_EditHandle added = eh.AttachEntry(*prot_entry);
    return prot_entry;
}

bool CCleanup::SetGeneticCodes(CBioseq_Handle bsh)
{
    if (!bsh) {
        return false;
    }
    if (!bsh.IsNa()) {
        return false;
    }

    CSeqdesc_CI src(bsh, CSeqdesc::e_Source);
    if (!src) {
        // no source, don't fix
        return false;
    }
    const auto& bsrc = src->GetSource();
    if (!bsrc.IsSetOrg() || !bsrc.IsSetOrgname()) {
        return false;
    }
    const auto& orgname = bsrc.GetOrg().GetOrgname();
    if (!orgname.IsSetGcode() && !orgname.IsSetMgcode() && !orgname.IsSetPgcode()) {
        return false;
    }
    int bioseqGenCode = src->GetSource().GetGenCode();

    bool any_changed = false;
    // set Cdregion's gcode from BioSource (unless except-text)
    SAnnotSelector sel(CSeqFeatData::e_Cdregion);
    CFeat_CI feat_ci(bsh, sel);
    for (; feat_ci; ++feat_ci) {
        const CSeq_feat& feat = feat_ci->GetOriginalFeature();
        const CCdregion& cds = feat.GetData().GetCdregion();
        int cdregionGenCode = (cds.IsSetCode() ?
            cds.GetCode().GetId() :
            0);
        if (cdregionGenCode != bioseqGenCode)
        {
            // make cdregion's gencode match bioseq's gencode,
            // if allowed
            if (!feat.HasExceptionText("genetic code exception"))
            {
                CRef<CSeq_feat> new_feat(new CSeq_feat);
                new_feat->Assign(feat);
                CCdregion& new_cds = new_feat->SetData().SetCdregion();
                new_cds.ResetCode();
                new_cds.SetCode().SetId(bioseqGenCode);
                CSeq_feat_EditHandle edit_handle(*feat_ci);
                edit_handle.Replace(*new_feat);
                any_changed = true;
            }
        }
    }
    return any_changed;
}


// return position of " [" + sOrganism + "]", but only if it's
// at the end and there are characters before it.
// Also, returns the position of the organelle prefix in the title.
static SIZE_TYPE s_TitleEndsInOrganism(
    const string & sTitle,
    const string & sOrganism,
    SIZE_TYPE& OrganellePos)
{
    OrganellePos = NPOS;

    SIZE_TYPE answer = NPOS;

    const string sPattern = " [" + sOrganism + "]";
    if (NStr::EndsWith(sTitle, sPattern, NStr::eNocase)) {
        answer = sTitle.length() - sPattern.length();
        if (answer < 1) {
            // title must have something before the pattern
            answer = NPOS;
        }
    } else {
        answer = NStr::Find(sTitle, sPattern, NStr::eNocase, NStr::eReverseSearch);
        if (answer < 1 || answer == NPOS) {
            // pattern not found
            answer = NPOS;
        }
    }

    if (answer != NPOS) {
        // find organelle prefix
        for (unsigned int genome = CBioSource::eGenome_chloroplast;
            genome <= CBioSource::eGenome_chromatophore;
            genome++) {
            if (genome != CBioSource::eGenome_extrachrom &&
                genome != CBioSource::eGenome_transposon &&
                genome != CBioSource::eGenome_insertion_seq &&
                genome != CBioSource::eGenome_proviral &&
                genome != CBioSource::eGenome_virion &&
                genome != CBioSource::eGenome_chromosome)
            {
                string organelle = " (" + CBioSource::GetOrganelleByGenome(genome) + ")";
                SIZE_TYPE possible_organelle_start_pos = NStr::Find(sTitle, organelle, NStr::eNocase, NStr::eReverseSearch);
                if (possible_organelle_start_pos != NPOS &&
                    NStr::EndsWith(CTempString(sTitle, 0, answer), organelle)) {
                    OrganellePos = possible_organelle_start_pos;
                    break;
                }

            }
        }
    }
    return answer;
}


static SIZE_TYPE s_TitleEndsInOrganism(
    const string & sTitle,
    const COrgName::TName& orgname,
    SIZE_TYPE &organelle_pos)
{
    SIZE_TYPE suffixPos = NPOS; // will point to " [${organism name}]" at end
    organelle_pos = NPOS;

    if (orgname.IsBinomial() &&
        orgname.GetBinomial().IsSetGenus() &&
        !NStr::IsBlank(orgname.GetBinomial().GetGenus()) &&
        orgname.GetBinomial().IsSetSpecies() &&
        !NStr::IsBlank(orgname.GetBinomial().GetSpecies())) {
        string binomial = orgname.GetBinomial().GetGenus() + " " + orgname.GetBinomial().GetSpecies();
        suffixPos = s_TitleEndsInOrganism(sTitle, binomial, organelle_pos);
    }
    return suffixPos;
}


bool IsCrossKingdom(const COrg_ref& org, string& first_kingdom, string& second_kingdom)
{
    bool is_cross_kingdom = false;
    first_kingdom = kEmptyStr;
    second_kingdom = kEmptyStr;
    if (org.IsSetOrgname() && org.GetOrgname().IsSetName() &&
        org.GetOrgname().GetName().IsPartial() &&
        org.GetOrgname().GetName().GetPartial().IsSet()) {
        ITERATE(CPartialOrgName::Tdata, it, org.GetOrgname().GetName().GetPartial().Get()) {
            const CTaxElement& te = **it;
            if (te.IsSetFixed_level() && te.GetFixed_level() == 0 &&
                te.IsSetLevel() &&
                NStr::EqualNocase(te.GetLevel(), "superkingdom") &&
                te.IsSetName() && !NStr::IsBlank(te.GetName())) {
                if (first_kingdom.empty()) {
                    first_kingdom = te.GetName();
                } else if (!NStr::EqualNocase(first_kingdom, te.GetName())) {
                    is_cross_kingdom = true;
                    second_kingdom = te.GetName();
                    break;
                }
            }
        }
    }
    return is_cross_kingdom;
}


bool IsCrossKingdom(const COrg_ref& org)
{
    string first_kingdom, second_kingdom;
    return IsCrossKingdom(org, first_kingdom, second_kingdom);
}


static SIZE_TYPE s_TitleEndsInOrganism(
    const string & sTitle,
    const COrg_ref& org,
    SIZE_TYPE &organelle_pos)
{
    SIZE_TYPE suffixPos = NPOS; // will point to " [${organism name}]" at end
    organelle_pos = NPOS;

    // first, check to see if protein title matches old-name
    if (org.IsSetOrgMod()) {
        ITERATE(COrgName::TMod, it, org.GetOrgname().GetMod()) {
            if ((*it)->IsSetSubtype() && (*it)->IsSetSubname() &&
                (*it)->GetSubtype() == COrgMod::eSubtype_old_name &&
                !NStr::IsBlank((*it)->GetSubname())) {
                suffixPos = s_TitleEndsInOrganism(sTitle, (*it)->GetSubname(), organelle_pos);
                if (suffixPos != NPOS) {
                    return suffixPos;
                }
            }
        }
    }

    // next, check to see if protein title matches taxname
    if (org.IsSetTaxname() && !NStr::IsBlank(org.GetTaxname())) {
        suffixPos = s_TitleEndsInOrganism(sTitle, org.GetTaxname(), organelle_pos);
        if (suffixPos != NPOS) {
            return suffixPos;
        }
    }

    // try binomial if preset
    if (org.IsSetOrgname() && org.GetOrgname().IsSetName() &&
        org.GetOrgname().GetName().IsBinomial()) {
        suffixPos = s_TitleEndsInOrganism(sTitle, org.GetOrgname().GetName(), organelle_pos);
        if (suffixPos != NPOS) {
            return suffixPos;
        }
    }

    // cross-kingdom?
    if (IsCrossKingdom(org)) {
        SIZE_TYPE sep = NStr::Find(sTitle, "][");
        if (sep != string::npos) {
            suffixPos = s_TitleEndsInOrganism(sTitle.substr(0, sep + 1), org.GetTaxname(), organelle_pos);
        }
    }
    return suffixPos;
}

static bool s_RemoveSuffixFromNames(const string& suffix, list<string>& names)
{
    if (names.empty()) {
        return false;
    }

    auto suffix_len = suffix.length();
    bool change_made{ false };
    for (auto& name : names) {
        if (auto len = name.length(); (len >= 5) && NStr::EndsWith(name, suffix)) {
            name.erase(len - suffix_len); // remove the suffix
            Asn2gnbkCompressSpaces(name);
            change_made = true;
        }
    }
    return change_made;
}

static bool s_RemoveTaxnameFromProtRef(const string& taxname, CBioseq_Handle bsh)
{
    if (! bsh.HasAnnots()) {
        return false;
    }

    if (NStr::IsBlank(taxname) || taxname.starts_with("NAD"sv)) { // Exclude NAD
        return false;
    }

    const string taxname_in_brackets{ '[' + taxname + ']' };

    bool change_made{ false };

    // Consider also the case where bsh doesn't have a parent entry
    CSeq_annot_CI annot_it(bsh.GetParentEntry(), CSeq_annot_CI::eSearch_entry);
    for(; annot_it; ++annot_it) {
        if (! annot_it->IsFtable()) {
            continue;
        }
        SAnnotSelector sel;
        sel.SetFeatType(CSeqFeatData::e_Prot);
        CFeat_CI feat_it(*annot_it, sel);
        for (; feat_it; ++feat_it) {
            if (feat_it->GetOriginalFeature().GetData().GetProt().IsSetName()) {
                auto temp_names = feat_it->GetOriginalFeature().GetData().GetProt().GetName();
                if (s_RemoveSuffixFromNames(taxname_in_brackets, temp_names)) {
                    change_made = true;
                    auto pEditedFeat = Ref(new CSeq_feat());
                    pEditedFeat->Assign(feat_it->GetOriginalFeature());
                    pEditedFeat->SetData().SetProt().SetName() = move(temp_names);
                    CSeq_feat_EditHandle(*feat_it).Replace(*pEditedFeat);
                }
            }
        }
    }

    return change_made;
}


static bool s_IsSwissProt(const CBioseq_Handle& bsh) {
    if (bsh.IsSetId()) {    
        for (auto id_handle : bsh.GetId()) {
            auto pId = id_handle.GetSeqId();
            if (pId && (pId->Which() == CSeq_id::e_Swissprot)) {
                return true;
            }
        }
    }
    return false;
}


static bool s_IsPartial(const CMolInfo& molInfo)
{
    if (! molInfo.IsSetCompleteness()) {
        return false;
    }

    using enum CMolInfo::ECompleteness;
    switch (molInfo.GetCompleteness()) {
    case eCompleteness_partial:
    case eCompleteness_no_left:
    case eCompleteness_no_right:
    case eCompleteness_no_ends:
        return true;
    default:
        break;
    }

    return false;
}


static void s_GetMolInfoAndSrcDescriptors(const CSeq_descr&    seq_descr,
                                          CConstRef<CSeqdesc>& pMolinfoDesc,
                                          CConstRef<CSeqdesc>& pSrcDesc)
{
    if ((! seq_descr.IsSet()) || (pMolinfoDesc && pSrcDesc)) {
        return;
    }

    for (auto pDesc : seq_descr.Get()) {
        if (! pDesc) {
            continue;
        }
        if ((! pMolinfoDesc) && pDesc->IsMolinfo()) {
            pMolinfoDesc = pDesc;
            if (pSrcDesc) {
                return;
            }
        } else if ((! pSrcDesc) && pDesc->IsSource()) {
            pSrcDesc = pDesc;
            if (pMolinfoDesc) {
                return;
            }
        }
    }
}


static auto s_GetMolInfoAndSrcDescriptors(const CBioseq_Handle& bsh)
{
    CConstRef<CSeqdesc> pMolinfoDesc;
    CConstRef<CSeqdesc> pSrcDesc;

    if (bsh.IsSetDescr()) {
        s_GetMolInfoAndSrcDescriptors(bsh.GetDescr(), pMolinfoDesc, pSrcDesc);
    }

    if ((! pMolinfoDesc) || (! pSrcDesc)) {
        auto pParentSet = bsh.GetParentBioseq_set();
        if (pParentSet && pParentSet.IsSetDescr()) {
            s_GetMolInfoAndSrcDescriptors(pParentSet.GetDescr(),
                                          pMolinfoDesc,
                                          pSrcDesc);
        }
    }

    return make_pair(pMolinfoDesc, pSrcDesc);
}


static void s_UpdateTitleString(const CBioSource& src, bool bPartial, string& sTitle)
{
    // search for partial, must be just before bracketed organism
    SIZE_TYPE partialPos = NStr::Find(sTitle, ", partial [");
    if (partialPos == NPOS) {
        partialPos = NStr::Find(sTitle, ", partial (");
    }

    // find oldname or taxname in brackets at end of protein title
    SIZE_TYPE penult = NPOS;
    SIZE_TYPE suffixPos = s_TitleEndsInOrganism(sTitle, src.GetOrg(), penult); // will point to " [${organism name}]" at end
    // do not change unless [genus species] was at the end
    if (suffixPos == NPOS) {
        return;
    }

    // truncate bracketed info from end of title, will replace with current taxname
    sTitle.resize(suffixPos);
    if (penult != NPOS) {
        sTitle.resize(penult);
    }

    // if ", partial [" was indeed just before the [genus species], it will now be ", partial"
    // Note: 9 is length of ", partial"
    if (!bPartial  &&
        partialPos != string::npos &&
        (partialPos == (sTitle.length() - 9)))
    {
        sTitle.resize(partialPos);
    }
    NStr::TruncateSpacesInPlace(sTitle);

    //
    if (bPartial && partialPos == NPOS) {
        sTitle += ", partial";
    }


    const auto genome = (src.IsSetGenome() ? src.GetGenome() : CBioSource::eGenome_unknown);

    using enum CBioSource::EGenome;
    if (genome >= eGenome_chloroplast &&
        genome <= eGenome_chromatophore &&
        genome != eGenome_extrachrom &&
        genome != eGenome_transposon &&
        genome != eGenome_insertion_seq &&
        genome != eGenome_proviral &&
        genome != eGenome_virion &&
        genome != eGenome_chromosome) {
        auto organelle = CBioSource::GetOrganelleByGenome(genome);
        if (! NStr::IsBlank(organelle)) {
            sTitle += " (" + string(organelle) + ")";
        }
    }

    const auto& org = src.GetOrg();
    string first_kingdom, second_kingdom;
    if (IsCrossKingdom(org, first_kingdom, second_kingdom)) {
        sTitle += " [" + first_kingdom + "][" + second_kingdom + "]";
    } else {
        sTitle += " [";
        if (org.IsSetTaxname()) {
            sTitle += org.GetTaxname();
        }
        sTitle += "]";
    }
}

// Fetch a CConstRef to the bioseq title descriptor
static CConstRef<CSeqdesc> s_GetTitleDesc(const CSeq_descr& descr)
{
    // Returns the last title descriptor - is that intentional?
    const auto& desc_list = descr.Get();
    for (auto it = desc_list.rbegin(); it != desc_list.rend(); ++it) {
        if ((*it)->IsTitle()) {
            return *it;
        }
    }
    return CConstRef<CSeqdesc>();
}


bool CCleanup::AddPartialToProteinTitle(CBioseq_Handle bsh)
{
    // Bail if not protein
    if (! bsh.IsSetInst_Mol() || ! (bsh.GetInst_Mol() == CSeq_inst::eMol_aa) ) {
        return false;
    }

    // Bail if record is swissprot
    if (s_IsSwissProt(bsh)) {
        return false;
    }

    // gather some info from the Seqdesc's on the bioseq, into
    // the following variables
    CConstRef<CSeqdesc> molinfo_desc;
    CConstRef<CSeqdesc> src_desc;
    // Look for molinfo and src descriptors, first on the bioseq, then
    // on the parent set
    auto                molinfo_and_src = s_GetMolInfoAndSrcDescriptors(bsh);
    molinfo_desc                        = molinfo_and_src.first;
    src_desc                            = molinfo_and_src.second;
    if (! src_desc || (! src_desc->GetSource().IsSetOrg())) {
        return false;
    }
    const auto& source = src_desc->GetSource();
    const auto& org    = source.GetOrg();


    if (org.IsSetTaxname()) {
        s_RemoveTaxnameFromProtRef(org.GetTaxname(), bsh); // Note: this may modify Prot-ref names
    }

    // find the title to edit
    if (! bsh.IsSetDescr()) {
        return false;
    }

    CConstRef<CSeqdesc> title_desc = s_GetTitleDesc(bsh.GetDescr());
    if (! title_desc) {
        return false;
    }

    string title = title_desc->GetTitle();
    const bool   bPartial       = (molinfo_desc && s_IsPartial(molinfo_desc->GetMolinfo()));
    s_UpdateTitleString(source, bPartial, title);

    // Check for change to title string
    if (title != title_desc->GetTitle()) {
        auto pNewTitleDesc = Ref(new CSeqdesc());
        pNewTitleDesc->SetTitle() = move(title);
        bsh.GetEditHandle().ReplaceSeqdesc(*title_desc, *pNewTitleDesc);
        return true;
    }

    return false;
}


bool CCleanup::RemovePseudoProduct(CSeq_feat& cds, CScope& scope)
{
    if (! sequence::IsPseudo(cds, scope) ||
        ! cds.IsSetData() || ! cds.GetData().IsCdregion() ||
        ! cds.IsSetProduct()) {
        return false;
    }
    CBioseq_Handle pseq = scope.GetBioseqHandle(cds.GetProduct());
    if (pseq) {
        CFeat_CI prot(pseq, CSeqFeatData::eSubtype_prot);
        if (prot) {
            string label;
            if (prot->GetData().GetProt().IsSetName() &&
                ! prot->GetData().GetProt().GetName().empty()) {
                label = prot->GetData().GetProt().GetName().front();
            } else if (prot->GetData().GetProt().IsSetDesc()) {
                label = prot->GetData().GetProt().GetDesc();
            }
            if (! NStr::IsBlank(label)) {
                if (cds.IsSetComment() && ! NStr::IsBlank(cds.GetComment())) {
                    cds.SetComment(cds.GetComment() + "; " + label);
                } else {
                    cds.SetComment(label);
                }
            }
        }
        CBioseq_EditHandle pseq_e(pseq);
        pseq_e.Remove();
    }
    cds.ResetProduct();
    return true;
}


bool CCleanup::ExpandGeneToIncludeChildren(CSeq_feat& gene, const CTSE_Handle& tse)
{
    if (!gene.IsSetXref() || !gene.IsSetLocation() || !gene.GetLocation().IsInt()) {
        return false;
    }
    bool any_change = false;
    TSeqPos gene_start = gene.GetLocation().GetStart(eExtreme_Positional);
    TSeqPos gene_stop = gene.GetLocation().GetStop(eExtreme_Positional);
    for (const auto& pXref : gene.GetXref()) {
        if (pXref->IsSetId() && pXref->GetId().IsLocal()) {
            const auto& feat_id = pXref->GetId().GetLocal();
            auto far_feats = tse.GetFeaturesWithId(CSeqFeatData::eSubtype_any, feat_id);
            for (auto fhandle : far_feats) {
                auto f_start = fhandle.GetLocation().GetStart(eExtreme_Positional);
                auto f_stop = fhandle.GetLocation().GetStop(eExtreme_Positional);
                if (f_start < gene_start) {
                    gene.SetLocation().SetInt().SetFrom(f_start);
                    gene_start = f_start;
                    any_change = true;
                }
                if (f_stop > gene_stop) {
                    gene.SetLocation().SetInt().SetTo(f_stop);
                    gene_stop = f_stop;
                    any_change = true;
                }
            }
        }
    }
    return any_change;
}


typedef pair<size_t, bool> TRNALength;
typedef map<string, TRNALength > TRNALengthMap;

static const TRNALengthMap kTrnaLengthMap{
    { "16S", { 1000, false } },
    { "18S", { 1000, false } },
    { "23S", { 2000, false } },
    { "25S", { 1000, false } },
    { "26S", { 1000, false } },
    { "28S", { 3300, false } },
    { "small", { 1000, false } },
    { "large", { 1000, false } },
    { "5.8S", { 130, true } },
    { "5S", { 90, true } }
    // possible problem: if it matches /25S/ it would also match /5S/
    // luckily, if it fails the /5S/ rule it would fail the /25S/ rule
};


static bool s_CleanupIsShortrRNA(const CSeq_feat& f, CScope* scope) // used in feature_tests.cpp
{
    if (f.GetData().GetSubtype() != CSeqFeatData::eSubtype_rRNA) {
        return false;
    }
    bool is_bad = false;
    size_t len = sequence::GetLength(f.GetLocation(), scope);
    const CRNA_ref& rrna = f.GetData().GetRna();
    string rrna_name = rrna.GetRnaProductName();
    if (rrna_name.empty()) {
        // RNA name may still be in product GBQual
        if (f.IsSetQual()) {
            for (auto qit : f.GetQual()) {
                const CGb_qual& gbq = *qit;
                if ( gbq.IsSetQual() && gbq.GetQual() == "product" ) {
                    rrna_name = gbq.GetVal();
                    break;
                }
            }
        }
    }
    ITERATE (TRNALengthMap, it, kTrnaLengthMap) {
        SIZE_TYPE pos = NStr::FindNoCase(rrna_name, it->first);
        if (pos != string::npos && len < it->second.first && !(it->second.second && f.IsSetPartial() && f.GetPartial()) ) {
            is_bad = true;
            break;
        }
    }
    return is_bad;
}

bool CCleanup::WGSCleanup(CSeq_entry_Handle entry, bool instantiate_missing_proteins, Uint4 options, bool run_extended_cleanup, bool also_fix_location)
{
    bool any_changes = false;

    int protein_id_counter = 1;
    bool create_general_only = edit::IsGeneralIdProtPresent(entry.GetTopLevelEntry());
    SAnnotSelector sel(CSeqFeatData::e_Cdregion);

    for (CFeat_CI cds_it(entry, sel); cds_it; ++cds_it) {
        bool change_this_cds = false;
        CRef<CSeq_feat> new_cds(new CSeq_feat());
        new_cds->Assign(*(cds_it->GetSeq_feat()));
        if (sequence::IsPseudo(*(cds_it->GetSeq_feat()), entry.GetScope())) {
            change_this_cds = RemovePseudoProduct(*new_cds, entry.GetScope());
        } else {
            string current_name = GetProteinName(*new_cds, entry);

            change_this_cds |= SetBestFrame(*new_cds, entry.GetScope());
            change_this_cds |= SetCDSPartialsByFrameAndTranslation(*new_cds, entry.GetScope());

            // retranslate
            if (new_cds->IsSetProduct() && entry.GetScope().GetBioseqHandleFromTSE(*(new_cds->GetProduct().GetId()), entry)) {
                any_changes |= feature::RetranslateCDS(*new_cds, entry.GetScope());
            } else {
                // need to set product if not set
                if (!new_cds->IsSetProduct() && !sequence::IsPseudo(*new_cds, entry.GetScope())) {
                    string id_label;
                    CRef<CSeq_id> new_id = edit::GetNewProtId(entry.GetScope().GetBioseqHandle(new_cds->GetLocation()), protein_id_counter, id_label, create_general_only);
                    if (new_id) {
                        new_cds->SetProduct().SetWhole().Assign(*new_id);
                        change_this_cds = true;
                    }
                }
                if (new_cds->IsSetProduct() && instantiate_missing_proteins) {
                    CRef<CSeq_entry> prot = AddProtein(*new_cds, entry.GetScope());
                    if (prot) {
                        any_changes = true;
                    }
                }
                any_changes |= feature::AdjustForCDSPartials(*new_cds, entry);
            }
            //prefer ncbieaa
            if (new_cds->IsSetProduct()) {
                CBioseq_Handle p = entry.GetScope().GetBioseqHandle(new_cds->GetProduct());
                if (p && p.IsSetInst() && p.GetInst().IsSetSeq_data() && p.GetInst().GetSeq_data().IsIupacaa()) {
                    CBioseq_EditHandle peh(p);
                    string current = p.GetInst().GetSeq_data().GetIupacaa().Get();
                    CRef<CSeq_inst> new_inst(new CSeq_inst());
                    new_inst->Assign(p.GetInst());
                    new_inst->SetSeq_data().SetNcbieaa().Set(current);
                    peh.SetInst(*new_inst);
                    any_changes = true;
                }
            }

            if (NStr::IsBlank(current_name)) {
                SetProteinName(*new_cds, "hypothetical protein", false, entry.GetScope());
                current_name = "hypothetical protein";
                change_this_cds = true;
            } else if (new_cds->IsSetProduct()) {
                CBioseq_Handle p = entry.GetScope().GetBioseqHandle(new_cds->GetProduct());
                if (p) {
                    CFeat_CI feat_ci(p, CSeqFeatData::eSubtype_prot);
                    if (!feat_ci) {
                        // make new protein feature
                        feature::AddProteinFeature(*(p.GetCompleteBioseq()), current_name, *new_cds, entry.GetScope());
                    }
                }
            }

            CConstRef<CSeq_feat> mrna = sequence::GetmRNAforCDS(*(cds_it->GetSeq_feat()), entry.GetScope());
            if (mrna) {
                bool change_mrna = false;
                CRef<CSeq_feat> new_mrna(new CSeq_feat());
                new_mrna->Assign(*mrna);
                // Make mRNA name match coding region protein
                string mrna_name = new_mrna->GetData().GetRna().GetRnaProductName();
                if (NStr::IsBlank(mrna_name) && new_mrna->IsSetQual()) {
                    for (auto it = new_mrna->GetQual().begin(); it != new_mrna->GetQual().end(); it++) {
                        if ((*it)->IsSetQual() && (*it)->IsSetVal() && NStr::EqualNocase((*it)->GetQual(), "product")) {
                            mrna_name = (*it)->GetVal();
                            break;
                        }
                    }
                }
                if (NStr::IsBlank(mrna_name)
                    || (!NStr::Equal(current_name, "hypothetical protein") &&
                    !NStr::Equal(current_name, mrna_name))) {
                    SetMrnaName(*new_mrna, current_name);
                    change_mrna = true;
                }
                // Adjust mRNA partials to match coding region
                change_mrna |= feature::CopyFeaturePartials(*new_mrna, *new_cds);
                if (change_mrna) {
                    CSeq_feat_Handle fh = entry.GetScope().GetSeq_featHandle(*mrna);
                    CSeq_feat_EditHandle feh(fh);
                    feh.Replace(*new_mrna);
                    any_changes = true;
                }
            }
        }

        //any_changes |= feature::RetranslateCDS(*new_cds, entry.GetScope());
        if (change_this_cds) {
            CSeq_feat_EditHandle cds_h(*cds_it);

            cds_h.Replace(*new_cds);
            any_changes = true;

            //also need to redo protein title
        }
    }


    for (CFeat_CI rna_it(entry, SAnnotSelector(CSeqFeatData::e_Rna)); rna_it; ++rna_it) {

        const CSeq_feat& rna_feat = *(rna_it->GetSeq_feat());
        if (rna_feat.IsSetData() &&
            rna_feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_rRNA &&
            s_CleanupIsShortrRNA(rna_feat, &(entry.GetScope()))) {

            bool change_this_rrna = false;
            CRef<CSeq_feat> new_rrna(new CSeq_feat());
            new_rrna->Assign(*(rna_it->GetSeq_feat()));

            const CSeq_loc& loc = rna_feat.GetLocation();
            CBioseq_Handle r = entry.GetScope().GetBioseqHandle(loc);
            if (r && loc.IsSetStrand() && loc.GetStrand() == eNa_strand_minus) {
                if (loc.GetStart(eExtreme_Biological) >= r.GetBioseqLength()) {
                    new_rrna->SetLocation().SetPartialStart(true, eExtreme_Biological);
                    change_this_rrna = true;
                }
                if (loc.GetStop(eExtreme_Biological) < 1) {
                    new_rrna->SetLocation().SetPartialStop(true, eExtreme_Biological);
                    change_this_rrna = true;
                }
            } else if (r) {
                if (loc.GetStart(eExtreme_Biological) < 1) {
                    new_rrna->SetLocation().SetPartialStart(true, eExtreme_Biological);
                    change_this_rrna = true;
                }
                if (loc.GetStop(eExtreme_Biological) >= r.GetBioseqLength()) {
                    new_rrna->SetLocation().SetPartialStop(true, eExtreme_Biological);
                    change_this_rrna = true;
                }
            }

            if (change_this_rrna) {
                CSeq_feat_EditHandle rrna_h(*rna_it);
                rrna_h.Replace(*new_rrna);
                any_changes = true;
            }
        }
    }

    CTSE_Handle tse = entry.GetTSE_Handle();
    for (CFeat_CI gene_it(entry, SAnnotSelector(CSeqFeatData::e_Gene)); gene_it; ++gene_it) {
        CRef<CSeq_feat> new_gene(new CSeq_feat());
        new_gene->Assign(*(gene_it->GetSeq_feat()));

        if (ExpandGeneToIncludeChildren(*new_gene, tse)) {
            CSeq_feat_EditHandle gene_h(*gene_it);
            gene_h.Replace(*new_gene);
            any_changes = true;
        }
    }

    any_changes |= s_SetGenePartialsFromChildren(entry);

    NormalizeDescriptorOrder(entry);

    for (CBioseq_CI bi(entry, CSeq_inst::eMol_na); bi; ++bi) {
        any_changes |= SetGeneticCodes(*bi);
    }

    if (run_extended_cleanup) {
        auto pChanged = CCleanup::ExtendedCleanup(entry, options);
        if (pChanged->ChangeCount()>0) {
            return true;
        }
    }
    return any_changes;
}


bool CCleanup::x_HasShortIntron(const CSeq_loc& loc, size_t min_len)
{
    CSeq_loc_CI li(loc);
    while (li && li.IsEmpty()) {
        ++li;
    }
    if (!li) {
        return false;
    }
    while (li) {
        TSeqPos prev_end;
        ENa_strand prev_strand;
        if (li.IsSetStrand() && li.GetStrand() == eNa_strand_minus) {
            prev_end = li.GetRange().GetFrom();
            prev_strand = eNa_strand_minus;
        } else {
            prev_end = li.GetRange().GetTo();
            prev_strand = eNa_strand_plus;
        }
        ++li;
        while (li && li.IsEmpty()) {
            ++li;
        }
        if (li) {
            TSeqPos this_start;
            ENa_strand this_strand;
            if (li.IsSetStrand() && li.GetStrand() == eNa_strand_minus) {
                this_start = li.GetRange().GetTo();
                this_strand = eNa_strand_minus;
            } else {
                this_start = li.GetRange().GetFrom();
                this_strand = eNa_strand_plus;
            }
            if (this_strand == prev_strand) {
                if (abs((long int)this_start - (long int)prev_end) < min_len) {
                    return true;
                }
            }
        }
    }
    return false;
}

//LCOV_EXCL_START
//not used by asn_cleanup but used by table2asn
const string kLowQualitySequence = "low-quality sequence region";

bool CCleanup::x_AddLowQualityException(CSeq_feat& feat)
{
    bool any_change = false;
    if (!feat.IsSetExcept()) {
        any_change = true;
        feat.SetExcept(true);
    }
    if (!feat.IsSetExcept_text() || NStr::IsBlank(feat.GetExcept_text())) {
        feat.SetExcept_text(kLowQualitySequence);
        any_change = true;
    } else if (NStr::Find(feat.GetExcept_text(), kLowQualitySequence) == string::npos) {
        feat.SetExcept_text(feat.GetExcept_text() + "; " + kLowQualitySequence);
        any_change = true;
    }
    return any_change;
}


bool CCleanup::x_AddLowQualityException(CSeq_entry_Handle entry, CSeqFeatData::ESubtype subtype)
{
    bool any_changes = false;

    SAnnotSelector sel(subtype);
    for (CFeat_CI cds_it(entry, sel); cds_it; ++cds_it) {
        bool change_this_cds = false;
        CRef<CSeq_feat> new_cds(new CSeq_feat());
        new_cds->Assign(*(cds_it->GetSeq_feat()));
        if (!sequence::IsPseudo(*(cds_it->GetSeq_feat()), entry.GetScope()) &&
            x_HasShortIntron(cds_it->GetLocation())) {
            change_this_cds = x_AddLowQualityException(*new_cds);
        }

        if (change_this_cds) {
            CSeq_feat_EditHandle cds_h(*cds_it);

            cds_h.Replace(*new_cds);
            any_changes = true;
        }
    }
    return any_changes;
}


bool CCleanup::AddLowQualityException(CSeq_entry_Handle entry)
{
    bool any_changes = x_AddLowQualityException(entry, CSeqFeatData::eSubtype_cdregion);
    any_changes |= x_AddLowQualityException(entry, CSeqFeatData::eSubtype_mRNA);
    return any_changes;
}
//LCOV_EXCL_STOP


// maps the type of seqdesc to the order it should be in
// (lowest to highest)
typedef SStaticPair<CSeqdesc::E_Choice, int>  TSeqdescOrderElem;
static const TSeqdescOrderElem sc_seqdesc_order_map[] = {
    // Note that ordering must match ordering
    // in CSeqdesc::E_Choice
        { CSeqdesc::e_Mol_type, 13 },
        { CSeqdesc::e_Modif, 14 },
        { CSeqdesc::e_Method, 15 },
        { CSeqdesc::e_Name, 7 },
        { CSeqdesc::e_Title, 1 },
        { CSeqdesc::e_Org, 16 },
        { CSeqdesc::e_Comment, 6 },
        { CSeqdesc::e_Num, 11 },
        { CSeqdesc::e_Maploc, 9 },
        { CSeqdesc::e_Pir, 18 },
        { CSeqdesc::e_Genbank, 22 },
        { CSeqdesc::e_Pub, 5 },
        { CSeqdesc::e_Region, 10 },
        { CSeqdesc::e_User, 8 },
        { CSeqdesc::e_Sp, 17 },
        { CSeqdesc::e_Dbxref, 12 },
        { CSeqdesc::e_Embl, 21 },
        { CSeqdesc::e_Create_date, 24 },
        { CSeqdesc::e_Update_date, 25 },
        { CSeqdesc::e_Prf, 19 },
        { CSeqdesc::e_Pdb, 20 },
        { CSeqdesc::e_Het, 4 },

        { CSeqdesc::e_Source, 2 },
        { CSeqdesc::e_Molinfo, 3 },
        { CSeqdesc::e_Modelev, 23 }
};
typedef CStaticPairArrayMap<CSeqdesc::E_Choice, int> TSeqdescOrderMap;
DEFINE_STATIC_ARRAY_MAP(TSeqdescOrderMap, sc_SeqdescOrderMap, sc_seqdesc_order_map);

static
int s_SeqDescToOrdering(CSeqdesc::E_Choice chs) {
    // ordering assigned to unknown
    const int unknown_seqdesc = static_cast<int>(1 + sc_SeqdescOrderMap.size());

    TSeqdescOrderMap::const_iterator find_iter = sc_SeqdescOrderMap.find(chs);
    if (find_iter == sc_SeqdescOrderMap.end()) {
        return unknown_seqdesc;
    }

    return find_iter->second;
}

static
bool s_SeqDescLessThan(const CRef<CSeqdesc> &desc1, const CRef<CSeqdesc> &desc2)
{
    CSeqdesc::E_Choice chs1, chs2;

    chs1 = desc1->Which();
    chs2 = desc2->Which();

    return (s_SeqDescToOrdering(chs1) < s_SeqDescToOrdering(chs2));
}

bool CCleanup::NormalizeDescriptorOrder(CSeq_descr& descr)
{
    bool rval = false;
    if (!seq_mac_is_sorted(descr.Set().begin(), descr.Set().end(), s_SeqDescLessThan)) {
        descr.Set().sort(s_SeqDescLessThan);
        rval = true;
    }
    return rval;
}

bool CCleanup::NormalizeDescriptorOrder(CSeq_entry_Handle seh)
{
    bool rval = false;

    CSeq_entry_CI ci(seh, CSeq_entry_CI::fRecursive | CSeq_entry_CI::fIncludeGivenEntry);
    while (ci) {
        CSeq_entry_EditHandle edit(*ci);
        if (edit.IsSetDescr()) {
            rval |= NormalizeDescriptorOrder(edit.SetDescr());
        }
        ++ci;
    }

    return rval;
}


bool CCleanup::RemoveUnseenTitles(CSeq_entry_EditHandle::TSeq seq)
{
    bool removed = false;
    if (seq.IsSetDescr()) {
        CConstRef<CSeqdesc> last_title;
        ITERATE(CBioseq::TDescr::Tdata, d, seq.GetDescr().Get()) {
            if ((*d)->IsTitle()) {
                if (last_title) {
                    seq.RemoveSeqdesc(*last_title);
                    removed = true;
                }
                last_title.Reset(d->GetPointer());
            }
        }
    }
    return removed;
}


bool CCleanup::RemoveUnseenTitles(CSeq_entry_EditHandle::TSet set)
{
    bool removed = false;
    if (set.IsSetDescr()) {
        CConstRef<CSeqdesc> last_title;
        ITERATE(CBioseq::TDescr::Tdata, d, set.GetDescr().Get()) {
            if ((*d)->IsTitle()) {
                if (last_title) {
                    set.RemoveSeqdesc(*last_title);
                    removed = true;
                }
                last_title.Reset(d->GetPointer());
            }
        }
    }
    return removed;
}


bool CCleanup::AddGenBankWrapper(CSeq_entry_Handle seh)
{
    if (seh.IsSet() && seh.GetSet().IsSetClass() &&
        seh.GetSet().GetClass() == CBioseq_set::eClass_genbank) {
        return false;
    }
    CSeq_entry_EditHandle eh(seh);
    eh.ConvertSeqToSet(CBioseq_set::eClass_genbank);
    return true;
}


void s_GetAuthorsString(string *out_authors, const CAuth_list& auth_list)
{
    string & auth_str = *out_authors;
    auth_str.clear();

    if (!auth_list.IsSetNames()) {
        return;
    }

    vector<string> name_list;

    if (auth_list.GetNames().IsStd()) {
        ITERATE(CAuth_list::TNames::TStd, auth_it, auth_list.GetNames().GetStd()) {
            if ((*auth_it)->IsSetName()) {
                string label;
                (*auth_it)->GetName().GetLabel(&label);
                name_list.push_back(label);
            }
        }
    } else if (auth_list.GetNames().IsMl()) {
        copy(BEGIN_COMMA_END(auth_list.GetNames().GetMl()),
            back_inserter(name_list));
    } else if (auth_list.GetNames().IsStr()) {
        copy(BEGIN_COMMA_END(auth_list.GetNames().GetStr()),
            back_inserter(name_list));
    }

    if (name_list.size() == 0) {
        return;
    } else if (name_list.size() == 1) {
        auth_str = name_list.back();
        return;
    }

    // join most of them by commas, but the last one gets an "and"
    string last_author;
    last_author.swap(name_list.back());
    name_list.pop_back();
    // swap is faster than assignment
    NStr::Join(name_list, ", ").swap(auth_str);
    auth_str += "and ";
    auth_str += last_author;

    return;
}


void s_GetAuthorsString(
    string *out_authors_string, const CPubdesc& pd)
{
    string & authors_string = *out_authors_string;
    authors_string.clear();

    FOR_EACH_PUB_ON_PUBDESC(pub, pd) {
        if ((*pub)->IsSetAuthors()) {
            s_GetAuthorsString(&authors_string, (*pub)->GetAuthors());
            break;
        }
    }
}


void CCleanup::GetPubdescLabels
(const CPubdesc& pd,
vector<TEntrezId>& pmids, vector<TEntrezId>& muids, vector<int>& serials,
vector<string>& published_labels,
vector<string>& unpublished_labels)
{
    string label;
    bool   is_published = false;
    bool   need_label = false;

    if (!pd.IsSetPub()) {
        return;
    }
    ITERATE(CPubdesc::TPub::Tdata, it, pd.GetPub().Get()) {
        if ((*it)->IsPmid()) {
            pmids.push_back((*it)->GetPmid());
            is_published = true;
        } else if ((*it)->IsMuid()) {
            muids.push_back((*it)->GetMuid());
            is_published = true;
        } else if ((*it)->IsGen()) {
            if ((*it)->GetGen().IsSetCit()
                && NStr::StartsWith((*it)->GetGen().GetCit(), "BackBone id_pub", NStr::eNocase)) {
                need_label = true;
            }
            if ((*it)->GetGen().IsSetSerial_number()) {
                serials.push_back((*it)->GetGen().GetSerial_number());
                if ((*it)->GetGen().IsSetCit()
                    || (*it)->GetGen().IsSetJournal()
                    || (*it)->GetGen().IsSetDate()) {
                    need_label = true;
                }
            } else {
                need_label = true;
            }
        } else if ((*it)->IsArticle() && (*it)->GetArticle().IsSetIds()) {
            is_published = true;
            ITERATE(CArticleIdSet::Tdata, id, (*it)->GetArticle().GetIds().Get()) {
                if ((*id)->IsPubmed()) {
                    pmids.push_back((*id)->GetPubmed());
                    is_published = true;
                } else if ((*id)->IsMedline()) {
                    muids.push_back((*id)->GetMedline());
                }
            }
            need_label = true;
        } else {
            need_label = true;
        }
        if (need_label && NStr::IsBlank(label)) {
            // create unique label
            (*it)->GetLabel(&label, CPub::eContent, CPub::fLabel_Unique);
            string auth_str;
            s_GetAuthorsString(&auth_str, pd);
            label += "; ";
            label += auth_str;
        }
    }
    if (!NStr::IsBlank(label)) {
        if (is_published) {
            published_labels.push_back(label);
        } else {
            unpublished_labels.push_back(label);
        }
    }
}


vector<CConstRef<CPub> > CCleanup::GetCitationList(CBioseq_Handle bsh)
{
    vector<CConstRef<CPub> > pub_list;

    // first get descriptor pubs
    CSeqdesc_CI di(bsh, CSeqdesc::e_Pub);
    while (di) {
        vector<TEntrezId> pmids;
        vector<TEntrezId> muids;
        vector<int> serials;
        vector<string> published_labels;
        vector<string> unpublished_labels;
        GetPubdescLabels(di->GetPub(), pmids, muids, serials, published_labels, unpublished_labels);
        if (pmids.size() > 0) {
            CRef<CPub> pub(new CPub());
            pub->SetPmid().Set(pmids[0]);
            pub_list.push_back(pub);
        } else if (muids.size() > 0) {
            CRef<CPub> pub(new CPub());
            pub->SetMuid(muids[0]);
            pub_list.push_back(pub);
        } else if (serials.size() > 0) {
            CRef<CPub> pub(new CPub());
            pub->SetGen().SetSerial_number(serials[0]);
            pub_list.push_back(pub);
        } else if (published_labels.size() > 0) {
            CRef<CPub> pub(new CPub());
            pub->SetGen().SetCit(published_labels[0]);
            pub_list.push_back(pub);
        } else if (unpublished_labels.size() > 0) {
            CRef<CPub> pub(new CPub());
            pub->SetGen().SetCit(unpublished_labels[0]);
            pub_list.push_back(pub);
        }

        ++di;
    }
    // now get pub features
    CFeat_CI fi(bsh, SAnnotSelector(CSeqFeatData::e_Pub));
    while (fi) {
        vector<TEntrezId> pmids;
        vector<TEntrezId> muids;
        vector<int> serials;
        vector<string> published_labels;
        vector<string> unpublished_labels;
        GetPubdescLabels(fi->GetData().GetPub(), pmids, muids, serials, published_labels, unpublished_labels);
        if (pmids.size() > 0) {
            CRef<CPub> pub(new CPub());
            pub->SetPmid().Set(pmids[0]);
            pub_list.push_back(pub);
        } else if (muids.size() > 0) {
            CRef<CPub> pub(new CPub());
            pub->SetMuid(muids[0]);
            pub_list.push_back(pub);
        } else if (serials.size() > 0) {
            CRef<CPub> pub(new CPub());
            pub->SetGen().SetSerial_number(serials[0]);
            pub_list.push_back(pub);
        } else if (published_labels.size() > 0) {
            CRef<CPub> pub(new CPub());
            pub->SetGen().SetCit(published_labels[0]);
            pub_list.push_back(pub);
        } else if (unpublished_labels.size() > 0) {
            CRef<CPub> pub(new CPub());
            pub->SetGen().SetCit(unpublished_labels[0]);
            pub_list.push_back(pub);
        }

        ++fi;
    }
    return pub_list;
}


bool CCleanup::RemoveDuplicatePubs(CSeq_descr& descr)
{
    bool any_change = false;
    CSeq_descr::Tdata::iterator it1 = descr.Set().begin();
    while (it1 != descr.Set().end()) {
        if ((*it1)->IsPub()) {
            CSeq_descr::Tdata::iterator it2 = it1;
            ++it2;
            while (it2 != descr.Set().end()) {
                if ((*it2)->IsPub() && (*it1)->GetPub().Equals((*it2)->GetPub())) {
                    it2 = descr.Set().erase(it2);
                    any_change = true;
                } else {
                    ++it2;
                }
            }
        }
        ++it1;
    }
    return any_change;
}


bool s_FirstPubMatchesSecond(const CPubdesc& pd1, const CPubdesc& pd2)
{
    if (pd1.Equals(pd2)) {
        return true;
    } else if (pd1.IsSetPub() && pd2.IsSetPub() && pd1.GetPub().Get().size() == 1) {
        ITERATE(CPubdesc::TPub::Tdata, it, pd2.GetPub().Get()) {
            if (pd1.GetPub().Get().front()->Equals(**it)) {
                return true;
            }
        }
    }
    return false;
}


bool CCleanup::PubAlreadyInSet(const CPubdesc& pd, const CSeq_descr& descr)
{
    ITERATE(CSeq_descr::Tdata, d, descr.Get()) {
        if ((*d)->IsPub() && s_FirstPubMatchesSecond(pd, (*d)->GetPub())) {
            return true;
        }
    }
    return false;
}


bool CCleanup::OkToPromoteNpPub(const CBioseq& b)
{
    bool is_embl_or_ddbj = false;
    ITERATE(CBioseq::TId, id, b.GetId()) {
        if ((*id)->IsEmbl() || (*id)->IsDdbj()) {
            is_embl_or_ddbj = true;
            break;
        }
    }
    return !is_embl_or_ddbj;
}


bool CCleanup::OkToPromoteNpPub(const CPubdesc& pd)
{
    if (pd.IsSetNum() || pd.IsSetName() || pd.IsSetFig() || pd.IsSetComment()) {
        return false;
    } else {
        return true;
    }
}


void CCleanup::MoveOneFeatToPubdesc(CSeq_feat_Handle feat, CRef<CSeqdesc> d, CBioseq_Handle b, bool remove_feat)
{
    // add descriptor to nuc-prot parent or sequence itself
    CBioseq_set_Handle parent = b.GetParentBioseq_set();
    if (!CCleanup::OkToPromoteNpPub(*(b.GetCompleteBioseq()))) {
        // add to sequence
        CBioseq_EditHandle eh(b);
        eh.AddSeqdesc(*d);
        RemoveDuplicatePubs(eh.SetDescr());
        NormalizeDescriptorOrder(eh.SetDescr());
    } else if (parent && parent.IsSetClass() &&
        parent.GetClass() == CBioseq_set::eClass_nuc_prot &&
        parent.IsSetDescr() && PubAlreadyInSet(d->GetPub(), parent.GetDescr())) {
        // don't add descriptor, just delete feature
    } else if (OkToPromoteNpPub((d)->GetPub()) &&
        parent && parent.IsSetClass() &&
        parent.GetClass() == CBioseq_set::eClass_nuc_prot) {
        CBioseq_set_EditHandle eh(parent);
        eh.AddSeqdesc(*d);
        RemoveDuplicatePubs(eh.SetDescr());
        NormalizeDescriptorOrder(eh.SetDescr());
    } else {
        CBioseq_EditHandle eh(b);
        eh.AddSeqdesc(*d);
        RemoveDuplicatePubs(eh.SetDescr());
        NormalizeDescriptorOrder(eh.SetDescr());
    }
    if (remove_feat) {
        // remove feature
        CSeq_feat_EditHandle feh(feat);
        feh.Remove();
    }
}


bool CCleanup::ConvertPubFeatsToPubDescs(CSeq_entry_Handle seh)
{
    bool any_change = false;
    for (CBioseq_CI b(seh); b; ++b) {
        for (CFeat_CI p(*b, CSeqFeatData::e_Pub); p; ++p) {
            if (p->GetLocation().IsInt() &&
                p->GetLocation().GetStart(eExtreme_Biological) == 0 &&
                p->GetLocation().GetStop(eExtreme_Biological) == b->GetBioseqLength() - 1) {
                CRef<CSeqdesc> d(new CSeqdesc());
                d->SetPub().Assign(p->GetData().GetPub());
                if (p->IsSetComment()) {
                    if (d->GetPub().IsSetComment() && !NStr::IsBlank(d->GetPub().GetComment())) {
                        d->SetPub().SetComment(d->GetPub().GetComment() + "; " + p->GetComment());
                    } else {
                        d->SetPub().SetComment();
                    }
                }
                MoveOneFeatToPubdesc(*p, d, *b);
                any_change = true;
            }
        }
    }
    return any_change;
}


bool IsSiteRef(const CSeq_feat& sf)
{
    if (sf.GetData().IsImp() &&
        sf.GetData().GetImp().IsSetKey() &&
        NStr::Equal(sf.GetData().GetImp().GetKey(), "Site-ref")) {
        return true;
    } else {
        return false;
    }
}


bool CCleanup::IsMinPub(const CPubdesc& pd, bool is_refseq_prot)
{
    if (!pd.IsSetPub()) {
        return true;
    }
    bool found_non_minimal = false;
    ITERATE(CPubdesc::TPub::Tdata, it, pd.GetPub().Get()) {
        if ((*it)->IsMuid() || (*it)->IsPmid()) {
            if (is_refseq_prot) {
                found_non_minimal = true;
                break;
            }
        } else if ((*it)->IsGen()) {
            const CCit_gen& gen = (*it)->GetGen();
            if (gen.IsSetCit() && !gen.IsSetJournal() &&
                !gen.IsSetAuthors() && !gen.IsSetVolume() &&
                !gen.IsSetPages()) {
                //minimalish, keep looking
            } else {
                found_non_minimal = true;
            }
        } else {
            found_non_minimal = true;
            break;
        }
    }

    return !found_non_minimal;
}


bool CCleanup::RescueSiteRefPubs(CSeq_entry_Handle seh)
{
    bool found_site_ref = false;
    CFeat_CI f(seh, CSeqFeatData::e_Imp);
    while (f && !found_site_ref) {
        if (IsSiteRef(*(f->GetSeq_feat()))) {
            found_site_ref = true;
        }
        ++f;
    }
    if (!found_site_ref) {
        return false;
    }

    bool any_change = false;
    for (CBioseq_CI b(seh); b; ++b) {
        bool is_refseq_prot = false;
        if (b->IsAa()) {
            ITERATE(CBioseq::TId, id_it, b->GetCompleteBioseq()->GetId()) {
                if ((*id_it)->IsOther()) {
                    is_refseq_prot = true;
                    break;
                }
            }
        }

        for (CFeat_CI p(*b); p; ++p) {
            if (!p->IsSetCit() || p->GetCit().Which() != CPub_set::e_Pub) {
                continue;
            }

            bool is_site_ref = IsSiteRef(*(p->GetSeq_feat()));
            ITERATE(CSeq_feat::TCit::TPub, c, p->GetCit().GetPub()) {
                CRef<CSeqdesc> d(new CSeqdesc());
                if ((*c)->IsEquiv()) {
                    ITERATE(CPub_equiv::Tdata, t, (*c)->GetEquiv().Get()) {
                        CRef<CPub> pub_copy(new CPub());
                        pub_copy->Assign(**t);
                        d->SetPub().SetPub().Set().push_back(pub_copy);
                    }

                } else {
                    CRef<CPub> pub_copy(new CPub());
                    pub_copy->Assign(**c);
                    d->SetPub().SetPub().Set().push_back(pub_copy);
                }
                if (is_site_ref) {
                    d->SetPub().SetReftype(CPubdesc::eReftype_sites);
                } else {
                    d->SetPub().SetReftype(CPubdesc::eReftype_feats);
                }
                auto changes = makeCleanupChange(0);
                CNewCleanup_imp pubclean(changes, 0);
                pubclean.BasicCleanup(d->SetPub(), ShouldStripPubSerial(*(b->GetCompleteBioseq())));
                if (!IsMinPub(d->SetPub(), is_refseq_prot)) {
                    MoveOneFeatToPubdesc(*p, d, *b, false);
                }
            }
            if (is_site_ref) {

                CSeq_feat_EditHandle feh(*p);
                CSeq_annot_Handle annot = feh.GetAnnot();

                feh.Remove();

                // remove old annot if now empty
                if (CNewCleanup_imp::ShouldRemoveAnnot(*(annot.GetCompleteSeq_annot()))) {
                    CSeq_annot_EditHandle annot_edit(annot);
                    annot_edit.Remove();
                }

            }
            any_change = true;
        }
    }
    return any_change;
}


bool CCleanup::AreBioSourcesMergeable(const CBioSource& src1, const CBioSource& src2)
{
    if (src1.IsSetOrg() && src1.GetOrg().IsSetTaxname() &&
        src2.IsSetOrg() && src2.GetOrg().IsSetTaxname() &&
        NStr::Equal(src1.GetOrg().GetTaxname(), src2.GetOrg().GetTaxname())) {
        return true;
    } else {
        return false;
    }
}


static bool s_SubsourceCompareC (
    const CRef<CSubSource>& st1,
    const CRef<CSubSource>& st2
)

{
    const CSubSource& sbs1 = *(st1);
    const CSubSource& sbs2 = *(st2);

    TSUBSOURCE_SUBTYPE chs1 = GET_FIELD (sbs1, Subtype);
    TSUBSOURCE_SUBTYPE chs2 = GET_FIELD (sbs2, Subtype);

    if (chs1 < chs2) return true;
    if (chs1 > chs2) return false;

    if (FIELD_IS_SET (sbs2, Name)) {
        if (! FIELD_IS_SET (sbs1, Name)) return true;
        if (NStr::CompareNocase(GET_FIELD (sbs1, Name), GET_FIELD (sbs2, Name)) < 0) return true;
    }

    return false;
}

static bool s_SameSubtypeC(const CSubSource& s1, const CSubSource& s2)
{
    if (!s1.IsSetSubtype() && !s2.IsSetSubtype()) {
        return true;
    } else if (!s1.IsSetSubtype() || !s2.IsSetSubtype()) {
        return false;
    } else {
        return s1.GetSubtype() == s2.GetSubtype();
    }
}

// close enough if second name contains the first
static bool s_NameCloseEnoughC(const CSubSource& s1, const CSubSource& s2)
{
    if (!s1.IsSetName() && !s2.IsSetName()) {
        return true;
    } else if (!s1.IsSetName() || !s2.IsSetName()) {
        return false;
    }
    const string& n1 = s1.GetName();
    const string& n2 = s2.GetName();

    if (NStr::Equal(n1, n2)) {
        return true;
    } else {
        return false;
    }
}


bool s_SubSourceListUniqued(CBioSource& biosrc)
{
    bool res = false;

    // sort and remove duplicates.
    if (biosrc.IsSetSubtype() && biosrc.GetSubtype().size() > 1) {
        if (!SUBSOURCE_ON_BIOSOURCE_IS_SORTED(biosrc, s_SubsourceCompareC)) {
            SORT_SUBSOURCE_ON_BIOSOURCE(biosrc, s_SubsourceCompareC);
        }

        // remove duplicates and subsources that contain previous values
        CBioSource::TSubtype::iterator s = biosrc.SetSubtype().begin();
        CBioSource::TSubtype::iterator s_next = s;
        ++s_next;
        while (s_next != biosrc.SetSubtype().end()) {
            if (s_SameSubtypeC(**s, **s_next) && s_NameCloseEnoughC(**s, **s_next)) {
                s = biosrc.SetSubtype().erase(s);
                res = true;
            } else {
                ++s;
            }
            ++s_next;
        }
    }

    return res;
}

bool CCleanup::MergeDupBioSources(CBioSource& src1, const CBioSource& add)
{
    bool any_change = false;
    // genome
    if ((!src1.IsSetGenome() || src1.GetGenome() == CBioSource::eGenome_unknown) &&
        add.IsSetGenome() && add.GetGenome() != CBioSource::eGenome_unknown) {
        src1.SetGenome(add.GetGenome());
        any_change = true;
    }
    // origin
    if ((!src1.IsSetOrigin() || src1.GetOrigin() == CBioSource::eOrigin_unknown) &&
        add.IsSetOrigin() && add.GetOrigin() != CBioSource::eOrigin_unknown) {
        src1.SetOrigin(add.GetOrigin());
        any_change = true;
    }
    // focus
    if (!src1.IsSetIs_focus() && add.IsSetIs_focus()) {
        src1.SetIs_focus();
        any_change = true;
    }

    // merge subtypes
    if (add.IsSetSubtype()) {
        ITERATE(CBioSource::TSubtype, it, add.GetSubtype()) {
            CRef<CSubSource> a(new CSubSource());
            a->Assign(**it);
            src1.SetSubtype().push_back(a);
        }
        any_change = true;
    }

    x_MergeDupOrgRefs(src1.SetOrg(), add.GetOrg());

    if (s_SubSourceListUniqued(src1)) {
        any_change = true;
    }

    return any_change;
}


bool CCleanup::x_MergeDupOrgNames(COrgName& on1, const COrgName& add)
{
    bool any_change = false;

    // OrgMods
    if (add.IsSetMod()) {
        ITERATE(COrgName::TMod, it, add.GetMod()) {
            CRef<COrgMod> a(new COrgMod());
            a->Assign(**it);
            on1.SetMod().push_back(a);
        }
        any_change = true;
    }

    // gcode
    if ((!on1.IsSetGcode() || on1.GetGcode() == 0) && add.IsSetGcode() && add.GetGcode() != 0) {
        on1.SetGcode(add.GetGcode());
        any_change = true;
    }

    // mgcode
    if ((!on1.IsSetMgcode() || on1.GetMgcode() == 0) && add.IsSetMgcode() && add.GetMgcode() != 0) {
        on1.SetMgcode(add.GetMgcode());
        any_change = true;
    }

    // lineage
    if (!on1.IsSetLineage() && add.IsSetLineage()) {
        on1.SetLineage(add.GetLineage());
        any_change = true;
    }

    // div
    if (!on1.IsSetDiv() && add.IsSetDiv()) {
        on1.SetDiv(add.GetDiv());
        any_change = true;
    }

    return any_change;
}


bool HasMod(const COrg_ref& org, const string& mod)
{
    if (!org.IsSetMod()) {
        return false;
    }
    ITERATE(COrg_ref::TMod, it, org.GetMod()) {
        if (NStr::Equal(*it, mod)) {
            return true;
        }
    }
    return false;
}


bool CCleanup::x_MergeDupOrgRefs(COrg_ref& org1, const COrg_ref& add)
{
    bool any_change = false;
    // mods
    if (add.IsSetMod()) {
        ITERATE(COrg_ref::TMod, it, add.GetMod()) {
            if (!HasMod(org1, *it)) {
                org1.SetMod().push_back(*it);
                any_change = true;
            }
        }
    }

    // dbxrefs
    if (add.IsSetDb()) {
        ITERATE(COrg_ref::TDb, it, add.GetDb()) {
            CRef<CDbtag> a(new CDbtag());
            a->Assign(**it);
            org1.SetDb().push_back(a);
        }
        any_change = true;
    }

    // synonyms
    if (add.IsSetSyn()) {
        ITERATE(COrg_ref::TSyn, it, add.GetSyn()) {
            org1.SetSyn().push_back(*it);
        }
        any_change = true;
    }

    if (add.IsSetOrgname()) {
        any_change |= x_MergeDupOrgNames(org1.SetOrgname(), add.GetOrgname());
    }

    return any_change;
}


bool CCleanup::MergeDupBioSources(CSeq_descr & seq_descr)
{
    bool any_change = false;
    CSeq_descr::Tdata::iterator src1 = seq_descr.Set().begin();
    while (src1 != seq_descr.Set().end()) {
        if ((*src1)->IsSource() && (*src1)->GetSource().IsSetOrg() && (*src1)->GetSource().GetOrg().IsSetTaxname()) {
            CSeq_descr::Tdata::iterator src2 = src1;
            ++src2;
            while (src2 != seq_descr.Set().end()) {
                if ((*src2)->IsSource() &&
                    AreBioSourcesMergeable((*src1)->GetSource(), (*src2)->GetSource())) {
                    MergeDupBioSources((*src1)->SetSource(), (*src2)->GetSource());

                    auto changes = makeCleanupChange(0);
                    CNewCleanup_imp srcclean(changes, 0);
                    srcclean.ExtendedCleanup((*src1)->SetSource());
                    src2 = seq_descr.Set().erase(src2);
                    any_change = true;
                } else {
                    ++src2;
                }
            }
        }
        ++src1;
    }
    return any_change;
}

/// Remove duplicate biosource descriptors
bool CCleanup::RemoveDupBioSource(CSeq_descr& descr)
{
    bool any_change = false;
    vector<CConstRef<CBioSource> > src_list;
    CSeq_descr::Tdata::iterator d = descr.Set().begin();
    while (d != descr.Set().end()) {
        if ((*d)->IsSource()) {
            bool found = false;
            ITERATE(vector<CConstRef<CBioSource> >, s, src_list) {
                if ((*d)->GetSource().Equals(**s)) {
                    found = true;
                    break;
                }
            }
            if (found) {
                d = descr.Set().erase(d);
                any_change = true;
            } else {
                CConstRef<CBioSource> src(&((*d)->GetSource()));
                src_list.push_back(src);
                ++d;
            }
        } else {
            ++d;
        }
    }
    return any_change;
}


CRef<CBioSource> CCleanup::BioSrcFromFeat(const CSeq_feat& f)
{
    if (!f.IsSetData() || !f.GetData().IsBiosrc()) {
        return CRef<CBioSource>();
    }
    CRef<CBioSource> src(new CBioSource());
    src->Assign(f.GetData().GetBiosrc());

    // move comment to subsource note
    if (f.IsSetComment()) {
        CRef<CSubSource> s(new CSubSource());
        s->SetSubtype(CSubSource::eSubtype_other);
        s->SetName(f.GetComment());
        src->SetSubtype().push_back(s);

    }

    // move dbxrefs on feature to source
    if (f.IsSetDbxref()) {
        ITERATE(CSeq_feat::TDbxref, it, f.GetDbxref()) {
            CRef<CDbtag> a(new CDbtag());
            a->Assign(**it);
            src->SetOrg().SetDb().push_back(a);
        }
    }
    auto changes = makeCleanupChange(0);
    CNewCleanup_imp srcclean(changes, 0);
    srcclean.ExtendedCleanup(*src);

    return src;
}


bool CCleanup::ConvertSrcFeatsToSrcDescs(CSeq_entry_Handle seh)
{
    bool any_change = false;
    for (CBioseq_CI b(seh); b; ++b) {
        bool transgenic_or_focus = false;
        CSeqdesc_CI existing_src(*b, CSeqdesc::e_Source);
        while (existing_src && !transgenic_or_focus) {
            if (existing_src->GetSource().IsSetIs_focus() ||
                existing_src->GetSource().HasSubtype(CSubSource::eSubtype_transgenic)) {
                transgenic_or_focus = true;
            }
            ++existing_src;
        }
        if (transgenic_or_focus) {
            continue;
        }
        for (CFeat_CI p(*b, CSeqFeatData::e_Biosrc); p; ++p) {
            if (p->GetLocation().IsInt() &&
                p->GetLocation().GetStart(eExtreme_Biological) == 0 &&
                p->GetLocation().GetStop(eExtreme_Biological) == b->GetBioseqLength() - 1) {
                CRef<CSeqdesc> d(new CSeqdesc());
                d->SetSource().Assign(*(BioSrcFromFeat(*(p->GetSeq_feat()))));

                // add descriptor to nuc-prot parent or sequence itself
                CBioseq_set_Handle parent = b->GetParentBioseq_set();
                if (parent && parent.IsSetClass() &&
                    parent.GetClass() == CBioseq_set::eClass_nuc_prot) {
                    CBioseq_set_EditHandle eh(parent);
                    eh.AddSeqdesc(*d);
                    MergeDupBioSources(eh.SetDescr());
                    RemoveDupBioSource(eh.SetDescr());
                    NormalizeDescriptorOrder(eh.SetDescr());
                } else {
                    CBioseq_EditHandle eh(*b);
                    eh.AddSeqdesc(*d);
                    MergeDupBioSources(eh.SetDescr());
                    RemoveDupBioSource(eh.SetDescr());
                    NormalizeDescriptorOrder(eh.SetDescr());
                }

                // remove feature
                CSeq_feat_EditHandle feh(*p);
                CSeq_annot_Handle ah = feh.GetAnnot();
                feh.Remove();
                if (CNewCleanup_imp::ShouldRemoveAnnot(*(ah.GetCompleteSeq_annot()))) {
                    CSeq_annot_EditHandle aeh(ah);
                    aeh.Remove();
                }

                any_change = true;
            }
        }
    }
    return any_change;
}



bool CCleanup::FixGeneXrefSkew(CSeq_entry_Handle seh)
{
    CFeat_CI fi(seh);
    size_t num_gene_locus = 0;
    size_t num_gene_locus_tag = 0;
    size_t num_gene_xref_locus = 0;
    size_t num_gene_xref_locus_tag = 0;

    while (fi) {
        if (fi->GetData().IsGene()) {
            if (fi->GetData().GetGene().IsSetLocus()) {
                num_gene_locus++;
            }
            if (fi->GetData().GetGene().IsSetLocus_tag()) {
                num_gene_locus_tag++;
            }
        } else if (fi->IsSetXref()) {
            const CGene_ref* g = fi->GetGeneXref();
            if (g) {
                if (g->IsSetLocus()) {
                    num_gene_xref_locus++;
                }
                if (g->IsSetLocus_tag()) {
                    num_gene_xref_locus_tag++;
                }
            }
        }
        if (num_gene_locus > 0) {
            if (num_gene_locus_tag > 0) {
                return false;
            }
            if (num_gene_xref_locus > 0) {
                return false;
            }
        }
        if (num_gene_locus_tag > 0) {
            if (num_gene_locus > 0) {
                return false;
            }
            if (num_gene_xref_locus_tag > 0) {
                return false;
            }
        }
        ++fi;
    }

    bool any_change = false;
    if (num_gene_locus == 0 && num_gene_locus_tag > 0) {
        if (num_gene_xref_locus > 0 && num_gene_xref_locus_tag == 0) {
            fi.Rewind();
            while (fi) {
                if (!fi->GetData().IsGene() && fi->GetGeneXref()) {
                    bool this_change = false;
                    CRef<CSeq_feat> new_f(new CSeq_feat());
                    new_f->Assign(*(fi->GetSeq_feat()));
                    NON_CONST_ITERATE(CSeq_feat::TXref, it, new_f->SetXref()) {
                        if ((*it)->IsSetData() && (*it)->GetData().IsGene()
                            && (*it)->GetData().GetGene().IsSetLocus()) {
                            (*it)->SetData().SetGene().SetLocus_tag((*it)->GetData().GetGene().GetLocus());
                            (*it)->SetData().SetGene().ResetLocus();
                            this_change = true;
                        }
                    }
                    if (this_change) {
                        CSeq_feat_EditHandle eh(*fi);
                        eh.Replace(*new_f);
                    }
                }
                ++fi;
            }
        }
    } else if (num_gene_locus > 0 && num_gene_locus_tag == 0) {
        if (num_gene_xref_locus == 0 && num_gene_xref_locus_tag > 0) {
            fi.Rewind();
            while (fi) {
                if (!fi->GetData().IsGene() && fi->GetGeneXref()) {
                    bool this_change = false;
                    CRef<CSeq_feat> new_f(new CSeq_feat());
                    new_f->Assign(*(fi->GetSeq_feat()));
                    NON_CONST_ITERATE(CSeq_feat::TXref, it, new_f->SetXref()) {
                        if ((*it)->IsSetData() && (*it)->GetData().IsGene()
                            && (*it)->GetData().GetGene().IsSetLocus_tag()) {
                            (*it)->SetData().SetGene().SetLocus((*it)->GetData().GetGene().GetLocus_tag());
                            (*it)->SetData().SetGene().ResetLocus_tag();
                            this_change = true;
                        }
                    }
                    if (this_change) {
                        CSeq_feat_EditHandle eh(*fi);
                        eh.Replace(*new_f);
                        any_change = true;
                    }
                }
                ++fi;
            }
        }
    }
    return any_change;
}


bool CCleanup::ShouldStripPubSerial(const CBioseq& bs)
{
    bool strip_serial = true;
    ITERATE(CBioseq::TId, id, bs.GetId()) {
        const CSeq_id& sid = **id;
        switch (sid.Which()) {
        case NCBI_SEQID(Genbank):
        case NCBI_SEQID(Tpg):
        {
            const CTextseq_id& tsid = *GET_FIELD(sid, Textseq_Id);
            if (FIELD_IS_SET(tsid, Accession)) {
                const string& acc = GET_FIELD(tsid, Accession);
                if (acc.length() == 6) {
                    strip_serial = false;
                }
            }
        }
        break;
        case NCBI_SEQID(Embl):
        case NCBI_SEQID(Ddbj):
            strip_serial = false;
            break;
        case NCBI_SEQID(not_set):
        case NCBI_SEQID(Local):
        case NCBI_SEQID(Other):
        case NCBI_SEQID(General):
            break;
        case NCBI_SEQID(Gibbsq):
        case NCBI_SEQID(Gibbmt):
        case NCBI_SEQID(Pir):
        case NCBI_SEQID(Swissprot):
        case NCBI_SEQID(Patent):
        case NCBI_SEQID(Prf):
        case NCBI_SEQID(Pdb):
        case NCBI_SEQID(Gpipe):
        case NCBI_SEQID(Tpe):
        case NCBI_SEQID(Tpd):
            strip_serial = false;
            break;
        default:
            break;
        }
    }
    return strip_serial;
}


bool CCleanup::RenormalizeNucProtSets(CSeq_entry_Handle seh)
{
    bool change_made = false;
    CConstRef<CSeq_entry> entry = seh.GetCompleteSeq_entry();
    if (seh.IsSet() && seh.GetSet().IsSetClass() &&
        entry->GetSet().IsSetSeq_set()) {
        CBioseq_set::TClass set_class = seh.GetSet().GetClass();
        if (set_class == CBioseq_set::eClass_nuc_prot) {
            if (entry->GetSet().GetSeq_set().size() == 1 &&
                entry->GetSet().GetSeq_set().front()->IsSeq()) {
                CSeq_entry_EditHandle eh = seh.GetEditHandle();
                eh.ConvertSetToSeq();
                if (eh.GetSeq().IsSetDescr()) {
                    RemoveUnseenTitles(eh.SetSeq());
                    NormalizeDescriptorOrder(eh.SetSeq().SetDescr());
                }
                change_made = true;
            }
        } else if (set_class == CBioseq_set::eClass_genbank ||
            set_class == CBioseq_set::eClass_mut_set ||
            set_class == CBioseq_set::eClass_pop_set ||
            set_class == CBioseq_set::eClass_phy_set ||
            set_class == CBioseq_set::eClass_eco_set ||
            set_class == CBioseq_set::eClass_wgs_set ||
            set_class == CBioseq_set::eClass_gen_prod_set ||
            set_class == CBioseq_set::eClass_small_genome_set) {
            ITERATE(CBioseq_set::TSeq_set, s, entry->GetSet().GetSeq_set()) {
                CSeq_entry_Handle ch = seh.GetScope().GetSeq_entryHandle(**s);
                change_made |= RenormalizeNucProtSets(ch);
            }
        }
    }
    return change_made;
}


bool CCleanup::DecodeXMLMarkChanged(std::string & str)
{
// return false;
    bool change_made = false;

    // This is more complex than you might initially think is necessary
    // because this needs to be as efficient as possible since it's
    // called on every single string in an object.

    SIZE_TYPE amp = str.find('&');
    if( NPOS == amp ) {
        // Check for the common case of no replacements required
        return change_made;
    }

    // transformations done by this function:
    const static struct {
        string src_word;
        string result_word;
    } transformations[] = {
        // all start with an implicit ampersand
        // and end with an implicit semi-colon
        { "amp",      "&"      },
        { "apos",     "\'"     },
        { "gt",       ">"      },
        { "lt",       "<"      },
        { "quot",     "\""     },
        { "#13&#10",  ""       },
        { "#13;&#10", ""       },
        { "#916",     "Delta"  },
        { "#945",     "alpha"  },
        { "#946",     "beta"   },
        { "#947",     "gamma"  },
        { "#952",     "theta"  },
        { "#955",     "lambda" },
        { "#956",     "mu"     },
        { "#957",     "nu"     },
        { "#8201",    ""       },
        { "#8206",    ""       },
        { "#8242",    "'"      },
        { "#8594",    "->"     },
        { "#8722",    "-"      },
        { "#8710",    "delta"  },
        { "#64257",   "fi"     },
        { "#64258",   "fl"     },
        { "#65292",   ","      },
    };

    // Collisions should be rare enough that the CFastMutex is
    // faster than recreating the searcher each time this function is called
    static CTextFsm<int> searcher;
    // set searcher's state, if not already done
    {
        // just in case of the tiny chance that two threads try to prime
        // the searcher at the same time.
        static CFastMutex searcher_mtx;
        CFastMutexGuard searcher_mtx_guard( searcher_mtx );
        if (! searcher.IsPrimed()) {
            for (unsigned idx = 0; idx < ArraySize(transformations); ++idx) {
                // match type is index into transformations array
                searcher.AddWord(transformations[idx].src_word, idx);
            }
            searcher.Prime();
        }
    }

    // a smart compiler probably won't need this manual optimization,
    // but just in case.
    const SIZE_TYPE str_len = str.length();

    // fill result up to the first '&'
    string result;
    result.reserve( str_len );
    copy( str.begin(), str.begin() + amp,
        back_inserter(result) );

    // at the start of each loop, the result is filled in
    // up to the ampersand (amp)
    while( amp != NPOS && amp < str_len ) {

        // find out what the ampersand code represents
        // (if it represents anything)
        int state = searcher.GetInitialState();
        SIZE_TYPE search_pos = (amp + 1);
        if (str[search_pos] == ' ') {
            break;
        }
        for( ; search_pos < str_len ; ++search_pos ) {
            const char ch = str[search_pos];
            if( ch == ';' ) {
                break;
            }
            if( ch == '&' && state == 0 ) {
                --search_pos; // so we don't skip over the '&'
                state = searcher.GetInitialState(); // force "no-match"
                break;
            }
            state = searcher.GetNextState(state, ch);
        }

        if( search_pos == str_len && searcher.IsMatchFound(state) ) {
            // copy the translation of the XML code:
            _ASSERT( searcher.GetMatches(state).size() == 1 );
            const int match_idx = searcher.GetMatches(state)[0];
            const string & result_word = transformations[match_idx].result_word;
            copy( result_word.begin(), result_word.end(),
                back_inserter(result) );
            change_made = true;
            break;
        }

        if( search_pos >= str_len ) {
            // we reached the end without finding anything, so
            // copy the rest and break
            copy( str.begin() + amp, str.end(),
                back_inserter(result) );
            break;
        }

        if( searcher.IsMatchFound(state) ) {
            // copy the translation of the XML code:
            _ASSERT( searcher.GetMatches(state).size() == 1 );
            const int match_idx = searcher.GetMatches(state)[0];
            const string & result_word = transformations[match_idx].result_word;
            copy( result_word.begin(), result_word.end(),
                back_inserter(result) );
            change_made = true;
        } else {
            // no match found, so copy the text we looked at
            // as-is
            copy( str.begin() + amp, str.begin() + search_pos + 1,
                back_inserter(result) );
        }

        // find next_amp
        if( str[search_pos] == '&' ) {
            // special case that occurs when there are multiple '&' together
            ++search_pos;
            result += '&';
        }
        SIZE_TYPE next_amp = str.find('&', search_pos );
        if( NPOS == next_amp ) {
            // no more amps; copy the rest and break
            copy( str.begin() + search_pos + 1, str.end(),
                back_inserter(result) );
            break;
        }

        // copy up to the next amp
        if( (search_pos + 1) < next_amp ) {
            copy( str.begin() + search_pos + 1, str.begin() + next_amp,
                back_inserter(result) );
        }
        amp = next_amp;
    }

    if (change_made) {
      str = result;
    }

    return change_made;
}


CRef<CSeq_loc> CCleanup::GetProteinLocationFromNucleotideLocation(const CSeq_loc& nuc_loc, const CSeq_feat& cds, CScope& scope, bool require_inframe)
{
    if (require_inframe) {
        feature::ELocationInFrame is_in_frame = feature::IsLocationInFrame(scope.GetSeq_featHandle(cds), nuc_loc);
        bool is_ok = false;
        switch (is_in_frame) {
            case feature::eLocationInFrame_InFrame:
                is_ok = true;
                break;
            case feature::eLocationInFrame_BadStart:
                if (cds.GetLocation().GetStart(eExtreme_Biological) == nuc_loc.GetStart(eExtreme_Biological)) {
                    is_ok = true;
                }
                break;
            case feature::eLocationInFrame_BadStop:
                if (cds.GetLocation().GetStop(eExtreme_Biological) == nuc_loc.GetStop(eExtreme_Biological)) {
                    is_ok = true;
                }
                break;
            case feature::eLocationInFrame_BadStartAndStop:
                if (cds.GetLocation().GetStart(eExtreme_Biological) == nuc_loc.GetStart(eExtreme_Biological) &&
                    cds.GetLocation().GetStop(eExtreme_Biological) == nuc_loc.GetStop(eExtreme_Biological)) {
                    is_ok = true;
                }
                break;
            case feature::eLocationInFrame_NotIn:
                break;
        }
        if (!is_ok) {
            return CRef<CSeq_loc>();
        }
    }
    CRef<CSeq_loc> new_loc;
    CRef<CSeq_loc_Mapper> nuc2prot_mapper(
        new CSeq_loc_Mapper(cds, CSeq_loc_Mapper::eLocationToProduct, &scope));
    new_loc = nuc2prot_mapper->Map(nuc_loc);
    if (!new_loc) {
        return CRef<CSeq_loc>();
    }

    const CSeq_id* sid = new_loc->GetId();
    const CSeq_id* orig_id = nuc_loc.GetId();
    if (!sid || (orig_id && sid->Equals(*orig_id))) {
        // unable to map to protein location
        return CRef<CSeq_loc>();
    }

    new_loc->ResetStrand();

    // if location includes stop codon, remove it
    CBioseq_Handle prot = scope.GetBioseqHandle(*sid);
    if (prot && new_loc->GetStop(eExtreme_Positional) >= prot.GetBioseqLength())
    {
        CRef<CSeq_id> sub_id(new CSeq_id());
        sub_id->Assign(*sid);
        CSeq_loc sub(*sub_id, prot.GetBioseqLength(), new_loc->GetStop(eExtreme_Positional), new_loc->GetStrand());
        new_loc = sequence::Seq_loc_Subtract(*new_loc, sub, CSeq_loc::fMerge_All | CSeq_loc::fSort, &scope);
        if (nuc_loc.IsPartialStop(eExtreme_Biological)) {
            new_loc->SetPartialStop(true, eExtreme_Biological);
        }
    }

    if (!new_loc->IsInt() && !new_loc->IsPnt()) {
        CRef<CSeq_loc> tmp = sequence::Seq_loc_Merge(*new_loc, CSeq_loc::fMerge_All, &scope);
        new_loc = tmp;
    }

    // fix partials if protein feature starts or ends at beginning or end of protein sequence
    if (!cds.GetLocation().IsPartialStart(eExtreme_Biological) &&
        new_loc->GetStart(eExtreme_Biological) == 0) {
        if (new_loc->IsPartialStart(eExtreme_Biological)) {
            new_loc->SetPartialStart(false, eExtreme_Biological);
        }
    }
    if (!cds.GetLocation().IsPartialStop(eExtreme_Biological) &&
        new_loc->GetStop(eExtreme_Biological) == prot.GetBioseqLength() - 1) {
        if (new_loc->IsPartialStop(eExtreme_Biological)) {
            new_loc->SetPartialStop(false, eExtreme_Biological);
        }
    }

    return new_loc;
}


CRef<CSeq_loc> CCleanup::GetProteinLocationFromNucleotideLocation(const CSeq_loc& nuc_loc, CScope& scope)
{
    CConstRef<CSeq_feat> cds = sequence::GetOverlappingCDS(nuc_loc, scope);
    if (!cds || !cds->IsSetProduct()) {
        // there is no overlapping coding region feature, so there is no appropriate
        // protein sequence to move to
        return CRef<CSeq_loc>();
    }

    return GetProteinLocationFromNucleotideLocation(nuc_loc, *cds, scope);
}



bool CCleanup::RepackageProteins(const CSeq_feat& cds, CBioseq_set_Handle np)
{
    if (!cds.IsSetProduct() || !cds.GetProduct().IsWhole()) {
        // no product, or product is specified weirdly
        return false;
    }
    CBioseq_Handle protein = np.GetTSE_Handle().GetBioseqHandle(cds.GetProduct().GetWhole());
    if (!protein) {
        // protein is not in the same TSE
        return false;
    }
    if (protein.GetParentBioseq_set() == np) {
        // already in the right set
        return false;
    }
    CBioseq_set_EditHandle eh(np);
    CSeq_entry_Handle ph = protein.GetSeq_entry_Handle();
    CSeq_entry_EditHandle peh(ph);
    eh.TakeEntry(peh);
    return true;
}


bool CCleanup::RepackageProteins(CSeq_entry_Handle seh)
{
    bool changed = false;
    CSeq_entry_CI si(seh, CSeq_entry_CI::fRecursive | CSeq_entry_CI::fIncludeGivenEntry, CSeq_entry::e_Set);
    while (si) {
        CBioseq_set_Handle set = si->GetSet();
        if (set.IsSetClass() && set.GetClass() == CBioseq_set::eClass_nuc_prot && set.HasAnnots()) {
            ITERATE(CBioseq_set::TAnnot, annot_it, set.GetCompleteBioseq_set()->GetAnnot()) {
                if ((*annot_it)->IsSetData() && (*annot_it)->IsFtable()) {
                    ITERATE(CSeq_annot::TData::TFtable, feat_it, (*annot_it)->GetData().GetFtable()) {
                        if ((*feat_it)->IsSetData() && (*feat_it)->GetData().IsCdregion()) {
                            changed |= RepackageProteins(**feat_it, set);
                        }
                    }
                }
            }
        }
        ++si;
    }
    return changed;
}


bool CCleanup::ConvertDeltaSeqToRaw(CSeq_entry_Handle seh, CSeq_inst::EMol filter)
{
    bool any_change = false;
    for (CBioseq_CI bi(seh, filter); bi; ++bi) {
        CBioseq_Handle bsh = *bi;
        CRef<CSeq_inst> inst(new CSeq_inst());
        inst->Assign(bsh.GetInst());
        if (inst->ConvertDeltaToRaw()) {
            CBioseq_EditHandle beh(bsh);
            beh.SetInst(*inst);
            any_change = true;
        }
    }
    return any_change;
}


bool CCleanup::ParseCodeBreak(const CSeq_feat& feat,
        CCdregion& cds,
        const CTempString& str,
        CScope& scope,
        IObjtoolsListener* pMessageListener)
{
    if (str.empty() || !feat.IsSetLocation()) {
        return false;
    }

    const CSeq_id* feat_loc_seq_id = feat.GetLocation().GetId();
    if (!feat_loc_seq_id) {
        return false;
    }

    string::size_type aa_pos = NStr::Find(str, "aa:");
    string::size_type len = 0;
    string::size_type loc_pos, end_pos;
    char protein_letter = 'X';
    CRef<CSeq_loc> break_loc;

    if (aa_pos == string::npos) {
        aa_pos = NStr::Find(str, ",");
        if (aa_pos != string::npos) {
            aa_pos = NStr::Find(str, ":", aa_pos);
        }
        if (aa_pos != string::npos) {
            aa_pos++;
        }
    } else {
        aa_pos += 3;
    }

    if (aa_pos != string::npos) {
        while (aa_pos < str.length() && isspace(str[aa_pos])) {
            aa_pos++;
        }
        while (aa_pos + len < str.length() && isalpha(str[aa_pos + len])) {
            len++;
        }
        if (len != 0) {
            protein_letter = x_ValidAminoAcid(str.substr(aa_pos, len));
        }
    }

    loc_pos = NStr::Find(str, "(pos:");

    using TSubcode = CCleanupMessage::ESubcode;
    auto postMessage =
        [pMessageListener](string msg, TSubcode subcode) {
            pMessageListener->PutMessage(
                    CCleanupMessage(msg, eDiag_Error, CCleanupMessage::ECode::eCodeBreak, subcode));
        };

    if (loc_pos == string::npos) {
        if (pMessageListener) {
            string msg = "Unable to identify code-break location in '" + str + "'";
            postMessage(msg, TSubcode::eParseError);
        }
        return false;
    }
    loc_pos += 5;
    while (loc_pos < str.length() && isspace(str[loc_pos])) {
        loc_pos++;
    }

    end_pos = NStr::Find(str, ",aa:", loc_pos);
    if (end_pos == NPOS) {
        end_pos = NStr::Find(str, ",", loc_pos);
        if (end_pos == NPOS) {
            end_pos = str.length();
        }
    }

    string pos = NStr::TruncateSpaces_Unsafe(str.substr(loc_pos, end_pos - loc_pos));

    // handle multi-interval positions by adding a join() around them
    if (pos.find_first_of(",") != string::npos) {
        pos = "join(" + pos + ")";
    }

    break_loc = ReadLocFromText(pos, feat_loc_seq_id, &scope);

    if (!break_loc) {
        if (pMessageListener) {
            string msg = "Unable to extract code-break location from '" + str + "'";
            postMessage(msg, TSubcode::eParseError);
        }
        return false;
    }

    if (break_loc->IsInt() && sequence::GetLength(*break_loc, &scope) > 3) {
        if (pMessageListener) {
            string msg = "code-break location exceeds 3 bases";
            postMessage(msg, TSubcode::eBadLocation);
        }
        return false;
    }
    if ((break_loc->IsInt() || break_loc->IsPnt()) &&
         sequence::Compare(*break_loc, feat.GetLocation(), &scope, sequence::fCompareOverlapping) != sequence::eContained) {
        if (pMessageListener) {
            string msg = "code-break location lies outside of coding region";
            postMessage(msg, TSubcode::eBadLocation);
        }
        return false;
    }

    if (FIELD_IS_SET(feat.GetLocation(), Strand)) {
        if (GET_FIELD(feat.GetLocation(), Strand) == eNa_strand_minus) {
            break_loc->SetStrand(eNa_strand_minus);
        }
        else if (GET_FIELD(feat.GetLocation(), Strand) == eNa_strand_plus) {
            break_loc->SetStrand(eNa_strand_plus);
        }
    } else {
        RESET_FIELD(*break_loc, Strand);
    }

    // need to build code break object and add it to coding region
    CRef<CCode_break> newCodeBreak(new CCode_break());
    CCode_break::TAa& aa = newCodeBreak->SetAa();
    aa.SetNcbieaa(protein_letter);
    newCodeBreak->SetLoc(*break_loc);

    CCdregion::TCode_break& orig_list = cds.SetCode_break();
    orig_list.push_back(newCodeBreak);

    return true;
}


bool CCleanup::ParseCodeBreaks(CSeq_feat& feat, CScope& scope)
{
    if (!feat.IsSetData() || !feat.GetData().IsCdregion() ||
        !feat.IsSetQual() || !feat.IsSetLocation()) {
        return false;
    }

    bool any_removed = false;
    CSeq_feat::TQual::iterator it = feat.SetQual().begin();
    while (it != feat.SetQual().end()) {
        if ((*it)->IsSetQual() &&
            NStr::EqualNocase((*it)->GetQual(), "transl_except") &&
            (*it)->IsSetVal() &&
            ParseCodeBreak(feat, feat.SetData().SetCdregion(), (*it)->GetVal(), scope)) {
            it = feat.SetQual().erase(it);
            any_removed = true;
        } else {
            ++it;
        }
    }
    if (feat.GetQual().size() == 0) {
        feat.ResetQual();
    }
    return any_removed;
}


size_t CCleanup::MakeSmallGenomeSet(CSeq_entry_Handle entry)
{
    map<string, CRef<CInfluenzaSet>> flu_map;

    CBioseq_CI bi(entry, CSeq_inst::eMol_na);
    while (bi) {
        CSeqdesc_CI src(*bi, CSeqdesc::e_Source);
        if (src && src->GetSource().IsSetOrg()) {
            string key = CInfluenzaSet::GetKey(src->GetSource().GetOrg());
            if (!NStr::IsBlank(key)) {
                // add to set
                auto it = flu_map.find(key);
                if (it == flu_map.end()) {
                    CRef<CInfluenzaSet> new_set(new CInfluenzaSet(key));
                    new_set->AddBioseq(*bi);
                    flu_map[key] = new_set;
                } else {
                    it->second->AddBioseq(*bi);
                }
            }
        }
        ++bi;
    }
    // now create sets
    size_t added = 0;
    for (auto& entry : flu_map) {
        if (entry.second->OkToMakeSet()) {
            entry.second->MakeSet();
            added++;
        }
    }

    return added;
}


void AddIRDMiscFeature(CBioseq_Handle bh, const CDbtag& tag)
{
    CSeq_annot_Handle ftable;

    CSeq_annot_CI annot_ci(bh);
    for (; annot_ci; ++annot_ci) {
        if ((*annot_ci).IsFtable()) {
            ftable = *annot_ci;
            break;
        }
    }

    if (!ftable) {
        CBioseq_EditHandle beh = bh.GetEditHandle();
        CRef<CSeq_annot> new_annot(new CSeq_annot());
        ftable = beh.AttachAnnot(*new_annot);
    }

    CSeq_annot_EditHandle aeh(ftable);

    CRef<CSeq_feat> f(new CSeq_feat());
    f->SetData().SetImp().SetKey("misc_feature");
    f->SetLocation().SetInt().SetFrom(0);
    f->SetLocation().SetInt().SetTo(bh.GetBioseqLength() - 1);
    f->SetLocation().SetInt().SetId().Assign(*(bh.GetSeqId()));
    CRef<CDbtag> xref(new CDbtag());
    xref->Assign(tag);
    f->SetDbxref().push_back(xref);
    CRef<CSeqFeatXref> suppress(new CSeqFeatXref());
    suppress->SetData().SetGene();
    f->SetXref().push_back(suppress);
    aeh.AddFeat(*f);
}


bool CCleanup::MakeIRDFeatsFromSourceXrefs(CSeq_entry_Handle entry)
{
    bool any = false;
    CBioseq_CI bi(entry, CSeq_inst::eMol_na);
    while (bi) {
        CSeqdesc_CI src(*bi, CSeqdesc::e_Source);
        while (src) {
            if (src->GetSource().IsSetOrg() && src->GetSource().GetOrg().IsSetDb()) {
                CRef<COrg_ref> org(const_cast<COrg_ref *>(&(src->GetSource().GetOrg())));
                COrg_ref::TDb::iterator db = org->SetDb().begin();
                while (db != org->SetDb().end()) {
                    if ((*db)->IsSetDb() && NStr::Equal((*db)->GetDb(), "IRD")) {
                        AddIRDMiscFeature(*bi, **db);
                        db = org->SetDb().erase(db);
                        any = true;
                    } else {
                        ++db;
                    }
                }
                if (org->GetDb().size() == 0) {
                    org->ResetDb();
                }
            }
            ++src;
        }
        ++bi;
    }
    return any;
}

//LCOV_EXCL_START
//not used by asn_cleanup but used by other applications
const unsigned int methionine_encoded = 'M' - 'A';

bool CCleanup::IsMethionine(const CCode_break& cb)
{
    if (!cb.IsSetAa()) {
        return false;
    }
    bool rval = false;
    switch (cb.GetAa().Which()) {
        case CCode_break::TAa::e_Ncbi8aa:
            if (cb.GetAa().GetNcbi8aa() == methionine_encoded) {
                rval = true;
            }
            break;
        case CCode_break::TAa::e_Ncbieaa:
            if (cb.GetAa().GetNcbieaa() == 'M') {
                rval = true;
            }
            break;
        case CCode_break::TAa::e_Ncbistdaa:
            if (cb.GetAa().GetNcbistdaa() == methionine_encoded) {
                rval = true;
            }
            break;
        default:
            break;
    }
    return rval;
}
//LCOV_EXCL_STOP


//LCOV_EXCL_START
//not used by asn_cleanup but used by other applications
CConstRef<CCode_break> CCleanup::GetCodeBreakForLocation(size_t pos, const CSeq_feat& cds)
{
    if (!cds.IsSetData() || !cds.GetData().IsCdregion() ||
        !cds.IsSetLocation() ||
        !cds.GetData().GetCdregion().IsSetCode_break()) {
        return CConstRef<CCode_break>();
    }

    TSeqPos frame = 0;
    if (cds.IsSetData() && cds.GetData().IsCdregion() && cds.GetData().GetCdregion().IsSetFrame())
    {
        switch(cds.GetData().GetCdregion().GetFrame())
        {
        case CCdregion::eFrame_not_set :
        case CCdregion::eFrame_one : frame = 0; break;
        case CCdregion::eFrame_two : frame = 1; break;
        case  CCdregion::eFrame_three : frame = 2; break;
        default : frame = 0; break;
        }
    }

    for (auto cb : cds.GetData().GetCdregion().GetCode_break()) {
        if (cb->IsSetLoc()) {
            TSeqPos offset = sequence::LocationOffset(cds.GetLocation(),
                            cb->GetLoc());
            if (offset >= frame &&
                ((offset - frame) / 3 ) + 1 == pos) {
                return cb;
            }
        }
    }
    return CConstRef<CCode_break>();
}
//LCOV_EXCL_STOP

//LCOV_EXCL_START
//appears not to be used
void CCleanup::SetCodeBreakLocation(CCode_break& cb, size_t pos, const CSeq_feat& cds)
{
    int start = static_cast<int>((pos-1)*3);
    //start -= 1;
    //start *= 3;
    int frame = 0;
    if (cds.IsSetData() && cds.GetData().IsCdregion() && cds.GetData().GetCdregion().IsSetFrame())
    {
        switch(cds.GetData().GetCdregion().GetFrame())
        {
        case CCdregion::eFrame_not_set :
        case CCdregion::eFrame_one : frame = 0; break;
        case CCdregion::eFrame_two : frame = 1; break;
        case  CCdregion::eFrame_three : frame = 2; break;
        default : frame = 0; break;
        }
    }
    int frame_shift = (start - frame) % 3;
    if (frame_shift < 0) {
        frame_shift += 3;
    }
    if (frame_shift == 1)
    start += 2;
    else if (frame_shift == 2)
    start += 1;

    int offset = 0;
    CRef<CSeq_loc> packed (new CSeq_loc());
    for (CSeq_loc_CI loc_iter(cds.GetLocation());  loc_iter;  ++loc_iter) {
        int len = loc_iter.GetRange().GetLength();
        if (offset <= start && offset + len > start) {
            CRef<CSeq_interval> tmp(new CSeq_interval());
            tmp->SetId().Assign(loc_iter.GetSeq_id());
            if (loc_iter.IsSetStrand() && loc_iter.GetStrand() == eNa_strand_minus) {
                tmp->SetStrand(eNa_strand_minus);
                tmp->SetTo(loc_iter.GetRange().GetTo() - (start - offset) );
            } else {
                tmp->SetFrom(loc_iter.GetRange().GetFrom() + start - offset);
            }
            if (offset <= start + 2 && offset + len > start + 2) {
                if (loc_iter.IsSetStrand() && loc_iter.GetStrand() == eNa_strand_minus) {
                    tmp->SetFrom(loc_iter.GetRange().GetTo() - (start - offset + 2) );
                } else {
                    tmp->SetTo(loc_iter.GetRange().GetFrom() + start - offset + 2);
                }
            } else {
                if (loc_iter.IsSetStrand() && loc_iter.GetStrand() == eNa_strand_minus) {
                    tmp->SetFrom(loc_iter.GetRange().GetFrom());
                } else {
                    tmp->SetTo(loc_iter.GetRange().GetTo());
                }
            }
            packed->SetPacked_int().Set().push_back(tmp);
        } else if (offset > start && offset <= start + 2) {
            // add new interval
            CRef<CSeq_interval> tmp (new CSeq_interval());
            tmp->SetId().Assign(loc_iter.GetSeq_id());
            if (loc_iter.IsSetStrand() && loc_iter.GetStrand() == eNa_strand_minus) {
                tmp->SetStrand(eNa_strand_minus);
                tmp->SetTo(loc_iter.GetRange().GetTo());
                if (offset + len >= start + 2) {
                    tmp->SetFrom(loc_iter.GetRange().GetTo() - (start - offset + 2) );
                } else {
                    tmp->SetFrom(loc_iter.GetRange().GetFrom());
                }
            } else {
                tmp->SetFrom(loc_iter.GetRange().GetFrom());
                if (offset + len >= start + 2) {
                    tmp->SetTo(loc_iter.GetRange().GetFrom() + start - offset + 2);
                } else {
                    tmp->SetTo(loc_iter.GetRange().GetTo());
                }
            }

            packed->SetPacked_int().Set().push_back(tmp);
        }
        offset += len;
    }
    if (packed->Which() != CSeq_loc::e_Packed_int || packed->GetPacked_int().Get().size() == 0) {
        cb.ResetLoc();
    }
    if (packed->GetPacked_int().Get().size() == 1) {
        cb.SetLoc().SetInt().Assign(*(packed->GetPacked_int().Get().front()));
    } else {
        cb.SetLoc(*packed);
    }
}
//LCOV_EXCL_STOP


//LCOV_EXCL_START
//not used by asn_cleanup but used by other applications
bool CCleanup::FixRNAEditingCodingRegion(CSeq_feat& cds)
{
    if (!cds.IsSetData() || !cds.GetData().IsCdregion()) {
        return false;
    }
    if (!cds.IsSetLocation() ||
        cds.GetLocation().IsPartialStart(eExtreme_Biological)) {
        return false;
    }
    CConstRef<CCode_break> cbstart = GetCodeBreakForLocation(1, cds);
    if (cbstart && !CCleanup::IsMethionine(*cbstart)) {
        // already have a start translation exception AND it is not methionine
        return false;
    }

    bool any_change = false;
    if (!cds.IsSetExcept_text() || NStr::IsBlank(cds.GetExcept_text())) {
        cds.SetExcept_text("RNA editing");
        any_change = true;
    } else if (NStr::Find(cds.GetExcept_text(), "RNA editing") == string::npos) {
        cds.SetExcept_text(cds.GetExcept_text() + "; RNA editing");
        any_change = true;
    }
    if (!cds.IsSetExcept() || !cds.GetExcept()) {
        cds.SetExcept(true);
        any_change = true;
    }
    return any_change;
}
//LCOV_EXCL_STOP


//LCOV_EXCL_START
//not used by asn_cleanup but used by other applications
bool CCleanup::CleanupCollectionDates(CSeq_entry_Handle seh, bool month_first)
{
    bool any_changes = false;

    vector<CRef<COrg_ref> > rq_list;
    vector<const CSeqdesc* > src_descs;
    vector<CConstRef<CSeq_feat> > src_feats;

    GetSourceDescriptors(*(seh.GetCompleteSeq_entry()), src_descs);
    vector<const CSeqdesc* >::iterator desc_it = src_descs.begin();
    while (desc_it != src_descs.end()) {
        if ((*desc_it)->GetSource().IsSetSubtype()) {
            CSeqdesc* desc = const_cast<CSeqdesc*>(*desc_it);
            for (auto s : desc->SetSource().SetSubtype()) {
                if (s->IsSetSubtype() && s->GetSubtype() == CSubSource::eSubtype_collection_date
                    && s->IsSetName()) {
                    bool month_ambiguous = false;
                    string new_date = CSubSource::FixDateFormat(s->GetName(), month_first, month_ambiguous);
                    if (!NStr::Equal(new_date, s->GetName())) {
                        s->SetName(new_date);
                        any_changes = true;
                    }
                }
            }
        }
        ++desc_it;
    }

    CFeat_CI feat(seh, SAnnotSelector(CSeqFeatData::e_Biosrc));
    while (feat) {
        if (feat->GetData().GetBiosrc().IsSetSubtype()) {
            CRef<CSeq_feat> new_feat(new CSeq_feat());
            new_feat->Assign(*(feat->GetOriginalSeq_feat()));
            bool local_change = false;
            for (auto s : new_feat->SetData().SetBiosrc().SetSubtype()) {
                if (s->IsSetSubtype() && s->GetSubtype() == CSubSource::eSubtype_collection_date
                    && s->IsSetName()) {
                    bool month_ambiguous = false;
                    string new_date = CSubSource::FixDateFormat(s->GetName(), month_first, month_ambiguous);
                    if (!NStr::Equal(new_date, s->GetName())) {
                        s->SetName(new_date);
                        local_change = true;
                    }
                }
            }
            if (local_change) {
                any_changes = true;
                CSeq_feat_EditHandle efh(*feat);
                efh.Replace(*new_feat);
            }
            ++feat;
        }
    }

    return any_changes;
}
//LCOV_EXCL_STOP


void CCleanup::AutodefId(CSeq_entry_Handle seh)
{
    // remove existing options (TODO)
    for (CBioseq_CI b(seh); b; ++b) {
        bool removed = true;
        while (removed) {
            removed = false;
            CSeqdesc_CI ud(*b, CSeqdesc::e_User);
            while (ud) {
                if (ud->GetUser().IsAutodefOptions()) {
                    CSeq_entry_Handle s = ud.GetSeq_entry_Handle();
                    CSeq_entry_EditHandle se = s.GetEditHandle();
                    se.RemoveSeqdesc(*ud);
                    removed = true;
                    break;
                }
                ++ud;
            }
        }
    }

    // create new options
    CRef<CUser_object> auto_user = CAutoDef::CreateIDOptions(seh);
    CRef<CSeqdesc> d(new CSeqdesc());
    d->SetUser().Assign(*auto_user);
    CSeq_entry_EditHandle eh = seh.GetEditHandle();
    eh.AddSeqdesc(*d);

    CAutoDef::RegenerateSequenceDefLines(seh);
}


MAKE_CONST_MAP(kViralStrandMap, string, string,
{
    {"root",                                        "dsDNA"},
    {"Acinetobacter phage vB_AbaP_100",             "unknown"},
    {"Aeromonas phage BUCT551",                     "unknown"},
    {"Aeromonas phage BUCT552",                     "unknown"},
    {"Ainoaviricetes",                              "ssDNA"},
    {"Albetovirus",                                 "ssRNA(+)"},
    {"Alphapleolipovirus",                          "dsDNA; ssDNA"},
    {"Alphasatellitidae",                           "ssDNA"},
    {"Alphatetraviridae",                           "ssRNA(+)"},
    {"Alvernaviridae",                              "ssRNA(+)"},
    {"Amalgaviridae",                               "dsRNA"},
    {"Anelloviridae",                               "ssDNA(-)"},
    {"Arenaviridae",                                "ssRNA(+/-)"},
    {"Astroviridae",                                "ssRNA(+)"},
    {"Aumaivirus",                                  "ssRNA(+)"},
    {"Avsunviroidae",                               "ssRNA"},
    {"Bacilladnaviridae",                           "ssDNA"},
    {"Bacillus phage SWEP1",                        "unknown"},
    {"Bandavirus",                                  "ssRNA(+/-)"},
    {"Banyangvirus",                                "ssRNA(+/-)"},
    {"Barnaviridae",                                "ssRNA(+)"},
    {"Beidivirus",                                  "ssRNA(-)"},
    {"Benyviridae",                                 "ssRNA(+)"},
    {"Betapleolipovirus",                           "dsDNA"},
    {"Bidnaviridae",                                "ssDNA"},
    {"Bifilivirus",                                 "ssDNA(+)"},
    {"Birnaviridae",                                "dsRNA"},
    {"Blubervirales",                               "dsDNA-RT"},
    {"Botourmiaviridae",                            "ssRNA(+)"},
    {"Botybirnavirus",                              "dsRNA"},
    {"Bromoviridae",                                "ssRNA(+)"},
    {"Caliciviridae",                               "ssRNA(+)"},
    {"Campylobacter phage PC10",                    "unknown"},
    {"Carmotetraviridae",                           "ssRNA(+)"},
    {"Caulimoviridae",                              "dsDNA-RT"},
    {"Chrysoviridae",                               "dsRNA"},
    {"Circoviridae",                                "ssDNA(+/-)"},
    {"Closteroviridae",                             "ssRNA(+)"},
    {"Coconut foliar decay virus",                  "ssDNA(+)"},
    {"Coguvirus eburi",                             "ssRNA(+/-)"},
    {"Coguvirus",                                   "ssRNA(-)"},
    {"Cronobacter phage JC03",                      "unknown"},
    {"Cruliviridae",                                "ssRNA(-)"},
    {"Cystoviridae",                                "dsRNA"},
    {"Deltavirus",                                  "ssRNA(-)"},
    {"Duplornaviricota",                            "dsRNA"},
    {"Endornaviridae",                              "dsRNA"},
    {"Enterococcus phage 113",                      "unknown"},
    {"Enterococcus phage EFC-1",                    "unknown"},
    {"Enterococcus phage vb_GEC_Ef_S_9",            "unknown"},
    {"Erwinia phage pEa_SNUABM_4",                  "unknown"},
    {"Erwinia phage pEa_SNUABM_6",                  "unknown"},
    {"Erwinia phage pEa_SNUABM_9",                  "unknown"},
    {"Erwinia phage pEa_SNUABM_10",                 "unknown"},
    {"Erwinia phage pEa_SNUABM_29",                 "unknown"},
    {"Erwinia phage pEa_SNUABM_42",                 "unknown"},
    {"Escherichia phage C6",                        "unknown"},
    {"Escherichia phage E212",                      "unknown"},
    {"Escherichia phage P762",                      "unknown"},
    {"Escherichia phage vB_EcoA_fTuEco01",          "unknown"},
    {"Escherichia phage vB_EcoM-ZQ1",               "unknown"},
    {"Fiersviridae",                                "ssRNA(+)"},
    {"Fimoviridae",                                 "ssRNA(-)"},
    {"Flaviviridae",                                "ssRNA(+)"},
    {"Flavobacterium phage vB_FspP_elemoB_14-3B",   "unknown"},
    {"Flavobacterium phage vB_FspP_elemoC_14-1A",   "unknown"},
    {"Fusariviridae",                               "ssRNA"},
    {"Gammapleolipovirus",                          "dsDNA"},
    {"Geminiviridae",                               "ssDNA(+/-)"},
    {"Genomoviridae",                               "ssDNA"},
    {"Goukovirus",                                  "ssRNA(-)"},
    {"Hantaviridae",                                "ssRNA(-)"},
    {"Haploviricotina",                             "ssRNA(-)"},
    {"Hepadnaviridae",                              "dsDNA-RT"},
    {"Hepeviridae",                                 "ssRNA(+)"},
    {"Horwuvirus",                                  "ssRNA(-)"},
    {"Hudivirus",                                   "ssRNA(-)"},
    {"Hudovirus",                                   "ssRNA(-)"},
    {"Hypoviridae",                                 "ssRNA(+)"},
    {"Idaeovirus",                                  "ssRNA(+)"},
    {"Inoviridae",                                  "ssDNA(+)"},
    {"Insthoviricetes",                             "ssRNA(-)"},
    {"Kabutovirus",                                 "ssRNA(-)"},
    {"Kitaviridae",                                 "ssRNA(+)"},
    {"Kitrinoviricota",                             "ssRNA(+)"},
    {"Klebsiella phage Kpn-11mx",                   "unknown"},
    {"Klebsiella phage Shaphc-TDM-1124-4",          "unknown"},
    {"Lasius neglectus virus 2",                    "ssRNA(-)"},
    {"Laulavirus",                                  "ssRNA(-)"},
    {"Leishbuviridae",                              "ssRNA(-)"},
    {"Leviviridae",                                 "ssRNA(+)"},
    {"Luteoviridae",                                "ssRNA(+)"},
    {"Matonaviridae",                               "ssRNA(+)"},
    {"Megabirnaviridae",                            "dsRNA"},
    {"Microviridae",                                "ssDNA(+)"},
    {"Mitoviridae",                                 "ssRNA(+)"},
    {"Mobuvirus",                                   "ssRNA(-)"},
    {"Monodnaviria",                                "ssDNA"},
    {"Mypoviridae",                                 "ssRNA(-)"},
    {"Nairoviridae",                                "ssRNA(-)"},
    {"Nanoviridae",                                 "ssDNA(+)"},
    {"Narnaviridae",                                "ssRNA(+)"},
    {"Nidovirales",                                 "ssRNA(+)"},
    {"Nodaviridae",                                 "ssRNA(+)"},
    {"Ortervirales",                                "ssRNA-RT"},
    {"Papanivirus",                                 "ssRNA(+)"},
    {"Partitiviridae",                              "dsRNA"},
    {"Parvoviridae",                                "ssDNA(+/-)"},
    {"Peribunyaviridae",                            "ssRNA(-)"},
    {"Permutotetraviridae",                         "ssRNA(+)"},
    {"Phage CBW1004C-Prop1",                        "unknown"},
    {"Phasivirus",                                  "ssRNA(-)"},
    {"Phasmaviridae",                               "ssRNA(-)"},
    {"Phlebovirus",                                 "ssRNA(+/-)"},
    {"Picobirnaviridae",                            "dsRNA"},
    {"Picornavirales",                              "ssRNA(+)"},
    {"Pidchovirus",                                 "ssRNA(-)"},
    {"Pisoniviricetes",                             "ssRNA(+)"},
    {"Pospiviroidae",                               "ssRNA"},
    {"Potyviridae",                                 "ssRNA(+)"},
    {"Pseudomonas phage Medea1",                    "unknown"},
    {"Quadriviridae",                               "dsRNA"},
    {"Ralstonia phage RsoM2USA",                    "unknown"},
    {"Reoviridae",                                  "dsRNA"},
    {"Riboviria",                                   "RNA"},
    {"Salmonella phage LPSTLL",                     "unknown"},
    {"Salmonella phage PSDA-2",                     "unknown"},
    {"Salmonella phage vB_SentM_sal1",              "unknown"},
    {"Salmonella phage vB_SentM_sal2",              "unknown"},
    {"Salmonella phage vB_SentM_sal3",              "unknown"},
    {"Salmonella phage vB_STM-ZS",                  "unknown"},
    {"Sarthroviridae",                              "ssRNA(+)"},
    {"Sepolyvirales",                               "dsDNA"},
    {"Sinaivirus",                                  "ssRNA(+)"},
    {"Smacoviridae",                                "ssDNA"},
    {"Solemoviridae",                               "ssRNA(+)"},
    {"Solinviviridae",                              "ssRNA(+)"},
    {"Spiraviridae",                                "ssDNA(+)"},
    {"Staphylococcus phage PHB21",                  "unknown"},
    {"Stelpaviricetes",                             "ssRNA(+)"},
    {"Stenotrophomonas phage BUCT603",              "unknown"},
    {"Stenotrophomonas phage BUCT609",              "unknown"},
    {"Tenuivirus",                                  "ssRNA(-)"},
    {"Thomixvirus",                                 "ssDNA(+)"},
    {"Togaviridae",                                 "ssRNA(+)"},
    {"Tolecusatellitidae",                          "ssDNA"},
    {"Tombusviridae",                               "ssRNA(+)"},
    {"Tospoviridae",                                "ssRNA(+/-)"},
    {"Totiviridae",                                 "dsRNA"},
    {"Tymovirales",                                 "ssRNA(+)"},
    {"Uukuvirus",                                   "ssRNA(-)"},
    {"Virgaviridae",                                "ssRNA(+)"},
    {"Virtovirus",                                  "ssRNA(+)"},
    {"Wenrivirus",                                  "ssRNA(-)"},
    {"Wubeivirus",                                  "ssRNA(-)"},
    {"Wupedeviridae",                               "ssRNA(-)"},
    {"Zurhausenvirales",                            "dsDNA"},
    {"DNA molecule",                                "DNA"},
    {"DNA satellites",                              "DNA"},
    {"dsRNA viruses",                               "dsRNA"},
    {"environmental samples",                       "unknown"},
    {"RNA satellites",                              "RNA"},
    {"ssRNA viruses",                               "ssRNA"},
    {"unclassified archaeal dsDNA viruses",         "dsDNA"},
    {"unclassified bacterial viruses",              "unknown"},
    {"unclassified DNA viruses",                    "DNA"},
    {"unclassified dsDNA phages",                   "dsDNA"},
    {"unclassified dsDNA viruses",                  "dsDNA"},
    {"unclassified ssDNA bacterial viruses",        "ssDNA"},
    {"unclassified ssDNA viruses",                  "ssDNA"},
    {"unclassified ssRNA negative-strand viruses",  "ssRNA(-)"},
    {"unclassified ssRNA positive-strand viruses",  "ssRNA(+)"},
    {"unclassified ssRNA viruses",                  "ssRNA"},
    {"unclassified viroids",                        "ssRNA"},
    {"unclassified viruses",                        "unknown"},
});

static string s_GetStrandedMolStringFromLineage(const string& lineage)
{
    // Retroviridae no longer in list
    if (NStr::FindNoCase(lineage, "Retroviridae") != NPOS) {
        return "ssRNA-RT";
    }

    // Topsovirus (ambisense) not in list
    if (NStr::FindNoCase(lineage, "Tospovirus") != NPOS) {
        return "ssRNA(+/-)";
    }

    // Tenuivirus has several segments, most of which are ambisense
    if (NStr::FindNoCase(lineage, "Tenuivirus") != NPOS) {
        return "ssRNA(+/-)";
    }

    // Arenaviridae is ambisense, has priority over old-style checks
    if (NStr::FindNoCase(lineage, "Arenaviridae") != NPOS) {
        return "ssRNA(+/-)";
    }

    // Phlebovirus is ambisense, has priority over old-style checks
    if (NStr::FindNoCase(lineage, "Phlebovirus") != NPOS) {
        return "ssRNA(+/-)";
    }

    // unclassified viruses have old-style lineage
    if (NStr::FindNoCase(lineage, "negative-strand viruses") != NPOS) {
        return "ssRNA(-)";
    }
    if (NStr::FindNoCase(lineage, "positive-strand viruses") != NPOS) {
        return "ssRNA(+)";
    }

    for (const auto& x : kViralStrandMap) {
        if (NStr::FindNoCase(lineage, x.first) != NPOS) {
            return x.second;
        }
    }

    // use root value for default
    return "dsDNA";
}

static CMolInfo::TBiomol s_CheckSingleStrandedRNAViruses(
    const string& lineage,
    const string& division,
    CBioSource::TOrigin origin,
    const string& stranded_mol,
    const CMolInfo::TBiomol biomol,
    const CBioseq_Handle& bsh
)
{
    if (NStr::FindNoCase(stranded_mol, "ssRNA") == NPOS) {
        return CMolInfo::eBiomol_unknown;
    }
    if (NStr::FindNoCase(stranded_mol, "DNA") != NPOS) {
        return CMolInfo::eBiomol_unknown;
    }

    const bool is_ambisense = NStr::EqualNocase(stranded_mol, "ssRNA(+/-)");

    // special cases
    if (is_ambisense) {
        return CMolInfo::eBiomol_unknown;
    }

    if (NStr::FindNoCase(lineage, "Retroviridae") != NPOS && NStr::EqualNocase(stranded_mol, "ssRNA-RT")) {
        return CMolInfo::eBiomol_unknown;
    }

    bool negative_strand_virus = false;
    bool plus_strand_virus = false;
    if (NStr::FindNoCase(stranded_mol, "-)") != NPOS) {
        negative_strand_virus = true;
    }
    if (NStr::FindNoCase(stranded_mol, "(+") != NPOS) {
        plus_strand_virus = true;
    }
    if (! negative_strand_virus && ! plus_strand_virus) {
        return CMolInfo::eBiomol_unknown;
    }

    bool is_synthetic = false;
    if (NStr::EqualNocase(division, "SYN")) {
        is_synthetic = true;
    } else if (origin == CBioSource::eOrigin_mut
        || origin == CBioSource::eOrigin_artificial
        || origin == CBioSource::eOrigin_synthetic) {
        is_synthetic = true;
    }

    bool has_cds = false;
    bool has_plus_cds = false;
    bool has_minus_cds = false;

    CFeat_CI cds_ci(bsh, SAnnotSelector(CSeqFeatData::e_Cdregion));
    while (cds_ci) {
        has_cds = true;
        if (cds_ci->GetLocation().GetStrand() == eNa_strand_minus) {
            has_minus_cds = true;
        } else {
            has_plus_cds = true;
        }
        if (has_minus_cds && has_plus_cds) {
            break;
        }

        ++cds_ci;
    }

    bool has_minus_misc_feat = false;
    bool has_plus_misc_feat = false;

    if (! has_cds) {
        CFeat_CI misc_ci(bsh, SAnnotSelector(CSeqFeatData::eSubtype_misc_feature));
        while (misc_ci) {
            if (misc_ci->IsSetComment()
                && NStr::FindNoCase(misc_ci->GetComment(), "nonfunctional") != NPOS) {
                if (misc_ci->GetLocation().GetStrand() == eNa_strand_minus) {
                    has_minus_misc_feat = true;
                } else {
                    has_plus_misc_feat = true;
                }
            }
            if (has_minus_misc_feat && has_plus_misc_feat) {
                break;
            }
            ++misc_ci;
        }
    }

    if (negative_strand_virus) {

        if (has_minus_cds) {
            if (biomol != CMolInfo::eBiomol_genomic) {
                // cerr << "Negative-sense single-stranded RNA virus with minus strand CDS should be genomic RNA" << endl;
                return CMolInfo::eBiomol_genomic;
            }
        }

        if (has_plus_cds && ! is_synthetic && ! is_ambisense) {
            if (biomol != CMolInfo::eBiomol_cRNA) {
                // cerr << "Negative-sense single-stranded RNA virus with plus strand CDS should be cRNA" << endl;
                return CMolInfo::eBiomol_cRNA;
            }
        }

        if (has_minus_misc_feat) {
            if (biomol != CMolInfo::eBiomol_genomic) {
                // cerr << "Negative-sense single-stranded RNA virus with nonfunctional minus strand misc_feature should be genomic RNA" << endl;
                return CMolInfo::eBiomol_genomic;
            }
        }

        if (has_plus_misc_feat && ! is_synthetic && ! is_ambisense) {
            if (biomol != CMolInfo::eBiomol_cRNA) {
                // cerr << "Negative-sense single-stranded RNA virus with nonfunctional plus strand misc_feature should be cRNA" << endl;
                return CMolInfo::eBiomol_cRNA;
            }
        }
    }

    if (plus_strand_virus) {

        if (! is_synthetic && ! is_ambisense) {
            if (biomol != CMolInfo::eBiomol_genomic) {
                // cerr << "Positive-sense single-stranded RNA virus should be genomic RNA" << endl;
                return CMolInfo::eBiomol_genomic;
            }
        }
    }

    return CMolInfo::eBiomol_unknown;
}

static CMolInfo::TBiomol s_ReportLineageConflictWithMol(
    const string& lineage,
    const string& stranded_mol,
    const CMolInfo::TBiomol biomol,
    CSeq_inst::EMol mol
)
{
    if (mol != CSeq_inst::eMol_rna && mol != CSeq_inst::eMol_dna)  {
        return CMolInfo::eBiomol_unknown;
    }

    // special cases
    if (NStr::FindNoCase(lineage, "Retroviridae") != NPOS && NStr::EqualNocase(stranded_mol, "ssRNA-RT")) {
        // retrovirus can be rna or dna
        return CMolInfo::eBiomol_unknown;
    }

    if (NStr::EqualNocase(stranded_mol, "dsRNA")) {
        if (biomol != CMolInfo::eBiomol_genomic) {
            // cerr << "dsRNA virus should be genomic RNA" << endl;
            return CMolInfo::eBiomol_genomic;
        }
    }

    return CMolInfo::eBiomol_unknown;
}

void CCleanup::FixViralMolInfo(CSeq_entry_Handle seh)
{
    for (CBioseq_CI b(seh); b; ++b) {
        if (b->IsAa()) {
            continue;
        }
        const CBioseq_Handle& bsh = *b;
        CConstRef<CBioseq> bsr = bsh.GetCompleteBioseq();

        string lineage = "";
        string division = "";
        CBioSource::TOrigin origin = CBioSource::eOrigin_unknown;

        CConstRef<CSeqdesc> closest_biosource = bsr->GetClosestDescriptor(CSeqdesc::e_Source);
        if (! closest_biosource) {
            continue;
        }
        const CBioSource& src = closest_biosource->GetSource();
        if (! src.IsSetOrg() || ! src.GetOrg().IsSetLineage()) {
            continue;
        }

        lineage = src.GetOrg().GetLineage();
        if (! NStr::StartsWith(lineage, "Viruses; ")) {
            continue;
        }

        division = src.GetOrg().GetDivision();
        if (src.IsSetOrigin()) {
            origin = src.GetOrigin();
        }

        CMolInfo::TBiomol biomol = CMolInfo::eBiomol_unknown;

        CConstRef<CSeqdesc> closest_molinfo = bsr->GetClosestDescriptor(CSeqdesc::e_Molinfo);
        if (! closest_molinfo) {
            continue;
        }
        const CMolInfo& molinf = closest_molinfo->GetMolinfo();
        if (molinf.IsSetBiomol()) {
            biomol = molinf.GetBiomol();
        }

        CSeq_inst::EMol mol = CSeq_inst::eMol_not_set;

        if (bsh.IsSetInst_Mol()) {
            mol = bsh.GetInst_Mol();
        }

        string stranded_mol = s_GetStrandedMolStringFromLineage(lineage);
        if (NStr::FindNoCase(stranded_mol, "unknown") != NPOS) {
            continue;
        }

        CMolInfo::TBiomol newbiomol = s_CheckSingleStrandedRNAViruses(lineage, division, origin, stranded_mol, biomol, bsh);

        if (newbiomol != CMolInfo::eBiomol_unknown) {
            CRef<CSeqdesc> new_desc(new CSeqdesc);
            new_desc->Assign(*closest_molinfo);
            new_desc->SetMolinfo().SetBiomol(newbiomol);
            bsh.GetEditHandle().ReplaceSeqdesc(*closest_molinfo, *new_desc);
            // s_ReportLineageConflictWithMol, below, only affects double stranded molecules, not the case here, so okay to skip.
            continue;
        }

        newbiomol = s_ReportLineageConflictWithMol(lineage, stranded_mol, biomol, mol);

        if (newbiomol != CMolInfo::eBiomol_unknown) {
            CRef<CSeqdesc> new_desc(new CSeqdesc);
            new_desc->Assign(*closest_molinfo);
            new_desc->SetMolinfo().SetBiomol(newbiomol);
            bsh.GetEditHandle().ReplaceSeqdesc(*closest_molinfo, *new_desc);
        }
    }
}

char CCleanup::ValidAminoAcid(string_view abbrev)
{
    return x_ValidAminoAcid(abbrev);
}

END_SCOPE(objects)
END_NCBI_SCOPE
