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
 *   class for grouping test results for Discrepancy Report
 *
 * $Log:$ 
 */

#include <ncbi_pch.hpp>

#include <misc/discrepancy_report/analysis_process.hpp>
#include <misc/discrepancy_report/group_policy.hpp>

BEGIN_NCBI_SCOPE;

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(DiscRepNmSpc);


typedef map<string, CRef<CReportItem> > TNewItems;

bool CGroupPolicy::Group(TReportItemList& list)
{
    bool any_change = false;

    TNewItems new_items;

    ITERATE(TSubGroupList, it, m_SubGroupList) {
        CRef<CReportItem> item(new CReportItem());
        item->SetSettingName(it->second.front());
        item->SetText(it->first);
        new_items[it->first] = item;
    }

    TReportItemList::iterator it = list.begin();
    while (it != list.end()) {
        TChildParent::iterator c_p = m_ChildParentMap.find((*it)->GetSettingName());
        if (c_p != m_ChildParentMap.end()) {
            new_items[c_p->second]->SetSubitems().push_back(*it);
            it = list.erase(it);
            any_change = true;
        } else {
            it++;
        }
    }

    ITERATE(TNewItems, ni, new_items) {
        if (ni->second->GetSubitems().size() > 0) {
            list.insert(list.begin(), ni->second);
        }
    }

    return any_change;
}


void CGroupPolicy::x_MakeChildParentMap()
{
    ITERATE(TSubGroupList, it, m_SubGroupList) {
        ITERATE(vector<string>, s, it->second) {
            m_ChildParentMap[*s] = it->first;
        }
    }
}


static const string kSourceQualReport = "Source Qualifier Report";

CGUIGroupPolicy::CGUIGroupPolicy()
{
    m_SubGroupList[kSourceQualReport].push_back(kDISC_SRC_QUAL_PROBLEM);
    m_SubGroupList[kSourceQualReport].push_back(kDISC_MISSING_SRC_QUAL);

    x_MakeChildParentMap();
}


static const string kMolecule_type_tests = "Molecule type tests";
static const string kCit_sub_type_tests = "Cit-sub type tests";
static const string kSource_tests = "Source tests";
static const string kFeature_tests = "Feature tests";


