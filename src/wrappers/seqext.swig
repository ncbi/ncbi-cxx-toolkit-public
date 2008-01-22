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

// Nested classes fixes
%extend ncbi::objects::CAlnMap_CAlnChunkVec {
public:
    CAlnMap_CAlnChunkVec(const ncbi::objects::CAlnMap& aln_map,
                         ncbi::objects::CAlnMap::TNumrow row) {
        return new CAlnMap_CAlnChunkVec(aln_map, row);
    }
    ncbi::CConstRef<ncbi::objects::CAlnMap::CAlnChunk>
    operator[] (ncbi::objects::CAlnMap::TNumchunk i) const {
        return self->operator[](i);
    }
    ncbi::objects::CAlnMap::TNumchunk size() const {
        return self->size();
    }
}
%ignore *::CAlnMap_CAlnChunkVec::CAlnMap_CAlnChunkVec;
%ignore *::CAlnMap_CAlnChunkVec::operator[];
%ignore *::CAlnMap_CAlnChunkVec::size;

%extend ncbi::objects::CAlnMap_CAlnChunk {
public:
    ncbi::objects::CAlnMap::TSegTypeFlags GetType() const {
        return self->GetType();
    }
    ncbi::objects::CAlnMap::CAlnChunk&
    SetType(ncbi::objects::CAlnMap::TSegTypeFlags type_flags) {
        return self->SetType(type_flags);
    }
    const ncbi::objects::CAlnMap::TSignedRange& GetRange() const {
        return self->GetRange();
    }
    const ncbi::objects::CAlnMap::TSignedRange& GetAlnRange() const {
        return self->GetAlnRange();
    }
}
%ignore *::CAlnMap_CAlnChunk::GetType;
%ignore *::CAlnMap_CAlnChunk::SetType;
%ignore *::CAlnMap_CAlnChunk::GetRange;
%ignore *::CAlnMap_CAlnChunk::GetAlnRange;

// These cause trouble due to lack of export specifiers
// for themselves or CTSE_Scope*Locker.
// We do not want to wrap them anyway (declared in objmgr/impl).
%ignore ncbi::objects::SSeq_id_ScopeInfo;
%ignore ncbi::objects::CPriority_I;
%ignore ncbi::objects::CScope_Impl;
%ignore ncbi::objects::CDataSource_ScopeInfo;
%ignore ncbi::objects::CTSE_ScopeInfo;
%ignore ncbi::objects::SSeqMatch_Scope;

// Flat file stream inserter
%ignore *::operator<<(CFlatItemOStream& os, CConstRef<IFlatItem>& item);
%extend ncbi::objects::CFlatItemOStream {
    // like new signature of non-member operator
    ncbi::objects::CFlatItemOStream&
    __lshift__(CConstRef<IFlatItem>& item) {
        return *self << item;
    }
    // old, more convenient signature we've supported for a while
    ncbi::objects::CFlatItemOStream&
    __lshift__(const IFlatItem* item) {
        CConstRef<IFlatItem> item_ref(item);
        return *self << item_ref;
    }
};

// Protected typedef (TStyle)
%ignore *::CFlatStringQVal::CFlatStringQVal;
%ignore *::CFlatStringListQVal::CFlatStringListQVal;

// Protected typedef (TObject)
%ignore ncbi::objects::CTSE_Handle::x_GetScopeInfo;

// ReadFasta parameter
%template(vector_CConstRef_CSeq_loc)
    std::vector<ncbi::CConstRef<ncbi::objects::CSeq_loc> >;

#ifdef SWIGPYTHON
%rename(is_) ncbi::CAgpReader::is;  // "is" is a reserved word in Python
#endif

// CBlob_id stream inserter
%ignore ncbi::objects::operator<<(CNcbiOstream& ostr, const CBlob_id& id);
#ifdef SWIGPYTHON
%extend ncbi::objects::CBlob_id {
    std::ostream&
    __rlshift__(std::ostream& ostr) {
        return ostr << *self;
    }
};
#endif

