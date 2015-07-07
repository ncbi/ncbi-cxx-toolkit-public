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
 * Author:  Frank Ludwig, leaning heavily on code lifted from the the wig2table
 *          project, by Aaron Ucko.
 *
 * File Description:
 *   WIGGLE file reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/stream_utils.hpp>

#include <util/line_reader.hpp>

// Objects includes
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seqtable/seqtable__.hpp>

#include <objtools/readers/read_util.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/reader_base.hpp>
#include <objtools/readers/wiggle_reader.hpp>

#include <objects/seqres/Seq_graph.hpp>
#include <objects/seqres/Real_graph.hpp>
#include <objects/seqres/Byte_graph.hpp>

#include "reader_data.hpp"

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
CRef<CSeq_loc> 
CWiggleReader::xGetContainingLoc()
//  ----------------------------------------------------------------------------
{
    if (m_Values.empty()) {
        return CRef<CSeq_loc>();
    }
    CRef<CSeq_loc> pContainingLoc(new CSeq_loc);

    int sortAndMergeFlags = CSeq_loc::fMerge_All;
    if (this->xValuesAreFromSingleSequence()) {
        sortAndMergeFlags = CSeq_loc::fMerge_SingleRange;
    }
    CWiggleReader::TValues::const_iterator cit = m_Values.begin();
    CRef<CSeq_id> pId = CReadUtil::AsSeqId(cit->m_Chrom, m_iFlags);
    CRef<CSeq_interval> pInterval(new CSeq_interval(*pId, cit->m_Pos,
        cit->m_Pos + cit->m_Span));
    pContainingLoc->SetInt(*pInterval);
    int count = 0;
    for (cit++; cit != m_Values.end(); ++cit) {
        ++count;
        CRef<CSeq_id> pId = CReadUtil::AsSeqId(cit->m_Chrom, m_iFlags);
        CRef<CSeq_interval> pInterval(new CSeq_interval(*pId, cit->m_Pos,
            cit->m_Pos + cit->m_Span));
        CRef<CSeq_loc> pAdd(new CSeq_loc);
        pAdd->SetInt(*pInterval);
        pContainingLoc->Assign(
            *pContainingLoc->Add(*pAdd, sortAndMergeFlags, 0));
    }
    return pContainingLoc;
}

