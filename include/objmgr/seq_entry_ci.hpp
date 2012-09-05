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
    /// Recursion mode
    enum ERecursionMode {
        eNonRecursive, ///< Iterate single level
        eRecursive     ///< Iterate recursively
    };

    /// Create an empty iterator
    CSeq_entry_CI(void);

    /// Create an iterator that enumerates Seq-entries
    /// inside the given Seq-entry.
    CSeq_entry_CI(const CSeq_entry_Handle& entry,
                  ERecursionMode           recursive = eNonRecursive,
                  CSeq_entry::E_Choice     type_filter = CSeq_entry::e_not_set);

    /// Create an iterator that enumerates Seq-entries
    /// inside the given Bioseq-set.
    CSeq_entry_CI(const CBioseq_set_Handle& set,
                  ERecursionMode            recursive = eNonRecursive,
                  CSeq_entry::E_Choice      type_filter = CSeq_entry::e_not_set);

    CSeq_entry_CI(const CSeq_entry_CI& iter);
    CSeq_entry_CI& operator =(const CSeq_entry_CI& iter);

    /// Check if iterator points to an object
    DECLARE_OPERATOR_BOOL(m_Current);

    bool operator ==(const CSeq_entry_CI& iter) const;
    bool operator !=(const CSeq_entry_CI& iter) const;

    /// Move to the next object in iterated sequence
    CSeq_entry_CI& operator ++(void);

    const CSeq_entry_Handle& operator*(void) const;
    const CSeq_entry_Handle* operator->(void) const;

    const CBioseq_set_Handle& GetParentBioseq_set(void) const;

    /// Return current depth relative to the initial seq-entry, 0-based.
    /// In non-recursive mode always returns 0.
    int GetDepth(void) const;

private:
    typedef vector< CRef<CSeq_entry_Info> > TSeq_set;
    typedef TSeq_set::const_iterator  TIterator;

    void x_Initialize(const CBioseq_set_Handle& set);
    void x_SetCurrentEntry(void);

    friend class CBioseq_set_Handle;

    CSeq_entry_CI& operator ++(int);

    bool x_ValidType(void) const;
    void x_Next(void);

    CBioseq_set_Handle      m_Parent;
    TIterator               m_Iterator;
    CSeq_entry_Handle       m_Current;
    ERecursionMode          m_Recursive;
    CSeq_entry::E_Choice    m_Filter;
    auto_ptr<CSeq_entry_CI> m_SubIt;
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
    /// Recursion mode
    enum ERecursionMode {
        eNonRecursive, ///< Iterate single level
        eRecursive     ///< Iterate recursively
    };

    /// Create an empty iterator
    CSeq_entry_I(void);

    /// Create an iterator that enumerates seq-entries
    /// related to the given seq-entrie
    CSeq_entry_I(const CSeq_entry_EditHandle& entry,
                 ERecursionMode               recursive = eNonRecursive,
                 CSeq_entry::E_Choice         type_filter = CSeq_entry::e_not_set);

    /// Create an iterator that enumerates seq-entries
    /// related to the given seq-set
    CSeq_entry_I(const CBioseq_set_EditHandle& set,
                 ERecursionMode                recursive = eNonRecursive,
                 CSeq_entry::E_Choice          type_filter = CSeq_entry::e_not_set);

    CSeq_entry_I(const CSeq_entry_I& iter);
    CSeq_entry_I& operator =(const CSeq_entry_I& iter);

    /// Check if iterator points to an object
    DECLARE_OPERATOR_BOOL(m_Current);

    bool operator ==(const CSeq_entry_I& iter) const;
    bool operator !=(const CSeq_entry_I& iter) const;

    CSeq_entry_I& operator ++(void);

    const CSeq_entry_EditHandle& operator*(void) const;
    const CSeq_entry_EditHandle* operator->(void) const;

    const CBioseq_set_EditHandle& GetParentBioseq_set(void) const;

    /// Return current depth relative to the initial seq-entry, 0-based.
    /// In non-recursive mode always returns 0.
    int GetDepth(void) const;

private:
    typedef vector< CRef<CSeq_entry_Info> > TSeq_set;
    typedef TSeq_set::iterator  TIterator;

    void x_Initialize(const CBioseq_set_EditHandle& set);
    void x_SetCurrentEntry(void);

    friend class CBioseq_set_Handle;

    /// Move to the next object in iterated sequence
    CSeq_entry_I& operator ++(int);

    bool x_ValidType(void) const;
    void x_Next(void);

    CBioseq_set_EditHandle  m_Parent;
    TIterator               m_Iterator;
    CSeq_entry_EditHandle   m_Current;
    ERecursionMode          m_Recursive;
    CSeq_entry::E_Choice    m_Filter;
    auto_ptr<CSeq_entry_I>  m_SubIt;
};


/////////////////////////////////////////////////////////////////////////////
// CSeq_entry_CI inline methods
/////////////////////////////////////////////////////////////////////////////

inline
CSeq_entry_CI::CSeq_entry_CI(void)
    : m_Recursive(eNonRecursive),
      m_Filter(CSeq_entry::e_not_set)
{
}


inline
bool CSeq_entry_CI::operator ==(const CSeq_entry_CI& iter) const
{
    if (m_Current != iter.m_Current) return false;
    if (m_SubIt.get()  &&  iter.m_SubIt.get()) {
        return *m_SubIt == *iter.m_SubIt;
    }
    return (!m_SubIt.get()  &&  !iter.m_SubIt.get());
}


inline
bool CSeq_entry_CI::operator !=(const CSeq_entry_CI& iter) const
{
    return !((*this) == iter);
}


inline
const CSeq_entry_Handle& CSeq_entry_CI::operator*(void) const
{
    return m_SubIt.get() ? **m_SubIt : m_Current;
}


inline
const CSeq_entry_Handle* CSeq_entry_CI::operator->(void) const
{
    return m_SubIt.get() ? &(**m_SubIt) : &m_Current;
}


inline
const CBioseq_set_Handle& CSeq_entry_CI::GetParentBioseq_set(void) const
{
    return m_Parent;
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_entry_I inline methods
/////////////////////////////////////////////////////////////////////////////

inline
CSeq_entry_I::CSeq_entry_I(void)
    : m_Recursive(eNonRecursive),
      m_Filter(CSeq_entry::e_not_set)
{
}


inline
bool CSeq_entry_I::operator ==(const CSeq_entry_I& iter) const
{
    if (m_Current != iter.m_Current) return false;
    if (m_SubIt.get()  &&  iter.m_SubIt.get()) {
        return *m_SubIt == *iter.m_SubIt;
    }
    return (!m_SubIt.get()  &&  !iter.m_SubIt.get());
}


inline
bool CSeq_entry_I::operator !=(const CSeq_entry_I& iter) const
{
    return !((*this) == iter);
}


inline
const CSeq_entry_EditHandle& CSeq_entry_I::operator*(void) const
{
    return m_SubIt.get() ? **m_SubIt : m_Current;
}


inline
const CSeq_entry_EditHandle* CSeq_entry_I::operator->(void) const
{
    return m_SubIt.get() ? &(**m_SubIt) : &m_Current;
}


inline
const CBioseq_set_EditHandle& CSeq_entry_I::GetParentBioseq_set(void) const
{
    return m_Parent;
}


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

#endif//OBJMGR__SEQ_ENTRY_CI__HPP
