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
 * Author:  Frank Ludwig
 *
 * File Description:
 *   MicroArray file reader
 *
 */

#include <ncbi_pch.hpp>
#include <util/line_reader.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

#include <objtools/readers/microarray_reader.hpp>

#include "reader_message_handler.hpp"

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

//  ----------------------------------------------------------------------------
CMicroArrayReader::CMicroArrayReader(
    int flags,
    CReaderListener* pRL)
//  ----------------------------------------------------------------------------
    : CReaderBase(flags, "", "", CReadUtil::AsSeqId, pRL),
      m_currentId(""),
      m_columncount(15),
      m_usescore(false)
{
    m_iFlags |= fReadAsBed;
}

//  ----------------------------------------------------------------------------
CMicroArrayReader::~CMicroArrayReader()
//  ----------------------------------------------------------------------------
{ 
}

//  ----------------------------------------------------------------------------                
CRef< CSeq_annot >
CMicroArrayReader::ReadSeqAnnot(
    ILineReader& lr,
    ILineErrorListener* pEC) 
//  ----------------------------------------------------------------------------                
{
    CRef<CSeq_annot> pAnnot = CReaderBase::ReadSeqAnnot(lr, pEC);
    if (pAnnot) {
        xAssignTrackData(*pAnnot);

        if(m_columncount >= 3) {
            CRef<CUser_object> columnCountUser( new CUser_object() );
            columnCountUser->SetType().SetStr( "NCBI_BED_COLUMN_COUNT" );
            columnCountUser->AddField("NCBI_BED_COLUMN_COUNT", int ( m_columncount ) );
    
            CRef<CAnnotdesc> userDesc( new CAnnotdesc() );
            userDesc->SetUser().Assign( *columnCountUser );
            pAnnot->SetDesc().Set().push_back( userDesc );
        }
    }
    return pAnnot;
}

//  ----------------------------------------------------------------------------
CRef<CSeq_annot>
CMicroArrayReader::xCreateSeqAnnot() 
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_annot> pAnnot = CReaderBase::xCreateSeqAnnot();
    CRef<CAnnot_descr> desc(new CAnnot_descr);
    pAnnot->SetDesc(*desc);
    pAnnot->SetData().SetFtable();
    return pAnnot;
}


//  ----------------------------------------------------------------------------
void
CMicroArrayReader::xProcessData(
    const TReaderData& readerData,
    CSeq_annot& annot) 
//  ----------------------------------------------------------------------------
{
    for (const auto& lineInfo: readerData) {
        const auto& line = lineInfo.mData; 
        if (xParseBrowserLine(line, annot)) {
            return;
        }
        if (xProcessTrackLine(line)) {
            return;
        }
        xProcessFeature(line, annot);
    }
}

//  ----------------------------------------------------------------------------
void
CMicroArrayReader::xGetData(
    ILineReader& lr,
    TReaderData& readerData)
//  ----------------------------------------------------------------------------
{
    const int MAX_RECORDS = 100000;

    readerData.clear();
    if (m_uDataCount == MAX_RECORDS) {
        m_uDataCount = 0;
        m_currentId.clear();
        return;
    }

    string line, head, tail;
    if (!xGetLine( lr, line)) {
        return;
    }
    if (xIsTrackLine(line)) {
        if (!m_currentId.empty()) {
            xUngetLine(lr);
            m_uDataCount = 0;
            m_currentId.clear();
            return;
        }
        else {
            readerData.push_back(TReaderLine{m_uLineNumber, line});
            ++m_uDataCount;
            return;
        } 
    }

    NStr::SplitInTwo(line, "\t", head, tail);
    if (!m_currentId.empty()  &&  head != m_currentId) {
        xUngetLine(lr);
        m_uDataCount = 0;
        m_currentId.clear();
        return;
    }
    readerData.push_back(TReaderLine{m_uLineNumber, line});
    if (m_currentId.empty()) {
        m_currentId = head;
    }
    ++m_uDataCount;
}

//  ----------------------------------------------------------------------------
bool CMicroArrayReader::xProcessFeature(
    const string& line,
    CSeq_annot& annot)
//  ----------------------------------------------------------------------------
{
    const size_t COLUMNCOUNT = 15;

    vector<string> fields;
    NStr::Split(line, " \t", fields, NStr::fSplit_MergeDelimiters);
    xCleanColumnValues(fields);
    if (fields.size() != COLUMNCOUNT) {
        CReaderMessage error(
            eDiag_Error,
            m_uLineNumber,
            "Feature Processing: Bad column count. Should be 15." );
        throw(error);
    }

    CRef<CSeq_feat> feature;
    feature.Reset(new CSeq_feat);
    xSetFeatureLocation(feature, fields);
    xSetFeatureDisplayData(feature, fields);
    annot.SetData().SetFtable().push_back(feature);
    return true;
}

