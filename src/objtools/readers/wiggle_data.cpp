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
 *   WIGGLE transient data structures
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>

// Objects includes
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqres/Seq_graph.hpp>
#include <objects/seqres/Real_graph.hpp>
#include <objects/seqres/Int_graph.hpp>
#include <objects/seqres/Byte_graph.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objtools/readers/reader_exception.hpp>

#include <objtools/readers/reader_exception.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/error_container.hpp>
#include <objtools/readers/reader_base.hpp>
#include <objtools/readers/wiggle_reader.hpp>

#include "wiggle_data.hpp"

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ===========================================================================
CWiggleData::CWiggleData(
    unsigned int seq_start,
    double value ):
//  ===========================================================================
    m_uSeqStart( seq_start ),
    m_dValue( value )
{
};

//  ===========================================================================
CWiggleRecord::CWiggleRecord()
//  ===========================================================================
{
    Reset();
};

//  ----------------------------------------------------------------------------
void CWiggleRecord::Reset()
//  ----------------------------------------------------------------------------
{
    m_strName = "User Track";
    m_strBuild = "";
    m_strChrom = "";
    m_uSeqStart = 0;
    m_uSeqStep = 0;
    m_uSeqSpan = 0;
    m_dValue = 0;
}

//  ----------------------------------------------------------------------------
void CWiggleRecord::ParseTrackDefinition(
    const vector<string>& data )
//  ----------------------------------------------------------------------------
{
    Reset();
    if ( data.size() < 2 || data[0] != "track" ) {
        CObjReaderLineException err( 
            eDiag_Critical,
            0,
            "Missing track line --- Is this even WIGGLE?" );
        throw( err );
    }
    vector<string>::const_iterator it = data.begin();
    for ( ++it; it != data.end(); ++it ) {
        string strKey;
        string strValue;
        if ( ! NStr::SplitInTwo( *it, "=", strKey, strValue ) ) {
            CObjReaderLineException err( 
                eDiag_Error,
                0,
                "Invalid track line format --- only <key>=<value> pairs expected." );
            throw( err );
        }
        NStr::ReplaceInPlace( strValue, "\"", "" );
        if ( strKey == "name" ) {
            m_strName = strValue;
        }
    }
}

//  ----------------------------------------------------------------------------
void CWiggleRecord::ParseDataBed(
    const vector<string>& data )
//  ----------------------------------------------------------------------------
{
    if ( data.size() != 4 ) {
        CObjReaderLineException err( 
            eDiag_Error,
            0,
            "Invalid data line --- BED data with exactly four columns expected" );
        throw( err );
    }
    m_strChrom = data[0];
    m_uSeqStart = NStr::StringToUInt( data[1] );
    m_uSeqSpan = NStr::StringToUInt( data[2] ) - m_uSeqStart;
    m_dValue = NStr::StringToDouble( data[3] );
}

//  ----------------------------------------------------------------------------
void CWiggleRecord::ParseDataVarstep(
    const vector<string>& data )
//  ----------------------------------------------------------------------------
{
    m_uSeqStart = NStr::StringToUInt( data[0] ) - 1; // varStep is 1- based
    m_dValue = NStr::StringToDouble( data[1] );
}

//  ----------------------------------------------------------------------------
void CWiggleRecord::ParseDataFixedstep(
    const vector<string>& data )
//  ----------------------------------------------------------------------------
{
    m_uSeqStart += m_uSeqStep;
    m_dValue = NStr::StringToDouble( data[0] );
}

//  ----------------------------------------------------------------------------
void CWiggleRecord::ParseDeclarationVarstep(
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
            CObjReaderLineException err( 
                eDiag_Error,
                0,
                "Invalid VarStep declaration --- only key value pairs allowed" );
            throw( err );
        }
        if ( key_value_pair[0] == "chrom" ) {
            m_strChrom = key_value_pair[1];
            continue;
        }
        if ( key_value_pair[0] == "span" ) {
            m_uSeqSpan = NStr::StringToUInt( key_value_pair[1] );
            continue;
        }
        CObjReaderLineException err( 
            eDiag_Error,
            0,
            "Invalid VarStep declaration --- only \"chrom\" and \"span\" are "
            "permissible as keys" );
        throw( err );
    }
}

