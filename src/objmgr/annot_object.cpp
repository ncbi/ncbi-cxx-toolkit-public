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
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.5  2002/04/05 21:26:19  grichenk
* Enabled iteration over annotations defined on segments of a
* delta-sequence.
*
* Revision 1.4  2002/02/21 19:27:04  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.3  2002/01/23 21:59:31  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.2  2002/01/16 16:25:56  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:16  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/

#include "annot_object.hpp"
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqalign/Dense_diag.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqalign/Packed_seg.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <serial/iterator.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//
//  CAnnotObject::
//


void CAnnotObject::x_ProcessAlign(const CSeq_align& align)
{
    //### Check the implementation.
    switch ( align.GetSegs().Which() ) {
    case CSeq_align::C_Segs::e_Dendiag:
        {
            const CSeq_align::C_Segs::TDendiag& dendiag =
                align.GetSegs().GetDendiag();
            iterate ( CSeq_align::C_Segs::TDendiag, it, dendiag ) {
                CDense_diag::TIds::const_iterator it_id =
                    (*it)->GetIds().begin();
                CDense_diag::TStarts::const_iterator it_start =
                    (*it)->GetStarts().begin();
                int len = (*it)->GetLen();
                CDense_diag::TStrands::const_iterator it_strand;
                if ( (*it)->IsSetStrands() ) {
                    it_strand = (*it)->GetStrands().begin();
                }
                while (it_id != (*it)->GetIds().end()) {
                    // Create CHandleRange from an align element
                    CSeq_loc* loc = new CSeq_loc;
                    SerialAssign<CSeq_id>(loc->SetInt().SetId(), **it_id);
                    loc->GetInt().SetFrom(*it_start);
                    loc->GetInt().SetTo(*it_start + len);
                    if ( (*it)->IsSetStrands() ) {
                        loc->GetInt().SetStrand(*it_strand);
                        ++it_strand;
                    }
                    m_RangeMap->AddLocation(*loc);
                    ++it_id;
                    ++it_start;
                }
            }
            break;
        }
    case CSeq_align::C_Segs::e_Denseg:
        {
            const CSeq_align::C_Segs::TDenseg& denseg =
                align.GetSegs().GetDenseg();
            CDense_seg::TStarts::const_iterator it_start =
                denseg.GetStarts().begin();
            CDense_seg::TLens::const_iterator it_len =
                denseg.GetLens().begin();
            CDense_seg::TStrands::const_iterator it_strand;
            if ( denseg.IsSetStrands() ) {
                it_strand = denseg.GetStrands().begin();
            }
            for (int seg = 0; seg < denseg.GetNumseg(); seg++, ++it_len) {
                CDense_seg::TIds::const_iterator it_id =
                    denseg.GetIds().begin();
                for (int seq = 0; seq < denseg.GetDim(); seq++,
                    ++it_start, ++it_id) {
                    if ( *it_start < 0 )
                        continue;
                    CSeq_loc* loc = new CSeq_loc;
                    SerialAssign<CSeq_id>(loc->SetInt().SetId(), **it_id);
                    loc->GetInt().SetFrom(*it_start);
                    loc->GetInt().SetTo(*it_start + *it_len);
                    if ( denseg.IsSetStrands() ) {
                        loc->GetInt().SetStrand(*it_strand);
                        ++it_strand;
                    }
                    m_RangeMap->AddLocation(*loc);
                }
            }
            break;
        }
    case CSeq_align::C_Segs::e_Std:
        {
            const CSeq_align::C_Segs::TStd& std =
                align.GetSegs().GetStd();
            iterate ( CSeq_align::C_Segs::TStd, it, std ) {
                //### Ignore Seq-id, assuming it is also set in Seq-loc?
                iterate ( CStd_seg::TLoc, it_loc, (*it)->GetLoc() ) {
                    // Create CHandleRange from an align element
                    m_RangeMap->AddLocation(**it_loc);
                }
            }
            break;
        }
    case CSeq_align::C_Segs::e_Packed:
        {
            const CSeq_align::C_Segs::TPacked& packed =
                align.GetSegs().GetPacked();
            CPacked_seg::TStarts::const_iterator it_start =
                packed.GetStarts().begin();
            CPacked_seg::TLens::const_iterator it_len =
                packed.GetLens().begin();
            CPacked_seg::TPresent::const_iterator it_pres =
                packed.GetPresent().begin();
            CPacked_seg::TStrands::const_iterator it_strand;
            if ( packed.IsSetStrands() ) {
                it_strand = packed.GetStrands().begin();
            }
            for (int seg = 0; seg < packed.GetNumseg(); seg++, ++it_len) {
                CPacked_seg::TIds::const_iterator it_id =
                    packed.GetIds().begin();
                for (int seq = 0; seq < packed.GetDim(); seq++, ++it_pres) {
                    if ( *it_pres ) {
                        CSeq_loc* loc = new CSeq_loc;
                        SerialAssign<CSeq_id>(loc->SetInt().SetId(), **it_id);
                        loc->GetInt().SetFrom(*it_start);
                        loc->GetInt().SetTo(*it_start + *it_len);
                        if ( packed.IsSetStrands() ) {
                            loc->GetInt().SetStrand(*it_strand);
                            ++it_strand;
                        }
                        ++it_id;
                        ++it_start;
                        m_RangeMap->AddLocation(*loc);
                    }
                }
            }
            break;
        }
    case CSeq_align::C_Segs::e_Disc:
        {
            const CSeq_align::C_Segs::TDisc& disc =
                align.GetSegs().GetDisc();
            iterate ( CSeq_align_set::Tdata, it, disc.Get() ) {
                x_ProcessAlign(**it);
            }
            break;
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
