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


inline
void CBioseq_CI::x_SetEntry(const CSeq_entry_Handle& entry)
{
    m_CurrentEntry = entry;
    if ( !m_EntryStack.empty()  &&  m_CurrentEntry.IsSet() ) {
        m_EntryStack.push(CSeq_entry_CI(m_CurrentEntry));
    }
}


void CBioseq_CI::x_Settle(void)
{
    m_CurrentBioseq.Reset();
    for ( ;; ) {
        if ( m_CurrentEntry  &&  m_CurrentEntry.IsSeq() ) {
            // Single bioseq
            const CBioseq_Info& seq = m_CurrentEntry.x_GetInfo().GetSeq();
            if (m_Level != eLevel_Parts  ||  m_InParts > 0) {
                if ( x_IsValidMolType(seq) ) {
                    m_CurrentBioseq = m_CurrentEntry.GetSeq();
                    return; // valid bioseq found
                }
            }
        }
        // Bioseq set or next entry in the parent set
        if ( m_EntryStack.empty() ) {
            // End
            m_CurrentEntry = CSeq_entry_Handle();
            return;
        }
        if ( m_EntryStack.top() ) {
            CSeq_entry_CI& entry_iter = m_EntryStack.top();
            CSeq_entry_CI parts_iter = m_EntryStack.top();
            CSeq_entry_Handle sub_entry = *entry_iter;
            ++entry_iter;
            if (sub_entry.IsSet() &&
                sub_entry.GetSet().GetClass() == CBioseq_set::eClass_parts) {
                if (m_Level == eLevel_Mains) {
                    m_CurrentEntry = CSeq_entry_Handle();
                    continue;
                }
                m_InParts++;
                m_EntryStack.push(parts_iter);
            }
            x_SetEntry(sub_entry);
        }
        else {
            m_EntryStack.pop();
            if ( m_EntryStack.empty() ) {
                return;
            }
            if ( m_EntryStack.top() ) {
                CSeq_entry_CI entry_iter = m_EntryStack.top();
                CSeq_entry_Handle sub_entry = *entry_iter;
                if (sub_entry.IsSet() &&
                    sub_entry.GetSet().GetClass() == CBioseq_set::eClass_parts) {
                    m_InParts--;
                    m_EntryStack.pop();
                }
            }
        }
    }
}


void CBioseq_CI::x_Initialize(const CSeq_entry_Handle& entry)
{
    if ( !entry ) {
        NCBI_THROW(CObjMgrException, eOtherError,
                   "Can not find seq-entry to initialize bioseq iterator");
    }
    x_SetEntry(entry);
    if (  m_CurrentEntry.IsSet() ) {
        m_EntryStack.push(CSeq_entry_CI(m_CurrentEntry));
    }
    x_Settle();
}


CBioseq_CI& CBioseq_CI::operator++ (void)
{
    m_CurrentEntry = CSeq_entry_Handle();
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
    x_Initialize(m_Scope->GetSeq_entryHandle(entry));
}


CBioseq_CI& CBioseq_CI::operator= (const CBioseq_CI& bioseq_ci)
{
    if (this != &bioseq_ci) {
        m_Scope = bioseq_ci.m_Scope;
        m_Filter = bioseq_ci.m_Filter;
        m_Level = bioseq_ci.m_Level;
        m_InParts = bioseq_ci.m_InParts;
        if ( bioseq_ci ) {
            m_EntryStack = bioseq_ci.m_EntryStack;
            m_CurrentEntry = bioseq_ci.m_CurrentEntry;
        }
        else {
            m_CurrentEntry.Reset();
            m_CurrentBioseq.Reset();
            while ( !m_EntryStack.empty() ) {
                m_EntryStack.pop();
            }
        }
    }
    return *this;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.7  2005/04/07 16:30:42  vasilche
* Inlined handles' constructors and destructors.
* Optimized handles' assignment operators.
*
* Revision 1.6  2005/03/18 16:14:26  grichenk
* Iterator with CSeq_inst::eMol_na includes both dna and rna.
*
* Revision 1.5  2005/01/18 14:58:58  grichenk
* Added constructor accepting CBioseq_set_Handle
*
* Revision 1.4  2005/01/10 19:06:27  grichenk
* Redesigned CBioseq_CI not to collect all bioseqs in constructor.
*
* Revision 1.3  2004/05/21 21:42:12  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.2  2004/03/16 15:47:27  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.1  2003/09/30 16:22:02  vasilche
* Updated internal object manager classes to be able to load ID2 data.
* SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
* Scope caches results of requests for data to data loaders.
* Optimized CSeq_id_Handle for gis.
* Optimized bioseq lookup in scope.
* Reduced object allocations in annotation iterators.
* CScope is allowed to be destroyed before other objects using this scope are
* deleted (feature iterators, bioseq handles etc).
* Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
* Added 'adaptive' option to objmgr_demo application.
*
* ===========================================================================
*/