//  ----------------------------------------------------------------------------
bool
CWiggleReader::xValuesAreFromSingleSequence() const
//  ----------------------------------------------------------------------------
{
    if (m_Values.empty()) {
        return false;
    }
    TValues::const_iterator cit = m_Values.begin();
    string first = cit->m_Chrom;
    for (cit++; cit != m_Values.end(); ++cit) {
        if (cit->m_Chrom != first) {
            return false;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
CWiggleReader::CWiggleReader(
    int iFlags,
    const string& name,
    const string& title):
//  ----------------------------------------------------------------------------
    CReaderBase(iFlags, name, title),
    m_OmitZeros(false),
    m_TrackType(eTrackType_invalid)
{
    m_uLineNumber = 0;
    m_GapValue = 0.0; 
}

//  ----------------------------------------------------------------------------
CWiggleReader::~CWiggleReader()
//  ----------------------------------------------------------------------------
{
}

//  ----------------------------------------------------------------------------                
CRef< CSerialObject >
CWiggleReader::ReadObject(
    ILineReader& lr,
    ILineErrorListener* pMessageListener ) 
//  ----------------------------------------------------------------------------                
{ 
    CRef<CSerialObject> object( 
        ReadSeqAnnot( lr, pMessageListener ).ReleaseOrNull() );
    return object; 
}

//  ----------------------------------------------------------------------------                
CRef<CSeq_annot>
CWiggleReader::ReadSeqAnnot(
    ILineReader& lr,
    ILineErrorListener* pMessageListener ) 
//  ----------------------------------------------------------------------------                
{
    xProgressInit(lr);
    CRef<CSeq_annot> pAnnot;
    if (m_iFlags & CWiggleReader::fAsGraph) {
        pAnnot = xReadSeqAnnotGraph(lr, pMessageListener);
    }
    else {
        pAnnot = xReadSeqAnnotTable(lr, pMessageListener);
    }
    if (pAnnot) {
        xAssignTrackData(pAnnot);
    }
    return pAnnot;
}

//  ----------------------------------------------------------------------------                
CRef<CSeq_annot>
CWiggleReader::xReadSeqAnnotGraph(
    ILineReader& lr,
    ILineErrorListener* pMessageListener ) 
//  ----------------------------------------------------------------------------                
{
    m_ChromId.clear();
    m_Values.clear();
    if (lr.AtEOF()) {
        return CRef<CSeq_annot>();
    }
    bool haveData = false;
    while (xGetLine(lr, m_CurLine)) {
        if (IsCanceled()) {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Info,
                0,
                "Reader stopped by user.",
                ILineError::eProblem_ProgressInfo));
            ProcessError(*pErr, pMessageListener);
            return CRef<CSeq_annot>();
        }
        xReportProgress(pMessageListener);
        if (xIsTrackLine(m_CurLine)  &&  haveData) {
            xUngetLine(lr);
            break;
        }
        if (xProcessBrowserLine(pMessageListener)) {
            continue;
        }
        if (xParseTrackLine(m_CurLine, pMessageListener)) {
            continue;
        }

        string s = xGetWord(pMessageListener);
        if ( s == "fixedStep" ) {
            SFixedStepInfo fixedStepInfo;
            xGetFixedStepInfo(fixedStepInfo, pMessageListener);
            if (!m_ChromId.empty() && fixedStepInfo.mChrom != m_ChromId) {
                lr.UngetLine();
                return xGetAnnot();
            }
            xReadFixedStepData(fixedStepInfo, lr, pMessageListener);
            haveData = true;
        }
        else if ( s == "variableStep" ) {
            SVarStepInfo varStepInfo;
            xGetVarStepInfo(varStepInfo, pMessageListener);
            if (!m_ChromId.empty() && varStepInfo.mChrom != m_ChromId) {
                lr.UngetLine();
                return xGetAnnot();
            }
            xReadVariableStepData(varStepInfo, lr, pMessageListener);
            haveData = true;
        }
        else {
            xReadBedLine(s, pMessageListener);
            haveData = true;
        }
    }
    xSetChrom("");
    return m_Annot;
}
    
//  ----------------------------------------------------------------------------                
CRef<CSeq_annot>
CWiggleReader::xReadSeqAnnotTable(
    ILineReader& lr,
    ILineErrorListener* pMessageListener ) 
//  ----------------------------------------------------------------------------                
{
    m_ChromId.clear();
    m_Values.clear();
    if (lr.AtEOF()) {
        return CRef<CSeq_annot>();
    }
    xResetChromValues();

    bool haveData = false;
    while (xGetLine(lr, m_CurLine)) {
        if (IsCanceled()) {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Info,
                0,
                "Reader stopped by user.",
                ILineError::eProblem_ProgressInfo));
            ProcessError(*pErr, pMessageListener);
            return CRef<CSeq_annot>();
        }
        xReportProgress(pMessageListener);
        if (xIsTrackLine(m_CurLine)  &&  haveData) {
            xUngetLine(lr);
            break;
        }
        if (xProcessBrowserLine(pMessageListener)) {
            continue;
        }
        if (xParseTrackLine(m_CurLine, pMessageListener)) {
            continue;
        }

        string s = xGetWord(pMessageListener);
        if ( s == "fixedStep" ) {
            SFixedStepInfo fixedStepInfo;
            xGetFixedStepInfo(fixedStepInfo, pMessageListener);
            if (!m_ChromId.empty() && fixedStepInfo.mChrom != m_ChromId) {
                lr.UngetLine();
                return xGetAnnot();
            }
            xReadFixedStepData(fixedStepInfo, lr, pMessageListener);
            haveData = true;
        }
        else if ( s == "variableStep" ) {
            SVarStepInfo varStepInfo;
            xGetVarStepInfo(varStepInfo, pMessageListener);
            if (!m_ChromId.empty() && varStepInfo.mChrom != m_ChromId) {
                lr.UngetLine();
                return xGetAnnot();
            }
            xReadVariableStepData(varStepInfo, lr, pMessageListener);
            haveData = true;
        }
        else {
            xReadBedLine(s, pMessageListener);
            haveData = true;
        }
    }
    return xGetAnnot();
}

//  ----------------------------------------------------------------------------
bool 
CWiggleReader::ReadTrackData(
    ILineReader& lr,
    CRawWiggleTrack& rawdata,
    ILineErrorListener* pMessageListener)
