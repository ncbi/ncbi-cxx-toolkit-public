#ifndef TSE_CI__HPP
#define TSE_CI__HPP

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
*   Iterates over TSEs from a given scope. To be used together with
*   CBioseq_CI and CFeat_CI to get all objects from a scope.
*
*/


#include <objects/objmgr/scope.hpp>
#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class NCBI_XOBJMGR_EXPORT CTSE_CI
{
public:
    // 'ctors
    CTSE_CI(void);
    // Iterate over TSEs taken from the scope.
    CTSE_CI(CScope& scope);
    CTSE_CI(const CTSE_CI& tse_ci);
    ~CTSE_CI(void);

    CTSE_CI& operator= (const CTSE_CI& tse_ci);
    CTSE_CI& operator++ (void);
    operator bool (void) const;

    const CSeq_entry& operator* (void) const;
    const CSeq_entry* operator-> (void) const;

private:
    typedef set<TTSE_Lock>          TTSE_LockSet;
    typedef TTSE_LockSet::const_iterator TTSEIterator;

    CScope*       m_Scope;
    TTSE_LockSet  m_Entries;
    TTSEIterator  m_Current;
};


inline
CTSE_CI::CTSE_CI(void)
    : m_Scope(0)
{
    m_Current = m_Entries.end();
}

inline
CTSE_CI::CTSE_CI(CScope& scope)
    : m_Scope(&scope)
{
    ITERATE (CScope::TRequestHistory, it, m_Scope->x_GetHistory()) {
        m_Entries.insert(*it);
    }
    m_Current = m_Entries.begin();
}

inline
CTSE_CI::CTSE_CI(const CTSE_CI& tse_ci)
{
    *this = tse_ci;
}

inline
CTSE_CI::~CTSE_CI(void)
{
}

inline
CTSE_CI& CTSE_CI::operator= (const CTSE_CI& tse_ci)
{
    if (this != &tse_ci) {
        m_Scope = tse_ci.m_Scope;
        ITERATE (TTSE_LockSet, it, tse_ci.m_Entries) {
            m_Entries.insert(*it);
        }
        if (tse_ci) {
            m_Current = m_Entries.find(*tse_ci.m_Current);
        }
        else {
            m_Current = m_Entries.end();
        }
    }
    return *this;
}

inline
CTSE_CI& CTSE_CI::operator++ (void)
{
    if ( m_Scope  &&  m_Current != m_Entries.end() ) {
        m_Current++;
    }
    return *this;
}

inline
CTSE_CI::operator bool (void) const
{
    return m_Scope  &&  m_Current != m_Entries.end();
}

inline
const CSeq_entry& CTSE_CI::operator* (void) const
{
    return m_Scope->x_GetTSEFromInfo(*m_Current);
}

inline
const CSeq_entry* CTSE_CI::operator-> (void) const
{
    return &m_Scope->x_GetTSEFromInfo(*m_Current);
}



END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2003/04/29 19:51:12  vasilche
* Fixed interaction of Data Loader garbage collector and TSE locking mechanism.
* Made some typedefs more consistent.
*
* Revision 1.1  2003/03/24 21:26:02  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#endif  // TSE_CI__HPP
