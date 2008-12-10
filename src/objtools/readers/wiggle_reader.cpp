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
 *   WIGGLE file reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/stream_utils.hpp>

#include <util/static_map.hpp>

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
#include <objtools/readers/reader_base.hpp>
#include <objtools/readers/wiggle_reader.hpp>
#include <objtools/error_codes.hpp>

#include <algorithm>
#include <objects/seqres/Seq_graph.hpp>
#include <objects/seqres/Real_graph.hpp>

#define NCBI_USE_ERRCODE_X   Objtools_Rd_RepMask

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
CWiggleReader::CWiggleReader(
    int flags )
//  ----------------------------------------------------------------------------
{
}


//  ----------------------------------------------------------------------------
CWiggleReader::~CWiggleReader()
//  ----------------------------------------------------------------------------
{
}

//  ----------------------------------------------------------------------------
bool CWiggleReader::VerifyFormat(
    CNcbiIstream& is )
//  ----------------------------------------------------------------------------
{
    bool verify = false;
    return verify;
}

//  ----------------------------------------------------------------------------
void CWiggleReader::Read( 
    CNcbiIstream& input, 
    CRef<CSeq_annot>& annot )
//
//  Note:   Expecting a sequence of: <trackline> <graph_data>*
//  ----------------------------------------------------------------------------
{
    string pending;
    unsigned int count = 0;

    CSeq_annot::TData::TGraph& graph_set = annot->SetData().SetGraph();
    x_ReadLine( input, pending );
    while ( !input.eof() ) {
        if ( ! x_ReadTrackData( input, pending ) ) {
            return;
        }
        while ( x_ReadGraphData( input, pending ) ) {
            x_AddGraph( graph_set );
        }
    }
}

//  ----------------------------------------------------------------------------
bool CWiggleReader::x_ReadTrackData(
    CNcbiIstream& input,
    string& pending )
//
//  Note:   Expecting a line that starts with "track". With comments already
//          weeded out coming in, everything else triggers an error.
//  ----------------------------------------------------------------------------
{
    m_trackdata.Reset();
    vector<string> parts;
    Tokenize( pending, " \t", parts );
    if ( ! m_trackdata.ParseData( parts ) ) {
        return false;
    }
    return x_ReadLine( input, pending );
}

//  ----------------------------------------------------------------------------
bool CWiggleReader::x_ReadGraphData(
    CNcbiIstream& input,
    string& pending )
//
//  Note:   Several possibilities here the pending line:
//          (1) The line is a "variableStep" declaration. Such a declaration
//              initializes some data but does not represent a complete graph.
//              The following lines then in turn give the missing pieces and
//              complete the record.
//          (2) The line is a "fixedStep" declaration. Similar to (1).
//          (3) BED data. Self contained graph data.
//          (4) "variableStep" data. Completes graph record that was started in
//              the last "variableStep" declaration.
//          (5) "fixedStep" data. Completes graph record that was started in the
//              last "fixedStep" declaration.
//  ----------------------------------------------------------------------------
{
    if ( input.eof() ) {
        return false;
    }
    vector<string> parts;
    Tokenize( pending, " \t", parts );
    switch ( x_GetLineType( pending ) ) {

        default:
            return false;

        case TYPE_DECLARATION_VARSTEP:
            m_graphdata.ParseDeclarationVarstep( parts );
            x_ReadLine( input, pending );
            return x_ReadGraphData( input, pending );

        case TYPE_DECLARATION_FIXEDSTEP:
            m_graphdata.ParseDeclarationFixedstep( parts );
            x_ReadLine( input, pending );
            return x_ReadGraphData( input, pending );

        case TYPE_DATA_BED:
            if ( ! m_graphdata.ParseDataBed( parts ) ) {
                return false;
            }
            break;

        case TYPE_DATA_VARSTEP:
            if ( ! m_graphdata.ParseDataVarstep( parts ) ) {
                return false;
            }
            break;

        case TYPE_DATA_FIXEDSTEP:
            if ( ! m_graphdata.ParseDataFixedstep( parts ) ) {
                return false;
            }
            break;
    }
    x_ReadLine( input, pending );
    return true;
}

//  ----------------------------------------------------------------------------
bool CWiggleReader::x_ReadLine(
    CNcbiIstream& input,
    string& line )
