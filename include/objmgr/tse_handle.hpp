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
#include <objects/seq/seq_id_handle.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CScope;
class CTSE_ScopeInfo;
class CTSE_Info;
class CTSE_Info_Object;
class CTSE_Lock;
class CBioseq_Handle;
class CSeq_entry;
class CSeq_entry_Handle;
class CSeq_id;
class CSeq_id_Handle;
class CBlobIdKey;
class CDataLoader;
class CSeq_feat_Handle;
class CAnnotObject_Info;
class CObject_id;

class CScopeInfo_Base;
class CScopeInfoLocker;

/////////////////////////////////////////////////////////////////////////////
// CTSE_Handle definition
/////////////////////////////////////////////////////////////////////////////


class NCBI_XOBJMGR_EXPORT CTSE_Handle
{
public:
    /// Default constructor/destructor and assignment
    CTSE_Handle(void);
    CTSE_Handle(const CTSE_Handle& tse);
    CTSE_Handle& operator=(const CTSE_Handle& tse);
    ~CTSE_Handle(void);

    /// Returns scope
    CScope& GetScope(void) const;

    /// State check
    bool IsValid(void) const;
    DECLARE_OPERATOR_BOOL(IsValid());

    bool operator==(const CTSE_Handle& tse) const;
    bool operator!=(const CTSE_Handle& tse) const;
    bool operator<(const CTSE_Handle& tse) const;

    /// Reset to null state
    void Reset(void);

    /// TSE info getters
    typedef CBlobIdKey TBlobId;
    TBlobId GetBlobId(void) const;
    CDataLoader* GetDataLoader(void) const;

    bool Blob_IsSuppressed(void) const;
    bool Blob_IsSuppressedTemp(void) const;
    bool Blob_IsSuppressedPerm(void) const;
    bool Blob_IsDead(void) const;

    /// Complete and get const reference to the seq-entry
    CConstRef<CSeq_entry> GetCompleteTSE(void) const;

    /// Get const reference to the seq-entry
    CConstRef<CSeq_entry> GetTSECore(void) const;

    /// Unified interface for templates
    typedef CSeq_entry TObject;
    CConstRef<TObject> GetCompleteObject(void) const;
    CConstRef<TObject> GetObjectCore(void) const;

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

    /// Return true if this TSE handle is local to scope and can be edited.
    bool CanBeEdited(void) const;

    
    typedef vector<CSeq_feat_Handle> TSeq_feat_Handles;

    /// Find features by an integer FeatId
    typedef int TFeatureIdInt;
    TSeq_feat_Handles GetFeaturesWithId(CSeqFeatData::E_Choice type,
                                        TFeatureIdInt id) const;
    TSeq_feat_Handles GetFeaturesWithId(CSeqFeatData::ESubtype subtype,
                                        TFeatureIdInt id) const;
    TSeq_feat_Handles GetFeaturesWithXref(CSeqFeatData::E_Choice type,
                                          TFeatureIdInt id) const;
    TSeq_feat_Handles GetFeaturesWithXref(CSeqFeatData::ESubtype subtype,
                                          TFeatureIdInt id) const;
    CSeq_feat_Handle GetFeatureWithId(CSeqFeatData::E_Choice type,
                                      TFeatureIdInt id) const;
    CSeq_feat_Handle GetFeatureWithId(CSeqFeatData::ESubtype subtype,
                                      TFeatureIdInt id) const;
    /// Find features by a string FeatId
    typedef string TFeatureIdStr;
    TSeq_feat_Handles GetFeaturesWithId(CSeqFeatData::E_Choice type,
                                        const TFeatureIdStr& id) const;
    TSeq_feat_Handles GetFeaturesWithId(CSeqFeatData::ESubtype subtype,
                                        const TFeatureIdStr& id) const;
    TSeq_feat_Handles GetFeaturesWithXref(CSeqFeatData::E_Choice type,
                                          const TFeatureIdStr& id) const;
    TSeq_feat_Handles GetFeaturesWithXref(CSeqFeatData::ESubtype subtype,
                                          const TFeatureIdStr& id) const;
    CSeq_feat_Handle GetFeatureWithId(CSeqFeatData::E_Choice type,
                                      const TFeatureIdStr& id) const;
    CSeq_feat_Handle GetFeatureWithId(CSeqFeatData::ESubtype subtype,
                                      const TFeatureIdStr& id) const;

