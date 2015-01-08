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
 *  and reliability of the software and data,  the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties,  express or implied,  including
 *  warranties of performance,  merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Authors:  Colleen Bollin
 * Created:  17/09/2013 15:38:53
 */


#ifndef _ANALYSIS_PROCESS_H_
#define _ANALYSIS_PROCESS_H_

#include <corelib/ncbistd.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <misc/discrepancy_report/report_item.hpp>
#include <misc/discrepancy_report/config.hpp>

BEGIN_NCBI_SCOPE

USING_SCOPE(objects);
BEGIN_SCOPE(DiscRepNmSpc);

class CReportMetadata : public CObject
{
public:
    CReportMetadata() {};
    ~CReportMetadata() {};

    string GetFilename() const { return m_Filename; }
    void SetFilename(const string& filename) { m_Filename = filename; }

protected:
    string m_Filename;
};



class CAnalysisProcess : public CObject
{
public:
    // method to collect data from a Seq-entry Handle. Data may be stored in an 
    // intermediate format, rather than using CReportItem if this is useful. The 
    // CollectData method could be called on multiple Seq-entry Handles that should
    // be compared against each other before CollateData and GetReportItems are called
    virtual void CollectData(CSeq_entry_Handle seh, const CReportMetadata& metadata) = 0;
    // CollateData should be called after CollectData has been called for all of the 
    // Seq-entry Handles that are being compared against each other. Not every test will
    // need a body for CollateData; many simpler tests will not.
    virtual void CollateData() {}
    // ResetData should be called if a CAnalysisProcess has been used to collect and/or 
    // collate data from Seq-entry Handles and a new, separate report should be generated
    // with this object for a new set of Seq-entry Handles. This method should clear 
    // intermediate formats used by the test.
    virtual void ResetData() = 0;
    // GetReportItems should convert intermediate formats to CReportItem objects. It
    // should be possible to call GetReportItems multiple times with the same internal
    // data.
    virtual TReportItemList GetReportItems(CReportConfig& cfg) const = 0;
    // bool Handles(const string& test_name) returns true or false to indicate whether 
    // this CAnalysisProcess handles the listed test_name. Some test_names refer to exactly 
    // the same kind of intermediate data collection.
    virtual bool Handles(const string& test_name) const;
    // vector<string> Handles() provides a list of all of the test_names that are 
    // handled by this CAnalysisProcess.
    virtual vector<string> Handles() const = 0;
    // DropReferences should cause the test to save text descriptions of related objects
    // and remove any internal references (CRef or CConstRef)
    virtual void DropReferences(CScope& scope) = 0;
    // Autofix problem for collected items
    virtual bool Autofix(CScope& scope) { return false; };

    // utility methods
    void SetDefaultLineage(const string& lineage) { m_DefaultLineage = lineage; };
    bool IsEukaryotic(CBioseq_Handle bh);
    bool HasLineage(const CBioSource& biosrc, const string& type);

protected:
    void x_DropReferences(TReportObjectList& obj_list, CScope& scope);
    string m_DefaultLineage;

    bool x_StrandsMatch(const CSeq_loc& loc1, const CSeq_loc& loc2);
    static vector<string> x_HandlesOne(const string& test_name);

    // utility methods for Autofixing
    void x_DeleteProteinSequence(CBioseq_Handle prot);
    bool x_ConvertCDSToMiscFeat(const CSeq_feat& feat, CScope& scope);

};


class CAnalysisProcessFactory
{
public:
    // The Create method will create a CAnalysisProcess object to perform the test specified
    // by test_name. This CAnalysisProcess may also perform other tests. To avoid duplication
    // of effort and results, only one CAnalysisProcess object of any class should be created.
    static CRef<CAnalysisProcess> Create(const string &test_name);
    // The Implemented method produces a list of the currently implemented tests. This
    // may be used for measuring the progress of the development of the Discrepancy 
    // Report library. It is also used by the default COrderPolicy class to list 
    // test_names as they were generated by the C Toolkit library, for compatibility
    // purposes.
    static vector<string> Implemented();
};


