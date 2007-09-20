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
* Author:  Colleen Bollin
*
* File Description:
*   Generate unique definition lines for a set of sequences using organism
*   descriptions and feature clauses.
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbimisc.hpp>
#include <objmgr/util/autodef.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/feature.hpp>

#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Bioseq.hpp>

#include <serial/iterator.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

            
CAutoDefSourceGroup::CAutoDefSourceGroup()
{
    m_SourceList.clear();
}


CAutoDefSourceGroup::CAutoDefSourceGroup(CAutoDefSourceGroup *other)
{
    _ASSERT (other);
    unsigned int index;
    
    m_SourceList.clear();
    
    for (index = 0; index < other->GetNumDescriptions(); index++) {
        m_SourceList.push_back(new CAutoDefSourceDescription(other->GetSourceDescription(index)));
    }
}


CAutoDefSourceGroup::~CAutoDefSourceGroup()
{
}

unsigned int CAutoDefSourceGroup::GetNumDescriptions()
{
    return m_SourceList.size();
}


CAutoDefSourceDescription *CAutoDefSourceGroup::GetSourceDescription(unsigned int index)
{
    _ASSERT(index < m_SourceList.size());
    return m_SourceList[index];
}


void CAutoDefSourceGroup::AddSourceDescription(CAutoDefSourceDescription *tmp)
{
    if (tmp == NULL) {
        return;
    }
    m_SourceList.push_back (tmp);
    
    
}


void CAutoDefSourceGroup::x_SortDescriptions(IAutoDefCombo *mod_combo) 
{
    CAutoDefSourceDescription *tmp;
    // insertion sort
    for (unsigned int k = 1; k < m_SourceList.size(); k ++) {
        unsigned int j = k;
        tmp = m_SourceList[k];
        string tmp_desc = m_SourceList[k]->GetComboDescription(mod_combo);
        
        while (j > 0 && NStr::strcasecmp(m_SourceList[j - 1]->GetComboDescription(mod_combo).c_str(), tmp_desc.c_str()) > 0) {
            m_SourceList[j] = m_SourceList[j - 1];
            j--;
        }
        m_SourceList[j] = tmp;
    }
}


CAutoDefSourceGroup *CAutoDefSourceGroup::RemoveNonMatchingDescriptions (IAutoDefCombo *mod_combo)
{
    x_SortDescriptions(mod_combo);

    if (m_SourceList.size() < 2) { 
        return NULL;
    }
    
    string first_desc = m_SourceList[0]->GetComboDescription(mod_combo);
    string last_desc = m_SourceList[m_SourceList.size() - 1]->GetComboDescription(mod_combo);
    if (NStr::Equal(first_desc, last_desc)) {
        return NULL;
    }
    
    CAutoDefSourceGroup *new_grp = new CAutoDefSourceGroup();
    unsigned int start_index = 1;
    while (start_index < m_SourceList.size() 
           && NStr::Equal(first_desc, m_SourceList[start_index]->GetComboDescription(mod_combo))) {
        start_index++;
    }
    for (unsigned int k = start_index; k < m_SourceList.size(); k++) {
        new_grp->AddSourceDescription(m_SourceList[k]);
        m_SourceList[k] = NULL;
    }
    while (m_SourceList.size() > start_index) {
        m_SourceList.pop_back();
    }
    return new_grp;
}


bool CAutoDefSourceGroup::SourceDescBelongsHere(CAutoDefSourceDescription *source_desc, IAutoDefCombo *mod_combo)
{
    string this_desc, other_desc;
    if (source_desc == NULL) {
        return false;
    }
    
    this_desc = m_SourceList[0]->GetComboDescription(mod_combo);
    other_desc = source_desc->GetComboDescription(mod_combo);
    return NStr::Equal(this_desc, other_desc);
}


void CAutoDefSourceGroup::GetAvailableModifiers (CAutoDefSourceDescription::TAvailableModifierVector &modifier_list)
{
    unsigned int k;
    
    for (k = 0; k < m_SourceList.size(); k++) {
        m_SourceList[k]->GetAvailableModifiers(modifier_list);
    }
}


bool CAutoDefSourceGroup::HasTrickyHIV ()
{
    bool has_tricky = false;
    
    for (unsigned int k = 0; k < m_SourceList.size() && ! has_tricky; k++) {
        has_tricky = m_SourceList[k]->IsTrickyHIV();
    }
    return has_tricky;
}


bool CAutoDefSourceGroup::GetDefaultExcludeSp ()
{
    bool all_diff_sp_orgnames = true;

    for (unsigned int k = 0; k < m_SourceList.size() && all_diff_sp_orgnames; k++) {
        const CBioSource& bsrc = m_SourceList[k]->GetBioSource();
        if (bsrc.CanGetOrg() && bsrc.GetOrg().CanGetTaxname()) {
            string taxname1 = bsrc.GetOrg().GetTaxname();
            if (IsSpName (taxname1)) {
                for (unsigned int j = 0; j < m_SourceList.size() && all_diff_sp_orgnames; j++) {
                    const CBioSource& bsrc2 = m_SourceList[k]->GetBioSource();
                    if (bsrc2.CanGetOrg() && bsrc2.GetOrg().CanGetTaxname()) {
                        string taxname2 = bsrc.GetOrg().GetTaxname();
                        if (IsSpName (taxname2) && NStr::Equal (taxname1, taxname2)) {
                            all_diff_sp_orgnames = false;
                        }
                    }
                }
            }
        }
    }
    return all_diff_sp_orgnames;
}


END_SCOPE(objects)
END_NCBI_SCOPE
