#ifndef ANNOT_CI__HPP
#define ANNOT_CI__HPP

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
* Author: Aleksey Grichenko, Michael Kimelman, Eugene Vasilchenko
*
* File Description:
*   Object manager iterators
*
*/

#include <objmgr/annot_types_ci.hpp>
#include <objmgr/seq_annot_ci.hpp>
#include <corelib/ncbistd.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/** @addtogroup ObjectManagerIterators
 *
 * @{
 */

/////////////////////////////////////////////////////////////////////////////
///
///  CAnnot_CI --
///
///  Searche individual features, alignments and graphs related to 
///  the specified bioseq or location
///

class NCBI_XOBJMGR_EXPORT CAnnot_CI
{
public:
    /// Create an empty iterator
    CAnnot_CI(void);

    /// Create an iterator that enumerates CSeq_annot objects 
    /// related to the given bioseq
    explicit CAnnot_CI(const CBioseq_Handle& bioseq);

    /// Create an iterator that enumerates CSeq_annot objects 
    /// related to the given bioseq
    ///
    /// @sa
    ///   SAnnotSelector
    CAnnot_CI(const CBioseq_Handle& bioseq,
              const SAnnotSelector& sel);

    /// Create an iterator that enumerates CSeq_annot objects 
    /// related to the given seq-loc based on selection
    CAnnot_CI(CScope& scope,
              const CSeq_loc& loc);

    /// Create an iterator that enumerates CSeq_annot objects 
    /// related to the given seq-loc based on selection
    ///
    /// @sa
    ///   SAnnotSelector
    CAnnot_CI(CScope& scope,
              const CSeq_loc& loc,
              const SAnnotSelector& sel);

    /// Iterate all Seq-annot objects from the seq-entry
    /// regardless of their location, using SAnnotSelector for filtering
    ///
    /// @sa
    ///   SAnnotSelector
    CAnnot_CI(const CSeq_entry_Handle& entry,
              const SAnnotSelector& sel);

    /// Copy constructor
    CAnnot_CI(const CAnnot_CI& iter);

    /// Create an iterator that enumerates all CSeq_annot objects
    /// collected by another iterator CFeat_CI, CGraph_CI, or CAlign_CI
    explicit CAnnot_CI(const CAnnotTypes_CI& iter);

    virtual ~CAnnot_CI(void);

    CAnnot_CI& operator= (const CAnnot_CI& iter);

    /// Move to the next object in iterated sequence
    CAnnot_CI& operator++ (void);

    /// Move to the pervious object in iterated sequence
    CAnnot_CI& operator-- (void);

    void Rewind(void);

    /// Check if iterator points to an object
    DECLARE_OPERATOR_BOOL(x_IsValid());

    /// Check if iterator is empty
    bool empty(void) const;

    /// Get number of collected Seq-annots
    size_t size(void) const;

    const CSeq_annot_Handle& operator*(void) const;
    const CSeq_annot_Handle* operator->(void) const;

private:
    void x_Initialize(const CAnnotTypes_CI& iter);

    bool x_IsValid(void) const;

    CAnnot_CI operator++ (int);
    CAnnot_CI operator-- (int);

    typedef set<CSeq_annot_Handle> TSeqAnnotSet;
    typedef TSeqAnnotSet::const_iterator TIterator;

    TSeqAnnotSet m_SeqAnnotSet;
    TIterator m_Iterator;
};


inline
CAnnot_CI& CAnnot_CI::operator++ (void)
{
    _ASSERT(m_Iterator != m_SeqAnnotSet.end());
    ++m_Iterator;
    return *this;
}


inline
CAnnot_CI& CAnnot_CI::operator-- (void)
{
    _ASSERT(m_Iterator != m_SeqAnnotSet.begin());
    --m_Iterator;
    return *this;
}


inline
const CSeq_annot_Handle& CAnnot_CI::operator*(void) const
{
    _ASSERT(*this);
    return *m_Iterator;
}


inline
const CSeq_annot_Handle* CAnnot_CI::operator->(void) const
{
    _ASSERT(*this);
    return &*m_Iterator;
}


inline
bool CAnnot_CI::x_IsValid(void) const
{
    return m_Iterator != m_SeqAnnotSet.end();
}


inline
bool CAnnot_CI::empty(void) const
{
    return m_SeqAnnotSet.empty();
}


inline
size_t CAnnot_CI::size(void) const
{
    return m_SeqAnnotSet.size();
}


inline
void CAnnot_CI::Rewind(void)
{
    m_Iterator = m_SeqAnnotSet.begin();
}


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // ANNOT_CI__HPP
