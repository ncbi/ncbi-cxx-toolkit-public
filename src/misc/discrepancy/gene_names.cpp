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
#include "utils.hpp"
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE;
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

DISCREPANCY_MODULE(gene_names);


// BAD_GENE_NAME
static bool HasBadWord(const string& s)
{
    static const string BadWords[] = { "putative", "fragment", "gene", "orf", "like" };
    for (size_t i = 0; i < ArraySize(BadWords); i++) {
        if (NStr::FindNoCase(s, BadWords[i]) != string::npos) {
            return true;
        }
    }
    return false;
}


static bool Has4Numbers(const string& s)
{
    size_t n = 0;
    for (size_t i = 0; i < s.size() && n < 4; i++) {
        n = isdigit(s[i]) ? n+1 : 0;
    }
    return n >= 4;
};


DISCREPANCY_CASE(BAD_GENE_NAME, CSeqFeatData, eDisc, "Bad gene name")
{
    if (!obj.IsGene() || !obj.GetGene().CanGetLocus()) {
        return;
    }
    string locus = obj.GetGene().GetLocus();
    if (locus.size() > 10 || Has4Numbers(locus) || HasBadWord(locus)) {
        m_Objs["[n] gene[s] contain[S] suspect phrase or characters"].Add(*new CDiscrepancyObject(context.GetCurrentSeq_feat(), context.GetScope(), context.GetFile(), context.GetKeepRef(), true));
    }
}


DISCREPANCY_SUMMARIZE(BAD_GENE_NAME)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


static void MoveLocusToNote(const CSeq_feat* sf, CScope& scope)
{
    CRef<CSeq_feat> new_feat(new CSeq_feat());
    new_feat->Assign(*sf);
    AddComment(*new_feat, sf->GetData().GetGene().GetLocus());
    new_feat->SetData().SetGene().ResetLocus();
    CSeq_feat_EditHandle feh(scope.GetSeq_featHandle(*sf));
    feh.Replace(*new_feat);
}


DISCREPANCY_AUTOFIX(BAD_GENE_NAME)
{
    TReportObjectList list = item->GetDetails();
    NON_CONST_ITERATE(TReportObjectList, it, list) {
        const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer())->GetObject().GetPointer());
        if (sf) {
            MoveLocusToNote(sf, scope);
        }
    }
}


// BAD_BACTERIAL_GENE_NAME
DISCREPANCY_CASE(BAD_BACTERIAL_GENE_NAME, CSeqFeatData, eDisc, "Bad bacterial gene name")
{
    if (!obj.IsGene() || !obj.GetGene().CanGetLocus() || !context.IsBacterial()) {
        return;
    }
    string locus = obj.GetGene().GetLocus();
    if (!isalpha(locus[0]) || !islower(locus[0])) {
        m_Objs["[n] bacterial gene[s] [does] not start with lowercase letter"].Add(*new CDiscrepancyObject(context.GetCurrentSeq_feat(), context.GetScope(), context.GetFile(), context.GetKeepRef(), true)); // maybe false
    }
}


DISCREPANCY_SUMMARIZE(BAD_BACTERIAL_GENE_NAME)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_AUTOFIX(BAD_BACTERIAL_GENE_NAME)
{
    TReportObjectList list = item->GetDetails();
    NON_CONST_ITERATE(TReportObjectList, it, list) {
        const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer())->GetObject().GetPointer());
        if (sf) {
            MoveLocusToNote(sf, scope);
        }
    }
}


// EC_NUMBER_ON_UNKNOWN_PROTEIN
DISCREPANCY_CASE(EC_NUMBER_ON_UNKNOWN_PROTEIN, CSeqFeatData, eDisc, "EC number on unknown protein")
{
    if (!obj.IsProt() || !obj.GetProt().CanGetName() || !obj.GetProt().CanGetEc() || obj.GetProt().GetEc().empty()) {
        return;
    }
    const list <string>& names = obj.GetProt().GetName();
    if (names.empty()) {
        return;
    }
    string str = *names.begin();
    NStr::ToLower(str);
    //if (NStr::FindNoCase(*names.begin(), "hypothetical protein") != string::npos || NStr::FindNoCase(*names.begin(), "unknown protein") != string::npos) {
    if (str == "hypothetical protein" || str == "unknown protein") {
        m_Objs["[n] protein feature[s] [has] an EC number and a protein name of 'unknown/hypothetical protein'"].Add(*new CDiscrepancyObject(context.GetCurrentSeq_feat(), context.GetScope(), context.GetFile(), context.GetKeepRef(), true));
    }
}


DISCREPANCY_SUMMARIZE(EC_NUMBER_ON_UNKNOWN_PROTEIN)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_AUTOFIX(EC_NUMBER_ON_UNKNOWN_PROTEIN)
{
    TReportObjectList list = item->GetDetails();
    NON_CONST_ITERATE(TReportObjectList, it, list) {
        const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer())->GetObject().GetPointer());
        if (sf) {
            CRef<CSeq_feat> new_feat(new CSeq_feat());
            new_feat->Assign(*sf);
            new_feat->SetData().SetProt().ResetEc();
            CSeq_feat_EditHandle feh(scope.GetSeq_featHandle(*sf));
            feh.Replace(*new_feat);
        }
    }
}


// SHOW_HYPOTHETICAL_CDS_HAVING_GENE_NAME
DISCREPANCY_CASE(SHOW_HYPOTHETICAL_CDS_HAVING_GENE_NAME, CSeqFeatData, eDisc, "Hypothetical CDS with gene names")
{
    if (!obj.IsCdregion() || !context.GetCurrentSeq_feat()->CanGetProduct()) {
        return;
    }
    CConstRef<CSeq_feat> gene = sequence::GetBestGeneForCds(*context.GetCurrentSeq_feat(), context.GetScope());
    if (gene.IsNull() || !gene->GetData().GetGene().CanGetLocus() || gene->GetData().GetGene().GetLocus().empty()) {
        return;
    }
    CBioseq_Handle bioseq = sequence::GetBioseqFromSeqLoc(context.GetCurrentSeq_feat()->GetProduct(), context.GetScope());
    if (!bioseq) {
        return;
    }
    CFeat_CI feat_it(bioseq, CSeqFeatData :: e_Prot);
    if (!feat_it) {
        return;
    }
    const CProt_ref& prot = feat_it->GetOriginalFeature().GetData().GetProt();
    if (!prot.CanGetName()) {
        return;
    }
    ITERATE(list <string>, jt, prot.GetName()) {
        if (NStr::FindNoCase(*jt, "hypothetical protein") != string::npos) {
            m_Objs["[n] hypothetical coding region[s] [has] a gene name"].Add(*new CDiscrepancyObject(context.GetCurrentSeq_feat(), context.GetScope(), context.GetFile(), context.GetKeepRef(), true, (CObject*)&*gene));
            break;
        }
    }
}


DISCREPANCY_SUMMARIZE(SHOW_HYPOTHETICAL_CDS_HAVING_GENE_NAME)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_AUTOFIX(SHOW_HYPOTHETICAL_CDS_HAVING_GENE_NAME)
{
    TReportObjectList list = item->GetDetails();
    NON_CONST_ITERATE(TReportObjectList, it, list) {
        CDiscrepancyObject& obj = *dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer());
        const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(obj.GetMoreInfo().GetPointer());
        if (sf) {
            MoveLocusToNote(sf, scope);
        }
    }
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
