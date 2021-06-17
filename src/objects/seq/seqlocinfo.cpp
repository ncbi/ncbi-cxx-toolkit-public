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
* Author:  Christiam Camacho, Vahram Avagyan
*
*/

#include <ncbi_pch.hpp>

#include <objects/seq/seqlocinfo.hpp>
#include <objects/seqloc/Packed_seqint.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

objects::ENa_strand CSeqLocInfo::GetStrand() const
{
    objects::ENa_strand retval;
    switch (GetFrame()) {
    case eFramePlus1:
    case eFramePlus2:
    case eFramePlus3:
        retval = objects::eNa_strand_plus;
        break;
    case eFrameMinus1:
    case eFrameMinus2:
    case eFrameMinus3:
        retval = objects::eNa_strand_minus;
        break;
    case eFrameNotSet:
        retval = objects::eNa_strand_unknown;
        break;
    default:
        abort();
    }
    return retval;
}

bool CSeqLocInfo::operator==(const CSeqLocInfo& rhs) const
{
    if (this != &rhs) {
        if (GetFrame() != rhs.GetFrame()) {
            return false;
        }

        if ( !GetSeqId().Match(rhs.GetSeqId()) ) {
            return false;
        }

        TSeqRange me = (TSeqRange)*this;
        TSeqRange you = (TSeqRange) rhs;
        if (me != you) {
            return false;
        }
    }
    return true;
}

void CSeqLocInfo::SetFrame(int frame)
{
    if (frame < -3 || frame > 3) {
        string msg = 
            "CSeqLocInfo::SetFrame: input " + NStr::IntToString(frame) + 
            " out of range";
        throw std::out_of_range(msg);
    }
    m_Frame = (ETranslationFrame) frame;
}

//
// TMaskedQueryRegions
//

TMaskedQueryRegions
TMaskedQueryRegions::RestrictToSeqInt(const objects::CSeq_interval& location) const
{
    TMaskedQueryRegions retval;

    TSeqRange loc(location.GetFrom(), 0);
    loc.SetToOpen(location.GetTo());

    ITERATE(TMaskedQueryRegions, maskinfo, *this) {
        const CSeq_interval& intv = (*maskinfo)->GetInterval();
    	TSeqRange mask (intv.GetFrom(), intv.GetTo());
        TSeqRange range = loc.IntersectionWith(mask);
        if (range.NotEmpty()) {
            const ENa_strand kStrand = intv.CanGetStrand() 
                ? intv.GetStrand() : eNa_strand_unknown;
            CRef<CSeq_interval> si
                (new CSeq_interval(const_cast<CSeq_id&>(intv.GetId()), 
                                   range.GetFrom(), 
                                   range.GetToOpen(), 
                                   kStrand));
            CRef<CSeqLocInfo> sli(new CSeqLocInfo(si, 
                                                  (*maskinfo)->GetFrame()));
            retval.push_back(sli);
        }
    }

    return retval;
}

CRef<objects::CPacked_seqint> 
TMaskedQueryRegions::ConvertToCPacked_seqint() const
{
    CRef<CPacked_seqint> retval(new CPacked_seqint);

    ITERATE(TMaskedQueryRegions, mask, *this) {
        // this is done because the CSeqLocInfo doesn't guarantee that the
        // strand and the frame are consistent, so we don't call
        // CPacked_seqint::AddInterval(const CSeq_interval& itv);
        retval->AddInterval((*mask)->GetSeqId(),
                            (*mask)->GetInterval().GetFrom(),
                            (*mask)->GetInterval().GetTo(),
                            (*mask)->GetStrand());
    }
    if (retval->CanGet() && !retval->Get().empty()) {
        return retval;
    }
    retval.Reset();
    return retval;
}

bool 
TMaskedQueryRegions::HasNegativeStrandMasks() const
{
    ITERATE(TMaskedQueryRegions, mask, *this) {
        if ((*mask)->GetStrand() == eNa_strand_minus) {
            return true;
        }
    }
    return false;
}

END_NCBI_SCOPE
