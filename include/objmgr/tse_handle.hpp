#ifndef OBJMGR_TSE_HANDLE__HPP
#define OBJMGR_TSE_HANDLE__HPP

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
*           CTSE_Handle is handle to TSE
*
*/

#include <corelib/ncbiobj.hpp>
#include <objmgr/impl/heap_scope.hpp>
#include <objmgr/impl/tse_scope_lock.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CScope;
class CTSE_ScopeInfo;
class CTSE_Info;
class CTSE_Lock;
class CBioseq_Handle;
class CSeq_entry_Handle;
class CSeq_id;
class CSeq_id_Handle;
class CBlobIdKey;

/////////////////////////////////////////////////////////////////////////////
// CTSE_Handle definition
/////////////////////////////////////////////////////////////////////////////


class NCBI_XOBJMGR_EXPORT CTSE_Handle
{
public:
    /// Default constructor/destructor and assignment
    CTSE_Handle(void);
    CTSE_Handle& operator=(const CTSE_Handle& tse);

    /// Returns scope
    CScope& GetScope(void) const;

    /// State check
    DECLARE_OPERATOR_BOOL(m_TSE);

    bool operator==(const CTSE_Handle& tse) const;
    bool operator!=(const CTSE_Handle& tse) const;
    bool operator<(const CTSE_Handle& tse) const;

    /// Reset to null state
    void Reset(void);

    /// TSE info getters
    CBlobIdKey GetBlobId(void) const;

    bool Blob_IsSuppressed(void) const;
    bool Blob_IsSuppressedTemp(void) const;
    bool Blob_IsSuppressedPerm(void) const;
    bool Blob_IsDead(void) const;

    /// Get top level Seq-entry handle
    CSeq_entry_Handle GetTopLevelEntry(void) const;
    
    /// Get Bioseq handle from this TSE
    CBioseq_Handle GetBioseqHandle(const CSeq_id& id) const;
    CBioseq_Handle GetBioseqHandle(const CSeq_id_Handle& id) const;

    /// Register argument TSE as used by this TSE, so it will be
    /// released by scope only after this TSE is released.
    ///
    /// @param tse
    ///  Used TSE handle
    ///
    /// @return
    ///  True if argument TSE was successfully registered as 'used'.
    ///  False if argument TSE was not registered as 'used'.
    ///  Possible reasons:
    ///   Circular reference in 'used' tree.
    bool AddUsedTSE(const CTSE_Handle& tse) const;

protected:
    friend class CScope_Impl;

    typedef CTSE_ScopeInfo TObject;

    CTSE_Handle(TObject& object);

private:

    CHeapScope          m_Scope;
    CTSE_ScopeUserLock  m_TSE;

public: // non-public section

    TObject& x_GetScopeInfo(void) const;
    const CTSE_Info& x_GetTSE_Info(void) const;
    CScope_Impl& x_GetScopeImpl(void) const;
};


/////////////////////////////////////////////////////////////////////////////
// CTSE_Handle inline methods
/////////////////////////////////////////////////////////////////////////////


inline
CTSE_Handle::CTSE_Handle(void)
{
}


inline
CScope& CTSE_Handle::GetScope(void) const
{
    return m_Scope;
}


inline
CScope_Impl& CTSE_Handle::x_GetScopeImpl(void) const
{
    return *m_Scope.GetImpl();
}


inline
bool CTSE_Handle::operator==(const CTSE_Handle& tse) const
{
    return m_TSE == tse.m_TSE;
}


inline
bool CTSE_Handle::operator!=(const CTSE_Handle& tse) const
{
    return m_TSE != tse.m_TSE;
}


inline
bool CTSE_Handle::operator<(const CTSE_Handle& tse) const
{
    return m_TSE < tse.m_TSE;
}


inline
CTSE_Handle::TObject& CTSE_Handle::x_GetScopeInfo(void) const
{
    return *m_TSE;
}


END_SCOPE(objects)
END_NCBI_SCOPE

#endif//OBJMGR_TSE_HANDLE__HPP
