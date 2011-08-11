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
*   Translate HGVS expression to Variation-ref seq-feats.
*   HGVS nomenclature rules: http://www.hgvs.org/mutnomen/
*
* ===========================================================================
*/

#ifndef VARIATION_UTL_HPP_
#define VARIATION_UTIL_HPP_

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
#include <objects/seqfeat/Variation_inst.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seq_vector.hpp>

#include <dbapi/dbapi.hpp>
#include <dbapi/driver/drivers.hpp>
#include <dbapi/driver/dbapi_driver_conn_params.hpp>
#include <objmgr/seq_loc_mapper.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

namespace variation_ref {

class CVariationUtil : public CObject
{
public:
    CVariationUtil(CScope& scope)
       : m_scope(&scope)
    {}

    enum ETestStatus
    {
        eFail,
        ePass,
        eNotApplicable,
    };

    /*!
     * If variation-feat is not intronic, or alignment is not spliced-seg -> eNotApplicable
     * Else if variation is intronic but location is not at exon boundary -> eFail
     * Else -> ePass
     */
    ETestStatus CheckExonBoundary(const CSeq_feat& variation_feat, const CSeq_align& aln);


    /*!
     * Given a variation-ref, find set(s) of type package, and add or overwrite a member variation
     * of type inst/observation=reference iff there are no intronic members for the package.
     */
    bool SetReferenceSequence(CVariation_ref& vr, const CSeq_loc& location);

    /*!
     * Create remapped copy of the variation feat via given alignment
     * The seq-locs that are
     * in scope for remapping: feat.location, variation-ref.location
     * and the recursive counterparts, Variation-inst.delta.seq.loc
     * where the id of the loc is also represented in the location
     * of the parent variation-ref.
     */
    CRef<CSeq_feat> Remap(const CSeq_feat& variation_feat, const CSeq_align& aln);


    /// Convert protein-variation (single-AA missense/nonsense) to nuc-variation on the parent.
    CRef<CSeq_feat> ProtToPrecursor(const CSeq_feat& prot_variation_feat);

    /// Convert to nuc-variation on the parent to protein-variation (single-AA missense/nonsense)
    /// Only a subset of sequence edits is supported (mismatch, ins, del, indel)
    /// The location must fall within annotated CDS
    CRef<CSeq_feat> PrecursorToProt(const CSeq_feat& prot_variation_feat);


    /// todo: implement when decide on representation in the schema
    void FlipStrand(CSeq_feat& variation_feat);

    /// Calculate upstream (first) and downstream(second) flanks for loc
    struct SFlankLocs
    {
        CRef<CSeq_loc> upstream;
        CRef<CSeq_loc> downstream;
    };
    SFlankLocs CreateFlankLocs(const CSeq_loc& loc, TSeqPos len);


    /*!
     * Convert any inst variation to delins form, if possible; throw if not.
     * Precondition: location must be set.
     */
    void ChangeToDelins(CVariation_ref& v);

    /*!
     * Extend or truncate delins to specified location.
     * truncate or attach suffixes/prefixes to seq-literals as necessary).
     *
     * Precondition: variation must be a normalized delins (via x_ChangeToDelins)
     */
    void AdjustDelinsToInterval(CVariation_ref& delins_variation, const CSeq_loc& int_loc);


    /*!
     * Set location of variation-sets as union of the children sub-variation locs.
     * If all children have the same location, their locations are reset
     * (recorded only at the parent variation level)
     */
    static void s_FactorOutLocsInPlace(CVariation_ref& v);

    /// Propagate parent variation location to the members of set, unles they have their own location set.
    /// (opposite of factor-out)
    static void s_PropagateLocsInPlace(CVariation_ref& v);


    /// Set location-specific variant-properties.
    void SetVariantProperties(CVariantProperties& prop, const CSeq_loc& orig_loc);

    /// Recursively iterate sub-variations: calculate location and variant properties for inst-type variations
    /// unless their observation-type is asserted or reference. Calculate location properties on factored locations.
    ///
    /// Precondition: root location must be set.
    void SetVariantProperties(CVariation_ref& vr);


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

private:

    void x_SetVariantProperties(CVariantProperties& p, const CVariation_inst& vi, const CSeq_loc& loc);
    void x_SetVariantPropertiesForIntronic(CVariantProperties& p, int offset, const CSeq_loc& loc, CBioseq_Handle& bsh);


    void x_ProtToPrecursor(CVariation_ref& v);

    //precondition: s_AddIntronicOffsets has been called as applicable
    static void s_Remap(CVariation_ref& vr, CSeq_loc_Mapper& mapper, const CSeq_loc& parent_variation_loc);

    //return iupacna or ncbieaa literals
    CRef<CSeq_literal> x_GetLiteralAtLoc(const CSeq_loc& loc);

    static CRef<CSeq_literal> s_CatLiterals(const CSeq_literal& a, const CSeq_literal& b);

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
    static void s_ResolveIntronicOffsets(CVariation_ref& v, const CSeq_loc& parent_variation_loc);

    /*!
     * If start|stop don't fall into an exon, adjust start|stop to the closest exon boundary and add offset to inst.
     * This is to be applied before remapping a genomic variation to transcript coordinates
     */
    static void s_AddIntronicOffsets(CVariation_ref& v, const CSpliced_seg& ss, const CSeq_loc& parent_variation_loc);

private:
    CRef<CScope> m_scope;
    static const int m_variant_properties_schema_version;
};

};
END_NCBI_SCOPE;
#endif
