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

#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(sequence)


TSeqPos GetLength(const CSeq_id& id, CScope* scope)
{
    if ( !scope ) {
        return numeric_limits<TSeqPos>::max();
    }
    CBioseq_Handle hnd = scope->GetBioseqHandle(id);
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
    case CSeq_loc::e_not_set:
    case CSeq_loc::e_Bond:         //can't calculate length
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
            ERR_POST(e.what() << ": CSeq_id1: " << id1.GetSeqId()->DumpAsFasta()
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

    for (CSeq_loc_CI it(loc); it; ++it) {
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


TSeqPos GetStart(const CSeq_loc& loc, CScope* scope)
{
    // Throw CObjmgrUtilException if loc does not represent one CBioseq
    GetId(loc, scope);

    CSeq_loc::TRange rg = loc.GetTotalRange();
    return rg.GetFrom();
}


TSeqPos GetStop(const CSeq_loc& loc, CScope* scope)
{
    // Throw CObjmgrUtilException if loc does not represent one CBioseq
    GetId(loc, scope);

    CSeq_loc::TRange rg = loc.GetTotalRange();
    return rg.GetTo();
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
                            if (itv.GetTo() != hnd.GetBioseqLength() - 1) {
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


/////////////////////////////////////////////////////////////////////
//
//  Implementation of SeqLocRevCmp()
//

static CSeq_interval* s_SeqIntRevCmp(const CSeq_interval& i,
                                     CScope* /* scope */)
{
    CRef<CSeq_interval> rev_int(new CSeq_interval);
    rev_int->Assign(i);
    
    ENa_strand s = i.CanGetStrand() ? i.GetStrand() : eNa_strand_unknown;
    rev_int->SetStrand(Reverse(s));

    return rev_int.Release();
}


static CSeq_point* s_SeqPntRevCmp(const CSeq_point& pnt,
                                  CScope* /* scope */)
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


/////////////////////////////////////////////////////////////////////
//
//  Implementation of Compare()
//

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


/////////////////////////////////////////////////////////////////////
//
//  Implementation of TestForOverlap()
//

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


inline
Int8 AbsInt8(Int8 x)
{
    return x < 0 ? -x : x;
}


// Checks for eContained.
// May be used for eContains with reverse order of ranges.
inline
Int8 x_GetRangeDiff(const CHandleRange& hrg1,
                    const CHandleRange& hrg2,
                    CHandleRange::ETotalRangeFlags strand_flag)
{
    CRange<TSeqPos> rg1 =
        hrg1.GetOverlappingRange(strand_flag);
    CRange<TSeqPos> rg2 =
        hrg2.GetOverlappingRange(strand_flag);
    if ( rg1.GetFrom() <= rg2.GetFrom()  &&
        rg1.GetTo() >= rg2.GetTo() ) {
        return Int8(rg2.GetFrom() - rg1.GetFrom()) +
            Int8(rg1.GetTo() - rg2.GetTo());
    }
    return -1;
}


Int8 x_TestForOverlap_MultiSeq(const CSeq_loc& loc1,
                              const CSeq_loc& loc2,
                              EOverlapType type)
{
    // Special case of TestForOverlap() - multi-sequences locations
    typedef CRange<Int8> TRange8;
    switch (type) {
    case eOverlap_Simple:
        {
            // Find all intersecting intervals
            Int8 diff = 0;
            for (CSeq_loc_CI li1(loc1); li1; ++li1) {
                for (CSeq_loc_CI li2(loc2); li2; ++li2) {
                    if ( !li1.GetSeq_id().Equals(li2.GetSeq_id()) ) {
                        continue;
                    }
                    // Compare strands
                    if ( IsReverse(li1.GetStrand()) !=
                        IsReverse(li2.GetStrand()) ) {
                        continue;
                    }
                    CRange<TSeqPos> rg1 = li1.GetRange();
                    CRange<TSeqPos> rg2 = li2.GetRange();
                    if ( rg1.GetTo() >= rg2.GetFrom()  &&
                         rg1.GetFrom() <= rg2.GetTo() ) {
                        diff += AbsInt8(Int8(rg2.GetFrom()) - Int8(rg1.GetFrom())) +
                            AbsInt8(Int8(rg1.GetTo()) - Int8(rg2.GetTo()));
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
            Int8 diff = 0;
            CHandleRangeMap::const_iterator it2 = rm2.begin();
            for ( ; it2 != rm2.end(); ++it2) {
                CHandleRangeMap::const_iterator it1 =
                    rm1.GetMap().find(it2->first);
                if (it1 == rm1.end()) {
                    // loc2 has region on a sequence not present in loc1
                    return -1;
                }
                Int8 diff_plus = x_GetRangeDiff(it1->second,
                                                it2->second,
                                                CHandleRange::eStrandPlus);
                if (diff_plus < 0) {
                    return -1;
                }
                diff += diff_plus;
                Int8 diff_minus = x_GetRangeDiff(it1->second,
                                                 it2->second,
                                                 CHandleRange::eStrandPlus);
                if (diff_minus < 0) {
                    return -1;
                }
                diff += diff_minus;
            }
            return diff;
        }
    case eOverlap_Contains:
        {
            // loc2 is contained in loc1
            CHandleRangeMap rm1, rm2;
            rm1.AddLocation(loc1);
            rm2.AddLocation(loc2);
            Int8 diff = 0;
            CHandleRangeMap::const_iterator it1 = rm1.begin();
            for ( ; it1 != rm1.end(); ++it1) {
                CHandleRangeMap::const_iterator it2 =
                    rm2.GetMap().find(it1->first);
                if (it2 == rm2.end()) {
                    // loc1 has region on a sequence not present in loc2
                    return -1;
                }
                Int8 diff_plus = x_GetRangeDiff(it2->second,
                                                it1->second,
                                                CHandleRange::eStrandPlus);
                if (diff_plus < 0) {
                    return -1;
                }
                diff += diff_plus;
                Int8 diff_minus = x_GetRangeDiff(it2->second,
                                                 it1->second,
                                                 CHandleRange::eStrandPlus);
                if (diff_minus < 0) {
                    return -1;
                }
                diff += diff_minus;
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


Int8 x_TestForOverlap_MultiStrand(const CSeq_loc& loc1,
                                  const CSeq_loc& loc2,
                                  EOverlapType type,
                                  CScope* scope)
{
    switch (type) {
    case eOverlap_Interval:
        {
            if (Compare(loc1, loc2, scope) == eNoOverlap) {
                return -1;
            }
            // Proceed to eSimple case to calculate diff by strand
        }
    case eOverlap_Simple:
        {
            CHandleRangeMap hrm1;
            CHandleRangeMap hrm2;
            // Each range map should have single ID
            hrm1.AddLocation(loc1);
            _ASSERT(hrm1.GetMap().size() == 1);
            hrm2.AddLocation(loc2);
            _ASSERT(hrm2.GetMap().size() == 1);
            if (hrm1.begin()->first != hrm2.begin()->first) {
                // Different IDs
                return -1;
            }
            Int8 diff_plus = -1;
            Int8 diff_minus = -1;
            {{
                // Plus strand
                CRange<TSeqPos> rg1 = hrm1.begin()->second.
                    GetOverlappingRange(CHandleRange::eStrandPlus);
                CRange<TSeqPos> rg2 = hrm2.begin()->second.
                    GetOverlappingRange(CHandleRange::eStrandPlus);
                if ( rg1.GetTo() >= rg2.GetFrom()  &&
                    rg1.GetFrom() <= rg2.GetTo() ) {
                    diff_plus = AbsInt8(rg2.GetFrom() - rg1.GetFrom()) +
                        AbsInt8(rg1.GetTo() - rg2.GetTo());
                }
            }}
            {{
                // Minus strand
                CRange<TSeqPos> rg1 = hrm1.begin()->second.
                    GetOverlappingRange(CHandleRange::eStrandMinus);
                CRange<TSeqPos> rg2 = hrm2.begin()->second.
                    GetOverlappingRange(CHandleRange::eStrandMinus);
                if ( rg1.GetTo() >= rg2.GetFrom()  &&
                    rg1.GetFrom() <= rg2.GetTo() ) {
                    diff_minus = AbsInt8(rg2.GetFrom() - rg1.GetFrom()) +
                        AbsInt8(rg1.GetTo() - rg2.GetTo());
                }
            }}
            if (diff_plus < 0  &&  diff_minus < 0) {
                return -1;
            }
            return (diff_plus < 0 ? 0 : diff_plus) +
                (diff_minus < 0 ? 0 : diff_minus);
        }
    case eOverlap_Contained:
    case eOverlap_Contains:
        {
            return x_TestForOverlap_MultiSeq(loc1, loc2, type);
        }
    case eOverlap_CheckIntervals:
    default:
        {
            // For this types the function should not be called
            NCBI_THROW(CObjmgrUtilException, eNotImplemented,
                "TestForOverlap() -- error processing multi-strand seq-loc");
        }
    }
    return -1;
}


int TestForOverlap(const CSeq_loc& loc1,
                   const CSeq_loc& loc2,
                   EOverlapType type,
                   TSeqPos circular_len,
                   CScope* scope)
{
    Int8 ret = x_TestForOverlap(loc1, loc2, type, circular_len, scope);
    return ret <= kMax_Int ? int(ret) : kMax_Int;
}


Int8 x_TestForOverlap(const CSeq_loc& loc1,
                      const CSeq_loc& loc2,
                      EOverlapType type,
                      TSeqPos circular_len,
                      CScope* scope)
{
    typedef CRange<Int8> TRange8;
    CRange<TSeqPos> int_rg1, int_rg2;
    TRange8 rg1, rg2;
    bool multi_seq = false;
    try {
        int_rg1 = loc1.GetTotalRange();
        int_rg2 = loc2.GetTotalRange();
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
    if ( scope && loc1.IsWhole() ) {
        CBioseq_Handle h1 = scope->GetBioseqHandle(loc1.GetWhole());
        if ( h1 ) {
            int_rg1.Set(0, h1.GetBioseqLength() - 1);
        }
    }
    if ( scope && loc2.IsWhole() ) {
        CBioseq_Handle h2 = scope->GetBioseqHandle(loc2.GetWhole());
        if ( h2 ) {
            int_rg2.Set(0, h2.GetBioseqLength() - 1);
        }
    }
    rg1.Set(int_rg1.GetFrom(), int_rg1.GetTo());
    rg2.Set(int_rg2.GetFrom(), int_rg2.GetTo());

    ENa_strand strand1 = GetStrand(loc1);
    ENa_strand strand2 = GetStrand(loc2);
    if ( !TestForStrands(strand1, strand2) ) {
        // Subset and CheckIntervals don't use total ranges
        if ( type != eOverlap_Subset   &&  type != eOverlap_CheckIntervals ) {
            if ( strand1 == eNa_strand_other  ||
                strand2 == eNa_strand_other ) {
                return x_TestForOverlap_MultiStrand(loc1, loc2, type, scope);
            }
            return -1;
        }
    }
    switch (type) {
    case eOverlap_Simple:
        {
            if (circular_len != kInvalidSeqPos) {
                Int8 from1 = loc1.GetStart();
                Int8 from2 = loc2.GetStart();
                Int8 to1 = loc1.GetEnd();
                Int8 to2 = loc2.GetEnd();
                if (from1 > to1) {
                    if (from2 > to2) {
                        // Both locations are circular and must intersect at 0
                        return AbsInt8(from2 - from1) + AbsInt8(to1 - to2);
                    }
                    else {
                        // Only the first location is circular, rg2 may be used
                        // for the second one.
                        Int8 loc_len =
                            rg2.GetLength() +
                            Int8(loc1.GetCircularLength(circular_len));
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
                    Int8 loc_len =
                        rg1.GetLength() +
                        Int8(loc2.GetCircularLength(circular_len));
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
                return AbsInt8(rg2.GetFrom() - rg1.GetFrom()) +
                    AbsInt8(rg1.GetTo() - rg2.GetTo());
            }
            return -1;
        }
    case eOverlap_Contained:
        {
            if (circular_len != kInvalidSeqPos) {
                Int8 from1 = loc1.GetStart();
                Int8 from2 = loc2.GetStart();
                Int8 to1 = loc1.GetEnd();
                Int8 to2 = loc2.GetEnd();
                if (from1 > to1) {
                    if (from2 > to2) {
                        return (from1 <= from2  &&  to1 >= to2) ?
                            (from2 - from1) - (to1 - to2) : -1;
                    }
                    else {
                        if (rg2.GetFrom() >= from1  ||  rg2.GetTo() <= to1) {
                            return Int8(loc1.GetCircularLength(circular_len)) -
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
                Int8 from1 = loc1.GetStart();
                Int8 from2 = loc2.GetStart();
                Int8 to1 = loc1.GetEnd();
                Int8 to2 = loc2.GetEnd();
                if (from1 > to1) {
                    if (from2 > to2) {
                        return (from2 <= from1  &&  to2 >= to1) ?
                            (from1 - from2) + (to2 - to1) : -1;
                    }
                    else {
                        // Non-circular location can not contain a circular one
                        return -1;
                    }
                }
                else if (from2 > to2) {
                    if (rg1.GetFrom() >= from2  ||  rg1.GetTo() <= to2) {
                        return Int8(loc2.GetCircularLength(circular_len)) -
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
            if ( Compare(loc1, loc2, scope) != eContains ) {
                return -1;
            }
            return Int8(GetLength(loc1, scope)) - Int8(GetLength(loc2, scope));
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
                        return Int8(GetLength(loc1, scope)) -
                            Int8(GetLength(loc2, scope));
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
                        return Int8(GetLength(loc1, scope)) -
                            Int8(GetLength(loc2, scope));
                    }
                }
            }
            return -1;
        }
    case eOverlap_Interval:
        {
            return (Compare(loc1, loc2, scope) == eNoOverlap) ? -1
                : AbsInt8(rg2.GetFrom() - rg1.GetFrom()) +
                AbsInt8(rg1.GetTo() - rg2.GetTo());
        }
    }
    return -1;
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

/*
* ===========================================================================
* $Log$
* Revision 1.16  2005/02/02 19:49:55  grichenk
* Fixed more warnings
*
* Revision 1.15  2004/12/16 19:26:39  shomrat
* Fixed difference calculation for circular bioseqs
*
* Revision 1.14  2004/12/13 12:56:31  shomrat
* x_TestForOverlap_MultiStrand doesn't handle eOverlap_CheckIntervals
*
* Revision 1.13  2004/12/10 16:53:23  shomrat
* Restore previous semantics for IsSameBioseq()
*
* Revision 1.12  2004/12/07 15:20:47  ucko
* Restore GetLength adjustment from R1.10.
*
* Revision 1.11  2004/12/06 14:55:15  shomrat
* Added GetIdHandle and IsSameBioseq for CSeq_id_Handles
*
* Revision 1.10  2004/12/01 16:20:29  grichenk
* Do not catch exceptions in GetLength()
*
* Revision 1.9  2004/12/01 14:29:52  grichenk
* Removed old SeqLocMerge
*
* Revision 1.8  2004/11/24 15:32:06  shomrat
* Skip Null and Empty locations when calculating strand for mixed location
*
* Revision 1.7  2004/11/22 19:56:57  shomrat
* Bug fix (s_GetStrand for mix location)
*
* Revision 1.6  2004/11/22 16:10:30  dicuccio
* Optimized sequence::GetId(const CSeq_loc&) and sequence::GetStrand(const
* CSeq_loc&)
*
* Revision 1.5  2004/11/18 21:27:40  grichenk
* Removed default value for scope argument in seq-loc related functions.
*
* Revision 1.4  2004/11/18 15:56:51  grichenk
* Added Doxigen comments, removed THROWS.
*
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
