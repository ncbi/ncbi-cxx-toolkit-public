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
#include <objects/seqfeat/Code_break.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

DISCREPANCY_MODULE(overlapping_features);

// CDS_TRNA_OVERLAP

static const string kCDSoverlapTRNA = "[n] Bioseq[s] [has] coding regions that overlap tRNAs";

DISCREPANCY_CASE(CDS_TRNA_OVERLAP, SEQUENCE, eDisc | eSubmitter | eSmart, "CDS tRNA Overlap")
{
    const auto& cds = context.FeatCDS();
    const auto& trnas = context.FeatTRNAs();

    bool increase_count = false;
    static size_t bs_count = 0;
    string report_item_str = "[n] coding region[s] [has] overlapping tRNAs[*" + to_string(++bs_count) + "*]";

    for (size_t i = 0; i < cds.size(); i++) {
        const CSeq_loc& loc_i = cds[i]->GetLocation();
        bool has_overlap = false;
        string cur_report_cds_trna_pair_str = "Coding region overlaps tRNAs[*" + to_string(i) + "*]";
        ENa_strand cds_strand = loc_i.IsSetStrand() ? loc_i.GetStrand() : eNa_strand_unknown;
        for (size_t j = 0; j < trnas.size(); j++) {
            const CSeq_loc& loc_j = trnas[j]->GetLocation();
            sequence::ECompare ovlp = sequence::eNoOverlap;
            ENa_strand trna_strand = loc_j.IsSetStrand() ? loc_j.GetStrand() : eNa_strand_unknown;
            bool need_to_compare = (cds_strand == eNa_strand_minus) == (trna_strand == eNa_strand_minus);
            if (need_to_compare) {
                ovlp = context.Compare(loc_i, loc_j);
            }
            if (ovlp != sequence::eNoOverlap) {
                increase_count = true;
                CReportNode& out = m_Objs[kCDSoverlapTRNA][report_item_str].Ext();
                CReportNode& subitem = out[cur_report_cds_trna_pair_str];
                if (!has_overlap) {
                    out.Incr();
                    has_overlap = true;
                    subitem.Ext().Add(*context.SeqFeatObjRef(*cds[i]));
                }
                subitem.Ext().Add(*context.SeqFeatObjRef(*trnas[j]));
            }
        }
    }
    if (increase_count) {
        m_Objs[kCDSoverlapTRNA].Incr();
    }
}


DISCREPANCY_SUMMARIZE(CDS_TRNA_OVERLAP)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


static const string kCdsTrnaOverlapComment = "TAA stop codon is completed by the addition of 3' A residues to the mRNA";

DISCREPANCY_CASE(_CDS_TRNA_OVERLAP, SEQUENCE, 0, "CDS tRNA Overlap - autofix") {} // LCOV_EXCL_LINE
DISCREPANCY_SUMMARIZE(_CDS_TRNA_OVERLAP) {} // LCOV_EXCL_LINE

