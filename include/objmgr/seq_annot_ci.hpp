#ifndef SEQ_ANNOT_CI__HPP
#define SEQ_ANNOT_CI__HPP

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
*   Seq-annot iterator
*
*/


#include <corelib/ncbiobj.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/seq_entry_info.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class NCBI_XOBJMGR_EXPORT CSeq_annot_Handle
{
public:
    CSeq_annot_Handle(void);
    ~CSeq_annot_Handle(void);

    const CSeq_annot& GetSeq_annot(void) const;
    operator bool(void) const;

private:
    void x_Set(CScope& scope, const CSeq_annot_Info& annot);
    void x_Reset(void);

    CRef<CScope>               m_Scope;
    CConstRef<CSeq_annot_Info> m_Seq_annot;

    friend class CSeq_annot_CI;
    friend class CAnnotTypes_CI;
};


struct SAnnotILevel {
    typedef CSeq_entry_Info::TChildren TEntries;
    typedef TEntries::const_iterator   TEntry_I;

    CConstRef<CSeq_entry_Info> m_Seq_entry;
    TEntry_I                   m_Child;
};


class NCBI_XOBJMGR_EXPORT CSeq_annot_CI
{
public:
    enum EFlags {
        eSearch_entry,
        eSearch_recursive
    };

    CSeq_annot_CI(void);
    CSeq_annot_CI(CScope& scope,
                  const CSeq_entry& entry,
                  EFlags flags = eSearch_recursive);
    CSeq_annot_CI(const CSeq_annot_CI& iter);
    ~CSeq_annot_CI(void);

    CSeq_annot_CI& operator=(const CSeq_annot_CI& iter);

    operator bool(void) const;
    CSeq_annot_CI& operator++(void);

    CSeq_annot_Handle& operator*(void) const;
    CSeq_annot_Handle* operator->(void) const;

private:
    bool x_Found(void) const;
    void x_Next(void);

    typedef CSeq_entry_Info::TAnnots::const_iterator TAnnot_I;
    typedef stack<SAnnotILevel>                      TLevel_Stack;

    mutable CRef<CScope> m_Scope;
    TLevel_Stack         m_Level_Stack;
    SAnnotILevel         m_Level;
    TAnnot_I             m_Annot;
    EFlags               m_Flags;
    mutable CSeq_annot_Handle   m_Value;
};


/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


inline
CSeq_annot_Handle::CSeq_annot_Handle(void)
    : m_Scope(0),
      m_Seq_annot(0)
{
    return;
}

inline
CSeq_annot_Handle::~CSeq_annot_Handle(void)
{
    return;
}

inline
CSeq_annot_Handle::operator bool(void) const
{
    return bool(m_Scope)  &&  bool(m_Seq_annot);
}

inline
void CSeq_annot_Handle::x_Set(CScope& scope, const CSeq_annot_Info& annot)
{
    m_Scope = &scope;
    m_Seq_annot = &annot;
}

inline
void CSeq_annot_Handle::x_Reset(void)
{
    m_Scope.Reset();
    m_Seq_annot.Reset();
}

inline
const CSeq_annot& CSeq_annot_Handle::GetSeq_annot(void) const
{
    return m_Seq_annot->GetSeq_annot();
}


inline
CSeq_annot_CI::CSeq_annot_CI(void)
    : m_Scope(0)
{
}

inline
CSeq_annot_CI::CSeq_annot_CI(const CSeq_annot_CI& iter)
{
    *this = iter;
}

inline
CSeq_annot_CI::~CSeq_annot_CI(void)
{
    return;
}

inline
CSeq_annot_CI::operator bool(void) const
{
    return bool(m_Level.m_Seq_entry)  &&
        m_Annot != m_Level.m_Seq_entry->m_Annots.end();
}

inline
CSeq_annot_CI& CSeq_annot_CI::operator++(void)
{
    _ASSERT(bool(*this));
    m_Value.x_Reset();
    do {
        x_Next();
    } while (!x_Found());
    return *this;
}

inline
CSeq_annot_Handle& CSeq_annot_CI::operator*(void) const
{
    _ASSERT(bool(*this));
    if (!m_Value) {
        m_Value.x_Set(*m_Scope, **m_Annot);
    }
    return m_Value;
}

inline
CSeq_annot_Handle* CSeq_annot_CI::operator->(void) const
{
    _ASSERT(bool(*this));
    if (!m_Value) {
        m_Value.x_Set(*m_Scope, **m_Annot);
    }
    return &m_Value;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  2003/08/04 17:02:57  grichenk
* Added constructors to iterate all annotations from a
* seq-entry or seq-annot.
*
* Revision 1.2  2003/07/25 21:41:27  grichenk
* Implemented non-recursive mode for CSeq_annot_CI,
* fixed friend declaration in CSeq_entry_Info.
*
* Revision 1.1  2003/07/25 15:23:41  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#endif  // SEQ_ANNOT_CI__HPP
