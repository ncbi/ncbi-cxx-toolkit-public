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
 *   Code for cleaning up duplicate features
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <serial/serialbase.hpp>

#include <objmgr/feat_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seq_feat_handle.hpp>
#include <objtools/validator/validerror_bioseq.hpp>
#include <objtools/validator/dup_feats.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)


void GetProductToCDSMap(CScope &scope, map<CBioseq_Handle, set<CSeq_feat_Handle> > &product_to_cds)
{
    product_to_cds.clear();
    CScope::TTSE_Handles tses;
    scope.GetAllTSEs(tses, CScope::eAllTSEs);
    for (auto tse : tses)
    {
        for (CFeat_CI feat_it(tse, SAnnotSelector(CSeqFeatData::eSubtype_cdregion)); feat_it; ++feat_it)
        {
            if (feat_it->IsSetProduct())
            {
                CSeq_feat_Handle fh = feat_it->GetSeq_feat_Handle();
                CBioseq_Handle product = scope.GetBioseqHandle(*(fh.GetProductId().GetSeqId()));
                product_to_cds[product].insert(fh);
            }
        }
    }
}


set< CSeq_feat_Handle > GetDuplicateFeaturesForRemoval(CSeq_entry_Handle seh)
{
    map<objects::CBioseq_Handle, set<objects::CSeq_feat_Handle> > product_to_cds;
    GetProductToCDSMap(seh.GetScope(), product_to_cds);
    set< CSeq_feat_Handle > deleted_feats;
    for (CFeat_CI feat1(seh); feat1; ++feat1)
    {
        for (CFeat_CI feat2(seh.GetScope(), feat1->GetLocation()); feat2; ++feat2)
        {
            if (feat1->GetSeq_feat_Handle() < feat2->GetSeq_feat_Handle() &&
                deleted_feats.find(feat1->GetSeq_feat_Handle()) == deleted_feats.end() &&
                deleted_feats.find(feat2->GetSeq_feat_Handle()) == deleted_feats.end()) {
                validator::EDuplicateFeatureType dup_type = validator::IsDuplicate(feat1->GetSeq_feat_Handle(), feat2->GetSeq_feat_Handle());
                if (dup_type == validator::eDuplicate_Duplicate || dup_type == validator::eDuplicate_DuplicateDifferentTable) {
                    deleted_feats.insert(feat2->GetSeq_feat_Handle());
                }
            }
        }
    }
    return deleted_feats;
}


bool AllowOrphanedProtein(const CBioseq& seq, bool force_refseq)
{
    bool is_genbank = false;
    bool is_embl = false;
    bool is_ddbj = false;
    bool is_refseq = force_refseq;
    bool is_wp = false;
    bool is_yp = false;
    bool is_gibbmt = false;
    bool is_gibbsq = false;
    bool is_patent = false;
    FOR_EACH_SEQID_ON_BIOSEQ(id_it, seq) {
        const CSeq_id& sid = **id_it;
        switch (sid.Which()) {
        case CSeq_id::e_Genbank:
            is_genbank = true;
            break;
        case CSeq_id::e_Embl:
            is_embl = true;
            break;
        case CSeq_id::e_Ddbj:
            is_ddbj = true;
            break;
        case CSeq_id::e_Other:
        {
            is_refseq = true;
            const CTextseq_id* tsid = sid.GetTextseq_Id();
            if (tsid != NULL && tsid->IsSetAccession()) {
                const string& acc = tsid->GetAccession();
                if (NStr::StartsWith(acc, "WP_")) {
                    is_wp = true;
                }
                else if (NStr::StartsWith(acc, "YP_")) {
                    is_yp = true;
                }
            }
        }
        break;
        case CSeq_id::e_Gibbmt:
            is_gibbmt = true;
            break;
        case CSeq_id::e_Gibbsq:
            is_gibbsq = true;
            break;
        case CSeq_id::e_Patent:
            is_patent = true;
            break;
        default:
            break;
        }
    }
    if ((is_genbank || is_embl || is_ddbj || is_refseq)
        && !is_gibbmt && !is_gibbsq && !is_patent && !is_wp && !is_yp) {
        return false;
    }
    else {
        return true;
    }
}


set< CBioseq_Handle > ListOrphanProteins(CSeq_entry_Handle seh, bool force_refseq)
{
    set<CBioseq_Handle> proteins;
    for (CFeat_CI fi(seh, CSeqFeatData::eSubtype_cdregion); fi; ++fi)
    {
        if (fi->IsSetProduct())
        {
            CBioseq_Handle prot_bsh = fi->GetScope().GetBioseqHandle(fi->GetProduct());
            if (prot_bsh && prot_bsh.IsProtein())
            {
                proteins.insert(prot_bsh);
            }
        }
    }
    set< CBioseq_Handle > orphan_proteins;
    objects::CBioseq_CI b_iter(seh, objects::CSeq_inst::eMol_aa);
    for (; b_iter; ++b_iter)
    {
        CBioseq_Handle bsh = *b_iter;
        if (!AllowOrphanedProtein(*(bsh.GetBioseqCore()), force_refseq) &&
            proteins.find(bsh) == proteins.end())
        {
            orphan_proteins.insert(bsh);
        }
    }
    return orphan_proteins;
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
