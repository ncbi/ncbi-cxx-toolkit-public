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
class CTSE_Info;

class CSeqdesc;


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

    typedef CConstRef<CObject> TBlobId;
    typedef int TBlobVersion;
    TBlobId GetBlobId(void) const;
    TBlobVersion GetBlobVersion(void) const;

    // Utility methods/operators

    DECLARE_OPERATOR_BOOL_REF(m_Info);


    // Get CTSE_Handle of containing TSE
    const CTSE_Handle& GetTSE_Handle(void) const;


    CSeq_entry_Handle& operator=(const CSeq_entry_Handle& seh);

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

    CSeq_entry_Handle(const CSeq_entry_Info& info,
                      const CTSE_Handle& tse);

    CScope_Impl& x_GetScopeImpl(void) const;

    CTSE_Handle         m_TSE;
    CConstRef<CObject>  m_Info;

public: // non-public section

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
    void AddDescr(const CSeq_entry_EditHandle& src_entry) const;

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

    /// Make the empty Seq-entry be in set state with given Bioseq-set object.
    /// Returns new Bioseq-set handle.
    TSet CopySet(const CBioseq_set_Handle& seqset) const;

    /// Make the empty Seq-entry be in set state with moving Bioseq-set object
    /// from the argument seqset.
    /// Returns new Bioseq-set handle which could be different 
    /// from the argument is the argument is from another scope.
    TSet TakeSet(const TSet& seqset) const;

    /// Make the empty Seq-entry be in seq state with specified Bioseq object.
    /// Returns new Bioseq handle.
    TSeq SelectSeq(CBioseq& seq) const;

    /// Make the empty Seq-entry be in seq state with specified Bioseq object.
    /// Returns new Bioseq handle.
    TSeq CopySeq(const CBioseq_Handle& seq) const;

    /// Make the empty Seq-entry be in seq state with moving bioseq object
    /// from the argument seq.
    /// Returns new Bioseq handle which could be different from the argument
    /// is the argument is from another scope.
    TSeq TakeSeq(const TSeq& seq) const;

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
    CSeq_annot_EditHandle AttachAnnot(const CSeq_annot& annot) const;

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
    CSeq_annot_EditHandle CopyAnnot(const CSeq_annot_Handle&annot) const;

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
    ///  TakeAllAnnots()
    CSeq_annot_EditHandle TakeAnnot(const CSeq_annot_EditHandle& annot) const;

    /// Remove all the annotation from seq-entry and attach to current one
    ///
    /// @param src_entry
    ///  A seq-entry hanlde where annotations will be taken
    ///
    /// @sa
     ///  TakeAnnot()
    void TakeAllAnnots(const CSeq_entry_EditHandle& src_entry) const;

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

    /// Remove this Seq-entry from parent,
    /// or scope if it's top level Seq-entry.
    void Remove(void) const;

protected:
    friend class CScope_Impl;
    friend class CBioseq_EditHandle;
    friend class CBioseq_set_EditHandle;
    friend class CSeq_annot_EditHandle;
    friend class CSeq_entry_I;

    CSeq_entry_EditHandle(const CSeq_entry_Handle& h);
    CSeq_entry_EditHandle(CSeq_entry_Info& info,
                          const CTSE_Handle& tse);

public: // non-public section
    CSeq_entry_Info& x_GetInfo(void) const;
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
    return m_TSE;
}


inline
CScope& CSeq_entry_Handle::GetScope(void) const
{
    return m_TSE.GetScope();
}


inline
CScope_Impl& CSeq_entry_Handle::x_GetScopeImpl(void) const
{
    return m_TSE.x_GetScopeImpl();
}


inline
const CSeq_entry_Info& CSeq_entry_Handle::x_GetInfo(void) const
{
    return reinterpret_cast<const CSeq_entry_Info&>(*m_Info);
}


inline
bool CSeq_entry_Handle::operator ==(const CSeq_entry_Handle& handle) const
{
    return m_TSE == handle.m_TSE  &&  m_Info == handle.m_Info;
}


inline
bool CSeq_entry_Handle::operator !=(const CSeq_entry_Handle& handle) const
{
    return m_TSE != handle.m_TSE  ||  m_Info != handle.m_Info;
}


inline
bool CSeq_entry_Handle::operator <(const CSeq_entry_Handle& handle) const
{
    if ( m_TSE != handle.m_TSE ) {
        return m_TSE < handle.m_TSE;
    }
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
CSeq_entry_EditHandle::CSeq_entry_EditHandle(const CSeq_entry_Handle& h)
    : CSeq_entry_Handle(h)
{
}


inline
CSeq_entry_EditHandle::CSeq_entry_EditHandle(CSeq_entry_Info& info,
                                             const CTSE_Handle& tse)
    : CSeq_entry_Handle(info, tse)
{
}

/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.19  2005/02/28 15:23:05  grichenk
* RemoveDesc() returns CRef<CSeqdesc>
*
* Revision 1.18  2005/01/24 17:09:36  vasilche
* Safe boolean operators.
*
* Revision 1.17  2005/01/12 17:16:14  vasilche
* Avoid performance warning on MSVC.
*
* Revision 1.16  2005/01/05 18:43:57  vasilche
* Added CTSE_Handle::GetTopLevelEntry().
*
* Revision 1.15  2004/12/22 15:56:04  vasilche
* Introduced CTSE_Handle.
*
* Revision 1.14  2004/09/29 19:08:08  kononenk
* Added doxygen formatting
*
* Revision 1.13  2004/08/05 18:28:17  vasilche
* Fixed order of CRef<> release in destruction and assignment of handles.
*
* Revision 1.12  2004/08/04 14:53:26  vasilche
* Revamped object manager:
* 1. Changed TSE locking scheme
* 2. TSE cache is maintained by CDataSource.
* 3. CObjectManager::GetInstance() doesn't hold CRef<> on the object manager.
* 4. Fixed processing of split data.
*
* Revision 1.11  2004/04/29 15:44:30  grichenk
* Added GetTopLevelEntry()
*
* Revision 1.10  2004/03/31 19:54:08  vasilche
* Fixed removal of bioseqs and bioseq-sets.
*
* Revision 1.9  2004/03/31 17:08:06  vasilche
* Implemented ConvertSeqToSet and ConvertSetToSeq.
*
* Revision 1.8  2004/03/29 20:13:05  vasilche
* Implemented whole set of methods to modify Seq-entry object tree.
* Added CBioseq_Handle::GetExactComplexityLevel().
*
* Revision 1.7  2004/03/25 20:06:41  vasilche
* Fixed typo TSet -> TSeq.
*
* Revision 1.6  2004/03/24 18:30:28  vasilche
* Fixed edit API.
* Every *_Info object has its own shallow copy of original object.
*
* Revision 1.5  2004/03/16 21:01:32  vasilche
* Added methods to move Bioseq withing Seq-entry
*
* Revision 1.4  2004/03/16 15:47:26  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.3  2004/02/09 19:18:50  grichenk
* Renamed CDesc_CI to CSeq_descr_CI. Redesigned CSeq_descr_CI
* and CSeqdesc_CI to avoid using data directly.
*
* Revision 1.2  2003/12/03 16:40:03  grichenk
* Added GetParentEntry()
*
* Revision 1.1  2003/11/28 15:12:30  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#endif //SEQ_ENTRY_HANDLE__HPP
