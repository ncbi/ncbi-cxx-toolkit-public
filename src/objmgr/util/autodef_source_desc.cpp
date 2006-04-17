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

#include <gui/objutils/edit.hpp>
#include <gui/objutils/bioseq_edit.hpp>

#include <objtools/format/context.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CAutoDefSourceDescription::CAutoDefSourceDescription(const CBioSource& bs) : m_BS(bs)
{
    m_DescriptionStringList.clear();
    m_DescriptionStringList.push_back(m_BS.GetOrg().GetTaxname());
}


CAutoDefSourceDescription::CAutoDefSourceDescription(CAutoDefSourceDescription * value) : m_BS(value->GetBioSource())
{
    unsigned int index;
    m_DescriptionStringList.clear();
    for (index = 0; index < value->GetNumDescriptionStrings(); index++) {
        m_DescriptionStringList.push_back(value->GetDescriptionString(index));
    }
}


CAutoDefSourceDescription::~CAutoDefSourceDescription()
{
}

const CBioSource& CAutoDefSourceDescription::GetBioSource()
{
    return m_BS;
}


string CAutoDefSourceDescription::GetDescriptionString(unsigned int index)
{
    if (index >= m_DescriptionStringList.size()) {
        return "";
    } else {
        return m_DescriptionStringList[index];
    }
}

unsigned int CAutoDefSourceDescription::GetNumDescriptionStrings()
{
    return m_DescriptionStringList.size();
}


bool CAutoDefSourceDescription::RemainingStringsEmpty(unsigned int index) 
{
    while (index < m_DescriptionStringList.size()) {
        if(!NStr::Equal("", m_DescriptionStringList[index])) {
            return false;
        }
        index++;
    }
    return true;
}


int CAutoDefSourceDescription::CompareDescription (CAutoDefSourceDescription *other_desc)
{
    unsigned int index;
    int          retval;
    
    if (other_desc == NULL) {
        return 1;
    }
    
    for (index = 0; index < m_DescriptionStringList.size(); index++) {
        retval = NStr::CompareNocase(m_DescriptionStringList[index], other_desc->GetDescriptionString(index));
        if (retval != 0) {
            return retval;
        }
    }
    if (!other_desc->RemainingStringsEmpty(index)) {
        return -1;
    } else {
        return 0;
    }
}


void CAutoDefSourceDescription::AddSubsource(CSubSource::ESubtype st)
{
    bool found = false;
    
    if (m_BS.CanGetSubtype()) {
        ITERATE (CBioSource::TSubtype, subSrcI, m_BS.GetSubtype()) {
            if ((*subSrcI)->GetSubtype() == st) {
                m_DescriptionStringList.push_back ((*subSrcI)->GetName());
                found = true;
            }
        }
    }
    if (!found) {
        m_DescriptionStringList.push_back("");
    }
}


void CAutoDefSourceDescription::AddOrgMod(COrgMod::ESubtype st)
{
    bool found = false;
    if (m_BS.CanGetOrg() && m_BS.GetOrg().CanGetOrgname() && m_BS.GetOrg().GetOrgname().CanGetMod()) {
        ITERATE (COrgName::TMod, modI, m_BS.GetOrg().GetOrgname().GetMod()) {
            if ((*modI)->GetSubtype() == st) {
                string value;
                (*modI)->GetSubtypeValue(value);
                m_DescriptionStringList.push_back (value);
                found = true;
            }
        }
    }
    if (!found) {
        m_DescriptionStringList.push_back("");
    }
}


void CAutoDefSourceDescription::GetAvailableModifiers (TAvailableModifierVector &modifier_list)
{
    unsigned int k;
    
    for (k = 0; k < modifier_list.size(); k++) {
        bool found = false;
        if (modifier_list[k].IsOrgMod()) {
            if (m_BS.CanGetOrg() && m_BS.GetOrg().CanGetOrgname() && m_BS.GetOrg().GetOrgname().CanGetMod()) {
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


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.2  2006/04/17 17:39:37  ucko
* Fix capitalization of header filenames.
*
* Revision 1.1  2006/04/17 16:25:05  bollin
* files for automatically generating definition lines, using a combination
* of modifiers to make definition lines unique within a set and listing the
* relevant features on the sequence.
*
*
* ===========================================================================
*/

