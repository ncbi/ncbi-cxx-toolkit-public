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
 *   Implementation of CSeqMaskerOStatOpt class.
 *
 */

#include <ncbi_pch.hpp>

#include <algorithm>

#include <corelib/ncbitype.h>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimisc.hpp>

#include <algo/winmask/seq_masker_ostat_opt.hpp>
#include <algo/winmask/seq_masker_util.hpp>

BEGIN_NCBI_SCOPE

static const unsigned long MB = 1024L*1024L;
static const unsigned long GROW_CHUNK = MB;
static const char * PARAMS[] = { "t_low", "t_extend", "t_threshold", "t_high" };

//------------------------------------------------------------------------------
CSeqMaskerOstatOpt::CSeqMaskerOstatOpt( 
        CNcbiOstream & os, Uint2 sz, bool alloc ) 
    : CSeqMaskerOstat( os, alloc ), size_requested( sz ),
      pvalues( sizeof( PARAMS )/sizeof( const char * ) )
{}

//------------------------------------------------------------------------------
Uint1 CSeqMaskerOstatOpt::UnitSize() const
{ return unit_bit_size/2; }

//------------------------------------------------------------------------------
const vector< Uint4 > & CSeqMaskerOstatOpt::GetParams() const
{ return pvalues; }

//------------------------------------------------------------------------------
void CSeqMaskerOstatOpt::doSetUnitSize( Uint4 us )
{ unit_bit_size = 2*us; }

//------------------------------------------------------------------------------
void CSeqMaskerOstatOpt::doSetUnitCount( Uint4 unit, Uint4 count )
{
    // The coditional code below is needed for low-memory machines
    // (32-bit with 3GB user space memory limit) to avoid problems
    // with default doubling of capacity in vector.
    // 
    // It will probably be removed in the future.
    //
    if( units.size() == units.capacity() )
    {
        size_t grow( std::max( (size_t)GROW_CHUNK, units.size()/10 ) );
        units.reserve( units.size() + grow );
        counts.reserve( units.size() + grow );
    }

    units.push_back( unit );
    counts.push_back( count );
}

//------------------------------------------------------------------------------
void CSeqMaskerOstatOpt::doSetParam( const string & name, Uint4 value )
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

//------------------------------------------------------------------------------
void CSeqMaskerOstatOpt::createCacheBitArray( Uint4 ** cba )
{
    *cba = 0;
    typedef vector< Uint4 >::size_type size_type;
    Uint8 total = (unit_bit_size == 32) ? 0x100000000ULL 
                                        : (1ULL<<unit_bit_size);
    Uint8 divisor = 8*sizeof( Uint4 );
    _TRACE( "divisor: " << divisor << " size: " << 
            (total/(2048*divisor)) << " KB" );
    size_type size = (size_type)( total/divisor );

    try
    {
        *cba = new Uint4[size];
    }
    catch( std::exception & e )
    { 
        ERR_POST( Warning << "cache bit array could not be allocated: " 
                          << e.what() );
        size = 0;
    }

    if( size > 0 )
    {
        fill( *cba, *cba + size, 0 );

        for( size_type i = 0; i < units.size(); ++i )
        {
            if( counts[i] >= pvalues[1] )
            {
                Uint4 unit = units[i];
                Uint4 rcunit = CSeqMaskerUtil::reverse_complement( 
                    unit, unit_bit_size/2 );
                Uint4 & word = (*cba)[unit/divisor];
                word |= (1<<(unit%divisor));
                Uint4 & word1 = (*cba)[rcunit/divisor];
                word1 |= (1<<(rcunit%divisor));
            }
        }
    }
}

//------------------------------------------------------------------------------
Uint1 CSeqMaskerOstatOpt::findBestRoff( Uint1 k, Uint1 & max_coll, 
                                        Uint4 & M, Uint4 * ht )
{
    Uint1 u = unit_bit_size;
    Uint4 sz = (1<<k);
    Uint1 mxcoll[8];
    Uint4 ncoll[8];
    double avcoll[8];

    for( Uint1 i = 0; i <= u - k; ++i )
    {
        fill( ht, ht + sz, 0 );

        for( vector< Uint4 >::const_iterator j = units.begin(); 
             j != units.end(); ++j )
          ++ht[ CSeqMaskerUtil::hash_code( *j, k, i ).first ];

        mxcoll[i] = *max_element( ht, ht + sz );
        Uint4 t = 0, tc = 0;

        for( Uint4 l = 0; l < sz; ++l )
            if( ht[l] > 1 )
            {
                ++tc;
                t += ht[l];
            }

        if( tc > 0 ) avcoll[i] = (double)t/tc;
        else avcoll[i] = 0;

        ncoll[i] = t;
        /*
        _TRACE(     " k                  = " << (int)k << endl
                 << " i                  = " << (int)i << endl
                 << " colliding units    = " << t << endl
                 << " collisions         = " << tc << endl
                 << " max collisions     = " << (int)mxcoll[i] << endl
                 << " average collisions = " << avcoll[i] );
        */
    }

    const double * minav = min_element( avcoll, avcoll + u - k + 1 );
    Uint1 roff = (minav - avcoll);
    max_coll = mxcoll[roff];
    M = ncoll[roff];
    return roff;
}

