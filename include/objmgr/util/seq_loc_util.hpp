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
#include <objmgr/scope.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// Forward declarations
class CSeq_loc;
class CSeq_id_Handle;
class CSeq_id;
class CBioseq_Handle;

BEGIN_SCOPE(sequence)


/** @addtogroup ObjUtilSeqLoc
 *
 * @{
 */


/** @name Basic information
 * Basic seq-loc information and verification
 * @{
 */

/// Get sequence length if scope not null, else return max possible TSeqPos
NCBI_XOBJUTIL_EXPORT
TSeqPos GetLength(const CSeq_id& id, CScope* scope);

/// Get length of sequence represented by CSeq_loc, if possible
NCBI_XOBJUTIL_EXPORT
TSeqPos GetLength(const CSeq_loc& loc, CScope* scope);

/// Get number of unique bases in the location
NCBI_XOBJUTIL_EXPORT
TSeqPos GetCoverage(const CSeq_loc& loc, CScope* scope);

/// Get length of CSeq_loc_mix == sum (length of embedded CSeq_locs)
NCBI_XOBJUTIL_EXPORT
TSeqPos GetLength(const CSeq_loc_mix& mix, CScope* scope);

/// Checks that point >= 0 and point < length of Bioseq
NCBI_XOBJUTIL_EXPORT
bool IsValid(const CSeq_point& pt, CScope* scope);

/// Checks that all points >=0 and < length of CBioseq. If scope is 0
/// assumes length of CBioseq is max value of TSeqPos.
NCBI_XOBJUTIL_EXPORT
bool IsValid(const CPacked_seqpnt& pts, CScope* scope);

/// Checks from and to of CSeq_interval. If from < 0, from > to, or
/// to >= length of CBioseq this is an interval for, returns false, else true.
NCBI_XOBJUTIL_EXPORT
bool IsValid(const CSeq_interval& interval, CScope* scope);

/// Determines if two CSeq_ids represent the same CBioseq
NCBI_XOBJUTIL_EXPORT
bool IsSameBioseq(const CSeq_id& id1, const CSeq_id& id2, CScope* scope,
                  CScope::EGetBioseqFlag get_flag = CScope::eGetBioseq_All);
NCBI_XOBJUTIL_EXPORT
bool IsSameBioseq(const CSeq_id_Handle& id1, const CSeq_id_Handle& id2, CScope* scope,
                  CScope::EGetBioseqFlag get_flag = CScope::eGetBioseq_All);

/// Returns true if all embedded CSeq_ids represent the same CBioseq, else false
NCBI_XOBJUTIL_EXPORT
bool IsOneBioseq(const CSeq_loc& loc, CScope* scope);

/// If all CSeq_ids embedded in CSeq_loc refer to the same CBioseq, returns
/// the first CSeq_id found, else throws CObjmgrUtilException exception.
NCBI_XOBJUTIL_EXPORT
const CSeq_id& GetId(const CSeq_loc& loc, CScope* scope);
NCBI_XOBJUTIL_EXPORT
CSeq_id_Handle GetIdHandle(const CSeq_loc& loc, CScope* scope);


/// Returns eNa_strand_unknown if multiple Bioseqs in loc
/// Returns eNa_strand_other if multiple strands in same loc
/// Returns eNa_strand_both if loc is a Whole
/// Returns strand otherwise
NCBI_XOBJUTIL_EXPORT
ENa_strand GetStrand(const CSeq_loc& loc, CScope* scope = 0);

/// If only one CBioseq is represented by CSeq_loc, returns the position at the
/// start of the location. By defulat this is the lowest residue position
/// represented by the location.
/// If not null, scope is used to determine if two
/// CSeq_ids represent the same CBioseq. Throws CObjmgrUtilException if
/// CSeq_loc does not represent one CBioseq.
NCBI_XOBJUTIL_EXPORT
TSeqPos GetStart(const CSeq_loc& loc, CScope* scope,
                 ESeqLocExtremes ext = eExtreme_Positional);

/// If only one CBioseq is represented by CSeq_loc, returns the position at the
/// stop of the location. By defualt this is the highest residue position
/// represented by the location.
/// If not null, scope is used to determine if two
/// CSeq_ids represent the same CBioseq. Throws CObjmgrUtilException exception
/// if CSeq_loc does not represent one CBioseq.
NCBI_XOBJUTIL_EXPORT
TSeqPos GetStop(const CSeq_loc& loc, CScope* scope,
                ESeqLocExtremes ext = eExtreme_Positional);


