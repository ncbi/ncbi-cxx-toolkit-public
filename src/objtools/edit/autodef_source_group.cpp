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
//#include <objtools/edit/autodef.hpp>
#include <corelib/ncbimisc.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/feature.hpp>

#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Bioseq.hpp>

#include <serial/iterator.hpp>

#include <objtools/edit/autodef_source_group.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)



CAutoDefSourceGroup::CAutoDefSourceGroup()
{
    //m_SourceList.clear();
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
    unsigned int k;
    for (k = 0; k < m_SourceList.size(); k++) {
        delete (m_SourceList[k]);
    }
}


void CAutoDefSourceGroup::AddSource (CAutoDefSourceDescription *src)
{
    m_SourceList.push_back (new CAutoDefSourceDescription(src));
}


bool CAutoDefSourceGroup::AddQual (bool IsOrgMod, int subtype)
{
    bool rval = false;

    NON_CONST_ITERATE (TSourceDescriptionVector, it, m_SourceList) {
        rval |= (*it)->AddQual (IsOrgMod, subtype);
    }

    return rval;
}


bool CAutoDefSourceGroup::RemoveQual (bool IsOrgMod, int subtype)
{
    bool rval = false;

    NON_CONST_ITERATE (TSourceDescriptionVector, it, m_SourceList) {
        rval |= (*it)->RemoveQual (IsOrgMod, subtype);
    }

    return rval;
}


vector<CAutoDefSourceGroup *> CAutoDefSourceGroup::RemoveNonMatchingDescriptions ()
{
    vector<CAutoDefSourceGroup *> group_list;
    TSourceDescriptionVector::iterator it;

    group_list.clear();

    if (m_SourceList.size() < 2) {
        return group_list;
    }
    std::sort (m_SourceList.begin(), m_SourceList.end());
    it = m_SourceList.begin();
    it++;
    while (it != m_SourceList.end() && (*it)->Compare (*m_SourceList[0]) == 0) {
        it++;
    }
    while (it != m_SourceList.end()) {
        CAutoDefSourceGroup *g = new CAutoDefSourceGroup();
        g->AddSource (*it);
        it = m_SourceList.erase(it);
        while (it != m_SourceList.end() 
            && (*it)->Compare (*(g->GetSrcList()[0])) == 0) {
            g->AddSource (*it);
            it = m_SourceList.erase(it);
        }        
        group_list.push_back (g);
    }
    return group_list;
}


CAutoDefSourceDescription::TModifierVector CAutoDefSourceGroup::GetModifiersPresentForAll()
{
    CAutoDefSourceDescription::TModifierVector mods;
    CAutoDefSourceDescription::TModifierVector::const_iterator mod_it, mod_it_other;
    CAutoDefSourceDescription::TModifierVector::iterator mod_list_it;
    TSourceDescriptionVector::iterator it;
    bool found_mod;

    mods.clear();
    if (m_SourceList.size() > 0) {
        it = m_SourceList.begin();
        mod_it = (*it)->GetModifiers().begin();
        while (mod_it != (*it)->GetModifiers().end()) {
            mods.push_back (*mod_it);
            ++mod_it;
        }
        it++;
        while (it != m_SourceList.end() && mods.size() > 0) {
            mod_list_it = mods.begin();
            while (mod_list_it != mods.end()) {
                mod_it_other = (*it)->GetModifiers().begin();
                found_mod = false;
                while (mod_it_other != (*it)->GetModifiers().end() && !found_mod) {
                    if (mod_list_it->Compare (*mod_it_other) == 0) {
                        found_mod = true;
                    }
                    mod_it_other++;
                }
                if (found_mod) {
                    mod_list_it++;
                } else {
                    mod_list_it = mods.erase (mod_list_it);
                }
            }
            it++;
        }
    }

    return mods;
}


CAutoDefSourceDescription::TModifierVector CAutoDefSourceGroup::GetModifiersPresentForAny()
{
    CAutoDefSourceDescription::TModifierVector mods;
    CAutoDefSourceDescription::TModifierVector::iterator mod_it_other;
    bool found_mod;

    mods.clear();
    ITERATE (TSourceDescriptionVector, it, m_SourceList) {
        ITERATE (CAutoDefSourceDescription::TModifierVector, mod_it, (*it)->GetModifiers()) {
            found_mod = false;
            mod_it_other = mods.begin();
            while (mod_it_other != mods.end() && !found_mod) {
                if (mod_it->Compare (*mod_it_other) == 0) {
                    found_mod = true;
                }
                mod_it_other++;
            }
            if (!found_mod) {
                mods.push_back (*mod_it);
            }
        }
    }

    return mods;
}


// want to sort in descending number of sources
int CAutoDefSourceGroup::Compare(const CAutoDefSourceGroup& s) const
{
    unsigned int num_other, num_this;
    int rval = 0;

    num_other = s.GetSrcList().size();
    num_this = m_SourceList.size();

    if (num_other < num_this) {
        rval = -1;
    } else if (num_other > num_this) {
        rval = 1;
    }
    return rval;
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


bool IsSpName (string taxname)
{
    bool is_sp_name = false;
    string::size_type pos = NStr::Find(taxname, " sp. ");
    if (pos != NCBI_NS_STD::string::npos
        && (pos < 2 || !NStr::StartsWith(taxname.substr(pos - 2), "f."))) {
        is_sp_name = true;
    }
    return is_sp_name;
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
