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
* File Description:
*
*   Utility functions pretaining to Variation
*
* ===========================================================================
*/

#ifndef VARIATION_UTIL2_HPP_
#define VARIATION_UTIL2_HPP_

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seq/Seq_data.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Variation_ref.hpp>
#include <objects/seqfeat/VariantProperties.hpp>

#include <objects/variation/Variation.hpp>
#include <objects/variation/VariantPlacement.hpp>
#include <objects/seqfeat/Variation_inst.hpp>

#include <util/rangemap.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seq_loc_mapper.hpp>
#include <objmgr/util/feature.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

namespace variation {

class CVariationUtil : public CObject
{
public:
    CVariationUtil(CScope& scope)
       : m_scope(&scope)
       , m_variant_properties_index(scope)
    {}

    static TSeqPos s_GetLength(const CVariantPlacement& p, CScope* scope);

/// Methods to remap a VariantPlacement

    /*!
     * Remap using provided alignment. Ids of at least one row must match.
     * When remapping between transcript/genomic coordiane systems, this method should be used
     * instead of the mapper-based, as if the location is intronic, the method
     * will use the spliced-alignment to create or resolve offset-placement as appropriate.
     */
    CRef<CVariantPlacement> Remap(const CVariantPlacement& p, const CSeq_align& aln);

    // Remap a placement with a specified mapper. If a cdna->genomic or genomic->cdna mapping
    // is attempted, this will throw.
    CRef<CVariantPlacement> Remap(const CVariantPlacement& p, CSeq_loc_Mapper& mapper);


/// Methods to convert between nucleotide and protein

    /// Convert protein-variation (single-AA missense/nonsense) to nuc-variation on the precursor in the parent nuc-prot.
    typedef int TAA2NAFlags;
    enum EAA2NAFlags
    {
        fAA2NA_truncate_common_prefix_and_suffix = 1,
        fAA2NA_default = 0
    };
    CRef<CVariation> InferNAfromAA(const CVariation& prot_variation, TAA2NAFlags flags = fAA2NA_default);


    /// Evaluate protein effect of a single-inst @ single-placement
    ///
    /// Two placements will be added: The protein placement, and an additional placement
    /// on the original nucleotide level, representing the location and sequence of affected codons.
    ///
    /// The inst will be at the nucleotide level (describe variant codons). Rationale: the user may
    /// want to know the codons, or translate it to AA if necessary.
    ///
    /// Only a subset of sequence edits is supported (mismatch, ins, del, indel)
    /// Nuc-variation's placement must fall within cds-feat location.
    CRef<CVariation> TranslateNAtoAA(const CVariation_inst& nuc_inst, const CVariantPlacement& p, const CSeq_feat& cds_feat);


    /// Find the CDSes for the first placement; Compute prot consequence using TranslateNAtoAA for each
    /// and attach results to nuc_variation.consequnece. If seq-id specified, use only the placement with the specified id.
    /// Note: Alternatively, the API could be "create and return consequence protein variation(s)", rather than attach,
    ///       but that for hierarchical input it would be hard to tell which consequence corresponds to which node.
    void AttachProteinConsequences(CVariation& nuc_variation, const CSeq_id* = NULL);


///Other utility methods:

    /// Flipping strand involves flipping all placements, and insts.
    /// Note: for insertions, the placement(s) must be dinucleotide where
    /// insertion occurs, such that semantic correctness is maintained.
    void FlipStrand(CVariation& v) const;

    /// Flipping a placement involves flipping strand on the seq-loc and swapping the offsets (and signs).
    /// If there's a seq-literal, it is reverse-complemented.
    void FlipStrand(CVariantPlacement& vp) const;

    /// Flip strand on delta-items and reverse the order.
    void FlipStrand(CVariation_inst& vi) const;

    /// Reverse-complement seq-literals, flip strand on the seq-locs.
    void FlipStrand(CDelta_item& di) const;

    /// If have offsets (intronic) - return false; else set seq field on the placement and return true.
    bool AttachSeq(CVariantPlacement& p, TSeqPos max_len = 50);

    CVariantPlacement::TMol GetMolType(const CSeq_id& id);

    /*!
     * If placement is not intronic, or alignment is not spliced-seg -> eNotApplicable
     * Else if variation is intronic but location is not at exon boundary -> eFail
     * Else -> ePass
     */
    enum ETestStatus
    {
        eFail,
        ePass,
        eNotApplicable,
    };
    ETestStatus CheckExonBoundary(const CVariantPlacement& p, const CSeq_align& aln);

    /// Calculate upstream (first) and downstream(second) flanks for loc
    struct SFlankLocs
    {
        CRef<CSeq_loc> upstream;
        CRef<CSeq_loc> downstream;
    };
    SFlankLocs CreateFlankLocs(const CSeq_loc& loc, TSeqPos len);


/// Methods to compute properties


    /// Set location-specific variant properties for a variation by applying
    /// x_SetVariantProperties(...) on each found placement + its variation
    void SetVariantProperties(CVariation& v);

    /// Supported SO-terms
    enum ESOTerm
    {
        //location-specific terms
        eSO_2KB_upstream_variant    =1636,
        eSO_500B_downstream_variant =1634,
        eSO_splice_donor_variant    =1575,
        eSO_splice_acceptor_variant =1574,
        eSO_intron_variant          =1627,
        eSO_5_prime_UTR_variant     =1623,
        eSO_3_prime_UTR_variant     =1624,
        eSO_coding_sequence_variant =1580,
        eSO_nc_transcript_variant   =1619,