bool COncallerGroupPolicy::Group(TReportItemList& list)
{
    // needs to do two levels of grouping

    m_SubGroupList[kSourceQualReport].push_back(kDISC_SRC_QUAL_PROBLEM);
    m_SubGroupList[kSourceQualReport].push_back(kDISC_MISSING_SRC_QUAL);

    x_MakeChildParentMap();
    bool rval = CGroupPolicy::Group(list);

    m_SubGroupList.clear();
    m_ChildParentMap.clear();

    m_SubGroupList[kMolecule_type_tests].push_back(kDISC_FEATURE_MOLTYPE_MISMATCH);
    m_SubGroupList[kMolecule_type_tests].push_back(kTEST_ORGANELLE_NOT_GENOMIC);
    m_SubGroupList[kMolecule_type_tests].push_back(kDISC_INCONSISTENT_MOLTYPES);
    m_SubGroupList[kCit_sub_type_tests].push_back(kDISC_CHECK_AUTH_CAPS);
    m_SubGroupList[kCit_sub_type_tests].push_back(kONCALLER_CONSORTIUM);
    m_SubGroupList[kCit_sub_type_tests].push_back(kDISC_UNPUB_PUB_WITHOUT_TITLE);
    m_SubGroupList[kCit_sub_type_tests].push_back(kDISC_TITLE_AUTHOR_CONFLICT);
    m_SubGroupList[kCit_sub_type_tests].push_back(kDISC_SUBMITBLOCK_CONFLICT);
    m_SubGroupList[kCit_sub_type_tests].push_back(kDISC_CITSUBAFFIL_CONFLICT);
    m_SubGroupList[kCit_sub_type_tests].push_back(kDISC_CHECK_AUTH_NAME);
    m_SubGroupList[kCit_sub_type_tests].push_back(kDISC_MISSING_AFFIL);
    m_SubGroupList[kCit_sub_type_tests].push_back(kDISC_USA_STATE);
    m_SubGroupList[kCit_sub_type_tests].push_back(kONCALLER_CITSUB_AFFIL_DUP_TEXT);
    m_SubGroupList[kSource_tests].push_back(kDISC_SRC_QUAL_PROBLEM);
    m_SubGroupList[kSource_tests].push_back(kDISC_DUP_SRC_QUAL);
    m_SubGroupList[kSource_tests].push_back(kDISC_MISSING_SRC_QUAL);
    m_SubGroupList[kSource_tests].push_back(kDISC_DUP_SRC_QUAL_DATA);
    m_SubGroupList[kSource_tests].push_back(kDISC_MISSING_VIRAL_QUALS);
    m_SubGroupList[kSource_tests].push_back(kDISC_INFLUENZA_DATE_MISMATCH);
    m_SubGroupList[kSource_tests].push_back(kDISC_HUMAN_HOST);
    m_SubGroupList[kSource_tests].push_back(kDISC_SPECVOUCHER_TAXNAME_MISMATCH);
    m_SubGroupList[kSource_tests].push_back(kDISC_BIOMATERIAL_TAXNAME_MISMATCH);
    m_SubGroupList[kSource_tests].push_back(kDISC_CULTURE_TAXNAME_MISMATCH);
    m_SubGroupList[kSource_tests].push_back(kDISC_STRAIN_TAXNAME_MISMATCH);
    m_SubGroupList[kSource_tests].push_back(kDISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE);
    m_SubGroupList[kSource_tests].push_back(kDISC_BACTERIA_MISSING_STRAIN);
    m_SubGroupList[kSource_tests].push_back(kONCALLER_STRAIN_TAXNAME_CONFLICT);
    m_SubGroupList[kSource_tests].push_back(kTEST_SP_NOT_UNCULTURED);
    m_SubGroupList[kSource_tests].push_back(kDISC_REQUIRED_CLONE);
    m_SubGroupList[kSource_tests].push_back(kTEST_UNNECESSARY_ENVIRONMENTAL);
    m_SubGroupList[kSource_tests].push_back(kTEST_AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE);
    m_SubGroupList[kSource_tests].push_back(kONCALLER_MULTISRC);
    m_SubGroupList[kSource_tests].push_back(kONCALLER_COUNTRY_COLON);
    m_SubGroupList[kSource_tests].push_back(kEND_COLON_IN_COUNTRY);
    m_SubGroupList[kSource_tests].push_back(kDUP_DISC_ATCC_CULTURE_CONFLICT);
    m_SubGroupList[kSource_tests].push_back(kDUP_DISC_CBS_CULTURE_CONFLICT);
    m_SubGroupList[kSource_tests].push_back(kONCALLER_STRAIN_CULTURE_COLLECTION_MISMATCH);
    m_SubGroupList[kSource_tests].push_back(kONCALLER_MULTIPLE_CULTURE_COLLECTION);
    m_SubGroupList[kSource_tests].push_back(kDISC_TRINOMIAL_SHOULD_HAVE_QUALIFIER);
    m_SubGroupList[kSource_tests].push_back(kONCALLER_CHECK_AUTHORITY);
    m_SubGroupList[kSource_tests].push_back(kDISC_MAP_CHROMOSOME_CONFLICT);
    m_SubGroupList[kSource_tests].push_back(kDISC_METAGENOMIC);
    m_SubGroupList[kSource_tests].push_back(kDISC_METAGENOME_SOURCE);
    m_SubGroupList[kSource_tests].push_back(kDISC_RETROVIRIDAE_DNA);
    m_SubGroupList[kSource_tests].push_back(kNON_RETROVIRIDAE_PROVIRAL);
    m_SubGroupList[kSource_tests].push_back(kONCALLER_HIV_RNA_INCONSISTENT);
    m_SubGroupList[kSource_tests].push_back(kRNA_PROVIRAL);
    m_SubGroupList[kSource_tests].push_back(kTEST_BAD_MRNA_QUAL);
    m_SubGroupList[kSource_tests].push_back(kDISC_MITOCHONDRION_REQUIRED);
    m_SubGroupList[kSource_tests].push_back(kTEST_UNWANTED_SPACER);
    m_SubGroupList[kSource_tests].push_back(kTEST_SMALL_GENOME_SET_PROBLEM);
    m_SubGroupList[kSource_tests].push_back(kONCALLER_DUPLICATE_PRIMER_SET);
    m_SubGroupList[kSource_tests].push_back(kONCALLER_MORE_NAMES_COLLECTED_BY);
    m_SubGroupList[kSource_tests].push_back(kONCALLER_MORE_OR_SPEC_NAMES_IDENTIFIED_BY);
    m_SubGroupList[kSource_tests].push_back(kONCALLER_SUSPECTED_ORG_IDENTIFIED);
    m_SubGroupList[kSource_tests].push_back(kONCALLER_SUSPECTED_ORG_COLLECTED);
    m_SubGroupList[kFeature_tests].push_back(kONCALLER_SUPERFLUOUS_GENE);
    m_SubGroupList[kFeature_tests].push_back(kONCALLER_GENE_MISSING);
    m_SubGroupList[kFeature_tests].push_back(kDISC_GENE_PARTIAL_CONFLICT);
    m_SubGroupList[kFeature_tests].push_back(kDISC_BAD_GENE_STRAND);
    m_SubGroupList[kFeature_tests].push_back(kTEST_UNNECESSARY_VIRUS_GENE);
    m_SubGroupList[kFeature_tests].push_back(kDISC_NON_GENE_LOCUS_TAG);
    m_SubGroupList[kFeature_tests].push_back(kDISC_RBS_WITHOUT_GENE);
    m_SubGroupList[kFeature_tests].push_back(kONCALLER_ORDERED_LOCATION);
    m_SubGroupList[kFeature_tests].push_back(kMULTIPLE_CDS_ON_MRNA);
    m_SubGroupList[kFeature_tests].push_back(kDISC_CDS_WITHOUT_MRNA);
    m_SubGroupList[kFeature_tests].push_back(kDISC_mRNA_ON_WRONG_SEQUENCE_TYPE);
    m_SubGroupList[kFeature_tests].push_back(kDISC_BACTERIA_SHOULD_NOT_HAVE_MRNA);
    m_SubGroupList[kFeature_tests].push_back(kTEST_MRNA_OVERLAPPING_PSEUDO_GENE);
    m_SubGroupList[kFeature_tests].push_back(kTEST_EXON_ON_MRNA);
    m_SubGroupList[kFeature_tests].push_back(kDISC_CDS_HAS_NEW_EXCEPTION);
    m_SubGroupList[kFeature_tests].push_back(kDISC_SHORT_INTRON);
    m_SubGroupList[kFeature_tests].push_back(kDISC_EXON_INTRON_CONFLICT);
    m_SubGroupList[kFeature_tests].push_back(kDISC_PSEUDO_MISMATCH);
    m_SubGroupList[kFeature_tests].push_back(kDISC_RNA_NO_PRODUCT);
    m_SubGroupList[kFeature_tests].push_back(kDISC_BADLEN_TRNA);
    m_SubGroupList[kFeature_tests].push_back(kDISC_MICROSATELLITE_REPEAT_TYPE);
    m_SubGroupList[kFeature_tests].push_back(kDISC_SHORT_RRNA);

    x_MakeChildParentMap();
    rval |= CGroupPolicy::Group(list);

    return rval;
}


END_NCBI_SCOPE
