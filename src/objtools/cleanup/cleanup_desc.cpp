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
 * Author:  Robert G. Smith
 *
 * File Description:
 *   implementation of BasicCleanup for CBioSource and sub-objects.
 *
 */

#include <ncbi_pch.hpp>
#include "cleanup_utils.hpp"
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <corelib/ncbistr.hpp>
#include <set>

#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seqblock/GB_block.hpp>

#include <objects/general/Dbtag.hpp>
#include <objects/general/Date.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seq_annot_ci.hpp>
#include <objmgr/feat_ci.hpp>

#include "cleanupp.hpp"


BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::


void CCleanup_imp::x_RecurseForDescriptors (const CSeq_entry& se, RecurseDescriptor pmf)
{
    switch (se.Which()) {
        case CSeq_entry::e_Seq:
            x_RecurseForDescriptors(m_Scope->GetBioseqHandle(se.GetSeq()), pmf);
            break;
        case CSeq_entry::e_Set:
            x_RecurseForDescriptors(m_Scope->GetBioseq_setHandle(se.GetSet()), pmf);
            break;
        case CSeq_entry::e_not_set:
        default:
            break;
    }
}


void CCleanup_imp::x_RecurseForDescriptors (CBioseq_Handle bs, RecurseDescriptor pmf)
{
    if (bs.IsSetDescr()) {
        CSeq_descr::Tdata remove_list;

        remove_list.clear();
        CBioseq_EditHandle eh = bs.GetEditHandle();    
        (this->*pmf)(eh.SetDescr(), remove_list);
        for (CSeq_descr::Tdata::iterator it1 = remove_list.begin();
            it1 != remove_list.end(); ++it1) { 
            eh.RemoveSeqdesc(**it1);
            ChangeMade(CCleanupChange::eRemoveDescriptor);
        }        
        if (eh.GetDescr().Get().empty()) {
            eh.ResetDescr();
        }
    }
}


void CCleanup_imp::x_ActOnDescriptors (CBioseq_set_Handle bss, RecurseDescriptor pmf)
{
    if (bss.IsSetDescr()) {
        CSeq_descr::Tdata remove_list;

        remove_list.clear();
        CBioseq_set_EditHandle eh = bss.GetEditHandle();
        (this->*pmf)(eh.SetDescr(), remove_list);

        for (CSeq_descr::Tdata::iterator it1 = remove_list.begin();
            it1 != remove_list.end(); ++it1) { 
            eh.RemoveSeqdesc(**it1);
            ChangeMade(CCleanupChange::eRemoveDescriptor);
        }   
        if (eh.GetDescr().Get().empty()) {
            eh.ResetDescr();
        }             
    }
}


void CCleanup_imp::x_RecurseForDescriptors (CBioseq_set_Handle bss, RecurseDescriptor pmf)
{
    x_ActOnDescriptors (bss, pmf);

    if (bss.GetCompleteBioseq_set()->IsSetSeq_set()) {
       CBioseq_set_EditHandle eh = bss.GetEditHandle();
       CConstRef<CBioseq_set> b = bss.GetCompleteBioseq_set();
       list< CRef< CSeq_entry > > set = (*b).GetSeq_set();
       
       ITERATE (list< CRef< CSeq_entry > >, it, set) {
            x_RecurseForDescriptors (**it, pmf);
        }
    }
}


void CCleanup_imp::x_RecurseDescriptorsForMerge(CSeq_descr& desc, IsMergeCandidate is_can, Merge do_merge, CSeq_descr::Tdata& remove_list)
{
    for (CSeq_descr::Tdata::iterator it1 = desc.Set().begin();
        it1 != desc.Set().end(); ++it1) { 
        CRef< CSeqdesc > sd = *it1;
        if ((this->*is_can)(*sd)) {
            bool found_match = false;
            CSeq_descr::Tdata::iterator it2 = it1;
            it2++;
            while (it2 != desc.Set().end() && !found_match) {
                if ((this->*is_can)(**it2)) {
                    if ((this->*do_merge)(**it2, *sd)) {
                        found_match = true;
                    }
                }
                ++it2;
            }
            if (found_match) {
               remove_list.push_front(sd);
            }
        }
    }
}


void CCleanup_imp::x_RecurseDescriptorsForMerge(CBioseq_Handle bh, IsMergeCandidate is_can, Merge do_merge)
{
    if (!bh.IsSetDescr()) {
        return;
    }
    CSeq_descr::Tdata remove_list;    

    CBioseq_EditHandle edith = m_Scope->GetEditHandle(bh);
     
    remove_list.clear();

    x_RecurseDescriptorsForMerge(edith.SetDescr(), is_can, do_merge, remove_list);
    
    for (CSeq_descr::Tdata::iterator it1 = remove_list.begin();
        it1 != remove_list.end(); ++it1) { 
        edith.RemoveSeqdesc(**it1);
        ChangeMade(CCleanupChange::eRemoveDescriptor);
    }
}


void CCleanup_imp::x_ActOnDescriptorsForMerge (CBioseq_set_Handle bh, IsMergeCandidate is_can, Merge do_merge)
{
    if (bh.IsSetDescr()) {
        CSeq_descr::Tdata remove_list;    

        CBioseq_set_EditHandle edith = m_Scope->GetEditHandle(bh);
     
        remove_list.clear();

        x_RecurseDescriptorsForMerge(edith.SetDescr(), is_can, do_merge, remove_list);
    
        for (CSeq_descr::Tdata::iterator it1 = remove_list.begin();
            it1 != remove_list.end(); ++it1) { 
            edith.RemoveSeqdesc(**it1);
            ChangeMade(CCleanupChange::eRemoveDescriptor);
        }
    }
}


