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
#include <objects/seqfeat/Seq_feat.hpp>
#include <objtools/writers/gff_writer.hpp>
#include <objtools/writers/gff3_write_data.hpp>

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

    bool x_WriteAlign( 
        const CSeq_align&,
        bool=false );                   // invert width 
    virtual bool x_WriteAlignDenseg(
        const CSeq_align&,
        bool=false );                   // invert width 
    bool x_WriteAlignSpliced(
        const CSeq_align&,    
        bool=false );                   // invert width 
    virtual bool x_WriteAlignDisc(
        const CSeq_align&,    
        bool=false );                   // invert width 

    virtual bool xTryAssignGeneParent(
        CGff3WriteRecordFeature&,
        CGffFeatureContext&,
        CMappedFeat );

    virtual bool xTryAssignMrnaParent(
        CGff3WriteRecordFeature&,
        CGffFeatureContext&,
        CMappedFeat );

    virtual bool x_WriteSequenceHeader(
        CBioseq_Handle );
    virtual bool x_WriteSequenceHeader(
        CSeq_id_Handle );
    virtual bool x_WriteSource(
        CGffFeatureContext& );
    virtual bool x_WriteFeature(
        CGffFeatureContext&,
        CMappedFeat );
    virtual bool x_WriteFeatureGene(
        CGffFeatureContext&,
        CMappedFeat );
    virtual bool x_WriteFeatureRna(
        CGffFeatureContext&,
        CMappedFeat );
    virtual bool x_WriteFeatureCds(
        CGffFeatureContext&,
        CMappedFeat );
    virtual bool x_WriteFeatureGeneric(
        CGffFeatureContext&,
        CMappedFeat );
    virtual bool x_WriteFeatureTrna(
        CGffFeatureContext&,
        CMappedFeat );

    void x_WriteAlignment( 
        const CGffAlignmentRecord& record );

    virtual bool x_WriteFeatureRecords(
        const CGff3WriteRecordFeature&,
        const CSeq_loc&,
        unsigned int );

    virtual bool xAssignRecordFromAsn(
        const CMappedFeat&, 
        CGffFeatureContext&,
        CGff3WriteRecordFeature&);

    string x_GetParentId(
        CMappedFeat );

protected:
    unsigned int m_uRecordId;
 
    typedef map< CMappedFeat, CRef<CGff3WriteRecordFeature> > TGeneMap;
    TGeneMap m_GeneMap;
    unsigned int m_uPendingGeneId;

    typedef map< CMappedFeat, CRef<CGff3WriteRecordFeature> > TMrnaMap;
    TMrnaMap m_MrnaMap;
    unsigned int m_uPendingMrnaId;
    unsigned int m_uPendingTrnaId;
    unsigned int m_uPendingCdsId;
    unsigned int m_uPendingGenericId;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif  // OBJTOOLS_WRITERS___GFF3_WRITER__HPP