//  ----------------------------------------------------------------------------
void CWiggleRecord::ParseDeclarationFixedstep(
    const vector<string>& data )
//  ----------------------------------------------------------------------------
{
    Reset();

    vector<string>::const_iterator it = data.begin();
    for ( ++it; it != data.end(); ++it ) {
        vector<string> key_value_pair;
        CWiggleReader::Tokenize( *it, "=", key_value_pair );

        if ( key_value_pair.size() != 2 ) {
            CObjReaderLineException err( 
                eDiag_Error,
                0,
                "Invalid FixedStep declaration --- only key value pairs allowed" );
            throw( err );
        }
        if ( key_value_pair[0] == "chrom" ) {
            m_strChrom = key_value_pair[1];
            continue;
        }
        if ( key_value_pair[0] == "span" ) {
            m_uSeqSpan = NStr::StringToUInt( key_value_pair[1] );
            continue;
        }
        if ( key_value_pair[0] == "start" ) {
            m_uSeqStart = NStr::StringToUInt( key_value_pair[1] ) - 1;
            continue;
        }
        if ( key_value_pair[0] == "step" ) {
            m_uSeqStep = NStr::StringToUInt( key_value_pair[1] );
            continue;
        }
        CObjReaderLineException err( 
            eDiag_Error,
            0,
            "Invalid VarStep declaration --- only \"chrom\", \"span\", \"start\" "
            "and \"step\" are permissible as keys" );
        throw( err );
    }
    m_uSeqStart -= m_uSeqStep;
}

//  ===========================================================================
CWiggleTrack::CWiggleTrack(
    const CWiggleRecord& record ):
//  ===========================================================================
    m_strName( record.Name() ),
    m_strChrom( record.Chrom() ),
    m_uGraphType( GRAPH_UNKNOWN ),
    m_uSeqLength( 0 )
{
    CWiggleData* pData = new CWiggleData( record.SeqStart(), record.Value() );
    m_Entries[ pData->SeqStart() ] = pData;
    m_uSeqSpan = record.SeqSpan();
    m_dMaxValue = (record.Value() > 0) ? record.Value() : 0;
    m_dMinValue = (record.Value() < 0) ? record.Value() : 0;

    if ( m_uSeqLength == 0 ) {
        m_uSeqStart = record.SeqStart();
        m_uSeqStop = record.SeqStart() + record.SeqSpan();
    }
    m_dMaxValue = (record.Value() > 0) ? record.Value() : 0;
    m_dMinValue = (record.Value() < 0) ? record.Value() : 0;
    
};

//  ===========================================================================
CWiggleTrack::~CWiggleTrack()
//  ===========================================================================
{
    for ( DataIter it = m_Entries.begin(); it != m_Entries.end(); ++it ) {
        delete it->second;
    }
}

//  ===========================================================================
void CWiggleTrack::AddRecord(
    const CWiggleRecord& record )
//  ===========================================================================
{
    if ( m_strChrom != record.Chrom() ) {
        CObjReaderLineException err( 
            eDiag_Warning,
            0,
            "Data record with wrong chromosome: rejected" );
        throw( err );
    }
    if ( SeqSpan() != record.SeqSpan() ) {
        CObjReaderLineException err( 
            eDiag_Warning,
            0,
            "Data record with suspicious span: rejected" );
        throw( err );
    }
    if ( 0 != (record.SeqStart() - m_uSeqStart) % SeqSpan() ) {
        CObjReaderLineException err( 
            eDiag_Warning,
            0,
            "Data record not aligned with span multiple: rejected" );
        throw( err );
    }
    CWiggleData* pData = new CWiggleData( record.SeqStart(), record.Value() );        
    m_Entries[ record.SeqStart() ] = pData;
    
    if ( m_uSeqLength == 0 ) {
        if ( m_uSeqStart > record.SeqStart() ) {
            m_uSeqStart = record.SeqStart();
        }
        if ( m_uSeqStop < record.SeqStart() + record.SeqSpan() ) {
            m_uSeqStop = record.SeqStart() + record.SeqSpan();
        }
    }
    if ( m_dMaxValue < record.Value() ) {
        m_dMaxValue = record.Value();
    }
    if ( record.Value() < m_dMinValue ) {
        m_dMinValue = record.Value();
    }
};