void CCleanup_imp::x_RecurseDescriptorsForMerge(CBioseq_set_Handle bh, IsMergeCandidate is_can, Merge do_merge)
{
    x_ActOnDescriptorsForMerge (bh, is_can, do_merge);
    
    if (bh.GetCompleteBioseq_set()->IsSetSeq_set()) {
       CConstRef<CBioseq_set> b = bh.GetCompleteBioseq_set();
       list< CRef< CSeq_entry > > set = (*b).GetSeq_set();
       
       ITERATE (list< CRef< CSeq_entry > >, it, set) {
            switch ((**it).Which()) {
                case CSeq_entry::e_Seq:
                    x_RecurseDescriptorsForMerge(m_Scope->GetBioseqHandle((**it).GetSeq()), is_can, do_merge);
                    break;
                case CSeq_entry::e_Set:
                    x_RecurseDescriptorsForMerge(m_Scope->GetBioseq_setHandle((**it).GetSet()), is_can, do_merge);
                    break;
                case CSeq_entry::e_not_set:
                default:
                    break;
            }
        }
    }
}


bool IsEmpty (CGB_block& block) 
{
    if ((!block.CanGetExtra_accessions() || block.GetExtra_accessions().size() == 0)
        && (!block.CanGetSource() || NStr::IsBlank(block.GetSource()))
        && (!block.CanGetKeywords() || block.GetKeywords().size() == 0)
        && (!block.CanGetOrigin() || NStr::IsBlank(block.GetOrigin()))
        && (!block.CanGetDate() || NStr::IsBlank (block.GetDate()))
        && (!block.CanGetEntry_date())
        && (!block.CanGetDiv() || NStr::IsBlank(block.GetDiv()))
        && (!block.CanGetTaxonomy() || NStr::IsBlank(block.GetTaxonomy()))) {
        return true;
    } else {
        return false;
    }
}


// Was CleanupGenbankCallback in C Toolkit
// removes taxonomy
// remove Genbank Block descriptors when
//      if (gbp->extra_accessions == NULL && gbp->source == NULL &&
//         gbp->keywords == NULL && gbp->origin == NULL &&
//         gbp->date == NULL && gbp->entry_date == NULL &&
//         gbp->div == NULL && gbp->taxonomy == NULL) {

void CCleanup_imp::x_RemoveEmptyGenbankDesc(CSeq_descr& sdr, CSeq_descr::Tdata& remove_list)
{
    NON_CONST_ITERATE (CSeq_descr::Tdata, it, sdr.Set()) {
        if ((*it)->Which() == CSeqdesc::e_Genbank) {
            CGB_block& block = (*it)->SetGenbank();
            block.ResetTaxonomy();
            (*it)->SetGenbank(block);
            if (IsEmpty(block)) {
                remove_list.push_back(*it);
            }
        }
    }
}


// was EntryCheckGBBlock in C Toolkit
// cleans strings for the GenBankBlock, removes descriptor if block is now empty
void CCleanup_imp::x_CleanGenbankBlockStrings (CGB_block& block)
{
    bool changed = false;
    // clean extra accessions
    if (block.IsSetExtra_accessions()) {
        changed |= CleanStringContainer(block.SetExtra_accessions());
        if (block.GetExtra_accessions().size() == 0) {
            block.ResetExtra_accessions();
            changed = true;
        }
    }
                
    // clean keywords
    if (block.IsSetKeywords()) {
        changed |= CleanStringContainer(block.SetKeywords());
        if (block.GetKeywords().size() == 0) {
            block.ResetKeywords();
            changed = true;
        }
    }
                
    // clean source
    if (block.IsSetSource()) {
        string source = block.GetSource();
        changed |= CleanString (source);
        if (NStr::IsBlank (source)) {
            block.ResetSource();
            changed = true;
        } else {
            block.SetSource (source);
        }
    }
    // clean origin
    if (block.IsSetOrigin()) {
        string origin = block.GetOrigin();
        changed |= CleanString (origin);
        if (NStr::IsBlank (origin)) {
            block.ResetOrigin();
            changed = true;
        } else {
            block.SetOrigin(origin);
        }
    }
    //clean date
    if (block.IsSetDate()) {
        string date = block.GetDate();
        changed |= CleanString (date);
        if (NStr::IsBlank (date)) {
            block.ResetDate();
            changed = true;
        } else {
            block.SetDate(date);
        }
    }
    //clean div
    if (block.IsSetDiv()) {
        string div = block.GetDiv();
        changed |= CleanString (div);
        if (NStr::IsBlank (div)) {
            block.ResetDiv();
            changed = true;
        } else {
            block.SetDiv(div);
        }
    }
    //clean taxonomy
    if (block.IsSetTaxonomy()) {
        string tax = block.GetTaxonomy();
        if (NStr::IsBlank (tax)) {
            block.ResetTaxonomy();
            changed = true;
        } else {
            block.SetTaxonomy(tax);
        }
    }
    if (changed) {
        ChangeMade(CCleanupChange::eChangeOther);
    }
}


void CCleanup_imp::x_CleanGenbankBlockStrings (CSeq_descr& sdr, CSeq_descr::Tdata& remove_list)
{
    NON_CONST_ITERATE (CSeq_descr::Tdata, it, sdr.Set()) {
        if ((*it)->Which() == CSeqdesc::e_Genbank) {
            x_CleanGenbankBlockStrings((*it)->SetGenbank());
                
            if (IsEmpty((*it)->SetGenbank())) {
                remove_list.push_back(*it);
            }
        }
    }
}


// removes all title descriptors except the last one
// Was RemoveMultipleTitles in C Toolkit
void CCleanup_imp::x_RemoveMultipleTitles (CSeq_descr& sdr, CSeq_descr::Tdata& remove_list)
{
    int num_titles = 0;
    
    NON_CONST_REVERSE_ITERATE (CSeq_descr::Tdata, it, sdr.Set()) {
        if ((*it)->Which() == CSeqdesc::e_Title ) {
            if (num_titles == 0) {
                num_titles ++;
            } else {
                remove_list.push_back(*it);
            }
        }
    }
}


// removes all empty title descriptors
// Was part of RemoveEmptyTitleAndPubGenAsOnlyPub in C Toolkit
void CCleanup_imp::x_RemoveEmptyTitles (CSeq_descr& sdr, CSeq_descr::Tdata& remove_list)
{
    NON_CONST_REVERSE_ITERATE (CSeq_descr::Tdata, it, sdr.Set()) {
        if ((*it)->Which() == CSeqdesc::e_Title ) {
            if (NStr::IsBlank ((*it)->GetTitle())) {
                remove_list.push_back(*it);
            }
        }
    }
}


