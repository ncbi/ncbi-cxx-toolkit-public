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
 *   BED file reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>              
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/stream_utils.hpp>

#include <util/static_map.hpp>
#include <util/line_reader.hpp>

#include <serial/iterator.hpp>
#include <serial/objistrasn.hpp>

// Objects includes
#include <objects/general/Int_fuzz.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp> 
#include <objects/general/Dbtag.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Feat_id.hpp>

#include <objtools/readers/read_util.hpp>
#include <objtools/readers/reader_exception.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/bed_reader.hpp>
#include <objtools/error_codes.hpp>

#include <algorithm>


#define NCBI_USE_ERRCODE_X   Objtools_Rd_RepMask

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
CBedReader::CBedReader(
    unsigned int flags ) :
//  ----------------------------------------------------------------------------
    CReaderBase(flags),
    m_currentId(""),
    m_columncount(0),
    m_usescore(false),
    m_CurBatchSize(0),
    m_MaxBatchSize(10000)
{
}


//  ----------------------------------------------------------------------------
CBedReader::~CBedReader()
//  ----------------------------------------------------------------------------
{
}

//  ----------------------------------------------------------------------------                
CRef< CSeq_annot >
CBedReader::ReadSeqAnnot(
    ILineReader& lr,
    IMessageListener* pEC ) 
//  ----------------------------------------------------------------------------                
{
    const int MAX_RECORDS = 100000;

    CRef<CSeq_annot> annot;
    CRef<CAnnot_descr> desc;

    annot.Reset(new CSeq_annot);
    desc.Reset(new CAnnot_descr);
    annot->SetDesc(*desc);
    CSeq_annot::C_Data::TFtable& tbl = annot->SetData().SetFtable();

    string line;
    int featureCount = 0;

    while (!lr.AtEOF()) {
        ++m_uLineNumber;
        line = *++lr;
        if (NStr::TruncateSpaces(line).empty()) {
            continue;
        }
        if (xParseComment(line, annot)) {
            continue;
        }
        if (x_ParseBrowserLine(line, annot, pEC)) {
            continue;
        }
        if (NStr::StartsWith(line, "track ")) {
            if (featureCount > 0) {
                --m_uLineNumber;
                lr.UngetLine();
                break;
            }
            else {
                if (xParseTrackLine(line, annot, pEC)) {
                    continue;
                }
            }
        }

	    string record_copy = line;
	    NStr::TruncateSpacesInPlace(record_copy);

        //  parse
        vector<string> fields;
        NStr::Tokenize( record_copy, " \t", fields, NStr::eMergeDelims );
        try {
            xCleanColumnValues(fields);
        }
        catch(CObjReaderLineException& err) {
            ProcessError(err, pEC);
            continue;
        }
        if (fields[0] != m_currentId) {
            //record id has changed
            if (featureCount > 0) {
                --m_uLineNumber;
                lr.UngetLine();
                break;
            }
        }
        if (xParseFeature(fields, annot, pEC)) {
            ++featureCount;
            continue;
        }
        if (tbl.size() >= MAX_RECORDS) {
            break;
        }
    }
    //  Only return a valid object if there was at least one feature
    if (0 == featureCount) {
        return CRef<CSeq_annot>();
    }
    x_AddConversionInfo(annot, pEC);
    x_AssignTrackData( annot );

    if(m_columncount >= 3) {
        CRef<CUser_object> columnCountUser( new CUser_object() );
        columnCountUser->SetType().SetStr( "NCBI_BED_COLUMN_COUNT" );
        columnCountUser->AddField("NCBI_BED_COLUMN_COUNT", int ( m_columncount ) );
    
        CRef<CAnnotdesc> userDesc( new CAnnotdesc() );
        userDesc->SetUser().Assign( *columnCountUser );
        annot->SetDesc().Set().push_back( userDesc );
    }
    return annot;
}

//  --------------------------------------------------------------------------- 
void
CBedReader::ReadSeqAnnots(
    vector< CRef<CSeq_annot> >& annots,
    CNcbiIstream& istr,
    IMessageListener* pMessageListener )
//  ---------------------------------------------------------------------------
{
    CStreamLineReader lr( istr );
    ReadSeqAnnots( annots, lr, pMessageListener );
}
 
//  ---------------------------------------------------------------------------                       
void
CBedReader::ReadSeqAnnots(
    vector< CRef<CSeq_annot> >& annots,
    ILineReader& lr,
    IMessageListener* pMessageListener )
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_annot> annot = ReadSeqAnnot(lr, pMessageListener);
    while (annot) {
        annots.push_back(annot);
        annot = ReadSeqAnnot(lr, pMessageListener);
    }
}
                        
//  ----------------------------------------------------------------------------                
CRef< CSerialObject >
CBedReader::ReadObject(
    ILineReader& lr,
    IMessageListener* pMessageListener ) 
