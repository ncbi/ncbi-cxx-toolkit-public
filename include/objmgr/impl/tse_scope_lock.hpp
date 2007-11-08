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
class CTSE_ScopeLocker;
class CTSE_ScopeInternalLocker;
class CTSE_ScopeUserLocker;

class CTSE_ScopeInfo_Base : public CObject
{
protected:
    mutable CAtomicCounter m_TSE_LockCounter;

    void x_LockTSE(void);
    void x_InternalUnlockTSE(void);
    void x_UserUnlockTSE(void);

    friend class CTSE_ScopeLocker;
};


class CTSE_ScopeLocker : protected CObjectCounterLocker
{
public:
    void Lock(CTSE_ScopeInfo* tse) const
        {
            CTSE_ScopeInfo_Base* base =
                reinterpret_cast<CTSE_ScopeInfo_Base*>(tse);
            CObjectCounterLocker::Lock(base);
            base->m_TSE_LockCounter.Add(1);
            base->x_LockTSE();
        }
    void InternalUnlock(CTSE_ScopeInfo* tse) const
        {
            CTSE_ScopeInfo_Base* base =
                reinterpret_cast<CTSE_ScopeInfo_Base*>(tse);
            if ( base->m_TSE_LockCounter.Add(-1) == 0 ) {
                base->x_InternalUnlockTSE();
            }
            CObjectCounterLocker::Unlock(base);
        }
    void UserUnlock(CTSE_ScopeInfo* tse) const
        {
            CTSE_ScopeInfo_Base* base =
                reinterpret_cast<CTSE_ScopeInfo_Base*>(tse);
            if ( base->m_TSE_LockCounter.Add(-1) == 0 ) {
                base->x_UserUnlockTSE();
            }
            CObjectCounterLocker::Unlock(base);
        }
    void Relock(CTSE_ScopeInfo* tse) const
        {
            Lock(tse);
        }
};


class CTSE_ScopeInternalLocker : public CTSE_ScopeLocker
{
public:
    void Unlock(CTSE_ScopeInfo* tse) const
        {
            InternalUnlock(tse);
        }
};


class CTSE_ScopeUserLocker : public CTSE_ScopeLocker
{
public:
    void Unlock(CTSE_ScopeInfo* tse) const
        {
            UserUnlock(tse);
        }
};


typedef CRef<CTSE_ScopeInfo, CTSE_ScopeInternalLocker> CTSE_ScopeInternalLock;
typedef CRef<CTSE_ScopeInfo, CTSE_ScopeUserLocker> CTSE_ScopeUserLock;


END_SCOPE(objects)
END_NCBI_SCOPE

#endif//OBJECTS_OBJMGR_IMPL___TSE_SCOPE_LOCK__HPP
