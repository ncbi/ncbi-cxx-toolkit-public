#ifndef OBJMGR__BIOSEQ_SET_HANDLE__HPP
#define OBJMGR__BIOSEQ_SET_HANDLE__HPP

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
* Author: Aleksey Grichenko, Eugene Vasilchenko
*
* File Description:
*    Handle to Seq-entry object
*
*/

#include <corelib/ncbiobj.hpp>

#include <objects/seqset/Bioseq_set.hpp>

#include <objmgr/tse_handle.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/** @addtogroup ObjectManagerHandles
 *
 * @{
 */


/////////////////////////////////////////////////////////////////////////////
// CSeq_entry_Handle
/////////////////////////////////////////////////////////////////////////////


class CSeq_annot;
class CSeq_entry;
class CBioseq;
class CBioseq_set;
class CSeqdesc;

class CScope;

class CSeq_entry_Handle;
class CBioseq_set_Handle;
class CBioseq_Handle;
class CSeq_annot_Handle;
class CSeq_entry_EditHandle;
class CBioseq_set_EditHandle;
class CBioseq_EditHandle;
class CSeq_annot_EditHandle;
class CSeq_entry_Info;

class CBioseq_set_Info;
class CBioObjectId;

class CBioseq_set_ScopeInfo : public CScopeInfo_Base
{
public:
    typedef CBioseq_set_Info TObjectInfo;

    CBioseq_set_ScopeInfo(const CTSE_Handle& tse, const TObjectInfo& info)
        : CScopeInfo_Base(tse, reinterpret_cast<const CTSE_Info_Object&>(info))
        {
        }

    const TObjectInfo& GetObjectInfo(void) const
        {
            return reinterpret_cast<const TObjectInfo&>(GetObjectInfo_Base());
        }
    TObjectInfo& GetNCObjectInfo(void)
        {
            return const_cast<TObjectInfo&>(GetObjectInfo());
        }
};



/////////////////////////////////////////////////////////////////////////////
///
///  CBioseq_set_Handle --
///
///  Proxy to access the bioseq_set objects
///

class NCBI_XOBJMGR_EXPORT CBioseq_set_Handle
{
public:
    // Default constructor
    CBioseq_set_Handle(void);

    /// Get scope this handle belongs to
    CScope& GetScope(void) const;

    /// Return a handle for the parent seq-entry of the bioseq
    CSeq_entry_Handle GetParentEntry(void) const;
    
    /// Return a handle for the parent Bioseq-set, or null handle
    CBioseq_set_Handle GetParentBioseq_set(void) const;

    /// Return a handle for the top-level seq-entry
    CSeq_entry_Handle GetTopLevelEntry(void) const;

    /// Get 'edit' version of handle
    CBioseq_set_EditHandle GetEditHandle(void) const;

    /// Return the complete bioseq-set object. 
    /// Any missing data will be loaded and put in the bioseq members.
    CConstRef<CBioseq_set> GetCompleteBioseq_set(void) const;

    /// Return core data for the bioseq-set. 
    /// The object is guaranteed to have basic information loaded. 
    /// Some information may be not loaded yet.
    CConstRef<CBioseq_set> GetBioseq_setCore(void) const;

    /// Unified interface for templates
    typedef CBioseq_set TObject;
    CConstRef<TObject> GetCompleteObject(void) const;
    CConstRef<TObject> GetObjectCore(void) const;

    /// Get unique object id
    const CBioObjectId& GetBioObjectId(void) const;

    /// Check if the bioseq set is empty
    bool IsEmptySeq_set(void) const;

    // member access
    typedef CBioseq_set::TId TId;
    bool IsSetId(void) const;
    bool CanGetId(void) const;
    const TId& GetId(void) const;

    typedef CBioseq_set::TColl TColl;
    bool IsSetColl(void) const;
    bool CanGetColl(void) const;
    const TColl& GetColl(void) const;

    typedef CBioseq_set::TLevel TLevel;
    bool IsSetLevel(void) const;
    bool CanGetLevel(void) const;
    TLevel GetLevel(void) const;

    typedef CBioseq_set::TClass TClass;
    bool IsSetClass(void) const;
    bool CanGetClass(void) const;
    TClass GetClass(void) const;

    typedef CBioseq_set::TRelease TRelease;
    bool IsSetRelease(void) const;
    bool CanGetRelease(void) const;
    const TRelease& GetRelease(void) const;

    typedef CBioseq_set::TDate TDate;
    bool IsSetDate(void) const;
    bool CanGetDate(void) const;
    const TDate& GetDate(void) const;

    typedef CBioseq_set::TDescr TDescr;
    bool IsSetDescr(void) const;
    bool CanGetDescr(void) const;
    const TDescr& GetDescr(void) const;

    // Utility methods/operators

    /// Check if handle points to a bioseq-set
    ///
    /// @sa
    ///    operator !()
    DECLARE_OPERATOR_BOOL(m_Info.IsValid());

    bool IsRemoved(void) const;
        
    // Get CTSE_Handle of containing TSE
    const CTSE_Handle& GetTSE_Handle(void) const;

    // Reset handle and make it not to point to any bioseq-set
    void Reset(void);

    /// Check if handles point to the same bioseq
    ///
    /// @sa
    ///     operator!=()
    bool operator ==(const CBioseq_set_Handle& handle) const;

    // Check if handles point to different bioseqs
    ///
    /// @sa
    ///     operator==()
    bool operator !=(const CBioseq_set_Handle& handle) const;

    /// For usage in containers
    bool operator <(const CBioseq_set_Handle& handle) const;

    /// Go up to a certain complexity level (or the nearest level of the same
    /// priority if the required class is not found).
    CSeq_entry_Handle GetComplexityLevel(CBioseq_set::EClass cls) const;

    /// Return level with exact complexity, or empty handle if not found.
    CSeq_entry_Handle GetExactComplexityLevel(CBioseq_set::EClass cls) const;

    int GetSeq_entry_Index(const CSeq_entry_Handle& handle) const;

protected:
    friend class CScope_Impl;
    friend class CBioseq_Handle;
    friend class CSeq_entry_Handle;
    friend class CSeq_entry_EditHandle;

    friend class CSeqMap_CI;
    friend class CSeq_entry_CI;
    friend class CSeq_annot_CI;
    friend class CAnnotTypes_CI;

    typedef CBioseq_set_ScopeInfo TScopeInfo;
    typedef CScopeInfo_Ref<TScopeInfo> TLock;
    CBioseq_set_Handle(const CBioseq_set_Info& info, const CTSE_Handle& tse);
    CBioseq_set_Handle(const TLock& lock);

    CScope_Impl& x_GetScopeImpl(void) const;

    TLock m_Info;

    typedef int TComplexityTable[20];
    static const TComplexityTable& sx_GetComplexityTable(void);

    static TComplexityTable sm_ComplexityTable;

public: // non-public section

    const TScopeInfo& x_GetScopeInfo(void) const;
    const CBioseq_set_Info& x_GetInfo(void) const;
};


/////////////////////////////////////////////////////////////////////////////
///
///  CBioseq_set_EditHandle --
///
///  Proxy to access and edit the bioseq_set objects
///

class NCBI_XOBJMGR_EXPORT CBioseq_set_EditHandle : public CBioseq_set_Handle
{
public:
    // Default constructor
    CBioseq_set_EditHandle(void);
    /// create edit interface class to the object which already allows editing
    /// throw an exception if the argument is not in editing mode
    explicit CBioseq_set_EditHandle(const CBioseq_set_Handle& h);

    /// Navigate object tree
    CSeq_entry_EditHandle GetParentEntry(void) const;

    // Member modification
    void ResetId(void) const;
    void SetId(TId& id) const;

    void ResetColl(void) const;
    void SetColl(TColl& v) const;

    void ResetLevel(void) const;
    void SetLevel(TLevel v) const;

    void ResetClass(void) const;
    void SetClass(TClass v) const;

    void ResetRelease(void) const;
    void SetRelease(TRelease& v) const;

    void ResetDate(void) const;
    void SetDate(TDate& v) const;

    void ResetDescr(void) const;
    void SetDescr(TDescr& v) const;
    TDescr& SetDescr(void) const;
    bool AddSeqdesc(CSeqdesc& d) const;
    CRef<CSeqdesc> RemoveSeqdesc(const CSeqdesc& d) const;
    void AddSeq_descr(TDescr& v) const;

    /// Create new empty seq-entry
    ///
    /// @param index
    ///  Start index is 0, and -1 means end
    /// 
    /// @return 
    ///  Edit handle to the new seq-entry
    ///
    /// @sa
    ///  AttachEntry()
    ///  CopyEntry()
    ///  TakeEntry()
    CSeq_entry_EditHandle AddNewEntry(int index) const;

    /// Attach an annotation
    ///
    /// @param annot
    ///  Reference to this annotation will be attached
    ///
    /// @return
    ///  Edit handle to the attached annotation
    ///
    /// @sa
    ///  CopyAnnot()
    ///  TakeAnnot()
    CSeq_annot_EditHandle AttachAnnot(CSeq_annot& annot) const;

    /// Attach a copy of the annotation
    ///
    /// @param annot
    ///  Copy of the annotation pointed by this handle will be attached
    ///
    /// @return
    ///  Edit handle to the attached annotation
    ///
    /// @sa
    ///  AttachAnnot()
    ///  TakeAnnot()
    CSeq_annot_EditHandle CopyAnnot(const CSeq_annot_Handle& annot) const;

    /// Remove the annotation from its location and attach to current one
    ///
    /// @param annot
    ///  An annotation  pointed by this handle will be removed and attached
    ///
    /// @return
    ///  Edit handle to the attached annotation
    ///
    /// @sa
    ///  AttachAnnot()
    ///  CopyAnnot()
    CSeq_annot_EditHandle TakeAnnot(const CSeq_annot_EditHandle& annot) const;

    /// Attach a bioseq
    ///
    /// @param seq
    ///  Reference to this bioseq will be attached
    /// @param index
    ///  Start index is 0 and -1 means end
    ///
    /// @return 
    ///  Edit handle to the attached bioseq
    ///
    /// @sa
    ///  CopyBioseq()
    ///  TakeBioseq()
    CBioseq_EditHandle AttachBioseq(CBioseq& seq,
                                    int index = -1) const;

    /// Attach a copy of the bioseq
    ///
    /// @param seq
    ///  Copy of the bioseq pointed by this handle will be attached
    /// @param index
    ///  Start index is 0 and -1 means end
    ///
    /// @return 
    ///  Edit handle to the attached bioseq
    ///
    /// @sa
    ///  AttachBioseq()
    ///  TakeBioseq()
    CBioseq_EditHandle CopyBioseq(const CBioseq_Handle& seq,
                                  int index = -1) const;

    /// Remove bioseq from its location and attach to current one
    ///
    /// @param seq
    ///  bioseq pointed by this handle will be removed and attached
    /// @param index
    ///  Start index is 0 and -1 means end
    ///
    /// @return 
    ///  Edit handle to the attached bioseq
    ///
    /// @sa
    ///  AttachBioseq()
    ///  CopyBioseq()
    CBioseq_EditHandle TakeBioseq(const CBioseq_EditHandle& seq,
                                  int index = -1) const;

    /// Attach an existing seq-entry
    ///
    /// @param entry
    ///  Reference to this seq-entry will be attached
    /// @param index
    ///  Start index is 0 and -1 means end
    ///
    /// @return 
    ///  Edit handle to the attached seq-entry
    ///
    /// @sa
    ///  AddNewEntry()
    ///  CopyEntry()
    ///  TakeEntry()
    CSeq_entry_EditHandle AttachEntry(CSeq_entry& entry,
                                      int index = -1) const;
    CSeq_entry_EditHandle AttachEntry(CRef<CSeq_entry_Info> entry, 
                                      int index = -1) const;

    /// Attach a copy of the existing seq-entry
    ///
    /// @param entry
    ///  Copy of this seq-entry will be attached
    /// @param index
    ///  Start index is 0 and -1 means end
    ///
    /// @return 
    ///  Edit handle to the attached seq-entry
    ///
    /// @sa
    ///  AddNewEntry()
    ///  AttachEntry()
    ///  TakeEntry()
    CSeq_entry_EditHandle CopyEntry(const CSeq_entry_Handle& entry,
                                    int index = -1) const;

    /// Remove seq-entry from its location and attach to current one
    ///
    /// @param entry
    ///  seq-entry pointed by this handle will be removed and attached
    /// @param index
    ///  Start index is 0 and -1 means end
    ///
    /// @return 
    ///  Edit handle to the attached seq-entry
    ///
    /// @sa
    ///  AddNewEntry()
    ///  AttachEntry()
    ///  CopyEntry()
    CSeq_entry_EditHandle TakeEntry(const CSeq_entry_EditHandle& entry,
                                    int index = -1) const;

    /// Attach seq-entry previously removed from another place.
    ///
    /// @param entry
    ///  Edit handle to seq-entry to be attached
    ///  Must be removed.
    /// @param index
    ///  Start index is 0 and -1 means end
    ///
    /// @return 
    ///  Edit handle to the attached seq-entry
    ///
    /// @sa
    ///  AddNewEntry()
    ///  CopyEntry()
    ///  TakeEntry()
    CSeq_entry_EditHandle AttachEntry(const CSeq_entry_EditHandle& entry,
                                      int index = -1) const;

    enum ERemoveMode {
        eRemoveSeq_entry,
        eKeepSeq_entry
    };
    /// Remove current seqset-entry from its location
    void Remove(ERemoveMode mode = eRemoveSeq_entry) const;

protected:
    friend class CScope_Impl;
    friend class CBioseq_EditHandle;
    friend class CSeq_entry_EditHandle;

    CBioseq_set_EditHandle(CBioseq_set_Info& info,
                           const CTSE_Handle& tse);

    void x_Detach(void) const;

public: // non-public section
    TScopeInfo& x_GetScopeInfo(void) const;
    CBioseq_set_Info& x_GetInfo(void) const;

public:

    void x_RealResetDescr(void) const;
    void x_RealSetDescr(TDescr& v) const;
    bool x_RealAddSeqdesc(CSeqdesc& d) const;
    CRef<CSeqdesc> x_RealRemoveSeqdesc(const CSeqdesc& d) const;
    void x_RealAddSeq_descr(TDescr& v) const;

    void x_RealResetId(void) const;
    void x_RealSetId(TId& id) const;
    void x_RealResetColl(void) const;
    void x_RealSetColl(TColl& v) const;
    void x_RealResetLevel(void) const;
    void x_RealSetLevel(TLevel v) const;
    void x_RealResetClass(void) const;
    void x_RealSetClass(TClass v) const;
    void x_RealResetRelease(void) const;
    void x_RealSetRelease(TRelease& v) const;
    void x_RealResetDate(void) const;
    void x_RealSetDate(TDate& v) const;

};


/////////////////////////////////////////////////////////////////////////////
// CBioseq_set_Handle inline methods
/////////////////////////////////////////////////////////////////////////////


inline
CBioseq_set_Handle::CBioseq_set_Handle(void)
{
}


inline
const CTSE_Handle& CBioseq_set_Handle::GetTSE_Handle(void) const
{
    return m_Info->GetTSE_Handle();
}


inline
CScope& CBioseq_set_Handle::GetScope(void) const
{
    return GetTSE_Handle().GetScope();
}


inline
CScope_Impl& CBioseq_set_Handle::x_GetScopeImpl(void) const
{
    return GetTSE_Handle().x_GetScopeImpl();
}


inline
const CBioseq_set_ScopeInfo& CBioseq_set_Handle::x_GetScopeInfo(void) const
{
    return *m_Info;
}


inline
bool CBioseq_set_Handle::IsRemoved(void) const
{
    return m_Info.IsRemoved();
}


inline
bool CBioseq_set_Handle::operator ==(const CBioseq_set_Handle& handle) const
{
    return m_Info == handle.m_Info;
}


inline
bool CBioseq_set_Handle::operator !=(const CBioseq_set_Handle& handle) const
{
    return m_Info != handle.m_Info;
}


inline
bool CBioseq_set_Handle::operator <(const CBioseq_set_Handle& handle) const
{
    return m_Info < handle.m_Info;
}


inline
CConstRef<CBioseq_set> CBioseq_set_Handle::GetCompleteObject(void) const
{
    return GetCompleteBioseq_set();
}


inline
CConstRef<CBioseq_set> CBioseq_set_Handle::GetObjectCore(void) const
{
    return GetBioseq_setCore();
}


/////////////////////////////////////////////////////////////////////////////
// CBioseq_set_EditHandle
/////////////////////////////////////////////////////////////////////////////


inline
CBioseq_set_EditHandle::CBioseq_set_EditHandle(void)
{
}


inline
CBioseq_set_ScopeInfo& CBioseq_set_EditHandle::x_GetScopeInfo(void) const
{
    return m_Info.GetNCObject();
}

/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

#endif//OBJMGR__BIOSEQ_SET_HANDLE__HPP
