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

#include <util/line_reader.hpp>

#include <objects/seq/Annotdesc.hpp>
#include <objects/seqtable/Seq_table.hpp>
#include <objects/seqtable/SeqTable_column.hpp>
#include <objects/seqres/Seq_graph.hpp>
#include <objects/seqres/Byte_graph.hpp>

#include <objtools/readers/wiggle_reader.hpp>
#include "reader_message_handler.hpp"

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

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
    const string& title,
    CReaderListener* pRl):
//  ----------------------------------------------------------------------------
    CReaderBase(iFlags, name, title, CReadUtil::AsSeqId, pRl),
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
CRef< CSeq_annot >
CWiggleReader::ReadSeqAnnot(
    ILineReader& lr,
    ILineErrorListener* pEL)
//  ----------------------------------------------------------------------------
{
    m_ChromId.clear();
    m_Values.clear();
    if (!(m_iFlags & CWiggleReader::fAsGraph)) {
        m_ChromId.clear();
        m_Values.clear();
        xParseTrackLine("track type=wiggle_0");
    }

    xProgressInit(lr);

    m_uDataCount = 0;
    CRef<CSeq_annot> pAnnot = xCreateSeqAnnot();

    TReaderData readerData;
    xGuardedGetData(lr, readerData, pEL);
    if (readerData.empty()) {
        pAnnot.Reset();
        return pAnnot;
    }
    xGuardedProcessData(readerData, *pAnnot, pEL);
    xPostProcessAnnot(*pAnnot);
    return pAnnot;
}

//  ----------------------------------------------------------------------------
void
CWiggleReader::xGetData(
    ILineReader& lr,
    TReaderData& readerData)
//  ----------------------------------------------------------------------------
{
    // Goal: Extract one wiggle graph object (including all its data lines) at
    //  once
    bool haveData = false;
    readerData.clear();
    string line;
    while (xGetLine(lr, line)) {
        bool isMeta = NStr::StartsWith(line, "fixedStep")  ||
            NStr::StartsWith(line, "variableStep")  ||
            xIsTrackLine(line)  ||
            xIsBrowserLine(line);
        if (isMeta) {
            if (haveData) {
                xUngetLine(lr);
                break;
            }
        }
        readerData.push_back(TReaderLine{m_uLineNumber, line});
        if (!isMeta) {
            haveData = true;
        }
        ++m_uDataCount;
    }
}

//  ----------------------------------------------------------------------------
void
CWiggleReader::xProcessData(
    const TReaderData& readerData,
    CSeq_annot& annot)
//  ----------------------------------------------------------------------------
{
    for (auto curData = readerData.begin(); curData != readerData.end(); curData++) {
        auto line = curData->mData;
        if (xParseBrowserLine(line, annot)) {
            continue;
        }
        if (xParseTrackLine(line)) {
            continue;
        }

        if (xProcessFixedStepData(curData, readerData)) {
            break;
        }
        if (xProcessVariableStepData(curData, readerData)) {
            break;
        }
        xProcessBedData(curData, readerData);
        break;
    }
}


//  ----------------------------------------------------------------------------
bool
CWiggleReader::ReadTrackData(
    ILineReader& lr,
    CRawWiggleTrack& rawdata,
    ILineErrorListener* pMessageListener)
