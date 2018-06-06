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
    for (auto it: subs) {
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


static void RecursiveText(ostream& out, const TReportItemList& list, const vector<string>& fnames, bool fatal, bool ext)
{
    for (auto it: list) {
        if (it->IsExtended() && !ext) {
            continue;
        }
        if (fatal && ShowFatal(*it)) {
            out << "FATAL: ";
        }
        out << deunderscore(it->GetTitle()) << ": " << it->GetMsg() << "\n";
        TReportItemList subs = it->GetSubitems();
        if (!subs.empty() && (ext || !subs[0]->IsExtended())) {
            RecursiveText(out, subs, fnames, fatal, ext);
        }
        else {
            TReportObjectList det = it->GetDetails();
            for (auto obj: det) {
                if (!fnames.empty()) {
                    out << fnames[obj->GetFileID()] << ":";
                }
                out << obj->GetText() << "\n";
            }
        }
    }
}


static void RecursiveSummary(ostream& out, const TReportItemList& list, bool fatal, size_t level = 0)
{
    for (auto it: list) {
        if (!level) {
            if (fatal && ShowFatal(*it)) {
                out << "FATAL: ";
            }
            out << deunderscore(it->GetTitle()) << ": " << it->GetMsg() << "\n";
        }
        else if (it->IsSummary()) {
            for (size_t i = 0; i < level; i++) {
                out << "\t";
            }
            if (fatal && ShowFatal(*it)) {
                out << "FATAL: ";
            }
            out << it->GetMsg() << "\n";
        }
        else {
            continue;
        }
        RecursiveSummary(out, it->GetSubitems(), fatal, level + 1);
    }
}


static bool RecursiveFatalSummary(ostream& out, const TReportItemList& list, size_t level = 0)
{
    bool found = false;
    for (auto it: list) {
        if (it->IsFatal() && it->GetTitle() != "SOURCE_QUALS" && it->GetTitle() != "SUSPECT_PRODUCT_NAMES") {
            found = true;
            if (!level) {
                out << "FATAL: ";
                out << deunderscore(it->GetTitle()) << ": " << it->GetMsg() << "\n";
            }
            else if (it->IsSummary()) {
                for (size_t i = 0; i < level; i++) {
                    out << "\t";
                }
                out << "FATAL: ";
                out << it->GetMsg() << "\n";
            }
            else {
                continue;
            }
            RecursiveFatalSummary(out, it->GetSubitems(), level + 1);
        }
    }
    return found;
}


void CDiscrepancyContext::OutputText(ostream& out, bool fatal, bool summary, bool ext, bool big)
{
    out << (big ? "Discrepancy Report Results (due to the large size of the file some checks may not have run)\n\n" : "Discrepancy Report Results\n\n");

    out << "Summary\n";
    const CDiscrepancyGroup& order = x_OutputOrder();
    TReportItemList group0 = order[0].Collect(m_Tests, false);
    TReportItemList group1 = order[1].Collect(m_Tests, true);
    RecursiveSummary(out, group0, fatal);
    if (fatal) {
        RecursiveFatalSummary(out, group1);
    }
    RecursiveSummary(out, group1, fatal);

    if (summary) return;
    
    out << "\nDetailed Report\n\n";
    RecursiveText(out, group0, m_Files, fatal, ext);
    RecursiveText(out, group1, m_Files, fatal, ext);
}


static const size_t XML_INDENT = 2;

static void Indent(ostream& out, size_t indent)
{
    for (size_t i = 0; i < indent; i++) {
        out << ' ';
    }
}


static string SevLevel[] = {"INFO", "WARNING", "FATAL"};

static void RecursiveXML(ostream& out, const TReportItemList& list, const vector<string>& fnames, size_t indent, bool ext)
{
    for (auto it: list) {
        if (it->IsExtended() && !ext) {
            continue;
        }
        Indent(out, indent);
        out << "<details message=\"" << NStr::XmlEncode(it->GetXml()) << "\"" << " severity=\"" << SevLevel[it->GetSeverity()] << "\"";
        if (it->GetCount()) {
            out << " cardinality=\"" << NStr::Int8ToString(it->GetCount()) << "\"";
        }
        if (!it->GetUnit().empty()) {
            out << " unit=\"" << NStr::XmlEncode(it->GetUnit()) << "\"";
        }
        if (it->CanAutofix()) {
            out << " autofix=\"true\"";
        }
        out << ">\n";

        indent += XML_INDENT;
        TReportItemList subs = it->GetSubitems();
        if (!subs.empty() && (ext || !subs[0]->IsExtended())) {
            RecursiveXML(out, subs, fnames, indent, ext);
        }
        else {
            for (auto obj: it->GetDetails()) {
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
                }
                if (!fnames.empty()) {
                    out << " file=\"" << NStr::XmlEncode(fnames[obj->GetFileID()]) << "\"";
                }
                if (!obj->GetFeatureType().empty()) {
                    out << " feature_type=\"" << NStr::XmlEncode(obj->GetFeatureType()) << "\"";
                }
                if (!obj->GetProductName().empty()) {
                    out << (obj->GetFeatureType() == "Gene" ? " symbol=\"" : " product=\"") << NStr::XmlEncode(obj->GetProductName()) << "\"";
                }
                if (!obj->GetLocation().empty()) {
                    out << " location=\"" << NStr::XmlEncode(obj->GetLocation()) << "\"";
                }
                if (!obj->GetLocusTag().empty()) {
                    out << " locus_tag=\"" << NStr::XmlEncode(obj->GetLocusTag()) << "\"";
                }
                string text = NStr::Replace(obj->GetText(), "\t", " ");
                out << " label=\"" << NStr::XmlEncode(text) << "\"/>\n";
            }
        }
        indent -= XML_INDENT;
        Indent(out, indent);
        out << "</details>\n";
    }
}


