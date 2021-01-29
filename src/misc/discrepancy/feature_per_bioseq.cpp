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
#include <objtools/validator/utilities.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

DISCREPANCY_MODULE(feature_per_bioseq);

// COUNT_RRNAS

static inline string rRnaLabel(const CSeqFeatData& data) // eSubtype_rRNA assumed
{
    return data.IsRna() && data.GetRna().IsSetExt() && data.GetRna().GetExt().Which() == CRNA_ref::C_Ext::e_Name ? data.GetRna().GetExt().GetName() : "==invalid==";
}


DISCREPANCY_CASE(COUNT_RRNAS, SEQUENCE, eDisc, "Count rRNAs")
{
    const CSeqdesc* biosrc = context.GetBiosource();
    if (biosrc && biosrc->GetSource().IsSetGenome() && (biosrc->GetSource().GetGenome() == CBioSource::eGenome_mitochondrion || biosrc->GetSource().GetGenome() == CBioSource::eGenome_chloroplast || biosrc->GetSource().GetGenome() == CBioSource::eGenome_plastid)) {
        size_t total = 0;
        CReportNode tmp;
        for (const CSeq_feat* feat : context.FeatRRNAs()) {
            tmp[rRnaLabel(feat->GetData())].Add(*context.SeqFeatObjRef(*feat));
            total++;
        }
        if (total) {
            auto bsref = context.BioseqObjRef();
            string item = " [n] sequence[s] [has] [(]" + to_string(total) + "[)] rRNA feature" + (total == 1 ? kEmptyStr : "s");
            m_Objs[item].Add(*bsref).Incr();
            string short_name = bsref->GetShort();
            string subitem = "[n] rRNA feature[s] found on [(]" + short_name;
            for (auto& it : tmp.GetMap()) {
                m_Objs[item][subitem].Ext().Add(it.second->GetObjects());
            }
            for (auto& it : tmp.GetMap()) {
                if (it.second->GetObjects().size() > 1) {
                    m_Objs["[(]" + to_string(it.second->GetObjects().size()) + "[)] rRNA features on [(]" + short_name + "[)] have the same name [(](" + it.first + ")"].Add(tmp[it.first].GetObjects());
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(COUNT_RRNAS)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_ALIAS(COUNT_RRNAS, FIND_DUP_RRNAS);


// COUNT_TRNAS
// (also report extra and missing tRNAs)

struct DesiredAAData
{
    char   short_symbol;
    const char*  long_symbol;
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


DISCREPANCY_CASE(COUNT_TRNAS, SEQUENCE, eDisc, "Count tRNAs")
{
    const CSeqdesc* biosrc = context.GetBiosource();
    if (biosrc && biosrc->GetSource().IsSetGenome() && (biosrc->GetSource().GetGenome() == CBioSource::eGenome_mitochondrion || biosrc->GetSource().GetGenome() == CBioSource::eGenome_chloroplast || biosrc->GetSource().GetGenome() == CBioSource::eGenome_plastid)) {
        size_t total = 0;
        CReportNode tmp;
        for (const CSeq_feat* feat : context.FeatTRNAs()) {
            tmp[context.GetAminoacidName(*feat)].Add(*context.SeqFeatObjRef(*feat));
            total++;
        }
        if (total) {
            static CSafeStatic<map<string, size_t> > DesiredCount;
            if (DesiredCount->empty()) {
                for (size_t i = 0; i < sizeof(desired_aaList) / sizeof(desired_aaList[0]); i++) {
                    (*DesiredCount)[desired_aaList[i].long_symbol] = desired_aaList[i].num_expected;
                }
            }

            auto bsref = context.BioseqObjRef();
            string item = " [n] sequence[s] [has] [(]" + to_string(total) + "[)] tRNA feature" + (total == 1 ? kEmptyStr : "s");
            m_Objs[item].NoRec().Add(*bsref);
            string short_name = bsref->GetShort();
            string subitem = "[n] tRNA feature[s] found on [(]" + short_name;
            for (auto& it : tmp.GetMap()) {
                m_Objs[item][subitem].Ext().Add(it.second->GetObjects());
            }
            // extra tRNAs
            for (size_t i = 0; i < sizeof(desired_aaList) / sizeof(desired_aaList[0]); i++) {
                const size_t n = tmp[desired_aaList[i].long_symbol].GetObjects().size();
                if (n > desired_aaList[i].num_expected) {
                    subitem = "Sequence [(]" + short_name + "[)] has [(]" + to_string(n) + "[)] trna-[(]" + desired_aaList[i].long_symbol + "[)] feature" + (n == 1 ? kEmptyStr : "s");
                    m_Objs[subitem].Add(*bsref);
                    m_Objs[subitem].Add(tmp[desired_aaList[i].long_symbol].GetObjects());
                }
            }
            // unusual tRNAs
            for (auto& it : tmp.GetMap()) {
                if (DesiredCount->find(it.first) == DesiredCount->end()) {
                    subitem = "Sequence [(]" + short_name + "[)] has [(]" + to_string(it.second->GetObjects().size()) + "[)] trna-[(]" + it.first + "[)] feature" + (it.second->GetObjects().size() == 1 ? kEmptyStr : "s");
                    m_Objs[subitem].Add(*bsref);
                    m_Objs[subitem].Add(tmp[it.first].GetObjects(), false);
                }
            }
            // missing tRNAs
            for (size_t i = 0; i < sizeof(desired_aaList) / sizeof(desired_aaList[0]); i++) {
                const size_t n = tmp[desired_aaList[i].long_symbol].GetObjects().size();
                if (n < desired_aaList[i].num_expected) {
                    subitem = "Sequence [(]" + short_name + "[)] is missing trna-[(]" + desired_aaList[i].long_symbol;
                    m_Objs[subitem].Add(*bsref);
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(COUNT_TRNAS)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_ALIAS(COUNT_TRNAS, FIND_DUP_TRNAS);

END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