// Retain the most recent create date, remove the others
// Was MergeMultipleDates in C Toolkit
void CCleanup_imp::x_MergeMultipleDates (CSeq_descr& sdr, CSeq_descr::Tdata& remove_list)
{
    CSeq_descr::Tdata dates_list;    
    int num_dates = 0;
    
    dates_list.clear();
    
    NON_CONST_ITERATE (CSeq_descr::Tdata, it, sdr.Set()) {
        if ((**it).Which() == CSeqdesc::e_Create_date ) {
            if (dates_list.size() > 0) {
                if ((*(dates_list.back())).GetCreate_date().Compare ((**it).GetCreate_date()) == CDate::eCompare_before) {
                    dates_list.pop_back();
                    dates_list.push_back(*it);
                }
            } else {
                dates_list.push_back(*it);
            }
            num_dates ++;
        }
    }
    if (dates_list.size() == 0 || num_dates == 1) {
        return;
    }
    
    num_dates = 0;
    NON_CONST_ITERATE (CSeq_descr::Tdata, it, sdr.Set()) {
        if ((*it)->Which() == CSeqdesc::e_Create_date ) {
            if (num_dates > 0
                || (*(dates_list.back())).GetCreate_date().Compare ((*it)->GetCreate_date()) != CDate::eCompare_same) {
                remove_list.push_back(*it);
            } else {
                num_dates ++;
            }
        }
                
    }
}


// Used by x_ChangeGenBankBlocks
void CCleanup_imp::x_CheckGenbankBlockTechKeywords(CGB_block& gb_block, CMolInfo::TTech tech)
{
    if (!gb_block.IsSetKeywords()) {
        return;
    }
    
    CGB_block::TKeywords::iterator it = gb_block.SetKeywords().begin();
    while (it != gb_block.SetKeywords().end()) {
        if (NStr::Equal((*it), "HTG")
            || (tech == CMolInfo::eTech_htgs_0 && NStr::Equal((*it), "HTGS_PHASE0"))     
            || (tech == CMolInfo::eTech_htgs_1 && NStr::Equal((*it), "HTGS_PHASE1"))     
            || (tech == CMolInfo::eTech_htgs_2 && NStr::Equal((*it), "HTGS_PHASE2"))     
            || (tech == CMolInfo::eTech_htgs_3 && NStr::Equal((*it), "HTGS_PHASE3"))     
            || (tech == CMolInfo::eTech_est && NStr::Equal((*it), "EST"))     
            || (tech == CMolInfo::eTech_sts && NStr::Equal((*it), "STS"))) {
            it = gb_block.SetKeywords().erase(it);
            ChangeMade(CCleanupChange::eRemoveKeyword);
        } else {
            ++it;
        }
    }
}


// Used by x_ChangeGenBankBlocks
void CCleanup_imp::x_ChangeGBDiv (CSeq_entry_Handle seh, string div)
{
    CSeqdesc_CI molinfo (seh, CSeqdesc::e_Molinfo, 1);

    CSeqdesc_CI gbdesc_i (seh, CSeqdesc::e_Genbank, 1);
    while (gbdesc_i) {
        CGB_block& gb_block = const_cast<CGB_block&> ((*gbdesc_i).GetGenbank());
        CMolInfo::TTech tech = CMolInfo::eTech_unknown;
        if (molinfo && (*molinfo).GetMolinfo().CanGetTech()) {
            CMolInfo::TTech tech = (*molinfo).GetMolinfo().GetTech();
            if (tech == CMolInfo::eTech_htgs_0
                || tech == CMolInfo::eTech_htgs_1
                || tech == CMolInfo::eTech_htgs_2
                || tech == CMolInfo::eTech_htgs_3
                || tech == CMolInfo::eTech_est
                || tech == CMolInfo::eTech_sts) {
                x_CheckGenbankBlockTechKeywords(gb_block, tech);
            }
        }
        if (gb_block.CanGetDiv()) {
            string gb_div = gb_block.GetDiv();
            if (!NStr::IsBlank(gb_div)
                && molinfo
                && (NStr::Equal(gb_div, "EST")
                    || NStr::Equal(gb_div, "STS")
                    || NStr::Equal(gb_div, "GSS")
                    || NStr::Equal(gb_div, "HTG"))) {
                if (NStr::Equal(gb_div, "HTG") || NStr::Equal(gb_div, "PRI")) {
                    if (tech == CMolInfo::eTech_htgs_1
                        || tech == CMolInfo::eTech_htgs_2
                        || tech == CMolInfo::eTech_htgs_3) {
                        gb_block.ResetDiv();
                        ChangeMade(CCleanupChange::eChangeOther);
                    }
                } else if ((tech == CMolInfo::eTech_est && NStr::Equal(gb_div, "EST"))
                           || (tech == CMolInfo::eTech_sts && NStr::Equal(gb_div, "STS"))
                           || (tech == CMolInfo::eTech_survey && NStr::Equal(gb_div, "GSS"))
                           || ((tech == CMolInfo::eTech_htgs_1 
                                || tech == CMolInfo::eTech_htgs_2
                                || tech == CMolInfo::eTech_htgs_3) && NStr::Equal(gb_div, "HTG"))) {
                    gb_block.ResetDiv();
                    ChangeMade(CCleanupChange::eChangeOther);
                }
			      }
			      if (!NStr::IsBlank(div)) {
			          if (NStr::Equal(div, gb_div)) {
			              gb_block.ResetDiv();
			              gb_block.ResetTaxonomy();
			          } else if (NStr::Equal(gb_div, "UNA")
			               || NStr::Equal(gb_div, "UNC")) {
			              gb_block.ResetDiv();
			              ChangeMade(CCleanupChange::eChangeOther);
			          }
			      } 
		    }
		    ++gbdesc_i;
	  }
}

