/* $Id$
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
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko.......
 *
 * File Description:
 *   Validates CSeq_entries and CSeq_submits
 *
 */
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbistd.hpp>

#include <serial/objostr.hpp>
#include <serial/serial.hpp>
#include <serial/iterator.hpp>
#include <serial/enumvalues.hpp>
#include <serial/serialimpl.hpp>

#include <objects/objmgr/scope.hpp>
#include <objects/objmgr/seq_vector.hpp>
#include <objects/objmgr/feat_ci.hpp>
#include <objects/objmgr/desc_ci.hpp>
#include <objects/objmgr/seqdesc_ci.hpp>

#include <objects/general/Int_fuzz.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/Date.hpp>

#include <objects/pub/Pub.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objects/seq/seq__.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seq/gencode.hpp>
#include <objects/seq/Seqdesc.hpp>

#include <objects/seqalign/Seq_align.hpp>

#include <objects/seqblock/GB_block.hpp>
#include <objects/seqblock/EMBL_block.hpp>

#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/Prot_ref.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seqfeat/Imp_feat.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>

#include <objects/seqres/Seq_graph.hpp>

#include <objects/submit/Seq_submit.hpp>
#include <objects/submit/Submit_block.hpp>

#include <objects/util/sequence.hpp>
#include <objects/util/feature.hpp>
#include <objects/util/validate.hpp>

#include <util/strsearch.hpp>

#include <algorithm>
#include <list>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)
using namespace sequence;


// validator internal error types
enum EErrType {
    eErr_ALL = 0,

    eErr_SEQ_INST_ExtNotAllowed,
    eErr_SEQ_INST_ExtBadOrMissing,
    eErr_SEQ_INST_SeqDataNotFound,
    eErr_SEQ_INST_SeqDataNotAllowed,
    eErr_SEQ_INST_ReprInvalid,
    eErr_SEQ_INST_CircularProtein,
    eErr_SEQ_INST_DSProtein,
    eErr_SEQ_INST_MolNotSet,
    eErr_SEQ_INST_MolOther,
    eErr_SEQ_INST_FuzzyLen,
    eErr_SEQ_INST_InvalidLen,
    eErr_SEQ_INST_InvalidAlphabet,
    eErr_SEQ_INST_SeqDataLenWrong,
    eErr_SEQ_INST_SeqPortFail,
    eErr_SEQ_INST_InvalidResidue,
    eErr_SEQ_INST_StopInProtein,
    eErr_SEQ_INST_PartialInconsistent,
    eErr_SEQ_INST_ShortSeq,
    eErr_SEQ_INST_NoIdOnBioseq,
    eErr_SEQ_INST_BadDeltaSeq,
    eErr_SEQ_INST_LongHtgsSequence,
    eErr_SEQ_INST_LongLiteralSequence,
    eErr_SEQ_INST_SequenceExceeds350kbp,
    eErr_SEQ_INST_ConflictingIdsOnBioseq,
    eErr_SEQ_INST_MolNuclAcid,
    eErr_SEQ_INST_ConflictingBiomolTech,
    eErr_SEQ_INST_SeqIdNameHasSpace,
    eErr_SEQ_INST_IdOnMultipleBioseqs,
    eErr_SEQ_INST_DuplicateSegmentReferences,
    eErr_SEQ_INST_TrailingX,
    eErr_SEQ_INST_BadSeqIdFormat,
    eErr_SEQ_INST_PartsOutOfOrder,
    eErr_SEQ_INST_BadSecondaryAccn,
    eErr_SEQ_INST_ZeroGiNumber,
    eErr_SEQ_INST_RnaDnaConflict,
    eErr_SEQ_INST_HistoryGiCollision,
    eErr_SEQ_INST_GiWithoutAccession,
    eErr_SEQ_INST_MultipleAccessions,

    eErr_SEQ_DESCR_BioSourceMissing,
    eErr_SEQ_DESCR_InvalidForType,
    eErr_SEQ_DESCR_FileOpenCollision,
    eErr_SEQ_DESCR_Unknown,
    eErr_SEQ_DESCR_NoPubFound,
    eErr_SEQ_DESCR_NoOrgFound,
    eErr_SEQ_DESCR_MultipleBioSources,
    eErr_SEQ_DESCR_NoMolInfoFound,
    eErr_SEQ_DESCR_BadCountryCode,
    eErr_SEQ_DESCR_NoTaxonID,
    eErr_SEQ_DESCR_InconsistentBioSources,
    eErr_SEQ_DESCR_MissingLineage,
    eErr_SEQ_DESCR_SerialInComment,
    eErr_SEQ_DESCR_BioSourceNeedsFocus,
    eErr_SEQ_DESCR_BadOrganelle,
    eErr_SEQ_DESCR_MultipleChromosomes,
    eErr_SEQ_DESCR_BadSubSource,
    eErr_SEQ_DESCR_BadOrgMod,
    eErr_SEQ_DESCR_InconsistentProteinTitle,
    eErr_SEQ_DESCR_Inconsistent,
    eErr_SEQ_DESCR_ObsoleteSourceLocation,
    eErr_SEQ_DESCR_ObsoleteSourceQual,
    eErr_SEQ_DESCR_StructuredSourceNote,
    eErr_SEQ_DESCR_MultipleTitles,

    eErr_GENERIC_NonAsciiAsn,
    eErr_GENERIC_Spell,
    eErr_GENERIC_AuthorListHasEtAl,
    eErr_GENERIC_MissingPubInfo,
    eErr_GENERIC_UnnecessaryPubEquiv,
    eErr_GENERIC_BadPageNumbering,

    eErr_SEQ_PKG_NoCdRegionPtr,
    eErr_SEQ_PKG_NucProtProblem,
    eErr_SEQ_PKG_SegSetProblem,
    eErr_SEQ_PKG_EmptySet,
    eErr_SEQ_PKG_NucProtNotSegSet,
    eErr_SEQ_PKG_SegSetNotParts,
    eErr_SEQ_PKG_SegSetMixedBioseqs,
    eErr_SEQ_PKG_PartsSetMixedBioseqs,
    eErr_SEQ_PKG_PartsSetHasSets,
    eErr_SEQ_PKG_FeaturePackagingProblem,
    eErr_SEQ_PKG_GenomicProductPackagingProblem,
    eErr_SEQ_PKG_InconsistentMolInfoBiomols,

    eErr_SEQ_FEAT_InvalidForType,
    eErr_SEQ_FEAT_PartialProblem,
    eErr_SEQ_FEAT_InvalidType,
    eErr_SEQ_FEAT_Range,
    eErr_SEQ_FEAT_MixedStrand,
    eErr_SEQ_FEAT_SeqLocOrder,
    eErr_SEQ_FEAT_CdTransFail,
    eErr_SEQ_FEAT_StartCodon,
    eErr_SEQ_FEAT_InternalStop,
    eErr_SEQ_FEAT_NoProtein,
    eErr_SEQ_FEAT_MisMatchAA,
    eErr_SEQ_FEAT_TransLen,
    eErr_SEQ_FEAT_NoStop,
    eErr_SEQ_FEAT_TranslExcept,
    eErr_SEQ_FEAT_NoProtRefFound,
    eErr_SEQ_FEAT_NotSpliceConsensus,
    eErr_SEQ_FEAT_OrfCdsHasProduct,
    eErr_SEQ_FEAT_GeneRefHasNoData,
    eErr_SEQ_FEAT_ExceptInconsistent,
    eErr_SEQ_FEAT_ProtRefHasNoData,
    eErr_SEQ_FEAT_GenCodeMismatch,
    eErr_SEQ_FEAT_RNAtype0,
    eErr_SEQ_FEAT_UnknownImpFeatKey,
    eErr_SEQ_FEAT_UnknownImpFeatQual,
    eErr_SEQ_FEAT_WrongQualOnImpFeat,
    eErr_SEQ_FEAT_MissingQualOnImpFeat,
    eErr_SEQ_FEAT_PsuedoCdsHasProduct,
    eErr_SEQ_FEAT_IllegalDbXref,
    eErr_SEQ_FEAT_FarLocation,
    eErr_SEQ_FEAT_DuplicateFeat,
    eErr_SEQ_FEAT_UnnecessaryGeneXref,
    eErr_SEQ_FEAT_TranslExceptPhase,
    eErr_SEQ_FEAT_TrnaCodonWrong,
    eErr_SEQ_FEAT_BothStrands,
    eErr_SEQ_FEAT_CDSgeneRange,
    eErr_SEQ_FEAT_CDSmRNArange,
    eErr_SEQ_FEAT_OverlappingPeptideFeat,
    eErr_SEQ_FEAT_SerialInComment,
    eErr_SEQ_FEAT_MultipleCDSproducts,
    eErr_SEQ_FEAT_FocusOnBioSourceFeature,
    eErr_SEQ_FEAT_PeptideFeatOutOfFrame,
    eErr_SEQ_FEAT_InvalidQualifierValue,
    eErr_SEQ_FEAT_MultipleMRNAproducts,
    eErr_SEQ_FEAT_mRNAgeneRange,
    eErr_SEQ_FEAT_TranscriptLen,
    eErr_SEQ_FEAT_TranscriptMismatches,
    eErr_SEQ_FEAT_CDSproductPackagingProblem,
    eErr_SEQ_FEAT_DuplicateInterval,
    eErr_SEQ_FEAT_PolyAsiteNotPoint,
    eErr_SEQ_FEAT_ImpFeatBadLoc,
    eErr_SEQ_FEAT_LocOnSegmentedBioseq,
    eErr_SEQ_FEAT_UnnecessaryCitPubEquiv,
    eErr_SEQ_FEAT_ImpCDShasTranslation,
    eErr_SEQ_FEAT_ImpCDSnotPseudo,
    eErr_SEQ_FEAT_MissingMRNAproduct,
    eErr_SEQ_FEAT_AbuttingIntervals,
    eErr_SEQ_FEAT_CollidingGeneNames,
    eErr_SEQ_FEAT_MultiIntervalGene,
    eErr_SEQ_FEAT_FeatContentDup,

    eErr_SEQ_ALIGN_SeqIdProblem,
    eErr_SEQ_ALIGN_StrandRev,
    eErr_SEQ_ALIGN_DensegLenStart,
    eErr_SEQ_ALIGN_StartLessthanZero,
    eErr_SEQ_ALIGN_StartMorethanBiolen,
    eErr_SEQ_ALIGN_EndLessthanZero,
    eErr_SEQ_ALIGN_EndMorethanBiolen,
    eErr_SEQ_ALIGN_LenLessthanZero,
    eErr_SEQ_ALIGN_LenMorethanBiolen,
    eErr_SEQ_ALIGN_SumLenStart,
    eErr_SEQ_ALIGN_AlignDimSeqIdNotMatch,
    eErr_SEQ_ALIGN_SegsDimSeqIdNotMatch,
    eErr_SEQ_ALIGN_FastaLike,
    eErr_SEQ_ALIGN_NullSegs,
    eErr_SEQ_ALIGN_SegmentGap,
    eErr_SEQ_ALIGN_SegsDimOne,
    eErr_SEQ_ALIGN_AlignDimOne,
    eErr_SEQ_ALIGN_Segtype,
    eErr_SEQ_ALIGN_BlastAligns,

    eErr_SEQ_GRAPH_GraphMin,
    eErr_SEQ_GRAPH_GraphMax,
    eErr_SEQ_GRAPH_GraphBelow,
    eErr_SEQ_GRAPH_GraphAbove,
    eErr_SEQ_GRAPH_GraphByteLen,
    eErr_SEQ_GRAPH_GraphOutOfOrder,
    eErr_SEQ_GRAPH_GraphBioseqLen,
    eErr_SEQ_GRAPH_GraphSeqLitLen,
    eErr_SEQ_GRAPH_GraphSeqLocLen,
    eErr_SEQ_GRAPH_GraphStartPhase,
    eErr_SEQ_GRAPH_GraphStopPhase,
    eErr_SEQ_GRAPH_GraphDiffNumber,
    eErr_SEQ_GRAPH_GraphACGTScore,
    eErr_SEQ_GRAPH_GraphNScore,
    eErr_SEQ_GRAPH_GraphGapScore,
    eErr_SEQ_GRAPH_GraphOverlap,

    eErr_UNKNOWN
};


// *********************** Debug only -- remove before release **********

template<class T>
void display_object(const T& obj)
{
    auto_ptr<CObjectOStream> os (CObjectOStream::Open(eSerial_AsnText, cout));
    *os << obj;
    cout << endl;
}


// ********************** CValidException used internally only **********

class CValidException : public CException
{
public:
    enum EErrCode {
        eSeqId
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode())  {
        case eSeqId:  return "eSeqId";
        default:      return CException::GetErrCodeString();
        }
    }
    
    NCBI_EXCEPTION_DEFAULT(CValidException,CException);
};


// ******************************* CGbqualType *****************************

// Enumeration of GBQual types.
class CGbqualType
{
public:
    enum EType {
        e_Bad = 0,
        e_Allele,
        e_Anticodon,
        e_Bond,
        e_Bond_type,
        e_Bound_moiety,
        e_Cds_product,
        e_Citation,
        e_Clone,
        e_Coded_by,
        e_Codon,
        e_Codon_start,
        e_Cons_splice,
        e_Db_xref,
        e_Direction,
        e_EC_number,
        e_Evidence,
        e_Exception,
        e_Exception_note,
        e_Figure,
        e_Frequency,
        e_Function,
        e_Gene,
        e_Gene_desc,
        e_Gene_allele,
        e_Gene_map,
        e_Gene_syn,
        e_Gene_note,
        e_Gene_xref,
        e_Heterogen,
        e_Illegal_qual,
        e_Insertion_seq,
        e_Label,
        e_Locus_tag,
        e_Map,
        e_Maploc,
        e_Mod_base,
        e_Modelev,
        e_Note,
        e_Number,
        e_Organism,
        e_Partial,
        e_PCR_conditions,
        e_Phenotype,
        e_Product,
        e_Product_quals,
        e_Prot_activity,
        e_Prot_comment,
        e_Prot_EC_number,
        e_Prot_note,
        e_Prot_method,
        e_Prot_conflict,
        e_Prot_desc,
        e_Prot_missing,
        e_Prot_name,
        e_Prot_names,
        e_Protein_id,
        e_Pseudo,
        e_Region,
        e_Region_name,
        e_Replace,
        e_Rpt_family,
        e_Rpt_type,
        e_Rpt_unit,
        e_Rrna_its,
        e_Sec_str_type,
        e_Selenocysteine,
        e_Seqfeat_note,
        e_Site,
        e_Site_type,
        e_Standard_name,
        e_Transcript_id,
        e_Transl_except,
        e_Transl_table,
        e_Translation,
        e_Transposon,
        e_Trna_aa,
        e_Trna_codons,
        e_Usedin,
        e_Xtra_prod_quals
    };

    // Conversions to enumerated type:
    static EType GetType(const string& qual);
    static EType GetType(const CGb_qual& qual);
    
    // Conversion from enumerated to string:
    static const string& GetString(EType gbqual);

private:
    CGbqualType();
    DECLARE_INTERNAL_ENUM_INFO(EType);
};


CGbqualType::EType CGbqualType::GetType(const string& qual)
{
    EType type;
    try {
        type = static_cast<EType>(GetTypeInfo_enum_EType()->FindValue(qual));
    } catch (runtime_error rt) {
        type = CGbqualType::e_Bad;
    }

    return type;
}


CGbqualType::EType CGbqualType::GetType(const CGb_qual& qual)
{
    return GetType(qual.GetQual());
}


const string& CGbqualType::GetString(CGbqualType::EType gbqual)
{
    return GetTypeInfo_enum_EType()->FindName(gbqual, true);
}

        
BEGIN_NAMED_ENUM_IN_INFO("", CGbqualType::, EType, false)
{
    ADD_ENUM_VALUE("bad",               e_Bad);
    ADD_ENUM_VALUE("allele",            e_Allele);
    ADD_ENUM_VALUE("anticodon",         e_Anticodon);
    ADD_ENUM_VALUE("bond_type",         e_Bond_type);
    ADD_ENUM_VALUE("bound_moiety",      e_Bound_moiety);
    ADD_ENUM_VALUE("cds_product",       e_Cds_product);
    ADD_ENUM_VALUE("citation",          e_Citation);
    ADD_ENUM_VALUE("clone",             e_Clone);
    ADD_ENUM_VALUE("coded_by",          e_Coded_by);
    ADD_ENUM_VALUE("codon",             e_Codon);
    ADD_ENUM_VALUE("codon_start",       e_Codon_start);
    ADD_ENUM_VALUE("cons_splice",       e_Cons_splice);
    ADD_ENUM_VALUE("db_xref",           e_Db_xref);
    ADD_ENUM_VALUE("direction",         e_Direction);
    ADD_ENUM_VALUE("EC_number",         e_EC_number);
    ADD_ENUM_VALUE("evidence",          e_Evidence);
    ADD_ENUM_VALUE("exception",         e_Exception);
    ADD_ENUM_VALUE("exception_note",    e_Exception_note);
    ADD_ENUM_VALUE("figure",            e_Figure);
    ADD_ENUM_VALUE("frequency",         e_Frequency);
    ADD_ENUM_VALUE("function",          e_Function);
    ADD_ENUM_VALUE("gene",              e_Gene);
    ADD_ENUM_VALUE("gene_desc",         e_Gene_desc);
    ADD_ENUM_VALUE("gene_allele",       e_Gene_allele);
    ADD_ENUM_VALUE("gene_map",          e_Gene_map);
    ADD_ENUM_VALUE("gene_syn",          e_Gene_syn);
    ADD_ENUM_VALUE("gene_note",         e_Gene_note);
    ADD_ENUM_VALUE("gene_xref",         e_Gene_xref);
    ADD_ENUM_VALUE("heterogen",         e_Heterogen);
    ADD_ENUM_VALUE("illegal_qual",      e_Illegal_qual);
    ADD_ENUM_VALUE("insertion_seq",     e_Insertion_seq);
    ADD_ENUM_VALUE("label",             e_Label);
    ADD_ENUM_VALUE("locus_tag",         e_Locus_tag);
    ADD_ENUM_VALUE("map",               e_Map);
    ADD_ENUM_VALUE("maploc",            e_Maploc);
    ADD_ENUM_VALUE("mod_base",          e_Mod_base);
    ADD_ENUM_VALUE("modelev",           e_Modelev);
    ADD_ENUM_VALUE("note",              e_Note);
    ADD_ENUM_VALUE("number",            e_Number);
    ADD_ENUM_VALUE("organism",          e_Organism);
    ADD_ENUM_VALUE("partial",           e_Partial);
    ADD_ENUM_VALUE("PCR_conditions",    e_PCR_conditions);
    ADD_ENUM_VALUE("phenotype",         e_Phenotype);
    ADD_ENUM_VALUE("product",           e_Product);
    ADD_ENUM_VALUE("product_quals",     e_Product_quals);
    ADD_ENUM_VALUE("prot_activity",     e_Prot_activity);
    ADD_ENUM_VALUE("prot_comment",      e_Prot_comment);
    ADD_ENUM_VALUE("prot_EC_number",    e_Prot_EC_number);
    ADD_ENUM_VALUE("prot_note",         e_Prot_note);
    ADD_ENUM_VALUE("prot_method",       e_Prot_method);
    ADD_ENUM_VALUE("prot_conflict",     e_Prot_conflict);
    ADD_ENUM_VALUE("prot_desc",         e_Prot_desc);
    ADD_ENUM_VALUE("prot_missing",      e_Prot_missing);
    ADD_ENUM_VALUE("prot_name",         e_Prot_name);
    ADD_ENUM_VALUE("prot_names",        e_Prot_names);
    ADD_ENUM_VALUE("protein_id",        e_Protein_id);
    ADD_ENUM_VALUE("pseudo",            e_Pseudo);
    ADD_ENUM_VALUE("region",            e_Region);
    ADD_ENUM_VALUE("region_name",       e_Region_name);
    ADD_ENUM_VALUE("replace",           e_Replace);
    ADD_ENUM_VALUE("rpt_family",        e_Rpt_family);
    ADD_ENUM_VALUE("rpt_type",          e_Rpt_type);
    ADD_ENUM_VALUE("rpt_unit",          e_Rpt_unit);
    ADD_ENUM_VALUE("rrna_its",          e_Rrna_its);
    ADD_ENUM_VALUE("sec_str_type",      e_Sec_str_type);
    ADD_ENUM_VALUE("selenocysteine",    e_Selenocysteine);
    ADD_ENUM_VALUE("seqfeat_note",      e_Seqfeat_note);
    ADD_ENUM_VALUE("site",              e_Site);
    ADD_ENUM_VALUE("site_type",         e_Site_type);
    ADD_ENUM_VALUE("standard_name",     e_Standard_name);
    ADD_ENUM_VALUE("transcript_id",     e_Transcript_id);
    ADD_ENUM_VALUE("transl_except",     e_Transl_except);
    ADD_ENUM_VALUE("transl_table",      e_Transl_table);
    ADD_ENUM_VALUE("translation",       e_Translation);
    ADD_ENUM_VALUE("transposon",        e_Transposon);
    ADD_ENUM_VALUE("trna_aa",           e_Trna_aa);
    ADD_ENUM_VALUE("trna_codons",       e_Trna_codons);
    ADD_ENUM_VALUE("usedin",            e_Usedin);
    ADD_ENUM_VALUE("xtra_prod_quals",   e_Xtra_prod_quals);
}
END_ENUM_INFO


// *********************** CFeatQualAssoc implementation ********************

class CFeatQualAssoc
{
public:
    typedef vector<CGbqualType::EType> GBQualTypeVec;
    typedef map<CSeqFeatData::ESubtype, GBQualTypeVec > TFeatQualMap;

    // Check to see is a certain gbqual is legal within the context of 
    // the specified feature
    static bool IsLegalGbqual(CSeqFeatData::ESubtype ftype,
                              CGbqualType::EType gbqual);

    // Retrieve the mandatory gbquals for a specific feature type.
    static const GBQualTypeVec& GetMandatoryGbquals(CSeqFeatData::ESubtype ftype);

private:

    CFeatQualAssoc(void);
    void PoplulateLegalGbquals(void);
    void PopulateMandatoryGbquals(void);
    bool IsLegal(CSeqFeatData::ESubtype ftype, CGbqualType::EType gbqual);
    const GBQualTypeVec& GetMandatoryQuals(CSeqFeatData::ESubtype ftype);
    
    static CFeatQualAssoc* Instance(void);

    static auto_ptr<CFeatQualAssoc> sm_Instance;
    // list of feature and their associated gbquals
    TFeatQualMap                    m_LegalGbquals;
    // list of features and their mandatory gbquals
    TFeatQualMap                    m_MandatoryGbquals;
};


bool CFeatQualAssoc::IsLegalGbqual
(CSeqFeatData::ESubtype ftype,
 CGbqualType::EType gbqual) 
{
    return Instance()->IsLegal(ftype, gbqual);
}


const CFeatQualAssoc::GBQualTypeVec& CFeatQualAssoc::GetMandatoryGbquals
(CSeqFeatData::ESubtype ftype)
{
    return Instance()->GetMandatoryQuals(ftype);
}


auto_ptr<CFeatQualAssoc> CFeatQualAssoc::sm_Instance;

bool CFeatQualAssoc::IsLegal
(CSeqFeatData::ESubtype ftype, 
 CGbqualType::EType gbqual)
{
    if ( m_LegalGbquals.find(ftype) != m_LegalGbquals.end() ) {
        if ( find( m_LegalGbquals[ftype].begin(), 
                   m_LegalGbquals[ftype].end(), 
                   gbqual) != m_LegalGbquals[ftype].end() ) {
            return true;
        }
    }
    return false;
}


const CFeatQualAssoc::GBQualTypeVec& CFeatQualAssoc::GetMandatoryQuals
(CSeqFeatData::ESubtype ftype)
{
    static GBQualTypeVec empty;

    if ( m_MandatoryGbquals.find(ftype) != m_MandatoryGbquals.end() ) {
        return m_MandatoryGbquals[ftype];
    }

    return empty;
}


CFeatQualAssoc* CFeatQualAssoc::Instance(void)
{
    if ( !sm_Instance.get() ) {
        sm_Instance.reset(new CFeatQualAssoc);
    }
    return sm_Instance.get();
}


CFeatQualAssoc::CFeatQualAssoc(void)
{
    PoplulateLegalGbquals();
    PopulateMandatoryGbquals();
}


#define ASSOCIATE(feat_subtype, gbqual_type) \
    m_LegalGbquals[CSeqFeatData::feat_subtype].push_back(CGbqualType::gbqual_type)

