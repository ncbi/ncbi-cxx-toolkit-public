#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/object_manager.hpp>

#include <objmgr/seq_entry_ci.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/general/Object_id.hpp>

//#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/edit/protein_match/prot_match_exception.hpp>
#include <objtools/edit/protein_match/setup_match.hpp>
#include <serial/iterator.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CMatchSetup::CMatchSetup(CRef<CScope> db_scope) : m_DBScope(db_scope)
{
}


void CMatchSetup::GetNucProtSets(
    CRef<CSeq_entry> input_entry,
    list<CRef<CSeq_entry>>& nuc_prot_sets)
{
    if (!input_entry->IsSet()) {
        return;
    }

    for (CTypeIterator<CSeq_entry> it(*input_entry); it; ++it) {
        if (it->IsSet() &&
            it->GetSet().IsSetClass() &&
            it->GetSet().GetClass() == CBioseq_set::eClass_nuc_prot) {
            nuc_prot_sets.push_back(Ref(&*it));
        }
    }

}


CBioseq& CMatchSetup::SetNucSeq(CRef<CSeq_entry> nuc_prot_set) {

    NON_CONST_ITERATE(CBioseq_set::TSeq_set, seq_it, nuc_prot_set->SetSet().SetSeq_set()) {
        CSeq_entry& se = **seq_it;
        if (se.IsSeq() && se.GetSeq().IsNa()) {
            return se.SetSeq();
        }
    }

    NCBI_THROW(CProteinMatchException, 
        eInputError,
        "No nucleotide sequence found");
}


CConstRef<CBioseq_set> CMatchSetup::GetDBNucProtSet(const CBioseq& nuc_seq) 
{
    for (auto pNucId : nuc_seq.GetId()) {
        if (pNucId->IsGenbank()) {
            CBioseq_Handle db_bsh = m_DBScope->GetBioseqHandle(*pNucId);
            if (!db_bsh) {
                NCBI_THROW(CProteinMatchException, 
                    eInputError,
                    "Failed to fetch DB entry");
            }
            return db_bsh.GetCompleteBioseq()->GetParentSet();
        }
    }
    return ConstRef(new CBioseq_set());
}


struct SIdCompare
{
    bool operator()(const CRef<CSeq_id>& id1,
        const CRef<CSeq_id>& id2) const 
    {
        return id1->CompareOrdered(*id2) < 0;
    }
};


bool CMatchSetup::GetNucSeqIdFromCDSs(const CSeq_entry& nuc_prot_set,
    CRef<CSeq_id>& id)
{
    // Set containing distinct ids
    set<CRef<CSeq_id>, SIdCompare> ids;

    for (CTypeConstIterator<CSeq_feat> feat(nuc_prot_set); feat; ++feat) 
    {
        if (!feat->GetData().IsCdregion()) {
            continue;
        }

        CRef<CSeq_id> nucseq_id = Ref(new CSeq_id());
        nucseq_id->Assign(*(feat->GetLocation().GetId()));
        // Throw an exception if null pointer
        ids.insert(nucseq_id);
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


bool CMatchSetup::UpdateNucSeqIds(CRef<CSeq_id> new_id,
        CRef<CSeq_entry> nuc_prot_set)
{
    if (!nuc_prot_set->IsSet()) {
        return false;
    }

    CBioseq& nuc_seq = SetNucSeq(nuc_prot_set);

    nuc_seq.ResetId();
    nuc_seq.SetId().push_back(new_id);

    CSeq_entry& nuc_prot_se = nuc_prot_set.GetNCObject();

    for (CTypeIterator<CSeq_feat> feat(nuc_prot_se); feat; ++feat) {
        if (!feat->GetData().IsCdregion()) {
            continue;
        }
        feat->SetLocation().SetId(*new_id);
    }

    return true;   
}



END_SCOPE(objects)
END_NCBI_SCOPE