DISCREPANCY_AUTOFIX(_CDS_TRNA_OVERLAP)
{
    const CSeq_feat& cds = dynamic_cast<const CSeq_feat&>(*context.FindObject(*obj));
    const CSeq_loc& loc = cds.GetLocation();
    CBioseq_Handle bsh = context.GetScope().GetBioseqHandle(loc);
    CFeat_CI f(bsh, CSeqFeatData::e_Rna);
    ENa_strand cds_strand = loc.IsSetStrand() ? loc.GetStrand() : eNa_strand_unknown;
    CSeq_loc::TRange r1 = loc.GetTotalRange();
    CConstRef<CSeq_loc> other;
    int ovlp_len = 0;
    while (f) {
        if (f->GetData().GetSubtype() == CSeqFeatData::eSubtype_tRNA) {
            const CSeq_loc& loc_t = f->GetLocation();
            ENa_strand trna_strand = loc_t.IsSetStrand() ? loc_t.GetStrand() : eNa_strand_unknown;
            if ((cds_strand == eNa_strand_minus) == (trna_strand == eNa_strand_minus)) {
                CSeq_loc::TRange r2 = loc_t.GetTotalRange();
                if (!(r1.GetFrom() >= r2.GetToOpen() || r2.GetFrom() >= r1.GetToOpen())) {
                    ovlp_len = r1.GetToOpen() - r2.GetFrom();
                    if (ovlp_len > 0 && ovlp_len < 3 && cds_strand != eNa_strand_minus) {
                        other.Reset(&loc_t);
                        break;
                    }
                    else {
                        ovlp_len = r2.GetToOpen() - r1.GetFrom();
                        if (ovlp_len > 0 && ovlp_len < 3 && cds_strand == eNa_strand_minus) {
                            other.Reset(&loc_t);
                            break;
                        }
                        else {
                            ovlp_len = 0;
                        }
                    }
                }
            }
        }
        ++f;
    }
    if (ovlp_len) {
        CConstRef<CSeq_feat> gene = sequence::GetGeneForFeature(cds, context.GetScope());
        CRef<CSeq_feat> new_cds(new CSeq_feat());
        new_cds->Assign(cds);
        new_cds->SetLocation().Assign(*sequence::Seq_loc_Subtract(new_cds->GetLocation(), *other, CSeq_loc::fStrand_Ignore, &context.GetScope()));
        CRef<CCode_break> code_break(new CCode_break);
        CRef<CSeq_loc> br_loc(new CSeq_loc);
        br_loc->Assign(new_cds->GetLocation());
        if (!br_loc->GetId()) {
            return CRef<CAutofixReport>(new CAutofixReport("CDS_TRNA_OVERLAP: cannot trim [n] CDS", 1));
        }
        CSeq_loc::TRange rr = br_loc->GetTotalRange();
        CRef<CSeq_id> seq_id(new CSeq_id);
        seq_id->Assign(*br_loc->GetId());
        if (br_loc->GetStrand() == eNa_strand_minus) {
            br_loc->SetInt().SetId(*seq_id);
            br_loc->SetInt().SetFrom(rr.GetFrom());
            br_loc->SetInt().SetTo(rr.GetFrom() + 2 - ovlp_len);
        }
        else {
            br_loc->SetInt().SetId(*seq_id);
            br_loc->SetInt().SetFrom(rr.GetTo() - 2 + ovlp_len);
            br_loc->SetInt().SetTo(rr.GetTo());
        }
        br_loc->SetPartialStart(false, eExtreme_Positional);
        br_loc->SetPartialStop(false, eExtreme_Positional);
        code_break->SetLoc().Assign(*br_loc);
        code_break->SetAa().SetNcbieaa('*');
        string comment;
        if (new_cds->CanGetComment()) {
            comment = new_cds->GetComment();
        }
        if (comment.find(kCdsTrnaOverlapComment) == string::npos) {
            if (comment.length()) {
                comment += "; ";
            }
            comment += kCdsTrnaOverlapComment;
        }
        new_cds->SetComment(comment);
        new_cds->SetData().SetCdregion().SetCode_break().push_back(code_break);
        context.ReplaceSeq_feat(*obj, cds, *new_cds);
        //CSeq_feat_EditHandle feh(context.GetScope().GetSeq_featHandle(cds));
        //feh.Replace(*new_cds);

        if (gene) {
            CRef<CSeq_feat> new_gene(new CSeq_feat());
            new_gene->Assign(*gene);
            new_gene->SetLocation().Assign(*sequence::Seq_loc_Subtract(new_gene->GetLocation(), *other, CSeq_loc::fStrand_Ignore, &context.GetScope()));
            CSeq_feat_EditHandle feh(context.GetScope().GetSeq_featHandle(*gene));
            feh.Replace(*new_gene);
        }
        return CRef<CAutofixReport>(new CAutofixReport("CDS_TRNA_OVERLAP: [n] CDS trimmed", 1));
    }
    return CRef<CAutofixReport>();
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
    { "28S", { 3300, false } },
    { "small", { 1000, false } },
    { "large", { 1000, false } },
    { "5.8S", { 130, true } },
    { "5S", { 90, true } } 
    // possible problem: if it matches /25S/ it would also match /5S/
    // luckily, if it fails the /5S/ rule it would fail the /25S/ rule
};


