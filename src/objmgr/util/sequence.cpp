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
* Author:  Clifford Clausen
*
* File Description:
*   Sequence utilities requiring CScope
*/

#include <serial/iterator.hpp>

#include <objects/objmgr/object_manager.hpp>
#include <objects/objmgr/scope.hpp>
#include <objects/objmgr/seq_vector.hpp>

#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seg_ext.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seqloc/Packed_seqpnt.hpp>
#include <objects/seqloc/Seq_bond.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seqloc/Seq_loc_mix.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <objects/util/sequence.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(sequence)


TSeqPos GetLength(const CSeq_id& id, CScope* scope)
{
    if ( !scope ) {
        return numeric_limits<TSeqPos>::max();
    }
    CBioseq_Handle hnd = scope->GetBioseqHandle(id);
    CBioseq_Handle::TBioseqCore core = hnd.GetBioseqCore();
    return core->GetInst().IsSetLength() ? core->GetInst().GetLength() :
        numeric_limits<TSeqPos>::max();    
}


TSeqPos GetLength(const CSeq_loc& loc, CScope* scope) 
    THROWS((CNoLength))
{
    switch (loc.Which()) {
    case CSeq_loc::e_Pnt:
        return 1;
    case CSeq_loc::e_Int:
        return loc.GetInt().GetLength();
    case CSeq_loc::e_Null:
    case CSeq_loc::e_Empty:
        return 0;
    case CSeq_loc::e_Whole:
        return GetLength(loc.GetWhole(), scope);
    case CSeq_loc::e_Packed_int:
        return loc.GetPacked_int().GetLength();
    case CSeq_loc::e_Mix:
        return GetLength(loc.GetMix(), scope);
    case CSeq_loc::e_Packed_pnt:   // just a bunch of points
        return loc.GetPacked_pnt().GetPoints().size();
    case CSeq_loc::e_not_set:
    case CSeq_loc::e_Bond:         //can't calculate length
    case CSeq_loc::e_Feat:
    case CSeq_loc::e_Equiv:        // unless actually the same length...
    default:
        THROW0_TRACE(CNoLength());
    }
}


TSeqPos GetLength(const CSeq_loc_mix& mix, CScope* scope)
    THROWS((CNoLength))
{
    TSeqPos length = 0;

    iterate( CSeq_loc_mix::Tdata, i, mix.Get() ) {
        TSeqPos ret = GetLength((**i), scope);
        length += ret;
    }
    return length;
}


bool IsValid(const CSeq_point& pt, CScope* scope)
{
    if (static_cast<TSeqPos>(pt.GetPoint()) >=
         GetLength(pt.GetId(), scope) ) 
    {
        return false;
    }
    
    return true;
}


bool IsValid(const CPacked_seqpnt& pts, CScope* scope)
{
    typedef CPacked_seqpnt::TPoints TPoints;
    
    TSeqPos length = GetLength(pts.GetId(), scope);
    iterate (TPoints, it, pts.GetPoints()) {
        if (*it >= length) {
            return false;
        }
    }
    return true;
}


bool IsValid(const CSeq_interval& interval, CScope* scope)
{
    if (interval.GetFrom() > interval.GetTo() ||
        interval.GetTo() >= GetLength(interval.GetId(), scope))
    {
        return false;
    }
    
    return true;
}


bool IsSameBioseq (const CSeq_id& id1, const CSeq_id& id2, CScope* scope)
{
    // Compare CSeq_ids directly
    if (id1.Compare(id2) == CSeq_id::e_YES) {
        return true;
    }
    
    // Compare handles
    if ( scope ) {
        try {
            CBioseq_Handle hnd1 = scope->GetBioseqHandle(id1);
            CBioseq_Handle hnd2 = scope->GetBioseqHandle(id2);
            return hnd1  &&  (hnd1 == hnd2);
        } catch (runtime_error& e) {
            ERR_POST(e.what() << ": CSeq_id1: " << id1.DumpAsFasta() 
                     << ": CSeq_id2: " << id2.DumpAsFasta());
            return false;
        }
    }
    
    return false;
}


bool IsOneBioseq(const CSeq_loc& loc, CScope* scope)
{
    try {
        GetId(loc, scope);
        return true;
    } catch (CNotUnique& e) {
        return false;
    }
}


const CSeq_id& GetId(const CSeq_loc& loc, CScope* scope)
    THROWS((CNotUnique))
{
    CTypeConstIterator<CSeq_id> cur = ConstBegin(loc);
    CTypeConstIterator<CSeq_id> first = ConstBegin(loc);
    
    if (!first) {
        THROW0_TRACE(CNotUnique());
    }
    
    for (++cur; cur; ++cur) {
        if ( !IsSameBioseq(*cur, *first, scope) ) {
            THROW0_TRACE(CNotUnique());
        }
    }
    return *first; 
}


TSeqPos GetStart(const CSeq_loc& loc, CScope* scope) 
    THROWS((CNotUnique))
{
    // Throw CNotUnique if loc does not represent one CBioseq
    GetId(loc, scope);
    
    CSeq_loc::TRange rg = loc.GetTotalRange();
    return rg.GetFrom();
}


/*
*******************************************************************************
                        Implementation of Compare()
*******************************************************************************
*/


ECompare s_Compare
(const CSeq_loc&,
 const CSeq_loc&,
 CScope*);

int s_RetA[5][5] = {
    { 0, 4, 2, 2, 4 },
    { 4, 1, 4, 1, 4 },
    { 2, 4, 2, 2, 4 },
    { 2, 1, 2, 3, 4 },
    { 4, 4, 4, 4, 4 }
};


int s_RetB[5][5] = {
    { 0, 1, 4, 1, 4 },
    { 1, 1, 4, 1, 1 },
    { 4, 4, 2, 2, 4 },
    { 1, 1, 4, 3, 4 },
    { 4, 1, 4, 4, 4 }
};


// Returns the complement of an ECompare value
inline
ECompare s_Complement(ECompare cmp)
{
    switch ( cmp ) {
    case eContains:
        return eContained;
    case eContained:
        return eContains;
    default:
        return cmp;
    }
}


// Compare two Whole sequences
inline
ECompare s_Compare
(const CSeq_id& id1,
 const CSeq_id& id2,
 CScope*        scope)
{
    // If both sequences from same CBioseq, they are the same
    if ( IsSameBioseq(id1, id2, scope) ) {
        return eSame;
    } else {
        return eNoOverlap;
    }
}


// Compare Whole sequence with a Bond
inline
ECompare s_Compare
(const CSeq_id&   id,
 const CSeq_bond& bond,
 CScope*          scope)
{
    unsigned int count = 0;

    // Increment count if bond point A is from same CBioseq as id
    if ( IsSameBioseq(id, bond.GetA().GetId(), scope) ) {
        ++count;
    }

    // Increment count if second point in bond is set and is from
    // same CBioseq as id.
    if (bond.IsSetB()  &&  IsSameBioseq(id, bond.GetB().GetId(), scope)) {
        ++count;
    }

    switch ( count ) {
    case 1:
        if ( bond.IsSetB() ) {
            // One of two bond points are from same CBioseq as id --> overlap
            return eOverlap;
        } else {
            // There is only one bond point set --> id contains bond
            return eContains;
        }
    case 2:
        // Both bond points are from same CBioseq as id --> id contains bond
        return eContains;
    default:
        // id and bond do not overlap
        return eNoOverlap;
    }
}


// Compare Whole sequence with an interval
inline
ECompare s_Compare
(const CSeq_id&       id,
 const CSeq_interval& interval,
 CScope*              scope)
{
    // If interval is from same CBioseq as id and interval is
    // [0, length of id-1], then they are the same. If interval is from same
    // CBioseq as id, but interval is not [0, length of id -1] then id
    // contains seqloc.
    if ( IsSameBioseq(id, interval.GetId(), scope) ) {
        if (interval.GetFrom() == 0  &&
            interval.GetTo()   == GetLength(id, scope) - 1) {
            return eSame;
        } else {
            return eContains;
        }
    } else {
        return eNoOverlap;
    }
}


// Compare Whole sequence with a point
inline
ECompare s_Compare
(const CSeq_id&     id,
 const CSeq_point&  point,
 CScope*            scope)
{
    // If point from the same CBioseq as id, then id contains point
    if ( IsSameBioseq(id, point.GetId(), scope) ) {
        return eContains;
    } else {
        return eNoOverlap;
    }
}


// Compare Whole sequence with packed points
inline
ECompare s_Compare
(const CSeq_id&        id,
 const CPacked_seqpnt& packed,
 CScope*               scope)
{
    // If packed from same CBioseq as id, then id contains packed
    if ( IsSameBioseq(id, packed.GetId(), scope) ) {
        return eContains;
    } else {
        return eNoOverlap;
    }
}


// Compare an interval with a bond
inline
ECompare s_Compare
(const CSeq_interval& interval,
 const CSeq_bond&     bond,
 CScope*              scope)
{
    unsigned int count = 0;

    // Increment count if interval contains the first point of the bond
    if (IsSameBioseq(interval.GetId(), bond.GetA().GetId(), scope)  &&
        interval.GetFrom() <= bond.GetA().GetPoint()  &&
        interval.GetTo()   >= bond.GetA().GetPoint()) {
        ++count;
    }

    // Increment count if the second point of the bond is set and the
    // interval contains the second point.
    if (bond.IsSetB()  &&
        IsSameBioseq(interval.GetId(), bond.GetB().GetId(), scope)  &&
        interval.GetFrom() <= bond.GetB().GetPoint()  &&
        interval.GetTo()   >= bond.GetB().GetPoint()) {
        ++count;
    }

    switch ( count ) {
    case 1:
        if ( bond.IsSetB() ) {
            // The 2nd point of the bond is set
            return eOverlap;
        } else {
            // The 2nd point of the bond is not set
            return eContains;
        }
    case 2:
        // Both points of the bond are in the interval
        return eContains;
    default:
        return eNoOverlap;
    }
}


// Compare a bond with an interval
inline
ECompare s_Compare
(const CSeq_bond&     bond,
 const CSeq_interval& interval,
 CScope*              scope)
{
    // Just return the complement of comparing an interval with a bond
    return s_Complement(s_Compare(interval, bond, scope));
}


// Compare an interval to an interval
inline
ECompare s_Compare
(const CSeq_interval& intA,
 const CSeq_interval& intB,
 CScope*              scope)
{
    // If the intervals are not on the same bioseq, then there is no overlap
    if ( !IsSameBioseq(intA.GetId(), intB.GetId(), scope) ) {
        return eNoOverlap;
    }

    // Compare two intervals
    TSeqPos fromA = intA.GetFrom();
    TSeqPos toA   = intA.GetTo();
    TSeqPos fromB = intB.GetFrom();
    TSeqPos toB   = intB.GetTo();

    if (fromA == fromB  &&  toA == toB) {
        // End points are the same --> the intervals are the same.
        return eSame;
    } else if (fromA <= fromB  &&  toA >= toB) {
        // intA contains intB
        return eContains;
    } else if (fromA >= fromB  &&  toA <= toB) {
        // intB contains intA
        return eContained;
    } else if (fromA > toB  ||  toA < fromB) {
        // No overlap
        return eNoOverlap;
    } else {
        // The only possibility left is overlap
        return eOverlap;
    }
}


// Compare an interval and a point
inline
ECompare s_Compare
(const CSeq_interval& interval,
 const CSeq_point&    point,
 CScope*              scope)
{
    // If not from same CBioseq, then no overlap
    if ( !IsSameBioseq(interval.GetId(), point.GetId(), scope) ) {
        return eNoOverlap;
    }

    // If point in interval, then interval contains point
    TSeqPos pnt = point.GetPoint();
    if (interval.GetFrom() <= pnt  &&  interval.GetTo() >= pnt ) {
        return eContains;
    }

    // Only other possibility
    return eNoOverlap;
}


// Compare a point and an interval
inline
ECompare s_Compare
(const CSeq_point&    point,
 const CSeq_interval& interval,
 CScope*              scope)
{
    // The complement of comparing an interval with a point.
    return s_Complement(s_Compare(interval, point, scope));
}


// Compare a point with a point
inline
ECompare s_Compare
(const CSeq_point& pntA,
 const CSeq_point& pntB,
 CScope*           scope)
{
    // If points are on same bioseq and coordinates are the same, then same.
    if (IsSameBioseq(pntA.GetId(), pntB.GetId(), scope)  &&
        pntA.GetPoint() == pntB.GetPoint() ) {
        return eSame;
    }

    // Otherwise they don't overlap
    return eNoOverlap;
}


// Compare a point with packed point
inline
ECompare s_Compare
(const CSeq_point&     point,
 const CPacked_seqpnt& points,
 CScope*               scope)
{
    // If on the same bioseq, and any of the packed points are the
    // same as point, then the point is contained in the packed point
    if ( IsSameBioseq(point.GetId(), points.GetId(), scope) ) {
        TSeqPos pnt = point.GetPoint();

        // This loop will only be executed if points.GetPoints().size() > 0
        iterate(CSeq_loc::TPoints, it, points.GetPoints()) {
            if (pnt == *it) {
                return eContained;
            }
        }
    }

    // Otherwise, no overlap
    return eNoOverlap;
}


// Compare a point with a bond
inline
ECompare s_Compare
(const CSeq_point& point,
 const CSeq_bond&  bond,
 CScope*           scope)
{
    unsigned int count = 0;

    // If the first point of the bond is on the same CBioseq as point
    // and the point coordinates are the same, increment count by 1
    if (IsSameBioseq(point.GetId(), bond.GetA().GetId(), scope)  &&
        bond.GetA().GetPoint() == point.GetPoint()) {
        ++count;
    }

    // If the second point of the bond is set, the points are from the
    // same CBioseq, and the coordinates of the points are the same,
    // increment the count by 1.
    if (bond.IsSetB()  &&
        IsSameBioseq(point.GetId(), bond.GetB().GetId(), scope)  &&
        bond.GetB().GetPoint() == point.GetPoint()) {
        ++count;
    }

    switch ( count ) {
    case 1:
        if ( bond.IsSetB() ) {
            // The second point of the bond is set -- overlap
            return eOverlap;
            // The second point of the bond is not set -- same
        } else {
            return eSame;
        }
    case 2:
        // Unlikely case -- can happen if both points of the bond are the same
        return eSame;
    default:
        // All that's left is no overlap
        return eNoOverlap;
    }
}


// Compare packed point with an interval
inline
ECompare s_Compare
(const CPacked_seqpnt& points,
 const CSeq_interval&  interval,
 CScope*               scope)
{
    // If points and interval are from same bioseq and some points are
    // in interval, then overlap. If all points are in interval, then
    // contained. Else, no overlap.
    if ( IsSameBioseq(points.GetId(), interval.GetId(), scope) ) {
        bool    got_one    = false;
        bool    missed_one = false;
        TSeqPos from = interval.GetFrom();
        TSeqPos to   = interval.GetTo();

        // This loop will only be executed if points.GetPoints().size() > 0
        iterate(CSeq_loc::TPoints, it, points.GetPoints()) {
            if (from <= *it  &&  to >= *it) {
                got_one = true;
            } else {
                missed_one = true;
            }
            if (got_one  &&  missed_one) {
                break;
            }
        }

        if ( !got_one ) {
            return eNoOverlap;
        } else if ( missed_one ) {
            return eOverlap;
        } else {
            return eContained;
        }
    }

    return eNoOverlap;
}


// Compare two packed points
inline
ECompare s_Compare
(const CPacked_seqpnt& pntsA,
 const CPacked_seqpnt& pntsB,
 CScope*               scope)
{
    // If CPacked_seqpnts from different CBioseqs, then no overlap
    if ( !IsSameBioseq(pntsA.GetId(), pntsB.GetId(), scope) ) {
        return eNoOverlap;
    }

    const CSeq_loc::TPoints& pointsA = pntsA.GetPoints();
    const CSeq_loc::TPoints& pointsB = pntsB.GetPoints();

    // Check for equality. Note order of points is significant
    if (pointsA.size() == pointsB.size()) {
        CSeq_loc::TPoints::const_iterator iA = pointsA.begin();
        CSeq_loc::TPoints::const_iterator iB = pointsB.begin();
        bool check_containtment = false;
        // This loop will only be executed if pointsA.size() > 0
        while (iA != pointsA.end()) {
            if (*iA != *iB) {
                check_containtment = true;
                break;
            }
            ++iA;
            ++iB;
        }

        if ( !check_containtment ) {
            return eSame;
        }
    }

    // Check for containment
    size_t hits = 0;
    // This loop will only be executed if pointsA.size() > 0
    iterate(CSeq_loc::TPoints, iA, pointsA) {
        iterate(CSeq_loc::TPoints, iB, pointsB) {
            hits += (*iA == *iB) ? 1 : 0;
        }
    }
    if (hits == pointsA.size()) {
        return eContained;
    } else if (hits == pointsB.size()) {
        return eContains;
    } else if (hits == 0) {
        return eNoOverlap;
    } else {
        return eOverlap;
    }
}


// Compare packed point with bond
inline
ECompare s_Compare
(const CPacked_seqpnt& points,
 const CSeq_bond&      bond,
 CScope*               scope)
{
    // If all set bond points (can be just 1) are in points, then contains. If
    // one, but not all bond points in points, then overlap, else, no overlap
    const CSeq_point& bondA = bond.GetA();
    ECompare cmp = eNoOverlap;

    // Check for containment of the first residue in the bond
    if ( IsSameBioseq(points.GetId(), bondA.GetId(), scope) ) {
        TSeqPos pntA = bondA.GetPoint();

        // This loop will only be executed if points.GetPoints().size() > 0
        iterate(CSeq_loc::TPoints, it, points.GetPoints()) {
            if (pntA == *it) {
                cmp = eContains;
                break;
            }
        }
    }

    // Check for containment of the second residue of the bond if it exists
    if ( !bond.IsSetB() ) {
        return cmp;
    }

    const CSeq_point& bondB = bond.GetB();
    if ( !IsSameBioseq(points.GetId(), bondB.GetId(), scope) ) {
        return cmp;
    }

    TSeqPos pntB = bondB.GetPoint();
    // This loop will only be executed if points.GetPoints().size() > 0
    iterate(CSeq_loc::TPoints, it, points.GetPoints()) {
        if (pntB == *it) {
            if (cmp == eContains) {
                return eContains;
            } else {
                return eOverlap;
            }
        }
    }

    return cmp == eContains ? eOverlap : cmp;
}


// Compare a bond with a bond
inline
ECompare s_Compare
(const CSeq_bond& bndA,
 const CSeq_bond& bndB,
 CScope*          scope)
{
    // Performs two comparisons. First comparison is comparing the a points
    // of each bond and the b points of each bond. The second comparison
    // compares point a of bndA with point b of bndB and point b of bndA
    // with point a of bndB. The best match is used.
    ECompare cmp1, cmp2;
    int div = static_cast<int> (eSame);

    // Compare first points in each bond
    cmp1 = s_Compare(bndA.GetA(), bndB.GetA(), scope);

    // Compare second points in each bond if both exist
    if (bndA.IsSetB()  &&  bndB.IsSetB() ) {
        cmp2 = s_Compare(bndA.GetB(), bndB.GetB(), scope);
    } else {
        cmp2 = eNoOverlap;
    }
    int count1 = (static_cast<int> (cmp1) + static_cast<int> (cmp2)) / div;

    // Compare point A of bndA with point B of bndB (if it exists)
    if ( bndB.IsSetB() ) {
        cmp1 = s_Compare(bndA.GetA(), bndB.GetB(), scope);
    } else {
        cmp1 = eNoOverlap;
    }

    // Compare point B of bndA (if it exists) with point A of bndB.
    if ( bndA.IsSetB() ) {
        cmp2 = s_Compare(bndA.GetB(), bndB.GetA(), scope);
    } else {
        cmp2 = eNoOverlap;
    }
    int count2 = (static_cast<int> (cmp1) + static_cast<int> (cmp2)) / div;

    // Determine best match
    int count = (count1 > count2) ? count1 : count2;

    // Return based upon count in the obvious way
    switch ( count ) {
    case 1:
        if (!bndA.IsSetB()  &&  !bndB.IsSetB()) {
            return eSame;
        } else if ( !bndB.IsSetB() ) {
            return eContains;
        } else if ( !bndA.IsSetB() ) {
            return eContained;
        } else {
            return eOverlap;
        }
    case 2:
        return eSame;
    default:
        return eNoOverlap;
    }
}


// Compare an interval with a whole sequence
inline
ECompare s_Compare
(const CSeq_interval& interval,
 const CSeq_id&       id,
 CScope*              scope)
{
    // Return the complement of comparing id with interval
    return s_Complement(s_Compare(id, interval, scope));
}


// Compare an interval with a packed point
inline
ECompare s_Compare
(const CSeq_interval&  interval,
 const CPacked_seqpnt& packed,
 CScope*               scope)
{
    // Return the complement of comparing packed points and an interval
    return s_Complement(s_Compare(packed, interval, scope));
}


// Compare a Packed Interval with one of Whole, Interval, Point,
// Packed Point, or Bond.
template <class T>
ECompare s_Compare
(const CPacked_seqint& intervals,
 const T&              slt,
 CScope*               scope)
{
    // Check that intervals is not empty
    if(intervals.Get().size() == 0) {
        return eNoOverlap;
    }

    ECompare cmp1 , cmp2;
    CSeq_loc::TIntervals::const_iterator it = intervals.Get().begin();
    cmp1 = s_Compare(**it, slt, scope);

    // This loop will be executed only if intervals.Get().size() > 1
    for (++it;  it != intervals.Get().end();  ++it) {
        cmp2 = s_Compare(**it, slt, scope);
        cmp1 = static_cast<ECompare> (s_RetA[cmp1][cmp2]);
    }
    return cmp1;
}


// Compare one of Whole, Interval, Point, Packed Point, or Bond with a
// Packed Interval.
template <class T>
ECompare s_Compare
(const T&              slt,
 const CPacked_seqint& intervals,
 CScope*               scope)
{
    // Check that intervals is not empty
    if(intervals.Get().size() == 0) {
        return eNoOverlap;
    }

    ECompare cmp1 , cmp2;
    CSeq_loc::TIntervals::const_iterator it = intervals.Get().begin();
    cmp1 = s_Compare(slt, **it, scope);

    // This loop will be executed only if intervals.Get().size() > 1
    for (++it;  it != intervals.Get().end();  ++it) {
        cmp2 = s_Compare(slt, **it, scope);
        cmp1 = static_cast<ECompare> (s_RetB[cmp1][cmp2]);
    }
    return cmp1;
}


// Compare a CSeq_loc and a CSeq_interval. Used by s_Compare_Help below
// when comparing list<CRef<CSeq_loc>> with list<CRef<CSeq_interval>>.
// By wrapping the CSeq_interval in a CSeq_loc, s_Compare(const CSeq_loc&,
// const CSeq_loc&, CScope*) can be called. This permits CPacked_seqint type
// CSeq_locs to be treated the same as CSeq_loc_mix and CSeq_loc_equiv type
// CSeq_locs
inline
ECompare s_Compare
(const CSeq_loc&      sl,
 const CSeq_interval& si,
 CScope*              scope)
{
    CSeq_loc nsl;
    nsl.SetInt(const_cast<CSeq_interval&>(si));
    return s_Compare(sl, nsl, scope);
}


// Compare a CSeq_interval and a CSeq_loc. Used by s_Compare_Help below
// when comparing TLocations with TIntervals. By
// wrapping the CSeq_interval in a CSeq_loc, s_Compare(const CSeq_loc&,
// const CSeq_loc&, CScope*) can be called. This permits CPacked_seqint type
// CSeq_locs to be treated the same as CSeq_loc_mix and CSeq_loc_equiv type
// CSeq_locs
inline
ECompare s_Compare
(const CSeq_interval& si,
 const CSeq_loc&      sl,
 CScope*              scope)
{
    CSeq_loc nsl;
    nsl.SetInt(const_cast<CSeq_interval&>(si));
    return s_Compare(nsl, sl, scope);
}


// Called by s_Compare() below for <CSeq_loc, CSeq_loc>,
// <CSeq_loc, CSeq_interval>, <CSeq_interval, CSeq_loc>, and
// <CSeq_interval, CSeq_interval>
template <class T1, class T2>
ECompare s_Compare_Help
(const list<CRef<T1> >& mec,
 const list<CRef<T2> >& youc,
 const CSeq_loc&        you,
 CScope*                scope)
{
    // If either mec or youc is empty, return eNoOverlap
    if(mec.size() == 0  ||  youc.size() == 0) {
        return eNoOverlap;
    }

    list<CRef<T1> >::const_iterator mit = mec.begin();
    list<CRef<T2> >::const_iterator yit = youc.begin();
    ECompare cmp1, cmp2;

    // Check for equality of the lists. Note, order of the lists contents are
    // significant
    if (mec.size() == youc.size()) {
        bool check_contained = false;

        for ( ;  mit != mec.end()  &&  yit != youc.end();  ++mit, ++yit) {
            if (s_Compare(**mit, ** yit, scope) != eSame) {
                check_contained = true;
                break;
            }
        }

        if ( !check_contained ) {
            return eSame;
        }
    }

    // Check if mec contained in youc
    mit = mec.begin();
    yit = youc.begin();
    bool got_one = false;
    while (mit != mec.end()  &&  yit != youc.end()) {
        cmp1 = s_Compare(**mit, **yit, scope);
        switch ( cmp1 ) {
        case eNoOverlap:
            ++yit;
            break;
        case eSame:
            ++mit;
            ++yit;
            got_one = true;
            break;
        case eContained:
            ++mit;
            got_one = true;
            break;
        default:
            got_one = true;
            // artificial trick -- just to get out the loop "prematurely"
            yit = youc.end();
        }
    }
    if (mit == mec.end()) {
        return eContained;
    }

    // Check if mec contains youc
    mit = mec.begin();
    yit = youc.begin();
    while (mit != mec.end()  &&  yit != youc.end() ) {
        cmp1 = s_Compare(**yit, **mit, scope);
        switch ( cmp1 ) {
        case eNoOverlap:
            ++mit;
            break;
        case eSame:
            ++mit;
            ++yit;
            got_one = true;
            break;
        case eContained:
            ++yit;
            got_one = true;
            break;
        default:
            got_one = true;
            // artificial trick -- just to get out the loop "prematurely"
            mit = mec.end();
        }
    }
    if (yit == youc.end()) {
        return eContains;
    }

    // Check for overlap of mec and youc
    if ( got_one ) {
        return eOverlap;
    }
    mit = mec.begin();
    cmp1 = s_Compare(**mit, you, scope);
    for (++mit;  mit != mec.end();  ++mit) {
        cmp2 = s_Compare(**mit, you, scope);
        cmp1 = static_cast<ECompare> (s_RetA[cmp1][cmp2]);
    }
    return cmp1;
}


inline
const CSeq_loc::TLocations& s_GetLocations(const CSeq_loc& loc)
{
    switch (loc.Which()) {
        case CSeq_loc::e_Mix:    return loc.GetMix().Get();
        case CSeq_loc::e_Equiv:  return loc.GetEquiv().Get();
        default: // should never happen, but the compiler doesn't know that...
            THROW1_TRACE(runtime_error,
                         "s_GetLocations: unsupported location type:"
                         + CSeq_loc::SelectionName(loc.Which()));
    }
}


// Compares two Seq-locs
ECompare s_Compare
(const CSeq_loc& me,
 const CSeq_loc& you,
 CScope*         scope)
{
    // Handle the case where me is one of e_Mix, e_Equiv, e_Packed_int
    switch ( me.Which() ) {
    case CSeq_loc::e_Mix:
    case CSeq_loc::e_Equiv: {
        const CSeq_loc::TLocations& pmc = s_GetLocations(me);
        switch ( you.Which() ) {
        case CSeq_loc::e_Mix:
        case CSeq_loc::e_Equiv: {
            const CSeq_loc::TLocations& pyc = s_GetLocations(you);
            return s_Compare_Help(pmc, pyc, you, scope);
        }
        case CSeq_loc::e_Packed_int: {
            const CSeq_loc::TIntervals& pyc = you.GetPacked_int().Get();
            return s_Compare_Help(pmc,pyc, you, scope);
        }
        case CSeq_loc::e_Null:
        case CSeq_loc::e_Empty:
        case CSeq_loc::e_Whole:
        case CSeq_loc::e_Int:
        case CSeq_loc::e_Pnt:
        case CSeq_loc::e_Packed_pnt:
        case CSeq_loc::e_Bond:
        case CSeq_loc::e_Feat: {
            //Check that pmc is not empty
            if(pmc.size() == 0) {
                return eNoOverlap;
            }

            CSeq_loc::TLocations::const_iterator mit = pmc.begin();
            ECompare cmp1, cmp2;
            cmp1 = s_Compare(**mit, you, scope);

            // This loop will only be executed if pmc.size() > 1
            for (++mit;  mit != pmc.end();  ++mit) {
                cmp2 = s_Compare(**mit, you, scope);
                cmp1 = static_cast<ECompare> (s_RetA[cmp1][cmp2]);
            }
            return cmp1;
        }
        default:
            return eNoOverlap;
        }
    }
    case CSeq_loc::e_Packed_int: {
        switch ( you.Which() ) {
        case CSeq_loc::e_Mix:
        case CSeq_loc::e_Equiv: {
            const CSeq_loc::TLocations& pyc = s_GetLocations(you);
            const CSeq_loc::TIntervals& pmc = me.GetPacked_int().Get();
            return s_Compare_Help(pmc,pyc, you, scope);
        }
        case CSeq_loc::e_Packed_int: {
            const CSeq_loc::TIntervals& mc = me.GetPacked_int().Get();
            const CSeq_loc::TIntervals& yc = you.GetPacked_int().Get();
            return s_Compare_Help(mc, yc, you, scope);
        }
        default:
            // All other cases are handled below
            break;
        }
    }
    default:
        // All other cases are handled below
        break;
    }

    // Note, at this point, me is not one of e_Mix or e_Equiv
    switch ( you.Which() ) {
    case CSeq_loc::e_Mix:
    case CSeq_loc::e_Equiv: {
        const CSeq_loc::TLocations& pyouc = s_GetLocations(you);

        // Check that pyouc is not empty
        if(pyouc.size() == 0) {
            return eNoOverlap;
        }

        CSeq_loc::TLocations::const_iterator yit = pyouc.begin();
        ECompare cmp1, cmp2;
        cmp1 = s_Compare(me, **yit, scope);

        // This loop will only be executed if pyouc.size() > 1
        for (++yit;  yit != pyouc.end();  ++yit) {
            cmp2 = s_Compare(me, **yit, scope);
            cmp1 = static_cast<ECompare> (s_RetB[cmp1][cmp2]);
        }
        return cmp1;
    }
    // All other cases handled below
    default:
        break;
    }

    switch ( me.Which() ) {
    case CSeq_loc::e_Null: {
        switch ( you.Which() ) {
        case CSeq_loc::e_Null:
            return eSame;
        default:
            return eNoOverlap;
        }
    }
    case CSeq_loc::e_Empty: {
        switch ( you.Which() ) {
        case CSeq_loc::e_Empty:
            if ( IsSameBioseq(me.GetEmpty(), you.GetEmpty(), scope) ) {
                return eSame;
            } else {
                return eNoOverlap;
            }
        default:
            return eNoOverlap;
        }
    }
    case CSeq_loc::e_Packed_int: {
        // Comparison of two e_Packed_ints is handled above
        switch ( you.Which() ) {
        case CSeq_loc::e_Whole:
            return s_Compare(me.GetPacked_int(), you.GetWhole(), scope);
        case CSeq_loc::e_Int:
            return s_Compare(me.GetPacked_int(), you.GetInt(), scope);
        case CSeq_loc::e_Pnt:
            return s_Compare(me.GetPacked_int(), you.GetPnt(), scope);
        case CSeq_loc::e_Packed_pnt:
            return s_Compare(me.GetPacked_int(), you.GetPacked_pnt(), scope);
        case CSeq_loc::e_Bond:
            return s_Compare(me.GetPacked_int(), you.GetBond(), scope);
        default:
            return eNoOverlap;
        }
    }
    case CSeq_loc::e_Whole: {
        switch ( you.Which() ) {
        case CSeq_loc::e_Whole:
            return s_Compare(me.GetWhole(), you.GetWhole(), scope);
        case CSeq_loc::e_Bond:
            return s_Compare(me.GetWhole(), you.GetBond(), scope);
        case CSeq_loc::e_Int:
            return s_Compare(me.GetWhole(), you.GetInt(), scope);
        case CSeq_loc::e_Pnt:
            return s_Compare(me.GetWhole(), you.GetPnt(), scope);
        case CSeq_loc::e_Packed_pnt:
            return s_Compare(me.GetWhole(), you.GetPacked_pnt(), scope);
        case CSeq_loc::e_Packed_int:
            return s_Compare(me.GetWhole(), you.GetPacked_int(), scope);
        default:
            return eNoOverlap;
        }
    }
    case CSeq_loc::e_Int: {
        switch ( you.Which() ) {
        case CSeq_loc::e_Whole:
            return s_Compare(me.GetInt(), you.GetWhole(), scope);
        case CSeq_loc::e_Bond:
            return s_Compare(me.GetInt(), you.GetBond(), scope);
        case CSeq_loc::e_Int:
            return s_Compare(me.GetInt(), you.GetInt(), scope);
        case CSeq_loc::e_Pnt:
            return s_Compare(me.GetInt(), you.GetPnt(), scope);
        case CSeq_loc::e_Packed_pnt:
            return s_Compare(me.GetInt(), you.GetPacked_pnt(), scope);
        case CSeq_loc::e_Packed_int:
            return s_Compare(me.GetInt(), you.GetPacked_int(), scope);
        default:
            return eNoOverlap;
        }
    }
    case CSeq_loc::e_Pnt: {
        switch ( you.Which() ) {
        case CSeq_loc::e_Whole:
            return s_Complement(s_Compare(you.GetWhole(), me.GetPnt(), scope));
        case CSeq_loc::e_Int:
            return s_Compare(me.GetPnt(), you.GetInt(), scope);
        case CSeq_loc::e_Pnt:
            return s_Compare(me.GetPnt(), you.GetPnt(), scope);
        case CSeq_loc::e_Packed_pnt:
            return s_Compare(me.GetPnt(), you.GetPacked_pnt(), scope);
        case CSeq_loc::e_Bond:
            return s_Compare(me.GetPnt(), you.GetBond(), scope);
        case CSeq_loc::e_Packed_int:
            return s_Compare(me.GetPnt(), you.GetPacked_int(), scope);
        default:
            return eNoOverlap;
        }
    }
    case CSeq_loc::e_Packed_pnt: {
        const CPacked_seqpnt& packed = me.GetPacked_pnt();
        switch ( you.Which() ) {
        case CSeq_loc::e_Whole:
            return s_Complement(s_Compare(you.GetWhole(), packed, scope));
        case CSeq_loc::e_Int:
            return s_Compare(packed, you.GetInt(), scope);
        case CSeq_loc::e_Pnt:
            return s_Complement(s_Compare(you.GetPnt(), packed, scope));
        case CSeq_loc::e_Packed_pnt:
            return s_Compare(packed, you.GetPacked_pnt(),scope);
        case CSeq_loc::e_Bond:
            return s_Compare(packed, you.GetBond(), scope);
        case CSeq_loc::e_Packed_int:
            return s_Compare(me.GetPacked_pnt(), you.GetPacked_int(), scope);
        default:
            return eNoOverlap;
        }
    }
    case CSeq_loc::e_Bond: {
        const CSeq_bond& bnd = me.GetBond();
        switch ( you.Which() ) {
        case CSeq_loc::e_Whole:
            return s_Complement(s_Compare(you.GetWhole(), bnd, scope));
        case CSeq_loc::e_Bond:
            return s_Compare(bnd, you.GetBond(), scope);
        case CSeq_loc::e_Int:
            return s_Compare(bnd, you.GetInt(), scope);
        case CSeq_loc::e_Packed_pnt:
            return s_Complement(s_Compare(you.GetPacked_pnt(), bnd, scope));
        case CSeq_loc::e_Pnt:
            return s_Complement(s_Compare(you.GetPnt(), bnd, scope));
        case CSeq_loc::e_Packed_int:
            return s_Complement(s_Compare(you.GetPacked_int(), bnd, scope));
        default:
            return eNoOverlap;
        }
    }
    default:
        return eNoOverlap;
    }
}


ECompare Compare
(const CSeq_loc& loc1,
 const CSeq_loc& loc2,
 CScope*         scope)
{
    return s_Compare(loc1, loc2, scope);
}


END_SCOPE(sequence)


void CFastaOstream::Write(CBioseq_Handle& handle, const CSeq_loc* location)
{
    WriteTitle(handle);
    WriteSequence(handle, location);
}


static int s_ScoreNAForFasta(const CSeq_id* id)
{
    switch (id->Which()) {
    case CSeq_id::e_not_set:
    case CSeq_id::e_Giim:
    case CSeq_id::e_Pir:
    case CSeq_id::e_Swissprot:
    case CSeq_id::e_Prf:       return kMax_Int;
    case CSeq_id::e_Local:     return 230;
    case CSeq_id::e_Gi:        return 120;
    case CSeq_id::e_General:   return 50;
    case CSeq_id::e_Patent:    return 40;
    case CSeq_id::e_Gibbsq:
    case CSeq_id::e_Gibbmt:
    case CSeq_id::e_Pdb:       return 30;
    case CSeq_id::e_Other:     return 15;
    default:                   return 20; // [third party] GenBank/EMBL/DDBJ
    }
}


static int s_ScoreAAForFasta(const CSeq_id* id)
{
    switch (id->Which()) {
    case CSeq_id::e_not_set:
    case CSeq_id::e_Giim:      return kMax_Int;
    case CSeq_id::e_Local:     return 230;
    case CSeq_id::e_Gi:        return 120;
    case CSeq_id::e_General:   return 90;
    case CSeq_id::e_Patent:    return 80;
    case CSeq_id::e_Prf:       return 70;
    case CSeq_id::e_Pdb:       return 50;
    case CSeq_id::e_Gibbsq:
    case CSeq_id::e_Gibbmt:    return 40;
    case CSeq_id::e_Pir:       return 30;
    case CSeq_id::e_Swissprot: return 20;
    case CSeq_id::e_Other:     return 15;
    default:                   return 60; // [third party] GenBank/EMBL/DDBJ
    }
}


void CFastaOstream::WriteTitle(CBioseq_Handle& handle)
{
    CBioseq_Handle::TBioseqCore core = handle.GetBioseqCore();

    bool is_na    = core->GetInst().GetMol() != CSeq_inst::eMol_aa;
    bool found_gi = false;

    m_Out << '>';

    iterate (CBioseq::TId, id, core->GetId()) {
        if ((*id)->IsGi()) {
            (*id)->WriteAsFasta(m_Out);
            found_gi = true;
            break;
        }
    }

    CRef<CSeq_id> id = FindBestChoice(core->GetId(),
                                      is_na ? s_ScoreNAForFasta
                                      : s_ScoreAAForFasta);
    if (id.NotEmpty()  &&  id->Which() != CSeq_id::e_Gi) {
        if (found_gi) {
            m_Out << '|';
        }
        id->WriteAsFasta(m_Out);
    }

#if 0
// This was in id1_fetch for some reason, but then it never got updated
// to use the sophisticated version of GetTitle....
    for (CSeqdesc_CI it(handle, CSeqdesc::e_Name);  it;  ++it) {
        m_Out << ' ' << it->GetName();
        BREAK(it);
    }
#endif

    m_Out << ' ' << sequence::GetTitle(handle) << NcbiEndl;
}


struct SGap {
    SGap(TSeqPos start, TSeqPos length) : m_Start(start), m_Length(length) { }
    TSeqPos GetEnd(void) const { return m_Start + m_Length - 1; }

    TSeqPos m_Start, m_Length;
};
typedef list<SGap> TGaps;


static bool s_IsGap(const CSeq_loc& loc, CScope& scope)
{
    if (loc.IsNull()) {
        return true;
    }

    CTypeConstIterator<CSeq_id> id(loc);
    CBioseq_Handle handle = scope.GetBioseqHandle(*id);
    if (handle  &&  handle.GetBioseqCore()->GetInst().GetRepr()
        == CSeq_inst::eRepr_virtual) {
        return true;
    }

    return false; // default
}


static TGaps s_FindGaps(const CSeq_ext& ext, CScope& scope)
{
    TSeqPos pos = 0;
    TGaps   gaps;

    switch (ext.Which()) {
    case CSeq_ext::e_Seg:
        iterate (CSeg_ext::Tdata, it, ext.GetSeg().Get()) {
            TSeqPos length = sequence::GetLength(**it, &scope);
            if (s_IsGap(**it, scope)) {
                gaps.push_back(SGap(pos, length));
            }
            pos += length;
        }
        break;

    case CSeq_ext::e_Delta:
        iterate (CDelta_ext::Tdata, it, ext.GetDelta().Get()) {
            switch ((*it)->Which()) {
            case CDelta_seq::e_Loc:
            {
                const CSeq_loc& loc = (*it)->GetLoc();
                TSeqPos length = sequence::GetLength(loc, &scope);
                if (s_IsGap(loc, scope)) {
                    gaps.push_back(SGap(pos, length));
                }
                pos += length;
                break;
            }

            case CDelta_seq::e_Literal:
            {
                const CSeq_literal& lit    = (*it)->GetLiteral();
                TSeqPos             length = lit.GetLength();
                if ( !lit.IsSetSeq_data() ) {
                    gaps.push_back(SGap(pos, length));
                }
                pos += length;
                break;
            }

            default:
                ERR_POST(Warning << "CFastaOstream::WriteSequence: "
                         "unsupported Delta-seq selection "
                         << CDelta_seq::SelectionName((*it)->Which()));
                break;
            }
        }

    default:
        break;
    }

    return gaps;
}


static TGaps s_AdjustGaps(const TGaps& gaps, const CSeq_loc& location)
{
    // assume location matches handle
    const TSeqPos         kMaxPos = numeric_limits<TSeqPos>::max();
    TSeqPos               pos     = 0;
    TGaps::const_iterator gap_it  = gaps.begin();
    TGaps                 adjusted_gaps;
    SGap                  new_gap(kMaxPos, 0);

    for (CSeq_loc_CI loc_it(location);  loc_it  &&  gap_it != gaps.end();
         pos += loc_it.GetRange().GetLength(), loc_it++) {
        CSeq_loc_CI::TRange range = loc_it.GetRange();

        if (new_gap.m_Start != kMaxPos) {
            // in progress
            if (gap_it->GetEnd() < range.GetFrom()) {
                adjusted_gaps.push_back(new_gap);
                new_gap.m_Start = kMaxPos;
                ++gap_it;
            } else if (gap_it->GetEnd() <= range.GetTo()) {
                new_gap.m_Length += gap_it->GetEnd() - range.GetFrom() + 1;
                adjusted_gaps.push_back(new_gap);
                new_gap.m_Start = kMaxPos;
                ++gap_it;
            } else {
                new_gap.m_Length += range.GetLength();
                continue;
            }
        }

        while (gap_it->GetEnd() < range.GetFrom()) {
            ++gap_it; // skip
        }

        if (gap_it->m_Start <= range.GetFrom()) {
            if (gap_it->GetEnd() <= range.GetTo()) {
                adjusted_gaps.push_back
                    (SGap(pos, gap_it->GetEnd() - range.GetFrom() + 1));
                ++gap_it;
            } else {
                new_gap.m_Start  = pos;
                new_gap.m_Length = range.GetLength();
                continue;
            }
        }

        while (gap_it->m_Start <= range.GetTo()) {
            TSeqPos pos2 = pos + gap_it->m_Start - range.GetFrom();
            if (gap_it->GetEnd() <= range.GetTo()) {
                adjusted_gaps.push_back(SGap(pos2, gap_it->m_Length));
                ++gap_it;
            } else {
                new_gap.m_Start  = pos2;
                new_gap.m_Length = range.GetTo() - gap_it->m_Start + 1;
            }
        }
    }

    if (new_gap.m_Start != kMaxPos) {
        adjusted_gaps.push_back(new_gap);
    }

    return adjusted_gaps;
}


void CFastaOstream::WriteSequence(CBioseq_Handle& handle,
                                  const CSeq_loc* location)
{
    const CBioseq&   seq  = handle.GetBioseq();
    const CSeq_inst& inst = seq.GetInst();
    if ( !(m_Flags & eAssembleParts)  &&  !inst.IsSetSeq_data() ) {
        return;
    }

    CSeqVector v = (location
                    ? handle.GetSequenceView(*location,
                                             CBioseq_Handle::eViewMerged,
                                             true)
                    : handle.GetSeqVector(true));
    bool is_na = inst.GetMol() != CSeq_inst::eMol_aa;
    // autodetection is sometimes broken (!)
    v.SetCoding(is_na ? CSeq_data::e_Iupacna : CSeq_data::e_Iupacaa);

    TSeqPos              size = v.size();
    TSeqPos              pos  = 0;
    CSeqVector::TResidue gap  = v.GetGapChar();
    string               buffer;
    TGaps                gaps;
    CScope&              scope   = handle.GetScope();
    const TSeqPos        kMaxPos = numeric_limits<TSeqPos>::max();

    if ( !inst.IsSetSeq_data()  &&  inst.IsSetExt() ) {
        gaps = s_FindGaps(inst.GetExt(), scope);
        if (location) {
            gaps = s_AdjustGaps(gaps, *location);
        }
    }
    gaps.push_back(SGap(kMaxPos, 0));

    while (pos < size) {
        unsigned int limit = min(m_Width,
                                 min(size, gaps.front().m_Start) - pos);
        v.GetSeqData(pos, pos + limit, buffer);
        pos += limit;
        if (limit > 0) {
            m_Out << buffer << '\n';
        }
        if (pos == gaps.front().m_Start) {
            if (m_Flags & eInstantiateGaps) {
                for (TSeqPos l = gaps.front().m_Length; l > 0; l -= m_Width) {
                    m_Out << string(min(l, m_Width), gap) << '\n';
                }
            } else {
                m_Out << "-\n";
            }
            pos += gaps.front().m_Length;
            gaps.pop_front();
        }
    }
    m_Out << NcbiFlush;
}


void CFastaOstream::Write(CSeq_entry& entry, const CSeq_loc* location)
{
    CObjectManager om;
    CScope         scope(om);

    scope.AddTopLevelSeqEntry(entry);
    for (CTypeConstIterator<CBioseq> it(entry);  it;  ++it) {
        if ( !SkipBioseq(*it) ) {
            CBioseq_Handle h = scope.GetBioseqHandle(*it->GetId().front());
            Write(h, location);
        }
    }
}


void CFastaOstream::Write(CBioseq& seq, const CSeq_loc* location)
{
    CSeq_entry entry;
    entry.SetSeq(seq);
    Write(entry, location);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.3  2002/08/27 21:41:15  ucko
* Add CFastaOstream.
*
* Revision 1.2  2002/06/07 16:11:09  ucko
* Move everything into the "sequence" namespace.
* Drop the anonymous-namespace business; "sequence" should provide
* enough protection, and anonymous namespaces ironically interact poorly
* with WorkShop's caching when rebuilding shared libraries.
*
* Revision 1.1  2002/06/06 18:41:41  clausen
* Initial version
*
* ===========================================================================
*/
