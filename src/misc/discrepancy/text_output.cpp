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
 * Authors: please dont mention my name here
 *
 */

#include <ncbi_pch.hpp>
#include "discrepancy_core.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

static constexpr std::initializer_list<eTestNames> g_ReportOrder0 = {
        eTestNames::COUNT_NUCLEOTIDES,
        eTestNames::VERY_LONG_NO_ANNOTATION,
        eTestNames::LONG_NO_ANNOTATION,
        eTestNames::NO_ANNOTATION,
        };

static constexpr std::initializer_list<eTestNames> g_ReportOrder1 = {
        eTestNames::SOURCE_QUALS,
        eTestNames::DEPRECATED,
        eTestNames::DUP_SRC_QUAL,
        eTestNames::MAP_CHROMOSOME_CONFLICT,
        eTestNames::BIOMATERIAL_TAXNAME_MISMATCH,
        eTestNames::SPECVOUCHER_TAXNAME_MISMATCH,
        eTestNames::STRAIN_CULTURE_COLLECTION_MISMATCH,
        eTestNames::TRINOMIAL_SHOULD_HAVE_QUALIFIER,
        eTestNames::REQUIRED_STRAIN,
        eTestNames::METAGENOMIC,
        eTestNames::METAGENOME_SOURCE,
        eTestNames::MAG_SHOULD_NOT_HAVE_STRAIN,
        eTestNames::MAG_MISSING_ISOLATE,

        eTestNames::TITLE_ENDS_WITH_SEQUENCE,
        eTestNames::GAPS,
        eTestNames::N_RUNS,
        eTestNames::PERCENT_N,
        eTestNames::TEN_PERCENTN,
        eTestNames::TERMINAL_NS,
        eTestNames::ZERO_BASECOUNT,
        eTestNames::LOW_QUALITY_REGION,
        eTestNames::UNUSUAL_NT,
        //eTestNames::SHORT_CONTIG,
        //eTestNames::SHORT_SEQUENCES,
        //eTestNames::SEQUENCES_ARE_SHORT,
        eTestNames::GENOMIC_MRNA,

        eTestNames::CHECK_AUTH_CAPS,
        eTestNames::CHECK_AUTH_NAME,
        eTestNames::TITLE_AUTHOR_CONFLICT,
        eTestNames::CITSUBAFFIL_CONFLICT,
        eTestNames::SUBMITBLOCK_CONFLICT,
        eTestNames::UNPUB_PUB_WITHOUT_TITLE,
        eTestNames::USA_STATE,

        eTestNames::FEATURE_COUNT,
        eTestNames::PROTEIN_NAMES,
        eTestNames::SUSPECT_PRODUCT_NAMES,
        eTestNames::SUSPECT_PHRASES,
        eTestNames::INCONSISTENT_PROTEIN_ID,
        eTestNames::MISSING_PROTEIN_ID,
        eTestNames::MRNA_SHOULD_HAVE_PROTEIN_TRANSCRIPT_IDS,
        eTestNames::BAD_LOCUS_TAG_FORMAT,
        eTestNames::INCONSISTENT_LOCUS_TAG_PREFIX,
        eTestNames::DUPLICATE_LOCUS_TAGS,
        eTestNames::MISSING_LOCUS_TAGS,
        eTestNames::NON_GENE_LOCUS_TAG,
        eTestNames::MISSING_GENES,
        eTestNames::EXTRA_GENES,
        eTestNames::BAD_BACTERIAL_GENE_NAME,
        eTestNames::BAD_GENE_NAME,
        eTestNames::BAD_GENE_STRAND,
        eTestNames::DUP_GENES_OPPOSITE_STRANDS,
        eTestNames::GENE_PARTIAL_CONFLICT,
        eTestNames::GENE_PRODUCT_CONFLICT,
        eTestNames::SHOW_HYPOTHETICAL_CDS_HAVING_GENE_NAME,
        eTestNames::EC_NUMBER_ON_UNKNOWN_PROTEIN,
        eTestNames::MISC_FEATURE_WITH_PRODUCT_QUAL,
        eTestNames::PARTIAL_CDS_COMPLETE_SEQUENCE,
        eTestNames::CONTAINED_CDS,
        eTestNames::RNA_CDS_OVERLAP,
        eTestNames::CDS_TRNA_OVERLAP,
        eTestNames::OVERLAPPING_RRNAS,
        eTestNames::FIND_OVERLAPPED_GENES,
        eTestNames::ORDERED_LOCATION,
        eTestNames::PARTIAL_PROBLEMS,
        eTestNames::FEATURE_LOCATION_CONFLICT,
        eTestNames::PSEUDO_MISMATCH,
        eTestNames::EUKARYOTE_SHOULD_HAVE_MRNA,
        eTestNames::MULTIPLE_CDS_ON_MRNA,
        eTestNames::CDS_WITHOUT_MRNA,
        eTestNames::BACTERIA_SHOULD_NOT_HAVE_MRNA,
        eTestNames::BACTERIAL_PARTIAL_NONEXTENDABLE_EXCEPTION,
        eTestNames::BACTERIAL_PARTIAL_NONEXTENDABLE_PROBLEMS,
        eTestNames::BACTERIAL_JOINED_FEATURES_NO_EXCEPTION,
        eTestNames::JOINED_FEATURES,
        eTestNames::RIBOSOMAL_SLIPPAGE,
        eTestNames::BAD_BGPIPE_QUALS,
        eTestNames::CDS_HAS_NEW_EXCEPTION,
        eTestNames::SHOW_TRANSL_EXCEPT,
        eTestNames::RNA_NO_PRODUCT,
        eTestNames::RRNA_NAME_CONFLICTS,
        eTestNames::SUSPECT_RRNA_PRODUCTS,
        eTestNames::SHORT_RRNA,
        eTestNames::FIND_BADLEN_TRNAS,
        eTestNames::UNUSUAL_MISC_RNA,
        eTestNames::SHORT_LNCRNA,
        eTestNames::SHORT_INTRON,
        eTestNames::EXON_INTRON_CONFLICT,
        eTestNames::EXON_ON_MRNA,
        eTestNames::SHORT_PROT_SEQUENCES,

        eTestNames::INCONSISTENT_DBLINK,
        eTestNames::INCONSISTENT_MOLINFO_TECH,
        eTestNames::INCONSISTENT_MOLTYPES,
        eTestNames::INCONSISTENT_STRUCTURED_COMMENTS,
        eTestNames::QUALITY_SCORES,
        eTestNames::SEGSETS_PRESENT,
        };

