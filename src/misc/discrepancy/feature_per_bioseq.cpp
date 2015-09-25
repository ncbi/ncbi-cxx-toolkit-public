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
 * Authors: Igor Filippov, Sema Kachalo
 *
 */

#include <ncbi_pch.hpp>
#include "discrepancy_core.hpp"
#include "utils.hpp"
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/feature.hpp>

BEGIN_NCBI_SCOPE;
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

DISCREPANCY_MODULE(feature_per_bioseq);


// DUPLICATE_GENE_LOCUS


DISCREPANCY_CASE(DUPLICATE_GENE_LOCUS, CSeq_feat_BY_BIOSEQ, eNormal, "Duplicate Gene Locus")
{
    if (!obj.GetData().IsGene() || !obj.GetData().GetGene().IsSetLocus()) {
        return;
    }
    if (m_Count != context.GetCountBioseq()) {
        m_Count = context.GetCountBioseq();
        Summarize();
    }
    // TODO Skip any Bioseqs that are an mRNA sequence in a GenProdSet
    CConstRef<CBioseq> bioseq = context.GetCurrentBioseq();
    CConstRef<CBioseq_set> bioseq_set = context.GetCurrentBioseq_set();
    if (!IsmRNASequenceInGenProdSet(bioseq, bioseq_set))
        m_Objs[obj.GetData().GetGene().GetLocus()].Add(*new CDiscrepancyObject(CConstRef<CSeq_feat>(&obj), context.GetScope(), context.GetFile(), context.GetKeepRef()), false);
}


DISCREPANCY_SUMMARIZE(DUPLICATE_GENE_LOCUS)
{
    if (m_Objs.empty()) {
        return;
    }
    NON_CONST_ITERATE(CReportNode::TNodeMap, it, m_Objs.GetMap()) {
        if (!NStr::IsBlank(it->first) && it->second->GetObjects().size() > 1) {
            CReportNode tmpNode;
            tmpNode["[n] genes have the same locus as another gene on the same Bioseq"].Add(it->second->GetObjects());
            TReportItemList tmpList = tmpNode.Export(*this)->GetSubitems();
            m_ReportItems.insert(m_ReportItems.end(), tmpList.begin(), tmpList.end());
        }
    }
    m_Objs.clear();
}


// COUNT_RRNAS

DISCREPANCY_CASE(COUNT_RRNAS, CSeq_feat_BY_BIOSEQ, eNormal, "Count rRNAs")
{
    if (obj.GetData().GetSubtype() != CSeqFeatData::eSubtype_rRNA) {
        return;
    }
    CBioSource::TGenome genome = context.GetCurrentGenome();
    if (genome != CBioSource::eGenome_mitochondrion && genome != CBioSource::eGenome_chloroplast && genome != CBioSource::eGenome_plastid) return;

    if (m_Count != context.GetCountBioseq()) {
        m_Count = context.GetCountBioseq();
        Summarize();
        m_Objs[kEmptyStr].Add(*new CDiscrepancyObject(context.GetCurrentBioseq(), context.GetScope(), context.GetFile(), context.GetKeepRef()));
    }

    string aa;
    feature::GetLabel(obj, &aa, feature::fFGL_Content);
    size_t n = aa.find_last_of('-');            // cut off the "rRNA-" prefix
    if (n != string::npos) {
        aa = aa.substr(n + 1); // is there any better way to get the aminoacid name?
    }
    m_Objs[aa].Add(*new CDiscrepancyObject(CConstRef<CSeq_feat>(&obj), context.GetScope(), context.GetFile(), context.GetKeepRef()), false);
}


