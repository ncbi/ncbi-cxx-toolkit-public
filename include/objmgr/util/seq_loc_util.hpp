#ifndef SEQ_LOC_UTIL__HPP
#define SEQ_LOC_UTIL__HPP

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
* Author:  Clifford Clausen, Aaron Ucko, Aleksey Grichenko
*
* File Description:
*   Seq-loc utilities requiring CScope
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objmgr/objmgr_exception.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// Forward declarations
class CSeq_loc;
class CScope;
class CSeq_id_Handle;
class CSeq_id;
class CBioseq_Handle;

BEGIN_SCOPE(sequence)


// Get sequence length if scope not null, else return max possible TSeqPos
NCBI_XOBJUTIL_EXPORT
TSeqPos GetLength(const CSeq_id& id, CScope* scope = 0);

// Get length of sequence represented by CSeq_loc, if possible
NCBI_XOBJUTIL_EXPORT
TSeqPos GetLength(const CSeq_loc& loc, CScope* scope = 0)
    THROWS((CObjmgrUtilException));

// Get length of CSeq_loc_mix == sum (length of embedded CSeq_locs)
NCBI_XOBJUTIL_EXPORT
TSeqPos GetLength(const CSeq_loc_mix& mix, CScope* scope = 0)
    THROWS((CObjmgrUtilException));

// Checks that point >= 0 and point < length of Bioseq
NCBI_XOBJUTIL_EXPORT
bool IsValid(const CSeq_point& pt, CScope* scope = 0);

// Checks that all points >=0 and < length of CBioseq. If scope is 0
// assumes length of CBioseq is max value of TSeqPos.
NCBI_XOBJUTIL_EXPORT
bool IsValid(const CPacked_seqpnt& pts, CScope* scope = 0);

// Checks from and to of CSeq_interval. If from < 0, from > to, or
// to >= length of CBioseq this is an interval for, returns false, else true.
NCBI_XOBJUTIL_EXPORT
bool IsValid(const CSeq_interval& interval, CScope* scope = 0);

// Determines if two CSeq_ids represent the same CBioseq
NCBI_XOBJUTIL_EXPORT
bool IsSameBioseq(const CSeq_id& id1, const CSeq_id& id2, CScope* scope = 0);

// Returns true if all embedded CSeq_ids represent the same CBioseq, else false
NCBI_XOBJUTIL_EXPORT
bool IsOneBioseq(const CSeq_loc& loc, CScope* scope = 0);

// If all CSeq_ids embedded in CSeq_loc refer to the same CBioseq, returns
// the first CSeq_id found, else throws exception CNotUnique()
NCBI_XOBJUTIL_EXPORT
const CSeq_id& GetId(const CSeq_loc& loc, CScope* scope = 0)
    THROWS((CObjmgrUtilException));


// Returns eNa_strand_unknown if multiple Bioseqs in loc
// Returns eNa_strand_other if multiple strands in same loc
// Returns eNa_strand_both if loc is a Whole
// Returns strand otherwise
NCBI_XOBJUTIL_EXPORT
ENa_strand GetStrand(const CSeq_loc& loc, CScope* scope = 0);

// If only one CBioseq is represented by CSeq_loc, returns the lowest residue
// position represented. If not null, scope is used to determine if two
// CSeq_ids represent the same CBioseq. Throws exception CNotUnique if
// CSeq_loc does not represent one CBioseq.
NCBI_XOBJUTIL_EXPORT
TSeqPos GetStart(const CSeq_loc& loc, CScope* scope = 0)
    THROWS((CObjmgrUtilException));

// If only one CBioseq is represented by CSeq_loc, returns the highest residue
// position represented. If not null, scope is used to determine if two
// CSeq_ids represent the same CBioseq. Throws exception CNotUnique if
// CSeq_loc does not represent one CBioseq.
NCBI_XOBJUTIL_EXPORT
TSeqPos GetStop(const CSeq_loc& loc, CScope* scope = 0)
    THROWS((CObjmgrUtilException));


