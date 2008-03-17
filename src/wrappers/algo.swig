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
 * File Description:  SWIG interface file for algo
 *
 */


// CSeqMatch::SMatchNcbi8na, a nested template struct, completely
// confuses SWIG.  The result is code that g++ can compile but
// MSVC cannot.  Therefore, don't let SWIG see the contents of
// algo/sequence/seq_match.hpp.
#define GUI_CORE_ALGO_BASIC___SEQ_MATCH__HPP

// blast::TSeqLocVector
%template(vector_SSeqLoc) std::vector<SSeqLoc>;
// blast::TSeqAlignVector
VECTOR_CREF(vector_CRef_CSeq_align_set, ncbi::objects::CSeq_align_set);

// Returned by CContigAssembly::Align
VECTOR_CREF(vector_CRef_CSeq_align, ncbi::objects::CSeq_align);

%template(vector_CNWAligner_ETranscriptSymbol)
    std::vector<ncbi::CNWAligner::ETranscriptSymbol>;

// Take ownership of some objects returned by pointer
%newobject ncbi::CDistMethods::FastMeTree;
%newobject ncbi::CDistMethods::NjTree;
%newobject ncbi::ReadNewickTree;

// SWIG thinks this is a function
%ignore ncbi::blast::Version;

// Work-around for private typedef issue
%extend ncbi::CSymDustMasker {
    ncbi::CRef<ncbi::objects::CPacked_seqint>
    GetMaskedInts(ncbi::objects::CSeq_id& seq_id,
                  const ncbi::objects::CSeqVector& seq ) {
        return self->GetMaskedInts(seq_id, seq);
    }
}
%ignore ncbi::CSymDustMasker::GetMaskedInts(objects::CSeq_id& seq_id,
                                            const sequence_type& seq);

%ignore ncbi::gnomon::operator>>;
%ignore ncbi::gnomon::operator<<;


%%

algo/align/nw/align_exception.hpp
algo/align/nw/mm_aligner.hpp
algo/align/nw/nw_aligner.hpp
algo/align/nw/nw_formatter.hpp
algo/align/nw/nw_spliced_aligner.hpp
algo/align/nw/nw_spliced_aligner16.hpp
algo/align/nw/nw_spliced_aligner32.hpp
algo/align/nw/nw_band_aligner.hpp
algo/align/nw/nw_pssm_aligner.hpp
// prosplign not built in public build as of 8/21/07
//algo/align/prosplign/prosplign.hpp
//algo/align/prosplign/prosplign_exception.hpp
algo/align/splign/splign.hpp
algo/align/splign/splign_formatter.hpp
algo/align/splign/splign_cmdargs.hpp
// "temporary" code, has problems
//algo/align/splign/splign_hit.hpp
algo/align/splign/splign_simple.hpp
algo/align/contig_assembly/contig_assembly.hpp
algo/align/util/algo_align_util_exceptions.hpp
algo/align/util/align_shadow.hpp
algo/align/util/blast_tabular.hpp
// compartment_finder.hpp consists mainly of templates, including problematic
// nested template class
//algo/align/util/compartment_finder.hpp
algo/align/util/hit_comparator.hpp
algo/align/util/hit_filter.hpp
algo/align/util/score_builder.hpp

algo/sequence/align_group.hpp
algo/sequence/annot_compare.hpp
algo/sequence/antigenic.hpp
algo/sequence/coiled_coil.hpp
algo/sequence/consensus_splice.hpp
algo/sequence/cpg.hpp
algo/sequence/find_pattern.hpp
algo/sequence/gene_model.hpp
algo/sequence/make_cdr_prods.hpp
algo/sequence/nuc_prop.hpp
algo/sequence/orf.hpp
algo/sequence/prot_prop.hpp
algo/sequence/regexp_loc.hpp
algo/sequence/restriction.hpp
algo/sequence/seq_match.hpp
algo/sequence/signal_seq.hpp

