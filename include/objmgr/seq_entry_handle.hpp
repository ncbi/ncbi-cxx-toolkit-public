#ifndef SEQ_ENTRY_HANDLE__HPP
#define SEQ_ENTRY_HANDLE__HPP

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

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objmgr/tse_handle.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/** @addtogroup ObjectManagerHandles
 *
 * @{
 */


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
class CSeq_annot_Info;
class CBioseq_set_Info;
class CBioseq_Info;

class CTSE_Info;

class CSeqdesc;
class CBioObjectId;


class CSeq_entry_ScopeInfo : public CScopeInfo_Base
{
public:
    typedef CSeq_entry_Info TObjectInfo;

    CSeq_entry_ScopeInfo(const CTSE_Handle& tse, const TObjectInfo& info)
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
///  CSeq_entry_Handle --
///
///  Proxy to access seq-entry objects
///

class NCBI_XOBJMGR_EXPORT CSeq_entry_Handle
{
public:
    // default constructor
    CSeq_entry_Handle(void);
    CSeq_entry_Handle(const CTSE_Handle& tse);

    /// Get scope this handle belongs to
    CScope& GetScope(void) const;

    /// Get unique object id
    const CBioObjectId& GetBioObjectId(void) const;

    // Navigate object tree
    /// Check if current seq-entry has a parent
    bool HasParentEntry(void) const;

    /// Get parent bioseq-set handle
    CBioseq_set_Handle GetParentBioseq_set(void) const;

    /// Get parent Seq-entry handle
    CSeq_entry_Handle GetParentEntry(void) const;

    /// Get handle of the sub seq-entry
    /// If current seq-entry is not seq-set or 
    /// has more than one subentry exception is thrown
    CSeq_entry_Handle GetSingleSubEntry(void) const;

    /// Get top level Seq-entry handle
    CSeq_entry_Handle GetTopLevelEntry(void) const;

    /// Get 'edit' version of handle
    CSeq_entry_EditHandle GetEditHandle(void) const;

    /// Complete and get const reference to the seq-entry
    CConstRef<CSeq_entry> GetCompleteSeq_entry(void) const;

    /// Get const reference to the seq-entry
    CConstRef<CSeq_entry> GetSeq_entryCore(void) const;

    /// Unified interface for templates
    typedef CSeq_entry TObject;
    CConstRef<TObject> GetCompleteObject(void) const;
    CConstRef<TObject> GetObjectCore(void) const;

    // Seq-entry accessors
    typedef CSeq_entry::E_Choice E_Choice;
    E_Choice Which(void) const;

    // Bioseq access
    bool IsSeq(void) const;
    typedef CBioseq_Handle TSeq;
    TSeq GetSeq(void) const;

    // Bioseq_set access
    bool IsSet(void) const;
    typedef CBioseq_set_Handle TSet;
    TSet GetSet(void) const;

    // descr field is in both Bioseq and Bioseq-set
    bool IsSetDescr(void) const;
    typedef CSeq_descr TDescr;
    const TDescr& GetDescr(void) const;

    typedef CBioseq_set::TClass TClass;

    typedef CBlobIdKey TBlobId;
    typedef int TBlobVersion;
    TBlobId GetBlobId(void) const;
    TBlobVersion GetBlobVersion(void) const;

    // Utility methods/operators

    DECLARE_OPERATOR_BOOL(m_Info.IsValid());

    bool IsRemoved(void) const;


    // Get CTSE_Handle of containing TSE
    const CTSE_Handle& GetTSE_Handle(void) const;


    /// Reset handle and make it not to point to any seq-entry
    void Reset(void);

    /// Check if handles point to the same seq-entry
    ///
    /// @sa
    ///     operator!=()
    bool operator ==(const CSeq_entry_Handle& handle) const;

    // Check if handles point to different seq-entry
    ///
    /// @sa
    ///     operator==()
    bool operator !=(const CSeq_entry_Handle& handle) const;

    /// For usage in containers
    bool operator <(const CSeq_entry_Handle& handle) const;

protected:
    friend class CScope_Impl;
    friend class CBioseq_Handle;
    friend class CBioseq_set_Handle;
    friend class CSeq_annot_Handle;
    friend class CTSE_Handle;
    friend class CSeqMap_CI;
    friend class CSeq_entry_CI;

    typedef CSeq_entry_ScopeInfo TScopeInfo;
    typedef CScopeInfo_Ref<TScopeInfo> TLock;

    CSeq_entry_Handle(const CSeq_entry_Info& info, const CTSE_Handle& tse);
    CSeq_entry_Handle(const TLock& lock);

    CScope_Impl& x_GetScopeImpl(void) const;

    TLock m_Info;

public: // non-public section

