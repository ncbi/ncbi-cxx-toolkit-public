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

#ifndef VALIDATOR___SINGLE_FEAT_VALIDATOR__HPP
#define VALIDATOR___SINGLE_FEAT_VALIDATOR__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_autoinit.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>  // for CMappedFeat
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>

#include <objtools/validator/validator.hpp>
#include <objtools/validator/feature_match.hpp>
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
//class CDense_diag;
//class CDense_seg;
//class CSeq_align_set;
class CPubdesc;
class CBioSource;
class COrg_ref;
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

BEGIN_SCOPE(validator)

class CValidError_imp;
class CValidError_base;

// =============================  Validate SeqFeat  ============================


class CSingleFeatValidator
{
public:
    CSingleFeatValidator(const CSeq_feat& feat, CScope& scope, CValidError_imp& imp);
    virtual ~CSingleFeatValidator() {}

    virtual void Validate();

    static bool x_HasSeqLocBond(const CSeq_feat& feat);
    static bool s_IsPseudo(const CSeq_feat& feat);
    static bool s_IsPseudo(const CGene_ref& ref);
    static bool s_BioseqHasRefSeqThatStartsWithPrefix(CBioseq_Handle bsh, string prefix);
    static bool s_GeneRefsAreEquivalent(const CGene_ref& g1, const CGene_ref& g2, string& label);
    static void s_RemoveDuplicateGoTerms(CUser_object::TData& field_list);
    static void s_RemoveDuplicateGoTerms(CSeq_feat& feat);
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
    virtual bool x_ReportOrigProteinId();
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
    EDiagSev x_SeverityForConsensusSplice();
    void x_ReportSpliceProblems(const CSpliceProblems& problems, const string& label);
    void x_ReportDonorSpliceSiteReadErrors(const CSpliceProblems::TSpliceProblem& problem, const string& label);
    void x_ReportAcceptorSpliceSiteReadErrors(const CSpliceProblems::TSpliceProblem& problem, const string& label);

    static bool x_BioseqHasNmAccession (CBioseq_Handle bsh);

    void x_ValidateNonImpFeat();
    void x_ValidateGeneXRef();
    void x_ValidateGeneFeaturePair(const CSeq_feat& gene);
    void x_ValidateNonGene();
    void x_ValidateOldLocusTag(const string& old_locus_tag);

    void x_ValidateImpFeatLoc();
    void x_ValidateImpFeatQuals();
    void x_ValidateSeqFeatDataType();

    void x_ReportPseudogeneConflict(CConstRef <CSeq_feat> gene);
    void x_ValidateLocusTagGeneralMatch(CConstRef <CSeq_feat> gene);

    void x_CheckForNonAsciiCharacters();
};

class CCdregionValidator : public CSingleFeatValidator
{
public:
    CCdregionValidator(const CSeq_feat& feat, CScope& scope, CValidError_imp& imp);

    void Validate() override;

protected:
    void x_ValidateFeatComment() override;
    void x_ValidateExceptText(const string& text) override;
    void x_ValidateQuals();
    bool x_ReportOrigProteinId() override;
    static bool IsPlastid(int genome);
    void x_ValidateGeneticCode();
    void x_ValidateBadMRNAOverlap();
    bool x_HasGoodParent();
    void x_ValidateSeqFeatLoc() override;
    void x_ValidateFarProducts();
    void x_ValidateCDSPeptides();
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

    void x_ValidateProductPartials();
    void x_ValidateParentPartialness(const CSeq_loc& parent_loc, const string& parent_name);
    void x_ValidateParentPartialness();
    bool x_CheckPosNOrGap(TSeqPos pos, const CSeqVector& vec);

    CConstRef<CSeq_feat> m_Gene;
    bool m_GeneIsPseudo;
};


class CGeneValidator : public CSingleFeatValidator
{
public:
    using CSingleFeatValidator::CSingleFeatValidator;

    void Validate() override;

protected:
    void x_ValidateExceptText(const string& text) override;
    void x_ValidateOperon();
    void x_ValidateMultiIntervalGene();
    bool x_AllIntervalGapsAreMobileElements();
};


class CProtValidator : public CSingleFeatValidator
{
public:
    using CSingleFeatValidator::CSingleFeatValidator;

    void Validate() override;

protected:
    void x_CheckForEmpty();
    void x_ReportUninformativeNames();
    void x_ValidateECNumbers();
    void x_ValidateProteinName(const string& prot_name);
    void x_ValidateMolinfoPartials();
};


class CRNAValidator : public CSingleFeatValidator
{
public:
    CRNAValidator(const CSeq_feat& feat, CScope& scope, CValidError_imp& imp)
        : CSingleFeatValidator(feat, scope, imp) {}
    using CSingleFeatValidator::CSingleFeatValidator;

    void Validate() override;

protected:
    void x_ValidateRnaProduct(bool feat_pseudo, bool pseudo);
    void x_ValidateRnaProductType();
    void x_ReportRNATranslationProblems(size_t problems, size_t mismatches);
    void x_ValidateRnaTrans();

    // for tRNAs
    void x_ValidateAnticodon(const CSeq_loc& anticodon);
    void x_ValidateTrnaCodons();
    void x_ValidateTrnaType();
    void x_ValidateTrnaData();
    void x_ValidateTrnaOverlap();

};


class CMRNAValidator : public CRNAValidator
{
public:
    CMRNAValidator(const CSeq_feat& feat, CScope& scope, CValidError_imp& imp);

    void Validate() override;

protected:
    // for mRNAs
    void x_ValidateMrna();
    void x_ValidateCommonMRNAProduct();
    void x_ValidateMrnaGene();

    CConstRef<CSeq_feat> m_Gene;
    bool m_GeneIsPseudo;
    bool m_FeatIsPseudo;
};


class CPubFeatValidator : public CSingleFeatValidator
{
public:
    using CSingleFeatValidator::CSingleFeatValidator;

    void Validate() override;

protected:
};


class CSrcFeatValidator : public CSingleFeatValidator
{
public:
    using CSingleFeatValidator::CSingleFeatValidator;

    void Validate() override;

protected:
};


class CPolyASiteValidator : public CSingleFeatValidator
{
public:
    using CSingleFeatValidator::CSingleFeatValidator;

    void x_ValidateSeqFeatLoc() override;

protected:
};


class CPolyASignalValidator : public CSingleFeatValidator
{
public:
    using CSingleFeatValidator::CSingleFeatValidator;

    void x_ValidateSeqFeatLoc() override;

protected:
};


class CPeptideValidator : public CSingleFeatValidator
{
public:
    CPeptideValidator(const CSeq_feat& feat, CScope& scope, CValidError_imp& imp);

    void Validate() override;

protected:
    void x_ValidatePeptideOnCodonBoundary();

    CConstRef<CSeq_feat> m_CDS;
};



class CExonValidator : public CSingleFeatValidator
{
public:
    using CSingleFeatValidator::CSingleFeatValidator;

    void Validate() override;

protected:
};


class CIntronValidator : public CSingleFeatValidator
{
public:
    using CSingleFeatValidator::CSingleFeatValidator;

    void Validate() override;

protected:
    bool x_IsIntronShort(bool pseudo);
};


class CMiscFeatValidator : public CSingleFeatValidator
{
public:
    using CSingleFeatValidator::CSingleFeatValidator;

    void Validate() override;

protected:
};


class CAssemblyGapValidator : public CSingleFeatValidator
{
public:
    using CSingleFeatValidator::CSingleFeatValidator;

    void Validate() override;

protected:
};


class CGapFeatValidator : public CSingleFeatValidator
{
public:
    using CSingleFeatValidator::CSingleFeatValidator;

    void Validate() override;

protected:
};


class CImpFeatValidator : public CSingleFeatValidator
{
public:
    using CSingleFeatValidator::CSingleFeatValidator;
    void Validate() override;
protected:
};

CSingleFeatValidator* FeatValidatorFactory(const CSeq_feat& feat, CScope& scope, CValidError_imp& imp);


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* VALIDATOR___SINGLE_FEAT_VALIDATOR__HPP */
