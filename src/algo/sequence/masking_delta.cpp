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
 * Authors:  Eugene Vasilchenko
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <algo/sequence/masking_delta.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqloc/seqloc__.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

namespace {
    struct ByFrom
    {
        bool operator()(const CRange<TSeqPos>& a,
                        const CRange<TSeqPos>& b) const {
            return a.GetFrom() < b.GetFrom() ||
                (a.GetFrom() == b.GetFrom() && a.GetToOpen() < b.GetToOpen());
        }
    };

    template<typename Iter, typename Less>
    bool s_is_sorted(Iter begin, Iter end, Less less)
    {
        if ( begin == end ) {
            return true;
        }
        for ( Iter next = begin; ++next != end; begin = next ) {
            if ( less(*next, *begin) ) {
                return false;
            }
        }
        return true;
    }
}

CRef<CBioseq> MakeMaskingBioseq(const CSeq_id& new_id,
                                TSeqPos seq_length,
                                const CSeq_id& original_id,
                                const vector<CRange<TSeqPos> >& mask_ranges)
{
    if ( !s_is_sorted(mask_ranges.begin(), mask_ranges.end(), ByFrom()) ) {
        vector<CRange<TSeqPos> > ranges(mask_ranges);
        sort(ranges.begin(), ranges.end(), ByFrom());
        return MakeMaskingBioseq(new_id, seq_length, original_id, ranges);
    }
    _ASSERT(s_is_sorted(mask_ranges.begin(), mask_ranges.end(), ByFrom()));

    CRef<CBioseq> seq(new CBioseq);
    CRef<CSeq_id> seq_id(new CSeq_id);
    seq->SetId().push_back(seq_id);
    seq_id->Assign(new_id);
    CSeq_inst& inst = seq->SetInst();
    inst.SetRepr(inst.eRepr_delta);
    inst.SetMol(inst.eMol_not_set);
    inst.SetLength(seq_length);
    CDelta_ext& delta = inst.SetExt().SetDelta();
    TSeqPos cur_pos = 0, last_pos = 0;
    vector<CRange<TSeqPos> >::const_iterator mask_it = mask_ranges.begin();
    while ( cur_pos < seq_length && mask_it != mask_ranges.end() ) {
        if ( !mask_it->Empty() ) {
            TSeqPos mask_start = mask_it->GetFrom();
            if ( mask_start <= cur_pos ) {
                TSeqPos mask_end = mask_it->GetToOpen();
                if ( mask_end > cur_pos ) {
                    cur_pos = mask_it->GetToOpen();
                }
            }
            else {
                if ( last_pos < cur_pos ) {
                    // add gap
                    CRef<CDelta_seq> seg(new CDelta_seq);
                    delta.Set().push_back(seg);
                    seg->SetLiteral().SetLength(cur_pos-last_pos);
                }
                CRef<CDelta_seq> seg(new CDelta_seq);
                delta.Set().push_back(seg);
                CSeq_interval& ref_int = seg->SetLoc().SetInt();
                ref_int.SetId().Assign(original_id);
                ref_int.SetFrom(cur_pos);
                if ( mask_start >= seq_length ) {
                    ref_int.SetTo(seq_length-1);
                    last_pos = cur_pos = seq_length;
                    break;
                }
                ref_int.SetTo(mask_start-1);
                last_pos = cur_pos = mask_start;
            }
        }
        ++mask_it;
    }
    if ( last_pos < cur_pos ) {
        // trailing gap
        CRef<CDelta_seq> seg(new CDelta_seq);
        delta.Set().push_back(seg);
        seg->SetLiteral().SetLength(cur_pos-last_pos);
    }
    return seq;
}

static
CRef<CBioseq> MakeMaskingBioseq(const CSeq_id& new_id,
                                TSeqPos seq_length,
                                CConstRef<CSeq_id> original_id,
                                const CSeq_loc& masking_loc)
{
    vector<CRange<TSeqPos> > mask_ranges;
    for ( CSeq_loc_CI it(masking_loc); it; ++it ) {
        if ( !original_id ) {
            original_id = &it.GetSeq_id();
        }
        else if ( !original_id->Equals(it.GetSeq_id()) ) {
            continue;
        }
        mask_ranges.push_back(it.GetRange());
    }
    return MakeMaskingBioseq(new_id, seq_length, *original_id, mask_ranges);
}


CRef<CBioseq> MakeMaskingBioseq(const CSeq_id& new_id,
                                TSeqPos seq_length,
                                const CSeq_id& original_id,
                                const CSeq_loc& masking_loc)
{
    return MakeMaskingBioseq(new_id, seq_length,
                             ConstRef(&original_id), masking_loc);
}


CRef<CBioseq> MakeMaskingBioseq(const CSeq_id& new_id,
                                TSeqPos seq_length,
                                const CSeq_loc& masking_loc)
{
    return MakeMaskingBioseq(new_id, seq_length, null, masking_loc);
}


END_NCBI_SCOPE
