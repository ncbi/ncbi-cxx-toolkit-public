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
#include <corelib/ncbiexpt.hpp>
#include <util/range.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
    class CScope;
    class CSeq_feat;
    class CSeq_align;
    class CSeq_annot;
    class CBioseq_set;
    class CBioseq_Handle;
    struct SAnnotSelector;
END_SCOPE(objects)

class CAlgoFeatureGeneratorException : public CException
 {
 public:
     enum EErrCode {
         eUnknown,
         eMicroIntrons,
     };
     virtual const char* GetErrCodeString(void) const override {
         switch ( GetErrCode() ) {
         case eUnknown:
             return "Unknown error";
         case eMicroIntrons:
             return "MicroIntron generation failure";
         default:
             return CException::GetErrCodeString();
         }
     }
     NCBI_EXCEPTION_DEFAULT(CAlgoFeatureGeneratorException, CException);
 };


class NCBI_XALGOSEQ_EXPORT CFeatureGenerator
{
public:
    NCBI_DEPRECATED
    CFeatureGenerator(CRef<objects::CScope> scope);

    CFeatureGenerator(objects::CScope& scope);
    ~CFeatureGenerator();

    enum EGeneModelCreateFlags {

        // CleanAlignment flags
        fTrimEnds            = 0x1000, // trim ends to codon boundaries (protein or mrna with CDS partially aligned)
        fMaximizeTranslation = 0x2000, // leave only 1-2 base indels: 
                                       // minimize product-ins modulo 3,
                                       // replace complete genomic-ins triplets with diags 
                                       // recalculate query positions.
                                       // Need to be careful with transcript queries -
                                       // cdregion passed to convert should correspond to the modified
                                       // query positions

        // Convert flags
        fCreateGene          = 0x001,
        fCreateMrna          = 0x002,
        fCreateCdregion      = 0x004,
        fPromoteAllFeatures  = 0x008,
        fPropagateOnly       = 0x010,
        fForceTranslateCds   = 0x020,
        fForceTranscribeMrna = 0x040,
        fDensegAsExon        = 0x080,
        fGenerateLocalIds    = 0x100,     // uses current date
        fGenerateStableLocalIds = 0x200,  // reproducible ids
        fPropagateNcrnaFeats = 0x400,
        fTrustProteinSeq     = 0x800,
        // already-used:     = 0x1000,
        // already-used:     = 0x2000,
        fDeNovoProducts      = 0x4000,
        fAddTranslatedCDSAssembly = 0x8000, // add translated_cds_bioseq->SetInst().SetHist().SetAssembly().push_back(align)
        fDropManeMarkup       = 0x00010000,
        fSkipLocationCheck    = 0x00020000,

        fDefaults = fCreateGene | fCreateMrna | fCreateCdregion |
                    fGenerateLocalIds | fPropagateNcrnaFeats
    };
    typedef int TFeatureGeneratorFlags;

    enum EIntronStitchThresholdFlags {
        fProduct = 1,
        fGenomic = 2,
        fBoth = 3
    };

    static const TSeqPos kDefaultMinIntron = 200;
    static const TSeqPos kDefaultAllowedUnaligned = 10;

    void SetFlags(TFeatureGeneratorFlags);
    TFeatureGeneratorFlags GetFlags() const;
    void SetIntronStitchThresholdFlags(EIntronStitchThresholdFlags);
    void SetMinIntron(TSeqPos);
    void SetAllowedUnaligned(TSeqPos);

    /// Clean an alignment according to our best guess of its biological
    /// representation.  Cleaning involves adjusting segments to satisfy our
    /// expectations of partial exonic alignments and account for unaligned
    /// parts. Eg. stitching small gaps (less than min_intron), trimming to codon boundaries.
    /// May shift product positions.
    CConstRef<objects::CSeq_align>
    CleanAlignment(const objects::CSeq_align& align);

    /// Adjust alignment to the specified range
    /// (cross-the-origin range on circular chromosome is indicated by range.from > range.to)
    /// Will add necessary 'diags' at ends.
    /// Throws an exception on attempt to shink past an indel in CDS
    /// Works on Spliced-seg alignments only.
    /// Note: for a protein alignment do not expand it to include stop codon.

    enum EProductPositionsMode {
        eForceProductFrom0,
        eTryToPreserveProductPositions
    };
    CConstRef<objects::CSeq_align>
    AdjustAlignment(const objects::CSeq_align& align, TSeqRange range, EProductPositionsMode mode = eForceProductFrom0);


    /// Convert an alignment to an annotation.
    /// This will optionally promote all features through the alignment
    /// and create product sequences
    /// Returns mRNA feature
    CRef<objects::CSeq_feat> ConvertAlignToAnnot(const objects::CSeq_align& align,
                             objects::CSeq_annot& annot,
                             objects::CBioseq_set& seqs,
                             Int8 gene_id = 0,
                             const objects::CSeq_feat* cdregion_on_mrna = NULL);

    void ConvertAlignToAnnot(const list< CRef<objects::CSeq_align> > &aligns,
                             objects::CSeq_annot &annot,
                             objects::CBioseq_set &seqs);

