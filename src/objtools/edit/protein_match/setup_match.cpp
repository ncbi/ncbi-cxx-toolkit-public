#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/object_manager.hpp>

#include <objmgr/seq_entry_ci.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/general/Object_id.hpp>

#include <objtools/edit/protein_match/prot_match_exception.hpp>
#include <objtools/edit/protein_match/setup_match.hpp>
#include <serial/iterator.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CMatchSetup::CMatchSetup(CRef<CScope> db_scope) : m_DBScope(db_scope)
{
}


void CMatchSetup::GatherNucProtSets(
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


CBioseq& CMatchSetup::x_FetchNucSeqRef(CSeq_entry& nuc_prot_set) const
{

    NON_CONST_ITERATE(CBioseq_set::TSeq_set, seq_it, nuc_prot_set.SetSet().SetSeq_set()) {
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
        if (pNucId->IsGenbank() || pNucId->IsOther()) {  // Look at GetBioseqHandle
            CBioseq_Handle db_bsh = m_DBScope->GetBioseqHandle(*pNucId);
            if (!db_bsh) {
                NCBI_THROW(CProteinMatchException, 
                    eInputError,
                    "Failed to fetch DB entry");
            }
            // HasParentSet
            //return db_bsh.GetCompleteBioseq()->GetParentSet(); 
            if (db_bsh.GetParentBioseq_set()) {
                return db_bsh.GetParentBioseq_set().GetCompleteBioseq_set();
            }
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
    CRef<CSeq_id>& id) const
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


bool CMatchSetup::UpdateNucSeqIds(CRef<CSeq_id> new_id,
    CSeq_entry& nuc_prot_set) const
{
    if (nuc_prot_set.IsSet()) {
        return false;
    }

    CBioseq& nuc_seq = x_FetchNucSeqRef(nuc_prot_set);
    nuc_seq.ResetId();
    nuc_seq.SetId().push_back(new_id);

    for (CTypeIterator<CSeq_feat> feat(nuc_prot_set); feat; ++feat) {
        if (feat->GetData().IsCdregion()) {
            feat->SetLocation().SetId(*new_id);
        }
    }

    return true;   
}


END_SCOPE(objects)
END_NCBI_SCOPE