DISCREPANCY_SUMMARIZE(COUNT_RRNAS)
{
    if (m_Objs.empty()) {
        return;
    }

    CRef<CReportObj> bioseq = m_Objs[kEmptyStr].GetObjects()[0];
    string short_name = bioseq->GetShort();
    m_Objs[kEmptyStr].clear();

    size_t total = 0;
    // count rRNAs
    CReportNode::TNodeMap& map = m_Objs.GetMap();
    NON_CONST_ITERATE(CReportNode::TNodeMap, it, map) {
        if (!NStr::IsBlank(it->first)) {
            total += it->second->GetObjects().size();
        }
    }
    CNcbiOstrstream ss;
    ss << " sequence " << short_name << " has " << total << " rRNA feature" << (total == 1 ? kEmptyStr : "s");
    m_Objs[kEmptyStr][CNcbiOstrstreamToString(ss)].Add(*bioseq);
    // duplicated rRNA names
    NON_CONST_ITERATE(CReportNode::TNodeMap, it, map) {
        if (NStr::IsBlank(it->first) || it->second->GetObjects().size() <= 1) {
            continue;
        }
        CNcbiOstrstream ss;
        ss << it->second->GetObjects().size() << " rRNA features on " << short_name << " have the same name (" << it->first << ")";
        m_Objs[kEmptyStr][CNcbiOstrstreamToString(ss)].Add(*bioseq);
        m_Objs[kEmptyStr][CNcbiOstrstreamToString(ss)].Add(m_Objs[it->first].GetObjects(), false);
    }
    TReportItemList tmpList = m_Objs[kEmptyStr].Export(*this, false)->GetSubitems();
    m_ReportItems.insert(m_ReportItems.end(), tmpList.begin(), tmpList.end());
    m_Objs.clear();
}


DISCREPANCY_ALIAS(COUNT_RRNAS, FIND_DUP_RRNAS);


// COUNT_TRNAS
// (also report extra and missing tRNAs)

struct DesiredAAData
{
    char short_symbol;
    string long_symbol;
    size_t num_expected;
};

static const DesiredAAData desired_aaList[] = {
        { 'A', "Ala", 1 },
        { 'B', "Asx", 0 },
        { 'C', "Cys", 1 },
        { 'D', "Asp", 1 },
        { 'E', "Glu", 1 },
        { 'F', "Phe", 1 },
        { 'G', "Gly", 1 },
        { 'H', "His", 1 },
        { 'I', "Ile", 1 },
        { 'J', "Xle", 0 },
        { 'K', "Lys", 1 },
        { 'L', "Leu", 2 },
        { 'M', "Met", 1 },
        { 'N', "Asn", 1 },
        { 'P', "Pro", 1 },
        { 'Q', "Gln", 1 },
        { 'R', "Arg", 1 },
        { 'S', "Ser", 2 },
        { 'T', "Thr", 1 },
        { 'V', "Val", 1 },
        { 'W', "Trp", 1 },
        { 'X', "Xxx", 0 },
        { 'Y', "Tyr", 1 },
        { 'Z', "Glx", 0 },
        { 'U', "Sec", 0 },
        { 'O', "Pyl", 0 },
        { '*', "Ter", 0 }
};


DISCREPANCY_CASE(COUNT_TRNAS, CSeq_feat_BY_BIOSEQ, eNormal, "Count tRNAs")
{
    if (obj.GetData().GetSubtype() != CSeqFeatData::eSubtype_tRNA) {
        return;
    }
    CBioSource::TGenome genome = context.GetCurrentGenome();
    if (genome != CBioSource::eGenome_mitochondrion && genome != CBioSource::eGenome_chloroplast && genome != CBioSource::eGenome_plastid) return;

    if (m_Count != context.GetCountBioseq()) {
        m_Count = context.GetCountBioseq();
        Summarize();
        m_Objs[kEmptyStr].Add(*new CDiscrepancyObject(context.GetCurrentBioseq(), context.GetScope(), context.GetFile(), context.GetKeepRef()));
    }

    string aa;
    feature::GetLabel(obj, &aa, feature::fFGL_Content);
    size_t n = aa.find_last_of('-');            // cut off the "tRNA-" prefix
    if (n != string::npos) {
        aa = aa.substr(n + 1); // is there any better way to get the aminoacid name?
    }
    m_Objs[aa].Add(*new CDiscrepancyObject(CConstRef<CSeq_feat>(&obj), context.GetScope(), context.GetFile(), context.GetKeepRef()), false);
}


