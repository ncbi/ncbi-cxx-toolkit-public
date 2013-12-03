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
 *   Implementation of CWinMaskCountsGenerator class.
 *
 */

#include <ncbi_pch.hpp>
#include <stdlib.h>

#include <vector>
#include <sstream>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seq/IUPACna.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seq_vector.hpp>

#include <algo/winmask/seq_masker_util.hpp>

#include <algo/winmask/win_mask_gen_counts.hpp>
#include <algo/winmask/win_mask_dup_table.hpp>
#include <algo/winmask/win_mask_util.hpp>
#include "algo/winmask/seq_masker_ostat_factory.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


//------------------------------------------------------------------------------
static Uint4 letter( char c )
{
    switch( c )
    {
    case 'a': case 'A': return 0;
    case 'c': case 'C': return 1;
    case 'g': case 'G': return 2;
    case 't': case 'T': return 3;
    default: return 0;
    }
}

//------------------------------------------------------------------------------
static inline bool ambig( char c )
{
    return    c != 'a' && c != 'A' && c != 'c' && c != 'C'
        && c != 'g' && c != 'G' && c != 't' && c != 'T';
}

#if 0
//------------------------------------------------------------------------------
string mkdata( const CSeq_entry & entry )
{
    const CBioseq & bioseq( entry.GetSeq() );

    if(    bioseq.CanGetInst() 
           && bioseq.GetInst().CanGetLength()
           && bioseq.GetInst().CanGetSeq_data() )
    {
        TSeqPos len( bioseq.GetInst().GetLength() );
        const CSeq_data & seqdata( bioseq.GetInst().GetSeq_data() );
        auto_ptr< CSeq_data > dest( new CSeq_data );
        CSeqportUtil::Convert( seqdata, dest.get(), CSeq_data::e_Iupacna, 
                               0, len );
        return dest->GetIupacna().Get();
    }

    return string( "" );
}
#endif

//------------------------------------------------------------------------------
Uint8 CWinMaskCountsGenerator::fastalen( const string & fname ) const
{
    Uint8 result = 0;

    for(CWinMaskUtil::CInputBioseq_CI bs_iter(fname, infmt); bs_iter; ++bs_iter)
    {
        CBioseq_Handle bsh = *bs_iter;

        if( CWinMaskUtil::consider( bsh, ids, exclude_ids ) )
            result += bsh.GetBioseqLength();
    }

    return result;
}

//------------------------------------------------------------------------------
static Uint4 reverse_complement( Uint4 seq, Uint1 size )
{ return CSeqMaskerUtil::reverse_complement( seq, size ); }

//------------------------------------------------------------------------------
CWinMaskCountsGenerator::CWinMaskCountsGenerator( 
    const string & arg_input,
    CNcbiOstream & os,
    const string & infmt_arg,
    const string & sformat,
    const string & arg_th,
    Uint4 mem_avail,
    Uint1 arg_unit_size,
    Uint8 arg_genome_size,
    Uint4 arg_min_count,
    Uint4 arg_max_count,
    bool arg_check_duplicates,
    bool arg_use_list,
    const CWinMaskUtil::CIdSet * arg_ids,
    const CWinMaskUtil::CIdSet * arg_exclude_ids,
    bool use_ba )
:   input( arg_input ),
    ustat( CSeqMaskerOstatFactory::create( sformat, os, use_ba ) ),
    max_mem( mem_avail*1024*1024ULL ), unit_size( arg_unit_size ),
    genome_size( arg_genome_size ),
    min_count( arg_min_count == 0 ? 1 : arg_min_count ), 
    max_count( 500 ),
    t_high( arg_max_count ),
    has_min_count( arg_min_count != 0 ),
    no_extra_pass( arg_min_count != 0 && arg_max_count != 0 ),
    check_duplicates( arg_check_duplicates ),use_list( arg_use_list ), 
    total_ecodes( 0 ), 
    score_counts( max_count, 0 ),
    ids( arg_ids ), exclude_ids( arg_exclude_ids ),
    infmt( infmt_arg )
{
    // Parse arg_th to set up th[].
    string::size_type pos( 0 );
    Uint1 count( 0 );

    while( pos != string::npos && count < 4 )
    {
        string::size_type newpos = arg_th.find_first_of( ",", pos );
        th[count++] = atof( arg_th.substr( pos, newpos - pos ).c_str() );
        pos = (newpos == string::npos ) ? newpos : newpos + 1;
    }
}

//------------------------------------------------------------------------------
CWinMaskCountsGenerator::CWinMaskCountsGenerator( 
    const string & arg_input,
    const string & output,
    const string & infmt_arg,
    const string & sformat,
    const string & arg_th,
    Uint4 mem_avail,
    Uint1 arg_unit_size,
    Uint8 arg_genome_size,
    Uint4 arg_min_count,
    Uint4 arg_max_count,
    bool arg_check_duplicates,
    bool arg_use_list,
    const CWinMaskUtil::CIdSet * arg_ids,
    const CWinMaskUtil::CIdSet * arg_exclude_ids,
    bool use_ba )
:   input( arg_input ),
    ustat( CSeqMaskerOstatFactory::create( sformat, output, use_ba ) ),
    max_mem( mem_avail*1024*1024ULL ), unit_size( arg_unit_size ),
    genome_size( arg_genome_size ),
    min_count( arg_min_count == 0 ? 1 : arg_min_count ), 
    max_count( 500 ),
    t_high( arg_max_count ),
    has_min_count( arg_min_count != 0 ),
    no_extra_pass( arg_min_count != 0 && arg_max_count != 0 ),
    check_duplicates( arg_check_duplicates ),use_list( arg_use_list ), 
    total_ecodes( 0 ), 
    score_counts( max_count, 0 ),
    ids( arg_ids ), exclude_ids( arg_exclude_ids ),
    infmt( infmt_arg )
{
    // Parse arg_th to set up th[].
    string::size_type pos( 0 );
    Uint1 count( 0 );

    while( pos != string::npos && count < 4 )
    {
        string::size_type newpos = arg_th.find_first_of( ",", pos );
        th[count++] = atof( arg_th.substr( pos, newpos - pos ).c_str() );
        pos = (newpos == string::npos ) ? newpos : newpos + 1;
    }
}

//------------------------------------------------------------------------------
CWinMaskCountsGenerator::~CWinMaskCountsGenerator() {}

//------------------------------------------------------------------------------
void CWinMaskCountsGenerator::operator()()
{
    // Generate a list of files to process.
    vector< string > file_list;

    if( !use_list ) {
        NStr::Tokenize(input, ",", file_list);
    } else {
        string line;
        CNcbiIfstream fl_stream( input.c_str() );

        while( getline( fl_stream, line ) ) {
            if( !line.empty() ) {
                file_list.push_back( line );
            }
        }
    }

    // Check for duplicates, if necessary.
    if( check_duplicates )
    {
        CheckDuplicates( file_list, infmt, ids, exclude_ids );
    }

    if( unit_size == 0 )
    {
        if( genome_size == 0 )
        {
            LOG_POST( "computing the genome length" );
            Uint8 total = 0;

            for(    vector< string >::const_iterator i = file_list.begin();
                    i != file_list.end(); ++i )
            {
                total += fastalen( *i );
            }

            genome_size = total;

            if( genome_size == 0 ) {
                NCBI_THROW( GenCountsException, eNullGenome, "" );
            }
        }

        for( unit_size = 15; unit_size > 0; --unit_size ) {
            if(   (genome_size>>(2*unit_size)) >= 5 ) {
                break;
            }
        }

        ++unit_size;
        _TRACE( "unit size is: " << unit_size );
    }

    // Estimate the length of the prefix. 
    // Prefix length is unit_size - suffix length, where suffix length
    // is max N: (4**N) < max_mem.
    Uint1 prefix_size( 0 ), suffix_size( unit_size );
    Uint8 n_units( max_mem/sizeof( Uint4 ) );

    while( suffix_size > 0 ) {
        Uint8 units_needed( 1<<(2*suffix_size) );
        if( units_needed <= n_units ) break; 
        --suffix_size;
    }

    NCBI_ASSERT( suffix_size > 0, "suffix size is 0" );
    prefix_size = unit_size - suffix_size;
    ustat->setUnitSize( unit_size );

    // Now process for each prefix.
    Uint4 prefix_exp( 1<<(2*prefix_size) );
    Uint4 passno = 1;
    LOG_POST( "pass " << passno );

    for( Uint4 prefix( 0 ); prefix < prefix_exp; ++prefix ) {
        process( prefix, prefix_size, file_list, no_extra_pass );
    }

    ++passno;

    // Now put the final statistics as comments at the end of the output.
    for( Uint4 i( 1 ); i < max_count; ++i )
        score_counts[i] += score_counts[i-1];

    Uint4 offset( total_ecodes - score_counts[max_count - 1] );
    Uint4 index[4] = {0, 0, 0, 0};
    double previous( 0.0 );
    double current;

    if( no_extra_pass )
    {
        ustat->setBlank();
        ostringstream s;
        s << " " << total_ecodes << " ecodes";
        ustat->setComment( s.str() );
    }

    for( Uint4 i( 1 ); i <= max_count; ++i )
    {
        current = 100.0*(((double)(score_counts[i - 1] + offset))
                  /((double)total_ecodes));

        if( no_extra_pass )
        {
            ostringstream s;
            s << " " << dec << i << "\t" << score_counts[i - 1] + offset << "\t"
              << current;
            ustat->setComment( s.str() );
        }

        for( Uint1 j( 0 ); j < 4; ++j )
            if( previous < th[j] && current >= th[j] )
                index[j] = i;

        previous = current;
    }

    // If min_count or t_high must be deduced do it and reprocess.
    if( !no_extra_pass )
    {
        total_ecodes = 0;

        if( !has_min_count )
            min_count = index[0];

        if( t_high == 0 )
            t_high = index[3];

        if( min_count == 0 )
          min_count = 1;

        for( Uint4 i( 0 ); i < max_count; ++i )
            score_counts[i] = 0;

        LOG_POST( "pass " << passno );

        for( Uint4 prefix( 0 ); prefix < prefix_exp; ++prefix )
            process( prefix, prefix_size, file_list, true );

        for( Uint4 i( 1 ); i < max_count; ++i )
            score_counts[i] += score_counts[i-1];

        offset = total_ecodes - score_counts[max_count - 1];

        {
            ustat->setBlank();
            ostringstream s;
            s << " " << total_ecodes << " ecodes";
            ustat->setComment( s.str() );
        }

        for( Uint4 i( 1 ); i <= max_count; ++i )
        {
            current 
                = 100.0*(((double)(score_counts[i - 1] + offset))
                  /((double)total_ecodes));
            ostringstream s;
            s << " " << dec << i << "\t" << score_counts[i - 1] + offset << "\t"
              << current;
            ustat->setComment( s.str() );
        }
    }

    ustat->setComment( "" );

    for( Uint1 i( 0 ); i < 4; ++i )
    {
        ostringstream s;
        s << " " << th[i] << "%% threshold at " << index[i];
        ustat->setComment( s.str() );
    }

    ustat->setBlank();
    ustat->setParam( "t_low      ", index[0] );
    ustat->setParam( "t_extend   ", index[1] );
    ustat->setParam( "t_threshold", index[2] );
    ustat->setParam( "t_high     ", index[3] );
    ustat->setBlank();
    ustat->finalize();
}