bool IsShortrRNA(const CSeq_feat& f, CScope* scope) // used in feature_tests.cpp
{
    if (f.GetData().GetSubtype() != CSeqFeatData::eSubtype_rRNA) {
        return false;
    }
    bool is_bad = false;
    size_t len = sequence::GetLength(f.GetLocation(), scope);
    string rrna_name = f.GetData().GetRna().GetRnaProductName();
    for (const auto& it : kTrnaLengthMap) {
        SIZE_TYPE pos = NStr::FindNoCase(rrna_name, it.first);
        if (pos != string::npos && len < it.second.first && !(it.second.second && f.IsSetPartial() && f.GetPartial()) ) {
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


DISCREPANCY_CASE(RNA_CDS_OVERLAP, SEQUENCE, eDisc | eSubmitter | eSmart | eFatal, "CDS RNA Overlap")
{
    const CSeqdesc* biosrc = context.GetBiosource();
    bool is_eukariotic = context.IsEukaryotic(biosrc ? &biosrc->GetSource() : nullptr);

    const auto& cds = context.FeatCDS();
    const auto& rnas = context.Feat_RNAs();
    for (size_t i = 0; i < rnas.size(); i++) {
        const CSeq_loc& loc_i = rnas[i]->GetLocation();
        CSeqFeatData::ESubtype subtype = rnas[i]->GetData().GetSubtype();
        if ((subtype == CSeqFeatData::eSubtype_tRNA && is_eukariotic) || subtype == CSeqFeatData::eSubtype_mRNA || subtype == CSeqFeatData::eSubtype_ncRNA) {
            continue;
        }
        else if (subtype == CSeqFeatData::eSubtype_rRNA) {
            size_t len = sequence::GetLength(loc_i, &context.GetScope());
            string rrna_name = rnas[i]->GetData().GetRna().GetRnaProductName();
            bool is_bad = false;
            for (const auto& it : kTrnaLengthMap) {
                if (NStr::FindNoCase(rrna_name, it.first) != string::npos && len < it.second.first && (!it.second.second || (rnas[i]->IsSetPartial() && rnas[i]->GetPartial())) ) {
                    is_bad = true;
                    break;
                }
            }
            if (is_bad) {
                continue;
            }
        }
        for (size_t j = 0; j < cds.size(); j++) {
            const CSeq_loc& loc_j = cds[j]->GetLocation();
            sequence::ECompare compare = context.Compare(loc_j, loc_i);
            if (compare == sequence::eSame) {
                m_Objs[kCDSRNAAnyOverlap][kCDSRNAExactMatch].Add(*context.SeqFeatObjRef(*rnas[i]), false).Add(*context.SeqFeatObjRef(*cds[j]), false).Fatal();
            }
            else if (compare == sequence::eContained) {
                m_Objs[kCDSRNAAnyOverlap][kCDSRNAContainedIn].Add(*context.SeqFeatObjRef(*rnas[i]), false).Add(*context.SeqFeatObjRef(*cds[j]), false); // no Fatal();
            }
            else if (compare == sequence::eContains) {
                if (rnas[i]->GetData().GetSubtype() == CSeqFeatData::eSubtype_tRNA) {
                    m_Objs[kCDSRNAAnyOverlap][kCDSRNAContainstRNA].Add(*context.SeqFeatObjRef(*rnas[i]), false).Add(*context.SeqFeatObjRef(*cds[j]), false).Fatal();
                }
                else {
                    m_Objs[kCDSRNAAnyOverlap][kCDSRNAContains].Add(*context.SeqFeatObjRef(*rnas[i]), false).Add(*context.SeqFeatObjRef(*cds[j]), false).Fatal();
                }
            }
            else if (compare != sequence::eNoOverlap) {
                ENa_strand cds_strand = cds[j]->GetLocation().GetStrand();
                ENa_strand rna_strand = rnas[i]->GetLocation().GetStrand();
                if (cds_strand == eNa_strand_minus && rna_strand != eNa_strand_minus) {
                    m_Objs[kCDSRNAAnyOverlap][kCDSRNAOverlapNoContain][kCDSRNAOverlapNoContainOppStrand].Add(*context.SeqFeatObjRef(*rnas[i]), false).Add(*context.SeqFeatObjRef(*cds[j]), false); // no Fatal();
                }
                else if (cds_strand != eNa_strand_minus && rna_strand == eNa_strand_minus) {
                    m_Objs[kCDSRNAAnyOverlap][kCDSRNAOverlapNoContain][kCDSRNAOverlapNoContainOppStrand].Add(*context.SeqFeatObjRef(*rnas[i]), false).Add(*context.SeqFeatObjRef(*cds[j]), false); // no Fatal();
                }
                else {
                    m_Objs[kCDSRNAAnyOverlap][kCDSRNAOverlapNoContain][kCDSRNAOverlapNoContainSameStrand].Add(*context.SeqFeatObjRef(*rnas[i]), false).Add(*context.SeqFeatObjRef(*cds[j]), false); // no Fatal();
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(RNA_CDS_OVERLAP)
{
    m_ReportItems = m_Objs.Export(*this, false)->GetSubitems();
}


DISCREPANCY_CASE(OVERLAPPING_RRNAS, SEQUENCE, eDisc | eSubmitter | eSmart | eFatal, "Overlapping rRNAs")
{
    const auto& rrnas = context.FeatRRNAs();
    for (size_t i = 0; i < rrnas.size(); i++) {
        const CSeq_loc& loc_i = rrnas[i]->GetLocation();
        for (size_t j = i + 1; j < rrnas.size(); j++) {
            const CSeq_loc& loc_j = rrnas[j]->GetLocation();
            if (context.Compare(loc_j, loc_i) != sequence::eNoOverlap) {
                m_Objs["[n] rRNA feature[s] overlap[S] another rRNA feature."].Add(*context.SeqFeatObjRef(*rrnas[i])).Add(*context.SeqFeatObjRef(*rrnas[j])).Fatal();
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(OVERLAPPING_RRNAS)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// OVERLAPPING_GENES

DISCREPANCY_CASE(OVERLAPPING_GENES, SEQUENCE, eDisc, "Overlapping Genes")
{
    const auto& genes = context.FeatGenes();
    for (size_t i = 0; i < genes.size(); i++) {
        const CSeq_loc& loc_i = genes[i]->GetLocation();
        ENa_strand strand_i = loc_i.GetStrand();
        for (size_t j = i + 1; j < genes.size(); j++) {
            const CSeq_loc& loc_j = genes[j]->GetLocation();
            if (loc_j.GetStrand() == strand_i && context.Compare(loc_j, loc_i) != sequence::eNoOverlap) {
                m_Objs["[n] gene[s] overlap[S] another gene on the same strand."].Add(*context.SeqFeatObjRef(*genes[i])).Add(*context.SeqFeatObjRef(*genes[j]));
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(OVERLAPPING_GENES)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// FIND_OVERLAPPED_GENES

DISCREPANCY_CASE(FIND_OVERLAPPED_GENES, SEQUENCE, eDisc | eSmart, "Genes completely contained by another gene on the same strand")
{
    const auto& genes = context.FeatGenes();
    for (size_t i = 0; i < genes.size(); i++) {
        const CSeq_loc& loc_i = genes[i]->GetLocation();
        ENa_strand strand_i = loc_i.IsSetStrand() ? loc_i.GetStrand() : eNa_strand_unknown;

        for (size_t j = i + 1; j < genes.size(); j++) {
            const CSeq_loc& loc_j = genes[j]->GetLocation();
            ENa_strand strand_j = loc_j.IsSetStrand() ? loc_j.GetStrand() : eNa_strand_unknown;

            if (strand_i == strand_j) {

                sequence::ECompare ovlp = context.Compare(loc_i, loc_j);
                if (ovlp == sequence::eContained || ovlp == sequence::eSame) {
                    m_Objs["[n] gene[s] completely overlapped by other genes"].Add(*context.SeqFeatObjRef(*genes[i]));
                }
                else if (ovlp == sequence::eContains) {
                    m_Objs["[n] gene[s] completely overlapped by other genes"].Add(*context.SeqFeatObjRef(*genes[j]));
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(FIND_OVERLAPPED_GENES)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// DUP_GENES_OPPOSITE_STRANDS

DISCREPANCY_CASE(DUP_GENES_OPPOSITE_STRANDS, SEQUENCE, eDisc | eOncaller | eSubmitter | eSmart, "Genes match other genes in the same location, but on the opposite strand")
{
    const auto& genes = context.FeatGenes();
    for (size_t i = 0; i < genes.size(); i++) {
        const CSeq_loc& loc_i = genes[i]->GetLocation();
        ENa_strand strand_i = loc_i.GetStrand();
        for (size_t j = i + 1; j < genes.size(); j++) {
            const CSeq_loc& loc_j = genes[j]->GetLocation();
            if ((loc_j.GetStrand() == eNa_strand_minus) == (strand_i == eNa_strand_minus)) {
                continue;
            }
            sequence::ECompare ovlp = context.Compare(loc_i, loc_j);
            if (ovlp == sequence::eSame) {
                m_Objs["[n] genes match other genes in the same location, but on the opposite strand"].Add(*context.SeqFeatObjRef(*genes[i])).Add(*context.SeqFeatObjRef(*genes[j]));
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(DUP_GENES_OPPOSITE_STRANDS)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// MRNA_OVERLAPPING_PSEUDO_GENE

DISCREPANCY_CASE(MRNA_OVERLAPPING_PSEUDO_GENE, SEQUENCE, eOncaller, "mRNA overlapping pseudo gene")
{
    const auto& pseudo = context.FeatPseudo();
    const auto& mrnas = context.FeatMRNAs();
    for (size_t i = 0; i < mrnas.size(); i++) {
        const CSeq_loc& loc_i = mrnas[i]->GetLocation();
        for (size_t j = 0; j < pseudo.size(); j++) {
            const CSeq_loc& loc_j = pseudo[j]->GetLocation();
            sequence::ECompare ovlp = context.Compare(loc_i, loc_j);
            if (ovlp != sequence::eNoOverlap) {
                m_Objs["[n] Pseudogene[s] [has] overlapping mRNA[s]."].Add(*context.SeqFeatObjRef(*mrnas[i], CDiscrepancyContext::eFixSet));  // should say "n mRNAs overlapping pseudogenes", but C Toolkit reports this way.
                break;
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(MRNA_OVERLAPPING_PSEUDO_GENE)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_AUTOFIX(MRNA_OVERLAPPING_PSEUDO_GENE)
{
    const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(context.FindObject(*obj));
    CSeq_feat_EditHandle eh = CSeq_feat_EditHandle(context.GetScope().GetSeq_featHandle(*sf));
    eh.Remove();
    obj->SetFixed();
    return CRef<CAutofixReport>(new CAutofixReport("MRNA_OVERLAPPING_PSEUDO_GENE: [n] mRNA[s] removed", 1));
}


// EXON_INTRON_CONFLICT

static bool less(const CSeq_feat* A, const CSeq_feat* B)
{
    unsigned int a = A->GetLocation().GetStart(eExtreme_Positional);
    unsigned int b = B->GetLocation().GetStart(eExtreme_Positional);
    if (a != b) {
        return a < b;
    }
    a = A->GetLocation().GetStop(eExtreme_Positional);
    b = B->GetLocation().GetStop(eExtreme_Positional);
    return a < b;
}

static const string kIntronExon = "[n] introns and exons are incorrectly positioned";

static void CollectExonsIntrons(CReportNode& out, CDiscrepancyContext& context, vector<const CSeq_feat*>& vex, vector<const CSeq_feat*>& vint)
{
    sort(vex.begin(), vex.end(), less);
    sort(vint.begin(), vint.end(), less);
    auto Iex = vex.cbegin();
    auto Iint = vint.cbegin();
    while (Iex != vex.cend() && Iint != vint.cend()) {
        const unsigned int e0 = (*Iex)->GetLocation().GetStart(eExtreme_Positional);
        const unsigned int e1 = (*Iex)->GetLocation().GetStop(eExtreme_Positional);
        const unsigned int i0 = (*Iint)->GetLocation().GetStart(eExtreme_Positional);
        const unsigned int i1 = (*Iint)->GetLocation().GetStop(eExtreme_Positional);
        if (i0 <= e0) {
            if (i1 != e0 - 1) {
                out[kIntronExon].Add(*context.SeqFeatObjRef(**Iint)).Add(*context.SeqFeatObjRef(**Iex));
            }
            ++Iint;
        }
        else /*if (e0 < i0)*/ {
            if (e1 != i0 - 1) {
                out[kIntronExon].Add(*context.SeqFeatObjRef(**Iex)).Add(*context.SeqFeatObjRef(**Iint));
            }
            ++Iex;
        }
    }
}


DISCREPANCY_CASE(EXON_INTRON_CONFLICT, SEQUENCE, eOncaller | eSubmitter | eSmart, "Exon and intron locations should abut (unless gene is trans-spliced)")
{
    const auto& genes = context.FeatGenes();
    const auto& exons = context.FeatExons();
    const auto& introns = context.FeatIntrons();
    if (exons.empty() || introns.empty()) {
        return;
    }
    if (genes.empty()) {
        vector<const CSeq_feat*> vex;
        vector<const CSeq_feat*> vint;
        vex.insert(vex.end(), exons.cbegin(), exons.cend());
        vint.insert(vint.end(), introns.cbegin(), introns.cend());
        CollectExonsIntrons(m_Objs, context, vex, vint);
    }
    else {
        for (const CSeq_feat* gg : genes) {
            if (gg->CanGetExcept_text() && gg->GetExcept_text() == "trans-splicing") {
                continue;
            }
            const unsigned int g0 = gg->GetLocation().GetStart(eExtreme_Positional);
            const unsigned int g1 = gg->GetLocation().GetStop(eExtreme_Positional);
            vector<const CSeq_feat*> vex;
            vector<const CSeq_feat*> vint;
            for (const CSeq_feat* ff : exons) {
                if (ff->GetLocation().GetStart(eExtreme_Positional) <= g1 && ff->GetLocation().GetStop(eExtreme_Positional) >= g0) {
                    vex.push_back(ff);
                }
            }
            for (const CSeq_feat* ff : introns) {
                if (ff->GetLocation().GetStart(eExtreme_Positional) <= g1 && ff->GetLocation().GetStop(eExtreme_Positional) >= g0) {
                    vint.push_back(ff);
                }
            }
            CollectExonsIntrons(m_Objs, context, vex, vint);
        }
    }
}


DISCREPANCY_SUMMARIZE(EXON_INTRON_CONFLICT)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// GENE_MISC_IGS_OVERLAP

static const string kGeneMisc = "[n] gene[s] overlap[S] with IGS misc features";

DISCREPANCY_CASE(GENE_MISC_IGS_OVERLAP, SEQUENCE, eOncaller, "Gene with misc feature overlap")
{
    for (const CSeq_feat* gene : context.FeatGenes()) {
        if (gene->IsSetLocation() && gene->IsSetData() && gene->GetData().GetGene().IsSetLocus() &&
            NStr::StartsWith(gene->GetData().GetGene().GetLocus(), "trn")) {

            const CSeq_loc& loc_gene = gene->GetLocation();
            bool gene_added = false;

            for (const CSeq_feat* misc : context.FeatMisc()) {
                if (misc->IsSetLocation() && misc->IsSetComment() && NStr::FindNoCase(misc->GetComment(), "intergenic spacer") != NPOS) {
                    const CSeq_loc& loc_misc = misc->GetLocation();
                    if (context.Compare(loc_gene, loc_misc) != sequence::eNoOverlap) {
                        if (!gene_added) {
                            m_Objs[kGeneMisc].Add(*context.SeqFeatObjRef(*gene)).Incr();
                            gene_added = true;
                        }
                        m_Objs[kGeneMisc].Add(*context.SeqFeatObjRef(*misc));
                    }
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(GENE_MISC_IGS_OVERLAP)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// GENE_LOCUS_MISSING

DISCREPANCY_CASE(GENE_LOCUS_MISSING, SEQUENCE, eOncaller, "Gene locus missing")
{
    const auto& genes = context.FeatGenes();
    const auto& cds = context.FeatCDS();
    const auto& mrnas = context.FeatMRNAs();
    for (const CSeq_feat* gene : genes) {
        const CGene_ref& gref = gene->GetData().GetGene();
        if (context.IsPseudo(*gene) || !gref.CanGetDesc() || gref.GetDesc().empty() || (gref.CanGetLocus() && !gref.GetLocus().empty())) {
            continue;
        }
        bool found = false;
        for (const CSeq_feat* feat : cds) {
            if (context.GetGeneForFeature(*feat) == &*gene) {
                found = true;
                break;
            }
        }
        if (!found) {
            for (const CSeq_feat* feat : mrnas) {
                if (context.GetGeneForFeature(*feat) == &*gene) {
                    found = true;
                    break;
                }
            }
        }
        if (found) {
            m_Objs["[n] gene[s] missing locus"].Add(*context.SeqFeatObjRef(*gene, gene));
        }
    }
}


DISCREPANCY_SUMMARIZE(GENE_LOCUS_MISSING)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_AUTOFIX(GENE_LOCUS_MISSING)
{
    const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(context.FindObject(*obj));
    CRef<CSeq_feat> new_feat(new CSeq_feat());
    new_feat->Assign(*sf);
    new_feat->SetData().SetGene().SetLocus(new_feat->GetData().GetGene().GetDesc());
    new_feat->SetData().SetGene().ResetDesc();
    context.ReplaceSeq_feat(*obj, *sf, *new_feat);
    obj->SetFixed();
    return CRef<CAutofixReport>(new CAutofixReport("GENE_LOCUS_MISSING: [n] gene[s] fixed", 1));
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
