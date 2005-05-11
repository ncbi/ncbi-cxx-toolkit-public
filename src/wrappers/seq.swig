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
 * File Description:  SWIG interface file for seq
 *
 */


%%

objects/seq/Align_def.hpp
objects/seq/Align_def_.hpp
objects/seq/Annot_descr.hpp
objects/seq/Annot_descr_.hpp
objects/seq/Annot_id.hpp
objects/seq/Annot_id_.hpp
objects/seq/Annotdesc.hpp
objects/seq/Annotdesc_.hpp
objects/seq/Bioseq.hpp
objects/seq/Bioseq_.hpp
objects/seq/Delta_ext.hpp
objects/seq/Delta_ext_.hpp
objects/seq/Delta_seq.hpp
objects/seq/Delta_seq_.hpp
objects/seq/GIBB_method.hpp
objects/seq/GIBB_method_.hpp
objects/seq/GIBB_mod.hpp
objects/seq/GIBB_mod_.hpp
objects/seq/GIBB_mol.hpp
objects/seq/GIBB_mol_.hpp
objects/seq/Heterogen.hpp
objects/seq/Heterogen_.hpp
objects/seq/IUPACaa.hpp
objects/seq/IUPACaa_.hpp
objects/seq/IUPACna.hpp
objects/seq/IUPACna_.hpp
objects/seq/Map_ext.hpp
objects/seq/Map_ext_.hpp
objects/seq/MolInfo.hpp
objects/seq/MolInfo_.hpp
objects/seq/NCBI2na.hpp
objects/seq/NCBI2na_.hpp
objects/seq/NCBI4na.hpp
objects/seq/NCBI4na_.hpp
objects/seq/NCBI8aa.hpp
objects/seq/NCBI8aa_.hpp
objects/seq/NCBI8na.hpp
objects/seq/NCBI8na_.hpp
objects/seq/NCBIeaa.hpp
objects/seq/NCBIeaa_.hpp
objects/seq/NCBIpaa.hpp
objects/seq/NCBIpaa_.hpp
objects/seq/NCBIpna.hpp
objects/seq/NCBIpna_.hpp
objects/seq/NCBIstdaa.hpp
objects/seq/NCBIstdaa_.hpp
objects/seq/Num_cont.hpp
objects/seq/Num_cont_.hpp
objects/seq/Num_enum.hpp
objects/seq/Num_enum_.hpp
objects/seq/Num_real.hpp
objects/seq/Num_real_.hpp
objects/seq/Num_ref.hpp
objects/seq/Num_ref_.hpp
objects/seq/Numbering.hpp
objects/seq/Numbering_.hpp
objects/seq/Pubdesc.hpp
objects/seq/Pubdesc_.hpp
objects/seq/Ref_ext.hpp
objects/seq/Ref_ext_.hpp
objects/seq/Seg_ext.hpp
objects/seq/Seg_ext_.hpp
objects/seq/Seq_annot.hpp
objects/seq/Seq_annot_.hpp
objects/seq/Seq_data.hpp
objects/seq/Seq_data_.hpp
objects/seq/Seq_descr.hpp
objects/seq/Seq_descr_.hpp
objects/seq/Seq_ext.hpp
objects/seq/Seq_ext_.hpp
objects/seq/Seq_hist.hpp
objects/seq/Seq_hist_.hpp
objects/seq/Seq_hist_rec.hpp
objects/seq/Seq_hist_rec_.hpp
objects/seq/Seq_inst.hpp
objects/seq/Seq_inst_.hpp
objects/seq/Seq_literal.hpp
objects/seq/Seq_literal_.hpp
objects/seq/Seqdesc.hpp
objects/seq/Seqdesc_.hpp
objects/seq/seqport_util.hpp
objects/seqalign/Dense_diag.hpp
objects/seqalign/Dense_diag_.hpp
objects/seqalign/Dense_seg.hpp
objects/seqalign/Dense_seg_.hpp
objects/seqalign/Packed_seg.hpp
objects/seqalign/Packed_seg_.hpp
objects/seqalign/Score.hpp
objects/seqalign/Score_.hpp
objects/seqalign/Score_set.hpp
objects/seqalign/Score_set_.hpp
objects/seqalign/Seq_align.hpp
objects/seqalign/Seq_align_.hpp
objects/seqalign/Seq_align_set.hpp
objects/seqalign/Seq_align_set_.hpp
objects/seqalign/Std_seg.hpp
objects/seqalign/Std_seg_.hpp
objects/seqblock/EMBL_block.hpp
objects/seqblock/EMBL_block_.hpp
objects/seqblock/EMBL_dbname.hpp
objects/seqblock/EMBL_dbname_.hpp
objects/seqblock/EMBL_xref.hpp
objects/seqblock/EMBL_xref_.hpp
objects/seqblock/GB_block.hpp
objects/seqblock/GB_block_.hpp
objects/seqblock/PDB_block.hpp
objects/seqblock/PDB_block_.hpp
objects/seqblock/PDB_replace.hpp
objects/seqblock/PDB_replace_.hpp
objects/seqblock/PIR_block.hpp
objects/seqblock/PIR_block_.hpp
objects/seqblock/PRF_block.hpp
objects/seqblock/PRF_block_.hpp
objects/seqblock/PRF_ExtraSrc.hpp
objects/seqblock/PRF_ExtraSrc_.hpp
objects/seqblock/SP_block.hpp
objects/seqblock/SP_block_.hpp
objects/seqcode/Seq_code_set.hpp
objects/seqcode/Seq_code_set_.hpp
objects/seqcode/Seq_code_table.hpp
objects/seqcode/Seq_code_table_.hpp
objects/seqcode/Seq_code_type.hpp
objects/seqcode/Seq_code_type_.hpp
objects/seqcode/Seq_map_table.hpp
objects/seqcode/Seq_map_table_.hpp
objects/seqfeat/BinomialOrgName.hpp
objects/seqfeat/BinomialOrgName_.hpp
objects/seqfeat/BioSource.hpp
objects/seqfeat/BioSource_.hpp
objects/seqfeat/Cdregion.hpp
objects/seqfeat/Cdregion_.hpp
objects/seqfeat/Code_break.hpp
objects/seqfeat/Code_break_.hpp
objects/seqfeat/Feat_id.hpp
objects/seqfeat/Feat_id_.hpp
objects/seqfeat/Gb_qual.hpp
objects/seqfeat/Gb_qual_.hpp
objects/seqfeat/Gene_ref.hpp
objects/seqfeat/Gene_ref_.hpp
objects/seqfeat/Genetic_code.hpp
objects/seqfeat/Genetic_code_.hpp
objects/seqfeat/Genetic_code_table.hpp
objects/seqfeat/Genetic_code_table_.hpp
objects/seqfeat/Imp_feat.hpp
objects/seqfeat/Imp_feat_.hpp
objects/seqfeat/MultiOrgName.hpp
objects/seqfeat/MultiOrgName_.hpp
objects/seqfeat/Org_ref.hpp
objects/seqfeat/Org_ref_.hpp
objects/seqfeat/OrgMod.hpp
objects/seqfeat/OrgMod_.hpp
objects/seqfeat/OrgName.hpp
objects/seqfeat/OrgName_.hpp
objects/seqfeat/PartialOrgName.hpp
objects/seqfeat/PartialOrgName_.hpp
objects/seqfeat/Prot_ref.hpp
objects/seqfeat/Prot_ref_.hpp
objects/seqfeat/RNA_ref.hpp
objects/seqfeat/RNA_ref_.hpp
objects/seqfeat/Rsite_ref.hpp
objects/seqfeat/Rsite_ref_.hpp
objects/seqfeat/Seq_feat.hpp
objects/seqfeat/Seq_feat_.hpp
objects/seqfeat/SeqFeatData.hpp
objects/seqfeat/SeqFeatData_.hpp
objects/seqfeat/SeqFeatXref.hpp
objects/seqfeat/SeqFeatXref_.hpp
objects/seqfeat/SubSource.hpp
objects/seqfeat/SubSource_.hpp
objects/seqfeat/TaxElement.hpp
objects/seqfeat/TaxElement_.hpp
objects/seqfeat/Trna_ext.hpp
objects/seqfeat/Trna_ext_.hpp
objects/seqfeat/Tx_evidence.hpp
objects/seqfeat/Tx_evidence_.hpp
objects/seqfeat/Txinit.hpp
objects/seqfeat/Txinit_.hpp
objects/seqloc/Giimport_id.hpp
objects/seqloc/Giimport_id_.hpp
objects/seqloc/Na_strand.hpp
objects/seqloc/Na_strand_.hpp
objects/seqloc/Packed_seqint.hpp
objects/seqloc/Packed_seqint_.hpp
objects/seqloc/Packed_seqpnt.hpp
objects/seqloc/Packed_seqpnt_.hpp
objects/seqloc/Patent_seq_id.hpp
objects/seqloc/Patent_seq_id_.hpp
objects/seqloc/PDB_mol_id.hpp
objects/seqloc/PDB_mol_id_.hpp
objects/seqloc/PDB_seq_id.hpp
objects/seqloc/PDB_seq_id_.hpp
objects/seqloc/Seq_bond.hpp
objects/seqloc/Seq_bond_.hpp
objects/seqloc/Seq_id.hpp
objects/seqloc/Seq_id_.hpp
objects/seqloc/Seq_interval.hpp
objects/seqloc/Seq_interval_.hpp
objects/seqloc/Seq_loc.hpp
objects/seqloc/Seq_loc_.hpp
objects/seqloc/Seq_loc_equiv.hpp
objects/seqloc/Seq_loc_equiv_.hpp
objects/seqloc/Seq_loc_mix.hpp
objects/seqloc/Seq_loc_mix_.hpp
objects/seqloc/Seq_point.hpp
objects/seqloc/Seq_point_.hpp
objects/seqloc/Textseq_id.hpp
objects/seqloc/Textseq_id_.hpp
objects/seqres/Byte_graph.hpp
objects/seqres/Byte_graph_.hpp
objects/seqres/Int_graph.hpp
objects/seqres/Int_graph_.hpp
objects/seqres/Real_graph.hpp
objects/seqres/Real_graph_.hpp
objects/seqres/Seq_graph.hpp
objects/seqres/Seq_graph_.hpp
objects/seqset/Bioseq_set.hpp
objects/seqset/Bioseq_set_.hpp
objects/seqset/Seq_entry.hpp
objects/seqset/Seq_entry_.hpp
objects/submit/Contact_info.hpp
objects/submit/Contact_info_.hpp
objects/submit/Seq_submit.hpp
objects/submit/Seq_submit_.hpp
objects/submit/Submit_block.hpp
objects/submit/Submit_block_.hpp

