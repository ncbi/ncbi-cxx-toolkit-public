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
* ===========================================================================
*/

/// @file gene_finder.hpp
/// Public API for finding the gene(s) on a given feature using the same
/// criteria as the flatfile generator.

#ifndef __gene_finder_hpp__
#define __gene_finder_hpp__

#include <objmgr/scope.hpp>
#include <objmgr/seq_feat_handle.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objtools/format/context.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class NCBI_XOBJUTIL_EXPORT CGeneFinder {
public:

    /// Find the gene associated with the given feature.
    /// @in_feat
    ///  The handle to the feature for which we are seeking the gene.
    /// @param ctx
    ///  This holds contextual information on the bioseq that the feature 
    ///  is located on.
    /// @feat_loc
    ///  This is the location of the feature which is used for gene-finding
    ///  purposes.  It is occasionally different from in_feat.GetLocation()
    ///  due to CDS-mapping and such.
    /// @out_suppression_check_gene_ref
    ///  This holds the first suppressed CGene_ref on the feature.
    ///  If none are suppressed, it holds the last CGene_ref on in_feat.
    /// @out_g_ref
    ///  This holds the CGene_ref referring to the gene of in_feat.
    /// @out_s_feat
    ///  If found, this holds the CSeq_feat of the gene belonging to in_feat.
    /// @in_parent_feat
    ///  If the feature has no parent, just supply an empty CSeq_feat_Handle.
    static void GetAssociatedGeneInfo(
        const CSeq_feat_Handle & in_feat,
        CBioseqContext& ctx,
        const CConstRef<CSeq_loc> & feat_loc,
        CConstRef<CGene_ref> & out_suppression_check_gene_ref,
        const CGene_ref*& out_g_ref,      //  out: gene ref
        CConstRef<CSeq_feat>& out_s_feat, //  out: gene seq feat
        const CSeq_feat_Handle & in_parent_feat );

    /// This does plain, simple resolution of a CGene_ref to its
    /// gene.  for example, this might be used to do gene-resolution
    /// for certain non-Genbank records.
    /// @xref_g_ref
    ///  The CGene_ref we're resolving.
    /// @top_level_seq_entry
    ///  This is the Seq-entry under which we look for the gene that
    ///  xref_g_ref points to.
    /// @return
    ///  The CSeq_feat_Handle of the gene that xref_g_ref refers to, or
    ///  an empty CSeq_feat_Handle if none found.
    static CSeq_feat_Handle ResolveGeneXref( 
        const CGene_ref *xref_g_ref, 
        const CSeq_entry_Handle &top_level_seq_entry );

