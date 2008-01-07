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
#include <objmgr/util/autodef.hpp>
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

CAutoDefSourceDescription::CAutoDefSourceDescription(const CBioSource& bs) : m_BS(bs)
{
}


CAutoDefSourceDescription::CAutoDefSourceDescription(CAutoDefSourceDescription * value) : m_BS(value->GetBioSource())
{
}


CAutoDefSourceDescription::~CAutoDefSourceDescription()
{
}

const CBioSource& CAutoDefSourceDescription::GetBioSource()
{
    return m_BS;
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


END_SCOPE(objects)
END_NCBI_SCOPE
