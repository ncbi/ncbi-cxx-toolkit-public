#ifndef OBJECTS_OBJMGR_IMPL___TSE_LOCK__HPP
#define OBJECTS_OBJMGR_IMPL___TSE_LOCK__HPP

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
*   CTSE_Lock -- class to lock TSEs from garbage collector
*
*/

#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CTSE_Info;
class CTSE_LoadLock;

class NCBI_XOBJMGR_EXPORT CTSE_Lock
{
public:
    CTSE_Lock(void)
        {
        }
    CTSE_Lock(const CTSE_Lock& lock)
        {
            x_Assign(lock);
        }
    CTSE_Lock(const CTSE_LoadLock& load_lock)
        {
            x_Assign(load_lock);
        }
    ~CTSE_Lock(void)
        {
            Reset();
        }
    CTSE_Lock& operator=(const CTSE_Lock& lock)
        {
            if ( m_Info != lock.m_Info ) {
                Reset();
                x_Assign(lock);
            }
            return *this;
        }
    
    operator const CTSE_Info*(void) const
        {
            return GetPointerOrNull();
        }

    bool operator==(const CTSE_Lock& lock) const
        {
            return m_Info == lock.m_Info;
        }
    bool operator!=(const CTSE_Lock& lock) const
        {
            return m_Info != lock.m_Info;
        }
    bool operator<(const CTSE_Lock& lock) const
        {
            return m_Info < lock.m_Info;
        }
    
    const CTSE_Info* GetPointerOrNull(void) const
        {
            return reinterpret_cast<const CTSE_Info*>
                (m_Info.GetPointerOrNull());
        }
    const CTSE_Info* GetNonNullPointer(void) const
        {
            return reinterpret_cast<const CTSE_Info*>
                (m_Info.GetNonNullPointer());
        }
    const CTSE_Info& operator*(void) const
        {
            return *GetNonNullPointer();
        }
    const CTSE_Info* operator->(void) const
        {
            return GetNonNullPointer();
        }
    
    void Reset(void)
        {
            if ( *this ) {
                x_Unlock();
            }
        }
    
protected:
    // TSE locks can be aquired only through CDataSource.
    friend class CDataSource;

    // returns true if first lock is aquired
    bool Lock(const CTSE_Info* info)
        {
            if ( GetPointerOrNull() != info ) {
                Reset();
                if ( info ) {
                    return x_Lock(info);
                }
            }
            return false;
        }

    void x_Assign(const CTSE_Lock& lock)
        {
            const CTSE_Info* info = lock.GetPointerOrNull();
            if ( info ) {
                x_Relock(info);
            }
        }
    void x_Assign(const CTSE_LoadLock& load_lock);

    void x_Unlock(void);
    bool x_Lock(const CTSE_Info* info);
    void x_Relock(const CTSE_Info* info);

private:
    // m_Info is declared as CRef<CObject> to avoid inclusion of tse_info.hpp
    CConstRef<CObject>  m_Info;
};


class NCBI_XOBJMGR_EXPORT CTSE_LockSet
{
public:

    bool IsEmpty(void) const;

    void Clear(void);

    CTSE_Lock FindLock(const CTSE_Info* info) const;

    bool AddLock(const CTSE_Lock& lock);
    bool RemoveLock(const CTSE_Lock& lock);
    bool RemoveLock(const CTSE_Info* info);

    set<CTSE_Lock> GetBestTSEs(void) const;
    static bool IsBetter(const CTSE_Info& tse1, const CTSE_Info& tse2);

private:
    typedef map<const CTSE_Info*, CTSE_Lock>  TTSE_LockSet;

    TTSE_LockSet m_TSE_LockSet;
};


/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////

END_SCOPE(objects)
END_NCBI_SCOPE

#endif//OBJECTS_OBJMGR_IMPL___TSE_LOCK__HPP
