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
 * File Description:  Write vcf file
 *
 */

#ifndef OBJTOOLS_WRITERS___VCF_WRITER__HPP
#define OBJTOOLS_READERS___VCF_WRITER__HPP

#include <objtools/writers/writer.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE


//  ============================================================================
class NCBI_XOBJWRITE_EXPORT CVcfWriter :
    public CWriterBase 
//  ============================================================================
{
public:
    typedef enum {
        fNormal =       0,
    } TFlags;
    
public:
    CVcfWriter(
        CScope&,
        CNcbiOstream&,
        TFlags = fNormal );

    virtual ~CVcfWriter();

    bool WriteAnnot( 
        const CSeq_annot&,
        const string& = "",
        const string& = "" );

protected:
    bool x_WriteInit(
        const CSeq_annot& );
    bool x_WriteMeta(
        const CSeq_annot& );
    bool x_WriteHeader(
        const CSeq_annot& );
    bool x_WriteData(
        const CSeq_annot& );

    bool x_WriteMetaCreateNew(
        const CSeq_annot& );
    bool x_WriteFeature(
        feature::CFeatTree&,
        CMappedFeat );

    bool x_WriteFeatureChrom(
        feature::CFeatTree&,
        CMappedFeat );
        
    bool x_WriteFeaturePos(
        feature::CFeatTree&,
        CMappedFeat );
        
    bool x_WriteFeatureId(
        feature::CFeatTree&,
        CMappedFeat );
        
    bool x_WriteFeatureRef(
        feature::CFeatTree&,
        CMappedFeat );
        
    bool x_WriteFeatureAlt(
        feature::CFeatTree&,
        CMappedFeat );
        
    bool x_WriteFeatureQual(
        feature::CFeatTree&,
        CMappedFeat );
        
    bool x_WriteFeatureFilter(
        feature::CFeatTree&,
        CMappedFeat );
        
    bool x_WriteFeatureInfo(
        feature::CFeatTree&,
        CMappedFeat );
        
    bool x_WriteFeatureGenotypeData(
        feature::CFeatTree&,
        CMappedFeat );
    
    CScope& m_Scope;
    vector<string> m_GenotypeHeaders;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif  // OBJTOOLS_WRITERS___VCF_WRITER__HPP