    /// Find features by CObject_id (string or integer)
    typedef CObject_id TFeatureId;
    TSeq_feat_Handles GetFeaturesWithId(CSeqFeatData::E_Choice type,
                                        const TFeatureId& id) const;
    TSeq_feat_Handles GetFeaturesWithId(CSeqFeatData::ESubtype subtype,
                                        const TFeatureId& id) const;
    TSeq_feat_Handles GetFeaturesWithXref(CSeqFeatData::E_Choice type,
                                          const TFeatureId& id) const;
    TSeq_feat_Handles GetFeaturesWithXref(CSeqFeatData::ESubtype subtype,
                                          const TFeatureId& id) const;
    CSeq_feat_Handle GetFeatureWithId(CSeqFeatData::E_Choice type,
                                      const TFeatureId& id) const;
    CSeq_feat_Handle GetFeatureWithId(CSeqFeatData::ESubtype subtype,
                                      const TFeatureId& id) const;

    CSeq_feat_Handle GetGeneWithLocus(const string& locus, bool tag) const;
    TSeq_feat_Handles GetGenesWithLocus(const string& locus, bool tag) const;
    CSeq_feat_Handle GetGeneByRef(const CGene_ref& ref) const;
    TSeq_feat_Handles GetGenesByRef(const CGene_ref& ref) const;


protected:
    friend class CScope_Impl;
    friend class CTSE_ScopeInfo;

    typedef CTSE_ScopeInfo TScopeInfo;

    CTSE_Handle(TScopeInfo& object);

    typedef vector<CAnnotObject_Info*> TAnnotObjectList;
    CSeq_feat_Handle x_MakeHandle(CAnnotObject_Info* info) const;
    CSeq_feat_Handle x_MakeHandle(const TAnnotObjectList& info) const;
    TSeq_feat_Handles x_MakeHandles(const TAnnotObjectList& infos) const;

private:

    CHeapScope          m_Scope;
    CTSE_ScopeUserLock  m_TSE;

public: // non-public section

    CTSE_Handle(const CTSE_ScopeUserLock& lock);

    TScopeInfo& x_GetScopeInfo(void) const;
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
    return m_Scope.GetScope();
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
CTSE_Handle::TScopeInfo& CTSE_Handle::x_GetScopeInfo(void) const
{
    return const_cast<TScopeInfo&>(*m_TSE);
}


inline
CConstRef<CSeq_entry> CTSE_Handle::GetCompleteObject(void) const
{
    return GetCompleteTSE();
}


inline
CConstRef<CSeq_entry> CTSE_Handle::GetObjectCore(void) const
{
    return GetTSECore();
}


/////////////////////////////////////////////////////////////////////////////
// CScopeInfo classes
/////////////////////////////////////////////////////////////////////////////

class CScopeInfo_Base : public CObject
{
public:
    // creates object with one reference
    NCBI_XOBJMGR_EXPORT CScopeInfo_Base(void);
    NCBI_XOBJMGR_EXPORT CScopeInfo_Base(const CTSE_ScopeUserLock& tse,
                                        const CTSE_Info_Object& info);
    NCBI_XOBJMGR_EXPORT CScopeInfo_Base(const CTSE_Handle& tse,
                                        const CTSE_Info_Object& info);
    NCBI_XOBJMGR_EXPORT ~CScopeInfo_Base(void);
    
    bool IsDetached(void) const
        {
            return m_TSE_ScopeInfo == 0;
        }
    bool IsAttached(void) const
        {
            return m_TSE_ScopeInfo != 0;
        }
    bool HasObject(void) const
        {
            return m_ObjectInfo.NotNull();
        }

    typedef CSeq_id_Handle TIndexId;
    typedef vector<TIndexId> TIndexIds;

    virtual NCBI_XOBJMGR_EXPORT const TIndexIds* GetIndexIds(void) const;

    // Temporary means that this TSE object doesn't have identification,
    // so we cannot retrieve the same CScopeInfo_Base for the TSE object
    // after we release TSE object pointer.
    // As a result, when all handles to this TSE object are removed,
    // we simply forget this CScopeInfo_Base object.
    // For non-temporary TSE object we keep CScopeInfo_Base object in
    // index by its ids, and reuse it when new handle is created.
    bool IsTemporary(void) const
        {
            const TIndexIds* ids = GetIndexIds();
            return !ids || ids->empty();
        }
    
    CTSE_ScopeInfo& x_GetTSE_ScopeInfo(void) const
        {
            CTSE_ScopeInfo* info = m_TSE_ScopeInfo;
            _ASSERT(info);
            return *info;
        }

