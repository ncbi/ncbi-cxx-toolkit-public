
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
* Author: Justin Foley, NCBI
*
* File Description:
*   Feature trimming code
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objmgr/mapped_feat.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objtools/edit/feature_edit.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

CMappedFeat CFeatTrim::Apply(const CMappedFeat& mapped_feat, 
        const CRange<TSeqPos>& range)
{
    auto from = range.GetFrom();
    auto to = range.GetTo();

    CSeq_feat_Handle sfh = mapped_feat.GetSeq_feat_Handle();
    CRef<CSeq_loc> loc(new CSeq_loc());
    loc->Assign(sfh.GetLocation());

    CSeq_loc_CI loc_it(*loc);

    bool partial_start = false;
    bool partial_stop = false;
    const auto strand = loc->GetStrand();


    for(CSeq_loc_CI loc_it(*loc); loc_it; ++loc_it) {

        const auto& current_range = loc_it.GetRange();
        const auto current_from = current_range.GetFrom();
        const auto current_to = current_range.GetTo();

        CRef<CSeq_id> current_seqid = Ref(new CSeq_id());
        current_seqid->Assign(loc_it.GetSeq_id());


        // May be able to do this more succinctly and efficiently using CSeq_loc::Intersect
        if ((current_to < from) ||
            (current_from > to)) {
            CRef<CSeq_loc> trim(new CSeq_loc(*current_seqid,
                current_from,
                current_to,
                strand));

            loc = loc->Subtract(*trim, 0, NULL, NULL);
            if (current_to < from) {
                partial_start = true;
            }
            if (current_from > to) {
                partial_stop = true;
            }
            continue;
        }

        if (current_from < from) {
            CRef<CSeq_loc> trim(new CSeq_loc(*current_seqid,
                current_from,
                from-1,
                strand));

            loc = loc->Subtract(*trim, 0, NULL, NULL);
            partial_start = true;
        }

        if (current_to > to) {
            CRef<CSeq_loc> trim(new CSeq_loc(*current_seqid,
                to+1,
                current_to,
                strand));

            loc = loc->Subtract(*trim, 0, NULL, NULL);     
            partial_stop = true; 
        }
    }

    if (strand == eNa_strand_minus) {
        swap(partial_start, partial_stop);
    }


    if (partial_start) {
        loc->SetPartialStart(true, eExtreme_Biological);
    }

    if (partial_stop) {
        loc->SetPartialStop(true, eExtreme_Biological);
    }


    // Create a new seq-feat with the trimmed location
    CSeq_feat_EditHandle sfeh(sfh);
    CRef<CSeq_feat> new_sf(new CSeq_feat());
    new_sf->Assign(*mapped_feat.GetSeq_feat());
    new_sf->SetLocation(*loc);
    if (partial_start || partial_stop) {
        new_sf->SetPartial(true);
    }


    // If Cdregion need to consider changes in frameshift
    if (new_sf->GetData().IsCdregion()) {
        CRange<TSeqPos> feat_range = mapped_feat.GetLocationTotalRange();
        // Calculate the difference between the range and feature start position
        TSeqPos offset = 0;
        if (strand != eNa_strand_minus) {
            TSeqPos feat_from = feat_range.GetFrom();
            if (feat_from < from) {
                offset = from - feat_from;
            }
        } 
        else { // eNa_strand_minus
            TSeqPos feat_to = feat_range.GetTo();
            if (feat_to > to) {
                offset = feat_to - to;
            }
        }

        x_UpdateFrame(offset, new_sf->SetData().SetCdregion());
/*
        const TSeqPos frame_change = offset%3;
        if (frame_change) {
            const TSeqPos old_frame = s_GetFrame(new_sf->GetData().GetCdregion());
            const TSeqPos new_frame = (old_frame + 3 - frame_change)%3;

            CCdregion::EFrame frame = CCdregion::eFrame_one;
            switch(new_frame) {
                case 1:
                    frame = CCdregion::eFrame_two;
                    break;
                case 2:
                    frame = CCdregion::eFrame_three;
                    break;
                default:
                    break;
            }
            new_sf->SetData().SetCdregion().ResetFrame();
            new_sf->SetData().SetCdregion().SetFrame(frame);
        }
        */
    }

    sfeh.Replace(*new_sf);
    return CMappedFeat(sfh);
}


TSeqPos CFeatTrim::x_GetFrame(const CCdregion& cds)
{
    switch(cds.GetFrame()) {
    case CCdregion::eFrame_not_set:
    case CCdregion::eFrame_one:
        return 0;
    case CCdregion::eFrame_two:
        return 1;
    case CCdregion::eFrame_three:
        return 2;
    default:
        return 0;

    }
    return 0;
}


void CFeatTrim::x_UpdateFrame(const TSeqPos offset, CCdregion& cdregion)
{
    const TSeqPos frame_change = offset%3;
    if (!frame_change) {
        return;
    }

    const TSeqPos old_frame = x_GetFrame(cdregion);
    const TSeqPos new_frame = (old_frame + 3 - frame_change)%3;

    CCdregion::EFrame frame = CCdregion::eFrame_one;
    switch(new_frame) {
        case 1:
            frame = CCdregion::eFrame_two;
            break;
        case 2:
            frame = CCdregion::eFrame_three;
            break;
        default:
            break;
    }
    cdregion.ResetFrame();
    cdregion.SetFrame(frame);
}


END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE
