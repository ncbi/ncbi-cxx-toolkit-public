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
* ---------------------------------------------------------------------------
* $Log$
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

#include <objects/objmgr1/bioseq_handle.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <util/range.hpp>
#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// Locations list for a particular bioseq handle
class CHandleRange
{
public:
    typedef CRange<int> TRange;
    typedef pair<TRange, ENa_strand> TRangeWithStrand;
    typedef list<TRangeWithStrand> TRanges;

    CHandleRange(const CSeq_id_Handle& handle);
    CHandleRange(const CHandleRange& hrange);
    ~CHandleRange(void);

    CHandleRange& operator= (const CHandleRange& hrange);

    // Get the list of ranges
    const TRanges& GetRanges(void) const { return m_Ranges; }
    // Add a new range
    void AddRange(TRange range, ENa_strand strand/* = eNa_strand_unknown*/);
    // Get the range including all ranges in the list (with any strand)
    TRange GetOverlappingRange(void) const;
    // Check if the two sets of ranges do intersect
    bool IntersectingWith(const CHandleRange& hloc) const;

private:
    static void x_CombineRanges(TRange& dest, const TRange& src);
    static bool x_IntersectingStrands(ENa_strand str1, ENa_strand str2);

    CSeq_id_Handle m_Handle;
    TRanges        m_Ranges;

    friend class CDataSource;
    friend class CHandleRangeMap;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // HANDLE_RANGE__HPP
