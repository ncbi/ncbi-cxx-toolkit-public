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

#define STAT_FMT_COMPONENT_NAME "windowmasker-statistics-format-version"
#define STAT_FMT_VER_MAJOR 1
#define STAT_FMT_VER_MINOR 0
#define STAT_FMT_VER_PATCH 0
#define STAT_FMT_VER_PFX "binary "

//------------------------------------------------------------------------------
CSeqMaskerVersion CSeqMaskerOstatBin::FormatVersion(
        STAT_FMT_COMPONENT_NAME, 
        STAT_FMT_VER_MAJOR,
        STAT_FMT_VER_MINOR,
        STAT_FMT_VER_PATCH,
        STAT_FMT_VER_PFX
);

//------------------------------------------------------------------------------
CSeqMaskerOstatBin::CSeqMaskerOstatBin( 
        const string & name, string const & metadata )
    : CSeqMaskerOstat( static_cast< CNcbiOstream& >(
        *new CNcbiOfstream( name.c_str(), IOS_BASE::binary ) ), 
        true, metadata )
{}

//------------------------------------------------------------------------------
CSeqMaskerOstatBin::CSeqMaskerOstatBin( 
        CNcbiOstream & os, string const & metadata )
    : CSeqMaskerOstat( os, false, metadata )
{ write_word( (Uint4)0 ); } // Format identifier.

//------------------------------------------------------------------------------
CSeqMaskerOstatBin::~CSeqMaskerOstatBin()
{
}

//------------------------------------------------------------------------------
void CSeqMaskerOstatBin::write_word( Uint4 word )
{
  out_stream.write( reinterpret_cast< const char * >(&word), sizeof( Uint4 ) );
}

//------------------------------------------------------------------------------
void CSeqMaskerOstatBin::doSetUnitCount( Uint4 unit, Uint4 count )
{
  counts.push_back( std::make_pair( unit, count ) );
}

//------------------------------------------------------------------------------
void CSeqMaskerOstatBin::doFinalize() {
    write_word( (Uint4)3 ); // new binary id
    WriteBinMetaData( out_stream );
    write_word( (Uint4)0 ); // Format identifier.
    write_word( (Uint4)unit_size );

    for( size_t i( 0 ); i < counts.size(); ++i ) {
        write_word( counts[i].first );
        write_word( counts[i].second );
    }

    for( vector< Uint4 >::const_iterator i = pvalues.begin(); 
            i != pvalues.end(); ++i ) {
         write_word( *i );
    }

    out_stream.flush();
}

END_NCBI_SCOPE
