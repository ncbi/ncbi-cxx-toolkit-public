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
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.9  2002/04/11 12:07:30  grichenk
* Redesigned CAnnotTypes_CI to resolve segmented sequences correctly.
*
* Revision 1.8  2002/03/19 19:16:28  gouriano
* added const qualifier to GetTitle and GetSeqVector
*
* Revision 1.7  2002/03/15 18:10:07  grichenk
* Removed CRef<CSeq_id> from CSeq_id_Handle, added
* key to seq-id map th CSeq_id_Mapper
*
* Revision 1.6  2002/03/04 15:08:44  grichenk
* Improved CTSE_Info locks
*
* Revision 1.5  2002/02/21 19:27:05  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.4  2002/01/28 19:44:49  gouriano
* changed the interface of BioseqHandle: two functions moved from Scope
*
* Revision 1.3  2002/01/23 21:59:31  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.2  2002/01/16 16:25:56  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:17  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/

#include <objects/objmgr1/bioseq_handle.hpp>
#include "data_source.hpp"
#include "tse_info.hpp"
#include <objects/objmgr1/seq_vector.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CBioseq_Handle::~CBioseq_Handle(void)
{
    if ( m_TSE )
        m_TSE->Unlock();
}


CBioseq_Handle::CBioseq_Handle(const CBioseq_Handle& h)
    : m_Value(h.m_Value),
      m_Scope(h.m_Scope),
      m_DataSource(h.m_DataSource),
      m_Entry(h.m_Entry),
      m_TSE(h.m_TSE)
{
    if ( m_TSE )
        m_TSE->Lock();
}


CBioseq_Handle& CBioseq_Handle::operator= (const CBioseq_Handle& h)
{
    m_Value = h.m_Value;
    m_Scope = h.m_Scope;
    m_DataSource = h.m_DataSource;
    m_Entry = h.m_Entry;
    CMutexGuard guard(CDataSource::sm_DataSource_Mutex);
    if ( m_TSE )
        m_TSE->Unlock();
    m_TSE = h.m_TSE;
    if ( m_TSE )
        m_TSE->Lock();
    return *this;
}


const CSeq_id* CBioseq_Handle::GetSeqId(void) const
{
    if (!m_Value) return 0;
    return m_Value.x_GetSeqId();
}


const CBioseq& CBioseq_Handle::GetBioseq(void) const
{
    return x_GetDataSource().GetBioseq(*this);
}


const CSeq_entry& CBioseq_Handle::GetTopLevelSeqEntry(void) const
{
    // Can not use m_TSE->m_TSE since the handle may be unresolved yet
    return x_GetDataSource().GetTSE(*this);
}

CBioseq_Handle::TBioseqCore CBioseq_Handle::GetBioseqCore(void) const
{
    return x_GetDataSource().GetBioseqCore(*this);
}


const CSeqMap& CBioseq_Handle::GetSeqMap(void) const
{
    return x_GetDataSource().GetSeqMap(*this);
}


CSeqVector CBioseq_Handle::GetSeqVector(bool plus_strand) const
{
    return CSeqVector(*this, plus_strand, *m_Scope);
}


void CBioseq_Handle::x_ResolveTo(
    CScope& scope, CDataSource& datasource,
    CSeq_entry& entry, CTSE_Info& tse)
{
    m_Scope = &scope;
    m_DataSource = &datasource;
    m_Entry = &entry;
    CMutexGuard guard(CDataSource::sm_DataSource_Mutex);
    if ( m_TSE )
        m_TSE->Unlock();
    m_TSE = &tse;
    m_TSE->Lock();
}


const CSeqMap& CBioseq_Handle::GetResolvedSeqMap(void) const
{
    return x_GetDataSource().GetResolvedSeqMap(*this);
}



END_SCOPE(objects)
END_NCBI_SCOPE
