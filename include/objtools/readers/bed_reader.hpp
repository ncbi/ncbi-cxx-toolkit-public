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

#ifndef OBJTOOLS_READERS___BEDREADER__HPP
#define OBJTOOLS_READERS___BEDREADER__HPP

#include <corelib/ncbistd.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objtools/readers/reader_base.hpp>
BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CLinePreBuffer;
class CBedAutoSql;
class CBedColumnData;

//  ----------------------------------------------------------------------------
class NCBI_XOBJREAD_EXPORT CRawBedRecord
//  ----------------------------------------------------------------------------
{
public:
    CRawBedRecord(): m_score(-1) {};

    virtual ~CRawBedRecord() {};

    void SetInterval(
        CSeq_id& id,
        unsigned int start,
        unsigned int stop,
        ENa_strand strand);

    void SetScore(
        unsigned int score);

    void Dump(
        CNcbiOstream& ostr) const;

public:
    CRef<CSeq_interval> m_pInterval;
    int m_score;
};


//  ----------------------------------------------------------------------------
class NCBI_XOBJREAD_EXPORT CRawBedTrack
//  ----------------------------------------------------------------------------
{
public:
    CRawBedTrack() {};
    ~CRawBedTrack() {};

public:
    void Dump(
        CNcbiOstream& ostr) const;

    void Reset() { m_Records.clear(); };
    void AddRecord(
        CRawBedRecord& record) { m_Records.push_back(record); };
    const vector<CRawBedRecord>& Records() const { return m_Records; };
    bool HasData() const { return (!m_Records.empty()); };

public:
    CRef<CSeq_id> m_pId;
    vector<CRawBedRecord> m_Records;
};


