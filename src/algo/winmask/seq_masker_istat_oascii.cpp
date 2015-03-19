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
 *   IMplementation for CSeqMaskerIstatOAscii class.
 *
 */

#include <ncbi_pch.hpp>

#include <string>
#include <sstream>

#include <algo/winmask/seq_masker_istat_oascii.hpp>

BEGIN_NCBI_SCOPE

static const unsigned int HEADER_LINES = 7U;

//------------------------------------------------------------------------------
const char * 
CSeqMaskerIstatOAscii::Exception::GetErrCodeString() const
{
    switch( GetErrCode() )
    {
        case eStreamOpenFail:   return "open failed";
        case eBadHashParam:     return "bad hash parameter";
        case eBadParam:         return "bad parameter";
        case eFormat:           return "format error";
        case eAlloc:            return "allocation failure";
        default:                return CException::GetErrCodeString();
    }
}

//------------------------------------------------------------------------------
CSeqMaskerIstatOAscii::CSeqMaskerIstatOAscii( const string & name,
                                              Uint4 arg_threshold,
                                              Uint4 arg_textend,
                                              Uint4 arg_max_count,
                                              Uint4 arg_use_max_count,
                                              Uint4 arg_min_count,
                                              Uint4 arg_use_min_count,
                                              Uint4 start_line )
    :   CSeqMaskerIstat(    arg_threshold, arg_textend, 
                            arg_max_count, arg_use_max_count,
                            arg_min_count, arg_use_min_count )
{
    CNcbiIfstream input_stream( name.c_str() );

    if( !input_stream )
        NCBI_THROW( Exception, eStreamOpenFail,
                    string( "could not open " ) + name );

    Uint4 linenum = 0;
    string line;
    for( Uint4 i( 0 ); i < start_line; ++i ) getline( input_stream, line );
    Uint1 unit_size;
    Uint4 k, roff, bc;
    Uint4 t_low    = 0, 
          t_extend = 0, 
          t_thres  = 0, 
          t_high   = 0, 
          M;

    while( getline( input_stream, line ) )
    {
        ++linenum;

        switch( linenum )
        {
            case 1: break; //skip the file format identifier
            case 2: // unit size
                
                unit_size = (Uint1)atoi( line.c_str() ); 

                if( unit_size == 0 || unit_size > 16 )
                    NCBI_THROW( Exception, eBadParam,
                                "unit size must be in [1,16]" );

                uset.setUnitSize( unit_size );
                break;

            case 3: // hash table parameters

                {
                    istringstream i( line );
                    i >> M >> k >> roff >> bc;

                    if( k == 0U || k > (Uint4)(2*unit_size - 1) )
                        NCBI_THROW( 
                            Exception, eBadHashParam,
                            "hash key size must be in [1,2*unit_size - 1]" );

                    if( roff > 32 - k )
                        NCBI_THROW( 
                            Exception, eBadHashParam,
                            "offset must by in [0,32 - hash_key_size]" );

                    if( bc == 0 || bc > 32 - k )
                        NCBI_THROW(
                            Exception, eBadHashParam,
                            "shift must be in "
                            "[1, 32 - hash_key_size]" );
                }

                break;

            case 4: t_low    = atoi( line.c_str() ); break;
            case 5: t_extend = atoi( line.c_str() ); break;
            case 6: t_thres  = atoi( line.c_str() ); break;
            case 7: t_high   = atoi( line.c_str() ); break;

            default: break;
        }

        if( linenum == HEADER_LINES )
            break;
    }

    if( linenum < HEADER_LINES )
        NCBI_THROW( Exception, eFormat, "file too short" );

    set_min_count( t_low );

    if( get_textend() == 0 )
        set_textend( t_extend );

    if( get_threshold() == 0 )
        set_threshold( t_thres );

    if( get_max_count() == 0 )
        set_max_count( t_high );

    if( get_use_min_count() == 0 )
      set_use_min_count( (get_min_count() + 1)/2 );

    if( get_use_max_count() == 0 )
      set_use_max_count( get_max_count() );

    Uint4 ht_size = (1<<k);
    Uint4 * ht = new Uint4[ht_size];
    
    if( ht == 0 )
        NCBI_THROW( Exception, eAlloc, "hash table allocation failed" );

    for( linenum = 0; 
         linenum < ht_size && getline( input_stream, line ); 
         ++linenum )
        ht[linenum] = atoi( line.c_str() );

    if( linenum < ht_size )
        NCBI_THROW( Exception, eFormat, 
                    "not enough lines to fill the hash table" );

    uset.add_ht_info( (Uint1)k, (Uint1)roff, (Uint1)bc, ht );

    Uint2 * vt = new Uint2[M];

    if( vt == 0 )
        NCBI_THROW( Exception, eAlloc, "values table allocation failed" );

    for( linenum = 0; 
         linenum < M && getline( input_stream, line ); 
         ++linenum )
        vt[linenum] = (Uint2)atoi( line.c_str() );

    if( linenum < M )
        NCBI_THROW( Exception, eFormat, 
                    "not enough lines to fill the values table" );

    uset.add_vt_info( M, vt );
}

//------------------------------------------------------------------------------
Uint4 CSeqMaskerIstatOAscii::trueat( Uint4 unit ) const
{ return uset.get_info( unit ); }

//------------------------------------------------------------------------------
Uint4 CSeqMaskerIstatOAscii::at( Uint4 unit ) const
{
    Uint4 res = uset.get_info( unit );

    if( res == 0 || res < get_min_count() )
        return get_use_min_count();

    return (res > get_max_count()) ? get_use_max_count() : res;
}

END_NCBI_SCOPE