    const CTSE_Handle& GetTSE_Handle(void) const
        {
            return m_TSE_Handle;
        }

    NCBI_XOBJMGR_EXPORT CScope_Impl& x_GetScopeImpl(void) const;

    const CTSE_Info_Object& GetObjectInfo_Base(void) const
        {
            return reinterpret_cast<const CTSE_Info_Object&>(*m_ObjectInfo);
        }

protected:
    // CScopeInfo_Base can be in the following states:
    //
    // S0. detached, unlocked:
    //    TSE_ScopeInfo == 0,
    //    LockCounter == 0,
    //    TSE_Handle == 0,
    //    ObjectInfo == 0.
    //
    // S1. attached, locked:
    //    TSE_ScopeInfo != 0,
    //    LockCounter > 0,
    //    TSE_Handle != 0,
    //    ObjectInfo != 0, indexed.
    //  When unlocked by handles (LockCounter becomes 0):
    //   1. TSE_Handle is reset,
    //   New state: S5.
    //  When removed explicitly:
    //   1. Do actual remove,
    //   2. Scan for implicit removals,
    //   3. Assert that this CScopeInfo_Base is removed (detached),
    //   New state: S3 (implicitly).
    //  When removed implicitly (found in ObjectInfo index as being removed):
    //   1. TSE_Handle is reset,
    //   2. Removed from index by ObjectInfo,
    //   3. Detached from TSE,
    //   New state: S3.
    //
    // S5. attached, unlocked, indexed:
    //    TSE_ScopeInfo != 0,
    //    LockCounter == 0,
    //    TSE_Handle == 0,
    //    ObjectInfo != 0, indexed.
    //  When relocked:
    //   1. TSE_Handle is set,
    //   New state: S1.
    //  When removed implicitly (found in ObjectInfo index as being removed)
    //   1. Removing from index by ObjectInfo,
    //   2. Detached from TSE,
    //   New state: S3 (or S0).
    //  When TSE is unlocked completely, temporary:
    //   1. ObjectInfo is reset, with removing from index by ObjectInfo,
    //   2. Detached from TSE,
    //   New state: S0.
    //  When TSE is unlocked completely, non-temporary:
    //   1. ObjectInfo is reset, with removing from index by ObjectInfo,
    //   New state: S2.
    //
    // S2. attached, unlocked, non-temporary:
    //    TSE_ScopeInfo != 0,
    //    LockCounter == 0,
    //    TSE_Handle == 0,
    //    ObjectInfo == 0.
    //  When relocked:
    //   1. TSE_Handle is set,
    //   2. ObjectInfo is set, adding to index by ObjectInfo,
    //   New state: S1.
    //  When removed implicitly (is it possible to determine?)
    //   1. Detached from TSE,
    //   New state: S0.
    //
    // S3. detached, locked:
    //    TSE_ScopeInfo == 0,
    //    LockCounter > 0,
    //    TSE_Handle == 0,
    //    ObjectInfo != 0, unindexed.
    //  When unlocked by handles (LockCounter becomes 0):
    //   1. ObjectInfo is reset,
    //   New state: S0.
    //  When added into another place:
    //   1. attached to TSE,
    //   2. TSE_Handle is set,
    //   3. ObjectInfo is set, adding to index by ObjectInfo,
    //   New state: S1.
    //
    // S4. detached, locked, dummy:
    //    TSE_ScopeInfo == 0,
    //    LockCounter > 0,
    //    TSE_Handle == 0,
    //    ObjectInfo == 0.
    //  When unlocked by handles (LockCounter becomes 0):
    //   1. -> S0.
    
    // A. TSE_ScopeInfo != 0: attached, means it's used by some TSE_ScopeInfo.
    // B. LockCounter > 0: locked, means it's used by some handle.
    // C. TSE_Handle != 0 only when attached and locked.
    // D. Indexed by ObjectInfo only when attached and ObjectInfo != 0.

    friend class CTSE_ScopeInfo;
    friend class CScopeInfoLocker;

    // attached new tse and object info
    virtual NCBI_XOBJMGR_EXPORT void x_SetLock(const CTSE_ScopeUserLock& tse,
                                               const CTSE_Info_Object& info);
    virtual NCBI_XOBJMGR_EXPORT void x_ResetLock(void);