//static_assert(g_ReportOrder0.size()+g_ReportOrder1.size() == TTestNamesSet::capacity(), "Not all of the tests included in the reporting groups");


static bool ShowFatal(const CReportItem& item)
{
    if (!item.IsFatal()) {
        return false;
    }
    TReportItemList subs = item.GetSubitems();
    for (const auto& it : subs) {
        if (it->IsSummary() && it->IsFatal()) {
            return false;
        }
    }
    return true;
}


static inline string_view s_RemoveInitialUnderscore(string_view s)
{
    return s[0] == '_' ? s.substr(1) : s;
}


static void RecursiveText(ostream& out, const TReportItemList& list, unsigned short flags)
{
    bool ext = (flags & CDiscrepancySet::eOutput_Ext) != 0;
    bool fatal = (flags & CDiscrepancySet::eOutput_Fatal) != 0;
    for (const auto& it : list) {
        if (it->IsExtended() && !ext) {
            continue;
        }
        if (fatal && ShowFatal(*it)) {
            out << "FATAL: ";
        }
        auto title = s_RemoveInitialUnderscore(it->GetTitle());
        auto IsDupDefline = (title == string_view("DUP_DEFLINE"));
        out << title << ": " << it->GetMsg() << '\n';
        TReportItemList subs = it->GetSubitems();
        if (!subs.empty() && (ext || !subs[0]->IsExtended())) {
            RecursiveText(out, subs, flags);
        }
        else {
            TReportObjectList det = it->GetDetails();
            for (const auto& obj : det) {
                if (flags & CDiscrepancySet::eOutput_Files) {
                    out << obj->GetPath() << ":";
                }
                if (obj->IsFixed()) {
                    out << "[FIXED] ";
                }

                auto text = (IsDupDefline && obj->GetType() == CReportObj::eType_sequence) ?
                    obj->GetBioseqLabel() :
                    obj->GetText();
                out << text << '\n';
            }
        }
    }
}