objects/scoremat/BlockProperty.hpp
objects/scoremat/BlockProperty_.hpp
objects/scoremat/CoreBlock.hpp
objects/scoremat/CoreBlock_.hpp
objects/scoremat/CoreDef.hpp
objects/scoremat/CoreDef_.hpp
objects/scoremat/FormatRpsDbParameters.hpp
objects/scoremat/FormatRpsDbParameters_.hpp
objects/scoremat/LoopConstraint.hpp
objects/scoremat/LoopConstraint_.hpp
objects/scoremat/Pssm.hpp
objects/scoremat/PssmFinalData.hpp
objects/scoremat/PssmFinalData_.hpp
objects/scoremat/PssmIntermediateData.hpp
objects/scoremat/PssmIntermediateData_.hpp
objects/scoremat/PssmParameters.hpp
objects/scoremat/PssmParameters_.hpp
objects/scoremat/PssmWithParameters.hpp
objects/scoremat/PssmWithParameters_.hpp
objects/scoremat/Pssm_.hpp

objects/blast/Blas_get_searc_resul_reply.hpp
objects/blast/Blas_get_searc_resul_reply_.hpp
objects/blast/Blas_get_searc_resul_reque.hpp
objects/blast/Blas_get_searc_resul_reque_.hpp

objects/blast/Blast4_cutoff.hpp
objects/blast/Blast4_cutoff_.hpp
objects/blast/Blast4_database.hpp
objects/blast/Blast4_database_.hpp
objects/blast/Blast4_database_info.hpp
objects/blast/Blast4_database_info_.hpp
objects/blast/Blast4_error.hpp
objects/blast/Blast4_error_.hpp
objects/blast/Blast4_error_code.hpp
objects/blast/Blast4_error_code_.hpp
objects/blast/Blast4_error_flags.hpp
objects/blast/Blast4_error_flags_.hpp
objects/blast/Blast4_finish_params_reply.hpp
objects/blast/Blast4_finish_params_reply_.hpp
objects/blast/Blast4_frame_type.hpp
objects/blast/Blast4_frame_type_.hpp
objects/blast/Blast4_get_databases_reply.hpp
objects/blast/Blast4_get_databases_reply_.hpp
objects/blast/Blast4_get_matrices_reply.hpp
objects/blast/Blast4_get_matrices_reply_.hpp
objects/blast/Blast4_get_parameter_reply.hpp
objects/blast/Blast4_get_parameter_reply_.hpp
objects/blast/Blast4_get_paramsets_reply.hpp
objects/blast/Blast4_get_paramsets_reply_.hpp
objects/blast/Blast4_get_programs_reply.hpp
objects/blast/Blast4_get_programs_reply_.hpp
objects/blast/Blast4_get_sequenc_request.hpp
objects/blast/Blast4_get_sequenc_request_.hpp
objects/blast/Blast4_get_sequences_reply.hpp
objects/blast/Blast4_get_sequences_reply_.hpp
objects/blast/Blast4_ka_block.hpp
objects/blast/Blast4_ka_block_.hpp
objects/blast/Blast4_mask.hpp
objects/blast/Blast4_mask_.hpp
objects/blast/Blast4_matrix_id.hpp
objects/blast/Blast4_matrix_id_.hpp
objects/blast/Blast4_parameter.hpp
objects/blast/Blast4_parameter_.hpp
objects/blast/Blast4_parameter_info.hpp
objects/blast/Blast4_parameter_info_.hpp
objects/blast/Blast4_parameters.hpp
objects/blast/Blast4_parameters_.hpp
objects/blast/Blast4_paramset_info.hpp
objects/blast/Blast4_paramset_info_.hpp
objects/blast/Blast4_phi_alignments.hpp
objects/blast/Blast4_phi_alignments_.hpp
objects/blast/Blast4_program_info.hpp
objects/blast/Blast4_program_info_.hpp
objects/blast/Blast4_queries.hpp
objects/blast/Blast4_queries_.hpp
objects/blast/Blast4_queue_search_reply.hpp
objects/blast/Blast4_queue_search_reply_.hpp
objects/blast/Blast4_queue_search_reques.hpp
objects/blast/Blast4_queue_search_reques_.hpp
objects/blast/Blast4_reply.hpp
objects/blast/Blast4_reply_.hpp
objects/blast/Blast4_reply_body.hpp
objects/blast/Blast4_reply_body_.hpp
objects/blast/Blast4_request.hpp
objects/blast/Blast4_request_.hpp
objects/blast/Blast4_request_body.hpp
objects/blast/Blast4_request_body_.hpp
objects/blast/Blast4_residue_type.hpp
objects/blast/Blast4_residue_type_.hpp
objects/blast/Blast4_seqtech.hpp
objects/blast/Blast4_seqtech_.hpp
objects/blast/Blast4_strand_type.hpp
objects/blast/Blast4_strand_type_.hpp
objects/blast/Blast4_subject.hpp
objects/blast/Blast4_subject_.hpp
objects/blast/Blast4_value.hpp
objects/blast/Blast4_value_.hpp
objects/blast/Blast_finish_params_reques.hpp
objects/blast/Blast_finish_params_reques_.hpp
objects/blast/blastclient.hpp
objects/blast/blastclient_.hpp

