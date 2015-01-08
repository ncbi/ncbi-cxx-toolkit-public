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
 *   class for ordering test results for Discrepancy Report
 *
 * $Log:$ 
 */

#include <ncbi_pch.hpp>

#include <misc/discrepancy_report/order_policy.hpp>
#include <misc/discrepancy_report/analysis_process.hpp>

BEGIN_NCBI_SCOPE;

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(DiscRepNmSpc);


// note: I don't want to do a straight up sort, because I want to
// preserve the order of the results with the same setting name
void COrderPolicy::Order(TReportItemList& list)
{
    TReportItemList sorted;

    vector<string> implemented = CAnalysisProcessFactory::Implemented();

    ITERATE(vector<string>, test_name, implemented) {
        TReportItemList::iterator li = list.begin();
        while (li != list.end()) {
            if (NStr::EqualNocase((*li)->GetSettingName(), *test_name)) {
                sorted.push_back(*li);
                li = list.erase(li);
            } else {
                li++;
            }
        }
    }    
    list.insert(list.begin(), sorted.begin(), sorted.end());
}


static const string sOncallerOrder[] = {
    kDISC_COUNT_NUCLEOTIDES,
    kDISC_DUP_DEFLINE,
    kDISC_MISSING_DEFLINES,
    kTEST_TAXNAME_NOT_IN_DEFLINE,
    kTEST_HAS_PROJECT_ID,
    kONCALLER_BIOPROJECT_ID,
    kONCALLER_DEFLINE_ON_SET,
    kTEST_UNWANTED_SET_WRAPPER,
    kTEST_COUNT_UNVERIFIED,
    kDISC_SRC_QUAL_PROBLEM,
    kDISC_FEATURE_COUNT,
    kDISC_NO_ANNOTATION,
    kDISC_FEATURE_MOLTYPE_MISMATCH,
    kTEST_ORGANELLE_NOT_GENOMIC,
    kDISC_INCONSISTENT_MOLTYPES,
    kDISC_CHECK_AUTH_CAPS,
    kONCALLER_CONSORTIUM,
    kDISC_UNPUB_PUB_WITHOUT_TITLE,
    kDISC_TITLE_AUTHOR_CONFLICT,
    kDISC_SUBMITBLOCK_CONFLICT,
    kDISC_CITSUBAFFIL_CONFLICT,
    kDISC_CHECK_AUTH_NAME,
    kDISC_MISSING_AFFIL,
    kDISC_USA_STATE,
    kONCALLER_CITSUB_AFFIL_DUP_TEXT,
    kDISC_DUP_SRC_QUAL,
    kDISC_MISSING_SRC_QUAL,
    kDISC_DUP_SRC_QUAL_DATA,
    kONCALLER_DUPLICATE_PRIMER_SET,
    kONCALLER_MORE_NAMES_COLLECTED_BY,
    kONCALLER_MORE_OR_SPEC_NAMES_IDENTIFIED_BY,
    kONCALLER_SUSPECTED_ORG_IDENTIFIED,
    kONCALLER_SUSPECTED_ORG_COLLECTED,
    kDISC_MISSING_VIRAL_QUALS,
    kDISC_INFLUENZA_DATE_MISMATCH,
    kDISC_HUMAN_HOST,
    kDISC_SPECVOUCHER_TAXNAME_MISMATCH,
    kDISC_BIOMATERIAL_TAXNAME_MISMATCH,
    kDISC_CULTURE_TAXNAME_MISMATCH,
    kDISC_STRAIN_TAXNAME_MISMATCH,
    kDISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE,
    kDISC_BACTERIA_MISSING_STRAIN,
    kONCALLER_STRAIN_TAXNAME_CONFLICT,
    kTEST_SP_NOT_UNCULTURED,
    kDISC_REQUIRED_CLONE,
    kTEST_UNNECESSARY_ENVIRONMENTAL,
    kTEST_AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE,
    kONCALLER_MULTISRC,
    kONCALLER_COUNTRY_COLON,
    kEND_COLON_IN_COUNTRY,
    kDUP_DISC_ATCC_CULTURE_CONFLICT,
    kDUP_DISC_CBS_CULTURE_CONFLICT,
    kONCALLER_STRAIN_CULTURE_COLLECTION_MISMATCH,
    kONCALLER_MULTIPLE_CULTURE_COLLECTION,
    kDISC_TRINOMIAL_SHOULD_HAVE_QUALIFIER,
    kONCALLER_CHECK_AUTHORITY,
    kDISC_MAP_CHROMOSOME_CONFLICT,
    kDISC_METAGENOMIC,
    kDISC_METAGENOME_SOURCE,
    kDISC_RETROVIRIDAE_DNA,
    kNON_RETROVIRIDAE_PROVIRAL,
    kONCALLER_HIV_RNA_INCONSISTENT,
    kRNA_PROVIRAL,
    kTEST_BAD_MRNA_QUAL,
    kDISC_MITOCHONDRION_REQUIRED,
    kTEST_UNWANTED_SPACER,
    kTEST_SMALL_GENOME_SET_PROBLEM,
    kONCALLER_SUPERFLUOUS_GENE,
    kONCALLER_GENE_MISSING,
    kDISC_GENE_PARTIAL_CONFLICT,
    kDISC_BAD_GENE_STRAND,
    kTEST_UNNECESSARY_VIRUS_GENE,
    kDISC_NON_GENE_LOCUS_TAG,
    kDISC_RBS_WITHOUT_GENE,
    kONCALLER_ORDERED_LOCATION,
    kMULTIPLE_CDS_ON_MRNA,
    kDISC_CDS_WITHOUT_MRNA,
    kDISC_mRNA_ON_WRONG_SEQUENCE_TYPE,
    kDISC_BACTERIA_SHOULD_NOT_HAVE_MRNA,
    kTEST_MRNA_OVERLAPPING_PSEUDO_GENE,
    kTEST_EXON_ON_MRNA,
    kDISC_CDS_HAS_NEW_EXCEPTION,
    kDISC_SHORT_INTRON,
    kDISC_EXON_INTRON_CONFLICT,
    kDISC_PSEUDO_MISMATCH,
    kDISC_RNA_NO_PRODUCT,
    kDISC_BADLEN_TRNA,
    kDISC_MICROSATELLITE_REPEAT_TYPE,
    kDISC_SHORT_RRNA,
    kDISC_POSSIBLE_LINKER,
    kDISC_HAPLOTYPE_MISMATCH,
    kDISC_FLATFILE_FIND_ONCALLER,
    kDISC_CDS_PRODUCT_FIND,
    kDISC_SUSPICIOUS_NOTE_TEXT,
    kDISC_CHECK_RNA_PRODUCTS_AND_COMMENTS,
    kDISC_INTERNAL_TRANSCRIBED_SPACER_RRNA,
    kONCALLER_COMMENT_PRESENT,
    kDISC_SUSPECT_PRODUCT_NAME
};