algo/blast/api/bl2seq.hpp
algo/blast/api/blast_advprot_options.hpp
algo/blast/api/blast_aux.hpp
algo/blast/api/blast_exception.hpp
algo/blast/api/blast_mtlock.hpp
algo/blast/api/blast_nucl_options.hpp
algo/blast/api/blast_options.hpp
algo/blast/api/blast_options_builder.hpp
algo/blast/api/blast_options_handle.hpp
algo/blast/api/blast_prot_options.hpp
algo/blast/api/blast_results.hpp
algo/blast/api/blast_rps_options.hpp
algo/blast/api/blast_seqinfosrc.hpp
algo/blast/api/blast_seqinfosrc_aux.hpp
algo/blast/api/blast_types.hpp
algo/blast/api/blastx_options.hpp
algo/blast/api/disc_nucl_options.hpp
algo/blast/api/effsearchspace_calc.hpp
algo/blast/api/hspstream_queue.hpp
algo/blast/api/local_blast.hpp
algo/blast/api/local_search.hpp
algo/blast/api/objmgr_query_data.hpp
algo/blast/api/objmgrfree_query_data.hpp
algo/blast/api/phiblast_nucl_options.hpp
algo/blast/api/phiblast_prot_options.hpp
algo/blast/api/prelim_stage.hpp
algo/blast/api/psi_pssm_input.hpp
algo/blast/api/psibl2seq.hpp
algo/blast/api/psiblast.hpp
algo/blast/api/psiblast_iteration.hpp
algo/blast/api/psiblast_options.hpp
algo/blast/api/pssm_engine.hpp
algo/blast/api/pssm_input.hpp
algo/blast/api/query_data.hpp
algo/blast/api/remote_blast.hpp
algo/blast/api/remote_search.hpp
algo/blast/api/remote_services.hpp
algo/blast/api/rps_aux.hpp
algo/blast/api/rpstblastn_options.hpp
algo/blast/api/seqinfosrc_seqdb.hpp
algo/blast/api/seqinfosrc_seqvec.hpp
algo/blast/api/seqsrc_multiseq.hpp
algo/blast/api/seqsrc_seqdb.hpp
algo/blast/api/setup_factory.hpp
algo/blast/api/sseqloc.hpp
algo/blast/api/tblastn_options.hpp
algo/blast/api/tblastx_options.hpp
algo/blast/api/traceback_stage.hpp
algo/blast/api/uniform_search.hpp
algo/blast/api/version.hpp

algo/cobalt/base.hpp
algo/cobalt/cobalt.hpp
algo/cobalt/dist.hpp
algo/cobalt/exception.hpp
algo/cobalt/hit.hpp
algo/cobalt/hitlist.hpp
algo/cobalt/resfreq.hpp
algo/cobalt/seq.hpp
algo/cobalt/traceback.hpp
//algo/cobalt/tree.hpp

algo/dustmask/symdust.hpp
algo/gnomon/gnomon.hpp
algo/gnomon/gnomon_exception.hpp
algo/gnomon/gnomon_model.hpp
algo/gnomon/Exon_params.hpp
algo/gnomon/Exon_params_.hpp
algo/gnomon/Gnomon_param.hpp
algo/gnomon/Gnomon_param_.hpp
algo/gnomon/Gnomon_params.hpp
algo/gnomon/Gnomon_params_.hpp
algo/gnomon/Intergenic_params.hpp
algo/gnomon/Intergenic_params_.hpp
algo/gnomon/Intron_params.hpp
algo/gnomon/Intron_params_.hpp
algo/gnomon/Length_distribution_params.hpp
algo/gnomon/Length_distribution_params_.hpp
algo/gnomon/Markov_chain_array.hpp
algo/gnomon/Markov_chain_array_.hpp
algo/gnomon/Markov_chain_params.hpp
algo/gnomon/Markov_chain_params_.hpp
algo/gnomon/NCBI_GNOMON_module.hpp
algo/gnomon/asn1.hpp

// next two trouble for Perl?
//algo/phy_tree/bio_tree.hpp
//algo/phy_tree/bio_tree_conv.hpp
algo/phy_tree/dist_methods.hpp
algo/phy_tree/phy_node.hpp

