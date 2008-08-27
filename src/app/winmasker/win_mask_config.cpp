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

#include "win_mask_config.hpp"
#include <objtools/seqmasks_io/mask_cmdline_args.hpp>
#include <objtools/seqmasks_io/mask_fasta_reader.hpp>
#include <objtools/seqmasks_io/mask_bdb_reader.hpp>
#include <objtools/seqmasks_io/mask_writer_int.hpp>
#include <objtools/seqmasks_io/mask_writer_fasta.hpp>
#include <objtools/seqmasks_io/mask_writer_seqloc.hpp>
#include <objtools/seqmasks_io/mask_writer_blastdb_maskinfo.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

CMaskWriter*
CWinMaskConfig::x_GetWriter(const CArgs& args, 
                            CNcbiOstream& output, 
                            const string& format)
{
    CMaskWriter* retval = NULL;
    if (format == "interval") {
        retval = new CMaskWriterInt(output);
    } else if (format == "fasta") {
        retval = new CMaskWriterFasta(output);
    } else if (NStr::StartsWith(format, "seqloc_")) {
        retval = new CMaskWriterSeqLoc(output, format);
    } else if (NStr::StartsWith(format, "maskinfo_")) {
        retval = 
            new CMaskWriterBlastDbMaskInfo(output, format, 3,
                               eBlast_filter_program_windowmasker,
                               BuildAlgorithmParametersString(args));
    } else {
        throw runtime_error("Unknown output format");
    }
    return retval;
}

