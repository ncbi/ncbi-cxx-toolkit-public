/*  $Id$
 * =========================================================================
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
 * =========================================================================
 *
 * Author:  Jie Chen
 *
 * File Description:
 *   Cpp Discrepany Report configuration
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <connect/ncbi_core_cxx.hpp>

// Objects includes
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

// Object Manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <serial/objistr.hpp>
#include <serial/serial.hpp>

#include <common/test_assert.h>

#include "hchecking_class.hpp"
#include "hauto_disc_class.hpp"
#include "hDiscRep_config.hpp"
#include "hDiscRep_tests.hpp"

using namespace ncbi;
using namespace objects;
using namespace DiscRepNmSpc;

static CDiscRepInfo thisInfo;
static string       strtmp;

vector < CRef <CTestAndRepData> > CRepConfig :: tests_on_Bioseq;
vector < CRef <CTestAndRepData> > CRepConfig :: tests_on_Bioseq_aa;
vector < CRef <CTestAndRepData> > CRepConfig :: tests_on_Bioseq_na;
vector < CRef <CTestAndRepData> > CRepConfig :: tests_on_Bioseq_CFeat;
vector < CRef <CTestAndRepData> > CRepConfig :: tests_on_Bioseq_CFeat_NotInGenProdSet;
vector < CRef <CTestAndRepData> > CRepConfig :: tests_on_Bioseq_NotInGenProdSet;
vector < CRef <CTestAndRepData> > CRepConfig :: tests_on_Bioseq_CFeat_CSeqdesc;
vector < CRef <CTestAndRepData> > CRepConfig :: tests_on_SeqEntry;
vector < CRef <CTestAndRepData> > CRepConfig :: tests_on_SeqEntry_feat_desc;
vector < CRef <CTestAndRepData> > CRepConfig :: tests_4_once;
vector < CRef <CTestAndRepData> > CRepConfig :: tests_on_BioseqSet;
vector < CRef <CTestAndRepData> > CRepConfig :: tests_on_SubmitBlk;
set <string> CDiscTestInfo :: tests_run;

static CDiscTestInfo thisTest;
static const s_test_property test_list[] = {
  {"SUSPECT_PRODUCT_NAMES", fDiscrepancy}
};

static const s_test_property test1_list[] = {
// tests_on_SubmitBlk
   {"DISC_SUBMITBLOCK_CONFLICT", fDiscrepancy | fOncaller},

// tests_on_Bioseq
   {"DISC_COUNT_NUCLEOTIDES", fDiscrepancy | fOncaller},
   {"DISC_QUALITY_SCORES", fDiscrepancy},
   {"DISC_FEATURE_COUNT_oncaller", fOncaller},  // oncaller tool version
   
// tests_on_Bioseq_aa
   {"COUNT_PROTEINS", fDiscrepancy},
   {"MISSING_PROTEIN_ID1", fDiscrepancy},
   {"MISSING_PROTEIN_ID", fDiscrepancy},
   {"INCONSISTENT_PROTEIN_ID_PREFIX1", fDiscrepancy},
//   {"INCONSISTENT_PROTEIN_ID_PREFIX", fDiscrepancy},

// tests_on_Bioseq_na
   {"TEST_DEFLINE_PRESENT", fDiscrepancy},
   {"N_RUNS", fDiscrepancy},
   {"N_RUNS_14", fDiscrepancy},
   {"ZERO_BASECOUNT", fDiscrepancy},
   {"TEST_LOW_QUALITY_REGION", fDiscrepancy},
   {"DISC_PERCENT_N", fDiscrepancy},
   {"DISC_10_PERCENTN", fDiscrepancy},
   {"TEST_UNUSUAL_NT", fDiscrepancy},

// tests_on_Bioseq_CFeat
   {"SUSPECT_PHRASES", fDiscrepancy},
   {"DISC_SUSPECT_RRNA_PRODUCTS", fDiscrepancy},
   {"SUSPECT_PRODUCT_NAMES", fDiscrepancy},
   {"DISC_PRODUCT_NAME_TYPO", fDiscrepancy},
   {"DISC_PRODUCT_NAME_QUICKFIX", fDiscrepancy},
   {"TEST_ORGANELLE_PRODUCTS", fDiscrepancy},
   {"DISC_GAPS", fDiscrepancy},
   {"TEST_MRNA_OVERLAPPING_PSEUDO_GENE", fDiscrepancy},
   {"ONCALLER_HAS_STANDARD_NAME", fDiscrepancy},
   {"ONCALLER_ORDERED_LOCATION", fDiscrepancy},
   {"DISC_FEATURE_LIST", fDiscrepancy},
   {"TEST_CDS_HAS_CDD_XREF", fDiscrepancy},
   {"DISC_CDS_HAS_NEW_EXCEPTION", fDiscrepancy},
   {"DISC_MICROSATELLITE_REPEAT_TYPE", fDiscrepancy},
   {"DISC_SUSPECT_MISC_FEATURES", fDiscrepancy},
   {"DISC_CHECK_RNA_PRODUCTS_AND_COMMENTS", fDiscrepancy},
   {"DISC_FEATURE_MOLTYPE_MISMATCH", fDiscrepancy},
   {"ADJACENT_PSEUDOGENES", fDiscrepancy},
   {"MISSING_GENPRODSET_PROTEIN", fDiscrepancy},
   {"DUP_GENPRODSET_PROTEIN", fDiscrepancy},
   {"MISSING_GENPRODSET_TRANSCRIPT_ID", fDiscrepancy},
   {"DISC_DUP_GENPRODSET_TRANSCRIPT_ID", fDiscrepancy},
   {"DISC_FEAT_OVERLAP_SRCFEAT", fDiscrepancy},
   {"CDS_TRNA_OVERLAP", fDiscrepancy},
   {"TRANSL_NO_NOTE", fDiscrepancy},
   {"NOTE_NO_TRANSL", fDiscrepancy},
   {"TRANSL_TOO_LONG", fDiscrepancy},
   {"FIND_STRAND_TRNAS", fDiscrepancy},
   {"FIND_BADLEN_TRNAS", fDiscrepancy},
   {"COUNT_TRNAS", fDiscrepancy},
   {"FIND_DUP_TRNAS", fDiscrepancy},
   {"COUNT_RRNAS", fDiscrepancy},
   {"FIND_DUP_RRNAS", fDiscrepancy},
   {"PARTIAL_CDS_COMPLETE_SEQUENCE", fDiscrepancy},
   {"CONTAINED_CDS", fDiscrepancy},
   {"PSEUDO_MISMATCH", fDiscrepancy},
   {"EC_NUMBER_NOTE", fDiscrepancy},
   {"NON_GENE_LOCUS_TAG", fDiscrepancy},
   {"JOINED_FEATURES", fDiscrepancy},
   {"SHOW_TRANSL_EXCEPT", fDiscrepancy},
   {"MRNA_SHOULD_HAVE_PROTEIN_TRANSCRIPT_IDS", fDiscrepancy},
   {"RRNA_NAME_CONFLICTS", fDiscrepancy},
   {"ONCALLER_GENE_MISSING", fDiscrepancy},
   {"ONCALLER_SUPERFLUOUS_GENE", fDiscrepancy},
   {"MISSING_GENES", fDiscrepancy},
   {"EXTRA_GENES", fDiscrepancy},
   //tests_on_Bioseq_CFeat.push_back(CRef <CTestAndRepData>(new CBioseq_EXTRA_MISSING_GENES"));
   {"OVERLAPPING_CDS", fDiscrepancy},
   {"RNA_CDS_OVERLAP", fDiscrepancy},
   {"FIND_OVERLAPPED_GENES", fDiscrepancy},
   {"OVERLAPPING_GENES", fDiscrepancy},
   {"DISC_PROTEIN_NAMES", fDiscrepancy},
   {"DISC_CDS_PRODUCT_FIND", fDiscrepancy},
   {"EC_NUMBER_ON_UNKNOWN_PROTEIN", fDiscrepancy},
   {"RNA_NO_PRODUCT", fDiscrepancy},
   {"DISC_SHORT_INTRON", fDiscrepancy},
   {"DISC_BAD_GENE_STRAND", fDiscrepancy},
   {"DISC_INTERNAL_TRANSCRIBED_SPACER_RRNA", fDiscrepancy},
   {"DISC_SHORT_RRNA", fDiscrepancy},
   {"TEST_OVERLAPPING_RRNAS", fDiscrepancy},
   {"HYPOTHETICAL_CDS_HAVING_GENE_NAME", fDiscrepancy},
   {"DISC_SUSPICIOUS_NOTE_TEXT", fDiscrepancy},
   {"NO_ANNOTATION", fDiscrepancy},
   {"DISC_LONG_NO_ANNOTATION", fDiscrepancy},
   {"DISC_PARTIAL_PROBLEMS", fDiscrepancy},
   {"TEST_UNUSUAL_MISC_RNA", fDiscrepancy},
   {"GENE_PRODUCT_CONFLICT", fDiscrepancy},
   {"DISC_CDS_WITHOUT_MRNA", fDiscrepancy},

// tests_on_Bioseq_CFeat_NotInGenProdSet
   {"DUPLICATE_GENE_LOCUS", fDiscrepancy},
   {"LOCUS_TAGS", fDiscrepancy},
   {"FEATURE_LOCATION_CONFLICT", fDiscrepancy},

// tests_on_Bioseq_CFeat_CSeqdesc
   {"DISC_INCONSISTENT_MOLINFO_TECH", fDiscrepancy},
   {"SHORT_CONTIG", fDiscrepancy},
   {"SHORT_SEQUENCES", fDiscrepancy},
   {"SHORT_SEQUENCES_200", fDiscrepancy},
   {"TEST_UNWANTED_SPACER", fDiscrepancy},
   {"TEST_UNNECESSARY_VIRUS_GENE", fDiscrepancy},
   {"TEST_ORGANELLE_NOT_GENOMIC", fDiscrepancy},
   {"MULTIPLE_CDS_ON_MRNA", fDiscrepancy},
   {"TEST_MRNA_SEQUENCE_MINUS_ST", fDiscrepancy},
   {"TEST_BAD_MRNA_QUAL", fDiscrepancy},
   {"TEST_EXON_ON_MRNA", fDiscrepancy},
   {"ONCALLER_HIV_RNA_INCONSISTENT", fDiscrepancy},
   {"DISC_BACTERIAL_PARTIAL_NONEXTENDABLE_EXCEPTION", fDiscrepancy},
   {"DISC_BACTERIAL_PARTIAL_NONEXTENDABLE_PROBLEMS", fDiscrepancy},
   {"DISC_MITOCHONDRION_REQUIRED", fDiscrepancy},
   {"EUKARYOTE_SHOULD_HAVE_MRNA", fDiscrepancy},
   {"RNA_PROVIRAL", fDiscrepancy},
   {"NON_RETROVIRIDAE_PROVIRAL", fDiscrepancy},
   {"DISC_RETROVIRIDAE_DNA", fDiscrepancy},
   {"DISC_mRNA_ON_WRONG_SEQUENCE_TYPE", fDiscrepancy},
   {"DISC_RBS_WITHOUT_GENE", fDiscrepancy},
   {"DISC_EXON_INTRON_CONFLICT", fDiscrepancy},
   {"TEST_TAXNAME_NOT_IN_DEFLINE", fDiscrepancy},
   {"INCONSISTENT_SOURCE_DEFLINE", fDiscrepancy},
   {"DISC_BACTERIA_SHOULD_NOT_HAVE_MRNA", fDiscrepancy},
   {"TEST_BAD_GENE_NAME", fDiscrepancy},
   {"MOLTYPE_NOT_MRNA", fDiscrepancy},
   {"TECHNIQUE_NOT_TSA", fDiscrepancy},
   {"SHORT_PROT_SEQUENCES", fDiscrepancy},
   {"TEST_COUNT_UNVERIFIED", fDiscrepancy},
   {"TEST_DUP_GENES_OPPOSITE_STRANDS", fDiscrepancy},
   {"DISC_GENE_PARTIAL_CONFLICT", fDiscrepancy},

// tests_on_SeqEntry
   {"DISC_FLATFILE_FIND_ONCALLER", fDiscrepancy},
   {"DISC_FEATURE_COUNT", fDiscrepancy}, // asndisc version   

// tests_on_SeqEntry_feat_desc: all CSeqEntry_Feat_desc tests need RmvRedundancy
   {"DISC_INCONSISTENT_STRUCTURED_COMMENTS", fDiscrepancy},
   {"DISC_INCONSISTENT_DBLINK", fDiscrepancy},
   {"END_COLON_IN_COUNTRY", fDiscrepancy},
   {"ONCALLER_SUSPECTED_ORG_COLLECTED", fDiscrepancy},
   {"ONCALLER_SUSPECTED_ORG_IDENTIFIED", fDiscrepancy},
   {"ONCALLER_MORE_NAMES_COLLECTED_BY", fDiscrepancy},
   {"ONCALLER_STRAIN_TAXNAME_CONFLICT", fDiscrepancy},
   {"TEST_SMALL_GENOME_SET_PROBLEM", fDiscrepancy},
   {"DISC_INCONSISTENT_MOLTYPES", fDiscrepancy},
   {"DISC_BIOMATERIAL_TAXNAME_MISMATCH", fDiscrepancy},
   {"DISC_CULTURE_TAXNAME_MISMATCH", fDiscrepancy},
   {"DISC_STRAIN_TAXNAME_MISMATCH", fDiscrepancy},
   {"DISC_SPECVOUCHER_TAXNAME_MISMATCH", fDiscrepancy},
   {"DISC_HAPLOTYPE_MISMATCH", fDiscrepancy},
   {"DISC_MISSING_VIRAL_QUALS", fDiscrepancy},
   {"DISC_INFLUENZA_DATE_MISMATCH", fDiscrepancy},
   {"TAX_LOOKUP_MISSING", fDiscrepancy},
   {"TAX_LOOKUP_MISMATCH", fDiscrepancy},
   {"MISSING_STRUCTURED_COMMENT", fDiscrepancy},
   {"ONCALLER_MISSING_STRUCTURED_COMMENTS", fDiscrepancy},
   {"DISC_MISSING_AFFIL", fDiscrepancy},
   {"DISC_CITSUBAFFIL_CONFLICT", fDiscrepancy},
   {"DISC_TITLE_AUTHOR_CONFLICT", fDiscrepancy},
   {"DISC_USA_STATE", fDiscrepancy},
   {"DISC_CITSUB_AFFIL_DUP_TEXT", fDiscrepancy},
   {"DISC_REQUIRED_CLONE", fDiscrepancy},
   {"ONCALLER_MULTISRC", fDiscrepancy},
   {"DISC_SRC_QUAL_PROBLEM", fDiscrepancy},
   {"DISC_SOURCE_QUALS_ASNDISC", fDiscrepancy}, // asndisc version
   {"DISC_UNPUB_PUB_WITHOUT_TITLE", fDiscrepancy},
   {"ONCALLER_CONSORTIUM", fDiscrepancy},
   {"DISC_CHECK_AUTH_NAME", fDiscrepancy},
   {"DISC_CHECK_AUTH_CAPS", fDiscrepancy},
   {"DISC_MISMATCHED_COMMENTS", fDiscrepancy},
   {"ONCALLER_COMMENT_PRESENT", fDiscrepancy},
   {"DUP_DISC_ATCC_CULTURE_CONFLICT", fDiscrepancy},
   {"ONCALLER_STRAIN_CULTURE_COLLECTION_MISMATCH", fDiscrepancy},
   {"DISC_DUP_DEFLINE", fDiscrepancy},
   {"DISC_TITLE_ENDS_WITH_SEQUENCE", fDiscrepancy},
   {"DISC_MISSING_DEFLINES", fDiscrepancy},
   {"ONCALLER_DEFLINE_ON_SET", fDiscrepancy},
   {"DISC_BACTERIAL_TAX_STRAIN_MISMATCH", fDiscrepancy},
   {"DUP_DISC_CBS_CULTURE_CONFLICT", fDiscrepancy},
   {"INCONSISTENT_BIOSOURCE", fDiscrepancy},
   {"ONCALLER_BIOPROJECT_ID", fDiscrepancy},
   {"ONCALLER_SWITCH_STRUCTURED_COMMENT_PREFIX", fDiscrepancy},
   {"MISSING_GENOMEASSEMBLY_COMMENTS", fDiscrepancy},
   {"TEST_HAS_PROJECT_ID", fDiscrepancy},
   {"DISC_TRINOMIAL_SHOULD_HAVE_QUALIFIER", fDiscrepancy},
   {"ONCALLER_DUPLICATE_PRIMER_SET", fDiscrepancy},
   {"ONCALLER_COUNTRY_COLON", fDiscrepancy},
   {"TEST_MISSING_PRIMER", fDiscrepancy},
   {"TEST_SP_NOT_UNCULTURED", fDiscrepancy},
   {"DISC_METAGENOMIC", fDiscrepancy},
   {"DISC_MAP_CHROMOSOME_CONFLICT", fDiscrepancy},
   {"DIVISION_CODE_CONFLICTS", fDiscrepancy},
   {"TEST_AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE", fDiscrepancy},
   {"TEST_UNNECESSARY_ENVIRONMENTAL", fDiscrepancy},
   {"DISC_HUMAN_HOST", fDiscrepancy},
   {"ONCALLER_MULTIPLE_CULTURE_COLLECTION", fDiscrepancy},
   {"ONCALLER_CHECK_AUTHORITY", fDiscrepancy},
   {"DISC_METAGENOME_SOURCE", fDiscrepancy},
   {"DISC_BACTERIA_MISSING_STRAIN", fDiscrepancy},
   {"DISC_REQUIRED_STRAIN", fDiscrepancy},
   {"MORE_OR_SPEC_NAMES_IDENTIFIED_BY", fDiscrepancy},
   {"MORE_NAMES_COLLECTED_BY", fDiscrepancy},
   {"MISSING_PROJECT", fDiscrepancy},
   {"DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE", fDiscrepancy},
   {"DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE1", fDiscrepancy}, // not tested

// tests_on_BioseqSet   // redundant because of nested set?
   {"DISC_SEGSETS_PRESENT", fDiscrepancy},
   {"TEST_UNWANTED_SET_WRAPPER", fDiscrepancy},
   {"DISC_NONWGS_SETS_PRESENT", fDiscrepancy}
};

CRepConfig* CRepConfig :: factory(const string& report_tp)
{
   if (report_tp == "Discrepancy") {
         return new CRepConfDiscrepancy();
         thisInfo.output_config.use_flag = true; // output flags
   }
   else if(report_tp ==  "Oncaller") return new  CRepConfOncaller();
   else {
      NCBI_THROW(CException, eUnknown, "Unrecognized report type.");
      return 0;
   }
};  // CRepConfig::factory()

void CRepConfig :: CollectTests() 
{
   unsigned sz = thisTest.tests_run.size(), i=0;
   if (i >= sz) return;
   if (thisTest.tests_run.find("DISC_SUBMITBLOCK_CONFLICT") != thisTest.tests_run.end()) {
        tests_on_SubmitBlk.push_back(
                      CRef <CTestAndRepData>(new CSeqEntry_DISC_SUBMITBLOCK_CONFLICT));
        if (++i >= sz) return;
   }
   if (thisTest.tests_run.find("DISC_COUNT_NUCLEOTIDES") != thisTest.tests_run.end()) {
        tests_on_Bioseq.push_back(CRef <CTestAndRepData>(new CBioseq_DISC_COUNT_NUCLEOTIDES));
        if (++i >= sz) return;
   }
   if (thisTest.tests_run.find("DISC_QUALITY_SCORES") != thisTest.tests_run.end()) {
        tests_on_Bioseq.push_back(CRef <CTestAndRepData>(new CBioseq_DISC_QUALITY_SCORES));
        thisInfo.test_item_list["DISC_QUALITY_SCORES"].clear();
        if (++i >= sz) return;
   }
   if (thisTest.tests_run.find("DISC_FEATURE_COUNT_oncaller") != thisTest.tests_run.end()) {
        tests_on_Bioseq.push_back(CRef <CTestAndRepData>(new CBioseq_DISC_FEATURE_COUNT)); 
        if (++i >= sz) return;
   } 
   if ( thisTest.tests_run.find("COUNT_PROTEINS") != thisTest.tests_run.end()) {
        tests_on_Bioseq_aa.push_back(CRef <CTestAndRepData>(new CBioseq_COUNT_PROTEINS));
        if (++i >= sz) return;
   } 
   if ( thisTest.tests_run.find("MISSING_PROTEIN_ID1") != thisTest.tests_run.end()) {
        tests_on_Bioseq_aa.push_back(CRef <CTestAndRepData>(new CBioseq_MISSING_PROTEIN_ID1));
        tests_on_Bioseq_aa.push_back(CRef <CTestAndRepData>(new CBioseq_MISSING_PROTEIN_ID));
i += 2;
if (i > sz) return;

//        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("INCONSISTENT_PROTEIN_ID_PREFIX1") != thisTest.tests_run.end()) {
        tests_on_Bioseq_aa.push_back(
                   CRef <CTestAndRepData>(new CBioseq_INCONSISTENT_PROTEIN_ID_PREFIX1));
/*
        tests_on_Bioseq_aa.push_back(
                   CRef <CTestAndRepData>(new CBioseq_INCONSISTENT_PROTEIN_ID_PREFIX));
i += 2;
if (i > sz) return;
*/
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_DEFLINE_PRESENT") != thisTest.tests_run.end()) {
         tests_on_Bioseq_na.push_back(
                     CRef <CTestAndRepData>(new CBioseq_TEST_DEFLINE_PRESENT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("N_RUNS") != thisTest.tests_run.end()) {
        tests_on_Bioseq_na.push_back(CRef <CTestAndRepData>(new CBioseq_N_RUNS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("N_RUNS_14") != thisTest.tests_run.end()) {
       tests_on_Bioseq_na.push_back(CRef <CTestAndRepData>(new CBioseq_N_RUNS_14));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ZERO_BASECOUNT") != thisTest.tests_run.end()) {
       tests_on_Bioseq_na.push_back(CRef <CTestAndRepData>(new CBioseq_ZERO_BASECOUNT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_LOW_QUALITY_REGION") != thisTest.tests_run.end()) {
       tests_on_Bioseq_na.push_back(
                     CRef <CTestAndRepData>(new CBioseq_TEST_LOW_QUALITY_REGION));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_PERCENT_N") != thisTest.tests_run.end()) { 
       tests_on_Bioseq_na.push_back(CRef <CTestAndRepData>(new CBioseq_DISC_PERCENT_N));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_10_PERCENTN") != thisTest.tests_run.end()) {
       tests_on_Bioseq_na.push_back(CRef <CTestAndRepData>(new CBioseq_DISC_10_PERCENTN));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_UNUSUAL_NT") != thisTest.tests_run.end()) {
       tests_on_Bioseq_na.push_back(CRef <CTestAndRepData>(new CBioseq_TEST_UNUSUAL_NT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("SUSPECT_PHRASES") != thisTest.tests_run.end()) {
        tests_on_Bioseq.push_back(CRef <CTestAndRepData>(new CBioseq_SUSPECT_PHRASES));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_SUSPECT_RRNA_PRODUCTS") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back( 
                CRef <CTestAndRepData> (new CBioseq_DISC_SUSPECT_RRNA_PRODUCTS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("SUSPECT_PRODUCT_NAMES") != thisTest.tests_run.end()
        || thisTest.tests_run.find("DISC_PRODUCT_NAME_TYPO") != thisTest.tests_run.end()
        || thisTest.tests_run.find("DISC_PRODUCT_NAME_QUICKFIX") != thisTest.tests_run.end()) {
        
        if (thisTest.tests_run.find("SUSPECT_PRODUCT_NAMES") != thisTest.tests_run.end()) i++;
        if (thisTest.tests_run.find("DISC_PRODUCT_NAME_TYPO") != thisTest.tests_run.end()) i++;
        if (thisTest.tests_run.find("DISC_PRODUCT_NAME_QUICKFIX") != thisTest.tests_run.end()) 
             i++;
        tests_on_Bioseq_CFeat_NotInGenProdSet.push_back(
                                CRef <CTestAndRepData>(new CBioseq_on_SUSPECT_RULE()));
        if (i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_ORGANELLE_PRODUCTS") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back( 
                CRef <CTestAndRepData> (new CBioseq_TEST_ORGANELLE_PRODUCTS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_GAPS") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back( CRef <CTestAndRepData> (new CBioseq_DISC_GAPS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_MRNA_OVERLAPPING_PSEUDO_GENE") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back( 
                CRef <CTestAndRepData> (new CBioseq_TEST_MRNA_OVERLAPPING_PSEUDO_GENE));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_HAS_STANDARD_NAME") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back( 
                   CRef <CTestAndRepData> (new CBioseq_ONCALLER_HAS_STANDARD_NAME));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_ORDERED_LOCATION") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back( 
                   CRef <CTestAndRepData> (new CBioseq_ONCALLER_ORDERED_LOCATION));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_FEATURE_LIST") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back( 
                    CRef <CTestAndRepData> (new CBioseq_DISC_FEATURE_LIST));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_CDS_HAS_CDD_XREF") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_TEST_CDS_HAS_CDD_XREF));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_CDS_HAS_NEW_EXCEPTION") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_DISC_CDS_HAS_NEW_EXCEPTION));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_MICROSATELLITE_REPEAT_TYPE") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
             CRef <CTestAndRepData> (new CBioseq_DISC_MICROSATELLITE_REPEAT_TYPE));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_SUSPECT_MISC_FEATURES") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_DISC_SUSPECT_MISC_FEATURES));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_CHECK_RNA_PRODUCTS_AND_COMMENTS") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
             CRef <CTestAndRepData> (new CBioseq_DISC_CHECK_RNA_PRODUCTS_AND_COMMENTS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_FEATURE_MOLTYPE_MISMATCH") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                CRef <CTestAndRepData> (new CBioseq_DISC_FEATURE_MOLTYPE_MISMATCH));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ADJACENT_PSEUDOGENES") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                         CRef <CTestAndRepData> (new CBioseq_ADJACENT_PSEUDOGENES));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("MISSING_GENPRODSET_PROTEIN") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                CRef <CTestAndRepData> (new CBioseq_MISSING_GENPRODSET_PROTEIN));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DUP_GENPRODSET_PROTEIN") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                      CRef <CTestAndRepData> (new CBioseq_DUP_GENPRODSET_PROTEIN));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("MISSING_GENPRODSET_TRANSCRIPT_ID") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                CRef <CTestAndRepData> (new CBioseq_MISSING_GENPRODSET_TRANSCRIPT_ID));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_DUP_GENPRODSET_TRANSCRIPT_ID") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                CRef <CTestAndRepData> (new CBioseq_DISC_DUP_GENPRODSET_TRANSCRIPT_ID));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_FEAT_OVERLAP_SRCFEAT") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                      CRef <CTestAndRepData> (new CBioseq_DISC_FEAT_OVERLAP_SRCFEAT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("CDS_TRNA_OVERLAP") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_CDS_TRNA_OVERLAP));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TRANSL_NO_NOTE") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_TRANSL_NO_NOTE));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("NOTE_NO_TRANSL") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_NOTE_NO_TRANSL));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TRANSL_TOO_LONG") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_TRANSL_TOO_LONG));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("FIND_STRAND_TRNAS") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_FIND_STRAND_TRNAS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("FIND_BADLEN_TRNAS") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_FIND_BADLEN_TRNAS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("COUNT_TRNAS") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_COUNT_TRNAS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("FIND_DUP_TRNAS") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_FIND_DUP_TRNAS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("COUNT_RRNAS") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_COUNT_RRNAS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("FIND_DUP_RRNAS") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_FIND_DUP_RRNAS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("PARTIAL_CDS_COMPLETE_SEQUENCE") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
           CRef <CTestAndRepData> (new CBioseq_PARTIAL_CDS_COMPLETE_SEQUENCE));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("CONTAINED_CDS") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_CONTAINED_CDS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("PSEUDO_MISMATCH") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_PSEUDO_MISMATCH));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("EC_NUMBER_NOTE") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_EC_NUMBER_NOTE));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("NON_GENE_LOCUS_TAG") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                         CRef <CTestAndRepData> (new CBioseq_NON_GENE_LOCUS_TAG));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("JOINED_FEATURES") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                        CRef <CTestAndRepData> (new CBioseq_JOINED_FEATURES));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("SHOW_TRANSL_EXCEPT") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                         CRef <CTestAndRepData>( new CBioseq_SHOW_TRANSL_EXCEPT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("MRNA_SHOULD_HAVE_PROTEIN_TRANSCRIPT_IDS") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
          CRef <CTestAndRepData>( new CBioseq_MRNA_SHOULD_HAVE_PROTEIN_TRANSCRIPT_IDS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("RRNA_NAME_CONFLICTS") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                         CRef <CTestAndRepData>(new CBioseq_RRNA_NAME_CONFLICTS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_GENE_MISSING") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                         CRef <CTestAndRepData>(new CBioseq_ONCALLER_GENE_MISSING));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_SUPERFLUOUS_GENE") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                         CRef <CTestAndRepData>(new CBioseq_ONCALLER_SUPERFLUOUS_GENE));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("MISSING_GENES") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
              CRef <CTestAndRepData>(new CBioseq_MISSING_GENES));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("EXTRA_GENES") != thisTest.tests_run.end()) { 
       tests_on_Bioseq_CFeat.push_back(CRef <CTestAndRepData>(new CBioseq_EXTRA_GENES));
       //???tests_on_Bioseq_CFeat.push_back(CRef <CTestAndRepData>(new CBioseq_EXTRA_MISSING_GENES));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("OVERLAPPING_CDS") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(CRef <CTestAndRepData>(new CBioseq_OVERLAPPING_CDS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("RNA_CDS_OVERLAP") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(CRef <CTestAndRepData>(new CBioseq_RNA_CDS_OVERLAP));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("FIND_OVERLAPPED_GENES") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                         CRef <CTestAndRepData>(new CBioseq_FIND_OVERLAPPED_GENES));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("OVERLAPPING_GENES") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                         CRef <CTestAndRepData>(new CBioseq_OVERLAPPING_GENES));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_PROTEIN_NAMES") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                   CRef <CTestAndRepData>( new CBioseq_DISC_PROTEIN_NAMES));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_CDS_PRODUCT_FIND") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                   CRef <CTestAndRepData>( new CBioseq_DISC_CDS_PRODUCT_FIND));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("EC_NUMBER_ON_UNKNOWN_PROTEIN") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                   CRef <CTestAndRepData>( new CBioseq_EC_NUMBER_ON_UNKNOWN_PROTEIN));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("RNA_NO_PRODUCT") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                   CRef <CTestAndRepData>(new CBioseq_RNA_NO_PRODUCT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_SHORT_INTRON") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                         CRef <CTestAndRepData>(new CBioseq_DISC_SHORT_INTRON));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_BAD_GENE_STRAND") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                         CRef <CTestAndRepData>(new CBioseq_DISC_BAD_GENE_STRAND));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_INTERNAL_TRANSCRIBED_SPACER_RRNA") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
             CRef <CTestAndRepData>(new CBioseq_DISC_INTERNAL_TRANSCRIBED_SPACER_RRNA));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_SHORT_RRNA") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                        CRef <CTestAndRepData>(new CBioseq_DISC_SHORT_RRNA));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_OVERLAPPING_RRNAS") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                        CRef <CTestAndRepData>(new CBioseq_TEST_OVERLAPPING_RRNAS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("HYPOTHETICAL_CDS_HAVING_GENE_NAME") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
               CRef <CTestAndRepData>( new CBioseq_HYPOTHETICAL_CDS_HAVING_GENE_NAME));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_SUSPICIOUS_NOTE_TEXT") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                  CRef <CTestAndRepData>( new CBioseq_DISC_SUSPICIOUS_NOTE_TEXT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("NO_ANNOTATION") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back(
                           CRef <CTestAndRepData>(new CBioseq_NO_ANNOTATION));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_LONG_NO_ANNOTATION") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back( 
              CRef <CTestAndRepData>( new CBioseq_DISC_LONG_NO_ANNOTATION));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_PARTIAL_PROBLEMS") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back( 
              CRef <CTestAndRepData>( new CBioseq_DISC_PARTIAL_PROBLEMS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_UNUSUAL_MISC_RNA") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back( 
              CRef <CTestAndRepData>( new CBioseq_TEST_UNUSUAL_MISC_RNA));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("GENE_PRODUCT_CONFLICT") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back( 
                     CRef <CTestAndRepData>( new CBioseq_GENE_PRODUCT_CONFLICT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_CDS_WITHOUT_MRNA") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat.push_back( 
                     CRef <CTestAndRepData>( new CBioseq_DISC_CDS_WITHOUT_MRNA));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DUPLICATE_GENE_LOCUS") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat_NotInGenProdSet.push_back(
                          CRef <CTestAndRepData>(new CBioseq_DUPLICATE_GENE_LOCUS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("LOCUS_TAGS") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat_NotInGenProdSet.push_back(
                                   CRef <CTestAndRepData>(new CBioseq_LOCUS_TAGS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("FEATURE_LOCATION_CONFLICT") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat_NotInGenProdSet.push_back(
                          CRef <CTestAndRepData>(new CBioseq_FEATURE_LOCATION_CONFLICT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_INCONSISTENT_MOLINFO_TECH") != thisTest.tests_run.end()) {
       tests_on_Bioseq_NotInGenProdSet.push_back(
                     CRef<CTestAndRepData>(new CBioseq_DISC_INCONSISTENT_MOLINFO_TECH));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("SHORT_CONTIG") != thisTest.tests_run.end()) {
       tests_on_Bioseq_NotInGenProdSet.push_back(
                                 CRef<CTestAndRepData>(new CBioseq_SHORT_CONTIG));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("SHORT_SEQUENCES") != thisTest.tests_run.end()) {
       tests_on_Bioseq_NotInGenProdSet.push_back(
                                     CRef<CTestAndRepData>(new CBioseq_SHORT_SEQUENCES));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("SHORT_SEQUENCES_200") != thisTest.tests_run.end()) {
       tests_on_Bioseq_NotInGenProdSet.push_back(
                                CRef<CTestAndRepData>(new CBioseq_SHORT_SEQUENCES_200));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_UNWANTED_SPACER") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                     CRef <CTestAndRepData>(new CBioseq_TEST_UNWANTED_SPACER));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_UNNECESSARY_VIRUS_GENE") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                 CRef <CTestAndRepData>(new CBioseq_TEST_UNNECESSARY_VIRUS_GENE));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_ORGANELLE_NOT_GENOMIC") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                 CRef <CTestAndRepData>(new CBioseq_TEST_ORGANELLE_NOT_GENOMIC));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("MULTIPLE_CDS_ON_MRNA") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                    CRef <CTestAndRepData>(new CBioseq_MULTIPLE_CDS_ON_MRNA));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_MRNA_SEQUENCE_MINUS_ST") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                        CRef <CTestAndRepData>(new CBioseq_TEST_MRNA_SEQUENCE_MINUS_ST));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_BAD_MRNA_QUAL") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                            CRef <CTestAndRepData>(new CBioseq_TEST_BAD_MRNA_QUAL));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_EXON_ON_MRNA") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                          CRef <CTestAndRepData>(new CBioseq_TEST_EXON_ON_MRNA));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_HIV_RNA_INCONSISTENT") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                     CRef <CTestAndRepData>( new CBioseq_ONCALLER_HIV_RNA_INCONSISTENT));
        if (++i >= sz) return;
   }
   if(thisTest.tests_run.find("DISC_BACTERIAL_PARTIAL_NONEXTENDABLE_EXCEPTION")
                                                             != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(CRef <CTestAndRepData>( 
                           new CBioseq_DISC_BACTERIAL_PARTIAL_NONEXTENDABLE_EXCEPTION));
        if (++i >= sz) return;
   }
   if (thisTest.tests_run.find("DISC_BACTERIAL_PARTIAL_NONEXTENDABLE_PROBLEMS")
                                                             != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(CRef <CTestAndRepData>(
                            new CBioseq_DISC_BACTERIAL_PARTIAL_NONEXTENDABLE_PROBLEMS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_MITOCHONDRION_REQUIRED") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                    CRef <CTestAndRepData>( new CBioseq_DISC_MITOCHONDRION_REQUIRED));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("EUKARYOTE_SHOULD_HAVE_MRNA") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                     CRef <CTestAndRepData>( new CBioseq_EUKARYOTE_SHOULD_HAVE_MRNA));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("RNA_PROVIRAL") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                         CRef <CTestAndRepData>( new CBioseq_RNA_PROVIRAL));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("NON_RETROVIRIDAE_PROVIRAL") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                    CRef <CTestAndRepData>( new CBioseq_NON_RETROVIRIDAE_PROVIRAL));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_RETROVIRIDAE_DNA") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                        CRef <CTestAndRepData>( new CBioseq_DISC_RETROVIRIDAE_DNA));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_mRNA_ON_WRONG_SEQUENCE_TYPE") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
              CRef <CTestAndRepData>( new CBioseq_DISC_mRNA_ON_WRONG_SEQUENCE_TYPE));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_RBS_WITHOUT_GENE") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                         CRef <CTestAndRepData>(new CBioseq_DISC_RBS_WITHOUT_GENE));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_EXON_INTRON_CONFLICT") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                      CRef <CTestAndRepData>(new CBioseq_DISC_EXON_INTRON_CONFLICT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_TAXNAME_NOT_IN_DEFLINE") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                     CRef <CTestAndRepData>(new CBioseq_TEST_TAXNAME_NOT_IN_DEFLINE));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("INCONSISTENT_SOURCE_DEFLINE") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                     CRef <CTestAndRepData>(new CBioseq_INCONSISTENT_SOURCE_DEFLINE));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_BACTERIA_SHOULD_NOT_HAVE_MRNA") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
               CRef <CTestAndRepData>(new CBioseq_DISC_BACTERIA_SHOULD_NOT_HAVE_MRNA));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_BAD_GENE_NAME") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                            CRef <CTestAndRepData> (new CBioseq_TEST_BAD_GENE_NAME));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("MOLTYPE_NOT_MRNA") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                             CRef <CTestAndRepData> (new CBioseq_MOLTYPE_NOT_MRNA));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TECHNIQUE_NOT_TSA") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                             CRef <CTestAndRepData> (new CBioseq_TECHNIQUE_NOT_TSA));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("SHORT_PROT_SEQUENCES") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                         CRef <CTestAndRepData> (new CBioseq_SHORT_PROT_SEQUENCES));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_COUNT_UNVERIFIED") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                   CRef <CTestAndRepData>(new CBioseq_TEST_COUNT_UNVERIFIED));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_DUP_GENES_OPPOSITE_STRANDS") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                CRef <CTestAndRepData>(new CBioseq_TEST_DUP_GENES_OPPOSITE_STRANDS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_GENE_PARTIAL_CONFLICT") != thisTest.tests_run.end()) {
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
               CRef <CTestAndRepData>(new CBioseq_DISC_GENE_PARTIAL_CONFLICT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_FLATFILE_FIND_ONCALLER") != thisTest.tests_run.end()) {
       tests_on_SeqEntry.push_back(
             CRef <CTestAndRepData>(new CSeqEntry_DISC_FLATFILE_FIND_ONCALLER));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_FEATURE_COUNT") != thisTest.tests_run.end()) { // asndisc version   
       tests_on_SeqEntry.push_back(CRef <CTestAndRepData>(new CSeqEntry_DISC_FEATURE_COUNT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_INCONSISTENT_STRUCTURED_COMMENTS") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back( 
            CRef <CTestAndRepData>(new CSeqEntry_DISC_INCONSISTENT_STRUCTURED_COMMENTS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_INCONSISTENT_DBLINK") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_DISC_INCONSISTENT_DBLINK));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("END_COLON_IN_COUNTRY") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_END_COLON_IN_COUNTRY));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_SUSPECTED_ORG_COLLECTED") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_SUSPECTED_ORG_COLLECTED));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_SUSPECTED_ORG_IDENTIFIED") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_SUSPECTED_ORG_IDENTIFIED));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_MORE_NAMES_COLLECTED_BY") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_MORE_NAMES_COLLECTED_BY));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_STRAIN_TAXNAME_CONFLICT") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_STRAIN_TAXNAME_CONFLICT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_SMALL_GENOME_SET_PROBLEM") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_TEST_SMALL_GENOME_SET_PROBLEM));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_INCONSISTENT_MOLTYPES") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_DISC_INCONSISTENT_MOLTYPES));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_BIOMATERIAL_TAXNAME_MISMATCH") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_DISC_BIOMATERIAL_TAXNAME_MISMATCH));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_CULTURE_TAXNAME_MISMATCH") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_DISC_CULTURE_TAXNAME_MISMATCH));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_STRAIN_TAXNAME_MISMATCH") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_DISC_STRAIN_TAXNAME_MISMATCH));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_SPECVOUCHER_TAXNAME_MISMATCH") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_DISC_SPECVOUCHER_TAXNAME_MISMATCH));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_HAPLOTYPE_MISMATCH") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_DISC_HAPLOTYPE_MISMATCH));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_MISSING_VIRAL_QUALS") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_DISC_MISSING_VIRAL_QUALS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_INFLUENZA_DATE_MISMATCH") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_DISC_INFLUENZA_DATE_MISMATCH));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TAX_LOOKUP_MISSING") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_TAX_LOOKUP_MISSING));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TAX_LOOKUP_MISMATCH") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_TAX_LOOKUP_MISMATCH));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("MISSING_STRUCTURED_COMMENT") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_MISSING_STRUCTURED_COMMENT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_MISSING_STRUCTURED_COMMENTS") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_MISSING_STRUCTURED_COMMENTS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_MISSING_AFFIL") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back( 
                      CRef <CTestAndRepData>(new CSeqEntry_DISC_MISSING_AFFIL));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_CITSUBAFFIL_CONFLICT") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back( 
                      CRef <CTestAndRepData>(new CSeqEntry_DISC_CITSUBAFFIL_CONFLICT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_TITLE_AUTHOR_CONFLICT") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back( 
                      CRef <CTestAndRepData>(new CSeqEntry_DISC_TITLE_AUTHOR_CONFLICT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_USA_STATE") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back( 
                      CRef <CTestAndRepData>(new CSeqEntry_DISC_USA_STATE));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_CITSUB_AFFIL_DUP_TEXT") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back( 
                      CRef <CTestAndRepData>(new CSeqEntry_DISC_CITSUB_AFFIL_DUP_TEXT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_REQUIRED_CLONE") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back( 
                            CRef <CTestAndRepData>(new CSeqEntry_DISC_REQUIRED_CLONE));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_MULTISRC") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back( 
                            CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_MULTISRC));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_SRC_QUAL_PROBLEM") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back(
                       CRef <CTestAndRepData>(new CSeqEntry_DISC_SRC_QUAL_PROBLEM));
        if (++i >= sz) return;
   }
   if (thisTest.tests_run.find("DISC_SOURCE_QUALS_ASNDISC") != thisTest.tests_run.end()) { //asndisc version
       tests_on_SeqEntry_feat_desc.push_back(
                       CRef <CTestAndRepData>(new CSeqEntry_DISC_SOURCE_QUALS_ASNDISC));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_UNPUB_PUB_WITHOUT_TITLE") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back(
                    CRef <CTestAndRepData>(new CSeqEntry_DISC_UNPUB_PUB_WITHOUT_TITLE));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_CONSORTIUM") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back(
                       CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_CONSORTIUM));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_CHECK_AUTH_NAME") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back(
                          CRef <CTestAndRepData>(new CSeqEntry_DISC_CHECK_AUTH_NAME));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_CHECK_AUTH_CAPS") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back(
                          CRef <CTestAndRepData>(new CSeqEntry_DISC_CHECK_AUTH_CAPS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_MISMATCHED_COMMENTS") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back(
                       CRef <CTestAndRepData>(new CSeqEntry_DISC_MISMATCHED_COMMENTS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_COMMENT_PRESENT") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back(
                       CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_COMMENT_PRESENT));
        if (++i >= sz) return;
   }
   if (thisTest.tests_run.find("DUP_DISC_ATCC_CULTURE_CONFLICT") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back(
                 CRef <CTestAndRepData>(new CSeqEntry_DUP_DISC_ATCC_CULTURE_CONFLICT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_STRAIN_CULTURE_COLLECTION_MISMATCH") 
                                                           != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back(CRef <CTestAndRepData>(
                          new CSeqEntry_ONCALLER_STRAIN_CULTURE_COLLECTION_MISMATCH));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_DUP_DEFLINE") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back(
                         CRef <CTestAndRepData>(new CSeqEntry_DISC_DUP_DEFLINE));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_TITLE_ENDS_WITH_SEQUENCE") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back(
                    CRef <CTestAndRepData>(new CSeqEntry_DISC_TITLE_ENDS_WITH_SEQUENCE));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_MISSING_DEFLINES") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back(
                         CRef <CTestAndRepData>(new CSeqEntry_DISC_MISSING_DEFLINES));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_DEFLINE_ON_SET") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back(
                         CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_DEFLINE_ON_SET));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_BACTERIAL_TAX_STRAIN_MISMATCH") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back(
              CRef <CTestAndRepData>(new CSeqEntry_DISC_BACTERIAL_TAX_STRAIN_MISMATCH));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DUP_DISC_CBS_CULTURE_CONFLICT") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back(
                   CRef <CTestAndRepData>(new CSeqEntry_DUP_DISC_CBS_CULTURE_CONFLICT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("INCONSISTENT_BIOSOURCE") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back(
                          CRef <CTestAndRepData>(new CSeqEntry_INCONSISTENT_BIOSOURCE));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_BIOPROJECT_ID") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back(
                       CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_BIOPROJECT_ID));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_SWITCH_STRUCTURED_COMMENT_PREFIX") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back(CRef <CTestAndRepData>(
                           new CSeqEntry_ONCALLER_SWITCH_STRUCTURED_COMMENT_PREFIX));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("MISSING_GENOMEASSEMBLY_COMMENTS") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back(
                 CRef <CTestAndRepData>(new CSeqEntry_MISSING_GENOMEASSEMBLY_COMMENTS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_HAS_PROJECT_ID") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back(
                            CRef <CTestAndRepData>(new CSeqEntry_TEST_HAS_PROJECT_ID));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_TRINOMIAL_SHOULD_HAVE_QUALIFIER") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back(
             CRef <CTestAndRepData>(new CSeqEntry_DISC_TRINOMIAL_SHOULD_HAVE_QUALIFIER));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_DUPLICATE_PRIMER_SET") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back(
                    CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_DUPLICATE_PRIMER_SET));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_COUNTRY_COLON") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back(
                         CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_COUNTRY_COLON));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_MISSING_PRIMER") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back(
                         CRef <CTestAndRepData>(new CSeqEntry_TEST_MISSING_PRIMER));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_SP_NOT_UNCULTURED") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back(
                         CRef <CTestAndRepData>(new CSeqEntry_TEST_SP_NOT_UNCULTURED));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_METAGENOMIC") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back(
                         CRef <CTestAndRepData>(new CSeqEntry_DISC_METAGENOMIC));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_MAP_CHROMOSOME_CONFLICT") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back(
                    CRef <CTestAndRepData>(new CSeqEntry_DISC_MAP_CHROMOSOME_CONFLICT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DIVISION_CODE_CONFLICTS") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back(
                         CRef <CTestAndRepData>(new CSeqEntry_DIVISION_CODE_CONFLICTS));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE") 
                                                             != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back( CRef <CTestAndRepData>(
                         new CSeqEntry_TEST_AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_UNNECESSARY_ENVIRONMENTAL") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back(
                   CRef <CTestAndRepData>(new CSeqEntry_TEST_UNNECESSARY_ENVIRONMENTAL));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_HUMAN_HOST") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back(
                   CRef <CTestAndRepData>(new CSeqEntry_DISC_HUMAN_HOST));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_MULTIPLE_CULTURE_COLLECTION") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back(
             CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_MULTIPLE_CULTURE_COLLECTION));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("ONCALLER_CHECK_AUTHORITY") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back(
                         CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_CHECK_AUTHORITY));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_METAGENOME_SOURCE") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back(
                         CRef <CTestAndRepData>(new CSeqEntry_DISC_METAGENOME_SOURCE));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_BACTERIA_MISSING_STRAIN") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back(
                     CRef <CTestAndRepData>(new CSeqEntry_DISC_BACTERIA_MISSING_STRAIN));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_REQUIRED_STRAIN") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back(
                            CRef <CTestAndRepData>(new CSeqEntry_DISC_REQUIRED_STRAIN));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("MORE_OR_SPEC_NAMES_IDENTIFIED_BY") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back(
                CRef <CTestAndRepData>(new CSeqEntry_MORE_OR_SPEC_NAMES_IDENTIFIED_BY));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("MORE_NAMES_COLLECTED_BY") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back(
                       CRef <CTestAndRepData>(new CSeqEntry_MORE_NAMES_COLLECTED_BY));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("MISSING_PROJECT") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back(
                       CRef <CTestAndRepData>(new CSeqEntry_MISSING_PROJECT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE1") != thisTest.tests_run.end()) {
       tests_on_SeqEntry_feat_desc.push_back(CRef <CTestAndRepData>(
                       new CSeqEntry_DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE)); // not tested
       tests_on_SeqEntry_feat_desc.push_back(CRef <CTestAndRepData>(
                       new CSeqEntry_DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE1)); // not tested
i += 2;
if (i >= sz) return;
      //  if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_SEGSETS_PRESENT") != thisTest.tests_run.end()) {
       tests_on_BioseqSet.push_back(
                       CRef <CTestAndRepData>(new CBioseqSet_DISC_SEGSETS_PRESENT));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("TEST_UNWANTED_SET_WRAPPER") != thisTest.tests_run.end()) {
       tests_on_BioseqSet.push_back(
                       CRef <CTestAndRepData>(new CBioseqSet_TEST_UNWANTED_SET_WRAPPER));
        if (++i >= sz) return;
   }
   if ( thisTest.tests_run.find("DISC_NONWGS_SETS_PRESENT") != thisTest.tests_run.end()) {
       tests_on_BioseqSet.push_back(
                       CRef <CTestAndRepData>(new CBioseqSet_DISC_NONWGS_SETS_PRESENT));
        if (++i >= sz) return;
   }

   NCBI_THROW(CException, eUnknown, "Some requested tests are unrecognizable.");
};