//------------------------------------------------------------------------------
void CSeqMaskerOstatOpt::doFinalize()
{
    LOG_POST( "optimizing the data structure" );
    Uint4 *cba = 0;
    createCacheBitArray( &cba );
    Uint1 k = unit_bit_size - 1;
    Uint1 roff, max_coll;
    Uint4 M; /* Units with collisions. */
    AutoPtr< Uint4, ArrayDeleter< Uint4 > > ht;

    Uint8 emem = 1ULL;
    for( Uint1 i = 0; i < k + 2; ++i ) emem *= 2;

    // estimate the range of k
    while( k >= unit_bit_size - 7 )
    {
        if( emem <= size_requested*MB ) break;
        emem /= 2;
        --k;
    }

    // Guard against allocating too much memory on 32 bit platform.
    // 
    if( sizeof( char * ) <= sizeof( Uint4 ) ) {
        if( k > 8*sizeof( char * ) - 4 )
            k = 8*sizeof( char * ) - 4;
    }

    if( k < unit_bit_size - 7 )
        NCBI_THROW( Exception, eMemory,
                    "Can not find parameters to satisfy memory requirements" );

    Uint1 b0, bc;
    Uint4 sz = (1<<k);

    // find the best value of k
    while( k >= unit_bit_size - 7 )
    {
        ht.reset();
        ht.reset( new Uint4[sz] );
        roff = findBestRoff( k, max_coll, M, ht.get() );
        bc = 0;

        while( (1UL<<bc) <= max_coll )
            ++bc;

        if( bc <= 7 )
        {
            b0 = 0;

            while( (1UL<<b0) <= M )
                ++b0;

            if( b0 + bc <= 32 )
                if( 2*M + (1UL<<(k+2)) <= size_requested*MB )
                    break;
        }

        --k;
    }

    if( k < unit_bit_size - 7 )
        NCBI_THROW( Exception, eMemory,
                    "Can not find parameters to satisfy memory requirements" );

    _TRACE(     "Using the following hash parameters: \n"
             << "hash key length = " << (int)k << " bits\n"
             << "right offset    = " << (int)roff << " bits\n"
             << "estimated size  = " << 2*M + (1UL<<(k+2)) << " bytes\n" );

    // fill in the hash and value tables.
    Uint4 * htp = ht.get();
    fill( htp, htp + sz, 0 );

    for( vector< Uint4 >::const_iterator j = units.begin(); 
         j != units.end(); ++j )
      ++htp[ CSeqMaskerUtil::hash_code( *j, k, roff ).first ];

    AutoPtr< Uint2, ArrayDeleter< Uint2 > > vt( new Uint2[M] );
    Uint2 * vtp = vt.get();
    Uint4 vend = 0;
    Uint4 coll_mask = ((1<<bc) - 1);

    for( Uint4 i = 0; i < units.size(); ++i )
    {
        pair< Uint4, Uint1 > hash 
            = CSeqMaskerUtil::hash_code( units[i], k, roff );
        Uint1 ccount = (htp[hash.first]&coll_mask);

        if( ccount != 0 ) {
            if( ccount == 1 )
            {
                Uint4 hsb = (((Uint4)(hash.second))<<24);
                htp[hash.first] += (((Uint4)counts[i])<<bc);
                htp[hash.first] += hsb;
            }
            else
            {
                if( (htp[hash.first]&~coll_mask) == 0 )
                {
                    vend += ccount;
                    htp[hash.first] += ((vend - 1)<<bc);
                }
                else htp[hash.first] -= (1<<bc);

                vtp[htp[hash.first]>>bc] 
                    = (((Uint2)(hash.second))<<9) + counts[i];
            }
        }
    }

    params p = { M, k, roff, bc, htp, vtp, cba };
    write_out( p );
}

//------------------------------------------------------------------------------
const char * CSeqMaskerOstatOpt::Exception::GetErrCodeString() const
{
    switch( GetErrCode() )
    {
        case eMemory: return "insufficient memory";
        default:      return CException::GetErrCodeString();
    }
}

END_NCBI_SCOPE