DISCREPANCY_SUMMARIZE(COUNT_TRNAS)
{
    if (m_Objs.empty()) {
        return;
    }

    static map<string, int> DesiredCount;
    if (DesiredCount.empty()) {
        for (size_t i = 0; i < sizeof(desired_aaList) / sizeof(desired_aaList[0]); i++) {
            DesiredCount[desired_aaList[i].long_symbol] = desired_aaList[i].num_expected;
        }
    }

    CRef<CReportObj> bioseq = m_Objs[kEmptyStr].GetObjects()[0];
    string short_name = bioseq->GetShort();
    m_Objs[kEmptyStr].clear();

    size_t total = 0;
    // count tRNAs
    CReportNode::TNodeMap& map = m_Objs.GetMap();
    NON_CONST_ITERATE(CReportNode::TNodeMap, it, map) {
        if (!NStr::IsBlank(it->first)) {
            total += it->second->GetObjects().size();
        }
    }

    CNcbiOstrstream ss;
    ss << " sequence " << short_name << " has " << total << " tRNA feature" << (total == 1 ? kEmptyStr : "s");
    m_Objs[kEmptyStr][CNcbiOstrstreamToString(ss)].Add(*bioseq);

    // extra tRNAs
    for (size_t i = 0; i < sizeof(desired_aaList) / sizeof(desired_aaList[0]); i++) {
        const size_t n = m_Objs[desired_aaList[i].long_symbol].GetObjects().size();
        if (n <= desired_aaList[i].num_expected) {
            continue;
        }
        CNcbiOstrstream ss;
        ss << "sequence " << short_name << " has " << n << " trna-" << desired_aaList[i].long_symbol << " feature" << (n == 1 ? kEmptyStr : "s");
        m_Objs[kEmptyStr][CNcbiOstrstreamToString(ss)].Add(*bioseq);
        m_Objs[kEmptyStr][CNcbiOstrstreamToString(ss)].Add(m_Objs[desired_aaList[i].long_symbol].GetObjects(), false);
    }
    NON_CONST_ITERATE(CReportNode::TNodeMap, it, map) {
        if (NStr::IsBlank(it->first) || DesiredCount.find(it->first) != DesiredCount.end()) {
            continue;
        }
        CNcbiOstrstream ss;
        ss << "sequence " << short_name << " has " << it->second->GetObjects().size() << " trna-" << it->first << " feature" << (it->second->GetObjects().size() == 1 ? kEmptyStr : "s");
        m_Objs[kEmptyStr][CNcbiOstrstreamToString(ss)].Add(*bioseq);
        m_Objs[kEmptyStr][CNcbiOstrstreamToString(ss)].Add(m_Objs[it->first].GetObjects(), false);
    }

    // missing tRNAs
    for (size_t i = 0; i < sizeof(desired_aaList) / sizeof(desired_aaList[0]); i++) {
        if (!desired_aaList[i].num_expected) {
            continue;
        }
        const size_t n = m_Objs[desired_aaList[i].long_symbol].GetObjects().size();
        if (n >= desired_aaList[i].num_expected) {
            continue;
        }
        CNcbiOstrstream ss;
        ss << "sequence " << short_name << " is missing trna-" << desired_aaList[i].long_symbol;
        m_Objs[kEmptyStr][CNcbiOstrstreamToString(ss)].Add(*bioseq);
    }

    TReportItemList tmpList = m_Objs[kEmptyStr].Export(*this, false)->GetSubitems();
    m_ReportItems.insert(m_ReportItems.end(), tmpList.begin(), tmpList.end());
    m_Objs.clear();
}


DISCREPANCY_ALIAS(COUNT_TRNAS, FIND_DUP_TRNAS);


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
