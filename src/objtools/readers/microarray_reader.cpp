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

#include <objtools/readers/reader_exception.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/error_container.hpp>
#include <objtools/readers/reader_base.hpp>
#include <objtools/readers/microarray_reader.hpp>
#include <objtools/error_codes.hpp>

#include <algorithm>


#define NCBI_USE_ERRCODE_X   Objtools_Rd_RepMask

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
CMicroArrayReader::CMicroArrayReader(
    int flags )
//  ----------------------------------------------------------------------------
    : CReaderBase(flags), m_usescore(false)
{
}

//  ----------------------------------------------------------------------------
CMicroArrayReader::~CMicroArrayReader()
//  ----------------------------------------------------------------------------
{ 
}

//  ----------------------------------------------------------------------------                
CRef< CSerialObject >
CMicroArrayReader::ReadObject(
    ILineReader& lr,
    IErrorContainer* pErrorContainer ) 
//  ----------------------------------------------------------------------------                
{ 
    CRef<CSerialObject> object( 
        ReadSeqAnnot( lr, pErrorContainer ).ReleaseOrNull() );
    return object;
}
    
//  ----------------------------------------------------------------------------                
CRef< CSeq_annot >
CMicroArrayReader::ReadSeqAnnot(
    ILineReader& lr,
    IErrorContainer* pErrorContainer ) 
//  ----------------------------------------------------------------------------                
{
    CRef< CSeq_annot > annot( new CSeq_annot );
    CRef< CAnnot_descr > desc( new CAnnot_descr );
    annot->SetDesc( *desc );

    string line;
    int linecount = 0;

    while ( ! lr.AtEOF() ) {
        ++linecount;
        line = *++lr;
        if ( NStr::TruncateSpaces( line ).empty() ) {
            continue;
        }
        try {
            if ( x_ParseBrowserLine( line, annot ) ) {
                continue;
            }
            if ( x_ParseTrackLine( line, annot ) ) {
                continue;
            }
            x_ParseFeature( line, annot );
        }
        catch( CObjReaderLineException& err ) {
            err.SetLineNumber( linecount );
            ProcessError( err, pErrorContainer );
        }
        continue;
    }
    x_AddConversionInfo( annot, pErrorContainer );
    return annot;
}

//  ----------------------------------------------------------------------------
void CMicroArrayReader::x_ParseFeature(
    const string& record,
    CRef<CSeq_annot>& annot )
//  ----------------------------------------------------------------------------
{
    const size_t columncount = 15;
    CRef<CSeq_feat> feature;

    //  parse
    vector<string> fields;
    NStr::Tokenize( record, " \t", fields, NStr::eMergeDelims );
    if (fields.size() != columncount) {
            CObjReaderLineException err(
                eDiag_Error,
                0,
                "Feature Processing: Bad column count. Should be 15." );
            throw( err );
    }

    //  assign
    feature.Reset( new CSeq_feat );
    try {
        x_SetFeatureLocation( feature, fields );
        x_SetFeatureDisplayData( feature, fields );
    }
    catch (...) {
        CObjReaderLineException err(
            eDiag_Error,
            0,
            "Feature Processing: General Parse Error." );
        throw( err );
    }
    annot->SetData().SetFtable().push_back( feature );
}

//  ----------------------------------------------------------------------------
void CMicroArrayReader::x_SetFeatureLocation(
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
void CMicroArrayReader::x_SetFeatureDisplayData(
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
bool CMicroArrayReader::x_ParseTrackLine(
    const string& strLine,
    CRef<CSeq_annot>& annot )
//  ----------------------------------------------------------------------------
{
    m_strExpNames = "";
    m_iExpScale = -1;
    m_iExpStep = -1;
    
    if ( ! CReaderBase::x_ParseTrackLine( strLine, annot ) ) {
        return false;
    }
    if ( m_iFlags & fReadAsBed ) {
        return true;
    }
    
    if ( m_strExpNames.empty() ) {
        CObjReaderLineException err(
            eDiag_Warning,
            0,
            "Track Line Processing: Missing \"expName\" parameter." );
        throw( err );
    }
    if ( m_iExpScale == -1 ) {
        CObjReaderLineException err(
            eDiag_Warning,
            0,
            "Track Line Processing: Missing \"expScale\" parameter." );
        throw( err );
    }
    if ( m_iExpStep == -1 ) {
        CObjReaderLineException err(
            eDiag_Warning,
            0,
            "Track Line Processing: Missing \"expStep\" parameter." );
        throw( err );
    }
    
    return true;
}
//  ----------------------------------------------------------------------------
void CMicroArrayReader::x_SetTrackData(
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
    if ( strKey == "expNames" ) {
        trackdata->AddField( strKey, strValue );
        m_strExpNames = strValue;
        return;
    }
    if ( strKey == "expScale" ) {
        trackdata->AddField( strKey, NStr::StringToInt( strValue ) );
        m_iExpScale = NStr::StringToInt( strValue );
        return;
    }
    if ( strKey == "expStep" ) {
        trackdata->AddField( strKey, NStr::StringToInt( strValue ) );
        m_iExpStep = NStr::StringToInt( strValue );
        return;
    }
    CReaderBase::x_SetTrackData( annot, trackdata, strKey, strValue );
}

END_objects_SCOPE
END_NCBI_SCOPE
