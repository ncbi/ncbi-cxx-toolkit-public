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
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/seq_entry_ci.hpp>

#include "cleanupp.hpp"


BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

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
	}
}

// Was EntryChangeGBSource in C Toolkit
void CCleanup_imp::x_ChangeGenBankBlocks(CSeq_entry_Handle seh)
{
    CSeqdesc_CI desc_i(seh, CSeqdesc::e_Source);
    
    if (desc_i && (*desc_i).GetSource().CanGetOrg()) {
        string src;
        src.clear();
        
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
        div.clear();
        
        if (org.CanGetOrgname() && org.GetOrgname().CanGetDiv()) {
            div = org.GetOrgname().GetDiv();
        }
        CSeq_entry_CI seq_iter(seh);
        while (seq_iter) {
            x_ChangeGBDiv (*seq_iter, div);
        }
    }
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
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
