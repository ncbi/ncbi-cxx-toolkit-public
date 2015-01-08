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
 * Author:  Colleen Bollin
 *
 * File Description:
 *   class for storing results for Discrepancy Report
 *
 * $Log:$ 
 */

#include <ncbi_pch.hpp>

#include <misc/discrepancy_report/config.hpp>
#include <misc/discrepancy_report/analysis_process.hpp>

BEGIN_NCBI_SCOPE;

USING_NCBI_SCOPE;
USING_SCOPE(DiscRepNmSpc);


void CReportConfig::Configure(EConfigType config_type)
{
    m_TestList.clear();
    vector<string> implemented = CAnalysisProcessFactory::Implemented();


    switch (config_type) {
        case eConfigTypeDefault:
            ITERATE(vector<string>, it, implemented) {
                m_TestList[*it] = true;
            }
            m_TestList[kDISC_SOURCE_QUALS_ASNDISC] = false;
            m_TestList[kUNCULTURED_NOTES_ONCALLER] = false;
            m_TestList[kONCALLER_COUNTRY_COLON] = false;
            m_TestList[kEND_COLON_IN_COUNTRY] = false;
            m_TestList[kONCALLER_MORE_NAMES_COLLECTED_BY] = false;
            m_TestList[kONCALLER_MORE_OR_SPEC_NAMES_IDENTIFIED_BY] = false;
            m_TestList[kONCALLER_SUSPECTED_ORG_IDENTIFIED] = false;
            m_TestList[kONCALLER_SUSPECTED_ORG_COLLECTED] = false;
            m_TestList[kONCALLER_STRAIN_TAXNAME_CONFLICT] = false;
            m_TestList[kONCALLER_BIOPROJECT_ID] = false;
            m_TestList[kDIVISION_CODE_CONFLICTS] = false;
            m_TestList[kDUP_DISC_CBS_CULTURE_CONFLICT] = false;
            m_TestList[kMULTIPLE_CDS_ON_MRNA] = false;
            m_TestList[kRNA_PROVIRAL] = false;
            m_TestList[kNON_RETROVIRIDAE_PROVIRAL] = false;
            m_TestList[kDISC_CHECK_AUTH_NAME] = false;
            m_TestList[kDISC_CULTURE_TAXNAME_MISMATCH] = false;
            m_TestList[kDISC_BIOMATERIAL_TAXNAME_MISMATCH] = false;
            m_TestList[kTEST_MRNA_OVERLAPPING_PSEUDO_GENE] = false;
            m_TestList[kDISC_MISSING_VIRAL_QUALS] = false;
            m_TestList[kDISC_MISSING_SRC_QUAL] = false;
            m_TestList[kDISC_DUP_SRC_QUAL] = false;
            m_TestList[kDISC_DUP_SRC_QUAL_DATA] = false;
            m_TestList[kDISC_HAPLOTYPE_MISMATCH] = false;
            m_TestList[kDISC_FEATURE_MOLTYPE_MISMATCH] = false;
            m_TestList[kDISC_CDS_WITHOUT_MRNA] = false;
            m_TestList[kDISC_EXON_INTRON_CONFLICT] = false;
            m_TestList[kDISC_FEATURE_COUNT] = false;
            m_TestList[kDISC_SPECVOUCHER_TAXNAME_MISMATCH] = false;
            m_TestList[kDISC_GENE_PARTIAL_CONFLICT] = false;
            m_TestList[kDISC_FLATFILE_FIND_ONCALLER] = false;
            m_TestList[kDISC_FLATFILE_FIND_ONCALLER_FIXABLE] = false;
            m_TestList[kDISC_FLATFILE_FIND_ONCALLER_UNFIXABLE] = false;
            m_TestList[kDISC_CDS_PRODUCT_FIND] = false;
            m_TestList[kDISC_DUP_DEFLINE] = false;
            m_TestList[kDISC_COUNT_NUCLEOTIDES] = false;
            m_TestList[kDUP_DISC_ATCC_CULTURE_CONFLICT] = false;
            m_TestList[kDISC_USA_STATE] = false;
            m_TestList[kDISC_INCONSISTENT_MOLTYPES] = false;
            m_TestList[kDISC_SRC_QUAL_PROBLEM] = false;
            m_TestList[kDISC_SUBMITBLOCK_CONFLICT] = false;
            m_TestList[kDISC_POSSIBLE_LINKER] = false;
            m_TestList[kDISC_TITLE_AUTHOR_CONFLICT] = false;
            m_TestList[kDISC_BAD_GENE_STRAND] = false;
            m_TestList[kDISC_MAP_CHROMOSOME_CONFLICT] = false;
            m_TestList[kDISC_RBS_WITHOUT_GENE] = false;
            m_TestList[kDISC_CITSUBAFFIL_CONFLICT] = false;
            m_TestList[kDISC_REQUIRED_CLONE] = false;
            m_TestList[kDISC_SUSPICIOUS_NOTE_TEXT] = false;
            m_TestList[kDISC_mRNA_ON_WRONG_SEQUENCE_TYPE] = false;
            m_TestList[kDISC_RETROVIRIDAE_DNA] = false;
            m_TestList[kDISC_CHECK_AUTH_CAPS] = false;
            m_TestList[kDISC_CHECK_RNA_PRODUCTS_AND_COMMENTS] = false;
            m_TestList[kDISC_MICROSATELLITE_REPEAT_TYPE] = false;
            m_TestList[kDISC_MITOCHONDRION_REQUIRED] = false;
            m_TestList[kDISC_UNPUB_PUB_WITHOUT_TITLE] = false;
            m_TestList[kDISC_INTERNAL_TRANSCRIBED_SPACER_RRNA] = false;
            m_TestList[kDISC_BACTERIA_MISSING_STRAIN] = false;
            m_TestList[kDISC_MISSING_DEFLINES] = false;
            m_TestList[kDISC_MISSING_AFFIL] = false;
            m_TestList[kDISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE] = false;
            m_TestList[kDISC_BACTERIA_SHOULD_NOT_HAVE_MRNA] = false;
            m_TestList[kDISC_CDS_HAS_NEW_EXCEPTION] = false;
            m_TestList[kDISC_TRINOMIAL_SHOULD_HAVE_QUALIFIER] = false;
            m_TestList[kDISC_METAGENOMIC] = false;
            m_TestList[kDISC_METAGENOME_SOURCE] = false;
            m_TestList[kONCALLER_GENE_MISSING] = false;
            m_TestList[kONCALLER_SUPERFLUOUS_GENE] = false;
            m_TestList[kONCALLER_CHECK_AUTHORITY] = false;
            m_TestList[kONCALLER_CONSORTIUM] = false;
            m_TestList[kONCALLER_STRAIN_CULTURE_COLLECTION_MISMATCH] = false;
            m_TestList[kONCALLER_MULTISRC] = false;
            m_TestList[kONCALLER_MULTIPLE_CULTURE_COLLECTION] = false;
            m_TestList[kDISC_STRAIN_TAXNAME_MISMATCH] = false;
            m_TestList[kDISC_HUMAN_HOST] = false;
            m_TestList[kONCALLER_ORDERED_LOCATION] = false;
            m_TestList[kONCALLER_COMMENT_PRESENT] = false;
            m_TestList[kONCALLER_DEFLINE_ON_SET] = false;
            m_TestList[kONCALLER_HIV_RNA_INCONSISTENT] = false;
            m_TestList[kTEST_EXON_ON_MRNA] = false;
            m_TestList[kTEST_HAS_PROJECT_ID] = false;
            m_TestList[kONCALLER_HAS_STANDARD_NAME] = false;
            m_TestList[kONCALLER_MISSING_STRUCTURED_COMMENTS] = false;
            m_TestList[kTEST_ORGANELLE_PRODUCTS] = false;
            m_TestList[kTEST_SP_NOT_UNCULTURED] = false;
            m_TestList[kTEST_BAD_MRNA_QUAL] = false;
            m_TestList[kTEST_UNNECESSARY_ENVIRONMENTAL] = false;
            m_TestList[kTEST_UNNECESSARY_VIRUS_GENE] = false;
            m_TestList[kTEST_UNWANTED_SET_WRAPPER] = false;
            m_TestList[kTEST_MISSING_PRIMER] = false;
            m_TestList[kTEST_AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE] = false;
            m_TestList[kTEST_SMALL_GENOME_SET_PROBLEM] = false;
            m_TestList[kTEST_MRNA_SEQUENCE_MINUS_STRAND_FEATURES] = false;
            m_TestList[kTEST_TAXNAME_NOT_IN_DEFLINE] = false;
            m_TestList[kTEST_COUNT_UNVERIFIED] = false;
            m_TestList[kONCALLER_SWITCH_STRUCTURED_COMMENT_PREFIX] = false;
            m_TestList[kONCALLER_CITSUB_AFFIL_DUP_TEXT] = false;
            m_TestList[kONCALLER_DUPLICATE_PRIMER_SET] = false;
            break;
        case eConfigTypeBigSequences:
            m_TestList[kDISC_PROTEIN_NAMES] = true;
            m_TestList[kDISC_SHORT_CONTIG] = true;
            m_TestList[kDISC_INCONSISTENT_BIOSRC] = true;
            m_TestList[kDISC_SHORT_SEQUENCE] = true;
            m_TestList[kDISC_PERCENTN] = true;
            m_TestList[kDISC_N_RUNS] = true;
            m_TestList[kDISC_ZERO_BASECOUNT] = true;
            m_TestList[kDISC_LONG_NO_ANNOTATION] = true;
            m_TestList[kDISC_NO_ANNOTATION] = true;
            m_TestList[kDISC_COUNT_NUCLEOTIDES] = true;
            m_TestList[kMISSING_GENOMEASSEMBLY_COMMENTS] = true;
            m_TestList[kDISC_GAPS] = true;
            break;
        case eConfigTypeGenomes:
            ITERATE(vector<string>, it, implemented) {
                m_TestList[*it] = true;
            }
            m_TestList[kDISC_STRAIN_TAXNAME_MISMATCH] = false;
            m_TestList[kDISC_CITSUBAFFIL_CONFLICT] = false;
            m_TestList[kDISC_INCONSISTENT_BIOSRC_DEFLINE] = false;
            m_TestList[kDISC_NO_TAXLOOKUP] = false;
            m_TestList[kDISC_BAD_TAXLOOKUP] = false;
            m_TestList[kDISC_COUNT_TRNA] = false;
            m_TestList[kDISC_BADLEN_TRNA] = false;
            m_TestList[kDISC_STRAND_TRNA] = false;
            m_TestList[kDISC_COUNT_RRNA] = false;
            m_TestList[kDISC_CDS_OVERLAP_TRNA] = false;
            m_TestList[kDISC_FEAT_OVERLAP_SRCFEAT] = false;
            m_TestList[kDISC_INFLUENZA_DATE_MISMATCH] = false;
            m_TestList[kDISC_MISSING_VIRAL_QUALS] = false;
            m_TestList[kDISC_SRC_QUAL_PROBLEM] = false;
            m_TestList[kDISC_MISSING_SRC_QUAL] = false;
            m_TestList[kDISC_DUP_SRC_QUAL] = false;
            m_TestList[kDISC_HAPLOTYPE_MISMATCH] = false;
            m_TestList[kDISC_FEATURE_MOLTYPE_MISMATCH] = false;
            m_TestList[kDISC_SPECVOUCHER_TAXNAME_MISMATCH] = false;
            m_TestList[kDISC_FLATFILE_FIND_ONCALLER] = false;
            m_TestList[kDISC_CDS_PRODUCT_FIND] = false;
            m_TestList[kDISC_DUP_DEFLINE] = false;
            m_TestList[kDISC_INCONSISTENT_MOLTYPES] = false;
            m_TestList[kDISC_SUBMITBLOCK_CONFLICT] = false;
            m_TestList[kDISC_POSSIBLE_LINKER] = false;
            m_TestList[kDISC_TITLE_AUTHOR_CONFLICT] = false;
            m_TestList[kDISC_MAP_CHROMOSOME_CONFLICT] = false;
            m_TestList[kDISC_REQUIRED_CLONE] = false;
            m_TestList[kDISC_mRNA_ON_WRONG_SEQUENCE_TYPE] = false;
            m_TestList[kDISC_RETROVIRIDAE_DNA] = false;
            m_TestList[kDISC_MISSING_DEFLINES] = false;
            m_TestList[kONCALLER_GENE_MISSING] = false;
            m_TestList[kONCALLER_CONSORTIUM] = false;
            m_TestList[kDISC_FEATURE_LIST] = false;
            m_TestList[kTEST_ORGANELLE_PRODUCTS] = false;
            m_TestList[kDISC_DUP_TRNA] = false;
            m_TestList[kDISC_DUP_RRNA] = false;
            m_TestList[kDISC_TRANSL_NO_NOTE] = false;
            m_TestList[kDISC_NOTE_NO_TRANSL] = false;
            m_TestList[kDISC_TRANSL_TOO_LONG] = false;
            m_TestList[kDISC_COUNT_PROTEINS] = false;
            m_TestList[kDISC_SRC_QUAL_PROBLEM] = false;
            m_TestList[kDISC_CATEGORY_HEADER] = false;
            m_TestList[kTEST_TAXNAME_NOT_IN_DEFLINE] = false;
            m_TestList[kTEST_SP_NOT_UNCULTURED] = false;
            break;
        case eConfigTypeOnCaller:
            m_TestList[kDISC_RNA_NO_PRODUCT ] = true;
            m_TestList[kUNCULTURED_NOTES_ONCALLER] = true;
            m_TestList[kEND_COLON_IN_COUNTRY] = true;
            m_TestList[kONCALLER_MORE_NAMES_COLLECTED_BY] = true;
            m_TestList[kONCALLER_MORE_OR_SPEC_NAMES_IDENTIFIED_BY] = true;
            m_TestList[kONCALLER_SUSPECTED_ORG_IDENTIFIED] = true;
            m_TestList[kONCALLER_SUSPECTED_ORG_COLLECTED] = true;
            m_TestList[kONCALLER_STRAIN_TAXNAME_CONFLICT] = true;
            m_TestList[kONCALLER_BIOPROJECT_ID] = true;
            m_TestList[kDISC_SUSPECT_PRODUCT_NAME] = true;
            m_TestList[kONCALLER_COUNTRY_COLON] = true;
            m_TestList[kDIVISION_CODE_CONFLICTS] = true;
            m_TestList[kDUP_DISC_CBS_CULTURE_CONFLICT] = true;
            m_TestList[kMULTIPLE_CDS_ON_MRNA] = true;
            m_TestList[kRNA_PROVIRAL] = true;
            m_TestList[kNON_RETROVIRIDAE_PROVIRAL] = true;
            m_TestList[kDISC_CHECK_AUTH_NAME] = true;
            m_TestList[kDISC_CULTURE_TAXNAME_MISMATCH] = true;
            m_TestList[kDISC_BIOMATERIAL_TAXNAME_MISMATCH] = true;
            m_TestList[kTEST_MRNA_OVERLAPPING_PSEUDO_GENE] = true;
            m_TestList[kDISC_BADLEN_TRNA] = true;
            m_TestList[kDISC_MISSING_VIRAL_QUALS] = true;
            m_TestList[kDISC_MISSING_SRC_QUAL] = true;
            m_TestList[kDISC_DUP_SRC_QUAL] = true;
            m_TestList[kDISC_DUP_SRC_QUAL_DATA] = true;
            m_TestList[kDISC_NON_GENE_LOCUS_TAG] = true;
            m_TestList[kDISC_PSEUDO_MISMATCH] = true;
            m_TestList[kDISC_SHORT_INTRON] = true;
            m_TestList[kDISC_INFLUENZA_DATE_MISMATCH] = true;
            m_TestList[kDISC_HAPLOTYPE_MISMATCH] = true;
            m_TestList[kDISC_FEATURE_MOLTYPE_MISMATCH] = true;
            m_TestList[kDISC_CDS_WITHOUT_MRNA] = true;
            m_TestList[kDISC_EXON_INTRON_CONFLICT] = true;
            m_TestList[kDISC_FEATURE_COUNT] = true;
            m_TestList[kDISC_SPECVOUCHER_TAXNAME_MISMATCH] = true;
            m_TestList[kDISC_GENE_PARTIAL_CONFLICT] = true;
            m_TestList[kDISC_FLATFILE_FIND_ONCALLER] = true;
            m_TestList[kDISC_FLATFILE_FIND_ONCALLER_FIXABLE] = true;
            m_TestList[kDISC_FLATFILE_FIND_ONCALLER_UNFIXABLE] = true;
            m_TestList[kDISC_CDS_PRODUCT_FIND] = true;
            m_TestList[kDISC_DUP_DEFLINE] = true;
            m_TestList[kDISC_COUNT_NUCLEOTIDES] = true;
            m_TestList[kDUP_DISC_ATCC_CULTURE_CONFLICT] = true;
            m_TestList[kDISC_USA_STATE] = true;
            m_TestList[kDISC_INCONSISTENT_MOLTYPES] = true;
            m_TestList[kDISC_SRC_QUAL_PROBLEM] = true;
            m_TestList[kDISC_SUBMITBLOCK_CONFLICT] = true;
            m_TestList[kDISC_POSSIBLE_LINKER] = true;
            m_TestList[kDISC_TITLE_AUTHOR_CONFLICT] = true;
            m_TestList[kDISC_BAD_GENE_STRAND] = true;
            m_TestList[kDISC_MAP_CHROMOSOME_CONFLICT] = true;
            m_TestList[kDISC_RBS_WITHOUT_GENE] = true;
            m_TestList[kDISC_CITSUBAFFIL_CONFLICT] = true;
            m_TestList[kDISC_REQUIRED_CLONE] = true;
            m_TestList[kDISC_SUSPICIOUS_NOTE_TEXT] = true;
            m_TestList[kDISC_mRNA_ON_WRONG_SEQUENCE_TYPE] = true;
            m_TestList[kDISC_RETROVIRIDAE_DNA] = true;
            m_TestList[kDISC_CHECK_AUTH_CAPS] = true;
            m_TestList[kDISC_CHECK_RNA_PRODUCTS_AND_COMMENTS] = true;
            m_TestList[kDISC_MICROSATELLITE_REPEAT_TYPE] = true;
            m_TestList[kDISC_MITOCHONDRION_REQUIRED] = true;
            m_TestList[kDISC_UNPUB_PUB_WITHOUT_TITLE] = true;
            m_TestList[kDISC_INTERNAL_TRANSCRIBED_SPACER_RRNA] = true;
            m_TestList[kDISC_BACTERIA_MISSING_STRAIN] = true;
            m_TestList[kDISC_MISSING_DEFLINES] = true;
            m_TestList[kDISC_MISSING_AFFIL] = true;
            m_TestList[kDISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE] = true;
            m_TestList[kDISC_BACTERIA_SHOULD_NOT_HAVE_MRNA] = true;
            m_TestList[kDISC_CDS_HAS_NEW_EXCEPTION] = true;
            m_TestList[kDISC_TRINOMIAL_SHOULD_HAVE_QUALIFIER] = true;
            m_TestList[kDISC_METAGENOMIC] = true;
            m_TestList[kDISC_METAGENOME_SOURCE] = true;
            m_TestList[kONCALLER_GENE_MISSING] = true;
            m_TestList[kONCALLER_SUPERFLUOUS_GENE] = true;
            m_TestList[kONCALLER_CHECK_AUTHORITY] = true;
            m_TestList[kONCALLER_CONSORTIUM] = true;
            m_TestList[kONCALLER_STRAIN_CULTURE_COLLECTION_MISMATCH] = true;
            m_TestList[kONCALLER_MULTISRC] = true;
            m_TestList[kONCALLER_MULTIPLE_CULTURE_COLLECTION] = true;
            m_TestList[kDISC_STRAIN_TAXNAME_MISMATCH] = true;
            m_TestList[kDISC_HUMAN_HOST] = true;
            m_TestList[kONCALLER_ORDERED_LOCATION] = true;
            m_TestList[kONCALLER_COMMENT_PRESENT] = true;
            m_TestList[kONCALLER_DEFLINE_ON_SET] = true;
            m_TestList[kONCALLER_HIV_RNA_INCONSISTENT] = true;
            m_TestList[kTEST_EXON_ON_MRNA] = true;
            m_TestList[kTEST_HAS_PROJECT_ID] = true;
            m_TestList[kONCALLER_HAS_STANDARD_NAME] = true;
            m_TestList[kONCALLER_MISSING_STRUCTURED_COMMENTS] = true;
            m_TestList[kTEST_ORGANELLE_NOT_GENOMIC] = true;
            m_TestList[kTEST_UNWANTED_SPACER] = true;
            m_TestList[kTEST_ORGANELLE_PRODUCTS] = true;
            m_TestList[kTEST_SP_NOT_UNCULTURED] = true;
            m_TestList[kTEST_BAD_MRNA_QUAL] = true;
            m_TestList[kTEST_UNNECESSARY_ENVIRONMENTAL] = true;
            m_TestList[kTEST_UNNECESSARY_VIRUS_GENE] = true;
            m_TestList[kTEST_UNWANTED_SET_WRAPPER] = true;
            m_TestList[kTEST_MISSING_PRIMER] = true;
            m_TestList[kTEST_AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE] = true;
            m_TestList[kTEST_SMALL_GENOME_SET_PROBLEM] = true;
            m_TestList[kTEST_MRNA_SEQUENCE_MINUS_STRAND_FEATURES] = true;
            m_TestList[kTEST_TAXNAME_NOT_IN_DEFLINE] = true;
            m_TestList[kTEST_COUNT_UNVERIFIED] = true;
            m_TestList[kDISC_SHORT_RRNA] = true;
            m_TestList[kONCALLER_SWITCH_STRUCTURED_COMMENT_PREFIX] = true;
            m_TestList[kONCALLER_CITSUB_AFFIL_DUP_TEXT] = true;
            m_TestList[kONCALLER_DUPLICATE_PRIMER_SET] = true;
            m_TestList[kDISC_NO_ANNOTATION] = true;
            m_TestList[kTEST_SHORT_LNCRNA] = true;
            break;
        case eConfigTypeMegaReport:
            ITERATE(vector<string>, it, implemented) {
                m_TestList[*it] = true;
            }
            break;
        case eConfigTypeTSA:
            m_TestList[kSHORT_SEQUENCES_200] = true;
            m_TestList[kDISC_10_PERCENTN] = true;
            m_TestList[kN_RUNS_14] = true;
            m_TestList[kMOLTYPE_NOT_MRNA] = true;
            m_TestList[kTECHNIQUE_NOT_TSA] = true;
            m_TestList[kMISSING_STRUCTURED_COMMENT] = true;
            m_TestList[kMISSING_PROJECT] = true;
            break;
    }
}


void CReportConfig::DisableTRNATests ()
{
    m_TestList[kDISC_COUNT_TRNA] = false;
    m_TestList[kDISC_DUP_TRNA] = false;
    m_TestList[kDISC_BADLEN_TRNA] = false;
    m_TestList[kDISC_COUNT_RRNA] = false;
    m_TestList[kDISC_DUP_RRNA] = false;
    m_TestList[kDISC_TRANSL_NO_NOTE] = false;
    m_TestList[kDISC_NOTE_NO_TRANSL] = false;
    m_TestList[kDISC_TRANSL_TOO_LONG] = false;
    m_TestList[kDISC_CDS_OVERLAP_TRNA] = false;
    m_TestList[kDISC_COUNT_PROTEINS] = false;
}


vector<string> CReportConfig::GetEnabledTestNames()
{
    vector<string> test_list;

    ITERATE(TEnabledTestList, it, m_TestList) {
        if (it->second) {
            test_list.push_back(it->first);
        }
    }
    return test_list;
}


END_NCBI_SCOPE
