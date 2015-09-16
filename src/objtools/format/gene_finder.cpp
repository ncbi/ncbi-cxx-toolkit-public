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
* Author:  Aaron Ucko, NCBI
*          Mati Shomrat
* Maintainer: Frank Ludwig, Michael Kornbluh
*                        
* File Description:
*   Public API for finding the gene(s) on a given feature using the same
*   criteria as the flatfile generator.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objtools/format/items/gene_finder.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objmgr/blob_id.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/seq_map_ci.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

using namespace sequence;

CGeneFinder::CGeneSearchPlugin::CGeneSearchPlugin( 
    const CSeq_loc &location, 
    CScope & scope,
    const CGene_ref* filtering_gene_xref ) 
    : m_Loc_original_strand(eNa_strand_other),
    m_Filtering_gene_xref(filtering_gene_xref),
    m_Scope(&scope)
{
    ITERATE( CSeq_loc, loc_iter, location ) {
        const CSeq_id *seq_id = loc_iter.GetRangeAsSeq_loc()->GetId();
        if( seq_id != NULL ) {
            m_BioseqHandle = m_Scope->GetBioseqHandle( *seq_id );
            if( m_BioseqHandle ) {
                break;
            }
        }
    }
}

void CGeneFinder::CGeneSearchPlugin::processSAnnotSelector( 
    SAnnotSelector &sel ) 
{
    sel.SetIgnoreStrand();
    sel.SetIgnoreFarLocationsForSorting( m_BioseqHandle );
}

void CGeneFinder::CGeneSearchPlugin::setUpFeatureIterator ( 
    CBioseq_Handle &ignored_bioseq_handle,
    auto_ptr<CFeat_CI> &feat_ci,
    TSeqPos circular_length,
    CRange<TSeqPos> &range,
    const CSeq_loc& loc,
    SAnnotSelector &sel,
    CScope &scope,
    ENa_strand &strand )
{
    if ( m_BioseqHandle ) {
        // if we're circular, we may need to split our range into two pieces
        if( ( circular_length != kInvalidSeqPos ) &&
            ( range.GetFrom() > range.GetTo() )) 
        {
            // For circular locations, the "from" is greater than the "to", which
            // would not work properly if given to CFeat_CI.
            // So, as a work around, we transform the range
            // into a mix location of the form "join(0..to, from..MAXINT)"

            CRef<CSeq_loc> new_loc( new CSeq_loc );
            new_loc->SetInt().SetFrom( 0 );
            new_loc->SetInt().SetTo( range.GetTo() );

            CRef<CSeq_loc> otherHalfOfRange( new CSeq_loc );
            otherHalfOfRange->SetInt().SetFrom( range.GetFrom() );
            otherHalfOfRange->SetInt().SetTo( kMax_Int );

            new_loc->Add( *otherHalfOfRange );

            new_loc->SetStrand( loc.GetStrand() );
            new_loc->SetId( *loc.GetId() );

            feat_ci.reset( new CFeat_CI(scope, *new_loc, sel) );
        } else {            
            // remove far parts, if necessary
            bool loc_change_needed = false;
            ITERATE( CSeq_loc, loc_iter, loc ) {
                if( ! m_BioseqHandle.IsSynonym( loc_iter.GetSeq_id() ) ) {
                    loc_change_needed = true;
                    break;
                }
            }
            if( loc_change_needed ) {
                CRef<CSeq_loc> new_loc( new CSeq_loc );
                ITERATE( CSeq_loc, loc_iter, loc ) {
                    if( m_BioseqHandle.IsSynonym( loc_iter.GetSeq_id() ) ) {
                        new_loc->Add( *loc_iter.GetRangeAsSeq_loc() );
                    }
                }
                feat_ci.reset( new CFeat_CI(scope, *new_loc, sel) );
            } else {
                feat_ci.reset( new CFeat_CI(scope, loc, sel) );
            }
        }
    } else {
        feat_ci.reset( new CFeat_CI(scope, loc, sel) );
    }
}

void CGeneFinder::CGeneSearchPlugin::processLoc( 
    CBioseq_Handle &ignored_bioseq_handle,
    CRef<CSeq_loc> &loc,
    TSeqPos circular_length )
{
    m_Loc_original_strand = GeneSearchNormalizeLoc( m_BioseqHandle, loc, circular_length );
}

