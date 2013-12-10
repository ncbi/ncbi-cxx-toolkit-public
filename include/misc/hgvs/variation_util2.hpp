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
    enum EOptions
    {
        fOpt_cache_exon_sequence = 1, ///< Use when there will be many calls to calculate protein consequnece per sequence
        fOpt_default = 0
    };
    typedef int TOptions;

    CVariationUtil(CScope& scope, TOptions options = fOpt_default)
      : m_scope(&scope)
      , m_variant_properties_index(scope)
      , m_cdregion_index(scope, options)
    {}

    void ClearCache()
    {
        m_variant_properties_index.Clear();
        m_cdregion_index.Clear();
    }

    CRef<CVariation> AsVariation(const CSeq_feat& variation_ref);
    void AsVariation_feats(const CVariation& v, CSeq_annot::TData::TFtable& feats);


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


    /// Remap variation from product coordinates onto a nucleotide sequence on which this product is annotated.
    /// e.g. {NM,NR,NP} -> {NG,NT,NW,AC,NC}. Use annotated seq-align, if available; otherwise use spliced rna or cdregion.
    /// If starting from NP, remap to the parent mRNA first.
    CRef<CVariantPlacement> RemapToAnnotatedTarget(const CVariation& v, const CSeq_id& target);


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
    /// and attach results to nuc_variation.consequnece.
    /// If seq-id specified, use only the placement with the specified id.
    /// If ignore_genomic set to true, then consequences will be evaluated for cDNA or MT placements only.
    /// Note: Alternatively, the API could be "create and return consequence protein variation(s)", rather than attach,
    ///       but that for hierarchical input it would be hard to tell which consequence corresponds to which node.
    void AttachProteinConsequences(CVariation& nuc_variation, const CSeq_id* = NULL, bool ignore_genomic = false);


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

    enum { kMaxAttachSeqLen = 100000} ; //VAR-724
    /// If have offsets (intronic) or too long, return false;
    /// else set seq field on the placement and return true.
    bool AttachSeq(CVariantPlacement& p, TSeqPos max_len = kMaxAttachSeqLen);

    //Call AttachSeq on every placement; Try to find asserted-type subvariation.
    //If the asserted sequence is different from the attached reference, add 
    //corresponding exception object to the placement. Return true iff there were no exceptions added
    bool AttachSeq(CVariation& v, TSeqPos max_len = kMaxAttachSeqLen);


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

    // Similar to above, except verify exon boundary using exon annotation on refseq transcripts, if exists
    ETestStatus CheckExonBoundary(const CVariantPlacement& p); 

    /// if placement is invalid SeqLocCheck fails, or offsets out of order, attach VariationException and return true
    /// otherwise return false;
    bool CheckPlacement(CVariantPlacement& p);

    /// if variation.data contains a seq-literal with non-ACGT residues, attach VariationException to the first placement
    /// (if exists) and return true. otherwise return false;
    bool CheckAmbiguitiesInLiterals(CVariation& v);
    
    /// Calculate upstream (first) and downstream(second) flanks for loc
    struct SFlankLocs
    {
        CRef<CSeq_loc> upstream;
        CRef<CSeq_loc> downstream;
    };
    SFlankLocs CreateFlankLocs(const CSeq_loc& loc, TSeqPos len);


