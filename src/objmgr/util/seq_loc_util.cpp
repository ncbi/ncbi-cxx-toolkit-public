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

#include <ncbi_pch.hpp>
#include <corelib/ncbi_limits.hpp>

#include <serial/iterator.hpp>

#include <objects/seq/seq_id_mapper.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objects/seq/seq_loc_reverse_complementer.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/MolInfo.hpp>

#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Packed_seqint.hpp>
#include <objects/seqloc/Seq_loc_mix.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_bond.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>

#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objmgr/seq_loc_mapper.hpp>
#include <objmgr/impl/handle_range_map.hpp>
#include <objmgr/impl/synonyms.hpp>
#include <objmgr/error_codes.hpp>

#include <util/range_coll.hpp>

#include <algorithm>

#define NCBI_USE_ERRCODE_X   ObjMgr_SeqUtil

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(sequence)


TSeqPos GetLength(const CSeq_id& id, CScope* scope)
{
    if ( !scope ) {
        return numeric_limits<TSeqPos>::max();
    }
    CBioseq_Handle hnd = scope->GetBioseqHandle(id);
    if ( !hnd ) {
        return numeric_limits<TSeqPos>::max();
    }
    return hnd.GetBioseqLength();
}


TSeqPos GetLength(const CSeq_loc& loc, CScope* scope)
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
    case CSeq_loc::e_Bond:         // return number of points
        return loc.GetBond().IsSetB() + loc.GetBond().IsSetA();
    case CSeq_loc::e_not_set:      //can't calculate length
    case CSeq_loc::e_Feat:
    case CSeq_loc::e_Equiv:        // unless actually the same length...
    default:
        NCBI_THROW(CObjmgrUtilException, eUnknownLength,
            "Unable to determine length");
    }
}


namespace {
    struct SCoverageCollector {
        SCoverageCollector(const CSeq_loc& loc, CScope* scope)
            {
                Add(loc, scope);
            }
        void Add(const CSeq_loc& loc, CScope* scope)
            {
                switch (loc.Which()) {
                case CSeq_loc::e_Pnt:
                    Add(loc.GetPnt());
                    return;
                case CSeq_loc::e_Int:
                    Add(loc.GetInt());
                    return;
                case CSeq_loc::e_Null:
                case CSeq_loc::e_Empty:
                    return;
                case CSeq_loc::e_Whole:
                    AddWhole(loc.GetWhole(), scope);
                    return;
                case CSeq_loc::e_Mix:
                    Add(loc.GetMix(), scope);
                    return;
                case CSeq_loc::e_Packed_int:
                    Add(loc.GetPacked_int());
                    return;
                case CSeq_loc::e_Packed_pnt:
                    Add(loc.GetPacked_pnt());
                    return;
                case CSeq_loc::e_Bond:
                    Add(loc.GetBond().GetA());
                    if ( loc.GetBond().IsSetB() ) {
                        Add(loc.GetBond().GetB());
                    }
                    return;
                case CSeq_loc::e_not_set: //can't calculate coverage
                case CSeq_loc::e_Feat:
                case CSeq_loc::e_Equiv: // unless actually the same length...
                default:
                    NCBI_THROW(CObjmgrUtilException, eUnknownLength,
                               "Unable to determine coverage");
                }
            }
        void Add(const CSeq_id_Handle& idh, TSeqPos from, TSeqPos to)
            {
                m_Intervals[idh] += TRange(from, to);
            }
        void Add(const CSeq_id& id, TSeqPos from, TSeqPos to)
            {
                Add(CSeq_id_Handle::GetHandle(id), from, to);
            }
        void AddWhole(const CSeq_id& id, CScope* scope)
            {
                Add(id, 0, GetLength(id, scope)-1);
            }
        void Add(const CSeq_point& seq_pnt)
            {
                Add(seq_pnt.GetId(), seq_pnt.GetPoint(), seq_pnt.GetPoint());
            }
        void Add(const CSeq_interval& seq_int)
            {
                Add(seq_int.GetId(), seq_int.GetFrom(), seq_int.GetTo());
            }
        void Add(const CPacked_seqint& packed_int)
            {
                ITERATE ( CPacked_seqint::Tdata, it, packed_int.Get() ) {
                    Add(**it);
                }
            }
        void Add(const CPacked_seqpnt& packed_pnt)
            {
                CSeq_id_Handle idh =
                    CSeq_id_Handle::GetHandle(packed_pnt.GetId());
                ITERATE(CPacked_seqpnt::TPoints, it, packed_pnt.GetPoints()) {
                    Add(idh, *it, *it);
                }
            }
        void Add(const CSeq_loc_mix& mix, CScope* scope)
            {
                ITERATE ( CSeq_loc_mix::Tdata, it, mix.Get() ) {
                    Add(**it, scope);
                }
            }

        TSeqPos GetCoverage(void) const
            {
                TSeqPos coverage = 0;
                ITERATE ( TIntervals, it, m_Intervals ) {
                    ITERATE ( TRanges, it2, it->second ) {
                        coverage += it2->GetLength();
                    }
                }
                return coverage;
            }

    private:
        typedef CRange<TSeqPos> TRange;
        typedef CRangeCollection<TSeqPos> TRanges;
        typedef map<CSeq_id_Handle, TRanges> TIntervals;
        TIntervals m_Intervals;
    };
}


TSeqPos GetCoverage(const CSeq_loc& loc, CScope* scope)
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
    case CSeq_loc::e_Mix:
    case CSeq_loc::e_Packed_pnt:
    case CSeq_loc::e_Bond:
        return SCoverageCollector(loc, scope).GetCoverage();
    case CSeq_loc::e_not_set:      // can't calculate length
    case CSeq_loc::e_Feat:
    case CSeq_loc::e_Equiv:        // unless actually the same length...
    default:
        NCBI_THROW(CObjmgrUtilException, eUnknownLength,
            "Unable to determine length");
    }
}


