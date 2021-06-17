/* $Id$
 * ===========================================================================
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
 * ===========================================================================
 *
 * Author:  Colleen Bollin
 *
 * File Description:
 *   Code for moving gene qualifiers from gene xrefs on child features to
 *   the actual gene
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <serial/serialbase.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objtools/cleanup/cleanup.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)



bool IsMappablePair(const CSeq_feat& cds, const CSeq_feat& gene)
{
    // we're only going to move the qualifiers if the gene has a
    // locus_tag and no locus
    if (!gene.GetData().IsGene() ||
        gene.GetData().GetGene().IsSetLocus() ||
        !gene.GetData().GetGene().IsSetLocus_tag() ||
        !cds.IsSetXref()) {
        return false;
    }

    CTempString locus;
    CTempString locus_tag;
    for (auto it : cds.GetXref()) {
        if (it->IsSetData() && it->GetData().IsGene()) {
            if (it->GetData().GetGene().IsSetLocus()) {
                if (NStr::IsBlank(locus)) {
                    locus = it->GetData().GetGene().GetLocus();
                } else {
                    // already found a locus value, quit
                    return false;
                }
            }
            if (it->GetData().GetGene().IsSetLocus_tag()) {
                if (NStr::IsBlank(locus_tag)) {
                    locus_tag = it->GetData().GetGene().GetLocus_tag();
                } else {
                    // already found a locus-tag value, quit
                    return false;
                }
            }
        }
    }
    if (NStr::IsBlank(locus)) {
        // no locus that we can use
        return false;
    }

    if (!NStr::IsBlank(locus_tag) &&
        gene.GetData().GetGene().IsSetLocus_tag() &&
        !NStr::Equal(locus_tag, gene.GetData().GetGene().GetLocus_tag())) {
        return false;
    }

    return true;
}


bool CCleanup::NormalizeGeneQuals(CSeq_feat& cds, CSeq_feat& gene)
{
    if (!gene.GetData().IsGene() || gene.GetData().GetGene().IsSetLocus()
        || !cds.IsSetXref()) {
        return false;
    }
    CTempString locus;
    CTempString locus_tag;
    CRef<CSeqFeatXref> gene_xref(NULL);
    for (auto it : cds.SetXref()) {
        if (it->IsSetData() && it->GetData().IsGene()) {
            if (it->GetData().GetGene().IsSetLocus()) {
                if (NStr::IsBlank(locus)) {
                    locus = it->GetData().GetGene().GetLocus();
                    gene_xref = it;
                } else {
                    // already found a locus value, quit
                    return false;
                }
            }
            if (it->GetData().GetGene().IsSetLocus_tag()) {
                if (NStr::IsBlank(locus_tag)) {
                    locus_tag = it->GetData().GetGene().GetLocus_tag();
                } else {
                    // already found a locus-tag value, quit
                    return false;
                }
            }
        }
    }
    if (NStr::IsBlank(locus)) {
        // no locus that we can use
        return false;
    }

    if (!NStr::IsBlank(locus_tag) &&
        gene.GetData().GetGene().IsSetLocus_tag() &&
        !NStr::Equal(locus_tag, gene.GetData().GetGene().GetLocus_tag())) {
        return false;
    }

    if (!NStr::IsBlank(locus)) {
        gene.SetData().SetGene().SetLocus(locus);
        if (gene.GetData().GetGene().IsSetLocus_tag() &&
            !NStr::IsBlank(gene.GetData().GetGene().GetLocus_tag())) {
            // gene xrefs should point to locus_tag instead of locus
            gene_xref->SetData().SetGene().ResetLocus();
            gene_xref->SetData().SetGene().SetLocus_tag(gene.GetData().GetGene().GetLocus_tag());
        }
        return true;
    } else {
        return false;
    }
}


vector<CCleanup::TFeatGenePair> CCleanup::GetNormalizableGeneQualPairs(CBioseq_Handle bsh)
{
    vector<CCleanup::TFeatGenePair> rval;

    // only for nucleotide sequences
    if (bsh.IsAa()) {
        return rval;
    }
    // only for prokaryotes
    CSeqdesc_CI src(bsh, CSeqdesc::e_Source);
    if (!src || !src->IsSource() || !src->GetSource().IsSetLineage() ||
        (NStr::Find(src->GetSource().GetLineage(), "Bacteria; ") == NPOS &&
         NStr::Find(src->GetSource().GetLineage(), "Archaea") == NPOS)) {
        return rval;
    }

    typedef pair<CSeq_feat_Handle, bool> TFeatOkPair;
    typedef map<CSeq_feat_Handle, TFeatOkPair > TGeneCDSMap;
    TGeneCDSMap gene_cds;

    CFeat_CI f(bsh);
    CRef<feature::CFeatTree> tr(new feature::CFeatTree(f));
    tr->SetIgnoreMissingGeneXref();
    while (f) {
        if (f->GetData().IsCdregion()) {
            CMappedFeat gene = tr->GetBestGene(*f);
            if (gene) {
                TGeneCDSMap::iterator smit = gene_cds.find(gene);
                if (smit == gene_cds.end()) {
                    // not found before
                    bool ok_to_map = IsMappablePair(f->GetOriginalFeature(), gene.GetOriginalFeature());
                    gene_cds[gene] = { f->GetSeq_feat_Handle(), ok_to_map };
                } else {
                    // same gene, two different coding regions
                    gene_cds[gene].second = false;
                }
            }
        }
        ++f;
    }

    for (auto copy_pair : gene_cds) {
        if (copy_pair.second.second) {
            rval.push_back({ copy_pair.second.first, copy_pair.first });
        }
    }
    return rval;
}

bool CCleanup::NormalizeGeneQuals(CBioseq_Handle bsh)
{
    vector<CCleanup::TFeatGenePair> cds_gene_pairs = GetNormalizableGeneQualPairs(bsh);
    bool any_change = false;
    for (auto copy_pair : cds_gene_pairs) {
        CRef<CSeq_feat> new_cds(new CSeq_feat());
        new_cds->Assign(*(copy_pair.first.GetOriginalSeq_feat()));
        CRef<CSeq_feat> new_gene(new CSeq_feat());
        new_gene->Assign(*(copy_pair.second.GetOriginalSeq_feat()));
        if (NormalizeGeneQuals(*new_cds, *new_gene)) {
            CSeq_feat_EditHandle gene_edit = CSeq_feat_EditHandle(copy_pair.second);
            gene_edit.Replace(*new_gene);
            CSeq_feat_EditHandle cds_edit = CSeq_feat_EditHandle(copy_pair.first);
            cds_edit.Replace(*new_cds);
            any_change = true;
        }
    }

    return any_change;
}

bool CCleanup::NormalizeGeneQuals(CSeq_entry_Handle seh)
{
    bool any_change = false;
    CBioseq_CI bi(seh, CSeq_inst::eMol_na);
    while (bi) {
        any_change |= NormalizeGeneQuals(*bi);
        ++bi;
    }
    return any_change;
}

END_SCOPE(objects)
END_NCBI_SCOPE
