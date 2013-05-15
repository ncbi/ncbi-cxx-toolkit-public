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

static const s_test_property test_list[] = {
// tests_on_SubmitBlk
   {"DISC_SUBMITBLOCK_CONFLICT", fDiscrepancy | fOncaller},

// tests_on_Bioseq
   {"DISC_COUNT_NUCLEOTIDES", fDiscrepancy | fOncaller},
   {"DISC_QUALITY_SCORES", fDiscrepancy},
   {"DISC_FEATURE_COUNT", fOncaller},  // oncaller tool version
   
// tests_on_Bioseq_aa
   {"COUNT_PROTEINS", fDiscrepancy},
   {"MISSING_PROTEIN_ID1", fDiscrepancy},
   {"MISSING_PROTEIN_ID", fDiscrepancy},
   {"INCONSISTENT_PROTEIN_ID_PREFIX1", fDiscrepancy},
   {"INCONSISTENT_PROTEIN_ID_PREFIX", fDiscrepancy},

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
   {"on_SUSPECT_RULE", fDiscrepancy},
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
   {"DISC_INCONSISTENT_STRUCTURED_COMMENTS", fDiscrepancy},
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
   ITERATE (set <string>, it, tests_run) {
     if ((*it) == "DISC_SUBMITBLOCK_CONFLICT") {
        tests_on_SubmitBlk.push_back(
                      CRef <CTestAndRepData>(new CSeqEntry_DISC_SUBMITBLOCK_CONFLICT));
     }
     else if ((*it) == "DISC_COUNT_NUCLEOTIDES")
        tests_on_Bioseq.push_back(CRef <CTestAndRepData>(new CBioseq_DISC_COUNT_NUCLEOTIDES));
     else if ((*it) == "DISC_QUALITY_SCORES") {
        tests_on_Bioseq.push_back(CRef <CTestAndRepData>(new CBioseq_DISC_QUALITY_SCORES));
        thisInfo.test_item_list["DISC_QUALITY_SCORES"].clear();
     }
     else if ((*it) == "DISC_FEATURE_COUNT")
        tests_on_Bioseq.push_back(CRef <CTestAndRepData>(new CBioseq_DISC_FEATURE_COUNT)); 
     else if ( (*it) == "COUNT_PROTEINS")
        tests_on_Bioseq_aa.push_back(CRef <CTestAndRepData>(new CBioseq_COUNT_PROTEINS));
     else if ( (*it) == "CBioseq_MISSING_PROTEIN_ID") {
        tests_on_Bioseq_aa.push_back(CRef <CTestAndRepData>(new CBioseq_MISSING_PROTEIN_ID1));
        tests_on_Bioseq_aa.push_back(CRef <CTestAndRepData>(new CBioseq_MISSING_PROTEIN_ID));
     }
     else if ( (*it) == "INCONSISTENT_PROTEIN_ID_PREFIX") {
        tests_on_Bioseq_aa.push_back(
                   CRef <CTestAndRepData>(new CBioseq_INCONSISTENT_PROTEIN_ID_PREFIX1));
        tests_on_Bioseq_aa.push_back(
                   CRef <CTestAndRepData>(new CBioseq_INCONSISTENT_PROTEIN_ID_PREFIX));
     } 
     else if ( (*it) == "TEST_DEFLINE_PRESENT")
         tests_on_Bioseq_na.push_back(
                     CRef <CTestAndRepData>(new CBioseq_TEST_DEFLINE_PRESENT));
     else if ( (*it) == "N_RUNS") 
        tests_on_Bioseq_na.push_back(CRef <CTestAndRepData>(new CBioseq_N_RUNS));
     else if ( (*it) == "N_RUNS_14") 
       tests_on_Bioseq_na.push_back(CRef <CTestAndRepData>(new CBioseq_N_RUNS_14));
     else if ( (*it) == "ZERO_BASECOUNT")
       tests_on_Bioseq_na.push_back(CRef <CTestAndRepData>(new CBioseq_ZERO_BASECOUNT));
     else if ( (*it) == "TEST_LOW_QUALITY_REGION")
       tests_on_Bioseq_na.push_back(
                     CRef <CTestAndRepData>(new CBioseq_TEST_LOW_QUALITY_REGION));
     else if ( (*it) == "DISC_PERCENT_N")
       tests_on_Bioseq_na.push_back(CRef <CTestAndRepData>(new CBioseq_DISC_PERCENT_N));
     else if ( (*it) == "DISC_10_PERCENTN")
       tests_on_Bioseq_na.push_back(CRef <CTestAndRepData>(new CBioseq_DISC_10_PERCENTN));
     else if ( (*it) == "TEST_UNUSUAL_NT")
       tests_on_Bioseq_na.push_back(CRef <CTestAndRepData>(new CBioseq_TEST_UNUSUAL_NT));
     else if ( (*it) == "SUSPECT_PHRASES")
        tests_on_Bioseq.push_back(CRef <CTestAndRepData>(new CBioseq_SUSPECT_PHRASES));
     else if ( (*it) == "DISC_SUSPECT_RRNA_PRODUCTS")
       tests_on_Bioseq_CFeat.push_back( 
                CRef <CTestAndRepData> (new CBioseq_DISC_SUSPECT_RRNA_PRODUCTS));
     else if ( (*it) == "SUSPECT_RULE")
       tests_on_Bioseq_CFeat.push_back( 
                CRef <CTestAndRepData> (new CBioseq_on_SUSPECT_RULE));
     else if ( (*it) == "TEST_ORGANELLE_PRODUCTS")
       tests_on_Bioseq_CFeat.push_back( 
                CRef <CTestAndRepData> (new CBioseq_TEST_ORGANELLE_PRODUCTS));
     else if ( (*it) == "DISC_GAPS")
       tests_on_Bioseq_CFeat.push_back( CRef <CTestAndRepData> (new CBioseq_DISC_GAPS));
     else if ( (*it) == "TEST_MRNA_OVERLAPPING_PSEUDO_GENE")
       tests_on_Bioseq_CFeat.push_back( 
                CRef <CTestAndRepData> (new CBioseq_TEST_MRNA_OVERLAPPING_PSEUDO_GENE));
     else if ( (*it) == "ONCALLER_HAS_STANDARD_NAME")
       tests_on_Bioseq_CFeat.push_back( 
                   CRef <CTestAndRepData> (new CBioseq_ONCALLER_HAS_STANDARD_NAME));
     else if ( (*it) == "ONCALLER_ORDERED_LOCATION")
       tests_on_Bioseq_CFeat.push_back( 
                   CRef <CTestAndRepData> (new CBioseq_ONCALLER_ORDERED_LOCATION));
     else if ( (*it) == "DISC_FEATURE_LIST")
       tests_on_Bioseq_CFeat.push_back( 
                    CRef <CTestAndRepData> (new CBioseq_DISC_FEATURE_LIST));
     else if ( (*it) == "TEST_CDS_HAS_CDD_XREF")
       tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_TEST_CDS_HAS_CDD_XREF));
     else if ( (*it) == "DISC_CDS_HAS_NEW_EXCEPTION")
       tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_DISC_CDS_HAS_NEW_EXCEPTION));
     else if ( (*it) == "DISC_MICROSATELLITE_REPEAT_TYPE")
       tests_on_Bioseq_CFeat.push_back(
             CRef <CTestAndRepData> (new CBioseq_DISC_MICROSATELLITE_REPEAT_TYPE));
     else if ( (*it) == "DISC_SUSPECT_MISC_FEATURES")
       tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_DISC_SUSPECT_MISC_FEATURES));
     else if ( (*it) == "DISC_CHECK_RNA_PRODUCTS_AND_COMMENTS")
       tests_on_Bioseq_CFeat.push_back(
             CRef <CTestAndRepData> (new CBioseq_DISC_CHECK_RNA_PRODUCTS_AND_COMMENTS));
     else if ( (*it) == "DISC_FEATURE_MOLTYPE_MISMATCH")
       tests_on_Bioseq_CFeat.push_back(
                CRef <CTestAndRepData> (new CBioseq_DISC_FEATURE_MOLTYPE_MISMATCH));
     else if ( (*it) == "ADJACENT_PSEUDOGENES")
       tests_on_Bioseq_CFeat.push_back(
                         CRef <CTestAndRepData> (new CBioseq_ADJACENT_PSEUDOGENES));
     else if ( (*it) == "MISSING_GENPRODSET_PROTEIN")
       tests_on_Bioseq_CFeat.push_back(
                CRef <CTestAndRepData> (new CBioseq_MISSING_GENPRODSET_PROTEIN));
     else if ( (*it) == "DUP_GENPRODSET_PROTEIN")
       tests_on_Bioseq_CFeat.push_back(
                      CRef <CTestAndRepData> (new CBioseq_DUP_GENPRODSET_PROTEIN));
     else if ( (*it) == "MISSING_GENPRODSET_TRANSCRIPT_ID")
       tests_on_Bioseq_CFeat.push_back(
                CRef <CTestAndRepData> (new CBioseq_MISSING_GENPRODSET_TRANSCRIPT_ID));
     else if ( (*it) == "DISC_DUP_GENPRODSET_TRANSCRIPT_ID")
       tests_on_Bioseq_CFeat.push_back(
                CRef <CTestAndRepData> (new CBioseq_DISC_DUP_GENPRODSET_TRANSCRIPT_ID));
     else if ( (*it) == "DISC_FEAT_OVERLAP_SRCFEAT")
       tests_on_Bioseq_CFeat.push_back(
                      CRef <CTestAndRepData> (new CBioseq_DISC_FEAT_OVERLAP_SRCFEAT));
     else if ( (*it) == "CDS_TRNA_OVERLAP")
       tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_CDS_TRNA_OVERLAP));
     else if ( (*it) == "TRANSL_NO_NOTE")
       tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_TRANSL_NO_NOTE));
     else if ( (*it) == "NOTE_NO_TRANSL")
       tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_NOTE_NO_TRANSL));
     else if ( (*it) == "TRANSL_TOO_LONG")
       tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_TRANSL_TOO_LONG));
     else if ( (*it) == "FIND_STRAND_TRNAS")
       tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_FIND_STRAND_TRNAS));
     else if ( (*it) == "FIND_BADLEN_TRNAS")
       tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_FIND_BADLEN_TRNAS));
     else if ( (*it) == "COUNT_TRNAS")
       tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_COUNT_TRNAS));
     else if ( (*it) == "FIND_DUP_TRNAS")
       tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_FIND_DUP_TRNAS));
     else if ( (*it) == "COUNT_RRNAS")
       tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_COUNT_RRNAS));
     else if ( (*it) == "FIND_DUP_RRNAS")
       tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_FIND_DUP_RRNAS));
     else if ( (*it) == "PARTIAL_CDS_COMPLETE_SEQUENCE")
       tests_on_Bioseq_CFeat.push_back(
           CRef <CTestAndRepData> (new CBioseq_PARTIAL_CDS_COMPLETE_SEQUENCE));
     else if ( (*it) == "CONTAINED_CDS")
       tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_CONTAINED_CDS));
     else if ( (*it) == "PSEUDO_MISMATCH")
       tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_PSEUDO_MISMATCH));
     else if ( (*it) == "EC_NUMBER_NOTE")
       tests_on_Bioseq_CFeat.push_back(
                    CRef <CTestAndRepData> (new CBioseq_EC_NUMBER_NOTE));
     else if ( (*it) == "NON_GENE_LOCUS_TAG")
       tests_on_Bioseq_CFeat.push_back(
                         CRef <CTestAndRepData> (new CBioseq_NON_GENE_LOCUS_TAG));
     else if ( (*it) == "JOINED_FEATURES")
       tests_on_Bioseq_CFeat.push_back(
                        CRef <CTestAndRepData> (new CBioseq_JOINED_FEATURES));
     else if ( (*it) == "SHOW_TRANSL_EXCEPT")
       tests_on_Bioseq_CFeat.push_back(
                         CRef <CTestAndRepData>( new CBioseq_SHOW_TRANSL_EXCEPT));
     else if ( (*it) == "MRNA_SHOULD_HAVE_PROTEIN_TRANSCRIPT_IDS")
       tests_on_Bioseq_CFeat.push_back(
          CRef <CTestAndRepData>( new CBioseq_MRNA_SHOULD_HAVE_PROTEIN_TRANSCRIPT_IDS));
     else if ( (*it) == "RRNA_NAME_CONFLICTS")
       tests_on_Bioseq_CFeat.push_back(
                         CRef <CTestAndRepData>(new CBioseq_RRNA_NAME_CONFLICTS));
     else if ( (*it) == "ONCALLER_GENE_MISSING")
       tests_on_Bioseq_CFeat.push_back(
                         CRef <CTestAndRepData>(new CBioseq_ONCALLER_GENE_MISSING));
     else if ( (*it) == "ONCALLER_SUPERFLUOUS_GENE")
       tests_on_Bioseq_CFeat.push_back(
                         CRef <CTestAndRepData>(new CBioseq_ONCALLER_SUPERFLUOUS_GENE));
     else if ( (*it) == "MISSING_GENES")
       tests_on_Bioseq_CFeat.push_back(
              CRef <CTestAndRepData>(new CBioseq_MISSING_GENES));
     else if ( (*it) == "EXTRA_GENES") {
       tests_on_Bioseq_CFeat.push_back(CRef <CTestAndRepData>(new CBioseq_EXTRA_GENES));
       //???tests_on_Bioseq_CFeat.push_back(CRef <CTestAndRepData>(new CBioseq_EXTRA_MISSING_GENES));
    }
     else if ( (*it) == "OVERLAPPING_CDS")
       tests_on_Bioseq_CFeat.push_back(CRef <CTestAndRepData>(new CBioseq_OVERLAPPING_CDS));
     else if ( (*it) == "RNA_CDS_OVERLAP")
       tests_on_Bioseq_CFeat.push_back(CRef <CTestAndRepData>(new CBioseq_RNA_CDS_OVERLAP));
     else if ( (*it) == "FIND_OVERLAPPED_GENES")
       tests_on_Bioseq_CFeat.push_back(
                         CRef <CTestAndRepData>(new CBioseq_FIND_OVERLAPPED_GENES));
     else if ( (*it) == "OVERLAPPING_GENES")
       tests_on_Bioseq_CFeat.push_back(
                         CRef <CTestAndRepData>(new CBioseq_OVERLAPPING_GENES));
     else if ( (*it) == "DISC_PROTEIN_NAMES")
       tests_on_Bioseq_CFeat.push_back(
                   CRef <CTestAndRepData>( new CBioseq_DISC_PROTEIN_NAMES));
     else if ( (*it) == "DISC_CDS_PRODUCT_FIND")
       tests_on_Bioseq_CFeat.push_back(
                   CRef <CTestAndRepData>( new CBioseq_DISC_CDS_PRODUCT_FIND));
     else if ( (*it) == "EC_NUMBER_ON_UNKNOWN_PROTEIN")
       tests_on_Bioseq_CFeat.push_back(
                   CRef <CTestAndRepData>( new CBioseq_EC_NUMBER_ON_UNKNOWN_PROTEIN));
     else if ( (*it) == "RNA_NO_PRODUCT")
       tests_on_Bioseq_CFeat.push_back(
                   CRef <CTestAndRepData>(new CBioseq_RNA_NO_PRODUCT));
     else if ( (*it) == "DISC_SHORT_INTRON")
       tests_on_Bioseq_CFeat.push_back(
                         CRef <CTestAndRepData>(new CBioseq_DISC_SHORT_INTRON));
     else if ( (*it) == "DISC_BAD_GENE_STRAND")
       tests_on_Bioseq_CFeat.push_back(
                         CRef <CTestAndRepData>(new CBioseq_DISC_BAD_GENE_STRAND));
     else if ( (*it) == "DISC_INTERNAL_TRANSCRIBED_SPACER_RRNA")
       tests_on_Bioseq_CFeat.push_back(
             CRef <CTestAndRepData>(new CBioseq_DISC_INTERNAL_TRANSCRIBED_SPACER_RRNA));
     else if ( (*it) == "DISC_SHORT_RRNA")
       tests_on_Bioseq_CFeat.push_back(
                        CRef <CTestAndRepData>(new CBioseq_DISC_SHORT_RRNA));
     else if ( (*it) == "TEST_OVERLAPPING_RRNAS")
       tests_on_Bioseq_CFeat.push_back(
                        CRef <CTestAndRepData>(new CBioseq_TEST_OVERLAPPING_RRNAS));
     else if ( (*it) == "HYPOTHETICAL_CDS_HAVING_GENE_NAME")
       tests_on_Bioseq_CFeat.push_back(
               CRef <CTestAndRepData>( new CBioseq_HYPOTHETICAL_CDS_HAVING_GENE_NAME));
     else if ( (*it) == "DISC_SUSPICIOUS_NOTE_TEXT")
       tests_on_Bioseq_CFeat.push_back(
                  CRef <CTestAndRepData>( new CBioseq_DISC_SUSPICIOUS_NOTE_TEXT));
     else if ( (*it) == "NO_ANNOTATION")
       tests_on_Bioseq_CFeat.push_back(
                           CRef <CTestAndRepData>(new CBioseq_NO_ANNOTATION));
     else if ( (*it) == "DISC_LONG_NO_ANNOTATION")
       tests_on_Bioseq_CFeat.push_back( 
              CRef <CTestAndRepData>( new CBioseq_DISC_LONG_NO_ANNOTATION));
     else if ( (*it) == "DISC_PARTIAL_PROBLEMS")
       tests_on_Bioseq_CFeat.push_back( 
              CRef <CTestAndRepData>( new CBioseq_DISC_PARTIAL_PROBLEMS));
     else if ( (*it) == "TEST_UNUSUAL_MISC_RNA")
       tests_on_Bioseq_CFeat.push_back( 
              CRef <CTestAndRepData>( new CBioseq_TEST_UNUSUAL_MISC_RNA));
     else if ( (*it) == "GENE_PRODUCT_CONFLICT")
       tests_on_Bioseq_CFeat.push_back( 
                     CRef <CTestAndRepData>( new CBioseq_GENE_PRODUCT_CONFLICT));
     else if ( (*it) == "DISC_CDS_WITHOUT_MRNA")
       tests_on_Bioseq_CFeat.push_back( 
                     CRef <CTestAndRepData>( new CBioseq_DISC_CDS_WITHOUT_MRNA));
     else if ( (*it) == "DUPLICATE_GENE_LOCUS")
       tests_on_Bioseq_CFeat_NotInGenProdSet.push_back(
                          CRef <CTestAndRepData>(new CBioseq_DUPLICATE_GENE_LOCUS));
     else if ( (*it) == "LOCUS_TAGS")
       tests_on_Bioseq_CFeat_NotInGenProdSet.push_back(
                                   CRef <CTestAndRepData>(new CBioseq_LOCUS_TAGS));
     else if ( (*it) == "FEATURE_LOCATION_CONFLICT")
       tests_on_Bioseq_CFeat_NotInGenProdSet.push_back(
                          CRef <CTestAndRepData>(new CBioseq_FEATURE_LOCATION_CONFLICT));
     else if ( (*it) == "DISC_INCONSISTENT_MOLINFO_TECH")
       tests_on_Bioseq_NotInGenProdSet.push_back(
                     CRef<CTestAndRepData>(new CBioseq_DISC_INCONSISTENT_MOLINFO_TECH));
     else if ( (*it) == "SHORT_CONTIG")
       tests_on_Bioseq_NotInGenProdSet.push_back(
                                 CRef<CTestAndRepData>(new CBioseq_SHORT_CONTIG));
     else if ( (*it) == "SHORT_SEQUENCES")
       tests_on_Bioseq_NotInGenProdSet.push_back(
                                     CRef<CTestAndRepData>(new CBioseq_SHORT_SEQUENCES));
     else if ( (*it) == "SHORT_SEQUENCES_200")
       tests_on_Bioseq_NotInGenProdSet.push_back(
                                CRef<CTestAndRepData>(new CBioseq_SHORT_SEQUENCES_200));
     else if ( (*it) == "TEST_UNWANTED_SPACER")
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                     CRef <CTestAndRepData>(new CBioseq_TEST_UNWANTED_SPACER));
     else if ( (*it) == "TEST_UNNECESSARY_VIRUS_GENE")
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                 CRef <CTestAndRepData>(new CBioseq_TEST_UNNECESSARY_VIRUS_GENE));
     else if ( (*it) == "TEST_ORGANELLE_NOT_GENOMIC")
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                 CRef <CTestAndRepData>(new CBioseq_TEST_ORGANELLE_NOT_GENOMIC));
     else if ( (*it) == "MULTIPLE_CDS_ON_MRNA")
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                    CRef <CTestAndRepData>(new CBioseq_MULTIPLE_CDS_ON_MRNA));
     else if ( (*it) == "TEST_MRNA_SEQUENCE_MINUS_ST")
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                        CRef <CTestAndRepData>(new CBioseq_TEST_MRNA_SEQUENCE_MINUS_ST));
     else if ( (*it) == "TEST_BAD_MRNA_QUAL")
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                            CRef <CTestAndRepData>(new CBioseq_TEST_BAD_MRNA_QUAL));
     else if ( (*it) == "TEST_EXON_ON_MRNA")
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                          CRef <CTestAndRepData>(new CBioseq_TEST_EXON_ON_MRNA));
     else if ( (*it) == "ONCALLER_HIV_RNA_INCONSISTENT")
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                     CRef <CTestAndRepData>( new CBioseq_ONCALLER_HIV_RNA_INCONSISTENT));
     else if ( (*it) == "DISC_BACTERIAL_PARTIAL_NONEXTENDABLE_EXCEPTION")
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(CRef <CTestAndRepData>( 
                           new CBioseq_DISC_BACTERIAL_PARTIAL_NONEXTENDABLE_EXCEPTION));
     else if ( (*it) == "DISC_BACTERIAL_PARTIAL_NONEXTENDABLE_PROBLEMS")
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(CRef <CTestAndRepData>(
                            new CBioseq_DISC_BACTERIAL_PARTIAL_NONEXTENDABLE_PROBLEMS));
     else if ( (*it) == "DISC_MITOCHONDRION_REQUIRED")
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                    CRef <CTestAndRepData>( new CBioseq_DISC_MITOCHONDRION_REQUIRED));
     else if ( (*it) == "EUKARYOTE_SHOULD_HAVE_MRNA")
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                     CRef <CTestAndRepData>( new CBioseq_EUKARYOTE_SHOULD_HAVE_MRNA));
     else if ( (*it) == "RNA_PROVIRAL")
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                         CRef <CTestAndRepData>( new CBioseq_RNA_PROVIRAL));
     else if ( (*it) == "NON_RETROVIRIDAE_PROVIRAL")
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                    CRef <CTestAndRepData>( new CBioseq_NON_RETROVIRIDAE_PROVIRAL));
     else if ( (*it) == "DISC_RETROVIRIDAE_DNA")
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                        CRef <CTestAndRepData>( new CBioseq_DISC_RETROVIRIDAE_DNA));
     else if ( (*it) == "DISC_mRNA_ON_WRONG_SEQUENCE_TYPE")
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
              CRef <CTestAndRepData>( new CBioseq_DISC_mRNA_ON_WRONG_SEQUENCE_TYPE));
     else if ( (*it) == "DISC_RBS_WITHOUT_GENE")
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                         CRef <CTestAndRepData>(new CBioseq_DISC_RBS_WITHOUT_GENE));
     else if ( (*it) == "DISC_EXON_INTRON_CONFLICT")
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                      CRef <CTestAndRepData>(new CBioseq_DISC_EXON_INTRON_CONFLICT));
     else if ( (*it) == "TEST_TAXNAME_NOT_IN_DEFLINE")
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                     CRef <CTestAndRepData>(new CBioseq_TEST_TAXNAME_NOT_IN_DEFLINE));
     else if ( (*it) == "INCONSISTENT_SOURCE_DEFLINE")
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                     CRef <CTestAndRepData>(new CBioseq_INCONSISTENT_SOURCE_DEFLINE));
     else if ( (*it) == "DISC_BACTERIA_SHOULD_NOT_HAVE_MRNA")
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
               CRef <CTestAndRepData>(new CBioseq_DISC_BACTERIA_SHOULD_NOT_HAVE_MRNA));
     else if ( (*it) == "TEST_BAD_GENE_NAME")
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                            CRef <CTestAndRepData> (new CBioseq_TEST_BAD_GENE_NAME));
     else if ( (*it) == "MOLTYPE_NOT_MRNA")
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                             CRef <CTestAndRepData> (new CBioseq_MOLTYPE_NOT_MRNA));
     else if ( (*it) == "TECHNIQUE_NOT_TSA")
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                             CRef <CTestAndRepData> (new CBioseq_TECHNIQUE_NOT_TSA));
     else if ( (*it) == "SHORT_PROT_SEQUENCES")
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                         CRef <CTestAndRepData> (new CBioseq_SHORT_PROT_SEQUENCES));
     else if ( (*it) == "TEST_COUNT_UNVERIFIED")
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                   CRef <CTestAndRepData>(new CBioseq_TEST_COUNT_UNVERIFIED));
     else if ( (*it) == "TEST_DUP_GENES_OPPOSITE_STRANDS")
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
                CRef <CTestAndRepData>(new CBioseq_TEST_DUP_GENES_OPPOSITE_STRANDS));
     else if ( (*it) == "DISC_GENE_PARTIAL_CONFLICT")
       tests_on_Bioseq_CFeat_CSeqdesc.push_back(
               CRef <CTestAndRepData>(new CBioseq_DISC_GENE_PARTIAL_CONFLICT));
     else if ( (*it) == "DISC_FLATFILE_FIND_ONCALLER")
       tests_on_SeqEntry.push_back(
             CRef <CTestAndRepData>(new CSeqEntry_DISC_FLATFILE_FIND_ONCALLER));
     else if ( (*it) == "DISC_FEATURE_COUNT") // asndisc version   
       tests_on_SeqEntry.push_back(CRef <CTestAndRepData>(new CSeqEntry_DISC_FEATURE_COUNT));
     else if ( (*it) == "DISC_INCONSISTENT_STRUCTURED_COMMENTS")
       tests_on_SeqEntry_feat_desc.push_back( 
            CRef <CTestAndRepData>(new CSeqEntry_DISC_INCONSISTENT_STRUCTURED_COMMENTS));
     else if ( (*it) == "DISC_INCONSISTENT_DBLINK")
       tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_DISC_INCONSISTENT_DBLINK));
     else if ( (*it) == "END_COLON_IN_COUNTRY")
       tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_END_COLON_IN_COUNTRY));
     else if ( (*it) == "ONCALLER_SUSPECTED_ORG_COLLECTED")
       tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_SUSPECTED_ORG_COLLECTED));
     else if ( (*it) == "ONCALLER_SUSPECTED_ORG_IDENTIFIED")
       tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_SUSPECTED_ORG_IDENTIFIED));
     else if ( (*it) == "ONCALLER_MORE_NAMES_COLLECTED_BY")
       tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_MORE_NAMES_COLLECTED_BY));
     else if ( (*it) == "ONCALLER_STRAIN_TAXNAME_CONFLICT")
       tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_STRAIN_TAXNAME_CONFLICT));
     else if ( (*it) == "TEST_SMALL_GENOME_SET_PROBLEM")
       tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_TEST_SMALL_GENOME_SET_PROBLEM));
     else if ( (*it) == "DISC_INCONSISTENT_MOLTYPES")
       tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_DISC_INCONSISTENT_MOLTYPES));
     else if ( (*it) == "DISC_BIOMATERIAL_TAXNAME_MISMATCH")
       tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_DISC_BIOMATERIAL_TAXNAME_MISMATCH));
     else if ( (*it) == "DISC_CULTURE_TAXNAME_MISMATCH")
       tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_DISC_CULTURE_TAXNAME_MISMATCH));
     else if ( (*it) == "DISC_STRAIN_TAXNAME_MISMATCH")
       tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_DISC_STRAIN_TAXNAME_MISMATCH));
     else if ( (*it) == "DISC_SPECVOUCHER_TAXNAME_MISMATCH")
       tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_DISC_SPECVOUCHER_TAXNAME_MISMATCH));
     else if ( (*it) == "DISC_HAPLOTYPE_MISMATCH")
       tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_DISC_HAPLOTYPE_MISMATCH));
     else if ( (*it) == "DISC_MISSING_VIRAL_QUALS")
       tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_DISC_MISSING_VIRAL_QUALS));
     else if ( (*it) == "DISC_INFLUENZA_DATE_MISMATCH")
       tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_DISC_INFLUENZA_DATE_MISMATCH));
     else if ( (*it) == "TAX_LOOKUP_MISSING")
       tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_TAX_LOOKUP_MISSING));
     else if ( (*it) == "TAX_LOOKUP_MISMATCH")
       tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_TAX_LOOKUP_MISMATCH));
     else if ( (*it) == "MISSING_STRUCTURED_COMMENT")
       tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_MISSING_STRUCTURED_COMMENT));
     else if ( (*it) == "ONCALLER_MISSING_STRUCTURED_COMMENTS")
       tests_on_SeqEntry_feat_desc.push_back( 
             CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_MISSING_STRUCTURED_COMMENTS));
     else if ( (*it) == "DISC_MISSING_AFFIL")
       tests_on_SeqEntry_feat_desc.push_back( 
                      CRef <CTestAndRepData>(new CSeqEntry_DISC_MISSING_AFFIL));
     else if ( (*it) == "DISC_CITSUBAFFIL_CONFLICT")
       tests_on_SeqEntry_feat_desc.push_back( 
                      CRef <CTestAndRepData>(new CSeqEntry_DISC_CITSUBAFFIL_CONFLICT));
     else if ( (*it) == "DISC_TITLE_AUTHOR_CONFLICT")
       tests_on_SeqEntry_feat_desc.push_back( 
                      CRef <CTestAndRepData>(new CSeqEntry_DISC_TITLE_AUTHOR_CONFLICT));
     else if ( (*it) == "DISC_USA_STATE")
       tests_on_SeqEntry_feat_desc.push_back( 
                      CRef <CTestAndRepData>(new CSeqEntry_DISC_USA_STATE));
     else if ( (*it) == "DISC_CITSUB_AFFIL_DUP_TEXT")
       tests_on_SeqEntry_feat_desc.push_back( 
                      CRef <CTestAndRepData>(new CSeqEntry_DISC_CITSUB_AFFIL_DUP_TEXT));
     else if ( (*it) == "DISC_REQUIRED_CLONE")
       tests_on_SeqEntry_feat_desc.push_back( 
                            CRef <CTestAndRepData>(new CSeqEntry_DISC_REQUIRED_CLONE));
     else if ( (*it) == "ONCALLER_MULTISRC")
       tests_on_SeqEntry_feat_desc.push_back( 
                            CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_MULTISRC));
     else if ( (*it) == "DISC_SRC_QUAL_PROBLEM")
       tests_on_SeqEntry_feat_desc.push_back(
                       CRef <CTestAndRepData>(new CSeqEntry_DISC_SRC_QUAL_PROBLEM));
     else if ( (*it) == "DISC_SOURCE_QUALS_ASNDISC") // asndisc version
       tests_on_SeqEntry_feat_desc.push_back(
                       CRef <CTestAndRepData>(new CSeqEntry_DISC_SOURCE_QUALS_ASNDISC));
     else if ( (*it) == "DISC_UNPUB_PUB_WITHOUT_TITLE")
       tests_on_SeqEntry_feat_desc.push_back(
                    CRef <CTestAndRepData>(new CSeqEntry_DISC_UNPUB_PUB_WITHOUT_TITLE));
     else if ( (*it) == "ONCALLER_CONSORTIUM")
       tests_on_SeqEntry_feat_desc.push_back(
                       CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_CONSORTIUM));
     else if ( (*it) == "DISC_CHECK_AUTH_NAME")
       tests_on_SeqEntry_feat_desc.push_back(
                          CRef <CTestAndRepData>(new CSeqEntry_DISC_CHECK_AUTH_NAME));
     else if ( (*it) == "DISC_CHECK_AUTH_CAPS")
       tests_on_SeqEntry_feat_desc.push_back(
                          CRef <CTestAndRepData>(new CSeqEntry_DISC_CHECK_AUTH_CAPS));
     else if ( (*it) == "DISC_MISMATCHED_COMMENTS")
       tests_on_SeqEntry_feat_desc.push_back(
                       CRef <CTestAndRepData>(new CSeqEntry_DISC_MISMATCHED_COMMENTS));
     else if ( (*it) == "ONCALLER_COMMENT_PRESENT")
       tests_on_SeqEntry_feat_desc.push_back(
                       CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_COMMENT_PRESENT));
     else if ( (*it) == "DUP_DISC_ATCC_CULTURE_CONFLICT")
       tests_on_SeqEntry_feat_desc.push_back(
                 CRef <CTestAndRepData>(new CSeqEntry_DUP_DISC_ATCC_CULTURE_CONFLICT));
     else if ( (*it) == "ONCALLER_STRAIN_CULTURE_COLLECTION_MISMATCH")
       tests_on_SeqEntry_feat_desc.push_back(CRef <CTestAndRepData>(
                          new CSeqEntry_ONCALLER_STRAIN_CULTURE_COLLECTION_MISMATCH));
     else if ( (*it) == "DISC_DUP_DEFLINE")
       tests_on_SeqEntry_feat_desc.push_back(
                         CRef <CTestAndRepData>(new CSeqEntry_DISC_DUP_DEFLINE));
     else if ( (*it) == "DISC_TITLE_ENDS_WITH_SEQUENCE")
       tests_on_SeqEntry_feat_desc.push_back(
                    CRef <CTestAndRepData>(new CSeqEntry_DISC_TITLE_ENDS_WITH_SEQUENCE));
     else if ( (*it) == "DISC_MISSING_DEFLINES")
       tests_on_SeqEntry_feat_desc.push_back(
                         CRef <CTestAndRepData>(new CSeqEntry_DISC_MISSING_DEFLINES));
     else if ( (*it) == "ONCALLER_DEFLINE_ON_SET")
       tests_on_SeqEntry_feat_desc.push_back(
                         CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_DEFLINE_ON_SET));
     else if ( (*it) == "DISC_BACTERIAL_TAX_STRAIN_MISMATCH")
       tests_on_SeqEntry_feat_desc.push_back(
              CRef <CTestAndRepData>(new CSeqEntry_DISC_BACTERIAL_TAX_STRAIN_MISMATCH));
     else if ( (*it) == "DUP_DISC_CBS_CULTURE_CONFLICT")
       tests_on_SeqEntry_feat_desc.push_back(
                   CRef <CTestAndRepData>(new CSeqEntry_DUP_DISC_CBS_CULTURE_CONFLICT));
     else if ( (*it) == "INCONSISTENT_BIOSOURCE")
       tests_on_SeqEntry_feat_desc.push_back(
                          CRef <CTestAndRepData>(new CSeqEntry_INCONSISTENT_BIOSOURCE));
     else if ( (*it) == "ONCALLER_BIOPROJECT_ID")
       tests_on_SeqEntry_feat_desc.push_back(
                       CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_BIOPROJECT_ID));
     else if ( (*it) == "DISC_INCONSISTENT_STRUCTURED_COMMENTS")
       tests_on_SeqEntry_feat_desc.push_back(
            CRef <CTestAndRepData>(new CSeqEntry_DISC_INCONSISTENT_STRUCTURED_COMMENTS));
     else if ( (*it) == "ONCALLER_SWITCH_STRUCTURED_COMMENT_PREFIX")
       tests_on_SeqEntry_feat_desc.push_back(CRef <CTestAndRepData>(
                           new CSeqEntry_ONCALLER_SWITCH_STRUCTURED_COMMENT_PREFIX));
     else if ( (*it) == "MISSING_GENOMEASSEMBLY_COMMENTS")
       tests_on_SeqEntry_feat_desc.push_back(
                 CRef <CTestAndRepData>(new CSeqEntry_MISSING_GENOMEASSEMBLY_COMMENTS));
     else if ( (*it) == "TEST_HAS_PROJECT_ID")
       tests_on_SeqEntry_feat_desc.push_back(
                            CRef <CTestAndRepData>(new CSeqEntry_TEST_HAS_PROJECT_ID));
     else if ( (*it) == "DISC_TRINOMIAL_SHOULD_HAVE_QUALIFIER")
       tests_on_SeqEntry_feat_desc.push_back(
             CRef <CTestAndRepData>(new CSeqEntry_DISC_TRINOMIAL_SHOULD_HAVE_QUALIFIER));
     else if ( (*it) == "ONCALLER_DUPLICATE_PRIMER_SET")
       tests_on_SeqEntry_feat_desc.push_back(
                    CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_DUPLICATE_PRIMER_SET));
     else if ( (*it) == "ONCALLER_COUNTRY_COLON")
       tests_on_SeqEntry_feat_desc.push_back(
                         CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_COUNTRY_COLON));
     else if ( (*it) == "TEST_MISSING_PRIMER")
       tests_on_SeqEntry_feat_desc.push_back(
                         CRef <CTestAndRepData>(new CSeqEntry_TEST_MISSING_PRIMER));
     else if ( (*it) == "TEST_SP_NOT_UNCULTURED")
       tests_on_SeqEntry_feat_desc.push_back(
                         CRef <CTestAndRepData>(new CSeqEntry_TEST_SP_NOT_UNCULTURED));
     else if ( (*it) == "DISC_METAGENOMIC")
       tests_on_SeqEntry_feat_desc.push_back(
                         CRef <CTestAndRepData>(new CSeqEntry_DISC_METAGENOMIC));
     else if ( (*it) == "DISC_MAP_CHROMOSOME_CONFLICT")
       tests_on_SeqEntry_feat_desc.push_back(
                    CRef <CTestAndRepData>(new CSeqEntry_DISC_MAP_CHROMOSOME_CONFLICT));
     else if ( (*it) == "DIVISION_CODE_CONFLICTS")
       tests_on_SeqEntry_feat_desc.push_back(
                         CRef <CTestAndRepData>(new CSeqEntry_DIVISION_CODE_CONFLICTS));
     else if ( (*it) == "TEST_AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE")
       tests_on_SeqEntry_feat_desc.push_back( CRef <CTestAndRepData>(
                         new CSeqEntry_TEST_AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE));
     else if ( (*it) == "TEST_UNNECESSARY_ENVIRONMENTAL")
       tests_on_SeqEntry_feat_desc.push_back(
                   CRef <CTestAndRepData>(new CSeqEntry_TEST_UNNECESSARY_ENVIRONMENTAL));
     else if ( (*it) == "DISC_HUMAN_HOST")
       tests_on_SeqEntry_feat_desc.push_back(
                   CRef <CTestAndRepData>(new CSeqEntry_DISC_HUMAN_HOST));
     else if ( (*it) == "ONCALLER_MULTIPLE_CULTURE_COLLECTION")
       tests_on_SeqEntry_feat_desc.push_back(
             CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_MULTIPLE_CULTURE_COLLECTION));
     else if ( (*it) == "ONCALLER_CHECK_AUTHORITY")
       tests_on_SeqEntry_feat_desc.push_back(
                         CRef <CTestAndRepData>(new CSeqEntry_ONCALLER_CHECK_AUTHORITY));
     else if ( (*it) == "DISC_METAGENOME_SOURCE")
       tests_on_SeqEntry_feat_desc.push_back(
                         CRef <CTestAndRepData>(new CSeqEntry_DISC_METAGENOME_SOURCE));
     else if ( (*it) == "DISC_BACTERIA_MISSING_STRAIN")
       tests_on_SeqEntry_feat_desc.push_back(
                     CRef <CTestAndRepData>(new CSeqEntry_DISC_BACTERIA_MISSING_STRAIN));
     else if ( (*it) == "DISC_REQUIRED_STRAIN")
       tests_on_SeqEntry_feat_desc.push_back(
                            CRef <CTestAndRepData>(new CSeqEntry_DISC_REQUIRED_STRAIN));
     else if ( (*it) == "MORE_OR_SPEC_NAMES_IDENTIFIED_BY")
       tests_on_SeqEntry_feat_desc.push_back(
                CRef <CTestAndRepData>(new CSeqEntry_MORE_OR_SPEC_NAMES_IDENTIFIED_BY));
     else if ( (*it) == "MORE_NAMES_COLLECTED_BY")
       tests_on_SeqEntry_feat_desc.push_back(
                       CRef <CTestAndRepData>(new CSeqEntry_MORE_NAMES_COLLECTED_BY));
     else if ( (*it) == "MISSING_PROJECT")
       tests_on_SeqEntry_feat_desc.push_back(
                       CRef <CTestAndRepData>(new CSeqEntry_MISSING_PROJECT));
     else if ( (*it) == "DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE")
       tests_on_SeqEntry_feat_desc.push_back( 
          CRef <CTestAndRepData>( new CSeqEntry_DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE));
     else if ( (*it) == "DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE")
       tests_on_SeqEntry_feat_desc.push_back(CRef <CTestAndRepData>(
                       new CSeqEntry_DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE1)); // not tested
     else if ( (*it) == "DISC_SEGSETS_PRESENT")
       tests_on_BioseqSet.push_back(
                       CRef <CTestAndRepData>(new CBioseqSet_DISC_SEGSETS_PRESENT));
     else if ( (*it) == "TEST_UNWANTED_SET_WRAPPER")
       tests_on_BioseqSet.push_back(
                       CRef <CTestAndRepData>(new CBioseqSet_TEST_UNWANTED_SET_WRAPPER));
     else if ( (*it) == "DISC_NONWGS_SETS_PRESENT")
       tests_on_BioseqSet.push_back(
                       CRef <CTestAndRepData>(new CBioseqSet_DISC_NONWGS_SETS_PRESENT));
     else NCBI_THROW(CException, eUnknown, "The requested test is unrecognizable."); 
   }
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
   if (report_type == "Discrepancy") cate_flag = fDiscrepancy;
   else if(report_type ==  "Oncaller") cate_flag = fOncaller;
   else NCBI_THROW(CException, eUnknown, "Unrecognized report type.");

   for (unsigned i=0; i< sizeof(test_list)/sizeof(s_test_property); i++) {
      if (test_list[i].category & cate_flag) 
                tests_run.insert(test_list[i].name);
   }
   
   ITERATE (vector <string>, it, thisInfo.tests_enabled) {
      if (tests_run.find(*it) == tests_run.end()) 
            tests_run.insert(*it);
   }
   ITERATE (vector <string>, it, thisInfo.tests_disabled) {
      if (tests_run.find(*it) != tests_run.end())
           tests_run.erase(*it);
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
   tests_on_Bioseq_CFeat.push_back( 
                CRef <CTestAndRepData> (new CBioseq_on_SUSPECT_RULE));
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
  