void CRepConfig :: Init(const string& report_type)
{
   tests_on_Bioseq.reserve(50);
   tests_on_Bioseq_aa.reserve(50);
   tests_on_Bioseq_na.reserve(50);
   tests_on_Bioseq_CFeat.reserve(50);
   tests_on_Bioseq_NotInGenProdSet.reserve(50);
   tests_on_Bioseq_CFeat_CSeqdesc.reserve(50);
   tests_on_SeqEntry.reserve(50);
   tests_on_SeqEntry_feat_desc.reserve(50);
   tests_on_BioseqSet.reserve(50);
   tests_on_SubmitBlk.reserve(1);
  
   ETestCategoryFlags cate_flag;
   if (report_type == "Discrepancy") {
            cate_flag = fDiscrepancy;
            thisInfo.output_config.use_flag = true;
   }
   else if(report_type ==  "Oncaller") cate_flag = fOncaller;
   else NCBI_THROW(CException, eUnknown, "Unrecognized report type.");

   for (unsigned i=0; i< sizeof(test_list)/sizeof(s_test_property); i++) {
      if (test_list[i].category & cate_flag) 
                thisTest.tests_run.insert(test_list[i].name);
   }
   
   ITERATE (vector <string>, it, thisInfo.tests_enabled) {
      if (thisTest.tests_run.find(*it) == thisTest.tests_run.end()) 
            thisTest.tests_run.insert(*it);
   }
   ITERATE (vector <string>, it, thisInfo.tests_disabled) {
      if (thisTest.tests_run.find(*it) != thisTest.tests_run.end())
           thisTest.tests_run.erase(*it);
   }
  
   CollectTests();

}; // Init()



