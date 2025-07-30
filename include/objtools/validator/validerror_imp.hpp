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
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko......
 *
 * File Description:
 *   Privae classes and definition for the validator
 *   .......
 *
 */

#ifndef VALIDATOR___VALIDERROR_IMP__HPP
#define VALIDATOR___VALIDERROR_IMP__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_autoinit.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>  // for CMappedFeat
#include <objmgr/util/seq_loc_util.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/GIBB_mol.hpp>
#include <objects/misc/sequence_macros.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqalign/Packed_seg.hpp>
#include <objects/valid/Comment_set.hpp>
#include <objects/valid/Comment_rule.hpp>
#include <objects/taxon3/taxon3.hpp>

#include <objtools/validator/tax_validation_and_cleanup.hpp>
#include <objtools/validator/utilities.hpp>
#include <objtools/validator/feature_match.hpp>
#include <objtools/validator/cache_impl.hpp>
#include <objtools/validator/gene_cache.hpp>
#include <objtools/validator/entry_info.hpp>

#include <objtools/alnmgr/sparse_aln.hpp>

#include <objmgr/util/create_defline.hpp>

#include <objmgr/util/feature.hpp>
#include <memory>

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


struct SValidatorContext;
class CValidError_desc;
class CValidError_descr;
class CTaxValidationAndCleanup;


// ===========================  Central Validation  ==========================

// CValidError_imp provides the entry point to the validation process.
// It calls upon the various validation classes to perform validation of
// each part.
// The class holds all the data for the validation process.
class NCBI_VALIDATOR_EXPORT CValidError_imp
{
public:
    typedef map<CSubSource::TSubtype, int> TCount;

    CValidError_imp(CObjectManager& objmgr,
            shared_ptr<SValidatorContext> pContext,
            IValidError* errors,
            Uint4 options=0);

    // Destructor
    virtual ~CValidError_imp();

    void SetOptions(Uint4 options);
    void SetErrorRepository(IValidError* errors);
    void Reset(size_t initialInferenceCount, bool notJustLocalOrGeneral, bool hasRefSeq);

    // Validation methods
    bool Validate(const CSeq_entry& se, const CCit_sub* cs = nullptr,
                  CScope* scope = nullptr);
    bool Validate(
        const CSeq_entry_Handle& seh, const CCit_sub* cs = nullptr);
    void Validate(
        const CSeq_submit& ss, CScope* scope = nullptr);
    void Validate(const CSeq_annot_Handle& sa);

    void Validate(const CSeq_feat& feat, CScope* scope = nullptr);
    void Validate(const CBioSource& src, CScope* scope = nullptr);
    void Validate(const CPubdesc& pubdesc, CScope* scope = nullptr);
    void Validate(const CSeqdesc& desc, const CSeq_entry& ctx);
    void ValidateSubAffil(const CAffil::TStd& std, const CSerialObject& obj, const CSeq_entry *ctx);
    void ValidateAffil(const CAffil::TStd& std, const CSerialObject& obj, const CSeq_entry *ctx);

    bool GetTSANStretchErrors(const CSeq_entry_Handle& se);
    bool GetTSACDSOnMinusStrandErrors (const CSeq_entry_Handle& se);
    bool GetTSAConflictingBiomolTechErrors (const CSeq_entry_Handle& se);
    bool GetTSANStretchErrors(const CBioseq& seq);
    bool GetTSACDSOnMinusStrandErrors (const CSeq_feat& f, const CBioseq& seq);
    bool GetTSAConflictingBiomolTechErrors (const CBioseq& seq);


    void SetProgressCallback(CValidator::TProgressCallback callback,
        void* user_data);

    void SetTSE(const CSeq_entry_Handle& seh);

    bool ShouldSubdivide() const { if (m_NumTopSetSiblings > 1000) return true; else return false; }

    SValidatorContext& SetContext();
    const SValidatorContext& GetContext() const;

    bool IsHugeFileMode() const;
    bool IsHugeSet(const CBioseq_set& bioseqSet) const;
    bool IsHugeSet(CBioseq_set::TClass setClass) const;

public:
    // interface to be used by the various validation classes