//  ----------------------------------------------------------------------------
{
    while (xGetLine(lr, m_CurLine)) {
        string word = xGetWord(pMessageListener);
        if (word == "browser") {
            continue;
        }
        if (word == "track") {
            continue;
        }
        if (word == "fixedStep") {
            return xReadFixedStepDataRaw(lr, rawdata, pMessageListener);
        }
        if (word == "variableStep") {
            return xReadVariableStepDataRaw(lr, rawdata, pMessageListener);
        }
        //data line
        continue;
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool
CWiggleReader::xReadFixedStepDataRaw(
    ILineReader& lr,
    CRawWiggleTrack& rawdata,
    ILineErrorListener* pMessageListener)
//  ----------------------------------------------------------------------------
{
    rawdata.Reset();

    SFixedStepInfo fixedStepInfo;
    xGetFixedStepInfo(fixedStepInfo, pMessageListener);
    CRef<CSeq_id> id =
        CReadUtil::AsSeqId(fixedStepInfo.mChrom, m_iFlags);

    unsigned int pos(fixedStepInfo.mStart);
    while (xGetLine(lr, m_CurLine)) {
        double value(0);
        if (!xTryGetDouble(value, pMessageListener)) {
            lr.UngetLine();
            break;
        }
        rawdata.AddRecord(
            CRawWiggleRecord(*id, pos, fixedStepInfo.mSpan, value));
        pos += fixedStepInfo.mStep;
    }
    return rawdata.HasData();
}

//  ----------------------------------------------------------------------------
bool
CWiggleReader::xReadVariableStepDataRaw(
    ILineReader& lr,
    CRawWiggleTrack& rawdata,
    ILineErrorListener* pMessageListener)
//  ----------------------------------------------------------------------------
{
    rawdata.Reset();

    SVarStepInfo varStepInfo;
    xGetVarStepInfo(varStepInfo, pMessageListener);
    CRef<CSeq_id> id =
        CReadUtil::AsSeqId(varStepInfo.mChrom, m_iFlags);
    while (xGetLine(lr, m_CurLine)) {
        unsigned int pos(0);
        if (!xTryGetPos(pos, pMessageListener)) {
            lr.UngetLine();
            break;
        }
        xSkipWS();
        double value(0);
        xGetDouble(value, pMessageListener);
        rawdata.AddRecord(
            CRawWiggleRecord(*id, pos, varStepInfo.mSpan, value));
    }
    return rawdata.HasData();
}

//  ----------------------------------------------------------------------------
double CWiggleReader::xEstimateSize(size_t rows, bool fixed_span) const
//  ----------------------------------------------------------------------------
{
    double ret = 0;
    ret += rows*4;
    if ( !fixed_span )
        ret += rows*4;
    if (m_iFlags & fAsByte)
        ret += rows;
    else
        ret += 8*rows;
    return ret;
}

//  ----------------------------------------------------------------------------
void CWiggleReader::xPreprocessValues(SWiggleStat& stat)
//  ----------------------------------------------------------------------------
{
    bool sorted = true;
    size_t size = m_Values.size();
    if ( size ) {
        stat.SetFirstSpan(m_Values[0].m_Span);
        stat.SetFirstValue(m_Values[0].m_Value);

        for ( size_t i = 1; i < size; ++i ) {
            stat.AddSpan(m_Values[i].m_Span);
            stat.AddValue(m_Values[i].m_Value);
            if ( sorted ) {
                if ( m_Values[i].m_Pos < m_Values[i-1].m_Pos ) {
                    sorted = false;
                }
                if ( m_Values[i].m_Pos != m_Values[i-1].GetEnd() ) {
                    stat.m_HaveGaps = true;
                }
            }
        }
    }
    if ( !sorted ) {
        sort(m_Values.begin(), m_Values.end());
        stat.m_HaveGaps = false;
        for ( size_t i = 1; i < size; ++i ) {
            if ( m_Values[i].m_Pos != m_Values[i-1].GetEnd() ) {
                stat.m_HaveGaps = true;
                break;
            }
        }
    }
    if ( (m_iFlags & fAsGraph) && stat.m_HaveGaps ) {
        stat.AddValue(m_GapValue);
    }

    const int range = 255;
    if ( stat.m_Max > stat.m_Min && 
            (!stat.m_IntValues || stat.m_Max-stat.m_Min > range) ) {
        stat.m_Step = (stat.m_Max-stat.m_Min)/range;
        stat.m_StepMul = 1/stat.m_Step;
    }

    if ( !(m_iFlags & fAsGraph) && (m_iFlags & fJoinSame) && size ) {
        TValues nv;
        nv.reserve(size);
        nv.push_back(m_Values[0]);
        for ( size_t i = 1; i < size; ++i ) {
            if ( m_Values[i].m_Pos == nv.back().GetEnd() &&
                 m_Values[i].m_Value == nv.back().m_Value ) {
                nv.back().m_Span += m_Values[i].m_Span;
            }
            else {
                nv.push_back(m_Values[i]);
            }
        }
        if ( nv.size() != size ) {
            double s = xEstimateSize(size, stat.m_FixedSpan);
            double ns = xEstimateSize(nv.size(), false);
            if ( ns < s*.75 ) {
                m_Values.swap(nv);
                size = m_Values.size();
                LOG_POST("Joined size: "<<size);
                stat.m_FixedSpan = false;
            }
        }
    }

    if ( (m_iFlags & fAsGraph) && !stat.m_FixedSpan ) {
        stat.m_Span = 1;
        stat.m_FixedSpan = true;
    }
}

//  ----------------------------------------------------------------------------
CRef<CSeq_id> CWiggleReader::xMakeChromId()
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_id> id =
        CReadUtil::AsSeqId(m_ChromId, m_iFlags);
    return id;
}

//  ----------------------------------------------------------------------------
void CWiggleReader::xSetTotalLoc(CSeq_loc& loc, CSeq_id& chrom_id)
//  ----------------------------------------------------------------------------
{
    if ( m_Values.empty() ) {
        loc.SetEmpty(chrom_id);
    }
    else {
        CSeq_interval& interval = loc.SetInt();
        interval.SetId(chrom_id);
        interval.SetFrom(m_Values.front().m_Pos);
        interval.SetTo(m_Values.back().GetEnd()-1);
    }
}

//  ----------------------------------------------------------------------------
CRef<CSeq_table> CWiggleReader::xMakeTable(void)
//  ----------------------------------------------------------------------------
{
    size_t size = m_Values.size();

    CRef<CSeq_table> table(new CSeq_table);
    table->SetFeat_type(0);

    CRef<CSeq_id> chrom_id = xMakeChromId();

    CRef<CSeq_loc> table_loc(new CSeq_loc);
    { // Seq-table location
        CRef<CSeqTable_column> col_id(new CSeqTable_column);
        table->SetColumns().push_back(col_id);
        col_id->SetHeader().SetField_name("Seq-table location");
        //col_id->SetDefault().SetLoc(*xGetContainingLoc());
        col_id->SetDefault().SetLoc(*table_loc);
    }

    { // Seq-id
        CRef<CSeqTable_column> col_id(new CSeqTable_column);
        table->SetColumns().push_back(col_id);
        col_id->SetHeader().SetField_id(CSeqTable_column_info::eField_id_location_id);
        //col_id->SetDefault().SetId(*chrom_id);
        if (this->xValuesAreFromSingleSequence()) {
            CRef<CSeq_id> pId = CReadUtil::AsSeqId(m_Values.front().m_Chrom, m_iFlags);
            col_id->SetDefault().SetId(*pId);
        }
        else {
            CSeqTable_multi_data::TId& seq_id = col_id->SetData().SetId();
            seq_id.reserve(size);
            for (TValues::const_iterator cit = m_Values.begin(); cit != m_Values.end(); ++cit) {
                CRef<CSeq_id> pId = CReadUtil::AsSeqId(cit->m_Chrom, m_iFlags);
                seq_id.push_back(pId);
            }
        }
    }
    
    // position
    CRef<CSeqTable_column> col_pos(new CSeqTable_column);
    table->SetColumns().push_back(col_pos);
    col_pos->SetHeader().SetField_id(CSeqTable_column_info::eField_id_location_from);
    CSeqTable_multi_data::TInt& pos = col_pos->SetData().SetInt();

    SWiggleStat stat;
    xPreprocessValues(stat);
    
    xSetTotalLoc(*table_loc, *chrom_id);

    table->SetNum_rows(size);
    pos.reserve(size);

    CSeqTable_multi_data::TInt* span_ptr = 0;
    { // span
        CRef<CSeqTable_column> col_span(new CSeqTable_column);
        table->SetColumns().push_back(col_span);
        col_span->SetHeader().SetField_name("span");
        if ( stat.m_FixedSpan ) {
            col_span->SetDefault().SetInt(stat.m_Span);
        }
        else {
            span_ptr = &col_span->SetData().SetInt();
            span_ptr->reserve(size);
        }
    }

    if ( stat.m_HaveGaps ) {
        CRef<CSeqTable_column> col_step(new CSeqTable_column);
        table->SetColumns().push_back(col_step);
        col_step->SetHeader().SetField_name("value_gap");
        col_step->SetDefault().SetReal(m_GapValue);
    }

    if (m_iFlags & fAsByte) { // values
        CRef<CSeqTable_column> col_min(new CSeqTable_column);
        table->SetColumns().push_back(col_min);
        col_min->SetHeader().SetField_name("value_min");
        col_min->SetDefault().SetReal(stat.m_Min);

        CRef<CSeqTable_column> col_step(new CSeqTable_column);
        table->SetColumns().push_back(col_step);
        col_step->SetHeader().SetField_name("value_step");
        col_step->SetDefault().SetReal(stat.m_Step);

        CRef<CSeqTable_column> col_val(new CSeqTable_column);
        table->SetColumns().push_back(col_val);
        col_val->SetHeader().SetField_name("values");
        
        if ( 1 ) {
            AutoPtr< vector<char> > values(new vector<char>());
            values->reserve(size);
            ITERATE ( TValues, it, m_Values ) {
                pos.push_back(it->m_Pos);
                if ( span_ptr ) {
                    span_ptr->push_back(it->m_Span);
                }
                values->push_back(stat.AsByte(it->m_Value));
            }
            col_val->SetData().SetBytes().push_back(values.release());
        }
        else {
            CSeqTable_multi_data::TInt& values = col_val->SetData().SetInt();
            values.reserve(size);
            
            ITERATE ( TValues, it, m_Values ) {
                pos.push_back(it->m_Pos);
                if ( span_ptr ) {
                    span_ptr->push_back(it->m_Span);
                }
                values.push_back(stat.AsByte(it->m_Value));
            }
        }
    }
    else {
        CRef<CSeqTable_column> col_val(new CSeqTable_column);
        table->SetColumns().push_back(col_val);
        col_val->SetHeader().SetField_name("values");
        CSeqTable_multi_data::TReal& values = col_val->SetData().SetReal();
        values.reserve(size);
        
        ITERATE ( TValues, it, m_Values ) {
            pos.push_back(it->m_Pos);
            if ( span_ptr ) {
                span_ptr->push_back(it->m_Span);
            }
            values.push_back(it->m_Value);
        }
    }
    return table;
}

//  ----------------------------------------------------------------------------
CRef<CSeq_graph> CWiggleReader::xMakeGraph(void)
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_graph> graph(new CSeq_graph);

    CRef<CSeq_id> chrom_id = xMakeChromId();

    CRef<CSeq_loc> graph_loc(new CSeq_loc);
    graph->SetLoc(*graph_loc);

    SWiggleStat stat;
    xPreprocessValues(stat);
    
    xSetTotalLoc(*graph_loc, *chrom_id);

    string trackName = m_pTrackDefaults->Name();
    if (!trackName.empty()) {
        graph->SetTitle(trackName);
    }
       
    graph->SetComp(stat.m_Span);
    graph->SetA(stat.m_Step);
    graph->SetB(stat.m_Min);

    CByte_graph& b_graph = graph->SetGraph().SetByte();
    b_graph.SetMin(stat.AsByte(stat.m_Min));
    b_graph.SetMax(stat.AsByte(stat.m_Max));
    b_graph.SetAxis(0);
    vector<char>& bytes = b_graph.SetValues();

    if ( m_Values.empty() ) {
        graph->SetNumval(0);
    }
    else {
        _ASSERT(stat.m_FixedSpan);
        TSeqPos start = m_Values[0].m_Pos;
        TSeqPos end = m_Values.back().GetEnd();
        size_t size = (end-start)/stat.m_Span;
        graph->SetNumval(size);
        bytes.resize(size, stat.AsByte(m_GapValue));
        ITERATE ( TValues, it, m_Values ) {
            TSeqPos pos = it->m_Pos - start;
            TSeqPos span = it->m_Span;
            _ASSERT(pos % stat.m_Span == 0);
            _ASSERT(span % stat.m_Span == 0);
            size_t i = pos / stat.m_Span;
            int v = stat.AsByte(it->m_Value);
            for ( ; span > 0; span -= stat.m_Span, ++i ) {
                bytes[i] = v;
            }
        }
    }
    return graph;
}

