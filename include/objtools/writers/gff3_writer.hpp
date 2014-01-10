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

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

class CGffAlignmentRecord;

//  ============================================================================
class NCBI_XOBJWRITE_EXPORT CGff3Writer
//  ============================================================================
    : public CGff2Writer
{
public:
    typedef enum {
        fExtraQuals = (fSoQuirks << 1),
        fGff3WriterLast = fExtraQuals,
    } TFlags;
    
public:
    CGff3Writer(
        CScope&,
        CNcbiOstream&,
        unsigned int = fNormal );
    CGff3Writer(
        CNcbiOstream&,
        unsigned int = fNormal );
    virtual ~CGff3Writer();

    virtual bool WriteHeader();
    virtual bool WriteHeader(
        const CSeq_annot& annot) { return CGff2Writer::WriteHeader(annot); };
    bool WriteAlign(
        const CSeq_align&,
        const string& asmblyName="",
        const string& asmblyAccession="" );


protected:
    virtual bool x_WriteBioseqHandle(
        CBioseq_Handle );
	virtual bool x_WriteSeqAnnotHandle(
        CSeq_annot_Handle );

    bool xWriteAlign( 
        const CSeq_align&);
    virtual bool xWriteAlignDenseg(
        const CSeq_align&);
    bool xWriteAlignSpliced(
        const CSeq_align&);
    virtual bool xWriteAlignDisc(
        const CSeq_align&);

    virtual bool xAssignFeatureAttributeParentGene(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CMappedFeat );

    virtual bool xAssignFeatureAttributeParentMrna(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CMappedFeat );

    virtual bool xWriteSequenceHeader(
        CBioseq_Handle );
    virtual bool xWriteSequenceHeader(
        CSeq_id_Handle );
    virtual bool xWriteSource(
        CGffFeatureContext& );
    virtual bool xWriteFeature(
        CGffFeatureContext&,
        CMappedFeat );
    virtual bool xWriteFeatureGene(
        CGffFeatureContext&,
        CMappedFeat );
    virtual bool xWriteFeatureRna(
        CGffFeatureContext&,
        CMappedFeat );
    virtual bool xWriteFeatureCds(
        CGffFeatureContext&,
        CMappedFeat );
    virtual bool xWriteFeatureGeneric(
        CGffFeatureContext&,
        CMappedFeat );
    virtual bool xWriteFeatureTrna(
        CGffFeatureContext&,
        CMappedFeat );

    virtual bool xWriteFeatureRecord( 
        const CGffFeatureRecord& );

    void xWriteAlignment( 
        const CGffAlignmentRecord& record );

    virtual bool xWriteFeatureRecords(
        const CGffFeatureRecord&,
        const CSeq_loc&,
        unsigned int );

    string x_GetParentId(
        CMappedFeat );

    bool xAssignSource(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CBioseq_Handle);
    bool xAssignSourceType(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CBioseq_Handle);
    bool xAssignSourceSeqId(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CBioseq_Handle);
    bool xAssignSourceSource(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CBioseq_Handle);
    bool xAssignSourceEndpoints(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CBioseq_Handle);
    bool xAssignSourceAttributes(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CBioseq_Handle);
    bool xAssignSourceAttributeGbKey(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CBioseq_Handle);
    bool xAssignSourceAttributeMolType(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CBioseq_Handle);
    bool xAssignSourceAttributeIsCircular(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CBioseq_Handle);
    bool xAssignSourceAttributesBioSource(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CBioseq_Handle);

    bool xAssignSourceAttributeGenome(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CBioSource&);
    bool xAssignSourceAttributeName(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CBioSource&);
    bool xAssignSourceAttributeDbxref(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CBioSource&);
    bool xAssignSourceAttributesOrgMod(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CBioSource&);
    bool xAssignSourceAttributesSubSource(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        const CBioSource&);

    //begin mss-234//
    bool xAssignFeature(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CMappedFeat);
    bool xAssignFeatureBasic(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CMappedFeat);
    bool xAssignFeatureType(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CMappedFeat);
    bool xAssignFeatureSeqId(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CMappedFeat);
    bool xAssignFeatureSource(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CMappedFeat);
    bool xAssignFeatureEndpoints(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CMappedFeat);
    bool xAssignFeatureScore(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CMappedFeat);
    bool xAssignFeatureStrand(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CMappedFeat);
    bool xAssignFeaturePhase(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CMappedFeat);
    bool xAssignFeatureAttributes(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CMappedFeat);
        
    bool xAssignFeatureAttributeCodeBreak(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CMappedFeat);
    bool xAssignFeatureAttributeException(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CMappedFeat);
    bool xAssignFeatureAttributeExonNumber(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CMappedFeat);
    bool xAssignFeatureAttributeDbXref(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CMappedFeat);
    bool xAssignFeatureAttributeGbKey(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CMappedFeat);
    bool xAssignFeatureAttributeGene(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CMappedFeat);
    bool xAssignFeatureAttributeGeneDesc(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CMappedFeat);
    bool xAssignFeatureAttributeGeneSynonym(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CMappedFeat);
    bool xAssignFeatureAttributeIsOrdered(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CMappedFeat);
    bool xAssignFeatureAttributeLocusTag(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CMappedFeat);
    bool xAssignFeatureAttributeOldLocusTag(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CMappedFeat);
    bool xAssignFeatureAttributeMapLoc(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CMappedFeat);
    bool xAssignFeatureAttributeName(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CMappedFeat);
    bool xAssignFeatureAttributeNote(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CMappedFeat);
    bool xAssignFeatureAttributeNcrnaClass(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CMappedFeat);
    bool xAssignFeatureAttributeParent(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CMappedFeat);
    bool xAssignFeatureAttributePartial(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CMappedFeat);
    bool xAssignFeatureAttributeProduct(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CMappedFeat);
    bool xAssignFeatureAttributeProteinId(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CMappedFeat);
    bool xAssignFeatureAttributePseudo(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CMappedFeat);
    bool xAssignFeatureAttributeTranscriptId(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CMappedFeat);
    bool xAssignFeatureAttributeTranslationTable(
        CGffFeatureRecord&,
        CGffFeatureContext&,
        CMappedFeat);
    //end mss-234//
    string xNextGenericId();
    string xNextCdsId();
    string xNextTrnaId();

protected:
    unsigned int m_uRecordId;
 
    typedef map< CMappedFeat, CRef<CGffFeatureRecord> > TGeneMapNew;
    TGeneMapNew m_GeneMapNew;
    typedef map< CMappedFeat, CRef<CGff3WriteRecordFeature> > TGeneMap;
    TGeneMap m_GeneMap;

    typedef map< CMappedFeat, CRef<CGffFeatureRecord> > TMrnaMapNew;
    TMrnaMapNew m_MrnaMapNew;
    typedef map< CMappedFeat, CRef<CGff3WriteRecordFeature> > TMrnaMap;
    TMrnaMap m_MrnaMap;

    unsigned int m_uPendingGeneId;
    unsigned int m_uPendingMrnaId;
    unsigned int m_uPendingTrnaId;
    unsigned int m_uPendingCdsId;
    unsigned int m_uPendingGenericId;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif  // OBJTOOLS_WRITERS___GFF3_WRITER__HPP