TSeqPos GetLength(const CSeq_loc_mix& mix, CScope* scope)
{
    TSeqPos length = 0;

    ITERATE( CSeq_loc_mix::Tdata, i, mix.Get() ) {
        TSeqPos ret = GetLength((**i), scope);
        if (ret < numeric_limits<TSeqPos>::max()) {
            length += ret;
        }
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


bool IsSameBioseq(const CSeq_id& id1, const CSeq_id& id2, CScope* scope,
                  CScope::EGetBioseqFlag get_flag)
{
    return IsSameBioseq(CSeq_id_Handle::GetHandle(id1),
                        CSeq_id_Handle::GetHandle(id2),
                        scope, get_flag);
}


bool IsSameBioseq(const CSeq_id_Handle& id1, const CSeq_id_Handle& id2, CScope* scope,
                  CScope::EGetBioseqFlag get_flag)
{
    // Compare CSeq_ids directly
    if (id1 == id2) {
        return true;
    }

    // Compare handles
    if (scope != NULL) {
        try {
            CBioseq_Handle hnd1 = scope->GetBioseqHandle(id1, get_flag);
            CBioseq_Handle hnd2 = scope->GetBioseqHandle(id2, get_flag);
            return hnd1  &&  hnd2  &&  (hnd1 == hnd2);
        } catch (CException& e) {
            ERR_POST_X(1, e.what() << ": CSeq_id1: " << id1.GetSeqId()->DumpAsFasta()
                       << ": CSeq_id2: " << id2.GetSeqId()->DumpAsFasta());
        }
    }

    return false;
}


static const CSeq_id* s_GetId(const CSeq_loc& loc, CScope* scope,
                              string* msg = NULL)
{
    const CSeq_id* sip = NULL;
    if (msg != NULL) {
        msg->erase();
    }

    for (CSeq_loc_CI it(loc, CSeq_loc_CI::eEmpty_Allow); it; ++it) {
        const CSeq_id& id = it.GetSeq_id();
        if (id.Which() == CSeq_id::e_not_set) {
            continue;
        }
        if (sip == NULL) {
            sip = &id;
        } else {
            if (!IsSameBioseq(*sip, id, scope)) {
                if (msg != NULL) {
                    *msg = "Location contains segments on more than one bioseq.";
                }
                sip = NULL;
                break;
            }
        }
    }

    if (sip == NULL  &&  msg != NULL  &&  msg->empty()) {
        *msg = "Location contains no IDs.";
    }

    return sip;
}


const CSeq_id& GetId(const CSeq_loc& loc, CScope* scope)
{
    string msg;
    const CSeq_id* sip = s_GetId(loc, scope, &msg);

    if (sip == NULL) {
        NCBI_THROW(CObjmgrUtilException, eNotUnique, msg);
    }

    return *sip;
}


CSeq_id_Handle GetIdHandle(const CSeq_loc& loc, CScope* scope)
{
    CSeq_id_Handle retval;

    try {
        if (!loc.IsNull()) {
            retval = CSeq_id_Handle::GetHandle(GetId(loc, scope));
        }
    } catch (CObjmgrUtilException&) {}

    return retval;
}


bool IsOneBioseq(const CSeq_loc& loc, CScope* scope)
{
    return s_GetId(loc, scope) != NULL;
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
            if ((*it)->IsNull()  ||  (*it)->IsEmpty()) {
                continue;
            }
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
        return strand;
    }
    default:
        return eNa_strand_unknown;
    }
}



ENa_strand GetStrand(const CSeq_loc& loc, CScope* scope)
{
    switch (loc.Which()) {
    case CSeq_loc::e_Int:
        if (loc.GetInt().IsSetStrand()) {
            return loc.GetInt().GetStrand();
        }
        break;

    case CSeq_loc::e_Whole:
        return eNa_strand_both;

    case CSeq_loc::e_Pnt:
        if (loc.GetPnt().IsSetStrand()) {
            return loc.GetPnt().GetStrand();
        }
        break;

    case CSeq_loc::e_Packed_pnt:
        return loc.GetPacked_pnt().IsSetStrand() ?
            loc.GetPacked_pnt().GetStrand() : eNa_strand_unknown;

    default:
        if (!IsOneBioseq(loc, scope)) {
            return eNa_strand_unknown;  // multiple bioseqs
        } else {
            return s_GetStrand(loc);
        }
    }

    /// default to unknown strand
    return eNa_strand_unknown;
}


TSeqPos GetStart(const CSeq_loc& loc, CScope* scope, ESeqLocExtremes ext)
{
    // Throw CObjmgrUtilException if loc does not represent one CBioseq
    GetId(loc, scope);

    return loc.GetStart(ext);
}


TSeqPos GetStop(const CSeq_loc& loc, CScope* scope, ESeqLocExtremes ext)
{
    // Throw CObjmgrUtilException if loc does not represent one CBioseq
    GetId(loc, scope);

    if (loc.IsWhole()  &&  scope != NULL) {
        CBioseq_Handle seq = GetBioseqFromSeqLoc(loc, *scope);
        if (seq) {
            return seq.GetBioseqLength() - 1;
        }
    }
    return loc.GetStop(ext);
}


