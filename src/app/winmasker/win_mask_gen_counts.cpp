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

#include "algo/winmask/seq_masker_util.hpp"

#include "win_mask_fasta_reader.hpp"
#include "win_mask_gen_counts.hpp"
#include "win_mask_dup_table.hpp"

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

//------------------------------------------------------------------------------
Uint8 fastalen( const string & fname )
{
    CNcbiIfstream input_stream( fname.c_str() );
    CWinMaskFastaReader reader( input_stream );
    CRef< CSeq_entry > entry( 0 );
    Uint8 result = 0;

    while( (entry = reader.GetNextSequence()).NotEmpty() )
    {
        string data( mkdata( *entry ) );
        result += data.length();
    }

    return result;
}

//------------------------------------------------------------------------------
static Uint4 reverse_complement( Uint4 seq, Uint1 size )
{ return CSeqMaskerUtil::reverse_complement( seq, size ); }

//------------------------------------------------------------------------------
CWinMaskCountsGenerator::CWinMaskCountsGenerator( const string & arg_input,
                                                  const string & output,
                                                  const string & arg_th,
                                                  Uint4 mem_avail,
                                                  Uint1 arg_unit_size,
                                                  Uint8 arg_genome_size,
                                                  Uint4 arg_min_count,
                                                  Uint4 arg_max_count,
                                                  bool arg_check_duplicates,
                                                  bool arg_use_list )
:   input( arg_input ), out_stream( &NcbiCout ),
    max_mem( mem_avail*1024*1024 ), unit_size( arg_unit_size ),
    genome_size( arg_genome_size ),
    min_count( arg_min_count == 0 ? 1 : arg_min_count ), 
    max_count( arg_max_count == 0 ? 500 : arg_max_count ),
    has_min_count( arg_min_count != 0 ),
    check_duplicates( arg_check_duplicates ),use_list( arg_use_list ), 
    total_ecodes( 0 ), 
    score_counts( arg_max_count == 0 ? 500 : arg_max_count, 0 )
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
    vector< string > file_list;

    if( !use_list )
        file_list.push_back( input );
    else
    {
        string line;
        CNcbiIfstream fl_stream( input.c_str() );

        while( getline( fl_stream, line ) )
            if( !line.empty() )
                file_list.push_back( line );
    }

    // Check for duplicates, if necessary.
    if( check_duplicates )
    {
        CheckDuplicates( file_list );
        cerr << "." << flush;
    }

    if( unit_size == 0 )
    {
        if( genome_size == 0 )
        {
            cerr << "Computing the genome length" << flush;
            Uint8 total = 0;

            for(    vector< string >::const_iterator i = file_list.begin();
                    i != file_list.end(); ++i )
            {
                total += fastalen( *i );
                cerr << "." << flush;
            }

            cerr << "done." << endl;
            genome_size = total;
        }

        for( unit_size = 15; unit_size >= 0; --unit_size )
            if(   (genome_size>>(2*unit_size)) >= 5 )
                break;

        ++unit_size;
        cerr << "Unit size is: " << unit_size << endl;
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

    *out_stream << unit_size << endl;

    // Now process for each prefix.
    Uint4 prefix_exp( 1<<(2*prefix_size) );
    Uint4 passno = 1;
    cerr << "Pass " << passno << flush;

    for( Uint4 prefix( 0 ); prefix < prefix_exp; ++prefix )
        process( prefix, prefix_size, file_list, has_min_count );

    ++passno;
    cerr << endl;

    // Now put the final statistics as comments at the end of the output.
    for( Uint4 i( 1 ); i < max_count; ++i )
        score_counts[i] += score_counts[i-1];

    Uint4 offset( total_ecodes - score_counts[max_count - 1] );
    Uint4 index[4] = {0, 0, 0, 0};
    double previous( 0.0 );
    double current;

    if( has_min_count )
        *out_stream << "\n# " << total_ecodes << " ecodes" << endl;

    for( Uint4 i( 1 ); i <= max_count; ++i )
    {
        current 
            = 100.0*(((double)(score_counts[i - 1] + offset))/((double)total_ecodes));

        if( has_min_count )
            *out_stream << "# " << dec << i << "\t" 
                        << score_counts[i - 1] + offset << "\t"
                        << current << endl;

        for( Uint1 j( 0 ); j < 4; ++j )
            if( previous < th[j] && current >= th[j] )
                index[j] = i;

        previous = current;
    }

    // If min_count must be deduced do it and reprocess.
    if( !has_min_count )
    {
        total_ecodes = 0;
        min_count = index[0];

        for( Uint4 i( 0 ); i < max_count; ++i )
            score_counts[i] = 0;

        cerr << "Pass " << passno << flush;

        for( Uint4 prefix( 0 ); prefix < prefix_exp; ++prefix )
            process( prefix, prefix_size, file_list, true );

        cerr << endl;

        for( Uint4 i( 1 ); i < max_count; ++i )
            score_counts[i] += score_counts[i-1];

        offset = total_ecodes - score_counts[max_count - 1];
        *out_stream << "\n# " << total_ecodes << " ecodes" << endl;

        for( Uint4 i( 1 ); i <= max_count; ++i )
        {
            current 
                = 100.0*(((double)(score_counts[i - 1] + offset))/((double)total_ecodes));
    
            *out_stream << "# " << dec << i << "\t" 
                        << score_counts[i - 1] + offset << "\t"
                        << current << endl;
        }
    }

    *out_stream << "#" << endl;

    for( Uint1 i( 0 ); i < 4; ++i )
        *out_stream << "# " << th[i]
            << "%% threshold at " << index[i] << endl;

    *out_stream << endl;
    *out_stream << ">t_low       " << index[0] << endl;
    *out_stream << ">t_extend    " << index[1] << endl;
    *out_stream << ">t_threshold " << index[2] << endl;
    *out_stream << ">t_high      " << index[3] << endl;
    *out_stream << endl;
    cerr << endl;
}