//----------------------------------------------------------------------------
CWinMaskConfig::CWinMaskConfig( const CArgs & args )
    : is( !args["mk_counts"].AsBoolean() && args[kInputFormat].AsString() != "blastdb" ? 
          ( !args[kInput].AsString().empty() 
            ? new CNcbiIfstream( args[kInput].AsString().c_str() ) 
            : static_cast<CNcbiIstream*>(&NcbiCin) ) : NULL ), reader( NULL ), 
      os( !args["mk_counts"].AsBoolean() ?
          ( !args[kOutput].AsString().empty() 
            ? new CNcbiOfstream( args[kOutput].AsString().c_str() )
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
      input( args[kInput].AsString() ),
      output( args[kOutput].AsString() ),
      // th( args["th"].AsString() ),
      th( "90,99,99.5,99.8" ),
      use_dust( args["dust"].AsBoolean() ),
      // dust_window( args["dust_window"].AsInteger() ),
      // dust_linker( args["dust_linker"].AsInteger() ),
      dust_window( 64 ),
      // dust_level( 20 ),
      dust_level( args["dust_level"].AsInteger() ),
      dust_linker( 1 ),
      checkdup( args["checkdup"].AsBoolean() ),
      sformat( args["sformat"].AsString() ),
      smem( args["smem"].AsInteger() ),
      ids( 0 ), exclude_ids( 0 ),
      use_ba( args["use_ba"].AsBoolean() ),
      text_match( args["text_match"].AsBoolean() )
{
    _TRACE( "Entering CWinMaskConfig::CWinMaskConfig()" );

    string iformatstr = args[kInputFormat].AsString();

    if( !mk_counts )
    {
        if( is && !*is )
        {
            NCBI_THROW( CWinMaskConfigException,
                        eInputOpenFail,
                        args[kInput].AsString() );
        }

        if( iformatstr == "fasta" )
            reader = new CMaskFastaReader( *is, true, args["parse_seqids"] );
        else if( iformatstr == "blastdb" )
            reader = new CMaskBDBReader( args[kInput].AsString() );

        string oformatstr = args[kOutputFormat].AsString();

        writer = x_GetWriter(args, *os, oformatstr);

        if( !reader )
        {
            NCBI_THROW( CWinMaskConfigException,
                        eReaderAllocFail, "" );
        }

        set_max_score = args["set_t_high"]  ? args["set_t_high"].AsInteger()
                                            : 0;
        set_min_score = args["set_t_low"]   ? args["set_t_low"].AsInteger()
                                            : 0;
    }
    else {
        text_match = true;
    }

    string ids_file_name( args["ids"].AsString() );
    string exclude_ids_file_name( args["exclude_ids"].AsString() );

    if(    !ids_file_name.empty()
        && !exclude_ids_file_name.empty() )
    {
        NCBI_THROW( CWinMaskConfigException, eInconsistentOptions,
                    "only one of -ids or -exclude_ids can be specified" );
    }

    if( !ids_file_name.empty() ) {
        if( text_match ) {
            ids = new CIdSet_TextMatch;
        }else {
            if( !mk_counts && iformatstr == "blastdb" ) 
                ids = new CIdSet_SeqId;
            else
                NCBI_THROW( CWinMaskConfigException, eInconsistentOptions,
                        "-text_match false can be used only with -mk_counts true "
                        "and " + string(kInputFormat) + " blastdb" );
        }

        FillIdList( ids_file_name, *ids );
    }

    if( !exclude_ids_file_name.empty() ) {
        if( text_match ) {
            exclude_ids = new CIdSet_TextMatch;
        }else {
            if( !mk_counts && iformatstr == "blastdb" ) 
                exclude_ids = new CIdSet_SeqId;
            else
                NCBI_THROW( CWinMaskConfigException, eInconsistentOptions,
                        "-text_match false can be used only with -mk_counts true "
                        "and " + string(kInputFormat) + " blastdb" );
        }

        FillIdList( exclude_ids_file_name, *exclude_ids );
    }

    _TRACE( "Leaving CWinMaskConfig::CWinMaskConfig" );
}

CWinMaskConfig::~CWinMaskConfig()
{
    if ( writer ) {
        delete writer;
    }
}

//----------------------------------------------------------------------------
void CWinMaskConfig::Validate() const
{
    _TRACE( "Entering CWinMaskConfig::Validate" );

    if( !mk_counts && lstat_name.empty() ){
        NCBI_THROW( CWinMaskConfigException, eInconsistentOptions,
                    "one of '-mk_counts true' or '-ustat <stat_file>' "
                    "must be specified" );
    }

    _TRACE( "Leaving CWinMaskConfig::Validate" );
}

//----------------------------------------------------------------------------
void CWinMaskConfig::FillIdList( const string & file_name, 
                                 CIdSet & id_list )
{
    CNcbiIfstream file( file_name.c_str() );
    string line;

    while( NcbiGetlineEOL( file, line ) ) {
        if( !line.empty() )
        {
            string::size_type stop( line.find_first_of( " \t" ) );
            string::size_type start( line[0] == '>' ? 1 : 0 );
            string id_str = line.substr( start, stop - start );
            id_list.insert( id_str );
        }
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

    case eInconsistentOptions:

        return "inconsistent program options";

    default: 

        return CException::GetErrCodeString();
    }
}

//----------------------------------------------------------------------------
void CWinMaskConfig::CIdSet_SeqId::insert( const string & id_str )
{
    try {
        CRef<CSeq_id> id(new CSeq_id(id_str));
        idset.insert(CSeq_id_Handle::GetHandle(*id));
    } catch (CSeqIdException& e) {
        LOG_POST(Error
            << "CWinMaskConfig::FillIdList(): can't understand id: "
            << id_str << ": " << e.what() << ": ignoring");
    }
}

//----------------------------------------------------------------------------
bool CWinMaskConfig::CIdSet_SeqId::find( 
        const objects::CBioseq_Handle & bsh ) const
{
    const CBioseq_Handle::TId & syns = bsh.GetId();

    ITERATE( CBioseq_Handle::TId, iter, syns )
    {
        if( idset.find( *iter ) != idset.end() ) {
            return true;
        }
    }

    return false;
}

//----------------------------------------------------------------------------
const vector< Uint4 > 
CWinMaskConfig::CIdSet_TextMatch::split( const string & id_str )
{
    vector< Uint4 > result;
    string tmp = id_str;

    if( !tmp.empty() && tmp[tmp.length() - 1] == '|' )
        tmp = tmp.substr( 0, tmp.length() - 1 );

    if( !tmp.empty() ) {
        string::size_type pos = ( tmp[0] == '>' ) ? 1 : 0;
        string::size_type len = tmp.length();

        while( pos != string::npos && pos < len ) {
            result.push_back( pos );

            if( (pos = tmp.find_first_of( "|", pos )) != string::npos ) {
                ++pos;
            }
        }
    }

    result.push_back( tmp.length() + 1 );
    return result;
}

//----------------------------------------------------------------------------
void CWinMaskConfig::CIdSet_TextMatch::insert( const string & id_str )
{
    Uint4 nwords = split( id_str ).size() - 1;

    if( nwords == 0 ) {
        LOG_POST( Error
            << "CWinMaskConfig::CIdSet_TextMatch::insert(): bad id: "
            << id_str << ": ignoring");
    }

    if( nword_sets_.size() < nwords ) {
        nword_sets_.resize( nwords );
    }

    if( id_str[id_str.length() - 1] != '|' ) {
        nword_sets_[nwords - 1].insert( id_str );
    }else {
        nword_sets_[nwords - 1].insert( 
                id_str.substr( 0, id_str.length() - 1 ) );
    }
}

//----------------------------------------------------------------------------
bool CWinMaskConfig::CIdSet_TextMatch::find( 
        const objects::CBioseq_Handle & bsh ) const
{
    CConstRef< CBioseq > seq = bsh.GetCompleteBioseq();
    string id_str = sequence::GetTitle( bsh );
    
    if( !id_str.empty() ) {
        string::size_type pos = id_str.find_first_of( " \t" );
        id_str = id_str.substr( 0, pos );
    }

    if( find( id_str ) ) return true;
    else if( id_str.substr( 0, 4 ) == "lcl|" ) {
        id_str = id_str.substr( 4, string::npos );
        return find( id_str );
    }
    else return false;
}

//----------------------------------------------------------------------------
inline bool CWinMaskConfig::CIdSet_TextMatch::find( 
        const string & id_str, Uint4 nwords ) const
{
    return nword_sets_[nwords].find( id_str ) != nword_sets_[nwords].end();
}

//----------------------------------------------------------------------------
bool CWinMaskConfig::CIdSet_TextMatch::find( const string & id_str ) const
{
    vector< Uint4 > word_starts = split( id_str );

    for( Uint4 i = 0; 
            i < nword_sets_.size() && i < word_starts.size() - 1; ++i ) {
        if( !nword_sets_[i].empty() ) {
            for( Uint4 j = 0; j < word_starts.size() - i - 1; ++j ) {
                string pattern = id_str.substr(
                        word_starts[j],
                        word_starts[j + i + 1] - word_starts[j] - 1 );

                if( find( pattern, i ) ) {
                    return true;
                }
            }
        }
    }

    return false;
}

END_NCBI_SCOPE
