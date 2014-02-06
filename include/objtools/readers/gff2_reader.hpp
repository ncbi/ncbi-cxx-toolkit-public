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

#ifndef OBJTOOLS_READERS___GFF2_READER__HPP
#define OBJTOOLS_READERS___GFF2_READER__HPP

#include <corelib/ncbistd.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Cdregion.hpp>

#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/reader_base.hpp>
#include <objtools/readers/gff_reader.hpp>
#include <objtools/readers/gff2_data.hpp>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects) // namespace ncbi::objects::

class CGFFReader;
class CGff2Record;
class SRecord;

//  ----------------------------------------------------------------------------
class NCBI_XOBJREAD_EXPORT CGff2Reader
//  ----------------------------------------------------------------------------
    : public CReaderBase
{
public:
    typedef enum {
        fNormal =       0,
        fNewCode =      0x1000, // for now don't clobber CGFFReader flags
    } TFlags;

    typedef map< string, CRef< CSeq_feat > > IdToFeatureMap;
   
public:
    typedef vector< CRef< CSeq_annot > > TAnnots;
    typedef TAnnots::iterator TAnnotIt;
    typedef TAnnots::const_iterator TAnnotCit;

    //
    //  object management:
    //
public:
    CGff2Reader(
        int iFlags,
        const string& name = "",
        const string& title = "" );

    virtual ~CGff2Reader();
    
    //
    //  object interface:
    //
public:
    unsigned int 
    ObjectType() const { return OT_SEQENTRY; };
    
    CRef< CSeq_entry >
    ReadSeqEntry(
        ILineReader&,
        IMessageListener* =0 );
        
    virtual CRef< CSerialObject >
    ReadObject(
        ILineReader&,
        IMessageListener* =0 );
                
    virtual void
    ReadSeqAnnots(
        TAnnots&,
        CNcbiIstream&,
        IMessageListener* =0 );
                        
    virtual void
    ReadSeqAnnots(
        TAnnots&,
        ILineReader&,
        IMessageListener* =0 );

    virtual void
    ReadSeqAnnotsNew(
        TAnnots&,
        ILineReader&,
        IMessageListener* =0 );

    //
    // class interface:
    //
    static bool 
    IsAlignmentData(
        const string&);

    //
    //  new stuff: 
    //
    bool x_ParseBrowserLineGff(
        const string&,
        CRef< CAnnotdesc >& );
        
    virtual bool x_ParseTrackLineGff(
        const string&,
        CRef< CAnnotdesc >& );
                                
    virtual bool x_ParseStructuredCommentGff(
        const string&,
        CRef< CAnnotdesc >& );
    
    virtual bool x_ParseDataGff(
        const string&,
        TAnnots&,
        IMessageListener*);

    virtual bool x_ParseFeatureGff(
        const string&,
        TAnnots&,
        IMessageListener*);

    virtual bool x_ParseAlignmentGff(
        const string&,
        TAnnots& );

    virtual bool x_InitAnnot(
        const CGff2Record&,
        CRef< CSeq_annot > );

    virtual bool x_UpdateAnnotFeature(
        const CGff2Record&,
        CRef< CSeq_annot >,
        IMessageListener* =0);

    virtual bool x_UpdateAnnotAlignment(
        const CGff2Record&,
        CRef< CSeq_annot > );

    virtual bool x_AddFeatureToAnnot(
        CRef< CSeq_feat >,
        CRef< CSeq_annot > );
                            
    virtual bool x_FeatureSetId(
        const CGff2Record&,
        CRef< CSeq_feat > );

    virtual bool x_FeatureSetXref(
        const CGff2Record&,
        CRef< CSeq_feat > );

    bool x_FeatureSetQualifiers(
        const CGff2Record&,
        CRef< CSeq_feat > );

    bool x_FeatureTrimQualifiers(
        const CGff2Record&,
        CRef< CSeq_feat > );

    virtual bool x_ProcessQualifierSpecialCase(
        CGff2Record::TAttrCit,
        CRef< CSeq_feat > );
  
    virtual bool x_FeatureSetLocation(
        const CGff2Record&,
        CRef< CSeq_feat > );
    
    bool x_FeatureSetGffInfo(
        const CGff2Record&,
        CRef< CSeq_feat > );
    
    bool x_FeatureSetData(
        const CGff2Record&,
        CRef< CSeq_feat > );
    
    virtual bool x_FeatureSetDataGene(
        const CGff2Record&,
        CRef< CSeq_feat > );

    virtual bool x_FeatureSetDataMRNA(
        const CGff2Record&,
        CRef< CSeq_feat > );

    virtual bool x_FeatureSetDataCDS(
        const CGff2Record&,
        CRef< CSeq_feat > );

    bool x_FeatureSetDataExon(
        const CGff2Record&,
        CRef< CSeq_feat > );

    bool x_FeatureSetDataMiscFeature(
        const CGff2Record&,
        CRef< CSeq_feat > );

    bool x_GetFeatureById(
        const string&,
        CRef< CSeq_feat >& );

    bool x_GetParentFeature(
        const CSeq_feat&,
        CRef< CSeq_feat >& );

    bool xGetExistingFeature(
        const CSeq_feat&,
        CRef<CSeq_annot>,
        CRef< CSeq_feat >& );

    bool xFeatureMergeExon(
        CRef< CSeq_feat >,
        CRef< CSeq_feat > );

    bool xFeatureMergeCds(
        CRef< CSeq_feat >,
        CRef< CSeq_feat > );

    bool xAlignmentSetScore(
        const CGff2Record&,
        CRef<CSeq_align> );

    virtual bool xReadInit();

    virtual bool xAnnotPostProcess(
        CRef<CSeq_annot>);

    bool xAlignmentSetSegment(
        const CGff2Record&,
        CRef<CSeq_align> );

    static CRef< CDbtag >
    x_ParseDbtag(
        const string& );

    CMessageListenerLenient m_ErrorsPrivate;
    IdToFeatureMap m_MapIdToFeature;

    static string GenbankKey(
        CSeqFeatData::ESubtype );

    bool x_HasTemporaryLocation(
        const CSeq_feat& );

    static bool IsExon(
        CRef< CSeq_feat > );

    static bool IsCds(
        CRef<CSeq_feat>);

    //
    //  helpers:
    //
protected:
    virtual CGff2Record* x_CreateRecord() { return new CGff2Record(); };

    virtual void x_SetTrackDataToSeqEntry(
        CRef<CSeq_entry>&,
        CRef<CUser_object>&,
        const string&,
        const string& );
                
    bool x_IsCommentLine(
        const CTempString& );

    bool x_ReadLine(
        ILineReader&,
        string& );

    void x_PlaceFeature(
        CSeq_feat& feat, 
        const SRecord& );

    static bool s_GetAnnotId(
        const CSeq_annot&,
        string& );

    //  data:
    //
protected:
//    TFlags             m_uFlags;
    IMessageListener*   m_pErrors;
    CRef< CAnnotdesc > m_CurrentTrackInfo;
    CRef< CAnnotdesc > m_CurrentBrowserInfo;
    string             m_AnnotName;
    string             m_AnnotTitle;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___GFF2_READER__HPP
