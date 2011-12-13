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
 *   WIGGLE transient data structures
 *
 */

#ifndef OBJTOOLS_READERS___WIGGLEDATA__HPP
#define OBJTOOLS_READERS___WIGGLEDATA__HPP

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

class CReal_graph;
class CInt_graph;
class CByte_graph;

class CWiggleSet;
class CWiggleTrack;
class CWiggleRecord;
class CWiggleData;

typedef std::map<string,CWiggleTrack*> TrackMap;
typedef std::map<string,CWiggleTrack*>::iterator TrackIter;
typedef std::vector<CWiggleData> DataVector;
typedef DataVector::iterator DataIter;
typedef DataVector::const_iterator DataCiter;

//class CIdMapper;

//  ===========================================================================
class CWiggleData
//  ===========================================================================
{
    friend class CWiggleTrack;
public:
    CWiggleData(
        int,
        unsigned int=0,
        double = 0 );
    CWiggleData(
        const CWiggleRecord& );

    ~CWiggleData() {};

    void Dump(
        CNcbiOstream& );
        
    int SeqStart() const { return m_uSeqStart; };
    int SeqSpan() const { return m_uSeqSpan; };
    double Value() const { return m_dValue; };

    bool operator<(const CWiggleData& d) const {
        return m_uSeqStart < d.m_uSeqStart;
    }
    bool Matches( const CWiggleData& d ) const {
        return (m_uSeqStart == d.m_uSeqStart);
    }
    
protected:
    void FillGraphsReal(
        CSeq_graph& );
        
    void FillGraphsInt(
        CSeq_graph& );
        
    void FillGraphsByte(
        CSeq_graph&,
        const CWiggleTrack&);

    int m_uSeqStart;
    int m_uSeqSpan;
    double m_dValue;
};

//  ===========================================================================
class CWiggleTrack
//  ===========================================================================
{
    enum GraphType {
        GRAPH_UNKNOWN,
        GRAPH_REAL,
        GRAPH_INT,
        GRAPH_BYTE
    };
    
public:
    CWiggleTrack(
        const CWiggleRecord& );
    ~CWiggleTrack();

    void AddRecord(
        const CWiggleRecord& );

    void Dump(
        CNcbiOstream& );
    
    void MakeAsn(
        unsigned int,                               // flags
        const string& strName,                      // track line name
        const string& strTitle,                     // track line descr
        CSeq_annot& );

    void MakeGraph(
        unsigned int,                               // flags
        const string& strName,                      // track line name
        const string& strTitle,                     // track line descr
        CSeq_annot& );
        
    void MakeTable(
        unsigned int,                               // flags
        const string& strName,                      // track line name
        const string& strTitle,                     // track line descr
        CSeq_annot& );
        
    const string& Chrom() const { return m_strChrom; };
    size_t Count() const { return m_Data.size(); };
    int SeqStart() const { return m_uSeqStart; };
    int SeqStop() const { return m_uSeqStop-1; };
    int SeqSpan() const { return m_uSeqSpan; };
    
protected:
    void FillGraphsReal(
        CReal_graph& );
        
    void FillGraphsInt(
        CInt_graph& );
        
    void FillGraphsByte(
        CByte_graph& );
        
    bool DataValue(
        int,
        double& );

    unsigned char ByteGraphValue(
        unsigned int );

    double ScaleConst() const;
    double ScaleLinear() const;              
    unsigned int GetGraphType();

    double MinGraphValue() const { return ( m_dMinValue <= 0 ? m_dMinValue : 0 ); };
    double MaxGraphValue() const { return ( m_dMaxValue >= 0 ? m_dMaxValue : 0 ); };

    std::string m_strName;                       
    std::string m_strChrom;
    unsigned int m_uGraphType;
    int m_uSeqStart;
    int m_uSeqStop;
    int m_uSeqSpan;
    double m_dMaxValue;
    double m_dMinValue;
    bool m_bAllDataIsInteger;
    bool m_bEvenlySpaced;
    DataVector m_Data;
    
    double EstimateSize( size_t, bool, bool ) const;

};

//  ===========================================================================
class CWiggleRecord
//  ===========================================================================
{
public:
    CWiggleRecord();
    ~CWiggleRecord() {};

    void Reset();

    void ParseTrackDefinition( 
        const std::vector<std::string>& );
    /* throws CLineError */
    
    void ParseDeclarationVarstep(
        const std::vector<std::string>& );
    /* throws CLineError */

    void ParseDeclarationFixedstep(
        const std::vector<std::string>& );
    /* throws CLineError */

    void ParseDataBed(
        const std::vector<std::string>& );
    /* throws CLineError */

    void ParseDataVarstep(
        const std::vector<std::string>& );
    /* throws CLineError */

    void ParseDataFixedstep(
        const std::vector<std::string>& );
    /* throws CLineError */

    const string& Chrom() const { return m_strChrom; };
    const string& Name() const { return m_strName; };
    int SeqStart() const { return m_uSeqStart; };
    int SeqSpan() const { return m_uSeqSpan; };
    double Value() const { return m_dValue; };
    
protected:
    std::string m_strName;
    std::string m_strChrom;
    int m_uSeqStart;
    int m_uSeqSpan;
    unsigned int m_uSeqStep;
    double m_dValue;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___WIGGLEDATA__HPP
