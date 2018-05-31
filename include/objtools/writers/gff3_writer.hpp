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
#include <objtools/writers/gff_feature_record.hpp>
#include <objtools/alnmgr/alnmap.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_exon.hpp>

#include <objtools/writers/gff3_idgen.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE


//  ============================================================================
class CGff3SourceRecord
    //  ============================================================================
    : public CGffSourceRecord
{
public:
    CGff3SourceRecord()
        : CGffSourceRecord("")  {};
    CGff3SourceRecord(const CGff3SourceRecord& rhs)
        : CGffSourceRecord(rhs), mRecordId(rhs.mRecordId)  {};
    void SetRecordId(
        const string& recordId) { mRecordId = recordId; };
    string Id() const { return mRecordId; };

    string StrAttributes() const {
        string attributes;
        attributes.reserve(256);

        if (!mRecordId.empty()) {
            attributes += "ID=";
            attributes += mRecordId;
        }
        auto baseAttributes = CGffBaseRecord::StrAttributes();
        if (!baseAttributes.empty()) {
            attributes +=  ATTR_SEPARATOR;
            attributes += baseAttributes;
        }
        return attributes;   
    }
protected:
    string mRecordId;
};

//  ============================================================================
class CGff3FeatureRecord
//  ============================================================================
    : public CGffFeatureRecord
{
public:
    CGff3FeatureRecord()
        : CGffFeatureRecord("") 
    {};
    
    CGff3FeatureRecord(const CGff3FeatureRecord& rhs) 
        : CGffFeatureRecord(rhs), mRecordId(rhs.mRecordId), mParent(rhs.mParent) 
    {};

    void SetRecordId(
        const string& recordId) { mRecordId = recordId; };
    void SetParent(
        const string& parent) { mParent = parent; };
    string Id() const { return mRecordId; };
    string Parent() const { return mParent; };

    string StrAttributes() const {
        string attributes;
        attributes.reserve(256);

        if (!mRecordId.empty()) {
            attributes += "ID=";
            attributes += mRecordId;
        }
        if (!mParent.empty()) {
            if (!attributes.empty()) {
                attributes += ATTR_SEPARATOR;
            }
            attributes += "Parent=";
            attributes += mParent;
        }
        auto baseAttributes = CGffBaseRecord::StrAttributes();
        if (!baseAttributes.empty()) {
            attributes +=  ATTR_SEPARATOR;
            attributes += baseAttributes;
        }
        return attributes;   
    }

protected:
    string mRecordId;
    string mParent;

};


//  ============================================================================
class NCBI_XOBJWRITE_EXPORT CGff3Writer
//  ============================================================================
    : public CGff2Writer, public CFeatWriter, public CAlignWriter
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

    virtual bool WriteHeader() override;
    virtual bool WriteHeader(
        const CSeq_annot& annot) override { return CGff2Writer::WriteHeader(annot); };
    bool WriteAlign(
        const CSeq_align&,
        const string& asmblyName="",
        const string& asmblyAccession="" ) override;

protected:
    typedef list<pair<CConstRef<CSeq_align>, string>> TAlignCache;

