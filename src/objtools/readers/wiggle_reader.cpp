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
#include <objtools/readers/error_container.hpp>
#include <objtools/readers/reader_base.hpp>
#include <objtools/readers/wiggle_reader.hpp>

#include <objects/seqres/Seq_graph.hpp>
#include <objects/seqres/Real_graph.hpp>
#include <objects/seqres/Byte_graph.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
CWiggleReader::CWiggleReader(
    TFlags flags ) :
//  ----------------------------------------------------------------------------
    CReaderBase(flags),
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
    IErrorContainer* pErrorContainer ) 
//  ----------------------------------------------------------------------------                
{ 
    CRef<CSerialObject> object( 
        ReadSeqAnnot( lr, pErrorContainer ).ReleaseOrNull() );
    return object; 
}

//  ----------------------------------------------------------------------------                
CRef<CSeq_annot>
CWiggleReader::ReadSeqAnnot(
    ILineReader& lr,
    IErrorContainer* pErrorContainer ) 
//  ----------------------------------------------------------------------------                
{
    m_ChromId.clear();
    m_Values.clear();
    if (lr.AtEOF()) {
        return CRef<CSeq_annot>();
    }
    while ( xGetLine(lr) ) {
        if ( xCommentLine() ) {
            continue;
        }
        CTempString s = xGetWord(pErrorContainer);
        if ( s == "browser" ) {
            xReadBrowser();
        }
        else if ( s == "track" ) {
            xReadTrack(pErrorContainer);
        }
        else if ( s == "fixedStep" ) {
            if (!m_ChromId.empty()) {
                lr.UngetLine();
                return xGetAnnot();
            }
            xReadFixedStep(lr, pErrorContainer);
        }
        else if ( s == "variableStep" ) {
            if (!m_ChromId.empty()) {
                lr.UngetLine();
                return xGetAnnot();
            }
            xReadVariableStep(lr, pErrorContainer);
        }
        else {
            xReadBedLine(s, pErrorContainer);
        }
    }
    return xGetAnnot();
}
    
//  --------------------------------------------------------------------------- 
void
CWiggleReader::ReadSeqAnnots(
    vector< CRef<CSeq_annot> >& annots,
    CNcbiIstream& istr,
    IErrorContainer* pErrorContainer )
//  ---------------------------------------------------------------------------
{
    CStreamLineReader lr( istr );
    ReadSeqAnnots( annots, lr, pErrorContainer );
}
 
//  ---------------------------------------------------------------------------                       
void
CWiggleReader::ReadSeqAnnots(
    vector< CRef<CSeq_annot> >& annots,
    ILineReader& lr,
    IErrorContainer* pErrorContainer )
//  ----------------------------------------------------------------------------
{
    while ( ! lr.AtEOF() ) {
        CRef<CSeq_annot> pAnnot = ReadSeqAnnot( lr, pErrorContainer );
        if ( pAnnot ) {
            annots.push_back( pAnnot );
        }
    }
}

//  ----------------------------------------------------------------------------
void
CWiggleReader::xProcessError(
    CObjReaderLineException& err,
    IErrorContainer* pContainer)
//  ----------------------------------------------------------------------------
{
    err.SetLineNumber(m_uLineNumber);
    ProcessError(err, pContainer);
}

//  =========================================================================
double CWiggleReader::xEstimateSize(size_t rows, bool fixed_span) const
//  =========================================================================
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

