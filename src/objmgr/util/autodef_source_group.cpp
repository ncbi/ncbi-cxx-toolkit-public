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

#include <objtools/format/context.hpp>

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


void CAutoDefSourceGroup::x_SortDescriptions() 
{
    CAutoDefSourceDescription *tmp;
    // insertion sort
    for (unsigned int k = 1; k < m_SourceList.size(); k ++) {
        unsigned int j = k;
        tmp = m_SourceList[k];
        while (j > 0 && m_SourceList[j - 1]->CompareDescription(tmp) > 0) {
            m_SourceList[j] = m_SourceList[j - 1];
            j--;
        }
        m_SourceList[j] = tmp;
    }
}


CAutoDefSourceGroup *CAutoDefSourceGroup::RemoveNonMatchingDescriptions ()
{
    x_SortDescriptions();

    if (m_SourceList.size() < 2 || m_SourceList[m_SourceList.size() - 1]->CompareDescription(m_SourceList[0]) == 0) {
        return NULL;
    }
    
    CAutoDefSourceGroup *new_grp = new CAutoDefSourceGroup();
    unsigned int start_index = 1;
    while (start_index < m_SourceList.size() && m_SourceList[start_index]->CompareDescription(m_SourceList[0]) == 0) {
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


void CAutoDefSourceGroup::AddSubsource(CSubSource::ESubtype st)
{
    for (unsigned int k = 0; k < m_SourceList.size(); k++) {
        m_SourceList[k]->AddSubsource(st);
    }
}


void CAutoDefSourceGroup::AddOrgMod(COrgMod::ESubtype st)
{
    for (unsigned int k = 0; k < m_SourceList.size(); k++) {
        m_SourceList[k]->AddOrgMod(st);
    }
}


bool CAutoDefSourceGroup::SourceDescBelongsHere(CAutoDefSourceDescription *source_desc)
{
    if (source_desc == NULL) {
        return false;
    } else if (source_desc->CompareDescription(m_SourceList[0]) == 0) {
        return true;
    } else {
        return false;
    }
}


void CAutoDefSourceGroup::GetAvailableModifiers (CAutoDefSourceDescription::TAvailableModifierVector &modifier_list)
{
    unsigned int k;
    
    for (k = 0; k < m_SourceList.size(); k++) {
        m_SourceList[k]->GetAvailableModifiers(modifier_list);
    }
}



END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.3  2006/04/17 17:42:21  ucko
* Drop extraneous and disconcerting inclusion of gui headers.
*
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