void CGeneFinder::CGeneSearchPlugin::processMainLoop( 
    bool &shouldContinueToNextIteration,
    CRef<CSeq_loc> &cleaned_loc_this_iteration, 
    CRef<CSeq_loc> &candidate_feat_loc,
    sequence::EOverlapType &overlap_type_this_iteration,
    bool &revert_locations_this_iteration,
    CBioseq_Handle &ignored_bioseq_handle,
    const CMappedFeat &feat,
    TSeqPos circular_length,
    SAnnotSelector::EOverlapType annot_overlap_type )
{
    // check if given candidate feat matches the filter
    if( m_Filtering_gene_xref != NULL && 
        feat.GetOriginalFeature().IsSetData() &&
        feat.GetOriginalFeature().GetData().IsGene() ) 
    {
        if( ! GeneMatchesXref( &feat.GetOriginalFeature().GetData().GetGene(), &*m_Filtering_gene_xref ) ) {
            shouldContinueToNextIteration = true;
            return;
        }
    }

    // determine if the candidate feat location is mixed-strand

    ENa_strand candidate_feat_original_strand = eNa_strand_other;

    const bool candidate_feat_is_mixed = IsMixedStrand( m_BioseqHandle, *candidate_feat_loc );

    const bool candidate_feat_bad_order = BadSeqLocSortOrderCStyle( m_BioseqHandle, *candidate_feat_loc );

    const TGeneSearchLocOpt norm_opt = ( (overlap_type_this_iteration == eOverlap_Contained) ?
fGeneSearchLocOpt_RemoveFar :
    0 );
    candidate_feat_original_strand = GeneSearchNormalizeLoc( m_BioseqHandle, candidate_feat_loc, 
        circular_length, norm_opt ) ;

    if( (norm_opt & fGeneSearchLocOpt_RemoveFar) != 0 ) {
        GeneSearchNormalizeLoc( m_BioseqHandle, 
            cleaned_loc_this_iteration, circular_length, norm_opt );
    }

    if( ( candidate_feat_bad_order || candidate_feat_is_mixed ) && 
        annot_overlap_type == SAnnotSelector::eOverlap_TotalRange )
    {
        if( overlap_type_this_iteration == eOverlap_Contained ) {
            overlap_type_this_iteration = eOverlap_SubsetRev;
            revert_locations_this_iteration = true;
        }
    }

    if( (candidate_feat_bad_order || candidate_feat_is_mixed) && 
        feat.IsSetExcept_text() && feat.GetExcept_text() == "trans-splicing" )
    {
        // strand matching is done piecewise if we're trans-spliced
        shouldContinueToNextIteration = true;

        CSeq_loc_CI candidate_feat_loc_iter( feat.GetLocation() );
        for( ; candidate_feat_loc_iter; ++candidate_feat_loc_iter ) {
            // any piece that's in cleaned_loc_this_iteration, must have a matching strand
            sequence::ECompare piece_comparison = sequence::Compare(
                *candidate_feat_loc_iter.GetRangeAsSeq_loc(),
                *cleaned_loc_this_iteration,
                &*m_Scope, sequence::fCompareOverlapping);
            if( piece_comparison != sequence::eNoOverlap ) 
            {
                if( x_StrandsMatch( m_Loc_original_strand, candidate_feat_loc_iter.GetStrand() ) ) {
                    // matching strands; don't skip this feature
                    shouldContinueToNextIteration = false;
                    break;
                }
            }
        }

        if( x_StrandsMatch( m_Loc_original_strand, candidate_feat_original_strand ) ) {
            // matching strands; don't skip this feature
            shouldContinueToNextIteration = false;
        }
    } else {
        if( ! x_StrandsMatch( m_Loc_original_strand, candidate_feat_original_strand ) ) {
            // mismatched strands; skip this feature
            shouldContinueToNextIteration = true;
        }
    }
}

void CGeneFinder::CGeneSearchPlugin::postProcessDiffAmount( 
    Int8 &cur_diff, 
    CRef<CSeq_loc> &cleaned_loc, 
    CRef<CSeq_loc> &candidate_feat_loc, 
    CScope &scope, 
    SAnnotSelector &sel, 
    TSeqPos circular_length ) 
{
    if( cur_diff < 0 ) {
        return;
    }

    // for, e.g. AL596104
    if( sel.GetOverlapType() == SAnnotSelector::eOverlap_Intervals ) {
        cur_diff = sequence::GetLength( *candidate_feat_loc, &scope );
    } else {
        const int start = (int)sequence::GetStart(*candidate_feat_loc, &scope, eExtreme_Positional);
        const int stop  = (int)sequence::GetStop(*candidate_feat_loc, &scope, eExtreme_Positional);
        if( (start > stop) && (circular_length > 0) &&
            (circular_length != kInvalidSeqPos) ) 
        {
            cur_diff = circular_length - abs( start - stop );
        } else {
            cur_diff = abs( start - stop );
        }
    }
}

