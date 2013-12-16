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
 *   Implementation of CSeqMaskerOStatOptBin class.
 *
 */

#include <ncbi_pch.hpp>

#include "algo/winmask/seq_masker_ostat_opt_bin.hpp"

BEGIN_NCBI_SCOPE

//------------------------------------------------------------------------------
CSeqMaskerOstatOptBin::CSeqMaskerOstatOptBin( const string & name, 
                                              Uint2 sz, bool arg_use_ba )
    : CSeqMaskerOstatOpt( static_cast< CNcbiOstream& >(
        *new CNcbiOfstream( name.c_str(), IOS_BASE::binary ) ), sz, true ),
      use_ba( arg_use_ba )
{ 
    if( use_ba )
        write_word( (Uint4)2 ); 
    else write_word( (Uint4)1 );
} 

//------------------------------------------------------------------------------
CSeqMaskerOstatOptBin::CSeqMaskerOstatOptBin( CNcbiOstream & os,
                                              Uint2 sz, bool arg_use_ba )
    : CSeqMaskerOstatOpt( os, sz, false ),
      use_ba( arg_use_ba )
{ 
    if( use_ba )
        write_word( (Uint4)2 ); 
    else write_word( (Uint4)1 );
} 

//------------------------------------------------------------------------------
void CSeqMaskerOstatOptBin::write_out( const params & p ) const
{
    write_word( (Uint4)UnitSize() );
    write_word( p.M );
    write_word( (Uint4)p.k );
    write_word( (Uint4)p.roff );
    write_word( (Uint4)p.bc );

    for( Uint4 i = 0; i < GetParams().size(); ++i )
        write_word( GetParams()[i] );

    if( use_ba ) {
        if( p.cba != 0 )
        {
            Uint8 total = 
                (UnitSize() == 16) ? 0x100000000ULL : (1ULL<<(2*UnitSize()));
            Uint4 size = (Uint4)(total/(8*sizeof( Uint4 )));
            write_word( (Uint4)1 );
            out_stream.write( (const char *)(p.cba), size*sizeof( Uint4 ) );
        }
        else write_word( (Uint4)0 );
    }

    Uint4 sz = (1<<p.k);
    out_stream.write( (const char *)(p.ht), sz*sizeof( Uint4 ) );
    out_stream.write( (const char *)(p.vt), p.M*sizeof( Uint2 ) );
    out_stream << flush;
}

END_NCBI_SCOPE