objects/id1/Entry_complexities.hpp
objects/id1/Entry_complexities_.hpp
objects/id1/id1_client.hpp
objects/id1/id1_client_.hpp
objects/id1/ID1blob_info.hpp
objects/id1/ID1blob_info_.hpp
objects/id1/ID1Seq_hist.hpp
objects/id1/ID1Seq_hist_.hpp
objects/id1/ID1SeqEntry_info.hpp
objects/id1/ID1SeqEntry_info_.hpp
objects/id1/ID1server_back.hpp
objects/id1/ID1server_back_.hpp
objects/id1/ID1server_debug.hpp
objects/id1/ID1server_debug_.hpp
objects/id1/ID1server_maxcomplex.hpp
objects/id1/ID1server_maxcomplex_.hpp
objects/id1/ID1server_request.hpp
objects/id1/ID1server_request_.hpp

objects/id2/id2_client.hpp
objects/id2/id2_client_.hpp
objects/id2/ID2_Error.hpp
objects/id2/ID2_Error_.hpp
// objects/seqsplit/ID2_Id_Range.hpp
// objects/seqsplit/ID2_Id_Range_.hpp
// objects/seqsplit/ID2_Interval.hpp
// objects/seqsplit/ID2_Interval_.hpp
// objects/seqsplit/ID2_Packed_Seq_ints.hpp
// objects/seqsplit/ID2_Packed_Seq_ints_.hpp
objects/id2/ID2_Param.hpp
objects/id2/ID2_Param_.hpp
objects/id2/ID2_Params.hpp
objects/id2/ID2_Params_.hpp
objects/id2/ID2_Reply.hpp
objects/id2/ID2_Reply_.hpp
objects/id2/ID2_Reply_Data.hpp
objects/id2/ID2_Reply_Data_.hpp
objects/id2/ID2_Reply_Get_Package.hpp
objects/id2/ID2_Reply_Get_Package_.hpp
objects/id2/ID2_Reply_Get_Seq_id.hpp
objects/id2/ID2_Reply_Get_Seq_id_.hpp
objects/id2/ID2_Request.hpp
objects/id2/ID2_Request_.hpp
objects/id2/ID2_Request_Get_Packages.hpp
objects/id2/ID2_Request_Get_Packages_.hpp
objects/id2/ID2_Request_Packet.hpp
objects/id2/ID2_Request_Packet_.hpp
// objects/seqsplit/ID2_Seq_loc.hpp
// objects/seqsplit/ID2_Seq_loc_.hpp
// objects/seqsplit/ID2_Seq_range.hpp
// objects/seqsplit/ID2_Seq_range_.hpp
// objects/seqsplit/ID2S_Bioseq_Info.hpp
// objects/seqsplit/ID2S_Bioseq_Info_.hpp
// objects/seqsplit/ID2S_Bioseqs_Info.hpp
// objects/seqsplit/ID2S_Bioseqs_Info_.hpp

