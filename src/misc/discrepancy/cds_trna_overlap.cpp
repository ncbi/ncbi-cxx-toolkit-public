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
 * Authors: Frank Ludwig, based on similar discrepancy tests
 *
 */

#include <ncbi_pch.hpp>
#include "discrepancy_core.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

DISCREPANCY_MODULE(cds_trna_overlap);


// CDS_TRNA_OVERLAP

DISCREPANCY_CASE(CDS_TRNA_OVERLAP, CSeq_feat_BY_BIOSEQ, eDisc, "CDS tRNA Overlap")
{
    if (m_Count != context.GetCountBioseq()) {
        m_Count = context.GetCountBioseq();
        Summarize(context);
    }
    CSeqFeatData::ESubtype subtype = obj.GetData().GetSubtype();
    if (subtype == CSeqFeatData::eSubtype_tRNA) {
        m_Objs["tRNA"].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj), eKeepRef), false);
    }  
    else if (subtype == CSeqFeatData::eSubtype_cdregion) {
        m_Objs["CDS"].Add(*context.NewDiscObj(CConstRef<CSeq_feat>(&obj), eKeepRef), false);
    }
}


DISCREPANCY_SUMMARIZE(CDS_TRNA_OVERLAP)
{
    if (m_Objs["tRNA"].empty() || m_Objs["CDS"].empty()) {
        return;
    }
    static const char* kMessage = "[n] Bioseq[s] [has] coding regions that overlap tRNAs";
    TReportObjectList& trna = m_Objs["tRNA"].GetObjects();
    TReportObjectList& cds = m_Objs["CDS"].GetObjects();
    CSeq_loc trna_L;
    CSeq_loc cds_L;
    CReportNode trna_N;
    CReportNode cds_N;
    
    ITERATE (TReportObjectList, it, trna) {
        trna_L.Add(((CSeq_feat*)(*it)->GetObject().GetPointer())->GetLocation());
    }
    ITERATE (TReportObjectList, it, cds) {
        cds_L.Add(((CSeq_feat*)(*it)->GetObject().GetPointer())->GetLocation());
    }
    NON_CONST_ITERATE (TReportObjectList, it, trna) {
        if (cds_L.Intersect(((CSeq_feat*)(*it)->GetObject().GetPointer())->GetLocation(), 0, 0)) {
            trna_N.Add(**it);
        }
    }
    NON_CONST_ITERATE (TReportObjectList, it, cds) {
        if (trna_L.Intersect(((CSeq_feat*)(*it)->GetObject().GetPointer())->GetLocation(), 0, 0)) {
            cds_N.Add(**it);
        }
    }
    m_Objs["tRNA"].clear();
    m_Objs["CDS"].clear();
    if (!cds_N.empty() || !trna_N.empty()) {
        CReportNode& out = m_Objs[kEmptyStr]["[n] Bioseq[s] [has] coding regions that overlap tRNAs"].Incr();
        string cds_S = "[n] coding region[s] [has] overlapping tRNAs";
        string trna_S = "[n] tRNA[s] [has] overlapping CDS";
        size_t n = out.GetMap().size();
        for (size_t i = 0; i < n; i++) {
            trna_S += " ";
            cds_S += " ";
        }
        if (!cds_N.empty()) {
            out[cds_S].Ext().Add(cds_N.GetObjects());
        }
        if (!trna_N.empty()) {
            out[trna_S].Ext().Add(trna_N.GetObjects());
        }
        m_ReportItems = m_Objs[kEmptyStr].Export(*this)->GetSubitems();
    }
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
