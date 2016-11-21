#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/object_manager.hpp>

#include <objmgr/seq_entry_ci.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/general/Object_id.hpp>

#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/edit/protein_match/prot_match_exception.hpp>
#include <objtools/edit/protein_match/setup_match.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CMatchSetup::CMatchSetup() {
    CRef<CObjectManager> obj_mgr = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*obj_mgr);
    m_GBScope = Ref(new CScope(*obj_mgr));
    m_GBScope->AddDataLoader("GBLOADER");
}


void CMatchSetup::GetNucProtSets(
    CSeq_entry_Handle seh,
    list<CSeq_entry_Handle>& nuc_prot_sets) 
{
    if (!seh.IsSet()) {
        return;
    }

    // Use ITERATE macros everywhere - or ordinary loop
    for (CSeq_entry_CI it(seh, CSeq_entry_CI::fRecursive); it; ++it) {
        if (it->IsSet() &&
            it->GetSet().IsSetClass() &&
            it->GetSet().GetClass() == CBioseq_set::eClass_nuc_prot) {
            nuc_prot_sets.push_back(*it);
        }
    }
}



CSeq_entry_Handle CMatchSetup::GetNucleotideSEH(CSeq_entry_Handle seh) const
{
    if (seh.IsSeq()) {
        const CMolInfo* molinfo = sequence::GetMolInfo(seh.GetSeq());
        if (molinfo->GetBiomol() != CMolInfo::eBiomol_peptide) {
            return seh;
        }
    }

    // Same thing - Don't use CSeq_entry_CI
    for (CSeq_entry_CI it(seh, CSeq_entry_CI::fRecursive); it; ++it) {
        if (it->IsSeq()) {
            const CMolInfo* molinfo = sequence::GetMolInfo(it->GetSeq());
            if (molinfo->GetBiomol() != CMolInfo::eBiomol_peptide) {
                return *it;
            }
        }
    }
    return CSeq_entry_Handle();
}


CSeq_entry_Handle CMatchSetup::GetGenBankTopLevelEntry(CSeq_entry_Handle nucleotide_seh)  
{
    CBioseq_Handle gb_bsh;
    for (auto pNucId : nucleotide_seh.GetSeq().GetCompleteBioseq()->GetId()) {
        if (pNucId->IsGenbank()) {
            gb_bsh = m_GBScope->GetBioseqHandle(*pNucId);
            if (!gb_bsh) {
                NCBI_THROW(CProteinMatchException, 
                    eInputError,
                    "Failed to fetch GenBank entry");
            }
            return gb_bsh.GetTopLevelEntry(); // GetParentBioseqSet() 
        }
    }

    CSeq_entry_Handle empty;
    return empty;
}


CConstRef<CBioseq_set> CMatchSetup::GetGenBankNucProtSet(const CBioseq& nuc_seq) 
{
    for (auto pNucId : nuc_seq.GetId()) {
        if (pNucId->IsGenbank()) {
            CBioseq_Handle gb_bsh = m_GBScope->GetBioseqHandle(*pNucId);
            if (!gb_bsh) {
                NCBI_THROW(CProteinMatchException, 
                    eInputError,
                    "Failed to fetch GenBank entry");
            }
            return gb_bsh.GetCompleteBioseq()->GetParentSet();
        }
    }
    return ConstRef(new CBioseq_set());
}


struct SEquivalentTo 
{
    CRef<CSeq_id> sid;
    
    SEquivalentTo(CRef<CSeq_id>& id) : sid(Ref(new CSeq_id()))
    {
        sid->Assign(*id);
    }

    bool operator()(const CRef<CSeq_id>& id) const
    {
        return (id->Compare(*sid) == CSeq_id::e_YES);
    }
};


bool CMatchSetup::GetNucSeqIdFromCDSs(const CSeq_entry& nuc_prot_set,
    CScope& scope,
    CRef<CSeq_id>& id)
{
    CSeq_entry_Handle nuc_prot_handle = scope.GetObjectHandle(nuc_prot_set);
    return GetNucSeqIdFromCDSs(nuc_prot_handle, id);
}


bool CMatchSetup::GetNucSeqIdFromCDSs(CSeq_entry_Handle& seh, CRef<CSeq_id>& id) 
{
    // Set containing distinct ids
    set<CRef<CSeq_id>> ids;

    SAnnotSelector sel(CSeqFeatData::e_Cdregion);

    for(CFeat_CI feature_it(seh, sel); feature_it; ++feature_it) {
        CRef<CSeq_id> nucseq_id = Ref(new CSeq_id());
        nucseq_id->Assign(*(feature_it->GetLocation().GetId()));

        if(ids.empty() || // Change this!
           find_if(ids.begin(), ids.end(), SEquivalentTo(nucseq_id)) == ids.end()) {
           ids.insert(nucseq_id);
        }
    }

    if (ids.size() > 1) {
        NCBI_THROW(CProteinMatchException, 
            eBadInput, 
            "Multiple CDS location ids found");
    }

    if (ids.empty()) { // Change this - make sure there's some check
        return false;
    }

    CRef<CSeq_id> nucseq_id = *(ids.begin());
    const bool with_version = true;

    const string local_id_string = nucseq_id->GetSeqIdString(with_version);

    if (id.IsNull()) {
        id = Ref(new CSeq_id());
    }
    id->SetLocal().SetStr(local_id_string);

    return true;
}


// Strip old identifiers on the sequence and annotations and replace with new_id
bool CMatchSetup::UpdateNucSeqIds(CRef<CSeq_id>& new_id,
        CSeq_entry_Handle& nucleotide_seh,
        CSeq_entry_Handle& nuc_prot_seh)
{
    if (!nucleotide_seh.IsSeq() || 
        !nuc_prot_seh.IsSet()) {
        return false;
    }
    CBioseq_EditHandle bseh = nucleotide_seh.GetSeq().GetEditHandle();

    bseh.ResetId(); // remove the old sequence identifiers
    CSeq_id_Handle new_idh = CSeq_id_Handle::GetHandle(*new_id);
    bseh.AddId(new_idh); // Add the new sequence id

    SAnnotSelector sel(CSeqFeatData::e_Cdregion);
    for (CFeat_CI feature_it(nuc_prot_seh, sel); feature_it; ++feature_it) {
        CRef<CSeq_feat> new_sf = Ref(new CSeq_feat());
        new_sf->Assign(feature_it->GetOriginalFeature());
        new_sf->SetLocation().SetId(*new_id);

        CSeq_feat_EditHandle sfeh(feature_it->GetSeq_feat_Handle());
        sfeh.Replace(*new_sf);
    }
    return true;
}

END_SCOPE(objects)
END_NCBI_SCOPE