//  ----------------------------------------------------------------------------
/// CReaderBase implementation that reads BED data files, either a single object
/// or all objects found. For the purpose of CBedReader, an object consists of
/// a run of records all with the same ID (BED comlumn 1), and all contained
/// within a single track.
///
class NCBI_XOBJREAD_EXPORT CBedReader
//  ----------------------------------------------------------------------------
    : public CReaderBase
{
    //
    //  object management:
    //
public:
    CBedReader( 
        int = fNormal,
        const string& = "",
        const string& = "",
        CReaderListener* = nullptr);
    virtual ~CBedReader();
    
    //
    //  object interface:
    //
public:
    enum EBedFlags {
        fThreeFeatFormat = 1<<8,
        fDirectedFeatureModel = 1<<9,
        fAutoSql = 1<<10,
        fAddDefaultColumns = 1<<11,
    };
    typedef int TFlags;

    /// Read a single object from given line reader containing BED data. The
    /// resulting Seq-annot will contain a feature table.
    /// @param lr
    ///   line reader to read from.
    /// @param pErrors
    ///   pointer to optional error container object. 
    ///  
    virtual CRef< CSeq_annot >
    ReadSeqAnnot(
        ILineReader& lr,
        ILineErrorListener* pErrors=0 );

    virtual bool 
    ReadTrackData(
        ILineReader&,
        CRawBedTrack&,
        ILineErrorListener* =0 );

    virtual bool
    SetAutoSql(
        const string&);

    virtual bool
    SetAutoSql(
        CNcbiIstream&);

protected:
    virtual CRef<CSeq_annot> xCreateSeqAnnot();

    virtual void xGetData(
        ILineReader&,
        TReaderData&);

    virtual void xProcessData(
        const TReaderData&,
        CSeq_annot&);

    virtual bool xDetermineLikelyColumnCount(
        CLinePreBuffer&,
        ILineErrorListener*);

    virtual bool xParseTrackLine(
        const string&);
  
    bool xParseFeature(
        const SReaderLine&,
        CSeq_annot&,
        ILineErrorListener*);
      
    bool xParseFeatureAutoSql(
        const CBedColumnData&,
        CSeq_annot&,
        ILineErrorListener*);

    bool xParseFeatureUserFormat(
        const CBedColumnData&,
        CSeq_annot&,
        ILineErrorListener*);

    bool xParseFeatureThreeFeatFormat(
        const CBedColumnData&,
        CSeq_annot&,
        ILineErrorListener*);

    bool xParseFeatureGeneModelFormat(
        const CBedColumnData&,
        CSeq_annot&,
        ILineErrorListener*);

    bool xAppendFeatureChrom(
        const CBedColumnData&,
        CSeq_annot&,
        unsigned int,
        ILineErrorListener*);

    bool xAppendFeatureThick(
        const CBedColumnData&,
        CSeq_annot&,
        unsigned int,
        ILineErrorListener*);

    bool xAppendFeatureBlock(
        const CBedColumnData&,
        CSeq_annot&,
        unsigned int,
        ILineErrorListener*);

    CRef<CSeq_feat> xAppendFeatureGene(
        const CBedColumnData&,
        CSeq_annot&,
        unsigned int,
        ILineErrorListener*);

    CRef<CSeq_feat> xAppendFeatureRna(
        const CBedColumnData&,
        CSeq_annot&,
        unsigned int,
        ILineErrorListener*);

    CRef<CSeq_feat> xAppendFeatureCds(
        const CBedColumnData&,
        CSeq_annot&,
        unsigned int,
        ILineErrorListener*);

    void xSetFeatureLocation(
        CRef<CSeq_feat>&,
        const CBedColumnData&);
        
    void xSetFeatureLocationChrom(
        CRef<CSeq_feat>&,
        const CBedColumnData&);
    void xSetFeatureLocationGene(
        CRef<CSeq_feat>&,
        const CBedColumnData&);
        
    void xSetFeatureLocationThick(
        CRef<CSeq_feat>&,
        const CBedColumnData&);
    void xSetFeatureLocationCds(
        CRef<CSeq_feat>&,
        const CBedColumnData&);
        
    void xSetFeatureLocationBlock(
        CRef<CSeq_feat>&,
        const CBedColumnData&);
    void xSetFeatureLocationRna(
        CRef<CSeq_feat>&,
        const CBedColumnData&);
        
    void xSetFeatureIdsChrom(
        CRef<CSeq_feat>&,
        const CBedColumnData&,
        unsigned int);
    void xSetFeatureIdsGene(
        CRef<CSeq_feat>&,
        const CBedColumnData&,
        unsigned int);
        
    void xSetFeatureIdsThick(
        CRef<CSeq_feat>&,
        const CBedColumnData&,
        unsigned int);
    void xSetFeatureIdsCds(
        CRef<CSeq_feat>&,
        const CBedColumnData&,
        unsigned int);
        
    void xSetFeatureIdsBlock(
        CRef<CSeq_feat>&,
        const CBedColumnData&,
        unsigned int);
    void xSetFeatureIdsRna(
        CRef<CSeq_feat>&,
        const CBedColumnData&,
        unsigned int);
        
    void xSetFeatureBedData(
        CRef<CSeq_feat>&,
        const CBedColumnData&,
        ILineErrorListener*);
        
    void xSetFeatureTitle(
        CRef<CSeq_feat>&,
        const CBedColumnData&);
        
    void xSetFeatureScore(
        CRef<CUser_object>,
        const CBedColumnData&);
    void xSetFeatureColor(
        CRef<CUser_object>,
        const CBedColumnData&,
        ILineErrorListener*);

    void xSetFeatureColorFromItemRgb(
        CRef<CUser_object>,
        const string&,
        ILineErrorListener*);
    void xSetFeatureColorFromScore(
        CRef<CUser_object>,
        const string&);
    void xSetFeatureColorByStrand(
        CRef<CUser_object>,
        const string&,
        ENa_strand,
        ILineErrorListener*);
    void xSetFeatureColorDefault(
        CRef<CUser_object>);

    bool xContainsThickFeature(
        const CBedColumnData&) const;

    bool xContainsBlockFeature(
        const CBedColumnData&) const;

    bool xContainsRnaFeature(
        const CBedColumnData&) const;

    bool xContainsCdsFeature(
        const CBedColumnData&) const;

    ENa_strand xGetStrand(
        const CBedColumnData&) const;

    virtual void xAssignBedColumnCount(
        CSeq_annot&);
                    
    void xSetFeatureDisplayData(
        CRef<CSeq_feat>&,
        const CBedColumnData&);

    virtual void xPostProcessAnnot(
        CSeq_annot&);

    bool
    xReadBedDataRaw(
        ILineReader&,
        CRawBedTrack&,
        ILineErrorListener*);

    bool
    xReadBedRecordRaw(
        const string&,
        CRawBedRecord&,
        ILineErrorListener*);

    static void xCleanColumnValues(
        vector<string>&);

    //
    //  data:
    //
protected:
    string m_currentId;
    string mColumnSeparator;
    NStr::TSplitFlags mColumnSplitFlags;
    vector<string>::size_type mRealColumnCount;
    vector<string>::size_type mValidColumnCount;
    bool mAssumeErrorsAreRecordLevel;
    unsigned int m_CurrentFeatureCount;
    bool m_usescore;
    unsigned int m_CurBatchSize;
    const unsigned int m_MaxBatchSize;
    unique_ptr<CLinePreBuffer> mLinePreBuffer;

    unique_ptr<CBedAutoSql> mpAutoSql;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___BEDREADER__HPP