// alnmgr stream inserters
%ignore ncbi::operator<<;
#ifdef SWIGPYTHON
%define ADD_STREAM_INSERTER(class_name)
%extend class_name {
    std::ostream&
    __rlshift__(std::ostream& ostr) {
        return ostr << *self;
    }
};
%enddef
//ADD_STREAM_INSERTER(CPairwiseAln::TAlnRng)  // CAlignRange<TPos>
//ADD_STREAM_INSERTER(CPairwiseAln::EFlags)
//ADD_STREAM_INSERTER(TAlnSeqIdIRef)  // CIRef<IAlnSeqId>
ADD_STREAM_INSERTER(ncbi::CPairwiseAln)
ADD_STREAM_INSERTER(ncbi::CAnchoredAln)
#endif

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

objtools/alnmgr/aln_asn_reader.hpp
objtools/alnmgr/aln_builders.hpp
objtools/alnmgr/aln_container.hpp
objtools/alnmgr/aln_converters.hpp
objtools/alnmgr/aln_explorer.hpp
objtools/alnmgr/aln_generators.hpp
objtools/alnmgr/aln_rng_coll_oper.hpp
objtools/alnmgr/aln_seqid.hpp
objtools/alnmgr/aln_serial.hpp
objtools/alnmgr/aln_stats.hpp
objtools/alnmgr/aln_tests.hpp
objtools/alnmgr/aln_user_options.hpp
objtools/alnmgr/alndiag.hpp
objtools/alnmgr/alnexception.hpp
objtools/alnmgr/alnmap.hpp
objtools/alnmgr/alnmatch.hpp
objtools/alnmgr/alnmerger.hpp
objtools/alnmgr/alnmix.hpp
objtools/alnmgr/alnpos_ci.hpp
objtools/alnmgr/alnsegments.hpp
objtools/alnmgr/alnseq.hpp
objtools/alnmgr/alnvec.hpp
objtools/alnmgr/pairwise_aln.hpp
objtools/alnmgr/seqid_comp.hpp
objtools/alnmgr/seqids_extractor.hpp
objtools/alnmgr/sparse_aln.hpp
objtools/alnmgr/sparse_ci.hpp
objtools/alnmgr/task_progress.hpp

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
//objmgr/impl/prefetch_impl.hpp
objmgr/impl/prefetch_manager_impl.hpp
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
objmgr/impl/tse_assigner.hpp
objmgr/impl/tse_chunk_info.hpp
objmgr/impl/tse_info.hpp
objmgr/impl/tse_info_object.hpp
objmgr/impl/tse_loadlock.hpp
objmgr/impl/tse_lock.hpp

//objmgr/prefetch.hpp
objmgr/prefetch_actions.hpp
objmgr/prefetch_manager.hpp
objmgr/align_ci.hpp
objmgr/annot_ci.hpp
objmgr/annot_name.hpp
objmgr/annot_selector.hpp
objmgr/annot_type_selector.hpp
objmgr/annot_types_ci.hpp
objmgr/bioseq_ci.hpp
objmgr/bioseq_handle.hpp
objmgr/bioseq_set_handle.hpp
objmgr/blob_id.hpp
objmgr/data_loader.hpp
objmgr/data_loader_factory.hpp
objmgr/edit_saver.hpp
objmgr/feat_ci.hpp
objmgr/graph_ci.hpp
objmgr/object_manager.hpp
objmgr/objmgr_exception.hpp
objmgr/objmgr_version.hpp
objmgr/scope.hpp
objmgr/scope_transaction.hpp
objmgr/seq_align_handle.hpp
objmgr/seq_annot_ci.hpp
objmgr/seq_annot_handle.hpp
objmgr/seq_descr_ci.hpp
objmgr/seq_entry_ci.hpp
objmgr/seq_entry_handle.hpp
objmgr/seq_feat_handle.hpp
objmgr/seq_graph_handle.hpp
objects/seq/seq_id_handle.hpp
objects/seq/seq_id_mapper.hpp
objects/seq/seq_loc_mapper_base.hpp
objmgr/seq_loc_mapper.hpp
objmgr/seq_map.hpp
objmgr/seq_map_ci.hpp
objmgr/seq_vector.hpp
objmgr/seq_vector_ci.hpp
objmgr/seqdesc_ci.hpp
// won't compile (nothing must #include this)
//objmgr/tse_ci.hpp
objmgr/tse_handle.hpp
objmgr/util/feature.hpp
// non-inline function definition in header; messes up splitting
//objmgr/util/genbank.hpp
objmgr/util/obj_sniff.hpp
objmgr/util/sequence.hpp
objmgr/util/weight.hpp
objmgr/util/seq_loc_util.hpp