/// Methods to compute properties

    /*!
     * Fill location-specific variant-properties for a placement.
     * Attach related GeneID(s) to placement with respect to which the properties are defined.
     */
    void SetPlacementProperties(CVariantPlacement& placement);

    /*
     * Set placement-specific variant properties for a variation by applying
     * x_SetVariantProperties(...) on each found placement and collapse those
     * onto parent VariationProperties
     */
    void SetVariantProperties(CVariation& v);


    /// Supported SO-terms
    enum ESOTerm
    {
        //location-specific terms
        eSO_intergenic_variant      =1628,
        eSO_2KB_upstream_variant    =1636,
        eSO_500B_downstream_variant =1634,
        eSO_splice_donor_variant    =1575,
        eSO_splice_acceptor_variant =1574,
        eSO_intron_variant          =1627,
        eSO_5_prime_UTR_variant     =1623,
        eSO_3_prime_UTR_variant     =1624,
        eSO_coding_sequence_variant =1580,
        eSO_nc_transcript_variant   =1619,
        eSO_initiator_codon_variant =1582,
        eSO_terminator_codon_variant=1590,

        //variation-specific terms
        eSO_synonymous_variant      =1819,
        eSO_missense_variant        =1583,
        eSO_inframe_indel           =1820,
        eSO_frameshift_variant      =1589,
        eSO_stop_gained             =1587,
        eSO_stop_lost               =1578
    };
    typedef vector<ESOTerm> TSOTerms;
    void AsSOTerms(const CVariantProperties& p, TSOTerms& terms);
    static string AsString(ESOTerm term);


    /// Find location properties based on alignment.
    void FindLocationProperties(const CSeq_align& transcript_aln,
                                const CSeq_loc& query_loc,
                                TSOTerms& terms);


    static void s_FindLocationProperties(CConstRef<CSeq_loc> rna_loc,
                                         CConstRef<CSeq_loc> cds_loc,
                                         const CSeq_loc& query_loc,
                                         TSOTerms& terms);
                                    

    static TSeqPos s_GetLength(const CVariantPlacement& p, CScope* scope);

    /// If at any level in variation-set all variations have all same placements, move them to the parent level.
    static void s_FactorOutPlacements(CVariation& v);

    /*!
     * If this variation has placements defined, return it; otherwise, recursively try
     * parent all the way to the root; return NULL if nothing found.
     * Precondition: root Variation must have been indexed (links to parents defined as necessary)
     */
    static const CVariation::TPlacements* s_GetPlacements(const CVariation& v);


    /// Find attached consequence variation in v that corresponds to p (has same seq-id).
    static CConstRef<CVariation> s_FindConsequenceForPlacement(const CVariation& v, const CVariantPlacement& p);


    /// Length up to last position of the last exon (i.e. not including polyA)
    /// If neither is annotated, return normal length
    TSeqPos GetEffectiveTranscriptLength(const CBioseq_Handle& bsh);


    CRef<CSeq_literal> GetLiteralAtLoc(const CSeq_loc& loc);
private:


    CRef<CVariantPlacement> x_Remap(const CVariantPlacement& p, CSeq_loc_Mapper& mapper);

    void ChangeToDelins(CVariation& v);

    void x_SetVariantProperties(CVariantProperties& p, const CVariation_inst& vi, const CSeq_loc& loc);
    void x_SetVariantPropertiesForIntronic(CVariantPlacement& p, int offset, const CSeq_loc& loc, CBioseq_Handle& bsh);

    void x_ChangeToDelins(CVariation& v);

    void x_InferNAfromAA(CVariation& v, TAA2NAFlags flags);

    void x_TranslateNAtoAA(CVariation& prot_variation);

    CRef<CVariation> x_CreateUnknownVariation(const CSeq_id& id, CVariantPlacement::TMol mol);

    //return iupacna or ncbieaa literals
    CRef<CSeq_literal> x_GetLiteralAtLoc(const CSeq_loc& loc);

    CConstRef<CSeq_literal> x_FindOrCreateLiteral(const CVariation& v);

    /*
     * If we are flipping an insertion, we must verify that the location is a multinucleotide (normally dinucleotide)
     * (in this case insertion is interpreted as "insert-between", and can be strand-flipped).
     * It is impossible, however, to correctly flip the strand of insert-before-a-point variation, because
     * "before" would become "after".
     */
    static bool s_IsInstStrandFlippable(const CVariation& v, const CVariation_inst& inst);

    /// join two seq-literals
    static CRef<CSeq_literal> s_CatLiterals(const CSeq_literal& a, const CSeq_literal& b);

    /// insert seq-literal payload into ref before pos (pos=0 -> prepend; pos=ref.len -> append)
    static CRef<CSeq_literal> s_SpliceLiterals(const CSeq_literal& payload, const CSeq_literal& ref, TSeqPos pos);

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

    /*
     * Find a sibling variation with with observation=asserted;
     * if not found, recursively try parent, or return null if root.
     * Note: Assumes CVariation::Index has been called.
     */
    static const CConstRef<CSeq_literal> s_FindAssertedLiteral(const CVariation& v);


    CRef<CVariation> x_AsVariation(const CVariation_ref& vr);
    CRef<CVariation_ref> x_AsVariation_ref(const CVariation& v, const CVariantPlacement& p);


    /*
     * In variation-ref the intronic offsets are encoded as last and/or first inst delta-items.
     * In Variation the intronic offsets are part of the placement. After creating a variation
     * from a variation-ref we need to migrate the offsets from inst into placement.
     */
    static void s_ConvertInstOffsetsToPlacementOffsets(CVariation& v, CVariantPlacement& p);
    static void s_AddInstOffsetsFromPlacementOffsets(CVariation_inst& vi, const CVariantPlacement& p);


