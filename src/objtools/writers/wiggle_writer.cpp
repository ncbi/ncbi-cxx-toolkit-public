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

//#include <objtools/writers/gff_record.hpp>
#include <objtools/writers/wiggle_writer.hpp>

#include <math.h>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

#define ROUND(x)    int( floor((x)+0.5) )

//  ----------------------------------------------------------------------------
CWiggleWriter::CWiggleWriter(
    CNcbiOstream& ostr,
    size_t uTrackSize ) :
//  ----------------------------------------------------------------------------
    CWriterBase( ostr, 0 ),
    m_uTrackSize( uTrackSize == 0 ? size_t( -1 ) : uTrackSize )
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
    if ( annot.IsGraph() ) {
        return WriteAnnotGraphs( annot );
    }
    cerr << "Unexpected!" << endl;
    return false;
}

//  ----------------------------------------------------------------------------
bool CWiggleWriter::WriteAnnotTable( const CSeq_annot& annot )
//  ----------------------------------------------------------------------------
{
    cerr << "To be done." << endl;
    return false;
}

//  ----------------------------------------------------------------------------
bool CWiggleWriter::WriteAnnotGraphs( const CSeq_annot& annot )
//  ----------------------------------------------------------------------------
{

    if ( ! annot.CanGetDesc() ) {
        return false;
    }
    const CAnnot_descr& descr = annot.GetDesc();
    if ( ! WriteGraphsTrackLine( descr ) ) {
        return false;
    }

    const list< CRef<CSeq_graph> >& graphs = annot.GetData().GetGraph();
    list< CRef<CSeq_graph> >::const_iterator it = graphs.begin();
    while ( it != graphs.end() ) {
        if ( ! WriteSingleGraph( **it ) ) {
            return false;
        }
        ++it;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CWiggleWriter::WriteSingleGraph( const CSeq_graph& graph )
//  ----------------------------------------------------------------------------
{
    if ( ! graph.CanGetNumval() ) {
        return false;
    }
    size_t uNumVals = graph.GetNumval();

    for ( size_t u=0; u < uNumVals; u += m_uTrackSize ) {
        if ( ! ContainsData( graph, u ) ) {
            continue;
        }
        if ( ! WriteSingleGraphFixedStep( graph, u ) ) {
            return false;
        }
        if ( ! WriteSingleGraphRecords( graph, u ) ) {
            return false;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CWiggleWriter::WriteGraphsTrackLine( const CAnnot_descr& descr )
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

    m_Os << strTrackLine << endl;
    return true;
}

//  ----------------------------------------------------------------------------
bool CWiggleWriter::WriteSingleGraphFixedStep( 
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
    string strChrom( " chrom=" );
    //  ------------------------------------------------------------------------
    const CSeq_id* pId = graph.GetLoc().GetId();
    switch ( pId->Which() ) {
    default:
        pId->GetLabel( &strChrom );
        break;

    case CSeq_id::e_Local:
        if ( pId->GetLocal().IsStr() ) {
            strChrom += pId->GetLocal().GetStr();
            break;
        }
        pId->GetLabel( &strChrom );
        break;
    }
    strFixedStep += strChrom;

    //  ------------------------------------------------------------------------
    string strStart( " start=" );
    //  ------------------------------------------------------------------------
    const CSeq_loc& location = graph.GetLoc();
    if ( ! location.IsInt() || ! location.GetInt().CanGetFrom() ) {
        return false;
    }
    size_t uFrom = location.GetInt().GetFrom();

    strStart += NStr::NumericToString( uFrom + uSeqStart * graph.GetComp() );
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
   
    m_Os << strFixedStep << endl;
    return true;
}

//  ----------------------------------------------------------------------------
bool CWiggleWriter::WriteSingleGraphRecords( 
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
    const vector<char>& values = graph.GetGraph().GetByte().GetValues();

    for ( size_t u=0; u + uStartRecord < uNumVals && u < m_uTrackSize; ++u ) {
        int iVal = (unsigned char)values[u + uStartRecord];
        m_Os << ROUND( dA*iVal+dB ) << endl;
    }
//    m_Os << endl;
    return true;
}

//  ----------------------------------------------------------------------------
bool CWiggleWriter::ContainsData(
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
    const vector<char>& values = graph.GetGraph().GetByte().GetValues();
    for ( size_t u=0; u + uStartRecord < uNumVals && u < m_uTrackSize; ++u ) {
        int iVal = (unsigned char)values[u + uStartRecord];
        if ( 0 != iVal ) {
            return true;
        }
    }
    return false;
}

END_NCBI_SCOPE