/// SeqLocCheck results
enum ESeqLocCheck {
    eSeqLocCheck_ok,
    eSeqLocCheck_warning,
    eSeqLocCheck_error
};

/// Checks that a CSeq_loc is all on one strand on one CBioseq. For embedded 
/// points, checks that the point location is <= length of sequence of point. 
/// For packed points, checks that all points are within length of sequence. 
/// For intervals, ensures from <= to and interval is within length of sequence.
/// If no mixed strands and lengths are valid, returns eSeqLocCheck_ok. If
/// only mixed strands/CBioseq error, then returns eSeqLocCheck_warning. If 
/// length error, then returns eSeqLocCheck_error.
NCBI_XOBJUTIL_EXPORT
ESeqLocCheck SeqLocCheck(const CSeq_loc& loc, CScope* scope);

/// Returns true if the order of Seq_locs is bad, otherwise, false
NCBI_XOBJUTIL_EXPORT
bool BadSeqLocSortOrder(const CBioseq_Handle& bsh,
                        const CSeq_loc&       loc);
NCBI_XOBJUTIL_EXPORT
bool BadSeqLocSortOrder(const CBioseq&  seq,
                        const CSeq_loc& loc,
                        CScope*         scope);

/* @} */


/** @name Compare
 * Containment relationships between CSeq_locs
 * @{
 */

enum ECompare {
    eNoOverlap = 0, ///< CSeq_locs do not overlap
    eContained,     ///< First CSeq_loc contained by second
    eContains,      ///< First CSeq_loc contains second
    eSame,          ///< CSeq_locs contain each other
    eOverlap        ///< CSeq_locs overlap
};

/// Returns the sequence::ECompare containment relationship between CSeq_locs
NCBI_XOBJUTIL_EXPORT
sequence::ECompare Compare(const CSeq_loc& loc1,
                           const CSeq_loc& loc2,
                           CScope*         scope);

/* @} */


/** @name Change id
 * Replace seq-id with the best or worst rank
 * @{
 */

/// Change a CSeq_id to the one for the CBioseq that it represents
/// that has the best rank or worst rank according on value of best.
/// Just returns if scope == 0
NCBI_XOBJUTIL_EXPORT
void ChangeSeqId(CSeq_id* id, bool best, CScope* scope);

/// Change each of the CSeq_ids embedded in a CSeq_loc to the best
/// or worst CSeq_id accoring to the value of best. Just returns if
/// scope == 0
NCBI_XOBJUTIL_EXPORT
void ChangeSeqLocId(CSeq_loc* loc, bool best, CScope* scope);

/* @} */


/** @name Overlapping
 * Overlapping of seq-locs
 * @{
 */

enum EOffsetType {
    /// For positive-orientation strands, start = left and end = right;
    /// for reverse-orientation strands, start = right and end = left.
    eOffset_FromStart, ///< relative to beginning of location
    eOffset_FromEnd,   ///< relative to end of location
    eOffset_FromLeft,  ///< relative to low-numbered end
    eOffset_FromRight  ///< relative to high-numbered end
};

/// returns (TSeqPos)-1 if the locations don't overlap
NCBI_XOBJUTIL_EXPORT
TSeqPos LocationOffset(const CSeq_loc& outer, const CSeq_loc& inner,
                       EOffsetType how = eOffset_FromStart, CScope* scope = 0);

enum EOverlapType {
    eOverlap_Simple,         ///< any overlap of extremes
    eOverlap_Contained,      ///< 2nd contained within 1st extremes
    eOverlap_Contains,       ///< 2nd contains 1st extremes
    eOverlap_Subset,         ///< 2nd is a subset of 1st ranges
    eOverlap_SubsetRev,      ///< 1st is a subset of 2nd ranges
    eOverlap_CheckIntervals, ///< 2nd is a subset of 1st with matching boundaries
    eOverlap_CheckIntRev,    ///< 1st is a subset of 2nd with matching boundaries
    eOverlap_Interval        ///< at least one pair of intervals must overlap
};

/// 64-bit version of TestForOverlap()
/// Check if the two locations have ovarlap of a given type.
/// Return quality of the overlap: lower values mean better overlapping.
/// 0 = exact match of the ranges, -1 = no overlap.
NCBI_XOBJUTIL_EXPORT
Int8 TestForOverlap64(const CSeq_loc& loc1,
                      const CSeq_loc& loc2,
                      EOverlapType    type,
                      TSeqPos         circular_len = kInvalidSeqPos,
                      CScope*         scope = 0);