// objects/seqsplit/ID2S_Chunk.hpp
// objects/seqsplit/ID2S_Chunk_.hpp
// objects/seqsplit/ID2S_Chunk_Content.hpp
// objects/seqsplit/ID2S_Chunk_Content_.hpp
// objects/seqsplit/ID2S_Chunk_Data.hpp
// objects/seqsplit/ID2S_Chunk_Data_.hpp
// objects/seqsplit/ID2S_Chunk_Id.hpp
// objects/seqsplit/ID2S_Chunk_Id_.hpp
// objects/seqsplit/ID2S_Chunk_Info.hpp
// objects/seqsplit/ID2S_Chunk_Info_.hpp
// objects/seqsplit/ID2S_Feat_type_Info.hpp
// objects/seqsplit/ID2S_Feat_type_Info_.hpp

objects/id2/ID2S_Reply_Get_Chunk.hpp
objects/id2/ID2S_Reply_Get_Chunk_.hpp
objects/id2/ID2S_Request_Get_Chunks.hpp
objects/id2/ID2S_Request_Get_Chunks_.hpp

// objects/seqsplit/ID2S_Seq_annot_Info.hpp
// objects/seqsplit/ID2S_Seq_annot_Info_.hpp
// objects/seqsplit/ID2S_Seq_annot_place_Info.hpp
// objects/seqsplit/ID2S_Seq_annot_place_Info_.hpp
// objects/seqsplit/ID2S_Seq_assembly_Info.hpp
// objects/seqsplit/ID2S_Seq_assembly_Info_.hpp
// objects/seqsplit/ID2S_Seq_data_Info.hpp
// objects/seqsplit/ID2S_Seq_data_Info_.hpp
// objects/seqsplit/ID2S_Seq_descr_Info.hpp
// objects/seqsplit/ID2S_Seq_descr_Info_.hpp
// objects/seqsplit/ID2S_Seq_map_Info.hpp
// objects/seqsplit/ID2S_Seq_map_Info_.hpp
// objects/seqsplit/ID2S_Sequence_Split_Info.hpp
// objects/seqsplit/ID2S_Sequence_Split_Info_.hpp
// objects/seqsplit/ID2S_Split_Info.hpp
// objects/seqsplit/ID2S_Split_Info_.hpp

