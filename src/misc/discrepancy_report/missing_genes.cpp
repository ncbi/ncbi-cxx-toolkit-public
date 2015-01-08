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
 * Author:  Colleen Bollin
 *
 * File Description:
 *   class for storing tests for Discrepancy Report
 *
 * $Log:$ 
 */

#include <ncbi_pch.hpp>

#include <misc/discrepancy_report/missing_genes.hpp>

#include <objects/seqfeat/Gb_qual.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objtools/edit/gene_utils.hpp>

BEGIN_NCBI_SCOPE;

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(DiscRepNmSpc);




CMissingGenes::CMissingGenes()
{
}

void CMissingGenes::CollectData(CSeq_entry_Handle seh, const CReportMetadata& metadata)
{
    CBioseq_CI bi(seh, CSeq_inst::eMol_na);
    while (bi) {
        CollectData(*bi, metadata.GetFilename());
        ++bi;
    }
}


void CMissingGenes::CollectData(CBioseq_Handle bh, const string& filename)
{
    // first, get a list of all genes
    TGenesUsedMap genes;
    CFeat_CI fi(bh, CSeqFeatData::e_Gene);
    while (fi) {
        genes[fi->GetSeq_feat().GetPointer()] = false;
        ++fi;
    }

    // check coding regions
    x_CheckGenesForFeatureType (bh, genes, CSeqFeatData::eSubtype_cdregion, true, filename);

    // check RNA features
    x_CheckGenesForFeatureType (bh, genes, CSeqFeatData::e_Rna, true, filename);

    // check RBS features
    x_CheckGenesForFeatureType (bh, genes, CSeqFeatData::eSubtype_RBS, false, filename);

    // check exons
    x_CheckGenesForFeatureType (bh, genes, CSeqFeatData::eSubtype_exon, false, filename);

    // check introns
    x_CheckGenesForFeatureType (bh, genes, CSeqFeatData::eSubtype_intron, false, filename);
    
    ITERATE(TGenesUsedMap, g, genes) {
        if (!g->second && !x_IsOkSuperfluousGene(*(g->first))) {
            CRef<CReportObject> obj(new CReportObject(CConstRef<CObject>(g->first)));
            obj->SetFilename(filename);
            if (x_IsPseudoGene(*(g->first))) {
                m_SuperfluousPseudoGenes.push_back(obj);
            } else if (!x_GeneHasNoteOrDesc(*(g->first))) {
                m_SuperfluousNonPseudoNonFrameshiftGenes.push_back(obj);
            }
        }
    }
}


void CMissingGenes::x_CheckGenesForFeatureType(CBioseq_Handle bh, 
                                               TGenesUsedMap& genes, 
                                               CSeqFeatData::E_Choice choice,
                                               bool makes_gene_not_superfluous, 
                                               const string& filename)
{
    CFeat_CI fi(bh, choice);
    while (fi) {
        CConstRef<CSeq_feat> feat_gene = edit::GetGeneForFeature(*(fi->GetSeq_feat()), bh.GetScope());
        if (feat_gene) {
            if (makes_gene_not_superfluous) {
                genes[feat_gene.GetPointer()] = true;
            }
        } else {
            CRef< CReportObject> obj(new CReportObject(CConstRef<CObject>(fi->GetSeq_feat())));
            obj->SetFilename(filename);
            m_MissingGenes.push_back(obj);
        }
        ++fi;
    }
}


void CMissingGenes::x_CheckGenesForFeatureType(CBioseq_Handle bh, 
                                               TGenesUsedMap& genes, 
                                               CSeqFeatData::ESubtype subtype,
                                               bool makes_gene_not_superfluous, 
                                               const string& filename)
{
    CFeat_CI fi(bh, subtype);
    while (fi) {
        CConstRef<CSeq_feat> feat_gene = edit::GetGeneForFeature(*(fi->GetSeq_feat()), bh.GetScope());
        if (feat_gene) {
            if (makes_gene_not_superfluous) {
                genes[feat_gene.GetPointer()] = true;
            }
        } else {
            CRef< CReportObject> obj(new CReportObject(CConstRef<CObject>(fi->GetSeq_feat())));
            obj->SetFilename(filename);
            m_MissingGenes.push_back(obj);
        }
        ++fi;
    }
}


bool s_StringHasPhrase(const string& str, const string& phrase)
{    
    if (NStr::EqualNocase(str, phrase)) {
        return true;
    } else if (NStr::StartsWith(str, phrase) && str.c_str()[phrase.length()] == ';') {
        return true;
    } else {
        size_t pos = NStr::Find(str, ";");
        if (pos != string::npos) {
            while (isspace(str.c_str()[pos])) {
                pos++;
            }
            return s_StringHasPhrase(str.substr(pos), phrase);
        }
    }
    return false;
}


