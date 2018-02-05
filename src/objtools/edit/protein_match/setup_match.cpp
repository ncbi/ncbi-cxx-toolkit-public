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
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/RNA_ref.hpp>

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
    for (CRef<CSeq_entry> sub_entry : nuc_prot_set.SetSeq_set()) {
        if (sub_entry->IsSeq() && sub_entry->GetSeq().IsNa()) {
            return sub_entry->SetSeq();
        }
    }

    NCBI_THROW(CProteinMatchException, 
        eInputError,
        "No nucleotide sequence found");
}


const CBioseq& CMatchSetup::x_FetchNucSeqRef(const CBioseq_set& nuc_prot_set) const 
{
    for (CRef<CSeq_entry> sub_entry : nuc_prot_set.GetSeq_set()) {
        if (sub_entry->IsSeq() && sub_entry->GetSeq().IsNa()) {
            return sub_entry->GetSeq();
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

    return CConstRef<CSeq_entry>();

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


void CMatchSetup::GatherCdregionFeatures(const CBioseq_set& nuc_prot_set,
    list<CRef<CSeq_feat>>& cds_feats) const 
{
    cds_feats.clear();

    
    if (nuc_prot_set.IsSetAnnot()) { 
        for (CRef<CSeq_annot> pAnnot : nuc_prot_set.GetAnnot()) {
            if (pAnnot->IsFtable()) {
                for (CRef<CSeq_feat> feat : pAnnot->GetData().GetFtable()) {
                    if (feat->GetData().IsCdregion()) {
                        cds_feats.push_back(feat);
                    }
                } 
            }
        }
    }

    for (CRef<CSeq_entry> pSubentry : nuc_prot_set.GetSeq_set()) {
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


bool s_GetGeneralOrLocalID(const CBioseq& bioseq, CRef<CSeq_id>& id) 
{
    if (!bioseq.IsSetId()) {
        return false;
    }

    CRef<CSeq_id> best_id;
    for (CRef<CSeq_id> seq_id : bioseq.GetId()) {
        if (seq_id->IsGeneral()) {
            id = seq_id;
            return true;
        }
        else 
        if (seq_id->IsLocal()){
            best_id = seq_id;
        }
    }

    if (!best_id.Empty()) {
        id = best_id;
        return true;
    }
    return false;
}


CRef<CBioseq_set> CMatchSetup::GetCoreNucProtSet(const CBioseq_set& nuc_prot_set, bool exclude_local_prot_ids) const 
{
    CRef<CBioseq_set> pCoreSet = Ref(new CBioseq_set());
    pCoreSet->SetClass(CBioseq_set::eClass_nuc_prot);

    list<CRef<CSeq_feat>> cds_feats;
    GatherCdregionFeatures(nuc_prot_set, cds_feats);

    // cds_feats - remove if
    set<CRef<CSeq_id>, SIdCompare> excluded_ids;
    if (exclude_local_prot_ids) {
        for (CRef<CSeq_entry> pSubEntry : nuc_prot_set.GetSeq_set()) {
            const CBioseq& seq = pSubEntry->GetSeq();
            if (seq.IsAa() && seq.IsSetId()) {
                CRef<CSeq_id> id;
                if (!GetAccession(seq, id) &&
                    s_GetGeneralOrLocalID(seq, id)) {
                    excluded_ids.insert(id);
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


    for (CRef<CSeq_entry> pSubEntry : nuc_prot_set.GetSeq_set()) {
        const CBioseq& old_seq = pSubEntry->GetSeq();

        CRef<CSeq_id> dummy_id;
        if (old_seq.IsAa() &&
            exclude_local_prot_ids 
            && !GetAccession(old_seq, dummy_id)) {
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
        pCoreSet->SetSeq_set().push_back(pCoreSubEntry);
    }

    return pCoreSet;
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
            if (seq.IsAa() && seq.IsSetId()) {
                CRef<CSeq_id> id;
                if (!GetAccession(seq, id) &&
                    s_GetGeneralOrLocalID(seq, id)) {
                    excluded_ids.insert(id);
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

        CRef<CSeq_id> dummy_id;
        if (old_seq.IsAa() &&
            exclude_local_prot_ids 
            && !GetAccession(old_seq, dummy_id)) {
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


CRef<CSeq_id> CMatchSetup::GetLocalSeqId(const CBioseq& seq) const
{   
    CRef<CSeq_id> pLocalId(new CSeq_id());

    const CSeq_id* local_id = seq.GetLocalId();
    if (local_id) {
        pLocalId->Assign(*local_id);
        return pLocalId;
    }

    const CSeq_id* first_id = seq.GetFirstId();
    if (!first_id) {
        NCBI_THROW(CProteinMatchException, 
                   eBadInput, 
                   "Unable to find sequence id");
    }

    const bool with_version = true;
    const string local_id_string = first_id->GetSeqIdString(with_version);
    pLocalId->SetLocal().SetStr(local_id_string);

    return pLocalId;
}


bool CMatchSetup::UpdateNucSeqIds(CRef<CSeq_id> new_id,
    CBioseq_set& nuc_prot_set) const
{
    CBioseq& nuc_seq = x_FetchNucSeqRef(nuc_prot_set);
    nuc_seq.ResetId();
    nuc_seq.SetId().push_back(new_id);

    for (CTypeIterator<CSeq_feat> feat(nuc_prot_set); feat; ++feat) {
        if (feat->GetData().IsCdregion() ||
            feat->GetData().IsGene() ||
            (feat->GetData().IsRna() && (feat->GetData().GetRna().GetType() == CRNA_ref::eType_mRNA)) ||
            (feat->GetData().IsImp() && feat->GetData().GetImp().GetKey()=="exon") ) {
            feat->SetLocation().SetId(*new_id);
        }
    }
    
    return true;   
}

END_SCOPE(objects)
END_NCBI_SCOPE