//  ----------------------------------------------------------------------------
void CMicroArrayReader::xSetFeatureLocation(
    CRef<CSeq_feat>& feature,
    const vector<string>& fields )
//  ----------------------------------------------------------------------------
{
    feature->ResetLocation();
    
    CRef<CSeq_id> id( new CSeq_id() );
    id->SetLocal().SetStr( fields[0] );

    CRef<CSeq_loc> location( new CSeq_loc );
    CSeq_interval& interval = location->SetInt();
    interval.SetFrom( NStr::StringToInt( fields[1] ) );
    interval.SetTo( NStr::StringToInt( fields[2] ) - 1 );
    interval.SetStrand( 
        ( fields[5] == "+" ) ? eNa_strand_plus : eNa_strand_minus );
    location->SetId( *id );
    
    feature->SetLocation( *location );
}

//  ----------------------------------------------------------------------------
void CMicroArrayReader::xSetFeatureDisplayData(
    CRef<CSeq_feat>& feature,
    const vector<string>& fields )
//  ----------------------------------------------------------------------------
{
    CRef<CUser_object> display_data( new CUser_object );
    display_data->SetType().SetStr( "Display Data" );
    
    display_data->AddField( "name", fields[3] );
    if ( !m_usescore ) {
        display_data->AddField( "score", NStr::StringToInt(fields[4]) );
    }
    else {
        display_data->AddField( "greylevel", NStr::StringToInt(fields[4]) );
    }
    display_data->AddField( "thickStart", NStr::StringToInt(fields[6]) );
    display_data->AddField( "thickEnd", NStr::StringToInt(fields[7]) - 1 );
    display_data->AddField( "itemRGB", NStr::StringToInt(fields[8]) );
    display_data->AddField( "blockCount", NStr::StringToInt(fields[9]) );
    display_data->AddField( "blockSizes", fields[10] );
    display_data->AddField( "blockStarts", fields[11] );

    if ( !(m_iFlags & fReadAsBed) ) {
        if ( fields.size() >= 13 ) {
            display_data->AddField( "expCount", NStr::StringToInt(fields[12]) );
        }
        if ( fields.size() >= 14 ) {
            display_data->AddField( "expIds", fields[13] );
        }
        if ( fields.size() >= 15 ) {
            display_data->AddField( "expStep", NStr::StringToInt(fields[14]) );
        }
    }

    feature->SetData().SetUser( *display_data );
}

//  ----------------------------------------------------------------------------
bool CMicroArrayReader::xProcessTrackLine(
    const string& strLine)
//  ----------------------------------------------------------------------------
{
    m_strExpNames = "";
    m_iExpScale = -1;
    m_iExpStep = -1;
    
    if (!CReaderBase::xParseTrackLine(strLine)) {
        return false;
    }
    if ( m_iFlags & fReadAsBed ) {
        return true;
    }
    
    if ( m_strExpNames.empty() ) {
        CReaderMessage error(
            eDiag_Warning,
            m_uLineNumber,
            "Track Line Processing: Missing \"expName\" parameter.");
        m_pMessageHandler->Report(error);
    }
    if ( m_iExpScale == -1 ) {
         CReaderMessage error(
            eDiag_Warning,
            m_uLineNumber,
            "Track Line Processing: Missing \"expScale\" parameter." );
        m_pMessageHandler->Report(error);
    }
    if ( m_iExpStep == -1 ) {
         CReaderMessage error(
            eDiag_Warning,
            m_uLineNumber,
            "Track Line Processing: Missing \"expStep\" parameter." );
        m_pMessageHandler->Report(error);
    }
    
    return true;
}

//  ----------------------------------------------------------------------------
void
CMicroArrayReader::xCleanColumnValues(
   vector<string>& columns)
//  ----------------------------------------------------------------------------
{
    string fixup;
    auto columnCount = columns.size();

    if (columnCount <= 1) {
        return;
    }
    if (NStr::EqualNocase(columns[0], "chr")) {
        columns[1] = columns[0] + columns[1];
        columns.erase(columns.begin());
    }

    if (columnCount <= 2) {
        return;
    }
    try {
        NStr::Replace(columns[1], ",", "", fixup);
        columns[1] = fixup;
    }
    catch (CException&) {
        CReaderMessage error(
            eDiag_Error,
            0,
            "Bad data line: Invalid \"SeqStart\" (column 2) value." );
        throw(error);
    }

    if (columnCount <= 3) {
        return;
    }
    try {
        NStr::Replace(columns[2], ",", "", fixup);
        columns[2] = fixup;
    }
    catch (CException&) {
        CReaderMessage error(
            eDiag_Error,
            0,
            "Bad data line: Invalid \"SeqStop\" (column 3) value." );
        throw(error);
    }
}

END_objects_SCOPE
END_NCBI_SCOPE
