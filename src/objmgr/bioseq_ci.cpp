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
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/impl/scope_impl.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CBioseq_CI::CBioseq_CI(void)
{
    m_Current = m_Handles.end();
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
    : m_Scope(&entry.GetScope())
{
    m_Scope->x_PopulateBioseq_HandleSet(entry, m_Handles, filter, level);
    m_Current = m_Handles.begin();
}


CBioseq_CI::CBioseq_CI(CScope& scope, const CSeq_entry& entry,
                       CSeq_inst::EMol filter,
                       EBioseqLevelFlag level)
    : m_Scope(&scope)
{
    m_Scope->x_PopulateBioseq_HandleSet(m_Scope->GetSeq_entryHandle(entry),
                                        m_Handles, filter, level);
    m_Current = m_Handles.begin();
}


CBioseq_CI& CBioseq_CI::operator= (const CBioseq_CI& bioseq_ci)
{
    if (this != &bioseq_ci) {
        m_Scope = bioseq_ci.m_Scope;
        m_Handles = bioseq_ci.m_Handles;
        if ( bioseq_ci ) {
            m_Current = m_Handles.begin() +
                (bioseq_ci.m_Current - bioseq_ci.m_Handles.begin());
        }
        else {
            m_Current = m_Handles.end();
        }
    }
    return *this;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
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