//  ----------------------------------------------------------------------------
CRef<CSeq_annot> CWiggleReader::xMakeAnnot(void)
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_annot> annot(new CSeq_annot);
    return annot;
}

//  ----------------------------------------------------------------------------
CRef<CSeq_annot> CWiggleReader::xMakeTableAnnot(void)
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_annot> annot = xMakeAnnot();
    annot->SetData().SetSeq_table(*xMakeTable());
    return annot;
}

//  ----------------------------------------------------------------------------
CRef<CSeq_annot> CWiggleReader::xMakeGraphAnnot(void)
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_annot> annot = xMakeAnnot();
    annot->SetData().SetGraph().push_back(xMakeGraph());
    return annot;
}

//  ----------------------------------------------------------------------------
void CWiggleReader::xResetChromValues(void)
//  ----------------------------------------------------------------------------
{
    m_ChromId.clear();
    m_Values.clear();
}

//  ----------------------------------------------------------------------------
bool CWiggleReader::xSkipWS(void)
//  ----------------------------------------------------------------------------
{
    const char* ptr = m_CurLine.c_str();
    size_t skip = 0;
    for ( size_t len = m_CurLine.size(); skip < len; ++skip ) {
        char c = ptr[skip];
        if ( c != ' ' && c != '\t' ) {
            break;
        }
    }
    m_CurLine = m_CurLine.substr(skip);
    return !m_CurLine.empty();
}