void CFeatQualAssoc::PoplulateLegalGbquals(void)
{
    // gene
    ASSOCIATE( eSubtype_gene, e_Allele );
    ASSOCIATE( eSubtype_gene, e_Function );
    ASSOCIATE( eSubtype_gene, e_Label );
    ASSOCIATE( eSubtype_gene, e_Map );
    ASSOCIATE( eSubtype_gene, e_Phenotype );
    ASSOCIATE( eSubtype_gene, e_Product );
    ASSOCIATE( eSubtype_gene, e_Standard_name );
    ASSOCIATE( eSubtype_gene, e_Usedin );
    
 
    // CDS
    ASSOCIATE( eSubtype_cdregion, e_Allele );
    ASSOCIATE( eSubtype_cdregion, e_Codon );
    ASSOCIATE( eSubtype_cdregion, e_Label );
    ASSOCIATE( eSubtype_cdregion, e_Map );
    ASSOCIATE( eSubtype_cdregion, e_Number );
    ASSOCIATE( eSubtype_cdregion, e_Standard_name );
    ASSOCIATE( eSubtype_cdregion, e_Usedin );

    // prot
    ASSOCIATE( eSubtype_prot, e_Product );

    // preRNA
    ASSOCIATE( eSubtype_preRNA, e_Allele );
    ASSOCIATE( eSubtype_preRNA, e_Function );
    ASSOCIATE( eSubtype_preRNA, e_Label );
    ASSOCIATE( eSubtype_preRNA, e_Map );
    ASSOCIATE( eSubtype_preRNA, e_Product );
    ASSOCIATE( eSubtype_preRNA, e_Standard_name );
    ASSOCIATE( eSubtype_preRNA, e_Usedin );

    // mRNA
    ASSOCIATE( eSubtype_mRNA, e_Allele );
    ASSOCIATE( eSubtype_mRNA, e_Function );
    ASSOCIATE( eSubtype_mRNA, e_Label );
    ASSOCIATE( eSubtype_mRNA, e_Map );
    ASSOCIATE( eSubtype_mRNA, e_Product );
    ASSOCIATE( eSubtype_mRNA, e_Standard_name );
    ASSOCIATE( eSubtype_mRNA, e_Usedin );

    // tRNA
    ASSOCIATE( eSubtype_tRNA, e_Function );
    ASSOCIATE( eSubtype_tRNA, e_Label );
    ASSOCIATE( eSubtype_tRNA, e_Map );
    ASSOCIATE( eSubtype_tRNA, e_Product );
    ASSOCIATE( eSubtype_tRNA, e_Standard_name );
    ASSOCIATE( eSubtype_tRNA, e_Usedin );

    // rRNA
    ASSOCIATE( eSubtype_rRNA, e_Function );
    ASSOCIATE( eSubtype_rRNA, e_Label );
    ASSOCIATE( eSubtype_rRNA, e_Map );
    ASSOCIATE( eSubtype_rRNA, e_Product );
    ASSOCIATE( eSubtype_rRNA, e_Standard_name );
    ASSOCIATE( eSubtype_rRNA, e_Usedin );

    // snRNA
    ASSOCIATE( eSubtype_snRNA, e_Function );
    ASSOCIATE( eSubtype_snRNA, e_Label );
    ASSOCIATE( eSubtype_snRNA, e_Map );
    ASSOCIATE( eSubtype_snRNA, e_Product );
    ASSOCIATE( eSubtype_snRNA, e_Standard_name );
    ASSOCIATE( eSubtype_snRNA, e_Usedin );

    // scRNA
    ASSOCIATE( eSubtype_scRNA, e_Function );
    ASSOCIATE( eSubtype_scRNA, e_Label );
    ASSOCIATE( eSubtype_scRNA, e_Map );
    ASSOCIATE( eSubtype_scRNA, e_Product );
    ASSOCIATE( eSubtype_scRNA, e_Standard_name );
    ASSOCIATE( eSubtype_scRNA, e_Usedin );

    // otherRNA
    ASSOCIATE( eSubtype_otherRNA, e_Function );
    ASSOCIATE( eSubtype_otherRNA, e_Label );
    ASSOCIATE( eSubtype_otherRNA, e_Map );
    ASSOCIATE( eSubtype_otherRNA, e_Product );
    ASSOCIATE( eSubtype_otherRNA, e_Standard_name );
    ASSOCIATE( eSubtype_otherRNA, e_Usedin );

    // attenuator
    ASSOCIATE( eSubtype_attenuator, e_Label );
    ASSOCIATE( eSubtype_attenuator, e_Map );
    ASSOCIATE( eSubtype_attenuator, e_Phenotype );
    ASSOCIATE( eSubtype_attenuator, e_Usedin );

    // C_region
    ASSOCIATE( eSubtype_C_region, e_Label );
    ASSOCIATE( eSubtype_C_region, e_Map );
    ASSOCIATE( eSubtype_C_region, e_Product );
    ASSOCIATE( eSubtype_C_region, e_Standard_name );
    ASSOCIATE( eSubtype_C_region, e_Usedin );

    // CAAT_signal
    ASSOCIATE( eSubtype_CAAT_signal, e_Label );
    ASSOCIATE( eSubtype_CAAT_signal, e_Map );
    ASSOCIATE( eSubtype_CAAT_signal, e_Usedin );

    // Imp_CDS
    ASSOCIATE( eSubtype_Imp_CDS, e_Codon );
    ASSOCIATE( eSubtype_Imp_CDS, e_EC_number );
    ASSOCIATE( eSubtype_Imp_CDS, e_Function );
    ASSOCIATE( eSubtype_Imp_CDS, e_Label );
    ASSOCIATE( eSubtype_Imp_CDS, e_Map );
    ASSOCIATE( eSubtype_Imp_CDS, e_Number );
    ASSOCIATE( eSubtype_Imp_CDS, e_Product );
    ASSOCIATE( eSubtype_Imp_CDS, e_Standard_name );
    ASSOCIATE( eSubtype_Imp_CDS, e_Usedin );

    // conflict
    ASSOCIATE( eSubtype_conflict, e_Label );
    ASSOCIATE( eSubtype_conflict, e_Map );
    ASSOCIATE( eSubtype_conflict, e_Replace );
    ASSOCIATE( eSubtype_conflict, e_Label );

    // D_loop
    ASSOCIATE( eSubtype_D_loop, e_Label );
    ASSOCIATE( eSubtype_D_loop, e_Map );
    ASSOCIATE( eSubtype_D_loop, e_Usedin );

    // D_segment
    ASSOCIATE( eSubtype_D_segment, e_Label );
    ASSOCIATE( eSubtype_D_segment, e_Map );
    ASSOCIATE( eSubtype_D_segment, e_Product );
    ASSOCIATE( eSubtype_D_segment, e_Standard_name );
    ASSOCIATE( eSubtype_D_segment, e_Usedin );

    // enhancer
    ASSOCIATE( eSubtype_enhancer, e_Label );
    ASSOCIATE( eSubtype_enhancer, e_Map );
    ASSOCIATE( eSubtype_enhancer, e_Standard_name );
    ASSOCIATE( eSubtype_enhancer, e_Usedin );

    // exon
    ASSOCIATE( eSubtype_exon, e_Allele );
    ASSOCIATE( eSubtype_exon, e_EC_number );
    ASSOCIATE( eSubtype_exon, e_Function );
    ASSOCIATE( eSubtype_exon, e_Label );
    ASSOCIATE( eSubtype_exon, e_Map );
    ASSOCIATE( eSubtype_exon, e_Number );
    ASSOCIATE( eSubtype_exon, e_Product );
    ASSOCIATE( eSubtype_exon, e_Standard_name );
    ASSOCIATE( eSubtype_exon, e_Usedin );

    // GC_signal
    ASSOCIATE( eSubtype_GC_signal, e_Label );
    ASSOCIATE( eSubtype_GC_signal, e_Map );
    ASSOCIATE( eSubtype_GC_signal, e_Usedin );

    // iDNA
    ASSOCIATE( eSubtype_iDNA, e_Function );
    ASSOCIATE( eSubtype_iDNA, e_Label );
    ASSOCIATE( eSubtype_iDNA, e_Map );
    ASSOCIATE( eSubtype_iDNA, e_Number );
    ASSOCIATE( eSubtype_iDNA, e_Standard_name );
    ASSOCIATE( eSubtype_iDNA, e_Usedin );

    // intron
    ASSOCIATE( eSubtype_intron, e_Allele );
    ASSOCIATE( eSubtype_intron, e_Cons_splice );
    ASSOCIATE( eSubtype_intron, e_Function );
    ASSOCIATE( eSubtype_intron, e_Label );
    ASSOCIATE( eSubtype_intron, e_Map );
    ASSOCIATE( eSubtype_intron, e_Number );
    ASSOCIATE( eSubtype_intron, e_Standard_name );
    ASSOCIATE( eSubtype_intron, e_Usedin );

    // J_segment
    ASSOCIATE( eSubtype_J_segment, e_Label );
    ASSOCIATE( eSubtype_J_segment, e_Map );
    ASSOCIATE( eSubtype_J_segment, e_Product );
    ASSOCIATE( eSubtype_J_segment, e_Standard_name );
    ASSOCIATE( eSubtype_J_segment, e_Usedin );

    // LTR
    ASSOCIATE( eSubtype_LTR, e_Function );
    ASSOCIATE( eSubtype_LTR, e_Label );
    ASSOCIATE( eSubtype_LTR, e_Map );
    ASSOCIATE( eSubtype_LTR, e_Standard_name );
    ASSOCIATE( eSubtype_LTR, e_Usedin );

    // mat_peptide
    ASSOCIATE( eSubtype_mat_peptide, e_EC_number );
    ASSOCIATE( eSubtype_mat_peptide, e_Function );
    ASSOCIATE( eSubtype_mat_peptide, e_Label );
    ASSOCIATE( eSubtype_mat_peptide, e_Map );
    ASSOCIATE( eSubtype_mat_peptide, e_Product );
    ASSOCIATE( eSubtype_mat_peptide, e_Standard_name );
    ASSOCIATE( eSubtype_mat_peptide, e_Usedin );

    // misc_binding
    ASSOCIATE( eSubtype_misc_binding, e_Bound_moiety );
    ASSOCIATE( eSubtype_misc_binding, e_Function );
    ASSOCIATE( eSubtype_misc_binding, e_Label );
    ASSOCIATE( eSubtype_misc_binding, e_Map );
    ASSOCIATE( eSubtype_misc_binding, e_Usedin );

    // misc_difference
    ASSOCIATE( eSubtype_misc_difference, e_Clone );
    ASSOCIATE( eSubtype_misc_difference, e_Label );
    ASSOCIATE( eSubtype_misc_difference, e_Map );
    ASSOCIATE( eSubtype_misc_difference, e_Phenotype );
    ASSOCIATE( eSubtype_misc_difference, e_Replace );
    ASSOCIATE( eSubtype_misc_difference, e_Standard_name );
    ASSOCIATE( eSubtype_misc_difference, e_Usedin );

    // misc_feature
    ASSOCIATE( eSubtype_misc_feature, e_Function );
    ASSOCIATE( eSubtype_misc_feature, e_Label );
    ASSOCIATE( eSubtype_misc_feature, e_Map );
    ASSOCIATE( eSubtype_misc_feature, e_Number );
    ASSOCIATE( eSubtype_misc_feature, e_Phenotype );
    ASSOCIATE( eSubtype_misc_feature, e_Product );
    ASSOCIATE( eSubtype_misc_feature, e_Standard_name );
    ASSOCIATE( eSubtype_misc_feature, e_Usedin );

    // misc_recomb
    ASSOCIATE( eSubtype_misc_recomb, e_Label );
    ASSOCIATE( eSubtype_misc_recomb, e_Map );
    ASSOCIATE( eSubtype_misc_recomb, e_Organism );
    ASSOCIATE( eSubtype_misc_recomb, e_Standard_name );
    ASSOCIATE( eSubtype_misc_recomb, e_Usedin );

    // misc_signal
    ASSOCIATE( eSubtype_misc_signal, e_Function );
    ASSOCIATE( eSubtype_misc_signal, e_Label );
    ASSOCIATE( eSubtype_misc_signal, e_Map );
    ASSOCIATE( eSubtype_misc_signal, e_Phenotype );
    ASSOCIATE( eSubtype_misc_signal, e_Standard_name );
    ASSOCIATE( eSubtype_misc_signal, e_Usedin );

    // misc_structure
    ASSOCIATE( eSubtype_misc_structure, e_Function );
    ASSOCIATE( eSubtype_misc_structure, e_Label );
    ASSOCIATE( eSubtype_misc_structure, e_Map );
    ASSOCIATE( eSubtype_misc_structure, e_Standard_name );
    ASSOCIATE( eSubtype_misc_structure, e_Usedin );

    // modified_base
    ASSOCIATE( eSubtype_misc_structure, e_Frequency );
    ASSOCIATE( eSubtype_misc_structure, e_Label );
    ASSOCIATE( eSubtype_misc_structure, e_Map );
    ASSOCIATE( eSubtype_misc_structure, e_Mod_base );
    ASSOCIATE( eSubtype_misc_structure, e_Usedin );

    // N_region
    ASSOCIATE( eSubtype_N_region, e_Label );
    ASSOCIATE( eSubtype_N_region, e_Map );
    ASSOCIATE( eSubtype_N_region, e_Product );
    ASSOCIATE( eSubtype_N_region, e_Standard_name );
    ASSOCIATE( eSubtype_N_region, e_Usedin );

    // old_sequence
    ASSOCIATE( eSubtype_old_sequence, e_Label );
    ASSOCIATE( eSubtype_old_sequence, e_Map );
    ASSOCIATE( eSubtype_old_sequence, e_Replace );
    ASSOCIATE( eSubtype_old_sequence, e_Usedin );

    // polyA_signal
    ASSOCIATE( eSubtype_polyA_signal, e_Label );
    ASSOCIATE( eSubtype_polyA_signal, e_Map );
    ASSOCIATE( eSubtype_polyA_signal, e_Usedin );

    // polyA_site
    ASSOCIATE( eSubtype_polyA_site, e_Label );
    ASSOCIATE( eSubtype_polyA_site, e_Map );
    ASSOCIATE( eSubtype_polyA_site, e_Usedin );

    // prim_transcript
    ASSOCIATE( eSubtype_prim_transcript, e_Allele );
    ASSOCIATE( eSubtype_prim_transcript, e_Function );
    ASSOCIATE( eSubtype_prim_transcript, e_Label );
    ASSOCIATE( eSubtype_prim_transcript, e_Map );
    ASSOCIATE( eSubtype_prim_transcript, e_Standard_name );
    ASSOCIATE( eSubtype_prim_transcript, e_Usedin );

    // primer_bind
    ASSOCIATE( eSubtype_primer_bind, e_Label );
    ASSOCIATE( eSubtype_primer_bind, e_Map );
    ASSOCIATE( eSubtype_primer_bind, e_PCR_conditions );
    ASSOCIATE( eSubtype_primer_bind, e_Standard_name );
    ASSOCIATE( eSubtype_primer_bind, e_Usedin );

    // promoter
    ASSOCIATE( eSubtype_promoter, e_Function );
    ASSOCIATE( eSubtype_promoter, e_Label );
    ASSOCIATE( eSubtype_promoter, e_Map );
    ASSOCIATE( eSubtype_promoter, e_Phenotype );
    ASSOCIATE( eSubtype_promoter, e_Standard_name );
    ASSOCIATE( eSubtype_promoter, e_Usedin );

    // protein_bind
    ASSOCIATE( eSubtype_promoter, e_Bound_moiety );
    ASSOCIATE( eSubtype_promoter, e_Function );
    ASSOCIATE( eSubtype_promoter, e_Label );
    ASSOCIATE( eSubtype_promoter, e_Map );
    ASSOCIATE( eSubtype_promoter, e_Standard_name );
    ASSOCIATE( eSubtype_promoter, e_Usedin );

    // RBS
    ASSOCIATE( eSubtype_RBS, e_Label );
    ASSOCIATE( eSubtype_RBS, e_Map );
    ASSOCIATE( eSubtype_RBS, e_Standard_name );
    ASSOCIATE( eSubtype_RBS, e_Usedin );

    // repeat_region
    ASSOCIATE( eSubtype_repeat_region, e_Function );
    ASSOCIATE( eSubtype_repeat_region, e_Insertion_seq );
    ASSOCIATE( eSubtype_repeat_region, e_Label );
    ASSOCIATE( eSubtype_repeat_region, e_Map );
    ASSOCIATE( eSubtype_repeat_region, e_Rpt_family );
    ASSOCIATE( eSubtype_repeat_region, e_Rpt_type );
    ASSOCIATE( eSubtype_repeat_region, e_Rpt_unit );
    ASSOCIATE( eSubtype_repeat_region, e_Standard_name );
    ASSOCIATE( eSubtype_repeat_region, e_Transposon );
    ASSOCIATE( eSubtype_repeat_region, e_Usedin );

    // repeat_unit
    ASSOCIATE( eSubtype_repeat_unit, e_Function );
    ASSOCIATE( eSubtype_repeat_unit, e_Label );
    ASSOCIATE( eSubtype_repeat_unit, e_Map );
    ASSOCIATE( eSubtype_repeat_unit, e_Rpt_family );
    ASSOCIATE( eSubtype_repeat_unit, e_Rpt_type );
    ASSOCIATE( eSubtype_repeat_unit, e_Usedin );

    // rep_origin
    ASSOCIATE( eSubtype_rep_origin, e_Direction );
    ASSOCIATE( eSubtype_rep_origin, e_Label );
    ASSOCIATE( eSubtype_rep_origin, e_Map );
    ASSOCIATE( eSubtype_rep_origin, e_Standard_name );
    ASSOCIATE( eSubtype_rep_origin, e_Usedin );

    // S_region
    ASSOCIATE( eSubtype_repeat_unit, e_Label );
    ASSOCIATE( eSubtype_repeat_unit, e_Map );
    ASSOCIATE( eSubtype_repeat_unit, e_Product );
    ASSOCIATE( eSubtype_repeat_unit, e_Standard_name );
    ASSOCIATE( eSubtype_repeat_unit, e_Usedin );

    // satellite
    ASSOCIATE( eSubtype_satellite, e_Label );
    ASSOCIATE( eSubtype_satellite, e_Map );
    ASSOCIATE( eSubtype_satellite, e_Rpt_family );
    ASSOCIATE( eSubtype_satellite, e_Rpt_type );
    ASSOCIATE( eSubtype_satellite, e_Rpt_unit );
    ASSOCIATE( eSubtype_satellite, e_Standard_name );
    ASSOCIATE( eSubtype_satellite, e_Usedin );

    // sig_peptide
    ASSOCIATE( eSubtype_sig_peptide, e_Function );
    ASSOCIATE( eSubtype_sig_peptide, e_Label );
    ASSOCIATE( eSubtype_sig_peptide, e_Map );
    ASSOCIATE( eSubtype_sig_peptide, e_Product );
    ASSOCIATE( eSubtype_sig_peptide, e_Standard_name );
    ASSOCIATE( eSubtype_sig_peptide, e_Usedin );

    // stem_loop
    ASSOCIATE( eSubtype_stem_loop, e_Function );
    ASSOCIATE( eSubtype_stem_loop, e_Label );
    ASSOCIATE( eSubtype_stem_loop, e_Map );
    ASSOCIATE( eSubtype_stem_loop, e_Standard_name );
    ASSOCIATE( eSubtype_stem_loop, e_Usedin );

    // STS
    ASSOCIATE( eSubtype_STS, e_Label );
    ASSOCIATE( eSubtype_STS, e_Map );
    ASSOCIATE( eSubtype_STS, e_Standard_name );
    ASSOCIATE( eSubtype_STS, e_Usedin );

    // TATA_signal
    ASSOCIATE( eSubtype_TATA_signal, e_Label );
    ASSOCIATE( eSubtype_TATA_signal, e_Map );
    ASSOCIATE( eSubtype_TATA_signal, e_Usedin );

    // terminator
    ASSOCIATE( eSubtype_terminator, e_Label );
    ASSOCIATE( eSubtype_terminator, e_Map );
    ASSOCIATE( eSubtype_terminator, e_Standard_name );
    ASSOCIATE( eSubtype_terminator, e_Usedin );

    // transit_peptide
    ASSOCIATE( eSubtype_transit_peptide, e_Function );
    ASSOCIATE( eSubtype_transit_peptide, e_Label );
    ASSOCIATE( eSubtype_transit_peptide, e_Map );
    ASSOCIATE( eSubtype_transit_peptide, e_Product );
    ASSOCIATE( eSubtype_transit_peptide, e_Standard_name );
    ASSOCIATE( eSubtype_transit_peptide, e_Usedin );

    // unsure
    ASSOCIATE( eSubtype_unsure, e_Label );
    ASSOCIATE( eSubtype_unsure, e_Map );
    ASSOCIATE( eSubtype_unsure, e_Replace );
    ASSOCIATE( eSubtype_unsure, e_Usedin );

    // V_region
    ASSOCIATE( eSubtype_V_region, e_Label );
    ASSOCIATE( eSubtype_V_region, e_Map );
    ASSOCIATE( eSubtype_V_region, e_Product );
    ASSOCIATE( eSubtype_V_region, e_Standard_name );
    ASSOCIATE( eSubtype_V_region, e_Usedin );

    // V_segment
    ASSOCIATE( eSubtype_V_segment, e_Label );
    ASSOCIATE( eSubtype_V_segment, e_Map );
    ASSOCIATE( eSubtype_V_segment, e_Product );
    ASSOCIATE( eSubtype_V_segment, e_Standard_name );
    ASSOCIATE( eSubtype_V_segment, e_Usedin );

    // variation
    ASSOCIATE( eSubtype_variation, e_Allele );
    ASSOCIATE( eSubtype_variation, e_Frequency );
    ASSOCIATE( eSubtype_variation, e_Label );
    ASSOCIATE( eSubtype_variation, e_Map );
    ASSOCIATE( eSubtype_variation, e_Phenotype );
    ASSOCIATE( eSubtype_variation, e_Product );
    ASSOCIATE( eSubtype_variation, e_Replace );
    ASSOCIATE( eSubtype_variation, e_Standard_name );
    ASSOCIATE( eSubtype_variation, e_Usedin );

    // 3clip
    ASSOCIATE( eSubtype_3clip, e_Allele );
    ASSOCIATE( eSubtype_3clip, e_Function );
    ASSOCIATE( eSubtype_3clip, e_Label );
    ASSOCIATE( eSubtype_3clip, e_Map );
    ASSOCIATE( eSubtype_3clip, e_Standard_name );
    ASSOCIATE( eSubtype_3clip, e_Usedin );

    // 3UTR
    ASSOCIATE( eSubtype_3UTR, e_Allele );
    ASSOCIATE( eSubtype_3UTR, e_Function );
    ASSOCIATE( eSubtype_3UTR, e_Label );
    ASSOCIATE( eSubtype_3UTR, e_Map );
    ASSOCIATE( eSubtype_3UTR, e_Standard_name );
    ASSOCIATE( eSubtype_3UTR, e_Usedin );

    // 5clip
    ASSOCIATE( eSubtype_5clip, e_Allele );
    ASSOCIATE( eSubtype_5clip, e_Function );
    ASSOCIATE( eSubtype_5clip, e_Label );
    ASSOCIATE( eSubtype_5clip, e_Map );
    ASSOCIATE( eSubtype_5clip, e_Standard_name );
    ASSOCIATE( eSubtype_5clip, e_Usedin );

    // 5UTR
    ASSOCIATE( eSubtype_5UTR, e_Allele );
    ASSOCIATE( eSubtype_5UTR, e_Function );
    ASSOCIATE( eSubtype_5UTR, e_Label );
    ASSOCIATE( eSubtype_5UTR, e_Map );
    ASSOCIATE( eSubtype_5UTR, e_Standard_name );
    ASSOCIATE( eSubtype_5UTR, e_Usedin );

    // 10_signal
    ASSOCIATE( eSubtype_10_signal, e_Label );
    ASSOCIATE( eSubtype_10_signal, e_Map );
    ASSOCIATE( eSubtype_10_signal, e_Standard_name );
    ASSOCIATE( eSubtype_10_signal, e_Usedin );

    // 35_signal
    ASSOCIATE( eSubtype_35_signal, e_Label );
    ASSOCIATE( eSubtype_35_signal, e_Map );
    ASSOCIATE( eSubtype_35_signal, e_Standard_name );
    ASSOCIATE( eSubtype_35_signal, e_Usedin );

    // region
    ASSOCIATE( eSubtype_region, e_Function );
    ASSOCIATE( eSubtype_region, e_Label );
    ASSOCIATE( eSubtype_region, e_Map );
    ASSOCIATE( eSubtype_region, e_Number );
    ASSOCIATE( eSubtype_region, e_Phenotype );
    ASSOCIATE( eSubtype_region, e_Product );
    ASSOCIATE( eSubtype_region, e_Standard_name );
    ASSOCIATE( eSubtype_region, e_Usedin );

    // mat_peptide_aa
    ASSOCIATE( eSubtype_mat_peptide_aa, e_Label );
    ASSOCIATE( eSubtype_mat_peptide_aa, e_Map );
    ASSOCIATE( eSubtype_mat_peptide_aa, e_Product );
    ASSOCIATE( eSubtype_mat_peptide_aa, e_Standard_name );
    ASSOCIATE( eSubtype_mat_peptide_aa, e_Usedin );

    // sig_peptide_aa
    ASSOCIATE( eSubtype_sig_peptide_aa, e_Label );
    ASSOCIATE( eSubtype_sig_peptide_aa, e_Map );
    ASSOCIATE( eSubtype_sig_peptide_aa, e_Product );
    ASSOCIATE( eSubtype_sig_peptide_aa, e_Standard_name );
    ASSOCIATE( eSubtype_sig_peptide_aa, e_Usedin );

    // transit_peptide_aa
    ASSOCIATE( eSubtype_transit_peptide_aa, e_Label );
    ASSOCIATE( eSubtype_transit_peptide_aa, e_Map );
    ASSOCIATE( eSubtype_transit_peptide_aa, e_Product );
    ASSOCIATE( eSubtype_transit_peptide_aa, e_Standard_name );
    ASSOCIATE( eSubtype_transit_peptide_aa, e_Usedin );

    // snoRNA
    ASSOCIATE( eSubtype_snoRNA, e_Function );
    ASSOCIATE( eSubtype_snoRNA, e_Label );
    ASSOCIATE( eSubtype_snoRNA, e_Map );
    ASSOCIATE( eSubtype_snoRNA, e_Product );
    ASSOCIATE( eSubtype_snoRNA, e_Standard_name );
    ASSOCIATE( eSubtype_snoRNA, e_Usedin );
}

void CFeatQualAssoc::PopulateMandatoryGbquals(void)
{
    // gene feature requires gene gbqual
    m_MandatoryGbquals[CSeqFeatData::eSubtype_gene].push_back(CGbqualType::e_Gene);

    // misc_binding & protein_bind require bound_moiety
    m_MandatoryGbquals[CSeqFeatData::eSubtype_misc_binding].push_back(CGbqualType::e_Bound_moiety);
    m_MandatoryGbquals[CSeqFeatData::eSubtype_protein_bind].push_back(CGbqualType::e_Bound_moiety);

    // modified_base requires mod_base
    m_MandatoryGbquals[CSeqFeatData::eSubtype_modified_base].push_back(CGbqualType::e_Mod_base);
}


// *********************** CValidErrItem implementation ********************

CValidErrItem::CValidErrItem
(EDiagSev sev,
 unsigned int         ei,
 const string&        msg,
 const CSerialObject& obj)
  : m_Severity (sev),
    m_ErrIndex (ei),
    m_Message (msg),
    m_Object (&obj, obj.GetThisTypeInfo ())

{
}

CValidErrItem::~CValidErrItem()
{
}


EDiagSev CValidErrItem::GetSeverity() const
{
    return m_Severity;
}


const string& CValidErrItem::GetErrCode (void) const
{
    if (m_ErrIndex <= eErr_UNKNOWN) {
        return sm_Terse [m_ErrIndex];
    }
    return sm_Terse [eErr_UNKNOWN];
}

const string& CValidErrItem::GetMessage (void) const
{
    return m_Message;
}

const string& CValidErrItem::GetVerbose (void) const
{
    if (m_ErrIndex <= eErr_UNKNOWN) {
        return sm_Verbose [m_ErrIndex];
    }
    return sm_Verbose [eErr_UNKNOWN];
}

const CConstObjectInfo& CValidErrItem::GetObject (void) const
{
    return m_Object;
}

// ************************ CValidError_CI implementation **************
CValidError_CI::CValidError_CI(void) :
    m_ValidError(0),
    m_ErrCodeFilter(eErr_ALL), // eErr_UNKNOWN
    m_MinSeverity(eDiag_Info),
    m_MaxSeverity(eDiag_Critical)
{
}

CValidError_CI::CValidError_CI
(const CValidError& ve,
 string             errcode,
 EDiagSev           minsev,
 EDiagSev           maxsev) :
    m_ValidError(&ve),
    m_ErrIter(ve.m_ErrItems.begin()),
    m_MinSeverity(minsev),
    m_MaxSeverity(maxsev)
{
    if (errcode.empty()) {
        m_ErrCodeFilter = eErr_ALL;
    }

    for (unsigned int i = eErr_ALL; i < eErr_UNKNOWN; i++) {
        if (errcode == CValidErrItem::sm_Terse[i]) {
            m_ErrCodeFilter = i;
            return;
        }
    }
    m_ErrCodeFilter = eErr_ALL; // eErr_UNKNOWN
}


CValidError_CI::CValidError_CI(const CValidError_CI& iter) :
    m_ValidError(iter.m_ValidError),
    m_ErrIter(iter.m_ErrIter),
    m_ErrCodeFilter(iter.m_ErrCodeFilter),
    m_MinSeverity(iter.m_MinSeverity),
    m_MaxSeverity(iter.m_MaxSeverity)
{
}


CValidError_CI::~CValidError_CI(void)
{
}


CValidError_CI& CValidError_CI::operator= (const CValidError_CI& iter)
{
    if (this == &iter) {
        return *this;
    }

    m_ValidError = iter.m_ValidError;
    m_ErrIter = iter.m_ErrIter;
    m_ErrCodeFilter = iter.m_ErrCodeFilter;
    m_MinSeverity = iter.m_MinSeverity;
    m_MaxSeverity = iter.m_MaxSeverity;
    return *this;
}


CValidError_CI& CValidError_CI::operator++ (void)
{
    if (m_ErrIter != m_ValidError->m_ErrItems.end()) {
        ++m_ErrIter;
    }
    
    for (; m_ErrIter != m_ValidError->m_ErrItems.end(); ++m_ErrIter) {
        if ((m_ErrCodeFilter == eErr_ALL  ||
            (**m_ErrIter).GetErrCode() == CValidErrItem::sm_Terse[m_ErrCodeFilter])  &&
            (**m_ErrIter).GetSeverity() <= m_MaxSeverity  &&
            (**m_ErrIter).GetSeverity() >= m_MinSeverity) {
            break;
        }
    }
    return *this;
}


CValidError_CI::operator bool (void) const
{
    return m_ErrIter != m_ValidError->m_ErrItems.end();
}


const CValidErrItem& CValidError_CI::operator* (void) const
{
    return **m_ErrIter;
}


// ************************** CValidError_impl declaration **************
class CValidError_impl
{
public:
    //ctors
    CValidError_impl(CObjectManager&, TErrs&, unsigned int options = 0);
    virtual ~CValidError_impl(void);

    void Validate(const CSeq_entry& se, const CCit_sub* cs = 0);
    void Validate(const CSeq_submit& ss);

private:
    // Prohibit copy constructor & assignment operator
    CValidError_impl(const CValidError_impl&);
    CValidError_impl& operator= (const CValidError_impl&);

    CObjectManager* const m_ObjMgr;
    CRef<CScope> m_Scope;

    void SetScope(const CSeq_entry& se);

    // Top level Seq_entry, set by SetScope
    const CSeq_entry*   m_SeqEntry;
    TErrs*              m_Errors;

    // flags derived from options parameter
    bool m_NonASCII;                // User sets if Non ASCII char found
    bool m_SuppressContext;         // Include context in errors if true
    bool m_ValidateAlignments;      // Validate Alignments if true
    bool m_ValidateExons;           // Check exon feature splice sites
    bool m_SpliceErr;               // Bad splice site error if true, else warn
    bool m_OvlPepErr;               // Peptide overlap error if true, else warn
    bool m_RequireTaxonID;          // BioSource requires taxonID dbxref
    bool m_RequireISOJTA;           // Journal requires ISO JTA

    // flags calculated by examining data in record
    bool m_NoPubs;                  // Suppress no pub error if true
    bool m_NoBioSource;             // Suppress no organism error if true
    bool m_IsGPS;
    bool m_IsGED;
    bool m_IsPDB;
    bool m_IsTPA;
    bool m_IsPatent;
    bool m_IsRefSeq;
    bool m_IsNC;
    bool m_IsNG;
    bool m_IsNM;
    bool m_IsNP;
    bool m_IsNR;
    bool m_IsNS;
    bool m_IsNT;
    bool m_IsNW;
    bool m_IsXR;
    bool m_IsGI;

    CTextFsa                m_SourceQualTags;
    // seq ids contained withinh the orignal seq entry.
    set< CConstRef<CSeq_id> > m_InitialSeqIds;
    // prot bioseqs without a full reference
    vector< CConstRef<CBioseq> > m_ProtWithNoFullRef;
    // Bioseqs without pubs (should be considered only if m_NoPubs is false)
    vector< CConstRef<CBioseq> > m_BioseqWithNoPubs;
    // Bioseqs without source (should be considered only if m_NoSource is false)
    vector< CConstRef<CBioseq> > m_BioseqWithNoSource;

    // Validation methods
    void ValidateDescrChain(const CSeq_descr& descr);
    void ValidateSeqHist(const CSeq_hist& hist);
    void ValidateSeqGraph(const CSeq_graph& graph);
    void ValidateSeqAlign(const CSeq_align& align);
    void ValidateSeqFeat(const CSeq_feat& feat);
    void ValidateSeqAnnot(const CSeq_annot& annot);
    void ValidateSeqSet(const CBioseq_set& set);
    void ValidateNucProt(const CBioseq_set& seqset, int nuccnt, int protcnt);
    void ValidateSegSet(const CBioseq_set& seqset, int segcnt);
    void ValidatePartsSet(const CBioseq_set& seqset);
    void ValidatePopSet(const CBioseq_set& seqset);
    void ValidateGenProdSet(const CBioseq_set& seqset);
    void ValidateBioseq(const CBioseq& seq);
    void ValidateSeqDesc(const CSeqdesc& desc);
    void ValidateSeqDescContext(const CBioseq& seq);
    void ValidateSeqIds(const CBioseq& seq);
    void ValidateSeqLoc(const CSeq_loc& loc, const CBioseq& seq,
        const char* prefix);
    void ValidateInst(const CBioseq& seq);
    bool ValidateRepr(const CSeq_inst& inst, const CBioseq& seq);
    void ValidateSeqLen(const CBioseq& seq);
    void ValidateSeqParts(const CBioseq& seq);
    void ValidateProteinTitle(const CBioseq& seq);
    void ValidateBioseqContext(const CBioseq& seq);
    void ValidateMultiIntervalGene (const CBioseq& seq);
    void ValidateSeqFeatContext(const CBioseq& seq);
    void ValidateDupOrOverlapFeats(const CBioseq& seq);
    void ValidateCollidingGeneNames(const CBioseq& seq);
    void ValidateRawConst(const CBioseq& seq);
    void ValidateSegRef(const CBioseq& seq);
    void ValidateDelta(const CBioseq& seq);
    void ValidateBioSource(const CBioSource& bsrc, const CSerialObject& obj);
    void ValidateSourceQualTags(const string& str, const CSerialObject& obj);
    void ValidatePubdesc(const CPubdesc& pub, const CSerialObject& obj);
    void ValidateGene(const CGene_ref& gene, const CSerialObject& obj);
    void ValidateProt(const CProt_ref& prot, const CSerialObject& obj);
    void ValidateRna(const CRNA_ref& rna, const CSeq_feat& feat);
    void ValidateCdregion(const CCdregion& cdregion, const CSeq_feat& obj);
    void ValidateImp(const CImp_feat& imp, const CSeq_feat& feat);
    void ValidateImpGbquals(const CImp_feat& imp, const CSeq_feat& feat);
    void ValidateFeatPartialness(const CSeq_feat& feat);
    void ValidateExcept(const CSeq_feat& feat);
    void ValidateExceptText(const string& text, const CSeq_feat& feat);
    void ValidateDbxref(const CDbtag& xref, const CSerialObject& obj);
    void ValidateDbxref(const list< CRef< CDbtag > >& xref_list, const CSerialObject& obj);
    bool IsCountryValid(const string& str);
    bool OverlappingGeneIsPseudo(const CSeq_feat& obj);
    void CheckForBothStrands(const CSeq_feat& feat);
    void CdTransCheck(const CSeq_feat& feat);
    void SpliceCheck (const CSeq_feat& feat);
    void SpliceCheckEx(const CSeq_feat& feat, bool check_all);
    void CheckForBadGeneOverlap(const CSeq_feat& feat);
    void CheckForBadMRNAOverlap(const CSeq_feat& feat);
    void CheckForCommonCDSProduct(const CSeq_feat& feat);
    void CheckTrnaCodons(const CTrna_ext& trna, const CSeq_feat& feat);
    bool IsEqualSeqAnnot(const CFeat_CI& fi1, const CFeat_CI& fi2) const;
    bool IsEqualSeqAnnotDesc(const CFeat_CI& fi1, const CFeat_CI& fi2) const;
    void InitializeSourceQualTags();
    size_t SearchNoCase(const string& text, const string& pat) const;
    bool NotPeptideException(const CFeat_CI& curr, const CFeat_CI& prev) const;
    bool DifferentDbxrefs(const list< CRef< CDbtag > >& dbxref1,
                          const list< CRef< CDbtag > >& dbxref2) const;
    void ReportCdTransErrors(const CSeq_feat& feat,
        bool show_stop, bool got_stop, bool no_end, int ragged);
    int CheckForRaggedEnd(const CSeq_loc& loc, const CCdregion& cdregion);
    void CheckForCodeBreakNotOnCodon(const CSeq_feat& feat,const CSeq_loc& loc,
                                     const CCdregion& cdregion);
    bool IsDeltaOrFarSeg(const CSeq_loc& loc);
    bool IsFarLocation(const CSeq_loc& loc) const;
    void CheckForPubOnBioseq(const CBioseq& seq);
    void CheckForBiosourceOnBioseq(const CBioseq& seq);
    void ReportMissingPubs(const CBioseq& seq, const CCit_sub* cs);
    void ReportMissingBiosource(const CBioseq& seq);
    void ReportProtWithoutFullRef(void);

    // typedefs:
    typedef const CSeq_feat& TFeat;
    typedef const CBioseq& TBioseq;
    typedef const CBioseq_set& TSet;
    typedef const CSeqdesc& TDesc;
    typedef const CSeq_annot& TAnnot;


    // Posts errors.
    void ValidErr(EDiagSev sv, EErrType et, const string& msg,
        const CSerialObject& obj);
    void ValidErr(EDiagSev sv, EErrType et, const string& msg, TDesc ds);
    void ValidErr(EDiagSev sv, EErrType et, const string& msg, TFeat ft);
    void ValidErr(EDiagSev sv, EErrType et, const string& msg, TBioseq sq);
    void ValidErr(EDiagSev sv, EErrType et, const string& msg, TBioseq sq,
        TDesc ds);
    void ValidErr(EDiagSev sv, EErrType et, const string& msg, TSet set);
    void ValidErr(EDiagSev sv, EErrType et, const string& msg, TSet set, 
        TDesc ds);
    void ValidErr(EDiagSev sv, EErrType et, const string& msg, TAnnot an);

    // legal dbxref database strings
    static const char * legalDbXrefs [];
    static const char * legalRefSeqDbXrefs [];

    // legal country strings
    static const string sm_CountryCodes [];

    // legal exception strings
    static const string sm_LegalExceptionStrings [];
    static const string sm_RefseqExceptionStrings [];

    static const string sm_PlastidTxt [];

    static const string sm_SourceQualPrefixes [];

    static const string sm_BypassCdsTransCheck [];

    
};

//********************** CValidError_impl implementation ******************

const string CValidError_impl::sm_SourceQualPrefixes [] = {
  "acronym:",
  "anamorph:",
  "authority:",
  "biotype:",
  "biovar:",
  "breed:",
  "cell_line:",
  "cell_type:",
  "chemovar:",
  "chromosome:",
  "clone:",
  "clone_lib:",
  "common:",
  "country:",
  "cultivar:",
  "dev_stage:",
  "dosage:",
  "ecotype:",
  "endogenous_virus_name:",
  "environmental_sample:",
  "forma:",
  "forma_specialis:",
  "frequency:",
  "genotype:",
  "germline:",
  "group:",
  "haplotype:",
  "insertion_seq_name:",
  "isolate:",
  "isolation_source:",
  "lab_host:",
  "map:",
  "nat_host:",
  "pathovar:",
  "plasmid_name:",
  "plastid_name:",
  "pop_variant:",
  "rearranged:",
  "segment:",
  "serogroup:",
  "serotype:",
  "serovar:",
  "sex:",
  "specimen_voucher:",
  "strain:",
  "subclone:",
  "subgroup:",
  "substrain:",
  "subtype:",
  "sub_species:",
  "synonym:",
  "taxon:",
  "teleomorph:",
  "tissue_lib:",
  "tissue_type:",
  "transgenic:",
  "transposon_name:",
  "type:",
  "variety:",
};


const string CValidError_impl::sm_PlastidTxt[] = {
  "",
  "",
  "chloroplast",
  "chromoplast",
  "",
  "",
  "plastid",
  "",
  "",
  "",
  "",
  "",
  "cyanelle",
  "",
  "",
  "",
  "apicoplast",
  "leucoplast",
  "proplastid",
  "",
};


const string CValidError_impl::sm_LegalExceptionStrings[] = {
  "RNA editing",
  "reasons given in citation",
  "ribosomal slippage",
  "ribosome slippage",
  "trans splicing",
  "trans-splicing",
  "alternative processing",
  "alternate processing",
  "artificial frameshift",
  "non-consensus splice site",
  "nonconsensus splice site",
};


const string CValidError_impl::sm_RefseqExceptionStrings [] = {
  "unclassified transcription discrepancy",
  "unclassified translation discrepancy",
};


const string CValidError_impl::sm_BypassCdsTransCheck [] = {
  "RNA editing",
  "reasons given in citation",
  "artificial frameshift",
  "unclassified translation discrepancy",
};


CValidError_impl::CValidError_impl
(CObjectManager&     objmgr,
 TErrs&              errs,
 unsigned int        options)
    : m_ObjMgr(&objmgr),
      m_Scope(0),
      m_SeqEntry(0),
      m_Errors(&errs),
      m_NonASCII((options & CValidError::eVal_non_ascii) != 0),
      m_SuppressContext((options & CValidError::eVal_no_context) != 0),
      m_ValidateAlignments((options & CValidError::eVal_val_align) != 0),
      m_ValidateExons((options & CValidError::eVal_val_exons) != 0),
      m_SpliceErr((options & CValidError::eVal_splice_err) != 0),
      m_OvlPepErr((options & CValidError::eVal_ovl_pep_err) != 0),
      m_RequireTaxonID((options & CValidError::eVal_need_taxid) != 0),
      m_RequireISOJTA((options & CValidError::eVal_need_isojta) != 0),
      m_NoPubs(false),
      m_NoBioSource(false),
      m_IsGPS(false),
      m_IsGED(false),
      m_IsPDB(false),
      m_IsTPA(false),
      m_IsPatent(false),
      m_IsRefSeq(false),
      m_IsNC(false),
      m_IsNG(false),
      m_IsNM(false),
      m_IsNP(false),
      m_IsNR(false),
      m_IsNS(false),
      m_IsNT(false),
      m_IsNW(false),
      m_IsXR(false),
      m_IsGI(false)
{
    InitializeSourceQualTags();
}


CValidError_impl::~CValidError_impl()
{
}


inline
const CSeq_id& s_GetId(const CBioseq& seq)
{
    if (!seq.GetId().empty()) {
        return **seq.GetId().begin();
    } else {
        NCBI_THROW(CValidException, eSeqId, "Bioseq has no Seq-id");
    }
}


inline
const CTextseq_id& s_GetTextseq_id(const CSeq_id& id)
{
    switch (id.Which()) {
    case CSeq_id::e_Genbank:
        return id.GetGenbank();
    case CSeq_id::e_Embl:
        return id.GetEmbl();
    case CSeq_id::e_Pir:
        return id.GetPir();
    case CSeq_id::e_Swissprot:
        return id.GetSwissprot();
    case CSeq_id::e_Other:
        return id.GetOther();
    case CSeq_id::e_Ddbj:
        return id.GetDdbj();
    case CSeq_id::e_Prf:
        return id.GetPrf();
    case CSeq_id::e_Tpg:
        return id.GetTpg();
    case CSeq_id::e_Tpe:
        return id.GetTpe();
    case CSeq_id::e_Tpd:
        return id.GetTpd();
    default:
        NCBI_THROW(CValidException, eSeqId, "Seq-id type not handled");
    }
}


inline
const string& s_GetAccession(const CSeq_id& id)
{
    try {
        const CTextseq_id& tid = s_GetTextseq_id(id);
        if (tid.IsSetAccession()) {
            return tid.GetAccession();
        } else {
            NCBI_THROW(CValidException, eSeqId, "Accession not set");
        }
    } catch(CValidException& e) {
        NCBI_RETHROW(e, CValidException, eSeqId, "Accession not set");        
    }
}


inline
const string& s_GetName(const CSeq_id& id)
{
    try {
        const CTextseq_id& tid = s_GetTextseq_id(id);
        if (tid.IsSetName()) {
            return tid.GetName();
        } else {
            NCBI_THROW(CValidException, eSeqId, "Name not set");
        }
    } catch (CValidException& e) {
        NCBI_RETHROW(e, CValidException, eSeqId, "Name not set");        
    }        
}


inline
static bool s_isNa(const CSeq_inst::EMol mol)
{
    if (mol == CSeq_inst::eMol_dna  ||  mol == CSeq_inst::eMol_rna  ||
        mol == CSeq_inst::eMol_na)
    {
        return true;
    } else {
        return false;
    }
}


inline
static bool s_isNa(const CSeq_inst& inst)
{
    const CSeq_inst::EMol& mol = inst.GetMol();
    return s_isNa(mol);
}


inline
static bool s_isNa(const CBioseq& seq)
{
    return s_isNa(seq.GetInst());
}


inline
static bool s_isAa(const CSeq_inst& inst)
{
    return inst.GetMol() == CSeq_inst::eMol_aa;
}


inline
static bool s_isAa(const CBioseq& seq)
{
    return s_isAa(seq.GetInst());
}


