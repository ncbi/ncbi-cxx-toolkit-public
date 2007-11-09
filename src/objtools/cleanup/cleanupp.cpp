/*  $Id$
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
*   Implementation of private parts of basic cleanup.
*
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seqloc/Seq_bond.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/seqblock/EMBL_block.hpp>
#include <objects/biblio/Cit_gen.hpp>
#include <serial/iterator.hpp>

#include <objects/seq/MolInfo.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/general/Date.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_annot_ci.hpp>
#include <objmgr/feat_ci.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/bioseq_set_handle.hpp>
#include <objmgr/seq_annot_handle.hpp>
#include <objmgr/seq_feat_handle.hpp>
#include <objmgr/bioseq_ci.hpp>

#include <objtools/cleanup/cleanup.hpp>
#include "../format/utils.hpp"
#include "cleanupp.hpp"
#include "cleanup_utils.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CCleanup_imp::CCleanup_imp(CRef<CCleanupChange> changes, CRef<CScope> scope, Uint4 options)
: m_Changes(changes), m_Options(options), m_Mode(eCleanup_GenBank), m_Scope (scope)
{
   
}


CCleanup_imp::~CCleanup_imp()
{
}


void CCleanup_imp::Setup(const CSeq_entry& se)
{
    /*** Set cleanup mode. ***/
    
    CConstRef<CBioseq> first_bioseq;
    switch (se.Which()) {
        default:
            break;
        case CSeq_entry::e_Seq:
        {
            first_bioseq.Reset(&se.GetSeq());
            break;            
        }
        case CSeq_entry::e_Set:
        {
            CTypeConstIterator<CBioseq> seq(ConstBegin(se.GetSet()));
            if (seq) {
                first_bioseq.Reset(&*seq);
            }
            break;            
        }
    }
    
    m_Mode = eCleanup_GenBank;
    if (first_bioseq) {
        ITERATE (CBioseq::TId, it, first_bioseq->GetId()) {
            const CSeq_id& id = **it;
            if (id.IsEmbl()  ||  id.IsTpe()) {
                m_Mode = eCleanup_EMBL;
            } else if (id.IsDdbj()  ||  id.IsTpd()) {
                m_Mode = eCleanup_DDBJ;
            } else if (id.IsSwissprot()) {
                m_Mode = eCleanup_SwissProt;
            }
        }        
    }
    
}


void CCleanup_imp::Finish(CSeq_entry& se)
{
    // cleanup for Cleanup.
    
}


void CCleanup_imp::ChangeMade(CCleanupChange::EChanges e)
{
    if (m_Changes) {
        m_Changes->SetChanged(e);
    }
}


void CCleanup_imp::BasicCleanup(CSeq_entry& se)
{
    Setup(se);
    switch (se.Which()) {
        case CSeq_entry::e_Seq:
            BasicCleanup(se.SetSeq());
            break;
        case CSeq_entry::e_Set:
            BasicCleanup(se.SetSet());
            break;
        case CSeq_entry::e_not_set:
        default:
            break;
    }
    Finish(se);
}


void CCleanup_imp::BasicCleanup(CSeq_submit& ss)
{
    // TODO Cleanup Submit-block.
    
    switch (ss.GetData().Which()) {
        case CSeq_submit::TData::e_Entrys:
            NON_CONST_ITERATE(CSeq_submit::TData::TEntrys, it, ss.SetData().SetEntrys()) {
                BasicCleanup(**it);
            }
            break;
        case CSeq_submit::TData::e_Annots:
            NON_CONST_ITERATE(CSeq_submit::TData::TAnnots, it, ss.SetData().SetAnnots()) {
                BasicCleanup(**it);
            }
            break;
        case CSeq_submit::TData::e_Delete:
        case CSeq_submit::TData::e_not_set:
        default:
            break;
    }
}


void CCleanup_imp::BasicCleanup(CBioseq_set& bss)
{
    if (bss.IsSetAnnot()) {
        NON_CONST_ITERATE (CBioseq_set::TAnnot, it, bss.SetAnnot()) {
            BasicCleanup(**it);
        }
    }
    if (bss.IsSetDescr()) {
        BasicCleanup(bss.SetDescr());
    }
    if (bss.IsSetSeq_set()) {
        // copies form BasicCleanup(CSeq_entry) to avoid recursing through it.
        NON_CONST_ITERATE (CBioseq_set::TSeq_set, it, bss.SetSeq_set()) {
            CSeq_entry& se = **it;
            switch (se.Which()) {
                case CSeq_entry::e_Seq:
                    BasicCleanup(se.SetSeq());
                    break;
                case CSeq_entry::e_Set:
                    BasicCleanup(se.SetSet());
                    break;
                case CSeq_entry::e_not_set:
                default:
                    break;
            }
        }
    }
}


void CCleanup_imp::BasicCleanup(CBioseq& bs)
{    
    if (bs.IsSetAnnot()) {
        NON_CONST_ITERATE (CBioseq::TAnnot, it, bs.SetAnnot()) {
            BasicCleanup(**it);
        }
    }
    if (bs.IsSetDescr()) {
        BasicCleanup(bs.SetDescr());
    }
}


void CCleanup_imp::BasicCleanup(CSeq_annot& sa)
{
    if (sa.IsSetData()  &&  sa.GetData().IsFtable()) {
        NON_CONST_ITERATE (CSeq_annot::TData::TFtable, it, sa.SetData().SetFtable()) {
            BasicCleanup(**it);
        }
    }
}


void CCleanup_imp::BasicCleanup(CSeq_descr& sdr)
{
    NON_CONST_ITERATE (CSeq_descr::Tdata, it, sdr.Set()) {
        BasicCleanup(**it);
    }
}


static bool s_SeqDescCommentCleanup( CSeqdesc::TComment& comment )
{
    bool changed = RemoveTrailingJunk(comment);
    if ( RemoveSpacesBetweenTildes(comment) ) {
        changed = true;
    }
    
    return changed;
};