objtools/cleanup/cleanup.hpp
objtools/cleanup/cleanup_change.hpp

objtools/edit/edit_exception.hpp
objtools/edit/promote.hpp
objtools/edit/seq_entry_edit.hpp

objtools/readers/agp_read.hpp
objtools/readers/agp_util.hpp
objtools/readers/aln_reader.hpp
objtools/readers/cigar.hpp
objtools/readers/fasta.hpp
objtools/readers/featuredump.hpp
objtools/readers/getfeature.hpp
objtools/readers/gff_reader.hpp
objtools/readers/glimmer_reader.hpp
objtools/readers/phrap.hpp
objtools/readers/reader_exception.hpp
objtools/readers/readfeat.hpp
objtools/readers/rm_reader.hpp
objtools/readers/seqdb/seqdb.hpp
objtools/readers/seqdb/seqdbcommon.hpp
objtools/readers/seqdb/seqdbexpert.hpp
objtools/writers/agp_write.hpp
objtools/writers/writer_exception.hpp
objtools/writers/writedb/writedb.hpp
objtools/writers/writedb/writedb_error.hpp
objtools/manip/sage_manip.hpp

objtools/format/context.hpp
objtools/format/embl_formatter.hpp
objtools/format/embl_gather.hpp
objtools/format/flat_expt.hpp
objtools/format/flat_file_config.hpp
objtools/format/flat_file_generator.hpp
objtools/format/format_item_ostream.hpp
objtools/format/formatter.hpp
objtools/format/ftable_formatter.hpp
objtools/format/ftable_gather.hpp
objtools/format/gather_items.hpp
objtools/format/gbseq_formatter.hpp
objtools/format/genbank_formatter.hpp
objtools/format/genbank_gather.hpp
objtools/format/gff3_formatter.hpp
objtools/format/gff_formatter.hpp
objtools/format/gff_gather.hpp
objtools/format/item_formatter.hpp
objtools/format/item_ostream.hpp
objtools/format/ostream_text_ostream.hpp
objtools/format/text_ostream.hpp
objtools/format/items/accession_item.hpp
objtools/format/items/alignment_item.hpp
objtools/format/items/basecount_item.hpp
objtools/format/items/comment_item.hpp
objtools/format/items/contig_item.hpp
objtools/format/items/ctrl_items.hpp
objtools/format/items/date_item.hpp
objtools/format/items/dbsource_item.hpp
objtools/format/items/defline_item.hpp
objtools/format/items/feature_item.hpp
objtools/format/items/flat_qual_slots.hpp
objtools/format/items/flat_seqloc.hpp
objtools/format/items/gap_item.hpp
objtools/format/items/genome_item.hpp
objtools/format/items/genome_project_item.hpp
objtools/format/items/item.hpp
objtools/format/items/item_base.hpp
objtools/format/items/keywords_item.hpp
objtools/format/items/locus_item.hpp
objtools/format/items/origin_item.hpp
objtools/format/items/primary_item.hpp
objtools/format/items/qualifiers.hpp
objtools/format/items/reference_item.hpp
objtools/format/items/segment_item.hpp
objtools/format/items/sequence_item.hpp
objtools/format/items/source_item.hpp
objtools/format/items/version_item.hpp
objtools/format/items/wgs_item.hpp

%%

// CSeqVector_CI is not a smart pointer (no overloaded operator->),
// so this must be done to provide __deref__.
%rename(__deref__) ncbi::objects::CSeqVector_CI::operator*;

%extend ncbi::objects::CSeqVector {
public:
#ifdef SWIGPYTHON
    std::string __getslice__(ssize_t i, ssize_t j) const {
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


// Returned by CObjectsSniffer::GetTopLevelMap
// Must go in third section to be below nested #define in .cpp file
%template(vector_CObjectsSniffer_SObjectDescription)
    std::vector<ncbi::objects::CObjectsSniffer_SObjectDescription>;


%template(TAlnIdMap)
ncbi::CAlnIdMap<std::vector<const ncbi::objects::CSeq_align*>,
                ncbi::TIdExtract>;
%template(TIdExtract) ncbi::CAlnSeqIdsExtract<ncbi::CAlnSeqId>;
%template(TAlnStats) ncbi::CAlnStats<ncbi::TAlnIdMap>;