inline 
static bool s_IsMrna(const CBioseq_Handle& bsh) 
{
    CSeqdesc_CI sd(bsh, CSeqdesc::e_Molinfo);

    if ( sd ) {
        const CMolInfo &mi = sd->GetMolinfo();
        if ( mi.IsSetBiomol() ) {
            return mi.GetBiomol() == CMolInfo::eBiomol_mRNA;
        }
    }

    return false;
}


inline 
static bool s_IsPrerna(const CBioseq_Handle& bsh) 
{
    CSeqdesc_CI sd(bsh, CSeqdesc::e_Molinfo);

    if ( sd ) {
        const CMolInfo &mi = sd->GetMolinfo();
        if ( mi.IsSetBiomol() ) {
            return mi.GetBiomol() == CMolInfo::eBiomol_pre_RNA;
        }
    }

    return false;
}


inline
static TSeqPos s_GetDataLen(const CSeq_inst& inst)
{
    if (!inst.IsSetSeq_data()) {
        return 0;
    }

    const CSeq_data& seqdata = inst.GetSeq_data();
    switch (seqdata.Which()) {
    case CSeq_data::e_not_set:
        return 0;
    case CSeq_data::e_Iupacna:
        return seqdata.GetIupacna().Get().size();
    case CSeq_data::e_Iupacaa:
        return seqdata.GetIupacaa().Get().size();
    case CSeq_data::e_Ncbi2na:
        return seqdata.GetNcbi2na().Get().size();
    case CSeq_data::e_Ncbi4na:
        return seqdata.GetNcbi4na().Get().size();
    case CSeq_data::e_Ncbi8na:
        return seqdata.GetNcbi8na().Get().size();
    case CSeq_data::e_Ncbipna:
        return seqdata.GetNcbipna().Get().size();
    case CSeq_data::e_Ncbi8aa:
        return seqdata.GetNcbi8aa().Get().size();
    case CSeq_data::e_Ncbieaa:
        return seqdata.GetNcbieaa().Get().size();
    case CSeq_data::e_Ncbipaa:
        return seqdata.GetNcbipaa().Get().size();
    case CSeq_data::e_Ncbistdaa:
        return seqdata.GetNcbistdaa().Get().size();
    default:
        return 0;
    }
}


inline
static bool s_IsGEDSeqEmbedded(const CSeq_entry& se)
{
    for (CTypeConstIterator<CBioseq> seq(ConstBegin(se)); seq; ++seq) {
        iterate (CBioseq::TId, id, seq->GetId()) {
            switch ((**id).Which()) {
            case CSeq_id::e_Genbank:
            case CSeq_id::e_Embl:
            case CSeq_id::e_Ddbj:
            case CSeq_id::e_Tpg:
            case CSeq_id::e_Tpe:
            case CSeq_id::e_Tpd:
                return true;
            default:
                break;
            }
        }
    }
    return false;
}


inline
static bool s_IsIdIn(const CSeq_id& id, const CBioseq& seq)
{
    iterate (CBioseq::TId, it, seq.GetId()) {
        if (id.Match(**it)) {
            return true;
        }
    }
    return false;
}


// TO BE REPLACED
inline
static const CSeq_feat* s_GetCDSForProduct
(const CBioseq& seq,
 CScope* const  scope)
{
     // Get id for CBioseq
     const CSeq_id* id = seq.GetFirstId();
 
     // Return null poiner if no id
     if (!id) {
         return 0;
     }
 
     CSeq_loc loc;
     loc.SetWhole().Assign(*id);
     CFeat_CI fi(*scope, loc, CSeqFeatData::e_Cdregion);
     return &(*fi);
}


// TO BE REPLACED
// Check if CdRegion required but not found
static bool s_CdError(const CBioseq& seq, CScope* scope)
{
    const CSeq_id* sid = seq.GetFirstId();
     if (s_isAa(seq)  &&  sid) {
         const CSeq_entry* parent = seq.GetParentEntry();
         const CSeq_entry* grand_parent;
         if (parent) {
             grand_parent = parent->GetParentEntry();
         }
         if (grand_parent  &&  grand_parent->IsSet()) {
             const CBioseq_set& set = grand_parent->GetSet();
             if (set.IsSetClass()  &&
                 set.GetClass() == CBioseq_set::eClass_nuc_prot) {
                 CTypeConstIterator<CSeq_feat> ft(ConstBegin(set));
                 bool isFoundCd = false;
                 for (; ft; ++ft) {
                     if (ft->IsSetProduct()  &&
                         ft->GetData().Which() == CSeqFeatData::e_Cdregion) {
                         const CSeq_loc& loc = ft->GetProduct();
                         const CSeq_id& lid = GetId(loc, scope);
                         if (IsSameBioseq(*sid, lid, scope)) {
                             isFoundCd = true;
                             break;
                         }
                     }
                 }
                 return !isFoundCd;
             }
         }
     }
     return false;
}


inline
static bool s_SameAnnotDesc(const CAnnot_descr& d1, const CAnnot_descr& d2)
{
    const list< CRef<CAnnotdesc> >& ad1 = d1.Get();
    const list< CRef<CAnnotdesc> >& ad2 = d2.Get();

    if (ad1.size() != ad2.size()) {
        return false;
    }

    list< CRef<CAnnotdesc> >::const_iterator i1 = ad1.begin();
    list< CRef<CAnnotdesc> >::const_iterator i2 = ad2.begin();
    for (; i1 != ad1.end(); ++i1, ++i2) {
        if ((**i1).Which() != (**i2).Which()) {
            return false;
        }
        switch ((**i1).Which()) {
        case CAnnotdesc::e_Name:
            if ((**i1).GetName() != (**i2).GetName()) {
                return false;
            }
            break;
        case CAnnotdesc::e_Title:
            if ((**i1).GetTitle() != (**i2).GetTitle()) {
                return false;
            }
            break;
        default:
            return false;
        }
    }
    return true;
}


inline
static bool s_IsSameComment(const CSeq_feat& f1, const CSeq_feat& f2)
{
    if (f1.IsSetComment()  &&  !f2.IsSetComment()) {
        return false;
    } else if (!f1.IsSetComment()  &&  f2.IsSetComment()) {
        return false;
    } else if (!f1.IsSetComment()  && !f2.IsSetComment()) {
        return true;
    } else {
        return f1.GetComment() == f2.GetComment();
    }
}


inline
static bool s_IsDifferentFrame(const CSeq_feat& f1, const CSeq_feat& f2)
{
    if (!f1.GetData().IsCdregion()  ||  !f2.GetData().IsCdregion()) {
        return false;
    }

    if (!f1.GetData().GetCdregion().IsSetFrame()  ||
        !f2.GetData().GetCdregion().IsSetFrame()) {
            return false;
    }

    return f1.GetData().GetCdregion().GetFrame() !=
        f2.GetData().GetCdregion().GetFrame();
}


inline
static bool s_IsWarn(CSeqFeatData::ESubtype sub_type)
{
    switch (sub_type) {
    case CSeqFeatData::eSubtype_pub:
    case CSeqFeatData::eSubtype_region:
    case CSeqFeatData::eSubtype_misc_feature:
    case CSeqFeatData::eSubtype_STS:
    case CSeqFeatData::eSubtype_variation:
        return true;
    default:
        return false;
    }
}


// Finds the first COrg_ref up the CSeq_entry tree from seq and
// return taxname, if found
inline
static string s_GetTaxname(const CBioseq& seq)
{
    const CSeq_entry* se = seq.GetParentEntry();
    for (; se; se->GetParentEntry()) {
        CTypeConstIterator<COrg_ref> ref(ConstBegin(*se));
        if (ref) {
            if (ref->IsSetTaxname()) {
                return ref->GetTaxname();
            }
            return "";
        }
    }
    return "";
}


inline
static bool s_IsDifferentDbxref(const CSeq_feat& f1, const CSeq_feat& f2)
{
    if (!f1.IsSetDbxref()  ||  !f2.IsSetDbxref()) {
        return false;
    }

    const list< CRef<CDbtag> >& list1 = f1.GetDbxref();
    const list< CRef<CDbtag> >& list2 = f2.GetDbxref();
   
    if (list1.empty()  ||  list2.empty()) {
        return false;
    } else if (list1.size() != list2.size()) {
        return true;
    }

    list< CRef<CDbtag> >::const_iterator it1 = list1.begin();
    list< CRef<CDbtag> >::const_iterator it2 = list2.begin();
    for (; it1 != list1.end(); ++it1, ++it2) {
        if ((**it1).GetDb() != (**it2).GetDb()) {
            return true;
        }
        string str1 =
            (**it1).GetTag().IsStr() ? (**it1).GetTag().GetStr() : "";
        string str2 =
            (**it2).GetTag().IsStr() ? (**it2).GetTag().GetStr() : "";
        if (str1.empty()  &&  str2.empty()) {
            if (!(**it1).GetTag().IsId()  &&  !(**it2).GetTag().IsId()) {
                continue;
            } else if ((**it1).GetTag().IsId()  &&  (**it2).GetTag().IsId()) {
                if ((**it1).GetTag().GetId() != (**it2).GetTag().GetId()) {
                    return true;
                }
            } else {
                return true;
            }
        }
    }
    return false;
}


// Returns true if seq derived from translation ending in "*" or
// seq is 3' partial (i.e. the right of the sequence is incomplete)
inline
static bool s_SuppressTrailingXMsg(const CBioseq& seq, CScope* const scope)
{

    // Look for the Cdregion feature used to create this aa product
    // Use the Cdregion to translate the associated na sequence
    // and check if translation has a '*' at the end. If it does.
    // message about 'X' at the end of this aa product sequence is suppressed
    const CSeq_feat* fi = s_GetCDSForProduct(seq, scope);
    if (fi) {
    
        // Get CCdregion 
        CTypeConstIterator<CCdregion> cdr(ConstBegin(*fi));
        
        // Get location on source sequence
        const CSeq_loc& loc = (*fi).GetLocation();

        // Get CSeq_id of source sequence
        const CSeq_id& id = GetId(loc);

        // Get CBioseq_Handle for source sequence
        CBioseq_Handle hnd = scope->GetBioseqHandle(id);

        // Translate na CSeq_data
        string prot;        
        CCdregion_translate::TranslateCdregion(prot, hnd, loc, *cdr);
        
        if (!NStr::CompareCase(prot.substr(prot.size()-1, 1), "*")) {
            return true;
        }
        return false;
    }

    // Get CMolInfo for seq and determine if completeness is
    // "eCompleteness_no_right or eCompleteness_no_ends. If so
    // suppress message about "X" at end of aa sequence is suppressed
    CTypeConstIterator<CMolInfo> mi = ConstBegin(seq);
    if (mi  &&  mi->IsSetCompleteness()) {
        if (mi->GetCompleteness() == CMolInfo::eCompleteness_no_right  ||
          mi->GetCompleteness() == CMolInfo::eCompleteness_no_ends) {
            return true;
        }
    }
    return false;
}


// TO BE REPLACED
// Called by s_GetGeneLabel
inline
static const CGene_ref* s_GetGeneRef(const CSeq_feat& cds)
{
    CTypeConstIterator<CGene_ref> grf = ConstBegin(cds);
    if (grf) {
        return &(*grf);
    } else {
        return 0;
    }
}


// TO BE REPLACED
// Get Seq-feat with SeqFeatData of type Gene-ref that best
// contains location of cds -- used to create a label
// for error message
inline
static const CSeq_feat* s_GetContainingGene
(const CSeq_feat* cds,
CScope*           scope)
{
    if (!cds) {
        return 0;
    }

    // Iterate through Seq-feats of type Gene-ref that overlap cds location
    // and pick the best one -- the one containing cds location
    // that least differs from cds location.
    CFeat_CI fi(*scope, cds->GetLocation(), CSeqFeatData::e_Gene);
    TSeqPos best_diff = kMax_UInt;
    TSeqPos diff;
    const CSeq_feat* best = 0;
    for (; fi; ++fi) {
        try {
            TSeqPos cds_left = GetStart(cds->GetLocation(), scope);
            TSeqPos fi_left = GetStart(fi->GetLocation(), scope);
            if (fi_left > cds_left) {
                continue;
            }
            TSeqPos cds_right = GetLength(cds->GetLocation(), scope)
                + cds_left - 1;
            TSeqPos fi_right = GetLength(fi->GetLocation(), scope) +
                fi_left - 1;
            if (fi_right < cds_right) {
                continue;
            }
            diff = cds_left - fi_left + fi_right - cds_right;
            if (diff < best_diff) {
                best_diff = diff;
                best = &(*fi);
            }
        } catch(const CNoLength&) {}
    }
    return best;
}


// TO BE REPLACED
// Gets a label for the gene sequence that produces the sequence seq.
inline
static void s_GetGeneLabel(const CBioseq& seq, string* lbl, CScope* scope)
{
    if (!lbl) {
        return;
    }

    // Get the cdregion that created this product
    const CSeq_feat* cds = s_GetCDSForProduct(seq, scope);
    const CGene_ref* grf;
    const CSeq_feat* gene;
    grf = cds ? s_GetGeneRef(*cds) : 0;
    if (!grf  &&  cds) {
        gene = s_GetContainingGene(cds, scope);
        if (gene) {
            if (!(grf = s_GetGeneRef(*gene))) {
                (*lbl) = "gene?";
                return;
            }
        }
    }
    if (grf->IsSetLocus()) {
        (*lbl) = grf->GetLocus();
    } else if (grf->IsSetDesc()) {
        (*lbl) = grf->GetDesc();
    } else if (grf->IsSetSyn()  &&  !grf->GetSyn().empty()) {
        (*lbl) = *grf->GetSyn().begin();
    }
    if (lbl->empty()) {
        (*lbl) = "gene?";
    }
    return;
}


// Gets the label for a sequence -- used to get the label for a protein
inline
static void s_GetSeqLabel
(const CBioseq& seq,
 string*        lbl,
 CScope*        scope,
 const char*  default_label = "prot?")
{
    if (!lbl) {
        return;
    }
    // Get id for CBioseq
    const CSeq_id* id = (*seq.GetId().begin()).GetPointer();

    // Return if no id
    if (!id) {
        (*lbl) = default_label;
        return;
    }

    // Get the protein label.
    CBioseq_Handle hnd = scope->GetBioseqHandle(*id);
    (*lbl) = GetTitle(hnd);
    if (lbl->empty()) {
        (*lbl) = default_label;
    }
    return;
}


inline
static bool s_GetLocFromSeq(const CBioseq& seq, CSeq_loc* loc)
{
    if (!seq.GetInst().IsSetExt()  ||  !seq.GetInst().GetExt().IsSeg()) {
        return false;
    }

    CSeq_loc_mix& mix = loc->SetMix();
    iterate (list< CRef<CSeq_loc> >, it,
        seq.GetInst().GetExt().GetSeg().Get()) {
        mix.Set().push_back(*it);
    }
    return true;
}


// If seq has extension data, wraps it in a Seq-loc of type mix.
enum ESeqlocPartial {
    eSeqlocPartial_Complete   =   0,
    eSeqlocPartial_Start      =   1,
    eSeqlocPartial_Stop       =   2,
    eSeqlocPartial_Internal   =   4,
    eSeqlocPartial_Other      =   8,
    eSeqlocPartial_Nostart    =  16,
    eSeqlocPartial_Nostop     =  32,
    eSeqlocPartial_Nointernal =  64,
    eSeqlocPartial_Limwrong   = 128,
    eSeqlocPartial_Haderror   = 256
};


static unsigned int s_GetSeqlocPartialInfo(const CSeq_loc& loc, CScope* scope)
{
    unsigned int retval = 0;
    if (!scope) {
        return retval;
    }

    // Find first and last Seq-loc
    const CSeq_loc *first = 0, *last = 0;
    CTypeConstIterator<CSeq_loc> i1(ConstBegin(loc));
    for (++i1; i1; ++i1) {
        if (!first) {
            first = &(*i1);
        }
        last = &(*i1);
    }
    if (!first) {
        return retval;
    }

    CTypeConstIterator<CSeq_loc> i2(ConstBegin(loc));
    for (++i2; i2; ++i2) {
        switch ((*i2).Which()) {
        case CSeq_loc::e_Null:
            if (&(*i2) == first) {
                retval |= eSeqlocPartial_Start;
            } else if (&(*i2) == last) {
                retval |= eSeqlocPartial_Stop;
            } else {
                retval |= eSeqlocPartial_Internal;
            }
            break;
        case CSeq_loc::e_Int:
            if ((*i2).GetInt().IsSetFuzz_from()) {
                const CInt_fuzz& fuzz = (*i2).GetInt().GetFuzz_from();
                if (fuzz.Which() == CInt_fuzz::e_Lim) {
                    CInt_fuzz::ELim lim = fuzz.GetLim();
                    if (lim == CInt_fuzz::eLim_gt) {
                        retval |= eSeqlocPartial_Limwrong;
                    } else if (lim == CInt_fuzz::eLim_lt  ||
                        lim == CInt_fuzz::eLim_unk) {
                        if ((*i2).GetInt().IsSetStrand()  &&
                            (*i2).GetInt().GetStrand() == eNa_strand_minus) {
                            if (&(*i2) == last) {
                                retval |= eSeqlocPartial_Stop;
                            } else {
                                retval |= eSeqlocPartial_Internal;
                            }
                            if ((*i2).GetInt().GetFrom() != 0) {
                                if (&(*i2) == last) {
                                    retval |= eSeqlocPartial_Nostop;
                                } else {
                                    retval |= eSeqlocPartial_Nointernal;
                                }
                            }
                        } else {
                            if (&(*i2) == first) {
                                retval |= eSeqlocPartial_Start;
                            } else {
                                retval |= eSeqlocPartial_Internal;
                            }
                            if ((*i2).GetInt().GetFrom() != 0) {
                                if (&(*i2) == first) {
                                    retval |= eSeqlocPartial_Nostart;
                                } else {
                                    retval |= eSeqlocPartial_Nointernal;
                                }
                            }
                        }
                    }
                }
            }

            if ((*i2).GetInt().IsSetFuzz_to()) {
                const CInt_fuzz& fuzz = (*i2).GetInt().GetFuzz_to();
                CInt_fuzz::ELim lim = fuzz.GetLim();
                if (lim == CInt_fuzz::eLim_lt) {
                    retval |= eSeqlocPartial_Limwrong;
                } else if (lim == CInt_fuzz::eLim_gt  ||
                    lim == CInt_fuzz::eLim_unk) {
                    CBioseq_Handle hnd =
                        scope->GetBioseqHandle((*i2).GetInt().GetId());
                    CBioseq_Handle::TBioseqCore bc = hnd.GetBioseqCore();
                    bool miss_end = false;
                    const CSeq_interval& itv = (*i2).GetInt();
                    if (itv.GetTo() != bc->GetInst().GetLength() - 1) {
                        miss_end = true;
                    }
                    if (itv.IsSetStrand()  &&
                        itv.GetStrand() == eNa_strand_minus) {
                        if (&(*i2) == first) {
                            retval |= eSeqlocPartial_Start;
                        } else {
                            retval |= eSeqlocPartial_Internal;
                        }
                        if (miss_end) {
                            if (&(*i2) == last) {
                                retval |= eSeqlocPartial_Nostart;
                            } else {
                                retval |= eSeqlocPartial_Nointernal;
                            }
                        }
                    } else {
                        if (&(*i2) == last) {
                            retval |= eSeqlocPartial_Stop;
                        } else {
                            retval |= eSeqlocPartial_Internal;
                        }
                        if (miss_end) {
                            if (&(*i2) == last) {
                                retval |= eSeqlocPartial_Nostop;
                            } else {
                                retval |= eSeqlocPartial_Nointernal;
                            }
                        }
                    }
                }
            }
            break;
        case CSeq_loc::e_Pnt:
            if ((*i2).GetPnt().IsSetFuzz()) {
                const CInt_fuzz& fuzz = (*i2).GetPnt().GetFuzz();
                if (fuzz.Which() == CInt_fuzz::e_Lim) {
                    CInt_fuzz::ELim lim = fuzz.GetLim();
                    if (lim == CInt_fuzz::eLim_gt  ||
                        lim == CInt_fuzz::eLim_lt  ||
                        lim == CInt_fuzz::eLim_unk) {
                        if (&(*i2) == first) {
                            retval |= eSeqlocPartial_Start;
                        } else if (&(*i2) == last) {
                            retval |= eSeqlocPartial_Stop;
                        } else {
                            retval |= eSeqlocPartial_Internal;
                        }
                    }
                }
            }
            break;
        case CSeq_loc::e_Packed_pnt:
            if ((*i2).GetPacked_pnt().IsSetFuzz()) {
                const CInt_fuzz& fuzz = (*i2).GetPacked_pnt().GetFuzz();
                if (fuzz.Which() == CInt_fuzz::e_Lim) {
                    CInt_fuzz::ELim lim = fuzz.GetLim();
                    if (lim == CInt_fuzz::eLim_gt  ||
                        lim == CInt_fuzz::eLim_lt  ||
                        lim == CInt_fuzz::eLim_unk) {
                        if (&(*i2) == first) {
                            retval |= eSeqlocPartial_Start;
                        } else if (&(*i2) == last) {
                            retval |= eSeqlocPartial_Stop;
                        } else {
                            retval |= eSeqlocPartial_Internal;
                        }
                    }
                }
            }
            break;
        case CSeq_loc::e_Whole:
        {
            // Get the Bioseq referred to by Whole
            CBioseq_Handle hnd = scope->GetBioseqHandle((*i2).GetWhole());
            CBioseq_Handle::TBioseqCore bc = hnd.GetBioseqCore();
            if (!(*bc).IsSetDescr()) {
                // If no CSeq_descr, nothing can be done
                break;
            }
            // First try to loop through CMolInfo
            CTypeConstIterator<CMolInfo> mi(ConstBegin((*bc).GetDescr()));
            bool found_molinfo = mi ? true : false;
            for (; mi; ++mi) {
                if (!(*mi).IsSetCompleteness()) {
                    continue;
                }
                switch ((*mi).GetCompleteness()) {
                case CMolInfo::eCompleteness_no_left:
                    if (&(*i2) == first) {
                        retval |= eSeqlocPartial_Start;
                    } else {
                        retval |= eSeqlocPartial_Internal;
                    }
                    break;
                case CMolInfo::eCompleteness_no_right:
                    if (&(*i2) == last) {
                        retval |= eSeqlocPartial_Stop;
                    } else {
                        retval |= eSeqlocPartial_Internal;
                    }
                    break;
                case CMolInfo::eCompleteness_partial:
                    retval |= eSeqlocPartial_Other;
                    break;
                case CMolInfo::eCompleteness_no_ends:
                    retval |= eSeqlocPartial_Start;
                    retval |= eSeqlocPartial_Stop;
                    break;
                default:
                    break;
                }
            }
            if (found_molinfo) {
                break;
            }
            break;
        }
        default:
            break;
        }
    }
    return retval;
}


// TO BE REPLACED
static bool s_SeqLocMixedStrands
(const CBioseq&,  //seq, 
 const CSeq_loc&) // loc)
{
    return true;
}



// Returns true if an object of type T is embedded in in object of type K,
// else false
template <class T, class K>
bool s_AnyObj(const K& obj)
{
    CTypeConstIterator<T> i(ConstBegin(obj));
    return i ? true : false;
}


// Returns true if first CBioseq in se has CSeq_id of type C.
template <CSeq_id::E_Choice C>
bool s_IsType(const CSeq_entry& se)
{
    // Get first CBioseq
    CTypeConstIterator<CBioseq> seq(ConstBegin(se));
    if (seq) {
    list< CRef<CSeq_id> >::const_iterator id = seq->GetId().begin();
        // Loop thru CSeq_ids of seq & return true if C type id found
        for (;id != seq->GetId().end(); ++id)
        {
            if ((*id)->Which() == C) {
                return true;
            }
        }
    }
    return false;
}

/*
inline
static bool s_NotPeptideException(const CSeq_feat& ft)
{
    if (!ft.IsSetExcept_text()) {
        return true;
    }

    if (ft.GetExcept_text().find("alternative processing") != string::npos) {
        return false;
    }

    if (ft.GetExcept_text().find("alternate processing") != string::npos) {
        return false;
    }

    return true;
}
*/

void CValidError_impl::ValidErr
(EDiagSev sv,
 EErrType et,
 const string&  msg,
 const CSerialObject& obj)
{
    const CSeqdesc* desc = dynamic_cast < const CSeqdesc* > (&obj);
    if (desc != 0) {
        ValidErr (sv, et, msg, *desc);
        return;
    }
    const CSeq_feat* feat = dynamic_cast < const CSeq_feat* > (&obj);
    if (feat != 0) {
        ValidErr (sv, et, msg, *feat);
        return;
    }
    const CBioseq* seq = dynamic_cast < const CBioseq* > (&obj);
    if (seq != 0) {
        ValidErr (sv, et, msg, *seq);
        return;
    }
    const CBioseq_set* set = dynamic_cast < const CBioseq_set* > (&obj);
    if (set != 0) {
        ValidErr (sv, et, msg, *set);
        return;
    }
    const CSeq_annot* annot = dynamic_cast < const CSeq_annot* > (&obj);
    if (annot != 0) {
        ValidErr (sv, et, msg, *annot);
        return;
    }
}


void CValidError_impl::ValidErr
(EDiagSev sv,
 EErrType et,
 const string&   message,
 TDesc    ds)
{
    // Append Descriptor label
    string msg(message + " DESCRIPTOR: ");
    ds.GetLabel (&msg, CSeqdesc::eBoth);

    m_Errors->push_back(CRef<CValidErrItem>
        (new CValidErrItem(sv, et, msg, ds)));
}


void CValidError_impl::ValidErr
(EDiagSev sv,
 EErrType et,
 const string&   message,
 TFeat    ft)
{
    // Add feature part of label
    string msg(message + " FEATURE: ");
    feature::GetLabel(ft, &msg, feature::eBoth, m_Scope.GetPointer());

    // Add feature location part of label
    string loc_label;
    if (m_SuppressContext) {
        CSeq_loc loc;
        SerialAssign(loc, ft.GetLocation());
        ChangeSeqLocId(&loc, false, m_Scope.GetPointer());
        loc.GetLabel(&loc_label);
    } else {
        ft.GetLocation().GetLabel(&loc_label);
    }
    if (loc_label.size() > 800) {
        loc_label = loc_label.substr(0, 797) + "...";
    }
    if (!loc_label.empty()) {
        loc_label = string("[") + loc_label + "]";
        msg += loc_label;
    }

    // Append label for bioseq of feature location
    if (!m_SuppressContext) {
        try {
            const CSeq_id& id = GetId(ft.GetLocation(), m_Scope.GetPointer());
            CBioseq_Handle hnd = m_Scope.GetPointer()->GetBioseqHandle(id);
            CBioseq_Handle::TBioseqCore bc = hnd.GetBioseqCore();
            msg += "[";
            bc->GetLabel(&msg, CBioseq::eBoth);
            msg += "]";
        } catch (...){};
    }

    // Append label for product of feature
    loc_label.erase();
    if (ft.IsSetProduct()) {
        if (m_SuppressContext) {
            CSeq_loc loc;
            SerialAssign(loc, ft.GetProduct());
            ChangeSeqLocId(&loc, false, m_Scope.GetPointer());
            loc.GetLabel(&loc_label);
        } else {
            ft.GetProduct().GetLabel(&loc_label);
        }
        if (loc_label.size() > 800) {
            loc_label = loc_label.substr(0, 797) + "...";
        }
        if (!loc_label.empty()) {
            loc_label = string("[") + loc_label + "]";
            msg += loc_label;
        }
    }
    m_Errors->push_back(CRef<CValidErrItem>
        (new CValidErrItem(sv, et, msg, ft)));
}


void CValidError_impl::ValidErr
(EDiagSev sv,
 EErrType et,
 const string&   message,
 TBioseq  sq)
{
    // Append bioseq label
    string msg(message + " BIOSEQ: ");
    if (m_SuppressContext) {
        sq.GetLabel(&msg, CBioseq::eContent, true);
    } else {
        sq.GetLabel(&msg, CBioseq::eBoth, false);
    }
    m_Errors->push_back(CRef<CValidErrItem>
        (new CValidErrItem(sv, et, msg, sq)));
}


void CValidError_impl::ValidErr
(EDiagSev sv,
 EErrType et,
 const string&   message,
 TBioseq  sq,
 TDesc    ds)
{
    // Append Descriptor label
    string msg(message + " DESCRIPTOR: ");
    ds.GetLabel(&msg, CSeqdesc::eBoth);
    ValidErr(sv, et, msg, sq);
}


void CValidError_impl::ValidErr
(EDiagSev      sv,
 EErrType      et,
 const string& message,
 TSet          set)
{
    // Append Bioseq_set label
    string msg(message + " BIOSEQ-SET: ");
    if (m_SuppressContext) {
        set.GetLabel(&msg, CBioseq_set::eContent);
    } else {
        set.GetLabel(&msg, CBioseq_set::eBoth);
    }
    m_Errors->push_back(CRef<CValidErrItem>
                        (new CValidErrItem(sv, et, msg, set)));
}


void CValidError_impl::ValidErr
(EDiagSev        sv,
 EErrType        et,
 const string&   message,
 TSet            set,
 TDesc           ds)
{
    // Append Descriptor label
    string msg(message + " DESCRIPTOR: ");
    ds.GetLabel(&msg, CSeqdesc::eBoth);
    ValidErr(sv, et, msg, set);
}


void CValidError_impl::ValidErr
(EDiagSev sv,
 EErrType et,
 const string&   message,
 TAnnot    an)
{
    // Append Annotation label
    string msg(message + " ANNOTAION: ");

    // !!! need to decide on the message

    m_Errors->push_back(CRef<CValidErrItem>
        (new CValidErrItem(sv, et, msg, an)));
}


void CValidError_impl::SetScope(const CSeq_entry& se)
{
    m_SeqEntry = &se;
    m_Scope.Reset(new CScope(*m_ObjMgr));
    m_Scope->AddTopLevelSeqEntry(*const_cast<CSeq_entry*>(&se));
    m_Scope->AddDefaults();
}


void CValidError_impl::ValidateDescrChain(const CSeq_descr& descr)
{
    int numSources = 0;
    int numTitles = 0;
    const CSeqdesc * lastsource = 0;
    const CSeqdesc * lasttitle = 0;

    for (CTypeConstIterator <CSeqdesc> dt (descr); dt; ++dt) {
        switch (dt->Which ()) {
            case CSeqdesc::e_Title:
                numTitles++;
                lasttitle = &(*dt);
                break;
            case CSeqdesc::e_Source:
                numSources++;
                lastsource = &(*dt);
                break;
            default:
                break;
        }
    }
    if (numSources > 1) {
        ValidErr(eDiag_Error, eErr_SEQ_DESCR_MultipleBioSources,
            "Multiple BioSource blocks", *lastsource);
    }
    if (numTitles > 1) {
        ValidErr(eDiag_Error, eErr_SEQ_DESCR_MultipleTitles,
            "Multiple Title blocks", *lasttitle);
    }
}


void CValidError_impl::ValidateSeqDescContext(const CBioseq& seq)
{
    // !!! is everything done?
    bool prf_found = false;
    const CDate* create_date_prev = 0;
    
    for (CTypeConstIterator<CSeqdesc> dt(ConstBegin(seq)); dt; ++dt) {
        switch (dt->Which()) {
        case CSeqdesc::e_Prf:
            if (prf_found) {
                ValidErr(eDiag_Error, eErr_SEQ_DESCR_Inconsistent,
                    "Multiple PRF blocks", *dt);
            } else {
                prf_found = true;
            }
            break;
        case CSeqdesc::e_Create_date:
            if (create_date_prev) {
                if (create_date_prev->Compare(dt->GetCreate_date()) !=
                    CDate::eCompare_same) {
                    string str_date_prev;
                    string str_date;
                    create_date_prev->GetDate(&str_date_prev);
                    dt->GetCreate_date().GetDate(&str_date);
                    ValidErr(eDiag_Warning, eErr_SEQ_DESCR_Inconsistent,
                        "Inconsistent create_dates " + str_date +
                        " and " + str_date_prev, *dt);
                }
            } else {
                create_date_prev = &dt->GetCreate_date();
            }

            break;
        default:
            break;
        }
    }
}