void CCleanup_imp::BasicCleanup(CSeqdesc& sd)
{
    switch (sd.Which()) {
        case CSeqdesc::e_Mol_type:
            break;
        case CSeqdesc::e_Modif:
            break;
        case CSeqdesc::e_Method:
            break;
        case CSeqdesc::e_Name:
            if (CleanString( sd.SetName() ) ) {
                ChangeMade(CCleanupChange::eTrimSpaces);
            }
            break;
        case CSeqdesc::e_Title:
            if (CleanString( sd.SetTitle() ) ) {
                ChangeMade(CCleanupChange::eTrimSpaces);
            }
            break;
        case CSeqdesc::e_Org:
            BasicCleanup(sd.SetOrg() );
            break;
        case CSeqdesc::e_Comment:
            if (s_SeqDescCommentCleanup( sd.SetComment() ) ) {
                ChangeMade(CCleanupChange::eTrimSpaces);
            }
            break;
        case CSeqdesc::e_Num:
            break;
        case CSeqdesc::e_Maploc:
            break;
        case CSeqdesc::e_Pir:
            break;
        case CSeqdesc::e_Genbank:
            BasicCleanup(sd.SetGenbank());
            break;
        case CSeqdesc::e_Pub:
            BasicCleanup(sd.SetPub());
            break;
        case CSeqdesc::e_Region:
            if (CleanString( sd.SetRegion() ) ) {
                ChangeMade(CCleanupChange::eTrimSpaces); 
            }
            break;
        case CSeqdesc::e_User:
            BasicCleanup( sd.SetUser() );
            break;
        case CSeqdesc::e_Sp:
            break;
        case CSeqdesc::e_Dbxref:
            break;
        case CSeqdesc::e_Embl:
            BasicCleanup( sd.SetEmbl() );
            break;
        case CSeqdesc::e_Create_date:
            break;
        case CSeqdesc::e_Update_date:
            break;
        case CSeqdesc::e_Prf:
            break;
        case CSeqdesc::e_Pdb:
            break;
        case CSeqdesc::e_Het:
            break;
        case CSeqdesc::e_Source:
            BasicCleanup(sd.SetSource());
            break;
        case CSeqdesc::e_Molinfo:
            break;
            
        default:
            break;
    }
}


// CGB_block data member cleanup
void CCleanup_imp::BasicCleanup(CGB_block& gbb)   
{
    CLEAN_STRING_LIST_JUNK(gbb, Extra_accessions);
    if (gbb.IsSetExtra_accessions()) {
        CGB_block::TExtra_accessions& x_accs = gbb.SetExtra_accessions();
        if ( ! objects::is_sorted(x_accs.begin(), x_accs.end())) {
            x_accs.sort();
            ChangeMade(CCleanupChange::eCleanQualifiers);
        }
        size_t xaccs_len = x_accs.size();
        x_accs.unique();
        if (xaccs_len != x_accs.size()) {
            ChangeMade(CCleanupChange::eCleanQualifiers);
        }
    }
    
    CLEAN_STRING_LIST(gbb, Keywords);
    // don't sort keywords, but get rid of dups.
    if (gbb.IsSetKeywords()) {
        if (RemoveDupsNoSort(gbb.SetKeywords(), m_Mode != eCleanup_EMBL)) { // case insensitive
            ChangeMade(CCleanupChange::eCleanKeywords);
        }        
    }
    
    
    CLEAN_STRING_MEMBER(gbb, Source);
    CLEAN_STRING_MEMBER(gbb, Origin);
    //
    //  origin:
    //  append period if there isn't one already
    //
    if ( gbb.CanGetOrigin() ) {
        const CGB_block::TOrigin& origin = gbb.GetOrigin();
        if ( ! origin.empty() && ! NStr::EndsWith(origin, ".")) {
            gbb.SetOrigin() += '.';
            ChangeMade(CCleanupChange::eChangeOther);
        }
    }
    CLEAN_STRING_MEMBER(gbb, Date);
    CLEAN_STRING_MEMBER_JUNK(gbb, Div);
    gbb.ResetTaxonomy();
}


void CCleanup_imp::BasicCleanup(CEMBL_block& emb)
{
    CLEAN_STRING_LIST_JUNK(emb, Extra_acc);
    if ( ! objects::is_sorted(emb.GetExtra_acc().begin(),
                              emb.GetExtra_acc().end())) {
        emb.SetExtra_acc().sort();
        ChangeMade(CCleanupChange::eCleanQualifiers);
    }
    if (RemoveDupsNoSort(emb.SetKeywords(), false)) { // case insensitive
        ChangeMade(CCleanupChange::eCleanKeywords);
    }
}
 

// Basic Cleanup w/Object Manager Handles.
// cleanup fields that object manager cares about. (like changing feature types.)

void CCleanup_imp::Setup(const CSeq_entry_Handle& seh)
{
    /*** Set cleanup mode. ***/
    
    CBioseq_Handle first_bioseq;
    switch (seh.Which()) {
        default:
            break;
        case CSeq_entry::e_Seq :
        {
            first_bioseq = seh.GetSeq();
            break;            
        }
        case CSeq_entry::e_Set :
        {
            CBioseq_CI bs_i(seh.GetSet());
            if (bs_i) {
                first_bioseq = *bs_i;
            }
            break;            
        }
    }
    
    m_Mode = eCleanup_GenBank;
    if (first_bioseq  &&  first_bioseq.CanGetId()) {
        ITERATE (CBioseq_Handle::TId, it, first_bioseq.GetId()) {
            const CSeq_id& id = *(it->GetSeqId());
            if (id.IsEmbl()  ||  id.IsTpe()) {
                m_Mode = eCleanup_EMBL;
            } else if (id.IsDdbj()  ||  id.IsTpd()) {
                m_Mode = eCleanup_DDBJ;
            } else if (id.IsSwissprot()) {
                m_Mode = eCleanup_SwissProt;
            }
        }        
    }
    
}


void CCleanup_imp::Finish(CSeq_entry_Handle& seh)
{
    // cleanup for Cleanup.
    
}

void CCleanup_imp::BasicCleanup(CSeq_entry_Handle& seh)
{
    Setup(seh);
    CFeat_CI fi(seh);
    for (; fi; ++fi) {
        BasicCleanup(fi->GetSeq_feat_Handle());
    }
    // special GB-block cleanup. Was in Extended Cleanup.
    x_ChangeGenBankBlocks(seh);
    // do the non-handle stuff
    BasicCleanup(const_cast<CSeq_entry&>(*seh.GetCompleteSeq_entry()));
    Finish(seh);
}


void CCleanup_imp::BasicCleanup(const CBioseq_Handle& bsh)
{
    CFeat_CI fi(bsh);
    for (; fi; ++fi) {
        BasicCleanup(fi->GetSeq_feat_Handle());
    }
    // do the non-handle stuff
    BasicCleanup(const_cast<CBioseq&>(*bsh.GetCompleteBioseq()));
}


void CCleanup_imp::BasicCleanup(CBioseq_set_Handle& bssh)
{
    CBioseq_CI bsi(bssh);
    for (; bsi; ++bsi) {
        BasicCleanup(*bsi);
    }
    // do the non-handle stuff
    BasicCleanup(const_cast<CBioseq_set&>(*bssh.GetCompleteBioseq_set()));
}


