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

    /// Flags that affect iteration.  Flags that are irrelevant are ignored.
    /// For example, passing fIncludeGivenEntry into the constructor that 
    /// takes a CBioseq_set_Handle is ignored because there is no 
    /// "given entry" because only a CBioseq_set_Handle has been given.
    enum EFlags {
        fRecursive         = (1 << 0), ///< Iterate recursively
        fIncludeGivenEntry = (1 << 1), ///< Include the top (given) entry

        // backward compatibility synonyms
        eNonRecursive = 0, ///< Deprecated @deprecated No need to use.
        eRecursive    = fRecursive ///< Deprecated @deprecated Use fRecursive instead.
    };
    typedef int TFlags; ///< bitwise OR of "EFlags"

    /// backward compatibility synonym
    /// @deprecated
    ///   Use EFlags instead.
    typedef EFlags ERecursionMode;

    /// Create an empty iterator
    CSeq_entry_CI(void);

    /// Create an iterator that enumerates Seq-entries
    /// inside the given Seq-entry.
    explicit
    CSeq_entry_CI(const CSeq_entry_Handle& entry,
                  TFlags flags = 0,
                  CSeq_entry::E_Choice     type_filter = CSeq_entry::e_not_set);

    /// Create an iterator that enumerates Seq-entries
    /// inside the given Bioseq-set.
    explicit
    CSeq_entry_CI(const CBioseq_set_Handle& set,
                  TFlags flags = 0,
                  CSeq_entry::E_Choice      type_filter = CSeq_entry::e_not_set);

    CSeq_entry_CI(const CSeq_entry_CI& iter);
    CSeq_entry_CI& operator =(const CSeq_entry_CI& iter);

    /// Check if iterator points to an object
    DECLARE_OPERATOR_BOOL( IsValid() );

    bool operator ==(const CSeq_entry_CI& iter) const;
    bool operator !=(const CSeq_entry_CI& iter) const;

    /// Move to the next object in iterated sequence
    CSeq_entry_CI& operator ++(void);

    const CSeq_entry_Handle& operator*(void) const;
    const CSeq_entry_Handle* operator->(void) const;

    const CBioseq_set_Handle& GetParentBioseq_set(void) const;

    /// Returns the current depth, which may vary depending on flags.
    /// If fIncludeGivenEntry is set, it returns the current depth relative to 
    /// the initially given seq-entry (or initially given bioseq-set), where the given 
    /// has depth "0", its direct children have depth "1", and so on.
    /// If fIncludeGivenEntry is NOT set, then the returned values are 1 less.
    /// That is, "0" for direct descendents of the given, "1" for the direct descendents
    /// of those, etc.
    /// @sa fIncludeGivenEntry
    int GetDepth(void) const;

protected:

    bool IsValid(void) const;

private:
    void x_Initialize(const CBioseq_set_Handle& set);
    void x_SetCurrentEntry(void);

    friend class CBioseq_set_Handle;

    CSeq_entry_CI& operator ++(int);

    bool x_ValidType(void) const;
    void x_Next(void);

    CBioseq_set_Handle      m_Parent;
    size_t                  m_Index;
    CSeq_entry_Handle       m_Current;
    TFlags                  m_Flags;
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

class NCBI_XOBJMGR_EXPORT CSeq_entry_I : private CSeq_entry_CI
{
public:

    using CSeq_entry_CI::EFlags;
    using CSeq_entry_CI::fRecursive;
    using CSeq_entry_CI::fIncludeGivenEntry;
    using CSeq_entry_CI::eNonRecursive;
    using CSeq_entry_CI::eRecursive;
    using CSeq_entry_CI::TFlags;
    using CSeq_entry_CI::ERecursionMode;
    using CSeq_entry_CI::GetDepth;

    /// Create an empty iterator
    CSeq_entry_I(void);

    /// Create an iterator that enumerates seq-entries
    /// related to the given seq-entry
    CSeq_entry_I(const CSeq_entry_EditHandle& entry,
                 TFlags flags = 0,
                 CSeq_entry::E_Choice         type_filter = CSeq_entry::e_not_set);

    /// Create an iterator that enumerates seq-entries
    /// related to the given seq-set
    CSeq_entry_I(const CBioseq_set_EditHandle& set,
                 TFlags flags = 0,
                 CSeq_entry::E_Choice          type_filter = CSeq_entry::e_not_set);

    CSeq_entry_I(const CSeq_entry_I& iter);
    CSeq_entry_I& operator =(const CSeq_entry_I& iter);

    /// Check if iterator points to an object
    OVERRIDE_OPERATOR_BOOL(CSeq_entry_CI, CSeq_entry_CI::IsValid());

    bool operator ==(const CSeq_entry_I& iter) const;
    bool operator !=(const CSeq_entry_I& iter) const;

    CSeq_entry_I& operator ++(void);

    const CSeq_entry_EditHandle operator*(void) const;
    auto_ptr<const CSeq_entry_EditHandle> operator->(void) const;

    CBioseq_set_EditHandle GetParentBioseq_set(void) const;
    
private:
    friend class CBioseq_set_Handle;

    /// Move to the next object in iterated sequence
    CSeq_entry_I& operator ++(int);
};


/////////////////////////////////////////////////////////////////////////////
// CSeq_entry_CI inline methods
/////////////////////////////////////////////////////////////////////////////

inline
CSeq_entry_CI::CSeq_entry_CI(void)
    : m_Flags(0),
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

inline
bool CSeq_entry_CI::IsValid(void) const
{
    return m_Current;
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
    return CSeq_entry_CI::operator==(iter);
}


inline
bool CSeq_entry_I::operator !=(const CSeq_entry_I& iter) const
{
    return CSeq_entry_CI::operator!=(iter);
}

inline
CSeq_entry_I& CSeq_entry_I::operator ++(void)
{
    CSeq_entry_CI::operator++();
    return *this;
}

inline
const CSeq_entry_EditHandle CSeq_entry_I::operator*(void) const
{
    return CSeq_entry_EditHandle( CSeq_entry_CI::operator*() );
}


inline
auto_ptr<const CSeq_entry_EditHandle> CSeq_entry_I::operator->(void) const
{
    auto_ptr<const CSeq_entry_EditHandle> rval( new CSeq_entry_EditHandle( CSeq_entry_CI::operator*() ) );
    return rval;
}


inline
CBioseq_set_EditHandle CSeq_entry_I::GetParentBioseq_set(void) const
{
    return CBioseq_set_EditHandle( CSeq_entry_CI::GetParentBioseq_set() );
}


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

#endif//OBJMGR__SEQ_ENTRY_CI__HPP