//  =========================================================================
void CWiggleReader::xPreprocessValues(SWiggleStat& stat)
//  =========================================================================
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
         (!m_KeepInteger ||
          !stat.m_IntValues ||
          stat.m_Max-stat.m_Min > range) ) {
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

//  =========================================================================
CRef<CSeq_id> CWiggleReader::xMakeChromId()
//  =========================================================================
{
    CRef<CSeq_id> chrom_id(new CSeq_id(CSeq_id::e_Local, m_ChromId));
    return chrom_id;
}

//  =========================================================================
void CWiggleReader::xSetTotalLoc(CSeq_loc& loc, CSeq_id& chrom_id)
//  =========================================================================
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

//  =========================================================================
CRef<CSeq_table> CWiggleReader::xMakeTable(void)
//  =========================================================================
{
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
        col_id->SetDefault().SetId(*chrom_id);
    }
    
    // position
    CRef<CSeqTable_column> col_pos(new CSeqTable_column);
    table->SetColumns().push_back(col_pos);
    col_pos->SetHeader().SetField_id(CSeqTable_column_info::eField_id_location_from);
    CSeqTable_multi_data::TInt& pos = col_pos->SetData().SetInt();

    SWiggleStat stat;
    xPreprocessValues(stat);
    
    xSetTotalLoc(*table_loc, *chrom_id);

    size_t size = m_Values.size();
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

//  =========================================================================
CRef<CSeq_graph> CWiggleReader::xMakeGraph(void)
//  =========================================================================
{
    CRef<CSeq_graph> graph(new CSeq_graph);

    CRef<CSeq_id> chrom_id = xMakeChromId();

    CRef<CSeq_loc> graph_loc(new CSeq_loc);
    graph->SetLoc(*graph_loc);

    SWiggleStat stat;
    xPreprocessValues(stat);
    
    xSetTotalLoc(*graph_loc, *chrom_id);

    if ( !m_TrackName.empty() ) {
        graph->SetTitle(m_TrackName);
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

//  =========================================================================
CRef<CSeq_annot> CWiggleReader::xMakeAnnot(void)
//  =========================================================================
{
    CRef<CSeq_annot> annot(new CSeq_annot);
    if ( !m_TrackDescription.empty() ) {
        CRef<CAnnotdesc> desc(new CAnnotdesc);
        desc->SetTitle(m_TrackDescription);
        annot->SetDesc().Set().push_back(desc);
    }
    if ( !m_TrackName.empty() ) {
        CRef<CAnnotdesc> desc(new CAnnotdesc);
        desc->SetName(m_TrackName);
        annot->SetDesc().Set().push_back(desc);
    }
    if ( !m_TrackParams.empty() ) {
        CRef<CAnnotdesc> desc(new CAnnotdesc);
        annot->SetDesc().Set().push_back(desc);
        CUser_object& user = desc->SetUser();
        user.SetType().SetStr("Track Data");
        ITERATE ( TTrackParams, it, m_TrackParams ) {
            CRef<CUser_field> field(new CUser_field);
            field->SetLabel().SetStr(it->first);
            field->SetData().SetStr(it->second);
            user.SetData().push_back(field);
        }
    }
    return annot;
}

//  =========================================================================
CRef<CSeq_annot> CWiggleReader::xMakeTableAnnot(void)
//  =========================================================================
{
    CRef<CSeq_annot> annot = xMakeAnnot();
    annot->SetData().SetSeq_table(*xMakeTable());
    return annot;
}

//  =========================================================================
CRef<CSeq_annot> CWiggleReader::xMakeGraphAnnot(void)
//  =========================================================================
{
    CRef<CSeq_annot> annot = xMakeAnnot();
    annot->SetData().SetGraph().push_back(xMakeGraph());
    return annot;
}

//  =========================================================================
void CWiggleReader::xResetChromValues(void)
//  =========================================================================
{
    m_ChromId.clear();
    m_Values.clear();
}

//  =========================================================================
bool CWiggleReader::xSkipWS(void)
//  =========================================================================
{
    const char* ptr = m_CurLine.data();
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

//  =========================================================================
inline bool CWiggleReader::xCommentLine(void) const
//  =========================================================================
{
    char c = m_CurLine.data()[0];
    return c == '#' || c == '\0';
}

//  =========================================================================
CTempString CWiggleReader::xGetWord(
    IErrorContainer* pErrorContainer)
//  =========================================================================
{
    const char* ptr = m_CurLine.data();
    size_t skip = 0;
    for ( size_t len = m_CurLine.size(); skip < len; ++skip ) {
        char c = ptr[skip];
        if ( c == ' ' || c == '\t' ) {
            break;
        }
    }
    if ( skip == 0 ) {
        CObjReaderLineException err(
            eDiag_Warning,
            0,
            "Identifier expected");
        xProcessError(err, pErrorContainer);
    }
    m_CurLine = m_CurLine.substr(skip);
    return CTempString(ptr, skip);
}

//  =========================================================================
CTempString CWiggleReader::xGetParamName(
    IErrorContainer* pErrorContainer)
//  =========================================================================
{
    const char* ptr = m_CurLine.data();
    size_t skip = 0;
    for ( size_t len = m_CurLine.size(); skip < len; ++skip ) {
        char c = ptr[skip];
        if ( c == '=' ) {
            m_CurLine = m_CurLine.substr(skip+1);
            return CTempString(ptr, skip);
        }
        if ( c == ' ' || c == '\t' ) {
            break;
        }
    }
    CObjReaderLineException err(
        eDiag_Warning,
        0,
        "\"=\" expected");
    xProcessError(err, pErrorContainer);
    return CTempString();
}

//  =========================================================================
CTempString CWiggleReader::xGetParamValue(
    IErrorContainer* pErrorContainer)
//  =========================================================================
{
    const char* ptr = m_CurLine.data();
    size_t len = m_CurLine.size();
    if ( len && *ptr == '"' ) {
        size_t pos = 1;
        for ( ; pos < len; ++pos ) {
            char c = ptr[pos];
            if ( c == '"' ) {
                m_CurLine = m_CurLine.substr(pos+1);
                return CTempString(ptr+1, pos-1);
            }
        }
        CObjReaderLineException err(
            eDiag_Warning,
            0,
            "Open quotes");
        xProcessError(err, pErrorContainer);
    }
    return xGetWord(pErrorContainer);
}

//  =========================================================================
void CWiggleReader::xGetPos(
    TSeqPos& v,
    IErrorContainer* pErrorContainer)
//  =========================================================================
{
    TSeqPos ret = 0;
    const char* ptr = m_CurLine.data();
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
        CObjReaderLineException err(
            eDiag_Error,
            0,
            "Integer value expected");
        xProcessError(err, pErrorContainer);
        }
    }
}

//  =========================================================================
bool CWiggleReader::xTryGetDoubleSimple(double& v)
//  =========================================================================
{
    double ret = 0;
    const char* ptr = m_CurLine.data();
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
            v = ret;
            if ( negate ) {
                ret = -ret;
            }
            return true;
        }
        else {
            return false;
        }
    }
}