//  ----------------------------------------------------------------------------
inline bool CWiggleReader::xCommentLine(void) const
//  ----------------------------------------------------------------------------
{
    char c = m_CurLine.c_str()[0];
    return c == '#' || c == '\0';
}

//  ----------------------------------------------------------------------------
string CWiggleReader::xGetWord(
    ILineErrorListener* pMessageListener)
//  ----------------------------------------------------------------------------
{
    const char* ptr = m_CurLine.c_str();
    size_t skip = 0;
    for ( size_t len = m_CurLine.size(); skip < len; ++skip ) {
        char c = ptr[skip];
        if ( c == ' ' || c == '\t' ) {
            break;
        }
    }
    if ( skip == 0 ) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Warning,
            0,
            "Identifier expected") );
        ProcessError(*pErr, pMessageListener);
    }
    string word(ptr, skip);
    m_CurLine = m_CurLine.substr(skip);
    return word;
}

//  ----------------------------------------------------------------------------
string CWiggleReader::xGetParamName(
    ILineErrorListener* pMessageListener)
//  ----------------------------------------------------------------------------
{
    const char* ptr = m_CurLine.c_str();
    size_t skip = 0;
    for ( size_t len = m_CurLine.size(); skip < len; ++skip ) {
        char c = ptr[skip];
        if ( c == '=' ) {
            string name(ptr, skip);
            m_CurLine = m_CurLine.substr(skip+1);
            return name;
        }
        if ( c == ' ' || c == '\t' ) {
            break;
        }
    }
    AutoPtr<CObjReaderLineException> pErr(
        CObjReaderLineException::Create(
        eDiag_Warning,
        0,
        "\"=\" expected") );
    ProcessWarning(*pErr, pMessageListener);
    return string();
}