static const string kMISSING_GENES = "MISSING_GENES";
static const string kDISC_SUPERFLUOUS_GENE = "EXTRA_GENES";
static const string kDISC_GENE_MISSING_LOCUS_TAG = "DISC_GENE_MISSING_LOCUS_TAG";
static const string kDISC_GENE_DUPLICATE_LOCUS_TAG = "DISC_GENE_DUPLICATE_LOCUS_TAG";
static const string kDISC_GENE_LOCUS_TAG_BAD_FORMAT = "DISC_GENE_LOCUS_TAG_BAD_FORMAT";
static const string kDISC_GENE_LOCUS_TAG_INCONSISTENT_PREFIX = "DISC_GENE_LOCUS_TAG_INCONSISTENT_PREFIX";
static const string kDISC_NON_GENE_LOCUS_TAG = "DISC_NON_GENE_LOCUS_TAG";
static const string kDISC_COUNT_NUCLEOTIDES = "DISC_COUNT_NUCLEOTIDES";
static const string kDISC_MISSING_PROTEIN_ID = "DISC_MISSING_PROTEIN_ID";
static const string kDISC_INCONSISTENT_PROTEIN_ID_PREFIX = "DISC_INCONSISTENT_PROTEIN_ID_PREFIX";
static const string kDISC_GENE_CDS_mRNA_LOCATION_CONFLICT = "DISC_GENE_CDS_mRNA_LOCATION_CONFLICT";
static const string kDISC_GENE_PRODUCT_CONFLICT = "DISC_GENE_PRODUCT_CONFLICT";
static const string kDISC_GENE_DUPLICATE_LOCUS = "DISC_GENE_DUPLICATE_LOCUS";
static const string kDISC_EC_NUMBER_NOTE = "DISC_EC_NUMBER_NOTE";
static const string kDISC_PSEUDO_MISMATCH = "DISC_PSEUDO_MISMATCH";
static const string kDISC_JOINED_FEATURES = "DISC_JOINED_FEATURES";
static const string kDISC_OVERLAPPING_GENES = "DISC_OVERLAPPING_GENES";
static const string kDISC_OVERLAPPING_CDS = "DISC_OVERLAPPING_CDS";
static const string kDISC_CONTAINED_CDS = "CONTAINED_CDS";
static const string kDISC_RNA_CDS_OVERLAP = "DISC_RNA_CDS_OVERLAP";
static const string kDISC_SHORT_CONTIG = "DISC_SHORT_CONTIG";
static const string kDISC_INCONSISTENT_BIOSRC = "DISC_INCONSISTENT_BIOSRC";
static const string kDISC_SUSPECT_PRODUCT_NAME = "DISC_SUSPECT_PRODUCT_NAME";
static const string kDISC_PRODUCT_NAME_TYPO = "DISC_PRODUCT_NAME_TYPO";
static const string kDISC_PRODUCT_NAME_QUICKFIX = "DISC_PRODUCT_NAME_QUICKFIX";
static const string kDISC_INCONSISTENT_BIOSRC_DEFLINE = "DISC_INCONSISTENT_BIOSRC_DEFLINE";
static const string kDISC_PARTIAL_CDS_IN_COMPLETE_SEQUENCE = "DISC_PARTIAL_CDS_IN_COMPLETE_SEQUENCE";
static const string kDISC_EC_NUMBER_ON_HYPOTHETICAL_PROTEIN = "DISC_EC_NUMBER_ON_HYPOTHETICAL_PROTEIN";
static const string kDISC_NO_TAXLOOKUP = "DISC_NO_TAXLOOKUP";
static const string kDISC_BAD_TAXLOOKUP = "DISC_BAD_TAXLOOKUP";
static const string kDISC_SHORT_SEQUENCE = "DISC_SHORT_SEQUENCE";
static const string kDISC_SUSPECT_PHRASES = "DISC_SUSPECT_PHRASES";
static const string kDISC_SUSPICIOUS_NOTE_TEXT = "DISC_SUSPICIOUS_NOTE_TEXT";
static const string kDISC_COUNT_TRNA = "DISC_COUNT_TRNA";
static const string kDISC_DUP_TRNA = "DISC_DUP_TRNA";
static const string kDISC_BADLEN_TRNA = "DISC_BADLEN_TRNA";
static const string kDISC_STRAND_TRNA = "DISC_STRAND_TRNA";
static const string kDISC_COUNT_RRNA = "DISC_COUNT_RRNA";
static const string kDISC_DUP_RRNA = "DISC_DUP_RRNA";
static const string kDISC_RNA_NO_PRODUCT = "DISC_RNA_NO_PRODUCT";
static const string kDISC_TRANSL_NO_NOTE = "DISC_TRANSL_NO_NOTE";
static const string kDISC_NOTE_NO_TRANSL = "DISC_NOTE_NO_TRANSL";
static const string kDISC_TRANSL_TOO_LONG = "DISC_TRANSL_TOO_LONG";
static const string kDISC_CDS_OVERLAP_TRNA = "DISC_CDS_OVERLAP_TRNA";
static const string kDISC_COUNT_PROTEINS = "DISC_COUNT_PROTEINS";
static const string kDISC_FEAT_OVERLAP_SRCFEAT = "DISC_FEAT_OVERLAP_SRCFEAT";
static const string kDISC_MISSING_GENPRODSET_PROTEIN = "DISC_MISSING_GENPRODSET_PROTEIN";
static const string kDISC_DUP_GENPRODSET_PROTEIN = "DISC_DUP_GENPRODSET_PROTEIN";
static const string kDISC_MISSING_GENPRODSET_TRANSCRIPT_ID = "DISC_MISSING_GENPRODSET_TRANSCRIPT_ID";
static const string kDISC_DUP_GENPRODSET_TRANSCRIPT_ID = "DISC_DUP_GENPRODSET_TRANSCRIPT_ID";
static const string kDISC_PERCENTN = "DISC_PERCENTN";
static const string kDISC_N_RUNS = "DISC_N_RUNS";
static const string kDISC_ZERO_BASECOUNT = "DISC_ZERO_BASECOUNT";
static const string kDISC_ADJACENT_PSEUDOGENE = "DISC_ADJACENT_PSEUDOGENE";
static const string kDISC_LONG_NO_ANNOTATION = "DISC_LONG_NO_ANNOTATION";
static const string kDISC_NO_ANNOTATION = "DISC_NO_ANNOTATION";
static const string kDISC_INFLUENZA_DATE_MISMATCH = "DISC_INFLUENZA_DATE_MISMATCH";
static const string kDISC_SHORT_INTRON = "DISC_SHORT_INTRON";
static const string kDISC_MISSING_VIRAL_QUALS = "DISC_MISSING_VIRAL_QUALS";
static const string kDISC_SRC_QUAL_PROBLEM = "DISC_SRC_QUAL_PROBLEM";
static const string kDISC_MISSING_SRC_QUAL = "DISC_MISSING_SRC_QUAL";
static const string kDISC_DUP_SRC_QUAL = "DISC_DUP_SRC_QUAL";
static const string kDISC_DUP_SRC_QUAL_DATA = "DISC_DUP_SRC_QUAL_DATA";
static const string kDISC_HAPLOTYPE_MISMATCH = "DISC_HAPLOTYPE_MISMATCH";
static const string kDISC_FEATURE_MOLTYPE_MISMATCH = "DISC_FEATURE_MOLTYPE_MISMATCH";
static const string kDISC_CDS_WITHOUT_MRNA = "DISC_CDS_WITHOUT_MRNA";
static const string kDISC_EXON_INTRON_CONFLICT = "DISC_EXON_INTRON_CONFLICT";
static const string kDISC_FEATURE_COUNT = "DISC_FEATURE_COUNT";
static const string kDISC_SPECVOUCHER_TAXNAME_MISMATCH = "DISC_SPECVOUCHER_TAXNAME_MISMATCH";
static const string kDISC_GENE_PARTIAL_CONFLICT = "DISC_GENE_PARTIAL_CONFLICT";
static const string kDISC_FLATFILE_FIND_ONCALLER = "DISC_FLATFILE_FIND_ONCALLER";
static const string kDISC_FLATFILE_FIND_ONCALLER_FIXABLE = "DISC_FLATFILE_FIND_ONCALLER_FIXABLE";
static const string kDISC_FLATFILE_FIND_ONCALLER_UNFIXABLE = "DISC_FLATFILE_FIND_ONCALLER_UNFIXABLE";
static const string kDISC_CDS_PRODUCT_FIND = "DISC_CDS_PRODUCT_FIND";
static const string kDISC_DUP_DEFLINE = "DISC_DUP_DEFLINE";
static const string kDUP_DISC_ATCC_CULTURE_CONFLICT = "DUP_DISC_ATCC_CULTURE_CONFLICT";
static const string kDISC_USA_STATE = "DISC_USA_STATE";
static const string kDISC_INCONSISTENT_MOLTYPES = "DISC_INCONSISTENT_MOLTYPES";
static const string kDISC_SUBMITBLOCK_CONFLICT = "DISC_SUBMITBLOCK_CONFLICT";
static const string kDISC_POSSIBLE_LINKER = "DISC_POSSIBLE_LINKER";
static const string kDISC_TITLE_AUTHOR_CONFLICT = "DISC_TITLE_AUTHOR_CONFLICT";
static const string kDISC_BAD_GENE_STRAND = "DISC_BAD_GENE_STRAND";
static const string kDISC_MAP_CHROMOSOME_CONFLICT = "DISC_MAP_CHROMOSOME_CONFLICT";
static const string kDISC_RBS_WITHOUT_GENE = "DISC_RBS_WITHOUT_GENE";
static const string kDISC_CITSUBAFFIL_CONFLICT = "DISC_CITSUBAFFIL_CONFLICT";
static const string kDISC_REQUIRED_CLONE = "DISC_REQUIRED_CLONE";
static const string kDISC_SOURCE_QUALS_ASNDISC = "DISC_SOURCE_QUALS_ASNDISC";
static const string kDISC_mRNA_ON_WRONG_SEQUENCE_TYPE = "DISC_MRNA_ON_WRONG_SEQUENCE_TYPE";
static const string kDISC_RETROVIRIDAE_DNA = "DISC_RETROVIRIDAE_DNA";
static const string kDISC_CHECK_AUTH_CAPS = "DISC_CHECK_AUTH_CAPS";
static const string kDISC_CHECK_RNA_PRODUCTS_AND_COMMENTS = "DISC_CHECK_RNA_PRODUCTS_AND_COMMENTS";
static const string kDISC_MICROSATELLITE_REPEAT_TYPE = "DISC_MICROSATELLITE_REPEAT_TYPE";
static const string kDISC_MITOCHONDRION_REQUIRED = "DISC_MITOCHONDRION_REQUIRED";
static const string kDISC_UNPUB_PUB_WITHOUT_TITLE = "DISC_UNPUB_PUB_WITHOUT_TITLE";
static const string kDISC_QUALITY_SCORES = "DISC_QUALITY_SCORES";
static const string kDISC_INTERNAL_TRANSCRIBED_SPACER_RRNA = "DISC_INTERNAL_TRANSCRIBED_SPACER_RRNA";
static const string kDISC_PARTIAL_PROBLEMS = "DISC_PARTIAL_PROBLEMS";
static const string kDISC_BACTERIAL_PARTIAL_NONEXTENDABLE_PROBLEMS = "DISC_BACTERIAL_PARTIAL_NONEXTENDABLE_PROBLEMS";
static const string kDISC_BACTERIAL_PARTIAL_NONEXTENDABLE_EXCEPTION = "DISC_BACTERIAL_PARTIAL_NONEXTENDABLE_EXCEPTION";
static const string kDISC_SUSPECT_RRNA_PRODUCTS = "DISC_SUSPECT_RRNA_PRODUCTS";
static const string kDISC_SUSPECT_MISC_FEATURES = "DISC_SUSPECT_MISC_FEATURES";
static const string kDISC_BACTERIA_MISSING_STRAIN = "DISC_BACTERIA_MISSING_STRAIN";
static const string kDISC_MISSING_DEFLINES = "DISC_MISSING_DEFLINES";
static const string kDISC_MISSING_AFFIL = "DISC_MISSING_AFFIL";
static const string kDISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE = "DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE";
static const string kDISC_BACTERIA_SHOULD_NOT_HAVE_MRNA = "DISC_BACTERIA_SHOULD_NOT_HAVE_MRNA";
static const string kDISC_CDS_HAS_NEW_EXCEPTION = "DISC_CDS_HAS_NEW_EXCEPTION";
static const string kDISC_TRINOMIAL_SHOULD_HAVE_QUALIFIER = "DISC_TRINOMIAL_SHOULD_HAVE_QUALIFIER";
static const string kDISC_METAGENOMIC = "DISC_METAGENOMIC";
static const string kDISC_METAGENOME_SOURCE = "DISC_METAGENOME_SOURCE";
static const string kONCALLER_GENE_MISSING = "ONCALLER_GENE_MISSING";
static const string kONCALLER_SUPERFLUOUS_GENE = "ONCALLER_SUPERFLUOUS_GENE";
static const string kDISC_SHORT_RRNA = "DISC_SHORT_RRNA";
static const string kONCALLER_CHECK_AUTHORITY = "ONCALLER_CHECK_AUTHORITY";
static const string kONCALLER_CONSORTIUM = "ONCALLER_CONSORTIUM";
static const string kONCALLER_STRAIN_CULTURE_COLLECTION_MISMATCH = "ONCALLER_STRAIN_CULTURE_COLLECTION_MISMATCH";
static const string kONCALLER_MULTISRC = "ONCALLER_MULTISRC";
static const string kONCALLER_MULTIPLE_CULTURE_COLLECTION = "ONCALLER_MULTIPLE_CULTURE_COLLECTION";
static const string kDISC_SEGSETS_PRESENT = "DISC_SEGSETS_PRESENT";
static const string kDISC_NONWGS_SETS_PRESENT = "DISC_NONWGS_SETS_PRESENT";
static const string kDISC_FEATURE_LIST = "DISC_FEATURE_LIST";
static const string kDISC_CATEGORY_HEADER = "DISC_CATEGORY_HEADER";
static const string kDISC_MISMATCHED_COMMENTS = "DISC_MISMATCHED_COMMENTS";
static const string kDISC_STRAIN_TAXNAME_MISMATCH = "DISC_STRAIN_TAXNAME_MISMATCH";
static const string kDISC_HUMAN_HOST = "DISC_HUMAN_HOST";
static const string kDISC_BAD_BACTERIAL_GENE_NAME = "DISC_BAD_BACTERIAL_GENE_NAME";
static const string kTEST_BAD_GENE_NAME = "TEST_BAD_GENE_NAME";
static const string kONCALLER_ORDERED_LOCATION = "ONCALLER_ORDERED_LOCATION";
static const string kONCALLER_COMMENT_PRESENT = "ONCALLER_COMMENT_PRESENT";
static const string kONCALLER_DEFLINE_ON_SET = "ONCALLER_DEFLINE_ON_SET";
static const string kONCALLER_HIV_RNA_INCONSISTENT = "ONCALLER_HIV_RNA_INCONSISTENT";
static const string kSHORT_PROT_SEQUENCES = "SHORT_PROT_SEQUENCES";
static const string kTEST_EXON_ON_MRNA = "TEST_EXON_ON_MRNA";
static const string kTEST_HAS_PROJECT_ID = "TEST_HAS_PROJECT_ID";
static const string kONCALLER_HAS_STANDARD_NAME = "ONCALLER_HAS_STANDARD_NAME";
static const string kONCALLER_MISSING_STRUCTURED_COMMENTS = "ONCALLER_MISSING_STRUCTURED_COMMENTS";
static const string kDISC_REQUIRED_STRAIN = "DISC_REQUIRED_STRAIN";
static const string kMISSING_GENOMEASSEMBLY_COMMENTS = "MISSING_GENOMEASSEMBLY_COMMENTS";
static const string kDISC_BACTERIAL_TAX_STRAIN_MISMATCH = "DISC_BACTERIAL_TAX_STRAIN_MISMATCH";
static const string kTEST_CDS_HAS_CDD_XREF = "TEST_CDS_HAS_CDD_XREF";
static const string kTEST_UNUSUAL_NT = "TEST_UNUSUAL_NT";
static const string kTEST_LOW_QUALITY_REGION = "TEST_LOW_QUALITY_REGION";
static const string kTEST_ORGANELLE_NOT_GENOMIC = "TEST_ORGANELLE_NOT_GENOMIC";
static const string kTEST_UNWANTED_SPACER = "TEST_UNWANTED_SPACER";
static const string kTEST_ORGANELLE_PRODUCTS = "TEST_ORGANELLE_PRODUCTS";
static const string kTEST_SP_NOT_UNCULTURED = "TEST_SP_NOT_UNCULTURED";
static const string kTEST_BAD_MRNA_QUAL = "TEST_BAD_MRNA_QUAL";
static const string kTEST_UNNECESSARY_ENVIRONMENTAL = "TEST_UNNECESSARY_ENVIRONMENTAL";
static const string kTEST_UNNECESSARY_VIRUS_GENE = "TEST_UNNECESSARY_VIRUS_GENE";
static const string kTEST_UNWANTED_SET_WRAPPER = "TEST_UNWANTED_SET_WRAPPER";
static const string kTEST_MISSING_PRIMER = "TEST_MISSING_PRIMER";
static const string kTEST_UNUSUAL_MISC_RNA = "TEST_UNUSUAL_MISC_RNA";
static const string kTEST_AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE = "TEST_AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE";
static const string kTEST_DUP_GENES_OPPOSITE_STRANDS = "TEST_DUP_GENES_OPPOSITE_STRANDS";
static const string kTEST_SMALL_GENOME_SET_PROBLEM = "TEST_SMALL_GENOME_SET_PROBLEM";
static const string kTEST_OVERLAPPING_RRNAS = "TEST_OVERLAPPING_RRNAS";
static const string kTEST_MRNA_SEQUENCE_MINUS_STRAND_FEATURES = "TEST_MRNA_SEQUENCE_MINUS_STRAND_FEATURES";
static const string kTEST_TAXNAME_NOT_IN_DEFLINE = "TEST_TAXNAME_NOT_IN_DEFLINE";
static const string kTEST_COUNT_UNVERIFIED = "TEST_COUNT_UNVERIFIED";
static const string kSHOW_TRANSL_EXCEPT = "SHOW_TRANSL_EXCEPT";
static const string kSHOW_HYPOTHETICAL_CDS_HAVING_GENE_NAME = "SHOW_HYPOTHETICAL_CDS_HAVING_GENE_NAME";
static const string kTEST_DEFLINE_PRESENT = "TEST_DEFLINE_PRESENT";
static const string kTEST_MRNA_OVERLAPPING_PSEUDO_GENE = "TEST_MRNA_OVERLAPPING_PSEUDO_GENE";
static const string kFIND_OVERLAPPED_GENES = "FIND_OVERLAPPED_GENES";
static const string kDISC_BIOMATERIAL_TAXNAME_MISMATCH = "DISC_BIOMATERIAL_TAXNAME_MISMATCH";
static const string kDISC_CULTURE_TAXNAME_MISMATCH = "DISC_CULTURE_TAXNAME_MISMATCH";
static const string kDISC_CHECK_AUTH_NAME = "DISC_CHECK_AUTH_NAME";
static const string kNON_RETROVIRIDAE_PROVIRAL = "NON_RETROVIRIDAE_PROVIRAL";
static const string kRNA_PROVIRAL = "RNA_PROVIRAL";
static const string kSHORT_SEQUENCES_200 = "SHORT_SEQUENCES_200";
static const string kDISC_10_PERCENTN = "DISC_10_PERCENTN";
static const string kN_RUNS_14 = "N_RUNS_14";
static const string kMOLTYPE_NOT_MRNA = "MOLTYPE_NOT_MRNA";
static const string kTECHNIQUE_NOT_TSA = "TECHNIQUE_NOT_TSA";
static const string kMISSING_STRUCTURED_COMMENT = "MISSING_STRUCTURED_COMMENT";
static const string kMISSING_PROJECT = "MISSING_PROJECT";
static const string kMULTIPLE_CDS_ON_MRNA = "MULTIPLE_CDS_ON_MRNA";
static const string kDUP_DISC_CBS_CULTURE_CONFLICT = "DUP_DISC_CBS_CULTURE_CONFLICT";
static const string kDIVISION_CODE_CONFLICTS = "DIVISION_CODE_CONFLICTS";
static const string kRRNA_NAME_CONFLICTS = "RRNA_NAME_CONFLICTS";
static const string kEUKARYOTE_SHOULD_HAVE_MRNA = "EUKARYOTE_SHOULD_HAVE_MRNA";
static const string kMRNA_SHOULD_HAVE_PROTEIN_TRANSCRIPT_IDS = "MRNA_SHOULD_HAVE_PROTEIN_TRANSCRIPT_IDS";
static const string kONCALLER_COUNTRY_COLON = "ONCALLER_COUNTRY_COLON";
static const string kONCALLER_BIOPROJECT_ID = "ONCALLER_BIOPROJECT_ID";
static const string kONCALLER_STRAIN_TAXNAME_CONFLICT = "ONCALLER_STRAIN_TAXNAME_CONFLICT";
static const string kONCALLER_MORE_NAMES_COLLECTED_BY = "ONCALLER_MORE_NAMES_COLLECTED_BY";
static const string kONCALLER_MORE_OR_SPEC_NAMES_IDENTIFIED_BY = "ONCALLER_MORE_OR_SPEC_NAMES_IDENTIFIED_BY";
static const string kONCALLER_SUSPECTED_ORG_IDENTIFIED = "ONCALLER_SUSPECTED_ORG_IDENTIFIED";
static const string kONCALLER_SUSPECTED_ORG_COLLECTED = "ONCALLER_SUSPECTED_ORG_COLLECTED";
static const string kONCALLER_SWITCH_STRUCTURED_COMMENT_PREFIX = "ONCALLER_SWITCH_STRUCTURED_COMMENT_PREFIX";
static const string kONCALLER_CITSUB_AFFIL_DUP_TEXT = "ONCALLER_CITSUB_AFFIL_DUP_TEXT";
static const string kONCALLER_DUPLICATE_PRIMER_SET = "ONCALLER_DUPLICATE_PRIMER_SET";
static const string kEND_COLON_IN_COUNTRY = "END_COLON_IN_COUNTRY";
static const string kDISC_PROTEIN_NAMES = "DISC_PROTEIN_NAMES";
static const string kDISC_TITLE_ENDS_WITH_SEQUENCE = "DISC_TITLE_ENDS_WITH_SEQUENCE";
static const string kDISC_INCONSISTENT_STRUCTURED_COMMENTS = "DISC_INCONSISTENT_STRUCTURED_COMMENTS";
static const string kDISC_INCONSISTENT_DBLINK = "DISC_INCONSISTENT_DBLINK";
static const string kDISC_INCONSISTENT_MOLINFO_TECH = "DISC_INCONSISTENT_MOLINFO_TECH";
static const string kDISC_GAPS = "DISC_GAPS";
static const string kDISC_BAD_BGPIPE_QUALS = "DISC_BAD_BGPIPE_QUALS";
static const string kTEST_SHORT_LNCRNA = "TEST_SHORT_LNCRNA";
static const string kTEST_TERMINAL_NS = "TEST_TERMINAL_NS";
static const string kTEST_ALIGNMENT_HAS_SCORE = "TEST_ALIGNMENT_HAS_SCORE";
static const string kUNCULTURED_NOTES_ONCALLER = "UNCULTURED_NOTES_ONCALLER";
static const string kSEQ_ID_PHRASES = "SEQ_ID_PHRASES";
static const string kNO_PRODUCT_STRING = "NO_PRODUCT_STRING";