//  =========================================================================
bool CWiggleReader::xTryGetDouble(
    double& v,
    IErrorContainer* pErrorContainer)
//  =========================================================================
{
    if ( xTryGetDoubleSimple(v) ) {
        return true;
    }
    const char* ptr = m_CurLine.data();
    char* endptr = 0;
    v = strtod(ptr, &endptr);
    if ( endptr == ptr ) {
        return false;
    }
    if ( *endptr ) {
        CObjReaderLineException err(
            eDiag_Warning,
            0,
            "Extra text on line");
        xProcessError(err, pErrorContainer);
    }
    m_CurLine.clear();
    return true;
}

//  =========================================================================
inline bool CWiggleReader::xTryGetPos(
    TSeqPos& v,
    IErrorContainer* pErrorContainer)
//  =========================================================================
{
    char c = m_CurLine.data()[0];
    if ( c < '0' || c > '9' ) {
        return false;
    }
    xGetPos(v, pErrorContainer);
    return true;
}

//  =========================================================================
inline void CWiggleReader::xGetDouble(
    double& v,
    IErrorContainer* pErrorContainer)
//  =========================================================================
{
    if ( !xTryGetDouble(v, pErrorContainer) ) {
        CObjReaderLineException err(
            eDiag_Error,
            0,
            "Floating point value expected");
        xProcessError(err, pErrorContainer);
    }
}