private:

    // This is a plugin supplied to sequence::GetBestOverlappingFeat
    // to adjust its behavior to reflect how the flatfile generator
    // searches for genes.
    class CGeneSearchPlugin : public sequence::CGetOverlappingFeaturesPlugin {
    public:
        CGeneSearchPlugin( 
            const CSeq_loc &location, 
            CScope & scope,
            const CGene_ref* filtering_gene_xref );

        void processSAnnotSelector( SAnnotSelector &sel );
        void setUpFeatureIterator ( 
            CBioseq_Handle &ignored_bioseq_handle,
            auto_ptr<CFeat_CI> &feat_ci,
            TSeqPos circular_length,
            CRange<TSeqPos> &range,
            const CSeq_loc& loc,
            SAnnotSelector &sel,
            CScope &scope,
            ENa_strand &strand );
        void processLoc( 
            CBioseq_Handle &ignored_bioseq_handle,
            CRef<CSeq_loc> &loc,
            TSeqPos circular_length );
        void processMainLoop( 
            bool &shouldContinueToNextIteration,
            CRef<CSeq_loc> &cleaned_loc_this_iteration, 
            CRef<CSeq_loc> &candidate_feat_loc,
            sequence::EOverlapType &overlap_type_this_iteration,
            bool &revert_locations_this_iteration,
            CBioseq_Handle &ignored_bioseq_handle,
            const CMappedFeat &feat,
            TSeqPos circular_length,
            SAnnotSelector::EOverlapType annot_overlap_type );
        void postProcessDiffAmount( 
            Int8 &cur_diff, 
            CRef<CSeq_loc> &cleaned_loc, 
            CRef<CSeq_loc> &candidate_feat_loc, 
            CScope &scope, 
            SAnnotSelector &sel, 
            TSeqPos circular_length );

    private:
        // Our algo does require a little bit of state.
        ENa_strand m_Loc_original_strand;
        CBioseq_Handle m_BioseqHandle;
        CConstRef<CGene_ref> m_Filtering_gene_xref;
        CRef<CScope> m_Scope;

        bool x_StrandsMatch( ENa_strand feat_strand, ENa_strand candidate_feat_original_strand );
    };

    // Helper functions for implementing the gene-finding logic.

    // If any gene-ref is suppressed, it returns that one.
    // Otherwise, it returns the last generef
    static
    CConstRef<CGene_ref> GetSuppressionCheckGeneRef(const CSeq_feat_Handle & feat);

    static bool CanUseExtremesToFindGene( CBioseqContext& ctx, const CSeq_loc &location );

    static
    CConstRef<CSeq_feat> 
        GetFeatViaSubsetThenExtremesIfPossible( 
            CBioseqContext& ctx, CSeqFeatData::E_Choice feat_type,
            CSeqFeatData::ESubtype feat_subtype,
            const CSeq_loc &location, CSeqFeatData::E_Choice sought_type,
            const CGene_ref* filtering_gene_xref ) ;

    static
    CConstRef<CSeq_feat> 
        GetFeatViaSubsetThenExtremesIfPossible_Helper(
            CBioseqContext& ctx, CScope *scope, const CSeq_loc &location, CSeqFeatData::E_Choice sought_type,
            const CGene_ref* filtering_gene_xref );

    // These 2 functions could just be folded into x_GetFeatViaSubsetThenExtremesIfPossible_Helper,
    // but they're separate to make it easier to profile the different paths.
    static
    CConstRef<CSeq_feat> 
        GetFeatViaSubsetThenExtremesIfPossible_Helper_subset(
            CBioseqContext& ctx, CScope *scope, const CSeq_loc &location, CSeqFeatData::E_Choice sought_type,
            const CGene_ref* filtering_gene_xref );
    static
    CConstRef<CSeq_feat> 
        GetFeatViaSubsetThenExtremesIfPossible_Helper_extremes(
            CBioseqContext& ctx, CScope *scope, const CSeq_loc &location, CSeqFeatData::E_Choice sought_type,
            const CGene_ref* filtering_gene_xref );

    static CConstRef<CSeq_feat> ResolveGeneObjectId( 
        CBioseqContext& ctx, 
        const CSeq_feat_Handle &feat,
        int recursion_depth = 0 );

    static bool GeneMatchesXref( 
        const CGene_ref * other_ref, 
        const CGene_ref * xref );

    static bool BadSeqLocSortOrderCStyle( 
        CBioseq_Handle &bioseq_handle, const CSeq_loc &location );

    // returns original strand (actually, just the strandedness of the first part,
    // if it was mixed )
    // Also, it converts the given loc to positive.
    enum FGeneSearchLocOpt {
        fGeneSearchLocOpt_RemoveFar = 1 << 0
    };
    typedef int TGeneSearchLocOpt;

    static ENa_strand GeneSearchNormalizeLoc( 
        CBioseq_Handle top_bioseq_handle, 
        CRef<CSeq_loc> &loc, const TSeqPos circular_length,
        TGeneSearchLocOpt opt = 0 );

    static bool IsMixedStrand( 
        CBioseq_Handle bioseq_handle, const CSeq_loc &loc );
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // ! __gene_finder_hpp__

