#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/object_manager.hpp>

#include <objmgr/seq_entry_ci.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/Seq_hist.hpp>
#include <objects/seq/Seq_hist_rec.hpp>

#include <objtools/edit/protein_match/prot_match_exception.hpp>
#include <objtools/edit/protein_match/setup_match.hpp>
#include <serial/iterator.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CMatchSetup::CMatchSetup(CRef<CScope> db_scope) : m_DBScope(db_scope)
{
}


void CMatchSetup::GatherNucProtSets(
    CSeq_entry& input_entry,
    list<CRef<CSeq_entry>>& nuc_prot_sets) 
{
    if (!input_entry.IsSet()) {
        return;
    }

    for (CTypeIterator<CSeq_entry> it(input_entry); it; ++it) {
        if (it->IsSet() &&
            it->GetSet().IsSetClass() &&
            it->GetSet().GetClass() == CBioseq_set::eClass_nuc_prot) {
            nuc_prot_sets.push_back(Ref(&*it));
        }
    }

}


CBioseq& CMatchSetup::x_FetchNucSeqRef(CBioseq_set& nuc_prot_set) const 
{
    NON_CONST_ITERATE(CBioseq_set::TSeq_set, seq_it, nuc_prot_set.SetSeq_set()) {
        CSeq_entry& se = **seq_it;
        if (se.IsSeq() && se.GetSeq().IsNa()) {
            return se.SetSeq();
        }
    }

    NCBI_THROW(CProteinMatchException, 
        eInputError,
        "No nucleotide sequence found");
}


const CBioseq& CMatchSetup::x_FetchNucSeqRef(const CBioseq_set& nuc_prot_set) const 
{
    ITERATE(CBioseq_set::TSeq_set, seq_it, nuc_prot_set.GetSeq_set()) {
        const CSeq_entry& se = **seq_it;
        if (se.IsSeq() && se.GetSeq().IsNa()) {
            return se.GetSeq();
        }
    }

    NCBI_THROW(CProteinMatchException, 
        eInputError,
        "No nucleotide sequence found");
}


CBioseq& CMatchSetup::x_FetchNucSeqRef(CSeq_entry& nuc_prot_set) const
{
    return x_FetchNucSeqRef(nuc_prot_set.SetSet());
}


bool CMatchSetup::GetReplacedIdsFromHist(const CBioseq& nuc_seq, list<CRef<CSeq_id>>& ids) const
{
    ids.clear();

    if (!nuc_seq.IsSetInst()) {
        return false;
    }

    const auto& seq_inst = nuc_seq.GetInst();
    if (!seq_inst.IsSetHist() ||
        !seq_inst.GetHist().IsSetReplaces() ||
        !seq_inst.GetHist().GetReplaces().IsSetIds()) {
        return false;
    }

    for (auto pReplacesId : seq_inst.GetHist().GetReplaces().GetIds()) {
        if (pReplacesId->IsGenbank() || pReplacesId->IsOther()) {
            ids.push_back(pReplacesId);
        }
    }

    return !ids.empty();
}


bool CMatchSetup::GetAccession(const CBioseq& bioseq, CRef<CSeq_id>& id) const
{
    CRef<CSeq_id> other_id;
    for (auto pSeqId : bioseq.GetId()) {
        if (pSeqId->IsGenbank() || pSeqId->IsOther()) {
            id = pSeqId;
            return true;
        }
    }
    return false;
}


bool CMatchSetup::GetNucSeqId(const CBioseq_set& nuc_prot_set, CRef<CSeq_id>& id) const
{
    const CBioseq& nuc_seq = x_FetchNucSeqRef(nuc_prot_set);
    return GetAccession(nuc_seq, id);
}


CSeq_entry_Handle CMatchSetup::GetTopLevelEntry(const CSeq_id& nuc_id)
{
    CBioseq_Handle db_bsh = m_DBScope->GetBioseqHandle(nuc_id);
    if (db_bsh) {
        return db_bsh.GetTopLevelEntry();
    }
    return CSeq_entry_Handle();
}


CConstRef<CSeq_entry> CMatchSetup::GetDBEntry(const CSeq_id& nuc_id)
{
    CBioseq_Handle db_bsh = m_DBScope->GetBioseqHandle(nuc_id);
    if (db_bsh) {
        CBioseq_set_Handle bss_handle = db_bsh.GetParentBioseq_set();
        if (bss_handle && 
            bss_handle.IsSetClass() &&
            bss_handle.GetClass() == CBioseq_set::eClass_nuc_prot) {
            CRef<CSeq_entry> entry(new CSeq_entry());
            entry->SetSet().Assign(*bss_handle.GetCompleteBioseq_set());
            return entry;
        } 
        else {
            CRef<CSeq_entry> entry(new CSeq_entry());
            entry->SetSeq().Assign(*db_bsh.GetCompleteBioseq());
            return entry;
        }
    }

    NCBI_THROW(CProteinMatchException,
               eInputError,
               "Failed to find a valid database id");

    return CConstRef<CSeq_entry>();

}



