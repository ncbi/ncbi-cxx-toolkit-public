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
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Score.hpp>

#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/reader_base.hpp>
#include <objtools/readers/gff2_data.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CGff2Record;

//  ----------------------------------------------------------------------------
class NCBI_XOBJREAD_EXPORT CGff2Reader
//  ----------------------------------------------------------------------------
    : public CReaderBase
{
public:
    typedef enum {  // Must be consistent with flags in CReaderBase
        fNormal =       0,
        fGenbankMode =  1 << 4,
        fRetainLocusIds = 1 << 5,
        fAssumeCircularSequence = 1 << 6,
    } TFlags;

    using IdToFeatureMap = map<string, CRef<CSeq_feat>>;
    using TScoreValueMap = map<string, CRef<CScore::TValue>>;

public:

    //
    //  object management:
    //
public:
    CGff2Reader(
        int iFlags,
        const string& name = "",
        const string& title = "",
        SeqIdResolver resolver = CReadUtil::AsSeqId,
        CReaderListener* pListener = nullptr);

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
        ILineErrorListener* =nullptr);
        
    virtual CRef< CSerialObject >
    ReadObject(
        ILineReader&,
        ILineErrorListener* =nullptr);
                
    virtual void
    ReadSeqAnnots(
        TAnnotList&,
        CNcbiIstream&,
        ILineErrorListener* =nullptr);
                        
    virtual void
    ReadSeqAnnots(
        TAnnotList&,
        ILineReader&,
        ILineErrorListener* =nullptr);

    unsigned int
    SequenceSize() const { return mSequenceSize; };

    bool
    AtSequenceData() const { return mAtSequenceData; };

    //
    // class interface:
    //
    static bool 
    IsAlignmentData(
        const string&);

    //
    //  new stuff: 
    //
    virtual void xGetData(
        ILineReader&,
        TReaderData&);

    bool IsInGenbankMode() const;

    virtual bool xParseStructuredComment(
        const string&);
    
    virtual bool xParseFeature(
        const string&,
        CSeq_annot&,
        ILineErrorListener*);
      
    virtual bool xIsCurrentDataType(
        const string&);

    virtual void xPostProcessAnnot(
        CSeq_annot&);

    virtual void xAssignAnnotId(
        CSeq_annot&,
        const string& = "");

    virtual bool x_ParseAlignmentGff(
        const string& strLine, 
        list<string>& id_list,
        map<string, list<CRef<CSeq_align>>>& alignments);

    void x_GetAlignmentScores(
        const CSeq_align& alignment,
        TScoreValueMap& score_values) const;

    void x_FindMatchingScores(
        const TScoreValueMap& scores_1,
        const TScoreValueMap& scores_2,
        set<string>& matching_scores) const;

    virtual bool x_CreateAlignment(
        const CGff2Record& gff,
        CRef<CSeq_align>& pAlign);

    bool x_MergeAlignments(
        const list<CRef<CSeq_align>>& alignment_list,
        CRef<CSeq_align>& processed);

    void x_InitializeScoreSums(const TScoreValueMap score_values,
        map<string, TSeqPos>& summed_scores) const;

    void x_ProcessAlignmentScores(const CSeq_align& alignment, 
        map<string, TSeqPos>& summed_scores,
        TScoreValueMap& common_scores) const;
     
    void x_ProcessAlignmentsGff(
        const list<string>& id_list,
        const map<string, list<CRef<CSeq_align>>>& alignments,
        CRef<CSeq_annot> pAnnot);

    virtual bool xUpdateAnnotFeature(
        const CGff2Record&,
        CSeq_annot&,
        ILineErrorListener* =0);

    virtual bool x_UpdateAnnotAlignment(
        const CGff2Record&,
        CSeq_annot&,
        ILineErrorListener* =0);

    virtual bool xAddFeatureToAnnot(
        CRef< CSeq_feat >,
        CSeq_annot& );
                            
    bool xFeatureSetQualifier(
        const string&,
        const string&,
        CRef<CSeq_feat>);

    virtual bool x_ProcessQualifierSpecialCase(
        CGff2Record::TAttrCit,
        CRef< CSeq_feat > );
  
    bool x_GetFeatureById(
        const string&,
        CRef< CSeq_feat >& );

    bool xAlignmentSetScore(
        const CGff2Record&,
        CRef<CSeq_align> );

    bool xSetDensegStarts(
        const vector<string>& gapParts, 
        ENa_strand identStrand,
        ENa_strand targetStrand,
        const TSeqPos targetStart,
        const TSeqPos targetEnd,
        const CGff2Record& gff,
        CSeq_align::C_Segs::TDenseg& denseg);

    bool xGetStartsOnMinusStrand(TSeqPos offset,
        const vector<string>& gapParts,
        bool isTarget,
        vector<int>& starts) const;

    bool xGetStartsOnPlusStrand(TSeqPos offset,
        const vector<string>& gapParts,
        bool isTarget,
        vector<int>& starts) const;

    virtual bool xAnnotPostProcess(
        CSeq_annot&);

    virtual bool xGenerateParentChildXrefs(
        CSeq_annot&);

    bool xUpdateSplicedAlignment(const CGff2Record& gff, 
                                 CRef<CSeq_align> pAlign) const;

    bool xUpdateSplicedSegment(const CGff2Record& gff, 
                               CSpliced_seg& segment) const;

    bool xSetSplicedExon(
        const CGff2Record& gff,
        CRef<CSpliced_exon> pExon) const;

    bool xGetTargetParts(const CGff2Record& gff, 
                         vector<string>& targetParts) const;

    bool xAlignmentSetSegment(
        const CGff2Record&,
        CRef<CSeq_align> );

    bool xAlignmentSetDenseg(
        const CGff2Record&,
        CRef<CSeq_align> );

    bool xAlignmentSetSpliced_seg(
        const CGff2Record&,
        CRef<CSeq_align> );

    static CRef< CDbtag >
    x_ParseDbtag(
        const string& );

    static bool xIsSequenceRegion(
        const string& line);

    static bool xIsFastaMarker(
        const string& line);

    CMessageListenerLenient m_ErrorsPrivate;
    IdToFeatureMap m_MapIdToFeature;

    virtual bool xIsIgnoredFeatureType(
        const string&);

    virtual bool xIsIgnoredFeatureId(
        const string&);

    //
    //  helpers:
    //
protected:
    virtual CGff2Record* x_CreateRecord() { return new CGff2Record(); };

    void xSetAncestryLine(
        CSeq_feat&,
        const string&);

    virtual void xSetAncestorXrefs(
        CSeq_feat&,
        CSeq_feat&);

    void xSetXrefFromTo(
        CSeq_feat&,
        CSeq_feat&);

    bool xNeedsNewSeqAnnot(
        const string&);

    //  data:
    //
protected:
    string m_CurrentSeqId;
    ILineErrorListener* m_pErrors;
    unsigned int mCurrentFeatureCount;
    bool mParsingAlignment;
    CRef<CAnnotdesc> m_CurrentBrowserInfo;
    CRef<CAnnotdesc> m_CurrentTrackInfo;
    unsigned int mSequenceSize;
    bool mAtSequenceData;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___GFF2_READER__HPP
