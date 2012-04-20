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
* Author: Aleksey Grichenko, Eugene Vasilchenko
*
* File Description:
*   Bioseq iterator
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/impl/scope_impl.hpp>
#include <objmgr/impl/bioseq_set_info.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


inline
bool CBioseq_CI::x_IsValidMolType(const CBioseq_Info& seq) const
{
    switch (m_Filter) {
    case CSeq_inst::eMol_not_set:
        return true;
    case CSeq_inst::eMol_na:
        return seq.IsNa();
    default:
        break;
    }
    return seq.GetInst_Mol() == m_Filter;
}


void CBioseq_CI::x_PushEntry(const CSeq_entry_Handle& entry)
{
    if ( !entry || entry.IsSeq() ) {
        m_CurrentEntry = entry;
    }
    else {
        if ( entry.x_GetInfo().GetSet().GetClass() ==
             CBioseq_set::eClass_parts ) {
            if ( m_Level == eLevel_Mains ) {
                x_NextEntry();
                return;
            }
            ++m_InParts;
        }
        m_EntryStack.push_back(CSeq_entry_CI(entry));
        _ASSERT(m_EntryStack.back().GetParentBioseq_set()==entry.GetSet());
        if ( m_EntryStack.back() ) {
            m_CurrentEntry = *m_EntryStack.back();
        }
        else {
            m_CurrentEntry.Reset();
        }
    }
}


void CBioseq_CI::x_NextEntry(void)
{
    if ( !m_EntryStack.empty() &&
         m_EntryStack.back() &&
         ++m_EntryStack.back() ) {
        m_CurrentEntry = *m_EntryStack.back();
    }
    else {
        m_CurrentEntry.Reset();
    }
}


void CBioseq_CI::x_PopEntry(bool next)
{
    if ( m_EntryStack.back().GetParentBioseq_set().GetClass() ==
         CBioseq_set::eClass_parts ) {
        --m_InParts;
    }
    m_EntryStack.pop_back();
    if ( next ) {
        x_NextEntry();
    }
    else {
        m_CurrentEntry.Reset();
    }
}


inline
bool sx_IsNa(CSeq_inst::EMol mol)
{
    return mol == CSeq_inst::eMol_dna  ||
        mol == CSeq_inst::eMol_rna  ||
        mol == CSeq_inst::eMol_na;
}


inline
bool sx_IsProt(CSeq_inst::EMol mol)
{
    return mol == CSeq_inst::eMol_aa;
}


bool CBioseq_CI::x_SkipClass(CBioseq_set::TClass set_class)
{
    int pos = m_EntryStack.size();
    while ( --pos >= 0 &&
            m_EntryStack[pos].GetParentBioseq_set().GetClass() != set_class ) {
        // level up
    }
    if ( pos < 0 ) {
        return false;
    }
    while ( m_EntryStack.size() > size_t(pos+1) ) {
        x_PopEntry(false);
    }
    x_PopEntry();
    return true;
}


void CBioseq_CI::x_Settle(void)
{
    bool found_na = m_CurrentBioseq  &&  sx_IsNa(m_Filter);
    m_CurrentBioseq.Reset();
    for ( ;; ) {
        if ( !m_CurrentEntry ) {
            if ( m_EntryStack.empty() ) {
                // no more entries
                return;
            }
            x_PopEntry();
        }
        else if ( m_CurrentEntry.IsSeq() ) {
            // Single bioseq
            if ( m_Level != eLevel_Parts  ||  m_InParts > 0 ) {
                if ( x_IsValidMolType(m_CurrentEntry.x_GetInfo().GetSeq()) ) {
                    m_CurrentBioseq = m_CurrentEntry.GetSeq();
                    return; // valid bioseq found
                }
                else if ( m_Level != eLevel_IgnoreClass  &&
                          !m_EntryStack.empty() ) {
                    if ( found_na &&
                         m_EntryStack.back().GetParentBioseq_set().GetClass()
                         == CBioseq_set::eClass_nuc_prot ) {
                        // Skip only the same level nuc-prot set
                        found_na = false; // no more skipping
                        if ( x_SkipClass(CBioseq_set::eClass_nuc_prot) ) {
                            continue;
                        }
                    }
                    else if ( sx_IsProt(m_Filter) ) {
                        // Skip the whole nuc segset when collecting prots
                        // Also skip conset
                        if ( x_SkipClass(CBioseq_set::eClass_segset) ||
                             x_SkipClass(CBioseq_set::eClass_conset) ) {
                            continue;
                        }
                    }
                }
            }
            x_NextEntry();
        }
        else {
            found_na = false; // no more skipping
            x_PushEntry(m_CurrentEntry);
        }
    }
}


void CBioseq_CI::x_Initialize(const CSeq_entry_Handle& entry)
{
    if ( !entry ) {
        NCBI_THROW(CObjMgrException, eOtherError,
                   "Can not find seq-entry to initialize bioseq iterator");
    }
    x_PushEntry(entry);
    x_Settle();
}


CBioseq_CI& CBioseq_CI::operator++ (void)
{
    x_NextEntry();
    x_Settle();
    return *this;
}


CBioseq_CI::CBioseq_CI(void)
    : m_Filter(CSeq_inst::eMol_not_set),
      m_Level(eLevel_All),
      m_InParts(0)
{
}


CBioseq_CI::CBioseq_CI(const CBioseq_CI& bioseq_ci)
{
    *this = bioseq_ci;
}


CBioseq_CI::~CBioseq_CI(void)
{
}


CBioseq_CI::CBioseq_CI(const CSeq_entry_Handle& entry,
                       CSeq_inst::EMol filter,
                       EBioseqLevelFlag level)
    : m_Scope(&entry.GetScope()),
      m_Filter(filter),
      m_Level(level),
      m_InParts(0)
{
    x_Initialize(entry);
}


CBioseq_CI::CBioseq_CI(const CBioseq_set_Handle& bioseq_set,
                       CSeq_inst::EMol filter,
                       EBioseqLevelFlag level)
    : m_Scope(&bioseq_set.GetScope()),
      m_Filter(filter),
      m_Level(level),
      m_InParts(0)
{
    x_Initialize(bioseq_set.GetParentEntry());
}


CBioseq_CI::CBioseq_CI(CScope& scope, const CSeq_entry& entry,
                       CSeq_inst::EMol filter,
                       EBioseqLevelFlag level)
    : m_Scope(&scope),
      m_Filter(filter),
      m_Level(level),
      m_InParts(0)
{
    x_Initialize(scope.GetSeq_entryHandle(entry));
}


CBioseq_CI& CBioseq_CI::operator= (const CBioseq_CI& bioseq_ci)
{
    if ( this != &bioseq_ci ) {
        m_Scope = bioseq_ci.m_Scope;
        m_Filter = bioseq_ci.m_Filter;
        m_Level = bioseq_ci.m_Level;
        m_InParts = bioseq_ci.m_InParts;
        m_EntryStack = bioseq_ci.m_EntryStack;
        m_CurrentEntry = bioseq_ci.m_CurrentEntry;
        m_CurrentBioseq = bioseq_ci.m_CurrentBioseq;
    }
    return *this;
}


END_SCOPE(objects)
END_NCBI_SCOPE
