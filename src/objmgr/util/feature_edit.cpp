
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
#include <objmgr/util/feature_edit.hpp>

#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seqfeat/RNA_ref.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(sequence)


struct SOutsideRange
{
    SOutsideRange(TSeqPos from, TSeqPos to) 
        : m_From(from), m_To(to) {}

    bool operator()(const CRef<CCode_break>& code_break) {
        CRange<TSeqPos> cb_range = code_break->GetLoc().GetTotalRange();
        const auto strand = code_break->GetLoc().GetStrand();
        if (strand != eNa_strand_minus) {
            const TSeqPos cb_from = cb_range.GetFrom();
            return ((cb_from < m_From) || (m_To < cb_from)); 
        } 
        // eNa_strand_minus
        const TSeqPos cb_to = cb_range.GetTo();
        return ((cb_to > m_To) || (m_From > cb_to)); 
    }

    TSeqPos m_From;
    TSeqPos m_To;  
};


CRef<CSeq_loc> CFeatTrim::Apply(const CSeq_loc& loc, 
    const CRange<TSeqPos>& range)
{
    const bool set_partial = true;
    const TSeqPos from = range.GetFrom();
    const TSeqPos to = range.GetTo();

    CRef<CSeq_loc> trimmed_loc(new CSeq_loc());
    trimmed_loc->Assign(loc);

    x_TrimLocation(from, to, set_partial, trimmed_loc);

    return trimmed_loc;
}


CRef<CSeq_feat> CFeatTrim::Apply(const CSeq_feat& feat,
    const CRange<TSeqPos>& range)
{
    CRef<CSeq_loc> loc = Ref(new CSeq_loc());
    loc->Assign(feat.GetLocation());

    const TSeqPos from = range.GetFrom();
    const TSeqPos to = range.GetTo();

    const bool set_partial = true;

    x_TrimLocation(from, to, set_partial, loc);
    if (loc->IsNull()) {
        return Ref(new CSeq_feat());
    }

    // Create a new seq-feat with the trimmed location
    CRef<CSeq_feat> new_sf(new CSeq_feat());
    new_sf->Assign(feat);
    new_sf->SetLocation(*loc);
    if (!loc->IsNull() &&
        (loc->IsPartialStart(eExtreme_Biological) || 
        loc->IsPartialStop(eExtreme_Biological))) {
        new_sf->SetPartial(true);
    }


    // If Cdregion need to consider changes in frameshift
    if (new_sf->GetData().IsCdregion()) {
        const TSeqPos offset = x_GetStartOffset(feat, from, to);
        x_UpdateFrame(offset, new_sf->SetData().SetCdregion());

        if (new_sf->SetData().SetCdregion().IsSetCode_break()) {
            // iterate over code breaks and remove if they fall outside the range
            list<CRef<CCode_break>>& code_breaks = new_sf->SetData().SetCdregion().SetCode_break();
            code_breaks.remove_if(SOutsideRange(from,to));
            if (code_breaks.empty()) {
                new_sf->SetData().SetCdregion().ResetCode_break();
            }
            else {
                const auto strand = loc->GetStrand();
                // Trim the 3' end
                if (strand != eNa_strand_minus) {
                    for (auto code_break : code_breaks) {
                        const TSeqPos cb_to = code_break->GetLoc().GetTotalRange().GetTo();
                        if (cb_to > to) {
                            x_TrimCodeBreak(from, to, *code_break);
                        }
                    }
                }
                else { // strand == eNa_strand_minus
                    for (auto code_break : code_breaks) {
                        const TSeqPos cb_from = code_break->GetLoc().GetTotalRange().GetFrom();
                        if (cb_from < from) {
                            x_TrimCodeBreak(from, to, *code_break);
                        }
                    }
                } 
            }
        }
    }
    else 
    if (new_sf->GetData().GetSubtype() == CSeqFeatData::eSubtype_tRNA) {
        auto& rna = new_sf->SetData().SetRna();
        if (rna.IsSetExt() && rna.GetExt().IsTRNA()) {
            x_TrimTrnaExt(from, to, rna.SetExt().SetTRNA());
        }
    }
    return new_sf;
}


void CFeatTrim::x_TrimCodeBreak(const TSeqPos from, const TSeqPos to,
    CCode_break& code_break)
{
    const bool not_partial = false;
    CRef<CSeq_loc> cb_loc(new CSeq_loc());
    cb_loc->Assign(code_break.GetLoc());
    x_TrimLocation(from, to, not_partial, cb_loc);
    code_break.ResetLoc();
    code_break.SetLoc(*cb_loc);
}


void CFeatTrim::x_TrimLocation(const TSeqPos from, const TSeqPos to,
    const bool set_partial,
    CRef<CSeq_loc>& loc) 
{
    if (loc.IsNull()) {
        return;
    }

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

            loc =  loc->Subtract(*trim, 0, NULL, NULL);     
            partial_stop = true; 
        }
    }

    if (loc->IsNull() || !set_partial) {
        return;
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
}


TSeqPos CFeatTrim::x_GetStartOffset(const CSeq_feat& feat,
    TSeqPos from, TSeqPos to) 
{
    TSeqPos offset = 0;
    const auto strand = feat.GetLocation().GetStrand();
    CRange<TSeqPos> feat_range = feat.GetLocation().GetTotalRange();

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
    return offset;
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
    const TSeqPos new_frame = (old_frame + frame_change)%3; 

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


void CFeatTrim::x_TrimTrnaExt(const TSeqPos from, const TSeqPos to, CTrna_ext& ext)
{
    if (!ext.IsSetAnticodon()) {
        return;
    }

    CRange<TSeqPos> ac_range = ext.GetAnticodon().GetTotalRange();

    const TSeqPos ac_from = ac_range.GetFrom();
    const TSeqPos ac_to   = ac_range.GetTo();
   
   if (from <= ac_from && to >= ac_to) {
        return;
   } 

    if (from > ac_to || to < ac_from) {   
        ext.ResetAnticodon();
        return;
    } 

    const bool set_partial=true;
    // else there is some overlap
    CRef<CSeq_loc> loc(new CSeq_loc());
    loc->Assign(ext.GetAnticodon());
    x_TrimLocation(from, to, set_partial, loc);
    ext.ResetAnticodon();
    ext.SetAnticodon(*loc);

    return;
}


END_SCOPE(sequence)
END_SCOPE(objects)
END_NCBI_SCOPE
