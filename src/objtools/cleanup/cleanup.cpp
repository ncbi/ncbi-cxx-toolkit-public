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
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/general/User_object.hpp>
#include <objects/submit/Seq_submit.hpp>


#include <objmgr/util/sequence.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/seq_annot_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objtools/cleanup/cleanup.hpp>

#include "newcleanupp.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

enum EChangeType {
    eChange_UNKNOWN
};

// *********************** CCleanup implementation **********************


CCleanup::CCleanup(CScope* scope) : m_Scope( scope )
{
}


CCleanup::~CCleanup(void)
{
}


void CCleanup::SetScope(CScope* scope)
{
    m_Scope = scope;
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

CConstRef<CCleanupChange> CCleanup::BasicCleanup(CSeq_entry& se, Uint4 options)
{
    CRef<CCleanupChange> changes(makeCleanupChange(options));
    CNewCleanup_imp clean_i(changes, options);
    clean_i.BasicCleanupSeqEntry(se);
    return changes;
}


CConstRef<CCleanupChange> CCleanup::BasicCleanup(CSeq_submit& ss, Uint4 options)
{
    CRef<CCleanupChange> changes(makeCleanupChange(options));
    CNewCleanup_imp clean_i(changes, options);
    clean_i.BasicCleanupSeqSubmit(ss);
    return changes;
}


/// Cleanup a Bioseq. 
CConstRef<CCleanupChange> CCleanup::BasicCleanup(CBioseq& bs, Uint4 options)
{
    CRef<CCleanupChange> changes(makeCleanupChange(options));
    CNewCleanup_imp clean_i(changes, options);
    clean_i.BasicCleanupBioseq(bs);
    return changes;
}


CConstRef<CCleanupChange> CCleanup::BasicCleanup(CBioseq_set& bss, Uint4 options)
{
    CRef<CCleanupChange> changes(makeCleanupChange(options));
    CNewCleanup_imp clean_i(changes, options);
    clean_i.BasicCleanupBioseqSet(bss);
    return changes;
}


CConstRef<CCleanupChange> CCleanup::BasicCleanup(CSeq_annot& sa, Uint4 options)
{
    CRef<CCleanupChange> changes(makeCleanupChange(options));
    CNewCleanup_imp clean_i(changes, options);
    clean_i.BasicCleanupSeqAnnot(sa);
    return changes;
}


CConstRef<CCleanupChange> CCleanup::BasicCleanup(CSeq_feat& sf, Uint4 options)
{
    CRef<CCleanupChange> changes(makeCleanupChange(options));
    CNewCleanup_imp clean_i(changes, options);
    clean_i.BasicCleanupSeqFeat(sf);
    return changes;
}


CConstRef<CCleanupChange> CCleanup::BasicCleanup(CBioSource& src, Uint4 options)
{
    CRef<CCleanupChange> changes(makeCleanupChange(options));
    CNewCleanup_imp clean_i(changes, options);
    clean_i.BasicCleanupBioSource(src);
    return changes;
}


CConstRef<CCleanupChange> CCleanup::BasicCleanup(CSeq_entry_Handle& seh, Uint4 options)
{
    CRef<CCleanupChange> changes(makeCleanupChange(options));
    CNewCleanup_imp clean_i(changes, options);
    clean_i.BasicCleanupSeqEntryHandle(seh);
    return changes;
}


CConstRef<CCleanupChange> CCleanup::BasicCleanup(CBioseq_Handle& bsh,    Uint4 options)
{
    CRef<CCleanupChange> changes(makeCleanupChange(options));
    CNewCleanup_imp clean_i(changes, options);
    clean_i.BasicCleanupBioseqHandle(bsh);
    return changes;
}


CConstRef<CCleanupChange> CCleanup::BasicCleanup(CBioseq_set_Handle& bssh, Uint4 options)
{
    CRef<CCleanupChange> changes(makeCleanupChange(options));
    CNewCleanup_imp clean_i(changes, options);
    clean_i.BasicCleanupBioseqSetHandle(bssh);
    return changes;
}


CConstRef<CCleanupChange> CCleanup::BasicCleanup(CSeq_annot_Handle& sah, Uint4 options)
{
    CRef<CCleanupChange> changes(makeCleanupChange(options));
    CNewCleanup_imp clean_i(changes, options);
    clean_i.BasicCleanupSeqAnnotHandle(sah);
    return changes;
}


CConstRef<CCleanupChange> CCleanup::BasicCleanup(CSeq_feat_Handle& sfh,  Uint4 options)
{
    CRef<CCleanupChange> changes(makeCleanupChange(options));
    CNewCleanup_imp clean_i(changes, options);
    clean_i.BasicCleanupSeqFeatHandle(sfh);
    return changes;
}





// *********************** Extended Cleanup implementation ********************
CConstRef<CCleanupChange> CCleanup::ExtendedCleanup(CSeq_entry& se,  Uint4 options)
{
    CRef<CCleanupChange> changes(makeCleanupChange(options));
    CNewCleanup_imp clean_i(changes, options);
    clean_i.ExtendedCleanupSeqEntry(se);
    
    return changes;
}


CConstRef<CCleanupChange> CCleanup::ExtendedCleanup(CSeq_submit& ss,  Uint4 options)
{
    CRef<CCleanupChange> changes(makeCleanupChange(options));
    CNewCleanup_imp clean_i(changes, options);
    clean_i.ExtendedCleanupSeqSubmit(ss);
    return changes;
}


CConstRef<CCleanupChange> CCleanup::ExtendedCleanup(CSeq_annot& sa,  Uint4 options)
{
    CRef<CCleanupChange> changes(makeCleanupChange(options));
    CNewCleanup_imp clean_i(changes, options);
    clean_i.ExtendedCleanupSeqAnnot(sa); // (m_Scope->GetSeq_annotHandle(sa));
    return changes;
}

CConstRef<CCleanupChange> CCleanup::ExtendedCleanup(CSeq_entry_Handle& seh,  Uint4 options)
{
    CRef<CCleanupChange> changes(makeCleanupChange(options));
    CNewCleanup_imp clean_i(changes, options);
    clean_i.ExtendedCleanupSeqEntryHandle(seh); // (m_Scope->GetSeq_annotHandle(sa));
    return changes;
}


// *********************** CCleanupChange implementation **********************


CCleanupChange::CCleanupChange()
{
}


size_t CCleanupChange::ChangeCount() const
{
    return m_Changes.count();
}


bool CCleanupChange::IsChanged(CCleanupChange::EChanges e) const
{
    return m_Changes.test(e);
}


void CCleanupChange::SetChanged(CCleanupChange::EChanges e)
{
    m_Changes.set(e);
}


vector<CCleanupChange::EChanges> CCleanupChange::GetAllChanges() const
{
    vector<EChanges>  result;
    for (size_t i = eNoChange + 1; i < m_Changes.size(); ++i) {
        if (m_Changes.test(i)) {
            result.push_back( (EChanges) i);
        }
    }
    return result;
}


vector<string> CCleanupChange::GetAllDescriptions() const
{
    vector<string>  result;
    for (size_t i = eNoChange + 1; i < m_Changes.size(); ++i) {
        if (m_Changes.test(i)) {
            result.push_back( GetDescription((EChanges) i) );
        }
    }
    return result;
}


string CCleanupChange::GetDescription(EChanges e)
{
    if (e <= eNoChange  ||  e >= eNumberofChangeTypes) {
        return sm_ChangeDesc[eNoChange];
    }
    return sm_ChangeDesc[e];
}

// corresponds to the values in CCleanupChange::EChanges.
// They must be edited together.
const char* const CCleanupChange::sm_ChangeDesc[eNumberofChangeTypes + 1] = {
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

    // set when any other change is made.
    "Change Other", 
    "Invalid Change Code"
};


bool CCleanup::MoveProteinSpecificFeats(CSeq_entry_Handle seh)
{
    bool any_change = false;
    SAnnotSelector sel(CSeqFeatData::e_Prot);
    sel.IncludeFeatType(CSeqFeatData::e_Psec_str);
    for (CFeat_CI prot_it(seh, sel); prot_it; ++prot_it) {
        CBioseq_Handle parent_bsh = seh.GetScope().GetBioseqHandle(prot_it->GetLocation());

        if (!parent_bsh) {
            // protein feature is mispackaged
            continue;
        }
        if (parent_bsh.IsAa()) {
            // protein feature is already on protein sequence
            continue;
        }

        CConstRef<CSeq_feat> cds = sequence::GetOverlappingCDS(prot_it->GetLocation(), seh.GetScope());
        if (!cds || !cds->IsSetProduct()) {
            // there is no overlapping coding region feature, so there is no appropriate
            // protein sequence to move to
            continue;
        }

        CSeq_feat_Handle cds_h = seh.GetScope().GetSeq_featHandle(*cds);
        if (!cds_h) {
            // can't get handle
            continue;
        }

        if (feature::IsLocationInFrame(cds_h, prot_it->GetLocation()) != feature::eLocationInFrame_InFrame) {
            // not in frame, can't convert
            continue;
        }

        CConstRef<CSeq_feat> orig_feat = prot_it->GetSeq_feat();
        CRef<CSeq_feat> new_feat(new CSeq_feat());
        new_feat->Assign(*orig_feat);
        CRef<CSeq_loc> new_loc;
        CRef<CSeq_loc_Mapper> nuc2prot_mapper(
            new CSeq_loc_Mapper(*cds, CSeq_loc_Mapper::eLocationToProduct, &seh.GetScope()) );
        new_loc = nuc2prot_mapper->Map(orig_feat->GetLocation());
        if (!new_loc || new_loc->GetId()->Equals(*(orig_feat->GetLocation().GetId()))) {
            // unable to map to protein location
            continue;
        }
        new_loc->GetStart(eExtreme_Biological);
        new_loc->GetStop(eExtreme_Biological);

        // change location to protein
        new_feat->ResetLocation();
        new_feat->SetLocation(*new_loc);

        // remove the feature from the nuc bioseq
        CSeq_feat_Handle fh = seh.GetScope().GetSeq_featHandle(*orig_feat);
        CSeq_feat_EditHandle edh(fh);
        edh.Remove();

        CBioseq_Handle target_bsh = seh.GetScope().GetBioseqHandle(new_feat->GetLocation());
        CBioseq_EditHandle eh = target_bsh.GetEditHandle();

        // Find a feature table on the protein sequence to add the feature to.       
        CSeq_annot_Handle ftable;
        CSeq_annot_CI annot_ci(target_bsh);
        for (; annot_ci; ++annot_ci) {
            if ((*annot_ci).IsFtable()) {
                ftable = *annot_ci;
                break;
            }
        }
        // If there is no feature table present, make one
        if (!ftable) {
            CRef<CSeq_annot> new_annot(new CSeq_annot());
            ftable = eh.AttachAnnot(*new_annot);
        }

        // add feature to the protein bioseq
        CSeq_annot_EditHandle aeh(ftable);
        aeh.AddFeat(*new_feat);
        any_change = true;
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

    return gene->GetData().GetGene().RefersToSameGene(gene_xref);    
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


bool CCleanup::AddMissingMolInfo(CBioseq& seq, bool is_product)
{
    if (!seq.IsSetInst() || !seq.GetInst().IsSetMol()) {
        return false;
    }
    bool needs_molinfo = true;

    if (seq.IsSetDescr()) {
        NON_CONST_ITERATE(CBioseq::TDescr::Tdata, it, seq.SetDescr().Set()) {
            if ((*it)->IsMolinfo()) {
                needs_molinfo = false;
                if (seq.IsAa() && 
                    (!(*it)->GetMolinfo().IsSetBiomol() || 
                     (*it)->GetMolinfo().GetBiomol() == CMolInfo::eBiomol_unknown)) {
                    (*it)->SetMolinfo().SetBiomol(CMolInfo::eBiomol_peptide);
                }
            }
        }
    }
    if (needs_molinfo) {
        if (seq.IsAa()) {
            CRef<CSeqdesc> m(new CSeqdesc());
            m->SetMolinfo().SetBiomol(CMolInfo::eBiomol_peptide);
            if (is_product) {
                m->SetMolinfo().SetTech(CMolInfo::eTech_concept_trans);
            }
            seq.SetDescr().Set().push_back(m);
        } else if (seq.GetInst().GetMol() == CSeq_inst::eMol_rna && is_product) {
            CRef<CSeqdesc> m(new CSeqdesc());
            m->SetMolinfo().SetBiomol(CMolInfo::eBiomol_mRNA);
            m->SetMolinfo().SetTech(CMolInfo::eTech_standard);
            seq.SetDescr().Set().push_back(m);
        }
    }

    return needs_molinfo;
}


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
    CSeqdesc_CI di(bsh, CSeqdesc::e_Title);
    if (di) {
        if (sequence::GetCDSForProduct(*(bsh.GetCompleteBioseq()), &(bsh.GetScope())) != NULL
            && !NStr::Equal(di->GetTitle(), new_defline)) {
            CSeqdesc* d = const_cast<CSeqdesc*>(&(*di));
            d->SetTitle(new_defline);
            return true;
        } else {
            return false;
        }
    }

    CRef<CSeqdesc> t(new CSeqdesc());
    t->SetTitle(sequence::CDeflineGenerator().GenerateDefline(bsh));
    CBioseq_EditHandle eh = bsh.GetEditHandle();
    eh.AddSeqdesc(*t);
    return true;
}


bool CCleanup::RemoveNcbiCleanupObject(CSeq_entry &seq_entry)
{
    bool rval = false;
    if (seq_entry.IsSetDescr()) {
        CBioseq::TDescr::Tdata::iterator it = seq_entry.SetDescr().Set().begin();
        while (it != seq_entry.SetDescr().Set().end()) {
            if ((*it)->IsUser() && (*it)->GetUser().GetObjectType() == CUser_object::eObjectType_Cleanup){
                it = seq_entry.SetDescr().Set().erase(it);
                rval = true;
            }
            else {
                ++it;
            }
        }
        if (seq_entry.SetDescr().Set().empty()) {
            if (seq_entry.IsSeq()) {
                seq_entry.SetSeq().ResetDescr();
            }
            else if (seq_entry.IsSet()) {
                seq_entry.SetSet().ResetDescr();
            }
        }
    }
    if (seq_entry.IsSet() && seq_entry.GetSet().IsSetSeq_set()) {
        NON_CONST_ITERATE(CBioseq_set::TSeq_set, it, seq_entry.SetSet().SetSeq_set()) {
            rval |= RemoveNcbiCleanupObject(**it);
        }
    }
    return rval;
}

END_SCOPE(objects)
END_NCBI_SCOPE
