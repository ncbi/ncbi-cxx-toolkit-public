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
 * Authors:  Frank Ludwig
 *
 * File Description:  Write gff file
 *
 */

#include <ncbi_pch.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/so_map.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/RNA_gen.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Spliced_exon_chunk.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seq/sofa_type.hpp>
#include <objects/seq/sofa_map.hpp>

#include <objmgr/feat_ci.hpp>
#include <objmgr/annot_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/mapped_feat.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/feature_edit.hpp>

#include <objtools/writers/writer_exception.hpp>
#include <objtools/writers/write_util.hpp>
#include <objtools/writers/gff3_write_data.hpp>
#include <objtools/writers/gff3_source_data.hpp>
#include <objtools/writers/gff3_alignment_data.hpp>
#include <objects/seqalign/Score_set.hpp>
#include <objtools/writers/gff3_writer.hpp>

#include <array>
#include <sstream>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//bool bDebugHere = false;

#define IS_INSERTION(sf, tf) \
    ( ((sf) &  CAlnMap::fSeq) && !((tf) &  CAlnMap::fSeq) )
#define IS_DELETION(sf, tf) \
    ( !((sf) &  CAlnMap::fSeq) && ((tf) &  CAlnMap::fSeq) )
#define IS_MATCH(sf, tf) \
    ( ((sf) &  CAlnMap::fSeq) && ((tf) &  CAlnMap::fSeq) )

//  ------------------------------------------------------------------------------
string CGffIdGenerator::GetId(
    const CMappedFeat& mf) 
//  ------------------------------------------------------------------------------
{
    string id;
    switch(mf.GetFeatSubtype()) {
    default:
        return "";
    case CSeqFeatData::eSubtype_gene:
        id = xGetIdForGene(mf);
        break;
    case CSeqFeatData::eSubtype_mRNA:
        return "";
    case CSeqFeatData::eSubtype_cdregion:
        return "";
    }
    if (!id.empty()) {
        id = xDisambiguate(id);
        m_existingIds.emplace(id);
    }
    return id;
}

//  -----------------------------------------------------------------------------
void CGffIdGenerator::Reset()
//  -----------------------------------------------------------------------------
{
    m_existingIds.clear();
}

//  -----------------------------------------------------------------------------
string CGffIdGenerator::xGetIdForGene(
    const CMappedFeat& mf)
//  -----------------------------------------------------------------------------
{
    string retainedId = mf.GetNamedQual("ID");
    if (!retainedId.empty()) {
        return retainedId;
    }
    string id("gene-");

    //1st choice: locus_tag
    const auto& geneRef = mf.GetData().GetGene();
    if (geneRef.IsSetLocus_tag()) {
        id += geneRef.GetLocus_tag();
        return id;
    }

    //2nd choice: locus
    if (geneRef.IsSetLocus()) {
        id += geneRef.GetLocus();
        return id;
    }
    //3rd choice: CombinedFeatureUserObjects:TrackingID
    auto trackingId = xExtractTrackingId(mf);
    if (!trackingId.empty()) {
        id += trackingId;
        return id;
    }

    //4th choice: local ID
    auto localId = xExtractLocalId(mf);
    if (!localId.empty()) {
        id += localId;
        return id;
    }
    //5th choice: best ID:start..stop
    string bestId;
    if (!CWriteUtil::GetBestId(mf, bestId)) {
        bestId = "unknown";
    } 
    auto inPoint = NStr::NumericToString(mf.GetLocationTotalRange().GetFrom());
    auto outPoint = NStr::NumericToString(mf.GetLocationTotalRange().GetTo());
    id += bestId;
    id += ":";
    id += inPoint;
    id += "..";
    id += outPoint;
    return id;
}

//  ----------------------------------------------------------------------------
string CGffIdGenerator::xExtractLocalId(
    const CMappedFeat& mf)
    //  ----------------------------------------------------------------------------
{
    ostringstream idStream;
    if (mf.IsSetId()) {
        auto& id = mf.GetId();
        if (id.IsLocal()) {
            id.GetLocal().AsString(idStream);
            return idStream.str();
        }
    }
    if (mf.IsSetIds()) {
        for (auto id: mf.GetIds()) {
            if (id->IsLocal()) {
                id->GetLocal().AsString(idStream);
                return idStream.str();
            }
        }
    }
    return "";
}

//  ------------------------------------------------------------------------------
string CGffIdGenerator::xExtractTrackingId(
    const CMappedFeat& mf)
//  ------------------------------------------------------------------------------
{
    CConstRef< CUser_object > cfob = mf.GetOriginalFeature().FindExt(
        "CombinedFeatureUserObjects");
    if (!cfob) {
        return "";
    }
    if (!cfob->HasField("TrackingId")) {
        return "";
    }
    auto trackingNumber = cfob->GetField("TrackingId").GetInt();
    return NStr::NumericToString(trackingNumber);
}

//  ------------------------------------------------------------------------------
string CGffIdGenerator::xDisambiguate(
    const string& baseId)
//  ------------------------------------------------------------------------------
{
    auto preExisting = m_existingIds.find(baseId);
    if (preExisting == m_existingIds.end()) {
        return baseId;
    }
    for (int suffix = 2; true; ++suffix) {
        auto disambiguated = baseId + "-" + NStr::NumericToString(suffix);
        preExisting = m_existingIds.find(disambiguated);
        if (preExisting == m_existingIds.end()) {
            return disambiguated;
        }
    }
    return baseId;
}

END_NCBI_SCOPE

