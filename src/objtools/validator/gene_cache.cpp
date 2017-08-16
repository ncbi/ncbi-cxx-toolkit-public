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
#include <objtools/validator/validator_barcode.hpp>
#include <objtools/cleanup/cleanup.hpp>

#include <serial/iterator.hpp>
#include <serial/enumvalues.hpp>

#include <objects/general/Dbtag.hpp>
#include <objects/general/Person_id.hpp>
#include <objects/general/Name_std.hpp>

#include <objects/seqalign/Seq_align.hpp>

#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SubSource.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Textseq_id.hpp>

#include <objects/seqres/Seq_graph.hpp>

#include <objects/submit/Seq_submit.hpp>
#include <objects/submit/Submit_block.hpp>

#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/seq_annot_ci.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>

#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/scope.hpp>

#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>

#include <objects/biblio/Author.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/Cit_book.hpp>
#include <objects/biblio/Cit_gen.hpp>
#include <objects/biblio/Cit_jour.hpp>
#include <objects/biblio/Cit_let.hpp>
#include <objects/biblio/Cit_proc.hpp>
#include <objects/biblio/Cit_sub.hpp>
#include <objects/biblio/PubMedId.hpp>
#include <objects/biblio/PubStatus.hpp>
#include <objects/biblio/Title.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/biblio/Affil.hpp>
#include <objects/misc/sequence_macros.hpp>
#include <objects/taxon3/itaxon3.hpp>
#include <objects/taxon3/taxon3.hpp>
#include <objects/taxon3/Taxon3_reply.hpp>

#include <objects/valid/Comment_set.hpp>
#include <objects/valid/Comment_rule.hpp>
#include <objects/valid/Field_set.hpp>
#include <objects/valid/Field_rule.hpp>
#include <objects/valid/Dependent_field_set.hpp>
#include <objects/valid/Dependent_field_rule.hpp>

#include <objtools/error_codes.hpp>
#include <objtools/validator/validerror_format.hpp>
#include <objtools/validator/utilities.hpp>
#include <objtools/edit/seq_entry_edit.hpp>
#include <util/sgml_entity.hpp>
#include <util/line_reader.hpp>
#include <util/util_misc.hpp>
#include <util/static_set.hpp>

#include <algorithm>


#include <serial/iterator.hpp>

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
        return (CRef<feature::CFeatTree>(NULL));
    }
}


CRef<feature::CFeatTree> CGeneCache::GetFeatTreeFromCache(const CSeq_feat& feat, CScope& scope)
{
    return GetFeatTreeFromCache(feat.GetLocation(), scope);
}


CConstRef<CSeq_feat> CGeneCache::GetGeneFromCache(const CSeq_feat* feat, CScope& scope)
{
    if (!feat) {
        return CConstRef<CSeq_feat>(NULL);
    }
    CConstRef<CSeq_feat> gene;
    TFeatGeneMap::iterator it = m_FeatGeneMap.find(feat);
    if (it == m_FeatGeneMap.end()) {
        try {
            CSeq_feat_Handle fh = scope.GetSeq_featHandle(*feat);
            CRef<feature::CFeatTree> tr = GetFeatTreeFromCache(*feat, scope);
            if (!tr) {
                return CConstRef<CSeq_feat>(NULL);
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


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