void COncallerOrderPolicy::Order(TReportItemList& list)
{
    COrderPolicy::Order(list);

    TReportItemList sorted;
    size_t num_oncaller = sizeof (sOncallerOrder) / sizeof (string);
    for (size_t i = 0; i < num_oncaller; i++) {
        TReportItemList::iterator li = list.begin();
        while (li != list.end()) {
            if (NStr::EqualNocase((*li)->GetSettingName(), sOncallerOrder[i])) {
                sorted.push_back(*li);
                li = list.erase(li);
            } else {
                li++;
            }
        }
    }    
    sorted.insert(sorted.end(), list.begin(), list.end());
    list = sorted;
}


static const string sCommandLineOrder[] = {
    kDISC_MISSING_PROTEIN_ID,
    kDISC_SOURCE_QUALS_ASNDISC,
    kDISC_FEATURE_COUNT,
    kDISC_COUNT_NUCLEOTIDES,
    kMISSING_GENES
};


void CCommandLineOrderPolicy::Order(TReportItemList& list)
{
    COrderPolicy::Order(list);

    TReportItemList sorted;
    size_t num_commandline = sizeof (sCommandLineOrder) / sizeof (string);
    for (size_t i = 0; i < num_commandline; i++) {
        TReportItemList::iterator li = list.begin();
        while (li != list.end()) {
            if (NStr::EqualNocase((*li)->GetSettingName(), sCommandLineOrder[i])) {
                sorted.push_back(*li);
                li = list.erase(li);
            } else {
                li++;
            }
        }
    }    
    sorted.insert(sorted.end(), list.begin(), list.end());
    list = sorted;
}


END_NCBI_SCOPE
