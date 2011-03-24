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
 *   Basic reader interface.
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
#include <objects/seq/Seq_descr.hpp>
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
#include <objtools/readers/bed_reader.hpp>
#include <objtools/readers/microarray_reader.hpp>
#include <objtools/readers/wiggle_reader.hpp>
#include <objtools/readers/gff3_reader.hpp>
#include <objtools/error_codes.hpp>

#include <algorithm>


#define NCBI_USE_ERRCODE_X   Objtools_Rd_RepMask

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
CReaderBase*
CReaderBase::GetReader(
    CFormatGuess::EFormat format,
    int flags )
//  ----------------------------------------------------------------------------
{
    switch ( format ) {
    default:
        return 0;
    case CFormatGuess::eBed:
        return new CBedReader( flags );
    case CFormatGuess::eBed15:
        return new CMicroArrayReader( flags );
    case CFormatGuess::eWiggle:
        return new CWiggleReader( flags );
    case CFormatGuess::eGtf:
        return new CGff3Reader( flags );
    }
}

//  ----------------------------------------------------------------------------
CRef< CSerialObject >
CReaderBase::ReadObject(
    CNcbiIstream& istr,
    IErrorContainer* pErrorContainer ) 
//  ----------------------------------------------------------------------------
{
    CStreamLineReader lr( istr );
    return ReadObject( lr, pErrorContainer );
}

//  ----------------------------------------------------------------------------
CRef< CSeq_annot >
CReaderBase::ReadSeqAnnot(
    CNcbiIstream& istr,
    IErrorContainer* pErrorContainer ) 
//  ----------------------------------------------------------------------------
{
    CStreamLineReader lr( istr );
    return ReadSeqAnnot( lr, pErrorContainer );
}

//  ----------------------------------------------------------------------------
CRef< CSeq_annot >
CReaderBase::ReadSeqAnnot(
    ILineReader&,
    IErrorContainer* ) 
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_annot> object( new CSeq_annot() );
    return object;
}
                
//  ----------------------------------------------------------------------------
CRef< CSeq_entry >
CReaderBase::ReadSeqEntry(
    CNcbiIstream& istr,
    IErrorContainer* pErrorContainer ) 
//  ----------------------------------------------------------------------------
{
    CStreamLineReader lr( istr );
    return ReadSeqEntry( lr, pErrorContainer );
}

//  ----------------------------------------------------------------------------
CRef< CSeq_entry >
CReaderBase::ReadSeqEntry(
    ILineReader&,
    IErrorContainer* ) 
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_entry> object( new CSeq_entry() );
    return object;
}
                
//  ----------------------------------------------------------------------------
bool CReaderBase::SplitLines( 
    const char* pcBuffer, 
    size_t uSize,
    list<string>& lines )
//  ----------------------------------------------------------------------------
{
    //
    //  Make sure the given data is ASCII before checking potential line breaks:
    //
    const size_t MIN_HIGH_RATIO = 20;
    size_t high_count = 0;
    for ( size_t i=0; i < uSize; ++i ) {
        if ( 0x80 & pcBuffer[i] ) {
            ++high_count;
        }
    }
    if ( 0 < high_count && uSize / high_count < MIN_HIGH_RATIO ) {
        return false;
    }

    //
    //  Let's expect at least one line break in the given data:
    //
    string data( pcBuffer, uSize );
    
    lines.clear();
    if ( NStr::Split( data, "\r\n", lines ).size() > 1 ) {
        return true;
    }
    lines.clear();
    if ( NStr::Split( data, "\r", lines ).size() > 1 ) {
        return true;
    }
    lines.clear();
    if ( NStr::Split( data, "\n", lines ).size() > 1 ) {
        return true;
    }
    
    //
    //  Suspicious for non-binary files. Unfortunately, it can actually happen
    //  for Newick tree files.
    //
    lines.clear();
    lines.push_back( data );
    return true;
}