protected:
    virtual bool x_WriteBioseqHandle(
        CBioseq_Handle ) override;
	virtual bool x_WriteSeqAnnotHandle(
        CSeq_annot_Handle );

    virtual bool xWriteAlign(
        CAlign_CI align_it) override;
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
        const CGffAlignRecord& record );

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
        CGff3SourceRecord&,
        CBioseq_Handle);
    bool xAssignSourceType(
        CGff3SourceRecord&);
    bool xAssignSourceSeqId(
        CGff3SourceRecord&,
        CBioseq_Handle);
    bool xAssignSourceMethod(
        CGff3SourceRecord&,
        CBioseq_Handle);
    bool xAssignSourceEndpoints(
        CGff3SourceRecord&,
        CBioseq_Handle);
    bool xAssignSourceAttributes(
        CGff3SourceRecord&,
        CBioseq_Handle);
    bool xAssignSourceAttributeGbKey(
        CGff3SourceRecord&);
    bool xAssignSourceAttributeMolType(
        CGff3SourceRecord&,
        CBioseq_Handle);
    bool xAssignSourceAttributeIsCircular(
        CGff3SourceRecord&,
        CBioseq_Handle);
    bool xAssignSourceAttributesBioSource(
        CGff3SourceRecord&,
        CBioseq_Handle);

    bool xAssignSourceAttributeGenome(
        CGff3SourceRecord&,
        const CBioSource&);
    bool xAssignSourceAttributeName(
        CGff3SourceRecord&,
        const CBioSource&);
    bool xAssignSourceAttributeDbxref(
        CGff3SourceRecord&,
        const CBioSource&);
    bool xAssignSourceAttributesOrgMod(
        CGff3SourceRecord&,
        const CBioSource&);
    bool xAssignSourceAttributesSubSource(
        CGff3SourceRecord&,
        const CBioSource&);

    //begin mss-234//
    virtual bool xAssignFeature(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&) override;
    virtual bool xAssignFeatureType(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&) override;
    virtual bool xAssignFeatureMethod(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&) override;
    virtual bool xAssignFeatureEndpoints(
        CGffFeatureRecord& record,
        CGffFeatureContext&,
        const CMappedFeat& mapped_feat) override;
    virtual bool xAssignFeatureStrand(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);
    virtual bool xAssignFeaturePhase(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&) override;

    virtual bool xAssignFeatureAttributesFormatIndependent(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&) override;

    virtual bool xAssignFeatureAttributesFormatSpecific(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&) override;
    bool xAssignFeatureAttributeParent(
        CGff3FeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeID(
        CGff3FeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);
    virtual bool xAssignFeatureAttributeParentMrna(
        CGff3FeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat& );
    virtual bool xAssignFeatureAttributeParentpreRNA(
        CGff3FeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);
    virtual bool xAssignFeatureAttributeParentVDJsegmentCregion(
        CGff3FeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat&);
    virtual bool xAssignFeatureAttributeParentGene(
        CGff3FeatureRecord&,
        CGffFeatureContext&,
        const CMappedFeat& );

    bool xAssignFeatureAttributeCodeBreak(
        CGffFeatureRecord&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeEcNumbers(
        CGffFeatureRecord&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeException(
        CGffFeatureRecord&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeExperiment(
        CGffFeatureRecord&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeExonNumber(
        CGffFeatureRecord&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeFunction(
        CGffFeatureRecord&,
        const CMappedFeat&);
    bool xAssignFeatureAttributesGoMarkup(
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
    bool xAssignFeatureAttributesQualifiers(
        CGffFeatureRecord&,
        const CMappedFeat&);
    //end mss-234//
    bool xAssignFeatureAttributeModelEvidence(
        CGffFeatureRecord&,
        const CMappedFeat&);
    bool xAssignFeatureAttributeRptFamily(
        CGffFeatureRecord&,
        const CMappedFeat&);

    string xNextAlignId();

protected:
    unsigned int m_uRecordId;
    string m_sDefaultMethod;

    using TFeatureMap = map<CMappedFeat, CRef<CGff3FeatureRecord>>;

    using TGeneMapNew  = TFeatureMap;
    TGeneMapNew m_GeneMapNew;

    using TMrnaMapNew = TFeatureMap;
    TMrnaMapNew m_MrnaMapNew;

    TFeatureMap m_PrernaMapNew;

    TFeatureMap m_VDJsegmentCregionMapNew;


    bool m_SortAlignments;
    unsigned int m_uPendingGeneId;
    unsigned int m_uPendingMrnaId;
    unsigned int m_uPendingTrnaId;
    unsigned int m_uPendingCdsId;
    unsigned int m_uPendingGenericId;
    unsigned int m_uPendingAlignId;

    CGffIdGenerator m_idGenerator;
    CBioseq_Handle m_BioseqHandle;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif  // OBJTOOLS_WRITERS___GFF3_WRITER__HPP
