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

#include <objmgr/impl/heap_scope.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////
// CSeq_entry_Handle
/////////////////////////////////////////////////////////////////////////////


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

class NCBI_XOBJMGR_EXPORT CSeq_entry_Handle
{
public:
    // default constructor
    CSeq_entry_Handle(void);

    // Get scope this handle belongs to
    CScope& GetScope(void) const;

    // Navigate object tree
    bool HasParentEntry(void) const;
    CBioseq_set_Handle GetParentBioseq_set(void) const;
    CSeq_entry_Handle GetParentEntry(void) const;
    CSeq_entry_Handle GetSingleSubEntry(void) const;
    CSeq_entry_Handle GetTopLevelEntry(void) const;

    // Get 'edit' version of handle
    CSeq_entry_EditHandle GetEditHandle(void) const;

    // Get controlled object
    CConstRef<CSeq_entry> GetCompleteSeq_entry(void) const;
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

    CConstRef<CObject> GetBlobId(void) const;

    // Utility methods/operators
    operator bool(void) const;
    bool operator!(void) const;
    void Reset(void);

    bool operator ==(const CSeq_entry_Handle& handle) const;
    bool operator !=(const CSeq_entry_Handle& handle) const;
    bool operator <(const CSeq_entry_Handle& handle) const;

protected:
    friend class CScope_Impl;
    friend class CBioseq_Handle;
    friend class CBioseq_set_Handle;
    friend class CSeq_annot_Handle;
    friend class CSeqMap_CI;
    friend class CSeq_entry_CI;

    CSeq_entry_Handle(CScope& scope, const CSeq_entry_Info& info);
    CHeapScope          m_Scope;
    CConstRef<CObject>  m_Info;

public: // non-public section
    CConstRef<CTSE_Info> GetTSE_Info(void) const;

    const CSeq_entry_Info& x_GetInfo(void) const;
};


class NCBI_XOBJMGR_EXPORT CSeq_entry_EditHandle : public CSeq_entry_Handle
{
public:
    // Default constructor
    CSeq_entry_EditHandle(void);

    // Navigate object tree
    CBioseq_set_EditHandle GetParentBioseq_set(void) const;
    CSeq_entry_EditHandle GetParentEntry(void) const;
    CSeq_entry_EditHandle GetSingleSubEntry(void) const;

    // Change descriptions
    void SetDescr(TDescr& v) const;
    void ResetDescr(void) const;
    bool AddSeqdesc(CSeqdesc& v) const;
    bool RemoveSeqdesc(const CSeqdesc& v) const;
    void AddDescr(const CSeq_entry_EditHandle& src_entry) const;

    typedef CBioseq_EditHandle TSeq;
    typedef CBioseq_set_EditHandle TSet;

    TSet SetSet(void) const;
    TSeq SetSeq(void) const;

    // Make this Seq-entry to be empty.
    // Old contents of the entry will be deleted.
    void SelectNone(void) const;

    // Convert the empty Seq-entry to Bioseq-set.
    // Returns new Bioseq-set handle.
    TSet SelectSet(TClass set_class = CBioseq_set::eClass_not_set) const;

    // Make the empty Seq-entry be in set state with given Bioseq-set object.
    // Returns new Bioseq-set handle.
    TSet SelectSet(CBioseq_set& seqset) const;

    // Make the empty Seq-entry be in set state with given Bioseq-set object.
    // Returns new Bioseq-set handle.
    TSet CopySet(const CBioseq_set_Handle& seqset) const;

    // Make the empty Seq-entry be in set state with moving Bioseq-set object
    // from the argument seqset.
    // Returns new Bioseq-set handle which could be different from the argument
    // is the argument is from another scope.
    TSet TakeSet(const TSet& seqset) const;

    // Make the empty Seq-entry be in seq state with specified Bioseq object.
    // Returns new Bioseq handle.
    TSeq SelectSeq(CBioseq& seq) const;

    // Make the empty Seq-entry be in seq state with specified Bioseq object.
    // Returns new Bioseq handle.
    TSeq CopySeq(const CBioseq_Handle& seq) const;

