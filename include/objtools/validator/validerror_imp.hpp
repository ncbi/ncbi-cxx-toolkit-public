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
#include <util/strsearch.hpp>
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

#include <objtools/alnmgr/sparse_aln.hpp>

#include <objmgr/util/create_defline.hpp>

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

class CValidError_desc;
class CValidError_descr;


// ===========================  Central Validation  ==========================

// CValidError_imp provides the entry point to the validation process.
// It calls upon the various validation classes to perform validation of
// each part.
// The class holds all the data for the validation process.
class NCBI_VALIDATOR_EXPORT CValidError_imp
{
public:
    typedef map<int, int> TCount;

    // Interface to be used by the CValidError class

    CValidError_imp(CObjectManager& objmgr, CValidError* errors,
        Uint4 options = 0);

    // Constructor allowing over-ride of Services
    // Namely, the taxonomy service.
    // NB: ITaxon is owned by CValidator.
    CValidError_imp(CObjectManager& objmgr, CValidError* errors,
        ITaxon3* taxon, Uint4 options = 0);

    // Destructor
    virtual ~CValidError_imp(void);

    void SetOptions (Uint4 options);
    void SetErrorRepository (CValidError* errors);
    void Reset(void);

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
    void ValidateSubSource(const CSubSource& subsrc, const CSerialObject& obj, const CSeq_entry *ctx = nullptr, const bool isViral = false);
    void ValidateOrgRef(const COrg_ref& orgref, const CSerialObject& obj, const CSeq_entry *ctx);
    void ValidateTaxNameOrgname(const string& taxname, const COrgName& orgname, const CSerialObject& obj, const CSeq_entry *ctx);
    void ValidateOrgName(const COrgName& orgname, const bool has_taxon, const CSerialObject& obj, const CSeq_entry *ctx);
    void ValidateOrgModVoucher(const COrgMod& orgmod, const CSerialObject& obj, const CSeq_entry *ctx);
    void ValidateBioSourceForSeq(const CBioSource& bsrc, const CSerialObject& obj, const CSeq_entry *ctx, const CBioseq_Handle& bsh);

    void ValidateLatLonCountry(string countryname, string lat_lon, const CSerialObject& obj, const CSeq_entry *ctx);

    static bool IsSyntheticConstruct (const CBioSource& src);
    bool IsArtificial (const CBioSource& src);
    bool IsOtherDNA(const CBioseq_Handle& bsh) const;
    void ValidateSeqLoc(const CSeq_loc& loc, const CBioseq_Handle& seq, bool report_abutting,
                        const string& prefix, const CSerialObject& obj, bool lowerSev = false);

    void ValidateSeqLocIds(const CSeq_loc& loc, const CSerialObject& obj);
    static bool IsInOrganelleSmallGenomeSet(const CSeq_id& id, CScope& scope);
    static bool BadMultipleSequenceLocation(const CSeq_loc& loc, CScope& scope);
    void CheckMultipleIds(const CSeq_loc& loc, const CSerialObject& obj);
    void ValidateDbxref(const CDbtag& xref, const CSerialObject& obj,
    bool biosource = false, const CSeq_entry *ctx = nullptr);
    void ValidateDbxref(TDbtags& xref_list, const CSerialObject& obj,
    bool biosource = false, const CSeq_entry *ctx = nullptr);
    void ValidateCitSub(const CCit_sub& cs, const CSerialObject& obj, const CSeq_entry *ctx = nullptr);
    void ValidateTaxonomy(const CSeq_entry& se);
    void ValidateOrgRefs(CTaxValidationAndCleanup& tval);
    void ValidateSpecificHost(CTaxValidationAndCleanup& tval);
    void ValidateStrain(CTaxValidationAndCleanup& tval);
    void ValidateSpecificHost (const CSeq_entry& se);
    void ValidateTentativeName(const CSeq_entry& se);
    void ValidateTaxonomy(const COrg_ref& org, int genome = CBioSource::eGenome_unknown);
    void ValidateMultipleTaxIds(const CSeq_entry_Handle& seh);
    void ValidateCitations (const CSeq_entry_Handle& seh);
    bool x_IsFarFetchFailure (const CSeq_loc& loc);

