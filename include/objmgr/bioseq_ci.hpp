#ifndef BIOSEQ_CI__HPP
#define BIOSEQ_CI__HPP

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
*   Iterates over bioseqs from a given seq-entry and scope
*
*/


#include <objects/objmgr/scope.hpp>
#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CBioseq_CI
{
public:
    // 'ctors
    CBioseq_CI(void);
    // Iterate over bioseqs from the entry taken from the scope. Use optional
    // filter to iterate over selected bioseq types only.
    CBioseq_CI(CScope& scope, const CSeq_entry& entry,
        CSeq_inst::EMol filter = CSeq_inst::eMol_not_set);
    CBioseq_CI(const CBioseq_CI& bioseq_ci);
    ~CBioseq_CI(void);

    CBioseq_CI& operator= (const CBioseq_CI& bioseq_ci);
    CBioseq_CI& operator++ (void);
    operator bool (void) const;

    const CBioseq_Handle& operator* (void) const;
    const CBioseq_Handle* operator-> (void) const;

private:
    CBioseq_CI operator++ (int);

    typedef set<CBioseq_Handle>              TBioseqHandleSet;
    typedef TBioseqHandleSet::const_iterator THandleIterator;

    CScope*          m_Scope;
    TBioseqHandleSet m_Handles;
    THandleIterator  m_Current;
};


inline
CBioseq_CI::CBioseq_CI(void)
    : m_Scope(0)
{
    m_Current = m_Handles.end();
}

inline
CBioseq_CI::CBioseq_CI(CScope& scope, const CSeq_entry& entry, CSeq_inst::EMol filter)
    : m_Scope(&scope)
{
    m_Scope->x_PopulateBioseq_HandleSet(entry, m_Handles, filter);
    m_Current = m_Handles.begin();
}

inline
CBioseq_CI::CBioseq_CI(const CBioseq_CI& bioseq_ci)
{
    *this = bioseq_ci;
}

inline
CBioseq_CI::~CBioseq_CI(void)
{
}

inline
CBioseq_CI& CBioseq_CI::operator= (const CBioseq_CI& bioseq_ci)
{
    if (this != &bioseq_ci) {
        m_Scope = bioseq_ci.m_Scope;
        iterate (TBioseqHandleSet, it, bioseq_ci.m_Handles) {
            m_Handles.insert(*it);
        }
        if (bioseq_ci) {
            m_Current = m_Handles.find(*bioseq_ci.m_Current);
        }
        else {
            m_Current = m_Handles.end();
        }
    }
    return *this;
}

inline
CBioseq_CI& CBioseq_CI::operator++ (void)
{
    if ( m_Scope  &&  m_Current != m_Handles.end() ) {
        m_Current++;
    }
    return *this;
}

inline
CBioseq_CI::operator bool (void) const
{
    return m_Scope  &&  m_Current != m_Handles.end();
}

inline
const CBioseq_Handle& CBioseq_CI::operator* (void) const
{
    return *m_Current;
}

inline
const CBioseq_Handle* CBioseq_CI::operator-> (void) const
{
    return &(*m_Current);
}



END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  2002/12/05 19:28:29  grichenk
* Prohibited postfix operator ++()
*
* Revision 1.3  2002/10/15 13:37:28  grichenk
* Fixed inline declarations
*
* Revision 1.2  2002/10/02 17:58:21  grichenk
* Added sequence type filter to CBioseq_CI
*
* Revision 1.1  2002/09/30 20:00:48  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#endif  // BIOSEQ_CI__HPP