//  ----------------------------------------------------------------------------                
{ 
    CRef<CSerialObject> object( 
        ReadSeqAnnot( lr, pMessageListener ).ReleaseOrNull() );    
    return object;
}

//  ----------------------------------------------------------------------------
bool
CBedReader::xParseTrackLine(
    const string& strLine,
    CRef< CSeq_annot >& current,
    IMessageListener* pEC)
//  ----------------------------------------------------------------------------
{
    if ( ! NStr::StartsWith( strLine, "track" ) ) {
        return false;
    }
    vector<string> parts;
    CReadUtil::Tokenize( strLine, " \t", parts );
    if (parts.size() >= 3) {
        try {
            NStr::StringToInt(parts[1]);
            NStr::StringToInt(parts[2]);
            return false;
        }
        catch(...) {
        }
    }
    if ( !m_currentId.empty() ) {
        m_columncount = 0;
    }
    m_currentId.clear();
    if (!CReaderBase::x_ParseTrackLine(strLine, current, pEC)) {
        CObjReaderLineException err(
            eDiag_Warning,
            0,
            "Bad track line: Expected \"track key1=value1 key2=value2 ...\". Ignored.",
            ILineError::eProblem_BadTrackLine);
        ProcessWarning(err , pEC);    
    }
    return true;
}

//  ----------------------------------------------------------------------------
CRef< CSeq_annot >
CBedReader::x_AppendAnnot(
    vector< CRef< CSeq_annot > >& annots )
//  ----------------------------------------------------------------------------
{
    CRef< CSeq_annot > annot( new CSeq_annot );
    CRef< CAnnot_descr > desc( new CAnnot_descr );
    annot->SetDesc( *desc );
    annots.push_back( annot );
    return annot;
}    
    
//  ----------------------------------------------------------------------------
bool CBedReader::xParseFeature(
    const vector<string>& fields,
    CRef<CSeq_annot>& annot,
    IMessageListener* pEC)
//  ----------------------------------------------------------------------------
{
    static int count = 0;
    count++;

    if (fields.size() != m_columncount) {
        if ( 0 == m_columncount ) {
            m_columncount = fields.size();
        }
        else {
            CObjReaderLineException err(
                eDiag_Error,
                0,
                "Bad data line: Inconsistent column count." );
            ProcessError(err, pEC );
            return false;
        }
    }

    //  assign
    CSeq_annot::C_Data::TFtable& ftable = annot->SetData().SetFtable();
    CRef<CSeq_feat> feature;
    feature.Reset( new CSeq_feat );
    try {
        x_SetFeatureLocation(feature, fields);
        x_SetFeatureDisplayData(feature, fields);
    }
    catch(CObjReaderLineException& err) {
        //m_currentId.clear();
        ProcessError(err, pEC);
        return false;
    }
    ftable.push_back( feature );
    m_currentId = fields[0];
    return true;
}

//  ----------------------------------------------------------------------------
bool CBedReader::xParseComment(
    const string& record,
    CRef<CSeq_annot>& annot ) /* throws CObjReaderLineException */
//  ----------------------------------------------------------------------------
{
    if (NStr::StartsWith(record, "#")) {
        return true;
    }
    return false;
}
 
//  ----------------------------------------------------------------------------
void CBedReader::x_SetFeatureDisplayData(
    CRef<CSeq_feat>& feature,
    const vector<string>& fields )
//  ----------------------------------------------------------------------------
{
    CRef<CUser_object> display_data( new CUser_object );
    display_data->SetType().SetStr( "Display Data" );
    if ( m_columncount >= 4 ) {
        display_data->AddField( "name", fields[3] );
    }
    else {
        display_data->AddField( "name", string("") );
        feature->SetData().SetUser( *display_data );
        return;
    }
    if ( m_columncount >= 5 ) {
        if ( !m_usescore ) {
            display_data->AddField( 
                "score",
                NStr::StringToInt(fields[4], NStr::fConvErr_NoThrow) );
        }
        else {
            display_data->AddField( 
                "greylevel",
               NStr::StringToInt(fields[4], NStr::fConvErr_NoThrow) );
        }
    }
    if ( m_columncount >= 7 ) {
        display_data->AddField( 
            "thickStart",
            NStr::StringToInt(fields[6], NStr::fConvErr_NoThrow) );
    }
    if ( m_columncount >= 8 ) {
        display_data->AddField( 
            "thickEnd",
            NStr::StringToInt(fields[7], NStr::fConvErr_NoThrow) - 1 );
    }
    if ( m_columncount >= 9 ) {
        display_data->AddField( 
            "itemRGB",
            fields[8]);
    }
    if ( m_columncount >= 10 ) {
        display_data->AddField( 
            "blockCount",
            NStr::StringToInt(fields[9], NStr::fConvErr_NoThrow) );
    }
    if ( m_columncount >= 11 ) {
        display_data->AddField( "blockSizes", fields[10] );
    }
    if ( m_columncount >= 12 ) {
        display_data->AddField( "blockStarts", fields[11] );
    }
    feature->SetData().SetUser( *display_data );
}

