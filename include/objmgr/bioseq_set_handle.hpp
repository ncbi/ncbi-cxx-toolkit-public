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
    CBioseq_set_Handle(void);
    CBioseq_set_Handle(CScope& scope, const CBioseq_set_Info& info);

    //
    operator bool(void) const;
    bool operator!(void) const;
    void Reset(void);

    bool operator ==(const CBioseq_set_Handle& handle) const;
    bool operator !=(const CBioseq_set_Handle& handle) const;
    bool operator <(const CBioseq_set_Handle& handle) const;

    // 
    CScope& GetScope(void) const;

    CConstRef<CBioseq_set> GetCompleteBioseq_set(void) const;
    CConstRef<CBioseq_set> GetBioseq_setCore(void) const;

    // owner Seq-entry
    CSeq_entry_Handle GetParentEntry(void) const;

    // member access
    bool IsSetId(void) const;
    const CBioseq_set::TId& GetId(void) const;

    bool IsSetColl(void) const;
    const CBioseq_set::TColl& GetColl(void) const;

    bool IsSetLevel(void) const;
    CBioseq_set::TLevel GetLevel(void) const;

    bool IsSetClass(void) const;
    CBioseq_set::TClass GetClass(void) const;

    bool IsSetRelease(void) const;
    const CBioseq_set::TRelease& GetRelease(void) const;

    bool IsSetDate(void) const;
    const CBioseq_set::TDate& GetDate(void) const;

    bool IsSetDescr(void) const;
    const CSeq_descr& GetDescr(void) const;

protected:
    friend class CScope_Impl;

    friend class CBioseq_Handle;

    friend class CSeqMap_CI;
    friend class CSeq_entry_CI;
    friend class CSeq_annot_CI;
    friend class CAnnotTypes_CI;

    CScope_Impl* x_GetScopeImpl(void) const;
    const CBioseq_set_Info& x_GetInfo(void) const;

private:
    CHeapScope          m_Scope;
    CConstRef<CObject>  m_Info;
};


class NCBI_XOBJMGR_EXPORT CBioseq_set_EditHandle : public CBioseq_set_Handle
{
public:
    CBioseq_set_EditHandle(void);
    CBioseq_set_EditHandle(CScope& scope, CBioseq_set_Info& info);

    CSeq_entry_EditHandle GetParentEntry(void) const;

    CSeq_annot_EditHandle AttachAnnot(const CSeq_annot& annot);
    void RemoveAnnot(CSeq_annot_EditHandle& annot);
    CSeq_annot_EditHandle ReplaceAnnot(CSeq_annot_EditHandle& old_annot,
                                       const CSeq_annot& new_annot);

    CSeq_entry_EditHandle AttachEntry(const CSeq_entry& entry);
    void RemoveEntry(CSeq_entry_EditHandle& sub_entry);

    void RemoveEntry(void);

protected:
    friend class CScope_Impl;

    friend class CSeq_entry_I;
    friend class CSeq_annot_I;

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
CScope_Impl* CBioseq_set_Handle::x_GetScopeImpl(void) const
{
    return m_Scope;
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