/// Flags, controlling behavior of TestForOverlapEx().
enum EOverlapFlags {
    fOverlap_NoMultiSeq     = 1 << 0, ///< Throw if locations reference multiple bioseqs
    fOverlap_NoMultiStrand  = 1 << 1, ///< Throw if locations reference multiple strands
    fOverlap_IgnoreTopology = 1 << 2, ///< Ignore sequence topology (circularity)
    fOverlap_Default = 0              ///< Enable multi-id, multi-strand, check topology
};


/// Updated version of TestForOverlap64(). Allows more control over
/// handling multi-id/multi-strand bioseqs.
/// Return quality of the overlap: lower values mean better overlapping.
/// 0 = exact match of the ranges, -1 = no overlap.
NCBI_XOBJUTIL_EXPORT
Int8 TestForOverlapEx(const CSeq_loc& loc1,
                      const CSeq_loc& loc2,
                      EOverlapType    type,
                      CScope*         scope = 0,
                      EOverlapFlags   flags = fOverlap_Default);

/// Calls TestForOverlap64() and if the result is greater than kMax_Int
/// truncates it to kMax_Int. To get the exact value use TestForOverlap64().
NCBI_XOBJUTIL_EXPORT
int TestForOverlap(const CSeq_loc& loc1,
                   const CSeq_loc& loc2,
                   EOverlapType    type,
                   TSeqPos         circular_len = kInvalidSeqPos,
                   CScope*         scope = 0);

/* @} */


/** @name PartialCheck
 * Sets bits for incomplete location and/or errors
 * @{
 */

enum ESeqlocPartial {
    eSeqlocPartial_Complete   = 0,
    eSeqlocPartial_Start      = 1<<0,
    eSeqlocPartial_Stop       = 1<<1,
    eSeqlocPartial_Internal   = 1<<2,
    eSeqlocPartial_Other      = 1<<3,
    eSeqlocPartial_Nostart    = 1<<4,
    eSeqlocPartial_Nostop     = 1<<5,
    eSeqlocPartial_Nointernal = 1<<6,
    eSeqlocPartial_Limwrong   = 1<<7,
    eSeqlocPartial_Haderror   = 1<<8
};
   
NCBI_XOBJUTIL_EXPORT
int SeqLocPartialCheck(const CSeq_loc& loc, CScope* scope);

/* @} */


/// Get reverse complement of the seq-loc (?)
NCBI_XOBJUTIL_EXPORT
CSeq_loc* SeqLocRevCmp(const CSeq_loc& loc, CScope* scope);


/** @name Operations
 * Seq-loc operations
 * All operations create and return a new seq-loc object.
 * Optional scope or synonym mapper may be provided to detect and convert
 * synonyms of a bioseq.
 * @{
 */

/// Merge ranges in the seq-loc
NCBI_XOBJUTIL_EXPORT
CRef<CSeq_loc> Seq_loc_Merge(const CSeq_loc&    loc,
                             CSeq_loc::TOpFlags flags,
                             CScope*            scope);

/// Merge multiple locations
template<typename TSeq_loc_Set>
CSeq_loc* Seq_locs_Merge(TSeq_loc_Set&      locs,
                         CSeq_loc::TOpFlags flags,
                         CScope*            scope)
{
    // create a single Seq-loc holding all the locations
    CSeq_loc temp;
    ITERATE(typename TSeq_loc_Set, it, locs) {
        temp.Add(**it);
    }
    return Seq_loc_Merge(temp, flags, scope);
}

/// Add two seq-locs
NCBI_XOBJUTIL_EXPORT
CRef<CSeq_loc> Seq_loc_Add(const CSeq_loc&    loc1,
                           const CSeq_loc&    loc2,
                           CSeq_loc::TOpFlags flags,
                           CScope*            scope);

/// Subtract the second seq-loc from the first one
NCBI_XOBJUTIL_EXPORT
CRef<CSeq_loc> Seq_loc_Subtract(const CSeq_loc&    loc1,
                                const CSeq_loc&    loc2,
                                CSeq_loc::TOpFlags flags,
                                CScope*            scope);

/* @} */


END_SCOPE(sequence)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* SEQ_LOC_UTIL__HPP */