objects/id2/ID2_Blob_Id.hpp
objects/id2/ID2_Blob_Id_.hpp
objects/id2/ID2_Blob_Seq_id.hpp
objects/id2/ID2_Blob_Seq_id_.hpp
objects/id2/ID2_Blob_Seq_ids.hpp
objects/id2/ID2_Blob_Seq_ids_.hpp
objects/id2/ID2_Request_Get_Blob_Id.hpp
objects/id2/ID2_Request_Get_Blob_Id_.hpp
objects/id2/ID2_Request_Get_Blob_Info.hpp
objects/id2/ID2_Request_Get_Blob_Info_.hpp
objects/id2/ID2_Get_Blob_Details.hpp
objects/id2/ID2_Get_Blob_Details_.hpp
objects/id2/ID2_Seq_id.hpp
objects/id2/ID2_Seq_id_.hpp
objects/id2/ID2_Request_Get_Seq_id.hpp
objects/id2/ID2_Request_Get_Seq_id_.hpp
objects/id2/ID2_Request_ReGet_Blob.hpp
objects/id2/ID2_Request_ReGet_Blob_.hpp
objects/id2/ID2_Reply_Get_Blob_Id.hpp
objects/id2/ID2_Reply_Get_Blob_Id_.hpp
objects/id2/ID2_Reply_Get_Blob_Seq_ids.hpp
objects/id2/ID2_Reply_Get_Blob_Seq_ids_.hpp
objects/id2/ID2_Reply_Get_Blob.hpp
objects/id2/ID2_Reply_Get_Blob_.hpp
objects/id2/ID2S_Reply_Get_Split_Info.hpp
objects/id2/ID2S_Reply_Get_Split_Info_.hpp
objects/id2/ID2_Reply_ReGet_Blob.hpp
objects/id2/ID2_Reply_ReGet_Blob_.hpp