class CUnimplemented : public CAnalysisProcess
{
public:
    CUnimplemented(const string& test_name) : m_TestName(test_name) {}
    ~CUnimplemented() {}

    virtual void CollectData(CSeq_entry_Handle seh, const CReportMetadata& metadata) {}
    virtual void ResetData() { }
    virtual TReportItemList GetReportItems(CReportConfig& cfg) const;
    virtual vector<string> Handles() const { return x_HandlesOne(m_TestName); };
    virtual void DropReferences(CScope& scope) {};
protected:
    string m_TestName;
};


class CSimpleStorageProcess : public CAnalysisProcess
{
public:
    virtual void ResetData() { m_Objs.clear(); };
    virtual void DropReferences(CScope& scope);
protected:
    TReportObjectList m_Objs;
};


class CCountNucSeqs : public CSimpleStorageProcess
{
public:
    CCountNucSeqs() {};
    ~CCountNucSeqs() {};

    virtual void CollectData(CSeq_entry_Handle seh, const CReportMetadata& metadata);
    virtual TReportItemList GetReportItems(CReportConfig& cfg) const;
    virtual vector<string> Handles() const { return x_HandlesOne(kDISC_COUNT_NUCLEOTIDES);};

protected:
};


class COverlappingCDS : public CAnalysisProcess
{
public:
    COverlappingCDS() {};
    ~COverlappingCDS() {};

