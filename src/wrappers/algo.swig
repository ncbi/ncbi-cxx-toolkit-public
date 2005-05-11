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


// blast::TSeqLocVector
%template(vector_SSeqLoc) std::vector<SSeqLoc>;
// blast::TSeqAlignVector
VECTOR_CREF(vector_CRef_CSeq_align_set, ncbi::objects::CSeq_align_set);

%template(vector_CNWAligner_ETranscriptSymbol)
    std::vector<ncbi::CNWAligner::ETranscriptSymbol>;

// Take ownership of some objects returned by pointer
%newobject ncbi::CDistMethods::FastMeTree;
%newobject ncbi::CDistMethods::NjTree;
%newobject ncbi::ReadNewickTree;

%%

algo/align/nw/align_exception.hpp
algo/align/nw/mm_aligner.hpp
algo/align/nw/nw_aligner.hpp
algo/align/nw/nw_formatter.hpp
algo/align/nw/nw_spliced_aligner.hpp
algo/align/nw/nw_spliced_aligner16.hpp
algo/align/nw/nw_spliced_aligner32.hpp
algo/align/nw/nw_band_aligner.hpp
algo/align/splign/splign.hpp
algo/align/splign/splign_compartment_finder.hpp
algo/align/splign/splign_formatter.hpp
// "temporary" code, has problems
//algo/align/splign/splign_hit.hpp
algo/align/splign/splign_simple.hpp
algo/sequence/antigenic.hpp
algo/sequence/coiled_coil.hpp
algo/sequence/cpg.hpp
algo/sequence/find_pattern.hpp
algo/sequence/make_cdr_prods.hpp
algo/sequence/nuc_prop.hpp
algo/sequence/orf.hpp
algo/sequence/prot_prop.hpp
algo/sequence/regexp_loc.hpp
algo/sequence/restriction.hpp
algo/sequence/seq_match.hpp
algo/sequence/signal_seq.hpp
algo/blast/api/bl2seq.hpp
algo/blast/api/blast_aux.hpp
algo/blast/api/blast_exception.hpp
algo/blast/api/blast_nucl_options.hpp
algo/blast/api/blast_options.hpp
algo/blast/api/blast_options_handle.hpp
algo/blast/api/blast_prot_options.hpp
algo/blast/api/blast_rps_options.hpp
algo/blast/api/blast_types.hpp
algo/blast/api/blastx_options.hpp
algo/blast/api/db_blast.hpp
algo/blast/api/disc_nucl_options.hpp
algo/blast/api/seqsrc_multiseq.hpp
algo/blast/api/remote_blast.hpp
algo/blast/api/seqsrc_seqdb.hpp
algo/blast/api/rpstblastn_options.hpp
algo/blast/api/tblastn_options.hpp
algo/blast/api/tblastx_options.hpp

algo/blast/api/bl2seq.hpp
algo/blast/api/blast_aux.hpp
algo/blast/api/blast_exception.hpp
algo/blast/api/blast_mtlock.hpp
algo/blast/api/blast_nucl_options.hpp
algo/blast/api/blast_options.hpp
algo/blast/api/blast_options_handle.hpp
algo/blast/api/blast_prot_options.hpp
algo/blast/api/blast_psi.hpp
algo/blast/api/blast_rps_options.hpp
algo/blast/api/blast_seqinfosrc.hpp
algo/blast/api/blast_types.hpp
algo/blast/api/blastx_options.hpp
algo/blast/api/db_blast.hpp
algo/blast/api/disc_nucl_options.hpp
algo/blast/api/hspstream_queue.hpp
algo/blast/api/psi_pssm_input.hpp
algo/blast/api/psiblast_options.hpp
algo/blast/api/pssm_input.hpp
algo/blast/api/remote_blast.hpp
algo/blast/api/repeats_filter.hpp
algo/blast/api/rpstblastn_options.hpp
algo/blast/api/seqinfosrc_seqdb.hpp
algo/blast/api/seqinfosrc_seqvec.hpp
algo/blast/api/seqsrc_multiseq.hpp
algo/blast/api/seqsrc_seqdb.hpp
algo/blast/api/tblastn_options.hpp
algo/blast/api/tblastx_options.hpp

algo/blast/core/aa_ungapped.h
algo/blast/core/blast_def.h
algo/blast/core/blast_diagnostics.h
algo/blast/core/blast_dust.h
algo/blast/core/blast_encoding.h
algo/blast/core/blast_engine.h
algo/blast/core/blast_export.h
algo/blast/core/blast_extend.h
algo/blast/core/blast_filter.h
algo/blast/core/blast_gapalign.h
algo/blast/core/blast_hits.h
algo/blast/core/blast_hspstream.h
algo/blast/core/blast_kappa.h
//algo/blast/core/blast_lookup.h
algo/blast/core/blast_message.h
algo/blast/core/blast_options.h
algo/blast/core/blast_parameters.h
algo/blast/core/blast_psi.h
algo/blast/core/blast_rps.h
algo/blast/core/blast_seg.h
algo/blast/core/blast_seqsrc.h
algo/blast/core/blast_setup.h
algo/blast/core/blast_stat.h
algo/blast/core/blast_toolkit.h
algo/blast/core/blast_traceback.h
algo/blast/core/blast_util.h
algo/blast/core/gapinfo.h
algo/blast/core/greedy_align.h
algo/blast/core/hspstream_collector.h
algo/blast/core/link_hsps.h
algo/blast/core/lookup_util.h
//algo/blast/core/lookup_wrap.h
algo/blast/core/mb_lookup.h
algo/blast/core/ncbi_math.h
algo/blast/core/ncbi_std.h
algo/blast/core/pattern.h
algo/blast/core/phi_extend.h
algo/blast/core/phi_lookup.h

algo/gnomon/gnomon.hpp
algo/gnomon/gnomon_exception.hpp
// next two trouble for Perl?
//algo/phy_tree/bio_tree.hpp
//algo/phy_tree/bio_tree_conv.hpp
algo/phy_tree/dist_methods.hpp
algo/phy_tree/phy_node.hpp
algo/phy_tree/phy_tree_serial.hpp

// If these come too early, Seq-align stuff
// not %included.  Perhaps related to
// include/objmgr/*.hpp that should be %included
algo/seqqa/blastp_tests.hpp
algo/seqqa/prot_prod_tests.hpp
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


#if 0   // yields uncompilable code

%{
#include <algo/phy_tree/bio_tree.hpp>
#include <algo/phy_tree/bio_tree_conv.hpp>
%}

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
    void BioTreeConvertContainer2Dynamic
    (CBioTreeDynamic& dyn_tree, 
     const objects::CBioTreeContainer& tree_container);
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
#endif


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/05/11 21:27:35  jcherry
 * Initial version
 *
 * ===========================================================================
 */
