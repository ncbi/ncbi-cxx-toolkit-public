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
#include <objtools/edit/autodef_source_desc.hpp>
#include <corelib/ncbimisc.hpp>
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

CAutoDefSourceDescription::CAutoDefSourceDescription(const CBioSource& bs, string feature_clauses) : m_BS(bs)
{
    // consider feature clauses when looking for uniqueness
    m_FeatureClauses = feature_clauses;

    if (bs.CanGetOrg() && bs.GetOrg().CanGetTaxname()) {
        m_DescStrings.push_back (bs.GetOrg().GetTaxname());
    }
    if (bs.CanGetOrg() && bs.GetOrg().CanGetOrgname() && bs.GetOrg().GetOrgname().CanGetMod()) {
        ITERATE (COrgName::TMod, modI, bs.GetOrg().GetOrgname().GetMod()) {
            m_Modifiers.push_back (CAutoDefSourceModifierInfo(true, (*modI)->GetSubtype(), (*modI)->GetSubname()));
        }
    }
    ITERATE (CBioSource::TSubtype, subSrcI, bs.GetSubtype()) {
        m_Modifiers.push_back (CAutoDefSourceModifierInfo(false, (*subSrcI)->GetSubtype(), (*subSrcI)->GetName()));
    }
    std::sort (m_Modifiers.begin(), m_Modifiers.end());
}


CAutoDefSourceDescription::CAutoDefSourceDescription(CAutoDefSourceDescription *other) : m_BS(other->GetBioSource())
{
    // copy strings
    ITERATE (TDescString, string_it, other->GetStrings()) {
        m_DescStrings.push_back (*string_it);
    }
    // copy remaining modifier list
    ITERATE (TModifierVector, it, other->GetModifiers()) {
        m_Modifiers.push_back (CAutoDefSourceModifierInfo(*it));
    }
    // copy feature clauses
    m_FeatureClauses = other->m_FeatureClauses;
}


CAutoDefSourceDescription::~CAutoDefSourceDescription()
{
}

const CBioSource& CAutoDefSourceDescription::GetBioSource() const
{
    return m_BS;
}


bool CAutoDefSourceDescription::AddQual (bool isOrgMod, int subtype, bool keepAfterSemicolon)
{
    bool rval = false;
    TModifierVector::iterator it;

    it = m_Modifiers.begin();
    while (it != m_Modifiers.end()) {
        if (isOrgMod) {
            if (it->IsOrgMod() && it->GetSubtype() == subtype) {
				string val = it->GetValue();
				if (!keepAfterSemicolon) {
                    string::size_type end = NStr::Find(val, ";");
                    if (end != NCBI_NS_STD::string::npos) {
                        val = val.substr(0, end);
					}
				}
                m_DescStrings.push_back (val);
                it = m_Modifiers.erase(it);
                rval = true;
            } else {
                ++it;
            }
        } else {
            if (!it->IsOrgMod() && it->GetSubtype() == subtype) {
				string val = it->GetValue();
				if (!keepAfterSemicolon) {
                    string::size_type end = NStr::Find(val, ";");
                    if (end != NCBI_NS_STD::string::npos) {
                        val = val.substr(0, end);
					}
				}
                m_DescStrings.push_back (val);
                it = m_Modifiers.erase(it);
                rval = true;
            } else {
                ++it;
            }
        }
    }
    return rval;
}


bool CAutoDefSourceDescription::RemoveQual (bool isOrgMod, int subtype)
{
    bool rval = false;
    TModifierVector::iterator it;

    it = m_Modifiers.begin();
    while (it != m_Modifiers.end()) {
        if (isOrgMod) {
            if (it->IsOrgMod() && it->GetSubtype() == subtype) {
                it = m_Modifiers.erase(it);
                rval = true;
            } else {
                ++it;
            }
        } else {
            if (!it->IsOrgMod() && it->GetSubtype() == subtype) {
                it = m_Modifiers.erase(it);
                rval = true;
            } else {
                ++it;
            }
        }
    }
    return rval;
}


int CAutoDefSourceDescription::Compare(const CAutoDefSourceDescription& s) const
{
    unsigned int k = 0;
    int rval = 0;
    TDescString::const_iterator s_it, this_it;

    s_it = s.GetStrings().begin();
    this_it = GetStrings().begin();
    while (s_it != s.GetStrings().end()
           && this_it != GetStrings().end()
           && rval == 0) { 
        rval = NStr::Compare (*this_it, *s_it);
        k++;
        ++s_it;
        ++this_it;
    }
    if (rval == 0) {
        if (k < s.GetStrings().size()) {
            rval = -1;
        } else if (k < m_DescStrings.size()) {
            rval = 1;
        }
    }
    if (rval == 0) {
        rval = NStr::Compare (GetFeatureClauses(), s.GetFeatureClauses());
    }
    return rval;
}


string CAutoDefSourceDescription::GetComboDescription(IAutoDefCombo *mod_combo)
{
    string desc = "";
    if (mod_combo) {
        return mod_combo->GetSourceDescriptionString(m_BS);
    } else {
        return m_BS.GetOrg().GetTaxname();
    }
}


