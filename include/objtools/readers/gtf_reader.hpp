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
 * Author: Frank Ludwig
 *
 * File Description:
 *   BED file reader
 *
 */

#ifndef OBJTOOLS_READERS___GTF_READER__HPP
#define OBJTOOLS_READERS___GTF_READER__HPP

#include <corelib/ncbistd.hpp>
#include <objtools/readers/gff3_reader.hpp>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects) // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
class NCBI_XOBJREAD_EXPORT CGtfReader
//  ----------------------------------------------------------------------------
    : public CGff3Reader
{
public:
    CGtfReader();

    virtual ~CGtfReader();
    
    virtual void
    ReadSeqAnnots(
        TAnnots&,
        CNcbiIstream&,
        IErrorContainer* =0 );
                        
    virtual void
    ReadSeqAnnots(
        TAnnots&,
        ILineReader&,
        IErrorContainer* =0 );

protected:
    bool
    x_GetLine(
        ILineReader&,
        string&,
        int& );

    bool x_ParseFeatureGff(
        const string& strLine,
        TAnnots& annots );

    virtual bool x_UpdateAnnot(
        const CGff3Record&,
        CRef< CSeq_annot > );

    virtual bool x_UpdateAnnotCds(
        const CGff3Record&,
        CRef< CSeq_annot > );

    virtual bool x_UpdateAnnotStartCodon(
        const CGff3Record&,
        CRef< CSeq_annot > );

    virtual bool x_UpdateAnnotStopCodon(
        const CGff3Record&,
        CRef< CSeq_annot > );

    virtual bool x_UpdateAnnot5utr(
        const CGff3Record&,
        CRef< CSeq_annot > );

    virtual bool x_UpdateAnnot3utr(
        const CGff3Record&,
        CRef< CSeq_annot > );

    virtual bool x_UpdateAnnotInter(
        const CGff3Record&,
        CRef< CSeq_annot > );

    virtual bool x_UpdateAnnotInterCns(
        const CGff3Record&,
        CRef< CSeq_annot > );

    virtual bool x_UpdateAnnotIntronCns(
        const CGff3Record&,
        CRef< CSeq_annot > );

    virtual bool x_UpdateAnnotExon(
        const CGff3Record&,
        CRef< CSeq_annot > );

    virtual bool x_UpdateAnnotMiscFeature(
        const CGff3Record&,
        CRef< CSeq_annot > );

    bool x_UpdateFeatureId(
        const CGff3Record&,
        CRef< CSeq_feat > );

    bool x_CreateFeatureLocation(
        const CGff3Record&,
        CRef< CSeq_feat > );
    
    bool x_CreateGeneXref(
        const CGff3Record&,
        CRef< CSeq_feat > );
    
    bool x_MergeFeatureLocationSingleInterval(
        const CGff3Record&,
        CRef< CSeq_feat > );
    
    bool x_MergeFeatureLocationMultiInterval(
        const CGff3Record&,
        CRef< CSeq_feat > );

    bool x_CreateParentGene(
        const CGff3Record&,
        CRef< CSeq_annot > );
        
    bool x_MergeParentGene(
        const CGff3Record&,
        CRef< CSeq_feat > );
            
    bool x_CreateParentCds(
        const CGff3Record&,
        CRef< CSeq_annot > );
        
    bool x_CreateParentMrna(
        const CGff3Record&,
        CRef< CSeq_annot > );
        
    bool x_MergeParentCds(
        const CGff3Record&,
        CRef< CSeq_feat > );
            
    bool x_FeatureSetDataGene(
        const CGff3Record&,
        CRef< CSeq_feat > );

    bool x_FeatureSetDataMRNA(
        const CGff3Record&,
        CRef< CSeq_feat > );

    bool x_FeatureSetDataCDS(
        const CGff3Record&,
        CRef< CSeq_feat > );

protected:
    bool x_FindParentGene(
        const CGff3Record&,
        CRef< CSeq_feat >& );

    bool x_FindParentCds(
        const CGff3Record&,
        CRef< CSeq_feat >& );

    bool x_FindParentMrna(
        const CGff3Record&,
        CRef< CSeq_feat >& );

    bool x_CdsIsPartial(
        const CGff3Record& );

    bool x_SkipAttribute(
        const CGff3Record&,
        const string& ) const;

    typedef map< string, CRef< CSeq_feat > > TIdToFeature;
    TIdToFeature m_GeneMap;
    TIdToFeature m_CdsMap;
    TIdToFeature m_MrnaMap;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___GTF_READER__HPP