//  ----------------------------------------------------------------------------
void CBedReader::x_SetFeatureLocation(
    CRef<CSeq_feat>& feature,
    const vector<string>& fields )
//  ----------------------------------------------------------------------------
{
    //
    //  Note:
    //  BED convention for specifying intervals is 0-based, first in, first out.
    //  ASN convention for specifying intervals is 0-based, first in, last in.
    //  Hence, conversion BED->ASN  leaves the first leaves the "from" coordinate
    //  unchanged, and decrements the "to" coordinate by one.
    //

    CRef<CSeq_loc> location(new CSeq_loc);
    int from, to;

    //already established: We got at least three columns
    try {
        from = NStr::StringToInt(fields[1]);
    }
    catch ( ... ) {
        CObjReaderLineException err( 
            eDiag_Error,
            0,
            "Invalid data line: Bad \"SeqStart\" value." );
        throw( err );
    }
    try {
        to = NStr::StringToInt(fields[2]) - 1;
    }
    catch ( ... ) {
        CObjReaderLineException err( 
            eDiag_Error,
            0,
            "Invalid data line: Bad \"SeqStop\" value.");
        throw( err );
    }
    if (from == to) {
        location->SetPnt().SetPoint(from);
    }
    else if (from < to) {
        location->SetInt().SetFrom(from);
        location->SetInt().SetTo(to);
    }
    else {
        CObjReaderLineException err( 
            eDiag_Error,
            0,
            "Invalid data line: \"SeqStop\" less than \"SeqStart\"." );
        throw(err);
    }

    size_t strand_field = 5;
    if (fields.size() == 5  &&  (fields[4] == "-"  ||  fields[4] == "+")) {
        strand_field = 4;
    }
    if (strand_field < fields.size()) {
        string strand = fields[strand_field];
        if (strand != "+"  &&  strand != "-"  &&  strand != ".") {
            CObjReaderLineException err( 
                eDiag_Error,
                0,
                "Invalid data line: Invalid strand character." );
            throw(err);
        }
        location->SetStrand(( fields[strand_field] == "+" ) ?
                           eNa_strand_plus : eNa_strand_minus );
    }
    
    CRef<CSeq_id> id = CReadUtil::AsSeqId(fields[0]);
    location->SetId(*id);
    feature->SetLocation(*location);
}

//  ----------------------------------------------------------------------------
void CBedReader::x_SetTrackData(
    CRef<CSeq_annot>& annot,
    CRef<CUser_object>& trackdata,
    const string& strKey,
    const string& strValue )
//  ----------------------------------------------------------------------------
{
    CAnnot_descr& desc = annot->SetDesc();

    if ( strKey == "useScore" ) {
        m_usescore = ( 1 == NStr::StringToInt( strValue ) );
        trackdata->AddField( strKey, NStr::StringToInt( strValue ) );
        return;
    }
    if ( strKey == "name" ) {
        CRef<CAnnotdesc> name( new CAnnotdesc() );
        name->SetName( strValue );
        desc.Set().push_back( name );
        return;
    }
    if ( strKey == "description" ) {
        CRef<CAnnotdesc> title( new CAnnotdesc() );
        title->SetTitle( strValue );
        desc.Set().push_back( title );
        return;
    }
    if ( strKey == "visibility" ) {
        trackdata->AddField( strKey, NStr::StringToInt( strValue ) );
        return;
    }
    CReaderBase::x_SetTrackData( annot, trackdata, strKey, strValue );
}

//  ----------------------------------------------------------------------------
bool CBedReader::xGetLine(
    ILineReader& lr,
    string& line)
//  ----------------------------------------------------------------------------
{
    while (!lr.AtEOF()) {
        line = *++lr;
        if (!xCommentLine(line)) {
            return true;
        }
    }
	return false;
}

//  ----------------------------------------------------------------------------
inline bool CBedReader::xCommentLine(
    const string& line) const
//  ----------------------------------------------------------------------------
{
    char c = line.data()[0];
    return c == '#' || c == '\0';
}

//  ----------------------------------------------------------------------------
bool 
CBedReader::ReadTrackData(
    ILineReader& lr,
    CRawBedTrack& rawdata,
    IMessageListener* pMessageListener)