bool CGeneFinder::CGeneSearchPlugin::x_StrandsMatch( 
    ENa_strand feat_strand, ENa_strand candidate_feat_original_strand )
{
    return ( candidate_feat_original_strand == feat_strand
        || ( candidate_feat_original_strand == eNa_strand_both    && feat_strand != eNa_strand_minus )
        || feat_strand == eNa_strand_both
        || (candidate_feat_original_strand == eNa_strand_unknown  && feat_strand  != eNa_strand_minus)
        || (feat_strand == eNa_strand_unknown                     && candidate_feat_original_strand != eNa_strand_minus) );
}

//  ----------------------------------------------------------------------------
// static
void CGeneFinder::GetAssociatedGeneInfo(
    const CSeq_feat_Handle & in_feat,
    CBioseqContext& ctx,
    const CConstRef<CSeq_loc> & feat_loc,
    CConstRef<CGene_ref> & out_suppression_check_gene_ref,
    const CGene_ref*& out_g_ref,      //  out: gene ref
    CConstRef<CSeq_feat>& out_s_feat, //  out: gene seq feat
    const CSeq_feat_Handle & in_parent_feat )
    // CConstRef<CFeatureItem> parentFeatureItem )
//
//  Find the feature's related gene information. The association is established
//  through dbxref if it exists and through best overlap otherwise.
//
//  Note: Any of the two outs may be invalid if the corresponding information
//  could not be found.
//  ----------------------------------------------------------------------------
{
    out_s_feat.Reset();
    out_g_ref = NULL;

    // guard against suppressed gene xrefs
    out_suppression_check_gene_ref = GetSuppressionCheckGeneRef(in_feat);
    if( out_suppression_check_gene_ref && 
        out_suppression_check_gene_ref->IsSuppressed() ) 
    {
        return;
    }

    // Try to resolve the gene directly
    CConstRef<CSeq_feat> resolved_feat = 
        CGeneFinder::ResolveGeneObjectId( ctx, in_feat );
    if( resolved_feat ) {
        out_s_feat = resolved_feat;
        out_g_ref = &out_s_feat->GetData().GetGene();
        return;
    }

    // this will point to the gene xref inside the feature, if any
    const CGene_ref *xref_g_ref = in_feat.GetGeneXref();
    string xref_label;
    if( xref_g_ref ) {
        xref_g_ref->GetLabel(&xref_label);
    }

    if( xref_label.empty() ) {
        xref_g_ref = NULL;
    }

    bool also_look_at_parent_CDS = false;

    // special cases for some subtypes
    switch( in_feat.GetFeatSubtype() ) {
        case CSeqFeatData::eSubtype_region:
        case CSeqFeatData::eSubtype_site:
        case CSeqFeatData::eSubtype_bond:
        case CSeqFeatData::eSubtype_mat_peptide_aa:
        case CSeqFeatData::eSubtype_sig_peptide_aa:
        case CSeqFeatData::eSubtype_transit_peptide_aa:
        case CSeqFeatData::eSubtype_preprotein:
            also_look_at_parent_CDS = true;
            break;
        default:
            break;
    }

    CConstRef<CGene_ref> pParentDecidingGeneRef = GetSuppressionCheckGeneRef(in_parent_feat);

    // always use CDS's ref if xref_g_ref directly if it's set (e.g. AB280922)
    if( also_look_at_parent_CDS &&
        pParentDecidingGeneRef &&
        (! pParentDecidingGeneRef->IsSuppressed()) )
    {
        out_g_ref = pParentDecidingGeneRef;
        out_s_feat.ReleaseOrNull();
        return;
    }

    // always use xref_g_ref directly if it's set, but CDS's xref isn't (e.g. NP_041400)
    if( also_look_at_parent_CDS && NULL != xref_g_ref ) {
        out_g_ref = xref_g_ref;
        out_s_feat.ReleaseOrNull();
        return;
    }

    // For primer_bind, we get genes only by xref, not by overlap
    if( in_feat.GetData().GetSubtype() != CSeqFeatData::eSubtype_primer_bind ) {
        CSeq_id_Handle id1 = sequence::GetId(ctx.GetHandle(),
            sequence::eGetId_Canonical);
        CSeq_id_Handle id2 = sequence::GetIdHandle(in_feat.GetLocation(),
            &ctx.GetScope());

        if (sequence::IsSameBioseq(id1, id2, &ctx.GetScope())) {
            out_s_feat = GetFeatViaSubsetThenExtremesIfPossible( 
                ctx, in_feat.GetFeatType(), in_feat.GetFeatSubtype(), in_feat.GetLocation(), CSeqFeatData::e_Gene, xref_g_ref );
        }
        else if (ctx.IsProt()  &&  in_feat.GetData().IsCdregion()) {
            /// genpept report; we need to do something different
            CMappedFeat cds = GetMappedCDSForProduct(ctx.GetHandle());
            if (cds) {
                out_s_feat = GetFeatViaSubsetThenExtremesIfPossible( 
                    ctx, cds.GetFeatType(), cds.GetFeatSubtype(), cds.GetLocation(), CSeqFeatData::e_Gene, xref_g_ref );
            }
        }
        else {
            out_s_feat = GetFeatViaSubsetThenExtremesIfPossible(
                ctx, in_feat.GetFeatType(), in_feat.GetFeatSubtype(), *feat_loc, CSeqFeatData::e_Gene, xref_g_ref );
        }

        // special cases for some subtypes
        if( also_look_at_parent_CDS ) {

            // remove gene if bad match
            bool ownGeneIsOkay = false;
            if( out_s_feat ) {
                const CSeq_loc &gene_loc = out_s_feat->GetLocation();
                if( sequence::Compare(gene_loc, *feat_loc, &ctx.GetScope(), sequence::fCompareOverlapping) == sequence::eSame ) {
                    ownGeneIsOkay = true;
                }
            }

            // Priority order for finding the peptide's gene
            // 1. Use parent CDS's gene xref (from the .asn)
            // 2. Use the feature's own gene but only 
            //    if it *exactly* overlaps it.
            // 3. Use the parent CDS's gene (found via overlap)
            if( pParentDecidingGeneRef ) {
                // get the parent CDS's gene
                out_s_feat.Reset();
                if( ! pParentDecidingGeneRef->IsSuppressed() ) {
                    out_g_ref = pParentDecidingGeneRef;
                    xref_g_ref = NULL; // TODO: is it right to ignore mat_peptide gene xrefs?
                }
            } else if( ownGeneIsOkay ) {
                // do nothing; it's already set
            } else {
                if( in_parent_feat ) {
                    CConstRef<CSeq_loc> pParentLocation( &in_parent_feat.GetLocation() );
                    out_s_feat = GetFeatViaSubsetThenExtremesIfPossible( 
                        ctx, CSeqFeatData::e_Cdregion, CSeqFeatData::eSubtype_cdregion, 
                        *pParentLocation, CSeqFeatData::e_Gene, xref_g_ref );
                } else {
                    CConstRef<CSeq_feat> cds_feat = GetFeatViaSubsetThenExtremesIfPossible(
                        ctx, in_feat.GetFeatType(), in_feat.GetFeatSubtype(), *feat_loc, CSeqFeatData::e_Cdregion, xref_g_ref );
                    if( cds_feat ) {
                        out_s_feat  = GetFeatViaSubsetThenExtremesIfPossible( 
                            ctx, CSeqFeatData::e_Cdregion, CSeqFeatData::eSubtype_cdregion, 
                            cds_feat->GetLocation(), CSeqFeatData::e_Gene, xref_g_ref );
                    } 
                }
            }
        } // end: if( also_look_at_parent_CDS )
    }

    if ( in_feat && NULL == xref_g_ref ) {
        if (out_s_feat) {
            out_g_ref = &( out_s_feat->GetData().GetGene() );
        }
    }
    else {

        // if we used an xref to a gene, but the new gene doesn't equal the xref,
        // then override it (example accession where this issue crops up:
        // AF231993.1 )

        if (out_s_feat) {
            out_g_ref = &out_s_feat->GetData().GetGene();
        }

        // find a gene match using the xref (e.g. match by locus or whatever)
        if( NULL != xref_g_ref && ! GeneMatchesXref( out_g_ref, xref_g_ref ) )
        {
            out_g_ref = NULL;
            out_s_feat.Reset();

            CSeq_feat_Handle feat = ResolveGeneXref( xref_g_ref, ctx.GetTopLevelEntry() );
            if( feat ) {
                const CGene_ref& other_ref = feat.GetData().GetGene();

                out_s_feat.Reset( &*feat.GetSeq_feat() );
                out_g_ref = &other_ref;
            }
        }

        // we found no match for the gene, but we can fall back on the xref
        // itself (e.g. K03223.1)
        if( NULL == out_g_ref ) {
            out_g_ref = xref_g_ref;
        }
    }
}