    // getters
    inline CScope* GetScope(void) { return m_Scope; }
    inline CCacheImpl & GetCache(void) { return m_cache; }

    inline CConstRef<CSeq_feat> GetCachedGene(const CSeq_feat* f) { return m_GeneCache.GetGeneFromCache(f, *m_Scope); }
    inline CGeneCache& GetGeneCache() { return m_GeneCache; }

    // flags derived from options parameter
    bool IsNonASCII(void)             const { return m_NonASCII; }
    bool IsSuppressContext(void)      const { return m_SuppressContext; }
    bool IsValidateAlignments(void)   const { return m_ValidateAlignments; }
    bool IsValidateExons(void)        const { return m_ValidateExons; }
    bool IsOvlPepErr(void)            const { return m_OvlPepErr; }
    bool IsRequireTaxonID(void)       const { return !m_SeqSubmitParent; }
    bool IsSeqSubmitParent(void)      const { return m_SeqSubmitParent; }
    bool IsRequireISOJTA(void)        const { return m_RequireISOJTA; }
    bool IsValidateIdSet(void)        const { return m_ValidateIdSet; }
    bool IsRemoteFetch(void)          const { return m_RemoteFetch; }
    bool IsFarFetchMRNAproducts(void) const { return m_FarFetchMRNAproducts; }
    bool IsFarFetchCDSproducts(void)  const { return m_FarFetchCDSproducts; }
    bool IsLocusTagGeneralMatch(void) const { return m_LocusTagGeneralMatch; }
    bool DoRubiscoTest(void)          const { return m_DoRubiscoText; }
    bool IsIndexerVersion(void)       const { return m_IndexerVersion; }
    bool IsGenomeSubmission(void)     const { return m_genomeSubmission; }
    bool UseEntrez(void)              const { return m_UseEntrez; }
    bool DoTaxLookup(void)            const { return m_DoTaxLookup; }
    bool ValidateInferenceAccessions(void) const { return m_ValidateInferenceAccessions; }
    bool IgnoreExceptions(void) const { return m_IgnoreExceptions; }
    bool ReportSpliceAsError(void) const { return m_ReportSpliceAsError; }
    bool IsLatLonCheckState(void)     const { return m_LatLonCheckState; }
    bool IsLatLonIgnoreWater(void)    const { return m_LatLonIgnoreWater; }
    bool IsRefSeqConventions(void)    const { return m_RefSeqConventions; }
    bool GenerateGoldenFile(void)    const { return m_GenerateGoldenFile; }
    bool DoCompareVDJCtoCDS(void)    const { return m_CompareVDJCtoCDS; }


