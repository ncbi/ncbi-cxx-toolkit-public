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
 * Authors:  Frank Ludwig
 *
 * File Description:  Write wiggle file
 *
 */

#include <ncbi_pch.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seq_annot.hpp>

#include <objects/seqres/Seq_graph.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqres/Byte_graph.hpp>
#include <objects/seqres/Int_graph.hpp>
#include <objects/seqres/Real_graph.hpp>
#include <objects/seqtable/Seq_table.hpp>
#include <objects/seqtable/SeqTable_column.hpp>
#include <objects/seqtable/SeqTable_column_info.hpp>

#include <objtools/writers/write_util.hpp>
#include <objtools/writers/wiggle_writer.hpp>

#include <math.h>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

#define ROUND(x)    int( floor((x)+0.5) )

//  ----------------------------------------------------------------------------
CWiggleWriter::CWiggleWriter(
    CScope& scope,
    CNcbiOstream& ostr,
    size_t uTrackSize) :
//  ----------------------------------------------------------------------------
    CWriterBase(ostr, 0),
    mpScope(&scope),
    m_uTrackSize(uTrackSize == 0 ? size_t( -1 ) : uTrackSize)
{
};

//  ----------------------------------------------------------------------------
CWiggleWriter::CWiggleWriter(
    CNcbiOstream& ostr,
    size_t uTrackSize ) :
//  ----------------------------------------------------------------------------
    CWriterBase(ostr, 0),
    mpScope(nullptr),
    m_uTrackSize(uTrackSize == 0 ? size_t( -1 ) : uTrackSize)
{
};

//  ----------------------------------------------------------------------------
CWiggleWriter::~CWiggleWriter()
//  ----------------------------------------------------------------------------
{
};

//  ----------------------------------------------------------------------------
bool CWiggleWriter::WriteAnnot(
    const CSeq_annot& annot,
    const string&,
    const string& )
