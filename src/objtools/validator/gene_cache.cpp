/*  $Id$
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
 *   Gene cache for validating features
 *   .......
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbiapp.hpp>
#include <objmgr/object_manager.hpp>

#include <objtools/validator/gene_cache.hpp>
#include <objtools/validator/utilities.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>

#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seq_annot_ci.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>

#include <objmgr/feat_ci.hpp>
#include <objmgr/scope.hpp>


#define NCBI_USE_ERRCODE_X   Objtools_Validator

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)
using namespace sequence;


CRef<feature::CFeatTree> CGeneCache::GetFeatTreeFromCache(CBioseq_Handle bsh)
{
    TSeqTreeMap::iterator smit = m_SeqTreeMap.find(bsh);
    if (smit == m_SeqTreeMap.end()) {
        // test: only keep the last one
        m_SeqTreeMap.clear();
        CFeat_CI f(bsh);
        CRef<feature::CFeatTree> tr(new feature::CFeatTree(f));
        m_SeqTreeMap[bsh] = tr;
        return tr;
    } else  {
        return smit->second;
    }
}


CRef<feature::CFeatTree> CGeneCache::GetFeatTreeFromCache(const CSeq_loc& loc, CScope& scope)
{
    CBioseq_Handle bsh;
    try {
        bsh = scope.GetBioseqHandle(loc);
    } catch (CException&) {
        CSeq_loc_CI li(loc);
        while (li && !bsh) {
            bsh = scope.GetBioseqHandle(li.GetSeq_id());
            ++li;
        }
    }

    if (bsh) {
        return GetFeatTreeFromCache(bsh);
    } else {
        return (CRef<feature::CFeatTree>());
    }
}


CRef<feature::CFeatTree> CGeneCache::GetFeatTreeFromCache(const CSeq_feat& feat, CScope& scope)
{
    return GetFeatTreeFromCache(feat.GetLocation(), scope);
}


CConstRef<CSeq_feat> CGeneCache::GetGeneFromCache(const CSeq_feat* feat, CScope& scope)
{
    if (!feat) {
        return CConstRef<CSeq_feat>();
    }
    CConstRef<CSeq_feat> gene;
    TFeatGeneMap::iterator it = m_FeatGeneMap.find(feat);
    if (it == m_FeatGeneMap.end()) {
        try {
            CSeq_feat_Handle fh = scope.GetSeq_featHandle(*feat);
            CRef<feature::CFeatTree> tr = GetFeatTreeFromCache(*feat, scope);
            if (!tr) {
                return CConstRef<CSeq_feat>();
            }
            CMappedFeat mf = tr->GetBestGene(fh);
            if (mf) {
                gene = mf.GetSeq_feat();
            }
        } catch (CException&) {
            gene = sequence::GetGeneForFeature(*feat, scope);
        }
        m_FeatGeneMap[feat] = gene;
        return gene;
    } else {
        return it->second;
    }
}


bool CGeneCache::x_HasNamedQual(const CSeq_feat& feat, const string& qual)
{
    bool rval = false;
    if (feat.IsSetQual()) {
        for (auto it : feat.GetQual()) {
            if (it->IsSetQual() && NStr::EqualNocase(it->GetQual(), qual)) {
                rval = true;
                break;
            }
        }
    }
    return rval;
}


bool CGeneCache::x_IsPseudo(const CGene_ref& gref)
{
    return (gref.IsSetPseudo() && gref.GetPseudo());
}


bool CGeneCache::IsPseudo(const CSeq_feat& feat, CScope& scope)
{
    return (feat.IsSetPseudo() && feat.GetPseudo()) ||
        (x_HasNamedQual(feat, "pseudogene")) ||
        (feat.GetData().IsGene() && x_IsPseudo(feat.GetData().GetGene()));
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