//  =========================================================================
bool CWiggleReader::xGetLine(
    ILineReader& lr)
//  =========================================================================
{
    if ( lr.AtEOF() ) {
        return false;
    }
    m_CurLine = *++lr;
    return true;
}

//  =========================================================================
CRef<CSeq_annot> CWiggleReader::xGetAnnot()
//  =========================================================================
{
    if ( m_ChromId.empty() ) {
        return CRef<CSeq_annot>();
    }
    CRef<CSeq_annot> pAnnot = xMakeAnnot();
    if (m_iFlags & fAsGraph) {
        pAnnot->SetData().SetGraph().push_back(xMakeGraph());
    }
    else {
        pAnnot->SetData().SetSeq_table(*xMakeTable());
    }
    m_ChromId.clear();
    return pAnnot;
}

//  =========================================================================
void CWiggleReader::xDumpChromValues(void)
//  =========================================================================
{
    if ( m_ChromId.empty() ) {
        return;
    }
    LOG_POST("Chrom: "<<m_ChromId<<" "<<m_Values.size());
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
    xResetChromValues();
}

//  =========================================================================
void CWiggleReader::xSetChrom(CTempString chrom)
//  =========================================================================
{
    if ( chrom != m_ChromId ) {
        xDumpChromValues();
        m_ChromId = chrom;
    }
}

//  =========================================================================
void CWiggleReader::xReadBrowser(void)
//  =========================================================================
{
}

//  =========================================================================
void CWiggleReader::xReadTrack(
    IErrorContainer* pErrorContainer)
//  =========================================================================
{
    m_TrackName = "User Track";
    m_TrackDescription.clear();
    m_TrackTypeValue.clear();
    m_TrackType = eTrackType_invalid;
    m_TrackParams.clear();
    while ( xSkipWS() ) {
        CTempString name = xGetParamName(pErrorContainer);
        CTempString value = xGetParamValue(pErrorContainer);
        if ( name == "type" ) {
            m_TrackTypeValue = value;
            if ( value == "wiggle_0" ) {
                m_TrackType = eTrackType_wiggle_0;
            }
            else if ( value == "bedGraph" ) {
                m_TrackType = eTrackType_bedGraph;
            }
            else {
                CObjReaderLineException err(
                    eDiag_Warning,
                    0,
                    "Invalid track type");
                xProcessError(err, pErrorContainer);
            }
        }
        else if ( name == "name" ) {
            m_TrackName = value;
        }
        else if ( name == "description" ) {
            m_TrackDescription = value;
        }
        else {
            m_TrackParams[name] = value;
        }
    }
    if ( m_TrackType == eTrackType_invalid ) {
        CObjReaderLineException err(
            eDiag_Error,
            0,
            "Unknown track type");
        xProcessError(err, pErrorContainer);
    }
}

//  =========================================================================
void CWiggleReader::xReadFixedStep(
    ILineReader& lr,
    IErrorContainer* pErrorContainer)