void CValidError_impl::ValidateSeqIds
(const CBioseq& seq)
{

    // Ensure that CBioseq has at least one CSeq_id
    if (!seq.GetId().size()) {
        ValidErr(eDiag_Critical, eErr_SEQ_INST_NoIdOnBioseq,
                 "No ids on a Bioseq", seq);
    }

    // Loop thru CSeq_ids for this CBioseq. Determine if seq has
    // gi, NT, or NC. Check that the same CSeq_id not included more
    // than once.
    iterate (CBioseq::TId, i, seq.GetId()) {

        // Check that no two CSeq_ids for same CBioseq are same type
        list< CRef < CSeq_id > >::const_iterator j;
        for (j = i, ++j; j != seq.GetId().end(); ++j) {
            if ((**i).Compare(**j) != CSeq_id::e_DIFF) {
                CNcbiOstrstream os;
                /*
                os << "Conflicting ids on a Bioseq: ("
                           << (**i).DumpAsFasta() << " - "
                           << (**j).DumpAsFasta() << ")";
                */
                os << "Conflicting ids on a Bioseq: (";
                (**i).WriteAsFasta(os);
                os << " - ";
                (**j).WriteAsFasta(os);
                os << ")";
                ValidErr(eDiag_Error, eErr_SEQ_INST_ConflictingIdsOnBioseq,
                         CNcbiOstrstreamToString (os) /* os.str() */, seq);
            }
        }
    }


    // Loop thru CSeq_ids to check formatting
    unsigned int gi_count = 0;
    unsigned int accn_count = 0;
    iterate (CBioseq::TId, k, seq.GetId()) {
        switch ((**k).Which()) {
        case CSeq_id::e_Tpg:
        case CSeq_id::e_Tpe:
        case CSeq_id::e_Tpd:
        case CSeq_id::e_Genbank:
        case CSeq_id::e_Embl:
        case CSeq_id::e_Ddbj:
        try {
            const string& acc = s_GetAccession(**k);
            unsigned int numDigits = 0;
            unsigned int numLetters = 0;
            bool letterAfterDigit = false;
            bool badIDchars = false;
            iterate (string, s, acc) {
                if (isupper(*s)) {
                    numLetters++;
                    if (numDigits > 0) {
                        letterAfterDigit = true;
                    }
                } else if (isdigit(*s)) {
                    numDigits++;
                } else {
                    badIDchars = true;
                }
            }
            if (letterAfterDigit || badIDchars) {
                ValidErr(eDiag_Error, eErr_SEQ_INST_BadSeqIdFormat,
                         "Bad accession: " + acc, seq);
            } else if (numLetters == 1 && numDigits == 5 && s_isNa(seq)) {
            } else if (numLetters == 2 && numDigits == 6 && s_isNa(seq)) {
            } else if (numLetters == 3 && numDigits == 5 && s_isAa(seq)) {
            } else if (numLetters == 2 && numDigits == 6 && s_isAa(seq) &&
                seq.GetInst().GetRepr() == CSeq_inst:: eRepr_seg) {
            } else if (numLetters == 4  &&  numDigits == 8  &&
                ((**k).IsGenbank()  ||  (**k).IsEmbl()  ||
                (**k).IsDdbj())) {
            } else {
                ValidErr(eDiag_Error, eErr_SEQ_INST_BadSeqIdFormat,
                         "Bad accession: " + acc, seq);
            }

            // Check for secondary conflicts
            if (!acc.empty()  &&  seq.GetFirstId()) {
                CDesc_CI ds(m_Scope->GetBioseqHandle(*seq.GetFirstId()));
                CSeqdesc_CI sd(ds);
                for (; sd; ++sd) {
                    if (sd->IsGenbank()) {
                        const CGB_block& gb = sd->GetGenbank();
                        if (gb.IsSetExtra_accessions()) {
                            iterate(list<string>, a,
                                gb.GetExtra_accessions()) {
                                if (!NStr::CompareNocase(acc, *a)) {
                                    // If the same post error
                                    ValidErr(eDiag_Error,
                                        eErr_SEQ_INST_BadSecondaryAccn,
                                        acc + " used for both primary and"
                                        " secondary accession", seq);
                                }
                            }
                        }
                    }
                    if (sd->IsEmbl()) {
                        const CEMBL_block& eb = sd->GetEmbl();
                        if (eb.IsSetExtra_acc()) {
                            iterate(list<string>, a, eb.GetExtra_acc()) {
                                if (!NStr::CompareNocase(acc, *a)) {
                                    // If the same post error
                                    ValidErr(eDiag_Error,
                                        eErr_SEQ_INST_BadSecondaryAccn,
                                        acc + " used for both primary and"
                                        " secondary accession", seq);
                                }
                            }
                        }
                    }
                }
            }
        } catch (CValidException& e) {
            ValidErr(eDiag_Error, eErr_SEQ_INST_BadSeqIdFormat, e.what(),
                seq);
        }
        // Fall thru
        case CSeq_id::e_Other:
            try{
                const CTextseq_id& ti = s_GetTextseq_id(**k);
                if (ti.IsSetName()) {
                    const string& name = s_GetName(**k);
                    iterate (string, s, name) {
                        if (isspace(*s)) {
                            ValidErr(eDiag_Critical, eErr_SEQ_INST_SeqIdNameHasSpace,
                                     "Seq-id.name " + name + " should be a single "
                                     "word without any spaces", seq);
                            break;
                        }
                    }
                }
            } catch (CValidException& e) {
                ValidErr(eDiag_Critical, eErr_SEQ_INST_BadSeqIdFormat,
                    e.what(), seq);
            }
            // Fall thru
        case CSeq_id::e_Pir:
        case CSeq_id::e_Swissprot:
        case CSeq_id::e_Prf:
        try{
            const CTextseq_id& ti = s_GetTextseq_id(**k);
            if (s_isNa(seq)  &&  (!ti.IsSetAccession() ||
                ti.GetAccession().empty())) {
                if (seq.GetInst().GetRepr() != CSeq_inst::eRepr_seg  ||
                    m_IsGI) {
                    if (!(**k).IsDdbj()  ||
                        seq.GetInst().GetRepr() != CSeq_inst::eRepr_seg) {
                        CNcbiOstrstream os;
                        os << "Missing accession for " << (**k).DumpAsFasta();
                        ValidErr(eDiag_Error, eErr_SEQ_INST_BadSeqIdFormat,
                            string(os.str()), seq);
                    }
                }
            }
            accn_count++;
            break;
        } catch(CValidException& e) {
            ValidErr(eDiag_Error, eErr_SEQ_INST_BadSeqIdFormat, e.what(), seq);
        }            
        case CSeq_id::e_Patent:
            break;
        case CSeq_id::e_Pdb:
            break;
        case CSeq_id::e_Gi:
            if ((**k).GetGi() == 0) {
                ValidErr(eDiag_Error, eErr_SEQ_INST_ZeroGiNumber,
                         "Invalid GI number", seq);
            }
            gi_count++;
            break;
        case CSeq_id::e_General:
            break;
        default:
            break;
        }
    }

    // Check that a sequence with a gi number has exactly one accession
    if (gi_count > 0  &&  accn_count == 0) {
        ValidErr(eDiag_Error, eErr_SEQ_INST_GiWithoutAccession,
            "No accession on sequence with gi number", seq);
    }
    if (gi_count > 0  &&  accn_count > 1) {
        ValidErr(eDiag_Error, eErr_SEQ_INST_MultipleAccessions,
            "Multiple accessions on sequence with gi number", seq);
    }

    // C toolkit ensures that there is exactly one CBioseq for a CSeq_id
    // Not done here because object manager will not allow
    // the same Seq-id on multiple Bioseqs

}


void CValidError_impl::ValidateInst
(const CBioseq& seq)
{

    const CSeq_inst& inst = seq.GetInst();

    // Check representation
    if (!ValidateRepr(inst, seq)) {
        return;
    }

    // Check molecule, topology, and strand
    const CSeq_inst::EMol& mol = inst.GetMol();
    switch (mol) {
        case CSeq_inst::eMol_na:
            ValidErr(eDiag_Error, eErr_SEQ_INST_MolNuclAcid,
                     "Bioseq.mol is type na", seq);
            break;
        case CSeq_inst::eMol_aa:
            if (inst.IsSetTopology()  &&
              inst.GetTopology() != CSeq_inst::eTopology_linear)
            {
                ValidErr(eDiag_Error, eErr_SEQ_INST_CircularProtein,
                         "Non-linear topology set on protein", seq);
            }
            if (inst.IsSetStrand()  &&
              inst.GetStrand() != CSeq_inst::eStrand_ss)
            {
                ValidErr(eDiag_Error, eErr_SEQ_INST_DSProtein,
                         "Protein not single stranded", seq);
            }
            break;
        case CSeq_inst::eMol_not_set:
            ValidErr(eDiag_Error, eErr_SEQ_INST_MolNotSet, "Bioseq.mol not set",
                seq);
            break;
        case CSeq_inst::eMol_other:
            ValidErr(eDiag_Error, eErr_SEQ_INST_MolOther,
                     "Bioseq.mol is type other", seq);
            break;
        default:
            break;
    }

    CSeq_inst::ERepr rp = seq.GetInst().GetRepr();

    if (rp == CSeq_inst::eRepr_raw  ||  rp == CSeq_inst::eRepr_const) {    
        // Validate raw and constructed sequences
        ValidateRawConst(seq);
    }

    if (rp == CSeq_inst::eRepr_seg  ||  rp == CSeq_inst::eRepr_ref) {
        // Validate segmented and reference sequences
        ValidateSegRef(seq);
    }

    if (rp == CSeq_inst::eRepr_delta) {
        // Validate delta sequences
        ValidateDelta(seq);
    }

    if (rp == CSeq_inst::eRepr_seg  &&  seq.GetInst().IsSetExt()  &&
        seq.GetInst().GetExt().IsSeg()) {
        // Validate part of segmented sequence
        ValidateSeqParts(seq);
    }

    if (s_isAa(seq)) {
        // Validate protein title (amino acids only)
        ValidateProteinTitle(seq);
    }

    // Validate sequence length
    ValidateSeqLen(seq);
}


void CValidError_impl::ValidateBioseq(const CBioseq& seq)
{
    // Validate CSeq_ids
    ValidateSeqIds(seq);

    // Validate the inst data
    ValidateInst(seq);

    // Validate the context
    ValidateBioseqContext(seq);
}


void CValidError_impl::ValidateSeqDesc(const CSeqdesc& desc)
{
    // switch on type, e.g., call ValidateBioSource or ValidatePubdesc
    switch (desc.Which ()) {
        case CSeqdesc::e_not_set:
            break;
        case CSeqdesc::e_Mol_type:
            break;
        case CSeqdesc::e_Modif:
            break;
        case CSeqdesc::e_Method:
            break;
        case CSeqdesc::e_Name:
            break;
        case CSeqdesc::e_Title:
            break;
        case CSeqdesc::e_Org:
            break;
        case CSeqdesc::e_Comment:
            break;
        case CSeqdesc::e_Num:
            break;
        case CSeqdesc::e_Maploc:
            break;
        case CSeqdesc::e_Pir:
            break;
        case CSeqdesc::e_Genbank:
            break;
        case CSeqdesc::e_Pub:
        {
            const CPubdesc& pub = desc.GetPub ();
            ValidatePubdesc (pub, desc);
            break;
        }
        case CSeqdesc::e_Region:
            break;
        case CSeqdesc::e_User:
        {
            const CUser_object& usr = desc.GetUser ();
            const CObject_id& oi = usr.GetType();
            if (!oi.IsStr()) {
                break;
            }
            if (!NStr::CompareNocase(oi.GetStr(), "TpaAssembly")) {
                try {
                    ValidErr(eDiag_Error, eErr_SEQ_DESCR_InvalidForType,
                        "Non-TPA record should not have TpaAssembly object", desc);
                } catch (const runtime_error&) {
                    return;
                }
            }
            break;
        }
        case CSeqdesc::e_Sp:
            break;
        case CSeqdesc::e_Dbxref:
            break;
        case CSeqdesc::e_Embl:
            break;
        case CSeqdesc::e_Create_date:
            break;
        case CSeqdesc::e_Update_date:
            break;
        case CSeqdesc::e_Prf:
            break;
        case CSeqdesc::e_Pdb:
            break;
        case CSeqdesc::e_Het:
            break;
        case CSeqdesc::e_Source:
        {
            const CBioSource& src = desc.GetSource ();
            ValidateBioSource (src, desc);
            break;
        }
        case CSeqdesc::e_Molinfo:
            break;
        default:
            break;
    }
}


static bool s_MrnaProductInGPS(const CBioseq& seq, CScope* scope)
{
    bool found = true;
    CBioseq_Handle bsh = scope->GetBioseqHandle(seq);
    if ( bsh ) {
        const CSeq_entry& se = bsh.GetTopLevelSeqEntry();
        if ( se.IsSet()  && 
             se.GetSet().GetClass() == CBioseq_set::eClass_gen_prod_set ) {
            found = false;
            for ( CFeat_CI fi(bsh, 0, 0, CSeqFeatData::e_Rna);
                  fi;
                  ++fi ) {
                if ( fi->GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA ) {
                    found = true;
                }
            }
        }
    }
    return found;
}



void CValidError_impl::ValidateNucProt 
(const CBioseq_set& seqset,
 int nuccnt, 
 int protcnt)
{
    if ( nuccnt == 0 ) {
        ValidErr(eDiag_Error, eErr_SEQ_PKG_NucProtProblem,
                 "No nucleotides in nuc-prot set", seqset);
    }
    if ( protcnt == 0 ) {
        ValidErr(eDiag_Error, eErr_SEQ_PKG_NucProtProblem,
                 "No proteins in nuc-prot set", seqset);
    }

    iterate( list< CRef<CSeq_entry> >, se_list_it, seqset.GetSeq_set() ) {
        if ( (**se_list_it).IsSeq() ) {
            const CBioseq& seq = (**se_list_it).GetSeq();
            if ( s_isNa(seq)  && !s_MrnaProductInGPS(seq, m_Scope) ) {
                ValidErr(eDiag_Warning,
                    eErr_SEQ_PKG_GenomicProductPackagingProblem,
                    "Nucleotide bioseq should be product of mRNA "
                    "feature on contig, but is not",
                    seq);
            }
        }

        if ( !(**se_list_it).IsSet() )
            continue;

        const CBioseq_set& set = (**se_list_it).GetSet();
        if ( set.GetClass() != CBioseq_set::eClass_segset ) {

            const CEnumeratedTypeValues* tv = 
                CBioseq_set::GetTypeInfo_enum_EClass();
            const string& set_class = tv->FindName(set.GetClass(), true);

            ValidErr(eDiag_Error, eErr_SEQ_PKG_NucProtNotSegSet,
                     "Nuc-prot Bioseq-set contains wrong Bioseq-set, "
                     "its class is \"" + set_class + "\"", set);
            break;
        }
    }
}


void CValidError_impl::ValidateSegSet(const CBioseq_set& seqset, int segcnt)
{
    if ( segcnt == 0 ) {
        ValidErr(eDiag_Error, eErr_SEQ_PKG_SegSetProblem,
            "No segmented Bioseq in segset", seqset);
    }

    CSeq_inst::EMol     mol = CSeq_inst::eMol_not_set;
    CSeq_inst::EMol     seq_inst_mol;
    
    iterate ( list< CRef<CSeq_entry> >, se_list_it, seqset.GetSeq_set() ) {
        if ( (**se_list_it).IsSeq() ) {
            const CSeq_inst& seq_inst = (**se_list_it).GetSeq().GetInst();
            
            if ( mol == CSeq_inst::eMol_not_set ||
                mol == CSeq_inst::eMol_other ) {
                mol = seq_inst.GetMol();
            } else if ( (seq_inst_mol = seq_inst.GetMol()) != CSeq_inst::eMol_other) {
                if ( s_isNa(seq_inst) != s_isNa(mol) ) {
                    ValidErr(eDiag_Critical, eErr_SEQ_PKG_SegSetMixedBioseqs,
                        "Segmented set contains mixture of nucleotides"
                        "and proteins", seqset);
                    break;
                }
            }
        } else if ( (**se_list_it).IsSet() ) {
            const CBioseq_set& set = (**se_list_it).GetSet();
            
            const CEnumeratedTypeValues* tv = 
                CBioseq_set::GetTypeInfo_enum_EClass();
            const string& set_class_str = 
                tv->FindName(set.GetClass(), true);
            
            ValidErr(eDiag_Critical, eErr_SEQ_PKG_SegSetNotParts,
                "Segmented set contains wrong Bioseq-set, "
                "its class is \"" + set_class_str + "\"", set);
            
            break;
        } // else if
    } // iterate
    
    CTypeConstIterator<CMolInfo> miit(ConstBegin(seqset));
    const CMolInfo* mol_info = 0;
    
    for (; miit; ++miit) {
        if (mol_info == 0) {
            mol_info = &(*miit);
        } else if (mol_info->GetBiomol() != miit->GetBiomol() ) {
            ValidErr(eDiag_Error, eErr_SEQ_PKG_InconsistentMolInfoBiomols,
                "Segmented set contains inconsistent MolInfo biomols",
                seqset);
            break;
        }
    } // for
}


void CValidError_impl::ValidatePartsSet(const CBioseq_set& seqset)
{
    CSeq_inst::EMol     mol = CSeq_inst::eMol_not_set;
    CSeq_inst::EMol     seq_inst_mol;

    iterate ( list< CRef<CSeq_entry> >, se_list_it, seqset.GetSeq_set() ) {
        if ( (**se_list_it).IsSeq() ) {
            const CSeq_inst& seq_inst = (**se_list_it).GetSeq().GetInst();

            if ( mol == CSeq_inst::eMol_not_set  ||
                 mol == CSeq_inst::eMol_other ) {
                mol = seq_inst.GetMol();
            } else  {
                seq_inst_mol = seq_inst.GetMol();
                if ( seq_inst_mol != CSeq_inst::eMol_other) {
                    if ( s_isNa(seq_inst) != s_isNa(mol) ) {
                        ValidErr(eDiag_Critical, eErr_SEQ_PKG_PartsSetMixedBioseqs,
                                 "Parts set contains mixture of nucleotides "
                                 "and proteins", seqset);
                        break;
                    }
                }
            }
        } else if ( (**se_list_it).IsSet() ) {
            const CBioseq_set& set = (**se_list_it).GetSet();
            const CEnumeratedTypeValues* tv = 
                CBioseq_set::GetTypeInfo_enum_EClass();
            const string& set_class_str = 
                tv->FindName(set.GetClass(), true);

            ValidErr(eDiag_Error, eErr_SEQ_PKG_PartsSetHasSets,
                    "Parts set contains unwanted Bioseq-set, "
                    "its class is \"" + set_class_str + "\".", set);
            break;
        } // else if
    } // for
}


void CValidError_impl::ValidatePopSet(const CBioseq_set& seqset)
{
    const CBioSource*   biosrc  = 0;
    const string        *first_taxname = 0;
    static const string influenza = "Influenza virus ";

    CTypeConstIterator<CBioseq> seqit(ConstBegin(seqset));
    for (; seqit; ++seqit) {

        biosrc = 0;

        // Will get the first biosource either from the descriptor
        // or feature.
        CTypeConstIterator<CBioSource> biosrc_it(ConstBegin(*seqit));
        if (biosrc_it) {
            biosrc = &(*biosrc_it);
        } 

        if (biosrc == 0)
            continue;

        const string& taxname = biosrc->GetOrg().GetTaxname();
        if (first_taxname == 0) {
            first_taxname = &taxname;
            continue;
        }

	// Make sure all the taxnames in the set are the same.
        if ( NStr::CompareNocase(*first_taxname, taxname) == 0 ) {
            continue;
        }

	// if the names differ issue an error with the exception of Influenza
	// virus, where we allow different types of it in the set.
        if ( NStr::StartsWith(taxname, influenza, NStr::eNocase)         &&
             NStr::StartsWith(*first_taxname, influenza, NStr::eNocase)  &&
             NStr::CompareNocase(*first_taxname, 0, influenza.length() + 1, taxname) == 0 ) {
            continue;
        }

        ValidErr(eDiag_Error, eErr_SEQ_DESCR_InconsistentBioSources,
                  "Population set contains inconsistent organisms.",
                  *seqit);
        break;
    }
}


static const CBioseq* s_GetBioSeq (const CSeq_loc& loc, CScope* scope)
{
    try {

        const CSeq_id& id = GetId(loc, scope);
        const CBioseq_Handle se_handle = scope->GetBioseqHandle(id);
            return &(se_handle.GetBioseq());

    } catch (...) {
        return 0;
    }
}


void CValidError_impl::ValidateGenProdSet(const CBioseq_set& seqset)
{
    bool                id_no_good = false;
    CSeq_id::E_Choice   id_type;
    
    list< CRef<CSeq_entry> >::const_iterator se_list_it =
        seqset.GetSeq_set().begin();
    
    if ( !(**se_list_it).IsSeq() ) {
        return;
    }
    
    const CBioseq& seq = (**se_list_it).GetSeq();
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);

    CFeat_CI feat_it(bsh, 0, 0, CSeqFeatData::e_Rna);
    for (; feat_it; ++feat_it) {
        if ((*feat_it).GetData().GetRna().GetType() ==
            CRNA_ref::eType_mRNA) {
            const CBioseq* cdna =
                s_GetBioSeq ((*feat_it).GetProduct(), m_Scope);
            if (cdna == 0) {
                try {
                    const CSeq_id& id = GetId((*feat_it).GetProduct(),
                        m_Scope);
                    id_type = id.Which();
                }
                catch (...) {
                    id_no_good = true;
                }
                
                // okay to have far RefSeq product
                if (id_no_good || id_type != CSeq_id::e_Other) {
                    string loc_label;
                    (*feat_it).GetProduct().GetLabel(&loc_label);
                    
                    if (loc_label.empty()) {
                        loc_label = "?";
                    }
                    
                    ValidErr(eDiag_Error,
                        eErr_SEQ_PKG_GenomicProductPackagingProblem,
                        "Product of mRNA feature (" + loc_label +
                        ") not packaged in genomic product set", seq);
                    
                }
            } // if (cdna == 0)
        } // if ((*feat_it)
    } // for 
}


void CValidError_impl::ValidateSeqSet(const CBioseq_set& seqset)
{
    int protcnt = 0;
    int nuccnt  = 0;
    int segcnt  = 0;

    // Validate Set Contents
    CTypeConstIterator<CBioseq> seqit(ConstBegin(seqset));
    for (; seqit; ++seqit) {

        if (s_isAa(*seqit))
            protcnt++;
        else
            nuccnt++;

        if (seqit->GetInst().GetRepr() == CSeq_inst::eRepr_seg)
            segcnt++;
    }

    switch ( seqset.GetClass() ) {
    case CBioseq_set::eClass_nuc_prot:
        ValidateNucProt(seqset, nuccnt, protcnt);
        break;

    case CBioseq_set::eClass_segset:
        ValidateSegSet(seqset, segcnt);
        break;

    case CBioseq_set::eClass_parts:
        ValidatePartsSet(seqset);
        break;

    case CBioseq_set::eClass_pop_set:
        ValidatePopSet(seqset);
        break;

    case CBioseq_set::eClass_gen_prod_set:
        ValidateGenProdSet(seqset);
        break;
    case CBioseq_set::eClass_other:
        ValidErr(eDiag_Critical, eErr_SEQ_PKG_GenomicProductPackagingProblem, 
                     "Genomic product set class incorrectly set to other", seqset);
        break;
    default:
        if ( nuccnt == 0  &&  protcnt == 0 )  {
            ValidErr(eDiag_Error, eErr_SEQ_PKG_EmptySet, 
                     "No Bioseqs in this set", seqset);
        }
        break;
    }

}


void CValidError_impl::ValidateSeqAnnot(const CSeq_annot& annot)
{   
    if ( !annot.GetData().IsAlign() ) return;
    if ( !annot.IsSetDesc() ) return;
    
    iterate( list< CRef< CAnnotdesc > >, iter, annot.GetDesc().Get() ) {
        
        if ( (*iter)->IsUser() ) {
            const CObject_id& oid = (*iter)->GetUser().GetType();
            if ( oid.IsStr() ) {
                if ( oid.GetStr() == "Blast Type" ) {
                    ValidErr(eDiag_Error, eErr_SEQ_ALIGN_BlastAligns,
                        "Record contains BLAST alignments", annot); // !!!
                    
                    break;
                }
            }
        }
    } // iterate
}


void CValidError_impl::ValidateSeqAlign(const CSeq_align& ) //align)
{
    // !!!
}


void CValidError_impl::ValidateSeqGraph(const CSeq_graph& ) //graph)
{
    // !!!
}


static bool s_LocOnSeg(const CBioseq& seq, const CSeq_loc& loc) 
{
    for ( CSeq_loc_CI sli( loc ); sli;  ++sli ) {
        const CSeq_id& loc_id = sli.GetSeq_id();
        iterate(  CBioseq::TId, seq_id, seq.GetId() ) {
            if ( loc_id.Match(**seq_id) ) {
                return true;
            }
        }
    }
    return false;
}


static size_t s_NumOfIntervals(const CSeq_loc& loc) 
{
    size_t counter = 0;
    for ( CSeq_loc_CI slit(loc); slit; ++slit ) {
        ++counter;
    }
    return counter;
}


bool CValidError_impl::IsFarLocation(const CSeq_loc& loc) const 
{
    // !!! need implementation as binary search
    for ( CSeq_loc_CI citer(loc); citer; ++citer ) {
        bool found = false;
        const CSeq_id& id = citer.GetSeq_id();
        iterate( set< CConstRef<CSeq_id> >, i,  m_InitialSeqIds ) {
            if ( (*i)->Match(id) ) {
                found = true;
                break;
            }
        }
        if ( !found ) {
            return true;
        }
    }
    return false;
}


void CValidError_impl::ValidateSeqFeatContext(const CBioseq& seq)
{
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);
    EDiagSev sev = eDiag_Warning;
    bool full_length_prot_ref = false;

    for ( CFeat_CI fi(bsh, 0, 0, CSeqFeatData::e_not_set); fi; ++fi ) {
        
        CSeqFeatData::E_Choice ftype = fi->GetData().Which();
        
        if ( s_isAa(seq) ) {                // protein
            switch ( ftype ) {
            case CSeqFeatData::e_Prot:
                {
                    TSeqPos len = bsh.GetBioseq().GetInst().GetLength();
                    CSeq_loc::TRange range = fi->GetLocation().GetTotalRange();
                    
                    if ( range.GetFrom() == 0 && range.GetTo() == len - 1 ) {
                        full_length_prot_ref = true;
                    }
                }
                break;
                
            case CSeqFeatData::e_Cdregion:
            case CSeqFeatData::e_Rna:
            case CSeqFeatData::e_Rsite:
            case CSeqFeatData::e_Txinit:
                ValidErr(eDiag_Error, eErr_SEQ_FEAT_InvalidForType,
                    "Invalid feature for a protein Bioseq.", *fi);
                break;
            }
        } else {                            // nucleotide
            switch ( ftype ) {
            case CSeqFeatData::e_Prot:
            case CSeqFeatData::e_Psec_str:
                ValidErr(eDiag_Error, eErr_SEQ_FEAT_InvalidForType,
                    "Invalid feature for a nucleotide Bioseq.", *fi);
                break;
            }
        }
        
        if ( s_IsMrna(bsh) ) {              // mRNA
            switch ( ftype ) {
            case CSeqFeatData::e_Cdregion:
                if ( s_NumOfIntervals(fi->GetLocation()) > 1 ) {
                    bool excpet = fi->IsSetExcept()  &&  !fi->GetExcept();
                    string except_text;
                    if ( fi->IsSetExcept_text() ) {
                        except_text = fi->GetExcept_text();
                    }
                    if ( excpet  ||
                         (SearchNoCase(except_text, "ribosomal slippage") == string::npos  &&
                          SearchNoCase(except_text, "ribosome slippage") == string::npos) ) {
                        ValidErr(sev, eErr_SEQ_FEAT_InvalidForType,
                            "Multi-interval CDS feature is invalid on an mRNA "
                            "(cDNA) Bioseq.", *fi);
                    }
                }
                break;
                
            case CSeqFeatData::e_Rna:
                {
                    const CRNA_ref& rref = fi->GetData().GetRna();
                    if ( rref.GetType() == CRNA_ref::eType_mRNA ) {
                        ValidErr(eDiag_Error, eErr_SEQ_FEAT_InvalidForType,
                            "mRNA feature is invalid on an mRNA (cDNA) Bioseq.",
                            *fi);
                    }
                }
                break;
                
            case CSeqFeatData::e_Imp:
                {
                    const CImp_feat& imp = fi->GetData().GetImp();
                    if ( imp.GetKey() == "intron"  ||
                        imp.GetKey() == "CAAT_signal" ) {
                        ValidErr(eDiag_Error, eErr_SEQ_FEAT_InvalidForType,
                            "Invalid feature for an mRNA Bioseq.", *fi);
                    }
                }
                break;
            }
        } else if ( s_IsPrerna(bsh) ) { // preRNA
            switch ( ftype ) {
            case CSeqFeatData::e_Imp:
                {
                    const CImp_feat& imp = fi->GetData().GetImp();
                    if ( imp.GetKey() == "CAAT_signal" ) {
                        ValidErr(eDiag_Error, eErr_SEQ_FEAT_InvalidForType,
                            "Invalid feature for a pre-RNA Bioseq.", *fi);
                    }
                }
                break;
            }
        }

        if ( IsFarLocation(fi->GetLocation())  && !m_IsNC ) {
            ValidErr(eDiag_Warning, eErr_SEQ_FEAT_FarLocation,
                "Feature has 'far' location - accession not packaged in record",
                *fi);
        }

        if ( seq.GetInst().GetRepr() == CSeq_inst::eRepr_seg ) {
            if ( s_LocOnSeg(seq, fi->GetLocation()) ) {
                if ( !IsDeltaOrFarSeg(fi->GetLocation()) ) {
                    sev = m_IsNC ? eDiag_Warning : eDiag_Error;
                    ValidErr(sev, eErr_SEQ_FEAT_LocOnSegmentedBioseq,
                        "Feature location on segmented bioseq, not on parts", *fi);
                }
            }
        }

    }  // end of for loop

    if ( s_isAa(seq)  && !full_length_prot_ref  &&  !m_IsPDB) {
        m_ProtWithNoFullRef.push_back(CConstRef<CBioseq>(&seq));
    }
}


static bool s_SerialNumberInComment(const string& comment)
{
    size_t pos = comment.find('[', 0);
    while ( pos != string::npos ) {
        ++pos;
        if ( isdigit(comment[pos]) ) {
            while ( isdigit(comment[pos]) ) {
                ++pos;
            }
            if ( comment[pos] == ']' ) {
                return true;
            }
        }

        pos = comment.find('[', pos);
    }
    return false;
}


void CValidError_impl::ValidateSeqFeat(const CSeq_feat& feat)
{
    CBioseq_Handle bsh;
    bsh = m_Scope->GetBioseqHandle(feat.GetLocation());
    ValidateSeqLoc(feat.GetLocation(), bsh.GetBioseq(), "Location");
    
    if ( feat.IsSetProduct() ) {
        bsh = m_Scope->GetBioseqHandle(feat.GetProduct());
        ValidateSeqLoc(feat.GetProduct(), bsh.GetBioseq(), "Product");
    }

    ValidateFeatPartialness(feat);
    
    switch ( feat.GetData ().Which () ) {
        case CSeqFeatData::e_Gene:
            // Validate CGene_ref
            ValidateGene(feat.GetData ().GetGene (), feat);
            break;
        case CSeqFeatData::e_Cdregion:
            // Validate CCdregion
            ValidateCdregion(feat.GetData ().GetCdregion (), feat);
            break;
        case CSeqFeatData::e_Prot:
            // Validate CProt_ref
            ValidateProt(feat.GetData ().GetProt (), feat);
            break;
        case CSeqFeatData::e_Rna:
            // Validate CRNA_ref
            ValidateRna(feat.GetData ().GetRna (), feat);
            break;
        case CSeqFeatData::e_Pub:
            // Validate CPubdesc
            ValidatePubdesc(feat.GetData ().GetPub (), feat);
            break;
        case CSeqFeatData::e_Imp:
            // Validate CPubdesc
            ValidateImp(feat.GetData ().GetImp (), feat);
            break;
        case CSeqFeatData::e_Biosrc:
            // Validate CBioSource
            ValidateBioSource(feat.GetData ().GetBiosrc (), feat);
            break;
        default:
            ValidErr(eDiag_Error, eErr_SEQ_FEAT_InvalidType,
                "Invalid SeqFeat type [" + 
                NStr::IntToString(feat.GetData ().Which ()) +
                "]", feat);
            break;
    }
    if (feat.IsSetDbxref ()) {
        ValidateDbxref (feat.GetDbxref (), feat);
    }
    ValidateExcept(feat);

    if ( feat.GetData ().Which () != CSeqFeatData::e_Gene ) {
        // !!!
    }

    if ( feat.IsSetComment() ) {
        if ( s_SerialNumberInComment(feat.GetComment()) ) {
            ValidErr(eDiag_Info, eErr_SEQ_FEAT_SerialInComment,
              "Feature comment may refer to reference by serial number - "
              "attach reference specific comments to the reference "
              "REMARK instead.", feat);
        }
    }
}


void CValidError_impl::ValidateExcept(const CSeq_feat& feat)
{
    if ( !feat.GetExcept () && !feat.GetExcept_text ().empty() ) {
        ValidErr (eDiag_Warning, eErr_SEQ_FEAT_ExceptInconsistent,
            "Exception text is set, but exception flag is not set", feat);
    }
    if ( !feat.GetExcept_text ().empty() ) {
        ValidateExceptText(feat.GetExcept_text(), feat);
    }
}


void CValidError_impl::ValidateExceptText(const string& text, const CSeq_feat& feat)
{
    if ( text.empty() ) return;

    EDiagSev sev = eDiag_Error;
    bool found = false;

    string str;
    string::size_type   begin = 0, end, textlen = text.length();

    const string * legal_begin = sm_LegalExceptionStrings;
    const string *legal_end = 
        &(sm_LegalExceptionStrings[sizeof (sm_LegalExceptionStrings) / sizeof (string)]);

    
    while ( begin < textlen ) {
        found = false;
        end = min( text.find_first_of(',', begin), textlen );
        str = NStr::TruncateSpaces( text.substr(begin, end) );
        if ( find(legal_begin, legal_end, str) != legal_end ) {
            found = true;
        }
        if ( !found && (m_IsGPS || m_IsRefSeq) ) {
            legal_begin = sm_RefseqExceptionStrings;
            const string *legal_end = 
                &(sm_RefseqExceptionStrings[sizeof (sm_RefseqExceptionStrings) / sizeof (string)]);
            if ( find(legal_begin, legal_end, str) != legal_end ) {
               found = true;
            }
        }
        if ( !found ) {
            if ( m_IsNC || m_IsNT ) {
                sev = eDiag_Warning;
            }
            ValidErr(sev, eErr_SEQ_FEAT_InvalidQualifierValue,
                str + " is not legal exception explanation", feat);
        }
    }
}



void CValidError_impl::ValidateSeqLoc
(const CSeq_loc& loc,
 const CBioseq&  seq,
 const char*     prefix)
{
    bool circular = false;
    if (seq.GetInst().IsSetTopology()) {
        circular =
            seq.GetInst().GetTopology() == CSeq_inst::eTopology_circular;
    }
    
    CTypeConstIterator<CSeq_loc> lit = ConstBegin(loc);
    bool ordered = true, adjacent = false, chk = true,
        unmarked_strand = false, mixed_strand = false;
    const CSeq_id* id_cur = 0, *id_prv = 0;
    const CSeq_interval *int_cur = 0, *int_prv = 0;
    ENa_strand strand_cur = eNa_strand_unknown,
        strand_prv = eNa_strand_unknown;
    for (; lit; ++lit) {
        try {
            switch (lit->Which()) {
            case CSeq_loc::e_Int:
                int_cur = &lit->GetInt();
                strand_cur = int_cur->IsSetStrand() ?
                    int_cur->GetStrand() : eNa_strand_unknown;
                id_cur = &int_cur->GetId();
                chk = IsValid(*int_cur, m_Scope);
                if (chk  &&  int_prv  && ordered  &&
                    !circular  && id_prv) {
                    if (IsSameBioseq(*id_prv, *id_cur, m_Scope)) {
                        if (strand_cur == eNa_strand_minus) {
                            if (int_prv->GetTo() < int_cur->GetTo()) {
                                ordered = false;
                            }
                            if (int_cur->GetTo() + 1 == int_prv->GetFrom()) {
                                adjacent = true;
                            }
                        } else {
                            if (int_prv->GetTo() > int_cur->GetTo()) {
                                ordered = false;
                            }
                            if (int_prv->GetTo() + 1 == int_cur->GetFrom()) {
                                adjacent = true;
                            }
                        }
                    }
                }
                if (int_prv) {
                    if (IsSameBioseq(int_prv->GetId(), int_cur->GetId(), m_Scope)){
                        if (strand_prv == strand_cur  &&
                            int_prv->GetFrom() == int_cur->GetFrom()  &&
                            int_prv->GetTo() == int_cur->GetTo()) {
                            ValidErr(eDiag_Error,
                                eErr_SEQ_FEAT_DuplicateInterval,
                                "Duplicate exons in location", seq);
                        }
                    }
                }
                int_prv = int_cur;
                break;
            case CSeq_loc::e_Pnt:
                strand_cur = lit->GetPnt().IsSetStrand() ?
                    lit->GetPnt().GetStrand() : eNa_strand_unknown;
                id_cur = &lit->GetPnt().GetId();
                chk = IsValid(lit->GetPnt(), m_Scope);
                int_prv = 0;
                break;
            case CSeq_loc::e_Packed_pnt:
                strand_cur = lit->GetPacked_pnt().IsSetStrand() ?
                    lit->GetPacked_pnt().GetStrand() : eNa_strand_unknown;
                id_cur = &lit->GetPacked_pnt().GetId();
                chk = IsValid(lit->GetPacked_pnt(), m_Scope);
                int_prv = 0;
                break;
            case CSeq_loc::e_Null:
                break;
            default:
                strand_cur = eNa_strand_other;
                id_cur = 0;
                int_prv = 0;
                break;
            }
            if (!chk) {
                string lbl;
                lit->GetLabel(&lbl);
                ValidErr(eDiag_Critical, eErr_SEQ_FEAT_Range,
                    string(prefix) + ": Seq-loc " + lbl + " out of range", seq);
            }
            
            if (lit->Which() != CSeq_loc::e_Null) {
                if (strand_prv != eNa_strand_other  &&
                    strand_cur != eNa_strand_other) {
                    if (id_cur  &&  id_prv  &&
                        IsSameBioseq(*id_cur, *id_prv, m_Scope)) {
                        if (strand_prv != strand_cur) {
                            if ((strand_prv == eNa_strand_plus  &&
                                strand_cur == eNa_strand_unknown)  ||
                                (strand_prv == eNa_strand_unknown  &&
                                strand_cur == eNa_strand_plus)) {
                                unmarked_strand = true;
                            } else {
                                mixed_strand = true;
                            }
                        }
                    }
                }
                strand_prv = strand_cur;
                id_prv = id_cur;
            }
        } catch( runtime_error rt ) {
            string label;
            lit->GetLabel(&label);
            ValidErr(eDiag_Error, eErr_SEQ_FEAT_Range,  // !!! need to chenge error type
                "Exception caught while validating location " +
                label + ". exception: " + rt.what(), seq);
                
            strand_cur = eNa_strand_other;
            id_cur = 0;
            int_prv = 0;
        }
    }

    string loc_lbl;
    if (adjacent) {
        loc.GetLabel(&loc_lbl);
        ValidErr(eDiag_Error, eErr_SEQ_FEAT_AbuttingIntervals,
            string(prefix) + ": Adjacent intervals in SeqLoc [" +
            loc_lbl + "]", seq);
    }
    if (mixed_strand  ||  unmarked_strand  ||  !ordered) {
        if (loc_lbl.empty()) {
            loc.GetLabel(&loc_lbl);
        }
        if (mixed_strand) {
            ValidErr(eDiag_Error, eErr_SEQ_FEAT_MixedStrand,
                string(prefix) + ": Mixed strands in SeqLoc [" +
                loc_lbl + "]", seq);
        } else if (unmarked_strand) {
            ValidErr(eDiag_Warning, eErr_SEQ_FEAT_MixedStrand,
                string(prefix) + ": Mixed plus and unknown strands in SeqLoc "
                " [" + loc_lbl + "]", seq);
        }
        if (!ordered) {
            ValidErr(eDiag_Error, eErr_SEQ_FEAT_SeqLocOrder,
                string(prefix) + ": Intervals out of order in SeqLoc [" +
                loc_lbl + "]", seq);
        }
        return;
    }

    if (seq.GetInst().GetRepr() != CSeq_inst::eRepr_seg) {
        return;
    }

    // Check for intervals out of order on segmented Bioseq
    if (BadSeqLocSortOrder(seq, loc, m_Scope)) {
        if (loc_lbl.empty()) {
            loc.GetLabel(&loc_lbl);
        }
        ValidErr(eDiag_Error, eErr_SEQ_FEAT_SeqLocOrder,
            string(prefix) + "Intervals out of order in SeqLoc [" +
            loc_lbl + "]", seq);
    }

    // Check for mixed strand on segmented Bioseq
    if (s_SeqLocMixedStrands(seq, loc)) {
        if (loc_lbl.empty()) {
            loc.GetLabel(&loc_lbl);
        }
        ValidErr(eDiag_Error, eErr_SEQ_FEAT_MixedStrand,
            string(prefix) + ": Mixed strands in SeqLoc [" +
            loc_lbl + "]", seq);
    }

}


