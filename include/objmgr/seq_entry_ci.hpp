#ifndef OBJMGR__SEQ_ENTRY_CI__HPP
#define OBJMGR__SEQ_ENTRY_CI__HPP

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
#include <objmgr/bioseq_set_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/** @addtogroup ObjectManagerIterators
 *
 * @{
 */


class CSeq_entry_Handle;
class CSeq_entry_EditHandle;
class CBioseq_set_Handle;
class CBioseq_set_EditHandle;
class CSeq_entry_CI;
class CSeq_entry_I;


/////////////////////////////////////////////////////////////////////////////
///
///  CSeq_entry_CI --
///
///  Enumerate seq-entries in a given parent seq-entry or a bioseq-set
///

class NCBI_XOBJMGR_EXPORT CSeq_entry_CI
{
public:
    /// Create an empty iterator
    CSeq_entry_CI(void);

    /// Create an iterator that enumerates seq-entries
    /// related to the given seq-entry
    CSeq_entry_CI(const CSeq_entry_Handle& entry);

    /// Create an iterator that enumerates seq-entries
    /// related to the given seq-set
    CSeq_entry_CI(const CBioseq_set_Handle& set);

    /// Check if iterator points to an object
    DECLARE_OPERATOR_BOOL(m_Current);

    bool operator ==(const CSeq_entry_CI& iter) const;
    bool operator !=(const CSeq_entry_CI& iter) const;

    /// Move to the next object in iterated sequence
    CSeq_entry_CI& operator ++(void);

    const CSeq_entry_Handle& operator*(void) const;
    const CSeq_entry_Handle* operator->(void) const;

private:
    typedef vector< CRef<CSeq_entry_Info> > TSeq_set;
    typedef TSeq_set::const_iterator  TIterator;

    void x_Initialize(const CBioseq_set_Handle& set);
    void x_SetCurrentEntry(void);

    friend class CBioseq_set_Handle;

    CSeq_entry_CI& operator ++(int);

    CBioseq_set_Handle  m_Parent;
    TIterator           m_Iterator;
    CSeq_entry_Handle   m_Current;
};


/////////////////////////////////////////////////////////////////////////////
///
///  CSeq_entry_I --
///
///  Non const version of CSeq_entry_CI
///
///  @sa
///    CSeq_entry_CI

class NCBI_XOBJMGR_EXPORT CSeq_entry_I
{
public:
    /// Create an empty iterator
    CSeq_entry_I(void);

    /// Create an iterator that enumerates seq-entries
    /// related to the given seq-entrie
    CSeq_entry_I(const CSeq_entry_EditHandle& entry);

    /// Create an iterator that enumerates seq-entries
    /// related to the given seq-set
    CSeq_entry_I(const CBioseq_set_EditHandle& set);

    /// Check if iterator points to an object
    DECLARE_OPERATOR_BOOL(m_Current);

    bool operator ==(const CSeq_entry_I& iter) const;
    bool operator !=(const CSeq_entry_I& iter) const;

    CSeq_entry_I& operator ++(void);

    const CSeq_entry_EditHandle& operator*(void) const;
    const CSeq_entry_EditHandle* operator->(void) const;

private:
    typedef vector< CRef<CSeq_entry_Info> > TSeq_set;
    typedef TSeq_set::iterator  TIterator;

    void x_Initialize(const CBioseq_set_EditHandle& set);
    void x_SetCurrentEntry(void);

    friend class CBioseq_set_Handle;

    /// Move to the next object in iterated sequence
    CSeq_entry_I& operator ++(int);

    CBioseq_set_EditHandle  m_Parent;
    TIterator               m_Iterator;
    CSeq_entry_EditHandle   m_Current;
};


/////////////////////////////////////////////////////////////////////////////
// CSeq_entry_CI inline methods
/////////////////////////////////////////////////////////////////////////////

inline
CSeq_entry_CI::CSeq_entry_CI(void)
{
}


inline
bool CSeq_entry_CI::operator ==(const CSeq_entry_CI& iter) const
{
    return m_Current == iter.m_Current;
}


inline
bool CSeq_entry_CI::operator !=(const CSeq_entry_CI& iter) const
{
    return m_Current != iter.m_Current;
}


inline
const CSeq_entry_Handle& CSeq_entry_CI::operator*(void) const
{
    return m_Current;
}


inline
const CSeq_entry_Handle* CSeq_entry_CI::operator->(void) const
{
    return &m_Current;
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_entry_I inline methods
/////////////////////////////////////////////////////////////////////////////

inline
CSeq_entry_I::CSeq_entry_I(void)
{
}


inline
bool CSeq_entry_I::operator ==(const CSeq_entry_I& iter) const
{
    return m_Current == iter.m_Current;
}


inline
bool CSeq_entry_I::operator !=(const CSeq_entry_I& iter) const
{
    return m_Current != iter.m_Current;
}


inline
const CSeq_entry_EditHandle& CSeq_entry_I::operator*(void) const
{
    return m_Current;
}


inline
const CSeq_entry_EditHandle* CSeq_entry_I::operator->(void) const
{
    return &m_Current;
}


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

#endif//OBJMGR__SEQ_ENTRY_CI__HPP
