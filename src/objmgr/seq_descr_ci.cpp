
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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2002/01/16 16:25:57  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:18  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objects/objmgr1/desc_ci.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CDesc_CI::CDesc_CI(void)
    : m_NextEntry(0), m_Current(0)
{
    return;
}


CDesc_CI::CDesc_CI(const CBioseqHandle& handle)
    : m_Handle(handle)
{
    const CBioseq* seq = &(handle.GetBioseq());
    m_NextEntry = seq->GetParentEntry();
    if ( seq->IsSetDescr() ) {
        m_Current = &seq->GetDescr();
    }
    else {
        m_NextEntry = m_NextEntry->GetParentEntry();
        x_Walk(); // Skip entries without descriptions
        if ( m_NextEntry ) {
            m_Current = &m_NextEntry->GetSet().GetDescr();
        }
        else {
            m_Current = 0;
        }
    }
}


CDesc_CI::CDesc_CI(const CDesc_CI& iter)
    : m_Handle(iter.m_Handle),
      m_NextEntry(iter.m_NextEntry),
      m_Current(iter.m_Current)
{
    return;
}


CDesc_CI::~CDesc_CI(void)
{
    return;
}


CDesc_CI& CDesc_CI::operator= (const CDesc_CI& iter)
{
    m_Handle = iter.m_Handle;
    m_NextEntry = iter.m_NextEntry;
    m_Current = iter.m_Current;
    return *this;
}


CDesc_CI& CDesc_CI::operator++(void)
{
    if ( !m_NextEntry )
        return *this;
    m_NextEntry = m_NextEntry->GetParentEntry();
    x_Walk(); // Skip entries without descriptions
    if ( m_NextEntry ) {
        m_Current = &m_NextEntry->GetSet().GetDescr();
    }
    else {
        m_Current = 0;
    }
    return *this;
}


CDesc_CI::operator bool (void) const
{
    return m_Current;
}


const CSeq_descr& CDesc_CI::operator* (void) const
{
    _ASSERT(m_Current);
    return *m_Current;
}


const CSeq_descr* CDesc_CI::operator-> (void) const
{
    _ASSERT(m_Current);
    return m_Current;
}


void CDesc_CI::x_Walk(void)
{
    if ( !m_NextEntry )
        return;
    _ASSERT( m_NextEntry->IsSet() );
    // Find an entry with seq-descr member set
    while (m_NextEntry.GetPointer()  &&
           !m_NextEntry->GetSet().IsSetDescr()) {
        m_NextEntry = m_NextEntry->GetParentEntry();
    }
}

END_SCOPE(objects)
END_NCBI_SCOPE