//  ----------------------------------------------------------------------------
string CWiggleReader::xGetParamValue(
    ILineErrorListener* pMessageListener)
//  ----------------------------------------------------------------------------
{
    const char* ptr = m_CurLine.c_str();
    size_t len = m_CurLine.size();
    if ( len && *ptr == '"' ) {
        size_t pos = 1;
        for ( ; pos < len; ++pos ) {
            char c = ptr[pos];
            if ( c == '"' ) {
                string value(ptr, pos);
                m_CurLine = m_CurLine.substr(pos+1);
                return value;
            }
        }
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Warning,
            0,
            "Open quotes") );
        ProcessError(*pErr, pMessageListener);
    }
    return xGetWord(pMessageListener);
}

//  ----------------------------------------------------------------------------
void CWiggleReader::xGetPos(
    TSeqPos& v,
    ILineErrorListener* pMessageListener)
//  ----------------------------------------------------------------------------
{
    TSeqPos ret = 0;
    const char* ptr = m_CurLine.c_str();
    for ( size_t skip = 0; ; ++skip ) {
        char c = ptr[skip];
        if ( c >= '0' && c <= '9' ) {
            ret = ret*10 + (c-'0');
        }
        else if ( (c == ' ' || c == '\t' || c == '\0') && skip ) {
            m_CurLine = m_CurLine.substr(skip);
            v = ret;
            return;
        }
        else {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
            eDiag_Error,
            0,
            "Integer value expected") );
        ProcessError(*pErr, pMessageListener);
        }
    }
}

//  ----------------------------------------------------------------------------
bool CWiggleReader::xTryGetDoubleSimple(double& v)
//  ----------------------------------------------------------------------------
{
    double ret = 0;
    const char* ptr = m_CurLine.c_str();
    size_t skip = 0;
    bool negate = false, digits = false;
    for ( ; ; ++skip ) {
        char c = ptr[skip];
        if ( !skip ) {
            if ( c == '-' ) {
                negate = true;
                continue;
            }
            if ( c == '+' ) {
                continue;
            }
        }
        if ( c >= '0' && c <= '9' ) {
            digits = true;
            ret = ret*10 + (c-'0');
        }
        else if ( c == '.' ) {
            ++skip;
            break;
        }
        else if ( c == '\0' ) {
            if ( !digits ) {
                return false;
            }
            m_CurLine.clear();
            if ( negate ) {
                ret = -ret;
            }
            v = ret;
            return true;
        }
        else {
            return false;
        }
    }
    double digit_mul = 1;
    for ( ; ; ++skip ) {
        char c = ptr[skip];
        if ( c >= '0' && c <= '9' ) {
            digits = true;
            digit_mul *= .1;
            ret += (c-'0')*digit_mul;
        }
        else if ( (c == ' ' || c == '\t' || c == '\0') && digits ) {
            m_CurLine.clear();
            v = (negate ? -ret : ret);
            return true;
        }
        else {
            return false;
        }
    }
}

//  ----------------------------------------------------------------------------
bool CWiggleReader::xTryGetDouble(
    double& v,
    ILineErrorListener* pMessageListener)
