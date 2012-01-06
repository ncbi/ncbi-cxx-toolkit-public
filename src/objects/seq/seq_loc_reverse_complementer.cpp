/* $Id$
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
 * Author:  Clifford Clausen, Aaron Ucko, Aleksey Grichenko, Michael Kornbluh
 *
 * File Description:
 *   Get reverse complement of a CSeq_loc.
 *
 * ===========================================================================
 */
#include <ncbi_pch.hpp>

#include <objects/seq/seq_loc_reverse_complementer.hpp>

#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seqloc/Seq_bond.hpp>

/////////////////////////////////////////////////////////////////////
//
//  Implementation of GetReverseComplement()
//

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

static ENa_strand s_GetPackedPntStrand(const CSeq_loc & loc )
{
    _ASSERT( loc.IsPacked_pnt() );
    return ( loc.GetPacked_pnt().IsSetStrand() ?
        loc.GetPacked_pnt().GetStrand() :
        eNa_strand_unknown );
}

static CSeq_interval* s_SeqIntRevCmp(const CSeq_interval& i)
{
    auto_ptr<CSeq_interval> rev_int(new CSeq_interval);
    rev_int->Assign(i);
    
    ENa_strand s = i.CanGetStrand() ? i.GetStrand() : eNa_strand_unknown;
    rev_int->SetStrand(Reverse(s));

    return rev_int.release();
}


static CSeq_point* s_SeqPntRevCmp(const CSeq_point& pnt)
{
    auto_ptr<CSeq_point> rev_pnt(new CSeq_point);
    rev_pnt->Assign(pnt);
    
    ENa_strand s = pnt.CanGetStrand() ? pnt.GetStrand() : eNa_strand_unknown;
    rev_pnt->SetStrand(Reverse(s));

    return rev_pnt.release();
}

CSeq_loc* 
GetReverseComplement(const CSeq_loc& loc, CReverseComplementHelper* helper)
{
    _ASSERT( helper != NULL ); 

    auto_ptr<CSeq_loc> rev_loc( new CSeq_loc );

    switch ( loc.Which() ) {

    // -- reverse is the same.
    case CSeq_loc::e_Null:
    case CSeq_loc::e_Empty:
    case CSeq_loc::e_Whole:
        rev_loc->Assign(loc);
        break;

    // -- just reverse the strand
    case CSeq_loc::e_Int:
        rev_loc->SetInt(*s_SeqIntRevCmp(loc.GetInt()));
        break;
    case CSeq_loc::e_Pnt:
        rev_loc->SetPnt(*s_SeqPntRevCmp(loc.GetPnt()));
        break;
    case CSeq_loc::e_Packed_pnt:
        rev_loc->SetPacked_pnt().Assign(loc.GetPacked_pnt());
        rev_loc->SetPacked_pnt().SetStrand(Reverse( s_GetPackedPntStrand(loc) ));
        break;

    // -- possibly more than one sequence
    case CSeq_loc::e_Packed_int:
    {
        // reverse each interval and store them in reverse order
        typedef CRef< CSeq_interval > TInt;
        CPacked_seqint& pint = rev_loc->SetPacked_int();
        ITERATE (CPacked_seqint::Tdata, it, loc.GetPacked_int().Get()) {
            pint.Set().push_front(TInt(s_SeqIntRevCmp(**it)));
        }
        break;
    }
    case CSeq_loc::e_Mix:
    {
        // reverse each location and store them in reverse order
        typedef CRef< CSeq_loc > TLoc;
        CSeq_loc_mix& mix = rev_loc->SetMix();
        ITERATE (CSeq_loc_mix::Tdata, it, loc.GetMix().Get()) {
            mix.Set().push_front(TLoc(GetReverseComplement(**it, helper)));
        }
        break;
    }
    case CSeq_loc::e_Equiv:
    {
        // reverse each location (no need to reverse order)
        typedef CRef< CSeq_loc > TLoc;
        CSeq_loc_equiv& e = rev_loc->SetEquiv();
        ITERATE (CSeq_loc_equiv::Tdata, it, loc.GetEquiv().Get()) {
            e.Set().push_back(TLoc(GetReverseComplement(**it, helper)));
        }
        break;
    }

    case CSeq_loc::e_Bond:
    {
        CSeq_bond& bond = rev_loc->SetBond();
        bond.SetA(*s_SeqPntRevCmp(loc.GetBond().GetA()));
        if ( loc.GetBond().CanGetB() ) {
            bond.SetA(*s_SeqPntRevCmp(loc.GetBond().GetB()));
        }
    }
        
    // -- not supported
    case CSeq_loc::e_Feat:
    default:
        NCBI_THROW(CException, eUnknown,
            "CSeq_loc::GetReverseComplement -- unsupported location type");
    }

    return rev_loc.release();
}

END_SCOPE(objects)
END_NCBI_SCOPE