//  ----------------------------------------------------------------------------
{
    if (m_CurBatchSize == m_MaxBatchSize) {
        m_CurBatchSize = 0;
        return xReadBedDataRaw(lr, rawdata, pMessageListener);
    }

    string line;
    while (xGetLine(lr, line)) {
        m_CurBatchSize = 0;
        if (line == "browser"  ||  NStr::StartsWith(line, "browser ")) {
            continue;
        }
        if (line == "track"  ||  NStr::StartsWith(line, "track ")) {
            continue;
        }
        //data line
        lr.UngetLine();
        return xReadBedDataRaw(lr, rawdata, pMessageListener);
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool
CBedReader::xReadBedRecordRaw(
    const string& line,
    CRawBedRecord& record,
    IMessageListener* pMessageListener)
//  ----------------------------------------------------------------------------
{
    if (line == "browser"  || NStr::StartsWith(line, "browser ")) {
        return false;
    }
    if (line == "track"  || NStr::StartsWith(line, "track ")) {
        return false;
    }

    vector<string> columns;
	string linecopy = line;
	NStr::TruncateSpacesInPlace(linecopy);

    //  parse
    NStr::Tokenize( linecopy, " \t", columns, NStr::eMergeDelims );
    try {
        xCleanColumnValues(columns);
    }
    catch(CObjReaderLineException& err) {
        ProcessError(err, pMessageListener);
        return false;
    }

    if (columns.size() != m_columncount) {
        if ( 0 == m_columncount ) {
            m_columncount = columns.size();
        }
        else {
            CObjReaderLineException err(
                eDiag_Error,
                0,
                "Bad data line: Inconsistent column count." );
            ProcessError(err, pMessageListener);
            return false;
        }
    }

    //assign columns to record:
    CRef<CSeq_id> id = CReadUtil::AsSeqId(columns[0]);

    unsigned int start;
    try {
        start = NStr::StringToInt(columns[1]);
    }
    catch (...) {
        CObjReaderLineException err(
            eDiag_Error,
            0,
            "Bad data line: Invalid \"SeqStart\" (column 2) value." );
        ProcessError(err, pMessageListener);
        return false;
    }

    unsigned int stop;
    try {
        stop = NStr::StringToInt(columns[2]);
    }
    catch (...) {
        CObjReaderLineException err(
            eDiag_Error,
            0,
            "Bad data line: Invalid \"SeqStop\" (column 3) value." );
        ProcessError(err, pMessageListener);
        return false;
    }

    int score(-1);
    if (m_columncount >= 7  &&  columns[6] != ".") {
        try {
            score = NStr::StringToInt(columns[6]);
        }
        catch (...) {
            CObjReaderLineException err(
                eDiag_Error,
                0,
                "Bad data line: Invalid \"Score\" (column 5) value." );
            ProcessError(err, pMessageListener);
            return false;
        }
    }
    ENa_strand strand = eNa_strand_plus;
    if (m_columncount >= 6) {
        if (columns[5] == "-") {
            strand = eNa_strand_minus;
        }
    }
    record.SetInterval(*id, start, stop, strand);
    if (score >= 0) {
        record.SetScore(score);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool
CBedReader::xReadBedDataRaw(
    ILineReader& lr,
    CRawBedTrack& rawdata,
    IMessageListener* pMessageListener)
//  ----------------------------------------------------------------------------
{
    rawdata.Reset();
    string line;
    while (xGetLine(lr, line)) {
        CRawBedRecord record;
        if (!xReadBedRecordRaw(line, record, pMessageListener)) {
            lr.UngetLine();
            break;
        }
        rawdata.AddRecord(record);
        ++m_CurBatchSize;
        if (m_CurBatchSize == m_MaxBatchSize) {
            return rawdata.HasData();
        }
    }

    return rawdata.HasData();
}

//  ----------------------------------------------------------------------------
void
CBedReader::xCleanColumnValues(
   vector<string>& columns)
//  ----------------------------------------------------------------------------
{
    string fixup;

    if (NStr::EqualNocase(columns[0], "chr")  &&  columns.size() > 1) {
        columns[1] = columns[0] + columns[1];
        columns.erase(columns.begin());
    }
    if (columns.size() < 3) {
        CObjReaderLineException err(
            eDiag_Error,
            0,
            "Bad data line: Insufficient column count." );
        throw(err);
    }

    try {
        NStr::Replace(columns[1], ",", "", fixup);
        columns[1] = fixup;
    }
    catch (...) {
        CObjReaderLineException err(
            eDiag_Error,
            0,
            "Bad data line: Invalid \"SeqStart\" (column 2) value." );
        throw( err );
    }

    try {
        NStr::Replace(columns[2], ",", "", fixup);
        columns[2] = fixup;
    }
    catch (...) {
        CObjReaderLineException err(
            eDiag_Error,
            0,
            "Bad data line: Invalid \"SeqStop\" (column 3) value." );
        throw( err );
    }
}

END_objects_SCOPE
END_NCBI_SCOPE
