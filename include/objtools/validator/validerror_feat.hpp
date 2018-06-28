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
 *`
 * Author:  Colleen Bollin, Jonathan Kans, Clifford Clausen, Aaron Ucko......
 *
 * File Description:
 *   For validating individual features
 *   .......
 *
 */

#ifndef VALIDATOR___VALIDERROR_FEAT__HPP
#define VALIDATOR___VALIDERROR_FEAT__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_autoinit.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>  // for CMappedFeat
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>

#include <objtools/validator/validator.hpp>
#include <objtools/validator/utilities.hpp>
#include <objtools/validator/feature_match.hpp>
#include <objtools/validator/gene_cache.hpp>
#include <objtools/validator/validerror_base.hpp>
#include <objtools/validator/translation_problems.hpp>
#include <objtools/validator/splice_problems.hpp>

#include <objmgr/util/feature.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;
class CCit_sub;
class CCit_art;
class CCit_gen;
class CSeq_feat;
class CBioseq;
class CSeqdesc;
class CSeq_annot;
class CTrna_ext;
class CProt_ref;
class CSeq_loc;
class CFeat_CI;
class CPub_set;
class CAuth_list;
class CTitle;
class CMolInfo;
class CUser_object;
class CSeqdesc_CI;
class CSeq_graph;
class CMappedGraph;
class CDense_diag;
class CDense_seg;
class CSeq_align_set;
class CPubdesc;
class CBioSource;
class COrg_ref;
class CByte_graph;
class CDelta_seq;
class CGene_ref;
class CCdregion;
class CRNA_ref;
class CImp_feat;
class CSeq_literal;
class CBioseq_Handle;
class CSeq_feat_Handle;
class CCountries;
class CInferencePrefixList;
class CComment_set;
class CTaxon3_reply;
class ITaxon3;
class CT3Error;

BEGIN_SCOPE(validator)

class CValidError_imp;
class CTaxValidationAndCleanup;
class CGeneCache;
class CValidError_base;

// =============================  Validate SeqFeat  ============================



class NCBI_VALIDATOR_EXPORT CValidError_feat : private validator::CValidError_base
{
public:
    CValidError_feat(CValidError_imp& imp);
    virtual ~CValidError_feat(void);

    void SetScope(CScope& scope) { m_Scope = &scope; }
    void SetTSE(CSeq_entry_Handle seh);

    void ValidateSeqFeat(
        const CSeq_feat& feat);
    void ValidateSeqFeatWithParent(const CSeq_feat& feat, const CTSE_Handle& tse);
    void ValidateSeqFeatContext(const CSeq_feat& feat, const CBioseq& seq);

    enum EInferenceValidCode {
        eInferenceValidCode_valid = 0,
        eInferenceValidCode_empty,
        eInferenceValidCode_bad_prefix,
        eInferenceValidCode_bad_body,
        eInferenceValidCode_single_field,
        eInferenceValidCode_spaces,
        eInferenceValidCode_comment,
        eInferenceValidCode_same_species_misused,
        eInferenceValidCode_bad_accession,
        eInferenceValidCode_bad_accession_version,
        eInferenceValidCode_accession_version_not_public,
        eInferenceValidCode_bad_accession_type,
        eInferenceValidCode_unrecognized_database
    };

    static vector<string> GetAccessionsFromInferenceString (string inference, string &prefix, string &remainder, bool &same_species);
    static bool GetPrefixAndAccessionFromInferenceAccession (string inf_accession, string &prefix, string &accession);
    static EInferenceValidCode ValidateInferenceAccession (string accession, bool fetch_accession, bool is_similar_to);
    static EInferenceValidCode ValidateInference(string inference, bool fetch_accession);

    // functions expected to be used in Discrepancy Report
    bool DoesCDSHaveShortIntrons(const CSeq_feat& feat);
    bool IsIntronShort(const CSeq_feat& feat);
    void ValidatemRNAGene (const CSeq_feat &feat);
    bool GetTSACDSOnMinusStrandErrors(const CSeq_feat& feat, const CBioseq& seq);

    static bool IsPlastid(int genome);

    static bool x_FindProteinGeneXrefByKey(CBioseq_Handle bsh, const string& key);
    static bool x_FindGeneToMatchGeneXref(const CGene_ref& xref, CSeq_entry_Handle seh);

private:

    CSeq_entry_Handle  m_TSE;
    CGeneCache         m_GeneCache;
    CCacheImpl         m_SeqCache;

    CBioseq_Handle x_GetCachedBsh(const CSeq_loc& loc);

    void ValidateSeqFeatData(const CSeqFeatData& data, const CSeq_feat& feat);
    void ValidateGene(const CGene_ref& gene, const CSeq_feat& feat);
    void ValidateGeneXRef(const CSeq_feat& feat);
    void ValidateGeneFeaturePair(const CSeq_feat& feat, const CSeq_feat& gene);
//    void ValidateOperon(const CSeq_feat& feat);

    void ValidateCdregion(const CCdregion& cdregion, const CSeq_feat& obj);
#if 0
    void ValidateCdTrans(const CSeq_feat& feat, bool &nonsense_intron);
#endif



    void x_ReportTranslExceptProblems(const CCDSTranslationProblems::TTranslExceptProblems& problems, const CSeq_feat& feat, bool has_exception);
    void x_ReportCDSTranslationProblems(const CSeq_feat& feat, const CCDSTranslationProblems& problems, bool far_product);

    CBioseq_Handle x_GetCDSProduct(const CSeq_feat& feat, bool& is_far);
    CBioseq_Handle x_GetRNAProduct(const CSeq_feat& feat, bool& is_far);
    CBioseq_Handle x_GetFeatureProduct(const CSeq_feat& feat, bool look_far, bool& is_far, bool& is_misplaced);
    CBioseq_Handle x_GetFeatureProduct(const CSeq_feat& feat, bool& is_far, bool& is_misplaced);

    void x_ReportTranslationMismatches(const CCDSTranslationProblems::TTranslationMismatches& mismatches, const CSeq_feat& feat, bool far_product);

#if 0
    void ValidateCdsProductId(const CSeq_feat& feat);
    void ValidateCdConflict(const CCdregion& cdregion, const CSeq_feat& feat);
#endif

#if 0
    EDiagSev x_SeverityForConsensusSplice(void);
    void ValidateSplice(const CSeq_feat& feat, bool check_all = false);
#endif
    static void x_FeatLocHasBadStrandBoth(const CSeq_feat& feat, bool& both, bool& both_rev);
#if 0
    void ValidateCommonCDSProduct(const CSeq_feat& feat);

    void x_ValidateCdregionCodebreak(const CSeq_feat& feat);
#endif

    void ValidateProt(const CProt_ref& prot, const CSeq_feat& feat);
#if 0
    void x_ValidateProteinName(const string& prot_name, const CSeq_feat& feat);
    void x_ReportUninformativeNames(const CProt_ref& prot, const CSeq_feat& feat);
    void x_ValidateProtECNumbers(const CProt_ref& prot, const CSeq_feat& feat);
#endif

    void ValidateRna(const CRNA_ref& rna, const CSeq_feat& feat);
#if 0
    void ValidateAnticodon(const CSeq_loc& anticodon, const CSeq_feat& feat);
    void ValidateTrnaCodons(const CTrna_ext& trna, const CSeq_feat& feat, bool mustbemethionine);
    void ValidateMrnaTrans(const CSeq_feat& feat);
    void ReportMRNATranslationProblems(size_t problems, const CSeq_feat& feat, size_t mismatches);

    void ValidateCommonMRNAProduct(const CSeq_feat& feat);
    void ValidateRnaProductType(const CRNA_ref& rna, const CSeq_feat& feat);
#endif
    void ValidateIntron(const CSeq_feat& feat);

    void ValidateImp(const CImp_feat& imp, const CSeq_feat& feat);
#if 0
    void ValidateNonImpFeat (const CSeq_feat& feat);
#endif

    void ValidateGapFeature (const CSeq_feat& feat);

    void ValidatePeptideOnCodonBoundry(const CSeq_feat& feat, 
        const string& key);

    void ValidateSeqFeatXref(const CSeq_feat& feat);
    void ValidateSeqFeatXref(const CSeq_feat& feat, const CTSE_Handle& tse);
    void ValidateSeqFeatXref (const CSeqFeatXref& xref, const CSeq_feat& feat);
    void ValidateSeqFeatXref(const CSeqFeatXref& xref, const CSeq_feat& feat, const CTSE_Handle& tse);
    void x_ValidateSeqFeatExceptXref(const CSeq_feat& feat);

    // does feat have an xref to a feature other than the one specified by id with the same subtype
    static bool HasNonReciprocalXref(const CSeq_feat& feat, 
                                     const CFeat_id& id, CSeqFeatData::ESubtype subtype,
                                     const CTSE_Handle& tse);
    void ValidateOneFeatXrefPair(const CSeq_feat& feat, const CSeq_feat& far_feat, const CSeqFeatXref& xref);

#if 0
    void ValidateFeatCit(const CPub_set& cit, const CSeq_feat& feat);
#endif
    void ValidateFeatBioSource(const CBioSource& bsrc, const CSeq_feat& feat);

    bool IsOverlappingGenePseudo(const CSeq_feat& feat, CScope* scope);

    string MapToNTCoords(const CSeq_feat& feat, const CSeq_loc& product,
        TSeqPos pos);

#if 0
    void ValidateCharactersInField (string value, string field_name, const CSeq_feat& feat);
    void x_ReportECNumFileStatus(const CSeq_feat& feat);

    void ReportSpliceProblems(const CSpliceProblems& problems, const string& label, const CSeq_feat& feat);
    void ReportDonorSpliceSiteReadErrors(const CSpliceProblems::TSpliceProblem& problem, const string& label, const CSeq_feat& feat);
    void ReportAcceptorSpliceSiteReadErrors(const CSpliceProblems::TSpliceProblem& problem, const string& label, const CSeq_feat& feat);
#endif

    bool x_HasNonReciprocalXref(const CSeq_feat& feat, const CFeat_id& id, CSeqFeatData::ESubtype subtype);
    bool x_LocIsNmAccession(const CSeq_loc& loc);
    void x_ReportMisplacedCodingRegionProduct(const CSeq_feat& feat);

};


class CSingleFeatValidator
{
public:
    CSingleFeatValidator(const CSeq_feat& feat, CScope& scope, CValidError_imp& imp);
    virtual ~CSingleFeatValidator() {};

    virtual void Validate();

    static bool x_HasSeqLocBond(const CSeq_feat& feat);

protected:
    const CSeq_feat& m_Feat;
    CScope& m_Scope;
    CValidError_imp& m_Imp;
    CBioseq_Handle m_LocationBioseq;
    CBioseq_Handle m_ProductBioseq;
    bool m_ProductIsFar;

    void PostErr(EDiagSev sv, EErrType et, const string& msg);

    CBioseq_Handle x_GetBioseqByLocation(const CSeq_loc& loc);
    void x_ValidateSeqFeatProduct();
    void x_ValidateBothStrands();
    static void x_LocHasStrandBoth(const CSeq_loc& feat, bool& both, bool& both_rev);
    void x_ValidateGeneId();
    void x_ValidateFeatCit();
    virtual void x_ValidateFeatComment();
    void x_ValidateGbQual(const CGb_qual& qual);
    void x_ReportECNumFileStatus();

    void x_ValidateExtUserObject();
    void x_ValidateGoTerms(CUser_object::TData field_list, vector<pair<string, string> >& id_terms);

    bool x_HasNamedQual(const string& qual_name);

    void x_ValidateFeatPartialness();
    virtual void x_ValidateSeqFeatLoc();
    bool x_AllowFeatureToMatchGapExactly();

    typedef enum {
        eLocationGapNoProblems = 0,
        eLocationGapFeatureMatchesGap = 1,
        eLocationGapContainedInGap = 4,
        eLocationGapContainedInGapOfNs = 8,
        eLocationGapInternalIntervalEndpointInGap = 16,
        eLocationGapCrossesUnknownGap = 32,
        eLocationGapMostlyNs = 64

    } ELocationGap;

    static size_t x_CalculateLocationGaps(CBioseq_Handle bsh, const CSeq_loc& loc, vector<TSeqPos>& gap_starts);
    static bool x_IsMostlyNs(const CSeq_loc& loc, CBioseq_Handle bsh);
    static size_t x_FindStartOfGap(CBioseq_Handle bsh, int pos, CScope* scope);

    void x_ValidateExcept();
    virtual void x_ValidateExceptText(const string& text);

    void x_ValidateGbquals();
    void x_ValidateRptUnitVal(const string& val, const string& key);
    void x_ValidateRptUnitSeqVal(const string& val, const string& key);
    void x_ValidateRptUnitRangeVal(const string& val);
    void x_ValidateLabelVal(const string& val);
    void x_ValidateCompareVal(const string& val);
    void x_ValidateReplaceQual(const string& key, const string& qual_str, const string& val);

    CBioseq_Handle x_GetFeatureProduct(bool look_far, bool& is_far);
    CBioseq_Handle x_GetFeatureProduct(bool& is_far);

    void ValidateCharactersInField (string value, string field_name);
    void ValidateSplice(bool gene_pseudo, bool check_all);
    EDiagSev x_SeverityForConsensusSplice(void);
    void x_ReportSpliceProblems(const CSpliceProblems& problems, const string& label);
    void x_ReportDonorSpliceSiteReadErrors(const CSpliceProblems::TSpliceProblem& problem, const string& label);
    void x_ReportAcceptorSpliceSiteReadErrors(const CSpliceProblems::TSpliceProblem& problem, const string& label);

    static bool x_BioseqHasNmAccession (CBioseq_Handle bsh);

    void x_ValidateNonImpFeat();
    void x_ValidateGeneXRef();
    void x_ValidateGeneFeaturePair(const CSeq_feat& gene);
    void x_ValidateNonGene();
    void x_ValidateOldLocusTag(const string& old_locus_tag);
};

class CCdregionValidator : public CSingleFeatValidator
{
public:
    CCdregionValidator(const CSeq_feat& feat, CScope& scope, CValidError_imp& imp);

    virtual void Validate();

protected:
    virtual void x_ValidateFeatComment();
    virtual void x_ValidateExceptText(const string& text);
    void x_ValidateQuals();
    void x_ValidateGeneticCode();
    void x_ValidateBadMRNAOverlap();
    bool x_HasGoodParent();
    virtual void x_ValidateSeqFeatLoc();
    void x_ValidateFarProducts();
    void x_ValidateCDSPartial();
    bool x_BypassCDSPartialTest() const;
    bool x_CDS3primePartialTest() const;
    bool x_CDS5primePartialTest() const;

    bool x_IsProductMisplaced() const;

    typedef pair<TSeqPos, TSeqPos> TShortIntron;
    static vector<TShortIntron> x_GetShortIntrons(const CSeq_loc& loc, CScope* scope);
    static void x_AddToIntronList(vector<TShortIntron>& shortlist, TSeqPos last_start, TSeqPos last_stop, TSeqPos this_start, TSeqPos this_stop);
    static string x_FormatIntronInterval(const TShortIntron& interval);
    void ReportShortIntrons();

    void x_ValidateTrans();
    void x_ValidateCodebreak();
    void x_ReportTranslationProblems(const CCDSTranslationProblems& problems);
    void x_ReportTranslExceptProblems(const CCDSTranslationProblems::TTranslExceptProblems& problems, bool has_exception);
    void x_ReportTranslationMismatches(const CCDSTranslationProblems::TTranslationMismatches& mismatches);
    string MapToNTCoords(TSeqPos pos);

    void x_ValidateProductId();
    void x_ValidateConflict();
    void x_ValidateCommonProduct();

    CConstRef<CSeq_feat> m_Gene;
    bool m_GeneIsPseudo;
};


class CGeneValidator : public CSingleFeatValidator
{
public:
    CGeneValidator(const CSeq_feat& feat, CScope& scope, CValidError_imp& imp) :
        CSingleFeatValidator(feat, scope, imp) {}

    virtual void Validate();

protected:
    virtual void x_ValidateExceptText(const string& text);
    void x_ValidateOperon();
};


class CProtValidator : public CSingleFeatValidator
{
public:
    CProtValidator(const CSeq_feat& feat, CScope& scope, CValidError_imp& imp) :
        CSingleFeatValidator(feat, scope, imp) {}

    virtual void Validate();

protected:
    void x_CheckForEmpty();
    void x_ReportUninformativeNames();
    void x_ValidateECNumbers();
    void x_ValidateProteinName(const string& prot_name);
};


class CRNAValidator : public CSingleFeatValidator
{
public:
    CRNAValidator(const CSeq_feat& feat, CScope& scope, CValidError_imp& imp) :
        CSingleFeatValidator(feat, scope, imp) {}

    virtual void Validate();

protected:
    // for tRNAs
    void x_ValidateAnticodon(const CSeq_loc& anticodon);
    void x_ValidateTrnaCodons();
    void x_ValidateTrnaType();
    void x_ValidateTrnaData();

    // for mRNAs
    void x_ValidateMrna(bool pseudo);
    void x_ValidateCommonMRNAProduct();
    void x_ReportMRNATranslationProblems(size_t problems, size_t mismatches);
    void x_ValidateRnaTrans();
    void x_ValidateRnaProduct(bool feat_pseudo, bool pseudo);
    void x_ValidateRnaProductType();
    void x_ValidateTrnaOverlap();
};


class CExonValidator : public CSingleFeatValidator
{
public:
    CExonValidator(const CSeq_feat& feat, CScope& scope, CValidError_imp& imp) :
        CSingleFeatValidator(feat, scope, imp) {}

    virtual void Validate();

protected:
};


class CIntronValidator : public CSingleFeatValidator
{
public:
    CIntronValidator(const CSeq_feat& feat, CScope& scope, CValidError_imp& imp) :
        CSingleFeatValidator(feat, scope, imp) {}

    virtual void Validate();

protected:
    bool x_IsIntronShort(bool pseudo);
};


CSingleFeatValidator* FeatValidatorFactory(const CSeq_feat& feat, CScope& scope, CValidError_imp& imp);


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* VALIDATOR___VALIDERROR_FEAT__HPP */
