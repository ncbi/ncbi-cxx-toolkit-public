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

class CBioseq_set_Info;


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
    DECLARE_OPERATOR_BOOL_REF(m_Info);
        
    // Get CTSE_Handle of containing TSE
    const CTSE_Handle& GetTSE_Handle(void) const;


    CBioseq_set_Handle& operator=(const CBioseq_set_Handle& bsh);

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

protected:
    friend class CScope_Impl;
    friend class CBioseq_Handle;
    friend class CSeq_entry_Handle;

    friend class CSeqMap_CI;
    friend class CSeq_entry_CI;
    friend class CSeq_annot_CI;
    friend class CAnnotTypes_CI;

    CBioseq_set_Handle(const CBioseq_set_Info& info,
                       const CTSE_Handle& tse);

    typedef int TComplexityTable[20];
    static const TComplexityTable& sx_GetComplexityTable(void);

    static TComplexityTable sm_ComplexityTable;

    CTSE_Handle         m_TSE;
    CConstRef<CObject>  m_Info;

public: // non-public section

    CScope_Impl& x_GetScopeImpl(void) const;
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
    bool AddDesc(CSeqdesc& v) const;
    CRef<CSeqdesc> RemoveDesc(CSeqdesc& v) const;
    void AddAllDescr(CSeq_descr& v) const;

    /// Create new empty seq-entry
    ///
    /// @param index
    ///  Start index is 0 and -1 means end
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

    /// Remove current seqset-entry from its location
    void Remove(void) const;

protected:
    friend class CScope_Impl;
    friend class CBioseq_EditHandle;
    friend class CSeq_entry_EditHandle;

    CBioseq_set_EditHandle(const CBioseq_set_Handle& h);
    CBioseq_set_EditHandle(CBioseq_set_Info& info,
                           const CTSE_Handle& tse);

public: // non-public section
    CBioseq_set_Info& x_GetInfo(void) const;
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
    return m_TSE;
}


inline
CScope& CBioseq_set_Handle::GetScope(void) const
{
    return m_TSE.GetScope();
}


inline
CScope_Impl& CBioseq_set_Handle::x_GetScopeImpl(void) const
{
    return m_TSE.x_GetScopeImpl();
}


inline
const CBioseq_set_Info& CBioseq_set_Handle::x_GetInfo(void) const
{
    return reinterpret_cast<const CBioseq_set_Info&>(*m_Info);
}


inline
bool CBioseq_set_Handle::operator ==(const CBioseq_set_Handle& handle) const
{
    return m_TSE == handle.m_TSE  &&  m_Info == handle.m_Info;
}


inline
bool CBioseq_set_Handle::operator !=(const CBioseq_set_Handle& handle) const
{
    return m_TSE != handle.m_TSE  ||  m_Info != handle.m_Info;
}


inline
bool CBioseq_set_Handle::operator <(const CBioseq_set_Handle& handle) const
{
    if ( m_TSE != handle.m_TSE ) {
        return m_TSE < handle.m_TSE;
    }
    return m_Info < handle.m_Info;
}


/////////////////////////////////////////////////////////////////////////////
// CBioseq_set_EditHandle
/////////////////////////////////////////////////////////////////////////////


inline
CBioseq_set_EditHandle::CBioseq_set_EditHandle(void)
{
}


inline
CBioseq_set_EditHandle::CBioseq_set_EditHandle(const CBioseq_set_Handle& h)
    : CBioseq_set_Handle(h)
{
}


inline
CBioseq_set_EditHandle::CBioseq_set_EditHandle(CBioseq_set_Info& info,
                                               const CTSE_Handle& tse)
    : CBioseq_set_Handle(info, tse)
{
}


inline
CBioseq_set_Info& CBioseq_set_EditHandle::x_GetInfo(void) const
{
    return const_cast<CBioseq_set_Info&>(CBioseq_set_Handle::x_GetInfo());
}


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.16  2005/02/28 15:23:05  grichenk
* RemoveDesc() returns CRef<CSeqdesc>
*
* Revision 1.15  2005/01/24 17:09:36  vasilche
* Safe boolean operators.
*
* Revision 1.14  2005/01/12 17:16:13  vasilche
* Avoid performance warning on MSVC.
*
* Revision 1.13  2004/12/22 15:56:04  vasilche
* Introduced CTSE_Handle.
*
* Revision 1.12  2004/09/29 19:52:19  kononenk
* Updated doxygen documentation
*
* Revision 1.11  2004/09/28 15:25:26  kononenk
* Added doxygen formatting
*
* Revision 1.10  2004/08/05 18:28:17  vasilche
* Fixed order of CRef<> release in destruction and assignment of handles.
*
* Revision 1.9  2004/08/04 14:53:25  vasilche
* Revamped object manager:
* 1. Changed TSE locking scheme
* 2. TSE cache is maintained by CDataSource.
* 3. CObjectManager::GetInstance() doesn't hold CRef<> on the object manager.
* 4. Fixed processing of split data.
*
* Revision 1.8  2004/06/09 16:42:25  grichenk
* Added GetComplexityLevel() and GetExactComplexityLevel() to CBioseq_set_Handle
*
* Revision 1.7  2004/05/06 17:32:37  grichenk
* Added CanGetXXXX() methods
*
* Revision 1.6  2004/04/29 15:44:30  grichenk
* Added GetTopLevelEntry()
*
* Revision 1.5  2004/03/31 19:54:07  vasilche
* Fixed removal of bioseqs and bioseq-sets.
*
* Revision 1.4  2004/03/31 17:08:06  vasilche
* Implemented ConvertSeqToSet and ConvertSetToSeq.
*
* Revision 1.3  2004/03/29 20:13:05  vasilche
* Implemented whole set of methods to modify Seq-entry object tree.
* Added CBioseq_Handle::GetExactComplexityLevel().
*
* Revision 1.2  2004/03/24 18:30:28  vasilche
* Fixed edit API.
* Every *_Info object has its own shallow copy of original object.
*
* Revision 1.1  2004/03/16 15:47:26  vasilche
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

#endif//OBJMGR__BIOSEQ_SET_HANDLE__HPP