void CRepConfDiscrepancy :: ConfigRep()
{
// tests_on_SubmitBlk
   tests_on_SubmitBlk.push_back(
                      CRef <CTestAndRepData>(new CSeqEntry_DISC_SUBMITBLOCK_CONFLICT));

// tests_on_Bioseq
   tests_on_Bioseq.push_back(CRef <CTestAndRepData>(new CBioseq_DISC_COUNT_NUCLEOTIDES));
   tests_on_Bioseq.push_back(CRef <CTestAndRepData>(new CBioseq_DISC_QUALITY_SCORES));
   // oncaller tool version
   // tests_on_Bioseq.push_back(CRef <CTestAndRepData>(new CBioseq_DISC_FEATURE_COUNT));
   thisInfo.test_item_list["DISC_QUALITY_SCORES"].clear();
   
// tests_on_Bioseq_aa
   tests_on_Bioseq_aa.push_back(CRef <CTestAndRepData>(new CBioseq_COUNT_PROTEINS));
   tests_on_Bioseq_aa.push_back(CRef <CTestAndRepData>(new CBioseq_MISSING_PROTEIN_ID1));
   tests_on_Bioseq_aa.push_back(CRef <CTestAndRepData>(new CBioseq_MISSING_PROTEIN_ID));
   tests_on_Bioseq_aa.push_back(
                   CRef <CTestAndRepData>(new CBioseq_INCONSISTENT_PROTEIN_ID_PREFIX1));
   tests_on_Bioseq_aa.push_back(
                   CRef <CTestAndRepData>(new CBioseq_INCONSISTENT_PROTEIN_ID_PREFIX));

// tests_on_Bioseq_na
   tests_on_Bioseq_na.push_back(
                     CRef <CTestAndRepData>(new CBioseq_TEST_DEFLINE_PRESENT));
   tests_on_Bioseq_na.push_back(CRef <CTestAndRepData>(new CBioseq_N_RUNS));
   tests_on_Bioseq_na.push_back(CRef <CTestAndRepData>(new CBioseq_N_RUNS_14));
   tests_on_Bioseq_na.push_back(CRef <CTestAndRepData>(new CBioseq_ZERO_BASECOUNT));
   tests_on_Bioseq_na.push_back(
                     CRef <CTestAndRepData>(new CBioseq_TEST_LOW_QUALITY_REGION));
   tests_on_Bioseq_na.push_back(CRef <CTestAndRepData>(new CBioseq_DISC_PERCENT_N));
   tests_on_Bioseq_na.push_back(CRef <CTestAndRepData>(new CBioseq_DISC_10_PERCENTN));
   tests_on_Bioseq_na.push_back(CRef <CTestAndRepData>(new CBioseq_TEST_UNUSUAL_NT));

// tests_on_Bioseq_CFeat
   tests_on_Bioseq_CFeat.push_back( 
                CRef <CTestAndRepData> (new CBioseq_DISC_SUSPECT_RRNA_PRODUCTS));
   tests_on_Bioseq_CFeat.push_back( CRef <CTestAndRepData> (new CBioseq_on_SUSPECT_RULE));
   tests_on_Bioseq_CFeat.push_back( 
                CRef <CTestAndRepData> (new CBioseq_TEST_ORGANELLE_PRODUCTS));
   tests_on_Bioseq_CFeat.push_back( CRef <CTestAndRepData> (new CBioseq_DISC_GAPS));
   tests_on_Bioseq_CFeat.push_back( 
                CRef <CTestAndRepData> (new CBioseq_TEST_MRNA_OVERLAPPING_PSEUDO_GENE));
   tests_on_Bioseq_CFeat.push_back( 
                   CRef <CTestAndRepData> (new CBioseq_ONCALLER_HAS_STANDARD_NAME));
   tests_on_Bioseq_CFeat.push_back( 
                   CRef <CTestAndRepData> (new CBioseq_ONCALLER_ORDERED_LOCATION));
   tests_on_Bioseq_CFeat.push_back( 
                    CRef <CTestAndRepData> (new CBioseq_DISC_FEATURE_LIST));
   tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_TEST_CDS_HAS_CDD_XREF));
   tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_DISC_CDS_HAS_NEW_EXCEPTION));
   tests_on_Bioseq_CFeat.push_back(
             CRef <CTestAndRepData> (new CBioseq_DISC_MICROSATELLITE_REPEAT_TYPE));
   tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_DISC_SUSPECT_MISC_FEATURES));
   tests_on_Bioseq_CFeat.push_back(
             CRef <CTestAndRepData> (new CBioseq_DISC_CHECK_RNA_PRODUCTS_AND_COMMENTS));
   tests_on_Bioseq_CFeat.push_back(
                CRef <CTestAndRepData> (new CBioseq_DISC_FEATURE_MOLTYPE_MISMATCH));
   tests_on_Bioseq_CFeat.push_back(
                         CRef <CTestAndRepData> (new CBioseq_ADJACENT_PSEUDOGENES));
   tests_on_Bioseq_CFeat.push_back(
                CRef <CTestAndRepData> (new CBioseq_MISSING_GENPRODSET_PROTEIN));
   tests_on_Bioseq_CFeat.push_back(
                      CRef <CTestAndRepData> (new CBioseq_DUP_GENPRODSET_PROTEIN));
   tests_on_Bioseq_CFeat.push_back(
                CRef <CTestAndRepData> (new CBioseq_MISSING_GENPRODSET_TRANSCRIPT_ID));
   tests_on_Bioseq_CFeat.push_back(
                CRef <CTestAndRepData> (new CBioseq_DISC_DUP_GENPRODSET_TRANSCRIPT_ID));
   tests_on_Bioseq_CFeat.push_back(
                      CRef <CTestAndRepData> (new CBioseq_DISC_FEAT_OVERLAP_SRCFEAT));
   tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_CDS_TRNA_OVERLAP));
   tests_on_Bioseq_CFeat.push_back(CRef <CTestAndRepData> (new CBioseq_TRANSL_NO_NOTE));
   tests_on_Bioseq_CFeat.push_back(CRef <CTestAndRepData> (new CBioseq_NOTE_NO_TRANSL));
   tests_on_Bioseq_CFeat.push_back(CRef <CTestAndRepData> (new CBioseq_TRANSL_TOO_LONG));
   tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_FIND_STRAND_TRNAS));
   tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_FIND_BADLEN_TRNAS));
   tests_on_Bioseq_CFeat.push_back(CRef <CTestAndRepData> (new CBioseq_COUNT_TRNAS));
   tests_on_Bioseq_CFeat.push_back(CRef <CTestAndRepData> (new CBioseq_FIND_DUP_TRNAS));
   tests_on_Bioseq_CFeat.push_back(CRef <CTestAndRepData> (new CBioseq_COUNT_RRNAS));
   tests_on_Bioseq_CFeat.push_back(CRef <CTestAndRepData> (new CBioseq_FIND_DUP_RRNAS));
   tests_on_Bioseq_CFeat.push_back(CRef <CTestAndRepData> (
                                            new CBioseq_PARTIAL_CDS_COMPLETE_SEQUENCE));
   tests_on_Bioseq_CFeat.push_back(CRef <CTestAndRepData> (new CBioseq_CONTAINED_CDS));
   tests_on_Bioseq_CFeat.push_back(CRef <CTestAndRepData> (new CBioseq_PSEUDO_MISMATCH));
   tests_on_Bioseq_CFeat.push_back(CRef <CTestAndRepData> (new CBioseq_EC_NUMBER_NOTE));
   tests_on_Bioseq_CFeat.push_back(
                         CRef <CTestAndRepData> (new CBioseq_NON_GENE_LOCUS_TAG));
   tests_on_Bioseq_CFeat.push_back(CRef <CTestAndRepData> (new CBioseq_JOINED_FEATURES));
   tests_on_Bioseq_CFeat.push_back(
                         CRef <CTestAndRepData>( new CBioseq_SHOW_TRANSL_EXCEPT));
   tests_on_Bioseq_CFeat.push_back(
          CRef <CTestAndRepData>( new CBioseq_MRNA_SHOULD_HAVE_PROTEIN_TRANSCRIPT_IDS));
   tests_on_Bioseq_CFeat.push_back(
                         CRef <CTestAndRepData>(new CBioseq_RRNA_NAME_CONFLICTS));
   tests_on_Bioseq_CFeat.push_back(
                         CRef <CTestAndRepData>(new CBioseq_ONCALLER_GENE_MISSING));
   tests_on_Bioseq_CFeat.push_back(
                         CRef <CTestAndRepData>(new CBioseq_ONCALLER_SUPERFLUOUS_GENE));
   tests_on_Bioseq_CFeat.push_back(CRef <CTestAndRepData>(new CBioseq_MISSING_GENES));
   tests_on_Bioseq_CFeat.push_back(CRef <CTestAndRepData>(new CBioseq_EXTRA_GENES));
   //tests_on_Bioseq_CFeat.push_back(CRef <CTestAndRepData>(new CBioseq_EXTRA_MISSING_GENES));
   tests_on_Bioseq_CFeat.push_back(CRef <CTestAndRepData>(new CBioseq_OVERLAPPING_CDS));
   tests_on_Bioseq_CFeat.push_back(CRef <CTestAndRepData>(new CBioseq_RNA_CDS_OVERLAP));
   tests_on_Bioseq_CFeat.push_back(
                         CRef <CTestAndRepData>(new CBioseq_FIND_OVERLAPPED_GENES));
   tests_on_Bioseq_CFeat.push_back(
                         CRef <CTestAndRepData>(new CBioseq_OVERLAPPING_GENES));
   tests_on_Bioseq_CFeat.push_back(
                   CRef <CTestAndRepData>( new CBioseq_DISC_PROTEIN_NAMES));
   tests_on_Bioseq_CFeat.push_back(
                   CRef <CTestAndRepData>( new CBioseq_DISC_CDS_PRODUCT_FIND));
   tests_on_Bioseq_CFeat.push_back(
                   CRef <CTestAndRepData>( new CBioseq_EC_NUMBER_ON_UNKNOWN_PROTEIN));
   tests_on_Bioseq_CFeat.push_back(CRef <CTestAndRepData>(new CBioseq_RNA_NO_PRODUCT));
   tests_on_Bioseq_CFeat.push_back(
                         CRef <CTestAndRepData>(new CBioseq_DISC_SHORT_INTRON));
   tests_on_Bioseq_CFeat.push_back(
                         CRef <CTestAndRepData>(new CBioseq_DISC_BAD_GENE_STRAND));
   tests_on_Bioseq_CFeat.push_back(
             CRef <CTestAndRepData>(new CBioseq_DISC_INTERNAL_TRANSCRIBED_SPACER_RRNA));
   tests_on_Bioseq_CFeat.push_back(CRef <CTestAndRepData>(new CBioseq_DISC_SHORT_RRNA));
   tests_on_Bioseq_CFeat.push_back(
                        CRef <CTestAndRepData>(new CBioseq_TEST_OVERLAPPING_RRNAS));
   tests_on_Bioseq_CFeat.push_back(
               CRef <CTestAndRepData>( new CBioseq_HYPOTHETICAL_CDS_HAVING_GENE_NAME));
   tests_on_Bioseq_CFeat.push_back(
                  CRef <CTestAndRepData>( new CBioseq_DISC_SUSPICIOUS_NOTE_TEXT));
   tests_on_Bioseq_CFeat.push_back( CRef <CTestAndRepData>(new CBioseq_NO_ANNOTATION));
   tests_on_Bioseq_CFeat.push_back( 
              CRef <CTestAndRepData>( new CBioseq_DISC_LONG_NO_ANNOTATION));
   tests_on_Bioseq_CFeat.push_back( 
              CRef <CTestAndRepData>( new CBioseq_DISC_PARTIAL_PROBLEMS));
   tests_on_Bioseq_CFeat.push_back( 
              CRef <CTestAndRepData>( new CBioseq_TEST_UNUSUAL_MISC_RNA));
   tests_on_Bioseq_CFeat.push_back( 
                     CRef <CTestAndRepData>( new CBioseq_GENE_PRODUCT_CONFLICT));
   tests_on_Bioseq_CFeat.push_back( 
                     CRef <CTestAndRepData>( new CBioseq_DISC_CDS_WITHOUT_MRNA));