static void RecursiveSummary(ostream& out, const TReportItemList& list, unsigned short flags, size_t level = 0)
{
    bool fatal = (flags & CDiscrepancySet::eOutput_Fatal) != 0;
    for (const auto& it : list) {
        auto title = it->GetTitle();
        auto msg = it->GetMsg();
        bool includeInSummary = (level == 0 )
            ||  (title == string_view("SOURCE_QUALS")  &&  level == 1)
            ||  (title == string_view("DEPRECATED")  &&  level == 1);
        if (includeInSummary) {
            if (fatal && ShowFatal(*it)) {
                out << "FATAL: ";
            }
            out << s_RemoveInitialUnderscore(title) << ": " << msg << '\n';
        }
        else if (it->IsSummary()) {
            out << string(level, '\t');
            if (fatal && ShowFatal(*it)) {
                out << "FATAL: ";
            }
            out << msg << '\n';
        }
        else {
            continue;
        }
        RecursiveSummary(out, it->GetSubitems(), flags, level + 1);
    }
}


static bool RecursiveFatalSummary(ostream& out, const TReportItemList& list, size_t level = 0)
{
    bool found = false;
    for (const auto& it : list) {
        if (it->IsFatal() && it->GetTitle() != string_view("SOURCE_QUALS")
            && it->GetTitle() != string_view("DEPRECATED")
            && it->GetTitle() != string_view("SUSPECT_PRODUCT_NAMES")) {
            found = true;
            if (level == 0) {
                out << "FATAL: ";
                out << s_RemoveInitialUnderscore(it->GetTitle()) << ": " << it->GetMsg() << '\n';
            }
            else if (it->IsSummary()) {
                out << string(level, '\t');
                out << "FATAL: ";
                out << it->GetMsg() << '\n';
            }
            else {
                continue;
            }
            RecursiveFatalSummary(out, it->GetSubitems(), level + 1);
        }
    }
    return found;
}

static TReportItemList x_CollectGroup(const std::initializer_list<eTestNames>& m_List, TDiscrepancyCoreMap& tests, bool all)
{
    TReportItemList out;
    for (const auto& it : m_List) {
        if (tests.find(it) != tests.end()) {
            TReportItemList tmp = tests[it]->GetReport();
            for (const auto& tt : tmp) {
                out.push_back(tt);
            }
            tests.erase(it);
        }
    }
    if (all) {
        for (const auto& it : tests) {
            TReportItemList list = it.second->GetReport();
            for (const auto& it2 : list) {
                out.push_back(it2);
            }
        }
    }
    #if 0
    for (const auto& it : m_List) {
        TReportItemList tmp = it.Collect(tests, false);
        for (const auto& tt : tmp) {
            out.push_back(tt);
        }
    }
    if (tests.find(m_Test) != tests.end()) {
        TReportItemList tmp = tests[m_Test]->GetReport();
        for (const auto& tt : tmp) {
            out.push_back(tt);
        }
        tests.erase(m_Test);
    }
    if (!m_Label.empty()) {
        TReportObjectList objs;
        TReportObjectSet hash;
        _ASSERT(0);
        #if 0
        CRef<CDiscrepancyItem> di(new CDiscrepancyItem(m_Label));
        di->m_Subs = out;
        bool empty = true;
        for (const auto& tt : out) {
            TReportObjectList details = tt->GetDetails();
            if (!details.empty() || tt->GetCount() > 0) {
                empty = false;
            }
            for (auto& ob : details) {
                CReportNode::Add(objs, hash, *ob);
            }
            if (tt->CanAutofix()) {
                di->m_Autofix = true;
            }
            if (tt->IsInfo()) {
                di->m_Severity = CDiscrepancyItem::eSeverity_info;
            }
            else if (tt->IsFatal()) {
                di->m_Severity = CDiscrepancyItem::eSeverity_error;
            }
        }
        di->m_Objs = objs;
        out.clear();
        if (!empty) {
            out.push_back(CRef<CReportItem>(di));
        }
        #endif
    }
    if (all) {
        for (const auto& it : tests) {
            TReportItemList list = it.second->GetReport();
            for (const auto& it : list) {
                out.push_back(it);
            }
        }
    }
    #endif

    return out;
}