//  =========================================================================
{
    if ( m_TrackType != eTrackType_wiggle_0 ) {
        if ( m_TrackType != eTrackType_invalid ) {
            CObjReaderLineException err(
                eDiag_Warning,
                0,
                "Track \"type=wiggle_0\" is required");
            xProcessError(err, pErrorContainer);
        }
        else {
            m_TrackType = eTrackType_wiggle_0;
        }
    }

    size_t start = 0;
    size_t step = 0;
    size_t span = 1;
    while ( xSkipWS() ) {
        CTempString name = xGetParamName(pErrorContainer);
        CTempString value = xGetParamValue(pErrorContainer);
        if ( name == "chrom" ) {
            xSetChrom(value);
        }
        else if ( name == "start" ) {
            start = NStr::StringToUInt(value);
        }
        else if ( name == "step" ) {
            step = NStr::StringToUInt(value);
        }
        else if ( name == "span" ) {
            span = NStr::StringToUInt(value);
        }
        else {
            CObjReaderLineException err(
                eDiag_Warning,
                0,
                "Bad parameter name");
            xProcessError(err, pErrorContainer);
        }
    }
    if ( m_ChromId.empty() ) {
        CObjReaderLineException err(
            eDiag_Error,
            0,
            "Missing chrom parameter");
        xProcessError(err, pErrorContainer);
    }
    if ( start == 0 ) {
        CObjReaderLineException err(
            eDiag_Error,
            0,
            "Missing start value");
        xProcessError(err, pErrorContainer);
    }
    if ( step == 0 ) {
        CObjReaderLineException err(
            eDiag_Error,
            0,
            "Missing step value");
        xProcessError(err, pErrorContainer);
    }

    SValueInfo value;
    value.m_Pos = start-1;
    value.m_Span = span;
    while ( xGetLine(lr) ) {
        if ( !xTryGetDouble(value.m_Value, pErrorContainer) ) {
            lr.UngetLine();
            break;
        }
        xAddValue(value);
        value.m_Pos += step;
    }
}

//  =========================================================================
void CWiggleReader::xReadVariableStep(
    ILineReader& lr,
    IErrorContainer* pErrorContainer)
//  =========================================================================
{
    if ( m_TrackType != eTrackType_wiggle_0 ) {
        if ( m_TrackType != eTrackType_invalid ) {
            CObjReaderLineException err(
                eDiag_Warning,
                0,
                "Track \"type=wiggle_0\" is required");
            xProcessError(err, pErrorContainer);
        }
        else {
            m_TrackType = eTrackType_wiggle_0;
        }
    }

    size_t span = 1;
    while ( xSkipWS() ) {
        CTempString name = xGetParamName(pErrorContainer);
        CTempString value = xGetParamValue(pErrorContainer);
        if ( name == "chrom" ) {
            xSetChrom(value);
        }
        else if ( name == "span" ) {
            span = NStr::StringToUInt(value);
        }
        else {
            CObjReaderLineException err(
                eDiag_Warning,
                0,
                "Bad parameter name");
            xProcessError(err, pErrorContainer);
        }
    }
    if ( m_ChromId.empty() ) {
        CObjReaderLineException err(
            eDiag_Error,
            0,
            "Missing chrom parameter");
        xProcessError(err, pErrorContainer);
    }
    SValueInfo value;
    value.m_Span = span;
    while ( xGetLine(lr) ) {
        if ( !xTryGetPos(value.m_Pos, pErrorContainer) ) {
            lr.UngetLine();
            break;
        }
        xSkipWS();
        xGetDouble(value.m_Value, pErrorContainer);
        value.m_Pos -= 1;
        xAddValue(value);
    }
}

//  =========================================================================
void CWiggleReader::xReadBedLine(
    CTempString chrom,
    IErrorContainer* pErrorContainer)
//  =========================================================================
{
    if ( m_TrackType != eTrackType_bedGraph &&
        m_TrackType != eTrackType_invalid ) {
        CObjReaderLineException err(
            eDiag_Warning,
            0,
            "Track \"type=bedGraph\" is required");
        xProcessError(err, pErrorContainer);
    }
    xSetChrom(chrom);
    SValueInfo value;
    xSkipWS();
    xGetPos(value.m_Pos, pErrorContainer);
    xSkipWS();
    xGetPos(value.m_Span, pErrorContainer);
    xSkipWS();
    xGetDouble(value.m_Value, pErrorContainer);
    value.m_Span -= value.m_Pos;
    xAddValue(value);
}

END_objects_SCOPE
END_NCBI_SCOPE
