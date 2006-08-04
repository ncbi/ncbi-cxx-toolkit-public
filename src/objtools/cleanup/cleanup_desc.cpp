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

#include "cleanupp.hpp"


BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::


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
        }        
    }
}


void CCleanup_imp::x_RecurseForDescriptors (CBioseq_set_Handle bss, RecurseDescriptor pmf)
{
    if (bss.IsSetDescr()) {
        CSeq_descr::Tdata remove_list;

        remove_list.clear();
        CBioseq_set_EditHandle eh = bss.GetEditHandle();
        (this->*pmf)(eh.SetDescr(), remove_list);

        for (CSeq_descr::Tdata::iterator it1 = remove_list.begin();
            it1 != remove_list.end(); ++it1) { 
            eh.RemoveSeqdesc(**it1);
        }        
    }

    if (bss.GetCompleteBioseq_set()->IsSetSeq_set()) {
       CBioseq_set_EditHandle eh = bss.GetEditHandle();
       CConstRef<CBioseq_set> b = bss.GetCompleteBioseq_set();
       list< CRef< CSeq_entry > > set = (*b).GetSeq_set();
       
       ITERATE (list< CRef< CSeq_entry > >, it, set) {
            switch ((**it).Which()) {
                case CSeq_entry::e_Seq:
                    x_RecurseForDescriptors(m_Scope->GetBioseqHandle((**it).GetSeq()), pmf);
                    break;
                case CSeq_entry::e_Set:
                    x_RecurseForDescriptors(m_Scope->GetBioseq_setHandle((**it).GetSet()), pmf);
                    break;
                case CSeq_entry::e_not_set:
                default:
                    break;
            }
        }
    }
}