//  ===========================================================================
CWiggleSet::CWiggleSet()
//  ===========================================================================
{
};

//  ===========================================================================
unsigned int CWiggleTrack::SeqStart() const
//  ===========================================================================
{
    if ( m_uSeqLength == 0 ) { 
        return m_uSeqStart;
    }
    return 0;
};

//  ===========================================================================
unsigned int CWiggleTrack::SeqStop() const
//  ===========================================================================
{
    if ( m_uSeqLength == 0 ) { 
        return m_uSeqStop-1;
    }
    return m_uSeqLength-1;
};

//  ===========================================================================
bool CWiggleSet::AddRecord(
    const CWiggleRecord& record )
//  ===========================================================================
{
    CWiggleTrack* pTrack = 0;
    if ( FindTrack( record.Chrom(), pTrack ) ) {
        pTrack->AddRecord( record );
    }
    else {
        m_Tracks[ record.Chrom() ] = new CWiggleTrack( record );
    }
    return true;
};

//  ===========================================================================
bool CWiggleSet::FindTrack(
    const string& key,
    CWiggleTrack*& pTrack )
//  ===========================================================================
{
    for ( TrackIter it=m_Tracks.begin(); it!=m_Tracks.end(); ++it ) {
        if ( it->second->Chrom() == key ) {
            pTrack = it->second;
            return true;
        }
    }
    return false;
}

//  ===========================================================================
void CWiggleSet::Dump(
    CNcbiOstream& Out )
//  ===========================================================================
{
    for ( TrackIter it = m_Tracks.begin(); it != m_Tracks.end(); ++it ) {
        it->second->Dump( Out );
    }
}

//  ===========================================================================
void CWiggleSet::MakeGraph(
    CSeq_annot::TData::TGraph& graphset )
//  ===========================================================================
{
    for ( TrackIter it = m_Tracks.begin(); it != m_Tracks.end(); ++it ) {
        it->second->MakeGraph( graphset );
    }       
}

//  ===========================================================================
void CWiggleTrack::MakeGraph(
    CSeq_annot::TData::TGraph& graphset )
//  ===========================================================================
{
    CRef<CSeq_graph> graph( new CSeq_graph );
    graph->SetTitle( m_strName );
    
    CSeq_interval& loc = graph->SetLoc().SetInt();
    loc.SetId().Set( CSeq_id::e_Local, m_strChrom );
    loc.SetFrom( SeqStart() );
    loc.SetTo( SeqStop() );
        
    graph->SetComp( SeqSpan() );
    graph->SetNumval( (SeqStop() - SeqStart() + 1) / SeqSpan() );
    graph->SetA( ScaleLinear() );
    graph->SetB( ScaleConst() );
    
    switch( GetGraphType() ) {
            
        default:
            FillGraphByte( graph->SetGraph().SetByte() );
            break;
    
        case GRAPH_REAL:
            FillGraphReal( graph->SetGraph().SetReal() );
            break;
            
        case GRAPH_INT:
            FillGraphInt( graph->SetGraph().SetInt() );
            break;
    }
            
    graphset.push_back( graph );
}

//  ===========================================================================
void CWiggleTrack::FillGraphReal(
    CReal_graph& graph )