    // typedefs:
    typedef const CSeq_feat& TFeat;
    typedef const CBioseq& TBioseq;
    typedef const CBioseq_set& TSet;
    typedef const CSeqdesc& TDesc;
    typedef const CSeq_annot& TAnnot;
    typedef const CSeq_graph& TGraph;
    typedef const CSeq_align& TAlign;
    typedef const CSeq_entry& TEntry;
    typedef map < const CSeq_feat*, const CSeq_annot* >& TFeatAnnotMap;


    const CValidatorEntryInfo& GetEntryInfo() const;

    // Posts errors.
    void PostErr(EDiagSev sv, EErrType et, const string& msg,
        const CSerialObject& obj);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TDesc ds);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TFeat ft);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TBioseq sq);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TEntry ctx,
        TDesc ds);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TSet set);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TAnnot annot);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TGraph graph);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TBioseq sq,
        TGraph graph);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TAlign align);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, TEntry entry);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, const CBioSource& src);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, const COrg_ref& org);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, const CPubdesc& src);
    void PostErr(EDiagSev sv, EErrType et, const string& msg, const CSeq_submit& ss);
    void PostObjErr (EDiagSev sv, EErrType et, const string& msg, const CSerialObject& obj, const CSeq_entry *ctx = nullptr);
    void PostBadDateError (EDiagSev sv, const string& msg, int flags, const CSerialObject& obj, const CSeq_entry *ctx = nullptr);

    void HandleTaxonomyError(const CT3Error& error, const string& host, const COrg_ref& orf);
    void HandleTaxonomyError(const CT3Error& error, const EErrType type, const CSeq_feat& feat);
    void HandleTaxonomyError(const CT3Error& error, const EErrType type, const CSeqdesc& desc, const CSeq_entry* entry);

    bool RaiseGenomeSeverity(EErrType et);

    // General use validation methods
    void ValidatePubdesc(const CPubdesc& pub, const CSerialObject& obj, const CSeq_entry *ctx = nullptr);
    void ValidateBioSource(const CBioSource& bsrc, const CSerialObject& obj, const CSeq_entry *ctx = nullptr);
    void ValidatePCRReactionSet(const CPCRReactionSet& pcrset, const CSerialObject& obj, const CSeq_entry *ctx = nullptr);
    void ValidateSubSource(const CSubSource& subsrc, const CSerialObject& obj, const CSeq_entry *ctx = nullptr, const bool isViral = false, const bool isInfluenzaOrSars2 = false);
    void ValidateOrgRef(const COrg_ref& orgref, const CSerialObject& obj, const CSeq_entry *ctx, const bool checkForUndefinedSpecies = false, const bool is_single_cell_amplification = false);
    void ValidateTaxNameOrgname(const string& taxname, const COrgName& orgname, const CSerialObject& obj, const CSeq_entry *ctx);
    void ValidateOrgName(const COrgName& orgname, const bool has_taxon, const CSerialObject& obj, const CSeq_entry *ctx);
    void ValidateOrgModVoucher(const COrgMod& orgmod, const CSerialObject& obj, const CSeq_entry *ctx);
    void ValidateBioSourceForSeq(const CBioSource& bsrc, const CSerialObject& obj, const CSeq_entry *ctx, const CBioseq_Handle& bsh);

    void ValidateLatLonCountry(string countryname, string lat_lon, const CSerialObject& obj, const CSeq_entry *ctx);

    bool IsSyntheticConstruct (const CBioSource& src);
    bool IsArtificial (const CBioSource& src);
    bool IsOtherDNA(const CBioseq_Handle& bsh) const;
    void ValidateSeqLoc(const CSeq_loc& loc, const CBioseq_Handle& seq, bool report_abutting,
                        const string& prefix, const CSerialObject& obj, bool lowerSev = false);

    void ValidateSeqLocIds(const CSeq_loc& loc, const CSerialObject& obj);
    NCBI_STD_DEPRECATED("Please use corresponding function in objtools/validator/utilities.hpp") 
        static bool IsInOrganelleSmallGenomeSet(const CSeq_id& id, CScope& scope);
    NCBI_STD_DEPRECATED("Please use corresponding function in objtools/validator/utilities.hpp") 
    static bool BadMultipleSequenceLocation(const CSeq_loc& loc, CScope& scope);
    void CheckMultipleIds(const CSeq_loc& loc, const CSerialObject& obj);
    void ValidateDbxref(const CDbtag& xref, const CSerialObject& obj,
    bool biosource = false, const CSeq_entry *ctx = nullptr);
    void ValidateDbxref(TDbtags& xref_list, const CSerialObject& obj,
    bool biosource = false, const CSeq_entry *ctx = nullptr);
    void ValidateBadNameStd ( const CName_std& nstd, const CSerialObject& obj, const CSeq_entry* ctx = nullptr );
    void ValidateBadAffil ( const CAffil::TStd& astd, const CSerialObject& obj, const CSeq_entry* ctx = nullptr );
    void ValidateCitSub(const CCit_sub& cs, const CSerialObject& obj, const CSeq_entry *ctx = nullptr);
    void ValidateTaxonomy(const CSeq_entry& se);
    void ValidateOrgRefs(CTaxValidationAndCleanup& tval);
    void ValidateSpecificHost(CTaxValidationAndCleanup& tval);
    void ValidateStrain(CTaxValidationAndCleanup& tval, TTaxId descTaxID = ZERO_TAX_ID);
    void ValidateSpecificHost (const CSeq_entry& se);
    void ValidateTentativeName(const CSeq_entry& se);
    void ValidateTaxonomy(const COrg_ref& org, CBioSource::TGenome genome = CBioSource::eGenome_unknown);
    void ValidateMultipleTaxIds(const CSeq_entry_Handle& seh);
    void ValidateCitations (const CSeq_entry_Handle& seh);
    bool x_IsFarFetchFailure (const CSeq_loc& loc);

    // getters
    inline CScope* GetScope() { return m_Scope; }
    inline CCacheImpl& GetCache() { return m_cache; }

    inline CConstRef<CSeq_feat> GetCachedGene(const CSeq_feat* f) { return m_GeneCache.GetGeneFromCache(f, *m_Scope); }
    inline CGeneCache& GetGeneCache() { return m_GeneCache; }

    // flags derived from options parameter
    bool IsNonASCII()             const { return m_NonASCII; }
    bool IsSuppressContext()      const { return m_SuppressContext; }
    bool IsValidateAlignments()   const { return m_ValidateAlignments; }
    bool IsValidateExons()        const { return m_ValidateExons; }
    bool IsOvlPepErr()            const { return m_OvlPepErr; }
    bool IsRequireTaxonID()       const { return !m_SeqSubmitParent; }
    bool IsSeqSubmitParent()      const { return m_SeqSubmitParent; }
    bool IsRequireISOJTA()        const { return m_RequireISOJTA; }
    bool IsValidateIdSet()        const { return m_ValidateIdSet; }
    bool IsRemoteFetch()          const { return m_RemoteFetch; }
    bool IsFarFetchMRNAproducts() const { return m_FarFetchMRNAproducts; }
    bool IsFarFetchCDSproducts()  const { return m_FarFetchCDSproducts; }
    bool IsLocusTagGeneralMatch() const { return m_LocusTagGeneralMatch; }
    bool DoRubiscoTest()          const { return m_DoRubiscoText; }
    bool IsIndexerVersion()       const { return m_IndexerVersion; }
    bool IsGenomeSubmission()     const { return m_genomeSubmission; }
    bool UseEntrez()              const { return m_UseEntrez; }
    bool DoTaxLookup()            const { return m_DoTaxLookup; }
    bool ValidateInferenceAccessions() const { return m_ValidateInferenceAccessions; }
    bool IgnoreExceptions()       const { return m_IgnoreExceptions; }
    bool ReportSpliceAsError()    const { return m_ReportSpliceAsError; }
    bool IsLatLonCheckState()     const { return m_LatLonCheckState; }
    bool IsLatLonIgnoreWater()    const { return m_LatLonIgnoreWater; }
    bool IsRefSeqConventions()    const { return m_RefSeqConventions; }
    bool GenerateGoldenFile()     const { return m_GenerateGoldenFile; }
    bool DoCompareVDJCtoCDS()     const { return m_CompareVDJCtoCDS; }
    bool IgnoreInferences()       const { return m_IgnoreInferences; }
    bool ForceInferences()        const { return m_ForceInferences; }
    bool NewStrainValidation()    const { return m_NewStrainValidation; }


    // flags calculated by examining data in record
    bool IsStandaloneAnnot() const { return m_IsStandaloneAnnot; }
    bool IsNoPubs() const;
    bool IsNoCitSubPubs() const;
    bool IsNoBioSource() const;
    bool IsGPS() const;
    bool IsGED() const;
    bool IsPDB() const;
    bool IsPatent() const;
    bool IsRefSeq() const;
    bool IsEmbl() const;
    bool IsDdbj() const;
    bool IsTPE() const;
    NCBI_DEPRECATED bool IsNC() const;
    NCBI_DEPRECATED bool IsNG() const;
    NCBI_DEPRECATED bool IsNM() const;
    NCBI_DEPRECATED bool IsNP() const;
    NCBI_DEPRECATED bool IsNR() const;
    NCBI_DEPRECATED bool IsNZ() const;
    NCBI_DEPRECATED bool IsNS() const;
    bool IsNT() const;
    NCBI_DEPRECATED bool IsNW() const;
    bool IsWP() const;
    bool IsXR() const;
    bool IsGI() const;
    bool IsGpipe() const;
    bool IsHtg() const;
    bool IsLocalGeneralOnly() const;
    bool HasGiOrAccnVer() const;
    bool IsGenomic() const;
    bool IsSeqSubmit() const;
    bool IsSmallGenomeSet() const;
    bool IsNoncuratedRefSeq(const CBioseq& seq, EDiagSev& sev);
    bool IsGenbank() const;
    bool DoesAnyFeatLocHaveGI() const;
    bool DoesAnyProductLocHaveGI() const;
    bool DoesAnyGeneHaveLocusTag() const;
    bool DoesAnyProteinHaveGeneralID() const;
    bool IsINSDInSep() const;
    bool IsGeneious() const;
    const CBioSourceKind& BioSourceKind() const;
    inline bool HasRefSeq(void) const { return m_HasRefSeq; }

    // counting number of misplaced features
    inline void ResetMisplacedFeatureCount() { m_NumMisplacedFeatures = 0; }
    inline void IncrementMisplacedFeatureCount() { m_NumMisplacedFeatures++; }
    inline void AddToMisplacedFeatureCount(size_t num) { m_NumMisplacedFeatures += num; }

    // counting number of small genome set misplaced features
    inline void ResetSmallGenomeSetMisplacedCount() { m_NumSmallGenomeSetMisplaced = 0; }
    inline void IncrementSmallGenomeSetMisplacedCount() { m_NumSmallGenomeSetMisplaced++; }
    inline void AddToSmallGenomeSetMisplacedCount(size_t num) { m_NumSmallGenomeSetMisplaced += num; }

    // counting number of misplaced graphs
    inline void ResetMisplacedGraphCount() { m_NumMisplacedGraphs = 0; }
    inline void IncrementMisplacedGraphCount() { m_NumMisplacedGraphs++; }
    inline void AddToMisplacedGraphCount(size_t num) { m_NumMisplacedGraphs += num; }

    // counting number of genes and gene xrefs
    inline void ResetGeneCount() { m_NumGenes = 0; }
    inline void IncrementGeneCount() { m_NumGenes++; }
    inline void AddToGeneCount(size_t num) { m_NumGenes += num; }
    inline size_t GetGeneCount(void) const { return m_NumGenes; }
    inline void ResetGeneXrefCount() { m_NumGeneXrefs = 0; }
    inline void IncrementGeneXrefCount() { m_NumGeneXrefs++; }
    inline void AddToGeneXrefCount(size_t num) { m_NumGeneXrefs += num; }
    inline size_t GetGeneXrefCount(void) const { return m_NumGeneXrefs; }

    // counting cumulative number of inference qualifiers with accessions
    inline void ResetCumulativeInferenceCount() { m_CumulativeInferenceCount = 0; }
    inline void IncrementCumulativeInferenceCount() { m_CumulativeInferenceCount++; }
    inline void AddToCumulativeInferenceCount(size_t num) { m_CumulativeInferenceCount += num; }
    inline size_t GetCumulativeInferenceCount(void) const { return m_CumulativeInferenceCount; }

    // counting sequences with and without TPA history
    inline void ResetTpaWithHistoryCount() { m_NumTpaWithHistory = 0; }
    inline void IncrementTpaWithHistoryCount() { m_NumTpaWithHistory++; }
    inline void AddToTpaWithHistoryCount(size_t num) { m_NumTpaWithHistory += num; }
    inline void ResetTpaWithoutHistoryCount() { m_NumTpaWithoutHistory = 0; }
    inline void IncrementTpaWithoutHistoryCount() { m_NumTpaWithoutHistory++; }
    inline void AddToTpaWithoutHistoryCount(size_t num) { m_NumTpaWithoutHistory += num; }

    // counting number of Pseudos and Pseudogenes
    inline void ResetPseudoCount() { m_NumPseudo = 0; }
    inline void IncrementPseudoCount() { m_NumPseudo++; }
    inline void AddToPseudoCount(size_t num) { m_NumPseudo += num; }
    inline void ResetPseudogeneCount() { m_NumPseudogene = 0; }
    inline void IncrementPseudogeneCount() { m_NumPseudogene++; }
    inline void AddToPseudogeneCount(size_t num) { m_NumPseudogene += num; }

    // set flag for farfetchfailure
    inline void SetFarFetchFailure() { m_FarFetchFailure = true; }

    bool IsFarSequence(const CSeq_id& id); // const;

    const CSeq_entry& GetTSE() const { return *m_TSE; };
    const CSeq_entry_Handle& GetTSEH() { return m_TSEH; }
    const CTSE_Handle& GetTSE_Handle() { return
            (m_TSEH ? m_TSEH.GetTSE_Handle() : CCacheImpl::kEmptyTSEHandle); }

    CBioseq_Handle GetBioseqHandleFromTSE(const CSeq_id& id);
    CBioseq_Handle GetLocalBioseqHandle(const CSeq_id& id); // Local here means not far

    const CConstRef<CSeq_annot>& GetSeqAnnot() { return m_SeqAnnot; }

    void AddBioseqWithNoPub(const CBioseq& seq);
    void AddBioseqWithNoBiosource(const CBioseq& seq);
    void AddProtWithoutFullRef(const CBioseq_Handle& seq);
    static bool IsWGSIntermediate(const CBioseq& seq);
    static bool IsTSAIntermediate(const CBioseq& seq);
    void ReportMissingPubs(const CSeq_entry& se, const CCit_sub* cs);
    void ReportMissingBiosource(const CSeq_entry& se);

    CConstRef<CSeq_feat> GetCDSGivenProduct(const CBioseq& seq);
    NCBI_DEPRECATED CConstRef<CSeq_feat> GetmRNAGivenProduct(const CBioseq& seq);
    CConstRef<CSeq_feat> GetmRNAGivenProduct(const CBioseq_Handle& seq);
    const CSeq_entry* GetAncestor(const CBioseq& seq, CBioseq_set::EClass clss);
    bool IsSerialNumberInComment(const string& comment);

    bool IsTransgenic(const CBioSource& bsrc);

    bool RequireLocalProduct(const CSeq_id* sid) const;

    using TSuppressed = set<CValidErrItem::TErrIndex>;
    TSuppressed& SetSuppressed();