//------------------------------------------------------------------------------
void CWinMaskCountsGenerator::process( Uint4 prefix, 
                                       Uint1 prefix_size, 
                                       const vector< string > & input_list,
                                       bool do_output )
{
    Uint1 suffix_size( unit_size - prefix_size );
    Uint4 vector_size( 1<<(2*suffix_size) );
    vector< Uint4 > counts( vector_size, 0 );
    Uint4 unit_mask( (1<<(2*unit_size)) - 1 );
    Uint4 prefix_mask( ((1<<(2*prefix_size)) - 1)<<(2*suffix_size) );
    Uint4 suffix_mask( (1<<2*suffix_size) - 1 );
    if( unit_size == 16 ) unit_mask = 0xFFFFFFFF;
    prefix <<= (2*suffix_size);
    CRef<CObjectManager> om(CObjectManager::GetInstance());

    for( vector< string >::const_iterator it( input_list.begin() );
         it != input_list.end(); ++it )
    {
        for(CWinMaskUtil::CInputBioseq_CI bs_iter(*it, infmt); bs_iter; ++bs_iter)
        {
            CBioseq_Handle bsh = *bs_iter;

            if( CWinMaskUtil::consider( bsh, ids, exclude_ids ) )
            {
                CSeqVector data =
                    bs_iter->GetSeqVector(CBioseq_Handle::eCoding_Iupac);

                if( data.empty() )
                    continue;

                TSeqPos length( data.size() );
                Uint4 count( 0 );
                Uint4 unit( 0 );

                for( Uint4 i( 0 ); i < length; ++i ) {
                    if( ambig( data[i] ) )
                    {
                        count = 0;
                        unit = 0;
                        continue;
                    }
                    else
                    {
                        unit = ((unit<<2)&unit_mask) + letter( data[i] );

                        if( count >= unit_size - 1 )
                        {
                            Uint4 runit( reverse_complement( unit, unit_size ) );

                            if( unit <= runit && (unit&prefix_mask) == prefix )
                                ++counts[unit&suffix_mask];

                            if( runit <= unit && (runit&prefix_mask) == prefix )
                                ++counts[runit&suffix_mask];
                        }

                        ++count;
                    }
                }
            }
        }
    }

    for( Uint4 i( 0 ); i < vector_size; ++i )
    {
        Uint4 u( prefix + i ), ru( 0 );

        if( counts[i] > 0 )
        {
            ru = reverse_complement( u, unit_size );
            if( u == ru ) ++total_ecodes; else total_ecodes += 2;
        }

        if( counts[i] >= min_count )
        {
            if( counts[i] >= max_count )
                if( u == ru ) ++score_counts[max_count - 1];
                else score_counts[max_count - 1] += 2;
            else if( u == ru ) ++score_counts[counts[i] - 1];
            else score_counts[counts[i] - 1] += 2;

            if( do_output )
                ustat->setUnitCount( 
                        u, (counts[i] > t_high) ? t_high : counts[i] );
        }
    }
}

//------------------------------------------------------------------------------
const char * 
CWinMaskCountsGenerator::GenCountsException::GetErrCodeString() const
{
    switch( GetErrCode() ) {
        case eNullGenome: return "empty genome";
        default: return CException::GetErrCodeString();
    }
}

END_NCBI_SCOPE
