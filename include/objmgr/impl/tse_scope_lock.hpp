#ifndef OBJECTS_OBJMGR_IMPL___TSE_SCOPE_LOCK__HPP
#define OBJECTS_OBJMGR_IMPL___TSE_SCOPE_LOCK__HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*   CTSE_Scope*Lock -- classes to lock scope's TSE structures
*
*/

#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CTSE_Lock;
class CTSE_ScopeInfo;

class CTSE_ScopeInternalLock
{
public:
    typedef CTSE_ScopeInfo TObject;

    CTSE_ScopeInternalLock(void)
        {
        }
    CTSE_ScopeInternalLock(TObject& object)
        {
            x_Lock(object);
        }
    ~CTSE_ScopeInternalLock(void)
        {
            Reset();
        }

    CTSE_ScopeInternalLock(const CTSE_ScopeInternalLock& src)
        {
            x_Relock(src);
        }
    CTSE_ScopeInternalLock& operator=(const CTSE_ScopeInternalLock& src)
        {
            if ( *this != src ) {
                x_Relock(src);
            }
            return *this;
        }

    DECLARE_OPERATOR_BOOL_REF(m_Object);

    TObject& operator*(void) const
        {
            return const_cast<TObject&>
                (reinterpret_cast<const TObject&>(*m_Object));
        }
    TObject* operator->(void) const
        {
            return &**this;
        }

    bool operator==(const CTSE_ScopeInternalLock& lock) const
        {
            return m_Object == lock.m_Object;
        }
    bool operator!=(const CTSE_ScopeInternalLock& lock) const
        {
            return m_Object != lock.m_Object;
        }
    bool operator<(const CTSE_ScopeInternalLock& lock) const
        {
            return m_Object < lock.m_Object;
        }

    void Reset(void)
        {
            if ( *this ) {
                x_Unlock();
            }
        }

private:
    void x_Lock(TObject& object);
    void x_Relock(const CTSE_ScopeInternalLock& src);
    void x_Unlock(void);
    
    CRef<CObject> m_Object;
};


class CTSE_ScopeUserLock
{
public:
    typedef CTSE_ScopeInfo TObject;

    CTSE_ScopeUserLock(void)
        {
        }
    CTSE_ScopeUserLock(TObject& object)
        {
            x_Lock(object);
        }
    CTSE_ScopeUserLock(TObject& object, const CTSE_Lock& tse_lock)
        {
            x_Lock(object, tse_lock);
        }
    ~CTSE_ScopeUserLock(void)
        {
            Reset();
        }

    CTSE_ScopeUserLock(const CTSE_ScopeUserLock& src)
        {
            x_Relock(src);
        }
    CTSE_ScopeUserLock& operator=(const CTSE_ScopeUserLock& src)
        {
            if ( *this != src ) {
                x_Relock(src);
            }
            return *this;
        }

    DECLARE_OPERATOR_BOOL_REF(m_Object);

    TObject& operator*(void) const
        {
            return const_cast<TObject&>
                (reinterpret_cast<const TObject&>(*m_Object));
        }
    TObject* operator->(void) const
        {
            return &**this;
        }
    const CTSE_Lock& GetTSE_Lock(void) const;

    bool operator==(const CTSE_ScopeUserLock& lock) const
        {
            return m_Object == lock.m_Object;
        }
    bool operator!=(const CTSE_ScopeUserLock& lock) const
        {
            return m_Object != lock.m_Object;
        }
    bool operator<(const CTSE_ScopeUserLock& lock) const
        {
            return m_Object < lock.m_Object;
        }

    void Reset(void)
        {
            if ( *this ) {
                x_Unlock();
            }
        }

    void Release(void);

private:
    void x_Lock(TObject& object);
    void x_Lock(TObject& object, const CTSE_Lock& tse_lock);
    void x_Relock(const CTSE_ScopeUserLock& src);
    void x_Unlock(void);
    
    CRef<CObject> m_Object;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif//OBJECTS_OBJMGR_IMPL___TSE_SCOPE_LOCK__HPP
