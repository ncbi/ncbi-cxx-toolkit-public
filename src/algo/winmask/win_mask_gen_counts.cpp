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

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seq/IUPACna.hpp>

#include <algo/winmask/win_mask_fasta_reader.hpp>
#include <algo/winmask/win_mask_gen_counts.hpp>
#include <algo/winmask/win_mask_dup_table.hpp>

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

//------------------------------------------------------------------------------
std::string mkdata( const CSeq_entry & entry )
{
    const CBioseq & bioseq( entry.GetSeq() );

    if(    bioseq.CanGetInst() 
           && bioseq.GetInst().CanGetLength()
           && bioseq.GetInst().CanGetSeq_data() )
    {
        TSeqPos len( bioseq.GetInst().GetLength() );
        const CSeq_data & seqdata( bioseq.GetInst().GetSeq_data() );
        std::auto_ptr< CSeq_data > dest( new CSeq_data );
        CSeqportUtil::Convert( seqdata, dest.get(), CSeq_data::e_Iupacna, 
                               0, len );
        return dest->GetIupacna().Get();
    }

    return std::string( "" );
}

//------------------------------------------------------------------------------
static Uint4 reverse_complement( Uint4 seq, Uint1 size )
{
    Uint4 result( 0 );

    for( Uint1 i( 0 ); i < size; ++i )
    {
        Uint4 letter( ~(((seq>>(2*i))&0x3)|(~0x3)) );
        result = (result<<2)|letter;
    }

    return result;
}

//------------------------------------------------------------------------------
CWinMaskCountsGenerator::CWinMaskCountsGenerator( const std::string & arg_input,
                                                  const std::string & output,
                                                  const std::string & arg_th,
                                                  Uint4 mem_avail,
                                                  Uint1 arg_unit_size,
                                                  Uint4 arg_min_count,
                                                  Uint4 arg_max_count,
                                                  bool arg_check_duplicates,
                                                  bool arg_use_list )
: input( arg_input ), out_stream( &NcbiCout ),
    max_mem( mem_avail*1024*1024 ), unit_size( arg_unit_size ),
    min_count( arg_min_count ), max_count( arg_max_count ),
    check_duplicates( arg_check_duplicates ),use_list( arg_use_list ), 
total_ecodes( 0 ), score_counts( arg_max_count, 0 )
{
    if( !output.empty() )
        out_stream = new CNcbiOfstream( output.c_str() );

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
CWinMaskCountsGenerator::~CWinMaskCountsGenerator()
{
    if( out_stream != &NcbiCout )
        delete out_stream;
}

//------------------------------------------------------------------------------
void CWinMaskCountsGenerator::operator()()
{
    // Generate a list of files to process.
    std::vector< std::string > file_list;

    if( !use_list )
        file_list.push_back( input );
    else
    {
        std::string line;
        CNcbiIfstream fl_stream( input.c_str() );

        while( std::getline( fl_stream, line ) )
            if( !line.empty() )
                file_list.push_back( line );
    }

    // Check for duplicates, if necessary.
    if( check_duplicates )
    {
        CheckDuplicates( file_list );
        std::cerr << "." << std::flush;
    }

    // Estimate the length of the prefix. 
    // Prefix length is unit_size - suffix length, where suffix length
    // is max N: (4**N) < max_mem.
    Uint1 prefix_size( 0 ), suffix_size( 0 );

    for( Uint4 suffix_exp( 1 ); suffix_size <= unit_size; 
         ++suffix_size, suffix_exp *= 4 )
        if( suffix_exp >= max_mem/sizeof( Uint4 ) )
            prefix_size = unit_size - (--suffix_size);

    if( prefix_size == 0 )
        suffix_size = unit_size;

    *out_stream << unit_size << std::endl;

    // Now process for each prefix.
    Uint4 prefix_exp( 1<<(2*prefix_size) );

    for( Uint4 prefix( 0 ); prefix < prefix_exp; ++prefix )
        process( prefix, prefix_size, file_list );

    // Now put the final statistics as comments at the end of the output.
    *out_stream << "\n# " << total_ecodes << " ecodes" << std::endl;

    for( Uint4 i( 1 ); i < max_count; ++i )
        score_counts[i] += score_counts[i-1];

    Uint4 offset( total_ecodes - score_counts[max_count - 1] );
    Uint4 index[4] = {0, 0, 0, 0};
    double previous( 0.0 );
    double current;

    for( Uint4 i( 1 ); i <= max_count; ++i )
    {
        current 
            = 100.0*(((double)(score_counts[i - 1] + offset))/((double)total_ecodes));
        *out_stream << "# " << std::dec << i << "\t" 
            << score_counts[i - 1] + offset << "\t"
            << current << std::endl;

        for( Uint1 j( 0 ); j < 4; ++j )
            if( previous < th[j] && current >= th[j] )
                index[j] = i;

        previous = current;
    }

    *out_stream << "#" << std::endl;

    for( Uint1 i( 0 ); i < 4; ++i )
        *out_stream << "# " << th[i]
            << "%% threshold at " << index[i] << std::endl;

    std::cerr << std::endl;
}

//------------------------------------------------------------------------------
void CWinMaskCountsGenerator::process( 
                                      Uint4 prefix, Uint1 prefix_size, const std::vector< std::string > & input )
{
    Uint1 suffix_size( unit_size - prefix_size );
    Uint4 vector_size( 1<<(2*suffix_size) );
    vector< Uint4 > counts( vector_size, 0 );
    Uint4 unit_mask( (1<<(2*unit_size)) - 1 );
    Uint4 prefix_mask( ((1<<(2*prefix_size)) - 1)<<(2*suffix_size) );
    Uint4 suffix_mask( (1<<2*suffix_size) - 1 );

    prefix <<= (2*suffix_size);

    for( std::vector< std::string >::const_iterator it( input.begin() );
         it != input.end(); ++it )
    {
        CNcbiIfstream input_stream( it->c_str() );
        CWinMaskFastaReader reader( input_stream );
        CRef< CSeq_entry > entry( 0 );

        while( (entry = reader.GetNextSequence()).NotEmpty() )
        {
            std::string data( mkdata( *entry ) );

            if( data.empty() )
                continue;

            std::string::size_type length( data.length() );
            Uint4 count( 0 );
            Uint4 unit( 0 );

            for( Uint4 i( 0 ); i < length; ++i )
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

                        if( (unit&prefix_mask) == prefix )
                            ++counts[unit&suffix_mask];

                        if( (runit&prefix_mask) == prefix )
                            ++counts[runit&suffix_mask];
                    }

                    ++count;
                }
        }

        std::cerr << "." << std::flush;
    }

    for( Uint4 i( 0 ); i < vector_size; ++i )
    {
        if( counts[i] > 0 )
            ++total_ecodes;

        if( counts[i] >= min_count )
        {
            if( counts[i] >= max_count )
                ++score_counts[max_count - 1];
            else ++score_counts[counts[i] - 1];

            *out_stream << std::hex << prefix + i << " " 
                << std::dec << counts[i] << "\n";
        }
    }
}


END_NCBI_SCOPE

/*
 * ========================================================================
 * $Log$
 * Revision 1.1  2005/02/12 19:15:11  dicuccio
 * Initial version - ported from Aleksandr Morgulis's tree in internal/winmask
 *
 * ========================================================================
 */