objects/seqsplit/ID2S_Bioseq_Ids.hpp
objects/seqsplit/ID2S_Bioseq_Ids_.hpp
objects/seqsplit/ID2S_Bioseq_Info.hpp
objects/seqsplit/ID2S_Bioseq_Info_.hpp
objects/seqsplit/ID2S_Bioseq_place_Info.hpp
objects/seqsplit/ID2S_Bioseq_place_Info_.hpp
objects/seqsplit/ID2S_Bioseq_set_Ids.hpp
objects/seqsplit/ID2S_Bioseq_set_Ids_.hpp
objects/seqsplit/ID2S_Bioseqs_Info.hpp
objects/seqsplit/ID2S_Bioseqs_Info_.hpp
objects/seqsplit/ID2S_Chunk.hpp
objects/seqsplit/ID2S_Chunk_.hpp
objects/seqsplit/ID2S_Chunk_Content.hpp
objects/seqsplit/ID2S_Chunk_Content_.hpp
objects/seqsplit/ID2S_Chunk_Data.hpp
objects/seqsplit/ID2S_Chunk_Data_.hpp
objects/seqsplit/ID2S_Chunk_Id.hpp
objects/seqsplit/ID2S_Chunk_Id_.hpp
objects/seqsplit/ID2S_Chunk_Info.hpp
objects/seqsplit/ID2S_Chunk_Info_.hpp
objects/seqsplit/ID2S_Feat_type_Info.hpp
objects/seqsplit/ID2S_Feat_type_Info_.hpp
objects/seqsplit/ID2S_Gi_Interval.hpp
objects/seqsplit/ID2S_Gi_Interval_.hpp
objects/seqsplit/ID2S_Gi_Ints.hpp
objects/seqsplit/ID2S_Gi_Ints_.hpp
objects/seqsplit/ID2S_Gi_Range.hpp
objects/seqsplit/ID2S_Gi_Range_.hpp
objects/seqsplit/ID2S_Interval.hpp
objects/seqsplit/ID2S_Interval_.hpp
objects/seqsplit/ID2S_Seq_annot_Info.hpp
objects/seqsplit/ID2S_Seq_annot_Info_.hpp
objects/seqsplit/ID2S_Seq_annot_place_Info.hpp
objects/seqsplit/ID2S_Seq_annot_place_Info_.hpp
objects/seqsplit/ID2S_Seq_assembly_Info.hpp
objects/seqsplit/ID2S_Seq_assembly_Info_.hpp
objects/seqsplit/ID2S_Seq_data_Info.hpp
objects/seqsplit/ID2S_Seq_data_Info_.hpp
objects/seqsplit/ID2S_Seq_descr_Info.hpp
objects/seqsplit/ID2S_Seq_descr_Info_.hpp
objects/seqsplit/ID2S_Seq_id_Interval.hpp
objects/seqsplit/ID2S_Seq_id_Interval_.hpp
objects/seqsplit/ID2S_Seq_id_Ints.hpp
objects/seqsplit/ID2S_Seq_id_Ints_.hpp
objects/seqsplit/ID2S_Seq_loc.hpp
objects/seqsplit/ID2S_Seq_loc_.hpp
objects/seqsplit/ID2S_Seq_map_Info.hpp
objects/seqsplit/ID2S_Seq_map_Info_.hpp
objects/seqsplit/ID2S_Sequence_Piece.hpp
objects/seqsplit/ID2S_Sequence_Piece_.hpp
objects/seqsplit/ID2S_Split_Info.hpp
objects/seqsplit/ID2S_Split_Info_.hpp

