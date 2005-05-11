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
 * Authors:  Josh Cherry
 *
 * File Description:  SWIG interface file for misc
 *
 */

%%

objects/access/Link_set.hpp
objects/access/Link_set_.hpp
objects/docsum/NSE_Assay.hpp
objects/docsum/NSE_Assay_.hpp
objects/docsum/NSE_AssayList.hpp
objects/docsum/NSE_AssayList_.hpp
objects/docsum/NSE_BaseURL.hpp
objects/docsum/NSE_BaseURL_.hpp
objects/docsum/NSE_BaseURLSet.hpp
objects/docsum/NSE_BaseURLSet_.hpp
objects/docsum/NSE_Date.hpp
objects/docsum/NSE_Date_.hpp
objects/docsum/NSE_Date_std.hpp
objects/docsum/NSE_Date_std_.hpp
objects/docsum/NSE_ExchangeSet.hpp
objects/docsum/NSE_ExchangeSet_.hpp
objects/docsum/NSE_FxnSet.hpp
objects/docsum/NSE_FxnSet_.hpp
objects/docsum/NSE_Pop.hpp
objects/docsum/NSE_Pop_.hpp
objects/docsum/NSE_PopList.hpp
objects/docsum/NSE_PopList_.hpp
objects/docsum/NSE_alleleinfo.hpp
objects/docsum/NSE_alleleinfo_.hpp
objects/docsum/NSE_rs.hpp
objects/docsum/NSE_rs_.hpp
objects/docsum/NSE_rsContigHit.hpp
objects/docsum/NSE_rsContigHit_.hpp
objects/docsum/NSE_rslink.hpp
objects/docsum/NSE_rslink_.hpp
objects/docsum/NSE_rsLocusID.hpp
objects/docsum/NSE_rsLocusID_.hpp
objects/docsum/NSE_rsMaploc.hpp
objects/docsum/NSE_rsMaploc_.hpp
objects/docsum/NSE_rsMerge.hpp
objects/docsum/NSE_rsMerge_.hpp
objects/docsum/NSE_rsSeqHit.hpp
objects/docsum/NSE_rsSeqHit_.hpp
objects/docsum/NSE_rsStruct.hpp
objects/docsum/NSE_rsStruct_.hpp
objects/docsum/NSE_rsUnigene.hpp
objects/docsum/NSE_rsUnigene_.hpp
objects/docsum/NSE_SeqLoc.hpp
objects/docsum/NSE_SeqLoc_.hpp
objects/docsum/NSE_ss.hpp
objects/docsum/NSE_ss_.hpp
objects/docsum/NSE_ss_popinfo.hpp
objects/docsum/NSE_ss_popinfo_.hpp
objects/docsum/NSE_Submitter.hpp
objects/docsum/NSE_Submitter_.hpp
objects/docsum/NSE_SubmitterList.hpp
objects/docsum/NSE_SubmitterList_.hpp
objects/entrez2/E2Reply.hpp
objects/entrez2/E2Reply_.hpp
objects/entrez2/E2Request.hpp
objects/entrez2/E2Request_.hpp
objects/entrez2/Entrez2_boolean_element.hpp
objects/entrez2/Entrez2_boolean_element_.hpp
objects/entrez2/Entrez2_boolean_exp.hpp
objects/entrez2/Entrez2_boolean_exp_.hpp
objects/entrez2/Entrez2_boolean_reply.hpp
objects/entrez2/Entrez2_boolean_reply_.hpp
objects/entrez2/Entrez2_boolean_term.hpp
objects/entrez2/Entrez2_boolean_term_.hpp
objects/entrez2/entrez2_client.hpp
objects/entrez2/entrez2_client_.hpp
objects/entrez2/Entrez2_db_id.hpp
objects/entrez2/Entrez2_db_id_.hpp
objects/entrez2/Entrez2_db_info.hpp
objects/entrez2/Entrez2_db_info_.hpp
objects/entrez2/Entrez2_docsum.hpp
objects/entrez2/Entrez2_docsum_.hpp
objects/entrez2/Entrez2_docsum_data.hpp
objects/entrez2/Entrez2_docsum_data_.hpp
objects/entrez2/Entrez2_docsum_field_info.hpp
objects/entrez2/Entrez2_docsum_field_info_.hpp
objects/entrez2/Entrez2_docsum_field_type.hpp
objects/entrez2/Entrez2_docsum_field_type_.hpp
objects/entrez2/Entrez2_docsum_list.hpp
objects/entrez2/Entrez2_docsum_list_.hpp
objects/entrez2/Entrez2_dt.hpp
objects/entrez2/Entrez2_dt_.hpp
objects/entrez2/Entrez2_dt_filter.hpp
objects/entrez2/Entrez2_dt_filter_.hpp
objects/entrez2/Entrez2_eval_boolean.hpp
objects/entrez2/Entrez2_eval_boolean_.hpp
objects/entrez2/Entrez2_field_id.hpp
objects/entrez2/Entrez2_field_id_.hpp
objects/entrez2/Entrez2_field_info.hpp
objects/entrez2/Entrez2_field_info_.hpp
objects/entrez2/Entrez2_get_links.hpp
objects/entrez2/Entrez2_get_links_.hpp
objects/entrez2/Entrez2_hier_node.hpp
objects/entrez2/Entrez2_hier_node_.hpp
objects/entrez2/Entrez2_hier_query.hpp
objects/entrez2/Entrez2_hier_query_.hpp
objects/entrez2/Entrez2_id.hpp
objects/entrez2/Entrez2_id_.hpp
objects/entrez2/Entrez2_id_list.hpp
objects/entrez2/Entrez2_id_list_.hpp
objects/entrez2/Entrez2_info.hpp
objects/entrez2/Entrez2_info_.hpp
objects/entrez2/Entrez2_limits.hpp
objects/entrez2/Entrez2_limits_.hpp
objects/entrez2/Entrez2_link_count.hpp
objects/entrez2/Entrez2_link_count_.hpp
objects/entrez2/Entrez2_link_count_list.hpp
objects/entrez2/Entrez2_link_count_list_.hpp
objects/entrez2/Entrez2_link_id.hpp
objects/entrez2/Entrez2_link_id_.hpp
objects/entrez2/Entrez2_link_info.hpp
objects/entrez2/Entrez2_link_info_.hpp
objects/entrez2/Entrez2_link_set.hpp
objects/entrez2/Entrez2_link_set_.hpp
objects/entrez2/Entrez2_operator.hpp
objects/entrez2/Entrez2_operator_.hpp
objects/entrez2/Entrez2_reply.hpp
objects/entrez2/Entrez2_reply_.hpp
objects/entrez2/Entrez2_request.hpp
objects/entrez2/Entrez2_request_.hpp
objects/entrez2/Entrez2_term.hpp
objects/entrez2/Entrez2_term_.hpp
objects/entrez2/Entrez2_term_list.hpp
objects/entrez2/Entrez2_term_list_.hpp
objects/entrez2/Entrez2_term_pos.hpp
objects/entrez2/Entrez2_term_pos_.hpp
objects/entrez2/Entrez2_term_query.hpp
objects/entrez2/Entrez2_term_query_.hpp
objects/featdef/FeatDef.hpp
objects/featdef/FeatDef_.hpp
objects/featdef/FeatDefGroupSet.hpp
objects/featdef/FeatDefGroupSet_.hpp
objects/featdef/FeatDefSet.hpp
objects/featdef/FeatDefSet_.hpp
objects/featdef/FeatDispGroup.hpp
objects/featdef/FeatDispGroup_.hpp
objects/featdef/FeatDispGroupSet.hpp
objects/featdef/FeatDispGroupSet_.hpp
objects/featdef/FeatMolType.hpp
objects/featdef/FeatMolType_.hpp
objects/gbseq/GBAuthor.hpp
objects/gbseq/GBAuthor_.hpp
objects/gbseq/GBFeature.hpp
objects/gbseq/GBFeature_.hpp
objects/gbseq/GBInterval.hpp
objects/gbseq/GBInterval_.hpp
objects/gbseq/GBKeyword.hpp
objects/gbseq/GBKeyword_.hpp
objects/gbseq/GBQualifier.hpp
objects/gbseq/GBQualifier_.hpp
objects/gbseq/GBReference.hpp
objects/gbseq/GBReference_.hpp
objects/gbseq/GBSecondary_accn.hpp
objects/gbseq/GBSecondary_accn_.hpp
objects/gbseq/GBSeq.hpp
objects/gbseq/GBSeq_.hpp
objects/gbseq/GBSeqid.hpp
objects/gbseq/GBSeqid_.hpp
objects/gbseq/GBSet.hpp
objects/gbseq/GBSet_.hpp
objects/mim/Mim_allelic_variant.hpp
objects/mim/Mim_allelic_variant_.hpp
objects/mim/Mim_author.hpp
objects/mim/Mim_author_.hpp
objects/mim/Mim_cit.hpp
objects/mim/Mim_cit_.hpp
objects/mim/Mim_date.hpp
objects/mim/Mim_date_.hpp
objects/mim/Mim_edit_item.hpp
objects/mim/Mim_edit_item_.hpp
objects/mim/Mim_entry.hpp
objects/mim/Mim_entry_.hpp
objects/mim/Mim_index_term.hpp
objects/mim/Mim_index_term_.hpp
objects/mim/Mim_link.hpp
objects/mim/Mim_link_.hpp
objects/mim/Mim_page.hpp
objects/mim/Mim_page_.hpp
objects/mim/Mim_reference.hpp
objects/mim/Mim_reference_.hpp
objects/mim/Mim_set.hpp
objects/mim/Mim_set_.hpp
objects/mim/Mim_text.hpp
objects/mim/Mim_text_.hpp
objects/objprt/PrintForm.hpp
objects/objprt/PrintForm_.hpp
objects/objprt/PrintFormat.hpp
objects/objprt/PrintFormat_.hpp
objects/objprt/PrintFormBlock.hpp
objects/objprt/PrintFormBlock_.hpp
objects/objprt/PrintFormBoolean.hpp
objects/objprt/PrintFormBoolean_.hpp
objects/objprt/PrintFormEnum.hpp
objects/objprt/PrintFormEnum_.hpp
objects/objprt/PrintFormText.hpp
objects/objprt/PrintFormText_.hpp
objects/objprt/PrintTemplate.hpp
objects/objprt/PrintTemplate_.hpp
objects/objprt/PrintTemplateSet.hpp
objects/objprt/PrintTemplateSet_.hpp
objects/objprt/TemplateName.hpp
objects/objprt/TemplateName_.hpp
objects/objprt/UserFormat.hpp
objects/objprt/UserFormat_.hpp
objects/tinyseq/TSeq.hpp
objects/tinyseq/TSeq_.hpp
objects/tinyseq/TSeqSet.hpp
objects/tinyseq/TSeqSet_.hpp
objects/remap/RMReply.hpp
objects/remap/RMReply_.hpp
objects/remap/RMRequest.hpp
objects/remap/RMRequest_.hpp
objects/remap/Remap_db_id.hpp
objects/remap/Remap_db_id_.hpp
objects/remap/Remap_dt.hpp
objects/remap/Remap_dt_.hpp
objects/remap/Remap_query.hpp
objects/remap/Remap_query_.hpp
objects/remap/Remap_reply.hpp
objects/remap/Remap_reply_.hpp
objects/remap/Remap_request.hpp
objects/remap/Remap_request_.hpp
objects/remap/Remap_result.hpp
objects/remap/Remap_result_.hpp
objects/remap/remap_client.hpp
objects/remap/remap_client_.hpp