// static
CSeq_feat_Handle CGeneFinder::ResolveGeneXref( 
    const CGene_ref *xref_g_ref, 
    const CSeq_entry_Handle &top_level_seq_entry )
{
    // Allow locus to match locus-tag or vice versa, but favor proper
    // locus-to-locus or tag-to-tag matches.
    enum ERGX_MatchQuality {
        eRGX_NoMatch,
        eRGX_MatchedTagForLocus,
        eRGX_MatchedLocusForTag,
        eRGX_MatchedAsIs
    };
    CSeq_feat_Handle feat;

    if( xref_g_ref == NULL ) {
        return feat;
    }

    if( top_level_seq_entry ) {
        CTSE_Handle::TSeq_feat_Handles possible_feats;
        set<CTSE_Handle> tried;
        SSeqMapSelector sel(CSeqMap::fFindAny, kMax_Auto);
        ERGX_MatchQuality match_quality = eRGX_NoMatch;
        bool found_near_match = false;
        for (CBioseq_CI bs(top_level_seq_entry);
             !found_near_match  &&  bs;
             ++bs) {
            for (CSeqMap_CI it(*bs, sel);  it;  ++it) {
                const CTSE_Handle& tse_handle = it.GetUsingTSE();
                if (tried.find(tse_handle) != tried.end()) {
                    continue;
                }
                tried.insert(tse_handle);

                ERGX_MatchQuality new_quality = eRGX_NoMatch;
                
                CTSE_Handle::TSeq_feat_Handles new_possibilities
                    = tse_handle.GetGenesByRef(*xref_g_ref);
                if ( !new_possibilities.empty() ) {
                    new_quality = eRGX_MatchedAsIs;
                }
                if (new_quality == eRGX_NoMatch
                    &&  match_quality <= eRGX_MatchedLocusForTag
                    &&  xref_g_ref->IsSetLocus_tag()) {
                    new_possibilities = tse_handle.GetGenesWithLocus
                        (xref_g_ref->GetLocus_tag(), false);
                    if ( !new_possibilities.empty() ) {
                        new_quality = eRGX_MatchedLocusForTag;
                    }
                }
                if (new_quality == eRGX_NoMatch
                    &&  match_quality <= eRGX_MatchedTagForLocus
                    &&  xref_g_ref->IsSetLocus()) {
                    new_possibilities = tse_handle.GetGenesWithLocus
                        (xref_g_ref->GetLocus(), true);
                    if ( !new_possibilities.empty() ) {
                        new_quality = eRGX_MatchedTagForLocus;
                    }
                }

                if (new_quality > match_quality) {
                    possible_feats = new_possibilities;
                    match_quality  = new_quality;
                } else if (new_quality == match_quality) {
                    possible_feats.insert(possible_feats.end(),
                                          new_possibilities.begin(),
                                          new_possibilities.end());
                } else {
                    _ASSERT(new_quality == eRGX_NoMatch);
                }

                if (match_quality != eRGX_NoMatch
                    &&  tse_handle == top_level_seq_entry.GetTSE_Handle()) {
                    found_near_match = true;
                    break;
                }
            }
        }

        if (match_quality != eRGX_NoMatch) {
            int best_score = INT_MAX;
            NON_CONST_ITERATE( CTSE_Handle::TSeq_feat_Handles, feat_iter, possible_feats) {
                CSeq_feat_Handle a_possible_feat = *feat_iter;
                const int this_feats_score = sequence::GetLength( a_possible_feat.GetLocation(), &top_level_seq_entry.GetScope() );
                if( this_feats_score < best_score ) {
                    feat = a_possible_feat;
                    best_score = this_feats_score;
                }
            }
        }
    }

    return feat;
}