void CCleanup_imp::BasicCleanup(CSeq_annot_Handle& sah)
{
    CFeat_CI fi(sah);
    for (; fi; ++fi) {
        BasicCleanup(fi->GetSeq_feat_Handle());
    }
    // do the non-handle stuff
    BasicCleanup(const_cast<CSeq_annot&>(*sah.GetCompleteSeq_annot()));
}


static
bool x_IsOneMinusStrand(const CSeq_loc& sl)
{
//    bool isReverse = true;
    switch ( sl.Which() ) {
        default:
            return false;
        case CSeq_loc::e_Int:
        case CSeq_loc::e_Pnt:
            return sl.IsReverseStrand();

        case CSeq_loc::e_Packed_int:
            ITERATE(CSeq_loc::TPacked_int::Tdata, i, sl.GetPacked_int().Get()) {
                if (IsReverse((*i)->GetStrand())) {
                    return true;
                }
            }
            break;
        case CSeq_loc::e_Packed_pnt:
            return IsReverse(sl.GetPacked_pnt().GetStrand());
        case CSeq_loc::e_Mix:
            ITERATE(CSeq_loc::TMix::Tdata, i, sl.GetMix().Get()) {
                if (x_IsOneMinusStrand(**i)) {
                    return true;
                }
            }
            break;
        case CSeq_loc::e_Equiv:
            ITERATE(CSeq_loc::TEquiv::Tdata, i, sl.GetEquiv().Get()) {
                if (x_IsOneMinusStrand(**i)) {
                    return true;
                }
            }
            break;
        case CSeq_loc::e_Bond:
            if (IsReverse(sl.GetBond().GetA().GetStrand())) {
                return true;
            }
            if (sl.GetBond().IsSetB()) {
                if (IsReverse(sl.GetBond().GetB().GetStrand())) {
                    return true;
                }
            }
            break;
    }
    return false;
}


void CCleanup_imp::BasicCleanup(CSeq_loc& sl)
{
    
    if (sl.IsWhole()  &&  m_Scope) {
        // change the Seq-loc/whole to a Seq-loc/interval which covers the whole sequence.
        CRef<CSeq_id> id(&sl.SetWhole());
        CBioseq_Handle bsh;
        try {
            bsh = m_Scope->GetBioseqHandle(*id);
        } catch (...) { }
        if (bsh) {
            TSeqPos bs_len = bsh.GetBioseqLength();
            
            sl.SetInt().SetId(*id);
            sl.SetInt().SetFrom(0);
            sl.SetInt().SetTo(bs_len - 1);
            ChangeMade(CCleanupChange::eChangeWholeLocation);
        }
    }
    
        
    switch (sl.Which()) {
    default:
        break;
    case CSeq_loc::e_Int :
        BasicCleanup(sl.SetInt());
        break;
    case CSeq_loc::e_Packed_int :
        {
            CSeq_loc::TPacked_int::Tdata& ints = sl.SetPacked_int().Set();
            NON_CONST_ITERATE(CSeq_loc::TPacked_int::Tdata, interval_it, ints) {
                BasicCleanup(**interval_it);
            }
            if (ints.size() == 1) {
                CRef<CSeq_interval> int_ref = ints.front();
                sl.SetInt(*int_ref);
                ChangeMade(CCleanupChange::eChangeSeqloc);
            }
        }
        break;
    case CSeq_loc::e_Pnt :
        {
            CSeq_loc::TPnt& pnt = sl.SetPnt();
            
            if (pnt.CanGetStrand()) {
                ENa_strand strand = pnt.GetStrand();
                if (strand == eNa_strand_both) {
                    pnt.SetStrand(eNa_strand_plus);
                    ChangeMade(CCleanupChange::eChangeStrand);
                } else if (strand == eNa_strand_both_rev) {
                    pnt.SetStrand(eNa_strand_minus);
                    ChangeMade(CCleanupChange::eChangeStrand);
                }                
            }
        }
        break;
    case CSeq_loc::e_Mix :
        typedef CSeq_loc::TMix::Tdata TMixList;
        // delete Null type Seq-locs from beginning and end of Mix list.
        TMixList& sl_list = sl.SetMix().Set();
        TMixList::iterator sl_it = sl_list.begin();
        while (sl_it != sl_list.end()) {
            if ((*sl_it)->IsNull()) {
                sl_it = sl_list.erase(sl_it);
                ChangeMade(CCleanupChange::eChangeSeqloc);
            } else {
                break;
            }
        }
        sl_it = sl_list.end();
        while (sl_it != sl_list.begin()) {
            --sl_it;
            if ( ! (*sl_it)->IsNull()) {
                break;
            }
        }
        ++sl_it;
        if (sl_it != sl_list.end()) {
            sl_list.erase(sl_it, sl_list.end());
            ChangeMade(CCleanupChange::eChangeSeqloc);            
        }

        NON_CONST_ITERATE(TMixList, sl_it, sl_list) {
            BasicCleanup(**sl_it);
        }
            
        if (sl_list.size() == 1) {
            CRef<CSeq_loc> only_sl = sl_list.front();
            sl.Assign(*only_sl);
            ChangeMade(CCleanupChange::eChangeSeqloc);
        }
        break;
    }

    /* don't allow negative strand on protein sequences */
    {
        CBioseq_Handle bsh;
        try {
            if (m_Scope.GetPointerOrNull() ) {
                bsh = m_Scope->GetBioseqHandle(sl);
            }
        } catch (...) { }
        if (bsh  &&  bsh.IsProtein()  && x_IsOneMinusStrand(sl) ) {
            sl.SetStrand(eNa_strand_unknown);
            ChangeMade(CCleanupChange::eChangeSeqloc);
        }
    }
    
}


void CCleanup_imp::BasicCleanup(CSeq_interval& si)
{
    // Fix backwards intervals
    if ( si.CanGetFrom()  &&  si.CanGetTo()  &&  si.GetFrom() > si.GetTo()) {
        swap(si.SetFrom(), si.SetTo());
        ChangeMade(CCleanupChange::eChangeSeqloc);
    }
    // change bad strand values.
    if (si.CanGetStrand()) {
        ENa_strand strand = si.GetStrand();
        if (strand == eNa_strand_both) {
            si.SetStrand(eNa_strand_plus);
            ChangeMade(CCleanupChange::eChangeStrand);
        } else if (strand == eNa_strand_both_rev) {
            si.SetStrand(eNa_strand_minus);
            ChangeMade(CCleanupChange::eChangeStrand);
        }        
    }
}


