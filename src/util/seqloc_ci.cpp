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
* Author: Aleksey Grichenko
*
* File Description:
*   Seq-loc ranges iterator
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2002/04/16 20:01:18  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#include <util/seqloc_ci.hpp>
#include <objects/seqloc/seqloc__.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CSeq_loc_CI::CSeq_loc_CI(void)
    : m_Location(0)
{
    m_CurLoc = m_LocList.end();
}


CSeq_loc_CI::CSeq_loc_CI(const CSeq_loc& loc)
    : m_Location(&loc)
{
    x_ProcessLocation(loc);
    m_CurLoc = m_LocList.begin();
}


CSeq_loc_CI::~CSeq_loc_CI(void)
{
    return;
}


CSeq_loc_CI::CSeq_loc_CI(const CSeq_loc_CI& iter)
{
    *this = iter;
}


CSeq_loc_CI& CSeq_loc_CI::operator= (const CSeq_loc_CI& iter)
{
    if (this == &iter)
        return *this;
    m_LocList.clear();
    m_Location = iter.m_Location;
    iterate(TLocList, li, iter.m_LocList) {
        TLocList::iterator tmp = m_LocList.insert(m_LocList.end(), *li);
        if (iter.m_CurLoc == li)
            m_CurLoc = tmp;
    }
    return *this;
}


void CSeq_loc_CI::x_ProcessLocation(const CSeq_loc& loc)
{
    switch ( loc.Which() ) {
    case CSeq_loc::e_not_set:
    case CSeq_loc::e_Null:
    case CSeq_loc::e_Empty:
        {
            // Ignore empty locations
            return;
        }
    case CSeq_loc::e_Whole:
        {
            SLoc_Info info;
            info.m_Id = &loc.GetWhole();
            info.m_Range.SetFrom(TRange::GetWholeFrom());
            info.m_Range.SetTo(TRange::GetWholeTo());
            m_LocList.push_back(info);
            return;
        }
    case CSeq_loc::e_Int:
        {
            SLoc_Info info;
            info.m_Id = &loc.GetInt().GetId();
            info.m_Range.SetFrom(loc.GetInt().GetFrom());
            info.m_Range.SetTo(loc.GetInt().GetTo());
            if ( loc.GetInt().IsSetStrand() )
                info.m_Strand = loc.GetInt().GetStrand();
            m_LocList.push_back(info);
            return;
        }
    case CSeq_loc::e_Pnt:
        {
            SLoc_Info info;
            info.m_Id = &loc.GetPnt().GetId();
            info.m_Range.SetFrom(loc.GetPnt().GetPoint());
            info.m_Range.SetTo(loc.GetPnt().GetPoint());
            if ( loc.GetPnt().IsSetStrand() )
                info.m_Strand = loc.GetPnt().GetStrand();
            m_LocList.push_back(info);
            return;
        }
    case CSeq_loc::e_Packed_int:
        {
            iterate ( CPacked_seqint::Tdata, ii, loc.GetPacked_int().Get() ) {
                SLoc_Info info;
                info.m_Id = &(*ii)->GetId();
                info.m_Range.SetFrom((*ii)->GetFrom());
                info.m_Range.SetTo((*ii)->GetTo());
                if ( (*ii)->IsSetStrand() )
                    info.m_Strand = (*ii)->GetStrand();
                m_LocList.push_back(info);
            }
            return;
        }
    case CSeq_loc::e_Packed_pnt:
        {
            iterate ( CPacked_seqpnt::TPoints, pi, loc.GetPacked_pnt().GetPoints() ) {
                SLoc_Info info;
                info.m_Id = &loc.GetPacked_pnt().GetId();
                info.m_Range.SetFrom(*pi);
                info.m_Range.SetTo(*pi);
                if ( loc.GetPacked_pnt().IsSetStrand() )
                    info.m_Strand = loc.GetPacked_pnt().GetStrand();
                m_LocList.push_back(info);
            }
            return;
        }
    case CSeq_loc::e_Mix:
        {
            iterate(CSeq_loc_mix::Tdata, li, loc.GetMix().Get()) {
                x_ProcessLocation(**li);
            }
            return;
        }
    case CSeq_loc::e_Equiv:
        {
            iterate(CSeq_loc_equiv::Tdata, li, loc.GetEquiv().Get()) {
                x_ProcessLocation(**li);
            }
            return;
        }
    case CSeq_loc::e_Bond:
        {
            SLoc_Info infoA;
            infoA.m_Id = &loc.GetBond().GetA().GetId();
            infoA.m_Range.SetFrom(loc.GetBond().GetA().GetPoint());
            infoA.m_Range.SetTo(loc.GetBond().GetA().GetPoint());
            if ( loc.GetBond().GetA().IsSetStrand() )
                infoA.m_Strand = loc.GetBond().GetA().GetStrand();
            m_LocList.push_back(infoA);
            if ( loc.GetBond().IsSetB() ) {
                SLoc_Info infoB;
                infoB.m_Id = &loc.GetBond().GetB().GetId();
                infoB.m_Range.SetFrom(loc.GetBond().GetB().GetPoint());
                infoB.m_Range.SetTo(loc.GetBond().GetB().GetPoint());
                if ( loc.GetBond().GetB().IsSetStrand() )
                    infoB.m_Strand = loc.GetBond().GetB().GetStrand();
                m_LocList.push_back(infoB);
            }
            return;
        }
    case CSeq_loc::e_Feat:
    default:
        {
            throw runtime_error
                ("CAnnotTypes_CI -- unsupported location type");
        }
    }
}



END_SCOPE(objects)
END_NCBI_SCOPE