    const TScopeInfo& x_GetScopeInfo(void) const;
    const CSeq_entry_Info& x_GetInfo(void) const;
};


/////////////////////////////////////////////////////////////////////////////
///
///  CSeq_entry_Handle --
///
///  Proxy to access seq-entry objects
///

class NCBI_XOBJMGR_EXPORT CSeq_entry_EditHandle : public CSeq_entry_Handle
{
public:
    // Default constructor
    CSeq_entry_EditHandle(void);
    /// create edit interface class to the object which already allows editing
    /// throw an exception if the argument is not in editing mode
    explicit CSeq_entry_EditHandle(const CSeq_entry_Handle& h);

    // Navigate object tree

    /// Get parent bioseq-set edit handle
    CBioseq_set_EditHandle GetParentBioseq_set(void) const;

    /// Get parent seq-entry edit handle
    CSeq_entry_EditHandle GetParentEntry(void) const;

    /// Get edit handle of the sub seq-entry
    /// If current seq-entry is not seq-set or 
    /// has more than one subentry exception is thrown
    CSeq_entry_EditHandle GetSingleSubEntry(void) const;

    // Change descriptions
    void SetDescr(TDescr& v) const;
    void ResetDescr(void) const;
    bool AddSeqdesc(CSeqdesc& v) const;
    CRef<CSeqdesc> RemoveSeqdesc(const CSeqdesc& v) const;

    void AddDescr(TDescr& v) const;

    typedef CBioseq_EditHandle TSeq;
    typedef CBioseq_set_EditHandle TSet;

    TSet SetSet(void) const;
    TSeq SetSeq(void) const;

    /// Make this Seq-entry to be empty.
    /// Old contents of the entry will be deleted.
    void SelectNone(void) const;

    /// Convert the empty Seq-entry to Bioseq-set.
    /// Returns new Bioseq-set handle.
    TSet SelectSet(TClass set_class = CBioseq_set::eClass_not_set) const;

    /// Make the empty Seq-entry be in set state with given Bioseq-set object.
    /// Returns new Bioseq-set handle.
    TSet SelectSet(CBioseq_set& seqset) const;
    TSet SelectSet(CRef<CBioseq_set_Info>) const;

    /// Make the empty Seq-entry be in set state with given Bioseq-set object.
    /// Returns new Bioseq-set handle.
    TSet CopySet(const CBioseq_set_Handle& seqset) const;

    /// Make the empty Seq-entry be in set state with moving Bioseq-set object
    /// from the argument seqset.
    /// Returns new Bioseq-set handle which could be different 
    /// from the argument is the argument is from another scope.
    TSet TakeSet(const TSet& seqset) const;

    /// Make the empty Seq-entry be in set state with Bioseq-set object
    /// from the argument seqset.
    /// The seqset argument must point to removed Bioseq-set.
    /// Returns new Bioseq-set handle which could be different 
    /// from the argument is the argument is from another scope.
    TSet SelectSet(const TSet& seqset) const;

    /// Make the empty Seq-entry be in seq state with specified Bioseq object.
    /// Returns new Bioseq handle.
    TSeq SelectSeq(CBioseq& seq) const;
    TSeq SelectSeq(CRef<CBioseq_Info> seq) const;

    /// Make the empty Seq-entry be in seq state with specified Bioseq object.
    /// Returns new Bioseq handle.
    TSeq CopySeq(const CBioseq_Handle& seq) const;

    /// Make the empty Seq-entry be in seq state with moving bioseq object
    /// from the argument seq.
    /// Returns Bioseq handle which could be different from the argument
    /// is the argument is from another scope.
    TSeq TakeSeq(const TSeq& seq) const;

    /// Make the empty Seq-entry be in seq state with Bioseq object
    /// from the argument seqset.
    /// The seq argument must point to removed Bioseq.
    /// Returns Bioseq handle which could be different 
    /// from the argument is the argument is from another scope.
    TSeq SelectSeq(const TSeq& seq) const;

    /// Convert the entry from Bioseq to Bioseq-set.
    /// Old Bioseq will become the only entry of new Bioseq-set.
    /// New Bioseq-set will have the specified class.
    /// If the set_class argument is omitted,
    /// or equals to CBioseq_set::eClass_not_set,
    /// the class field of new Bioseq-set object will not be initialized.
    /// Returns new Bioseq-set handle.
    TSet ConvertSeqToSet(TClass set_class = CBioseq_set::eClass_not_set) const;

    /// Collapse one level of Bioseq-set.
    /// The Bioseq-set should originally contain only one sub-entry.
    /// Current Seq-entry will become the same type as sub-entry.
    /// All Seq-annot and Seq-descr objects from old Bioseq-set
    /// will be moved to new contents (sub-entry).
    void CollapseSet(void) const;

    /// Do the same as CollapseSet() when sub-entry is of type bioseq.
    /// Throws an exception in other cases.
    /// Returns resulting Bioseq handle.
    TSeq ConvertSetToSeq(void) const;

    // Attach new Seq-annot to Bioseq or Bioseq-set
    
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
    CSeq_annot_EditHandle AttachAnnot(CRef<CSeq_annot_Info> annot) const;

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
    ///  An annotation pointed by this handle will be removed and attached.
    ///
    /// @return
    ///  Edit handle to the attached annotation
    ///
    /// @sa
    ///  AttachAnnot()
    ///  CopyAnnot()
    ///  TakeAllAnnots()
    CSeq_annot_EditHandle TakeAnnot(const CSeq_annot_EditHandle& annot) const;