// Extended Cleanup Methods
void CCleanup_imp::ExtendedCleanup(CSeq_entry_Handle seh)
{    
    switch (seh.Which()) {
        case CSeq_entry::e_Seq:
            ExtendedCleanup(seh.GetSeq());
            break;
        case CSeq_entry::e_Set:
            ExtendedCleanup(seh.GetSet());
            break;
        case CSeq_entry::e_not_set:
        default:
            break;
    }
}


void CCleanup_imp::ExtendedCleanup(const CSeq_submit& ss)
{
    // TODO Cleanup Submit-block.
    
    switch (ss.GetData().Which()) {
        case CSeq_submit::TData::e_Entrys:
            ITERATE(CSeq_submit::TData::TEntrys, it, ss.GetData().GetEntrys()) {
                CSeq_entry_Handle seh = m_Scope->GetSeq_entryHandle((**it));
                ExtendedCleanup(seh);
            }
            break;
        case CSeq_submit::TData::e_Annots:
            ITERATE(CSeq_submit::TData::TAnnots, it, ss.GetData().GetAnnots()) {
                ExtendedCleanup(m_Scope->GetSeq_annotHandle(**it));
            }
            break;
        case CSeq_submit::TData::e_Delete:
        case CSeq_submit::TData::e_not_set:
        default:
            break;
    }
}


void CCleanup_imp::ExtendedCleanup(CBioseq_set_Handle bss)
{
    // change any citation qualifiers into real citations
    x_ChangeCitSub(bss);

    x_MoveGeneQuals (bss);
    x_RecurseForSeqAnnots(bss, &ncbi::objects::CCleanup_imp::x_RemoveEmptyFeatures);
    // These two steps were EntryChangeImpFeat in the C Toolkit
    x_RecurseForSeqAnnots(bss, &ncbi::objects::CCleanup_imp::x_ChangeImpFeatToCDS);
    x_RecurseForSeqAnnots(bss, &ncbi::objects::CCleanup_imp::x_ChangeImpFeatToProt);
    x_ExtendSingleGeneOnmRNA(bss);
    
    x_RecurseForDescriptors(bss, &ncbi::objects::CCleanup_imp::x_RemoveMultipleTitles);
    x_RecurseForDescriptors(bss, &ncbi::objects::CCleanup_imp::x_MergeMultipleDates);

    LoopToAsn3(bss);                                  
                                      
    RemoveEmptyFeaturesDescriptorsAndAnnots(bss);
    x_RecurseForSeqAnnots(bss, &ncbi::objects::CCleanup_imp::x_RemovePseudoProducts);
    
    CSeq_entry_EditHandle seh = bss.GetEditHandle().GetParentEntry();
    RenormalizeNucProtSets(bss);
    
    if (seh.IsSet()) {
        bss = seh.GetSet().GetEditHandle();
        x_MoveFeaturesOnPartsToCorrectSeqAnnots (bss);
        MoveCodingRegionsToNucProtSets(bss);
        x_RecurseForSeqAnnots(bss, &ncbi::objects::CCleanup_imp::x_MoveDbxrefs);

        x_RecurseForSeqAnnots(bss, &ncbi::objects::CCleanup_imp::x_RemoveEmptyFeatures);
    
        x_RecurseForSeqAnnots(bss, &ncbi::objects::CCleanup_imp::x_CheckCodingRegionEnds);
    
        x_RecurseForSeqAnnots(bss, &ncbi::objects::CCleanup_imp::x_RemoveUnnecessaryGeneXrefs);
        x_RemoveMarkedGeneXrefs(bss);
        x_MergeAdjacentAnnots(bss);

        // Remove empty GenBank block descriptors
        x_RecurseForDescriptors(bss, &ncbi::objects::CCleanup_imp::x_RemoveEmptyGenbankDesc);

        // Cleanup Publications        
        x_ExtendedCleanupPubs(bss);                
        
        // Clean up MolInfo descriptors
        x_ExtendedCleanupMolInfoDescriptors (bss);

        // Clean up BioSource features and descriptors last
        x_ExtendedCleanupBioSourceDescriptorsAndFeatures (bss);
    } else {
        CBioseq_EditHandle bsh = seh.GetSeq().GetEditHandle();
        x_RecurseForSeqAnnots(bsh, &ncbi::objects::CCleanup_imp::x_MoveDbxrefs);

        x_RecurseForSeqAnnots(bsh, &ncbi::objects::CCleanup_imp::x_RemoveEmptyFeatures);
    
        x_RecurseForSeqAnnots(bsh, &ncbi::objects::CCleanup_imp::x_CheckCodingRegionEnds);
    
        x_RecurseForSeqAnnots(bsh, &ncbi::objects::CCleanup_imp::x_RemoveUnnecessaryGeneXrefs);
        x_RemoveMarkedGeneXrefs(bsh);
        x_MergeAdjacentAnnots(bsh);

        // Remove empty GenBank block descriptors
        x_RecurseForDescriptors(bsh, &ncbi::objects::CCleanup_imp::x_RemoveEmptyGenbankDesc);

        // Cleanup Publications        
        x_ExtendedCleanupPubs(bsh);                
        
        // Clean up MolInfo descriptors
        x_ExtendedCleanupMolInfoDescriptors (bss);

        // Clean up BioSource features and descriptors last
        x_ExtendedCleanupBioSourceDescriptorsAndFeatures (bsh);
    }
}



