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
 *   WIGGLE file reader
 *
 */

#ifndef OBJTOOLS_READERS___WIGGLEREADER__HPP
#define OBJTOOLS_READERS___WIGGLEREADER__HPP

#include <corelib/ncbistd.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include <objtools/readers/reader_base.hpp>
#include <objtools/readers/message_listener.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ============================================================================
struct SFixedStepInfo {
//  ============================================================================
    string mChrom;
    TSeqPos mStart;
    TSeqPos mStep;
    TSeqPos mSpan;

    void Reset() {
        mChrom.clear();
        mStart = mStep = 0;
        mSpan = 1;
    }
    SFixedStepInfo() {
        Reset();
    }
};

//  ============================================================================
struct SVarStepInfo {
//  ============================================================================
    string mChrom;
    TSeqPos mSpan;

    void Reset() {
        mChrom.clear();
        mSpan =1;
    }
    SVarStepInfo() {
        Reset();
    }
};

//  ============================================================================
struct SValueInfo {
//  ============================================================================
    string m_Chrom;
    TSeqPos m_Pos;
    TSeqPos m_Span;
    double m_Value;

    SValueInfo(): m_Pos(0), m_Span(1), m_Value(0.0) {};

    TSeqPos GetEnd(void) const {
        return m_Pos + m_Span;
    }
    bool operator<(const SValueInfo& v) const {
        if (m_Chrom != v.m_Chrom) {
            return m_Chrom < v.m_Chrom;
        }
        return m_Pos < v.m_Pos;
    }
};

//  ============================================================================
struct SWiggleStat {
//  ============================================================================
    bool m_FixedSpan;
    bool m_HaveGaps;
    bool m_IntValues;
    TSeqPos m_Span;
    double m_Min, m_Max, m_Step, m_StepMul;

    SWiggleStat()
        : m_FixedSpan(true),
          m_HaveGaps(false),
          m_IntValues(true),
          m_Span(1),
          m_Min(0),
          m_Max(0),
          m_Step(1),
          m_StepMul(1)
        {
        }
    void SetFirstSpan(TSeqPos span)
        {
            m_FixedSpan = true;
            m_Span = span;
        }
    void AddSpan(TSeqPos span)
        {
            if ( span != m_Span ) {
                m_FixedSpan = false;
            }
        }
    void SetFirstValue(double v)
        {
            m_Min = m_Max = v;
            m_IntValues = v == int(v);
        }
    void AddValue(double v)
        {
            if ( v < m_Min ) {
                m_Min = v;
            }
            if ( v > m_Max ) {
                m_Max = v;
            }
            if ( m_IntValues && v != int(v) ) {
                m_IntValues = false;
            }
        }
    int AsByte(double v) const
        {
            return int((v-m_Min)*m_StepMul+.5);
        }
};

//  ----------------------------------------------------------------------------
class CRawWiggleRecord
//  ----------------------------------------------------------------------------
{
public:
    CRawWiggleRecord(
        CSeq_id& id,
        unsigned int start,
        unsigned int span,
        double value) 
    {
        m_pInterval.Reset(new CSeq_interval());
        m_pInterval->SetId(id);
        m_pInterval->SetFrom(start-1);
        m_pInterval->SetTo(start+span-1);
        m_value = value;
    };
    
    ~CRawWiggleRecord() {};

    void Dump(
        CNcbiOstream& ostr) const
    {
        ostr << "  [CRawWiggleRecord ";
        ostr << "id=\"" << m_pInterval->GetId().AsFastaString() << "\" ";
        ostr << "start=" << m_pInterval->GetFrom() << " ";
        ostr << "stop=" << m_pInterval->GetTo() << " ";
        ostr << "value=" << m_value << "]" << endl;
    }

public:
    CRef<CSeq_interval> m_pInterval;
    double m_value;
};

//  ----------------------------------------------------------------------------
class CRawWiggleTrack
//  ----------------------------------------------------------------------------
{
public:
    CRawWiggleTrack() {};
    ~CRawWiggleTrack() {};

public:
    void Reset()
    {
        m_pId.Reset();
        m_Records.clear();
    }

    void Dump(
        CNcbiOstream& ostr) const
    {
        ostr << "[CRawWiggleTrack" << endl;
        for (vector<CRawWiggleRecord>::const_iterator it = m_Records.begin();
                it != m_Records.end(); ++it) {
            it->Dump(ostr);
        }
        ostr << "]" << std::endl;
    }

    void AddRecord(
        CRawWiggleRecord record)
    {
        m_Records.push_back(record);
    }

    const vector<CRawWiggleRecord>& Records() const
    {
        return m_Records;
    }

    bool HasData() const
    {
        return (!m_Records.empty());
    }

public:
    CRef<CSeq_id> m_pId;
    vector<CRawWiggleRecord> m_Records;
};