    /// Attach an annotation
    ///
    /// @param annot
    ///  Reference to this annotation will be attached,
    ///  the annot must be removed.
    ///
    /// @return
    ///  Edit handle to the attached annotation
    ///
    /// @sa
    ///  CopyAnnot()
    ///  TakeAnnot()
    CSeq_annot_EditHandle AttachAnnot(const CSeq_annot_EditHandle& annot) const;

    /// Remove all the annotation from seq-entry and attach to current one
    ///
    /// @param src_entry
    ///  A seq-entry hanlde where annotations will be taken
    ///
    /// @sa
     ///  TakeAnnot()
    void TakeAllAnnots(const CSeq_entry_EditHandle& src_entry) const;

    /// Remove all the descritions from seq-entry and attach to current one
    ///
    /// @param src_entry
    ///  A seq-entry hanlde where annotations will be taken
    ///
    void TakeAllDescr(const CSeq_entry_EditHandle& src_entry) const;

    // Attach new sub objects to Bioseq-set
    // index < 0 or index >= current number of entries 
    // means to add at the end.

    /// Attach an existing bioseq
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

    /// Attach a copy of the existing bioseq
    ///
    /// @param seq
    ///  Copy of this bioseq will be attached
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

    /// Add removed seq-entry
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
    CSeq_entry_EditHandle AttachEntry(const CSeq_entry_EditHandle& entry,
                                      int index = -1) const;

    /// Remove this Seq-entry from parent,
    /// or scope if it's top level Seq-entry.
    void Remove(void) const;

    /// Update annotation index after manual modification of the object
    void UpdateAnnotations(void) const;

protected:
    friend class CScope_Impl;
    friend class CBioseq_EditHandle;
    friend class CBioseq_set_EditHandle;
    friend class CSeq_annot_EditHandle;
    friend class CSeq_entry_I;

    CSeq_entry_EditHandle(CSeq_entry_Info& info, const CTSE_Handle& tse);

public: // non-public section
    TScopeInfo& x_GetScopeInfo(void) const;
    CSeq_entry_Info& x_GetInfo(void) const;

public:
    void x_RealSetDescr(TDescr& v) const;
    void x_RealResetDescr(void) const;
    bool x_RealAddSeqdesc(CSeqdesc& v) const;
    CRef<CSeqdesc> x_RealRemoveSeqdesc(const CSeqdesc& v) const;
    void x_RealAddSeq_descr(TDescr& v) const;

};


/////////////////////////////////////////////////////////////////////////////
// CSeq_entry_Handle inline methods
/////////////////////////////////////////////////////////////////////////////


inline
CSeq_entry_Handle::CSeq_entry_Handle(void)
{
}


inline
const CTSE_Handle& CSeq_entry_Handle::GetTSE_Handle(void) const
{
    return m_Info->GetTSE_Handle();
}


inline
CScope& CSeq_entry_Handle::GetScope(void) const
{
    return GetTSE_Handle().GetScope();
}


inline
CScope_Impl& CSeq_entry_Handle::x_GetScopeImpl(void) const
{
    return GetTSE_Handle().x_GetScopeImpl();
}


inline
const CSeq_entry_ScopeInfo& CSeq_entry_Handle::x_GetScopeInfo(void) const
{
    return *m_Info;
}


inline
CConstRef<CSeq_entry> CSeq_entry_Handle::GetCompleteObject(void) const
{
    return GetCompleteSeq_entry();
}


inline
CConstRef<CSeq_entry> CSeq_entry_Handle::GetObjectCore(void) const
{
    return GetSeq_entryCore();
}


inline
bool CSeq_entry_Handle::IsRemoved(void) const
{
    return m_Info.IsRemoved();
}


inline
bool CSeq_entry_Handle::operator==(const CSeq_entry_Handle& handle) const
{
    return m_Info == handle.m_Info;
}


inline
bool CSeq_entry_Handle::operator!=(const CSeq_entry_Handle& handle) const
{
    return m_Info != handle.m_Info;
}


inline
bool CSeq_entry_Handle::operator<(const CSeq_entry_Handle& handle) const
{
    return m_Info < handle.m_Info;
}


inline
bool CSeq_entry_Handle::IsSeq(void) const
{
    return Which() == CSeq_entry::e_Seq;
}


inline
bool CSeq_entry_Handle::IsSet(void) const
{
    return Which() == CSeq_entry::e_Set;
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_entry_EditHandle
/////////////////////////////////////////////////////////////////////////////


inline
CSeq_entry_EditHandle::CSeq_entry_EditHandle(void)
{
}


inline
CSeq_entry_ScopeInfo& CSeq_entry_EditHandle::x_GetScopeInfo(void) const
{
    return m_Info.GetNCObject();
}


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

#endif //SEQ_ENTRY_HANDLE__HPP