// Was EntryChangeGBSource in C Toolkit
void CCleanup_imp::x_ChangeGenBankBlocks(CSeq_entry_Handle seh)
{
    CSeqdesc_CI desc_i(seh, CSeqdesc::e_Source);
    bool changed = false;
    
    if (desc_i && (*desc_i).GetSource().CanGetOrg()) {
        string src;
        
        const COrg_ref& org = (*desc_i).GetSource().GetOrg();
        
        if (org.CanGetCommon()
            && !NStr::IsBlank(org.GetCommon())) {
            src = org.GetCommon();
        } else if (org.CanGetTaxname()) {
            src = org.GetTaxname();
        }
        if (org.IsSetMod()) {
            list<string>::const_iterator it = org.GetMod().begin();
            while (it != org.GetMod().end()) {
                src += " ";
                src += (*it);
                ++it;
                changed = true;
            }
        }
        if (NStr::EndsWith(src, '.')) {
            src = src.substr(0, src.length() - 2);
        }
        
        if (!NStr::IsBlank(src)) {
            CSeqdesc_CI gbdesc_i(seh, CSeqdesc::e_Genbank);
            while (gbdesc_i) {
                if ((*gbdesc_i).GetGenbank().CanGetSource()) {
                    string gb_src = (*gbdesc_i).GetGenbank().GetSource();
                    if (NStr::EndsWith(gb_src, '.')) {
                        gb_src = gb_src.substr(0, gb_src.length() - 1);
                    }
                    if (NStr::EqualNocase(src, gb_src)) {
                        CGB_block& gb_block = const_cast<CGB_block&> ((*gbdesc_i).GetGenbank());
                        gb_block.ResetSource();
                        changed = true;
                    }
                }
                ++gbdesc_i;
            }
        }
        
        string div;
        
        if (org.CanGetOrgname() && org.GetOrgname().CanGetDiv()) {
            div = org.GetOrgname().GetDiv();
        }
        if (seh.IsSet()) {
            CSeq_entry_CI seq_iter(seh);
            while (seq_iter) {
                x_ChangeGBDiv (*seq_iter, div);
                ++seq_iter;
            }
        } else {
            x_ChangeGBDiv (seh, div);
        }
    }
    if (changed) {
        ChangeMade(CCleanupChange::eChangeOther);
    }
}


bool CCleanup_imp::x_SeqDescMatch (const CSeqdesc& d1, const CSeqdesc& d2)
{
    if (d1.Equals(d2)) {
        return true;
    } else {
        return false;
    }
}

bool CCleanup_imp::x_IsDescrSameForAllInPartsSet (CBioseq_set_Handle bss, CSeqdesc::E_Choice desctype, CSeq_descr::Tdata& desc_list)
{
    if (!bss.CanGetClass() || bss.GetClass() != CBioseq_set::eClass_parts) {
        return false;
    }
    
    CConstRef<CBioseq_set> b = bss.GetCompleteBioseq_set();
    list< CRef< CSeq_entry > > set = (*b).GetSeq_set();

    ITERATE (list< CRef< CSeq_entry > >, it, set) {
        if ((*it)->Which() == CSeq_entry::e_Seq) {
            CBioseq_EditHandle eh = (m_Scope->GetBioseqHandle((*it)->GetSeq())).GetEditHandle();
            if (eh.IsSetDescr()) {
                NON_CONST_ITERATE (CSeq_descr::Tdata, desc_it, eh.SetDescr().Set()) {
                    if ((*desc_it)->Which() == desctype ) {
                        if (desc_list.size() == 0) {
                            desc_list.push_back(*desc_it);
                        } else if (!x_SeqDescMatch (**desc_it, **(desc_list.begin()))) {
                            return false;
                        }
                    }
                }
            }                        
        } else if ((*it)->Which() == CSeq_entry::e_Set) {
            CBioseq_set_EditHandle eh = (m_Scope->GetBioseq_setHandle((*it)->GetSet())).GetEditHandle();
            if (eh.IsSetDescr()) {
                NON_CONST_ITERATE (CSeq_descr::Tdata, desc_it, eh.SetDescr().Set()) {
                    if ((*desc_it)->Which() == desctype ) {
                        if (desc_list.size() == 0) {
                            desc_list.push_back(*desc_it);
                        } else if (!x_SeqDescMatch (**desc_it, **(desc_list.begin()))) {
                            return false;
                        }
                    }
                }
            }                        
        }
    }
    
    if (desc_list.size() == 0) {
        return false;
    } else {
        return true;
    }
}


void CCleanup_imp::x_RemoveDescrByType(CBioseq_Handle bh, CSeqdesc::E_Choice desctype)
{
    CBioseq_EditHandle eh = bh.GetEditHandle();
    if (eh.IsSetDescr()) {
        CSeq_descr::Tdata desc_list;
        desc_list.clear();
        NON_CONST_ITERATE (CSeq_descr::Tdata, desc_it, eh.SetDescr().Set()) {
            if ((*desc_it)->Which() == desctype ) {
                desc_list.push_back(*desc_it);
            }
        }
        for (CSeq_descr::Tdata::iterator desc_it = desc_list.begin();
             desc_it != desc_list.end(); ++desc_it) { 
            eh.RemoveSeqdesc(**desc_it);
            ChangeMade(CCleanupChange::eRemoveDescriptor);
        }        
    }                        
}


void CCleanup_imp::x_RemoveDescrByType(CBioseq_set_Handle bh, CSeqdesc::E_Choice desctype, bool recurse)
{
    CBioseq_set_EditHandle eh = bh.GetEditHandle();
    if (eh.IsSetDescr()) {
        CSeq_descr::Tdata desc_list;
        desc_list.clear();
        NON_CONST_ITERATE (CSeq_descr::Tdata, desc_it, eh.SetDescr().Set()) {
            if ((*desc_it)->Which() == desctype ) {
                desc_list.push_back(*desc_it);
            }
        }
        for (CSeq_descr::Tdata::iterator desc_it = desc_list.begin();
             desc_it != desc_list.end(); ++desc_it) { 
            eh.RemoveSeqdesc(**desc_it);
            ChangeMade(CCleanupChange::eRemoveDescriptor);
        }        
    }
    if (recurse) {
        x_RemoveDescrForAllInSet (bh, desctype);
    }
}


