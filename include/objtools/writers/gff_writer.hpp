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

#ifndef OBJTOOLS_READERS___GFF_WRITER__HPP
#define OBJTOOLS_READERS___GFF_WRITER__HPP

#include <corelib/ncbistd.hpp>
#include <objtools/writers/gff2_write_data.hpp>
#include <objtools/writers/gff_feature_record.hpp>
#include <objtools/writers/writer.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

//  ============================================================================
/// CWriterBase implementation that formats Genbank objects as plain GFF files.
/// GFF (or GFF2) is a predecessor of GTF, GFF3, GVF and probably half a dozen
/// other dialects in use today. GFF files consist of feature records, with each
/// feature record consisting of nine columns.
/// There is some agreed upon meaning of the first eight columns and on the 
/// general grammer of the column contents. Beyond that, there has never been
/// a universally agreed upon defintion of the format (probably the prime reason
/// the format is deprecated now).
/// For the purpose of this implementation, GFF is taken to be the greatest 
/// common denominator between GTF and GFF3 (and this GVF). Those other writers
/// then derive from this one, adding their own bits and pieces to complete the
/// format renderer.
///
class NCBI_XOBJWRITE_EXPORT CGff2Writer:
    public CWriterBase, CFeatWriter
//  ============================================================================
{
public:
    typedef enum {
        fSoQuirks = (fWriterBaseLast << 1),
        fGenerateMissingTranscripts = (fWriterBaseLast << 2),
        fGff2WriterLast = fSoQuirks,
    } TFlags;
    
public:
    /// Constructor.
    /// @param scope
    ///   scope to be used for ID reference resolution (it's OK to create one
    ///   on the fly).
    /// @param ostr
    ///   stream objects should be written to.
    /// @param flags
    ///   any output customization flags.
    ///
    CGff2Writer(
        CScope& scope,
        CNcbiOstream& ostr,
        unsigned int flags=fNormal );

    /// Constructor.
    /// Scopeless version. A scope will be allocated internally.
    /// @param ostr
    ///   stream objects should be written to.
    /// @param flags
    ///   any output customization flags.
    ///
    CGff2Writer(
        CNcbiOstream&,
        unsigned int = fNormal );

    virtual ~CGff2Writer();

    /// Write a file header identifying the file content as GFF version 2.
    ///
    virtual bool WriteHeader();

    virtual bool WriteHeader(
        const CSeq_annot& ) { return WriteHeader(); };

    /// Write a trailer marking the end of a parsing context.
    ///
    virtual bool WriteFooter();

    virtual bool WriteFooter(
        const CSeq_annot& ) { return WriteFooter(); };

    /// Convenience function to render a "naked" Seq-annot. Makes use of the
    /// internal scope.
    /// @param annot
    ///   Seq-annot object to be rendered
    /// @param asmblyName
    ///   optional assembly name to use for the file header
    /// @param asmblyAccession
    ///   optional assembly accession to use for the file header
    ///
    virtual bool WriteAnnot( 
        const CSeq_annot& annot,
        const string& asmblyName="",
        const string& asmblyAccession="" );

    /// Write a Seq-align object.
    /// Calling this function on a general GFF2 writer (as opposed to GFF3 or
    /// another more specialized format will fail because GFF2 at this general 
    /// level does not address alignments; you will need at least a GFF3 writer 
    /// for that.
    /// @param align
    ///   Seq-align object to be rendered
    /// @param asmblyName
    ///   optional assembly name to use for the file header
    /// @param asmblyAccession
    ///   optional assembly accession to use for the file header
    ///
    virtual bool WriteAlign( 
        const CSeq_align&,
        const string& asmblyName="",
        const string& asmblyAccession="" );

    /// Write Seq-entry contained in a given handle.
    /// Essentially, will iterate through all contained Bioseq objects and process
    /// those, with some special processing for nuc-prot sets.
    /// @param seh
    ///   Seq-entry handle to be processed
    /// @param asmblyName
    ///   optional assembly name to use for the file header
    /// @param asmblyAccession
    ///   optional assembly accession to use for the file header
    ///
    virtual bool WriteSeqEntryHandle(
        CSeq_entry_Handle seh,
        const string& asmblyName="",
        const string& asmblyAccession="" );

    /// Write Bioseq contained in given handle
    /// Essentially, will write all features that live on the given Bioseq.
    /// @param bsh
    ///   Bioseq handle to be processed
    /// @param asmblyName
    ///   optional assembly name to use for the file header
    /// @param asmblyAccession
    ///   optional assembly accession to use for the file header
    ///
    virtual bool WriteBioseqHandle(
        CBioseq_Handle bsh,
        const string& asmblyName="",
        const string& asmblyAccession="" );

    /// Write Seq-annot contained in given handle
    /// Essentially, write out embedded feature table. Other annotation
    /// types are not supported in the generic GFF2 writer(i.e. there will be 
    /// a header and nothing else.
    /// @param sah
    ///   Seq-annot handle to be processed
    /// @param asmblyName
    ///   optional assembly name to use for the file header
    /// @param asmblyAccession
    ///   optional assembly accession to use for the file header
    ///
    virtual bool WriteSeqAnnotHandle(
        CSeq_annot_Handle sah,
        const string& asmblyName="",
        const string& asmblyAccession="" );

protected:
    virtual bool xAssignFeature(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);

    virtual bool xAssignFeatureBasic(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);

    virtual bool xAssignFeatureType(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);

    virtual bool xAssignFeatureSeqId(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);

    virtual bool xAssignFeatureMethod(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);

    virtual bool xAssignFeatureEndpoints(
        CGffFeatureRecord& record,
        CGffFeatureContext&,
        const CMappedFeat& mapped_feat);

    virtual bool xAssignFeatureScore(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);

    virtual bool xAssignFeatureStrand(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);

    virtual bool xAssignFeaturePhase(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);

    virtual bool xAssignFeatureAttributes(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);

    virtual bool xAssignFeatureAttributesFormatIndependent(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);

    virtual bool xAssignFeatureAttributesFormatSpecific(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);

    virtual bool xAssignFeatureAttributesQualifiers(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&) { return false; };
    virtual bool xAssignFeatureAttributesGene(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);
    virtual bool xAssignFeatureAttributeDbxref(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&) { return false; };
    virtual bool xAssignFeatureAttributeNote(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&) { return false; };
    virtual bool xAssignFeatureAttributeProduct(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);
    virtual bool xAssignFeatureAttributeProteinId(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);
    virtual bool xAssignFeatureAttributeRibosomalSlippage(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);
    virtual bool xAssignFeatureAttributeTranslationTable(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);
    virtual bool xAssignFeatureAttributePartial(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);
    virtual bool xAssignFeatureAttributePseudo(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeCodeBreak(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeOldLocusTag(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeGeneBiotype(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeMapLoc(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeException(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeExperiment(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeModelEvidence(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeRptFamily(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);
    bool xAssignFeatureAttributePseudoGene(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeIsOrdered(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeFunction(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);
    bool xAssignFeatureAttributesGoMarkup(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeEcNumbers(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeExonNumber(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeGbKey(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);

    virtual bool x_WriteSequenceHeader(
        CBioseq_Handle ) { return true; };

    virtual bool x_WriteSequenceHeader(
        CSeq_id_Handle ) { return true; };

    virtual bool x_WriteAnnot( 
        const CSeq_annot& );

    virtual bool x_WriteAlign(
        const CSeq_align&);

    virtual bool x_WriteSeqEntryHandle(
        CSeq_entry_Handle );

    virtual bool x_WriteSeqAnnotHandle(
        CSeq_annot_Handle );
    virtual bool x_WriteBioseqHandle(
        CBioseq_Handle );

    virtual bool xWriteFeature(
        CGffFeatureContext&,
        const CMappedFeat& );
    virtual bool xWriteFeature(
        CFeat_CI feat_it);

    virtual bool xWriteAllChildren(
        CGffFeatureContext&,
        const CMappedFeat&);

    virtual bool x_WriteAssemblyInfo(
        const string&,
        const string& );

    virtual bool x_WriteBrowserLine(
        const CRef< CUser_object > );

    virtual bool x_WriteTrackLine(
        const CRef< CUser_object > );

    virtual bool xGeneratingMissingTranscripts() const 
    {
        return (m_uFlags & fGenerateMissingTranscripts);
    }

    static bool IsTranscriptType(
        const CMappedFeat&);
    static bool HasAccaptableTranscriptParent(
        CGffFeatureContext&,
        const CMappedFeat&);

    // data:
    CRef<CScope> m_pScope;
    bool m_bHeaderWritten;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif  // OBJTOOLS_WRITERS___GFF_WRITER__HPP
