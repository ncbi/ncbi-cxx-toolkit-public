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

// THIS WHOLE THING IS EVIL!
// DATA LAYER IS MIXED WITH PRESENTATION LAYER :(


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


static inline string deunderscore(const string s)
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
        out << deunderscore(it->GetTitle()) << ": " << it->GetMsg() << '\n';
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
                out << obj->GetText() << '\n';
            }
        }
    }
}


static void RecursiveSummary(ostream& out, const TReportItemList& list, unsigned short flags, size_t level = 0)
{
    bool fatal = (flags & CDiscrepancySet::eOutput_Fatal) != 0;
    for (const auto& it : list) {
        if (level == 0) {
            if (fatal && ShowFatal(*it)) {
                out << "FATAL: ";
            }
            out << deunderscore(it->GetTitle()) << ": " << it->GetMsg() << '\n';
        }
        else if (it->IsSummary()) {
            out << string(level, '\t');
            if (fatal && ShowFatal(*it)) {
                out << "FATAL: ";
            }
            out << it->GetMsg() << '\n';
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
        if (it->IsFatal() && it->GetTitle() != "SOURCE_QUALS" && it->GetTitle() != "SUSPECT_PRODUCT_NAMES") {
            found = true;
            if (level == 0) {
                out << "FATAL: ";
                out << deunderscore(it->GetTitle()) << ": " << it->GetMsg() << '\n';
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


void CDiscrepancyContext::OutputText(ostream& out, unsigned short flags, char group)
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
    if (m_Group0.empty() && m_Group1.empty()) {
        const CDiscrepancyGroup& order = x_OutputOrder();
        m_Group0 = order[0].Collect(m_Tests, false);
        m_Group1 = order[1].Collect(m_Tests, true);
    }
    RecursiveSummary(out, m_Group0, flags);
    if (flags & eOutput_Fatal) {
        RecursiveFatalSummary(out, m_Group1, flags);
    }
    RecursiveSummary(out, m_Group1, flags);

    if (flags & eOutput_Summary) return;

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


void CDiscrepancyContext::OutputXML(ostream& out, unsigned short flags)
{
    const TDiscrepancyCaseMap& tests = GetTests();
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<discrepancy_report>\n";

    for (const auto& tst : tests) {
        TReportItemList rep = tst.second->GetReport();
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
        out << "<test name=\"" << deunderscore(tst.first)
            << "\" description=\"" << NStr::XmlEncode(GetDiscrepancyDescr(tst.first))
            << "\" severity=\"" << SevLevel[max_sev]
            << "\" cardinality=\"" << rep.size() << "\">\n";
        RecursiveXML(out, rep, flags, 2);
        Indent(out, 1);
        out << "</test>\n";
    }
    out << "</discrepancy_report>";
}


const CDiscrepancyGroup& CDiscrepancyContext::x_OutputOrder()
{
    if (!m_Order) {
        CRef<CDiscrepancyGroup> G, H;
        m_Order.Reset(new CDiscrepancyGroup);
        G.Reset(new CDiscrepancyGroup("", "")); m_Order->Add(G);
#define LIST_TEST(name) H.Reset(new CDiscrepancyGroup("", #name)); G->Add(H);
        LIST_TEST(COUNT_NUCLEOTIDES)
        LIST_TEST(LONG_NO_ANNOTATION)
        LIST_TEST(NO_ANNOTATION)

        G.Reset(new CDiscrepancyGroup("", "")); m_Order->Add(G);
        LIST_TEST(SOURCE_QUALS)
        LIST_TEST(DUP_SRC_QUAL)
        LIST_TEST(MAP_CHROMOSOME_CONFLICT)
        LIST_TEST(BIOMATERIAL_TAXNAME_MISMATCH)
        LIST_TEST(SPECVOUCHER_TAXNAME_MISMATCH)
        LIST_TEST(STRAIN_CULTURE_COLLECTION_MISMATCH)
        LIST_TEST(TRINOMIAL_SHOULD_HAVE_QUALIFIER)
        LIST_TEST(REQUIRED_STRAIN)
        LIST_TEST(BACTERIA_SHOULD_NOT_HAVE_ISOLATE)
        LIST_TEST(METAGENOMIC)
        LIST_TEST(METAGENOME_SOURCE)
        LIST_TEST(MAG_SHOULD_NOT_HAVE_STRAIN)
        LIST_TEST(MAG_MISSING_ISOLATE)

        LIST_TEST(TITLE_ENDS_WITH_SEQUENCE)
        LIST_TEST(GAPS)
        LIST_TEST(N_RUNS)
        LIST_TEST(PERCENT_N)
        LIST_TEST(10_PERCENTN)
        LIST_TEST(TERMINAL_NS)
        LIST_TEST(ZERO_BASECOUNT)
        LIST_TEST(LOW_QUALITY_REGION)
        LIST_TEST(UNUSUAL_NT)
        LIST_TEST(SHORT_CONTIG)
        LIST_TEST(SHORT_SEQUENCES)
        LIST_TEST(SEQUENCES_ARE_SHORT)
        LIST_TEST(GENOMIC_MRNA)

        LIST_TEST(CHECK_AUTH_CAPS)
        LIST_TEST(CHECK_AUTH_NAME)
        LIST_TEST(TITLE_AUTHOR_CONFLICT)
        LIST_TEST(CITSUBAFFIL_CONFLICT)
        LIST_TEST(SUBMITBLOCK_CONFLICT)
        LIST_TEST(UNPUB_PUB_WITHOUT_TITLE)
        LIST_TEST(USA_STATE)

        LIST_TEST(FEATURE_COUNT)
        LIST_TEST(PROTEIN_NAMES)
        LIST_TEST(SUSPECT_PRODUCT_NAMES)
        LIST_TEST(SUSPECT_PHRASES)
        LIST_TEST(INCONSISTENT_PROTEIN_ID)
        LIST_TEST(MISSING_PROTEIN_ID)
        LIST_TEST(MRNA_SHOULD_HAVE_PROTEIN_TRANSCRIPT_IDS)
        LIST_TEST(BAD_LOCUS_TAG_FORMAT)
        LIST_TEST(INCONSISTENT_LOCUS_TAG_PREFIX)
        LIST_TEST(DUPLICATE_LOCUS_TAGS)
        LIST_TEST(MISSING_LOCUS_TAGS)
        LIST_TEST(NON_GENE_LOCUS_TAG)
        LIST_TEST(MISSING_GENES)
        LIST_TEST(EXTRA_GENES)
        LIST_TEST(BAD_BACTERIAL_GENE_NAME)
        LIST_TEST(BAD_GENE_NAME)
        LIST_TEST(BAD_GENE_STRAND)
        LIST_TEST(DUP_GENES_OPPOSITE_STRANDS)
        LIST_TEST(GENE_PARTIAL_CONFLICT)
        LIST_TEST(GENE_PRODUCT_CONFLICT)
        LIST_TEST(SHOW_HYPOTHETICAL_CDS_HAVING_GENE_NAME)
        LIST_TEST(EC_NUMBER_ON_UNKNOWN_PROTEIN)
        LIST_TEST(MISC_FEATURE_WITH_PRODUCT_QUAL)
        LIST_TEST(PARTIAL_CDS_COMPLETE_SEQUENCE)
        LIST_TEST(CONTAINED_CDS)
        LIST_TEST(RNA_CDS_OVERLAP)
        LIST_TEST(CDS_TRNA_OVERLAP)
        LIST_TEST(OVERLAPPING_RRNAS)
        LIST_TEST(FIND_OVERLAPPED_GENES)
        LIST_TEST(ORDERED_LOCATION)
        LIST_TEST(PARTIAL_PROBLEMS)
        LIST_TEST(FEATURE_LOCATION_CONFLICT)
        LIST_TEST(PSEUDO_MISMATCH)
        LIST_TEST(EUKARYOTE_SHOULD_HAVE_MRNA)
        LIST_TEST(MULTIPLE_CDS_ON_MRNA)
        LIST_TEST(CDS_WITHOUT_MRNA)
        LIST_TEST(BACTERIA_SHOULD_NOT_HAVE_MRNA)
        LIST_TEST(BACTERIAL_PARTIAL_NONEXTENDABLE_EXCEPTION)
        LIST_TEST(BACTERIAL_PARTIAL_NONEXTENDABLE_PROBLEMS)
        LIST_TEST(BACTERIAL_JOINED_FEATURES_NO_EXCEPTION)
        LIST_TEST(JOINED_FEATURES)
        LIST_TEST(RIBOSOMAL_SLIPPAGE)
        LIST_TEST(BAD_BGPIPE_QUALS)
        LIST_TEST(CDS_HAS_NEW_EXCEPTION)
        LIST_TEST(SHOW_TRANSL_EXCEPT)
        LIST_TEST(RNA_NO_PRODUCT)
        LIST_TEST(RRNA_NAME_CONFLICTS)
        LIST_TEST(SUSPECT_RRNA_PRODUCTS)
        LIST_TEST(SHORT_RRNA)
        LIST_TEST(FIND_BADLEN_TRNAS)
        LIST_TEST(UNUSUAL_MISC_RNA)
        LIST_TEST(SHORT_LNCRNA)
        LIST_TEST(SHORT_INTRON)
        LIST_TEST(EXON_INTRON_CONFLICT)
        LIST_TEST(EXON_ON_MRNA)
        LIST_TEST(SHORT_PROT_SEQUENCES)

        LIST_TEST(INCONSISTENT_DBLINK)
        LIST_TEST(INCONSISTENT_MOLINFO_TECH)
        LIST_TEST(INCONSISTENT_MOLTYPES)
        LIST_TEST(INCONSISTENT_STRUCTURED_COMMENTS)
        LIST_TEST(QUALITY_SCORES)
        LIST_TEST(SEGSETS_PRESENT)
    }
    return *m_Order;
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
