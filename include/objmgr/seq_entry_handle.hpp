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

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objmgr/impl/seq_entry_info.hpp>
#include <objmgr/impl/scope_info.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////
// CSeq_entry_Handle
/////////////////////////////////////////////////////////////////////////////


class CBioseq_Handle;
class CSeq_entry_CI;

class NCBI_XOBJMGR_EXPORT CSeq_entry_Handle
{
public:
    CSeq_entry_Handle(void);
    ~CSeq_entry_Handle(void);

    operator bool(void) const;

    bool operator ==(const CSeq_entry_Handle& handle) const;
    bool operator !=(const CSeq_entry_Handle& handle) const;
    bool operator <(const CSeq_entry_Handle& handle) const;

    // const CSeq_entry& GetSeq_entry(void) const;
    CSeq_entry::E_Choice Which(void) const;

    // Bioseq access
    bool IsSeq(void) const;
    CBioseq_Handle GetBioseq(void) const;

    // Bioseq-set access
    bool IsSet(void) const;
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

    CSeq_entry_CI BeginSet(void);

private:
    friend class CBioseq_Handle;
    friend class CSeq_entry_CI;

    CSeq_entry_Handle(CScope& scope, const CSeq_entry_Info& info);
    void x_Reset(void);

    const CBioseq_set& x_GetBioseq_set(void) const;

    CScope*                    m_Scope;
    CConstRef<CSeq_entry_Info> m_Entry_Info;
};


class CSeq_entry_CI
{
public:
    CSeq_entry_CI(void);
    CSeq_entry_CI(const CSeq_entry_CI& iter);
    ~CSeq_entry_CI(void);

    CSeq_entry_CI& operator =(const CSeq_entry_CI& iter);
    operator bool(void) const;
    bool operator ==(const CSeq_entry_CI& iter) const;
    bool operator !=(const CSeq_entry_CI& iter) const;
    CSeq_entry_CI& operator ++(void);

    CSeq_entry_Handle& operator*(void) const;
    CSeq_entry_Handle* operator->(void) const;

private:
    typedef CSeq_entry_Info::TEntries TEntries;
    typedef TEntries::const_iterator  TIterator;

    const TEntries& x_GetEntries(void) const;

    friend class CSeq_entry_Handle;

    CSeq_entry_CI(CSeq_entry_Handle& entry);
    CSeq_entry_CI& operator ++(int);

    CSeq_entry_Handle         m_Seq_entry;
    TIterator                 m_Iterator;
    mutable CSeq_entry_Handle m_Handle;
};


/////////////////////////////////////////////////////////////////////////////
// CSeq_entry_Handle inline methods
/////////////////////////////////////////////////////////////////////////////


inline
CSeq_entry_Handle::CSeq_entry_Handle(CScope& scope,
                                     const CSeq_entry_Info& info)
    : m_Scope(&scope),
      m_Entry_Info(&info)
{
    return;
}

inline
CSeq_entry_Handle::CSeq_entry_Handle(void)
{
    return;
}

inline
CSeq_entry_Handle::~CSeq_entry_Handle(void)
{
    return;
}

inline
CSeq_entry_Handle::operator bool(void) const
{
    return bool(m_Entry_Info);
}

inline
bool CSeq_entry_Handle::operator ==(const CSeq_entry_Handle& handle) const
{
    return m_Scope == handle.m_Scope  &&  m_Entry_Info == handle.m_Entry_Info;
}

inline
bool CSeq_entry_Handle::operator !=(const CSeq_entry_Handle& handle) const
{
    return m_Scope != handle.m_Scope  ||  m_Entry_Info != handle.m_Entry_Info;
}

inline
bool CSeq_entry_Handle::operator <(const CSeq_entry_Handle& handle) const
{
    if (m_Scope == handle.m_Scope) {
        return m_Entry_Info < handle.m_Entry_Info;
    }
    return m_Scope < handle.m_Scope;
}

inline
CSeq_entry::E_Choice CSeq_entry_Handle::Which(void) const
{
    return m_Entry_Info->GetSeq_entry().Which();
}

inline
bool CSeq_entry_Handle::IsSeq(void) const
{
    return m_Entry_Info->GetSeq_entry().IsSeq();
}

inline
bool CSeq_entry_Handle::IsSet(void) const
{
    return m_Entry_Info->GetSeq_entry().IsSet();
}

inline
const CBioseq_set& CSeq_entry_Handle::x_GetBioseq_set(void) const
{
    _ASSERT(IsSet());
    return m_Entry_Info->GetSeq_entry().GetSet();
}

inline
bool CSeq_entry_Handle::Set_IsSetId(void) const
{
    return x_GetBioseq_set().IsSetId();
}

inline
const CBioseq_set::TId& CSeq_entry_Handle::Set_GetId(void) const
{
    _ASSERT(Set_IsSetId());
    return x_GetBioseq_set().GetId();
}

