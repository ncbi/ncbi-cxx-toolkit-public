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

class NCBI_XOBJMGR_EXPORT CAnnot_CI : public CAnnotTypes_CI
{
public:
    /// Create an empty iterator
    CAnnot_CI(void);

    /// Create an iterator that enumerates CSeq_annot objects 
    /// related to the given bioseq
    CAnnot_CI(const CBioseq_Handle& bioseq);

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

    /// Create an iterator that enumerates CSeq_annot objects
    /// from the annotation regardless of their location
    CAnnot_CI(const CAnnot_CI& iter);
    virtual ~CAnnot_CI(void);

    CAnnot_CI& operator= (const CAnnot_CI& iter);

    /// Move to the next object in iterated sequence
    CAnnot_CI& operator++ (void);

    /// Move to the pervious object in iterated sequence
    CAnnot_CI& operator-- (void);

    /// Check if iterator points to an object
    DECLARE_OPERATOR_BOOL(x_IsValid());

    CSeq_annot_Handle& operator*(void) const;
    CSeq_annot_Handle* operator->(void) const;

private:
    void x_Collect(void);

    bool x_IsValid(void) const;

    CAnnot_CI operator++ (int);
    CAnnot_CI operator-- (int);

    typedef set<CSeq_annot_Handle> TSeqAnnotSet;
    typedef TSeqAnnotSet::iterator TIterator;

    mutable TSeqAnnotSet m_SeqAnnotSet;
    TIterator m_Iterator;
    mutable CSeq_annot_Handle m_Value;
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
CSeq_annot_Handle& CAnnot_CI::operator*(void) const
{
    _ASSERT(*this);
    return m_Value = *m_Iterator;
}


inline
CSeq_annot_Handle* CAnnot_CI::operator->(void) const
{
    _ASSERT(*this);
    m_Value = *m_Iterator;
    return &m_Value;
}


inline
bool CAnnot_CI::x_IsValid(void) const
{
    return m_Iterator != m_SeqAnnotSet.end();
}


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.32  2005/03/15 22:05:28  grichenk
* Fixed operator bool.
*
* Revision 1.31  2005/01/24 17:09:36  vasilche
* Safe boolean operators.
*
* Revision 1.30  2005/01/06 16:41:30  grichenk
* Removed deprecated methods
*
* Revision 1.29  2004/10/29 16:29:47  grichenk
* Prepared to remove deprecated methods, added new constructors.
*
* Revision 1.28  2004/10/01 14:36:44  kononenk
* Added doxygen formatting
*
* Revision 1.27  2004/04/05 15:56:13  grichenk
* Redesigned CAnnotTypes_CI: moved all data and data collecting
* functions to CAnnotDataCollector. CAnnotTypes_CI is no more
* inherited from SAnnotSelector.
*
* Revision 1.26  2004/03/16 15:47:25  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.25  2004/02/04 18:05:31  grichenk
* Added annotation filtering by set of types/subtypes.
* Renamed *Choice to *Type in SAnnotSelector.
*
* Revision 1.24  2003/10/09 12:29:52  dicuccio
* Added missing 'public' in inheritance
*
* Revision 1.23  2003/09/11 17:45:06  grichenk
* Added adaptive-depth option to annot-iterators.
*
* Revision 1.22  2003/08/27 21:08:03  grichenk
* CAnnotTypes_CI made private
*
* Revision 1.21  2003/08/22 15:00:47  grichenk
* Redesigned CAnnot_CI to iterate over seq-annots, containing
* given location.
*
* Revision 1.20  2003/08/15 15:21:58  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#endif  // ANNOT_CI__HPP
