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
 * File Description:  SWIG interface file for seqext
 *
 */


// CObjReaderParseException inherits from this
%template(CParseTemplException_CObjReaderException)
    ncbi::CParseTemplException<ncbi::CObjReaderException>;

class CAlnMap_CAlnChunkVec;
%template() ncbi::CRef<CAlnMap_CAlnChunkVec>;

%%

// Obsolescent
// objtools/flat/flat_context.hpp
// objtools/flat/flat_ddbj_formatter.hpp
// objtools/flat/flat_embl_formatter.hpp
// objtools/flat/flat_feature.hpp
// objtools/flat/flat_formatter.hpp
// objtools/flat/flat_gbseq_formatter.hpp
// objtools/flat/flat_gff_formatter.hpp
// objtools/flat/flat_head.hpp
// objtools/flat/flat_item.hpp
// objtools/flat/flat_items.hpp
// objtools/flat/flat_loc.hpp
// objtools/flat/flat_ncbi_formatter.hpp
// objtools/flat/flat_qual_slots.hpp
// objtools/flat/flat_quals.hpp
// objtools/flat/flat_reference.hpp
// objtools/flat/flat_table_formatter.hpp
// objtools/flat/flat_text_formatter.hpp

objtools/alnmgr/alnexception.hpp
objtools/alnmgr/alnmap.hpp
objtools/alnmgr/alnmix.hpp
objtools/alnmgr/alnvec.hpp
objmgr/impl/annot_collector.hpp
objmgr/impl/annot_object.hpp
objmgr/impl/annot_object_index.hpp
objmgr/impl/annot_type_index.hpp
objmgr/impl/bioseq_base_info.hpp
objmgr/impl/bioseq_info.hpp
objmgr/impl/bioseq_set_info.hpp
objmgr/impl/data_source.hpp
objmgr/impl/handle_range.hpp
objmgr/impl/handle_range_map.hpp
objmgr/impl/mutex_pool.hpp
objmgr/prefetch.hpp
objmgr/impl/prefetch_impl.hpp
objmgr/impl/priority.hpp
objmgr/impl/scope_impl.hpp
objmgr/impl/scope_info.hpp
objmgr/impl/seq_align_mapper.hpp
objmgr/impl/seq_annot_info.hpp
objmgr/impl/seq_entry_info.hpp
objmgr/impl/seq_loc_cvt.hpp
objmgr/impl/seq_vector_cvt.hpp
objmgr/impl/seq_vector_cvt_gen.hpp
objmgr/impl/snp_annot_info.hpp
objmgr/impl/synonyms.hpp
objmgr/impl/tse_chunk_info.hpp
objmgr/impl/tse_info.hpp
objmgr/impl/tse_info_object.hpp
objmgr/impl/tse_loadlock.hpp
objmgr/impl/tse_lock.hpp
objmgr/align_ci.hpp
objmgr/annot_ci.hpp
objmgr/annot_selector.hpp
objmgr/annot_type_selector.hpp
objmgr/annot_types_ci.hpp
objmgr/bioseq_ci.hpp
objmgr/bioseq_handle.hpp
objmgr/bioseq_set_handle.hpp
objmgr/data_loader.hpp
objmgr/data_loader_factory.hpp
objmgr/feat_ci.hpp
objmgr/graph_ci.hpp
objmgr/object_manager.hpp
objmgr/objmgr_exception.hpp
objmgr/scope.hpp
objmgr/seq_annot_ci.hpp
objmgr/seq_annot_handle.hpp
objmgr/seq_descr_ci.hpp
objmgr/seq_entry_ci.hpp
objmgr/seq_entry_handle.hpp
objects/seq/seq_id_handle.hpp
objects/seq/seq_id_mapper.hpp
objmgr/seq_loc_mapper.hpp
objmgr/seq_map.hpp
objmgr/seq_map_ci.hpp
objmgr/seq_map_ext.hpp
objmgr/seq_vector.hpp
objmgr/seq_vector_ci.hpp
objmgr/seqdesc_ci.hpp
// won't compile (nothing must #include this)
//objmgr/tse_ci.hpp
objmgr/util/feature.hpp
// non-inline function definition in header; messes up splitting
//objmgr/util/genbank.hpp
objmgr/util/obj_sniff.hpp
objmgr/util/sequence.hpp
objmgr/util/weight.hpp
objmgr/util/seq_loc_util.hpp
objtools/readers/agp_read.hpp
objtools/readers/aln_reader.hpp
objtools/readers/fasta.hpp
objtools/readers/gff_reader.hpp
objtools/readers/reader_exception.hpp
objtools/readers/readfeat.hpp
// CSeqDBIter copy ctor
//objtools/readers/seqdb/seqdb.hpp
//objtools/readers/seqdb/seqdbcommon.hpp
objtools/writers/agp_write.hpp
objtools/manip/sage_manip.hpp

