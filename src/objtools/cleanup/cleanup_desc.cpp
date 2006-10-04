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
            x_RecurseForDescriptors (**it, pmf);
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
            CBioseq_EditHandle eh(m_Scope->GetBioseqHandle((*it)->GetSeq()));
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
            CBioseq_set_EditHandle eh(m_Scope->GetBioseq_setHandle((*it)->GetSet()));
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
    CBioseq_EditHandle eh(bh);
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
        }        
    }                        
}


void CCleanup_imp::x_RemoveDescrByType(CBioseq_set_Handle bh, CSeqdesc::E_Choice desctype)
{
    CBioseq_set_EditHandle eh(bh);
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
        }        
    }
    x_RemoveDescrForAllInSet (bh, desctype);                     
}


void CCleanup_imp::x_RemoveDescrByType(const CSeq_entry& se, CSeqdesc::E_Choice desctype)
{
    if (se.Which() == CSeq_entry::e_Seq) {
        x_RemoveDescrByType(m_Scope->GetBioseqHandle(se.GetSeq()), desctype);
    } else if (se.Which() == CSeq_entry::e_Set) {
        x_RemoveDescrByType(m_Scope->GetBioseq_setHandle(se.GetSet()), desctype);
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
            CBioseq_set_EditHandle eh(segset);
            eh.AddSeqdesc(**(desc_list.begin()));  
            remove_from_parts = true;          
        } else if (desctype == CSeqdesc::e_Update_date) {
            CBioseq_set_EditHandle eh(segset);
            eh.RemoveSeqdesc (*desc_it);
            eh.AddSeqdesc(**(desc_list.begin())); 
            remove_from_parts = true;
        } else if (desctype != CSeqdesc::e_Mol_type || x_SeqDescMatch(*desc_it, **(desc_list.begin()))) {
	        remove_from_parts = true;
	    }
	    if (remove_from_parts) {
	        x_RemoveDescrForAllInSet (parts, desctype);
	    }
	}
}


static bool s_IsExtractableEGIBBMod (const CSeqdesc::TModif& modif)
{
    if (*(modif.begin()) == eGIBB_mod_dna
        || *(modif.begin()) == eGIBB_mod_rna
        || *(modif.begin()) == eGIBB_mod_est
        || *(modif.begin()) == eGIBB_mod_complete
        || *(modif.begin()) == eGIBB_mod_partial) {
        return false;
    } else {
        return true;
    }
}