private:

    // Setup common options during consturction;
    void x_Init(Uint4 options, size_t initialInferenceCount, bool notJustLocalOrGeneral, bool hasRefSeq);

    // This is so we can temporarily set m_Scope in a function
    // and be sure that it will be set to its old value when we're done
    class CScopeRestorer {
    public:
        CScopeRestorer( CRef<CScope> &scope ) :
          m_scopeToRestore(scope), m_scopeOriginalValue(scope) { }

        ~CScopeRestorer() { m_scopeToRestore = m_scopeOriginalValue; }
    private:
        CRef<CScope>& m_scopeToRestore;
        CRef<CScope> m_scopeOriginalValue;
    };

    // Prohibit copy constructor & assignment operator
    CValidError_imp(const CValidError_imp&);
    CValidError_imp& operator= (const CValidError_imp&);

    void Setup(const CSeq_entry_Handle& seh);
    void Setup(const CSeq_annot_Handle& sa);
    CSeq_entry_Handle Setup(const CBioseq& seq);
    void SetScope(const CSeq_entry& se);

    CValidatorEntryInfo& x_SetEntryInfo();

    void x_AddValidErrItem(EDiagSev sev, 
            EErrType type, 
            const string& msg,
            const string& desc,
            const CSerialObject& obj,
            const string& accession,
            const int version);

    void ValidateSubmitBlock(const CSubmit_block& block, const CSeq_submit& ss);

    void InitializeSourceQualTags();
    void ValidateSourceQualTags(const string& str, const CSerialObject& obj, const CSeq_entry *ctx = nullptr);

    bool IsMixedStrands(const CSeq_loc& loc);

    void ValidatePubGen(const CCit_gen& gen, const CSerialObject& obj, const CSeq_entry *ctx = nullptr);
    void ValidatePubArticle(const CCit_art& art, TEntrezId uid, const CSerialObject& obj, const CSeq_entry *ctx = nullptr);
    void ValidatePubArticleNoPMID(const CCit_art& art, const CSerialObject& obj, const CSeq_entry *ctx = nullptr);
    void x_ValidatePages(const string& pages, const CSerialObject& obj, const CSeq_entry *ctx = nullptr);
    void ValidateAuthorList(const CAuth_list::C_Names& names, const CSerialObject& obj, const CSeq_entry *ctx = nullptr);
    void ValidateAuthorsInPubequiv (const CPub_equiv& pe, const CSerialObject& obj, const CSeq_entry *ctx = nullptr);
    void ValidatePubHasAuthor(const CPubdesc& pubdesc, const CSerialObject& obj, const CSeq_entry *ctx = nullptr);

    bool HasName(const CAuth_list& authors);
    bool HasTitle(const CTitle& title);
    bool HasIsoJTA(const CTitle& title);

    void FindEmbeddedScript(const CSerialObject& obj);
    void FindNonAsciiText (const CSerialObject& obj);
    void FindCollidingSerialNumbers (const CSerialObject& obj);


    void GatherTentativeName (const CSeq_entry& se, vector<CConstRef<CSeqdesc> >& usr_descs, vector<CConstRef<CSeq_entry> >& desc_ctxs, vector<CConstRef<CSeq_feat> >& usr_feats);

    static bool s_IsSalmonellaGenus(const string& taxname);
    EDiagSev x_SalmonellaErrorLevel();

    typedef struct tagSLocCheck {
        bool chk;
        bool unmarked_strand;
        bool mixed_strand;
        bool has_other;
        bool has_not_other;
        CConstRef<CSeq_id> id_cur;
        CConstRef<CSeq_id> id_prv;
        const CSeq_interval *int_cur = nullptr;
        const CSeq_interval *int_prv = nullptr;
        ENa_strand strand_cur;
        ENa_strand strand_prv;
        string prefix;
    } SLocCheck;

    void x_InitLocCheck(SLocCheck& lc, const string& prefix);
    void x_CheckForStrandChange(SLocCheck& lc);
    void x_CheckLoc(const CSeq_loc& loc, const CSerialObject& obj, SLocCheck& lc, bool lowerSev = false);
    void x_CheckPackedInt(const CPacked_seqint& packed_int,
                          SLocCheck& lc,
                          const CSerialObject& obj);
    bool x_CheckSeqInt(CConstRef<CSeq_id>& id_cur,
                       const CSeq_interval * int_cur,
                       ENa_strand& strand_cur);
    void x_ReportInvalidFuzz(const CPacked_seqint& packed_int, const CSerialObject& obj);
    void x_ReportInvalidFuzz(const CSeq_interval& interval, const CSerialObject& obj);
    void x_ReportInvalidFuzz(const CSeq_point& point, const CSerialObject& obj);
    void x_ReportInvalidFuzz(const CSeq_loc& loc, const CSerialObject& obj);
    void x_ReportPCRSeqProblem(const string& primer_kind,
                               char badch,
                               const CSerialObject& obj,
                               const CSeq_entry *ctx);
    void x_CheckPCRPrimer(const CPCRPrimer& primer,
                          const string& primer_kind,
                          const CSerialObject& obj,
                          const CSeq_entry *ctx);

    void x_DoBarcodeTests(CSeq_entry_Handle seh);

    bool x_DowngradeForMissingAffil(const CCit_sub& cs);

    bool x_IsSuppressed(CValidErrItem::TErrIndex errType) const;

    CRef<CObjectManager>    m_ObjMgr;
    CRef<CScope>            m_Scope;
    CConstRef<CSeq_entry>   m_TSE;
    CSeq_entry_Handle       m_TSEH;
    CConstRef<CSeq_annot>   m_SeqAnnot;

    CCacheImpl              m_cache;
    CGeneCache              m_GeneCache;

    // error repoitory
    IValidError*       m_ErrRepository;

    // flags derived from options parameter
    bool m_NonASCII;             // User sets if Non ASCII char found
    bool m_SuppressContext;      // Include context in errors if true
    bool m_ValidateAlignments;   // Validate Alignments if true
    bool m_ValidateExons;        // Check exon feature splice sites
    bool m_OvlPepErr;            // Peptide overlap error if true, else warn
    bool m_RequireISOJTA;        // Journal requires ISO JTA
    bool m_ValidateIdSet;        // validate update against ID set in database
    bool m_RemoteFetch;          // Remote fetch enabled?
    bool m_FarFetchMRNAproducts; // Remote fetch mRNA products
    bool m_FarFetchCDSproducts;  // Remote fetch proteins
    bool m_LatLonCheckState;
    bool m_LatLonIgnoreWater;
    bool m_LocusTagGeneralMatch;
    bool m_DoRubiscoText;
    bool m_IndexerVersion;
    bool m_genomeSubmission;
    bool m_UseEntrez;
    bool m_IgnoreExceptions;             // ignore exceptions when validating translation
    bool m_ValidateInferenceAccessions;  // check that accessions in inferences are valid
    bool m_ReportSpliceAsError;
    bool m_DoTaxLookup;
    bool m_DoBarcodeTests;
    bool m_RefSeqConventions;
    bool m_CollectLocusTags; // collect locus tags for use in special formatted reports
    bool m_SeqSubmitParent; // some errors are suppressed if this is run on a newly created submission
    bool m_GenerateGoldenFile;
    bool m_CompareVDJCtoCDS;
    bool m_IgnoreInferences;
    bool m_ForceInferences;
    bool m_NewStrainValidation;

    // flags calculated by examining data in record
    bool m_IsStandaloneAnnot;

    bool m_IsNC=false;
    bool m_IsNG=false;
    bool m_IsNM=false;
    bool m_IsNP=false;
    bool m_IsNR=false;
    bool m_IsNZ=false;
    bool m_IsNS=false;
    bool m_IsNT=false;
    bool m_IsNW=false;
    bool m_IsWP=false;
    bool m_IsXR=false;

    bool m_FarFetchFailure;
    CBioSourceKind m_biosource_kind;

    bool m_IsTbl2Asn;

    // seq ids contained within the orignal seq entry.
    // (used to check for far location)
    vector< CConstRef<CSeq_id> >    m_InitialSeqIds;
    // Bioseqs without source (should be considered only if m_NoSource is false)
    vector< CConstRef<CBioseq> >    m_BioseqWithNoSource;

    // list of publication serial numbers
    vector< int > m_PubSerialNumbers;

    CValidator::TProgressCallback m_PrgCallback;
    CValidator::CProgressInfo     m_PrgInfo;
    size_t      m_NumAlign;
    size_t      m_NumAnnot;
    size_t      m_NumBioseq;
    size_t      m_NumBioseq_set;
    size_t      m_NumDesc;
    size_t      m_NumDescr;
    size_t      m_NumFeat;
    size_t      m_NumGraph;

    size_t      m_NumMisplacedFeatures;
    size_t      m_NumSmallGenomeSetMisplaced;
    size_t      m_NumMisplacedGraphs;
    size_t      m_NumGenes;
    size_t      m_NumGeneXrefs;
    
    size_t      m_CumulativeInferenceCount;
    bool        m_NotJustLocalOrGeneral;
    bool        m_HasRefSeq;

    size_t      m_NumTpaWithHistory;
    size_t      m_NumTpaWithoutHistory;

    size_t      m_NumPseudo;
    size_t      m_NumPseudogene;

    size_t      m_NumTopSetSiblings;

    // Taxonomy service interface.
    unique_ptr<CTaxValidationAndCleanup> x_CreateTaxValidator() const;

    shared_ptr<SValidatorContext> m_pContext;
    unique_ptr<CValidatorEntryInfo> m_pEntryInfo = make_unique<CValidatorEntryInfo>();

    TSuppressed m_SuppressedErrors;
};


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* VALIDATOR___VALIDERROR_IMP__HPP */