// tests_on_Bioseq_CFeat_NotInGenProdSet
   tests_on_Bioseq_CFeat_NotInGenProdSet.push_back(
                          CRef <CTestAndRepData>(new CBioseq_DUPLICATE_GENE_LOCUS));
   tests_on_Bioseq_CFeat_NotInGenProdSet.push_back(
                                   CRef <CTestAndRepData>(new CBioseq_LOCUS_TAGS));
   tests_on_Bioseq_CFeat_NotInGenProdSet.push_back(
                          CRef <CTestAndRepData>(new CBioseq_FEATURE_LOCATION_CONFLICT));

// tests_on_Bioseq_CFeat_CSeqdesc
   tests_on_Bioseq_NotInGenProdSet.push_back(
                     CRef<CTestAndRepData>(new CBioseq_DISC_INCONSISTENT_MOLINFO_TECH));
   tests_on_Bioseq_NotInGenProdSet.push_back(
                                 CRef<CTestAndRepData>(new CBioseq_SHORT_CONTIG));
   tests_on_Bioseq_NotInGenProdSet.push_back(
                                     CRef<CTestAndRepData>(new CBioseq_SHORT_SEQUENCES));
   tests_on_Bioseq_NotInGenProdSet.push_back(
                                CRef<CTestAndRepData>(new CBioseq_SHORT_SEQUENCES_200));
   tests_on_Bioseq_CFeat_CSeqdesc.push_back(CRef <CTestAndRepData>(
                                                     new CBioseq_TEST_UNWANTED_SPACER));
   tests_on_Bioseq_CFeat_CSeqdesc.push_back(CRef <CTestAndRepData>(
                                new CBioseq_TEST_UNNECESSARY_VIRUS_GENE));
   tests_on_Bioseq_CFeat_CSeqdesc.push_back(CRef <CTestAndRepData>(
                                                     new CBioseq_TEST_ORGANELLE_NOT_GENOMIC));
   tests_on_Bioseq_CFeat_CSeqdesc.push_back(CRef <CTestAndRepData>(
                                               new CBioseq_MULTIPLE_CDS_ON_MRNA));
   tests_on_Bioseq_CFeat_CSeqdesc.push_back(CRef <CTestAndRepData>(
                                               new CBioseq_TEST_MRNA_SEQUENCE_MINUS_ST));
   tests_on_Bioseq_CFeat_CSeqdesc.push_back(CRef <CTestAndRepData>(
                                                     new CBioseq_TEST_BAD_MRNA_QUAL));
   tests_on_Bioseq_CFeat_CSeqdesc.push_back(CRef <CTestAndRepData>(
                                                     new CBioseq_TEST_EXON_ON_MRNA));
   tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                     CRef <CTestAndRepData>( new CBioseq_ONCALLER_HIV_RNA_INCONSISTENT));
   tests_on_Bioseq_CFeat_CSeqdesc.push_back(
    CRef <CTestAndRepData>( new CBioseq_DISC_BACTERIAL_PARTIAL_NONEXTENDABLE_EXCEPTION));
   tests_on_Bioseq_CFeat_CSeqdesc.push_back(
     CRef <CTestAndRepData>( new CBioseq_DISC_BACTERIAL_PARTIAL_NONEXTENDABLE_PROBLEMS));
   tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                      CRef <CTestAndRepData>( new CBioseq_DISC_MITOCHONDRION_REQUIRED));
   tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                             CRef <CTestAndRepData>( new CBioseq_EUKARYOTE_SHOULD_HAVE_MRNA));
   tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                             CRef <CTestAndRepData>( new CBioseq_RNA_PROVIRAL));
   tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                             CRef <CTestAndRepData>( new CBioseq_NON_RETROVIRIDAE_PROVIRAL));
   tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                             CRef <CTestAndRepData>( new CBioseq_DISC_RETROVIRIDAE_DNA));
   tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                  CRef <CTestAndRepData>( new CBioseq_DISC_mRNA_ON_WRONG_SEQUENCE_TYPE));
   tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                             CRef <CTestAndRepData>(new CBioseq_DISC_RBS_WITHOUT_GENE));
   tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                          CRef <CTestAndRepData>(new CBioseq_DISC_EXON_INTRON_CONFLICT));
   tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                        CRef <CTestAndRepData>(new CBioseq_TEST_TAXNAME_NOT_IN_DEFLINE));
   tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                        CRef <CTestAndRepData>(new CBioseq_INCONSISTENT_SOURCE_DEFLINE));
   tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                 CRef <CTestAndRepData>(new CBioseq_DISC_BACTERIA_SHOULD_NOT_HAVE_MRNA));
   tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                                CRef <CTestAndRepData> (new CBioseq_TEST_BAD_GENE_NAME));
   tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                                 CRef <CTestAndRepData> (new CBioseq_MOLTYPE_NOT_MRNA));
   tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                                 CRef <CTestAndRepData> (new CBioseq_TECHNIQUE_NOT_TSA));
   tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                              CRef <CTestAndRepData> (new CBioseq_SHORT_PROT_SEQUENCES));
   tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                   CRef <CTestAndRepData>(new CBioseq_TEST_COUNT_UNVERIFIED));
   tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                   CRef <CTestAndRepData>(new CBioseq_TEST_DUP_GENES_OPPOSITE_STRANDS));
   tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                   CRef <CTestAndRepData>(new CBioseq_DISC_GENE_PARTIAL_CONFLICT));

