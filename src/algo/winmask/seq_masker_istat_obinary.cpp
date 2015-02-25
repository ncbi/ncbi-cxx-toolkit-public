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
 *   IMplementation for CSeqMaskerIstatOBinary class.
 *
 */

#include <ncbi_pch.hpp>

#include <string>
#include <sstream>

#include <algo/winmask/seq_masker_istat_obinary.hpp>

BEGIN_NCBI_SCOPE

//------------------------------------------------------------------------------
const char * 
CSeqMaskerIstatOBinary::Exception::GetErrCodeString() const
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
Uint4 CSeqMaskerIstatOBinary::readWord( CNcbiIstream & is ) const
{
    Uint4 result;

    if( !is )
        NCBI_THROW( Exception, eFormat, "file too short" );

    is.read( (char *)&result, sizeof( Uint4 ) );
    return result;
}

//------------------------------------------------------------------------------
CSeqMaskerIstatOBinary::CSeqMaskerIstatOBinary( const string & name,
                                                Uint4 arg_threshold,
                                                Uint4 arg_textend,
                                                Uint4 arg_max_count,
                                                Uint4 arg_use_max_count,
                                                Uint4 arg_min_count,
                                                Uint4 arg_use_min_count,
                                                bool arg_use_ba )
    :   CSeqMaskerIstat(    arg_threshold, arg_textend, 
                            arg_max_count, arg_use_max_count,
                            arg_min_count, arg_use_min_count )
{
    bool use_opt = true;
    CNcbiIfstream input_stream( name.c_str(), IOS_BASE::binary );

    if( !input_stream )
        NCBI_THROW( Exception, eStreamOpenFail,
                    string( "could not open " ) + name );

    Uint4 word;
    Uint1 unit_size;
    Uint4 k, roff, bc;
    Uint4 t_low    = 0, 
          t_extend = 0, 
          t_thres  = 0, 
          t_high   = 0, 
          M;

    word = readWord( input_stream );

    if( word == 1 )
        use_opt = false;

    word = readWord( input_stream );
    unit_size = (Uint1)word;

    if( unit_size == 0 || unit_size > 16 )
        NCBI_THROW( Exception, eBadParam,
            "unit size must be in [1,16]" );

    uset.setUnitSize( unit_size );

    M    = readWord( input_stream );
    k    = readWord( input_stream );
    roff = readWord( input_stream );
    bc   = readWord( input_stream );

    if( k == 0U || k > (Uint4)(2*unit_size - 1) )
        NCBI_THROW( Exception, eBadHashParam,
                    "hash key size must be in [1,2*unit_size - 1]" );

    if( roff > 32 - k )
        NCBI_THROW( Exception, eBadHashParam,
                    "offset must by in [0,32 - hash_key_size]" );

    if( bc == 0 || bc > 32 - k )
        NCBI_THROW( Exception, eBadHashParam,
                    "shift must be in [1, 32 - hash_key_size]" );

    t_low    = readWord( input_stream );
    t_extend = readWord( input_stream );
    t_thres  = readWord( input_stream );
    t_high   = readWord( input_stream );

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

    if( use_opt )
    {
        Uint4 divisor = readWord( input_stream );

        if( divisor > 0 )
        {
            Uint8 total = (1ULL<<(2*unit_size));
            Uint4 cba_size = (Uint4)(total/(8*sizeof( Uint4 )));
            Uint4 * cba = new Uint4[cba_size];

            if( cba == 0 )
                LOG_POST( Warning << "allocation failed: "
                                  << "bit array optimizations are not used." );
            else if( !input_stream.read( (char *)cba, cba_size*sizeof( Uint4 ) ) )
            {
                LOG_POST( Warning << "file read failed: "
                                  << "bit array optimizations are not used." );
                delete[] cba;
                cba = 0;
            }
                
            if( !arg_use_ba )
            {
                delete[] cba;
                cba = 0;
            }

            optimization_data opt_data( 8*sizeof( Uint4 ), cba );
            set_optimization_data( opt_data );
        }
    }

    Uint4 ht_size = (1<<k);
    Uint4 * ht = new Uint4[ht_size];
    
    if( ht == 0 )
        NCBI_THROW( Exception, eAlloc, "hash table allocation failed" );

    if( !input_stream.read( (char *)ht, ht_size*sizeof( Uint4 ) ) )
        NCBI_THROW( Exception, eFormat, 
                    "not enough data to fill the hash table" );

    uset.add_ht_info( (Uint1)k, (Uint1)roff, (Uint1)bc, ht );

    Uint2 * vt = new Uint2[M];

    if( vt == 0 )
        NCBI_THROW( Exception, eAlloc, "values table allocation failed" );

    if( !input_stream.read( (char *)vt, M*sizeof( Uint2 ) ) )
        NCBI_THROW( Exception, eFormat, 
                    "not enough data to fill the values table" );

    uset.add_vt_info( M, vt );
    Uint4 mdsz( 0 );

    if( input_stream.read( (char *)&mdsz, sizeof( Uint4 ) ) ) {
        string md;
        char c;

        for( Uint4 i( 0 ); i < mdsz; ++i ) {
            input_stream.read( &c, 1 );
            md.push_back( c );
        }

        if( md.size() < 2 || md.substr( 0, 2 ) != "##" ) {
            md = string( "##" ) + md;
        }

        ParseMetaDataString( md );
    }
}

//------------------------------------------------------------------------------
Uint4 CSeqMaskerIstatOBinary::trueat( Uint4 unit ) const
{ return uset.get_info( unit ); }

//------------------------------------------------------------------------------
Uint4 CSeqMaskerIstatOBinary::at( Uint4 unit ) const
{
    Uint4 res = uset.get_info( unit );

    if( res == 0 || res < get_min_count() )
        return get_use_min_count();

    return (res > get_max_count()) ? get_use_max_count() : res;
}

END_NCBI_SCOPE
