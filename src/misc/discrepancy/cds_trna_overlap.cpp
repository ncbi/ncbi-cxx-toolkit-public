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

BEGIN_NCBI_SCOPE;
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

DISCREPANCY_MODULE(cds_trna_overlap);


// CDS_TRNA_OVERLAP

CSeq_loc trnaTotal;
CSeq_loc cdsTotal;

//  ----------------------------------------------------------------------------
string sFeatureToString(
    CSeqFeatData::ESubtype subtype,
    const CSeq_loc& location)
//  ----------------------------------------------------------------------------
{
    string featType = CSeqFeatData::SubtypeValueToName(subtype);
    string startPt = NStr::IntToString(location.GetStart(eExtreme_Positional));
    string endPt = NStr::IntToString(location.GetStop(eExtreme_Positional));
    return (featType + " at " + startPt + ".." + endPt);
}


//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(CDS_TRNA_OVERLAP, CSeq_feat_BY_BIOSEQ, eAll, "CDS tRNA Overlap")
//  ----------------------------------------------------------------------------
{
    CSeqFeatData::ESubtype subtype = obj.GetData().GetSubtype();
    if (subtype == CSeqFeatData::eSubtype_tRNA) {
        const CSeq_loc& trnaLocation = obj.GetLocation();
        CRef<CSeq_loc> cdsIntersect = cdsTotal.Intersect(trnaLocation, 0, 0);
        if (!cdsIntersect->IsNull()) {
            string msg(sFeatureToString(subtype, trnaLocation) +
                " overlaps with at least one CDS");
            m_Objs[msg].Add(
                *new CDiscrepancyObject(
                CConstRef<CSeq_feat>(&obj),
                context.GetScope(),
                context.GetFile(),
                context.GetKeepRef()),
                false);
        }
        trnaTotal.Add(trnaLocation);
    }  
    if (subtype == CSeqFeatData::eSubtype_cdregion) {
        const CSeq_loc& cdsLocation = obj.GetLocation();
        CRef<CSeq_loc> trnaIntersect = trnaTotal.Intersect(cdsLocation, 0, 0);
        if (!trnaIntersect->IsNull()) {
            string msg(sFeatureToString(subtype, cdsLocation) +
                " overlaps with at least one tRNA");
            m_Objs[msg].Add(
                *new CDiscrepancyObject(
                    CConstRef<CSeq_feat>(&obj), 
                    context.GetScope(), 
                    context.GetFile(), 
                    context.GetKeepRef()), 
                    false);
        }
        cdsTotal.Add(cdsLocation);
    }
    //cerr << "Discrepancy case CDS_TRNA_OVERLAP" << endl;
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(CDS_TRNA_OVERLAP)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