void CCleanup_imp::x_RemoveDescrByType(const CSeq_entry& se, CSeqdesc::E_Choice desctype, bool recurse)
{
    if (se.Which() == CSeq_entry::e_Seq) {
        x_RemoveDescrByType(m_Scope->GetBioseqHandle(se.GetSeq()), desctype);
    } else if (se.Which() == CSeq_entry::e_Set) {
        x_RemoveDescrByType(m_Scope->GetBioseq_setHandle(se.GetSet()), desctype, recurse);
    }
}


void CCleanup_imp::x_RemoveDescrForAllInSet (CBioseq_set_Handle bss, CSeqdesc::E_Choice desctype)
{
    CConstRef<CBioseq_set> b = bss.GetCompleteBioseq_set();
    list< CRef< CSeq_entry > > set = (*b).GetSeq_set();

    ITERATE (list< CRef< CSeq_entry > >, it, set) {
        x_RemoveDescrByType((**it), desctype);
    }
}


void CCleanup_imp::x_MoveIdenticalPartDescriptorsToSegSet (CBioseq_set_Handle segset, CBioseq_set_Handle parts, CSeqdesc::E_Choice desctype)
{
    CSeq_descr::Tdata desc_list;
    
    desc_list.clear();
	if (x_IsDescrSameForAllInPartsSet (parts, desctype, desc_list)) {
	    CSeqdesc_CI desc_it (segset.GetParentEntry(), desctype);
	    bool remove_from_parts = false;
        if (!desc_it) {
            // segset does not already have a descriptor of this type
            CBioseq_set_EditHandle eh = segset.GetEditHandle();
            eh.AddSeqdesc(**(desc_list.begin()));
            ChangeMade(CCleanupChange::eAddDescriptor);
            remove_from_parts = true;          
        } else if (desctype == CSeqdesc::e_Update_date) {
            CBioseq_set_EditHandle eh = segset.GetEditHandle();
            eh.RemoveSeqdesc (*desc_it);
            eh.AddSeqdesc(**(desc_list.begin())); 
            ChangeMade(CCleanupChange::eAddDescriptor);
            remove_from_parts = true;
        } else if (x_SeqDescMatch(*desc_it, **(desc_list.begin()))) {
	        remove_from_parts = true;
	    }
	    if (remove_from_parts) {
	        x_RemoveDescrForAllInSet (parts, desctype);
	    }
	}
}


void CCleanup_imp::x_RemoveNucProtSetTitle(CBioseq_set_EditHandle bsh, const CSeq_entry& se)
{
    if (!bsh.IsSetDescr()) {
        // ditch if nuc-prot-set has no descriptors at all
        return;
    }
    
    CSeqdesc_CI nuc_desc_it (m_Scope->GetSeq_entryHandle(se), CSeqdesc::e_Title, 1);
    if (!nuc_desc_it) {
        // ditch if nuc sequence has no descriptors at all
        return;
    }

    // look for old style nps title, remove if based on nucleotide title, also remove exact duplicate
    NON_CONST_ITERATE (CSeq_descr::Tdata, desc_it, bsh.SetDescr().Set()) {
        if ((*desc_it)->Which() == CSeqdesc::e_Title) {
            string npstitle = (*desc_it)->GetTitle();
            string nuctitle = nuc_desc_it->GetTitle();
            size_t title_len = nuctitle.length();
            if (NStr::StartsWith(npstitle, nuctitle)
                && (npstitle.length() == title_len
                    || NStr::EqualCase (npstitle.substr(title_len), ", and translated products"))) {
                bsh.RemoveSeqdesc(**desc_it);
                ChangeMade(CCleanupChange::eRemoveDescriptor);
                break;
            }
        }
    }
}

void CCleanup_imp::x_ExtractNucProtDescriptors(CBioseq_set_EditHandle bsh, const CSeq_entry& se, CSeqdesc::E_Choice desctype)
{
    CSeqdesc_CI it (bsh.GetParentEntry(), desctype);
  
    // if nuc-prot set already has one, don't move the lower one up
    if (it) return;
    
    CSeq_descr::Tdata desc_list;
    if (se.Which() == CSeq_entry::e_Seq) {
        CBioseq_EditHandle eh = (m_Scope->GetBioseqHandle(se.GetSeq())).GetEditHandle();
        if (eh.IsSetDescr()) {
            NON_CONST_ITERATE (CSeq_descr::Tdata, desc_it, eh.SetDescr().Set()) {
                if ((*desc_it)->Which() == desctype) {
                    desc_list.push_back(*desc_it);
                }
            }
            for (CSeq_descr::Tdata::iterator desc_it = desc_list.begin();
                 desc_it != desc_list.end(); ++desc_it) { 
                bsh.AddSeqdesc(**desc_it);
                ChangeMade(CCleanupChange::eAddDescriptor);
                eh.RemoveSeqdesc(**desc_it);
                ChangeMade(CCleanupChange::eRemoveDescriptor);
            }                        
        }
    } else if (se.Which() == CSeq_entry::e_Set) {
        CBioseq_set_EditHandle eh = (m_Scope->GetBioseq_setHandle(se.GetSet())).GetEditHandle();
        if (eh.IsSetDescr()) {
            NON_CONST_ITERATE (CSeq_descr::Tdata, desc_it, eh.SetDescr().Set()) {
                if ((*desc_it)->Which() == desctype) {
                    desc_list.push_back(*desc_it);
                }
            }
            for (CSeq_descr::Tdata::iterator desc_it = desc_list.begin();
                 desc_it != desc_list.end(); ++desc_it) { 
                bsh.AddSeqdesc(**desc_it);
                ChangeMade(CCleanupChange::eAddDescriptor);
                eh.RemoveSeqdesc(**desc_it);
                ChangeMade(CCleanupChange::eRemoveDescriptor);
            }                        
        }
    }   
}