void CCleanup_imp::ExtendedCleanup(CBioseq_Handle bsh)
{
    // change any citation qualifiers into real citations
    x_ChangeCitSub(bsh);
    
    x_MoveGeneQuals (bsh);
    x_RecurseForSeqAnnots(bsh, &ncbi::objects::CCleanup_imp::x_RemoveEmptyFeatures); 
    x_RecurseForSeqAnnots(bsh, &ncbi::objects::CCleanup_imp::x_ChangeImpFeatToCDS);
    x_RecurseForSeqAnnots(bsh, &ncbi::objects::CCleanup_imp::x_ChangeImpFeatToProt);
    if (IsmRNA (bsh) && ! IsArtificialSyntheticConstruct (bsh)) {
        x_ExtendSingleGeneOnmRNA(bsh, false);
    }
    
    x_RecurseForDescriptors(bsh, &ncbi::objects::CCleanup_imp::x_RemoveMultipleTitles);
    x_RecurseForDescriptors(bsh, &ncbi::objects::CCleanup_imp::x_MergeMultipleDates);

    LoopToAsn3(bsh.GetSeq_entry_Handle());
                                          
    RemoveEmptyFeaturesDescriptorsAndAnnots(bsh);
    
    x_RecurseForSeqAnnots(bsh, &ncbi::objects::CCleanup_imp::x_MoveDbxrefs);

    x_RecurseForSeqAnnots(bsh, &ncbi::objects::CCleanup_imp::x_RemoveEmptyFeatures); 
    
    x_RecurseForSeqAnnots(bsh, &ncbi::objects::CCleanup_imp::x_CheckCodingRegionEnds);        

    x_RecurseForSeqAnnots(bsh, &ncbi::objects::CCleanup_imp::x_RemoveUnnecessaryGeneXrefs);
    x_RemoveMarkedGeneXrefs(bsh);
    x_MergeAdjacentAnnots(bsh);

    // Remove empty GenBank block descriptors
    x_RecurseForDescriptors(bsh, &ncbi::objects::CCleanup_imp::x_RemoveEmptyGenbankDesc);

    // Cleanup Publications
    x_ExtendedCleanupPubs(bsh);

    // Clean up MolInfo descriptors
    x_ExtendedCleanupMolInfoDescriptors (bsh);

    // Clean up BioSource features and descriptors last
    x_ExtendedCleanupBioSourceDescriptorsAndFeatures (bsh);
}


void CCleanup_imp::ExtendedCleanup(CSeq_annot_Handle sa)
{
    x_MoveGeneQuals (sa);
    x_RemoveEmptyFeatures(sa);

    x_ChangeImpFeatToCDS(sa);
    x_ChangeImpFeatToProt(sa);
   
    x_RemovePseudoProducts(sa);   
    
    x_MoveDbxrefs(sa);
 
    x_CheckCodingRegionEnds(sa);
    
    x_RemoveUnnecessaryGeneXrefs (sa);
    x_RemoveMarkedGeneXrefs(sa);
    x_ExtendedCleanupPubs(sa);    
}


void CCleanup_imp::x_RecurseForSeqAnnots (const CSeq_entry& se, RecurseSeqAnnot pmf)
{
    switch (se.Which()) {
        case CSeq_entry::e_Seq:
            x_RecurseForSeqAnnots(m_Scope->GetBioseqHandle(se.GetSeq()), pmf);
            break;
        case CSeq_entry::e_Set:
            x_RecurseForSeqAnnots(m_Scope->GetBioseq_setHandle(se.GetSet()), pmf);
            break;
        case CSeq_entry::e_not_set:
        default:
            break;
    }
}


void CCleanup_imp::x_RecurseForSeqAnnots (CBioseq_Handle bs, RecurseSeqAnnot pmf)
{
    CBioseq_EditHandle bseh = bs.GetEditHandle();
    
    vector<CSeq_annot_EditHandle> sah; // copy empty annot handles to not break iterator while moving    
    CSeq_annot_CI annot_it(bseh.GetSeq_entry_Handle(), CSeq_annot_CI::eSearch_entry);
    for(; annot_it; ++annot_it) {
        (this->*pmf)(*annot_it);
        if (annot_it->IsFtable() && annot_it->GetCompleteSeq_annot()->GetData().GetFtable().empty()) {
            // add annot_it to list of annotations to remove
            sah.push_back((*annot_it).GetEditHandle());
        }
    }   
    ITERATE ( vector<CSeq_annot_EditHandle>, it, sah ) {
        (*it).Remove();
    }
}


void CCleanup_imp::x_ActOnSeqAnnots (CBioseq_set_Handle bss, RecurseSeqAnnot pmf)
{
    CBioseq_set_EditHandle bseh = bss.GetEditHandle();

    vector<CSeq_annot_EditHandle> sah; // copy empty annot handles to not break iterator while moving    
    CSeq_annot_CI annot_it(bseh.GetParentEntry(), CSeq_annot_CI::eSearch_entry);
    for(; annot_it; ++annot_it) {
        (this->*pmf)(*annot_it);
        if (annot_it->IsFtable() && annot_it->GetCompleteSeq_annot()->GetData().GetFtable().empty()) {
            // add annot_it to list of annotations to remove
            sah.push_back((*annot_it).GetEditHandle());
        }
    }
    ITERATE ( vector<CSeq_annot_EditHandle>, it, sah ) {
        (*it).Remove();
    }
}


void CCleanup_imp::x_RecurseForSeqAnnots (CBioseq_set_Handle bss, RecurseSeqAnnot pmf)
{
    x_ActOnSeqAnnots(bss, pmf);

    // now operate on members of set
    if (bss.GetCompleteBioseq_set()->IsSetSeq_set()) {
       CConstRef<CBioseq_set> b = bss.GetCompleteBioseq_set();
       list< CRef< CSeq_entry > > set = (*b).GetSeq_set();
       
       ITERATE (list< CRef< CSeq_entry > >, it, set) {
           x_RecurseForSeqAnnots (**it, pmf);
       }
    }
}


bool IsEmpty (CGene_ref& gene_ref)
{
    if ((!gene_ref.CanGetLocus() || NStr::IsBlank (gene_ref.GetLocus()))
        && (!gene_ref.CanGetAllele() || NStr::IsBlank (gene_ref.GetAllele()))
        && (!gene_ref.CanGetDesc() || NStr::IsBlank (gene_ref.GetDesc()))
        && (!gene_ref.CanGetMaploc() || NStr::IsBlank (gene_ref.GetMaploc()))
        && (!gene_ref.CanGetLocus_tag() || NStr::IsBlank (gene_ref.GetLocus_tag()))
        && (!gene_ref.CanGetDb() || gene_ref.GetDb().size() == 0)
        && (!gene_ref.CanGetSyn() || gene_ref.GetSyn().size() == 0)) {
        return true;
    } else {
        return false;
    }   
}