void CCleanup_imp::x_RecurseDescriptorsForMerge(CBioseq_Handle bh, IsMergeCandidate is_can, Merge do_merge)
{
    CSeq_descr::Tdata remove_list;    

    CBioseq_EditHandle edith = m_Scope->GetEditHandle(bh);
     
    remove_list.clear();

    for (CSeq_descr::Tdata::iterator it1 = edith.SetDescr().Set().begin();
        it1 != edith.SetDescr().Set().end(); ++it1) { 
        CRef< CSeqdesc > sd = *it1;
        if ((this->*is_can)(*sd)) {
            bool found_match = false;
            CSeq_descr::Tdata::iterator it2 = it1;
            it2++;
            while (it2 != edith.SetDescr().Set().end() && !found_match) {
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
    
    for (CSeq_descr::Tdata::iterator it1 = remove_list.begin();
        it1 != remove_list.end(); ++it1) { 
        edith.RemoveSeqdesc(**it1);
    }
}


void CCleanup_imp::x_RecurseDescriptorsForMerge(CBioseq_set_Handle bh, IsMergeCandidate is_can, Merge do_merge)
{
    CSeq_descr::Tdata remove_list;    

    CBioseq_set_EditHandle edith = m_Scope->GetEditHandle(bh);
     
    remove_list.clear();

    for (CSeq_descr::Tdata::iterator it1 = edith.SetDescr().Set().begin();
        it1 != edith.SetDescr().Set().end(); ++it1) { 
        CRef< CSeqdesc > sd = *it1;
        if ((this->*is_can)(*sd)) {
            bool found_match = false;
            CSeq_descr::Tdata::iterator it2 = it1;
            it2++;
            while (it2 != edith.SetDescr().Set().end() && !found_match) {
                if ((this->*is_can)(**it2)) {
                    if (do_merge == NULL || (this->*do_merge)(**it2, *sd)) {
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
    
    for (CSeq_descr::Tdata::iterator it1 = remove_list.begin();
        it1 != remove_list.end(); ++it1) { 
        edith.RemoveSeqdesc(**it1);
    }
    
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


//   For any Bioseq or BioseqSet that has a MolInfo descriptor,
//1) If the Bioseq or BioseqSet also has a MolType descriptor, if the MolInfo biomol value is not set 
//   and the value from the MolType descriptor is MOLECULE_TYPE_GENOMIC, MOLECULE_TYPE_PRE_MRNA, 
//   MOLECULE_TYPE_MRNA, MOLECULE_TYPE_RRNA, MOLECULE_TYPE_TRNA, MOLECULE_TYPE_SNRNA, MOLECULE_TYPE_SCRNA, 
//   MOLECULE_TYPE_PEPTIDE, MOLECULE_TYPE_OTHER_GENETIC_MATERIAL, MOLECULE_TYPE_GENOMIC_MRNA_MIX, or 255, 
//   the value from the MolType descriptor will be used to set the MolInfo biomol value.  The MolType descriptor
//   will be removed, whether its value was copied to the MolInfo descriptor or not.
//2) If the Bioseq or BioseqSet also has a Method descriptor, if the MolInfo technique value has not been set and 
//   the Method descriptor value is “concept_trans”, “seq_pept”, “both”, “seq_pept_overlap”,  “seq_pept_homol”, 
//   “concept_transl_a”, or “other”, then the Method descriptor value will be used to set the MolInfo technique value.  
//   The Method descriptor will be removed, whether its value was copied to the MolInfo descriptor or not.
void CCleanup_imp::x_MolInfoUpdate(CSeq_descr& sdr, CSeq_descr::Tdata& remove_list)
{
    bool has_molinfo = false;
    CMolInfo::TBiomol biomol = CMolInfo::eBiomol_unknown;
    CMolInfo::TTech   tech = CMolInfo::eTech_unknown;
    bool changed = false;
    
    NON_CONST_ITERATE (CSeq_descr::Tdata, it, sdr.Set()) {
        if ((*it)->Which() == CSeqdesc::e_Molinfo) {
            if (!has_molinfo) {
                has_molinfo = true;
                const CMolInfo& mol_info = (*it)->GetMolinfo();
                if (mol_info.CanGetBiomol()) {
                    biomol = mol_info.GetBiomol();
                }
                if (mol_info.CanGetTech()) {
                    tech = mol_info.GetTech();
                }
            }          
        } else if ((*it)->Which() == CSeqdesc::e_Mol_type) {
            if (biomol == CMolInfo::eBiomol_unknown) {
                CSeqdesc::TMol_type mol_type = (*it)->GetMol_type();
                switch (mol_type) {
                    case eGIBB_mol_unknown:
                        break;
                    case eGIBB_mol_genomic:
                        biomol = CMolInfo::eBiomol_genomic;
                        break;
                    case eGIBB_mol_pre_mRNA:
                        biomol = CMolInfo::eBiomol_pre_RNA;
                        break;
                    case eGIBB_mol_mRNA:
                        biomol = CMolInfo::eBiomol_mRNA;
                        break;
                    case eGIBB_mol_rRNA:
                        biomol = CMolInfo::eBiomol_rRNA;
                        break;
                    case eGIBB_mol_tRNA:
                        biomol = CMolInfo::eBiomol_tRNA;
                        break;
                    case eGIBB_mol_snRNA:
                        biomol = CMolInfo::eBiomol_snRNA;
                        break;
                    case eGIBB_mol_scRNA:
                        biomol = CMolInfo::eBiomol_scRNA;
                        break;
                    case eGIBB_mol_peptide:
                        biomol = CMolInfo::eBiomol_peptide;
                        break;
                    case eGIBB_mol_other_genetic:
                        biomol = CMolInfo::eBiomol_other_genetic;
                        break;
                    case eGIBB_mol_genomic_mRNA:
                        biomol = CMolInfo::eBiomol_genomic_mRNA;
                        break;
                    case eGIBB_mol_other:
                        biomol = CMolInfo::eBiomol_other;
                        break;
                }
            }
        } else if ((*it)->Which() == CSeqdesc::e_Method) {
            if (tech == CMolInfo::eTech_unknown) {
                CSeqdesc::TMethod method = (*it)->GetMethod();
                switch (method) {
                    case eGIBB_method_concept_trans:
                        tech = CMolInfo::eTech_concept_trans;
                        break;
                    case eGIBB_method_seq_pept:
                        tech = CMolInfo::eTech_seq_pept;
                        break;
                    case eGIBB_method_both:
                        tech = CMolInfo::eTech_both;
                        break;
                    case eGIBB_method_seq_pept_overlap:
                        tech = CMolInfo::eTech_seq_pept_overlap;
                        break;
                    case eGIBB_method_seq_pept_homol:
                        tech = CMolInfo::eTech_seq_pept_homol;
                        break;
                    case eGIBB_method_concept_trans_a:
                        tech = CMolInfo::eTech_concept_trans_a;
                        break;
                    case eGIBB_method_other:
                        tech = CMolInfo::eTech_other;
                        break;
                }
            }
        }
            
    }
    if (!has_molinfo) {
        return;
    }    
    
    NON_CONST_ITERATE (CSeq_descr::Tdata, it, sdr.Set()) {
        if ((*it)->Which() == CSeqdesc::e_Molinfo) {
            CSeqdesc::TMolinfo& mi = (*it)->SetMolinfo();
            if (biomol != CMolInfo::eBiomol_unknown) {
                if (!mi.CanGetBiomol() || mi.GetBiomol() != biomol) {
                    changed = true;
                    mi.SetBiomol (biomol);
                }
            }
            if (tech != CMolInfo::eTech_unknown) {
                if (!mi.CanGetTech() || mi.GetTech() != tech) {
                    changed = true;
                    mi.SetTech (tech);
                }
            }
        } else if ((*it)->Which() == CSeqdesc::e_Method
                   || (*it)->Which() == CSeqdesc::e_Mol_type) {
            remove_list.push_back(*it);
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
    // clean extra accessions
    if (block.CanGetExtra_accessions()) {
        CleanVisStringList(block.SetExtra_accessions());
        if (block.GetExtra_accessions().size() == 0) {
            block.ResetExtra_accessions();
        }
    }
                
    // clean keywords
    if (block.CanGetKeywords()) {
        CleanVisStringList(block.SetKeywords());
        if (block.GetKeywords().size() == 0) {
            block.ResetKeywords();
        }
    }
                
    // clean source
    if (block.CanGetSource()) {
        string source = block.GetSource();
        CleanVisString (source);
        if (NStr::IsBlank (source)) {
            block.ResetSource();
        } else {
            block.SetSource (source);
        }
    }
    // clean origin
    if (block.CanGetOrigin()) {
        string origin = block.GetOrigin();
        CleanVisString (origin);
        if (NStr::IsBlank (origin)) {
            block.ResetOrigin();
        } else {
            block.SetOrigin(origin);
        }
    }
    //clean date
    if (block.CanGetDate()) {
        string date = block.GetDate();
        CleanVisString (date);
        if (NStr::IsBlank (date)) {
            block.ResetDate();
        } else {
            block.SetDate(date);
        }
    }
    //clean div
    if (block.CanGetDiv()) {
        string div = block.GetDiv();
        CleanVisString (div);
        if (NStr::IsBlank (div)) {
            block.ResetDiv();
        } else {
            block.SetDiv(div);
        }
    }
    //clean taxonomy
    if (block.CanGetTaxonomy()) {
        string tax = block.GetTaxonomy();
        if (NStr::IsBlank (tax)) {
            block.ResetTaxonomy();
        } else {
            block.SetTaxonomy(tax);
        }
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
    if (!gb_block.CanGetKeywords()) {
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
                    }
                } else if ((tech == CMolInfo::eTech_est && NStr::Equal(gb_div, "EST"))
                           || (tech == CMolInfo::eTech_sts && NStr::Equal(gb_div, "STS"))
                           || (tech == CMolInfo::eTech_survey && NStr::Equal(gb_div, "GSS"))
                           || ((tech == CMolInfo::eTech_htgs_1 
                                || tech == CMolInfo::eTech_htgs_2
                                || tech == CMolInfo::eTech_htgs_3) && NStr::Equal(gb_div, "HTG"))) {
                    gb_block.ResetDiv();
                } else if (tech == CMolInfo::eTech_unknown && NStr::Equal(gb_div, "STS")) {
                    CMolInfo& m = const_cast<CMolInfo&> ((*molinfo).GetMolinfo());
                    m.SetTech(CMolInfo::eTech_sts);
                    gb_block.ResetDiv();
                }
			}
			if (!NStr::IsBlank(div)) {
			    if (NStr::Equal(div, gb_div)) {
			        gb_block.ResetDiv();
			        gb_block.ResetTaxonomy();
			    } else if (NStr::Equal(gb_div, "UNA")
			               || NStr::Equal(gb_div, "UNC")) {
			        gb_block.ResetDiv();
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
    
    if (desc_i && (*desc_i).GetSource().CanGetOrg()) {
        string src;
        
        const COrg_ref& org = (*desc_i).GetSource().GetOrg();
        
        if (org.CanGetCommon()
            && !NStr::IsBlank(org.GetCommon())) {
            src = org.GetCommon();
        } else if (org.CanGetTaxname()) {
            src = org.GetTaxname();
        }
        if (org.CanGetMod()) {
            list<string>::const_iterator it = org.GetMod().begin();
            while (it != org.GetMod().end()) {
                src += " ";
                src += (*it);
                ++it;
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
                        gb_src = gb_src.substr(0, gb_src.length() - 2);
                    }
                    if (NStr::EqualNocase(src, gb_src)) {
                        CGB_block& gb_block = const_cast<CGB_block&> ((*gbdesc_i).GetGenbank());
                        gb_block.ResetSource();
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
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.5  2006/08/04 12:11:20  bollin
 * fixed crash bug in extended cleanup
 *
 * Revision 1.4  2006/08/03 12:03:23  bollin
 * moved descriptor extended cleanup methods here from cleanupp.cpp
 *
 * Revision 1.3  2006/07/26 18:58:27  ucko
 * x_ChangeGenBankBlocks: don't bother clear()ing brand new strings,
 * particularly given that GCC 2.95 would require erase() instead.
 *
 * Revision 1.2  2006/07/26 17:12:41  bollin
 * added method to remove redundant genbank block information
 *
 * Revision 1.1  2006/07/26 15:40:48  bollin
 * file to hold basic and extended cleanup functions for descriptors that are not
 * source descriptors
 *
 * Revision 1.10  2006/07/18 16:43:43  bollin
 * added x_RecurseDescriptorsForMerge and changed the ExtendedCleanup functions
 * for merging duplicate BioSources and equivalent CitSubs to use the new function
 *
 * Revision 1.9  2006/07/13 22:16:13  ucko
 * x_TrimParensAndCommas: call string::erase rather than string::clear
 * for compatibility with GCC 2.95.
 *
 * Revision 1.8  2006/07/13 19:31:15  ucko
 * Pass remove_if(), sort(), and unique() functions rather than predicate
 * objects, which are overkill and break on WorkShop.
 *
 * Revision 1.7  2006/07/13 17:12:12  rsmith
 * Bring up to date with C BSEC.
 *
 * Revision 1.6  2006/07/06 15:10:52  bollin
 * avoid setting empty values
 *
 * Revision 1.5  2006/07/05 17:26:11  bollin
 * cleared compiler error
 *
 * Revision 1.4  2006/07/05 16:43:34  bollin
 * added step to ExtendedCleanup to clean features and descriptors
 * and remove empty feature table seq-annots
 *
 * Revision 1.3  2006/06/28 13:22:39  bollin
 * added step to merge duplicate biosources to ExtendedCleanup
 *
 * Revision 1.2  2006/03/23 18:30:56  rsmith
 * cosmetic and comment changes.
 *
 * Revision 1.1  2006/03/20 14:21:25  rsmith
 * move Biosource related cleanup to its own file.
 *
 *
 *
 * ===========================================================================
 */