void CDiscrepancyContext::OutputXML(ostream& out, bool ext)
{
    const TDiscrepancyCaseMap& tests = GetTests();
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<discrepancy_report>\n";

    for (auto tst: tests) {
        TReportItemList rep = tst.second->GetReport();
        if (rep.empty()) {
            continue;
        }
        CReportItem::ESeverity sev = CReportItem::eSeverity_info;
        for (auto it: rep) {
            CReportItem::ESeverity s = it->GetSeverity();
            if (s > sev) {
                sev = s;
            }
        }
        TReportObjectList objs = tst.second->GetObjects();
        Indent(out, XML_INDENT);
        out << "<test name=\"" << deunderscore(tst.first) << "\" description=\"" << NStr::XmlEncode(GetDiscrepancyDescr(tst.first)) << "\" severity=\"" << SevLevel[sev] << "\" cardinality=\"" << objs.size() << "\">\n";
        RecursiveXML(out, rep, m_Files, XML_INDENT * 2, ext);
        Indent(out, XML_INDENT);
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
        LIST_TEST(TERMINAL_NS)
        LIST_TEST(MAP_CHROMOSOME_CONFLICT)
        LIST_TEST(TITLE_AUTHOR_CONFLICT)
        LIST_TEST(CITSUBAFFIL_CONFLICT)
        LIST_TEST(SUBMITBLOCK_CONFLICT)
        LIST_TEST(UNPUB_PUB_WITHOUT_TITLE)
        LIST_TEST(USA_STATE)
        LIST_TEST(RNA_NO_PRODUCT)
        LIST_TEST(SUSPECT_RRNA_PRODUCTS)
        LIST_TEST(SHORT_RRNA)
        LIST_TEST(MRNA_SHOULD_HAVE_PROTEIN_TRANSCRIPT_IDS)
        LIST_TEST(INCONSISTENT_PROTEIN_ID)
        LIST_TEST(MISSING_PROTEIN_ID)
        LIST_TEST(BAD_LOCUS_TAG_FORMAT)
        LIST_TEST(INCONSISTENT_LOCUS_TAG_PREFIX)
        LIST_TEST(MISSING_LOCUS_TAGS)
        LIST_TEST(MISSING_GENES)
        LIST_TEST(SHOW_HYPOTHETICAL_CDS_HAVING_GENE_NAME)
        LIST_TEST(EC_NUMBER_ON_UNKNOWN_PROTEIN)
        LIST_TEST(PARTIAL_CDS_COMPLETE_SEQUENCE)
        LIST_TEST(CONTAINED_CDS)
        LIST_TEST(RNA_CDS_OVERLAP)
        LIST_TEST(OVERLAPPING_RRNAS)
        LIST_TEST(ORDERED_LOCATION)
        LIST_TEST(PSEUDO_MISMATCH)
        LIST_TEST(EUKARYOTE_SHOULD_HAVE_MRNA)
        LIST_TEST(BACTERIA_SHOULD_NOT_HAVE_MRNA)
        LIST_TEST(BACTERIAL_PARTIAL_NONEXTENDABLE_PROBLEMS)
        LIST_TEST(BACTERIAL_JOINED_FEATURES_NO_EXCEPTION)
        LIST_TEST(BAD_BGPIPE_QUALS)
        LIST_TEST(INCONSISTENT_MOLTYPES)
        LIST_TEST(QUALITY_SCORES)
        LIST_TEST(SEGSETS_PRESENT)
        LIST_TEST(SOURCE_QUALS)
        LIST_TEST(SUSPECT_PRODUCT_NAMES)

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
