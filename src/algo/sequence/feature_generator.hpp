#ifndef ALGO_SEQUENCE___FEAT_GEN__HPP
#define ALGO_SEQUENCE___FEAT_GEN__HPP

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
 * Authors:  Vyacheslav Chetvernin
 *
 * File Description:
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <algo/sequence/gene_model.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqloc/Na_strand.hpp>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE()

struct SExon {
    TSignedSeqPos prod_from;
    TSignedSeqPos prod_to;
    TSignedSeqPos genomic_from;
    TSignedSeqPos genomic_to;
};

END_SCOPE();

struct CFeatureGenerator::SImplementation {
    SImplementation(CRef<objects::CScope> scope);
    ~SImplementation();

    CRef<objects::CScope> m_scope;
    TFeatureGeneratorFlags m_flags;
    TSeqPos m_min_intron;
    TSeqPos m_allowed_unaligned;

    void StitchSmallHoles(objects::CSeq_align& align);
    void TrimHolesToCodons(objects::CSeq_align& align);

    void ConvertAlignToAnnot(const objects::CSeq_align& align,
                             objects::CSeq_annot& annot,
                             objects::CBioseq_set& seqs);
    void SetFeatureExceptions(objects::CSeq_feat& feat,
                              const objects::CSeq_align* align);

    TSignedSeqPos GetCdsStart(const objects::CSeq_id& seqid);

    void TrimLeftExon(int trim_amount,
                      vector<SExon>::iterator left_edge,
                      vector<SExon>::iterator& exon_it,
                      objects::CSpliced_seg::TExons::iterator& spl_exon_it,
                      objects::ENa_strand product_strand,
                      objects::ENa_strand genomic_strand);
    void TrimRightExon(int trim_amount,
                       vector<SExon>::iterator& exon_it,
                       vector<SExon>::iterator right_edge,
                       objects::CSpliced_seg::TExons::iterator& spl_exon_it,
                       objects::ENa_strand product_strand,
                       objects::ENa_strand genomic_strand);
};

END_NCBI_SCOPE

#endif  // ALGO_SEQUENCE___FEAT_GEN__HPP