//  ----------------------------------------------------------------------------
{
    if ( xTryGetDoubleSimple(v) ) {
        return true;
    }
    const char* ptr = m_CurLine.c_str();
    char* endptr = 0;
    v = strtod(ptr, &endptr);
    if ( endptr == ptr ) {
        return false;
    }
    if ( *endptr ) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Warning,
            0,
            "Extra text on line") );
        ProcessError(*pErr, pMessageListener);
    }
    m_CurLine.clear();
    return true;
}

//  ----------------------------------------------------------------------------
inline bool CWiggleReader::xTryGetPos(
    TSeqPos& v,
    ILineErrorListener* pMessageListener)
//  ----------------------------------------------------------------------------
{
    char c = m_CurLine.c_str()[0];
    if ( c < '0' || c > '9' ) {
        return false;
    }
    xGetPos(v, pMessageListener);
    return true;
}

//  ----------------------------------------------------------------------------
inline void CWiggleReader::xGetDouble(
    double& v,
    ILineErrorListener* pMessageListener)
//  ----------------------------------------------------------------------------
{
    if ( !xTryGetDouble(v, pMessageListener) ) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            0,
            "Floating point value expected") );
        ProcessError(*pErr, pMessageListener);
    }
}

//  ----------------------------------------------------------------------------
CRef<CSeq_annot> CWiggleReader::xGetAnnot()
//  ----------------------------------------------------------------------------
{
    if ( m_ChromId.empty() ) {
        return CRef<CSeq_annot>();
    }
    CRef<CSeq_annot> pAnnot(new CSeq_annot);
    if (m_iFlags & fAsGraph) {
        //pAnnot->SetData().SetGraph().push_back(xMakeGraph());
        pAnnot = xMakeGraphAnnot();
    }
    else {
        pAnnot->SetData().SetSeq_table(*xMakeTable());
    }
    m_ChromId.clear();
    return pAnnot;
}

//  ----------------------------------------------------------------------------
void CWiggleReader::xDumpChromValues(void)
//  ----------------------------------------------------------------------------
{
    if ( m_ChromId.empty() ) {
        return;
    }
    //LOG_POST("Chrom: "<<m_ChromId<<" "<<m_Values.size());
    if ( !m_Annot ) {
        m_Annot = xMakeAnnot();
    }
    if (m_iFlags & fAsGraph) {
        m_Annot->SetData().SetGraph().push_back(xMakeGraph());
    }
    else {
        m_Annot->SetData().SetSeq_table(*xMakeTable());
    }
    if ( !m_SingleAnnot ) {
//        xDumpAnnot();
    }
    //xResetChromValues();
}

//  ----------------------------------------------------------------------------
void CWiggleReader::xSetChrom(
    const string& chrom)
//  ----------------------------------------------------------------------------
{
    if ( chrom != m_ChromId ) {
        xDumpChromValues();
        if (m_iFlags & CWiggleReader::fAsGraph) {
            m_Values.clear();
        }
        m_ChromId = chrom;
    }
}

//  ----------------------------------------------------------------------------
bool CWiggleReader::xProcessBrowserLine(
    ILineErrorListener* pMl)
//  ----------------------------------------------------------------------------
{
    if (!NStr::StartsWith(m_CurLine, "browser")) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CWiggleReader::xParseTrackLine(
    const string& line,
    ILineErrorListener* pMessageListener)
//  ----------------------------------------------------------------------------
{
    if (!xIsTrackLine(line)) {
        return false;
    }
    CReaderBase::xParseTrackLine(line, pMessageListener);

    m_TrackType = eTrackType_invalid;
    if (m_pTrackDefaults->Type() == "wiggle_0") {
        m_TrackType = eTrackType_wiggle_0;
    }
    else if (m_pTrackDefaults->Type() == "bedGraph") {
        m_TrackType = eTrackType_bedGraph;
    }
    else {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Warning,
            0,
            "Invalid track type") );
        ProcessError(*pErr, pMessageListener);
    }
    return true;
}

//  ----------------------------------------------------------------------------
void CWiggleReader::xGetFixedStepInfo(
    SFixedStepInfo& fixedStepInfo,
    ILineErrorListener* pMessageListener)