bool IsEmpty (CSeq_feat& sf)
{
    if (sf.CanGetData() && sf.SetData().IsGene()) {
        if (IsEmpty(sf.SetData().SetGene())) {
            if (!sf.CanGetComment() || NStr::IsBlank (sf.GetComment())) {
                return true;
            } else {
                // convert this feature to a misc_feat
                sf.SetData().Reset();
                sf.SetData().SetImp().SetKey("misc_feature");
                sf.SetData().InvalidateSubtype();
            }
        }
    } else if (sf.CanGetData() && sf.SetData().IsProt()) {
        CProt_ref& prot = sf.SetData().SetProt();
        if (prot.CanGetProcessed()) {
            unsigned int processed = sf.SetData().GetProt().GetProcessed();
            
            if (processed != CProt_ref::eProcessed_signal_peptide
                && processed != CProt_ref::eProcessed_transit_peptide
                && (!prot.CanGetName() || prot.GetName().size() == 0)
                && sf.CanGetComment()
                && NStr::StartsWith (sf.GetComment(), "putative")) {
                prot.SetName ().push_back(sf.GetComment());
                sf.ResetComment();
            }

            if (processed == CProt_ref::eProcessed_mature && (!prot.CanGetName() || prot.GetName().size() == 0)) {
                prot.SetName().push_back("unnamed");
            }
            
            if (processed != CProt_ref::eProcessed_signal_peptide
                && processed != CProt_ref::eProcessed_transit_peptide
                && (!prot.CanGetName() 
                    || prot.GetName().size() == 0 
                    || NStr::IsBlank(prot.GetName().front()))
                && (!prot.CanGetDesc() || NStr::IsBlank(prot.GetDesc()))
                && (!prot.CanGetEc() || prot.GetEc().size() == 0)
                && (!prot.CanGetActivity() || prot.GetActivity().size() == 0)
                && (!prot.CanGetDb() || prot.GetDb().size() == 0)) {
                return true;
            }
        }
    } else if (sf.CanGetData() && sf.SetData().IsComment()) {
        if (!sf.CanGetComment() || NStr::IsBlank (sf.GetComment())) {
            return true;
        }
    }
    return false;
}


// Was GetRidOfEmptyFeatsDescCallback in the C Toolkit
void CCleanup_imp::RemoveEmptyFeaturesDescriptorsAndAnnots (CBioseq_Handle bs)
{
    CBioseq_EditHandle bseh = bs.GetEditHandle();
    
    // combine adjacent annotations
    vector<CSeq_annot_EditHandle> sah; // copy annot handles to not to break iterator while moving
    CSeq_annot_CI annot_it(bseh.GetSeq_entry_Handle(), CSeq_annot_CI::eSearch_entry);
    for(; annot_it; ++annot_it) {
        if ((*annot_it).IsFtable()) {
            CFeat_CI feat_ci((*annot_it));
            if (!feat_ci) {
                // add annot_it to list of annotations to remove
                sah.push_back((*annot_it).GetEditHandle());
            }
        }
    }
    // move annots from one place to another without copying their contents
    ITERATE ( vector<CSeq_annot_EditHandle>, it, sah ) {
        (*it).Remove();
    }
}


void CCleanup_imp::RemoveEmptyFeaturesDescriptorsAndAnnots (CBioseq_set_Handle bs)
{
    CBioseq_set_EditHandle bseh = bs.GetEditHandle();
    
    // combine adjacent annotations
    vector<CSeq_annot_EditHandle> sah; // copy annot handles to not to break iterator while moving
    CSeq_annot_CI annot_it(bseh.GetParentEntry(), CSeq_annot_CI::eSearch_entry);
    for(; annot_it; ++annot_it) {
        if ((*annot_it).IsFtable()) {
            CFeat_CI feat_ci((*annot_it));
            if (!feat_ci) {
                // add annot_it to list of annotations to remove
                sah.push_back((*annot_it).GetEditHandle());
            }
        }
    }
    // move annots from one place to another without copying their contents
    ITERATE ( vector<CSeq_annot_EditHandle>, it, sah ) {
        (*it).Remove();
    }
    
    if (bs.GetCompleteBioseq_set()->IsSetSeq_set()) {
       CConstRef<CBioseq_set> b = bs.GetCompleteBioseq_set();
       list< CRef< CSeq_entry > > set = (*b).GetSeq_set();
       
       ITERATE (list< CRef< CSeq_entry > >, it, set) {
            switch ((**it).Which()) {
                case CSeq_entry::e_Seq:
                    RemoveEmptyFeaturesDescriptorsAndAnnots(m_Scope->GetBioseqHandle((**it).GetSeq()));
                    break;
                case CSeq_entry::e_Set:
                    RemoveEmptyFeaturesDescriptorsAndAnnots(m_Scope->GetBioseq_setHandle((**it).GetSet()));
                    break;
                case CSeq_entry::e_not_set:
                default:
                    break;
            }
        }
    }

}


void CCleanup_imp::x_RemoveEmptyFeatures (CSeq_annot_Handle sa) 
{
    if (sa.IsFtable()) {
        CFeat_CI feat_ci(sa);
        while (feat_ci) {
            const CSeq_feat &cf = feat_ci->GetOriginalFeature();
            if (IsEmpty(const_cast<CSeq_feat &>(cf))) {
                CSeq_feat_EditHandle efh (feat_ci->GetSeq_feat_Handle());
                efh.Remove();
                ChangeMade(CCleanupChange::eRemoveFeat);
            }
            ++feat_ci;
        }    
    }
}


void CCleanup_imp::x_RemoveEmptyFeatureAnnots (CBioseq_Handle bs)
{
    CBioseq_EditHandle bseh = bs.GetEditHandle();
    bool any_left = false;
    
    // combine adjacent annotations
    vector<CSeq_annot_EditHandle> sah; // copy annot handles to not to break iterator while moving
    CSeq_annot_CI annot_it(bseh.GetSeq_entry_Handle(), CSeq_annot_CI::eSearch_entry);
    while (annot_it) {
        if ((*annot_it).IsFtable()
            && (! (*annot_it).Seq_annot_CanGetId() 
                || (*annot_it).Seq_annot_GetId().size() == 0)
            && (! (*annot_it).Seq_annot_CanGetName() 
                || NStr::IsBlank((*annot_it).Seq_annot_GetName()))
            && (! (*annot_it).Seq_annot_CanGetDb()
                || (*annot_it).Seq_annot_GetDb() == 0)
            && (! (*annot_it).Seq_annot_CanGetDesc()
                || (*annot_it).Seq_annot_GetDesc().Get().size() == 0)) {
            CFeat_CI feat_ci((*annot_it));
            if (!feat_ci) {
                // add annot_it to list of annotations to remove
                (*annot_it).GetEditHandle().Remove();
                ChangeMade (CCleanupChange::eRemoveAnnot);
            } else {
                ++annot_it;
                any_left = true;
            }
        } else {
            ++annot_it;
            any_left = true;
        }
    }
    if (!any_left) {
        // TODO: Need to reset Annot list if empty
    }    
}