    // flags calculated by examining data in record
    inline bool IsStandaloneAnnot(void) const { return m_IsStandaloneAnnot; }
    inline bool IsNoPubs(void) const { return m_NoPubs; }
    inline bool IsNoCitSubPubs(void) const { return m_NoCitSubPubs; }
    inline bool IsNoBioSource(void) const { return m_NoBioSource; }
    inline bool IsGPS(void) const { return m_IsGPS; }
    inline bool IsGED(void) const { return m_IsGED; }
    inline bool IsPDB(void) const { return m_IsPDB; }
    inline bool IsPatent(void) const { return m_IsPatent; }
    inline bool IsRefSeq(void) const { return m_IsRefSeq || m_RefSeqConventions; }
    inline bool IsEmbl(void) const { return m_IsEmbl; }
    inline bool IsDdbj(void) const { return m_IsDdbj; }
    inline bool IsTPE(void) const { return m_IsTPE; }
    inline bool IsNC(void) const { return m_IsNC; }
    inline bool IsNG(void) const { return m_IsNG; }
    inline bool IsNM(void) const { return m_IsNM; }
    inline bool IsNP(void) const { return m_IsNP; }
    inline bool IsNR(void) const { return m_IsNR; }
    inline bool IsNZ(void) const { return m_IsNZ; }
    inline bool IsNS(void) const { return m_IsNS; }
    inline bool IsNT(void) const { return m_IsNT; }
    inline bool IsNW(void) const { return m_IsNW; }
    inline bool IsWP(void) const { return m_IsWP; }
    inline bool IsXR(void) const { return m_IsXR; }
    inline bool IsGI(void) const { return m_IsGI; }
    inline bool IsGpipe(void) const { return m_IsGpipe; }
    bool IsHtg(void) const;
    inline bool IsLocalGeneralOnly(void) const { return m_IsLocalGeneralOnly; }
    inline bool HasGiOrAccnVer(void) const { return m_HasGiOrAccnVer; }
    inline bool IsGenomic(void) const { return m_IsGenomic; }
    inline bool IsSeqSubmit(void) const { return m_IsSeqSubmit; }
    inline bool IsSmallGenomeSet(void) const { return m_IsSmallGenomeSet; }
    bool IsNoncuratedRefSeq(const CBioseq& seq, EDiagSev& sev);
    inline bool IsGenbank(void) const { return m_IsGB; }
    inline bool DoesAnyFeatLocHaveGI(void) const { return m_FeatLocHasGI; }
    inline bool DoesAnyProductLocHaveGI(void) const { return m_ProductLocHasGI; }
    inline bool DoesAnyGeneHaveLocusTag(void) const { return m_GeneHasLocusTag; }
    inline bool DoesAnyProteinHaveGeneralID(void) const { return m_ProteinHasGeneralID; }
    inline bool IsINSDInSep(void) const { return m_IsINSDInSep; }
    inline bool IsGeneious(void) const { return m_IsGeneious; }
    inline const CBioSourceKind& BioSourceKind() const { return m_biosource_kind; }

    // counting number of misplaced features
    inline void ResetMisplacedFeatureCount (void) { m_NumMisplacedFeatures = 0; }
    inline void IncrementMisplacedFeatureCount (void) { m_NumMisplacedFeatures++; }
    inline void AddToMisplacedFeatureCount (SIZE_TYPE num) { m_NumMisplacedFeatures += num; }

    // counting number of small genome set misplaced features
    inline void ResetSmallGenomeSetMisplacedCount (void) { m_NumSmallGenomeSetMisplaced = 0; }
    inline void IncrementSmallGenomeSetMisplacedCount (void) { m_NumSmallGenomeSetMisplaced++; }
    inline void AddToSmallGenomeSetMisplacedCount (SIZE_TYPE num) { m_NumSmallGenomeSetMisplaced += num; }

    // counting number of misplaced graphs
    inline void ResetMisplacedGraphCount (void) { m_NumMisplacedGraphs = 0; }
    inline void IncrementMisplacedGraphCount (void) { m_NumMisplacedGraphs++; }
    inline void AddToMisplacedGraphCount (SIZE_TYPE num) { m_NumMisplacedGraphs += num; }

    // counting number of genes and gene xrefs
    inline void ResetGeneCount (void) { m_NumGenes = 0; }
    inline void IncrementGeneCount (void) { m_NumGenes++; }
    inline void AddToGeneCount (SIZE_TYPE num) { m_NumGenes += num; }
    inline void ResetGeneXrefCount (void) { m_NumGeneXrefs = 0; }
    inline void IncrementGeneXrefCount (void) { m_NumGeneXrefs++; }
    inline void AddToGeneXrefCount (SIZE_TYPE num) { m_NumGeneXrefs += num; }