        //variant-specific terms
        eSO_synonymous_codon        =1588,
        eSO_non_synonymous_codon    =1583, //missense
        eSO_stop_gained             =1587, //nonsense
        eSO_stop_lost               =1578,
        eSO_frameshift_variant      =1589
    };
    typedef vector<ESOTerm> TSOTerms;
    void AsSOTerms(const CVariantProperties& p, TSOTerms& terms);
    static string AsString(ESOTerm term);


    /// If at any level in variation-set all variations have all same placements, move them to the parent level.
    static void s_FactorOutPlacements(CVariation& v);

    /*!
     * If this variation has placements defined, return it; otherwise, recursively try
     * parent all the way to the root; return NULL if nothing found.
     * Precondition: root Variation must have been indexed (links to parents defined as necessary)
     */
    static const CVariation::TPlacements* s_GetPlacements(const CVariation& v);


private:
    /// Fill location-specific variant-properties for a placement.
    /// Attach related GeneID(s) to placement with respect to which the properties are defined.
    /// Note: This may include
    void x_SetVariantProperties(CVariantProperties& prop, CVariantPlacement& placement);

    CRef<CVariantPlacement> x_Remap(const CVariantPlacement& p, CSeq_loc_Mapper& mapper);

    void ChangeToDelins(CVariation& v);

    void x_SetVariantProperties(CVariantProperties& p, const CVariation_inst& vi, const CSeq_loc& loc);
    void x_SetVariantPropertiesForIntronic(CVariantProperties& p, int offset, const CSeq_loc& loc, CBioseq_Handle& bsh);

    void x_ChangeToDelins(CVariation& v);

    void x_InferNAfromAA(CVariation& v, TAA2NAFlags flags);

    void x_TranslateNAtoAA(CVariation& prot_variation);

    //return iupacna or ncbieaa literals
    CRef<CSeq_literal> x_GetLiteralAtLoc(const CSeq_loc& loc);

    static CRef<CSeq_literal> s_CatLiterals(const CSeq_literal& a, const CSeq_literal& b);

    static void s_AttachGeneIDdbxref(CVariantPlacement& p, int gene_id);

    static void s_UntranslateProt(const string& prot_str, vector<string>& codons);

    static size_t s_CountMatches(const string& a, const string& b);

    void s_CalcPrecursorVariationCodon(
            const string& codon_from, //codon on cDNA
            const string& prot_to,    //missense/nonsense AA
            vector<string>& codons_to);           //calculated variation-codon

    static string s_CollapseAmbiguities(const vector<string>& seqs);

    /*!
     * Apply offsets to the variation's location (variation must be inst)
     * E.g. the original variation could be a transcript location with offsets specifying intronic location.
     * After original transcript lociation is remapped, the offsets are to be applied to the remapped location
     * to get absolute offset-free genomic location.
     */
    static void s_ResolveIntronicOffsets(CVariantPlacement& p);

    /*!
     * If start|stop don't fall into an exon, adjust start|stop to the closest exon boundary and add offset to inst.
     * This is to be applied before remapping a genomic variation to transcript coordinates
     */
    static void s_AddIntronicOffsets(CVariantPlacement& p, const CSpliced_seg& ss, CScope* scope);

    /*!
     * Extend delins to specified location (adjust location and delta)
     *
     * Precondition:
     *    -variation must be a inst-type normalized delins (via x_ChangeToDelins)
     *    -loc must be a superset of variation's location.
     */
    void x_AdjustDelinsToInterval(CVariation& v, const CSeq_loc& loc);



    /*!
     * Call s_GetPlacements and find first seq-literal in any of them.
     */
    static const CConstRef<CSeq_literal> s_FindFirstLiteral(const CVariation& v);


private:

    /// Given a seq-loc, compute CVariantProperties::TGene_location from annotation.
    /// Precompute for the whole sequence and keep the cache to avoid objmgr annotation
    /// lookups on every call.
    class CVariantPropertiesIndex
    {
    public:
        typedef pair<int, CVariantProperties::TGene_location> TGeneIDAndProp;
        typedef vector<TGeneIDAndProp> TGeneIDAndPropVector;

        CVariantPropertiesIndex(CScope& scope)
          : m_scope(&scope)
        {}

        void GetLocationProperties(const CSeq_loc& loc, TGeneIDAndPropVector& v);

    private:
        void x_Index(const CSeq_id_Handle& idh);
        void x_Index(feature::CFeatTree& ft);
        void x_Add(const CSeq_loc& loc, int gene_id, CVariantProperties::TGene_location prop);


        typedef pair<CRef<CSeq_loc>, CRef<CSeq_loc> > TLocsPair; //5' and 3' respectively
        static TLocsPair s_GetStartAndStopCodonsLocs(const CSeq_loc& cds_loc);
        static TLocsPair s_GetUTRLocs(const CSeq_loc& cds_loc, const CSeq_loc& parent_loc);
        static TLocsPair s_GetNeighborhoodLocs(const CSeq_loc& gene_loc, TSeqPos max_pos);
        static TLocsPair s_GetIntronsAndSpliceSiteLocs(const CSeq_loc& rna_loc);
        static int s_GetGeneID(const CMappedFeat& mf, feature::CFeatTree& ft);

    private:
        typedef CRangeMap<TGeneIDAndPropVector, TSeqPos> TRangeMap;
        typedef map<CSeq_id_Handle, TRangeMap> TIdRangeMap;

        CRef<CScope> m_scope;
        TIdRangeMap m_loc2prop;
    };

private:
    CRef<CScope> m_scope;
    CVariantPropertiesIndex m_variant_properties_index;
};

};

END_NCBI_SCOPE;
#endif