void CCleanup_imp::x_RemoveEmptyFeatureAnnots (CBioseq_set_Handle bs)
{
    CBioseq_set_EditHandle bseh = bs.GetEditHandle();
    
    bool any_left = false;
    
    // combine adjacent annotations
    vector<CSeq_annot_EditHandle> sah; // copy annot handles to not to break iterator while moving
    CSeq_annot_CI annot_it(bseh.GetParentEntry(), CSeq_annot_CI::eSearch_entry);
    while (annot_it) {
        if ((*annot_it).IsFtable()
            && (! (*annot_it).Seq_annot_CanGetId() 
                || (*annot_it).Seq_annot_GetId().size() == 0)
            && (! (*annot_it).Seq_annot_CanGetName() 
                || NStr::IsBlank((*annot_it).Seq_annot_GetName()))
            && (! (*annot_it).Seq_annot_CanGetDb()
                || (*annot_it).Seq_annot_GetDb() == 0)
            && (! (*annot_it).Seq_annot_CanGetDesc()
                || (*annot_it).Seq_annot_GetDesc().Get().size() == 0)) {
            CFeat_CI feat_ci((*annot_it));
            if (!feat_ci) {
                // add annot_it to list of annotations to remove
                (*annot_it).GetEditHandle().Remove();
                ChangeMade (CCleanupChange::eRemoveAnnot);                
            } else {
                ++annot_it;
                any_left = true;
            }
        } else {
            ++annot_it;
            any_left = true;
        }
    }
    if (!any_left) {
        // TODO: Need to reset annot list if empty
    }    
}


// combine CSeq_annot objects if two adjacent objects in the list are both
// feature table annotations, both have no id, no name, no db, and no description
// Was MergeAdjacentAnnotsCallback in C Toolkit

void CCleanup_imp::x_MergeAdjacentAnnots (CBioseq_Handle bs)
{
    CBioseq_EditHandle bseh = bs.GetEditHandle();
    
    // combine adjacent annotations
    vector<CSeq_annot_EditHandle> sah; // copy annot handles to not to break iterator while moving
    CSeq_annot_EditHandle prev;
    bool prev_can_merge = false;
    CSeq_annot_CI annot_it(bseh.GetSeq_entry_Handle(), CSeq_annot_CI::eSearch_entry);
    for(; annot_it; ++annot_it) {
        if ((*annot_it).IsFtable()
            && (! (*annot_it).Seq_annot_CanGetId() 
                || (*annot_it).Seq_annot_GetId().size() == 0)
            && (! (*annot_it).Seq_annot_CanGetName() 
                || NStr::IsBlank((*annot_it).Seq_annot_GetName()))
            && (! (*annot_it).Seq_annot_CanGetDb()
                || (*annot_it).Seq_annot_GetDb() == 0)
            && (! (*annot_it).Seq_annot_CanGetDesc()
                || (*annot_it).Seq_annot_GetDesc().Get().size() == 0)) {
            if (prev_can_merge) {
                vector<CSeq_feat_EditHandle> sfh; // copy seqfeat handles to not break iterator while moving
                // merge features from annot_it into prev
                CFeat_CI feat_ci((*annot_it));
                while (feat_ci) {
                    sfh.push_back(CSeq_feat_EditHandle (feat_ci->GetSeq_feat_Handle()));
                    ++feat_ci;
                }
                // move features to prev annot
                ITERATE (vector<CSeq_feat_EditHandle>, it, sfh) {
                  prev.TakeFeat (*it);
                  ChangeMade(CCleanupChange::eMoveFeat);
                }
                // add annot_it to list of annotations to remove
                sah.push_back((*annot_it).GetEditHandle());
                
            } else {
                prev = (*annot_it).GetEditHandle();
            }
            prev_can_merge = true;
        } else {
            prev_can_merge = false;
        }
    }
    // move annots from one place to another without copying their contents
    ITERATE ( vector<CSeq_annot_EditHandle>, it, sah ) {
        (*it).Remove();
        ChangeMade(CCleanupChange::eRemoveAnnot);
    }
   
}


void CCleanup_imp::x_MergeAdjacentAnnots (CBioseq_set_Handle bs)
{
    CBioseq_set_EditHandle bseh = bs.GetEditHandle();
    
    // combine adjacent annotations
    vector<CSeq_annot_EditHandle> sah; // copy annot handles to not to break iterator while moving
    CSeq_annot_EditHandle prev;
    bool prev_can_merge = false;
    CSeq_annot_CI annot_it(bseh.GetParentEntry(), CSeq_annot_CI::eSearch_entry);
    for(; annot_it; ++annot_it) {
        if ((*annot_it).IsFtable()
            && (! (*annot_it).Seq_annot_CanGetId() 
                || (*annot_it).Seq_annot_GetId().size() == 0)
            && (! (*annot_it).Seq_annot_CanGetName() 
                || NStr::IsBlank((*annot_it).Seq_annot_GetName()))
            && (! (*annot_it).Seq_annot_CanGetDb()
                || (*annot_it).Seq_annot_GetDb() == 0)
            && (! (*annot_it).Seq_annot_CanGetDesc()
                || (*annot_it).Seq_annot_GetDesc().Get().size() == 0)) {
            if (prev_can_merge) {
                vector<CSeq_feat_EditHandle> sfh; // copy seqfeat handles to not break iterator while moving
                // merge features from annot_it into prev
                CFeat_CI feat_ci((*annot_it));
                while (feat_ci) {
                    sfh.push_back(CSeq_feat_EditHandle (feat_ci->GetSeq_feat_Handle()));
                    ++feat_ci;
                }
                // move features to prev annot
                ITERATE (vector<CSeq_feat_EditHandle>, it, sfh) {
                  prev.TakeFeat (*it);
                  ChangeMade(CCleanupChange::eMoveFeat);
                }
                // add annot_it to list of annotations to remove
                sah.push_back((*annot_it).GetEditHandle());
                
            } else {
                prev = (*annot_it).GetEditHandle();
            }
            prev_can_merge = true;
        } else {
            prev_can_merge = false;
        }
    }
    // move annots from one place to another without copying their contents
    ITERATE ( vector<CSeq_annot_EditHandle>, it, sah ) {
        (*it).Remove();
        ChangeMade(CCleanupChange::eRemoveAnnot);
    }

    // now operate on members of set
    if (bs.GetCompleteBioseq_set()->IsSetSeq_set()) {
       CConstRef<CBioseq_set> b = bs.GetCompleteBioseq_set();
       list< CRef< CSeq_entry > > set = (*b).GetSeq_set();
       
       ITERATE (list< CRef< CSeq_entry > >, it, set) {
            switch ((**it).Which()) {
                case CSeq_entry::e_Seq:
                    x_MergeAdjacentAnnots(m_Scope->GetBioseqHandle((**it).GetSeq()));
                    break;
                case CSeq_entry::e_Set:
                    x_MergeAdjacentAnnots(m_Scope->GetBioseq_setHandle((**it).GetSet()));
                    break;
                case CSeq_entry::e_not_set:
                default:
                    break;
            }
        }
    }
   
}