CConstRef<CSeq_entry> CMatchSetup::GetDBEntry(const CBioseq& nuc_seq)
{
    CBioseq_Handle db_bsh;
    for (auto pNucId : nuc_seq.GetId()) {
        if (pNucId->IsGenbank() || pNucId->IsOther()) {
            db_bsh = m_DBScope->GetBioseqHandle(*pNucId);
            if (db_bsh) {
                CBioseq_set_Handle bss_handle = db_bsh.GetParentBioseq_set();
                if (bss_handle && 
                    bss_handle.IsSetClass() &&
                    bss_handle.GetClass() == CBioseq_set::eClass_nuc_prot) {
                    CRef<CSeq_entry> entry(new CSeq_entry());
                    entry->SetSet().Assign(*bss_handle.GetCompleteBioseq_set());
                    return entry;
                } 
                else {
                    CRef<CSeq_entry> entry(new CSeq_entry());
                    entry->SetSeq().Assign(*db_bsh.GetCompleteBioseq());
                    return entry;
                }
            }
        }
    }

    list<CRef<CSeq_id>> replacedIds;
    const bool use_replaced_id = GetReplacedIdsFromHist(nuc_seq, replacedIds);

    if (use_replaced_id) {
        db_bsh = m_DBScope->GetBioseqHandle(*(replacedIds.front()));
        if (db_bsh) {
            CBioseq_set_Handle bss_handle =  db_bsh.GetParentBioseq_set();
            if (bss_handle &&
                bss_handle.IsSetClass() &&
                bss_handle.GetClass() == CBioseq_set::eClass_nuc_prot) {
                CRef<CSeq_entry> entry(new CSeq_entry());
                entry->SetSet().Assign(*bss_handle.GetCompleteBioseq_set());
                return entry;
            }
            else {
                CRef<CSeq_entry> entry(new CSeq_entry());
                entry->SetSeq().Assign(*db_bsh.GetCompleteBioseq());
                return entry;
            }
        }
    }

    NCBI_THROW(CProteinMatchException,
               eInputError,
               "Failed to find a valid database id");

    return CConstRef<CSeq_entry>();
}


static bool s_InList(const CSeq_id& id, const CBioseq::TId& id_list)
{
    for (auto pId : id_list) {
        if (id.Match(*pId)) {
            return true;
        }
    }
    return false;
}

bool CMatchSetup::x_GetNucSeqIdsFromCDSs(const CSeq_annot& annot,
    set<CRef<CSeq_id>, SIdCompare>& ids) const
{
    if (!annot.IsFtable()) {
        return false;
    }

    bool found_id = false;

    for (CRef<CSeq_feat> feat : annot.GetData().GetFtable()) {
        if (feat->GetData().IsCdregion()) {

            const CSeq_id* id_ptr = feat->GetLocation().GetId();
            if (!id_ptr) {
                NCBI_THROW(CProteinMatchException,
                    eBadInput,
                    "Invalid CDS location");
            }
            CRef<CSeq_id> nucseq_id = Ref(new CSeq_id());
            nucseq_id->Assign(*id_ptr);
            ids.insert(nucseq_id);
            found_id = true;
        }
    }
    return found_id;
}

// Should be replaced with a gather features method with a unary predicate for filtering
void CMatchSetup::GatherCdregionFeatures(const CSeq_entry& nuc_prot_set,
    list<CRef<CSeq_feat>>& cds_feats) const 
{
    cds_feats.clear();

    const CBioseq_set& bioseq_set = nuc_prot_set.GetSet();
    if (bioseq_set.IsSetAnnot()) { 
        for (CRef<CSeq_annot> pAnnot : bioseq_set.GetAnnot()) {
            if (pAnnot->IsFtable()) {
                for (CRef<CSeq_feat> feat : pAnnot->GetData().GetFtable()) {
                    if (feat->GetData().IsCdregion()) {
                        cds_feats.push_back(feat);
                    }
                } 
            }
        }
    }

    for (CRef<CSeq_entry> pSubentry : bioseq_set.GetSeq_set()) {
        if (pSubentry->IsSeq() && pSubentry->GetSeq().IsNa()) {
            const CBioseq& nucseq = pSubentry->GetSeq();
            if (nucseq.IsSetAnnot()) {
                for (CRef<CSeq_annot> pAnnot : nucseq.GetAnnot()) {
                    if (pAnnot->IsFtable()) {
                        for (CRef<CSeq_feat> feat : pAnnot->GetData().GetFtable()) {
                            if (feat->GetData().IsCdregion()) {
                            cds_feats.push_back(feat);
                            }
                        } 
                    }
                }
            }
        }
    }
}


