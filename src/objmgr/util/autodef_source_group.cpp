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
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/feature.hpp>

#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Bioseq.hpp>

#include <serial/iterator.hpp>

#include <objmgr/util/autodef_source_group.hpp>

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
        m_SourceList.push_back(CRef<CAutoDefSourceDescription>(new CAutoDefSourceDescription(other->GetSourceDescription(index))));
    }
}


CAutoDefSourceGroup::~CAutoDefSourceGroup()
{
}


void CAutoDefSourceGroup::AddSource (CRef<CAutoDefSourceDescription> src)
{
    m_SourceList.push_back(src);
}


bool CAutoDefSourceGroup::AddQual (bool IsOrgMod, int subtype, bool keepAfterSemicolon)
{
    bool rval = false;

    NON_CONST_ITERATE (TSourceDescriptionVector, it, m_SourceList) {
        rval |= (*it)->AddQual (IsOrgMod, subtype, keepAfterSemicolon);
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


struct SAutoDefSourceDescByStrings {
    bool operator()(CRef<CAutoDefSourceDescription> s1,
                    CRef<CAutoDefSourceDescription> s2) const
    {
        return (*s1 < *s2);
    }
};


void CAutoDefSourceGroup::SortDescriptions()
{
     std::sort(m_SourceList.begin(), m_SourceList.end(), SAutoDefSourceDescByStrings());
}


// this function will make a new group out of any source descriptions that don't match the first one in the list
CRef<CAutoDefSourceGroup> CAutoDefSourceGroup::SplitGroup()
{
    CRef<CAutoDefSourceGroup> g(NULL);
    auto it = m_SourceList.begin();
    it++;
    while (it != m_SourceList.end() && (*it)->Compare(*m_SourceList[0]) == 0) {
        it++;
    }
    if (it != m_SourceList.end()) {
        g.Reset(new CAutoDefSourceGroup());
        while (it != m_SourceList.end()) {
            g->AddSource(*it);
            it = m_SourceList.erase(it);
        }
    }
    return g;
}


// After adding a qualifier, some descriptions should no longer match, so they should be
// part of new source groups
vector<CRef<CAutoDefSourceGroup> > CAutoDefSourceGroup::RemoveNonMatchingDescriptions ()
{
    vector<CRef<CAutoDefSourceGroup> > group_list;
    TSourceDescriptionVector::iterator it;

    group_list.clear();

    if (m_SourceList.size() < 2) {
        return group_list;
    }
    std::sort (m_SourceList.begin(), m_SourceList.end(), SAutoDefSourceDescByStrings());
    it = m_SourceList.begin();
    it++;
    while (it != m_SourceList.end() && (*it)->Compare (*m_SourceList[0]) == 0) {
        it++;
    }
    while (it != m_SourceList.end()) {
        CRef<CAutoDefSourceGroup> g(new CAutoDefSourceGroup());
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

//isOrgMod, subtype
typedef pair<bool, int> TModPair;
typedef map<TModPair, bool> TModMap;

CAutoDefSourceDescription::TModifierVector CAutoDefSourceGroup::GetModifiersPresentForAny()
{
    CAutoDefSourceDescription::TModifierVector mods;
    CAutoDefSourceDescription::TModifierVector::iterator mod_it_other;
    TModMap present_any;

    mods.clear();
    ITERATE (TSourceDescriptionVector, it, m_SourceList) {
        ITERATE (CAutoDefSourceDescription::TModifierVector, mod_it, (*it)->GetModifiers()) {
            // don't include notes. both CSubSource::eSubtype_other and COrgMod::eSubtype_other
            // are notes and have the same numerical value (255)
            if (mod_it->GetSubtype() != (int)CSubSource::eSubtype_other) {
                TModPair x(mod_it->IsOrgMod(), mod_it->GetSubtype());
                if (present_any.find(x) == present_any.end()) {
                    present_any[x] = true;
                    mods.push_back(*mod_it);
                }
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


void CAutoDefSourceGroup::AddSourceDescription(CRef<CAutoDefSourceDescription> tmp)
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
    string::size_type pos = NStr::Find(taxname, " sp.");
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