//  ----------------------------------------------------------------------------
{
    while ( ! input.eof() ) {
        NcbiGetlineEOL( input, line );
        NStr::TruncateSpacesInPlace( line );
        if ( ! x_IsCommentLine( line ) ) {
            return true;
        }
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CWiggleReader::x_IsCommentLine(
    const string& line )
//  ----------------------------------------------------------------------------
{
    if ( line.empty() ) {
        return true;
    }
    if ( NStr::StartsWith( line, "browser" ) ) {
        return true;
    }
    if ( NStr::StartsWith( line, "#" ) ) {
        return true;
    }
    return false;
}

//  ----------------------------------------------------------------------------
unsigned int CWiggleReader::x_GetLineType(
    const string& line )
//  ----------------------------------------------------------------------------
{
    vector<string> parts;
    Tokenize( line, " \t", parts );
    if ( parts.empty() ) {
        return TYPE_NONE;
    }
    if ( parts[0] == "track" ) {
        return TYPE_TRACK;
    }
    if ( parts[0] == "variableStep" ) {
        return TYPE_DECLARATION_VARSTEP;
    }
    if ( parts[0] == "fixedStep" ) {
        return TYPE_DECLARATION_FIXEDSTEP;
    }
    if ( parts.size() == 4 ) {
        return TYPE_DATA_BED;
    }
    if ( parts.size() == 2 ) {
        return TYPE_DATA_VARSTEP;
    }
    if ( parts.size() == 1 ) {
        return TYPE_DATA_FIXEDSTEP;
    }
    return TYPE_NONE;
}


//  ----------------------------------------------------------------------------
void CWiggleReader::x_AddGraph(
    CSeq_annot::TData::TGraph& graph_set )
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_id> id( new CSeq_id( CSeq_id::e_Local, m_graphdata.Chrom()) );
    CRef<CSeq_graph> graph( new CSeq_graph() );
    graph->SetNumval( m_graphdata.Span() );
    CReal_graph& data = graph->SetGraph().SetReal();
    CSeq_interval& loc = graph->SetLoc().SetInt();
    loc.SetFrom( 0 );
    loc.SetTo( 1000 );
    loc.SetId( *id );

    vector<double> values( m_graphdata.Span(), m_graphdata.Value() );
    data.SetMin( m_graphdata.Start() );
    data.SetMax( m_graphdata.Start()+ m_graphdata.Span() );
    data.SetAxis( 0 );
    data.SetValues() = values;
    graph_set.push_back( graph );
}

//  ----------------------------------------------------------------------------
void CWiggleReader::Tokenize(
    const string& str,
    const string& delims,
    vector< string >& parts )
//  ----------------------------------------------------------------------------
{
    string temp( str );
    bool in_quote( false );
    const char joiner( '#' );

    for ( size_t i=0; i < temp.size(); ++i ) {
        switch( temp[i] ) {

            default:
                break;
            case '\"':
                in_quote = in_quote ^ true;
                break;

            case ' ':
                if ( in_quote ) {
                    temp[i] = joiner;
                }
                break;
        }
    }
    NStr::Tokenize( temp, delims, parts, NStr::eMergeDelims );
    for ( size_t j=0; j < parts.size(); ++j ) {
        for ( size_t i=0; i < parts[j].size(); ++i ) {
            if ( parts[j][i] == joiner ) {
                parts[j][i] = ' ';
            }
        }
    }
}

//  ----------------------------------------------------------------------------
CWiggleTrackData::CWiggleTrackData()
//  ----------------------------------------------------------------------------
{
    Reset();
}

//  ----------------------------------------------------------------------------
CWiggleTrackData::~CWiggleTrackData()
//  ----------------------------------------------------------------------------
{
}

//  ----------------------------------------------------------------------------
void CWiggleTrackData::Reset()
//  ----------------------------------------------------------------------------
{
    m_type = "";
    m_name = "";
    m_description = "";
}

//  ----------------------------------------------------------------------------
bool CWiggleTrackData::ParseData(
    const vector<string>& track_data )
//  ----------------------------------------------------------------------------
{
    if ( track_data.size() < 2 || track_data[0] != "track" ) {
        return false;
    }
    Reset();

    vector< string >::const_iterator it = track_data.begin();    
    for ( ++it; it != track_data.end(); ++it ) {
        vector< string > key_value_pair;
        CWiggleReader::Tokenize( *it, "=", key_value_pair );
        if ( key_value_pair.size() != 2 ) {
            return false;
        }
        if ( key_value_pair[0] == "type" ) {
            m_type = key_value_pair[1];
        }
        if ( key_value_pair[0] == "name" ) {
            m_name = key_value_pair[1];
        }
        if ( key_value_pair[0] == "description" ) {
            m_description = key_value_pair[1];
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
CWiggleGraphData::CWiggleGraphData()
//  ----------------------------------------------------------------------------
{
    Reset();
}

//  ----------------------------------------------------------------------------
void CWiggleGraphData::Reset()
//  ----------------------------------------------------------------------------
{
    m_type = TYPE_DATA_BED;
    m_chrom = "";
    m_start = 0;
    m_step = 0;
    m_span = 0;
}

//  ----------------------------------------------------------------------------
bool CWiggleGraphData::ParseDataBed(
    const vector<string>& data )
//  ----------------------------------------------------------------------------
{
    if ( data.size() != 4 ) {
        return false;
    }
    m_chrom = data[0];
    m_start = NStr::StringToUInt( data[1] );
    m_span = NStr::StringToUInt( data[2] ) - m_start;
    m_value = NStr::StringToDouble( data[3] );
    m_type = TYPE_DATA_BED;
    return true;
}

//  ----------------------------------------------------------------------------
bool CWiggleGraphData::ParseDataVarstep(
    const vector<string>& data )
//  ----------------------------------------------------------------------------
{
    if ( m_type != TYPE_DATA_VARSTEP || data.size() != 2 ) {
        return false;
    }
    m_start = NStr::StringToUInt( data[0] );
    m_value = NStr::StringToDouble( data[1] );
    return true;
}

//  ----------------------------------------------------------------------------
bool CWiggleGraphData::ParseDataFixedstep(
    const vector<string>& data )
//  ----------------------------------------------------------------------------
{
    if ( m_type != TYPE_DATA_FIXEDSTEP || data.size() != 1 ) {
        return false;
    }
    m_value = NStr::StringToDouble( data[0] );
    return true;
}

//  ----------------------------------------------------------------------------
bool CWiggleGraphData::ParseDeclarationVarstep(
    const vector<string>& data )
//
//  Note:   Once we make it in here, we know "data" starts with the obligatory
//          "variableStep" declaration.
//  ----------------------------------------------------------------------------
{
    Reset();

    vector<string>::const_iterator it = data.begin();
    for ( ++it; it != data.end(); ++it ) {
        vector<string> key_value_pair;
        CWiggleReader::Tokenize( *it, "=", key_value_pair );

        if ( key_value_pair.size() != 2 ) {
            return false;
        }
        if ( key_value_pair[0] == "chrom" ) {
            m_chrom = key_value_pair[1];
            continue;
        }
        if ( key_value_pair[0] == "span" ) {
            m_span = NStr::StringToUInt( key_value_pair[1] );
            continue;
        }
        return false;
    }
    m_type = TYPE_DATA_VARSTEP;
    return true;
}

//  ----------------------------------------------------------------------------
bool CWiggleGraphData::ParseDeclarationFixedstep(
    const vector<string>& data )
//  ----------------------------------------------------------------------------
{
    Reset();

    vector<string>::const_iterator it = data.begin();
    for ( ++it; it != data.end(); ++it ) {
        vector<string> key_value_pair;
        CWiggleReader::Tokenize( *it, "=", key_value_pair );

        if ( key_value_pair.size() != 2 ) {
            return false;
        }
        if ( key_value_pair[0] == "chrom" ) {
            m_chrom = key_value_pair[1];
            continue;
        }
        if ( key_value_pair[0] == "span" ) {
            m_span = NStr::StringToUInt( key_value_pair[1] );
            continue;
        }
        if ( key_value_pair[0] == "start" ) {
            m_start = NStr::StringToUInt( key_value_pair[1] );
            continue;
        }
        if ( key_value_pair[0] == "step" ) {
            m_step = NStr::StringToUInt( key_value_pair[1] );
            continue;
        }
        return false;
    }
    m_type = TYPE_DATA_FIXEDSTEP;
    return true;
}

END_objects_SCOPE
END_NCBI_SCOPE