//  ----------------------------------------------------------------------------
{
    TReaderData readerData;
    xGuardedGetData(lr, readerData, pMessageListener);
    auto curIt = readerData.cbegin();
    while (curIt != readerData.end()) {
        auto firstLine(curIt->mData);
        if (NStr::StartsWith(firstLine, "fixedStep")) {
            SFixedStepInfo fixedStepInfo;
            xGetFixedStepInfo(firstLine, fixedStepInfo);
            return xReadFixedStepDataRaw(
                fixedStepInfo, ++curIt, readerData, rawdata);
        }
        if (NStr::StartsWith(firstLine, "variableStep")) {
            SVarStepInfo varStepInfo;
            xGetVariableStepInfo(firstLine, varStepInfo);
            return xReadVariableStepDataRaw(
                varStepInfo, ++curIt, readerData, rawdata);
        }
        ++curIt;
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool
CWiggleReader::xReadFixedStepDataRaw(
    const SFixedStepInfo& fixedStepInfo,
    TReaderData::const_iterator& curIt,
    const TReaderData& readerData,
    CRawWiggleTrack& rawdata)
//  ----------------------------------------------------------------------------
{
    rawdata.Reset();

    CRef<CSeq_id> id =
        CReadUtil::AsSeqId(fixedStepInfo.mChrom, m_iFlags);

    unsigned int pos(fixedStepInfo.mStart);
    while (curIt != readerData.end()) {
        auto line(curIt->mData);
        double value(0);
        xGetDouble(line, value);
        rawdata.AddRecord(
            CRawWiggleRecord(*id, pos, fixedStepInfo.mSpan, value));
        pos += fixedStepInfo.mStep;
        curIt++;
    }
    return rawdata.HasData();
}

//  ----------------------------------------------------------------------------
bool
CWiggleReader::xReadVariableStepDataRaw(
    const SVarStepInfo& varStepInfo,
    TReaderData::const_iterator& curIt,
    const TReaderData& readerData,
    CRawWiggleTrack& rawdata)
//  ----------------------------------------------------------------------------
{
    rawdata.Reset();

    CRef<CSeq_id> id =
        CReadUtil::AsSeqId(varStepInfo.mChrom, m_iFlags);

    while (curIt != readerData.end()) {
        auto line(curIt->mData);
        unsigned int pos(0);
        xGetPos(line, pos);
        xSkipWS(line);
        double value(0);
        xGetDouble(line, value);
        rawdata.AddRecord(
            CRawWiggleRecord(*id, pos, varStepInfo.mSpan, value));
        curIt++;
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
    (m_iFlags & fAsByte) ? (ret += rows) : (ret += 8*rows);
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
CRef<CSeq_table> CWiggleReader::xMakeTable()
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

    table->SetNum_rows(static_cast<TSeqPos>(size));
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

        CSeqTable_multi_data::TInt2& values = col_val->SetData().SetInt2();
        values.reserve(size);
        ITERATE ( TValues, it, m_Values ) {
            pos.push_back(it->m_Pos);
            if ( span_ptr ) {
                span_ptr->push_back(it->m_Span);
            }
            values.push_back(stat.AsByte(it->m_Value));
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
CRef<CSeq_graph> CWiggleReader::xMakeGraph()
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
    CByte_graph::TValues& bytes = b_graph.SetValues();

    if ( m_Values.empty() ) {
        graph->SetNumval(0);
    }
    else {
        _ASSERT(stat.m_FixedSpan);
        TSeqPos start = m_Values[0].m_Pos;
        TSeqPos end = m_Values.back().GetEnd();
        size_t size = (end-start)/stat.m_Span;
        graph->SetNumval(static_cast<TSeqPos>(size));
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
bool CWiggleReader::xSkipWS(
    string& line)
//  ----------------------------------------------------------------------------
{
    const char* ptr = line.c_str();
    size_t skip = 0;
    for ( size_t len = line.size(); skip < len; ++skip ) {
        char c = ptr[skip];
        if ( c != ' ' && c != '\t' ) {
            break;
        }
    }
    line = line.substr(skip);
    return !line.empty();
}

//  ----------------------------------------------------------------------------
string CWiggleReader::xGetWord(
    string& line)
//  ----------------------------------------------------------------------------
{
    const char* ptr = line.c_str();
    size_t skip = 0;
    for ( size_t len = line.size(); skip < len; ++skip ) {
        char c = ptr[skip];
        if ( c == ' ' || c == '\t' ) {
            break;
        }
    }
    if ( skip == 0 ) {
        CReaderMessage error(eDiag_Error,
            m_uLineNumber,
            "Identifier expected");
        throw error;
    }
    string word(ptr, skip);
    line = line.substr(skip);
    return word;
}

//  ----------------------------------------------------------------------------
string CWiggleReader::xGetParamName(
    string& line)
//  ----------------------------------------------------------------------------
{
    const char* ptr = line.c_str();
    size_t skip = 0;
    for ( size_t len = line.size(); skip < len; ++skip ) {
        char c = ptr[skip];
        if ( c == '=' ) {
            string name(ptr, skip);
            line = line.substr(skip+1);
            return name;
        }
        if ( c == ' ' || c == '\t' ) {
            break;
        }
    }
    CReaderMessage error(eDiag_Error,
        m_uLineNumber,
        "\"=\" expected");
    throw error;
}

//  ----------------------------------------------------------------------------
string CWiggleReader::xGetParamValue(
    string& line)
//  ----------------------------------------------------------------------------
{
    const char* ptr = line.c_str();
    size_t len = line.size();
    if ( len && *ptr == '"' ) {
        size_t pos = 1;
        for ( ; pos < len; ++pos ) {
            char c = ptr[pos];
            if ( c == '"' ) {
                string value(ptr, pos);
                line = line.substr(pos+1);
                return value;
            }
        }
        CReaderMessage error(eDiag_Error,
            m_uLineNumber,
            "Open quotes");
        throw error;
    }
    return xGetWord(line);
}

//  ----------------------------------------------------------------------------
void CWiggleReader::xGetPos(
    string& line,
    TSeqPos& v)
//  ----------------------------------------------------------------------------
{
    CReaderMessage error(eDiag_Error,
        m_uLineNumber,
        "Integer value expected");

    char c = line.c_str()[0];
    if ( c < '0' || c > '9' ) {
        throw error;
    }

    TSeqPos ret = 0;
    const char* ptr = line.c_str();
    for ( size_t skip = 0; ; ++skip ) {
        char c = ptr[skip];
        if ( c >= '0' && c <= '9' ) {
            ret = ret*10 + (c-'0');
        }
        else if ( (c == ' ' || c == '\t' || c == '\0') && skip ) {
            line = line.substr(skip);
            v = ret;
            return;
        }
        else {
            throw error;
        }
    }
}

//  ----------------------------------------------------------------------------
bool CWiggleReader::xTryGetDoubleSimple(
    string& line,
    double& v)
//  ----------------------------------------------------------------------------
{
    double ret = 0;
    const char* ptr = line.c_str();
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
            line.clear();
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
            line.clear();
            v = (negate ? -ret : ret);
            return true;
        }
        else {
            return false;
        }
    }
}

//  ----------------------------------------------------------------------------
inline void CWiggleReader::xGetDouble(
    string& line,
    double& v)
//  ----------------------------------------------------------------------------
{
    if (xTryGetDoubleSimple(line, v)) {
        return;
    }
    const char* ptr = line.c_str();
    char* endptr = 0;
    v = strtod(ptr, &endptr);
    if ( endptr == ptr ) {
        CReaderMessage error(eDiag_Error,
            m_uLineNumber,
            "Floating point value expected");
        throw error;
    }
    if ( *endptr ) {
        CReaderMessage error(eDiag_Error,
            m_uLineNumber,
            "Extra text on line");
        throw error;
    }
    line.clear();
}

//  ----------------------------------------------------------------------------
void CWiggleReader::xPostProcessAnnot(
    CSeq_annot& annot)
//  ----------------------------------------------------------------------------
{
    if ( m_ChromId.empty() ) {
        return;
    }
    if (m_iFlags & fAsGraph) {
        annot.SetData().SetGraph().push_back(xMakeGraph());
    }
    else {
        annot.SetData().SetSeq_table(*xMakeTable());
    }
    if (annot.GetData().Which() != CSeq_annot::TData::e_not_set) {
        xAssignTrackData(annot);
    }
    m_ChromId.clear();
}

//  ----------------------------------------------------------------------------
void CWiggleReader::xDumpChromValues(void)
//  ----------------------------------------------------------------------------
{
    if ( m_ChromId.empty() ) {
        return;
    }
    if ( !m_Annot ) {
        m_Annot = xCreateSeqAnnot();
    }
    if (m_iFlags & fAsGraph) {
        m_Annot->SetData().SetGraph().push_back(xMakeGraph());
    }
    else {
        m_Annot->SetData().SetSeq_table(*xMakeTable());
    }
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
bool CWiggleReader::xParseBrowserLine(
    const CTempString& line,
    CSeq_annot&)
//  ----------------------------------------------------------------------------
{
    if (!NStr::StartsWith(line, "browser")) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CWiggleReader::xParseTrackLine(
    const CTempString& line)
//  ----------------------------------------------------------------------------
{
    if (!xIsTrackLine(line)) {
        return false;
    }
    CReaderBase::xParseTrackLine(line);

    m_TrackType = eTrackType_invalid;
    if (m_pTrackDefaults->Type() == "wiggle_0") {
        m_TrackType = eTrackType_wiggle_0;
        return true;
    }
    if (m_pTrackDefaults->Type() == "bedGraph") {
        m_TrackType = eTrackType_bedGraph;
        return true;
    }
    CReaderMessage error(eDiag_Error,
        m_uLineNumber,
        "Invalid track type");
    throw error;
}

//  ----------------------------------------------------------------------------
bool
CWiggleReader::xProcessFixedStepData(
    TReaderData::const_iterator& curIt,
    const TReaderData& readerData)
//  ----------------------------------------------------------------------------
{
    auto firstLine(curIt->mData);
    if (!NStr::StartsWith(firstLine, "fixedStep")) {
        return false;
    }

    SFixedStepInfo fixedStepInfo;
    xGetFixedStepInfo(firstLine, fixedStepInfo);
    ++curIt;
    xReadFixedStepData(fixedStepInfo, curIt, readerData);
    return true;
}


//  ----------------------------------------------------------------------------
bool
CWiggleReader::xProcessVariableStepData(
    TReaderData::const_iterator& curIt,
    const TReaderData& readerData)
//  ----------------------------------------------------------------------------
{
    auto firstLine(curIt->mData);
    if (!NStr::StartsWith(firstLine, "variableStep")) {
        return false;
    }

    SVarStepInfo variableStepInfo;
    xGetVariableStepInfo(firstLine, variableStepInfo);
    ++curIt;
    xReadVariableStepData(variableStepInfo, curIt, readerData);
    return true;
}


//  ----------------------------------------------------------------------------
void CWiggleReader::xGetFixedStepInfo(
    const string& directive,
    SFixedStepInfo& fixedStepInfo)
//  ----------------------------------------------------------------------------
{
    if ( m_TrackType != eTrackType_wiggle_0 ) {
        if ( m_TrackType != eTrackType_invalid ) {
            CReaderMessage error(eDiag_Error,
                m_uLineNumber,
                "Track \"type=wiggle_0\" is required");
            throw error;
        }
        else {
            m_TrackType = eTrackType_wiggle_0;
        }
    }

    auto line(directive.substr(string("fixedStep").size() + 1));
    NStr::TruncateSpacesInPlace(line);
    fixedStepInfo.Reset();
    while (xSkipWS(line)) {
        string name = xGetParamName(line);
        string value = xGetParamValue(line);
        if (name == "chrom") {
            fixedStepInfo.mChrom = value;
            continue;
        }
        if (name == "start") {
            fixedStepInfo.mStart = NStr::StringToUInt(value);
            if (0 == fixedStepInfo.mStart) {
                CReaderMessage warning(eDiag_Warning,
                    m_uLineNumber,
                    "Bad start value: must be positive. Assuming \"start=1\"");
                m_pMessageHandler->Report(warning);
                fixedStepInfo.mStart = 1;
            }
            continue;
        }
        if (name == "step") {
            fixedStepInfo.mStep = NStr::StringToUInt(value);
            continue;
        }
        if (name == "span") {
            fixedStepInfo.mSpan = NStr::StringToUInt(value);
            continue;
        }
        CReaderMessage warning(eDiag_Warning,
            m_uLineNumber,
            "Bad parameter name. Ignored");
        m_pMessageHandler->Report(warning);
    }
    if (fixedStepInfo.mChrom.empty()) {
        CReaderMessage error(eDiag_Error,
            m_uLineNumber,
            "Missing chrom parameter");
        throw error;
    }
    if (fixedStepInfo.mStart == 0) {
        CReaderMessage error(eDiag_Error,
            m_uLineNumber,
            "Missing start parameter");
        throw error;
    }
    if (fixedStepInfo.mStep == 0) {
        CReaderMessage error(eDiag_Error,
            m_uLineNumber,
            "Missing step parameter");
        throw error;
    }
}

//  ----------------------------------------------------------------------------
void CWiggleReader::xReadFixedStepData(
    const SFixedStepInfo& fixedStepInfo,
    TReaderData::const_iterator& curIt,
    const TReaderData& readerData)
//  ----------------------------------------------------------------------------
{
    xSetChrom(fixedStepInfo.mChrom);
    SValueInfo value;
    value.m_Chrom = fixedStepInfo.mChrom;
    value.m_Pos = fixedStepInfo.mStart-1;
    value.m_Span = fixedStepInfo.mSpan;
    while (curIt != readerData.end()) {
        auto line(curIt->mData);
        xGetDouble(line, value.m_Value);
        xAddValue(value);
        value.m_Pos += fixedStepInfo.mStep;
        curIt++;
    }
}

//  ----------------------------------------------------------------------------
void CWiggleReader::xGetVariableStepInfo(
    const string& directive,
    SVarStepInfo& varStepInfo)
//  ----------------------------------------------------------------------------
{
    if ( m_TrackType != eTrackType_wiggle_0 ) {
        if ( m_TrackType != eTrackType_invalid ) {
            CReaderMessage error(eDiag_Error,
                m_uLineNumber,
                "Track \"type=wiggle_0\" is required");
            throw error;
        }
        else {
            m_TrackType = eTrackType_wiggle_0;
        }
    }

    varStepInfo.Reset();
    auto line(directive.substr(string("variableStep").size() + 1));
    while (xSkipWS(line)) {
        string name = xGetParamName(line);
        string value = xGetParamValue(line);
        if ( name == "chrom" ) {
            varStepInfo.mChrom = value;
        }
        else if ( name == "span" ) {
            varStepInfo.mSpan = NStr::StringToUInt(value);
        }
        else {
            CReaderMessage warning(eDiag_Warning,
                m_uLineNumber,
                "Bad parameter name. Ignored");
            m_pMessageHandler->Report(warning);
        }
    }
    if ( varStepInfo.mChrom.empty() ) {
        CReaderMessage error(eDiag_Error,
            m_uLineNumber,
            "Missing chrom parameter");
        throw error;
    }
}

//  ----------------------------------------------------------------------------
void CWiggleReader::xReadVariableStepData(
    const SVarStepInfo& varStepInfo,
    TReaderData::const_iterator& curIt,
    const TReaderData& readerData)
//  ----------------------------------------------------------------------------
{
    xSetChrom(varStepInfo.mChrom);
    SValueInfo value;
    value.m_Chrom = varStepInfo.mChrom;
    value.m_Span = varStepInfo.mSpan;
    while (curIt != readerData.end()) {
        string line(curIt->mData);
        xGetPos(line, value.m_Pos);
        xSkipWS(line);
        xGetDouble(line, value.m_Value);
        value.m_Pos -= 1;
        xAddValue(value);
        curIt++;
    }
}

//  =========================================================================
bool CWiggleReader::xProcessBedData(
    TReaderData::const_iterator& curIt,
    const TReaderData& readerData)
//  =========================================================================
{
    while (curIt != readerData.end()) {
        auto line(curIt->mData);
        auto chrom = xGetWord(line);
        xSetChrom(chrom);

        SValueInfo value;
        xSkipWS(line);
        xGetPos(line, value.m_Pos);
        xSkipWS(line);
        xGetPos(line, value.m_Span);
        xSkipWS(line);
        xGetDouble(line, value.m_Value);
        value.m_Span -= value.m_Pos;
        xAddValue(value);

        curIt++;
    }
    return true;
}

END_objects_SCOPE
END_NCBI_SCOPE
