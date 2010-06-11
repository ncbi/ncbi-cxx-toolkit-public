#ifndef ALGO_SEQUENCE___GENE_MODEL__HPP
#define ALGO_SEQUENCE___GENE_MODEL__HPP

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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
    class CScope;
    class CSeq_feat;
    class CSeq_align;
    class CSeq_annot;
    class CBioseq_set;
END_SCOPE(objects)


class NCBI_XALGOSEQ_EXPORT CGeneModel
{
public:
    enum EGeneModelCreateFlags {
        fCreateGene          = 0x01,
        fCreateMrna          = 0x02,
        fCreateCdregion      = 0x04,
        fPromoteAllFeatures  = 0x08,
        fPropagateOnly       = 0x10,
        fForceTranslateCds   = 0x20,
        fForceTranscribeMrna = 0x40,
        fDensegAsExon        = 0x80,

        fDefaults = fCreateGene | fCreateMrna | fCreateCdregion
    };
    typedef int TGeneModelCreateFlags;

    /// Trim an alignment according to our best guess of its biological
    /// representation.  Trimming involves adjusting segments to satisfy our
    /// expectations of partial exonic alignments and account for unaligned
    /// parts.
    static CConstRef<objects::CSeq_align>
    TrimAlignment(const objects::CSeq_align& align,
                  objects::CScope& scope);

    /// Create a gene model from an alignment
    /// this will optionally promote all features through the alignment
    static void CreateGeneModelFromAlign(const objects::CSeq_align& align,
                                         objects::CScope& scope,
                                         objects::CSeq_annot& annot,
                                         objects::CBioseq_set& seqs,
                                         TGeneModelCreateFlags flags = fDefaults,
                                         TSeqPos allowed_unaligned = 10);


    /// Correctly mark exceptions on a feature
    ///
    static void SetFeatureExceptions(objects::CSeq_feat& feat,
                                     objects::CScope& scope,
                                     const objects::CSeq_align* align = NULL);

};


END_NCBI_SCOPE

#endif  // ALGO_SEQUENCE___GENE_MODEL__HPP
