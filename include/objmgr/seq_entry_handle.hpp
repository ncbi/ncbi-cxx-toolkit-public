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

class NCBI_XOBJMGR_EXPORT CSeq_entry_Handle
{
public:
    CSeq_entry_Handle(void);
    CSeq_entry_Handle(CScope& scope, const CSeq_entry_Info& info);

    //
    operator bool(void) const;
    bool operator!(void) const;
    void Reset(void);

    bool operator ==(const CSeq_entry_Handle& handle) const;
    bool operator !=(const CSeq_entry_Handle& handle) const;
    bool operator <(const CSeq_entry_Handle& handle) const;

    // 
    CScope& GetScope(void) const;

    CConstRef<CSeq_entry> GetCompleteSeq_entry(void) const;
    CConstRef<CSeq_entry> GetSeq_entryCore(void) const;

    bool HasParentEntry(void) const;
    CBioseq_set_Handle GetParentBioseq_set(void) const;
    CSeq_entry_Handle GetParentEntry(void) const;

    // Seq-entry accessors
    // const CSeq_entry& GetSeq_entry(void) const;
    CSeq_entry::E_Choice Which(void) const;

    // Bioseq access
    bool IsSeq(void) const;
    CBioseq_Handle GetSeq(void) const;

    // Bioseq_set access
    bool IsSet(void) const;
    CBioseq_set_Handle GetSet(void) const;

    bool Set_IsSetId(void) const;
    const CBioseq_set::TId& Set_GetId(void) const;
    bool Set_IsSetColl(void) const;
    const CBioseq_set::TColl& Set_GetColl(void) const;
    bool Set_IsSetLevel(void) const;
    CBioseq_set::TLevel Set_GetLevel(void) const;
    bool Set_IsSetClass(void) const;
    CBioseq_set::TClass Set_GetClass(void) const;
    bool Set_IsSetRelease(void) const;
    const CBioseq_set::TRelease& Set_GetRelease(void) const;
    bool Set_IsSetDate(void) const;
    const CBioseq_set::TDate& Set_GetDate(void) const;

    bool IsSetDescr(void) const;
    const CSeq_descr& GetDescr(void) const;

    CConstRef<CObject> GetBlobId(void) const;

    CConstRef<CTSE_Info> GetTSE_Info(void) const;

    const CSeq_entry_Info& x_GetInfo(void) const;

protected:
    CHeapScope          m_Scope;
    CConstRef<CObject>  m_Info;
};


class NCBI_XOBJMGR_EXPORT CSeq_entry_EditHandle : public CSeq_entry_Handle
{
public:
    CSeq_entry_EditHandle(void);
    CSeq_entry_EditHandle(CScope& scope, CSeq_entry_Info& info);

    CBioseq_set_EditHandle SetSet(void) const;

    CBioseq_set_EditHandle GetParentBioseq_set(void) const;
    CSeq_entry_EditHandle GetParentEntry(void) const;

    CSeq_annot_EditHandle AttachAnnot(const CSeq_annot& annot);
    void RemoveAnnot(CSeq_annot_EditHandle& annot);
    CSeq_annot_EditHandle ReplaceAnnot(CSeq_annot_EditHandle& old_annot,
                                       const CSeq_annot& new_annot);

    CSeq_entry_EditHandle AttachEntry(const CSeq_entry& entry);
    void RemoveEntry(CSeq_entry_EditHandle& sub_entry);

    void Remove(void);

    // Convert entry to bioseq set. Return resulting bioseq_set handle.
    CBioseq_set_EditHandle MakeSet(void);
    // Convert entry to bioseq.
    void MakeSeq(CBioseq_EditHandle& seq);

protected:
    friend class CScope_Impl;

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
