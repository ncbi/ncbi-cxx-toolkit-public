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

#include <objects/objmgr1/seqdesc_ci.hpp>
#include <objects/seq/Seq_descr.hpp>

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
    : m_Outer(), m_Inner(kDummyInner), m_InnerEnd(kDummyInner), m_Current(NULL)
{}


CSeqdesc_CI::CSeqdesc_CI(const CDesc_CI& desc_it, CSeqdesc::E_Choice choice)
    : m_Outer(desc_it), m_Inner(desc_it->Get().begin()),
      m_InnerEnd(desc_it->Get().end()), m_Current(NULL), m_Choice(choice)
{
    // Advance to the first relevant Seqdesc, if any.
    ++*this;
}


CSeqdesc_CI::CSeqdesc_CI(const CSeqdesc_CI& iter)
    : m_Outer(iter.m_Outer), m_Inner(iter.m_Inner),
      m_InnerEnd(iter.m_InnerEnd), m_Current(iter.m_Current),
      m_Choice(iter.m_Choice)
{
}


CSeqdesc_CI::~CSeqdesc_CI(void)
{
    return;
}


CSeqdesc_CI& CSeqdesc_CI::operator= (const CSeqdesc_CI& iter)
{
    m_Current  = iter.m_Current;
    m_Outer    = iter.m_Outer;
    m_Inner    = iter.m_Inner;
    m_InnerEnd = iter.m_InnerEnd;
    m_Choice   = iter.m_Choice;
    return *this;
}


CSeqdesc_CI& CSeqdesc_CI::operator++(void) // prefix
{
    while (m_Outer) {
        while (m_Inner != m_InnerEnd) {
            if ((*m_Inner)->Which() == m_Choice
                ||  m_Choice == CSeqdesc::e_not_set) {
                m_Current = *m_Inner;
                ++m_Inner;
                return *this;
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
    return *this;
}


CSeqdesc_CI CSeqdesc_CI::operator++(int) // postfix
{
    CSeqdesc_CI old_value = *this;
    ++*this;
    return old_value;
}


CSeqdesc_CI::operator bool(void) const
{
    return m_Inner != m_InnerEnd  ||  m_Outer;
}


const CSeqdesc& CSeqdesc_CI::operator*(void) const
{
    return *m_Current;
}


const CSeqdesc* CSeqdesc_CI::operator->(void) const
{
    return m_Current;
}


END_SCOPE(objects)
END_NCBI_SCOPE