bool CValidError_impl::ValidateRepr
(const CSeq_inst& inst,
 const CBioseq&   seq)
{
    bool rtn = true;
    const CEnumeratedTypeValues* tv = CSeq_inst::GetTypeInfo_enum_ERepr();
    const string& rpr = tv->FindName(inst.GetRepr(), true);
    const string err0 = "Bioseq-ext not allowed on " + rpr + " Bioseq";
    const string err1 = "Missing or incorrect Bioseq-ext on " + rpr + " Bioseq";
    const string err2 = "Missing Seq-data on " + rpr + " Bioseq";
    const string err3 = "Seq-data not allowed on " + rpr + " Bioseq";
    switch (inst.GetRepr()) {
    case CSeq_inst::eRepr_virtual:
        if (inst.IsSetExt()) {
            ValidErr(eDiag_Error, eErr_SEQ_INST_ExtNotAllowed, err0, seq);
            rtn = false;
        }
        if (inst.IsSetSeq_data()) {
            ValidErr(eDiag_Error, eErr_SEQ_INST_SeqDataNotAllowed, err3, seq);
            rtn = false;
        }
        break;
    case CSeq_inst::eRepr_map:
        if (!inst.IsSetExt() || !inst.GetExt().IsMap()) {
            ValidErr(eDiag_Error, eErr_SEQ_INST_ExtBadOrMissing, err1, seq);
            rtn = false;
        }
        if (inst.IsSetSeq_data()) {
            ValidErr(eDiag_Error, eErr_SEQ_INST_SeqDataNotAllowed, err3, seq);
            rtn = false;
        }
        break;
    case CSeq_inst::eRepr_ref:
        if (!inst.IsSetExt() || !inst.GetExt().IsRef() ) {
            ValidErr(eDiag_Error, eErr_SEQ_INST_ExtBadOrMissing, err1, seq);
            rtn = false;
        }
        if (inst.IsSetSeq_data()) {
            ValidErr(eDiag_Error, eErr_SEQ_INST_SeqDataNotAllowed, err3, seq);
            rtn = false;
        }
        break;
    case CSeq_inst::eRepr_seg:
        if (!inst.IsSetExt() || !inst.GetExt().IsSeg() ) {
            ValidErr(eDiag_Error, eErr_SEQ_INST_ExtBadOrMissing, err1, seq);
            rtn = false;
        }
        if (inst.IsSetSeq_data()) {
            ValidErr(eDiag_Error, eErr_SEQ_INST_SeqDataNotAllowed, err3, seq);
            rtn = false;
        }
        break;
    case CSeq_inst::eRepr_raw:
    case CSeq_inst::eRepr_const:
        if (inst.IsSetExt()) {
            ValidErr(eDiag_Error, eErr_SEQ_INST_ExtNotAllowed, err0, seq);
            rtn = false;
        }
        if (!inst.IsSetSeq_data() ||
          inst.GetSeq_data().Which() == CSeq_data::e_not_set)
        {
            ValidErr(eDiag_Error, eErr_SEQ_INST_SeqDataNotFound, err2, seq);
            rtn = false;
        }
        break;
    case CSeq_inst::eRepr_delta:
        if (!inst.IsSetExt() || !inst.GetExt().IsDelta() ) {
            ValidErr(eDiag_Error, eErr_SEQ_INST_ExtBadOrMissing, err1, seq);
            rtn = false;
        }
        if (inst.IsSetSeq_data()) {
            ValidErr(eDiag_Error, eErr_SEQ_INST_SeqDataNotAllowed, err3, seq);
            rtn = false;
        }
        break;
    default:
        ValidErr(
            eDiag_Critical, eErr_SEQ_INST_ReprInvalid,
            "Invalid Bioseq->repr = " +
            NStr::IntToString(static_cast<int>(inst.GetRepr())), seq);
        rtn = false;
    }
    return rtn;
}


void CValidError_impl::Validate(const CSeq_entry& se, const CCit_sub* cs)
{
    // Check that CSeq_entry has data
    if (se.Which() == CSeq_entry::e_not_set) {
        ERR_POST(Warning << "Seq_entry not set");
        return;
    }

    // Set scope to CSeq_entry
    SetScope(se);

    // Get first CBioseq object pointer for ValidErr below.
    CTypeConstIterator<CBioseq> seq(ConstBegin(se));
    if (!seq) {
        ERR_POST("No Bioseq anywhere on this Seq-entry");
        return;
    }

    // If m_NonASCII is true, then this flag was set by the caller
    // of validate to indicate that a non ascii character had been
    // read from a file being used to create a CSeq_entry, that the
    // error had been corrected, but that the error needs to be reported
    // by Validate. Note, Validate is not doing anything other than
    // reporting an error if m_NonASCII is true;
    if (m_NonASCII) {
        ValidErr (eDiag_Critical, eErr_GENERIC_NonAsciiAsn,
                  "Non-ascii chars in input ASN.1 strings", *seq);
        // Only report the error once
        m_NonASCII = false;
    }

    // If no Pubs/BioSource in CSeq_entry, post only one error
    m_NoPubs = !s_AnyObj<CPub, CSeq_entry>(se);
    m_NoBioSource = !s_AnyObj<CBioSource, CSeq_entry>(se);

    // Look for genomic product set
    for (CTypeConstIterator <CBioseq_set> si (se); si; ++si) {
        if (si->IsSetClass ()) {
            if (si->GetClass () == CBioseq_set::eClass_gen_prod_set) {
                m_IsGPS = true;
            }
        }
    }

    // Examine all Seq-ids on Bioseqs
    for (CTypeConstIterator <CBioseq> bi (se); bi; ++bi) {
        iterate (CBioseq::TId, id, bi->GetId()) {
            CSeq_id::E_Choice typ = (**id).Which();
            switch (typ) {
                case CSeq_id::e_not_set:
                    break;
                case CSeq_id::e_Local:
                    break;
                case CSeq_id::e_Gibbsq:
                    break;
                case CSeq_id::e_Gibbmt:
                    break;
                case CSeq_id::e_Giim:
                    break;
                case CSeq_id::e_Genbank:
                    m_IsGED = true;
                    break;
                case CSeq_id::e_Embl:
                    m_IsGED = true;
                    break;
                case CSeq_id::e_Pir:
                    break;
                case CSeq_id::e_Swissprot:
                    break;
                case CSeq_id::e_Patent:
                    m_IsPatent = true;
                    break;
                case CSeq_id::e_Other:
                    m_IsRefSeq = true;
                    // and do RefSeq subclasses up front as well
                    if ((**id).GetOther().IsSetAccession()) {
                        string acc = (**id).GetOther().GetAccession().substr(0, 3);
                        if (acc == "NC_") {
                            m_IsNC = true;
                        } else if (acc == "NG_") {
                            m_IsNG = true;
                        } else if (acc == "NM_") {
                            m_IsNM = true;
                        } else if (acc == "NP_") {
                            m_IsNP = true;
                        } else if (acc == "NR_") {
                            m_IsNR = true;
                        } else if (acc == "NS_") {
                            m_IsNS = true;
                        } else if (acc == "NT_") {
                            m_IsNT = true;
                        } else if (acc == "NW_") {
                            m_IsNW = true;
                        } else if (acc == "XR_") {
                            m_IsXR = true;
                        }
                    }
                    break;
                case CSeq_id::e_General:
                    if (!NStr::CompareCase((**id).GetGeneral().GetDb(), "BankIt")) {
                        m_IsTPA = true;
                    }
                    break;
                case CSeq_id::e_Gi:
                    m_IsGI = true;
                    break;
                case CSeq_id::e_Ddbj:
                    m_IsGED = true;
                    break;
                case CSeq_id::e_Prf:
                    break;
                case CSeq_id::e_Pdb:
                    m_IsPDB = true;
                    break;
                case CSeq_id::e_Tpg:
                    m_IsTPA = true;
                    break;
                case CSeq_id::e_Tpe:
                    m_IsTPA = true;
                    break;
                case CSeq_id::e_Tpd:
                    m_IsTPA = true;
                    break;
                default:
                    break;
            }
            // store the seq_id in the initial seq_entry
            m_InitialSeqIds.insert(*id);
        }
    }


    // Iterate thru components of record and validate each
    for (CTypeConstIterator <CSeq_feat> fi (se); fi; ++fi) {
        ValidateSeqFeat (*fi);
    }

    for (CTypeConstIterator <CSeqdesc> di (se); di; ++di) {
        ValidateSeqDesc (*di);
    }

    for (CTypeConstIterator <CBioseq> bi (se); bi; ++bi) {
        ValidateBioseq (*bi);
    }

    for (CTypeConstIterator <CBioseq_set> si (se); si; ++si) {
        ValidateSeqSet (*si);
    }

    for (CTypeConstIterator <CSeq_align> ai (se); ai; ++ai) {
        ValidateSeqAlign (*ai);
    }

    for (CTypeConstIterator <CSeq_graph> gi (se); gi; ++gi) {
        ValidateSeqGraph (*gi);
    }

    for (CTypeConstIterator <CSeq_annot> ni (se); ni; ++ni) {
        ValidateSeqAnnot (*ni);
    }

    for (CTypeConstIterator <CSeq_descr> ei (se); ei; ++ei) {
        ValidateDescrChain (*ei);
    }

    ReportMissingPubs(*seq, cs);
    ReportMissingBiosource(*seq);
    ReportProtWithoutFullRef();
}


void CValidError_impl::ReportMissingPubs(const CBioseq& seq, const CCit_sub* cs)
{
    if (m_NoPubs  &&  !m_IsGPS  &&  !m_IsRefSeq  &&  !cs) {
        ValidErr(eDiag_Error, eErr_SEQ_DESCR_NoPubFound, 
            "No publications anywhere on this entire record", seq);
    } else {
        size_t num_no_pubs = m_BioseqWithNoPubs.size();
        if ( num_no_pubs > 10 ) {
            ValidErr(eDiag_Error, eErr_SEQ_DESCR_NoPubFound, 
                NStr::IntToString(num_no_pubs) + 
                " Bioseqs without publication in this record  (first reported)",
                *(m_BioseqWithNoPubs[0]));
        } else {
            string msg;
            for ( size_t i =0; i < num_no_pubs; ++i ) {
                msg = NStr::IntToString(i + 1) + " of " + 
                    NStr::IntToString(num_no_pubs) + 
                    " Bioseqs without publication";
                ValidErr(eDiag_Error, eErr_SEQ_DESCR_NoPubFound, msg, 
                    *(m_BioseqWithNoPubs[0]));
            }
        }
    }        
}


void CValidError_impl::ReportMissingBiosource(const CBioseq& seq)
{
    if(m_NoBioSource  &&  !m_IsPatent  &&  !m_IsPDB) {
        ValidErr(eDiag_Error, eErr_SEQ_DESCR_NoOrgFound,
            "No organism name anywhere on this entire record", seq);
    } else {
        size_t num_no_source = m_BioseqWithNoSource.size();
        if ( num_no_source > 10 ) {
            ValidErr(eDiag_Error, eErr_SEQ_DESCR_NoOrgFound, 
                NStr::IntToString(num_no_source) + 
                " Bioseqs without source in this record (first reported)",
                *(m_BioseqWithNoSource[0]));
        } else {
            string msg;
            for ( size_t i =0; i < num_no_source; ++i ) {
                msg = NStr::IntToString(i + 1) + " of " + 
                    NStr::IntToString(num_no_source) + 
                    " Bioseqs without publication";
                ValidErr(eDiag_Error, eErr_SEQ_DESCR_NoOrgFound, msg, 
                    *(m_BioseqWithNoSource[0]));
            }
        }
    }        
}


void CValidError_impl::ReportProtWithoutFullRef(void)
{
    size_t num = m_ProtWithNoFullRef.size();
    
    if ( num > 10 ) {
        ValidErr(eDiag_Error, eErr_SEQ_FEAT_NoProtRefFound, 
            NStr::IntToString(num) + " Bioseqs with no full length " 
            "Prot-ref feature (first reported)",
            *(m_ProtWithNoFullRef[0]));
    } else {
        string msg;
        for ( size_t i = 0; i < num; ++i ) {
            msg = NStr::IntToString(i + 1) + " of " + 
                NStr::IntToString(num) + 
                " Bioseqs without full length Prot-ref feature";
            ValidErr(eDiag_Error, eErr_SEQ_FEAT_NoProtRefFound, msg, 
                *(m_ProtWithNoFullRef[0]));
        }
    }
}   


void CValidError_impl::Validate(const CSeq_submit& ss)
{
    // Check that ss is type e_Entrys
    if (ss.GetData().Which() != CSeq_submit::C_Data::e_Entrys) {
        return;
    }

    // Get CCit_sub pointer
    const CCit_sub* cs = &ss.GetSub().GetCit();

    // Just loop thru CSeq_entrys
    list< CRef< CSeq_entry > >::const_iterator i;
    i = ss.GetData().GetEntrys().begin();
    for (; i != ss.GetData().GetEntrys().end(); ++i) {
        const CSeq_entry* se = (*i).GetPointer();
        Validate(*se, cs);
    }
}


void CValidError_impl::InitializeSourceQualTags() 
{
    int size = sizeof(sm_SourceQualPrefixes) / sizeof(string);

    for (int i = 0; i < size; ++i ) {
        m_SourceQualTags.AddWord(sm_SourceQualPrefixes[i]);
    }

    m_SourceQualTags.Prime();
}


void CValidError_impl::ValidateSeqLen(const CBioseq& seq)
{

    const CSeq_inst& inst = seq.GetInst();

    TSeqPos len = inst.IsSetLength() ? inst.GetLength() : 0;
    if (s_isAa(seq)) {
        if (len <= 3  &&  !m_IsPDB) {
            ValidErr(eDiag_Warning, eErr_SEQ_INST_ShortSeq, "Sequence only " +
                NStr::IntToString(len) + " residue(s) long", seq);
        }
    } else {
        if (len <= 10  &&  !m_IsPDB) {
            ValidErr(eDiag_Warning, eErr_SEQ_INST_ShortSeq, "Sequence only " +
                NStr::IntToString(len) + " residue(s) long", seq);
        }
    }

    if (len <= 350000) {
        return;
    }

    CTypeConstIterator<CMolInfo> mi = ConstBegin(seq);
    if (inst.GetRepr() == CSeq_inst::eRepr_delta) {
        if (mi  &&  mi->IsSetTech()  &&  m_IsGED) {
            CMolInfo::ETech tech = static_cast<CMolInfo::ETech>(mi->GetTech());
            if (tech == CMolInfo::eTech_htgs_0  ||
                tech == CMolInfo::eTech_htgs_1  ||
                tech == CMolInfo::eTech_htgs_2)
            {
                ValidErr(eDiag_Warning, eErr_SEQ_INST_LongHtgsSequence,
                    "Phase 0, 1 or 2 HTGS sequence exceeds 350kbp limit",
                    seq);
            } else if (tech == CMolInfo::eTech_htgs_3) {
                ValidErr(eDiag_Warning, eErr_SEQ_INST_SequenceExceeds350kbp,
                    "Phase 3 HTGS sequence exceeds 350kbp limit", seq);
            } else if (tech == CMolInfo::eTech_wgs) {
                ValidErr(eDiag_Warning, eErr_SEQ_INST_SequenceExceeds350kbp,
                    "WGS sequence exceeds 350kbp limit", seq);
            } else {
                len = 0;
                bool litHasData = false;
                CTypeConstIterator<CSeq_literal> lit(ConstBegin(seq));
                for (; lit; ++lit) {
                    if (lit->IsSetSeq_data()) {
                        litHasData = true;
                    }
                    len += lit->GetLength();
                }
                if (len > 350000  && litHasData) {
                    ValidErr(eDiag_Critical, eErr_SEQ_INST_LongLiteralSequence,
                        "Length of sequence literals exceeds 350kbp limit",
                        seq);
                }
            }
        }

    } else if (inst.GetRepr() == CSeq_inst::eRepr_raw) {
        if (mi  &&  mi->IsSetTech()) {
            CMolInfo::ETech tech = static_cast<CMolInfo::ETech>(mi->GetTech());
            if (tech == CMolInfo::eTech_htgs_0  ||
                tech == CMolInfo::eTech_htgs_1  ||
                tech == CMolInfo::eTech_htgs_2)
            {
                ValidErr(eDiag_Warning, eErr_SEQ_INST_LongHtgsSequence,
                    "Phase 0, 1 or 2 HTGS sequence exceeds 350kbp limit",
                    seq);
            } else if (tech == CMolInfo::eTech_htgs_3) {
                ValidErr(eDiag_Warning, eErr_SEQ_INST_SequenceExceeds350kbp,
                    "Phase 3 HTGS sequence exceeds 350kbp limit", seq);
            } else if (tech == CMolInfo::eTech_wgs) {
                ValidErr(eDiag_Warning, eErr_SEQ_INST_SequenceExceeds350kbp,
                    "WGS sequence exceeds 350kbp limit", seq);
            } else {
                ValidErr (eDiag_Warning, eErr_SEQ_INST_SequenceExceeds350kbp,
                    "Length of sequence exceeds 350kbp limit", seq);
            }
        }
    }
}


// Assumes that seq is segmented and has Seq-ext data
void CValidError_impl::ValidateSeqParts(const CBioseq& seq)
{
    // Get parent CSeq_entry of seq
    const CSeq_entry* parent = seq.GetParentEntry();
    if (!parent) {
        return;
    }

    // Need to get seq-set containing parent and then find the next
    // CSeq_entry in the set. This CSeq_entry should be a CBioseq_set
    // of class parts.
    const CSeq_entry* grand_parent = parent->GetParentEntry();
    if (!grand_parent  ||  !grand_parent->IsSet()) {
        return;
    }
    const CBioseq_set& set = grand_parent->GetSet();
    const list< CRef<CSeq_entry> >& seq_set = set.GetSeq_set();

    // Loop through seq_set looking for parent. When found, the next
    // CSeq_entry should have the parts
    const CSeq_entry* se = 0;
    iterate (list< CRef<CSeq_entry> >, it, seq_set) {
        if (parent == &(**it)) {
            ++it;
            se = &(**it);
            break;
        }
    }
    if (!se) {
        return;
    }

    // Check that se is a CBioseq_set of class parts
    if (!se->IsSet()  ||  !se->GetSet().IsSetClass()  ||
        se->GetSet().GetClass() != CBioseq_set::eClass_parts) {
        return;
    }

    // Get iterator for CSeq_entries in se
    CTypeConstIterator<CSeq_entry> se_parts(ConstBegin(*se));

    // Now, simultaneously loop through the parts of se and CSeq_locs of seq's
    // CSseq-ext. If don't compare, post error
    const list< CRef<CSeq_loc> > locs = seq.GetInst().GetExt().GetSeg().Get();
    iterate (list< CRef<CSeq_loc> >, lit, locs) {
        if ((**lit).Which() == CSeq_loc::e_Null) {
            continue;
        }
        if (!se_parts) {
            ValidErr(eDiag_Error, eErr_SEQ_INST_PartsOutOfOrder,
                "Parts set does not contain enough Bioseqs", seq);
        }
        if (se_parts->Which() == CSeq_entry::e_Seq) {
            try {
                const CBioseq& part = se_parts->GetSeq();
                const CSeq_id& id = GetId(**lit, m_Scope.GetPointer());
                if (!s_IsIdIn(id, part)) {
                    ValidErr(eDiag_Error, eErr_SEQ_INST_PartsOutOfOrder,
                        "Segmented bioseq seq_ext does not correspond to parts"
                        "packaging order", seq);
                }
            } catch (const CNotUnique&) {
                ERR_POST("Seq-loc not for unique sequence");
                return;
            } catch (...) {
                ERR_POST("Unknown error");
                return;
            }
        } else {
            ValidErr(eDiag_Error, eErr_SEQ_INST_PartsOutOfOrder,
                "Parts set component is not Bioseq", seq);
            return;
        }
        ++se_parts;
    }
    if (se_parts) {
        ValidErr(eDiag_Error, eErr_SEQ_INST_PartsOutOfOrder,
            "Parts set contains too many Bioseqs", seq);
    }
}


// Assumes seq is an amino acid sequence
void CValidError_impl::ValidateProteinTitle(const CBioseq& seq)
{
    const CSeq_id* id = seq.GetFirstId();
    if (!id) {
        return;
    }

    CBioseq_Handle hnd = m_Scope.GetPointer()->GetBioseqHandle(*id);
    string title_no_recon = GetTitle(hnd);
    string title_recon = GetTitle(hnd, fGetTitle_Reconstruct);
    if (title_no_recon != title_recon) {
        ValidErr(eDiag_Warning, eErr_SEQ_DESCR_InconsistentProteinTitle,
            "Instantiated protein title does not match automatically "
            "generated title", seq);
    }
}


void CValidError_impl::ValidateFeatPartialness(const CSeq_feat& feat)
{
    unsigned int  partial_prod = eSeqlocPartial_Complete, 
                  partial_loc = eSeqlocPartial_Complete;
    static string parterr[2] = { "PartialProduct", "PartialLocation" };
    static string parterrs[4] = {
        "Start does not include first/last residue of sequence",
        "Stop does not include first/last residue of sequence",
        "Internal partial intervals do not include first/last residue of sequence",
        "Improper use of partial (greater than or less than)"
    };

    partial_loc  = s_GetSeqlocPartialInfo(feat.GetLocation (), m_Scope.GetPointer () );
    if (feat.IsSetProduct ()) {
        partial_prod = s_GetSeqlocPartialInfo(feat.GetProduct (), m_Scope.GetPointer () );
    }

    if ( (partial_loc  != eSeqlocPartial_Complete)  ||
         (partial_prod != eSeqlocPartial_Complete)  ||   
        feat.GetPartial () == true ) {
        // a feature on a partial sequence should be partial -- it often isn't
        if ( !feat.GetPartial ()  &&
             partial_loc != eSeqlocPartial_Complete  &&
             feat.GetLocation ().Which () == CSeq_loc::e_Whole ) {
            ValidErr(eDiag_Info, eErr_SEQ_FEAT_PartialProblem,
                        "On partial Bioseq, SeqFeat.partial should be TRUE", feat);
        }
        // a partial feature, with complete location, but partial product
        else if ( feat.GetPartial()  &&
                  partial_loc == eSeqlocPartial_Complete  &&
                  feat.IsSetProduct () &&
                  feat.GetProduct ().Which () == CSeq_loc::e_Whole  &&
                  partial_prod != eSeqlocPartial_Complete ) {
            ValidErr(eDiag_Warning, eErr_SEQ_FEAT_PartialProblem,
                        "When SeqFeat.product is a partial Bioseq, SeqFeat.location should also be partial", feat);
        }
        // gene on segmented set is now 'order', should also be partial
        else if ( feat.GetData ().IsGene ()  &&
                  !feat.IsSetProduct ()  &&
                  partial_loc == eSeqlocPartial_Internal ) {
            ValidErr(eDiag_Warning, eErr_SEQ_FEAT_PartialProblem,
                        "Gene of 'order' with otherwise complete location should have partial flag set", feat);
        }
        // inconsistent combination of partial/complete product,location,partial flag
        else if ( (partial_prod == eSeqlocPartial_Complete  &&  feat.IsSetProduct ())  ||
             partial_loc == eSeqlocPartial_Complete  ||
             !feat.GetPartial () ) {
            string str("Inconsistent: ");
            if ( feat.IsSetProduct () ) {
                str += "Product= ";
                if ( partial_prod != eSeqlocPartial_Complete ) {
                    str += "partial, ";
                } else {
                    str += "complete, ";
                }
            }
            str += "Location= ";
            if ( partial_loc != eSeqlocPartial_Complete ) {
                str += "partial, ";
            } else {
                str += "complete, ";
            }
            str += "Feature.partial= ";
            if ( feat.GetPartial () ) {
                str += "TRUE";
            } else {
                str += "FALSE";
            }
            ValidErr(eDiag_Warning, eErr_SEQ_FEAT_PartialProblem, str, feat);
        }
        // 5' or 3' partial location giving unclassified partial product
        else if ( (partial_loc & eSeqlocPartial_Start || partial_loc & eSeqlocPartial_Stop) &&
                  partial_prod & eSeqlocPartial_Other &&
                  feat.GetPartial () ) {
            ValidErr(eDiag_Warning, eErr_SEQ_FEAT_PartialProblem,
                "5' or 3' partial location should not have unclassified partial location", feat);
        }
        
        // may have other error bits set as well 
        unsigned int partials[2] = { partial_prod, partial_loc };
        for ( int i = 0; i < 2; ++i ) {
            unsigned int errtype = eSeqlocPartial_Nostart;
            for ( int j = 0; j < 4; ++j ) {
                if (partials[i] & errtype) {
                if (i == 1 && j < 2 ) { // !!! && PartialAtSpliceSite (sfp->location, errtype) ) {
                        ValidErr (eDiag_Info, eErr_SEQ_FEAT_PartialProblem,
                            parterr[i] + ":" + parterrs[j] + "(but is at consensus splice site)", feat);
                    } else if (feat.GetData ().Which () == CSeqFeatData::e_Cdregion && j == 0) {
                        ValidErr (eDiag_Warning, eErr_SEQ_FEAT_PartialProblem,
                            parterr[i] + 
                            ": 5' partial is not at start AND is not at consensus splice site", feat); 
                    } else if (feat.GetData ().Which () == CSeqFeatData::e_Cdregion && j == 1) {
                        ValidErr (eDiag_Warning, eErr_SEQ_FEAT_PartialProblem,
                            parterr[i] + 
                            ": 3' partial is not at stop AND is not at consensus splice site", feat);
                    } else {
                        ValidErr (eDiag_Warning, eErr_SEQ_FEAT_PartialProblem,
                            parterr[i] + ": " + parterrs[j], feat);
                    }
                }
                errtype <<= 1;
            }
        }
    }
}


void CValidError_impl::ValidateBioseqContext(const CBioseq& seq)
{
    // Get Molinfo
    CTypeConstIterator<CMolInfo> mi(ConstBegin(seq));

    if ( mi  &&  mi->IsSetTech()) {
        switch (mi->GetTech()) {
        case CMolInfo::eTech_sts:
        case CMolInfo::eTech_survey:
        case CMolInfo::eTech_wgs:
        case CMolInfo::eTech_htgs_0:
        case CMolInfo::eTech_htgs_1:
        case CMolInfo::eTech_htgs_2:
        case CMolInfo::eTech_htgs_3:
            if (mi->GetTech() == CMolInfo::eTech_sts  &&
                seq.GetInst().GetMol() == CSeq_inst::eMol_rna  &&
                mi->IsSetBiomol()  &&
                mi->GetBiomol() == CMolInfo::eBiomol_mRNA) {
                // !!!
                // Ok, there are some STS sequences derived from 
                // cDNAs, so do not report these
            } else if (mi->IsSetBiomol()  &&
                mi->GetBiomol() != CMolInfo::eBiomol_genomic) {
                ValidErr(eDiag_Error, eErr_SEQ_INST_ConflictingBiomolTech,
                    "HTGS/STS/GSS/WGS sequence should be genomic", seq);
            } else if (seq.GetInst().GetMol() != CSeq_inst::eMol_dna  &&
                seq.GetInst().GetMol() != CSeq_inst::eMol_na) {
                    ValidErr(eDiag_Error, eErr_SEQ_INST_ConflictingBiomolTech,
                        "HTGS/STS/GSS/WGS sequence should not be RNA", seq);
            }
            break;
        default:
            break;
        }
    }
    
    // Check that proteins in nuc_prot set have a CdRegion
    if (s_CdError(seq, m_Scope.GetPointer())) {
        ValidErr(eDiag_Error, eErr_SEQ_PKG_NoCdRegionPtr,
            "No CdRegion in nuc-prot set points to this protein", seq);
    }

    // Check that gene on non-segmented sequence does not have
    // multiple intervals
    ValidateMultiIntervalGene(seq);

    ValidateSeqFeatContext(seq);

    // Check for duplicate features and overlapping peptide features.
    ValidateDupOrOverlapFeats(seq);

    // Check for colliding gene names
    ValidateCollidingGeneNames(seq);

    ValidateSeqDescContext(seq);

    if ( !m_NoPubs ) {  // make sure that there is a pub on this bioseq
        CheckForPubOnBioseq(seq);
    }
    if ( !m_NoBioSource ) { // make sure that there is a source on this bioseq
        CheckForBiosourceOnBioseq(seq);
    }
}


// look for a pub desc on the bioseq, if none
// look for a covarge of the bioseq by pub feat
void CValidError_impl::CheckForPubOnBioseq(const CBioseq& seq)
{
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);
    if ( !bsh ) {
        return;
    }

    // Check for CPubdesc on the biodseq
    CSeqdesc_CI desc( bsh, CSeqdesc::e_Pub );
    if ( desc ) {
        return;
    }
    
    if ( !seq.GetInst().IsSetLength() ) {
        return;
    }

    // check for full cover of the bioseq by pub features.
    TSeqPos len = bsh.GetSeqVector().size();
    bool covered = false;
    CSeq_loc::TRange range = CSeq_loc::TRange::GetEmpty();

    for ( CFeat_CI fi(bsh, 0, 0, CSeqFeatData::e_Pub); fi; ++fi ) {
        range += fi->GetLocation().GetTotalRange();;
        if ( (fi->GetLocation().IsWhole())  ||
             ((range.GetFrom() == 0)  &&  (range.GetTo() == len - 1)) ) {
            covered = true;
            break;
        }
    }
    if ( !covered ) {
        m_BioseqWithNoPubs.push_back( CConstRef<CBioseq>(&seq) );
    }
}


void CValidError_impl::CheckForBiosourceOnBioseq(const CBioseq& seq)
{
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);
    CSeqdesc_CI di(bsh, CSeqdesc::e_Source);
    if ( !di ) {
        m_BioseqWithNoSource.push_back(CConstRef<CBioseq>(&seq));
    }
}


void CValidError_impl::ValidateMultiIntervalGene(const CBioseq& seq)
{
    // Create a loc for iterator
    CSeq_loc loc;
    CSeq_id& lid = loc.SetWhole();
    const CSeq_id* sid = seq.GetFirstId();
    if (!sid) {
        return;
    }
    SerialAssign(lid, *sid);

    // Loop through features on gene
    for (CFeat_CI fit(*m_Scope.GetPointer(), loc, CSeqFeatData::e_Gene);
        fit; ++fit) {
            // !!!
    }
}


// Functional used by multimap below for sorting
class CRangeCmp
{
public:
    bool operator()(const CRange<TSeqPos>& r1, const CRange<TSeqPos>& r2)
    {
        if (r1.GetFrom() < r2.GetFrom()) {
            return true;
        } else if (r1.GetFrom() > r2.GetFrom()) {
            return false;
        } else if (r1.GetTo() < r2.GetTo()) {
            return true;
        } else {
            return false;
        }
    }
};


size_t CValidError_impl::SearchNoCase
(const string& text,
 const string& pat) const {
    string str = text, pattern = pat;
    NStr::ToLower(str);
    NStr::ToLower(pattern);
    return str.find(pattern);
}


bool CValidError_impl::NotPeptideException
(const CFeat_CI& curr,
 const CFeat_CI& prev) const
{
    string  alternative = "alternative processing",
            alternate   = "alternate processing";
    
    if ( curr->GetExcept() ) {
        if ( SearchNoCase(curr->GetExcept_text(), alternative) != string::npos ||
             SearchNoCase(curr->GetExcept_text(), alternate) != string::npos) {
            return false;
        }
    }
    if ( prev->GetExcept() ) {
        if ( SearchNoCase(prev->GetExcept_text(), alternative) != string::npos ||
             SearchNoCase(prev->GetExcept_text(), alternate) != string::npos ) {
            return false;
        }
    }
    return true;
}


bool CValidError_impl::IsEqualSeqAnnot
(const CFeat_CI& fi1,
 const CFeat_CI& fi2) const
{
    // !!!
    return true;
}


bool CValidError_impl::IsEqualSeqAnnotDesc
(const CFeat_CI& fi1,
 const CFeat_CI& fi2) const
{
    // !!!
    return true;
}


bool CValidError_impl::DifferentDbxrefs
(const list< CRef< CDbtag > >& dbxref1,
 const list< CRef< CDbtag > >& dbxref2) const
{
    if ( dbxref1.empty() || dbxref2.empty() ) {
        return false;
    }

    const CDbtag& dbt1 = **dbxref1.begin();
    const CDbtag& dbt2 = **dbxref2.begin();
    return !dbt1.Match(dbt2);
}