    // Make the empty Seq-entry be in seq state with moving bioseq object
    // from the argument seq.
    // Returns new Bioseq handle which could be different from the argument
    // is the argument is from another scope.
    TSeq TakeSeq(const TSeq& seq) const;

    // Convert the entry from Bioseq to Bioseq-set.
    // Old Bioseq will become the only entry of new Bioseq-set.
    // New Bioseq-set will have the specified class.
    // If the set_class argument is omitted,
    // or equals to CBioseq_set::eClass_not_set,
    // the class field of new Bioseq-set object will not be initialized.
    // Returns new Bioseq-set handle.
    TSet ConvertSeqToSet(TClass set_class = CBioseq_set::eClass_not_set) const;

    // Collapse one level of Bioseq-set.
    // The Bioseq-set should originally contain only one sub-entry.
    // Current Seq-entry will become the same type as sub-entry.
    // All Seq-annot and Seq-descr objects from old Bioseq-set
    // will be moved to new contents (sub-entry).
    void CollapseSet(void) const;

    // Do the same as CollapseSet() when sub-entry is of type bioseq.
    // Throws an exception in other cases.
    // Returns resulting Bioseq handle.
    TSeq ConvertSetToSeq(void) const;

    // Attach new Seq-annot to Bioseq or Bioseq-set
    CSeq_annot_EditHandle AttachAnnot(const CSeq_annot& annot) const;
    CSeq_annot_EditHandle CopyAnnot(const CSeq_annot_Handle&annot) const;
    CSeq_annot_EditHandle TakeAnnot(const CSeq_annot_EditHandle& annot) const;
    void TakeAllAnnots(const CSeq_entry_EditHandle& src_entry) const;

    // Attach new sub objects to Bioseq-set
    // index < 0 or index >= current number of entries means to add at the end.
    CBioseq_EditHandle AttachBioseq(CBioseq& seq,
                                    int index = -1) const;
    CBioseq_EditHandle CopyBioseq(const CBioseq_Handle& seq,
                                  int index = -1) const;
    CBioseq_EditHandle TakeBioseq(const CBioseq_EditHandle& seq,
                                  int index = -1) const;

    CSeq_entry_EditHandle AttachEntry(CSeq_entry& entry,
                                      int index = -1) const;
    CSeq_entry_EditHandle CopyEntry(const CSeq_entry_Handle& entry,
                                    int index = -1) const;
    CSeq_entry_EditHandle TakeEntry(const CSeq_entry_EditHandle& entry,
                                    int index = -1) const;

    // Remove this Seq-entry from parent,
    // or scope if it's top level Seq-entry.
    void Remove(void) const;

protected:
    friend class CScope_Impl;
    friend class CBioseq_EditHandle;
    friend class CBioseq_set_EditHandle;
    friend class CSeq_annot_EditHandle;
    friend class CSeq_entry_I;

    CSeq_entry_EditHandle(const CSeq_entry_Handle& h);
    CSeq_entry_EditHandle(CScope& scope, CSeq_entry_Info& info);

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
CScope& CSeq_entry_Handle::GetScope(void) const
{
    return m_Scope;
}


inline
CSeq_entry_Handle::operator bool(void) const
{
    return m_Info;
}


inline
bool CSeq_entry_Handle::operator!(void) const
{
    return !m_Info;
}


inline
const CSeq_entry_Info& CSeq_entry_Handle::x_GetInfo(void) const
{
    return reinterpret_cast<const CSeq_entry_Info&>(*m_Info);
}


inline
bool CSeq_entry_Handle::operator ==(const CSeq_entry_Handle& handle) const
{
    return m_Scope == handle.m_Scope  &&  m_Info == handle.m_Info;
}


inline
bool CSeq_entry_Handle::operator !=(const CSeq_entry_Handle& handle) const
{
    return m_Scope != handle.m_Scope  ||  m_Info != handle.m_Info;
}


inline
bool CSeq_entry_Handle::operator <(const CSeq_entry_Handle& handle) const
{
    if ( m_Scope != handle.m_Scope ) {
        return m_Scope < handle.m_Scope;
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
CSeq_entry_EditHandle::CSeq_entry_EditHandle(CScope& scope,
                                             CSeq_entry_Info& info)
    : CSeq_entry_Handle(scope, info)
{
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
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
