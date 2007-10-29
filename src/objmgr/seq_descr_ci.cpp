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
#include <objmgr/bioseq_set_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/bioseq_set_info.hpp>
#include <objmgr/impl/seq_entry_info.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CSeq_descr_CI::CSeq_descr_CI(void)
{
    return;
}


CSeq_descr_CI::CSeq_descr_CI(const CBioseq_Handle& handle,
                             size_t search_depth)
    : m_CurrentBase(&handle.x_GetInfo()),
      m_CurrentSeq(handle),
      m_ParentLimit(search_depth-1)
{
    x_Settle(); // Skip entries without descriptions
}


CSeq_descr_CI::CSeq_descr_CI(const CBioseq_set_Handle& handle,
                             size_t search_depth)
    : m_CurrentBase(&handle.x_GetInfo()),
      m_CurrentSet(handle),
      m_ParentLimit(search_depth-1)
{
    x_Settle(); // Skip entries without descriptions
}


CSeq_descr_CI::CSeq_descr_CI(const CSeq_entry_Handle& entry,
                             size_t search_depth)
    : m_ParentLimit(search_depth-1)
{
    if ( entry.IsSeq() ) {
        m_CurrentSeq = entry.GetSeq();
        m_CurrentBase = &m_CurrentSeq.x_GetInfo();
    }
    else {
        m_CurrentSet = entry.GetSet();
        m_CurrentBase = &m_CurrentSet.x_GetInfo();
    }
    x_Settle(); // Skip entries without descriptions
    _ASSERT(!m_CurrentBase || m_CurrentBase->IsSetDescr());
}


CSeq_descr_CI::CSeq_descr_CI(const CSeq_descr_CI& iter)
    : m_CurrentBase(iter.m_CurrentBase),
      m_CurrentSeq(iter.m_CurrentSeq),
      m_CurrentSet(iter.m_CurrentSet),
      m_ParentLimit(iter.m_ParentLimit)
{
    _ASSERT(!m_CurrentBase || m_CurrentBase->IsSetDescr());
}


CSeq_descr_CI::~CSeq_descr_CI(void)
{
}


CSeq_descr_CI& CSeq_descr_CI::operator= (const CSeq_descr_CI& iter)
{
    if (this != &iter) {
        m_CurrentBase = iter.m_CurrentBase;
        m_CurrentSeq = iter.m_CurrentSeq;
        m_CurrentSet = iter.m_CurrentSet;
        m_ParentLimit = iter.m_ParentLimit;
    }
    _ASSERT(!m_CurrentBase || m_CurrentBase->IsSetDescr());
    return *this;
}



CSeq_entry_Handle CSeq_descr_CI::GetSeq_entry_Handle(void) const
{
    return m_CurrentSeq?
        m_CurrentSeq.GetParentEntry():
        m_CurrentSet.GetParentEntry();
}


void CSeq_descr_CI::x_Next(void)
{
    x_Step();
    x_Settle();
    _ASSERT(!m_CurrentBase || m_CurrentBase->IsSetDescr());
}


void CSeq_descr_CI::x_Settle(void)
{
    while ( m_CurrentBase && !m_CurrentBase->IsSetDescr() ) {
        x_Step();
    }
    _ASSERT(!m_CurrentBase || m_CurrentBase->IsSetDescr());
}


void CSeq_descr_CI::x_Step(void)
{
    if ( !m_CurrentBase ) {
        return;
    }
    if ( m_ParentLimit <= 0 ) {
        m_CurrentBase.Reset();
        m_CurrentSeq.Reset();
        m_CurrentSet.Reset();
        return;
    }
    --m_ParentLimit;
    if ( m_CurrentSeq ) {
        m_CurrentSet = m_CurrentSeq.GetParentBioseq_set();
    }
    else {
        m_CurrentSet = m_CurrentSet.GetParentBioseq_set();
    }
    m_CurrentSeq.Reset();
    if ( m_CurrentSet ) {
        m_CurrentBase = &m_CurrentSet.x_GetInfo();
    }
    else {
        m_CurrentBase.Reset();
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