// Containment relationships between CSeq_locs
enum ECompare {
    eNoOverlap = 0, // CSeq_locs do not overlap
    eContained,     // First CSeq_loc contained by second
    eContains,      // First CSeq_loc contains second
    eSame,          // CSeq_locs contain each other
    eOverlap        // CSeq_locs overlap
};

// Returns the sequence::ECompare containment relationship between CSeq_locs
NCBI_XOBJUTIL_EXPORT
sequence::ECompare Compare(const CSeq_loc& loc1,
                           const CSeq_loc& loc2,
                           CScope* scope = 0);


// Change a CSeq_id to the one for the CBioseq that it represents
// that has the best rank or worst rank according on value of best.
// Just returns if scope == 0
NCBI_XOBJUTIL_EXPORT
void ChangeSeqId(CSeq_id* id, bool best, CScope* scope = 0);

// Change each of the CSeq_ids embedded in a CSeq_loc to the best
// or worst CSeq_id accoring to the value of best. Just returns if
// scope == 0
NCBI_XOBJUTIL_EXPORT
void ChangeSeqLocId(CSeq_loc* loc, bool best, CScope* scope = 0);

enum ESeqLocCheck {
    eSeqLocCheck_ok,
    eSeqLocCheck_warning,
    eSeqLocCheck_error
};

// Checks that a CSeq_loc is all on one strand on one CBioseq. For embedded 
// points, checks that the point location is <= length of sequence of point. 
// For packed points, checks that all points are within length of sequence. 
// For intervals, ensures from <= to and interval is within length of sequence.
// If no mixed strands and lengths are valid, returns eSeqLocCheck_ok. If
// only mixed strands/CBioseq error, then returns eSeqLocCheck_warning. If 
// length error, then returns eSeqLocCheck_error.
NCBI_XOBJUTIL_EXPORT
ESeqLocCheck SeqLocCheck(const CSeq_loc& loc, CScope* scope);

// Returns true if the order of Seq_locs is bad, otherwise, false
NCBI_XOBJUTIL_EXPORT
bool BadSeqLocSortOrder
(const CBioseq&  seq,
 const CSeq_loc& loc,
 CScope*         scope);


enum EOffsetType {
    // For positive-orientation strands, start = left and end = right;
    // for reverse-orientation strands, start = right and end = left.
    eOffset_FromStart, // relative to beginning of location
    eOffset_FromEnd,   // relative to end of location
    eOffset_FromLeft,  // relative to low-numbered end
    eOffset_FromRight  // relative to high-numbered end
};
// returns (TSeqPos)-1 if the locations don't overlap
NCBI_XOBJUTIL_EXPORT
TSeqPos LocationOffset(const CSeq_loc& outer, const CSeq_loc& inner,
                       EOffsetType how = eOffset_FromStart, CScope* scope = 0);

enum EOverlapType {
    eOverlap_Simple,         // any overlap of extremes
    eOverlap_Contained,      // 2nd contained within 1st extremes
    eOverlap_Contains,       // 2nd contains 1st extremes
    eOverlap_Subset,         // 2nd is a subset of 1st ranges
    eOverlap_CheckIntervals, // 2nd is a subset of 1st with matching boundaries
    eOverlap_Interval        // at least one pair of intervals must overlap
};

// Check if the two locations have ovarlap of a given type
// Calls x_TestForOverlap() and if the result is greater than kMax_Int
// truncates it to kMax_Int. To get the exact value use x_TestForOverlap().
NCBI_XOBJUTIL_EXPORT
int TestForOverlap(const CSeq_loc& loc1,
                   const CSeq_loc& loc2,
                   EOverlapType type,
                   TSeqPos circular_len = kInvalidSeqPos,
                   CScope* scope = 0);

