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
 * Authors: Sema Kachalo
 *
 */

#include <ncbi_pch.hpp>
#include "discrepancy_core.hpp"
#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

DISCREPANCY_MODULE(overlapping_features);

// CDS_TRNA_OVERLAP

DISCREPANCY_CASE(CDS_TRNA_OVERLAP, CSeq_feat_BY_BIOSEQ, eDisc | eSubmitter | eSmart, "CDS tRNA Overlap")
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
        CRef<CSeq_loc> loc = cds_L.Intersect(((CSeq_feat*)(*it)->GetObject().GetPointer())->GetLocation(), 0, 0);
        if (loc.NotEmpty() && !loc->IsEmpty() && !loc->IsNull()) {
            trna_N.Add(**it);
        }
    }
    NON_CONST_ITERATE (TReportObjectList, it, cds) {
        CRef<CSeq_loc> loc = trna_L.Intersect(((CSeq_feat*)(*it)->GetObject().GetPointer())->GetLocation(), 0, 0);
        if (loc.NotEmpty() && !loc->IsEmpty() && !loc->IsNull()) {
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


// RNA_CDS_OVERLAP

typedef pair<size_t, bool> TRNALength;
typedef map<string, TRNALength > TRNALengthMap;

static const TRNALengthMap kTrnaLengthMap{
    { "16S", { 1000, false } },
    { "18S", { 1000, false } },
    { "23S", { 2000, false } },
    { "25S", { 1000, false } },
    { "26S", { 1000, false } },
    { "28S", { 1000, false } },
    { "28S", { 3300, false } },
    { "small", { 1000, false } },
    { "large", { 1000, false } },
    { "5.8S", { 130, true } },
    { "5S", { 90, true } } 
};


bool IsShortrRNA(const CSeq_feat& f, CScope* scope)
{
    if (f.GetData().GetSubtype() != CSeqFeatData::eSubtype_rRNA) {
        return false;
    }

    bool is_bad = false;

    size_t len = sequence::GetLength(f.GetLocation(), scope);

    string rrna_name = f.GetData().GetRna().GetRnaProductName();

    ITERATE (TRNALengthMap, it, kTrnaLengthMap) {
        if (NStr::FindNoCase(rrna_name, it->first) != string::npos &&
            len < it->second.first &&
            (!it->second.second || (f.IsSetPartial() && f.GetPartial())) ) {
            is_bad = true;
            break;
        }
    }

    return is_bad;
}


const string kCDSRNAAnyOverlap = "[n/2] coding region[s] overlap RNA feature[s]";
const string kCDSRNAExactMatch = "[n/2] coding region location[s] exactly match an RNA location";
const string kCDSRNAContainedIn = "[n/2] coding region[s] [is] completely contained in RNA[s]";
const string kCDSRNAContains = "[n/2] coding region[s] completely contain RNA[s]";
const string kCDSRNAContainstRNA = "[n/2] coding region[s] completely contain tRNA[s]";
const string kCDSRNAOverlapNoContain = "[n/2] coding regions overlap RNA[s] (no containment)";
const string kCDSRNAOverlapNoContainSameStrand = "[n/2] coding region[s] overlap RNA[s] on the same strand (no containment)";
const string kCDSRNAOverlapNoContainOppStrand = "[n/2] coding region[s] overlap RNA[s] on the opposite strand (no containment)";

void ProcessCDSRNAPair(CConstRef<CSeq_feat> cds_sf, CConstRef<CSeq_feat> rna_sf, CReportNode& m_Objs, CDiscrepancyContext& context)
{
    sequence::ECompare loc_compare =
        sequence::Compare(cds_sf->GetLocation(),
        rna_sf->GetLocation(),
        &(context.GetScope()),
        sequence::fCompareOverlapping);
    if (loc_compare == sequence::eSame) {
        m_Objs[kCDSRNAAnyOverlap][kCDSRNAExactMatch].Ext().Add(*context.NewDiscObj(cds_sf), false).Add(*context.NewDiscObj(rna_sf), false).Fatal();
    }
    else if (loc_compare == sequence::eContained) {
        m_Objs[kCDSRNAAnyOverlap][kCDSRNAContainedIn].Ext().Add(*context.NewDiscObj(cds_sf), false).Add(*context.NewDiscObj(rna_sf), false).Fatal();
    }
    else if (loc_compare == sequence::eContains) {
        if (rna_sf->GetData().GetSubtype() == CSeqFeatData::eSubtype_tRNA) {
            m_Objs[kCDSRNAAnyOverlap][kCDSRNAContainstRNA].Ext().Add(*context.NewDiscObj(cds_sf), false).Add(*context.NewDiscObj(rna_sf), false).Fatal();
        }
        else {
            m_Objs[kCDSRNAAnyOverlap][kCDSRNAContains].Ext().Add(*context.NewDiscObj(cds_sf), false).Add(*context.NewDiscObj(rna_sf), false).Fatal();
        }
    }
    else if (loc_compare != sequence::eNoOverlap) {
        ENa_strand cds_strand = cds_sf->GetLocation().GetStrand();
        ENa_strand rna_strand = rna_sf->GetLocation().GetStrand();
        if (cds_strand == eNa_strand_minus && rna_strand != eNa_strand_minus) {
            //ADD_CDS_RNA_TRIPLET(kCDSRNAOverlapNoContain, kCDSRNAOverlapNoContainOppStrand);
            m_Objs[kCDSRNAAnyOverlap][kCDSRNAOverlapNoContain].Ext()[kCDSRNAOverlapNoContainOppStrand].Ext().Add(*context.NewDiscObj(cds_sf), false).Add(*context.NewDiscObj(rna_sf), false).Fatal();
        }
        else if (cds_strand != eNa_strand_minus && rna_strand == eNa_strand_minus) {
            //ADD_CDS_RNA_TRIPLET(kCDSRNAOverlapNoContain, kCDSRNAOverlapNoContainOppStrand);
            m_Objs[kCDSRNAAnyOverlap][kCDSRNAOverlapNoContain].Ext()[kCDSRNAOverlapNoContainOppStrand].Ext().Add(*context.NewDiscObj(cds_sf), false).Add(*context.NewDiscObj(rna_sf), false).Fatal();
        }
        else {
            //ADD_CDS_RNA_TRIPLET(kCDSRNAOverlapNoContain, kCDSRNAOverlapNoContainSameStrand);
            m_Objs[kCDSRNAAnyOverlap][kCDSRNAOverlapNoContain].Ext()[kCDSRNAOverlapNoContainSameStrand].Ext().Add(*context.NewDiscObj(cds_sf), false).Add(*context.NewDiscObj(rna_sf), false).Fatal();
        }
    }
}

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(RNA_CDS_OVERLAP, CSeq_feat_BY_BIOSEQ, eDisc | eSubmitter | eSmart, "CDS RNA Overlap")
//  ----------------------------------------------------------------------------
{
    if (!obj.GetData().IsRna() && !obj.GetData().IsCdregion()) {
        return;
    }

    // See if we have moved to the "next" Bioseq
    if (m_Count != context.GetCountBioseq()) {
        m_Count = context.GetCountBioseq();
        m_Objs["RNAs"].clear();
        m_Objs["coding regions"].clear();
        m_Objs["tRNAs"].clear();
    }

    // We ask to keep the reference because we do need the actual object to stick around so we can deal with them later.
    CRef<CDiscrepancyObject> this_disc_obj(context.NewDiscObj(CConstRef<CSeq_feat>(&obj), eKeepRef));
    const CSeq_loc& this_location = obj.GetLocation();

    if (obj.GetData().IsCdregion()) {
        CConstRef<CSeq_feat> cds_sf(&obj);
        NON_CONST_ITERATE (TReportObjectList, robj, m_Objs["RNAs"].GetObjects()) {
            const CDiscrepancyObject* other_disc_obj = dynamic_cast<CDiscrepancyObject*>(robj->GetNCPointer());
            CConstRef<CSeq_feat> rna_sf(dynamic_cast<const CSeq_feat*>(other_disc_obj->GetObject().GetPointer()));
            if (rna_sf) {
                ProcessCDSRNAPair(cds_sf, rna_sf, m_Objs, context);
            }
        }
        NON_CONST_ITERATE (TReportObjectList, robj, m_Objs["tRNAs"].GetObjects()) {
            const CDiscrepancyObject* other_disc_obj = dynamic_cast<CDiscrepancyObject*>(robj->GetNCPointer());
            CConstRef<CSeq_feat> rna_sf(dynamic_cast<const CSeq_feat*>(other_disc_obj->GetObject().GetPointer()));
            if (rna_sf) {
                ProcessCDSRNAPair(cds_sf, rna_sf, m_Objs, context);
            }
        }
        m_Objs["coding regions"].Add(*this_disc_obj);
    } else if (obj.GetData().IsRna()) {
        bool do_check = false;
        CSeqFeatData::ESubtype subtype = obj.GetData().GetSubtype();
        if (subtype == CSeqFeatData::eSubtype_tRNA) {
            if (!context.IsEukaryotic()) {
                do_check = true;
            }
        } else if (subtype == CSeqFeatData::eSubtype_mRNA || subtype == CSeqFeatData::eSubtype_ncRNA) {
            //always ignore these
        } else if (subtype == CSeqFeatData::eSubtype_rRNA) {
            // check to see if these are "short"
            do_check = !IsShortrRNA(obj, &(context.GetScope()));
        } else {
            do_check = true;
        }
        if (do_check) {
            CConstRef<CSeq_feat> rna_sf(&obj);
            NON_CONST_ITERATE (TReportObjectList, cobj, m_Objs["coding regions"].GetObjects()) {
                const CDiscrepancyObject* other_disc_obj = dynamic_cast<CDiscrepancyObject*>(cobj->GetNCPointer());
                CConstRef<CSeq_feat> cds_sf(dynamic_cast<const CSeq_feat*>(other_disc_obj->GetObject().GetPointer()));
                if (cds_sf) {
                    ProcessCDSRNAPair(cds_sf, rna_sf, m_Objs, context);
                }
            }
            if (subtype == CSeqFeatData::eSubtype_tRNA) {
                m_Objs["tRNAs"].Add(*this_disc_obj);
            } else {
                m_Objs["RNAs"].Add(*this_disc_obj);
            }
        }
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(RNA_CDS_OVERLAP)
//  ----------------------------------------------------------------------------
{
    m_Objs.GetMap().erase("RNAs");
    m_Objs.GetMap().erase("coding regions");
    m_Objs.GetMap().erase("tRNAs");

    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


DISCREPANCY_CASE(OVERLAPPING_RRNAS, COverlappingFeatures, eDisc | eSubmitter | eSmart, "Overlapping rRNAs")
{
    const vector<CConstRef<CSeq_feat> >& rrnas = context.FeatRRNAs();
    for (size_t i = 0; i < rrnas.size(); i++) {
        const CSeq_loc& loc_i = rrnas[i]->GetLocation();
        for (size_t j = i + 1; j < rrnas.size(); j++) {
            const CSeq_loc& loc_j = rrnas[j]->GetLocation();
            if (sequence::Compare(loc_j, loc_i, &context.GetScope(), sequence::fCompareOverlapping) != sequence::eNoOverlap) {
                m_Objs["[n] rRNA feature[s] overlap[S] another rRNA feature."].Add(*context.NewDiscObj(rrnas[i])).Add(*context.NewDiscObj(rrnas[j])).Fatal();
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(OVERLAPPING_RRNAS)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// OVERLAPPING_GENES

DISCREPANCY_CASE(OVERLAPPING_GENES, COverlappingFeatures, eDisc, "Overlapping Genes")
{
    const vector<CConstRef<CSeq_feat> >& genes = context.FeatGenes();
    for (size_t i = 0; i < genes.size(); i++) {
        const CSeq_loc& loc_i = genes[i]->GetLocation();
        for (size_t j = i + 1; j < genes.size(); j++) {
            const CSeq_loc& loc_j = genes[j]->GetLocation();
            if (sequence::Compare(loc_j, loc_i, &context.GetScope(), sequence::fCompareOverlapping) != sequence::eNoOverlap) {
                m_Objs["[n] gene[s] overlap[S] another gene."].Add(*context.NewDiscObj(genes[i])).Add(*context.NewDiscObj(genes[j]));
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(OVERLAPPING_GENES)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// FIND_OVERLAPPED_GENES

DISCREPANCY_CASE(FIND_OVERLAPPED_GENES, COverlappingFeatures, eDisc | eSmart, "Genes completely contained by another gene on the same strand")
{
    const vector<CConstRef<CSeq_feat> >& genes = context.FeatGenes();
    for (size_t i = 0; i < genes.size(); i++) {
        const CSeq_loc& loc_i = genes[i]->GetLocation();
        for (size_t j = i + 1; j < genes.size(); j++) {
            const CSeq_loc& loc_j = genes[j]->GetLocation();
            sequence::ECompare ovlp = sequence::Compare(loc_i, loc_j, &context.GetScope(), sequence::fCompareOverlapping);
            if (ovlp == sequence::eContained || ovlp == sequence::eSame) {
                m_Objs["[n] gene[s] completely overlapped by other genes"].Add(*context.NewDiscObj(genes[i]));
            }
            else if (ovlp == sequence::eContains) {
                m_Objs["[n] gene[s] completely overlapped by other genes"].Add(*context.NewDiscObj(genes[j]));
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(FIND_OVERLAPPED_GENES)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// DUP_GENES_OPPOSITE_STRANDS

DISCREPANCY_CASE(DUP_GENES_OPPOSITE_STRANDS, COverlappingFeatures, eDisc | eOncaller | eSubmitter | eSmart, "Genes match other genes in the same location, but on the opposite strand")
{
    const vector<CConstRef<CSeq_feat> >& genes = context.FeatGenes();
    for (size_t i = 0; i < genes.size(); i++) {
        const CSeq_loc& loc_i = genes[i]->GetLocation();
        for (size_t j = i + 1; j < genes.size(); j++) {
            const CSeq_loc& loc_j = genes[j]->GetLocation();
            if (loc_i.GetStrand() == loc_j.GetStrand()) {
                continue;
            }
            sequence::ECompare ovlp = sequence::Compare(loc_i, loc_j, &context.GetScope(), sequence::fCompareOverlapping);
            if (ovlp == sequence::eSame) {
                m_Objs["[n] genes match other genes in the same location, but on the opposite strand"].Add(*context.NewDiscObj(genes[i])).Add(*context.NewDiscObj(genes[j]));
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(DUP_GENES_OPPOSITE_STRANDS)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// MRNA_OVERLAPPING_PSEUDO_GENE

DISCREPANCY_CASE(MRNA_OVERLAPPING_PSEUDO_GENE, COverlappingFeatures, eOncaller, "mRNA overlapping pseudo gene")
{
    const vector<CConstRef<CSeq_feat> >& pseudo = context.FeatPseudo();
    const vector<CConstRef<CSeq_feat> >& mrnas = context.FeatMRNAs();
    for (size_t i = 0; i < pseudo.size(); i++) {
        const CSeq_loc& loc_i = pseudo[i]->GetLocation();
        for (size_t j = 0; j < mrnas.size(); j++) {
            const CSeq_loc& loc_j = mrnas[j]->GetLocation();
            sequence::ECompare ovlp = sequence::Compare(loc_i, loc_j, &context.GetScope(), sequence::fCompareOverlapping);
            if (ovlp != sequence::eNoOverlap) {
                m_Objs["[n] Pseudogene[s] [has] overlapping mRNA[s]."].Add(*context.NewDiscObj(pseudo[i]));
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(MRNA_OVERLAPPING_PSEUDO_GENE)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