CRef<CSeq_entry> CMatchSetup::GetCoreNucProtSet(const CSeq_entry& nuc_prot_set, bool exclude_local_prot_ids) const 
{
    CRef<CSeq_entry> pCoreSet = Ref(new CSeq_entry());
    pCoreSet->SetSet().SetClass(CBioseq_set::eClass_nuc_prot);

    list<CRef<CSeq_feat>> cds_feats;
    GatherCdregionFeatures(nuc_prot_set, cds_feats);

    // cds_feats - remove if
    set<CRef<CSeq_id>, SIdCompare> excluded_ids;
    if (exclude_local_prot_ids) {
        for (CRef<CSeq_entry> pSubEntry : nuc_prot_set.GetSet().GetSeq_set()) {
            const CBioseq& seq = pSubEntry->GetSeq();
            if (seq.IsAa()) {
                if (seq.GetNonLocalId() == nullptr) {
                    CRef<CSeq_id> pLocalId(new CSeq_id());
                    pLocalId->Assign(*seq.GetLocalId());
                    excluded_ids.insert(pLocalId);
                }
            }
        }

        if (!excluded_ids.empty()) {
            // drop CDS features whose products are excluded protein sequences
            cds_feats.remove_if([&excluded_ids](CRef<CSeq_feat> cds_feat)->bool {
                        if (cds_feat->IsSetProduct() &&
                            cds_feat->GetProduct().GetId()) {
                            CRef<CSeq_id> pProductId(new CSeq_id()); 
                            pProductId->Assign(*cds_feat->GetProduct().GetId());
                            if (excluded_ids.find(pProductId) != excluded_ids.end()) {
                                return true;
                            }
                        }
                        return false;
                    }
            );
        }
    }


    for (CRef<CSeq_entry> pSubEntry : nuc_prot_set.GetSet().GetSeq_set()) {
        const CBioseq& old_seq = pSubEntry->GetSeq();

        if (old_seq.IsAa() &&
            exclude_local_prot_ids 
            && !old_seq.GetNonLocalId()) {
            continue;
        }
        CRef<CSeq_entry> pCoreSubEntry(new CSeq_entry());
        CBioseq& core_seq = pCoreSubEntry->SetSeq();
        core_seq.SetId() = old_seq.GetId();
        core_seq.SetInst().Assign(old_seq.GetInst()); // May be no need to copy here
       
        if (old_seq.IsNa() && !cds_feats.empty()) {
            CRef<CSeq_annot> pCoreAnnot(new CSeq_annot());
            pCoreAnnot->SetData().SetFtable() = cds_feats;
            core_seq.SetAnnot().push_back(pCoreAnnot);
        }
        pCoreSet->SetSet().SetSeq_set().push_back(pCoreSubEntry);
    }

    return pCoreSet;
}



bool CMatchSetup::GetNucSeqIdFromCDSs(const CSeq_entry& nuc_prot_set,
    CRef<CSeq_id>& id) const
{
    // Set containing distinct ids
    set<CRef<CSeq_id>, SIdCompare> ids;
    
    const CBioseq_set& bioseq_set = nuc_prot_set.GetSet();
    if (bioseq_set.IsSetAnnot()) { 
        for (CRef<CSeq_annot> pAnnot : bioseq_set.GetAnnot()) {
            x_GetNucSeqIdsFromCDSs(*pAnnot, ids);
        }
    }

    for (CRef<CSeq_entry> pSubentry : bioseq_set.GetSeq_set()) {
        if (pSubentry->IsSeq() && pSubentry->GetSeq().IsNa()) {
            const CBioseq& nucseq = pSubentry->GetSeq();
            if (nucseq.IsSetAnnot()) {
                for (CRef<CSeq_annot> pAnnot : nucseq.GetAnnot()) {
                    x_GetNucSeqIdsFromCDSs(*pAnnot, ids);
                }
            }
        }
    }

    if (ids.empty()) {
        return false;
    }


    // Check that the ids point to the nucleotide sequence
    const CBioseq& nuc_seq = nuc_prot_set.GetSet().GetNucFromNucProtSet();

    for ( auto pId : ids) {
        if (!s_InList(*pId, nuc_seq.GetId())) {
            NCBI_THROW(CProteinMatchException,
                eBadInput,
                "Unrecognized CDS location id");
        }
    }

    const bool with_version = true;
    const string local_id_string = nuc_seq.GetFirstId()->GetSeqIdString(with_version);

    if (id.IsNull()) {
        id = Ref(new CSeq_id());
    }
    id->SetLocal().SetStr(local_id_string);

    return true;
}


bool CMatchSetup::UpdateNucSeqIds(CRef<CSeq_id> new_id,
    CSeq_entry& nuc_prot_set) const
{
    if (!nuc_prot_set.IsSet()) {
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
