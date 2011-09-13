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

#include "reader_data.hpp"

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
    case CFormatGuess::eGff3:
        return new CGff3Reader( flags );
    }
}

//  ----------------------------------------------------------------------------
CReaderBase::CReaderBase(
    int flags) :
//  ----------------------------------------------------------------------------
    m_iFlags(flags)
{
    m_pTrackDefaults = new CTrackData;
}

//  ----------------------------------------------------------------------------
CReaderBase::~CReaderBase()
//  ----------------------------------------------------------------------------
{
    delete m_pTrackDefaults;
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
void CReaderBase::x_AssignTrackData(
    CRef<CSeq_annot>& annot )
//  ----------------------------------------------------------------------------
{
    CAnnot_descr& desc = annot->SetDesc();

    CRef<CUser_object> trackdata( new CUser_object() );
    trackdata->SetType().SetStr( "Track Data" );   
    if ( !m_pTrackDefaults->Description().empty() ) {
        trackdata->AddField( "description", m_pTrackDefaults->Description() );
        annot->SetTitle(m_pTrackDefaults->Description());
    }
    if ( !m_pTrackDefaults->Name().empty() ) {
        annot->AddName(m_pTrackDefaults->Name());
        trackdata->AddField( "name", m_pTrackDefaults->Name() );
    }
    map<string,string>::const_iterator cit = m_pTrackDefaults->Values().begin();
    while ( cit != m_pTrackDefaults->Values().end() ) {
        trackdata->AddField( cit->first, cit->second );
        ++cit;
    }
    if ( trackdata->CanGetData() && ! trackdata->GetData().empty() ) {
        CRef<CAnnotdesc> user( new CAnnotdesc() );
        user->SetUser( *trackdata );
        desc.Set().push_back( user );
    }
}

//  ----------------------------------------------------------------------------
bool CReaderBase::x_ParseTrackLine(
    const string& strLine,
    CRef<CSeq_annot>& annot )
//  ----------------------------------------------------------------------------
{
    vector<string> parts;
    Tokenize( strLine, " \t", parts );
    if ( !CTrackData::IsTrackData( parts ) ) {
        return false;
    }
    m_pTrackDefaults->ParseLine( parts );
    x_AssignTrackData( annot );
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
    CRef<CAnnotdesc> user( new CAnnotdesc() );
    user->SetUser( *x_MakeAsnConversionInfo( pErrorContainer ) );
    annot->SetDesc().Set().push_back( user );
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
    CRef<CSeqdesc> user( new CSeqdesc() );
    user->SetUser( *x_MakeAsnConversionInfo( pErrorContainer ) );
    entry->SetDescr().Set().push_back( 
        user );
}

//  ----------------------------------------------------------------------------
CRef<CUser_object> CReaderBase::x_MakeAsnConversionInfo(
    IErrorContainer* pErrorContainer )
//  ----------------------------------------------------------------------------
{
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
    return conversioninfo;
}

//  ----------------------------------------------------------------------------
void CReaderBase::Tokenize(
    const string& str,
    const string& delim,
    vector< string >& parts )
//  ----------------------------------------------------------------------------
{
    string temp;
    bool in_quote( false );
    const char joiner( '#' );

    for ( size_t i=0; i < str.size(); ++i ) {
        switch( str[i] ) {

            default:
                break;
            case '\"':
                in_quote = in_quote ^ true;
                break;

            case ' ':
                if ( in_quote ) {
                    if ( temp.empty() )
                        temp = str;
                    temp[i] = joiner;
                }
                break;
        }
    }
    if ( temp.empty() ) {
        NStr::Tokenize(str, delim, parts, NStr::eMergeDelims);
    }
    else {
        NStr::Tokenize(temp, delim, parts, NStr::eMergeDelims);
        for ( size_t j=0; j < parts.size(); ++j ) {
            for ( size_t i=0; i < parts[j].size(); ++i ) {
                if ( parts[j][i] == joiner ) {
                    parts[j][i] = ' ';
                }
            }
        }
    }
}

//  ----------------------------------------------------------------------------
void 
CReaderBase::x_Warn(
    const string& message, 
    unsigned int line)
//  ----------------------------------------------------------------------------
{
    if (line) {
        ERR_POST_X(2, Warning << message << " [GFF input, line " << line << ']');
    } else {
        ERR_POST_X(3, Warning << message << " [GFF input]");
    }
}

//  ----------------------------------------------------------------------------
void 
CReaderBase::x_Error(
    const string& message, 
    unsigned int line)
//  ----------------------------------------------------------------------------
{
    if (line) {
        ERR_POST_X(2, Error << message << " [GFF input, line " << line << ']');
    } else {
        ERR_POST_X(3, Error << message << " [GFF input]");
    }
}

//  ----------------------------------------------------------------------------
void 
CReaderBase::x_Info(
    const string& message, 
    unsigned int line)
//  ----------------------------------------------------------------------------
{
    if (line) {
        ERR_POST_X(2, Info << message << " [GFF input, line " << line << ']');
    } else {
        ERR_POST_X(3, Info << message << " [GFF input]");
    }
}



END_objects_SCOPE
END_NCBI_SCOPE