    // counting sequences with and without TPA history
    inline void ResetTpaWithHistoryCount (void) { m_NumTpaWithHistory = 0; }
    inline void IncrementTpaWithHistoryCount (void) { m_NumTpaWithHistory++; }
    inline void AddToTpaWithHistoryCount (SIZE_TYPE num) { m_NumTpaWithHistory += num; }
    inline void ResetTpaWithoutHistoryCount (void) { m_NumTpaWithoutHistory = 0; }
    inline void IncrementTpaWithoutHistoryCount (void) { m_NumTpaWithoutHistory++; }
    inline void AddToTpaWithoutHistoryCount (SIZE_TYPE num) { m_NumTpaWithoutHistory += num; }

    // counting number of Pseudos and Pseudogenes
    inline void ResetPseudoCount (void) { m_NumPseudo = 0; }
    inline void IncrementPseudoCount (void) { m_NumPseudo++; }
    inline void AddToPseudoCount (SIZE_TYPE num) { m_NumPseudo += num; }
    inline void ResetPseudogeneCount (void) { m_NumPseudogene = 0; }
    inline void IncrementPseudogeneCount (void) { m_NumPseudogene++; }
    inline void AddToPseudogeneCount (SIZE_TYPE num) { m_NumPseudogene += num; }

    // set flag for farfetchfailure
    inline void SetFarFetchFailure (void) { m_FarFetchFailure = true; }

    const CSeq_entry& GetTSE(void) const { return *m_TSE; };
    const CSeq_entry_Handle & GetTSEH(void) { return m_TSEH; }
    const CTSE_Handle & GetTSE_Handle(void) { return
            (m_TSEH ? m_TSEH.GetTSE_Handle() : CCacheImpl::kEmptyTSEHandle); }
    const CConstRef<CSeq_annot>& GetSeqAnnot(void) { return m_SeqAnnot; }

    void AddBioseqWithNoPub(const CBioseq& seq);
    void AddBioseqWithNoBiosource(const CBioseq& seq);
    void AddProtWithoutFullRef(const CBioseq_Handle& seq);
    static bool IsWGSIntermediate(const CBioseq& seq);
    static bool IsTSAIntermediate(const CBioseq& seq);
    void ReportMissingPubs(const CSeq_entry& se, const CCit_sub* cs);
    void ReportMissingBiosource(const CSeq_entry& se);

    CConstRef<CSeq_feat> GetCDSGivenProduct(const CBioseq& seq);
    CConstRef<CSeq_feat> GetmRNAGivenProduct(const CBioseq& seq);
    const CSeq_entry* GetAncestor(const CBioseq& seq, CBioseq_set::EClass clss);
    bool IsSerialNumberInComment(const string& comment);

    bool IsTransgenic(const CBioSource& bsrc);

    bool RequireLocalProduct(const CSeq_id* sid) const;

private:

    // Setup common options during consturction;
    void x_Init(Uint4 options);

    // This is so we can temporarily set m_Scope in a function
    // and be sure that it will be set to its old value when we're done
    class CScopeRestorer {
    public:
        CScopeRestorer( CRef<CScope> &scope ) :
          m_scopeToRestore(scope), m_scopeOriginalValue(scope) { }

        ~CScopeRestorer(void) { m_scopeToRestore = m_scopeOriginalValue; }
    private:
        CRef<CScope> &m_scopeToRestore;
        CRef<CScope> m_scopeOriginalValue;
    };

    // Prohibit copy constructor & assignment operator
    CValidError_imp(const CValidError_imp&);
    CValidError_imp& operator= (const CValidError_imp&);

