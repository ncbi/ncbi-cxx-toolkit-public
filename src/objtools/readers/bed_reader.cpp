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
#include <objtools/readers/error_container.hpp>
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
    m_columncount(0),
    m_usescore(false),
    m_currentId("")
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
    IErrorContainer* pErrorContainer ) 
//  ----------------------------------------------------------------------------                
{
    CRef<CSeq_annot> annot;
    CRef<CAnnot_descr> desc;

    annot.Reset(new CSeq_annot);
    desc.Reset(new CAnnot_descr);
    annot->SetDesc(*desc);
    annot->SetData().SetFtable();

    string line;
    int featureCount = 0;

    while (!lr.AtEOF()) {
        ++m_uLineNumber;
        line = *++lr;
        if (NStr::TruncateSpaces(line).empty()) {
            continue;
        }
        try {
            if (x_ParseBrowserLine(line, annot)) {
                continue;
            }
            if (x_ParseTrackLine(line, annot)) {
                if (featureCount > 0) {
                    --m_uLineNumber;
                    lr.UngetLine();
                    break;
                }
                continue;
            }
            if (!x_ParseFeature( line, annot )) {
                --m_uLineNumber;
                lr.UngetLine();
                break;
            }
            ++featureCount;
        }
        catch( CObjReaderLineException& err ) {
            xProcessError( err, pErrorContainer );
        }
        continue;
    }
    //  Only return a valid object if there was at least one feature
    if (0 == featureCount) {
        return CRef<CSeq_annot>();
    }
    x_AddConversionInfo(annot, pErrorContainer);
    return annot;
}

//  --------------------------------------------------------------------------- 
void
CBedReader::ReadSeqAnnots(
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
CBedReader::ReadSeqAnnots(
    vector< CRef<CSeq_annot> >& annots,
    ILineReader& lr,
    IErrorContainer* pErrorContainer )
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_annot> annot = ReadSeqAnnot(lr, pErrorContainer);
    while (annot) {
        annots.push_back(annot);
        annot = ReadSeqAnnot(lr, pErrorContainer);
    }
}
                        
//  ----------------------------------------------------------------------------                
CRef< CSerialObject >
CBedReader::ReadObject(
    ILineReader& lr,
    IErrorContainer* pErrorContainer ) 
//  ----------------------------------------------------------------------------                
{ 
    CRef<CSerialObject> object( 
        ReadSeqAnnot( lr, pErrorContainer ).ReleaseOrNull() );    
    return object;
}

//  ----------------------------------------------------------------------------
bool
CBedReader::xParseTrackLine(
    const string& strLine,
    CRef< CSeq_annot >& current )
//  ----------------------------------------------------------------------------
{
    if ( ! NStr::StartsWith( strLine, "track" ) ) {
        return false;
    }

    m_currentId.clear();
    if ( !m_currentId.empty() ) {
        x_AddConversionInfo( current, &m_ErrorsPrivate );    
        m_columncount = 0;
        m_ErrorsPrivate.ClearAll();
    }
    return CReaderBase::x_ParseTrackLine( strLine, current );
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
bool CBedReader::x_ParseFeature(
    const string& record,
    CRef<CSeq_annot>& annot ) /* throws CObjReaderLineException */
//  ----------------------------------------------------------------------------
{
    const int MAX_RECORDS = 100000;
    static int count = 0;
    count++;

    vector<string> fields;

	string record_copy = record;
	NStr::TruncateSpacesInPlace(record_copy);

	// 'chr 8' fixup.
	if (record_copy.find("chr ") == 0 || 
		record_copy.find("Chr ") == 0 || 
		record_copy.find("CHR ") == 0)
		record_copy.erase(3, 1);

    //  parse
    NStr::Tokenize( record_copy, " \t", fields, NStr::eMergeDelims );
    if (fields.size() != m_columncount) {
        if ( 0 == m_columncount ) {
            m_columncount = fields.size();
        }
        else {
            CObjReaderLineException err(
                eDiag_Error,
                0,
                "Bad data line: Inconsistent column count." );
            throw( err );
        }
    }

    //  if feature tables get too big we _will_ run out of memory. To guard against
    //  that, limit feature tables to a single id, and to MAX_RECORDS at the most.
    if (m_currentId != fields[0]  ||  count == MAX_RECORDS+1) {
        if (m_currentId.empty()) {
            m_currentId = fields[0];
        }
        else {
            m_currentId.clear();
            count = 0;
            return false; //indicate no data has been processed 
        }
    }

    //  assign
    CSeq_annot::C_Data::TFtable& ftable = annot->SetData().SetFtable();
    CRef<CSeq_feat> feature;
    feature.Reset( new CSeq_feat );
    try {
        x_SetFeatureLocation( feature, fields );
        x_SetFeatureDisplayData( feature, fields );
    }
    catch( ... ) {
        m_currentId.clear();
        CObjReaderLineException err(
            eDiag_Error,
            0,
            "Bad data line: General parsing error." );
        throw( err );    
    }
    ftable.push_back( feature );
    return true;
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
            NStr::StringToInt(fields[8], NStr::fConvErr_NoThrow) );
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
    feature->ResetLocation();
    
    CRef<CSeq_id> id = CReadUtil::AsSeqId(fields[0]);

    CRef<CSeq_loc> location( new CSeq_loc );
    int from, to;
    CSeq_interval& interval = location->SetInt();
    try {
		string cleaned_from;
		NStr::Replace(fields[1], ",", "", cleaned_from);
        from = NStr::StringToInt( cleaned_from ) - 1;
    }
    catch ( ... ) {
        CObjReaderLineException err( 
            eDiag_Error,
            0,
            "Invalid data line --- Bad \"SeqStart\" value." );
        throw( err );
    }
    try {
    	string cleaned_to;
		NStr::Replace(fields[2], ",", "", cleaned_to);
        to = NStr::StringToInt( cleaned_to ) - 1;
    }
    catch ( ... ) {
        CObjReaderLineException err( 
            eDiag_Error,
            0,
            "Invalid data line --- Bad \"SeqStop\" value." );
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
            "Invalid data line --- \"SeqStop\" less than \"SeqStart\"." );
        throw( err );
    }

    size_t strand_field = 5;
    if (fields.size() == 5  &&  (fields[4] == "-"  ||  fields[4] == "+")) {
        strand_field = 4;
    }
    if (strand_field < fields.size()) {
        interval.SetStrand(( fields[strand_field] == "+" ) ?
                           eNa_strand_plus : eNa_strand_minus );
    }
    location->SetId( *id );
    
    feature->SetLocation( *location );
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
void
CBedReader::xProcessError(
    CObjReaderLineException& err,
    IErrorContainer* pContainer)
//  ----------------------------------------------------------------------------
{
    err.SetLineNumber(m_uLineNumber);
    m_ErrorsPrivate.PutError(err);
    ProcessError(err, pContainer);
}

END_objects_SCOPE
END_NCBI_SCOPE
