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

class CHeapScope
{
public:
    CHeapScope(void)
        {
        }
    explicit CHeapScope(CScope& scope)
        {
            Set(&scope);
        }
    explicit CHeapScope(CScope* scope)
        {
            Set(scope);
        }

    // check is scope is not null
    bool IsSet(void) const
        {
            return m_Scope.NotEmpty();
        }
    bool IsNull(void) const
        {
            return !m_Scope;
        }

    DECLARE_OPERATOR_BOOL_REF(m_Scope);

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
    NCBI_XOBJMGR_EXPORT CScope& GetScope(void) const;
    NCBI_XOBJMGR_EXPORT CScope* GetScopeOrNull(void) const;
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
    NCBI_XOBJMGR_EXPORT CScope_Impl* GetImpl(void) const;

    operator CScope_Impl*(void) const
        {
            return GetImpl();
        }
    CScope_Impl* operator->(void) const
        {
            return GetImpl();
        }


    NCBI_XOBJMGR_EXPORT void Set(CScope* scope);
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


END_SCOPE(objects)
END_NCBI_SCOPE

#endif//OBJMGR_IMPL_HEAP_SCOPE__HPP