algo/winmask/seq_masker.hpp
algo/winmask/seq_masker_cache_boost.hpp
algo/winmask/seq_masker_istat.hpp
algo/winmask/seq_masker_istat_ascii.hpp
algo/winmask/seq_masker_istat_bin.hpp
algo/winmask/seq_masker_istat_factory.hpp
algo/winmask/seq_masker_istat_oascii.hpp
algo/winmask/seq_masker_istat_obinary.hpp
algo/winmask/seq_masker_ostat.hpp
algo/winmask/seq_masker_ostat_ascii.hpp
algo/winmask/seq_masker_ostat_bin.hpp
algo/winmask/seq_masker_ostat_factory.hpp
algo/winmask/seq_masker_ostat_opt.hpp
algo/winmask/seq_masker_ostat_opt_ascii.hpp
algo/winmask/seq_masker_ostat_opt_bin.hpp
algo/winmask/seq_masker_score.hpp
algo/winmask/seq_masker_score_mean.hpp
algo/winmask/seq_masker_score_mean_glob.hpp
algo/winmask/seq_masker_score_min.hpp
algo/winmask/seq_masker_uset_array.hpp
algo/winmask/seq_masker_uset_hash.hpp
algo/winmask/seq_masker_uset_simple.hpp
algo/winmask/seq_masker_util.hpp
algo/winmask/seq_masker_window.hpp
algo/winmask/seq_masker_window_ambig.hpp
algo/winmask/seq_masker_window_pattern.hpp
algo/winmask/seq_masker_window_pattern_ambig.hpp

// If these come too early, Seq-align stuff
// not %included.  Perhaps related to
// include/objmgr/*.hpp that should be %included
algo/seqqa/blastp_tests.hpp
algo/seqqa/prot_prod_tests.hpp
algo/seqqa/seq_id_tests.hpp
algo/seqqa/seqtest.hpp
algo/seqqa/single_aln_tests.hpp
algo/seqqa/xcript_tests.hpp

%%

// Poly-A stuff (template functions; we'll declare them as non-template)

%{
#include <algo/sequence/polya.hpp>
%}

namespace ncbi
{
enum EPolyTail {
    ePolyTail_None,
    ePolyTail_A3, //> 3' poly-A tail
    ePolyTail_T5  //> 5' poly-T head (submitted to db reversed?)
};

TSignedSeqPos
FindPolyA(objects::CSeqVector_CI begin, objects::CSeqVector_CI end);

EPolyTail
FindPolyTail(objects::CSeqVector_CI begin, objects::CSeqVector_CI end,
             TSignedSeqPos &cleavage_site);
}


%{
#include <algo/phy_tree/bio_tree.hpp>
#include <algo/phy_tree/bio_tree_conv.hpp>
%}
//#define ALGO_PHY_TREE___BIO_TREE__HPP
%include algo/phy_tree/bio_tree.hpp
%include algo/phy_tree/bio_tree_conv.hpp

%template(CBioTreeDynamic)
   ncbi::CBioTree<ncbi::BioTreeBaseNode<ncbi::CBioTreeEmptyNodeData,
                                        ncbi::CBioTreeFeatureList> >;

%template(TTreeNode)
    ncbi::CTreeNode<ncbi::BioTreeBaseNode<ncbi::CBioTreeEmptyNodeData,
                                          ncbi::CBioTreeFeatureList> >;
%template(TBioTreeBaseNode) ncbi::BioTreeBaseNode<ncbi::CBioTreeEmptyNodeData,
                                                  ncbi::CBioTreeFeatureList>;
namespace ncbi {
//     class CBioTreeDynamic
//     {
//     };
    void BioTreeConvertContainer2Dynamic
    (CBioTreeDynamic& dyn_tree, 
     const objects::CBioTreeContainer& tree_container);
    void BioTreeConvert2Container(objects::CBioTreeContainer& tree_container,
                                  const CBioTreeDynamic& dyn_tree);
}

%ignore ncbi::CTreeNode<ncbi::CPhyNodeData>::RemoveAllSubNodes;
%template(TPhyTreeNode) ncbi::CTreeNode<ncbi::CPhyNodeData>;
%template(vector_TPhyTreeNodePtr)
    std::vector<ncbi::CTreeNode<ncbi::CPhyNodeData>*>;
%extend ncbi::CTreeNode<ncbi::CPhyNodeData> {
public:
    void Print(void) const {
        cout << *self;
    }
    std::vector<ncbi::CTreeNode<ncbi::CPhyNodeData>*> GetChildren(void) const {
        std::vector<ncbi::CTreeNode<ncbi::CPhyNodeData>*> rv;
        for (ncbi::CTreeNode<ncbi::CPhyNodeData>::TNodeList_CI it
                = self->SubNodeBegin();
             it != self->SubNodeEnd();  ++it) {
            rv.push_back(*it);
        }
        return rv;
    }
}

%extend ncbi::CBioTreeDynamic {
public:
    void Print(void) const {
        cout << *self;
    }
}


// Member of CContigAssembly::SAlignStats
%template(vector_CContigAssembly_SAlignStats_STails)
    std::vector<ncbi::CContigAssembly_SAlignStats_STails>;
