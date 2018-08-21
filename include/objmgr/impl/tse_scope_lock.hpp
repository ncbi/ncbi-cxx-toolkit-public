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
class CTSE_ScopeInternalLocker;
class CTSE_ScopeUserLocker;

class NCBI_XOBJMGR_EXPORT CTSE_ScopeInternalLocker : public CObjectCounterLocker
{
public:
    void Lock(CTSE_ScopeInfo* tse) const;
    void Unlock(CTSE_ScopeInfo* tse) const;
    void Relock(CTSE_ScopeInfo* tse) const
        {
            Lock(tse);
        }
    void TransferLock(const CTSE_ScopeInfo* /*tse*/,
                      const CTSE_ScopeInternalLocker& /*old_locker*/) const
        {
        }
};


class NCBI_XOBJMGR_EXPORT CTSE_ScopeUserLocker : public CObjectCounterLocker
{
public:
    void Lock(CTSE_ScopeInfo* tse) const;
    void Unlock(CTSE_ScopeInfo* tse) const;
    void Relock(CTSE_ScopeInfo* tse) const
        {
            Lock(tse);
        }
    void TransferLock(const CTSE_ScopeInfo* /*tse*/,
                      const CTSE_ScopeUserLocker& /*old_locker*/) const
        {
        }
};


typedef CRef<CTSE_ScopeInfo, CTSE_ScopeInternalLocker> CTSE_ScopeInternalLock;
typedef CRef<CTSE_ScopeInfo, CTSE_ScopeUserLocker> CTSE_ScopeUserLock;


END_SCOPE(objects)
END_NCBI_SCOPE

#endif//OBJECTS_OBJMGR_IMPL___TSE_SCOPE_LOCK__HPP
