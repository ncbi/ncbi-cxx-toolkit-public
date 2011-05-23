/*
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
 * Author:  Michael Kornbluh
 *
 * File Description:
 *   stores feats for efficient retrieval in finding the best one
 *
 */

#include <ncbi_pch.hpp>

#include "best_feat_finder.hpp"

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqloc/Seq_interval.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

CBestFeatFinder::CBestFeatFinder(void)
{
    // nothing to do here
}

bool CBestFeatFinder::AddFeat( const CSeq_feat& new_feat )
{
    CConstRef<CSeq_feat> new_feat_ref( &new_feat );
    CConstRef<CSeq_loc> new_feat_loc_ref( &new_feat.GetLocation() );

    if( new_feat_ref && new_feat_loc_ref ) {
        loc_to_feat_map.insert( TLocToFeatMap::value_type( new_feat_loc_ref, new_feat_ref ) );
        return true;
    } else {
        return false;
    }
}

CConstRef<CSeq_feat> 
CBestFeatFinder::FindBestFeatForLoc( const CSeq_loc &sought_loc ) const
{
    // Try to find the smallest CDS that contains the given location
    // (we use extremes as an approximation)

    CConstRef<CSeq_loc> sought_loc_ref( &sought_loc );

    const int loc_start = sought_loc.GetStart(eExtreme_Positional);
    const int loc_stop  = sought_loc.GetStop(eExtreme_Positional);

    return FindBestFeatForLoc( loc_start, loc_stop );
}

CConstRef<CSeq_feat> 
CBestFeatFinder::FindBestFeatForLoc( const int loc_start, const int loc_stop ) const
{
    // something wrong with sought_loc
    if( loc_start < 0 || loc_stop < 0 ) {
        return CConstRef<CSeq_feat>();
    }

    const int loc_len = (loc_stop - loc_start + 1);

    CRef<CSeq_loc> sought_loc_ref( new CSeq_loc );
    sought_loc_ref->SetInt().SetFrom(loc_start);
    sought_loc_ref->SetInt().SetTo(loc_stop);

    // find first feat which is to the right of sought_loc and therefore
    // can't possibly contain the whole thing.
    TLocToFeatMap::const_iterator feat_iter =
        loc_to_feat_map.upper_bound( sought_loc_ref );

    // go "leftwards" looking for best CDS
    int best_overlap_extra_bases = INT_MAX; // 0 would imply a perfect match
    CConstRef<CSeq_feat> best_feat;
    while( feat_iter != loc_to_feat_map.begin() ) {
        --feat_iter;

        const int feat_start = feat_iter->first->GetStart(eExtreme_Positional);
        const int feat_stop  = feat_iter->first->GetStop(eExtreme_Positional);
        const int feat_len = ( feat_stop - feat_start + 1 );

        // something wrong with feat loc
        if( feat_start < 0 || feat_stop < 0 ) {
            continue;
        }

        // see if we can't possibly find something better at this point
        // because we've gone too far left
        const int best_possible_overlap_extra_bases = ( loc_start - feat_start );
        if( best_possible_overlap_extra_bases > best_overlap_extra_bases ) {
            break;
        }

        if( loc_start >= feat_start && loc_stop <= feat_stop ) {
            const int overlap_extra_bases = ( feat_len - loc_len );
            if( overlap_extra_bases < best_overlap_extra_bases ) {
                best_overlap_extra_bases = overlap_extra_bases;
                best_feat = feat_iter->second;
                if( best_overlap_extra_bases == 0 ) {
                    // found a perfect match
                    break;
                }
            }
        }
    }

    return best_feat;
}

bool
CBestFeatFinder::CSeqLocSort::operator()( 
    const CConstRef<CSeq_loc> &loc1, 
    const CConstRef<CSeq_loc> &loc2 ) const
{
    // sort by location start
    const TSeqPos start1 = loc1->GetStart(eExtreme_Positional);
    const TSeqPos start2 = loc2->GetStart(eExtreme_Positional);
    if( start1 != start2 ) {
        return (start1 < start2);
    }

    // then by length (we use "stop" as a reasonable proxy for length comparisons)
    const TSeqPos stop1 = loc1->GetStop(eExtreme_Positional);
    const TSeqPos stop2 = loc2->GetStop(eExtreme_Positional);
    if( stop1 != stop2 ) {
        return (stop2 < stop1); // reversed because we want longest first
    }

    // extremes are equal
    return false;
}

END_objects_SCOPE
END_NCBI_SCOPE