// static
CConstRef<CGene_ref> 
CGeneFinder::GetSuppressionCheckGeneRef(const CSeq_feat_Handle & feat)
{
    CConstRef<CGene_ref> answer;
    if( ! feat ) {
        return answer;
    }

    if (feat.IsSetXref()) {
        ITERATE (CSeq_feat::TXref, it, feat.GetXref()) {
            const CSeqFeatXref& xref = **it;
            if (xref.IsSetData() && xref.GetData().IsGene() ) {
                answer.Reset( &xref.GetData().GetGene() ) ;
                if( xref.GetData().GetGene().IsSuppressed()) {
                    return answer;
                }
            }
        }
    }
    
    return answer;
}

// static
bool CGeneFinder::CanUseExtremesToFindGene( CBioseqContext& ctx, const CSeq_loc &location )
{
    // disallowed if mixed strand
    if( IsMixedStrand( CBioseq_Handle(), location) ) {
        return false;
    }

    // disallowed if bad order inside seqloc
    // if( sequence::BadSeqLocSortOrder( ctx.GetHandle(), location ) ) { // TODO: one day switch to this.
    // We use BadSeqLocSortOrderCStyle to match C's behavior, even though it's not strictly correct.
    if( BadSeqLocSortOrderCStyle( ctx.GetHandle(), location ) ) {
        return false;
    }

    if( ctx.IsSegmented() || ctx.IsEMBL() || ctx.IsDDBJ() ) {
        return true;
    }

    if( ctx.CanGetMaster() ) {
        const bool isSegmented = (ctx.GetMaster().GetNumParts() > 1);
        if( isSegmented ) {
            return true;
        }
    }

    // allow for old-style accessions (1 letter + 5 digits)
    // chop off the decimal point and anything after it, if necessary
    string::size_type length_before_decimal_point = ctx.GetAccession().find( '.' );
    if( length_before_decimal_point == string::npos ) {
        // no decimal point
        length_before_decimal_point = ctx.GetAccession().length();
    }
    if( length_before_decimal_point == 6 ) {
        return true;
    }

    return false;
}

