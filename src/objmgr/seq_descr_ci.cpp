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
* Author: Aleksey Grichenko
*
* File Description:
*   Object manager iterators
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/seq_descr_ci.hpp>
#include <objmgr/bioseq_handle.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CSeq_descr_CI::CSeq_descr_CI(void)
{
    return;
}


CSeq_descr_CI::CSeq_descr_CI(const CBioseq_Handle& handle,
                             size_t search_depth)
    : m_CurrentEntry(handle.GetParentEntry()),
      m_ParentLimit(search_depth-1)
{
    x_Settle(); // Skip entries without descriptions
}


CSeq_descr_CI::CSeq_descr_CI(const CSeq_entry_Handle& entry,
                             size_t search_depth)
    : m_CurrentEntry(entry),
      m_ParentLimit(search_depth-1)
{
    x_Settle(); // Skip entries without descriptions
    _ASSERT(!m_CurrentEntry || m_CurrentEntry.IsSetDescr());
}

    
CSeq_descr_CI::CSeq_descr_CI(const CSeq_descr_CI& iter)
    : m_CurrentEntry(iter.m_CurrentEntry),
      m_ParentLimit(iter.m_ParentLimit)
{
    _ASSERT(!m_CurrentEntry || m_CurrentEntry.IsSetDescr());
}


CSeq_descr_CI::~CSeq_descr_CI(void)
{
}


CSeq_descr_CI& CSeq_descr_CI::operator= (const CSeq_descr_CI& iter)
{
    if (this != &iter) {
        m_CurrentEntry = iter.m_CurrentEntry;
        m_ParentLimit = iter.m_ParentLimit;
    }
    _ASSERT(!m_CurrentEntry || m_CurrentEntry.IsSetDescr());
    return *this;
}


void CSeq_descr_CI::x_Next(void)
{
    x_Step();
    x_Settle();
    _ASSERT(!m_CurrentEntry || m_CurrentEntry.IsSetDescr());
}


void CSeq_descr_CI::x_Settle(void)
{
    while ( m_CurrentEntry && !m_CurrentEntry.IsSetDescr() ) {
        x_Step();
    }
    _ASSERT(!m_CurrentEntry || m_CurrentEntry.IsSetDescr());
}


void CSeq_descr_CI::x_Step(void)
{
    if ( m_CurrentEntry && m_ParentLimit > 0 ) {
        --m_ParentLimit;
        m_CurrentEntry = m_CurrentEntry.GetParentEntry();
    }
    else {
        m_CurrentEntry.Reset();
    }
}


CSeq_descr_CI& CSeq_descr_CI::operator++(void)
{
    x_Next();
    return *this;
}


const CSeq_descr& CSeq_descr_CI::operator* (void) const
{
    _ASSERT(m_CurrentEntry  &&  m_CurrentEntry.IsSetDescr());
    return m_CurrentEntry.GetDescr();
}


const CSeq_descr* CSeq_descr_CI::operator-> (void) const
{
    _ASSERT(m_CurrentEntry  &&  m_CurrentEntry.IsSetDescr());
    return &m_CurrentEntry.GetDescr();
}


CSeq_entry_Handle CSeq_descr_CI::GetSeq_entry_Handle(void) const
{
    return m_CurrentEntry;
}


END_SCOPE(objects)
END_NCBI_SCOPE