// was FixProtMolInfo in C Toolkit
void CCleanup_imp::x_AddMissingProteinMolInfo(CSeq_entry_Handle seh)
{
    CBioseq_CI bs_ci(seh, CSeq_inst::eMol_aa);
    
    while (bs_ci) {
        CBioseq_EditHandle eh = (*bs_ci).GetEditHandle();
        bool found = false;
        if (eh.IsSetDescr()) {
            NON_CONST_ITERATE (CSeq_descr::Tdata, desc_it, eh.SetDescr().Set()) {
                if ((*desc_it)->Which() == CSeqdesc::e_Molinfo) {
                    if (!(*desc_it)->GetMolinfo().CanGetBiomol()
                        || (*desc_it)->GetMolinfo().GetBiomol() == CMolInfo::eBiomol_unknown) {
                        (*desc_it)->SetMolinfo().SetBiomol(CMolInfo::eBiomol_peptide);
                        ChangeMade(CCleanupChange::eChangeMolInfo);
                    }
                    found = true;
                }
            }
        }
        if (!found) {
            CRef<CSeqdesc> desc(new CSeqdesc);
            CRef<CMolInfo> mol(new CMolInfo);
            mol->SetBiomol(CMolInfo::eBiomol_peptide);
            desc->SetMolinfo(*mol);
            eh.AddSeqdesc(*desc);
            ChangeMade(CCleanupChange::eChangeMolInfo);
        }
        ++bs_ci;
    }
}


// was FuseMolInfos in C Toolkit
// If a Bioseq or BioseqSet has more than one MolInfo descriptor, the biomol, tech, completeness, and techexp values will be combined
// into the first MolInfo descriptor and the rest of the MolInfo descriptors will be removed.
void CCleanup_imp::x_FuseMolInfos (CSeq_descr& desc_set, CSeq_descr::Tdata& desc_list)
{
    NON_CONST_ITERATE (CSeq_descr::Tdata, desc_it, desc_set.Set()) {
        if ((*desc_it)->Which() == CSeqdesc::e_Molinfo) {
            if (desc_list.size() > 0) {
                // biomol
                if ((!desc_list.front()->GetMolinfo().CanGetBiomol()
                    || desc_list.front()->GetMolinfo().GetBiomol() == CMolInfo::eBiomol_unknown)
                    && (*desc_it)->GetMolinfo().CanGetBiomol()
                    && (*desc_it)->GetMolinfo().GetBiomol() != CMolInfo::eBiomol_unknown) {
                    desc_list.front()->SetMolinfo().SetBiomol((*desc_it)->GetMolinfo().GetBiomol());
                    ChangeMade(CCleanupChange::eChangeMolInfo);
                }
                
                //completeness
                if ((!desc_list.front()->GetMolinfo().CanGetCompleteness()
                    || desc_list.front()->GetMolinfo().GetCompleteness() == CMolInfo::eCompleteness_unknown)
                    && (*desc_it)->GetMolinfo().CanGetCompleteness()
                    && (*desc_it)->GetMolinfo().GetCompleteness() != CMolInfo::eCompleteness_unknown) {
                    desc_list.front()->SetMolinfo().SetCompleteness((*desc_it)->GetMolinfo().GetCompleteness());
                    ChangeMade(CCleanupChange::eChangeMolInfo);
                }

                //tech
                if ((!desc_list.front()->GetMolinfo().CanGetTech()
                    || desc_list.front()->GetMolinfo().GetTech() == CMolInfo::eTech_unknown)
                    && (*desc_it)->GetMolinfo().CanGetTech()
                    && (*desc_it)->GetMolinfo().GetTech() != CMolInfo::eTech_unknown) {
                    desc_list.front()->SetMolinfo().SetTech((*desc_it)->GetMolinfo().GetTech());
                    ChangeMade(CCleanupChange::eChangeMolInfo);
                }

                //techexp
                if ((!desc_list.front()->GetMolinfo().CanGetTechexp()
                    || NStr::IsBlank(desc_list.front()->GetMolinfo().GetTechexp()))
                    && (*desc_it)->GetMolinfo().CanGetTechexp()
                    && !NStr::IsBlank((*desc_it)->GetMolinfo().GetTechexp())) {
                    desc_list.front()->SetMolinfo().SetTechexp((*desc_it)->GetMolinfo().GetTechexp());
                    ChangeMade(CCleanupChange::eChangeMolInfo);
                }
                
            }
            
            desc_list.push_back (*desc_it);
        }
    }
}


void CCleanup_imp::x_FuseMolInfos (CBioseq_Handle bh)
{
    if (!bh.IsSetDescr()) {
        return;
    }

    CBioseq_EditHandle eh = bh.GetEditHandle();
    CSeq_descr::Tdata desc_list;
    
    x_FuseMolInfos(eh.SetDescr(), desc_list);

    if (desc_list.size() > 1) {
        // keep the first MolInfo descriptor
        desc_list.pop_front();
        for (CSeq_descr::Tdata::iterator it1 = desc_list.begin();
            it1 != desc_list.end(); ++it1) { 
            eh.RemoveSeqdesc(**it1);
            ChangeMade(CCleanupChange::eChangeMolInfo);
        }
    }        
}
    

void CCleanup_imp::x_FuseMolInfos (CBioseq_set_Handle bh)
{
    if (!bh.IsSetDescr()) {
        return;
    }

    CBioseq_set_EditHandle eh = bh.GetEditHandle();
    CSeq_descr::Tdata desc_list;
    
    x_FuseMolInfos(eh.SetDescr(), desc_list);

    // keep the first MolInfo descriptor
    if (desc_list.size() > 1) {
        // keep the first MolInfo descriptor
        desc_list.pop_front();
        for (CSeq_descr::Tdata::iterator it1 = desc_list.begin();
            it1 != desc_list.end(); ++it1) { 
            eh.RemoveSeqdesc(**it1);
            ChangeMade(CCleanupChange::eChangeMolInfo);
        }
    }        
    
    ITERATE (list< CRef< CSeq_entry > >, it, bh.GetCompleteBioseq_set()->GetSeq_set()) {
        x_FuseMolInfos(m_Scope->GetSeq_entryHandle(**it));
    }           
}