// tests_on_SeqEntry
   tests_on_SeqEntry.push_back(CRef <CTestAndRepData>(
                                new CSeqEntry_DISC_FLATFILE_FIND_ONCALLER));

   // asndisc version   
   tests_on_SeqEntry.push_back(CRef <CTestAndRepData>(new CSeqEntry_DISC_FEATURE_COUNT));

// tests_on_SeqEntry_feat_desc: all CSeqEntry_Feat_desc tests need RmvRedundancy
   tests_on_SeqEntry_feat_desc.push_back( 
            CRef <CTestAndRepData>(new CSeqEntry_DISC_INCONSISTENT_STRUCTURED_COMMENTS));
   tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_DISC_INCONSISTENT_DBLINK));
   tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_END_COLON_IN_COUNTRY));
   tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_SUSPECTED_ORG_COLLECTED));
   tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_SUSPECTED_ORG_IDENTIFIED));
   tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_MORE_NAMES_COLLECTED_BY));
   tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_STRAIN_TAXNAME_CONFLICT));
   tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_TEST_SMALL_GENOME_SET_PROBLEM));
   tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_DISC_INCONSISTENT_MOLTYPES));
   tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_DISC_BIOMATERIAL_TAXNAME_MISMATCH));
   tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_DISC_CULTURE_TAXNAME_MISMATCH));
   tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_DISC_STRAIN_TAXNAME_MISMATCH));
   tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_DISC_SPECVOUCHER_TAXNAME_MISMATCH));
   tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_DISC_HAPLOTYPE_MISMATCH));
   tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_DISC_MISSING_VIRAL_QUALS));
   tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_DISC_INFLUENZA_DATE_MISMATCH));
   tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_TAX_LOOKUP_MISSING));
   tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_TAX_LOOKUP_MISMATCH));
   tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_MISSING_STRUCTURED_COMMENT));
   tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_MISSING_STRUCTURED_COMMENTS));
   tests_on_SeqEntry_feat_desc.push_back( 
                      CRef <CTestAndRepData>(new CSeqEntry_DISC_MISSING_AFFIL));
   tests_on_SeqEntry_feat_desc.push_back( 
                      CRef <CTestAndRepData>(new CSeqEntry_DISC_CITSUBAFFIL_CONFLICT));
   tests_on_SeqEntry_feat_desc.push_back( 
                      CRef <CTestAndRepData>(new CSeqEntry_DISC_TITLE_AUTHOR_CONFLICT));
   tests_on_SeqEntry_feat_desc.push_back( 
                      CRef <CTestAndRepData>(new CSeqEntry_DISC_USA_STATE));
   tests_on_SeqEntry_feat_desc.push_back( 
                      CRef <CTestAndRepData>(new CSeqEntry_DISC_CITSUB_AFFIL_DUP_TEXT));
   tests_on_SeqEntry_feat_desc.push_back( 
                            CRef <CTestAndRepData>(new CSeqEntry_DISC_REQUIRED_CLONE));
   tests_on_SeqEntry_feat_desc.push_back( 
                            CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_MULTISRC));
   tests_on_SeqEntry_feat_desc.push_back(
                       CRef <CTestAndRepData>(new CSeqEntry_DISC_SRC_QUAL_PROBLEM));
   // asndisc version
   tests_on_SeqEntry_feat_desc.push_back(
                       CRef <CTestAndRepData>(new CSeqEntry_DISC_SOURCE_QUALS_ASNDISC));
   tests_on_SeqEntry_feat_desc.push_back(
                          CRef <CTestAndRepData>(new CSeqEntry_DISC_UNPUB_PUB_WITHOUT_TITLE));
   tests_on_SeqEntry_feat_desc.push_back(
                          CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_CONSORTIUM));
   tests_on_SeqEntry_feat_desc.push_back(
                          CRef <CTestAndRepData>(new CSeqEntry_DISC_CHECK_AUTH_NAME));
   tests_on_SeqEntry_feat_desc.push_back(
                          CRef <CTestAndRepData>(new CSeqEntry_DISC_CHECK_AUTH_CAPS));
   tests_on_SeqEntry_feat_desc.push_back(
                       CRef <CTestAndRepData>(new CSeqEntry_DISC_MISMATCHED_COMMENTS));
   tests_on_SeqEntry_feat_desc.push_back(
                       CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_COMMENT_PRESENT));
   tests_on_SeqEntry_feat_desc.push_back(
                 CRef <CTestAndRepData>(new CSeqEntry_DUP_DISC_ATCC_CULTURE_CONFLICT));
   tests_on_SeqEntry_feat_desc.push_back(
     CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_STRAIN_CULTURE_COLLECTION_MISMATCH));
   tests_on_SeqEntry_feat_desc.push_back(
                         CRef <CTestAndRepData>(new CSeqEntry_DISC_DUP_DEFLINE));
   tests_on_SeqEntry_feat_desc.push_back(
                    CRef <CTestAndRepData>(new CSeqEntry_DISC_TITLE_ENDS_WITH_SEQUENCE));
   tests_on_SeqEntry_feat_desc.push_back(
                         CRef <CTestAndRepData>(new CSeqEntry_DISC_MISSING_DEFLINES));
   tests_on_SeqEntry_feat_desc.push_back(
                         CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_DEFLINE_ON_SET));
   tests_on_SeqEntry_feat_desc.push_back(
              CRef <CTestAndRepData>(new CSeqEntry_DISC_BACTERIAL_TAX_STRAIN_MISMATCH));
   tests_on_SeqEntry_feat_desc.push_back(
                   CRef <CTestAndRepData>(new CSeqEntry_DUP_DISC_CBS_CULTURE_CONFLICT));
   tests_on_SeqEntry_feat_desc.push_back(
                          CRef <CTestAndRepData>(new CSeqEntry_INCONSISTENT_BIOSOURCE));
   tests_on_SeqEntry_feat_desc.push_back(
                       CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_BIOPROJECT_ID));
   tests_on_SeqEntry_feat_desc.push_back(
            CRef <CTestAndRepData>(new CSeqEntry_DISC_INCONSISTENT_STRUCTURED_COMMENTS));
   tests_on_SeqEntry_feat_desc.push_back(
       CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_SWITCH_STRUCTURED_COMMENT_PREFIX));
   tests_on_SeqEntry_feat_desc.push_back(
                 CRef <CTestAndRepData>(new CSeqEntry_MISSING_GENOMEASSEMBLY_COMMENTS));
   tests_on_SeqEntry_feat_desc.push_back(
                            CRef <CTestAndRepData>(new CSeqEntry_TEST_HAS_PROJECT_ID));
   tests_on_SeqEntry_feat_desc.push_back(
             CRef <CTestAndRepData>(new CSeqEntry_DISC_TRINOMIAL_SHOULD_HAVE_QUALIFIER));
   tests_on_SeqEntry_feat_desc.push_back(
                    CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_DUPLICATE_PRIMER_SET));
   tests_on_SeqEntry_feat_desc.push_back(
                         CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_COUNTRY_COLON));
   tests_on_SeqEntry_feat_desc.push_back(
                         CRef <CTestAndRepData>(new CSeqEntry_TEST_MISSING_PRIMER));
   tests_on_SeqEntry_feat_desc.push_back(
                         CRef <CTestAndRepData>(new CSeqEntry_TEST_SP_NOT_UNCULTURED));
   tests_on_SeqEntry_feat_desc.push_back(
                         CRef <CTestAndRepData>(new CSeqEntry_DISC_METAGENOMIC));
   tests_on_SeqEntry_feat_desc.push_back(
                    CRef <CTestAndRepData>(new CSeqEntry_DISC_MAP_CHROMOSOME_CONFLICT));
   tests_on_SeqEntry_feat_desc.push_back(
                         CRef <CTestAndRepData>(new CSeqEntry_DIVISION_CODE_CONFLICTS));
   tests_on_SeqEntry_feat_desc.push_back( CRef <CTestAndRepData>(
                         new CSeqEntry_TEST_AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE));
   tests_on_SeqEntry_feat_desc.push_back(
                   CRef <CTestAndRepData>(new CSeqEntry_TEST_UNNECESSARY_ENVIRONMENTAL));
   tests_on_SeqEntry_feat_desc.push_back(
                   CRef <CTestAndRepData>(new CSeqEntry_DISC_HUMAN_HOST));
   tests_on_SeqEntry_feat_desc.push_back(
             CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_MULTIPLE_CULTURE_COLLECTION));
   tests_on_SeqEntry_feat_desc.push_back(
                         CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_CHECK_AUTHORITY));
   tests_on_SeqEntry_feat_desc.push_back(
                         CRef <CTestAndRepData>(new CSeqEntry_DISC_METAGENOME_SOURCE));
   tests_on_SeqEntry_feat_desc.push_back(
                     CRef <CTestAndRepData>(new CSeqEntry_DISC_BACTERIA_MISSING_STRAIN));
   tests_on_SeqEntry_feat_desc.push_back(
                            CRef <CTestAndRepData>(new CSeqEntry_DISC_REQUIRED_STRAIN));
   tests_on_SeqEntry_feat_desc.push_back(
                CRef <CTestAndRepData>(new CSeqEntry_MORE_OR_SPEC_NAMES_IDENTIFIED_BY));
   tests_on_SeqEntry_feat_desc.push_back(
                       CRef <CTestAndRepData>(new CSeqEntry_MORE_NAMES_COLLECTED_BY));
   tests_on_SeqEntry_feat_desc.push_back(
                       CRef <CTestAndRepData>(new CSeqEntry_MISSING_PROJECT));
   tests_on_SeqEntry_feat_desc.push_back( 
          CRef <CTestAndRepData>( new CSeqEntry_DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE));
   tests_on_SeqEntry_feat_desc.push_back( 
          CRef <CTestAndRepData>( new CSeqEntry_DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE1)); // not tested

// tests_on_BioseqSet   // redundant because of nested set?
   tests_on_BioseqSet.push_back(
                       CRef <CTestAndRepData>(new CBioseqSet_DISC_SEGSETS_PRESENT));
   tests_on_BioseqSet.push_back(
                       CRef <CTestAndRepData>(new CBioseqSet_TEST_UNWANTED_SET_WRAPPER));
   tests_on_BioseqSet.push_back(
                       CRef <CTestAndRepData>(new CBioseqSet_DISC_NONWGS_SETS_PRESENT));

// output flags
   thisInfo.output_config.use_flag = true;
  
} // CRepConfDiscrepancy :: configRep



void CRepConfOncaller :: ConfigRep()
{
  tests_on_SeqEntry_feat_desc.push_back(
                           CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_COMMENT_PRESENT));  

} // CRepConfOncaller :: ConfigRep()



void CRepConfAll :: ConfigRep()
{
  CRepConfDiscrepancy rep_disc;
  CRepConfOncaller rep_on;

  rep_disc.ConfigRep();
  rep_on.ConfigRep();
}; // CRepConfAll
  