//------------------------------------------------------------------------------
void CWinMaskCountsGenerator::process( Uint4 prefix, 
                                       Uint1 prefix_size, 
                                       const vector< string > & input,
                                       bool do_output )
{
    Uint1 suffix_size( unit_size - prefix_size );
    Uint4 vector_size( 1<<(2*suffix_size) );
    vector< Uint4 > counts( vector_size, 0 );
    Uint4 unit_mask( (1<<(2*unit_size)) - 1 );
    Uint4 prefix_mask( ((1<<(2*prefix_size)) - 1)<<(2*suffix_size) );
    Uint4 suffix_mask( (1<<2*suffix_size) - 1 );

    prefix <<= (2*suffix_size);

    for( vector< string >::const_iterator it( input.begin() );
         it != input.end(); ++it )
    {
        CNcbiIfstream input_stream( it->c_str() );
        CWinMaskFastaReader reader( input_stream );
        CRef< CSeq_entry > entry( 0 );

        while( (entry = reader.GetNextSequence()).NotEmpty() )
        {
            string data( mkdata( *entry ) );

            if( data.empty() )
                continue;

            string::size_type length( data.length() );
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

                        if( unit <= runit && (unit&prefix_mask) == prefix )
                            ++counts[unit&suffix_mask];

                        if( runit <= unit && (runit&prefix_mask) == prefix )
                            ++counts[runit&suffix_mask];
                    }

                    ++count;
                }
        }

        cerr << "." << flush;
    }

    for( Uint4 i( 0 ); i < vector_size; ++i )
    {
        Uint4 ri; reverse_complement( i, unit_size );

        if( counts[i] > 0 )
        {
            ri = reverse_complement( i, unit_size );
            
            if( i == ri )
                ++total_ecodes; 
            else total_ecodes += 2;
        }

        if( counts[i] >= min_count )
        {
            if( counts[i] >= max_count )
                if( i == ri )
                    ++score_counts[max_count - 1];
                else score_counts[max_count - 1] += 2;
            else if( i == ri )
                ++score_counts[counts[i] - 1];
            else score_counts[counts[i] - 1] += 2;

            if( do_output )
                *out_stream << hex << prefix + i << " " 
                            << dec << counts[i] << "\n";
        }
    }
}


END_NCBI_SCOPE

/*
 * ========================================================================
 * $Log$
 * Revision 1.3  2005/03/17 20:21:22  morgulis
 * Only store half of the units in unit counts file.
 *
 * Revision 1.2  2005/03/08 17:02:30  morgulis
 * Changed unit counts file to include precomputed threshold values.
 * Changed masking code to pick up threshold values from the units counts file.
 * Unit size is computed automatically from the genome length.
 * Added extra option for specifying genome length.
 * Removed all experimental command line options.
 * Fixed id strings in duplicate sequence checking code.
 *
 * Revision 1.1  2005/02/25 21:32:54  dicuccio
 * Rearranged winmasker files:
 * - move demo/winmasker to a separate app directory (src/app/winmasker)
 * - move win_mask_* to app directory
 *
 * Revision 1.4  2005/02/25 21:09:18  morgulis
 * 1. Reduced the number of binary searches by the factor of 2 by locally
 *    caching some search results.
 * 2. Automatically compute -lowscore value if it is not specified on the
 *    command line during the counts generation pass.
 *
 * Revision 1.3  2005/02/12 20:24:39  dicuccio
 * Dropped use of std:: (not needed)
 *
 * Revision 1.2  2005/02/12 19:58:04  dicuccio
 * Corrected file type issues introduced by CVS (trailing return).  Updated
 * typedef names to match C++ coding standard.
 *
 * Revision 1.1  2005/02/12 19:15:11  dicuccio
 * Initial version - ported from Aleksandr Morgulis's tree in internal/winmask
 *
 * ========================================================================
 */

