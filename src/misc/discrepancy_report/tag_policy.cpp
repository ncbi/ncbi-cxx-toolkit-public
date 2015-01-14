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
 *   class for storing tests for Discrepancy Report
 *
 * $Log:$ 
 */

#include <ncbi_pch.hpp>

#include <misc/discrepancy_report/tag_policy.hpp>

BEGIN_NCBI_SCOPE;

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(DiscRepNmSpc);

typedef struct {
  string setting_name;
  string description;
  string nofix_description;
} SOutputFlag;


SOutputFlag wgs_fatal[] = {
        {"BAD_LOCUS_TAG_FORMAT", "", ""},
        {"CONTAINED_CDS", "", "coding regions are completely contained in another coding region but have note"},
        {"DISC_BACTERIAL_PARTIAL_NONEXTENDABLE_PROBLEMS", "", ""},
        {"DISC_BACTERIA_SHOULD_NOT_HAVE_MRNA", "", ""},
        {"DISC_BAD_BGPIPE_QUALS", "", ""},
        {"DISC_CITSUBAFFIL_CONFLICT", "", "No citsubs were found!"},
        {"DISC_INCONSISTENT_MOLTYPES", "", "Moltypes are consistent"},
        {"DISC_MAP_CHROMOSOME_CONFLICT", "", ""},
        {"DISC_MICROSATELLITE_REPEAT_TYPE", "", ""},
        {"DISC_MISSING_AFFIL", "", ""},
        {"DISC_NONWGS_SETS_PRESENT", "", ""},
        {"DISC_QUALITY_SCORES", "Quality scores are missing on some sequences.", ""},
        {"DISC_RBS_WITHOUT_GENE", "", ""},
        {"DISC_SHORT_RRNA", "", ""},
        {"DISC_SEGSETS_PRESENT", "", ""},
        {"DISC_SOURCE_QUALS_ASNDISC", "collection-date", ""},
        {"DISC_SOURCE_QUALS_ASNDISC", "country", ""},
        {"DISC_SOURCE_QUALS_ASNDISC", "isolation-source", ""},
        {"DISC_SOURCE_QUALS_ASNDISC", "host", ""},
        {"DISC_SOURCE_QUALS_ASNDISC", "strain", ""},
        {"DISC_SOURCE_QUALS_ASNDISC", "taxname", ""},
        {"DISC_SOURCE_QUALS_ASNDISC", "taxname (all present, all unique)", ""},
        {"DISC_SUBMITBLOCK_CONFLICT", "", ""},
        {"DISC_SUSPECT_RRNA_PRODUCTS", "", ""},
        {"DISC_TITLE_AUTHOR_CONFLICT", "", ""},
        {"DISC_UNPUB_PUB_WITHOUT_TITLE", "", ""},
        {"EC_NUMBER_ON_UNKNOWN_PROTEIN", "", ""},
        {"EUKARYOTE_SHOULD_HAVE_MRNA", "no mRNA present", ""},
        {"INCONSISTENT_LOCUS_TAG_PREFIX", "", ""},
        {"INCONSISTENT_PROTEIN_ID", "", ""},
        {"MISSING_GENES", "", ""},
        {"MISSING_LOCUS_TAGS", "", ""},
        {"MISSING_PROTEIN_ID", "", ""},
        {"ONCALLER_ORDERED_LOCATION", "", ""},
        {"PARTIAL_CDS_COMPLETE_SEQUENCE", "", ""},
        {"PSEUDO_MISMATCH", "", ""},
        {"RNA_CDS_OVERLAP", "coding regions are completely contained in RNAs", ""},
        {"RNA_CDS_OVERLAP", "coding regions completely contain RNAs", ""},
        {"RNA_NO_PRODUCT", "", ""},
        {"SHOW_HYPOTHETICAL_CDS_HAVING_GENE_NAME", "", ""},
        {"SUSPECT_PRODUCT_NAMES", "Remove organism from product name", ""},
        {"SUSPECT_PRODUCT_NAMES", "Possible parsing error or incorrect formatting; remove inappropriate symbols", ""},
        {"TEST_OVERLAPPING_RRNAS", "", ""},
        {"TEST_TERMINAL_NS", "", ""},
        {"DISC_RNA_CDS_OVERLAP", "completely contain tRNAs", ""}
};


bool CWGSTagPolicy::AddTag(CReportItem& item)
{
    bool add_tag = false;

    NON_CONST_ITERATE(TReportItemList, it, item.SetSubitems()) {
        add_tag |= AddTag(**it);
    }

    if (!add_tag) {
        size_t num_wgs_fatal = sizeof (wgs_fatal) / sizeof (SOutputFlag);
        for (size_t i = 0; i < num_wgs_fatal && !add_tag; i++) {
            if (NStr::EqualNocase(item.GetSettingName(), wgs_fatal[i].setting_name)
                && (NStr::IsBlank(wgs_fatal[i].description) || NStr::FindNoCase(item.GetText(), wgs_fatal[i].description) != string::npos)
                && (NStr::IsBlank(wgs_fatal[i].nofix_description) || NStr::FindNoCase(item.GetText(), wgs_fatal[i].nofix_description) == string::npos)) {
                add_tag = true;        
            }
        }
        // handle special exceptions
        if (add_tag) {
            if (NStr::EqualNocase(item.GetSettingName(), "DISC_SOURCE_QUALS_ASNDISC")
                && (!m_MoreThanOneNuc || NStr::FindNoCase(item.GetText(), "taxname (all present, all unique)") == string::npos)
                && NStr::FindNoCase(item.GetText(), "some missing") == string::npos
                && NStr::FindNoCase(item.GetText(), "some duplicate") == string::npos) {
                add_tag = false;
            }
        }
    }

    item.SetNeedsTag(add_tag);
    
    return add_tag;
}




END_NCBI_SCOPE