// entrezgene

objects/entrezgene/Entrezgene.hpp
objects/entrezgene/Entrezgene_.hpp
objects/entrezgene/Gene_commentary.hpp
objects/entrezgene/Gene_commentary_.hpp
objects/entrezgene/Gene_source.hpp
objects/entrezgene/Gene_source_.hpp
objects/entrezgene/Gene_track.hpp
objects/entrezgene/Gene_track_.hpp
objects/entrezgene/Maps.hpp
objects/entrezgene/Maps_.hpp
objects/entrezgene/Other_source.hpp
objects/entrezgene/Other_source_.hpp
objects/entrezgene/Xtra_Terms.hpp
objects/entrezgene/Xtra_Terms_.hpp

// proj

objects/proj/Projdesc.hpp
objects/proj/Projdesc_.hpp
objects/proj/Project.hpp
objects/proj/Project_.hpp
objects/proj/Project_descr.hpp
objects/proj/Project_descr_.hpp
objects/proj/Project_id.hpp
objects/proj/Project_id_.hpp
objects/proj/Project_item.hpp
objects/proj/Project_item_.hpp

// biotree

objects/biotree/BioTreeContainer.hpp
objects/biotree/BioTreeContainer_.hpp
objects/biotree/FeatureDescr.hpp
objects/biotree/FeatureDescr_.hpp
objects/biotree/FeatureDictSet.hpp
objects/biotree/FeatureDictSet_.hpp
objects/biotree/Node.hpp
objects/biotree/NodeFeature.hpp
objects/biotree/NodeFeatureSet.hpp
objects/biotree/NodeFeatureSet_.hpp
objects/biotree/NodeFeature_.hpp
objects/biotree/NodeSet.hpp
objects/biotree/NodeSet_.hpp
objects/biotree/Node_.hpp

// INSD

objects/insdseq/INSDAuthor.hpp
objects/insdseq/INSDAuthor_.hpp
objects/insdseq/INSDFeature.hpp
objects/insdseq/INSDFeature_.hpp
objects/insdseq/INSDInterval.hpp
objects/insdseq/INSDInterval_.hpp
objects/insdseq/INSDKeyword.hpp
objects/insdseq/INSDKeyword_.hpp
objects/insdseq/INSDQualifier.hpp
objects/insdseq/INSDQualifier_.hpp
objects/insdseq/INSDReference.hpp
objects/insdseq/INSDReference_.hpp
objects/insdseq/INSDSecondary_accn.hpp
objects/insdseq/INSDSecondary_accn_.hpp
objects/insdseq/INSDSeq.hpp
objects/insdseq/INSDSeq_.hpp
objects/insdseq/INSDSeqid.hpp
objects/insdseq/INSDSeqid_.hpp
objects/insdseq/INSDSet.hpp
objects/insdseq/INSDSet_.hpp

%%

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/05/11 21:27:35  jcherry
 * Initial version
 *
 * ===========================================================================
 */