// objtools/format/items/accession_item.hpp
// objtools/format/items/basecount_item.hpp
// objtools/format/items/comment_item.hpp
// objtools/format/items/contig_item.hpp
// objtools/format/items/ctrl_items.hpp
// objtools/format/items/date_item.hpp
// objtools/format/items/dbsource_item.hpp
// objtools/format/items/defline_item.hpp
// objtools/format/items/endsection_item.hpp
// objtools/format/items/feature_item.hpp
// objtools/format/items/flat_qual_slots.hpp
// objtools/format/items/flat_seqloc.hpp
// objtools/format/items/genome_item.hpp
// objtools/format/items/item.hpp
// objtools/format/items/item_base.hpp
// objtools/format/items/keywords_item.hpp
// objtools/format/items/locus_item.hpp
// objtools/format/items/origin_item.hpp
// objtools/format/items/primary_item.hpp
// objtools/format/items/qualifiers.hpp
// objtools/format/items/reference_item.hpp
// objtools/format/items/segment_item.hpp
// objtools/format/items/sequence_item.hpp
// objtools/format/items/source_item.hpp
// objtools/format/items/version_item.hpp
// objtools/format/items/wgs_item.hpp
// objtools/format/context.hpp
// objtools/format/embl_formatter.hpp
// objtools/format/embl_gather.hpp
// objtools/format/flat_expt.hpp
// objtools/format/flat_file_config.hpp
// objtools/format/flat_file_generator.hpp
// objtools/format/format_item_ostream.hpp
// objtools/format/formatter.hpp
// objtools/format/gather_items.hpp
// objtools/format/gbseq_formatter.hpp
// objtools/format/genbank_formatter.hpp
// objtools/format/genbank_gather.hpp
// objtools/format/gff_formatter.hpp
// objtools/format/gff_gather.hpp
// objtools/format/item_formatter.hpp
// objtools/format/item_ostream.hpp
// objtools/format/ostream_text_ostream.hpp
// objtools/format/text_ostream.hpp
// objtools/format/ftable_formatter.hpp
// objtools/format/ftable_gather.hpp

%%

// CSeqVector_CI is not a smart pointer (no overloaded operator->),
// so this must be done to provide __deref__.
%rename(__deref__) ncbi::objects::CSeqVector_CI::operator*;

%extend ncbi::objects::CSeqVector {
public:
#ifdef SWIGPYTHON
    std::string __getslice__(int i, int j) const {
#else
    std::string getslice(int i, int j) const {
#endif
        // make it Python-like:
        // negative indices count from last element backwards,
        // and slices extending out of range get truncated.
        int size = self->size();
        if (i < 0) {
            i = size + i;
        }
        if (j < 0) {
            j = size + j;
        }
        if (i < 0) {
            i = 0;
        }
        if (j > size) {
            j = size;
        }
        std::string rv;
        rv.reserve(j - i);
        self->GetSeqData(i, j, rv);
        return rv;
    }
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/05/11 21:27:35  jcherry
 * Initial version
 *
 * ===========================================================================
 */
