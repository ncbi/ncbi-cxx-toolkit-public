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

#include <objmgr/impl/heap_scope.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

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

class NCBI_XOBJMGR_EXPORT CBioseq_set_Handle
{
public:
    // Default constructor
    CBioseq_set_Handle(void);

    // Get scope this handle belongs to
    CScope& GetScope(void) const;

    // owner Seq-entry
    CSeq_entry_Handle GetParentEntry(void) const;
    CSeq_entry_Handle GetTopLevelEntry(void) const;

    // Get 'edit' version of handle
    CBioseq_set_EditHandle GetEditHandle(void) const;

    // Get controlled object
    CConstRef<CBioseq_set> GetCompleteBioseq_set(void) const;
    CConstRef<CBioseq_set> GetBioseq_setCore(void) const;

    // member access
    typedef CBioseq_set::TId TId;
    bool IsSetId(void) const;
    const TId& GetId(void) const;

    typedef CBioseq_set::TColl TColl;
    bool IsSetColl(void) const;
    const TColl& GetColl(void) const;

    typedef CBioseq_set::TLevel TLevel;
    bool IsSetLevel(void) const;
    TLevel GetLevel(void) const;

    typedef CBioseq_set::TClass TClass;
    bool IsSetClass(void) const;
    TClass GetClass(void) const;

    typedef CBioseq_set::TRelease TRelease;
    bool IsSetRelease(void) const;
    const TRelease& GetRelease(void) const;

    typedef CBioseq_set::TDate TDate;
    bool IsSetDate(void) const;
    const TDate& GetDate(void) const;

    typedef CBioseq_set::TDescr TDescr;
    bool IsSetDescr(void) const;
    const TDescr& GetDescr(void) const;

    // Utility methods/operators
    operator bool(void) const;
    bool operator!(void) const;
    void Reset(void);

    bool operator ==(const CBioseq_set_Handle& handle) const;
    bool operator !=(const CBioseq_set_Handle& handle) const;
    bool operator <(const CBioseq_set_Handle& handle) const;

protected:
    friend class CScope_Impl;
    friend class CBioseq_Handle;
    friend class CSeq_entry_Handle;

    friend class CSeqMap_CI;
    friend class CSeq_entry_CI;
    friend class CSeq_annot_CI;
    friend class CAnnotTypes_CI;

    CBioseq_set_Handle(CScope& scope, const CBioseq_set_Info& info);

    CHeapScope          m_Scope;
    CConstRef<CObject>  m_Info;

public: // non-public section
    const CBioseq_set_Info& x_GetInfo(void) const;
};


class NCBI_XOBJMGR_EXPORT CBioseq_set_EditHandle : public CBioseq_set_Handle
{
public:
    // Default constructor
    CBioseq_set_EditHandle(void);

    // Navigate object tree
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
    bool RemoveDesc(CSeqdesc& v) const;
    void AddAllDescr(CSeq_descr& v) const;

    // Modify object tree
    CSeq_entry_EditHandle AddNewEntry(int index) const;

    CSeq_annot_EditHandle AttachAnnot(const CSeq_annot& annot) const;
    CSeq_annot_EditHandle CopyAnnot(const CSeq_annot_Handle&annot) const;
    CSeq_annot_EditHandle TakeAnnot(const CSeq_annot_EditHandle& annot) const;

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

    void Remove(void) const;

protected:
    friend class CScope_Impl;
    friend class CBioseq_EditHandle;
    friend class CSeq_entry_EditHandle;

    CBioseq_set_EditHandle(const CBioseq_set_Handle& h);
    CBioseq_set_EditHandle(CScope& scope, CBioseq_set_Info& info);

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
CScope& CBioseq_set_Handle::GetScope(void) const
{
    return m_Scope;
}


inline
void CBioseq_set_Handle::Reset(void)
{
    m_Scope.Reset();
    m_Info.Reset();
}


inline
const CBioseq_set_Info& CBioseq_set_Handle::x_GetInfo(void) const
{
    return reinterpret_cast<const CBioseq_set_Info&>(*m_Info);
}


inline
CBioseq_set_Handle::operator bool(void) const
{
    return m_Info;
}


inline
bool CBioseq_set_Handle::operator!(void) const
{
    return !m_Info;
}


inline
bool CBioseq_set_Handle::operator ==(const CBioseq_set_Handle& handle) const
{
    return m_Scope == handle.m_Scope  &&  m_Info == handle.m_Info;
}


inline
bool CBioseq_set_Handle::operator !=(const CBioseq_set_Handle& handle) const
{
    return m_Scope != handle.m_Scope  ||  m_Info != handle.m_Info;
}


inline
bool CBioseq_set_Handle::operator <(const CBioseq_set_Handle& handle) const
{
    if ( m_Scope != handle.m_Scope ) {
        return m_Scope < handle.m_Scope;
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
CBioseq_set_EditHandle::CBioseq_set_EditHandle(CScope& scope,
                                               CBioseq_set_Info& info)
    : CBioseq_set_Handle(scope, info)
{
}


inline
CBioseq_set_Info& CBioseq_set_EditHandle::x_GetInfo(void) const
{
    return const_cast<CBioseq_set_Info&>(CBioseq_set_Handle::x_GetInfo());
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
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