objects/taxon1/taxon1.hpp
objects/taxon1/taxon1__.hpp
objects/taxon1/Taxon1_data.hpp
objects/taxon1/Taxon1_data_.hpp
objects/taxon1/Taxon1_error.hpp
objects/taxon1/Taxon1_error_.hpp
objects/taxon1/Taxon1_info.hpp
objects/taxon1/Taxon1_info_.hpp
objects/taxon1/Taxon1_name.hpp
objects/taxon1/Taxon1_name_.hpp
objects/taxon1/Taxon1_req.hpp
objects/taxon1/Taxon1_req_.hpp
objects/taxon1/Taxon1_resp.hpp
objects/taxon1/Taxon1_resp_.hpp
objects/taxon1/Taxon2_data.hpp
objects/taxon1/Taxon2_data_.hpp
objects/blastdb/Blast_def_line.hpp
objects/blastdb/Blast_def_line_.hpp
objects/blastdb/Blast_def_line_set.hpp
objects/blastdb/Blast_def_line_set_.hpp
objects/blastdb/defline_extra.hpp

// seqtest

objects/seqtest/SeqTestResults.hpp
objects/seqtest/SeqTestResults_.hpp
objects/seqtest/Seq_test_result.hpp
objects/seqtest/Seq_test_result_.hpp
objects/seqtest/Seq_test_result_set.hpp
objects/seqtest/Seq_test_result_set_.hpp

%%

// Need to specialize std::vector for enums
namespace std {
#ifdef SWIGPYTHON
specialize_std_vector(ncbi::objects::ENa_strand, PyInt_Check, \
                      PyInt_AsLong, PyInt_FromLong);
#endif
}
%template(vector_ENa_strand) std::vector<ncbi::objects::ENa_strand>;


%extend ncbi::CSeqTestContext {
public:
#ifdef SWIGPYTHON
    void __setitem__(const string& key, const string& value) {
        (*self)[key] = value;
    }
#endif
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/05/11 21:27:35  jcherry
 * Initial version
 *
 * ===========================================================================
 */