void CValidError_impl::ValidateDupOrOverlapFeats(const CBioseq& bioseq)
{
    ENa_strand              curr_strand, prev_strand;
    CCdregion::EFrame       curr_frame, prev_frame;
    const CSeq_loc*         curr_location = 0, *prev_location = 0;
    EDiagSev                severity;
    CSeqFeatData::ESubtype  curr_subtype = CSeqFeatData::eSubtype_bad, 
                            prev_subtype = CSeqFeatData::eSubtype_bad;
    bool same_label;
    string curr_label, prev_label;
    
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle (bioseq);

    bool fruit_fly = false;
    CSeqdesc_CI di(bsh, CSeqdesc::e_Source);
    if ( di ) {
        if ( NStr::CompareNocase(di->GetSource().GetOrg().GetTaxname(),
             "Drosophila melanogaster") == 0 ) {
            fruit_fly = true;
        }
    }

    CFeat_CI curr(bsh, 0, 0, CSeqFeatData::e_not_set);
    CFeat_CI prev = curr;
    ++curr;
    while ( curr ) {
        curr_location = &(curr->GetLocation());
        prev_location = &(prev->GetLocation());
        curr_subtype = curr->GetData().GetSubtype();
        prev_subtype = prev->GetData().GetSubtype();

        // if same location, subtype and strand
        if ( Compare(*curr_location, *prev_location, m_Scope) == eSame &&
             curr_subtype == prev_subtype ) {

            curr_strand = GetStrand(curr->GetLocation(), m_Scope);
            prev_strand = GetStrand(prev->GetLocation(), m_Scope);
            if ( curr_strand == prev_strand         || 
                 curr_strand == eNa_strand_unknown  ||
                 prev_strand == eNa_strand_unknown  ) {

                if ( IsEqualSeqAnnot(curr, prev)  ||
                     IsEqualSeqAnnotDesc(curr, prev) ) {
                    severity = eDiag_Error;

                    // compare labels and comments
                    same_label = true;
                    const string &curr_comment = curr->GetComment();
                    const string &prev_comment = prev->GetComment();
                    curr_label.erase();
                    prev_label.erase();
                    feature::GetLabel(*curr, &curr_label, feature::eContent, m_Scope);
                    feature::GetLabel(*prev, &prev_label, feature::eContent, m_Scope);
                    if ( NStr::CompareNocase(curr_comment, prev_comment) != 0  ||
                         NStr::CompareNocase(curr_label, prev_label) != 0 ) {
                        same_label = false;
                    }

                    // lower sevrity for the following cases:

                    if ( curr_subtype == CSeqFeatData::eSubtype_pub          ||
                         curr_subtype == CSeqFeatData::eSubtype_region       ||
                         curr_subtype == CSeqFeatData::eSubtype_misc_feature ||
                         curr_subtype == CSeqFeatData::eSubtype_STS          ||
                         curr_subtype == CSeqFeatData::eSubtype_variation ) {
                        severity = eDiag_Warning;
                    } else if ( !(m_IsGPS || m_IsNT || m_IsNC) ) {
                        severity = eDiag_Warning;
                    } else if ( fruit_fly ) {
                        // curated fly source still has duplicate features
                        severity = eDiag_Warning;
                    }
                    
                    // if different CDS frames, lower to warning
                    if ( curr->GetData().Which() == CSeqFeatData::e_Cdregion ) {
                        curr_frame = curr->GetData().GetCdregion().GetFrame();
                        prev_frame = CCdregion::eFrame_not_set;
                        if ( prev->GetData().Which() == CSeqFeatData::e_Cdregion ) {
                            prev_frame = prev->GetData().GetCdregion().GetFrame();
                        }
                        
                        if ( (curr_frame != CCdregion::eFrame_not_set  &&
                            curr_frame != CCdregion::eFrame_one)     ||
                            (prev_frame != CCdregion::eFrame_not_set  &&
                            prev_frame != CCdregion::eFrame_one) ) {
                            if ( curr_frame != prev_frame ) {
                                severity = eDiag_Warning;
                            }
                        }
                    }

                    // Report duplicates
                    if ( curr_subtype == CSeqFeatData::eSubtype_region  &&
                        DifferentDbxrefs(curr->GetDbxref(), prev->GetDbxref()) ) {
                        // do not report if both have dbxrefs and they are 
                        // different.
                    } else if ( IsEqualSeqAnnot(curr, prev) ) {  // !!!
                        if (same_label) {
                            ValidErr (severity, eErr_SEQ_FEAT_FeatContentDup, 
                                "Duplicate feature", *curr);
                        } else if ( curr_subtype != CSeqFeatData::eSubtype_pub ) {
                            ValidErr (severity, eErr_SEQ_FEAT_DuplicateFeat,
                                "Features have identical intervals, but labels"
                                "differ", *curr);
                        }
                    } else {
                        if (same_label) {
                            ValidErr (severity, eErr_SEQ_FEAT_FeatContentDup, 
                                "Duplicate feature (packaged in different feature table)",
                                *curr);
                        } else if ( prev_subtype != CSeqFeatData::eSubtype_pub ) {
                            ValidErr (severity, eErr_SEQ_FEAT_DuplicateFeat,
                                "Features have identical intervals, but labels"
                                "differ (packaged in different feature table)",
                                *curr);
                        }
                    }
                }

                if ( (curr_subtype == CSeqFeatData::eSubtype_mat_peptide_aa       ||
                      curr_subtype == CSeqFeatData::eSubtype_sig_peptide_aa       ||
                      curr_subtype == CSeqFeatData::eSubtype_transit_peptide_aa)  &&
                     (prev_subtype == CSeqFeatData::eSubtype_mat_peptide_aa       ||
                      prev_subtype == CSeqFeatData::eSubtype_sig_peptide_aa       ||
                      prev_subtype == CSeqFeatData::eSubtype_transit_peptide_aa) ) {
                    if ( Compare(*curr_location, *prev_location, m_Scope) == eOverlap &&
                        NotPeptideException(curr, prev) ) {
                        EDiagSev overlapPepSev = 
                            m_OvlPepErr ? eDiag_Error :eDiag_Warning;
                        ValidErr( overlapPepSev,
                            eErr_SEQ_FEAT_OverlappingPeptideFeat,
                            "Signal, Transit, or Mature peptide features overlap",
                            *curr);
                    }
                }
            }
        }

        prev = curr; 
        ++curr;
    }  // end of while loop
}


class CNoCaseCompare
{
public:
    bool operator ()(const string& s1, const string& s2) const
    {
        if (NStr::CompareNocase(s1, s2) < 0) {
            return true;
        } else {
            return false;
        }
    }
};


void CValidError_impl::ValidateCollidingGeneNames(const CBioseq& ) //seq)
{
    // !!!
    /*
    // Loop through features and insert into multimap sorted by
    // feature label--case insensitive
    typedef multimap<string, const CSeq_feat*, CNoCaseCompare> TSmap;
    TSmap label_map;
    string label;
    for (CTypeConstIterator<CSeq_feat> ft(ConstBegin(seq)); ft; ++ft) {
        label.erase();
        feature::GetLabel(*ft, &label, feature::eContent, m_Scope.GetPointer());
        label_map.insert(TSmap::value_type(label, &*ft));
    }

    // Iterate through multimap and compare labels
    bool first = true;
    const string* plabel;
    iterate (TSmap, it, label_map) {
        if (first) {
            first = false;
            plabel = &(it->first);
            continue;
        }
        if (NStr::CompareCase(*plabel, it->first)) {
            ValidErr(eDiag_Warning, eErr_SEQ_FEAT_CollidingGeneNames,
                "Colliding names in gene features", *it->second);
        } else if (NStr::CompareNocase(*plabel, it->first)) {
            ValidErr(eDiag_Warning, eErr_SEQ_FEAT_CollidingGeneNames,
                "Colliding names (with different capitalization) in gene"
                "features", *it->second);
        }
        plabel = &(it->first);
    }
    */
}


// Assumes that seq is eRepr_raw or eRepr_inst
void CValidError_impl::ValidateRawConst(const CBioseq& seq)
{
    const CSeq_inst& inst = seq.GetInst();
    const CEnumeratedTypeValues* tv = CSeq_inst::GetTypeInfo_enum_ERepr();
    const string& rpr = tv->FindName(inst.GetRepr(), true);

    if (inst.IsSetFuzz()) {
        ValidErr(eDiag_Error, eErr_SEQ_INST_FuzzyLen,
            "Fuzzy length on " + rpr + "Bioseq", seq);
    }

    if (!inst.IsSetLength()  ||  inst.GetLength() == 0) {
        string len = inst.IsSetLength() ?
            NStr::IntToString(inst.GetLength()) : "0";
        ValidErr(eDiag_Error, eErr_SEQ_INST_InvalidLen,
                 "Invalid Bioseq length [" + len + "]", seq);
    }

    CSeq_data::E_Choice seqtyp = inst.IsSetSeq_data() ?
        inst.GetSeq_data().Which() : CSeq_data::e_not_set;
    switch (seqtyp) {
    case CSeq_data::e_Iupacna:
    case CSeq_data::e_Ncbi2na:
    case CSeq_data::e_Ncbi4na:
    case CSeq_data::e_Ncbi8na:
    case CSeq_data::e_Ncbipna:
        if (s_isAa(inst)) {
            ValidErr(eDiag_Critical, eErr_SEQ_INST_InvalidAlphabet,
                     "Using a nucleic acid alphabet on a protein sequence",
                     seq);
            return;
        }
        break;
    case CSeq_data::e_Iupacaa:
    case CSeq_data::e_Ncbi8aa:
    case CSeq_data::e_Ncbieaa:
    case CSeq_data::e_Ncbipaa:
    case CSeq_data::e_Ncbistdaa:
        if (s_isNa(inst)) {
            ValidErr(eDiag_Critical, eErr_SEQ_INST_InvalidAlphabet,
                     "Using a protein alphabet on a nucleic acid",
                     seq);
            return;
        }
        break;
    default:
        ValidErr(eDiag_Critical, eErr_SEQ_INST_InvalidAlphabet,
                 "Sequence alphabet not set",
                 seq);
        return;
    }

    bool check_alphabet = false;
    unsigned int factor = 1;
    switch (seqtyp) {
    case CSeq_data::e_Iupacaa:
    case CSeq_data::e_Iupacna:
    case CSeq_data::e_Ncbieaa:
    case CSeq_data::e_Ncbistdaa:
        check_alphabet = true;
        break;
    case CSeq_data::e_Ncbi8na:
    case CSeq_data::e_Ncbi8aa:
        break;
    case CSeq_data::e_Ncbi4na:
        factor = 2;
        break;
    case CSeq_data::e_Ncbi2na:
        factor = 4;
        break;
    case CSeq_data::e_Ncbipna:
        factor = 5;
        break;
    case CSeq_data::e_Ncbipaa:
        factor = 30;
        break;
    default:
        // Logically, should not occur
        ValidErr(eDiag_Critical, eErr_SEQ_INST_InvalidAlphabet,
                 "Sequence alphabet not set",
                 seq);
        return;
    }
    TSeqPos calc_len = inst.IsSetLength() ? inst.GetLength() : 0;
    string s_len = NStr::IntToString(calc_len);
    switch (seqtyp) {
    case CSeq_data::e_Ncbipna:
    case CSeq_data::e_Ncbipaa:
        calc_len *= factor;
        break;
    default:
        if (calc_len % factor) {
            calc_len += factor;
        }
        calc_len /= factor;
    }
    TSeqPos data_len = s_GetDataLen(inst);
    string s_data_len = NStr::IntToString(data_len);
    if (calc_len > data_len) {
        ValidErr(eDiag_Critical, eErr_SEQ_INST_SeqDataLenWrong,
                 "Bioseq.seq_data too short [" + s_data_len +
                 "] for given length [" + s_len + "]", seq);
        return;
    } else if (calc_len < data_len) {
        ValidErr(eDiag_Critical, eErr_SEQ_INST_SeqDataLenWrong,
                 "Bioseq.seq_data is larger [" + s_data_len +
                 "] than given length [" + s_len + "]", seq);
    }

    unsigned char termination;
    unsigned int trailingX = 0;
    unsigned int terminations = 0;
    if (check_alphabet) {

        switch (seqtyp) {
        case CSeq_data::e_Iupacaa:
        case CSeq_data::e_Ncbieaa:
            termination = '*';
            break;
        case CSeq_data::e_Ncbistdaa:
            termination = 25;
            break;
        default:
            termination = '\0';
        }

        const CSeq_id* id = (*seq.GetId().begin()).GetPointer();
        CSeqVector sv = m_Scope->GetBioseqHandle(*id).GetSeqVector();

        unsigned int bad_cnt = 0;
        for (TSeqPos pos = 0; pos < sv.size(); pos++) {
            if (sv[pos] > 250) {
                if (++bad_cnt > 10) {
                    ValidErr(eDiag_Critical, eErr_SEQ_INST_InvalidResidue,
                        "More than 10 invalid residues. Checking stopped",
                        seq);
                    return;
                } else {
                    if (seqtyp == CSeq_data::e_Ncbistdaa) {
                        ValidErr(eDiag_Critical, eErr_SEQ_INST_InvalidResidue,
                            "Invalid residue [" +
                            NStr::IntToString((int)sv[pos]) +
                            "] in position [" +
                            NStr::IntToString((int)pos) + "]",
                            seq);
                    } else {
                        ValidErr(eDiag_Critical, eErr_SEQ_INST_InvalidResidue,
                            "Invalid residue [" +
                            NStr::IntToString((int)sv[pos]) +
                            "] in position [" +
                            NStr::IntToString((int)pos) + "]",
                            seq);
                    }
                }
            } else if (sv[pos] == termination) {
                terminations++;
                trailingX = 0;
            } else if (sv[pos] == 'X') {
                trailingX++;
            } else {
                trailingX = 0;
            }
        }

        if (trailingX > 0 && !s_SuppressTrailingXMsg(seq, m_Scope)) {
            // Suppress if cds ends in "*" or 3' partial
            ValidErr(eDiag_Warning, eErr_SEQ_INST_TrailingX,
                "Sequence ends in " +
                NStr::IntToString(trailingX) + " trailing X(s)",
                seq);
        }

        if (terminations  && seqtyp != CSeq_data::e_Iupacna) {
            // Post error indicating terminations found in protein sequence
            // First get gene label
            string glbl;
            s_GetGeneLabel(seq, &glbl, m_Scope);
            string plbl;
            s_GetSeqLabel(seq, &plbl, m_Scope);
            ValidErr(eDiag_Error, eErr_SEQ_INST_StopInProtein,
                NStr::IntToString(terminations) +
                " termination symbols in protein sequence (" +
                glbl + string(" - ") + plbl + ")", seq);
            if (!bad_cnt) {
                return;
            }
        }
        return;
    }
}


// Assumes seq is eRepr_seg or eRepr_ref
void CValidError_impl::ValidateSegRef(const CBioseq& seq)
{
    const CSeq_inst& inst = seq.GetInst();

    // Validate extension data -- wrap in CSeq_loc_mix for convenience
    CSeq_loc loc;
    if (s_GetLocFromSeq(seq, &loc)) {
        ValidateSeqLoc(loc, seq, "Segmented Bioseq");
    }

    // Validate Length
    try {
        TSeqPos loclen = GetLength(loc, m_Scope);
        TSeqPos seqlen = inst.IsSetLength() ? inst.GetLength() : 0;
        if (seqlen > loclen) {
            ValidErr(eDiag_Critical, eErr_SEQ_INST_SeqDataLenWrong,
                "Bioseq.seq_data too short [" + NStr::IntToString(loclen) +
                "] for given length [" + NStr::IntToString(seqlen) + "]",
                seq);
        } else if (seqlen < loclen) {
            ValidErr(eDiag_Critical, eErr_SEQ_INST_SeqDataLenWrong,
                "Bioseq.seq_data is larger [" + NStr::IntToString(loclen) +
                "] than given length [" + NStr::IntToString(seqlen) + "]",
                seq);
        }
    } catch (const CNoLength&) {
        ERR_POST(Critical << "Unable to calculate length: ");
    }

    // Check for multiple references to the same Bioseq
    if (inst.IsSetExt()  &&  inst.GetExt().IsSeg()) {
        const list< CRef<CSeq_loc> >& locs = inst.GetExt().GetSeg().Get();
        iterate(list< CRef<CSeq_loc> >, i1, locs) {
           if (!IsOneBioseq(**i1, m_Scope)) {
                continue;
            }
            const CSeq_id& id1 = GetId(**i1);
            list< CRef<CSeq_loc> >::const_iterator i2 = i1;
            for (++i2; i2 != locs.end(); ++i2) {
                if (!IsOneBioseq(**i2, m_Scope)) {
                    continue;
                }
                const CSeq_id& id2 = GetId(**i2);
                if (IsSameBioseq(id1, id2, m_Scope)) {
                    CNcbiOstrstream os;
                    os << id1.DumpAsFasta();
                    string sid(os.str());
                    if ((**i1).IsWhole()  &&  (**i2).IsWhole()) {
                        ValidErr(eDiag_Error,
                            eErr_SEQ_INST_DuplicateSegmentReferences,
                            "Segmented sequence has multiple references to " +
                            sid, seq);
                    } else {
                        ValidErr(eDiag_Warning,
                            eErr_SEQ_INST_DuplicateSegmentReferences,
                            "Segmented sequence has multiple references to " +
                            sid + " that are not e_Whole", seq);
                    }
                }
            }
        }
    }

    // Check that partial sequence info on sequence segments is consistent with
    // partial sequence info on sequence -- aa  sequences only
    unsigned int partial = s_GetSeqlocPartialInfo(loc, m_Scope);
    if (partial  &&  s_isAa(seq)) {
        bool got_partial = false;
        CTypeConstIterator<CSeqdesc> sd(ConstBegin(seq.GetDescr()));
        for (; sd; ++sd) {
            if (!(*sd).IsModif()) {
                continue;
            }
            iterate(list< EGIBB_mod >, md, (*sd).GetModif()) {
                switch (*md) {
                case eGIBB_mod_partial:
                    got_partial = true;
                    break;
                case eGIBB_mod_no_left:
                    if (partial & eSeqlocPartial_Start) {
                        ValidErr(eDiag_Error, eErr_SEQ_INST_PartialInconsistent,
                            "GIBB-mod no-left inconsistent with segmented "
                            "SeqLoc", seq);
                    }
                    got_partial = true;
                    break;
                case eGIBB_mod_no_right:
                    if (partial & eSeqlocPartial_Stop) {
                        ValidErr(eDiag_Error, eErr_SEQ_INST_PartialInconsistent,
                            "GIBB-mod no-right inconsistene with segmented "
                            "SeqLoc", seq);
                    }
                    got_partial = true;
                    break;
                default:
                    break;
                }
            }
        }
        if (!got_partial) {
            ValidErr(eDiag_Error, eErr_SEQ_INST_PartialInconsistent,
                "Partial segmented sequence without GIBB-mod", seq);
        }
    }
}


// Assumes seq is a delta sequence
void CValidError_impl::ValidateDelta(const CBioseq& seq)
{
    const CSeq_inst& inst = seq.GetInst();

    if (!inst.IsSetExt()  ||  !inst.GetExt().IsDelta()  ||
        inst.GetExt().GetDelta().Get().empty()) {
        ValidErr(eDiag_Error, eErr_SEQ_INST_SeqDataLenWrong,
            "No CDelta_ext data for delta Bioseq", seq);
    }

    TSeqPos len = 0;
    iterate(list< CRef<CDelta_seq> >, sg, inst.GetExt().GetDelta().Get()) {
        switch ((**sg).Which()) {
        case CDelta_seq::e_Loc:
            try {
                len += GetLength((**sg).GetLoc(), m_Scope);
            } catch (const CNoLength&) {
                ValidErr(eDiag_Error, eErr_SEQ_INST_SeqDataLenWrong,
                    "No length for Seq-loc of delta seq-ext", seq);
            }
            break;
        case CDelta_seq::e_Literal:
        {
            // The C toolkit code checks for valid alphabet here
            // The C++ object serializaton will not load if invalid alphabet
            // so no check needed here

            // Check for invalid residues
            if (!(**sg).GetLiteral().IsSetSeq_data()) {
                break;
            }
            const CSeq_data& data = (**sg).GetLiteral().GetSeq_data();
            vector<TSeqPos> badIdx;
            CSeqportUtil::Validate(data, &badIdx);
            const string* ss = 0;
            switch (data.Which()) {
            case CSeq_data::e_Iupacaa:
                ss = &data.GetIupacaa().Get();
                break;
            case CSeq_data::e_Iupacna:
                ss = &data.GetIupacna().Get();
                break;
            case CSeq_data::e_Ncbieaa:
                ss = &data.GetNcbieaa().Get();
                break;
            case CSeq_data::e_Ncbistdaa:
            {
                const vector<char>& c = data.GetNcbistdaa().Get();
                iterate (vector<TSeqPos>, ci, badIdx) {
                    ValidErr(eDiag_Error, eErr_SEQ_INST_InvalidResidue,
                        "Invalid residue [" +
                        NStr::IntToString((int)c[*ci]) + "] in position " +
                        NStr::IntToString(*ci), seq);
                }
                break;
            }
            default:
                break;
            }
            if (ss) {
                iterate (vector<TSeqPos>, it, badIdx) {
                    ValidErr(eDiag_Error, eErr_SEQ_INST_InvalidResidue,
                        "Invalid residue [" +
                        ss->substr(*it, 1) + "] in position " +
                        NStr::IntToString(*it), seq);
                }
            }
            len += (**sg).GetLiteral().GetLength();
            break;
        }
        default:
            ValidErr(eDiag_Error, eErr_SEQ_INST_ExtNotAllowed,
                "CDelta_seq::Which() is e_not_set", seq);
        }
    }
    if (inst.GetLength() > len) {
        ValidErr(eDiag_Critical, eErr_SEQ_INST_SeqDataLenWrong,
            "Bioseq.seq_data too short [" + NStr::IntToString(len) +
            "] for given length [" + NStr::IntToString(inst.GetLength()) +
            "]", seq);
    } else if (inst.GetLength() < len) {
        ValidErr(eDiag_Critical, eErr_SEQ_INST_SeqDataLenWrong,
            "Bisoeq.seq_data is larger [" + NStr::IntToString(len) +
            "] than given length [" + NStr::IntToString(inst.GetLength()) +
            "]", seq);
    }

    // Get CMolInfo and tech used for validating technique and gap positioning
    CTypeConstIterator<CMolInfo> mi(ConstBegin(seq.GetDescr()));
    int tech = mi->IsSetTech() ? mi->GetTech() : 0;

    // Validate technique
    if (mi  &&  !m_IsNT  &&  !m_IsNC  &&  !m_IsGPS) {
        if (tech != CMolInfo::eTech_unknown   &&
            tech != CMolInfo::eTech_standard  &&
            tech != CMolInfo::eTech_wgs       &&
            tech != CMolInfo::eTech_htgs_0    &&
            tech != CMolInfo::eTech_htgs_1    &&
            tech != CMolInfo::eTech_htgs_2    &&
            tech != CMolInfo::eTech_htgs_3      ) {
            const CEnumeratedTypeValues* tv =
                CMolInfo::GetTypeInfo_enum_ETech();
            const string& stech = tv->FindName(mi->GetTech(), true);
            ValidErr(eDiag_Error, eErr_SEQ_INST_BadDeltaSeq,
                "Delta seq technique should not be " + stech, seq);
        }
    }

    // Validate positioning of gaps
    CTypeConstIterator<CDelta_seq> ds(ConstBegin(inst));
    if (ds  &&  (*ds).IsLiteral()  &&  !(*ds).GetLiteral().IsSetSeq_data()) {
        ValidErr(eDiag_Error, eErr_SEQ_INST_BadDeltaSeq,
            "First delta seq component is a gap", seq);
    }
    bool last_is_gap = false;
    unsigned int num_adjacent_gaps = 0;
    unsigned int num_gaps = 0;
    for (++ds; ds; ++ds) {
        if ((*ds).IsLiteral()) {
            if (!(*ds).GetLiteral().IsSetSeq_data()) {
                if (last_is_gap) {
                    num_adjacent_gaps++;
                }
                last_is_gap = true;
                num_gaps++;
            } else {
                last_is_gap = false;
            }
        } else {
            last_is_gap = false;
        }
    }
    if (num_adjacent_gaps > 1) {
        ValidErr(eDiag_Error, eErr_SEQ_INST_BadDeltaSeq,
            "There is (are) " + NStr::IntToString(num_adjacent_gaps) +
            " adjacent gap(s) in delta seq", seq);
    }
    if (last_is_gap) {
        ValidErr(eDiag_Error, eErr_SEQ_INST_BadDeltaSeq,
            "Last delta seq component is a gap", seq);
    }
    if (num_gaps == 0  &&  mi) {
        if (tech == CMolInfo::eTech_htgs_1  ||
            tech == CMolInfo::eTech_htgs_2) {
            ValidErr(eDiag_Warning, eErr_SEQ_INST_BadDeltaSeq,
                "HTGS 1 or 2 delta seq has no gaps", seq);
        }
    }
}


void CValidError_impl::ValidatePubdesc
(const CPubdesc& pub, 
 const CSerialObject& obj)
{
}


const char* CValidError_impl::legalDbXrefs[] = {
    "PIDe", "PIDd", "PIDg", "PID",
    "AceView/WormGenes",
    "ATCC",
    "ATCC(in host)",
    "ATCC(dna)",
    "BDGP_EST",
    "BDGP_INS",
    "CDD",
    "CK",
    "COG",
    "dbEST",
    "dbSNP",
    "dbSTS",
    "ENSEMBL",
    "ESTLIB",
    "FANTOM_DB",
    "FLYBASE",
    "GABI",
    "GDB",
    "GeneID",
    "GI",
    "GO",
    "GOA",
    "IFO",
    "IMGT/LIGM",
    "IMGT/HLA",
    "InterimID",
    "ISFinder",
    "JCM",
    "LocusID",
    "MaizeDB",
    "MGD",
    "MGI",
    "MIM",
    "NextDB",
    "niaEST",
    "PIR",
    "PSEUDO",
    "RATMAP",
    "RiceGenes",
    "REMTREMBL",
    "RGD",
    "RZPD",
    "SGD",
    "SoyBase",
    "SPTREMBL",
    "SWISS-PROT",
    "taxon",
    "UniGene",
    "UniSTS",
    "WorfDB",
    "WormBase",
    0  // to indicate that there is no more data
};


const char* CValidError_impl::legalRefSeqDbXrefs[] = {
  "GenBank",
  "EMBL",
  "DDBJ",
    0  // to indicate that there is no more data
};



void CValidError_impl::ValidateDbxref
(const CDbtag&        xref,
 const CSerialObject& obj)
{
    const string& str = xref.GetDb ();
    for (size_t i = 0; legalDbXrefs [i];  i++) {
        if (NStr::Compare (str, legalDbXrefs [i]) == 0) {
            return;
        }
    }
    if (m_IsRefSeq) {
        for (size_t i = 0; legalDbXrefs [i];  i++) {
            if (NStr::Compare (str, legalRefSeqDbXrefs [i]) == 0) {
                return;
            }
        }
    }
    ValidErr (eDiag_Warning, eErr_SEQ_FEAT_IllegalDbXref,
             "Illegal db_xref type " + str, obj);
}


void CValidError_impl::ValidateDbxref (
    const list< CRef< CDbtag > > &xref_list,
    const CSerialObject& obj
)
{
    iterate( list< CRef< CDbtag > >, xref, xref_list) {
        ValidateDbxref(**xref, obj);
    }
}

// ************************** Official country names  ***********************

const string CValidError_impl::sm_CountryCodes [] = {
  "Afghanistan",
  "Albania",
  "Algeria",
  "American Samoa",
  "Andorra",
  "Angola",
  "Anguilla",
  "Antarctica",
  "Antigua and Barbuda",
  "Argentina",
  "Armenia",
  "Aruba",
  "Ashmore and Cartier Islands",
  "Australia",
  "Austria",
  "Azerbaijan",
  "Bahamas",
  "Bahrain",
  "Baker Island",
  "Bangladesh",
  "Barbados",
  "Bassas da India",
  "Belarus",
  "Belgium",
  "Belize",
  "Benin",
  "Bermuda",
  "Bhutan",
  "Bolivia",
  "Bosnia and Herzegovina",
  "Botswana",
  "Bouvet Island",
  "Brazil",
  "British Virgin Islands",
  "Brunei",
  "Bulgaria",
  "Burkina Faso",
  "Burundi",
  "Cambodia",
  "Cameroon",
  "Canada",
  "Cape Verde",
  "Cayman Islands",
  "Central African Republic",
  "Chad",
  "Chile",
  "China",
  "Christmas Island",
  "Clipperton Island",
  "Cocos Islands",
  "Colombia",
  "Comoros",
  "Cook Islands",
  "Coral Sea Islands",
  "Costa Rica",
  "Cote d'Ivoire",
  "Croatia",
  "Cuba",
  "Cyprus",
  "Czech Republic",
  "Democratic Republic of the Congo",
  "Denmark",
  "Djibouti",
  "Dominica",
  "Dominican Republic",
  "Ecuador",
  "Egypt",
  "El Salvador",
  "Equatorial Guinea",
  "Eritrea",
  "Estonia",
  "Ethiopia",
  "Europa Island",
  "Falkland Islands (Islas Malvinas)",
  "Faroe Islands",
  "Fiji",
  "Finland",
  "France",
  "French Guiana",
  "French Polynesia",
  "French Southern and Antarctic Lands",
  "Gabon",
  "Gambia",
  "Gaza Strip",
  "Georgia",
  "Germany",
  "Ghana",
  "Gibraltar",
  "Glorioso Islands",
  "Greece",
  "Greenland",
  "Grenada",
  "Guadeloupe",
  "Guam",
  "Guatemala",
  "Guernsey",
  "Guinea",
  "Guinea-Bissau",
  "Guyana",
  "Haiti",
  "Heard Island and McDonald Islands",
  "Honduras",
  "Hong Kong",
  "Howland Island",
  "Hungary",
  "Iceland",
  "India",
  "Indonesia",
  "Iran",
  "Iraq",
  "Ireland",
  "Isle of Man",
  "Israel",
  "Italy",
  "Jamaica",
  "Jan Mayen",
  "Japan",
  "Jarvis Island",
  "Jersey",
  "Johnston Atoll",
  "Jordan",
  "Juan de Nova Island",
  "Kazakhstan",
  "Kenya",
  "Kingman Reef",
  "Kiribati",
  "Kuwait",
  "Kyrgyzstan",
  "Laos",
  "Latvia",
  "Lebanon",
  "Lesotho",
  "Liberia",
  "Libya",
  "Liechtenstein",
  "Lithuania",
  "Luxembourg",
  "Macau",
  "Macedonia",
  "Madagascar",
  "Malawi",
  "Malaysia",
  "Maldives",
  "Mali",
  "Malta",
  "Marshall Islands",
  "Martinique",
  "Mauritania",
  "Mauritius",
  "Mayotte",
  "Mexico",
  "Micronesia",
  "Midway Islands",
  "Moldova",
  "Monaco",
  "Mongolia",
  "Montserrat",
  "Morocco",
  "Mozambique",
  "Myanmar",
  "Namibia",
  "Nauru",
  "Navassa Island",
  "Nepal",
  "Netherlands",
  "Netherlands Antilles",
  "New Caledonia",
  "New Zealand",
  "Nicaragua",
  "Niger",
  "Nigeria",
  "Niue",
  "Norfolk Island",
  "North Korea",
  "Northern Mariana Islands",
  "Norway",
  "Oman",
  "Pakistan",
  "Palau",
  "Palmyra Atoll",
  "Panama",
  "Papua New Guinea",
  "Paracel Islands",
  "Paraguay",
  "Peru",
  "Philippines",
  "Pitcairn Islands",
  "Poland",
  "Portugal",
  "Puerto Rico",
  "Qatar",
  "Republic of the Congo",
  "Reunion",
  "Romania",
  "Russia",
  "Rwanda",
  "Saint Helena",
  "Saint Kitts and Nevis",
  "Saint Lucia",
  "Saint Pierre and Miquelon",
  "Saint Vincent and the Grenadines",
  "Samoa",
  "San Marino",
  "Sao Tome and Principe",
  "Saudi Arabia",
  "Senegal",
  "Seychelles",
  "Sierra Leone",
  "Singapore",
  "Slovakia",
  "Slovenia",
  "Solomon Islands",
  "Somalia",
  "South Africa",
  "South Georgia and the South Sandwich Islands",
  "South Korea",
  "Spain",
  "Spratly Islands",
  "Sri Lanka",
  "Sudan",
  "Suriname",
  "Svalbard",
  "Swaziland",
  "Sweden",
  "Switzerland",
  "Syria",
  "Taiwan",
  "Tajikistan",
  "Tanzania",
  "Thailand",
  "Togo",
  "Tokelau",
  "Tonga",
  "Trinidad and Tobago",
  "Tromelin Island",
  "Tunisia",
  "Turkey",
  "Turkmenistan",
  "Turks and Caicos Islands",
  "Tuvalu",
  "Uganda",
  "Ukraine",
  "United Arab Emirates",
  "United Kingdom",
  "Uruguay",
  "USA",
  "Uzbekistan",
  "Vanuatu",
  "Venezuela",
  "Viet Nam",
  "Virgin Islands",
  "Wake Island",
  "Wallis and Futuna",
  "West Bank",
  "Western Sahara",
  "Yemen",
  "Yugoslavia",
  "Zambia",
  "Zimbabwe"
};


bool CValidError_impl::IsCountryValid(const string& str) {
  if ( str.empty() ) {
    return false;
  }

  string country_name = str;
  string::size_type pos = str.find(':');
  if ( pos != string::npos ) {
    country_name = str.substr(0, pos);
  }

  const string *begin = sm_CountryCodes;
  const string *end = &(sm_CountryCodes[sizeof (sm_CountryCodes) / sizeof (string)]);
  if ( find (begin, end, country_name) == end ) {
    return false;
  }

  return true;
}


void CValidError_impl::ValidateSourceQualTags(
    const string& str, 
    const CSerialObject& obj)
{
    if ( str.empty() ) return;

    size_t str_len = str.length();

    int state = m_SourceQualTags.GetInitialState();
    for ( size_t i = 0; i < str_len; ++i ) {
        state = m_SourceQualTags.GetNextState(state, str[i]);
        if ( m_SourceQualTags.IsMatchFound(state) ) {
            string match = m_SourceQualTags.GetMatches(state)[0];
            if ( match.empty() ) {
                match = "?";
            }

            bool okay = true;
            if ( i - str_len >= 0 ) {
                char ch = str[i - str_len];
                if ( !isspace(ch) || ch != ';' ) {
                    okay = false;
                }
            }
            if ( okay ) {
                ValidErr (eDiag_Warning, eErr_SEQ_DESCR_StructuredSourceNote,
                    "Source note has structured tag " + match, obj);
            }
        }
    }
}


