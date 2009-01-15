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
typedef std::map<int,CWiggleData*> DataStore;
typedef std::map<int,CWiggleData*>::iterator DataIter;
typedef std::map<int,CWiggleData*>::const_iterator DataCiter;

class CIdMapper;

//  ===========================================================================
class CWiggleData
//  ===========================================================================
{
public:
    CWiggleData(
        unsigned int,
        double );
    ~CWiggleData() {};

    void Dump(
        CNcbiOstream& );
        
    unsigned int SeqStart() const { return m_uSeqStart; };
    double Value() const { return m_dValue; };
    
protected:
    unsigned int m_uSeqStart;
    double m_dValue;
};

// ===========================================================================
class CWiggleSet
//  ===========================================================================
{
public:
    CWiggleSet(
        CIdMapper* );
    ~CWiggleSet() {};

    bool AddRecord(
        const CWiggleRecord& );

    bool MakeGraph(
        CSeq_annot::TData::TGraph& );

    void Dump(
        CNcbiOstream& );

    size_t Count() const { return m_Tracks.size(); };
                    
protected:
    bool FindTrack(
        const std::string&,
        CWiggleTrack*& );

    TrackMap m_Tracks;
    CIdMapper* m_pMapper;
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
        const CWiggleRecord&,
        CIdMapper* pMapper );
    ~CWiggleTrack();

    bool AddRecord(
        const CWiggleRecord& );

    void Dump(
        CNcbiOstream& );
    
    bool MakeGraph(
        CSeq_annot::TData::TGraph& );
        
    const char* Chrom() const { return m_strChrom.c_str(); };
    size_t Count() const { return m_Entries.size(); };
    unsigned int SeqStart() const;
    unsigned int SeqStop() const;
    unsigned int SeqSpan() const { return m_uSeqSpan; };
    
protected:
    bool FillGraphReal(
        CReal_graph& );
        
    bool FillGraphInt(
        CInt_graph& );
        
    bool FillGraphByte(
        CByte_graph& );
        
    double DataValue(
        unsigned int );

    unsigned char ByteGraphValue(
        unsigned int );

    double ScaleConst() const;
    double ScaleLinear() const;              
    unsigned int GetGraphType();

    CSeq_id_Handle m_MappedID;                       
    std::string m_strChrom;
    unsigned int m_uGraphType;
    unsigned int m_uSeqStart;
    unsigned int m_uSeqStop;
    unsigned int m_uSeqLength;
    unsigned int m_uSeqSpan;
    double m_dMaxValue;
    double m_dMinValue;
    DataStore m_Entries;
};

//  ===========================================================================
class CWiggleRecord
//  ===========================================================================
{
public:
    enum IDMODE {
        IDMODE_NAME_CHROM,
        IDMODE_CHROM
    };
    
public:
    CWiggleRecord();
    ~CWiggleRecord() {};

    void Reset();

    bool ParseTrackDefinition(
        const std::vector<std::string>&,
        unsigned int );
    
    bool ParseDeclarationVarstep(
        const std::vector<std::string>&,
        unsigned int );

    bool ParseDeclarationFixedstep(
        const std::vector<std::string>&,
        unsigned int );

    bool ParseDataBed(
        const std::vector<std::string>&,
        unsigned int );

    bool ParseDataVarstep(
        const std::vector<std::string>&,
        unsigned int );

    bool ParseDataFixedstep(
        const std::vector<std::string>&,
        unsigned int );

    const char* Chrom() const { return m_strChrom.c_str(); };
    const char* Id() const { return m_strId.c_str(); };
    unsigned int SeqStart() const { return m_uSeqStart; };
    unsigned int SeqSpan() const { return m_uSeqSpan; };
    double Value() const { return m_dValue; };
    
protected:
    std::string m_strId;
    std::string m_strChrom;
    unsigned int m_uSeqStart;
    unsigned int m_uSeqSpan;
    unsigned int m_uSeqStep;
    double m_dValue;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___WIGGLEDATA__HPP
