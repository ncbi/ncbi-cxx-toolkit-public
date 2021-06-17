#ifndef OBJECTS_OBJMGR_IMPL___HANDLE_RANGE__HPP
#define OBJECTS_OBJMGR_IMPL___HANDLE_RANGE__HPP

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
 *
 */

#include <objects/seqloc/Na_strand.hpp>
#include <util/range.hpp>
#include <vector>
#include <utility>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// Locations list for a particular bioseq handle
class NCBI_XOBJMGR_EXPORT CHandleRange
{
public:
    typedef CRange<TSeqPos> TRange;
    typedef COpenRange<TSeqPos> TOpenRange;
    typedef pair<TRange, ENa_strand> TRangeWithStrand;
    typedef vector<TRangeWithStrand> TRanges;
    typedef TRanges::const_iterator const_iterator;

    CHandleRange(void);
    /// Trim src with filter range
    CHandleRange(const CHandleRange& src, const TOpenRange& range);
    ~CHandleRange(void);

    bool Empty(void) const;
    const_iterator begin(void) const;
    const_iterator end(void) const;

    // Add a new range
    void AddRange(TRange range, ENa_strand strand/* = eNa_strand_unknown*/);
    void AddRange(TRange range, ENa_strand strand,
                  bool more_before, bool more_after);
    // Merge a new range with the existing ranges
    void MergeRange(TRange range, ENa_strand strand);

    void AddRanges(const CHandleRange& hr);

    // return true if there is a gap between some intervals
    bool HasGaps(void) const;

    bool IsMultipart(void) const;
    bool IsCircular(void) const;
    bool IsSingleStrand(void) const;

    // Get the range including all ranges in the list (with any strand)
    enum ETotalRangeFlags {
        eStrandPlus    = 1 << 0,
        eStrandMinus   = 1 << 1,
        eStrandAny     = eStrandPlus | eStrandMinus
    };
    typedef unsigned int TTotalRangeFlags;

    // Return strands flag
    TTotalRangeFlags GetStrandsFlag(void) const;

    TRange GetOverlappingRange(TTotalRangeFlags flags = eStrandAny) const;

    // Leftmost and rightmost points of the total range ragardless of strand
    TSeqPos GetLeft(void) const;
    TSeqPos GetRight(void) const;

    // Ranges for circular locations
    // valid only when IsCircular() returns true
    // return first part of a circular location up to origin
    TRange GetCircularRangeStart(bool include_origin = true) const;
    // return second part of a circular location starting from origin
    TRange GetCircularRangeEnd(bool include_origin = true) const;
    
    // Get the range including all ranges in the list which (with any strand)
    // filter the list through 'range' argument
    TRange GetOverlappingRange(const TRange& range) const;

    // Check if the two sets of ranges do intersect
    bool IntersectingWith(const CHandleRange& hr) const;

    // Check if the two sets of ranges do intersect by total range
    bool IntersectingWithTotalRange(const CHandleRange& hr) const;

    // Check if the two sets of ranges do intersect by individual subranges
    bool IntersectingWithSubranges(const CHandleRange& hr) const;

    // Check if the two sets of ranges do intersect ignoring strands
    bool IntersectingWith_NoStrand(const CHandleRange& hr) const;

    // Check if the two sets of ranges do intersect
    bool IntersectingWith(const TRange& range,
                          ENa_strand strand = eNa_strand_unknown) const;

private:
    // Strand checking methods
    bool x_IncludesPlus(const ENa_strand& strand) const;
    bool x_IncludesMinus(const ENa_strand& strand) const;

    static bool x_IntersectingStrands(ENa_strand str1, ENa_strand str2);

    TRanges        m_Ranges;
    TRange         m_TotalRanges_plus;
    TRange         m_TotalRanges_minus;
    bool           m_IsCircular;
    bool           m_IsSingleStrand;
    bool           m_MoreBefore;
    bool           m_MoreAfter;

    // friend class CDataSource;
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


inline
bool CHandleRange::IsMultipart(void) const
{
    return m_IsCircular || !m_IsSingleStrand;
}


inline
bool CHandleRange::IsCircular(void) const
{
    return m_IsCircular;
}


inline
bool CHandleRange::IsSingleStrand(void) const
{
    return m_IsSingleStrand;
}


inline
bool CHandleRange::x_IncludesPlus(const ENa_strand& strand) const
{
    // Anything but "minus" includes "plus"
    return strand != eNa_strand_minus;
}


inline
bool CHandleRange::x_IncludesMinus(const ENa_strand& strand) const
{
    return strand == eNa_strand_unknown
        || strand == eNa_strand_minus
        ||  strand == eNa_strand_both
        ||  strand == eNa_strand_both_rev;
}


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // OBJECTS_OBJMGR_IMPL___HANDLE_RANGE__HPP