    /// Convert genomic location to an annotation. Populates seqs with mRna
    /// and protein sequences, and populates annot with gene, mRna
    /// and cdretgion features
    void ConvertLocToAnnot(
        const objects::CSeq_loc &loc,
        objects::CSeq_annot& annot,
        objects::CBioseq_set& seqs,
        objects::CCdregion::EFrame frame = objects::CCdregion::eFrame_one,
        CRef<objects::CSeq_id> prot_id = CRef<objects::CSeq_id>(),
        CRef<objects::CSeq_id> rna_id = CRef<objects::CSeq_id>());

    /// Correctly mark exceptions on a feature
    ///
    void SetFeatureExceptions(objects::CSeq_feat& feat,
                              const objects::CSeq_align* align = NULL);

    /// Mark the correct partial states for a set of features
    ///
    void SetPartialFlags(CRef<objects::CSeq_feat> gene_feat,
                         CRef<objects::CSeq_feat> mrna_feat,
                         CRef<objects::CSeq_feat> cds_feat);

    /// Recompute the correct partial states for all features in this annotation
    void RecomputePartialFlags(objects::CSeq_annot& annot);




    /// Project RNA, preserving discontinuities in the CDS.
    ///
    /// Postcondition: Output is a mix of packed-ints, where each sub-loc in the mix
    /// is an exon, and each subloc in the exon packed-int is an exon chunk. The chunks may
    /// have gaps between them or overlap as to preserve the translation frame of the CDS.
    ///
    /// The discontinuities (gaps and overlaps of chunks) that are outside of the CDS are collapsed.
    ///
    /// Singleton container locs (comprised of single element) are canonicalized:
    ///   unbroken exons are represented as a single interval
    ///   single-exon locs are represented as a single packed-int (or int, as per above)
    static CRef<objects::CSeq_loc> s_ProjectRNA(const objects::CSeq_align& spliced_aln, 
                                                CConstRef<objects::CSeq_loc> product_cds_loc = CConstRef<objects::CSeq_loc>(NULL),
                                                size_t unaligned_ends_partialness_thr = kDefaultAllowedUnaligned);
    /// Similar to s_ProjectRNA(...)
    /// Postcondition: seq-vector of the returned loc is of exact same length and has no indels 
    ///                relative to the seq-vector of the product_cds_loc truncated to the alignment boundaries.
    /// 1-2 bp overlaps converted to gaps preserving frame if convert_overlaps = true
    static CRef<objects::CSeq_loc> s_ProjectCDS(const objects::CSeq_align& spliced_aln, 
                                                const objects::CSeq_loc& product_cds_loc,
                                                bool convert_overlaps = true);
    // when specified, annot_name creates introns for features from a given annot_name
    // non-NULL range limits processing to a specific range
    static void CreateMicroIntrons(
                                                objects::CScope& scope,
                                                objects::CBioseq_Handle bsh,
                                                const string& annot_name = "",
                                                TSeqRange* range = NULL,
                                                bool ignore_errors = false);

private:
    struct SImplementation;
    unique_ptr<SImplementation> m_impl;

    // adjust the selector to use a given annotation if not empty
    static void x_SetAnnotName(objects::SAnnotSelector& sel, const string& annot_name);
};


class NCBI_XALGOSEQ_EXPORT CGeneModel
{
public:
    enum EGeneModelCreateFlags {
        fCreateGene          = CFeatureGenerator::fCreateGene,
        fCreateMrna          = CFeatureGenerator::fCreateMrna,
        fCreateCdregion      = CFeatureGenerator::fCreateCdregion,
        fForceTranslateCds   = CFeatureGenerator::fForceTranslateCds,
        fForceTranscribeMrna = CFeatureGenerator::fForceTranscribeMrna,

        fDefaults = fCreateGene | fCreateMrna | fCreateCdregion
    };
    typedef int TGeneModelCreateFlags;

    /// Create a gene model from an alignment
    /// this will optionally promote all features through the alignment
    NCBI_DEPRECATED
    static void CreateGeneModelFromAlign(const objects::CSeq_align& align,
                                         objects::CScope& scope,
                                         objects::CSeq_annot& annot,
                                         objects::CBioseq_set& seqs,
                                         TGeneModelCreateFlags flags = fDefaults,
                                         TSeqPos allowed_unaligned = 10);

    NCBI_DEPRECATED
    static void CreateGeneModelsFromAligns(const list< CRef<objects::CSeq_align> > &aligns,
                                           objects::CScope& scope,
                                           objects::CSeq_annot& annot,
                                           objects::CBioseq_set& seqs,
                                           TGeneModelCreateFlags flags = fDefaults,
                                           TSeqPos allowed_unaligned = 10);

    /// Correctly mark exceptions on a feature
    ///
    NCBI_DEPRECATED
    static void SetFeatureExceptions(objects::CSeq_feat& feat,
                                     objects::CScope& scope,
                                     const objects::CSeq_align* align = NULL);

    NCBI_DEPRECATED
    static void SetPartialFlags(objects::CScope& scope,
                                CRef<objects::CSeq_feat> gene_feat,
                                CRef<objects::CSeq_feat> mrna_feat,
                                CRef<objects::CSeq_feat> cds_feat);

    NCBI_DEPRECATED
    static void RecomputePartialFlags(objects::CScope& scope,
                                      objects::CSeq_annot& annot);
};

END_NCBI_SCOPE

#endif  // ALGO_SEQUENCE___GENE_MODEL__HPP
