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
 *   CWinMaskConfig class member and method definitions.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbidbg.hpp>

#include "win_mask_fasta_reader.hpp"
#include "win_mask_writer_int.hpp"
#include "win_mask_writer_fasta.hpp"
#include "win_mask_config.hpp"

BEGIN_NCBI_SCOPE

//----------------------------------------------------------------------------
CWinMaskConfig::CWinMaskConfig( const CArgs & args )
    : is( !args["mk_counts"].AsBoolean() ? 
          ( !args["input"].AsString().empty() 
            ? new CNcbiIfstream( args["input"].AsString().c_str() ) 
            : static_cast<CNcbiIstream*>(&NcbiCin) ) : NULL ), reader( NULL ), 
      os( !args["mk_counts"].AsBoolean() ?
          ( !args["output"].AsString().empty() 
            ? new CNcbiOfstream( args["output"].AsString().c_str() )
            : static_cast<CNcbiOstream*>(&NcbiCout) ) : NULL ), writer( NULL ),
      lstat_name( args["ustat"].AsString() ),
      textend( args["t_extend"] ? args["t_extend"].AsInteger() : 0 ), 
      cutoff_score( args["t_thres"] ? args["t_thres"].AsInteger() : 0 ),
      max_score( args["t_high"] ? args["t_high"].AsInteger() : 0 ),
      min_score( args["t_low"] ? args["t_low"].AsInteger() : 0 ),
      window_size( args["window"] ? args["window"].AsInteger() : 0 ),
      // merge_pass( args["mpass"].AsBoolean() ),
      // merge_cutoff_score( args["mscore"].AsInteger() ),
      // abs_merge_cutoff_dist( args["mabs"].AsInteger() ),
      // mean_merge_cutoff_dist( args["mmean"].AsInteger() ),
      merge_pass( false ),
      merge_cutoff_score( 50 ),
      abs_merge_cutoff_dist( 8 ),
      mean_merge_cutoff_dist( 50 ),
      // trigger( args["trigger"].AsString() ),
      trigger( "mean" ),
      // tmin_count( args["tmin_count"].AsInteger() ),
      tmin_count( 0 ),
      // discontig( args["discontig"].AsBoolean() ),
      // pattern( args["pattern"].AsInteger() ),
      discontig( false ),
      pattern( 0 ),
      // window_step( args["wstep"].AsInteger() ),
      // unit_step( args["ustep"].AsInteger() ),
      window_step( 1 ),
      unit_step( 1 ),
      // merge_unit_step( args["mustep"].AsInteger() ),
      merge_unit_step( 1 ),
      mk_counts( args["mk_counts"].AsBoolean() ),
      fa_list( args["fa_list"].AsBoolean() ),
      mem( args["mem"].AsInteger() ),
      unit_size( args["unit"] ? args["unit"].AsInteger() : 0 ),
      genome_size( args["genome_size"] ? args["genome_size"].AsInt8() : 0 ),
      input( args["input"].AsString() ),
      output( args["output"].AsString() ),
      // th( args["th"].AsString() ),
      th( "90,99,99.5,99.8" ),
      use_dust( args["dust"].AsBoolean() ),
      // dust_window( args["dust_window"].AsInteger() ),
      // dust_level( args["dust_level"].AsInteger() ),
      // dust_linker( args["dust_linker"].AsInteger() ),
      dust_window( 64 ),
      dust_level( 20 ),
      dust_linker( 1 ),
      checkdup( args["checkdup"].AsBoolean() )
{
    _TRACE( "Entering CWinMaskConfig::CWinMaskConfig()" );

    if( !mk_counts )
    {
        if( !*is )
        {
            NCBI_THROW( CWinMaskConfigException,
                        eInputOpenFail,
                        args["input"].AsString() );
        }

        reader = new CWinMaskFastaReader( *is );
        string oformatstr = args["oformat"].AsString();

        if( oformatstr == "interval" )
            writer = new CWinMaskWriterInt( *os );
        else if( oformatstr == "fasta" )
            writer = new CWinMaskWriterFasta( *os );

        if( !reader )
        {
            NCBI_THROW( CWinMaskConfigException,
                        eReaderAllocFail, "" );
        }

        set_max_score = args["set_t_high"]  ? args["set_t_high"].AsInteger()
                                            : 0;
        set_min_score = args["set_t_low"]   ? args["set_t_low"].AsInteger()
                                            : 0;

        string ids_file_name( args["ids"].AsString() );
        string exclude_ids_file_name( args["exclude_ids"].AsString() );

        if(    !ids_file_name.empty()
               && !exclude_ids_file_name.empty() )
        {
            NCBI_THROW( CWinMaskConfigException, eInconsistentOptions,
                        "only one of -ids or -exclude_ids can be specified" );
        }

        if( !ids_file_name.empty() )
            FillIdList( ids_file_name, ids );

        if( !exclude_ids_file_name.empty() )
            FillIdList( exclude_ids_file_name, exclude_ids );
    }

    _TRACE( "Leaving CWinMaskConfig::CWinMaskConfig" );
}

//----------------------------------------------------------------------------
void CWinMaskConfig::Validate() const
{
    _TRACE( "Entering CWinMaskConfig::Validate" );
    _TRACE( "Leaving CWinMaskConfig::Validate" );
}

//----------------------------------------------------------------------------
void CWinMaskConfig::FillIdList( const string & file_name, 
                                 set< string > & id_list )
{
    CNcbiIfstream file( file_name.c_str() );
    string line;

    while( getline( file, line ) )
        if( !line.empty() )
        {
            string::size_type stop( line.find_first_of( " \t" ) );
            string::size_type start( line[0] == '>' ? 1 : 0 );
            id_list.insert( line.substr( start, stop - start ) );
        }
}

//----------------------------------------------------------------------------
const char * CWinMaskConfig::CWinMaskConfigException::GetErrCodeString() const
{
    switch( GetErrCode() )
    {
    case eInputOpenFail: 

        return "can not open input stream";

    case eReaderAllocFail:

        return "can not allocate fasta sequence reader";

    default: 

        return CException::GetErrCodeString();
    }
}

END_NCBI_SCOPE

/*
 * ========================================================================
 * $Log$
 * Revision 1.4  2005/03/11 15:08:22  morgulis
 * 1. Made -window parameter optional and be default equal to unit_size + 4;
 * 2. Changed the name of -lstat parameter to -ustat.
 * 3. Added README file.
 *
 * Revision 1.3  2005/03/08 17:02:30  morgulis
 * Changed unit counts file to include precomputed threshold values.
 * Changed masking code to pick up threshold values from the units counts file.
 * Unit size is computed automatically from the genome length.
 * Added extra option for specifying genome length.
 * Removed all experimental command line options.
 * Fixed id strings in duplicate sequence checking code.
 *
 * Revision 1.2  2005/03/01 18:18:25  ucko
 * Make a couple of casts explicit for the sake of GCC 2.95.
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