void ChangeSeqId(CSeq_id* id, bool best, CScope* scope)
{
    // Return if no scope
    if (!scope  ||  !id) {
        return;
    }

    // Get CBioseq represented by *id
    CBioseq_Handle::TBioseqCore seq =
        scope->GetBioseqHandle(*id).GetBioseqCore();

    // Get pointer to the best/worst id of *seq
    const CSeq_id* tmp_id;
    if (best) {
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
(const CBioseq_Handle& bsh,
 const CSeq_loc& loc)
{
    try {
        CSeq_loc_Mapper mapper (bsh, CSeq_loc_Mapper::eSeqMap_Up);
        CConstRef<CSeq_loc> mapped_loc = mapper.Map(loc);
        if (!mapped_loc) {
            return false;
        }
        
        // Check that loc segments are in order
        CSeq_loc::TRange last_range;
        bool first = true;
        for (CSeq_loc_CI lit(*mapped_loc); lit; ++lit) {
            if (first) {
                last_range = lit.GetRange();
                first = false;
                continue;
            }
            if (lit.GetStrand() == eNa_strand_minus) {
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
    } catch (CException) {
        // exception will be thrown if references far sequence and not remote fetching
    }
    return false;
}


bool BadSeqLocSortOrder
(const CBioseq&  seq,
 const CSeq_loc& loc,
 CScope*         scope)
{
    if (scope) {
        return BadSeqLocSortOrder (scope->GetBioseqHandle(seq), loc);
    } else {
        return false;
    }
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


void SeqIntPartialCheck(const CSeq_interval& itv,
                        unsigned int& retval,
                        bool is_first,
                        bool is_last,
                        CScope& scope)
{
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
                    if ( is_last ) {
                        retval |= eSeqlocPartial_Stop;
                    } else {
                        retval |= eSeqlocPartial_Internal;
                    }
                    if (itv.GetFrom() != 0) {
                        if ( is_last ) {
                            retval |= eSeqlocPartial_Nostop;
                        } else {
                            retval |= eSeqlocPartial_Nointernal;
                        }
                    }
                } else {
                    if ( is_first ) {
                        retval |= eSeqlocPartial_Start;
                    } else {
                        retval |= eSeqlocPartial_Internal;
                    }
                    if (itv.GetFrom() != 0) {
                        if ( is_first ) {
                            retval |= eSeqlocPartial_Nostart;
                        } else {
                            retval |= eSeqlocPartial_Nointernal;
                        }
                    }
                }
            }
        } else if (fuzz.Which() == CInt_fuzz::e_Range) {
            // range
            if (itv.IsSetStrand()  &&   itv.GetStrand() == eNa_strand_minus) {
                if (is_last) {
                    retval |= eSeqlocPartial_Stop;
                } 
            } else {
                if (is_first) {
                    retval |= eSeqlocPartial_Start;
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
                scope.GetBioseqHandle(itv.GetId());
            bool miss_end = false;
            if ( hnd ) {                            
                if (itv.GetTo() != hnd.GetBioseqLength() - 1) {
                    miss_end = true;
                }
            }
            if (itv.IsSetStrand()  &&
                itv.GetStrand() == eNa_strand_minus) {
                if ( is_first ) {
                    retval |= eSeqlocPartial_Start;
                } else {
                    retval |= eSeqlocPartial_Internal;
                }
                if (miss_end) {
                    if ( is_first /* was last */) {
                        retval |= eSeqlocPartial_Nostart;
                    } else {
                        retval |= eSeqlocPartial_Nointernal;
                    }
                }
            } else {
                if ( is_last ) {
                    retval |= eSeqlocPartial_Stop;
                } else {
                    retval |= eSeqlocPartial_Internal;
                }
                if ( miss_end ) {
                    if ( is_last ) {
                        retval |= eSeqlocPartial_Nostop;
                    } else {
                        retval |= eSeqlocPartial_Nointernal;
                    }
                }
            }
        }
    }
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
            first = &(loc_iter.GetEmbeddingSeq_loc());
        }
        last = &(loc_iter.GetEmbeddingSeq_loc());
    }
    if (!first) {
        return retval;
    }

    CSeq_loc_CI i2(loc, CSeq_loc_CI::eEmpty_Allow);
    for ( ; i2; ++i2 ) {
        const CSeq_loc* slp = &(i2.GetEmbeddingSeq_loc());
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
                SeqIntPartialCheck(slp->GetInt(), retval,
                    slp == first, slp == last, *scope);
            }
            break;
        case CSeq_loc::e_Packed_int:
            {
                const CPacked_seqint::Tdata& ints = slp->GetPacked_int().Get();
                const CSeq_interval* first_int =
                    ints.empty() ? 0 : ints.front().GetPointer();
                const CSeq_interval* last_int =
                    ints.empty() ? 0 : ints.back().GetPointer();
                ITERATE(CPacked_seqint::Tdata, it, ints) {
                    SeqIntPartialCheck(**it, retval,
                        slp == first  &&  *it == first_int,
                        slp == last  &&  *it == last_int,
                        *scope);
                    ++i2;
                }
                break;
            }
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
        if ( !i2 ) {
            break;
        }
    }
    return retval;
}

/////////////////////////////////////////////////////////////////////
//
//  Implementation of SeqLocRevCmp()
//

CSeq_loc* SeqLocRevCmp(const CSeq_loc& loc, CScope* scope)
{
    CReverseComplementHelper helper;
    return GetReverseComplement( loc, &helper );
}

/////////////////////////////////////////////////////////////////////
//
//  Implementation of Compare()
//


typedef map<CSeq_id_Handle, CSeq_id_Handle> TSynMap;

// Map the id to it's 'main' synonym.
CSeq_id_Handle s_GetSynHandle(CSeq_id_Handle id, TSynMap& syns, CScope* scope)
{
    TSynMap::const_iterator syn_it = syns.find(id);
    if (syn_it != syns.end()) {
        // known id
        return syn_it->second;
    }
    // Unknown id, try to find a match
    ITERATE(TSynMap, sit, syns) {
        if ( IsSameBioseq(sit->first, id, scope) ) {
            // Found a synonym
            CSeq_id_Handle map_to = sit->second;
            syns[id] = map_to;
            return map_to;
        }
    }
    syns[id] = id;
    return id;
}


typedef CRange<TSeqPos>  TRangeInfo;
typedef list<TRangeInfo> TRangeInfoList;
typedef map<CSeq_id_Handle, TRangeInfoList> TRangeInfoMap;

// Convert the seq-loc to TRangeInfos. The id map is used to
// normalize ids.
void s_SeqLocToRangeInfoMap(const CSeq_loc& loc,
                            TRangeInfoMap&  infos,
                            TSynMap&        syns,
                            CScope*         scope)
{
    CSeq_loc_CI it(loc,
        CSeq_loc_CI::eEmpty_Skip, CSeq_loc_CI::eOrder_Positional);
    for ( ; it; ++it) {
        TRangeInfo info;
        if ( it.IsWhole() ) {
            info.SetOpen(0, GetLength(it.GetSeq_id(), scope));
        }
        else {
            info.SetOpen(it.GetRange().GetFrom(), it.GetRange().GetToOpen());
        }
        CSeq_id_Handle id = s_GetSynHandle(it.GetSeq_id_Handle(), syns, scope);
        infos[id].push_back(info);
    }
    NON_CONST_ITERATE(TRangeInfoMap, it, infos) {
        it->second.sort();
    }
}