void CCleanup_imp::x_FuseMolInfos (CSeq_entry_Handle seh)
{
    if (seh.IsSeq()) {
        x_FuseMolInfos(seh.GetSeq());
    } else if (seh.IsSet()) {
        x_FuseMolInfos(seh.GetSet());
    }
}

// This function was LoopSeqEntryToAsn3 in the C Toolkit
void CCleanup_imp::LoopToAsn3(CBioseq_set_Handle bh)
{
    
    // these steps were called RemoveEmptyTitleAndPubGenAsOnlyPub
    x_RecurseForDescriptors(bh, &ncbi::objects::CCleanup_imp::x_RemoveEmptyTitles);
        
    CheckNucProtSet(bh);

    // these two steps were EntryChangeImpFeat in the C Toolkit
    x_RecurseForSeqAnnots(bh, &ncbi::objects::CCleanup_imp::x_ChangeImpFeatToCDS);
    x_RecurseForSeqAnnots(bh, &ncbi::objects::CCleanup_imp::x_ChangeImpFeatToProt);
 
    // this step was StripProtXrefs in the C Toolkit
    x_RecurseForSeqAnnots (bh, &ncbi::objects::CCleanup_imp::x_StripProtXrefs);
    // this step was called by CheckMaps in the C Toolkit,
    // but CheckMaps didn't actually make any other changes
    x_RecurseForSeqAnnots (bh, &ncbi::objects::CCleanup_imp::x_ConvertUserObjectToAnticodon);
    // this step was called MapsToGenref in the C Toolkit
    x_RecurseForSeqAnnots (bh, &ncbi::objects::CCleanup_imp::x_MoveMapQualsToGeneMaploc);
    
    // these steps were part of CheckGeneticCode in the C Toolkit
    x_RecurseForSeqAnnots (bh, &ncbi::objects::CCleanup_imp::x_FixProteinIDs);
    x_RecurseForSeqAnnots (bh, &ncbi::objects::CCleanup_imp::x_CheckConflictFlag);
    // final portion of CheckGeneticCode
    x_RecurseForSeqAnnots (bh, &ncbi::objects::CCleanup_imp::x_RemoveAsn2ffGeneratedComments);
    
}


// This function was LoopSeqEntryToAsn3 in the C Toolkit
void CCleanup_imp::LoopToAsn3(CBioseq_Handle bh)
{
    // these steps were called RemoveEmptyTitleAndPubGenAsOnlyPub
    x_RecurseForDescriptors(bh, &ncbi::objects::CCleanup_imp::x_RemoveEmptyTitles);
    
    // these two steps were EntryChangeImpFeat in the C Toolkit
    x_RecurseForSeqAnnots(bh, &ncbi::objects::CCleanup_imp::x_ChangeImpFeatToCDS);
    x_RecurseForSeqAnnots(bh, &ncbi::objects::CCleanup_imp::x_ChangeImpFeatToProt);
 
    // The FixNucProtSet step from the C Toolkit is eliminated here, as it only applies
    // to Nuc-Prot sets and sets that might contain nuc-prot sets.
    
    // this step was StripProtXrefs in the C Toolkit
    x_RecurseForSeqAnnots (bh, &ncbi::objects::CCleanup_imp::x_StripProtXrefs);
    // this step was called by CheckMaps in the C Toolkit,
    // but CheckMaps didn't actually make any other changes
    x_RecurseForSeqAnnots (bh, &ncbi::objects::CCleanup_imp::x_ConvertUserObjectToAnticodon);
    // this step was called MapsToGenref in the C Toolkit
    x_RecurseForSeqAnnots (bh, &ncbi::objects::CCleanup_imp::x_MoveMapQualsToGeneMaploc);

    // these steps were part of CheckGeneticCode in the C Toolkit
    x_RecurseForSeqAnnots (bh, &ncbi::objects::CCleanup_imp::x_FixProteinIDs);
    x_RecurseForSeqAnnots (bh, &ncbi::objects::CCleanup_imp::x_CheckConflictFlag);
    // final portion of CheckGeneticCode
    x_RecurseForSeqAnnots (bh, &ncbi::objects::CCleanup_imp::x_RemoveAsn2ffGeneratedComments);

}


void CCleanup_imp::LoopToAsn3 (CSeq_entry_Handle seh)
{ 
    if (seh.Which() == CSeq_entry::e_Seq) {
        LoopToAsn3 (seh.GetSeq());
    } else if (seh.Which() == CSeq_entry::e_Set) {
        LoopToAsn3 (seh.GetSet());
    }
}


