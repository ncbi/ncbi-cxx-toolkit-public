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

#include <objmgr/seqdesc_ci.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objmgr/impl/annot_object.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//
//  CSeqdesc_CI::
//


static const list< CRef<CSeqdesc> > kEmptySeqdescList;
static const list< CRef<CSeqdesc> >::const_iterator kDummyInner
    = kEmptySeqdescList.end();


CSeqdesc_CI::CSeqdesc_CI(void)
    : m_Outer(),
      m_Inner(kDummyInner),
      m_InnerEnd(kDummyInner),
      m_Current(NULL)
{}


CSeqdesc_CI::CSeqdesc_CI(const CSeq_descr_CI& desc_it,
                         CSeqdesc::E_Choice choice)
    : m_Outer(desc_it),
      m_Current(NULL)
{
    if (choice != CSeqdesc::e_not_set) {
        m_Choice.set(choice);
    }
    else {
        m_Choice.set();
    }
    if ( !m_Outer )
        return;
    m_Inner = desc_it->Get().begin();
    m_InnerEnd = desc_it->Get().end();
    // Advance to the first relevant Seqdesc, if any.
    x_Next();
}


CSeqdesc_CI::CSeqdesc_CI(const CBioseq_Handle& handle,
                         CSeqdesc::E_Choice choice,
                         size_t search_depth)
    : m_Outer(handle, search_depth),
      m_Current(NULL)
{
    if (choice != CSeqdesc::e_not_set) {
        m_Choice.set(choice);
    }
    else {
        m_Choice.set();
    }
    if ( !m_Outer )
        return;
    m_Inner = m_Outer->Get().begin();
    m_InnerEnd = m_Outer->Get().end();
    // Advance to the first relevant Seqdesc, if any.
    x_Next();
}


CSeqdesc_CI::CSeqdesc_CI(const CSeq_entry_Handle& entry,
                         CSeqdesc::E_Choice choice,
                         size_t search_depth)
    : m_Outer(entry, search_depth),
      m_Current(NULL)
{
    if (choice != CSeqdesc::e_not_set) {
        m_Choice.set(choice);
    }
    else {
        m_Choice.set();
    }
    if ( !m_Outer )
        return;
    m_Inner = m_Outer->Get().begin();
    m_InnerEnd = m_Outer->Get().end();
    // Advance to the first relevant Seqdesc, if any.
    x_Next();
}


CSeqdesc_CI::CSeqdesc_CI(const CBioseq_Handle& handle,
                         const TDescChoices& choices,
                         size_t search_depth)
    : m_Outer(handle, search_depth),
      m_Current(NULL)
{
    ITERATE(TDescChoices, it, choices) {
        m_Choice.set(*it);
    }
    if ( !m_Outer )
        return;
    m_Inner = m_Outer->Get().begin();
    m_InnerEnd = m_Outer->Get().end();
    // Advance to the first relevant Seqdesc, if any.
    x_Next();
}


CSeqdesc_CI::CSeqdesc_CI(const CSeq_entry_Handle& entry,
                         const TDescChoices& choices,
                         size_t search_depth)
    : m_Outer(entry, search_depth),
      m_Current(NULL)
{
    ITERATE(TDescChoices, it, choices) {
        m_Choice.set(*it);
    }
    if ( !m_Outer )
        return;
    m_Inner = m_Outer->Get().begin();
    m_InnerEnd = m_Outer->Get().end();
    // Advance to the first relevant Seqdesc, if any.
    x_Next();
}


CSeqdesc_CI::CSeqdesc_CI(const CSeqdesc_CI& iter)
    : m_Outer(iter.m_Outer),
      m_Inner(iter.m_Inner),
      m_InnerEnd(iter.m_InnerEnd),
      m_Current(iter.m_Current),
      m_Choice(iter.m_Choice)
{
}


CSeqdesc_CI::~CSeqdesc_CI(void)
{
    return;
}


CSeqdesc_CI& CSeqdesc_CI::operator= (const CSeqdesc_CI& iter)
{
    if (this != &iter) {
        m_Current  = iter.m_Current;
        m_Outer    = iter.m_Outer;
        m_Inner    = iter.m_Inner;
        m_InnerEnd = iter.m_InnerEnd;
        m_Choice   = iter.m_Choice;
    }
    return *this;
}


void CSeqdesc_CI::x_Next(void)
{
    while (m_Outer) {
        while (m_Inner != m_InnerEnd) {
            if ( m_Choice[(*m_Inner)->Which()] ) {
                m_Current = *m_Inner;
                ++m_Inner;
                return;
            } else {
                ++m_Inner;
            }
        }
        if (++m_Outer) {
            m_Inner    = m_Outer->Get().begin();
            m_InnerEnd = m_Outer->Get().end();
        }
    }
    m_Current = NULL;
}


CSeqdesc_CI& CSeqdesc_CI::operator++(void)
{
    x_Next();
    return *this;
}


CSeqdesc_CI::operator bool(void) const
{
    return bool(m_Current)  &&  (m_Inner != m_InnerEnd  ||  m_Outer);
}


const CSeqdesc& CSeqdesc_CI::operator*(void) const
{
    _ASSERT(m_Current);
    return *m_Current;
}


const CSeqdesc* CSeqdesc_CI::operator->(void) const
{
    _ASSERT(m_Current);
    return m_Current;
}


CSeq_entry_Handle CSeqdesc_CI::GetSeq_entry_Handle(void) const
{
    return m_Outer ? m_Outer.GetSeq_entry_Handle() :
        CSeq_entry_Handle();
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.12  2004/04/28 14:14:39  grichenk
* Added filtering by several seqdesc types.
*
* Revision 1.11  2004/03/16 15:47:28  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.10  2004/02/09 19:18:54  grichenk
* Renamed CDesc_CI to CSeq_descr_CI. Redesigned CSeq_descr_CI
* and CSeqdesc_CI to avoid using data directly.
*
* Revision 1.9  2003/06/02 16:06:38  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.8  2003/03/14 19:10:41  grichenk
* + SAnnotSelector::EIdResolving; fixed operator=() for several classes
*
* Revision 1.7  2003/03/13 13:02:36  dicuccio
* Added #include for annot_object.hpp - fixes obtuse error for Win32 builds
*
* Revision 1.6  2002/12/05 19:28:32  grichenk
* Prohibited postfix operator ++()
*
* Revision 1.5  2002/07/08 20:51:02  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.4  2002/05/09 14:20:54  grichenk
* Added checking of m_Current validity
*
* Revision 1.3  2002/05/06 03:28:48  vakatov
* OM/OM1 renaming
*
* Revision 1.2  2002/02/21 19:27:06  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.1  2002/01/11 19:06:25  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/