ECompare Compare(const CSeq_loc& me,
                 const CSeq_loc& you,
                 CScope*         scope)
{
    TRangeInfoMap me_infos, you_infos;
    TSynMap syns;
    s_SeqLocToRangeInfoMap(me, me_infos, syns, scope);
    s_SeqLocToRangeInfoMap(you, you_infos, syns, scope);

    // Check if locs are equal. The ranges are sorted now, so the original
    // order is not important.
    if (me_infos.size() == you_infos.size()) {
        bool equal = true;
        TRangeInfoMap::const_iterator mid_it = me_infos.begin();
        TRangeInfoMap::const_iterator yid_it = you_infos.begin();
        for ( ; mid_it != me_infos.end(); ++mid_it, ++yid_it) {
            _ASSERT(yid_it != you_infos.end());
            if (mid_it->first != yid_it->first  ||
                mid_it->second.size() != yid_it->second.size()) {
                equal = false;
                break;
            }
            TRangeInfoList::const_iterator mit = mid_it->second.begin();
            TRangeInfoList::const_iterator yit = yid_it->second.begin();
            for ( ; mit != mid_it->second.end(); ++mit, ++yit) {
                _ASSERT(yit != yid_it->second.end());
                if (*mit != *yit) {
                    equal = false;
                    break;
                }
            }
            if ( !equal ) break;
        }
        if ( equal ) {
            return eSame;
        }
    }

    // Check if 'me' is contained or overlapping with 'you'.
    bool me_contained = true;
    bool overlap = false;

    ITERATE(TRangeInfoMap, mid_it, me_infos) {
        TRangeInfoMap::const_iterator yid_it = you_infos.find(mid_it->first);
        if (yid_it == you_infos.end()) {
            // The id is missing from 'you'.
            me_contained = false;
            if ( overlap ) {
                break; // nothing else to check
            }
            continue; // check for overlap
        }
        ITERATE(TRangeInfoList, mit, mid_it->second) {
            // current range is contained in 'you'?
            bool mit_contained = false;
            ITERATE(TRangeInfoList, yit, yid_it->second) {
                if (yit->GetToOpen() > mit->GetFrom()  &&
                    yit->GetFrom() < mit->GetToOpen()) {
                    overlap = true;
                    if (yit->GetFrom() <= mit->GetFrom()  &&
                        yit->GetToOpen() >= mit->GetToOpen()) {
                        mit_contained = true;
                        break;
                    }
                }
            }
            if ( !mit_contained ) {
                me_contained = false;
                if ( overlap ) break; // nothing else to check
            }
        }
        if (!me_contained  &&  overlap) {
            break; // nothing else to check
        }
    }

    // Reverse check: if 'you' is contained in 'me'.
    bool you_contained = true;

    ITERATE(TRangeInfoMap, yid_it, you_infos) {
        TRangeInfoMap::const_iterator mid_it = me_infos.find(yid_it->first);
        if (mid_it == me_infos.end()) {
            // The id is missing from 'me'.
            you_contained = false;
            break; // nothing else to check
        }
        ITERATE(TRangeInfoList, yit, yid_it->second) {
            // current range is contained in 'me'?
            bool yit_contained = false;
            ITERATE(TRangeInfoList, mit, mid_it->second) {
                if (mit->GetFrom() <= yit->GetFrom()  &&
                    mit->GetToOpen() >= yit->GetToOpen()) {
                    yit_contained = true;
                    break;
                }
            }
            if ( !yit_contained ) {
                you_contained = false;
                break;
            }
        }
        if ( !you_contained ) {
            break; // nothing else to check
        }
    }

    // Always prefere 'eContains' over 'eContained'.
    if ( you_contained ) {
        return eContains;
    }
    if ( me_contained ) {
        return eContained;
    }
    return overlap ? eOverlap : eNoOverlap;
}


/////////////////////////////////////////////////////////////////////
//
//  Implementation of TestForOverlap()
//

bool s_Test_Strands(ENa_strand strand1, ENa_strand strand2)
{
    // Check strands. Overlapping rules for strand:
    //   - equal strands overlap
    //   - "both" overlaps with any other
    //   - "unknown" overlaps with any other except "minus"
    //   - "other" indicates mixed strands and needs a closer look
    if (strand1 == eNa_strand_other  ||  strand2 == eNa_strand_other) {
        return false;
    }
    return strand1 == strand2
        || strand1 == eNa_strand_both
        || strand2 == eNa_strand_both
        || (strand1 == eNa_strand_unknown  && strand2 != eNa_strand_minus)
        || (strand2 == eNa_strand_unknown  && strand1 != eNa_strand_minus);
}


inline
Int8 AbsInt8(Int8 x)
{
    return x < 0 ? -x : x;
}


typedef pair<TRangeInfo, TRangeInfo> TRangeInfoByStrand;
typedef map<CSeq_id_Handle, TRangeInfoByStrand> TTotalRangeInfoMap;

// Convert the seq-loc to TTotalRangeInfoMap, which stores total
// ranges by id and strand. The id map is used to normalize ids.
void s_SeqLocToTotalRangeInfoMap(const CSeq_loc&     loc,
                                 TTotalRangeInfoMap& infos,
                                 TSynMap&            syns,
                                 CScope*             scope)
{
    CSeq_loc_CI it(loc,
        CSeq_loc_CI::eEmpty_Skip, CSeq_loc_CI::eOrder_Positional);
    for ( ; it; ++it) {
        TRangeInfo rg;
        if ( it.IsWhole() ) {
            rg.SetOpen(0, GetLength(it.GetSeq_id(), scope));
        }
        else {
            rg.SetOpen(it.GetRange().GetFrom(), it.GetRange().GetToOpen());
        }
        CSeq_id_Handle id = s_GetSynHandle(it.GetSeq_id_Handle(), syns, scope);
        if ( IsReverse(it.GetStrand()) ) {
            infos[id].second.CombineWith(rg);
        }
        else {
            infos[id].first.CombineWith(rg);
        }
    }
}


int TestForOverlap(const CSeq_loc& loc1,
                   const CSeq_loc& loc2,
                   EOverlapType type,
                   TSeqPos circular_len,
                   CScope* scope)
{
    Int8 ret = TestForOverlap64(loc1, loc2, type, circular_len, scope);
    return ret <= kMax_Int ? int(ret) : kMax_Int;
}


typedef pair<TRangeInfoList, TRangeInfoList> TRangeInfoListByStrand;
typedef map<CSeq_id_Handle, TRangeInfoListByStrand> TRangeInfoMapByStrand;

// Convert the seq-loc to TRangeInfos for each strand. The id map is used to
// normalize ids.
void s_SeqLocToRangeInfoMapByStrand(const CSeq_loc&         loc,
                                    TRangeInfoMapByStrand&  infos,
                                    TSynMap&                syns,
                                    CScope*                 scope)
{
    CSeq_loc_CI it(loc,
        CSeq_loc_CI::eEmpty_Skip, CSeq_loc_CI::eOrder_Positional);
    for ( ; it; ++it) {
        TRangeInfo info;
        if ( it.IsWhole() ) {
            info.SetOpen(0, GetLength(it.GetSeq_id(), scope));
        }
        else {
            info.SetOpen(it.GetRange().GetFrom(), it.GetRange().GetToOpen());
        }
        CSeq_id_Handle id = s_GetSynHandle(it.GetSeq_id_Handle(), syns, scope);
        if (it.IsSetStrand()  &&  IsReverse(it.GetStrand())) {
            infos[id].second.push_back(info);
        }
        else {
            infos[id].first.push_back(info);
        }
    }
    NON_CONST_ITERATE(TRangeInfoMapByStrand, it, infos) {
        it->second.first.sort();
        it->second.second.sort();
    }
}


