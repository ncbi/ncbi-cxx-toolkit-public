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


class NCBI_XOBJMGR_EXPORT CAnnot_CI : public CAnnotTypes_CI
{
public:
    CAnnot_CI(void);
    CAnnot_CI(CScope& scope, const CSeq_loc& loc,
              SAnnotSelector sel);
    CAnnot_CI(const CBioseq_Handle& bioseq, TSeqPos start, TSeqPos stop,
              SAnnotSelector sel);
    // Search all TSEs in all datasources
    CAnnot_CI(CScope& scope, const CSeq_loc& loc,
              EOverlapType overlap_type = eOverlap_Intervals,
              EResolveMethod resolve = eResolve_TSE,
              const CSeq_entry* entry = 0);
    // Search only in TSE, containing the bioseq
    CAnnot_CI(const CBioseq_Handle& bioseq, TSeqPos start, TSeqPos stop,
              EOverlapType overlap_type = eOverlap_Intervals,
              EResolveMethod resolve = eResolve_TSE,
              const CSeq_entry* entry = 0);

    // Iterate all features from the object regardless of their location
    CAnnot_CI(const CAnnot_CI& iter);
    virtual ~CAnnot_CI(void);

    CAnnot_CI& operator= (const CAnnot_CI& iter);

    CAnnot_CI& operator++ (void);
    CAnnot_CI& operator-- (void);
    operator bool (void) const;

    CSeq_annot_Handle& operator*(void) const;
    CSeq_annot_Handle* operator->(void) const;

private:
    void x_Collect(void);

    CAnnot_CI operator++ (int);
    CAnnot_CI operator-- (int);

    typedef set<CSeq_annot_Handle> TSeqAnnotSet;
    typedef TSeqAnnotSet::iterator TIterator;

    mutable TSeqAnnotSet m_SeqAnnotSet;
    TIterator m_Iterator;
    mutable CSeq_annot_Handle m_Value;
};


inline
CAnnot_CI::CAnnot_CI(void)
{
    SetNoMapping(true);
    SetSortOrder(eSortOrder_None);
    m_Iterator = m_SeqAnnotSet.end();
}

inline
CAnnot_CI::CAnnot_CI(const CAnnot_CI& iter)
    : CAnnotTypes_CI(iter)
{
    m_SeqAnnotSet = iter.m_SeqAnnotSet;
    if (iter) {
        m_Iterator = m_SeqAnnotSet.find(*iter.m_Iterator);
    }
    else {
        m_Iterator = m_SeqAnnotSet.end();
    }
}

inline
CAnnot_CI::CAnnot_CI(CScope& scope, const CSeq_loc& loc,
                     SAnnotSelector sel)
    : CAnnotTypes_CI(scope, loc,
                     sel.SetAnnotChoice(CSeq_annot::C_Data::e_not_set)
                     .SetNoMapping(true)
                     .SetSortOrder(eSortOrder_None))
{
    x_Collect();
}


inline
CAnnot_CI::CAnnot_CI(const CBioseq_Handle& bioseq,
                     TSeqPos start, TSeqPos stop,
                     SAnnotSelector sel)
    : CAnnotTypes_CI(bioseq, start, stop,
                     sel.SetAnnotChoice(CSeq_annot::C_Data::e_not_set)
                     .SetNoMapping(true)
                     .SetSortOrder(eSortOrder_None))
{
    x_Collect();
}

inline
CAnnot_CI& CAnnot_CI::operator= (const CAnnot_CI& iter)
{
    CAnnotTypes_CI::operator=(iter);
    m_SeqAnnotSet.clear();
    m_SeqAnnotSet = iter.m_SeqAnnotSet;
    if (iter) {
        m_Iterator = m_SeqAnnotSet.find(*iter.m_Iterator);
    }
    else {
        m_Iterator = m_SeqAnnotSet.end();
    }
    return *this;
}

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
CAnnot_CI::operator bool (void) const
{
    return m_Iterator != m_SeqAnnotSet.end();
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


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
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