// If the feature location is exactly the entire length of the Bioseq that the feature is 
// located on, convert the feature to a descriptor on the same Bioseq.
// Was ConvertFullLenSourceFeatToDesc in C Toolkit
void CCleanup_imp::x_ConvertFullLenFeatureToDescriptor (CSeq_annot_Handle sa, CSeqFeatData::E_Choice choice)
{
    if (sa.IsFtable()) {
        CFeat_CI feat_ci(sa);
        while (feat_ci) {
            const CSeq_feat& cf = feat_ci->GetOriginalFeature();
            if (cf.CanGetData() && cf.GetData().Which()  == choice
                && IsFeatureFullLength(cf, m_Scope)) {
                CBioseq_Handle bh = m_Scope->GetBioseqHandle(cf.GetLocation());  
                
                if (choice == CSeqFeatData::e_Biosrc
                    && !x_OkToConvertToDescriptor(bh, cf.GetData().GetBiosrc())) {
                    // don't convert
                } else {
                    CRef<CSeqdesc> desc(new CSeqdesc);
             
                    if (choice == CSeqFeatData::e_Biosrc) {
                        desc->Select(CSeqdesc::e_Source);
                        desc->SetSource(const_cast< CBioSource& >(cf.GetData().GetBiosrc()));
                    } else if (choice == CSeqFeatData::e_Pub) {
                        desc->Select(CSeqdesc::e_Pub);
                        desc->SetPub(const_cast< CPubdesc& >(cf.GetData().GetPub()));
                    }
            
                    CBioseq_EditHandle eh = bh.GetEditHandle();
                    eh.AddSeqdesc(*desc);
                    CSeq_feat_EditHandle efh (feat_ci->GetSeq_feat_Handle());
                    efh.Remove();
                    ChangeMade (CCleanupChange::eConvertFeatureToDescriptor);
                }
            }
            ++feat_ci;                
        }
    }
}


void CCleanup_imp::x_ConvertFullLenPubFeatureToDescriptor (CSeq_annot_Handle sah)
{
    x_ConvertFullLenFeatureToDescriptor(sah, CSeqFeatData::e_Pub);
}


// collapse nuc-prot sets with only one seq-entry
void CCleanup_imp::RenormalizeNucProtSets (CBioseq_set_Handle bsh)
{
    CConstRef<CBioseq_set> b = bsh.GetCompleteBioseq_set();
    if (bsh.CanGetClass() && b->IsSetSeq_set()) {
        unsigned int bclass = bsh.GetClass();

        list< CRef< CSeq_entry > > set = (*b).GetSeq_set();

        if (bclass == CBioseq_set::eClass_genbank
            || bclass == CBioseq_set::eClass_mut_set
            || bclass == CBioseq_set::eClass_pop_set
            || bclass == CBioseq_set::eClass_phy_set
            || bclass == CBioseq_set::eClass_eco_set
            || bclass == CBioseq_set::eClass_wgs_set
            || bclass == CBioseq_set::eClass_gen_prod_set) {
       
            ITERATE (list< CRef< CSeq_entry > >, it, set) {
                if ((**it).Which() == CSeq_entry::e_Set) {
                    RenormalizeNucProtSets (m_Scope->GetBioseq_setHandle((**it).GetSet()));
                }
            }
        } else if (bclass == CBioseq_set::eClass_nuc_prot && set.size() == 1) {
            // collapse nuc-prot set
            CSeq_entry_EditHandle seh = bsh.GetEditHandle().GetParentEntry();
            
            seh.CollapseSet();
            ChangeMade(CCleanupChange::eCollapseSet);
            if (seh.IsSetDescr()) {
                CSeq_descr::Tdata remove_list;

                remove_list.clear();
                
                if (seh.IsSeq()) {
                    CBioseq_EditHandle eh = seh.GetSeq().GetEditHandle();
                    x_RemoveMultipleTitles(eh.SetDescr(), remove_list);
                    for (CSeq_descr::Tdata::iterator it1 = remove_list.begin();
                        it1 != remove_list.end(); ++it1) { 
                        eh.RemoveSeqdesc(**it1);
                        ChangeMade(CCleanupChange::eRemoveDescriptor);
                    }        
                } else if (seh.IsSet()) {
                    CBioseq_set_EditHandle eh = seh.GetSet().GetEditHandle();
                    x_RemoveMultipleTitles(eh.SetDescr(), remove_list);
                    for (CSeq_descr::Tdata::iterator it1 = remove_list.begin();
                        it1 != remove_list.end(); ++it1) { 
                        eh.RemoveSeqdesc(**it1);
                        ChangeMade(CCleanupChange::eRemoveDescriptor);
                    }        
                }
            }
        }
    }
}

/*
static bool s_HasBackboneID (CBioseq_Handle bs)
{
    ITERATE (CBioseq::TId, it, bs.GetBioseqCore()->GetId()) {
        if ((*it)->Which() == CSeq_id::e_Gibbsq
            || (*it)->Which() == CSeq_id::e_Gibbmt) {
            return true;
        }
    }
    return false;
}
*/

void CCleanup_imp::CheckNucProtSet (CBioseq_set_Handle bss)
{
    if (!bss.CanGetClass()) {
        return;
    }
 
    CConstRef<CBioseq_set> b = bss.GetCompleteBioseq_set();
    list< CRef< CSeq_entry > > set = (*b).GetSeq_set();

    if (bss.GetClass() == CBioseq_set::eClass_nuc_prot) {
        list< CRef< CSeq_entry > >::const_iterator se_it = set.begin();
        
        CBioseq_set_EditHandle eh = bss.GetEditHandle();

        // look for old style nps title, remove if based on nucleotide title, also remove exact duplicate
        x_RemoveNucProtSetTitle(eh, **se_it);
        
        // promote descriptors on nuc to nuc-prot set if type not already on nuc-prot set
        x_ExtractNucProtDescriptors(eh, **se_it, CSeqdesc::e_Update_date);

    }
    
    ITERATE (list< CRef< CSeq_entry > >, it, set) {
        if ((*it)->Which() == CSeq_entry::e_Set) {
            CheckNucProtSet(m_Scope->GetBioseq_setHandle((**it).GetSet()));
        }
	}
}


END_SCOPE(objects)
END_NCBI_SCOPE