void CValidError_impl::ValidateBioSource
(const CBioSource&    bsrc,
 const CSerialObject& obj)
{
	const COrg_ref& orgref = bsrc.GetOrg();
  
	// Organism must have a name.
	if ( orgref.GetTaxname().empty() && orgref.GetCommon().empty() ) {
		ValidErr(eDiag_Error, eErr_SEQ_DESCR_NoOrgFound,
			     "No organism name has been applied to this Bioseq.", obj);
	}

	// validate legal locations.
	if ( bsrc.GetGenome() == CBioSource_Base::eGenome_transposon  ||
		 bsrc.GetGenome() == CBioSource_Base::eGenome_insertion_seq ) {
		ValidErr(eDiag_Warning, eErr_SEQ_DESCR_ObsoleteSourceLocation,
			     "Transposon and insertion sequence are no longer legal locations.", obj);
	}

	int chrom_count = 0;
	bool chrom_conflict = false;
    const CSubSource *chromosome = 0;
	string countryname;
	iterate( CBioSource::TSubtype, ssit, bsrc.GetSubtype() ) {
		switch ( (**ssit).GetSubtype() ) {

		case CSubSource::eSubtype_country:
			countryname = (**ssit).GetName();
			if ( !IsCountryValid(countryname) ) {
				if ( countryname.empty() ) {
					countryname = "?";
				}
				ValidErr(eDiag_Warning, eErr_SEQ_DESCR_BadCountryCode,
						 "Bad country name " + countryname, obj);
			}
			break;

		case CSubSource::eSubtype_chromosome:
			++chrom_count;
			if ( chromosome != 0 ) {
				if ( NStr::CompareNocase((**ssit).GetName(), chromosome->GetName()) != 0) {
					chrom_conflict = true;
				}          
			} else {
				chromosome = ssit->GetPointer();
			}
			break;

		case CSubSource::eSubtype_transposon_name:
		case CSubSource::eSubtype_insertion_seq_name:
			ValidErr(eDiag_Warning, eErr_SEQ_DESCR_ObsoleteSourceQual,
					 "Transposon name and insertion sequence name are no longer legal qualifiers.", obj);
		break;

		case 0:
			ValidErr(eDiag_Warning, eErr_SEQ_DESCR_BadSubSource,
               "Unknown subsource subtype 0.", obj);
			break;
    
		case CSubSource::eSubtype_other:
			ValidateSourceQualTags((**ssit).GetName(), obj);
			break;
		}
	}
	if ( chrom_count > 1 ) {
		string msg = chrom_conflict ? "Multiple conflicting chromosome qualifiers" :
									  "Multiple identical chromosome qualifiers";
		ValidErr(eDiag_Warning, eErr_SEQ_DESCR_MultipleChromosomes, msg, obj);
	}

    if ( !orgref.IsSetOrgname()  ||
         orgref.GetOrgname().GetLineage().empty() ) {
		ValidErr(eDiag_Error, eErr_SEQ_DESCR_MissingLineage, 
			     "No lineage for this BioSource.", obj);
	} else {
        const string& lineage = orgref.GetOrgname().GetLineage();
		if ( bsrc.GetGenome() == CBioSource_Base::eGenome_kinetoplast ) {
			if ( lineage.find("Kinetoplastida") == string::npos ) {
				ValidErr(eDiag_Warning, eErr_SEQ_DESCR_BadOrganelle, 
						 "Only Kinetoplastida have kinetoplasts", obj);
			}
		} 
		if ( bsrc.GetGenome() == CBioSource_Base::eGenome_nucleomorph ) {
			if ( lineage.find("Chlorarchniophyta") == string::npos  &&
				lineage.find("Cryptophyta") == string::npos) {
				ValidErr(eDiag_Warning, eErr_SEQ_DESCR_BadOrganelle, 
						 "Only Chlorarchniophyta and Cryptophyta have nucleomorphs", obj);
			}
		}
	}
    if ( !orgref.IsSetOrgname() ) {
        return;
    }
    const COrgName& orgname = orgref.GetOrgname();

	iterate ( COrgName::TMod, omit, orgname.GetMod() ) {
		int subtype = (**omit).GetSubtype();

		if ( (subtype == 0) || (subtype == 1) ) {
			ValidErr(eDiag_Warning, eErr_SEQ_DESCR_BadOrgMod, 
				     "Unknown orgmod subtype " + subtype, obj);
		}
		if ( subtype == COrgMod::eSubtype_variety ) {
			if ( NStr::CompareNocase( orgname.GetDiv(), "PLN" ) != 0 ) {
				ValidErr(eDiag_Warning, eErr_SEQ_DESCR_BadOrgMod, 
						 "Orgmod variety should only be in plants or fungi", obj);
			}
		}
		if ( subtype == COrgMod::eSubtype_other ) {
			ValidateSourceQualTags( (**omit).GetSubname(), obj);
		}
	}

    if (orgref.IsSetDb ()) {
        ValidateDbxref(orgref.GetDb(), obj);
    }
}


void CValidError_impl::ValidateGene(const CGene_ref& gene, const CSerialObject& obj)
{
    if ( gene.GetLocus().empty()      &&
         gene.GetAllele().empty()     &&
         gene.GetDesc().empty()       &&
         gene.GetMaploc().empty()     &&
         gene.GetDb().empty()         &&
         gene.GetSyn().empty()        &&
         gene.GetLocus_tag().empty()  ) {
        ValidErr(eDiag_Warning, eErr_SEQ_FEAT_GeneRefHasNoData,
                  "There is a gene feature where all fields are empty", obj);
    }
    if (gene.IsSetDb ()) {
        ValidateDbxref(gene.GetDb(), obj);
    }
}


void CValidError_impl::ValidateProt(const CProt_ref& prot, const CSerialObject& obj) 
{
    if ( prot.GetProcessed() != CProt_ref::eProcessed_signal_peptide  &&
         prot.GetProcessed() != CProt_ref::eProcessed_transit_peptide ) {
        if ( (prot.GetName().empty()  ||  prot.GetName().front().empty())  &&
             prot.GetDesc().empty()  &&  prot.GetEc().empty()  &&
             prot.GetActivity().empty()  &&  prot.GetDb().empty() ) {
            ValidErr(eDiag_Warning, eErr_SEQ_FEAT_ProtRefHasNoData,
                  "There is a protein feature where all fields are empty", obj);
        }
    }
    if (prot.IsSetDb ()) {
        ValidateDbxref(prot.GetDb(), obj);
    }
}


void CValidError_impl::ValidateRna(const CRNA_ref& rna, const CSeq_feat& feat) 
{
    const CRNA_ref::EType& rna_type = rna.GetType ();

    if ( rna_type == CRNA_ref::eType_mRNA ) {
        // !!! 
    }

    if ( rna.GetExt ().Which() == CRNA_ref::C_Ext::e_TRNA ) {
        const CTrna_ext& trna = rna.GetExt ().GetTRNA ();
        if ( trna.IsSetAnticodon () ) {
            ECompare comp = Compare( trna.GetAnticodon (), feat.GetLocation () );
            if ( comp != eContained  &&  comp != eSame ) {
                ValidErr (eDiag_Error, eErr_SEQ_FEAT_Range,
                    "Anticodon location not in tRNA", feat);
            }
            if ( GetLength( trna.GetAnticodon (), m_Scope ) != 3 ) {
                ValidErr (eDiag_Warning, eErr_SEQ_FEAT_Range,
                    "Anticodon is not 3 bases in length", feat);
            }
        }
        CheckTrnaCodons(trna, feat);
    }

    if ( rna_type == CRNA_ref::eType_tRNA ) {
        iterate ( list< CRef< CGb_qual > >, gbqual, feat.GetQual () ) {
            if ( NStr::CompareNocase((**gbqual).GetVal (), "anticodon") == 0 ) {
                ValidErr (eDiag_Error, eErr_SEQ_FEAT_InvalidQualifierValue,
                    "Unparsed anticodon qualifier in tRNA", feat);
                break;
            }
        }
        /* tRNA with string extension */
        if ( rna.GetExt ().Which () == CRNA_ref::C_Ext::e_Name ) {
            ValidErr (eDiag_Error, eErr_SEQ_FEAT_InvalidQualifierValue,
                    "Unparsed product qualifier in tRNA", feat);
        }
    }

    if ( rna_type == CRNA_ref::eType_unknown ) {
        ValidErr (eDiag_Warning, eErr_SEQ_FEAT_RNAtype0,
                    "RNA type 0 (unknown) not supported", feat);
    }

}


static bool s_IsPlastid(int genome)
{
    if ( genome == CBioSource::eGenome_chloroplast  ||
         genome == CBioSource::eGenome_chromoplast  ||
         genome == CBioSource::eGenome_plastid      ||
         genome == CBioSource::eGenome_cyanelle     ||
         genome == CBioSource::eGenome_apicoplast   ||
         genome == CBioSource::eGenome_leucoplast   ||
         genome == CBioSource::eGenome_proplastid  ) { 
        return true;
    }

    return false;
}


void CValidError_impl::ValidateCdregion (
    const CCdregion& cdregion, 
    const CSeq_feat& feat
) 
{
    iterate( list< CRef< CGb_qual > >, qual, feat.GetQual () ) {
        if ( (**qual).GetQual() == "codon" ) {
            ValidErr (eDiag_Warning, eErr_SEQ_FEAT_WrongQualOnImpFeat,
                "Use the proper genetic code, if available, or set transl_excepts on specific codons", feat);
            break;
        }
    }

    bool pseudo = OverlappingGeneIsPseudo(feat);
    if ( !pseudo && !cdregion.GetConflict () ) {
        CdTransCheck (feat);
        SpliceCheck (feat);
    }

    iterate( list< CRef< CCode_break > >, codebreak, cdregion.GetCode_break () ) {
        ECompare comp = Compare ((**codebreak).GetLoc (), feat.GetLocation (), m_Scope );
        if ( (comp != eContained) && (comp != eSame))
            ValidErr (eDiag_Error, eErr_SEQ_FEAT_Range, 
                "Code-break location not in coding region", feat);
    }

    if ( cdregion.GetOrf () && feat.IsSetProduct () ) {
        ValidErr (eDiag_Warning, eErr_SEQ_FEAT_OrfCdsHasProduct,
            "An ORF coding region should not have a product", feat);
    }

    if ( pseudo && feat.IsSetProduct () ) {
        ValidErr (eDiag_Warning, eErr_SEQ_FEAT_PsuedoCdsHasProduct,
            "A pseudo coding region should not have a product", feat);
    }
    
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle (feat.GetLocation ());
    CSeqdesc_CI diter (bsh, CSeqdesc::e_Source);
    if ( diter ) {
        const CBioSource& src = diter->GetSource();
        
        int biopgencode = src.GetGenCode();
        
        if (cdregion.IsSetCode ()) {
            int cdsgencode = cdregion.GetCode().GetId();
            
            if ( biopgencode != cdsgencode ) {
                int genome = 0;
                
                if ( src.IsSetGenome() ) {
                    genome = src.GetGenome();
                }

                if ( s_IsPlastid(genome) ) {
                    ValidErr (eDiag_Warning, eErr_SEQ_FEAT_GenCodeMismatch,
                        "Genetic code conflict between CDS (code " +
                        NStr::IntToString (cdsgencode) +
                        ") and BioSource.genome biological context (" +
                        sm_PlastidTxt [genome] + ") (uses code 11)", feat);
                } else {
                    ValidErr (eDiag_Warning, eErr_SEQ_FEAT_GenCodeMismatch,
                        "Genetic code conflict between CDS (code " +
                        NStr::IntToString (cdsgencode) +
                        ") and BioSource (code " +
                        NStr::IntToString (biopgencode) + ")", feat);
                }
            }
        }
    }

    CheckForBothStrands (feat);
    CheckForBadGeneOverlap (feat);
    CheckForBadMRNAOverlap (feat);
    CheckForCommonCDSProduct (feat);
}


static string s_LegalRepeatTypes[] = {
  "tandem", "inverted", "flanking", "terminal",
  "direct", "dispersed", "other"
};


static string s_LegalConsSpliceStrings[] = {
  "(5'site:YES, 3'site:YES)",
  "(5'site:YES, 3'site:NO)",
  "(5'site:YES, 3'site:ABSENT)",
  "(5'site:NO, 3'site:YES)",
  "(5'site:NO, 3'site:NO)",
  "(5'site:NO, 3'site:ABSENT)",
  "(5'site:ABSENT, 3'site:YES)",
  "(5'site:ABSENT, 3'site:NO)",
  "(5'site:ABSENT, 3'site:ABSENT)",
};


void CValidError_impl::ValidateImpGbquals
(const CImp_feat& imp,
 const CSeq_feat& feat)
{
    CSeqFeatData::ESubtype ftype = feat.GetData().GetSubtype();
    const string& key = imp.GetKey();

    iterate( CSeq_feat::TQual, qual, feat.GetQual() ) {
        const string& qual_str = (*qual)->GetQual();

        if ( qual_str == "gsdb_id" ) {
            continue;
        }

        CGbqualType::EType gbqual = CGbqualType::GetType(qual_str);
        
        if ( gbqual == CGbqualType::e_Bad ) {
            if ( !qual_str.empty() ) {
                ValidErr(eDiag_Warning, eErr_SEQ_FEAT_UnknownImpFeatQual,
                    "Unknown qualifier " + qual_str, feat);
            } else {
                ValidErr(eDiag_Warning, eErr_SEQ_FEAT_UnknownImpFeatQual,
                    "Empty qualifier", feat);
            }
        } else {
            if ( !CFeatQualAssoc::IsLegalGbqual(ftype, gbqual) ) {
                ValidErr(eDiag_Warning, eErr_SEQ_FEAT_WrongQualOnImpFeat,
                    "Wrong qualifier " + qual_str + " for feature " + 
                    key, feat);
            }

            string val = (*qual)->GetVal();

            bool error = false;
            switch ( gbqual ) {
            case CGbqualType::e_Rpt_type:
                for ( size_t i = 0; 
                      i < sizeof(s_LegalRepeatTypes) / sizeof(string); 
                      ++i ) {
                    if ( val.find(s_LegalRepeatTypes[i]) != string::npos ) {
                        bool left = false, right = false;
                        if ( i > 0 ) {
                            left = val[i-1] == ','  ||  val[i-1] == '(';
                        }
                        if ( i < val.length() - 1 ) {
                            right = val[i+1] == ','  ||  val[i+1] == ')';
                        }
                        if ( left  &&  right ) {
                            error = true;
                        }
                        break;
                    }
                }
                break;

            case CGbqualType::e_Rpt_unit:
                {
                    bool found = false,
                         multiple_rpt_unit = true;

                    for ( size_t i = 0; i < val.length(); ++i ) {
                        if ( val[i] <= ' ' ) {
                            found = true;
                        } else if ( val[i] == '('  ||  val[i] == ')'  ||
                                    val[i] == ','  ||  val[i] == '.'  ||
                                    isdigit(val[i]) ) {
                        } else {
                            multiple_rpt_unit = false;
                        }
                    }
                    if ( found || 
                         (!multiple_rpt_unit && val.length() > 48) ) {
                        error = true;
                    }
                    break;
                }
                
            case CGbqualType::e_Label:
                {
                    for ( size_t i = 0; i < val.length(); ++i ) {
                        if ( isspace(val[i])  ||  isdigit(val[i]) ) {
                            error = true;
                            break;
                        }
                    }
                }                
                break;
                
            case CGbqualType::e_Cons_splice:
                { 
                    error = true;
                    for ( size_t i = 0; 
                          i < sizeof(s_LegalConsSpliceStrings) / sizeof(string); 
                          ++i ) {
                        if ( NStr::CompareNocase(val, s_LegalConsSpliceStrings[i]) == 0 ) {
                            error = false;
                            break;
                        }
                    }
                }
                break;
            }
            if ( error ) {
                ValidErr(eDiag_Error, eErr_SEQ_FEAT_InvalidQualifierValue,
                    val + " is not a legal value for qualifier " + qual_str, feat);
            }
        }
    }
}


void CValidError_impl::ValidateImp(const CImp_feat& imp, const CSeq_feat& feat)
{
    CSeqFeatData::ESubtype subtype = feat.GetData().GetSubtype();
    string key = imp.GetKey();

    switch ( subtype ) {
    case CSeqFeatData::eSubtype_exon:
        if ( m_ValidateExons ) {
            SpliceCheckEx(feat, true);
        }
        break;

    case CSeqFeatData::eSubtype_bad:
        ValidErr(eDiag_Warning, eErr_SEQ_FEAT_UnknownImpFeatKey, 
            "Unknown feature key " + key, feat);
        break;

    case CSeqFeatData::eSubtype_virion:
    case CSeqFeatData::eSubtype_mutation:
    case CSeqFeatData::eSubtype_allele:
        ValidErr(eDiag_Error, eErr_SEQ_FEAT_UnknownImpFeatKey,
            "Feature key " + key + " is no longer legal", feat);
        break;

    case CSeqFeatData::eSubtype_polyA_site:
        {
            CSeq_loc::TRange range = feat.GetLocation().GetTotalRange();
            if ( range.GetFrom() != range.GetTo() ) {
                ValidErr(eDiag_Warning, eErr_SEQ_FEAT_PolyAsiteNotPoint,
                    "PolyA_site should be a single point", feat);
            }
        }
        break;

    case CSeqFeatData::eSubtype_mat_peptide:
    case CSeqFeatData::eSubtype_sig_peptide:
    case CSeqFeatData::eSubtype_transit_peptide:
        ValidErr(eDiag_Warning, eErr_SEQ_FEAT_InvalidForType,
            "Peptide processing feature should be converted to the "
            "appropriate protein feature subtype", feat);
        // !!! CheckPeptideOnCodonBoundary (???); requires overlapping CDS
        break;
        
    case CSeqFeatData::eSubtype_mRNA:
    case CSeqFeatData::eSubtype_tRNA:
    case CSeqFeatData::eSubtype_rRNA:
    case CSeqFeatData::eSubtype_snRNA:
    case CSeqFeatData::eSubtype_scRNA:
    case CSeqFeatData::eSubtype_snoRNA:
    case CSeqFeatData::eSubtype_misc_RNA:
    case CSeqFeatData::eSubtype_precursor_RNA:
    // !!! what about other RNA types (preRNA, otherRNA)?
        ValidErr(eDiag_Error, eErr_SEQ_FEAT_InvalidForType,
              "RNA feature should be converted to the appropriate RNA feature "
              "subtype, location should be converted manually", feat);
        break;

    case CSeqFeatData::eSubtype_Imp_CDS:
        {
            // impfeat CDS must be pseudo; fail if not
            bool pseudo = false;
            // !!! check for pseudo requires overlapping gene - not yet implemented
            
            iterate( CSeq_feat::TQual, gbqual, feat.GetQual() ) {
                if ( NStr::CompareNocase( (*gbqual)->GetQual(), "translation") == 0 ) {
                    ValidErr(eDiag_Error, eErr_SEQ_FEAT_ImpCDShasTranslation,
                        "ImpFeat CDS with /translation found", feat);
                }
            }
            if ( !pseudo ) {
                ValidErr(eDiag_Info, eErr_SEQ_FEAT_ImpCDSnotPseudo,
                    "ImpFeat CDS should be pseudo", feat);
            }
        }
        break;
    }// end of switch statement  
    
    // validate the feature's location
    if ( imp.IsSetLoc() ) {
        const string& imp_loc = imp.GetLoc();
        if ( imp_loc.find("one-of") != string::npos ) {
            ValidErr(eDiag_Error, eErr_SEQ_FEAT_ImpFeatBadLoc,
                "ImpFeat loc " + imp_loc + 
                " has obsolete 'one-of' text for feature " + key, feat);
        } else if ( feat.GetLocation().IsInt() ) {
            const CSeq_interval& seq_int = feat.GetLocation().GetInt();
            string temp_loc = NStr::IntToString(seq_int.GetFrom() + 1) +
                ".." + NStr::IntToString(seq_int.GetTo() + 1);
            if ( imp_loc != temp_loc ) {
                ValidErr(eDiag_Error, eErr_SEQ_FEAT_ImpFeatBadLoc,
                    "ImpFeat loc " + imp_loc + " does not equal feature location " +
                    temp_loc + "for feature " + key, feat);
            }
        }
    }

    if ( feat.IsSetQual() ) {
        ValidateImpGbquals(imp, feat);

        // Make sure a feature has its mandatory qualifiers
        iterate( CFeatQualAssoc::GBQualTypeVec,
                 required,
                 CFeatQualAssoc::GetMandatoryGbquals(subtype) ) {
            bool found = false;
            iterate( CSeq_feat::TQual, qual, feat.GetQual() ) {
                if ( CGbqualType::GetType(**qual) == *required ) {
                    found = true;
                    break;
                }
            }
            if ( !found ) {
                ValidErr(eDiag_Warning, eErr_SEQ_FEAT_MissingQualOnImpFeat,
                    "Missing qualifier " + CGbqualType::GetString(*required) +
                    " for feature " + key, feat);
            }
        }
    }
}


bool CValidError_impl::OverlappingGeneIsPseudo (const CSeq_feat& feat)
{
    // !!!
    return false;
}


void CValidError_impl::CheckTrnaCodons(const CTrna_ext& trna, const CSeq_feat& feat)
{
    // Make sure AA coding is ncbieaa.
    if ( trna.GetAa().Which() != CTrna_ext::C_Aa::e_Ncbieaa ) {
        ValidErr (eDiag_Error, eErr_SEQ_FEAT_TrnaCodonWrong,
            "tRNA AA coding is not match ncbieaa", feat);
        return;
    }
    
    // Retrive the Genetic code id for the tRna
    int gcode = 0;
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(feat.GetLocation());
    for ( CSeqdesc_CI diter (bsh, CSeqdesc::e_Source); diter; ++diter) {
        gcode = diter->GetSource().GetGenCode();
        break; // need only the closest biosoure.
    }
    
    const string& ncbieaa = CGen_code_table::GetNcbieaa(gcode);
    if ( ncbieaa.length() != 64 ) {
        return;  // !!!  need to issue a warning/error?
    }
    
    iterate( CTrna_ext::TCodon, iter, trna.GetCodon() ) {
        if ( *iter < 64 ) {  // 0-63 = codon,  255=no data in cell
            unsigned char taa = ncbieaa[*iter];
            unsigned char  aa = trna.GetAa().GetNcbieaa();
            if ( (aa > 0)  &&  (aa != 255) ) {
                if ( taa != aa ) {
                    EDiagSev sev = (aa == 'U') ? eDiag_Warning : eDiag_Error;
                    ValidErr (sev, eErr_SEQ_FEAT_TrnaCodonWrong,
                        "tRNA codon does not match genetic code", feat);
                }
            }
        }
    }
}


void CValidError_impl::CheckForCommonCDSProduct(const CSeq_feat& feat)
{
    // !!!
}


void CValidError_impl::CheckForBadMRNAOverlap(const CSeq_feat& feat)
{
    // !!!
}

void CValidError_impl::CheckForBadGeneOverlap(const CSeq_feat& feat)
{
    // !!!
}


static bool s_IsResidue(unsigned char res) 
{
    return res < 250;
}


void CValidError_impl::SpliceCheckEx(const CSeq_feat& feat, bool check_all)
{
    // !!! suppress if NCBISubValidate
    //if (GetAppProperty ("NcbiSubutilValidation") != NULL)
    //    return;

    // specific biological exceptions suppress check
    if ( feat.GetExcept() && feat.IsSetExcept_text() ) {
        const string& except_text = feat.GetExcept_text();
        if ( SearchNoCase(except_text, "ribosomal slippage") != string::npos        ||
             SearchNoCase(except_text, "ribosome slippage") != string::npos         ||
             SearchNoCase(except_text, "artificial frameshift") != string::npos     ||
             SearchNoCase(except_text, "non-consensus splice site") != string::npos ||
             SearchNoCase(except_text, "nonconsensus splice site") != string::npos) {
            return;
        }
    }

    size_t num_of_parts = 0;
    ENa_strand  strand = eNa_strand_unknown;

    // !!! The C version treated seq_loc equiv as one whereas the iterator
    // treats it as many. 
    const CSeq_loc& location = feat.GetLocation ();

    for (CSeq_loc_CI citer(location); citer; ++citer) {
        ++num_of_parts;
        if ( num_of_parts == 1 ) {  // first part
            strand = citer.GetStrand();
        } else {
            if ( strand != citer.GetStrand() ) {
                return;         //bail on mixed strand
            }
        }
    }

    if ( num_of_parts == 0 ) {
        return;
    }
    if ( !check_all  &&  num_of_parts == 1 ) {
        return;
    }
    
    bool partial_first = location.IsPartialLeft();
    bool partial_last = location.IsPartialRight();

    size_t counter = 0;
    const CSeq_id* last_id = 0;

    CBioseq_Handle bsh;
    for (CSeq_loc_CI citer(location); citer; ++citer) {
        ++counter;

        const CSeq_id& seq_id = citer.GetSeq_id();
        
        size_t         seq_len = 0;
        if ( last_id == 0 || !last_id->Match(seq_id) ) {
            bsh = m_Scope->GetBioseqHandle(seq_id);
            seq_len = bsh.GetSeqVector().size();
            last_id = &seq_id;
        }

        TSeqPos acceptor = citer.GetRange().GetFrom();
        TSeqPos donor = citer.GetRange().GetTo();
        TSeqPos start = acceptor;
        TSeqPos stop = donor;

        if ( citer.GetStrand() == eNa_strand_minus ) {
            swap(acceptor, donor);
            // CSeqVector uses start and stop based on the strand.
        }


        // set severity level
        // genomic product set or NT_ contig always relaxes to SEV_WARNING
        EDiagSev sev = eDiag_Warning;
        if ( m_SpliceErr && !(m_IsGPS || m_IsRefSeq) && !check_all ) {
            sev = eDiag_Error;
        }

        // get the label. if m_SuppressContext flag in true, get the worst label.
        const CBioseq& bsq = bsh.GetBioseq();
        string label;
        bsq.GetLabel(&label, CBioseq::eContent, m_SuppressContext);
        
        CSeqVector seq_vec = bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
        // check donor on all but last exon and on sequence
        if ( ((check_all && !partial_last)  ||  counter < num_of_parts)  &&
             (stop < seq_len - 2) ) {
            CSeqVector::TResidue res1 = seq_vec[stop + 1];    
            CSeqVector::TResidue res2 = seq_vec[stop + 2];

            if ( s_IsResidue(res1)  &&  s_IsResidue(res2) ) {
                if ( (res1 != 'G')  || (res2 != 'T' ) ) {
                    string msg;
                    if ( (res1 == 'G')  && (res2 != 'C' ) ) { // GC minor splice site
                        sev = eDiag_Warning;
                        msg = "Rare splice donor consensus (GC) found instead of "
                              "(GT) after exon ending at position " +
                              NStr::IntToString(donor + 1) + " of " + label;
                    } else {
                        msg = "Splice donor consensus (GT) not found after exon"
                            " ending at position " + 
                            NStr::IntToString(donor + 1) + " of " + label;
                    }
                    ValidErr(sev, eErr_SEQ_FEAT_NotSpliceConsensus, msg, feat);
                }
            }
        }

        if ( ((check_all && !partial_first)  ||  counter != 1)  &&
             (start > 1) ) {
            CSeqVector::TResidue res1 = seq_vec[start - 2];
            CSeqVector::TResidue res2 = seq_vec[start - 1];

            if ( s_IsResidue(res1)  &&  s_IsResidue(res2) ) {
                if ( (res1 != 'A')  ||  (res2 != 'G') ) {
                    ValidErr(sev, eErr_SEQ_FEAT_NotSpliceConsensus,
                        "Splice acceptor consensus (AG) not found before "
                        "exon starting at position " + 
                        NStr::IntToString(acceptor + 1) + " of " + label, feat);
                }
            }
        }
    } // end of for loop
}


void CValidError_impl::SpliceCheck (const CSeq_feat& feat)
{
    SpliceCheckEx(feat, false);
}

static bool s_FoundBioseqSetClass(const CSeq_entry& se,  CBioseq_set::EClass clss)
{
    for ( CTypeConstIterator <CBioseq_set> si(se); si; ++si ) {
        if ( si->GetClass() == clss ) {
            return true;
        }
    }
    return false;
}


bool CValidError_impl::IsDeltaOrFarSeg(const CSeq_loc& loc)
{
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(loc);
    const CSeq_entry& se = bsh.GetTopLevelSeqEntry();
    const CBioseq& bsq = bsh.GetBioseq();

    if ( bsq.GetInst().GetRepr() == CSeq_inst::eRepr_delta ) {
        if ( !s_FoundBioseqSetClass(se, CBioseq_set::eClass_nuc_prot) ) {
            return true;
        }
    }
    if ( bsq.GetInst().GetRepr() == CSeq_inst::eRepr_seg ) {
        if ( !s_FoundBioseqSetClass(se, CBioseq_set::eClass_parts) ) {
            return true;
        }
    }
    
    return false;
}


void CValidError_impl::ReportCdTransErrors
(const CSeq_feat& feat,
 bool show_stop,
 bool got_stop, 
 bool no_end,
 int ragged)
{
    if ( show_stop ) {
        if ( !got_stop  && !no_end ) {
            ValidErr(eDiag_Error, eErr_SEQ_FEAT_NoStop, 
                "Missing stop codon", feat);
        } else if ( got_stop  &&  no_end ) {
            ValidErr (eDiag_Error, eErr_SEQ_FEAT_PartialProblem,
                "Got stop codon, but 3'end is labeled partial", feat);
        } else if ( got_stop  &&  !no_end  &&  ragged ) {
            ValidErr (eDiag_Error, eErr_SEQ_FEAT_TransLen, 
                "Coding region extends " + NStr::IntToString(ragged) +
                " base(s) past stop codon", feat);
        }
    }
}


int CValidError_impl::CheckForRaggedEnd
(const CSeq_loc& loc,
 const CCdregion& cdregion)
{
    size_t len = GetLength(loc, m_Scope);
    if ( cdregion.GetFrame() > CCdregion::eFrame_one ) {
        len -= cdregion.GetFrame() - 1;
    }

    int ragged = len % 3;
    if ( ragged > 0 ) {
        len = GetLength(loc, m_Scope);

        CSeq_loc::TRange range = CSeq_loc::TRange::GetEmpty();
        iterate( CCdregion::TCode_break, cbr, cdregion.GetCode_break() ) {
            SRelLoc rl(loc, (*cbr)->GetLoc(), m_Scope);
            CRef<CSeq_loc> rel_loc = rl.Resolve(m_Scope);
            range += rel_loc->GetTotalRange();
        }

        // allowing a partial codon at the end
        TSeqPos codon_length = range.GetLength();
        if ( (codon_length == 0 || codon_length == 1)  && 
            range.GetTo() == len - 1 ) {
            ragged = 0;
        }
    }
    return ragged;
}


void CValidError_impl::CheckForCodeBreakNotOnCodon
(const CSeq_feat& feat,
 const CSeq_loc& loc, 
 const CCdregion& cdregion)
{
    TSeqPos len = GetLength(loc, m_Scope);

    iterate( CCdregion::TCode_break, cbr, cdregion.GetCode_break() ) {
        size_t codon_length = GetLength((*cbr)->GetLoc(), m_Scope);
        TSeqPos from = LocationOffset(loc, (*cbr)->GetLoc(), 
            eOffset_FromStart, m_Scope);
        TSeqPos to = from + codon_length - 1;
        
        // check for code break not on a codon
        if ( codon_length == 3  ||
            ((codon_length == 1 || codon_length == 2)  && 
            to == len - 1) ) {
            size_t start_pos;
            switch ( cdregion.GetFrame() ) {
            case CCdregion::eFrame_two:
                start_pos = 1;
                break;
            case CCdregion::eFrame_three:
                start_pos = 2;
                break;
            default:
                start_pos = 0;
                break;
            }
            if ( (from % 3) != start_pos ) {
                ValidErr(eDiag_Warning, eErr_SEQ_FEAT_TranslExceptPhase,
                    "transl_except qual out of frame.", feat);
            }
        }
    }
}


// Map 'pos' on 'product' to a position on the feature
static string s_MapToNTCoords
(const CSeq_feat& feat,
 const CSeq_loc& product,
 TSeqPos pos, 
 CScope* scope)
{
    string result;

    CSeq_point pnt;
    pnt.SetPoint(pos);
    pnt.SetStrand( GetStrand(product, scope) );

    try {
        pnt.SetId().Assign(GetId(product, scope));
    } catch (CNotUnique) {}

    CSeq_loc tmp;
    tmp.SetPnt(pnt);
    CRef<CSeq_loc> loc = ProductToSource(feat, tmp, 0, scope);
    
    loc->GetLabel(&result);

    return result;
}


inline
static char s_Residue(unsigned char res)
{
    return res == 255 ? '?' : res;
}


void CValidError_impl::CdTransCheck(const CSeq_feat& feat)
{
    bool prot_ok = true;
    int  ragged = 0;

    // biological exception
    size_t except_num = sizeof(sm_BypassCdsTransCheck) / sizeof(string);
    if ( feat.GetExcept() && feat.IsSetExcept_text() ) {
        for ( size_t i = 0; i < except_num; ++i ) {
            if ( SearchNoCase(feat.GetExcept_text(), 
                sm_BypassCdsTransCheck[i]) != string::npos ) {
                return; 
            }
        }
    }
    
    // pseuogene
    if ( feat.GetPseudo() ) { // !!! need to check for overlapping too.
        return;
    }

    if ( !feat.IsSetProduct() ) {
        // bail if no product exists. shuold be checked elsewhere.
        return;
    }

    const CCdregion& cdregion = feat.GetData().GetCdregion();
    const CSeq_loc& location = feat.GetLocation();
    const CSeq_loc& product = feat.GetProduct();
    
    int gc = 0;
    if ( cdregion.IsSetCode() ) {
        // We assume that the id is set for all Genetic_code
        gc = cdregion.GetCode().GetId();
    }
    string gccode = NStr::IntToString(gc);

    string transl_prot;   // translated protein
    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(location);
    CCdregion_translate::TranslateCdregion(
        transl_prot, 
        bsh, 
        location, 
        cdregion, 
        true,   // include stop codons
        false); // do not remove trailing X/B/Z
    
    
    if ( transl_prot.empty() ) {
        ValidErr (eDiag_Error, eErr_SEQ_FEAT_CdTransFail, 
            "Unable to translate", feat);
        prot_ok = false;
        return;
    }
    
    bool no_end = false;
    unsigned int part_loc = s_GetSeqlocPartialInfo(location, m_Scope);
    unsigned int part_prod = s_GetSeqlocPartialInfo(product, m_Scope);
    if ( (part_loc & eSeqlocPartial_Stop)  ||
        (part_prod & eSeqlocPartial_Stop) ) {
        no_end = true;
    } else {    
        // complete stop, so check for ragged end
        ragged = CheckForRaggedEnd(location, cdregion);
    }
    
    // check for code break not on a codon
    CheckForCodeBreakNotOnCodon(feat, location, cdregion);
    
    if ( cdregion.GetFrame() > CCdregion::eFrame_one ) {
        EDiagSev sev = m_IsRefSeq ? eDiag_Error : eDiag_Warning;
        if ( !(part_loc & eSeqlocPartial_Start) ) {
            ValidErr(sev, eErr_SEQ_FEAT_PartialProblem, 
                "Suspicious CDS location - frame > 1 but not 5' partial", feat);
        } else if ( part_loc & eSeqlocPartial_Nostart ) { 
            //  && (!PartialAtSpliceSite (sfp->location, SLP_NOSTART)) !!!
            ValidErr (sev, eErr_SEQ_FEAT_PartialProblem, 
                "Suspicious CDS location - frame > 1 and not at consensus splice site",
                feat);
        }
    }
    
    bool no_beg = false;
    
    size_t stop_count = 0;
    if ( (part_loc & eSeqlocPartial_Start)  ||
         (part_prod & eSeqlocPartial_Start) ) {
        no_beg = true;
    }
    
    bool got_dash = (transl_prot[0] == '-');
    bool got_stop = (transl_prot[transl_prot.length() - 1] == '*');
    
    // count internal stops
    iterate( string, it, transl_prot ) {
        if ( *it == '*' ) {
            ++stop_count;
        }
    }
    if ( got_stop ) {
        --stop_count;
    }
    
    if ( stop_count > 0 ) {
        if ( got_dash ) {
            ValidErr(eDiag_Error, eErr_SEQ_FEAT_StartCodon,
                "Illegal start codon and " + 
                NStr::IntToString(stop_count) +
                " internal stops. Probably wrong genetic code [" + gccode + "]",
                feat);
        } else {
            ValidErr(eDiag_Error, eErr_SEQ_FEAT_InternalStop, 
                NStr::IntToString(stop_count) + 
                " internal stops. Genetic code [" + gccode + "]", feat);
            prot_ok = false;
            if ( stop_count > 5 ) {
                return;
            }
        }
    } else if ( got_dash ) {
        ValidErr(eDiag_Error, eErr_SEQ_FEAT_StartCodon, 
            "Illegal start codon used. Wrong genetic code [" +
            gccode + "] or protein should be partial", feat);
    }
    
    bool show_stop = true;

    const CSeq_id& protid = GetId(product, m_Scope);
    bsh = m_Scope->GetBioseqHandle(protid);

    CSeqVector prot_vec = bsh.GetSeqVector();
    size_t prot_len = prot_vec.size();

    if ( prot_len == 0 ) {
        if ( transl_prot.length() > 6 ) {
            if ( !(m_IsNG || m_IsNT) ) {
                EDiagSev sev = eDiag_Error;
                if ( IsDeltaOrFarSeg(location) ) {
                    sev = eDiag_Warning;
                }
                if ( m_IsNC ) {
                    sev = eDiag_Warning;

                    if ( bsh.GetTopLevelSeqEntry().IsSeq() ) {
                        sev = eDiag_Info;
                    }
                }
                if ( sev != eDiag_Info ) {
                    ValidErr(sev, eErr_SEQ_FEAT_NoProtein, 
                        "No protein Bioseq given", feat);
                }
            }
        }
        ReportCdTransErrors(feat, show_stop, got_stop, no_end, ragged);
        return;
    }
    
    size_t len = transl_prot.length();
    if ( got_stop  &&  (len == prot_len + 1) ) { // ok, got stop
        --len;
    }

    // ignore terminal 'X' from partial last codon if present
    
    while ( prot_len > 0 ) {
        if ( prot_vec[prot_len - 1] == 'X' ) {  //remove terminal X
            --prot_len;
        } else {
            break;
        }
    }
    
    while ( len > 0 ) {
        if ( transl_prot[len - 1] == 'X' ) {  //remove terminal X
            --len;
        } else {
            break;
        }
    }

    vector<TSeqPos> mismatches;
    if ( len == prot_len )  {                // could be identical
        for ( TSeqPos i = 0; i < len; ++i ) {
            if ( transl_prot[i] != prot_vec[i] ) {
                mismatches.push_back(i);
                prot_ok = false;
            }
        }
    } else {
        ValidErr(eDiag_Error, eErr_SEQ_FEAT_TransLen,
            "Given protein length [" + NStr::IntToString(prot_len) + 
            "] does not match translation length [" + 
            NStr::IntToString(len) + "]", feat);
    }
    
    // Mismatch on first residue
    string msg;
    if ( !mismatches.empty() && mismatches.front() == 0 ) {
        if ( feat.GetPartial() && (!no_beg) && (!no_end)) {
            ValidErr(eDiag_Error, eErr_SEQ_FEAT_PartialProblem, 
                "Start of location should probably be partial",
                feat);
        } else if ( transl_prot[mismatches.front()] == '-' ) {
            ValidErr(eDiag_Error, eErr_SEQ_FEAT_StartCodon,
                "Illegal start codon used. Wrong genetic code [" +
                gccode + "] or protein should be partial", feat);
        }
    }

    char prot_res, transl_res;
    string nuclocstr;
    if ( mismatches.size() > 10 ) {
        // report total number of mismatches and the details of the 
        // first and last.
        nuclocstr = s_MapToNTCoords(feat, product, mismatches.front(), m_Scope);
        prot_res = prot_vec[mismatches.front()];
        transl_res = s_Residue(transl_prot[mismatches.front()]);
        msg = 
            NStr::IntToString(mismatches.size()) + "mismatches found. " +
            "First mismatch at " + NStr::IntToString(mismatches.front() + 1) +
            ", residue in protein [" + prot_res + "]" +
            " != translation [" + transl_res + "]";
        if ( !nuclocstr.empty() ) {
            msg += " at " + nuclocstr;
        }
        nuclocstr = s_MapToNTCoords(feat, product, mismatches.back(), m_Scope);
        prot_res = prot_vec[mismatches.back()];
        transl_res = s_Residue(transl_prot[mismatches.back()]);
        msg += 
            ". Last mismatch at " + NStr::IntToString(mismatches.back() + 1) +
            ", residue in protein [" + prot_res + "]" +
            " != translation [" + transl_res + "]";
        if ( !nuclocstr.empty() ) {
            msg += " at " + nuclocstr;
        }
        msg += ". Genetic code [" + gccode + "]";
        ValidErr(eDiag_Error, eErr_SEQ_FEAT_MisMatchAA, msg, feat);
    } else {
        // report individual mismatches
        for ( size_t i = 0; i < mismatches.size(); ++i ) {
            nuclocstr = s_MapToNTCoords(feat, product, mismatches[i], m_Scope);
            prot_res = prot_vec[mismatches[i]];
            transl_res = s_Residue(transl_prot[mismatches[i]]);
            msg += 
                "Residue " + NStr::IntToString(mismatches[i] + 1) + 
                " in protein [" + prot_res + "]" +
                " != translation [" + transl_res + "]";
            if ( !nuclocstr.empty() ) {
                msg += " at " + nuclocstr;
            }
            ValidErr(eDiag_Error, eErr_SEQ_FEAT_MisMatchAA, msg, feat);
        }
    }

    if ( feat.GetPartial()  && mismatches.empty() ) {
        if ( !no_beg  && !no_end ) {
            if ( !got_stop ) {
                ValidErr(eDiag_Error, eErr_SEQ_FEAT_PartialProblem, 
                    "End of location should probably be partial", feat);
            } else {
                ValidErr(eDiag_Error, eErr_SEQ_FEAT_PartialProblem,
                    "This SeqFeat should not be partial", feat);
            }
            show_stop = false;
        }
    }

    ReportCdTransErrors(feat, show_stop, got_stop, no_end, ragged);
}