struct STopologyInfo
{
    bool    circular;
    TSeqPos length;
};

typedef map<CSeq_id_Handle, STopologyInfo> TTopologyMap;

STopologyInfo s_GetTopology(CSeq_id_Handle idh,
                            TTopologyMap&  topologies,
                            EOverlapFlags flags,
                            CScope*       scope)
{
    TTopologyMap::const_iterator found = topologies.find(idh);
    if (found != topologies.end()) {
        return found->second;
    }
    STopologyInfo info;
    info.circular = false;
    info.length = kInvalidSeqPos;
    if ( scope ) {
        CBioseq_Handle bh = scope->GetBioseqHandle(idh);
        if ( bh ) {
            if ((flags & fOverlap_IgnoreTopology) == 0) {
                info.circular = (bh.IsSetInst_Topology()  &&
                    bh.GetInst_Topology() == CSeq_inst::eTopology_circular);
            }
            info.length = bh.GetBioseqLength();
        }
    }
    topologies[idh] = info;
    return info;
}


// Convert the seq-loc to TRangeInfos using extremes for each bioseq,
// strand and ordered set of ranges. The id map is used to normalize ids.
void s_SeqLocToTotalRangesInfoMapByStrand(const CSeq_loc&         loc,
                                          TRangeInfoMapByStrand&  infos,
                                          TSynMap&                syns,
                                          TTopologyMap&           topologies,
                                          EOverlapFlags           flags,
                                          CScope*                 scope)
{
    CSeq_loc_CI it(loc,
        CSeq_loc_CI::eEmpty_Skip, CSeq_loc_CI::eOrder_Biological);
    if ( !it ) return;

    CSeq_id_Handle last_id = s_GetSynHandle(it.GetSeq_id_Handle(), syns, scope);
    TRangeInfo last_rg;
    bool last_reverse = it.IsSetStrand()  &&  IsReverse(it.GetStrand());
    // In case of circular bioseq allow to cross zero only once.
    bool crossed_zero = false;

    TRangeInfo total_range;
    for ( ; it; ++it) {
        CSeq_id_Handle id = s_GetSynHandle(it.GetSeq_id_Handle(), syns, scope);
        TRangeInfo rg = it.GetRange();
        STopologyInfo topo =
            s_GetTopology(id, topologies, flags, scope);
        bool reverse = it.IsSetStrand()  &&  IsReverse(it.GetStrand());
        bool break_range = reverse != last_reverse  ||  id != last_id;

        bool bad_order = false;
        // Don't try to check the order if id or strand have just changed.
        // Also don't check it for the first range.
        if ( !break_range  &&  !last_rg.Empty() ) {
            if ( reverse ) {
                if (rg.GetFrom() > last_rg.GetFrom()) {
                    bad_order = true;
                    if ( topo.circular ) {
                        if ( !crossed_zero ) {
                            total_range.SetFrom(0);
                        }
                        crossed_zero = true;
                    }
                }
            }
            else {
                if (rg.GetFrom() < last_rg.GetFrom()) {
                    bad_order = true;
                    if ( topo.circular ) {
                        if ( !crossed_zero ) {
                            total_range.SetToOpen(topo.length != kInvalidSeqPos ?
                                topo.length : TRangeInfo::GetWholeToOpen());
                            crossed_zero = true;
                        }
                    }
                }
            }
        }
        if (break_range  ||  bad_order) {
            // Push the next total range, start the new one
            if ( last_reverse ) {
                infos[last_id].second.push_back(total_range);
            }
            else {
                infos[last_id].first.push_back(total_range);
            }
            total_range = TRangeInfo::GetEmpty();
            if ( crossed_zero ) {
                if ( reverse ) {
                    rg.SetToOpen(topo.length != kInvalidSeqPos ?
                        topo.length : TRangeInfo::GetWholeToOpen());
                }
                else {
                    rg.SetFrom(0);
                }
            }
            crossed_zero = false;
        }
        last_rg = rg;
        total_range.CombineWith(rg);
        last_id = id;
        last_reverse = reverse;
    }
    if ( !total_range.Empty() ) {
        if ( last_reverse ) {
            infos[last_id].second.push_back(total_range);
        }
        else {
            infos[last_id].first.push_back(total_range);
        }
    }
    NON_CONST_ITERATE(TRangeInfoMapByStrand, it, infos) {
        it->second.first.sort();
        it->second.second.sort();
    }
}


Int8 s_GetUncoveredLength(const TRangeInfoList& ranges1,
                          const TRangeInfoList& ranges2)
{
    Int8 diff = 0;
    ITERATE(TRangeInfoList, rg_it1, ranges1) {
        TRangeInfo rg = *rg_it1;
        ITERATE(TRangeInfoList, rg_it2, ranges2) {
            if (rg_it2->GetFrom() > rg.GetTo()) break;
            if ( !rg.IntersectingWith(*rg_it2) ) continue;
            if (rg_it2->GetFrom() > rg.GetFrom()) {
                diff += static_cast<Int8>(rg_it2->GetFrom() - rg.GetFrom());
            }
            if (rg_it2->GetTo() < rg.GetTo()) {
                rg.SetFrom(rg_it2->GetToOpen());
            }
            else {
                rg = TRangeInfo::GetEmpty();
                break;
            }
        }
        if (rg.IsWhole()) return numeric_limits<Int8>::max();
        diff += rg.GetLength();
    }
    return diff;
}


// Calculate sum of all subranges from ranges1 not covered by ranges2.
Int8 s_GetUncoveredLength(const TRangeInfoMapByStrand& ranges1,
                          const TRangeInfoMapByStrand& ranges2)
{
    Int8 diff = 0;
    ITERATE(TRangeInfoMapByStrand, id_it1, ranges1) {
        TRangeInfoMapByStrand::const_iterator id_it2 = ranges2.find(id_it1->first);
        if (id_it2 != ranges2.end()) {
            Int8 diff_plus = s_GetUncoveredLength(id_it1->second.first, id_it2->second.first);
            Int8 diff_minus = s_GetUncoveredLength(id_it1->second.second, id_it2->second.second);
            if (diff_plus == numeric_limits<Int8>::max()) return diff_plus;
            if (diff_minus == numeric_limits<Int8>::max()) return diff_minus;
            diff += diff_plus + diff_minus;
        }
        else {
            ITERATE(TRangeInfoList, rg_it, id_it1->second.first) {
                if (rg_it->IsWhole()) return numeric_limits<Int8>::max();
                diff += rg_it->GetLength();
            }
            ITERATE(TRangeInfoList, rg_it, id_it1->second.second) {
                if (rg_it->IsWhole()) return numeric_limits<Int8>::max();
                diff += rg_it->GetLength();
            }
        }
    }
    return diff;
}