void CDiscrepancyProductImpl::OutputText(ostream& out, unsigned short flags, char group)
{
    switch (group) {
        case 'b':
            out << "Discrepancy Report Results (due to the large size of the file some checks may not have run)\n\n";
            break;
        case 'q':
            out << "Discrepancy Report Results (SMART set of checks)\n\n";
            break;
        case 'u':
            out << "Discrepancy Report Results (submitter set of checks)\n\n";
            break;
        default:
            out << "Discrepancy Report Results\n\n";
    }

    out << "Summary\n";
    TReportItemList m_Group0;
    TReportItemList m_Group1;

    m_Group0 = x_CollectGroup(g_ReportOrder0, m_Tests, false);
    m_Group1 = x_CollectGroup(g_ReportOrder1, m_Tests, true);
    #ifdef _DEBUG111
    std::cerr << g_ReportOrder0.size() << ":" << g_ReportOrder1.size() << ":" << TTestNamesSet::capacity() << "\n";
    std::cerr << m_Group0.size() << ":" << m_Group1.size() << "\n";
    #endif


    RecursiveSummary(out, m_Group0, flags);
    if (flags & CDiscrepancySet::eOutput_Fatal) {
        RecursiveFatalSummary(out, m_Group1, flags);
    }
    RecursiveSummary(out, m_Group1, flags);

    if (flags & CDiscrepancySet::eOutput_Summary) return;

    out << "\nDetailed Report\n\n";
    RecursiveText(out, m_Group0, flags);
    RecursiveText(out, m_Group1, flags);
}

static void Indent(ostream& out, size_t indent)
{
    static const size_t XML_INDENT = 2;
    out << string(indent * XML_INDENT, ' ');
}

static string SevLevel[CReportItem::eSeverity_error + 1] = { "INFO", "WARNING", "FATAL" };

