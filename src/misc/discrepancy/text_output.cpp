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
        if (!subs.empty() && (ext || !subs[0]->IsExtended() && !subs[0]->IsSummary())) {
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
            out << "\n";
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
                out << "\t" ;
            }
            if (fatal && ShowFatal(*it)) {
                out << "FATAL: ";
            }
            out << it->GetMsg() << "\n";
        }
        else {
            continue;
        }
        //TReportItemList subs = it->GetSubitems();
        RecursiveSummary(out, it->GetSubitems(), fatal, level + 1);
    }
}


void CDiscrepancyContext::OutputText(ostream& out, bool fatal, bool summary, bool ext, bool big)
{
    const TDiscrepancyCaseMap& tests = GetTests();
    out << (big ? "Discrepancy Report Results (due to the large size of the file some checks may not have run)\n\n" : "Discrepancy Report Results\n\n");

    out << "Summary\n";
    for (auto tst: tests) {
        //TReportItemList rep = tst.second->GetReport();
        RecursiveSummary(out, tst.second->GetReport(), fatal);
    }
    if (summary) return;
    
    out << "\nDetailed Report\n\n";
    for (auto tst: tests) {
        //TReportItemList rep = tst.second->GetReport();
        RecursiveText(out, tst.second->GetReport(), m_Files, fatal, ext);
    }
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
                    out << " product=\"" << NStr::XmlEncode(obj->GetProductName()) << "\"";
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


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