bool s_Test_Subset(const CSeq_loc& loc1,
                   const CSeq_loc& loc2,
                   CScope*         scope)
{
    TSynMap syns;
    TRangeInfoMapByStrand rm1, rm2;
    s_SeqLocToRangeInfoMapByStrand(loc1, rm1, syns, scope);
    s_SeqLocToRangeInfoMapByStrand(loc2, rm2, syns, scope);
    ITERATE(TRangeInfoMapByStrand, id_it2, rm2) {
        // For each id from loc2 find an entry in loc1.
        TRangeInfoMapByStrand::iterator id_it1 = rm1.find(id_it2->first);
        if (id_it1 == rm1.end()) {
            return false; // unmatched id in loc2
        }
        const TRangeInfoList& rglist1_plus = id_it1->second.first;
        const TRangeInfoList& rglist1_minus = id_it1->second.second;
        const TRangeInfoList& rglist2_plus = id_it2->second.first;
        const TRangeInfoList& rglist2_minus = id_it2->second.second;
        ITERATE(TRangeInfoList, it2, rglist2_plus) {
            bool contained = false;
            ITERATE(TRangeInfoList, it1, rglist1_plus) {
                // Already missed the rage on loc2?
                if (!contained  &&  it1->GetFrom() > it2->GetFrom()) {
                    return false;
                }
                // found a contaning range?
                if (it1->IsWhole()  ||
                    (it1->GetFrom() <= it2->GetFrom()  &&
                    it1->GetTo() >= it2->GetTo())) {
                    contained = true;
                    break;
                }
            }
            if ( !contained ) return false;
        }
        ITERATE(TRangeInfoList, it2, rglist2_minus) {
            bool contained = false;
            ITERATE(TRangeInfoList, it1, rglist1_minus) {
                // Already missed the rage on loc2?
                if (!contained  &&  it1->GetFrom() > it2->GetFrom()) {
                    return false;
                }
                // found a contaning range?
                if (it1->IsWhole()  ||
                    (it1->GetFrom() <= it2->GetFrom()  &&
                    it1->GetTo() >= it2->GetTo())) {
                    contained = true;
                    break;
                }
            }
            if ( !contained ) return false;
        }
    }
    return true;
}