//  ----------------------------------------------------------------------------
{
    if ( m_TrackType != eTrackType_wiggle_0 ) {
        if ( m_TrackType != eTrackType_invalid ) {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Warning,
                0,
                "Track \"type=wiggle_0\" is required") );
            ProcessError(*pErr, pMessageListener);
        }
        else {
            m_TrackType = eTrackType_wiggle_0;
        }
    }

    fixedStepInfo.Reset();
    while ( xSkipWS() ) {
        string name = xGetParamName(pMessageListener);
        string value = xGetParamValue(pMessageListener);
        if ( name == "chrom" ) {
            fixedStepInfo.mChrom = value;
        }
        else if ( name == "start" ) {
            fixedStepInfo.mStart = NStr::StringToUInt(value);
        }
        else if ( name == "step" ) {
            fixedStepInfo.mStep = NStr::StringToUInt(value);
        }
        else if ( name == "span" ) {
            fixedStepInfo.mSpan = NStr::StringToUInt(value);
        }
        else {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Warning,
                0,
                "Bad parameter name") );
            ProcessError(*pErr, pMessageListener);
        }
    }
    if ( fixedStepInfo.mChrom.empty() ) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            0,
            "Missing chrom parameter") );
        ProcessError(*pErr, pMessageListener);
    }
    if ( fixedStepInfo.mStart == 0 ) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            0,
            "Missing start value") );
        ProcessError(*pErr, pMessageListener);
    }
    if ( fixedStepInfo.mStep == 0 ) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            0,
            "Missing step value") );
        ProcessError(*pErr, pMessageListener);
    }
}

//  ----------------------------------------------------------------------------
void CWiggleReader::xReadFixedStepData(
    const SFixedStepInfo& fixedStepInfo,
    ILineReader& lr,
    ILineErrorListener* pMessageListener)
//  ----------------------------------------------------------------------------
{
    xSetChrom(fixedStepInfo.mChrom);
    SValueInfo value;
    value.m_Chrom = fixedStepInfo.mChrom;
    value.m_Pos = fixedStepInfo.mStart-1;
    value.m_Span = fixedStepInfo.mSpan;
    while ( xGetLine(lr, m_CurLine) ) {
        if ( !xTryGetDouble(value.m_Value, pMessageListener) ) {
            lr.UngetLine();
            break;
        }
        xAddValue(value);
        value.m_Pos += fixedStepInfo.mStep;
    }
}

//  ----------------------------------------------------------------------------
void CWiggleReader::xGetVarStepInfo(
    SVarStepInfo& varStepInfo,
    ILineErrorListener* pMessageListener)
//  ----------------------------------------------------------------------------
{
    if ( m_TrackType != eTrackType_wiggle_0 ) {
        if ( m_TrackType != eTrackType_invalid ) {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Warning,
                0,
                "Track \"type=wiggle_0\" is required") );
            ProcessError(*pErr, pMessageListener);
        }
        else {
            m_TrackType = eTrackType_wiggle_0;
        }
    }

    varStepInfo.Reset();
    while ( xSkipWS() ) {
        string name = xGetParamName(pMessageListener);
        string value = xGetParamValue(pMessageListener);
        if ( name == "chrom" ) {
            varStepInfo.mChrom = value;
        }
        else if ( name == "span" ) {
            varStepInfo.mSpan = NStr::StringToUInt(value);
        }
        else {
            AutoPtr<CObjReaderLineException> pErr(
                CObjReaderLineException::Create(
                eDiag_Warning,
                0,
                "Bad parameter name") );
            ProcessError(*pErr, pMessageListener);
        }
    }
    if ( varStepInfo.mChrom.empty() ) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Error,
            0,
            "Missing chrom parameter") );
        ProcessError(*pErr, pMessageListener);
    }
}

//  =========================================================================
void CWiggleReader::xReadVariableStepData(
    const SVarStepInfo& varStepInfo,
    ILineReader& lr,
    ILineErrorListener* pMessageListener)
//  =========================================================================
{
    xSetChrom(varStepInfo.mChrom);
    SValueInfo value;
    value.m_Chrom = varStepInfo.mChrom;
    value.m_Span = varStepInfo.mSpan;
    while ( xGetLine(lr, m_CurLine) ) {
        if ( !xTryGetPos(value.m_Pos, pMessageListener) ) {
            lr.UngetLine();
            break;
        }
        xSkipWS();
        xGetDouble(value.m_Value, pMessageListener);
        value.m_Pos -= 1;
        xAddValue(value);
    }
}

//  =========================================================================
void CWiggleReader::xReadBedLine(
    const string& chrom,
    ILineErrorListener* pMessageListener)
//  =========================================================================
{
    if ( m_TrackType != eTrackType_bedGraph &&
        m_TrackType != eTrackType_invalid ) {
        AutoPtr<CObjReaderLineException> pErr(
            CObjReaderLineException::Create(
            eDiag_Warning,
            0,
            "Track \"type=bedGraph\" is required") );
        ProcessError(*pErr, pMessageListener);
    }
    xSetChrom(chrom);
    SValueInfo value;
    xSkipWS();
    xGetPos(value.m_Pos, pMessageListener);
    xSkipWS();
    xGetPos(value.m_Span, pMessageListener);
    xSkipWS();
    xGetDouble(value.m_Value, pMessageListener);
    value.m_Span -= value.m_Pos;
    xAddValue(value);
}

END_objects_SCOPE
END_NCBI_SCOPE
