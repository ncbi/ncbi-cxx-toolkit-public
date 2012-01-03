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
 * File Description:  Write gtf file
 *
 */

#ifndef OBJTOOLS_WRITERS___GTF_WRITER__HPP
#define OBJTOOLS_READERS___GTF_WRITER__HPP

#include <corelib/ncbistd.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objtools/writers/gff3_write_data.hpp>
//#include <objtools/writers/gff_writer.hpp>
#include <objmgr/util/feature.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

class CGtfRecord;

//  ============================================================================
class NCBI_XOBJWRITE_EXPORT CGtfWriter:
    public CGff2Writer
//  ============================================================================
{
public:
    enum {
        fStructibutes = 1<<16,
        fNoGeneFeatures = 1<<17,
        fNoExonNumbers = 1<<18
    };

    CGtfWriter(
        CScope&,
        CNcbiOstream&,
        unsigned int = 0 );
    CGtfWriter(
        CNcbiOstream&,
        unsigned int = 0 );
    ~CGtfWriter();

    bool WriteHeader();
    virtual bool WriteHeader(
        const CSeq_annot& annot) { return CGff2Writer::WriteHeader(annot); };

protected:
    bool x_WriteRecord( 
        const CGffWriteRecord* );

    virtual bool x_WriteFeature(
        feature::CFeatTree&,
        CMappedFeat );
    virtual bool x_WriteFeatureGene(
        feature::CFeatTree&,
        CMappedFeat );
    virtual bool x_WriteFeatureMrna(
        feature::CFeatTree&,
        CMappedFeat );
    virtual bool x_WriteFeatureCds(
        feature::CFeatTree&,
        CMappedFeat );
    virtual bool x_WriteFeatureCdsFragments(
        CGtfRecord&,
        const CSeq_loc& );

    bool x_SplitCdsLocation(
        const CSeq_feat&,
        CRef< CSeq_loc >&,
        CRef< CSeq_loc >&,
        CRef< CSeq_loc >& ) const;

    SAnnotSelector x_GetAnnotSelector();

    typedef map< int, CRef< CSeq_interval > > TExonMap;
    typedef TExonMap::const_iterator TExonCit;
    TExonMap m_exonMap;
//    unsigned int m_uFlags;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif  // OBJTOOLS_WRITERS___GTF_WRITER__HPP