    // disconnect from TSE
    virtual NCBI_XOBJMGR_EXPORT void x_AttachTSE(CTSE_ScopeInfo* tse);
    virtual NCBI_XOBJMGR_EXPORT void x_DetachTSE(CTSE_ScopeInfo* tse);
    virtual NCBI_XOBJMGR_EXPORT void x_ForgetTSE(CTSE_ScopeInfo* tse);

    enum ECheckFlags {
        fAllowZero  = 0x00,
        fForceZero  = 0x01,
        fForbidZero = 0x02,
        fAllowInfo  = 0x00,
        fForceInfo  = 0x10,
        fForbidInfo = 0x20
    };
    typedef int TCheckFlags;

    bool NCBI_XOBJMGR_EXPORT x_Check(TCheckFlags zero_counter_mode) const;
    void NCBI_XOBJMGR_EXPORT x_RemoveLastInfoLock(void);

    void AddInfoLock(void)
        {
            _ASSERT(x_Check(fForceInfo));
            m_LockCounter.Add(1);
            _ASSERT(x_Check(fForbidZero));
        }
    void RemoveInfoLock(void)
        {
            _ASSERT(x_Check(fForbidZero));
            if ( m_LockCounter.Add(-1) <= 0 ) {
                x_RemoveLastInfoLock();
            }
        }

private: // data members

    CTSE_ScopeInfo*         m_TSE_ScopeInfo; // null if object is removed.
    CAtomicCounter_WithAutoInit m_LockCounter; // counts all referencing handles.
    // The following members are not null when handle is locked (counter > 0)
    // and not removed.
    CTSE_Handle             m_TSE_Handle; // locks TSE from releasing.
    CConstRef<CObject>      m_ObjectInfo; // current object info.
    CRef<CObject>           m_DetachedInfo;

private: // to prevent copying
    CScopeInfo_Base(const CScopeInfo_Base&);
    void operator=(const CScopeInfo_Base&);
};


class CScopeInfoLocker : public CObjectCounterLocker
{
public:
    void Lock(CScopeInfo_Base* info) const
        {
            CObjectCounterLocker::Lock(info);
            info->AddInfoLock();
        }
    void Relock(CScopeInfo_Base* info) const
        {
            Lock(info);
        }
    void Unlock(CScopeInfo_Base* info) const
        {
            info->RemoveInfoLock();
            CObjectCounterLocker::Unlock(info);
        }
};


class CScopeInfo_RefBase : public CRef<CScopeInfo_Base, CScopeInfoLocker>
{
public:
    CScopeInfo_RefBase(void)
        {
        }
    explicit CScopeInfo_RefBase(CScopeInfo_Base* info)
        : CRef<CScopeInfo_Base, CScopeInfoLocker>(info)
        {
        }

    bool IsValid(void) const
        {
            return NotNull() && GetPointerOrNull()->IsAttached();
        }
    bool IsRemoved(void) const
        {
            return NotNull() && GetPointerOrNull()->IsDetached();
        }
};


template<class Info>
class CScopeInfo_Ref : public CScopeInfo_RefBase
{
public:
    typedef Info TScopeInfo;

    CScopeInfo_Ref(void)
        {
        }
    explicit CScopeInfo_Ref(TScopeInfo& info)
        : CScopeInfo_RefBase(toBase(&info))
        {
        }

    void Reset(void)
        {
            CScopeInfo_RefBase::Reset();
        }
    void Reset(TScopeInfo* info)
        {
            CScopeInfo_RefBase::Reset(toBase(info));
        }

    TScopeInfo& operator*(void)
        {
            return *toInfo(GetNonNullPointer());
        }
    const TScopeInfo& operator*(void) const
        {
            return *toInfo(GetNonNullPointer());
        }
    TScopeInfo& GetNCObject(void) const
        {
            return *toInfo(GetNonNullNCPointer());
        }

    TScopeInfo* operator->(void)
        {
            return toInfo(GetNonNullPointer());
        }
    const TScopeInfo* operator->(void) const
        {
            return toInfo(GetNonNullPointer());
        }

protected:
    static CScopeInfo_Base* toBase(TScopeInfo* info)
        {
            return reinterpret_cast<CScopeInfo_Base*>(info);
        }
    static TScopeInfo* toInfo(CScopeInfo_Base* base)
        {
            return reinterpret_cast<TScopeInfo*>(base);
        }
    static const TScopeInfo* toInfo(const CScopeInfo_Base* base)
        {
            return reinterpret_cast<const TScopeInfo*>(base);
        }
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif//OBJMGR_TSE_HANDLE__HPP
