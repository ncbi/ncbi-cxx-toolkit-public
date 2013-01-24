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
static CRef <CRuleProperties> rule_prop (new CRuleProperties);
static string       strtmp;

vector < CRef <CTestAndRepData> > CRepConfig :: tests_on_Bioseq;
vector < CRef <CTestAndRepData> > CRepConfig :: tests_on_Bioseq_aa;
vector < CRef <CTestAndRepData> > CRepConfig :: tests_on_Bioseq_na;
vector < CRef <CTestAndRepData> > CRepConfig :: tests_on_Bioseq_CFeat;
vector < CRef <CTestAndRepData> > CRepConfig :: tests_on_Bioseq_CFeat_NotInGenProdSet;
vector < CRef <CTestAndRepData> > CRepConfig :: tests_on_Bioseq_NotInGenProdSet;
vector < CRef <CTestAndRepData> > CRepConfig :: tests_on_Bioseq_CFeat_CSeqdesc;
vector < CRef <CTestAndRepData> > CRepConfig :: tests_on_GenProdSetFeat;
vector < CRef <CTestAndRepData> > CRepConfig :: tests_on_SeqFeat;
vector < CRef <CTestAndRepData> > CRepConfig :: tests_on_SeqEntry;
vector < CRef <CTestAndRepData> > CRepConfig :: tests_on_SeqEntry_feat_desc;
vector < CRef <CTestAndRepData> > CRepConfig :: tests_4_once;
vector < CRef <CTestAndRepData> > CRepConfig :: tests_on_BioseqSet;
vector < CRef <CTestAndRepData> > CRepConfig :: tests_on_SubmitBlk;

CRepConfig* CRepConfig :: factory(const string& report_tp)
{
   if (report_tp == "Discrepancy") return new CRepConfDiscrepancy();
   else if(report_tp ==  "Oncaller") return new  CRepConfOncaller();
   else NCBI_THROW(CException, eUnknown, "Unrecognized report type.");
};  // CRepConfig::factory()



void CRepConfig :: Init()
{
   tests_on_Bioseq.reserve(50);
   tests_on_Bioseq_aa.reserve(50);
   tests_on_Bioseq_na.reserve(50);
   tests_on_Bioseq_CFeat.reserve(50);
   tests_on_Bioseq_NotInGenProdSet.reserve(50);
   tests_on_Bioseq_CFeat_CSeqdesc.reserve(50);
   tests_on_GenProdSetFeat.reserve(50);
   tests_on_SeqEntry.reserve(50);
   tests_on_SeqEntry_feat_desc.reserve(50);
   tests_on_BioseqSet.reserve(50);
   tests_on_SubmitBlk.reserve(1);
  
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
                CRef <CTestAndRepData> (new CBioseq_DISC_GAPS));
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
   //tests_on_Bioseq_CFeat.push_back(CRef <CTestAndRepData>(new CBioseq_SUSPECT_PRODUCT_NAMES));

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

// tests_on_GenProdSetFeat


// tests_on_SeqEntry
   /* not yet ready to imple. 
   tests_on_SeqEntry.push_back(CRef <CTestAndRepData>(
                                new CSeqEntry_DISC_FLATFILE_FIND_ONCALLER));
   */
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

// ini.
   // search function empty?
   const CSuspect_rule_set::Tdata& rules = thisInfo.suspect_rules->Get();
   rule_prop->srch_func_empty.reserve(rules.size());   // necessary for static vector?
   rule_prop->except_empty.reserve(rules.size());
   unsigned i=0;
   ITERATE (CSuspect_rule_set::Tdata, it, rules) {
      rule_prop->srch_func_empty[i] = rule_prop->IsSearchFuncEmpty((*it)->GetFind());
      rule_prop->except_empty[i] = 
       ((*it)->IsSetExcept()) ? rule_prop->IsSearchFuncEmpty((*it)->GetExcept()) : true;
      i++;
   };

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
  