NCBI_XOBJUTIL_EXPORT
Int8 x_TestForOverlap(const CSeq_loc& loc1,
                      const CSeq_loc& loc2,
                      EOverlapType type,
                      TSeqPos circular_len = kInvalidSeqPos,
                      CScope* scope = 0);

enum ESeqlocPartial {
    eSeqlocPartial_Complete   =   0,
    eSeqlocPartial_Start      =   1,
    eSeqlocPartial_Stop       =   2,
    eSeqlocPartial_Internal   =   4,
    eSeqlocPartial_Other      =   8,
    eSeqlocPartial_Nostart    =  16,
    eSeqlocPartial_Nostop     =  32,
    eSeqlocPartial_Nointernal =  64,
    eSeqlocPartial_Limwrong   = 128,
    eSeqlocPartial_Haderror   = 256
};
   

// Sets bits for incomplete location and/or errors
NCBI_XOBJUTIL_EXPORT
int SeqLocPartialCheck(const CSeq_loc& loc, CScope* scope);

enum ESeqLocFlags
{
    fMergeIntervals  = 1,    // merge overlapping intervals
    fFuseAbutting    = 2,    // fuse together abutting intervals
    fSingleInterval  = 4,    // create a single interval
    fAddNulls        = 8     // will add a null Seq-loc between intervals 
};
typedef unsigned int TSeqLocFlags;  // logical OR of ESeqLocFlags

// Merge two Seq-locs returning the merged location.
NCBI_XOBJUTIL_EXPORT
CSeq_loc* SeqLocMerge(const CBioseq_Handle& target,
                      const CSeq_loc& loc1, const CSeq_loc& loc2,
                      TSeqLocFlags flags = 0);

// Merge a single Seq-loc
NCBI_XOBJUTIL_EXPORT
CSeq_loc* SeqLocMergeOne(const CBioseq_Handle& target,
                        const CSeq_loc& loc,
                        TSeqLocFlags flags = 0);

// Merge a set of locations, returning the result.
template<typename LocContainer>
CSeq_loc* SeqLocMerge(const CBioseq_Handle& target,
                      LocContainer& locs,
                      TSeqLocFlags flags = 0)
{
    // create a single Seq-loc holding all the locations
    CSeq_loc temp;
    ITERATE( typename LocContainer, it, locs ) {
        temp.Add(**it);
    }
    return SeqLocMergeOne(target, temp, flags);
}


// All operations create and return a new seq-loc object.
// Optional scope or synonym mapper may be provided to detect and convert
// synonyms of a bioseq.

// Merge ranges in a seq-loc
NCBI_XOBJUTIL_EXPORT
CRef<CSeq_loc> Seq_loc_Merge(const CSeq_loc& loc,
                             CSeq_loc::TOpFlags flags,
                             CScope* scope);

// Add two seq-locs
NCBI_XOBJUTIL_EXPORT
CRef<CSeq_loc> Seq_loc_Add(const CSeq_loc& loc1,
                           const CSeq_loc& loc2,
                           CSeq_loc::TOpFlags flags,
                           CScope* scope);

NCBI_XOBJUTIL_EXPORT
CRef<CSeq_loc> Seq_loc_Subtract(const CSeq_loc& loc1,
                                const CSeq_loc& loc2,
                                CSeq_loc::TOpFlags flags,
                                CScope* scope);


NCBI_XOBJUTIL_EXPORT
CSeq_loc* SeqLocRevCmp(const CSeq_loc& loc, CScope* scope = 0);

END_SCOPE(sequence)
END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.3  2004/11/17 21:25:13  grichenk
* Moved seq-loc related functions to seq_loc_util.[hc]pp.
* Replaced CNotUnique and CNoLength exceptions with CObjmgrUtilException.
*
* Revision 1.2  2004/11/15 15:07:57  grichenk
* Moved seq-loc operations to CSeq_loc, modified flags.
*
* Revision 1.1  2004/10/20 18:09:43  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#endif  /* SEQ_LOC_UTIL__HPP */
