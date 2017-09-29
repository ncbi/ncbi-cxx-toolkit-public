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
    ITERATE (TReportItemList, it, subs) {
        if ((*it)->IsSummary() && (*it)->IsFatal()) {
            return false;
        }
    }
    return true;
}


static void RecursiveText(ostream& out, const TReportItemList& list, bool fatal, bool ext)
{
    ITERATE (TReportItemList, it, list) {
        if ((*it)->IsExtended() && !ext) {
            continue;
        }
        if (fatal && ShowFatal(**it)) {
            out << "FATAL: ";
        }
        out << (*it)->GetTitle() << ": " << (*it)->GetMsg() << "\n";
        TReportItemList subs = (*it)->GetSubitems();
        if (!subs.empty() && (ext || !subs[0]->IsExtended() && !subs[0]->IsSummary())) {
            RecursiveText(out, subs, fatal, ext);
        }
        else {
            TReportObjectList det = (*it)->GetDetails();
            ITERATE (TReportObjectList, obj, det) {
                out << (*obj)->GetText() << "\n";
            }
            out << "\n";
        }
    }
}


static void RecursiveSummary(ostream& out, const TReportItemList& list, bool fatal, size_t level = 0)
{
    ITERATE (TReportItemList, it, list) {
        if (!level) {
            if (fatal && ShowFatal(**it)) {
                out << "FATAL: ";
            }
            out << (*it)->GetTitle() << ": " << (*it)->GetMsg() << "\n";
        }
        else if ((*it)->IsSummary()) {
            for (size_t i = 0; i < level; i++) {
                out << "\t" ;
            }
            if (fatal && ShowFatal(**it)) {
                out << "FATAL: ";
            }
            out << (*it)->GetMsg() << "\n";
        }
        else {
            continue;
        }
        TReportItemList subs = (*it)->GetSubitems();
        RecursiveSummary(out, subs, fatal, level + 1);
    }
}


void CDiscrepancyContext::OutputText(ostream& out, bool fatal, bool summary, bool ext)
{
    const TDiscrepancyCaseMap& tests = GetTests();
    out << "Discrepancy Report Results\n\n";

    out << "Summary\n";
    ITERATE(TDiscrepancyCaseMap, tst, tests) {
        TReportItemList rep = tst->second->GetReport();
        RecursiveSummary(out, rep, fatal);
    }
    if (summary) return;

    out << "\nDetailed Report\n\n";
    ITERATE(TDiscrepancyCaseMap, tst, tests) {
        TReportItemList rep = tst->second->GetReport();
        RecursiveText(out, rep, fatal, ext);
    }
}


static const size_t XML_INDENT = 2;

static void Indent(ostream& out, size_t indent)
{
    for (size_t i = 0; i < indent; i++) {
        out << ' ';
    }
}


static void RecursiveXML(ostream& out, const TReportItemList& list, size_t indent, bool ext)
{
    ITERATE(TReportItemList, it, list) {
        if ((*it)->IsExtended() && !ext) {
            continue;
        }
        Indent(out, indent);
        out << "<category message=\"" << NStr::XmlEncode((*it)->GetXml()) << "\"" << " severity=";
        switch ((*it)->GetSeverity()) {
            case CReportItem::eSeverity_info:
                out << "\"INFO\"";
                break;
            case CReportItem::eSeverity_warning:
                out << "\"WARNING\"";
                break;
            case CReportItem::eSeverity_error:
                out << "\"FATAL\"";
                break;
        }
        if ((*it)->GetCount()) {
            out << " cardinality=\"" << NStr::Int8ToString((*it)->GetCount()) << "\"";
        }
        if (!(*it)->GetUnit().empty()) {
            out << " unit=\"" << NStr::XmlEncode((*it)->GetUnit()) << "\"";
        }
        if ((*it)->CanAutofix()) {
            out << " autofix=\"true\"";
        }
        out << ">\n";

        TReportItemList subs = (*it)->GetSubitems();
        if (!subs.empty() && (ext || !subs[0]->IsExtended())) {
            RecursiveXML(out, subs, indent + XML_INDENT, ext);
        }
        else {
            TReportObjectList det = (*it)->GetDetails();
            ITERATE(TReportObjectList, obj, det) {
                Indent(out, indent + XML_INDENT);
                out << "<object text=\"" << NStr::XmlEncode((*obj)->GetText()) << "\"/>\n";
            }
        }
        Indent(out, indent);
        out << "</category>\n";
    }
}


void CDiscrepancyContext::OutputXML(ostream& out, bool ext)
{
    const TDiscrepancyCaseMap& tests = GetTests();
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<discrepancy_report>\n";

    ITERATE(TDiscrepancyCaseMap, tst, tests) {
        TReportItemList rep = tst->second->GetReport();
        if (rep.empty()) {
            continue;
        }
        Indent(out, XML_INDENT);
        out << "<test name=\"" << NStr::XmlEncode(tst->first) << "\">\n";
        RecursiveXML(out, rep, XML_INDENT * 2, ext);
        Indent(out, XML_INDENT);
        out << "</test>\n";
    }
    out << "</discrepancy_report>";
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