// static
CConstRef<CSeq_feat> 
CGeneFinder::GetFeatViaSubsetThenExtremesIfPossible( 
    CBioseqContext& ctx, CSeqFeatData::E_Choice feat_type,
    CSeqFeatData::ESubtype feat_subtype,
    const CSeq_loc &location, CSeqFeatData::E_Choice sought_type,
    const CGene_ref* filtering_gene_xref )
{
    CRef<CSeq_loc> cleaned_location( new CSeq_loc );
    cleaned_location->Assign( location );

    CScope *scope = &ctx.GetScope();

    // special case for variation
    if( feat_type == CSeqFeatData::e_Variation || 
        ( feat_type == CSeqFeatData::e_Imp && 
            ( feat_subtype == CSeqFeatData::eSubtype_variation ||
              feat_subtype == CSeqFeatData::eSubtype_variation_ref ))) 
    {
        const ENa_strand first_strand_to_try = ( 
            location.GetStrand() == eNa_strand_minus ?
                eNa_strand_minus :
                eNa_strand_plus );

        // try one strand first
        cleaned_location->SetStrand( first_strand_to_try );
        CConstRef<CSeq_feat> feat;
        CGeneSearchPlugin plugin( *cleaned_location, *scope, filtering_gene_xref );
        feat = sequence::GetBestOverlappingFeat
            ( *cleaned_location,
            sought_type,
            sequence::eOverlap_Contained,
            *scope,
            0,
            &plugin );
        if( feat ) {
            return feat;
        }

        // if that fails, try the other strand
        if( first_strand_to_try == eNa_strand_plus ) {
            cleaned_location->SetStrand( eNa_strand_minus );
        } else {
            cleaned_location->SetStrand( eNa_strand_plus );
        }
        CGeneSearchPlugin plugin2( *cleaned_location, *scope, filtering_gene_xref );
        return sequence::GetBestOverlappingFeat
            ( *cleaned_location,
            sought_type,
            sequence::eOverlap_Contained,
            *scope,
            0,
            &plugin2 );
    }

    // normal case
    return GetFeatViaSubsetThenExtremesIfPossible_Helper( ctx, scope, *cleaned_location, sought_type, filtering_gene_xref );
}

// static
CConstRef<CSeq_feat> 
CGeneFinder::GetFeatViaSubsetThenExtremesIfPossible_Helper(
    CBioseqContext& ctx, CScope *scope, const CSeq_loc &location, CSeqFeatData::E_Choice sought_type,
    const CGene_ref* filtering_gene_xref)
{
    // holds reference to temporary scope if it's used
    CRef<CScope> temp_scope;

    const static string kGbLoader = "GBLOADER";
    bool needToAddGbLoaderBack = false;
    if( scope && ( ctx.IsEMBL() || ctx.IsDDBJ() ) && 
        scope->GetObjectManager().FindDataLoader(kGbLoader) ) 
    {
        // try to remove the GBLOADER temporarily
        try {
            scope->RemoveDataLoader(kGbLoader);
            needToAddGbLoaderBack = true;
        } catch(...) {
            // we couldn't remove the GBLOADER temporarily, so we make a temporary substitute CScope

            // add copy of scope, but without the gbloader
            // TODO: check if this call is fast
            scope = new CScope(*CObjectManager::GetInstance());
            scope->AddDefaults();
            scope->RemoveDataLoader(kGbLoader);
            temp_scope.Reset(scope);
        }
    }

    CConstRef<CSeq_feat> feat;
    feat = GetFeatViaSubsetThenExtremesIfPossible_Helper_subset(
        ctx, scope, location, sought_type,
        filtering_gene_xref );
    if( ! feat && CanUseExtremesToFindGene(ctx, location) ) {
        feat = GetFeatViaSubsetThenExtremesIfPossible_Helper_extremes(
            ctx, scope, location, sought_type,
            filtering_gene_xref );
    }

    if( needToAddGbLoaderBack ) {
        scope->AddDataLoader(kGbLoader);
    }

    return feat;
}

// static
CConstRef<CSeq_feat> 
CGeneFinder::GetFeatViaSubsetThenExtremesIfPossible_Helper_subset(
    CBioseqContext& ctx, CScope *scope, const CSeq_loc &location, CSeqFeatData::E_Choice sought_type,
    const CGene_ref* filtering_gene_xref )
{
    CGeneSearchPlugin plugin( location, *scope, filtering_gene_xref );
    return sequence::GetBestOverlappingFeat
                    ( location,
                     sought_type,
                     sequence::eOverlap_SubsetRev,
                     *scope,
                     0,
                     &plugin );
}

// static
CConstRef<CSeq_feat> 
CGeneFinder::GetFeatViaSubsetThenExtremesIfPossible_Helper_extremes(
    CBioseqContext& ctx, CScope *scope, const CSeq_loc &location, CSeqFeatData::E_Choice sought_type,
    const CGene_ref* filtering_gene_xref )
{
    CGeneSearchPlugin plugin( location, *scope, filtering_gene_xref );
    return sequence::GetBestOverlappingFeat
        ( location,
        sought_type,
        sequence::eOverlap_Contained,
        *scope,
        0,
        &plugin );
}

// static
CConstRef<CSeq_feat> 
CGeneFinder::ResolveGeneObjectId( CBioseqContext& ctx, 
                       const CSeq_feat_Handle &feat,
                       int recursion_depth )
{
    const static CConstRef<CSeq_feat> kNullRef;

    // prevent infinite loop due to circular references
    if( recursion_depth > 10 ) {
        return kNullRef;
    }

    if (feat.IsSetXref()) {
        ITERATE (CSeq_feat::TXref, it, feat.GetXref()) {
            const CSeqFeatXref& xref = **it;
            if (xref.IsSetData() && xref.GetData().IsGene() ) {
                if( xref.GetData().GetGene().IsSuppressed()) {
                    return kNullRef;
                }
                // TODO: in the future, we should handle non-local references, too
                if( xref.IsSetId() ) {
                    if( xref.GetId().IsLocal() ) {
                        const CObject_id &obj_id = xref.GetId().GetLocal();
                        SAnnotSelector sel;
                        sel.SetLimitTSE( ctx.GetHandle().GetTSE_Handle() );
                        CFeat_CI feat_ci( ctx.GetHandle().GetTSE_Handle(), sel, obj_id );
                        if( feat_ci ) {
                            const CSeq_feat &feat = feat_ci->GetOriginalFeature();
                            if( feat.IsSetData() && feat.GetData().IsGene() ) {
                                return CConstRef<CSeq_feat>( &feat );
                            } else {
                                // we resolved to a non-gene, so try to resolve to that feature's gene
                                return CGeneFinder::ResolveGeneObjectId( ctx, *feat_ci, recursion_depth+1 );
                            }
                        }
                    }
                }
            }
        }
    }

    return kNullRef;
}

//  ----------------------------------------------------------------------------
// static 
bool CGeneFinder::GeneMatchesXref( 
    const CGene_ref * other_ref, 
    const CGene_ref * xref )
{
    if( NULL == other_ref || NULL == xref ) {
        return false;
    }

    // in case we get a weird xref with nothing useful set
    if( ! xref->IsSetLocus() && ! xref->IsSetLocus_tag() && ! xref->IsSetSyn() ) {
        return false;
    }

    if( xref->IsSetLocus() ) {
        if( (! other_ref->IsSetLocus() || other_ref->GetLocus() != xref->GetLocus()) &&
            (! other_ref->IsSetLocus_tag() || other_ref->GetLocus_tag() != xref->GetLocus()) ) 
        {
            return false;
        }
    }

    if( xref->IsSetLocus_tag() ) {
        if( ! other_ref->IsSetLocus_tag() || other_ref->GetLocus_tag() != xref->GetLocus_tag() ) {
            return false;
        }
    }

    if( xref->IsSetSyn() ) {
        // make sure all syns in the xref are also set in the gene (other_ref)
        if( ! other_ref->IsSetSyn() ) {
            return false;
        }

        // get set of gene syns so we can quickly check if the gene has the ref'd syns
        set<string> gene_syns;
        const CGene_ref::TSyn & gene_syns_list = xref->GetSyn();
        copy( gene_syns_list.begin(), gene_syns_list.end(),
            inserter(gene_syns, gene_syns.begin()) );

        const CGene_ref::TSyn & ref_syns = xref->GetSyn();
        ITERATE( CGene_ref::TSyn, syn_iter, ref_syns ) {
            if( gene_syns.find(*syn_iter) == gene_syns.end() ) {
                return false;
            }
        }
    }

    return true;
}