void CCleanup_imp::x_RemoveNucProtSetTitle(CBioseq_set_EditHandle bsh, const CSeq_entry& se)
{
    if (!bsh.IsSetDescr()) {
        // ditch if nuc-prot-set has no descriptors at all
        return;
    }
    CSeqdesc_CI nuc_desc_it (m_Scope->GetSeq_entryHandle(se), CSeqdesc::e_Title);
    if (!nuc_desc_it) {
        // ditch if nuc sequence has no descriptors at all
        return;
    }

    // look for old style nps title, remove if based on nucleotide title, also remove exact duplicate
    NON_CONST_ITERATE (CSeq_descr::Tdata, desc_it, bsh.SetDescr().Set()) {
        if ((*desc_it)->Which() == CSeqdesc::e_Title) {
            string npstitle = (*desc_it)->GetTitle();
            string nuctitle = nuc_desc_it->GetTitle();
            int title_len = nuctitle.length();
            if (NStr::StartsWith(npstitle, nuctitle)
                && (npstitle.length() == title_len
                    || NStr::EqualCase (npstitle.substr(title_len), ", and translated products"))) {
                bsh.RemoveSeqdesc(**desc_it);
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
        CBioseq_EditHandle eh(m_Scope->GetBioseqHandle(se.GetSeq()));
        if (eh.IsSetDescr()) {
            NON_CONST_ITERATE (CSeq_descr::Tdata, desc_it, eh.SetDescr().Set()) {
                if ((*desc_it)->Which() == desctype 
                    && (desctype != CSeqdesc::e_Modif 
                        || s_IsExtractableEGIBBMod((*desc_it)->GetModif()))) {
                    desc_list.push_back(*desc_it);
                }
            }
            for (CSeq_descr::Tdata::iterator desc_it = desc_list.begin();
                 desc_it != desc_list.end(); ++desc_it) { 
                bsh.AddSeqdesc(**desc_it);
                eh.RemoveSeqdesc(**desc_it);
            }                        
        }
    } else if (se.Which() == CSeq_entry::e_Set) {
        CBioseq_set_EditHandle eh(m_Scope->GetBioseq_setHandle(se.GetSet()));
        if (eh.IsSetDescr()) {
            NON_CONST_ITERATE (CSeq_descr::Tdata, desc_it, eh.SetDescr().Set()) {
                if ((*desc_it)->Which() == desctype 
                    && (desctype != CSeqdesc::e_Modif 
                        || s_IsExtractableEGIBBMod((*desc_it)->GetModif()))) {
                    desc_list.push_back(*desc_it);
                }
            }
            for (CSeq_descr::Tdata::iterator desc_it = desc_list.begin();
                 desc_it != desc_list.end(); ++desc_it) { 
                bsh.AddSeqdesc(**desc_it);
                eh.RemoveSeqdesc(**desc_it);
            }                        
        }
    }   
}


void CCleanup_imp::x_StripOldDescriptorsAndFeatures (CBioseq_set_Handle bh)
{
    // remove old descriptors
    x_RemoveDescrByType (bh, CSeqdesc::e_Modif);
    x_RemoveDescrByType (bh, CSeqdesc::e_Mol_type);
    x_RemoveDescrByType (bh, CSeqdesc::e_Method);
    x_RemoveDescrByType (bh, CSeqdesc::e_Org);
    
    // remove old features
    x_RemoveFeaturesBySubtype (bh, CSeqFeatData::eSubtype_org);
    x_RecurseForSeqAnnots (bh, &ncbi::objects::CCleanup_imp::x_RemoveImpSourceFeatures);
}


void CCleanup_imp::x_StripOldDescriptorsAndFeatures (CBioseq_Handle bh)
{
    // remove old descriptors
    x_RemoveDescrByType (bh, CSeqdesc::e_Modif);
    x_RemoveDescrByType (bh, CSeqdesc::e_Mol_type);
    x_RemoveDescrByType (bh, CSeqdesc::e_Method);
    x_RemoveDescrByType (bh, CSeqdesc::e_Org);
    
    // remove old features
    x_RemoveFeaturesBySubtype (bh, CSeqFeatData::eSubtype_org);
    x_RecurseForSeqAnnots (bh, &ncbi::objects::CCleanup_imp::x_RemoveImpSourceFeatures);
}


// was FixProtMolInfo in C Toolkit
void CCleanup_imp::x_AddMissingProteinMolInfo(CSeq_entry_Handle seh)
{
    CBioseq_CI bs_ci(seh, CSeq_inst::eMol_aa);
    
    while (bs_ci) {
        CBioseq_EditHandle eh(*bs_ci);
        bool found = false;
        if (eh.IsSetDescr()) {
            NON_CONST_ITERATE (CSeq_descr::Tdata, desc_it, eh.SetDescr().Set()) {
                if ((*desc_it)->Which() == CSeqdesc::e_Molinfo) {
                    if (!(*desc_it)->GetMolinfo().CanGetBiomol()
                        || (*desc_it)->GetMolinfo().GetBiomol() == CMolInfo::eBiomol_unknown) {
                        (*desc_it)->SetMolinfo().SetBiomol(CMolInfo::eBiomol_peptide);
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
        }
        ++bs_ci;
    }
}
    
        
void CCleanup_imp::LoopToAsn3(CBioseq_set_Handle bh)
{
    // these steps were called RemoveEmptyTitleAndPubGenAsOnlyPub
    x_RecurseForDescriptors(bh, &ncbi::objects::CCleanup_imp::x_RemoveEmptyTitles);
    x_RecurseForDescriptors(bh, &ncbi::objects::CCleanup_imp::x_RemoveCitGenPubDescriptors);
    x_RecurseForSeqAnnots(bh, &ncbi::objects::CCleanup_imp::x_RemoveCitGenPubFeatures);
    CheckSegSet(bh);
    CheckNucProtSet(bh);

    CSeqdesc_CI src_desc (bh.GetParentEntry(), CSeqdesc::e_Source);
    if (src_desc) {
        // this step was called StripOld
        x_StripOldDescriptorsAndFeatures (bh);
              
        // this step was part of ToAsn4
        x_ConvertPubsToAsn4(bh.GetParentEntry());
        
        // this step was also part of ToAsn4
        string lineage = "";
        x_GetGenBankTaxonomy(bh, lineage);
        if (!NStr::IsBlank(lineage)) {
            x_SetSourceLineage(bh, lineage);
        }
        
        // missing steps
#if 0
		CombineBSFeat(sep);
#endif            
    } else {
        // missing steps
#if 0
	if (ta.ofp == NULL) {
		ErrPostStr(SEV_WARNING, ERR_ORGANISM_NotFound, "No information found to create BioSource");
	}
	if (ta.mfp == NULL) {
		ErrPostStr(SEV_WARNING, ERR_ORGANISM_NotFound, "No information found to create MolInfo"); 
	}

	FixToAsn(sep, (Pointer)(&ta));

	if (ta.ofp != NULL) {
		ofp = ta.ofp;
		SeqEntryExplore(sep, (Pointer)ofp, FixOrg);
	}
	if (ta.mfp != NULL) {
		mfp = ta.mfp;
		SeqEntryExplore(sep, (Pointer)mfp, FixMol);
	}

/* entry  is converted to asn.1 spec 3.0, now do the checks */
	retval = INFO_ASNNEW;
	if(ta.had_biosource) {
		SeqEntryExplore(sep, NULL, StripOld);
	}
	ToAsn4(sep);          /* move pubs and lineage */
#endif    
    }
    
    // these two steps were EntryChangeImpFeat in the C Toolkit
    x_RecurseForSeqAnnots(bh, &ncbi::objects::CCleanup_imp::x_ChangeImpFeatToCDS);
    x_RecurseForSeqAnnots(bh, &ncbi::objects::CCleanup_imp::x_ChangeImpFeatToProt);
    // this step was EntryChangeGBSource in the C Toolkit
    x_ChangeGenBankBlocks(bh.GetParentEntry());
 
    //missing steps
#if 0
		if (is_equiv(sep)) {
			/*do nothing*/
		}else if (NOT_segment(sep)) {
			SeqEntryExplore(sep, mult, MergeBSinDescr);
		} else {
			SeqEntryExplore(sep, (Pointer) (&bs), CheckBS);
			if (bs.same == TRUE) {
				SeqEntryExplore(sep, (Pointer) (&bs), StripBSfromParts);
			} else {
				SeqEntryExplore(sep, (Pointer) (&bs), StripBSfromTop);
			}
		}

    FixNucProtSet(sep);
#endif
    // this step was FixProtMolInfo in the C Toolkit
    x_AddMissingProteinMolInfo(bh.GetParentEntry());

#if 0    
    // missing steps
	SeqEntryExplore (sep, NULL, FuseMolInfos);
	SeqEntryExplore(sep, NULL, StripProtXref);
	SeqEntryExplore(sep, (Pointer)(&qm), CheckMaps);
	/*
	if (qm.same == TRUE) {
		SeqEntryExplore(sep, (Pointer)(&qm), StripMaps);
	} else {
		SeqEntryExplore(sep, NULL, MapsToGenref);
	}
	*/
	SeqEntryExplore(sep, NULL, MapsToGenref);
	CheckGeneticCode(sep);
#endif        
    
    x_NormalizeMolInfo (bh);
}


void CCleanup_imp::LoopToAsn3(CBioseq_Handle bh)
{
    // these steps were called RemoveEmptyTitleAndPubGenAsOnlyPub
    x_RecurseForDescriptors(bh, &ncbi::objects::CCleanup_imp::x_RemoveEmptyTitles);
    x_RecurseForDescriptors(bh, &ncbi::objects::CCleanup_imp::x_RemoveCitGenPubDescriptors);
    x_RecurseForSeqAnnots(bh, &ncbi::objects::CCleanup_imp::x_RemoveCitGenPubFeatures);
    CheckSegSet(bh);
    
    CSeqdesc_CI src_desc (bh.GetParentEntry(), CSeqdesc::e_Source);
    if (src_desc) {
        // this step was called StripOld
        x_StripOldDescriptorsAndFeatures (bh);
              
        // this step was part of ToAsn4
        x_ConvertPubsToAsn4(bh.GetSeq_entry_Handle());
        
        // this step was also part of ToAsn4
        string lineage = "";
        x_GetGenBankTaxonomy(bh, lineage);
        if (!NStr::IsBlank(lineage)) {
            x_SetSourceLineage(bh, lineage);
        }
        
        //missing steps
    }
    
}


void CCleanup_imp::LoopToAsn3 (CSeq_entry_Handle seh)
{ 
    if (seh.Which() == CSeq_entry::e_Seq) {
        LoopToAsn3 (seh.GetSeq());
    } else if (seh.Which() == CSeq_entry::e_Set) {
        LoopToAsn3 (seh.GetSet());
    }
#if 0  
    const CSeq_entry& se = *(seh.GetCompleteSeq_entry());
    
    // these steps were called RemoveEmptyTitleAndPubGenAsOnlyPub
    x_RecurseForDescriptors(se, &ncbi::objects::CCleanup_imp::x_RemoveEmptyTitles);
    x_RecurseForDescriptors(se, &ncbi::objects::CCleanup_imp::x_RemoveCitGenPubDescriptors);
    x_RecurseForSeqAnnots(se, &ncbi::objects::CCleanup_imp::x_RemoveCitGenPubFeatures);
 
    if (seh.Which() == CSeq_entry::e_Set) {
        // these steps were called by toporg
        CheckSegSet(seh.GetSet());
        CheckNucProtSet(seh.GetSet());
    } else if (seh.Which() == CSeq_entry::e_Seq) {
        // this step was called by toporg
        CheckSegSet(seh.GetSet());
    }
    
    CSeqdesc_CI src_desc (seh, CSeqdesc::e_Source);
    if (src_desc) {
        // this step was called StripOld
        
        x_StripOldDescriptorsAndFeatures (seh.GetSet());
              
        // this step was part of ToAsn4
        x_ConvertPubsToAsn4(seh);
        
        // this step was also part of ToAsn4
        string lineage = "";
        x_GetGenBankTaxonomy(seh, lineage);
        if (!NStr::IsBlank(lineage)) {
            x_SetSourceLineage(seh, lineage);
        }

        
#if 0
		CombineBSFeat(sep);
		if (is_equiv(sep)) {
			/*do nothing*/
		}else if (NOT_segment(sep)) {
			SeqEntryExplore(sep, mult, MergeBSinDescr);
		} else {
			SeqEntryExplore(sep, (Pointer) (&bs), CheckBS);
			if (bs.same == TRUE) {
				SeqEntryExplore(sep, (Pointer) (&bs), StripBSfromParts);
			} else {
				SeqEntryExplore(sep, (Pointer) (&bs), StripBSfromTop);
			}
		}
		FixNucProtSet(sep);
		EntryChangeImpFeat(sep); 
		EntryChangeGBSource(sep); 
		SeqEntryExplore (sep, NULL, FixProtMolInfo);
		SeqEntryExplore (sep, NULL, FuseMolInfos);
		SeqEntryExplore(sep, NULL, StripProtXref);
		SeqEntryExplore(sep, (Pointer)(&qm), CheckMaps);

		SeqEntryExplore(sep, NULL, MapsToGenref);
		CheckGeneticCode(sep);
		NormalizeSegSeqMolInfo (sep);
		if(qm.name != NULL)
		{
			MemFree(qm.name);
		}
		
#endif    
        if (seh.Which() == CSeq_entry::e_Set) { 
            x_NormalizeMolInfo (CBioseq_set_Handle(seh.GetSet()));
        }
    }
    
#if 0
	ToAsn3 ta;
	OrgFixPtr ofp = NULL;
	MolFixPtr mfp = NULL;
	CharPtr porg = NULL;
	QualMap qm;
	BSMap bs;
	ValNodePtr mult = NULL;
	Int4 retval = INFO_ASNOLD, ret;
	Int2 update_date_pos;
	
	ta.had_biosource = FALSE;
	ta.had_molinfo = FALSE;
	ta.ofp = NULL;
	ta.mfp = NULL;
	qm.name = NULL;
	qm.same = TRUE;
	bs.same = TRUE;
	bs.bsp = NULL;
	
	if (sep == NULL) {
		return ERR_INPUT;
	}
	update_date_pos = GetUpdateDatePos (sep);
	RemoveEmptyTitleAndPubGenAsOnlyPub (sep);
	toporg(sep);
	SeqEntryExplore(sep, (Pointer)(&ta), FindOrg);
	
	if (ta.had_biosource) {    
/* entry is in asn.1 spec 3.0 already do the checks only */
		retval |= INFO_ASNNEW;
      	SeqEntryExplore(sep, NULL, StripOld);
		ToAsn4(sep);   			/* move pubs and lineage */
		CombineBSFeat(sep);
		if (is_equiv(sep)) {
			/*do nothing*/
		}else if (NOT_segment(sep)) {
			SeqEntryExplore(sep, mult, MergeBSinDescr);
		} else {
			SeqEntryExplore(sep, (Pointer) (&bs), CheckBS);
			if (bs.same == TRUE) {
				SeqEntryExplore(sep, (Pointer) (&bs), StripBSfromParts);
			} else {
				SeqEntryExplore(sep, (Pointer) (&bs), StripBSfromTop);
			}
		}
		 ret = FixNucProtSet(sep);
		 retval |= ret;
		EntryChangeImpFeat(sep); 
		EntryChangeGBSource(sep); 
		SeqEntryExplore (sep, NULL, FixProtMolInfo);
		SeqEntryExplore (sep, NULL, FuseMolInfos);
		SeqEntryExplore(sep, NULL, StripProtXref);
		SeqEntryExplore(sep, (Pointer)(&qm), CheckMaps);

		SeqEntryExplore(sep, NULL, MapsToGenref);
		CheckGeneticCode(sep);
		NormalizeSegSeqMolInfo (sep);
		toasn3_free(&ta);
		RestoreUpdateDatePos (sep, update_date_pos);
		if(qm.name != NULL)
		{
			MemFree(qm.name);
		}
		
		return retval;
	}
	if (ta.ofp == NULL) {
		ErrPostStr(SEV_WARNING, ERR_ORGANISM_NotFound, "No information found to create BioSource");
	}
	if (ta.mfp == NULL) {
		ErrPostStr(SEV_WARNING, ERR_ORGANISM_NotFound, "No information found to create MolInfo"); 
	}

	FixToAsn(sep, (Pointer)(&ta));

	if (ta.ofp != NULL) {
		ofp = ta.ofp;
		SeqEntryExplore(sep, (Pointer)ofp, FixOrg);
	}
	if (ta.mfp != NULL) {
		mfp = ta.mfp;
		SeqEntryExplore(sep, (Pointer)mfp, FixMol);
	}

/* entry  is converted to asn.1 spec 3.0, now do the checks */
	retval = INFO_ASNNEW;
	if(ta.had_biosource) {
		SeqEntryExplore(sep, NULL, StripOld);
	}
	ToAsn4(sep);          /* move pubs and lineage */
	if (is_equiv(sep)) {
			/*do nothing*/
	} else if (NOT_segment(sep)) {
		SeqEntryExplore(sep, mult, MergeBSinDescr);
	} else {
		SeqEntryExplore(sep, (Pointer) (&bs), CheckBS);
		if (bs.same == TRUE) {
			SeqEntryExplore(sep, (Pointer) (&bs), StripBSfromParts);
		} else {
			SeqEntryExplore(sep, (Pointer) (&bs), StripBSfromTop);
		}
	}
	ret = FixNucProtSet(sep);
	retval |= ret;
	EntryChangeImpFeat(sep); 
	EntryChangeGBSource(sep); 
	SeqEntryExplore (sep, NULL, FixProtMolInfo);
	SeqEntryExplore (sep, NULL, FuseMolInfos);
	SeqEntryExplore(sep, NULL, StripProtXref);
	SeqEntryExplore(sep, (Pointer)(&qm), CheckMaps);
	/*
	if (qm.same == TRUE) {
		SeqEntryExplore(sep, (Pointer)(&qm), StripMaps);
	} else {
		SeqEntryExplore(sep, NULL, MapsToGenref);
	}
	*/
	SeqEntryExplore(sep, NULL, MapsToGenref);
	CheckGeneticCode(sep);
	NormalizeSegSeqMolInfo (sep);
	toasn3_free(&ta);
	RestoreUpdateDatePos (sep, update_date_pos);
	if(qm.name)
		qm.name=MemFree(qm.name);
#endif    
#endif
}


void CCleanup_imp::x_GetGenBankTaxonomy(const CSeq_entry& se, string &taxonomy)
{
    if (se.Which() == CSeq_entry::e_Seq) {
        x_GetGenBankTaxonomy(m_Scope->GetBioseqHandle(se.GetSeq()), taxonomy);
    } else if (se.Which() == CSeq_entry::e_Set) {
        x_GetGenBankTaxonomy(m_Scope->GetBioseq_setHandle(se.GetSet()), taxonomy);
    }
}


void CCleanup_imp::x_GetGenBankTaxonomy(CSeq_entry_Handle seh, string& taxonomy)
{
    x_GetGenBankTaxonomy(*(seh.GetCompleteSeq_entry()), taxonomy);
}


void CCleanup_imp::x_GetGenBankTaxonomy(CBioseq_Handle bh, string &taxonomy)
{
    if (!bh.CanGetInst_Repr()
        || (bh.GetInst_Repr() != CSeq_inst::eRepr_raw
            && bh.GetInst_Repr() != CSeq_inst::eRepr_const)) {
        return;
    }
    CBioseq_EditHandle eh(bh);
    if (eh.IsSetDescr()) {
        NON_CONST_ITERATE (CSeq_descr::Tdata, desc_it, eh.SetDescr().Set()) {
            if ((*desc_it)->Which() == CSeqdesc::e_Genbank
                && (*desc_it)->GetGenbank().CanGetTaxonomy()) {
                taxonomy = (*desc_it)->GetGenbank().GetTaxonomy();
                (*desc_it)->SetGenbank().ResetTaxonomy();
            }
        }
    }
}


void CCleanup_imp::x_GetGenBankTaxonomy(CBioseq_set_Handle bh, string &taxonomy)
{
    CBioseq_set_EditHandle eh(bh);
    if (eh.IsSetDescr()) {
        NON_CONST_ITERATE (CSeq_descr::Tdata, desc_it, eh.SetDescr().Set()) {
            if ((*desc_it)->Which() == CSeqdesc::e_Genbank
                && (*desc_it)->GetGenbank().CanGetTaxonomy()) {
                taxonomy = (*desc_it)->GetGenbank().GetTaxonomy();
                (*desc_it)->SetGenbank().ResetTaxonomy();
            }
        }
    }
    
    ITERATE (list< CRef< CSeq_entry > >, it, bh.GetCompleteBioseq_set()->GetSeq_set()) {
        x_GetGenBankTaxonomy(m_Scope->GetSeq_entryHandle(**it), taxonomy);
    }    
}


// this function should find the highest level for which all of the
// mol-info descriptors on sequences and sets below this level are
// consistent, put the mol-info on this level, and remove all mol-info
// descriptors below this level
// was NormalizeSegSeqMolInfo in C Toolkit
void CCleanup_imp::x_NormalizeMolInfo(CBioseq_set_Handle bh)
{
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
           
    if (is_consistent) {
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
            CBioseq_set_EditHandle eh(bh);
            CRef<CSeqdesc> desc(new CSeqdesc);
            desc->SetMolinfo(*mol);
            eh.AddSeqdesc(*desc);
        }
    } else {
        ITERATE (list< CRef< CSeq_entry > >, it, bh.GetCompleteBioseq_set()->GetSeq_set()) {
            if ((*it)->Which() == CSeq_entry::e_Set) {
                x_NormalizeMolInfo(m_Scope->GetBioseq_setHandle((*it)->GetSet()));
            }
        }
    }
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.6  2006/10/04 14:17:47  bollin
 * Added step to ExtendedCleanup to move coding regions on nucleotide sequences
 * in nuc-prot sets to the nuc-prot set (was move_cds_ex in C Toolkit).
 * Replaced deprecated CSeq_feat_Handle.Replace calls with CSeq_feat_EditHandle.Replace calls.
 * Began implementing C++ version of LoopSeqEntryToAsn3.
 *
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
