#ifndef OBJMGR_IMPL_HEAP_SCOPE__HPP
#define OBJMGR_IMPL_HEAP_SCOPE__HPP

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
* Authors:
*           Eugene Vasilchenko
*
* File Description:
*           CHeapScope is internal holder of CScope_Impl object
*
*/

#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// objmgr
class CScope;
class CHeapScope;
class CScope_Impl;

/////////////////////////////////////////////////////////////////////////////
// CHeapScope
//    Holds reference on heap scope object
//    used internally in interface classes (iterators, handles etc)
/////////////////////////////////////////////////////////////////////////////

class NCBI_XOBJMGR_EXPORT CHeapScope
{
public:
    CHeapScope(void)
        {
        }
    explicit CHeapScope(CScope& scope);
    explicit CHeapScope(CScope* scope);

    // check is scope is not null
    operator bool(void) const
        {
            return m_Scope;
        }
    bool operator!(void) const
        {
            return !m_Scope;
        }

    bool operator==(const CHeapScope& scope) const
        {
            return m_Scope == scope.m_Scope;
        }
    bool operator!=(const CHeapScope& scope) const
        {
            return m_Scope != scope.m_Scope;
        }
    bool operator<(const CHeapScope& scope) const
        {
            return m_Scope < scope.m_Scope;
        }

    // scope getters
    CScope& GetScope(void) const;
    operator CScope&(void) const
        {
            return GetScope();
        }
    operator CScope*(void) const
        {
            return &GetScope();
        }
    CScope& operator*(void) const
        {
            return GetScope();
        }

    // scope impl getters
    CScope_Impl* GetImpl(void) const;

    operator CScope_Impl*(void) const
        {
            return GetImpl();
        }
    CScope_Impl* operator->(void) const
        {
            return GetImpl();
        }


    void Set(CScope* scope);
    void Reset(void)
        {
            m_Scope.Reset();
        }

private:
    // the reference has to be CObject* to avoid circular header dep.
    CRef<CObject> m_Scope;
};


/////////////////////////////////////////////////////////////////////////////
// inline methods
/////////////////////////////////////////////////////////////////////////////


inline
CHeapScope::CHeapScope(CScope& scope)
{
    Set(&scope);
}


inline
CHeapScope::CHeapScope(CScope* scope)
{
    Set(scope);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2004/03/16 15:47:26  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
*
* ===========================================================================
*/

#endif//OBJMGR_IMPL_HEAP_SCOPE__HPP