void CAutoDefSourceDescription::GetAvailableModifiers (TAvailableModifierVector &modifier_list)
{
    unsigned int k;
    
    for (k = 0; k < modifier_list.size(); k++) {
        bool found = false;
        if (modifier_list[k].IsOrgMod()) {
            if (m_BS.CanGetOrg() && m_BS.GetOrg().CanGetOrgname() && m_BS.GetOrg().GetOrgname().IsSetMod()) {
                ITERATE (COrgName::TMod, modI, m_BS.GetOrg().GetOrgname().GetMod()) {
                    if ((*modI)->GetSubtype() == modifier_list[k].GetOrgModType()) {
                        found = true;
                        modifier_list[k].ValueFound((*modI)->GetSubname() );
                    }
                }
            }
        } else {
            // get subsource modifiers
            if (m_BS.CanGetSubtype()) {
                ITERATE (CBioSource::TSubtype, subSrcI, m_BS.GetSubtype()) {
                    if ((*subSrcI)->GetSubtype() == modifier_list[k].GetSubSourceType()) {
                        found = true;
                        modifier_list[k].ValueFound((*subSrcI)->GetName());
                    }
                }
            }
        }
        if (!found) {
            modifier_list[k].ValueFound("");
        }
    }    
}

// tricky HIV records have an isolate and a clone
bool CAutoDefSourceDescription::IsTrickyHIV()
{
    string tax_name = m_BS.GetOrg().GetTaxname();
    if (!NStr::Equal(tax_name, "HIV-1") && !NStr::Equal(tax_name, "HIV-2")) {
        return false;
    }
    
    bool found = false;
    
    if (m_BS.CanGetSubtype()) {
        ITERATE (CBioSource::TSubtype, subSrcI, m_BS.GetSubtype()) {
            if ((*subSrcI)->GetSubtype() == CSubSource::eSubtype_clone) {
                found = true;
            }
        }
    }
    if (!found) {
        return false;
    }   
    
    found = false;
    if (m_BS.CanGetOrg() && m_BS.GetOrg().CanGetOrgname() && m_BS.GetOrg().GetOrgname().IsSetMod()) {
        ITERATE (COrgName::TMod, modI, m_BS.GetOrg().GetOrgname().GetMod()) {
            if ((*modI)->GetSubtype() == COrgMod::eSubtype_isolate) {
                found = true;
            }
        }
    }
    return found;
}


CAutoDefSourceModifierInfo::CAutoDefSourceModifierInfo(bool isOrgMod, int subtype, string value)
{
    m_IsOrgMod = isOrgMod;
    m_Subtype = subtype;
    m_Value = value;
}


CAutoDefSourceModifierInfo::CAutoDefSourceModifierInfo(const CAutoDefSourceModifierInfo &other)
{
    m_IsOrgMod = other.IsOrgMod();
    m_Subtype = other.GetSubtype();
    m_Value = other.GetValue();
}


CAutoDefSourceModifierInfo::~CAutoDefSourceModifierInfo()
{
}


unsigned int CAutoDefSourceModifierInfo::GetRank() const
{
    if (m_IsOrgMod) {
        if (m_Subtype == COrgMod::eSubtype_strain) {
            return 3;
        } else if (m_Subtype == COrgMod::eSubtype_isolate) {
            return 5;
        } else if (m_Subtype == COrgMod::eSubtype_cultivar) {
            return 7;
        } else if (m_Subtype == COrgMod::eSubtype_specimen_voucher) {
            return 8;
        } else if (m_Subtype == COrgMod::eSubtype_ecotype) {
            return 9;
        } else if (m_Subtype == COrgMod::eSubtype_type) {
            return 10;
        } else if (m_Subtype == COrgMod::eSubtype_serotype) {
            return 11;
        } else if (m_Subtype == COrgMod::eSubtype_authority) {
            return 12;
        } else if (m_Subtype == COrgMod::eSubtype_breed) {
            return 13;
        }
    } else {
        if (m_Subtype == CSubSource::eSubtype_transgenic) {
            return 0;
        } else if (m_Subtype == CSubSource::eSubtype_plasmid_name) {
            return 1;
         } else if (m_Subtype == CSubSource::eSubtype_endogenous_virus_name)  {
            return 2;
        } else if (m_Subtype == CSubSource::eSubtype_clone) {
            return 4;
        } else if (m_Subtype == CSubSource::eSubtype_haplotype) {
            return 6;
        }
    }
    return 50;
}


int CAutoDefSourceModifierInfo::Compare(const CAutoDefSourceModifierInfo& mod) const
{
    int rank1, rank2;

    rank1 = GetRank();
    rank2 = mod.GetRank();
 
    if (rank1 < rank2) {
        return -1;
    } else if (rank1 > rank2) {
        return 1;
    } else if (IsOrgMod() && !mod.IsOrgMod()) {
        // prefer subsource to orgmod qualifiers 
        return -1;
    } else if (!IsOrgMod() && mod.IsOrgMod()) {
        return 1;
    } else if (IsOrgMod() && mod.IsOrgMod()) {
        if (GetSubtype() == mod.GetSubtype()) {
            return 0;
        } else {
            return (GetSubtype() < mod.GetSubtype() ? -1 : 1);
        }
    } else {
        if (GetSubtype() == mod.GetSubtype()) {
            return 0;
        } else {
            return (GetSubtype() < mod.GetSubtype() ? -1 : 1);
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
