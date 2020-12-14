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

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

DISCREPANCY_MODULE(gene_names);


// BAD_GENE_NAME
static bool HasBadWord(const string& s, string& word)
{
    static const char* BadWords[] = { "putative", "fragment", "gene", "orf", "like" };
    for (size_t i = 0; i < ArraySize(BadWords); i++) {
        if (NStr::FindNoCase(s, BadWords[i]) != string::npos) {
            word = BadWords[i];
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


DISCREPANCY_CASE(BAD_GENE_NAME, FEAT, eDisc | eSubmitter | eSmart, "Bad gene name")
{
    for (auto& feat : context.GetFeat()) {
        if (feat.IsSetData() && feat.GetData().IsGene() && feat.GetData().GetGene().CanGetLocus()) {
            string locus = feat.GetData().GetGene().GetLocus();
            string word;
            if (locus.size() > 10 || Has4Numbers(locus) || HasBadWord(locus, word)) {
                m_Objs[word.empty() ? "[n] gene[s] contain[S] suspect phrase or characters" : "[n] gene[s] contain[S] [(]" + word].Add(*context.SeqFeatObjRef(feat, &feat));
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(BAD_GENE_NAME)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_AUTOFIX(BAD_GENE_NAME)
{
    const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(context.FindObject(*obj));
    CRef<CSeq_feat> new_feat(new CSeq_feat());
    new_feat->Assign(*sf);
    if (sf->IsSetData() && sf->GetData().IsGene() && sf->GetData().GetGene().CanGetLocus()) {
        AddComment(*new_feat, sf->GetData().GetGene().GetLocus());
    }
    new_feat->SetData().SetGene().ResetLocus();
    context.ReplaceSeq_feat(*obj, *sf, *new_feat);
    obj->SetFixed();
    return CRef<CAutofixReport>(new CAutofixReport("BAD_GENE_NAME: [n] gene name[s] fixed", 1));
}


// BAD_BACTERIAL_GENE_NAME

DISCREPANCY_CASE(BAD_BACTERIAL_GENE_NAME, FEAT, eDisc | eOncaller | eSubmitter | eSmart, "Bad bacterial gene name")
{
    const CSeqdesc* biosrc = context.GetBiosource();
    if (biosrc) {
        const CBioSource* src = &biosrc->GetSource();
        if ((src->IsSetLineage() || !context.GetLineage().empty()) && !context.HasLineage(src, "Eukaryota") && !context.IsViral(src)) {
            for (auto& feat : context.GetFeat()) {
                if (feat.IsSetData() && feat.GetData().IsGene() && feat.GetData().GetGene().CanGetLocus()) {
                    string locus = feat.GetData().GetGene().GetLocus();
                    if (!isalpha(locus[0]) || !islower(locus[0])) {
                        m_Objs["[n] bacterial gene[s] [does] not start with lowercase letter"].Add(*context.SeqFeatObjRef(feat, &feat));
                    }
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(BAD_BACTERIAL_GENE_NAME)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_AUTOFIX(BAD_BACTERIAL_GENE_NAME)
{
    const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(context.FindObject(*obj));
    CRef<CSeq_feat> new_feat(new CSeq_feat());
    new_feat->Assign(*sf);
    AddComment(*new_feat, sf->GetData().GetGene().GetLocus());
    new_feat->SetData().SetGene().ResetLocus();
    context.ReplaceSeq_feat(*obj, *sf, *new_feat);
    obj->SetFixed();
    return CRef<CAutofixReport>(new CAutofixReport("BAD_BACTERIAL_GENE_NAME: [n] bacterial gene name[s] fixed", 1));
}


// EC_NUMBER_ON_UNKNOWN_PROTEIN

DISCREPANCY_CASE(EC_NUMBER_ON_UNKNOWN_PROTEIN, FEAT, eDisc | eSubmitter | eSmart | eFatal, "EC number on unknown protein")
{
    for (auto& feat : context.GetFeat()) {
        if (feat.IsSetData() && feat.GetData().IsProt() && feat.GetData().GetProt().CanGetName() && feat.GetData().GetProt().CanGetEc() && !feat.GetData().GetProt().GetEc().empty()) {
            const list <string>& names = feat.GetData().GetProt().GetName();
            if (!names.empty()) {
                string str = *names.begin();
                NStr::ToLower(str);
                //if (NStr::FindNoCase(*names.begin(), "hypothetical protein") != string::npos || NStr::FindNoCase(*names.begin(), "unknown protein") != string::npos) {
                if (str == "hypothetical protein" || str == "unknown protein") {
                    m_Objs["[n] protein feature[s] [has] an EC number and a protein name of 'unknown protein' or 'hypothetical protein'"].Add(*context.SeqFeatObjRef(feat, &feat)).Fatal();
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(EC_NUMBER_ON_UNKNOWN_PROTEIN)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_AUTOFIX(EC_NUMBER_ON_UNKNOWN_PROTEIN)
{
    const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(context.FindObject(*obj));
    CRef<CSeq_feat> new_feat(new CSeq_feat());
    new_feat->Assign(*sf);
    new_feat->SetData().SetProt().ResetEc();
    context.ReplaceSeq_feat(*obj, *sf, *new_feat);
    obj->SetFixed();
    return CRef<CAutofixReport>(new CAutofixReport("EC_NUMBER_ON_UNKNOWN_PROTEIN: removed [n] EC number[s] from unknown protein[s]", 1));
}


// SHOW_HYPOTHETICAL_CDS_HAVING_GENE_NAME

DISCREPANCY_CASE(SHOW_HYPOTHETICAL_CDS_HAVING_GENE_NAME, FEAT, eDisc | eSubmitter | eSmart | eFatal, "Hypothetical CDS with gene names")
{
    for (auto& feat : context.GetFeat()) {
        if (feat.IsSetData() && feat.GetData().IsCdregion() && feat.CanGetProduct()) {
            const CSeq_feat* gene = context.GetGeneForFeature(feat);
            if (gene && gene->GetData().GetGene().CanGetLocus() && !gene->GetData().GetGene().GetLocus().empty()) {
                CBioseq_Handle bioseq = sequence::GetBioseqFromSeqLoc(feat.GetProduct(), context.GetScope());
                if (bioseq) {
                    CFeat_CI feat_it(bioseq, CSeqFeatData::e_Prot); // consider different implementation
                    if (feat_it) {
                        const CProt_ref& prot = feat_it->GetOriginalFeature().GetData().GetProt();
                        if (prot.CanGetName()) {
                            auto& names = prot.GetName();
                            if (!names.empty() && NStr::FindNoCase(names.front(), "hypothetical protein") != string::npos) {
                                m_Objs["[n] hypothetical coding region[s] [has] a gene name"].Fatal().Add(*context.SeqFeatObjRef(feat, gene));
                            }
                        }
                    }
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(SHOW_HYPOTHETICAL_CDS_HAVING_GENE_NAME)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_AUTOFIX(SHOW_HYPOTHETICAL_CDS_HAVING_GENE_NAME)
{
    const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(context.FindObject(*obj, true));
    CRef<CSeq_feat> new_feat(new CSeq_feat());
    new_feat->Assign(*sf);
    AddComment(*new_feat, sf->GetData().GetGene().IsSetLocus() ? sf->GetData().GetGene().GetLocus() : kEmptyStr);
    new_feat->SetData().SetGene().ResetLocus();
    context.ReplaceSeq_feat(*obj, *sf, *new_feat, true);
    obj->SetFixed();
    return CRef<CAutofixReport>(new CAutofixReport("SHOW_HYPOTHETICAL_CDS_HAVING_GENE_NAME: [n] hypothetical CDS fixed", 1));
}


// DUPLICATE_LOCUS_TAGS
const string kDuplicateLocusTagsTop = "[n] gene[s] [has] duplicate locus tags";
const string kDuplicateLocusTagsStart = "[n] gene[s] [has] locus tag ";
const string kDuplicateAdjacent = "[n] gene[s] [is] adjacent to another gene with the same locus tag.";


DISCREPANCY_CASE(DUPLICATE_LOCUS_TAGS, SEQUENCE, eDisc | eOncaller | eSubmitter | eSmart, "Duplicate Locus Tags")
{
    auto& genes = context.FeatGenes();
    string last_locus_tag = kEmptyStr;
    CRef<CDiscrepancyObject> last_disc_obj;
    for (auto gene: genes) {
        if (gene->GetData().GetGene().IsSetLocus_tag()) {
            CRef<CDiscrepancyObject> this_disc_obj(context.SeqFeatObjRef(*gene));
            const string& this_locus_tag = gene->GetData().GetGene().GetLocus_tag();
            m_Objs[kEmptyStr][this_locus_tag].Add(*this_disc_obj);
            if (last_disc_obj && last_locus_tag == this_locus_tag) {
                m_Objs[kDuplicateLocusTagsTop][kDuplicateAdjacent].Add(*last_disc_obj);
                m_Objs[kDuplicateLocusTagsTop][kDuplicateAdjacent].Add(*this_disc_obj);
            }
            last_locus_tag = this_locus_tag;
            last_disc_obj = this_disc_obj;
        }
        else {
            last_locus_tag = kEmptyStr;
        }
    }
}


DISCREPANCY_SUMMARIZE(DUPLICATE_LOCUS_TAGS)
{
    for (auto it: m_Objs[kEmptyStr].GetMap()) {
        if (it.second->GetObjects().size() > 1) {
            string label = kDuplicateLocusTagsStart + it.first + ".";
            for (auto obj: it.second->GetObjects()) {
                m_Objs[kDuplicateLocusTagsTop][label].Add(*obj);
            }
        }
    }
    m_Objs.GetMap().erase(kEmptyStr);
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