static void RecursiveXML(ostream& out, const TReportItemList& list, unsigned short flags, size_t indent)
{
    bool ext = (flags & CDiscrepancySet::eOutput_Ext) != 0;
    for (const auto& it : list) {
        if (it->IsExtended() && !ext) {
            continue;
        }
        Indent(out, indent);
        out << "<details message=\"" << NStr::XmlEncode(it->GetXml()) << "\"";
        out << " severity=\"" << SevLevel[it->GetSeverity()] << "\"";
        if (it->GetCount() > 0) {
            out << " cardinality=\"" << NStr::Int8ToString(it->GetCount()) << "\"";
        }
        if (!it->GetUnit().empty()) {
            out << " unit=\"" << NStr::XmlEncode(it->GetUnit()) << "\"";
        }
        if (it->CanAutofix()) {
            out << " autofix=\"true\"";
        }
        out << ">\n";

        ++indent;
        TReportItemList subs = it->GetSubitems();
        if (!subs.empty() && (ext || !subs[0]->IsExtended())) {
            RecursiveXML(out, subs, flags, indent);
        }
        else {
            for (const auto& obj : it->GetDetails()) {
                Indent(out, indent);
                out << "<object type=";
                switch (obj->GetType()) {
                    case CReportObj::eType_feature:
                        out << "\"feature\"";
                        break;
                    case CReportObj::eType_descriptor:
                        out << "\"descriptor\"";
                        break;
                    case CReportObj::eType_sequence:
                        out << "\"sequence\"";
                        break;
                    case CReportObj::eType_seq_set:
                        out << "\"set\"";
                        break;
                    case CReportObj::eType_submit_block:
                        out << "\"submit_block\"";
                        break;
                    case CReportObj::eType_string:
                        out << "\"string\"";
                        break;
                    default:
                        out << "\"\"";
                        break;
                }
                if (flags & CDiscrepancySet::eOutput_Files) {
                    out << " file=\"" << NStr::XmlEncode(obj->GetPath()) << "\"";
                }
                const string sFeatureType = obj->GetFeatureType();
                if (!sFeatureType.empty()) {
                    out << " feature_type=\"" << NStr::XmlEncode(sFeatureType) << "\"";
                }
                const string sProductName = obj->GetProductName();
                if (!sProductName.empty()) {
                    out << (sFeatureType == "Gene" ? " symbol=\"" : " product=\"") << NStr::XmlEncode(sProductName) << "\"";
                }
                const string sLocation = obj->GetLocation();
                if (!sLocation.empty()) {
                    out << " location=\"" << NStr::XmlEncode(sLocation) << "\"";
                }
                const string sLocusTag = obj->GetLocusTag();
                if (!sLocusTag.empty()) {
                    out << " locus_tag=\"" << NStr::XmlEncode(sLocusTag) << "\"";
                }
                const string text = obj->GetText();
                out << " label=\"" << NStr::XmlEncode(text) << "\" />\n";
            }
        }
        --indent;
        Indent(out, indent);
        out << "</details>\n";
    }
}

static list<CRef<CDiscrepancyCore>> x_ReorderList(const std::vector<eTestNames>& order, const TDiscrepancyCoreMap& tests)
{
    vector<std::pair<TDiscrepancyCoreMap::key_type, TDiscrepancyCoreMap::mapped_type>> vec;
    vec.reserve(tests.size());
    for (const auto& test : tests) {
        vec.push_back(test);
    }

    sort(vec.begin(), vec.end(), [&order](auto& l, auto r)
    {
        auto it_l = std::find(order.begin(), order.end(), l.first);
        auto it_r = std::find(order.begin(), order.end(), r.first);
        // in case the test is not put into ordering list, compare by test value
        if (it_l == it_r)
            return r.first < l.first;
        return it_l < it_r;
    });

    list<CRef<CDiscrepancyCore>> result;

    for (const auto& test : vec) {
        result.push_back(test.second);
    }

    return result;
}

void CDiscrepancyProductImpl::OutputXML(ostream& out, unsigned short flags)
{
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<discrepancy_report>\n";

    auto sorted = x_ReorderList(g_ReportOrder1, m_Tests);
    for (const auto& test : sorted) {
        TReportItemList rep = test->GetReport();
        if (rep.empty()) {
            continue;
        }
        CReportItem::ESeverity max_sev = CReportItem::eSeverity_info;
        for (const auto& it : rep) {
            CReportItem::ESeverity s = it->GetSeverity();
            if (max_sev < s) {
                max_sev = s;
            }
        }
        Indent(out, 1);
        out << "<test name=\"" << s_RemoveInitialUnderscore(test->GetSName())
            << "\" description=\"" << NStr::XmlEncode(test->GetDescription())
            << "\" severity=\"" << SevLevel[max_sev]
            << "\" cardinality=\"" << rep.size() << "\">\n";
        RecursiveXML(out, rep, flags, 2);
        Indent(out, 1);
        out << "</test>\n";
    }
    out << "</discrepancy_report>\n";
}

END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