//  ----------------------------------------------------------------------------
{
    if (annot.IsGraph()) {
        return xWriteAnnotGraphs(annot);
    }
    if (annot.IsSeq_table()) {
        return xWriteAnnotTable(annot);
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CWiggleWriter::WriteFooter()
//  ----------------------------------------------------------------------------
{
    m_Os << '\n';
    return true;
}

//  ----------------------------------------------------------------------------
bool CWiggleWriter::xWriteAnnotGraphs( const CSeq_annot& annot )
//  ----------------------------------------------------------------------------
{
    if (!annot.CanGetDesc()) {
        if (!xWriteDefaultTrackLine()) {
            return false;
        }
    }
    else {
        const CAnnot_descr& descr = annot.GetDesc();
        if (!xWriteTrackLine(descr)) {
            return false;
        }
    }

    const list< CRef<CSeq_graph> >& graphs = annot.GetData().GetGraph();
    list< CRef<CSeq_graph> >::const_iterator it = graphs.begin();
    while ( it != graphs.end() ) {
        if ( ! xWriteSingleGraph( **it ) ) {
            return false;
        }
        ++it;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CWiggleWriter::xWriteSingleGraph( const CSeq_graph& graph )
//  ----------------------------------------------------------------------------
{
    if ( ! graph.CanGetNumval() ) {
        return false;
    }
    size_t uNumVals = graph.GetNumval();

    for ( size_t u=0; u < uNumVals; u += m_uTrackSize ) {
        if (xContainsDataByte(graph, u)) {
            if (!xWriteSingleGraphFixedStep(graph, u)) {
                return false;
            }
            if (!xWriteSingleGraphRecordsByte(graph, u)) {
                return false;
            }
        }
        if (xContainsDataInt(graph, u)) {
            if (!xWriteSingleGraphFixedStep(graph, u)) {
                return false;
            }
            if (!xWriteSingleGraphRecordsInt(graph, u)) {
                return false;
            }
        }
        if (xContainsDataReal(graph, u)) {
            if (!xWriteSingleGraphFixedStep(graph, u)) {
                return false;
            }
            if (!xWriteSingleGraphRecordsReal(graph, u)) {
                return false;
            }
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CWiggleWriter::xWriteDefaultTrackLine()
//  ----------------------------------------------------------------------------
{
    m_Os << "track type=wiggle_0" << '\n';
    return true;
}

//  ----------------------------------------------------------------------------
bool CWiggleWriter::xWriteTrackLine( const CAnnot_descr& descr )
//  ----------------------------------------------------------------------------
{
    string strTrackLine( "track type=wiggle_0" );

    const list<CRef< CAnnotdesc > >& data = descr.Get();
    list< CRef< CAnnotdesc > >::const_iterator it = data.begin();
    while ( it != data.end() ) {
        if ( (*it)->IsName() ) {
            strTrackLine += " name=\"";
            strTrackLine += (*it)->GetName();
            strTrackLine += "\"";
        }
        if ( ! (*it)->IsUser() ) {
            ++it;
            continue;
        }
        const CUser_object& user = (*it)->GetUser();
        if ( ! user.CanGetType() || ! user.GetType().IsStr() ||
            user.GetType().GetStr() != "Track Data")
        {
            ++it;
            continue;
        }
        if ( ! user.CanGetData() ) {
            ++it;
            continue;
        }

        const vector< CRef< CUser_field > >& fields = user.GetData();
        for ( size_t u=0; u < fields.size(); ++u ) {
            const CUser_field& field = *(fields[u]);
            strTrackLine += " ";
            strTrackLine += field.GetLabel().GetStr();
            strTrackLine += "=";
            strTrackLine += field.GetData().GetStr();
        }

        ++it;
    }

    m_Os << strTrackLine << '\n';
    return true;
}

//  ----------------------------------------------------------------------------
bool CWiggleWriter::xWriteSingleGraphFixedStep(
    const CSeq_graph& graph,
    size_t uSeqStart )
//  ----------------------------------------------------------------------------
{
    string strFixedStep( "fixedStep" );

    if ( ! graph.CanGetLoc() ) {
        return false;
    }
    if ( ! graph.CanGetComp() ) {
        return false;
    }

    //  ------------------------------------------------------------------------
    string strChrom;
    //  ------------------------------------------------------------------------
    const CSeq_id* pId = graph.GetLoc().GetId();
    switch ( pId->Which() ) {
    default:
        //pId->GetLabel(&strChrom, CSeq_id::eContent);
        pId->GetLabel(&strChrom);
        if (mpScope) {
            string bestId;
            GetBestId(CSeq_id_Handle::GetHandle(strChrom), *mpScope, bestId);
            strChrom = bestId;
        }
        break;

    case CSeq_id::e_Local:
        if ( pId->GetLocal().IsStr() ) {
            strChrom += pId->GetLocal().GetStr();
            break;
        }
        //pId->GetLabel(&strChrom, CSeq_id::eContent);
        pId->GetLabel(&strChrom);
        break;
    }
    strFixedStep += string(" chrom=");
    strFixedStep += strChrom;

    //  ------------------------------------------------------------------------
    string strStart( " start=" );
    //  ------------------------------------------------------------------------
    size_t uFrom = 0;
    const CSeq_loc& location = graph.GetLoc();
    if ( location.IsInt() && location.GetInt().CanGetFrom() ) {
        uFrom = location.GetInt().GetFrom();;
    }

    strStart += NStr::NumericToString( (uFrom + 1) + uSeqStart * graph.GetComp() );
    strFixedStep += strStart;

    //  ------------------------------------------------------------------------
    string strSpan( " span=" );
    string strStep( " step=" );
    //  ------------------------------------------------------------------------
    string strComp = NStr::IntToString( graph.GetComp() );
    strStep += strComp;
    strFixedStep += strStep;
    strSpan += strComp;
    strFixedStep += strSpan;

    m_Os << strFixedStep << '\n';
    return true;
}

//  ----------------------------------------------------------------------------
bool CWiggleWriter::xWriteSingleGraphRecordsByte(
    const CSeq_graph& graph,
    size_t uStartRecord )
//  ----------------------------------------------------------------------------
{
    if ( ! graph.CanGetA() || ! graph.CanGetB() ) {
        return false;
    }
    if ( ! graph.CanGetNumval() || ! graph.CanGetGraph() ) {
        return false;
    }
    if ( ! graph.GetGraph().IsByte() || ! graph.GetGraph().GetByte().CanGetValues() ) {
        return false;
    }
    double dA = graph.GetA();
    double dB = graph.GetB();
    size_t uNumVals = graph.GetNumval();
    const CByte_graph::TValues& values = graph.GetGraph().GetByte().GetValues();

    for ( size_t u=0; u + uStartRecord < uNumVals && u < m_uTrackSize; ++u ) {

        if (IsCanceled()) {
            NCBI_THROW(
                CObjWriterException,
                eInterrupted,
                "Processing terminated by user");
        }

        int iVal = (unsigned char)values[u + uStartRecord];
        m_Os << ROUND( dA*iVal+dB ) << '\n';
    }
//    m_Os << '\n';
    return true;
}

//  ----------------------------------------------------------------------------
bool CWiggleWriter::xWriteSingleGraphRecordsInt(
    const CSeq_graph& graph,
    size_t uStartRecord )
//  ----------------------------------------------------------------------------
{
    if ( ! graph.CanGetA() || ! graph.CanGetB() ) {
        return false;
    }
    if ( ! graph.CanGetNumval() || ! graph.CanGetGraph() ) {
        return false;
    }
    if ( ! graph.GetGraph().IsInt() || ! graph.GetGraph().GetInt().CanGetValues() ) {
        return false;
    }
    double dA = graph.GetA();
    double dB = graph.GetB();
    size_t uNumVals = graph.GetNumval();
    const vector<int>& values = graph.GetGraph().GetInt().GetValues();

    for ( size_t u=0; u + uStartRecord < uNumVals && u < m_uTrackSize; ++u ) {

        if (IsCanceled()) {
            NCBI_THROW(
                CObjWriterException,
                eInterrupted,
                "Processing terminated by user");
        }

        double dVal = values[u + uStartRecord];
        m_Os << (dA*dVal+dB) << '\n';
    }
//    m_Os << '\n';
    return true;
}

//  ----------------------------------------------------------------------------
bool CWiggleWriter::xWriteSingleGraphRecordsReal(
    const CSeq_graph& graph,
    size_t uStartRecord )
//  ----------------------------------------------------------------------------
{
    if ( ! graph.CanGetA() || ! graph.CanGetB() ) {
        return false;
    }
    if ( ! graph.CanGetNumval() || ! graph.CanGetGraph() ) {
        return false;
    }
    if ( ! graph.GetGraph().IsReal() || ! graph.GetGraph().GetReal().CanGetValues() ) {
        return false;
    }
    double dA = graph.GetA();
    double dB = graph.GetB();
    size_t uNumVals = graph.GetNumval();
    const vector<double>& values = graph.GetGraph().GetReal().GetValues();

    for ( size_t u=0; u + uStartRecord < uNumVals && u < m_uTrackSize; ++u ) {

        if (IsCanceled()) {
            NCBI_THROW(
                CObjWriterException,
                eInterrupted,
                "Processing terminated by user");
        }

        double dVal = values[u + uStartRecord];
        m_Os << ( dA*dVal+dB ) << '\n';
    }
//    m_Os << '\n';
    return true;
}

//  ----------------------------------------------------------------------------
bool CWiggleWriter::xContainsDataByte(
    const CSeq_graph& graph,
    size_t uStartRecord )
//  ----------------------------------------------------------------------------
{
    if ( ! graph.CanGetNumval() || ! graph.CanGetGraph() ) {
        return false;
    }
    if ( ! graph.GetGraph().IsByte() || ! graph.GetGraph().GetByte().CanGetValues() ) {
        return false;
    }
    size_t uNumVals = graph.GetNumval();
    const CByte_graph::TValues& values = graph.GetGraph().GetByte().GetValues();
    for ( size_t u=0; u + uStartRecord < uNumVals && u < m_uTrackSize; ++u ) {
        int iVal = (unsigned char)values[u + uStartRecord];
        if ( 0 != iVal ) {
            return true;
        }
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CWiggleWriter::xContainsDataInt(
    const CSeq_graph& graph,
    size_t uStartRecord )
//  ----------------------------------------------------------------------------
{
    if ( ! graph.CanGetNumval() || ! graph.CanGetGraph() ) {
        return false;
    }
    if ( ! graph.GetGraph().IsInt() || ! graph.GetGraph().GetInt().CanGetValues() ) {
        return false;
    }
    size_t uNumVals = graph.GetNumval();
    const vector<int>& values = graph.GetGraph().GetInt().GetValues();
    for ( size_t u=0; u + uStartRecord < uNumVals && u < m_uTrackSize; ++u ) {
        int iVal = (unsigned char)values[u + uStartRecord];
        if ( 0 != iVal ) {
            return true;
        }
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CWiggleWriter::xContainsDataReal(
    const CSeq_graph& graph,
    size_t uStartRecord )
//  ----------------------------------------------------------------------------
{
    if ( ! graph.CanGetNumval() || ! graph.CanGetGraph() ) {
        return false;
    }
    if ( ! graph.GetGraph().IsReal() || ! graph.GetGraph().GetReal().CanGetValues() ) {
        return false;
    }
    size_t uNumVals = graph.GetNumval();
    const vector<double>& values = graph.GetGraph().GetReal().GetValues();
    for ( size_t u=0; u + uStartRecord < uNumVals && u < m_uTrackSize; ++u ) {
        int iVal = (unsigned char)values[u + uStartRecord];
        if ( 0 != iVal ) {
            return true;
        }
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CWiggleWriter::xWriteAnnotTable( const CSeq_annot& annot )
//  ----------------------------------------------------------------------------
{
    if (!annot.CanGetDesc()) {
        if (!xWriteDefaultTrackLine()) {
            return false;
        }
    }
    else {
        const CAnnot_descr& descr = annot.GetDesc();
        if (!xWriteTrackLine(descr)) {
            return false;
        }
    }

    string chrom;
    int span(0), start(0), step(0);
    const CSeq_table& table = annot.GetData().GetSeq_table();
    if (xIsFixedStepData(table, chrom, span, start, step)) {
        return xWriteTableFixedStep(table, chrom, span, start, step);
    }
    if (xIsVariableStepData(table, chrom, span)) {
        return xWriteTableVariableStep(table, chrom, span);
    }
    return xWriteTableBedStyle(table);
}

//  ----------------------------------------------------------------------------
bool CWiggleWriter::xIsFixedStepData(
    const CSeq_table& table,
    string& chrom,
    int& span,
    int& start,
    int& step)
//
//  Criterion: Must be variableStep data to begin with.
//    In addition, consecutive in points are equidistant from each other.
//  ----------------------------------------------------------------------------
{
    chrom.clear();
    span = start = step = 0;

    if (!xIsVariableStepData(table, chrom, span)) {
        return false;
    }
    if (!xTableGetPosIn(table, 0, start)) {
        return false;
    }
    int currentIn(0);
    if (!xTableGetPosIn(table, 1, currentIn)) {
        return false;
    }
    step = currentIn - start;

    int numRows = table.GetNum_rows();
    for (int i=1; i < numRows-1; ++i) {
        int nextIn(0);
        if (!xTableGetPosIn(table, i+1, nextIn)) {
            return false;
        }
        if (nextIn-currentIn  !=  step) {
            return false;
        }
        currentIn = nextIn;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CWiggleWriter::xIsVariableStepData(
    const CSeq_table& table,
    string& chrom,
    int& span)
//
//  Criterion: At least two rows or it is simply not worth it.
//    All table rows have the same ID, and the table has a "span" column.
//  ----------------------------------------------------------------------------
{
    chrom.clear();
    span = 0;

    int numRows = table.GetNum_rows();
    if (numRows < 2) {//don't bother
        return false;
    }
    if (!xTableGetChromName(table, 0, chrom)) {
        return false;
    }
    for (int i=1; i < numRows; ++i) {
        string curChrom;
        if (!xTableGetChromName(table, i, curChrom)  ||  curChrom != chrom) {
            chrom.clear();
            return false;
        }
    }

    const vector<CRef<CSeqTable_column> > columns = table.GetColumns();
    for (size_t u = 0; u < columns.size(); ++u) {
        const CSeqTable_column_info& header = columns[u]->GetHeader();
        if (header.IsSetField_name()) {
            string fieldName = header.GetField_name();
            if (fieldName == "span") {
                for (size_t i=0; i < numRows; ++i) {
                    int curspan = 0;
                    if (!columns[u]->TryGetInt(i, curspan)) {
                        return false;
                    }
                    if (span == 0) {
                        span = curspan;
                    }
                    else if (curspan != span) {
                        return false;
                    }
                }
                return true;
            }
        }
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CWiggleWriter::xWriteTableFixedStep(
    const CSeq_table& table,
    const string& chromId,
    int span,
    int start,
    int step)
//  Record format is:
//      variableStep crom=<chromName> span=<span> start=<start> step=<step>
//      value
//      ...
//  ----------------------------------------------------------------------------
{
    string chrom(chromId);
    if (mpScope) {
        string bestId;
        GetBestId(CSeq_id_Handle::GetHandle(chrom), *mpScope, bestId);
        chrom = bestId;
    }

    //write fixedStep directive
    m_Os << "fixedStep chrom=" << chrom << " span=" << span <<
        " start=" << (start+1) << " step=" << step << '\n';

    //write "value" lines
    int numRows = table.GetNum_rows();
    for (int i=0; i < numRows; ++i) {

        if (IsCanceled()) {
            NCBI_THROW(
                CObjWriterException,
                eInterrupted,
                "Processing terminated by user");
        }

        double value(0);
        if (!xTableGetValue(table, i, value)) {
            return false;
        }
        m_Os << value << '\n';
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CWiggleWriter::xWriteTableVariableStep(
    const CSeq_table& table,
    const string& chromId,
    int span)
//
//  Record format is:
//      variableStep crom=<chromName> span=<span>
//      posIn value
//      ...
//  ----------------------------------------------------------------------------
{
    string chrom(chromId);
    if (mpScope) {
        string bestId;
        GetBestId(CSeq_id_Handle::GetHandle(chrom), *mpScope, bestId);
        chrom = bestId;
    }

    //write variableStep directive
    m_Os << "variableStep chrom=" << chrom << " span=" << span << '\n';

    //write "posIn value pairs"
    int numRows = table.GetNum_rows();
    for (int i=0; i < numRows; ++i) {

        if (IsCanceled()) {
            NCBI_THROW(
                CObjWriterException,
                eInterrupted,
                "Processing terminated by user");
        }

        int posIn(0);
        if (!xTableGetPosIn(table, i, posIn)) {
            return false;
        }
        double value(0);
        if (!xTableGetValue(table, i, value)) {
            return false;
        }
        m_Os << (posIn+1) << '\t' << value << '\n';
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CWiggleWriter::xWriteTableBedStyle(
    const CSeq_table& table)
//
//  Record format is:
//      chromName posIn+1 posOut+1 value
//  or:
//      variableStep chrom=chromName span=intervalSize
//      posIn+1   value
// or:
//      posIn+1   value
// if the span has not changed from the last record.
//  ----------------------------------------------------------------------------
{
    map<string, string> bestIdCache;

    int numRows = table.GetNum_rows();
    int lastSpan(0);

    for (int i=0; i < numRows; ++i) {

        if (IsCanceled()) {
            NCBI_THROW(
                CObjWriterException,
                eInterrupted,
                "Processing terminated by user");
        }

        string chrom;
        if (!xTableGetChromName(table, i, chrom)) {
            return false;
        }
        auto idIt = bestIdCache.find(chrom);
        if (idIt != bestIdCache.end()) {
            chrom = idIt->second;
        }
        else {
            if (mpScope) {
                string bestId;
                GetBestId(CSeq_id_Handle::GetHandle(chrom), *mpScope, bestId);
                bestIdCache[chrom] = bestId;
                chrom = bestId;
            }
            else {
                bestIdCache[chrom] = chrom;
            }
        }

        int posIn(0);
        if (!xTableGetPosIn(table, i, posIn)) {
            return false;
        }

        int posOut(0);
        if (!xTableGetPosOut(table, i, posIn, posOut)) {
            return false;
        }

        double value(0);
        if (!xTableGetValue(table, i, value)) {
            return false;
        }
#define BED_AS_VARSTEP 1
#if BED_AS_VARSTEP
        auto span = posOut - posIn;
        if (span != lastSpan) {
            m_Os << "variableStep chrom=" << chrom << " span=" << span << '\n';
            lastSpan = span;
        }
        m_Os << posIn+1 << '\t' << value << '\n';
#else
        m_Os << chrom << '\t' << (posIn + 1) << '\t' << (posOut + 1) << '\t' << value << '\n';
#endif
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CWiggleWriter::xTableGetChromName(
    const CSeq_table& table,
    int index,
    string& chrom)
//  ----------------------------------------------------------------------------
{
    const vector<CRef<CSeqTable_column> > columns = table.GetColumns();
    for (size_t u = 0; u < columns.size(); ++u) {
        const CSeqTable_column_info& header = columns[u]->GetHeader();
        if (header.IsSetField_name()) {
            string fieldName = header.GetField_name();
            if (fieldName == "Seq-table location") {
                CConstRef< CSeq_loc > pLoc = columns[u]->GetSeq_loc(index);
                pLoc->GetId()->GetLabel(&chrom, CSeq_id::eContent);
                if (mpScope) {
                    string bestId;
                    GetBestId(CSeq_id_Handle::GetHandle(chrom), *mpScope, bestId);
                    chrom = bestId;
                }
                return true;
            }
        }
        if (header.IsSetField_id()) {
            int fieldId = header.GetField_id();
            if (fieldId == CSeqTable_column_info::eField_id_location_id) {
                CConstRef< CSeq_id > pId = columns[u]->GetSeq_id(index);
                pId->GetLabel(&chrom, CSeq_id::eContent);
                if (mpScope) {
                    string bestId;
                    GetBestId(CSeq_id_Handle::GetHandle(chrom), *mpScope, bestId);
                    chrom = bestId;
                }
                return true;
            }
        }
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CWiggleWriter::xTableGetPosIn(
    const CSeq_table& table,
    int index,
    int& posIn)
//  ----------------------------------------------------------------------------
{
    const vector<CRef<CSeqTable_column> > columns = table.GetColumns();
    for (size_t u = 0; u < columns.size(); ++u) {
        const CSeqTable_column_info& header = columns[u]->GetHeader();
        if (header.IsSetField_id()) {
            int fieldId = header.GetField_id();
            if (fieldId == CSeqTable_column_info::eField_id_location_from) {
                return (columns[u]->TryGetInt(index, posIn));
            }
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CWiggleWriter::xTableGetPosOut(
    const CSeq_table& table,
    int index,
    int posIn,
    int& posOut)
//  ----------------------------------------------------------------------------
{
    const vector<CRef<CSeqTable_column> > columns = table.GetColumns();
    for (size_t u = 0; u < columns.size(); ++u) {
        const CSeqTable_column_info& header = columns[u]->GetHeader();
        if (header.IsSetField_name()) {
            string fieldName = header.GetField_name();
            if (fieldName == "span") {
                if (!columns[u]->TryGetInt(index, posOut)) {
                    return false;
                }
                posOut += posIn;
                return true;
            }
        }
        if (header.IsSetField_id()) {
            int fieldId = header.GetField_id();
            if (fieldId == CSeqTable_column_info::eField_id_location_to) {
                return (columns[u]->TryGetInt(index, posOut));
            }
        }
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CWiggleWriter::xTableGetValue(
    const CSeq_table& table,
    int index,
    double& value)
//  ----------------------------------------------------------------------------
{
    const vector<CRef<CSeqTable_column> > columns = table.GetColumns();
    for (size_t u = 0; u < columns.size(); ++u) {
        const CSeqTable_column_info& header = columns[u]->GetHeader();
        if (header.IsSetField_name()) {
            string fieldName = header.GetField_name();
            if (fieldName == "values") {
                if (!columns[u]->TryGetReal(index, value)) {
                    int intValue(0);
                    if (!columns[u]->TryGetInt(index, intValue)) {
                        return false;
                    }
                    value = intValue;
                    return true;
                }
                return true;
            }
        }
    }
    return false;
}

END_NCBI_SCOPE