// this function should check segsets to see if all mol-info descriptors on sequences 
// and sets below this level are consistent, put the mol-info on this level, and remove 
// all mol-info descriptors below this level.
// For all other sets, check the members of the set.
// was NormalizeSegSeqMolInfo in C Toolkit
void CCleanup_imp::x_NormalizeMolInfo(CBioseq_set_Handle bh)
{
    if (!bh.CanGetClass() || bh.GetClass() != CBioseq_set::eClass_segset) {
        ITERATE (list< CRef< CSeq_entry > >, it, bh.GetCompleteBioseq_set()->GetSeq_set()) {
            if ((*it)->Which() == CSeq_entry::e_Set) {
                x_NormalizeMolInfo(m_Scope->GetBioseq_setHandle((*it)->GetSet()));
            }
        }        
    } else {
        bool is_consistent = true;
        int num_found = 0;
        CRef<CMolInfo> mol(new CMolInfo);
        CSeqdesc_CI desc_it(bh.GetParentEntry(), CSeqdesc::e_Molinfo);
        while (desc_it && is_consistent) {
            if (num_found == 0) {
                //biomol
                if (desc_it->GetMolinfo().CanGetBiomol()) {
                    mol->SetBiomol(desc_it->GetMolinfo().GetBiomol());
                }
                //completeness
                if (desc_it->GetMolinfo().CanGetCompleteness()) {
                    mol->SetCompleteness(desc_it->GetMolinfo().GetCompleteness());
                }
                //tech
                if (desc_it->GetMolinfo().CanGetTech()) {
                    mol->SetTech(desc_it->GetMolinfo().GetTech());
                }
                //techexp
                if (desc_it->GetMolinfo().CanGetTechexp()) {
                    mol->SetTechexp(desc_it->GetMolinfo().GetTechexp());
                }
            } else {
                //biomol
                if (mol->CanGetBiomol()) {
                    if (!desc_it->GetMolinfo().CanGetBiomol()
                        || mol->GetBiomol() != desc_it->GetMolinfo().GetBiomol()) {
                        is_consistent = false;
                    }
                } else if (desc_it->GetMolinfo().CanGetBiomol()) {
                    is_consistent = false;
                }
                //completeness
                if (mol->CanGetCompleteness()) {
                    if (!desc_it->GetMolinfo().CanGetCompleteness()
                        || mol->GetCompleteness() != desc_it->GetMolinfo().GetCompleteness()) {
                        is_consistent = false;
                    }
                } else if (desc_it->GetMolinfo().CanGetCompleteness()) {
                    is_consistent = false;
                }
                //tech
                if (mol->CanGetTech()) {
                    if (!desc_it->GetMolinfo().CanGetTech()
                        || mol->GetTech() != desc_it->GetMolinfo().GetTech()) {
                        is_consistent = false;
                    }
                } else if (desc_it->GetMolinfo().CanGetTech()) {
                    is_consistent = false;
                }
                 //techexp
                if (mol->CanGetTechexp()) {
                    if (!desc_it->GetMolinfo().CanGetTechexp()
                        || !NStr::Equal(mol->GetTechexp(), desc_it->GetMolinfo().GetTechexp())) {
                        is_consistent = false;
                    }
                } else if (desc_it->GetMolinfo().CanGetTechexp()) {
                    is_consistent = false;
                }
            }
            ++desc_it;
            num_found++;
        }
           
        if (is_consistent && num_found > 0) {
            bool need_to_move = false;
            // is one molinfo already on this set?
            CSeqdesc_CI this_set_desc(bh.GetParentEntry(), CSeqdesc::e_Molinfo, 1);
            if (!this_set_desc) {
                need_to_move = true;
            }
            if (num_found > 1 || need_to_move) {
                ITERATE (list< CRef< CSeq_entry > >, it, bh.GetCompleteBioseq_set()->GetSeq_set()) {
                    x_RemoveDescrByType((**it), CSeqdesc::e_Molinfo);
                }
            }
            if (need_to_move) {
                CBioseq_set_EditHandle eh = bh.GetEditHandle();
                CRef<CSeqdesc> desc(new CSeqdesc);
                desc->SetMolinfo(*mol);
                eh.AddSeqdesc(*desc);
            }
        }
    }
}


// This step was part of CheckGCode in the C Toolkit
// It should be called after x_CheckConflictFlag has been called for the relevant
// features - x_CheckConflictFlag may clear conflict flags if the translation matches
void CCleanup_imp::x_SetMolInfoTechForConflictCDS (CBioseq_Handle bh)
{
    bool need_tech = false;
    SAnnotSelector sel(CSeqFeatData::e_Cdregion);
    CFeat_CI feat_ci (bh, sel);    
    while (feat_ci && !need_tech) {
        if (feat_ci->GetData().GetCdregion().CanGetConflict()
            && feat_ci->GetData().GetCdregion().GetConflict()
            && feat_ci->IsSetProduct()) {
            need_tech = true;
        }
        ++feat_ci;
    }
    
    if (need_tech) {
        CBioseq_EditHandle eh = bh.GetEditHandle();
        bool need_new_desc = true;        
        if (eh.IsSetDescr()) {
            NON_CONST_ITERATE (CSeq_descr::Tdata, desc_it, eh.SetDescr().Set()) {
                if ((*desc_it)->Which() == CSeqdesc::e_Molinfo) {
                    (*desc_it)->SetMolinfo().SetTech(CMolInfo::eTech_concept_trans_a);
                    ChangeMade (CCleanupChange::eChangeMolInfo);
                    need_new_desc = false;
                    break;
                }
            }
        }
        if (need_new_desc) {
            CRef<CSeqdesc> new_desc(new CSeqdesc);
            new_desc->SetMolinfo().SetTech(CMolInfo::eTech_concept_trans_a);
            eh.AddSeqdesc(*new_desc);
            ChangeMade (CCleanupChange::eChangeMolInfo);
        }
    }    
}


void CCleanup_imp::x_ExtendedCleanupMolInfoDescriptors(CBioseq_Handle bh)
{
    // this step was FuseMolInfos in the C Toolkit
    x_FuseMolInfos (bh);

    // this step was FixProtMolInfo in the C Toolkit
    x_AddMissingProteinMolInfo(bh.GetParentEntry());

    // The NormalizeSegSeqMolInfo from the C Toolkit is eliminated here, as it only
    // applies to MolInfo descriptors that need to be moved from sequences to sets

    // NOTE - this is still part of CheckGeneticCode in the C Toolkit.
    // This step must be performed after the x_CheckConflictFlag step
    x_SetMolInfoTechForConflictCDS (bh);
}


void CCleanup_imp::x_ExtendedCleanupMolInfoDescriptors(CBioseq_set_Handle bh)
{
    // this step was FuseMolInfos in the C Toolkit
    x_FuseMolInfos (bh);

    // this step was FixProtMolInfo in the C Toolkit
    x_AddMissingProteinMolInfo(bh.GetParentEntry());

    // this step was NormalizeSegSeqMolInfo in the C Toolkit
    x_NormalizeMolInfo (bh);

    // NOTE - this is still part of CheckGeneticCode in the C Toolkit.
    // This step must be performed after the x_CheckConflictFlag step
    CBioseq_CI bioseq_ci(bh);
    while (bioseq_ci) {
        x_SetMolInfoTechForConflictCDS (*bioseq_ci);
        ++bioseq_ci;
    }

}



END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE
