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

#include <ncbi_pch.hpp>
#include <serial/iterator.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seq_vector_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/impl/handle_range_map.hpp>
#include <objmgr/impl/synonyms.hpp>

#include <objects/general/Int_fuzz.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/MolInfo.hpp>
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

#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

#include <objmgr/seq_loc_mapper.hpp>
#include <objmgr/util/sequence.hpp>
#include <util/strsearch.hpp>

#include <list>
#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(sequence)


const COrg_ref& GetOrg_ref(const CBioseq_Handle& handle)
{
    {{
        CSeqdesc_CI desc(handle, CSeqdesc::e_Source);
        if (desc) {
            return desc->GetSource().GetOrg();
        }
    }}

    {{
        CSeqdesc_CI desc(handle, CSeqdesc::e_Org);
        if (desc) {
            return desc->GetOrg();
        }
    }}

    NCBI_THROW(CException, eUnknown, "No organism set");
}


int GetTaxId(const CBioseq_Handle& handle)
{
    try {
        return GetOrg_ref(handle).GetTaxId();
    }
    catch (...) {
        return 0;
    }
}



TSeqPos GetLength(const CSeq_id& id, CScope* scope)
{
    if ( !scope ) {
        return numeric_limits<TSeqPos>::max();
    }
    CBioseq_Handle hnd = scope->GetBioseqHandle(id);
    if ( !hnd ) {
        return numeric_limits<TSeqPos>::max();
    }
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

    ITERATE( CSeq_loc_mix::Tdata, i, mix.Get() ) {
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
    ITERATE (TPoints, it, pts.GetPoints()) {
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
            return hnd1  &&  hnd2  &&  (hnd1 == hnd2);
        } catch (exception& e) {
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
    } catch (CNotUnique&) {
        return false;
    }
}


class CSeqIdFromHandleException : EXCEPTION_VIRTUAL_BASE public CException
{
public:
    // Enumerated list of document management errors
    enum EErrCode {
        eNoSynonyms,
        eRequestedIdNotFound
    };

    // Translate the specific error code into a string representations of
    // that error code.
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eNoSynonyms:           return "eNoSynonyms";
        case eRequestedIdNotFound:  return "eRequestedIdNotFound";
        default:                    return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CSeqIdFromHandleException, CException);
};


const CSeq_id& GetId(const CBioseq_Handle& handle,
                     EGetIdType type)
{
    switch (type) {
    case eGetId_HandleDefault:
        return *handle.GetSeqId();

    case eGetId_ForceGi:
        if (handle.GetSeqId().GetPointer()  &&  handle.GetSeqId()->IsGi()) {
            return *handle.GetSeqId();
        }
        {{
            CConstRef<CSynonymsSet> syns =
                handle.GetScope().GetSynonyms(*handle.GetSeqId());
            if ( !syns ) {
                string msg("No synonyms found for sequence ");
                handle.GetSeqId()->GetLabel(&msg);
                NCBI_THROW(CSeqIdFromHandleException, eNoSynonyms, msg);
            }

            ITERATE (CSynonymsSet, iter, *syns) {
                CSeq_id_Handle idh = CSynonymsSet::GetSeq_id_Handle(iter);
                if (idh.GetSeqId()->IsGi()) {
                    return *idh.GetSeqId();
                }
            }
        }}
        break;

    case eGetId_Best:
        {{
            CConstRef<CSynonymsSet> syns =
                handle.GetScope().GetSynonyms(handle.GetSeq_id_Handle());
            if ( !syns ) {
                string msg("No synonyms found for sequence ");
                handle.GetSeqId()->GetLabel(&msg);
                NCBI_THROW(CSeqIdFromHandleException, eNoSynonyms, msg);
            }

            list< CRef<CSeq_id> > ids;
            ITERATE (CSynonymsSet, iter, *syns) {
                CSeq_id_Handle idh = CSynonymsSet::GetSeq_id_Handle(iter);
                ids.push_back
                    (CRef<CSeq_id>(const_cast<CSeq_id*>(idh.GetSeqId().GetPointer())));
            }
            CConstRef<CSeq_id> best_id = FindBestChoice(ids, CSeq_id::Score);
            if (best_id) {
                return *best_id;
            }
        }}
        break;

    default:
        NCBI_THROW(CSeqIdFromHandleException, eRequestedIdNotFound,
                   "Unhandled seq-id type");
        break;
    }

    NCBI_THROW(CSeqIdFromHandleException, eRequestedIdNotFound,
               "No best seq-id could be found");
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

inline
static ENa_strand s_GetStrand(const CSeq_loc& loc)
{
    switch (loc.Which()) {
    case CSeq_loc::e_Bond:
        {
            const CSeq_bond& bond = loc.GetBond();
            ENa_strand a_strand = bond.GetA().IsSetStrand() ?
                bond.GetA().GetStrand() : eNa_strand_unknown;
            ENa_strand b_strand = eNa_strand_unknown;
            if ( bond.IsSetB() ) {
                b_strand = bond.GetB().IsSetStrand() ?
                    bond.GetB().GetStrand() : eNa_strand_unknown;
            }

            if ( a_strand == eNa_strand_unknown  &&
                 b_strand != eNa_strand_unknown ) {
                a_strand = b_strand;
            } else if ( a_strand != eNa_strand_unknown  &&
                        b_strand == eNa_strand_unknown ) {
                b_strand = a_strand;
            }

            return (a_strand != b_strand) ? eNa_strand_other : a_strand;
        }
    case CSeq_loc::e_Whole:
        return eNa_strand_both;
    case CSeq_loc::e_Int:
        return loc.GetInt().IsSetStrand() ? loc.GetInt().GetStrand() :
            eNa_strand_unknown;
    case CSeq_loc::e_Pnt:
        return loc.GetPnt().IsSetStrand() ? loc.GetPnt().GetStrand() :
            eNa_strand_unknown;
    case CSeq_loc::e_Packed_pnt:
        return loc.GetPacked_pnt().IsSetStrand() ?
            loc.GetPacked_pnt().GetStrand() : eNa_strand_unknown;
    case CSeq_loc::e_Packed_int:
    {
        ENa_strand strand = eNa_strand_unknown;
        bool strand_set = false;
        ITERATE(CPacked_seqint::Tdata, i, loc.GetPacked_int().Get()) {
            ENa_strand istrand = (*i)->IsSetStrand() ? (*i)->GetStrand() :
                eNa_strand_unknown;
            if (strand == eNa_strand_unknown  &&  istrand == eNa_strand_plus) {
                strand = eNa_strand_plus;
                strand_set = true;
            } else if (strand == eNa_strand_plus  &&
                istrand == eNa_strand_unknown) {
                istrand = eNa_strand_plus;
                strand_set = true;
            } else if (!strand_set) {
                strand = istrand;
                strand_set = true;
            } else if (istrand != strand) {
                return eNa_strand_other;
            }
        }
        return strand;
    }
    case CSeq_loc::e_Mix:
    {
        ENa_strand strand = eNa_strand_unknown;
        bool strand_set = false;
        ITERATE(CSeq_loc_mix::Tdata, it, loc.GetMix().Get()) {
            ENa_strand istrand = GetStrand(**it);
            if (strand == eNa_strand_unknown  &&  istrand == eNa_strand_plus) {
                strand = eNa_strand_plus;
                strand_set = true;
            } else if (strand == eNa_strand_plus  &&
                istrand == eNa_strand_unknown) {
                istrand = eNa_strand_plus;
                strand_set = true;
            } else if (!strand_set) {
                strand = istrand;
                strand_set = true;
            } else if (istrand != strand) {
                return eNa_strand_other;
            }
        }
    }
    default:
        return eNa_strand_unknown;
    }
}


ENa_strand GetStrand(const CSeq_loc& loc, CScope* scope)
{
    if (!IsOneBioseq(loc, scope)) {
        return eNa_strand_unknown;  // multiple bioseqs
    }

    ENa_strand strand = eNa_strand_unknown, cstrand;
    bool strand_set = false;
    for (CSeq_loc_CI i(loc); i; ++i) {
        switch (i.GetSeq_loc().Which()) {
        case CSeq_loc::e_Equiv:
            break;
        default:
            cstrand = s_GetStrand(i.GetSeq_loc());
            if (strand == eNa_strand_unknown  &&  cstrand == eNa_strand_plus) {
                strand = eNa_strand_plus;
                strand_set = true;
            } else if (strand == eNa_strand_plus  &&
                cstrand == eNa_strand_unknown) {
                cstrand = eNa_strand_plus;
                strand_set = true;
            } else if (!strand_set) {
                strand = cstrand;
                strand_set = true;
            } else if (cstrand != strand) {
                return eNa_strand_other;
            }
        }
    }
    return strand;
}


TSeqPos GetStart(const CSeq_loc& loc, CScope* scope)
    THROWS((CNotUnique))
{
    // Throw CNotUnique if loc does not represent one CBioseq
    GetId(loc, scope);

    CSeq_loc::TRange rg = loc.GetTotalRange();
    return rg.GetFrom();
}


TSeqPos GetStop(const CSeq_loc& loc, CScope* scope)
    THROWS((CNotUnique))
{
    // Throw CNotUnique if loc does not represent one CBioseq
    GetId(loc, scope);

    CSeq_loc::TRange rg = loc.GetTotalRange();
    return rg.GetTo();
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

    TSeqPos pnt = point.GetPoint();

    // If the interval is of length 1 and contains the point, then they are
    // identical
    if (interval.GetFrom() == pnt  &&  interval.GetTo() == pnt ) {
        return eSame;
    }

    // If point in interval, then interval contains point
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
        ITERATE(CSeq_loc::TPoints, it, points.GetPoints()) {
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
        ITERATE(CSeq_loc::TPoints, it, points.GetPoints()) {
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
    ITERATE(CSeq_loc::TPoints, iA, pointsA) {
        ITERATE(CSeq_loc::TPoints, iB, pointsB) {
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
        ITERATE(CSeq_loc::TPoints, it, points.GetPoints()) {
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
    ITERATE(CSeq_loc::TPoints, it, points.GetPoints()) {
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

    typename list<CRef<T1> >::const_iterator mit = mec.begin();
    typename list<CRef<T2> >::const_iterator yit = youc.begin();
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
        NCBI_THROW(CObjmgrUtilException, eBadLocation,
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


void ChangeSeqId(CSeq_id* id, bool best, CScope* scope)
{
    // Return if no scope
    if (!scope) {
        return;
    }

    // Get CBioseq represented by *id
    CBioseq_Handle::TBioseqCore seq =
        scope->GetBioseqHandle(*id).GetBioseqCore();

    // Get pointer to the best/worst id of *seq
    const CSeq_id* tmp_id;
    if (!best) {
        tmp_id = FindBestChoice(seq->GetId(), CSeq_id::BestRank).GetPointer();
    } else {
        tmp_id = FindBestChoice(seq->GetId(), CSeq_id::WorstRank).GetPointer();
    }

    // Change the contents of *id to that of *tmp_id
    id->Reset();
    id->Assign(*tmp_id);
}


void ChangeSeqLocId(CSeq_loc* loc, bool best, CScope* scope)
{
    if (!scope) {
        return;
    }

    for (CTypeIterator<CSeq_id> id(Begin(*loc)); id; ++id) {
        ChangeSeqId(&(*id), best, scope);
    }
}


bool BadSeqLocSortOrder
(const CBioseq&, //  seq,
 const CSeq_loc& loc,
 CScope*         scope)
{
    ENa_strand strand = GetStrand(loc, scope);
    if (strand == eNa_strand_unknown  ||  strand == eNa_strand_other) {
        return false;
    }
    
    // Check that loc segments are in order
    CSeq_loc::TRange last_range;
    bool first = true;
    for (CSeq_loc_CI lit(loc); lit; ++lit) {
        if (first) {
            last_range = lit.GetRange();
            first = false;
            continue;
        }
        if (strand == eNa_strand_minus) {
            if (last_range.GetTo() < lit.GetRange().GetTo()) {
                return true;
            }
        } else {
            if (last_range.GetFrom() > lit.GetRange().GetFrom()) {
                return true;
            }
        }
        last_range = lit.GetRange();
    }
    return false;
}


ESeqLocCheck SeqLocCheck(const CSeq_loc& loc, CScope* scope)
{
    ESeqLocCheck rtn = eSeqLocCheck_ok;

    ENa_strand strand = GetStrand(loc, scope);
    if (strand == eNa_strand_unknown  ||  strand == eNa_strand_other) {
        rtn = eSeqLocCheck_warning;
    }

    CTypeConstIterator<CSeq_loc> lit(ConstBegin(loc));
    for (;lit; ++lit) {
        switch (lit->Which()) {
        case CSeq_loc::e_Int:
            if (!IsValid(lit->GetInt(), scope)) {
                rtn = eSeqLocCheck_error;
            }
            break;
        case CSeq_loc::e_Packed_int:
        {
            CTypeConstIterator<CSeq_interval> sit(ConstBegin(*lit));
            for(;sit; ++sit) {
                if (!IsValid(*sit, scope)) {
                    rtn = eSeqLocCheck_error;
                    break;
                }
            }
            break;
        }
        case CSeq_loc::e_Pnt:
            if (!IsValid(lit->GetPnt(), scope)) {
                rtn = eSeqLocCheck_error;
            }
            break;
        case CSeq_loc::e_Packed_pnt:
            if (!IsValid(lit->GetPacked_pnt(), scope)) {
                rtn = eSeqLocCheck_error;
            }
            break;
        default:
            break;
        }
    }
    return rtn;
}


CRef<CSeq_loc> SourceToProduct(const CSeq_feat& feat,
                               const CSeq_loc& source_loc, TS2PFlags flags,
                               CScope* scope, int* frame)
{
    SRelLoc::TFlags rl_flags = 0;
    if (flags & fS2P_NoMerge) {
        rl_flags |= SRelLoc::fNoMerge;
    }
    SRelLoc rl(feat.GetLocation(), source_loc, scope, rl_flags);
    _ASSERT(!rl.m_Ranges.empty());
    rl.m_ParentLoc.Reset(&feat.GetProduct());
    if (feat.GetData().IsCdregion()) {
        // 3:1 ratio
        const CCdregion& cds         = feat.GetData().GetCdregion();
        int              base_frame  = cds.GetFrame();
        if (base_frame > 0) {
            --base_frame;
        }
        if (frame) {
            *frame = 3 - (rl.m_Ranges.front()->GetFrom() + 2 - base_frame) % 3;
        }
        TSeqPos prot_length;
        try {
            prot_length = GetLength(feat.GetProduct(), scope);
        } catch (CNoLength) {
            prot_length = numeric_limits<TSeqPos>::max();
        }
        NON_CONST_ITERATE (SRelLoc::TRanges, it, rl.m_Ranges) {
            if (IsReverse((*it)->GetStrand())) {
                ERR_POST(Warning
                         << "SourceToProduct:"
                         " parent and child have opposite orientations");
            }
            (*it)->SetFrom(((*it)->GetFrom() - base_frame) / 3);
            (*it)->SetTo  (((*it)->GetTo()   - base_frame) / 3);
            if ((flags & fS2P_AllowTer)  &&  (*it)->GetTo() == prot_length) {
                --(*it)->SetTo();
            }
        }
    } else {
        if (frame) {
            *frame = 0; // not applicable; explicitly zero
        }
    }

    return rl.Resolve(scope, rl_flags);
}


CRef<CSeq_loc> ProductToSource(const CSeq_feat& feat, const CSeq_loc& prod_loc,
                               TP2SFlags flags, CScope* scope)
{
    SRelLoc rl(feat.GetProduct(), prod_loc, scope);
    _ASSERT(!rl.m_Ranges.empty());
    rl.m_ParentLoc.Reset(&feat.GetLocation());
    if (feat.GetData().IsCdregion()) {
        // 3:1 ratio
        const CCdregion& cds        = feat.GetData().GetCdregion();
        int              base_frame = cds.GetFrame();
        if (base_frame > 0) {
            --base_frame;
        }
        TSeqPos nuc_length, prot_length;
        try {
            nuc_length = GetLength(feat.GetLocation(), scope);
        } catch (CNoLength) {
            nuc_length = numeric_limits<TSeqPos>::max();
        }
        try {
            prot_length = GetLength(feat.GetProduct(), scope);
        } catch (CNoLength) {
            prot_length = numeric_limits<TSeqPos>::max();
        }
        NON_CONST_ITERATE(SRelLoc::TRanges, it, rl.m_Ranges) {
            _ASSERT( !IsReverse((*it)->GetStrand()) );
            TSeqPos from, to;
            if ((flags & fP2S_Extend)  &&  (*it)->GetFrom() == 0) {
                from = 0;
            } else {
                from = (*it)->GetFrom() * 3 + base_frame;
            }
            if ((flags & fP2S_Extend)  &&  (*it)->GetTo() == prot_length - 1) {
                to = nuc_length - 1;
            } else {
                to = (*it)->GetTo() * 3 + base_frame + 2;
            }
            (*it)->SetFrom(from);
            (*it)->SetTo  (to);
        }
    }

    return rl.Resolve(scope);
}


TSeqPos LocationOffset(const CSeq_loc& outer, const CSeq_loc& inner,
                       EOffsetType how, CScope* scope)
{
    SRelLoc rl(outer, inner, scope);
    if (rl.m_Ranges.empty()) {
        return (TSeqPos)-1;
    }
    bool want_reverse = false;
    {{
        bool outer_is_reverse = IsReverse(GetStrand(outer, scope));
        switch (how) {
        case eOffset_FromStart:
            want_reverse = false;
            break;
        case eOffset_FromEnd:
            want_reverse = true;
            break;
        case eOffset_FromLeft:
            want_reverse = outer_is_reverse;
            break;
        case eOffset_FromRight:
            want_reverse = !outer_is_reverse;
            break;
        }
    }}
    if (want_reverse) {
        return GetLength(outer, scope) - rl.m_Ranges.back()->GetTo();
    } else {
        return rl.m_Ranges.front()->GetFrom();
    }
}


bool TestForStrands(ENa_strand strand1, ENa_strand strand2)
{
    // Check strands. Overlapping rules for strand:
    //   - equal strands overlap
    //   - "both" overlaps with any other
    //   - "unknown" overlaps with any other except "minus"
    return strand1 == strand2
        || strand1 == eNa_strand_both
        || strand2 == eNa_strand_both
        || (strand1 == eNa_strand_unknown  && strand2 != eNa_strand_minus)
        || (strand2 == eNa_strand_unknown  && strand1 != eNa_strand_minus);
}


bool TestForIntervals(CSeq_loc_CI it1, CSeq_loc_CI it2, bool minus_strand)
{
    // Check intervals one by one
    while ( it1  &&  it2 ) {
        if ( !TestForStrands(it1.GetStrand(), it2.GetStrand())  ||
             !it1.GetSeq_id().Equals(it2.GetSeq_id())) {
            return false;
        }
        if ( minus_strand ) {
            if (it1.GetRange().GetFrom()  !=  it2.GetRange().GetFrom() ) {
                // The last interval from loc2 may be shorter than the
                // current interval from loc1
                if (it1.GetRange().GetFrom() > it2.GetRange().GetFrom()  ||
                    ++it2) {
                    return false;
                }
                break;
            }
        }
        else {
            if (it1.GetRange().GetTo()  !=  it2.GetRange().GetTo() ) {
                // The last interval from loc2 may be shorter than the
                // current interval from loc1
                if (it1.GetRange().GetTo() < it2.GetRange().GetTo()  ||
                    ++it2) {
                    return false;
                }
                break;
            }
        }
        // Go to the next interval start
        if ( !(++it2) ) {
            break;
        }
        if ( !(++it1) ) {
            return false; // loc1 has not enough intervals
        }
        if ( minus_strand ) {
            if (it1.GetRange().GetTo() != it2.GetRange().GetTo()) {
                return false;
            }
        }
        else {
            if (it1.GetRange().GetFrom() != it2.GetRange().GetFrom()) {
                return false;
            }
        }
    }
    return true;
}


int x_TestForOverlap_MultiSeq(const CSeq_loc& loc1,
                              const CSeq_loc& loc2,
                              EOverlapType type)
{
    // Special case of TestForOverlap() - multi-sequences locations
    switch (type) {
    case eOverlap_Simple:
        {
            // Find all intersecting intervals
            int diff = 0;
            for (CSeq_loc_CI li1(loc1); li1; ++li1) {
                for (CSeq_loc_CI li2(loc2); li2; ++li2) {
                    if ( !li1.GetSeq_id().Equals(li2.GetSeq_id()) ) {
                        continue;
                    }
                    CRange<TSeqPos> rg1 = li1.GetRange();
                    CRange<TSeqPos> rg2 = li2.GetRange();
                    if ( rg1.GetTo() >= rg2.GetFrom()  &&
                         rg1.GetFrom() <= rg2.GetTo() ) {
                        diff += abs(int(rg2.GetFrom() - rg1.GetFrom())) +
                            abs(int(rg1.GetTo() - rg2.GetTo()));
                    }
                }
            }
            return diff ? diff : -1;
        }
    case eOverlap_Contained:
        {
            // loc2 is contained in loc1
            CHandleRangeMap rm1, rm2;
            rm1.AddLocation(loc1);
            rm2.AddLocation(loc2);
            int diff = 0;
            CHandleRangeMap::const_iterator it2 = rm2.begin();
            for ( ; it2 != rm2.end(); ++it2) {
                CHandleRangeMap::const_iterator it1 =
                    rm1.GetMap().find(it2->first);
                if (it1 == rm1.end()) {
                    // loc2 has region on a sequence not present in loc1
                    diff = -1;
                    break;
                }
                CRange<TSeqPos> rg1 = it1->second.GetOverlappingRange();
                CRange<TSeqPos> rg2 = it2->second.GetOverlappingRange();
                if ( rg1.GetFrom() <= rg2.GetFrom()  &&
                    rg1.GetTo() >= rg2.GetTo() ) {
                    diff += (rg2.GetFrom() - rg1.GetFrom()) +
                        (rg1.GetTo() - rg2.GetTo());
                }
                else {
                    // Found an interval on loc2 not contained in loc1
                    diff = -1;
                    break;
                }
            }
            return diff;
        }
    case eOverlap_Contains:
        {
            // loc2 is contained in loc1
            CHandleRangeMap rm1, rm2;
            rm1.AddLocation(loc1);
            rm2.AddLocation(loc2);
            int diff = 0;
            CHandleRangeMap::const_iterator it1 = rm1.begin();
            for ( ; it1 != rm1.end(); ++it1) {
                CHandleRangeMap::const_iterator it2 =
                    rm2.GetMap().find(it1->first);
                if (it2 == rm2.end()) {
                    // loc1 has region on a sequence not present in loc2
                    diff = -1;
                    break;
                }
                CRange<TSeqPos> rg1 = it1->second.GetOverlappingRange();
                CRange<TSeqPos> rg2 = it2->second.GetOverlappingRange();
                if ( rg2.GetFrom() <= rg1.GetFrom()  &&
                    rg2.GetTo() >= rg1.GetTo() ) {
                    diff += (rg1.GetFrom() - rg2.GetFrom()) +
                        (rg2.GetTo() - rg1.GetTo());
                }
                else {
                    // Found an interval on loc1 not contained in loc2
                    diff = -1;
                    break;
                }
            }
            return diff;
        }
    case eOverlap_Subset:
    case eOverlap_CheckIntervals:
    case eOverlap_Interval:
        {
            // For this types the function should not be called
            NCBI_THROW(CObjmgrUtilException, eNotImplemented,
                "TestForOverlap() -- error processing multi-ID seq-loc");
        }
    }
    return -1;
}


int TestForOverlap(const CSeq_loc& loc1,
                   const CSeq_loc& loc2,
                   EOverlapType type,
                   TSeqPos circular_len)
{
    CRange<TSeqPos> rg1, rg2;
    bool multi_seq = false;
    try {
        rg1 = loc1.GetTotalRange();
        rg2 = loc2.GetTotalRange();
    }
    catch (exception&) {
        // Can not use total range for multi-sequence locations
        if (type == eOverlap_Simple  ||
            type == eOverlap_Contained  ||
            type == eOverlap_Contains) {
            // Can not process circular multi-id locations
            if (circular_len != 0  &&  circular_len != kInvalidSeqPos) {
                throw;
            }
            return x_TestForOverlap_MultiSeq(loc1, loc2, type);
        }
        multi_seq = true;
    }

    ENa_strand strand1 = GetStrand(loc1);
    ENa_strand strand2 = GetStrand(loc2);
    if ( !TestForStrands(strand1, strand2) )
        return -1;
    switch (type) {
    case eOverlap_Simple:
        {
            if (circular_len != kInvalidSeqPos) {
                TSeqPos from1 = loc1.GetStart(circular_len);
                TSeqPos from2 = loc2.GetStart(circular_len);
                TSeqPos to1 = loc1.GetEnd(circular_len);
                TSeqPos to2 = loc2.GetEnd(circular_len);
                if (from1 > to1) {
                    if (from2 > to2) {
                        // Both locations are circular and must intersect at 0
                        return abs(int(from2 - from1)) +
                            abs(int(to1 - to2));
                    }
                    else {
                        // Only the first location is circular, rg2 may be used
                        // for the second one.
                        int loc_len = rg2.GetLength() + loc1.GetCircularLength(circular_len);
                        if (from1 < rg2.GetFrom()  ||  to1 > rg2.GetTo()) {
                            // loc2 is completely in loc1
                            return loc_len - 2*rg2.GetLength();
                        }
                        if (from1 < rg2.GetTo()) {
                            return loc_len - (rg2.GetTo() - from1);
                        }
                        if (rg2.GetFrom() < to1) {
                            return loc_len - (to1 - rg2.GetFrom());
                        }
                        return -1;
                    }
                }
                else if (from2 > to2) {
                    // Only the second location is circular
                    int loc_len = rg1.GetLength() + loc2.GetCircularLength(circular_len);
                    if (from2 < rg1.GetFrom()  ||  to2 > rg1.GetTo()) {
                        // loc2 is completely in loc1
                        return loc_len - 2*rg1.GetLength();
                    }
                    if (from2 < rg1.GetTo()) {
                        return loc_len - (rg1.GetTo() - from2);
                    }
                    if (rg1.GetFrom() < to2) {
                        return loc_len - (to2 - rg1.GetFrom());
                    }
                    return -1;
                }
                // Locations are not circular, proceed to normal calculations
            }
            if ( rg1.GetTo() >= rg2.GetFrom()  &&
                rg1.GetFrom() <= rg2.GetTo() ) {
                return abs(int(rg2.GetFrom() - rg1.GetFrom())) +
                    abs(int(rg1.GetTo() - rg2.GetTo()));
            }
            return -1;
        }
    case eOverlap_Contained:
        {
            if (circular_len != kInvalidSeqPos) {
                TSeqPos from1 = loc1.GetStart(circular_len);
                TSeqPos from2 = loc2.GetStart(circular_len);
                TSeqPos to1 = loc1.GetEnd(circular_len);
                TSeqPos to2 = loc2.GetEnd(circular_len);
                if (from1 > to1) {
                    if (from2 > to2) {
                        return (from1 <= from2  &&  to1 >= to2) ?
                            int(from2 - from1) - int(to1 - to2) : -1;
                    }
                    else {
                        if (rg2.GetFrom() >= from1  ||  rg2.GetTo() <= to1) {
                            return loc1.GetCircularLength(circular_len) -
                                rg2.GetLength();
                        }
                        return -1;
                    }
                }
                else if (from2 > to2) {
                    // Non-circular location can not contain a circular one
                    return -1;
                }
            }
            if ( rg1.GetFrom() <= rg2.GetFrom()  &&
                rg1.GetTo() >= rg2.GetTo() ) {
                return (rg2.GetFrom() - rg1.GetFrom()) +
                    (rg1.GetTo() - rg2.GetTo());
            }
            return -1;
        }
    case eOverlap_Contains:
        {
            if (circular_len != kInvalidSeqPos) {
                TSeqPos from1 = loc1.GetStart(circular_len);
                TSeqPos from2 = loc2.GetStart(circular_len);
                TSeqPos to1 = loc1.GetEnd(circular_len);
                TSeqPos to2 = loc2.GetEnd(circular_len);
                if (from1 > to1) {
                    if (from2 > to2) {
                        return (from2 <= from1  &&  to2 >= to1) ?
                            int(from1 - from2) - int(to2 - to1) : -1;
                    }
                    else {
                        // Non-circular location can not contain a circular one
                        return -1;
                    }
                }
                else if (from2 > to2) {
                    if (rg1.GetFrom() >= from2  ||  rg1.GetTo() <= to2) {
                        return loc2.GetCircularLength(circular_len) -
                            rg1.GetLength();
                    }
                    return -1;
                }
            }
            if ( rg2.GetFrom() <= rg1.GetFrom()  &&
                rg2.GetTo() >= rg1.GetTo() ) {
                return (rg1.GetFrom() - rg2.GetFrom()) +
                    (rg2.GetTo() - rg1.GetTo());
            }
            return -1;
        }
    case eOverlap_Subset:
        {
            // loc1 should contain loc2
            if ( Compare(loc1, loc2) != eContains ) {
                return -1;
            }
            return GetLength(loc1) - GetLength(loc2);
        }
    case eOverlap_CheckIntervals:
        {
            if ( !multi_seq  &&
                (rg1.GetFrom() > rg2.GetTo()  || rg1.GetTo() < rg2.GetTo()) ) {
                return -1;
            }
            // Check intervals' boundaries
            CSeq_loc_CI it1(loc1);
            CSeq_loc_CI it2(loc2);
            if (!it1  ||  !it2) {
                break;
            }
            // check case when strand is minus
            if (it2.GetStrand() == eNa_strand_minus) {
                // The first interval should be treated as the last one
                // for minuns strands.
                TSeqPos loc2end = it2.GetRange().GetTo();
                TSeqPos loc2start = it2.GetRange().GetFrom();
                // Find the first interval in loc1 intersecting with loc2
                for ( ; it1  &&  it1.GetRange().GetTo() >= loc2start; ++it1) {
                    if (it1.GetRange().GetTo() >= loc2end  &&
                        TestForIntervals(it1, it2, true)) {
                        return GetLength(loc1) - GetLength(loc2);
                    }
                }
            }
            else {
                TSeqPos loc2start = it2.GetRange().GetFrom();
                //TSeqPos loc2end = it2.GetRange().GetTo();
                // Find the first interval in loc1 intersecting with loc2
                for ( ; it1  /*&&  it1.GetRange().GetFrom() <= loc2end*/; ++it1) {
                    if (it1.GetSeq_id().Equals(it2.GetSeq_id())  &&
                        it1.GetRange().GetFrom() <= loc2start  &&
                        TestForIntervals(it1, it2, false)) {
                        return GetLength(loc1) - GetLength(loc2);
                    }
                }
            }
            return -1;
        }
    case eOverlap_Interval:
        {
            return (Compare(loc1, loc2) == eNoOverlap) ? -1
                : abs(int(rg2.GetFrom() - rg1.GetFrom())) +
                abs(int(rg1.GetTo() - rg2.GetTo()));
        }
    }
    return -1;
}


CConstRef<CSeq_feat> x_GetBestOverlappingFeat(const CSeq_loc& loc,
                                              CSeqFeatData::E_Choice feat_type,
                                              CSeqFeatData::ESubtype feat_subtype,
                                              EOverlapType overlap_type,
                                              CScope& scope)
{
    bool revert_locations = false;
    SAnnotSelector::EOverlapType annot_overlap_type;
    switch (overlap_type) {
    case eOverlap_Simple:
    case eOverlap_Contained:
    case eOverlap_Contains:
        // Require total range overlap
        annot_overlap_type = SAnnotSelector::eOverlap_TotalRange;
        break;
    case eOverlap_Subset:
    case eOverlap_CheckIntervals:
    case eOverlap_Interval:
        revert_locations = true;
        // there's no break here - proceed to "default"
    default:
        // Require intervals overlap
        annot_overlap_type = SAnnotSelector::eOverlap_Intervals;
        break;
    }

    CConstRef<CSeq_feat> feat_ref;
    int diff = -1;

    // Check if the sequence is circular
    TSeqPos circular_length = kInvalidSeqPos;
    {{
        const CSeq_id* single_id = 0;
        loc.CheckId(single_id);
        if ( single_id ) {
            CBioseq_Handle h = scope.GetBioseqHandle(*single_id);
            if ( h
                &&  h.IsSetInst_Topology()
                &&  h.GetInst_Topology() == CSeq_inst::eTopology_circular ) {
                circular_length = h.GetBioseqLength();
            }
        }
    }}

    CFeat_CI feat_it(scope, loc, SAnnotSelector()
        .SetFeatType(feat_type)
        .SetFeatSubtype(feat_subtype)
        .SetOverlapType(annot_overlap_type)
        .SetResolveTSE());
    for ( ; feat_it; ++feat_it) {
        // treat subset as a special case
        int cur_diff = ( !revert_locations ) ?
            TestForOverlap(loc,
                           feat_it->GetLocation(),
                           overlap_type,
                           circular_length) :
            TestForOverlap(feat_it->GetLocation(),
                           loc,
                           overlap_type,
                           circular_length);
        if (cur_diff < 0)
            continue;
        if ( cur_diff < diff  ||  diff < 0 ) {
            diff = cur_diff;
            feat_ref = &feat_it->GetMappedFeature();
        }
    }
    return feat_ref;
}


CConstRef<CSeq_feat> GetBestOverlappingFeat(const CSeq_loc& loc,
                                            CSeqFeatData::E_Choice feat_type,
                                            EOverlapType overlap_type,
                                            CScope& scope)
{
    return x_GetBestOverlappingFeat(loc,
        feat_type, CSeqFeatData::eSubtype_any,
        overlap_type, scope);
}


CConstRef<CSeq_feat> GetBestOverlappingFeat(const CSeq_loc& loc,
                                            CSeqFeatData::ESubtype feat_type,
                                            EOverlapType overlap_type,
                                            CScope& scope)
{
    return x_GetBestOverlappingFeat(loc,
        CSeqFeatData::GetTypeFromSubtype(feat_type), feat_type,
        overlap_type, scope);
}


CConstRef<CSeq_feat> GetOverlappingGene(const CSeq_loc& loc, CScope& scope)
{
    return GetBestOverlappingFeat(loc, CSeqFeatData::eSubtype_gene,
        eOverlap_Contains, scope);
}


CConstRef<CSeq_feat> GetOverlappingmRNA(const CSeq_loc& loc, CScope& scope)
{
    return GetBestOverlappingFeat(loc, CSeqFeatData::eSubtype_mRNA,
        eOverlap_Contains, scope);
}


CConstRef<CSeq_feat> GetOverlappingCDS(const CSeq_loc& loc, CScope& scope)
{
    return GetBestOverlappingFeat(loc, CSeqFeatData::eSubtype_cdregion,
        eOverlap_Contains, scope);
}


CConstRef<CSeq_feat> GetOverlappingPub(const CSeq_loc& loc, CScope& scope)
{
    return GetBestOverlappingFeat(loc, CSeqFeatData::eSubtype_pub,
        eOverlap_Contains, scope);
}


CConstRef<CSeq_feat> GetOverlappingSource(const CSeq_loc& loc, CScope& scope)
{
    return GetBestOverlappingFeat(loc, CSeqFeatData::eSubtype_biosrc,
        eOverlap_Contains, scope);
}


CConstRef<CSeq_feat> GetOverlappingOperon(const CSeq_loc& loc, CScope& scope)
{
    return GetBestOverlappingFeat(loc, CSeqFeatData::eSubtype_operon,
        eOverlap_Contains, scope);
}


int SeqLocPartialCheck(const CSeq_loc& loc, CScope* scope)
{
    unsigned int retval = 0;
    if (!scope) {
        return retval;
    }

    // Find first and last Seq-loc
    const CSeq_loc *first = 0, *last = 0;
    for ( CSeq_loc_CI loc_iter(loc); loc_iter; ++loc_iter ) {
        if ( first == 0 ) {
            first = &(loc_iter.GetSeq_loc());
        }
        last = &(loc_iter.GetSeq_loc());
    }
    if (!first) {
        return retval;
    }

    CSeq_loc_CI i2(loc, CSeq_loc_CI::eEmpty_Allow);
    for ( ; i2; ++i2 ) {
        const CSeq_loc* slp = &(i2.GetSeq_loc());
        switch (slp->Which()) {
        case CSeq_loc::e_Null:
            if (slp == first) {
                retval |= eSeqlocPartial_Start;
            } else if (slp == last) {
                retval |= eSeqlocPartial_Stop;
            } else {
                retval |= eSeqlocPartial_Internal;
            }
            break;
        case CSeq_loc::e_Int:
            {
                const CSeq_interval& itv = slp->GetInt();
                if (itv.IsSetFuzz_from()) {
                    const CInt_fuzz& fuzz = itv.GetFuzz_from();
                    if (fuzz.Which() == CInt_fuzz::e_Lim) {
                        CInt_fuzz::ELim lim = fuzz.GetLim();
                        if (lim == CInt_fuzz::eLim_gt) {
                            retval |= eSeqlocPartial_Limwrong;
                        } else if (lim == CInt_fuzz::eLim_lt  ||
                            lim == CInt_fuzz::eLim_unk) {
                            if (itv.IsSetStrand()  &&
                                itv.GetStrand() == eNa_strand_minus) {
                                if (slp == last) {
                                    retval |= eSeqlocPartial_Stop;
                                } else {
                                    retval |= eSeqlocPartial_Internal;
                                }
                                if (itv.GetFrom() != 0) {
                                    if (slp == last) {
                                        retval |= eSeqlocPartial_Nostop;
                                    } else {
                                        retval |= eSeqlocPartial_Nointernal;
                                    }
                                }
                            } else {
                                if (slp == first) {
                                    retval |= eSeqlocPartial_Start;
                                } else {
                                    retval |= eSeqlocPartial_Internal;
                                }
                                if (itv.GetFrom() != 0) {
                                    if (slp == first) {
                                        retval |= eSeqlocPartial_Nostart;
                                    } else {
                                        retval |= eSeqlocPartial_Nointernal;
                                    }
                                }
                            }
                        }
                    }
                }
                
                if (itv.IsSetFuzz_to()) {
                    const CInt_fuzz& fuzz = itv.GetFuzz_to();
                    CInt_fuzz::ELim lim = fuzz.IsLim() ? 
                        fuzz.GetLim() : CInt_fuzz::eLim_unk;
                    if (lim == CInt_fuzz::eLim_lt) {
                        retval |= eSeqlocPartial_Limwrong;
                    } else if (lim == CInt_fuzz::eLim_gt  ||
                        lim == CInt_fuzz::eLim_unk) {
                        CBioseq_Handle hnd =
                            scope->GetBioseqHandle(itv.GetId());
                        bool miss_end = false;
                        if ( hnd ) {
                            CBioseq_Handle::TBioseqCore bc = hnd.GetBioseqCore();
                            
                            if (itv.GetTo() != bc->GetInst().GetLength() - 1) {
                                miss_end = true;
                            }
                        }
                        if (itv.IsSetStrand()  &&
                            itv.GetStrand() == eNa_strand_minus) {
                            if (slp == first) {
                                retval |= eSeqlocPartial_Start;
                            } else {
                                retval |= eSeqlocPartial_Internal;
                            }
                            if (miss_end) {
                                if (slp == first /* was last */) {
                                    retval |= eSeqlocPartial_Nostart;
                                } else {
                                    retval |= eSeqlocPartial_Nointernal;
                                }
                            }
                        } else {
                            if (slp == last) {
                                retval |= eSeqlocPartial_Stop;
                            } else {
                                retval |= eSeqlocPartial_Internal;
                            }
                            if (miss_end) {
                                if (slp == last) {
                                    retval |= eSeqlocPartial_Nostop;
                                } else {
                                    retval |= eSeqlocPartial_Nointernal;
                                }
                            }
                        }
                    }
                }
            }
            break;
        case CSeq_loc::e_Pnt:
            if (slp->GetPnt().IsSetFuzz()) {
                const CInt_fuzz& fuzz = slp->GetPnt().GetFuzz();
                if (fuzz.Which() == CInt_fuzz::e_Lim) {
                    CInt_fuzz::ELim lim = fuzz.GetLim();
                    if (lim == CInt_fuzz::eLim_gt  ||
                        lim == CInt_fuzz::eLim_lt  ||
                        lim == CInt_fuzz::eLim_unk) {
                        if (slp == first) {
                            retval |= eSeqlocPartial_Start;
                        } else if (slp == last) {
                            retval |= eSeqlocPartial_Stop;
                        } else {
                            retval |= eSeqlocPartial_Internal;
                        }
                    }
                }
            }
            break;
        case CSeq_loc::e_Packed_pnt:
            if (slp->GetPacked_pnt().IsSetFuzz()) {
                const CInt_fuzz& fuzz = slp->GetPacked_pnt().GetFuzz();
                if (fuzz.Which() == CInt_fuzz::e_Lim) {
                    CInt_fuzz::ELim lim = fuzz.GetLim();
                    if (lim == CInt_fuzz::eLim_gt  ||
                        lim == CInt_fuzz::eLim_lt  ||
                        lim == CInt_fuzz::eLim_unk) {
                        if (slp == first) {
                            retval |= eSeqlocPartial_Start;
                        } else if (slp == last) {
                            retval |= eSeqlocPartial_Stop;
                        } else {
                            retval |= eSeqlocPartial_Internal;
                        }
                    }
                }
            }
            break;
        case CSeq_loc::e_Whole:
        {
            // Get the Bioseq referred to by Whole
            CBioseq_Handle bsh = scope->GetBioseqHandle(slp->GetWhole());
            if ( !bsh ) {
                break;
            }
            // Check for CMolInfo on the biodseq
            CSeqdesc_CI di( bsh, CSeqdesc::e_Molinfo );
            if ( !di ) {
                // If no CSeq_descr, nothing can be done
                break;
            }
            // First try to loop through CMolInfo
            const CMolInfo& mi = di->GetMolinfo();
            if (!mi.IsSetCompleteness()) {
                continue;
            }
            switch (mi.GetCompleteness()) {
            case CMolInfo::eCompleteness_no_left:
                if (slp == first) {
                    retval |= eSeqlocPartial_Start;
                } else {
                    retval |= eSeqlocPartial_Internal;
                }
                break;
            case CMolInfo::eCompleteness_no_right:
                if (slp == last) {
                    retval |= eSeqlocPartial_Stop;
                } else {
                    retval |= eSeqlocPartial_Internal;
                }
                break;
            case CMolInfo::eCompleteness_partial:
                retval |= eSeqlocPartial_Other;
                break;
            case CMolInfo::eCompleteness_no_ends:
                retval |= eSeqlocPartial_Start;
                retval |= eSeqlocPartial_Stop;
                break;
            default:
                break;
            }
            break;
        }
        default:
            break;
        }
    }
    return retval;
}


typedef CSeq_loc::TRange    TRange;
typedef vector<TRange>      TRangeVec;


CSeq_loc* SeqLocMerge
(const CBioseq_Handle& target,
 const CSeq_loc& loc1,
 const CSeq_loc& loc2,
 TSeqLocFlags flags)
{
    _ASSERT(target);

    vector< CConstRef<CSeq_loc> > locs;
    locs.push_back(CConstRef<CSeq_loc>(&loc1));
    locs.push_back(CConstRef<CSeq_loc>(&loc2));
    return SeqLocMerge(target, locs, flags);
}


static void s_CollectRanges(const CSeq_loc& loc, TRangeVec& ranges)
{
    // skipping empty / nulls
    for ( CSeq_loc_CI li(loc); li; ++li ) {
        ranges.push_back(li.GetRange());
    }
}


static void s_MergeRanges(TRangeVec& ranges, TSeqLocFlags flags)
{
    bool merge = (flags & fMergeIntervals) != 0;
    bool fuse  = (flags & fFuseAbutting) != 0;

    if ( !merge  &&  !fuse ) {
        return;
    }

    TRangeVec::iterator next = ranges.begin();
    TRangeVec::iterator curr = next++;
    bool advance = true;
    while ( next != ranges.end() ) {
        advance = true;
        if ( (merge  &&  curr->GetTo() >= next->GetFrom())  ||
             (fuse   &&  curr->GetTo() + 1 == next->GetFrom()) ) {
            *curr += *next;
            next = ranges.erase(next);
            advance = false;
        }
        if ( advance ) {
            curr = next++;
        }
    }
}


static CSeq_loc* s_CreateSeqLoc
(CSeq_id& id,
 const TRangeVec& ranges,
 ENa_strand strand,
 bool rearranged,
 bool add_null)
{
    static const  CSeq_loc null_loc(CSeq_loc::e_Null);

    bool has_pnt = false;
    bool has_int = false;

    if ( ranges.size() < 2 ) {
        add_null = false;
    }

    if ( !add_null ) {
        ITERATE(TRangeVec, it, ranges) {
            if ( it->GetLength() == 1 ) {
                has_pnt = true;
            } else {
                has_int = true;
            }
        }
    }
    
    CSeq_loc::E_Choice loc_type = CSeq_loc::e_not_set;
    if ( add_null ) {
        loc_type = CSeq_loc::e_Mix;
    } else if ( has_pnt  &&  has_int ) {  // points and intervals  => mix location
        loc_type = CSeq_loc::e_Mix;
    } else if ( has_pnt ) {  // only points
        loc_type = ranges.size() == 1 ? CSeq_loc::e_Pnt : CSeq_loc::e_Packed_pnt;
    } else if ( has_int ) {  // only intervals
        loc_type = ranges.size() == 1 ? CSeq_loc::e_Int : CSeq_loc::e_Packed_int;
    } else {
        loc_type = CSeq_loc::e_Null;
    }

    CRef<CSeq_loc> result;
    switch ( loc_type ) {
    case CSeq_loc::e_Pnt:
        {
            result.Reset(new CSeq_loc(id, ranges.front().GetFrom(), strand));
            break;
        }
    case CSeq_loc::e_Packed_pnt:
        {
            CPacked_seqpnt::TPoints points(ranges.size());;
            ITERATE(TRangeVec, it, ranges) {
                points.push_back(it->GetFrom());
            }
            result.Reset(new CSeq_loc(id, points, strand));
            
            break;
        }
    case CSeq_loc::e_Int:
        {
            const TRange& r = ranges.front();
            result.Reset(new CSeq_loc(id, r.GetFrom(), r.GetTo(), strand));
            break;
        }
    case CSeq_loc::e_Packed_int:
        {
            result.Reset(new CSeq_loc(id, ranges, strand));
            if ( rearranged ) {
                // the first 2 intervals span the origin
                CPacked_seqint::Tdata ivals = result->SetPacked_int().Set();
                CPacked_seqint::Tdata::iterator it = ivals.begin();
                if ( strand == eNa_strand_minus ) {
                    (*it)->SetFuzz_from().SetLim(CInt_fuzz::eLim_circle);
                    ++it;
                    (*it)->SetFuzz_to().SetLim(CInt_fuzz::eLim_circle);
                } else {
                    (*it)->SetFuzz_to().SetLim(CInt_fuzz::eLim_circle);
                    ++it;
                    (*it)->SetFuzz_from().SetLim(CInt_fuzz::eLim_circle);
                }
            }
            break;
        }
    case CSeq_loc::e_Mix:
        {
            result.Reset(new CSeq_loc);
            CSeq_loc_mix& mix = result->SetMix();
            TRangeVec::const_iterator last = ranges.end(); --last;
            ITERATE(TRangeVec, it, ranges) {
                if ( it->GetLength() == 1 ) {
                    mix.AddSeqLoc(*new CSeq_loc(id, it->GetFrom(), strand));
                } else {
                    mix.AddSeqLoc(*new CSeq_loc(id, it->GetFrom(), it->GetTo(), strand));
                }
                if ( add_null  &&  it != last ) {
                    mix.AddSeqLoc(null_loc);
                }
            }
            break;
        }
    case CSeq_loc::e_Null:
        {
            result.Reset(new CSeq_loc);
            result->SetNull();
            break;
        }
    default:
        {
            result.Reset();
        }
    };

    return result.Release();
}


static void s_RearrangeRanges(TRangeVec& ranges)
{
    deque<TRange> temp(ranges.size());
    copy(ranges.begin(), ranges.end(), temp.begin());
    ranges.clear();
    temp.push_front(temp.back());  
    temp.pop_back();
    copy(temp.begin(), temp.end(), back_inserter(ranges));
}


CSeq_loc* SeqLocMergeOne
(const CBioseq_Handle& target,
 const CSeq_loc& loc,
 TSeqLocFlags flags)
{
    _ASSERT(target);

    TRangeVec ranges;

    CRef<CSeq_id> id(new CSeq_id);
    id->Assign(GetId(target, eGetId_Best));

    const CSeq_inst& inst = target.GetInst();
    CSeq_inst::TTopology topology = inst.CanGetTopology() ?
        inst.GetTopology() : CSeq_inst::eTopology_not_set;
    bool circular = (topology == CSeq_inst::eTopology_circular);
    if ( circular ) {  // circular topology overrides fSingleInterval flag
        flags &= ~fSingleInterval;
    }
    TSeqPos seq_len = inst.IsSetLength() ? inst.GetLength() : 0;

    // map the location to the target bioseq
    CSeq_loc_Mapper mapper(target);
    mapper.PreserveDestinationLocs();
    CRef<CSeq_loc> mapped_loc = mapper.Map(loc);

    ENa_strand strand = GetStrand(*mapped_loc);
    bool rearranged = false;

    if ( (flags & fSingleInterval) != 0 ) {
            ranges.push_back(mapped_loc->GetTotalRange());
    } else {
        if ( strand == eNa_strand_other ) {  // multiple strands in location
            return mapped_loc.Release();
        }
    
        s_CollectRanges(*mapped_loc, ranges);
        sort(ranges.begin(), ranges.end());  // on the plus strand
        s_MergeRanges(ranges, flags);
        if ( strand == eNa_strand_minus ) {
            reverse(ranges.begin(), ranges.end());
        }
        // if circular bioseq and ranges span the origin, rearrange
        if ( circular ) {
            if ( ((strand == eNa_strand_minus)             &&
                  (ranges.front().GetTo() == seq_len - 1)  &&
                  (ranges.back().GetFrom() == 0))           ||
                 ((strand != eNa_strand_minus)             &&
                  (ranges.front().GetFrom() == 0)          &&
                  (ranges.back().GetTo() == seq_len - 1)) ) {
                rearranged = true;
                s_RearrangeRanges(ranges);
            }
        }
    }   

    return s_CreateSeqLoc(*id, ranges, strand, rearranged, (flags & fAddNulls) != 0);
}


static CSeq_interval* s_SeqIntRevCmp(const CSeq_interval& i, CScope* scope)
{
    CRef<CSeq_interval> rev_int(new CSeq_interval);
    rev_int->Assign(i);
    
    ENa_strand s = i.CanGetStrand() ? i.GetStrand() : eNa_strand_unknown;
    rev_int->SetStrand(Reverse(s));

    return rev_int.Release();
}


static CSeq_point* s_SeqPntRevCmp(const CSeq_point& pnt, CScope* scope)
{
    CRef<CSeq_point> rev_pnt(new CSeq_point);
    rev_pnt->Assign(pnt);
    
    ENa_strand s = pnt.CanGetStrand() ? pnt.GetStrand() : eNa_strand_unknown;
    rev_pnt->SetStrand(Reverse(s));

    return rev_pnt.Release();
}


CSeq_loc* SeqLocRevCmp(const CSeq_loc& loc, CScope* scope)
{
    CRef<CSeq_loc> rev_loc(new CSeq_loc);

    switch ( loc.Which() ) {

    // -- reverse is the same.
    case CSeq_loc::e_Null:
    case CSeq_loc::e_Empty:
    case CSeq_loc::e_Whole:
        rev_loc->Assign(loc);
        break;

    // -- just reverse the strand
    case CSeq_loc::e_Int:
        rev_loc->SetInt(*s_SeqIntRevCmp(loc.GetInt(), scope));
        break;
    case CSeq_loc::e_Pnt:
        rev_loc->SetPnt(*s_SeqPntRevCmp(loc.GetPnt(), scope));
        break;
    case CSeq_loc::e_Packed_pnt:
        rev_loc->SetPacked_pnt().Assign(loc.GetPacked_pnt());
        rev_loc->SetPacked_pnt().SetStrand(Reverse(GetStrand(loc, scope)));
        break;

    // -- possibly more than one sequence
    case CSeq_loc::e_Packed_int:
    {
        // reverse each interval and store them in reverse order
        typedef CRef< CSeq_interval > TInt;
        CPacked_seqint& pint = rev_loc->SetPacked_int();
        ITERATE (CPacked_seqint::Tdata, it, loc.GetPacked_int().Get()) {
            pint.Set().push_front(TInt(s_SeqIntRevCmp(**it, scope)));
        }
        break;
    }
    case CSeq_loc::e_Mix:
    {
        // reverse each location and store them in reverse order
        typedef CRef< CSeq_loc > TLoc;
        CSeq_loc_mix& mix = rev_loc->SetMix();
        ITERATE (CSeq_loc_mix::Tdata, it, loc.GetMix().Get()) {
            mix.Set().push_front(TLoc(SeqLocRevCmp(**it, scope)));
        }
        break;
    }
    case CSeq_loc::e_Equiv:
    {
        // reverse each location (no need to reverse order)
        typedef CRef< CSeq_loc > TLoc;
        CSeq_loc_equiv& e = rev_loc->SetEquiv();
        ITERATE (CSeq_loc_equiv::Tdata, it, loc.GetEquiv().Get()) {
            e.Set().push_back(TLoc(SeqLocRevCmp(**it, scope)));
        }
        break;
    }

    case CSeq_loc::e_Bond:
    {
        CSeq_bond& bond = rev_loc->SetBond();
        bond.SetA(*s_SeqPntRevCmp(loc.GetBond().GetA(), scope));
        if ( loc.GetBond().CanGetB() ) {
            bond.SetA(*s_SeqPntRevCmp(loc.GetBond().GetB(), scope));
        }
    }
        
    // -- not supported
    case CSeq_loc::e_Feat:
    default:
        NCBI_THROW(CException, eUnknown,
            "CSeq_loc::SeqLocRevCmp -- unsupported location type");
    }

    return rev_loc.Release();
}


// Get the encoding CDS feature of a given protein sequence.
const CSeq_feat* GetCDSForProduct(const CBioseq& product, CScope* scope)
{
    if ( scope == 0 ) {
        return 0;
    }

    return GetCDSForProduct(scope->GetBioseqHandle(product));
}

const CSeq_feat* GetCDSForProduct(const CBioseq_Handle& bsh)
{
    if ( bsh ) {
        CFeat_CI fi(bsh, 
                    0, 0,
                    CSeqFeatData::e_Cdregion,
                    SAnnotSelector::eOverlap_Intervals,
                    SAnnotSelector::eResolve_TSE,
                    CFeat_CI::e_Product,
                    0);
        if ( fi ) {
            // return the first one (should be the one packaged on the
            // nuc-prot set).
            return &(fi->GetOriginalFeature());
        }
    }

    return 0;
}


// Get the mature peptide feature of a protein
const CSeq_feat* GetPROTForProduct(const CBioseq& product, CScope* scope)
{
    if ( scope == 0 ) {
        return 0;
    }

    return GetPROTForProduct(scope->GetBioseqHandle(product));
}

const CSeq_feat* GetPROTForProduct(const CBioseq_Handle& bsh)
{
    if ( bsh ) {
        CFeat_CI fi(bsh, 
                    0, 0,
                    CSeqFeatData::e_Prot,
                    SAnnotSelector::eOverlap_Intervals,
                    SAnnotSelector::eResolve_TSE,
                    CFeat_CI::e_Product,
                    0);
        if ( fi ) {
            return &(fi->GetOriginalFeature());
        }
    }

    return 0;
}



// Get the encoding mRNA feature of a given mRNA (cDNA) bioseq.
const CSeq_feat* GetmRNAForProduct(const CBioseq& product, CScope* scope)
{
    if ( scope == 0 ) {
        return 0;
    }

    return GetmRNAForProduct(scope->GetBioseqHandle(product));
}

const CSeq_feat* GetmRNAForProduct(const CBioseq_Handle& bsh)
{
    if ( bsh ) {
        SAnnotSelector as;
        as.SetFeatSubtype(CSeqFeatData::eSubtype_mRNA);
        as.SetByProduct();

        CFeat_CI fi(bsh, 0, 0, as);
        if ( fi ) {
            return &(fi->GetOriginalFeature());
        }
    }

    return 0;
}


// Get the encoding sequence of a protein
const CBioseq* GetNucleotideParent(const CBioseq& product, CScope* scope)
{
    if ( scope == 0 ) {
        return 0;
    }
    CBioseq_Handle bsh = GetNucleotideParent(scope->GetBioseqHandle(product));
    return bsh ? &(bsh.GetBioseq()) : 0;
}

CBioseq_Handle GetNucleotideParent(const CBioseq_Handle& bsh)
{
    // If protein use CDS to get to the encoding Nucleotide.
    // if nucleotide (cDNA) use mRNA feature.
    const CSeq_feat* sfp = bsh.GetBioseqMolType() == CSeq_inst::eMol_aa ?
        GetCDSForProduct(bsh) : GetmRNAForProduct(bsh);

    CBioseq_Handle ret;
    if ( sfp ) {
        ret = bsh.GetScope().GetBioseqHandle(sfp->GetLocation());
    }
    return ret;
}


END_SCOPE(sequence)


void CFastaOstream::Write(const CBioseq_Handle& handle,
                          const CSeq_loc* location)
{
    WriteTitle(handle);
    WriteSequence(handle, location);
}


void CFastaOstream::WriteTitle(const CBioseq_Handle& handle)
{
    m_Out << '>' << CSeq_id::GetStringDescr(*handle.GetBioseqCore(),
                                            CSeq_id::eFormat_FastA)
          << ' ' << sequence::GetTitle(handle) << NcbiEndl;
}


// XXX - replace with SRelLoc?
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
        ITERATE (CSeg_ext::Tdata, it, ext.GetSeg().Get()) {
            TSeqPos length = sequence::GetLength(**it, &scope);
            if (s_IsGap(**it, scope)) {
                gaps.push_back(SGap(pos, length));
            }
            pos += length;
        }
        break;

    case CSeq_ext::e_Delta:
        ITERATE (CDelta_ext::Tdata, it, ext.GetDelta().Get()) {
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
         pos += loc_it.GetRange().GetLength(), ++loc_it) {
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


void CFastaOstream::WriteSequence(const CBioseq_Handle& handle,
                                  const CSeq_loc* location)
{
    CConstRef<CBioseq> seq  = handle.GetCompleteBioseq();
    const CSeq_inst& inst = seq->GetInst();
    if ( !(m_Flags & eAssembleParts)  &&  !inst.IsSetSeq_data() ) {
        return;
    }

    CSeqVector v = (location
                    ? handle.GetSequenceView(*location,
                                             CBioseq_Handle::eViewMerged,
                                             CBioseq_Handle::eCoding_Iupac)
                    : handle.GetSeqVector(CBioseq_Handle::eCoding_Iupac));
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
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CScope         scope(*om);

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



/////////////////////////////////////////////////////////////////////////////
//
// sequence translation
//


template <class Container>
void x_Translate(const Container& seq,
                 string& prot,
                 const CGenetic_code* code)
{
    // reserve our space
    const size_t mod = seq.size() % 3;
    prot.erase();
    prot.reserve(seq.size() / 3 + (mod ? 1 : 0));

    // get appropriate translation table
    const CTrans_table & tbl =
        (code ? CGen_code_table::GetTransTable(*code) :
                CGen_code_table::GetTransTable(1));

    // main loop through bases
    typename Container::const_iterator start = seq.begin();

    size_t i;
    size_t k;
    size_t state = 0;
    size_t length = seq.size() / 3;
    for (i = 0;  i < length;  ++i) {

        // loop through one codon at a time
        for (k = 0;  k < 3;  ++k, ++start) {
            state = tbl.NextCodonState(state, *start);
        }

        // save translated amino acid
        prot.append(1, tbl.GetCodonResidue(state));
    }

    if (mod) {
        LOG_POST(Warning <<
                 "translation of sequence whose length "
                 "is not an even number of codons");
        for (k = 0;  k < mod;  ++k, ++start) {
            state = tbl.NextCodonState(state, *start);
        }

        for ( ;  k < 3;  ++k) {
            state = tbl.NextCodonState(state, 'N');
        }

        // save translated amino acid
        prot.append(1, tbl.GetCodonResidue(state));
    }
}


void CSeqTranslator::Translate(const string& seq, string& prot,
                               const CGenetic_code* code,
                               bool include_stop,
                               bool remove_trailing_X)
{
    x_Translate(seq, prot, code);
}


void CSeqTranslator::Translate(const CSeqVector& seq, string& prot,
                               const CGenetic_code* code,
                               bool include_stop,
                               bool remove_trailing_X)
{
    x_Translate(seq, prot, code);
}


void CSeqTranslator::Translate(const CSeq_loc& loc,
                               const CBioseq_Handle& handle,
                               string& prot,
                               const CGenetic_code* code,
                               bool include_stop,
                               bool remove_trailing_X)
{
    CSeqVector seq =
        handle.GetSequenceView(loc, CBioseq_Handle::eViewConstructed,
                               CBioseq_Handle::eCoding_Iupac);
    x_Translate(seq, prot, code);
}




void CCdregion_translate::ReadSequenceByLocation (string& seq,
                                                  const CBioseq_Handle& bsh,
                                                  const CSeq_loc& loc)

{
    // get vector of sequence under location
    CSeqVector seqv = bsh.GetSequenceView (loc,
                                           CBioseq_Handle::eViewConstructed,
                                           CBioseq_Handle::eCoding_Iupac);
    seqv.GetSeqData(0, seqv.size(), seq);
}

void CCdregion_translate::TranslateCdregion (string& prot,
                                             const CBioseq_Handle& bsh,
                                             const CSeq_loc& loc,
                                             const CCdregion& cdr,
                                             bool include_stop,
                                             bool remove_trailing_X,
                                             bool* alt_start)
{
    // clear contents of result string
    prot.erase();
    if ( alt_start != 0 ) {
        *alt_start = false;
    }

    // copy bases from coding region location
    string bases = "";
    ReadSequenceByLocation (bases, bsh, loc);

    // calculate offset from frame parameter
    int offset = 0;
    if (cdr.IsSetFrame ()) {
        switch (cdr.GetFrame ()) {
            case CCdregion::eFrame_two :
                offset = 1;
                break;
            case CCdregion::eFrame_three :
                offset = 2;
                break;
            default :
                break;
        }
    }

    int dnalen = bases.size () - offset;
    if (dnalen < 1) return;

    // pad bases string if last codon is incomplete
    bool incomplete_last_codon = false;
    int mod = dnalen % 3;
    if ( mod != 0 ) {
        incomplete_last_codon = true;
        bases += (mod == 1) ? "NN" : "N";
        dnalen += 3 - mod;
    }
    _ASSERT((dnalen >= 3)  &&  ((dnalen % 3) == 0));

    // resize output protein translation string
    prot.resize(dnalen / 3);

    // get appropriate translation table
    const CTrans_table& tbl = cdr.IsSetCode() ?
        CGen_code_table::GetTransTable(cdr.GetCode()) :
        CGen_code_table::GetTransTable(1);

    // main loop through bases
    string::const_iterator it = bases.begin();
    string::const_iterator end = bases.end();
    for ( int i = 0; i < offset; ++i ) {
        ++it;
    }
    unsigned int state = 0, j = 0;
    while ( it != end ) {
        // get one codon at a time
        state = tbl.NextCodonState(state, *it++);
        state = tbl.NextCodonState(state, *it++);
        state = tbl.NextCodonState(state, *it++);

        // save translated amino acid
        prot[j++] = tbl.GetCodonResidue(state);
    }

    // check for alternative start codon
    if ( alt_start != 0 ) {
        state = 0;
        state = tbl.NextCodonState (state, bases[0]);
        state = tbl.NextCodonState (state, bases[1]);
        state = tbl.NextCodonState (state, bases[2]);
        if ( tbl.IsAltStart(state) ) {
            *alt_start = true;
        }
    }

    // if complete at 5' end, require valid start codon
    if (offset == 0  &&  (! loc.IsPartialLeft ())) {
        state = tbl.SetCodonState (bases [offset], bases [offset + 1], bases [offset + 2]);
        prot [0] = tbl.GetStartResidue (state);
    }

    // code break substitution
    if (cdr.IsSetCode_break ()) {
        SIZE_TYPE protlen = prot.size ();
        ITERATE (CCdregion::TCode_break, code_break, cdr.GetCode_break ()) {
            const CRef <CCode_break> brk = *code_break;
            const CSeq_loc& cbk_loc = brk->GetLoc ();
            TSeqPos seq_pos = sequence::LocationOffset (loc, cbk_loc, sequence::eOffset_FromStart, &bsh.GetScope ());
            seq_pos -= offset;
            SIZE_TYPE i = seq_pos / 3;
            if (i >= 0 && i < protlen) {
              const CCode_break::C_Aa& c_aa = brk->GetAa ();
              if (c_aa.IsNcbieaa ()) {
                prot [i] = c_aa.GetNcbieaa ();
              }
            }
        }
    }

    // optionally truncate at first terminator
    if (! include_stop) {
        SIZE_TYPE protlen = prot.size ();
        for (SIZE_TYPE i = 0; i < protlen; i++) {
            if (prot [i] == '*') {
                prot.resize (i);
                return;
            }
        }
    }

    // if padding was needed, trim ambiguous last residue
    if (incomplete_last_codon) {
        int protlen = prot.size ();
        if (protlen > 0 && prot [protlen - 1] == 'X') {
            protlen--;
            prot.resize (protlen);
        }
    }

    // optionally remove trailing X on 3' partial coding region
    if (remove_trailing_X) {
        int protlen = prot.size ();
        while (protlen > 0 && prot [protlen - 1] == 'X') {
            protlen--;
        }
        prot.resize (protlen);
    }
}


void CCdregion_translate::TranslateCdregion
(string& prot,
 const CSeq_feat& cds,
 CScope& scope,
 bool include_stop,
 bool remove_trailing_X,
 bool* alt_start)
{
    _ASSERT(cds.GetData().IsCdregion());

    prot.erase();

    CBioseq_Handle bsh = scope.GetBioseqHandle(cds.GetLocation());
    if ( !bsh ) {
        return;
    }

    CCdregion_translate::TranslateCdregion(
        prot,
        bsh,
        cds.GetLocation(),
        cds.GetData().GetCdregion(),
        include_stop,
        remove_trailing_X,
        alt_start);
}


SRelLoc::SRelLoc(const CSeq_loc& parent, const CSeq_loc& child, CScope* scope,
                 SRelLoc::TFlags flags)
    : m_ParentLoc(&parent)
{
    typedef CSeq_loc::TRange TRange0;
    for (CSeq_loc_CI cit(child);  cit;  ++cit) {
        const CSeq_id& cseqid  = cit.GetSeq_id();
        TRange0        crange  = cit.GetRange();
        if (crange.IsWholeTo()  &&  scope) {
            // determine actual end
            crange.SetToOpen(sequence::GetLength(cit.GetSeq_id(), scope));
        }
        ENa_strand     cstrand = cit.GetStrand();
        TSeqPos        pos     = 0;
        for (CSeq_loc_CI pit(parent);  pit;  ++pit) {
            ENa_strand pstrand = pit.GetStrand();
            TRange0    prange  = pit.GetRange();
            if (prange.IsWholeTo()  &&  scope) {
                // determine actual end
                prange.SetToOpen(sequence::GetLength(pit.GetSeq_id(), scope));
            }
            if ( !sequence::IsSameBioseq(cseqid, pit.GetSeq_id(), scope) ) {
                pos += prange.GetLength();
                continue;
            }
            CRef<TRange>         intersection(new TRange);
            TSeqPos              abs_from, abs_to;
            CConstRef<CInt_fuzz> fuzz_from, fuzz_to;
            if (crange.GetFrom() >= prange.GetFrom()) {
                abs_from  = crange.GetFrom();
                fuzz_from = cit.GetFuzzFrom();
                if (abs_from == prange.GetFrom()) {
                    // subtract out parent fuzz, if any
                    const CInt_fuzz* pfuzz = pit.GetFuzzFrom();
                    if (pfuzz) {
                        if (fuzz_from) {
                            CRef<CInt_fuzz> f(new CInt_fuzz);
                            f->Assign(*fuzz_from);
                            f->Subtract(*pfuzz, abs_from, abs_from);
                            if (f->IsP_m()  &&  !f->GetP_m() ) {
                                fuzz_from.Reset(); // cancelled
                            } else {
                                fuzz_from = f;
                            }
                        } else {
                            fuzz_from = pfuzz->Negative(abs_from);
                        }
                    }
                }
            } else {
                abs_from  = prange.GetFrom();
                // fuzz_from = pit.GetFuzzFrom();
                CRef<CInt_fuzz> f(new CInt_fuzz);
                f->SetLim(CInt_fuzz::eLim_lt);
                fuzz_from = f;
            }
            if (crange.GetTo() <= prange.GetTo()) {
                abs_to  = crange.GetTo();
                fuzz_to = cit.GetFuzzTo();
                if (abs_to == prange.GetTo()) {
                    // subtract out parent fuzz, if any
                    const CInt_fuzz* pfuzz = pit.GetFuzzTo();
                    if (pfuzz) {
                        if (fuzz_to) {
                            CRef<CInt_fuzz> f(new CInt_fuzz);
                            f->Assign(*fuzz_to);
                            f->Subtract(*pfuzz, abs_to, abs_to);
                            if (f->IsP_m()  &&  !f->GetP_m() ) {
                                fuzz_to.Reset(); // cancelled
                            } else {
                                fuzz_to = f;
                            }
                        } else {
                            fuzz_to = pfuzz->Negative(abs_to);
                        }
                    }
                }
            } else {
                abs_to  = prange.GetTo();
                // fuzz_to = pit.GetFuzzTo();
                CRef<CInt_fuzz> f(new CInt_fuzz);
                f->SetLim(CInt_fuzz::eLim_gt);
                fuzz_to = f;
            }
            if (abs_from <= abs_to) {
                if (IsReverse(pstrand)) {
                    TSeqPos sigma = pos + prange.GetTo();
                    intersection->SetFrom(sigma - abs_to);
                    intersection->SetTo  (sigma - abs_from);
                    if (fuzz_from) {
                        intersection->SetFuzz_to().AssignTranslated
                            (*fuzz_from, intersection->GetTo(), abs_from);
                        intersection->SetFuzz_to().Negate
                            (intersection->GetTo());
                    }
                    if (fuzz_to) {
                        intersection->SetFuzz_from().AssignTranslated
                            (*fuzz_to, intersection->GetFrom(), abs_to);
                        intersection->SetFuzz_from().Negate
                            (intersection->GetFrom());
                    }
                    if (cstrand == eNa_strand_unknown) {
                        intersection->SetStrand(pstrand);
                    } else {
                        intersection->SetStrand(Reverse(cstrand));
                    }
                } else {
                    TSignedSeqPos delta = pos - prange.GetFrom();
                    intersection->SetFrom(abs_from + delta);
                    intersection->SetTo  (abs_to   + delta);
                    if (fuzz_from) {
                        intersection->SetFuzz_from().AssignTranslated
                            (*fuzz_from, intersection->GetFrom(), abs_from);
                    }
                    if (fuzz_to) {
                        intersection->SetFuzz_to().AssignTranslated
                            (*fuzz_to, intersection->GetTo(), abs_to);
                    }
                    if (cstrand == eNa_strand_unknown) {
                        intersection->SetStrand(pstrand);
                    } else {
                        intersection->SetStrand(cstrand);
                    }
                }
                // add to m_Ranges, combining with the previous
                // interval if possible
                if ( !(flags & fNoMerge)  &&  !m_Ranges.empty()
                    &&  SameOrientation(intersection->GetStrand(),
                                        m_Ranges.back()->GetStrand()) ) {
                    if (m_Ranges.back()->GetTo() == intersection->GetFrom() - 1
                        &&  !IsReverse(intersection->GetStrand()) ) {
                        m_Ranges.back()->SetTo(intersection->GetTo());
                        if (intersection->IsSetFuzz_to()) {
                            m_Ranges.back()->SetFuzz_to
                                (intersection->SetFuzz_to());
                        } else {
                            m_Ranges.back()->ResetFuzz_to();
                        }
                    } else if (m_Ranges.back()->GetFrom()
                               == intersection->GetTo() + 1
                               &&  IsReverse(intersection->GetStrand())) {
                        m_Ranges.back()->SetFrom(intersection->GetFrom());
                        if (intersection->IsSetFuzz_from()) {
                            m_Ranges.back()->SetFuzz_from
                                (intersection->SetFuzz_from());
                        } else {
                            m_Ranges.back()->ResetFuzz_from();
                        }
                    } else {
                        m_Ranges.push_back(intersection);
                    }
                } else {
                    m_Ranges.push_back(intersection);
                }
            }
            pos += prange.GetLength();
        }
    }
}


// Bother trying to merge?
CRef<CSeq_loc> SRelLoc::Resolve(const CSeq_loc& new_parent, CScope* scope,
                                SRelLoc::TFlags /* flags */)
    const
{
    typedef CSeq_loc::TRange TRange0;
    CRef<CSeq_loc> result(new CSeq_loc);
    CSeq_loc_mix&  mix = result->SetMix();
    ITERATE (TRanges, it, m_Ranges) {
        _ASSERT((*it)->GetFrom() <= (*it)->GetTo());
        TSeqPos pos = 0, start = (*it)->GetFrom();
        bool    keep_going = true;
        for (CSeq_loc_CI pit(new_parent);  pit;  ++pit) {
            TRange0 prange = pit.GetRange();
            if (prange.IsWholeTo()  &&  scope) {
                // determine actual end
                prange.SetToOpen(sequence::GetLength(pit.GetSeq_id(), scope));
            }
            TSeqPos length = prange.GetLength();
            if (start >= pos  &&  start < pos + length) {
                TSeqPos              from, to;
                CConstRef<CInt_fuzz> fuzz_from, fuzz_to;
                ENa_strand           strand;
                if (IsReverse(pit.GetStrand())) {
                    TSeqPos sigma = pos + prange.GetTo();
                    from = sigma - (*it)->GetTo();
                    to   = sigma - start;
                    if (from < prange.GetFrom()  ||  from > sigma) {
                        from = prange.GetFrom();
                        keep_going = true;
                    } else {
                        keep_going = false;
                    }
                    if ( !(*it)->IsSetStrand()
                        ||  (*it)->GetStrand() == eNa_strand_unknown) {
                        strand = pit.GetStrand();
                    } else {
                        strand = Reverse((*it)->GetStrand());
                    }
                    if (from == prange.GetFrom()) {
                        fuzz_from = pit.GetFuzzFrom();
                    }
                    if ( !keep_going  &&  (*it)->IsSetFuzz_to() ) {
                        CRef<CInt_fuzz> f(new CInt_fuzz);
                        if (fuzz_from) {
                            f->Assign(*fuzz_from);
                        } else {
                            f->SetP_m(0);
                        }
                        f->Subtract((*it)->GetFuzz_to(), from, (*it)->GetTo(),
                                    CInt_fuzz::eAmplify);
                        if (f->IsP_m()  &&  !f->GetP_m() ) {
                            fuzz_from.Reset(); // cancelled
                        } else {
                            fuzz_from = f;
                        }
                    }
                    if (to == prange.GetTo()) {
                        fuzz_to = pit.GetFuzzTo();
                    }
                    if (start == (*it)->GetFrom()
                        &&  (*it)->IsSetFuzz_from()) {
                        CRef<CInt_fuzz> f(new CInt_fuzz);
                        if (fuzz_to) {
                            f->Assign(*fuzz_to);
                        } else {
                            f->SetP_m(0);
                        }
                        f->Subtract((*it)->GetFuzz_from(), to,
                                    (*it)->GetFrom(), CInt_fuzz::eAmplify);
                        if (f->IsP_m()  &&  !f->GetP_m() ) {
                            fuzz_to.Reset(); // cancelled
                        } else {
                            fuzz_to = f;
                        }
                    }
                } else {
                    TSignedSeqPos delta = prange.GetFrom() - pos;
                    from = start          + delta;
                    to   = (*it)->GetTo() + delta;
                    if (to > prange.GetTo()) {
                        to = prange.GetTo();
                        keep_going = true;
                    } else {
                        keep_going = false;
                    }
                    if ( !(*it)->IsSetStrand()
                        ||  (*it)->GetStrand() == eNa_strand_unknown) {
                        strand = pit.GetStrand();
                    } else {
                        strand = (*it)->GetStrand();
                    }
                    if (from == prange.GetFrom()) {
                        fuzz_from = pit.GetFuzzFrom();
                    }
                    if (start == (*it)->GetFrom()
                        &&  (*it)->IsSetFuzz_from()) {
                        CRef<CInt_fuzz> f(new CInt_fuzz);
                        if (fuzz_from) {
                            f->Assign(*fuzz_from);
                            f->Add((*it)->GetFuzz_from(), from,
                                   (*it)->GetFrom());
                        } else {
                            f->AssignTranslated((*it)->GetFuzz_from(), from,
                                                (*it)->GetFrom());
                        }
                        if (f->IsP_m()  &&  !f->GetP_m() ) {
                            fuzz_from.Reset(); // cancelled
                        } else {
                            fuzz_from = f;
                        }
                    }
                    if (to == prange.GetTo()) {
                        fuzz_to = pit.GetFuzzTo();
                    }
                    if ( !keep_going  &&  (*it)->IsSetFuzz_to() ) {
                        CRef<CInt_fuzz> f(new CInt_fuzz);
                        if (fuzz_to) {
                            f->Assign(*fuzz_to);
                            f->Add((*it)->GetFuzz_to(), to, (*it)->GetTo());
                        } else {
                            f->AssignTranslated((*it)->GetFuzz_to(), to,
                                                (*it)->GetTo());
                        }
                        if (f->IsP_m()  &&  !f->GetP_m() ) {
                            fuzz_to.Reset(); // cancelled
                        } else {
                            fuzz_to = f;
                        }
                    }
                }
                if (from == to
                    &&  (fuzz_from == fuzz_to
                         ||  (fuzz_from.GetPointer()  &&  fuzz_to.GetPointer()
                              &&  fuzz_from->Equals(*fuzz_to)))) {
                    // just a point
                    CRef<CSeq_loc> loc(new CSeq_loc);
                    CSeq_point& point = loc->SetPnt();
                    point.SetPoint(from);
                    if (strand != eNa_strand_unknown) {
                        point.SetStrand(strand);
                    }
                    if (fuzz_from) {
                        point.SetFuzz().Assign(*fuzz_from);
                    }
                    point.SetId().Assign(pit.GetSeq_id());
                    mix.Set().push_back(loc);
                } else {
                    CRef<CSeq_loc> loc(new CSeq_loc);
                    CSeq_interval& ival = loc->SetInt();
                    ival.SetFrom(from);
                    ival.SetTo(to);
                    if (strand != eNa_strand_unknown) {
                        ival.SetStrand(strand);
                    }
                    if (fuzz_from) {
                        ival.SetFuzz_from().Assign(*fuzz_from);
                    }
                    if (fuzz_to) {
                        ival.SetFuzz_to().Assign(*fuzz_to);
                    }
                    ival.SetId().Assign(pit.GetSeq_id());
                    mix.Set().push_back(loc);
                }
                if (keep_going) {
                    start = pos + length;
                } else {
                    break;
                }
            }
            pos += length;
        }
        if (keep_going) {
            TSeqPos total_length;
            string  label;
            new_parent.GetLabel(&label);
            try {
                total_length = sequence::GetLength(new_parent, scope);
                ERR_POST(Warning << "SRelLoc::Resolve: Relative position "
                         << start << " exceeds length (" << total_length
                         << ") of parent location " << label);
            } catch (sequence::CNoLength) {
                ERR_POST(Warning << "SRelLoc::Resolve: Relative position "
                         << start
                         << " exceeds length (?\?\?) of parent location "
                         << label);
            }            
        }
    }
    // clean up output
    switch (mix.Get().size()) {
    case 0:
        result->SetNull();
        break;
    case 1:
        result.Reset(mix.Set().front());
        break;
    default:
        break;
    }
    return result;
}


//============================================================================//
//                                 SeqSearch                                  //
//============================================================================//

// Public:
// =======

// Constructors and Destructors:
CSeqSearch::CSeqSearch(IClient *client, bool allow_mismatch) :
    m_AllowOneMismatch(allow_mismatch),
    m_MaxPatLen(0),
    m_Client(client)
{
    InitializeMaps();
}


CSeqSearch::~CSeqSearch(void) {}


// Add nucleotide pattern or restriction site to sequence search.
// Uses ambiguity codes, e.g., R = A and G, H = A, C and T
void CSeqSearch::AddNucleotidePattern
(const string& name,
const string& pat, 
int cut_site,
int overhang)
{
    string pattern = pat;
    NStr::TruncateSpaces(pattern);
    NStr::ToUpper(pattern);

    // reverse complement pattern to see if it is symetrical
    string rcomp = ReverseComplement(pattern);

    bool symmetric = (pattern == rcomp);

    ENa_strand strand = symmetric ? eNa_strand_both : eNa_strand_plus;

    AddNucleotidePattern(name, pat, cut_site, overhang, strand);
    if ( !symmetric ) {
        AddNucleotidePattern(name, rcomp, pat.length() - cut_site, 
                             overhang, eNa_strand_minus);
    }
}


// Program passes each character in turn to finite state machine.
int CSeqSearch::Search
(int current_state,
 char ch,
 int position,
 int length)
{
    if ( !m_Client ) return 0;

    // on first character, populate state transition table
    if ( !m_Fsa.IsPrimed() ) {
        m_Fsa.Prime();
    }
    
    int next_state = m_Fsa.GetNextState(current_state, ch);
    
    // report any matches at current state to the client object
    if ( m_Fsa.IsMatchFound(next_state) ) {
        ITERATE( vector<CMatchInfo>, it, m_Fsa.GetMatches(next_state) ) {
            //const CMatchInfo& match = *it;
            int start = position - it->GetPattern().length() + 1;    

            // prevent multiple reports of patterns for circular sequences.
            if ( start < length ) {
                m_Client->MatchFound(*it, start);
            }
        }
    }

    return next_state;
}


// Search an entire bioseq.
void CSeqSearch::Search(const CBioseq_Handle& bsh)
{
    if ( !bsh ) return;
    if ( !m_Client ) return;  // no one to report to, so why search at all.

    CSeqVector seq_vec = bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
    size_t seq_len = seq_vec.size();
    size_t search_len = seq_len;

    CSeq_inst::ETopology topology =
        bsh.GetBioseqCore()->GetInst().GetTopology();    
    if ( topology == CSeq_inst::eTopology_circular ) {
        search_len += m_MaxPatLen - 1;
    }
    
    int state = m_Fsa.GetInitialState();

    for ( size_t i = 0; i < search_len; ++i ) {
        state = Search(state, seq_vec[i % seq_len], i, seq_len);
    }
}


// Private:
// ========

// translation finite state machine base codes - ncbi4na
enum EBaseCode {
    eBase_gap = 0,
        eBase_A,      /* A    */
        eBase_C,      /* C    */
        eBase_M,      /* AC   */
        eBase_G,      /* G    */
        eBase_R,      /* AG   */
        eBase_S,      /* CG   */
        eBase_V,      /* ACG  */
        eBase_T,      /* T    */
        eBase_W,      /* AT   */
        eBase_Y,      /* CT   */
        eBase_H,      /* ACT  */
        eBase_K,      /* GT   */
        eBase_D,      /* AGT  */
        eBase_B,      /* CGT  */
        eBase_N       /* ACGT */
};


map<unsigned char, int> CSeqSearch::sm_CharToEnum;
map<int, unsigned char> CSeqSearch::sm_EnumToChar;
map<char, char>         CSeqSearch::sm_Complement;


void CSeqSearch::InitializeMaps(void)
{
    int c, i;
    static const string iupacna_alphabet      = "-ACMGRSVTWYHKDBN";
    static const string comp_iupacna_alphabet = "-TGKCYSBAWRDMHVN";

    if ( sm_CharToEnum.empty() ) {
        // illegal characters map to eBase_gap (0)
        for ( c = 0; c < 256; ++c ) {
            sm_CharToEnum[c] = eBase_gap;
        }
        
        // map iupacna alphabet to EBaseCode
        for (i = eBase_gap; i <= eBase_N; ++i) {
            c = iupacna_alphabet[i];
            sm_CharToEnum[c] = (EBaseCode)i;
            c = tolower(c);
            sm_CharToEnum[c] = (EBaseCode)i;
        }
        sm_CharToEnum ['U'] = eBase_T;
        sm_CharToEnum ['u'] = eBase_T;
        sm_CharToEnum ['X'] = eBase_N;
        sm_CharToEnum ['x'] = eBase_N;
        
        // also map ncbi4na alphabet to EBaseCode
        for (c = eBase_gap; c <= eBase_N; ++c) {
            sm_CharToEnum[c] = (EBaseCode)c;
        }
    }
    
    // map EBaseCode to iupacna alphabet
    if ( sm_EnumToChar.empty() ) { 
        for (i = eBase_gap; i <= eBase_N; ++i) {
            sm_EnumToChar[i] = iupacna_alphabet[i];
        }
    }

    // initialize table to convert character to complement character
    if ( sm_Complement.empty() ) {
        int len = iupacna_alphabet.length();
        for ( i = 0; i < len; ++i ) {
            sm_Complement.insert(make_pair(iupacna_alphabet[i], comp_iupacna_alphabet[i]));
        }
    }
}


string CSeqSearch::ReverseComplement(const string& pattern) const
{
    size_t len = pattern.length();
    string rcomp = pattern;
    rcomp.resize(len);

    // calculate the complement
    for ( size_t i = 0; i < len; ++i ) {
        rcomp[i] = sm_Complement[pattern[i]];
    }

    // reverse the complement
    reverse(rcomp.begin(), rcomp.end());

    return rcomp;
}


void CSeqSearch::AddNucleotidePattern
(const string& name,
const string& pattern, 
int cut_site,
int overhang,
ENa_strand strand)
{
    size_t pat_len = pattern.length();
    string temp;
    temp.resize(pat_len);
    CMatchInfo info(name,
                    CNcbiEmptyString::Get(), 
                    cut_site, 
                    overhang, 
                    strand);

    if ( pat_len > m_MaxPatLen ) {
        m_MaxPatLen = pat_len;
    }

    ExpandPattern(pattern, temp, 0, pat_len, info);
}


void CSeqSearch::ExpandPattern
(const string& pattern,
string& temp,
int position,
int pat_len,
CMatchInfo& info)
{
    static EBaseCode expension[] = { eBase_A, eBase_C, eBase_G, eBase_T };

    if ( position < pat_len ) {
        int code = sm_CharToEnum[(int)pattern[position]];
        
        for ( int i = 0; i < 4; ++i ) {
            if ( code & expension[i] ) {
                temp[position] = sm_EnumToChar[expension[i]];
                ExpandPattern(pattern, temp, position + 1, pat_len, info);
            }
        }
    } else { // recursion base
        // when position reaches pattern length, store one expanded string.
        info.m_Pattern = temp;
        m_Fsa.AddWord(info.m_Pattern, info);

        if ( m_AllowOneMismatch ) {
            char ch;
            // put 'N' at every position if a single mismatch is allowed.
            for ( int i = 0; i < pat_len; ++i ) {
                ch = temp[i];
                temp[i] = 'N';

                info.m_Pattern = temp;
                m_Fsa.AddWord(pattern, info);

                // restore proper character, go on to put N in next position.
                temp[i] = ch;
            }
        }
    }
}




END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.87  2004/08/26 18:26:27  grichenk
* Added check for circular sequence in x_GetBestOverlappingFeat()
*
* Revision 1.86  2004/08/24 16:42:03  vasilche
* Removed TAB symbols in sources.
*
* Revision 1.85  2004/08/19 15:26:56  shomrat
* Use Seq_loc_Mapper in SeqLocMerge
*
* Revision 1.84  2004/07/21 15:51:25  grichenk
* CObjectManager made singleton, GetInstance() added.
* CXXXXDataLoader constructors made private, added
* static RegisterInObjectManager() and GetLoaderNameFromArgs()
* methods.
*
* Revision 1.83  2004/07/12 16:54:43  vasilche
* Fixed compilation warnings.
*
* Revision 1.82  2004/05/27 13:48:24  jcherry
* Replaced some calls to deprecated CBioseq_Handle::GetBioseq() with
* GetCompleteBioseq() or GetBioseqCore()
*
* Revision 1.81  2004/05/25 21:06:34  johnson
* Bug fix in x_Translate
*
* Revision 1.80  2004/05/21 21:42:14  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.79  2004/05/06 17:39:43  shomrat
* Fixes to SeqLocMerge
*
* Revision 1.78  2004/04/06 14:03:15  dicuccio
* Added API to extract the single Org-ref from a bioseq handle.  Added API to
* retrieve a single tax-id from a bioseq handle
*
* Revision 1.77  2004/04/05 15:56:14  grichenk
* Redesigned CAnnotTypes_CI: moved all data and data collecting
* functions to CAnnotDataCollector. CAnnotTypes_CI is no more
* inherited from SAnnotSelector.
*
* Revision 1.76  2004/03/25 20:02:30  vasilche
* Added several method variants with CBioseq_Handle as argument.
*
* Revision 1.75  2004/03/01 18:28:18  dicuccio
* Changed sequence::Compare() such that a seq-interval of length 1 and a
* corresponding seq-point compare as the same
*
* Revision 1.74  2004/03/01 18:24:22  shomrat
* Removed branching in the main cdregion translation loop; Added alternative start flag
*
* Revision 1.73  2004/02/19 18:16:48  shomrat
* Implemented SeqlocRevCmp
*
* Revision 1.72  2004/02/09 14:43:18  vasilche
* Added missing typename keyword.
*
* Revision 1.71  2004/02/05 19:41:46  shomrat
* Convenience functions for popular overlapping types
*
* Revision 1.70  2004/02/04 18:05:41  grichenk
* Added annotation filtering by set of types/subtypes.
* Renamed *Choice to *Type in SAnnotSelector.
*
* Revision 1.69  2004/01/30 17:49:29  ucko
* Add missing "typename"
*
* Revision 1.68  2004/01/30 17:22:53  dicuccio
* Added sequence::GetId(const CBioseq_Handle&) - returns a selected ID class
* (best, GI).  Added CSeqTranslator - utility class to translate raw sequence
* data
*
* Revision 1.67  2004/01/28 17:19:04  shomrat
* Implemented SeqLocMerge
*
* Revision 1.66  2003/12/16 19:37:43  shomrat
* Retrieve encoding feature and bioseq of a protein
*
* Revision 1.65  2003/11/19 22:18:05  grichenk
* All exceptions are now CException-derived. Catch "exception" rather
* than "runtime_error".
*
* Revision 1.64  2003/10/17 20:55:27  ucko
* SRelLoc::Resolve: fix a fuzz-handling paste-o.
*
* Revision 1.63  2003/10/16 11:55:19  dicuccio
* Fix for brain-dead MSVC and ambiguous operator&&
*
* Revision 1.62  2003/10/15 19:52:18  ucko
* More adjustments to SRelLoc: support fuzz, opposite-strand children,
* and resolving against an alternate parent.
*
* Revision 1.61  2003/10/08 21:08:38  ucko
* CCdregion_translate: take const Bioseq_Handles, since there's no need
* to modify them.
* TestForOverlap: don't consider sequences circular if
* circular_len == kInvalidSeqPos
*
* Revision 1.60  2003/09/22 18:38:14  grichenk
* Fixed circular seq-locs processing by TestForOverlap()
*
* Revision 1.59  2003/07/17 21:00:50  vasilche
* Added missing include <list>
*
* Revision 1.58  2003/06/19 17:11:43  ucko
* SRelLoc::SRelLoc: remember to update the base position when skipping
* parent ranges whose IDs don't match.
*
* Revision 1.57  2003/06/13 17:23:32  grichenk
* Added special processing of multi-ID seq-locs in TestForOverlap()
*
* Revision 1.56  2003/06/05 18:08:36  shomrat
* Allow empty location when computing SeqLocPartial; Adjust GetSeqData in cdregion translation
*
* Revision 1.55  2003/06/02 18:58:25  dicuccio
* Fixed #include directives
*
* Revision 1.54  2003/06/02 18:53:32  dicuccio
* Fixed bungled commit
*
* Revision 1.52  2003/05/27 19:44:10  grichenk
* Added CSeqVector_CI class
*
* Revision 1.51  2003/05/15 19:27:02  shomrat
* Compare handle only if both valid; Check IsLim before GetLim
*
* Revision 1.50  2003/05/09 15:37:00  ucko
* Take const CBioseq_Handle references in CFastaOstream::Write et al.
*
* Revision 1.49  2003/05/06 19:34:36  grichenk
* Fixed GetStrand() (worked fine only for plus and unknown strands)
*
* Revision 1.48  2003/05/01 17:55:17  ucko
* Fix GetLength(const CSeq_id&, CScope*) to return ...::max() rather
* than throwing if it can't resolve the ID to a handle.
*
* Revision 1.47  2003/04/24 16:15:58  vasilche
* Added missing includes and forward class declarations.
*
* Revision 1.46  2003/04/16 19:44:26  grichenk
* More fixes to TestForOverlap() and GetStrand()
*
* Revision 1.45  2003/04/15 20:11:21  grichenk
* Fixed strands problem in TestForOverlap(), replaced type
* iterator with container iterator in s_GetStrand().
*
* Revision 1.44  2003/04/03 19:03:17  grichenk
* Two more cases to revert locations in GetBestOverlappingFeat()
*
* Revision 1.43  2003/04/02 16:58:59  grichenk
* Change location and feature in GetBestOverlappingFeat()
* for eOverlap_Subset.
*
* Revision 1.42  2003/03/25 22:00:20  grichenk
* Added strand checking to TestForOverlap()
*
* Revision 1.41  2003/03/18 21:48:35  grichenk
* Removed obsolete class CAnnot_CI
*
* Revision 1.40  2003/03/11 16:00:58  kuznets
* iterate -> ITERATE
*
* Revision 1.39  2003/02/19 16:25:14  grichenk
* Check strands in GetBestOverlappingFeat()
*
* Revision 1.38  2003/02/14 15:41:00  shomrat
* Minor implementation changes in SeqLocPartialTest
*
* Revision 1.37  2003/02/13 14:35:40  grichenk
* + eOverlap_Contains
*
* Revision 1.36  2003/02/10 15:54:01  grichenk
* Use CFeat_CI->GetMappedFeature() and GetOriginalFeature()
*
* Revision 1.35  2003/02/06 22:26:27  vasilche
* Use CSeq_id::Assign().
*
* Revision 1.34  2003/02/06 20:59:16  shomrat
* Bug fix in SeqLocPartialCheck
*
* Revision 1.33  2003/01/22 21:02:23  ucko
* Fix stupid logic error in SRelLoc's constructor; change LocationOffset
* to return -1 rather than crashing if the locations don't intersect.
*
* Revision 1.32  2003/01/22 20:15:02  vasilche
* Removed compiler warning.
*
* Revision 1.31  2003/01/22 18:17:09  ucko
* SRelLoc::SRelLoc: change intersection to a CRef, so we don't have to
* worry about it going out of scope while still referenced (by m_Ranges).
*
* Revision 1.30  2003/01/08 20:43:10  ucko
* Adjust SRelLoc to use (ID-less) Seq-intervals for ranges, so that it
* will be possible to add support for fuzz and strandedness/orientation.
*
* Revision 1.29  2002/12/30 19:38:35  vasilche
* Optimized CGenbankWriter::WriteSequence.
* Implemented GetBestOverlappingFeat() with CSeqFeatData::ESubtype selector.
*
* Revision 1.28  2002/12/26 21:45:29  grichenk
* + GetBestOverlappingFeat()
*
* Revision 1.27  2002/12/26 21:17:06  dicuccio
* Minor tweaks to avoid compiler warnings in MSVC (remove unused variables)
*
* Revision 1.26  2002/12/20 17:14:18  kans
* added SeqLocPartialCheck
*
* Revision 1.25  2002/12/19 20:24:55  grichenk
* Updated usage of CRange<>
*
* Revision 1.24  2002/12/10 16:56:41  ucko
* CFastaOstream::WriteTitle: restore leading > accidentally dropped in R1.19.
*
* Revision 1.23  2002/12/10 16:34:45  kans
* implement code break processing in TranslateCdregion
*
* Revision 1.22  2002/12/09 20:38:41  ucko
* +sequence::LocationOffset
*
* Revision 1.21  2002/12/06 15:36:05  grichenk
* Added overlap type for annot-iterators
*
* Revision 1.20  2002/12/04 15:38:22  ucko
* SourceToProduct, ProductToSource: just check whether the feature is a coding
* region rather than attempting to determine molecule types; drop s_DeduceMol.
*
* Revision 1.19  2002/11/26 15:14:04  dicuccio
* Changed CFastaOStream::WriteTitle() to make use of CSeq_id::GetStringDescr().
*
* Revision 1.18  2002/11/25 21:24:46  grichenk
* Added TestForOverlap() function.
*
* Revision 1.17  2002/11/18 19:59:23  shomrat
* Add CSeqSearch - a nucleotide search utility
*
* Revision 1.16  2002/11/12 20:00:25  ucko
* +SourceToProduct, ProductToSource, SRelLoc
*
* Revision 1.15  2002/10/23 19:22:39  ucko
* Move the FASTA reader from objects/util/sequence.?pp to
* objects/seqset/Seq_entry.?pp because it doesn't need the OM.
*
* Revision 1.14  2002/10/23 18:23:59  ucko
* Clean up #includes.
* Add a FASTA reader (known to compile, but not otherwise tested -- take care)
*
* Revision 1.13  2002/10/23 16:41:12  clausen
* Fixed warning in WorkShop53
*
* Revision 1.12  2002/10/23 15:33:50  clausen
* Fixed local variable scope warnings
*
* Revision 1.11  2002/10/08 12:35:37  clausen
* Fixed bugs in GetStrand(), ChangeSeqId() & SeqLocCheck()
*
* Revision 1.10  2002/10/07 17:11:16  ucko
* Fix usage of ERR_POST (caught by KCC)
*
* Revision 1.9  2002/10/03 18:44:09  clausen
* Removed extra whitespace
*
* Revision 1.8  2002/10/03 16:33:55  clausen
* Added functions needed by validate
*
* Revision 1.7  2002/09/13 15:30:43  ucko
* Change resize(0) to erase() at Denis's suggestion.
*
* Revision 1.6  2002/09/13 14:28:34  ucko
* string::clear() doesn't exist under g++ 2.95, so use resize(0) instead. :-/
*
* Revision 1.5  2002/09/12 21:39:13  kans
* added CCdregion_translate and CCdregion_translate
*
* Revision 1.4  2002/09/03 21:27:04  grichenk
* Replaced bool arguments in CSeqVector constructor and getters
* with enums.
*
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
