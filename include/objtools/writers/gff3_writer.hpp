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
 * File Description:  Write gff3 file
 *
 */

#ifndef OBJTOOLS_READERS___GFF3_WRITER__HPP
#define OBJTOOLS_READERS___GFF3_WRITER__HPP

#include <corelib/ncbistd.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objtools/writers/gff_writer.hpp>
#include <objtools/writers/gff3_write_data.hpp>
#include <objtools/writers/gff_feature_record.hpp>
#include <objtools/alnmgr/alnmap.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_exon.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

class CGffAlignmentRecord;

//  ============================================================================
class NCBI_XOBJWRITE_EXPORT CGff3Writer
//  ============================================================================
    : public CGff2Writer, public CFeatWriter
{
public:
    typedef enum {
        fExtraQuals = (fGff2WriterLast << 1),
        fMicroIntrons = (fGff2WriterLast << 2),
        fGff3WriterLast = fMicroIntrons,
    } TFlags;
    
public:
    CGff3Writer(
        CScope&,
        CNcbiOstream&,
        unsigned int = fNormal,
        bool sortAlignments = false 
        );
    CGff3Writer(
        CNcbiOstream&,
        unsigned int = fNormal,
        bool sortAlignments = false 
        );
    virtual ~CGff3Writer() = default;

    void SetDefaultMethod(
        const string& defaultMethod) { m_sDefaultMethod = defaultMethod; };
    void SetBioseqHandle(CBioseq_Handle bsh);

    virtual bool WriteHeader();
    virtual bool WriteHeader(
        const CSeq_annot& annot) { return CGff2Writer::WriteHeader(annot); };
    bool WriteAlign(
        const CSeq_align&,
        const string& asmblyName="",
        const string& asmblyAccession="" );

protected:
    typedef list<pair<CConstRef<CSeq_align>, string>> TAlignCache;

protected:
    virtual bool x_WriteBioseqHandle(
        CBioseq_Handle );
	virtual bool x_WriteSeqAnnotHandle(
        CSeq_annot_Handle );

    bool xWriteAlign( 
        const CSeq_align&,
        const string& = "");
    virtual bool xWriteAlignDenseg(
        const CSeq_align&,
        const string& = "");
    virtual bool xWriteAlignSpliced(
        const CSeq_align&,
        const string& = "");
    virtual bool xWriteAlignDisc(
        const CSeq_align&,
        const string& = "");

    virtual bool xAssignFeatureAttributeParentGene(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat& );

    virtual bool xAssignFeatureAttributeParentMrna(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat& );

    virtual bool xWriteSequenceHeader(
        CBioseq_Handle );
    virtual bool xWriteSequenceHeader(
        CSeq_id_Handle );
    virtual bool xWriteSource(
        CBioseq_Handle);
    virtual bool xWriteFeature(
        CFeat_CI feat_it) override;
    virtual bool xWriteFeature(
        CGffFeatureContext&,
        const CMappedFeat&);
    virtual bool xWriteFeatureGene(
        CGffFeatureContext&,
        const CMappedFeat& );
    virtual bool xWriteFeatureRna(
        CGffFeatureContext&,
        const CMappedFeat& );
    virtual bool xWriteFeatureCds(
        CGffFeatureContext&,
        const CMappedFeat& );
    virtual bool xWriteFeatureGeneric(
        CGffFeatureContext&,
        const CMappedFeat& );
    virtual bool xWriteFeatureTrna(
        CGffFeatureContext&,
        const CMappedFeat& );
    virtual bool xWriteFeatureCDJVSegment(
        CGffFeatureContext&,
        const CMappedFeat& );
    virtual bool xWriteAllChildren(
        CGffFeatureContext&,
        const CMappedFeat&);

    virtual bool xWriteRecord( 
        const CGffBaseRecord& );

    void xWriteAlignment( 
        const CGffAlignmentRecord& record );

    virtual bool xWriteFeatureRecords(
        const CGffFeatureRecord&,
        const CSeq_loc&,
        unsigned int );

    //bool xCreateMicroIntrons(
    //    CBioseq_Handle);
    //
    bool xSplicedSegHasProteinProd(
            const CSpliced_seg& spliced);

    bool xAssignAlignment(
        CGffFeatureRecord&);
    virtual bool xAssignAlignmentScores(
        CGffAlignRecord&,
        const CSeq_align&);

    bool xAssignAlignmentDenseg(
        CGffAlignRecord&,
        const CAlnMap&,
        unsigned int);
    virtual bool xAssignAlignmentDensegSeqId(
        CGffAlignRecord&,
        const CAlnMap&,
        unsigned int);
    bool xAssignAlignmentDensegType(
        CGffAlignRecord&,
        const CAlnMap&,
        unsigned int);
    bool xAssignAlignmentDensegMethod(
        CGffAlignRecord&,
        const CAlnMap&,
        unsigned int);
    virtual bool xAssignAlignmentDensegScores(
        CGffAlignRecord&,
        const CAlnMap&,
        unsigned int);
    virtual bool xAssignAlignmentDensegTarget(
        CGffAlignRecord&,
        const CAlnMap&,
        unsigned int);
    bool xAssignAlignmentDensegGap(
        CGffAlignRecord&,
        const CAlnMap&,
        unsigned int);
    virtual bool xAssignAlignmentDensegLocation(
        CGffAlignRecord&,
        const CAlnMap&,
        unsigned int);

    //Spliced-seg processing
    bool xAssignAlignmentSpliced(
        CGffAlignRecord&,
        const CSpliced_seg&,
        const CSpliced_exon&);
    virtual bool xAssignAlignmentSplicedTarget(
        CGffAlignRecord&,
        const CSpliced_seg&,
        const CSpliced_exon&);
    bool xAssignAlignmentSplicedPhase(
        CGffAlignRecord&,
        const CSpliced_seg&,
        const CSpliced_exon&);
    bool xAssignAlignmentSplicedAttributes(
        CGffAlignRecord&,
        const CSpliced_seg&,
        const CSpliced_exon&);
    virtual bool xAssignAlignmentSplicedGap(
        CGffAlignRecord&,
        const CSpliced_seg&,
        const CSpliced_exon&);
    virtual bool xAssignAlignmentSplicedScores(
        CGffAlignRecord&,
        const CSpliced_seg&,
        const CSpliced_exon&);
    virtual bool xAssignAlignmentSplicedLocation(
        CGffAlignRecord&,
        const CSpliced_seg&,
        const CSpliced_exon&);
    bool xAssignAlignmentSplicedType(
        CGffAlignRecord&,
        const CSpliced_seg&,
        const CSpliced_exon&);
    bool xAssignAlignmentSplicedMethod(
        CGffAlignRecord&,
        const CSpliced_seg&,
        const CSpliced_exon&);
    virtual bool xAssignAlignmentSplicedSeqId(
        CGffAlignRecord&,
        const CSpliced_seg&,
        const CSpliced_exon&);
        
    virtual void x_SortAlignments(TAlignCache& alignCache,
                                   CScope& scope);
    bool xAssignSource(
        CGffSourceRecord&,
        CBioseq_Handle);
    bool xAssignSourceType(
        CGffSourceRecord&);
    bool xAssignSourceSeqId(
        CGffSourceRecord&,
        CBioseq_Handle);
    bool xAssignSourceMethod(
        CGffSourceRecord&,
        CBioseq_Handle);
    bool xAssignSourceEndpoints(
        CGffSourceRecord&,
        CBioseq_Handle);
    bool xAssignSourceAttributes(
        CGffSourceRecord&,
        CBioseq_Handle);
    bool xAssignSourceAttributeGbKey(
        CGffSourceRecord&);
    bool xAssignSourceAttributeMolType(
        CGffSourceRecord&,
        CBioseq_Handle);
    bool xAssignSourceAttributeIsCircular(
        CGffSourceRecord&,
        CBioseq_Handle);
    bool xAssignSourceAttributesBioSource(
        CGffSourceRecord&,
        CBioseq_Handle);

    bool xAssignSourceAttributeGenome(
        CGffSourceRecord&,
        const CBioSource&);
    bool xAssignSourceAttributeName(
        CGffSourceRecord&,
        const CBioSource&);
    bool xAssignSourceAttributeDbxref(
        CGffSourceRecord&,
        const CBioSource&);
    bool xAssignSourceAttributesOrgMod(
        CGffSourceRecord&,
        const CBioSource&);
    bool xAssignSourceAttributesSubSource(
        CGffSourceRecord&,
        const CBioSource&);


    //begin mss-234//
    bool xAssignFeature(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);
    bool xAssignFeatureBasic(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);
    bool xAssignFeatureType(
        CGffFeatureRecord&,
        const CMappedFeat&);
    bool xAssignFeatureSeqId(
        CGffFeatureRecord&,
        const CMappedFeat&);
    bool xAssignFeatureMethod(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);
    bool xAssignFeatureEndpoints(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);
    bool xAssignFeatureScore(
        CGffFeatureRecord&,
        const CMappedFeat&);
    bool xAssignFeatureStrand(
        CGffFeatureRecord&,
        const CMappedFeat&);
    bool xAssignFeaturePhase(
        CGffFeatureRecord&,
        const CMappedFeat&);
    bool xAssignFeatureAttributes(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);
        
    bool xAssignFeatureAttributeCodeBreak(
        CGffFeatureRecord&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeException(
        CGffFeatureRecord&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeExonNumber(
        CGffFeatureRecord&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeDbXref(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeGbKey(
        CGffFeatureRecord&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeGene(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeGeneDesc(
        CGffFeatureRecord&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeGeneSynonym(
        CGffFeatureRecord&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeIsOrdered(
        CGffFeatureRecord&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeLocusTag(
        CGffFeatureRecord&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeOldLocusTag(
        CGffFeatureRecord&,
        const CMappedFeat&);
    bool xAssignFeatureAttributePseudoGene(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeMapLoc(
        CGffFeatureRecord&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeName(
        CGffFeatureRecord&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeNote(
        CGffFeatureRecord&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeNcrnaClass(
        CGffFeatureRecord&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeParent(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);
    bool xAssignFeatureAttributePartial(
        CGffFeatureRecord&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeProduct(
        CGffFeatureRecord&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeProteinId(
        CGffFeatureRecord&,
        const CMappedFeat&);
    bool xAssignFeatureAttributePseudo(
        CGffFeatureRecord&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeTranscriptId(
        CGffFeatureRecord&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeTranslationTable(
        CGffFeatureRecord&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeGeneBiotype(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);
    //end mss-234//
    bool xAssignFeatureAttributeModelEvidence(
        CGffFeatureRecord&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeRptFamily(
        CGffFeatureRecord&,
        const CMappedFeat&);
    string xNextGenericId();
    string xNextGeneId();
    string xNextCdsId();
    string xNextTrnaId();
    string xNextAlignId();

protected:
    unsigned int m_uRecordId;
    string m_sDefaultMethod;


    typedef map< CMappedFeat, CRef<CGffFeatureRecord> > TGeneMapNew;
    TGeneMapNew m_GeneMapNew;

    typedef map< CMappedFeat, CRef<CGffFeatureRecord> > TMrnaMapNew;
    TMrnaMapNew m_MrnaMapNew;

    bool m_SortAlignments;
    unsigned int m_uPendingGeneId;
    unsigned int m_uPendingMrnaId;
    unsigned int m_uPendingTrnaId;
    unsigned int m_uPendingCdsId;
    unsigned int m_uPendingGenericId;
    unsigned int m_uPendingAlignId;

    CBioseq_Handle m_BioseqHandle;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif  // OBJTOOLS_WRITERS___GFF3_WRITER__HPP