// matches C's behavior, even though it's not strictly correct.
// In the future, we may prefer to use sequence::BadSeqLocSortOrder
bool CGeneFinder::BadSeqLocSortOrderCStyle( CBioseq_Handle &bioseq_handle, const CSeq_loc &location )
{
    CSeq_loc_CI previous_loc;

    ITERATE( CSeq_loc, loc_iter, location ) {
        if( ! previous_loc ) {
            previous_loc = loc_iter;
            continue;
        }
        if ( previous_loc.GetSeq_id().Equals( loc_iter.GetSeq_id() ) ) {
            const int prev_to = previous_loc.GetRange().GetTo();
            const int this_to = loc_iter.GetRange().GetTo();
            if ( loc_iter.GetStrand() == eNa_strand_minus ) {
                if (  prev_to < this_to) {
                    return true;
                }
            } else {
                if (prev_to > this_to) {
                    return true;
                }
            }
        }
        previous_loc = loc_iter;
    }

    return false;
}

// static
ENa_strand CGeneFinder::GeneSearchNormalizeLoc( 
    CBioseq_Handle top_bioseq_handle, 
    CRef<CSeq_loc> &loc, const TSeqPos circular_length,
    TGeneSearchLocOpt opt )
{
    // remove far parts first, if requested
    if( top_bioseq_handle && (opt & fGeneSearchLocOpt_RemoveFar) != 0 ) {
        CRef<CSeq_loc> new_loc( new CSeq_loc );
        CSeq_loc_mix::Tdata &new_loc_parts = new_loc->SetMix().Set();

        CSeq_loc_CI loc_iter( *loc, CSeq_loc_CI::eEmpty_Skip, CSeq_loc_CI::eOrder_Biological );
        for( ; loc_iter; ++loc_iter ) {
            const CSeq_id& loc_id = loc_iter.GetSeq_id();
            if( top_bioseq_handle.IsSynonym(loc_id) ) {
                CRef<CSeq_loc> new_part( new CSeq_loc );
                new_part->Assign( *loc_iter.GetRangeAsSeq_loc() );
                new_loc_parts.push_back( new_part );
            } 
        }
        loc = new_loc;
    }

    CRef<CSeq_loc> new_loc( new CSeq_loc );
    CSeq_loc_mix::Tdata &new_loc_parts = new_loc->SetMix().Set();

    ENa_strand original_strand = eNa_strand_other;

    CSeq_loc_CI loc_iter( *loc, CSeq_loc_CI::eEmpty_Skip, CSeq_loc_CI::eOrder_Positional );
    for( ; loc_iter; ++loc_iter ) {
        // parts that are on far bioseqs don't count as part of strandedness (e.g. as in X17229)
        // ( CR956646 is another good test case since its near parts 
        //   are minus strand and far are plus on the "GNAS" gene )
        if( top_bioseq_handle && (opt & fGeneSearchLocOpt_RemoveFar) == 0 ) { 
            const CSeq_id& loc_id = loc_iter.GetSeq_id();
            if( top_bioseq_handle.IsSynonym(loc_id) ) {
                if( original_strand == eNa_strand_other) {
                    // strand should have strandedness of first near part
                    original_strand = loc_iter.GetStrand();
                }
            }
        } else {
            if( original_strand == eNa_strand_other) {
                // strand should have strandedness of first near part
                original_strand = loc_iter.GetStrand();
            }
        }
        // new_loc->Add( * );
        CRef<CSeq_loc> new_part( new CSeq_loc );
        new_part->Assign( *loc_iter.GetRangeAsSeq_loc() );
        new_loc_parts.push_back( new_part );
    }
    new_loc->SetStrand( eNa_strand_plus );
    loc = new_loc;

    // If location is from multiple seq-id's we can't
    // really determine the strand.  (e.g. AL022339)
    if( ! top_bioseq_handle ) {
        original_strand = eNa_strand_unknown;
    }

    return original_strand;
}

// static
bool CGeneFinder::IsMixedStrand( 
    CBioseq_Handle bioseq_handle, const CSeq_loc &loc )
{
    bool plus_seen = false;
    bool minus_seen = false;

    ITERATE( CSeq_loc, loc_iter, loc ) {
        if( loc_iter.IsEmpty() ) {
            continue;
        }
        // far parts don't count as part of strandedness
        if( bioseq_handle ) {
            const CSeq_id& loc_id = loc_iter.GetSeq_id();
            if( ! bioseq_handle.IsSynonym(loc_id) ) { 
                continue;
            }
        }
        switch( loc_iter.GetStrand() ) {
                case eNa_strand_unknown:
                case eNa_strand_plus:
                    plus_seen = true;
                    break;
                case eNa_strand_minus:
                    minus_seen = true;
                    break;
                default:
                    break;
        }
    }
    
    return ( plus_seen && minus_seen );
}

END_SCOPE(objects)
END_NCBI_SCOPE

