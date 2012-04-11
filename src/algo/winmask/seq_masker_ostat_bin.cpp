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
 * Author:  Aleksandr Morgulis
 *
 * File Description:
 *   Implementation of CSeqMaskerOstatBin class.
 *
 */

#include <ncbi_pch.hpp>

#include <algo/winmask/seq_masker_ostat_bin.hpp>

BEGIN_NCBI_SCOPE

//------------------------------------------------------------------------------
/**\internal
 **\brief Names and order of allowed parameters.
 **/
static const char * PARAMS[] = { "t_low", "t_extend", "t_threshold", "t_high" };

//------------------------------------------------------------------------------
CSeqMaskerOstatBin::CSeqMaskerOstatBin( const string & name )
    : CSeqMaskerOstat( static_cast< CNcbiOstream& >(
        *new CNcbiOfstream( name.c_str(), IOS_BASE::binary ) ), true ),
      pvalues( sizeof( PARAMS )/sizeof( const char * ) )
{ write_word( (Uint4)0 ); } // Format identifier.

//------------------------------------------------------------------------------
CSeqMaskerOstatBin::CSeqMaskerOstatBin( CNcbiOstream & os )
    : CSeqMaskerOstat( os, false ),
      pvalues( sizeof( PARAMS )/sizeof( const char * ) )
{ write_word( (Uint4)0 ); } // Format identifier.

//------------------------------------------------------------------------------
CSeqMaskerOstatBin::~CSeqMaskerOstatBin()
{
  try{
    for( vector< Uint4 >::const_iterator i = pvalues.begin();
         i != pvalues.end(); ++i )
         write_word( *i );
  }
  catch( std::exception & e )
  { LOG_POST( Error<< "Error writing trailer: " << e.what() ); }

  out_stream.flush();
}

//------------------------------------------------------------------------------
void CSeqMaskerOstatBin::write_word( Uint4 word )
{
  out_stream.write( reinterpret_cast< const char * >(&word), sizeof( Uint4 ) );
}

//------------------------------------------------------------------------------
void CSeqMaskerOstatBin::doSetUnitSize( Uint4 us )
{ write_word( us ); }

//------------------------------------------------------------------------------
void CSeqMaskerOstatBin::doSetUnitCount( Uint4 unit, Uint4 count )
{
  write_word( unit );
  write_word( count );
}

//------------------------------------------------------------------------------
void CSeqMaskerOstatBin::doSetParam( const string & name, Uint4 value )
{
    string::size_type pos = name.find_first_of( ' ' );
    string real_name = name.substr( 0, pos );

    for( unsigned ind = 0; 
         ind < sizeof( PARAMS )/sizeof( const char * ); ++ind )
        if( real_name == PARAMS[ind] )
        {
            pvalues[ind] = value;
            return;
        }

    LOG_POST( Error << "Unknown parameter name " << real_name );
}

END_NCBI_SCOPE