void CValidError_impl::CheckForBothStrands (const CSeq_feat& feat)
{
    const CSeq_loc& location = feat.GetLocation ();
    for (CSeq_loc_CI citer (location); citer; ++citer) {
        if ( citer.GetStrand () == eNa_strand_both ) {
            ValidErr (eDiag_Error, eErr_SEQ_FEAT_BothStrands, 
                "mRNA or CDS may not be on both strands", feat);  
            break;
        }
    }
}


//************************** CValidError implementation **********************

CValidError::CValidError
(CObjectManager&   objmgr,
 const CSeq_entry& se,
 unsigned int      options)
{
    CValidError_impl val_impl(objmgr, m_ErrItems, options);
    val_impl.Validate(se);
}


CValidError::CValidError
(CObjectManager&    objmgr,
 const CSeq_submit& ss,
 unsigned int       options)
{

    CValidError_impl val_impl(objmgr, m_ErrItems, options);
    val_impl.Validate(ss);
}


CValidError::~CValidError()
{
}


//************************** miscellaneous arrays **********************

// External terse error type explanation
const string CValidErrItem::sm_Terse [] = {
    "UNKNOWN",

    "SEQ_INST_ExtNotAllowed",
    "SEQ_INST_ExtBadOrMissing",
    "SEQ_INST_SeqDataNotFound",
    "SEQ_INST_SeqDataNotAllowed",
    "SEQ_INST_ReprInvalid",
    "SEQ_INST_CircularProtein",
    "SEQ_INST_DSProtein",
    "SEQ_INST_MolNotSet",
    "SEQ_INST_MolOther",
    "SEQ_INST_FuzzyLen",
    "SEQ_INST_InvalidLen",
    "SEQ_INST_InvalidAlphabet",
    "SEQ_INST_SeqDataLenWrong",
    "SEQ_INST_SeqPortFail",
    "SEQ_INST_InvalidResidue",
    "SEQ_INST_StopInProtein",
    "SEQ_INST_PartialInconsistent",
    "SEQ_INST_ShortSeq",
    "SEQ_INST_NoIdOnBioseq",
    "SEQ_INST_BadDeltaSeq",
    "SEQ_INST_LongHtgsSequence",
    "SEQ_INST_LongLiteralSequence",
    "SEQ_INST_SequenceExceeds350kbp",
    "SEQ_INST_ConflictingIdsOnBioseq",
    "SEQ_INST_MolNuclAcid",
    "SEQ_INST_ConflictingBiomolTech",
    "SEQ_INST_SeqIdNameHasSpace",
    "SEQ_INST_IdOnMultipleBioseqs",
    "SEQ_INST_DuplicateSegmentReferences",
    "SEQ_INST_TrailingX",
    "SEQ_INST_BadSeqIdFormat",
    "SEQ_INST_PartsOutOfOrder",
    "SEQ_INST_BadSecondaryAccn",
    "SEQ_INST_ZeroGiNumber",
    "SEQ_INST_RnaDnaConflict",
    "SEQ_INST_HistoryGiCollision",
    "SEQ_INST_GiWithoutAccession",
    "SEQ_INST_MultipleAccessions",

    "SEQ_DESCR_BioSourceMissing",
    "SEQ_DESCR_InvalidForType",
    "SEQ_DESCR_FileOpenCollision",
    "SEQ_DESCR_Unknown",
    "SEQ_DESCR_NoPubFound",
    "SEQ_DESCR_NoOrgFound",
    "SEQ_DESCR_MultipleBioSources",
    "SEQ_DESCR_NoMolInfoFound",
    "SEQ_DESCR_BadCountryCode",
    "SEQ_DESCR_NoTaxonID",
    "SEQ_DESCR_InconsistentBioSources",
    "SEQ_DESCR_MissingLineage",
    "SEQ_DESCR_SerialInComment",
    "SEQ_DESCR_BioSourceNeedsFocus",
    "SEQ_DESCR_BadOrganelle",
    "SEQ_DESCR_MultipleChromosomes",
    "SEQ_DESCR_BadSubSource",
    "SEQ_DESCR_BadOrgMod",
    "SEQ_DESCR_InconsistentProteinTitle",
    "SEQ_DESCR_Inconsistent",
    "SEQ_DESCR_ObsoleteSourceLocation",
    "SEQ_DESCR_ObsoleteSourceQual",
    "SEQ_DESCR_StructuredSourceNote",
    "SEQ_DESCR_MultipleTitles",

    "GENERIC_NonAsciiAsn",
    "GENERIC_Spell",
    "GENERIC_AuthorListHasEtAl",
    "GENERIC_MissingPubInfo",
    "GENERIC_UnnecessaryPubEquiv",
    "GENERIC_BadPageNumbering",

    "SEQ_PKG_NoCdRegionPtr",
    "SEQ_PKG_NucProtProblem",
    "SEQ_PKG_SegSetProblem",
    "SEQ_PKG_EmptySet",
    "SEQ_PKG_NucProtNotSegSet",
    "SEQ_PKG_SegSetNotParts",
    "SEQ_PKG_SegSetMixedBioseqs",
    "SEQ_PKG_PartsSetMixedBioseqs",
    "SEQ_PKG_PartsSetHasSets",
    "SEQ_PKG_FeaturePackagingProblem",
    "SEQ_PKG_GenomicProductPackagingProblem",
    "SEQ_PKG_InconsistentMolInfoBiomols",

    "SEQ_FEAT_InvalidForType",
    "SEQ_FEAT_PartialProblem",
    "SEQ_FEAT_InvalidType",
    "SEQ_FEAT_Range",
    "SEQ_FEAT_MixedStrand",
    "SEQ_FEAT_SeqLocOrder",
    "SEQ_FEAT_CdTransFail",
    "SEQ_FEAT_StartCodon",
    "SEQ_FEAT_InternalStop",
    "SEQ_FEAT_NoProtein",
    "SEQ_FEAT_MisMatchAA",
    "SEQ_FEAT_TransLen",
    "SEQ_FEAT_NoStop",
    "SEQ_FEAT_TranslExcept",
    "SEQ_FEAT_NoProtRefFound",
    "SEQ_FEAT_NotSpliceConsensus",
    "SEQ_FEAT_OrfCdsHasProduct",
    "SEQ_FEAT_GeneRefHasNoData",
    "SEQ_FEAT_ExceptInconsistent",
    "SEQ_FEAT_ProtRefHasNoData",
    "SEQ_FEAT_GenCodeMismatch",
    "SEQ_FEAT_RNAtype0",
    "SEQ_FEAT_UnknownImpFeatKey",
    "SEQ_FEAT_UnknownImpFeatQual",
    "SEQ_FEAT_WrongQualOnImpFeat",
    "SEQ_FEAT_MissingQualOnImpFeat",
    "SEQ_FEAT_PsuedoCdsHasProduct",
    "SEQ_FEAT_IllegalDbXref",
    "SEQ_FEAT_FarLocation",
    "SEQ_FEAT_DuplicateFeat",
    "SEQ_FEAT_UnnecessaryGeneXref",
    "SEQ_FEAT_TranslExceptPhase",
    "SEQ_FEAT_TrnaCodonWrong",
    "SEQ_FEAT_BothStrands",
    "SEQ_FEAT_CDSgeneRange",
    "SEQ_FEAT_CDSmRNArange",
    "SEQ_FEAT_OverlappingPeptideFeat",
    "SEQ_FEAT_SerialInComment",
    "SEQ_FEAT_MultipleCDSproducts",
    "SEQ_FEAT_FocusOnBioSourceFeature",
    "SEQ_FEAT_PeptideFeatOutOfFrame",
    "SEQ_FEAT_InvalidQualifierValue",
    "SEQ_FEAT_MultipleMRNAproducts",
    "SEQ_FEAT_mRNAgeneRange",
    "SEQ_FEAT_TranscriptLen",
    "SEQ_FEAT_TranscriptMismatches",
    "SEQ_FEAT_CDSproductPackagingProblem",
    "SEQ_FEAT_DuplicateInterval",
    "SEQ_FEAT_PolyAsiteNotPoint",
    "SEQ_FEAT_ImpFeatBadLoc",
    "SEQ_FEAT_LocOnSegmentedBioseq",
    "SEQ_FEAT_UnnecessaryCitPubEquiv",
    "SEQ_FEAT_ImpCDShasTranslation",
    "SEQ_FEAT_ImpCDSnotPseudo",
    "SEQ_FEAT_MissingMRNAproduct",
    "SEQ_FEAT_AbuttingIntervals",
    "SEQ_FEAT_CollidingGeneNames",
    "SEQ_FEAT_MultiIntervalGene",
    "SEQ_FEAT_FeatContentDup",

    "SEQ_ALIGN_SeqIdProblem",
    "SEQ_ALIGN_StrandRev",
    "SEQ_ALIGN_DensegLenStart",
    "SEQ_ALIGN_StartLessthanZero",
    "SEQ_ALIGN_StartMorethanBiolen",
    "SEQ_ALIGN_EndLessthanZero",
    "SEQ_ALIGN_EndMorethanBiolen",
    "SEQ_ALIGN_LenLessthanZero",
    "SEQ_ALIGN_LenMorethanBiolen",
    "SEQ_ALIGN_SumLenStart",
    "SEQ_ALIGN_AlignDimSeqIdNotMatch",
    "SEQ_ALIGN_SegsDimSeqIdNotMatch",
    "SEQ_ALIGN_FastaLike",
    "SEQ_ALIGN_NullSegs",
    "SEQ_ALIGN_SegmentGap",
    "SEQ_ALIGN_SegsDimOne",
    "SEQ_ALIGN_AlignDimOne",
    "SEQ_ALIGN_Segtype",
    "SEQ_ALIGN_BlastAligns",

    "SEQ_GRAPH_GraphMin",
    "SEQ_GRAPH_GraphMax",
    "SEQ_GRAPH_GraphBelow",
    "SEQ_GRAPH_GraphAbove",
    "SEQ_GRAPH_GraphByteLen",
    "SEQ_GRAPH_GraphOutOfOrder",
    "SEQ_GRAPH_GraphBioseqLen",
    "SEQ_GRAPH_GraphSeqLitLen",
    "SEQ_GRAPH_GraphSeqLocLen",
    "SEQ_GRAPH_GraphStartPhase",
    "SEQ_GRAPH_GraphStopPhase",
    "SEQ_GRAPH_GraphDiffNumber",
    "SEQ_GRAPH_GraphACGTScore",
    "SEQ_GRAPH_GraphNScore",
    "SEQ_GRAPH_GraphGapScore",
    "SEQ_GRAPH_GraphOverlap",

    "UNKONWN"
};


// External verbose error type explanation
const string CValidErrItem::sm_Verbose [] = {
"UNKNOWN",

/* SEQ_INST */

"A Bioseq 'extension' is used for special classes of Bioseq. This class \
of Bioseq should not have one but it does. This is probably a software \
error.",

"This class of Bioseq requires an 'extension' but it is missing or of \
the wrong type. This is probably a software error.",

"No actual sequence data was found on this Bioseq. This is probably a \
software problem.",

"The wrong type of sequence data was found on this Bioseq. This is \
probably a software problem.",

"This Bioseq has an invalid representation class. This is probably a \
software error.",

"This protein Bioseq is represented as circular. Circular topology is \
normally used only for certain DNA molecules, for example, plasmids.",

"This protein Bioseq has strandedness indicated. Strandedness is \
normally a property only of DNA sequences. Please unset the \
strandedness.",

"It is not clear whether this sequence is nucleic acid or protein. \
Please set the appropriate molecule type (Bioseq.mol).",

"Most sequences are either nucleic acid or protein. However, the \
molecule type (Bioseq.mol) is set to 'other'. It should probably be set \
to nucleic acid or a protein.",
"This sequence is marked as having an uncertain length, but the length \
is known exactly.",

"The length indicated for this sequence is invalid. This is probably a \
software error.",

"This Bioseq has an invalid alphabet (e.g. protein codes on a nucleic \
acid or vice versa). This is probably a software error.",

"The length of this Bioseq does not agree with the length of the actual \
data. This is probably a software error.",

"Something is very wrong with this entry. The validator cannot open a \
SeqPort on the Bioseq. Further testing cannot be done.",

"Invalid residue codes were found in this Bioseq.",

"Stop codon symbols were found in this protein Bioseq.",

"This segmented sequence is described as complete or incomplete in \
several places, but these settings are inconsistent.",

"This Bioseq is unusually short (less than 4 amino acids or less than 11 \
nucleic acids). GenBank does not usually accept such short sequences.",

"No SeqIds were found on this Bioseq. This is probably a software \
error.",

"Delta sequences should only be HTGS-1 or HTGS-2.",

"HTGS-1 or HTGS-2 sequences must be < 350 KB in length.",

"Delta literals must be < 350 KB in length.",

"Individual sequences must be < 350 KB in length, unless they represent \
a single gene.",

"Two SeqIds of the same class was found on this Bioseq. This is probably \
a software error.",

"The specific type of this nucleic acid (DNA or RNA) is not set.",

"HTGS/STS/GSS records should be genomic DNA. There is a conflict between \
the technique and expected molecule type.",

"The Seq-id.name field should be a single word without any whitespace. \
This should be fixed by the database staff.",

"There are multiple occurrences of the same Seq-id in this record. \
Sequence identifiers must be unique within a record.",

"The segmented sequence refers multiple times to the same Seq-id. This \
may be due to a software error. Please consult with the database staff \
to fix this record.",

"The protein sequence ends with one or more X (unknown) amino acids.",

"A nucleotide sequence identifier should be 1 letter plus 5 digits or 2 \
letters plus 6 digits, and a protein sequence identifer should be 3 \
letters plus 5 digits.",

"The parts inside a segmented set should correspond to the seq_ext of \
the segmented bioseq. A difference will affect how the flatfile is \
displayed.",

"A secondary accession usually indicates a record replaced or subsumed \
by the current record. In this case, the current accession and \
secondary are the same.",

"GI numbers are assigned to sequences by NCBI's sequence tracking \
database. 0 is not a legal value for a gi number.",

"The MolInfo biomol field is inconsistent with the Bioseq molecule type \
field.",

"The Bioseq history gi refers to this Bioseq, not to its predecessor or \
successor.",

"The Bioseq has a gi identifier but no GenBank/EMBL/DDBJ accession \
identifier.",

"The Bioseq has a gi identifier and more than one GenBank/EMBL/DDBJ \
accession identifier.",

/* SEQ_DESCR */

"The biological source of this sequence has not been described \
correctly. A Bioseq must have a BioSource descriptor that covers the \
entire molecule. Additional BioSource features may also be added to \
recombinant molecules, natural or otherwise, to designate the parts of \
the molecule. Please add the source information.",

"This descriptor cannot be used with this Bioseq. A descriptor placed at \
the BioseqSet level applies to all of the Bioseqs in the set. Please \
make sure the descriptor is consistent with every sequence to which it \
applies.",

"FileOpen is unable to find a local file. This is normal, and can be \
ignored.",

"An unknown or 'other' modifier was used.",

"No publications were found in this entry which refer to this Bioseq. If \
a publication descriptor is added to a BioseqSet, it will apply to all \
of the Bioseqs in the set. A publication feature should be used if the \
publication applies only to a subregion of a sequence.",

"This entry does not specify the organism that was the source of the \
sequence. Please name the organism.",

"There are multiple BioSource or OrgRef descriptors in the same chain \
with the same taxonomic name. Their information should be combined into \
a single BioSource descriptor.",

"This sequence does not have a Mol-info descriptor applying to it. This \
indicates genomic vs. message, sequencing technique, and whether the \
sequence is incomplete.",

"The country code (up to the first colon) is not on the approved list of \
countries.",

"The BioSource is missing a taxonID database identifier. This will be \
inserted by the automated taxonomy lookup called by Clean Up Record.",

"This population study has BioSource descriptors with different \
taxonomic names. All members of a population study should be from the \
same organism.",

"A BioSource should have a taxonomic lineage, which can be obtained from \
the taxonomy network server.",

"Comments that refer to the conclusions of a specific reference should \
not be cited by a serial number inside brackets (e.g., [3]), but should \
instead be attached as a REMARK on the reference itself.",

"Focus must be set on a BioSource descriptor in records where there is a \
BioSource feature with a different organism name.",

"Note that only Kinetoplastida have kinetoplasts, and that only \
Chlorarchniophyta and Cryptophyta have nucleomorphs.",

"There are multiple chromosome qualifiers on this Bioseq. With the \
exception of some pseudoautosomal genes, this is likely to be a \
biological annotation error.",

"Unassigned SubSource subtype.",

"Unassigned OrgMod subtype.",

"An instantiated protein title descriptor should normally be the same as \
the automatically generated title. This may be a curated exception, or \
it may be out of synch with the current annotation.",

"There are two descriptors of the same type which are inconsistent with \
each other. Please make them consistent.",

"There is a source location that is no longer legal for use in GenBank \
records.",

"There is a source qualifier that is no longer legal for use in GenBank \
records.",

"The name of a structured source field is present as text in a note. \
The data should probably be put into the appropriate field instead.",

"There are multiple title descriptors in the same chain.",

/* SEQ_GENERIC */

"There is a non-ASCII type character in this entry.",

"There is a potentially misspelled word in this entry.",

"The author list contains et al, which should be replaced with the \
remaining author names.",

"The publication is missing essential information, such as title or \
authors.",

"A nested Pub-equiv is not normally expected in a publication. This may \
prevent proper display of all publication information.",

"The publication page numbering is suspect.",

/* SEQ_PKG */

"A protein is found in this entry, but the coding region has not been \
described. Please add a CdRegion feature to the nucleotide Bioseq.",

"Both DNA and protein sequences were expected, but one of the two seems \
to be missing. Perhaps this is the wrong package to use.",

"A segmented sequence was expected, but it was not found. Perhaps this \
is the wrong package to use.",

"No Bioseqs were found in this BioseqSet. Is that what was intended?",

"A nuc-prot set should not contain any other BioseqSet except segset.",

"A segset should not contain any other BioseqSet except parts.",

"A segset should not contain both nucleotide and protein Bioseqs.",

"A parts set should not contain both nucleotide and protein Bioseqs.",

"A parts set should not contain BioseqSets.",

"A feature should be packaged on its bioseq, or on a set containing the \
Bioseq.",

"The product of an mRNA feature in a genomic product set should point to \
a cDNA Bioseq packaged in the set, perhaps within a nuc-prot set. \
RefSeq records may however be referenced remotely.",

"Mol-info.biomol is inconsistent within a segset or parts set.",

/* SEQ_FEAT */

"This feature type is illegal on this type of Bioseq.",

"There are several places in an entry where a sequence can be described \
as either partial or complete. In this entry, these settings are \
inconsistent. Make sure that the location and product Seq-locs, the \
Bioseqs, and the SeqFeat partial flag all agree in describing this \
SeqFeat as partial or complete.",

"A feature with an invalid type has been detected. This is most likely a \
software problem.",

"The coordinates describing the location of a feature do not fall within \
the sequence itself. A feature location or a product Seq-loc is out of \
range of the Bioseq it points to.",

"Mixed strands (plus and minus) have been found in the same location. \
While this is biologically possible, it is very unusual. Please check \
that this is really what you mean.",

"This location has intervals that are out of order. While whis is \
biologically possible, it is very unusual. Please check that this is \
really what you mean.",

"A fundamental error occurred in software while attempting to translate \
this coding region. It is either a software problem or sever data \
corruption.",

"An illegal start codon was used. Some possible explanations are: (1) \
the wrong genetic code may have been selected; (2) the wrong reading \
frame may be in use; or (3) the coding region may be incomplete at the \
5' end, in which case a partial location should be indicated.",

"Internal stop codons are found in the protein sequence. Some possible \
explanations are: (1) the wrong genetic code may have been selected; (2) \
the wrong reading frame may be in use; (3) the coding region may be \
incomplete at the 5' end, in which case a partial location should be \
indicated; or (4) the CdRegion feature location is incorrect.",

"Normally a protein sequence is supplied. This sequence can then be \
compared with the translation of the coding region. In this entry, no \
protein Bioseq was found, and the comparison could not be made.",

"The protein sequence that was supplied is not identical to the \
translation of the coding region. Mismatching amino acids are found \
between these two sequences.",

"The protein sequence that was supplied is not the same length as the \
translation of the coding region. Please determine why they are \
different.",

"A coding region that is complete should have a stop codon at the 3'end. \
 A stop codon was not found on this sequence, although one was \
expected.",

"An unparsed \transl_except qualifier was found. This indicates a parser \
problem.",

"The name and description of the protein is missing from this entry. \
Every protein Bioseq must have one full-length Prot-ref feature to \
provide this information.",

"Splice junctions typically have GT as the first two bases of the intron \
(splice donor) and AG as the last two bases of the intron (splice \
acceptor). This intron does not conform to that pattern.",

"A coding region flagged as orf has a protein product. There should be \
no protein product bioseq on an orf.",

"A gene feature exists with no locus name or other fields filled in.",

"A coding region has an exception gbqual but the excpt flag is not \
set.",

"A protein feature exists with no name or other fields filled in.",

"The genetic code stored in the BioSource is different than that for \
this CDS.",

"RNA type 0 (unknown RNA) should be type 255 (other).",

"An import feature has an unrecognized key.",

"An import feature has an unrecognized qualifier.",

"This qualifier is not legal for this feature.",

"An essential qualifier for this feature is missing.",

"A coding region flagged as pseudo has a protein product. There should \
be no protein product bioseq on a pseudo CDS.",

"The database in a cross-reference is not on the list of officially \
recognized database abbreviations.",

"The location has a reference to a bioseq that is not packaged in this \
record.",

"The intervals on this feature are identical to another feature of the \
same type, but the label or comment are different.",

"This feature has a gene xref that is identical to the overlapping gene. \
This is redundant, and probably should be removed.",

"A \transl_except qualifier was not on a codon boundary.",

"The tRNA codon recognized does not code for the indicated amino acid \
using the specified genetic code.",

"Feature location indicates that it is on both strands. This is not \
biologically possible for this kind of feature. Please indicate the \
correct strand (plus or minus) for this feature.",

"A CDS is overlapped by a gene feature, but is not completely contained \
by it. This may be an annotation error.",

"A CDS is overlapped by an mRNA feature, but the mRNA does not cover all \
intervals (i.e., exons) on the CDS. This may be an annotation error.",

"The intervals on this processed protein feature overlap another protein \
feature. This may be caused by errors in originally annotating these \
features on DNA coordinates, where start or stop positions do not occur \
in between codon boundaries. These then appear as errors when the \
features are converted to protein coordinates by mapping through the \
CDS.",

"Comments that refer to the conclusions of a specific reference should \
not be cited by a serial number inside brackets (e.g., [3]), but should \
instead be attached as a REMARK on the reference itself.",

"More than one CDS feature points to the same protein product. This can \
happen with viral long terminal repeats (LTRs), but GenBank policy is to \
have each equivalent CDS point to a separately accessioned protein \
Bioseq.",

"The /focus flag is only appropriate on BioSource descriptors, not \
BioSource features.",

"The start or stop positions of this processed peptide feature do not \
occur in between codon boundaries. This may incorrectly overlap other \
peptides when the features are converted to protein coordinates by \
mapping through the CDS.",

"The value of this qualifier is constrained to a particular vocabulary \
of style. This value does not conform to those constraints. Please see \
the feature table documentation",

"for more information.",

"More than one mRNA feature points to the same cDNA product. This is an \
error in the genomic product set. Each mRNA feature should have a \
unique product Bioseq.",

"An mRNA is overlapped by a gene feature, but is not completely \
contained by it. This may be an annotation error.",

"The mRNA sequence that was supplied is not the same length as the \
transcription of the mRNA feature. Please determine why they are \
different.",

"The mRNA sequence and the transcription of the mRNA feature are \
different. If the number is large, it may indicate incorrect intron/exon \
boundaries.",

"The nucleotide location and protein product of the CDS are not packaged \
together in the same nuc-prot set. This may be an error in the software \
used to create",

"the record.",

"The location has identical adjacent intervals, e.g., a duplicate exon \
reference.",

"A polyA_site should be at a single nucleotide position.",

"An import feature loc field does not equal the feature location. This \
should be corrected, and then the loc field should be cleared.",

"Feature locations traditionally go on the individual parts of a \
segmented bioseq, not on the segmented sequence itself. These features \
are invisible in asn2ff reports, and are now being flagged for \
correction.",

"A set of citations on a feature should not normally have a nested \
Pub-equiv construct. This may prevent proper matching to the correct \
publication.",

"A CDS that has known translation errors cannot have a /translation \
qualifier.",

"A CDS that has known translation errors must be marked as pseudo to \
suppress the",

"translation.",

"The mRNA feature points to a cDNA product that is not packaged in the \
record. This is an error in the genomic product set.",

"The start of one interval is next to the stop of another. A single \
interval may be desirable in this case.",

"Two gene features should not have the same name.",

"A gene feature on a single Bioseq should have a single interval \
spanning everything considered to be under that gene.",

"The intervals on this feature are identical to another feature of the \
same type, and the label and comment are also identical. This is likely \
to be an error in annotating the record. Note that GenBank format \
suppresses duplicate features, so use of Graphic view is recommended.",

/* SEQ_ALIGN */

"The seqence referenced by an alignment SeqID is not packaged in the \
record.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"Please contact the sequence database for further help with this \
error.",

"BLAST alignments are not desired in records submitted to the sequence \
database.",

/* SEQ_GRAPH */

"The graph minimum value is outside of the 0-100 range.",

"The graph maximum value is outside of the 0-100 range.",

"Some quality scores are below the stated graph minimum value.",

"Some quality scores are above the stated graph maximum value.",

"The number of bytes in the quality graph does not correspond to the",

"stated length of the graph.",

"The quality graphs are not packaged in order - may be due to an old \
fa2htgs bug.",

"The length of the quality graph does not correspond to the length of \
the Bioseq.",

"The length of the quality graph does not correspond to the length of \
the delta Bioseq literal component.",

"The length of the quality graph does not correspond to the length of \
the delta Bioseq location component.",

"The quality graph does not start or stop on a sequence segment \
boundary.",

"The quality graph does not start or stop on a sequence segment \
boundary.",

"The number quality graph does not equal the number of sequence \
segments.",

"Quality score values for known bases should be above 0.",

"Quality score values for unknown bases should not be above 0.",

"Gap positions should not have quality scores above 0.",

"Quality graphs overlap - may be due to an old fa2htgs bug.",

"UNKNOWN"
};


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE



/*
* ===========================================================================
* $Log$
* Revision 1.36  2002/12/17 17:59:35  shomrat
* Add checks for Bioseq with no pubs / source
*
* Revision 1.35  2002/12/13 22:35:38  ucko
* Fix compilation on at least GCC 2.9x.
*
* Revision 1.34  2002/12/13 21:57:58  shomrat
* Added: CGBQualType, CFeatQualAssoc, ValidateImp, ValidateSeqFeatContext; Changes in ValidateNucProt and ValidateSeqLoc
*
* Revision 1.33  2002/12/11 21:34:29  shomrat
* Add SpliceCheck (SpliceCheckEx)
*
* Revision 1.32  2002/12/11 18:55:33  shomrat
* Bug fixes for CdTransCheck and CheckForCodeBreakNotOnCodon
*
* Revision 1.31  2002/12/04 19:21:26  shomrat
* Add CdTrandCheck; minor bug and style fixes
*
* Revision 1.30  2002/11/26 19:10:50  shomrat
* Bug fix - GetGcode changed to GetGenCode
*
* Revision 1.29  2002/11/26 18:56:37  shomrat
* Implement CheckTrnaCodons
*
* Revision 1.28  2002/11/19 19:54:50  shomrat
* Bug fix in ValidatePopSet
*
* Revision 1.26  2002/11/18 19:48:44  grichenk
* Removed "const" from datatool-generated setters
*
* Revision 1.25  2002/11/18 15:39:50  shomrat
* ValidateSourceQualTags checks pattern on a left word boundry
*
* Revision 1.24  2002/11/18 15:29:05  kans
* Implemented ValidateSeqSet (LF)
*
* Revision 1.23  2002/11/08 20:13:39  kans
* implemented ValidateDupOrOverlapFeats, ValidateSourceQualTags (MS)
*
* Revision 1.22  2002/11/04 21:29:19  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.21  2002/10/31 22:27:36  kans
* GetBioseqHandleByLoc moved to objmgr/scope, used in ValidateCdregion to look for genetic code conflicts, which relies on CSeqdesc_CI up-the-hierarchy descriptor indexing
*
* Revision 1.20  2002/10/30 22:44:50  kans
* fixes to a few functions, first pass at GetBioseqHandleByLoc
*
* Revision 1.19  2002/10/29 19:35:46  clausen
* Moved CValidException definition here
*
* Revision 1.18  2002/10/29 19:23:24  clausen
* Added new NCBI_THROW & NCBI_RETHROW macros
*
* Revision 1.17  2002/10/29 18:56:43  kans
* implemented most of ValidateSeqFeat (MS)
*
* Revision 1.16  2002/10/23 16:35:10  clausen
* Fixed various warning messages in WorkShop53
*
* Revision 1.15  2002/10/23 15:34:12  clausen
* Fixed local variable scope warnings
*
* Revision 1.14  2002/10/22 19:28:44  clausen
* Fixed CValidError_CI::operator++ to prevent  m_ErrIter from going beyond end()
*
* Revision 1.13  2002/10/14 20:44:35  kans
* use WriteAsFasta and CNcbiOstrstreamToString in ValidateSeqIds
*
* Revision 1.12  2002/10/14 20:00:58  kans
* added multiple titles check, moved big string arrays to end
*
* Revision 1.11  2002/10/14 15:06:43  ucko
* ValidateSeqDesc: Put braces around the cases with local variables, to
* fix compilation on (at least) GCC.
*
* Revision 1.10  2002/10/11 21:36:48  kans
* flatten organization, iterating within main Validate function, and added ValidateBiosource (MS)
*
* Revision 1.9  2002/10/11 13:52:36  clausen
* QA fixes & changed Seq-entry navigation
*
* Revision 1.8  2002/10/08 19:51:37  kans
* ValidateBioSource and ValidatePubdesc take CSerialObject
*
* Revision 1.7  2002/10/08 14:35:42  clausen
* Added calls to ValidateBiosource() & ValidatePub()
*
* Revision 1.6  2002/10/07 18:14:22  clausen
* Fixed error in CNoCaseCompare that prevent compile on Mac
*
* Revision 1.5  2002/10/07 17:11:16  ucko
* Fix usage of ERR_POST (caught by KCC)
*
* Revision 1.4  2002/10/04 14:39:41  ucko
* Use TSmap::value_type's constructor directly rather than via make_pair,
* as the latter fails under WorkShop.
*
* Revision 1.3  2002/10/04 14:27:37  ivanov
* Fixed s_IsDifferentDbxref() -- MSVC don't like identifier names lst1
* and lst2 in this function (weird compiler :).
* Fixed some warnings.
*
* Revision 1.2  2002/10/03 18:40:12  clausen
* Removed extra whitespace
*
* Revision 1.1  2002/10/03 16:29:32  clausen
* First version
*
* ===========================================================================
*/