    virtual void CollectData(CSeq_entry_Handle seh, const CReportMetadata& metadata);
    virtual void ResetData() { m_ObjsNote.clear(); m_ObjsNoNote.clear(); };
    virtual TReportItemList GetReportItems(CReportConfig& cfg) const;
    virtual vector<string> Handles() const { return x_HandlesOne(kDISC_OVERLAPPING_CDS);};
    virtual void DropReferences(CScope& scope) { x_DropReferences(m_ObjsNote, scope); x_DropReferences(m_ObjsNoNote, scope); };
    virtual bool Autofix(CScope& scope);

    static bool HasOverlapNote(const CSeq_feat& feat);
    static bool SetOverlapNote(CSeq_feat& feat);

protected:
    TReportObjectList m_ObjsNote;
    TReportObjectList m_ObjsNoNote;

    bool x_ProductNamesAreSimilar(const string& product1, const string& product2);
    bool x_LocationsOverlapOnSameStrand(const CSeq_loc& loc1, const CSeq_loc& loc2, CScope* scope);
    bool x_ShouldIgnore(const string& product);
    void x_AddFeature(const CMappedFeat& f, const string& filename);
};


class CContainedCDS : public CAnalysisProcess
{
public:
    CContainedCDS() {};
    ~CContainedCDS() {};

    virtual void CollectData(CSeq_entry_Handle seh, const CReportMetadata& metadata);
    virtual void ResetData() { m_ObjsNote.clear(); m_ObjsSameStrand.clear(); m_ObjsDiffStrand.clear(); };
    virtual TReportItemList GetReportItems(CReportConfig& cfg) const;
    virtual vector<string> Handles() const { return x_HandlesOne(kDISC_CONTAINED_CDS);};
    virtual void DropReferences(CScope& scope) { 
        x_DropReferences(m_ObjsNote, scope); 
        x_DropReferences(m_ObjsSameStrand, scope); 
        x_DropReferences(m_ObjsDiffStrand, scope);};
    virtual bool Autofix(CScope& scope);

    static bool HasContainedNote(const CSeq_feat& feat);

protected:
    TReportObjectList m_ObjsNote;
    TReportObjectList m_ObjsSameStrand;
    TReportObjectList m_ObjsDiffStrand;

    void x_AddFeature(const CMappedFeat& f, const string& filename, bool same_strand);
};


END_SCOPE(DiscRepNmSpc);
END_NCBI_SCOPE

#endif 