bool s_Test_CheckIntervals(CSeq_loc_CI it1,
                           CSeq_loc_CI it2,
                           bool minus_strand,
                           CScope* scope,
                           bool single_id)
{
    // Check intervals one by one
    while ( it1  &&  it2 ) {
        bool same_it_id = single_id;
        if ( !same_it_id ) {
            if ( !IsSameBioseq(it1.GetSeq_id(), it2.GetSeq_id(), scope) ) {
                return false;
            }
        }
        if ( !s_Test_Strands(it1.GetStrand(), it2.GetStrand()) ) {
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


// Test for overlap using extremes rather than ranges (simple, contained).
// Used for multi-id, multi-strand and out-of-order locations.
Int8 s_Test_Extremes(const CSeq_loc& loc1,
                     const CSeq_loc& loc2,
                     EOverlapType    type,
                     TSynMap&        syns,
                     TTopologyMap&   topologies,
                     EOverlapFlags   flags,
                     CScope*         scope)
{
    // Here we accept only two overlap types.
    _ASSERT(type == eOverlap_Simple  ||  type == eOverlap_Contained);

    TRangeInfoMapByStrand ranges1, ranges2;

    s_SeqLocToTotalRangesInfoMapByStrand(loc1, ranges1, syns, topologies, flags, scope);
    s_SeqLocToTotalRangesInfoMapByStrand(loc2, ranges2, syns, topologies, flags, scope);

    bool overlap = false;
    ITERATE(TRangeInfoMapByStrand, id_it2, ranges2) {
        TRangeInfoMapByStrand::const_iterator id_it1 = ranges1.find(id_it2->first);
        if (id_it1 == ranges1.end()) {
            if (type == eOverlap_Contained) {
                // loc2 has parts not contained in loc1
                return -1;
            }
            else { // eOverlap_Simple
                continue; // next id_it2
            }
        }
        // Found the same id in loc1 - check ranges.
        // Plus strand
        ITERATE(TRangeInfoList, rg_it2, id_it2->second.first) {
            bool contained = false;
            ITERATE(TRangeInfoList, rg_it1, id_it1->second.first) {
                if ( !rg_it2->IntersectingWith(*rg_it1) ) {
                    // Ranges are sorted, we can stop as soon as
                    // we go beyond the right end of rg2.
                    if (rg_it2->GetTo() < rg_it1->GetFrom()) break;
                    continue;
                }
                overlap = true;
                if (type == eOverlap_Contained) {
                    if (rg_it2->GetFrom() >= rg_it1->GetFrom()  &&
                        rg_it2->GetTo() <= rg_it1->GetTo()) {
                        contained = true;
                        break;
                    }
                }
                else { // eOverlap_Simple
                    break; // found overlap, go to next range from loc2
                }
            }
            if (type == eOverlap_Contained) {
                if ( !contained ) return -1;
            }
            else if ( overlap ) break;
        }
        // Munis strand
        ITERATE(TRangeInfoList, rg_it2, id_it2->second.second) {
            bool contained = false;
            ITERATE(TRangeInfoList, rg_it1, id_it1->second.second) {
                if ( !rg_it2->IntersectingWith(*rg_it1) ) {
                    // Ranges are sorted, we can stop as soon as
                    // we go beyond the right end of rg2.
                    if (rg_it2->GetTo() < rg_it1->GetFrom()) break;
                    continue;
                }
                overlap = true;
                if (type == eOverlap_Contained) {
                    if (rg_it2->GetFrom() >= rg_it1->GetFrom()  &&
                        rg_it2->GetTo() <= rg_it1->GetTo()) {
                        contained = true;
                        break;
                    }
                }
                else { // eOverlap_Simple
                    break; // found overlap, go to next range from loc2
                }
            }
            if (type == eOverlap_Contained) {
                if ( !contained ) return -1;
            }
            else if ( overlap ) break;
        }
    }

    // Now it's time to calculate quality of the overlap. Take into
    // account that a single range from one location may contain/overlap
    // multiple ranges from another location.
    if (type == eOverlap_Contained) {
        // There should be no subranges in ranges2 not covered by ranges1.
        return s_GetUncoveredLength(ranges1, ranges2);
    }
    if (type == eOverlap_Simple  &&  overlap) {
        Int8 diff1 = s_GetUncoveredLength(ranges1, ranges2);
        Int8 diff2 = s_GetUncoveredLength(ranges2, ranges1);
        if (diff1 == numeric_limits<Int8>::max()) return diff1;
        if (diff2 == numeric_limits<Int8>::max()) return diff2;
        return diff1 + diff2;
    }
    return -1;
}


Int8 s_Test_Interval(const CSeq_loc& loc1,
                     const CSeq_loc& loc2,
                     TSynMap&        syns,
                     TTopologyMap&   topologies,
                     EOverlapFlags   flags,
                     CScope*         scope)
{
    TRangeInfoMapByStrand ranges1, ranges2;

    s_SeqLocToRangeInfoMapByStrand(loc1, ranges1, syns, scope);
    s_SeqLocToRangeInfoMapByStrand(loc2, ranges2, syns, scope);

    bool overlap = false;
    ITERATE(TRangeInfoMapByStrand, id_it1, ranges1) {
        TRangeInfoMapByStrand::const_iterator id_it2 = ranges2.find(id_it1->first);
        if (id_it2 == ranges2.end()) continue;
        // Plus strand ranges
        ITERATE(TRangeInfoList, rg_it1, id_it1->second.first) {
            ITERATE(TRangeInfoList, rg_it2, id_it2->second.first) {
                if ( rg_it1->IntersectingWith(*rg_it2) ) {
                    overlap = true;
                    break;
                }
            }
            if ( overlap ) break;
        }
        if ( overlap ) break;
        // Minus strand ranges
        ITERATE(TRangeInfoList, rg_it1, id_it1->second.second) {
            ITERATE(TRangeInfoList, rg_it2, id_it2->second.second) {
                if ( rg_it1->IntersectingWith(*rg_it2) ) {
                    overlap = true;
                    break;
                }
            }
            if ( overlap ) break;
        }
        if ( overlap ) break;
    }

    if ( !overlap ) return -1;

    ranges1.clear();
    ranges2.clear();
    s_SeqLocToTotalRangesInfoMapByStrand(loc1, ranges1,
        syns, topologies, flags, scope);
    s_SeqLocToTotalRangesInfoMapByStrand(loc2, ranges2,
        syns, topologies, flags, scope);

    Int8 diff1 = s_GetUncoveredLength(ranges1, ranges2);
    Int8 diff2 = s_GetUncoveredLength(ranges2, ranges1);
    if (diff1 == numeric_limits<Int8>::max()) return diff1;
    if (diff2 == numeric_limits<Int8>::max()) return diff2;
    return diff1 + diff2;
}


Int8 s_TestForOverlapEx(const CSeq_loc& loc1,
                        const CSeq_loc& loc2,
                        EOverlapType    type,
                        EOverlapFlags   flags,
                        TSeqPos         circular_len,
                        CScope*         scope)
{
    if (circular_len == 0) {
        circular_len = kInvalidSeqPos;
    }
    // Do not allow conflicting values.
    if (circular_len != kInvalidSeqPos  &&  (flags & fOverlap_IgnoreTopology)) {
        NCBI_THROW(CObjmgrUtilException, eBadSequenceType,
            "Circular length can not be combined with no-topology flag.");
    }

    const CSeq_loc* ploc1 = &loc1;
    const CSeq_loc* ploc2 = &loc2;
    typedef CRange<Int8> TRange8;
    const CSeq_id *id1 = NULL;
    const CSeq_id *id2 = NULL;
    id1 = loc1.GetId();
    id2 = loc2.GetId();
    TSynMap syns;
    TTopologyMap topologies;
    bool single_seq = true;

    // Get seq-ids. They should be cached by GetTotalRange() above and should
    // not be null.
    if (id1  &&  id2) {
        if ( !IsSameBioseq(*id1, *id2, scope) ) return -1;
        // Use known id and topology if there's just one sequence.
        CSeq_id_Handle idh1 = CSeq_id_Handle::GetHandle(*id1);
        CSeq_id_Handle idh2 = CSeq_id_Handle::GetHandle(*id2);
        syns[idh1] = idh1;
        if (idh2 != idh1) {
            syns[idh2] = idh1;
        }
        if (circular_len != kInvalidSeqPos) {
            STopologyInfo topo;
            topo.circular = true;
            topo.length = circular_len;
            topologies[idh1] = topo;
        }
    }
    else {
        if (flags & fOverlap_NoMultiSeq) {
            NCBI_THROW(CObjmgrUtilException, eBadLocation,
                "Multi-bioseq locations are disabled by the flags.");
        }
        // Multi-id locations - no circular_len allowed
        if (circular_len != kInvalidSeqPos) {
            NCBI_THROW(CObjmgrUtilException, eBadLocation,
                "Circular bioseq length can not be specified "
                "for multi-bioseq locations.");
        }
        single_seq = false;
    }

    // Shortcut - if strands do not intersect, don't even look at the ranges.
    ENa_strand strand1 = GetStrand(loc1);
    ENa_strand strand2 = GetStrand(loc2);
    if ( !s_Test_Strands(strand1, strand2) ) {
        // For multi-seq strand is unknown rather than other -
        // can not use this test.
        if (single_seq  &&
            strand1 != eNa_strand_other && strand2 != eNa_strand_other ) {
            // singular but incompatible strands
            return -1;
        }
        // There is a possibility of multiple strands that needs to be
        // checked too (if allowed by the flags).
        if (flags & fOverlap_NoMultiStrand) {
            NCBI_THROW(CObjmgrUtilException, eBadLocation,
                "Multi-strand locations are disabled by the flags.");
        }
    }

    switch (type) {
    case eOverlap_Simple:
        return s_Test_Extremes(loc1, loc2, eOverlap_Simple,
            syns, topologies, flags, scope);
    case eOverlap_Contains:
        swap(ploc1, ploc2);
        // Go on to the next case
    case eOverlap_Contained:
        return s_Test_Extremes(*ploc1, *ploc2, eOverlap_Contained,
            syns, topologies, flags, scope);
    case eOverlap_SubsetRev:
        swap(ploc1, ploc2);
        // continue to eOverlap_Subset case
    case eOverlap_Subset:
        if ( !s_Test_Subset(*ploc1, *ploc2, scope) ) return -1;
        return Int8(GetCoverage(*ploc1, scope)) -
            Int8(GetCoverage(*ploc2, scope));
    case eOverlap_CheckIntRev:
        swap(ploc1, ploc2);
        // Go on to the next case
    case eOverlap_CheckIntervals:
        {
            // Check intervals' boundaries.
            CSeq_loc_CI it1(*ploc1);
            CSeq_loc_CI it2(*ploc2);
            if (!it1  ||  !it2) {
                return -1;
            }
            TSeqPos loc2start = it2.GetRange().GetFrom();
            TSeqPos loc2end = it2.GetRange().GetTo();
            bool loc2rev = it2.GetStrand() == eNa_strand_minus;
            bool single_id = (id1  &&  id2);
            for ( ; it1; ++it1) {
                // If there are multiple ids per seq-loc, check each pair.
                if ( !single_id ) {
                    if ( !IsSameBioseq(it1.GetSeq_id(), it2.GetSeq_id(),
                        scope) ) continue;
                }
                // Find the first range in loc1 containing the first range
                // of loc2. s_Test_CheckIntervals will do the rest.
                if (it1.GetRange().GetFrom() <= loc2start  &&
                    it1.GetRange().GetTo() >= loc2end  &&
                    s_Test_CheckIntervals(it1, it2, loc2rev, scope, single_id)) {
                    // GetLength adds up all strands/seqs, but for this overlap
                    // type it's ok.
                    return Int8(GetLength(*ploc1, scope)) -
                        Int8(GetLength(*ploc2, scope));
                }
            }
            return -1;
        }
    case eOverlap_Interval:
        return s_Test_Interval(loc1, loc2, syns, topologies, flags, scope);
    }
    return -1;
}


Int8 TestForOverlap64(const CSeq_loc& loc1,
                      const CSeq_loc& loc2,
                      EOverlapType    type,
                      TSeqPos         circular_len,
                      CScope*         scope)
{
    return s_TestForOverlapEx(loc1, loc2, type, fOverlap_Default, circular_len, scope);
}


Int8 TestForOverlapEx(const CSeq_loc& loc1,
                      const CSeq_loc& loc2,
                      EOverlapType    type,
                      CScope*         scope,
                      EOverlapFlags   flags)
{
    return s_TestForOverlapEx(loc1, loc2, type, flags, kInvalidSeqPos, scope);
}


/////////////////////////////////////////////////////////////////////
//
//  Seq-loc operations
//

class CDefaultSynonymMapper : public ISynonymMapper
{
public:
    CDefaultSynonymMapper(CScope* scope);
    virtual ~CDefaultSynonymMapper(void);

    virtual CSeq_id_Handle GetBestSynonym(const CSeq_id& id);

private:
    typedef map<CSeq_id_Handle, CSeq_id_Handle> TSynonymMap;

    CRef<CSeq_id_Mapper> m_IdMapper;
    TSynonymMap          m_SynMap;
    CScope*              m_Scope;
};


CDefaultSynonymMapper::CDefaultSynonymMapper(CScope* scope)
    : m_IdMapper(CSeq_id_Mapper::GetInstance()),
      m_Scope(scope)
{
    return;
}


CDefaultSynonymMapper::~CDefaultSynonymMapper(void)
{
    return;
}


CSeq_id_Handle CDefaultSynonymMapper::GetBestSynonym(const CSeq_id& id)
{
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(id);
    if ( !m_Scope  ||  id.Which() == CSeq_id::e_not_set ) {
        return idh;
    }
    TSynonymMap::iterator id_syn = m_SynMap.find(idh);
    if (id_syn != m_SynMap.end()) {
        return id_syn->second;
    }
    CSeq_id_Handle best;
    int best_rank = kMax_Int;
    CConstRef<CSynonymsSet> syn_set = m_Scope->GetSynonyms(idh);
    ITERATE(CSynonymsSet, syn_it, *syn_set) {
        CSeq_id_Handle synh = syn_set->GetSeq_id_Handle(syn_it);
        int rank = synh.GetSeqId()->BestRankScore();
        if (rank < best_rank) {
            best = synh;
            best_rank = rank;
        }
    }
    if ( !best ) {
        // Synonyms set was empty
        m_SynMap[idh] = idh;
        return idh;
    }
    ITERATE(CSynonymsSet, syn_it, *syn_set) {
        m_SynMap[syn_set->GetSeq_id_Handle(syn_it)] = best;
    }
    return best;
}


class CDefaultLengthGetter : public ILengthGetter
{
public:
    CDefaultLengthGetter(CScope* scope);
    virtual ~CDefaultLengthGetter(void);

    virtual TSeqPos GetLength(const CSeq_id& id);

protected:
    CScope*              m_Scope;
};


CDefaultLengthGetter::CDefaultLengthGetter(CScope* scope)
    : m_Scope(scope)
{
    return;
}


CDefaultLengthGetter::~CDefaultLengthGetter(void)
{
    return;
}


TSeqPos CDefaultLengthGetter::GetLength(const CSeq_id& id)
{
    if (id.Which() == CSeq_id::e_not_set) {
        return 0;
    }
    CBioseq_Handle bh;
    if ( m_Scope ) {
        bh = m_Scope->GetBioseqHandle(id);
    }
    if ( !bh ) {
        NCBI_THROW(CException, eUnknown,
            "Can not get length of whole location");
    }
    return bh.GetBioseqLength();
}


CRef<CSeq_loc> Seq_loc_Merge(const CSeq_loc&    loc,
                             CSeq_loc::TOpFlags flags,
                             CScope*            scope)
{
    CDefaultSynonymMapper syn_mapper(scope);
    return loc.Merge(flags, &syn_mapper);
}


CRef<CSeq_loc> Seq_loc_Add(const CSeq_loc&    loc1,
                           const CSeq_loc&    loc2,
                           CSeq_loc::TOpFlags flags,
                           CScope*            scope)
{
    CDefaultSynonymMapper syn_mapper(scope);
    return loc1.Add(loc2, flags, &syn_mapper);
}


CRef<CSeq_loc> Seq_loc_Subtract(const CSeq_loc&    loc1,
                                const CSeq_loc&    loc2,
                                CSeq_loc::TOpFlags flags,
                                CScope*            scope)
{
    CDefaultSynonymMapper syn_mapper(scope);
    CDefaultLengthGetter len_getter(scope);
    return loc1.Subtract(loc2, flags, &syn_mapper, &len_getter);
}


END_SCOPE(sequence)
END_SCOPE(objects)
END_NCBI_SCOPE