bool CMissingGenes::x_IsOkSuperfluousGene (const CSeq_feat& gene)
{
    if (gene.IsSetComment() && 
        s_StringHasPhrase(gene.GetComment(), "coding region not determined")) {
        return true;
    } else if (gene.GetData().GetGene().IsSetDesc() &&
        s_StringHasPhrase(gene.GetData().GetGene().GetDesc(), "coding region not determined")) {
        return true;
    } else {
        return false;
    }
}


bool CMissingGenes::x_IsPseudoGene(const CSeq_feat& gene)
{
    bool rval = false;
    if (!gene.IsSetData() || !gene.GetData().IsGene()) {
        rval = false;
    } else if (gene.IsSetPseudo() && gene.GetPseudo()) {
        rval = true;
    } else if (gene.GetData().GetGene().IsSetPseudo() && gene.GetData().GetGene().GetPseudo()) {
        rval = true;
    } else if (gene.IsSetQual()) {
        ITERATE(CSeq_feat::TQual, it, gene.GetQual()) {
            if ((*it)->IsSetQual() && NStr::EqualNocase((*it)->GetQual(), "pseudogene")) {
                rval = true;
                break;
            }
        }
    }
    return rval;
}


bool CMissingGenes::x_GeneHasNoteOrDesc(const CSeq_feat& gene)
{
    bool rval = false;
    if (gene.IsSetComment() && !NStr::IsBlank(gene.GetComment())) {
        rval = true;
    } else if (gene.IsSetData() && gene.GetData().IsGene() &&
               gene.GetData().GetGene().IsSetDesc() &&
               !NStr::IsBlank(gene.GetData().GetGene().GetDesc())) {
        rval = true;
    }
    return rval;
}


void CMissingGenes::CollateData()
{
}


void CMissingGenes::ResetData()
{
    m_SuperfluousPseudoGenes.clear();
    m_SuperfluousNonPseudoNonFrameshiftGenes.clear();
    m_MissingGenes.clear();
}


TReportItemList CMissingGenes::GetReportItems(CReportConfig& cfg) const
{
    TReportItemList list;

    // Add individual feature without gene items
    if (cfg.IsEnabled(kMISSING_GENES) && !m_MissingGenes.empty()) {
        CRef<CReportItem> item(new CReportItem());
        item->SetSettingName(kMISSING_GENES);
        item->SetObjects() = m_MissingGenes;
        item->SetTextWithHas("feature", "no genes.");
        list.push_back(item);
    }

    if (cfg.IsEnabled(kDISC_SUPERFLUOUS_GENE) && 
        (!m_SuperfluousPseudoGenes.empty() ||
         !m_SuperfluousNonPseudoNonFrameshiftGenes.empty())) {
        CRef<CReportItem> item(new CReportItem());
        item->SetSettingName(kDISC_SUPERFLUOUS_GENE);
        if (!m_SuperfluousPseudoGenes.empty()) {
            CRef<CReportItem> sub(new CReportItem());
            sub->SetSettingName(kDISC_SUPERFLUOUS_GENE);
            sub->SetObjects() = m_SuperfluousPseudoGenes;
            sub->SetTextWithIs("pseudo gene feature", "not associated with a CDS or RNA feature.");
            item->SetSubitems().push_back(sub);
        }
        if (!m_SuperfluousNonPseudoNonFrameshiftGenes.empty()) {
            CRef<CReportItem> sub(new CReportItem());
            sub->SetSettingName(kDISC_SUPERFLUOUS_GENE);
            sub->SetObjects() = m_SuperfluousNonPseudoNonFrameshiftGenes;
            string phrase = "not associated with a CDS or RNA feature and ";
            if (m_SuperfluousNonPseudoNonFrameshiftGenes.size() > 1) {
                phrase += "do";
            } else {
                phrase += "does";
            }
            phrase += " not have frameshift in the comment.";
            sub->SetTextWithIs("non-pseudo gene feature", phrase);
            item->SetSubitems().push_back(sub);
        }
        item->AddObjectsFromSubcategories();
        item->SetTextWithIs("gene feature", "not associated with a CDS or RNA feature.");
        list.push_back(item);
    }

    return list;
}


vector<string> CMissingGenes::Handles() const
{
    vector<string> list;
    list.push_back(kMISSING_GENES);
    list.push_back(kDISC_SUPERFLUOUS_GENE);
    return list;
}


void CMissingGenes::DropReferences(CScope& scope)
{
    NON_CONST_ITERATE(TReportObjectList, it, m_SuperfluousPseudoGenes) {
        (*it)->DropReference(scope);
    }

    NON_CONST_ITERATE(TReportObjectList, it, m_SuperfluousNonPseudoNonFrameshiftGenes) {
        (*it)->DropReference(scope);
    }

    NON_CONST_ITERATE(TReportObjectList, it, m_MissingGenes) {
        (*it)->DropReference(scope);
    }

}





END_NCBI_SCOPE