private:

    /// Cache seq-data in the CDS regions and the cds features by location
    class CCdregionIndex
    {
    public:
        struct SCdregion
        {
            CConstRef<CSeq_feat> cdregion_feat;
            bool operator<(const SCdregion& other) const
            {
                return !this->cdregion_feat ? false
                     : !other.cdregion_feat ? true
                     : *this->cdregion_feat < *other.cdregion_feat;
            }
        };
        typedef vector<SCdregion> TCdregions;

        CCdregionIndex(CScope& scope, TOptions options)
          : m_scope(&scope)
          , m_options(options)
        {}

        void Get(const CSeq_loc& loc, TCdregions& cdregions);

        //return NULL if not cached in its entirety
        CRef<CSeq_literal> GetCachedLiteralAtLoc(const CSeq_loc& loc);

        void Clear()
        {
            m_data.clear();
            m_seq_data_map.clear();
        }
    private:

        struct SSeqData
        {
            mutable CRef<CSeq_loc_Mapper> mapper; //will map original location consisting of merged exon locations on the seq_data coordinate system
            string seq_data;
        };
        typedef map<CSeq_id_Handle, SSeqData> TSeqDataMap;
        typedef CRangeMap<TCdregions, TSeqPos> TRangeMap;
        typedef map<CSeq_id_Handle, TRangeMap> TIdRangeMap;

        void x_Index(const CSeq_id_Handle& idh);
        void x_CacheSeqData(const CSeq_loc& loc, const CSeq_id_Handle& idh);

        CRef<CScope> m_scope;
        TIdRangeMap m_data;
        TSeqDataMap m_seq_data_map;
        TOptions m_options;
    };

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
        void Clear()
        {
            m_loc2prop.clear();
        }

        typedef pair<CRef<CSeq_loc>, CRef<CSeq_loc> > TLocsPair; //5' and 3' respectively
        static TLocsPair s_GetStartAndStopCodonsLocs(const CSeq_loc& cds_loc);
        static TLocsPair s_GetUTRLocs(const CSeq_loc& cds_loc, const CSeq_loc& parent_loc);
        static TLocsPair s_GetNeighborhoodLocs(const CSeq_loc& gene_loc, TSeqPos max_pos);
        static TLocsPair s_GetIntronsAndSpliceSiteLocs(const CSeq_loc& rna_loc);
        static int s_GetGeneID(const CMappedFeat& mf, feature::CFeatTree& ft);
        static int s_GetGeneIdForProduct(CBioseq_Handle bsh);

    private:
        void x_Index(const CSeq_id_Handle& idh);
        void x_Add(const CSeq_loc& loc, int gene_id, CVariantProperties::TGene_location prop);

        typedef CRangeMap<TGeneIDAndPropVector, TSeqPos> TRangeMap;
        typedef map<CSeq_id_Handle, TRangeMap> TIdRangeMap;

        CRef<CScope> m_scope;
        TIdRangeMap m_loc2prop;
    };


private:
    CRef<CScope> m_scope;
    CVariantPropertiesIndex m_variant_properties_index;
    CCdregionIndex m_cdregion_index;
};

};

END_NCBI_SCOPE;
#endif