//  ===========================================================================
{
    unsigned int uDataSize = (SeqStop() - SeqStart() + 1) / SeqSpan();
    vector<double> values( uDataSize, 0 );
    for ( unsigned int u = 0; u < uDataSize; ++u ) {
    
        //  *******************************************************************
        //  Note:
        //  This code does not properly distiguish between missing values and 
        //  values being ==0. Need to come up with a convention if we ever
        //  commit to supporting float graph data.
        //  The byte graph convention does not quite carry over.
        //  ******************************************************************* 
        double dRaw( 0 );
        if ( DataValue( SeqStart() + u * SeqSpan(), dRaw ) ) {
            values[ u ] = dRaw;
        }
        else {
            values[ u ] = 0;
        } 
    }
    graph.SetMin( m_dMinValue );
    graph.SetMax( m_dMaxValue );
    graph.SetAxis( 0 );
    graph.SetValues() = values;
}

//  ===========================================================================
void CWiggleTrack::FillGraphInt(
    CInt_graph& graph )
//  ===========================================================================
{
    /* to do --- if we ever have a need for this */
}

//  ===========================================================================
void CWiggleTrack::FillGraphByte(
    CByte_graph& graph )
//
//  Idea:   Scale the set of values found linearly to the interval 1 (lowest)
//          to 255 (highest). Gap "values" are set to 0.
//  ===========================================================================
{
    graph.SetMin( 0 );         // the interval we are scaling the y-values
    graph.SetMax( 255 );       //   into...
    graph.SetAxis( 0 );
    
    unsigned int uDataSize = (SeqStop() - SeqStart() + 1) / SeqSpan();
    vector<char> values( uDataSize, 0 );
    for ( unsigned int u = 0; u < uDataSize; ++u ) {
        values[ u ] = ByteGraphValue( SeqStart() + u * SeqSpan() );
    }
    graph.SetValues() = values;
}

//  ===========================================================================
bool CWiggleTrack::DataValue(
    unsigned int uStart,
    double& dValue )
//  ===========================================================================
{
    if ( GRAPH_UNKNOWN == m_uGraphType ) {
        m_uGraphType = GetGraphType();
    }
    DataIter it = m_Entries.find( uStart );
    if ( it == m_Entries.end() ) {
        return false;
    }
    dValue = it->second->Value();
    return true;
}

//  ===========================================================================
unsigned char CWiggleTrack::ByteGraphValue(
    unsigned int uStart )
//  ===========================================================================
{
    double dRaw( 0 );
    if ( ! DataValue( uStart, dRaw ) ) {
        // return 0 as the gap value
        return static_cast<unsigned char>( 0 );
    }
    else {
        // scale into interval [1,255]
        if ( m_dMinValue == m_dMaxValue ) {
            return static_cast<unsigned char>( (dRaw ? 255 : 1) );
        }
        double dScaled =  1 +
            ( 254 * (dRaw - m_dMinValue) / (m_dMaxValue - m_dMinValue) );
        return static_cast<unsigned char>( dScaled + 0.5 );
    }
    
    
}

//  ===========================================================================
double CWiggleTrack::ScaleConst() const
//  ===========================================================================
{
    return m_dMinValue;
}

//  ===========================================================================
double CWiggleTrack::ScaleLinear() const
//  ===========================================================================
{
    return (m_dMaxValue - m_dMinValue) / 255;
}
    
//  ===========================================================================
unsigned int CWiggleTrack::GetGraphType()
//  ===========================================================================
{
    if ( m_uGraphType != GRAPH_UNKNOWN ) {
        return m_uGraphType;
    }
    m_uGraphType = GRAPH_BYTE;
    return m_uGraphType;
} 

//  ===========================================================================
void CWiggleTrack::Dump(
    CNcbiOstream& Out )
//  ===========================================================================
{
    Out << "track chrom=" << Chrom() << " seqstart=" << SeqStart()
        << " seqstop=" << SeqStop() << " count=" << Count() << endl;
    for (DataIter it = m_Entries.begin(); it != m_Entries.end(); ++it ) {
        it->second->Dump( Out );
    }
    Out << endl;
}

//  ===========================================================================
void CWiggleData::Dump(
    CNcbiOstream& Out )
//  ===========================================================================
{
    Out << "  data start=" << SeqStart() << " value=" << Value() << endl;
}

END_objects_SCOPE
END_NCBI_SCOPE