//  ----------------------------------------------------------------------------
class NCBI_XOBJREAD_EXPORT CWiggleReader
//  ----------------------------------------------------------------------------
    : public CReaderBase
{
public:
    typedef vector<SValueInfo> TValues;

public:
    CWiggleReader( 
        int = fDefaults,
        const string& = "",
        const string& = "" );
        
    virtual ~CWiggleReader();
    
    //
    //  object interface:
    //
public:
    enum EWiggleFlags {
        fDefaults = 0,
        fJoinSame = 1<<8,
        fAsByte = 1<<9,
        fAsGraph = 1<<10,
        fDumpStats = 1<<11,
        fAsRaw = 1<<12,
    };
    typedef int TFlags;

    virtual CRef< CSeq_annot >
    ReadSeqAnnot(
        ILineReader&,
        IMessageListener* =0 );

    virtual CRef< CSerialObject >
    ReadObject(
        ILineReader&,
        IMessageListener* =0 );
         
    virtual bool 
    ReadTrackData(
        ILineReader&,
        CRawWiggleTrack&,
        IMessageListener* =0 );

    //
    //  helpers:
    //
protected:
    CRef< CSeq_annot >
    xReadSeqAnnotGraph(
        ILineReader&,
        IMessageListener* =0 );

    CRef< CSeq_annot >
    xReadSeqAnnotTable(
        ILineReader&,
        IMessageListener* =0 );

    bool 
    xProcessBrowserLine(
        IMessageListener*);

    bool 
    xParseTrackLine(
        const string&,
        IMessageListener*);

    void
    xGetFixedStepInfo(
        SFixedStepInfo&,
        IMessageListener*);

    void 
    xReadFixedStepData(
        const SFixedStepInfo&,
        ILineReader&,
        IMessageListener*);
    
    bool
    xReadFixedStepDataRaw(
        ILineReader&,
        CRawWiggleTrack&,
        IMessageListener*);

    void
    xGetVarStepInfo(
        SVarStepInfo&,
        IMessageListener*);

    void 
    xReadVariableStepData(
        const SVarStepInfo&,
        ILineReader&,
        IMessageListener*);

    bool
    xReadVariableStepDataRaw(
        ILineReader&,
        CRawWiggleTrack&,
        IMessageListener*);

    void 
    xReadBedLine(
        const string& chrom,
        IMessageListener*);

    CRef<CSeq_annot>
    xGetAnnot();

    bool 
    xCommentLine() const;

    string 
    xGetWord(
        IMessageListener*);

    bool 
    xSkipWS();

    string 
    xGetParamName(
        IMessageListener*);

    string 
    xGetParamValue(
        IMessageListener*);

    void 
    xGetPos(
        TSeqPos& v,
        IMessageListener*);

    bool 
    xTryGetDoubleSimple(
        double& v);

    bool 
    xTryGetDouble(
        double& v,
        IMessageListener*);

    bool 
    xTryGetPos(
        TSeqPos& v,
        IMessageListener*);

    void 
    xGetDouble(
        double& v,
        IMessageListener*);

    CRef<CSeq_id> 
    xMakeChromId();

    CRef<CSeq_table> 
    xMakeTable();

    CRef<CSeq_graph> 
    xMakeGraph();

    CRef<CSeq_annot> 
    xMakeAnnot();

    CRef<CSeq_annot> 
    xMakeTableAnnot();

    CRef<CSeq_annot> 
    xMakeGraphAnnot(
        void);

    void 
    xPreprocessValues(
        SWiggleStat&);

    void 
    xAddValue(const SValueInfo& value) {
        if ( !m_OmitZeros || value.m_Value != 0 ) {
            m_Values.push_back(value);
        }
    }

    double 
    xEstimateSize(
        size_t rows, 
        bool fixed_span) const;

    void 
    xSetTotalLoc(
        CSeq_loc& loc, 
        CSeq_id& chrom_id);

    void 
    xResetChromValues();

    void
    xDumpChromValues();

    void 
    xSetChrom(
        const string& chrom);

    CRef<CSeq_loc> 
    xGetContainingLoc();

    bool
    xValuesAreFromSingleSequence() const;

    //
    //  data:
    //
protected:
    string m_CurLine;
    string m_ChromId;
    TValues m_Values;
    double m_GapValue;
    bool m_SingleAnnot;
    bool m_OmitZeros;

    enum ETrackType {
        eTrackType_invalid,
        eTrackType_wiggle_0,
        eTrackType_bedGraph
    };
    ETrackType m_TrackType;
    CRef<CSeq_annot> m_Annot;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___WIGGLEREADER__HPP