    void Setup(const CSeq_entry_Handle& seh);
    void Setup(const CSeq_annot_Handle& sa);
    CSeq_entry_Handle Setup(const CBioseq& seq);
    void SetScope(const CSeq_entry& se);

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
                       ENa_strand& strand_cur,
                       const CSerialObject& obj);
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

    CRef<CObjectManager>    m_ObjMgr;
    CRef<CScope>            m_Scope;
    CConstRef<CSeq_entry>   m_TSE;
    CSeq_entry_Handle       m_TSEH;
    CConstRef<CSeq_annot>   m_SeqAnnot;

    CCacheImpl              m_cache;
    CGeneCache              m_GeneCache;

    // error repoitory
    CValidError*       m_ErrRepository;

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

    // flags calculated by examining data in record
    bool m_IsStandaloneAnnot;
    bool m_NoPubs;                  // Suppress no pub error if true
    bool m_NoCitSubPubs;            // Suppress no cit-sub pub error if true
    bool m_NoBioSource;             // Suppress no organism error if true
    bool m_IsGPS;
    bool m_IsGED;
    bool m_IsPDB;
    bool m_IsPatent;
    bool m_IsRefSeq;
    bool m_IsEmbl;
    bool m_IsDdbj;
    bool m_IsTPE;
    bool m_IsNC;
    bool m_IsNG;
    bool m_IsNM;
    bool m_IsNP;
    bool m_IsNR;
    bool m_IsNZ;
    bool m_IsNS;
    bool m_IsNT;
    bool m_IsNW;
    bool m_IsWP;
    bool m_IsXR;
    bool m_IsGI;
    bool m_IsGB;
    bool m_IsGpipe;
    bool m_IsLocalGeneralOnly;
    bool m_HasGiOrAccnVer;
    bool m_IsGenomic;
    bool m_IsSeqSubmit;
    bool m_IsSmallGenomeSet;
    bool m_FeatLocHasGI;
    bool m_ProductLocHasGI;
    bool m_GeneHasLocusTag;
    bool m_ProteinHasGeneralID;
    bool m_IsINSDInSep;
    bool m_FarFetchFailure;
    bool m_IsGeneious;

    CBioSourceKind m_biosource_kind;

    bool m_IsTbl2Asn;

    // seq ids contained within the orignal seq entry.
    // (used to check for far location)
    vector< CConstRef<CSeq_id> >    m_InitialSeqIds;
    // Bioseqs without source (should be considered only if m_NoSource is false)
    vector< CConstRef<CBioseq> >    m_BioseqWithNoSource;

    // list of publication serial numbers
    vector< int > m_PubSerialNumbers;

    // legal dbxref database strings
    static const string legalDbXrefs[];
    static const string legalRefSeqDbXrefs[];

    // source qulalifiers prefixes
    static const string sm_SourceQualPrefixes[];
    static unique_ptr<CTextFsa> m_SourceQualTags;

    CValidator::TProgressCallback m_PrgCallback;
    CValidator::CProgressInfo     m_PrgInfo;
    SIZE_TYPE   m_NumAlign;
    SIZE_TYPE   m_NumAnnot;
    SIZE_TYPE   m_NumBioseq;
    SIZE_TYPE   m_NumBioseq_set;
    SIZE_TYPE   m_NumDesc;
    SIZE_TYPE   m_NumDescr;
    SIZE_TYPE   m_NumFeat;
    SIZE_TYPE   m_NumGraph;

    SIZE_TYPE   m_NumMisplacedFeatures;
    SIZE_TYPE   m_NumSmallGenomeSetMisplaced;
    SIZE_TYPE   m_NumMisplacedGraphs;
    SIZE_TYPE   m_NumGenes;
    SIZE_TYPE   m_NumGeneXrefs;

    SIZE_TYPE   m_NumTpaWithHistory;
    SIZE_TYPE   m_NumTpaWithoutHistory;

    SIZE_TYPE   m_NumPseudo;
    SIZE_TYPE   m_NumPseudogene;

    size_t      m_NumTopSetSiblings;

    // Taxonomy service interface.
    ITaxon3* m_taxon;
    ITaxon3* x_GetTaxonService();

};


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* VALIDATOR___VALIDERROR_IMP__HPP */