inline
bool CSeq_entry_Handle::Set_IsSetColl(void) const
{
    return x_GetBioseq_set().IsSetColl();
}

inline
const CBioseq_set::TColl& CSeq_entry_Handle::Set_GetColl(void) const
{
    _ASSERT(Set_IsSetColl());
    return x_GetBioseq_set().GetColl();
}

inline
bool CSeq_entry_Handle::Set_IsSetLevel(void) const
{
    return x_GetBioseq_set().IsSetLevel();
}

inline
CBioseq_set::TLevel CSeq_entry_Handle::Set_GetLevel(void) const
{
    _ASSERT(Set_IsSetLevel());
    return x_GetBioseq_set().GetLevel();
}

inline
bool CSeq_entry_Handle::Set_IsSetClass(void) const
{
    return x_GetBioseq_set().IsSetClass();
}

inline
CBioseq_set::TClass CSeq_entry_Handle::Set_GetClass(void) const
{
    _ASSERT(Set_IsSetClass());
    return x_GetBioseq_set().GetClass();
}

inline
bool CSeq_entry_Handle::Set_IsSetRelease(void) const
{
    return x_GetBioseq_set().IsSetRelease();
}

inline
const CBioseq_set::TRelease& CSeq_entry_Handle::Set_GetRelease(void) const
{
    _ASSERT(Set_IsSetClass());
    return x_GetBioseq_set().GetRelease();
}

inline
bool CSeq_entry_Handle::Set_IsSetDate(void) const
{
    return x_GetBioseq_set().IsSetDate();
}

inline
const CBioseq_set::TDate& CSeq_entry_Handle::Set_GetDate(void) const
{
    _ASSERT(Set_IsSetClass());
    return x_GetBioseq_set().GetDate();
}

inline
CSeq_entry_CI CSeq_entry_Handle::BeginSet(void)
{
    return CSeq_entry_CI(*this);
}

inline
void CSeq_entry_Handle::x_Reset(void)
{
    m_Scope = 0;
    m_Entry_Info.Reset();
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_entry_CI inline methods
/////////////////////////////////////////////////////////////////////////////

inline
CSeq_entry_CI::CSeq_entry_CI(void)
{
    return;
}

inline
CSeq_entry_CI::CSeq_entry_CI(const CSeq_entry_CI& iter)
{
    m_Seq_entry = iter.m_Seq_entry;
    if (m_Seq_entry) {
        m_Iterator = iter.m_Iterator;
    }
}

inline
CSeq_entry_CI::~CSeq_entry_CI(void)
{
    return;
}

inline
CSeq_entry_CI& CSeq_entry_CI::operator =(const CSeq_entry_CI& iter)
{
    m_Handle.x_Reset();
    if (this != &iter) {
        m_Seq_entry = iter.m_Seq_entry;
        if (m_Seq_entry) {
            m_Iterator = iter.m_Iterator;
        }
    }
    return *this;
}

inline
CSeq_entry_CI::operator bool(void) const
{
    return (m_Seq_entry && m_Iterator != x_GetEntries().end());
}

inline
bool CSeq_entry_CI::operator ==(const CSeq_entry_CI& iter) const
{
    if (m_Seq_entry != iter.m_Seq_entry) {
        return false;
    }
    return !m_Seq_entry  ||  m_Iterator == iter.m_Iterator;
}

inline
bool CSeq_entry_CI::operator !=(const CSeq_entry_CI& iter) const
{
    if (m_Seq_entry != iter.m_Seq_entry) {
        return true;
    }
    return !m_Seq_entry  &&  m_Iterator != iter.m_Iterator;
}

inline
CSeq_entry_CI& CSeq_entry_CI::operator ++(void)
{
    m_Handle.x_Reset();
    if ( bool(*this) ) {
        ++m_Iterator;
    }
    return *this;
}

inline
CSeq_entry_Handle& CSeq_entry_CI::operator*(void) const
{
    _ASSERT( bool(*this) );
    if ( !m_Handle ) {
        m_Handle = CSeq_entry_Handle(*m_Seq_entry.m_Scope, **m_Iterator);
    }
    return m_Handle;
}

inline
CSeq_entry_Handle* CSeq_entry_CI::operator->(void) const
{
    _ASSERT( bool(*this) );
    if ( !m_Handle ) {
        m_Handle = CSeq_entry_Handle(*m_Seq_entry.m_Scope, **m_Iterator);
    }
    return &m_Handle;
}

inline
CSeq_entry_CI::CSeq_entry_CI(CSeq_entry_Handle& entry)
{
    m_Seq_entry = entry;
    if (m_Seq_entry) {
        m_Iterator = x_GetEntries().begin();
    }
}

inline
const CSeq_entry_CI::TEntries& CSeq_entry_CI::x_GetEntries(void) const
{
    _ASSERT(m_Seq_entry);
    return m_Seq_entry.m_Entry_Info->m_Entries;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2003/11/28 15:12:30  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#endif //SEQ_ENTRY_HANDLE__HPP
