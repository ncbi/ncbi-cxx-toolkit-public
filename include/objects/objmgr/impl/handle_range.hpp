#ifndef HANDLE_RANGE__HPP
#define HANDLE_RANGE__HPP

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
* Author: Aleksey Grichenko, Michael Kimelman
*
* File Description:
*
*/

#include <objects/seqloc/Na_strand.hpp>
#include <util/range.hpp>
#include <vector>
#include <utility>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// Locations list for a particular bioseq handle
class CHandleRange
{
public:
    typedef CRange<TSeqPos> TRange;
    typedef COpenRange<TSeqPos> TOpenRange;
    typedef pair<TRange, ENa_strand> TRangeWithStrand;
    typedef vector<TRangeWithStrand> TRanges;
    typedef TRanges::const_iterator const_iterator;

    CHandleRange(void);
    CHandleRange(const CHandleRange& hr);
    ~CHandleRange(void);
    CHandleRange& operator=(const CHandleRange& hr);

    bool Empty(void) const;
    const_iterator begin(void) const;
    const_iterator end(void) const;

    // Add a new range
    void AddRange(TRange range, ENa_strand strand/* = eNa_strand_unknown*/);
    // Merge a new range with the existing ranges
    void MergeRange(TRange range, ENa_strand strand);

    void AddRanges(const CHandleRange& hr);

    // Get the range including all ranges in the list (with any strand)
    TRange GetOverlappingRange(void) const;
    // Check if the two sets of ranges do intersect
    bool IntersectingWith(const CHandleRange& hr) const;

private:
    static bool x_IntersectingStrands(ENa_strand str1, ENa_strand str2);

    TRanges        m_Ranges;

    friend class CDataSource;
    friend class CHandleRangeMap;
};


inline
bool CHandleRange::Empty(void) const
{
    return m_Ranges.empty();
}


inline
CHandleRange::const_iterator CHandleRange::begin(void) const
{
    return m_Ranges.begin();
}


inline
CHandleRange::const_iterator CHandleRange::end(void) const
{
    return m_Ranges.end();
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.13  2003/01/22 20:11:54  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.12  2002/08/07 17:51:54  grichenk
* Removed debug message
*
* Revision 1.11  2002/07/31 16:52:36  grichenk
* Fixed bug in IntersectingWith()
*
* Revision 1.10  2002/07/08 20:51:01  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.9  2002/06/12 14:40:47  grichenk
* Made some methods inline
*
* Revision 1.8  2002/05/06 03:28:47  vakatov
* OM/OM1 renaming
*
* Revision 1.7  2002/05/03 21:28:10  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.6  2002/04/22 20:05:35  grichenk
* +MergeRange()
*
* Revision 1.5  2002/02/21 19:27:05  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.4  2002/02/15 20:35:38  gouriano
* changed implementation of HandleRangeMap
*
* Revision 1.3  2002/01/23 21:59:31  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.2  2002/01/16 16:25:58  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:20  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/

#endif  // HANDLE_RANGE__HPP