//  ----------------------------------------------------------------------------
void
CReaderBase::ProcessError(
    CObjReaderLineException& err,
    IErrorContainer* pContainer )
//  ----------------------------------------------------------------------------
{
    err.SetLineNumber( m_uLineNumber );
    if ( 0 == pContainer ) {
        throw( err );
    }
    if ( ! pContainer->PutError( err ) )
    {
        throw( err );
    }
}

//  ----------------------------------------------------------------------------
void CReaderBase::x_SetBrowserRegion(
    const string& strRaw,
    CAnnot_descr& desc )
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_loc> location( new CSeq_loc );
    CSeq_interval& interval = location->SetInt();

    string strChrom;
    string strInterval;
    if ( ! NStr::SplitInTwo( strRaw, ":", strChrom, strInterval ) ) {
        CObjReaderLineException err(
            eDiag_Error,
            0,
            "Bad browser: cannot parse browser position" );
        throw( err );
    }
    CRef<CSeq_id> id( new CSeq_id( CSeq_id::e_Local, strChrom ) );
    location->SetId( *id );

    string strFrom;
    string strTo;
    if ( ! NStr::SplitInTwo( strInterval, "-", strFrom, strTo ) ) {
        CObjReaderLineException err(
            eDiag_Error,
            0,
            "Bad browser: cannot parse browser position" );
        throw( err );
    }    
    interval.SetFrom( NStr::StringToInt( strFrom ) - 1);
    interval.SetTo( NStr::StringToInt( strTo ) - 1 );
    interval.SetStrand( eNa_strand_unknown );

    CRef<CAnnotdesc> region( new CAnnotdesc() );
    region->SetRegion( *location );
    desc.Set().push_back( region );
}
    
//  ----------------------------------------------------------------------------
void CReaderBase::x_GetTrackValues(
    const string& strLine,
    map<string, string>& values )
//  ----------------------------------------------------------------------------
{
    string strTemp( strLine );
    
    //
    //  Hide blanks inside of string literals
    //
    bool bInString = false;
    for ( size_t u=0; u < strTemp.size(); ++u ) {
        switch ( strTemp[u] ) {
        default:
            break;
        case '\"':
            bInString = !bInString;
            break;
        case ' ':
            if ( bInString ) {
                strTemp[u] = '\"';
            }
            break;
        }
    }
    vector<string> fields;
    NStr::Tokenize( strTemp, " \t", fields, NStr::eMergeDelims );
    for ( vector<string>::size_type i=1; i < fields.size(); ++i ) {
        vector<string> splits;
        NStr::Tokenize( fields[i], "=", splits, NStr::eMergeDelims );        
        if ( splits.size() != 2 ) {
            CObjReaderLineException err(
                eDiag_Warning,
                0,
                "Bad track line: key=value pair expected" );
            throw( err );
        }
        string strKey = splits[0];
        //  Restore the hidden blanks, remove quotes     
        string strValue = splits[1];
        NStr::ReplaceInPlace( strValue, "\"", " " );
        NStr::TruncateSpacesInPlace( strValue );
        values[ strKey ] = strValue;
    }
}

//  ----------------------------------------------------------------------------
bool CReaderBase::x_ParseBrowserLine(
    const string& strLine,
    CRef<CSeq_annot>& annot )
//  ----------------------------------------------------------------------------
{
    if ( ! NStr::StartsWith( strLine, "browser" ) ) {
        return false;
    }
    CAnnot_descr& desc = annot->SetDesc();
    
    vector<string> fields;
    NStr::Tokenize( strLine, " \t", fields, NStr::eMergeDelims );
    for ( vector<string>::iterator it = fields.begin(); it != fields.end(); ++it ) {
        if ( *it == "position" ) {
            ++it;
            if ( it == fields.end() ) {
                CObjReaderLineException err(
                    eDiag_Error,
                    0,
                    "Bad browser line: incomplete position directive" );
                throw( err );
            }
            x_SetBrowserRegion( *it, desc );
            continue;
        }
    }

    return true;
}


//  ----------------------------------------------------------------------------
bool CReaderBase::x_IsTrackLine(
    const string& strLine )
//  ----------------------------------------------------------------------------
{
    return ( NStr::StartsWith( strLine, "track" ) );
}


//  ----------------------------------------------------------------------------
bool CReaderBase::x_ParseTrackLine(
    const string& strLine,
    CRef<CSeq_annot>& annot )
//  ----------------------------------------------------------------------------
{
    if ( ! x_IsTrackLine( strLine ) ) {
        return false;
    }
    CAnnot_descr& desc = annot->SetDesc();

    CRef<CUser_object> trackdata( new CUser_object() );
    trackdata->SetType().SetStr( "Track Data" );    
    
    map<string, string> values;
    x_GetTrackValues( strLine, values );
    for ( map<string,string>::iterator it = values.begin(); it != values.end(); ++it ) {
        x_SetTrackData( annot, trackdata, it->first, it->second );
    }
    if ( trackdata->CanGetData() && ! trackdata->GetData().empty() ) {
        CRef<CAnnotdesc> user( new CAnnotdesc() );
        user->SetUser( *trackdata );
        desc.Set().push_back( user );
    }
    return true;
}

//  ----------------------------------------------------------------------------
void CReaderBase::x_SetTrackData(
    CRef<CSeq_annot>& annot,
    CRef<CUser_object>& trackdata,
    const string& strKey,
    const string& strValue )
//  ----------------------------------------------------------------------------
{
    trackdata->AddField( strKey, strValue );
}

//  ----------------------------------------------------------------------------
void CReaderBase::x_AddConversionInfo(
    CRef<CSeq_annot >& annot,
    IErrorContainer *pErrorContainer )
//  ----------------------------------------------------------------------------
{
    if ( !annot || !pErrorContainer ) {
        return;
    }
    CAnnot_descr& desc = annot->SetDesc();

    CRef<CUser_object> conversioninfo( new CUser_object() );
    conversioninfo->SetType().SetStr( "Conversion Info" );    
    conversioninfo->AddField( 
        "critical errors", int ( pErrorContainer->LevelCount( eDiag_Critical ) ) );
    conversioninfo->AddField( 
        "errors", int ( pErrorContainer->LevelCount( eDiag_Error ) ) );
    conversioninfo->AddField( 
        "warnings", int ( pErrorContainer->LevelCount( eDiag_Warning ) ) );
    conversioninfo->AddField( 
        "notes", int ( pErrorContainer->LevelCount( eDiag_Info ) ) );
    CRef<CAnnotdesc> user( new CAnnotdesc() );
    user->SetUser( *conversioninfo );
    desc.Set().push_back( user );
}

//  ----------------------------------------------------------------------------
void CReaderBase::x_AddConversionInfo(
    CRef<CSeq_entry >& entry,
    IErrorContainer *pErrorContainer )
//  ----------------------------------------------------------------------------
{
    if ( !entry || !pErrorContainer ) {
        return;
    }
    CSeq_descr& descr = entry->SetDescr();

    CRef<CUser_object> conversioninfo( new CUser_object() );
    conversioninfo->SetType().SetStr( "Conversion Info" );    
    conversioninfo->AddField( 
        "critical errors", int ( pErrorContainer->LevelCount( eDiag_Critical ) ) );
    conversioninfo->AddField( 
        "errors", int ( pErrorContainer->LevelCount( eDiag_Error ) ) );
    conversioninfo->AddField( 
        "warnings", int ( pErrorContainer->LevelCount( eDiag_Warning ) ) );
    conversioninfo->AddField( 
        "notes", int ( pErrorContainer->LevelCount( eDiag_Info ) ) );
    CRef<CSeqdesc> user( new CSeqdesc() );
    user->SetUser( *conversioninfo );
    descr.Set().push_back( user );
}

END_objects_SCOPE
END_NCBI_SCOPE
